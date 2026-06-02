// Attenuate — reference pass-through attenuator: OUT = IN * SCALE (+ CV).
// Validates the panelkit -> SvgHelper binding: live widgets are bound by name to
// the anchor shapes in res/Attenuate.svg. Screws are real VCV widgets placed at
// the rack-grid corners (the SVG carries no painted screws).
#include <rack.hpp>
#include "SvgHelper.hpp"

using namespace rack;

extern Plugin* pluginInstance;

/// Reference pass-through attenuator: OUT = IN * SCALE (knob + CV).
struct Attenuate : Module {
    enum ParamId  { SCALE_PARAM, NUM_PARAMS };
    enum InputId  { IN_INPUT, SCALE_CV_INPUT, NUM_INPUTS };
    enum OutputId { OUT_OUTPUT, NUM_OUTPUTS };

    Attenuate() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(SCALE_PARAM, 0.f, 1.f, 1.f, "Scale", "%", 0.f, 100.f);
        configInput(SCALE_CV_INPUT, "Scale CV");
        configInput(IN_INPUT, "Signal");
        configOutput(OUT_OUTPUT, "Signal");
    }

    /// Scale the input by the knob plus CV (0.1 V/unit), clamped to [0, 1].
    void process(const ProcessArgs& args) override {
        float scale = params[SCALE_PARAM].getValue()
                      + inputs[SCALE_CV_INPUT].getVoltage() * 0.1f;
        scale = clamp(scale, 0.f, 1.f);
        outputs[OUT_OUTPUT].setVoltage(inputs[IN_INPUT].getVoltage() * scale);
    }
};

/// Panel widget: binds the scale knob, scale CV, signal in and signal out.
struct AttenuateWidget : ModuleWidget, SvgHelper<AttenuateWidget> {
    AttenuateWidget(Attenuate* module) {
        setModule(module);
        loadPanel(asset::plugin(pluginInstance, "res/Attenuate.svg"));

        // real VCV screws at the rack-grid corners (aligned to the rails)
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        bindParam<RoundBlackKnob>("param.main.scale", Attenuate::SCALE_PARAM);
        bindInput<PJ301MPort>("cv.main.scale", Attenuate::SCALE_CV_INPUT);
        bindInput<PJ301MPort>("in.main.signal", Attenuate::IN_INPUT);
        bindOutput<PJ301MPort>("out.main.signal", Attenuate::OUT_OUTPUT);
    }
};

Model* modelAttenuate = createModel<Attenuate, AttenuateWidget>("Attenuate");
