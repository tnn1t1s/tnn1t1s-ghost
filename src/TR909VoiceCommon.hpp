#pragma once

#include <rack.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace Ghost {
namespace TR909 {

/**
 * Shared helpers for the 909 family.
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

inline float sampleAt(const std::vector<float>& data, float pos) {
    if (data.empty() || pos < 0.f || pos >= float(data.size() - 1))
        return 0.f;
    int i0 = int(pos);
    int i1 = std::min(i0 + 1, int(data.size() - 1));
    float frac = pos - float(i0);
    return data[i0] + (data[i1] - data[i0]) * frac;
}

inline float playbackStep(float sourceSampleRate, float hostSampleRate, float playbackRate) {
    return (sourceSampleRate / hostSampleRate) * playbackRate;
}

inline float clampFilterHz(float hz, float sampleRate) {
    return std::min(hz, sampleRate * 0.45f);
}

inline float drive(float x, float driveNorm) {
    if (driveNorm <= 1e-5f)
        return x;
    float g = 1.f + driveNorm * 4.5f;
    return std::tanh(x * g) / std::sqrt(g);
}

inline float driveWithAccent(float x, float baseDriveNorm,
                             float charStrength, float accentDriveAmt) {
    return drive(x, baseDriveNorm + charStrength * accentDriveAmt);
}

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

struct RomVoice {
    float sourcePos = 1e9f;
    float env = 0.f;

    void trigger() {
        sourcePos = 0.f;
        env = 1.f;
    }

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

        env *= std::exp(-args.sampleTime / std::max(1e-4f, decaySec));
        out *= env;
        return bitReduce(out, cfg.bitDepth);
    }
};

struct TptSVF {
    float ic1 = 0.f;
    float ic2 = 0.f;
    float lpf = 0.f;
    float bpf = 0.f;
    float hpf = 0.f;

    void reset() {
        ic1 = ic2 = lpf = bpf = hpf = 0.f;
    }

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
        bpf = v1;
        lpf = v2;
        hpf = x - k * v1 - v2;
    }
};

}  // namespace TR909
}  // namespace Ghost
