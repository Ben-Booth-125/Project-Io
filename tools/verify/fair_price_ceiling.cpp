// fair_price_ceiling — BL-1172: the fair-price ceiling governs EVERY goods draw,
// and every shelf draw pays the POSTED price.
//
// FINANCE.md § Standing-force upkeep (Ben, 2026-10-03):
//   "A draw pays the posted price: a draw from a market's shelf is decided and
//    billed at the price that stood when it was made — the price it checked
//    against the ceiling — with one exchange row at that price."
//   "The same ceiling governs every goods draw: unit and building upkeep,
//    processor inputs and construction alike buy only at or under it."
//
// unit_upkeep (U10/U11) and building_upkeep (R10) hold the upkeep draws; this
// harness holds the other two draws and the cases where draws MEET on a shelf:
//
//   P1  a processor buys its input at or under the ceiling, billed at posted
//   P2  a processor refuses its input over the ceiling: runs on its pool alone
//   C1  a construction site under the ceiling draws and is billed at posted
//   C2  a construction site over the ceiling pauses: nothing drawn or billed
//   S1  ONE corp draws the same good for a processor AND for upkeep on one
//       market: one fill, billed once at posted, one exchange row
//   S2  TWO corps draw the same good on one market: each billed its own fill
//       at posted, one row each, the shelf drained by both
//   Z   reservation_mult 0 (the hand-built default): a processor buys as it
//       did before the ceiling existed — the OFF switch
//   M   MULTI-TICK (ten ticks of run_economy_step -> clear_markets each):
//       M1/M2 a processor and a construction site keep buying, every tick, on
//       a shelf-only market under the ceiling (the shelf is supply, MARKETS.md
//       § Price resolution); M3/M4 over the ceiling on an empty shelf they do
//       not bid, and the price eases after every such tick (Ben, 2026-10-03:
//       "a draw over it does not bid either, so its want leaves the price")
//
// Every case runs the real tick (`run_economy_step`, then `clear_markets`), and
// every price is placed either side of the shipped 2.0 multiple. The resolved
// price is checked to DIFFER from the posted one wherever billing is asserted,
// so a row cannot pass by billing the resolved price by accident.
//
// Build: node tools/verify/build_harness.js fair_price_ceiling --run

#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* what, double got = 0.0, double want = 0.0)
{
    std::printf("  %s  %s", ok ? "PASS" : "FAIL", what);
    if (!ok)
        std::printf("   (got %.4f, want %.4f)", got, want);
    std::printf("\n");
    if (!ok)
        ++g_failures;
}

bool near(double a, double b) { return std::fabs(a - b) < 1e-3; }

std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

/// The SHIPPED reservation multiple (scripts/economy.lua price_band), restated:
/// this harness reads no Lua. Prices are placed at 1.5x and 2.5x base around it.
constexpr float k_shipped_reservation = 2.0f;

/// The SHIPPED shelf share of supply, in ticks of demand (scripts/economy.lua
/// `price_band.shelf_supply_ticks`, BL-1172 — a first cut, Ben to set).
/// Restated, not loaded; the multi-tick rows also run at every k of the sweep.
constexpr float k_shipped_shelf_ticks = 4.0f;
constexpr float k_shelf_tick_sweep[] = {1.0f, 2.0f, 4.0f, 8.0f, 16.0f};

constexpr resource_type IRON  = resource_type::iron_ore;
constexpr resource_type STEEL = resource_type::steel;
constexpr float k_iron_base  = 2.5f;
constexpr float k_steel_base = 8.0f;

struct scene
{
    world     w;
    entity_id body   = null_entity;
    entity_id market = null_entity;
    entity_id tile   = null_entity;
};

scene make_scene(float iron_posted, float steel_posted, float shelf)
{
    scene s;
    s.body = s.w.create_entity();
    s.w.bodies[s.body] = body_component{};
    s.tile = s.w.create_entity();
    {
        tile_component tc{};
        tc.body = s.body;
        tc.substrate = terrain_substrate::sedimentary;
        tc.cover = terrain_cover::grass;
        tc.cover_density = 150;
        s.w.tiles[s.tile] = tc;
    }
    s.market = s.w.create_entity();
    market_component mc;
    mc.body        = s.body;
    mc.centre_tile = s.tile;
    mc.base_price[ri(IRON)]  = k_iron_base;
    mc.base_price[ri(STEEL)] = k_steel_base;
    mc.price[ri(IRON)]       = iron_posted;
    mc.price[ri(STEEL)]      = steel_posted;
    mc.inventory[ri(IRON)]   = shelf;
    mc.inventory[ri(STEEL)]  = shelf;
    s.w.markets[s.market] = mc;
    return s;
}

/// A corp (not AI-driven) owning `b`, with `iron_pool` iron in its market pool.
entity_id add_corp(scene& s, entity_id b, float iron_pool)
{
    const entity_id c = s.w.create_entity();
    corporation_component cc;
    cc.name = "Test Co";
    cc.is_player = true; // excluded from the strategic tier; see econ_harness
    cc.starting_capital = 10000.0f;
    cc.balance = 10000.0f;
    cc.assets.push_back(b);
    s.w.corporations[c] = cc;
    if (iron_pool > 0.0f)
        s.w.pool_at(c, s.market).quantities[ri(IRON)] = iron_pool;
    return c;
}

entity_id add_tile(scene& s)
{
    const entity_id t = s.w.create_entity();
    tile_component tc{};
    tc.body = s.body;
    tc.substrate = terrain_substrate::sedimentary;
    tc.cover = terrain_cover::grass;
    tc.cover_density = 150;
    s.w.tiles[t] = tc;
    return t;
}

entity_id add_processor(scene& s, uint16_t recipe_id, entity_id tile)
{
    const entity_id b = s.w.create_entity();
    building_component bc{};
    bc.tile = tile;
    bc.type = building_type::processing_facility;
    bc.workforce_assigned = 0.5f;
    bc.recipe = recipe_id;
    s.w.buildings[b] = bc;
    return b;
}

/// Processor: steel from 2 iron a batch, base_rate 8 x workforce 0.5 = 4
/// batches, so a full run needs 8 iron. Construction of a processing facility
/// costs 24 steel over 3 ticks (8 a tick).
recipe_registry make_registry(float reservation, uint16_t& steel_id,
                              float shelf_ticks = k_shipped_shelf_ticks)
{
    recipe_registry reg;
    reg.set_thresholds(/*t_full=*/1.0f, /*t_idle=*/0.2f);
    building_economics pr;
    pr.base_rate = 8.0f; pr.maintenance = 10.0f; pr.base_wage = 12.0f;
    pr.build_cost = 300.0f;
    pr.build_duration_ticks = 3.0f;
    pr.resource_build_cost[ri(STEEL)] = 24.0f;
    reg.set_economics(building_type::processing_facility, pr);
    construction_params cp; cp.max_stretch = 10.0f;
    reg.set_construction(cp);
    recipe steel;
    steel.name = "steel";
    steel.inputs[ri(IRON)]   = 2.0f;
    steel.outputs[ri(STEEL)] = 1.0f;
    steel_id = reg.add_recipe(steel);
    price_band_params pb;
    pb.floor_mult = 0.25f; pb.ceil_mult = 10.0f; pb.reservation_mult = reservation;
    pb.shelf_supply_ticks = shelf_ticks;
    reg.set_price_band(pb);
    return reg;
}

float fill_of(const economy_report& rep, entity_id corp, entity_id market, resource_type r)
{
    const auto it = rep.purchases.find(std::make_pair(corp, market));
    return it == rep.purchases.end() ? 0.0f : it->second[ri(r)];
}

float spent(const std::unordered_map<entity_id, corp_cash_flow>& flows, entity_id corp)
{
    const auto it = flows.find(corp);
    return it == flows.end() ? 0.0f : it->second.expenditure;
}

/// The buy-side exchange rows of `corp` in good `r`: count, total qty, and the
/// one price they carry (-1 when none, NaN-free; mixed prices make `one_px` false).
struct rows { int n = 0; float qty = 0.0f; float px = -1.0f; bool one_px = true; };
rows buyer_rows(const world& w, entity_id corp, resource_type r)
{
    rows out;
    for (const exchange_record& e : w.exchanges.entries)
        if (e.buyer == corp && e.resource == r)
        {
            if (out.n > 0 && !near(out.px, e.unit_price))
                out.one_px = false;
            ++out.n;
            out.qty += e.quantity;
            out.px = e.unit_price;
        }
    return out;
}

float output_of(const economy_report& rep, entity_id b)
{
    for (const auto& br : rep.buildings)
        if (br.building == b)
            return br.output_quantity;
    return -1.0f;
}

// ---------------------------------------------------------------------------

void p_processor()
{
    std::printf("\n--- P  processor inputs obey the ceiling and pay the posted price ---\n");
    const float under = k_iron_base * 1.5f, over = k_iron_base * 2.5f;

    // P1: under — the pool's 4 iron, the shelf the other 4, billed at posted.
    {
        uint16_t sid = 0;
        recipe_registry reg = make_registry(k_shipped_reservation, sid);
        scene s = make_scene(under, k_steel_base, 1000.0f);
        const entity_id b = add_processor(s, sid, s.tile);
        const entity_id c = add_corp(s, b, 4.0f);

        economy_report rep = run_economy_step(s.w, reg);
        check(near(fill_of(rep, c, s.market, IRON), 4.0f), "P1 under the ceiling the processor buys its shortfall (4)",
              fill_of(rep, c, s.market, IRON), 4.0);
        check(near(output_of(rep, b), 4.0f), "P1 ... and runs a full batch (4 steel)", output_of(rep, b), 4.0);
        const auto flows = clear_markets(s.w, reg, rep);
        const float resolved = s.w.markets.at(s.market).price[ri(IRON)];
        check(!near(resolved, under), "P1 the iron price resolved away from the posted price", resolved, under);
        check(near(spent(flows, c), 4.0f * under), "P1 billed fill x POSTED", spent(flows, c), 4.0 * under);
        const rows r = buyer_rows(s.w, c, IRON);
        check(r.n == 1 && near(r.qty, 4.0f) && near(r.px, under), "P1 one exchange row, the fill, at the posted price",
              r.px, under);
    }

    // P2: over — the shelf is not on offer; the processor runs on its pool alone.
    {
        uint16_t sid = 0;
        recipe_registry reg = make_registry(k_shipped_reservation, sid);
        scene s = make_scene(over, k_steel_base, 1000.0f);
        const entity_id b = add_processor(s, sid, s.tile);
        const entity_id c = add_corp(s, b, 4.0f);

        economy_report rep = run_economy_step(s.w, reg);
        check(near(fill_of(rep, c, s.market, IRON), 0.0f), "P2 over the ceiling the processor buys NOTHING",
              fill_of(rep, c, s.market, IRON), 0.0);
        check(near(s.w.markets.at(s.market).inventory[ri(IRON)], 1000.0f), "P2 the iron shelf is untouched",
              s.w.markets.at(s.market).inventory[ri(IRON)], 1000.0);
        check(near(output_of(rep, b), 2.0f), "P2 it runs on its own 4 iron: coverage 0.5, 2 steel",
              output_of(rep, b), 2.0);
        const auto flows = clear_markets(s.w, reg, rep);
        check(near(spent(flows, c), 0.0f), "P2 nothing billed", spent(flows, c), 0.0);
        check(buyer_rows(s.w, c, IRON).n == 0, "P2 no exchange row for a purchase that did not happen");
    }

    // Z: reservation_mult 0 — the OFF switch; the processor buys as it always did.
    {
        uint16_t sid = 0;
        recipe_registry reg = make_registry(/*reservation=*/0.0f, sid);
        scene s = make_scene(over, k_steel_base, 1000.0f);
        const entity_id b = add_processor(s, sid, s.tile);
        const entity_id c = add_corp(s, b, 4.0f);
        economy_report rep = run_economy_step(s.w, reg);
        check(near(fill_of(rep, c, s.market, IRON), 4.0f),
              "Z with the ceiling OFF (reservation_mult 0) a processor buys even at 2.5x base",
              fill_of(rep, c, s.market, IRON), 4.0);
    }
}

void c_construction()
{
    std::printf("\n--- C  construction obeys the ceiling and pays the posted price ---\n");
    const float under = k_steel_base * 1.5f, over = k_steel_base * 2.5f;

    auto site = [&](scene& s) {
        const entity_id b = s.w.create_entity();
        building_component bc{};
        bc.tile = s.tile;
        bc.type = building_type::processing_facility;
        bc.ticks_remaining = 3;
        s.w.buildings[b] = bc;
        return b;
    };

    // C1: under — one tick of steel (8) drawn, billed at posted, progress 3 -> 2.
    {
        uint16_t sid = 0;
        recipe_registry reg = make_registry(k_shipped_reservation, sid);
        scene s = make_scene(k_iron_base, under, 1000.0f);
        const entity_id b = site(s);
        const entity_id c = add_corp(s, b, 0.0f);
        economy_report rep = run_economy_step(s.w, reg);
        check(s.w.buildings.at(b).ticks_remaining == 2, "C1 under the ceiling the build advances (3 -> 2)");
        check(near(fill_of(rep, c, s.market, STEEL), 8.0f), "C1 ... drawing one tick of steel (8)",
              fill_of(rep, c, s.market, STEEL), 8.0);
        const auto flows = clear_markets(s.w, reg, rep);
        const float resolved = s.w.markets.at(s.market).price[ri(STEEL)];
        check(!near(resolved, under), "C1 the steel price resolved away from the posted price", resolved, under);
        check(near(spent(flows, c), 8.0f * under), "C1 billed fill x POSTED", spent(flows, c), 8.0 * under);
        const rows r = buyer_rows(s.w, c, STEEL);
        check(r.n == 1 && near(r.px, under), "C1 one exchange row at the posted price", r.px, under);
    }

    // C2: over — the steel is not on offer; the build pauses, nothing drawn or billed.
    {
        uint16_t sid = 0;
        recipe_registry reg = make_registry(k_shipped_reservation, sid);
        scene s = make_scene(k_iron_base, over, 1000.0f);
        const entity_id b = site(s);
        const entity_id c = add_corp(s, b, 0.0f);
        const float bal0 = s.w.corporations.at(c).balance;
        economy_report rep = run_economy_step(s.w, reg);
        check(s.w.buildings.at(b).ticks_remaining == 3 && near(s.w.buildings.at(b).construction_progress, 0.0f),
              "C2 over the ceiling the build PAUSES (no progress)");
        check(near(fill_of(rep, c, s.market, STEEL), 0.0f), "C2 no steel drawn", fill_of(rep, c, s.market, STEEL), 0.0);
        check(near(s.w.markets.at(s.market).inventory[ri(STEEL)], 1000.0f), "C2 the steel shelf is untouched");
        check(near(s.w.corporations.at(c).balance, bal0), "C2 a paused build spends nothing, cash cost included",
              s.w.corporations.at(c).balance, bal0);
        const auto flows = clear_markets(s.w, reg, rep);
        check(near(spent(flows, c), 0.0f) && buyer_rows(s.w, c, STEEL).n == 0,
              "C2 nothing billed, no exchange row");
    }

    // C3: construction capacity STRETCHES past a thin yard rather than pausing
    // (BL-709) — and is billed only for what the shelf actually held, never for
    // capacity that was not there. Need 0.5 a tick, shelf 0.1.
    {
        uint16_t sid = 0;
        recipe_registry reg = make_registry(k_shipped_reservation, sid);
        construction_params cp = reg.construction();
        cp.capacity_per_build_tick = 0.5f;
        reg.set_construction(cp);
        scene s = make_scene(k_iron_base, under, 1000.0f);
        const std::size_t cap = ri(resource_type::construction_capacity);
        s.w.markets.at(s.market).base_price[cap] = 4.0f;
        s.w.markets.at(s.market).price[cap]      = 4.0f;
        s.w.markets.at(s.market).inventory[cap]  = 0.1f;
        const entity_id b = site(s);
        const entity_id c = add_corp(s, b, 0.0f);
        economy_report rep = run_economy_step(s.w, reg);
        check(s.w.buildings.at(b).construction_progress > 0.0f || s.w.buildings.at(b).ticks_remaining < 3,
              "C3 a thin yard stretches the build, it does not pause it");
        check(near(fill_of(rep, c, s.market, resource_type::construction_capacity), 0.1f),
              "C3 capacity drawn is what the shelf held (0.1), not the 0.5 the tick wanted",
              fill_of(rep, c, s.market, resource_type::construction_capacity), 0.1);
        check(near(s.w.markets.at(s.market).inventory[cap], 0.0f), "C3 the capacity shelf is emptied, never negative");
    }
}

void s_shared_shelf()
{
    std::printf("\n--- S  draws that meet on one shelf ---\n");
    const float under = k_iron_base * 1.5f;

    // S1: one corp, one good, two draws — the processor's input and the same
    // building's upkeep. Processing takes the pool's 4 iron and buys 4; upkeep
    // (1 iron) then finds the pool empty and buys 1. One fill of 5, billed once.
    {
        uint16_t sid = 0;
        recipe_registry reg = make_registry(k_shipped_reservation, sid);
        building_upkeep_params up;
        up.supply_decay_permille = 50;
        up.supply_recovery_permille = 100;
        up.goods[static_cast<std::size_t>(building_type::processing_facility)]
                [static_cast<std::size_t>(era_band::any)][ri(IRON)] = 1.0f;
        reg.set_building_upkeep(up);
        reg.set_era(era_band::any);

        scene s = make_scene(under, k_steel_base, 1000.0f);
        const entity_id b = add_processor(s, sid, s.tile);
        const entity_id c = add_corp(s, b, 4.0f);
        economy_report rep = run_economy_step(s.w, reg);
        check(near(fill_of(rep, c, s.market, IRON), 5.0f), "S1 the processor's 4 and the upkeep's 1 make one fill of 5",
              fill_of(rep, c, s.market, IRON), 5.0);
        check(s.w.buildings.at(b).supply_factor_permille == 1000, "S1 the upkeep draw was met");
        const auto flows = clear_markets(s.w, reg, rep);
        check(near(spent(flows, c), 5.0f * under), "S1 billed 5 x POSTED, once", spent(flows, c), 5.0 * under);
        const rows r = buyer_rows(s.w, c, IRON);
        check(r.n == 1 && near(r.qty, 5.0f) && near(r.px, under), "S1 one exchange row of 5 at the posted price",
              r.qty, 5.0);
        check(near(s.w.markets.at(s.market).inventory[ri(IRON)], 995.0f), "S1 the shelf gave up exactly 5",
              s.w.markets.at(s.market).inventory[ri(IRON)], 995.0);
    }

    // S2: two corps, one good, one shelf — each billed its own fill.
    {
        uint16_t sid = 0;
        recipe_registry reg = make_registry(k_shipped_reservation, sid);
        scene s = make_scene(under, k_steel_base, 1000.0f);
        const entity_id b1 = add_processor(s, sid, s.tile);
        const entity_id c1 = add_corp(s, b1, 4.0f);   // buys 4
        const entity_id b2 = add_processor(s, sid, add_tile(s));
        const entity_id c2 = add_corp(s, b2, 2.0f);   // buys 6
        economy_report rep = run_economy_step(s.w, reg);
        check(near(fill_of(rep, c1, s.market, IRON), 4.0f) && near(fill_of(rep, c2, s.market, IRON), 6.0f),
              "S2 each corp buys its own shortfall (4 and 6)");
        const auto flows = clear_markets(s.w, reg, rep);
        check(near(spent(flows, c1), 4.0f * under) && near(spent(flows, c2), 6.0f * under),
              "S2 each is billed its own fill x POSTED");
        const rows r1 = buyer_rows(s.w, c1, IRON), r2 = buyer_rows(s.w, c2, IRON);
        check(r1.n == 1 && r2.n == 1 && near(r1.px, under) && near(r2.px, under),
              "S2 one row each, both at the posted price");
        check(near(s.w.markets.at(s.market).inventory[ri(IRON)], 990.0f), "S2 the shelf gave up 10 in all",
              s.w.markets.at(s.market).inventory[ri(IRON)], 990.0);
    }
}

// ---------------------------------------------------------------------------
// M — multi-tick: the price law and the bid, over ten ticks
// ---------------------------------------------------------------------------

float want_of(const economy_report& rep, entity_id corp, entity_id market, resource_type r)
{
    const auto it = rep.wants.find(std::make_pair(corp, market));
    return it == rep.wants.end() ? 0.0f : it->second[ri(r)];
}

/// A long build: 240 steel over 30 ticks (8 a tick), so ten ticks never finish it.
void lengthen_build(recipe_registry& reg)
{
    building_economics pr = reg.economics(building_type::processing_facility);
    pr.build_cost = 3000.0f;
    pr.build_duration_ticks = 30.0f;
    pr.resource_build_cost[ri(STEEL)] = 240.0f;
    reg.set_economics(building_type::processing_facility, pr);
}

entity_id add_site(scene& s, entity_id tile)
{
    const entity_id b = s.w.create_entity();
    building_component bc{};
    bc.tile = tile;
    bc.type = building_type::processing_facility;
    bc.ticks_remaining = 30;
    s.w.buildings[b] = bc;
    return b;
}

void m_at(float k)
{
    std::printf("   k = %.0f ticks of demand:\n", k);
    constexpr int ticks = 10;

    // M1: a processor on a shelf-only iron market (nobody lists iron), under.
    {
        uint16_t sid = 0;
        recipe_registry reg = make_registry(k_shipped_reservation, sid, k);
        scene s = make_scene(k_iron_base * 1.5f, k_steel_base, 1000.0f);
        const entity_id b = add_processor(s, sid, s.tile);
        const entity_id c = add_corp(s, b, 0.0f);
        int full = 0, over = 0;
        for (int t = 0; t < ticks; ++t)
        {
            if (s.w.markets.at(s.market).price[ri(IRON)] > k_iron_base * k_shipped_reservation)
                ++over;
            economy_report rep = run_economy_step(s.w, reg);
            if (near(fill_of(rep, c, s.market, IRON), 8.0f) && near(output_of(rep, b), 4.0f))
                ++full;
            clear_markets(s.w, reg, rep);
        }
        check(full == ticks, "M1 the processor buys a full run's iron off the shelf EVERY tick", full, ticks);
        check(over == 0, "M1 its own want never prices the full shelf over the ceiling", over, 0);
        check(near(s.w.markets.at(s.market).inventory[ri(IRON)], 1000.0f - 8.0f * ticks),
              "M1 the shelf gave up exactly ten runs of iron");
    }

    // M2: a construction site on a shelf-only steel market, under.
    {
        uint16_t sid = 0;
        recipe_registry reg = make_registry(k_shipped_reservation, sid, k);
        lengthen_build(reg);
        scene s = make_scene(k_iron_base, k_steel_base * 1.5f, 1000.0f);
        const entity_id b = add_site(s, s.tile);
        const entity_id c = add_corp(s, b, 0.0f);
        int drew = 0, over = 0;
        for (int t = 0; t < ticks; ++t)
        {
            if (s.w.markets.at(s.market).price[ri(STEEL)] > k_steel_base * k_shipped_reservation)
                ++over;
            economy_report rep = run_economy_step(s.w, reg);
            if (near(fill_of(rep, c, s.market, STEEL), 8.0f))
                ++drew;
            clear_markets(s.w, reg, rep);
        }
        check(drew == ticks, "M2 the site draws a tick of steel off the shelf EVERY tick", drew, ticks);
        check(over == 0, "M2 its own want never prices the full shelf over the ceiling", over, 0);
        check(s.w.buildings.at(b).ticks_remaining == 30 - ticks, "M2 ... and advances a whole tick each time",
              s.w.buildings.at(b).ticks_remaining, 30 - ticks);
    }

    // M3: a processor over the ceiling on an EMPTY iron shelf. On every tick
    // the posted price is over, it registers no want and draws nothing, and
    // the price the tick resolves is lower than the one it saw.
    {
        uint16_t sid = 0;
        recipe_registry reg = make_registry(k_shipped_reservation, sid, k);
        scene s = make_scene(k_iron_base * 2.5f, k_steel_base, 0.0f);
        const entity_id b = add_processor(s, sid, s.tile);
        const entity_id c = add_corp(s, b, 0.0f);
        int over = 0, bid_over = 0, rose_after_over = 0;
        for (int t = 0; t < ticks; ++t)
        {
            const float before = s.w.markets.at(s.market).price[ri(IRON)];
            const bool  is_over = before > k_iron_base * k_shipped_reservation;
            economy_report rep = run_economy_step(s.w, reg);
            if (is_over)
            {
                ++over;
                if (want_of(rep, c, s.market, IRON) > 0.0f || fill_of(rep, c, s.market, IRON) > 0.0f)
                    ++bid_over;
            }
            clear_markets(s.w, reg, rep);
            if (is_over && !(s.w.markets.at(s.market).price[ri(IRON)] < before))
                ++rose_after_over;
        }
        check(over > 0, "M3 the run met the ceiling (non-vacuous)", over, 1);
        check(bid_over == 0, "M3 over the ceiling the processor neither bids nor buys", bid_over, 0);
        check(rose_after_over == 0, "M3 ... and the price eases after every tick it sat over", rose_after_over, 0);
    }

    // M4: a construction site over the ceiling on an EMPTY steel shelf.
    {
        uint16_t sid = 0;
        recipe_registry reg = make_registry(k_shipped_reservation, sid, k);
        lengthen_build(reg);
        scene s = make_scene(k_iron_base, k_steel_base * 2.5f, 0.0f);
        const entity_id b = add_site(s, s.tile);
        const entity_id c = add_corp(s, b, 0.0f);
        int over = 0, bid_over = 0, rose_after_over = 0;
        for (int t = 0; t < ticks; ++t)
        {
            const float before = s.w.markets.at(s.market).price[ri(STEEL)];
            const bool  is_over = before > k_steel_base * k_shipped_reservation;
            economy_report rep = run_economy_step(s.w, reg);
            if (is_over)
            {
                ++over;
                if (want_of(rep, c, s.market, STEEL) > 0.0f || fill_of(rep, c, s.market, STEEL) > 0.0f)
                    ++bid_over;
            }
            clear_markets(s.w, reg, rep);
            if (is_over && !(s.w.markets.at(s.market).price[ri(STEEL)] < before))
                ++rose_after_over;
        }
        check(over > 0, "M4 the run met the ceiling (non-vacuous)", over, 1);
        check(bid_over == 0, "M4 over the ceiling the site neither bids nor draws", bid_over, 0);
        check(rose_after_over == 0, "M4 ... and the price eases after every tick it sat over", rose_after_over, 0);
        check(s.w.buildings.at(b).ticks_remaining == 30, "M4 an empty shelf never advances the build");
    }
}

void m_multi_tick()
{
    std::printf("\n--- M  ten ticks: shelf-only markets keep buying; over the ceiling nothing bids; at every k ---\n");
    for (const float k : k_shelf_tick_sweep)
        m_at(k);
}

} // namespace

int main()
{
    std::printf("fair_price_ceiling — BL-1172: one ceiling for every goods draw; a shelf draw pays the posted price\n");
    p_processor();
    c_construction();
    s_shared_shelf();
    m_multi_tick();
    std::printf("\n%s — %d failure(s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
