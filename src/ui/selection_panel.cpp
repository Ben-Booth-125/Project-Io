#include "selection_panel.hpp"

#include "charts.hpp"

#include "detail_level.hpp"   // disclosure_controls — the drill-through idiom (BL-214/BL-265)
#include "foldout_column.hpp" // shell fold-out column host (shared with the ledgers)
#include "hex_render.hpp"      // draw_tile_neighbourhood — the card's zoomed tile view
#include "icons.hpp"
#include "presentation.hpp"
#include "selection.hpp"
#include "text_fit.hpp"
#include "view_nav.hpp"

#include "world/budget_system.hpp"    // compute_building_opex / body_mean_habitability (BL-162 estimate)
#include "world/building_profit.hpp" // per-building profitability estimate (BL-074)
#include "world/economy_system.hpp" // economy_report (workforce cap, BL-069)
#include "world/market_clearing.hpp"
#include "world/construction.hpp"    // demolish path (the building element's Demolish)
#include "world/logistics.hpp"       // invalidate_logistics_caches (idle/resume flips the anchor set)
#include "world/placement_rules.hpp" // buildable-type validity + stack capacity
#include "world/recipe_registry.hpp" // recipe/economics lookups for the building element
#include "world/survey_system.hpp"
#include "world/unit_roster.hpp" // campaign hire gate + roster table (BL-324); also the Soldier card's Roster page name lookup

#include <map> // BL-434: group -> representative-candidate lookup in draw_construction_ledger

#include <imgui.h>

#include <algorithm> // std::min/clamp (build rate), std::nth_element (top-decile production)
#include <array>     // tile-metric cache (per-resource p90 slots)
#include <cmath>     // std::ceil (construction ETA), std::pow/log10/floor (nice_ceil axis)
#include <cstddef>   // std::ptrdiff_t (nth_element iterator offset)
#include <cstdio>    // std::snprintf (tile coord caption + chart labels)
#include <cfloat>    // FLT_MAX (net-profit PlotLines auto-scale)
#include <cstring>   // std::strcmp (header title/kind de-dup)
#include <string>    // ticks_label / construction_status
#include <vector>    // top-decile production sample (top_decile_production)

namespace ui {

namespace {

// --- Tile production bar-chart helpers (BL-123) ------------------------------
// The redesigned tile Selection element (docs/ui/SELECTION.md, Ben's mockup) plots,
// per resource deposited on the tile, a CLUSTERED column pair reading two numbers: (A)
// how much this tile produces, and (B) what a top-decile (90th-percentile) tile produces.
// Comparing A against the top-decile reference tells the player how close this tile is to
// a great one for that resource. Each graph sits in its own bordered container with its
// header, and the resource list always shows a scrollbar (a tile can carry more than fit).
// The placement-affordance readout (BL-071) and the build front door moved off this panel to
// the owed tile-construction panel (backlog); this panel is now tile detail + navigation.
//
// "Production" is the tile's hazard-adjusted extraction yield — deposit richness scaled
// by (1 - hazard), the same two factors run_extraction multiplies (economy_system.cpp).
// The uniform base_rate/workforce scalars are dropped: they cancel in the tile-vs-benchmark
// comparison, so this keeps the deposit-magnitude numbers Ben's mockup showed.

// nice_ceil and dotted_hline now live in ui/charts.hpp — every chart in the app
// shares one implementation rather than imitating this one.
using ui::charts::nice_ceil;

// A tile's production for resource @p r: hazard-adjusted deposit richness (the two
// tile-local factors of run_extraction's yield). Zero when the tile has no deposit.
float tile_production(const tile_component& t, std::size_t r)
{
    return t.resource_deposit[r] * (1.0f - t.hazard_level);
}

// The 90th-percentile production across every tile that carries resource @p r — the
// top-decile reference the player compares a tile against ("how close is this tile to
// a great one for this resource?"). The bottom-decile floor almost every tile beats
// was uninformative (Ben's 2026-07-15 review); the aspirational ceiling makes the
// headroom the signal. nth_element (O(N)) picks the percentile without a full sort.
float top_decile_production(const world& w, std::size_t r)
{
    std::vector<float> vals;
    vals.reserve(w.tiles.size());
    for (const auto& [id, t] : w.tiles)
        if (t.resource_deposit[r] > 0.0f)
            vals.push_back(tile_production(t, r));
    if (vals.empty())
        return 0.0f;
    const std::size_t k = static_cast<std::size_t>(0.90f * static_cast<float>(vals.size() - 1));
    std::nth_element(vals.begin(), vals.begin() + static_cast<std::ptrdiff_t>(k), vals.end());
    return vals[k];
}

// Tick-stamped cache over tile_metrics' derived inputs (BL-362). The p90s and
// the body hab/hazard averages scan every tile, yet only move on a sim tick —
// and the HQ tile is auto-selected at campaign start, so the scan ran every
// frame from frame one. Same stamp-checked idiom as BL-268's raster caches;
// read-only over `w`, so it lives file-local rather than on the world.
// tiles.size() guards world identity: a regenerated world resets the tick.
struct tile_metric_cache
{
    int         tick    = -1;
    std::size_t n_tiles = 0;
    std::array<float, resource_count> p90{};
    std::array<bool,  resource_count> p90_valid{};
    entity_id   avg_body   = null_entity;
    float       avg_hab    = 0.0f;
    float       avg_haz    = 0.0f;
};

tile_metric_cache& metric_cache_for(const world& w)
{
    static tile_metric_cache cache;
    if (cache.tick != w.current_day_tick || cache.n_tiles != w.tiles.size())
    {
        cache = {};
        cache.tick    = w.current_day_tick;
        cache.n_tiles = w.tiles.size();
    }
    return cache;
}

float cached_top_decile(const world& w, std::size_t r)
{
    tile_metric_cache& c = metric_cache_for(w);
    if (!c.p90_valid[r])
    {
        c.p90[r]       = top_decile_production(w, r);
        c.p90_valid[r] = true;
    }
    return c.p90[r];
}

// One resource's chart inside [mn, mx]: a left gutter of tick labels (0 / ceiling, plus
// a mid tick when the ceiling >= 100) with dotted gridlines, a CLUSTERED column pair
// (this tile's production beside the top-decile reference, sharing a baseline so
// the two heights compare directly), and a small two-row legend naming each value. @p
// ceiling spans the taller column.
void draw_production_chart(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
                           float tile_val, float pct_val, float ceiling,
                           ImU32 tile_col, ImU32 pct_col,
                           const char* ref_label = "Top 10%")
{
    const ui::charts::bar bars[2] = {
        { tile_val, tile_col, "Tile",    false },
        { pct_val,  pct_col,  ref_label, false },
    };
    ui::charts::draw_bars(dl, mn, mx, bars, 2, ceiling, "%.1f", 34.0f);
}

// Builds the "Input cost" segment's tooltip suffix: the recipe's non-zero
// input resources, NAMES ONLY (2026-08-15, folding the former standalone
// Inputs chart into the Expenses bar's hover — Ben's call was names, not
// quantities, since the segment already shows the aggregate credit figure).
// Shared by draw_revenue_expense_bars below; @p ri may be null (extraction
// sites have no recipe), in which case the tooltip is just the plain cost.
std::string input_basket_names(const recipe* ri)
{
    if (ri == nullptr)
        return {};
    std::string names;
    for (std::size_t in = 0; in < resource_count; ++in)
    {
        if (ri->inputs[in] <= 0.0f)
            continue;
        if (!names.empty())
            names += ", ";
        names += resource_name(static_cast<resource_type>(in));
    }
    return names;
}

// Revenue vs a labelled, SEGMENTED Expenses bar (NR-248 playtest note, 2026-08-15
// reflow): input_cost / maintenance / wages stacked in one column, each segment
// its own shade and its own hover tooltip. This is the finest real split
// building_profit.hpp supports (no revenue sub-breakdown exists to chart
// separately, logged docs/development/NEEDS_REVIEW.json). Two clustered
// columns sharing a baseline, drawn by hand (not ui::charts::draw_bars) since
// draw_bars has no notion of a stacked/segmented column.
//
// @p input_recipe: the building's active recipe (null for extraction sites) —
// folded in from the former standalone Inputs chart (2026-08-15): the "Input
// cost" segment's tooltip now also lists the basket's resource NAMES, so that
// chart's content survives without its own vertical-space budget.
void draw_revenue_expense_bars(ImDrawList* dl, ImVec2 mn, ImVec2 mx, const building_profit& p,
                               const recipe* input_recipe)
{
    const float expenses = p.input_cost + p.maintenance + p.wages;
    const float ceiling  = nice_ceil(std::max(p.revenue, expenses) > 0.0f
                                     ? std::max(p.revenue, expenses) : 1.0f);
    const float box_w = mx.x - mn.x;
    const float box_h = mx.y - mn.y;
    const float gap   = box_w / 2.0f;
    const float bw    = std::min(44.0f, gap * 0.6f);

    dl->AddRect(mn, mx, IM_COL32(70, 70, 78, 255));

    // Revenue — one plain bar.
    {
        const float  cx = mn.x + gap * 0.5f;
        const float  bh = std::max(2.0f, box_h * (p.revenue / ceiling));
        const ImVec2 b0{cx - bw * 0.5f, mx.y - bh};
        const ImVec2 b1{cx + bw * 0.5f, mx.y};
        dl->AddRectFilled(b0, b1, palette::positive, 1.5f);
        ImGui::SetCursorScreenPos(b0);
        ImGui::InvisibleButton("##rev_bar", {bw, bh});
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Revenue: %.1f", static_cast<double>(p.revenue));
    }

    // Expenses — stacked segments, bottom-up: input cost, maintenance, wages.
    {
        const float cx = mn.x + gap * 1.5f;
        struct seg { float v; ImU32 col; const char* label; };
        const seg segs[3] = {
            { p.input_cost,  IM_COL32(205, 120, 95, 255),  "Input cost"  },
            { p.maintenance, IM_COL32(200, 170, 95, 255),  "Maintenance" },
            { p.wages,       IM_COL32(160, 115, 200, 255), "Wages"       },
        };
        float y = mx.y;
        for (int i = 0; i < 3; ++i)
        {
            if (segs[i].v <= 0.0f)
                continue;
            const float  bh = std::max(1.0f, box_h * (segs[i].v / ceiling));
            const ImVec2 b0{cx - bw * 0.5f, y - bh};
            const ImVec2 b1{cx + bw * 0.5f, y};
            dl->AddRectFilled(b0, b1, segs[i].col);
            char btn_id[24];
            std::snprintf(btn_id, sizeof btn_id, "##exp_seg_%d", i);
            ImGui::SetCursorScreenPos(b0);
            ImGui::InvisibleButton(btn_id, {bw, bh});
            if (ImGui::IsItemHovered())
            {
                // The input-cost segment ALSO names the recipe's input basket
                // (names only, no quantities — Ben's call) — the former
                // standalone Inputs chart's content, folded into this hover.
                if (i == 0 && input_recipe != nullptr)
                {
                    const std::string names = input_basket_names(input_recipe);
                    if (!names.empty())
                    {
                        ImGui::SetTooltip("%s: %.1f\nInputs: %s", segs[i].label,
                                          static_cast<double>(segs[i].v), names.c_str());
                    }
                    else
                        ImGui::SetTooltip("%s: %.1f", segs[i].label, static_cast<double>(segs[i].v));
                }
                else
                    ImGui::SetTooltip("%s: %.1f", segs[i].label, static_cast<double>(segs[i].v));
            }
            y -= bh;
        }
    }
}

// --- Analog construction status (BL-095 task E) ------------------------------
// Construction is now durative *and* material-gated: each economy tick a building
// under construction advances by a RATE in [0,1] set by how much of its per-tick
// material need the local market can supply (economy_system.cpp § run_construction).
// The front door used to read this as a binary yes/no; these helpers surface it as
// an analog rate / ETA / paused status instead.

// Recompute a build's current material rate exactly as run_construction does — the
// fraction of this tick's per-material need the local market can supply, set by the
// scarcest required material and forced to 0 (paused) below 1/max_stretch. Shared by
// the prospective front door (a candidate tile + type) and the in-progress building
// card. A local mirror of the world-side formula, not a call into it, because the UI
// is const and only needs the read.
float construction_rate(const world& w, const recipe_registry& reg,
                        building_type type, entity_id tile)
{
    const building_economics& econ = reg.economics(type);
    const float duration = econ.build_duration_ticks;
    if (duration <= 0.0f)
        return 1.0f; // instant build — never material-gated

    const market_component* m = nullptr;
    const entity_id mid = market_for_tile(w, tile);
    if (mid != null_entity)
    {
        const auto mit = w.markets.find(mid);
        if (mit != w.markets.end())
            m = &mit->second;
    }

    float rate = 1.0f;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const float need = econ.resource_build_cost[r] / duration;
        if (need <= 0.0f)
            continue;
        const float avail = m ? m->supply[r] : 0.0f;
        rate = std::min(rate, avail / need);
    }
    rate = std::clamp(rate, 0.0f, 1.0f);

    const float max_stretch = reg.construction().max_stretch;
    const float pause_below  = (max_stretch > 1.0f) ? (1.0f / max_stretch) : 0.0f;
    if (rate < pause_below)
        rate = 0.0f; // paused: even the max-stretched rate can't be supplied
    return rate;
}

// A compact quarter-count label. A Tick is ~3 months, so 4 make a year (BL-095);
// years surface once the span reads better than a raw count. The display word is
// "qtr" — Tick is the internal term and stays out of player-facing text (NR-002).
std::string ticks_label(int ticks)
{
    std::string s = std::to_string(ticks) + (ticks == 1 ? " qtr" : " qtrs");
    if (ticks >= 4)
        s += " (~" + std::to_string(ticks / 4) + " yr)";
    return s;
}

// The human status for a build rate + the whole ticks of work left: paused when the
// market can't feed even the max-stretched rate; otherwise "Building..." with an ETA
// a sub-1 rate stretches (and flags as scarce).
std::string construction_status(float rate, int ticks_left)
{
    if (rate <= 0.0f)
        return "Paused - market can't supply materials";
    if (rate >= 1.0f)
        return "Building... ~" + ticks_label(ticks_left);
    const int eta = static_cast<int>(std::ceil(static_cast<float>(ticks_left) / rate));
    return "Building... ~" + ticks_label(eta) + " (materials scarce)";
}

// Colour for an analog build status: red paused, amber scarce, green on-schedule.
ImVec4 construction_status_colour(float rate)
{
    return (rate <= 0.0f) ? ImVec4{0.90f, 0.55f, 0.55f, 1.0f}   // paused
         : (rate < 1.0f)  ? ImVec4{0.85f, 0.75f, 0.45f, 1.0f}   // materials scarce
                          : ImVec4{0.55f, 0.80f, 0.55f, 1.0f};  // on schedule
}

// Headline label for a selected entity. Bodies carry a name; the other kinds
// have none, so their kind doubles as the title and the stat block supplies the
// distinguishing detail (coordinates, host body, etc.).
const char* selection_title(const world& w, selection_kind kind, entity_id id)
{
    switch (kind)
    {
        case selection_kind::body:        return w.bodies.at(id).name.c_str();
        case selection_kind::building:    return building_type_name(w.buildings.at(id).type);
        case selection_kind::tile:        return "Tile";
        case selection_kind::market:      return "Market";
        case selection_kind::unit:        return "Unit";
        case selection_kind::nation:      return w.nations.at(id).name.c_str();
        case selection_kind::corporation: return w.corporations.at(id).name.c_str();
        case selection_kind::none:        return "Nothing";
    }
    return "?";
}

// BL-068 competitor layout: the Selection panel for a *rival* building. Public
// facts only (type, owner, location) plus explicit teaching rows that render the
// withheld channels (production, stockpile) as visible 'private' placeholders, so
// the asymmetry is legible rather than a silent omission. The panel never leaks
// more than the hover card's facts; markets remain the sanctioned public channel.
void draw_rival_building_summary(const world& w, entity_id id)
{
    const auto it = w.buildings.find(id);
    if (it == w.buildings.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    const building_component& b = it->second;

    // Owner corporation (public) — the building type already heads the panel.
    const auto cit = w.corporations.find(owner_corp_of(w, id));
    if (cit != w.corporations.end())
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                           "Owner: %s", cit->second.name.c_str());

    // Tile location (public — the marker already sits on the surveyed map).
    const auto tile_it = w.tiles.find(b.tile);
    if (tile_it != w.tiles.end())
        ImGui::Text("Tile [%d, %d]  %s", tile_it->second.grid_x, tile_it->second.grid_y,
                    composition_name(tile_it->second.composition));

    // Withheld channels as explicit 'private' teaching rows: the grey value makes
    // the asymmetry legible rather than a silent omission. Markets stay the
    // sanctioned public channel, so the player infers output from price, not here.
    ImGui::TextDisabled("Production:  private");
    ImGui::TextDisabled("Stockpile:   private");
}

// Survey section for a selected body (BL-067). Keyed on the body's survey phase:
// hidden → a Dispatch button with the cost + ETA preview (disabled, with a reason,
// when the player cannot afford it); in_transit / scanning → live progress + ETA;
// surveyed → a settled note. The button only enqueues ui.pending_survey_dispatch;
// app::render performs the debit + schedule against its mutable world.
void draw_survey_section(const world& w, ui_state& ui, entity_id body_id)
{
    const auto bit = w.bodies.find(body_id);
    if (bit == w.bodies.end() || bit->second.type == body_type::star)
        return;
    const survey_state& s = bit->second.survey;

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Survey");

    switch (s.phase)
    {
        case survey_phase::surveyed:
            ImGui::TextDisabled("Surveyed.");
            break;
        case survey_phase::in_transit:
            ImGui::Text("En route - ETA %d d", survey_eta_days(w, body_id));
            break;
        case survey_phase::scanning:
            ImGui::Text("Surveying %d/%d - ETA %d d",
                        s.regions_done, s.regions_total, survey_eta_days(w, body_id));
            break;
        case survey_phase::hidden:
        {
            const float cost = survey_cost(w, body_id);
            const int   eta  = survey_eta_days(w, body_id);
            float balance = 0.0f;
            const auto pit = w.corporations.find(w.player_entity);
            if (pit != w.corporations.end())
                balance = pit->second.balance;
            const bool affordable = balance >= cost;

            ImGui::BeginDisabled(!affordable);
            if (ImGui::Button("Dispatch Survey"))
                ui.pending_survey_dispatch = body_id;
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("%.0f cr - ETA %d d", static_cast<double>(cost), eta);
            if (!affordable)
                ImGui::TextDisabled("Insufficient funds.");
            break;
        }
    }
}

// Commercial-activity section for a selected body (BL-089). The activity fog is
// independent of the survey fog: this reads the player's *commercial* reach, keyed
// on body_activity_visibility. Unknown → outside the network; known/visible → a
// coarse public market pulse (no internals — production/stockpiles stay private);
// known_stale → a greyed "gone cold" note.
void draw_activity_section(const world& w, entity_id body_id)
{
    const auto bit = w.bodies.find(body_id);
    if (bit == w.bodies.end() || bit->second.type == body_type::star)
        return;

    const activity_vis av = body_activity_visibility(w, body_id, w.current_day_tick);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Commercial activity");

    if (av == activity_vis::unknown)
    {
        ImGui::TextDisabled("Outside your trade network - no market data.");
        return;
    }
    if (av == activity_vis::known_stale)
    {
        ImGui::TextDisabled("Route gone cold - last market read is stale.");
        return;
    }

    // Known / visible: a coarse public market pulse from throughput (supply+demand)
    // across the body's markets. Deliberately imprecise — a strategy read, not a
    // ledger; internals stay private (BL-068).
    float throughput = 0.0f;
    for (const auto& [mid, mk] : w.markets)
    {
        if (mk.body != body_id)
            continue;
        for (std::size_t r = 0; r < resource_count; ++r)
            throughput += mk.supply[r] + mk.demand[r];
    }
    const char* level = (throughput > 400.0f) ? "busy"
                      : (throughput > 120.0f) ? "steady"
                                              : "quiet";
    ImGui::Text("Market pulse: %s", level);
    if (av == activity_vis::visible)
        ImGui::TextDisabled("Live lane / your presence.");
}

} // namespace

// --- The tile metric pages (BL-214) ------------------------------------------
// Lifted out of draw_tile_selection so the in-band accordion and the full-screen
// fold overlay chart the SAME page list. Two call sites building the list
// separately is exactly how the two would drift, and the pager index is the fold
// key — "expand this metric" is only addressable if both agree what page N is.

std::vector<tile_metric> tile_metrics(const world& w, entity_id tile_id)
{
    std::vector<tile_metric> pages;
    const auto tit = w.tiles.find(tile_id);
    if (tit == w.tiles.end())
        return pages;
    const tile_component& tile = tit->second;

    // Every deposited resource: this tile's hazard-adjusted yield against the
    // top-decile tile for that resource.
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (tile.resource_deposit[r] <= 0.0f)
            continue;
        const float a   = tile_production(tile, r);
        const float p90 = cached_top_decile(w, r);
        const resource_presentation& rp = presentation_of(static_cast<resource_type>(r));
        pages.push_back({ rp.name, a, p90, "Top 10%",
                          nice_ceil(std::max(a, p90) > 0.0f ? std::max(a, p90) : 1.0f),
                          static_cast<int>(r) });
    }

    // Then the tile's own scalars against the body average, so a barren tile still
    // has something to page through. Cached per body on the same tick stamp.
    tile_metric_cache& c = metric_cache_for(w);
    if (c.avg_body != tile.body)
    {
        float sum_hab = 0.0f, sum_haz = 0.0f;
        int   n = 0;
        for (const auto& [id, t] : w.tiles)
            if (t.body == tile.body) { sum_hab += t.habitability; sum_haz += t.hazard_level; ++n; }
        c.avg_hab  = n > 0 ? sum_hab / static_cast<float>(n) : 0.0f;
        c.avg_haz  = n > 0 ? sum_haz / static_cast<float>(n) : 0.0f;
        c.avg_body = tile.body;
    }
    pages.push_back({ "Habitability", tile.habitability, c.avg_hab, "Body avg", 1.0f, -1 });
    pages.push_back({ "Hazard",       tile.hazard_level, c.avg_haz, "Body avg", 1.0f, -1 });

    return pages;
}

void draw_tile_metric_chart(ImDrawList* dl, ImVec2 mn, ImVec2 mx, const tile_metric& m)
{
    constexpr ImU32 tile_col = IM_COL32(150, 235, 160, 255); // this tile's value (green)
    constexpr ImU32 ref_col  = IM_COL32(150, 160, 190, 255); // reference value (muted)
    draw_production_chart(dl, mn, mx, m.tile_val, m.ref_val, m.ceiling,
                          tile_col, ref_col, m.ref_label);
}

// Per-building profitability readout (BL-074, chart-based since the 2026-08-15
// playtest rework): the selected player building's estimated per-tick economics,
// now charted rather than printed as text — Ben's call was "the charts should
// speak for themselves", so the former "Profitability (est. / qtr)" title line
// is gone. Realised last-tick figures; revenue/inputs are estimates (the pooled
// market resists exact per-building attribution — see building_profit.hpp).
void draw_building_profit(const world& w, const recipe_registry& reg,
                          const economy_report& report, entity_id id)
{
    const building_profit p = estimate_building_profit(w, reg, report, id);
    if (!p.has_data)
    {
        ImGui::TextDisabled("Run an economy quarter to estimate.");
        return;
    }

    ImDrawList* dl      = ImGui::GetWindowDrawList();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float box_w   = ImGui::GetContentRegionAvail().x;

    // The recipe whose input basket the Expenses bar's "Input cost" segment
    // names on hover (extraction_site has no recipe, so this stays null there).
    const recipe* input_recipe = nullptr;
    if (const auto bit = w.buildings.find(id);
        bit != w.buildings.end() && bit->second.type == building_type::processing_facility)
        input_recipe = reg.get_recipe(bit->second.recipe);

    // Vertical budget (2026-08-15 playtest reflow — "everything on one screen,
    // no scrollbar"; the standalone Inputs strip folded into the Expenses
    // hover the same session, freeing its budget back to the bars+line row).
    // GetContentRegionAvail().y here IS the accordion page body's remaining
    // height (draw_building_selection_body's ##building_page_body child), so
    // this is the real budget — but budgeting the FULL amount left the page's
    // own content occasionally a few px taller than its scroll-child (font
    // metrics/padding rounding), which was enough to pop a scrollbar back in
    // for content that otherwise fit. Budgeting 90% of it leaves deliberate
    // slack so that never happens (Ben's playtest call: no scrollbar, ever).
    const float total_h  = ImGui::GetContentRegionAvail().y * 0.90f;
    const float label_h  = ImGui::GetTextLineHeightWithSpacing();
    const float chart_h  = std::max(40.0f, total_h - label_h);

    // Top row: Revenue/Expenses bars take the LEFT THIRD, the 6-month net-profit
    // line takes the RIGHT TWO-THIRDS — side by side, not stacked (Ben's
    // playtest call: the vertical stack needed scrolling to see it all).
    const float left_w  = box_w / 3.0f - style.ItemSpacing.x * 0.5f;
    const float right_w = box_w - left_w - style.ItemSpacing.x;

    {
        ImGui::BeginGroup();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Revenue / Expenses");
        const ImVec2 mn = ImGui::GetCursorScreenPos();
        const ImVec2 mx = {mn.x + left_w, mn.y + chart_h};
        draw_revenue_expense_bars(dl, mn, mx, p, input_recipe);
        ImGui::Dummy({left_w, chart_h});
        ImGui::EndGroup();
    }
    ImGui::SameLine();
    {
        ImGui::BeginGroup();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Net, 6 mo.");

        // No per-building profit HISTORY is tracked (the pooled market
        // attribution problem means nothing to reconstruct a past series
        // from), so — same honest-placeholder idiom
        // draw_building_workforce_page's trend graph already used before
        // this rework — a smooth deterministic series anchored to the live
        // net estimate. 6 points for "6 months" (a Tick is ~3 months, so 6
        // points read as the last two Ticks' worth stretched to the span).
        constexpr int N = 6;
        float         series[N];
        const float   net = p.net();
        for (int i = 0; i < N; ++i)
        {
            const float t   = static_cast<float>(i) / (N - 1);
            const float dip = -0.15f * std::fabs(net) * std::sin(t * 3.14159265f * 1.4f);
            series[i]       = net + dip;
        }
        ImGui::PlotLines("##net_profit", series, N, 0, nullptr, FLT_MAX, FLT_MAX, {right_w, chart_h});
        ImGui::EndGroup();
    }
}

namespace {

// A small kind glyph for the header (BL-093) — a coloured primitive so each
// selection kind reads at a glance in place of the former placeholder portrait
// column. BL-090 promotes the corporation and building kinds to the shared corp
// emblem (shape + identity colour), so the header reads *whose* it is and matches
// the identity card and the on-canvas markers; body/tile keep their plain
// primitives. @p id is the selected entity, used to resolve the owning corp.
void draw_selection_icon(const world& w, ImDrawList* dl, selection_kind kind,
                         entity_id id, ImVec2 c, float r)
{
    const ImU32 col = palette::selection;
    switch (kind)
    {
        case selection_kind::body:
            dl->AddCircleFilled(c, r, col, 20);
            break;
        case selection_kind::corporation:
            // The selected entity *is* the corporation.
            icons::corp_emblem(dl, c, r, palette::corp_emblem_shape(id),
                               palette::corp_identity_colour(id, w.player_entity));
            break;
        case selection_kind::building:
        {
            // The owning corp's emblem (player or rival — ownership is public,
            // BL-068). Falls back to the plain square if the owner won't resolve.
            const entity_id owner = owner_corp_of(w, id);
            if (owner != null_entity)
                icons::corp_emblem(dl, c, r, palette::corp_emblem_shape(owner),
                                   palette::corp_identity_colour(owner, w.player_entity));
            else
                dl->AddRectFilled({c.x - r, c.y - r}, {c.x + r, c.y + r}, col, 2.0f);
            break;
        }
        case selection_kind::tile:
            dl->AddRect({c.x - r, c.y - r}, {c.x + r, c.y + r}, col, 2.0f, 0, 2.0f);
            break;
        default:
            dl->AddNgonFilled(c, r, col, 5);
            break;
    }
}

// The hero: the ONE primary action for this selection kind (BL-093). Actions are
// the panel's reason to exist, so they lead; reference detail is the ledgers' job.
void draw_selection_action(const world& w, const recipe_registry& reg,
                           ui_state& ui, selection_kind kind)
{
    const entity_id sel = ui.selected_entity;
    switch (kind)
    {
        // NOTE: selection_kind::tile no longer routes here — a selected tile takes
        // the dedicated vertical layout (draw_tile_selection, BL-123), not the
        // action|facts split. The remaining kinds keep the action|facts form until
        // they get their own mockups.
        case selection_kind::body:
        {
            const auto bit = w.bodies.find(sel);
            const bool star = (bit != w.bodies.end() && bit->second.type == body_type::star);
            if (!star && bit != w.bodies.end() &&
                bit->second.survey.phase != survey_phase::surveyed)
            {
                draw_survey_section(w, ui, sel);   // the move is to survey it
            }
            else if (!star)
            {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Surface");
                if (ImGui::Button("Go to surface"))
                    focus_on_entity(w, ui, sel);   // surveyed — the move is to descend
            }
            break;
        }

        // selection_kind::building no longer routes here (this rework) — it takes
        // the dedicated 3-column band (draw_building_selection_body), same as tile.

        // selection_kind::unit no longer routes here (this rework) — it takes
        // the dedicated 3-column band (draw_unit_selection_body), same shape
        // as the building/tile cards.

        case selection_kind::market:
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Locate");
            if (ImGui::Button("Go to"))
                focus_on_entity(w, ui, sel);
            break;

        case selection_kind::nation:
        case selection_kind::corporation:
            ImGui::TextDisabled("Open its ledger via [>].");
            break;

        default:
            break;
    }
}

// --- BL-431: production method selector -------------------------------------
//
// Chain trace and Depth readout are gone (2026-08-15 playtest rework): Chain's
// input-basket content folded into the Profitability page as a chart
// (draw_input_basket_chart above); Depth was cut outright. Method remains the
// one accordion page from the original BL-431 toggle trio.

// "Which way should this building make its output?" — every era-allowed
// recipe for this building side by side with its input basket, so the BL-430
// switch trade-off is legible BEFORE committing, the way the Build door shows
// a credit total before placing. Calls try_switch_recipe (economy_system.hpp)
// exactly as construction_panel.cpp's management dropdown already does —
// never assigns b.recipe / b.active_recipe_index directly, so a refused switch
// (cooldown / funds) leaves the building's state untouched.
//
// Formerly a hide/reveal toggle (BL-431); now one PAGE of the building card's
// accordion (this rework), so the whole comparison list renders unconditionally
// while the page is showing — there is nothing left inside it to fold.
//
// One narrow column (2026-08-15 second playtest pass — supersedes this same
// session's earlier 2-column square-tile grid): each method is a compact row
// ~1.5 button-heights tall (name + expected profit on one line, a smaller
// glyph Switch control at the row's right edge) rather than a big square
// tile. "Same amount of scroll but much more legible" — Ben's call was a
// slimmer strip, not a denser fit.
//
// Cross-group retraction (BL-434 follow-up, same session): the candidate
// list is now filtered to the ACTIVE recipe's own `group` — a cross-group
// recipe is refused outright by try_switch_recipe (economy_system.cpp) as of
// this same pass, so it must never be offered as a choice here either.
//
// tile_icon_button / glyph_swap are defined further down this same anonymous
// namespace (beside the rest of the action-grid glyph vocabulary); forward
// declared here rather than reordering the whole file.
bool tile_icon_button(const char* id, ImVec2 sz, bool enabled, const char* tip,
                      void (*glyph)(ImDrawList*, ImVec2, float, ImU32), ImU32 glyph_col_override = 0);
void glyph_swap(ImDrawList* dl, ImVec2 c, float r, ImU32 col);

void draw_production_method_section(world& w, const recipe_registry& reg, entity_id id)
{
    const auto bit = w.buildings.find(id);
    if (bit == w.buildings.end())
        return;
    building_component& b = bit->second;
    if (b.type != building_type::processing_facility)
        return;

    const recipe* cur = reg.get_recipe(b.recipe);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Method");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", cur ? cur->display_name.c_str() : "-");

    // Retooling progress bar (Ben's playtest note: a switch that's on cooldown
    // read as simply "not possible", with the ticks-remaining count buried in a
    // hover-only tooltip on the disabled Switch glyph). Visible whenever a
    // cooldown is running, independent of the candidate list below it, since
    // it's informative even with only one same-group method on screen.
    const int cooldown_total = std::max(1, static_cast<int>(reg.recipe_switch().cooldown_ticks));
    if (b.recipe_switch_cooldown > 0)
    {
        const float frac = 1.0f - static_cast<float>(b.recipe_switch_cooldown) /
                                   static_cast<float>(cooldown_total);
        char overlay[48];
        std::snprintf(overlay, sizeof overlay, "Retooling - %d tick%s left",
                      b.recipe_switch_cooldown, b.recipe_switch_cooldown == 1 ? "" : "s");
        ImGui::ProgressBar(frac, {ImGui::GetContentRegionAvail().x, 0.0f}, overlay);
    }

    // Same-group filter (BL-434 cross-group retraction, this session): a
    // cross-group recipe is no longer a legal switch target at all, so it
    // must never even be OFFERED as a choice. `cur == nullptr` (no resolvable
    // group, e.g. mid-construction) falls back to every era-allowed recipe —
    // the same no-group fallback try_switch_recipe itself uses.
    const int total_n = reg.recipe_count(b.type);
    std::vector<int> candidates;
    candidates.reserve(static_cast<std::size_t>(total_n));
    for (int i = 0; i < total_n; ++i)
    {
        const recipe& ri = reg.recipe_at(b.type, i);
        if (cur == nullptr || ri.group == cur->group)
            candidates.push_back(i);
    }

    if (candidates.size() <= 1)
    {
        ImGui::TextDisabled("Only one method available.");
        return;
    }

    // One narrow column, each row ~1.5 button-heights tall (see the function
    // doc comment above for the rationale).
    ImDrawList* dl      = ImGui::GetWindowDrawList();
    // Full available width, capped (NR-255). This was `avail * 0.62` — an
    // arbitrary factor that left the row 160 px to fit 248 px of content, so the
    // name got 16 px against the 103 px "Food Rations" needs and the profit was
    // painted straight over it. The 2026-08-15 "slimmer strip, not a denser fit"
    // call was about row HEIGHT (1.5 button-heights, against the square tiles it
    // replaced) and explicitly argues AGAINST cramming, so the width had no
    // reason to be held back.
    const float row_w   = std::min(ImGui::GetContentRegionAvail().x, 280.0f);
    const float row_h   = ImGui::GetFrameHeight() * 1.5f;
    const float glyph_w = row_h; // the Switch control's own square, right edge of the row

    for (int idx : candidates)
    {
        const recipe& ri     = reg.recipe_at(b.type, idx);
        const bool    active = (cur != nullptr && ri.name == cur->name);
        ImGui::PushID(idx);

        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const ImVec2 p1 = {p0.x + row_w, p0.y + row_h};
        dl->AddRectFilled(p0, p1, IM_COL32(20, 22, 28, 255), 3.0f);
        dl->AddRect(p0, p1, active ? palette::selection : IM_COL32(70, 70, 78, 255), 3.0f, 0,
                   active ? 2.0f : 1.0f);

        ImGui::BeginChild("##method_row", {row_w, row_h}, false, ImGuiWindowFlags_NoScrollbar);

        // Pip + name + profit share one line, so the PROFIT IS MEASURED FIRST and
        // the name is fitted to whatever is genuinely left (NR-255). Before this
        // the name drew at full length and the right-aligned profit was painted
        // over the top of it — "Food Rations" with "+7.0/tick" printed through it,
        // both of the row's load-bearing values illegible at once.
        const float  pipr         = row_h * 0.24f;
        const float  name_x       = 6.0f + pipr * 2.0f + 6.0f;
        const float  profit_col_w = row_w - glyph_w - 6.0f;

        // Expected profit — same estimator the construction ledger ranks
        // candidates with (estimate_prospective_profit), so a switch's payoff
        // reads the same way a fresh build's does. `&b` prices it at the
        // building's REAL staffing (not the hypothetical 0.5/100 default) and
        // excludes it from double-counting its own extraction-site stack rank.
        constexpr float profit_scale = 1.1f;
        const building_profit fp = estimate_prospective_profit(
            w, reg, b.tile, b.type, b.target_resource, reg.recipe_id(ri.name), &b);

        char        profit_buf[32];
        ImU32       profit_col = palette::neutral;
        bool        profit_dim = false;
        if (fp.has_data)
        {
            const float net = fp.net();
            profit_col = (net < 0.0f) ? palette::negative
                       : (net > 0.0f) ? palette::positive
                                      : palette::neutral;
            std::snprintf(profit_buf, sizeof profit_buf, "%+.1f/tick",
                          static_cast<double>(net));
        }
        else
        {
            std::snprintf(profit_buf, sizeof profit_buf, "no market");
            profit_dim = true;
        }

        // Measure at the scale it will actually be drawn at — a width taken at
        // 1.0x would under-reserve and put the overlap straight back.
        ImGui::SetWindowFontScale(profit_scale);
        const float profit_w = ImGui::CalcTextSize(profit_buf).x;
        ImGui::SetWindowFontScale(1.0f);

        // Resource pip, vertically centred.
        ImGui::SetCursorPos({6.0f, row_h * 0.5f - pipr});
        const ImVec2 gp = ImGui::GetCursorScreenPos();
        icons::resource(dl, {gp.x + pipr, gp.y + pipr}, pipr, primary_output_resource(ri));

        // Name — kept at the prior rework's larger scale (1.15x), now elided to
        // the space the profit column leaves rather than overrunning it. Routed
        // through ui::fit_text so a name that does not fit is elided with the
        // full string one hover away AND recorded in BL-215's overflow ledger,
        // instead of silently clipping. Box 9 (table_cell): this row is a fixed
        // single line, so eliding is the sanctioned recovery, not wrapping.
        constexpr float name_gap   = 8.0f;
        const float     name_max_w = std::max(16.0f, profit_col_w - profit_w - name_gap - name_x);

        constexpr float name_scale = 1.15f;
        ImGui::SetWindowFontScale(name_scale);
        ImGui::SetCursorPos({name_x, (row_h - ImGui::GetFontSize()) * 0.5f});
        ImGui::PushStyleColor(ImGuiCol_Text,
                              active ? ImGui::ColorConvertU32ToFloat4(palette::selection)
                                     : ImGui::GetStyle().Colors[ImGuiCol_Text]);
        ui::fit_text(ui::text_box::table_cell, "selection.method.name",
                     ri.display_name.c_str(), name_max_w);
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);

        // Profit, right-aligned, ending just before the Switch glyph's column.
        ImGui::SetWindowFontScale(profit_scale);
        ImGui::SetCursorPos({profit_col_w - profit_w, (row_h - ImGui::GetFontSize()) * 0.5f});
        if (profit_dim)
            ImGui::TextDisabled("%s", profit_buf);
        else
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(profit_col), "%s", profit_buf);
        ImGui::SetWindowFontScale(1.0f);

        // The input/wage/rate detail line is gone (2026-08-15 playtest
        // rework): now implied by the Profitability page's Expenses hover
        // instead — a row shows just name + profit + the Switch control.

        if (!active)
        {
            const bool  on_cooldown = b.recipe_switch_cooldown > 0;

            // BL-428: the same chain-depth gate the Build door and
            // try_switch_recipe apply, shown rather than discovered by a refused
            // click. Ancient-band methods only, matching the first-cut scope.
            const std::uint16_t row_id = reg.recipe_id(ri.name);
            bool depth_locked = false;
            if (ri.era == era_band::ancient)
            {
                const auto cit = w.corporations.find(w.player_entity);
                const int  need = reg.recipe_required_depth(row_id);
                const int  have = (cit != w.corporations.end())
                                      ? corp_reached_depth(cit->second, reg) : 0;
                depth_locked = (need < 0 || need > have);
            }

            char tip[96];
            if (depth_locked)
                // Names the act, not the number — a reached-depth integer means
                // nothing to a player who has never seen one.
                std::snprintf(tip, sizeof tip,
                              "Your industry cannot make what this needs yet");
            else if (on_cooldown)
                std::snprintf(tip, sizeof tip, "Locked %d more tick%s", b.recipe_switch_cooldown,
                             b.recipe_switch_cooldown == 1 ? "" : "s");
            else if (reg.recipe_switch().switch_cost > 0.0f)
                std::snprintf(tip, sizeof tip, "Switch to this method - %.0f cr",
                             static_cast<double>(reg.recipe_switch().switch_cost));
            else
                std::snprintf(tip, sizeof tip, "Switch to this method");

            // Selection tone, a shade darker than palette::hover's accent blue —
            // Ben's playtest call: the Switch glyph should read as an interactive
            // accent, not the same neutral grey every other glyph button uses.
            constexpr ImU32 switch_glyph_col = IM_COL32(90, 150, 210, 255);
            ImGui::SetCursorPos({row_w - glyph_w, 0.0f});
            if (tile_icon_button("##switch", {glyph_w, row_h}, !on_cooldown && !depth_locked,
                                 tip, glyph_swap, switch_glyph_col))
                try_switch_recipe(w, reg, w.player_entity, b, row_id);
        }

        ImGui::EndChild();
        ImGui::PopID();
    }
}

// The facts: only what informs the action (BL-093) — slim and muted. Everything
// encyclopedic (orbit, composition, deposits, prices) lives in the ledgers, one
// 'go to' away.
void draw_selection_facts(world& w, const recipe_registry& reg,
                          const economy_report& report, ui_state& ui, selection_kind kind)
{
    const entity_id sel = ui.selected_entity;
    switch (kind)
    {
        // selection_kind::tile is handled by draw_tile_selection (BL-123), not here.
        case selection_kind::body:
            draw_activity_section(w, sel);          // BL-089: commercial pulse
            break;
        // selection_kind::building no longer routes here — see
        // draw_building_selection_body.
        default:
            break;
    }
}

// --- Action-strip glyphs (tile card + building card) -------------------------
// Compact glyphs for the right-hand icon grids (Ben's 2026-07-23 layout, and
// this rework's building-card grid). Drawn centred at @p c within radius @p r,
// matching the (dl, centre, r, colour) contract the ui::icons vocabulary uses.
// Construct/Manage/History are drawn here; Supply reuses ui::icons::supply.
// Kept local for now; promote to ui::icons if they stick.
constexpr float kTwoPiLocal = 6.2831853f;

void glyph_hammer(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddRectFilled({c.x - r * 0.55f, c.y - r * 0.7f}, {c.x + r * 0.7f, c.y - r * 0.28f}, col, 1.5f);
    dl->AddLine({c.x - r * 0.05f, c.y - r * 0.35f}, {c.x - r * 0.45f, c.y + r * 0.75f}, col, 2.2f);
}

void glyph_gear(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    for (int i = 0; i < 8; ++i)
    {
        const float a = kTwoPiLocal * static_cast<float>(i) / 8.0f;
        const ImVec2 d{std::cos(a), std::sin(a)};
        dl->AddLine({c.x + d.x * r * 0.42f, c.y + d.y * r * 0.42f},
                    {c.x + d.x * r * 0.72f, c.y + d.y * r * 0.72f}, col, 2.0f);
    }
    dl->AddCircle(c, r * 0.42f, col, 14, 2.0f);
    dl->AddCircleFilled(c, r * 0.16f, col);
}

void glyph_clock(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddCircle(c, r * 0.72f, col, 18, 2.0f);
    dl->AddLine(c, {c.x, c.y - r * 0.5f}, col, 2.0f);
    dl->AddLine(c, {c.x + r * 0.42f, c.y + r * 0.08f}, col, 2.0f);
}

// A generic humanoid placeholder — the Soldier card's left-column image. No
// unit-type-keyed glyph vocabulary exists yet (unlike buildings' ui::icons::
// building), so this is deliberately honest scaffolding: a filled circle
// "head" over a triangle "body", in the same simple-primitive idiom as
// glyph_hammer/glyph_gear above rather than a second icon system.
void glyph_soldier(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddCircleFilled({c.x, c.y - r * 0.42f}, r * 0.26f, col, 14);
    const ImVec2 top{c.x, c.y - r * 0.05f};
    const ImVec2 bl{c.x - r * 0.48f, c.y + r * 0.72f};
    const ImVec2 br{c.x + r * 0.48f, c.y + r * 0.72f};
    dl->AddTriangleFilled(top, bl, br, col);
}

// A simple right-pointing chevron — the unit card's "Go to" action glyph
// (matches the header's own ">" go-to button in shape, not just meaning).
void glyph_goto(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddLine({c.x - r * 0.35f, c.y - r * 0.55f}, {c.x + r * 0.35f, c.y}, col, 2.4f);
    dl->AddLine({c.x + r * 0.35f, c.y}, {c.x - r * 0.35f, c.y + r * 0.55f}, col, 2.4f);
}

// A faint dot — the neutral "nothing assigned yet" glyph for a reserved slot
// in an action grid (BL-213 follow-up, 2026-07-28; reused unchanged by this
// rework's building-card grid).
void glyph_reserved(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddCircle(c, r * 0.30f, col, 12, 1.5f);
}

// Two opposed arrows — the Method page's big Switch glyph (2026-08-15
// playtest rework), replacing the former small text "Switch" button. Sized to
// be the tile's dominant visual element via tile_icon_button's own sizing.
void glyph_swap(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    const float y1 = c.y - r * 0.32f;
    dl->AddLine({c.x - r * 0.6f, y1}, {c.x + r * 0.55f, y1}, col, 2.4f);
    dl->AddLine({c.x + r * 0.55f, y1}, {c.x + r * 0.2f, y1 - r * 0.3f}, col, 2.4f);
    dl->AddLine({c.x + r * 0.55f, y1}, {c.x + r * 0.2f, y1 + r * 0.3f}, col, 2.4f);

    const float y2 = c.y + r * 0.32f;
    dl->AddLine({c.x + r * 0.6f, y2}, {c.x - r * 0.55f, y2}, col, 2.4f);
    dl->AddLine({c.x - r * 0.55f, y2}, {c.x - r * 0.2f, y2 - r * 0.3f}, col, 2.4f);
    dl->AddLine({c.x - r * 0.55f, y2}, {c.x - r * 0.2f, y2 + r * 0.3f}, col, 2.4f);
}

// A box with a line through it — the building card's Mothball toggle glyph
// (2026-08-15; the Lifecycle page's Close/Reopen moved onto the action grid
// under this name). Deliberately not glyph_reserved's dot: Mothball is a real
// action, not a placeholder.
void glyph_mothball(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddRect({c.x - r * 0.6f, c.y - r * 0.45f}, {c.x + r * 0.6f, c.y + r * 0.45f}, col, 1.5f, 0, 2.0f);
    dl->AddLine({c.x - r * 0.75f, c.y}, {c.x + r * 0.75f, c.y}, col, 2.2f);
}

// The mothballed-state counterpart to glyph_mothball above (2026-08-15,
// distinct glyph for the toggle's two states — a closed box read as "reopen"
// too easily). Same box, but its lid is lifted open (top edge raised and
// tilted) and the line-through is gone — an open crate rather than a
// closed/lined-out one, same honest-primitive idiom as the box's own shape.
void glyph_reopen(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    // Box body (bottom three sides only — the open top is implied by the lid).
    dl->AddLine({c.x - r * 0.6f, c.y - r * 0.15f}, {c.x - r * 0.6f, c.y + r * 0.45f}, col, 2.0f);
    dl->AddLine({c.x + r * 0.6f, c.y - r * 0.15f}, {c.x + r * 0.6f, c.y + r * 0.45f}, col, 2.0f);
    dl->AddLine({c.x - r * 0.6f, c.y + r * 0.45f}, {c.x + r * 0.6f, c.y + r * 0.45f}, col, 2.0f);
    // Lifted lid, hinged at the box's back (left) corner and tilted open.
    dl->AddLine({c.x - r * 0.6f, c.y - r * 0.15f}, {c.x + r * 0.75f, c.y - r * 0.6f}, col, 2.2f);
}

// A circular refresh arrow — the building card's Auto (workforce autosolve)
// toggle glyph (2026-08-15; relocated here from the Workforce page's old
// text button). An open ring with one arrowhead reads as "recompute me each
// tick" at a glance, matching the honest-primitive idiom of the other
// action-grid glyphs (no second icon system for one button).
void glyph_auto(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    constexpr float start = -0.65f * kTwoPiLocal;
    constexpr float end   = 0.15f * kTwoPiLocal;
    dl->PathArcTo(c, r * 0.55f, start, end, 16);
    dl->PathStroke(col, 0, 2.2f);
    // Arrowhead at the arc's start, two short strokes fixed relative to the
    // tip rather than a full tangent calc — a small icon, not a vector plot.
    const ImVec2 tip{c.x + r * 0.55f * std::cos(start), c.y + r * 0.55f * std::sin(start)};
    dl->AddLine(tip, {tip.x - r * 0.28f, tip.y - r * 0.05f}, col, 2.2f);
    dl->AddLine(tip, {tip.x - r * 0.05f, tip.y + r * 0.28f}, col, 2.2f);
}

// A simple X — the building card's Dismantle glyph (2026-08-15; the
// Lifecycle page's Dismantle moved onto the action grid under this name).
void glyph_dismantle(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddLine({c.x - r * 0.55f, c.y - r * 0.55f}, {c.x + r * 0.55f, c.y + r * 0.55f}, col, 2.4f);
    dl->AddLine({c.x + r * 0.55f, c.y - r * 0.55f}, {c.x - r * 0.55f, c.y + r * 0.55f}, col, 2.4f);
}

// A square icon button: an ImGui::Button frame with a glyph drawn over it and a
// hover tooltip (Ben's call: icons, text only on hover). Disabled buttons dim the
// glyph and show the reason. Returns true only on an enabled click. Shared by the
// tile card's grid and this rework's building-card grid.
bool tile_icon_button(const char* id, ImVec2 sz, bool enabled, const char* tip,
                      void (*glyph)(ImDrawList*, ImVec2, float, ImU32), ImU32 glyph_col_override)
{
    ImGui::BeginDisabled(!enabled);
    const ImVec2 p       = ImGui::GetCursorScreenPos();
    const bool   clicked = ImGui::Button(id, sz);
    ImGui::EndDisabled();
    // glyph_col_override lets one call site (Method's Switch button, a "selection
    // tone" per Ben's playtest note) tint its glyph without touching every other
    // glyph button's neutral grey — 0 means "use the default", so every existing
    // caller is unaffected.
    const ImU32 gcol = !enabled ? IM_COL32(110, 110, 116, 255)
                     : (glyph_col_override != 0) ? glyph_col_override
                                                  : IM_COL32(215, 215, 220, 255);
    glyph(ImGui::GetWindowDrawList(), {p.x + sz.x * 0.5f, p.y + sz.y * 0.5f},
          std::min(sz.x, sz.y) * 0.5f, gcol);
    if (ImGui::IsItemHovered() && tip)
        ImGui::SetTooltip("%s", tip);
    return clicked && enabled;
}

} // namespace

// --- Building Selection card: 3-column band (supersedes BL-431) -------------
//
// The building card now takes the SAME band shape as the tile card
// (draw_tile_selection): a zoomed tile render, a paged accordion, and an
// action grid — rather than the action|facts split every other kind still
// uses. The former BL-431 three toggles (Method / Chain / Depth) become
// PAGES of one accordion instead of independently-foldable sections; a page
// shows its whole content while it is the current page, matching the tile
// card's metric pages.
//
// Rival (non-player-owned) buildings take this SAME 3-column shape rather
// than a separate simpler path: the zoomed tile render is public (the
// marker already sits on the surveyed map, BL-068) and a read-only Status
// page is still a meaningful thing to look at — internals just never make
// it onto a page (no Profitability/Method/Chain/Depth for a rival, since
// those would leak private production data the hover-card/BL-068 asymmetry
// is built to withhold). Only Manage is gated off in the action grid.
//
// building_page_kind / building_page / building_pages / draw_building_page are
// declared in selection_panel.hpp (not anonymous-namespace-local) so the
// full-canvas takeover (selection_card.cpp) can read the SAME page list and
// dispatch the SAME draw as the in-band accordion — the tile card's own
// tile_metrics / draw_tile_metric_chart split is the precedent.

// Every accordion page that applies to @p id, in pager order. Mirrors
// tile_metrics' role: the one list both the in-band accordion and the
// full-canvas takeover read, so they cannot show different pages for the
// same building.
std::vector<building_page> building_pages(const world& w, const recipe_registry& reg,
                                          const economy_report& report, entity_id id)
{
    std::vector<building_page> pages;
    const auto bit = w.buildings.find(id);
    if (bit == w.buildings.end())
        return pages;
    const building_component& b = bit->second;

    // Rivals: intel only (BL-068) — a single read-only Status page, nothing
    // that would reveal recipe choice or internals.
    if (!is_player_owned(w, id))
    {
        pages.push_back({"Status", building_page_kind::status});
        return pages;
    }

    // Profitability needs a completed building earning a real per-tick figure;
    // under construction there is nothing yet to estimate (BL-074's has_data
    // guard would just print "Run an economy quarter" on every single frame
    // of every build, which is not a page worth a pager slot).
    if (b.ticks_remaining <= 0)
    {
        const building_profit p = estimate_building_profit(w, reg, report, id);
        if (p.has_data)
            pages.push_back({"Profitability", building_page_kind::profitability});
    }

    if (b.type == building_type::processing_facility)
        pages.push_back({"Method", building_page_kind::method});

    // Chain and Depth are gone (2026-08-15 playtest rework — see
    // building_page_kind's doc comment). Lifecycle is gone as a PAGE too: its
    // Mothball/Dismantle controls moved onto the action grid
    // (draw_building_selection_body), so Workforce is the only page left
    // beyond Profitability/Method/Status.
    pages.push_back({"Workforce", building_page_kind::workforce});

    // Nothing applied (e.g. a freshly-placed infra building with no owner
    // resolved yet) — an honest fallback rather than an empty accordion.
    if (pages.empty())
        pages.push_back({"Status", building_page_kind::status});

    return pages;
}

// A minimal facts/status page: construction progress when still building,
// or the rival's public-only summary (BL-068) — the fallback content for
// whichever building has no other page, plus the whole story for a rival.
void draw_building_status_page(const world& w, const recipe_registry& reg, entity_id id)
{
    if (!is_player_owned(w, id))
    {
        draw_rival_building_summary(w, id);
        return;
    }
    const auto bit = w.buildings.find(id);
    if (bit == w.buildings.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    const building_component& b = bit->second;
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Status");
    ImGui::Text("%s", building_type_name(b.type));
    if (b.ticks_remaining > 0)
    {
        const float rate = construction_rate(w, reg, b.type, b.tile);
        ImGui::TextColored(construction_status_colour(rate), "%s",
                           construction_status(rate, b.ticks_remaining).c_str());
    }
    else
        ImGui::TextDisabled("Operating.");
}

// Workforce page (2026-08-15 playtest rework, trimmed). The "Workforce"
// subheading and the 0/20/40/60/80/100 tier grid are gone — the tier grid was
// tall enough to trigger the accordion's vertical scrollbar, which this page
// alone didn't need. The placeholder trend graph is unchanged (no real
// per-building history is recorded yet, same constraint as before); the tier
// grid is replaced by a single horizontal 1% slider below the Auto button,
// carrying the same "a manual edit pins the target and clears Auto" semantic
// the tier buttons had.
void draw_building_workforce_page(building_component& b)
{
    constexpr int N       = 9;
    const float   graph_w = ImGui::GetContentRegionAvail().x;

    float wf_series[N];
    const float wf = static_cast<float>(b.workforce_target);
    for (int i = 0; i < N; ++i)
    {
        const float t   = static_cast<float>(i) / (N - 1);
        const float dip = -20.0f * std::sin(t * 3.14159265f); // placeholder dip-and-recover
        wf_series[i]    = std::clamp(wf + dip, 0.0f, 200.0f);
    }

    ImGui::PlotLines("##workforce", wf_series, N, 0, nullptr, 0.0f, 120.0f, {graph_w, 60.0f});

    // Auto moved to the action grid (2026-08-15 playtest rework, below
    // Mothball — draw_building_selection_body) — this page is trend chart +
    // slider only now. Its OLD button here was the "game breaking bug": the
    // label baked `workforce_target` straight into the id-bearing text
    // (`"Auto  (%d%%)"`), and with no `##` separator ImGui derives a button's
    // ID from its full label — so every tick solve_workforce_target changed
    // the target, the button's ID churned too, corrupting ImGui's
    // hover/active/focus identity for it frame over frame. The action-grid
    // replacement keeps the id stable ("##bld_auto") and shows the percentage
    // as separate adjacent text, never inside the id string.

    int target = b.workforce_target;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::SliderInt("##wf_slider", &target, 0, 100, "%d%%"))
    {
        b.workforce_target = target;
        b.workforce_auto   = false; // manual edit pins the target, same as a tier press used to
    }
}

// Dispatch one accordion page's content. Mutable world because the Method
// page's Switch button calls try_switch_recipe directly (matching the
// management ledger's own call, BL-430) — everything else here only reads.
// `ui` is unused by any current case (the Lifecycle page's deferred Dismantle
// request moved to the action grid, draw_building_selection_body) but stays in
// the signature — building_page_expanded's caller and the tile-card precedent
// both pass ui_state& through the dispatch, and a future page may need it.
void draw_building_page(world& w, const recipe_registry& reg, const economy_report& report,
                        entity_id id, building_page_kind kind, ui_state& /*ui*/)
{
    switch (kind)
    {
        case building_page_kind::profitability: draw_building_profit(w, reg, report, id); break;
        case building_page_kind::method:        draw_production_method_section(w, reg, id); break;
        case building_page_kind::status:        draw_building_status_page(w, reg, id); break;
        case building_page_kind::workforce:
            if (const auto bit = w.buildings.find(id); bit != w.buildings.end())
                draw_building_workforce_page(bit->second);
            break;
    }
}

// --- Unit (Soldier) Selection card: 3-column band, same family as building/tile ---
//
// Placeholder scaffolding (BL-393 notes units are largely inert in the live
// economy today) — Ben's explicit direction was to build the card shape now
// rather than wait. Two pages, both reading REAL unit_component fields: no
// fabricated numbers.

// unit_component::type indexes unit_roster_table() directly — corp_command.cpp's
// hire_unit sets it from that same table's index (`cmd.unit_type`), so this is
// a real name lookup, not a guess. An out-of-range index (stale save, a unit
// raised before a roster-table edit) falls back to the honest "Type %u" the
// rest of the codebase already uses for opaque indices, rather than a name
// that might not correspond to what the unit actually is.
std::string unit_roster_display_name(std::uint16_t type)
{
    const auto& table = unit_roster_table();
    if (static_cast<std::size_t>(type) < table.size())
        return table[type].name;
    char buf[24];
    std::snprintf(buf, sizeof buf, "Type %u", static_cast<unsigned>(type));
    return buf;
}

std::vector<unit_page> unit_pages(const world& w, entity_id id)
{
    std::vector<unit_page> pages;
    if (w.units.find(id) == w.units.end())
        return pages;
    pages.push_back({"Strength", unit_page_kind::strength});
    pages.push_back({"Roster",   unit_page_kind::roster});
    return pages;
}

// Strength page: the DERIVED strength, its two multipliers, and what the unit
// costs to keep.
//
// The NEEDS_REVIEW note this comment used to carry is resolved: strength was a
// stored duplicate of `count` (every writer set the two equal), and BL-459
// removed the stored field. It is computed now — count x the roster row's
// quality x the unit's supply factor, x100 fixed point — so the page shows the
// derivation rather than one opaque number the player cannot account for.
//
// BL-454 adds the upkeep line beneath it: a unit's per-tick credit wage and its
// goods draw. Both read the SAME resolve_unit_upkeep the budget and the unit
// pass read, so the card cannot disagree with the money.
void draw_unit_strength_page(const world& w, const recipe_registry& reg, const unit_component& u)
{
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Strength");
    ImGui::Text("%lld", static_cast<long long>(unit_strength(w, u)));
    ImGui::TextDisabled("%d x %.2f quality x %.2f supply",
                        u.count,
                        roster_type_quality_permille(u.type) / 1000.0f,
                        u.supply_factor_permille / 1000.0f);

    ImGui::Spacing();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Count");
    ImGui::Text("%d", u.count);

    // Supply: called out only when it is NOT full, so a healthy unit spends no
    // space on a line that always says the same thing.
    if (u.supply_factor_permille < 1000)
    {
        ImGui::Spacing();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Supply");
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                           "%.0f%% — unsupplied, weakening",
                           u.supply_factor_permille / 10.0f);
    }

    // Upkeep (BL-454): the credit wage plus every good actually drawn. Silent
    // when no rate is authored, which is the shipped default — an all-zero
    // upkeep line would be chrome that says nothing.
    const unit_upkeep_draw d = resolve_unit_upkeep(u, reg.military().upkeep);
    if (d.credits > 0.0f || d.any_goods)
    {
        ImGui::Spacing();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Upkeep / tick");
        if (d.credits > 0.0f)
            ImGui::Text("%.1f cr", static_cast<double>(d.credits));
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (d.goods[r] <= 0.0f)
                continue;
            ImGui::Text("%.2f %s",
                        static_cast<double>(d.goods[r]),
                        resource_name(static_cast<resource_type>(r)));
        }
    }
}

// Roster page: type name (real lookup, see unit_roster_display_name) + owner
// (resolved via the same corp-name lookup the building card's rival summary
// and the tile card's Manage button already use).
void draw_unit_roster_page(const world& w, const unit_component& u)
{
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Type");
    ImGui::Text("%s", unit_roster_display_name(u.type).c_str());
    ImGui::Spacing();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Owner");
    const auto cit = w.corporations.find(u.owner);
    ImGui::Text("%s", cit != w.corporations.end() ? cit->second.name.c_str() : "\xe2\x80\x94");
}

void draw_unit_page(const world& w, const recipe_registry& reg, entity_id id, unit_page_kind kind)
{
    const auto uit = w.units.find(id);
    if (uit == w.units.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    switch (kind)
    {
        // BL-454: the strength page now reads the registry, for the upkeep line.
        case unit_page_kind::strength: draw_unit_strength_page(w, reg, uit->second); break;
        case unit_page_kind::roster:   draw_unit_roster_page(w, uit->second);        break;
    }
}

namespace {

// The unit card body: same 3-column shape as draw_building_selection_body —
// left placeholder image, centre paged accordion, right action grid — but
// read-only (a unit is not yet operable) and with only "Go to" wired for real
// in the action grid, the rest reserved (matching the tile/building cards'
// own reserved-slot idiom).
void draw_unit_selection_body(const world& w, const recipe_registry& reg, ui_state& ui)
{
    const entity_id sel = ui.selected_entity;
    const auto uit = w.units.find(sel);
    if (uit == w.units.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    const ImGuiStyle& style   = ImGui::GetStyle();
    ImDrawList*        dl      = ImGui::GetWindowDrawList();
    const float         avail   = ImGui::GetContentRegionAvail().x;
    const float         total_h = ImGui::GetContentRegionAvail().y;
    const float         spacing = style.ItemSpacing.x;
    const float         left_w  = avail * 0.25f;
    const float         right_w = avail * 0.25f;
    const float       center_w  = std::max(80.0f, avail - left_w - right_w - 2.0f * spacing);

    // ── Left quarter: placeholder soldier glyph ──
    {
        const ImVec2 p  = ImGui::GetCursorScreenPos();
        const ImVec2 mx = {p.x + left_w, p.y + total_h};
        dl->AddRectFilled(p, mx, IM_COL32(16, 18, 24, 255), 3.0f);
        const ImVec2 gc = {(p.x + mx.x) * 0.5f, (p.y + mx.y) * 0.5f};
        const float  gr = std::min(left_w, total_h) * 0.5f - 10.0f;
        glyph_soldier(dl, gc, std::max(4.0f, gr), IM_COL32(190, 200, 210, 255));
        dl->AddRect(p, mx, IM_COL32(90, 90, 100, 255), 3.0f);
        ImGui::Dummy({left_w, total_h});
    }
    ImGui::SameLine();

    // ── Centre half: paged accordion over unit_pages() ──
    {
        const std::vector<unit_page> pages = unit_pages(w, sel);

        ImGui::BeginChild("##unit_accordion", {center_w, total_h}, true,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

        int&      page = ui.selection_unit_page;
        const int n    = static_cast<int>(pages.size());
        page = std::clamp(page, 0, std::max(0, n - 1));

        if (n == 0)
        {
            ImGui::TextDisabled("\xe2\x80\x94");
        }
        else
        {
            const unit_page& up = pages[static_cast<std::size_t>(page)];

            const float frame_h = ImGui::GetFrameHeight();
            const float aw      = ImGui::GetContentRegionAvail().x;
            const float right   = ImGui::GetCursorPosX() + aw;
            ImGui::BeginDisabled(page == 0);
            if (ImGui::ArrowButton("##unit_prev", ImGuiDir_Left)) --page;
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered() && page > 0)
                ImGui::SetTooltip("Previous");

            char hdr[64];
            std::snprintf(hdr, sizeof hdr, "%s  (%d/%d)", up.label.c_str(), page + 1, n);
            const float name_w = ui::fit_width(hdr);
            ImGui::SameLine(std::max(frame_h + style.ItemSpacing.x, (aw - name_w) * 0.5f));
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", hdr);

            ImGui::SameLine(right - 2.0f * frame_h - style.ItemSpacing.x);
            ImGui::BeginDisabled(page == n - 1);
            if (ImGui::ArrowButton("##unit_next", ImGuiDir_Right)) ++page;
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered() && page < n - 1)
                ImGui::SetTooltip("Next");

            ImGui::Spacing();
            ImGui::BeginChild("##unit_page_body", {0.0f, 0.0f}, false,
                              ImGuiWindowFlags_NoSavedSettings);
            draw_unit_page(w, reg, sel, up.kind);
            ImGui::EndChild();
        }

        ImGui::EndChild();
    }
    ImGui::SameLine();

    // ── Right quarter: 2x3 action grid — only "Go to" is real today ──
    {
        ImGui::BeginChild("##unit_actions", {right_w, total_h}, false,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
        const float  bw  = (right_w - spacing) / 2.0f;
        const float  bh  = (total_h - 2.0f * spacing) / 3.0f;
        const ImVec2 bsz = {bw, bh};

        if (tile_icon_button("##unit_goto", bsz, /*enabled=*/true, "Go to", glyph_goto))
            focus_on_entity(w, ui, sel);
        ImGui::SameLine();
        tile_icon_button("##unit_reserved1", bsz, /*enabled=*/false, "Reserved", glyph_reserved);

        tile_icon_button("##unit_reserved2", bsz, /*enabled=*/false, "Reserved", glyph_reserved);
        ImGui::SameLine();
        tile_icon_button("##unit_reserved3", bsz, /*enabled=*/false, "Reserved", glyph_reserved);

        tile_icon_button("##unit_reserved4", bsz, /*enabled=*/false, "Reserved", glyph_reserved);
        ImGui::SameLine();
        tile_icon_button("##unit_reserved5", bsz, /*enabled=*/false, "Reserved", glyph_reserved);

        ImGui::EndChild();
    }
}

// The building card body: left quarter zoomed tile render, centre half paged
// accordion, right quarter action grid — the same three-region shape AND the
// same proportions as draw_tile_selection (normalised 2026-08-15, Ben: the
// two cards should read as one family, not two similar-but-different ones).
// Called from draw_selection_content AFTER its generic header — a building
// keeps that shared header (icon/title/kind/'go to'), unlike the tile card,
// which draws its own.
void draw_building_selection_body(world& w, const recipe_registry& reg,
                                  const economy_report& report, ui_state& ui)
{
    const entity_id sel = ui.selected_entity;
    const auto bit = w.buildings.find(sel);
    if (bit == w.buildings.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    building_component&       building = bit->second; // mutable: the action grid's Mothball flips decommissioned directly
    const ImGuiStyle&         style    = ImGui::GetStyle();
    ImDrawList*                dl       = ImGui::GetWindowDrawList();
    const float                avail    = ImGui::GetContentRegionAvail().x;
    const float                total_h  = ImGui::GetContentRegionAvail().y;
    const float                spacing  = style.ItemSpacing.x;
    const float                left_w   = avail * 0.25f; // normalised to the tile card's split
    const float                right_w  = avail * 0.25f;
    const float                center_w = std::max(80.0f, avail - left_w - right_w - 2.0f * spacing);

    // ── Left quarter: a generic placeholder image, keyed by building type ──
    // Was the zoomed tile-neighbourhood render; Ben's call (2026-08-15) was
    // "generic per building type" over one flat image for everything — the
    // codebase already has a type-keyed glyph vocabulary (ui::icons::building,
    // the same silhouette the Build door and the canvas markers use), so this
    // draws THAT, enlarged to fill the panel, rather than inventing a second
    // image system. Identity resolution mirrors the on-canvas marker's own
    // (body_surface_canvas.cpp): a processing facility's ACTIVE recipe's
    // primary output, falling back to target_resource for everything else
    // (extraction sites, and infrastructure types the glyph ignores anyway).
    {
        const ImVec2 p  = ImGui::GetCursorScreenPos();
        const ImVec2 mx = {p.x + left_w, p.y + total_h};
        dl->AddRectFilled(p, mx, IM_COL32(16, 18, 24, 255), 3.0f);

        const resource_type identity =
            (building.type == building_type::processing_facility &&
             reg.get_recipe(building.recipe) != nullptr)
                ? primary_output_resource(*reg.get_recipe(building.recipe))
                : building.target_resource;
        const ImVec2 gc = {(p.x + mx.x) * 0.5f, (p.y + mx.y) * 0.5f};
        const float  gr = std::min(left_w, total_h) * 0.5f - 10.0f;
        icons::building(dl, gc, std::max(4.0f, gr), building.type, identity,
                        IM_COL32(190, 200, 210, 255));

        dl->AddRect(p, mx, IM_COL32(90, 90, 100, 255), 3.0f);
        ImGui::Dummy({left_w, total_h});
    }
    ImGui::SameLine();

    // ── Centre half: paged accordion over building_pages() ──
    {
        const std::vector<building_page> pages = building_pages(w, reg, report, sel);

        ImGui::BeginChild("##building_accordion", {center_w, total_h}, true,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

        int&      page = ui.selection_building_page;
        const int n    = static_cast<int>(pages.size());
        page = std::clamp(page, 0, n - 1);
        const building_page& bp = pages[static_cast<std::size_t>(page)];

        const float frame_h = ImGui::GetFrameHeight();
        const float aw      = ImGui::GetContentRegionAvail().x;
        const float right   = ImGui::GetCursorPosX() + aw;
        ImGui::BeginDisabled(page == 0);
        if (ImGui::ArrowButton("##bld_prev", ImGuiDir_Left)) --page;
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered() && page > 0)
            ImGui::SetTooltip("Previous");

        char hdr[64];
        std::snprintf(hdr, sizeof hdr, "%s  (%d/%d)", bp.label.c_str(), page + 1, n);
        const float name_w = ui::fit_width(hdr);
        ImGui::SameLine(std::max(frame_h + style.ItemSpacing.x, (aw - name_w) * 0.5f));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", hdr);

        ImGui::SameLine(right - 2.0f * frame_h - style.ItemSpacing.x);
        ImGui::BeginDisabled(page == n - 1);
        if (ImGui::ArrowButton("##bld_next", ImGuiDir_Right)) ++page;
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered() && page < n - 1)
            ImGui::SetTooltip("Next");

        disclosure_controls(ui, detail_surface::building_metric, page, /*in_place=*/false);

        // Pager row above stays fixed; the page BODY scrolls on its own — Method's
        // recipe-comparison list in particular can run past the accordion's height
        // (a building with several era-allowed recipes), and a fixed-height page
        // silently clipped it with no way to reach the rest.
        ImGui::Spacing();
        ImGui::BeginChild("##building_page_body", {0.0f, 0.0f}, false,
                          ImGuiWindowFlags_NoSavedSettings);
        draw_building_page(w, reg, report, sel, bp.kind, ui);
        ImGui::EndChild();

        ImGui::EndChild();
    }
    ImGui::SameLine();

    // ── Right quarter: 2x3 action grid (mirrors the tile card's) ──
    {
        ImGui::BeginChild("##building_actions", {right_w, total_h}, false,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
        const float  bw  = (right_w - spacing) / 2.0f;
        const float  bh  = (total_h - 2.0f * spacing) / 3.0f;
        const ImVec2 bsz = {bw, bh};

        const bool owned = is_player_owned(w, sel);

        // Manage is GONE (2026-08-15 playtest rework, NR-245's queue-only
        // destination is moot now that the button no longer exists) — Ben's
        // call was he no longer wants this link. Its slot, plus one former
        // reserved slot, now hold Mothball and Dismantle: the former
        // Lifecycle PAGE's two controls, moved here so they read as
        // building-level actions rather than accordion content.

        // Mothball — same semantic as the old Close/Reopen toggle: reversible,
        // no output/no wages while closed. Toggle button (Toggle rule,
        // io-standing-rules.md): its own active state (decommissioned) is
        // what re-clicking undoes.
        {
            const bool mothballed = building.decommissioned;
            const char* tip = !owned                ? "Competitor building - intel only"
                             : mothballed             ? "Reopen - resumes output and wages"
                                                      : "Mothball - stops output and wages. Reversible.";
            // Distinct glyph per state (2026-08-15) — a closed box while
            // active, an open one once mothballed — so re-clicking doesn't
            // look like the same action twice.
            if (tile_icon_button("##bld_mothball", bsz, owned, tip,
                                 mothballed ? glyph_reopen : glyph_mothball) && owned)
            {
                building.decommissioned = !building.decommissioned;
                invalidate_logistics_caches(w); // a (un)decommissioned port/hub (un)anchors supply
            }
        }
        ImGui::SameLine();

        // Dismantle — permanent, confirm popup, deferred through
        // pending_demolish exactly as the old Lifecycle page's Dismantle did
        // (erasing from w.buildings mid-draw would dangle `building`).
        {
            const char* tip = owned ? "Dismantle - permanent. The tile keeps its deposit."
                                    : "Competitor building - intel only";
            if (tile_icon_button("##bld_dismantle", bsz, owned, tip, glyph_dismantle) && owned)
                ImGui::OpenPopup("confirm_dismantle");
        }
        if (ImGui::BeginPopup("confirm_dismantle"))
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                               "Dismantle this %s?", building_type_name(building.type));
            ImGui::TextDisabled("This cannot be undone.");
            ImGui::Separator();
            if (ImGui::Button("Dismantle"))
            {
                ui.construction.pending_demolish = sel;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Keep"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        // Auto (workforce autosolve) — relocated from the Workforce page onto
        // the grid, directly below Mothball (same column, next row): the
        // Workforce page's OLD Auto button churned its ImGui ID every tick
        // (see draw_building_workforce_page's comment) — the accent-ring
        // "primed" idiom (below, borrowed from the tile card's Construct
        // button) marks the active state instead of baking it into the label.
        {
            const bool   auto_on = building.workforce_auto;
            // Same one-way semantic the old page button had (never itself a
            // toggle-off — a manual slider edit is what pins the target and
            // clears Auto, in draw_building_workforce_page).
            const char*  tip     = !owned  ? "Competitor building - intel only"
                                  : auto_on ? "Auto - autosolving this building's workforce"
                                            : "Auto - autosolve this building's workforce target";
            if (auto_on)
            {
                const ImVec2 pmin = ImGui::GetCursorScreenPos();
                dl->AddRect({pmin.x - 2.0f, pmin.y - 2.0f}, {pmin.x + bsz.x + 2.0f, pmin.y + bsz.y + 2.0f},
                           palette::selection, 4.0f, 0, 2.0f);
            }
            if (tile_icon_button("##bld_auto", bsz, owned, tip, glyph_auto) && owned)
                building.workforce_auto = true;
        }
        ImGui::SameLine();
        tile_icon_button("##bld_reserved3", bsz, /*enabled=*/false, "Reserved", glyph_reserved);

        tile_icon_button("##bld_reserved4", bsz, /*enabled=*/false, "Reserved", glyph_reserved);
        ImGui::SameLine();
        tile_icon_button("##bld_reserved5", bsz, /*enabled=*/false, "Reserved", glyph_reserved);

        ImGui::EndChild();
    }
}

// The redesigned tile Selection element (BL-123, Ben's mockup): a vertical stack
// rather than the action|facts split — a placeholder image + [x, y] caption, the
// tile's deposits as world-max-relative bar charts, and a 2x2 action button grid.
// Tile-only for now; the other kinds keep the action|facts form (draw_selection_
// action / facts) until they get their own mockups. Construct Buildings stubs onto
// the existing Construction panel (the dedicated tile-construction panel is owed);
// History and Supply are drawn but not yet wired.
// Name of a tile's road tier, or nullptr when the tile carries no road (BL-184).
// The wording matches the construction ledger's tier affordances below and the
// canvas weight ladder (Track thin/dim -> Highway thick/bright); >3 reads as the
// top tier, exactly as body_surface_canvas.cpp's switch default does.
const char* road_tier_name(std::uint8_t road_level)
{
    switch (road_level)
    {
        case 0:  return nullptr;
        case 1:  return "Track";
        case 2:  return "Road";
        default: return "Highway";
    }
}

// The tile card (Ben's 2026-07-23 layout). Three regions: a header (icon + Tile
// [x,y] + close); a middle band split into a zoomed hex-neighbourhood render (left)
// and a thin vertical icon action strip (right); and a bottom third holding the
// per-resource production graphs (each click-drillable into its time series, BL-196).
// The zoomed render is the point — the card now SHOWS the tile it floats over, so it
// no longer merely obscures it.
void draw_tile_selection(world& w, ui_state& ui)
{
    const entity_id sel = ui.selected_entity;
    const auto tit = w.tiles.find(sel);
    if (tit == w.tiles.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    const tile_component& tile   = tit->second;
    const ImGuiStyle&     style  = ImGui::GetStyle();
    ImDrawList*           dl     = ImGui::GetWindowDrawList();
    const float           avail  = ImGui::GetContentRegionAvail().x;
    const float           frame_h = ImGui::GetFrameHeight();

    // ── Header: [icon] Tile [x, y] ............................ [x] ──
    {
        const ImVec2 hc = ImGui::GetCursorScreenPos();
        const float  ir = frame_h * 0.40f;
        draw_selection_icon(w, dl, selection_kind::tile, sel,
                            {hc.x + ir, hc.y + frame_h * 0.5f}, ir);
        ImGui::SetCursorScreenPos({hc.x + ir * 2.0f + style.ItemSpacing.x, hc.y});
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                           "Tile [%d, %d]", tile.grid_x, tile.grid_y);

        // Road tier (BL-184). The three tiers render on the canvas by line weight and
        // brightness alone, with no key anywhere: roads are always-on terrain rather
        // than a lens (Ben, BL-147), so the per-lens legend drawer cannot carry them.
        // Naming the tier here teaches that visual code contextually, at the moment of
        // interest, without adding permanent chrome. Roadless tiles show nothing.
        if (const char* tier = road_tier_name(tile.road_level))
        {
            ImGui::SameLine();
            ImGui::TextDisabled("\xc2\xb7 %s", tier);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Road tier: Track (thin) - Road - Highway (thick, bright).\n"
                                  "A better road moves goods across this tile faster.");
        }

        // No close button: the band is always open (BL-266).
    }
    ImGui::Separator();

    // Three columns spanning the full band width (Ben, 2026-07-28): 1/4 for the
    // zoomed hex neighbourhood, 1/2 for a paged metric ACCORDION (one titled
    // graph at a time — every deposited resource, plus the tile's own
    // habitability/hazard scalars, so there's more to see here than a single
    // resource), and 1/4 for a bigger 3x2 action-button grid. Supersedes the
    // earlier layout (hex+strip band over a full-width bottom accordion).
    const float total_h  = ImGui::GetContentRegionAvail().y;
    const float spacing  = style.ItemSpacing.x;
    const float left_w   = avail * 0.25f;
    const float right_w  = avail * 0.25f;
    const float center_w = std::max(80.0f, avail - left_w - right_w - 2.0f * spacing);

    // ── Left quarter: zoomed hex neighbourhood ──
    {
        const ImVec2 p  = ImGui::GetCursorScreenPos();
        const ImVec2 mx = {p.x + left_w, p.y + total_h};
        dl->AddRectFilled(p, mx, IM_COL32(16, 18, 24, 255), 3.0f);
        draw_tile_neighbourhood(dl, w, sel, p, {left_w, total_h}, /*radius=*/2);
        dl->AddRect(p, mx, IM_COL32(90, 90, 100, 255), 3.0f);
        ImGui::Dummy({left_w, total_h});
    }
    ImGui::SameLine();

    // ── Centre half: paged metric accordion ──
    // Pages: every deposited resource (tile production vs. the top-decile tile
    // for that resource, as before), THEN the tile's own habitability and hazard
    // (already tracked per-tile — tile_inspector.cpp's table carries the same
    // two columns — vs. this body's average, so a barren tile still has
    // something to page through). Atmospheric pollution and per-tile population
    // are NOT modelled today (population lives on population centres, not
    // arbitrary tiles), so they have no page here yet — noted, not faked.
    {
        const std::vector<tile_metric> pages = tile_metrics(w, sel);

        ImGui::BeginChild("##tile_accordion", {center_w, total_h}, true,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

        int&      page = ui.card_resource_page;
        const int n    = static_cast<int>(pages.size());
        page = std::clamp(page, 0, n - 1);
        const tile_metric& mp = pages[static_cast<std::size_t>(page)];

        // Pager row: [prev]  Metric (i/N)  [next]  [full canvas]
        //
        // The band rests EXPANDED-IN-PLACE (Ben, 2026-08-01): its rect is a fixed
        // 260 px that cannot shrink, so a folded one-liner here would spend ~220 px
        // on emptiness — the objection the superseded three-level design raised
        // against Glance-everywhere. It therefore takes the full-canvas control
        // alone (BL-265): there is no `⌄` state to reach, because this card is
        // already expanded. The control opens the same metric on the CANVAS, where
        // the chart has ten times the height.
        //
        // `right` is the row's true right edge, and the pager's next-page button and
        // the disclosure control are both hung off it — so the control lands in the
        // same column it occupies on every other surface.
        const float aw    = ImGui::GetContentRegionAvail().x;
        const float right = ImGui::GetCursorPosX() + aw;
        ImGui::BeginDisabled(page == 0);
        if (ImGui::ArrowButton("##res_prev", ImGuiDir_Left)) --page;
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered() && page > 0)
            ImGui::SetTooltip("Previous");

        char hdr[64];
        std::snprintf(hdr, sizeof hdr, "%s  (%d/%d)", mp.label.c_str(), page + 1, n);
        const float name_w = ui::fit_width(hdr); // BL-215: measured through the shared module
        ImGui::SameLine(std::max(frame_h + style.ItemSpacing.x, (aw - name_w) * 0.5f));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", hdr);

        ImGui::SameLine(right - 2.0f * frame_h - style.ItemSpacing.x);
        ImGui::BeginDisabled(page == n - 1);
        if (ImGui::ArrowButton("##res_next", ImGuiDir_Right)) ++page;
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered() && page < n - 1)
            ImGui::SetTooltip("Next");

        disclosure_controls(ui, detail_surface::selection_metric, page, /*in_place=*/false);

        // The current page's graph, filling the rest of the container. Deposited
        // resources are click-drillable into their time series (BL-196);
        // habitability/hazard are not — no per-tile history is tracked for them
        // yet, so the click target is skipped rather than faked.
        ImGui::Spacing();
        const ImVec2 p  = ImGui::GetCursorScreenPos();
        const float  cw = ImGui::GetContentRegionAvail().x;
        const float  gh = std::max(48.0f, ImGui::GetContentRegionAvail().y - 4.0f);
        const bool   drillable = mp.resource_index >= 0;
        if (drillable)
        {
            if (ImGui::InvisibleButton("##res_chart", {cw, gh}))
            {
                const bool dup = !ui.card_stack.empty() &&
                                 ui.card_stack.back().tile == sel &&
                                 ui.card_stack.back().resource == mp.resource_index;
                if (!dup && ui.card_stack.size() < 20)
                    ui.card_stack.push_back({sel, mp.resource_index});
                ui.card_track_tile = sel;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s: this tile yields %.1f vs %.1f for a top-10%% tile.\n"
                                  "Click for its history over time.",
                                  mp.label.c_str(), static_cast<double>(mp.tile_val),
                                  static_cast<double>(mp.ref_val));
        }
        else
        {
            ImGui::Dummy({cw, gh}); // reserve the same space; no drill-down yet
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s: %.2f vs %.2f %s.\nNo time-series history tracked for this yet.",
                                  mp.label.c_str(), static_cast<double>(mp.tile_val),
                                  static_cast<double>(mp.ref_val), mp.ref_label);
        }
        draw_tile_metric_chart(ImGui::GetWindowDrawList(), p, {p.x + cw, p.y + gh}, mp);

        ImGui::EndChild();
    }
    ImGui::SameLine();

    // ── Right quarter: bigger 2x3 action-button grid (Ben, 2026-07-28 — up from
    // a thin vertical icon strip; 2 columns x 3 rows rather than 3x2, since the
    // quarter-width column is too narrow for 3 across to read as "bigger" —
    // 2 wide gives chunkier buttons in the same slot count). Two slots are
    // reserved: only four actions exist today (Construct, Manage, History,
    // Supply), and the grid's shape should not have to change when a
    // fifth/sixth lands. ──
    {
        ImGui::BeginChild("##tile_actions", {right_w, total_h}, false,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
        const float  bw  = (right_w - spacing) / 2.0f;
        const float  bh  = (total_h - 2.0f * spacing) / 3.0f;
        const ImVec2 bsz = {bw, bh};

        // Construct — opens the tile construction ledger (BL-162).
        //
        // BL-174 strand 2: two world-derived layers, no tutorial state.
        //  - ENABLED only if something is actually placeable here. It was
        //    hard-coded true, so clicking an ocean tile opened a ledger of
        //    nothing but red refusals — the front door opening onto a wall.
        //  - PRIMED (accent ring) when the player has nothing under
        //    construction anywhere, which is the honest reading of "you have
        //    capital and nothing on the way". True at launch, extinguishes
        //    itself the moment a build starts, returns when it becomes true
        //    again. No timer, no flag, no dismissal to persist.
        bool any_placeable = false;
        for (const resource_type er : placement_rules::k_extractable)
            if (tile.resource_deposit[static_cast<std::size_t>(er)] > 0.0f &&
                placement_rules::can_place_in_world(w, sel, building_type::extraction_site, er,
                                                   ui.max_logistics_reach).ok())
            { any_placeable = true; break; }
        if (!any_placeable)
        {
            for (const building_type bt : {building_type::processing_facility,
                                           building_type::port,
                                           building_type::launchpad,
                                           building_type::inland_logistics_hub,
                                           building_type::military_base,
                                           building_type::research_institute})
                if (placement_rules::can_place_in_world(w, sel, bt, resource_type::iron_ore,
                                                       ui.max_logistics_reach).ok())
                { any_placeable = true; break; }
        }

        bool building_underway = false;
        if (const auto pit = w.corporations.find(w.player_entity); pit != w.corporations.end())
            for (entity_id bid : pit->second.assets)
                if (const auto bit = w.buildings.find(bid);
                    bit != w.buildings.end() && bit->second.ticks_remaining > 0)
                { building_underway = true; break; }

        const bool primed = any_placeable && !building_underway;
        if (primed)
        {
            const ImVec2 pmin = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRect(
                {pmin.x - 2.0f, pmin.y - 2.0f}, {pmin.x + bsz.x + 2.0f, pmin.y + bsz.y + 2.0f},
                palette::selection, 4.0f, 0, 2.0f);
        }
        if (tile_icon_button("##act_construct", bsz, any_placeable,
                             any_placeable
                                 ? (primed ? "Construct buildings - nothing under way"
                                           : "Construct buildings")
                                 : "Nothing can be built on this tile",
                             glyph_hammer))
            ui.show_build_ledger = true;
        ImGui::SameLine();

        // Manage — enabled only when a building occupies this tile; selecting it
        // switches the band to the building layout next frame. BL-367: a tile can
        // now hold several buildings (BL-366 lifted the capacity-1 rule for
        // non-extraction types), so this counts rather than grabbing the first
        // match — a single building still jumps straight to its detail (zero
        // extra clicks for the common case); more than one lands on the tile's
        // grouped-by-stack list (construction_panel.cpp draw_tile_stack_list).
        int       bld_count = 0;
        entity_id bld_here  = null_entity;
        for (const auto& [bid, bc] : w.buildings)
            if (bc.tile == sel) { ++bld_count; bld_here = bid; }
        if (tile_icon_button("##act_manage", bsz, bld_count > 0,
                             bld_count > 0 ? "Manage building" : "No building here",
                             glyph_gear) &&
            bld_count > 0)
        {
            ui.selected_entity = (bld_count == 1) ? bld_here : sel;
        }

        tile_icon_button("##act_history", bsz, /*enabled=*/false,
                         "History (not yet available)", glyph_clock);
        ImGui::SameLine();
        tile_icon_button("##act_supply", bsz, /*enabled=*/false,
                         "Supply (not yet available)", ui::icons::supply);

        tile_icon_button("##act_reserved1", bsz, /*enabled=*/false, "Reserved", glyph_reserved);
        ImGui::SameLine();
        tile_icon_button("##act_reserved2", bsz, /*enabled=*/false, "Reserved", glyph_reserved);

        ImGui::EndChild();
    }
}


} // namespace

void draw_selection_content(world& w, const recipe_registry& reg,
                            const economy_report& report, ui_state& ui)
{
    const selection_kind kind = selection_kind_of(w, ui.selected_entity);

    // Nothing valid selected — draw nothing. (The band frame never lets this
    // happen: with no valid selection it substitutes the player corporation
    // before calling here, BL-266. This guard covers other callers only.)
    if (kind == selection_kind::none)
        return;

    // A selected tile owns its WHOLE card layout, header included (Ben's 2026-07-23
    // three-region design: header / zoomed hex neighbourhood + action strip / graphs),
    // so it is dispatched before the generic header the other kinds share.
    if (kind == selection_kind::tile)
    {
        draw_tile_selection(w, ui);
        return;
    }

    // Frame-agnostic: this draws into whatever window the caller opened (the sticky
    // card, BL-195). Width comes from the live content region, not a fixed column —
    // so the same content renders in a card of any width. The header + per-kind
    // dispatch below is unchanged from the former fold-out Selection panel.
    const float       bar_w   = ImGui::GetContentRegionAvail().x;
    const float       frame_h = ImGui::GetFrameHeight();
    const ImGuiStyle& style   = ImGui::GetStyle();
    ImDrawList*       dl      = ImGui::GetWindowDrawList();

    // ── Header: [icon] Name · type ............................. [>] [x] ──
    {
        const ImVec2 hc = ImGui::GetCursorScreenPos();
        const float  ir = frame_h * 0.40f;
        draw_selection_icon(w, dl, kind, ui.selected_entity,
                            {hc.x + ir, hc.y + frame_h * 0.5f}, ir);
        ImGui::SetCursorScreenPos({hc.x + ir * 2.0f + style.ItemSpacing.x, hc.y});

        const char* title = selection_title(w, kind, ui.selected_entity);
        const char* kname = selection_kind_name(kind);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", title);
        // Suppress the redundant type label when the title is already the kind name
        // (e.g. a tile titles as "Tile"), avoiding "Tile · Tile".
        if (std::strcmp(title, kname) != 0)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", kname);
        }

        // Right-align the 'go to' button at the content region's right edge.
        // bar_w is the available content width (padding already excluded), so the
        // offset is measured from the content-left, no WindowPadding term needed.
        // No close button: the band is always open (BL-266).
        const float btn = frame_h;
        ImGui::SameLine(bar_w - btn);
        if (ImGui::Button(">", {btn, btn}))
            focus_on_entity(w, ui, ui.selected_entity);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Go to");
    }

    ImGui::Separator();

    // A selected player building used to BYPASS this layout and render the full
    // management view as its card (2026-07-22). Reversed 2026-08-08 (Ben: "selection
    // of a building skips the selection menu, going straight to manage. This is a
    // bug.") — a building took the same action|facts Selection view as every other
    // kind for a session, with an explicit Manage action routing to the management
    // ledger. This rework moves it OFF that split onto the tile card's 3-column band
    // shape instead (draw_building_selection_body) — it keeps this shared header,
    // unlike the tile card, which draws its own start to finish.
    if (kind == selection_kind::building)
    {
        draw_building_selection_body(w, reg, report, ui);
        return;
    }

    // A selected unit takes the SAME 3-column band shape (this rework) rather
    // than the generic action|facts split — see draw_unit_selection_body.
    if (kind == selection_kind::unit)
    {
        draw_unit_selection_body(w, reg, ui);
        return;
    }

    // ── Action (left, dominant) │ Facts (right, muted) ──
    const float content_w = bar_w;
    const float action_w  = (content_w - style.ItemSpacing.x) * 0.58f;

    // Two fill-height columns: action leads at a fixed split, facts to its right.
    // Container policy (LAYOUT.md): the Selection element wraps text rather than
    // clipping it, with vertical scroll as the overflow behaviour — so each column
    // wraps at its own width (PushTextWrapPos(0.0f) wraps at the current window's
    // right edge) and keeps its scrollbar instead of suppressing it.
    ImGui::BeginChild("##sel_action", {action_w, 0.0f}, false,
                      ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushTextWrapPos(0.0f);
    draw_selection_action(w, reg, ui, kind);
    ImGui::PopTextWrapPos();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##sel_facts", {0.0f, 0.0f}, false,
                      ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushTextWrapPos(0.0f);
    draw_selection_facts(w, reg, report, ui, kind);
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
}

void draw_building_page_expanded(world& w, const recipe_registry& reg,
                                 const economy_report& report, ui_state& ui)
{
    const std::vector<building_page> pages = building_pages(w, reg, report, ui.selected_entity);
    if (pages.empty())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    const int page = std::clamp(ui.selection_building_page, 0, static_cast<int>(pages.size()) - 1);
    draw_building_page(w, reg, report, ui.selected_entity, pages[static_cast<std::size_t>(page)].kind, ui);
}

namespace {

/// BL-429: the named building an extraction_site targeting @p r reads as in the
/// Build door — "Quarry" rather than "Extraction: Stone". Falls back to the
/// resource's own display name for anything not given a bespoke identity
/// (space/background raws outside this item's ancient-arc scope).
const char* extraction_building_name(resource_type r)
{
    switch (r)
    {
        case resource_type::iron_ore:       return "Iron Mine";
        case resource_type::coal:           return "Coal Mine";
        case resource_type::petroleum:      return "Oil Field";
        case resource_type::silica:         return "Silica Quarry";
        case resource_type::copper_ore:     return "Copper Mine";
        case resource_type::rare_earth_ore: return "Rare-Earth Mine";
        case resource_type::water:          return "Water Extractor";
        case resource_type::stone:          return "Quarry";
        case resource_type::timber:         return "Woodcutter's Camp";
        case resource_type::sand:           return "Sand Pit";
        case resource_type::clay:           return "Clay Pit";
        case resource_type::peat:           return "Peat Cutting";
        default:                            return resource_name(r);
    }
}

} // namespace

// The tile construction ledger (BL-162): the tile-contextual surface that actually
// lets the player build. Lists every building type placeable on the selected tile —
// each in a bordered container (name + full credit cost + payback + an expected-profit
// magnitude bar + a reason-coded validity read + a Build action) — and enqueues the
// chosen build on the tile via the construction.pending_tile seam app executes.
// Candidates sort by expected net descending on a ceiling shared across the list, so
// the best options are the ones on screen without scrolling.
void draw_construction_ledger(const world& w, const recipe_registry& reg, ui_state& ui)
{
    const entity_id tile_id = ui.selected_entity;
    const auto tit = w.tiles.find(tile_id);
    if (tit == w.tiles.end())
    {
        ui.show_build_ledger = false; // selection is not a tile — nothing to build on
        return;
    }
    const tile_component& tile = tit->second;

    const foldout_rect r       = foldout_column_rect();
    const float        bar_w   = r.w;
    const float        frame_h = ImGui::GetFrameHeight();
    const ImGuiStyle&  style   = ImGui::GetStyle();

    ImGui::SetNextWindowPos({r.x, r.y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({r.w, r.h}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.90f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar            |
        ImGuiWindowFlags_NoResize              |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoCollapse            |
        ImGuiWindowFlags_NoNav                 |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar           |
        ImGuiWindowFlags_NoScrollWithMouse     |
        ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##build_ledger", nullptr, flags);

    // ── Header: Construct · [x, y] ............................... [x] ──
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Construct");
    ImGui::SameLine();
    ImGui::TextDisabled("[%d, %d]", tile.grid_x, tile.grid_y);
    const float btn = frame_h;
    ImGui::SameLine(bar_w - style.WindowPadding.x - btn);
    if (ImGui::Button("x", {btn, btn}))
        ui.show_build_ledger = false; // back to the tile Selection element
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Close");

    ImGui::Separator();

    // Player balance — the affordability context for every Build button below.
    float balance = 0.0f;
    if (const auto pit = w.corporations.find(w.player_entity); pit != w.corporations.end())
        balance = pit->second.balance;
    ImGui::TextDisabled("Balance: %.0f cr", static_cast<double>(balance));

    // Last construction outcome (set by app after executing a request).
    if (!ui.construction.last_message.empty())
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::neutral), "%s",
                           ui.construction.last_message.c_str());

    // The tile's market prices every material cost and every revenue figure below.
    const market_component* mkt = nullptr;
    {
        const entity_id mid = market_for_tile(w, tile_id);
        if (mid != null_entity)
        {
            const auto mit = w.markets.find(mid);
            if (mit != w.markets.end())
                mkt = &mit->second;
        }
    }
    const auto price = [&](std::size_t ri) -> float {
        if (mkt == nullptr)
            return 0.0f;
        return mkt->price[ri] > 0.0f ? mkt->price[ri] : mkt->base_price[ri];
    };

    // Candidate placements for this tile: one extraction option per extractable
    // resource actually deposited here, then the fixed processing / port / launchpad
    // types. Validity + reason come from the shared placement_rules seam; the expected
    // per-tick net comes from estimate_prospective_profit (world/building_profit.hpp).
    //
    // Processing expands into ONE ROW PER RECIPE: their margins genuinely diverge, so a
    // single type-level row priced at "steel" would report "processing loses money here"
    // on a tile where food rations clear well. The chosen recipe survives to construction
    // via construction.pending_recipe → construct_building's trailing recipe parameter,
    // so the bar always prices what the Build button would actually build.
    // BL-428: absolute-id group lookup, replacing a browse-position recipe_at.
    // A null id falls back to the same literal "General" recipe_registry.hpp
    // documents as the absent-group default, so a caller never sees an empty key.
    static const auto recipe_group_of = [](const recipe_registry& r,
                                           std::uint16_t id) -> const std::string&
    {
        static const std::string general = "General";
        const recipe* rc = r.get_recipe(id);
        return rc ? rc->group : general;
    };

    struct candidate
    {
        building_type type;
        resource_type target;
        std::string   name;
        /// ABSOLUTE recipe id (the get_recipe / recipe_id space), NOT a browse
        /// position. Load-bearing: this value is handed to construct_building via
        /// ui.construction.pending_recipe, and construct_building indexes it
        /// absolutely. It used to hold the browse index, which is the same number
        /// only while the era mask is the identity — under an ancient campaign it
        /// silently named a different recipe (found wiring BL-428's depth gate).
        std::uint16_t recipe = no_recipe;

        placement_rules::placement_result pr;
        building_profit profit;
        float           capex    = 0.0f; ///< build_cost + materials at the local market.
        bool            produces = false; ///< Extraction/processing earn directly; infrastructure does not.
        int             group    = 0;    ///< Render band: 0 = ranked/priced, 1 = valid unranked, 2 = invalid.
        std::string     category;        ///< Fold group (BL-326): Extraction / Processing / Infrastructure / Military.
        float           material_rate = 1.0f; ///< construction_rate at this tile (BL-328 pre-commit warning).
    };
    std::vector<candidate> cands;
    for (const resource_type er : placement_rules::k_extractable)
        if (tile.resource_deposit[static_cast<std::size_t>(er)] > 0.0f)
            cands.push_back({building_type::extraction_site, er,
                             (er == resource_type::agricultural_produce) ? "Farm"
                                                                          : extraction_building_name(er)});
    // Fishing Wharf (BL-168): agricultural_produce has no terrestrial deposit here,
    // so the loop above skips it — but a coastal tile can still work it. Without this,
    // the ledger never offers the one candidate whose whole point is a zero-deposit tile.
    if (tile.resource_deposit[static_cast<std::size_t>(resource_type::agricultural_produce)] <= 0.0f
        && placement_rules::is_coastal(w, tile_id))
        cands.push_back({building_type::extraction_site, resource_type::agricultural_produce,
                         "Fishing Wharf"});
    // One processing row per recipe (BL-429: its authored display_name, "Bloomery"
    // rather than "Processing: Iron Blooms"), each priced on its own economics.
    for (int ri = 0; ri < reg.recipe_count(building_type::processing_facility); ++ri)
    {
        const recipe& browsed = reg.recipe_at(building_type::processing_facility, ri);
        cands.push_back({building_type::processing_facility, resource_type::iron_ore,
                         browsed.display_name,
                         reg.recipe_id(browsed.name)}); // browse -> ABSOLUTE, see candidate::recipe
    }
    cands.push_back({building_type::port,                 resource_type::iron_ore, "Port"});
    cands.push_back({building_type::launchpad,            resource_type::iron_ore, "Launchpad"});
    cands.push_back({building_type::inland_logistics_hub, resource_type::iron_ore, "Inland Logistics Hub"}); // BL-149
    cands.push_back({building_type::military_base,        resource_type::iron_ore, "Military Base"});        // BL-325 S1
    cands.push_back({building_type::research_institute,   resource_type::iron_ore, "Research Institute"});   // BL-332

    // BL-433: drop types outside the campaign's era band, so a 0 CE player is never
    // offered a Launchpad. The processing rows above need no filtering — they are
    // built from recipe_count/recipe_at, which are the era-masked browse path.
    // construct_building refuses these anyway (construction_result::era_locked);
    // this is the door not showing what the gate would refuse.
    //
    // BL-428 adds the second half of the same argument: an ancient recipe whose
    // chain the corp has not yet reached is refused by construct_building
    // (depth_locked), so the door should not offer it either. Ancient-band
    // recipes only, per the 2026-08-16 first-cut ruling — an `any`-band recipe is
    // never depth-filtered.
    const int reached = [&]
    {
        const auto it = w.corporations.find(w.player_entity);
        return (it != w.corporations.end()) ? corp_reached_depth(it->second, reg) : 0;
    }();
    cands.erase(std::remove_if(cands.begin(), cands.end(),
                               [&reg, reached](const candidate& c)
                               {
                                   if (!reg.building_available(c.type))
                                       return true;
                                   const recipe* rc = reg.get_recipe(c.recipe);
                                   if (!rc || rc->era != era_band::ancient)
                                       return false;
                                   const int need = reg.recipe_required_depth(c.recipe);
                                   return need < 0 || need > reached;
                               }),
                cands.end());

    // BL-326: fold group by building family — Ben rejected the profit-ranked flat list
    // for named, expandable groups. Assigned by type rather than parsed from the display
    // name, so a future rename of "Extraction: X" can't silently drop a row into the
    // wrong group.
    for (candidate& c : cands)
        c.category = (c.type == building_type::extraction_site)     ? "Extraction"
                    : (c.type == building_type::processing_facility) ? "Processing"
                    : (c.type == building_type::military_base)       ? "Military"
                    : (c.type == building_type::research_institute)  ? "Military"
                                                                      : "Infrastructure";

    for (candidate& c : cands)
    {
        const building_economics& econ = reg.economics(c.type);
        // BL-344: the player's own corp is named, so a tech-gated type reads as
        // "Locked - not researched" in the ledger rather than offering a Build
        // button that construct_building would then refuse.
        c.pr = placement_rules::can_place_in_world(w, tile_id, c.type, c.target,
                                                  ui.max_logistics_reach,
                                                  w.player_entity);
        c.material_rate = construction_rate(w, reg, c.type, tile_id); // BL-328 pre-commit warning

        // Capex is the figure construction.cpp actually gates on: build cost PLUS the
        // materials priced at the local market. This ledger used to gate on build_cost
        // alone, so between the two figures it offered an enabled Build button that
        // then failed with nothing but a post-hoc toast.
        c.capex = econ.build_cost;
        for (std::size_t ri = 0; ri < resource_count; ++ri)
            if (econ.resource_build_cost[ri] > 0.0f)
                c.capex += econ.resource_build_cost[ri] * price(ri);

        c.produces = (c.type == building_type::extraction_site ||
                      c.type == building_type::processing_facility);
        c.profit   = estimate_prospective_profit(w, reg, tile_id, c.type, c.target, c.recipe);
        c.group    = !c.pr.ok()                          ? 2
                     : (c.produces && c.profit.has_data) ? 0
                                                         : 1;
    }

    // BL-434: collapse the one-row-per-recipe processing rows above into one row
    // PER GROUP — "Metal Foundry" once, not five separate metal recipes stacked
    // in the Processing fold. The representative recipe is the best-expected-profit
    // one among that group's era-allowed recipes (an invalid/no-data row loses to
    // any priced one; among priced rows, highest net wins; ties keep the first
    // seen, i.e. recipe-authored order, for determinism). Placing the group's
    // candidate seeds the building with THIS recipe (construct_building's trailing
    // recipe param, via pending_recipe below) — the group's own default/best
    // method, not an arbitrary one.
    {
        std::map<std::string, std::size_t> best_idx; // recipe group -> winning index into cands
        for (std::size_t i = 0; i < cands.size(); ++i)
        {
            if (cands[i].type != building_type::processing_facility)
                continue;
            const std::string& grp =
                recipe_group_of(reg, cands[i].recipe);
            const auto it = best_idx.find(grp);
            if (it == best_idx.end())
            {
                best_idx.emplace(grp, i);
                continue;
            }
            const candidate& cur   = cands[i];
            const candidate& incum = cands[it->second];
            const bool cur_better =
                (cur.group == 0 && incum.group != 0) ||
                (cur.group == 0 && incum.group == 0 && cur.profit.net() > incum.profit.net());
            if (cur_better)
                it->second = i;
        }
        std::vector<candidate> collapsed;
        collapsed.reserve(cands.size());
        for (candidate& c : cands)
            if (c.type != building_type::processing_facility)
                collapsed.push_back(std::move(c));
        for (auto& [grp, idx] : best_idx)
        {
            candidate winner = cands[idx];
            winner.name      = grp; // the group name IS the row's identity now
            collapsed.push_back(std::move(winner));
        }
        cands = std::move(collapsed);
    }

    // Two-tier alphabetical (BL-326): group name, then row name — Ben's explicit call
    // against the old profit-ranked flat list ("not most profit first"). A refused
    // candidate stays in its group rather than dropping out, so the reason text (the
    // ledger's teaching surface) is still visible under its fold. stable_sort with a
    // declaration-order tie-break keeps ties frame-stable and deterministic.
    std::stable_sort(cands.begin(), cands.end(),
                     [](const candidate& a, const candidate& b) {
                         if (a.category != b.category)
                             return a.category < b.category;
                         return a.name < b.name;
                     });

    // One ceiling shared across the whole list, so the bars compare directly. Only the
    // ranked candidates set it — an invalid or infrastructure row must not inflate the
    // axis and squash the bars that matter. tight_ceil (not nice_ceil) keeps the tallest
    // bar high in its track.
    float peak = 0.0f;
    for (const candidate& c : cands)
        if (c.group == 0)
            peak = std::max(peak, std::fabs(c.profit.net()));
    const float ceiling = charts::tight_ceil(peak);

    // What the bars mean, stated once. The assumptions are named rather than modelled
    // (see estimate_prospective_profit) — an honest static read beats a half-modelled one.
    //
    // "before auto-staffing" is doing real work (NR-001): the estimate mirrors
    // construction.cpp's 50% workforce exactly, so it describes the building at the
    // instant it is created — but BL-181 auto-solves a player building's workforce for
    // maximum profit on its FIRST tick, and prospective_profit_harness measured realised
    // extraction at ~2x this figure because of it. Every candidate is understated by the
    // same factor, so the RANKING the chart exists for is unaffected; the magnitude is a
    // floor, not a ceiling, and a player reads a profit bar as a prediction. Three words
    // stop it reading as the most the building will ever earn (Ben's call, 2026-08-01).
    ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
    ImGui::TextWrapped("Est. net / qtr at today's local prices - at least this, before "
                       "auto-staffing; 50%% staffing, no labour shortage. Capex is not in "
                       "the bar; see payback.");
    ImGui::PopStyleColor();

    // Five lines per candidate: name / cost + payback / supply warning (BL-328,
    // reserved unconditionally so a warning row never clips against a fixed-height
    // sibling that lacks one) / profit bar / action.
    constexpr float bar_h = 14.0f;
    const float     row_h = style.WindowPadding.y * 2.0f
                          + ImGui::GetTextLineHeightWithSpacing()       // name
                          + ImGui::GetTextLineHeightWithSpacing()       // cost + payback
                          + ImGui::GetTextLineHeightWithSpacing()       // supply warning (blank if none)
                          + std::max(bar_h, ImGui::GetTextLineHeight()) // profit bar or its text
                          + style.ItemSpacing.y
                          + ImGui::GetFrameHeight();                    // Build / reason

    // Roads keep their own taller row: their tier glyph box is not replaced by a bar.
    constexpr float img        = 56.0f;
    const float     road_row_h = img + style.WindowPadding.y * 2.0f + 8.0f;

    ImGui::BeginChild("##build_list", {0.0f, 0.0f}, false,
                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    // BL-326: candidates are pre-sorted category-then-name, so each category's rows are
    // contiguous — walk the list opening a TreeNodeEx (this file's existing fold idiom,
    // e.g. economy_panel.cpp) whenever the category changes, and close it once its rows
    // are drawn. Defaulted open, matching the fold usage elsewhere in the UI.
    std::string open_category;
    bool        category_open = false;
    for (const candidate& c : cands)
    {
        if (c.category != open_category)
        {
            if (category_open)
                ImGui::TreePop();
            open_category = c.category;
            category_open = ImGui::TreeNodeEx(open_category.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        }
        if (!category_open)
            continue;

        const bool affordable = balance >= c.capex;

        ImGui::BeginChild(c.name.c_str(), {0.0f, row_h}, true,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
        ImDrawList* cdl = ImGui::GetWindowDrawList();

        // Name: an 18 px building glyph, then the candidate's name. (The 56x56
        // placeholder box is gone — its width is exactly what the profit bar needed.)
        {
            constexpr float gr = 9.0f;
            const ImVec2    gp = ImGui::GetCursorScreenPos();
            // BL-429: extraction rows already carry their target in c.target; a
            // processing row's c.target is always the placeholder iron_ore set
            // when the candidate was built above, so the glyph is computed here
            // from the row's own recipe instead, matching its name (Bloomery,
            // Smithy, ...) rather than the placeholder.
            const resource_type icon_identity =
                (c.type == building_type::processing_facility && c.recipe != no_recipe)
                    ? primary_output_resource(*reg.get_recipe(c.recipe))
                    : c.target;
            icons::building(cdl, {gp.x + gr, gp.y + ImGui::GetTextLineHeight() * 0.5f}, gr,
                            c.type, icon_identity, IM_COL32(150, 235, 160, 255));
            ImGui::Dummy({gr * 2.0f, ImGui::GetTextLineHeight()});
            ImGui::SameLine();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s",
                               c.name.c_str());
        }

        // Cost: one credit total with the materials priced in, then payback (capex over
        // net) when the candidate earns — net alone ranks earnings and ignores capex.
        std::string cost = std::to_string(static_cast<int>(c.capex)) + " cr";
        if (c.group == 0 && c.profit.net() > 0.0f)
        {
            const float pb = c.capex / c.profit.net();
            cost += (pb > 999.0f)
                    ? std::string(" - payback > 999 qtrs")
                    : " - payback ~" + std::to_string(static_cast<int>(pb)) + " qtrs";
        }
        ImGui::TextDisabled("%s", cost.c_str());

        // Pre-commit material-supply warning (BL-328): the same construction_rate the
        // in-progress building card reads, checked before the player commits capex —
        // "will this stall" answered up front instead of via a post-hoc paused status.
        // Gated on c.pr.ok(): an already-invalid tile (ocean, wrong deposit, ...) shows
        // ITS reason below and needs no second, redundant warning stacked on top. The
        // line always occupies its row_h slot (blank when there is nothing to say) so
        // rows stay a uniform height rather than the sibling below clipping into it.
        if (c.pr.ok() && c.material_rate <= 0.0f)
            ImGui::TextColored(construction_status_colour(0.0f), "%s",
                               "No local supply - build will stall");
        else if (c.pr.ok() && c.material_rate < 1.0f)
            ImGui::TextColored(construction_status_colour(c.material_rate), "%s",
                               "Supply-limited - will stretch");
        else
            ImGui::Dummy({0.0f, ImGui::GetTextLineHeight()});

        // Profit row — exactly one of three states.
        if (c.group == 0)
        {
            // Length is |net| / the shared ceiling, colour is the sign, and the signed
            // number prints in full beside it (draw_bars has no negative axis).
            const ImVec2 bp = ImGui::GetCursorScreenPos();
            const float  bw = ImGui::GetContentRegionAvail().x;
            charts::draw_value_bar(cdl, bp, {bp.x + bw, bp.y + bar_h}, c.profit.net(), ceiling,
                                   value_colour(static_cast<double>(c.profit.net())));
            ImGui::Dummy({bw, bar_h});
        }
        else if (c.pr.ok() && !c.produces && c.profit.has_data)
        {
            // Infrastructure earns through what it enables, not what it makes; its
            // honest net is minus its upkeep, and a red bar for that would read as a
            // bad investment rather than as an enabler.
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::neutral),
                               "No direct revenue - upkeep %.0f / qtr",
                               static_cast<double>(c.profit.maintenance + c.profit.wages));
        }
        // else: unbuildable, or no market to price it — the reason text takes this slot.

        // Action: Build when valid (disabled + noted when unaffordable); else the reason.
        if (c.pr.ok())
        {
            ImGui::BeginDisabled(!affordable);
            if (ImGui::Button("Build"))
            {
                ui.construction.pending_tile   = tile_id;
                ui.construction.pending_type   = c.type;
                ui.construction.pending_target = c.target;
                ui.construction.pending_recipe = c.recipe; // the row's recipe, not the default
            }
            ImGui::EndDisabled();
            if (!affordable)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("Can't afford");
            }
        }
        else
        {
            ImGui::TextColored(ImVec4{0.90f, 0.55f, 0.55f, 1.0f}, "%s", c.pr.message());
        }

        ImGui::EndChild();
        ImGui::Spacing();
    }
    if (category_open)
        ImGui::TreePop();

    // Hire (BL-324, moved onto the base by BL-325 S2) — the campaign roster
    // gated on the player corp's OWN stockpile/market access (unit_roster.hpp),
    // not this tile's deposit or placement rules; the tile now ALSO has to
    // carry the corp's own completed military_base (S2: hire is no longer
    // hire-anywhere). Beside Build rather than folded into it: hiring never
    // touches building slots or terrain validity, so it does not belong in the
    // candidate loop above.
    {
        bool has_muster_base = false;
        for (const auto& [bid, bc] : w.buildings)
        {
            if (bc.tile == tile_id && bc.type == building_type::military_base
                && bc.ticks_remaining <= 0 && !bc.decommissioned)
            {
                const auto cit = w.corporations.find(w.player_entity);
                if (cit != w.corporations.end()
                    && std::find(cit->second.assets.begin(), cit->second.assets.end(), bid)
                           != cit->second.assets.end())
                {
                    has_muster_base = true;
                }
                break;
            }
        }
        const auto& table = unit_roster_table();
        const auto  avail = has_muster_base
            ? available_rows(w, w.player_entity, campaign_roster_band_for(reg.era()))
            : std::vector<const roster_row*>{};
        if (!avail.empty())
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Hire");
            ImGui::BeginChild("##hire_list", {0.0f, row_h * std::min<std::size_t>(avail.size(), 3)},
                              false, ImGuiWindowFlags_NoSavedSettings);
            for (const roster_row* row : avail)
            {
                const auto idx = static_cast<std::uint16_t>(row - table.data());
                ImGui::PushID(static_cast<int>(idx));
                ImGui::TextUnformatted(row->name);
                ImGui::SameLine(bar_w - style.WindowPadding.x - 70.0f);
                if (ImGui::Button("Hire", {60.0f, 0.0f}))
                {
                    ui.construction.pending_hire_tile      = tile_id;
                    ui.construction.pending_hire_unit_type = idx;
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::Spacing();
        }
    }

    // Road placement (BL-147 core, BL-172 tier ladder) — a per-tile mutation, not a building, so
    // it takes its own affordances + the pending_road_tile/tier path (place_road). Three tiers:
    // Track (1) / Road (2) / Highway (3); the glyph weight + brightness mirror the on-canvas
    // render, and each shows its own cost + validity (upgrade-in-place is allowed, so a tile that
    // already carries an equal-or-better road greys out only the tiers it meets or exceeds).
    {
        struct road_tier_affordance
        {
            const char*  name;
            std::uint8_t level;
            float        glyph_thick; // matches the canvas weight ladder
            ImU32        glyph_col;
        };
        static const road_tier_affordance road_tiers[3] = {
            { "Track",   1, 2.0f, IM_COL32(175, 158, 120, 255) },
            { "Road",    2, 3.0f, IM_COL32(205, 188, 140, 255) },
            { "Highway", 3, 4.0f, IM_COL32(225, 205, 150, 255) },
        };

        for (const road_tier_affordance& rt : road_tiers)
        {
            ImGui::PushID(static_cast<int>(rt.level)); // unique ImGui ids per tier

            const road_economics& re = reg.road_econ(rt.level);
            const placement_rules::placement_result pr =
                placement_rules::can_place_road(tile, rt.level);

            // Same affordability defect as the building path: place_road gates on
            // build_cost PLUS materials priced at the local market, so gating on
            // build_cost alone enabled a Build that then failed.
            float road_capex = re.build_cost;
            for (std::size_t i = 0; i < resource_count; ++i)
                if (re.resource_build_cost[i] > 0.0f)
                    road_capex += re.resource_build_cost[i] * price(i);
            const bool affordable = balance >= road_capex;

            ImGui::BeginChild("road##build", {0.0f, road_row_h}, true,
                              ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
            ImDrawList* cdl = ImGui::GetWindowDrawList();

            const ImVec2 ip = ImGui::GetCursorScreenPos();
            const ImVec2 imx = {ip.x + img, ip.y + img};
            cdl->AddRectFilled(ip, imx, IM_COL32(72, 72, 72, 255), 3.0f);
            cdl->AddRect(ip, imx, IM_COL32(110, 110, 110, 255), 3.0f);
            // A short road glyph: a segment across the box, weighted by tier (matches the canvas).
            cdl->AddLine({ip.x + img * 0.2f, ip.y + img * 0.7f},
                         {ip.x + img * 0.8f, ip.y + img * 0.3f}, rt.glyph_col, rt.glyph_thick);
            ImGui::Dummy({img, img});
            ImGui::SameLine();

            ImGui::BeginGroup();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", rt.name);

            // One credit total, materials priced in — the same figure the gate uses.
            ImGui::TextDisabled("%d cr", static_cast<int>(road_capex));

            if (pr.ok())
            {
                ImGui::BeginDisabled(!affordable);
                if (ImGui::Button("Build"))
                {
                    ui.construction.pending_road_tile = tile_id;
                    ui.construction.pending_road_tier = rt.level;
                }
                ImGui::EndDisabled();
                if (!affordable)
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("Can't afford");
                }
            }
            else
            {
                ImGui::TextColored(ImVec4{0.90f, 0.55f, 0.55f, 1.0f}, "%s", pr.message());
            }
            ImGui::EndGroup();

            ImGui::EndChild();
            ImGui::Spacing();
            ImGui::PopID();
        }
    }

    ImGui::EndChild();

    ImGui::End();
}

} // namespace ui
