#pragma once

/**
 * ChhOhhEngine -- shared closed + open hi-hat ROM/sample engine constants.
 *
 * The anonymous-namespace tuning constants, the CH/OH AccentCharacter values,
 * and the embedded-PCM source accessors shared by both the production ChhOhh
 * module (ChhOhh.cpp) and the ChhOhhLab variant (lab/ChhOhhLab.cpp).
 */

#include <rack.hpp>
#include "GhostBus.hpp"
#include "GhostVoice.hpp"
#include "embedded/GhostChhData.hpp"
#include "embedded/GhostOhhData.hpp"
#include <vector>
#include <cmath>

using namespace rack;

namespace {

static constexpr float kChhTuneOctaves  = 1.0f;
static constexpr float kChhDecayMinSec = 0.010f;
static constexpr float kChhDecayMaxSec = 0.16f;

static constexpr float kOhhTuneOctaves  = 1.0f;
static constexpr float kOhhDecayMinSec = 0.006f;
static constexpr float kOhhDecayMaxSec = 3.20f;

static constexpr float kChokeDecaySec   = 0.005f;

// Per-DSP-stage accent character. CH and OH both have a small drive boost
// on accented hits; per the 909 reference doc, CH has level-only accent on
// the original 909 and OH has no accent (sits at full max). These are
// stylistic Ghost additions, not circuit reproductions.
// AccentCharacter member order: body, pitch, click, drive, noise, snap,
// decay, brightness, bend.
static const Ghost::AccentCharacter kChhAccent = Ghost::Accent::driveOnly();
static const Ghost::AccentCharacter kOhhAccent = Ghost::Accent::driveOnly();

/// Decode and cache the embedded closed-hat PCM sample (lazy, one-time).
static const std::vector<float>& chhSource() {
    static const std::vector<float> sample =
        Ghost::decodeEmbeddedF32(ghostChh_f32, ghostChh_f32_len);
    return sample;
}

/// Decode and cache the embedded open-hat PCM sample (lazy, one-time).
static const std::vector<float>& ohhSource() {
    static const std::vector<float> sample =
        Ghost::decodeEmbeddedF32(ghostOhh_f32, ghostOhh_f32_len);
    return sample;
}

} // namespace
