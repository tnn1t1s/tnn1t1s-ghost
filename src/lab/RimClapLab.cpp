#include <rack.hpp>
#include "RimClapEngine.hpp"
#include "GhostBus.hpp"
#include "GhostVoice.hpp"
#include "GhostPanel.hpp"
#include "PanelLayout.hpp"
#include "ghost/signal/Audio.hpp"

using namespace rack;
extern Plugin* pluginInstance;


// ---------------------------------------------------------------------------
// RimClapLab -- expanded expert / performance variant.
//
// The main module is intentionally minimal. The Lab variant adds per-voice ROM
// tune and bit depth, which are real processing hooks on the sampled sources
// without inventing analog controls that the current engine does not have.
// ---------------------------------------------------------------------------

/// Expert/performance variant of RimClap: adds per-voice ROM bit-depth control
/// alongside TUNE/LEVEL. Unregistered bench surface, not shipped in the browser.
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

    /// Per-sample DSP: like RimClap, plus per-voice bit-depth reduction driven by
    /// the BITS knobs.
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
            clap, 0.f, clapLatchedChar, kRimClapAccent.driveAmt);
        rim = Ghost::driveWithAccent(
            rim, 0.f, rimLatchedChar, kRimClapAccent.driveAmt);
        clap *= clapLatchedGain * bus.masterVolume;
        rim *= rimLatchedGain * bus.masterVolume;

        outputs[CLAP_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(clap));
        outputs[RIM_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(rim));
    }
};

/// One text label drawn on the Lab panel, positioned in millimetres.
struct RimClapLabLabelCell { float xMm, yMm; const char* text; };

/// Hand-drawn panel for the Lab variant: renders the lab shell plus its labels.
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

/// Panel widget for the Lab variant: places knobs/jacks at explicit mm
/// coordinates and registers their labels on the panel.
struct RimClapLabWidget : rack::ModuleWidget {
    /// Add a small knob for `paramId` at (`xMm`,`yMm`) mm and register its label
    /// just above it on `panel`.
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
