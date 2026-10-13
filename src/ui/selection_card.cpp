#include "selection_card.hpp"

#include "charts.hpp"           // draw_time_series — the drill-down chart
#include "presentation.hpp"     // presentation_of — resource name / colour
#include "selection.hpp"        // selection_kind_of — the open/kind gate
#include "selection_panel.hpp"  // draw_selection_content — the card body

#include "world/components.hpp" // tile_component (body-of-tile, deposits)

#include <algorithm>
#include <cstdio>
#include <vector>

namespace ui {

namespace {

constexpr float kPad      = 10.0f;
constexpr float kRounding = 6.0f;

// The drill-down view (BL-196): a resource time-series chart for the top stack
// frame — the aggregate (its body's total of that resource) as columns on the left
// axis, this tile's own series as a line on the right axis, over the shared day
// axis. Draws its own header (back / title). Returns nothing; mutates the stack
// via the back button.
void draw_resource_drill(const world& w, const resource_history_view& hist, ui_state& ui)
{
    ui_state::card_drill& frame = ui.card_stack.back(); // mutable: the scroll slider writes frame.scroll
    const std::size_t r = static_cast<std::size_t>(frame.resource);

    const auto tit = w.tiles.find(frame.tile);
    const bool tile_ok = tit != w.tiles.end() && frame.resource >= 0 &&
                         r < resource_count;
    const resource_presentation& rp =
        presentation_of(static_cast<resource_type>(tile_ok ? r : 0));

    // ── Header: [◀ Back] Resource · [x, y] ──
    const float btn = ImGui::GetFrameHeight();
    if (ImGui::Button("<", {btn, btn}))
        ui.card_stack.pop_back();            // unwind one level
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Back");
    ImGui::SameLine();
    if (tile_ok)
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                           "%s", rp.name);
    else
        ImGui::TextDisabled("\xe2\x80\x94");
    if (tile_ok)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("[%d, %d]", tit->second.grid_x, tit->second.grid_y);
    }
    // No close button: the band is always open (BL-266) — `<` unwinds the drill.
    ImGui::Separator();

    if (!tile_ok)
        return;

    // Resolve the two series from the history bundle: the body aggregate (columns,
    // left axis) and this tile's own series (line, right axis). Both share the day
    // axis; a lazily-started tile series aligns to the recent (right) end via offset.
    const std::vector<std::uint64_t>* days =
        hist.days && !hist.days->empty() ? hist.days : nullptr;
    const std::size_t points = days ? days->size() : 0;

    const std::vector<float>* body_series = nullptr;
    if (hist.body)
    {
        const auto bit = hist.body->find(tit->second.body);
        if (bit != hist.body->end())
            body_series = &bit->second[r];
    }
    const std::vector<float>* tile_series = nullptr;
    if (hist.tile)
    {
        const auto tt = hist.tile->find(frame.tile);
        if (tt != hist.tile->end())
            tile_series = &tt->second[r];
    }

    if (points == 0 || (body_series == nullptr && tile_series == nullptr) ||
        (body_series && body_series->empty() && (!tile_series || tile_series->empty())))
    {
        ImGui::Spacing();
        ImGui::TextWrapped("Recording began here \xe2\x80\x94 advance time to build "
                           "the %s history for this tile and its body.", rp.name);
        return;
    }

    // Window: 8 quarters by default (Ben's 2026-07-23), scrollable through history,
    // defaulting to the right (most recent). frame.scroll is the window's left sample
    // index; -1 tracks the most recent as time advances.
    constexpr std::size_t kWindow = 8;
    const std::size_t window    = std::min(kWindow, points);
    const std::size_t max_start = points - window;
    const std::size_t start = (frame.scroll < 0)
        ? max_start
        : std::min(static_cast<std::size_t>(frame.scroll), max_start);

    // Build the WINDOWED series. A series' global sample range is [points-size, points);
    // intersect it with the window [start, start+window) and set the in-window offset so
    // a short/late series (a lazily-tracked tile) still aligns on the shared axis.
    charts::time_series ser[2];
    std::size_t n = 0;
    const auto add = [&](const std::vector<float>* s, ImU32 col, const char* lbl,
                         bool line, bool right) {
        if (!s || s->empty()) return;
        const std::size_t sz     = s->size();
        const std::size_t gstart = points - std::min(points, sz);
        const std::size_t lo     = std::max(gstart, start);
        const std::size_t hi     = std::min(points, start + window);
        if (lo >= hi) return;
        ser[n].values     = s->data() + (lo - gstart);
        ser[n].count      = hi - lo;
        ser[n].offset     = lo - start;
        ser[n].colour     = col;
        ser[n].label      = lbl;
        ser[n].line       = line;
        ser[n].right_axis = right;
        ++n;
    };
    add(body_series, IM_COL32(90, 130, 200, 235), "Body total", false, false);
    add(tile_series, IM_COL32(150, 235, 160, 255), "This tile",  true,  true);

    const ImGuiStyle& style      = ImGui::GetStyle();
    const bool        scrollable = points > window;
    const float       slider_h   = scrollable ? ImGui::GetFrameHeight() + style.ItemSpacing.y
                                              : 0.0f;

    // Reserve the chart area as a hoverable item so the crosshair can read the mouse.
    const ImVec2 p  = ImGui::GetCursorScreenPos();
    const float  cw = ImGui::GetContentRegionAvail().x;
    const float  ch = std::max(120.0f, ImGui::GetContentRegionAvail().y - slider_h - 4.0f);
    ImGui::InvisibleButton("##ts_chart", {cw, ch});
    const ImVec2 hover = ImGui::IsItemHovered() ? ImGui::GetIO().MousePos
                                                : ImVec2{-1.0f, -1.0f};
    charts::draw_time_series(ImGui::GetWindowDrawList(), p, {p.x + cw, p.y + ch},
                             ser, n, days->data() + start, window,
                             /*log_scale=*/false, hover);

    // Horizontal scrollbar through history (left = oldest, right = most recent).
    if (scrollable)
    {
        int s = static_cast<int>(start);
        ImGui::SetNextItemWidth(cw);
        if (ImGui::SliderInt("##chart_scroll", &s, 0, static_cast<int>(max_start), ""))
            frame.scroll = s;
    }
}

} // namespace

void draw_selection_band(world& w, const recipe_registry& reg,
                         const economy_report& report,
                         const resource_history_view& history, ui_state& ui,
                         ImVec2 band_origin, ImVec2 band_size)
{
    // The band is ALWAYS open (BL-266) — there is no dismissal and no hide state.
    // It never renders selection_kind::none: with no (valid) selection it rests on
    // the player's own corporation instead. The enumerator stays (other callers
    // use it); ui.selected_entity itself is left null, so deselect — "stop looking
    // at that, back to looking at yourself" — remains representable.
    entity_id sel = ui.selected_entity;
    if (sel == null_entity || selection_kind_of(w, sel) == selection_kind::none)
        sel = w.player_entity; // the player corp exists before the first frame

    // A DEPOSIT or PLATE selection reaches here with `selected_entity` null and is
    // substituted above like any other empty selection — harmlessly, because
    // draw_selection_content dispatches on the region fields BEFORE it looks at
    // the entity and returns without reading `sel` (BL-671). Worth stating: this
    // substitution is what made a plate press show the previous card, which is
    // what NR-697 was reporting.

    // ── Placement (BL-213) ──
    // Fixed: fills the exact rect the caller computed (bottom band, between the
    // shell column and the right chrome column). No click-anchoring, no
    // clamping — the band is always the same rect regardless of where the
    // selecting click landed.
    ImGui::SetNextWindowPos(band_origin, ImGuiCond_Always);
    ImGui::SetNextWindowSize(band_size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.96f);

    // Inputs are ALLOWED (unlike the transient hover card): the band hosts action
    // buttons routed through the deferred pending_* seams. No title bar / move /
    // resize / scroll — the content owns its own inner scroll regions.
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                                 | ImGuiWindowFlags_NoResize
                                 | ImGuiWindowFlags_NoMove
                                 | ImGuiWindowFlags_NoCollapse
                                 | ImGuiWindowFlags_NoNav
                                 | ImGuiWindowFlags_NoScrollbar
                                 | ImGuiWindowFlags_NoScrollWithMouse
                                 | ImGuiWindowFlags_NoBringToFrontOnFocus
                                 | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  { kPad, kPad });

    if (ImGui::Begin("##selection_band", nullptr, flags))
    {
        // A non-empty drill stack shows the drilled view (a resource time-series
        // chart, BL-196) in place of the root selection content.
        if (ui.card_stack.empty())
        {
            // The content function draws ui.selected_entity, so the resting
            // substitution is applied for the draw and restored after — unless the
            // content itself re-pointed the selection (e.g. a tile card building button, BL-1300).
            const entity_id real_sel = ui.selected_entity;
            ui.selected_entity       = sel;
            draw_selection_content(w, reg, report, ui);
            if (ui.selected_entity == sel)
                ui.selected_entity = real_sel;
        }
        else
            draw_resource_drill(w, history, ui);
    }
    ImGui::End();

    ImGui::PopStyleVar(2);

    // NB: Esc is handled by app.cpp (it must take precedence over the system-menu
    // toggle, and unwind the drill stack one level in BL-196). The band does not
    // consume Esc itself — doing so would double-fire with the app's SDL handler.
}

} // namespace ui
