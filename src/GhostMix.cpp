// GHOST MIX — dedicated 909-kit summing mixer.
//
// Twelve labeled inputs: one per voice of the full 909 kit (KICK, SNARE, RIM,
// CLAP, TOM LO/MID/HI, CHH, OHH, CRASH, RIDE) plus an AUX channel for an
// external signal or a chained voice. Each has a mute switch, summed to a single
// MIX output. It is a deliberately dumb unity summer: per-voice level lives on
// the GHOST voices themselves, so the mixer just sums whatever is unmuted. Keeps
// the whole kit in the box on one master mix point instead of a third-party mixer.
//
// Panel is panelkit-generated (specs/panels/GhostMix.panel.yaml); live widgets
// bind by name to the anchor shapes in res/GhostMix.svg.
#include <rack.hpp>
#include <string>
#include "SvgHelper.hpp"

using namespace rack;

extern Plugin* pluginInstance;

/// Twelve-input unity summing mixer for the full 909 kit (one input per voice)
/// plus an aux channel.
struct GhostMix : Module {
    static const int kNumChannels = 12;
    enum ParamId  { MUTE_PARAM, NUM_PARAMS = MUTE_PARAM + kNumChannels };
    enum InputId  { IN_INPUT,   NUM_INPUTS = IN_INPUT + kNumChannels };
    enum OutputId { MIX_OUTPUT, NUM_OUTPUTS };

    GhostMix() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        static const char* names[kNumChannels] = {
            "Kick", "Snare", "Rim", "Clap", "Tom low", "Tom mid", "Tom high",
            "Closed hat", "Open hat", "Crash", "Ride", "Aux"};
        for (int i = 0; i < kNumChannels; i++) {
            configSwitch(MUTE_PARAM + i, 0.f, 1.f, 0.f,
                         std::string(names[i]) + " mute", {"On", "Muted"});
            configInput(IN_INPUT + i, names[i]);
        }
        configOutput(MIX_OUTPUT, "Mix");
    }

    /// Sum unmuted inputs into the mix output, with NaN guard and ±10V clamp.
    void process(const ProcessArgs& args) override {
        float sum = 0.f;
        for (int i = 0; i < kNumChannels; i++)
            if (params[MUTE_PARAM + i].getValue() < 0.5f)   // 0 = on, 1 = muted
                sum += inputs[IN_INPUT + i].getVoltage();
        // NaN guard + ±10 V safety clamp: a unity sum of 10 voices can run hot,
        // and one NaN input must not propagate to the DAC.
        if (!std::isfinite(sum)) sum = 0.f;
        outputs[MIX_OUTPUT].setVoltage(rack::math::clamp(sum, -10.f, 10.f));
    }
};

/// Panel widget: binds the ten input jacks, mute switches and mix output.
struct GhostMixWidget : ModuleWidget, SvgHelper<GhostMixWidget> {
    GhostMixWidget(GhostMix* module) {
        setModule(module);
        loadPanel(asset::plugin(pluginInstance, "res/GhostMix.svg"));

        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        static const char* in_ids[GhostMix::kNumChannels] = {
            "in.kick", "in.snare", "in.rim", "in.clap", "in.tomlo", "in.tommid",
            "in.tomhi", "in.chh", "in.ohh", "in.crash", "in.ride", "in.aux"};
        static const char* mute_ids[GhostMix::kNumChannels] = {
            "mute.kick", "mute.snare", "mute.rim", "mute.clap", "mute.tomlo", "mute.tommid",
            "mute.tomhi", "mute.chh", "mute.ohh", "mute.crash", "mute.ride", "mute.aux"};
        for (int i = 0; i < GhostMix::kNumChannels; i++) {
            bindInput<PJ301MPort>(in_ids[i], GhostMix::IN_INPUT + i);
            bindParam<CKSS>(mute_ids[i], GhostMix::MUTE_PARAM + i);
        }
        bindOutput<PJ301MPort>("out.main.mix", GhostMix::MIX_OUTPUT);
    }
};

Model* modelGhostMix = createModel<GhostMix, GhostMixWidget>("GhostMix");
