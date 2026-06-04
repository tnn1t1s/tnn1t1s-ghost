// fuzz_voice.cpp -- libFuzzer harness for the Ghost DSP voice cores (Layer 4).
//
// The fuzzer byte stream is consumed as a little driver program:
//   byte 0        -> selects one of the 10 voices (mod 10)
//   remaining bytes, read in small groups, become a sequence of:
//     * param updates  (tune/decay/pitch/.../level, each byte -> [0,1])
//     * gate events     (re-fire the voice with an accent)
//     * sample-rate switches (44.1 / 48 / 96 / 192 kHz)
//     * a per-step block length (how many process() samples to run)
//
// Every produced sample is asserted finite and bounded (|out| <= 12). Built with
// -fsanitize=fuzzer,address,undefined so NaN/Inf, UB, OOB, and runaway output all
// abort with a reproducer. Coverage-guided: libFuzzer mutates toward new control
// paths inside each voice's process().
//
// Build/run:  make -C tests/stress fuzz        (all voices, bounded campaign)
// Single run: ./fuzz_voice corpus/ -max_total_time=60
//
// SDK headers only + rack_shim.cpp, exactly like stress_test.cpp. No libRack/GUI.

#include "voices.hpp"

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cmath>

using namespace stress;
using namespace crashride_impl;

namespace {

constexpr double kBound = 12.0;          // runaway-output guard (volts)
const float kSampleRates[4] = {44100.f, 48000.f, 96000.f, 192000.f};

// Tiny cursor over the fuzzer byte stream. Reads return 0 once exhausted, so a
// short input simply drives fewer events rather than reading OOB.
struct ByteReader {
    const uint8_t* p;
    size_t n;
    size_t i;
    uint8_t u8() { return i < n ? p[i++] : 0; }
    float unit() { return u8() * (1.f / 255.f); }   // byte -> [0,1]
};

// Drive one concrete adapter through the fuzzer-derived program. Templated so it
// works uniformly across all 10 adapter types (no virtual dispatch).
template <class A>
void drive(A& v, ByteReader& r) {
    int sri = 0;
    rack::Module::ProcessArgs args = makeArgs(kSampleRates[sri], 0);
    Ctrl c;
    v.fire(r.unit());

    // Each loop iteration reads one opcode byte then acts on it. Bounded by the
    // input length (ByteReader yields 0 when exhausted, and we cap total work).
    long produced = 0;
    const long kMaxSamples = 1 << 20;     // hard ceiling per input (~22 ms work)
    while (r.i < r.n && produced < kMaxSamples) {
        uint8_t op = r.u8();
        switch (op & 0x07) {
            case 0:  c.tune       = r.unit(); break;
            case 1:  c.decay      = r.unit(); break;
            case 2:  c.pitch      = r.unit(); break;
            case 3:  c.pitchDecay = r.unit(); break;
            case 4:  c.click      = r.unit(); break;
            case 5:  c.drive      = r.unit(); break;
            case 6:  c.level      = r.unit();
                     c.accent     = (op & 0x08) ? 1.f : 0.f; break;
            case 7:  // gate / sample-rate control
                if (op & 0x08) {                       // re-fire (gate edge)
                    v.fire(r.unit());
                } else {                               // switch sample rate
                    sri = (op >> 4) & 0x03;
                    args = makeArgs(kSampleRates[sri], args.frame);
                }
                break;
        }
        // Run a fuzzer-chosen block of samples with the current control state.
        int block = 1 + (r.u8() & 0x3F);               // 1..64 samples
        for (int k = 0; k < block && produced < kMaxSamples; ++k, ++produced) {
            float out = v.process(args, c);
            if (!std::isfinite(out)) __builtin_trap();        // NaN/Inf
            if (std::fabs(out) > kBound) __builtin_trap();    // runaway
            args.frame++;
        }
    }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) return 0;
    ByteReader r{data + 1, size - 1, 0};  // byte 0 selects the voice
    switch (data[0] % 10) {
        case 0: { KckAdapter v;                                       drive(v, r); break; }
        case 1: { TomAdapter v(TomFit::makeLowTom(),  "TomLow");      drive(v, r); break; }
        case 2: { TomAdapter v(TomFit::makeMidTom(),  "TomMid");      drive(v, r); break; }
        case 3: { TomAdapter v(TomFit::makeHighTom(), "TomHigh");     drive(v, r); break; }
        case 4: { CrashRideAdapter v(&crashAsset(), kCrashRomCfg, kCrashTuneOctaves,
                                     kCrashDecayMinSec, kCrashDecayMaxSec, "Crash");
                  drive(v, r); break; }
        case 5: { CrashRideAdapter v(&rideAsset(), kRideRomCfg, kRideTuneOctaves,
                                     kRideDecayMinSec, kRideDecayMaxSec, "Ride");
                  drive(v, r); break; }
        case 6: { RimClapAdapter v(&rimClapClapSource(), "Clap");     drive(v, r); break; }
        case 7: { RimClapAdapter v(&rimClapRimSource(),  "Rim");      drive(v, r); break; }
        case 8: { SnrAdapter v;                                       drive(v, r); break; }
        case 9: { ChhOhhAdapter v;                                    drive(v, r); break; }
    }
    return 0;
}
