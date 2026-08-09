#include "detail_level.hpp"

#include "foldout_column.hpp" // canvas_rect — what a takeover takes (BL-265)
#include "presentation.hpp"   // palette::selection / text_secondary
#include "ui_state.hpp"

#include <imgui.h>

#include <algorithm>

namespace ui {

namespace {

/// Which way a caret points. Named rather than a bool because BL-265 needs four
/// directions where BL-214 needed two.
enum class caret_dir { down, up, left };

/// The caret glyph, drawn rather than typed. The atlas carries a narrow codepoint
/// set (BL-234), so `⌄ ⌃ ‹` would all render as "?" — the control builds its own
/// triangle through the draw list instead. The characters in BL-265's prose are
/// notation for the design, never string literals for the code.
void draw_caret(ImDrawList* dl, ImVec2 centre, float r, caret_dir d, ImU32 col)
{
    const float across = r * 0.75f; // half-width of the base
    const float along  = r * 0.45f; // half-depth from base to apex
    switch (d)
    {
        case caret_dir::down:
            dl->AddTriangleFilled({centre.x - across, centre.y - along},
                                  {centre.x + across, centre.y - along},
                                  {centre.x,          centre.y + along}, col);
            break;
        case caret_dir::up:
            dl->AddTriangleFilled({centre.x - across, centre.y + along},
                                  {centre.x + across, centre.y + along},
                                  {centre.x,          centre.y - along}, col);
            break;
        case caret_dir::left:
            dl->AddTriangleFilled({centre.x + along, centre.y - across},
                                  {centre.x + along, centre.y + across},
                                  {centre.x - along, centre.y         }, col);
            break;
    }
}

/// The `›` full-canvas glyph: an outlined caret pointing right, deliberately NOT
/// the filled triangle the in-place control uses. Two controls sat side by side in
/// one gutter have to differ in shape as well as in direction, or the pair reads as
/// one control drawn twice.
void draw_open_arrow(ImDrawList* dl, ImVec2 centre, float r, ImU32 col)
{
    const float across = r * 0.70f;
    const float along  = r * 0.45f;
    dl->AddLine({centre.x - along, centre.y - across}, {centre.x + along, centre.y}, col, 1.8f);
    dl->AddLine({centre.x + along, centre.y}, {centre.x - along, centre.y + across}, col, 1.8f);
}

/// One (surface, key) as a single value, for the in-place set. Keys are small
/// non-negative indices (a chain stage, a roll-up card, a pager page), so 16 bits
/// is room to spare and the packing stays trivially reversible by eye.
std::uint32_t packed(detail_surface s, int key)
{
    return (static_cast<std::uint32_t>(s) << 16) |
           (static_cast<std::uint32_t>(key) & 0xFFFFu);
}

/// The rectangle a takeover takes.
///
/// The canvas region for everything that lives inside the shell — that is the
/// whole of BL-265's change 2, and it is what leaves the header, clock, comms dock
/// and Selection band alive through a takeover.
///
/// `generation_stage` is the one exception, and it is a real one rather than a
/// special case: the New World wizard runs on its own screen BEFORE the shell
/// exists, so `canvas_rect()` there would carve an arbitrary sub-rect out of a
/// screen that has no shell column, no header and no Selection band to preserve.
/// Its takeover stays window-wide, which is what "take the main stage" means on
/// that screen.
foldout_rect takeover_rect(detail_surface s)
{
    if (s == detail_surface::generation_stage)
    {
        const ImVec2 disp = ImGui::GetIO().DisplaySize;
        return {0.0f, 0.0f, disp.x, disp.y};
    }
    return canvas_rect();
}

} // namespace

bool is_expanded(const ui_state& ui, detail_surface s, int key)
{
    return ui.expanded.surface == s && ui.expanded.key == key;
}

bool any_expanded(const ui_state& ui)
{
    return ui.expanded.surface != detail_surface::none;
}

void expand(ui_state& ui, detail_surface s, int key)
{
    ui.expanded.surface     = s;
    ui.expanded.key         = key;
    ui.expanded.focus       = true; // the opening frame raises the window, no more
    ui.corp_rollup_drill    = -1;   // ...and at its roll-up, never a stale drill
}

void fold(ui_state& ui)
{
    ui.expanded.surface  = detail_surface::none;
    ui.expanded.key      = 0;
    ui.expanded.focus    = false;
    ui.corp_rollup_drill = -1;
    // `in_place` is deliberately NOT cleared: the takeover never consulted it and
    // never wrote it, so returning restores the folded view exactly as it was.
}

bool is_open_in_place(const ui_state& ui, detail_surface s, int key)
{
    const std::uint32_t p = packed(s, key);
    return std::find(ui.expanded.in_place.begin(), ui.expanded.in_place.end(), p)
           != ui.expanded.in_place.end();
}

void set_open_in_place(ui_state& ui, detail_surface s, int key, bool open)
{
    std::vector<std::uint32_t>& set = ui.expanded.in_place;
    const std::uint32_t         p   = packed(s, key);
    const auto                  it  = std::find(set.begin(), set.end(), p);
    if (open && it == set.end())
        set.push_back(p);
    else if (!open && it != set.end())
        set.erase(it);
}

float disclosure_gutter_width()
{
    // Two frame-height slots and the spacing between them. Fixed, so the `›`
    // column lands in the same place on a row that offers `⌄` and one that does not.
    return 2.0f * ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
}

void gutter_text(unsigned int colour, const char* text)
{
    const ImVec2 cp = ImGui::GetCursorScreenPos();
    // Screen-space right edge of the content region — the same edge
    // `disclosure_controls` hangs the controls off, so the two cannot disagree
    // about where the gutter starts.
    const float right = cp.x + ImGui::GetContentRegionAvail().x;
    const float stop  = right - disclosure_gutter_width() - ImGui::GetStyle().ItemSpacing.x;

    ImGui::PushClipRect(cp, {std::max(cp.x, stop), cp.y + ImGui::GetFrameHeight()}, true);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(colour), "%s", text);
    ImGui::PopClipRect();
}

bool disclosure_controls(ui_state& ui, detail_surface s, int key, bool in_place)
{
    const float h      = ImGui::GetFrameHeight();
    const float gutter = disclosure_gutter_width();
    // The row's right edge, in the same coordinate space SameLine takes. Computed
    // from the content region rather than from the text, which is the whole point:
    // a longer title must not push the controls out of their column.
    const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;

    ImDrawList* dl      = ImGui::GetWindowDrawList();
    bool        changed = false;

    ImGui::PushID(static_cast<int>(s) * 1024 + key);

    if (in_place)
    {
        const bool open = is_open_in_place(ui, s, key);
        ImGui::SameLine(right - gutter);
        const bool clicked = ImGui::InvisibleButton("##in_place", {h, h});
        const bool hot     = ImGui::IsItemHovered();
        const ImVec2 mn    = ImGui::GetItemRectMin();
        draw_caret(dl, {mn.x + h * 0.5f, mn.y + h * 0.5f}, h * 0.30f,
                   open ? caret_dir::up : caret_dir::down,
                   hot ? palette::hover : palette::text_secondary);
        if (hot)
            ImGui::SetTooltip(open ? "Collapse" : "Expand here");
        if (clicked)
        {
            // A symmetric two-state control, so it IS a toggle (the standing rule's
            // default): clicking it while open undoes it.
            set_open_in_place(ui, s, key, !open);
            changed = true;
        }
    }

    // Always the rightmost slot, whether or not the left one was drawn.
    ImGui::SameLine(right - h);
    const bool full_clicked = ImGui::InvisibleButton("##full_canvas", {h, h});
    const bool full_hot     = ImGui::IsItemHovered();
    const ImVec2 fmn        = ImGui::GetItemRectMin();
    draw_open_arrow(dl, {fmn.x + h * 0.5f, fmn.y + h * 0.5f}, h * 0.32f,
                    full_hot ? palette::hover : palette::text_secondary);
    if (full_hot)
        ImGui::SetTooltip("Full canvas");
    if (full_clicked)
    {
        expand(ui, s, key);
        changed = true;
    }

    ImGui::PopID();
    return changed;
}

bool fold_overlay_begin(ui_state& ui, detail_surface s, int key, const char* title)
{
    if (!is_expanded(ui, s, key))
        return false;

    const foldout_rect r = takeover_rect(s);
    ImGui::SetNextWindowPos({r.x, r.y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({r.w, r.h}, ImGuiCond_Always);
    if (ui.expanded.focus)
    {
        // Raised ONCE, on the frame it opens. Per-frame focus was harmless while the
        // overlay covered the window; bounded to the canvas it would fight the
        // fold-out ledger beside it for the keyboard every frame.
        ImGui::SetNextWindowFocus();
        ui.expanded.focus = false;
    }

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar   |
        ImGuiWindowFlags_NoResize     |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoCollapse   |
        ImGuiWindowFlags_NoSavedSettings;

    // OPAQUE, and generously inset. Both were found by looking at the first
    // captures: a translucent scrim let the whole shell read through the overlay,
    // so a deliberate mode switch looked like a ghost drawn over the game, and
    // zero-inset content sat jammed into the top-left corner of a 1280 px screen.
    // A mode switch has to look like one.
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(14, 15, 19, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {36.0f, 28.0f});
    ImGui::Begin("##fold_overlay", nullptr, flags);
    ImGui::PopStyleVar();   // WindowPadding is consumed by Begin
    ImGui::PopStyleColor();

    // Header: `‹ Title`, the return control top-left immediately before the title
    // (BL-265's settled vocabulary). It acts on the whole view and means "back",
    // and back is top-left everywhere in software — including this app's own zoom
    // ladder, which the takeover is now geometrically the same kind of event as.
    const float h = ImGui::GetFrameHeight();
    const bool  back_clicked = ImGui::InvisibleButton("##fold_return", {h, h});
    const bool  back_hot     = ImGui::IsItemHovered();
    const ImVec2 bmn         = ImGui::GetItemRectMin();
    draw_caret(ImGui::GetWindowDrawList(), {bmn.x + h * 0.5f, bmn.y + h * 0.5f},
               h * 0.34f, caret_dir::left,
               back_hot ? palette::hover : palette::text_secondary);
    if (back_hot)
        ImGui::SetTooltip("Back");
    ImGui::SameLine();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", title);
    ImGui::Separator();
    ImGui::Spacing();

    if (back_clicked)
        fold(ui);
    return true;
}

void fold_overlay_end(ui_state&)
{
    ImGui::End();
}

// The per-chart question log (BL-247) used to live here: a "? Why this chart"
// toggle revealing an Answers/Because pair. REMOVED 2026-08-02 (Ben, NEEDS_REVIEW
// NR-018) — "the why shouldn't leak into the UI, players don't need us telling them
// what they ought to ask of the game." The log survives as DEVELOPMENT documentation
// only; BL-260 relocates the authored pairs (recoverable from commit d143aa4) into a
// JSON store tied to the backlog item that demanded each surface. Do not reinstate a
// draw path here without reopening NR-018.

} // namespace ui
