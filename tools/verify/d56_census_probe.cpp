// ---------------------------------------------------------------------------
// d56_census_probe — BL-1217 D5/D6 fix round: what the two rulings change
// ---------------------------------------------------------------------------
// A PURE READER over the world market_viability builds (build_app_start_world,
// the 12-tick settle, seat_player_corporation, play ticks as
// run_app_live_window steps them), read at GEN, HANDOFF and play tick 50:
//   * processors BUILT (complete, not decommissioned) and RUNNING (the tick's
//     report row active) by recipe — so a before/after pair names which
//     running plants a change removes;
//   * refined_fuel and propellant makers, built / running / under construction;
//   * specialists (not background), the seat;
//   * launchpads, and how many can fuel a launch (the owner's pool at the
//     pad's market holds >= 1 propellant, the per-launch draw) at t50 and at
//     any play tick up to 50; space-mode convoys dispatched by t50.
// Pooled by sum over seeds. Usage: d56_census_probe.exe [--seeds a,b] [--ticks N]
// Build: ./tools/verify/build_lua_harness.sh d56_census_probe
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
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

struct reading
{
    std::map<std::string, int> built, running;   // by recipe
    int built_all = 0, running_all = 0, constr = 0;
    int fuel_built = 0, fuel_run = 0, fuel_constr = 0;
    int prop_built = 0, prop_run = 0, prop_constr = 0;
    int specialists = 0, players = 0, corps = 0;
    int pads = 0, pads_fuelled = 0;
};

void read(world& w, const recipe_registry& reg, const economy_report* rep, reading& R)
{
    std::set<entity_id> active;
    if (rep) for (const building_report& br : rep->buildings) if (br.active) active.insert(br.building);
    const std::size_t fuel = static_cast<std::size_t>(resource_type::refined_fuel);
    const std::size_t prop = static_cast<std::size_t>(resource_type::propellant);
    std::map<entity_id, entity_id> owner;
    for (const auto& [cid, cc] : w.corporations)
    {
        ++R.corps;
        if (!cc.is_background) ++R.specialists;
        if (cc.is_player) ++R.players;
        for (const entity_id a : cc.assets) owner.emplace(a, cid);
    }
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type == building_type::launchpad && !b.decommissioned && b.ticks_remaining <= 0)
        {
            ++R.pads;
            const auto oit = owner.find(bid);
            if (oit != owner.end())
                if (const stockpile_component* p = w.find_pool(oit->second, pool_key_for_tile(w, b.tile)))
                    if (p->quantities[prop] >= 1.0f) ++R.pads_fuelled;
        }
        if (b.type != building_type::processing_facility || b.decommissioned) continue;
        const recipe* rc = reg.get_recipe(b.recipe);
        const bool f = rc && rc->outputs[fuel] > 0.0f, pr = rc && rc->outputs[prop] > 0.0f;
        if (b.ticks_remaining > 0)
        {
            ++R.constr;
            if (f) ++R.fuel_constr;
            if (pr) ++R.prop_constr;
            continue;
        }
        const std::string name = rc ? rc->name : std::string("(none)");
        ++R.built_all; ++R.built[name];
        if (f) ++R.fuel_built;
        if (pr) ++R.prop_built;
        if (active.count(bid))
        {
            ++R.running_all; ++R.running[name];
            if (f) ++R.fuel_run;
            if (pr) ++R.prop_run;
        }
    }
}

void add(reading& P, const reading& r)
{
    for (const auto& [k, n] : r.built) P.built[k] += n;
    for (const auto& [k, n] : r.running) P.running[k] += n;
    P.built_all += r.built_all; P.running_all += r.running_all; P.constr += r.constr;
    P.fuel_built += r.fuel_built; P.fuel_run += r.fuel_run; P.fuel_constr += r.fuel_constr;
    P.prop_built += r.prop_built; P.prop_run += r.prop_run; P.prop_constr += r.prop_constr;
    P.specialists += r.specialists; P.players += r.players; P.corps += r.corps;
    P.pads += r.pads; P.pads_fuelled += r.pads_fuelled;
}

void print(const char* label, const reading& r, bool by_recipe)
{
    std::printf("  %s: processors built %d running %d (constr %d) | refined_fuel built %d run %d constr %d"
                " | propellant built %d run %d constr %d | corps %d specialists %d players %d"
                " | pads %d fuelled %d\n",
                label, r.built_all, r.running_all, r.constr, r.fuel_built, r.fuel_run, r.fuel_constr,
                r.prop_built, r.prop_run, r.prop_constr, r.corps, r.specialists, r.players,
                r.pads, r.pads_fuelled);
    if (!by_recipe) return;
    std::printf("    running/built by recipe:");
    for (const auto& [k, n] : r.built)
    {
        const auto it = r.running.find(k);
        std::printf(" %s %d/%d", k.c_str(), it == r.running.end() ? 0 : it->second, n);
    }
    std::printf("\n");
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds = {46, 28, 11, 31, 40, 12, 37, 13, 41, 43, 32, 10, 25, 38, 9, 0};
    int ticks = 50;
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
        else if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::atoi(argv[++i]);
        else { std::fprintf(stderr, "usage: d56_census_probe [--seeds a,b] [--ticks N]\n"); return 2; }
    }
    std::printf("d56_census_probe - BL-1217 D5/D6: processors by recipe, fuel/propellant, pads, specialists\n");
    reading PG, PH, PT;
    int pads_ever = 0, space_convoys = 0;
    for (const std::uint32_t seed : seeds)
    {
        lua_state lua;
        world_params wp;
        wp.seed = seed;
        auto start = std::make_unique<app_start_world>();
        try { build_app_start_world(lua, wp, *start); }
        catch (const std::exception& e) { std::printf("seed %u: build threw %s\n", seed, e.what()); continue; }
        world& w = start->w;
        const recipe_registry& reg = start->reg;
        std::printf("seed %u\n", seed);
        reading g; read(w, reg, nullptr, g); print("GEN", g, true); add(PG, g);
        economy_report last;
        for (int step = 0; step < k_campaign_settle_ticks; ++step)
        {
            settle_tick_result r = run_settle_tick(w, reg, step, 0, true);
            if (step == k_campaign_settle_ticks - 1) last = std::move(r.report);
        }
        seat_player_corporation(w, seed, start->land.search.winner_score);
        reading h; read(w, reg, &last, h); print("HANDOFF", h, true); add(PH, h);
        constexpr int k_econ_tick_days = 90;
        std::set<std::uint32_t> seen;
        for (const convoy_component& c : w.convoys) seen.insert(c.id);
        int fuelled_ever = 0;
        economy_report rep;
        for (int k = 1; k <= ticks; ++k)
        {
            const int day = k * k_econ_tick_days;
            advance_orbits(w, static_cast<double>(k_econ_tick_days));
            advance_surveys(w, k_econ_tick_days);
            w.current_day_tick = day;
            settle_tick_result r = run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), day, false);
            for (const convoy_component& c : w.convoys)
                if (seen.insert(c.id).second && c.mode == convoy_mode::space) ++space_convoys;
            reading tmp; read(w, reg, nullptr, tmp);
            fuelled_ever = std::max(fuelled_ever, tmp.pads_fuelled);
            rep = std::move(r.report);
        }
        reading t; read(w, reg, &rep, t); print("T50", t, true); add(PT, t);
        pads_ever += fuelled_ever;
        std::fflush(stdout);
    }
    std::printf("POOLED over %zu seeds\n", seeds.size());
    print("GEN", PG, true);
    print("HANDOFF", PH, true);
    print("T50", PT, true);
    std::printf("  pads fuelled at some play tick <= %d (max per seed, summed): %d | space convoys dispatched: %d\n",
                ticks, pads_ever, space_convoys);
    return 0;
}
