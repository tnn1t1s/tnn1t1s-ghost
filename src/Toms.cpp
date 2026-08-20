#include <rack.hpp>
#include "AgentModule.hpp"
#include "GhostBus.hpp"
#include "ghost/signal/Audio.hpp"
#include "TomsEngine.hpp"
#include "SvgHelper.hpp"

using namespace rack;
extern Plugin* pluginInstance;

/**
 * Toms -- 909-style toms (LowTom / MidTom / HighTom).
 *
 * Production voice module. The shared tom engine (TomFit::Config recipe, the
 * makeLowTom/makeMidTom/makeHighTom factories and the TomVoice DSP struct)
 * lives in TomsEngine.hpp, shared with the TomLab fitting bench
 * (lab/TomLab.cpp). See TomsEngine.hpp for the full per-sample algorithm and
 * the stable Rack param/input/output ID list.
 */


// ---------------------------------------------------------------------------
// Toms -- 3-voice tom kit (Low / Mid / High) in one module.
//
// Mirrors RimClap's pattern: per-voice trigger + level + audio output, plus
// one shared accent input. Internal tune/decay use the calibrated defaults
// from TomFit. Use TomLab if you want to sweep internal voicing parameters.
// ---------------------------------------------------------------------------

/// Production 3-voice tom kit (Low/Mid/High) in one module: per-voice
/// TUNE/DECAY/LEVEL with CV plus a shared accent input. See banner.
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
    // Toms use a wider accent swing than the shared Accent::gentleMix()
    // +3dB (Snr/ChhOhh/CrashRide/RimClap) -- +3dB wasn't audible against a
    // full kit; +7dB is Toms-only, ear-tuned.
    Ghost::AccentMix accentMix = [] {
        Ghost::AccentMix m = Ghost::Accent::gentleMix();
        m.globalDb = 7.f;
        m.bothDb   = 7.f;
        return m;
    }();
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

    /// Knob value plus CV input scaled by 1/10 V, clamped to [0,1].
    float normWithCV(int paramId, int inputId) {
        return rack::math::clamp(
            params[paramId].getValue() + inputs[inputId].getVoltage() * 0.1f,
            0.f, 1.f);
    }

    /// Fire any voice whose trigger edged this sample (latching its accent),
    /// then render all three voices to their separate outputs scaled by the
    /// per-voice case gain and the shared master volume.
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

/// Module widget for the production Toms: binds the SVG panel's knobs, CV,
/// trigger, accent and audio-out ports to their param/port ids.
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
