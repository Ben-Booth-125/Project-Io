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
// corp hold ANY of the resource an axis needs, summed across its buildings.
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

/// Debit @p amount of @p res from @p corp's stockpile, draining across its
/// owned buildings in asset order. Only called after affordability is
/// confirmed, so this should never under-run — but walks defensively rather
/// than assuming a single building holds the whole amount.
void debit_from_corp(world& w, entity_id corp, resource_type res, float amount)
{
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end()) return;
    for (const entity_id asset : cit->second.assets)
    {
        if (amount <= 0.0f) return;
        const auto sit = w.stockpiles.find(asset);
        if (sit == w.stockpiles.end()) continue;
        float& q = sit->second.quantities[static_cast<std::size_t>(res)];
        const float take = std::min(q, amount);
        q      -= take;
        amount -= take;
    }
}

/// Debit the roster row's gated axes from the corp's stockpile. All-or-nothing:
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
            if (b.workforce_target == cmd.workforce)
                return corp_command_result::rejected_state; // no-op
            b.workforce_target = cmd.workforce;
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
    }
    return corp_command_result::rejected_invalid;
}
