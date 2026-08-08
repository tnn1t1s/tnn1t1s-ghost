#pragma once

#include <rack.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace Ghost {

/**
 * Shared helpers for the Ghost family.
 *
 * The sample-based voices in this plugin embed clean PCM captures directly in
 * C++ headers. Each module treats that payload as its "ROM" source, then
 * re-applies the 909-style shaping stage in code: sample-rate tuning, analog
 * envelope control, and light filtering / drive.
 *
 * This keeps each module self-contained:
 *   - no external docs are required to understand the architecture
 *   - no runtime file loading is required for the cymbal family
 *   - the voice comments stay next to the DSP that implements them
 */

static constexpr float kCvScale = 0.1f;

// All embedded 909 PCM captures in this suite were rendered at 44.1 kHz.
// Every rompler voice (Crash, Ride, Chh, Ohh, RimClap, CrashRide) uses this
// constant when constructing a RomAssetConfig so there is one source of truth
// for the source rate of the bundled samples.
static constexpr float kEmbeddedPcmSampleRate = 44100.f;

// Floor for decaying state. Once an envelope or filter tail drops below this it
// is snapped to zero, so idle voices never run subnormal (denormal) math --
// which is 10-100x slower and causes CPU spikes / crackle. Do NOT rely on CPU
// flush-to-zero; Rack doesn't guarantee it for plugins.
static constexpr float kDenormalFloor = 1e-20f;

// Smallest decay time constant, to keep the exp() argument finite (no ÷0).
static constexpr float kMinDecaySec = 1e-4f;

// drive(): tanh saturation. kDriveSlope sets how hard the knob pushes into
// tanh; kDriveEpsilon skips the cost (and slight gain loss) when drive is ~0.
static constexpr float kDriveSlope   = 4.5f;
static constexpr float kDriveEpsilon = 1e-5f;

inline float normWithCV(rack::Module& self, int paramId, int inputId) {
    float norm = self.params[paramId].getValue()
               + self.inputs[inputId].getVoltage() * kCvScale;
    return rack::math::clamp(norm, 0.f, 1.f);
}

inline std::vector<float> decodeEmbeddedF32(const unsigned char* bytes, size_t byteCount) {
    size_t frames = byteCount / sizeof(float);
    std::vector<float> out(frames);
    std::memcpy(out.data(), bytes, frames * sizeof(float));
    return out;
}

// Taper applied to the last frames of a ROM, in source frames (~2.9 ms at
// 44.1 kHz). Past the end sampleAt returns silence, so a ROM that does not
// itself end at zero would step -- a click. The hi-hat ROM is truncated, not
// faded, ending on -0.0033. Audibility is governed by TUNE rather than decay:
// raising TUNE brings the read head to the end sooner, with less of the
// envelope decayed. At maximum TUNE and decay that reached -40 dBFS. Fading in
// the read path rather than re-cutting the ROM keeps every sampled voice, and
// any future ROM, safe.
static constexpr float kRomEndFadeFrames = 128.f;

/// Read `data` at a fractional position with 4-point cubic Hermite
/// (Catmull-Rom) interpolation, tapering the final frames to silence.
///
/// The ROMs are 44.1 kHz and the host usually is not, so the read head almost
/// never lands on a sample boundary and the interpolator runs on every sample.
/// Linear interpolation rejects the source grid's images too weakly for that:
/// at 44.1k -> 48k it left an 8 kHz tone's image only 25.9 dB down at 11.9 kHz,
/// audible as grit on bright dense material like the hats, whose energy sits
/// right where the rejection is worst. Cubic also flattens the passband droop
/// that linear imposed (-3.1 dB at 16 kHz), which the voices' air shelves were
/// partly boosting back.
///
/// Positions outside the sample return silence, as before. Neighbours are
/// clamped at the ends, so the first and last frames degrade to a lower-order
/// fit rather than reading out of bounds.
inline float sampleAt(const std::vector<float>& data, float pos) {
    if (data.empty() || pos < 0.f || pos >= float(data.size() - 1))
        return 0.f;
    const int last = int(data.size()) - 1;
    const int i0 = int(pos);
    const float t = pos - float(i0);

    const float xm1 = data[i0 > 0 ? i0 - 1 : 0];
    const float x0  = data[i0];
    const float x1  = data[std::min(i0 + 1, last)];
    const float x2  = data[std::min(i0 + 2, last)];

    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    float out = ((c3 * t + c2) * t + c1) * t + x0;

    // Smoothstep the last frames to zero so the read head lands on silence.
    const float remaining = float(last) - pos;
    if (remaining < kRomEndFadeFrames) {
        const float x = remaining / kRomEndFadeFrames;   // 1 -> 0 across the fade
        out *= x * x * (3.f - 2.f * x);
    }
    return out;
}

inline float playbackStep(float sourceSampleRate, float hostSampleRate, float playbackRate) {
    return (sourceSampleRate / hostSampleRate) * playbackRate;
}

inline float clampFilterHz(float hz, float sampleRate) {
    return std::min(hz, sampleRate * 0.45f);
}

/// tanh soft-clip drive, gain-compensated so loudness stays ~constant as the
/// drive knob (driveNorm, 0..1) opens. Returns x unchanged when drive is ~0.
inline float drive(float x, float driveNorm) {
    if (driveNorm <= kDriveEpsilon)
        return x;
    float g = 1.f + driveNorm * kDriveSlope;
    return std::tanh(x * g) / std::sqrt(g);
}

inline float driveWithAccent(float x, float baseDriveNorm,
                             float charStrength, float accentDriveAmt) {
    return drive(x, baseDriveNorm + charStrength * accentDriveAmt);
}

/// Geometric (exponential) knob->time map: each equal knob move multiplies the
/// decay by a constant ratio, which is how decay length is perceived. Linear-in-
/// time crams the audible change into the bottom of the knob; this spreads it
/// evenly across the full sweep. Used by the sample voices (hats, cymbals); the
/// synth voices keep their calibrated 909 rate maps. See issue #19.
inline float expDecaySec(float norm, float minSec, float maxSec) {
    norm = norm < 0.f ? 0.f : (norm > 1.f ? 1.f : norm);
    return minSec * std::pow(maxSec / minSec, norm);
}

/// First-order high-shelf "air" lift. Adds a scaled copy of the content above
/// cutoffHz back into the signal (out = x + amount * highpassed(x)), brightening
/// without thinning the body. amount 0 = bypass; ~1.0 is roughly a +6 dB shelf.
/// One float of state. Models the 909 hat's high-pass-emphasized cymbal voicing
/// (the hardware shapes its 6-bit sample through series filters). See issue #20.
struct AirShelf {
    float lp = 0.f;
    void reset() { lp = 0.f; }
    float process(float x, float amount, float cutoffHz, float sampleRate) {
        if (amount <= 0.f) return x;
        float a = 1.f - std::exp(-2.f * float(M_PI) * cutoffHz / sampleRate);
        lp += a * (x - lp);
        if (std::abs(lp) < kDenormalFloor) lp = 0.f;   // denormal safety
        return x + amount * (x - lp);   // x + amount * highpass
    }
};

inline float bitReduce(float x, int bits) {
    if (bits >= 16) {
        return x;
    }
    bits = std::max(1, std::min(16, bits));
    float clamped = rack::math::clamp(x, -1.f, 1.f);
    const int levels = (1 << bits) - 1;
    const float scaled = (clamped + 1.f) * 0.5f;
    const float quantized = std::round(scaled * float(levels)) / float(levels);
    return quantized * 2.f - 1.f;
}

struct RomAssetConfig {
    float sourceSampleRate = 44100.f;

    RomAssetConfig() {}
    explicit RomAssetConfig(float sourceSampleRate)
        : sourceSampleRate(sourceSampleRate) {}
};

struct RomAsset {
    float sourceSampleRate = 44100.f;
    std::vector<float> source;
};

inline RomAsset makeRomAsset(const std::vector<float>& source,
                             const RomAssetConfig& cfg) {
    RomAsset asset;
    asset.sourceSampleRate = cfg.sourceSampleRate;
    asset.source = source;
    return asset;
}

struct RomVoiceConfig {
    float sourceGain = 1.f;
    float outputGain = 1.f;
    int bitDepth = 16;

    RomVoiceConfig() {}
    RomVoiceConfig(float sourceGain,
                   float outputGain,
                   int bitDepth = 16)
        : sourceGain(sourceGain),
          outputGain(outputGain),
          bitDepth(bitDepth) {}
};

/// One ROM-sample playback voice: a read head into embedded PCM plus an
/// exponential amplitude envelope. Shared by every sample-based voice
/// (hats, cymbals, rim, clap). Call trigger() on a hit, process() per sample.
struct RomVoice {
    float sourcePos = 1e9f;   // past end of buffer = silent until first trigger
    float env = 0.f;

    /// Restart playback from the head with a full-level envelope.
    void trigger() {
        sourcePos = 0.f;
        env = 1.f;
    }

    /// Advance one sample: read the ROM at sourcePos, apply source/output gains
    /// and the exp decay envelope (decaySec, seconds), bit-reduce, return the
    /// sample. The envelope is floored to zero once inaudible (denormal safety).
    float process(const rack::Module::ProcessArgs& args,
                  const RomAsset& asset,
                  float playbackRate,
                  float decaySec,
                  float decayNorm,
                  const RomVoiceConfig& cfg) {
        if (asset.source.empty()) {
            return 0.f;
        }

        const float step = playbackStep(asset.sourceSampleRate, args.sampleRate, playbackRate);
        float source = sampleAt(asset.source, sourcePos);
        sourcePos += step;
        float out = source * cfg.sourceGain * cfg.outputGain;

        env *= std::exp(-args.sampleTime / std::max(kMinDecaySec, decaySec));
        if (env < kDenormalFloor) env = 0.f;   // denormal safety
        out *= env;
        return bitReduce(out, cfg.bitDepth);
    }
};

/// Topology-preserving (Zavalishin) state-variable filter. One process() call
/// per sample updates the two integrator states and exposes simultaneous
/// low/band/high-pass outputs (lpf/bpf/hpf). Call reset() to clear state.
struct TptSVF {
    float ic1 = 0.f;   // integrator 1 state (the feedback tail)
    float ic2 = 0.f;   // integrator 2 state
    float lpf = 0.f;
    float bpf = 0.f;
    float hpf = 0.f;

    /// Clear all state (silence the filter).
    void reset() {
        ic1 = ic2 = lpf = bpf = hpf = 0.f;
    }

    /// Step the filter: input x, cutoff fHz (clamped below Nyquist so tan()
    /// can't blow up to NaN and poison the state), resonance Q.
    void process(float x, float fHz, float sampleRate, float Q) {
        float g = std::tan(float(M_PI) * clampFilterHz(fHz, sampleRate) / sampleRate);
        float k = 1.f / Q;
        float a1 = 1.f / (1.f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;
        float v3 = x - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        if (std::abs(ic1) < kDenormalFloor) ic1 = 0.f;   // denormal safety
        if (std::abs(ic2) < kDenormalFloor) ic2 = 0.f;
        bpf = v1;
        lpf = v2;
        hpf = x - k * v1 - v2;
    }
};

}  // namespace Ghost
