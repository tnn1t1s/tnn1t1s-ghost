#include <rack.hpp>
#include "AgentModule.hpp"
#include "GhostBus.hpp"
#include "GhostVoice.hpp"
#include "ghost/signal/Audio.hpp"
#include "ChhOhhEngine.hpp"
#include "SvgHelper.hpp"

using namespace rack;
extern Plugin* pluginInstance;

/**
 * ChhOhh -- 909-style closed + open hi-hat in a single module.
 *
 * The original 909 closed and open hi-hat share a single envelope/sound
 * circuit: a CH hit instantly mutes any sounding OH. We model that by
 * keeping both voices in the same module, where the choke is just
 * internal state (no cross-module bus signaling needed). This also
 * mirrors the CrashRide / RimClap pattern of grouping voices that
 * share a hardware path.
 *
 * Accent rails per the classic 909 voice layout:
 *   - CH has Accent B: responds to LOCAL_ACC and TOTAL_ACC
 *   - OH has only Accent A: responds to TOTAL_ACC
 *
 * Choke (issue #78): when the CH trigger fires, OH switches to a fast
 * release (~5ms tau) which decays its envelope to silence within ~25ms.
 * A subsequent OH trigger re-arms the voice and cancels any pending
 * choke state.
 */

/// Production combined closed + open hi-hat voice. Both hats share one module
/// so the CH-mutes-OH choke is internal state; CH carries Accent B, OH Accent A.
struct ChhOhh : GhostModule {
    enum ParamId {
        CHH_TUNE_PARAM,  CHH_DECAY_PARAM,  CHH_DRIVE_PARAM,  CHH_LEVEL_PARAM,
        OHH_TUNE_PARAM,  OHH_DECAY_PARAM,  OHH_DRIVE_PARAM,  OHH_LEVEL_PARAM,
        NUM_PARAMS
    };
    enum InputId {
        CHH_TRIG_INPUT, OHH_TRIG_INPUT,
        LOCAL_ACC_INPUT,   // CH only (Accent B); the classic 909: OH has no Accent B
        TOTAL_ACC_INPUT,   // shared by both voices (Accent A)
        // Per-knob CV for the panel controls (TUNE/DECAY/LEVEL each voice).
        // DRIVE is an internal/right-click param, so it has no CV jack.
        CHH_TUNE_CV_INPUT, CHH_DECAY_CV_INPUT, CHH_LEVEL_CV_INPUT,
        OHH_TUNE_CV_INPUT, OHH_DECAY_CV_INPUT, OHH_LEVEL_CV_INPUT,
        NUM_INPUTS
    };
    enum OutputId {
        CHH_OUT_OUTPUT, OHH_OUT_OUTPUT,
        NUM_OUTPUTS
    };

    // Per-sample synthesis (both hats + the choke) lives in ChhOhhVoice
    // (ChhOhhEngine.hpp), shared with the headless stress harness; the module
    // owns the Rack-side plumbing only.
    ChhOhhVoice voice;

    Ghost::AccentMix accentMix = Ghost::Accent::gentleMix();
    float chhLatchedGain = 1.f;
    float ohhLatchedGain = 1.f;

    ChhOhh() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(CHH_TUNE_PARAM,  0.f, 1.f, 0.50f, "Closed tune",  "%", 0.f, 100.f);
        configParam(CHH_DECAY_PARAM, 0.f, 1.f, 0.22f, "Closed decay", "%", 0.f, 100.f);
        configParam(CHH_DRIVE_PARAM, 0.f, 1.f, 0.10f, "Closed drive", "%", 0.f, 100.f);
        configParam(CHH_LEVEL_PARAM, 0.f, 1.f, 0.84f, "Closed level", "%", 0.f, 100.f);
        configParam(OHH_TUNE_PARAM,  0.f, 1.f, 0.50f, "Open tune",    "%", 0.f, 100.f);
        configParam(OHH_DECAY_PARAM, 0.f, 1.f, 0.58f, "Open decay",   "%", 0.f, 100.f);
        configParam(OHH_DRIVE_PARAM, 0.f, 1.f, 0.12f, "Open drive",   "%", 0.f, 100.f);
        configParam(OHH_LEVEL_PARAM, 0.f, 1.f, 0.82f, "Open level",   "%", 0.f, 100.f);
        configInput(CHH_TRIG_INPUT,  "Closed trigger");
        configInput(OHH_TRIG_INPUT,  "Open trigger");
        configInput(LOCAL_ACC_INPUT, "Local accent (Accent B; CH only)");
        configInput(TOTAL_ACC_INPUT, "Total accent (Accent A; shared)");
        configInput(CHH_TUNE_CV_INPUT,  "Closed tune CV");
        configInput(CHH_DECAY_CV_INPUT, "Closed decay CV");
        configInput(CHH_LEVEL_CV_INPUT, "Closed level CV");
        configInput(OHH_TUNE_CV_INPUT,  "Open tune CV");
        configInput(OHH_DECAY_CV_INPUT, "Open decay CV");
        configInput(OHH_LEVEL_CV_INPUT, "Open level CV");
        configOutput(CHH_OUT_OUTPUT, "Closed audio");
        configOutput(OHH_OUT_OUTPUT, "Open audio");
    }

    /// Return both voices to silence and a clean state on Rack "Initialize" /
    /// first load (zero envelopes, park read heads, drop the choke + latches).
    void onReset() override {
        voice.chhTrigger.reset();
        voice.ohhTrigger.reset();
        voice.chhSamplePos = voice.ohhSamplePos = 1e9f;
        voice.chhEnv = voice.ohhEnv = 0.f;
        voice.ohhChokeActive = false;
        chhLatchedGain = ohhLatchedGain = 1.f;
        voice.chhLatchedChar = voice.ohhLatchedChar = 0.f;
        voice.chhAir.reset();
        voice.ohhAir.reset();
    }

    /// Read a panel knob plus its CV input (CV/10), clamped to 0..1.
    /// (DRIVE is off-panel, so it has no CV jack.)
    float normWithCV(int paramId, int inputId) {
        return rack::math::clamp(
            params[paramId].getValue() + inputs[inputId].getVoltage() * 0.1f,
            0.f, 1.f);
    }

    /// Per-sample audio engine: handle triggers + choke, decay both envelopes,
    /// play back the embedded samples with tune/decay/drive/level/accent.
    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::resolveBus(this);

        // -- Closed hi-hat trigger -------------------------------------
        if (voice.chhTrigger.process(inputs[CHH_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            auto acc = Ghost::sampleAccentAtTrig(
                this, TOTAL_ACC_INPUT, bus, accentMix, LOCAL_ACC_INPUT);
            chhLatchedGain = acc.gain;
            // fireChh re-arms the CH voice and applies the CH->OH choke.
            voice.fireChh(acc.charStrength);
        }

        // -- Open hi-hat trigger ---------------------------------------
        if (voice.ohhTrigger.process(inputs[OHH_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            // OH has no Accent B; pass localInputId=-1 by default arg.
            auto acc = Ghost::sampleAccentAtTrig(
                this, TOTAL_ACC_INPUT, bus, accentMix);
            ohhLatchedGain = acc.gain;
            voice.fireOhh(acc.charStrength);
        }

        // -- Read controls once per frame for both voices -------------
        // TUNE/DECAY/LEVEL take per-knob CV; DRIVE is an internal/right-click
        // param (no panel knob, no CV) so it reads the param directly.
        float chhTune  = normWithCV(CHH_TUNE_PARAM,  CHH_TUNE_CV_INPUT);
        float chhDecay = normWithCV(CHH_DECAY_PARAM, CHH_DECAY_CV_INPUT);
        float chhDrive = rack::math::clamp(params[CHH_DRIVE_PARAM].getValue(), 0.f, 1.f);
        float chhLevel = normWithCV(CHH_LEVEL_PARAM, CHH_LEVEL_CV_INPUT);
        float ohhTune  = normWithCV(OHH_TUNE_PARAM,  OHH_TUNE_CV_INPUT);
        float ohhDecay = normWithCV(OHH_DECAY_PARAM, OHH_DECAY_CV_INPUT);
        float ohhDrive = rack::math::clamp(params[OHH_DRIVE_PARAM].getValue(), 0.f, 1.f);
        float ohhLevel = normWithCV(OHH_LEVEL_PARAM, OHH_LEVEL_CV_INPUT);

        float chhOut, ohhOut;
        voice.process(args,
                      chhTune, chhDecay, chhDrive, chhLevel,
                      ohhTune, ohhDecay, ohhDrive, ohhLevel,
                      chhOut, ohhOut);
        chhOut *= chhLatchedGain * bus.masterVolume;
        ohhOut *= ohhLatchedGain * bus.masterVolume;

        outputs[CHH_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(chhOut));
        outputs[OHH_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(ohhOut));
    }
};


// ---------------------------------------------------------------------------
// Panel: 14 HP, two voice sections stacked, shared accent inputs at bottom.
// Mirrors CrashRide's structure.
// ---------------------------------------------------------------------------

/// Right-click context-menu slider bound directly to a module param
/// (used to surface the off-panel DRIVE controls).
struct ChhOhhParamSlider : ui::Slider {
    ChhOhhParamSlider(engine::Module* m, int paramId, float widthPx = 200.f) {
        quantity = m->paramQuantities[paramId];
        box.size.x = widthPx;
    }
};

/// Panel widget for the production ChhOhh: two stacked voice sections in 14 HP
/// with shared accent inputs and DRIVE exposed via the right-click menu.
struct ChhOhhWidget : ModuleWidget, SvgHelper<ChhOhhWidget> {
    ChhOhhWidget(ChhOhh* module) {
        setModule(module);
        loadPanel(asset::plugin(pluginInstance, "res/ChhOhh.svg"));

        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        bindParam<RoundBlackKnob>("param.closed.tune",  ChhOhh::CHH_TUNE_PARAM);
        bindParam<RoundBlackKnob>("param.closed.decay", ChhOhh::CHH_DECAY_PARAM);
        bindParam<RoundBlackKnob>("param.closed.level", ChhOhh::CHH_LEVEL_PARAM);
        bindParam<RoundBlackKnob>("param.open.tune",    ChhOhh::OHH_TUNE_PARAM);
        bindParam<RoundBlackKnob>("param.open.decay",   ChhOhh::OHH_DECAY_PARAM);
        bindParam<RoundBlackKnob>("param.open.level",   ChhOhh::OHH_LEVEL_PARAM);

        bindInput<PJ301MPort>("cv.closed.tune",  ChhOhh::CHH_TUNE_CV_INPUT);
        bindInput<PJ301MPort>("cv.closed.decay", ChhOhh::CHH_DECAY_CV_INPUT);
        bindInput<PJ301MPort>("cv.closed.level", ChhOhh::CHH_LEVEL_CV_INPUT);
        bindInput<PJ301MPort>("cv.open.tune",    ChhOhh::OHH_TUNE_CV_INPUT);
        bindInput<PJ301MPort>("cv.open.decay",   ChhOhh::OHH_DECAY_CV_INPUT);
        bindInput<PJ301MPort>("cv.open.level",   ChhOhh::OHH_LEVEL_CV_INPUT);

        bindInput<PJ301MPort>("trig.closed.trig",   ChhOhh::CHH_TRIG_INPUT);
        bindInput<PJ301MPort>("trig.open.trig",     ChhOhh::OHH_TRIG_INPUT);
        bindInput<PJ301MPort>("accent.closed.local", ChhOhh::LOCAL_ACC_INPUT);
        bindInput<PJ301MPort>("accent.main.total",   ChhOhh::TOTAL_ACC_INPUT);
        bindOutput<PJ301MPort>("out.closed.audio",  ChhOhh::CHH_OUT_OUTPUT);
        bindOutput<PJ301MPort>("out.open.audio",    ChhOhh::OHH_OUT_OUTPUT);
    }

    void appendContextMenu(Menu* menu) override {
        if (!module) return;
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Drive (internal saturation)"));
        menu->addChild(new ChhOhhParamSlider(module, ChhOhh::CHH_DRIVE_PARAM));
        menu->addChild(new ChhOhhParamSlider(module, ChhOhh::OHH_DRIVE_PARAM));
    }
};

rack::Model* modelChhOhh = createModel<ChhOhh, ChhOhhWidget>("ChhOhh");
