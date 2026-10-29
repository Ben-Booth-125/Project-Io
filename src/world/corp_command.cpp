#include "corp_command.hpp"
#include "logistic_points.hpp" // BL-1309; BL-1405 retired its one use (lp_share_rule_valid)

#include "battle_system.hpp" // request_withdraw, unit_in_battle (BL-467)

#include "condition_set.hpp" // BL-350: request_quote's embargo decline condition
#include "construction.hpp"
#include "corporation_generation.hpp" // BL-1154: move_seat_force (take_seat)
#include "economy_system.hpp" // BL-430: try_switch_recipe, the shared recipe-switch gate
#include "fronts.hpp" // BL-1427: assign_front / set_front_order
#include "nation_tech.hpp" // BL-1276: the research-queue verbs
#include "logistics.hpp" // invalidate_logistics_caches (idle/resume flips the anchor set); intra_body_path (BL-470)
// BL-470 included placement_rules.hpp here for is_water_tile, march_unit's old
// destination check. BL-511 retargeted the verb to a province, and while the
// partition was LAND-ONLY the water test had no remaining caller. BL-516 draws
// provinces over the water, so the check is back — but it asks the PROVINCE its
// domain (`province_kind_of`, province.hpp) rather than asking a tile its
// substrate, so placement_rules is still not a dependency of this file.
#include "province.hpp"       // BL-511: march_unit's destination is a province
#include "recipe_registry.hpp"
#include "state_industry.hpp" // BL-1296: state_runs (the state build/retool doors)
#include "stance.hpp" // BL-448: corp stance verbs (declare_hostile / offer_friendship / accept_friendship / return_to_neutral)
#include "supply_system.hpp"
#include "trade.hpp" // BL-1266: trade_is_valid (set_trade)
#include "survey_system.hpp"
#include "unit_roster.hpp"
#include "unit_designs.hpp" // BL-1434: the live design a hire and a refit read
#include "air_combat.hpp"   // BL-1449: hire_wing / assign_air_mission / set_air_design
#include "world.hpp"

#include <algorithm>
#include <cassert> // BL-388: build-seam recipe-forwarding assertion
#include <cmath>
#include <cstddef> // BL-628: std::ptrdiff_t in the dissolution walk's vector erases
#include <iterator> // BL-628: std::next, over the erase-while-walking sets
#include <limits>  // BL-628: the HQ recompute's nearest-centroid search
#include <set>     // BL-628: the stance tables the dissolution walk drops from
#include <utility> // BL-628: std::pair / std::make_pair over the (corp, body) keys
#include <vector>

namespace {


// ---------------------------------------------------------------------------
// hire_unit's cost debit (BL-324)
//
// The campaign roster gate (unit_roster.hpp) asks a yes/no question: does the
// corp hold ANY of the resource an axis needs, summed across its (corp, market)
// pools — the live L3 store (world.hpp § corp_market_pools).
// The debit below spends a flat, first-cut draw (hire_axis_cost) from that
// same resource preference order — not final balance, just enough that
// hiring is a real spend rather than a free unlock. Two-phase (check every
// axis affordable, THEN debit) so a mid-hire failure never leaves the corp
// partially charged.
//
// BL-394: this leg alone was NOT enough — an ungated row (all-zero gate,
// e.g. row 0) tested and debited nothing, so the cheapest units were free
// and unbounded for any seam caller. A credit cost now sits alongside it,
// computed in the verb from economy.lua's military table (recipe_registry's
// military_capability_params) and debited from the corp's balance.
// ---------------------------------------------------------------------------

/// The resource an axis actually spends: the first candidate on the SHARED
/// table (unit_roster.hpp § hire_axis_table) the corp holds enough of. The
/// gate reads the same table, so a hire can never spend a resource the gate
/// didn't already confirm access to — BL-498 made that structural rather than
/// a comment asking two hand-written lists to stay in step.
resource_type hire_axis_resource(const world& w, entity_id corp, hire_axis axis)
{
    const hire_axis_entry& e = hire_axis_resources(axis);
    for (const resource_type r : e)
        if (corp_stockpile_total(w, corp, r) >= hire_axis_cost)
            return r;
    return e.candidates[0]; // unreachable if affordability already passed; a safe fallback.
}

/// Can @p corp cover one axis's hire cost from a SINGLE candidate resource?
bool corp_can_afford_axis(const world& w, entity_id corp, hire_axis axis)
{
    for (const resource_type r : hire_axis_resources(axis))
        if (corp_stockpile_total(w, corp, r) >= hire_axis_cost)
            return true;
    return false;
}

/// BUY @p amount of @p res for @p corp's hire off the shelves of the markets it
/// sits in, lowest market id first, at each shelf's posted price (BL-1265,
/// MARKETS.md § The shelf economy: a hire buys its kit like any buyer). Only
/// called after affordability is confirmed against the same shelves
/// (`corp_shelf_stock`). The fair-price ceiling is off for a hire's draw (0):
/// the gate counted the whole shelf, so the debit must be able to take it.
void debit_from_corp(world& w, entity_id corp, resource_type res, float amount)
{
    buy_from_corp_shelves(w, corp, static_cast<std::size_t>(res), amount, 0.0f);
}

/// Debit the roster row's gated axes from the corp's pools. All-or-nothing:
/// checks every gated axis is affordable before debiting any of them.
bool debit_hire_cost(world& w, entity_id corp, const roster_row& row)
{
    // Walk the axes in table order — ore, farm, energy — checking every gated
    // one is affordable BEFORE debiting any, so a part-paid hire is impossible.
    // port_q is a building check (corp_owns_port), not a resource: it is not on
    // the table and there is nothing to debit for it.
    for (std::size_t i = 0; i < hire_axis_count; ++i)
    {
        const hire_axis axis = static_cast<hire_axis>(i);
        if (gate_axis_q(row.gate, axis) > 0 && !corp_can_afford_axis(w, corp, axis))
            return false;
    }

    for (std::size_t i = 0; i < hire_axis_count; ++i)
    {
        const hire_axis axis = static_cast<hire_axis>(i);
        if (gate_axis_q(row.gate, axis) > 0)
            debit_from_corp(w, corp, hire_axis_resource(w, corp, axis), hire_axis_cost);
    }
    return true;
}

/// BL-1434 (unit designs): can @p nation buy @p row's gated goods (when @p row is
/// named) AND the kit goods @p kit, together? The row's axes are priced at the
/// resource `debit_hire_cost` would spend, so a resource both legs draw is counted
/// once, as the sum. With no kit this is exactly debit_hire_cost's own test.
bool hire_goods_affordable(const world& w, entity_id nation, const roster_row* row,
                           const std::array<float, resource_count>& kit)
{
    std::array<float, resource_count> need = kit;
    if (row != nullptr)
        for (std::size_t i = 0; i < hire_axis_count; ++i)
        {
            const hire_axis axis = static_cast<hire_axis>(i);
            if (gate_axis_q(row->gate, axis) <= 0)
                continue;
            if (!corp_can_afford_axis(w, nation, axis))
                return false;
            need[static_cast<std::size_t>(hire_axis_resource(w, nation, axis))] += hire_axis_cost;
        }
    for (std::size_t r = 0; r < resource_count; ++r)
        if (need[r] > 0.0f && corp_stockpile_total(w, nation, static_cast<resource_type>(r)) < need[r])
            return false;
    return true;
}

/// BL-1434: buy the kit goods @p kit off @p nation's shelves, ascending resource.
/// Only called once `hire_goods_affordable` has passed.
void debit_kit_goods(world& w, entity_id nation, const std::array<float, resource_count>& kit)
{
    for (std::size_t r = 0; r < resource_count; ++r)
        if (kit[r] > 0.0f)
            debit_from_corp(w, nation, static_cast<resource_type>(r), kit[r]);
}

// hire_batch_manpower lives in unit_roster.hpp (BL-1435: the muster pass raises the unit).

// BL-1407: the procurement helpers (BL-350 / BL-392: supplier capacity,
// throughput and input access, the buyer's delivery body) and the whole-firm
// buyout's dissolution walk (BL-628) are DELETED with the corporation: named-
// supplier procurement and the buyout are retired (CONTRACTS.md, FINANCE.md).


corp_command_result map_construction(construction_result r)
{
    switch (r)
    {
        case construction_result::placed:                 return corp_command_result::applied;
        case construction_result::insufficient_funds:     return corp_command_result::rejected_funds;
        case construction_result::no_corp:                return corp_command_result::rejected_no_corp;
        case construction_result::no_tile:                return corp_command_result::rejected_invalid;
        case construction_result::tech_locked:            return corp_command_result::rejected_tech_locked;
        case construction_result::era_locked:             return corp_command_result::rejected_era_locked;
        case construction_result::wrong_air:              return corp_command_result::rejected_wrong_air;
        // BL-1347 / BL-1348 / BL-1357: a road refusal IS a placement refusal at this
        // seam -- the agent-facing enum keeps its existing vocabulary, and the specific
        // reason reaches the player's surface through `construction_result` itself.
        case construction_result::needs_road:
        case construction_result::foreign_ground:
        case construction_result::site_busy: // one site per tile (Ben, 2026-10-26)
        case construction_result::invalid_tile:
        case construction_result::out_of_range:
        case construction_result::slot_occupied:
        case construction_result::insufficient_materials: return corp_command_result::rejected_placement;
    }
    return corp_command_result::rejected_invalid;
}

/// BL-1402 (NATIONS.md § What a nation owns): the verbs a NATION ACTING AS
/// ITSELF (`cmd.corp == cmd.as_nation`, the nation's own id) takes through the
/// SHARED verb bodies of `apply_corp_command` rather than the state-industry
/// door — every one of them already keyed on "the actor" and made owner-generic
/// here (the purse, the asset list, the home nation), so after the fold a
/// nation's force, roads, trades and research are the same code a
/// corporation's were. Each body validates everything before it mutates, so a
/// refusal mutates nothing, as on the corporation path. A champion's seat acting
/// FOR a nation (`cmd.corp` a corporation) still takes the state door, which
/// refuses these verbs as before.
bool nation_self_shared_verb(corp_verb v)
{
    switch (v)
    {
        case corp_verb::hire_unit:
        case corp_verb::march_unit:
        case corp_verb::halt_unit: // march's stop verb: a nation that may march may halt
        case corp_verb::refit_unit: // Ben, 2026-10-27: a nation refits its own units
        case corp_verb::assign_front:    // BL-1427: a nation's own units and fronts
        case corp_verb::set_front_order: // BL-1427
        case corp_verb::hire_wing:          // BL-1449: a nation's own airbase
        case corp_verb::assign_air_mission: // BL-1449: a nation's own wing
        case corp_verb::place_road:
        case corp_verb::set_trade:
        case corp_verb::clear_trade:
        case corp_verb::set_trade_reserve:
        case corp_verb::queue_research:
        case corp_verb::unqueue_research:
        case corp_verb::clear_research:
        // BL-1407: the player IS its nation, so every verb the player sends is
        // sent as the nation. Each body below is keyed on the actor alone.
        case corp_verb::survey:
        case corp_verb::hold_convoy:
        case corp_verb::disband_unit:
        case corp_verb::withdraw_from_battle:
        case corp_verb::raze_centre:
        case corp_verb::take_seat:
        // BL-1407 (RELATIONS.md § Stance: stance is between nations): the four
        // stance verbs act for a nation; no scorer issues them.
        case corp_verb::declare_hostile:
        case corp_verb::offer_friendship:
        case corp_verb::accept_friendship:
        case corp_verb::return_to_neutral:
            return true;
        default:
            return false;
    }
}

/// True iff @p nation owns @p building as state industry.
bool nation_owns(const world& w, entity_id nation, entity_id building)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return false;
    const auto& a = nit->second.state_assets;
    return std::find(a.begin(), a.end(), building) != a.end();
}

/// BL-1296 — a command acting for a nation's STATE INDUSTRY (`cmd.as_nation`;
/// NATIONS.md § State industry, "The player's controls": "a corporation's
/// building verbs — build, recipe, workforce dial, sell — paid from the
/// treasury"). `cmd.corp` exists (the caller checked). Every check is made
/// before anything mutates, so a rejection mutates nothing:
///   - the nation exists and is a MAJOR, and `cmd.corp` is its CHAMPION — a
///     nation's state industry is operated from its champion's seat (the
///     wire's actor gate has already pinned `cmd.corp` to the session's corp);
///   - the verb is one of a corporation's building verbs ("sell" is
///     `demolish`: the prototype has no salvage and no privatisation, so
///     disposing of a building is the one sale there is);
///   - a named building exists and is the nation's own.
corp_command_result apply_state_command(world& w, const recipe_registry& reg,
                                        const corp_command& cmd, entity_id* out_building)
{
    // BL-1405 (LOGISTICS.md § Logistic Points, "One pool"; Ben, 2026-10-23): the
    // share rule RETIRES. After the fold every building on a nation's ground is the
    // nation's, so its pool serves ONE class and there is nothing to share among.
    // The verb (BL-1309's) is refused whole, from every actor, before anything is
    // read or mutated. `nation_component::lp_shares` stays in the struct and the
    // save stream so old saves load; the resolver ignores it.
    if (cmd.verb == corp_verb::set_logistics_shares)
        return corp_command_result::rejected_invalid;
    const entity_id nation = cmd.as_nation;
    if (w.nations.find(nation) == w.nations.end())
        return corp_command_result::rejected_invalid;
    // BL-1399 / BL-1407: the nation acts for itself (cmd.corp == the nation); the
    // champion's seat retired with the champion.
    if (cmd.corp != nation)
        return corp_command_result::rejected_not_owner;
    // The SEAT: the player acting as its own nation. Every seat-only verb (rail,
    // the tax tier) and every seat refusal (BL-1322, the workforce lock) keys on
    // this.
    const bool seat = nation == player_nation(w);

    // BL-1255 (LOGISTICS.md § 4c): the railway verbs — a station, a tile of track —
    // are the PLAYER NATION's (Ben, 2026-10-13: "the player builds stations and lays
    // track in play as its nation's verbs"). No grant lets a rival build rail
    // (AI_OPPONENT.md § 11), so the seat alone may issue them: a rival champion's
    // scored layer, or a wire agent acting as one, is refused here. Checked before
    // anything mutates.
    const bool rail_verb = cmd.verb == corp_verb::lay_track
        || (cmd.verb == corp_verb::build && cmd.type == building_type::railway_station);
    if (rail_verb && !seat)
        return corp_command_result::rejected_not_owner;

    // BL-1335 (NATIONS.md § Taxes and Discontent): the nation's tax tier. The
    // seat's alone in play — an AI nation's tier is its scorer's
    // (run_nation_tax_ai), which writes through the same door,
    // set_nation_tax_tier. `order` is the tier, validated as the value that
    // lands (1..5) before anything mutates; out of range is rejected whole.
    // BL-1437 (MILITARY.md § Land — the classes): the seat marks a roster row
    // obsolete or live. Validated as the value that lands before anything mutates;
    // a no-op is refused (rejected_state) so a repeated press changes nothing.
    if (cmd.verb == corp_verb::set_unit_obsolete)
    {
        if (!seat)
            return corp_command_result::rejected_not_owner;
        if (cmd.unit_type >= unit_roster_table().size() || cmd.order > 1u)
            return corp_command_result::rejected_invalid;
        const bool want = (cmd.order == 1u);
        if (roster_row_obsolete(w, nation, cmd.unit_type) == want)
            return corp_command_result::rejected_state;
        const auto nit = w.nations.find(nation);
        if (nit == w.nations.end())
            return corp_command_result::rejected_invalid;
        std::vector<uint16_t>& flips = nit->second.roster_obsolete_flips;
        const auto at = std::lower_bound(flips.begin(), flips.end(), cmd.unit_type);
        if (at != flips.end() && *at == cmd.unit_type)
            flips.erase(at);          // back to the default
        else
            flips.insert(at, cmd.unit_type);
        return corp_command_result::applied;
    }

    // BL-1434 (unit designs; MILITARY.md § The kit): the seat fits one kit item into
    // one slot of its live design for one class. Every check first
    // (`unit_design_check`), so a refusal mutates nothing; a slot going back to its
    // base kit erases the slot's entry (the default is stored as nothing).
    if (cmd.verb == corp_verb::set_unit_design)
    {
        const corp_command_result r =
            unit_design_check(w, nation, cmd.order, cmd.recipe, cmd.unit_type);
        if (r != corp_command_result::applied)
            return r;
        const std::vector<kit_item>& cat = kit_catalogue();
        const kit_item& it = cat[cmd.unit_type];
        std::vector<uint16_t>& v = w.nations.at(nation).unit_design_kit;
        v.erase(std::remove_if(v.begin(), v.end(),
                               [&](uint16_t id) {
                                   return id < cat.size() && cat[id].cls == it.cls
                                       && cat[id].slot == it.slot;
                               }),
                v.end());
        if (kit_variant_of(cmd.unit_type) != 0)
            v.insert(std::lower_bound(v.begin(), v.end(), cmd.unit_type), cmd.unit_type);
        return corp_command_result::applied;
    }

    // BL-1449 (MILITARY.md § Air): the seat fits one air kit item into one slot of its
    // live air design, as set_unit_design does for a land class. Every check first.
    if (cmd.verb == corp_verb::set_air_design)
    {
        const corp_command_result r =
            air_design_check(w, nation, cmd.order, cmd.recipe, cmd.unit_type);
        if (r != corp_command_result::applied)
            return r;
        const std::vector<air_kit_item>& cat = air_kit_catalogue();
        const air_kit_item& it = cat[cmd.unit_type];
        std::vector<uint16_t>& v = w.nations.at(nation).air_design_kit;
        v.erase(std::remove_if(v.begin(), v.end(),
                               [&](uint16_t id) {
                                   return id < cat.size() && cat[id].cls == it.cls
                                       && cat[id].slot == it.slot;
                               }),
                v.end());
        if (air_kit_variant_of(cmd.unit_type) != 0)
            v.insert(std::lower_bound(v.begin(), v.end(), cmd.unit_type), cmd.unit_type);
        return corp_command_result::applied;
    }

    if (cmd.verb == corp_verb::set_tax_tier)
    {
        if (!seat)
            return corp_command_result::rejected_not_owner;
        if (cmd.order < static_cast<uint32_t>(k_tax_tier_min)
            || cmd.order > static_cast<uint32_t>(k_tax_tier_max))
            return corp_command_result::rejected_invalid;
        return set_nation_tax_tier(w, nation, static_cast<int>(cmd.order))
                   ? corp_command_result::applied
                   : corp_command_result::rejected_invalid;
    }

    switch (cmd.verb)
    {
        // BL-1402 (CONTRACTS.md: named-supplier procurement is RETIRED): there is
        // no nation contract form, so a nation is refused every procurement verb,
        // whole, mutating nothing — named here so the refusal is a decision, not
        // the default's accident.
        case corp_verb::request_quote:
        case corp_verb::accept_quote:
        case corp_verb::cancel_contract:
            return corp_command_result::rejected_invalid;
        case corp_verb::lay_track:
        {
            if (w.tiles.find(cmd.tile) == w.tiles.end())
                return corp_command_result::rejected_invalid;
            return map_construction(lay_track(w, reg, nation, cmd.tile));
        }
        case corp_verb::place_road:
            // BL-1401 (AI_OPPONENT.md § 11, Ben 2026-10-23): a nation lays a road for
            // itself — the corporation's verb, issued by and for the nation, paid from its
            // treasury. place_road's own crossing rules ask the nation's ground (one
            // crossing into foreign ground, none from it).
            return map_construction(place_road(w, reg, nation, cmd.tile, cmd.road_tier,
                                               cmd.road_side));
        case corp_verb::build:
        {
            if (cmd.type == building_type::railway_station
                || cmd.type == building_type::inland_logistics_hub) // BL-1309: a nation builds hubs
            {
                if (cmd.recipe != no_recipe || w.tiles.find(cmd.tile) == w.tiles.end())
                    return corp_command_result::rejected_invalid;
                entity_id built = null_entity;
                const construction_result r = cmd.type == building_type::railway_station
                    ? construct_state_station(w, reg, nation, cmd.tile, built)
                    : construct_state_logistics(w, reg, nation, cmd.tile, cmd.type, built);
                if (r == construction_result::placed && out_building)
                    *out_building = built;
                return map_construction(r);
            }
            // BL-1399 (Ben, 2026-10-23; NATIONS.md § What a nation owns): the
            // nation builds ANY type in ANY sector for itself - the strategic-sector
            // limit retired with the state/private split. A stated recipe must name
            // a processor and reach it, as on the corporation path (BL-388).
            if (cmd.recipe != no_recipe
                && (cmd.type != building_type::processing_facility
                    || reg.get_recipe(cmd.recipe) == nullptr))
                return corp_command_result::rejected_invalid;
            if (w.tiles.find(cmd.tile) == w.tiles.end())
                return corp_command_result::rejected_invalid;
            entity_id built = null_entity;
            const construction_result r =
                construct_nation(w, reg, nation, cmd.tile, cmd.type, cmd.target, cmd.recipe, built);
            if (r == construction_result::placed && out_building)
                *out_building = built;
            return map_construction(r);
        }
        case corp_verb::demolish:
        case corp_verb::set_recipe:
        case corp_verb::set_workforce:
        case corp_verb::set_workforce_auto:
        case corp_verb::idle:
        case corp_verb::resume:
            break;
        default:
            return corp_command_result::rejected_invalid; // not a building verb
    }

    const auto bit = w.buildings.find(cmd.subject);
    if (bit == w.buildings.end())
        return corp_command_result::rejected_invalid;
    if (!nation_owns(w, nation, cmd.subject))
        return corp_command_result::rejected_not_owner;
    building_component& b = bit->second;

    switch (cmd.verb)
    {
        case corp_verb::demolish:
            return demolish_state_building(w, nation, cmd.subject)
                       ? corp_command_result::applied
                       : corp_command_result::rejected_invalid;
        case corp_verb::set_recipe:
        {
            if (b.type != building_type::processing_facility)
                return corp_command_result::rejected_invalid;
            if (reg.get_recipe(cmd.recipe) == nullptr)
                return corp_command_result::rejected_invalid;
            if (b.recipe == cmd.recipe)
                return corp_command_result::rejected_state;
            // The shared retool gate, with the NATION as the payer (cost from
            // the treasury's uncommitted part; the recipe must stay strategic).
            switch (try_switch_recipe(w, reg, nation, b, cmd.recipe))
            {
                case recipe_switch_result::applied:            return corp_command_result::applied;
                case recipe_switch_result::on_cooldown:         return corp_command_result::rejected_cooldown;
                case recipe_switch_result::insufficient_funds:  return corp_command_result::rejected_funds;
                case recipe_switch_result::invalid:             return corp_command_result::rejected_invalid;
                case recipe_switch_result::cross_group:         return corp_command_result::rejected_invalid;
                case recipe_switch_result::tech_locked:         return corp_command_result::rejected_tech_locked;
                case recipe_switch_result::wrong_air:           return corp_command_result::rejected_wrong_air;
            }
            return corp_command_result::rejected_invalid;
        }
        case corp_verb::set_workforce:
        {
            // BL-1322: the player's state buildings are stepped by the game like
            // its own (economy_system Pass 3); the player has no workforce verb.
            if (seat)
                return corp_command_result::rejected_invalid;
            if (cmd.workforce < 0 || cmd.workforce > 200)
                return corp_command_result::rejected_invalid;
            // Infrastructure staffs to full and stays there (Ben, 2026-10-26;
            // PRODUCTION.md § Labour demand): no dial moves it off 200.
            if (is_infrastructure(b.type))
                return corp_command_result::rejected_invalid;
            if (b.workforce_target == cmd.workforce && !b.workforce_auto)
                return corp_command_result::rejected_state;
            // BL-1296 fix round: a raised target bills more this tick — it
            // must fit the uncommitted treasury, and is committed when it does.
            building_component after = b;
            after.workforce_target = cmd.workforce;
            if (!state_commit_change(w, reg, nation, b, after))
                return corp_command_result::rejected_funds;
            b.workforce_target = cmd.workforce;
            b.workforce_auto   = false;
            return corp_command_result::applied;
        }
        case corp_verb::set_workforce_auto:
            if (seat)
                return corp_command_result::rejected_invalid; // BL-1322, as above
            if (b.workforce_auto)
                return corp_command_result::rejected_state;
            b.workforce_auto = true;
            return corp_command_result::applied;
        case corp_verb::idle:
        case corp_verb::resume:
        {
            const bool want = (cmd.verb == corp_verb::idle);
            if (b.decommissioned == want)
                return corp_command_result::rejected_state;
            // BL-1296 fix round: a resume bills a running building's costs
            // this tick — funded from the uncommitted treasury, or refused.
            if (!want)
            {
                building_component after = b;
                after.decommissioned = false;
                if (!state_commit_change(w, reg, nation, b, after))
                    return corp_command_result::rejected_funds;
            }
            b.decommissioned = want;

            b.loss_streak    = 0;
            if (building_affects_logistics(b.type))
                invalidate_logistics_caches(w);
            return corp_command_result::applied;
        }
        default:
            break;
    }
    return corp_command_result::rejected_invalid;
}

} // namespace

// ---------------------------------------------------------------------------
// Refit to a newer type (Ben, 2026-10-27; MILITARY.md § The muster interface)
// ---------------------------------------------------------------------------

float refit_credit_cost(const recipe_registry& reg, uint16_t from, uint16_t to)
{
    const auto& table = unit_roster_table();
    if (from >= table.size() || to >= table.size())
        return 0.0f;
    // The hire price is base + per_power x power_mod; the base cancels.
    const float diff = reg.military().hire_cost_per_power
        * static_cast<float>(table[to].power_mod - table[from].power_mod);
    return std::max(0.0f, diff);
}

corp_command_result refit_check(const world& w, const recipe_registry& reg, entity_id nation,
                                entity_id unit, uint16_t target)
{
    const auto uit = w.units.find(unit);
    if (uit == w.units.end())
        return corp_command_result::rejected_invalid;
    const unit_component& u = uit->second;
    if (u.owner != nation)
        return corp_command_result::rejected_not_owner;

    // Compatible: a newer model of the unit's land class that the nation fields
    // NOW — the live gate, re-checked rather than trusting a caller's list.
    const auto& table = unit_roster_table();
    if (target >= table.size())
        return corp_command_result::rejected_invalid;
    // BL-1434: the unit's own targets — its own row too when its design is stale.
    const std::vector<uint16_t> ok =
        refit_targets(w, nation, u, campaign_roster_band_for(reg.era()));
    if (std::find(ok.begin(), ok.end(), target) == ok.end())
        return corp_command_result::rejected_invalid;

    // Re-equipping is done out of contact, once at a time.
    if (unit_in_battle(w, unit) || unit_refitting(u))
        return corp_command_result::rejected_state;

    // On the nation's own ground: the kit is delivered where the state can reach it.
    const auto own = w.tile_to_nation.find(u.position);
    if (own == w.tile_to_nation.end() || own->second != nation)
        return corp_command_result::rejected_placement;

    // Every leg checked before any is taken: the credits; every gated goods axis of
    // the target row, unless the refit keeps the row (a design refit buys no new
    // unit type); and, BL-1434, the kit goods of each slot the live design changes —
    // the goods legs together (`hire_goods_affordable`).
    if (purse_room_of(w, nation) < refit_credit_cost(reg, u.type, target))
        return corp_command_result::rejected_funds;
    const land_class lc = unit_land_class(u);
    const std::array<float, resource_count> kit =
        kit_refit_goods(lc, u.kit, design_of(w, nation, lc));
    if (!hire_goods_affordable(w, nation, target == u.type ? nullptr : &table[target], kit))
        return corp_command_result::rejected_funds;
    return corp_command_result::applied;
}

corp_command_result unit_design_check(const world& w, entity_id nation, uint32_t cls,
                                      uint16_t slot, uint16_t item)
{
    // The seat's alone: no grant lets a rival design (AI_OPPONENT.md § 11).
    if (w.nations.find(nation) == w.nations.end() || nation != player_nation(w))
        return corp_command_result::rejected_not_owner;
    if (cls >= static_cast<uint32_t>(land_class_count))
        return corp_command_result::rejected_invalid;
    const land_class lc = static_cast<land_class>(cls);
    if (kit_slot_count(lc) == 0 || slot >= kit_slot_count(lc))
        return corp_command_result::rejected_invalid;
    const std::vector<kit_item>& cat = kit_catalogue();
    if (item >= cat.size())
        return corp_command_result::rejected_invalid;
    if (cat[item].cls != lc || cat[item].slot != slot)
        return corp_command_result::rejected_invalid; // the item does not fit that slot
    if (!kit_item_researched(w, nation, item))
        return corp_command_result::rejected_tech_locked;
    if (design_fitted(w, nation, lc, slot) == item)
        return corp_command_result::rejected_state; // no change
    return corp_command_result::applied;
}

// BL-1407: the acquisition price (BL-628) and firm exit (BL-743) are DELETED
// with the corporation (FINANCE.md § Whole-firm acquisition, § Firm exit:
// retired). A nation is never bought and never goes insolvent.

corp_command_result march_check(world& w, entity_id actor, entity_id unit, entity_id tile,
                                entity_id assume_hostile, std::vector<entity_id>* out_path,
                                const military_capability_params* mil)
{
    const auto uit = w.units.find(unit);
    if (uit == w.units.end())
        return corp_command_result::rejected_invalid;
    if (uit->second.owner != actor)
        return corp_command_result::rejected_not_owner;

    // BL-467 (2026-08-21) FILLED THE GAP THIS COMMENT RESERVED. The
    // "reject march while in-battle" rule from BL-470's design was
    // unreachable while nothing recorded that a unit was in contact;
    // `world::battles` now does. A unit in a fight may not be given a
    // new movement order — walking away from contact is a WITHDRAWAL,
    // which is priced, not a march, which is not. The verb for it is
    // corp_verb::withdraw_from_battle.
    //
    // Note the state is on the BATTLE, not on unit_component: the flag
    // that comment predicted would have been a second place for the same
    // fact to live, and a second place to forget to clear.
    if (unit_in_battle(w, unit))
        return corp_command_result::rejected_state;
    // A unit refitting to a newer type re-equips in place (Ben, 2026-10-27).
    if (unit_refitting(uit->second))
        return corp_command_result::rejected_state;

    // EVERY MARCH GOES TO A TILE (Ben, 2026-10-27: "all marches go to a
    // tile"). The destination is `tile`, and the march ends on it.
    // `cmd.province` is IGNORED: the province grain BL-511 gave the march is
    // retired, and a command naming only a province names no tile, so it is
    // refused below as an unknown tile. This is the domain check the
    // untrusted-input rule asks for: the wire proves the id fits an
    // entity_id, and only `w.tiles` knows whether it is a tile.
    const auto dit = w.tiles.find(tile);
    if (dit == w.tiles.end())
        return corp_command_result::rejected_invalid;
    const uint32_t dprov = w.provinces.province_of(tile);
    const province* dp   = w.provinces.find(dprov);

    // BL-778: MAY THIS TYPE BE THERE. A unit type declares the domains it
    // may cross (docs/military/MILITARY.md § Domains and traversal). A land
    // type may enter coastal water its OWN polity holds and no other water.
    // The tile's domain is its province's kind; a tile no province holds is
    // read off its substrate (water is the open ocean, at any price).
    {
        traversal_domain dest_domain = traversal_domain::land;
        if (dp != nullptr)
        {
            const province_kind pk = province_kind_of(w, *dp);
            dest_domain = (pk == province_kind::open_ocean)    ? traversal_domain::open_ocean
                        : (pk == province_kind::coastal_water) ? traversal_domain::coastal_water
                                                               : traversal_domain::land;
        }
        else if (is_water(dit->second.substrate))
            dest_domain = traversal_domain::open_ocean;

        const std::vector<roster_row>& rows = unit_roster_table();
        if (uit->second.type >= rows.size())
            return corp_command_result::rejected_invalid; // no such row to ask

        bool owned_by_mover = false;
        if (dest_domain == traversal_domain::coastal_water)
        {
            // BL-1402: owner-generic — a corporation's home nation, or the
            // nation itself when it marches as itself.
            const entity_id home = owner_home_nation(w, actor);
            owned_by_mover = (home != null_entity) && (province_holder_for(w, dprov) == home);
        }
        if (!row_can_traverse(rows[uit->second.type], dest_domain, owned_by_mover))
            return corp_command_result::rejected_invalid;
    }

    const auto sit = w.tiles.find(uit->second.position);
    if (sit == w.tiles.end())
        return corp_command_result::rejected_invalid; // unit's own tile is gone

    if (dit->second.body != sit->second.body)
        return corp_command_result::rejected_invalid; // BL-470's ruling 1: intra-body path march only

    if (uit->second.position == tile)
        return corp_command_result::rejected_state; // already there — halt_unit is the stop verb

    // A SPENT UNIT CANNOT ATTACK (MILITARY.md § Battles: a spent attacker starts
    // nothing). A march whose destination a nation the actor is hostile to holds is
    // an attack, and `open_tile_battle` would refuse it on contact, so the verb
    // refuses it here, honestly, rather than accept an order that can only bump.
    // @p assume_hostile reads one nation as hostile already, for a caller that will
    // declare before it marches (the right-click "declare and attack").
    if (uit->second.readiness_permille <= 0)
        for (const auto& [vid, v] : w.units)
            if (v.count > 0 && v.position == tile && v.owner != uit->second.owner
                && (v.owner == assume_hostile || is_hostile(w, uit->second.owner, v.owner)))
                return corp_command_result::rejected_state;

    // ONE NATION PER TILE (Ben, 2026-10-27): the path is solved around every
    // tile another nation's units stand on, save the destination itself —
    // marching onto a held destination is an attack (toward a nation the
    // actor is hostile to) or a block, decided as the march steps
    // (run_unit_march). Read-only: nothing is written until the single
    // `order` assignment in apply_corp_command, so a refusal mutates nothing.
    //
    // TERRAIN AND THE CLASSES (BL-1455; MILITARY.md § Terrain and the classes): a block
    // is a per-class WALL in the path. The route is solved around it; an order with no
    // legal route because of it is refused with its own reason - a walled destination,
    // or a destination the unit could reach were it not for the walls.
    const std::set<entity_id> foreign = foreign_held_tiles(w, uit->second.owner);
    std::function<bool(const tile_component&)> wall;
    if (mil != nullptr)
    {
        wall = class_wall(*mil, uit->second.type); // empty when the class has no blocking row
        if (wall && wall(dit->second))
            return corp_command_result::rejected_terrain; // the class cannot enter it
    }
    const std::vector<entity_id> path =
        march_path(w, sit->second.body, uit->second.position, tile, foreign,
                   wall ? &wall : nullptr);
    if (path.size() < 2)
    {
        // The second search names the reason, and runs only where walls exist.
        if (wall
            && march_path(w, sit->second.body, uit->second.position, tile, foreign).size() >= 2)
            return corp_command_result::rejected_terrain; // only the walls stand in the way
        return corp_command_result::rejected_invalid; // unreachable
    }
    if (out_path != nullptr)
        *out_path = path;
    return corp_command_result::applied;
}

corp_command_result apply_corp_command(world& w, const recipe_registry& reg,
                                       const corp_command& cmd,
                                       entity_id* out_building)
{
    if (out_building)
        *out_building = null_entity;

    // BL-1407 (Ben, 2026-10-23: no corporation; the player IS its nation): EVERY
    // ACTOR IS A NATION, acting for itself (`corp == as_nation`, or `as_nation`
    // unnamed). A nation never acts for another (the champion's seat retired with
    // the champion), and the three verbs that must NAME the nation (track, the
    // share rule, the tax tier) are refused unnamed, as they always were.
    if (w.nations.find(cmd.corp) == w.nations.end())
        return corp_command_result::rejected_no_corp;
    if (cmd.as_nation != null_entity && cmd.as_nation != cmd.corp)
        return corp_command_result::rejected_not_owner;
    if (cmd.as_nation == null_entity
        && (cmd.verb == corp_verb::lay_track || cmd.verb == corp_verb::set_logistics_shares
            || cmd.verb == corp_verb::set_tax_tier))
        return corp_command_result::rejected_invalid;

    // The nation's own door for its building verbs (and the rail and tax acts);
    // the actor-generic bodies below for everything else (`nation_self_shared_verb`).
    if (!nation_self_shared_verb(cmd.verb))
    {
        corp_command self = cmd;
        self.as_nation    = cmd.corp;
        return apply_state_command(w, reg, self, out_building);
    }

    switch (cmd.verb)
    {
        // BL-1407: the building verbs (build, demolish, recipe, workforce, idle,
        // resume), the rail and tax verbs and the retired share rule take the
        // NATION's door (`apply_state_command`, above); their corporation bodies
        // are deleted with the type.

        case corp_verb::place_road:
            return map_construction(place_road(w, reg, cmd.corp, cmd.tile, cmd.road_tier,
                                              cmd.road_side));

        case corp_verb::survey:
        {
            switch (dispatch_survey(w, cmd.subject, cmd.corp))
            {
                case survey_dispatch_result::success:            return corp_command_result::applied;
                case survey_dispatch_result::insufficient_funds: return corp_command_result::rejected_funds;
                case survey_dispatch_result::already_surveyed:
                case survey_dispatch_result::in_progress:        return corp_command_result::rejected_state;
                case survey_dispatch_result::invalid:            return corp_command_result::rejected_invalid;
            }
            return corp_command_result::rejected_invalid;
        }

        case corp_verb::hire_unit:
        {
            const auto& table = unit_roster_table();
            if (cmd.unit_type >= table.size())
                return corp_command_result::rejected_invalid;
            const roster_row& row = table[cmd.unit_type];

            // Re-check availability against the LIVE gate rather than trusting a
            // caller's cached list — mirrors construct_building's own re-validation
            // of placement rather than the tile the UI last showed as buildable.
            const auto avail = available_rows(w, cmd.corp, campaign_roster_band_for(reg.era()));
            if (std::find(avail.begin(), avail.end(), &row) == avail.end())
                return corp_command_result::rejected_invalid;

            if (w.tiles.find(cmd.tile) == w.tiles.end())
                return corp_command_result::rejected_invalid;

            // BL-325 S2: hire moves onto the base. cmd.tile must carry the corp's
            // own COMPLETED military_base — supersedes hire-anywhere (BL-324). A
            // base under construction (ticks_remaining > 0) or a decommissioned
            // one does not qualify, mirroring the completed-hub idiom
            // (supply_system.cpp's inland_logistics_hub check).
            entity_id muster_base = null_entity;
            {
                // BL-454: the WINNING base id is captured, not just a bool — it
                // becomes the unit's `muster_base`, the key the upkeep pass uses
                // to disband a unit whose base has been demolished. Lowest id
                // wins rather than first-found, because w.buildings is an
                // unordered_map and "first" is not a defined answer.
                // BL-1402: "its own" is owner-generic (`owner_holds`: a
                // corporation's assets or a nation's state_assets).
                for (const auto& [bid, bc] : w.buildings)
                {
                    if (bc.tile == cmd.tile && bc.type == building_type::military_base
                        && bc.ticks_remaining <= 0 && !bc.decommissioned
                        && owner_holds(w, cmd.corp, bid))
                    {
                        if (muster_base == null_entity || bid < muster_base)
                            muster_base = bid;
                    }
                }
                if (muster_base == null_entity)
                    return corp_command_result::rejected_placement;
            }

            // BL-394: the credit cost. Row 0's gate is all-zero, so the
            // resource debit below tested and spent nothing for the cheapest
            // rows — a seam player raised ten free units in one tick (the
            // scorer's one-hire-per-eval cap is corp_ai policy, not a rule of
            // the game). Every row now costs credits: a floor plus a
            // power-scaled component, authored in economy.lua (§ military).
            // Checked BEFORE the resource debit so a refusal on either leg
            // leaves the corp wholly uncharged, preserving debit_hire_cost's
            // own all-or-nothing contract.
            const military_capability_params& mil = reg.military();
            const float hire_cost = mil.hire_base_cost
                + mil.hire_cost_per_power * static_cast<float>(row.power_mod);
            // BL-1402: the payer is owner-generic — a corporation's balance, or
            // a nation's treasury admitted only up to its UNCOMMITTED part
            // (`purse_room_of`): a nation never runs into debt. For a
            // corporation the room IS its balance, the comparison it always made.
            if (purse_room_of(w, cmd.corp) < hire_cost)
                return corp_command_result::rejected_funds;

            // BL-1434 (unit designs): the hire is raised to its class's LIVE design and
            // buys that design's kit goods (none for base kit) beside the row's gated
            // goods — both legs proven affordable together before either is taken.
            const land_class lc      = land_class_for(row);
            const unit_design design = design_of(w, cmd.corp, lc);
            const std::array<float, resource_count> kit = kit_goods(lc, design);
            if (!hire_goods_affordable(w, cmd.corp, &row, kit))
                return corp_command_result::rejected_funds;

            if (!debit_hire_cost(w, cmd.corp, row))
                return corp_command_result::rejected_funds;
            debit_kit_goods(w, cmd.corp, kit);
            *purse_of(w, cmd.corp) -= hire_cost; // the actor exists: proven at the seam's top

            // BL-1435: RAISING TAKES TIME (Ben, 2026-10-27; MILITARY.md § The muster
            // interface). The hire is paid now and QUEUED at the base; the unit
            // appears when its muster completes (advance_musters, economy_system).
            // So there is no unit to hand back: `out_building` is cleared.
            const int days = muster_days_for(cmd.unit_type);
            muster_order m;
            m.id             = w.next_muster_id++;
            m.base           = muster_base;
            m.owner          = cmd.corp;
            m.unit_type      = cmd.unit_type;
            m.days_total     = days;
            m.days_remaining = days;
            m.kit            = design; // BL-1434: recorded by the unit at completion
            w.musters.push_back(m);
            if (out_building)
                *out_building = null_entity;
            return corp_command_result::applied;
        }

        // --- BL-1449: air combat (MILITARY.md § Air) ---------------------------
        case corp_verb::hire_wing:
        {
            // The airbase is the one door: every placement check first, read-only.
            entity_id base = null_entity;
            const corp_command_result r = hire_wing_check(w, cmd.corp, cmd.tile, cmd.unit_type, &base);
            if (r != corp_command_result::applied)
                return r;
            const air_class cls = air_roster_table()[cmd.unit_type].cls;
            const air_class_stats& cs = reg.military().air.cls[static_cast<std::size_t>(cls)];
            // Both legs checked before either is taken: the class's credits, and its
            // hire goods with its live air design's kit goods, off the shelves.
            if (purse_room_of(w, cmd.corp) < cs.hire_cost)
                return corp_command_result::rejected_funds;
            const unit_design design = air_design_of(w, cmd.corp, cls);
            std::array<float, resource_count> goods = air_kit_goods(cls, design);
            for (std::size_t g = 0; g < resource_count; ++g)
                goods[g] += cs.hire_goods[g];
            if (!hire_goods_affordable(w, cmd.corp, nullptr, goods))
                return corp_command_result::rejected_funds;
            debit_kit_goods(w, cmd.corp, goods);
            *purse_of(w, cmd.corp) -= cs.hire_cost;

            const entity_id wid = w.create_entity();
            air_wing wg;
            wg.owner       = cmd.corp;
            wg.airbase     = base;
            wg.type        = cmd.unit_type;
            wg.muster_days = cs.muster_days;
            wg.kit         = design;
            w.wings.emplace(wid, std::move(wg));
            if (out_building)
                *out_building = wid;
            return corp_command_result::applied;
        }

        case corp_verb::assign_air_mission:
        {
            std::vector<uint32_t> set;
            const corp_command_result r =
                air_mission_check(w, reg, cmd.corp, cmd.subject, cmd.order, cmd.air_provinces.data(),
                                  cmd.air_provinces.size(), &set);
            if (r != corp_command_result::applied)
                return r;
            air_wing& wg = w.wings.at(cmd.subject);
            wg.mission   = static_cast<air_mission>(cmd.order);
            wg.provinces = std::move(set);
            return corp_command_result::applied;
        }

        // --- BL-293: the order book -----------------------------------------
        // There is no `place_sell_order(...)` seam elsewhere to delegate to, the
        // way build delegates to construct_building — the order book had no
        // world-side home at all before this item. So the validation the Market
        // Ledger's form performs (a market on the body, a resource that market
        // prices, a positive quantity) is performed HERE, once, and the UI now
        // routes its press through this same path. That is the point: the
        // player's press and the AI's command cannot diverge if there is only one
        // implementation of what the press means.

        // ── TOMBSTONES: THE ORDER BOOK AND THE CORPORATION CONVOY RETIRED ─────
        // (BL-1265 / BL-1266; MARKETS.md § The shelf economy, TRADE.md § What
        // trade replaces, Ben 2026-10-10.) Standing sell orders and the
        // directed convoy went with corporation pools: production sells on
        // landing, and goods move between markets only by a TRADE
        // (`set_trade`). The three slots survive by the append-only rule and
        // are REJECTED, mutating nothing — a caller is told No rather than
        // quietly ignored (the AI-facing-seam rule).
        case corp_verb::place_sell_order:
        case corp_verb::remove_sell_order:
        case corp_verb::dispatch_convoy:
            return corp_command_result::rejected_invalid;

        case corp_verb::request_quote:
        case corp_verb::accept_quote:
        case corp_verb::cancel_contract:
            // BL-1407 (CONTRACTS.md: named-supplier procurement is RETIRED): there
            // is no supplier firm to ask, so the trio is refused whole, from every
            // actor, mutating nothing. The slots stay (append-only enum).
            return corp_command_result::rejected_invalid;

        // ── BL-1266: TRADE JOINS THE SEAM (TRADE.md § A trade, § Auto and
        // reserved trade). The player's, and a rival's under the AI_OPPONENT.md
        // § 11 grant (a rival may set its own trades). Validated as the value
        // that lands — every field range-checked against the real domain, the
        // whole command rejected on any violation, nothing mutated on refusal.
        case corp_verb::set_trade:
        {
            standing_trade t;
            t.owner       = cmd.corp;
            t.resource    = cmd.target;
            t.from_market = cmd.subject;
            t.to_market   = cmd.counterparty;
            t.points      = cmd.quantity;
            // BL-1402: `trade_is_valid` and `price_trade_leg` accept any owner, so
            // a nation acting as itself sets a trade through this same body.
            if (!(t.points <= max_trade_points) || !trade_is_valid(w, reg, t))
                return corp_command_result::rejected_invalid;
            // NR-1018: reach runs from market centre to market centre — a leg
            // must reach the destination (same body, overland or by Ports;
            // another body, the owner's pad and propellant).
            {
                const logistics_nodes nodes = collect_logistics_nodes(w);
                if (!price_trade_leg(w, reg, nodes, cmd.corp, t.from_market, t.to_market,
                                     static_cast<std::size_t>(t.resource), 1.0f).viable)
                    return corp_command_result::rejected_placement;
            }
            if (cmd.order != 0)
            {
                // CHANGE an existing trade, keeping its id and its place in the
                // spending order. Someone else's trade answers exactly as a
                // nonexistent one does (BL-397: no existence oracle).
                const auto it = std::find_if(w.trades.begin(), w.trades.end(),
                                             [&](const standing_trade& x) { return x.id == cmd.order; });
                if (it == w.trades.end() || it->owner != cmd.corp)
                    return corp_command_result::rejected_invalid;
                t.id = it->id;
                *it  = t;
                return corp_command_result::applied;
            }
            std::size_t held = 0;
            for (const standing_trade& x : w.trades)
                if (x.owner == cmd.corp)
                    ++held;
            if (held >= max_trades_per_corp)
                return corp_command_result::rejected_state; // the book of trades is full
            t.id = w.allocate_trade_id();
            w.trades.push_back(t); // appended: placement order is spending order
            return corp_command_result::applied;
        }

        case corp_verb::clear_trade:
        {
            if (cmd.order == 0)
                return corp_command_result::rejected_invalid;
            const auto it = std::find_if(w.trades.begin(), w.trades.end(),
                                         [&](const standing_trade& x) { return x.id == cmd.order; });
            if (it == w.trades.end() || it->owner != cmd.corp)
                return corp_command_result::rejected_invalid; // BL-397: no existence oracle
            w.trades.erase(it); // by id: every surviving trade keeps its handle
            return corp_command_result::applied;
        }

        case corp_verb::set_trade_reserve:
        {
            // The points held back for manual trades; the rest is auto. Clamped
            // to what the corporation makes at use, so any finite amount in
            // range is a legal setting — 0 is "all auto".
            if (!std::isfinite(cmd.quantity) || !(cmd.quantity >= 0.0f)
                || !(cmd.quantity <= max_trade_points))
                return corp_command_result::rejected_invalid;
            // BL-1407: the actor's own reserve — a nation's
            // (`nation_component::trade_reserve`, serialised in the v56 stream)
            // or, while the type survives, a corporation's.
            float* reserve = trade_reserve_of(w, cmd.corp);
            if (reserve == nullptr)
                return corp_command_result::rejected_invalid;
            *reserve = cmd.quantity;
            return corp_command_result::applied;
        }

        case corp_verb::hold_convoy:
        {
            // NOT a cancel, and there is no verb that is one. Cargo left the
            // source pool at dispatch (SUPPLY.md § Convoy entity: "goods in
            // transit are committed"), so a cancel would have to invent a
            // return leg or mint the goods back at the source — neither is in
            // scope. A held convoy stops advancing (advance_convoys skips it)
            // and pays nothing further, because the haul was paid once at
            // dispatch. Issuing the verb again releases it: the toggle rule.
            if (cmd.order == 0)
                return corp_command_result::rejected_invalid;

            const auto it = std::find_if(w.convoys.begin(), w.convoys.end(),
                                         [&](const convoy_component& c) { return c.id == cmd.order; });
            // A convoy that does not exist and one belonging to another corp
            // answer identically, for BL-397's reason: convoy ids are one
            // global monotonic sequence, so a distinguishable rejection would
            // let an id sweep map every rival's cargo in flight — precisely the
            // intelligence the BL-068 visibility rule keeps private.
            if (it == w.convoys.end() || it->corp != cmd.corp)
                return corp_command_result::rejected_invalid;
            if (it->arrived)
                return corp_command_result::rejected_state; // already landed; nothing to hold

            it->held = !it->held;
            return corp_command_result::applied;
        }

        // --- BL-448: corp stance -------------------------------------------
        // Data model + verbs only. `is_hostile`/`are_friends`/etc. and the
        // mutators themselves live in stance.{hpp,cpp}; the seam here is only
        // the untrusted-boundary translation from stance_result to
        // corp_command_result (io-standing-rules.md — reject the whole
        // command on violation, never partial-apply; stance.cpp's mutators
        // already guarantee this on their own, so there is nothing more to
        // validate here beyond passing the right corp on each side).

        case corp_verb::declare_hostile:
        {
            const stance_result r = declare_hostile(w, cmd.corp, cmd.counterparty);
            return (r == stance_result::applied) ? corp_command_result::applied
                 : (r == stance_result::rejected_invalid) ? corp_command_result::rejected_invalid
                                                            : corp_command_result::rejected_state;
        }

        case corp_verb::offer_friendship:
        {
            const stance_result r = offer_friendship(w, cmd.corp, cmd.counterparty);
            return (r == stance_result::applied) ? corp_command_result::applied
                 : (r == stance_result::rejected_invalid) ? corp_command_result::rejected_invalid
                                                            : corp_command_result::rejected_state;
        }

        case corp_verb::accept_friendship:
        {
            // cmd.corp is the ACCEPTOR; cmd.counterparty is the corp whose
            // offer is being accepted — the offer was made counterparty ->
            // corp, so the underlying call takes (offerer, target) as
            // (cmd.counterparty, cmd.corp).
            const stance_result r = accept_friendship(w, cmd.counterparty, cmd.corp);
            return (r == stance_result::applied) ? corp_command_result::applied
                 : (r == stance_result::rejected_invalid) ? corp_command_result::rejected_invalid
                                                            : corp_command_result::rejected_state;
        }

        case corp_verb::return_to_neutral:
        {
            const stance_result r = return_to_neutral(w, cmd.corp, cmd.counterparty);
            return (r == stance_result::applied) ? corp_command_result::applied
                 : (r == stance_result::rejected_invalid) ? corp_command_result::rejected_invalid
                                                            : corp_command_result::rejected_state;
        }

        // --- BL-470: the unit march seam -------------------------------------
        // Extends THIS seam (BL-470's ruling 2) — no parallel unit_command
        // type. A rejection mutates nothing (io-standing-rules.md's untrusted-
        // seam rule): every check below runs before the single `order`/`erase`
        // write that applies the verb.

        case corp_verb::march_unit:
        {
            // Every check first (`march_check`, read-only but for the path cache), so a
            // refusal mutates nothing; the single `order` assignment below applies it.
            std::vector<entity_id> path;
            const corp_command_result r =
                march_check(w, cmd.corp, cmd.subject, cmd.tile, null_entity, &path, &reg.military());
            if (r != corp_command_result::applied)
                return r;
            movement_order mo;
            mo.dest       = cmd.tile;
            mo.path       = path; // travel order: path[0] is the tile the unit occupies
            mo.next_index = 1;
            mo.progress   = 0.0f;
            // BL-1445 (move by days): THE BANK SURVIVES A RE-ISSUE TOWARD THE SAME NEXT
            // TILE. Movement banks day by day in `progress` toward the next hop; a new
            // order whose first hop is the hop the unit was already banking toward keeps
            // that bank, so a slow unit a front re-targets every day or two (push mode)
            // still steps. A new first hop starts from zero, as it always did.
            unit_component& mu = w.units.at(cmd.subject);
            if (mu.order.dest != null_entity && mu.order.next_index < mu.order.path.size()
                && path.size() > 1 && mu.order.path[mu.order.next_index] == path[1]
                && std::isfinite(mu.order.progress) && mu.order.progress > 0.0f)
                mo.progress = mu.order.progress;
            mu.order = mo;
            return corp_command_result::applied;
        }

        case corp_verb::refit_unit:
        {
            // Ben, 2026-10-27 (MILITARY.md § The muster interface, "Refit to a newer
            // type"). Every check first (`refit_check`), so a refusal mutates nothing.
            const corp_command_result r = refit_check(w, reg, cmd.corp, cmd.subject, cmd.unit_type);
            if (r != corp_command_result::applied)
                return r;
            unit_component& u = w.units.at(cmd.subject);
            const float credits = refit_credit_cost(reg, u.type, cmd.unit_type);
            // BL-1434: every refit brings the unit to its class's live design, and buys
            // the kit of each slot that changes.
            const land_class lc    = unit_land_class(u);
            const unit_design live = design_of(w, cmd.corp, lc);
            const std::array<float, resource_count> kit = kit_refit_goods(lc, u.kit, live);
            // The goods leg is the hire's own path; its affordability was proven above.
            // A design refit keeps the row, so it buys no row goods.
            if (cmd.unit_type != u.type
                && !debit_hire_cost(w, cmd.corp, unit_roster_table()[cmd.unit_type]))
                return corp_command_result::rejected_funds;
            debit_kit_goods(w, cmd.corp, kit);
            *purse_of(w, cmd.corp) -= credits; // the actor exists: proven at the seam's top
            u.refit_kit  = live;
            u.refit_type = cmd.unit_type;
            u.refit_days = muster_days_for(cmd.unit_type);
            // Re-equipping is done in place: a standing march order is cleared.
            u.order = movement_order{};
            return corp_command_result::applied;
        }

        case corp_verb::assign_front:
        {
            // BL-1427 (fronts; MILITARY.md § Fronts). Every check first, against the
            // fronts as they stand (`current_fronts`, pure), so a refusal mutates
            // nothing — not even a re-anchor.
            const auto uit = w.units.find(cmd.subject);
            if (uit == w.units.end())
                return corp_command_result::rejected_invalid;
            if (uit->second.owner != cmd.corp)
                return corp_command_result::rejected_not_owner;
            entity_id key = null_entity; // a null tile unassigns
            if (cmd.tile != null_entity)
            {
                const std::vector<front> fr = current_fronts(w, cmd.corp);
                const front* f = front_named(fr, cmd.tile);
                if (f == nullptr)
                    return corp_command_result::rejected_invalid; // no front of the actor's holds it
                key = f->key;
            }
            if (uit->second.front == key)
                return corp_command_result::rejected_state; // already so: nothing to change
            uit->second.front = key;
            return corp_command_result::applied;
        }

        case corp_verb::set_front_order:
        {
            // BL-1427. `order` 0 = hold, 1 = push; push only toward a neighbour the
            // actor is hostile to (the battle trigger's directed test). Hold is the
            // default and is never stored, so setting hold erases the entry.
            if (cmd.order > 1)
                return corp_command_result::rejected_invalid;
            if (cmd.tile == null_entity)
                return corp_command_result::rejected_invalid;
            const std::vector<front> fr = current_fronts(w, cmd.corp);
            const front* f = front_named(fr, cmd.tile);
            if (f == nullptr)
                return corp_command_result::rejected_invalid;
            const front_order want = cmd.order == 1 ? front_order::push : front_order::hold;
            if (want == front_order::push && !front_may_push(w, *f))
                return corp_command_result::rejected_state;
            if (front_order_of(w, cmd.corp, f->key) == want)
                return corp_command_result::rejected_state;
            std::vector<front_order_entry>& v = w.front_orders;
            v.erase(std::remove_if(v.begin(), v.end(),
                                   [&](const front_order_entry& e) {
                                       return e.nation == cmd.corp && e.key == f->key;
                                   }),
                    v.end());
            if (want == front_order::push)
            {
                const front_order_entry e{ cmd.corp, f->key, want };
                const auto at = std::lower_bound(v.begin(), v.end(), e,
                    [](const front_order_entry& x, const front_order_entry& y) {
                        return x.nation != y.nation ? x.nation < y.nation : x.key < y.key;
                    });
                v.insert(at, e);
            }
            return corp_command_result::applied;
        }

        case corp_verb::halt_unit:
        {
            const auto uit = w.units.find(cmd.subject);
            if (uit == w.units.end())
                return corp_command_result::rejected_invalid;
            if (uit->second.owner != cmd.corp)
                return corp_command_result::rejected_not_owner;
            if (uit->second.order.dest == null_entity)
                return corp_command_result::rejected_state; // nothing to halt
            uit->second.order = movement_order{};
            return corp_command_result::applied;
        }

        case corp_verb::disband_unit:
        {
            const auto uit = w.units.find(cmd.subject);
            if (uit == w.units.end())
                return corp_command_result::rejected_invalid;
            if (uit->second.owner != cmd.corp)
                return corp_command_result::rejected_not_owner;
            // The committed-unit lock that used to sit here went with the
            // mercenary contract (NR-731): a unit could not be disbanded out
            // from under a live contract it was committed to. Nothing commits a
            // unit any more, so there is no lock left to honour.
            w.units.erase(uit); // no refund — manpower walks away (BL-470's design)
            w.forget_air_unit(cmd.subject); // BL-1449: no air record names a gone unit
            return corp_command_result::applied;
        }

        case corp_verb::withdraw_from_battle:
        {
            // The battle is named by its DEFENDED TILE (`cmd.tile`, 2026-10-27).
            // request_withdraw resolves against the live battle list and mutates
            // NOTHING when it finds no match — a tile id that merely fits, the
            // null default of an omitted field included, names no battle — so a
            // rejection here leaves the world byte-identical.
            if (!request_withdraw(w, cmd.corp, cmd.tile, cmd.counterparty))
                return corp_command_result::rejected_state;
            return corp_command_result::applied;
        }

        // --- BL-573: the mercenary-contract seam ----------------------------
        // UNTRUSTED INPUT BOUNDARY (io-standing-rules.md). Every field is
        // validated as the value that lands in the destination, before
        // anything is read from it, and the whole command is refused on
        // violation. Nothing below mutates until the single block of writes
        // at the end of each case.

        // TOMBSTONES (BL-693 / NR-731, 2026-08-30). The mercenary contract is
        // gone - its records, its passes and its serialisation with it - but
        // these two enum values CANNOT be removed. `corp_verb` is append-only
        // by contract: deleting a value renumbers every verb below it and
        // silently re-points ACTIONS.json and every recorded command. So the
        // slots stay and the behaviour goes. They reject rather than falling
        // through to a default, so a caller is told No instead of being
        // quietly ignored - the AI-facing-seam rule (io-standing-rules.md).
        case corp_verb::accept_offer:
        case corp_verb::abandon_contract:
            return corp_command_result::rejected_invalid;

        case corp_verb::raze_centre:
        {
            // BL-616/BL-624 (docs/economy/POPULATION.md § Growth, decline and
            // razing): the deliberate destruction act. Passive decline only
            // ever shrinks a centre; razing one is an agent's choice, in
            // occupation, and it should be rare because the occupier almost
            // always prefers to occupy. Since BL-624 (razed settlement tier)
            // it DEMOTES rather than erases — see below.
            const auto cit = w.population_centres.find(cmd.subject);
            if (cit == w.population_centres.end())
                return corp_command_result::rejected_invalid;
            if (cit->second.razed)
                return corp_command_result::rejected_state; // already ash — nothing to raze
            const auto tit = w.population_centre_tile.find(cmd.subject);
            if (tit == w.population_centre_tile.end())
                return corp_command_result::rejected_invalid; // no tile record — corrupt centre
            const auto tc_it = w.tiles.find(tit->second);
            if (tc_it == w.tiles.end())
                return corp_command_result::rejected_invalid;
            const entity_id body = tc_it->second.body;

            // OCCUPATION PRECONDITION (first cut, stated in BL-616's report):
            // the acting corp owns at least one unit whose position tile lies
            // on the centre's body. A unit is the only mobile military presence
            // and is complete from the moment hire_unit raises it (which itself
            // required a COMPLETED military_base), so "completed military
            // presence" reduces to unit-on-body. Pure existence predicate over
            // w.units — order-independent, so the unordered walk is safe.
            bool present = false;
            for (const auto& [uid, uc] : w.units)
            {
                if (uc.owner != cmd.corp)
                    continue;
                const auto ut = w.tiles.find(uc.position);
                if (ut != w.tiles.end() && ut->second.body == body)
                {
                    present = true;
                    break;
                }
            }
            if (!present)
                return corp_command_result::rejected_state; // no occupation, no razing

            // DEMOTE, never erase (BL-624, razed settlement tier; Ben,
            // 2026-08-25: "keep razed settlement as a settlement tier, so
            // it's cheap to rebuild there"). Population zeroed and the razed
            // flag set; the entity, its name, its tile record and its urban
            // land-use stamp (BL-612) all STAY — razed ground reads as
            // historied, the extraction it blocked stays blocked, and the
            // centre still anchors its province (scale set to 1 so the
            // strictly-positive anchor-scale scan still finds it; a ruin can
            // be captured, so razing never deletes a conquest handle).
            // `w.province_holder` is NOT touched here: the holder is live
            // state seeded from the anchor's nation (BL-611), and the
            // conquest consequence of taking an anchor belongs to BL-518,
            // not this verb. The growth pass re-settles a razed centre at a
            // reduced gate (economy_system.cpp § k_resettle_window_ticks).
            cit->second.razed              = true;
            cit->second.population         = 0;
            cit->second.scale              = 1;
            cit->second.growth_accumulator = 0;
            return corp_command_result::applied;
        }

        case corp_verb::buy_corporation:
            // BL-1407 (GLOSSARY.md § Buyout; FINANCE.md § Whole-firm acquisition,
            // retired): no firm remains to buy — ground and what stands on it change
            // hands only by conquest. The slot stays (append-only enum) and the verb
            // is REFUSED whole, from every actor, mutating nothing.
            return corp_command_result::rejected_invalid;

        case corp_verb::take_seat:
        {
            // BL-1076, STARTUP.md § The seat: the player chooses the NATION it
            // is (BL-1407; CONCEPT.md § Player identity). `corp` (already proven
            // to be a nation acting as itself above) is the nation; no other
            // field is read. THE WORLD HALF of the validation — the phase half
            // (pre-play only) is the host's, see the enum's note.
            //
            // A MINOR is never a seat (GLOSSARY.md § Major nation): refused
            // whole, nothing touched. A pick of the nation the generator
            // provisionally named is legitimate and re-points in place.
            if (!is_major_nation(w, cmd.corp))
                return corp_command_result::rejected_invalid;
            const entity_id previous = w.player_entity;
            w.player_entity          = cmd.corp;
            // BL-1206: the clean slate of construction, the same rule as the
            // draw (`seat_clean_slate`), before the force moves.
            seat_clean_slate(w, cmd.corp);
            // BL-1154: the seat opens unarmed, whichever way it is taken — the
            // same rule as the draw (`move_seat_force`).
            move_seat_force(w, previous, cmd.corp);
            // NR-986: the seat's dial-idled plants return to auto, the same
            // rule as the draw (`seat_release_dial_idled`).
            seat_release_dial_idled(w, cmd.corp);
            return corp_command_result::applied;
        }

        case corp_verb::queue_research:
        case corp_verb::unqueue_research:
        case corp_verb::clear_research:
        {
            // BL-1276 (the research panel). The seat only: the player reaches
            // its nation's queue as the nation's operating champion; a rival
            // corporation does not plan its nation's research. Validated in
            // full before any mutation, so a refusal touches nothing.
            // BL-1402: or the seat's NATION acting as itself (cmd.corp == the
            // player's nation) — the same seat, named as its nation. An AI
            // nation acting as itself is refused exactly as a rival corp is.
            // BL-1407: the actor is a nation; only the player's.
            if (cmd.corp != player_nation(w))
                return corp_command_result::rejected_state;
            const entity_id nation = cmd.corp;
            if (w.nations.find(nation) == w.nations.end())
                return corp_command_result::rejected_state;

            if (cmd.verb == corp_verb::queue_research)
            {
                if (cmd.order >= static_cast<uint32_t>(io::dg::node_count))
                    return corp_command_result::rejected_invalid;
                switch (enqueue_node(w, nation, static_cast<int>(cmd.order)))
                {
                    case enqueue_result::queued:         return corp_command_result::applied;
                    case enqueue_result::already_held:
                    case enqueue_result::already_queued:
                    case enqueue_result::refused_pair:   return corp_command_result::rejected_state;
                    case enqueue_result::invalid:        return corp_command_result::rejected_invalid;
                }
                return corp_command_result::rejected_invalid;
            }
            if (cmd.verb == corp_verb::unqueue_research)
            {
                // `order` = the queued node to take off with every later entry
                // that needs it (the toggle, NR-1033), or `unqueue_last` for the
                // last entry. Anything else outside the tree is rejected whole.
                if (cmd.order == unqueue_last)
                    return remove_last_queued(w, nation) ? corp_command_result::applied
                                                         : corp_command_result::rejected_state;
                if (cmd.order >= static_cast<uint32_t>(io::dg::node_count))
                    return corp_command_result::rejected_invalid;
                return unqueue_with_dependents(w, nation, static_cast<int>(cmd.order)) > 0
                           ? corp_command_result::applied
                           : corp_command_result::rejected_state;
            }
            return clear_research_queue(w, nation) ? corp_command_result::applied
                                                   : corp_command_result::rejected_state;
        }
    }
    return corp_command_result::rejected_invalid;
}
