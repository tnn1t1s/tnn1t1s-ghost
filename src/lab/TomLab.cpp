#include <rack.hpp>
#include "AgentModule.hpp"
#include "TomsEngine.hpp"
#include "GhostBus.hpp"
#include "GhostPanel.hpp"
#include "PanelLayout.hpp"
#include "ghost/signal/Audio.hpp"

using namespace rack;
extern Plugin* pluginInstance;


// ---------------------------------------------------------------------------
// TomLab -- expanded expert / fitting variant.
//
// Same engine as the production toms, but exposes all 17 internal `TomFit`
// parameters as knobs alongside Tune/Decay/Level. Defaults match LowTom so
// you can dial in low first, then sweep `baseHz` to dial mid (~154 Hz) and
// high (~220 Hz) without recompiling. Once a setting sounds right, copy
// the values back into TomFit::makeXxxTom().
// ---------------------------------------------------------------------------

/// Single-voice expert/fitting module: the production tom engine with all 17
/// internal TomFit parameters exposed as knobs (defaults = LowTom). See banner.
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

    /// Copy all fit knobs into the engine config, fire on a trigger edge
    /// (latching accent), then render and write the audio output.
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

/// One knob/port caption on the TomLab panel: text at a mm position.
struct TomLabLabelCell {
    float xMm;
    float yMm;
    const char* text;
};

/// Procedurally-drawn TomLab background: the lab shell plus all knob/port
/// captions (no SVG; this is the expert surface).
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

/// Module widget for TomLab: lays the 16-knob grid plus trigger/accent/out
/// ports on the procedural panel.
struct TomLabWidget : rack::ModuleWidget {
    /// Add a centred small knob at (xMm,yMm) and register its caption just below.
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

        const float kColsX[4] = { 14.f, 37.f, 60.f, 83.f };
        const float kRowsY[4] = { 24.f, 47.f, 70.f, 93.f };

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
            addLabeledKnob(module, cells[i].param, kColsX[c], kRowsY[r], cells[i].label, panel);
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
