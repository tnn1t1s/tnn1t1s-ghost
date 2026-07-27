#pragma once
#include <rack.hpp>

/**
 * PanelLayout -- shared grid constants for Ghost module panels.
 *
 * All measurements are in millimetres unless noted. Modules declare which
 * template they use; anything that fits the same shape shares the same pixel
 * grid, so ports across adjacent modules are always horizontally aligned.
 *
 * Naming doctrine:
 *   - Width comes first in the family name: 6HP, 8HP, 12HP, 16HP.
 *   - Column names describe spatial role, not shorthand math:
 *       kLeftColumn8Hp, kCenter12Hp, kOuterRightColumn12Hp.
 *   - Row names are either:
 *       * shared rhythm families, e.g. kCompactRows8Hp
 *       * shared semantic bands, e.g. kTopIoRow12Hp
 *   - Prefer these names directly in module widgets.
 *   - One-letter aliases like L/R/cx/ys hide intent and should be avoided.
 *
 * ┌─────────────────────────────────────────────────────────┐
 * │ Template    │ HP │  Width  │ Rows │ Use                  │
 * ├─────────────────────────────────────────────────────────┤
 * │ T_8HP_6ROW  │  8 │ 40.64mm │  6   │ GHOST CTRL │
 * │ T_8HP_PAIR  │  8 │ 40.64mm │  6   │ Sonic, Saphire, Crinkle │
 * │ T_6HP_7ROW  │  6 │ 30.48mm │  7   │ Ladder, Maurizio     │
 * │ T_12HP_FREE │ 12 │ 60.96mm │  --  │ BusCrush             │
 * └─────────────────────────────────────────────────────────┘
 *
 * Adding a new template:
 *   1. Define WIDTH_mm, and any named constants below.
 *   2. Add a drawScrews_*() overload.
 *   3. Document in the table above.
 *
 * Row grid (T_8HP_6ROW):
 *   Title bar centre:  y = 10px (draws "XXX" label in 20px bar)
 *   Row centres (mm):  26, 43, 60, 77, 94, 111
 *   Spacing:           17mm between rows
 *   Top margin:        26mm  (room for title + screw)
 *   Bottom margin:     17.5mm (128.5 - 111)
 */

namespace AgentLayout {

// ── Panel heights (all modules are standard 3U = 128.5mm) ──────────────────
static constexpr float kPanelH = 128.5f;

// ── Standard widths ─────────────────────────────────────────────────────────
static constexpr float kW4Hp  = 20.32f;
static constexpr float kW6Hp  = 30.48f;
static constexpr float kW8Hp  = 40.64f;

// ── Title bar ───────────────────────────────────────────────────────────────
static constexpr float kTitleBarHPx = 20.f;   // pixels (Rack native coords)
static constexpr float kTitleYPx    = 10.f;   // center y of title text (px)

// ── 6-row grid (shared by GHOST CTRL, and future 6-row modules) ─
static constexpr int   kRows         = 6;
static constexpr float kRowY[kRows]  = { 26.f, 43.f, 60.f, 77.f, 94.f, 111.f };
static constexpr float kRowSpacing   = 17.f;   // mm between row centres

// ── Column x positions for 8HP panel ────────────────────────────────────────
static constexpr float kCenter8Hp           = 20.32f;
static constexpr float kLeftColumn8Hp       =  7.f;
static constexpr float kRightColumn8Hp      = 33.64f;
static constexpr float kLeftPairColumn8Hp   = kCenter8Hp - 8.f;
static constexpr float kRightPairColumn8Hp  = kCenter8Hp + 8.f;

// Compatibility aliases for modules not yet migrated to the literate names.
static constexpr float kCx8Hp    = kCenter8Hp;
static constexpr float kLeft8Hp  = kLeftColumn8Hp;
static constexpr float kMid8Hp   = kCenter8Hp;
static constexpr float kRight8Hp = kRightColumn8Hp;
static constexpr float kPairL8Hp = kLeftPairColumn8Hp;
static constexpr float kPairR8Hp = kRightPairColumn8Hp;

// ── Compact 8HP shared row set (Sonic / Crinkle / Saphire family) ──────────
static constexpr int   kRows8Compact = 6;
static constexpr float kCompactRows8Hp[kRows8Compact] = {
    24.f, 41.f, 58.f, 76.f, 94.f, 112.f
};
static constexpr const float* kRowY8Compact = kCompactRows8Hp;

// ── Column x positions for 6HP panel ────────────────────────────────────────
static constexpr float kCenter6Hp      = 15.24f;
static constexpr float kLeftColumn6Hp  =  6.f;
static constexpr float kRightColumn6Hp = 24.5f;

static constexpr float kCx6Hp    = kCenter6Hp;
static constexpr float kLeft6Hp  = kLeftColumn6Hp;
static constexpr float kRight6Hp = kRightColumn6Hp;

// ── Compact 6HP shared row set (Ladder / Maurizio family) ──────────────────
static constexpr int   kRows6Compact = 7;
static constexpr float kCompactRows6Hp[kRows6Compact] = {
    22.f, 37.f, 52.f, 67.f, 82.f, 97.f, 112.f
};
static constexpr const float* kRowY6Compact = kCompactRows6Hp;

// ── Screw helpers ────────────────────────────────────────────────────────────

/// Place the four corner screws for an 8HP module.
inline void addScrews_8HP(rack::ModuleWidget* w) {
    using namespace rack;
    w->addChild(createWidget<ThemedScrew>(Vec(1 * RACK_GRID_WIDTH, 0)));
    w->addChild(createWidget<ThemedScrew>(Vec(6 * RACK_GRID_WIDTH, 0)));
    w->addChild(createWidget<ThemedScrew>(Vec(1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    w->addChild(createWidget<ThemedScrew>(Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}

/// Place the four corner screws for a 6HP module.
inline void addScrews_6HP(rack::ModuleWidget* w) {
    using namespace rack;
    w->addChild(createWidget<ThemedScrew>(Vec(0,                  0)));
    w->addChild(createWidget<ThemedScrew>(Vec(4 * RACK_GRID_WIDTH, 0)));
    w->addChild(createWidget<ThemedScrew>(Vec(0,                  RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    w->addChild(createWidget<ThemedScrew>(Vec(4 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}

// ── Panel size helpers ───────────────────────────────────────────────────────
// Use RACK_GRID_WIDTH/HEIGHT directly -- mm2px(128.5) gives 379.43px which
// does NOT equal RACK_GRID_HEIGHT (380px), causing addModule to throw when
// a module is dragged in from the browser.

/// Pixel size of an 8HP panel (full 3U height).
inline rack::math::Vec panelSize_8HP() {
    return rack::math::Vec(8.f * rack::RACK_GRID_WIDTH, rack::RACK_GRID_HEIGHT);
}
/// Pixel size of a 6HP panel (full 3U height).
inline rack::math::Vec panelSize_6HP() {
    return rack::math::Vec(6.f * rack::RACK_GRID_WIDTH, rack::RACK_GRID_HEIGHT);
}
/// Pixel size of a 4HP panel (full 3U height).
inline rack::math::Vec panelSize_4HP() {
    return rack::math::Vec(4.f * rack::RACK_GRID_WIDTH, rack::RACK_GRID_HEIGHT);
}

// ── Standard widths ─────────────────────────────────────────────────────────
static constexpr float kW12Hp = 60.96f;
static constexpr float kW14Hp = 71.12f;
static constexpr float kW16Hp = 81.28f;
static constexpr float kW18Hp = 91.44f;

// ── Column x positions for 12HP panel (BusCrush compact: IN left, PAN right)
static constexpr float kLeftColumn12Hp        = 15.f;
static constexpr float kRightColumn12Hp       = 46.f;
static constexpr float kCenter12Hp            = 30.48f;
static constexpr float kOuterLeftColumn12Hp   = 10.f;
static constexpr float kOuterRightColumn12Hp  = 51.f;

static constexpr float kLeft12Hp  = kLeftColumn12Hp;
static constexpr float kRight12Hp = kRightColumn12Hp;
static constexpr float kCx12Hp    = kCenter12Hp;
static constexpr float kOuterL12Hp = kOuterLeftColumn12Hp;
static constexpr float kOuterR12Hp = kOuterRightColumn12Hp;

// ── Shared 12HP lower I/O grid (Steel / Tonnetz family) ────────────────────
static constexpr float kTopIoRow12Hp    = 95.f;
static constexpr float kBottomIoRow12Hp = 109.f;
static constexpr float kControlRow12Hp  = 54.f;

static constexpr float kRowIo112Hp = kTopIoRow12Hp;
static constexpr float kRowIo212Hp = kBottomIoRow12Hp;
static constexpr float kRowCtrl12Hp = kControlRow12Hp;

// ── Column x positions for 16HP panel (BusCrush HAS_CONTROLS)
//   Row layout: [amp_knob | audio_in | pan_cv | pan_knob]
static constexpr float kCol116Hp = 12.f;   // amp knob
static constexpr float kCol216Hp = 27.f;   // audio in jack
static constexpr float kCol316Hp = 54.f;   // pan CV jack
static constexpr float kCol416Hp = 69.f;   // pan knob
static constexpr float kCx16Hp   = 40.64f;

// ── 8-row grid (BusCrush: 8 channels + output row) ──────────────────────────
static constexpr int   kRows8           = 8;
static constexpr float kRowY8[kRows8]   = { 22.f, 34.f, 46.f, 58.f, 70.f, 82.f, 94.f, 106.f };
static constexpr float kRowOutY         = 120.f;

// ── Panel size helpers ───────────────────────────────────────────────────────
/// Pixel size of a 12HP panel (full 3U height).
inline rack::math::Vec panelSize_12HP() {
    return rack::math::Vec(12.f * rack::RACK_GRID_WIDTH, rack::RACK_GRID_HEIGHT);
}
/// Pixel size of a 14HP panel (full 3U height).
inline rack::math::Vec panelSize_14HP() {
    return rack::math::Vec(14.f * rack::RACK_GRID_WIDTH, rack::RACK_GRID_HEIGHT);
}
/// Pixel size of a 16HP panel (full 3U height).
inline rack::math::Vec panelSize_16HP() {
    return rack::math::Vec(16.f * rack::RACK_GRID_WIDTH, rack::RACK_GRID_HEIGHT);
}
/// Pixel size of an 18HP panel (full 3U height).
inline rack::math::Vec panelSize_18HP() {
    return rack::math::Vec(18.f * rack::RACK_GRID_WIDTH, rack::RACK_GRID_HEIGHT);
}

// ── Screw helpers ────────────────────────────────────────────────────────────
/// Place the four corner screws for a 12HP module.
inline void addScrews_12HP(rack::ModuleWidget* w) {
    using namespace rack;
    w->addChild(createWidget<ThemedScrew>(Vec(1 * RACK_GRID_WIDTH,  0)));
    w->addChild(createWidget<ThemedScrew>(Vec(10 * RACK_GRID_WIDTH, 0)));
    w->addChild(createWidget<ThemedScrew>(Vec(1 * RACK_GRID_WIDTH,  RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    w->addChild(createWidget<ThemedScrew>(Vec(10 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}
/// Place the four corner screws for a 14HP module (paired 909 voices).
inline void addScrews_14HP(rack::ModuleWidget* w) {
    using namespace rack;
    // Wider 909 paired voices still follow Rack's corner-screw convention.
    // Keep this as a named helper so the next paired-voice panel does not
    // silently drift back to hand-coded coordinates.
    w->addChild(createWidget<ThemedScrew>(Vec(1 * RACK_GRID_WIDTH,  0)));
    w->addChild(createWidget<ThemedScrew>(Vec(12 * RACK_GRID_WIDTH, 0)));
    w->addChild(createWidget<ThemedScrew>(Vec(1 * RACK_GRID_WIDTH,  RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    w->addChild(createWidget<ThemedScrew>(Vec(12 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}
/// Place the four corner screws for a 16HP module.
inline void addScrews_16HP(rack::ModuleWidget* w) {
    using namespace rack;
    w->addChild(createWidget<ThemedScrew>(Vec(1  * RACK_GRID_WIDTH, 0)));
    w->addChild(createWidget<ThemedScrew>(Vec(14 * RACK_GRID_WIDTH, 0)));
    w->addChild(createWidget<ThemedScrew>(Vec(1  * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    w->addChild(createWidget<ThemedScrew>(Vec(14 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}
/// Place the four corner screws for an 18HP module (909 Lab modules).
inline void addScrews_18HP(rack::ModuleWidget* w) {
    using namespace rack;
    // All 909 Lab modules are fixed at 18HP. Centralizing the screw geometry
    // keeps that promise enforceable when new Lab variants are added later.
    w->addChild(createWidget<ThemedScrew>(Vec(1  * RACK_GRID_WIDTH, 0)));
    w->addChild(createWidget<ThemedScrew>(Vec(16 * RACK_GRID_WIDTH, 0)));
    w->addChild(createWidget<ThemedScrew>(Vec(1  * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    w->addChild(createWidget<ThemedScrew>(Vec(16 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}

// ── Standard panel draw helper ───────────────────────────────────────────────
/// Draw a background (image or fallback color) plus a dark title bar with text.
inline void drawStandardPanel(NVGcontext* vg, rack::math::Vec size,
                               int imgHandle, NVGcolor fallback,
                               const char* title, NVGcolor titleColor) {
    if (imgHandle > 0) {
        NVGpaint paint = nvgImagePattern(vg, 0, 0, size.x, size.y, 0.f, imgHandle, 1.f);
        nvgBeginPath(vg); nvgRect(vg, 0, 0, size.x, size.y);
        nvgFillPaint(vg, paint); nvgFill(vg);
    } else {
        nvgBeginPath(vg); nvgRect(vg, 0, 0, size.x, size.y);
        nvgFillColor(vg, fallback); nvgFill(vg);
    }
    nvgBeginPath(vg); nvgRect(vg, 0, 0, size.x, kTitleBarHPx);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 180)); nvgFill(vg);
    nvgFontSize(vg, 7.f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, titleColor);
    nvgText(vg, size.x / 2.f, kTitleYPx, title, nullptr);
}

/// Load a panel image from a plugin asset path and draw it via drawStandardPanel.
inline void drawAssetPanel(NVGcontext* vg, rack::math::Vec size,
                           rack::Plugin* plugin, const char* assetPath,
                           NVGcolor fallback,
                           const char* title, NVGcolor titleColor) {
    int imgHandle = 0;
    try {
        auto img = APP->window->loadImage(rack::asset::plugin(plugin, assetPath));
        if (img) imgHandle = img->handle;
    } catch (...) {}
    drawStandardPanel(vg, size, imgHandle, fallback, title, titleColor);
}

// ── Generic dark Lab shell ───────────────────────────────────────────────────
/// Draw a dark dashboard shell with left-aligned title and optional subtitle.
/// Used by expert / kitchen-sink variants outside the Ghost suite as well.
inline void drawLabShell(NVGcontext* vg, rack::math::Vec size,
                         const char* title,
                         const char* subtitle,
                         NVGcolor background = nvgRGB(10, 8, 10),
                         NVGcolor titleColor = nvgRGBA(220, 220, 240, 220),
                         NVGcolor subtitleColor = nvgRGBA(200, 200, 220, 160)) {
    nvgBeginPath(vg);
    nvgRect(vg, 0.f, 0.f, size.x, size.y);
    nvgFillColor(vg, background);
    nvgFill(vg);

    nvgFontSize(vg, 7.f);
    nvgFillColor(vg, titleColor);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, rack::mm2px(4.f), rack::mm2px(6.f), title, nullptr);

    if (subtitle && subtitle[0]) {
        nvgFontSize(vg, 4.5f);
        nvgFillColor(vg, subtitleColor);
        nvgText(vg, rack::mm2px(28.f), rack::mm2px(6.f), subtitle, nullptr);
    }
}

} // namespace AgentLayout
