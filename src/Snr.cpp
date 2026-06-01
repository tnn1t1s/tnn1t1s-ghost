#include <rack.hpp>
#include "AgentModule.hpp"
#include "PanelLayout.hpp"
#include "NineOhNinePanel.hpp"
#include "Tr909Bus.hpp"
#include "ghost/signal/Audio.hpp"
#include <cmath>
#include "SvgHelper.hpp"

using namespace rack;
extern Plugin* pluginInstance;

/**
 * Snr -- TR-909 inspired snare drum.
 *
 * The original 909 snare is a balanced instrument, not a generic synth patch:
 * two trigger-reset triangle VCOs supply the body, and a quasi-random binary
 * noise source supplies the upper spectrum. The front-panel controls on the
 * original machine are Tune, Tone, Snappy, and Level, so this module now keeps
 * that same surface and fixes the rest internally.
 *
 * Internal model:
 *   body  = two phase-reset triangle oscillators with a short pitch bend and
 *           slightly different decay times
 *   noise = binary-noise generator split into low/high branches with separate
 *           envelopes; Tone sets the noise duration, Snappy sets the gain
 *   mix   = body + low-noise + high-noise + short attack click
 *
 * Rack IDs (stable for the current 909 module generation):
 *   Params:  TUNE=0, TONE=1, SNAPPY=2, LEVEL=3
 *   Inputs:  TRIG=0, TUNE_CV=1, TONE_CV=2, SNAPPY_CV=3, LEVEL_CV=4
 *   Outputs: OUT=0
 */

static constexpr float SNR_TUNE_OCT_RANGE    = 0.75f;
static constexpr float SNR_TONE_MIN_SEC      = 0.018f;
static constexpr float SNR_NOISE_HIGH_Q      = 0.78f;
static constexpr float SNR_NOISE_LOW_Q       = 0.82f;
static constexpr float SNR_CV_SCALE          = 0.1f;

static inline float snrNormWithCV(rack::Module& self, int paramId, int inputId) {
    float norm = self.params[paramId].getValue()
               + self.inputs[inputId].getVoltage() * SNR_CV_SCALE;
    return rack::math::clamp(norm, 0.f, 1.f);
}

static inline float snrTriangle(float phase) {
    return 1.f - 4.f * std::fabs(phase - 0.5f);
}

// TPT SVF used for the snare-noise branches.
struct SnrSVF {
    float ic1 = 0.f, ic2 = 0.f;
    float lpf = 0.f;
    float hpf = 0.f;
    void reset() { ic1 = ic2 = lpf = hpf = 0.f; }
    void process(float x, float fHz, float sampleRate, float Q) {
        float g = std::tan(float(M_PI) * fHz / sampleRate);
        float k = 1.f / Q;
        float a1 = 1.f / (1.f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;
        float v3 = x - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        lpf = v2;
        hpf = x - k * v1 - v2;
    }
};

namespace SnrFit {
struct Config {
    float osc1BaseHz = 157.655031f;
    float osc2BaseHz = 332.641481f;
    float body1TauSec = 0.025189178f;
    float body2TauSec = 0.020462034f;
    float bodyLpHz = 1273.814504f;
    float toneMaxSec = 0.106f;
    float noiseHighRatio = 0.713894547f;
    float noiseClockHz = 14285.683594f;
    float noiseLpHz = 9278.987305f;
    float noiseHpHz = 3907.570801f;
    float bendMaxOct = 0.46f;
    float bendTauSec = 0.020f;
    float attackTauSec = 0.000685407f;
    float clickTauSec = 0.0015f;
    float body1Gain = 1.053525281f;
    float body2Gain = 0.196282045f;
    float bodyDrive = 1.028451443f;
    float lowNoiseGain = 0.074649002f;
    float lowNoiseToneBase = 0.1f;
    float lowNoiseToneSpan = 0.9f;
    float snappyShapePower = 1.5f;
    float lowNoiseSnappyGainDelta = 0.f;
    float lowNoiseSnappyTauMinScale = 0.45f;
    float lowNoiseSnappyTauDelta = 4.f;
    float highNoiseBase = 0.01f;
    float highNoiseSnappy = 0.42f;
    float highNoiseToneBase = 1.f;
    float highNoiseToneSpan = 0.f;
    float highNoiseSnappyTauDelta = 0.f;
    float clickBodyGain = 0.171073779f;
    float clickNoiseGain = 0.075186349f;
    float mixDriveBase = 0.960915566f;
    float mixDriveSnappy = 0.058808930f;
    float outputGain = 0.86f;
    float osc2BendRatio = 0.751340272f;
    // Per-DSP-stage CHARACTER weights, applied via accentScale / accentAdd
    // when charStrength > 0. Snr is the only voice with circuit-faithful
    // timbral accent on the original 909 (gates noise and tone VCAs);
    // noiseAmt is the audible signature. AccentCharacter member order is
    // body, pitch, click, drive, noise, snap, decay, brightness, bend.
    Ghost::TR909::AccentCharacter accent =
        { 0.f, 0.f, 0.f, 0.08f, 0.12f, 0.f, 0.f, 0.f, 0.f };
    //  body, pitch, click, drive, noise, snap, decay, brightness, bend
};

static inline const Config& defaults() {
    static const Config cfg;
    return cfg;
}

static Config current = defaults();

static inline void reset() {
    current = defaults();
}
}  // namespace SnrFit


struct Snr : Tr909Module {

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
    // Default to a neutral mix until Snr accent is ear-tuned; keeps the
    // existing 909-snare voicing audibly unchanged when no controller is
    // wired or accent gates fire.
    Ghost::TR909::AccentMix accentMix = Ghost::TR909::neutralMix();
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

    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::TR909::resolveBus(this);
        if (trigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            auto acc = Ghost::TR909::sampleAccentAtTrig(
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
        const SnrFit::Config& fit = SnrFit::current;

        float tune_oct = (tune_norm - 0.5f) * 2.f * SNR_TUNE_OCT_RANGE;
        float scale    = std::pow(2.f, tune_oct);
        float bendOct  = bendEnv * fit.bendMaxOct;
        float f1       = fit.osc1BaseHz * scale * std::pow(2.f, bendOct);
        float f2       = fit.osc2BaseHz * scale * std::pow(2.f, bendOct * fit.osc2BendRatio);
        float toneTau  = SNR_TONE_MIN_SEC + tone_norm * (fit.toneMaxSec - SNR_TONE_MIN_SEC);
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
        lpNoise.process(noiseValue, fit.noiseLpHz, args.sampleRate, SNR_NOISE_LOW_Q);
        hpNoise.process(noiseValue, fit.noiseHpHz, args.sampleRate, SNR_NOISE_HIGH_Q);
        float lowNoiseGain = fit.lowNoiseGain
                           * (fit.lowNoiseToneBase + tone_norm * fit.lowNoiseToneSpan)
                           * (1.f + snapShape * fit.lowNoiseSnappyGainDelta);
        float lowNoise = lpNoise.lpf * noiseLowEnv * lowNoiseGain;
        float highNoiseGain = (fit.highNoiseBase + snap_norm * fit.highNoiseSnappy)
                            * (fit.highNoiseToneBase + tone_norm * fit.highNoiseToneSpan);
        highNoiseGain = Ghost::TR909::accentScale(
            highNoiseGain, latchedCharStrength, fit.accent.noiseAmt);
        float highNoise = hpNoise.hpf * noiseHighEnv * highNoiseGain;
        float click = ((body - prevBody) * fit.clickBodyGain + (noiseValue - prevNoise) * fit.clickNoiseGain) * clickEnv;
        prevBody = body;
        prevNoise = noiseValue;

        // --- envelope decays --------------------------------------------
        bodyEnv1 *= std::exp(-args.sampleTime / fit.body1TauSec);
        bodyEnv2 *= std::exp(-args.sampleTime / fit.body2TauSec);
        noiseLowEnv *= std::exp(-args.sampleTime / noiseLowTau);
        noiseHighEnv *= std::exp(-args.sampleTime / noiseHighTau);
        bendEnv *= std::exp(-args.sampleTime / fit.bendTauSec);
        attackEnv += (1.f - attackEnv) * (1.f - std::exp(-args.sampleTime / fit.attackTauSec));
        clickEnv *= std::exp(-args.sampleTime / fit.clickTauSec);

        float mix = body + lowNoise + highNoise + click;
        mix = std::tanh(mix * Ghost::TR909::accentAdd(
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
        const float* ys = AgentLayout::ROW_Y_8;
        nvgFontSize(args.vg, 5.5f);
        nvgFillColor(args.vg, nvgRGBA(255, 190, 180, 180));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (int i = 0; i < 4; i++) {
            nvgText(args.vg, mm2px(AgentLayout::CENTER_12HP), mm2px(ys[i]),
                    LABELS[i], NULL);
        }
        // Accent input row labels (column-anchored, like the IO row).
        nvgFontSize(args.vg, 4.5f);
        nvgFillColor(args.vg, nvgRGBA(255, 190, 180, 160));
        nvgText(args.vg, mm2px(AgentLayout::LEFT_COLUMN_12HP),
                mm2px(ys[5] - 6.f), "LACC", NULL);
        nvgText(args.vg, mm2px(AgentLayout::RIGHT_COLUMN_12HP),
                mm2px(ys[5] - 6.f), "TACC", NULL);
    }
};


// ---------------------------------------------------------------------------
// Widget -- 12HP, 8-row grid
// ---------------------------------------------------------------------------

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


// ---------------------------------------------------------------------------
// SnrLab -- expanded expert / fitting variant.
//
// Keeps the 909-facing Tune / Tone / Snappy / Level surface, then exposes a
// focused subset of the hidden fit variables that most directly change the
// voicing. This is intentionally narrower than the full internal Config: the
// goal is a playable expert module, not a raw dump of every constant. SnrLab
// follows the same 18HP Lab doctrine as the rest of the 909 family: keep the
// module small enough to patch musically, but large enough to surface the fit
// controls that actually moved the sound during voice-lab passes.
// ---------------------------------------------------------------------------

struct SnrLab : Tr909Module {
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
    Ghost::TR909::AccentMix accentMix = Ghost::TR909::neutralMix();
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

    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::TR909::resolveBus(this);
        if (trigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            auto acc = Ghost::TR909::sampleAccentAtTrig(
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

        float tune_oct = (tune_norm - 0.5f) * 2.f * SNR_TUNE_OCT_RANGE;
        float scale = std::pow(2.f, tune_oct);
        float bendOct = bendEnv * fit.bendMaxOct;
        float f1 = fit.osc1BaseHz * scale * std::pow(2.f, bendOct);
        float f2 = fit.osc2BaseHz * scale * std::pow(2.f, bendOct * fit.osc2BendRatio);
        float toneTau = SNR_TONE_MIN_SEC + tone_norm * (fit.toneMaxSec - SNR_TONE_MIN_SEC);
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
        float body = std::tanh(bodyLP * fit.bodyDrive);

        noisePhase += fit.noiseClockHz * args.sampleTime;
        while (noisePhase >= 1.f) {
            noisePhase -= 1.f;
            uint32_t newBit = ((noiseShift >> 0) ^ (noiseShift >> 2)
                             ^ (noiseShift >> 3) ^ (noiseShift >> 5)) & 1u;
            noiseShift = (noiseShift >> 1) | (newBit << 15);
            noiseValue = (noiseShift & 1u) ? 1.f : -1.f;
        }
        lpNoise.process(noiseValue, fit.noiseLpHz, args.sampleRate, SNR_NOISE_LOW_Q);
        hpNoise.process(noiseValue, fit.noiseHpHz, args.sampleRate, SNR_NOISE_HIGH_Q);
        float lowNoiseGain = fit.lowNoiseGain
                           * (fit.lowNoiseToneBase + tone_norm * fit.lowNoiseToneSpan)
                           * (1.f + snapShape * fit.lowNoiseSnappyGainDelta);
        float lowNoise = lpNoise.lpf * noiseLowEnv * lowNoiseGain;
        float highNoiseGain = (fit.highNoiseBase + snap_norm * fit.highNoiseSnappy)
                            * (fit.highNoiseToneBase + tone_norm * fit.highNoiseToneSpan);
        highNoiseGain = Ghost::TR909::accentScale(
            highNoiseGain, latchedCharStrength, fit.accent.noiseAmt);
        float highNoise = hpNoise.hpf * noiseHighEnv * highNoiseGain;
        float click = ((body - prevBody) * fit.clickBodyGain + (noiseValue - prevNoise) * fit.clickNoiseGain) * clickEnv;
        prevBody = body;
        prevNoise = noiseValue;

        bodyEnv1 *= std::exp(-args.sampleTime / fit.body1TauSec);
        bodyEnv2 *= std::exp(-args.sampleTime / fit.body2TauSec);
        noiseLowEnv *= std::exp(-args.sampleTime / noiseLowTau);
        noiseHighEnv *= std::exp(-args.sampleTime / noiseHighTau);
        bendEnv *= std::exp(-args.sampleTime / fit.bendTauSec);
        attackEnv += (1.f - attackEnv) * (1.f - std::exp(-args.sampleTime / fit.attackTauSec));
        clickEnv *= std::exp(-args.sampleTime / fit.clickTauSec);

        float mix = body + lowNoise + highNoise + click;
        mix = std::tanh(mix * Ghost::TR909::accentAdd(
            fit.mixDriveBase + fit.mixDriveSnappy * snap_norm,
            latchedCharStrength, fit.accent.driveAmt));
        mix *= attackEnv;

        float out = mix * level_norm * fit.outputGain;
        out *= latchedCaseGain * bus.masterVolume;
        outputs[OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(out));
    }
};

struct SnrLabLabelCell { float xMm, yMm; const char* text; };

struct SnrLabPanel : rack::widget::Widget {
    std::vector<SnrLabLabelCell> labels;

    void draw(const DrawArgs& args) override {
        Ghost::NineOhNine::drawLabShell(
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

struct SnrLabWidget : rack::ModuleWidget {
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
