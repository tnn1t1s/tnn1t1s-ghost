#pragma once

/**
 * CrashRideEngine -- shared 909-style cymbal (crash + ride) ROM engine.
 *
 * The crashride_impl namespace constants (tune octaves, decay ranges, ROM
 * voice configs, accent characters) plus the cached embedded-PCM source and
 * ROM-asset accessors, shared by both the production CrashRide module
 * (CrashRide.cpp) and the CrashRideLab bench (lab/CrashRideLab.cpp).
 *
 * Each voice is a sampled 909 PCM ROM: embedded clean 909 PCM -> playback-rate
 * tune -> shortening VCA -> soft drive -> output level. The crash and ride each
 * carry their own fixed tuning constants (tune span, decay range, ROM config)
 * so the two voices keep distinct, calibrated character.
 */

#include <rack.hpp>
#include "GhostBus.hpp"
#include "GhostVoice.hpp"
#include "embedded/GhostCrashData.hpp"
#include "embedded/GhostRideData.hpp"
#include <vector>
#include <cmath>

using namespace rack;

// Named namespace (not anonymous) so the helpers don't collide with the
// per-TU anonymous-namespace helpers in Crash.cpp / Ride.cpp when those files
// share a translation unit, e.g. inside test_module_regressions.cpp.
namespace crashride_impl {

// Per-voice playable ranges. Source PCM rate is shared via
// Ghost::kEmbeddedPcmSampleRate.
static constexpr float kCrashTuneOctaves = 0.8f;
static constexpr float kCrashDecayMinSec = 0.25f;
static constexpr float kCrashDecayMaxSec = 3.80f;

static constexpr float kRideTuneOctaves  = 0.7f;
static constexpr float kRideDecayMinSec  = 0.12f;
static constexpr float kRideDecayMaxSec  = 4.80f;

/// Embedded crash PCM, decoded once and cached.
inline const std::vector<float>& crashSource() {
    static const std::vector<float> sample =
        Ghost::decodeEmbeddedF32(ghostCrash_f32, ghostCrash_f32_len);
    return sample;
}

/// Embedded ride PCM, decoded once and cached.
inline const std::vector<float>& rideSource() {
    static const std::vector<float> sample =
        Ghost::decodeEmbeddedF32(ghostRide_f32, ghostRide_f32_len);
    return sample;
}

/// Crash ROM asset (source + sample-rate config), built once and cached.
inline const Ghost::RomAsset& crashAsset() {
    static const Ghost::RomAsset asset =
        Ghost::makeRomAsset(crashSource(),
                                       Ghost::RomAssetConfig(Ghost::kEmbeddedPcmSampleRate));
    return asset;
}

/// Ride ROM asset (source + sample-rate config), built once and cached.
inline const Ghost::RomAsset& rideAsset() {
    static const Ghost::RomAsset asset =
        Ghost::makeRomAsset(rideSource(),
                                       Ghost::RomAssetConfig(Ghost::kEmbeddedPcmSampleRate));
    return asset;
}

// RomVoiceConfig fields are { sourceGain, outputGain, bitDepth }:
//   sourceGain: pre-decay gain on the PCM source (1.0 = no change).
//                Crash trims slightly to leave headroom for its longer cymbal
//                attack peak; Ride passes through.
//   outputGain: post-VCA gain on the voice output. Both pass through; final
//                level scaling lives downstream in voiceProcess() (postGain
//                argument and the user LEVEL knob).
//   bitDepth:   quantisation depth applied to the source samples. 16 = the
//                full embedded resolution; lower values are a debug hook for
//                researching ROMpler bit-crush character.
// These constants set the crash voice's ROM playback character.
static const Ghost::RomVoiceConfig kCrashRomCfg = { 0.98f, 1.00f, 16 };
static const Ghost::RomVoiceConfig kRideRomCfg  = { 1.00f, 1.00f, 16 };
// Per the 909 reference doc, RD and CY have NO accent on the original 909
// (sit at full max). The drive boosts below are stylistic Ghost
// additions, not circuit reproductions. AccentCharacter member order:
// body, pitch, click, drive, noise, snap, decay, brightness, bend.
static const Ghost::AccentCharacter kCrashAccent = Ghost::Accent::driveOnly();
static const Ghost::AccentCharacter kRideAccent  = Ghost::Accent::driveOnly();

} // namespace crashride_impl
