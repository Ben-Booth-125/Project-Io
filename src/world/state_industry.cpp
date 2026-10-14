#include "state_industry.hpp"

#include "budget_system.hpp"   // compute_building_opex, body_mean_habitability
#include "construction.hpp"    // construct_state_building, construction_capex (the founding)
#include "input_reach.hpp"     // recipe_inputs_supplied (the founding is chain-feasible)
#include "placement_rules.hpp" // is_water_tile
#include "market_clearing.hpp" // market_for_tile
#include "recipe_registry.hpp"
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <string>

const char* strategic_sector_name(strategic_sector s)
{
    switch (s)
    {
        case strategic_sector::none:      return "private";
        case strategic_sector::energy:    return "energy";
        case strategic_sector::steel:     return "steel";
        case strategic_sector::arms:      return "arms";
        case strategic_sector::transport: return "transport";
        case strategic_sector::space:     return "space";
    }
    return "private";
}

strategic_sector strategic_sector_of(const recipe_registry& reg, building_type type,
                                     std::uint16_t recipe)
{
    switch (type)
    {
        case building_type::port:
        case building_type::inland_logistics_hub:
            return strategic_sector::transport;
        case building_type::launchpad:
            return strategic_sector::space;
        case building_type::processing_facility:
            break;
        default:
            return strategic_sector::none; // extraction and every passive type are private
    }
    const ::recipe* rc = reg.get_recipe(recipe);
    if (rc == nullptr)
        return strategic_sector::none;
    const auto makes = [rc](resource_type r) {
        return rc->outputs[static_cast<std::size_t>(r)] > 0.0f;
    };
    // Order is a tie-break only (no shipped recipe makes two of these): the
    // first sector named wins.
    if (makes(resource_type::ordnance))
        return strategic_sector::arms;
    if (makes(resource_type::spacecraft_components) || makes(resource_type::propellant))
        return strategic_sector::space;
    if (makes(resource_type::steel))
        return strategic_sector::steel;
    if (makes(resource_type::power) || makes(resource_type::refined_fuel))
        return strategic_sector::energy;
    return strategic_sector::none;
}

strategic_sector strategic_sector_of(const recipe_registry& reg, const building_component& b)
{
    return strategic_sector_of(reg, b.type, b.recipe);
}

bool state_runs(const recipe_registry& reg, building_type type, std::uint16_t recipe)
{
    return type == building_type::processing_facility
        && strategic_sector_of(reg, type, recipe) != strategic_sector::none;
}

state_handover_report hand_over_state_industry(world& w, const recipe_registry& reg)
{
    state_handover_report out;

    std::vector<entity_id> nation_ids;
    for (const auto& [nid, nc] : w.nations)
    {
        (void)nc;
        if (is_major_nation(w, nid))
            nation_ids.push_back(nid);
    }
    std::sort(nation_ids.begin(), nation_ids.end());
    out.majors = static_cast<int>(nation_ids.size());
    const auto major = [&](entity_id n) {
        return n != null_entity && std::binary_search(nation_ids.begin(), nation_ids.end(), n);
    };

    std::vector<entity_id> corp_ids;
    corp_ids.reserve(w.corporations.size());
    for (const auto& kv : w.corporations)
        corp_ids.push_back(kv.first);
    std::sort(corp_ids.begin(), corp_ids.end());

    for (const entity_id cid : corp_ids)
    {
        corporation_component& cc = w.corporations.at(cid);
        std::vector<entity_id> keep;
        keep.reserve(cc.assets.size());
        bool lost_any = false;
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
            {
                keep.push_back(bid);
                continue;
            }
            const building_component& b = bit->second;
            const auto tn = w.tile_to_nation.find(b.tile);
            const entity_id ground = (tn != w.tile_to_nation.end()) ? tn->second : null_entity;
            const strategic_sector sec = strategic_sector_of(reg, b);
            if (!major(ground) || sec == strategic_sector::none)
            {
                keep.push_back(bid);
                continue;
            }
            if (!cc.is_background)
            {
                ++out.champion_kept; // a champion keeps its own, whatever the sector
                keep.push_back(bid);
                continue;
            }
            if (!state_runs(reg, b.type, b.recipe))
            {
                ++out.held_back; // corporation-only machinery: stays with the firm
                keep.push_back(bid);
                continue;
            }
            w.nations.at(ground).state_assets.push_back(bid);
            ++out.passed;
            ++out.by_sector[static_cast<std::size_t>(sec)];
            lost_any = true;
        }
        if (!lost_any)
            continue;
        cc.assets = std::move(keep);
        if (cc.assets.empty())
            ++out.firms_emptied;
        // Re-seat an HQ that passed: the lowest-id building it still holds on
        // the HQ's body, else none (no border, as a corp with no home holdings).
        if (cc.hq_building != null_entity
            && std::find(cc.assets.begin(), cc.assets.end(), cc.hq_building) == cc.assets.end())
        {
            entity_id hq_body = null_entity;
            if (const auto hb = w.buildings.find(cc.hq_building); hb != w.buildings.end())
                if (const auto ht = w.tiles.find(hb->second.tile); ht != w.tiles.end())
                    hq_body = ht->second.body;
            entity_id best = null_entity;
            for (const entity_id bid : cc.assets)
            {
                const auto bit = w.buildings.find(bid);
                if (bit == w.buildings.end())
                    continue;
                const auto tit = w.tiles.find(bit->second.tile);
                if (tit == w.tiles.end() || tit->second.body != hq_body)
                    continue;
                if (best == null_entity || bid < best)
                    best = bid;
            }
            cc.hq_building = best;
            if (best == null_entity)
                cc.influence_range = 0.0f;
        }
    }

    // ---- THE FOUNDING (Ben, 2026-10-14): every major still holding none ----
    {
        static const char* const k_found_recipes[] = {
            "steel", "steel_bessemer", "refined_fuel", "coal_power_plant",
            "oil_power_plant", "ordnance", "propellant_atmospheric"};
        std::vector<std::uint16_t> recipes;
        for (const char* rn : k_found_recipes)
        {
            const std::uint16_t rid = reg.recipe_id(rn);
            if (rid != no_recipe && reg.get_recipe(rid) != nullptr
                && state_runs(reg, building_type::processing_facility, rid))
                recipes.push_back(rid);
        }
        const float batches = reg.economics(building_type::processing_facility).base_rate * 0.5f;
        std::unique_ptr<input_reach> ir; // built on first use: a world fact, shared
        std::unique_ptr<input_reach> ir_local; // the same, reach = its own market only
        std::map<std::pair<entity_id, std::uint16_t>, bool> feasible_at; // (market, recipe)
        std::map<std::pair<entity_id, std::uint16_t>, bool> local_at;    // (market, recipe)
        for (const entity_id nid : nation_ids)
        {
            if (!w.nations.at(nid).state_assets.empty())
                continue;
            state_founding f;
            f.nation = nid;
            const std::vector<entity_id> tiles = w.nations.at(nid).tiles; // territory order
            // Sprint 53 economy pass (G1b): THREE passes, nearest its feed first
            // -- the charter's own order (CORPORATION_GENERATION.md, "the recipe
            // bends before the building goes": own market, then reach). Pass 0
            // founds only where its OWN market makes every input (no trade route
            // to wait on: a founded plant opens with no stock, and one fed only
            // by trade starved through the haul's lead time and was idled in the
            // settle's first quarters); pass 1 where the chain runs by reach;
            // pass 2 anywhere (unchanged: every major founds one).
            for (int pass = 0; pass < 3 && f.building == null_entity; ++pass)
                for (const std::uint16_t rid : recipes)
                {
                    if (f.building != null_entity) break;
                    const ::recipe& rc = *reg.get_recipe(rid);
                    for (const entity_id tile : tiles)
                    {
                        const auto tit = w.tiles.find(tile);
                        if (tit == w.tiles.end() || placement_rules::is_water_tile(tit->second.substrate))
                            continue;
                        if (!recipe_runs_at_tile(w, rc, tile))
                            continue;
                        bool feasible = false;
                        bool local    = false;
                        if (const entity_id mid = market_for_tile(w, tile); mid != null_entity)
                        {
                            const auto key = std::make_pair(mid, rid);
                            if (pass == 0)
                            {
                                auto lit = local_at.find(key);
                                if (lit == local_at.end())
                                {
                                    if (!ir_local)
                                    {
                                        ir_local = std::make_unique<input_reach>(make_input_reach(w, reg));
                                        ir_local->same_market_only = true;
                                    }
                                    lit = local_at.emplace(key, recipe_inputs_supplied(
                                              w, reg, *ir_local, mid, rc, batches, null_entity)).first;
                                }
                                local = lit->second;
                            }
                            auto fit = feasible_at.find(key);
                            if (fit == feasible_at.end())
                            {
                                if (!ir) ir = std::make_unique<input_reach>(make_input_reach(w, reg));
                                fit = feasible_at.emplace(key, recipe_inputs_supplied(
                                          w, reg, *ir, mid, rc, batches, null_entity)).first;
                            }
                            feasible = fit->second;
                        }
                        if (pass == 0 && !local)
                            continue; // pass 0 founds only where its own market feeds it
                        if (pass == 1 && !feasible)
                            continue; // pass 1 founds only where the chain runs
                        const float price = construction_capex(w, reg, tile, building_type::processing_facility,
                                                               resource_type::iron_ore, rid);
                        entity_id built = null_entity;
                        if (construct_state_building(w, reg, nid, tile, rid, built, /*founding=*/true)
                            != construction_result::placed)
                            continue;
                        building_component& b = w.buildings.at(built);
                        b.ticks_remaining       = 0; // founded standing, as every generation holding
                        b.construction_progress = 0.0f;
                        nation_component& nc = w.nations.at(nid);
                        const float paid = std::min(std::max(0.0f, nc.treasury), price);
                        nc.treasury        -= paid;
                        b.construction_paid = paid;
                        f.building = built;
                        f.recipe   = rid;
                        f.chain_feasible = feasible;
                        f.price    = price;
                        f.paid     = paid;
                        if (ir) input_reach_invalidate(*ir);
                        if (ir_local) input_reach_invalidate(*ir_local);
                        local_at.clear(); // the founded plant now draws: ask its market again
                        // Recorded as a state charter (the history log).
                        world_history_entry e;
                        e.timestamp = w.current_day_tick;
                        e.topic     = history_topic::agency;
                        e.body      = tit->second.body;
                        e.corp      = champion_of(w, nid);
                        e.event     = "State charter: " + nc.name + " founds a state " + rc.display_name
                                    + " (" + strategic_sector_name(strategic_sector_of(reg, b)) + ")"
                                    + (paid < price ? ", its treasury short of the price" : "");
                        w.history_log.push_back(std::move(e));
                        break;
                    }
                }
            out.founded.push_back(f);
        }
    }

    for (const entity_id nid : nation_ids)
        out.per_nation.emplace_back(nid, static_cast<int>(w.nations.at(nid).state_assets.size()));
    return out;
}

bool state_commit_change(world& w, const recipe_registry& reg, entity_id nation,
                         const building_component& before, const building_component& after)
{
    if (before.state_idle)
        return true; // unfunded this tick: it runs nothing whatever changes
    constexpr float k_headroom = 1.0005f; // the gate's own
    const float delta = (state_building_cost_bound(w, reg, after)
                         - state_building_cost_bound(w, reg, before)) * k_headroom;
    if (!(delta > 0.0f))
        return true;
    if (!std::isfinite(delta) || delta > state_uncommitted_treasury(w, nation))
        return false;
    w.state_ledger[nation].committed += delta;
    return true;
}

void print_state_handover(const state_handover_report& r)
{
    std::printf("[state_industry] %d major(s): %d building(s) passed to the state "
                "(energy %d, steel %d, arms %d, transport %d, space %d); %d held back "
                "(corporation-only machinery), %d kept by champions, %d firm(s) left empty;",
                r.majors, r.passed,
                r.by_sector[static_cast<std::size_t>(strategic_sector::energy)],
                r.by_sector[static_cast<std::size_t>(strategic_sector::steel)],
                r.by_sector[static_cast<std::size_t>(strategic_sector::arms)],
                r.by_sector[static_cast<std::size_t>(strategic_sector::transport)],
                r.by_sector[static_cast<std::size_t>(strategic_sector::space)],
                r.held_back, r.champion_kept, r.firms_emptied);
    int founded = 0, unfounded = 0, infeasible = 0;
    float short_by = 0.0f;
    for (const state_founding& f : r.founded)
    {
        if (f.building == null_entity) { ++unfounded; continue; }
        ++founded;
        if (!f.chain_feasible) ++infeasible;
        short_by += f.price - f.paid;
    }
    std::printf(" %d founded (%d not chain-feasible, %d with nowhere to stand; treasuries short %.1f);",
                founded, infeasible, unfounded, static_cast<double>(short_by));

    for (const auto& [nid, n] : r.per_nation)
        std::printf(" nation %u:%d", static_cast<unsigned>(nid), n);
    std::printf("\n");
    std::fflush(stdout);
}

float state_building_cost_bound(const world& w, const recipe_registry& reg,
                                const building_component& b)
{
    const auto tit = w.tiles.find(b.tile);
    const entity_id body = (tit != w.tiles.end()) ? tit->second.body : null_entity;
    const building_economics& e = reg.economics(b.type);

    // Maintenance and wages at a full labour grant: every grant the tick hands
    // out is <= 1 (economy_system.cpp's pools), so this is the most it pays.
    const float hab = (body != null_entity) ? body_mean_habitability(w, body) : 1.0f;
    const building_opex ox = compute_building_opex(b, e, 1.0f, hab, reg.idle_maintenance_floor());
    float bound = ox.maintenance + ox.wages;

    const entity_id mid = market_for_tile(w, b.tile);
    const market_component* m = nullptr;
    if (mid != null_entity)
        if (const auto mit = w.markets.find(mid); mit != w.markets.end())
            m = &mit->second;
    if (m == nullptr)
        return bound; // no shelf: nothing can be bought, so nothing is billed

    const auto shelf_cost = [&](const std::array<float, resource_count>& qty) {
        float c = 0.0f;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (qty[r] > 0.0f)
                c += qty[r] * posted_price(*m, r);
        return c;
    };

    if (b.ticks_remaining > 0)
    {
        // A full-rate construction tick (run_construction: flat build cost over
        // the authored duration, and the per-tick need row, capacity included).
        const float duration = e.build_duration_ticks;
        if (duration > 0.0f)
        {
            bound += e.build_cost / duration;
            const auto& row = reg.resource_build_cost_for(b.type, b.target_resource, b.recipe);
            std::array<float, resource_count> need{};
            for (std::size_t r = 0; r < resource_count; ++r)
                need[r] = row[r] / duration;
            const float cap_rate = reg.construction().capacity_per_build_tick;
            if (cap_rate > 0.0f)
                need[static_cast<std::size_t>(resource_type::construction_capacity)] += cap_rate;
            bound += shelf_cost(need);
        }
        return bound;
    }
    if (b.decommissioned)
        return bound; // idle floor only: no inputs, no upkeep goods

    // A full run's inputs: run_processing's batch count at a grant of 1 (the
    // same expression as economy_system.cpp's processing_batches_full).
    if (b.type == building_type::processing_facility)
        if (const ::recipe* rc = reg.get_recipe(b.recipe); rc != nullptr)
        {
            const float wt_scalar = std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);
            const float batches   = reg.economics(building_type::processing_facility).base_rate
                                  * b.workforce_assigned * 1.0f * wt_scalar * building_supply_scalar(b);
            std::array<float, resource_count> need{};
            for (std::size_t r = 0; r < resource_count; ++r)
                need[r] = rc->inputs[r] * batches;
            bound += shelf_cost(need);
        }

    // The upkeep goods basket (run_building_upkeep), billed at this market.
    bound += shelf_cost(building_upkeep_goods(reg.building_upkeep(), b.type, reg.era()));
    return bound;
}

void plan_state_funding(world& w, const recipe_registry& reg)
{
    w.state_ledger.clear();
    std::vector<entity_id> nation_ids;
    for (const auto& [nid, nc] : w.nations)
        if (!nc.state_assets.empty())
            nation_ids.push_back(nid);
    std::sort(nation_ids.begin(), nation_ids.end());

    // A hair of headroom on every bound: the tick's float sums group the same
    // terms differently from the bound's, so an exactly-fitting bound could
    // leave the treasury an ULP below zero after settlement.
    constexpr float k_headroom = 1.0005f;

    for (const entity_id nid : nation_ids)
    {
        const nation_component& nc = w.nations.at(nid);
        world::state_tick_ledger& led = w.state_ledger[nid];
        led.treasury_open = nc.treasury;
        float left = std::max(0.0f, nc.treasury);
        for (const entity_id bid : nc.state_assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            building_component& b = bit->second;
            const float bound = state_building_cost_bound(w, reg, b) * k_headroom;
            if (std::isfinite(bound) && bound <= left)
            {
                left        -= bound;
                led.committed += bound;
                b.state_idle  = false;
                ++led.funded;
            }
            else
            {
                b.state_idle = true;
                ++led.idled;
            }
        }
    }
}

float state_uncommitted_treasury(const world& w, entity_id nation)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return 0.0f;
    float committed = 0.0f;
    if (const auto lit = w.state_ledger.find(nation); lit != w.state_ledger.end())
        committed = lit->second.committed;
    return std::max(0.0f, nit->second.treasury - committed);
}
