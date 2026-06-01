#include <rack.hpp>
#include "AgentModule.hpp"
#include "PanelLayout.hpp"
#include "NineOhNinePanel.hpp"
#include "Tr909Bus.hpp"
#include "SvgHelper.hpp"

using namespace rack;
extern Plugin* pluginInstance;

/**
 * Tr909Ctrl -- TR-909 global state controller.
 *
 * Sits next to the 909 voice kit and broadcasts slow-changing global
 * controls via the expander bus.
 *
 * Knobs:
 *   - DEFAULT  -- 0..1 attenuator on the no-accent (ghost) case. At 1.0,
 *                 ghost notes hit at the per-voice ghostDb level. At 0,
 *                 ghost notes are silenced entirely. Useful as a global
 *                 "default volume" trim while the kit is being tuned.
 *   - ACCENT A -- 0..1 attenuator on the A rail's contribution. Scales
 *                 in any case where A fires (alone or with B).
 *   - ACCENT B -- 0..1 attenuator on the B rail's contribution. Scales
 *                 in any case where B fires.
 *   - MASTER   -- post-everything output volume scalar.
 *
 * Each knob has a CV input. Tr909Ctrl is NOT in the trigger path;
 * per-step gates (Total Accent, Local Accent, voice triggers) travel
 * from the sequencer directly to each voice's cable inputs.
 */

struct Tr909Ctrl : Tr909Module {
    // DEFAULT (no-accent / ghost level) is fixed at full internally -- it was a
    // dull control, so the panel exposes only the expressive trims.
    enum ParamId  {
        ACCENT_A_PARAM, ACCENT_B_PARAM, MASTER_VOL_PARAM, RANGE_PARAM,
        NUM_PARAMS
    };

    // RANGE positions -> ghost-floor scale broadcast on the bus. Against the
    // kick's -16.48 dB (15%) floor: Tight ~35%, Classic 15% (1.0=native), Wide ~8%.
    static constexpr float kRangeScale[3] = {0.55f, 1.0f, 1.33f};  // Tight/Classic/Wide
    enum InputId  {
        ACCENT_A_CV_INPUT, ACCENT_B_CV_INPUT, MASTER_VOL_CV_INPUT,
        NUM_INPUTS
    };
    enum OutputId { NUM_OUTPUTS };

    Tr909Ctrl() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(ACCENT_A_PARAM,   0.f, 1.f, 0.5f, "Accent A amount", "%", 0.f, 100.f);
        configParam(ACCENT_B_PARAM,   0.f, 1.f, 0.5f, "Accent B amount", "%", 0.f, 100.f);
        configParam(MASTER_VOL_PARAM, 0.f, 1.f, 0.5f, "Master volume",   "%", 0.f, 100.f);
        configSwitch(RANGE_PARAM, 0.f, 2.f, 1.f, "Dynamic range",
                     {"Tight", "Classic", "Wide"});
        configInput(ACCENT_A_CV_INPUT,   "Accent A amount CV");
        configInput(ACCENT_B_CV_INPUT,   "Accent B amount CV");
        configInput(MASTER_VOL_CV_INPUT, "Master volume CV");
    }

    static inline float knobPlusCV(rack::Module& self, int paramId, int cvInputId) {
        float v = self.params[paramId].getValue()
                + self.inputs[cvInputId].getNormalVoltage(0.f) * 0.1f;
        return rack::math::clamp(v, 0.f, 1.f);
    }

    void process(const ProcessArgs& args) override {
        currentBus.ghostAmount       = 1.f;  // fixed full ghost level (no panel control)
        currentBus.accentAAmount     = knobPlusCV(*this, ACCENT_A_PARAM,   ACCENT_A_CV_INPUT);
        currentBus.accentBAmount     = knobPlusCV(*this, ACCENT_B_PARAM,   ACCENT_B_CV_INPUT);
        currentBus.masterVolume      = knobPlusCV(*this, MASTER_VOL_PARAM, MASTER_VOL_CV_INPUT);
        int range = (int) std::lround(rack::math::clamp(params[RANGE_PARAM].getValue(), 0.f, 2.f));
        currentBus.ghostScale        = kRangeScale[range];
        currentBus.controllerPresent = true;
    }
};


// ---------------------------------------------------------------------------
// Panel -- 8 HP, solid black, 4 vertical knob+CV pairs
// ---------------------------------------------------------------------------

struct Tr909CtrlWidget : ModuleWidget, SvgHelper<Tr909CtrlWidget> {
    Tr909CtrlWidget(Tr909Ctrl* module) {
        setModule(module);
        loadPanel(asset::plugin(pluginInstance, "res/Tr909Ctrl.svg"));

        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        bindParam<RoundBlackKnob>("param.main.accent-a", Tr909Ctrl::ACCENT_A_PARAM);
        bindParam<RoundBlackKnob>("param.main.accent-b", Tr909Ctrl::ACCENT_B_PARAM);
        bindParam<RoundBlackKnob>("param.main.master",   Tr909Ctrl::MASTER_VOL_PARAM);

        bindInput<PJ301MPort>("cv.main.accent-a", Tr909Ctrl::ACCENT_A_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.accent-b", Tr909Ctrl::ACCENT_B_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.master",   Tr909Ctrl::MASTER_VOL_CV_INPUT);

        bindParam<CKSSThreeHorizontal>("param.main.range", Tr909Ctrl::RANGE_PARAM);
    }
};

constexpr float Tr909Ctrl::kRangeScale[3];

rack::Model* modelTr909Ctrl = createModel<Tr909Ctrl, Tr909CtrlWidget>("Tr909Ctrl");
