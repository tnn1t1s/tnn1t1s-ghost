#include <rack.hpp>
#include "AgentModule.hpp"
#include "ghost/signal/Audio.hpp"
#include "CrashRideEngine.hpp"
#include "SvgHelper.hpp"

using namespace rack;
extern Plugin* pluginInstance;

/**
 * CrashRide -- 909-style cymbal pair (crash + ride) consolidated into one
 * module. Mirrors the RimClap pattern but with the richer surface that the
 * cymbal voices need: independent TUNE / DECAY / DRIVE / LEVEL per voice.
 *
 * Each voice is the same engine the standalone Crash and Ride modules use:
 * embedded clean 909 PCM -> playback-rate tune -> shortening VCA -> soft
 * drive -> output level. Tuning constants per voice (sample rate, tune span,
 * decay range, ROM config) are kept identical to the standalones so a patch
 * that swaps Crash/Ride for CrashRide sounds the same on each output.
 *
 * One shared accent input, per the established Ghost 909 family
 * convention. Per-voice trigger and audio output.
 *
 * Rack IDs (stable):
 *   Params:  CRASH_TUNE=0, CRASH_DECAY=1, CRASH_DRIVE=2, CRASH_LEVEL=3,
 *            RIDE_TUNE=4,  RIDE_DECAY=5,  RIDE_DRIVE=6,  RIDE_LEVEL=7
 *   Inputs:  CRASH_TRIG=0, RIDE_TRIG=1, ACCENT=2
 *   Outputs: CRASH_OUT=0, RIDE_OUT=1
 */

/// 909-style crash + ride cymbal pair in one module: two independent ROM-PCM
/// voices (tune / decay / drive / level each) sharing one accent input.
struct CrashRide : GhostModule {
    enum ParamId {
        CRASH_TUNE_PARAM, CRASH_DECAY_PARAM, CRASH_DRIVE_PARAM, CRASH_LEVEL_PARAM,
        RIDE_TUNE_PARAM,  RIDE_DECAY_PARAM,  RIDE_DRIVE_PARAM,  RIDE_LEVEL_PARAM,
        NUM_PARAMS
    };
    // Per the classic 909 voice layout, neither CY nor RD has Accent B; they share a
    // single TOTAL_ACC_INPUT (Accent A). Each voice latches the case gain
    // independently at its own trigger edge.
    enum InputId {
        CRASH_TRIG_INPUT,
        RIDE_TRIG_INPUT,
        TOTAL_ACC_INPUT,
        // Per-knob CV for the panel controls (TUNE/DECAY/LEVEL each voice).
        // DRIVE is an internal/right-click param, so it has no CV jack.
        CRASH_TUNE_CV_INPUT, CRASH_DECAY_CV_INPUT, CRASH_LEVEL_CV_INPUT,
        RIDE_TUNE_CV_INPUT,  RIDE_DECAY_CV_INPUT,  RIDE_LEVEL_CV_INPUT,
        NUM_INPUTS
    };
    enum OutputId {
        CRASH_OUT_OUTPUT,
        RIDE_OUT_OUTPUT,
        NUM_OUTPUTS
    };

    rack::dsp::SchmittTrigger crashTrigger;
    rack::dsp::SchmittTrigger rideTrigger;
    Ghost::RomVoice crashVoice;
    Ghost::RomVoice rideVoice;
    Ghost::AccentMix accentMix = Ghost::Accent::gentleMix();
    float crashLatchedGain = 1.f;
    float rideLatchedGain  = 1.f;
    float crashLatchedChar = 0.f;
    float rideLatchedChar  = 0.f;

    CrashRide() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(CRASH_TUNE_PARAM,  0.f, 1.f, 0.50f, "Crash tune",  "%", 0.f, 100.f);
        configParam(CRASH_DECAY_PARAM, 0.f, 1.f, 0.58f, "Crash decay", "%", 0.f, 100.f);
        configParam(CRASH_DRIVE_PARAM, 0.f, 1.f, 0.10f, "Crash drive", "%", 0.f, 100.f);
        configParam(CRASH_LEVEL_PARAM, 0.f, 1.f, 0.82f, "Crash level", "%", 0.f, 100.f);
        configParam(RIDE_TUNE_PARAM,   0.f, 1.f, 0.50f, "Ride tune",   "%", 0.f, 100.f);
        configParam(RIDE_DECAY_PARAM,  0.f, 1.f, 0.65f, "Ride decay",  "%", 0.f, 100.f);
        configParam(RIDE_DRIVE_PARAM,  0.f, 1.f, 0.10f, "Ride drive",  "%", 0.f, 100.f);
        configParam(RIDE_LEVEL_PARAM,  0.f, 1.f, 0.80f, "Ride level",  "%", 0.f, 100.f);
        configInput(CRASH_TRIG_INPUT, "Crash trigger");
        configInput(RIDE_TRIG_INPUT,  "Ride trigger");
        configInput(TOTAL_ACC_INPUT,  "Total accent (Accent A, sampled at TRIG; shared)");
        configInput(CRASH_TUNE_CV_INPUT,  "Crash tune CV");
        configInput(CRASH_DECAY_CV_INPUT, "Crash decay CV");
        configInput(CRASH_LEVEL_CV_INPUT, "Crash level CV");
        configInput(RIDE_TUNE_CV_INPUT,   "Ride tune CV");
        configInput(RIDE_DECAY_CV_INPUT,  "Ride decay CV");
        configInput(RIDE_LEVEL_CV_INPUT,  "Ride level CV");
        configOutput(CRASH_OUT_OUTPUT, "Crash audio");
        configOutput(RIDE_OUT_OUTPUT,  "Ride audio");
    }

    /// Return both voices to silence on Rack "Initialize" / first load (reset
    /// triggers + ROM voices, drop latched accent gains and chars).
    void onReset() override {
        crashTrigger.reset();
        rideTrigger.reset();
        crashVoice.sourcePos = rideVoice.sourcePos = 1e9f;
        crashVoice.env = rideVoice.env = 0.f;
        crashLatchedGain = rideLatchedGain = 1.f;
        crashLatchedChar = rideLatchedChar = 0.f;
    }

    /// Knob value plus CV/10 for that control, clamped to 0..1. Returns the
    /// effective normalized control value (DRIVE is off-panel, so no CV).
    float normWithCV(int paramId, int inputId) {
        return rack::math::clamp(
            params[paramId].getValue() + inputs[inputId].getVoltage() * 0.1f,
            0.f, 1.f);
    }

    /// Render one cymbal voice for this sample: rate-scaled ROM playback over
    /// [tuneNorm/decayNorm] mapped through [tuneOctaves, decayMin..decayMax sec],
    /// then post-ROM gain, accent-aware drive, and the LEVEL trim. Returns the
    /// voice sample in internal (pre-master) signal units.
    inline float voiceProcess(const ProcessArgs& args,
                              Ghost::RomVoice& voice,
                              const Ghost::RomAsset& asset,
                              float tuneNorm, float decayNorm,
                              float driveNorm, float levelNorm,
                              float charStrength, float accentDriveAmt,
                              float tuneOctaves, float decayMin, float decayMax,
                              const Ghost::RomVoiceConfig& baseCfg,
                              float postGain) {
        float playbackRate = std::pow(2.f, (tuneNorm - 0.5f) * 2.f * tuneOctaves);
        float decaySec = decayMin + decayNorm * (decayMax - decayMin);
        Ghost::RomVoiceConfig romCfg = baseCfg;
        float raw = voice.process(args, asset, playbackRate, decaySec, decayNorm, romCfg);
        float out = raw * postGain;
        out = Ghost::driveWithAccent(out, driveNorm, charStrength, accentDriveAmt);
        out *= levelNorm * 0.92f;
        return out;
    }

    /// Per-sample: latch the shared accent at each voice's trigger edge, read
    /// knob+CV controls, render both voices, then apply latched accent gain and
    /// master volume to the two outputs.
    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::resolveBus(this);
        if (crashTrigger.process(inputs[CRASH_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            crashVoice.trigger();
            auto acc = Ghost::sampleAccentAtTrig(
                this, TOTAL_ACC_INPUT, bus, accentMix);
            crashLatchedChar = acc.charStrength;
            crashLatchedGain = acc.gain;
        }
        if (rideTrigger.process(inputs[RIDE_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            rideVoice.trigger();
            auto acc = Ghost::sampleAccentAtTrig(
                this, TOTAL_ACC_INPUT, bus, accentMix);
            rideLatchedChar = acc.charStrength;
            rideLatchedGain = acc.gain;
        }

        // TUNE/DECAY/LEVEL take per-knob CV; DRIVE is internal/right-click (no CV).
        const float crashTune  = normWithCV(CRASH_TUNE_PARAM,  CRASH_TUNE_CV_INPUT);
        const float crashDecay = normWithCV(CRASH_DECAY_PARAM, CRASH_DECAY_CV_INPUT);
        const float crashDrive = rack::math::clamp(params[CRASH_DRIVE_PARAM].getValue(), 0.f, 1.f);
        const float crashLevel = normWithCV(CRASH_LEVEL_PARAM, CRASH_LEVEL_CV_INPUT);

        const float rideTune  = normWithCV(RIDE_TUNE_PARAM,  RIDE_TUNE_CV_INPUT);
        const float rideDecay = normWithCV(RIDE_DECAY_PARAM, RIDE_DECAY_CV_INPUT);
        const float rideDrive = rack::math::clamp(params[RIDE_DRIVE_PARAM].getValue(), 0.f, 1.f);
        const float rideLevel = normWithCV(RIDE_LEVEL_PARAM, RIDE_LEVEL_CV_INPUT);

        namespace cri = crashride_impl;
        float crashOut = voiceProcess(args, crashVoice, cri::crashAsset(),
                                      crashTune, crashDecay, crashDrive, crashLevel,
                                      crashLatchedChar, cri::kCrashAccent.driveAmt,
                                      cri::kCrashTuneOctaves, cri::kCrashDecayMinSec, cri::kCrashDecayMaxSec,
                                      cri::kCrashRomCfg, 1.04f);
        float rideOut  = voiceProcess(args, rideVoice, cri::rideAsset(),
                                      rideTune, rideDecay, rideDrive, rideLevel,
                                      rideLatchedChar, cri::kRideAccent.driveAmt,
                                      cri::kRideTuneOctaves, cri::kRideDecayMinSec, cri::kRideDecayMaxSec,
                                      cri::kRideRomCfg, 1.02f);

        crashOut *= crashLatchedGain * bus.masterVolume;
        rideOut  *= rideLatchedGain  * bus.masterVolume;
        outputs[CRASH_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(crashOut));
        outputs[RIDE_OUT_OUTPUT] .setVoltage(Ghost::Signal::Audio::toRackVolts(rideOut));
    }
};

/// Right-click context-menu slider bound directly to a module param (used to
/// expose the off-panel DRIVE controls).
struct CrashRideParamSlider : ui::Slider {
    CrashRideParamSlider(engine::Module* m, int paramId, float widthPx = 200.f) {
        quantity = m->paramQuantities[paramId];
        box.size.x = widthPx;
    }
};

/// Panel widget for CrashRide: SVG-driven knob/jack layout plus a context menu
/// exposing the two off-panel DRIVE params.
struct CrashRideWidget : ModuleWidget, SvgHelper<CrashRideWidget> {
    CrashRideWidget(CrashRide* module) {
        setModule(module);
        loadPanel(asset::plugin(pluginInstance, "res/CrashRide.svg"));

        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        bindParam<RoundBlackKnob>("param.crash.tune",  CrashRide::CRASH_TUNE_PARAM);
        bindParam<RoundBlackKnob>("param.crash.decay", CrashRide::CRASH_DECAY_PARAM);
        bindParam<RoundBlackKnob>("param.crash.level", CrashRide::CRASH_LEVEL_PARAM);
        bindParam<RoundBlackKnob>("param.ride.tune",   CrashRide::RIDE_TUNE_PARAM);
        bindParam<RoundBlackKnob>("param.ride.decay",  CrashRide::RIDE_DECAY_PARAM);
        bindParam<RoundBlackKnob>("param.ride.level",  CrashRide::RIDE_LEVEL_PARAM);

        bindInput<PJ301MPort>("cv.crash.tune",  CrashRide::CRASH_TUNE_CV_INPUT);
        bindInput<PJ301MPort>("cv.crash.decay", CrashRide::CRASH_DECAY_CV_INPUT);
        bindInput<PJ301MPort>("cv.crash.level", CrashRide::CRASH_LEVEL_CV_INPUT);
        bindInput<PJ301MPort>("cv.ride.tune",   CrashRide::RIDE_TUNE_CV_INPUT);
        bindInput<PJ301MPort>("cv.ride.decay",  CrashRide::RIDE_DECAY_CV_INPUT);
        bindInput<PJ301MPort>("cv.ride.level",  CrashRide::RIDE_LEVEL_CV_INPUT);

        bindInput<PJ301MPort>("trig.crash.trig",   CrashRide::CRASH_TRIG_INPUT);
        bindInput<PJ301MPort>("trig.ride.trig",    CrashRide::RIDE_TRIG_INPUT);
        bindInput<PJ301MPort>("accent.main.total", CrashRide::TOTAL_ACC_INPUT);
        bindOutput<PJ301MPort>("out.crash.audio",  CrashRide::CRASH_OUT_OUTPUT);
        bindOutput<PJ301MPort>("out.ride.audio",   CrashRide::RIDE_OUT_OUTPUT);
    }

    void appendContextMenu(Menu* menu) override {
        if (!module) return;
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Drive (internal saturation)"));
        menu->addChild(new CrashRideParamSlider(module, CrashRide::CRASH_DRIVE_PARAM));
        menu->addChild(new CrashRideParamSlider(module, CrashRide::RIDE_DRIVE_PARAM));
    }
};

rack::Model* modelCrashRide = createModel<CrashRide, CrashRideWidget>("CrashRide");
