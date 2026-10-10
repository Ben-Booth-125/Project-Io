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
//   R11 BL-1230 (power crosses markets): the province is the grid's cell — a
//       wired province draws across markets, billed at its own market; a strait
//       joins; a dark province strikes power; a short grid is shared pro rata;
//       power's price pools per grid, capacity's does not; a road write
//       invalidates the grid.
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
#include "world/construction.hpp"
#include "world/corporation_generation.hpp" // BL-1232: body_power_grid_gap, power_grids_to_serve
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <set>
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

/// The SHIPPED reservation multiple (scripts/economy.lua `price_band.
/// reservation_mult`, BL-1172 "a fair price"). Restated, not loaded — this
/// harness reads no Lua — so a retune moves it here too; the rows only place
/// prices either side of it.
constexpr float k_shipped_reservation = 2.0f;

/// The SHIPPED shelf share of supply, in ticks of demand (scripts/economy.lua
/// `price_band.shelf_supply_ticks`): 1 (Ben, 2026-10-07, MARKETS.md § Price
/// resolution, under BL-1209's k x (demand + silenced want) law). Restated, not
/// loaded. Every row that claims shipped behaviour runs at this.
constexpr float k_shipped_shelf_ticks = 1.0f;

/// HISTORICAL k = 0 (listings only), shipped 2026-10-03 to 2026-10-07. Rows whose
/// point is k = 0's over-ceiling cost run at this and say HISTORICAL — they are
/// counterfactuals now.
constexpr float k_historical_shelf_ticks = 0.0f;

/// A k > 0 that is NOT shipped (the k = 4 the sweep measured). No processor or
/// site draws in these fixtures, so the silenced-want register is 0 and BL-1209's
/// k x (demand + silenced want) law reads exactly as k x demand here.
constexpr float k_dormant_shelf_ticks = 4.0f;

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

    /// BL-1265 (MARKETS.md § The shelf economy): corporations hold no pools; a
    /// building's upkeep is BOUGHT off the shelf of its tile's market. `pool()`
    /// keeps every row's wording and now names that shelf — the one market on
    /// @p on_body (made on first use: centred on the body's first tile, every
    /// good priced at 1, so the fair-price ceiling admits it). A row that never
    /// calls it has no market at all, so its draws go unmet exactly as an empty
    /// pool's did.
    struct shelf_ref { std::array<float, resource_count>& quantities; };
    shelf_ref pool_on(entity_id on_body)
    {
        entity_id mid = market_on_body(w, on_body);
        if (mid == null_entity)
        {
            mid = w.create_entity();
            market_component mc{};
            mc.body = on_body;
            for (const auto& [tid, t] : w.tiles)
                if (t.body == on_body && (mc.centre_tile == null_entity || tid < mc.centre_tile))
                    mc.centre_tile = tid;
            for (std::size_t r = 0; r < resource_count; ++r) mc.base_price[r] = 1.0f;
            mc.price = mc.base_price;
            w.markets[mid] = mc;
        }
        return shelf_ref{ w.markets.at(mid).inventory };
    }
    shelf_ref pool() { return pool_on(body); }
};

/// A registry authoring ONE resource on ONE building type in ONE band.
recipe_registry make_registry(building_type bt, era_band band, resource_type good, float qty,
                              int decay = 50, int recovery = 100, int floor = 0,
                              bool ceiling_on = true)
{
    recipe_registry reg;
    // BL-1265: the shelf is the ONLY source of an upkeep good now, and with the
    // fair-price ceiling OFF (reservation_mult 0, the hand-built default) the
    // upkeep draw buys nothing at all. Every row that means "a stocked source
    // meets the draw" runs at the shipped ceiling; R7f keeps the OFF default.
    if (ceiling_on)
    {
        price_band_params pb = reg.price_band();
        pb.reservation_mult = k_shipped_reservation;
        reg.set_price_band(pb);
    }
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

    // An ALL-ZERO table posts nothing — the property that keeps a zero-rate
    // world byte-identical. (BL-1265: it was "creates no pool"; with no pools
    // the register a zero rate must not touch is the want/fill book.)
    {
        fixture f;
        f.build(1, building_type::processing_facility);
        f.pool().quantities[ri(resource_type::tools)] = 10.0f;
        recipe_registry reg = make_registry(building_type::processing_facility,
                                            era_band::ancient, resource_type::tools, 0.0f);
        economy_report rep;
        run_building_upkeep(f.w, reg, rep);
        check(rep.wants.empty() && rep.purchases.empty() && rep.upkeep_wants.empty(),
              "R3 a zero-rate pass posts no want and buys nothing");
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
        f.pool_on(body2).quantities[ri(resource_type::tools)] = 10.0f;

        recipe_registry reg = make_registry(building_type::extraction_site,
                                            era_band::ancient, resource_type::tools, 1.0f);
        run_building_upkeep(f.w, reg, g_upkeep_report);

        check_near(f.pool().quantities[ri(resource_type::tools)], 9.0f,
                   "R1 the home-body building drew from the home-body pool");
        check_near(f.pool_on(body2).quantities[ri(resource_type::tools)], 9.0f,
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
recipe_registry registry_with_reservation(resource_type good, float qty, float reservation,
                                          float shelf_ticks = k_shipped_shelf_ticks)
{
    recipe_registry reg = make_registry(building_type::extraction_site, era_band::any, good, qty);
    price_band_params pb;
    pb.floor_mult       = 0.25f;
    pb.ceil_mult        = 10.0f;
    pb.reservation_mult = reservation;
    pb.shelf_supply_ticks = shelf_ticks;
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
        add_market(f, good, base, base * 1.5f, /*inventory*/ 10.0f); // 1.5x base, under 2x
        recipe_registry reg = registry_with_reservation(good, need, k_shipped_reservation);

        economy_report rep;
        const int before = f.w.buildings.at(f.buildings[0]).supply_factor_permille;
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, rep);

        check_near(want_of(rep, f.corp, market_on_body(f.w, f.body), good), need,
                   "R7a the WHOLE shortfall reaches the want register");
        check_near(fill_of(rep, f.corp, market_on_body(f.w, f.body), good), need,
                   "R7a the fill is what the shelf actually supplied");
        check_near(upkeep_want_of(rep, f.corp, market_on_body(f.w, f.body), good), need,
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
        add_market(f, good, base, base * 2.5f, /*inventory*/ 10.0f); // 2.5x base, over 2x
        recipe_registry reg = registry_with_reservation(good, need, k_shipped_reservation);

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
        add_market(f, good, base, base * 1.5f, /*inventory*/ 0.2f); // shelf < need
        recipe_registry reg = registry_with_reservation(good, need, k_shipped_reservation);

        economy_report rep;
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, rep);

        check_near(want_of(rep, f.corp, market_on_body(f.w, f.body), good), need,
                   "R7c the want is the full shortfall, UNREDUCED by what the shelf holds");
        check_near(fill_of(rep, f.corp, market_on_body(f.w, f.body), good), 0.2f,
                   "R7c the fill is only what was delivered");
        check_near(f.w.markets.begin()->second.inventory[ri(good)], 0.0f,
                   "R7c the shelf is emptied, never driven negative");
        check(t.unmet == 1, "R7c a partly-filled draw is still unmet");
    }

    // --- R7d (the pool is drawn first; only its shortfall is bid) RETIRED with
    // corporation pools (BL-1265): the whole need is the bid, as R7a asserts.

    // --- R7e: unpriced == unbuyable, and it needs no rule of its own ---------
    // A resource with base_price 0 has a reservation ceiling of 0, and no price
    // clears it. Same reading run_construction already takes.
    {
        fixture f;
        f.build(1, building_type::extraction_site);
        add_market(f, good, /*base*/ 0.0f, /*price*/ 0.0f, /*inventory*/ 10.0f);
        recipe_registry reg = registry_with_reservation(good, need, k_shipped_reservation);

        economy_report rep;
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, rep);

        check(rep.wants.empty(), "R7e an UNPRICED good is never bid for");
        check(t.unmet == 1,      "R7e and the draw goes unmet, as it always did");
    }

    // --- R7f: reservation_mult 0 is the ceiling OFF: the draw buys freely -----
    // The DEFAULT. BL-1265: with no corporation pools, "off" can no longer mean
    // the pre-BL-654 pool-only draw (it would leave upkeep no source at all);
    // it means what it means for every other shelf draw — no ceiling.
    {
        fixture f;
        f.build(1, building_type::extraction_site);
        add_market(f, good, base, base, /*inventory*/ 10.0f);
        recipe_registry reg = make_registry(building_type::extraction_site, era_band::any,
                                            good, need, 50, 100, 0,
                                            /*ceiling_on=*/false); // no price band authored
        check_near(reg.price_band().reservation_mult, 0.0f,
                   "R7f reservation_mult defaults to 0 — the feature ships OFF");

        economy_report rep;
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, rep);

        check(!rep.wants.empty(), "R7f at the default the draw bids on the shelf");
        check(f.w.markets.begin()->second.inventory[ri(good)] < 10.0f,
              "R7f at the default the draw buys off the shelf (no ceiling)");
        check(t.unmet == (need > 10.0f ? 1 : 0),
              "R7f at the default the draw is met when the shelf holds the need");
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

// ---------------------------------------------------------------------------
// R10 — BL-1172: A SHELF DRAW PAYS THE POSTED PRICE
// ---------------------------------------------------------------------------
// FINANCE.md § Standing-force upkeep (Ben, 2026-10-03): the same ceiling
// governs every goods draw, building upkeep's included, and "a draw from a
// market's shelf is decided and billed at the price that stood when it was
// made — the price it checked against the ceiling — with one exchange row at
// that price". The shelf posts 1.5x base. (a) Shelf only, at the HISTORICAL
// k = 0: the building is billed 1.5x — though its own want, against zero listed
// supply, resolves this tick's price over the ceiling (k = 0's known cost;
// fair_price_ceiling M5 / unit_upkeep U13). (b) A seller listing plenty,
// shipped k: the price resolves elsewhere — and the building still pays 1.5x.
// (c) k = 4 (not shipped) and (d) the SHIPPED k = 1: the shelf's share is
// supply, so the same want leaves the price at or under the ceiling.
void r10_a_shelf_draw_pays_the_posted_price()
{
    std::printf("\n--- R10  BL-1172: a shelf draw pays the posted price ---\n");

    constexpr resource_type good = resource_type::tools;
    constexpr float need   = 0.5f;
    constexpr float base   = 4.0f;
    constexpr float posted = base * 1.5f;
    constexpr float shelf  = 10.0f;

    auto buyer_rows = [&](const fixture& f, float& qty, float& px) {
        int n = 0; qty = 0.0f; px = -1.0f;
        for (const exchange_record& e : f.w.exchanges.entries)
            if (e.buyer == f.corp && e.resource == good) { ++n; qty += e.quantity; px = e.unit_price; }
        return n;
    };

    // --- R10a: shelf only, HISTORICAL k = 0 — billed at posted --------------
    {
        fixture f;
        f.build(1, building_type::extraction_site);
        const entity_id mid = add_market(f, good, base, posted, shelf);
        recipe_registry reg = registry_with_reservation(good, need, k_shipped_reservation,
                                                        k_historical_shelf_ticks);

        economy_report rep;
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, rep);
        check(t.unmet == 0 && t.weakened == 0, "R10a the shelf covered the draw; the tally says met, not weakened");

        const auto flows = clear_markets(f.w, reg, rep);
        const float resolved = f.w.markets.at(mid).price[ri(good)];
        check(resolved > base * k_shipped_reservation,
              "R10a HISTORICAL k = 0: the draw's own want resolves the price OVER the ceiling (the known cost)");
        const auto fit = flows.find(f.corp);
        check_near(fit == flows.end() ? 0.0f : fit->second.expenditure, need * posted,
                   "R10a billed need x POSTED, not the resolved price");
        float q, px;
        check(buyer_rows(f, q, px) == 1, "R10a exactly one exchange row for the fill");
        check_near(px, posted, "R10a ... at the posted price");
        check_near(q * px, fit == flows.end() ? 0.0f : fit->second.expenditure,
                   "R10a the row's value is exactly what the buyer was charged");
        check_near(f.w.markets.at(mid).inventory[ri(good)], shelf - need,
                   "R10a the shelf gave up exactly the fill");
        check(f.w.buildings.at(f.buildings[0]).supply_factor_permille == 1000,
              "R10a the building, supplied, does not weaken");
    }

    // --- R10d: the SHIPPED k = 1 — the shelf's share holds the price down ------
    {
        fixture f;
        f.build(1, building_type::extraction_site);
        const entity_id mid = add_market(f, good, base, posted, shelf);
        recipe_registry reg = registry_with_reservation(good, need, k_shipped_reservation,
                                                        k_shipped_shelf_ticks);
        economy_report rep;
        run_building_upkeep(f.w, reg, rep);
        const auto flows = clear_markets(f.w, reg, rep);
        const float resolved = f.w.markets.at(mid).price[ri(good)];
        check(resolved <= base * k_shipped_reservation && std::fabs(resolved - posted) > 1e-3f,
              "R10d SHIPPED k = 1: the shelf's share is supply, so the draw's own want leaves the price at or under the ceiling");
        const auto fit = flows.find(f.corp);
        check_near(fit == flows.end() ? 0.0f : fit->second.expenditure, need * posted,
                   "R10d SHIPPED k = 1: ... billed at the posted price");
    }

    // --- R10c: k = 4 (not shipped) — the shelf's share holds the price down ---
    {
        fixture f;
        f.build(1, building_type::extraction_site);
        const entity_id mid = add_market(f, good, base, posted, shelf);
        recipe_registry reg = registry_with_reservation(good, need, k_shipped_reservation,
                                                        k_dormant_shelf_ticks);
        economy_report rep;
        run_building_upkeep(f.w, reg, rep);
        const auto flows = clear_markets(f.w, reg, rep);
        const float resolved = f.w.markets.at(mid).price[ri(good)];
        check(resolved <= base * k_shipped_reservation && std::fabs(resolved - posted) > 1e-3f,
              "R10c k = 4 (not shipped): the shelf's share is supply, so the draw's own want leaves the price at or under the ceiling");
        const auto fit = flows.find(f.corp);
        check_near(fit == flows.end() ? 0.0f : fit->second.expenditure, need * posted,
                   "R10c k = 4 (not shipped): ... billed at the posted price all the same");
    }

    // --- R10b: a seller listing plenty — resolved low, billed at posted ------
    {
        fixture f;
        f.build(1, building_type::extraction_site);
        const entity_id mid = add_market(f, good, base, posted, shelf);
        const entity_id seller = f.w.create_entity();
        corporation_component sc;
        sc.name = "Seller";
        f.w.corporations[seller] = sc;
        f.w.land_goods(seller, mid, ri(good), 20.0f); // BL-1265: a landing the clear lists
        recipe_registry reg = registry_with_reservation(good, need, k_shipped_reservation);

        economy_report rep;
        run_building_upkeep(f.w, reg, rep);
        const auto flows = clear_markets(f.w, reg, rep);
        const float resolved = f.w.markets.at(mid).price[ri(good)];
        check(std::fabs(resolved - posted) > 1e-3f, "R10b the price resolved away from the posted price");
        check_near(flows.at(f.corp).expenditure, need * posted,
                   "R10b billed need x POSTED, the price the draw decided against");
        float q, px;
        check(buyer_rows(f, q, px) == 1, "R10b exactly one exchange row for the fill");
        check_near(q, need, "R10b ... carrying the whole fill");
        check_near(px, posted, "R10b ... at the posted price");
    }
}

// ---------------------------------------------------------------------------
// R11 — BL-1230 (power crosses markets): THE PROVINCE IS THE GRID'S CELL
// ---------------------------------------------------------------------------
// LOGISTICS.md § 3a (Ben, 2026-10-07). A one-row body of `n` tiles, two
// provinces (tiles [0, split) and [split, n)), two markets with centres at the
// two ends — so tiles left of the middle route to M1 and right of it to M2. A
// road mask wires provinces; a water mask makes a strait. Buildings stand on
// the tiles named, owned by one corp.
struct grid_fixture
{
    world     w;
    entity_id body = null_entity;
    entity_id corp = null_entity;
    entity_id m1 = null_entity, m2 = null_entity;
    std::vector<entity_id> tiles;
    std::vector<entity_id> buildings;

    void build(int n, int split, const std::string& road, const std::string& water,
               const std::vector<int>& at, float base = 1.0f, float p1 = 1.0f, float p2 = 1.5f)
    {
        body = w.create_entity();
        w.bodies[body] = body_component{};
        w.bodies[body].name        = "Wire";
        w.bodies[body].grid_width  = n;
        w.bodies[body].grid_height = 1;
        for (int x = 0; x < n; ++x)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body       = body;
            tc.grid_x     = x;
            tc.grid_y     = 0;
            tc.substrate  = (water[static_cast<std::size_t>(x)] == 'w') ? terrain_substrate::coast
                                                                        : terrain_substrate::sedimentary;
            tc.road_level = (road[static_cast<std::size_t>(x)] == 'r') ? 1 : 0;
            w.tiles[t] = tc;
            tiles.push_back(t);
        }
        // The partition: ascending ids, each id its lowest tile (province.hpp).
        province a, b;
        a.body = b.body = body;
        for (int x = 0; x < n; ++x)
            (x < split ? a : b).tiles.push_back(tiles[static_cast<std::size_t>(x)]);
        a.id = static_cast<std::uint32_t>(a.tiles.front());
        b.id = static_cast<std::uint32_t>(b.tiles.front());
        w.provinces.provinces = { a, b };
        for (const province& p : w.provinces.provinces)
            for (const entity_id t : p.tiles)
                w.provinces.tile_province[t] = p.id;

        corp = w.create_entity();
        corporation_component cc;
        cc.name = "Wire Holdings";
        cc.starting_capital = 1000.0f;
        cc.balance = 1000.0f;
        cc.is_player = true;
        for (const int x : at)
        {
            const entity_id bid = w.create_entity();
            building_component bc{};
            bc.tile = tiles[static_cast<std::size_t>(x)];
            bc.type = building_type::extraction_site;
            bc.workforce_assigned = 0.5f;
            w.buildings[bid] = bc;
            buildings.push_back(bid);
            cc.assets.push_back(bid);
        }
        w.corporations[corp] = cc;

        auto market = [&](entity_id centre, float price) {
            const entity_id mid = w.create_entity();
            market_component mc{};
            mc.body = body;
            mc.centre_tile = centre;
            mc.base_price[ri(resource_type::power)] = base;
            mc.price[ri(resource_type::power)]      = price;
            mc.base_price[ri(resource_type::construction_capacity)] = base;
            mc.price[ri(resource_type::construction_capacity)]      = price;
            w.markets[mid] = mc;
            return mid;
        };
        m1 = market(tiles.front(), p1);
        m2 = market(tiles.back(), p2);
    }

    std::uint32_t grid_at(int x) { return tile_power_grid(w, tiles[static_cast<std::size_t>(x)]); }
    float& shelf(entity_id mid, resource_type r = resource_type::power) { return w.markets.at(mid).inventory[ri(r)]; }
};

recipe_registry power_registry(float need)
{
    recipe_registry reg = registry_with_reservation(resource_type::power, need, k_shipped_reservation);
    grid_goods_params g;
    g.is_grid[ri(resource_type::power)] = true;
    g.is_grid[ri(resource_type::construction_capacity)] = true;
    reg.set_grid_goods(g);
    return reg;
}

void r11_the_province_is_the_grid_cell()
{
    std::printf("\n--- R11  BL-1230: a wired province draws power from any shelf on its grid ---\n");
    constexpr float need = 0.4f;

    // --- R11a: across markets, billed at the BUYER's market ------------------
    {
        grid_fixture f;
        f.build(6, 3, "rrrrrr", "......", { 4 });
        check(market_for_tile(f.w, f.tiles[4]) == f.m2, "R11a fixture: the building's tile routes to M2");
        check(f.grid_at(0) != 0 && f.grid_at(0) == f.grid_at(5), "R11a two road-joined provinces are one grid");
        f.shelf(f.m1) = 10.0f; // all the power is on M1's shelf; M2's is empty
        recipe_registry reg = power_registry(need);
        economy_report rep;
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, rep);
        check(t.unmet == 0, "R11a a building in M2's catchment is met from M1's shelf");
        check(f.w.buildings.at(f.buildings[0]).supply_factor_permille == 1000, "R11a ... and does not weaken");
        check_near(f.shelf(f.m1), 10.0f - need, "R11a M1's shelf gave up exactly the draw");
        const auto pit = rep.purchases.find({ f.corp, f.m2 });
        check_near(pit == rep.purchases.end() ? 0.0f : pit->second[ri(resource_type::power)], need,
                   "R11a the fill is booked to the buyer's OWN market (corp, M2)");
        const auto flows = clear_markets(f.w, reg, rep);
        const auto fit = flows.find(f.corp);
        check_near(fit == flows.end() ? 0.0f : fit->second.expenditure, need * 1.5f,
                   "R11a billed at M2's posted price (1.5), not M1's (1.0)");
    }

    // --- R11b: a strait joins; a wider water run does not ---------------------
    {
        grid_fixture f;
        f.build(8, 3, "rr..rr..", "..ww....", {});
        check(f.grid_at(1) != 0 && f.grid_at(1) == f.grid_at(4),
              "R11b roads either side of a 2-tile strait (kMaxCrossingTiles) are one grid");
        grid_fixture g;
        g.build(9, 3, "rr...rr..", "..www....", {});
        check(g.grid_at(1) != 0 && g.grid_at(5) != 0 && g.grid_at(1) != g.grid_at(5),
              "R11b ... a 3-tile water run is open water: two grids");
    }

    // --- R11c: a dark province strikes power ---------------------------------
    {
        grid_fixture f;
        f.build(6, 3, "rrr...", "......", { 4 });
        check(f.grid_at(4) == 0, "R11c a province with no road is dark");
        f.shelf(f.m1) = 10.0f;
        recipe_registry reg = power_registry(need);
        economy_report rep;
        for (int i = 0; i < 5; ++i)
            run_building_upkeep(f.w, reg, rep);
        check(f.w.buildings.at(f.buildings[0]).supply_factor_permille == 1000,
              "R11c a building in a dark province does not weaken for power it cannot receive (NR-782 b)");
        check_near(f.shelf(f.m1), 10.0f, "R11c ... and draws nothing off the grid's shelf");
    }

    // --- R11c2: an ISOLATED road wires a province onto a grid with no shelf ----
    // RULED (Ben, 2026-10-07): a wired grid with no generation keeps decaying as
    // written — wired, it bids and goes short; dark, it would not. This row pins
    // the ruled behaviour.
    {
        grid_fixture f;
        f.build(6, 3, "rr..r.", "......", { 4 });
        check(f.grid_at(4) != 0 && f.grid_at(4) != f.grid_at(0), "R11c2 an isolated road is a grid of its own");
        f.shelf(f.m1) = 10.0f;
        recipe_registry reg = power_registry(need);
        economy_report rep;
        run_building_upkeep(f.w, reg, rep);
        check(f.w.buildings.at(f.buildings[0]).supply_factor_permille == 950,
              "R11c2 ... with nothing on it, the wired building goes short (worse off than dark)");
    }

    // --- R11d: a short grid is shared PRO RATA --------------------------------
    {
        grid_fixture f;
        f.build(6, 3, "rrrrrr", "......", { 4, 5 });
        f.shelf(f.m1) = need; // enough for one of the two
        recipe_registry reg = power_registry(need);
        economy_report rep;
        const building_upkeep_tick t = run_building_upkeep(f.w, reg, rep);
        check(t.unmet == 2, "R11d both buildings on a half-covered grid are short (no id decides)");
        check_near(rep.purchases[{ f.corp, f.m2 }][ri(resource_type::power)], need,
                   "R11d the whole shelf is drawn (half each)");
        check_near(f.shelf(f.m1), 0.0f, "R11d ... leaving the grid's shelf empty");
    }

    // --- R11e: the pooled price converges per market; capacity stays local ----
    {
        auto run = [&](resource_type good, float& p1, float& p2) {
            grid_fixture f;
            f.build(6, 3, "rrrrrr", "......", {}, 1.0f, 1.0f, 5.0f);
            // A city at each centre anchors the reach field, so M1 may LIST
            // power at all (the BL-708 listing gate, unchanged).
            f.w.population_centre_tile[f.w.create_entity()] = f.tiles.front();
            f.w.population_centre_tile[f.w.create_entity()] = f.tiles.back();
            const entity_id seller = f.w.create_entity();
            corporation_component sc;
            sc.name = "Generator Co";
            f.w.corporations[seller] = sc;
            recipe_registry reg = power_registry(need);
            for (int i = 0; i < 60; ++i)
            {
                f.w.land_goods(seller, f.m1, ri(good), 10.0f); // M1 lists 10 (BL-1265: a landing)
                economy_report rep;
                rep.wants[{ f.corp, f.m2 }][ri(good)] = 10.0f;         // M2 wants 10
                clear_markets(f.w, reg, rep);
            }
            p1 = f.w.markets.at(f.m1).price[ri(good)];
            p2 = f.w.markets.at(f.m2).price[ri(good)];
        };
        float p1 = 0, p2 = 0;
        run(resource_type::power, p1, p2);
        std::printf("      power: M1 %.3f  M2 %.3f\n", p1, p2);
        check(std::fabs(p1 - p2) < 0.02f && p2 < 2.0f,
              "R11e POWER: each market's price converges on the grid's (supply 10 / demand 10 across two markets)");
        run(resource_type::construction_capacity, p1, p2);
        std::printf("      capacity: M1 %.3f  M2 %.3f\n", p1, p2);
        check(p1 < 0.5f && p2 > 5.0f,
              "R11e CAPACITY (a grid good the ruling does not name) still prices on its own market");
    }

    // --- R11g: the shelf cap is taken ONCE, at the grid (review round 2) -------
    // All the grid's power stands on M1's shelf; nothing is listed this tick;
    // the only demand is M2's building. Summed market by market, M1's capped
    // share (min(shelf, k x LOCAL demand) = 0) and M2's (empty shelf) would price
    // the grid as empty and drive M2 over the ceiling; capped at the grid, the
    // shelf answers the grid's demand and M2 stays under it.
    {
        grid_fixture f;
        f.build(6, 3, "rrrrrr", "......", { 4 }, 1.0f, 1.0f, 1.9f);
        f.shelf(f.m1) = 100.0f;
        recipe_registry reg = power_registry(need); // the SHIPPED k = 1
        bool under = true, met = true;
        float worst = 0.0f;
        for (int i = 0; i < 20; ++i)
        {
            economy_report rep;
            const building_upkeep_tick t = run_building_upkeep(f.w, reg, rep);
            if (t.unmet != 0) met = false;
            clear_markets(f.w, reg, rep);
            const float p2 = f.w.markets.at(f.m2).price[ri(resource_type::power)];
            worst = std::max(worst, p2);
            if (p2 > 1.0f * k_shipped_reservation) under = false;
        }
        std::printf("      M2 highest resolved price over 20 ticks %.3f (ceiling %.3f)\n", worst, k_shipped_reservation);
        check(under, "R11g shelf on M1, demand only at M2: the grid price stays under the ceiling");
        check(met && f.w.buildings.at(f.buildings[0]).supply_factor_permille == 1000,
              "R11g ... and M2's building is filled every tick from M1's shelf");
        check_near(f.shelf(f.m1), 100.0f - 20.0f * need, "R11g M1's shelf gave up exactly 20 draws", 1e-2f);
    }

    // --- R11h: construction capacity's wire and draw stay LOCAL ----------------
    // The ruling names power; capacity keeps the tile-reach wire (BL-708) and its
    // own market's shelf.
    {
        auto cap_registry = [&]() {
            recipe_registry reg = registry_with_reservation(resource_type::construction_capacity, need,
                                                            k_shipped_reservation);
            grid_goods_params g;
            g.is_grid[ri(resource_type::power)] = true;
            g.is_grid[ri(resource_type::construction_capacity)] = true;
            reg.set_grid_goods(g);
            return reg;
        };
        auto anchor = [](grid_fixture& f) {
            f.w.population_centre_tile[f.w.create_entity()] = f.tiles.front();
            f.w.population_centre_tile[f.w.create_entity()] = f.tiles.back();
        };
        // Reached by the tile wire, in a road-wired province, capacity only on M1:
        // the grid does NOT carry it — the building goes short at its own M2.
        {
            grid_fixture f;
            f.build(6, 3, "rrrrrr", "......", { 4 });
            anchor(f);
            f.shelf(f.m1, resource_type::construction_capacity) = 10.0f;
            recipe_registry reg = cap_registry();
            economy_report rep;
            const building_upkeep_tick t = run_building_upkeep(f.w, reg, rep);
            check(t.unmet == 1, "R11h capacity on M1's shelf does NOT reach a building at M2 (no grid draw)");
            check_near(f.shelf(f.m1, resource_type::construction_capacity), 10.0f,
                       "R11h ... M1's capacity shelf is untouched");
        }
        // A DARK province but a reached tile: capacity still draws at its own market.
        {
            grid_fixture f;
            f.build(6, 3, "rrr...", "......", { 4 });
            anchor(f);
            f.shelf(f.m2, resource_type::construction_capacity) = 10.0f;
            recipe_registry reg = cap_registry();
            economy_report rep;
            const building_upkeep_tick t = run_building_upkeep(f.w, reg, rep);
            check(f.grid_at(4) == 0 && t.unmet == 0,
                  "R11h a dark province does not strike capacity: its wire is the tile's reach");
            check_near(f.shelf(f.m2, resource_type::construction_capacity), 10.0f - need,
                       "R11h ... drawn from its OWN market's shelf");
        }
        // An unreached tile (no anchor) strikes capacity, as BL-708 always did.
        {
            grid_fixture f;
            f.build(6, 3, "rrrrrr", "......", { 4 });
            f.shelf(f.m2, resource_type::construction_capacity) = 10.0f;
            recipe_registry reg = cap_registry();
            economy_report rep;
            for (int i = 0; i < 3; ++i)
                run_building_upkeep(f.w, reg, rep);
            check(f.w.buildings.at(f.buildings[0]).supply_factor_permille == 1000
                      && f.shelf(f.m2, resource_type::construction_capacity) == 10.0f,
                  "R11h an unreached tile strikes capacity (no draw, no decay) even in a wired province");
        }
    }

    // --- R11f: a road write invalidates the grid ------------------------------
    {
        grid_fixture f;
        f.build(6, 3, "rrr...", "......", {});
        check(f.grid_at(4) == 0, "R11f before: the second province is dark");
        recipe_registry reg;
        const construction_result res = place_road(f.w, reg, f.corp, f.tiles[3], 1);
        check(res == construction_result::placed, "R11f place_road placed a Track on the second province's edge");
        check(f.grid_at(4) != 0 && f.grid_at(4) == f.grid_at(0),
              "R11f after: the cached grid was invalidated and the province joined the first's grid");
    }
}

// ---------------------------------------------------------------------------
// R12 — BL-1232 (power plants per grid): a generator serves its MARKET CENTRE's
// grid, and unpowered short grids are served first
// ---------------------------------------------------------------------------
// Fixture: six tiles, provinces [0,2) and [2,6), roads "rr..r." — two grids
// (A: tiles 0-1, B: tiles 2-5). M1's centre is tile 0 (grid A), M2's tile 5
// (grid B). Tile 2 stands on grid B but routes to M1, so a plant there LISTS
// on grid A's shelf; tile 3 routes to M2. Each extraction site draws 1.0 power;
// one plant makes 3.0 (half a plant = 1.5, so a grid needing 2.0 is counted).

namespace {
struct power_plant_reg
{
    recipe_registry reg;
    std::uint16_t   plant = 0;
    float           plant_output = 0.0f;
};

power_plant_reg power_plant_registry(float need)
{
    power_plant_reg p;
    p.reg = power_registry(need);
    building_economics pe;
    pe.base_rate = 2.0f;   // x the 0.5 default staffing = 1 batch a tick
    p.reg.set_economics(building_type::processing_facility, pe);
    recipe r;
    r.name  = "test_power_plant";
    r.group = "Power Generation";
    r.inputs [ri(resource_type::petroleum)] = 1.0f;
    r.outputs[ri(resource_type::power)]     = 3.0f;
    p.plant        = p.reg.add_recipe(r);
    p.plant_output = 3.0f;
    return p;
}

entity_id add_plant(grid_fixture& f, int x, std::uint16_t recipe_id)
{
    const entity_id bid = f.w.create_entity();
    building_component bc{};
    bc.tile               = f.tiles[static_cast<std::size_t>(x)];
    bc.type               = building_type::processing_facility;
    bc.recipe             = recipe_id;
    bc.workforce_assigned = 0.5f;
    bc.workforce_target   = 100;
    f.w.buildings[bid]    = bc;
    f.w.corporations.at(f.corp).assets.push_back(bid);
    return bid;
}
} // namespace

void r12_a_plant_serves_its_market_centres_grid()
{
    std::printf("\n--- R12  BL-1232: generation is keyed by the grid its market centre is on ---\n");
    const power_plant_reg P = power_plant_registry(1.0f);

    // --- R12a/b: the gap is closed only by a plant whose market centre is on it
    for (const int at : { 2, 3 })
    {
        grid_fixture f;
        f.build(6, 2, "rr..r.", "......", { 0, 1, 4, 5 });
        const std::uint32_t gA = f.grid_at(0), gB = f.grid_at(4);
        add_plant(f, at, P.plant);
        if (at == 2)
        {
            check(gA != 0 && gB != 0 && gA != gB, "R12 fixture: two separate grids");
            check(f.grid_at(2) == gB && market_for_tile(f.w, f.tiles[2]) == f.m1,
                  "R12 fixture: tile 2 stands on grid B but routes to M1 (centre on grid A)");
            check(tile_feed_power_grid(f.w, f.tiles[2]) == gA, "R12a tile_feed_power_grid reads the market CENTRE's grid");
        }
        std::set<std::uint32_t> short_grids, unpowered;
        const float gap = body_power_grid_gap(f.w, P.reg, f.body, P.plant_output, short_grids, &unpowered);
        if (at == 2)
        {
            check(short_grids == std::set<std::uint32_t>{ gB } && unpowered == std::set<std::uint32_t>{ gB },
                  "R12a a plant ON grid B whose market centre is on A leaves B short and unpowered");
            check_near(gap, 2.0f, "R12a ... by B's whole need (the plant's output is credited to A)");
        }
        else
        {
            check(short_grids == std::set<std::uint32_t>{ gA },
                  "R12b the same plant at tile 3 (M2, centre on B) closes B's gap and leaves A short");
            check_near(gap, 2.0f, "R12b ... by A's whole need");
        }
    }

    // --- R12c: two short grids, one firm — the UNPOWERED one is served ---------
    {
        grid_fixture f;
        // Six drawers on A (need 6.0), two on B (need 2.0); a plant at tile 2
        // feeds A (3.0): A short by 3 but powered, B short by only 2 and unpowered.
        f.build(6, 2, "rr..r.", "......", { 0, 0, 0, 1, 1, 1, 4, 5 });
        const std::uint32_t gA = f.grid_at(0), gB = f.grid_at(4);
        std::set<std::uint32_t> serve;
        float gap = power_grids_to_serve(f.w, P.reg, f.body, P.plant_output, serve);
        check(serve == std::set<std::uint32_t>{ gA, gB },
              "R12c with no generation anywhere, both short grids may be served");
        check_near(gap, 8.0f, "R12c ... and the gap is both grids' need");
        add_plant(f, 2, P.plant);
        gap = power_grids_to_serve(f.w, P.reg, f.body, P.plant_output, serve);
        check(serve == std::set<std::uint32_t>{ gB },
              "R12c two short grids, one firm: the UNPOWERED grid (B) is the one served, not the already-powered grid A with the LARGER shortfall");
        check_near(gap, 5.0f, "R12c ... while the gap still counts both shortfalls (3 + 2)");
    }

    // --- R12d: a grid is POWERED once a power firm is CHARTERED on it ----------
    // "Every grid gets a plant before any gets a second" (Ben, 2026-10-08). No
    // generator stands anywhere yet; the walk has chartered one power firm on
    // the core grid A. A is then powered (live output or not) and the next firm
    // may serve only B — the core grid cannot take every capped firm first.
    {
        grid_fixture f;
        f.build(6, 2, "rr..r.", "......", { 0, 0, 0, 1, 1, 1, 4, 5 });
        const std::uint32_t gA = f.grid_at(0), gB = f.grid_at(4);
        std::set<std::uint32_t> serve;
        const std::set<std::uint32_t> none, onA = { gA }, both = { gA, gB };
        power_grids_to_serve(f.w, P.reg, f.body, P.plant_output, serve, &none);
        check(serve == std::set<std::uint32_t>{ gA, gB }, "R12d nothing chartered: both short grids may be served");
        power_grids_to_serve(f.w, P.reg, f.body, P.plant_output, serve, &onA);
        check(serve == std::set<std::uint32_t>{ gB },
              "R12d a firm chartered on A (no output yet): the second firm serves only B, though A's shortfall is larger");
        power_grids_to_serve(f.w, P.reg, f.body, P.plant_output, serve, &both);
        check(serve == std::set<std::uint32_t>{ gA, gB },
              "R12d every short grid has a firm: a second firm may go to either");
    }

    // --- R12e: an UNREACHABLE unpowered grid never holds up the others ---------
    // A is powered (a plant at tile 2 feeds it) but short; B is short, unpowered
    // — and the deciding centre's windows feed only A. Unpowered-first must not
    // narrow the firm to B, which it cannot reach: the core shortfall is served.
    {
        grid_fixture f;
        f.build(6, 2, "rr..r.", "......", { 0, 0, 0, 1, 1, 1, 4, 5 });
        const std::uint32_t gA = f.grid_at(0), gB = f.grid_at(4);
        add_plant(f, 2, P.plant);
        std::set<std::uint32_t> serve;
        const std::set<std::uint32_t> none, only_A = { gA }, both = { gA, gB };
        power_grids_to_serve(f.w, P.reg, f.body, P.plant_output, serve, &none, &both);
        check(serve == std::set<std::uint32_t>{ gB }, "R12e B reachable: the unpowered grid B is served first");
        power_grids_to_serve(f.w, P.reg, f.body, P.plant_output, serve, &none, &only_A);
        check(serve == std::set<std::uint32_t>{ gA, gB },
              "R12e B unreachable from the windows: it is dropped and every short grid is served (A's ground can take the plant)");
        check(serve.count(gA) == 1, "R12e ... so the feedable core shortfall gets the plant");
    }
}

// ---------------------------------------------------------------------------
// R13 — BL-1232: the workforce solver forecasts power on the plant's GRID
// ---------------------------------------------------------------------------
// One plant on tile 0 (M1). M1 holds a glut — 30 listed against 1 bid — while
// M2, joined or not, bids 60. Petroleum costs the plant 2 a batch at M1's
// posted price; the plant makes 3 power a batch. Read on M1 alone, power sits
// at the floor (0.25 x 3 = 0.75 a batch) and the plant should not run; read on
// a grid that includes M2's bid it earns well above 2 and should.
void r13_the_solver_reads_power_on_its_grid()
{
    std::printf("\n--- R13  BL-1232: solve_workforce_target reads power's pooled grid figures ---\n");
    const power_plant_reg P = power_plant_registry(1.0f);
    for (const bool joined : { true, false })
    {
        grid_fixture f;
        f.build(6, 3, joined ? "rrrrrr" : "rr..r.", "......", {});
        const entity_id plant = add_plant(f, 0, P.plant);
        market_component& m1 = f.w.markets.at(f.m1);
        market_component& m2 = f.w.markets.at(f.m2);
        const std::size_t pw = ri(resource_type::power), pe = ri(resource_type::petroleum);
        m1.supply[pw] = 30.0f; m1.demand[pw] = 1.0f;
        m2.supply[pw] = 0.0f;  m2.demand[pw] = 60.0f;
        m1.base_price[pe] = 2.0f; m1.price[pe] = 2.0f;
        const bool one_grid = f.grid_at(0) == f.grid_at(5);
        check(one_grid == joined, joined ? "R13 fixture: the two markets share a grid"
                                         : "R13 fixture: the two markets are on separate grids");
        const int wt = solve_workforce_target(f.w, P.reg, f.w.buildings.at(plant), 1.0f);
        if (joined)
            check(wt > 0, "R13a on one grid the plant reads the GRID's bid (61 against 30) and runs");
        else
            check(wt == 0, "R13b on its own grid the same plant reads M1's glut and is zeroed (the pool is the grid's, no wider)");
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
    r10_a_shelf_draw_pays_the_posted_price();
    r11_the_province_is_the_grid_cell();
    r12_a_plant_serves_its_market_centres_grid();
    r13_the_solver_reads_power_on_its_grid();

    std::printf("\n%s — %d failure(s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
