#pragma once

/**
 * TomsEngine -- shared 909-style tom engine.
 *
 * The TomFit::Config recipe plus the makeLowTom/makeMidTom/makeHighTom
 * factories and the TomVoice DSP struct, shared by both the production Toms
 * module (Toms.cpp) and the TomLab fitting bench (lab/TomLab.cpp).
 *
 * Per-sample algorithm (since trigger):
 *   tunedFreq = baseHz * (tuneOffset + tune * tuneSpan)
 *   pitchEnv  = exp(-pitchBendRate * t)
 *   freq1     = tunedFreq + pitchEnv * (pitchBendBase + baseHz * pitchBendBaseScale)
 *   freq2     = freq1 * osc2Ratio
 *   envRate   = 1 / expDecaySec(decay, decayTauMinSec, decayTauMaxSec)
 *   env       = exp(-envRate * t)
 *   click     = (sample < clickLengthSamples)
 *               ? clickGain * (1 - sample / clickLengthSamples) : 0
 *   noise     = HP(white, noiseHpHz) * exp(-noiseDecayRate * t)
 *               * accentScale(noiseGain, accent, noiseAmt)
 *   out       = (tri(p1)*osc1Gain + tri(p2)*osc2Gain + tri(p3)*osc3Gain
 *               + click) * env + noise
 *   out       = HP(out, hpCoef)
 *   out       = Ghost::driveWithAccent(out, driveGain, accentNorm, accent.driveAmt)
 *   final     = clamp(out * outputGain * level, -1, 1)
 *
 * Per-voice difference is just `baseHz`; everything else shares defaults
 * so changes during fitting affect all three toms unless overridden.
 *
 * Rack IDs (stable, never reorder):
 *   Params:  TUNE=0, DECAY=1, LEVEL=2
 *   Inputs:  TRIG=0, TUNE_CV=1, DECAY_CV=2, LEVEL_CV=3, ACCENT=4
 *   Outputs: OUT=0
 */

#include <rack.hpp>
#include "GhostBus.hpp"
#include "GhostVoice.hpp"
#include <cmath>

using namespace rack;

namespace TomFit {

/// Full set of internal tom-engine model parameters (pitch, envelope, mix,
/// noise, click, drive). One Config per voice; only baseHz differs across the
/// three toms. Exposed for the voice_lab fitting workflow.
/// Calibration story (ratios, reference LowTom tau, noise burst): see doc/calibration.md.
struct Config {
    // Pitch
    float baseHz             = 100.f;
    float tuneOffset         = 0.62f;
    float tuneSpan           = 0.88f;

    // Pitch envelope (1/tau and bend amount)
    float pitchBendRate      = 16.f;
    float pitchBendBase      = 22.f;
    float pitchBendBaseScale = 0.03f;

    // Oscillator 2 frequency ratio (1.5 = perfect fifth)
    float osc2Ratio          = 1.5f;

    // Mix. The 909 low tom is THREE damped sines at ratios 1 : 1.5 : 2.77.
    float osc1Gain           = 0.63f;
    float osc2Gain           = 0.12f;
    float osc3Ratio          = 2.77f;   // top partial (relative to freq1)
    float osc3Gain           = 0.08f;

    // Noise circuit -- the attack transient. On the 909 a trigger-gated
    // noise VCA feeds all three toms; the burst is the audible "snap"
    // (service notes p.9 scope photo shows it riding the first cycles;
    // the s/n 426700 factory change emphasizes it). Band-limited, not
    // hiss: high-passed at noiseHpHz. Calibration: doc/dev/calibration.md.
    float noiseGain          = 0.30f;
    float noiseDecayRate     = 110.f;   // noise burst 1/tau (~9 ms spike)
    float noiseHpHz          = 350.f;   // one-pole HP corner on the burst

    // Click -- extra shaping option, off by default. The 909 has no click
    // feedthrough (the VCA path is muted during trigger reset); its click
    // is the noise burst plus the coherent oscillator onset.
    float clickGain          = 0.f;
    float clickLengthSamples = 30.f;

    // Body envelope: geometric knob->tau map (Ghost::expDecaySec, the same
    // curve the sample voices use), tau = min * (max/min)^decay. DECAY 0 is
    // a tight pluck, DECAY 1 the full body -- the real DECAY pot truncates
    // the natural ring down to a pluck (~10:1 span). sqrt(min*max) keeps the
    // reference calibration anchor: tau ~ 100 ms at decay=0.5.
    float decayTauMinSec     = 0.030f;
    float decayTauMaxSec     = 0.333f;

    // Leaky DC-blocking HP coefficient (~14 Hz cutoff at 44.1 kHz).
    float hpCoef             = 0.002f;

    // Soft-clip drive: 0 disables. JUCE original was 1.2; tanh added audible
    // even-harmonic colour, so the calibrated default is off.
    float driveGain          = 0.f;

    // Output
    float outputGain         = 0.78f;
    // On the 909 the accent pulse also drives the tom-noise VCA ("slightly
    // more attack and noise" -- TipTop's licensed circuit adaptation), so
    // accent is level (AccentMix, Toms.cpp) plus a noise-burst boost here.
    Ghost::AccentCharacter accent = Ghost::Accent::toms();
};

// baseHz calibrated against the reference machine references at tune050-decay050:
//   LowTom ref  = 90.8 Hz (F#2 -31c)  -> 90.8 / 1.06 = 85.7
//   MidTom ref  = 113.7 Hz (A#2 -42c) -> 113.7 / 1.06 = 107.3
//   HighTom ref = 133.9 Hz (C3 +41c)  -> 133.9 / 1.06 = 126.3
// Mid/High share every other fit param with Low; only baseHz differs.
inline Config makeLowTom()  { Config c; c.baseHz =  85.7f; return c; }
inline Config makeMidTom()  { Config c; c.baseHz = 107.3f; return c; }
inline Config makeHighTom() { Config c; c.baseHz = 126.3f; return c; }

}  // namespace TomFit

namespace {

/// Naive triangle wave from a normalised phase in [0,1); returns [-1,1].
static inline float tomTriangle(float phase) {
    return 1.f - 4.f * std::fabs(phase - 0.5f);
}

/// One mono tom voice: holds the per-trigger DSP state (oscillator phases,
/// time-since-trigger, HP filter state, noise RNG) and renders one sample.
struct TomVoice {
    dsp::SchmittTrigger trigger;
    float phase1 = 0.f;
    float phase2 = 0.f;
    float phase3 = 0.f;
    float t = 0.f;
    int   sampleCount = 0;
    float hpState = 0.f;
    float noiseLpState = 0.f;   // one-pole LP state for the noise HP (y = x - LP(x))
    bool  active = false;
    uint32_t rngState = 1u;
    // Memo for the geometric decay map (expDecaySec calls powf); recomputed
    // only when the DECAY knob or tau bounds move, not per sample.
    float lastDecayNorm = -1.f;
    float lastTauMin = -1.f, lastTauMax = -1.f;
    float cachedEnvRate = 10.f;

    /// Start a new one-shot: reset phases, time, filter and noise seed, activate.
    void fire() {
        phase1 = phase2 = phase3 = 0.f;
        t = 0.f;
        sampleCount = 0;
        hpState = 0.f;
        noiseLpState = 0.f;
        rngState = 22699u;
        active = true;
    }

    /// One sample of white noise in [-1,1) from a fast LCG (no allocation).
    inline float nextNoise() {
        rngState = rngState * 1664525u + 1013904223u;
        return ((rngState >> 8) & 0xFFFFFFu) * (2.f / 16777216.f) - 1.f;
    }

    /// Render one sample of the active one-shot. tuneNorm/decayNorm/levelNorm
    /// are knob values in [0,1]; accentNorm is the latched accent character
    /// strength. Returns the voice output in [-1,1] (pre case-gain/master);
    /// returns 0 when inactive.
    float process(const rack::Module::ProcessArgs& args,
                  const TomFit::Config& fit,
                  float tuneNorm,
                  float decayNorm,
                  float levelNorm,
                  float accentNorm) {
        if (!active) return 0.f;

        const float tunedFreq = fit.baseHz * (fit.tuneOffset + tuneNorm * fit.tuneSpan);
        float pitchEnv  = std::exp(-fit.pitchBendRate * t);
        if (pitchEnv < Ghost::kDenormalFloor) pitchEnv = 0.f;   // denormal safety
        const float freq1     = tunedFreq
                              + pitchEnv * (fit.pitchBendBase + fit.baseHz * fit.pitchBendBaseScale);
        const float freq2     = freq1 * fit.osc2Ratio;
        const float freq3     = freq1 * fit.osc3Ratio;
        if (decayNorm != lastDecayNorm || fit.decayTauMinSec != lastTauMin
            || fit.decayTauMaxSec != lastTauMax) {
            lastDecayNorm = decayNorm;
            lastTauMin = fit.decayTauMinSec;
            lastTauMax = fit.decayTauMaxSec;
            cachedEnvRate = 1.f / Ghost::expDecaySec(
                decayNorm, fit.decayTauMinSec, fit.decayTauMaxSec);
        }
        const float envRate   = cachedEnvRate;
        float env       = std::exp(-envRate * t);
        if (env < Ghost::kDenormalFloor) env = 0.f;   // denormal safety

        const float click = (sampleCount < (int)fit.clickLengthSamples)
            ? (Ghost::accentScale(
                    fit.clickGain, accentNorm, fit.accent.clickAmt)
               * (1.f - (float)sampleCount / fit.clickLengthSamples))
            : 0.f;

        phase1 += freq1 * args.sampleTime;
        phase2 += freq2 * args.sampleTime;
        phase3 += freq3 * args.sampleTime;
        phase1 -= std::floor(phase1);
        phase2 -= std::floor(phase2);
        phase3 -= std::floor(phase3);

        // Attack noise burst (909 tom-noise VCA): band-limited spike riding
        // the first cycles. One-pole HP at noiseHpHz strips the low end so
        // the burst reads as snap on top of the body, not added rumble.
        float noiseEnv = std::exp(-fit.noiseDecayRate * t);
        if (noiseEnv < Ghost::kDenormalFloor) noiseEnv = 0.f;   // denormal safety
        const float noiseRaw = nextNoise();
        const float noiseLpCoef = 1.f - std::exp(
            -2.f * float(M_PI) * fit.noiseHpHz * args.sampleTime);
        noiseLpState += noiseLpCoef * (noiseRaw - noiseLpState);
        if (std::abs(noiseLpState) < Ghost::kDenormalFloor) noiseLpState = 0.f;
        const float noise = (noiseRaw - noiseLpState) * noiseEnv
                          * Ghost::accentScale(fit.noiseGain, accentNorm, fit.accent.noiseAmt);

        float out = ((tomTriangle(phase1) * fit.osc1Gain
                   + tomTriangle(phase2) * fit.osc2Gain
                   + tomTriangle(phase3) * fit.osc3Gain)
                   * Ghost::accentScale(1.f, accentNorm, fit.accent.bodyAmt)
                   + click) * env
                   + noise;

        // Leaky DC-blocking HP: y = x - LP(x).
        hpState += fit.hpCoef * (out - hpState);
        if (std::abs(hpState) < Ghost::kDenormalFloor) hpState = 0.f;   // denormal safety
        out -= hpState;

        out = Ghost::driveWithAccent(out, fit.driveGain, accentNorm, fit.accent.driveAmt);

        float result = rack::math::clamp(out * fit.outputGain * levelNorm, -1.f, 1.f);

        t += args.sampleTime;
        sampleCount++;
        if (env < 1e-5f && sampleCount > 1024) active = false;

        return result;
    }
};

} // namespace
