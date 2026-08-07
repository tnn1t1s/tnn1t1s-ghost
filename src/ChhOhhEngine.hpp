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

// Ramp applied to the output jack the control line just left. Long enough to
// remove the step, short enough to read as part of the new hit's transient.
static constexpr float kDeclickSec      = 0.002f;

// Mix makeup (post-drive, level-only): the hat PCM is normalized ~0.6 peak,
// ~6 dB under the kick/snare/clap. Bring the hats to ~0.8 default with headroom
// to ~1.0 via the LEVEL knob, so they balance in GHOST MIX. Applied after drive,
// so the timbre is unchanged.
// The closed path is +5.3 dB over the open one because both now read the same
// ROM: under the closed decay only the first few tens of ms sound, which carry
// that much less energy than the open hat's full tail. Measured as RMS of the
// ROM under each path's default envelope. Applied post-drive, so this is level
// only -- it does not change the saturation the drive control produces.
static constexpr float kChhMixGain = 4.60f;   // mix makeup (post-drive); headroom to push hats past the cymbals
static constexpr float kOhhMixGain = 2.50f;   // "

// 909-style "air": a high-pass-emphasized HF lift inside the hat voice so it
// cuts the kick/snare/clap midrange instead of being masked by it. Our open-hat
// sample peaks at ~4 kHz (mid-heavy); the real 909 shapes its sample through
// series filters with high-pass/bandpass emphasis. Applied post-sample/pre-drive.
// Tunable by ear; see issue #20.
static constexpr float kOhhAirHz = 8000.f;
static constexpr float kOhhAir   = 1.20f;   // ~+7 dB shelf above 8 kHz on the open hat
static constexpr float kChhAirHz = 9000.f;
static constexpr float kChhAir   = 0.40f;   // closed hat already cuts; light touch

// Per-DSP-stage accent character. CH and OH both have a small drive boost
// on accented hits; per the 909 reference doc, CH has level-only accent on
// the original 909 and OH has no accent (sits at full max). These are
// stylistic Ghost additions, not circuit reproductions.
// AccentCharacter member order: body, pitch, click, drive, noise, snap,
// decay, brightness, bend.
static const Ghost::AccentCharacter kChhAccent = Ghost::Accent::driveOnly();
static const Ghost::AccentCharacter kOhhAccent = Ghost::Accent::driveOnly();

/// Decode and cache the hi-hat PCM ROM (lazy, one-time). One sample serves both
/// hats: closed is this ROM under the fast decay, exactly as the hardware makes
/// it. There is no separate closed-hat recording.
static const std::vector<float>& hatSource() {
    static const std::vector<float> sample =
        Ghost::decodeEmbeddedF32(ghostOhh_f32, ghostOhh_f32_len);
    return sample;
}

/// The hi-hat voice: ONE PCM playback system, as on the hardware. There is a
/// single sample ROM, a single address counter that any hi-hat trigger resets, a
/// single decay envelope, and a CLOSED/OPEN control line that selects which
/// voicing path the playback runs through (rate, decay range, air, accent rail,
/// mix gain).
///
/// Everything the two hats do to each other falls out of that structure instead
/// of being enforced by rules. Closed and open cannot sound together, because
/// there is only one voice. A closed hat over a ringing open hat replaces it with
/// the closed-hat sound -- the canonical choke -- because the trigger resets the
/// shared address counter and flips the control line. Two triggers on the same
/// step resolve to whichever state the control line takes, which the module
/// drives OPEN.
///
/// The per-sample synthesis lives here so the module and the headless stress
/// harness drive the same code (mirrors KckVoice / SnrVoice). Per-hit accent
/// CHARACTER is latched at fire(); the per-case output gain + bus master are
/// applied by the module post-voice.
struct ChhOhhVoice {
    dsp::SchmittTrigger chhTrigger;
    dsp::SchmittTrigger ohhTrigger;

    // The one playback system.
    float samplePos   = 1e9f;   // shared address counter
    float env         = 0.f;    // shared decay envelope
    bool  open        = false;  // the CLOSED/OPEN control line
    float latchedChar = 0.f;

    // 909-style HF "air" shelf (see issue #20); coefficients are picked by the
    // control line, so one filter serves both paths.
    Ghost::AirShelf air;

    // Declick for GHOST's second output jack. The hardware has one hi-hat output;
    // GHOST splits closed and open onto their own jacks, so when the control line
    // flips mid-tail the jack being vacated has to reach silence without a step.
    // Its last value is held and ramped out over kDeclickSec.
    float lastOut      = 0.f;
    float declickVal   = 0.f;
    float declickGain  = 0.f;
    bool  declickOpen  = false;   // which jack is ramping

    /// Trigger the hi-hat. `openMode` is the CLOSED/OPEN control line: it resets
    /// the shared address counter and envelope and selects the voicing path.
    void fire(bool openMode, float charStrength) {
        if (openMode != open) {
            if (env > 1e-4f) {          // hand the vacated jack a ramp, not a step
                declickOpen = open;
                declickVal  = lastOut;
                declickGain = 1.f;
            }
            air.reset();                // the shelf changes coefficients with the path
        }
        samplePos   = 0.f;
        env         = 1.f;
        open        = openMode;
        latchedChar = rack::math::clamp(charStrength, 0.f, 1.f);
    }

    /// Render one sample to chhOut / ohhOut (audio-normalized, pre case-gain and
    /// master). The voice emits on the jack its control line selects; the other
    /// carries only the declick ramp, if one is running.
    void process(const rack::Module::ProcessArgs& args,
                 float chhTune, float chhDecay, float chhDrive, float chhLevel,
                 float ohhTune, float ohhDecay, float ohhDrive, float ohhLevel,
                 float& chhOut, float& ohhOut) {
        // -- Control line selects the voicing path ---------------------
        const float tune     = open ? ohhTune     : chhTune;
        const float decay    = open ? ohhDecay    : chhDecay;
        const float drive    = open ? ohhDrive    : chhDrive;
        const float level    = open ? ohhLevel    : chhLevel;
        const float tuneOct  = open ? kOhhTuneOctaves : kChhTuneOctaves;
        const float decayMin = open ? kOhhDecayMinSec : kChhDecayMinSec;
        const float decayMax = open ? kOhhDecayMaxSec : kChhDecayMaxSec;
        const float airAmt   = open ? kOhhAir     : kChhAir;
        const float airHz    = open ? kOhhAirHz   : kChhAirHz;
        const float mixGain  = open ? kOhhMixGain : kChhMixGain;
        const float norm     = open ? 1.05f       : 1.04f;
        const float driveAmt = open ? kOhhAccent.driveAmt : kChhAccent.driveAmt;
        // The closed path square-roots its decay control for a usable knob taper
        // over its much shorter range; the open path maps straight through.
        const float decayShape = open ? decay : std::sqrt(decay);

        // -- The one playback system -----------------------------------
        const float rate = std::pow(2.f, (tune - 0.5f) * 2.f * tuneOct);
        const float decaySec = Ghost::expDecaySec(decayShape, decayMin, decayMax);
        const auto& src = hatSource();
        float s = Ghost::sampleAt(src, samplePos);
        samplePos += Ghost::playbackStep(
            Ghost::kEmbeddedPcmSampleRate, args.sampleRate, rate);
        env *= std::exp(-args.sampleTime / decaySec);
        if (env < Ghost::kDenormalFloor) env = 0.f;   // denormal safety
        float o = s * env * norm;
        o = air.process(o, airAmt, airHz, args.sampleRate);   // 909-style air
        o = Ghost::driveWithAccent(o, drive, latchedChar, driveAmt);
        o *= level * mixGain;
        lastOut = o;

        // -- Split onto GHOST's two jacks ------------------------------
        chhOut = open ? 0.f : o;
        ohhOut = open ? o   : 0.f;
        if (declickGain > 0.f) {
            const float d = declickVal * declickGain;
            if (declickOpen) ohhOut += d; else chhOut += d;
            declickGain -= args.sampleTime / kDeclickSec;
            if (declickGain < 0.f) declickGain = 0.f;
        }
    }
};

} // namespace
