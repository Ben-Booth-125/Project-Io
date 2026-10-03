// working_capital — BL-1173: every background firm opens with working capital.
//
// FINANCE.md § Debt interest (Ben, 2026-10-03): "a background firm opens with
// working capital priced from its opening stock, not at zero ... cash is one
// quarter of the stock's value, each good at its base price in the market the
// stock is pooled in ... minted at generation". Background firms only: the
// seat and the specialists keep `base_capital`.
//
// Built on the SHIPPED START (`build_app_start_world`, harness_params.hpp's
// app-order mirror), read the moment generation hands the world over — before
// the settle's first tick has spent or earned anything.
//
// RECOMPUTED, NOT CALLED. The expected cash is summed here from the world's own
// pools and base prices, independently of `background_working_capital`; only
// the fraction is restated (k_fraction below), and a retune moves it here too.
//
// Rows:
//   W1  every background firm's starting_capital AND balance equal
//       0.25 x sum(pool qty x base) over its pools (relative tolerance 1e-4).
//   W2  the field is really funded: at least one background firm opens > 0.
//   W3  SPECIALISTS KEEP base_capital: every non-background corporation (the
//       seat among them) opens on `compute_capital`'s band — base_capital x
//       [1 - variance, (1 + variance) x 1.15], read from corporation_params'
//       own defaults — with balance == starting_capital, and NOT at the
//       working-capital figure its own stock would price to. A rule that
//       handed specialists working capital (instead of, or on top of,
//       base_capital) fails one of the three.
//
// Build: bash tools/verify/build_lua_harness.sh working_capital --run
// Args:  --seeds a,b,c (default 0)   --full (prehistory on; default off, fast)

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
int g_pass = 0;

void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

/// `k_background_working_capital_of_stock` (corporation_generation.cpp), restated.
constexpr double k_fraction = 0.25;

/// The base-price table a pool is valued at: its market's, or — for a
/// body-level key — the lowest-id market on that body; none, nothing.
const market_component* pricing_market(const world& w, entity_id key)
{
    if (const auto it = w.markets.find(key); it != w.markets.end())
        return &it->second;
    entity_id best = null_entity;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == key && (best == null_entity || mid < best))
            best = mid;
    return best == null_entity ? nullptr : &w.markets.at(best);
}

void run_seed(lua_state& lua, uint32_t seed, bool full)
{
    std::printf("\n-- seed %u (%s) --\n", seed, full ? "prehistory" : "fast");
    world_params p;
    p.seed = seed;
    if (!full)
        p = no_prehistory(p);
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, p, *start);
    const world& w = start->w;

    int background = 0, funded = 0, wrong = 0;
    double total = 0.0;
    for (const auto& [cid, cc] : w.corporations)
    {
        if (!cc.is_background)
            continue;
        ++background;
        double value = 0.0;
        for (const auto& [key, pool] : w.corp_market_pools)
        {
            if (key.first != cid)
                continue;
            const market_component* m = pricing_market(w, key.second);
            if (m == nullptr)
                continue;
            for (std::size_t r = 0; r < resource_count; ++r)
                if (pool.quantities[r] > 0.0f && m->base_price[r] > 0.0f)
                    value += static_cast<double>(pool.quantities[r]) * m->base_price[r];
        }
        const double want = value * k_fraction;
        const double tol  = std::max(1e-3, std::fabs(want) * 1e-4);
        const bool ok = std::fabs(cc.starting_capital - want) <= tol &&
                        std::fabs(cc.balance - want) <= tol;
        if (!ok)
        {
            if (wrong < 5)
                std::printf("    corp %u: starting %.3f balance %.3f, want %.3f\n",
                            static_cast<unsigned>(cid), cc.starting_capital, cc.balance, want);
            ++wrong;
        }
        if (cc.starting_capital > 0.0f)
            ++funded;
        total += cc.starting_capital;
    }
    std::printf("  background firms %d, funded %d, total working capital %.0f (mean %.1f)\n",
                background, funded, total, background ? total / background : 0.0);

    check(background > 0, "W0 the shipped start charters background firms at all");
    check(wrong == 0, "W1 every background firm opens with 0.25 x its stock at base, balance and starting_capital alike");
    check(funded > 0, "W2 the field is funded: background firms open above zero");

    // W3 — specialists keep base_capital.
    const corporation_params cp{};
    const double lo = cp.base_capital * (1.0 - cp.wealth_variance);
    const double hi = cp.base_capital * (1.0 + cp.wealth_variance) * 1.15;
    int specialists = 0, out_of_band = 0, drifted = 0, priced_as_wc = 0, stocked = 0;
    for (const auto& [cid, cc] : w.corporations)
    {
        if (cc.is_background)
            continue;
        ++specialists;
        if (cc.starting_capital < lo - 1e-3 || cc.starting_capital > hi + 1e-3)
            ++out_of_band;
        if (std::fabs(cc.balance - cc.starting_capital) > 1e-3f)
            ++drifted;
        double value = 0.0;
        for (const auto& [key, pool] : w.corp_market_pools)
        {
            if (key.first != cid)
                continue;
            if (const market_component* m = pricing_market(w, key.second))
                for (std::size_t r = 0; r < resource_count; ++r)
                    if (pool.quantities[r] > 0.0f && m->base_price[r] > 0.0f)
                        value += static_cast<double>(pool.quantities[r]) * m->base_price[r];
        }
        if (value > 0.0)
        {
            ++stocked;
            if (std::fabs(cc.starting_capital - value * k_fraction) <= std::max(1e-3, value * k_fraction * 1e-4))
                ++priced_as_wc;
        }
    }
    std::printf("  specialists %d (stocked %d); base_capital band [%.0f, %.0f]; seat %u\n",
                specialists, stocked, lo, hi, static_cast<unsigned>(w.player_entity));
    check(specialists > 0 && stocked > 0, "W3 the start charters specialists holding stock (the row is not vacuous)");
    check(out_of_band == 0, "W3 every specialist opens inside base_capital's band");
    check(drifted == 0, "W3 ... with balance == starting_capital: nothing minted on top");
    check(priced_as_wc == 0, "W3 ... and none at the working-capital figure its stock would price to");
    const auto pit = w.corporations.find(w.player_entity);
    check(pit != w.corporations.end() && !pit->second.is_background,
          "W3 the seat is a specialist, so the rows above cover it");
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds = {0};
    bool full = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--full") == 0)
            full = true;
        else if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            seeds.clear();
            std::stringstream ss(argv[++i]);
            std::string t;
            while (std::getline(ss, t, ','))
                if (!t.empty())
                    seeds.push_back(static_cast<uint32_t>(std::strtoul(t.c_str(), nullptr, 10)));
        }
    }

    std::printf("=== working_capital — BL-1173, background firms open with working capital ===\n");
    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    for (const uint32_t s : seeds)
        run_seed(lua, s, full);

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
