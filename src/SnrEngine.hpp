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
