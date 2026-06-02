#include <rack.hpp>
#include "AgentModule.hpp"
#include "PanelLayout.hpp"
#include "GhostPanel.hpp"
#include "GhostBus.hpp"
#include "GhostVoice.hpp"
#include "ghost/signal/Audio.hpp"
#include <cmath>
#include "SvgHelper.hpp"

using namespace rack;
extern Plugin* pluginInstance;

/**
 * Toms -- 909-style toms (LowTom / MidTom / HighTom).
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

    // Mix. The 909 low tom is THREE damped sines at ratios 1 : 1.5 : 2.77.
    float osc1Gain           = 0.63f;
    float osc2Gain           = 0.12f;
    float osc3Ratio          = 2.77f;   // top partial (relative to freq1)
    float osc3Gain           = 0.08f;

    // Noise circuit -- the 909 toms mix in a short white-noise burst (the
    // "little noise" the body alone lacked). Subtle by default.
    float noiseGain          = 0.06f;
    float noiseDecayRate     = 28.f;    // noise burst 1/tau (~36 ms)

    // Click
    float clickGain          = 0.18f;
    float clickLengthSamples = 30.f;

    // Body envelope rate range: rate(decay) = envRateMin + (1-decay) * envRateSpan
    // Calibrated against the reference machine LowTom tune050-decay050: tau ~ 100 ms at decay=0.5.
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
    Ghost::AccentCharacter accent = Ghost::Accent::toms();  // shared policy
};

// baseHz calibrated against the reference machine references at tune050-decay050:
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
    float phase3 = 0.f;
    float t = 0.f;
    int   sampleCount = 0;
    float hpState = 0.f;
    bool  active = false;
    uint32_t rngState = 1u;

    void fire() {
        phase1 = phase2 = phase3 = 0.f;
        t = 0.f;
        sampleCount = 0;
        hpState = 0.f;
        rngState = 22699u;
        active = true;
    }

    inline float nextNoise() {
        rngState = rngState * 1664525u + 1013904223u;
        return ((rngState >> 8) & 0xFFFFFFu) * (2.f / 16777216.f) - 1.f;
    }

    float process(const rack::Module::ProcessArgs& args,
                  const TomFit::Config& fit,
                  float tuneNorm,
                  float decayNorm,
                  float levelNorm,
                  float accentNorm) {
        if (!active) return 0.f;

        const float tunedFreq = fit.baseHz * (fit.tuneOffset + tuneNorm * fit.tuneSpan);
        float pitchEnv  = std::exp(-fit.pitchBendRate * t);
        if (pitchEnv < Ghost::kDenormalFloor) pitchEnv = 0.f;   // denormal safety
        const float freq1     = tunedFreq
                              + pitchEnv * (fit.pitchBendBase + fit.baseHz * fit.pitchBendBaseScale);
        const float freq2     = freq1 * fit.osc2Ratio;
        const float freq3     = freq1 * fit.osc3Ratio;
        const float envRate   = fit.envRateMin + (1.f - decayNorm) * fit.envRateSpan;
        float env       = std::exp(-envRate * t);
        if (env < Ghost::kDenormalFloor) env = 0.f;   // denormal safety

        const float click = (sampleCount < (int)fit.clickLengthSamples)
            ? (Ghost::accentScale(
                    fit.clickGain, accentNorm, fit.accent.clickAmt)
               * (1.f - (float)sampleCount / fit.clickLengthSamples))
            : 0.f;

        phase1 += freq1 * args.sampleTime;
        phase2 += freq2 * args.sampleTime;
        phase3 += freq3 * args.sampleTime;
        phase1 -= std::floor(phase1);
        phase2 -= std::floor(phase2);
        phase3 -= std::floor(phase3);

        // Short white-noise burst (909 tom noise circuit), own fast envelope.
        float noiseEnv = std::exp(-fit.noiseDecayRate * t);
        if (noiseEnv < Ghost::kDenormalFloor) noiseEnv = 0.f;   // denormal safety
        const float noise = nextNoise() * noiseEnv
                          * Ghost::accentScale(fit.noiseGain, accentNorm, fit.accent.noiseAmt);

        float out = ((tomTriangle(phase1) * fit.osc1Gain
                   + tomTriangle(phase2) * fit.osc2Gain
                   + tomTriangle(phase3) * fit.osc3Gain)
                   * Ghost::accentScale(1.f, accentNorm, fit.accent.bodyAmt)
                   + click) * env
                   + noise;

        // Leaky DC-blocking HP: y = x - LP(x).
        hpState += fit.hpCoef * (out - hpState);
        if (std::abs(hpState) < Ghost::kDenormalFloor) hpState = 0.f;   // denormal safety
        out -= hpState;

        const float driveGain = Ghost::accentAdd(
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

struct TomLab : GhostModule {
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
    Ghost::AccentMix accentMix = Ghost::Accent::gentleMix();
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

        const auto bus = Ghost::resolveBus(this);
        if (voice.trigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            auto acc = Ghost::sampleAccentAtTrig(
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
        Ghost::LabArt::drawLabShell(
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

struct Toms : GhostModule {
    // GHOST surface: three toms as a small drum sub-system -- each voice fully
    // tunable (TUNE/DECAY/LEVEL) with per-knob CV. IDs finalized for release.
    enum ParamId  {
        LOW_TUNE_PARAM,  LOW_DECAY_PARAM,  LOW_LEVEL_PARAM,
        MID_TUNE_PARAM,  MID_DECAY_PARAM,  MID_LEVEL_PARAM,
        HIGH_TUNE_PARAM, HIGH_DECAY_PARAM, HIGH_LEVEL_PARAM,
        NUM_PARAMS
    };
    // Per the classic 909 voice layout, all three toms have Accent B. The shared
    // LOCAL_ACC and TOTAL_ACC inputs apply to whichever voice fires;
    // each voice latches its own gain at its own trigger edge.
    enum InputId  {
        LOW_TRIG_INPUT,
        MID_TRIG_INPUT,
        HIGH_TRIG_INPUT,
        LOCAL_ACC_INPUT,
        TOTAL_ACC_INPUT,
        LOW_TUNE_CV_INPUT,  LOW_DECAY_CV_INPUT,  LOW_LEVEL_CV_INPUT,
        MID_TUNE_CV_INPUT,  MID_DECAY_CV_INPUT,  MID_LEVEL_CV_INPUT,
        HIGH_TUNE_CV_INPUT, HIGH_DECAY_CV_INPUT, HIGH_LEVEL_CV_INPUT,
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
    Ghost::AccentMix accentMix = Ghost::Accent::gentleMix();
    float lowGain = 1.f, midGain = 1.f, highGain = 1.f;
    float lowChar = 0.f, midChar = 0.f, highChar = 0.f;

    Toms() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(LOW_TUNE_PARAM,   0.f, 1.f, 0.50f, "Low tune",   "%", 0.f, 100.f);
        configParam(LOW_DECAY_PARAM,  0.f, 1.f, 0.45f, "Low decay",  "%", 0.f, 100.f);
        configParam(LOW_LEVEL_PARAM,  0.f, 1.f, 0.85f, "Low level",  "%", 0.f, 100.f);
        configParam(MID_TUNE_PARAM,   0.f, 1.f, 0.50f, "Mid tune",   "%", 0.f, 100.f);
        configParam(MID_DECAY_PARAM,  0.f, 1.f, 0.45f, "Mid decay",  "%", 0.f, 100.f);
        configParam(MID_LEVEL_PARAM,  0.f, 1.f, 0.85f, "Mid level",  "%", 0.f, 100.f);
        configParam(HIGH_TUNE_PARAM,  0.f, 1.f, 0.50f, "High tune",  "%", 0.f, 100.f);
        configParam(HIGH_DECAY_PARAM, 0.f, 1.f, 0.45f, "High decay", "%", 0.f, 100.f);
        configParam(HIGH_LEVEL_PARAM, 0.f, 1.f, 0.85f, "High level", "%", 0.f, 100.f);
        configInput(LOW_TRIG_INPUT,   "Low trigger");
        configInput(MID_TRIG_INPUT,   "Mid trigger");
        configInput(HIGH_TRIG_INPUT,  "High trigger");
        configInput(LOCAL_ACC_INPUT,  "Local accent (Accent B, sampled at TRIG; shared)");
        configInput(TOTAL_ACC_INPUT,  "Total accent (Accent A, sampled at TRIG; shared)");
        configInput(LOW_TUNE_CV_INPUT,   "Low tune CV");
        configInput(LOW_DECAY_CV_INPUT,  "Low decay CV");
        configInput(LOW_LEVEL_CV_INPUT,  "Low level CV");
        configInput(MID_TUNE_CV_INPUT,   "Mid tune CV");
        configInput(MID_DECAY_CV_INPUT,  "Mid decay CV");
        configInput(MID_LEVEL_CV_INPUT,  "Mid level CV");
        configInput(HIGH_TUNE_CV_INPUT,  "High tune CV");
        configInput(HIGH_DECAY_CV_INPUT, "High decay CV");
        configInput(HIGH_LEVEL_CV_INPUT, "High level CV");
        configOutput(LOW_OUT_OUTPUT,  "Low audio");
        configOutput(MID_OUT_OUTPUT,  "Mid audio");
        configOutput(HIGH_OUT_OUTPUT, "High audio");

        lowFit  = TomFit::makeLowTom();
        midFit  = TomFit::makeMidTom();
        highFit = TomFit::makeHighTom();
    }

    /// Return all three toms to silence on Rack "Initialize" / first load
    /// (clear triggers, park each voice's DSP state, drop accent latches).
    void onReset() override {
        for (TomVoice* v : {&low, &mid, &high}) {
            v->trigger.reset();
            v->phase1 = v->phase2 = v->phase3 = 0.f;
            v->t = 0.f;
            v->sampleCount = 0;
            v->hpState = 0.f;
            v->rngState = 1u;
            v->active = false;
        }
        lowGain = midGain = highGain = 1.f;
        lowChar = midChar = highChar = 0.f;
    }

    // knob + CV/10, clamped to 0..1.
    float normWithCV(int paramId, int inputId) {
        return rack::math::clamp(
            params[paramId].getValue() + inputs[inputId].getVoltage() * 0.1f,
            0.f, 1.f);
    }

    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::resolveBus(this);
        auto sampleAcc = [&]() {
            return Ghost::sampleAccentAtTrig(
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

        const float master = bus.masterVolume;
        outputs[LOW_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(
            low.process(args, lowFit,
                        normWithCV(LOW_TUNE_PARAM,  LOW_TUNE_CV_INPUT),
                        normWithCV(LOW_DECAY_PARAM, LOW_DECAY_CV_INPUT),
                        normWithCV(LOW_LEVEL_PARAM, LOW_LEVEL_CV_INPUT),
                        lowChar) * lowGain * master));
        outputs[MID_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(
            mid.process(args, midFit,
                        normWithCV(MID_TUNE_PARAM,  MID_TUNE_CV_INPUT),
                        normWithCV(MID_DECAY_PARAM, MID_DECAY_CV_INPUT),
                        normWithCV(MID_LEVEL_PARAM, MID_LEVEL_CV_INPUT),
                        midChar) * midGain * master));
        outputs[HIGH_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(
            high.process(args, highFit,
                         normWithCV(HIGH_TUNE_PARAM,  HIGH_TUNE_CV_INPUT),
                         normWithCV(HIGH_DECAY_PARAM, HIGH_DECAY_CV_INPUT),
                         normWithCV(HIGH_LEVEL_PARAM, HIGH_LEVEL_CV_INPUT),
                         highChar) * highGain * master));
    }
};

struct TomsWidget : ModuleWidget, SvgHelper<TomsWidget> {
    TomsWidget(Toms* module) {
        setModule(module);
        loadPanel(asset::plugin(pluginInstance, "res/Toms.svg"));

        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        bindParam<RoundBlackKnob>("param.low.tune",   Toms::LOW_TUNE_PARAM);
        bindParam<RoundBlackKnob>("param.low.decay",  Toms::LOW_DECAY_PARAM);
        bindParam<RoundBlackKnob>("param.low.level",  Toms::LOW_LEVEL_PARAM);
        bindParam<RoundBlackKnob>("param.mid.tune",   Toms::MID_TUNE_PARAM);
        bindParam<RoundBlackKnob>("param.mid.decay",  Toms::MID_DECAY_PARAM);
        bindParam<RoundBlackKnob>("param.mid.level",  Toms::MID_LEVEL_PARAM);
        bindParam<RoundBlackKnob>("param.high.tune",  Toms::HIGH_TUNE_PARAM);
        bindParam<RoundBlackKnob>("param.high.decay", Toms::HIGH_DECAY_PARAM);
        bindParam<RoundBlackKnob>("param.high.level", Toms::HIGH_LEVEL_PARAM);

        bindInput<PJ301MPort>("cv.low.tune",   Toms::LOW_TUNE_CV_INPUT);
        bindInput<PJ301MPort>("cv.low.decay",  Toms::LOW_DECAY_CV_INPUT);
        bindInput<PJ301MPort>("cv.low.level",  Toms::LOW_LEVEL_CV_INPUT);
        bindInput<PJ301MPort>("cv.mid.tune",   Toms::MID_TUNE_CV_INPUT);
        bindInput<PJ301MPort>("cv.mid.decay",  Toms::MID_DECAY_CV_INPUT);
        bindInput<PJ301MPort>("cv.mid.level",  Toms::MID_LEVEL_CV_INPUT);
        bindInput<PJ301MPort>("cv.high.tune",  Toms::HIGH_TUNE_CV_INPUT);
        bindInput<PJ301MPort>("cv.high.decay", Toms::HIGH_DECAY_CV_INPUT);
        bindInput<PJ301MPort>("cv.high.level", Toms::HIGH_LEVEL_CV_INPUT);

        bindInput<PJ301MPort>("trig.low.trig",     Toms::LOW_TRIG_INPUT);
        bindInput<PJ301MPort>("trig.mid.trig",     Toms::MID_TRIG_INPUT);
        bindInput<PJ301MPort>("trig.high.trig",    Toms::HIGH_TRIG_INPUT);
        bindInput<PJ301MPort>("accent.main.local", Toms::LOCAL_ACC_INPUT);
        bindInput<PJ301MPort>("accent.main.total", Toms::TOTAL_ACC_INPUT);
        bindOutput<PJ301MPort>("out.low.audio",   Toms::LOW_OUT_OUTPUT);
        bindOutput<PJ301MPort>("out.mid.audio",   Toms::MID_OUT_OUTPUT);
        bindOutput<PJ301MPort>("out.high.audio",  Toms::HIGH_OUT_OUTPUT);
    }
};

rack::Model* modelToms = createModel<Toms, TomsWidget>("Toms");
