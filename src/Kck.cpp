#include <rack.hpp>
#include "AgentModule.hpp"
#include "GhostBus.hpp"
#include "ghost/signal/Audio.hpp"
#include "KckEngine.hpp"
#include "SvgHelper.hpp"

using namespace rack;
extern Plugin* pluginInstance;

/**
 * Kck -- 909-style kick drum.
 *
 * Production voice module. The shared kick engine (KckFit::Config recipe and
 * the KckVoice DSP struct) lives in KckEngine.hpp, shared with the KckLab
 * fitting bench (lab/KckLab.cpp). See KckEngine.hpp for the full per-sample
 * algorithm and the stable Rack param/input/output ID list.
 */


// ---------------------------------------------------------------------------
// Kck -- production module
// ---------------------------------------------------------------------------

/// Production kick module: seven playable knobs (+ ATTACK/TONE macros) over a
/// single KckVoice, with bus master volume and the two-axis accent system.
struct Kck : GhostModule {
    enum ParamId  {
        TUNE_PARAM, DECAY_PARAM, PITCH_PARAM, PITCH_DECAY_PARAM,
        CLICK_PARAM, DRIVE_PARAM, LEVEL_PARAM,
        ATTACK_PARAM, TONE_PARAM,
        NUM_PARAMS
    };
    enum InputId  {
        TRIG_INPUT, TUNE_CV_INPUT, DECAY_CV_INPUT, PITCH_CV_INPUT,
        PITCH_DECAY_CV_INPUT, CLICK_CV_INPUT, DRIVE_CV_INPUT, LEVEL_CV_INPUT,
        ATTACK_CV_INPUT, TONE_CV_INPUT,
        LOCAL_ACC_INPUT, TOTAL_ACC_INPUT,
        NUM_INPUTS
    };
    enum OutputId { OUT_OUTPUT, NUM_OUTPUTS };

    KckVoice voice;
    KckFit::Config fit;

    /// Configure params/inputs/outputs and load the default kick recipe.
    Kck() {
        fit = KckFit::makeKick();
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        // Calibrated 2026-06-01: TUNE=100% + every shaping knob at noon = the reference match.
        configParam(TUNE_PARAM,        0.f, 1.f, 1.00f, "Tune",         "%", 0.f, 100.f);
        configParam(DECAY_PARAM,       0.f, 1.f, 0.50f, "Decay",        "%", 0.f, 100.f);
        configParam(PITCH_PARAM,       0.f, 1.f, 0.50f, "Pitch amount", "%", 0.f, 100.f);
        configParam(PITCH_DECAY_PARAM, 0.f, 1.f, 0.50f, "Pitch decay",  "%", 0.f, 100.f);
        configParam(CLICK_PARAM,       0.f, 1.f, 0.50f, "Click",        "%", 0.f, 100.f);
        configParam(DRIVE_PARAM,       0.f, 1.f, 0.50f, "Drive",        "%", 0.f, 100.f);
        configParam(LEVEL_PARAM,       0.f, 1.f, 0.85f, "Level",        "%", 0.f, 100.f);
        configParam(ATTACK_PARAM,      0.f, 1.f, 0.50f, "Attack",       "%", 0.f, 100.f);
        configParam(TONE_PARAM,        0.f, 1.f, 0.50f, "Tone",         "%", 0.f, 100.f);
        configInput (TRIG_INPUT,           "Trigger");
        configInput (TUNE_CV_INPUT,        "Tune CV");
        configInput (DECAY_CV_INPUT,       "Decay CV");
        configInput (PITCH_CV_INPUT,       "Pitch amount CV");
        configInput (PITCH_DECAY_CV_INPUT, "Pitch decay CV");
        configInput (CLICK_CV_INPUT,       "Click CV");
        configInput (DRIVE_CV_INPUT,       "Drive CV");
        configInput (LEVEL_CV_INPUT,       "Level CV");
        configInput (ATTACK_CV_INPUT,      "Attack CV");
        configInput (TONE_CV_INPUT,        "Tone CV");
        configInput (LOCAL_ACC_INPUT,      "Local accent (Accent B, sampled at TRIG)");
        configInput (TOTAL_ACC_INPUT,      "Total accent (Accent A, sampled at TRIG)");
        configOutput(OUT_OUTPUT,           "Audio");
    }

    // Latched at TRIG rising edge along with voice.latchedAccent. Determines
    // the per-case output level (ghost / global / local / both dB lookup);
    // see GhostBus.hpp::resolveAccentGain for the math. Held constant for
    // the duration of the hit so knob movements during a hit don't shift
    // its level mid-flight.
    float latchedCaseGain = 1.f;

    /// Return the voice to silence and clean latched state on Rack "Initialize" /
    /// first load (reset trigger, park DSP/filter/envelope state, drop accent latches).
    void onReset() override {
        voice.trigger.reset();
        voice.phase = voice.phaseSub = 0.f;
        voice.t = 0.f;
        voice.active = false;
        voice.hpState = 0.f;
        voice.bodyLp = voice.clickLp = 0.f;
        voice.svfLp = voice.svfBp = 0.f;
        voice.latchedAccent = 0.f;
        latchedCaseGain = 1.f;
    }

    /// Per-sample: latch accent on a TRIG rising edge, map the playable knobs
    /// (with CV) into a per-frame config, run the voice, and apply per-case
    /// accent gain and bus master volume to the output.
    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::resolveBus(this);

        if (voice.trigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            // Hit-time gates from cables (deterministic, zero latency).
            const bool totalGate = inputs[TOTAL_ACC_INPUT].getNormalVoltage(0.f) > 1.f;
            const bool localGate = inputs[LOCAL_ACC_INPUT].getNormalVoltage(0.f) > 1.f;

            // Two orthogonal axes:
            //   character: boolean -- voice DSP applies its full accent
            //              feel (drive, pitch dive, click) on any accent.
            //   level:     per-case dB lerp from AccentMix, latched once.
            const float charStrength =
                Ghost::isAccentedHit(totalGate, localGate) ? 1.f : 0.f;
            latchedCaseGain = Ghost::resolveAccentGain(
                totalGate, localGate, bus, fit.accentMix);
            voice.fire(charStrength);
        }

        float tuneNorm       = kckNormWithCV(*this, TUNE_PARAM,        TUNE_CV_INPUT);
        float decayNorm      = kckNormWithCV(*this, DECAY_PARAM,       DECAY_CV_INPUT);
        float pitchNorm      = kckNormWithCV(*this, PITCH_PARAM,       PITCH_CV_INPUT);
        float pitchDecayNorm = kckNormWithCV(*this, PITCH_DECAY_PARAM, PITCH_DECAY_CV_INPUT);
        float clickNorm      = kckNormWithCV(*this, CLICK_PARAM,       CLICK_CV_INPUT);
        float driveNorm      = kckNormWithCV(*this, DRIVE_PARAM,       DRIVE_CV_INPUT);
        float levelNorm      = kckNormWithCV(*this, LEVEL_PARAM,       LEVEL_CV_INPUT);
        float attackNorm     = kckNormWithCV(*this, ATTACK_PARAM,      ATTACK_CV_INPUT);
        float toneNorm       = kckNormWithCV(*this, TONE_PARAM,        TONE_CV_INPUT);

        // Calibration (2026-06-01 reference match): the matched sound is TUNE=100%
        // with every shaping knob at noon. CLICK/DRIVE are remapped so noon hits
        // the matched values; ATTACK/TONE macro the click sharpness / body
        // brightness so noon -> matched clickRate 140 / bodyFcMult 1.30.
        KckFit::Config f = fit;
        // ATTACK solely controls click rate/tightness (decoupled from CLICK). GEOMETRIC
        // map (reuse Ghost::expDecaySec) so the audible change spreads evenly across the
        // knob instead of cramming into the bottom 10% -- rate is perceived
        // logarithmically, like decay. 55..1385 -> noon ~277 = the matched click;
        // long smeared click (CCW) -> ultra-tight tick (CW).
        f.clickRateBase = Ghost::expDecaySec(attackNorm, 12.f, 1500.f);
        f.bodyFcMult    = 0.70f + 1.20f * toneNorm;           // TONE=0.5   -> 1.30
        const float clickEff = rack::math::clamp(0.305f + clickNorm, 0.f, 1.f);  // CLICK=0.5 -> 0.805
        // DRIVE: drives the WHOLE kick at the final saturation stage (KckEngine
        // ~L307), the audible lever, not the body-only clip (which the dark TONE
        // filters out). noon (0.5) -> driveEff 0 -> driveBase exact (calibrated
        // kick). CCW cleaner, CW crushed. Effect knob: invisible at noon, obvious
        // at the ends.
        const float driveEff = rack::math::clamp((driveNorm - 0.5f) * 2.f, -1.f, 1.f);

        float out = voice.process(args, f,
                                  tuneNorm, decayNorm, pitchNorm, pitchDecayNorm,
                                  clickEff, driveEff, levelNorm);
        out *= latchedCaseGain * bus.masterVolume;
        outputs[OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(out));
    }
};


// ---------------------------------------------------------------------------
// Production widget -- panelkit SVG (res/Kck.svg) + SvgHelper name binding
// ---------------------------------------------------------------------------

/// Production panel widget: binds knobs/ports to the res/Kck.svg layout by name.
struct KckWidget : ModuleWidget, SvgHelper<KckWidget> {
    KckWidget(Kck* module) {
        setModule(module);
        loadPanel(asset::plugin(pluginInstance, "res/Kck.svg"));

        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        bindParam<RoundBlackKnob>("param.main.tune",        Kck::TUNE_PARAM);
        bindParam<RoundBlackKnob>("param.main.decay",       Kck::DECAY_PARAM);
        bindParam<RoundBlackKnob>("param.main.pitch",       Kck::PITCH_PARAM);
        bindParam<RoundBlackKnob>("param.main.pitch-decay", Kck::PITCH_DECAY_PARAM);
        bindParam<RoundBlackKnob>("param.main.click",       Kck::CLICK_PARAM);
        bindParam<RoundBlackKnob>("param.main.attack",      Kck::ATTACK_PARAM);
        bindParam<RoundBlackKnob>("param.main.tone",        Kck::TONE_PARAM);
        bindParam<RoundBlackKnob>("param.main.drive",       Kck::DRIVE_PARAM);
        bindParam<RoundBlackKnob>("param.main.level",       Kck::LEVEL_PARAM);

        bindInput<PJ301MPort>("cv.main.tune",        Kck::TUNE_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.decay",       Kck::DECAY_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.pitch",       Kck::PITCH_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.pitch-decay", Kck::PITCH_DECAY_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.click",       Kck::CLICK_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.attack",      Kck::ATTACK_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.tone",        Kck::TONE_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.drive",       Kck::DRIVE_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.level",       Kck::LEVEL_CV_INPUT);

        bindInput<PJ301MPort>("trig.main.trig",     Kck::TRIG_INPUT);
        bindInput<PJ301MPort>("accent.main.local",  Kck::LOCAL_ACC_INPUT);
        bindInput<PJ301MPort>("accent.main.total",  Kck::TOTAL_ACC_INPUT);
        bindOutput<PJ301MPort>("out.main.audio",    Kck::OUT_OUTPUT);
    }
};

rack::Model* modelKck = createModel<Kck, KckWidget>("Kck");
