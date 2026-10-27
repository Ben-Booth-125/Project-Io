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
#include "body_surface_canvas.hpp"    // logistic_points_or_fallback (BL-1354)
#include "world/market_clearing.hpp"
#include "world/construction.hpp"    // demolish path (the building element's Demolish)
#include "world/logistics.hpp"       // invalidate_logistics_caches (idle/resume flips the anchor set)
#include "world/placement_rules.hpp" // buildable-type validity + stack capacity
#include "world/province.hpp"        // BL-534: province membership + BL-513 building ceiling
#include "world/settlement.hpp"      // BL-785: region_domain_of - the three-domain word for a water tile
#include "world/recipe_registry.hpp" // recipe/economics lookups for the building element
#include "world/survey_system.hpp"
#include "world/trade.hpp"           // building_trade_points — a trade building's method line (BL-1321)
#include "player_side.hpp"          // BL-1406: is_player_side
#include "world/nation_tech.hpp"     // BL-593: recipe_unlocked — the door filters what the gate would refuse
#include "world/unit_roster.hpp" // campaign hire gate + roster table (BL-324); also the Soldier card's Roster page name lookup

#include "rail_tool.hpp" // BL-1255: the Track / Railway Station rows
#include "road_tool.hpp" // BL-1370: the crossings a road row may arm
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
// The mirror of `run_construction`'s rate — `ui::read_construction` and its status
// helpers — lives in construction_panel.hpp and is called from here.
//
// It USED to live here as well, a second private copy beside the Construction
// ledger's, and BL-1342 is what two copies cost: both had drifted off the sim
// (reading the per-tick FLOW where the sim draws the persistent STOCK, with no
// capacity row and no fair-price ceiling), so the card promised "ETA 2 qtr" over
// a build that had not moved in quarters. `construction.md` § "Construction is
// durative and material-gated" names mirroring `run_construction`'s formula as
// the contract, and a contract with two implementations has none.

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
        case selection_kind::corporation: return "Corporation"; // BL-1407: never selected
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
    // BL-1407: the owner is the building's NATION (public).
    if (const auto nit = w.nations.find(owner_nation_of(w, id)); nit != w.nations.end())
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                           "Owner: %s%s", nit->second.name.c_str(),
                           b.state_idle ? " \xc2\xb7 idle: treasury short" : "");

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
            // BL-1406: what the player's actor may commit — its nation's
            // uncommitted treasury once folded (the payer app.cpp names).
            const bool affordable = purse_room_of(w, player_actor(w)) >= cost;

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
    float         deposit = 0.0f; ///< Summed deposit across member tiles (a stock, BL-1312).
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

    // Richest DEPOSIT first (BL-1312: the table merged the former Deposits
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

// The expanded building card's PROFIT ROW (BL-1312 round 2; SELECTION.md § The
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
    const building_profit p = estimate_building_reading(w, reg, report, id);
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
            // BL-1406: the owner is a corporation or a NATION (building_owner_of);
            // the player's nation wears the player's emblem, as the profile does.
            entity_id owner = building_owner_of(w, id).id;
            if (owner != null_entity && owner == player_actor(w))
                owner = w.player_entity;
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
        // the tile element with its stack expanded (draw_tile_selection, BL-1312).

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
    // BL-1406: the player acts as its nation once folded (try_switch_recipe
    // takes a nation switching one of its own buildings).
    const recipe_switch_result r = try_switch_recipe(w, reg, player_actor(w), bit->second, recipe);
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
    // BL-1407: the actor is a NATION (the corporation is deleted).
    const auto cit = w.nations.find(id);
    if (cit == w.nations.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    const nation_component& c = cit->second;

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "God view");

    ImGui::Text("Treasury: %s", fmt::credits(c.treasury).c_str());
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
    for (const entity_id bid : c.state_assets)
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
        ImGui::Text("%s%s%s \xc2\xb7 wf %.0f%%%s",
                    building_type_name(b.type),
                    *what ? " \xc2\xb7 " : "", what,
                    static_cast<double>(b.workforce_assigned) * 100.0,
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
        // draw_tile_selection (BL-1312).
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

// (glyph_auto, the Auto toggle's circular arrow, retired with player workforce
// control — BL-1322.)

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

// --- The expanded building card (BL-1312, tile card split) -----------------
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
        const construction_read rd = read_construction(w, reg, b.type, b.target_resource,
                                                       b.recipe, b.tile, b.state_idle,
                                                       construction_days_left(b));
        line = construction_status(rd, construction_days_left(b)); // days (Ben, 2026-10-24)
        ImGui::TextColored(construction_status_colour(rd), "%s", line.c_str());
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

// The WORKFORCE ROW, read-only (BL-1322; SELECTION.md § The building element's
// layout; PRODUCTION.md § Workforce target and the auto-solver, Ben 2026-10-13).
// The game sets the player's workforce — one 25 % step a quarter toward the most
// profitable level — so the slider and the Auto control retired with player
// workforce control, and this bar is the only workforce surface: the target as a
// filled bar, the percentage to its RIGHT. Drawn by the expanded building card
// and the Construction ledger's Buildings view. Returns the percentage as drawn.
std::string draw_workforce_bar(const building_component& b)
{
    const int   target = std::clamp(b.workforce_target, 0, workforce_target_max);
    char        pct[16];
    std::snprintf(pct, sizeof pct, "%d%%", target / 2); // shown as target/2: 200 internal = 100% (BL-1322)
    const float pct_w = ImGui::CalcTextSize("100%").x;
    const float bar_w = std::max(40.0f, ImGui::GetContentRegionAvail().x - pct_w
                                            - ImGui::GetStyle().ItemSpacing.x);
    ImGui::ProgressBar(static_cast<float>(target) / static_cast<float>(workforce_target_max),
                       {bar_w, ImGui::GetTextLineHeight()}, "");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Workforce target, set by the game: each quarter it moves one\n"
                          "25%% step toward the most profitable level (100%% = full employment).");
    ImGui::SameLine();
    ImGui::TextUnformatted(pct);
    return pct;
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
/// One labelled condition bar (BL-1425): label and value on a line, the bar beneath,
/// in the colour the canvas counter uses for the same quantity.
void draw_unit_condition_bar(const char* label, float share, ImU32 col, const char* value)
{
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", label);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", value);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::ColorConvertU32ToFloat4(col));
    ImGui::ProgressBar(std::clamp(share, 0.0f, 1.0f), ImVec2(-1.0f, 6.0f), "");
    ImGui::PopStyleColor();
}

void draw_unit_strength_page(const world& w, const recipe_registry& reg, const unit_component& u)
{
    // BL-1425: THE FIGHT'S STATE FIRST (MILITARY.md § Land — the unit's stats) —
    // HP, readiness, morale, the three numbers the canvas counter and the land
    // round read. The player's own units only: a rival's internals stay private
    // (BL-068, the battle card's precedent; NR-1088 asks Ben).
    if (u.owner == player_actor(w) || u.owner == w.player_entity)
    {
        char v[48];
        const int est = unit_establishment(u);
        std::snprintf(v, sizeof v, "%d / %d men", u.count, est);
        draw_unit_condition_bar("HP", est > 0 ? static_cast<float>(u.count) / est : 0.0f,
                                palette::unit_hp, v);
        std::snprintf(v, sizeof v, "%.0f%%", u.readiness_permille / 10.0f);
        draw_unit_condition_bar("Readiness", u.readiness_permille / 1000.0f, palette::unit_readiness, v);
        std::snprintf(v, sizeof v, "%.0f%%", u.morale_permille / 10.0f);
        draw_unit_condition_bar("Morale", u.morale_permille / 1000.0f, palette::unit_morale, v);
        ImGui::Spacing();
    }

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

    // BL-1425: the land class the row fights as, and the numbers the round reads
    // (per man, quality-scaled; MILITARY.md § Land — the classes).
    {
        static constexpr const char* k_class_names[land_class_count] = {
            "Infantry", "Motorised infantry", "Mechanised infantry", "Armour",
            "Light armour", "Artillery", "Air defence", "Not a land fighter",
        };
        const std::vector<roster_row>& table = unit_roster_table();
        const land_class cls = u.type < table.size() ? land_class_for(table[u.type]) : land_class::infantry;
        const land_combat_stats st = unit_land_stats(u);
        ImGui::Spacing();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Fights as");
        ImGui::Text("%s", k_class_names[static_cast<int>(cls)]);
        ImGui::TextDisabled("soft %d  hard %d  hardness %d%%", st.soft, st.hard, st.hardness / 10);
    }
    ImGui::Spacing();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Owner");
    const char* owner_name = owner_display_name(w, u.owner); // BL-1406: a nation's unit
    ImGui::Text("%s", owner_name != nullptr ? owner_name : "\xe2\x80\x94");
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
        const bool owned = is_player_side(w, uit->second.owner); // BL-1406: the nation's unit

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

// The action grid RETARGETED to an expanded building (BL-1312; SELECTION.md
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
    const float         spacing  = style.ItemSpacing.x;

    ImGui::BeginChild("##building_actions", {right_w, total_h}, false,
                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
    const float  bw  = (right_w - spacing) / 2.0f;
    const float  bh  = (total_h - 2.0f * spacing) / 3.0f;
    const ImVec2 bsz = {bw, bh};

    // Deliberately NOT lifted by spectator god view (BL-408): god view
    // grants sight, never hands — a rival's Mothball/Dismantle stay
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

    // Mothball's column one row down was Auto (workforce autosolve). BL-1322
    // (Ben, 2026-10-13): the game sets the player's workforce with no opt-out,
    // so Auto retired with the slider and its cell is Reserved.
    tile_icon_button("##bld_reserved2", bsz, /*enabled=*/false, "Reserved", glyph_reserved);
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
    const char* side_name = owner_display_name(w, corp); // BL-1406: a nation's side
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", label);
    ImGui::SameLine();
    ImGui::TextUnformatted(side_name != nullptr ? side_name : "Unknown");

    // The aggregate first: it is the number that answers "am I winning".
    ImGui::ProgressBar(std::clamp(strength_permille / 1000.0f, 0.0f, 1.0f),
                       ImVec2(-1.0f, 0.0f));
    ImGui::TextDisabled("%.1f%% readiness", strength_permille / 10.0f); // BL-1421: side readiness

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

    // BL-1406: the player's side is its nation once folded (units re-owned).
    const entity_id player = is_player_side(w, b.attacker) ? b.attacker
                           : is_player_side(w, b.defender) ? b.defender : w.player_entity;
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

// ── The TILE CARD (BL-1312, tile card split) ──────────────────────────────
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
        key.owner = building_owner_of(w, id).id; // BL-1406: a nation, once folded
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
    /// BL-1365: the SAME figure as `text` where it is a net, abbreviated to the
    /// grid's 38 px column ("+1.2k"); "" while the stack is still building or its
    /// net cannot be priced. The column is the number, the card is the sentence.
    std::string net_short;
    ImU32       colour = palette::text_secondary;
    /// BL-1343: how far through its build the stack's first unfinished member is,
    /// in [0,1] — drawn as a thin bar along the button's bottom edge. Valid only
    /// with `building`.
    float       progress = 0.0f;
    bool        building = false;
    /// BL-1368: the full status line (`construction_status`, and so
    /// `construction_stall_reason`'s one authored clause) while `building`. The
    /// state mark's own explainer draws this, so the mark and the site card's
    /// status line cannot drift. `text` above is the SHORT reading the column had.
    std::string status;
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
        const building_component& b  = w.buildings.at(building);
        const construction_read   rd = read_construction(w, reg, b.type, b.target_resource,
                                                         b.recipe, b.tile, b.state_idle,
                                                         construction_days_left(b));
        out.colour   = ImGui::ColorConvertFloat4ToU32(construction_status_colour(rd));
        out.progress = construction_progress_fraction(w, reg, b);
        out.building = true;
        // BL-1342: a stall NAMES the good holding it, in the few words the stack
        // button's third line can carry ("Paused - no Steel"). "Paused" alone was
        // the reading Ben met on a build that had not moved in quarters.
        out.status = construction_status(rd, construction_days_left(b));
        const std::string blocker = construction_blocker_phrase(rd);
        if (rd.paused)
            out.text = blocker.empty() ? "Paused" : "Paused - " + blocker;
        else
        {
            // Days (Ben, 2026-10-24; PRODUCTION.md § Construction is measured in days).
            const int eta = construction_eta_days(rd, construction_days_left(b));
            out.text = "ETA " + days_label(eta);
            if (!blocker.empty())
                out.text += " - " + blocker;
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
        const building_profit p = estimate_building_reading(w, reg, report, m);
        if (!p.has_data)
            continue;
        data = true;
        net += p.net();
    }
    if (!data)
    {
        out.text      = "\xe2\x80\x94 / qtr";
        out.net_short = "\xe2\x80\x94";
        return out;
    }
    out.text      = fmt::rate(net, "qtr");
    out.net_short = fmt::signed_delta(static_cast<double>(net));
    out.colour    = ui::value_colour(static_cast<double>(net));
    return out;
}

/// The tile card's building list: ONE LINE A STACK, in the construction grid
/// (BL-1365; construction.md § "One line a row, and the words live in the hover",
/// § "The line is a grid, not a sentence"). It was three buttons across, each
/// three hand-drawn lines deep, in a ~260 px column — so the reading that said
/// "Paused - Steel priced out" had ~80 px to say it in and the card ran out of
/// height after six stacks.
///
/// The columns are the QUEUE's, deliberately: a stack of this tile and a row of
/// the in-flight queue are the same subject seen from two surfaces, so they read
/// in the same places. The press is unchanged — it selects the stack's oldest
/// member and the card redraws with that stack expanded.
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
    const float       row_h  = construction_row_height();
    ImDrawList*       dl     = ImGui::GetWindowDrawList();
    ui.tile_card.row_h       = row_h;
    ui.tile_card.line_h      = line_h;

    constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg
                                    | ImGuiTableFlags_NoPadInnerX
                                    | ImGuiTableFlags_SizingFixedFit;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {construction_grid::pad, style.CellPadding.y});
    // SIX columns (BL-1368 correction): the seventh was declared and never
    // filled, so the state mark sat in the net column and a 12 px column trailed
    // empty off the end of every row — a grid whose last column was a gap.
    if (!ImGui::BeginTable("##tile_stacks", 6, flags))
    {
        ImGui::PopStyleVar();
        return;
    }
    ImGui::TableSetupColumn("##glyph", ImGuiTableColumnFlags_WidthFixed, construction_grid::glyph);
    ImGui::TableSetupColumn("##name",  ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("##count", ImGuiTableColumnFlags_WidthFixed, 22.0f);
    ImGui::TableSetupColumn("##makes", ImGuiTableColumnFlags_WidthFixed, construction_grid::makes);
    ImGui::TableSetupColumn("##net",   ImGuiTableColumnFlags_WidthFixed, construction_grid::bar);
    ImGui::TableSetupColumn("##state", ImGuiTableColumnFlags_WidthFixed, construction_grid::state);

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

        ImGui::TableNextRow(ImGuiTableRowFlags_None, row_h);
        ImGui::PushID(static_cast<int>(si));

        // ── Column 0: the type glyph. A closed stack passes a good neither glyph
        // family keys on (coal), so the glyph says the TYPE and nothing about what
        // it makes.
        // BL-1368: an explainer per column, hit-tested after the row.
        std::vector<grid_explainer> cells;

        ImGui::TableSetColumnIndex(0);
        const ImVec2        gp    = ImGui::GetCursorScreenPos();
        const float         gr    = construction_grid::glyph * 0.5f;
        const resource_type ident = makes ? good : (st.open ? b0.target_resource : resource_type::coal);
        icons::building(dl, {gp.x + gr, gp.y + line_h * 0.5f}, gr, b0.type, ident,
                        mine ? IM_COL32(190, 200, 210, 255) : IM_COL32(150, 154, 166, 255));
        ImGui::Dummy({construction_grid::glyph, line_h});

        // ── Column 1: the name, over a row-wide press that expands the stack ──
        ImGui::TableSetColumnIndex(1);
        const float name_w = std::max(16.0f, ImGui::GetContentRegionAvail().x);
        const bool pressed = ImGui::Selectable("##stack", ui.selected_entity == first,
                                               ImGuiSelectableFlags_SpanAllColumns);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, palette::selection);
        // BL-1371: wraps into the row's second line rather than eliding.
        bool      name_elided = false;
        const int name_lines  = construction_grid_text("selection.tile.stack_name",
                                                       name.c_str(), name_w,
                                                       /*disabled=*/false, &name_elided);
        ImGui::PopStyleColor();
        {
            // Whose it is, and that the row is a press. A rival's name is the
            // public TYPE, which is the fog saying what it withholds.
            std::string t;
            if (!mine)
            {
                const char* on = owner_display_name(w, st.owner); // BL-1406
                t = std::string("Owned by ")
                  + (on != nullptr ? std::string(on) : std::string("another nation"))
                  + ". ";
            }
            cells.push_back(construction_cell(
                "name", gp, construction_grid::glyph + construction_grid::pad * 2.0f + name_w,
                t + "Press the row to expand it."));
        }

        // ── Column 2: the stack's count (the retired +N canvas badge's figure) ──
        ImGui::TableSetColumnIndex(2);
        const ImVec2 count_p = ImGui::GetCursorScreenPos();
        if (count > 1)
            ImGui::TextDisabled("\xc3\x97%d", count);
        if (count > 1)
        {
            char buf[180];
            std::snprintf(buf, sizeof buf,
                          "%d of these stand on this tile, each with its own output.", count);
            cells.push_back(construction_cell("count", count_p, 22.0f, buf));
        }

        // ── Column 3: a resource pip for what it makes ──
        ImGui::TableSetColumnIndex(3);
        {
            const ImVec2 pp = ImGui::GetCursorScreenPos();
            if (makes)
                icons::resource(dl, {pp.x + construction_grid::makes * 0.5f, pp.y + line_h * 0.5f},
                                construction_grid::makes * 0.42f, good);
            ImGui::Dummy({construction_grid::makes, line_h});
            cells.push_back(construction_cell(
                "makes", pp, construction_grid::makes,
                makes ? std::string("Makes ") + resource_name(good) + "."
                      : (st.open ? std::string("This one makes no good of its own.")
                                 : std::string("A rival's method is private, so the pip is "
                                               "empty."))));
        }

        // ── Column 4: WHAT IT EARNS ──
        // Abbreviated to the column ("+1.2k"); the card names the period. A rival's
        // is an em dash, which is the `private` reading said in one glyph-width. A
        // stack still building leaves the column empty: it earns nothing yet, and
        // the state mark beside it is what says so.
        ImGui::TableSetColumnIndex(4);
        const ImVec2 net_p = ImGui::GetCursorScreenPos();
        if (!rd.building)
        {
            if (!st.open)
                ImGui::TextDisabled("\xe2\x80\x94");
            else
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(rd.colour), "%s",
                                   rd.net_short.c_str());
        }
        cells.push_back(construction_cell(
            "net", net_p, construction_grid::bar,
            rd.building ? std::string("It earns nothing yet: it is still being built.")
            : !st.open  ? std::string("A rival's books are private, so the column is a dash.")
                        : "Earns " + rd.text + " at today's local prices.",
            rd.building ? 0u : rd.colour));

        // ── Column 5: the state mark ──
        ImGui::TableSetColumnIndex(5);
        {
            const ImVec2 sp = ImGui::GetCursorScreenPos();
            icons::value_mark(dl, {sp.x + construction_grid::state * 0.5f, sp.y + line_h * 0.5f},
                              3.0f, rd.building ? rd.colour : IM_COL32(110, 116, 128, 255));
            ImGui::Dummy({construction_grid::state, line_h});
            // While it builds, the mark NAMES THE GOOD that holds it, in
            // `construction_stall_reason`'s one authored clause, with how far it
            // has got. Standing, it says only that it is standing.
            char buf[260];
            if (rd.building)
                std::snprintf(buf, sizeof buf, "%s - %.0f%% built.", rd.status.c_str(),
                              static_cast<double>(rd.progress * 100.0f));
            else
                std::snprintf(buf, sizeof buf, "Standing and working.");
            cells.push_back(construction_cell("state", sp, construction_grid::state, buf,
                                              rd.building ? rd.colour : 0u));
        }
        ImGui::PopID();

        // ── ONE CARD, FOR THE COLUMN UNDER THE POINTER (BL-1368) ──
        // The reading in full lives on the column it belongs to: the owner and the
        // press on the name, the good on the pip, the net on the net column, the
        // stall's named good and the progress on the state mark.
        {
            const std::string title = name + (count > 1 ? " \xc3\x97" + std::to_string(count)
                                                        : std::string());
            construction_column_card(title.c_str(), cells);
        }

        // Selects a building OF the stack (its oldest member); the band draws
        // this tile next frame with that stack expanded in place.
        if (pressed)
            ui.selected_entity = first;

        ui_state::tile_card_button readout;
        for (const grid_explainer& e : cells)
            readout.cols.push_back({e.key, e.text, (e.mn.x + e.mx.x) * 0.5f,
                                    (e.mn.y + e.mx.y) * 0.5f});
        readout.label  = name + (count > 1 ? " \xc3\x97" + std::to_string(count) : std::string());
        readout.detail = rd.text;
        readout.good   = makes ? resource_name(good) : "";
        readout.open   = st.open;
        readout.count  = count;
        readout.first  = first;
        readout.x      = gp.x + 24.0f;
        readout.y      = gp.y + line_h * 0.5f;
        readout.name_lines  = name_lines;   // BL-1371
        readout.name_elided = name_elided;
        ui.tile_card.buttons.push_back(std::move(readout));
    }

    ImGui::EndTable();
    ImGui::PopStyleVar();
}

/// What a building DOES, in one line (BL-1321; SELECTION.md § The building
/// element's layout: "Every card shows its method and status line, always").
/// A recipe building names its method; a building with no recipe names what it
/// does instead, from the real fields that decide it — a trade building its
/// trade points this quarter and the market it makes them for, a hub the haul
/// discount it gives. Reads only; never an invented figure.
std::string building_method_line(const world& w, const recipe_registry& reg,
                                 const economy_report& report, entity_id bid,
                                 const building_component& b, std::string* market_out = nullptr)
{
    char buf[160];
    switch (b.type)
    {
        case building_type::extraction_site:
            return std::string("Extracting ") + resource_name(b.target_resource);
        case building_type::processing_facility:
            if (const recipe* rc = reg.get_recipe(b.recipe); rc != nullptr)
                return rc->display_name;
            return "No method set";
        case building_type::port:
        case building_type::planetary_marketplace:
        {
            const entity_id   mid    = market_for_tile(w, b.tile);
            const std::string market = mid != null_entity ? market_city_name(w, mid)
                                                          : std::string("no market");
            if (b.decommissioned)
                return "Mothballed: makes no trade points";
            if (report.upkeep_unmet.count(bid) != 0)
                return "No trade points this quarter: upkeep unmet";
            // The figure `market_trade_points` sums for this building: its
            // (owner, body) labour scalar this tick, 1 where the tick has none.
            float contention = 1.0f;
            const entity_id corp = building_owner_of(w, bid).id; // BL-1406: a nation owns it once folded
            if (const auto tit = w.tiles.find(b.tile); tit != w.tiles.end())
                if (const auto cit = report.workforce_contention.find({corp, tit->second.body});
                    cit != report.workforce_contention.end())
                    contention = cit->second;
            // The market rides the hover (the card's column fits ~30 characters);
            // the figure is the line.
            const float pts = building_trade_points(reg, b, contention, /*upkeep_met=*/true);
            if (pts > 0.0f)
                std::snprintf(buf, sizeof buf, "Makes %.1f trade points / qtr", static_cast<double>(pts));
            else
                std::snprintf(buf, sizeof buf, "Makes trade points for %s", market.c_str());
            if (market_out != nullptr)
                *market_out = market;
            return buf;
        }
        case building_type::inland_logistics_hub:
            std::snprintf(buf, sizeof buf, "Cuts haul cost %.0f%% on routes through it",
                          static_cast<double>(reg.logistics_nodes().hub_discount * 100.0f));
            return buf;
        case building_type::launchpad:
            return "Launches convoys into space";
        case building_type::military_base:
            return "Musters units";
        case building_type::research_institute:
        case building_type::university:
            return "Makes research points for its nation";
        case building_type::schooling:
            return "Schooling for the centre it stands in";
        default:
            return building_type_name(b.type);
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
///   - the workforce row: a read-only bar and percentage (draw_workforce_bar,
///     BL-1322 — the game sets it).
/// The profit and workforce rows need the player's own COMPLETED building (the
/// profit row also needs a reading — a fresh campaign has the settle's, BL-1321).
/// A rival's card is the header, owner and an explicit `private` row; under
/// spectator god view it opens the method, real rows and profit row (BL-408).
/// The card holds no lever (BL-1322).
void draw_expanded_building_card(world& w, const recipe_registry& reg,
                                 const economy_report& report, ui_state& ui, entity_id tile_id,
                                 entity_id bid, const std::vector<production_stack>& stacks)
{
    const building_component& b = w.buildings.at(bid); // the card writes nothing (BL-1322)
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
    // BL-1321: drawn always — before the first quarter and for a building with
    // no recipe, which names what it does instead (building_method_line).
    const bool under_construction = (b.ticks_remaining > 0);

    // A SITE under construction is ONE ROW OF THE CONSTRUCTION GRID (BL-1365),
    // in the queue's own columns — glyph | what it makes | how far built |
    // days left | cost left | state mark — because a site on this card and a
    // row in the queue are the same subject seen from two places, and the words
    // they both used to spend four lines on belong to one hover card. The method
    // phrase and the status sentence (BL-1342's named stall, verbatim) are in the
    // card; both are still PUBLISHED, so nothing is lost to nowhere.
    //
    // Drawn before the rival short-circuit: construction stands on the map and is
    // public (the stack reading already says so), while the cost-left column is
    // financial and so is filled only for an `open` building.
    if (under_construction)
    {
        const construction_read rd = read_construction(w, reg, b.type, b.target_resource,
                                                       b.recipe, b.tile, b.state_idle,
                                                       construction_days_left(b));
        const std::string status = construction_status(rd, construction_days_left(b));
        const float       frac   = construction_progress_fraction(w, reg, b);
        const float       capex  = construction_capex(w, reg, b.tile, b.type, b.target_resource,
                                                      b.recipe);
        const float       left   = std::max(0.0f, capex - b.construction_paid);
        std::string       method;
        if (open)
        {
            std::string market;
            method = building_method_line(w, reg, report, bid, b, &market);
            ui.tile_card.method = method;
        }
        ui.tile_card.expanded_player = mine;
        ui.tile_card.status          = status;

        resource_type good   = resource_type::iron_ore;
        const bool    makes  = open && good_made_by(reg, b, good);
        const float   line_h = ImGui::GetTextLineHeight();
        const float   row_h  = construction_row_height();
        ImDrawList*   dl     = ImGui::GetWindowDrawList();

        constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg
                                        | ImGuiTableFlags_NoPadInnerX
                                        | ImGuiTableFlags_SizingFixedFit;
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                            {construction_grid::pad, ImGui::GetStyle().CellPadding.y});
        if (ImGui::BeginTable("##site_row", 6, flags))
        {
            ImGui::TableSetupColumn("##glyph", ImGuiTableColumnFlags_WidthFixed, construction_grid::glyph);
            ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("##done",  ImGuiTableColumnFlags_WidthFixed, construction_grid::bar);
            ImGui::TableSetupColumn("##qtrs",  ImGuiTableColumnFlags_WidthFixed, construction_grid::eta);
            ImGui::TableSetupColumn("##cost",  ImGuiTableColumnFlags_WidthFixed, construction_grid::cost);
            ImGui::TableSetupColumn("##state", ImGuiTableColumnFlags_WidthFixed, construction_grid::state);
            ImGui::TableNextRow(ImGuiTableRowFlags_None, row_h);

            ImGui::TableSetColumnIndex(0);
            const ImVec2 gp = ImGui::GetCursorScreenPos();
            if (makes)
                icons::resource(dl, {gp.x + construction_grid::glyph * 0.5f, gp.y + line_h * 0.5f},
                                construction_grid::glyph * 0.42f, good);
            else
                icons::under_construction(dl,
                                          {gp.x + construction_grid::glyph * 0.5f,
                                           gp.y + line_h * 0.5f},
                                          construction_grid::glyph * 0.5f,
                                          IM_COL32(150, 235, 160, 255));
            ImGui::Dummy({construction_grid::glyph, line_h});

            // BL-1368: an explainer per column on this row too.
            std::vector<grid_explainer> cells;

            ImGui::TableSetColumnIndex(1);
            const float label_w = std::max(16.0f, ImGui::GetContentRegionAvail().x);
            // BL-1371: through the shared wrapping draw like every other name
            // column, so the budget is stated in one place even where the label
            // is a literal that always fits.
            construction_grid_text("selection.site.label", "Building", label_w,
                                   /*disabled=*/true);
            cells.push_back(construction_cell(
                "name", gp, construction_grid::glyph + construction_grid::pad * 2.0f + label_w,
                method.empty()
                    ? std::string("A site under construction. It has no method or workforce to "
                                  "set until it finishes.")
                    : method + ". It has no levers until it finishes."));

            ImGui::TableSetColumnIndex(2);
            {
                const ImVec2 bp = ImGui::GetCursorScreenPos();
                construction_grid_bar(dl, {bp.x, bp.y + 3.0f},
                                      {bp.x + construction_grid::bar, bp.y + line_h - 3.0f},
                                      frac, palette::selection);
                ImGui::Dummy({construction_grid::bar, line_h});
                char buf[200];
                std::snprintf(buf, sizeof buf,
                              "%.0f%% built - the days of work done against the days the "
                              "whole build takes.", static_cast<double>(frac * 100.0f));
                cells.push_back(construction_cell("bar", bp, construction_grid::bar, buf));
            }

            ImGui::TableSetColumnIndex(3);
            const ImVec2 ep  = ImGui::GetCursorScreenPos();
            const int    eta = construction_eta_days(rd, construction_days_left(b));
            if (eta < 0)
                ImGui::TextColored(construction_status_colour(rd), "\xe2\x80\x94");
            else
                ImGui::TextColored(construction_status_colour(rd), "%d", eta);
            {
                char buf[220];
                if (eta < 0)
                    std::snprintf(buf, sizeof buf,
                                  "No honest estimate while it is paused, so the column shows a "
                                  "dash.");
                else
                    std::snprintf(buf, sizeof buf,
                                  "About %s left at the rate it is building now.",
                                  days_label(eta).c_str());
                cells.push_back(construction_cell(
                    "eta", ep, construction_grid::eta, buf,
                    ImGui::ColorConvertFloat4ToU32(construction_status_colour(rd))));
            }

            ImGui::TableSetColumnIndex(4);
            const ImVec2 cp = ImGui::GetCursorScreenPos();
            if (open)
                ImGui::TextDisabled("%s", fmt::abbreviate(static_cast<double>(left)).c_str());
            cells.push_back(construction_cell(
                "cost", cp, construction_grid::cost,
                open ? "" + std::to_string(static_cast<int>(left))
                           + " cr still to pay: the materials it has left to buy, at the "
                             "market's posted prices. Its build cost was paid when it was placed."
                     : std::string("A rival's spending is private, so the column is empty.")));

            ImGui::TableSetColumnIndex(5);
            {
                const ImVec2 sp = ImGui::GetCursorScreenPos();
                const ImU32  c  = (rd.rate >= 1.0f)
                                     ? IM_COL32(110, 116, 128, 255)
                                     : ImGui::ColorConvertFloat4ToU32(construction_status_colour(rd));
                icons::value_mark(dl, {sp.x + construction_grid::state * 0.5f, sp.y + line_h * 0.5f},
                                  3.0f, c);
                ImGui::Dummy({construction_grid::state, line_h});
                // The mark carries the status line itself - `construction_status`,
                // and so `construction_stall_reason`'s one authored clause.
                cells.push_back(construction_cell(
                    "state", sp, construction_grid::state, status,
                    ImGui::ColorConvertFloat4ToU32(construction_status_colour(rd))));
            }

            // One card, for the column under the pointer (BL-1368).
            construction_column_card("Under construction", cells);
            ui.tile_card.site_cols.clear();
            for (const grid_explainer& e : cells)
                ui.tile_card.site_cols.push_back({e.key, e.text, (e.mn.x + e.mx.x) * 0.5f,
                                                  (e.mn.y + e.mx.y) * 0.5f});
            ImGui::EndTable();
            ImGui::PopStyleVar();
        }
        else
            ImGui::PopStyleVar();

        if (!mine)
            draw_rival_building_summary(w, reg, bid, god);
        return; // still building: the header and this one row are the card
    }

    if (open)
    {
        std::string       market;
        const std::string method = building_method_line(w, reg, report, bid, b, &market);
        fit_text(text_box::table_cell, "selection.building.method", method.c_str(),
                 std::max(24.0f, ImGui::GetContentRegionAvail().x));
        if (!market.empty() && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\nFor the %s market.", method.c_str(), market.c_str());
        ui.tile_card.method = method;
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
    // ── The profit row ── (with data: a fresh campaign has the settle's)
    if (const building_profit p = estimate_building_reading(w, reg, report, bid); p.has_data)
    {
        ui.tile_card.charts = true;
        ImGui::Spacing();
        draw_building_profit(w, reg, report, bid, &ui.tile_card.net, &ui.tile_card.expenses);
    }

    // ── The workforce row: the player's own building only, READ-ONLY ──
    // (BL-1322: the game sets it; god view grants sight of a rival's profit row
    // above, and nothing here was ever a rival's.)
    if (mine)
    {
        ImGui::TextDisabled("Workforce");
        ImGui::SameLine();
        ui.tile_card.workforce = draw_workforce_bar(b);
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

// ── The PROVINCE CARD (BL-1312, tile card split) ──────────────────────────
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
            // Agricultural Produce reads "Farm" here (Ben, 2026-10-14): the row's
            // question is what can be built, and the 247 px card elided the long name.
            const char* row_label = (r.resource == resource_type::agricultural_produce) ? "Farm" : rp.name;
            fit_text(text_box::table_cell, "selection.province.resource", row_label,
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

// BL-1354: the tile card's LOGISTICS section (SELECTION.md § The tile element's
// layout, item 3; Ben, 2026-10-16: "show the number of logistics points provided,
// and the number consumed by that tile"). Two figures under one label - what a
// source here MAKES and what this tile's buildings USE out of what they need - read
// off the tick's own resolve (`report.logistic_points`), never re-derived, and a
// hover with the detail: whose pool the ground draws on, the coverage a building
// here would get, and a line per source and per counted building.
//
// USES COUNTS THE PLAYER'S OWN BUILDINGS (god view: all). A rival building's need is
// its run rate, which the competitor-visibility rule keeps private (DISCOVERY.md),
// so the hover says how many were left out rather than folding them in.
static void draw_tile_logistics(world& w, const economy_report& report, ui_state& ui,
                                entity_id tile_id, bool god)
{
    const auto host = w.tiles.find(tile_id);
    const lp_tick& lp = logistic_points_or_fallback(
        w, host != w.tiles.end() ? host->second.body : null_entity, report.logistic_points);

    // BL-1356 (NR-1057): "Makes" counts what a source here adds to the pool, which a
    // source off the chain to its capital does not; the hover names it as such.
    float makes = 0.0f;
    std::vector<const lp_source_row*> sources;
    for (const lp_source_row& s : lp.sources)
        if (s.tile == tile_id)
        {
            if (s.connected)
                makes += s.made;
            sources.push_back(&s);
        }

    float uses = 0.0f, need = 0.0f;
    int   rivals_hidden = 0;
    std::vector<const lp_building_row*> consumers;
    for (const lp_building_row& r : lp.buildings)
    {
        const auto bit = w.buildings.find(r.building);
        if (bit == w.buildings.end() || bit->second.tile != tile_id)
            continue;
        if (!god && !is_player_owned(w, r.building))
        {
            ++rivals_hidden;
            continue;
        }
        uses += r.granted;
        need += r.need;
        consumers.push_back(&r);
    }

    ui.tile_card.lp_drawn         = true;
    ui.tile_card.lp_makes         = makes;
    ui.tile_card.lp_uses          = uses;
    ui.tile_card.lp_need          = need;
    ui.tile_card.lp_rivals_hidden = rivals_hidden;

    // The label carries the hover; the two figures sit under it, one a line, because
    // the tile card is a quarter of the band and narrow at the reference width.
    ImGui::TextDisabled("Logistic Points");
    const bool hovered_label = ImGui::IsItemHovered();
    if (sources.empty() || !(makes > 0.0f))
        ImGui::TextDisabled("Makes none");
    else
        ImGui::Text("Makes %.0f", static_cast<double>(makes));
    if (need <= 0.0f)
        ImGui::TextDisabled("Uses none");
    else
    {
        // A shortfall reads as one: granted short of need by more than rounding.
        const bool short_of = uses + 0.05f < need;
        const ImVec4 col = ImGui::ColorConvertU32ToFloat4(short_of ? palette::negative
                                                                   : palette::text_secondary);
        ImGui::TextColored(col, "Uses %.1f of %.1f", static_cast<double>(uses),
                           static_cast<double>(need));
    }
    if (!hovered_label)
        return;

    ImGui::BeginTooltip();
    const auto nit = w.tile_to_nation.find(tile_id);
    const entity_id nation = (nit != w.tile_to_nation.end()) ? nit->second : null_entity;
    const auto nat = w.nations.find(nation);
    if (nation == null_entity || nat == w.nations.end())
        ImGui::TextUnformatted("Nobody's ground: it feeds and draws no pool.");
    else
    {
        ImGui::Text("Draws on %s's pool", nat->second.name.c_str());
        for (const lp_nation_row& n : lp.nations)
            if (n.nation == nation)
            {
                float claim = 0.0f;
                for (const float c : n.claim)
                    claim += c;
                ImGui::TextDisabled("Pool %.0f a tick, claimed %.0f", static_cast<double>(n.pool),
                                    static_cast<double>(claim));
            }
        // The one genuinely per-tile figure: what a building HERE would get, which
        // means something on an empty tile too.
        lp_reach_memo memo;
        ImGui::Text("Coverage here: %.0f%%",
                    static_cast<double>(lp_coverage_at(w, memo, tile_id)) * 100.0);
    }
    for (const lp_source_row* s : sources)
    {
        const auto bit = w.buildings.find(s->building);
        const char* name = bit != w.buildings.end() ? building_type_name(bit->second.type) : "Source";
        if (s->connected)
            ImGui::Text("%s makes %.0f", name, static_cast<double>(s->made));
        else
            ImGui::TextDisabled("%s: not tied to the capital, adds nothing", name);
    }
    for (const lp_building_row* r : consumers)
    {
        const auto bit = w.buildings.find(r->building);
        ImGui::Text("%s uses %.1f of %.1f",
                    bit != w.buildings.end() ? building_type_name(bit->second.type) : "Building",
                    static_cast<double>(r->granted), static_cast<double>(r->need));
    }
    if (rivals_hidden > 0)
        ImGui::TextDisabled("%d rival building%s not counted: private", rivals_hidden,
                            rivals_hidden == 1 ? "" : "s");
    ImGui::EndTooltip();
}

// The tile element (BL-1312; SELECTION.md § The tile element's layout): four
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
            // BL-1340: a road is an EDGE, so the card names the best tier meeting at the
            // tile. ONE-LINE ADAPTATION ONLY — the road ladder’s build rows (and a per-side
            // reading of this card) belong to the lane that owns this file.
            if (const char* tier = road_tier_name(max_road_tier(tile)))
            {
                ImGui::SameLine();
                ImGui::TextDisabled("\xc2\xb7 %s", tier);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Road tier: Track (thin) - Road - Highway (thick, bright).\n"
                                      "A better road moves goods across this tile faster.");
            }
            ImGui::Separator();
            draw_tile_logistics(w, report, ui, tile_id, god);
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
        //
        // BL-1406: every probe names the player's ACTOR (its nation once folded),
        // so the own-ground gate (NATIONS.md § Own ground) and the tech gate are
        // asked here exactly as construct_nation will ask them — the button is
        // never enabled on ground the press would be refused on.
        const entity_id actor = player_actor(w);
        bool any_placeable = false;
        if (water)
        {
            any_placeable = placement_rules::can_place_in_world(
                                w, sel, building_type::port, resource_type::iron_ore,
                                ui.max_logistics_reach, actor).ok();
        }
        else
        for (const resource_type er : placement_rules::k_extractable)
            if (tile.resource_deposit[static_cast<std::size_t>(er)] > 0.0f &&
                placement_rules::can_place_in_world(w, sel, building_type::extraction_site, er,
                                                   ui.max_logistics_reach, actor).ok())
            { any_placeable = true; break; }
        // BL-1198: a Well needs no deposit, so the loop above cannot see it.
        if (!any_placeable && !water
            && placement_rules::is_well_site(w, sel, resource_type::water)
            && placement_rules::can_place_in_world(w, sel, building_type::extraction_site,
                                                   resource_type::water, ui.max_logistics_reach,
                                                   actor).ok())
            any_placeable = true;
        // BL-1199: nor a Fishing Wharf (coastal, no produce deposit).
        if (!any_placeable && !water
            && placement_rules::is_wharf_site(w, sel, resource_type::agricultural_produce)
            && placement_rules::can_place_in_world(w, sel, building_type::extraction_site,
                                                   resource_type::agricultural_produce,
                                                   ui.max_logistics_reach, actor).ok())
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
                                                       ui.max_logistics_reach, actor).ok())
                { any_placeable = true; break; }
        }

        bool building_underway = false;
        if (const std::vector<entity_id>* mine = assets_of(w, actor); mine != nullptr)
            for (entity_id bid : *mine)
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

        // No Manage Buildings (BL-1312): the tile card's building grid IS the
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
    // region. The tile card's verify readout (BL-1312) is refilled only by a
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

    // A selected TILE and a selected BUILDING are one element (BL-1312,
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

namespace {

// --- What a build COSTS and what it GIVES (BL-1344) -------------------------
// Ben, playtest 2026-10-16: "change building tiles in construction to show the
// cost of building it in both resources and credits, and then the output resource
// or effect, rather than just the credits expected. And we should remove the
// 'payback' portion."

/// The MATERIALS a build draws, in the registry's own row — "12 Stone, 4 Timber",
/// "" where the type needs none. The credit figure beside it (`construction_capex`)
/// prices these at the local market on top of the flat build cost: the flat credits
/// leave the treasury at placement, and the materials are bought at the posted
/// price as each tick draws them (Ben, 2026-10-26: costs depend on market prices).
std::string build_material_line(const recipe_registry& reg, building_type type,
                                resource_type target, std::uint16_t recipe)
{
    const auto& row = reg.resource_build_cost_for(type, target, recipe);
    std::string out;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (!(row[r] > 0.0f))
            continue;
        if (!out.empty())
            out += ", ";
        char qty[24];
        std::snprintf(qty, sizeof qty, "%.0f ", static_cast<double>(row[r]));
        out += qty;
        out += resource_name(static_cast<resource_type>(r));
    }
    return out;
}

/// What the building will DO, in one phrase — the output good for a producer, the
/// effect for infrastructure that has none. The SAME vocabulary the built card's
/// `building_method_line` uses, in the future tense a candidate needs; a second set
/// of words for the same types is how a door and a card come to describe different
/// buildings.
std::string candidate_effect_line(const recipe_registry& reg, building_type type,
                                  resource_type target, std::uint16_t recipe_id)
{
    char buf[160];
    switch (type)
    {
        case building_type::extraction_site:
            return std::string("Makes ") + resource_name(target);
        case building_type::processing_facility:
        {
            const recipe* rc = reg.get_recipe(recipe_id);
            if (rc == nullptr)
                return "No method set";
            std::string outs;
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                if (!(rc->outputs[r] > 0.0f))
                    continue;
                if (!outs.empty())
                    outs += ", ";
                outs += resource_name(static_cast<resource_type>(r));
            }
            return outs.empty() ? std::string("Makes nothing yet") : "Makes " + outs;
        }
        case building_type::port:
        case building_type::planetary_marketplace:
            return "Makes trade points for this market";
        case building_type::inland_logistics_hub:
            std::snprintf(buf, sizeof buf, "Cuts haul cost %.0f%% on routes through it",
                          static_cast<double>(reg.logistics_nodes().hub_discount * 100.0f));
            return buf;
        case building_type::launchpad:
            return "Launches convoys into space";
        case building_type::military_base:
            return "Musters units";
        case building_type::research_institute:
        case building_type::university:
            return "Makes research points for its nation";
        default:
            return building_type_name(type);
    }
}

/// --- How many of this already stand here, and how many more fit (BL-1341) ---
/// Ben, 2026-10-16: *"For multiple of the same building on one tile, it's not
/// clear how to build the second level."* Nothing refused him — the stack caps
/// permit 2-12 non-extraction buildings per tile and `richness / 50` extraction
/// sites per deposit — so this is an AFFORDANCE defect, not a rule one: the row
/// never said a second was allowed, nor that Build ADDS one rather than upgrading
/// the building already there. The road and rail rows beside it ARE upgrades in
/// place, so the two idioms sit together and must not read alike.
///
/// The numbers are the rules' own (`placement_rules`), never a guess, and the
/// wording keeps the two caps' different shapes: an extraction cap is PER DEPOSIT,
/// a non-extraction cap is AGGREGATE across every non-extraction type, so the room
/// left is shared with every other such row on the tile.
struct stack_room
{
    int  of_type = 0; ///< How many of THIS named thing already stand on the tile.
    int  here    = 0; ///< The occupancy the cap is measured against (aggregate when shared).
    int  cap     = 0; ///< That ceiling.
    bool shared = false; ///< The cap (and so the room left) is aggregate, not per type.
};

stack_room candidate_stack_room(const world& w, const recipe_registry& reg, entity_id tile_id,
                                const tile_component& tile, building_type type,
                                resource_type target, const std::string& row_name)
{
    stack_room out;
    out.shared  = (type != building_type::extraction_site);
    out.of_type = placement_rules::buildings_on_tile(w, tile_id, type, target);
    // A processing row is a RECIPE GROUP, not a type (BL-434), so counting the
    // type would tell an Alloy row that the Bloomery beside it is one of its own.
    // Count what the row actually offers: buildings the player would call by the
    // row's own name, which is what `building_group_name` answers.
    if (type == building_type::processing_facility)
    {
        out.of_type = 0;
        for (const auto& [bid, b] : w.buildings)
        {
            (void)bid;
            if (b.tile == tile_id && b.type == type && building_group_name(w, reg, b) == row_name)
                ++out.of_type;
        }
    }
    out.here    = out.shared ? placement_rules::non_extraction_buildings_on_tile(w, tile_id)
                             : out.of_type;
    out.cap     = placement_rules::stack_capacity(tile, type, target);
    return out;
}

/// The row's stack line. Present on EVERY valid row, because "you may add another"
/// is exactly as much news as "this tile is full".
///
/// The two caps are worded differently ON PURPOSE. An extraction cap is the
/// DEPOSIT's, and it is this type's alone. A non-extraction cap is the GROUND's,
/// and it is shared across every non-extraction building on the tile — so the
/// aggregate is said as slots used of slots available, never as a count of this
/// type, which would promise room that the row beside it can take.
std::string stack_room_line(const stack_room& sr)
{
    const int left = std::max(0, sr.cap - sr.here);
    char buf[200];
    // "Adds another", never "upgrades": a stacked build is one MORE building with
    // its own (decayed) output, not a tier. The road and rail rows below ARE
    // upgrades in place, and the two must not read alike.
    const char* add = (left > 0) ? " - Build adds another" : "";
    if (sr.shared)
    {
        if (sr.of_type > 0)
            std::snprintf(buf, sizeof buf, "%d here%s; %d of %d slots on this tile used",
                          sr.of_type, add, sr.here, sr.cap);
        else
            std::snprintf(buf, sizeof buf, "None here; %d of %d slots on this tile used",
                          sr.here, sr.cap);
        return buf;
    }
    if (sr.of_type > 0)
        std::snprintf(buf, sizeof buf, "%d here%s; this deposit supports %d", sr.of_type, add,
                      sr.cap);
    else
        std::snprintf(buf, sizeof buf, "None here; this deposit supports %d", sr.cap);
    return buf;
}

/// A refusal that NAMES WHAT FILLED THE TILE (BL-1341), where the shared
/// `placement_reason_text` can only say "No build slot free here" — it has no tile
/// and no counts. Everything else keeps the shared message verbatim.
std::string candidate_refusal(const world& w, entity_id tile_id,
                              const placement_rules::placement_result& pr, const stack_room& sr)
{
    char buf[200];
    if (pr.reason == placement_rules::placement_reason::slot_full)
    {
        if (sr.shared)
            std::snprintf(buf, sizeof buf,
                          "Full: this ground takes %d buildings of any kind, and %d stand here",
                          sr.cap, sr.here);
        else
            std::snprintf(buf, sizeof buf,
                          "Full: this deposit supports %d site%s, and %d work%s it",
                          sr.cap, sr.cap == 1 ? "" : "s", sr.here, sr.here == 1 ? "s" : "");
        return buf;
    }
    if (pr.reason == placement_rules::placement_reason::province_full)
    {
        const uint32_t pid = w.provinces.province_of(tile_id);
        const int ceiling  = (pid != 0) ? province_building_ceiling(w, pid) : -1;
        const int standing = (pid != 0) ? province_buildings_standing(w, pid) : -1;
        if (ceiling >= 0)
        {
            std::snprintf(buf, sizeof buf,
                          "The province is full: %d buildings standing of the %d this land "
                          "sustains - improve it, or build elsewhere",
                          standing, ceiling);
            return buf;
        }
    }
    return pr.message();
}

} // namespace

// The tile construction ledger (BL-162): the tile-contextual surface that actually
// lets the player build. Lists every building type placeable on the selected tile —
// each in a bordered container (name + the build cost in materials AND credits +
// what the building gives + an expected-profit magnitude bar + how many of it
// already stand on the tile + a reason-coded validity read + a Build action) — and
// enqueues the chosen build on the tile via the construction.pending_tile seam app
// executes.
// Candidates sort by expected net descending on a ceiling shared across the list, so
// the best options are the ones on screen without scrolling.
void draw_construction_ledger_body(const world& w, const recipe_registry& reg, ui_state& ui,
                                   float lp_cover)
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
        // Building is a TARGETED act, so with no target there is nothing to price.
        // The list of tiles the player could build on is the SITE LEDGER's, drawn
        // above this bar (construction_sites.cpp): building first, then the tile
        // (Ben, 2026-10-25, overturning the 2026-08-29 line that any ordering of
        // that list is a recommendation; CONCEPT.md). So this stays one line.
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

    // The player's money — the affordability context for every Build button below.
    // BL-1406: its nation's treasury once folded (HEADER.md § Contents).
    const entity_id actor = player_actor(w);
    float balance = 0.0f;
    if (const float* p = purse_of(w, actor); p != nullptr)
        balance = *p;
    ImGui::TextDisabled("Treasury: %.0f cr", static_cast<double>(balance));

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
        construction_read crd;           ///< The sim's build rate at this tile (BL-328 pre-commit warning, BL-1342's mirror).
        stack_room      room;            ///< BL-1341: how many stand here, how many more fit.
        std::string     materials;       ///< BL-1344: the build cost in goods.
        std::string     effect;          ///< BL-1344: what the building gives.
        /// BL-1365: the good this row's building would MAKE, for the line's
        /// resource pip — the extraction target, or the recipe's primary output.
        /// False for infrastructure, whose column stays empty and whose effect
        /// phrase is the hover card's.
        bool            makes_good = false;
        resource_type   good       = resource_type::iron_ore;
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
                               [&w, &reg, tile_id, actor](const candidate& c)
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
                                   return !recipe_unlocked(w, reg, actor, c.recipe);
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
                                                  actor, // BL-1406: own ground + tech
                                                  reg.placement_gate_for(c.type, c.recipe));
        // BL-328 pre-commit warning, on BL-1342's corrected mirror: the market's
        // persistent stock, the capacity row and the fair-price ceiling, exactly
        // as run_construction reads them.
        c.crd       = read_construction(w, reg, c.type, c.target, c.recipe, tile_id);
        // The row's own identity: a processing row is its recipe's GROUP (the name
        // the BL-434 collapse gives it below), everything else its own name.
        c.room      = candidate_stack_room(w, reg, tile_id, tile, c.type, c.target,
                                           c.type == building_type::processing_facility
                                               ? recipe_group_of(reg, c.recipe)
                                               : c.name);
        c.materials = build_material_line(reg, c.type, c.target, c.recipe);
        c.effect    = candidate_effect_line(reg, c.type, c.target, c.recipe);
        // BL-1365: what the row's resource pip draws. Same rule the glyph's
        // identity uses (ICONS.md § 1c) — the target for an extraction site, the
        // recipe's primary output for a processing facility, nothing otherwise.
        if (c.type == building_type::extraction_site)
        {
            c.makes_good = true;
            c.good       = c.target;
        }
        else if (c.type == building_type::processing_facility && c.recipe != no_recipe)
            if (const recipe* rc = reg.get_recipe(c.recipe); rc != nullptr)
            {
                c.makes_good = true;
                c.good       = primary_output_resource(*rc);
            }

        // Capex is the figure construction.cpp actually gates on and charges.
        // BL-1066: the SAME function the gate prices with — the flat cost times this
        // tile's site multiplier, plus the materials at this market's posted price
        // (Ben, 2026-10-26: costs depend on market prices) — so the two cannot disagree.
        c.capex = construction_capex(w, reg, tile_id, c.type, c.target, c.recipe);

        c.produces = (c.type == building_type::extraction_site ||
                      c.type == building_type::processing_facility);
        // Ben, 2026-10-25: the tile's Logistic Point cover is applied, exactly as
        // the site ledger applies it, so the two surfaces quote one figure.
        c.profit   = estimate_prospective_profit(w, reg, tile_id, c.type, c.target, c.recipe,
                                                 nullptr, nullptr, lp_cover);
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
    // BL-1344: the payback clause is gone from the rows, so the caption's pointer at
    // it goes with it. What replaced it is the cost said in full — goods and credits
    // — which is the honest version of the same sentence: the bar is earnings, the
    // cost lines above it are cost.
    ImGui::TextWrapped("Est. net / qtr at today's local prices - at least this, before "
                       "auto-staffing; 50%% staffing, no labour shortage, this tile's Logistic Point cover.");
    ImGui::PopStyleColor();

    // ONE LINE A ROW, AND IT IS A GRID (BL-1365; construction.md § "One line a
    // row, and the words live in the hover", § "The line is a grid, not a
    // sentence"; Ben, 2026-10-17). The previous pass put the right FACTS here and
    // then spent SEVEN reserved lines a row saying them, so four rows filled the
    // panel. Every fact is kept; what changed is WHERE it is read. The line is
    // eight fixed columns of glyphs and figures — the same columns in every row,
    // so a column reads down the list as a column — and every sentence the line
    // dropped is in the shared hover card (TOOLTIP.md), which is the only other
    // place a fact is allowed to go.
    const float row_h = construction_row_height();

    // The road ladder keeps its own TALLER row: its tier glyph box is 56 px, and
    // the three tiers are an UPGRADE ladder on one tile rather than rows of this
    // list. The RAIL rows are no longer the exception they were — BL-1366: they
    // drew a four-line bordered box, so Track and Railway Station sat visibly
    // taller than the one-line rows above and below them. They now take `row_h`
    // and `construction_begin_build_grid`'s own columns (rail_tool.cpp).
    constexpr float img = 56.0f;

    ImGui::BeginChild("##build_list", {0.0f, 0.0f}, false,
                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ui::foldout_scroll_child("##build_list"); // verify.scroll_panel("build_list", ...)
    // BL-326: candidates are pre-sorted category-then-name, so each category's rows are
    // contiguous — walk the list opening a TreeNodeEx (this file's existing fold idiom)
    // whenever the category changes, and close it once its rows
    // are drawn. Defaulted open, matching the fold usage elsewhere in the UI.
    // BL-1255 (LOGISTICS.md 4c): rail is built like anything else - its rows
    // join their kin (Ben, 2026-10-15): Track closes the road ladder below,
    // Railway Station closes the Infrastructure fold beside port and hub.
    // The grid itself. ONE TABLE PER CATEGORY FOLD, all of them declaring the
    // SAME fixed column widths (`construction_grid`), so a column is in the same
    // place in every fold and the stretch name column resolves to the same
    // remainder in each — the folds read as one grid with headings in it, which
    // is what the alignment rule asks for.
    //
    // Eight columns: glyph | name | cost | what it makes | stack | state | profit
    // bar | press. MATERIALS is the ninth and it does not fit at 380 px, so it is
    // the column dropped into the hover card (the lowest-value one: the cost in
    // credits beside it is the flat cost, and the quantities a player can act on
    // were never on the line anyway).
    std::string open_category;
    bool        category_open = false;
    bool        grid_open     = false;
    const auto  close_category = [&]() {
        if (grid_open)
        {
            construction_end_grid();
            grid_open = false;
        }
        if (category_open)
        {
            // BL-1366: the station row is now the SAME grid as the rows above it —
            // its own table, declared by the shared `construction_begin_build_grid`,
            // at the same indent and so in the same columns. It draws after the
            // fold's table rather than inside it because a nested table would
            // re-resolve the stretch column against one cell's width.
            if (open_category == "Infrastructure")
                draw_rail_rows(w, reg, ui, tile_id, row_h, rail_row::station);
            ImGui::TreePop();
            category_open = false;
        }
    };

    ui.construction_ui.door_rows.clear();
    ui.construction_ui.rail_rows.clear(); // BL-1366: both rail rows draw below this loop
    ui.construction_ui.road_rows.clear(); // BL-1370: the road ladder draws below it too
    ui.construction_ui.row_h  = row_h;
    ui.construction_ui.line_h = ImGui::GetTextLineHeight();

    ImDrawList* cdl = ImGui::GetWindowDrawList();
    for (const candidate& c : cands)
    {
        if (c.category != open_category)
        {
            close_category();
            open_category = c.category;
            category_open = ImGui::TreeNodeEx(open_category.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            if (category_open)
                grid_open = construction_begin_build_grid(("##grid_" + open_category).c_str());
        }
        if (!category_open || !grid_open)
            continue;

        const bool affordable = balance >= c.capex;
        const bool stalls     = c.pr.ok() && c.crd.rate < 1.0f;

        ImGui::TableNextRow(ImGuiTableRowFlags_None, row_h);
        ImGui::PushID(c.name.c_str());
        const float line_h = ImGui::GetTextLineHeight();
        // BL-1368: EACH ELEMENT EXPLAINS ITSELF, AND ONLY ITSELF. Every column
        // collects its own cell and its own one-or-two-sentence explainer as the
        // row draws it; the whole set is hit-tested AFTER the row, so at most one
        // card draws and the Build press keeps its click (a rect hit-test, never
        // an item). The row-wide card this replaced said all eight at once and was
        // longer in the hand than the seven lines it had replaced.
        std::vector<grid_explainer> cells;

        // ── Column 0: the building glyph ──
        ImGui::TableSetColumnIndex(0);
        const ImVec2 gp = ImGui::GetCursorScreenPos();
        {
            constexpr float gr = construction_grid::glyph * 0.5f;
            // BL-429: an extraction row carries its target in c.target; a
            // processing row's c.target is the placeholder iron_ore the candidate
            // was built with, so the glyph is computed from the row's own recipe
            // instead, matching its name (Bloomery, Smithy, ...).
            const resource_type icon_identity =
                (c.type == building_type::processing_facility && c.recipe != no_recipe)
                    ? primary_output_resource(*reg.get_recipe(c.recipe))
                    : c.target;
            icons::building(cdl, {gp.x + gr, gp.y + line_h * 0.5f}, gr,
                            c.type, icon_identity, IM_COL32(150, 235, 160, 255));
            ImGui::Dummy({construction_grid::glyph, line_h});
        }

        // ── Column 1: the name, elided to its column ──
        ImGui::TableSetColumnIndex(1);
        const float name_w = std::max(16.0f, ImGui::GetContentRegionAvail().x);
        ImGui::PushStyleColor(ImGuiCol_Text, palette::selection);
        // BL-1371: the name WRAPS into the row's second line rather than eliding.
        // This column is the whole reason the row grew: at the fold-out's
        // narrowest it resolves to ~70 px, and most building names did not fit.
        bool      name_elided = false;
        const int name_lines  = construction_grid_text("build.row.name", c.name.c_str(),
                                                       name_w, /*disabled=*/false,
                                                       &name_elided);
        ImGui::PopStyleColor();
        // The glyph and the name are ONE subject, so they share one explainer and
        // one cell: which building this row offers, said in full where the column
        // elided it (the card's title) and which family it belongs to.
        cells.push_back(construction_cell(
            "name", gp, construction_grid::glyph + construction_grid::pad * 2.0f + name_w,
            c.category + " building on this tile."));

        // ── Column 2: THE COST IN CREDITS, abbreviated ──
        // The figure `construction_capex` prices and the gate charges — the flat
        // cost times the site multiplier (Ben, 2026-10-26). Abbreviated ("1.2k") because the
        // column is 34 px; the hover says it in full, in credits, beside the
        // materials it paid for.
        ImGui::TableSetColumnIndex(2);
        const ImVec2 cost_p = ImGui::GetCursorScreenPos();
        ImGui::TextDisabled("%s", fmt::abbreviate(static_cast<double>(c.capex)).c_str());
        {
            // The cost column explains THE COST, which is one fact said two ways:
            // the credits, and the goods those credits buy at this market. The
            // materials were the ninth column the line could not fit.
            char buf[280];
            if (c.materials.empty())
                std::snprintf(buf, sizeof buf,
                              "Costs %d cr, the flat build cost; this one needs no materials.",
                              static_cast<int>(c.capex));
            else
                std::snprintf(buf, sizeof buf,
                              "Costs %d cr - the flat build cost, paid when it is placed, plus its "
                              "materials at this market's posted prices, bought as it builds. "
                              "Materials: %s.",
                              static_cast<int>(c.capex), c.materials.c_str());
            cells.push_back(construction_cell("cost", cost_p, construction_grid::cost, buf));
        }

        // ── Column 3: A RESOURCE PIP for what it makes ──
        // `icons::resource` in the good's own identity colour (ICONS.md § 1), not
        // the word: "Makes Iron Ore" is a sentence and sentences live in the card.
        // Infrastructure that makes no good leaves the column EMPTY rather than
        // closing it up, and its effect phrase is in the card.
        ImGui::TableSetColumnIndex(3);
        {
            const ImVec2 pp = ImGui::GetCursorScreenPos();
            if (c.makes_good)
                icons::resource(cdl, {pp.x + construction_grid::makes * 0.5f, pp.y + line_h * 0.5f},
                                construction_grid::makes * 0.42f, c.good);
            ImGui::Dummy({construction_grid::makes, line_h});
            // The pip says WHAT IT MAKES, and an empty pip still explains itself:
            // infrastructure earns through what it enables, and that phrase is the
            // one the column cannot draw.
            cells.push_back(construction_cell(
                "makes", pp, construction_grid::makes,
                c.makes_good ? c.effect + "."
                             : "Makes no good, so the pip is empty. " + c.effect + "."));
        }

        // ── Column 4: THE STACK, as "2/4" (BL-1341, compacted) ──
        // How many stand here over the ceiling they are measured against. The two
        // caps have different SHAPES — an extraction cap is this deposit's, a
        // non-extraction cap is the ground's and is shared with every other such
        // row — and the figure cannot say which, so the card does, in the sentence
        // the number replaced.
        ImGui::TableSetColumnIndex(4);
        const ImVec2      stack_p    = ImGui::GetCursorScreenPos();
        const std::string stack_line = stack_room_line(c.room);
        ImGui::TextDisabled("%d/%d", c.room.shared ? c.room.here : c.room.of_type, c.room.cap);
        // WHICH CAP the figure counts against - the half "2/4" cannot say, and the
        // reason this column needs an explainer of its own at all.
        cells.push_back(construction_cell(
            "stack", stack_p, construction_grid::stack,
            stack_line + ". "
                + (c.room.shared
                       ? "The cap is this ground's, shared with every non-extraction building on it."
                       : "The cap is this deposit's own, counting sites of this kind alone.")));

        // The three sentences the marks and the press explain, authored once each:
        // the stall clause is `construction_stall_reason`'s (the SINGLE author, so
        // the state mark and the status line elsewhere cannot drift), the refusal is
        // `candidate_refusal`'s with the rules' own counts.
        const std::string refusal = c.pr.ok() ? std::string()
                                              : candidate_refusal(w, tile_id, c.pr, c.room);
        const std::string stall   = !stalls ? std::string()
                                  : (c.crd.paused ? std::string("Will stall - ")
                                                  : std::string("Will stretch - "))
                                        + construction_stall_reason(c.crd) + ".";

        // ── Column 5: THE STATE MARK ──
        // `icons::value_mark`, the vocabulary's single filled dot: a quiet grey dot
        // when the build will simply proceed, amber when it will stretch, red when
        // it will stall or the tile refuses it outright. One mark in one place,
        // which is what replaced BL-328's reserved warning LINE.
        ImGui::TableSetColumnIndex(5);
        {
            const ImVec2 sp = ImGui::GetCursorScreenPos();
            const ImU32  mark = !c.pr.ok()        ? IM_COL32(230, 140, 140, 255)
                              : (!stalls)         ? IM_COL32(110, 116, 128, 255)
                                                  : ImGui::ColorConvertFloat4ToU32(
                                                        construction_status_colour(c.crd));
            icons::value_mark(cdl, {sp.x + construction_grid::state * 0.5f, sp.y + line_h * 0.5f},
                              3.0f, mark);
            ImGui::Dummy({construction_grid::state, line_h});
            // The mark NAMES THE GOOD that would hold the build, in the authored
            // clause; on a refused row it says only what the colour means and
            // leaves the reason to the press, whose fact it is.
            cells.push_back(construction_cell(
                "state", sp, construction_grid::state,
                !c.pr.ok() ? std::string("Red: this tile refuses the building. The Build press "
                                         "says what refused it.")
                : !stall.empty() ? stall
                                 : std::string("Nothing is holding this build back: it would run "
                                               "at full rate from the day it starts."),
                c.pr.ok() && !stall.empty()
                    ? ImGui::ColorConvertFloat4ToU32(construction_status_colour(c.crd))
                    : 0u));
        }

        // ── Column 6: THE PROFIT BAR ──
        // Length is |net| / the ceiling shared across the whole list, colour is
        // the sign. Infrastructure and an unpriceable row leave the column empty:
        // a red bar for "earns through what it enables" would read as a bad
        // investment, and the card carries the upkeep sentence instead.
        ImGui::TableSetColumnIndex(6);
        {
            const ImVec2 bp = ImGui::GetCursorScreenPos();
            if (c.group == 0)
                construction_grid_bar(cdl, {bp.x, bp.y + 3.0f},
                                      {bp.x + construction_grid::bar, bp.y + line_h - 3.0f},
                                      ceiling > 0.0f ? std::fabs(c.profit.net()) / ceiling : 0.0f,
                                      value_colour(static_cast<double>(c.profit.net())));
            ImGui::Dummy({construction_grid::bar, line_h});
            // The bar explains THE ESTIMATE, caveat included: the figure the bar
            // no longer prints (38 px holds a magnitude or a number, not both),
            // and the staffing assumption the caption above the list carries.
            char nbuf[260];
            if (c.group == 0)
                std::snprintf(nbuf, sizeof nbuf,
                              "Est. net %+d cr / qtr at today's local prices, at 50%% staffing "
                              "with no labour shortage and %d%% Logistic Point cover. The bar is "
                              "that figure against the best row in this list.",
                              static_cast<int>(c.profit.net()),
                              static_cast<int>(std::lround(lp_cover * 100.0f)));
            else if (c.pr.ok() && !c.produces && c.profit.has_data)
                std::snprintf(nbuf, sizeof nbuf,
                              "No direct revenue - upkeep %d cr / qtr. It earns through what it "
                              "enables, so the column stays empty rather than showing a loss.",
                              static_cast<int>(c.profit.maintenance + c.profit.wages));
            else
                std::snprintf(nbuf, sizeof nbuf,
                              "No estimate for this row yet, so the column is empty.");
            cells.push_back(construction_cell("bar", bp, construction_grid::bar, nbuf,
                                              c.group == 0
                                                  ? value_colour(static_cast<double>(c.profit.net()))
                                                  : 0u));
        }

        // ── Column 7: THE PRESS ──
        // Always in the same place, enabled only when the tile and the balance
        // both allow it. A refused or unaffordable row keeps a DISABLED press
        // rather than swapping the column for a sentence, so the grid never
        // shifts; the reason is the card's.
        ImGui::TableSetColumnIndex(7);
        const ImVec2 act_p = ImGui::GetCursorScreenPos();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.0f, 0.0f});
        ImGui::BeginDisabled(!c.pr.ok() || !affordable);
        if (ImGui::Button("Build"))
        {
            ui.construction.pending_tile   = tile_id;
            ui.construction.pending_type   = c.type;
            ui.construction.pending_target = c.target;
            ui.construction.pending_recipe = c.recipe; // the row's recipe, not the default
        }
        ImGui::EndDisabled();
        ImGui::PopStyleVar();
        ImGui::PopID();
        {
            // The press explains WHY IT WILL NOT PRESS, which is the only fact a
            // disabled control owes the player: the tile's refusal with the rules'
            // own counts, or the balance. The cell is hit-tested like every other,
            // so the explainer costs the press nothing - `construction_row_hovered`
            // is a rect test, never an item over the button.
            char abuf[300];
            if (!refusal.empty())
                std::snprintf(abuf, sizeof abuf, "%s", refusal.c_str());
            else if (!affordable)
                std::snprintf(abuf, sizeof abuf,
                              "Can't afford it: the press charges %d cr and the balance is short.",
                              static_cast<int>(c.capex));
            else
                std::snprintf(abuf, sizeof abuf,
                              "Starts the build here and charges the %d cr now.",
                              static_cast<int>(c.capex));
            cells.push_back(construction_cell(
                "act", act_p, construction_grid::act, abuf,
                refusal.empty() ? 0u : IM_COL32(230, 140, 140, 255)));
        }

        // ── ONE CARD, FOR THE COLUMN UNDER THE POINTER ──
        // Each column's own explainer, hit-tested in drawn order (BL-1368). What
        // the eight cells say between them is exactly what the row-wide card said
        // - every fact landed on exactly one column, none was dropped - but a
        // player now reads one or two sentences about the thing they pointed at.
        construction_column_card(c.name.c_str(), cells);

        // The surface reporting its own row: a one-line row carries glyphs and
        // abbreviations and its words are now per column, so a capture can prove
        // neither. The check hovers each cell's own centre and asserts that
        // column's explainer; `hover` stays the UNION, which is the one assertion
        // that nothing was lost when the row-wide card was split.
        {
            construction_controls::grid_row pub;
            pub.name  = c.name;
            pub.hover = construction_cells_text(cells);
            pub.x     = gp.x + 24.0f;
            pub.y     = gp.y + line_h * 0.5f;
            pub.h           = row_h;
            pub.name_lines  = name_lines;
            pub.name_elided = name_elided;
            for (const grid_explainer& e : cells)
                pub.cols.push_back({e.key, e.text, (e.mn.x + e.mx.x) * 0.5f,
                                    (e.mn.y + e.mx.y) * 0.5f});
            ui.construction_ui.door_rows.push_back(std::move(pub));
        }
    }
    close_category();

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
                // BL-1406: the base must be the player ACTOR's (its nation's once
                // folded) — `owner_holds` reads either owner's asset list.
                if (owner_holds(w, player_actor(w), bid))
                    has_muster_base = true;
                break;
            }
        }
        const auto& table = unit_roster_table();
        const auto  avail = has_muster_base
            ? available_rows(w, player_actor(w), campaign_roster_band_for(reg.era()))
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
    // it takes its own affordances + the pending_road_tile/tier/side path (place_road). Three
    // tiers: Track (1) / Road (2) / Highway (3); the glyph weight + brightness mirror the
    // on-canvas render, and each shows its own cost.
    //
    // BL-1370: THE PRESS ARMS, IT DOES NOT BUILD (construction.md § "A road is built from a road
    // to a tile, in two presses"). A road is a crossing between two tiles, so the request has to
    // name a SIDE — and until this item nothing did: the press passed `k_road_side_auto` and the
    // rules guessed, which is how a tile the player was building OUT of came back "this tile
    // already has an equal or better road". Each row now asks the real gate once per side
    // (`ui::road_crossing_targets`), arms on the press, and the canvas highlights exactly the
    // neighbours the rules admit.
    //
    // BL-1379: where that set is EMPTY the press is muted and unpressable, and the reason sits
    // on the press's own explainer — not in a row of red text (construction.md § "A tile with
    // nowhere to go mutes the press").
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
        // BL-1348: whether this tile carries a crossing at all decides WHICH empty-set
        // refusal the press explains — "nowhere left to go" or "nothing here to go from".
        const bool tile_carries_road = [&]() {
            const auto tit = w.tiles.find(tile_id);
            return tit != w.tiles.end() && tile_has_road(tit->second);
        }();

        for (const road_tier_affordance& rt : road_tiers)
        {
            ImGui::PushID(static_cast<int>(rt.level)); // unique ImGui ids per tier

            const road_economics& re = reg.road_econ(rt.level);
            // BL-1370: A ROAD NAMES WHICH NEIGHBOUR IT CROSSES TO. The row no
            // longer asks the weak "may this TILE be roaded?" form — that is the
            // question that answered "this tile already has an equal or better
            // road" for a tile the player was trying to build OUT of. It asks the
            // real gate, once per side, and the answer is a SET of neighbours
            // (`ui::road_crossing_targets` -> `placement_rules::can_place_road`).
            // The press arms; the targets highlight; the second press lays one
            // crossing. The refusal is therefore the ABSENCE of a target, which is
            // why there is no refusal sentence in this row any more.
            const std::vector<road_target> targets =
                road_crossing_targets(w, tile_id, rt.level, actor); // BL-1406: as the nation

            // Same affordability defect as the building path: place_road gates on
            // build_cost PLUS materials priced at the local market, so gating on
            // build_cost alone enabled a Build that then failed.
            float road_capex = re.build_cost;
            for (std::size_t i = 0; i < resource_count; ++i)
                if (re.resource_build_cost[i] > 0.0f)
                    road_capex += re.resource_build_cost[i] * price(i);
            const bool affordable = balance >= road_capex;

            // Auto-height: a refusal wraps onto a second line rather than clip.
            ImGui::BeginChild("road##build", {0.0f, 0.0f},
                              ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
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

            // THE PRESS ARMS. Always the same control in the same place: enabled
            // when the rules admit a crossing and the balance covers it, disabled
            // (never swapped for a sentence) when they do not. Pressing it while
            // this tier is already armed DISARMS it — the armed state is visible
            // on the canvas, so the standing Toggle rule applies to it.
            const bool armed    = (ui.construction.road_arm_tile == tile_id
                                   && ui.construction.road_arm_tier == rt.level);
            // BL-1379: A TILE WITH NOWHERE TO GO MUTES THE PRESS (construction.md
            // § "A tile with nowhere to go mutes the press", Ben 2026-10-18). The
            // first cut left this live on an empty target set, reasoning that
            // arming and lighting nothing said the refusal more quietly than a
            // sentence does; Ben overruled it, and he is right — a press that arms
            // and then does nothing teaches the player nothing about WHY, where a
            // dead control that explains itself does. So the empty set mutes the
            // press by the SAME mechanism affordability already used, and the
            // reason lives in this column's own explainer and nowhere else (never
            // back as a row of red text, which is what the grid moved away from).
            const bool can_arm  = affordable && !targets.empty();
            ImGui::BeginDisabled(!can_arm && !armed);
            const ImVec2 press_p = ImGui::GetCursorScreenPos();
            if (ImGui::Button(armed ? "Cancel" : "Build"))
            {
                if (armed)
                {
                    ui.construction.road_arm_tile = null_entity;
                }
                else
                {
                    ui.construction.road_arm_tile = tile_id;
                    ui.construction.road_arm_tier = rt.level;
                }
            }
            ImGui::EndDisabled();
            // The press's own rect, read while the button is still the last item
            // (EndGroup below replaces it with the group's).
            const ImVec2 press_sz  = ImGui::GetItemRectSize();
            const bool   press_vis = ImGui::IsItemVisible();
            // ONE SHORT EXPLAINER, ON THE PRESS, per construction.md's
            // per-column rule: what the two presses are, or which one thing is
            // missing. THE EXPLAINER IS THE ONLY PLACE THE REASON LIVES — the
            // muted press is silent on its own, so this column has to carry the
            // whole of why, and the three muted cases must not share a phrase:
            // a ground refusal does not read like a money one, and the player can
            // act on each differently.
            std::string why;
            // GROUND BEFORE MONEY. Where both refuse, the one that reads is the
            // one the player cannot fix by waiting: no funding ever opens a side
            // the rules will not cross.
            if (targets.empty() && !tile_carries_road)
                // BL-1348 (LOGISTICS.md § 4): a road is laid FROM a road. This tile carries
                // none, so there is nothing here to build out of -- a different refusal
                // from "nowhere left to go", and one the player can act on by bringing a
                // road to a neighbour first.
                why = "A road is laid from a road: this tile carries none, so there is "
                      "nowhere to cross from. Bring one to a neighbour first.";
            else if (targets.empty())
            {
                // BL-1379: the press is dead, not armed-and-silent, so the sentence says
                // what the ground refuses rather than what a press would show.
                //
                // BL-1385 (Ben, in play 2026-10-18): and it says WHICH ground rule
                // refused, asked of the rules rather than assumed. This sentence used to
                // be one sentence for every case - "every side is either already roaded
                // to this standard or one no crossing may take" - and it was simply not
                // true of the commonest case: a tile ANOTHER NATION holds, which a road
                // may cross into but never be built out of (BL-1357). A player reading it
                // would upgrade the tier, or look for an unroaded side, and neither can
                // ever work there.
                switch (ui::road_crossing_refusal(w, tile_id, rt.level, actor))
                {
                    case placement_rules::placement_reason::foreign_ground:
                        // BL-1406: the rule refuses ground that is not the player's
                        // nation's - ANOTHER nation's, or ground no nation holds
                        // (BL-1399, NATIONS.md § Own ground) - so the sentence says
                        // which, rather than naming a nation that is not there.
                        why = (w.tile_to_nation.count(tile_id) != 0)
                            ? "Another nation holds this tile. A road may cross INTO their "
                              "ground, but never be built out of it - so build from your "
                              "own side of the border."
                            : "No nation holds this tile. A road may cross INTO unowned "
                              "ground, but never be built out of it - so build from your "
                              "own ground.";
                        break;
                    case placement_rules::placement_reason::no_road_to_extend:
                        why = "A road is laid from a road: this tile carries none, so there "
                              "is nowhere to cross from. Bring one to a neighbour first.";
                        break;
                    case placement_rules::placement_reason::ocean:
                        why = "No side of this tile is a crossing: each one either has no "
                              "neighbour or is a side no cost walk can take.";
                        break;
                    default:
                        why = "Nowhere left to cross to from this tile at this tier: every "
                              "crossable side already stands at this standard or better. A "
                              "higher tier would raise them.";
                        break;
                }
            }
            else if (!affordable)
                why = "Can't afford it: the crossing charges "
                    + std::to_string(static_cast<int>(road_capex)) + " cr.";
            else if (armed)
                why = "Armed: press one of the highlighted tiles to lay the crossing to it. "
                      "A press anywhere else cancels.";
            else
                why = "Press, then press a highlighted neighbour - a road crosses between two "
                      "tiles, so it has to name which one.";
            if (construction_row_hovered(press_p,
                                         {press_p.x + press_sz.x, press_p.y + press_sz.y}))
                construction_row_card(rt.name, [&]() {
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextDisabled("%s", why.c_str());
                    ImGui::PopTextWrapPos();
                });
            ImGui::EndGroup();

            {
                construction_controls::road_row pub;
                pub.tier      = rt.level;
                pub.name      = rt.name;
                pub.cost      = road_capex;
                pub.enabled   = can_arm;
                pub.targets   = static_cast<int>(targets.size());
                pub.explainer = why;
                if (press_vis)
                {
                    pub.x = press_p.x + press_sz.x * 0.5f;
                    pub.y = press_p.y + press_sz.y * 0.5f;
                }
                ui.construction_ui.road_rows.push_back(std::move(pub));
            }

            ImGui::EndChild();
            ImGui::Spacing();
            ImGui::PopID();
        }
        // BL-1255: rail track is the road ladder's state-industry rung. BL-1366:
        // it is a GRID row even here, one line in the shared columns, so the
        // ladder's three boxes are the only tall rows left on the surface.
        draw_rail_rows(w, reg, ui, tile_id, row_h, rail_row::track);
    }

    ImGui::EndChild();
}

} // namespace ui
