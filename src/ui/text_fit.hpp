#pragma once

#include "imgui.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// ui::text_fit — the one measured fit-or-wrap utility (BL-215)
//
// Every text draw that must fit a box routes through here. Each call names its
// LAYOUT.md container and a stable site key; text that does not fit is recorded
// in an overflow ledger the verify harness can fail a build on
// (verify.expect_no_clipping). The helper never shortens the source string,
// never shrinks the font, and never clips without leaving a record.
// ---------------------------------------------------------------------------

namespace ui {

/// Which of LAYOUT.md's containers a text draw belongs to. Recorded with every
/// overflow so a report names the container whose policy was broken.
enum class text_box : std::uint8_t {
    foldout_column = 1,  ///< 1 — wrap
    canvas_legend,       ///< 2 — guaranteed-fit, box grows
    selection,           ///< 3 — wrap
    header_strip,        ///< 4 — guaranteed-fit, elide-with-tooltip (also the identity tile)
    time_panel,          ///< 5 — guaranteed-fit, authored to fit
    hover_card,          ///< 6 — wrap at max width
    lens_bar,            ///< 7 — icon-only / guaranteed-fit
    nav_rail,            ///< 8 — icon-only, tooltips wrap
    table_cell,          ///< 9 — guaranteed-fit (clip + tooltip) or wrap
    chart_plot,          ///< 10 — guaranteed-fit, reflowing (BL-215)
};

enum class overflow_severity : std::uint8_t {
    elided,      ///< Fitted by eliding, full string one hover away. Sanctioned (4, 9). WARN.
    clipped,     ///< Did not fit and no recovery path existed. FAIL.
    unfittable,  ///< A wrapping box met a single token wider than the box. FAIL.
};

enum class text_align : std::uint8_t { left, right, centre };

/// Draw @p text as an ImGui item, elided to @p max_w with a tooltip carrying the
/// full string when it did not fit. Guaranteed-fit containers (2, 4, 5, 7, 9-numeric, 10).
/// @param site Stable literal key naming the draw site, e.g. "header.balance". Reported.
/// @return true when the text was elided.
bool fit_text(text_box box, const char* site, const char* text, float max_w,
              bool disabled = false);

/// Draw @p text wrapped to @p max_w as an ImGui item. Wrapping containers (1, 3, 6, 8, 9-desc).
/// Records `unfittable` when a single unbreakable token exceeds @p max_w — the only
/// way a wrapping box can still clip. Never elides.
void wrap_text(text_box box, const char* site, const char* text, float max_w);

/// Draw-list variant for hand-drawn surfaces with no ImGui item (charts.cpp, the
/// canvas legend boxes). Elides to @p max_w.
/// @param align left = starts at pos.x; right = ENDS at pos.x; centre = centred on pos.x.
/// @return the width actually drawn.
float add_fit_text(ImDrawList* dl, text_box box, const char* site,
                   ImVec2 pos, float max_w, ImU32 col, const char* text,
                   text_align align = text_align::left);

/// The width @p text needs at the current font. Containers that SIZE themselves to
/// their text (2, 5, 7) measure through this rather than CalcTextSize direct, so the
/// measurement and the report cannot disagree.
float fit_width(const char* text);

/// The pure string operation, for hand-drawn hosts that need the fitted string
/// before they know where to put it. Records nothing.
std::string fitted(const char* text, float max_w);

// --- Overflow recorder -----------------------------------------------------

struct text_overflow {
    text_box          box       = text_box::foldout_column;
    const char*       site      = "";
    std::string       text;
    std::string       frame;     ///< Capture name current when it was recorded.
    float             needed    = 0.0f;
    float             available = 0.0f;
    overflow_severity sev       = overflow_severity::elided;
};

void        set_overflow_recording(bool on);
bool        overflow_recording();
void        set_overflow_frame(const std::string& label);
const std::vector<text_overflow>& overflows();
void        clear_overflows();
std::size_t overflow_failures();                     ///< clipped + unfittable
std::size_t write_overflow_report(const char* path); ///< returns the failure count

} // namespace ui
