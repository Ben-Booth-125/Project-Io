#include "corp_command.hpp"
#include "logistic_points.hpp" // BL-1309; BL-1405 retired its one use (lp_share_rule_valid)

#include "battle_system.hpp" // request_withdraw, unit_in_battle (BL-467)

#include "condition_set.hpp" // BL-350: request_quote's embargo decline condition
#include "construction.hpp"
#include "corporation_generation.hpp" // BL-1154: move_seat_force (take_seat)
#include "economy_system.hpp" // BL-430: try_switch_recipe, the shared recipe-switch gate
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

/// The batch size one hire_unit command raises — matches the campaign's
/// original Kepler unit stub (hard_coded_world.cpp), which used the same
/// figure before BL-324 gave hiring a real seam.
constexpr int hire_batch_manpower = 50;

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

// BL-1407: the acquisition price (BL-628) and firm exit (BL-743) are DELETED
// with the corporation (FINANCE.md § Whole-firm acquisition, § Firm exit:
// retired). A nation is never bought and never goes insolvent.

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

            if (!debit_hire_cost(w, cmd.corp, row))
                return corp_command_result::rejected_funds;
            *purse_of(w, cmd.corp) -= hire_cost; // the actor exists: proven at the seam's top

            const entity_id unit = w.create_entity();
            // BL-459: no `strength` — it was set to the same value as `count`,
            // a literal duplicate kept in sync by hand. Strength is derived
            // (unit_strength, unit_roster.hpp) from count x the row's quality x
            // the supply factor, so a heavy row is genuinely worth more than a
            // levy at equal headcount and an unsupplied army is measurably
            // weaker in the resolver.
            w.units[unit] = unit_component{
                .position = cmd.tile,
                .owner    = cmd.corp,
                .count    = hire_batch_manpower,
                .type     = cmd.unit_type,
                .supply_factor_permille = 1000, // raised fully supplied (BL-454)
                .muster_base            = muster_base,
                .establishment          = hire_batch_manpower, // BL-1421: the HP bar's full mark
            };
            if (out_building)
                *out_building = unit;
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
            const auto uit = w.units.find(cmd.subject);
            if (uit == w.units.end())
                return corp_command_result::rejected_invalid;
            if (uit->second.owner != cmd.corp)
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
            if (unit_in_battle(w, cmd.subject))
                return corp_command_result::rejected_state;

            // BL-511: the destination is a PROVINCE, not a tile. THIS is the
            // domain check the untrusted-input rule asks for and the one a
            // wire range gate cannot stand in for: `find` binary-searches the
            // built partition, so an id that merely FITS a uint32 — including
            // the 0 default of an omitted field — is refused here rather than
            // being coerced into some nearby province.
            const province* dp = w.provinces.find(cmd.province);
            if (dp == nullptr || dp->tiles.empty())
                return corp_command_result::rejected_invalid;

            // BL-778 TURNED THIS FLAT REFUSAL INTO A DOMAIN QUESTION.
            //
            // BL-516 restored a water test here once sea provinces became
            // addressable, and it read `!= province_kind::land` — units were
            // land-bound, full stop. Under docs/military/MILITARY.md § Domains
            // and traversal a unit TYPE declares which domains it may cross, so
            // the question is no longer "is this water" but "may THIS type be
            // here". Rejection still mutates nothing, as every path around it
            // does.
            //
            // The middle case is the one this exists for: a land type may enter
            // coastal water its OWN polity owns — a shore you hold is shallow,
            // bridged, causewayed — and may not enter anyone else's, nor the
            // open ocean at any price. Ownership is read from the province
            // holder, never from what is built on the water
            // (docs/generation/PROVINCES.md § Who owns water).
            //
            // In practice the campaign refuses every water destination today,
            // because it raises no naval rows and a land row needs owned water
            // — the behaviour is unchanged where nothing has changed. What is
            // different is the REASON, which is now a property of the unit.
            {
                const province_kind pk = province_kind_of(w, *dp);
                const traversal_domain dest_domain =
                      (pk == province_kind::open_ocean)    ? traversal_domain::open_ocean
                    : (pk == province_kind::coastal_water) ? traversal_domain::coastal_water
                                                           : traversal_domain::land;

                const std::vector<roster_row>& rows = unit_roster_table();
                if (uit->second.type >= rows.size())
                    return corp_command_result::rejected_invalid; // no such row to ask

                // "Its own polity": the acting corp's home nation against the
                // nation holding the destination. A corp with no home nation
                // owns no water, which refuses rather than permits.
                bool owned_by_mover = false;
                if (dest_domain == traversal_domain::coastal_water)
                {
                    // BL-1402: owner-generic — a corporation's home nation, or
                    // the nation itself when it marches as itself.
                    const entity_id home = owner_home_nation(w, cmd.corp);
                    owned_by_mover = (home != null_entity)
                                  && (province_holder_for(w, cmd.province) == home);
                }

                if (!row_can_traverse(rows[uit->second.type], dest_domain, owned_by_mover))
                    return corp_command_result::rejected_invalid;
            }

            const auto sit = w.tiles.find(uit->second.position);
            if (sit == w.tiles.end())
                return corp_command_result::rejected_invalid; // unit's own tile is gone

            if (dp->body != sit->second.body)
                return corp_command_result::rejected_invalid; // BL-470's ruling 1: intra-body path march only

            if (w.provinces.province_of(uit->second.position) == cmd.province)
                return corp_command_result::rejected_state; // already there — halt_unit is the stop verb

            // Solve to the LOWEST-ID reachable member tile. `province::tiles`
            // is ascending by contract, so first-reachable-wins is a stable,
            // container-independent choice; and because the march ENDS on
            // entering the province (run_unit_march), which member tile was
            // picked only decides the route's tail, never where the unit
            // stops. Every lookup below is read-only — nothing is written
            // until the single `order` assignment at the end, so a rejection
            // on any path here mutates nothing.
            entity_id       chosen = null_entity;
            std::vector<entity_id> chosen_path;
            for (const entity_id cand : dp->tiles)
            {
                if (cand == uit->second.position)
                    continue; // degenerate; the already-there test above covers the real case
                const logistics_path& p = intra_body_path(w, sit->second.body, uit->second.position, cand);
                if (p.reachable && !p.tiles.empty())
                {
                    chosen      = cand;
                    chosen_path = p.tiles;
                    break;
                }
            }
            if (chosen == null_entity)
                return corp_command_result::rejected_invalid; // no member tile is reachable

            movement_order mo;
            mo.dest          = chosen;
            mo.dest_province = cmd.province;
            mo.path = chosen_path; // canonical (lo->hi) order — see world.hpp's logistics_path comment
            if (mo.path.front() != uit->second.position)
                std::reverse(mo.path.begin(), mo.path.end());
            mo.next_index = 1; // path[0] is the tile the unit already occupies
            mo.progress   = 0.0f;
            uit->second.order = mo;
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
            return corp_command_result::applied;
        }

        case corp_verb::withdraw_from_battle:
        {
            // The DOMAIN CHECK the untrusted-input rule asks for, and the reason
            // it cannot be a wire range gate: `province` is a real id in a built
            // partition, so a value that merely fits a uint32 — including the 0
            // default of an omitted field — must be refused rather than coerced
            // into some nearby province. request_withdraw resolves against the
            // live battle list and mutates NOTHING when it finds no match, so a
            // rejection here leaves the world byte-identical.
            if (!request_withdraw(w, cmd.corp, cmd.province, cmd.counterparty))
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
