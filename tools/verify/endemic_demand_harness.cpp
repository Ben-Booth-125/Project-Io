// Headless harness for BL-647 (endemic luxury demand) — the Endemic trade
// channel (docs/economy/MARKETS.md § Demand channels; the goods are
// docs/economy/RESOURCES.md § Mercantile).
//
// The channel is a household pull for the endemic goods (tobacco, spices,
// coffee, furs) that scales with a nation's WEALTH (treasury + positive corp
// balances domiciled there) rather than its headcount, flavoured per
// (nation, good) by a seeded campaign-fixed preference weight — so different
// nations crave different luxuries and the trade route is directional by
// construction. Implemented by inject_endemic_demand (market_clearing.cpp),
// called from clear_markets beside inject_population_demand /
// inject_background_demand. The same change closes the placement gap BL-586
// slice 2 recorded: the four luxuries join placement_rules::k_extractable, so
// an extraction site can finally target what the channel buys.
//
// Verifies without SDL / Lua / ImGui:
//   E1 (wealth gates the pull): a wealthy nation's market receives
//       wealth × wealth_scale × basket[r] demand; a broke nation's receives
//       exactly none; a treasury alone is wealth; a corp in debt contributes
//       zero rather than draining its nation; a nation's pull splits evenly
//       across its markets (total independent of the BL-096 carve count).
//   E2 (character asymmetry): two equally wealthy nations with nonzero
//       preference_spread want measurably different luxury MIXES.
//   E3 (determinism): two same-fixture runs deposit bit-identical demand.
//   E4 (placement gap closed): all four luxuries are in k_extractable, an
//       extraction_site can_place on a deposit tile of each, and a bare tile
//       still refuses with no_deposit.
//   E5 (era banding): the shared (`any`) tranche survives both bands — the
//       luxuries are deposits, band-independent by construction — while a
//       banded row is masked exactly as BL-640's fold masks its siblings.
//   E6 (integration): on the generated world, endemic demand lands and
//       SURVIVES clear_markets' reset into the state price resolution reads;
//       per-good totals and per-nation wealth are printed as the magnitude
//       probe economy.lua's wealth_scale comment cites.
//
// Build + run via the verifier-headless skill (build_gen\verify\). Lua-free:
// every endemic_demand_params here is hand-set via set_endemic_demand, and the
// E6 world carries the C++ fallback gen config (its endemic distance-pricing
// constants equal scripts/world_gen.lua's authored values, so the luxuries ARE
// priced; the flat base_price table prices fewer goods than the shipped one,
// which E6's magnitudes inherit — relations here, magnitudes calibrated once
// against the shipped table by the BL-647 report).

#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include "world/resource_names.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <vector>

static int g_failures = 0;
static int g_passes   = 0;

static void check(bool cond, const char* what, float got = 0.0f, float want = 0.0f)
{
    if (cond) {
        std::printf("  PASS  %s\n", what);
        ++g_passes;
    } else {
        std::printf("  FAIL  %s   (got %.6f, want %.6f)\n", what, got, want);
        ++g_failures;
    }
}

static const resource_type k_luxuries[4] = {
    resource_type::tobacco, resource_type::spices,
    resource_type::coffee,  resource_type::furs,
};

static std::size_t idx(resource_type r) { return static_cast<std::size_t>(r); }

// ---------------------------------------------------------------------------
// Shared fixture: one body, two nations (each owning one tile), one market
// anchored per nation, one corp domiciled per nation. Entity creation order is
// fixed, so ids — and therefore the seeded preference weights — are identical
// across builds (E3 relies on that).
// ---------------------------------------------------------------------------
struct fixture
{
    world w;
    entity_id body = null_entity;
    entity_id nation_a = null_entity, nation_b = null_entity;
    entity_id mkt_a = null_entity, mkt_b = null_entity;
    entity_id corp_a = null_entity, corp_b = null_entity;
};

static fixture make_fixture(float balance_a, float balance_b)
{
    fixture f;
    world& w = f.w;

    f.body = w.create_entity();
    {
        body_component bc{};
        bc.name        = "LuxBody";
        bc.grid_width  = 10;
        bc.grid_height = 4;
        w.bodies[f.body] = bc;
    }

    const entity_id tile_a = w.create_entity();
    const entity_id tile_b = w.create_entity();
    {
        tile_component tc{};
        tc.body = f.body;
        tc.grid_x = 0; tc.grid_y = 0;
        w.tiles[tile_a] = tc;
        tc.grid_x = 5; tc.grid_y = 2;
        w.tiles[tile_b] = tc;
    }

    f.nation_a = w.create_entity();
    f.nation_b = w.create_entity();
    {
        nation_component na{};
        na.name     = "Aralik"; // arbitrary harness strings; only the BYTES matter
        na.politics = ideology::mercantile;
        w.nations[f.nation_a] = na;

        nation_component nb{};
        nb.name     = "Veshkar";
        nb.politics = ideology::isolationist;
        w.nations[f.nation_b] = nb;
    }
    w.tile_to_nation[tile_a] = f.nation_a;
    w.tile_to_nation[tile_b] = f.nation_b;

    f.mkt_a = w.create_entity();
    f.mkt_b = w.create_entity();
    {
        market_component mc{};
        mc.body        = f.body;
        mc.centre_tile = tile_a;
        for (const resource_type r : k_luxuries)
            mc.base_price[idx(r)] = 2.0f;
        w.markets[f.mkt_a] = mc;
        mc.centre_tile = tile_b;
        w.markets[f.mkt_b] = mc;
    }

    f.corp_a = w.create_entity();
    f.corp_b = w.create_entity();
    {
        corporation_component cc{};
        cc.home_nation = f.nation_a;
        cc.balance     = balance_a;
        w.corporations[f.corp_a] = cc;
        cc.home_nation = f.nation_b;
        cc.balance     = balance_b;
        w.corporations[f.corp_b] = cc;
    }

    return f;
}

/// Flat params: elasticity 0 (pow(x,0)==1, min<=1<=max) so every figure is
/// exactly wealth × wealth_scale × basket × pref, price drift excluded.
static endemic_demand_params flat_params(float tobacco_w, float furs_w,
                                         float spread)
{
    endemic_demand_params ed;
    ed.demand_basket[idx(resource_type::tobacco)] = tobacco_w;
    ed.demand_basket[idx(resource_type::furs)]    = furs_w;
    ed.demand_elasticity  = 0.0f;
    ed.elasticity_min     = 0.30f;
    ed.elasticity_max     = 2.50f;
    ed.wealth_scale       = 0.01f;
    ed.preference_spread  = spread;
    return ed;
}

// ---------------------------------------------------------------------------
// E1 — wealth gates the pull.
// ---------------------------------------------------------------------------
static void test_wealth_gates()
{
    std::printf("--- E1: wealth-scaled, not headcount-scaled ---\n");

    // Nation A rich (1000 cr corp), nation B broke (0 cr). spread 0 -> pref 1.
    {
        fixture f = make_fixture(1000.0f, 0.0f);
        recipe_registry reg;
        reg.set_endemic_demand(flat_params(1.0f, 0.0f, 0.0f));
        inject_endemic_demand(f.w, reg);

        const float da = f.w.markets.at(f.mkt_a).demand[idx(resource_type::tobacco)];
        const float db = f.w.markets.at(f.mkt_b).demand[idx(resource_type::tobacco)];
        check(std::fabs(da - 10.0f) < 1e-4f,
              "wealthy nation: demand = wealth x wealth_scale x basket (1000 x 0.01 x 1.0)",
              da, 10.0f);
        check(db == 0.0f, "broke nation: exactly zero luxury demand", db, 0.0f);
    }

    // A treasury alone is wealth (the state's riches count).
    {
        fixture f = make_fixture(0.0f, 0.0f);
        f.w.nations.at(f.nation_b).treasury = 500.0f;
        recipe_registry reg;
        reg.set_endemic_demand(flat_params(1.0f, 0.0f, 0.0f));
        inject_endemic_demand(f.w, reg);

        const float db = f.w.markets.at(f.mkt_b).demand[idx(resource_type::tobacco)];
        check(std::fabs(db - 5.0f) < 1e-4f,
              "treasury-only nation: demand = treasury x wealth_scale x basket",
              db, 5.0f);
    }

    // A corp in debt contributes zero, not a subtraction.
    {
        fixture f = make_fixture(1000.0f, 0.0f);
        const entity_id debtor = f.w.create_entity();
        corporation_component cc{};
        cc.home_nation = f.nation_a;
        cc.balance     = -400.0f;
        f.w.corporations[debtor] = cc;

        recipe_registry reg;
        reg.set_endemic_demand(flat_params(1.0f, 0.0f, 0.0f));
        inject_endemic_demand(f.w, reg);

        const float da = f.w.markets.at(f.mkt_a).demand[idx(resource_type::tobacco)];
        check(std::fabs(da - 10.0f) < 1e-4f,
              "a corp in debt contributes zero (10.0 unchanged, not 6.0)",
              da, 10.0f);
    }

    // A nation's pull splits across its markets; the TOTAL is carve-independent.
    {
        fixture f = make_fixture(1000.0f, 0.0f);
        const entity_id tile_a2 = f.w.create_entity();
        {
            tile_component tc{};
            tc.body = f.body; tc.grid_x = 1; tc.grid_y = 1;
            f.w.tiles[tile_a2] = tc;
        }
        f.w.tile_to_nation[tile_a2] = f.nation_a;
        const entity_id mkt_a2 = f.w.create_entity();
        {
            market_component mc{};
            mc.body        = f.body;
            mc.centre_tile = tile_a2;
            for (const resource_type r : k_luxuries)
                mc.base_price[idx(r)] = 2.0f;
            f.w.markets[mkt_a2] = mc;
        }

        recipe_registry reg;
        reg.set_endemic_demand(flat_params(1.0f, 0.0f, 0.0f));
        inject_endemic_demand(f.w, reg);

        const float d1 = f.w.markets.at(f.mkt_a).demand[idx(resource_type::tobacco)];
        const float d2 = f.w.markets.at(mkt_a2).demand[idx(resource_type::tobacco)];
        check(std::fabs(d1 - 5.0f) < 1e-4f && std::fabs(d2 - 5.0f) < 1e-4f,
              "two-market nation: each market takes half the pull",
              d1, 5.0f);
        check(std::fabs((d1 + d2) - 10.0f) < 1e-4f,
              "nation total is independent of the market carve count",
              d1 + d2, 10.0f);
    }
}

// ---------------------------------------------------------------------------
// E2 — national character: different nations crave different luxuries.
// ---------------------------------------------------------------------------
static void test_character_asymmetry()
{
    std::printf("--- E2: character-flavoured cravings differ by nation ---\n");

    fixture f = make_fixture(1000.0f, 1000.0f); // EQUAL wealth — only character differs
    recipe_registry reg;
    reg.set_endemic_demand(flat_params(1.0f, 1.0f, 0.9f));
    inject_endemic_demand(f.w, reg);

    const market_component& ma = f.w.markets.at(f.mkt_a);
    const market_component& mb = f.w.markets.at(f.mkt_b);
    const float a_tob = ma.demand[idx(resource_type::tobacco)];
    const float a_fur = ma.demand[idx(resource_type::furs)];
    const float b_tob = mb.demand[idx(resource_type::tobacco)];
    const float b_fur = mb.demand[idx(resource_type::furs)];
    std::printf("  nation A: tobacco %.4f furs %.4f | nation B: tobacco %.4f furs %.4f\n",
                a_tob, a_fur, b_tob, b_fur);

    check(a_tob > 0.0f && a_fur > 0.0f && b_tob > 0.0f && b_fur > 0.0f,
          "both nations want both luxuries (spread 0.9 floors pref at 0.1)");

    // The MIX is the design: equal wealth, equal basket, different ratios.
    const float ratio_a = a_tob / a_fur;
    const float ratio_b = b_tob / b_fur;
    check(std::fabs(ratio_a - ratio_b) > 0.01f * std::fabs(ratio_a),
          "the two nations' tobacco:furs mixes differ (directional trade)",
          ratio_a, ratio_b);
}

// ---------------------------------------------------------------------------
// E3 — determinism: two same-fixture runs deposit bit-identical demand.
// ---------------------------------------------------------------------------
static void test_determinism()
{
    std::printf("--- E3: same fixture, bit-identical deposits ---\n");

    auto run = [](std::array<float, 4>& out) {
        fixture f = make_fixture(1234.5f, 678.9f);
        recipe_registry reg;
        reg.set_endemic_demand(flat_params(0.7f, 0.3f, 0.9f));
        inject_endemic_demand(f.w, reg);
        out[0] = f.w.markets.at(f.mkt_a).demand[idx(resource_type::tobacco)];
        out[1] = f.w.markets.at(f.mkt_a).demand[idx(resource_type::furs)];
        out[2] = f.w.markets.at(f.mkt_b).demand[idx(resource_type::tobacco)];
        out[3] = f.w.markets.at(f.mkt_b).demand[idx(resource_type::furs)];
    };

    std::array<float, 4> first{}, second{};
    run(first);
    run(second);

    check(first[0] > 0.0f, "run deposits nonzero demand (the comparison is live)",
          first[0], 0.0f);
    check(first == second,
          "two same-seed runs deposit IDENTICAL demand (== on every float)",
          first[0] - second[0], 0.0f);
}

// ---------------------------------------------------------------------------
// E4 — the placement gap is closed: an extraction site can target each luxury.
// ---------------------------------------------------------------------------
static void test_placement_gap_closed()
{
    std::printf("--- E4: luxuries are extractable (BL-586 slice 2's gap) ---\n");

    for (const resource_type r : k_luxuries)
    {
        check(placement_rules::is_extractable(r),
              "luxury is in placement_rules::k_extractable");

        tile_component tc{};
        tc.substrate = terrain_substrate::sedimentary;
        tc.resource_deposit[idx(r)] = 100.0f;
        const placement_rules::placement_result on_deposit =
            placement_rules::can_place(tc, building_type::extraction_site, r);
        check(on_deposit.ok(),
              "extraction_site can_place on a deposit tile of the luxury");

        tile_component bare{};
        bare.substrate = terrain_substrate::sedimentary;
        const placement_rules::placement_result off_deposit =
            placement_rules::can_place(bare, building_type::extraction_site, r);
        check(off_deposit.reason == placement_rules::placement_reason::no_deposit,
              "a bare tile still refuses the target with no_deposit");
    }
}

// ---------------------------------------------------------------------------
// E5 — era banding: the shared tranche survives both bands; a banded row masks.
// ---------------------------------------------------------------------------
static void test_era_banding()
{
    std::printf("--- E5: shared tranche in both bands; banded rows mask ---\n");

    recipe_registry reg;
    endemic_demand_params ed = flat_params(1.0f, 0.0f, 0.0f);
    era_basket row;
    row.era = era_band::industrial;
    row.demand_basket[idx(resource_type::coffee)] = 0.5f;
    ed.baskets.push_back(row);
    reg.set_endemic_demand(ed);

    reg.set_era(era_band::ancient);
    check(reg.endemic_demand_basket()[idx(resource_type::tobacco)] == 1.0f,
          "ancient band: the shared (any) tobacco weight survives",
          reg.endemic_demand_basket()[idx(resource_type::tobacco)], 1.0f);
    check(reg.endemic_demand_basket()[idx(resource_type::coffee)] == 0.0f,
          "ancient band: an industrial-banded row is masked out",
          reg.endemic_demand_basket()[idx(resource_type::coffee)], 0.0f);

    reg.set_era(era_band::industrial);
    check(reg.endemic_demand_basket()[idx(resource_type::tobacco)] == 1.0f,
          "industrial band: the shared (any) tobacco weight survives",
          reg.endemic_demand_basket()[idx(resource_type::tobacco)], 1.0f);
    check(reg.endemic_demand_basket()[idx(resource_type::coffee)] == 0.5f,
          "industrial band: the banded row folds in",
          reg.endemic_demand_basket()[idx(resource_type::coffee)], 0.5f);
}

// ---------------------------------------------------------------------------
// E6 — integration: the channel lands on the generated world and survives
// clear_markets' reset. Also the magnitude probe for economy.lua's
// wealth_scale.
// ---------------------------------------------------------------------------
static void test_generated_world_integration()
{
    std::printf("--- E6: generated world — demand lands and survives clearing ---\n");

    world w = make_hard_coded_world(no_prehistory());

    recipe_registry reg;
    {
        // Mirrors scripts/economy.lua § endemic_demand.
        endemic_demand_params ed;
        ed.demand_basket[idx(resource_type::tobacco)] = 0.20f;
        ed.demand_basket[idx(resource_type::spices)]  = 0.30f;
        ed.demand_basket[idx(resource_type::coffee)]  = 0.30f;
        ed.demand_basket[idx(resource_type::furs)]    = 0.20f;
        ed.demand_elasticity  = 0.80f;
        ed.elasticity_min     = 0.30f;
        ed.elasticity_max     = 2.50f;
        ed.wealth_scale       = 0.01f;
        ed.preference_spread  = 0.90f;
        reg.set_endemic_demand(ed);
    }

    // The magnitude probe: what does the generated world hold?
    {
        std::vector<entity_id> corp_ids;
        for (const auto& [cid, cc] : w.corporations) { (void)cc; corp_ids.push_back(cid); }
        std::sort(corp_ids.begin(), corp_ids.end());
        std::map<entity_id, float> wealth;
        for (const entity_id cid : corp_ids)
        {
            const corporation_component& cc = w.corporations.at(cid);
            if (cc.home_nation != null_entity && cc.balance > 0.0f)
                wealth[cc.home_nation] += cc.balance;
        }
        float total = 0.0f;
        for (const auto& [nid, cr] : wealth) { (void)nid; total += cr; }
        std::printf("  nations with corp wealth: %zu, total %.1f cr\n",
                    wealth.size(), total);
        check(!wealth.empty(), "the generated world has domiciled corp wealth");
    }

    int priced = 0;
    for (const resource_type r : k_luxuries)
        for (const auto& [mid, mc] : w.markets)
        {
            (void)mid;
            if (mc.base_price[idx(r)] > 0.0f) { ++priced; break; }
        }
    std::printf("  luxuries priced somewhere: %d of 4 (biosphere roll)\n", priced);
    check(priced >= 1, "at least one endemic luxury is priced on this world",
          static_cast<float>(priced), 1.0f);

    const economy_report report = run_economy_step(w, reg);
    clear_markets(w, reg, report);

    float total_lux = 0.0f;
    for (const resource_type r : k_luxuries)
    {
        float per_good = 0.0f;
        // Deterministic print only — float order over the unordered map does
        // not feed an assertion tighter than > 0.
        for (const auto& [mid, mc] : w.markets)
        {
            (void)mid;
            per_good += mc.demand[idx(r)];
        }
        std::printf("  cleared demand, %-7s : %.3f\n",
                    resource_names::name_of(r).c_str(), per_good);
        total_lux += per_good;
    }
    check(total_lux > 0.0f,
          "endemic luxury demand survives clear_markets into priced state",
          total_lux, 0.0f);
}

// ---------------------------------------------------------------------------
int main()
{
    test_wealth_gates();
    test_character_asymmetry();
    test_determinism();
    test_placement_gap_closed();
    test_era_banding();
    test_generated_world_integration();

    if (g_failures == 0)
        std::printf("\nALL PASS (%d assertions)\n", g_passes);
    else
        std::printf("\n%d FAILURE(s) / %d assertions\n", g_failures, g_passes + g_failures);

    return g_failures > 0 ? 1 : 0;
}
