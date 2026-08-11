#include "corp_command.hpp"

#include "construction.hpp"
#include "logistics.hpp" // invalidate_logistics_caches (idle/resume flips the anchor set)
#include "recipe_registry.hpp"
#include "survey_system.hpp"
#include "unit_roster.hpp"
#include "world.hpp"

#include <algorithm>

namespace {

/// True iff `corp` owns `building` (the corp's `assets` list is the ownership
/// authority — world.hpp § Ownership accessors).
bool owns(const world& w, entity_id corp, entity_id building)
{
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return false;
    const auto& a = cit->second.assets;
    return std::find(a.begin(), a.end(), building) != a.end();
}

// ---------------------------------------------------------------------------
// hire_unit's cost debit (BL-324)
//
// The campaign roster gate (unit_roster.hpp) asks a yes/no question: does the
// corp hold ANY of the resource an axis needs, summed across its (corp, body)
// pools — the live L3 store (world.hpp § corp_body_pools).
// The debit below spends a flat, first-cut draw (hire_axis_cost) from that
// same resource preference order — not final balance, just enough that
// hiring is a real spend rather than a free unlock. Two-phase (check every
// axis affordable, THEN debit) so a mid-hire failure never leaves the corp
// partially charged.
// ---------------------------------------------------------------------------

constexpr float hire_axis_cost = 5.0f;

/// The resource (in preference order) each gated axis draws from — mirrors
/// unit_roster.cpp's campaign_gate_input exactly, so a hire never spends a
/// resource the gate didn't already confirm access to.
resource_type hire_axis_resource(const world& w, entity_id corp,
                                 std::initializer_list<resource_type> candidates)
{
    for (const resource_type r : candidates)
        if (corp_stockpile_total(w, corp, r) >= hire_axis_cost)
            return r;
    return candidates.begin()[0]; // unreachable if the gate already passed; a safe fallback.
}

/// Debit @p amount of @p res from @p corp's (corp, body) pools, draining in
/// ascending body-id order (the map's own key order, so deterministic). Only
/// called after affordability is confirmed, so this should never under-run —
/// but walks defensively rather than assuming a single pool holds the whole
/// amount.
void debit_from_corp(world& w, entity_id corp, resource_type res, float amount)
{
    for (auto it = w.corp_body_pools.lower_bound({corp, entity_id{0}});
         it != w.corp_body_pools.end() && it->first.first == corp; ++it)
    {
        if (amount <= 0.0f) return;
        float& q = it->second.quantities[static_cast<std::size_t>(res)];
        const float take = std::min(q, amount);
        q      -= take;
        amount -= take;
    }
}

/// Debit the roster row's gated axes from the corp's pools. All-or-nothing:
/// checks every gated axis is affordable before debiting any of them.
bool debit_hire_cost(world& w, entity_id corp, const roster_row& row)
{
    const bool need_ore    = row.gate.ore_q    > 0;
    const bool need_farm   = row.gate.farm_q   > 0;
    const bool need_energy = row.gate.energy_q > 0;
    // port_q is a building check (corp_owns_port), not a resource — nothing to debit.

    if (need_ore && corp_stockpile_total(w, corp, resource_type::steel) < hire_axis_cost &&
        corp_stockpile_total(w, corp, resource_type::iron_ore) < hire_axis_cost &&
        corp_stockpile_total(w, corp, resource_type::iron_nickel_ore) < hire_axis_cost)
        return false;
    if (need_farm && corp_stockpile_total(w, corp, resource_type::food_rations) < hire_axis_cost &&
        corp_stockpile_total(w, corp, resource_type::agricultural_produce) < hire_axis_cost)
        return false;
    if (need_energy && corp_stockpile_total(w, corp, resource_type::coal) < hire_axis_cost &&
        corp_stockpile_total(w, corp, resource_type::refined_fuel) < hire_axis_cost &&
        corp_stockpile_total(w, corp, resource_type::petroleum) < hire_axis_cost)
        return false;

    if (need_ore)
        debit_from_corp(w, corp,
            hire_axis_resource(w, corp, {resource_type::steel, resource_type::iron_ore,
                                         resource_type::iron_nickel_ore}),
            hire_axis_cost);
    if (need_farm)
        debit_from_corp(w, corp,
            hire_axis_resource(w, corp, {resource_type::food_rations,
                                         resource_type::agricultural_produce}),
            hire_axis_cost);
    if (need_energy)
        debit_from_corp(w, corp,
            hire_axis_resource(w, corp, {resource_type::coal, resource_type::refined_fuel,
                                         resource_type::petroleum}),
            hire_axis_cost);
    return true;
}

/// The batch size one hire_unit command raises — matches the campaign's
/// original Kepler unit stub (hard_coded_world.cpp), which used the same
/// figure before BL-324 gave hiring a real seam.
constexpr int hire_batch_manpower = 50;

/// The market a body's orders would list against, or `null_entity` if the body
/// carries none. A body may host several markets (BL-096); an order is placed at
/// body scope and routes to a centre at clearing time (market_for_tile), so this
/// only answers "is there a market here at all", which is the UI's own
/// precondition ("This body has no market.").
entity_id any_market_on_body(const world& w, entity_id body)
{
    entity_id best = null_entity;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body && (best == null_entity || mid < best))
            best = mid; // lowest id, so the answer does not depend on map order
    return best;
}

/// Number of standing sell orders `corp` currently holds, across all bodies.
std::size_t corp_order_count(const world& w, entity_id corp)
{
    std::size_t n = 0;
    for (const sell_order& o : w.sell_orders)
        if (o.corp == corp)
            ++n;
    return n;
}

corp_command_result map_construction(construction_result r)
{
    switch (r)
    {
        case construction_result::placed:                 return corp_command_result::applied;
        case construction_result::insufficient_funds:     return corp_command_result::rejected_funds;
        case construction_result::no_corp:                return corp_command_result::rejected_no_corp;
        case construction_result::no_tile:                return corp_command_result::rejected_invalid;
        case construction_result::tech_locked:            return corp_command_result::rejected_tech_locked;
        case construction_result::invalid_tile:
        case construction_result::out_of_range:
        case construction_result::slot_occupied:
        case construction_result::insufficient_materials: return corp_command_result::rejected_placement;
    }
    return corp_command_result::rejected_invalid;
}

} // namespace

corp_command_result apply_corp_command(world& w, const recipe_registry& reg,
                                       const corp_command& cmd,
                                       entity_id* out_building)
{
    if (out_building)
        *out_building = null_entity;

    if (w.corporations.find(cmd.corp) == w.corporations.end())
        return corp_command_result::rejected_no_corp;

    switch (cmd.verb)
    {
        case corp_verb::build:
        {
            entity_id built = null_entity;
            const construction_result r =
                construct_building(w, reg, cmd.corp, cmd.tile, cmd.type, cmd.target, built);
            if (r == construction_result::placed && out_building)
                *out_building = built;
            return map_construction(r);
        }

        case corp_verb::demolish:
        {
            // demolish_building validates existence + ownership and mutates only
            // on success; distinguish the refusal reasons for the log.
            if (w.buildings.find(cmd.subject) == w.buildings.end())
                return corp_command_result::rejected_invalid;
            if (!owns(w, cmd.corp, cmd.subject))
                return corp_command_result::rejected_not_owner;
            return demolish_building(w, cmd.corp, cmd.subject)
                       ? corp_command_result::applied
                       : corp_command_result::rejected_invalid;
        }

        case corp_verb::set_recipe:
        {
            const auto bit = w.buildings.find(cmd.subject);
            if (bit == w.buildings.end())
                return corp_command_result::rejected_invalid;
            if (!owns(w, cmd.corp, cmd.subject))
                return corp_command_result::rejected_not_owner;
            building_component& b = bit->second;
            if (b.type != building_type::processing_facility)
                return corp_command_result::rejected_invalid;
            if (reg.get_recipe(cmd.recipe) == nullptr)
                return corp_command_result::rejected_invalid;
            if (b.recipe == cmd.recipe)
                return corp_command_result::rejected_state; // no-op
            b.recipe              = cmd.recipe;
            b.active_recipe_index = static_cast<int>(cmd.recipe); // registry index == id (recipe_at)
            b.loss_streak         = 0; // give the new recipe a chance (BL-079 idiom)
            return corp_command_result::applied;
        }

        case corp_verb::set_workforce:
        {
            const auto bit = w.buildings.find(cmd.subject);
            if (bit == w.buildings.end())
                return corp_command_result::rejected_invalid;
            if (!owns(w, cmd.corp, cmd.subject))
                return corp_command_result::rejected_not_owner;
            if (cmd.workforce < 0 || cmd.workforce > 200)
                return corp_command_result::rejected_invalid;
            building_component& b = bit->second;
            // A manual target PINS the dial — the press has always cleared
            // workforce_auto (selection_panel.cpp, construction_panel.cpp) and
            // the seam never did, so a command-driven player's target was
            // silently re-solved by the auto-solver on the next tick. So the
            // no-op test is "already there AND already pinned": re-asserting a
            // target the solver happens to be holding is a real change of
            // control, not a no-op. (BL-293; the dictionary described this
            // behaviour before the seam implemented it.)
            if (b.workforce_target == cmd.workforce && !b.workforce_auto)
                return corp_command_result::rejected_state;
            b.workforce_target = cmd.workforce;
            b.workforce_auto   = false;
            return corp_command_result::applied;
        }

        case corp_verb::set_workforce_auto:
        {
            const auto bit = w.buildings.find(cmd.subject);
            if (bit == w.buildings.end())
                return corp_command_result::rejected_invalid;
            if (!owns(w, cmd.corp, cmd.subject))
                return corp_command_result::rejected_not_owner;
            building_component& b = bit->second;
            if (b.workforce_auto)
                return corp_command_result::rejected_state; // already auto
            b.workforce_auto = true;
            return corp_command_result::applied;
        }

        case corp_verb::idle:
        case corp_verb::resume:
        {
            const auto bit = w.buildings.find(cmd.subject);
            if (bit == w.buildings.end())
                return corp_command_result::rejected_invalid;
            if (!owns(w, cmd.corp, cmd.subject))
                return corp_command_result::rejected_not_owner;
            building_component& b     = bit->second;
            const bool          want  = (cmd.verb == corp_verb::idle);
            if (b.decommissioned == want)
                return corp_command_result::rejected_state; // already there
            b.decommissioned = want;
            b.loss_streak    = 0;
            // An idled port/hub stops anchoring supply (is_supply_anchor);
            // resume restores it — either way the reach field is stale.
            invalidate_logistics_caches(w);
            return corp_command_result::applied;
        }

        case corp_verb::place_road:
            return map_construction(place_road(w, reg, cmd.corp, cmd.tile, cmd.road_tier));

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
            const auto avail = available_rows(w, cmd.corp, campaign_roster_band);
            if (std::find(avail.begin(), avail.end(), &row) == avail.end())
                return corp_command_result::rejected_invalid;

            if (w.tiles.find(cmd.tile) == w.tiles.end())
                return corp_command_result::rejected_invalid;

            // BL-325 S2: hire moves onto the base. cmd.tile must carry the corp's
            // own COMPLETED military_base — supersedes hire-anywhere (BL-324). A
            // base under construction (ticks_remaining > 0) or a decommissioned
            // one does not qualify, mirroring the completed-hub idiom
            // (supply_system.cpp's inland_logistics_hub check).
            {
                bool has_muster_base = false;
                for (const auto& [bid, bc] : w.buildings)
                {
                    if (bc.tile == cmd.tile && bc.type == building_type::military_base
                        && bc.ticks_remaining <= 0 && !bc.decommissioned
                        && owns(w, cmd.corp, bid))
                    {
                        has_muster_base = true;
                        break;
                    }
                }
                if (!has_muster_base)
                    return corp_command_result::rejected_placement;
            }

            if (!debit_hire_cost(w, cmd.corp, row))
                return corp_command_result::rejected_funds;

            const entity_id unit = w.create_entity();
            w.units[unit] = unit_component{
                .position = cmd.tile,
                .owner    = cmd.corp,
                .count    = hire_batch_manpower,
                .type     = cmd.unit_type,
                .strength = hire_batch_manpower,
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

        case corp_verb::place_sell_order:
        {
            const entity_id body = cmd.subject;
            if (w.bodies.find(body) == w.bodies.end())
                return corp_command_result::rejected_invalid;

            const entity_id mid = any_market_on_body(w, body);
            if (mid == null_entity)
                return corp_command_result::rejected_invalid; // "This body has no market."

            const std::size_t r = static_cast<std::size_t>(cmd.target);
            if (r >= resource_count)
                return corp_command_result::rejected_invalid;
            if (w.markets.at(mid).base_price[r] <= 0.0f)
                return corp_command_result::rejected_invalid; // unpriced here

            if (!(cmd.quantity > 0.0f) || cmd.floor_price < 0.0f)
                return corp_command_result::rejected_invalid;

            if (corp_order_count(w, cmd.corp) >= max_sell_orders_per_corp)
                return corp_command_result::rejected_state; // book full

            sell_order o;
            o.id          = w.allocate_order_id();
            o.corp        = cmd.corp;
            o.body        = body;
            o.resource    = cmd.target;
            o.quantity    = cmd.quantity;
            o.floor_price = cmd.floor_price;
            w.sell_orders.push_back(o); // appended: insertion order is time priority
            return corp_command_result::applied;
        }

        case corp_verb::remove_sell_order:
        {
            if (cmd.order == 0)
                return corp_command_result::rejected_invalid;

            const auto it = std::find_if(w.sell_orders.begin(), w.sell_orders.end(),
                                         [&](const sell_order& o) { return o.id == cmd.order; });
            if (it == w.sell_orders.end())
                return corp_command_result::rejected_invalid; // no such order
            if (it->corp != cmd.corp)
                return corp_command_result::rejected_not_owner; // someone else's order

            // Erase by id, never by index: every surviving order keeps its handle,
            // so a removal cannot invalidate a command already composed against
            // another order.
            w.sell_orders.erase(it);
            return corp_command_result::applied;
        }
    }
    return corp_command_result::rejected_invalid;
}
