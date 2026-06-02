#include <rack.hpp>
#include "KckEngine.hpp"
#include "GhostBus.hpp"
#include "GhostPanel.hpp"
#include "PanelLayout.hpp"
#include "ghost/signal/Audio.hpp"

using namespace rack;
extern Plugin* pluginInstance;


// ---------------------------------------------------------------------------
// KckLab -- expanded expert / fitting variant.
//
// Same engine as Kck. Exposes the 7 user-facing controls plus the most
// informative KckFit knobs for hand-fitting. This is deliberately a curated
// 18HP expert surface, not a dump of every internal constant. Once a setting
// sounds right, copy the values back into KckFit::makeKick().
// ---------------------------------------------------------------------------

/// Expert/fitting variant of Kck: same KckVoice engine, but exposes a curated
/// set of KckFit constants as live knobs (each with its own CV input) so a
/// sound can be hand-fit before its values are copied back into makeKick().
struct KckLab : GhostModule {
    float latchedCaseGain = 1.f;

    enum ParamId {
        TUNE_PARAM, DECAY_PARAM, PITCH_PARAM, PITCH_DECAY_PARAM,
        CLICK_PARAM, DRIVE_PARAM, LEVEL_PARAM,
        // Fit knobs follow.
        BASE_PITCH_OFFSET_PARAM, BASE_PITCH_SPAN_PARAM,
        AMP_DECAY_MIN_PARAM,    AMP_DECAY_SPAN_PARAM,
        SWEEP_FAST_BASE_PARAM,  SWEEP_FAST_RATE_PARAM,
        SWEEP_SLOW_BASE_PARAM,  SWEEP_SLOW_RATE_PARAM,
        BODY_FUND_GAIN_PARAM,   BODY_HARM_RATIO_PARAM,  BODY_HARM_GAIN_PARAM,
        SUB_GAIN_PARAM,         SUB_DECAY_PARAM,
        CLICK_RATE_PARAM,       CLICK_NOISE_PARAM,
        CHIRP_START_PARAM,      CHIRP_RATE_PARAM,       CHIRP_GAIN_PARAM,
        HP_COEF_PARAM,          DRIVE_BASE_PARAM,       OUTPUT_GAIN_PARAM,
        NUM_PARAMS
    };
    // Per-knob CV inputs: one per ParamId. This keeps KckLab in the "full
    // expert instrument" tier, unlike the other Lab modules which are knob-only.
    enum InputId  {
        TRIG_INPUT, LOCAL_ACC_INPUT, TOTAL_ACC_INPUT,
        TUNE_CV, DECAY_CV, PITCH_CV, PITCH_DECAY_CV,
        CLICK_CV, DRIVE_CV, LEVEL_CV,
        BASE_PITCH_OFFSET_CV, BASE_PITCH_SPAN_CV,
        AMP_DECAY_MIN_CV, AMP_DECAY_SPAN_CV,
        SWEEP_FAST_BASE_CV, SWEEP_FAST_RATE_CV,
        SWEEP_SLOW_BASE_CV, SWEEP_SLOW_RATE_CV,
        BODY_FUND_GAIN_CV, BODY_HARM_RATIO_CV, BODY_HARM_GAIN_CV,
        SUB_GAIN_CV, SUB_DECAY_CV,
        CLICK_RATE_CV, CLICK_NOISE_CV,
        CHIRP_START_CV, CHIRP_RATE_CV, CHIRP_GAIN_CV,
        HP_COEF_CV, DRIVE_BASE_CV, OUTPUT_GAIN_CV,
        NUM_INPUTS
    };
    enum OutputId { OUT_OUTPUT, NUM_OUTPUTS };

    KckVoice voice;
    KckFit::Config fit;

    /// Configure the playable knobs plus the exposed fit-constant knobs and
    /// their per-knob CV inputs (each with engineering-unit ranges).
    KckLab() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        // Playable
        configParam(TUNE_PARAM,        0.f, 1.f, 0.50f,  "Tune");
        configParam(DECAY_PARAM,       0.f, 1.f, 0.50f,  "Decay");
        configParam(PITCH_PARAM,       0.f, 1.f, 0.385f, "Pitch amount");
        configParam(PITCH_DECAY_PARAM, 0.f, 1.f, 0.26f,  "Pitch decay");
        configParam(CLICK_PARAM,       0.f, 1.f, 0.50f, "Click / attack");
        configParam(DRIVE_PARAM,       0.f, 1.f, 0.f,   "Drive (extra)");
        configParam(LEVEL_PARAM,       0.f, 1.f, 0.85f, "Level");
        // Fit
        configParam(BASE_PITCH_OFFSET_PARAM, 10.f,  80.f,    20.f,   "Base pitch offset", " Hz");
        configParam(BASE_PITCH_SPAN_PARAM,   10.f, 120.f,    50.f,   "Base pitch span",   " Hz");
        configParam(AMP_DECAY_MIN_PARAM,      1.f,  40.f,    20.f,   "Body decay min (1/tau)");
        configParam(AMP_DECAY_SPAN_PARAM,     0.f,  30.f,    13.f,   "Body decay span");
        configParam(SWEEP_FAST_BASE_PARAM,    0.f, 300.f,   112.f,   "Fast sweep amp",    " Hz");
        configParam(SWEEP_FAST_RATE_PARAM,    1.f, 300.f,   150.f,   "Fast sweep rate (1/tau)");
        configParam(SWEEP_SLOW_BASE_PARAM,    0.f, 100.f,     8.f,   "Slow sweep amp",    " Hz");
        configParam(SWEEP_SLOW_RATE_PARAM,    0.5f, 50.f,    15.f,   "Slow sweep rate (1/tau)");
        configParam(BODY_FUND_GAIN_PARAM,     0.f,   1.5f,    0.88f, "Body fundamental gain");
        configParam(BODY_HARM_RATIO_PARAM,    1.f,   3.f,     2.02f, "Body harmonic ratio");
        configParam(BODY_HARM_GAIN_PARAM,     0.f,   1.f,     0.19f, "Body harmonic gain");
        configParam(SUB_GAIN_PARAM,           0.f,   1.f,     0.36f, "Sub gain");
        configParam(SUB_DECAY_PARAM,          0.1f,  5.f,     0.85f, "Sub decay (1/tau)");
        configParam(CLICK_RATE_PARAM,        20.f, 600.f,   140.f,   "Click rate (1/tau)");
        configParam(CLICK_NOISE_PARAM,        0.f,   1.f,     0.03f, "Click noise gain");
        configParam(CHIRP_START_PARAM,      400.f,3000.f,  1700.f,   "Click chirp start", " Hz");
        configParam(CHIRP_RATE_PARAM,         0.f,1500.f,   400.f,   "Click chirp falling rate");
        configParam(CHIRP_GAIN_PARAM,         0.f,   1.f,     0.f,   "Click chirp gain");
        configParam(HP_COEF_PARAM,            0.f,   0.05f,   0.0012f,"HP coef");
        configParam(DRIVE_BASE_PARAM,         0.5f,  4.f,     1.55f, "Drive base");
        configParam(OUTPUT_GAIN_PARAM,        0.f,   2.f,     1.f,   "Output gain");

        configInput (TRIG_INPUT,         "Trigger");
        configInput (LOCAL_ACC_INPUT,    "Local accent (Accent B, sampled at TRIG)");
        configInput (TOTAL_ACC_INPUT,    "Total accent (Accent A, sampled at TRIG)");

        configInput(TUNE_CV,                   "Tune CV");
        configInput(DECAY_CV,                  "Decay CV");
        configInput(PITCH_CV,                  "Pitch amount CV");
        configInput(PITCH_DECAY_CV,            "Pitch decay CV");
        configInput(CLICK_CV,                  "Click CV");
        configInput(DRIVE_CV,                  "Drive CV");
        configInput(LEVEL_CV,                  "Level CV");
        configInput(BASE_PITCH_OFFSET_CV,      "Base pitch offset CV");
        configInput(BASE_PITCH_SPAN_CV,        "Base pitch span CV");
        configInput(AMP_DECAY_MIN_CV,          "Amp decay min CV");
        configInput(AMP_DECAY_SPAN_CV,         "Amp decay span CV");
        configInput(SWEEP_FAST_BASE_CV,        "Fast sweep amp CV");
        configInput(SWEEP_FAST_RATE_CV,        "Fast sweep rate CV");
        configInput(SWEEP_SLOW_BASE_CV,        "Slow sweep amp CV");
        configInput(SWEEP_SLOW_RATE_CV,        "Slow sweep rate CV");
        configInput(BODY_FUND_GAIN_CV,         "Body fundamental gain CV");
        configInput(BODY_HARM_RATIO_CV,        "Body harm ratio CV");
        configInput(BODY_HARM_GAIN_CV,         "Body harm gain CV");
        configInput(SUB_GAIN_CV,               "Sub gain CV");
        configInput(SUB_DECAY_CV,              "Sub decay CV");
        configInput(CLICK_RATE_CV,             "Click rate CV");
        configInput(CLICK_NOISE_CV,            "Click noise CV");
        configInput(CHIRP_START_CV,            "Chirp start Hz CV");
        configInput(CHIRP_RATE_CV,             "Chirp falling rate CV");
        configInput(CHIRP_GAIN_CV,             "Chirp gain CV");
        configInput(HP_COEF_CV,                "HP coef CV");
        configInput(DRIVE_BASE_CV,             "Drive base CV");
        configInput(OUTPUT_GAIN_CV,            "Output gain CV");

        configOutput(OUT_OUTPUT,   "Audio");
    }

    /// Read a knob value plus its CV (0.1 of the param's range per volt),
    /// clamped to the param's [min, max]. Returns an engineering-unit value.
    inline float readWithCV(int paramId, int cvInputId) {
        rack::engine::ParamQuantity* q = paramQuantities[paramId];
        float range = q->maxValue - q->minValue;
        float v = params[paramId].getValue()
                + inputs[cvInputId].getVoltage() * 0.1f * range;
        return rack::math::clamp(v, q->minValue, q->maxValue);
    }

    /// Per-sample: live-copy every exposed fit knob (with CV) into the engine
    /// config, latch accent on a TRIG edge, run the voice, and apply per-case
    /// accent gain and bus master volume.
    void process(const ProcessArgs& args) override {
        // Live-copy fit knobs (with CV) into engine config every frame.
        fit.basePitchOffset          = readWithCV(BASE_PITCH_OFFSET_PARAM, BASE_PITCH_OFFSET_CV);
        fit.basePitchSpan            = readWithCV(BASE_PITCH_SPAN_PARAM,   BASE_PITCH_SPAN_CV);
        fit.ampDecayMin              = readWithCV(AMP_DECAY_MIN_PARAM,     AMP_DECAY_MIN_CV);
        fit.ampDecaySpan             = readWithCV(AMP_DECAY_SPAN_PARAM,    AMP_DECAY_SPAN_CV);
        fit.pitchSweepFastBase       = readWithCV(SWEEP_FAST_BASE_PARAM,   SWEEP_FAST_BASE_CV);
        fit.pitchSweepFastRateBase   = readWithCV(SWEEP_FAST_RATE_PARAM,   SWEEP_FAST_RATE_CV);
        fit.pitchSweepSlowBase       = readWithCV(SWEEP_SLOW_BASE_PARAM,   SWEEP_SLOW_BASE_CV);
        fit.pitchSweepSlowRateBase   = readWithCV(SWEEP_SLOW_RATE_PARAM,   SWEEP_SLOW_RATE_CV);
        fit.bodyFundGain             = readWithCV(BODY_FUND_GAIN_PARAM,    BODY_FUND_GAIN_CV);
        fit.bodyHarmRatio            = readWithCV(BODY_HARM_RATIO_PARAM,   BODY_HARM_RATIO_CV);
        fit.bodyHarmGain             = readWithCV(BODY_HARM_GAIN_PARAM,    BODY_HARM_GAIN_CV);
        fit.subGain                  = readWithCV(SUB_GAIN_PARAM,          SUB_GAIN_CV);
        fit.subDecayBase             = readWithCV(SUB_DECAY_PARAM,         SUB_DECAY_CV);
        fit.clickRateBase            = readWithCV(CLICK_RATE_PARAM,        CLICK_RATE_CV);
        fit.clickNoiseBase           = readWithCV(CLICK_NOISE_PARAM,       CLICK_NOISE_CV);
        fit.clickChirpStartHz        = readWithCV(CHIRP_START_PARAM,       CHIRP_START_CV);
        fit.clickChirpRate           = readWithCV(CHIRP_RATE_PARAM,        CHIRP_RATE_CV);
        fit.clickChirpBase           = readWithCV(CHIRP_GAIN_PARAM,        CHIRP_GAIN_CV);
        fit.hpCoef                   = readWithCV(HP_COEF_PARAM,           HP_COEF_CV);
        fit.driveBase                = readWithCV(DRIVE_BASE_PARAM,        DRIVE_BASE_CV);
        fit.outputGain               = readWithCV(OUTPUT_GAIN_PARAM,       OUTPUT_GAIN_CV);

        const auto bus = Ghost::resolveBus(this);

        if (voice.trigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            const bool totalGate = inputs[TOTAL_ACC_INPUT].getNormalVoltage(0.f) > 1.f;
            const bool localGate = inputs[LOCAL_ACC_INPUT].getNormalVoltage(0.f) > 1.f;
            const float charStrength =
                Ghost::isAccentedHit(totalGate, localGate) ? 1.f : 0.f;
            latchedCaseGain = Ghost::resolveAccentGain(
                totalGate, localGate, bus, fit.accentMix);
            voice.fire(charStrength);
        }

        const float tuneNorm       = readWithCV(TUNE_PARAM,        TUNE_CV);
        const float decayNorm      = readWithCV(DECAY_PARAM,       DECAY_CV);
        const float pitchNorm      = readWithCV(PITCH_PARAM,       PITCH_CV);
        const float pitchDecayNorm = readWithCV(PITCH_DECAY_PARAM, PITCH_DECAY_CV);
        const float clickNorm      = readWithCV(CLICK_PARAM,       CLICK_CV);
        const float driveNorm      = readWithCV(DRIVE_PARAM,       DRIVE_CV);
        const float levelNorm      = readWithCV(LEVEL_PARAM,       LEVEL_CV);

        float out = voice.process(args, fit,
                                  tuneNorm, decayNorm, pitchNorm, pitchDecayNorm,
                                  clickNorm, driveNorm, levelNorm);
        out *= latchedCaseGain * bus.masterVolume;
        outputs[OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(out));
    }
};


/// A single panel text label at a millimeter position.
struct KckLabLabelCell { float xMm, yMm; const char* text; };

/// KckLab background panel: draws the lab shell plus all knob/port labels.
struct KckLabPanel : rack::widget::Widget {
    std::vector<KckLabLabelCell> labels;

    void draw(const DrawArgs& args) override {
        Ghost::LabArt::drawLabShell(
            args.vg, box.size, "KCK LAB",
            "curated 18HP expert surface: 7 playable + 9 fit controls",
            nvgRGB(20, 18, 22));

        nvgFontSize(args.vg, 4.4f);
        nvgFillColor(args.vg, nvgRGBA(220, 220, 240, 200));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (const auto& l : labels) {
            nvgText(args.vg, mm2px(l.xMm), mm2px(l.yMm), l.text, nullptr);
        }
    }
};

/// KckLab panel widget: lays out the 16 curated knobs on a 4x4 grid plus the
/// trig/accent/output ports, registering each label with the panel.
struct KckLabWidget : rack::ModuleWidget {
    /// Add a centered knob at (xMm, yMm) and push its text label just above it.
    void addLabeledKnob(rack::engine::Module* module, int paramId,
                        float xMm, float yMm,
                        const char* label, KckLabPanel* panel) {
        panel->labels.push_back({ xMm, yMm - 6.5f, label });
        addParam(createParamCentered<rack::RoundSmallBlackKnob>(
            mm2px(Vec(xMm, yMm)), module, paramId));
    }

    KckLabWidget(KckLab* module) {
        setModule(module);

        auto* panel = new KckLabPanel;
        panel->box.size = AgentLayout::panelSize_18HP();
        addChild(panel);
        box.size = panel->box.size;

        AgentLayout::addScrews_18HP(this);

        const float kColsX[4] = { 14.f, 37.f, 60.f, 83.f };
        const float kRowsY[4] = { 24.f, 47.f, 70.f, 93.f };

        struct Cell { int param; const char* label; };
        // Deliberately capped at 16 controls so every 909 Lab module can share
        // the same 18HP footprint. This is the curated "kick voicing" surface,
        // not a dump of every hidden constant in KckFit::Config.
        Cell cells[16] = {
            {KckLab::TUNE_PARAM,              "TUNE"},
            {KckLab::DECAY_PARAM,             "DECAY"},
            {KckLab::PITCH_PARAM,             "PITCH"},
            {KckLab::PITCH_DECAY_PARAM,       "P DEC"},
            {KckLab::CLICK_PARAM,             "CLICK"},
            {KckLab::DRIVE_PARAM,             "DRIVE"},
            {KckLab::LEVEL_PARAM,             "LEVEL"},
            {KckLab::BASE_PITCH_OFFSET_PARAM, "BASE OFF"},
            {KckLab::BASE_PITCH_SPAN_PARAM,   "BASE SPN"},
            {KckLab::AMP_DECAY_MIN_PARAM,     "DEC MIN"},
            {KckLab::AMP_DECAY_SPAN_PARAM,    "DEC SPN"},
            {KckLab::SWEEP_FAST_BASE_PARAM,   "FSW AMP"},
            {KckLab::SWEEP_FAST_RATE_PARAM,   "FSW RATE"},
            {KckLab::BODY_FUND_GAIN_PARAM,    "BODY"},
            {KckLab::CLICK_RATE_PARAM,        "CLK RT"},
            {KckLab::DRIVE_BASE_PARAM,        "DRV BAS"},
        };
        for (int i = 0; i < 16; i++) {
            int r = i / 4;
            int c = i % 4;
            addLabeledKnob(module, cells[i].param, kColsX[c], kRowsY[r], cells[i].label, panel);
        }

        panel->labels.push_back({18.f, 117.5f, "TRIG"});
        panel->labels.push_back({38.f, 117.5f, "LACC"});
        panel->labels.push_back({58.f, 117.5f, "TACC"});
        panel->labels.push_back({78.f, 117.5f, "OUT"});
        addInput(createInputCentered<rack::PJ301MPort>(
            mm2px(Vec(18.f, 124.f)), module, KckLab::TRIG_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(
            mm2px(Vec(38.f, 124.f)), module, KckLab::LOCAL_ACC_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(
            mm2px(Vec(58.f, 124.f)), module, KckLab::TOTAL_ACC_INPUT));
        addOutput(createOutputCentered<rack::PJ301MPort>(
            mm2px(Vec(78.f, 124.f)), module, KckLab::OUT_OUTPUT));
    }
};

rack::Model* modelKckLab = createModel<KckLab, KckLabWidget>("KckLab");
