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
#include "world/unit_roster.hpp" // campaign hire gate + roster table (BL-324)

#include <imgui.h>

#include <algorithm> // std::min/clamp (build rate), std::nth_element (top-decile production)
#include <cmath>     // std::ceil (construction ETA), std::pow/log10/floor (nice_ceil axis)
#include <cstddef>   // std::ptrdiff_t (nth_element iterator offset)
#include <cstdio>    // std::snprintf (tile coord caption + chart labels)
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
        const float p90 = top_decile_production(w, r);
        const resource_presentation& rp = presentation_of(static_cast<resource_type>(r));
        pages.push_back({ rp.name, a, p90, "Top 10%",
                          nice_ceil(std::max(a, p90) > 0.0f ? std::max(a, p90) : 1.0f),
                          static_cast<int>(r) });
    }

    // Then the tile's own scalars against the body average, so a barren tile still
    // has something to page through.
    float sum_hab = 0.0f, sum_haz = 0.0f;
    int   n = 0;
    for (const auto& [id, t] : w.tiles)
        if (t.body == tile.body) { sum_hab += t.habitability; sum_haz += t.hazard_level; ++n; }
    const float avg_hab = n > 0 ? sum_hab / static_cast<float>(n) : 0.0f;
    const float avg_haz = n > 0 ? sum_haz / static_cast<float>(n) : 0.0f;
    pages.push_back({ "Habitability", tile.habitability, avg_hab, "Body avg", 1.0f, -1 });
    pages.push_back({ "Hazard",       tile.hazard_level, avg_haz, "Body avg", 1.0f, -1 });

    return pages;
}

void draw_tile_metric_chart(ImDrawList* dl, ImVec2 mn, ImVec2 mx, const tile_metric& m)
{
    constexpr ImU32 tile_col = IM_COL32(150, 235, 160, 255); // this tile's value (green)
    constexpr ImU32 ref_col  = IM_COL32(150, 160, 190, 255); // reference value (muted)
    draw_production_chart(dl, mn, mx, m.tile_val, m.ref_val, m.ceiling,
                          tile_col, ref_col, m.ref_label);
}

// Per-building profitability readout (BL-074): the selected player building's
// estimated net per-tick contribution and its component lines. Realised last-tick
// figures; revenue/inputs are estimates (the pooled market resists exact per-building
// attribution — see building_profit.hpp).
void draw_building_profit(const world& w, const recipe_registry& reg,
                          const economy_report& report, entity_id id)
{
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                       "Profitability (est. / qtr)");

    const building_profit p = estimate_building_profit(w, reg, report, id);
    if (!p.has_data)
    {
        ImGui::TextDisabled("Run an economy quarter to estimate.");
        return;
    }

    // Paired two-column layout — four component cells over two rows, then Net —
    // so revenue/inputs/wages/maintenance + net all fit the fixed bar height.
    const float v1 = 68.0f, l2 = 150.0f, v2 = 210.0f;
    const auto val = [](float value, ImU32 col)
    { ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%+.2f", value); };
    const auto pair = [&](const char* la, float va, ImU32 ca,
                          const char* lb, float vb, ImU32 cb)
    {
        ImGui::TextUnformatted(la);      ImGui::SameLine(v1); val(va, ca);
        ImGui::SameLine(l2);
        ImGui::TextUnformatted(lb);      ImGui::SameLine(v2); val(vb, cb);
    };

    pair("Revenue", +p.revenue,    palette::positive,
         "Inputs",  -p.input_cost, palette::negative);
    pair("Wages",   -p.wages,      palette::negative,
         "Maint",   -p.maintenance,palette::negative);

    const float net = p.net();
    const ImU32 nc = (net < 0.0f) ? palette::negative
                   : (net > 0.0f) ? palette::positive
                                  : palette::neutral;
    ImGui::TextUnformatted("Net");   ImGui::SameLine(v1); val(net, nc);
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

        case selection_kind::building:
            if (is_player_owned(w, sel))
            {
                // Under construction (BL-095 task E): lead with the live analog build
                // status — rate / ETA / paused — so a freshly-placed building reads as
                // "still building" rather than an empty card.
                if (const auto bit = w.buildings.find(sel);
                    bit != w.buildings.end() && bit->second.ticks_remaining > 0)
                {
                    const building_component& b = bit->second;
                    const float rate = construction_rate(w, reg, b.type, b.tile);
                    ImGui::TextColored(construction_status_colour(rate), "%s",
                        construction_status(rate, b.ticks_remaining).c_str());
                }
                // Manage is a deliberate step now, not the selection itself
                // (2026-08-08: selecting a building must land on the Selection
                // view first). The button opens the management ledger's
                // Buildings tab, which already keys off selected_entity.
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Operate");
                if (ImGui::Button("Manage"))
                {
                    ui.show_construction_panel  = true;
                    ui.construction.panel_view  = 1; // the Buildings tab
                }
            }
            else
            {
                ImGui::TextDisabled("Competitor building - intel only.");
            }
            break;

        case selection_kind::market:
        case selection_kind::unit:
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

// The facts: only what informs the action (BL-093) — slim and muted. Everything
// encyclopedic (orbit, composition, deposits, prices) lives in the ledgers, one
// 'go to' away.
void draw_selection_facts(const world& w, const recipe_registry& reg,
                          const economy_report& report, ui_state& ui, selection_kind kind)
{
    const entity_id sel = ui.selected_entity;
    switch (kind)
    {
        // selection_kind::tile is handled by draw_tile_selection (BL-123), not here.
        case selection_kind::body:
            draw_activity_section(w, sel);          // BL-089: commercial pulse
            break;
        case selection_kind::building:
            if (is_player_owned(w, sel))
                draw_building_profit(w, reg, report, sel);   // BL-074: is it profitable?
            else
                draw_rival_building_summary(w, sel);         // owner + private rows
            break;
        default:
            break;
    }
}

// The redesigned tile Selection element (BL-123, Ben's mockup): a vertical stack
// rather than the action|facts split — a placeholder image + [x, y] caption, the
// tile's deposits as world-max-relative bar charts, and a 2x2 action button grid.
// Tile-only for now; the other kinds keep the action|facts form (draw_selection_
// action / facts) until they get their own mockups. Construct Buildings stubs onto
// the existing Construction panel (the dedicated tile-construction panel is owed);
// History and Supply are drawn but not yet wired.
// --- Tile-card action-strip glyphs -------------------------------------------
// Compact glyphs for the right-hand icon strip (Ben's 2026-07-23 layout). Drawn
// centred at @p c within radius @p r, matching the (dl, centre, r, colour) contract
// the ui::icons vocabulary uses. Construct/Manage/History are drawn here; Supply
// reuses ui::icons::supply. Kept local for now; promote to ui::icons if they stick.
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

// A faint dot — the neutral "nothing assigned yet" glyph for a reserved slot
// in the 3x2 action grid (BL-213 follow-up, 2026-07-28: the grid grew from 4
// actions to 6 slots; two have no action yet).
void glyph_reserved(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddCircle(c, r * 0.30f, col, 12, 1.5f);
}

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

// A square icon button: an ImGui::Button frame with a glyph drawn over it and a
// hover tooltip (Ben's call: icons, text only on hover). Disabled buttons dim the
// glyph and show the reason. Returns true only on an enabled click.
bool tile_icon_button(const char* id, ImVec2 sz, bool enabled, const char* tip,
                      void (*glyph)(ImDrawList*, ImVec2, float, ImU32))
{
    ImGui::BeginDisabled(!enabled);
    const ImVec2 p       = ImGui::GetCursorScreenPos();
    const bool   clicked = ImGui::Button(id, sz);
    ImGui::EndDisabled();
    const ImU32 gcol = enabled ? IM_COL32(215, 215, 220, 255) : IM_COL32(110, 110, 116, 255);
    glyph(ImGui::GetWindowDrawList(), {p.x + sz.x * 0.5f, p.y + sz.y * 0.5f},
          std::min(sz.x, sz.y) * 0.5f, gcol);
    if (ImGui::IsItemHovered() && tip)
        ImGui::SetTooltip("%s", tip);
    return clicked && enabled;
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
                                           building_type::military_base})
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
        // switches the band to the building layout next frame.
        entity_id bld_here = null_entity;
        for (const auto& [bid, bc] : w.buildings)
            if (bc.tile == sel) { bld_here = bid; break; }
        if (tile_icon_button("##act_manage", bsz, bld_here != null_entity,
                             bld_here != null_entity ? "Manage building" : "No building here",
                             glyph_gear) &&
            bld_here != null_entity)
        {
            ui.selected_entity = bld_here;
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
    // bug.") — a building now takes the same action|facts Selection view as every
    // other kind, with an explicit Manage action routing to the management ledger.

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

namespace {

// A recipe's display name for a construction-ledger row: "food_rations" ->
// "Food Rations". Row names must be UNIQUE — the candidate card's ImGui id is its
// name — so each processing recipe gets its own readable label (BL-162).
std::string pretty_recipe(const std::string& raw)
{
    std::string out = raw;
    bool at_start = true;
    for (char& ch : out)
    {
        if (ch == '_')
        {
            ch = ' ';
            at_start = true;
            continue;
        }
        if (at_start && ch >= 'a' && ch <= 'z')
            ch = static_cast<char>(ch - 'a' + 'A');
        at_start = false;
    }
    return out;
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
    struct candidate
    {
        building_type type;
        resource_type target;
        std::string   name;
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
                             std::string("Extraction: ") + resource_name(er)});
    // Fishing Wharf (BL-168): agricultural_produce has no terrestrial deposit here,
    // so the loop above skips it — but a coastal tile can still work it. Without this,
    // the ledger never offers the one candidate whose whole point is a zero-deposit tile.
    if (tile.resource_deposit[static_cast<std::size_t>(resource_type::agricultural_produce)] <= 0.0f
        && placement_rules::is_coastal(w, tile_id))
        cands.push_back({building_type::extraction_site, resource_type::agricultural_produce,
                         std::string("Extraction: ") + resource_name(resource_type::agricultural_produce)});
    // One processing row per recipe, each priced on its own economics.
    for (int ri = 0; ri < reg.recipe_count(building_type::processing_facility); ++ri)
        cands.push_back({building_type::processing_facility, resource_type::iron_ore,
                         "Processing: " + pretty_recipe(
                             reg.recipe_at(building_type::processing_facility, ri).name),
                         static_cast<std::uint16_t>(ri)});
    cands.push_back({building_type::port,                 resource_type::iron_ore, "Port"});
    cands.push_back({building_type::launchpad,            resource_type::iron_ore, "Launchpad"});
    cands.push_back({building_type::inland_logistics_hub, resource_type::iron_ore, "Inland Logistics Hub"}); // BL-149
    cands.push_back({building_type::military_base,        resource_type::iron_ore, "Military Base"});        // BL-325 S1

    // BL-326: fold group by building family — Ben rejected the profit-ranked flat list
    // for named, expandable groups. Assigned by type rather than parsed from the display
    // name, so a future rename of "Extraction: X" can't silently drop a row into the
    // wrong group.
    for (candidate& c : cands)
        c.category = (c.type == building_type::extraction_site)     ? "Extraction"
                    : (c.type == building_type::processing_facility) ? "Processing"
                    : (c.type == building_type::military_base)       ? "Military"
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
            icons::building(cdl, {gp.x + gr, gp.y + ImGui::GetTextLineHeight() * 0.5f}, gr,
                            c.type, IM_COL32(150, 235, 160, 255));
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
        const auto  avail = has_muster_base ? available_rows(w, w.player_entity, campaign_roster_band)
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
