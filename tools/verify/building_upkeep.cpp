// building_upkeep.cpp — BL-641, requirement group `building-upkeep-goods` R1-R3, R6.
//
// WHAT THIS ASSERTS, and it is deliberately about RELATIONS, not magnitudes. The
// authored rates live in scripts/economy.lua and are a first cut flagged for
// calibration; what must not drift is the SHAPE:
//
//   R1  the draw is per-building-type, against the OWNER'S POOL ON THE BUILDING'S
//       OWN BODY, in a fixed deterministic order — and the order is load-bearing,
//       because two buildings of one corp on one body draw the same stock and the
//       order decides which one goes short.
//   R2  THE SHORTFALL RULE IS THE SAME RULE an out-of-supply unit takes: an unmet
//       draw WEAKENS the building by `supply_decay_permille` and never destroys,
//       idles or decommissions it. A factory short of its tools runs badly.
//   R3  rates are per type and ERA-BANDED, and every rate is authorable at 0.0 —
//       a zero entry skipped exactly as an absent one, so the shape lands inert
//       (the BL-454 precedent).
//   R6  determinism with rates ON: two identical runs produce identical pools and
//       identical supply factors, and no unordered container decides an outcome.
//
// R4 (bit-identical at zero rates) and R5 (the census reads Industry PRESENT) are
// NOT here on purpose: R4 is a byte-compare of econ_harness / econ_bankruptcy /
// econ_stability across the change, and R5 is demand_census, which loads the real
// Lua. Neither is an assertion this fixture could make honestly.
//
// Build (from the repo root):
//   cmd //c tools\verify\build_harness.bat building_upkeep
// Run:
//   .\build_gen\verify\building_upkeep.exe

#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

// BL-654: both upkeep passes now take an `economy_report&` — the shortfall bid
// they place on the market lands in its `wants` / `purchases` / `upkeep_wants`
// registers. R1/R2/R3/R6 below neither read nor write them: they hand-build a
// registry, `price_band_params::reservation_mult` defaults to 0, and there is no
// market on their fixture — so the bid path is off and the draw is pool-only
// exactly as it was before BL-654, which is what keeps those rows measuring the
// same thing they always did. One shared scratch report says that once rather
// than scattering a fresh local at every call site. R7 is the exception: it
// declares its OWN report per row, because the bid is what it is asserting.
economy_report g_upkeep_report;

int g_failures = 0;

void check(bool cond, const char* what)
{
    std::printf("  %s  %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond)
        ++g_failures;
}

void check_near(float got, float want, const char* what, float tol = 1e-3f)
{
    const bool ok = std::fabs(got - want) < tol;
    std::printf("  %s  %s   (got %.4f, want %.4f)\n", ok ? "PASS" : "FAIL", what, got, want);
    if (!ok)
        ++g_failures;
}

std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

/// The fixture: ONE body, ONE corp, and however many buildings the caller asks
/// for on it, all sharing the corp's single pool. That sharing is the whole point
/// — it is what makes the visit order observable.
struct fixture
{
    world     w;
    entity_id body = null_entity;
    entity_id corp = null_entity;
    std::vector<entity_id> buildings;

    void build(int n_buildings, building_type type)
    {
        body = w.create_entity();
        w.bodies[body] = body_component{};
        w.bodies[body].name = "Anvil";

        corp = w.create_entity();
        corporation_component cc;
        cc.name             = "Test Holdings";
        cc.starting_capital = 1000.0f;
        cc.balance          = 1000.0f;
        // Excluded from the AI tier for the same reason econ_harness excludes its
        // corps: this fixture pins ONE pass's arithmetic, and a scorer rewriting a
        // workforce dial mid-run would measure something else.
        cc.is_player = true;

        for (int i = 0; i < n_buildings; ++i)
        {
            const entity_id tile = w.create_entity();
            tile_component tc{};
            tc.body = body;
            w.tiles[tile] = tc;

            const entity_id b = w.create_entity();
            building_component bc{};
            bc.tile = tile;
            bc.type = type;
            bc.workforce_assigned = 0.5f;
            w.buildings[b] = bc;
            buildings.push_back(b);
            cc.assets.push_back(b);
        }
        w.corporations[corp] = cc;
    }

    stockpile_component& pool() { return w.pool_for(corp, body); }
};

/// A registry authoring ONE resource on ONE building type in ONE band.
recipe_registry make_registry(building_type bt, era_band band, resource_type good, float qty,
                              int decay = 50, int recovery = 100, int floor = 0)
{
    recipe_registry reg;
    building_upkeep_params up;
    up.supply_decay_permille    = decay;
    up.supply_recovery_permille = recovery;
    up.supply_floor_permille    = floor;
    up.goods[static_cast<std::size_t>(bt)][static_cast<std::size_t>(band)][ri(good)] = qty;
    reg.set_building_upkeep(up);
    reg.set_era(band == era_band::any ? era_band::any : band);
    return reg;
}

// ---------------------------------------------------------------------------
// R3 — per type, era-banded, and a zero entry is skipped as an absent one
// ---------------------------------------------------------------------------
void r3_rates_are_per_type_and_era_banded()
{
    std::printf("\n--- R3  rates are per building type, ERA-BANDED, and zero-skippable ---\n");

    building_upkeep_params up;
    up.goods[static_cast<std::size_t>(building_type::processing_facility)]
            [static_cast<std::size_t>(era_band::ancient)][ri(resource_type::tools)] = 0.14f;
    up.goods[static_cast<std::size_t>(building_type::processing_facility)]
            [static_cast<std::size_t>(era_band::industrial)][ri(resource_type::machinery)] = 0.15f;
    up.goods[static_cast<std::size_t>(building_type::processing_facility)]
            [static_cast<std::size_t>(era_band::any)][ri(resource_type::planks)] = 0.05f;

    const auto anc = building_upkeep_goods(up, building_type::processing_facility,
                                           era_band::ancient);
    const auto ind = building_upkeep_goods(up, building_type::processing_facility,
                                           era_band::industrial);
    const auto non = building_upkeep_goods(up, building_type::extraction_site,
                                           era_band::ancient);

    // THE BAND IS WHAT DECIDES THE BASKET. An ancient workshop runs on tools; an
    // industrial one on machinery; neither inherits the other's line.
    check_near(anc[ri(resource_type::tools)],     0.14f, "R3 ancient band draws its own good");
    check_near(anc[ri(resource_type::machinery)], 0.0f,  "R3 ancient band draws NO industrial good");
    check_near(ind[ri(resource_type::machinery)], 0.15f, "R3 industrial band draws its own good");
    check_near(ind[ri(resource_type::tools)],     0.0f,  "R3 industrial band draws NO ancient good");

    // An `any` line is common to both arcs and is authored ONCE.
    check_near(anc[ri(resource_type::planks)], 0.05f, "R3 an `any` line applies in the ancient band");
    check_near(ind[ri(resource_type::planks)], 0.05f, "R3 the same `any` line applies in industrial");

    // PER TYPE: a type with nothing authored draws nothing, in any band.
    float non_total = 0.0f;
    for (float q : non)
        non_total += q;
    check_near(non_total, 0.0f, "R3 an unauthored building type draws nothing");

    // An `any` CAMPAIGN — the unset default every hand-built registry carries —
    // takes the `any` basket alone, never the union of both arcs.
    const auto unset = building_upkeep_goods(up, building_type::processing_facility, era_band::any);
    check_near(unset[ri(resource_type::planks)],    0.05f, "R3 an unset campaign takes the `any` basket");
    check_near(unset[ri(resource_type::tools)],     0.0f,  "R3 an unset campaign takes no ancient line");
    check_near(unset[ri(resource_type::machinery)], 0.0f,  "R3 an unset campaign takes no industrial line");

    // ZERO IS SKIPPED EXACTLY AS ABSENT. Author a zero and prove the pass leaves
    // the pool and the supply factor untouched — this is the property R4 rests on.
    {
        fixture f;
        f.build(1, building_type::processing_facility);
        f.pool().quantities[ri(resource_type::tools)] = 10.0f;

        recipe_registry reg = make_registry(building_type::processing_facility,
                                            era_band::ancient, resource_type::tools, 0.0f);
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, g_upkeep_report);

        check(t.buildings == 1, "R3 the pass VISITED the building at a zero rate");
        check(t.drawing == 0,   "R3 ... and skipped it: a zero entry draws like an absent one");
        check_near(f.pool().quantities[ri(resource_type::tools)], 10.0f,
                   "R3 a zero rate leaves the pool untouched");
        check(f.w.buildings.at(f.buildings[0]).supply_factor_permille == 1000,
              "R3 a zero rate moves no supply factor (not even upward)");
    }

    // An ALL-ZERO table never creates a pool — the property that keeps a zero-rate
    // world byte-identical down to its pool SET, not merely its pool contents.
    {
        fixture f;
        f.build(1, building_type::processing_facility);
        recipe_registry reg = make_registry(building_type::processing_facility,
                                            era_band::ancient, resource_type::tools, 0.0f);
        const std::size_t pools_before = f.w.corp_body_pools.size();
        run_building_upkeep(f.w, reg, g_upkeep_report);
        check(f.w.corp_body_pools.size() == pools_before,
              "R3 a zero-rate pass creates no pool that did not already exist");
    }
}

// ---------------------------------------------------------------------------
// R1 — the draw, its pool, its body, and its ORDER
// ---------------------------------------------------------------------------
void r1_the_draw_and_its_order()
{
    std::printf("\n--- R1  the draw is per type, against the owner's pool ON ITS OWN BODY ---\n");

    // The basket really is drawn, and from the owner's own pool.
    {
        fixture f;
        f.build(1, building_type::processing_facility);
        f.pool().quantities[ri(resource_type::tools)] = 10.0f;

        recipe_registry reg = make_registry(building_type::processing_facility,
                                            era_band::ancient, resource_type::tools, 0.25f);
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, g_upkeep_report);

        check(t.drawing == 1, "R1 an authored type draws");
        check_near(f.pool().quantities[ri(resource_type::tools)], 9.75f,
                   "R1 the basket is debited from the owner's pool");
        check(t.unmet == 0, "R1 an ample pool meets the draw");
    }

    // THE BODY IS THE BUILDING'S OWN. A second body's pool is never touched, and a
    // building standing on it draws against ITS body, not the corp's first one.
    {
        fixture f;
        f.build(1, building_type::extraction_site);

        const entity_id body2 = f.w.create_entity();
        f.w.bodies[body2] = body_component{};
        const entity_id tile2 = f.w.create_entity();
        tile_component tc2{};
        tc2.body = body2;
        f.w.tiles[tile2] = tc2;
        const entity_id b2 = f.w.create_entity();
        building_component bc2{};
        bc2.tile = tile2;
        bc2.type = building_type::extraction_site;
        f.w.buildings[b2] = bc2;
        f.w.corporations.at(f.corp).assets.push_back(b2);

        f.pool().quantities[ri(resource_type::tools)] = 10.0f;
        f.w.pool_for(f.corp, body2).quantities[ri(resource_type::tools)] = 10.0f;

        recipe_registry reg = make_registry(building_type::extraction_site,
                                            era_band::ancient, resource_type::tools, 1.0f);
        run_building_upkeep(f.w, reg, g_upkeep_report);

        check_near(f.pool().quantities[ri(resource_type::tools)], 9.0f,
                   "R1 the home-body building drew from the home-body pool");
        check_near(f.w.pool_for(f.corp, body2).quantities[ri(resource_type::tools)], 9.0f,
                   "R1 the other-body building drew from THAT body's pool, not the first");
    }

    // THE ORDER IS LOAD-BEARING. Two buildings of one corp on one body share a pool
    // holding enough for exactly ONE of them. The LOWER building id must be the one
    // supplied and the higher the one that goes short — ascending id, stated.
    {
        fixture f;
        f.build(2, building_type::processing_facility);
        const entity_id lo = (f.buildings[0] < f.buildings[1]) ? f.buildings[0] : f.buildings[1];
        const entity_id hi = (f.buildings[0] < f.buildings[1]) ? f.buildings[1] : f.buildings[0];

        f.pool().quantities[ri(resource_type::tools)] = 1.0f; // exactly one draw's worth

        recipe_registry reg = make_registry(building_type::processing_facility,
                                            era_band::ancient, resource_type::tools, 1.0f);
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, g_upkeep_report);

        check(t.unmet == 1, "R1 with stock for one, exactly one of two buildings goes short");
        check(f.w.buildings.at(lo).supply_factor_permille == 1000,
              "R1 the LOWER building id was supplied (ascending order)");
        check(f.w.buildings.at(hi).supply_factor_permille < 1000,
              "R1 ... and the HIGHER id is the one that went short");
        check_near(f.pool().quantities[ri(resource_type::tools)], 0.0f,
                   "R1 the shared pool is drained, never negative");
    }

    // A pool that cannot cover a draw is taken down to zero, never below.
    {
        fixture f;
        f.build(1, building_type::processing_facility);
        f.pool().quantities[ri(resource_type::tools)] = 0.3f;

        recipe_registry reg = make_registry(building_type::processing_facility,
                                            era_band::ancient, resource_type::tools, 1.0f);
        run_building_upkeep(f.w, reg, g_upkeep_report);
        check_near(f.pool().quantities[ri(resource_type::tools)], 0.0f,
                   "R1 a partial draw takes what is there and stops at zero");
    }

    // WHO DRAWS: a building still under construction, and a decommissioned one,
    // are both passed over — and neither has its supply factor moved.
    {
        fixture f;
        f.build(2, building_type::processing_facility);
        f.w.buildings.at(f.buildings[0]).ticks_remaining = 3;
        f.w.buildings.at(f.buildings[1]).decommissioned  = true;
        f.pool().quantities[ri(resource_type::tools)] = 10.0f;

        recipe_registry reg = make_registry(building_type::processing_facility,
                                            era_band::ancient, resource_type::tools, 1.0f);
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, g_upkeep_report);

        check(t.buildings == 0, "R1 neither an unfinished nor a decommissioned building draws");
        check_near(f.pool().quantities[ri(resource_type::tools)], 10.0f,
                   "R1 ... and the pool is untouched by either");
        check(f.w.buildings.at(f.buildings[0]).supply_factor_permille == 1000
                  && f.w.buildings.at(f.buildings[1]).supply_factor_permille == 1000,
              "R1 ... and neither silently heals or decays");
    }
}

// ---------------------------------------------------------------------------
// R2 — the shortfall rule is the SAME rule, and it never destroys
// ---------------------------------------------------------------------------
void r2_the_shortfall_rule()
{
    std::printf("\n--- R2  an unmet draw WEAKENS the building; it never destroys it ---\n");

    fixture f;
    f.build(1, building_type::processing_facility);
    const entity_id b = f.buildings[0];
    // Empty pool: every draw goes unmet, every tick.
    recipe_registry reg = make_registry(building_type::processing_facility,
                                        era_band::ancient, resource_type::tools, 1.0f,
                                        /*decay=*/50, /*recovery=*/100);

    // THE SAME SUBTRACTION a unit takes: one decay step per unmet tick.
    run_building_upkeep(f.w, reg, g_upkeep_report);
    check(f.w.buildings.at(b).supply_factor_permille == 950,
          "R2 one unmet draw subtracts exactly supply_decay_permille");

    // Sustained neglect hollows the firm out — and NEVER past zero.
    for (int i = 0; i < 40; ++i)
        run_building_upkeep(f.w, reg, g_upkeep_report);
    check(f.w.buildings.at(b).supply_factor_permille == 0,
          "R2 sustained shortfall floors the supply factor at 0");

    // IT NEVER DESTROYS, IDLES OR DECOMMISSIONS. This is the requirement's own
    // wording and it is asserted literally: a factory short of its tools runs
    // badly; it does not vanish.
    check(f.w.buildings.find(b) != f.w.buildings.end(),
          "R2 the building still EXISTS after 41 unmet ticks");
    check(!f.w.buildings.at(b).decommissioned,
          "R2 ... is not decommissioned");
    check(f.w.buildings.at(b).workforce_target == 100
              && f.w.buildings.at(b).workforce_assigned > 0.0f,
          "R2 ... and is not idled (its dials are untouched)");
    check(f.w.corporations.at(f.corp).assets.size() == 1,
          "R2 ... and is still on its owner's books");

    // WHAT "RUNS BADLY" MEANS: the output scalar, which is the reader of the
    // factor. Fully supplied is exactly 1.0f — the arithmetic identity R4 needs.
    check(building_supply_scalar(f.w.buildings.at(b)) == 0.0f,
          "R2 a zero supply factor scales output to zero (weak, not gone)");
    {
        building_component full{};
        check(building_supply_scalar(full) == 1.0f,
              "R2 a fully-supplied building scales output by EXACTLY 1.0f");
        building_component half{};
        half.supply_factor_permille = 500;
        check_near(building_supply_scalar(half), 0.5f,
                   "R2 a half-supplied building scales output by half");
        building_component over{};
        over.supply_factor_permille = 5000;
        check(building_supply_scalar(over) == 1.0f,
              "R2 supply never scales output ABOVE nominal");
    }

    // RECOVERY: a met draw repairs, at the authored recovery rate, ceilinged at
    // 1000 — the other half of the one rule.
    f.pool().quantities[ri(resource_type::tools)] = 1000.0f;
    run_building_upkeep(f.w, reg, g_upkeep_report);
    check(f.w.buildings.at(b).supply_factor_permille == 100,
          "R2 a met draw recovers by supply_recovery_permille");
    for (int i = 0; i < 20; ++i)
        run_building_upkeep(f.w, reg, g_upkeep_report);
    check(f.w.buildings.at(b).supply_factor_permille == 1000,
          "R2 recovery is ceilinged at 1000 (fully supplied)");
}

// ---------------------------------------------------------------------------
// R6 — determinism with rates ON
// ---------------------------------------------------------------------------
void r6_determinism()
{
    std::printf("\n--- R6  determinism with the rates ON ---\n");

    // Two independently-constructed, identical worlds, run the same number of
    // ticks against the same registry, must agree on every pool quantity and
    // every supply factor. Built twice rather than copied, so an ordering that
    // depended on allocation or hash layout has a chance to diverge.
    const auto run_once = [](int ticks) {
        fixture f;
        f.build(6, building_type::processing_facility);
        // Enough for four of the six draws — so the pool runs out mid-walk every
        // tick and the ORDER decides who goes short. Nothing here is worth
        // measuring if the fixture never contends.
        f.pool().quantities[ri(resource_type::tools)] = 4.0f;

        recipe_registry reg = make_registry(building_type::processing_facility,
                                            era_band::ancient, resource_type::tools, 1.0f);
        for (int i = 0; i < ticks; ++i)
        {
            f.pool().quantities[ri(resource_type::tools)] += 4.0f; // a trickle of resupply
            run_building_upkeep(f.w, reg, g_upkeep_report);
        }

        std::vector<int> out;
        for (const entity_id b : f.buildings)
            out.push_back(f.w.buildings.at(b).supply_factor_permille);
        out.push_back(static_cast<int>(f.pool().quantities[ri(resource_type::tools)] * 1000.0f));
        return out;
    };

    const std::vector<int> a = run_once(12);
    const std::vector<int> b = run_once(12);
    check(a == b, "R6 two identical runs produce identical supply factors and pools");

    // The run must actually have CONTENDED, or the equality above is vacuous.
    bool any_weak = false, any_full = false;
    for (std::size_t i = 0; i + 1 < a.size(); ++i)
    {
        if (a[i] < 1000) any_weak = true;
        if (a[i] == 1000) any_full = true;
    }
    check(any_weak && any_full,
          "R6 the fixture genuinely contended (some short, some supplied)");

    // The pass's own tick record is stable too — it is what a caller reads.
    fixture f;
    f.build(3, building_type::extraction_site);
    f.pool().quantities[ri(resource_type::planks)] = 2.0f;
    recipe_registry reg = make_registry(building_type::extraction_site,
                                        era_band::ancient, resource_type::planks, 1.0f);
    const building_upkeep_tick t1 = run_building_upkeep(f.w, reg, g_upkeep_report);
    check(t1.buildings == 3 && t1.drawing == 3 && t1.unmet == 1 && t1.weakened == 1,
          "R6 the tick record reports what the pass did (3 seen, 3 drew, 1 short)");
}

// ---------------------------------------------------------------------------
// R7 — BL-654: a short pool BUYS, up to a reservation ceiling
// ---------------------------------------------------------------------------
// Relations again, never magnitudes: the authored `reservation_mult` is derived
// in scripts/economy.lua and is NOT asserted here. What must not drift is the
// shape — a short pool bids, it bids the WHOLE shortfall, it pays only for what
// it received, and above the ceiling it does not bid at all.

/// Put ONE market on the fixture's body, anchored on the first building's tile,
/// with an authored base price / current price / shelf stock for `good`.
entity_id add_market(fixture& f, resource_type good, float base, float price, float inventory)
{
    const entity_id mid = f.w.create_entity();
    market_component mc{};
    mc.body        = f.body;
    mc.centre_tile = f.w.buildings.at(f.buildings.front()).tile;
    mc.base_price[ri(good)] = base;
    mc.price[ri(good)]      = price;
    mc.inventory[ri(good)]  = inventory;
    f.w.markets[mid] = mc;
    return mid;
}

/// A registry authoring both the upkeep basket and a price band, so the
/// reservation ceiling is reachable. floor/ceil are the shipped pair; only
/// `reservation` varies across the rows below.
recipe_registry registry_with_reservation(resource_type good, float qty, float reservation)
{
    recipe_registry reg = make_registry(building_type::extraction_site, era_band::any, good, qty);
    price_band_params pb;
    pb.floor_mult       = 0.25f;
    pb.ceil_mult        = 10.0f;
    pb.reservation_mult = reservation;
    reg.set_price_band(pb);
    return reg;
}

float want_of(const economy_report& rep, entity_id corp, entity_id body, resource_type good)
{
    const auto it = rep.wants.find(std::make_pair(corp, body));
    return (it == rep.wants.end()) ? 0.0f : it->second[ri(good)];
}

float fill_of(const economy_report& rep, entity_id corp, entity_id body, resource_type good)
{
    const auto it = rep.purchases.find(std::make_pair(corp, body));
    return (it == rep.purchases.end()) ? 0.0f : it->second[ri(good)];
}

float upkeep_want_of(const economy_report& rep, entity_id corp, entity_id body, resource_type good)
{
    const auto it = rep.upkeep_wants.find(std::make_pair(corp, body));
    return (it == rep.upkeep_wants.end()) ? 0.0f : it->second[ri(good)];
}

void r7_the_reservation_ceiling()
{
    std::printf("\n--- R7  BL-654: a short pool BIDS the shortfall, up to a reservation ceiling ---\n");

    constexpr resource_type good = resource_type::tools;
    constexpr float need = 0.5f;
    constexpr float base = 4.0f;

    // --- R7a: UNDER the ceiling, an empty pool bids and the shelf fills it ---
    {
        fixture f;
        f.build(1, building_type::extraction_site);
        add_market(f, good, base, base * 5.0f, /*inventory*/ 10.0f); // 5x base, under 9x
        recipe_registry reg = registry_with_reservation(good, need, 9.0f);

        economy_report rep;
        const int before = f.w.buildings.at(f.buildings[0]).supply_factor_permille;
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, rep);

        check_near(want_of(rep, f.corp, f.body, good), need,
                   "R7a the WHOLE shortfall reaches the want register");
        check_near(fill_of(rep, f.corp, f.body, good), need,
                   "R7a the fill is what the shelf actually supplied");
        check_near(upkeep_want_of(rep, f.corp, f.body, good), need,
                   "R7a the attribution mirror carries the same bid");
        check_near(f.w.markets.begin()->second.inventory[ri(good)], 10.0f - need,
                   "R7a the market's real inventory is drained by the fill");
        check(t.unmet == 0, "R7a a draw the market covered is NOT unmet");
        check(f.w.buildings.at(f.buildings[0]).supply_factor_permille >= before,
              "R7a a covered draw does not weaken the building");
    }

    // --- R7b: ABOVE the ceiling it does not bid AT ALL ----------------------
    {
        fixture f;
        f.build(1, building_type::extraction_site);
        add_market(f, good, base, base * 9.5f, /*inventory*/ 10.0f); // 9.5x base, over 9x
        recipe_registry reg = registry_with_reservation(good, need, 9.0f);

        economy_report rep;
        const int before = f.w.buildings.at(f.buildings[0]).supply_factor_permille;
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, rep);

        // "does not bid at all" — not a reduced bid, not a bid that fails to
        // fill. The want register must not hear from it.
        check(rep.wants.empty(),     "R7b above the ceiling NOTHING reaches the want register");
        check(rep.purchases.empty(), "R7b above the ceiling nothing is bought, so nothing is billed");
        check_near(f.w.markets.begin()->second.inventory[ri(good)], 10.0f,
                   "R7b the shelf is untouched — the buyer walked away");
        check(t.unmet == 1, "R7b the draw goes unmet");
        check(f.w.buildings.at(f.buildings[0]).supply_factor_permille < before,
              "R7b the EXISTING shortfall rule weakens the building instead");
    }

    // --- R7c: the bid is the WANT, the payment is the FILL -------------------
    // BL-441's distinction, on this channel: a shelf that can cover only part of
    // the shortfall still hears the whole want, or the shortage silences the one
    // voice that would have priced it.
    {
        fixture f;
        f.build(1, building_type::extraction_site);
        add_market(f, good, base, base * 2.0f, /*inventory*/ 0.2f); // shelf < need
        recipe_registry reg = registry_with_reservation(good, need, 9.0f);

        economy_report rep;
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, rep);

        check_near(want_of(rep, f.corp, f.body, good), need,
                   "R7c the want is the full shortfall, UNREDUCED by what the shelf holds");
        check_near(fill_of(rep, f.corp, f.body, good), 0.2f,
                   "R7c the fill is only what was delivered");
        check_near(f.w.markets.begin()->second.inventory[ri(good)], 0.0f,
                   "R7c the shelf is emptied, never driven negative");
        check(t.unmet == 1, "R7c a partly-filled draw is still unmet");
    }

    // --- R7d: the pool is drawn FIRST; only its shortfall is bid -------------
    {
        fixture f;
        f.build(1, building_type::extraction_site);
        add_market(f, good, base, base, /*inventory*/ 10.0f);
        recipe_registry reg = registry_with_reservation(good, need, 9.0f);
        f.pool().quantities[ri(good)] = need * 0.4f;

        economy_report rep;
        run_building_upkeep(f.w, reg, rep);

        check_near(want_of(rep, f.corp, f.body, good), need * 0.6f,
                   "R7d only what the POOL could not cover is bid");
        check_near(f.pool().quantities[ri(good)], 0.0f, "R7d the pool is spent first");
    }

    // --- R7e: unpriced == unbuyable, and it needs no rule of its own ---------
    // A resource with base_price 0 has a reservation ceiling of 0, and no price
    // clears it. Same reading run_construction already takes.
    {
        fixture f;
        f.build(1, building_type::extraction_site);
        add_market(f, good, /*base*/ 0.0f, /*price*/ 0.0f, /*inventory*/ 10.0f);
        recipe_registry reg = registry_with_reservation(good, need, 9.0f);

        economy_report rep;
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, rep);

        check(rep.wants.empty(), "R7e an UNPRICED good is never bid for");
        check(t.unmet == 1,      "R7e and the draw goes unmet, as it always did");
    }

    // --- R7f: reservation_mult 0 is the pre-BL-654 behaviour exactly ---------
    // The DEFAULT. Every harness that hand-builds a registry and never authors a
    // band must see a pool-only draw, or this item is not inert where it claims.
    {
        fixture f;
        f.build(1, building_type::extraction_site);
        add_market(f, good, base, base, /*inventory*/ 10.0f);
        recipe_registry reg = make_registry(building_type::extraction_site, era_band::any,
                                            good, need); // no price band authored
        check_near(reg.price_band().reservation_mult, 0.0f,
                   "R7f reservation_mult defaults to 0 — the feature ships OFF");

        economy_report rep;
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, rep);

        check(rep.wants.empty(), "R7f at the default the draw never bids");
        check_near(f.w.markets.begin()->second.inventory[ri(good)], 10.0f,
                   "R7f at the default the market is untouched");
        check(t.unmet == 1, "R7f at the default a short pool simply goes short");
    }
}

} // namespace

// ---------------------------------------------------------------------------
// R8 — BL-746 (NR-782 (a)): the decay stops at the authored floor
// ---------------------------------------------------------------------------
void r8_the_floor()
{
    std::printf("\n--- R8  an unmet draw dims a building to the FLOOR and no further (BL-746) ---\n");

    // Floor 500: sustained neglect halves the building and stops there.
    {
        fixture f;
        f.build(1, building_type::processing_facility);
        const entity_id b = f.buildings[0];
        recipe_registry reg = make_registry(building_type::processing_facility,
                                            era_band::ancient, resource_type::tools, 1.0f,
                                            /*decay=*/50, /*recovery=*/100, /*floor=*/500);
        for (int i = 0; i < 40; ++i)
            run_building_upkeep(f.w, reg, g_upkeep_report);
        check(f.w.buildings.at(b).supply_factor_permille == 500,
              "R8 41 unmet ticks at floor 500 leave the factor at exactly 500");
        check_near(building_supply_scalar(f.w.buildings.at(b)), 0.5f,
                   "R8 ... so the building runs at half nominal, not zero");

        // A met draw recovers from the floor exactly as it recovered from any
        // other value — the floor is a floor, not a trap.
        f.pool().quantities[ri(resource_type::tools)] = 100.0f;
        run_building_upkeep(f.w, reg, g_upkeep_report);
        check(f.w.buildings.at(b).supply_factor_permille == 600,
              "R8 a met draw recovers from the floor by supply_recovery_permille");
    }

    // The differential: floor 0 is the old rule, and R2 above already pins that
    // it reaches 0 — so a floor of 250 lands at 250, not at 500 and not at 0.
    {
        fixture f;
        f.build(1, building_type::processing_facility);
        const entity_id b = f.buildings[0];
        recipe_registry reg = make_registry(building_type::processing_facility,
                                            era_band::ancient, resource_type::tools, 1.0f,
                                            50, 100, /*floor=*/250);
        for (int i = 0; i < 40; ++i)
            run_building_upkeep(f.w, reg, g_upkeep_report);
        check(f.w.buildings.at(b).supply_factor_permille == 250,
              "R8 the floor is the AUTHORED number, not a constant (250 -> 250)");
    }

    // A factor already below the floor (a save from before the rule) is lifted
    // to it on its next unmet tick rather than left stranded.
    {
        fixture f;
        f.build(1, building_type::processing_facility);
        const entity_id b = f.buildings[0];
        f.w.buildings.at(b).supply_factor_permille = 120;
        recipe_registry reg = make_registry(building_type::processing_facility,
                                            era_band::ancient, resource_type::tools, 1.0f,
                                            50, 100, /*floor=*/500);
        run_building_upkeep(f.w, reg, g_upkeep_report);
        check(f.w.buildings.at(b).supply_factor_permille == 500,
              "R8 a factor below the floor is lifted to it on the next unmet tick");
    }
}

// ---------------------------------------------------------------------------
// R9 — BL-746 (NR-782 (b)): no wire, no draw
// ---------------------------------------------------------------------------
void r9_no_wire_no_draw()
{
    std::printf("\n--- R9  a grid good is not drawn, and does not weaken, where no wire reaches (BL-746) ---\n");

    // A one-tile body with no road, no hub, no port: the network reaches nothing,
    // so the tile is unreached and power cannot arrive there.
    auto one_tile_body = [](fixture& f) {
        f.build(1, building_type::processing_facility);
        f.w.bodies[f.body].grid_width  = 1;
        f.w.bodies[f.body].grid_height = 1;
        tile_component& tc = f.w.tiles.at(f.w.buildings.at(f.buildings[0]).tile);
        tc.grid_x = 0;
        tc.grid_y = 0;
    };

    // Power on the grid, drawn by the industrial processor, pool empty.
    {
        fixture f;
        one_tile_body(f);
        const entity_id b = f.buildings[0];
        recipe_registry reg = make_registry(building_type::processing_facility,
                                            era_band::industrial, resource_type::power, 0.4f);
        grid_goods_params g;
        g.is_grid[ri(resource_type::power)] = true;
        reg.set_grid_goods(g);
        for (int i = 0; i < 5; ++i)
            run_building_upkeep(f.w, reg, g_upkeep_report);
        check(f.w.buildings.at(b).supply_factor_permille == 1000,
              "R9 an unreached building does NOT weaken for a grid good it cannot receive");
    }

    // THE DIFFERENTIAL: the same draw with power NOT on the grid is an ordinary
    // unmet draw and decays — so the rule keys on the grid flag, not on power.
    {
        fixture f;
        one_tile_body(f);
        const entity_id b = f.buildings[0];
        recipe_registry reg = make_registry(building_type::processing_facility,
                                            era_band::industrial, resource_type::power, 0.4f);
        for (int i = 0; i < 5; ++i)
            run_building_upkeep(f.w, reg, g_upkeep_report);
        check(f.w.buildings.at(b).supply_factor_permille == 750,
              "R9 ... while the same good OFF the grid is an ordinary unmet draw (5 x 50)");
    }

    // The ordinary goods in the basket still draw and still bind on an
    // unreached tile: strip the wire, not the timber.
    {
        fixture f;
        one_tile_body(f);
        const entity_id b = f.buildings[0];
        recipe_registry reg = make_registry(building_type::processing_facility,
                                            era_band::industrial, resource_type::power, 0.4f);
        building_upkeep_params up = reg.building_upkeep();
        up.goods[static_cast<std::size_t>(building_type::processing_facility)]
                [static_cast<std::size_t>(era_band::industrial)][ri(resource_type::timber)] = 0.08f;
        reg.set_building_upkeep(up);
        grid_goods_params g;
        g.is_grid[ri(resource_type::power)] = true;
        reg.set_grid_goods(g);
        run_building_upkeep(f.w, reg, g_upkeep_report);
        check(f.w.buildings.at(b).supply_factor_permille == 950,
              "R9 the unreached building still weakens for the TIMBER it could have had");
    }
}

int main()
{
    std::printf("building_upkeep — BL-641, requirement group `building-upkeep-goods` R1-R3, R6;\n");
    std::printf("  plus R7 (BL-654, the reservation ceiling)\n");
    std::printf("  Relations, not magnitudes: the authored rates are a first cut and are not\n");
    std::printf("  asserted here. R4 (bit-identical at zero) is a byte-compare of the econ\n");
    std::printf("  harnesses; R5 (Industry PRESENT) is demand_census.\n");

    r3_rates_are_per_type_and_era_banded();
    r1_the_draw_and_its_order();
    r2_the_shortfall_rule();
    r6_determinism();
    r7_the_reservation_ceiling();
    r8_the_floor();
    r9_no_wire_no_draw();

    std::printf("\n%s — %d failure(s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
