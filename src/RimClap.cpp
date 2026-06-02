#include <rack.hpp>
#include "GhostBus.hpp"
#include "ghost/signal/Audio.hpp"
#include "RimClapEngine.hpp"
#include "SvgHelper.hpp"

using namespace rack;
extern Plugin* pluginInstance;

/// Combined rim-shot + hand-clap voice (909-style): two ROM sample players with
/// per-voice TUNE/LEVEL knobs and CV, sharing one accent input. The shipped
/// minimal surface; see RimClapLab for the expert variant.
struct RimClap : GhostModule {
    // GHOST surface: each voice is fully tunable -- TUNE (playback-rate /
    // pitch) and LEVEL, each with a CV jack -- so the module plays as a
    // machine rather than needing hands. IDs finalized for release.
    enum ParamId {
        CLAP_TUNE_PARAM,
        CLAP_LEVEL_PARAM,
        RIM_TUNE_PARAM,
        RIM_LEVEL_PARAM,
        NUM_PARAMS
    };
    // Per the classic 909 voice layout, neither RS nor CP has Accent B; they share a
    // single TOTAL_ACC_INPUT (Accent A). Each voice latches the case gain
    // independently at its own trigger edge. Per-knob CV follows the gates.
    enum InputId {
        CLAP_TRIG_INPUT,
        RIM_TRIG_INPUT,
        TOTAL_ACC_INPUT,
        CLAP_TUNE_CV_INPUT,
        CLAP_LEVEL_CV_INPUT,
        RIM_TUNE_CV_INPUT,
        RIM_LEVEL_CV_INPUT,
        NUM_INPUTS
    };
    enum OutputId {
        CLAP_OUT_OUTPUT,
        RIM_OUT_OUTPUT,
        NUM_OUTPUTS
    };

    dsp::SchmittTrigger clapTrigger;
    dsp::SchmittTrigger rimTrigger;
    RomVoice clapVoice;
    RomVoice rimVoice;
    // Un-accented = normal (0 dB); accent adds a gentle ~+3 dB on top (909-style).
    Ghost::AccentMix accentMix = Ghost::Accent::gentleMix();
    float clapLatchedGain = 1.f;
    float rimLatchedGain  = 1.f;
    float clapLatchedChar = 0.f;
    float rimLatchedChar  = 0.f;

    /// knob + CV/10, clamped to the knob's normalized 0..1 range.
    float normWithCV(int paramId, int inputId) {
        return rack::math::clamp(
            params[paramId].getValue() + inputs[inputId].getVoltage() * 0.1f,
            0.f, 1.f);
    }
    /// Map a TUNE knob position (0..1) to a playback-rate multiplier, one octave
    /// each way (matches Lab).
    static float tuneToRate(float tuneNorm) {
        return std::pow(2.f, (tuneNorm - 0.5f) * 2.f);
    }

    RimClap() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(CLAP_TUNE_PARAM,  0.f, 1.f, 0.50f, "Clap tune",  "%", 0.f, 100.f);
        configParam(CLAP_LEVEL_PARAM, 0.f, 1.f, 0.90f, "Clap level", "%", 0.f, 100.f);
        configParam(RIM_TUNE_PARAM,   0.f, 1.f, 0.50f, "Rim tune",   "%", 0.f, 100.f);
        configParam(RIM_LEVEL_PARAM,  0.f, 1.f, 0.90f, "Rim level",  "%", 0.f, 100.f);
        configInput(CLAP_TRIG_INPUT, "Clap trigger");
        configInput(RIM_TRIG_INPUT, "Rim trigger");
        configInput(TOTAL_ACC_INPUT, "Total accent (Accent A, sampled at TRIG; shared)");
        configInput(CLAP_TUNE_CV_INPUT,  "Clap tune CV");
        configInput(CLAP_LEVEL_CV_INPUT, "Clap level CV");
        configInput(RIM_TUNE_CV_INPUT,   "Rim tune CV");
        configInput(RIM_LEVEL_CV_INPUT,  "Rim level CV");
        configOutput(CLAP_OUT_OUTPUT, "Clap audio");
        configOutput(RIM_OUT_OUTPUT, "Rim audio");
    }

    /// Return both voices to silence and a clean state on Rack "Initialize" /
    /// first load (park read heads, drop the accent latches).
    void onReset() override {
        clapTrigger.reset();
        rimTrigger.reset();
        clapVoice.pos = rimVoice.pos = 1e9f;
        clapLatchedGain = rimLatchedGain = 1.f;
        clapLatchedChar = rimLatchedChar = 0.f;
    }

    /// Per-sample DSP: latch accent at each voice's trigger edge, then play both
    /// ROM voices with TUNE/LEVEL (+ CV) and accent drive into the two outputs.
    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::resolveBus(this);
        if (clapTrigger.process(inputs[CLAP_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            clapVoice.trigger();
            auto acc = Ghost::sampleAccentAtTrig(
                this, TOTAL_ACC_INPUT, bus, accentMix);
            clapLatchedChar = acc.charStrength;
            clapLatchedGain = acc.gain;
        }
        if (rimTrigger.process(inputs[RIM_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            rimVoice.trigger();
            auto acc = Ghost::sampleAccentAtTrig(
                this, TOTAL_ACC_INPUT, bus, accentMix);
            rimLatchedChar = acc.charStrength;
            rimLatchedGain = acc.gain;
        }

        float clapLevel = normWithCV(CLAP_LEVEL_PARAM, CLAP_LEVEL_CV_INPUT);
        float rimLevel  = normWithCV(RIM_LEVEL_PARAM, RIM_LEVEL_CV_INPUT);
        float clapRate  = tuneToRate(normWithCV(CLAP_TUNE_PARAM, CLAP_TUNE_CV_INPUT));
        float rimRate   = tuneToRate(normWithCV(RIM_TUNE_PARAM, RIM_TUNE_CV_INPUT));

        float clap = clapVoice.process(rimClapClapSource(), args.sampleRate, clapRate) * clapLevel;
        float rim = rimVoice.process(rimClapRimSource(), args.sampleRate, rimRate) * rimLevel;
        clap = Ghost::driveWithAccent(
            clap, 0.f, clapLatchedChar, kRimClapAccent.driveAmt);
        rim = Ghost::driveWithAccent(
            rim, 0.f, rimLatchedChar, kRimClapAccent.driveAmt);
        clap *= clapLatchedGain * bus.masterVolume;
        rim  *= rimLatchedGain  * bus.masterVolume;

        outputs[CLAP_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(clap));
        outputs[RIM_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(rim));
    }
};

/// Panel widget for RimClap: binds knobs/jacks to the SVG by anchor name.
struct RimClapWidget : ModuleWidget, SvgHelper<RimClapWidget> {
    RimClapWidget(RimClap* module) {
        setModule(module);
        loadPanel(asset::plugin(pluginInstance, "res/RimClap.svg"));

        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        bindParam<RoundBlackKnob>("param.clap.tune",  RimClap::CLAP_TUNE_PARAM);
        bindParam<RoundBlackKnob>("param.clap.level", RimClap::CLAP_LEVEL_PARAM);
        bindParam<RoundBlackKnob>("param.rim.tune",   RimClap::RIM_TUNE_PARAM);
        bindParam<RoundBlackKnob>("param.rim.level",  RimClap::RIM_LEVEL_PARAM);

        bindInput<PJ301MPort>("cv.clap.tune",  RimClap::CLAP_TUNE_CV_INPUT);
        bindInput<PJ301MPort>("cv.clap.level", RimClap::CLAP_LEVEL_CV_INPUT);
        bindInput<PJ301MPort>("cv.rim.tune",   RimClap::RIM_TUNE_CV_INPUT);
        bindInput<PJ301MPort>("cv.rim.level",  RimClap::RIM_LEVEL_CV_INPUT);

        bindInput<PJ301MPort>("trig.clap.trig",   RimClap::CLAP_TRIG_INPUT);
        bindInput<PJ301MPort>("trig.rim.trig",    RimClap::RIM_TRIG_INPUT);
        bindInput<PJ301MPort>("accent.main.total", RimClap::TOTAL_ACC_INPUT);
        bindOutput<PJ301MPort>("out.clap.audio",  RimClap::CLAP_OUT_OUTPUT);
        bindOutput<PJ301MPort>("out.rim.audio",   RimClap::RIM_OUT_OUTPUT);
    }
};

rack::Model* modelRimClap = createModel<RimClap, RimClapWidget>("RimClap");
