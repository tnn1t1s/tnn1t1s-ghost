// rack_shim.cpp -- minimal stubs for the handful of non-header-only Rack/NanoVG
// symbols pulled in transitively by <rack.hpp>'s componentlibrary.hpp.
//
// The Ghost DSP engines themselves use ONLY header-only rack:: facilities
// (rack::math, rack::dsp::SchmittTrigger, rack::simd, Module::ProcessArgs).
// But including <rack.hpp> also drags in componentlibrary.hpp, whose global
// NVGcolor constants are initialized via nvgRGB()/nvgRGBA() -- functions that
// live in libnanovg, which we deliberately do NOT link (no GUI, headless).
//
// These stubs satisfy the linker with correct-but-unused color values. No DSP
// path touches them. This is the documented "minimal stub" the harness needs;
// nothing here affects audio output.

#include <nanovg.h>

extern "C" {

NVGcolor nvgRGB(unsigned char r, unsigned char g, unsigned char b) {
    NVGcolor c;
    c.r = r / 255.0f; c.g = g / 255.0f; c.b = b / 255.0f; c.a = 1.0f;
    return c;
}

NVGcolor nvgRGBf(float r, float g, float b) {
    NVGcolor c; c.r = r; c.g = g; c.b = b; c.a = 1.0f; return c;
}

NVGcolor nvgRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    NVGcolor c;
    c.r = r / 255.0f; c.g = g / 255.0f; c.b = b / 255.0f; c.a = a / 255.0f;
    return c;
}

NVGcolor nvgRGBAf(float r, float g, float b, float a) {
    NVGcolor c; c.r = r; c.g = g; c.b = b; c.a = a; return c;
}

} // extern "C"
