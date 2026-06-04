// stress_test.cpp -- headless DSP stress harness for the Ghost voice cores.
//
// Two build personalities from one source (see tests/stress/Makefile):
//   * sanitized build  (ASan+UBSan, default): robustness invariants + leaks.
//   * perf build       (-DSTRESS_PERF -O2):   robustness + zero-alloc + ns/sample + RSS.
//
// Layers (issue #17):
//   Layer 1  robustness  -- finite, bounded, denormal-flush, reset-determinism
//   Layer 2  RT-safety   -- zero heap allocation in steady-state process()  [perf build]
//   Layer 3  perf+leak   -- ns/sample, voices-per-core@48k, flat RSS         [perf build]
//
// No VCV Rack, no libRack, no GUI. SDK headers only + tests/stress/rack_shim.cpp.

#include "voices.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <chrono>
#include <atomic>

#if defined(__APPLE__)
#include <mach/mach.h>
#endif

// ---------------------------------------------------------------------------
// Allocation counter (perf build only -- overriding operator new under ASan
// would fight ASan's own allocator instrumentation).
// ---------------------------------------------------------------------------
#ifdef STRESS_PERF
static std::atomic<long> g_allocCount{0};
static std::atomic<bool> g_allocArmed{false};

void* operator new(std::size_t n) {
    if (g_allocArmed.load(std::memory_order_relaxed))
        g_allocCount.fetch_add(1, std::memory_order_relaxed);
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
#endif

using namespace stress;

// ---------------------------------------------------------------------------
// Adversarial control generator: deterministic LCG choosing extreme control
// states (full-scale, silence, denormal-range, mid-range sweeps).
// ---------------------------------------------------------------------------
struct Adversary {
    uint32_t s = 0x1234567u;
    float nextU() {   // [0,1)
        s = s * 1664525u + 1013904223u;
        return ((s >> 8) & 0xFFFFFFu) * (1.f / 16777216.f);
    }
    // Pick an extreme-leaning normalized value.
    float extreme() {
        float r = nextU();
        if (r < 0.30f) return 0.f;           // silence / min
        if (r < 0.60f) return 1.f;           // full-scale / max
        if (r < 0.70f) return 1e-25f;        // denormal-range
        return nextU();                       // mid sweep
    }
    Ctrl ctrl() {
        Ctrl c;
        c.tune = extreme(); c.decay = extreme(); c.pitch = extreme();
        c.pitchDecay = extreme(); c.click = extreme(); c.drive = extreme();
        c.level = extreme(); c.accent = (nextU() < 0.5f) ? 1.f : 0.f;
        return c;
    }
};

static const float kSampleRates[4] = {44100.f, 48000.f, 96000.f, 192000.f};

struct VoiceResult {
    std::string name;
    bool finite[4]   = {true, true, true, true};
    bool bounded[4]  = {true, true, true, true};
    double peak[4]   = {0, 0, 0, 0};
    bool denormFlush = true;
    bool resetDet    = true;
    long  allocs     = -1;     // -1 = not measured (san build)
    double nsPerSample[4] = {0, 0, 0, 0};
    long samplesPerSR = 0;
};

static const double kBound = 12.0;   // |out| <= ~12V (runaway guard)

// Layer 1: robustness sweep at one sample rate.
template <class A>
void robustnessAtSR(A& v, int sri, long samples, VoiceResult& res, double& peakOut) {
    Adversary adv;
    rack::Module::ProcessArgs args = makeArgs(kSampleRates[sri], 0);
    Ctrl c = adv.ctrl();
    bool finite = true, bounded = true;
    double peak = 0.0;
    long retrigMask = 0x1FFF;   // re-fire ~ every 8k samples
    for (long i = 0; i < samples; ++i) {
        if ((i & retrigMask) == 0) { v.fire(adv.nextU()); c = adv.ctrl(); }
        if ((i & 0x3FF) == 0) c = adv.ctrl();          // param sweep churn
        float out = v.process(args, c);
        if (!std::isfinite(out)) finite = false;
        double a = std::fabs(out);
        if (a > peak) peak = a;
        if (a > kBound) bounded = false;
        args.frame++;
    }
    res.finite[sri] = finite;
    res.bounded[sri] = bounded;
    res.peak[sri] = peak;
    peakOut = peak;
}

// Layer 1: denormal flush -- fire once, run well past decay, assert exact 0.
template <class A>
bool denormalFlush(A& v) {
    v.reset();
    v.fire(1.f);
    Ctrl c;                       // mid defaults, audible hit
    rack::Module::ProcessArgs args = makeArgs(48000.f, 0);
    const long total = 48000L * 8;          // 8 seconds -- past every voice's tail
    for (long i = 0; i < total; ++i) { v.process(args, c); args.frame++; }
    // Final 0.25 s must be exactly zero (denormal floor snapped state to 0).
    for (long i = 0; i < 12000; ++i) {
        float out = v.process(args, c); args.frame++;
        if (out != 0.0f) return false;
    }
    return true;
}

// Layer 1: reset determinism -- process/reset/process must be bit-identical.
template <class A>
bool resetDeterminism(A& v) {
    const long N = 48000;
    std::vector<float> a(N), b(N);
    Adversary seq;                            // shared deterministic ctrl stream
    auto run = [&](std::vector<float>& buf) {
        Adversary adv;                        // identical seed each pass
        v.reset();
        v.fire(0.5f);
        rack::Module::ProcessArgs args = makeArgs(48000.f, 0);
        Ctrl c = adv.ctrl();
        for (long i = 0; i < N; ++i) {
            if ((i & 0x7FF) == 0) c = adv.ctrl();
            buf[i] = v.process(args, c);
            args.frame++;
        }
    };
    (void)seq;
    run(a);
    run(b);
    return std::memcmp(a.data(), b.data(), N * sizeof(float)) == 0;
}

#ifdef STRESS_PERF
// Layer 2: zero allocation in steady state.
template <class A>
long allocInstrument(A& v) {
    // Warm up: trigger lazy PCM decode + any one-time vector allocs.
    rack::Module::ProcessArgs args = makeArgs(48000.f, 0);
    Ctrl c;
    v.reset();
    for (int k = 0; k < 8; ++k) {
        v.fire(0.5f);
        for (long i = 0; i < 48000; ++i) { v.process(args, c); args.frame++; }
    }
    // Armed steady-state measurement.
    g_allocCount.store(0);
    g_allocArmed.store(true);
    for (long i = 0; i < 2000000; ++i) {
        if ((i % 8000) == 0) v.fire(0.5f);
        v.process(args, c);
        args.frame++;
    }
    g_allocArmed.store(false);
    return g_allocCount.load();
}

// Layer 3: ns/sample at one sample rate.
template <class A>
double perfAtSR(A& v, int sri, long samples) {
    rack::Module::ProcessArgs args = makeArgs(kSampleRates[sri], 0);
    Ctrl c;
    v.reset();
    v.fire(0.5f);
    volatile float sink = 0.f;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (long i = 0; i < samples; ++i) {
        if ((i % 8000) == 0) v.fire(0.5f);
        sink += v.process(args, c);
        args.frame++;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    (void)sink;
    double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return ns / double(samples);
}

#if defined(__APPLE__)
static long currentRSSkb() {
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  (task_info_t)&info, &count) != KERN_SUCCESS)
        return -1;
    return (long)(info.resident_size / 1024);
}
#else
static long currentRSSkb() { return -1; }
#endif
#endif // STRESS_PERF

// ---------------------------------------------------------------------------
// Per-voice driver.
// ---------------------------------------------------------------------------
template <class A>
VoiceResult runVoice(A& v, long samples) {
    VoiceResult res;
    res.name = v.name();
    res.samplesPerSR = samples;
    for (int sri = 0; sri < 4; ++sri) {
        double pk = 0;
        robustnessAtSR(v, sri, samples, res, pk);
    }
    res.denormFlush = denormalFlush(v);
    res.resetDet = resetDeterminism(v);
#ifdef STRESS_PERF
    res.allocs = allocInstrument(v);
    for (int sri = 0; sri < 4; ++sri)
        res.nsPerSample[sri] = perfAtSR(v, sri, 10000000L);
#endif
    return res;
}

static const char* yn(bool b) { return b ? "PASS" : "FAIL"; }

int main() {
#ifdef STRESS_PERF
    const long SAMPLES = 10000000L;   // 10M per voice per SR
    const char* mode = "PERF (-O2, no sanitizer)";
#else
    const long SAMPLES = 2000000L;    // 2M per voice per SR under ASan+UBSan
    const char* mode = "SANITIZED (ASan+UBSan)";
#endif
    std::printf("# Ghost DSP stress harness -- %s build\n", mode);
    std::printf("# robustness samples/voice/SR = %ld; SRs = 44100/48000/96000/192000\n\n", SAMPLES);

    using namespace crashride_impl;

    std::vector<VoiceResult> results;

    { KckAdapter v;                                              results.push_back(runVoice(v, SAMPLES)); }
    { TomAdapter v(TomFit::makeLowTom(),  "TomLow");             results.push_back(runVoice(v, SAMPLES)); }
    { TomAdapter v(TomFit::makeMidTom(),  "TomMid");             results.push_back(runVoice(v, SAMPLES)); }
    { TomAdapter v(TomFit::makeHighTom(), "TomHigh");            results.push_back(runVoice(v, SAMPLES)); }
    { CrashRideAdapter v(&crashAsset(), kCrashRomCfg, kCrashTuneOctaves,
                         kCrashDecayMinSec, kCrashDecayMaxSec, "Crash");
      results.push_back(runVoice(v, SAMPLES)); }
    { CrashRideAdapter v(&rideAsset(), kRideRomCfg, kRideTuneOctaves,
                         kRideDecayMinSec, kRideDecayMaxSec, "Ride");
      results.push_back(runVoice(v, SAMPLES)); }
    { RimClapAdapter v(&rimClapClapSource(), "Clap");           results.push_back(runVoice(v, SAMPLES)); }
    { RimClapAdapter v(&rimClapRimSource(),  "Rim");            results.push_back(runVoice(v, SAMPLES)); }
    { SnrAdapter v;                                             results.push_back(runVoice(v, SAMPLES)); }
    { ChhOhhAdapter v;                                          results.push_back(runVoice(v, SAMPLES)); }

    // ---- Robustness table ----
    std::printf("## Robustness (per voice, across 44.1/48/96/192 kHz)\n");
    std::printf("%-9s | finite | bounded | peak(max) | denormFlush | resetDet | zeroAlloc\n", "voice");
    std::printf("----------|--------|---------|-----------|-------------|----------|----------\n");
    bool allPass = true;
    for (auto& r : results) {
        bool fin = r.finite[0]&&r.finite[1]&&r.finite[2]&&r.finite[3];
        bool bnd = r.bounded[0]&&r.bounded[1]&&r.bounded[2]&&r.bounded[3];
        double pk = 0; for (double p : r.peak) if (p > pk) pk = p;
        std::string alloc = (r.allocs < 0) ? "n/a" : (r.allocs == 0 ? "PASS(0)" : ("FAIL(" + std::to_string(r.allocs) + ")"));
        std::printf("%-9s | %-6s | %-7s | %9.4f | %-11s | %-8s | %s\n",
                    r.name.c_str(), yn(fin), yn(bnd), pk,
                    yn(r.denormFlush), yn(r.resetDet), alloc.c_str());
        allPass = allPass && fin && bnd && r.denormFlush && r.resetDet && (r.allocs <= 0 || r.allocs == 0);
        if (r.allocs > 0) allPass = false;
    }

#ifdef STRESS_PERF
    std::printf("\n## Performance (ns/sample; 10M samples/voice/SR)\n");
    std::printf("%-9s | 44.1kHz | 48kHz | 96kHz | 192kHz | voices/core@48k\n", "voice");
    std::printf("----------|---------|-------|-------|--------|----------------\n");
    for (auto& r : results) {
        double budget48 = 1e9 / 48000.0;          // ns available per sample @48k
        double vpc = (r.nsPerSample[1] > 0) ? budget48 / r.nsPerSample[1] : 0;
        std::printf("%-9s | %7.2f | %5.2f | %5.2f | %6.2f | %.0f\n",
                    r.name.c_str(), r.nsPerSample[0], r.nsPerSample[1],
                    r.nsPerSample[2], r.nsPerSample[3], vpc);
    }
    // RSS flatness across a long run.
    long rssStart = currentRSSkb();
    { KckAdapter v; v.fire(0.5f);
      rack::Module::ProcessArgs args = makeArgs(48000.f, 0); Ctrl c;
      for (long i = 0; i < 80000000L; ++i) { if ((i%8000)==0) v.fire(0.5f); v.process(args,c); args.frame++; } }
    long rssEnd = currentRSSkb();
    std::printf("\n## RSS flatness (80M-sample run)\n");
    std::printf("RSS start = %ld kB, RSS end = %ld kB, delta = %ld kB -> %s\n",
                rssStart, rssEnd, rssEnd - rssStart,
                (rssEnd - rssStart) < 256 ? "FLAT (PASS)" : "GROWTH (INVESTIGATE)");
#endif

    std::printf("\n## Overall: %s\n", allPass ? "ALL PASS" : "FAILURES PRESENT");
    return allPass ? 0 : 1;
}
