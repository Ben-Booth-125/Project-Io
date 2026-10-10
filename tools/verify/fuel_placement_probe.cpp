// ---------------------------------------------------------------------------
// fuel_placement_probe — BL-1217 D6 (no processor beyond its output's want)
// ---------------------------------------------------------------------------
// QUESTION. About 34 refined-fuel plants stand per seed against under one
// consumer. WHICH PLACEMENT PATH puts them there? Each processor making
// refined fuel is filed under the path that placed it:
//   specialist        a walk specialist (charter_place, serve = none: best tier)
//   seat              the seated specialist
//   charter:fuel      a walk firm chartered FOR refined fuel (its gap good)
//   charter:attached  a walk firm chartered for ANOTHER good; this processor is
//                     its incidental one (extraction focus) or a sibling of it
//   pass6             a legacy Pass 6 background firm (no origin region)
//   settle            built during the 12-tick settle (not in the GEN set)
//   unowned           no corporation holds it
// and split by whether its recipe at GEN already made fuel or SWITCHED into it.
//
// The WANT is read the generation way (body_demand + upkeep + construction +
// processor input demand at nominal batches) against fuel PRODUCTION at the
// same nominal rate, so the over-placement reads in one unit.
//
// TWO READS per seed, the world built as market_viability builds it
// (build_app_start_world, the 12-tick settle, seat_player_corporation).
// A PURE READER.
//
// Usage (repo root): build_gen/verify/fuel_placement_probe.exe [--seeds a,b] [--good name]
// Build:  ./tools/verify/build_lua_harness.sh fuel_placement_probe
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/resource_names.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

std::string gname(std::size_t g) { return resource_names::name_of(static_cast<resource_type>(g)); }

struct tally
{
    std::map<std::string, int> by_path;          // path -> count
    std::map<std::string, int> by_recipe;        // recipe -> count
    std::map<std::string, int> attached_for;     // charter:attached -> the firm's gap good
    int switched = 0;                            // recipe at GEN did not make the good
    int total = 0;
    int consumers = 0;                           // processors with the good as an input
    double want_final = 0, want_upkeep = 0, want_constr = 0, want_input = 0, production = 0;
};

void add(std::map<std::string, int>& m, const std::string& k, int n = 1) { m[k] += n; }

void read_world(const char* label, world& w, const recipe_registry& reg, std::size_t good,
                const std::set<entity_id>* gen_ids,
                const std::map<entity_id, uint16_t>& gen_recipe,
                const std::map<entity_id, const charter_record*>& charter_of,
                tally& T)
{
    std::map<entity_id, entity_id> owner;
    for (const auto& [cid, cc] : w.corporations)
        for (const entity_id a : cc.assets) owner.emplace(a, cid);

    std::vector<entity_id> ids;
    for (const auto& kv : w.buildings) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());

    tally t;
    for (const entity_id bid : ids)
    {
        const building_component& b = w.buildings.at(bid);
        if (b.type != building_type::processing_facility)
            continue;
        const recipe* rc = reg.get_recipe(b.recipe);
        if (!rc)
            continue;
        if (b.decommissioned)
        {
            if (rc->outputs[good] > 0.0f)
                add(t.by_path, "(decommissioned)");
            continue;
        }
        if (rc->inputs[good] > 0.0f)
            ++t.consumers;
        if (!(rc->outputs[good] > 0.0f))
            continue;
        if (b.ticks_remaining > 0)
            add(t.by_path, "(of which under construction)");
        ++t.total;
        add(t.by_recipe, rc->name);
        const auto git = gen_recipe.find(bid);
        if (git != gen_recipe.end() && git->second != b.recipe)
        {
            const recipe* g0 = reg.get_recipe(git->second);
            if (!(g0 && g0->outputs[good] > 0.0f))
                ++t.switched;
        }
        std::string path;
        const auto oit = owner.find(bid);
        const bool settle_built = gen_ids && !gen_ids->count(bid);
        if (oit == owner.end())
            path = "unowned";
        else
        {
            const corporation_component& cc = w.corporations.at(oit->second);
            const auto cit = charter_of.find(oit->second);
            if (cc.is_player)
                path = "seat";
            else if (!cc.is_background)
                path = "specialist";
            else if (cit == charter_of.end())
                path = "pass6";
            else if (cit->second->good == good)
                path = "charter:fuel";
            else
            {
                path = std::string("charter:attached(") +
                       (cc.focus == industrial_focus::extraction ? "extraction" :
                        cc.focus == industrial_focus::processing ? "processing" : "trade") + ")";
                add(t.attached_for, cit->second->good < resource_count
                                        ? gname(cit->second->good) : std::string("?"));
            }
        }
        if (settle_built)
            path = "settle-built/" + path;
        add(t.by_path, path);
    }

    // The want, the generation way (corporation_generation.cpp's file-local
    // body_* readers, restated): final demand per body, operating buildings'
    // upkeep basket, processors' inputs at nominal batches. Construction
    // demand is construction capacity only, so it is 0 for any other good.
    std::set<entity_id> bodies;
    for (const auto& [bid, b] : w.buildings)
    {
        const auto tit = w.tiles.find(b.tile);
        if (tit != w.tiles.end()) bodies.insert(tit->second.body);
    }
    for (const entity_id body : bodies)
    {
        t.want_final += measure_body_demand(w, reg, body)[good];
        t.production += measure_body_production(w, reg, body)[good];
    }
    const float batches = reg.economics(building_type::processing_facility).base_rate * 0.5f;
    for (const entity_id bid : ids)
    {
        const building_component& b = w.buildings.at(bid);
        if (b.decommissioned)
            continue;
        if (b.ticks_remaining <= 0 && static_cast<std::size_t>(b.type) < building_type_count)
            t.want_upkeep += building_upkeep_goods(reg.building_upkeep(), b.type, reg.era())[good];
        if (b.type == building_type::processing_facility)
            if (const recipe* rc = reg.get_recipe(b.recipe))
                t.want_input += batches * rc->inputs[good];
    }

    std::printf("  %s: %d %s makers (switched in %d), %d consumers | want final %.1f upkeep %.1f "
                "construction %.1f inputs %.1f = %.1f | nominal production %.1f\n",
                label, t.total, gname(good).c_str(), t.switched, t.consumers, t.want_final,
                t.want_upkeep, t.want_constr, t.want_input,
                t.want_final + t.want_upkeep + t.want_constr + t.want_input, t.production);
    std::printf("    by path:");
    for (const auto& [k, n] : t.by_path) std::printf(" %s %d", k.c_str(), n);
    std::printf("\n    by recipe:");
    for (const auto& [k, n] : t.by_recipe) std::printf(" %s %d", k.c_str(), n);
    if (!t.attached_for.empty())
    {
        std::printf("\n    attached to firms chartered for:");
        for (const auto& [k, n] : t.attached_for) std::printf(" %s %d", k.c_str(), n);
    }
    std::printf("\n");

    T.total += t.total; T.switched += t.switched; T.consumers += t.consumers;
    T.want_final += t.want_final; T.want_upkeep += t.want_upkeep; T.want_constr += t.want_constr;
    T.want_input += t.want_input; T.production += t.production;
    for (const auto& [k, n] : t.by_path) add(T.by_path, k, n);
    for (const auto& [k, n] : t.by_recipe) add(T.by_recipe, k, n);
    for (const auto& [k, n] : t.attached_for) add(T.attached_for, k, n);
}

void print_pool(const char* label, const tally& T, std::size_t good, int nseeds)
{
    std::printf("POOLED %s over %d seeds: %d %s makers (switched in %d), %d consumers | want %.1f "
                "(final %.1f upkeep %.1f construction %.1f inputs %.1f) | production %.1f\n",
                label, nseeds, T.total, gname(good).c_str(), T.switched, T.consumers,
                T.want_final + T.want_upkeep + T.want_constr + T.want_input, T.want_final,
                T.want_upkeep, T.want_constr, T.want_input, T.production);
    std::printf("  by path:");
    for (const auto& [k, n] : T.by_path) std::printf(" %s %d", k.c_str(), n);
    std::printf("\n  by recipe:");
    for (const auto& [k, n] : T.by_recipe) std::printf(" %s %d", k.c_str(), n);
    std::printf("\n  attached to firms chartered for:");
    for (const auto& [k, n] : T.attached_for) std::printf(" %s %d", k.c_str(), n);
    std::printf("\n");
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds = {46, 28, 11, 31, 40, 12, 37, 13, 41, 43, 32, 10, 25, 38, 9, 0};
    std::size_t good = static_cast<std::size_t>(resource_type::refined_fuel);
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc)
        {
            seeds.clear();
            std::string s = argv[++i];
            std::size_t a = 0;
            while (a <= s.size())
            {
                const std::size_t b = s.find(',', a);
                const std::string tok = s.substr(a, b == std::string::npos ? std::string::npos : b - a);
                if (!tok.empty()) seeds.push_back(static_cast<std::uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
                if (b == std::string::npos) break;
                a = b + 1;
            }
        }
        else if (!std::strcmp(argv[i], "--good") && i + 1 < argc)
        {
            const std::string n = argv[++i];
            bool found = false;
            for (std::size_t g = 0; g < resource_count; ++g)
                if (gname(g) == n) { good = g; found = true; }
            if (!found) { std::fprintf(stderr, "unknown good %s\n", n.c_str()); return 2; }
        }
        else { std::fprintf(stderr, "usage: fuel_placement_probe [--seeds a,b] [--good name]\n"); return 2; }
    }
    std::printf("fuel_placement_probe - BL-1217 D6: which path places the %s makers\n", gname(good).c_str());
    tally TG, TH;
    int n = 0;
    for (const std::uint32_t seed : seeds)
    {
        lua_state lua;
        world_params wp;
        wp.seed = seed;
        auto start = std::make_unique<app_start_world>();
        charter_spend_report rep;
        harness_charter_input ci;
        ci.report = &rep;
        try { build_app_start_world(lua, wp, *start, ci); }
        catch (const std::exception& e) { std::printf("seed %u: build threw %s\n", seed, e.what()); continue; }
        world& w = start->w;
        const recipe_registry& reg = start->reg;
        std::map<entity_id, const charter_record*> charter_of;
        for (const charter_record& cr : rep.charters) charter_of[cr.corp] = &cr;
        std::set<entity_id> gen_ids;
        std::map<entity_id, uint16_t> gen_recipe;
        for (const auto& kv : w.buildings) { gen_ids.insert(kv.first); gen_recipe[kv.first] = kv.second.recipe; }
        std::printf("seed %u (%zu charters, %zu corporations)\n", seed, rep.charters.size(),
                    w.corporations.size());
        {
            // Building census by type and state at GEN (decommissioned, under
            // construction, processor without a recipe).
            std::map<int, std::array<int, 4>> census;
            for (const auto& kv : w.buildings)
            {
                auto& c = census[static_cast<int>(kv.second.type)];
                ++c[0];
                if (kv.second.decommissioned) ++c[1];
                if (kv.second.ticks_remaining > 0) ++c[2];
                if (kv.second.type == building_type::processing_facility && kv.second.recipe == no_recipe) ++c[3];
            }
            std::printf("  GEN census (type: n decom constr norecipe):");
            for (const auto& [t, c] : census)
                std::printf("  %d: %d %d %d %d", t, c[0], c[1], c[2], c[3]);
            std::printf("\n");
        }
        read_world("GEN", w, reg, good, nullptr, gen_recipe, charter_of, TG);
        // A plant the settle builds is remembered with the recipe it first
        // carried, so "switched in" counts a settle plant retooled into the good.
        for (int step = 0; step < k_campaign_settle_ticks; ++step)
        {
            (void)run_settle_tick(w, reg, step, 0, true);
            for (const auto& kv : w.buildings)
                gen_recipe.emplace(kv.first, kv.second.recipe);
        }
        seat_player_corporation(w, seed, start->land.search.winner_score);
        read_world("HANDOFF", w, reg, good, &gen_ids, gen_recipe, charter_of, TH);
        {
            // What became of the GEN makers: still standing (any recipe), gone.
            int gone = 0, kept = 0, recipe_out = 0, gen_makers = 0, gen_survive_all = 0;
            for (const auto& [bid, rid] : gen_recipe)
            {
                if (!gen_ids.count(bid))
                    continue; // a settle-built plant: not a GEN maker

                const auto bit = w.buildings.find(bid);
                if (bit != w.buildings.end()) ++gen_survive_all;
                const recipe* r0 = reg.get_recipe(rid);
                if (!(r0 && r0->outputs[good] > 0.0f)) continue;
                ++gen_makers;
                if (bit == w.buildings.end()) { ++gone; continue; }
                ++kept;
                const recipe* r1 = reg.get_recipe(bit->second.recipe);
                if (!(r1 && r1->outputs[good] > 0.0f)) ++recipe_out;
            }
            std::printf("    GEN makers %d: standing at handoff %d (recipe switched out %d), gone %d | GEN buildings %zu, standing %d, handoff buildings %zu\n",
                        gen_makers, kept, recipe_out, gone, gen_ids.size(), gen_survive_all, w.buildings.size());
        }
        ++n;
        std::fflush(stdout);
    }
    print_pool("GEN", TG, good, n);
    print_pool("HANDOFF", TH, good, n);
    return 0;
}
