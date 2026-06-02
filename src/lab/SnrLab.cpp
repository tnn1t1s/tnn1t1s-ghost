#include <rack.hpp>
#include "SnrEngine.hpp"
#include "GhostBus.hpp"
#include "GhostPanel.hpp"
#include "PanelLayout.hpp"
#include "ghost/signal/Audio.hpp"

using namespace rack;
extern Plugin* pluginInstance;


// ---------------------------------------------------------------------------
// SnrLab -- expanded expert / fitting variant.
//
// Keeps the 909-facing Tune / Tone / Snappy / Level surface, then exposes a
// focused subset of the hidden fit variables that most directly change the
// voicing. This is intentionally narrower than the full internal Config: the
// goal is a playable expert module, not a raw dump of every constant. SnrLab
// follows the same 18HP Lab doctrine as the rest of the Ghost family: keep the
// module small enough to patch musically, but large enough to surface the fit
// controls that actually moved the sound during voice-lab passes.
// ---------------------------------------------------------------------------

struct SnrLab : GhostModule {
    enum ParamId {
        // Primary 909-facing surface.
        TUNE_PARAM, TONE_PARAM, SNAPPY_PARAM, LEVEL_PARAM,
        // Curated fit terms promoted to the Lab panel.
        BODY_DRIVE_PARAM, TONE_MAX_PARAM, NOISE_LP_PARAM, NOISE_HP_PARAM, LOW_NOISE_GAIN_PARAM,
        HIGH_NOISE_BASE_PARAM, HIGH_NOISE_SNAPPY_PARAM, MIX_DRIVE_BASE_PARAM, OUTPUT_GAIN_PARAM,
        SNAPPY_SHAPE_PARAM,
        NUM_PARAMS
    };
    enum InputId {
        TRIG_INPUT, LOCAL_ACC_INPUT, TOTAL_ACC_INPUT, NUM_INPUTS
    };
    enum OutputId { OUT_OUTPUT, NUM_OUTPUTS };

    dsp::SchmittTrigger trigger;
    Ghost::AccentMix accentMix = Ghost::Accent::gentleMix();
    float latchedCaseGain = 1.f;
    float latchedCharStrength = 0.f;

    float phase1 = 0.f;
    float phase2 = 0.f;
    float bodyEnv1 = 0.f;
    float bodyEnv2 = 0.f;
    float noiseLowEnv = 0.f;
    float noiseHighEnv = 0.f;
    float bendEnv = 0.f;
    float attackEnv = 1.f;
    float clickEnv = 0.f;
    float bodyLP = 0.f;

    float noisePhase = 0.f;
    uint32_t noiseShift = 0x1u;
    float noiseValue = 1.f;
    float prevBody = 0.f;
    float prevNoise = 0.f;

    SnrSVF lpNoise;
    SnrSVF hpNoise;

    /// Configure the 909 surface plus the curated fit-term knobs (ranges and
    /// defaults sourced from SnrFit defaults), trig/accent inputs, and output.
    SnrLab() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(TUNE_PARAM,               0.f, 1.f, 0.50f, "Tune", "%", 0.f, 100.f);
        configParam(TONE_PARAM,               0.f, 1.f, 1.00f, "Tone", "%", 0.f, 100.f);
        configParam(SNAPPY_PARAM,             0.f, 1.f, 1.00f, "Snappy", "%", 0.f, 100.f);
        configParam(LEVEL_PARAM,              0.f, 1.f, 0.82f, "Level", "%", 0.f, 100.f);
        configParam(BODY_DRIVE_PARAM,         0.5f, 2.0f,  SnrFit::defaults().bodyDrive,        "Body drive");
        configParam(TONE_MAX_PARAM,           0.04f, 0.18f, SnrFit::defaults().toneMaxSec,       "Tone max seconds");
        configParam(NOISE_LP_PARAM,           4000.f, 16000.f, SnrFit::defaults().noiseLpHz,     "Noise LP Hz");
        configParam(NOISE_HP_PARAM,           1000.f, 8000.f,  SnrFit::defaults().noiseHpHz,     "Noise HP Hz");
        configParam(LOW_NOISE_GAIN_PARAM,     0.00f, 0.20f, SnrFit::defaults().lowNoiseGain,      "Low noise gain");
        configParam(HIGH_NOISE_BASE_PARAM,    0.00f, 0.08f, SnrFit::defaults().highNoiseBase,     "High noise base");
        configParam(HIGH_NOISE_SNAPPY_PARAM,  0.00f, 0.80f, SnrFit::defaults().highNoiseSnappy,   "High noise snappy");
        configParam(MIX_DRIVE_BASE_PARAM,     0.50f, 1.50f, SnrFit::defaults().mixDriveBase,      "Mix drive base");
        configParam(OUTPUT_GAIN_PARAM,        0.40f, 1.40f, SnrFit::defaults().outputGain,        "Output gain");
        configParam(SNAPPY_SHAPE_PARAM,       0.50f, 4.00f, SnrFit::defaults().snappyShapePower,  "Snappy shape");
        configInput(TRIG_INPUT,        "Trigger");
        configInput(LOCAL_ACC_INPUT,   "Local accent (Accent B, sampled at TRIG)");
        configInput(TOTAL_ACC_INPUT,   "Total accent (Accent A, sampled at TRIG)");
        configOutput(OUT_OUTPUT,       "Audio");
    }

    /// Same synthesis as Snr::process, but each frame starts from SnrFit
    /// defaults and overrides the promoted lab knobs before computing the voice.
    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::resolveBus(this);
        if (trigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            auto acc = Ghost::sampleAccentAtTrig(
                this, TOTAL_ACC_INPUT, bus, accentMix, LOCAL_ACC_INPUT);
            latchedCharStrength = acc.charStrength;
            latchedCaseGain = acc.gain;
            bodyEnv1 = 1.f;
            bodyEnv2 = 1.f;
            noiseLowEnv = 1.f;
            noiseHighEnv = 1.f;
            bendEnv = 1.f;
            attackEnv = 0.f;
            clickEnv = 1.f;
            phase1 = 0.f;
            phase2 = 0.f;
            bodyLP = 0.f;
            noisePhase = 0.f;
            prevBody = 0.f;
            prevNoise = noiseValue;
            lpNoise.reset();
            hpNoise.reset();
        }

        // Start from production defaults every frame, then override only the
        // lab-facing fit terms. This keeps SnrLab audibly anchored to Snr and
        // makes it obvious which hidden constants were promoted to the expert
        // surface on purpose.
        SnrFit::Config fit = SnrFit::defaults();
        fit.bodyDrive = params[BODY_DRIVE_PARAM].getValue();
        fit.toneMaxSec = params[TONE_MAX_PARAM].getValue();
        fit.noiseLpHz = params[NOISE_LP_PARAM].getValue();
        fit.noiseHpHz = params[NOISE_HP_PARAM].getValue();
        fit.lowNoiseGain = params[LOW_NOISE_GAIN_PARAM].getValue();
        fit.highNoiseBase = params[HIGH_NOISE_BASE_PARAM].getValue();
        fit.highNoiseSnappy = params[HIGH_NOISE_SNAPPY_PARAM].getValue();
        fit.mixDriveBase = params[MIX_DRIVE_BASE_PARAM].getValue();
        fit.outputGain = params[OUTPUT_GAIN_PARAM].getValue();
        fit.snappyShapePower = params[SNAPPY_SHAPE_PARAM].getValue();

        float tune_norm = rack::math::clamp(params[TUNE_PARAM].getValue(), 0.f, 1.f);
        float tone_norm = rack::math::clamp(params[TONE_PARAM].getValue(), 0.f, 1.f);
        float snap_norm = rack::math::clamp(params[SNAPPY_PARAM].getValue(), 0.f, 1.f);
        float level_norm = rack::math::clamp(params[LEVEL_PARAM].getValue(), 0.f, 1.f);

        float tune_oct = (tune_norm - 0.5f) * 2.f * kSnrTuneOctRange;
        float scale = std::pow(2.f, tune_oct);
        float bendOct = bendEnv * fit.bendMaxOct;
        float f1 = fit.osc1BaseHz * scale * std::pow(2.f, bendOct);
        float f2 = fit.osc2BaseHz * scale * std::pow(2.f, bendOct * fit.osc2BendRatio);
        float toneTau = kSnrToneMinSec + tone_norm * (fit.toneMaxSec - kSnrToneMinSec);
        float snapShape = std::pow(1.f - snap_norm, fit.snappyShapePower);
        float noiseLowTau = toneTau
                          * (fit.lowNoiseSnappyTauMinScale + snapShape * fit.lowNoiseSnappyTauDelta);
        float noiseHighTau = toneTau * fit.noiseHighRatio
                           * (1.f + snapShape * fit.highNoiseSnappyTauDelta);

        phase1 += f1 * args.sampleTime;
        phase2 += f2 * args.sampleTime;
        phase1 -= std::floor(phase1);
        phase2 -= std::floor(phase2);

        float tri1 = snrTriangle(phase1);
        float tri2 = snrTriangle(phase2);
        float bodyRaw = tri1 * bodyEnv1 * fit.body1Gain + tri2 * bodyEnv2 * fit.body2Gain;
        float bodyLpAlpha = 1.f - std::exp(-2.f * float(M_PI) * fit.bodyLpHz * args.sampleTime);
        bodyLP += (bodyRaw - bodyLP) * bodyLpAlpha;
        if (std::abs(bodyLP) < Ghost::kDenormalFloor) bodyLP = 0.f;   // denormal safety
        float body = std::tanh(bodyLP * fit.bodyDrive);

        noisePhase += fit.noiseClockHz * args.sampleTime;
        while (noisePhase >= 1.f) {
            noisePhase -= 1.f;
            uint32_t newBit = ((noiseShift >> 0) ^ (noiseShift >> 2)
                             ^ (noiseShift >> 3) ^ (noiseShift >> 5)) & 1u;
            noiseShift = (noiseShift >> 1) | (newBit << 15);
            noiseValue = (noiseShift & 1u) ? 1.f : -1.f;
        }
        lpNoise.process(noiseValue, fit.noiseLpHz, args.sampleRate, kSnrNoiseLowQ);
        hpNoise.process(noiseValue, fit.noiseHpHz, args.sampleRate, kSnrNoiseHighQ);
        float lowNoiseGain = fit.lowNoiseGain
                           * (fit.lowNoiseToneBase + tone_norm * fit.lowNoiseToneSpan)
                           * (1.f + snapShape * fit.lowNoiseSnappyGainDelta);
        float lowNoise = lpNoise.lpf * noiseLowEnv * lowNoiseGain;
        float highNoiseGain = (fit.highNoiseBase + snap_norm * fit.highNoiseSnappy)
                            * (fit.highNoiseToneBase + tone_norm * fit.highNoiseToneSpan);
        highNoiseGain = Ghost::accentScale(
            highNoiseGain, latchedCharStrength, fit.accent.noiseAmt);
        float highNoise = hpNoise.hpf * noiseHighEnv * highNoiseGain;
        float click = ((body - prevBody) * fit.clickBodyGain + (noiseValue - prevNoise) * fit.clickNoiseGain) * clickEnv;
        prevBody = body;
        prevNoise = noiseValue;

        bodyEnv1 *= std::exp(-args.sampleTime / fit.body1TauSec);
        if (bodyEnv1 < Ghost::kDenormalFloor) bodyEnv1 = 0.f;   // denormal safety
        bodyEnv2 *= std::exp(-args.sampleTime / fit.body2TauSec);
        if (bodyEnv2 < Ghost::kDenormalFloor) bodyEnv2 = 0.f;   // denormal safety
        noiseLowEnv *= std::exp(-args.sampleTime / noiseLowTau);
        if (noiseLowEnv < Ghost::kDenormalFloor) noiseLowEnv = 0.f;   // denormal safety
        noiseHighEnv *= std::exp(-args.sampleTime / noiseHighTau);
        if (noiseHighEnv < Ghost::kDenormalFloor) noiseHighEnv = 0.f;   // denormal safety
        bendEnv *= std::exp(-args.sampleTime / fit.bendTauSec);
        if (bendEnv < Ghost::kDenormalFloor) bendEnv = 0.f;   // denormal safety
        attackEnv += (1.f - attackEnv) * (1.f - std::exp(-args.sampleTime / fit.attackTauSec));
        clickEnv *= std::exp(-args.sampleTime / fit.clickTauSec);
        if (clickEnv < Ghost::kDenormalFloor) clickEnv = 0.f;   // denormal safety

        float mix = body + lowNoise + highNoise + click;
        mix = std::tanh(mix * Ghost::accentAdd(
            fit.mixDriveBase + fit.mixDriveSnappy * snap_norm,
            latchedCharStrength, fit.accent.driveAmt));
        mix *= attackEnv;

        float out = mix * level_norm * fit.outputGain;
        out *= latchedCaseGain * bus.masterVolume;
        outputs[OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(out));
    }
};

/// A single positioned text label (mm coordinates) on the SnrLab panel.
struct SnrLabLabelCell { float xMm, yMm; const char* text; };

/// Background panel for SnrLab: draws the lab shell and a list of positioned
/// control labels supplied by the widget.
struct SnrLabPanel : rack::widget::Widget {
    std::vector<SnrLabLabelCell> labels;

    void draw(const DrawArgs& args) override {
        Ghost::LabArt::drawLabShell(
            args.vg, box.size, "SNR LAB",
            "curated 18HP expert surface",
            nvgRGB(10, 8, 10),
            nvgRGBA(255, 190, 180, 220),
            nvgRGBA(220, 180, 180, 165));

        nvgFontSize(args.vg, 4.3f);
        nvgFillColor(args.vg, nvgRGBA(220, 180, 180, 165));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (const auto& label : labels) {
            nvgText(args.vg, mm2px(label.xMm), mm2px(label.yMm), label.text, nullptr);
        }
    }
};

/// SnrLab module widget: 18HP expert surface laying out the curated fit knobs
/// on a 5x3 grid plus the trig/accent inputs and audio output.
struct SnrLabWidget : rack::ModuleWidget {
    /// Add a small knob at (xMm, yMm) and register its label 6mm above it.
    void addLabeledKnob(rack::engine::Module* module, int paramId,
                        float xMm, float yMm, const char* label, SnrLabPanel* panel) {
        addParam(createParamCentered<rack::RoundSmallBlackKnob>(
            mm2px(Vec(xMm, yMm)), module, paramId));
        panel->labels.push_back({xMm, yMm - 6.0f, label});
    }

    SnrLabWidget(SnrLab* module) {
        setModule(module);

        auto* panel = new SnrLabPanel;
        panel->box.size = AgentLayout::panelSize_18HP();
        addChild(panel);
        box.size = panel->box.size;

        AgentLayout::addScrews_18HP(this);

        const float xs[5] = {12.f, 28.f, 44.f, 60.f, 76.f};
        const float ys[3] = {24.f, 48.f, 72.f};
        struct Cell { int param; const char* label; };
        // These are the fit terms that consistently moved the snare during the
        // voice-lab passes: body drive, noise split, tone ceiling, overall
        // output, and the non-linear snappy contour. The rest stay internal
        // until they prove musically worth exposing or we decide the shared
        // Lab standard needs more than the current 18HP/16-control doctrine.
        Cell cells[] = {
            {SnrLab::TUNE_PARAM, "TUNE"},
            {SnrLab::TONE_PARAM, "TONE"},
            {SnrLab::SNAPPY_PARAM, "SNAP"},
            {SnrLab::LEVEL_PARAM, "LEVEL"},
            {SnrLab::BODY_DRIVE_PARAM, "BODYDRV"},
            {SnrLab::TONE_MAX_PARAM, "TMAX"},
            {SnrLab::NOISE_LP_PARAM, "NLP"},
            {SnrLab::NOISE_HP_PARAM, "NHP"},
            {SnrLab::LOW_NOISE_GAIN_PARAM, "LOW N"},
            {SnrLab::HIGH_NOISE_BASE_PARAM, "HI BASE"},
            {SnrLab::HIGH_NOISE_SNAPPY_PARAM, "HI SNAP"},
            {SnrLab::MIX_DRIVE_BASE_PARAM, "MIXDRV"},
            {SnrLab::OUTPUT_GAIN_PARAM, "OUT"},
            {SnrLab::SNAPPY_SHAPE_PARAM, "S-SHAPE"},
        };
        int idx = 0;
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 5 && idx < int(sizeof(cells) / sizeof(cells[0])); ++c, ++idx) {
                addLabeledKnob(module, cells[idx].param, xs[c], ys[r], cells[idx].label, panel);
            }
        }

        panel->labels.push_back({12.f, 94.f, "TRIG"});
        panel->labels.push_back({34.f, 94.f, "LACC"});
        panel->labels.push_back({56.f, 94.f, "TACC"});
        panel->labels.push_back({78.f, 94.f, "OUT"});
        addInput(createInputCentered<rack::PJ301MPort>(mm2px(Vec(12.f, 101.f)), module, SnrLab::TRIG_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(mm2px(Vec(34.f, 101.f)), module, SnrLab::LOCAL_ACC_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(mm2px(Vec(56.f, 101.f)), module, SnrLab::TOTAL_ACC_INPUT));
        addOutput(createOutputCentered<rack::PJ301MPort>(mm2px(Vec(78.f, 101.f)), module, SnrLab::OUT_OUTPUT));
    }
};

rack::Model* modelSnrLab = createModel<SnrLab, SnrLabWidget>("SnrLab");
