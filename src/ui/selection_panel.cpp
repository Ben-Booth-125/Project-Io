#include "selection_panel.hpp"

#include "core/battle_dispatch_text.hpp" // battle_phase_word — SHARED with BL-468's dispatches
#include "world/battle_system.hpp"      // active_battle, quote_withdrawal, read_battle_phase (BL-469)

#include "building_state.hpp" // BL-1239: the one running-state classification (hover card, Production, Status)
#include "charts.hpp"
#include "construction_panel.hpp" // building_group_name - the aim of the open-in-ledger button

#include "foldout_column.hpp" // shell fold-out column host (shared with the ledgers)
#include "format.hpp"         // fmt::credits — the god-view corp readout (BL-408)
#include "hex_render.hpp"      // draw_tile_neighbourhood — the card's zoomed tile view
#include "icons.hpp"
#include "market_ledger.hpp"   // market_city_name — a building's market, by city
#include "nav_pane.hpp"        // close_all_panels — the card's doors onto the fold-out column
#include "presentation.hpp"
#include "selection.hpp"
#include "text_fit.hpp"
#include "view_nav.hpp"

#include "world/budget_system.hpp"    // compute_building_opex / body_mean_habitability (BL-162 estimate)
#include "world/building_profit.hpp" // per-building profitability estimate (BL-074)
#include "world/condition_set.hpp"   // condition_text — the contract card's predicate (BL-577)
#include "world/corp_ai.hpp"         // corp_reserve_floor / corp_should_have_buffer — god-view corp readout (BL-408)
#include "world/economy_system.hpp" // economy_report (workforce cap, BL-069)
#include "world/market_clearing.hpp"
#include "world/construction.hpp"    // demolish path (the building element's Demolish)
#include "world/logistics.hpp"       // invalidate_logistics_caches (idle/resume flips the anchor set)
#include "world/placement_rules.hpp" // buildable-type validity + stack capacity
#include "world/province.hpp"        // BL-534: province membership + BL-513 building ceiling
#include "world/settlement.hpp"      // BL-785: region_domain_of - the three-domain word for a water tile
#include "world/recipe_registry.hpp" // recipe/economics lookups for the building element
#include "world/survey_system.hpp"
#include "world/nation_tech.hpp"     // BL-593: recipe_unlocked — the door filters what the gate would refuse
#include "world/unit_roster.hpp" // campaign hire gate + roster table (BL-324); also the Soldier card's Roster page name lookup

#include <map> // BL-434: group -> representative-candidate lookup in draw_construction_ledger_body

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

// Builds the "Input cost" segment's tooltip suffix: the recipe's non-zero
// input resources, NAMES ONLY (2026-08-15, folding the former standalone
// Inputs chart into the expenses hover — Ben's call was names, not
// quantities, since the segment already shows the aggregate credit figure).
// Read by the expenses hover (draw_building_profit); @p ri may be null (extraction
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
                        building_type type, resource_type target, uint16_t recipe,
                        entity_id tile)
{
    const building_economics& econ = reg.economics(type);
    const float duration = econ.build_duration_ticks;
    if (duration <= 0.0f)
        return 1.0f; // instant build — never material-gated
    // BL-590: the material cost specific to THIS named building.
    const auto& material_cost_row = reg.resource_build_cost_for(type, target, recipe);

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
        const float need = material_cost_row[r] / duration;
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
// withheld channel (production) as a visible 'private' placeholder, so
// the asymmetry is legible rather than a silent omission. The panel never leaks
// more than the hover card's facts; markets remain the sanctioned public channel.
//
// BL-408: under spectator god view (@p god_view) the 'private' row opens into
// the real values — same rows, real numbers — so the lift reads as exactly the
// redaction coming off, not as a different panel. Everything shown is plain world
// state the UI already holds a const ref to; the blackboard export never runs
// through here.
void draw_rival_building_summary(const world& w, const recipe_registry& reg,
                                 entity_id id, bool god_view)
{
    const auto it = w.buildings.find(id);
    if (it == w.buildings.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    const building_component& b = it->second;

    // Owner corporation (public) — the building type already heads the panel.
    const entity_id owner = owner_corp_of(w, id);
    const auto cit = w.corporations.find(owner);
    if (cit != w.corporations.end())
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                           "Owner: %s", cit->second.name.c_str());

    if (god_view)
    {
        // Production: what it runs and at what staffing.
        if (b.type == building_type::extraction_site)
            ImGui::Text("Production:  %s \xc2\xb7 workforce %.0f%%",
                        resource_name(b.target_resource),
                        static_cast<double>(b.workforce_assigned) * 100.0);
        else if (const recipe* r = reg.get_recipe(b.recipe); r != nullptr)
            ImGui::Text("Production:  %s \xc2\xb7 workforce %.0f%%",
                        r->display_name.c_str(),
                        static_cast<double>(b.workforce_assigned) * 100.0);
        else
            ImGui::Text("Production:  workforce %.0f%%",
                        static_cast<double>(b.workforce_assigned) * 100.0);

        // BL-1265: no corporation holds a stockpile — production lands on the
        // shelf of the market whose catchment holds the building and is sold
        // there. The god-view row names that market; the shelf itself is a
        // public read on the Market ledger.
        if (w.tiles.count(b.tile) != 0)
        {
            const entity_id mkt = market_for_tile(w, b.tile);
            if (mkt != null_entity)
                ImGui::Text("Sells into:  %s", market_city_name(w, mkt).c_str());
        }
        ImGui::TextDisabled("Competitor \xe2\x80\x94 god view");
        return;
    }

    // Withheld channels as explicit 'private' teaching rows: the grey value makes
    // the asymmetry legible rather than a silent omission. Markets stay the
    // sanctioned public channel, so the player infers output from price, not here.
    ImGui::TextDisabled("Production:  private");
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

// --- The province building-availability table (BL-534) -----------------------
//
// Ben ruled the grain explicitly on 2026-08-22: PER PROVINCE THROUGHOUT, not per
// tile with a province figure in the footer. So every number here is a province
// number, and the selected tile serves only to name which province is meant.
//
// "Maximum buildings" for a resource is the province's PLACEMENT CAPACITY for it:
// summed stack_capacity over the member tiles that would actually accept an
// extraction site targeting it. It is deliberately computed through the same
// can_place_in_world / stack_capacity the placement seam uses, rather than
// re-deriving "a deposit is here so a mine could go here" — the two would drift,
// and a table that disagrees with what the build button will let you do is worse
// than no table.
namespace {

struct province_build_row
{
    resource_type resource;
    int           built = 0;   ///< Extraction sites in the province targeting it.
    int           max   = 0;   ///< Placement slots the province has for it.
    float         deposit = 0.0f; ///< Summed deposit across member tiles (a stock, BL-1300).
};

struct province_build_table
{
    bool  known    = false;    ///< False when the tile has no province at all.
    int   standing = 0;        ///< Buildings of EVERY type standing (BL-513).
    int   ceiling  = -1;       ///< The province total ceiling; -1 is UNKNOWN, not "full".
    std::vector<province_build_row> rows;
};

province_build_table province_builds(const world& w, entity_id tile_id)
{
    province_build_table t;
    const uint32_t pid = w.provinces.province_of(tile_id);
    if (pid == 0) return t;
    const province* pr = w.provinces.find(pid);
    if (pr == nullptr) return t;

    t.known    = true;
    t.standing = province_buildings_standing(w, pid);
    t.ceiling  = province_building_ceiling(w, pid);

    std::array<int, resource_count> built{};
    std::array<int, resource_count> cap{};
    std::array<float, resource_count> dep{};
    built.fill(0);
    cap.fill(0);
    dep.fill(0.0f);

    // What stands. Extraction sites only: this table answers "what can I dig
    // here", and a port or a hub has no target resource to file under.
    for (const auto& [bid, bc] : w.buildings)
    {
        if (bc.type != building_type::extraction_site) continue;
        if (w.provinces.province_of(bc.tile) != pid)    continue;
        const auto ri = static_cast<std::size_t>(bc.target_resource);
        if (ri < resource_count) ++built[ri];
    }

    // What could stand. One pass over the member tiles; a resource with no
    // deposit anywhere in the province never enters the table at all, so the
    // list is the province's own geology rather than the full 38-row roster.
    for (const entity_id tid : pr->tiles)
    {
        const auto it = w.tiles.find(tid);
        if (it == w.tiles.end()) continue;
        const tile_component& tc = it->second;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (tc.resource_deposit[r] <= 0.0f) continue;
            dep[r] += tc.resource_deposit[r];
            const auto rt = static_cast<resource_type>(r);
            // placement_result converts to bool by design (BL-071), so the plain
            // negation is the idiomatic call — it carries no `ok` enumerator, only
            // an ok() accessor. Only four resources are extractable at all today,
            // and can_place is what knows that, so this table inherits the roster
            // rather than restating it.
            if (!placement_rules::can_place(tc, building_type::extraction_site, rt))
                continue;
            cap[r] += placement_rules::stack_capacity(tc, building_type::extraction_site, rt);
        }
        // The deposit-agnostic routes (stack cap 1: no richness): a Well on fresh
        // water (BL-1198) and a Fishing Wharf on the coast (BL-168). Without them
        // a standing Well read "water 1/0".
        if (placement_rules::is_well_site(w, tid, resource_type::water)
            && placement_rules::can_place(tc, building_type::extraction_site, resource_type::water))
            cap[static_cast<std::size_t>(resource_type::water)] += 1;
        if (placement_rules::is_wharf_site(w, tid, resource_type::agricultural_produce)
            && placement_rules::can_place(tc, building_type::extraction_site, resource_type::agricultural_produce))
            cap[static_cast<std::size_t>(resource_type::agricultural_produce)] += 1;
    }

    for (std::size_t r = 0; r < resource_count; ++r)
        if (cap[r] > 0 || built[r] > 0)
            t.rows.push_back({ static_cast<resource_type>(r), built[r], cap[r], dep[r] });

    // Richest DEPOSIT first (BL-1300: the table merged the former Deposits
    // section, whose order was this); room left breaks a tie. Stable over the
    // resource-index order the rows were built in, so the order is pinned.
    std::stable_sort(t.rows.begin(), t.rows.end(),
              [](const province_build_row& a, const province_build_row& b)
              {
                  if (a.deposit != b.deposit) return a.deposit > b.deposit;
                  return (a.max - a.built) > (b.max - b.built);
              });
    return t;
}

} // namespace

// The expanded building card's PROFIT ROW (BL-1300 round 2; SELECTION.md § The
// building element's layout, item 1 — Ben, 2026-10-13). LEFT, a short "Net, 6 mo."
// line chart; RIGHT, the net profit per quarter as one figure coloured by sign,
// with expenses per quarter below it as one figure whose hover splits it into
// input cost / maintenance / wages (NR-248). The revenue-against-stacked-expenses
// column chart retired: one number says what the column pair said, in a fraction
// of the height. A Tick is a quarter (BL-095), so the per-tick estimate IS the
// per-quarter figure. Realised last-tick figures; revenue/inputs are estimates
// (the pooled market resists exact per-building attribution — building_profit.hpp).
//
// @p net_out / @p exp_out (optional): the two figures as drawn, for the verify readout.
void draw_building_profit(const world& w, const recipe_registry& reg,
                          const economy_report& report, entity_id id,
                          std::string* net_out, std::string* exp_out)
{
    const building_profit p = estimate_building_profit(w, reg, report, id);
    if (!p.has_data)
    {
        ImGui::TextDisabled("Run an economy quarter to estimate.");
        return;
    }

    const ImGuiStyle& style  = ImGui::GetStyle();
    const float       box_w  = ImGui::GetContentRegionAvail().x;
    const float       line_h = ImGui::GetTextLineHeight();
    // The row is two text lines tall: the chart matches the two figures beside it.
    const float       row_h  = line_h * 2.0f + style.ItemSpacing.y;
    const float       net    = p.net();
    const float       exp    = p.input_cost + p.maintenance + p.wages;

    const std::string net_s = fmt::rate(net, "qtr");
    const std::string exp_s = fmt::credits(exp) + " / qtr";
    const float fig_w  = std::max(ImGui::CalcTextSize(net_s.c_str()).x,
                                  ImGui::CalcTextSize(("Exp " + exp_s).c_str()).x);
    const float chart_w = std::max(40.0f, box_w - fig_w - style.ItemSpacing.x);

    // No per-building profit HISTORY is tracked (the pooled market attribution
    // problem means nothing to reconstruct a past series from), so this is the
    // honest placeholder (NR-249): a smooth deterministic series anchored to the
    // live net estimate, 6 points for "6 months".
    constexpr int N = 6;
    float         series[N];
    for (int i = 0; i < N; ++i)
    {
        const float t   = static_cast<float>(i) / (N - 1);
        const float dip = -0.15f * std::fabs(net) * std::sin(t * 3.14159265f * 1.4f);
        series[i]       = net + dip;
    }
    ImGui::PlotLines("##net_profit", series, N, 0, "Net, 6 mo.", FLT_MAX, FLT_MAX, {chart_w, row_h});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Net over the last six months - a placeholder curve through\n"
                          "the live estimate; no per-building history is recorded yet.");
    ImGui::SameLine();

    ImGui::BeginGroup();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::value_colour(static_cast<double>(net))),
                       "%s", net_s.c_str());
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Net profit per quarter: revenue %s less expenses %s.",
                          fmt::credits(p.revenue).c_str(), fmt::credits(exp).c_str());
    ImGui::TextDisabled("Exp %s", exp_s.c_str());
    if (ImGui::IsItemHovered())
    {
        // The input basket's NAMES ride on the input-cost line (Ben, 2026-08-15:
        // names, not quantities). Extraction sites have no recipe.
        const recipe* ri = nullptr;
        if (const auto bit = w.buildings.find(id);
            bit != w.buildings.end() && bit->second.type == building_type::processing_facility)
            ri = reg.get_recipe(bit->second.recipe);
        const std::string names = input_basket_names(ri);
        ImGui::BeginTooltip();
        ImGui::TextUnformatted("Expenses per quarter");
        ImGui::Separator();
        ImGui::Text("Input cost   %s", fmt::credits(p.input_cost).c_str());
        if (!names.empty())
            ImGui::TextDisabled("  Inputs: %s", names.c_str());
        ImGui::Text("Maintenance  %s", fmt::credits(p.maintenance).c_str());
        ImGui::Text("Wages        %s", fmt::credits(p.wages).c_str());
        ImGui::EndTooltip();
    }
    ImGui::EndGroup();

    if (net_out) *net_out = net_s;
    if (exp_out) *exp_out = exp_s;
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
        // the tile element with its stack expanded (draw_tile_selection, BL-1300).

        // selection_kind::unit no longer routes here (this rework) — it takes
        // the dedicated 3-column band (draw_unit_selection_body), same shape
        // as the building/tile cards.

        case selection_kind::market:
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Locate");
            if (ImGui::Button("Go to"))
                focus_on_entity(w, ui, sel);
            ImGui::Separator();
            // BL-1269: the convoy dispatch form retired with corporation pools —
            // goods leave a market only by a TRADE (TRADE.md), and trades are
            // set on the Market ledger's Trades tab. The card is the door onto
            // it: the ledger follows the selected market, so it opens aimed.
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Trade");
            if (ImGui::Button("Trades"))
            {
                close_all_panels(ui);
                ui.show_market_ledger = true;
                ui.market_ledger_view = 1; // Trades
                ui.market_ledger_focus = sel; // aim it even if the selection is unchanged
            }
            ImGui::SetItemTooltip("Open this market's trades in the Market ledger.");
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

} // namespace

// Drawn at namespace scope rather than file-local since Ben's 2026-08-29 ruling that
// the Selection card's centre presents and never operates: this page's whole content
// is a control, so its CALLER is now the Construction ledger's Buildings view
// (construction_panel.cpp) and it needs external linkage. The body is untouched — the
// point of relocating rather than reimplementing is that there stays exactly one
// recipe switcher writing try_switch_recipe. The file-local helpers it calls
// (tile_icon_button, glyph_swap) stay in the anonymous namespace above; unqualified
// lookup still finds them from here.
// The method grid's last refused Switch press (presentation only: which
// building, and the seam's reason). One slot — only one building's grid is
// ever on screen.
static entity_id            s_switch_refused_building = null_entity;
static recipe_switch_result s_switch_refused_result   = recipe_switch_result::applied;

std::vector<int> method_grid_candidates(const world& w, const recipe_registry& reg,
                                        const building_component& b)
{
    // Same-group filter (BL-434 cross-group retraction): a cross-group recipe is
    // no longer a legal switch target at all, so it is never OFFERED. A null
    // active recipe (no resolvable group, e.g. mid-construction) falls back to
    // every era-allowed recipe — the no-group fallback try_switch_recipe uses.
    const recipe* cur = reg.get_recipe(b.recipe);
    const int total_n = reg.recipe_count(b.type);
    std::vector<int> candidates;
    candidates.reserve(static_cast<std::size_t>(std::max(total_n, 0)));
    for (int i = 0; i < total_n; ++i)
    {
        const recipe& ri = reg.recipe_at(b.type, i);
        // The body's air (Ben, 2026-10-09; BL-593, "the door not showing what the
        // gate would refuse"): a route try_switch_recipe refuses as `wrong_air`
        // is never offered. The ACTIVE recipe is always listed, whatever it is.
        const bool is_active = (cur != nullptr && ri.name == cur->name);
        if ((cur == nullptr || ri.group == cur->group)
            && (is_active || recipe_runs_at_tile(w, ri, b.tile)))
            candidates.push_back(i);
    }
    return candidates;
}

recipe_switch_result method_grid_press(world& w, const recipe_registry& reg, entity_id id,
                                       std::uint16_t recipe)
{
    const auto bit = w.buildings.find(id);
    if (bit == w.buildings.end())
        return recipe_switch_result::invalid;
    const recipe_switch_result r = try_switch_recipe(w, reg, w.player_entity, bit->second, recipe);
    s_switch_refused_building = (r == recipe_switch_result::applied) ? null_entity : id;
    s_switch_refused_result   = r;
    return r;
}

// Which building's grid DREW its status line on the last frame it drew (null when
// it drew none) — the surface reporting what it drew, for the air_gate check.
static entity_id s_status_drawn_for = null_entity;

entity_id method_grid_status_drawn_for() { return s_status_drawn_for; }

const char* method_grid_status(entity_id id)
{
    // The last Switch press on THIS building that the seam refused, said rather
    // than discarded. Cleared by the next press that applies.
    if (s_switch_refused_building != id || id == null_entity)
        return "";
    switch (s_switch_refused_result)
    {
        case recipe_switch_result::insufficient_funds: return "Switch refused: not enough funds.";
        case recipe_switch_result::tech_locked:        return "Switch refused: not researched yet.";
        case recipe_switch_result::on_cooldown:        return "Switch refused: still locked from the last switch.";
        case recipe_switch_result::wrong_air:          return "Switch refused: that method cannot run on this body's air.";
        case recipe_switch_result::cross_group:        return "Switch refused: a different kind of facility.";
        default: break;
    }
    return "Switch refused.";
}

void draw_production_method_section(world& w, const recipe_registry& reg, entity_id id)
{
    s_status_drawn_for = null_entity;
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
    const std::vector<int> candidates = method_grid_candidates(w, reg, b);

    if (candidates.size() <= 1)
    {
        ImGui::TextDisabled("Only one method available.");
        // A refused press (e.g. onto a route the body's air can't run, through the
        // seam) is still said here: the one-method grid is exactly the air-gated case.
        if (const char* why = method_grid_status(id); why[0] != '\0')
        {
            ImGui::TextDisabled("%s", why);
            s_status_drawn_for = id;
        }
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

            // BL-692: the BL-428 chain-depth lock this row used to show ("Your
            // industry cannot make what this needs yet", drawn greyed) is gone
            // along with the gate behind it. try_switch_recipe no longer refuses
            // on depth, so a greyed row here would be the UI inventing a refusal
            // the seam would not make. try_switch_recipe's other refusals:
            // cross-group and wrong-air recipes are never candidates (the filter
            // above), cooldown disables the control here, and a press the seam
            // still refuses (funds, tech) is said in the status line below — the
            // result is no longer discarded.
            const std::uint16_t row_id = reg.recipe_id(ri.name);

            char tip[96];
            if (on_cooldown)
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
            if (tile_icon_button("##switch", {glyph_w, row_h}, !on_cooldown,
                                 tip, glyph_swap, switch_glyph_col))
            {
                method_grid_press(w, reg, id, row_id);
            }
        }

        ImGui::EndChild();
        ImGui::PopID();
    }

    // The last Switch press on THIS building that the seam refused, said rather
    // than discarded. Cleared by the next press that applies.
    if (const char* why = method_grid_status(id); why[0] != '\0')
    {
        ImGui::TextDisabled("%s", why);
        s_status_drawn_for = id;
    }
}

namespace {

// BL-408 spectator god view: the full internals readout for a selected
// corporation — the readout BL-068 exists to withhold, drawn ONLY when the
// caller has already checked `ui.spectating && ui.god_view`. Cash, the
// solvency reserve floor and Should-Have buffer the scorer actually holds
// itself to, its trade points and manual trades, and each asset's running
// production — all plain reads over world state and the same public helpers
// (corp_reserve_floor / corp_should_have_buffer) the harnesses use. Strictly
// presentational: the blackboard export and the scorer never route through
// here, so what the AI knows is untouched by what the watcher sees.
void draw_corporation_god_facts(const world& w, const recipe_registry& reg,
                                const economy_report& report, entity_id id)
{
    const auto cit = w.corporations.find(id);
    if (cit == w.corporations.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    const corporation_component& c = cit->second;

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "God view");

    ImGui::Text("Cash: %s", fmt::credits(c.balance).c_str());
    ImGui::Text("Reserve floor: %s", fmt::credits(corp_reserve_floor(w, reg, id)).c_str());
    ImGui::Text("Should-have buffer: %s",
                fmt::credits(corp_should_have_buffer(w, reg, report, id)).c_str());

    // BL-1266: trade, in place of the retired per-market pools (BL-1265 — no
    // corporation holds goods; they sit on the markets' shelves). What its
    // Marketplaces and Ports made, what it reserves for manual trades, and how
    // many it has set.
    {
        int manual = 0;
        for (const standing_trade& t : w.trades)
            if (t.owner == id)
                ++manual;
        if (c.trade_points > 0.0f || manual > 0)
        {
            ImGui::Spacing();
            ImGui::Text("Trade: %.1f pts made \xc2\xb7 %.1f reserved \xc2\xb7 %d manual",
                        static_cast<double>(c.trade_points),
                        static_cast<double>(c.trade_reserve), manual);
        }
    }

    // Running production: one compact row per asset — what it runs, at what
    // staffing, and whether the tick-level agency has idled it.
    bool any_bld = false;
    for (const entity_id bid : c.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const building_component& b = bit->second;
        if (!any_bld)
        {
            ImGui::Spacing();
            ImGui::TextDisabled("Buildings");
            any_bld = true;
        }
        const char* what = "";
        if (b.type == building_type::extraction_site)
            what = resource_name(b.target_resource);
        else if (const recipe* r = reg.get_recipe(b.recipe); r != nullptr)
            what = r->display_name.c_str();
        const char* state = b.ticks_remaining > 0 ? " \xc2\xb7 constructing"
                          : b.decommissioned      ? " \xc2\xb7 idled"
                                                  : "";
        ImGui::Text("%s%s%s \xc2\xb7 wf %.0f%%%s%s",
                    building_type_name(b.type),
                    *what ? " \xc2\xb7 " : "", what,
                    static_cast<double>(b.workforce_assigned) * 100.0,
                    b.workforce_auto ? " (auto)" : "",
                    state);
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
        // draw_tile_selection (BL-1300).
        case selection_kind::corporation:
            // BL-408: a corp's facts column is empty today (its detail lives in
            // the ledgers). Under spectator god view it carries the internals
            // readout BL-068 withholds; with the flag off, empty as ever.
            if (ui.spectating && ui.god_view)
                draw_corporation_god_facts(w, reg, report, sel);
            break;
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
// BL-575 reuses this UNCHANGED for the unit card's Disband: the two share
// the same meaning ("erase this asset outright, no undo"), so the shape
// stays shared rather than growing a near-identical twin.
void glyph_dismantle(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddLine({c.x - r * 0.55f, c.y - r * 0.55f}, {c.x + r * 0.55f, c.y + r * 0.55f}, col, 2.4f);
    dl->AddLine({c.x + r * 0.55f, c.y - r * 0.55f}, {c.x - r * 0.55f, c.y + r * 0.55f}, col, 2.4f);
}

// A panel outline with one row picked out — the building card's "open this
// building in the Buildings ledger" glyph. Its own silhouette, deliberately: it
// sits inches from Mothball (a boxed line-through), Dismantle (an X) and Auto (an
// arc), and two lit controls sharing a shape is the confusion BL-174 named. Nothing
// else in this vocabulary draws STACKED HORIZONTAL rows, and the longer middle rule
// is the "this one" the button aims at.
void glyph_roster(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddRect({c.x - r * 0.68f, c.y - r * 0.62f}, {c.x + r * 0.68f, c.y + r * 0.62f},
                col, 2.0f, 0, 1.6f);
    dl->AddLine({c.x - r * 0.40f, c.y - r * 0.30f}, {c.x + r * 0.18f, c.y - r * 0.30f}, col, 1.8f);
    dl->AddLine({c.x - r * 0.40f, c.y},             {c.x + r * 0.42f, c.y},             col, 2.6f);
    dl->AddLine({c.x - r * 0.40f, c.y + r * 0.30f}, {c.x + r * 0.18f, c.y + r * 0.30f}, col, 1.8f);
}

// A shafted arrow pointing right — the unit card's March glyph (BL-575).
// Distinct from glyph_goto's bare chevron (which moves the CAMERA to the
// entity, not the entity itself) by carrying a full shaft behind the head —
// the plain "move this" pictogram, reading apart from "look at this".
void glyph_march(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    const float y = c.y;
    dl->AddLine({c.x - r * 0.62f, y}, {c.x + r * 0.30f, y}, col, 2.4f);
    dl->AddLine({c.x + r * 0.62f, y}, {c.x + r * 0.12f, y - r * 0.36f}, col, 2.4f);
    dl->AddLine({c.x + r * 0.62f, y}, {c.x + r * 0.12f, y + r * 0.36f}, col, 2.4f);
}

// Two filled vertical bars — the unit card's Halt glyph (BL-575): the
// universal "pause" pictogram, reading apart from the X (erase) and the
// arrow (move) it sits beside on the same grid.
void glyph_halt(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    const float bw  = r * 0.32f;
    const float bh  = r * 0.62f;
    const float gap = r * 0.20f;
    dl->AddRectFilled({c.x - gap - bw, c.y - bh}, {c.x - gap, c.y + bh}, col);
    dl->AddRectFilled({c.x + gap, c.y - bh}, {c.x + gap + bw, c.y + bh}, col);
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

// --- The expanded building card (BL-1300, tile card split) -----------------
//
// SELECTION.md § The building element's layout (Ben, 2026-10-13): a building is
// NOT a layout of its own. Selecting one draws its host tile's card with that
// building's stack expanded in the tile column, and the action grid retargets to
// it. The pager, its Status / Profitability pages and the full-canvas takeover
// retired with the standalone layout: everything the card says now fits one
// card — a status line, then the Profitability charts — so there is nothing to
// page between.

// The expanded card's STATUS LINE: the running state WITH ITS REASON, never a
// bare "Operating." (BL-1239), or the construction rate and ETA while the
// building is still building. Read from the same classification the hover card
// reads, so the two cannot disagree.
//
// @p open: the viewer may read this building's internals (own, or spectator god
// view). A rival's RUNNING STATE is private (DISCOVERY.md, the operational fog —
// "Idle: input short: Steel" is a recipe and a supply problem), so for a closed
// building only the construction line is drawn; construction itself stands on
// the map. Returns the line as drawn ("" when none) for the verify readout.
static std::string draw_building_status_line(const world& w, const recipe_registry& reg,
                                      const economy_report& report, entity_id id, bool open)
{
    const auto bit = w.buildings.find(id);
    if (bit == w.buildings.end())
        return {};
    const building_component& b = bit->second;
    std::string line;
    ImGui::PushTextWrapPos(0.0f);
    if (b.ticks_remaining > 0)
    {
        const float rate = construction_rate(w, reg, b.type, b.target_resource, b.recipe, b.tile);
        line = construction_status(rate, b.ticks_remaining);
        ImGui::TextColored(construction_status_colour(rate), "%s", line.c_str());
    }
    else if (open)
    {
        const building_running_state rs = classify_building_running(w, &reg, &report, id, b);
        line = running_state_text(rs);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(running_state_colour(rs.kind)), "%s",
                           line.c_str());
    }
    ImGui::PopTextWrapPos();
    return line;
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
    // Mothball — draw_building_actions) — this page is trend chart +
    // slider only now. Its OLD button here was the "game breaking bug": the
    // label baked `workforce_target` straight into the id-bearing text
    // (`"Auto  (%d%%)"`), and with no `##` separator ImGui derives a button's
    // ID from its full label — so every tick solve_workforce_target changed
    // the target, the button's ID churned too, corrupting ImGui's
    // hover/active/focus identity for it frame over frame. The action-grid
    // replacement keeps the id stable ("##bld_auto") and shows the percentage
    // as separate adjacent text, never inside the id string.

    draw_workforce_slider(b);
}

// The ONE writer of a building's workforce target from the UI (BL-1300 round 2).
// The Construction ledger's Buildings view (through draw_building_workforce_page)
// and the expanded building card on the Selection band both call this body, so
// there is one control writing `workforce_target`, never two that could come to
// disagree. A manual edit pins the target and clears Auto. The current
// percentage reads to the slider's RIGHT (Ben, 2026-10-13), never inside an
// id-bearing label.
bool draw_workforce_slider(building_component& b)
{
    int         target = b.workforce_target;
    char        pct[16];
    std::snprintf(pct, sizeof pct, "%d%%", target);
    const float pct_w = ImGui::CalcTextSize("100%").x;
    ImGui::SetNextItemWidth(std::max(40.0f, ImGui::GetContentRegionAvail().x - pct_w
                                                - ImGui::GetStyle().ItemSpacing.x));
    bool changed = false;
    if (ImGui::SliderInt("##wf_slider", &target, 0, 100, ""))
    {
        b.workforce_target = target;
        b.workforce_auto   = false; // manual edit pins the target, same as a tier press used to
        changed = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Workforce target. A manual edit turns Auto off.");
    ImGui::SameLine();
    ImGui::Text("%d%%", b.workforce_target);
    return changed;
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

// The unit card body: the 3-column band shape the building card once had —
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

    // ── Right quarter: 2x3 action grid — Go to, plus March/Halt/Disband
    // (BL-575, replacing three of the five reserved slots) ──
    {
        ImGui::BeginChild("##unit_actions", {right_w, total_h}, false,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
        const float  bw  = (right_w - spacing) / 2.0f;
        const float  bh  = (total_h - 2.0f * spacing) / 3.0f;
        const ImVec2 bsz = {bw, bh};

        // Units carry no corp.assets entry (unlike buildings), so ownership is
        // read straight off unit_component::owner rather than through
        // is_player_owned (which walks corp.assets and would always miss a
        // unit). A rival's unit is intel-only: every press below is disabled.
        const bool owned = (uit->second.owner == w.player_entity);

        if (tile_icon_button("##unit_goto", bsz, /*enabled=*/true, "Go to", glyph_goto))
            focus_on_entity(w, ui, sel);
        ImGui::SameLine();

        // March (BL-575): arms province-picking mode on the Planetary canvas
        // (body_surface_canvas.cpp reads pending_march_unit and, on the next
        // province click, fills pending_march_dest_province for app::render
        // to dispatch as corp_verb::march_unit). Visible armed state via the
        // same accent-ring "primed" idiom the building card's Auto button
        // uses — and per the project's toggle rule (any control whose active
        // state is visible is a toggle), pressing March again while armed for
        // THIS unit cancels the pick rather than re-arming it.
        {
            const bool  armed = owned && (ui.pending_march_unit == sel);
            const char* tip   = !owned ? "Competitor unit - intel only"
                               : armed ? "March - click a province to send this unit (click again to cancel)"
                                       : "March - pick a destination province";
            if (armed)
            {
                const ImVec2 pmin = ImGui::GetCursorScreenPos();
                dl->AddRect({pmin.x - 2.0f, pmin.y - 2.0f}, {pmin.x + bsz.x + 2.0f, pmin.y + bsz.y + 2.0f},
                           palette::selection, 4.0f, 0, 2.0f);
            }
            if (tile_icon_button("##unit_march", bsz, owned, tip, glyph_march) && owned)
                ui.pending_march_unit = armed ? null_entity : sel;
        }

        // Halt (BL-575): clears the movement order immediately — no
        // destination pick needed, unlike March — deferred through
        // pending_halt_unit for app::render, the same shape as every other
        // world-mutating press on this card (UI surfaces hold only
        // `const world&`).
        {
            const char* tip = owned ? "Halt - clear this unit's movement order"
                                    : "Competitor unit - intel only";
            if (tile_icon_button("##unit_halt", bsz, owned, tip, glyph_halt) && owned)
                ui.pending_halt_unit = sel;
        }
        ImGui::SameLine();

        // Disband (BL-575): permanent, confirm popup — same pattern as the
        // building card's Dismantle (no refund either way, MILITARY.md §
        // Marching; a confirm step is the UI's call, not the seam's).
        {
            const char* tip = owned ? "Disband - permanent. No refund; manpower walks away."
                                    : "Competitor unit - intel only";
            if (tile_icon_button("##unit_disband", bsz, owned, tip, glyph_dismantle) && owned)
                ImGui::OpenPopup("confirm_disband");
        }
        if (ImGui::BeginPopup("confirm_disband"))
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                               "Disband this unit?");
            ImGui::TextDisabled("No refund. This cannot be undone.");
            ImGui::Separator();
            if (ImGui::Button("Disband"))
            {
                ui.pending_disband_unit = sel;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Keep"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        tile_icon_button("##unit_reserved4", bsz, /*enabled=*/false, "Reserved", glyph_reserved);
        ImGui::SameLine();
        tile_icon_button("##unit_reserved5", bsz, /*enabled=*/false, "Reserved", glyph_reserved);

        ImGui::EndChild();
    }
}

// The action grid RETARGETED to an expanded building (BL-1300; SELECTION.md
// § The building element's layout, item 2). The same 2x3 cell layout as the
// tile's grid, holding the building's four actions — the element's designated
// place to ACT, while the cards present. Lifted out of the retired standalone
// building layout unchanged: same ids, same toggles, same deferral seams.
void draw_building_actions(world& w, const recipe_registry& reg, ui_state& ui, entity_id sel,
                           float right_w, float total_h)
{
    const auto bit = w.buildings.find(sel);
    if (bit == w.buildings.end())
        return;
    building_component& building = bit->second; // mutable: Mothball flips decommissioned directly
    const ImGuiStyle&   style    = ImGui::GetStyle();
    ImDrawList*         dl       = ImGui::GetWindowDrawList();
    const float         spacing  = style.ItemSpacing.x;

    ImGui::BeginChild("##building_actions", {right_w, total_h}, false,
                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
    const float  bw  = (right_w - spacing) / 2.0f;
    const float  bh  = (total_h - 2.0f * spacing) / 3.0f;
    const ImVec2 bsz = {bw, bh};

    // Deliberately NOT lifted by spectator god view (BL-408): god view
    // grants sight, never hands — a rival's Mothball/Dismantle/Auto stay
    // disabled under it, so the lift stays strictly presentational.
    const bool owned = is_player_owned(w, sel);

    // Mothball — reversible, no output/no wages while closed. Toggle button
    // (Toggle rule, io-standing-rules.md): its own active state
    // (decommissioned) is what re-clicking undoes.
    {
        const bool mothballed = building.decommissioned;
        const char* tip = !owned      ? "Competitor building - intel only"
                         : mothballed ? "Reopen - resumes output and wages"
                                      : "Mothball - stops output and wages. Reversible.";
        // Distinct glyph per state (2026-08-15) — a closed box while active, an
        // open one once mothballed — so re-clicking doesn't look like the same
        // action twice.
        if (tile_icon_button("##bld_mothball", bsz, owned, tip,
                             mothballed ? glyph_reopen : glyph_mothball) && owned)
        {
            building.decommissioned = !building.decommissioned;
            invalidate_logistics_caches(w); // a (un)decommissioned port/hub (un)anchors supply
        }
    }
    ImGui::SameLine();

    // Dismantle — permanent, confirm popup, deferred through pending_demolish
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

    // Auto (workforce autosolve), in Mothball's column one row down. One-way:
    // a manual slider edit in the Buildings view is what clears it. The active
    // state is the accent ring, never baked into the label — the id
    // "##bld_auto" stays stable while solve_workforce_target re-solves.
    {
        const bool  auto_on = building.workforce_auto;
        const char* tip     = !owned  ? "Competitor building - intel only"
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

    // Open in ledger — opens the Construction ledger's Buildings view AIMED at
    // this building (its type group expanded, the building selected), so the
    // levers that left the centre (Method, Workforce) are on screen in one press.
    // A DOOR, not a toggle: its active state is not visible here. ABSENT on a
    // rival's card — the Buildings view is the player's estate — and its cell
    // falls back to Reserved so the grid keeps its shape.
    if (owned)
    {
        const ImVec2 p_open = ImGui::GetCursorScreenPos();
        ui.construction_ui.open_ledger_x = p_open.x + bsz.x * 0.5f;
        ui.construction_ui.open_ledger_y = p_open.y + bsz.y * 0.5f;
        if (tile_icon_button("##bld_open_ledger", bsz, true,
                             "Open this building in the Buildings ledger",
                             glyph_roster))
        {
            close_all_panels(ui);
            ui.show_construction_panel = true;
            ui.construction.panel_view = 1; // Buildings
            ui.construction.buildings_expanded = building_group_name(w, reg, building);
            ui.selected_entity = sel; // already true; stated because the aim depends on it
        }
    }
    else
    {
        ui.construction_ui.open_ledger_x = -1.0f;
        ui.construction_ui.open_ledger_y = -1.0f;
        tile_icon_button("##bld_reserved3", bsz, /*enabled=*/false, "Reserved", glyph_reserved);
    }

    tile_icon_button("##bld_reserved4", bsz, /*enabled=*/false, "Reserved", glyph_reserved);
    ImGui::SameLine();
    tile_icon_button("##bld_reserved5", bsz, /*enabled=*/false, "Reserved", glyph_reserved);

    ImGui::EndChild();
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

// ---------------------------------------------------------------------------
// The battle card (BL-469)
// ---------------------------------------------------------------------------
// The question it answers, from the item's own question-log entry: "How is my
// battle going, and what does leaving cost right now?" The dispatch stream
// (BL-468) is the other half — that one narrates, this one COMMANDS.
//
// It owns its whole layout, header included, the way the tile card does, because
// its top region is a map rather than the shared three-column band.
//
// VISIBILITY (BL-068): the player's own battle shows everything. A rival-vs-rival
// battle shows the participants, the round and who is winning — and NO per-unit
// detail, because that is a rival's internals. The withdraw press is disabled
// with a reason rather than hidden, so the rule reads as a rule instead of as a
// missing button.

/// Resolve the selected battle, or nullptr. Also clears a stale selection — a
/// battle ends and is erased, and a card pointing at one that finished should
/// fall back cleanly rather than draw nothing forever.
/// BL-1241: ask the ground layer for the neighbourhood page around @p tile
/// (baked next frame, so the view shows the canvas's own baked ground).
static void request_neighbourhood_page(const world& w, ui_state& ui, entity_id tile, int radius)
{
    const auto it = w.tiles.find(tile);
    if (it == w.tiles.end())
        return;
    ground_neigh_request& q = ui.ground_neigh_req;
    q.body   = it->second.body;
    q.tile   = tile;
    q.col    = it->second.grid_x;
    q.row    = it->second.grid_y;
    q.radius = radius;
    q.valid  = true;
}

const active_battle* selected_battle(const world& w, ui_state& ui)
{
    if (!ui.has_battle_selection())
        return nullptr;
    for (const active_battle& b : w.battles)
        if (b.province == ui.selected_battle_province
            && b.attacker == ui.selected_battle_attacker
            && b.defender == ui.selected_battle_defender)
            return &b;
    ui.clear_battle_selection(); // the fight ended; stop pointing at it
    return nullptr;
}

// The phase word is SHARED with the dispatch stream, not redefined here — BL-468
// requires that the two surfaces never disagree about what phase a fight is in,
// and a second copy of the vocabulary is exactly how they would come to.
// battle_phase_word lives in core/battle_dispatch_text.hpp.


/// One side's strength bar, with the decomposition the unit card already prints
/// kept intact — count x quality x supply — so an unsupplied unit VISIBLY fights
/// weaker here for the same reason it does there.
void draw_battle_side(const world& w, const char* label, entity_id corp,
                      const std::vector<entity_id>& units, int strength_permille,
                      bool show_units)
{
    const auto cit = w.corporations.find(corp);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", label);
    ImGui::SameLine();
    ImGui::TextUnformatted(cit != w.corporations.end() ? cit->second.name.c_str() : "Unknown");

    // The aggregate first: it is the number that answers "am I winning".
    ImGui::ProgressBar(std::clamp(strength_permille / 1000.0f, 0.0f, 1.0f),
                       ImVec2(-1.0f, 0.0f));
    ImGui::TextDisabled("%.1f%% of what marched in", strength_permille / 10.0f);

    if (!show_units)
    {
        // BL-068: a rival's internals stay private. Say so rather than leaving a
        // gap — an absent section reads as a bug, a stated rule reads as a rule.
        ImGui::TextDisabled("Composition unknown — a rival's forces are not yours to count.");
        return;
    }

    for (const entity_id uid : units)
    {
        const auto uit = w.units.find(uid);
        if (uit == w.units.end() || uit->second.count <= 0)
            continue;
        const unit_component& u = uit->second;
        ImGui::Text("%d men", u.count);
        ImGui::SameLine();
        ImGui::TextDisabled("x %.2f quality x %.2f supply = %lld",
                            roster_type_quality_permille(u.type) / 1000.0f,
                            u.supply_factor_permille / 1000.0f,
                            static_cast<long long>(unit_strength(w, u)));
        if (u.supply_factor_permille < 1000)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                               "  unsupplied");
        }
    }
}

void draw_battle_selection(world& w, ui_state& ui)
{
    // The caller has already resolved and validated the pointer — a stale battle
    // selection falls through to the ordinary kind resolution there rather than
    // being rendered as an empty card here.
    const active_battle& b = *selected_battle(w, ui);

    const entity_id player = w.player_entity;
    const bool mine = (b.attacker == player || b.defender == player);
    // The spectator god view lifts the redaction in the UI ONLY (BL-408) — the
    // same pair-test every other read-site uses, never a world-side change.
    const bool show_all = mine || (ui.spectating && ui.god_view);

    const campaign_battle_params params{};
    const battle_phase phase =
        read_battle_phase(b, 0, 0, params); // no swing context on a redraw; state alone

    // --- header -----------------------------------------------------------
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Battle");
    ImGui::SameLine();
    ImGui::Text("%s", battle_phase_word(phase));
    ImGui::SameLine();
    ImGui::TextDisabled("round %d / %d", b.state.rounds_fought, params.max_rounds);
    ImGui::Separator();

    // --- the map ----------------------------------------------------------
    // The province's hexes drawn large, with each participating unit a marker on
    // its OWN tile — positions are real, because ruling 1 kept them: the province
    // frames the fight, it does not pool the forces into it.
    //
    // FRAMED ROUND THE GROUND THE FIGHT IS ON (BL-1145): the battle's own
    // `ground_tile`, the defender tile the resolver read its terrain from,
    // stored at open — so the frame is the battle's, and does not wander as the
    // units it was picked from move or die. Falls back (a record with none) to
    // the province anchor (`province_anchor_tile`, itself falling back to the
    // lowest-id tile when no centre stands there) — never the lowest-id tile
    // first, which in a province of a thousand tiles frames empty country.
    if (const province* pv = w.provinces.find(b.province); pv && !pv->tiles.empty())
    {
        entity_id frame = b.ground_tile;
        if (frame == null_entity || w.tiles.find(frame) == w.tiles.end())
            frame = province_anchor_tile(w, *pv);
        const float avail = ImGui::GetContentRegionAvail().x;
        const float h     = std::min(avail * 0.55f, 220.0f);
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        request_neighbourhood_page(w, ui, frame, 2);
        draw_tile_neighbourhood(dl, w, frame, origin, ImVec2(avail, h), 2, &ui.ground);
        ImGui::Dummy(ImVec2(avail, h));
    }

    // --- the two sides ----------------------------------------------------
    draw_battle_side(w, "Attacker", b.attacker, b.attacker_units,
                     b.state.attacker_strength_permille,
                     show_all || b.attacker == player);
    ImGui::Spacing();
    draw_battle_side(w, "Defender", b.defender, b.defender_units,
                     b.state.defender_strength_permille,
                     show_all || b.defender == player);

    // --- the withdraw press ----------------------------------------------
    // THE ONE DECISION THE SPAN CONTAINS, and the reason the card exists at all.
    // The price is quoted from the resolver's own arithmetic (quote_withdrawal)
    // rather than recomputed here: a second copy in a surface is a second place
    // for the quoted price to drift from the price actually charged.
    ImGui::Spacing();
    ImGui::Separator();
    if (!mine)
    {
        ImGui::BeginDisabled();
        ImGui::Button("Withdraw");
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("Not your fight.");
    }
    else
    {
        const withdrawing_side side = (b.attacker == player) ? withdrawing_side::attacker
                                                             : withdrawing_side::defender;
        const withdrawal_price q = quote_withdrawal(b, side, params);
        const bool already = (b.withdraw_requested != withdrawing_side::none);

        ImGui::BeginDisabled(already);
        if (ImGui::Button("Withdraw"))
            ImGui::OpenPopup("confirm_withdraw");
        ImGui::EndDisabled();
        ImGui::SameLine();
        // All three terms legible, and updating as rounds pass — the per-round
        // term is what makes leaving late dear, so hiding it inside a total would
        // hide the shape of the decision.
        ImGui::TextDisabled("costs %.1f%%  (base %.1f + %d rounds %.1f + pursuit %.1f)",
                            q.total / 10.0f, q.base / 10.0f,
                            b.state.rounds_fought, q.per_round / 10.0f, q.pursuit / 10.0f);
        if (already)
            ImGui::TextDisabled("Withdrawal ordered — it takes effect next tick.");

        // Confirm on press, the declare_hostile pattern (BL-449): breaking off is
        // priced and irreversible, so it is not a thing to do by mis-click.
        if (ImGui::BeginPopup("confirm_withdraw"))
        {
            ImGui::Text("Break off, forfeiting %.1f%% of your remaining force?", q.total / 10.0f);
            if (ImGui::Button("Break off"))
            {
                ui.construction.pending_withdraw_province = b.province;
                ui.construction.pending_withdraw_against =
                    (b.attacker == player) ? b.defender : b.attacker;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Hold"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
}

// --- The PROVINCE readings the tile element folds in (BL-598) ---------------
//
// Ben, 2026-08-24: "Province selection element must be bundled into the tile
// selection element." Bundled in means the province is a set of SECTIONS of the
// one element, so these readers take the selected TILE and resolve its province
// themselves — there is no province selection left to pass in.
//
// Nothing here assumes a province SIZE: the member list is walked and summed
// over, never indexed against a constant, so a repartition changes only how many
// tiles feed the total. Deposits and population stay where they live (tiles and
// population centres respectively); these read over them, they do not move them.

struct province_deposit_row
{
    resource_type resource;
    float         amount = 0.0f;
};

/// Deposits summed across the selected tile's province, richest first.
///
/// Summing is the right reduction because a deposit is a STOCK: for a player
/// deciding whether this locality is worth a mine, several tiles each holding a
/// little iron IS one province holding that much iron. The per-TILE yield is the
/// Resources section's question, and it is charted rather than summed — the two
/// sections ask different things about the same ground, which is why both earn a
/// place in one accordion.
std::vector<province_deposit_row> province_deposits(const world& w, entity_id tile_id)
{
    std::array<float, resource_count> dep{};
    dep.fill(0.0f);

    const uint32_t  pid = w.provinces.province_of(tile_id);
    const province* pr  = (pid != 0) ? w.provinces.find(pid) : nullptr;
    if (pr != nullptr)
    {
        for (const entity_id tid : pr->tiles)
            if (const auto it = w.tiles.find(tid); it != w.tiles.end())
                for (std::size_t r = 0; r < resource_count; ++r)
                    dep[r] += it->second.resource_deposit[r];
    }
    else if (const auto it = w.tiles.find(tile_id); it != w.tiles.end())
    {
        // Ocean and otherwise unpartitioned ground has no province, and the
        // section still has to say something TRUE rather than read as "nothing
        // here": fall back to the tile's own deposits. The draw site says which
        // grain it is showing, so the number is never ambiguous.
        for (std::size_t r = 0; r < resource_count; ++r)
            dep[r] += it->second.resource_deposit[r];
    }

    std::vector<province_deposit_row> rows;
    for (std::size_t r = 0; r < resource_count; ++r)
        if (dep[r] > 0.0f)
            rows.push_back({ static_cast<resource_type>(r), dep[r] });
    std::sort(rows.begin(), rows.end(),
              [](const province_deposit_row& a, const province_deposit_row& b)
              { return a.amount > b.amount; });
    return rows;
}

/// The five rungs of the population-centre scale ladder, POPULATION.md
/// § Scale bonus model. Canonical terms, not synonyms (standing rule: GLOSSARY
/// and the owning doc name the words that reach the screen).
const char* population_scale_name(int scale)
{
    switch (scale)
    {
        case 1:  return "Outpost";
        case 2:  return "Settlement";
        case 3:  return "Town";
        case 4:  return "City";
        default: return (scale >= 5) ? "Metropolis" : "Outpost";
    }
}

struct province_pop_row
{
    const char* name         = "";
    int         scale        = 1;
    int         population   = 0;     ///< Absolute headcount in thousands.
    float       habitability = 0.0f;
    bool        here         = false; ///< Stands on the SELECTED tile.
};

struct province_pop_table
{
    bool known = false;               ///< False when the tile has no province.
    int  total = 0;                   ///< Summed headcount, thousands.
    std::vector<province_pop_row> rows;
};

/// The population centres standing in the selected tile's province.
///
/// This section is province-grain because the DATA MODEL is: population lives on
/// population CENTRES, not on arbitrary tiles (POPULATION.md), so there is no
/// per-tile headcount to show and a per-tile section would have to invent one.
/// The centre that stands on the selected tile — if one does — is flagged, so the
/// list reads as "this ground, in its neighbourhood" rather than as a bare roster.
province_pop_table province_population(const world& w, entity_id tile_id)
{
    province_pop_table t;
    const uint32_t  pid = w.provinces.province_of(tile_id);
    const province* pr  = (pid != 0) ? w.provinces.find(pid) : nullptr;
    if (pr == nullptr)
        return t;
    t.known = true;

    for (const auto& [cid, pc] : w.population_centres)
    {
        const auto tit = w.population_centre_tile.find(cid);
        if (tit == w.population_centre_tile.end())
            continue;
        if (w.provinces.province_of(tit->second) != pid)
            continue;

        province_pop_row r;
        if (const auto nit = w.population_centre_name.find(cid);
            nit != w.population_centre_name.end())
            r.name = nit->second.c_str();
        r.scale        = pc.scale;
        r.population   = pc.population;
        r.habitability = pc.habitability;
        r.here         = (tit->second == tile_id);
        t.total       += pc.population;
        t.rows.push_back(r);
    }

    // `population_centres` is unordered, so pin the walk: largest first, which is
    // also the order the labour pool and the agglomeration bonus care about.
    std::sort(t.rows.begin(), t.rows.end(),
              [](const province_pop_row& a, const province_pop_row& b)
              {
                  if (a.population != b.population) return a.population > b.population;
                  return a.scale > b.scale;
              });
    return t;
}

// ── The TILE CARD (BL-1300, tile card split) ──────────────────────────────
//
// SELECTION.md § The tile element's layout, item 3 (Ben, 2026-10-13): one
// titled card, the buildings on this tile and nothing else. TILE GRAIN, because
// a building stands on a tile. One button per stack, three across: the type
// glyph with the stack's count, the building's name, and its net profit per
// quarter coloured by sign — a rival's reads "private". Running state is not on
// the button (it is the expanded card's status line, one press away), and the
// market and price rows that followed the stack rows retired with the 2026-10-13
// ruling: what a good fetches is the Market ledger's question.
//
// Pressing a button selects a building of the stack, and the band then draws
// this same tile with that stack EXPANDED in place (§ Multi-building tiles).
namespace {


/// One stack standing on the tile, in stack order (oldest member first).
///
/// KEYED BY (type, what it makes, owner) — not by `placement_rules::
/// stack_members` alone, which groups every non-extraction type by type only:
/// a Steel foundry and a Refinery on one tile would read as one row naming
/// one recipe's good, and a rival's building would fold into the player's row
/// (BL-1239 fix round). What-it-makes is the extraction target or the active
/// recipe — but only where the viewer may know it. A rival's recipe is private
/// (DISCOVERY.md § The operational fog), so a closed row keys on type and owner
/// alone: splitting it by recipe would itself tell the player how many recipes
/// the rival runs here.
struct production_stack
{
    building_type          type  = building_type::none;
    int                    makes = -1;          ///< Target index / recipe id; -1 = not keyed (closed or makes nothing).
    entity_id              owner = null_entity; ///< owner_corp_of the members.
    bool                   open  = false;       ///< Viewer's own, or spectator god view.
    std::vector<entity_id> members;             ///< Ascending id.
};

/// The good a building makes: an extraction site's target, a processing
/// facility's active recipe's primary output. False for a type that makes none.
bool good_made_by(const recipe_registry& reg, const building_component& b, resource_type& out)
{
    if (b.type == building_type::extraction_site)
    {
        out = b.target_resource;
        return true;
    }
    if (b.type == building_type::processing_facility)
        if (const recipe* rc = reg.get_recipe(b.recipe); rc != nullptr)
        {
            out = primary_output_resource(*rc);
            return true;
        }
    return false;
}

/// The stacks on @p tile, keyed as `production_stack` says, in the order of
/// each stack's oldest member — deterministic, never the unordered
/// `world::buildings` iteration order. UI grouping only: the world's own stack
/// rule (`placement_rules::stack_members`, capacity and placement) is untouched.
std::vector<production_stack> tile_stacks(const world& w, entity_id tile, bool god)
{
    std::vector<entity_id> here;
    for (const auto& [id, b] : w.buildings)
        if (b.tile == tile)
            here.push_back(id);
    std::sort(here.begin(), here.end());

    std::vector<production_stack> stacks;
    for (const entity_id id : here) // ascending, so each stack's first member is its oldest
    {
        const building_component& b = w.buildings.at(id);
        production_stack key;
        key.type  = b.type;
        key.owner = owner_corp_of(w, id);
        key.open  = god || is_player_owned(w, id);
        if (key.open)
        {
            if (b.type == building_type::extraction_site)
                key.makes = static_cast<int>(b.target_resource);
            else if (b.type == building_type::processing_facility)
                key.makes = (b.recipe == no_recipe) ? -1 : static_cast<int>(b.recipe);
        }
        std::size_t at = stacks.size();
        for (std::size_t k = 0; k < stacks.size(); ++k)
        {
            const production_stack& s = stacks[k];
            if (s.type == key.type && s.makes == key.makes && s.owner == key.owner &&
                s.open == key.open)
            {
                at = k;
                break;
            }
        }
        if (at == stacks.size())
            stacks.push_back(std::move(key));
        stacks[at].members.push_back(id);
    }
    return stacks;
}

/// A stack's third line on its button: net profit per quarter, "private" for a
/// closed (rival) stack, or the construction ETA while no member has finished.
/// A Tick is a quarter (BL-095), so `estimate_building_profit`'s per-tick net IS
/// the per-quarter figure the Profitability charts plot.
struct stack_reading
{
    std::string text;
    ImU32       colour = palette::text_secondary;
};

stack_reading read_stack(const world& w, const recipe_registry& reg,
                         const economy_report& report, const production_stack& st)
{
    stack_reading out;
    int        complete = 0;
    entity_id  building = null_entity; // the first member still under construction
    for (const entity_id m : st.members)
    {
        const building_component& b = w.buildings.at(m);
        if (b.ticks_remaining <= 0) ++complete;
        else if (building == null_entity) building = m;
    }

    // Under construction: the ETA, in place of profit. Construction stands on
    // the map, so this is public for a rival's stack too.
    if (complete == 0 && building != null_entity)
    {
        const building_component& b = w.buildings.at(building);
        const float rate = construction_rate(w, reg, b.type, b.target_resource, b.recipe, b.tile);
        out.colour = ImGui::ColorConvertFloat4ToU32(construction_status_colour(rate));
        if (rate <= 0.0f)
            out.text = "Paused";
        else
        {
            const int eta = static_cast<int>(std::ceil(static_cast<float>(b.ticks_remaining) / rate));
            out.text = "ETA " + std::to_string(eta) + (eta == 1 ? " qtr" : " qtrs");
        }
        return out;
    }

    if (!st.open)
    {
        out.text   = "private";
        out.colour = palette::neutral;
        return out;
    }

    float net  = 0.0f;
    bool  data = false;
    for (const entity_id m : st.members)
    {
        if (w.buildings.at(m).ticks_remaining > 0)
            continue;
        const building_profit p = estimate_building_profit(w, reg, report, m);
        if (!p.has_data)
            continue;
        data = true;
        net += p.net();
    }
    if (!data)
    {
        out.text = "\xe2\x80\x94 / qtr";
        return out;
    }
    out.text   = fmt::rate(net, "qtr");
    out.colour = ui::value_colour(static_cast<double>(net));
    return out;
}

/// The tile card's building grid: one button per stack, three across. Each
/// button is hand-drawn over an InvisibleButton so its three lines sit where the
/// card's narrow column needs them (~260 px at 1720x1080, so ~80 px a button).
void draw_tile_building_grid(const world& w, const recipe_registry& reg,
                             const economy_report& report, ui_state& ui,
                             const std::vector<production_stack>& stacks)
{
    if (stacks.empty())
    {
        // Stated positively: an empty grid and "Nothing built here" cost the
        // same pixels and only one of them reads as an answer.
        ImGui::TextDisabled("Nothing built here");
        return;
    }

    const ImGuiStyle& style  = ImGui::GetStyle();
    const float       line_h = ImGui::GetTextLineHeight();
    const float       sp     = style.ItemSpacing.x;
    const float       pad    = 4.0f;
    const float       gap    = 2.0f;
    const float       bw     = std::max(40.0f, (ImGui::GetContentRegionAvail().x - 2.0f * sp) / 3.0f);
    const float       bh     = line_h * 3.0f + gap * 2.0f + pad * 2.0f;
    ImDrawList*       dl     = ImGui::GetWindowDrawList();

    for (std::size_t si = 0; si < stacks.size(); ++si)
    {
        const production_stack&   st    = stacks[si];
        const entity_id           first = st.members.front();
        const building_component& b0    = w.buildings.at(first);
        const bool                mine  = is_player_owned(w, first);
        const int                 count = static_cast<int>(st.members.size());

        // What the stack makes — only for an OPEN stack. A rival's recipe is
        // private (DISCOVERY.md § The operational fog): its good is neither
        // named nor drawn into the glyph.
        resource_type good  = resource_type::iron_ore;
        const bool    makes = st.open && good_made_by(reg, b0, good);

        // Open: the group name ("Steel foundry"), which names the recipe.
        // Closed: the public building TYPE only.
        const std::string name = st.open ? building_group_name(w, reg, b0)
                                         : std::string(building_type_name(b0.type));
        const stack_reading rd = read_stack(w, reg, report, st);

        if (si % 3 != 0)
            ImGui::SameLine();
        ImGui::PushID(static_cast<int>(si));
        const ImVec2 p       = ImGui::GetCursorScreenPos();
        const bool   pressed = ImGui::InvisibleButton("##stack", {bw, bh});
        const bool   hovered = ImGui::IsItemHovered();
        const bool   held    = ImGui::IsItemActive();

        const ImU32 bg = held ? IM_COL32(62, 68, 84, 255)
                       : hovered ? IM_COL32(46, 50, 62, 255)
                                 : IM_COL32(28, 31, 40, 255);
        dl->AddRectFilled(p, {p.x + bw, p.y + bh}, bg, 4.0f);
        dl->AddRect(p, {p.x + bw, p.y + bh}, hovered ? IM_COL32(140, 145, 160, 255)
                                                     : IM_COL32(80, 82, 94, 255), 4.0f);

        // Line one: the type glyph, with the stack's count when above one (the
        // count the retired +N canvas badge used to carry). A closed stack passes
        // a good neither glyph family keys on (coal), so the glyph says the TYPE
        // and nothing about what it makes.
        const float         gr    = line_h * 0.45f;
        const resource_type ident = makes ? good : (st.open ? b0.target_resource : resource_type::coal);
        icons::building(dl, {p.x + pad + gr, p.y + pad + line_h * 0.5f}, gr, b0.type, ident,
                        mine ? IM_COL32(190, 200, 210, 255) : IM_COL32(150, 154, 166, 255));
        if (count > 1)
        {
            char cnt[16];
            std::snprintf(cnt, sizeof cnt, "\xc3\x97%d", count);
            dl->AddText({p.x + pad + gr * 2.0f + 4.0f, p.y + pad}, palette::text_secondary, cnt);
        }
        // Line two: the building's name. Line three: the reading.
        add_fit_text(dl, text_box::table_cell, "selection.tile.stack_name",
                     {p.x + pad, p.y + pad + line_h + gap}, bw - pad * 2.0f,
                     palette::selection, name.c_str());
        add_fit_text(dl, text_box::table_cell, "selection.tile.stack_reading",
                     {p.x + pad, p.y + pad + (line_h + gap) * 2.0f}, bw - pad * 2.0f,
                     rd.colour, rd.text.c_str());

        if (hovered)
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s%s", name.c_str(),
                        count > 1 ? (" \xc3\x97" + std::to_string(count)).c_str() : "");
            if (makes)
                ImGui::TextDisabled("Makes %s", resource_name(good));
            if (!mine)
            {
                const auto cit = w.corporations.find(st.owner);
                ImGui::TextDisabled("Owner: %s",
                                    cit != w.corporations.end() ? cit->second.name.c_str() : "unknown");
            }
            ImGui::TextDisabled("%s", rd.text.c_str());
            ImGui::TextDisabled("Press to expand.");
            ImGui::EndTooltip();
        }
        // Selects a building OF the stack (its oldest member); the band draws
        // this tile next frame with that stack expanded in place.
        if (pressed)
            ui.selected_entity = first;

        ui_state::tile_card_button readout;
        readout.label  = name + (count > 1 ? " \xc3\x97" + std::to_string(count) : std::string());
        readout.detail = rd.text;
        readout.good   = makes ? resource_name(good) : "";
        readout.open   = st.open;
        readout.count  = count;
        readout.first  = first;
        readout.x      = p.x + bw * 0.5f;
        readout.y      = p.y + bh * 0.5f;
        ui.tile_card.buttons.push_back(std::move(readout));
        ImGui::PopID();
    }
}

/// The tile column with one stack EXPANDED (SELECTION.md § The building
/// element's layout, item 1; round 2, Ben 2026-10-13). Top to bottom, laid out to
/// the card's height with no scrollbar:
///   - the header row: an unlabelled back control, the building name, the
///     stack's count (×N);
///   - the production method (the active recipe; an extraction site names its
///     good), then the status line;
///   - the profit row (draw_building_profit): a short net chart beside net and
///     expenses per quarter;
///   - the workforce row: the shared slider (draw_workforce_slider).
/// The profit and workforce rows need the player's own COMPLETED building with
/// profit data. A rival's card is the header, owner and an explicit `private`
/// row; under spectator god view it opens the method, real rows and profit row
/// (BL-408), but never the slider — god view grants sight, never hands.
void draw_expanded_building_card(world& w, const recipe_registry& reg,
                                 const economy_report& report, ui_state& ui, entity_id tile_id,
                                 entity_id bid, const std::vector<production_stack>& stacks)
{
    building_component& b    = w.buildings.at(bid); // mutable: the workforce slider writes it
    const bool          god  = ui.spectating && ui.god_view;
    const bool          mine = is_player_owned(w, bid);
    const bool          open = mine || god;

    int count = 1;
    for (const production_stack& st : stacks)
        for (const entity_id m : st.members)
            if (m == bid)
                count = static_cast<int>(st.members.size());
    ui.tile_card.count = count;

    // ── Header row: back, name, ×N ──
    if (ImGui::SmallButton("\xe2\x80\xb9##tile_back"))
        ui.selected_entity = tile_id; // the tile again: the grid returns
    {
        const ImVec2 mn = ImGui::GetItemRectMin();
        const ImVec2 mx = ImGui::GetItemRectMax();
        ui.tile_card.back_x = (mn.x + mx.x) * 0.5f;
        ui.tile_card.back_y = (mn.y + mx.y) * 0.5f;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Back to the buildings on this tile");
    ImGui::SameLine();
    {
        const std::string name = open ? building_group_name(w, reg, b) : std::string(building_type_name(b.type));
        char cnt[16] = "";
        if (count > 1)
            std::snprintf(cnt, sizeof cnt, " \xc3\x97%d", count);
        const float cnt_w = cnt[0] ? ImGui::CalcTextSize(cnt).x : 0.0f;
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(palette::selection));
        fit_text(text_box::table_cell, "selection.building.title", name.c_str(),
                 std::max(24.0f, ImGui::GetContentRegionAvail().x - cnt_w));
        ImGui::PopStyleColor();
        if (cnt[0])
        {
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextDisabled("%s", cnt);
        }
        ui.tile_card.title = name + cnt;
    }
    ImGui::Separator();

    // ── The production method, then the status line ──
    // Open only: a rival's recipe is private (DISCOVERY.md § The operational fog).
    if (open)
    {
        std::string method;
        if (b.type == building_type::extraction_site)
            method = std::string("Extracting ") + resource_name(b.target_resource);
        else if (b.type == building_type::processing_facility)
        {
            if (const recipe* rc = reg.get_recipe(b.recipe); rc != nullptr)
                method = rc->display_name;
        }
        if (!method.empty())
        {
            fit_text(text_box::table_cell, "selection.building.method", method.c_str(),
                     std::max(24.0f, ImGui::GetContentRegionAvail().x));
            ui.tile_card.method = method;
        }
    }
    ui.tile_card.expanded_player = mine;
    ui.tile_card.status = draw_building_status_line(w, reg, report, bid, open);

    // A rival: owner and the explicit `private` row (or, under god view, the
    // real rows). The short-circuit is the guarantee: nothing below this branch
    // is reached for a closed rival card, so a section added to the player's
    // card cannot leak onto a rival's by forgetting a guard.
    if (!mine)
    {
        draw_rival_building_summary(w, reg, bid, god);
        if (!god)
            return;
    }

    if (b.ticks_remaining > 0)
        return; // still building: header, method and status are the card
    const building_profit p = estimate_building_profit(w, reg, report, bid);
    if (!p.has_data)
        return;

    // ── The profit row ──
    ui.tile_card.charts = true;
    ImGui::Spacing();
    draw_building_profit(w, reg, report, bid, &ui.tile_card.net, &ui.tile_card.expenses);

    // ── The workforce row: the player's own building only ──
    if (mine)
    {
        ImGui::TextDisabled("Workforce");
        ImGui::SameLine();
        draw_workforce_slider(b);
        ui.tile_card.slider = true;
    }
}


} // namespace

// ── The WATER variant of the centre column (BL-785) ──────────────────────
//
// A water tile selects exactly as a land tile does, and its centre column
// answers a different question because the ground answers a different one.
// SELECTION.md § A water tile is selectable: owner and domain are the headline
// pair, then the province, then habitability. There is no accordion here —
// Buildings, Deposits, Resources and Population all read the ground, and on
// water there is none.
//
// UNOWNED IS THE ASSERTION, NOT THE ABSENCE OF ONE. Open ocean is unowned
// STRUCTURALLY (PROVINCES.md § Who owns water), so the row says so in words. An
// empty owner row and an owner row reading "Unowned" cost the same pixels and
// mean opposite things: the first looks like the panel failed, the second is
// the model's central claim — which is the whole reason this surface exists.
void draw_water_facts_column(const world& w, entity_id sel, const tile_component& tile,
                             float center_w, float total_h)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::BeginChild("##water_facts", {center_w, total_h}, true,
                      ImGuiWindowFlags_NoSavedSettings);

    const region_domain dom = region_domain_of(tile.substrate);
    const bool open_ocean   = (dom == region_domain::open_ocean);

    entity_id owner = null_entity;
    if (const auto nit = w.tile_to_nation.find(sel); nit != w.tile_to_nation.end())
        owner = nit->second;

    // ── Owner — the headline, and the reason the surface exists ──
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Owner");
    ImGui::SameLine();
    if (owner != null_entity)
    {
        if (const auto nat = w.nations.find(owner); nat != w.nations.end())
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::nation_colour(owner)),
                               "%s", nat->second.name.c_str());
        else
            ImGui::Text("Claimed");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Coastal water belongs to whoever owns the shore.\n"
                              "Title is derived from the shore that claims it,\n"
                              "never from anything built on the water.");
    }
    else
    {
        // Stated, never blank. The two branches are deliberately the same shape,
        // so "Unowned" reads as a VALUE in the row rather than as a gap in it.
        ImGui::TextDisabled("Unowned");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(open_ocean
                                  ? "Open ocean is unowned STRUCTURALLY - it is not a\n"
                                    "territory anyone can hold. It is crossed, not held."
                                  : "This water is claimed by no nation: no shore beside\n"
                                    "it is held either.");
    }

    // ── Domain — what KIND of water, which is what decides ownership ──
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Domain");
    ImGui::SameLine();
    ImGui::Text("%s", open_ocean ? "Open ocean"
                    : (tile.substrate == terrain_substrate::lake) ? "Lake (coastal water)"
                                                                  : "Coastal water");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Land, coastal water and open ocean are the three domains.\n"
                          "Coastal water and lakes can be owned; open ocean cannot.");

    ImGui::Separator();

    // ── Province ──
    const uint32_t prov = w.provinces.province_of(sel);
    ImGui::TextDisabled("Province");
    ImGui::SameLine();
    if (prov != 0u) ImGui::Text("#%u", prov);
    else            ImGui::TextDisabled("none");

    // ── Habitability — the one terrain scalar water still answers ──
    ImGui::TextDisabled("Habitability");
    ImGui::SameLine();
    ImGui::Text("%.2f", static_cast<double>(tile.habitability));

    ImGui::Dummy({1.0f, style.ItemSpacing.y});
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("No deposits, workforce or buildings are read here: those "
                        "questions are about ground, and there is none.");
    ImGui::PopTextWrapPos();

    ImGui::EndChild();
}

// ── The PROVINCE CARD (BL-1300, tile card split) ──────────────────────────
//
// SELECTION.md § The tile element's layout, item 2 (Ben, 2026-10-13): one
// titled card, province grain throughout, the selected tile serving only to
// name which province is meant. No pager — everything the province answers
// fits one card: capacity, one resource table, population. The table replaces
// the former separate Buildings and Deposits sections, which were two readings
// of the same rows.

/// The capacity bar's hover: WHERE the ceiling is granted — the four sustain
/// inputs `province_building_ceiling` computes from, each with its share of the
/// sustain units. Read through `measure_province_sustain`, so the tooltip cannot
/// disagree with the ceiling it explains.
///
/// The shares decompose units = (habitability_area + infrastructure_gain) x
/// population_factor additively: habitability-weighted AREA, the road GAIN,
/// and the population multiplier's lift over both. Area and habitability are one
/// product in the formula (a tile counts by how habitable it is), so they share
/// one figure rather than inventing a split the arithmetic does not make.
void draw_capacity_tooltip(const world& w, entity_id tile_id)
{
    const uint32_t  pid = w.provinces.province_of(tile_id);
    const province* pr  = (pid != 0) ? w.provinces.find(pid) : nullptr;
    if (pr == nullptr)
        return;
    const province_sustain s = measure_province_sustain(w, *pr);

    ImGui::BeginTooltip();
    ImGui::Text("Building ceiling %d - where it is granted", s.ceiling);
    ImGui::Separator();
    const float base  = s.habitability_area + s.infrastructure_gain;
    const float lift  = base * (s.population_factor - 1.0f);
    const float total = std::max(1e-6f, base + lift);
    const float mean_hab = s.land_tiles > 0 ? s.habitability_area / static_cast<float>(s.land_tiles) : 0.0f;
    if (ImGui::BeginTable("##sustain", 3, ImGuiTableFlags_SizingFixedFit))
    {
        const auto row = [](const char* name, const char* value, const char* share) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", name);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(value);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextDisabled("%s", share);
        };
        char v[64], sh[48];
        std::snprintf(v, sizeof v, "%d land tiles", s.land_tiles);
        row("Area", v, "");
        std::snprintf(v, sizeof v, "mean %.2f", static_cast<double>(mean_hab));
        std::snprintf(sh, sizeof sh, "%.0f%% (with area)",
                      static_cast<double>(100.0f * s.habitability_area / total));
        row("Habitability", v, sh);
        std::snprintf(v, sizeof v, "+%.1f from roads", static_cast<double>(s.infrastructure_gain));
        std::snprintf(sh, sizeof sh, "%.0f%%", static_cast<double>(100.0f * s.infrastructure_gain / total));
        row("Infrastructure", v, sh);
        std::snprintf(v, sizeof v, "x%.2f", static_cast<double>(s.population_factor));
        std::snprintf(sh, sizeof sh, "%.0f%%", static_cast<double>(100.0f * lift / total));
        row("Population", v, sh);
        ImGui::EndTable();
    }
    ImGui::TextDisabled("Buildings of every type count against it,\n"
                        "and it moves as the province develops.");
    ImGui::EndTooltip();
}

// The population figure's hover: each population centre in the province by
// name, scale word, headcount and habitability, largest first; the centre on the
// SELECTED tile is marked. Moved off the card into the hover (round 2) so the
// resource table can have the card's height.
void draw_population_tooltip(const province_pop_table& pt)
{
    ImGui::BeginTooltip();
    ImGui::Text("Province population  %d thousand", pt.total);
    ImGui::TextDisabled("Scale drives the agglomeration bonus and the labour pool.");
    ImGui::Separator();
    if (pt.rows.empty())
        ImGui::TextDisabled("Uninhabited.");
    for (const province_pop_row& r : pt.rows)
    {
        if (r.here)
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", "\xe2\x80\xa2");
        else
            ImGui::TextUnformatted(" ");
        ImGui::SameLine();
        ImGui::Text("%s", (r.name[0] != '\0') ? r.name : "Unnamed");
        ImGui::SameLine();
        ImGui::TextDisabled("%s \xc2\xb7 %d k \xc2\xb7 hab %.2f",
                            population_scale_name(r.scale), r.population,
                            static_cast<double>(r.habitability));
    }
    ImGui::EndTooltip();
}

// The province card (round 2, Ben 2026-10-13): ONE header row — title, the
// capacity bar narrowed to share it, the population figure right-aligned — then
// the resource table taking the rest of the height and scrolling INSIDE itself.
// The card's own window never scrolls (NoScrollbar | NoScrollWithMouse), and its
// scroll range is published so a check can hold it to zero.
void draw_province_card(const world& w, ui_state& ui, entity_id tile_id, float card_w, float total_h)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::BeginChild("##province_card", {card_w, total_h}, true,
                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                      ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Province");

    const province_build_table pb = province_builds(w, tile_id);
    if (!pb.known)
    {
        // Ocean and otherwise unpartitioned ground: say there is no province,
        // then fall back to the tile's OWN deposits so the card still says
        // something true (province_deposits does the fallback).
        ImGui::SameLine();
        ImGui::TextDisabled("none");
        ImGui::Separator();
        ImGui::TextDisabled("No province. This tile's own deposits:");
        const std::vector<province_deposit_row> rows = province_deposits(w, tile_id);
        if (rows.empty())
            ImGui::TextDisabled("none");
        for (const province_deposit_row& r : rows)
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(presentation_of(r.resource).colour),
                               "%s  %.1f", resource_name(r.resource), static_cast<double>(r.amount));
        ui.tile_card.province_scroll_max = ImGui::GetScrollMaxY();
        ImGui::EndChild();
        return;
    }

    // ── The header row: title | capacity bar | population ──
    const province_pop_table pt = province_population(w, tile_id);
    ui.tile_card.population = pt.total;
    char pop[32];
    std::snprintf(pop, sizeof pop, "%d k", pt.total);
    const float pop_w   = ImGui::CalcTextSize(pop).x;
    const float row_x   = ImGui::GetCursorPosX();     // after the title, on its line
    ImGui::SameLine();
    const float bar_x   = ImGui::GetCursorPosX();
    const float right_x = ImGui::GetWindowContentRegionMax().x;
    const float bar_w   = std::max(24.0f, right_x - bar_x - pop_w - style.ItemSpacing.x);
    (void)row_x;

    // Capacity: standing buildings against the province building ceiling. -1 is
    // UNKNOWN by the BL-513 contract and is said, never drawn as room.
    ui.tile_card.province_standing = pb.standing;
    ui.tile_card.province_ceiling  = pb.ceiling;
    if (pb.ceiling >= 0)
    {
        const bool  full = pb.standing >= pb.ceiling;
        const float frac = pb.ceiling > 0
                               ? std::clamp(static_cast<float>(pb.standing) / static_cast<float>(pb.ceiling), 0.0f, 1.0f)
                               : 1.0f;
        char ov[48];
        std::snprintf(ov, sizeof ov, full ? "%d/%d full" : "%d/%d", pb.standing, pb.ceiling);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
                              ImGui::ColorConvertU32ToFloat4(full ? palette::negative : palette::positive));
        ImGui::ProgressBar(frac, {bar_w, ImGui::GetTextLineHeight()}, ov);
        ImGui::PopStyleColor();
        const ImVec2 mn = ImGui::GetItemRectMin();
        const ImVec2 mx = ImGui::GetItemRectMax();
        ui.tile_card.capacity_x = (mn.x + mx.x) * 0.5f;
        ui.tile_card.capacity_y = (mn.y + mx.y) * 0.5f;
        if (ImGui::IsItemHovered())
            draw_capacity_tooltip(w, tile_id);
    }
    else
    {
        fit_text(text_box::table_cell, "selection.province.capacity", "capacity unknown", bar_w, true);
    }
    ImGui::SameLine(right_x - pop_w);
    ImGui::TextUnformatted(pop);
    {
        const ImVec2 mn = ImGui::GetItemRectMin();
        const ImVec2 mx = ImGui::GetItemRectMax();
        ui.tile_card.population_x = (mn.x + mx.x) * 0.5f;
        ui.tile_card.population_y = (mn.y + mx.y) * 0.5f;
    }
    if (ImGui::IsItemHovered())
        draw_population_tooltip(pt);
    ImGui::Separator();

    // ── The resource table: Built / Max / Resource, richest deposit first, ──
    // taking the rest of the card and scrolling inside its own rows.
    if (pb.rows.empty())
    {
        ImGui::TextDisabled("No workable deposits in this province.");
    }
    else if (ImGui::BeginTable("##prov_resources", 4,
                               ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit |
                               ImGuiTableFlags_ScrollY,
                               {0.0f, std::max(ImGui::GetFrameHeight(),
                                               ImGui::GetContentRegionAvail().y)}))
    {
        const float cpad = style.CellPadding.x * 2.0f;
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("##good",   ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Built",    ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Built").x + cpad);
        ImGui::TableSetupColumn("Max",      ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Max").x + cpad);
        ImGui::TableSetupColumn("Resource", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Resource").x + cpad);
        ImGui::TableHeadersRow();
        for (const province_build_row& r : pb.rows)
        {
            const resource_presentation& rp = presentation_of(r.resource);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(rp.colour));
            fit_text(text_box::table_cell, "selection.province.resource", rp.name,
                     ImGui::GetContentRegionAvail().x);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", r.built);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Extraction sites in this province working %s.", rp.name);
            ImGui::TableSetColumnIndex(2);
            if (r.built >= r.max) ImGui::TextDisabled("%d", r.max);
            else                  ImGui::Text("%d", r.max);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Placement slots for %s across this province's tiles.", rp.name);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.1f", static_cast<double>(r.deposit));
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s deposited, summed across this province.", rp.name);
            ui.tile_card.province_rows.push_back(rp.name);
        }
        ImGui::EndTable();
    }

    ui.tile_card.province_scroll_max = ImGui::GetScrollMaxY();
    ImGui::EndChild();
}

// The tile element (BL-1300; SELECTION.md § The tile element's layout): four
// columns, left to right — Preview, Province, Tile, Actions — and NO header row:
// the shared `[icon] Tile [x, y] [>]` row spent a line of the band on a name the
// canvas already shows and a 'go to' that is a no-op for a tile.
//
// The same element draws a selected BUILDING: @p expanded names it, and the
// band draws its host tile @p tile_id with that stack expanded in the tile
// column and the action grid retargeted to it (§ Multi-building tiles).
// The preview and the province card never move between the two states.
void draw_tile_selection(world& w, const recipe_registry& reg, const economy_report& report,
                         ui_state& ui, entity_id tile_id, entity_id expanded)
{
    const auto tit = w.tiles.find(tile_id);
    if (tit == w.tiles.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    if (expanded != null_entity)
    {
        const auto bit = w.buildings.find(expanded);
        if (bit == w.buildings.end() || bit->second.tile != tile_id)
            expanded = null_entity;
    }
    const tile_component& tile  = tit->second;
    // BL-785: water selects through this same element; the province column
    // forks on it (owner / domain), the others are shared.
    const bool            water = is_water(tile.substrate);
    const ImGuiStyle&     style = ImGui::GetStyle();
    ImDrawList*           dl    = ImGui::GetWindowDrawList();
    const float           avail = ImGui::GetContentRegionAvail().x;
    const bool            god   = ui.spectating && ui.god_view;

    // Preview and actions keep their quarter each; the two cards split the
    // former centre half equally (Ben, 2026-10-13).
    const float total_h  = ImGui::GetContentRegionAvail().y;
    const float spacing  = style.ItemSpacing.x;
    const float left_w   = avail * 0.25f;
    const float right_w  = avail * 0.25f;
    const float centre_w = std::max(160.0f, avail - left_w - right_w - 3.0f * spacing);
    const float prov_w   = std::floor(centre_w * 0.5f);
    const float tile_w   = centre_w - prov_w;

    ui.tile_card.drawn    = true;
    ui.tile_card.tile     = tile_id;
    ui.tile_card.expanded = expanded;
    ui.tile_card.preview_w  = left_w;
    ui.tile_card.province_w = prov_w;
    ui.tile_card.tile_w     = tile_w;
    ui.tile_card.actions_w  = right_w;
    ui.tile_card.height     = total_h;

    // ── 1. Preview: zoomed hex neighbourhood ──
    {
        const ImVec2 p  = ImGui::GetCursorScreenPos();
        const ImVec2 mx = {p.x + left_w, p.y + total_h};
        dl->AddRectFilled(p, mx, IM_COL32(16, 18, 24, 255), 3.0f);
        request_neighbourhood_page(w, ui, tile_id, 2);
        draw_tile_neighbourhood(dl, w, tile_id, p, {left_w, total_h}, /*radius=*/2, &ui.ground);
        dl->AddRect(p, mx, IM_COL32(90, 90, 100, 255), 3.0f);
        ImGui::Dummy({left_w, total_h});
    }
    ImGui::SameLine();

    // ── 2. Province ──
    if (water)
        draw_water_facts_column(w, tile_id, tile, prov_w, total_h);
    else
        draw_province_card(w, ui, tile_id, prov_w, total_h);
    ImGui::SameLine();

    // ── 3. Tile: its buildings, or one of them expanded ──
    {
        ImGui::BeginChild("##tile_card", {tile_w, total_h}, true, ImGuiWindowFlags_NoSavedSettings);
        const std::vector<production_stack> stacks = tile_stacks(w, tile_id, god);
        if (expanded != null_entity)
        {
            draw_expanded_building_card(w, reg, report, ui, tile_id, expanded, stacks);
        }
        else
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                               "Tile [%d, %d]", tile.grid_x, tile.grid_y);
            // Road tier (BL-184): the canvas draws the three tiers by weight
            // alone, with no key, so the card names the tier at the moment of
            // interest. Moved here from the retired header row.
            if (const char* tier = road_tier_name(tile.road_level))
            {
                ImGui::SameLine();
                ImGui::TextDisabled("\xc2\xb7 %s", tier);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Road tier: Track (thin) - Road - Highway (thick, bright).\n"
                                      "A better road moves goods across this tile faster.");
            }
            ImGui::Separator();
            draw_tile_building_grid(w, reg, report, ui, stacks);
        }
        ImGui::EndChild();
    }
    ImGui::SameLine();

    // ── 4. Actions: the building's while one is expanded, the tile's otherwise ──
    if (expanded != null_entity)
    {
        draw_building_actions(w, reg, ui, expanded, right_w, total_h);
        return;
    }
    {
        ImGui::BeginChild("##tile_actions", {right_w, total_h}, false,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
        const float  bw  = (right_w - spacing) / 2.0f;
        const float  bh  = (total_h - 2.0f * spacing) / 3.0f;
        const ImVec2 bsz = {bw, bh};
        const entity_id sel = tile_id;

        // Construct — opens the tile construction ledger (BL-162).
        //
        // BL-174 strand 2: two world-derived layers, no tutorial state.
        //  - ENABLED only if something is actually placeable here.
        //  - PRIMED (accent ring) when the player has nothing under
        //    construction anywhere.
        // ON WATER THE LIST IS ONE TYPE LONG (BL-785): a port, and only on water
        // this nation owns.
        bool any_placeable = false;
        if (water)
        {
            const bool owned = w.tile_to_nation.count(sel) != 0;
            any_placeable = owned
                            && placement_rules::can_place_in_world(
                                   w, sel, building_type::port, resource_type::iron_ore,
                                   ui.max_logistics_reach).ok();
        }
        else
        for (const resource_type er : placement_rules::k_extractable)
            if (tile.resource_deposit[static_cast<std::size_t>(er)] > 0.0f &&
                placement_rules::can_place_in_world(w, sel, building_type::extraction_site, er,
                                                   ui.max_logistics_reach).ok())
            { any_placeable = true; break; }
        // BL-1198: a Well needs no deposit, so the loop above cannot see it.
        if (!any_placeable && !water
            && placement_rules::is_well_site(w, sel, resource_type::water)
            && placement_rules::can_place_in_world(w, sel, building_type::extraction_site,
                                                   resource_type::water, ui.max_logistics_reach).ok())
            any_placeable = true;
        // BL-1199: nor a Fishing Wharf (coastal, no produce deposit).
        if (!any_placeable && !water
            && placement_rules::is_wharf_site(w, sel, resource_type::agricultural_produce)
            && placement_rules::can_place_in_world(w, sel, building_type::extraction_site,
                                                   resource_type::agricultural_produce,
                                                   ui.max_logistics_reach).ok())
            any_placeable = true;
        if (!any_placeable && !water)
        {
            for (const building_type bt : {building_type::processing_facility,
                                           building_type::port,
                                           building_type::launchpad,
                                           building_type::inland_logistics_hub,
                                           building_type::planetary_marketplace,
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
        {
            const ImVec2 cp = ImGui::GetCursorScreenPos();
            ui.construction_ui.construct_x = any_placeable ? cp.x + bsz.x * 0.5f : -1.0f;
            ui.construction_ui.construct_y = any_placeable ? cp.y + bsz.y * 0.5f : -1.0f;
        }
        if (tile_icon_button("##act_construct", bsz, any_placeable,
                             any_placeable
                                 ? (water
                                        ? "Construct a port - the one thing that stands on water"
                                        : (primed ? "Construct buildings - nothing under way"
                                                  : "Construct buildings"))
                             : water
                                 ? (w.tile_to_nation.count(sel) != 0
                                        ? "Only a port stands on water, and not on this water"
                                        : "Unowned water: a port needs a shore its owner holds")
                                 : "Nothing can be built on this tile",
                             glyph_hammer))
        {
            // A DOOR onto the one construction element, not a toggle — its
            // active state is not visible on the card.
            close_all_panels(ui);
            ui.show_construction_panel   = true;
            ui.construction.panel_view   = 0;
        }
        ImGui::SameLine();

        // No Manage Buildings (BL-1300): the tile card's building grid IS the
        // tile's buildings, one press from each.
        tile_icon_button("##act_history", bsz, /*enabled=*/false,
                         "History (not yet available)", glyph_clock);

        tile_icon_button("##act_supply", bsz, /*enabled=*/false,
                         "Supply (not yet available)", ui::icons::supply);
        ImGui::SameLine();
        tile_icon_button("##act_reserved1", bsz, /*enabled=*/false, "Reserved", glyph_reserved);

        tile_icon_button("##act_reserved2", bsz, /*enabled=*/false, "Reserved", glyph_reserved);
        ImGui::SameLine();
        tile_icon_button("##act_reserved3", bsz, /*enabled=*/false, "Reserved", glyph_reserved);

        ImGui::EndChild();
    }
}





} // namespace

/// The DEPOSIT card (BL-671). The Resource lens's structure is every tile on the
/// body carrying the selected good, so that is what this counts and that is what
/// it is about: not one tile's richness, but the whole showing.
///
/// Deliberately thin, and every figure derived. A deposit has no ledger of its
/// own and no actions — you cannot dig a region, you site a building on one tile
/// of it — so the card's job is to say how much is here and hand the player to
/// the Market ledger, which the press has already opened on Prices.
void draw_deposit_selection(const world& w, ui_state& ui)
{
    const int ri = ui.selected_deposit_resource;
    if (ri < 0 || static_cast<std::size_t>(ri) >= resource_count)
        return;
    const auto rt = static_cast<resource_type>(ri);
    const resource_presentation& rp = presentation_of(rt);

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(rp.colour), "%s", rp.name);
    ImGui::SameLine();
    ImGui::TextDisabled("Deposit");
    ImGui::Separator();

    // Counted over the ACTIVE BODY only, because that is the region the lens drew
    // and the selection grain follows the drawing. A body-spanning total would be
    // a different claim from the one the player just clicked on.
    int   tiles = 0;
    float total = 0.0f;
    float best  = 0.0f;
    for (const auto& [id, tile] : w.tiles)
    {
        if (tile.body != ui.active_body)
            continue;
        const float d = tile.resource_deposit[static_cast<std::size_t>(ri)];
        if (d <= 0.0f)
            continue;
        ++tiles;
        total += d;
        best = std::max(best, d);
    }

    if (tiles == 0)
    {
        ImGui::TextDisabled("No %s on this body.", rp.name);
        return;
    }

    ImGui::Text("%d tiles", tiles);
    ImGui::SameLine();
    ImGui::TextDisabled("carrying it on this body");
    ImGui::Text("Richest tile  %.2f", best);
    ImGui::Text("Total         %.2f", total);
    ImGui::Spacing();
    ImGui::TextDisabled("Prices for this good are open in the Market ledger.");
}

/// The PLATE card (BL-671, closing NR-697). Before this a plate press set its
/// channel and the band showed the PREVIOUS selection — a card asserting
/// something the player had not clicked, which is the exact failure NR-697
/// recorded.
///
/// Every figure comes from `ui.selected_plate_facts`, cached by the canvas at
/// press time; see that field for why it is a cache and not a lookup.
void draw_plate_selection(ui_state& ui)
{
    const ui_state::plate_summary& f = ui.selected_plate_facts;

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                       "Plate %d", ui.selected_plate);
    ImGui::SameLine();
    ImGui::TextDisabled(f.oceanic ? "Oceanic" : "Continental");
    ImGui::Separator();

    if (f.tiles == 0)
    {
        // A stagnant-lid body has one plate owning everything and no boundaries,
        // which is a legitimate answer rather than a degenerate one — but a plate
        // with no tiles means the report and the press disagree, so say that
        // instead of printing zeros as though they were measurements.
        ImGui::TextDisabled("No plate data for this body.");
        return;
    }

    ImGui::Text("%d tiles", f.tiles);
    ImGui::SameLine();
    ImGui::TextDisabled("on this body");

    // The boundary is the point of the lens — a plate interior is just a region,
    // but a boundary is where the mountains and the porphyry copper came from.
    if (f.convergent_tiles > 0 || f.divergent_tiles > 0)
    {
        // Two counts, never summed. A tile at a junction between a closing pair
        // and an opening pair sits on BOTH masks, so a single "boundary" number
        // would double-count it — and the two say different things anyway: a
        // collision is where the mountains and the porphyry copper came from, a
        // rift is where the crust thinned and the basins opened.
        if (f.convergent_tiles > 0)
            ImGui::Text("%d on a collision boundary", f.convergent_tiles);
        if (f.divergent_tiles > 0)
            ImGui::Text("%d on a rift", f.divergent_tiles);
    }
    else
        ImGui::TextDisabled("Interior plate - no classified boundary.");

    ImGui::Text("Drift  %+.2f, %+.2f", f.drift_col, f.drift_row);
    ImGui::SameLine();
    ImGui::TextDisabled("cols, rows per epoch");

    ImGui::Spacing();
    ImGui::TextDisabled("Its tectonic record is open in the History ledger.");
}

void draw_selection_content(world& w, const recipe_registry& reg,
                            const economy_report& report, ui_state& ui)
{
    // The building card's open-in-ledger button describes THIS frame. Cleared here,
    // at the dispatcher, so it reads absent for every selection that is not a
    // player-owned building — including the frames where no building card draws at
    // all. A probe left standing after its control is gone is a click into nothing.
    ui.construction_ui.open_ledger_x = -1.0f;
    ui.construction_ui.open_ledger_y = -1.0f;
    ui.construction_ui.construct_x   = -1.0f;
    ui.construction_ui.construct_y   = -1.0f;

    // Same rule for two more per-frame writes. The neighbourhood-page request
    // (BL-1241) is re-raised by the two views that draw the zoomed ground (the
    // tile card, the battle card) every frame they draw; dropped here, so a
    // selection that draws neither stops the ground layer baking a stale
    // region. The tile card's verify readout (BL-1300) is refilled only by a
    // frame that draws the tile element.
    ui.ground_neigh_req.valid = false;
    ui.tile_card = {};

    // BL-598 removed the PROVINCE branch that stood here. BL-511 gave a province
    // its own resolution ahead of `selection_kind_of` — it is not an entity, so
    // the kind resolution cannot see it — and BL-534 gave it its own body. Ben
    // dissolved that on 2026-08-24: the province is now a set of SECTIONS in the
    // tile element's accordion, resolved from the selected tile at the draw site,
    // so this dispatcher has one fewer thing to be. `ui_state::selected_province`
    // survives as a derived mirror the canvas outline reads; nothing here reads
    // it, which is the point — a card can no longer be routed by a stale id.
    //
    // BL-469: a BATTLE resolves before everything else, for the same structural
    // reason a province resolves before selection_kind_of — it is not an entity,
    // so the kind resolution cannot see it — and for one reason of its own: a
    // fight on the ground you are looking at is the most urgent thing that ground
    // has to say. It owns its whole card, so it returns rather than falling
    // through to the shared header.
    // A battle that has CONCLUDED is erased from `world::battles`, so a selection
    // can go stale between frames. `selected_battle` clears it and returns null,
    // and the element falls through to the ordinary resolution — the fallback is
    // a normal selection, never a blank card. Same shape as BL-511's "a province
    // id that no longer resolves clears itself here".
    if (ui.has_battle_selection() && selected_battle(w, ui) != nullptr)
    {
        draw_battle_selection(w, ui);
        return;
    }

    // BL-671: the two NON-ENTITY LENS REGIONS resolve next, for the same
    // structural reason the battle and the contract do — a deposit is a resource
    // index and a plate is a raster id, so neither travels in `selected_entity`
    // and `selection_kind_of` cannot see either. Each owns its whole card and
    // returns rather than falling through to the shared header.
    //
    // Both channels were WRITE-ONLY until this landed: the canvas set them and no
    // surface read them, so a press on a deposit or a plate left the band showing
    // whatever was selected before it (NR-697).
    if (ui.has_deposit_selection())
    {
        draw_deposit_selection(w, ui);
        return;
    }
    if (ui.has_plate_selection())
    {
        draw_plate_selection(ui);
        return;
    }

    const selection_kind kind = selection_kind_of(w, ui.selected_entity);

    // Nothing valid selected - draw nothing. (The band frame never lets this
    // happen: with no valid selection it substitutes the player corporation
    // before calling here, BL-266. This guard covers other callers only.)
    if (kind == selection_kind::none)
        return;

    // A selected TILE and a selected BUILDING are one element (BL-1300,
    // SELECTION.md § Multi-building tiles): a building draws its host tile's card
    // with its stack expanded. Both are dispatched before the shared header,
    // because the tile element draws none — its 'go to' is a no-op and the
    // canvas already shows the name (Ben, 2026-10-13). Every other kind keeps it.
    if (kind == selection_kind::tile)
    {
        draw_tile_selection(w, reg, report, ui, ui.selected_entity, null_entity);
        return;
    }
    if (kind == selection_kind::building)
    {
        const auto bit = w.buildings.find(ui.selected_entity);
        draw_tile_selection(w, reg, report, ui, bit->second.tile, ui.selected_entity);
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

    // The entity the header's icon and 'go to' act on. BL-598 removed the
    // province's anchor-tile borrow here: a province is no longer a thing this
    // element can BE, so the header always acts on the selected entity.
    const entity_id head_id = ui.selected_entity;

    // ── Header: [icon] Name · type ............................. [>] [x] ──
    {
        const ImVec2 hc = ImGui::GetCursorScreenPos();
        const float  ir = frame_h * 0.40f;
        draw_selection_icon(w, dl, kind, head_id,
                            {hc.x + ir, hc.y + frame_h * 0.5f}, ir);
        ImGui::SetCursorScreenPos({hc.x + ir * 2.0f + style.ItemSpacing.x, hc.y});

        const char* title = selection_title(w, kind, ui.selected_entity);
        const char* sub   = selection_kind_name(kind); // the muted trailing KIND
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", title);
        // Suppress the redundant type label when the title is already the kind name
        // (e.g. a tile titles as "Tile"), avoiding "Tile · Tile".
        if (std::strcmp(title, sub) != 0)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", sub);
        }

        // Right-align the 'go to' button at the content region's right edge.
        // bar_w is the available content width (padding already excluded), so the
        // offset is measured from the content-left, no WindowPadding term needed.
        // No close button: the band is always open (BL-266).
        const float btn = frame_h;
        ImGui::SameLine(bar_w - btn);
        if (ImGui::Button(">", {btn, btn}))
            focus_on_entity(w, ui, head_id);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Go to");
    }

    ImGui::Separator();

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

// The tile construction ledger (BL-162): the tile-contextual surface that actually
// lets the player build. Lists every building type placeable on the selected tile —
// each in a bordered container (name + full credit cost + payback + an expected-profit
// magnitude bar + a reason-coded validity read + a Build action) — and enqueues the
// chosen build on the tile via the construction.pending_tile seam app executes.
// Candidates sort by expected net descending on a ceiling shared across the list, so
// the best options are the ones on screen without scrolling.
void draw_construction_ledger_body(const world& w, const recipe_registry& reg, ui_state& ui)
{
    const ImGuiStyle& style = ImGui::GetStyle();

    // Measured, not assumed: the build bar is now a SECTION of a host ledger rather
    // than a window of its own, so its width budget is whatever the host gives it.
    // (NR-709: a surface that hard-codes a width loses its name column the first
    // time the column narrows.)
    const float bar_w = ImGui::GetContentRegionAvail().x;

    const entity_id tile_id = ui.selected_entity;
    const auto tit = w.tiles.find(tile_id);
    if (tit == w.tiles.end())
    {
        // THE EMPTY STATE IS THE ANSWER, not a placeholder (Ben, 2026-08-29).
        // Building is a TARGETED act, so with no target there is nothing to price.
        // The tempting richer state — a list of tiles the player could build on —
        // cannot be written without ordering it, and any ordering of that list is a
        // recommendation, which is the surface Ben declined ("it enters the territory
        // of telling the player what to do", CONCEPT.md). So this stays one line.
        // Contrast the Buildings view, whose empty state CAN be improved: listing
        // buildings the player already owns is navigation, not recommendation.
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Construct");
        ImGui::Spacing();
        ImGui::TextDisabled("Select a tile");
        return;
    }
    const tile_component& tile = tit->second;

    // ── Header: Construct · [x, y] ──
    // No close 'x': the host ledger's nav rail slot and its tab strip own closing
    // now, and a second closer on a section inside it would close something the
    // player did not point at.
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Construct");
    ImGui::SameLine();
    ImGui::TextDisabled("[%d, %d]", tile.grid_x, tile.grid_y);

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
    if (placement_rules::is_wharf_site(w, tile_id, resource_type::agricultural_produce))
        cands.push_back({building_type::extraction_site, resource_type::agricultural_produce,
                         "Fishing Wharf"});
    // Well (BL-1198, NR-971): the same shape for water — no ice deposit here, but
    // a river runs along the tile or a lake lies beside it.
    if (placement_rules::is_well_site(w, tile_id, resource_type::water))
        cands.push_back({building_type::extraction_site, resource_type::water, "Well"});
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
    cands.push_back({building_type::planetary_marketplace, resource_type::iron_ore, "Planetary Marketplace"}); // BL-1269
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
    // BL-428 used to add a second half to the same argument — an ancient recipe
    // deeper than the corp had reached was erased here too. BL-692 removed that
    // clause with the gate behind it: construct_building no longer refuses on
    // depth, so filtering here would hide rows the door would now accept.
    //
    // BL-593 adds the remaining one: a recipe locked by `recipe_unlocked`
    // (BL-588's tech-recipe gate) is dropped the same way, regardless of band.
    // Ben's ruling (2026-08-24, the same shape as the era precedent above, not
    // a new one): filtered out, not shown-and-locked — "the door not showing
    // what the gate would refuse" is the standing argument, unchanged by this
    // item. Since BL-1273 no node locks a recipe (E0-EC-03's lock on
    // `refined_copper` retired with the per-corp gates, NR-1022), so this
    // branch removes nothing until the effect pass gives a node an unlock.
    cands.erase(std::remove_if(cands.begin(), cands.end(),
                               [&w, &reg, tile_id](const candidate& c)
                               {
                                   if (!reg.building_available(c.type))
                                       return true;
                                   // The body's air (Ben, 2026-10-09): a propellant
                                   // route this tile's body cannot run is dropped the
                                   // same way — construct_building refuses it
                                   // (construction_result::wrong_air).
                                   if (c.type == building_type::processing_facility)
                                       if (const recipe* rc = reg.get_recipe(c.recipe);
                                           rc != nullptr && !recipe_runs_at_tile(w, *rc, tile_id))
                                           return true;
                                   return !recipe_unlocked(w, reg, w.player_entity, c.recipe);
                               }),
                cands.end());

    // Publish the processing rows this door LISTS (the recipe ids behind the
    // folded rows), for the air_gate verify check.
    ui.construction_ui.door_tile = tile_id;
    ui.construction_ui.door_recipes.clear();
    for (const candidate& c : cands)
        if (c.type == building_type::processing_facility)
            ui.construction_ui.door_recipes.push_back(c.recipe);

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
        // BL-344: the player's own corp is named, so a tech-gated type reads as
        // "Locked - not researched" in the ledger rather than offering a Build
        // button that construct_building would then refuse.
        c.pr = placement_rules::can_place_in_world(w, tile_id, c.type, c.target,
                                                  ui.max_logistics_reach,
                                                  w.player_entity,
                                                  reg.placement_gate_for(c.type, c.recipe));
        c.material_rate = construction_rate(w, reg, c.type, c.target, c.recipe, tile_id); // BL-328 pre-commit warning

        // Capex is the figure construction.cpp actually gates on: build cost PLUS the
        // materials priced at the local market. This ledger used to gate on build_cost
        // alone, so between the two figures it offered an enabled Build button that
        // then failed with nothing but a post-hoc toast.
        // BL-1066: the SAME function the gate prices with — flat cost plus materials,
        // times this tile's site multiplier — so the two cannot disagree again.
        c.capex = construction_capex(w, reg, tile_id, c.type, c.target, c.recipe);

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
    // contiguous — walk the list opening a TreeNodeEx (this file's existing fold idiom)
    // whenever the category changes, and close it once its rows
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
}

} // namespace ui
