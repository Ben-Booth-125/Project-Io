#include "state_industry.hpp"

#include "budget_system.hpp"   // compute_building_opex, body_mean_habitability
#include "construction.hpp"    // construct_state_building, construction_capex (the founding)
#include "input_reach.hpp"     // recipe_inputs_supplied (the founding is chain-feasible)
#include "placement_rules.hpp" // is_water_tile
#include "market_clearing.hpp" // market_for_tile
#include "recipe_registry.hpp"
#include "world.hpp"
#include "logistics.hpp"       // BL-1255: tile_touches_track, invalidate_logistics_caches
#include <set>
#include <functional>

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
        case building_type::railway_station: // BL-1255: rail is transport infrastructure
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
    // BL-1255: a railway station is state industry outright (LOGISTICS.md § 4c,
    // NATIONS.md § State industry) — it carries no corporation-only machinery:
    // the router reads every active station, whoever owns it.
    if (type == building_type::railway_station)
        return true;
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
                        f.history_index = w.history_log.size() - 1;
                        break;
                    }
                }
            out.founded.push_back(f);
        }
    }

    // ---- THE COASTAL SEAT'S PORT (NATIONS.md § State industry, Ben 2026-10-14) ----
    // A major whose capital stands on the coast holds a port as state industry
    // (transport infrastructure is a strategic sector), and the state carries
    // the trade machinery it needs: its points are the nation's, and the trade
    // pass ships them under the nation's id (trade.cpp). One per such major,
    // unless it already holds one; sited on the coastal tile of its own ground
    // nearest the capital (Chebyshev on the grid, ties to territory order),
    // founded standing and paid like the founding above.
    for (const entity_id nid : nation_ids)
    {
        nation_component& nc = w.nations.at(nid);
        const auto cap = w.tiles.find(nc.capital_tile);
        if (cap == w.tiles.end() || !placement_rules::is_coastal(w, nc.capital_tile))
            continue;
        bool has_port = false;
        for (const entity_id bid : nc.state_assets)
            if (const auto bit = w.buildings.find(bid);
                bit != w.buildings.end() && bit->second.type == building_type::port)
                has_port = true;
        if (has_port)
            continue;
        std::vector<std::pair<int, entity_id>> sites; // (distance, tile), territory order kept by stable sort
        for (const entity_id tile : nc.tiles)
        {
            const auto tit = w.tiles.find(tile);
            if (tit == w.tiles.end() || tit->second.body != cap->second.body
                || placement_rules::is_water_tile(tit->second.substrate)
                || !placement_rules::is_coastal(w, tile))
                continue;
            const int d = std::max(std::abs(tit->second.grid_x - cap->second.grid_x),
                                   std::abs(tit->second.grid_y - cap->second.grid_y));
            sites.emplace_back(d, tile);
        }
        std::stable_sort(sites.begin(), sites.end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });
        entity_id built = null_entity;
        state_port_founding pf;
        pf.nation = nid;
        for (const auto& [d, tile] : sites)
        {
            (void)d;
            if (construct_state_port(w, reg, nid, tile, built, /*founding=*/true)
                != construction_result::placed)
                continue;
            building_component& b = w.buildings.at(built);
            b.ticks_remaining       = 0; // founded standing, as every generation holding
            b.construction_progress = 0.0f;
            const float price = construction_capex(w, reg, tile, building_type::port, resource_type::iron_ore);
            const float paid  = std::min(std::max(0.0f, nc.treasury), price);
            nc.treasury        -= paid;
            b.construction_paid = paid;
            invalidate_logistics_caches(w); // a new supply anchor
            world_history_entry e;
            e.timestamp = w.current_day_tick;
            e.topic     = history_topic::agency;
            e.body      = cap->second.body;
            e.corp      = champion_of(w, nid);
            e.event     = "State charter: " + nc.name + " founds a state port at its coastal seat"
                        + std::string(paid < price ? ", its treasury short of the price" : "");
            w.history_log.push_back(std::move(e));
            pf.building      = built;
            pf.price         = price;
            pf.paid          = paid;
            pf.history_index = w.history_log.size() - 1;
            break;
        }
        if (built != null_entity) { ++out.ports_founded; out.port_buildings.push_back(built); out.ports.push_back(pf); }
        else ++out.ports_unplaced;
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
    std::printf(" %d state port(s) at coastal seats (%d with no ground);", r.ports_founded, r.ports_unplaced);
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
    // BL-1255 fix round (review 6): OWNERSHIP FOLLOWS TERRITORY. Track on ground
    // that changed hands passes to the new holder's state (or a minor's firm) —
    // and ownerless track to the owner nearest along the line — before the gate
    // reads who owes its upkeep. A no-op on a world with no rail.
    assign_rail_owners(w);
    // BL-1255: per owner, its track tiles (for the gate below).
    std::map<entity_id, int> rail_tiles_of;
    for (const auto& [tile, owner] : w.rail_track)
    {
        (void)tile;
        if (owner != null_entity && w.nations.count(owner) != 0)
            ++rail_tiles_of[owner];
    }
    const std::set<entity_id> rail_idle_before = w.rail_idle;
    w.rail_idle.clear();
    bool station_flipped = false;
    std::vector<entity_id> nation_ids;
    for (const auto& [nid, nc] : w.nations)
        if (!nc.state_assets.empty() || rail_tiles_of.count(nid) != 0)
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
            const bool was_idle = b.state_idle;
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
            // BL-1255 (review 3): an idle station carries nothing, so its flip
            // stales the rail network.
            if (b.type == building_type::railway_station && b.state_idle != was_idle)
                station_flipped = true;
        }
        // BL-1255 fix round (review 2; Ben's state rule "a losing building idles,
        // never debt"): THE TRACK'S UPKEEP IS FUNDED HERE, after the nation's
        // buildings. Funded: committed, and apply_budget charges it. Not: the
        // nation's track IDLES for the tick (`world::rail_idle`) — no rail route
        // runs over it — and pays nothing.
        if (const auto rt = rail_tiles_of.find(nid); rt != rail_tiles_of.end())
        {
            const float bill = reg.rail().track_upkeep_per_tile * static_cast<float>(rt->second) * k_headroom;
            if (std::isfinite(bill) && bill <= left)
            {
                left          -= bill;
                led.committed += bill;
            }
            else
                w.rail_idle.insert(nid);
        }
    }
    if (station_flipped || w.rail_idle != rail_idle_before)
    {
        w.rail_networks.clear();
        bump_logistics_cache_generation(w);
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

// ---------------------------------------------------------------------------
// BL-1382 economy follow-up -- the first tick's state bill and the founding's
// reserve (Ben, 2026-10-19; see the header)
// ---------------------------------------------------------------------------
namespace {

/// The gate's headroom, the same hair `plan_state_funding` adds to every bound.
constexpr float k_state_gate_headroom = 1.0005f;

} // namespace

float state_first_tick_bill(const world& w, const recipe_registry& reg, entity_id nation)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return 0.0f;
    float bill = 0.0f;
    for (const entity_id bid : nit->second.state_assets)
        if (const auto bit = w.buildings.find(bid); bit != w.buildings.end())
        {
            const float bound = state_building_cost_bound(w, reg, bit->second) * k_state_gate_headroom;
            if (std::isfinite(bound))
                bill += bound;
        }
    int track = 0;
    for (const auto& [tile, owner] : w.rail_track)
    {
        (void)tile;
        if (owner == nation)
            ++track;
    }
    if (track > 0)
        bill += reg.rail().track_upkeep_per_tile * static_cast<float>(track) * k_state_gate_headroom;
    return bill;
}

std::vector<entity_id> state_gate_would_idle(const world& w, const recipe_registry& reg)
{
    // plan_state_funding's building walk, read-only: nations ascending, each
    // one's state_assets in stored order, a bound that fits comes off what is
    // left, one that does not idles.
    std::vector<entity_id> idled;
    for (const auto& [nid, nc] : w.nations)
    {
        (void)nid;
        float left = std::max(0.0f, nc.treasury);
        for (const entity_id bid : nc.state_assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            const float bound = state_building_cost_bound(w, reg, bit->second) * k_state_gate_headroom;
            if (std::isfinite(bound) && bound <= left)
                left -= bound;
            else
                idled.push_back(bid);
        }
    }
    std::sort(idled.begin(), idled.end());
    return idled;
}

state_reserve_report reserve_state_first_tick(world& w, const recipe_registry& reg,
                                              state_handover_report& handover)
{
    state_reserve_report rep;
    // Per nation, ascending: its founded port and its founded plant, the
    // port first (it was paid last).
    std::map<entity_id, std::pair<state_port_founding*, state_founding*>> paid_by;
    for (state_founding& f : handover.founded)
        if (f.building != null_entity && f.paid > 0.0f)
            paid_by[f.nation].second = &f;
    for (state_port_founding& p : handover.ports)
        if (p.building != null_entity && p.paid > 0.0f)
            paid_by[p.nation].first = &p;

    const char* const k_short = ", its treasury short of the price";
    const auto mark_short = [&](std::size_t idx) {
        if (idx >= w.history_log.size())
            return;
        std::string& ev = w.history_log[idx].event;
        if (ev.find(k_short) == std::string::npos)
            ev += k_short;
    };

    for (auto& [nid, pf] : paid_by)
    {
        const auto nit = w.nations.find(nid);
        if (nit == w.nations.end())
            continue;
        nation_component& nc = nit->second;
        const float bill = state_first_tick_bill(w, reg, nid);
        float need = bill - std::max(0.0f, nc.treasury);
        if (!(need > 0.0f))
            continue;
        float back_total = 0.0f;
        const auto give_back = [&](float& paid, entity_id building, std::size_t history_index) {
            if (!(need > 0.0f) || !(paid > 0.0f))
                return;
            const float back = std::min(need, paid);
            paid       -= back;
            need       -= back;
            back_total += back;
            nc.treasury += back;
            if (const auto bit = w.buildings.find(building); bit != w.buildings.end())
                bit->second.construction_paid = paid;
            mark_short(history_index);
        };
        if (pf.first != nullptr)
            give_back(pf.first->paid, pf.first->building, pf.first->history_index);
        if (pf.second != nullptr)
            give_back(pf.second->paid, pf.second->building, pf.second->history_index);
        if (back_total > 0.0f)
        {
            ++rep.nations_reserved;
            rep.returned += back_total;
            if (need > 0.0f)
                ++rep.nations_short;
        }
    }
    return rep;
}

void print_state_reserve(const state_reserve_report& r)
{
    std::printf("[state_reserve] %d major(s) kept their first tick's state bill from the foundings "
                "(%.1f credits of payment left short; %d still under the bill)\n",
                r.nations_reserved, static_cast<double>(r.returned), r.nations_short);
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------
// BL-1255 -- rail ownership and the rail handover (see the header)
// ---------------------------------------------------------------------------
namespace {

/// A minor's background firm: the lowest-id one homed there.
std::map<entity_id, entity_id> rail_firm_of(const world& w)
{
    std::map<entity_id, entity_id> firm_of;
    std::vector<entity_id> ids;
    for (const auto& [cid, cc] : w.corporations)
        if (cc.is_background)
            ids.push_back(cid);
    std::sort(ids.begin(), ids.end());
    for (const entity_id cid : ids)
        firm_of.emplace(w.corporations.at(cid).home_nation, cid);
    return firm_of;
}

/// The owner the GROUND names for track on @p tile: a major's state, or a
/// minor's background firm; null for ground no nation holds or a minor with
/// no background firm.
entity_id rail_ground_owner(const world& w, const std::map<entity_id, entity_id>& firm_of,
                            entity_id tile)
{
    const auto tn = w.tile_to_nation.find(tile);
    if (tn == w.tile_to_nation.end() || tn->second == null_entity)
        return null_entity;
    if (is_major_nation(w, tn->second))
        return tn->second;
    const auto f = firm_of.find(tn->second);
    return f != firm_of.end() ? f->second : null_entity;
}

/// The four cardinal neighbour tiles of @p tile (N, E, S, W; columns wrap);
/// null for a step off the grid.
std::array<entity_id, 4> rail_tile_neighbours(world& w, entity_id tile)
{
    std::array<entity_id, 4> out{null_entity, null_entity, null_entity, null_entity};
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return out;
    const auto bit = w.bodies.find(tit->second.body);
    if (bit == w.bodies.end())
        return out;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    const int x = tit->second.grid_x, y = tit->second.grid_y;
    const std::vector<entity_id>& grid = body_tile_grid(w, tit->second.body);
    const int c[4] = {y > 0 ? (y - 1) * gw + x : -1, y * gw + (x + 1) % gw,
                      y + 1 < gh ? (y + 1) * gw + x : -1, y * gw + (x + gw - 1) % gw};
    for (int k = 0; k < 4; ++k)
        if (c[k] >= 0 && static_cast<std::size_t>(c[k]) < grid.size())
            out[static_cast<std::size_t>(k)] = grid[static_cast<std::size_t>(c[k])];
    return out;
}

} // namespace

int assign_rail_owners(world& w)
{
    if (w.rail_track.empty())
        return 0;
    const std::map<entity_id, entity_id> firm_of = rail_firm_of(w);

    // 1. What the ground names, tile by tile (ascending tile id).
    std::map<entity_id, entity_id> want;
    for (const auto& [tile, owner] : w.rail_track)
    {
        (void)owner;
        want[tile] = rail_ground_owner(w, firm_of, tile);
    }

    // 2. NO TRACK IS OWNERLESS (Ben, 2026-10-14): a tile the ground names no
    //    owner for passes to the owner NEAREST ALONG THE LINE — a breadth-first
    //    walk over track from every named tile at once, seeded in ascending tile
    //    id, FIFO, neighbours N/E/S/W: the first to reach a tile owns it.
    std::vector<entity_id> frontier;
    for (const auto& [tile, o] : want)
        if (o != null_entity)
            frontier.push_back(tile);
    for (std::size_t head = 0; head < frontier.size(); ++head)
    {
        const entity_id t = frontier[head];
        const entity_id o = want.at(t);
        for (const entity_id n : rail_tile_neighbours(w, t))
        {
            const auto it = want.find(n);
            if (it == want.end() || it->second != null_entity)
                continue;
            it->second = o;
            frontier.push_back(n);
        }
    }

    // 3. A line touching no owned ground at all (an island of track): the owner
    //    whose ground lies nearest to its lowest tile, wrapped Chebyshev on the
    //    body, ties to the lower ground tile id — then along the line as above.
    bool any_left = false;
    for (const auto& [tile, o] : want)
        any_left = any_left || o == null_entity;
    if (any_left)
    {
        std::vector<entity_id> ground;
        for (const auto& [t, n] : w.tile_to_nation)
            if (rail_ground_owner(w, firm_of, t) != null_entity)
                ground.push_back(t);
        std::sort(ground.begin(), ground.end());
        for (auto& [tile, o] : want)
        {
            if (o != null_entity)
                continue;
            const tile_component& tc = w.tiles.at(tile);
            const int gw = w.bodies.at(tc.body).grid_width;
            entity_id best = null_entity;
            int best_d = 0;
            for (const entity_id g : ground)
            {
                const tile_component& gt = w.tiles.at(g);
                if (gt.body != tc.body)
                    continue;
                int dx = std::abs(gt.grid_x - tc.grid_x);
                dx = std::min(dx, gw - dx);
                const int d = std::max(dx, std::abs(gt.grid_y - tc.grid_y));
                if (best == null_entity || d < best_d)
                {
                    best = g;
                    best_d = d;
                }
            }
            if (best == null_entity)
                continue; // no owner on the body at all: left unowned
            o = rail_ground_owner(w, firm_of, best);
            // The rest of this island takes it along the line.
            std::vector<entity_id> f{tile};
            for (std::size_t h = 0; h < f.size(); ++h)
                for (const entity_id n : rail_tile_neighbours(w, f[h]))
                {
                    const auto it = want.find(n);
                    if (it == want.end() || it->second != null_entity)
                        continue;
                    it->second = o;
                    f.push_back(n);
                }
        }
    }

    // 4. Apply. The rail network reads the owner (its operator for the cut, its
    //    funding for idle track), so a change stales it.
    int changed = 0;
    for (auto& [tile, owner] : w.rail_track)
    {
        const entity_id o = want.at(tile);
        if (o != owner)
        {
            owner = o;
            ++changed;
        }
    }
    if (changed > 0)
    {
        w.rail_networks.clear();
        bump_logistics_cache_generation(w);
    }
    return changed;
}

rail_handover_report hand_over_rail(world& w)
{
    rail_handover_report out;

    // 1. LAY THE GENERATED LINKS BETWEEN THE CITIES HOLDING MARKET CENTRES (the
    //    BL-1255 fix round: "a rail must reach markets"). Each end of a link —
    //    a region's seat tile — is snapped to the MARKET that serves it
    //    (`market_for_tile`, the catchment every tile already routes to), then to
    //    the CITY holding that market's centre: the population centre nearest the
    //    centre tile within kMarketCity cells (wrapped Chebyshev), ties to the
    //    larger scale, then the lower tile id; the centre tile itself when no
    //    city stands that near. The track follows the cheapest land path between
    //    the two cities. A link whose ends snap to one market, or a pair already
    //    laid, adds nothing; a pair with no land path between them (a strait) is
    //    LEFT BROKEN and counted — the line stops at the water.
    constexpr int kMarketCity = 4;
    const auto city_for = [&](entity_id seat) -> entity_id {
        const entity_id mid = market_for_tile(w, seat);
        const auto mit = w.markets.find(mid);
        if (mit == w.markets.end() || mit->second.centre_tile == null_entity)
            return null_entity;
        const entity_id ct = mit->second.centre_tile;
        const tile_component& tc = w.tiles.at(ct);
        const int gw = w.bodies.at(tc.body).grid_width;
        entity_id best = null_entity;
        int best_d = 0, best_s = 0;
        for (const auto& [cid, ctile] : w.population_centre_tile)
        {
            const auto tit = w.tiles.find(ctile);
            const auto pit = w.population_centres.find(cid);
            if (tit == w.tiles.end() || pit == w.population_centres.end() || tit->second.body != tc.body)
                continue;
            int dx = std::abs(tit->second.grid_x - tc.grid_x);
            dx = std::min(dx, gw - dx);
            const int d = std::max(dx, std::abs(tit->second.grid_y - tc.grid_y));
            if (d > kMarketCity)
                continue;
            const int s = pit->second.scale;
            if (best == null_entity || d < best_d || (d == best_d && (s > best_s || (s == best_s && ctile < best))))
            {
                best = ctile;
                best_d = d;
                best_s = s;
            }
        }
        return best != null_entity ? best : ct;
    };

    // The realm's LINE is what the span built: links joined end to end (a
    // union over seat tiles, in record order). Each line's seats snap to the
    // market cities serving them; the line is laid as the minimum spanning tree
    // of those cities by land path cost (Prim from the lowest city tile, ties
    // to the lower tile pair) — so a line that spans two markets JOINS them. A
    // line inside one market's catchment adds no track.
    std::map<entity_id, entity_id> parent;
    const std::function<entity_id(entity_id)> find = [&](entity_id x) -> entity_id {
        auto it = parent.find(x);
        if (it == parent.end()) { parent[x] = x; return x; }
        if (it->second == x) return x;
        const entity_id r = find(it->second);
        parent[x] = r;
        return r;
    };
    for (const auto& [sa, sb] : w.gen_rail_links)
    {
        ++out.links;
        const entity_id ra = find(sa), rb = find(sb);
        if (ra != rb)
            parent[std::max(ra, rb)] = std::min(ra, rb);
    }
    std::map<entity_id, std::set<entity_id>> line_cities;
    for (const auto& [seat, pr] : parent)
    {
        (void)pr;
        const entity_id c = city_for(seat);
        if (c != null_entity)
            line_cities[find(seat)].insert(c);
    }
    for (const auto& [root, cityset] : line_cities)
    {
        (void)root;
        if (cityset.size() < 2)
        {
            ++out.links_merged;
            continue;
        }
        const std::vector<entity_id> cities(cityset.begin(), cityset.end());
        const entity_id body = w.tiles.at(cities.front()).body;
        std::vector<char> in(cities.size(), 0);
        in[0] = 1;
        for (std::size_t added = 1; added < cities.size(); ++added)
        {
            std::size_t bi = cities.size(), bj = cities.size();
            float bc = 0.0f;
            for (std::size_t i = 0; i < cities.size(); ++i)
            {
                if (!in[i]) continue;
                for (std::size_t j = 0; j < cities.size(); ++j)
                {
                    if (in[j]) continue;
                    const logistics_path& lp = intra_body_leg_path(w, body, cities[i], cities[j], leg_domain::land);
                    if (!lp.reachable) continue;
                    if (bi == cities.size() || lp.cost < bc)
                    {
                        bi = i; bj = j; bc = lp.cost;
                    }
                }
            }
            if (bi == cities.size())
            {
                // The rest of the line's cities lie across water from the tree: the
                // line is LEFT BROKEN at the strait (counted per city stranded).
                for (std::size_t j = 0; j < cities.size(); ++j)
                    if (!in[j]) ++out.links_broken;
                break;
            }
            in[bj] = 1;
            const logistics_path lp = intra_body_leg_path(w, body, cities[bi], cities[bj], leg_domain::land); // copied
            for (const entity_id t : lp.tiles)
            {
                const auto tit = w.tiles.find(t);
                if (tit != w.tiles.end() && !is_water(tit->second.substrate))
                    w.rail_track.emplace(t, null_entity);
            }
            ++out.links_laid;
        }
    }
    w.gen_rail_links.clear();
    if (w.rail_track.empty())
        return out;
    invalidate_logistics_caches(w);

    // 2. OWNERS: the ground's, else nearest along the line (assign_rail_owners).
    assign_rail_owners(w);
    std::map<entity_id, int> per_owner;
    const std::map<entity_id, entity_id> firm_of = rail_firm_of(w);
    for (const auto& [tile, owner] : w.rail_track)
    {
        ++out.track_tiles;
        if (owner == null_entity)
        {
            ++out.unowned;
            continue;
        }
        ++per_owner[owner];
        if (w.nations.count(owner) != 0) ++out.to_nations; else ++out.to_firms;
        if (rail_ground_owner(w, firm_of, tile) != owner)
            ++out.along_the_line;
    }
    out.per_owner_tiles.assign(per_owner.begin(), per_owner.end());

    // 3. STATIONS: every city tile touching track, ascending tile id, owned by
    //    the track it touches (the tile's own, else its first track neighbour
    //    N/E/S/W) — so every station has the owner its line has.
    std::vector<entity_id> cities;
    for (const auto& [centre, ctile] : w.population_centre_tile)
    {
        (void)centre;
        cities.push_back(ctile);
    }
    std::sort(cities.begin(), cities.end());
    cities.erase(std::unique(cities.begin(), cities.end()), cities.end());
    std::set<entity_id> has_station;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::railway_station)
            has_station.insert(b.tile);
    std::set<entity_id> market_cities;
    for (const auto& [mid, mc] : w.markets)
        market_cities.insert(mc.centre_tile);
    for (const entity_id tile : cities)
    {
        if (has_station.count(tile) != 0 || !tile_touches_track(w, tile))
            continue;
        entity_id owner = null_entity;
        if (const auto rt = w.rail_track.find(tile); rt != w.rail_track.end())
            owner = rt->second;
        for (const entity_id n : rail_tile_neighbours(w, tile))
        {
            if (owner != null_entity)
                break;
            if (const auto rt = w.rail_track.find(n); rt != w.rail_track.end())
                owner = rt->second;
        }
        if (owner == null_entity)
            continue;
        const entity_id bid = w.create_entity();
        building_component bc;
        bc.tile               = tile;
        bc.type               = building_type::railway_station;
        bc.workforce_assigned = 0.0f; // zero staff, the port's shape
        bc.ticks_remaining    = 0;    // stands complete, as every generation holding does
        w.buildings[bid]  = bc;
        w.stockpiles[bid] = stockpile_component{};
        if (const auto nit = w.nations.find(owner); nit != w.nations.end())
        {
            nit->second.state_assets.push_back(bid);
            ++out.station_nations;
        }
        else
            w.corporations.at(owner).assets.push_back(bid);
        ++out.stations;
    }
    invalidate_logistics_caches(w);
    return out;
}

void print_rail_handover(const rail_handover_report& r)
{
    std::printf("[rail] handover: %d links laid between market cities of %d (%d merged into one, "
                "%d broken at water); %d track tiles (%d to majors, %d to minors' firms, %d of them "
                "along the line, %d unowned), %d stations (%d a major's)\n",
                r.links_laid, r.links, r.links_merged, r.links_broken, r.track_tiles, r.to_nations,
                r.to_firms, r.along_the_line, r.unowned, r.stations, r.station_nations);
    std::fflush(stdout);
}
