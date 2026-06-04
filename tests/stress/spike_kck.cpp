// SPIKE: prove KckEngine.hpp compiles + links into a standalone main()
// against the Rack SDK headers ALONE (no libRack.dylib). If this builds and
// runs, the foundation harness is viable.
//
// Build (see tests/stress/Makefile):
//   c++ -std=c++11 -I<RACK_SDK>/include -Isrc -Ivendor/svghelper \
//       tests/stress/spike_kck.cpp -o spike_kck

#include "KckEngine.hpp"
#include <cstdio>
#include <cmath>

int main() {
    KckVoice voice;
    KckFit::Config fit = KckFit::makeKick();

    rack::Module::ProcessArgs args;
    args.sampleRate = 48000.f;
    args.sampleTime = 1.f / 48000.f;
    args.frame = 0;

    voice.fire(0.5f);

    double peak = 0.0;
    long nonFinite = 0;
    const int N = 48000;  // 1 second
    for (int i = 0; i < N; ++i) {
        float out = voice.process(args, fit,
                                  /*tune*/ 1.0f, /*decay*/ 0.5f, /*pitch*/ 0.5f,
                                  /*pitchDecay*/ 0.5f, /*attack*/ 0.5f,
                                  /*drive*/ 0.0f, /*level*/ 0.85f);
        if (!std::isfinite(out)) ++nonFinite;
        if (std::fabs(out) > peak) peak = std::fabs(out);
        args.frame++;
    }

    std::printf("SPIKE KckEngine: ran %d samples @48k, peak=%.4f, nonFinite=%ld, active=%d\n",
                N, peak, nonFinite, (int)voice.active);
    return (nonFinite == 0) ? 0 : 1;
}
