#include <rack.hpp>
#include "AgentModule.hpp"
#include "PanelLayout.hpp"
#include "NineOhNinePanel.hpp"
#include "TR909VoiceCommon.hpp"
#include "Tr909Bus.hpp"
#include "ghost/signal/Audio.hpp"
#include "embedded/Chh909Data.hpp"
#include "embedded/Ohh909Data.hpp"
#include <cmath>

using namespace rack;
extern Plugin* pluginInstance;

/**
 * ChhOhh -- TR-909 closed + open hi-hat in a single module.
 *
 * The original 909 closed and open hi-hat share a single envelope/sound
 * circuit: a CH hit instantly mutes any sounding OH. We model that by
 * keeping both voices in the same module, where the choke is just
 * internal state (no cross-module bus signaling needed). This also
 * mirrors the CrashRide / RimClap pattern of grouping voices that
 * share a hardware path.
 *
 * Accent rails per Roland TR-909 OM:
 *   - CH has Accent B: responds to LOCAL_ACC and TOTAL_ACC
 *   - OH has only Accent A: responds to TOTAL_ACC
 *
 * Choke (issue #78): when the CH trigger fires, OH switches to a fast
 * release (~5ms tau) which decays its envelope to silence within ~25ms.
 * A subsequent OH trigger re-arms the voice and cancels any pending
 * choke state.
 */

namespace {

static constexpr float CHH_TUNE_OCTAVES  = 1.0f;
static constexpr float CHH_DECAY_MIN_SEC = 0.010f;
static constexpr float CHH_DECAY_MAX_SEC = 0.16f;

static constexpr float OHH_TUNE_OCTAVES  = 1.0f;
static constexpr float OHH_DECAY_MIN_SEC = 0.006f;
static constexpr float OHH_DECAY_MAX_SEC = 3.20f;

static constexpr float CHOKE_DECAY_SEC   = 0.005f;

// Per-DSP-stage accent character. CH and OH both have a small drive boost
// on accented hits; per the 909 reference doc, CH has level-only accent on
// the original 909 and OH has no accent (sits at full max). These are
// stylistic Ghost additions, not circuit reproductions.
// AccentCharacter member order: body, pitch, click, drive, noise, snap,
// decay, brightness, bend.
static const Ghost::TR909::AccentCharacter CHH_ACCENT =
    { 0.f, 0.f, 0.f, 0.08f, 0.f, 0.f, 0.f, 0.f, 0.f };
static const Ghost::TR909::AccentCharacter OHH_ACCENT =
    { 0.f, 0.f, 0.f, 0.08f, 0.f, 0.f, 0.f, 0.f, 0.f };

static const std::vector<float>& chhSource() {
    static const std::vector<float> sample =
        Ghost::TR909::decodeEmbeddedF32(chh909_f32, chh909_f32_len);
    return sample;
}

static const std::vector<float>& ohhSource() {
    static const std::vector<float> sample =
        Ghost::TR909::decodeEmbeddedF32(ohh909_f32, ohh909_f32_len);
    return sample;
}

} // namespace

struct ChhOhh : Tr909Module {
    enum ParamId {
        CHH_TUNE_PARAM,  CHH_DECAY_PARAM,  CHH_DRIVE_PARAM,  CHH_LEVEL_PARAM,
        OHH_TUNE_PARAM,  OHH_DECAY_PARAM,  OHH_DRIVE_PARAM,  OHH_LEVEL_PARAM,
        NUM_PARAMS
    };
    enum InputId {
        CHH_TRIG_INPUT, OHH_TRIG_INPUT,
        LOCAL_ACC_INPUT,   // CH only (Accent B); Roland: OH has no Accent B
        TOTAL_ACC_INPUT,   // shared by both voices (Accent A)
        NUM_INPUTS
    };
    enum OutputId {
        CHH_OUT_OUTPUT, OHH_OUT_OUTPUT,
        NUM_OUTPUTS
    };

    dsp::SchmittTrigger chhTrigger;
    dsp::SchmittTrigger ohhTrigger;

    // CH voice state.
    float chhSamplePos = 1e9f;
    float chhEnv       = 0.f;

    // OH voice state.
    float ohhSamplePos = 1e9f;
    float ohhEnv       = 0.f;
    bool  ohhChokeActive = false;

    Ghost::TR909::AccentMix accentMix = Ghost::TR909::neutralMix();
    float chhLatchedGain = 1.f;
    float ohhLatchedGain = 1.f;
    float chhLatchedChar = 0.f;
    float ohhLatchedChar = 0.f;

    int dbgBitDepth = 16;

    ChhOhh() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(CHH_TUNE_PARAM,  0.f, 1.f, 0.50f, "Closed tune",  "%", 0.f, 100.f);
        configParam(CHH_DECAY_PARAM, 0.f, 1.f, 0.22f, "Closed decay", "%", 0.f, 100.f);
        configParam(CHH_DRIVE_PARAM, 0.f, 1.f, 0.10f, "Closed drive", "%", 0.f, 100.f);
        configParam(CHH_LEVEL_PARAM, 0.f, 1.f, 0.84f, "Closed level", "%", 0.f, 100.f);
        configParam(OHH_TUNE_PARAM,  0.f, 1.f, 0.50f, "Open tune",    "%", 0.f, 100.f);
        configParam(OHH_DECAY_PARAM, 0.f, 1.f, 0.58f, "Open decay",   "%", 0.f, 100.f);
        configParam(OHH_DRIVE_PARAM, 0.f, 1.f, 0.12f, "Open drive",   "%", 0.f, 100.f);
        configParam(OHH_LEVEL_PARAM, 0.f, 1.f, 0.82f, "Open level",   "%", 0.f, 100.f);
        configInput(CHH_TRIG_INPUT,  "Closed trigger");
        configInput(OHH_TRIG_INPUT,  "Open trigger");
        configInput(LOCAL_ACC_INPUT, "Local accent (Accent B; CH only)");
        configInput(TOTAL_ACC_INPUT, "Total accent (Accent A; shared)");
        configOutput(CHH_OUT_OUTPUT, "Closed audio");
        configOutput(OHH_OUT_OUTPUT, "Open audio");
    }

    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::TR909::resolveBus(this);

        // -- Closed hi-hat trigger -------------------------------------
        if (chhTrigger.process(inputs[CHH_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            chhSamplePos = 0.f;
            chhEnv       = 1.f;
            auto acc = Ghost::TR909::sampleAccentAtTrig(
                this, TOTAL_ACC_INPUT, bus, accentMix, LOCAL_ACC_INPUT);
            chhLatchedChar = acc.charStrength;
            chhLatchedGain = acc.gain;
            // CH→OH choke: any sounding open hat is muted by a CH hit.
            if (ohhEnv > 1e-4f) ohhChokeActive = true;
        }

        // -- Open hi-hat trigger ---------------------------------------
        if (ohhTrigger.process(inputs[OHH_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            ohhSamplePos     = 0.f;
            ohhEnv           = 1.f;
            ohhChokeActive   = false;  // a fresh OH hit cancels pending choke
            // OH has no Accent B; pass localInputId=-1 by default arg.
            auto acc = Ghost::TR909::sampleAccentAtTrig(
                this, TOTAL_ACC_INPUT, bus, accentMix);
            ohhLatchedChar = acc.charStrength;
            ohhLatchedGain = acc.gain;
        }

        // -- Read knobs once per frame for both voices ----------------
        // The combined module doesn't expose per-knob CV (panel space),
        // so we read params directly. Per-voice CV is a follow-up if needed.
        float chhTune  = rack::math::clamp(params[CHH_TUNE_PARAM].getValue(),  0.f, 1.f);
        float chhDecay = rack::math::clamp(params[CHH_DECAY_PARAM].getValue(), 0.f, 1.f);
        float chhDrive = rack::math::clamp(params[CHH_DRIVE_PARAM].getValue(), 0.f, 1.f);
        float chhLevel = rack::math::clamp(params[CHH_LEVEL_PARAM].getValue(), 0.f, 1.f);
        float ohhTune  = rack::math::clamp(params[OHH_TUNE_PARAM].getValue(),  0.f, 1.f);
        float ohhDecay = rack::math::clamp(params[OHH_DECAY_PARAM].getValue(), 0.f, 1.f);
        float ohhDrive = rack::math::clamp(params[OHH_DRIVE_PARAM].getValue(), 0.f, 1.f);
        float ohhLevel = rack::math::clamp(params[OHH_LEVEL_PARAM].getValue(), 0.f, 1.f);

        // -- CH DSP ----------------------------------------------------
        float chhRate    = std::pow(2.f, (chhTune - 0.5f) * 2.f * CHH_TUNE_OCTAVES);
        float chhDecayShape = std::sqrt(chhDecay);
        float chhDecaySec = CHH_DECAY_MIN_SEC
                          + chhDecayShape * (CHH_DECAY_MAX_SEC - CHH_DECAY_MIN_SEC);
        const auto& chSrc = chhSource();
        float chhSrc = Ghost::TR909::sampleAt(chSrc, chhSamplePos);
        chhSamplePos += Ghost::TR909::playbackStep(
            Ghost::TR909::kEmbeddedPcmSampleRate, args.sampleRate, chhRate);
        chhEnv *= std::exp(-args.sampleTime / chhDecaySec);
        float chhOut = chhSrc * chhEnv * 1.04f;
        chhOut = Ghost::TR909::bitReduce(chhOut, dbgBitDepth);
        chhOut = Ghost::TR909::driveWithAccent(
            chhOut, chhDrive, chhLatchedChar, CHH_ACCENT.driveAmt);
        chhOut *= chhLevel * 0.94f;
        chhOut *= chhLatchedGain * bus.masterVolume;

        // -- OH DSP (with choke override on env decay) -----------------
        float ohhRate     = std::pow(2.f, (ohhTune - 0.5f) * 2.f * OHH_TUNE_OCTAVES);
        float ohhDecaySec = OHH_DECAY_MIN_SEC
                          + ohhDecay * (OHH_DECAY_MAX_SEC - OHH_DECAY_MIN_SEC);
        const auto& ohSrc = ohhSource();
        float ohhSrc = Ghost::TR909::sampleAt(ohSrc, ohhSamplePos);
        ohhSamplePos += Ghost::TR909::playbackStep(
            Ghost::TR909::kEmbeddedPcmSampleRate, args.sampleRate, ohhRate);
        const float ohhEffectiveDecaySec = ohhChokeActive ? CHOKE_DECAY_SEC : ohhDecaySec;
        ohhEnv *= std::exp(-args.sampleTime / ohhEffectiveDecaySec);
        if (ohhChokeActive && ohhEnv < 1e-4f) ohhChokeActive = false;
        float ohhOut = ohhSrc * ohhEnv * 1.05f;
        ohhOut = Ghost::TR909::bitReduce(ohhOut, dbgBitDepth);
        ohhOut = Ghost::TR909::driveWithAccent(
            ohhOut, ohhDrive, ohhLatchedChar, OHH_ACCENT.driveAmt);
        ohhOut *= ohhLevel * 0.96f;
        ohhOut *= ohhLatchedGain * bus.masterVolume;

        outputs[CHH_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(chhOut));
        outputs[OHH_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(ohhOut));
    }
};


// ---------------------------------------------------------------------------
// Panel: 14 HP, two voice sections stacked, shared accent inputs at bottom.
// Mirrors CrashRide's structure.
// ---------------------------------------------------------------------------

struct ChhOhhPanel : rack::widget::Widget {
    void draw(const DrawArgs& args) override {
        Ghost::NineOhNine::drawDarkShell(
            args.vg, box.size, "OHCH", nullptr,
            nvgRGB(8, 8, 10),
            nvgRGBA(220, 235, 240, 220));

        const float cx = box.size.x * 0.5f;

        // Section dividers: horizontal hairlines mark the boundary between
        // the CLOSED and OPEN sections, and between OPEN and the shared
        // TACC zone. Mirrors the way the original 909 panel groups CH
        // and OH as visually separate strips.
        auto hLine = [&](float y) { Ghost::NineOhNine::drawDarkDivider(args.vg, box.size, y); };
        hLine(60.f);   // CLOSED  ─┤
        hLine(110.f);  // OPEN    ─┤  shared zone below

        // Section labels.
        nvgFontSize(args.vg, 6.f);
        nvgFillColor(args.vg, nvgRGBA(220, 235, 240, 220));
        nvgText(args.vg, cx, mm2px(18.f), "CLOSED", nullptr);
        nvgText(args.vg, cx, mm2px(68.f), "OPEN",   nullptr);

        // Knob and jack labels.
        const float xs[4] = {12.f, 28.f, 44.f, 60.f};
        const char* knobs[4] = {"TUNE", "DECAY", "DRIVE", "LEVEL"};
        nvgFontSize(args.vg, 4.5f);
        nvgFillColor(args.vg, nvgRGBA(200, 220, 230, 180));
        for (int v = 0; v < 2; v++) {
            float yKnob = (v == 0) ? 28.f : 78.f;
            for (int i = 0; i < 4; i++) {
                nvgText(args.vg, mm2px(xs[i]), mm2px(yKnob - 6.f), knobs[i], nullptr);
            }
        }

        // CLOSED IO row: TRIG | LACC | OUT (LACC is CH-only per Roland OM).
        const float chIoY = 48.f;
        nvgText(args.vg, mm2px(xs[0]), mm2px(chIoY - 6.f), "TRIG", nullptr);
        nvgText(args.vg, cx,           mm2px(chIoY - 6.f), "LACC", nullptr);
        nvgText(args.vg, mm2px(xs[3]), mm2px(chIoY - 6.f), "OUT",  nullptr);

        // OPEN IO row: TRIG | OUT only -- OH has no Accent B.
        const float ohIoY = 98.f;
        nvgText(args.vg, mm2px(xs[0]), mm2px(ohIoY - 6.f), "TRIG", nullptr);
        nvgText(args.vg, mm2px(xs[3]), mm2px(ohIoY - 6.f), "OUT",  nullptr);

        // Shared TACC at the bottom (applies to both voices). Centered
        // above the jack at y=121 so it reads as belonging to neither
        // section -- it is the rail shared by CLOSED and OPEN.
        nvgFontSize(args.vg, 5.f);
        nvgFillColor(args.vg, nvgRGBA(220, 235, 240, 200));
        nvgText(args.vg, cx, mm2px(115.f), "TACC", nullptr);
        nvgFontSize(args.vg, 3.6f);
        nvgFillColor(args.vg, nvgRGBA(160, 180, 195, 160));
        nvgText(args.vg, cx, mm2px(118.5f), "(shared)", nullptr);
    }
};

struct ChhOhhWidget : rack::ModuleWidget {
    ChhOhhWidget(ChhOhh* module) {
        setModule(module);

        auto* panel = new ChhOhhPanel;
        panel->box.size = AgentLayout::panelSize_14HP();
        addChild(panel);
        box.size = panel->box.size;

        AgentLayout::addScrews_14HP(this);

        const float xs[4] = {12.f, 28.f, 44.f, 60.f};
        const float cx_mm = 14.f * 5.08f * 0.5f;  // 14HP center (~35.56mm)

        // CLOSED voice (top section): knobs at y=30, IO row at y=48 with
        // TRIG | LACC | OUT. LACC sits inside this section because Accent B
        // is CH-only per Roland TR-909 OM.
        addParam(createParamCentered<rack::RoundSmallBlackKnob>(
            mm2px(Vec(xs[0], 30.f)), module, ChhOhh::CHH_TUNE_PARAM));
        addParam(createParamCentered<rack::RoundSmallBlackKnob>(
            mm2px(Vec(xs[1], 30.f)), module, ChhOhh::CHH_DECAY_PARAM));
        addParam(createParamCentered<rack::RoundSmallBlackKnob>(
            mm2px(Vec(xs[2], 30.f)), module, ChhOhh::CHH_DRIVE_PARAM));
        addParam(createParamCentered<rack::RoundSmallBlackKnob>(
            mm2px(Vec(xs[3], 30.f)), module, ChhOhh::CHH_LEVEL_PARAM));
        addInput(createInputCentered<rack::PJ301MPort>(
            mm2px(Vec(xs[0], 48.f)), module, ChhOhh::CHH_TRIG_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(
            mm2px(Vec(cx_mm, 48.f)), module, ChhOhh::LOCAL_ACC_INPUT));
        addOutput(createOutputCentered<rack::PJ301MPort>(
            mm2px(Vec(xs[3], 48.f)), module, ChhOhh::CHH_OUT_OUTPUT));

        // OPEN voice (middle section): knobs at y=80, IO row at y=98 with
        // TRIG | OUT only. OH has no Accent B per Roland OM.
        addParam(createParamCentered<rack::RoundSmallBlackKnob>(
            mm2px(Vec(xs[0], 80.f)), module, ChhOhh::OHH_TUNE_PARAM));
        addParam(createParamCentered<rack::RoundSmallBlackKnob>(
            mm2px(Vec(xs[1], 80.f)), module, ChhOhh::OHH_DECAY_PARAM));
        addParam(createParamCentered<rack::RoundSmallBlackKnob>(
            mm2px(Vec(xs[2], 80.f)), module, ChhOhh::OHH_DRIVE_PARAM));
        addParam(createParamCentered<rack::RoundSmallBlackKnob>(
            mm2px(Vec(xs[3], 80.f)), module, ChhOhh::OHH_LEVEL_PARAM));
        addInput(createInputCentered<rack::PJ301MPort>(
            mm2px(Vec(xs[0], 98.f)), module, ChhOhh::OHH_TRIG_INPUT));
        addOutput(createOutputCentered<rack::PJ301MPort>(
            mm2px(Vec(xs[3], 98.f)), module, ChhOhh::OHH_OUT_OUTPUT));

        // Shared zone (below the OPEN divider): TACC applies to both voices.
        addInput(createInputCentered<rack::PJ301MPort>(
            mm2px(Vec(cx_mm, 121.f)), module, ChhOhh::TOTAL_ACC_INPUT));
    }
};

rack::Model* modelChhOhh = createModel<ChhOhh, ChhOhhWidget>("ChhOhh");


// ---------------------------------------------------------------------------
// ChhOhhLab -- expanded expert / performance variant.
//
// Keeps the combined 909 hi-hat architecture intact, but exposes per-voice ROM
// bit depth so experts can push the sampled-hat side into crunchier territory
// without changing the main production module.
// ---------------------------------------------------------------------------

struct ChhOhhLabLabelCell { float xMm, yMm; const char* text; };

struct ChhOhhLab : Tr909Module {
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

    dsp::SchmittTrigger chhTrigger;
    dsp::SchmittTrigger ohhTrigger;
    float chhSamplePos = 1e9f;
    float chhEnv = 0.f;
    float ohhSamplePos = 1e9f;
    float ohhEnv = 0.f;
    bool ohhChokeActive = false;
    Ghost::TR909::AccentMix accentMix = Ghost::TR909::neutralMix();
    float chhLatchedGain = 1.f;
    float ohhLatchedGain = 1.f;
    float chhLatchedChar = 0.f;
    float ohhLatchedChar = 0.f;

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

    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::TR909::resolveBus(this);
        if (chhTrigger.process(inputs[CHH_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            chhSamplePos = 0.f;
            chhEnv = 1.f;
            auto acc = Ghost::TR909::sampleAccentAtTrig(
                this, TOTAL_ACC_INPUT, bus, accentMix, LOCAL_ACC_INPUT);
            chhLatchedChar = acc.charStrength;
            chhLatchedGain = acc.gain;
            if (ohhEnv > 1e-4f) ohhChokeActive = true;
        }
        if (ohhTrigger.process(inputs[OHH_TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            ohhSamplePos = 0.f;
            ohhEnv = 1.f;
            ohhChokeActive = false;
            auto acc = Ghost::TR909::sampleAccentAtTrig(
                this, TOTAL_ACC_INPUT, bus, accentMix);
            ohhLatchedChar = acc.charStrength;
            ohhLatchedGain = acc.gain;
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

        float chhRate = std::pow(2.f, (chhTune - 0.5f) * 2.f * CHH_TUNE_OCTAVES);
        float chhDecayShape = std::sqrt(chhDecay);
        float chhDecaySec = CHH_DECAY_MIN_SEC
                          + chhDecayShape * (CHH_DECAY_MAX_SEC - CHH_DECAY_MIN_SEC);
        float chhSrc = Ghost::TR909::sampleAt(chhSource(), chhSamplePos);
        chhSamplePos += Ghost::TR909::playbackStep(
            Ghost::TR909::kEmbeddedPcmSampleRate, args.sampleRate, chhRate);
        chhEnv *= std::exp(-args.sampleTime / chhDecaySec);
        float chhOut = chhSrc * chhEnv * 1.04f;
        chhOut = Ghost::TR909::bitReduce(chhOut, chhBits);
        chhOut = Ghost::TR909::driveWithAccent(
            chhOut, chhDrive, chhLatchedChar, CHH_ACCENT.driveAmt);
        chhOut *= chhLevel * 0.94f;
        chhOut *= chhLatchedGain * bus.masterVolume;

        float ohhRate = std::pow(2.f, (ohhTune - 0.5f) * 2.f * OHH_TUNE_OCTAVES);
        float ohhDecaySec = OHH_DECAY_MIN_SEC + ohhDecay * (OHH_DECAY_MAX_SEC - OHH_DECAY_MIN_SEC);
        float ohhSrc = Ghost::TR909::sampleAt(ohhSource(), ohhSamplePos);
        ohhSamplePos += Ghost::TR909::playbackStep(
            Ghost::TR909::kEmbeddedPcmSampleRate, args.sampleRate, ohhRate);
        const float ohhEffectiveDecaySec = ohhChokeActive ? CHOKE_DECAY_SEC : ohhDecaySec;
        ohhEnv *= std::exp(-args.sampleTime / ohhEffectiveDecaySec);
        if (ohhChokeActive && ohhEnv < 1e-4f) ohhChokeActive = false;
        float ohhOut = ohhSrc * ohhEnv * 1.05f;
        ohhOut = Ghost::TR909::bitReduce(ohhOut, ohhBits);
        ohhOut = Ghost::TR909::driveWithAccent(
            ohhOut, ohhDrive, ohhLatchedChar, OHH_ACCENT.driveAmt);
        ohhOut *= ohhLevel * 0.96f;
        ohhOut *= ohhLatchedGain * bus.masterVolume;

        outputs[CHH_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(chhOut));
        outputs[OHH_OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(ohhOut));
    }
};

struct ChhOhhLabPanel : rack::widget::Widget {
    std::vector<ChhOhhLabLabelCell> labels;

    void draw(const DrawArgs& args) override {
        Ghost::NineOhNine::drawLabShell(
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

struct ChhOhhLabWidget : rack::ModuleWidget {
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
