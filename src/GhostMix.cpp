// GHOST MIX — dedicated 909-kit summing mixer.
//
// Ten labeled inputs, one per voice (KICK, RIM, CLAP, TOM LO/MID/HI, CHH, OHH,
// CRASH, RIDE), each with a mute switch, summed to a single MIX output. It is a
// deliberately dumb unity summer: per-voice level lives on the GHOST voices
// themselves, so the mixer just sums whatever is unmuted. Keeps the whole kit
// in the box on one master mix point instead of a third-party mixer.
//
// Panel is panelkit-generated (specs/panels/GhostMix.panel.yaml); live widgets
// bind by name to the anchor shapes in res/GhostMix.svg.
#include <rack.hpp>
#include <string>
#include "SvgHelper.hpp"

using namespace rack;

extern Plugin* pluginInstance;

struct GhostMix : Module {
    static const int N = 10;
    enum ParamId  { MUTE_PARAM, NUM_PARAMS = MUTE_PARAM + N };
    enum InputId  { IN_INPUT,   NUM_INPUTS = IN_INPUT + N };
    enum OutputId { MIX_OUTPUT, NUM_OUTPUTS };

    GhostMix() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        static const char* names[N] = {
            "Kick", "Rim", "Clap", "Tom low", "Tom mid", "Tom high",
            "Closed hat", "Open hat", "Crash", "Ride"};
        for (int i = 0; i < N; i++) {
            configSwitch(MUTE_PARAM + i, 0.f, 1.f, 0.f,
                         std::string(names[i]) + " mute", {"On", "Muted"});
            configInput(IN_INPUT + i, names[i]);
        }
        configOutput(MIX_OUTPUT, "Mix");
    }

    void process(const ProcessArgs& args) override {
        float sum = 0.f;
        for (int i = 0; i < N; i++)
            if (params[MUTE_PARAM + i].getValue() < 0.5f)   // 0 = on, 1 = muted
                sum += inputs[IN_INPUT + i].getVoltage();
        // NaN guard + ±10 V safety clamp: a unity sum of 10 voices can run hot,
        // and one NaN input must not propagate to the DAC.
        if (!std::isfinite(sum)) sum = 0.f;
        outputs[MIX_OUTPUT].setVoltage(rack::math::clamp(sum, -10.f, 10.f));
    }
};

struct GhostMixWidget : ModuleWidget, SvgHelper<GhostMixWidget> {
    GhostMixWidget(GhostMix* module) {
        setModule(module);
        loadPanel(asset::plugin(pluginInstance, "res/GhostMix.svg"));

        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        static const char* in_ids[GhostMix::N] = {
            "in.kick", "in.rim", "in.clap", "in.tomlo", "in.tommid",
            "in.tomhi", "in.chh", "in.ohh", "in.crash", "in.ride"};
        static const char* mute_ids[GhostMix::N] = {
            "mute.kick", "mute.rim", "mute.clap", "mute.tomlo", "mute.tommid",
            "mute.tomhi", "mute.chh", "mute.ohh", "mute.crash", "mute.ride"};
        for (int i = 0; i < GhostMix::N; i++) {
            bindInput<PJ301MPort>(in_ids[i], GhostMix::IN_INPUT + i);
            bindParam<CKSS>(mute_ids[i], GhostMix::MUTE_PARAM + i);
        }
        bindOutput<PJ301MPort>("out.main.mix", GhostMix::MIX_OUTPUT);
    }
};

Model* modelGhostMix = createModel<GhostMix, GhostMixWidget>("GhostMix");
