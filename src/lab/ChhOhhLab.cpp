#include <rack.hpp>
#include "ChhOhhEngine.hpp"
#include "GhostBus.hpp"
#include "GhostVoice.hpp"
#include "GhostPanel.hpp"
#include "PanelLayout.hpp"
#include "ghost/signal/Audio.hpp"

using namespace rack;
extern Plugin* pluginInstance;


// ---------------------------------------------------------------------------
// ChhOhhLab -- expanded expert / performance variant.
//
// Keeps the combined 909 hi-hat architecture intact, but exposes per-voice ROM
// bit depth so experts can push the sampled-hat side into crunchier territory
// without changing the main production module.
// ---------------------------------------------------------------------------

/// One text label placed on the Lab panel at a millimetre position.
struct ChhOhhLabLabelCell { float xMm, yMm; const char* text; };

/// Expert/performance variant of ChhOhh: same combined-hat engine plus a
/// per-voice ROM bit-depth control, exposed on an 18 HP panel.
struct ChhOhhLab : GhostModule {
    enum ParamId {
        CHH_TUNE_PARAM, CHH_DECAY_PARAM, CHH_DRIVE_PARAM, CHH_LEVEL_PARAM, CHH_BITS_PARAM,
        OHH_TUNE_PARAM, OHH_DECAY_PARAM, OHH_DRIVE_PARAM, OHH_LEVEL_PARAM, OHH_BITS_PARAM,
        NUM_PARAMS
    };
    enum InputId {
        CHH_TRIG_INPUT, OHH_TRIG_INPUT,
        LOCAL_ACC_INPUT, TOTAL_ACC_INPUT,
        NUM_INPUTS
    };
    enum OutputId {
        CHH_OUT_OUTPUT, OHH_OUT_OUTPUT,
        NUM_OUTPUTS
    };

    // Same voice as the production module -- one playback system with a
    // CLOSED/OPEN control line. The Lab adds BITS on top of it rather than
    // keeping its own copy of the DSP.
    ChhOhhVoice voice;
    Ghost::AccentMix accentMix = Ghost::Accent::gentleMix();
    float latchedGain = 1.f;

    ChhOhhLab() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(CHH_TUNE_PARAM,  0.f, 1.f, 0.50f, "Closed tune", "%", 0.f, 100.f);
        configParam(CHH_DECAY_PARAM, 0.f, 1.f, 0.22f, "Closed decay", "%", 0.f, 100.f);
        configParam(CHH_DRIVE_PARAM, 0.f, 1.f, 0.10f, "Closed drive", "%", 0.f, 100.f);
        configParam(CHH_LEVEL_PARAM, 0.f, 1.f, 0.84f, "Closed level", "%", 0.f, 100.f);
        configParam(CHH_BITS_PARAM,  1.f, 16.f, 16.f, "Closed bit depth");
        configParam(OHH_TUNE_PARAM,  0.f, 1.f, 0.50f, "Open tune", "%", 0.f, 100.f);
        configParam(OHH_DECAY_PARAM, 0.f, 1.f, 0.58f, "Open decay", "%", 0.f, 100.f);
        configParam(OHH_DRIVE_PARAM, 0.f, 1.f, 0.12f, "Open drive", "%", 0.f, 100.f);
        configParam(OHH_LEVEL_PARAM, 0.f, 1.f, 0.82f, "Open level", "%", 0.f, 100.f);
        configParam(OHH_BITS_PARAM,  1.f, 16.f, 16.f, "Open bit depth");
        configInput(CHH_TRIG_INPUT,  "Closed trigger");
        configInput(OHH_TRIG_INPUT,  "Open trigger");
        configInput(LOCAL_ACC_INPUT, "Local accent (Accent B; CH only)");
        configInput(TOTAL_ACC_INPUT, "Total accent (Accent A; shared)");
        configOutput(CHH_OUT_OUTPUT, "Closed audio");
        configOutput(OHH_OUT_OUTPUT, "Open audio");
    }

    /// Per-sample engine: the production voice, with the extra BITS control
    /// feeding bitReduce on whichever path the control line has selected.
    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::resolveBus(this);

        const bool chTrig = voice.chhTrigger.process(
            inputs[CHH_TRIG_INPUT].getVoltage(), 0.1f, 2.f);
        const bool ohTrig = voice.ohhTrigger.process(
            inputs[OHH_TRIG_INPUT].getVoltage(), 0.1f, 2.f);
        if (chTrig || ohTrig) {
            const bool openMode = ohTrig;
            auto acc = openMode
                ? Ghost::sampleAccentAtTrig(this, TOTAL_ACC_INPUT, bus, accentMix)
                : Ghost::sampleAccentAtTrig(this, TOTAL_ACC_INPUT, bus, accentMix,
                                            LOCAL_ACC_INPUT);
            latchedGain = acc.gain;
            voice.fire(openMode, acc.charStrength);
        }

        float chhTune = rack::math::clamp(params[CHH_TUNE_PARAM].getValue(), 0.f, 1.f);
        float chhDecay = rack::math::clamp(params[CHH_DECAY_PARAM].getValue(), 0.f, 1.f);
        float chhDrive = rack::math::clamp(params[CHH_DRIVE_PARAM].getValue(), 0.f, 1.f);
        float chhLevel = rack::math::clamp(params[CHH_LEVEL_PARAM].getValue(), 0.f, 1.f);
        // Bit depth is the one extra Lab control here because the sampled
        // cymbal/hat family responded well to "how much ROM grit?" in testing.
        // The production module keeps the cleaner 16-bit path.
        int chhBits = int(std::round(params[CHH_BITS_PARAM].getValue()));
        float ohhTune = rack::math::clamp(params[OHH_TUNE_PARAM].getValue(), 0.f, 1.f);
        float ohhDecay = rack::math::clamp(params[OHH_DECAY_PARAM].getValue(), 0.f, 1.f);
        float ohhDrive = rack::math::clamp(params[OHH_DRIVE_PARAM].getValue(), 0.f, 1.f);
        float ohhLevel = rack::math::clamp(params[OHH_LEVEL_PARAM].getValue(), 0.f, 1.f);
        int ohhBits = int(std::round(params[OHH_BITS_PARAM].getValue()));

        float chhOut, ohhOut;
        voice.process(args,
                      chhTune, chhDecay, chhDrive, chhLevel,
                      ohhTune, ohhDecay, ohhDrive, ohhLevel,
                      chhOut, ohhOut);
        // BITS still reads per-path off the panel, applied to the live jack.
        chhOut = Ghost::bitReduce(chhOut, chhBits);
        ohhOut = Ghost::bitReduce(ohhOut, ohhBits);
        chhOut *= latchedGain * bus.masterVolume;
        ohhOut *= latchedGain * bus.masterVolume;

        outputs[CHH_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(chhOut));
        outputs[OHH_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(ohhOut));
    }
};

/// Background panel for the Lab variant: draws the shell plus all knob/port
/// text labels.
struct ChhOhhLabPanel : rack::widget::Widget {
    std::vector<ChhOhhLabLabelCell> labels;

    /// Render the Lab shell and every queued label.
    void draw(const DrawArgs& args) override {
        Ghost::LabArt::drawLabShell(
            args.vg, box.size, "OHCH LAB",
            "curated 18HP expert surface",
            nvgRGB(8, 8, 10),
            nvgRGBA(220, 235, 240, 220),
            nvgRGBA(200, 220, 230, 170));

        nvgFontSize(args.vg, 4.2f);
        nvgFillColor(args.vg, nvgRGBA(200, 220, 230, 170));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (const auto& label : labels) {
            nvgText(args.vg, mm2px(label.xMm), mm2px(label.yMm), label.text, nullptr);
        }
    }
};

/// Panel widget for the Lab variant: lays out the mirrored CLOSED/OPEN knob
/// columns (including BITS) plus trigger/accent/output jacks.
struct ChhOhhLabWidget : rack::ModuleWidget {
    /// Add a centred small knob and queue its text label above it.
    void addLabeledKnob(rack::engine::Module* module, int paramId,
                        float xMm, float yMm, const char* label, ChhOhhLabPanel* panel) {
        addParam(createParamCentered<rack::RoundSmallBlackKnob>(
            mm2px(Vec(xMm, yMm)), module, paramId));
        panel->labels.push_back({xMm, yMm - 6.f, label});
    }

    ChhOhhLabWidget(ChhOhhLab* module) {
        setModule(module);
        auto* panel = new ChhOhhLabPanel;
        panel->box.size = AgentLayout::panelSize_18HP();
        addChild(panel);
        box.size = panel->box.size;

        AgentLayout::addScrews_18HP(this);

        const float chx = 22.f;
        const float ohx = 69.f;
        const float ys[5] = {22.f, 40.f, 58.f, 76.f, 94.f};
        // Mirrored columns keep the combined-hat Lab readable on stage:
        // production controls plus one extra BITS row per voice, nothing else.
        const char* labels[5] = {"TUNE", "DECAY", "DRIVE", "LEVEL", "BITS"};
        const int chParams[5] = {
            ChhOhhLab::CHH_TUNE_PARAM, ChhOhhLab::CHH_DECAY_PARAM, ChhOhhLab::CHH_DRIVE_PARAM,
            ChhOhhLab::CHH_LEVEL_PARAM, ChhOhhLab::CHH_BITS_PARAM
        };
        const int ohParams[5] = {
            ChhOhhLab::OHH_TUNE_PARAM, ChhOhhLab::OHH_DECAY_PARAM, ChhOhhLab::OHH_DRIVE_PARAM,
            ChhOhhLab::OHH_LEVEL_PARAM, ChhOhhLab::OHH_BITS_PARAM
        };
        panel->labels.push_back({chx, 12.f, "CLOSED"});
        panel->labels.push_back({ohx, 12.f, "OPEN"});
        for (int i = 0; i < 5; ++i) {
            addLabeledKnob(module, chParams[i], chx, ys[i], labels[i], panel);
            addLabeledKnob(module, ohParams[i], ohx, ys[i], labels[i], panel);
        }

        panel->labels.push_back({16.f, 112.f, "CH TRIG"});
        panel->labels.push_back({34.f, 112.f, "LACC"});
        panel->labels.push_back({52.f, 112.f, "TACC"});
        panel->labels.push_back({70.f, 112.f, "OH TRIG"});
        panel->labels.push_back({88.f, 112.f, "OUTS"});
        addInput(createInputCentered<rack::PJ301MPort>(mm2px(Vec(16.f, 120.f)), module, ChhOhhLab::CHH_TRIG_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(mm2px(Vec(34.f, 120.f)), module, ChhOhhLab::LOCAL_ACC_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(mm2px(Vec(52.f, 120.f)), module, ChhOhhLab::TOTAL_ACC_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(mm2px(Vec(70.f, 120.f)), module, ChhOhhLab::OHH_TRIG_INPUT));
        addOutput(createOutputCentered<rack::PJ301MPort>(mm2px(Vec(84.f, 120.f)), module, ChhOhhLab::CHH_OUT_OUTPUT));
        addOutput(createOutputCentered<rack::PJ301MPort>(mm2px(Vec(94.f, 120.f)), module, ChhOhhLab::OHH_OUT_OUTPUT));
    }
};

rack::Model* modelChhOhhLab = createModel<ChhOhhLab, ChhOhhLabWidget>("ChhOhhLab");
