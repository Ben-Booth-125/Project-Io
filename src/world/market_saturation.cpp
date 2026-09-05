#include "market_saturation.hpp"

// ---------------------------------------------------------------------------
// market_saturation — BL-775. Can a market close its chains, and can each part
// of it pay for itself?
//
// PROMOTED OUT OF A HARNESS, and that is the point. Both measures below were
// written inside the anonymous namespace of tools/verify/demand_census.cpp, so
// nothing in src/ could call them: generation could not use the measures its own
// verification already trusted. Ben's phase 6 (GENERATION_STRATEGY.md § The
// eight phases) needs exactly them — "given a planet with markets and national
// borders, how can we saturate all the resources in said market, so that each
// part makes some profit?" — so they move here and the census calls the promoted
// copy.
//
// TWO CALLERS, ONE IMPLEMENTATION. That is the whole reason to move rather than
// copy. era_minus_one.hpp records what the alternative costs: six divergences
// between what generation ran and what every harness measured, and a seventh
// caller nobody counted. A saturation measure computed one way by generation and
// another by the census would be that defect in a new place.
//
// THE BODIES ARE UNCHANGED. Only names moved — `classification` and `classify`
// were too generic for src/world (both already collide there), so they are
// `resource_classification` and `classify_resources`; `measure_completeness` is
// `measure_market_completeness`. The `terminal_names_out` out-param is gone
// because it existed only for the census's own printing.
// ---------------------------------------------------------------------------

#include "logistics.hpp"
#include "market_clearing.hpp" // market_for_tile — the catchment partition
#include "recipe_registry.hpp"

#include <algorithm>
#include <unordered_map>

std::vector<entity_id> sorted_market_ids(const world& w)
{
    std::vector<entity_id> ids;
    ids.reserve(w.markets.size());
    for (const auto& kv : w.markets)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::array<resource_classification, resource_count>
classify_resources(const world& w, const recipe_registry& reg)
{
    std::array<resource_classification, resource_count> c{};

    for (std::size_t r = 0; r < resource_count; ++r)
        c[r].depth = reg.depth_of(static_cast<resource_type>(r));

    // --- what an era-allowed recipe makes and eats -------------------------
    // The BROWSE path (recipe_count/recipe_at), which is the era-masked one.
    const int n_allowed = reg.recipe_count(building_type::processing_facility);
    const bool processing_available = reg.building_available(building_type::processing_facility);
    for (int i = 0; i < n_allowed && processing_available; ++i)
    {
        const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (rc.outputs[r] > 0.0f)
                c[r].produced_by_recipe = true;
            if (rc.inputs[r] > 0.0f)
                c[r].sink_process = true;
        }
    }

    // --- what the ground yields --------------------------------------------
    // A BOOLEAN only. `w.tiles` is unordered and a float sum over its layout
    // would be run-dependent; an OR is not.
    for (const auto& [tid, t] : w.tiles)
    {
        (void)tid;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (t.resource_deposit[r] > 0.0f)
                c[r].has_deposit = true;
    }

    // --- the two authored baskets, AS MASKED BY THIS BAND -------------------
    // BL-640: read population_demand_basket() / background_demand_basket(), the
    // registry's era-resolved folds - the exact vectors inject_population_demand
    // and inject_background_demand multiply by. `.demand_basket` on the params is
    // now the SHARED (`any`) tranche alone, and reading it here would have this
    // census attribute a structural sink to a band whose basket never names the
    // good - precisely the defect the banding closes. `reg` already carries this
    // band (set_era, above), so no extra state is threaded in.
    const std::array<float, resource_count>& pd_basket = reg.population_demand_basket();
    const std::array<float, resource_count>& bd_basket = reg.background_demand_basket();
    const std::array<float, resource_count>& ed_basket = reg.endemic_demand_basket();
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (pd_basket[r] > 0.0f)
            c[r].sink_household = true;
        if (bd_basket[r] > 0.0f)
            c[r].sink_background = true;
        if (ed_basket[r] > 0.0f)
            c[r].sink_endemic = true;   // BL-647: the era-resolved endemic fold
    }

    // BL-644: the State channel — the space programme's authored lumps, read
    // from the same registry dial derive_space_programme_claims gates on.
    {
        const space_programme_params& sp = reg.space_programme();
        if (std::isfinite(sp.components_lump) && sp.components_lump > 0.0f)
            c[static_cast<std::size_t>(resource_type::spacecraft_components)].sink_state = true;
        if (std::isfinite(sp.propellant_lump) && sp.propellant_lump > 0.0f)
            c[static_cast<std::size_t>(resource_type::propellant)].sink_state = true;
    }

    // BL-643: the Infrastructure channel — any non-zero authored rate makes
    // stone/timber structurally wanted by the network, read from the same
    // registry dial derive_network_upkeep_claims gates on.
    {
        const network_upkeep_params& nu = reg.network_upkeep();
        float stone_any = nu.stone_per_hub, timber_any = nu.timber_per_hub;
        for (std::size_t i = 0; i < 3; ++i)
        {
            stone_any  += nu.stone_per_level[i];
            timber_any += nu.timber_per_level[i];
        }
        if (std::isfinite(stone_any) && stone_any > 0.0f)
            c[static_cast<std::size_t>(resource_type::stone)].sink_infra = true;
        if (std::isfinite(timber_any) && timber_any > 0.0f)
            c[static_cast<std::size_t>(resource_type::timber)].sink_infra = true;
    }

    // --- construction baskets, over every era-available building -----------
    // extraction_site is keyed by target_resource and processing_facility by
    // recipe id (recipe_registry::resource_build_cost_for); every other type
    // resolves to its type-level basket. Enumerated through that ONE accessor so
    // the census cannot disagree with the draw run_construction actually makes.
    for (std::size_t bt_i = 0; bt_i < building_type_count; ++bt_i)
    {
        const building_type bt = static_cast<building_type>(bt_i);
        if (bt == building_type::none || !reg.building_available(bt))
            continue;

        auto note = [&c](const std::array<float, resource_count>& row) {
            for (std::size_t r = 0; r < resource_count; ++r)
                if (row[r] > 0.0f)
                    c[r].sink_construct = true;
        };

        if (bt == building_type::extraction_site)
        {
            for (std::size_t r = 0; r < resource_count; ++r)
                note(reg.resource_build_cost_for(bt, static_cast<resource_type>(r), no_recipe));
        }
        else if (bt == building_type::processing_facility)
        {
            for (int i = 0; i < n_allowed; ++i)
            {
                const recipe& rc = reg.recipe_at(bt, i);
                note(reg.resource_build_cost_for(bt, resource_type::iron_ore,
                                                 reg.recipe_id(rc.name)));
            }
        }
        else
        {
            note(reg.resource_build_cost_for(bt, resource_type::iron_ore, no_recipe));
        }
    }

    // BL-709: the sector's own draw is a construction sink too, and a
    // STRUCTURAL one — it applies to every building under construction whatever
    // its type or recipe, so it is not expressible as a row in any of the
    // baskets enumerated above. Read from the same registry dial
    // `run_construction` reads, so zeroing it un-substantiates this sink by
    // name rather than leaving a comment claiming one.
    if (reg.construction().capacity_per_build_tick > 0.0f)
        c[static_cast<std::size_t>(resource_type::construction_capacity)].sink_construct = true;

    // --- is it priced on any market? ---------------------------------------
    for (const auto& [mid, mc] : w.markets)
    {
        (void)mid;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (mc.base_price[r] > 0.0f)
                c[r].priced = true;
    }

    // --- the standing-force pool draw --------------------------------------
    const unit_upkeep_params& up = reg.military().upkeep;
    for (std::size_t r = 0; r < resource_count; ++r)
        if (up.goods_per_head[r] > 0.0f)
            c[r].sink_unit_upkeep = true;

    // --- BL-641: the INDUSTRY pool draw ------------------------------------
    // Read from the registry the same way the construction baskets above are:
    // over every building type AVAILABLE IN THIS BAND, through the registry's own
    // band-composing accessor, so the census cannot disagree with the draw
    // run_building_upkeep actually makes.
    for (std::size_t t = 0; t < building_type_count; ++t)
    {
        const building_type bt = static_cast<building_type>(t);
        if (!reg.building_available(bt))
            continue;
        const auto basket = building_upkeep_goods(reg.building_upkeep(), bt, reg.era());
        for (std::size_t r = 0; r < resource_count; ++r)
            if (basket[r] > 0.0f)
                c[r].sink_industry = true;
    }

    return c;
}

std::vector<std::size_t>
terminal_resources(const std::array<resource_classification, resource_count>& cls)
{
    std::vector<std::size_t> out;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const resource_classification& c = cls[r];
        // BL-647/BL-644: the endemic basket is a household-class terminal pull,
        // and a state purchase consumes what it buys — both are TERMINAL sinks
        // by MARKETS.md property 4's own definition.
        if (c.sink_household || c.sink_construct || c.sink_industry || c.sink_unit_upkeep
            || c.sink_endemic || c.sink_state || c.sink_infra)
            out.push_back(r);
    }
    return out;
}

bool tile_in_reach(const world& w, entity_id tile, float max_reach)
{
    if (is_supply_anchor(w, tile))
        return true;                       // the anchor exemption, as placement has it
    const float reach = tile_reach_cost(w, tile);
    if (reach < 0.0f)
        return true;                       // "not computed" is permissive, as placement has it
    return reach <= max_reach;             // infinity fails this, which is the point
}

std::vector<market_completeness>
measure_market_completeness(world& w, const recipe_registry& reg,
                     const std::array<resource_classification, resource_count>& cls)
{
    const std::vector<std::size_t> terminals = terminal_resources(cls);

    const std::vector<entity_id> mids = sorted_market_ids(w);
    std::vector<market_completeness> rows(mids.size());
    std::unordered_map<entity_id, std::size_t> slot;
    for (std::size_t i = 0; i < mids.size(); ++i)
    {
        rows[i].market          = mids[i];
        rows[i].body            = w.markets.at(mids[i]).body;
        rows[i].terminals_total = static_cast<int>(terminals.size());
        slot[mids[i]]           = i;
    }

    // The reach fields, one multi-source Dijkstra per body carrying a market,
    // seeded in ascending body id. `body_reach_field` is itself deterministic
    // (seeded from the anchor set in raster order); the order here only fixes
    // which bodies get a field, and every one of them does.
    {
        std::vector<entity_id> bodies;
        bodies.reserve(mids.size());
        for (const entity_id mid : mids)
            bodies.push_back(w.markets.at(mid).body);
        std::sort(bodies.begin(), bodies.end());
        bodies.erase(std::unique(bodies.begin(), bodies.end()), bodies.end());
        for (const entity_id b : bodies)
            (void)body_reach_field(w, b);
    }

    const float max_reach = reg.construction().max_logistics_reach;

    // The tile walk. `w.tiles` is unordered, so every accumulation here is an
    // integer increment or a boolean OR — both commutative, neither able to
    // vary with map layout (the R3 rule this file already runs on).
    std::vector<std::array<bool, resource_count>> deposit(mids.size());
    for (auto& row : deposit)
        row.fill(false);

    for (const auto& [tid, t] : w.tiles)
    {
        const entity_id mid = market_for_tile(w, tid);
        const auto it = slot.find(mid);
        if (it == slot.end())
            continue;                       // a body with no market: nothing clears here
        const std::size_t s = it->second;
        ++rows[s].catchment_tiles;
        if (max_reach >= 0.0f && !tile_in_reach(w, tid, max_reach))
            continue;
        if (max_reach < 0.0f)
        {
            // Rule disabled: only genuinely unreachable ground is excluded.
            const float reach = tile_reach_cost(w, tid);
            if (reach >= 0.0f && !std::isfinite(reach) && !is_supply_anchor(w, tid))
                continue;
        }
        ++rows[s].in_reach_tiles;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (t.resource_deposit[r] > 0.0f)
                deposit[s][r] = true;
    }

    // Settlement, as context beside the score — never inside it.
    for (const auto& [cid, pcc] : w.population_centres)
    {
        if (pcc.razed)
            continue;
        const auto tit = w.population_centre_tile.find(cid);
        if (tit == w.population_centre_tile.end())
            continue;
        const auto it = slot.find(market_for_tile(w, tit->second));
        if (it == slot.end())
            continue;
        rows[it->second].heads += static_cast<long long>(pcc.scale);
    }

    // The closure. Monotone: a good only ever enters the set, so the loop
    // terminates whatever the roster's shape, and the fixpoint is independent of
    // the order the recipes are visited in.
    const bool processing_available = reg.building_available(building_type::processing_facility);
    const int  n_allowed = processing_available
                         ? reg.recipe_count(building_type::processing_facility) : 0;

    for (std::size_t s = 0; s < rows.size(); ++s)
    {
        std::array<bool, resource_count> have = deposit[s];
        for (std::size_t r = 0; r < resource_count; ++r)
            if (have[r])
                ++rows[s].raws_in_reach;

        bool changed = true;
        while (changed)
        {
            changed = false;
            for (int i = 0; i < n_allowed; ++i)
            {
                const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
                bool inputs_ok = true;
                for (std::size_t r = 0; r < resource_count && inputs_ok; ++r)
                    if (rc.inputs[r] > 0.0f && !have[r])
                        inputs_ok = false;
                if (!inputs_ok)
                    continue;
                for (std::size_t r = 0; r < resource_count; ++r)
                    if (rc.outputs[r] > 0.0f && !have[r])
                    {
                        have[r] = true;
                        changed = true;
                    }
            }
        }

        for (const std::size_t r : terminals)
            if (have[r])
                ++rows[s].terminals_closed;
        rows[s].completeness = (rows[s].terminals_total > 0)
                             ? static_cast<double>(rows[s].terminals_closed)
                               / static_cast<double>(rows[s].terminals_total)
                             : 0.0;
    }

    return rows;
}
