// chain_conversion_probe — BL-707 diagnosis. WHY does the ancient chain not convert?
//
// demand_census (BL-649) reports the SYMPTOM: at the 0 CE band, `clay`, `sand`,
// `peat` and `hides` are produced 0.0 while the processors that eat them bid for
// them, so `ceramics` (63.5) and `leather` (6.8) are produced in single digits and
// price at the ceiling in 12-13 of 14 markets. The census cannot say why, because
// it measures demand and price, not SITING.
//
// This probe measures siting. Per extractable resource it reports:
//   - tiles carrying a deposit at all, and the mean/max magnitude authored there;
//   - tiles on which it is the RICHEST extractable (what `richest_extractable`
//     would return), which is the quantity the SPAWN path selects on;
//   - extraction sites named for it at spawn, and again after the warm start,
//     which separates "the generator never sites it" from "the corp AI never
//     adds one".
// Plus processing facilities per recipe, on the same two readings, so a missing
// CONVERTER is distinguishable from a converter starved of inputs.
//
// IT REPORTS. It asserts nothing about a magnitude — the same discipline as
// demand_census, for the same reason.
//
// Build:  cmd //c tools\verify\build_lua_harness.bat chain_conversion_probe

#include "scripting/lua_state.hpp"

#include "harness_params.hpp"
#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/market_clearing.hpp"
#include "world/nation_step.hpp"
#include "world/placement_rules.hpp"
#include "world/corp_ai.hpp"
#include "world/recipe_registry.hpp"
#include "world/survey_system.hpp"
#include "world/resource_names.hpp"
#include "world/supply_system.hpp"
#include "world/tech_gate.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

std::string rname(std::size_t r)
{
    return resource_names::name_of(static_cast<resource_type>(r));
}

struct site_counts
{
    std::array<int, resource_count> n{};
};

site_counts count_sites(const world& w)
{
    site_counts c;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::extraction_site)
            ++c.n[static_cast<std::size_t>(b.target_resource)];
    return c;
}

/// Processing facilities by recipe id.
std::map<int, int> count_processors(const world& w)
{
    std::map<int, int> c;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::processing_facility)
            ++c[b.recipe];
    return c;
}

} // namespace

int main(int argc, char** argv)
{
    std::uint32_t seed       = 0;
    int           warm_ticks = 80;
    std::int64_t  epoch      = 0;
    // The app seeds survey state at campaign start (app.cpp, start_new_game tail).
    // demand_census and the other make_hard_coded_world harnesses do NOT, which
    // leaves every body `hidden` — including home. Default ON here, because the
    // app is the truth; `--no-init-survey` reproduces the census's world so the
    // two readings can be diffed.
    bool init_survey = true;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--no-init-survey") == 0)
            init_survey = false;
        else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
            seed = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc)
            warm_ticks = std::max(0, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--epoch") == 0 && i + 1 < argc)
            epoch = std::atoll(argv[++i]);
    }

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    world_gen_config gen_cfg;
    gen_cfg.load_from_lua(lua);
    reg.set_era(era_band_for_epoch(epoch));

    if (reg.recipe_count() == 0)
    {
        std::printf("FATAL: no recipes loaded.\n");
        return 2;
    }

    world_params p;
    p.seed       = seed;
    p.epoch_year = epoch;
    world w = make_hard_coded_world(p, nullptr, gen_cfg);
    assign_default_recipes(w, reg);
    generate_background_firms(w, reg, seed ^ 0x8A21F00Du);
    assign_default_recipes(w, reg);
    if (init_survey)
        init_survey_states(w);

    std::printf("chain_conversion_probe — BL-707, epoch %lld, seed %u, %d warm ticks, init_survey=%s\n",
                static_cast<long long>(epoch), seed, warm_ticks, init_survey ? "ON (as app.cpp)" : "OFF (as demand_census)");
    std::printf("  world: %zu tiles, %zu buildings\n\n", w.tiles.size(), w.buildings.size());

    // --- the deposit field, and what richest_extractable would pick ----------
    std::array<int, resource_count>   tiles_with{};
    std::array<int, resource_count>   tiles_richest{};
    std::array<double, resource_count> mag_sum{};
    std::array<float, resource_count>  mag_max{};
    for (const auto& [tid, tc] : w.tiles)
    {
        if (placement_rules::is_water_tile(tc.substrate))
            continue;
        bool any = false;
        const resource_type best = placement_rules::richest_extractable(tc, any);
        if (any)
            ++tiles_richest[static_cast<std::size_t>(best)];
        for (const resource_type rt : placement_rules::k_extractable)
        {
            const std::size_t ri = static_cast<std::size_t>(rt);
            const float v = tc.resource_deposit[ri];
            if (v <= 0.0f)
                continue;
            ++tiles_with[ri];
            mag_sum[ri] += v;
            mag_max[ri] = std::max(mag_max[ri], v);
        }
    }

    const site_counts at_spawn = count_sites(w);
    const std::map<int, int> proc_spawn = count_processors(w);

    // --- warm start ----------------------------------------------------------
    lp_pool_map lp;
    for (int t = 0; t < warm_ticks; ++t)
    {
        w.current_econ_tick = t;
        w.current_day_tick  = t;
        lp.clear();
        dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                         reg.logistics_cost(convoy_mode::space), &lp);
        advance_convoys(w);
        economy_report rep = run_economy_step(w, reg, /*spectating=*/false, &lp);
        auto flows = clear_markets(w, reg, rep);
        apply_budget(w, reg, flows, rep.workforce_contention, &rep.budgets, &rep.buildings,
                     &rep.building_labour);
        run_nation_step(w, reg, rep, t);
        advance_tech_gates(w);
        credit_arrived_convoys(w, t);
    }
    const site_counts after = count_sites(w);
    const std::map<int, int> proc_after = count_processors(w);

    // --- report --------------------------------------------------------------
    std::printf("--- the deposit field vs. what gets SITED (extractables only) ---\n");
    std::printf("  `richest` is the tile count on which richest_extractable() returns this\n");
    std::printf("  resource — the quantity the SPAWN path (author_building) selects on.\n\n");
    std::printf("  %-22s %10s %10s %9s %9s | %8s %8s %7s\n",
                "resource", "tiles", "richest", "meanMag", "maxMag",
                "sites@0", "sites@N", "delta");
    std::printf("  %-22s %10s %10s %9s %9s | %8s %8s %7s\n",
                "----------------------", "----------", "----------", "---------", "---------",
                "--------", "--------", "-------");
    for (const resource_type rt : placement_rules::k_extractable)
    {
        const std::size_t r = static_cast<std::size_t>(rt);
        const double mean = tiles_with[r] > 0 ? mag_sum[r] / tiles_with[r] : 0.0;
        std::printf("  %-22s %10d %10d %9.1f %9.1f | %8d %8d %+7d\n",
                    rname(r).c_str(), tiles_with[r], tiles_richest[r], mean, mag_max[r],
                    at_spawn.n[r], after.n[r], after.n[r] - at_spawn.n[r]);
    }

    std::printf("\n--- processing facilities, by recipe ---\n");
    std::printf("  %-30s %8s %8s %7s\n", "recipe", "proc@0", "proc@N", "delta");
    std::printf("  %-30s %8s %8s %7s\n", "------------------------------",
                "--------", "--------", "-------");
    std::map<int, int> all;
    for (const auto& [rid, n] : proc_spawn) all[rid] += 0;
    for (const auto& [rid, n] : proc_after) all[rid] += 0;
    for (const auto& [rid, ignored] : all)
    {
        const recipe* rc = reg.get_recipe(rid);
        std::string label = "(no recipe)";
        if (rc != nullptr)
        {
            label.clear();
            for (std::size_t r = 0; r < resource_count; ++r)
                if (rc->outputs[r] > 0.0f)
                {
                    if (!label.empty()) label += "+";
                    label += rname(r);
                }
            std::string in;
            for (std::size_t r = 0; r < resource_count; ++r)
                if (rc->inputs[r] > 0.0f)
                {
                    if (!in.empty()) in += "+";
                    in += rname(r);
                }
            label = in + " -> " + label;
        }
        const int a = proc_spawn.count(rid) ? proc_spawn.at(rid) : 0;
        const int b = proc_after.count(rid) ? proc_after.at(rid) : 0;
        std::printf("  %-30s %8d %8d %+7d\n", label.c_str(), a, b, b - a);
    }

    // --- what corp_ai's GLOBAL top-M site pre-filter actually contains --------
    // rank_extraction_sites (corp_ai.cpp, anonymous namespace) is replicated here
    // formula-for-formula: suitability = deposit * affinity * demand_weight, then
    // a partial_sort truncated to corp_ai_params::top_m_sites over EVERY surveyed
    // land tile in the world. This is the list every corp scores its extraction
    // build candidate from, so a resource absent from it cannot be mined by the AI
    // however starved its consumers are.
    {
        corp_ai_params ap;
        std::array<int, resource_count> wanted{};
        const int nrec = reg.recipe_count(building_type::processing_facility);
        for (int i = 0; i < nrec; ++i)
        {
            const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
            for (std::size_t r = 0; r < resource_count; ++r)
                if (rc.inputs[r] > 0.0f) ++wanted[r];
        }
        const site_counts now = count_sites(w);
        std::array<float, resource_count> weight;
        weight.fill(1.0f);
        for (std::size_t r = 0; r < resource_count; ++r)
            if (wanted[r] > 0)
                weight[r] = 1.0f + ap.input_demand_pull * static_cast<float>(wanted[r])
                                 / static_cast<float>(1 + now.n[r]);

        std::printf("\n  per-body survey state (the gate rank_extraction_sites applies first):\n");
        for (const auto& [bid2, bc2] : w.bodies)
        {
            std::size_t ntiles = 0;
            for (const auto& [tid2, tc2] : w.tiles)
                if (tc2.body == bid2) ++ntiles;
            const char* ph = bc2.survey.phase == survey_phase::surveyed ? "surveyed"
                           : bc2.survey.phase == survey_phase::hidden   ? "hidden"
                           : bc2.survey.phase == survey_phase::in_transit ? "in_transit"
                                                                         : "scanning";
            std::printf("    body %-4llu grid %3dx%-3d tiles %6zu  phase %-11s regions_done %d/%d\n",
                        static_cast<unsigned long long>(bid2), bc2.grid_width, bc2.grid_height,
                        ntiles, ph, bc2.survey.regions_done,
                        survey_region_count(bc2.grid_width, bc2.grid_height));
        }
        std::printf("\n");

        struct cand { entity_id tile; float suit; resource_type target; };
        std::vector<cand> all;
        std::array<float, resource_count> best_for{};
        for (const auto& [tid, tc] : w.tiles)
        {
            if (placement_rules::is_water_tile(tc.substrate)) continue;
            const auto bit = w.bodies.find(tc.body);
            if (bit == w.bodies.end()) continue;
            if (!survey_tile_visible(bit->second.survey, bit->second.grid_width,
                                     bit->second.grid_height, tc.grid_x, tc.grid_y))
                continue;
            float affinity = 1.0f;
            if (tc.landform == terrain_landform::mountain ||
                tc.landform == terrain_landform::canyon) affinity = 1.15f;
            for (const resource_type rt : placement_rules::k_extractable)
            {
                const std::size_t ri = static_cast<std::size_t>(rt);
                const float rich = tc.resource_deposit[ri];
                if (rich <= 0.0f) continue;
                const float suit = rich * affinity * weight[ri];
                all.push_back({tid, suit, rt});
                best_for[ri] = std::max(best_for[ri], suit);
            }
        }
        std::sort(all.begin(), all.end(), [](const cand& a, const cand& b) {
            if (a.suit != b.suit) return a.suit > b.suit;
            return a.tile < b.tile;
        });

        std::printf("\n--- corp_ai's GLOBAL top-%d extraction-site pre-filter (replicated) ---\n",
                    ap.top_m_sites);
        std::printf("  input_demand_pull = %.2f; %zu candidate (tile,resource) pairs world-wide\n\n",
                    ap.input_demand_pull, all.size());
        std::printf("  %4s %-22s %12s   %s\n", "rank", "target", "suitability", "can_place_in_world");
        const std::size_t show = std::min<std::size_t>(all.size(),
                                     static_cast<std::size_t>(std::max(0, ap.top_m_sites)));
        for (std::size_t i = 0; i < show; ++i)
        {
            const auto res = placement_rules::can_place_in_world(
                w, all[i].tile, building_type::extraction_site, all[i].target);
            std::printf("  %4zu %-22s %12.1f   %s\n", i + 1,
                        rname(static_cast<std::size_t>(all[i].target)).c_str(),
                        all[i].suit,
                        res ? "ok" : placement_reason_text(res.reason));
        }

        const float cutoff = show > 0 ? all[show - 1].suit : 0.0f;
        std::printf("\n  cut-off suitability to enter the top-%d: %.1f\n", ap.top_m_sites, cutoff);
        std::printf("  BEST suitability any tile can offer, per resource, vs that cut-off:\n");
        std::printf("  %-22s %12s  %s\n", "resource", "best suit", "reaches top-M?");
        for (const resource_type rt : placement_rules::k_extractable)
        {
            const std::size_t r = static_cast<std::size_t>(rt);
            if (best_for[r] <= 0.0f) continue;
            std::printf("  %-22s %12.1f  %s\n", rname(r).c_str(), best_for[r],
                        best_for[r] >= cutoff ? "yes" : "NO");
        }
    }

    std::printf("\nPROBE COMPLETE (reports only; asserts nothing)\n");
    return 0;
}
