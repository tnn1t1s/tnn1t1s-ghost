#pragma once

/**
 * KckEngine -- shared 909-style kick engine.
 *
 * The KckFit::Config recipe plus the KckVoice DSP struct, shared by both the
 * production Kck module (Kck.cpp) and the KckLab fitting bench (lab/KckLab.cpp).
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

#include <rack.hpp>
#include "GhostBus.hpp"
#include "GhostVoice.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

using namespace rack;

namespace KckFit {

/// All tunable model constants for the kick engine (Hz, 1/tau decay rates,
/// gains, filter coefficients, accent policy). One instance is the full voice
/// recipe; KckLab exposes a curated subset as live knobs.
/// Calibration story (the reference match, knob remapping): see doc/calibration.md.
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

    // Accent application -- two orthogonal axes, see GhostBus.hpp.
    //
    // accentMix decides the LEVEL relationship across the four cases
    // (ghost / global / local / both) in dB. Wide spread for pronounced,
    // 909-style accent dynamics + multi-level kick rolls;
    // ghost stays -6 dB so steady patterns / the -6 dBFS calibration hold.
    Ghost::AccentMix accentMix = Ghost::Accent::kickMix();

    // Per-DSP-stage CHARACTER weights, applied multiplicatively at
    // fire-time when the hit is accented (any accent gate fired).
    // Defaults below are Kck's tuned values; AccentCharacter member order
    // is body, pitch, click, drive, noise, snap, decay, brightness, bend.
    // Note: there is intentionally NO level field here. Level on / across
    // accent cases is governed by AccentMix dB; a per-voice level boost
    // here would double-count with the shared abstraction.
    // Shared gentle-accent policy (see GhostBus.hpp Accent::). Click + drive,
    // no pitch/body sweep. Tune the suite's accent feel in Accent::, not here.
    Ghost::AccentCharacter accent = Ghost::Accent::kick();
};

inline Config makeKick() { return Config{}; }

}  // namespace KckFit

namespace {

static constexpr float kTwoPi   = 6.28318530717958647692f;
static constexpr float kCvScale = 0.1f;

/// Read a normalized (0..1) param value plus its CV input (0.1 per volt),
/// clamped to [0, 1]. Used for the playable knobs on production Kck.
static inline float kckNormWithCV(rack::Module& self, int paramId, int inputId) {
    float norm = self.params[paramId].getValue()
               + self.inputs[inputId].getVoltage() * kCvScale;
    return rack::math::clamp(norm, 0.f, 1.f);
}

/// One-shot 909-style kick voice: pitch-swept saturated-triangle body through a
/// resonant SVF, plus a click transient, HP-blocked and saturated. Stateless
/// config arrives per-call via KckFit::Config; per-hit accent is latched at fire().
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

    /// Retrigger the voice: reset phase/filter/envelope state to the hit start
    /// and latch the accent strength (clamped 0..1) for the whole hit.
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

    /// Next white-noise sample in [-1, 1) from a per-voice LCG (deterministic).
    inline float nextNoise() {
        // Numerical Recipes LCG; map upper bits to [-1, 1).
        rngState = rngState * 1664525u + 1013904223u;
        return ((rngState >> 8) & 0xFFFFFFu) * (2.f / 16777216.f) - 1.f;
    }

    /// Render one sample. Takes the engine config and the seven normalized
    /// (0..1) control values; returns the kick output in audio-normalized units
    /// (pre master/accent gain). Returns 0 while the voice is inactive.
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

        phase += kTwoPi * freq * args.sampleTime;
        if (phase > kTwoPi) phase -= kTwoPi;

        // Body: saturated triangle (the 909 "dirt") into a RESONANT 2-pole filter
        // whose cutoff tracks the swept pitch -- the resonance "zaps" as the
        // pitch sweeps, which is the bridged-T character missing before.
        const float pn   = phase * (1.f / kTwoPi);             // 0..1
        const float tri  = 4.f * std::fabs(pn - 0.5f) - 1.f;    // -1..1 triangle
        const float clip = fit.bodyClip + acc * fit.accent.driveAmt;  // DRIVE baked into bodyClip (Kck.cpp)
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
        if (std::abs(svfLp) < Ghost::kDenormalFloor) svfLp = 0.f;   // denormal safety
        if (std::abs(svfBp) < Ghost::kDenormalFloor) svfBp = 0.f;   // denormal safety

        const float ampEnv  = std::exp(-(fit.ampDecayMin - decayNorm * fit.ampDecaySpan) * t);
        const float bodyOut = svfLp * fit.bodyFundGain * ampEnv
                            * (1.f + acc * fit.accent.bodyAmt);

        // Click: short pulse + low-passed noise burst (the attack transient).
        // attackNorm is the CLICK knob; ATTACK macro feeds clickRateBase.
        const float clickEnv = std::exp(-fit.clickRateBase * t);  // rate = ATTACK only (decoupled from CLICK level)
        clickLp += 0.5f * (nextNoise() - clickLp);              // ~one-pole LP noise (~4 kHz)
        const float pulse = (t < 0.002f) ? 1.f : 0.f;           // ~2 ms onset pulse
        const float click =
            (clickLp * (fit.clickNoiseBase + attackNorm * fit.clickNoiseAttack)
             + pulse * fit.clickPulseGain * attackNorm)
            * clickEnv * (1.f + acc * fit.accent.clickAmt);

        float out = bodyOut + click;

        // Leaky DC-blocking HP.
        hpState += fit.hpCoef * (out - hpState);
        if (std::abs(hpState) < Ghost::kDenormalFloor) hpState = 0.f;   // denormal safety
        out -= hpState;

        // DRIVE knob (driveNorm, -1..1) scales the whole-kick saturation around
        // the matched driveBase: noon -> x1 (exact), CW crushes, CCW cleans.
        out = std::tanh(out * (fit.driveBase * std::pow(2.6f, driveNorm) + acc * fit.accent.driveAmt));

        const float levelGain = fit.outputGain * levelNorm;
        out = rack::math::clamp(out, -1.f, 1.f) * levelGain;

        t += args.sampleTime;
        if (ampEnv < 1e-5f && t > 0.5f) active = false;

        return out;
    }
};

} // namespace
