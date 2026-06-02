#pragma once

/**
 * RimClapEngine -- shared ROM-sample engine for the rim-shot + hand-clap voice.
 *
 * The file-local RomVoice ROM-player struct, the kRimClapAccent character
 * constant, and the embedded clap/rim sample-source accessors, shared by both
 * the production RimClap module (RimClap.cpp) and the RimClapLab bench
 * (lab/RimClapLab.cpp).
 */

#include <rack.hpp>
#include "GhostBus.hpp"
#include "GhostVoice.hpp"
#include "embedded/GhostClapData.hpp"
#include "embedded/GhostRimData.hpp"
#include <vector>
#include <cmath>

using namespace rack;

namespace {
// Per the 909 reference doc, RS and CP have NO accent on the original 909
// (sit at full max). The drive boost below is a stylistic Ghost
// addition, not a circuit reproduction. AccentCharacter member order:
// body, pitch, click, drive, noise, snap, decay, brightness, bend.
static const Ghost::AccentCharacter kRimClapAccent = Ghost::Accent::driveOnly();

/// Decoded embedded clap sample (lazily decoded once, shared by all instances).
static const std::vector<float>& rimClapClapSource() {
    static const std::vector<float> sample =
        Ghost::decodeEmbeddedF32(ghostClap_f32, ghostClap_f32_len);
    return sample;
}

/// Decoded embedded rim sample (lazily decoded once, shared by all instances).
static const std::vector<float>& rimClapRimSource() {
    static const std::vector<float> sample =
        Ghost::decodeEmbeddedF32(ghostRim_f32, ghostRim_f32_len);
    return sample;
}

/// One-shot ROM sample player: rewinds on trigger, then advances a read head
/// through a PCM sample with playback-rate resampling and optional bit reduction.
struct RomVoice {
    float pos = 1e9f;

    void trigger() {
        pos = 0.f;
    }

    /// Advance the read head one frame and return the next output sample.
    /// `sample` is the source PCM; `sampleRate` is the engine rate (Hz);
    /// `playbackRate` scales pitch/speed (1 = original); `bitDepth` sets
    /// quantization (16 = transparent). Returns 0 once the sample is exhausted.
    float process(const std::vector<float>& sample, float sampleRate,
                  float playbackRate = 1.f, int bitDepth = 16) {
        if (pos >= float(sample.size() - 1)) {
            return 0.f;
        }
        float out = Ghost::sampleAt(sample, pos);
        pos += Ghost::playbackStep(Ghost::kEmbeddedPcmSampleRate, sampleRate, playbackRate);
        return Ghost::bitReduce(out, bitDepth);
    }
};
}
