#pragma once

/**
 * SnrEngine -- shared 909-style snare engine.
 *
 * The SnrFit::Config voicing recipe, the SnrSVF state-variable filter, the
 * snrTriangle / snrNormWithCV helpers, and the snare constants, shared by both
 * the production Snr module (Snr.cpp) and the SnrLab fitting bench
 * (lab/SnrLab.cpp).
 *
 * Internal model:
 *   body  = two phase-reset triangle oscillators with a short pitch bend and
 *           slightly different decay times
 *   noise = binary-noise generator split into low/high branches with separate
 *           envelopes; Tone sets the noise duration, Snappy sets the gain
 *   mix   = body + low-noise + high-noise + short attack click
 *
 * Rack IDs (stable for the current 909 module generation):
 *   Params:  TUNE=0, TONE=1, SNAPPY=2, LEVEL=3
 *   Inputs:  TRIG=0, TUNE_CV=1, TONE_CV=2, SNAPPY_CV=3, LEVEL_CV=4
 *   Outputs: OUT=0
 */

#include <rack.hpp>
#include "GhostBus.hpp"
#include "GhostVoice.hpp"
#include <cmath>
#include <cstdint>

using namespace rack;

static constexpr float kSnrTuneOctRange = 0.75f;
static constexpr float kSnrToneMinSec   = 0.018f;
static constexpr float kSnrNoiseHighQ   = 0.78f;
static constexpr float kSnrNoiseLowQ    = 0.82f;
static constexpr float kSnrCvScale      = 0.1f;

/// Read a 0..1 param and fold in its CV input (scaled by kSnrCvScale), clamped
/// to [0, 1]. Used for the four front-panel controls.
static inline float snrNormWithCV(rack::Module& self, int paramId, int inputId) {
    float norm = self.params[paramId].getValue()
               + self.inputs[inputId].getVoltage() * kSnrCvScale;
    return rack::math::clamp(norm, 0.f, 1.f);
}

/// Unipolar-phase (0..1) triangle wave in [-1, 1], peaking at phase 0.5.
static inline float snrTriangle(float phase) {
    return 1.f - 4.f * std::fabs(phase - 0.5f);
}

/// Zero-delay-feedback (TPT) state-variable filter; exposes simultaneous low-
/// and high-pass outputs (lpf, hpf) for the snare-noise branches.
struct SnrSVF {
    float ic1 = 0.f, ic2 = 0.f;
    float lpf = 0.f;
    float hpf = 0.f;
    void reset() { ic1 = ic2 = lpf = hpf = 0.f; }
    /// Advance the filter by one sample. x is input; fHz the cutoff in Hz;
    /// sampleRate in Hz; Q the resonance. Updates lpf/hpf members in place.
    void process(float x, float fHz, float sampleRate, float Q) {
        float g = std::tan(float(M_PI) * fHz / sampleRate);
        float k = 1.f / Q;
        float a1 = 1.f / (1.f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;
        float v3 = x - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        if (std::abs(ic1) < Ghost::kDenormalFloor) ic1 = 0.f;   // denormal safety
        if (std::abs(ic2) < Ghost::kDenormalFloor) ic2 = 0.f;   // denormal safety
        lpf = v2;
        hpf = x - k * v1 - v2;
    }
};

namespace SnrFit {
/// Fitted internal voicing constants for the snare (oscillator base
/// frequencies in Hz, decay time-constants in seconds, gains, and accent
/// policy). Production Snr uses the defaults; SnrLab overrides a curated subset.
struct Config {
    float osc1BaseHz = 157.655031f;
    float osc2BaseHz = 332.641481f;
    float body1TauSec = 0.025189178f;
    float body2TauSec = 0.020462034f;
    float bodyLpHz = 1273.814504f;
    float toneMaxSec = 0.106f;
    float noiseHighRatio = 0.713894547f;
    float noiseClockHz = 14285.683594f;
    float noiseLpHz = 9278.987305f;
    float noiseHpHz = 3907.570801f;
    float bendMaxOct = 0.46f;
    float bendTauSec = 0.020f;
    float attackTauSec = 0.000685407f;
    float clickTauSec = 0.0015f;
    float body1Gain = 1.053525281f;
    float body2Gain = 0.196282045f;
    float bodyDrive = 1.028451443f;
    float lowNoiseGain = 0.074649002f;
    float lowNoiseToneBase = 0.1f;
    float lowNoiseToneSpan = 0.9f;
    float snappyShapePower = 1.5f;
    float lowNoiseSnappyGainDelta = 0.f;
    float lowNoiseSnappyTauMinScale = 0.45f;
    float lowNoiseSnappyTauDelta = 4.f;
    float highNoiseBase = 0.01f;
    float highNoiseSnappy = 0.42f;
    float highNoiseToneBase = 1.f;
    float highNoiseToneSpan = 0.f;
    float highNoiseSnappyTauDelta = 0.f;
    float clickBodyGain = 0.171073779f;
    float clickNoiseGain = 0.075186349f;
    float mixDriveBase = 0.960915566f;
    float mixDriveSnappy = 0.058808930f;
    float outputGain = 0.86f;
    float osc2BendRatio = 0.751340272f;
    // Per-DSP-stage CHARACTER weights, applied via accentScale / accentAdd
    // when charStrength > 0. Snr is the only voice with circuit-faithful
    // timbral accent on the original 909 (gates noise and tone VCAs);
    // noiseAmt is the audible signature. AccentCharacter member order is
    // body, pitch, click, drive, noise, snap, decay, brightness, bend.
    Ghost::AccentCharacter accent = Ghost::Accent::snare();  // shared policy
};

/// Shared immutable default fit config (constructed once on first use).
static inline const Config& defaults() {
    static const Config cfg;
    return cfg;
}
}  // namespace SnrFit

namespace {

/// One-shot 909-style snare voice: two phase-reset triangle bodies + split
/// low/high binary-noise branches + a transient click, soft-clipped and
/// level-scaled. The per-sample synthesis extracted from Snr.cpp so the
/// production module and the headless stress harness drive the same code
/// (mirrors KckVoice / TomVoice). Per-hit accent CHARACTER is latched at fire();
/// the per-case output gain + bus master are applied by the module post-voice.
struct SnrVoice {
    dsp::SchmittTrigger trigger;

    float phase1   = 0.f;
    float phase2   = 0.f;
    float bodyEnv1 = 0.f;
    float bodyEnv2 = 0.f;
    float noiseLowEnv = 0.f;
    float noiseHighEnv = 0.f;
    float bendEnv  = 0.f;
    float attackEnv = 1.f;
    float clickEnv = 0.f;
    float bodyLP = 0.f;

    float noisePhase = 0.f;
    uint32_t noiseShift = 0x1u;
    float noiseValue = 1.f;
    float prevBody = 0.f;
    float prevNoise = 0.f;

    SnrSVF lpNoise;
    SnrSVF hpNoise;

    // Latched at fire(): accent CHARACTER strength (0..1), held for the hit.
    float latchedCharStrength = 0.f;

    /// Retrigger: reset envelopes/phases/filters to the hit start and latch the
    /// accent character strength (clamped 0..1). The free-running binary-noise
    /// LFSR (noiseShift/noiseValue/noisePhase) intentionally persists across
    /// hits, exactly as the original module did.
    void fire(float charStrength) {
        bodyEnv1 = 1.f;
        bodyEnv2 = 1.f;
        noiseLowEnv = 1.f;
        noiseHighEnv = 1.f;
        bendEnv  = 1.f;
        attackEnv = 0.f;
        clickEnv = 1.f;
        phase1   = 0.f;
        phase2   = 0.f;
        bodyLP = 0.f;
        noisePhase = 0.f;
        prevBody = 0.f;
        prevNoise = noiseValue;
        lpNoise.reset();
        hpNoise.reset();
        latchedCharStrength = rack::math::clamp(charStrength, 0.f, 1.f);
    }
    void fire() { fire(0.f); }

    /// Render one sample. Takes the engine config and the four normalized
    /// (0..1) control values; returns the snare output in audio-normalized
    /// units (pre case-gain/master). Math/state/order are a byte-for-byte
    /// move of the original Snr::process body.
    float process(const rack::Module::ProcessArgs& args,
                  const SnrFit::Config& fit,
                  float tune_norm,
                  float tone_norm,
                  float snap_norm,
                  float level_norm) {
        float tune_oct = (tune_norm - 0.5f) * 2.f * kSnrTuneOctRange;
        float scale    = std::pow(2.f, tune_oct);
        float bendOct  = bendEnv * fit.bendMaxOct;
        float f1       = fit.osc1BaseHz * scale * std::pow(2.f, bendOct);
        float f2       = fit.osc2BaseHz * scale * std::pow(2.f, bendOct * fit.osc2BendRatio);
        float toneTau  = kSnrToneMinSec + tone_norm * (fit.toneMaxSec - kSnrToneMinSec);
        float snapShape = std::pow(1.f - snap_norm, fit.snappyShapePower);
        float noiseLowTau = toneTau
                          * (fit.lowNoiseSnappyTauMinScale + snapShape * fit.lowNoiseSnappyTauDelta);
        float noiseHighTau = toneTau * fit.noiseHighRatio
                           * (1.f + snapShape * fit.highNoiseSnappyTauDelta);

        // --- body: two phase-reset triangles with different decays ------
        phase1 += f1 * args.sampleTime;
        phase2 += f2 * args.sampleTime;
        phase1 -= std::floor(phase1);
        phase2 -= std::floor(phase2);

        float tri1 = snrTriangle(phase1);
        float tri2 = snrTriangle(phase2);
        float bodyRaw = tri1 * bodyEnv1 * fit.body1Gain + tri2 * bodyEnv2 * fit.body2Gain;
        float bodyLpAlpha = 1.f - std::exp(-2.f * float(M_PI) * fit.bodyLpHz * args.sampleTime);
        bodyLP += (bodyRaw - bodyLP) * bodyLpAlpha;
        if (std::abs(bodyLP) < Ghost::kDenormalFloor) bodyLP = 0.f;   // denormal safety
        float body = std::tanh(bodyLP * fit.bodyDrive);

        // --- noise: fixed binary source, split into low/high branches ----
        noisePhase += fit.noiseClockHz * args.sampleTime;
        while (noisePhase >= 1.f) {
            noisePhase -= 1.f;
            uint32_t newBit = ((noiseShift >> 0) ^ (noiseShift >> 2)
                             ^ (noiseShift >> 3) ^ (noiseShift >> 5)) & 1u;
            noiseShift = (noiseShift >> 1) | (newBit << 15);
            noiseValue = (noiseShift & 1u) ? 1.f : -1.f;
        }
        lpNoise.process(noiseValue, fit.noiseLpHz, args.sampleRate, kSnrNoiseLowQ);
        hpNoise.process(noiseValue, fit.noiseHpHz, args.sampleRate, kSnrNoiseHighQ);
        float lowNoiseGain = fit.lowNoiseGain
                           * (fit.lowNoiseToneBase + tone_norm * fit.lowNoiseToneSpan)
                           * (1.f + snapShape * fit.lowNoiseSnappyGainDelta);
        float lowNoise = lpNoise.lpf * noiseLowEnv * lowNoiseGain;
        float highNoiseGain = (fit.highNoiseBase + snap_norm * fit.highNoiseSnappy)
                            * (fit.highNoiseToneBase + tone_norm * fit.highNoiseToneSpan);
        highNoiseGain = Ghost::accentScale(
            highNoiseGain, latchedCharStrength, fit.accent.noiseAmt);
        float highNoise = hpNoise.hpf * noiseHighEnv * highNoiseGain;
        float click = ((body - prevBody) * fit.clickBodyGain + (noiseValue - prevNoise) * fit.clickNoiseGain) * clickEnv;
        prevBody = body;
        prevNoise = noiseValue;

        // --- envelope decays --------------------------------------------
        bodyEnv1 *= std::exp(-args.sampleTime / fit.body1TauSec);
        if (bodyEnv1 < Ghost::kDenormalFloor) bodyEnv1 = 0.f;   // denormal safety
        bodyEnv2 *= std::exp(-args.sampleTime / fit.body2TauSec);
        if (bodyEnv2 < Ghost::kDenormalFloor) bodyEnv2 = 0.f;   // denormal safety
        noiseLowEnv *= std::exp(-args.sampleTime / noiseLowTau);
        if (noiseLowEnv < Ghost::kDenormalFloor) noiseLowEnv = 0.f;   // denormal safety
        noiseHighEnv *= std::exp(-args.sampleTime / noiseHighTau);
        if (noiseHighEnv < Ghost::kDenormalFloor) noiseHighEnv = 0.f;   // denormal safety
        bendEnv *= std::exp(-args.sampleTime / fit.bendTauSec);
        if (bendEnv < Ghost::kDenormalFloor) bendEnv = 0.f;   // denormal safety
        attackEnv += (1.f - attackEnv) * (1.f - std::exp(-args.sampleTime / fit.attackTauSec));
        clickEnv *= std::exp(-args.sampleTime / fit.clickTauSec);
        if (clickEnv < Ghost::kDenormalFloor) clickEnv = 0.f;   // denormal safety

        float mix = body + lowNoise + highNoise + click;
        mix = std::tanh(mix * Ghost::accentAdd(
            fit.mixDriveBase + fit.mixDriveSnappy * snap_norm,
            latchedCharStrength, fit.accent.driveAmt));
        mix *= attackEnv;

        float out = mix * level_norm * fit.outputGain;
        return out;
    }
};

}  // namespace
