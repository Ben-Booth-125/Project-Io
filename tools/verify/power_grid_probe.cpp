// ---------------------------------------------------------------------------
// power_grid_probe — BL-1230 (power crosses markets), review round 1 measure
// ---------------------------------------------------------------------------
// QUESTION. The grid draw (run_building_upkeep) reads a building's grid off its
// PROVINCE, but places a shelf of power on the grid of its MARKET CENTRE's
// province — power has no province once a generator's output lands in its
// owner's (corp, market) pool. Where a building's province grid and its
// market centre's grid differ, the building cannot reach the generators beside
// it; where a generator's province grid and its market centre's grid differ,
// its power is offered on the wrong grid; a market centre in a dark province
// is off every grid. How much power need does that touch? And (item 4) how many
// wired buildings sit on a grid with NO live generator on it — wired, so they
// bid and decay, but with nothing to draw?
//
// THE WORLD. Seated as market_viability / mine_upkeep_probe seat it:
// build_app_start_world, the 12-tick settle, seat_player_corporation, then PLAY
// ticks through run_settle_tick. Read at handoff (play tick 0) and tick 50.
//
// Usage (repo root): build_gen/verify/power_grid_probe.exe [--seeds a,b] [--ticks N]
// Build:  ./tools/verify/build_lua_harness.sh power_grid_probe
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
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

constexpr std::size_t k_power = static_cast<std::size_t>(resource_type::power);

struct reading
{
    long markets = 0, markets_dark = 0;
    // power-drawing buildings (live, basket power > 0)
    long b_all = 0, b_dark = 0, b_match = 0, b_mkt_dark = 0, b_mismatch = 0;
    double n_all = 0, n_dark = 0, n_match = 0, n_mkt_dark = 0, n_mismatch = 0;
    // generators (live processors whose recipe outputs power), output per tick at full run
    long g_all = 0, g_dark = 0, g_match = 0, g_mkt_dark = 0, g_mismatch = 0;
    double o_all = 0, o_dark = 0, o_match = 0, o_mkt_dark = 0, o_mismatch = 0;
    // item 4: wired buildings on a grid with no live generator (province reading)
    long b_nogen = 0; double n_nogen = 0;
    // ... and on a grid with no market centre whose catchment holds a generator
    // (the reading the shipped clear actually uses)
    long b_noshelfgen = 0; double n_noshelfgen = 0;
    long grids = 0, grids_with_gen = 0;
};

reading read(world& w, const recipe_registry& reg)
{
    reading R;
    const building_upkeep_params& up = reg.building_upkeep();
    std::map<entity_id, std::uint32_t> mgrid;
    for (const auto& [mid, mc] : w.markets)
    {
        ++R.markets;
        const std::uint32_t g = (mc.centre_tile != null_entity) ? tile_power_grid(w, mc.centre_tile) : 0;
        mgrid[mid] = g;
        if (g == 0) ++R.markets_dark;
    }
    std::set<std::uint32_t> gen_grids, gen_market_grids, all_grids;
    for (const auto& [pid, g] : province_power_grid(w)) { (void)pid; all_grids.insert(g); }
    R.grids = static_cast<long>(all_grids.size());

    struct live { entity_id bid; std::uint32_t pg, mg; float need; };
    std::vector<live> drawers;
    std::vector<std::pair<entity_id, entity_id>> sorted;
    for (const auto& [bid, b] : w.buildings) sorted.push_back({bid, bid});
    std::sort(sorted.begin(), sorted.end());
    for (const auto& kv : sorted)
    {
        const building_component& b = w.buildings.at(kv.first);
        if (b.ticks_remaining > 0 || b.decommissioned) continue;
        const std::uint32_t pg = tile_power_grid(w, b.tile);
        const entity_id mid = market_for_tile(w, b.tile);
        const std::uint32_t mg = (mid != null_entity) ? mgrid[mid] : 0;

        // a generator?
        if (b.type == building_type::processing_facility)
            if (const recipe* rc = reg.get_recipe(b.recipe); rc && rc->outputs[k_power] > 0.0f)
            {
                const double o = rc->outputs[k_power];
                ++R.g_all; R.o_all += o;
                if (pg == 0) { ++R.g_dark; R.o_dark += o; }
                else if (mg == 0) { ++R.g_mkt_dark; R.o_mkt_dark += o; }
                else if (pg == mg) { ++R.g_match; R.o_match += o; }
                else { ++R.g_mismatch; R.o_mismatch += o; }
                if (pg != 0) gen_grids.insert(pg);
                if (mg != 0) gen_market_grids.insert(mg);
            }

        const std::array<float, resource_count> bk = building_upkeep_goods(up, b.type, reg.era());
        const float need = bk[k_power];
        if (!(need > 0.0f)) continue;
        drawers.push_back({kv.first, pg, mg, need});
    }
    R.grids_with_gen = static_cast<long>(gen_grids.size());
    for (const live& d : drawers)
    {
        ++R.b_all; R.n_all += d.need;
        if (d.pg == 0) { ++R.b_dark; R.n_dark += d.need; continue; }
        if (d.mg == 0) { ++R.b_mkt_dark; R.n_mkt_dark += d.need; }
        else if (d.pg == d.mg) { ++R.b_match; R.n_match += d.need; }
        else { ++R.b_mismatch; R.n_mismatch += d.need; }
        if (!gen_grids.count(d.pg)) { ++R.b_nogen; R.n_nogen += d.need; }
        if (!gen_market_grids.count(d.pg)) { ++R.b_noshelfgen; R.n_noshelfgen += d.need; }
    }
    return R;
}

void add(reading& a, const reading& b)
{
    a.markets += b.markets; a.markets_dark += b.markets_dark;
    a.b_all += b.b_all; a.b_dark += b.b_dark; a.b_match += b.b_match; a.b_mkt_dark += b.b_mkt_dark; a.b_mismatch += b.b_mismatch;
    a.n_all += b.n_all; a.n_dark += b.n_dark; a.n_match += b.n_match; a.n_mkt_dark += b.n_mkt_dark; a.n_mismatch += b.n_mismatch;
    a.g_all += b.g_all; a.g_dark += b.g_dark; a.g_match += b.g_match; a.g_mkt_dark += b.g_mkt_dark; a.g_mismatch += b.g_mismatch;
    a.o_all += b.o_all; a.o_dark += b.o_dark; a.o_match += b.o_match; a.o_mkt_dark += b.o_mkt_dark; a.o_mismatch += b.o_mismatch;
    a.b_nogen += b.b_nogen; a.n_nogen += b.n_nogen; a.b_noshelfgen += b.b_noshelfgen; a.n_noshelfgen += b.n_noshelfgen;
    a.grids += b.grids; a.grids_with_gen += b.grids_with_gen;
}

double pc(double a, double b) { return b > 0 ? 100.0 * a / b : 0.0; }

void print(const char* tag, const reading& R)
{
    std::printf("  %-8s markets %ld, centre in a dark province %ld | grids %ld, with a live generator %ld\n",
                tag, R.markets, R.markets_dark, R.grids, R.grids_with_gen);
    std::printf("           power drawers %ld (need %.1f/tick): dark %ld (%.1f%% need) | wired: same grid as market centre %ld (%.1f%%), market centre dark %ld (%.1f%%), DIFFERENT grid %ld (%.1f%%)\n",
                R.b_all, R.n_all, R.b_dark, pc(R.n_dark, R.n_all), R.b_match, pc(R.n_match, R.n_all),
                R.b_mkt_dark, pc(R.n_mkt_dark, R.n_all), R.b_mismatch, pc(R.n_mismatch, R.n_all));
    std::printf("           generators %ld (output %.1f/tick): dark %ld (%.1f%% output) | wired: same grid as market centre %ld (%.1f%%), market centre dark %ld (%.1f%%), DIFFERENT grid %ld (%.1f%%)\n",
                R.g_all, R.o_all, R.g_dark, pc(R.o_dark, R.o_all), R.g_match, pc(R.o_match, R.o_all),
                R.g_mkt_dark, pc(R.o_mkt_dark, R.o_all), R.g_mismatch, pc(R.o_mismatch, R.o_all));
    std::printf("           item 4: wired drawers on a grid with NO live generator (province reading) %ld (%.1f%% of drawers, %.1f%% of need)"
                " | on a grid no generator's market centre is on (shipped reading) %ld (%.1f%% of need)\n",
                R.b_nogen, pc(R.b_nogen, R.b_all), pc(R.n_nogen, R.n_all), R.b_noshelfgen, pc(R.n_noshelfgen, R.n_all));
}

} // namespace

int main(int argc, char** argv)
{
    int ticks = 50;
    std::vector<std::uint32_t> seeds = {0, 43, 10, 28, 38};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::max(1, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc)
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
        else { std::fprintf(stderr, "usage: power_grid_probe [--seeds a,b] [--ticks N]\n"); return 2; }
    }
    std::printf("power_grid_probe - BL-1230: province grid vs market-centre grid, and wired grids with no generator\n");

    reading pool0, poolT;
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
        for (int step = 0; step < k_campaign_settle_ticks; ++step) run_settle_tick(w, reg, step, 0, true);
        seat_player_corporation(w, seed, start->land.search.winner_score);

        std::printf("\n=== seed %u ===\n", seed);
        const reading r0 = read(w, reg);
        print("handoff", r0);
        add(pool0, r0);

        constexpr int k_days = 90;
        for (int k = 1; k <= ticks; ++k)
        {
            advance_orbits(w, static_cast<double>(k_days));
            advance_surveys(w, k_days);
            w.current_day_tick = k * k_days;
            (void)run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), k * k_days, false, nullptr);
        }
        const reading rT = read(w, reg);
        char tag[16];
        std::snprintf(tag, sizeof tag, "t%d", ticks);
        print(tag, rT);
        add(poolT, rT);
        std::fflush(stdout);
    }
    std::printf("\n================ POOLED over %zu seeds ================\n", seeds.size());
    print("handoff", pool0);
    char tag[16];
    std::snprintf(tag, sizeof tag, "t%d", ticks);
    print(tag, poolT);
    return 0;
}
