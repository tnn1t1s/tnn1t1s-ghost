#include <rack.hpp>
#include "AgentModule.hpp"
#include "PanelLayout.hpp"
#include "GhostPanel.hpp"
#include "GhostBus.hpp"
#include "ghost/signal/Audio.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "SvgHelper.hpp"

using namespace rack;
extern Plugin* pluginInstance;

/**
 * Kck -- 909-style kick drum.
 *
 * Direct port of a known-passable JUCE 909 kick generator with all internal
 * model parameters exposed via `KckFit::Config`. Mirrors the `TomFit::Config`
 * pattern used by Toms.cpp so the voice_lab fitting workflow can sweep any
 * constant via `--param fit_<name>=<value>`.
 *
 * Per-sample algorithm (since trigger):
 *   basePitch  = basePitchOffset + tune * basePitchSpan
 *   ampDecay   = ampDecayMin - decay * ampDecaySpan         (1/tau)
 *   fastSweep  = (pitchSweepFastBase + attack * pitchSweepFastAttack)
 *                * exp(-(pitchSweepFastRateBase + attack * pitchSweepFastRateAttack)
 *                      * pitchDecayScale * t)
 *                * pitchAmpScale
 *   slowSweep  = pitchSweepSlowBase
 *                * exp(-(pitchSweepSlowRateBase + (1-decay) * pitchSweepSlowRateDecay) * t)
 *   freq       = basePitch + fastSweep + slowSweep
 *   body       = sin(phase) * bodyFundGain
 *              + sin(phase * bodyHarmRatio + bodyHarmPhase) * bodyHarmGain
 *   subDecay   = subDecayBase + (1-decay) * subDecayInverse
 *   sub        = sin(phaseSub) * subGain * exp(-subDecay * t)
 *   ampEnv     = exp(-ampDecay * t)
 *   clickRate  = clickRateBase + attack * clickRateAttack
 *   clickEnv   = exp(-clickRate * t)
 *   clickNoise = noise() * clickEnv * (clickNoiseBase + attack * clickNoiseAttack)
 *   clickChirp = sin(2pi * (clickChirpStartHz - t * clickChirpRate) * t) * clickEnv
 *                * (clickChirpBase + attack * clickChirpAttack)
 *   out        = (body + sub) * ampEnv + clickNoise + clickChirp
 *   out        = HP(out, hpCoef)
 *   drive      = driveBase + decay * driveDecay + attack * driveAttack
 *                + driveExtra * driveExtraSpan
 *   out        = tanh(out * drive)
 *   final      = clamp(out, -1, 1) * outputGain * level
 *
 * 'attack' is our CLICK knob (0..1).
 * 'pitchAmpScale' = pitch_norm * 2 lets PITCH knob scale fast-sweep magnitude.
 * 'pitchDecayScale' = 0.5 + pitch_decay_norm scales the fast-sweep decay rate.
 * 'driveExtra' is the DRIVE knob, adds saturation on top of the JUCE default.
 *
 * Rack IDs (stable, never reorder):
 *   Params:  TUNE=0, DECAY=1, PITCH=2, PITCH_DECAY=3, CLICK=4, DRIVE=5, LEVEL=6
 *   Inputs:  TRIG=0, TUNE_CV=1, DECAY_CV=2, PITCH_CV=3, PITCH_DECAY_CV=4,
 *            CLICK_CV=5, DRIVE_CV=6, LEVEL_CV=7, ACCENT=8
 *   Outputs: OUT=0
 */

namespace KckFit {

struct Config {
    // Pitch range (calibrated against the reference machine BD ref tune050-attack050-decay050 = 49.8 Hz).
    // basePitch midpoint 45 Hz; FFT measurement adds ~5 Hz from slow-sweep residual.
    // Calibrated so TUNE=100% -> 68.5 Hz (the reference match, 2026-06-01).
    float basePitchOffset           = 38.0f;   // TUNE=0 -> 38 Hz
    float basePitchSpan             = 30.48f;  // TUNE=1 -> 68.48 Hz (matched)

    // Body envelope (1/tau range), calibrated against the reference machine BD: -20 dB at 170 ms
    // (tau ~ 73 ms, ampDecay ~ 13.5) at decay=0.5. JUCE original 2.25/1.75 was 10x too slow.
    float ampDecayMin               = 26.f;    // DECAY=0.5 -> 17.8 (matched); range tau ~38..104 ms
    float ampDecaySpan              = 16.4f;

    // Fast pitch sweep. DAFx-14 paper §8.1 reports the real 909 attack
    // frequency shift lasts ~6 ms ("less than a single period at the higher
    // frequency"). JUCE's rate of 38 (tau ~26 ms) was 4x too slow, smearing
    // pitch energy into the 60-100 Hz band during the body window.
    float pitchSweepFastBase        = 112.f;    // Hz
    float pitchSweepFastAttack      = 65.f;
    float pitchSweepFastRateBase    = 28.f;     // PITCH-DECAY=0.5 -> sweep 1/tau 28 (matched)
    float pitchSweepFastRateAttack  = 22.f;

    // Slow pitch sigh. DAFx-14 describes the 909's R161-leakage sigh as
    // "subtle". JUCE's 20 Hz / tau ~154 ms smeared the fundamental from ~58
    // down to ~46 Hz across the body window, putting a wandering 2H peak in
    // the 90-118 Hz region (audible as messy noise around 100 Hz).
    float pitchSweepSlowBase        = 8.f;      // (was 20)
    float pitchSweepSlowRateBase    = 15.f;     // 1/tau ~ 67 ms (was 6.5 / tau 154 ms)
    float pitchSweepSlowRateDecay   = 3.6f;     // additional rate at decay=0

    // Body harmonic content. The spectrum-faithful pass set bodyHarmGain=0.05
    // (matching ref 2H at -27 dB), but the artistic ear preferred the JUCE
    // value 0.19 for added body character. The 3H term remains as the analog
    // bite contribution.
    float bodyFundGain              = 0.696f;   // matched
    float bodyHarmRatio             = 2.02f;    // slightly detuned 2x for shimmer
    float bodyHarmPhase             = 0.10f;
    float bodyHarmGain              = 0.19f;    // ear-tuned: JUCE value
    float bodyThirdHarmGain         = 0.06f;

    // --- 909 rebuild: clipped-triangle body + single pitch sweep ---------
    // The 909 body is a saturated triangle filtered back toward a sine, not a
    // pure sine + harmonic stack (a clean sine "sounds cheap"). These drive
    // the new KckVoice::process; the sine/sub/chirp fields above are retained
    // only so KckLab still compiles, and are unused by the rebuilt engine.
    float bodyClip                  = 2.4f;    // triangle saturation (the 909 "dirt")
    float bodyLpCoef                = 0.40f;   // (legacy one-pole; unused once resonant SVF on)
    float clickPulseGain            = 0.6f;    // short attack pulse level

    // Resonant sweep (the 909 "rez" zap at high TUNE). The sweep is a RATIO of
    // the fundamental (so it widens dramatically as you tune up), and the body
    // runs through a resonant 2-pole filter whose cutoff tracks the swept pitch.
    float sweepRatio                = 4.0f;    // start freq = fundamental * (1 + sweepRatio*pitch)
    float bodyReso                  = 0.62f;   // 0..1 body filter resonance (the zap)
    float bodyFcMult                = 1.30f;   // body filter cutoff = freq * this (brightness)

    // Sub component retained on purpose. The strict circuit-faithful pass
    // disabled this entirely, but in live use the extra low-end weight made
    // Kck feel much closer to a playable 909 kick. Kck keeps that choice;
    // KckLab still exposes the underlying terms when a purist fit is wanted.
    float subRatio                  = 0.50f;
    float subGain                   = 0.36f;    // ear-tuned: JUCE value
    float subDecayBase              = 0.85f;
    float subDecayInverse           = 0.45f;

    // Click. The 1700 Hz tonal chirp added a persistent "rimshot ping" on top
    // of the kick body even at half magnitude; the real 909 click is a brief
    // filtered noise burst, not a tonal transient. Chirp disabled by default
    // (still tweakable via dbg knobs / fit_*); noise reduced for cleaner attack.
    float clickRateBase             = 140.f;
    float clickRateAttack           = 170.f;
    float clickNoiseBase            = 0.03f;    // (was 0.05)
    float clickNoiseAttack          = 0.10f;    // (was 0.18)
    float clickChirpStartHz         = 1700.f;
    float clickChirpRate            = 400.f;
    float clickChirpBase            = 0.f;      // (was 0.02) chirp off by default
    float clickChirpAttack          = 0.f;      // (was 0.10)

    // HP cutoff: spectrum-faithful pass raised this to 0.005 (~35 Hz) to clean
    // sub mud, but the artistic ear preferred the JUCE value 0.0012 (~8 Hz)
    // which keeps more deep bass through.
    float hpCoef                    = 0.0012f;  // ear-tuned: JUCE value
    // Drive: spectrum-faithful pass set this to 1.0 to avoid intermod 2H, but
    // the artistic ear preferred the JUCE saturator at 1.55 for bite.
    float driveBase                 = 1.55f;    // matched
    float driveDecay                = 0.42f;
    float driveAttack               = 0.35f;
    float driveExtraSpan            = 1.0f;     // DRIVE knob 0..1 -> 0..1 added to drive amount

    // Output
    float outputGain                = 1.0f;

    // Accent application -- two orthogonal axes, see Tr909Bus.hpp.
    //
    // accentMix decides the LEVEL relationship across the four cases
    // (ghost / global / local / both) in dB. Wide spread for pronounced,
    // 909-style accent dynamics + multi-level multi-level kick rolls;
    // ghost stays -6 dB so steady patterns / the -6 dBFS calibration hold.
    Ghost::AccentMix accentMix = Ghost::Accent::kickMix();

    // Per-DSP-stage CHARACTER weights, applied multiplicatively at
    // fire-time when the hit is accented (any accent gate fired).
    // Defaults below are Kck's tuned values; AccentCharacter member order
    // is body, pitch, click, drive, noise, snap, decay, brightness, bend.
    // Note: there is intentionally NO level field here. Level on / across
    // accent cases is governed by AccentMix dB; a per-voice level boost
    // here would double-count with the shared abstraction.
    // Shared gentle-accent policy (see Tr909Bus.hpp Accent::). Click + drive,
    // no pitch/body sweep. Tune the suite's accent feel in Accent::, not here.
    Ghost::AccentCharacter accent = Ghost::Accent::kick();
};

inline Config makeKick() { return Config{}; }

}  // namespace KckFit

namespace {

static constexpr float TWO_PI   = 6.28318530717958647692f;
static constexpr float CV_SCALE = 0.1f;

static inline float kckNormWithCV(rack::Module& self, int paramId, int inputId) {
    float norm = self.params[paramId].getValue()
               + self.inputs[inputId].getVoltage() * CV_SCALE;
    return rack::math::clamp(norm, 0.f, 1.f);
}

struct KckVoice {
    dsp::SchmittTrigger trigger;
    float phase    = 0.f;
    float phaseSub = 0.f;
    float t        = 0.f;
    float hpState  = 0.f;
    float bodyLp   = 0.f;   // (legacy one-pole; unused once resonant SVF on)
    float clickLp  = 0.f;   // one-pole LP state for the click noise
    float svfLp    = 0.f;   // resonant body filter state (lowpass)
    float svfBp    = 0.f;   // resonant body filter state (bandpass)
    bool  active   = false;
    uint32_t rngState = 1u;

    // Latched at the moment fire() is called; constant for the whole hit.
    // Per #73 design: sample-at-trig, no decay over the envelope.
    float latchedAccent = 0.f;

    void fire(float accentStrength) {
        phase = phaseSub = 0.f;
        bodyLp = clickLp = 0.f;
        svfLp = svfBp = 0.f;
        t = 0.f;
        hpState = 0.f;
        active = true;
        rngState = 1978u;
        latchedAccent = rack::math::clamp(accentStrength, 0.f, 1.f);
    }
    void fire() { fire(0.f); }

    inline float nextNoise() {
        // Numerical Recipes LCG; map upper bits to [-1, 1).
        rngState = rngState * 1664525u + 1013904223u;
        return ((rngState >> 8) & 0xFFFFFFu) * (2.f / 16777216.f) - 1.f;
    }

    float process(const rack::Module::ProcessArgs& args,
                  const KckFit::Config& fit,
                  float tuneNorm,
                  float decayNorm,
                  float pitchNorm,
                  float pitchDecayNorm,
                  float attackNorm,
                  float driveNorm,
                  float levelNorm) {
        if (!active) return 0.f;

        const float acc = latchedAccent;  // 0..1, latched at fire()

        // --- Authentic 909 rebuild -------------------------------------
        // ONE pitch envelope (instant attack, exp decay) sweeps the osc from
        // high down to the fundamental. TUNE sets the fundamental; the sweep
        // rides consistently above it -- that drop is the punch.
        const float fundamental = fit.basePitchOffset + tuneNorm * fit.basePitchSpan;
        // RATIO-based sweep: start freq is a MULTIPLE of the fundamental, so the
        // sweep widens dramatically as TUNE goes up (the 909 high-tune "rez" zap).
        const float sweepAmt = fit.sweepRatio
                             * (0.20f + pitchNorm * 0.70f)   // PITCH=0.5 -> matched depth
                             * (1.f + acc * fit.accent.pitchAmt);
        const float sweepRate = fit.pitchSweepFastRateBase
                              * (0.50f + pitchDecayNorm * 1.00f);  // PITCH-DECAY=0.5 -> matched
        const float freq = fundamental * (1.f + sweepAmt * std::exp(-sweepRate * t));

        phase += TWO_PI * freq * args.sampleTime;
        if (phase > TWO_PI) phase -= TWO_PI;

        // Body: saturated triangle (the 909 "dirt") into a RESONANT 2-pole filter
        // whose cutoff tracks the swept pitch -- the resonance "zaps" as the
        // pitch sweeps, which is the bridged-T character missing before.
        const float pn   = phase * (1.f / TWO_PI);             // 0..1
        const float tri  = 4.f * std::fabs(pn - 0.5f) - 1.f;    // -1..1 triangle
        const float clip = fit.bodyClip + driveNorm * 2.f + acc * fit.accent.driveAmt;
        const float sat  = std::tanh(tri * clip) / std::tanh(clip);

        // Chamberlin state-variable filter, cutoff tracking freq, Q from bodyReso.
        float fc = freq * fit.bodyFcMult;
        const float fn = rack::math::clamp(fc * args.sampleTime, 0.f, 0.45f);  // fc/sr
        const float fcoef = 2.f * std::sin(3.14159265f * fn);
        const float q = 2.f - fit.bodyReso * 1.9f;             // higher reso -> lower damping
        svfLp += fcoef * svfBp;
        const float hp = sat - svfLp - q * svfBp;
        svfBp += fcoef * hp;
        svfLp = rack::math::clamp(svfLp, -2.f, 2.f);           // safety

        const float ampEnv  = std::exp(-(fit.ampDecayMin - decayNorm * fit.ampDecaySpan) * t);
        const float bodyOut = svfLp * fit.bodyFundGain * ampEnv
                            * (1.f + acc * fit.accent.bodyAmt);

        // Click: short pulse + low-passed noise burst (the attack transient).
        // attackNorm is the CLICK knob; ATTACK macro feeds clickRateBase.
        const float clickEnv = std::exp(-(fit.clickRateBase
                                          + attackNorm * fit.clickRateAttack) * t);
        clickLp += 0.5f * (nextNoise() - clickLp);              // ~one-pole LP noise (~4 kHz)
        const float pulse = (t < 0.002f) ? 1.f : 0.f;           // ~2 ms onset pulse
        const float click =
            (clickLp * (fit.clickNoiseBase + attackNorm * fit.clickNoiseAttack)
             + pulse * fit.clickPulseGain * attackNorm)
            * clickEnv * (1.f + acc * fit.accent.clickAmt);

        float out = bodyOut + click;

        // Leaky DC-blocking HP.
        hpState += fit.hpCoef * (out - hpState);
        out -= hpState;

        out = std::tanh(out * (fit.driveBase + acc * fit.accent.driveAmt));

        const float levelGain = fit.outputGain * levelNorm;
        out = rack::math::clamp(out, -1.f, 1.f) * levelGain;

        t += args.sampleTime;
        if (ampEnv < 1e-5f && t > 0.5f) active = false;

        return out;
    }
};

} // namespace


// ---------------------------------------------------------------------------
// Kck -- production module
// ---------------------------------------------------------------------------

struct Kck : GhostModule {
    enum ParamId  {
        TUNE_PARAM, DECAY_PARAM, PITCH_PARAM, PITCH_DECAY_PARAM,
        CLICK_PARAM, DRIVE_PARAM, LEVEL_PARAM,
        ATTACK_PARAM, TONE_PARAM,
        NUM_PARAMS
    };
    enum InputId  {
        TRIG_INPUT, TUNE_CV_INPUT, DECAY_CV_INPUT, PITCH_CV_INPUT,
        PITCH_DECAY_CV_INPUT, CLICK_CV_INPUT, DRIVE_CV_INPUT, LEVEL_CV_INPUT,
        ATTACK_CV_INPUT, TONE_CV_INPUT,
        LOCAL_ACC_INPUT, TOTAL_ACC_INPUT,
        NUM_INPUTS
    };
    enum OutputId { OUT_OUTPUT, NUM_OUTPUTS };

    KckVoice voice;
    KckFit::Config fit;

    Kck() {
        fit = KckFit::makeKick();
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        // Calibrated 2026-06-01: TUNE=100% + every shaping knob at noon = the reference match.
        configParam(TUNE_PARAM,        0.f, 1.f, 1.00f, "Tune",         "%", 0.f, 100.f);
        configParam(DECAY_PARAM,       0.f, 1.f, 0.50f, "Decay",        "%", 0.f, 100.f);
        configParam(PITCH_PARAM,       0.f, 1.f, 0.50f, "Pitch amount", "%", 0.f, 100.f);
        configParam(PITCH_DECAY_PARAM, 0.f, 1.f, 0.50f, "Pitch decay",  "%", 0.f, 100.f);
        configParam(CLICK_PARAM,       0.f, 1.f, 0.50f, "Click",        "%", 0.f, 100.f);
        configParam(DRIVE_PARAM,       0.f, 1.f, 0.50f, "Drive",        "%", 0.f, 100.f);
        configParam(LEVEL_PARAM,       0.f, 1.f, 0.85f, "Level",        "%", 0.f, 100.f);
        configParam(ATTACK_PARAM,      0.f, 1.f, 0.50f, "Attack",       "%", 0.f, 100.f);
        configParam(TONE_PARAM,        0.f, 1.f, 0.50f, "Tone",         "%", 0.f, 100.f);
        configInput (TRIG_INPUT,           "Trigger");
        configInput (TUNE_CV_INPUT,        "Tune CV");
        configInput (DECAY_CV_INPUT,       "Decay CV");
        configInput (PITCH_CV_INPUT,       "Pitch amount CV");
        configInput (PITCH_DECAY_CV_INPUT, "Pitch decay CV");
        configInput (CLICK_CV_INPUT,       "Click CV");
        configInput (DRIVE_CV_INPUT,       "Drive CV");
        configInput (LEVEL_CV_INPUT,       "Level CV");
        configInput (ATTACK_CV_INPUT,      "Attack CV");
        configInput (TONE_CV_INPUT,        "Tone CV");
        configInput (LOCAL_ACC_INPUT,      "Local accent (Accent B, sampled at TRIG)");
        configInput (TOTAL_ACC_INPUT,      "Total accent (Accent A, sampled at TRIG)");
        configOutput(OUT_OUTPUT,           "Audio");
    }

    // Latched at TRIG rising edge along with voice.latchedAccent. Determines
    // the per-case output level (ghost / global / local / both dB lookup);
    // see Tr909Bus.hpp::resolveAccentGain for the math. Held constant for
    // the duration of the hit so knob movements during a hit don't shift
    // its level mid-flight.
    float latchedCaseGain = 1.f;

    void process(const ProcessArgs& args) override {
        const auto bus = Ghost::resolveBus(this);

        if (voice.trigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            // Hit-time gates from cables (deterministic, zero latency).
            const bool totalGate = inputs[TOTAL_ACC_INPUT].getNormalVoltage(0.f) > 1.f;
            const bool localGate = inputs[LOCAL_ACC_INPUT].getNormalVoltage(0.f) > 1.f;

            // Two orthogonal axes:
            //   character: boolean -- voice DSP applies its full accent
            //              feel (drive, pitch dive, click) on any accent.
            //   level:     per-case dB lerp from AccentMix, latched once.
            const float charStrength =
                Ghost::isAccentedHit(totalGate, localGate) ? 1.f : 0.f;
            latchedCaseGain = Ghost::resolveAccentGain(
                totalGate, localGate, bus, fit.accentMix);
            voice.fire(charStrength);
        }

        float tuneNorm       = kckNormWithCV(*this, TUNE_PARAM,        TUNE_CV_INPUT);
        float decayNorm      = kckNormWithCV(*this, DECAY_PARAM,       DECAY_CV_INPUT);
        float pitchNorm      = kckNormWithCV(*this, PITCH_PARAM,       PITCH_CV_INPUT);
        float pitchDecayNorm = kckNormWithCV(*this, PITCH_DECAY_PARAM, PITCH_DECAY_CV_INPUT);
        float clickNorm      = kckNormWithCV(*this, CLICK_PARAM,       CLICK_CV_INPUT);
        float driveNorm      = kckNormWithCV(*this, DRIVE_PARAM,       DRIVE_CV_INPUT);
        float levelNorm      = kckNormWithCV(*this, LEVEL_PARAM,       LEVEL_CV_INPUT);
        float attackNorm     = kckNormWithCV(*this, ATTACK_PARAM,      ATTACK_CV_INPUT);
        float toneNorm       = kckNormWithCV(*this, TONE_PARAM,        TONE_CV_INPUT);

        // Calibration (2026-06-01 reference match): the matched sound is TUNE=100%
        // with every shaping knob at noon. CLICK/DRIVE are remapped so noon hits
        // the matched values; ATTACK/TONE macro the click sharpness / body
        // brightness so noon -> matched clickRate 140 / bodyFcMult 1.30.
        KckFit::Config f = fit;
        f.clickRateBase = 60.f + 160.f * attackNorm;          // ATTACK=0.5 -> 140
        f.bodyFcMult    = 0.70f + 1.20f * toneNorm;           // TONE=0.5   -> 1.30
        const float clickEff = rack::math::clamp(0.305f + clickNorm, 0.f, 1.f);  // CLICK=0.5 -> 0.805
        const float driveEff = rack::math::clamp((driveNorm - 0.5f) * 2.f, 0.f, 1.f); // DRIVE=0.5 -> 0

        float out = voice.process(args, f,
                                  tuneNorm, decayNorm, pitchNorm, pitchDecayNorm,
                                  clickEff, driveEff, levelNorm);
        out *= latchedCaseGain * bus.masterVolume;
        outputs[OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(out));
    }
};


// ---------------------------------------------------------------------------
// Production widget -- panelkit SVG (res/Kck.svg) + SvgHelper name binding
// ---------------------------------------------------------------------------

struct KckWidget : ModuleWidget, SvgHelper<KckWidget> {
    KckWidget(Kck* module) {
        setModule(module);
        loadPanel(asset::plugin(pluginInstance, "res/Kck.svg"));

        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        bindParam<RoundBlackKnob>("param.main.tune",        Kck::TUNE_PARAM);
        bindParam<RoundBlackKnob>("param.main.decay",       Kck::DECAY_PARAM);
        bindParam<RoundBlackKnob>("param.main.pitch",       Kck::PITCH_PARAM);
        bindParam<RoundBlackKnob>("param.main.pitch-decay", Kck::PITCH_DECAY_PARAM);
        bindParam<RoundBlackKnob>("param.main.click",       Kck::CLICK_PARAM);
        bindParam<RoundBlackKnob>("param.main.attack",      Kck::ATTACK_PARAM);
        bindParam<RoundBlackKnob>("param.main.tone",        Kck::TONE_PARAM);
        bindParam<RoundBlackKnob>("param.main.drive",       Kck::DRIVE_PARAM);
        bindParam<RoundBlackKnob>("param.main.level",       Kck::LEVEL_PARAM);

        bindInput<PJ301MPort>("cv.main.tune",        Kck::TUNE_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.decay",       Kck::DECAY_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.pitch",       Kck::PITCH_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.pitch-decay", Kck::PITCH_DECAY_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.click",       Kck::CLICK_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.attack",      Kck::ATTACK_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.tone",        Kck::TONE_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.drive",       Kck::DRIVE_CV_INPUT);
        bindInput<PJ301MPort>("cv.main.level",       Kck::LEVEL_CV_INPUT);

        bindInput<PJ301MPort>("trig.main.trig",     Kck::TRIG_INPUT);
        bindInput<PJ301MPort>("accent.main.local",  Kck::LOCAL_ACC_INPUT);
        bindInput<PJ301MPort>("accent.main.total",  Kck::TOTAL_ACC_INPUT);
        bindOutput<PJ301MPort>("out.main.audio",    Kck::OUT_OUTPUT);
    }
};

rack::Model* modelKck = createModel<Kck, KckWidget>("Kck");


// ---------------------------------------------------------------------------
// KckLab -- expanded expert / fitting variant.
//
// Same engine as Kck. Exposes the 7 user-facing controls plus the most
// informative KckFit knobs for hand-fitting. This is deliberately a curated
// 18HP expert surface, not a dump of every internal constant. Once a setting
// sounds right, copy the values back into KckFit::makeKick().
// ---------------------------------------------------------------------------

struct KckLab : GhostModule {
    float latchedCaseGain = 1.f;

    enum ParamId {
        TUNE_PARAM, DECAY_PARAM, PITCH_PARAM, PITCH_DECAY_PARAM,
        CLICK_PARAM, DRIVE_PARAM, LEVEL_PARAM,
        // Fit knobs follow.
        BASE_PITCH_OFFSET_PARAM, BASE_PITCH_SPAN_PARAM,
        AMP_DECAY_MIN_PARAM,    AMP_DECAY_SPAN_PARAM,
        SWEEP_FAST_BASE_PARAM,  SWEEP_FAST_RATE_PARAM,
        SWEEP_SLOW_BASE_PARAM,  SWEEP_SLOW_RATE_PARAM,
        BODY_FUND_GAIN_PARAM,   BODY_HARM_RATIO_PARAM,  BODY_HARM_GAIN_PARAM,
        SUB_GAIN_PARAM,         SUB_DECAY_PARAM,
        CLICK_RATE_PARAM,       CLICK_NOISE_PARAM,
        CHIRP_START_PARAM,      CHIRP_RATE_PARAM,       CHIRP_GAIN_PARAM,
        HP_COEF_PARAM,          DRIVE_BASE_PARAM,       OUTPUT_GAIN_PARAM,
        NUM_PARAMS
    };
    // Per-knob CV inputs: one per ParamId. This keeps KckLab in the "full
    // expert instrument" tier, unlike the other Lab modules which are knob-only.
    enum InputId  {
        TRIG_INPUT, LOCAL_ACC_INPUT, TOTAL_ACC_INPUT,
        TUNE_CV, DECAY_CV, PITCH_CV, PITCH_DECAY_CV,
        CLICK_CV, DRIVE_CV, LEVEL_CV,
        BASE_PITCH_OFFSET_CV, BASE_PITCH_SPAN_CV,
        AMP_DECAY_MIN_CV, AMP_DECAY_SPAN_CV,
        SWEEP_FAST_BASE_CV, SWEEP_FAST_RATE_CV,
        SWEEP_SLOW_BASE_CV, SWEEP_SLOW_RATE_CV,
        BODY_FUND_GAIN_CV, BODY_HARM_RATIO_CV, BODY_HARM_GAIN_CV,
        SUB_GAIN_CV, SUB_DECAY_CV,
        CLICK_RATE_CV, CLICK_NOISE_CV,
        CHIRP_START_CV, CHIRP_RATE_CV, CHIRP_GAIN_CV,
        HP_COEF_CV, DRIVE_BASE_CV, OUTPUT_GAIN_CV,
        NUM_INPUTS
    };
    enum OutputId { OUT_OUTPUT, NUM_OUTPUTS };

    KckVoice voice;
    KckFit::Config fit;

    KckLab() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        // Playable
        configParam(TUNE_PARAM,        0.f, 1.f, 0.50f,  "Tune");
        configParam(DECAY_PARAM,       0.f, 1.f, 0.50f,  "Decay");
        configParam(PITCH_PARAM,       0.f, 1.f, 0.385f, "Pitch amount");
        configParam(PITCH_DECAY_PARAM, 0.f, 1.f, 0.26f,  "Pitch decay");
        configParam(CLICK_PARAM,       0.f, 1.f, 0.50f, "Click / attack");
        configParam(DRIVE_PARAM,       0.f, 1.f, 0.f,   "Drive (extra)");
        configParam(LEVEL_PARAM,       0.f, 1.f, 0.85f, "Level");
        // Fit
        configParam(BASE_PITCH_OFFSET_PARAM, 10.f,  80.f,    20.f,   "Base pitch offset", " Hz");
        configParam(BASE_PITCH_SPAN_PARAM,   10.f, 120.f,    50.f,   "Base pitch span",   " Hz");
        configParam(AMP_DECAY_MIN_PARAM,      1.f,  40.f,    20.f,   "Body decay min (1/tau)");
        configParam(AMP_DECAY_SPAN_PARAM,     0.f,  30.f,    13.f,   "Body decay span");
        configParam(SWEEP_FAST_BASE_PARAM,    0.f, 300.f,   112.f,   "Fast sweep amp",    " Hz");
        configParam(SWEEP_FAST_RATE_PARAM,    1.f, 300.f,   150.f,   "Fast sweep rate (1/tau)");
        configParam(SWEEP_SLOW_BASE_PARAM,    0.f, 100.f,     8.f,   "Slow sweep amp",    " Hz");
        configParam(SWEEP_SLOW_RATE_PARAM,    0.5f, 50.f,    15.f,   "Slow sweep rate (1/tau)");
        configParam(BODY_FUND_GAIN_PARAM,     0.f,   1.5f,    0.88f, "Body fundamental gain");
        configParam(BODY_HARM_RATIO_PARAM,    1.f,   3.f,     2.02f, "Body harmonic ratio");
        configParam(BODY_HARM_GAIN_PARAM,     0.f,   1.f,     0.19f, "Body harmonic gain");
        configParam(SUB_GAIN_PARAM,           0.f,   1.f,     0.36f, "Sub gain");
        configParam(SUB_DECAY_PARAM,          0.1f,  5.f,     0.85f, "Sub decay (1/tau)");
        configParam(CLICK_RATE_PARAM,        20.f, 600.f,   140.f,   "Click rate (1/tau)");
        configParam(CLICK_NOISE_PARAM,        0.f,   1.f,     0.03f, "Click noise gain");
        configParam(CHIRP_START_PARAM,      400.f,3000.f,  1700.f,   "Click chirp start", " Hz");
        configParam(CHIRP_RATE_PARAM,         0.f,1500.f,   400.f,   "Click chirp falling rate");
        configParam(CHIRP_GAIN_PARAM,         0.f,   1.f,     0.f,   "Click chirp gain");
        configParam(HP_COEF_PARAM,            0.f,   0.05f,   0.0012f,"HP coef");
        configParam(DRIVE_BASE_PARAM,         0.5f,  4.f,     1.55f, "Drive base");
        configParam(OUTPUT_GAIN_PARAM,        0.f,   2.f,     1.f,   "Output gain");

        configInput (TRIG_INPUT,         "Trigger");
        configInput (LOCAL_ACC_INPUT,    "Local accent (Accent B, sampled at TRIG)");
        configInput (TOTAL_ACC_INPUT,    "Total accent (Accent A, sampled at TRIG)");

        configInput(TUNE_CV,                   "Tune CV");
        configInput(DECAY_CV,                  "Decay CV");
        configInput(PITCH_CV,                  "Pitch amount CV");
        configInput(PITCH_DECAY_CV,            "Pitch decay CV");
        configInput(CLICK_CV,                  "Click CV");
        configInput(DRIVE_CV,                  "Drive CV");
        configInput(LEVEL_CV,                  "Level CV");
        configInput(BASE_PITCH_OFFSET_CV,      "Base pitch offset CV");
        configInput(BASE_PITCH_SPAN_CV,        "Base pitch span CV");
        configInput(AMP_DECAY_MIN_CV,          "Amp decay min CV");
        configInput(AMP_DECAY_SPAN_CV,         "Amp decay span CV");
        configInput(SWEEP_FAST_BASE_CV,        "Fast sweep amp CV");
        configInput(SWEEP_FAST_RATE_CV,        "Fast sweep rate CV");
        configInput(SWEEP_SLOW_BASE_CV,        "Slow sweep amp CV");
        configInput(SWEEP_SLOW_RATE_CV,        "Slow sweep rate CV");
        configInput(BODY_FUND_GAIN_CV,         "Body fundamental gain CV");
        configInput(BODY_HARM_RATIO_CV,        "Body harm ratio CV");
        configInput(BODY_HARM_GAIN_CV,         "Body harm gain CV");
        configInput(SUB_GAIN_CV,               "Sub gain CV");
        configInput(SUB_DECAY_CV,              "Sub decay CV");
        configInput(CLICK_RATE_CV,             "Click rate CV");
        configInput(CLICK_NOISE_CV,            "Click noise CV");
        configInput(CHIRP_START_CV,            "Chirp start Hz CV");
        configInput(CHIRP_RATE_CV,             "Chirp falling rate CV");
        configInput(CHIRP_GAIN_CV,             "Chirp gain CV");
        configInput(HP_COEF_CV,                "HP coef CV");
        configInput(DRIVE_BASE_CV,             "Drive base CV");
        configInput(OUTPUT_GAIN_CV,            "Output gain CV");

        configOutput(OUT_OUTPUT,   "Audio");
    }

    // Read knob value plus CV (0.1 of param range per volt), clamped to range.
    inline float readWithCV(int paramId, int cvInputId) {
        rack::engine::ParamQuantity* q = paramQuantities[paramId];
        float range = q->maxValue - q->minValue;
        float v = params[paramId].getValue()
                + inputs[cvInputId].getVoltage() * 0.1f * range;
        return rack::math::clamp(v, q->minValue, q->maxValue);
    }

    void process(const ProcessArgs& args) override {
        // Live-copy fit knobs (with CV) into engine config every frame.
        fit.basePitchOffset          = readWithCV(BASE_PITCH_OFFSET_PARAM, BASE_PITCH_OFFSET_CV);
        fit.basePitchSpan            = readWithCV(BASE_PITCH_SPAN_PARAM,   BASE_PITCH_SPAN_CV);
        fit.ampDecayMin              = readWithCV(AMP_DECAY_MIN_PARAM,     AMP_DECAY_MIN_CV);
        fit.ampDecaySpan             = readWithCV(AMP_DECAY_SPAN_PARAM,    AMP_DECAY_SPAN_CV);
        fit.pitchSweepFastBase       = readWithCV(SWEEP_FAST_BASE_PARAM,   SWEEP_FAST_BASE_CV);
        fit.pitchSweepFastRateBase   = readWithCV(SWEEP_FAST_RATE_PARAM,   SWEEP_FAST_RATE_CV);
        fit.pitchSweepSlowBase       = readWithCV(SWEEP_SLOW_BASE_PARAM,   SWEEP_SLOW_BASE_CV);
        fit.pitchSweepSlowRateBase   = readWithCV(SWEEP_SLOW_RATE_PARAM,   SWEEP_SLOW_RATE_CV);
        fit.bodyFundGain             = readWithCV(BODY_FUND_GAIN_PARAM,    BODY_FUND_GAIN_CV);
        fit.bodyHarmRatio            = readWithCV(BODY_HARM_RATIO_PARAM,   BODY_HARM_RATIO_CV);
        fit.bodyHarmGain             = readWithCV(BODY_HARM_GAIN_PARAM,    BODY_HARM_GAIN_CV);
        fit.subGain                  = readWithCV(SUB_GAIN_PARAM,          SUB_GAIN_CV);
        fit.subDecayBase             = readWithCV(SUB_DECAY_PARAM,         SUB_DECAY_CV);
        fit.clickRateBase            = readWithCV(CLICK_RATE_PARAM,        CLICK_RATE_CV);
        fit.clickNoiseBase           = readWithCV(CLICK_NOISE_PARAM,       CLICK_NOISE_CV);
        fit.clickChirpStartHz        = readWithCV(CHIRP_START_PARAM,       CHIRP_START_CV);
        fit.clickChirpRate           = readWithCV(CHIRP_RATE_PARAM,        CHIRP_RATE_CV);
        fit.clickChirpBase           = readWithCV(CHIRP_GAIN_PARAM,        CHIRP_GAIN_CV);
        fit.hpCoef                   = readWithCV(HP_COEF_PARAM,           HP_COEF_CV);
        fit.driveBase                = readWithCV(DRIVE_BASE_PARAM,        DRIVE_BASE_CV);
        fit.outputGain               = readWithCV(OUTPUT_GAIN_PARAM,       OUTPUT_GAIN_CV);

        const auto bus = Ghost::resolveBus(this);

        if (voice.trigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f)) {
            const bool totalGate = inputs[TOTAL_ACC_INPUT].getNormalVoltage(0.f) > 1.f;
            const bool localGate = inputs[LOCAL_ACC_INPUT].getNormalVoltage(0.f) > 1.f;
            const float charStrength =
                Ghost::isAccentedHit(totalGate, localGate) ? 1.f : 0.f;
            latchedCaseGain = Ghost::resolveAccentGain(
                totalGate, localGate, bus, fit.accentMix);
            voice.fire(charStrength);
        }

        const float tuneNorm       = readWithCV(TUNE_PARAM,        TUNE_CV);
        const float decayNorm      = readWithCV(DECAY_PARAM,       DECAY_CV);
        const float pitchNorm      = readWithCV(PITCH_PARAM,       PITCH_CV);
        const float pitchDecayNorm = readWithCV(PITCH_DECAY_PARAM, PITCH_DECAY_CV);
        const float clickNorm      = readWithCV(CLICK_PARAM,       CLICK_CV);
        const float driveNorm      = readWithCV(DRIVE_PARAM,       DRIVE_CV);
        const float levelNorm      = readWithCV(LEVEL_PARAM,       LEVEL_CV);

        float out = voice.process(args, fit,
                                  tuneNorm, decayNorm, pitchNorm, pitchDecayNorm,
                                  clickNorm, driveNorm, levelNorm);
        out *= latchedCaseGain * bus.masterVolume;
        outputs[OUT_OUTPUT].setVoltage(Ghost::Signal::Audio::toRackVolts(out));
    }
};


struct KckLabLabelCell { float xMm, yMm; const char* text; };

struct KckLabPanel : rack::widget::Widget {
    std::vector<KckLabLabelCell> labels;

    void draw(const DrawArgs& args) override {
        Ghost::LabArt::drawLabShell(
            args.vg, box.size, "KCK LAB",
            "curated 18HP expert surface: 7 playable + 9 fit controls",
            nvgRGB(20, 18, 22));

        nvgFontSize(args.vg, 4.4f);
        nvgFillColor(args.vg, nvgRGBA(220, 220, 240, 200));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (const auto& l : labels) {
            nvgText(args.vg, mm2px(l.xMm), mm2px(l.yMm), l.text, nullptr);
        }
    }
};

struct KckLabWidget : rack::ModuleWidget {
    void addLabeledKnob(rack::engine::Module* module, int paramId,
                        float xMm, float yMm,
                        const char* label, KckLabPanel* panel) {
        panel->labels.push_back({ xMm, yMm - 6.5f, label });
        addParam(createParamCentered<rack::RoundSmallBlackKnob>(
            mm2px(Vec(xMm, yMm)), module, paramId));
    }

    KckLabWidget(KckLab* module) {
        setModule(module);

        auto* panel = new KckLabPanel;
        panel->box.size = AgentLayout::panelSize_18HP();
        addChild(panel);
        box.size = panel->box.size;

        AgentLayout::addScrews_18HP(this);

        const float COLS_X[4] = { 14.f, 37.f, 60.f, 83.f };
        const float ROWS_Y[4] = { 24.f, 47.f, 70.f, 93.f };

        struct Cell { int param; const char* label; };
        // Deliberately capped at 16 controls so every 909 Lab module can share
        // the same 18HP footprint. This is the curated "kick voicing" surface,
        // not a dump of every hidden constant in KckFit::Config.
        Cell cells[16] = {
            {KckLab::TUNE_PARAM,              "TUNE"},
            {KckLab::DECAY_PARAM,             "DECAY"},
            {KckLab::PITCH_PARAM,             "PITCH"},
            {KckLab::PITCH_DECAY_PARAM,       "P DEC"},
            {KckLab::CLICK_PARAM,             "CLICK"},
            {KckLab::DRIVE_PARAM,             "DRIVE"},
            {KckLab::LEVEL_PARAM,             "LEVEL"},
            {KckLab::BASE_PITCH_OFFSET_PARAM, "BASE OFF"},
            {KckLab::BASE_PITCH_SPAN_PARAM,   "BASE SPN"},
            {KckLab::AMP_DECAY_MIN_PARAM,     "DEC MIN"},
            {KckLab::AMP_DECAY_SPAN_PARAM,    "DEC SPN"},
            {KckLab::SWEEP_FAST_BASE_PARAM,   "FSW AMP"},
            {KckLab::SWEEP_FAST_RATE_PARAM,   "FSW RATE"},
            {KckLab::BODY_FUND_GAIN_PARAM,    "BODY"},
            {KckLab::CLICK_RATE_PARAM,        "CLK RT"},
            {KckLab::DRIVE_BASE_PARAM,        "DRV BAS"},
        };
        for (int i = 0; i < 16; i++) {
            int r = i / 4;
            int c = i % 4;
            addLabeledKnob(module, cells[i].param, COLS_X[c], ROWS_Y[r], cells[i].label, panel);
        }

        panel->labels.push_back({18.f, 117.5f, "TRIG"});
        panel->labels.push_back({38.f, 117.5f, "LACC"});
        panel->labels.push_back({58.f, 117.5f, "TACC"});
        panel->labels.push_back({78.f, 117.5f, "OUT"});
        addInput(createInputCentered<rack::PJ301MPort>(
            mm2px(Vec(18.f, 124.f)), module, KckLab::TRIG_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(
            mm2px(Vec(38.f, 124.f)), module, KckLab::LOCAL_ACC_INPUT));
        addInput(createInputCentered<rack::PJ301MPort>(
            mm2px(Vec(58.f, 124.f)), module, KckLab::TOTAL_ACC_INPUT));
        addOutput(createOutputCentered<rack::PJ301MPort>(
            mm2px(Vec(78.f, 124.f)), module, KckLab::OUT_OUTPUT));
    }
};

rack::Model* modelKckLab = createModel<KckLab, KckLabWidget>("KckLab");
