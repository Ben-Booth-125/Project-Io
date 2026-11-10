// price_vanish_probe.cpp - BL-1618 (price falls as supply vanishes). A PROBE: seed 31's
// blockaded market, one good, per tick: price, base, band, listings, shelf, demand, the
// silenced want, the supply the price law reads and the target it aims at.
//
//   build_gen/verify/price_vanish_probe.exe [--seed S] [--market M] [--good G] [--ticks T]
//
// Build: bash tools/verify/build_lua_harness.sh price_vanish_probe. Run from the repo root.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/campaign_settle.hpp"
#include "world/market_clearing.hpp"
#include "world/orbital_system.hpp"
#include "world/spawn_seat.hpp"
#include "world/survey_system.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

int main(int argc, char** argv)
{
    std::uint32_t seed = 31; entity_id market = 47921; int good = 30; int last_tick = 40;
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seed"))   seed = (std::uint32_t)std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--market")) market = (entity_id)std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--good"))   good = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--ticks"))  last_tick = std::atoi(argv[++i]);
    }
    lua_state lua;
    world_params wp; wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, wp, *start);
    world& w = start->w;
    const recipe_registry& reg = start->reg;
    const auto& band = reg.price_band();
    const auto row = [&](const char* tag, int step) {
        const auto it = w.markets.find(market);
        if (it == w.markets.end()) { std::printf("%s %d: no market\n", tag, step); return; }
        const market_component& m = it->second;
        const std::size_t r = (std::size_t)good;
        const float ps = pricing_supply(m, r, band.shelf_supply_ticks);
        const float tg = price_target(m.base_price[r], ps, m.demand[r], band.floor_mult, band.ceil_mult);
        std::printf("%s %3d price %8.2f base %7.2f lo %7.2f hi %8.2f | listed %6.2f shelf %8.2f demand %6.2f want %6.2f | psupply %7.2f target %8.2f\n",
                    tag, step, m.price[r], m.base_price[r], m.base_price[r] * band.floor_mult, m.base_price[r] * band.ceil_mult,
                    m.supply[r], m.inventory[r], m.demand[r], m.hauler_want[r], ps, tg);
    };
    std::printf("band floor %.2f ceil %.2f k %.2f\n", band.floor_mult, band.ceil_mult, band.shelf_supply_ticks);
    row("gen", -1);
    for (int step = 0; step < k_campaign_settle_ticks; ++step)
    {
        (void)run_settle_tick(w, reg, step, 0, true);
        row("settle", step);
    }
    clear_research_at_handover(w);
    seat_player_corporation(w, seed, start->land.search.winner_score);
    constexpr int k_days = 90;
    for (int step = k_campaign_settle_ticks, k = 1; step <= last_tick; ++step, ++k)
    {
        advance_orbits(w, (double)k_days);
        advance_surveys(w, k_days);
        w.current_day_tick = k * k_days;
        (void)run_settle_tick(w, reg, step, k * k_days, false);
        bool closed = false;
        for (const entity_id t : w.closed_ports) if (market_for_tile(w, t) == market) closed = true;
        row(closed ? "CLOSED" : "tick", step);
    }
    return 0;
}
