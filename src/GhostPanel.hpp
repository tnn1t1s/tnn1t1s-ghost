#pragma once
#include <rack.hpp>
#include "PanelLayout.hpp"

extern rack::Plugin* pluginInstance;

/**
 * LabArtPanel -- shared widget template for the Ghost 909 drum suite.
 *
 * Panel doctrine (909 family only, supersedes the no-label rule in
 * DESIGN_PRINCIPLES.md for this suite):
 *
 *   - cream graph-paper ground (procedural grid, not an image asset)
 *   - drafting-table marks: 2 cosmetic center screws, registration crosshair
 *     top-right, "+" bottom-center, "-" mid-right
 *   - header top-left: "909" in black, two-letter voice code in red beneath
 *   - 2 columns of knob-over-jack pairs (3 pair rows) + 1 centered LEVEL
 *   - bottom I/O strip: TRIG | ACCENT | OUT with vertical dividers
 *
 * Width standards for the 909 family:
 *   - 8HP  : controller
 *   - 12HP : single-voice main module
 *   - 14HP : paired-voice main module
 *   - 18HP : Lab module (hard cap: 16 exposed knobs + bottom I/O strip)
 *
 * The `Lab` tier is first-class, but it is still a product surface rather
 * than a raw internal dump. If a voice has more than 16 meaningful expert
 * controls, curate them instead of widening the panel again.
 */

namespace Ghost {
namespace LabArt {

using namespace rack;

// ---- geometry (mm, 12HP panel) ---------------------------------------------

// Knob column x positions (kCenter12Hp = 30.48mm).
static constexpr float kKnobLX  = 17.5f;
static constexpr float kKnobRX  = 43.5f;

// Row y positions (mm).  Pair rows are knob-over-jack; LEVEL is centered.
static constexpr float kPairY[3][2] = {
    { 32.f, 44.f },   // TUNE    / DECAY
    { 55.f, 67.f },   // PITCH   / P DECAY
    { 78.f, 90.f },   // CLICK   / DRIVE
};
static constexpr float kLevelKnobY = 100.f;
static constexpr float kLevelJackY = 112.f;

// Bottom I/O strip.
static constexpr float kSeparatorY  = 116.5f;
static constexpr float kIoLabelY    = 119.f;
static constexpr float kIoJackY     = 124.f;
// 3-jack IO row (legacy; voices without Accent B can still use this).
static constexpr float kIoTrigX     = 12.f;
static constexpr float kIoAccentX   = 30.48f;
static constexpr float kIoOutX      = 49.f;

// 4-jack IO row used by voices that have BOTH local and total accent
// inputs (per the classic 909 voice layout: BD, SD, LT, MT, HT, CH).
static constexpr float kIo4TrigX = 8.f;
static constexpr float kIo4LaccX = 22.f;
static constexpr float kIo4TaccX = 38.f;
static constexpr float kIo4OutX  = 53.f;

// Header.
static constexpr float kHeaderXMm = 5.5f;
static constexpr float kHeaderYPx = 28.f;   // "909" baseline
static constexpr float kHeaderCodeYPx = 50.f; // voice code baseline

// Grid spacing.
static constexpr float kGridMm     = 2.5f;
static constexpr float kGridSubMm  = 0.5f;

// Palette.
inline NVGcolor paperColor()    { return nvgRGB(245, 242, 230); }
inline NVGcolor gridMajorColor(){ return nvgRGBA(70,  90, 110, 38); }
inline NVGcolor gridMinorColor(){ return nvgRGBA(70,  90, 110, 16); }
inline NVGcolor inkColor()      { return nvgRGB(28,  28,  30); }
inline NVGcolor markColor()     { return nvgRGBA(180, 60, 50, 180); }

// ---- font loader ------------------------------------------------------------

/// Lazily load and cache the Inter font used across the 909 panels.
inline std::shared_ptr<rack::window::Font> interFont() {
    static std::shared_ptr<rack::window::Font> f;
    if (!f) {
        try {
            f = APP->window->loadFont(
                asset::plugin(::pluginInstance, "res/fonts/Inter-Regular.ttf"));
        } catch (...) {}
    }
    return f;
}

// ---- draw helpers -----------------------------------------------------------

/// Fill the cream ground and stroke the 2.5mm major graph-paper grid.
inline void drawGraphPaper(NVGcontext* vg, Vec size) {
    // Cream ground.
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, size.x, size.y);
    nvgFillColor(vg, paperColor());
    nvgFill(vg);

    float w_mm = size.x / RACK_GRID_WIDTH * 5.08f;
    float h_mm = size.y / RACK_GRID_HEIGHT * 128.5f;

    // Minor sub-grid (0.5mm).  Skipped: too busy at Rack zoom.  Reserved for
    // a future 0.5mm trace overlay if the panel feels empty.

    // Major grid (2.5mm).
    nvgStrokeColor(vg, gridMajorColor());
    nvgStrokeWidth(vg, 0.5f);
    nvgBeginPath(vg);
    for (float x = 0.f; x <= w_mm + 0.01f; x += kGridMm) {
        float px = mm2px(x);
        nvgMoveTo(vg, px, 0.f);
        nvgLineTo(vg, px, size.y);
    }
    for (float y = 0.f; y <= h_mm + 0.01f; y += kGridMm) {
        float py = mm2px(y);
        nvgMoveTo(vg, 0.f, py);
        nvgLineTo(vg, size.x, py);
    }
    nvgStroke(vg);
}

/// Draw the top-left "909" mark with the two-letter voice code beneath it.
inline void drawHeader(NVGcontext* vg, Vec size, const char* voiceCode) {
    auto font = interFont();
    if (!font || !font->handle) return;
    nvgFontFaceId(vg, font->handle);

    // "909" in black.
    nvgFontSize(vg, 22.f);
    nvgFillColor(vg, inkColor());
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
    nvgText(vg, mm2px(kHeaderXMm), kHeaderYPx, "909", nullptr);

    // Voice code in red.
    nvgFontSize(vg, 18.f);
    nvgFillColor(vg, markColor());
    nvgText(vg, mm2px(kHeaderXMm), kHeaderCodeYPx, voiceCode, nullptr);
}

/// Draw the two cosmetic drafting-mark screw circles (top/bottom center).
inline void drawCosmeticScrews(NVGcontext* vg, Vec size) {
    // Two extra drafting-mark screw circles at top-center and bottom-center.
    float cx = size.x / 2.f;
    float r  = 2.2f;
    for (float cy : {mm2px(3.5f), size.y - mm2px(3.5f)}) {
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, r);
        nvgFillColor(vg, nvgRGB(210, 205, 190));
        nvgFill(vg);
        nvgStrokeColor(vg, nvgRGBA(60, 60, 60, 90));
        nvgStrokeWidth(vg, 0.4f);
        nvgStroke(vg);
    }
}

/// Draw the red registration marks: crosshair, "+" and "-" drafting marks.
inline void drawRegistrationMarks(NVGcontext* vg, Vec size) {
    NVGcolor c = markColor();
    nvgStrokeColor(vg, c);
    nvgFillColor(vg, c);
    nvgStrokeWidth(vg, 0.6f);

    // Crosshair top-right.
    float cx = size.x - mm2px(5.f);
    float cy = mm2px(6.f);
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, 3.f);
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, cx - 5.f, cy);
    nvgLineTo(vg, cx + 5.f, cy);
    nvgMoveTo(vg, cx, cy - 5.f);
    nvgLineTo(vg, cx, cy + 5.f);
    nvgStroke(vg);

    // "+" bottom center.
    float pcx = size.x / 2.f;
    float pcy = size.y - mm2px(2.f);
    nvgBeginPath(vg);
    nvgMoveTo(vg, pcx - 3.f, pcy);
    nvgLineTo(vg, pcx + 3.f, pcy);
    nvgMoveTo(vg, pcx, pcy - 3.f);
    nvgLineTo(vg, pcx, pcy + 3.f);
    nvgStroke(vg);

    // "-" right edge.
    float mcx = size.x - mm2px(1.5f);
    float mcy = mm2px(22.f);
    nvgBeginPath(vg);
    nvgMoveTo(vg, mcx - 3.f, mcy);
    nvgLineTo(vg, mcx + 3.f, mcy);
    nvgStroke(vg);
}

/// Draw a centered black knob label at the given mm position.
inline void drawKnobLabel(NVGcontext* vg, const char* label, float x_mm, float y_mm) {
    auto font = interFont();
    if (!font || !font->handle) return;
    nvgFontFaceId(vg, font->handle);
    nvgFontSize(vg, 8.f);
    nvgFillColor(vg, inkColor());
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
    nvgText(vg, mm2px(x_mm), mm2px(y_mm), label, nullptr);
}

/// 3-jack IO strip: TRIG | ACCENT | OUT with separator and vertical dividers.
inline void drawIOStrip(NVGcontext* vg, Vec size) {
    // Horizontal separator.
    nvgStrokeColor(vg, inkColor());
    nvgStrokeWidth(vg, 0.5f);
    nvgBeginPath(vg);
    nvgMoveTo(vg, mm2px(4.f),          mm2px(kSeparatorY));
    nvgLineTo(vg, size.x - mm2px(4.f), mm2px(kSeparatorY));
    nvgStroke(vg);

    // Vertical dividers between TRIG | ACCENT | OUT.
    for (float x : {(kIoTrigX + kIoAccentX) * 0.5f,
                    (kIoAccentX + kIoOutX) * 0.5f}) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, mm2px(x), mm2px(kSeparatorY + 1.f));
        nvgLineTo(vg, mm2px(x), mm2px(kIoJackY + 3.5f));
        nvgStroke(vg);
    }

    // Labels.
    auto font = interFont();
    if (!font || !font->handle) return;
    nvgFontFaceId(vg, font->handle);
    nvgFontSize(vg, 7.5f);
    nvgFillColor(vg, inkColor());
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
    nvgText(vg, mm2px(kIoTrigX),   mm2px(kIoLabelY), "TRIG",   nullptr);
    nvgText(vg, mm2px(kIoAccentX), mm2px(kIoLabelY), "ACCENT", nullptr);
    nvgText(vg, mm2px(kIoOutX),    mm2px(kIoLabelY), "OUT",    nullptr);
}

/** 4-jack IO strip: TRIG | LACC | TACC | OUT. */
inline void drawIOStrip4(NVGcontext* vg, Vec size) {
    nvgStrokeColor(vg, inkColor());
    nvgStrokeWidth(vg, 0.5f);
    nvgBeginPath(vg);
    nvgMoveTo(vg, mm2px(4.f),          mm2px(kSeparatorY));
    nvgLineTo(vg, size.x - mm2px(4.f), mm2px(kSeparatorY));
    nvgStroke(vg);

    for (float x : {(kIo4TrigX + kIo4LaccX) * 0.5f,
                    (kIo4LaccX + kIo4TaccX) * 0.5f,
                    (kIo4TaccX + kIo4OutX)  * 0.5f}) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, mm2px(x), mm2px(kSeparatorY + 1.f));
        nvgLineTo(vg, mm2px(x), mm2px(kIoJackY + 3.5f));
        nvgStroke(vg);
    }

    auto font = interFont();
    if (!font || !font->handle) return;
    nvgFontFaceId(vg, font->handle);
    nvgFontSize(vg, 6.5f);
    nvgFillColor(vg, inkColor());
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
    nvgText(vg, mm2px(kIo4TrigX), mm2px(kIoLabelY), "TRIG", nullptr);
    nvgText(vg, mm2px(kIo4LaccX), mm2px(kIoLabelY), "LACC", nullptr);
    nvgText(vg, mm2px(kIo4TaccX), mm2px(kIoLabelY), "TACC", nullptr);
    nvgText(vg, mm2px(kIo4OutX),  mm2px(kIoLabelY), "OUT",  nullptr);
}

// ---- lab shell -------------------------------------------------------------

// Shared 18HP Lab grid. Every 909 Lab module uses the same four-column shell
// so expert variants stay visually related instead of growing custom layouts.
static constexpr float kLab18ColX[4] = { 14.f, 37.f, 60.f, 83.f };
static constexpr float kLab18RowY[4] = { 24.f, 47.f, 70.f, 93.f };
static constexpr float kLab18Io4X[4] = { 18.f, 38.f, 58.f, 78.f };

/// Draw the dark 18HP Lab dashboard ground with title and optional subtitle.
inline void drawLabShell(NVGcontext* vg, Vec size,
                         const char* title,
                         const char* subtitle,
                         NVGcolor background = nvgRGB(10, 8, 10),
                         NVGcolor titleColor = nvgRGBA(220, 220, 240, 220),
                         NVGcolor subtitleColor = nvgRGBA(200, 200, 220, 160)) {
    // Purposefully simpler than the cream graph-paper production shell:
    // Lab modules are for expert fitting/performance work, so the shell is a
    // dark, dense dashboard that still keeps the family title treatment.
    nvgBeginPath(vg);
    nvgRect(vg, 0.f, 0.f, size.x, size.y);
    nvgFillColor(vg, background);
    nvgFill(vg);

    nvgFontSize(vg, 7.f);
    nvgFillColor(vg, titleColor);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, mm2px(4.f), mm2px(6.f), title, nullptr);

    if (subtitle && subtitle[0]) {
        nvgFontSize(vg, 4.5f);
        nvgFillColor(vg, subtitleColor);
        nvgText(vg, mm2px(28.f), mm2px(6.f), subtitle, nullptr);
    }
}

// Main 909 modules still have some per-voice visual personality, but the dark
// shell helpers below are the standard way to keep those panels coherent while
// the family converges on a single shared panel kit.

/// Draw the dark main-voice shell ground with centered title and subtitle.
inline void drawDarkShell(NVGcontext* vg, Vec size,
                          const char* title,
                          const char* subtitle = nullptr,
                          NVGcolor background = nvgRGB(8, 8, 10),
                          NVGcolor titleColor = nvgRGBA(230, 230, 240, 230),
                          NVGcolor subtitleColor = nvgRGBA(200, 200, 215, 200)) {
    nvgBeginPath(vg);
    nvgRect(vg, 0.f, 0.f, size.x, size.y);
    nvgFillColor(vg, background);
    nvgFill(vg);

    const float cx = size.x * 0.5f;
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFontSize(vg, 8.f);
    nvgFillColor(vg, titleColor);
    nvgText(vg, cx, mm2px(8.f), title, nullptr);

    if (subtitle && subtitle[0]) {
        nvgFontSize(vg, 5.0f);
        nvgFillColor(vg, subtitleColor);
        nvgText(vg, cx, mm2px(15.f), subtitle, nullptr);
    }
}

/// Draw a horizontal divider line across the dark shell at the given y (mm).
inline void drawDarkDivider(NVGcontext* vg, Vec size, float yMm,
                            NVGcolor color = nvgRGBA(80, 95, 105, 200)) {
    nvgStrokeColor(vg, color);
    nvgStrokeWidth(vg, 0.4f);
    nvgBeginPath(vg);
    nvgMoveTo(vg, mm2px(4.f), mm2px(yMm));
    nvgLineTo(vg, size.x - mm2px(4.f), mm2px(yMm));
    nvgStroke(vg);
}

/// Draw a centered light label on the dark shell at the given mm position.
inline void drawDarkLabel(NVGcontext* vg, float xMm, float yMm,
                          const char* text,
                          float fontSize = 4.5f,
                          NVGcolor color = nvgRGBA(200, 200, 215, 200)) {
    auto font = interFont();
    if (!font || !font->handle) return;
    nvgFontFaceId(vg, font->handle);
    nvgFontSize(vg, fontSize);
    nvgFillColor(vg, color);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, mm2px(xMm), mm2px(yMm), text, nullptr);
}

// ---- top-level panel --------------------------------------------------------

/// Production cream graph-paper panel: header, marks, IO strip and knob labels.
struct Panel : rack::widget::Widget {
    const char* voiceCode = "XX";
    const char* labels[7] = { "TUNE","DECAY","PITCH","P DECAY","CLICK","DRIVE","LEVEL" };

    void draw(const DrawArgs& args) override {
        drawGraphPaper(args.vg, box.size);
        drawCosmeticScrews(args.vg, box.size);
        drawRegistrationMarks(args.vg, box.size);
        drawHeader(args.vg, box.size, voiceCode);
        drawIOStrip(args.vg, box.size);

        // Param labels above each knob.
        const float kLabelOffsetMm = 6.5f;
        drawKnobLabel(args.vg, labels[0], kKnobLX, kPairY[0][0] - kLabelOffsetMm);
        drawKnobLabel(args.vg, labels[1], kKnobRX, kPairY[0][0] - kLabelOffsetMm);
        drawKnobLabel(args.vg, labels[2], kKnobLX, kPairY[1][0] - kLabelOffsetMm);
        drawKnobLabel(args.vg, labels[3], kKnobRX, kPairY[1][0] - kLabelOffsetMm);
        drawKnobLabel(args.vg, labels[4], kKnobLX, kPairY[2][0] - kLabelOffsetMm);
        drawKnobLabel(args.vg, labels[5], kKnobRX, kPairY[2][0] - kLabelOffsetMm);
        drawKnobLabel(args.vg, labels[6], AgentLayout::kCenter12Hp,
                      kLevelKnobY - kLabelOffsetMm);
    }
};

} // namespace LabArt
} // namespace Ghost
