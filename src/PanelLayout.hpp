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
 *       LEFT_COLUMN_8HP, CENTER_12HP, OUTER_RIGHT_COLUMN_12HP.
 *   - Row names are either:
 *       * shared rhythm families, e.g. COMPACT_ROWS_8HP
 *       * shared semantic bands, e.g. TOP_IO_ROW_12HP
 *   - Prefer these names directly in module widgets.
 *   - One-letter aliases like L/R/cx/ys hide intent and should be avoided.
 *
 * ┌─────────────────────────────────────────────────────────┐
 * │ Template    │ HP │  Width  │ Rows │ Use                  │
 * ├─────────────────────────────────────────────────────────┤
 * │ T_8HP_6ROW  │  8 │ 40.64mm │  6   │ Noise, Attenuate, ADSR │
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
static constexpr float PANEL_H = 128.5f;

// ── Standard widths ─────────────────────────────────────────────────────────
static constexpr float W_4HP  = 20.32f;
static constexpr float W_6HP  = 30.48f;
static constexpr float W_8HP  = 40.64f;

// ── Title bar ───────────────────────────────────────────────────────────────
static constexpr float TITLE_BAR_H_PX = 20.f;   // pixels (Rack native coords)
static constexpr float TITLE_Y_PX     = 10.f;   // center y of title text (px)

// ── 6-row grid (shared by Noise, Attenuate, ADSR, and future 6-row modules) ─
static constexpr int   ROWS         = 6;
static constexpr float ROW_Y[ROWS]  = { 26.f, 43.f, 60.f, 77.f, 94.f, 111.f };
static constexpr float ROW_SPACING  = 17.f;   // mm between row centres

// ── Column x positions for 8HP panel ────────────────────────────────────────
static constexpr float CENTER_8HP             = 20.32f;
static constexpr float LEFT_COLUMN_8HP        =  7.f;
static constexpr float RIGHT_COLUMN_8HP       = 33.64f;
static constexpr float LEFT_PAIR_COLUMN_8HP   = CENTER_8HP - 8.f;
static constexpr float RIGHT_PAIR_COLUMN_8HP  = CENTER_8HP + 8.f;

// Compatibility aliases for modules not yet migrated to the literate names.
static constexpr float CX_8HP    = CENTER_8HP;
static constexpr float LEFT_8HP  = LEFT_COLUMN_8HP;
static constexpr float MID_8HP   = CENTER_8HP;
static constexpr float RIGHT_8HP = RIGHT_COLUMN_8HP;
static constexpr float PAIR_L_8HP = LEFT_PAIR_COLUMN_8HP;
static constexpr float PAIR_R_8HP = RIGHT_PAIR_COLUMN_8HP;

// ── Compact 8HP shared row set (Sonic / Crinkle / Saphire family) ──────────
static constexpr int   ROWS_8_COMPACT = 6;
static constexpr float COMPACT_ROWS_8HP[ROWS_8_COMPACT] = {
    24.f, 41.f, 58.f, 76.f, 94.f, 112.f
};
static constexpr const float* ROW_Y_8_COMPACT = COMPACT_ROWS_8HP;

// ── Column x positions for 6HP panel ────────────────────────────────────────
static constexpr float CENTER_6HP       = 15.24f;
static constexpr float LEFT_COLUMN_6HP  =  6.f;
static constexpr float RIGHT_COLUMN_6HP = 24.5f;

static constexpr float CX_6HP    = CENTER_6HP;
static constexpr float LEFT_6HP  = LEFT_COLUMN_6HP;
static constexpr float RIGHT_6HP = RIGHT_COLUMN_6HP;

// ── Compact 6HP shared row set (Ladder / Maurizio family) ──────────────────
static constexpr int   ROWS_6_COMPACT = 7;
static constexpr float COMPACT_ROWS_6HP[ROWS_6_COMPACT] = {
    22.f, 37.f, 52.f, 67.f, 82.f, 97.f, 112.f
};
static constexpr const float* ROW_Y_6_COMPACT = COMPACT_ROWS_6HP;

// ── Screw helpers ────────────────────────────────────────────────────────────

inline void addScrews_8HP(rack::ModuleWidget* w) {
    using namespace rack;
    w->addChild(createWidget<ThemedScrew>(Vec(1 * RACK_GRID_WIDTH, 0)));
    w->addChild(createWidget<ThemedScrew>(Vec(6 * RACK_GRID_WIDTH, 0)));
    w->addChild(createWidget<ThemedScrew>(Vec(1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    w->addChild(createWidget<ThemedScrew>(Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}

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

inline rack::math::Vec panelSize_8HP() {
    return rack::math::Vec(8.f * rack::RACK_GRID_WIDTH, rack::RACK_GRID_HEIGHT);
}
inline rack::math::Vec panelSize_6HP() {
    return rack::math::Vec(6.f * rack::RACK_GRID_WIDTH, rack::RACK_GRID_HEIGHT);
}
inline rack::math::Vec panelSize_4HP() {
    return rack::math::Vec(4.f * rack::RACK_GRID_WIDTH, rack::RACK_GRID_HEIGHT);
}

// ── Standard widths ─────────────────────────────────────────────────────────
static constexpr float W_12HP = 60.96f;
static constexpr float W_14HP = 71.12f;
static constexpr float W_16HP = 81.28f;
static constexpr float W_18HP = 91.44f;

// ── Column x positions for 12HP panel (BusCrush compact: IN left, PAN right)
static constexpr float LEFT_COLUMN_12HP        = 15.f;
static constexpr float RIGHT_COLUMN_12HP       = 46.f;
static constexpr float CENTER_12HP             = 30.48f;
static constexpr float OUTER_LEFT_COLUMN_12HP  = 10.f;
static constexpr float OUTER_RIGHT_COLUMN_12HP = 51.f;

static constexpr float LEFT_12HP  = LEFT_COLUMN_12HP;
static constexpr float RIGHT_12HP = RIGHT_COLUMN_12HP;
static constexpr float CX_12HP    = CENTER_12HP;
static constexpr float OUTER_L_12HP = OUTER_LEFT_COLUMN_12HP;
static constexpr float OUTER_R_12HP = OUTER_RIGHT_COLUMN_12HP;

// ── Shared 12HP lower I/O grid (Steel / Tonnetz family) ────────────────────
static constexpr float TOP_IO_ROW_12HP     = 95.f;
static constexpr float BOTTOM_IO_ROW_12HP  = 109.f;
static constexpr float CONTROL_ROW_12HP    = 54.f;

static constexpr float ROW_IO1_12HP = TOP_IO_ROW_12HP;
static constexpr float ROW_IO2_12HP = BOTTOM_IO_ROW_12HP;
static constexpr float ROW_CTRL_12HP = CONTROL_ROW_12HP;

// ── Column x positions for 16HP panel (BusCrush HAS_CONTROLS)
//   Row layout: [amp_knob | audio_in | pan_cv | pan_knob]
static constexpr float COL1_16HP = 12.f;   // amp knob
static constexpr float COL2_16HP = 27.f;   // audio in jack
static constexpr float COL3_16HP = 54.f;   // pan CV jack
static constexpr float COL4_16HP = 69.f;   // pan knob
static constexpr float CX_16HP   = 40.64f;

// ── 8-row grid (BusCrush: 8 channels + output row) ──────────────────────────
static constexpr int   ROWS_8          = 8;
static constexpr float ROW_Y_8[ROWS_8] = { 22.f, 34.f, 46.f, 58.f, 70.f, 82.f, 94.f, 106.f };
static constexpr float ROW_OUT_Y       = 120.f;

// ── Panel size helpers ───────────────────────────────────────────────────────
inline rack::math::Vec panelSize_12HP() {
    return rack::math::Vec(12.f * rack::RACK_GRID_WIDTH, rack::RACK_GRID_HEIGHT);
}
inline rack::math::Vec panelSize_14HP() {
    return rack::math::Vec(14.f * rack::RACK_GRID_WIDTH, rack::RACK_GRID_HEIGHT);
}
inline rack::math::Vec panelSize_16HP() {
    return rack::math::Vec(16.f * rack::RACK_GRID_WIDTH, rack::RACK_GRID_HEIGHT);
}
inline rack::math::Vec panelSize_18HP() {
    return rack::math::Vec(18.f * rack::RACK_GRID_WIDTH, rack::RACK_GRID_HEIGHT);
}

// ── Screw helpers ────────────────────────────────────────────────────────────
inline void addScrews_12HP(rack::ModuleWidget* w) {
    using namespace rack;
    w->addChild(createWidget<ThemedScrew>(Vec(1 * RACK_GRID_WIDTH,  0)));
    w->addChild(createWidget<ThemedScrew>(Vec(10 * RACK_GRID_WIDTH, 0)));
    w->addChild(createWidget<ThemedScrew>(Vec(1 * RACK_GRID_WIDTH,  RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    w->addChild(createWidget<ThemedScrew>(Vec(10 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}
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
inline void addScrews_16HP(rack::ModuleWidget* w) {
    using namespace rack;
    w->addChild(createWidget<ThemedScrew>(Vec(1  * RACK_GRID_WIDTH, 0)));
    w->addChild(createWidget<ThemedScrew>(Vec(14 * RACK_GRID_WIDTH, 0)));
    w->addChild(createWidget<ThemedScrew>(Vec(1  * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    w->addChild(createWidget<ThemedScrew>(Vec(14 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}
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
    nvgBeginPath(vg); nvgRect(vg, 0, 0, size.x, TITLE_BAR_H_PX);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 180)); nvgFill(vg);
    nvgFontSize(vg, 7.f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, titleColor);
    nvgText(vg, size.x / 2.f, TITLE_Y_PX, title, nullptr);
}

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
// Used by expert / kitchen-sink variants outside the 909 suite as well.
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
