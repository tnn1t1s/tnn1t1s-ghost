#include <rack.hpp>
#include "AgentModule.hpp"
#include "PanelLayout.hpp"
#include "GhostBus.hpp"
#include "ghost/signal/Audio.hpp"
#include "SnrEngine.hpp"
#include "SvgHelper.hpp"

using namespace rack;
extern Plugin* pluginInstance;

/**
 * Snr -- 909-style snare drum.
 *
 * Production voice module. The shared snare engine (SnrFit::Config voicing
 * recipe, the SnrSVF filter, and the snrTriangle / snrNormWithCV helpers)
 * lives in SnrEngine.hpp, shared with the SnrLab fitting bench
 * (lab/SnrLab.cpp). See SnrEngine.hpp for the internal model and the stable
 * Rack param/input/output ID list.
 */


/// Production 909-style snare voice. Front-panel surface is Tune/Tone/Snappy/
/// Level (plus CV and accent inputs); all voicing is fixed from SnrFit defaults.
struct Snr : GhostModule {

    enum ParamId  {
        TUNE_PARAM, TONE_PARAM, SNAPPY_PARAM, LEVEL_PARAM,
        NUM_PARAMS
    };
    enum InputId  {
        TRIG_INPUT, TUNE_CV_INPUT, TONE_CV_INPUT, SNAPPY_CV_INPUT, LEVEL_CV_INPUT,
        LOCAL_ACC_INPUT, TOTAL_ACC_INPUT,
        NUM_INPUTS
    };
    enum OutputId { OUT_OUTPUT, NUM_OUTPUTS };

    // Per-sample synthesis lives in SnrVoice (SnrEngine.hpp), shared with the
    // headless stress harness; the module owns the Rack-side plumbing only.
    SnrVoice voice;
    // Per-instance fit config: each Snr owns its own copy so multiple
    // instances never share or race a file-scope static.
    SnrFit::Config fit = SnrFit::defaults();
    // Default to a neutral mix until Snr accent is ear-tuned; keeps the
    // existing 909-snare voicing audibly unchanged when no controller is
    // wired or accent gates fire.
    Ghost::AccentMix accentMix = Ghost::Accent::gentleMix();
    float latchedCaseGain = 1.f;

    /// Configure the four params, CV/accent inputs, and audio output.
    Snr() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(TUNE_PARAM,   0.f, 1.f, 0.50f, "Tune",   "%", 0.f, 100.f);
        configParam(TONE_PARAM,   0.f, 1.f, 1.00f, "Tone",   "%", 0.f, 100.f);
        configParam(SNAPPY_PARAM, 0.f, 1.f, 1.00f, "Snappy", "%", 0.f, 100.f);
        configParam(LEVEL_PARAM,  0.f, 1.f, 0.82f, "Level",  "%", 0.f, 100.f);
        configInput (TRIG_INPUT,        "Trigger");
        configInput (TUNE_CV_INPUT,     "Tune CV");
        configInput (TONE_CV_INPUT,     "Tone CV");
        configInput (SNAPPY_CV_INPUT,   "Snappy CV");
        configInput (LEVEL_CV_INPUT,    "Level CV");
        configInput (LOCAL_ACC_INPUT,   "Local accent (Accent B, sampled at TRIG)");
        configInput (TOTAL_ACC_INPUT,   "Total accent (Accent A, sampled at TRIG)");
        configOutput(OUT_OUTPUT,        "Audio");
    }

    /// Return the snare to silence and a clean state on Rack "Initialize" /
    /// first load (zero envelopes, phases, filters, and accent latches).
    void onReset() override {
        voice.trigger.reset();
        voice.phase1 = voice.phase2 = 0.f;
        voice.bodyEnv1 = voice.bodyEnv2 = 0.f;
        voice.noiseLowEnv = voice.noiseHighEnv = 0.f;
        voice.bendEnv = 0.f;
        voice.attackEnv = 1.f;
        voice.clickEnv = 0.f;
        voice.bodyLP = 0.f;
        voice.lpNoise.reset();
        voice.hpNoise.reset();
        latchedCaseGain = 1.f;
        voice.latchedCharStrength = 0.f;
    }

    /// Synthesize one sample: on a rising TRIG, latch accent and reset all
    /// envelopes/phases; then sum the two-triangle body, low/high noise
    /// branches, and transient click into the soft-clipped, level-scaled output.
    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::resolveBus(this);
        if (voice.trigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            auto acc = Ghost::sampleAccentAtTrig(
                this, TOTAL_ACC_INPUT, bus, accentMix, LOCAL_ACC_INPUT);
            latchedCaseGain = acc.gain;
            voice.fire(acc.charStrength);
        }

        float tune_norm    = snrNormWithCV(*this, TUNE_PARAM,   TUNE_CV_INPUT);
        float tone_norm    = snrNormWithCV(*this, TONE_PARAM,   TONE_CV_INPUT);
        float snap_norm    = snrNormWithCV(*this, SNAPPY_PARAM, SNAPPY_CV_INPUT);
        float level_norm   = snrNormWithCV(*this, LEVEL_PARAM,  LEVEL_CV_INPUT);

        float out = voice.process(args, fit, tune_norm, tone_norm, snap_norm, level_norm);
        out *= latchedCaseGain * bus.masterVolume;
        outputs[OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(out));
    }
};


// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------

/// Background panel for the production Snr: asset image plus the four control
/// labels and the two accent-input labels.
struct SnrPanel : rack::widget::Widget {
    void draw(const DrawArgs& args) override {
        AgentLayout::drawAssetPanel(
            args.vg, box.size, pluginInstance,
            "res/Snr-bg.jpg",
            nvgRGB(30, 16, 18),
            "SNR", nvgRGB(255, 120, 100));

        static const char* const LABELS[] = {
            "TUNE", "TONE", "SNAP", "LEVEL",
        };
        const float* ys = AgentLayout::kRowY8;
        nvgFontSize(args.vg, 5.5f);
        nvgFillColor(args.vg, nvgRGBA(255, 190, 180, 180));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (int i = 0; i < 4; i++) {
            nvgText(args.vg, mm2px(AgentLayout::kCenter12Hp), mm2px(ys[i]),
                    LABELS[i], NULL);
        }
        // Accent input row labels (column-anchored, like the IO row).
        nvgFontSize(args.vg, 4.5f);
        nvgFillColor(args.vg, nvgRGBA(255, 190, 180, 160));
        nvgText(args.vg, mm2px(AgentLayout::kLeftColumn12Hp),
                mm2px(ys[5] - 6.f), "LACC", NULL);
        nvgText(args.vg, mm2px(AgentLayout::kRightColumn12Hp),
                mm2px(ys[5] - 6.f), "TACC", NULL);
    }
};


// ---------------------------------------------------------------------------
// Widget -- 12HP, 8-row grid
// ---------------------------------------------------------------------------

/// Production Snr module widget: 12HP, 8-row grid wiring knobs/ports to the SVG.
struct SnrWidget : ModuleWidget, SvgHelper<SnrWidget> {
    SnrWidget(Snr* module) {
        setModule(module);
        loadPanel(asset::plugin(pluginInstance, "res/Snr.svg"));

        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        bindParam<RoundBlackKnob>("param.main.tune",   Snr::TUNE_PARAM);
        bindParam<RoundBlackKnob>("param.main.tone",   Snr::TONE_PARAM);
        bindParam<RoundBlackKnob>("param.main.snappy", Snr::SNAPPY_PARAM);
        bindParam<RoundBlackKnob>("param.main.level",  Snr::LEVEL_PARAM);

        bindInput<PJ301MPort>("cv.main.tune",   Snr::TUNE_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.tone",   Snr::TONE_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.snappy", Snr::SNAPPY_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.level",  Snr::LEVEL_CV_INPUT);

        bindInput<PJ301MPort>("trig.main.trig",    Snr::TRIG_INPUT);
        bindInput<PJ301MPort>("accent.main.local", Snr::LOCAL_ACC_INPUT);
        bindInput<PJ301MPort>("accent.main.total", Snr::TOTAL_ACC_INPUT);
        bindOutput<PJ301MPort>("out.main.audio",   Snr::OUT_OUTPUT);
    }
};


rack::Model* modelSnr = createModel<Snr, SnrWidget>("Snr");
