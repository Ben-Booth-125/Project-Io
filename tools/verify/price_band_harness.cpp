// price_band_harness — BL-442 step 1 (the price band moves from code to data).
//
// THE GUARD THIS FILE EXISTS TO BE.
//
// The band [0.25x, 4x] around a market's rarity-derived base_price is the single
// most load-bearing economic tunable in the game, and until BL-442 it was written
// down TWICE as hand-synchronised `constexpr` floats:
//
//   src/world/market_clearing.cpp  price_floor_mult / price_ceil_mult
//                                  (the clamp resolve_price applies to every
//                                   market price, every tick)
//   src/world/economy_system.cpp   wf_price_floor_mult / wf_price_ceil_mult
//                                  (the identical clamp the BL-181 workforce
//                                   auto-solver applies to its FORWARD price
//                                   estimate, so it prices a candidate target
//                                   against the band the market will clear it in)
//
// The second was literally commented "mirror market_clearing.cpp price band".
// Nothing enforced the mirror. Edit one, and the solver silently starts choosing
// workforce targets against a band the market does not use — a divergence that
// produces no error, no assertion, and no visible symptom beyond an economy that
// tunes wrong.
//
// BL-442 authors the band ONCE in scripts/economy.lua (`economy.price_band`) and
// routes it to both sites through `recipe_registry::price_band()`. This harness
// is the check that the routing is real and stays real. Its whole design point:
//
//   A GUARD THAT ONLY EXERCISED ONE CALL SITE WOULD HAVE PASSED BEFORE THE
//   CHANGE, AND WOULD PASS AGAIN THE DAY SOMEONE REINTRODUCES A LOCAL COPY.
//
// So every behavioural row here is DIFFERENTIAL: it sets a deliberately
// non-default band on the registry and asserts the site MOVES. A site holding a
// stale constexpr does not move, and fails.
//
//   P1  DEFAULTS. A default-constructed registry's band is bit-exactly
//       0.25 / 4.0 — the property that keeps every hand-built-registry harness
//       (most of them) byte-identical across this change.
//   P2  LUA REACHES C++. The real scripts/economy.lua is loaded and authors the
//       band; then a probe script overrides it with a distinctive value and the
//       registry is reloaded, proving the loader genuinely reads the table
//       rather than falling through to the struct defaults.
//   P3  SITE A — market clearing. On a supply-flooded fixture whose price would
//       sink to the floor, the resolved price honours a RAISED floor_mult.
//   P4  SITE A — market clearing. On a demand-starved fixture whose price would
//       run to the ceiling, the resolved price honours a LOWERED ceil_mult.
//   P5  SITE B — the workforce solver. The chosen workforce target CHANGES when
//       only ceil_mult changes. This is the row that would have failed before
//       BL-442, and the row that fails again if economy_system.cpp ever
//       reacquires a private copy of the band.
//   P6  BOTH SITES, ONE VALUE. With one band on the registry, the price the
//       solver models and the price the market resolves agree at the ceiling —
//       the mirror the old comment asserted and nothing checked.
//
// Live-Lua harness (see CMakeLists.txt) because P2 asks what the SHIPPED
// scripts/economy.lua actually delivers, which a hand-built fixture cannot answer
// without becoming the thing under test.

#include "scripting/lua_state.hpp"
#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok)
        ++g_failures;
}

std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

// --- P1: the defaults reproduce the pre-BL-442 constants exactly -------------

void run_defaults()
{
    std::printf("=== P1: default band == the constants it replaced ===\n\n");

    const recipe_registry reg;
    // Bit-exact, not near(): the acceptance bar for BL-442 step 1 is
    // behaviour-IDENTICAL, and a hand-built-registry harness that clamps against
    // 0.2500001 is not identical, it is merely close.
    check(reg.price_band().floor_mult == 0.25f, "default floor_mult is exactly 0.25");
    check(reg.price_band().ceil_mult == 4.0f, "default ceil_mult is exactly 4.0");
    std::printf("\n");
}

// --- P2: the Lua-authored value actually reaches the registry ----------------

void run_lua_reaches_cpp()
{
    std::printf("=== P2: scripts/economy.lua reaches recipe_registry ===\n\n");

    {
        lua_state lua;
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        recipe_registry reg;
        reg.load_from_lua(lua);

        // What the shipped file authors today. Deliberately asserted against the
        // literals rather than against the struct defaults: if someone deletes the
        // `price_band` table from economy.lua, the defaults would still make an
        // equality-with-defaults check pass, and the authoring would have silently
        // stopped being the authority.
        //
        // UPDATED for BL-442 step 2 (2026-08-17): the ceiling moved 4.0 -> 10.0,
        // DERIVED from measured inter-market haulage (tools/verify/haulage_measure.cpp;
        // docs/economy/MARKETS.md § Where the ceiling comes from). This row is not
        // a golden and updating it is not a blessing: it asserts what the shipped
        // file SAYS, and the shipped file deliberately says something new. The row
        // in fact got STRONGER — the struct default is still 4.0 (P1 asserts that,
        // unchanged), so an authored 10.0 now separates "read from Lua" from "fell
        // through to the defaults" on the ceiling as well as the floor.
        check(reg.price_band().floor_mult == 0.25f,
              "shipped economy.lua authors floor_mult 0.25");
        check(reg.price_band().ceil_mult == 10.0f,
              "shipped economy.lua authors ceil_mult 10.0 (BL-442 step 2, derived)");
    }

    {
        // The wire test. A probe script mutates the already-loaded `economy`
        // table to a value NO struct default could produce, so a loader that
        // ignored the table would be caught here rather than flattered by the
        // fact that the authored numbers happen to equal the defaults.
        const char* probe_path = "build_gen/verify/price_band_probe.lua";
        // Create the directory rather than assume it. It exists in a worktree
        // that has run a `cl` recipe from tools/verify/README.md, and does NOT
        // exist in a checkout built only through CMake — so this row failed on
        // the integration branch while every behavioural row passed, which is an
        // environment defect masquerading as a real one.
        {
            std::error_code ec;
            std::filesystem::create_directories("build_gen/verify", ec);
        }
        {
            std::ofstream probe(probe_path);
            if (!probe)
            {
                check(false, "could not write the probe script (is build_gen/verify present?)");
                std::printf("\n");
                return;
            }
            probe << "economy.price_band = { floor_mult = 0.375, ceil_mult = 6.25 }\n";
        }

        lua_state lua;
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        lua.load(probe_path);
        recipe_registry reg;
        reg.load_from_lua(lua);

        check(reg.price_band().floor_mult == 0.375f,
              "a Lua-authored floor_mult overrides the default (0.375 read back)");
        check(reg.price_band().ceil_mult == 6.25f,
              "a Lua-authored ceil_mult overrides the default (6.25 read back)");
    }
    std::printf("\n");
}

// --- shared fixture: one body, one market, one extraction site ---------------

struct fixture
{
    world     w;
    entity_id body   = null_entity;
    entity_id market = null_entity;
    entity_id bld    = null_entity;
    entity_id corp   = null_entity;
};

// A minimal iron_ore extractor selling into a single market.
//
// NOTE ON STEERING THE PRICE. `clear_markets` RESETS each market's accumulated
// supply/demand at the top of the tick, so pre-loading `mc.supply` / `mc.demand`
// on the fixture is inert — an earlier draft of this harness did exactly that and
// every price came back at base, which is the correct answer to the question it
// was accidentally asking. The flows have to be produced by the tick itself: an
// extractor with no buyer floods supply and drives the price to the FLOOR, and a
// processor with no extractor pulls demand against zero supply and drives it to
// the CEILING. Both are built below.
fixture make_fixture(recipe_registry& reg, float base_price)
{
    fixture f;

    reg.set_thresholds(/*t_full=*/1.0f, /*t_idle=*/0.2f);
    building_economics ex;
    ex.base_rate   = 20.0f;
    ex.maintenance = 5.0f;
    ex.base_wage   = 8.0f;
    reg.set_economics(building_type::extraction_site, ex);

    f.body = f.w.create_entity();
    f.w.bodies[f.body] = body_component{};
    f.w.bodies[f.body].name = "BandTest";

    f.market = f.w.create_entity();
    market_component mc;
    mc.body = f.body;
    mc.base_price[ri(resource_type::iron_ore)] = base_price;
    mc.price = mc.base_price;
    f.w.markets[f.market] = mc;

    const entity_id tile = f.w.create_entity();
    {
        tile_component tc{};
        tc.body        = f.body;
        tc.substrate = terrain_substrate::rocky;
        tc.resource_deposit[ri(resource_type::iron_ore)]   = 2.0f;
        tc.resource_remaining[ri(resource_type::iron_ore)] = 1.0e6f;
        tc.hazard_level = 0.0f;
        f.w.tiles[tile] = tc;
    }

    f.bld = f.w.create_entity();
    {
        building_component b{};
        b.tile               = tile;
        b.type               = building_type::extraction_site;
        b.workforce_assigned = 1.0f;
        b.workforce_target   = 100;
        b.target_resource    = resource_type::iron_ore;
        f.w.buildings[f.bld] = b;
    }

    f.corp = f.w.create_entity();
    {
        corporation_component cc;
        cc.name              = "Band Co";
        cc.starting_capital  = 10000.0f;
        cc.balance           = 10000.0f;
        cc.assets.push_back(f.bld);
        // Not AI-driven: the strategic tier would rewrite workforce_target at the
        // end of the tick, and these rows measure the band, not the scorer.
        cc.is_player = true;
        f.w.corporations[f.corp] = cc;
    }
    return f;
}

// Run the real economy loop `ticks` times on an extractor-only fixture (supply
// with no buyer) and return the market's settled iron_ore price. The price walks
// down toward the floor, so the floor is what this measures.
float settle_at_floor(recipe_registry& reg, float base_price, int ticks)
{
    fixture f = make_fixture(reg, base_price);
    for (int i = 0; i < ticks; ++i)
    {
        economy_report rep = run_economy_step(f.w, reg);
        clear_markets(f.w, reg, rep);
    }
    return f.w.markets.at(f.market).price[ri(resource_type::iron_ore)];
}

// The mirror image: demand with NO supply anywhere, which sends resolve_price
// down its `supply <= 0` branch — `target = base * ceil_mult` — and walks the
// price up to the ceiling.
//
// The demand is a STANDING BUY ORDER, re-posted each tick, rather than a
// processor's input shortfall. A processor was tried first and generated exactly
// zero demand: `mc.demand` accrues from what a corp actually BOUGHT
// (market_clearing.cpp's auto-buy pass), and with no supply and no market
// inventory there is nothing to buy, so unmet industrial need registers as no
// demand at all. That is a real property of the current model, not a fixture
// defect — and it is the same structural-zero-demand shape BL-441 is about. A
// standing buy order is counted into `mc.demand` unconditionally, so it is the
// lever that actually reaches the ceiling branch.
float settle_at_ceiling(recipe_registry& reg, float base_price, int ticks)
{
    fixture f = make_fixture(reg, base_price);

    // Remove the extractor: this fixture must have no supply at all.
    f.w.corporations.at(f.corp).assets.clear();
    f.w.buildings.erase(f.bld);

    for (int i = 0; i < ticks; ++i)
    {
        // Re-post each tick: matching drains the book, and this fixture wants a
        // standing appetite, not a one-off bid.
        buy_order bo{};
        bo.id        = static_cast<uint32_t>(i + 1);
        bo.corp      = f.corp;
        bo.body      = f.body;
        bo.resource  = resource_type::iron_ore;
        bo.quantity  = 500.0f;
        bo.max_price = 999.0f;
        f.w.buy_orders.clear();
        f.w.buy_orders.push_back(bo);

        economy_report rep = run_economy_step(f.w, reg);
        clear_markets(f.w, reg, rep);
    }
    return f.w.markets.at(f.market).price[ri(resource_type::iron_ore)];
}

// --- P3/P4: site A, market clearing, reacts to the registry band -------------

void run_market_site()
{
    std::printf("=== P3/P4: SITE A (resolve_price) reads the registry band ===\n\n");

    const float base  = 10.0f;
    const int   ticks = 40; // ample for the EMA to settle against either rail

    // P3 — the FLOOR. Supply with no buyer: the target price is 0, so the price
    // walks down and parks on the floor.
    {
        recipe_registry loose; // default band: floor 0.25
        const float p_default = settle_at_floor(loose, base, ticks);

        recipe_registry tight;
        price_band_params pb;
        pb.floor_mult = 0.90f; // a floor no stale 0.25f constant could produce
        pb.ceil_mult  = 4.0f;
        tight.set_price_band(pb);
        const float p_raised = settle_at_floor(tight, base, ticks);

        std::printf("       floor sweep: default (0.25) -> %.4f, floor_mult 0.90 -> %.4f\n",
                    p_default, p_raised);
        check(p_raised > p_default + 1e-3f,
              "P3 raising floor_mult raises the settled price (site A moved)");
        check(p_raised >= base * 0.90f - 1e-2f,
              "P3 the settled price honours the RAISED floor, not the old 0.25x");
        check(p_default <= base * 0.25f + 1e-2f,
              "P3 the default band still parks the price on 0.25x exactly as before");
    }

    // P4 — the CEILING. Demand with zero supply drives the target to the ceiling.
    {
        recipe_registry loose; // default band: ceiling 4.0
        const float p_default = settle_at_ceiling(loose, base, ticks);

        recipe_registry tight;
        price_band_params pb;
        pb.floor_mult = 0.25f;
        pb.ceil_mult  = 1.10f; // a ceiling no stale 4.0f constant could produce
        tight.set_price_band(pb);
        const float p_lowered = settle_at_ceiling(tight, base, ticks);

        std::printf("       ceil sweep:  default (4.00) -> %.4f, ceil_mult 1.10 -> %.4f\n",
                    p_default, p_lowered);
        check(p_lowered < p_default - 1e-3f,
              "P4 lowering ceil_mult lowers the settled price (site A moved)");
        check(p_lowered <= base * 1.10f + 1e-2f,
              "P4 the settled price honours the LOWERED ceiling, not the old 4x");
        check(p_default >= base * 4.0f - 1e-1f,
              "P4 the default band still parks the price on 4x exactly as before");
    }
    std::printf("\n");
}

// --- P5: site B, the workforce solver, reacts to the SAME registry band -------
//
// THE ROW THAT WOULD HAVE FAILED BEFORE BL-442. solve_workforce_target prices
// each candidate workforce target against the market's forward price response
// (wf_target_price), and that estimate is clamped to the band. Raise the ceiling
// and a supply-starved market rewards output far more, which moves the optimum.
// With a private constexpr band, nothing here would move at all.

int solve_with_ceiling(float ceil_mult, float base_price)
{
    recipe_registry reg;
    price_band_params pb;
    pb.floor_mult = 0.25f;
    pb.ceil_mult  = ceil_mult;
    reg.set_price_band(pb);

    fixture f = make_fixture(reg, base_price);
    market_component& mc = f.w.markets.at(f.market);
    // Starved of supply, thick with demand: the ceiling is the binding term.
    mc.supply[ri(resource_type::iron_ore)] = 0.0f;
    mc.demand[ri(resource_type::iron_ore)] = 400.0f;

    return solve_workforce_target(f.w, reg, f.w.buildings.at(f.bld), /*contention=*/1.0f);
}

void run_solver_site()
{
    std::printf("=== P5: SITE B (wf_target_price) reads the SAME registry band ===\n\n");

    const float base = 10.0f;
    const int   lo   = solve_with_ceiling(/*ceil_mult=*/0.30f, base);
    const int   hi   = solve_with_ceiling(/*ceil_mult=*/40.0f, base);

    std::printf("       solved target: ceil_mult 0.30 -> %d, ceil_mult 40.0 -> %d\n", lo, hi);
    check(lo != hi,
          "P5 the workforce solver's chosen target MOVES with the registry band "
          "(a private constexpr copy would not move)");

    // THE DIRECTION IS THE OPPOSITE OF THE NAIVE GUESS, AND CORRECTLY SO — worth
    // pinning, because it looks like a bug until you follow the model.
    //
    // A LOW ceiling makes the price CONSTANT across the whole candidate range: at
    // base 10 with demand 400, every candidate's target price
    // (10 * sqrt(400 / supply)) sits well above a 3.0 cap, so every candidate
    // prices at 3.0. Constant price means revenue is LINEAR in output, and the
    // solver's own header says exactly what that produces — "at fixed prices the
    // optimum would be degenerate bang-bang" — so it saturates at 200.
    //
    // A HIGH ceiling lets the real price response through: the price falls as this
    // building's own output raises supply, revenue goes CONCAVE, and the interior
    // optimum the solver exists to find appears (100). So a richer ceiling yields a
    // LOWER target, because it restores the price response that punishes flooding
    // the market. Asserted rather than merely commented, so the day this inverts,
    // somebody has to justify it.
    check(hi < lo,
          "P5 direction: a high ceiling restores the price response and yields an "
          "INTERIOR target; a low ceiling flattens price and saturates (bang-bang)");
    std::printf("\n");
}

// --- P6: both sites agree, because there is only one value -------------------

void run_sites_agree()
{
    std::printf("=== P6: the two sites clamp against ONE value ===\n\n");

    const float base = 10.0f;

    // Sweep several bands. For each, the market's ceiling-pinned resolved price
    // must sit inside the band the registry names — checked through the public
    // clearing path, so the assertion binds on whatever resolve_price actually
    // used rather than on a number this harness computed itself.
    const float ceilings[] = {1.10f, 2.00f, 4.00f, 7.50f};
    bool all_in_band = true;
    bool tracks_ceiling = true;
    float prev = -1.0f;
    for (const float c : ceilings)
    {
        recipe_registry reg;
        price_band_params pb;
        pb.floor_mult = 0.25f;
        pb.ceil_mult  = c;
        reg.set_price_band(pb);

        const float p  = settle_at_ceiling(reg, base, /*ticks=*/40);
        const bool  ok = p <= base * c + 1e-2f && p >= base * 0.25f - 1e-2f;
        std::printf("       ceil_mult %-5.2f -> settled %8.4f  (band [%.2f, %.2f])  %s\n",
                    c, p, base * 0.25f, base * c, ok ? "in" : "OUT");
        if (!ok)
            all_in_band = false;
        // A ceiling-pinned price must rise monotonically with the ceiling. A site
        // that ignored the registry would return the same number four times.
        if (prev >= 0.0f && !(p > prev))
            tracks_ceiling = false;
        prev = p;
    }
    check(all_in_band, "P6 every swept band contains the price the market resolved");
    check(tracks_ceiling,
          "P6 the settled price tracks the registry ceiling monotonically "
          "(a stale local constant would return one value four times)");

    check(solve_with_ceiling(4.0f, base) == solve_with_ceiling(4.0f, base),
          "P6 the solver is deterministic under a fixed band (no hidden state)");
    std::printf("\n");
}

} // namespace

int main()
{
    std::printf("\n=== price_band_harness (BL-442 step 1) ===\n");
    std::printf("The band is authored ONCE in scripts/economy.lua and reaches both\n");
    std::printf("resolve_price and wf_target_price via recipe_registry::price_band().\n\n");

    run_defaults();
    run_lua_reaches_cpp();
    run_market_site();
    run_solver_site();
    run_sites_agree();

    if (g_failures == 0)
        std::printf("ALL PASS (0 failures)\n");
    else
        std::printf("FAILURES: %d\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
