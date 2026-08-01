#include "selection_card.hpp"

#include "charts.hpp"           // draw_time_series — the drill-down chart
#include "detail_level.hpp"     // the fold overlay + the chart question log (BL-214/247)
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
// axis. Draws its own header (back / title / close). Returns nothing; mutates the
// stack via the back/close buttons.
void draw_resource_drill(const world& w, const resource_history_view& hist, ui_state& ui)
{
    ui_state::card_drill& frame = ui.card_stack.back(); // mutable: the scroll slider writes frame.scroll
    const std::size_t r = static_cast<std::size_t>(frame.resource);

    const auto tit = w.tiles.find(frame.tile);
    const bool tile_ok = tit != w.tiles.end() && frame.resource >= 0 &&
                         r < resource_count;
    const resource_presentation& rp =
        presentation_of(static_cast<resource_type>(tile_ok ? r : 0));

    // ── Header: [◀ Back] Resource · [x, y] ................ [x] ──
    const float bar_w = ImGui::GetContentRegionAvail().x;
    const float btn   = ImGui::GetFrameHeight();
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
    ImGui::SameLine(bar_w - btn);
    if (ImGui::Button("x", {btn, btn}))          // hide the whole card
    {
        ui.card_stack.clear();
        ui.selection_hidden_for = ui.selected_entity;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Close");
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

// The expanded view of one tile metric (BL-214): the same page the band's accordion
// rests on, given the whole screen. Nothing is withheld in the band — Ben's call was
// that a fixed 260 px rect opens showing its chart — so what the overlay adds is
// ROOM: a chart eight times taller, the reference legend unsquashed, and the space
// for the chart's own question log (BL-247), which the band has no room to carry.
//
// A drilled frame (BL-196) takes the overlay instead when one is open, so the two
// axes compose: expand then drill, or drill then expand, and either order reads.
void draw_metric_expanded(const world& w, const resource_history_view& hist, ui_state& ui)
{
    if (!ui.card_stack.empty())
    {
        draw_resource_drill(w, hist, ui);
        return;
    }

    const std::vector<tile_metric> pages = tile_metrics(w, ui.selected_entity);
    if (pages.empty())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    const int page = std::clamp(ui.card_resource_page, 0, static_cast<int>(pages.size()) - 1);
    const tile_metric& mp = pages[static_cast<std::size_t>(page)];

    // The chart takes the room it was always short of — but CAPPED. Given the whole
    // screen height it drew two 580 px ribbons, which is not more legible than the
    // band's version, only bigger. 380 px is roughly where a two-column comparison
    // stops gaining from height; the width is what this view was actually short of.
    const ImVec2 p  = ImGui::GetCursorScreenPos();
    // A two-column comparison does not grow more legible past ~560 px either —
    // draw_bars caps the columns at 34 px, so extra width is empty plot, not a
    // bigger chart. What the expanded view actually buys here is the axis room and
    // the question log the 260 px band could never carry.
    const float  cw = std::min(560.0f, ImGui::GetContentRegionAvail().x);
    const float  gh = std::min(380.0f, std::max(160.0f, ImGui::GetContentRegionAvail().y
                                                            - ImGui::GetFrameHeight() * 4.0f));
    const bool   drillable = mp.resource_index >= 0;
    if (drillable)
    {
        if (ImGui::InvisibleButton("##res_chart_full", {cw, gh}))
        {
            ui.card_stack.push_back({ui.selected_entity, mp.resource_index});
            ui.card_track_tile = ui.selected_entity;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Click for its history over time.");
    }
    else
    {
        ImGui::Dummy({cw, gh});
    }
    draw_tile_metric_chart(ImGui::GetWindowDrawList(), p, {p.x + cw, p.y + gh}, mp);

    ImGui::Spacing();
    if (drillable)
        why_note(ui,
                 "Is this tile worth extracting from, next to the best ground I have surveyed?",
                 "The top-decile column is the yield a great tile for this resource "
                 "actually returns, so the gap between the two columns is the headroom "
                 "a better site would buy.");
    else
        why_note(ui,
                 "Is this tile unusually hostile or unusually liveable for this world?",
                 "A single habitability figure means nothing on its own; against this "
                 "body's own average it says whether the tile is the exception or the rule.");
}

} // namespace

void draw_selection_band(world& w, const recipe_registry& reg,
                         const economy_report& report,
                         const resource_history_view& history, ui_state& ui,
                         ImVec2 band_origin, ImVec2 band_size)
{
    // Open iff a valid entity is selected and this selection was not dismissed.
    // This is the whole open/closed model — "stuck" and "selected" are one state
    // (SELECTION.md § Click model). The band owns the gate so the content function
    // itself never has to re-check selection_hidden_for.
    const entity_id sel = ui.selected_entity;
    if (sel == null_entity || sel == ui.selection_hidden_for)
        return;
    if (selection_kind_of(w, sel) == selection_kind::none)
        return; // stale id — nothing to show

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
            draw_selection_content(w, reg, report, ui);
        else
            draw_resource_drill(w, history, ui);
    }
    ImGui::End();

    ImGui::PopStyleVar(2);

    // ── The expanded (full-screen) view of the band's metric card (BL-214) ──
    // Drawn AFTER the band so it z-orders over it, and outside the band's style
    // scope so it takes the ordinary window padding rather than the band's tighter
    // one. Only a tile selection has a metric card, so nothing else can expand here.
    //
    // The `expanded.surface` test comes FIRST and is not merely a tidy guard:
    // tile_metrics runs a top-decile scan over every tile on the body per deposited
    // resource, so computing it here unconditionally would triple that cost every
    // frame (band + this title + the overlay body) for a view that is closed almost
    // always. The frame budget is 8 ms average (BL-249); this is the kind of
    // regression a clean build and a passing golden both sail straight past.
    if (ui.expanded.surface == detail_surface::selection_metric &&
        selection_kind_of(w, sel) == selection_kind::tile)
    {
        const std::vector<tile_metric> pages = tile_metrics(w, sel);
        const int page = pages.empty()
            ? 0 : std::clamp(ui.card_resource_page, 0, static_cast<int>(pages.size()) - 1);

        char title[96];
        std::snprintf(title, sizeof title, "%s",
                      pages.empty() ? "Tile" : pages[static_cast<std::size_t>(page)].label.c_str());

        if (fold_overlay_begin(ui, detail_surface::selection_metric, page, title))
        {
            draw_metric_expanded(w, history, ui);
            fold_overlay_end(ui);
        }
    }

    // NB: Esc is handled by app.cpp (it must take precedence over the system-menu
    // toggle, and unwind the drill stack one level in BL-196). The band does not
    // consume Esc itself — doing so would double-fire with the app's SDL handler.
}

} // namespace ui
