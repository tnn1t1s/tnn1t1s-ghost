#include <rack.hpp>
#include "CrashRideEngine.hpp"
#include "GhostBus.hpp"
#include "GhostVoice.hpp"
#include "GhostPanel.hpp"
#include "PanelLayout.hpp"
#include "ghost/signal/Audio.hpp"

using namespace rack;
extern Plugin* pluginInstance;


// ---------------------------------------------------------------------------
// CrashRideLab -- expanded expert / performance variant.
//
// Adds per-voice ROM bit depth to the combined cymbal module while preserving
// the same tune / decay / drive / level surface as the main module.
// ---------------------------------------------------------------------------

/// Expert/bench variant of CrashRide that adds a per-voice ROM bit-depth knob
/// on top of the same tune / decay / drive / level surface (unregistered Lab).
struct CrashRideLab : GhostModule {
    enum ParamId {
        CRASH_TUNE_PARAM, CRASH_DECAY_PARAM, CRASH_DRIVE_PARAM, CRASH_LEVEL_PARAM, CRASH_BITS_PARAM,
        RIDE_TUNE_PARAM,  RIDE_DECAY_PARAM,  RIDE_DRIVE_PARAM,  RIDE_LEVEL_PARAM,  RIDE_BITS_PARAM,
        NUM_PARAMS
    };
    enum InputId {
        CRASH_TRIG_INPUT, RIDE_TRIG_INPUT, TOTAL_ACC_INPUT, NUM_INPUTS
    };
    enum OutputId {
        CRASH_OUT_OUTPUT, RIDE_OUT_OUTPUT, NUM_OUTPUTS
    };

    rack::dsp::SchmittTrigger crashTrigger;
    rack::dsp::SchmittTrigger rideTrigger;
    Ghost::RomVoice crashVoice;
    Ghost::RomVoice rideVoice;
    Ghost::AccentMix accentMix = Ghost::Accent::gentleMix();
    float crashLatchedGain = 1.f;
    float rideLatchedGain = 1.f;
    float crashLatchedChar = 0.f;
    float rideLatchedChar = 0.f;

    CrashRideLab() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(CRASH_TUNE_PARAM,  0.f, 1.f, 0.50f, "Crash tune", "%", 0.f, 100.f);
        configParam(CRASH_DECAY_PARAM, 0.f, 1.f, 0.58f, "Crash decay", "%", 0.f, 100.f);
        configParam(CRASH_DRIVE_PARAM, 0.f, 1.f, 0.10f, "Crash drive", "%", 0.f, 100.f);
        configParam(CRASH_LEVEL_PARAM, 0.f, 1.f, 0.82f, "Crash level", "%", 0.f, 100.f);
        configParam(CRASH_BITS_PARAM,  1.f, 16.f, 16.f, "Crash bit depth");
        configParam(RIDE_TUNE_PARAM,   0.f, 1.f, 0.50f, "Ride tune", "%", 0.f, 100.f);
        configParam(RIDE_DECAY_PARAM,  0.f, 1.f, 0.65f, "Ride decay", "%", 0.f, 100.f);
        configParam(RIDE_DRIVE_PARAM,  0.f, 1.f, 0.10f, "Ride drive", "%", 0.f, 100.f);
        configParam(RIDE_LEVEL_PARAM,  0.f, 1.f, 0.80f, "Ride level", "%", 0.f, 100.f);
        configParam(RIDE_BITS_PARAM,   1.f, 16.f, 16.f, "Ride bit depth");
        configInput(CRASH_TRIG_INPUT, "Crash trigger");
        configInput(RIDE_TRIG_INPUT,  "Ride trigger");
        configInput(TOTAL_ACC_INPUT,  "Total accent (Accent A, sampled at TRIG; shared)");
        configOutput(CRASH_OUT_OUTPUT, "Crash audio");
        configOutput(RIDE_OUT_OUTPUT,  "Ride audio");
    }

    /// Render one cymbal voice for this sample, as in CrashRide but with an
    /// explicit per-voice bitDepth (1..16) overriding the base config. Returns
    /// the voice sample in internal (pre-master) signal units.
    inline float voiceProcess(const ProcessArgs& args,
                              Ghost::RomVoice& voice,
                              const Ghost::RomAsset& asset,
                              float tuneNorm, float decayNorm, float driveNorm, float levelNorm,
                              int bitDepth,
                              float charStrength, float accentDriveAmt,
                              float tuneOctaves, float decayMin, float decayMax,
                              const Ghost::RomVoiceConfig& baseCfg,
                              float postGain) {
        // Shared sampled-cymbal path used by both voices: rate-scaled ROM
        // playback, shortening envelope, optional bit reduction, then the same
        // post-ROM drive/level trim as the standalone voice modules.
        float playbackRate = std::pow(2.f, (tuneNorm - 0.5f) * 2.f * tuneOctaves);
        float decaySec = decayMin + decayNorm * (decayMax - decayMin);
        Ghost::RomVoiceConfig romCfg = baseCfg;
        romCfg.bitDepth = bitDepth;
        float raw = voice.process(args, asset, playbackRate, decaySec, decayNorm, romCfg);
        float out = raw * postGain;
        out = Ghost::driveWithAccent(out, driveNorm, charStrength, accentDriveAmt);
        out *= levelNorm * 0.92f;
        return out;
    }

    /// Per-sample render of both Lab voices, identical to CrashRide::process
    /// but driving each voice's bit-depth knob into voiceProcess.
    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::resolveBus(this);
        if (crashTrigger.process(inputs[CRASH_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            crashVoice.trigger();
            auto acc = Ghost::sampleAccentAtTrig(this, TOTAL_ACC_INPUT, bus, accentMix);
            crashLatchedChar = acc.charStrength;
            crashLatchedGain = acc.gain;
        }
        if (rideTrigger.process(inputs[RIDE_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            rideVoice.trigger();
            auto acc = Ghost::sampleAccentAtTrig(this, TOTAL_ACC_INPUT, bus, accentMix);
            rideLatchedChar = acc.charStrength;
            rideLatchedGain = acc.gain;
        }

        namespace cri = crashride_impl;
        float crashOut = voiceProcess(
            args, crashVoice, cri::crashAsset(),
            rack::math::clamp(params[CRASH_TUNE_PARAM].getValue(), 0.f, 1.f),
            rack::math::clamp(params[CRASH_DECAY_PARAM].getValue(), 0.f, 1.f),
            rack::math::clamp(params[CRASH_DRIVE_PARAM].getValue(), 0.f, 1.f),
            rack::math::clamp(params[CRASH_LEVEL_PARAM].getValue(), 0.f, 1.f),
            int(std::round(params[CRASH_BITS_PARAM].getValue())),
            crashLatchedChar, cri::kCrashAccent.driveAmt,
            cri::kCrashTuneOctaves, cri::kCrashDecayMinSec, cri::kCrashDecayMaxSec,
            cri::kCrashRomCfg, 1.04f);
        float rideOut = voiceProcess(
            args, rideVoice, cri::rideAsset(),
            rack::math::clamp(params[RIDE_TUNE_PARAM].getValue(), 0.f, 1.f),
            rack::math::clamp(params[RIDE_DECAY_PARAM].getValue(), 0.f, 1.f),
            rack::math::clamp(params[RIDE_DRIVE_PARAM].getValue(), 0.f, 1.f),
            rack::math::clamp(params[RIDE_LEVEL_PARAM].getValue(), 0.f, 1.f),
            int(std::round(params[RIDE_BITS_PARAM].getValue())),
            rideLatchedChar, cri::kRideAccent.driveAmt,
            cri::kRideTuneOctaves, cri::kRideDecayMinSec, cri::kRideDecayMaxSec,
            cri::kRideRomCfg, 1.02f);

        crashOut *= crashLatchedGain * bus.masterVolume;
        rideOut *= rideLatchedGain * bus.masterVolume;
        outputs[CRASH_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(crashOut));
        outputs[RIDE_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(rideOut));
    }
};

/// One text label on the Lab panel, positioned in millimetres.
struct CrashRideLabLabelCell { float xMm, yMm; const char* text; };

/// Procedurally drawn Lab panel: shared "Lab shell" art plus the collected
/// per-control text labels.
struct CrashRideLabPanel : rack::widget::Widget {
    std::vector<CrashRideLabLabelCell> labels;

    void draw(const DrawArgs& args) override {
        Ghost::LabArt::drawLabShell(
            args.vg, box.size, "CRSHRIDE LAB",
            "curated 18HP expert surface");

        nvgFontSize(args.vg, 4.2f);
        nvgFillColor(args.vg, nvgRGBA(200, 200, 215, 170));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (const auto& label : labels) {
            nvgText(args.vg, mm2px(label.xMm), mm2px(label.yMm), label.text, nullptr);
        }
    }
};

/// Panel widget for CrashRideLab: lays out the mirrored crash/ride knob grid
/// (with BITS), trigger/accent inputs, and per-voice outputs on the 18HP panel.
struct CrashRideLabWidget : rack::ModuleWidget {
    /// Add a centered small knob for paramId at (xMm, yMm) and register its
    /// text label just above it on the panel.
    void addLabeledKnob(rack::engine::Module* module, int paramId,
                        float xMm, float yMm, const char* label, CrashRideLabPanel* panel) {
        addParam(createParamCentered<rack::RoundSmallBlackKnob>(
            mm2px(Vec(xMm, yMm)), module, paramId));
        panel->labels.push_back({xMm, yMm - 6.f, label});
    }

    CrashRideLabWidget(CrashRideLab* module) {
        setModule(module);
        auto* panel = new CrashRideLabPanel;
        panel->box.size = AgentLayout::panelSize_18HP();
        addChild(panel);
        box.size = panel->box.size;
        AgentLayout::addScrews_18HP(this);

        const float crashX = 22.f;
        const float rideX = 69.f;
        const float ys[5] = {22.f, 40.f, 58.f, 76.f, 94.f};
        // Same mirrored "voice A / voice B" Lab pattern as ChhOhhLab so the
        // sampled 909 pair modules read as one family in the browser and rack.
        const char* labels[5] = {"TUNE", "DECAY", "DRIVE", "LEVEL", "BITS"};
        const int crashParams[5] = {
            CrashRideLab::CRASH_TUNE_PARAM, CrashRideLab::CRASH_DECAY_PARAM, CrashRideLab::CRASH_DRIVE_PARAM,
            CrashRideLab::CRASH_LEVEL_PARAM, CrashRideLab::CRASH_BITS_PARAM
        };
        const int rideParams[5] = {
            CrashRideLab::RIDE_TUNE_PARAM, CrashRideLab::RIDE_DECAY_PARAM, CrashRideLab::RIDE_DRIVE_PARAM,
            CrashRideLab::RIDE_LEVEL_PARAM, CrashRideLab::RIDE_BITS_PARAM
        };
        panel->labels.push_back({crashX, 12.f, "CRASH"});
        panel->labels.push_back({rideX, 12.f, "RIDE"});
        for (int i = 0; i < 5; ++i) {
            addLabeledKnob(module, crashParams[i], crashX, ys[i], labels[i], panel);
            addLabeledKnob(module, rideParams[i], rideX, ys[i], labels[i], panel);
        }

        panel->labels.push_back({16.f, 112.f, "CR TRIG"});
        panel->labels.push_back({41.f, 112.f, "TACC"});
        panel->labels.push_back({66.f, 112.f, "RD TRIG"});
        addInput(createInputCentered<rack::PJ301MPort>(mm2px(Vec(16.f, 120.f)), module, CrashRideLab::CRASH_TRIG_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(mm2px(Vec(41.f, 120.f)), module, CrashRideLab::TOTAL_ACC_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(mm2px(Vec(66.f, 120.f)), module, CrashRideLab::RIDE_TRIG_INPUT));
        panel->labels.push_back({24.f, 120.f, "CR OUT"});
        panel->labels.push_back({58.f, 120.f, "RD OUT"});
        addOutput(createOutputCentered<rack::PJ301MPort>(mm2px(Vec(24.f, 126.f)), module, CrashRideLab::CRASH_OUT_OUTPUT));
        addOutput(createOutputCentered<rack::PJ301MPort>(mm2px(Vec(58.f, 126.f)), module, CrashRideLab::RIDE_OUT_OUTPUT));
    }
};

rack::Model* modelCrashRideLab = createModel<CrashRideLab, CrashRideLabWidget>("CrashRideLab");
