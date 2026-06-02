#include <rack.hpp>
#include "AgentModule.hpp"
#include "PanelLayout.hpp"
#include "GhostPanel.hpp"
#include "GhostVoice.hpp"
#include "GhostBus.hpp"
#include "ghost/signal/Audio.hpp"
#include "embedded/GhostClapData.hpp"
#include "embedded/GhostRimData.hpp"
#include "SvgHelper.hpp"

using namespace rack;
extern Plugin* pluginInstance;

namespace {
// Per the 909 reference doc, RS and CP have NO accent on the original 909
// (sit at full max). The drive boost below is a stylistic Ghost
// addition, not a circuit reproduction. AccentCharacter member order:
// body, pitch, click, drive, noise, snap, decay, brightness, bend.
static const Ghost::AccentCharacter RIMCLAP_ACCENT = Ghost::Accent::driveOnly();
static const std::vector<float>& rimClapClapSource() {
    static const std::vector<float> sample =
        Ghost::decodeEmbeddedF32(ghostClap_f32, ghostClap_f32_len);
    return sample;
}

static const std::vector<float>& rimClapRimSource() {
    static const std::vector<float> sample =
        Ghost::decodeEmbeddedF32(ghostRim_f32, ghostRim_f32_len);
    return sample;
}

struct RomVoice {
    float pos = 1e9f;

    void trigger() {
        pos = 0.f;
    }

    float process(const std::vector<float>& sample, float sampleRate,
                  float playbackRate = 1.f, int bitDepth = 16) {
        if (pos >= float(sample.size() - 1)) {
            return 0.f;
        }
        float out = Ghost::sampleAt(sample, pos);
        pos += Ghost::playbackStep(Ghost::kEmbeddedPcmSampleRate, sampleRate, playbackRate);
        return Ghost::bitReduce(out, bitDepth);
    }
};
}

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
    // Per Roland TR-909 OM, neither RS nor CP has Accent B; they share a
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

    // knob + CV/10, clamped to the knob's normalized range.
    float normWithCV(int paramId, int inputId) {
        return rack::math::clamp(
            params[paramId].getValue() + inputs[inputId].getVoltage() * 0.1f,
            0.f, 1.f);
    }
    // TUNE 0..1 -> playback-rate multiplier, one octave each way (matches Lab).
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
            clap, 0.f, clapLatchedChar, RIMCLAP_ACCENT.driveAmt);
        rim = Ghost::driveWithAccent(
            rim, 0.f, rimLatchedChar, RIMCLAP_ACCENT.driveAmt);
        clap *= clapLatchedGain * bus.masterVolume;
        rim  *= rimLatchedGain  * bus.masterVolume;

        outputs[CLAP_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(clap));
        outputs[RIM_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(rim));
    }
};

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


// ---------------------------------------------------------------------------
// RimClapLab -- expanded expert / performance variant.
//
// The main module is intentionally minimal. The Lab variant adds per-voice ROM
// tune and bit depth, which are real processing hooks on the sampled sources
// without inventing analog controls that the current engine does not have.
// ---------------------------------------------------------------------------

struct RimClapLab : GhostModule {
    enum ParamId {
        CLAP_TUNE_PARAM, CLAP_BITS_PARAM, CLAP_LEVEL_PARAM,
        RIM_TUNE_PARAM,  RIM_BITS_PARAM,  RIM_LEVEL_PARAM,
        NUM_PARAMS
    };
    enum InputId {
        CLAP_TRIG_INPUT, RIM_TRIG_INPUT, TOTAL_ACC_INPUT, NUM_INPUTS
    };
    enum OutputId {
        CLAP_OUT_OUTPUT, RIM_OUT_OUTPUT, NUM_OUTPUTS
    };

    dsp::SchmittTrigger clapTrigger;
    dsp::SchmittTrigger rimTrigger;
    RomVoice clapVoice;
    RomVoice rimVoice;
    // Un-accented = normal (0 dB); accent adds a gentle ~+3 dB on top (909-style).
    Ghost::AccentMix accentMix = Ghost::Accent::gentleMix();
    float clapLatchedGain = 1.f;
    float rimLatchedGain = 1.f;
    float clapLatchedChar = 0.f;
    float rimLatchedChar = 0.f;

    RimClapLab() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(CLAP_TUNE_PARAM,  0.f, 1.f, 0.50f, "Clap tune", "%", 0.f, 100.f);
        configParam(CLAP_BITS_PARAM,  1.f, 16.f, 16.f, "Clap bit depth");
        configParam(CLAP_LEVEL_PARAM, 0.f, 1.f, 0.90f, "Clap level", "%", 0.f, 100.f);
        configParam(RIM_TUNE_PARAM,   0.f, 1.f, 0.50f, "Rim tune", "%", 0.f, 100.f);
        configParam(RIM_BITS_PARAM,   1.f, 16.f, 16.f, "Rim bit depth");
        configParam(RIM_LEVEL_PARAM,  0.f, 1.f, 0.90f, "Rim level", "%", 0.f, 100.f);
        configInput(CLAP_TRIG_INPUT, "Clap trigger");
        configInput(RIM_TRIG_INPUT,  "Rim trigger");
        configInput(TOTAL_ACC_INPUT, "Total accent (Accent A, sampled at TRIG; shared)");
        configOutput(CLAP_OUT_OUTPUT, "Clap audio");
        configOutput(RIM_OUT_OUTPUT,  "Rim audio");
    }

    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::resolveBus(this);
        if (clapTrigger.process(inputs[CLAP_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            clapVoice.trigger();
            auto acc = Ghost::sampleAccentAtTrig(this, TOTAL_ACC_INPUT, bus, accentMix);
            clapLatchedChar = acc.charStrength;
            clapLatchedGain = acc.gain;
        }
        if (rimTrigger.process(inputs[RIM_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            rimVoice.trigger();
            auto acc = Ghost::sampleAccentAtTrig(this, TOTAL_ACC_INPUT, bus, accentMix);
            rimLatchedChar = acc.charStrength;
            rimLatchedGain = acc.gain;
        }

        float clapRate = std::pow(2.f, (params[CLAP_TUNE_PARAM].getValue() - 0.5f) * 2.f * 1.0f);
        int clapBits = int(std::round(params[CLAP_BITS_PARAM].getValue()));
        float clapLevel = params[CLAP_LEVEL_PARAM].getValue();
        float rimRate = std::pow(2.f, (params[RIM_TUNE_PARAM].getValue() - 0.5f) * 2.f * 1.0f);
        int rimBits = int(std::round(params[RIM_BITS_PARAM].getValue()));
        float rimLevel = params[RIM_LEVEL_PARAM].getValue();

        float clap = clapVoice.process(rimClapClapSource(), args.sampleRate, clapRate, clapBits) * clapLevel;
        float rim = rimVoice.process(rimClapRimSource(), args.sampleRate, rimRate, rimBits) * rimLevel;
        clap = Ghost::driveWithAccent(
            clap, 0.f, clapLatchedChar, RIMCLAP_ACCENT.driveAmt);
        rim = Ghost::driveWithAccent(
            rim, 0.f, rimLatchedChar, RIMCLAP_ACCENT.driveAmt);
        clap *= clapLatchedGain * bus.masterVolume;
        rim *= rimLatchedGain * bus.masterVolume;

        outputs[CLAP_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(clap));
        outputs[RIM_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(rim));
    }
};

struct RimClapLabLabelCell { float xMm, yMm; const char* text; };

struct RimClapLabPanel : rack::widget::Widget {
    std::vector<RimClapLabLabelCell> labels;

    void draw(const DrawArgs& args) override {
        Ghost::LabArt::drawLabShell(
            args.vg, box.size, "RIMCLAP LAB",
            "curated 18HP expert surface",
            nvgRGB(10, 8, 10),
            nvgRGBA(255, 215, 155, 220),
            nvgRGBA(255, 215, 155, 170));

        nvgFontSize(args.vg, 4.2f);
        nvgFillColor(args.vg, nvgRGBA(255, 215, 155, 170));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (const auto& label : labels) {
            nvgText(args.vg, mm2px(label.xMm), mm2px(label.yMm), label.text, nullptr);
        }
    }
};

struct RimClapLabWidget : rack::ModuleWidget {
    void addLabeledKnob(rack::engine::Module* module, int paramId,
                        float xMm, float yMm, const char* label, RimClapLabPanel* panel) {
        addParam(createParamCentered<rack::RoundSmallBlackKnob>(
            mm2px(Vec(xMm, yMm)), module, paramId));
        panel->labels.push_back({xMm, yMm - 6.f, label});
    }

    RimClapLabWidget(RimClapLab* module) {
        setModule(module);
        auto* panel = new RimClapLabPanel;
        panel->box.size = AgentLayout::panelSize_18HP();
        addChild(panel);
        box.size = panel->box.size;
        AgentLayout::addScrews_18HP(this);

        const float clapX = 22.f;
        const float rimX = 69.f;
        const float ys[3] = {28.f, 50.f, 72.f};
        panel->labels.push_back({clapX, 14.f, "CLAP"});
        panel->labels.push_back({rimX, 14.f, "RIM"});
        addLabeledKnob(module, RimClapLab::CLAP_TUNE_PARAM, clapX, ys[0], "TUNE", panel);
        addLabeledKnob(module, RimClapLab::CLAP_BITS_PARAM, clapX, ys[1], "BITS", panel);
        addLabeledKnob(module, RimClapLab::CLAP_LEVEL_PARAM, clapX, ys[2], "LEVEL", panel);
        addLabeledKnob(module, RimClapLab::RIM_TUNE_PARAM, rimX, ys[0], "TUNE", panel);
        addLabeledKnob(module, RimClapLab::RIM_BITS_PARAM, rimX, ys[1], "BITS", panel);
        addLabeledKnob(module, RimClapLab::RIM_LEVEL_PARAM, rimX, ys[2], "LEVEL", panel);

        panel->labels.push_back({16.f, 98.f, "CLAP TRIG"});
        panel->labels.push_back({41.f, 98.f, "TACC"});
        panel->labels.push_back({66.f, 98.f, "RIM TRIG"});
        addInput(createInputCentered<rack::PJ301MPort>(mm2px(Vec(16.f, 106.f)), module, RimClapLab::CLAP_TRIG_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(mm2px(Vec(41.f, 106.f)), module, RimClapLab::TOTAL_ACC_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(mm2px(Vec(66.f, 106.f)), module, RimClapLab::RIM_TRIG_INPUT));
        panel->labels.push_back({24.f, 120.f, "CLAP OUT"});
        panel->labels.push_back({58.f, 120.f, "RIM OUT"});
        addOutput(createOutputCentered<rack::PJ301MPort>(mm2px(Vec(24.f, 126.f)), module, RimClapLab::CLAP_OUT_OUTPUT));
        addOutput(createOutputCentered<rack::PJ301MPort>(mm2px(Vec(58.f, 126.f)), module, RimClapLab::RIM_OUT_OUTPUT));
    }
};

rack::Model* modelRimClapLab = createModel<RimClapLab, RimClapLabWidget>("RimClapLab");
