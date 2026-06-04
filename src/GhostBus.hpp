#pragma once
#include <rack.hpp>
#include <algorithm>
#include <cmath>
#include "AgentModule.hpp"

/**
 * GhostBus -- adjacent-module state broadcast for the Ghost drum suite.
 *
 * GhostCtrl publishes slow-changing global state (accent multiplier and
 * master volume) and adjacent voices read it via the leftExpander/
 * rightExpander module pointer. Hit-time events (triggers, total accent
 * gate, local accent gate) are NOT carried on this bus -- they travel
 * through cables to per-voice inputs so they're sampled deterministically
 * on the trigger rising edge with zero latency.
 *
 * The bus uses a "pull" / direct-read pattern: each GhostModule exposes
 * its current state as a public member, neighbors read it directly.
 * Voices forward by copying the resolved state into their own currentBus
 * each frame, so a chain of [GhostCtrl][Kck][Snr][Toms]... all see the
 * controller's broadcast even though only the leftmost voice is directly
 * adjacent to the controller.
 *
 * Latency caveat: this is NOT a same-sample-deterministic bus. Reads see
 * the value the neighbor wrote on its most recent process() call. If the
 * neighbor processes AFTER us in a frame, we get its value from the
 * previous frame (1-sample staleness). Forwarding through N voices adds
 * up to N-1 samples of staleness for the far end of the chain. This is
 * acceptable for slow knob/CV state but DO NOT build synchronous
 * trigger logic on top of this bus -- use cables for any hit-time event.
 */

namespace Ghost {

struct Bus {
    // accentAAmount / accentBAmount are 0..1 linear-space attenuators on
    // each accent rail's contribution. amtA=0 mutes A's contribution
    // wherever A fires (alone or in the both case); same for amtB.
    // ghostAmount is a 0..1 attenuator on the ghost case (no accent
    // gates fired) -- lets the user globally tune "default volume" /
    // "ghost note level" without editing per-voice config.
    float accentAAmount     = 1.f;
    float accentBAmount     = 1.f;
    float ghostAmount       = 1.f;
    float masterVolume      = 1.f;   // post-everything linear scalar
    // RANGE: scales every voice's ghost-floor dB (mix.ghostDb * ghostScale).
    // Voices with a 0 dB floor (gentleMix) are unaffected (0 * x = 0); only
    // floored voices (the kick) widen/tighten. 1.0 = each voice's own default.
    float ghostScale        = 1.f;
    bool  controllerPresent = false;
};

/**
 * AccentMix -- per-voice level relationship across the four 909 accent
 * cases, expressed in dB relative to a reference level.
 *
 * The four cases:
 *   - ghost:   neither gate fired. On a real 909 this is the ghost note
 *              level: a programmer leaves both accents off when they
 *              want the voice to duck below its normal hit.
 *   - local:   B only (per-voice accent track). This is the 909 "normal"
 *              hit on supported voices (BD/SD/Toms/CH per classic 909 layout)
 *              and is the project reference (0 dB by default).
 *   - global:  A only (Total Accent track on its own). Slight emphasis,
 *              less than full because there's no per-voice support.
 *   - both:    A and B together. The strongest hit.
 *
 * Defaults are MODEST starting points (-6 / -1 / 0 / +1.5 dB) selected
 * to be tunable by ear without clipping. They are NOT verified
 * hardware-faithful values; per-voice tuning by ear or against the reference machine
 * reference samples is expected.
 *
 * AccentMix encodes ONLY the level relationship. The voice's own
 * Fit::Config still decides the CHARACTER of an accented hit
 * (drive, pitch dive, click brightness, etc.) via per-DSP-stage weights
 * gated on "is this hit accented at all?" -- different concerns,
 * intentionally separated.
 *
 * To make a voice ignore Accent B (Ohh / RimClap / Crash / Ride per
 * classic 909 layout): set localDb = ghostDb so B-alone collapses to ghost.
 * Or simply do not wire LOCAL_ACC_INPUT on the voice.
 */
struct AccentMix {
    float ghostDb  = -6.f;
    float globalDb = -1.f;
    float localDb  =  0.f;
    float bothDb   = +1.5f;
};

/**
 * Neutral mix: every case at 0 dB so accent rails change nothing.
 * Used as the per-voice default until that voice's level relationship
 * has been calibrated by ear; lets us plumb accent inputs everywhere
 * without changing audible behaviour for un-tuned voices.
 */
inline AccentMix neutralMix() {
    AccentMix m;
    m.ghostDb = m.globalDb = m.localDb = m.bothDb = 0.f;
    return m;
}

/** dB to linear gain (10^(db/20)). */
inline float dbToLinear(float db) {
    return std::pow(10.f, db / 20.f);
}

/**
 * AccentCharacter -- per-voice TIMBRAL response to charStrength.
 *
 * AccentMix encodes LEVEL (per-case dB applied at output). AccentCharacter
 * encodes CHARACTER (per-DSP-stage weight applied when charStrength > 0,
 * inside the voice's process() loop). Different concerns, intentionally
 * separated so a voice can have aggressive level dynamics without timbral
 * coloration, or vice versa.
 *
 * Each field is the per-stage weight passed to accentScale() or accentAdd()
 * (defined below). Defaults are 0 -- no character change. A voice declares
 * only the fields it actually uses; unused fields sit at zero and cost
 * nothing. Field-name convention encodes the conventional helper:
 *
 *   bodyAmt        accentScale on body / oscillator level
 *   pitchAmt       accentScale on pitch-dive envelope magnitude
 *   clickAmt       accentScale on click / transient gain
 *   driveAmt       accentAdd  on tanh drive amount
 *   noiseAmt       accentScale on noise / snappy gain (SD)
 *   snapAmt        accentScale on snap / attack brightness
 *   decayAmt       accentAdd  on envelope decay rate
 *   brightnessAmt  accentScale on filter cutoff / bit depth / brightness
 *   bendAmt        accentScale on tom pitch-bend depth
 *
 * The naming is convention; per-voice authors keep the freedom to choose
 * scale-vs-add per stage. Per the 909 reference doc, only SD has true
 * timbral accent on the original hardware (gates noise and tone VCAs);
 * other voices are level-only on the original. AccentCharacter is what
 * gives Ghost room to add tasteful per-voice timbral character on
 * top of the level rail without losing circuit-faithful behaviour
 * (zero is the faithful default).
 */
struct AccentCharacter {
    // No in-class member initializers: keeps the struct a C++11 aggregate
    // so voices can use braced init (`{0.80f, 0.80f, 0.50f, 0.60f}`).
    // Trailing fields not specified are value-initialized to 0 (no
    // character change). To declare an all-zero AccentCharacter
    // explicitly, write `AccentCharacter accent{};`.
    float bodyAmt;
    float pitchAmt;
    float clickAmt;
    float driveAmt;
    float noiseAmt;
    float snapAmt;
    float decayAmt;
    float brightnessAmt;
    float bendAmt;
};

// ---------------------------------------------------------------------------
// Shared accent policy. Tune the whole suite's accent FEEL here, in ONE place,
// instead of scattering magic numbers across every voice. Each voice composes
// from these named amounts via a factory; real per-voice differences (snare
// noise, tom body) live in the factory, not as repeated literals downstream.
// 909-style accent = a little drive + a touch of gain; NO pitch/body sweep.
// ---------------------------------------------------------------------------
namespace Accent {
    constexpr float kDrive     = 0.10f;  // saturation bump (sample voices: rim/hats/cymbals)
    constexpr float kClick     = 0.15f;  // attack/click emphasis (kick, toms)
    constexpr float kBody      = 0.10f;  // body emphasis (toms)
    constexpr float kNoise     = 0.12f;  // noise emphasis (snare)
    constexpr float kDriveKick = 0.20f;  // kick's slightly stronger drive
    constexpr float kGainDb    = 3.0f;   // additive level boost on an accented hit

    // {body, pitch, click, drive, noise, snap, decay, brightness, bend}
    inline AccentCharacter driveOnly(float drive = kDrive) { return AccentCharacter{0,0,0,drive,0,0,0,0,0}; }
    inline AccentCharacter kick()  { return AccentCharacter{0,0,kClick,kDriveKick,0,0,0,0,0}; }
    inline AccentCharacter snare() { return AccentCharacter{0,0,0,kDrive*0.8f,kNoise,0,0,0,0}; }
    inline AccentCharacter toms()  { return AccentCharacter{kBody,0,kClick,kDrive*0.8f,0,0,0,0,0}; }
    // Un-accented stays at normal level; an accented hit adds +kGainDb on top.
    inline AccentMix gentleMix() { AccentMix m = neutralMix(); m.globalDb = kGainDb; m.bothDb = kGainDb; return m; }

    // Kick: pronounced 909-style accent ladder, specified as linear LEVELS
    // (at full CTRL amount each tier's gain == its target):
    //   no accent (ghost)   15%  -> -16.48 dB  (floor at CTRL RANGE = Classic)
    //   Accent B (local)    60%  ->  -4.44 dB  (moving Accent B swings 15->60%, ~12 dB)
    //   Accent A (global)  100%  ->   0 dB     (the ceiling)
    //   both               100%  ->   0 dB     (capped at Accent A, never hotter)
    // 20*log10(level) for each. CTRL RANGE scales the ghost floor: Tight ~35%,
    // Classic 15% (this default), Wide ~8%.
    inline AccentMix kickMix() {
        AccentMix m;
        m.ghostDb = -16.48f; m.localDb = -4.44f; m.globalDb = 0.f; m.bothDb = 0.f;
        return m;
    }
}

/**
 * Resolve the per-case output gain (linear) at trigger-rising-edge time.
 *
 * Reads the bus's three accent attenuators (accentAAmount,
 * accentBAmount, ghostAmount) and the voice's per-case dB mix.
 *
 * Single-rail and both-case scaling (linear contribution model):
 *     gain = amtA * lin(globalDb) + amtB * lin(localDb)
 *          + amtA*amtB * (lin(bothDb) - lin(globalDb) - lin(localDb))
 * (cases compute only the contributions for rails that fired).
 *
 * - amtA = 0 removes A's contribution wherever A fires (alone or both).
 * - amtA = 1 makes A's contribution count fully.
 * - same for amtB.
 * - if a rail fires but its corresponding amount is 0, level falls back to
 *   the remaining effective rails; if none remain, the hit falls back to the
 *   ghost case rather than going silent.
 *
 * Ghost case (neither gate fires):
 *     gain = lin(ghostDb) * bus.ghostAmount
 * The bus's ghostAmount knob is a global trim on the no-accent level.
 *
 * Verified algebraically:
 *   - amtA=1, amtB=1, both fire: gain = lin(bothDb)
 *   - amtA=0, amtB=1, both fire: gain = lin(localDb) (B-only behavior)
 *   - amtA=1, amtB=0, both fire: gain = lin(globalDb) (A-only behavior)
 *   - amtA=0, amtB=0, any fire:  gain = lin(ghostDb) * ghostAmount
 *   - no gates, ghostAmt=1:      gain = lin(ghostDb)
 *   - no gates, ghostAmt=0:      gain = 0
 */
inline float resolveAccentGain(bool totalGate, bool localGate,
                               const Bus& bus,
                               const AccentMix& mix) {
    const bool effectiveTotal = totalGate && (bus.accentAAmount > 0.f);
    const bool effectiveLocal = localGate && (bus.accentBAmount > 0.f);
    const float floorDb = mix.ghostDb * bus.ghostScale;   // RANGE widens/tightens
    const float ghost = dbToLinear(floorDb) * bus.ghostAmount;
    if (!effectiveTotal && !effectiveLocal) {
        return ghost;
    }

    // Accent is a boost ADDED on top of the un-accented level, scaled by the
    // CTRL amounts -- so the accented hit never dips below normal and never
    // jumps in from silence. Matches the classic 909, where accent just adds a bit
    // of gain on top (gentle), rather than replacing the level outright.
    const float ghostLin = dbToLinear(floorDb);
    const float dA    = dbToLinear(mix.globalDb) - ghostLin;   // boost delta vs ghost
    const float dB    = dbToLinear(mix.localDb)  - ghostLin;
    const float dBoth = dbToLinear(mix.bothDb)   - ghostLin;
    const float amtA  = bus.accentAAmount;
    const float amtB  = bus.accentBAmount;

    float gain = ghost;
    if (effectiveTotal)                   gain += amtA * dA;
    if (effectiveLocal)                   gain += amtB * dB;
    if (effectiveTotal && effectiveLocal) gain += amtA * amtB * (dBoth - dA - dB);
    return gain;
}

/**
 * Whether the hit should apply the voice's accent CHARACTER (per-DSP
 * weights for drive/click/pitch/etc.). Boolean: any accent gate fires =
 * accent character on; ghost = off. The voice's own Fit::Config decides
 * what that character is.
 */
inline bool isAccentedHit(bool totalGate, bool localGate) {
    return totalGate || localGate;
}

inline float accentAdd(float base, float charStrength, float amount) {
    return base + charStrength * amount;
}

inline float accentScale(float base, float charStrength, float amount) {
    return base * (1.f + charStrength * amount);
}


/** Result of resolving the accent state at a TRIG rising edge. */
struct AccentResolution {
    float charStrength;  // 0 or 1, drives voice DSP accent character
    float gain;          // linear multiplier for the voice output
};

/**
 * One-shot sample of the accent gates at trigger-rising-edge time.
 *
 * Reads totalInputId from `self`'s inputs (always required); reads
 * localInputId if it is >= 0 (voices without Accent B per the classic 909
 * OM -- Ohh, RimClap, CrashRide -- pass -1 to skip). Combines the gates
 * with bus state and the voice's mix into a {charStrength, gain} pair.
 *
 * Voice integration (with Accent B):
 *     auto acc = sampleAccentAtTrig(this, TOTAL_ACC_INPUT, bus,
 *                                   fit.accentMix, LOCAL_ACC_INPUT);
 *     voice.fire(acc.charStrength);
 *     latchedCaseGain = acc.gain;
 *
 * Voice integration (without Accent B):
 *     auto acc = sampleAccentAtTrig(this, TOTAL_ACC_INPUT, bus,
 *                                   fit.accentMix);
 *     voice.fire(acc.charStrength);
 *     latchedCaseGain = acc.gain;
 */
inline AccentResolution sampleAccentAtTrig(rack::Module* self,
                                           int totalInputId,
                                           const Bus& bus,
                                           const AccentMix& mix,
                                           int localInputId = -1) {
    // Accent A (total) is normalled ON: an unpatched jack reads as accented, so a
    // lone voice sounds powerful by default; patching an accent cable overrides it
    // (see issue #21). Accent B (local) stays normalled OFF.
    const bool totalGate = (totalInputId >= 0)
        && self->inputs[totalInputId].getNormalVoltage(10.f) > 1.f;
    const bool localGate = (localInputId >= 0)
        && self->inputs[localInputId].getNormalVoltage(0.f) > 1.f;
    AccentResolution r;
    r.charStrength = isAccentedHit(totalGate, localGate) ? 1.f : 0.f;
    r.gain         = resolveAccentGain(totalGate, localGate, bus, mix);
    return r;
}

} // namespace

/** Marker base for any 909 module that participates in the state bus. */
struct GhostModule : AgentModule {
    Ghost::Bus currentBus;
};

namespace Ghost {

/**
 * Resolve the current bus state by checking adjacent GhostModules.
 * Returns first found controllerPresent state, defaulting to a "no
 * controller, neutral values" Bus otherwise. Voices should call this
 * once per process() and use the returned amounts when computing
 * accent/output.
 *
 * Also writes the resolved state into self->currentBus so the chain
 * extends through this voice to whichever neighbor is on the other side.
 */
inline Bus resolveBus(GhostModule* self) {
    Bus state;

    if (auto* leftN = dynamic_cast<GhostModule*>(self->leftExpander.module)) {
        if (leftN->currentBus.controllerPresent) {
            state = leftN->currentBus;
        }
    }
    if (!state.controllerPresent) {
        if (auto* rightN = dynamic_cast<GhostModule*>(self->rightExpander.module)) {
            if (rightN->currentBus.controllerPresent) {
                state = rightN->currentBus;
            }
        }
    }

    self->currentBus = state;
    return state;
}

} // namespace
