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

/// Combined closed + open hi-hat voice. Both hats share one struct so the
/// CH-mutes-OH choke is internal state (issue #78), exactly as the production
/// module modelled it. The per-sample synthesis extracted from ChhOhh.cpp so
/// the module and the headless stress harness drive the same code (mirrors
/// KckVoice / SnrVoice). Per-hit accent CHARACTER is latched at fireChh/fireOhh;
/// the per-case output gain + bus master are applied by the module post-voice.
struct ChhOhhVoice {
    dsp::SchmittTrigger chhTrigger;
    dsp::SchmittTrigger ohhTrigger;

    // CH voice state.
    float chhSamplePos = 1e9f;
    float chhEnv       = 0.f;

    // OH voice state.
    float ohhSamplePos = 1e9f;
    float ohhEnv       = 0.f;
    bool  ohhChokeActive = false;

    float chhLatchedChar = 0.f;
    float ohhLatchedChar = 0.f;

    /// Trigger the closed hat: rewind its read head, re-arm the envelope, latch
    /// accent character, and CHOKE any sounding open hat (the 909 CH->OH mute).
    void fireChh(float charStrength) {
        chhSamplePos = 0.f;
        chhEnv       = 1.f;
        chhLatchedChar = rack::math::clamp(charStrength, 0.f, 1.f);
        // CH->OH choke: any sounding open hat is muted by a CH hit.
        if (ohhEnv > 1e-4f) ohhChokeActive = true;
    }

    /// Trigger the open hat: rewind/re-arm, latch accent, and cancel any pending
    /// choke (a fresh OH hit re-arms the voice).
    void fireOhh(float charStrength) {
        ohhSamplePos     = 0.f;
        ohhEnv           = 1.f;
        ohhChokeActive   = false;  // a fresh OH hit cancels pending choke
        ohhLatchedChar = rack::math::clamp(charStrength, 0.f, 1.f);
    }

    /// Render one sample of each voice into chhOut / ohhOut (audio-normalized,
    /// pre case-gain/master). Math/state/order are a byte-for-byte move of the
    /// original ChhOhh::process body, including the choke env-rate override.
    void process(const rack::Module::ProcessArgs& args,
                 float chhTune, float chhDecay, float chhDrive, float chhLevel,
                 float ohhTune, float ohhDecay, float ohhDrive, float ohhLevel,
                 float& chhOut, float& ohhOut) {
        // -- CH DSP ----------------------------------------------------
        float chhRate    = std::pow(2.f, (chhTune - 0.5f) * 2.f * kChhTuneOctaves);
        float chhDecayShape = std::sqrt(chhDecay);
        float chhDecaySec = kChhDecayMinSec
                          + chhDecayShape * (kChhDecayMaxSec - kChhDecayMinSec);
        const auto& chSrc = chhSource();
        float chhSrc = Ghost::sampleAt(chSrc, chhSamplePos);
        chhSamplePos += Ghost::playbackStep(
            Ghost::kEmbeddedPcmSampleRate, args.sampleRate, chhRate);
        chhEnv *= std::exp(-args.sampleTime / chhDecaySec);
        if (chhEnv < Ghost::kDenormalFloor) chhEnv = 0.f;   // denormal safety
        float chhO = chhSrc * chhEnv * 1.04f;
        chhO = Ghost::driveWithAccent(
            chhO, chhDrive, chhLatchedChar, kChhAccent.driveAmt);
        chhO *= chhLevel * 0.94f;

        // -- OH DSP (with choke override on env decay) -----------------
        float ohhRate     = std::pow(2.f, (ohhTune - 0.5f) * 2.f * kOhhTuneOctaves);
        float ohhDecaySec = kOhhDecayMinSec
                          + ohhDecay * (kOhhDecayMaxSec - kOhhDecayMinSec);
        const auto& ohSrc = ohhSource();
        float ohhSrc = Ghost::sampleAt(ohSrc, ohhSamplePos);
        ohhSamplePos += Ghost::playbackStep(
            Ghost::kEmbeddedPcmSampleRate, args.sampleRate, ohhRate);
        const float ohhEffectiveDecaySec = ohhChokeActive ? kChokeDecaySec : ohhDecaySec;
        ohhEnv *= std::exp(-args.sampleTime / ohhEffectiveDecaySec);
        if (ohhEnv < Ghost::kDenormalFloor) ohhEnv = 0.f;   // denormal safety
        if (ohhChokeActive && ohhEnv < 1e-4f) ohhChokeActive = false;
        float ohhO = ohhSrc * ohhEnv * 1.05f;
        ohhO = Ghost::driveWithAccent(
            ohhO, ohhDrive, ohhLatchedChar, kOhhAccent.driveAmt);
        ohhO *= ohhLevel * 0.96f;

        chhOut = chhO;
        ohhOut = ohhO;
    }
};

} // namespace
