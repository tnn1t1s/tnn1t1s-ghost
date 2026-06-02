#include <rack.hpp>
#include "AgentModule.hpp"
#include "PanelLayout.hpp"
#include "GhostBus.hpp"
#include "ghost/signal/Audio.hpp"
#include "SnrEngine.hpp"
#include "SvgHelper.hpp"

using namespace rack;
extern Plugin* pluginInstance;

/**
 * Snr -- 909-style snare drum.
 *
 * Production voice module. The shared snare engine (SnrFit::Config voicing
 * recipe, the SnrSVF filter, and the snrTriangle / snrNormWithCV helpers)
 * lives in SnrEngine.hpp, shared with the SnrLab fitting bench
 * (lab/SnrLab.cpp). See SnrEngine.hpp for the internal model and the stable
 * Rack param/input/output ID list.
 */


/// Production 909-style snare voice. Front-panel surface is Tune/Tone/Snappy/
/// Level (plus CV and accent inputs); all voicing is fixed from SnrFit defaults.
struct Snr : GhostModule {

    enum ParamId  {
        TUNE_PARAM, TONE_PARAM, SNAPPY_PARAM, LEVEL_PARAM,
        NUM_PARAMS
    };
    enum InputId  {
        TRIG_INPUT, TUNE_CV_INPUT, TONE_CV_INPUT, SNAPPY_CV_INPUT, LEVEL_CV_INPUT,
        LOCAL_ACC_INPUT, TOTAL_ACC_INPUT,
        NUM_INPUTS
    };
    enum OutputId { OUT_OUTPUT, NUM_OUTPUTS };

    dsp::SchmittTrigger trigger;
    // Per-instance fit config: each Snr owns its own copy so multiple
    // instances never share or race a file-scope static.
    SnrFit::Config fit = SnrFit::defaults();
    // Default to a neutral mix until Snr accent is ear-tuned; keeps the
    // existing 909-snare voicing audibly unchanged when no controller is
    // wired or accent gates fire.
    Ghost::AccentMix accentMix = Ghost::Accent::gentleMix();
    float latchedCaseGain = 1.f;
    float latchedCharStrength = 0.f;

    float phase1   = 0.f;
    float phase2   = 0.f;
    float bodyEnv1 = 0.f;
    float bodyEnv2 = 0.f;
    float noiseLowEnv = 0.f;
    float noiseHighEnv = 0.f;
    float bendEnv  = 0.f;
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

    /// Configure the four params, CV/accent inputs, and audio output.
    Snr() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(TUNE_PARAM,   0.f, 1.f, 0.50f, "Tune",   "%", 0.f, 100.f);
        configParam(TONE_PARAM,   0.f, 1.f, 1.00f, "Tone",   "%", 0.f, 100.f);
        configParam(SNAPPY_PARAM, 0.f, 1.f, 1.00f, "Snappy", "%", 0.f, 100.f);
        configParam(LEVEL_PARAM,  0.f, 1.f, 0.82f, "Level",  "%", 0.f, 100.f);
        configInput (TRIG_INPUT,        "Trigger");
        configInput (TUNE_CV_INPUT,     "Tune CV");
        configInput (TONE_CV_INPUT,     "Tone CV");
        configInput (SNAPPY_CV_INPUT,   "Snappy CV");
        configInput (LEVEL_CV_INPUT,    "Level CV");
        configInput (LOCAL_ACC_INPUT,   "Local accent (Accent B, sampled at TRIG)");
        configInput (TOTAL_ACC_INPUT,   "Total accent (Accent A, sampled at TRIG)");
        configOutput(OUT_OUTPUT,        "Audio");
    }

    /// Return the snare to silence and a clean state on Rack "Initialize" /
    /// first load (zero envelopes, phases, filters, and accent latches).
    void onReset() override {
        trigger.reset();
        phase1 = phase2 = 0.f;
        bodyEnv1 = bodyEnv2 = 0.f;
        noiseLowEnv = noiseHighEnv = 0.f;
        bendEnv = 0.f;
        attackEnv = 1.f;
        clickEnv = 0.f;
        bodyLP = 0.f;
        lpNoise.reset();
        hpNoise.reset();
        latchedCaseGain = 1.f;
        latchedCharStrength = 0.f;
    }

    /// Synthesize one sample: on a rising TRIG, latch accent and reset all
    /// envelopes/phases; then sum the two-triangle body, low/high noise
    /// branches, and transient click into the soft-clipped, level-scaled output.
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
            bendEnv  = 1.f;
            attackEnv = 0.f;
            clickEnv = 1.f;
            phase1   = 0.f;
            phase2   = 0.f;
            bodyLP = 0.f;
            noisePhase = 0.f;
            prevBody = 0.f;
            prevNoise = noiseValue;
            lpNoise.reset();
            hpNoise.reset();
        }

        float tune_norm    = snrNormWithCV(*this, TUNE_PARAM,   TUNE_CV_INPUT);
        float tone_norm    = snrNormWithCV(*this, TONE_PARAM,   TONE_CV_INPUT);
        float snap_norm    = snrNormWithCV(*this, SNAPPY_PARAM, SNAPPY_CV_INPUT);
        float level_norm   = snrNormWithCV(*this, LEVEL_PARAM,  LEVEL_CV_INPUT);
        const SnrFit::Config& fit = this->fit;

        float tune_oct = (tune_norm - 0.5f) * 2.f * kSnrTuneOctRange;
        float scale    = std::pow(2.f, tune_oct);
        float bendOct  = bendEnv * fit.bendMaxOct;
        float f1       = fit.osc1BaseHz * scale * std::pow(2.f, bendOct);
        float f2       = fit.osc2BaseHz * scale * std::pow(2.f, bendOct * fit.osc2BendRatio);
        float toneTau  = kSnrToneMinSec + tone_norm * (fit.toneMaxSec - kSnrToneMinSec);
        float snapShape = std::pow(1.f - snap_norm, fit.snappyShapePower);
        float noiseLowTau = toneTau
                          * (fit.lowNoiseSnappyTauMinScale + snapShape * fit.lowNoiseSnappyTauDelta);
        float noiseHighTau = toneTau * fit.noiseHighRatio
                           * (1.f + snapShape * fit.highNoiseSnappyTauDelta);

        // --- body: two phase-reset triangles with different decays ------
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

        // --- noise: fixed binary source, split into low/high branches ----
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

        // --- envelope decays --------------------------------------------
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


// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------

/// Background panel for the production Snr: asset image plus the four control
/// labels and the two accent-input labels.
struct SnrPanel : rack::widget::Widget {
    void draw(const DrawArgs& args) override {
        AgentLayout::drawAssetPanel(
            args.vg, box.size, pluginInstance,
            "res/Snr-bg.jpg",
            nvgRGB(30, 16, 18),
            "SNR", nvgRGB(255, 120, 100));

        static const char* const LABELS[] = {
            "TUNE", "TONE", "SNAP", "LEVEL",
        };
        const float* ys = AgentLayout::kRowY8;
        nvgFontSize(args.vg, 5.5f);
        nvgFillColor(args.vg, nvgRGBA(255, 190, 180, 180));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (int i = 0; i < 4; i++) {
            nvgText(args.vg, mm2px(AgentLayout::kCenter12Hp), mm2px(ys[i]),
                    LABELS[i], NULL);
        }
        // Accent input row labels (column-anchored, like the IO row).
        nvgFontSize(args.vg, 4.5f);
        nvgFillColor(args.vg, nvgRGBA(255, 190, 180, 160));
        nvgText(args.vg, mm2px(AgentLayout::kLeftColumn12Hp),
                mm2px(ys[5] - 6.f), "LACC", NULL);
        nvgText(args.vg, mm2px(AgentLayout::kRightColumn12Hp),
                mm2px(ys[5] - 6.f), "TACC", NULL);
    }
};


// ---------------------------------------------------------------------------
// Widget -- 12HP, 8-row grid
// ---------------------------------------------------------------------------

/// Production Snr module widget: 12HP, 8-row grid wiring knobs/ports to the SVG.
struct SnrWidget : ModuleWidget, SvgHelper<SnrWidget> {
    SnrWidget(Snr* module) {
        setModule(module);
        loadPanel(asset::plugin(pluginInstance, "res/Snr.svg"));

        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        bindParam<RoundBlackKnob>("param.main.tune",   Snr::TUNE_PARAM);
        bindParam<RoundBlackKnob>("param.main.tone",   Snr::TONE_PARAM);
        bindParam<RoundBlackKnob>("param.main.snappy", Snr::SNAPPY_PARAM);
        bindParam<RoundBlackKnob>("param.main.level",  Snr::LEVEL_PARAM);

        bindInput<PJ301MPort>("cv.main.tune",   Snr::TUNE_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.tone",   Snr::TONE_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.snappy", Snr::SNAPPY_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.level",  Snr::LEVEL_CV_INPUT);

        bindInput<PJ301MPort>("trig.main.trig",    Snr::TRIG_INPUT);
        bindInput<PJ301MPort>("accent.main.local", Snr::LOCAL_ACC_INPUT);
        bindInput<PJ301MPort>("accent.main.total", Snr::TOTAL_ACC_INPUT);
        bindOutput<PJ301MPort>("out.main.audio",   Snr::OUT_OUTPUT);
    }
};


rack::Model* modelSnr = createModel<Snr, SnrWidget>("Snr");
