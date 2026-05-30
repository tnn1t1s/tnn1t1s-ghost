#include <rack.hpp>
#include "AgentModule.hpp"
#include "PanelLayout.hpp"
#include "NineOhNinePanel.hpp"
#include "Tr909Bus.hpp"
#include "ghost/signal/Audio.hpp"
#include <cmath>

using namespace rack;
extern Plugin* pluginInstance;

/**
 * Toms -- TR-909 inspired toms (LowTom / MidTom / HighTom).
 *
 * Direct port of a known-passable JUCE one-shot tom generator with every
 * internal model parameter exposed via `TomFit::Config`. Mirrors the
 * `SnrFit::Config` pattern in Snr.cpp so the voice_lab fitting workflow
 * can sweep any constant via `--param fit_<name>=<value>`.
 *
 * Per-sample algorithm (since trigger):
 *   tunedFreq = baseHz * (tuneOffset + tune * tuneSpan)
 *   pitchEnv  = exp(-pitchBendRate * t)
 *   freq1     = tunedFreq + pitchEnv * (pitchBendBase + baseHz * pitchBendBaseScale)
 *   freq2     = freq1 * osc2Ratio
 *   envRate   = envRateMin + (1 - decay) * envRateSpan
 *   env       = exp(-envRate * t)
 *   click     = (sample < clickLengthSamples)
 *               ? clickGain * (1 - sample / clickLengthSamples) : 0
 *   out       = (tri(p1)*osc1Gain + tri(p2)*osc2Gain + click) * env
 *   out       = HP(out, hpCoef)
 *   if driveGain > 0: out = tanh(out * driveGain)
 *   final     = clamp(out * outputGain * level, -1, 1)
 *
 * Per-voice difference is just `baseHz`; everything else shares defaults
 * so changes during fitting affect all three toms unless overridden.
 *
 * Rack IDs (stable, never reorder):
 *   Params:  TUNE=0, DECAY=1, LEVEL=2
 *   Inputs:  TRIG=0, TUNE_CV=1, DECAY_CV=2, LEVEL_CV=3, ACCENT=4
 *   Outputs: OUT=0
 */

namespace TomFit {

struct Config {
    // Pitch
    float baseHz             = 100.f;
    float tuneOffset         = 0.62f;
    float tuneSpan           = 0.88f;

    // Pitch envelope (1/tau and bend amount)
    float pitchBendRate      = 16.f;
    float pitchBendBase      = 22.f;
    float pitchBendBaseScale = 0.03f;

    // Oscillator 2 frequency ratio (1.5 = perfect fifth)
    float osc2Ratio          = 1.5f;

    // Mix
    float osc1Gain           = 0.63f;
    float osc2Gain           = 0.12f;

    // Click
    float clickGain          = 0.18f;
    float clickLengthSamples = 30.f;

    // Body envelope rate range: rate(decay) = envRateMin + (1-decay) * envRateSpan
    // Calibrated against TR-909 LowTom tune050-decay050: tau ~ 100 ms at decay=0.5.
    float envRateMin         = 6.f;
    float envRateSpan        = 8.f;

    // Leaky DC-blocking HP coefficient (~14 Hz cutoff at 44.1 kHz).
    float hpCoef             = 0.002f;

    // Soft-clip drive: 0 disables. JUCE original was 1.2; tanh added audible
    // even-harmonic colour, so the calibrated default is off.
    float driveGain          = 0.f;

    // Output
    float outputGain         = 0.78f;
    // Per-DSP-stage CHARACTER weights, applied multiplicatively when
    // charStrength > 0. Per the 909 reference doc, toms have level-only
    // accent on the original 909; the Ghost default keeps a small
    // body/click/drive boost as a stylistic add. AccentCharacter member
    // order is body, pitch, click, drive, noise, snap, decay, brightness, bend.
    Ghost::TR909::AccentCharacter accent =
        { 0.10f, 0.f, 0.15f, 0.08f, 0.f, 0.f, 0.f, 0.f, 0.f };
    //   body, pitch, click, drive, noise, snap, decay, brightness, bend
};

// baseHz calibrated against TR-909 references at tune050-decay050:
//   LowTom ref  = 90.8 Hz (F#2 -31c)  -> 90.8 / 1.06 = 85.7
//   MidTom ref  = 113.7 Hz (A#2 -42c) -> 113.7 / 1.06 = 107.3
//   HighTom ref = 133.9 Hz (C3 +41c)  -> 133.9 / 1.06 = 126.3
// Mid/High share every other fit param with Low; only baseHz differs.
inline Config makeLowTom()  { Config c; c.baseHz =  85.7f; return c; }
inline Config makeMidTom()  { Config c; c.baseHz = 107.3f; return c; }
inline Config makeHighTom() { Config c; c.baseHz = 126.3f; return c; }

}  // namespace TomFit

namespace {

static inline float tomTriangle(float phase) {
    return 1.f - 4.f * std::fabs(phase - 0.5f);
}

struct TomVoice {
    dsp::SchmittTrigger trigger;
    float phase1 = 0.f;
    float phase2 = 0.f;
    float t = 0.f;
    int   sampleCount = 0;
    float hpState = 0.f;
    bool  active = false;

    void fire() {
        phase1 = phase2 = 0.f;
        t = 0.f;
        sampleCount = 0;
        hpState = 0.f;
        active = true;
    }

    float process(const rack::Module::ProcessArgs& args,
                  const TomFit::Config& fit,
                  float tuneNorm,
                  float decayNorm,
                  float levelNorm,
                  float accentNorm) {
        if (!active) return 0.f;

        const float tunedFreq = fit.baseHz * (fit.tuneOffset + tuneNorm * fit.tuneSpan);
        const float pitchEnv  = std::exp(-fit.pitchBendRate * t);
        const float freq1     = tunedFreq
                              + pitchEnv * (fit.pitchBendBase + fit.baseHz * fit.pitchBendBaseScale);
        const float freq2     = freq1 * fit.osc2Ratio;
        const float envRate   = fit.envRateMin + (1.f - decayNorm) * fit.envRateSpan;
        const float env       = std::exp(-envRate * t);

        const float click = (sampleCount < (int)fit.clickLengthSamples)
            ? (Ghost::TR909::accentScale(
                    fit.clickGain, accentNorm, fit.accent.clickAmt)
               * (1.f - (float)sampleCount / fit.clickLengthSamples))
            : 0.f;

        phase1 += freq1 * args.sampleTime;
        phase2 += freq2 * args.sampleTime;
        phase1 -= std::floor(phase1);
        phase2 -= std::floor(phase2);

        float out = ((tomTriangle(phase1) * fit.osc1Gain
                   + tomTriangle(phase2) * fit.osc2Gain)
                   * Ghost::TR909::accentScale(1.f, accentNorm, fit.accent.bodyAmt)
                   + click) * env;

        // Leaky DC-blocking HP: y = x - LP(x).
        hpState += fit.hpCoef * (out - hpState);
        out -= hpState;

        const float driveGain = Ghost::TR909::accentAdd(
            fit.driveGain, accentNorm, fit.accent.driveAmt);
        if (driveGain > 0.f) {
            out = std::tanh(out * driveGain);
        }

        float result = rack::math::clamp(out * fit.outputGain * levelNorm, -1.f, 1.f);

        t += args.sampleTime;
        sampleCount++;
        if (env < 1e-5f && sampleCount > 1024) active = false;

        return result;
    }
};

} // namespace

// ---------------------------------------------------------------------------
// TomLab -- expanded expert / fitting variant.
//
// Same engine as the production toms, but exposes all 17 internal `TomFit`
// parameters as knobs alongside Tune/Decay/Level. Defaults match LowTom so
// you can dial in low first, then sweep `baseHz` to dial mid (~154 Hz) and
// high (~220 Hz) without recompiling. Once a setting sounds right, copy
// the values back into TomFit::makeXxxTom().
// ---------------------------------------------------------------------------

struct TomLab : Tr909Module {
    enum ParamId {
        TUNE_PARAM, DECAY_PARAM, LEVEL_PARAM,
        BASE_HZ_PARAM,
        TUNE_OFFSET_PARAM, TUNE_SPAN_PARAM,
        PITCH_BEND_RATE_PARAM, PITCH_BEND_BASE_PARAM, PITCH_BEND_BASE_SCALE_PARAM,
        OSC2_RATIO_PARAM, OSC1_GAIN_PARAM, OSC2_GAIN_PARAM,
        CLICK_GAIN_PARAM, CLICK_LEN_PARAM,
        ENV_RATE_MIN_PARAM, ENV_RATE_SPAN_PARAM,
        HP_COEF_PARAM, DRIVE_GAIN_PARAM,
        OUTPUT_GAIN_PARAM, ACCENT_DRIVE_PARAM,
        NUM_PARAMS
    };
    enum InputId  { TRIG_INPUT, LOCAL_ACC_INPUT, TOTAL_ACC_INPUT, NUM_INPUTS };
    enum OutputId { OUT_OUTPUT, NUM_OUTPUTS };

    TomVoice voice;
    TomFit::Config fit;
    Ghost::TR909::AccentMix accentMix = Ghost::TR909::neutralMix();
    float latchedCaseGain = 1.f;
    float voiceCharStrength = 0.f;

    TomLab() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        // Playable controls
        configParam(TUNE_PARAM,  0.f, 1.f, 0.50f, "Tune",  "%", 0.f, 100.f);
        configParam(DECAY_PARAM, 0.f, 1.f, 0.45f, "Decay", "%", 0.f, 100.f);
        configParam(LEVEL_PARAM, 0.f, 1.f, 0.85f, "Level", "%", 0.f, 100.f);
        // Fit knobs (defaults = LowTom calibrated)
        configParam(BASE_HZ_PARAM,              30.f,  300.f,    84.4f, "Base Hz",            " Hz");
        configParam(TUNE_OFFSET_PARAM,           0.f,    1.5f,    0.62f, "Tune offset");
        configParam(TUNE_SPAN_PARAM,             0.f,    2.f,     0.88f, "Tune span");
        configParam(PITCH_BEND_RATE_PARAM,       0.5f, 100.f,    16.f,   "Pitch bend rate (1/tau)");
        configParam(PITCH_BEND_BASE_PARAM,       0.f,  200.f,    22.f,   "Pitch bend base",     " Hz");
        configParam(PITCH_BEND_BASE_SCALE_PARAM, 0.f,    0.20f,   0.03f, "Pitch bend baseHz scale");
        configParam(OSC2_RATIO_PARAM,            0.5f,   4.f,     1.5f,  "Osc2 ratio (vs osc1)");
        configParam(OSC1_GAIN_PARAM,             0.f,    1.5f,    0.63f, "Osc1 gain");
        configParam(OSC2_GAIN_PARAM,             0.f,    1.f,     0.12f, "Osc2 gain");
        configParam(CLICK_GAIN_PARAM,            0.f,    1.f,     0.18f, "Click gain");
        configParam(CLICK_LEN_PARAM,             1.f,  200.f,    30.f,   "Click length",        " smp");
        configParam(ENV_RATE_MIN_PARAM,          1.f,   30.f,     6.f,   "Env rate min (decay=1)");
        configParam(ENV_RATE_SPAN_PARAM,         0.f,   50.f,     8.f,   "Env rate span");
        configParam(HP_COEF_PARAM,               0.f,    0.05f,   0.002f,"HP coef");
        configParam(DRIVE_GAIN_PARAM,            0.f,    3.f,     0.f,   "Drive gain (0 = off)");
        configParam(OUTPUT_GAIN_PARAM,           0.f,    2.f,     0.78f, "Output gain");
        configParam(ACCENT_DRIVE_PARAM,          0.f,    1.f,     0.08f, "Accent drive");

        configInput(TRIG_INPUT,        "Trigger");
        configInput(LOCAL_ACC_INPUT,   "Local accent (Accent B, sampled at TRIG)");
        configInput(TOTAL_ACC_INPUT,   "Total accent (Accent A, sampled at TRIG)");
        configOutput(OUT_OUTPUT,       "Audio");
    }

    void process(const ProcessArgs& args) override {
        // Live-copy every fit knob into the engine config.
        fit.baseHz             = params[BASE_HZ_PARAM].getValue();
        fit.tuneOffset         = params[TUNE_OFFSET_PARAM].getValue();
        fit.tuneSpan           = params[TUNE_SPAN_PARAM].getValue();
        fit.pitchBendRate      = params[PITCH_BEND_RATE_PARAM].getValue();
        fit.pitchBendBase      = params[PITCH_BEND_BASE_PARAM].getValue();
        fit.pitchBendBaseScale = params[PITCH_BEND_BASE_SCALE_PARAM].getValue();
        fit.osc2Ratio          = params[OSC2_RATIO_PARAM].getValue();
        fit.osc1Gain           = params[OSC1_GAIN_PARAM].getValue();
        fit.osc2Gain           = params[OSC2_GAIN_PARAM].getValue();
        fit.clickGain          = params[CLICK_GAIN_PARAM].getValue();
        fit.clickLengthSamples = params[CLICK_LEN_PARAM].getValue();
        fit.envRateMin         = params[ENV_RATE_MIN_PARAM].getValue();
        fit.envRateSpan        = params[ENV_RATE_SPAN_PARAM].getValue();
        fit.hpCoef             = params[HP_COEF_PARAM].getValue();
        fit.driveGain          = params[DRIVE_GAIN_PARAM].getValue();
        fit.outputGain         = params[OUTPUT_GAIN_PARAM].getValue();
        fit.accent.driveAmt     = params[ACCENT_DRIVE_PARAM].getValue();

        const auto bus = Ghost::TR909::resolveBus(this);
        if (voice.trigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            auto acc = Ghost::TR909::sampleAccentAtTrig(
                this, TOTAL_ACC_INPUT, bus, accentMix, LOCAL_ACC_INPUT);
            voice.fire();
            latchedCaseGain   = acc.gain;
            voiceCharStrength = acc.charStrength;
        }

        float tuneNorm   = rack::math::clamp(params[TUNE_PARAM].getValue(),  0.f, 1.f);
        float decayNorm  = rack::math::clamp(params[DECAY_PARAM].getValue(), 0.f, 1.f);
        float levelNorm  = rack::math::clamp(params[LEVEL_PARAM].getValue(), 0.f, 1.f);

        float out = voice.process(args, fit, tuneNorm, decayNorm, levelNorm,
                                  voiceCharStrength);
        out *= latchedCaseGain * bus.masterVolume;
        outputs[OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(out));
    }
};

struct TomLabLabelCell {
    float xMm;
    float yMm;
    const char* text;
};

struct TomLabPanel : rack::widget::Widget {
    std::vector<TomLabLabelCell> labels;

    void draw(const DrawArgs& args) override {
        Ghost::NineOhNine::drawLabShell(
            args.vg, box.size, "TOM LAB",
            "curated 18HP expert surface: 3 playable + 13 fit controls",
            nvgRGB(20, 22, 26),
            nvgRGBA(220, 230, 240, 200),
            nvgRGBA(180, 200, 220, 160));

        nvgFontSize(args.vg, 4.4f);
        nvgFillColor(args.vg, nvgRGBA(220, 230, 240, 200));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (const auto& l : labels) {
            nvgText(args.vg, mm2px(l.xMm), mm2px(l.yMm), l.text, nullptr);
        }
    }
};

struct TomLabWidget : rack::ModuleWidget {
    void addLabeledKnob(rack::engine::Module* module, int paramId,
                        float xMm, float yMm,
                        const char* label, TomLabPanel* panel) {
        addParam(createParamCentered<rack::RoundSmallBlackKnob>(
            mm2px(Vec(xMm, yMm)), module, paramId));
        panel->labels.push_back({ xMm, yMm + 7.f, label });
    }

    TomLabWidget(TomLab* module) {
        setModule(module);

        auto* panel = new TomLabPanel;
        panel->box.size = AgentLayout::panelSize_18HP();
        addChild(panel);
        box.size = panel->box.size;

        AgentLayout::addScrews_18HP(this);

        const float COLS_X[4] = { 14.f, 37.f, 60.f, 83.f };
        const float ROWS_Y[4] = { 24.f, 47.f, 70.f, 93.f };

        struct Cell { int param; const char* label; };
        Cell cells[16] = {
            {TomLab::TUNE_PARAM,                  "TUNE"},
            {TomLab::DECAY_PARAM,                 "DECAY"},
            {TomLab::LEVEL_PARAM,                 "LEVEL"},
            {TomLab::BASE_HZ_PARAM,               "BASE HZ"},
            {TomLab::TUNE_OFFSET_PARAM,           "TUN OFF"},
            {TomLab::TUNE_SPAN_PARAM,             "TUN SPAN"},
            {TomLab::PITCH_BEND_RATE_PARAM,       "PB RATE"},
            {TomLab::PITCH_BEND_BASE_PARAM,       "PB BASE"},
            {TomLab::OSC2_RATIO_PARAM,            "O2 RATIO"},
            {TomLab::OSC1_GAIN_PARAM,             "O1 GAIN"},
            {TomLab::OSC2_GAIN_PARAM,             "O2 GAIN"},
            {TomLab::CLICK_GAIN_PARAM,            "CLK GAIN"},
            {TomLab::CLICK_LEN_PARAM,             "CLK LEN"},
            {TomLab::ENV_RATE_MIN_PARAM,          "ENV MIN"},
            {TomLab::ENV_RATE_SPAN_PARAM,         "ENV SPAN"},
            {TomLab::DRIVE_GAIN_PARAM,            "DRIVE"},
        };
        for (int i = 0; i < 16; i++) {
            int r = i / 4;
            int c = i % 4;
            addLabeledKnob(module, cells[i].param, COLS_X[c], ROWS_Y[r], cells[i].label, panel);
        }

        addInput(createInputCentered<rack::PJ301MPort>(
            mm2px(Vec(18.f, 120.f)), module, TomLab::TRIG_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(
            mm2px(Vec(38.f, 120.f)), module, TomLab::LOCAL_ACC_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(
            mm2px(Vec(58.f, 120.f)), module, TomLab::TOTAL_ACC_INPUT));
        addOutput(createOutputCentered<rack::PJ301MPort>(
            mm2px(Vec(78.f, 120.f)), module, TomLab::OUT_OUTPUT));

        panel->labels.push_back({18.f, 113.f, "TRIG"});
        panel->labels.push_back({38.f, 113.f, "LACC"});
        panel->labels.push_back({58.f, 113.f, "TACC"});
        panel->labels.push_back({78.f, 113.f, "OUT"});
    }
};

rack::Model* modelTomLab = createModel<TomLab, TomLabWidget>("TomLab");


// ---------------------------------------------------------------------------
// Toms -- 3-voice tom kit (Low / Mid / High) in one module.
//
// Mirrors RimClap's pattern: per-voice trigger + level + audio output, plus
// one shared accent input. Internal tune/decay use the calibrated defaults
// from TomFit. Use TomLab if you want to sweep internal voicing parameters.
// ---------------------------------------------------------------------------

struct Toms : Tr909Module {
    enum ParamId  {
        LOW_LEVEL_PARAM,
        MID_LEVEL_PARAM,
        HIGH_LEVEL_PARAM,
        NUM_PARAMS
    };
    // Per Roland TR-909 OM, all three toms have Accent B. The shared
    // LOCAL_ACC and TOTAL_ACC inputs apply to whichever voice fires;
    // each voice latches its own gain at its own trigger edge.
    enum InputId  {
        LOW_TRIG_INPUT,
        MID_TRIG_INPUT,
        HIGH_TRIG_INPUT,
        LOCAL_ACC_INPUT,
        TOTAL_ACC_INPUT,
        NUM_INPUTS
    };
    enum OutputId {
        LOW_OUT_OUTPUT,
        MID_OUT_OUTPUT,
        HIGH_OUT_OUTPUT,
        NUM_OUTPUTS
    };

    TomVoice low, mid, high;
    TomFit::Config lowFit, midFit, highFit;
    Ghost::TR909::AccentMix accentMix = Ghost::TR909::neutralMix();
    float lowGain = 1.f, midGain = 1.f, highGain = 1.f;
    float lowChar = 0.f, midChar = 0.f, highChar = 0.f;

    Toms() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(LOW_LEVEL_PARAM,  0.f, 1.f, 0.85f, "Low level",  "%", 0.f, 100.f);
        configParam(MID_LEVEL_PARAM,  0.f, 1.f, 0.85f, "Mid level",  "%", 0.f, 100.f);
        configParam(HIGH_LEVEL_PARAM, 0.f, 1.f, 0.85f, "High level", "%", 0.f, 100.f);
        configInput(LOW_TRIG_INPUT,   "Low trigger");
        configInput(MID_TRIG_INPUT,   "Mid trigger");
        configInput(HIGH_TRIG_INPUT,  "High trigger");
        configInput(LOCAL_ACC_INPUT,  "Local accent (Accent B, sampled at TRIG; shared)");
        configInput(TOTAL_ACC_INPUT,  "Total accent (Accent A, sampled at TRIG; shared)");
        configOutput(LOW_OUT_OUTPUT,  "Low audio");
        configOutput(MID_OUT_OUTPUT,  "Mid audio");
        configOutput(HIGH_OUT_OUTPUT, "High audio");

        lowFit  = TomFit::makeLowTom();
        midFit  = TomFit::makeMidTom();
        highFit = TomFit::makeHighTom();
    }

    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::TR909::resolveBus(this);
        auto sampleAcc = [&]() {
            return Ghost::TR909::sampleAccentAtTrig(
                this, TOTAL_ACC_INPUT, bus, accentMix, LOCAL_ACC_INPUT);
        };
        if (low.trigger.process(inputs[LOW_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            low.fire(); auto a = sampleAcc(); lowGain = a.gain; lowChar = a.charStrength;
        }
        if (mid.trigger.process(inputs[MID_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            mid.fire(); auto a = sampleAcc(); midGain = a.gain; midChar = a.charStrength;
        }
        if (high.trigger.process(inputs[HIGH_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            high.fire(); auto a = sampleAcc(); highGain = a.gain; highChar = a.charStrength;
        }

        const float TUNE = 0.5f, DECAY = 0.45f;
        const float lowLevel  = params[LOW_LEVEL_PARAM].getValue();
        const float midLevel  = params[MID_LEVEL_PARAM].getValue();
        const float highLevel = params[HIGH_LEVEL_PARAM].getValue();

        const float master = bus.masterVolume;
        outputs[LOW_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(
            low.process(args, lowFit, TUNE, DECAY, lowLevel, lowChar) * lowGain * master));
        outputs[MID_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(
            mid.process(args, midFit, TUNE, DECAY, midLevel, midChar) * midGain * master));
        outputs[HIGH_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(
            high.process(args, highFit, TUNE, DECAY, highLevel, highChar) * highGain * master));
    }
};

struct TomsPanel : rack::widget::Widget {
    void draw(const DrawArgs& args) override {
        AgentLayout::drawAssetPanel(
            args.vg, box.size, pluginInstance,
            "res/Toms-bg.jpg",                     // optional asset; falls back to fill
            nvgRGB(22, 18, 20),
            "TOMS", nvgRGB(255, 200, 160));

        nvgFontSize(args.vg, 6.0f);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGBA(255, 220, 180, 200));
        nvgText(args.vg, mm2px(AgentLayout::CENTER_12HP), mm2px(20.f), "LOW",  nullptr);
        nvgText(args.vg, mm2px(AgentLayout::CENTER_12HP), mm2px(54.f), "MID",  nullptr);
        nvgText(args.vg, mm2px(AgentLayout::CENTER_12HP), mm2px(88.f), "HIGH", nullptr);

        nvgFontSize(args.vg, 5.0f);
        nvgFillColor(args.vg, nvgRGBA(255, 220, 180, 160));
        for (float y : { 30.f, 64.f, 98.f }) {
            nvgText(args.vg, mm2px(AgentLayout::LEFT_COLUMN_12HP),  mm2px(y), "TRIG",  nullptr);
            nvgText(args.vg, mm2px(AgentLayout::CENTER_12HP),       mm2px(y), "LEVEL", nullptr);
            nvgText(args.vg, mm2px(AgentLayout::RIGHT_COLUMN_12HP), mm2px(y), "OUT",   nullptr);
        }

        nvgText(args.vg, mm2px(AgentLayout::LEFT_COLUMN_12HP),  mm2px(112.f), "LACC", nullptr);
        nvgText(args.vg, mm2px(AgentLayout::RIGHT_COLUMN_12HP), mm2px(112.f), "TACC", nullptr);
    }
};

struct TomsWidget : rack::ModuleWidget {
    TomsWidget(Toms* module) {
        setModule(module);
        auto* panel = new TomsPanel;
        panel->box.size = AgentLayout::panelSize_12HP();
        addChild(panel);
        box.size = panel->box.size;
        AgentLayout::addScrews_12HP(this);

        struct Row { float y; int trig; int level; int out; };
        Row rows[3] = {
            { 38.f, Toms::LOW_TRIG_INPUT,  Toms::LOW_LEVEL_PARAM,  Toms::LOW_OUT_OUTPUT  },
            { 72.f, Toms::MID_TRIG_INPUT,  Toms::MID_LEVEL_PARAM,  Toms::MID_OUT_OUTPUT  },
            {106.f, Toms::HIGH_TRIG_INPUT, Toms::HIGH_LEVEL_PARAM, Toms::HIGH_OUT_OUTPUT },
        };
        for (auto& r : rows) {
            addInput(createInputCentered<rack::PJ301MPort>(
                mm2px(Vec(AgentLayout::LEFT_COLUMN_12HP, r.y)), module, r.trig));
            addParam(createParamCentered<rack::RoundSmallBlackKnob>(
                mm2px(Vec(AgentLayout::CENTER_12HP, r.y)), module, r.level));
            addOutput(createOutputCentered<rack::PJ301MPort>(
                mm2px(Vec(AgentLayout::RIGHT_COLUMN_12HP, r.y)), module, r.out));
        }

        // Shared accent inputs at the bottom of the panel.
        addInput(createInputCentered<rack::PJ301MPort>(
            mm2px(Vec(AgentLayout::LEFT_COLUMN_12HP,  120.f)),
            module, Toms::LOCAL_ACC_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(
            mm2px(Vec(AgentLayout::RIGHT_COLUMN_12HP, 120.f)),
            module, Toms::TOTAL_ACC_INPUT));
    }
};

rack::Model* modelToms = createModel<Toms, TomsWidget>("Toms");
