#pragma once
//
// voices.hpp -- uniform adapters over the Ghost DSP voice cores so the stress
// harness can drive every voice through one templated loop.
//
// Coverage: every Ghost voice now factors its per-sample DSP into an includable
// *Engine.hpp core -- Kck, Toms (Low/Mid/High), CrashRide (Crash/Ride),
// RimClap (Rim/Clap), Snr (SnrVoice), and ChhOhh (ChhOhhVoice, both hats + the
// internal CH->OH choke). The full 10-voice kit is driven header-only here.
//
// Each adapter exposes the same tiny interface:
//     const char* name() const
//     void  fire(float accent)     // (re)trigger the voice
//     void  reset()                // restore to default-constructed state
//     float process(const ProcessArgs&, const Ctrl&)
//
// The adapters are concrete (no virtual dispatch) so the perf numbers reflect
// real per-sample cost, not vtable overhead.

#include "KckEngine.hpp"
#include "TomsEngine.hpp"
#include "CrashRideEngine.hpp"
#include "RimClapEngine.hpp"
#include "SnrEngine.hpp"
#include "ChhOhhEngine.hpp"

#include <cmath>

namespace stress {

// Normalized (0..1) control values fed to a voice each sample. Each adapter
// reads the subset its engine needs.
struct Ctrl {
    float tune       = 0.5f;
    float decay      = 0.5f;
    float pitch      = 0.5f;
    float pitchDecay = 0.5f;
    float click      = 0.5f;
    float drive      = 0.0f;
    float level      = 0.85f;
    float accent     = 0.0f;
};

inline rack::Module::ProcessArgs makeArgs(float sampleRate, long frame) {
    rack::Module::ProcessArgs a;
    a.sampleRate = sampleRate;
    a.sampleTime = 1.f / sampleRate;
    a.frame = frame;
    return a;
}

// --- Kck -------------------------------------------------------------------
struct KckAdapter {
    KckVoice voice;
    KckFit::Config fit = KckFit::makeKick();
    const char* name() const { return "Kck"; }
    void fire(float accent) { voice.fire(accent); }
    void reset() { voice = KckVoice(); }
    float process(const rack::Module::ProcessArgs& args, const Ctrl& c) {
        return voice.process(args, fit, c.tune, c.decay, c.pitch,
                             c.pitchDecay, c.click, c.drive, c.level);
    }
};

// --- Toms (Low / Mid / High) ----------------------------------------------
struct TomAdapter {
    TomVoice voice;
    TomFit::Config fit;
    const char* nm;
    TomAdapter(const TomFit::Config& f, const char* n) : fit(f), nm(n) {}
    const char* name() const { return nm; }
    void fire(float /*accent*/) { voice.fire(); }
    void reset() { voice = TomVoice(); }
    float process(const rack::Module::ProcessArgs& args, const Ctrl& c) {
        return voice.process(args, fit, c.tune, c.decay, c.level, c.accent);
    }
};

// --- CrashRide (Crash / Ride), via Ghost::RomVoice -------------------------
struct CrashRideAdapter {
    Ghost::RomVoice voice;
    const Ghost::RomAsset* asset;
    Ghost::RomVoiceConfig cfg;
    float tuneOct, decMin, decMax;
    const char* nm;
    CrashRideAdapter(const Ghost::RomAsset* a, const Ghost::RomVoiceConfig& vc,
                     float to, float dmin, float dmax, const char* n)
        : asset(a), cfg(vc), tuneOct(to), decMin(dmin), decMax(dmax), nm(n) {}
    const char* name() const { return nm; }
    void fire(float /*accent*/) { voice.trigger(); }
    void reset() { voice = Ghost::RomVoice(); }
    float process(const rack::Module::ProcessArgs& args, const Ctrl& c) {
        const float rate = std::pow(2.f, (c.tune - 0.5f) * 2.f * tuneOct);
        const float decSec = decMin + (decMax - decMin) * c.decay;
        float s = voice.process(args, *asset, rate, decSec, c.decay, cfg);
        s = Ghost::drive(s, c.drive);   // module applies drive + level post-voice
        return s * c.level;
    }
};

// --- Snr -------------------------------------------------------------------
struct SnrAdapter {
    SnrVoice voice;
    SnrFit::Config fit = SnrFit::defaults();
    const char* name() const { return "Snr"; }
    void fire(float accent) { voice.fire(accent); }
    void reset() { voice = SnrVoice(); }
    float process(const rack::Module::ProcessArgs& args, const Ctrl& c) {
        // Snr surface: Tune / Tone / Snappy / Level. Map the generic Ctrl:
        // tune->tune, pitch->tone, click->snappy, level->level.
        return voice.process(args, fit, c.tune, c.pitch, c.click, c.level);
    }
};

// --- ChhOhh (closed + open hat in one struct, internal CH->OH choke) --------
// Single combined voice (the module has two outputs but one DSP struct). fire()
// triggers CH then OH each retrigger: CH-first so on the opening hit OH then
// rings out on its natural (up to 3.2 s) tail -- the longest, most denormal-
// prone path -- while the choke coupling in fireChh() is still exercised on
// every subsequent overlapping hit. process() returns CH + OH summed so the
// harness's finite/bounded/flush checks cover both signal paths at once.
struct ChhOhhAdapter {
    ChhOhhVoice voice;
    const char* name() const { return "ChhOhh"; }
    void fire(float accent) { voice.fireChh(accent); voice.fireOhh(accent); }
    void reset() { voice = ChhOhhVoice(); }
    float process(const rack::Module::ProcessArgs& args, const Ctrl& c) {
        float chh = 0.f, ohh = 0.f;
        voice.process(args,
                      c.tune, c.decay, c.drive, c.level,   // CH controls
                      c.tune, c.decay, c.drive, c.level,   // OH controls
                      chh, ohh);
        return chh + ohh;
    }
};

// --- RimClap (Rim / Clap), via the engine's local RomVoice -----------------
struct RimClapAdapter {
    RomVoice voice;                       // anonymous-namespace RomVoice from RimClapEngine.hpp
    const std::vector<float>* sample;
    const char* nm;
    RimClapAdapter(const std::vector<float>* s, const char* n) : sample(s), nm(n) {}
    const char* name() const { return nm; }
    void fire(float /*accent*/) { voice.trigger(); }
    void reset() { voice = RomVoice(); }
    float process(const rack::Module::ProcessArgs& args, const Ctrl& c) {
        const float rate = std::pow(2.f, (c.tune - 0.5f) * 2.f);
        float s = voice.process(*sample, args.sampleRate, rate, 16);
        return s * c.level;
    }
};

} // namespace stress
