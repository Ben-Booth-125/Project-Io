// ---------------------------------------------------------------------------
// shelf_share — BL-1209 (shelf sees silenced want)
// ---------------------------------------------------------------------------
// THE RULINGS (Ben, 2026-10-07; MARKETS.md § Price resolution):
//   (A) "The shelf's share reads the want the ceiling silenced": the shelf's
//       share of supply is min(inventory, k x (demand + suppressed want)), the
//       suppressed want being BL-1203's register (`market_component::
//       hauler_want`). The want never bids; it is read only through the cap.
//   (B) "A short shelf is shared pro-rata": when a shelf cannot meet every draw
//       admitted against it in a tick, each draw receives the same share of its
//       need, not first-come by building id.
//
// WHAT THIS PROVES
//   S0 THE PRICE LAW (pure, `pricing_supply` — the one reader of the register in
//      the price law, clearing and dispatch alike):
//      S0.1 k = 0: supply is the listings, whatever the register and the shelf
//           hold (the early return; the register is never read).
//      S0.2 k > 0, EMPTY shelf: supply and target are exactly what they are
//           with the register zeroed — the want never bids.
//      S0.3 k > 0, stocked shelf, demand 0, want W: supply = listings +
//           min(shelf, k W), and the target falls under the ceiling where the
//           register zeroed leaves it at base or the ceiling.
//   S1 THE SHARE (pure: `plan_short_shelves`, driven through a model of the
//      processor phase — first turn beyond the later floors, then the top-up):
//      S1.1 three draws on a short shelf reserve, and run, the same share of
//           their want; S1.2 an unshort shelf contends nothing;
//      S1.3 an EMPTY co-input shelf marks a draw hopeless up front and the
//           other draw takes the stock (review finding 1);
//      S1.4 an equal share too small to run anybody passes down the order;
//      S1.5 a draw fed by its own corp MID-PHASE releases its floor and the
//           earlier draw tops up — stranded without the top-up (finding 2);
//      S1.6 a PARTIAL run releases the floor it cannot use — stranded without
//           the top-up (finding 3).
//   S2 k = 0 IS INERT ON A REAL WORLD (multi-tick). Two copies of the seated
//      world, hauler view OFF in both; copy B's register is overwritten with
//      large values after every tick (its only cross-tick reader is dispatch's
//      projection of the price law). Bit-identical every tick at k = 0; at
//      k = 1 they part (non-vacuity: the k > 0 path does read it).
//   S3 THE CYCLE BREAKS (multi-tick). Every (market, good) on the seated world
//      whose shelf is stocked, priced over the fair-price ceiling, with
//      suppressed want: at k = 0 and the ruled k = 1 from the same world, the share that
//      is back under the ceiling WITH its buyers bidding (demand > 0) within 4
//      ticks.
//   S4 THE SHARE ON A REAL WORLD (multi-tick, 12 play ticks, shipped k):
//      S4.1 every contended shelf with 3+ hopeful draws reserves each the same
//      share; S4.2 THE INVARIANT, per phase (the phase's own audit, run on the
//      real shelves after its top-up): no shelf ends a phase holding stock while
//      an admitted draw left short could have used it; S4.3 the top-up did work.
//
// Usage (repo root): build_gen/verify/shelf_share.exe [--seed 0]
// Build: bash tools/verify/build_lua_harness.sh shelf_share
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace {

int g_fail = 0;
void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

constexpr int k_econ_tick_days = 90;

settle_tick_result play_tick(world& w, const recipe_registry& reg, int k)
{
    const int day = k * k_econ_tick_days;
    advance_orbits(w, static_cast<double>(k_econ_tick_days));
    advance_surveys(w, k_econ_tick_days);
    w.current_day_tick = day;
    return run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), day, /*spectating=*/false);
}

bool state_equal(const world& a, const world& b, std::string& where)
{
    for (const auto& [mid, ma] : a.markets)
    {
        const auto it = b.markets.find(mid);
        if (it == b.markets.end()) { where = "market set"; return false; }
        const market_component& mb = it->second;
        if (std::memcmp(ma.price.data(), mb.price.data(), sizeof(ma.price)) != 0) { where = "price"; return false; }
        if (std::memcmp(ma.demand.data(), mb.demand.data(), sizeof(ma.demand)) != 0) { where = "demand"; return false; }
        if (std::memcmp(ma.supply.data(), mb.supply.data(), sizeof(ma.supply)) != 0) { where = "supply"; return false; }
        if (std::memcmp(ma.inventory.data(), mb.inventory.data(), sizeof(ma.inventory)) != 0) { where = "inventory"; return false; }
    }
    for (const auto& [cid, ca] : a.corporations)
    {
        const auto it = b.corporations.find(cid);
        if (it == b.corporations.end()) { where = "corp set"; return false; }
        if (std::memcmp(&ca.balance, &it->second.balance, sizeof(ca.balance)) != 0) { where = "balance"; return false; }
    }
    return true;
}

void scramble_register(world& w)
{
    for (auto& [mid, mc] : w.markets)
    {
        (void)mid;
        for (std::size_t r = 0; r < resource_count; ++r)
            mc.hauler_want[r] = 1.0e6f;
    }
}

recipe_registry with_k(const recipe_registry& base, float k)
{
    recipe_registry r = base;
    price_band_params pb = r.price_band();
    pb.shelf_supply_ticks = k;
    r.set_price_band(pb);
    return r;
}

bool same_float(float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; }

// ---- S0: the price law, pure --------------------------------------------
void s0()
{
    const float base = 10.0f, floor_m = 0.25f, ceil_m = 10.0f, res = 2.0f;
    const float listed[] = {0.0f, 3.0f, 50.0f};
    const float shelves[] = {0.0f, 5.0f, 400.0f};
    const float demands[] = {0.0f, 2.0f, 80.0f};
    const float wants[] = {0.0f, 1.0f, 120.0f};
    bool k0_ok = true, empty_ok = true;
    for (const float L : listed)
        for (const float I : shelves)
            for (const float D : demands)
                for (const float W : wants)
                {
                    market_component m;
                    m.supply[0] = L; m.inventory[0] = I; m.demand[0] = D; m.hauler_want[0] = W;
                    if (!same_float(pricing_supply(m, 0, 0.0f), L)) k0_ok = false;
                    for (const float k : {1.0f, 2.0f, 4.0f})
                    {
                        if (I != 0.0f) continue;
                        market_component z = m;
                        z.hauler_want[0] = 0.0f;
                        const float s1 = pricing_supply(m, 0, k), s0v = pricing_supply(z, 0, k);
                        if (!same_float(s1, s0v)
                            || !same_float(price_target(base, s1, D, floor_m, ceil_m),
                                           price_target(base, s0v, D, floor_m, ceil_m)))
                            empty_ok = false;
                    }
                }
    check(k0_ok, "S0.1 k = 0: the shelf's share is the listings whatever the register and shelf hold (81 cases)");
    check(empty_ok, "S0.2 k > 0, empty shelf: supply and target bit-identical with the register zeroed (the want never bids)");

    market_component m;
    m.supply[0] = 0.0f; m.inventory[0] = 400.0f; m.demand[0] = 0.0f; m.hauler_want[0] = 30.0f;
    market_component z = m; z.hauler_want[0] = 0.0f;
    const float k = 2.0f;
    const float s_on = pricing_supply(m, 0, k), s_off = pricing_supply(z, 0, k);
    const float t_on = price_target(base, s_on, 0.0f, floor_m, ceil_m);
    const float t_off = price_target(base, s_off, 0.0f, floor_m, ceil_m);
    std::printf("   S0.3: stocked 400, demand 0, want 30, k 2: supply %.1f (zeroed %.1f), target %.2f (zeroed %.2f), ceiling %.2f\n",
                s_on, s_off, t_on, t_off, res * base);
    check(same_float(s_on, 60.0f) && t_on < res * base && t_off >= base,
          "S0.3 k > 0, stocked shelf, silenced want: supply = min(shelf, k x want), the target falls under the ceiling");
    // The share never exceeds the shelf, and demand is not read differently.
    market_component s = m; s.inventory[0] = 10.0f;
    check(same_float(pricing_supply(s, 0, k), 10.0f), "S0.4 the share is capped by the shelf (min(inventory, k x (demand + want)))");
}

// ---- S1: the share, pure ---------------------------------------------------
// A two-good world (Y = good 0, X = good 1) on one market. `plan_short_shelves`
// is the production function; the PHASE below drives it the way the processor
// pass does (economy_system.cpp, run_processing + top_up_processing): each draw
// in visit order is offered the shelf beyond the later draws' floors, runs at
// min(1, coverage) if coverage reaches its threshold, takes pool first then
// shelf; then draws left short top up, in visit order, from what is left.
// `feeds` models a mid-phase own production: when draw i runs, it adds
// feeds_qty x run of good feeds_good to draw feeds_to's pool (a corp's own
// upstream plant), which the pre-pass could not see.
struct sim_draw
{
    shelf_claimant c;              // pre-pass view (own = pool as the phase opens)
    std::array<float, resource_count> pool{};  // the real pool
    int   feeds_to = -1;
    std::size_t feeds_good = 0;
    float feeds_qty = 0.0f;
    float run = 0.0f;
};

struct sim_result
{
    std::vector<float> run;
    float left_y = 0.0f, left_x = 0.0f;
    int stranded = 0;    // a draw short that could still run more off the shelves
    int topups = 0;
};

float sim_cov(const sim_draw& d, const std::array<float, resource_count>& offer)
{
    float cov = std::numeric_limits<float>::infinity();
    for (std::size_t r = 0; r < resource_count; ++r)
        if (d.c.need[r] > 0.0f)
            cov = std::min(cov, (std::max(0.0f, d.pool[r]) + offer[r]) / d.c.need[r]);
    return cov;
}

void sim_take(std::vector<sim_draw>& ds, std::size_t i, float delta, std::array<float, resource_count>& shelf)
{
    sim_draw& d = ds[i];
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const float need = d.c.need[r] * delta;
        if (!(need > 0.0f)) continue;
        const float fp = std::min(std::max(0.0f, d.pool[r]), need);
        d.pool[r] -= fp;
        const float fs = std::min(shelf[r], need - fp);
        shelf[r] -= fs;
    }
    d.run += delta;
    if (d.feeds_to >= 0)
        ds[static_cast<std::size_t>(d.feeds_to)].pool[d.feeds_good] += d.feeds_qty * delta;
}

sim_result sim_phase(world& w, entity_id M, std::vector<sim_draw> ds, bool top_up)
{
    std::vector<shelf_claimant> cl;
    for (const sim_draw& d : ds) cl.push_back(d.c);
    const shelf_ration_plan plan = plan_short_shelves(w, cl);
    std::array<float, resource_count> shelf = w.markets.at(M).inventory;
    for (std::size_t i = 0; i < ds.size(); ++i)
    {
        std::array<float, resource_count> offer{};
        for (std::size_t r = 0; r < resource_count; ++r)
            if (ds[i].c.claim[r] > 0.0f)
                offer[r] = std::max(0.0f, shelf[r] - (plan.any ? plan.reserved_after[i][r] : 0.0f));
        const float cov = sim_cov(ds[i], offer);
        if (cov >= ds[i].c.threshold && cov > 0.0f)
            sim_take(ds, i, std::min(1.0f, cov), shelf);
    }
    sim_result out;
    auto sweep = [&](bool dry) {
        int n = 0;
        for (std::size_t i = 0; i < ds.size(); ++i)
        {
            if (ds[i].run >= 1.0f) continue;
            std::array<float, resource_count> offer{};
            for (std::size_t r = 0; r < resource_count; ++r)
                if (ds[i].c.claim[r] > 0.0f) offer[r] = shelf[r];
            const float nr = std::min(1.0f, ds[i].run + sim_cov(ds[i], offer));
            if (!(nr >= ds[i].c.threshold) || !(nr - ds[i].run > 1e-5f)) continue;
            ++n;
            if (!dry) sim_take(ds, i, nr - ds[i].run, shelf);
        }
        return n;
    };
    if (top_up)
        for (int s = 0; s < 8; ++s) { const int n = sweep(false); out.topups += n; if (n == 0) break; }
    out.stranded = sweep(true);
    for (const sim_draw& d : ds) out.run.push_back(d.run);
    out.left_y = shelf[0];
    out.left_x = shelf[1];
    return out;
}

shelf_claimant mk(entity_id b, entity_id mkt, float need, float claim, float thr)
{
    shelf_claimant c;
    c.building = b; c.market = mkt; c.threshold = thr;
    c.need[0] = need; c.claim[0] = claim; c.gates[0] = true;
    return c;
}

sim_draw sd(entity_id b, entity_id M, float need_y, float pool_y, float thr)
{
    sim_draw d;
    d.c = mk(b, M, need_y, std::max(0.0f, need_y - pool_y), thr);
    d.c.own[0] = std::min(pool_y, need_y);
    d.pool[0] = pool_y;
    return d;
}

void s1()
{
    world w;
    const entity_id M = 7;
    w.markets[M] = market_component{};
    w.markets[M].inventory[0] = 30.0f;

    std::vector<shelf_claimant> cl = {mk(1, M, 10, 10, 0.1f), mk(2, M, 20, 20, 0.1f), mk(3, M, 30, 30, 0.1f)};
    shelf_ration_plan p = plan_short_shelves(w, cl);
    std::printf("   S1.1: shelf 30, wants 10/20/30: floors %.3f %.3f %.3f, reserved after %.3f %.3f %.3f\n",
                p.floor[0][0], p.floor[1][0], p.floor[2][0],
                p.reserved_after[0][0], p.reserved_after[1][0], p.reserved_after[2][0]);
    check(p.any && std::fabs(p.floor[0][0] - 5.0f) < 1e-5f && std::fabs(p.floor[1][0] - 10.0f) < 1e-5f
              && std::fabs(p.floor[2][0] - 15.0f) < 1e-5f && p.reserved_after[2][0] == 0.0f
              && std::fabs(p.reserved_after[0][0] - 25.0f) < 1e-4f,
          "S1.1 a short shelf with three admitted draws reserves each the same share of its want (floors 5/10/15)");
    {
        std::vector<sim_draw> ds = {sd(1, M, 10, 0, 0.1f), sd(2, M, 20, 0, 0.1f), sd(3, M, 30, 0, 0.1f)};
        const sim_result r = sim_phase(w, M, ds, true);
        check(std::fabs(r.run[0] - 0.5f) < 1e-5f && std::fabs(r.run[1] - 0.5f) < 1e-5f && std::fabs(r.run[2] - 0.5f) < 1e-5f,
              "S1.1b the three draws each run the same share (0.5)");
    }

    w.markets[M].inventory[0] = 60.0f;
    p = plan_short_shelves(w, cl);
    check(!p.any, "S1.2 a shelf that meets every draw contends nothing (the draw is the pre-BL-1209 draw)");

    // S1.3 (finding 1): Y = 10 and X = 0, both under the ceiling. Q needs X + Y,
    // P needs Y. Q cannot run at its best case (an empty co-input shelf): it is
    // hopeless up front, reserves nothing, and P takes the 10 Y.
    {
        w.markets[M].inventory[0] = 10.0f;
        w.markets[M].inventory[1] = 0.0f;
        sim_draw q = sd(1, M, 10, 0, 0.2f);
        q.c.need[1] = 5.0f; q.c.claim[1] = 5.0f; q.c.gates[1] = true;
        sim_draw pp = sd(2, M, 10, 0, 0.2f);
        std::vector<sim_draw> ds = {q, pp};
        std::vector<shelf_claimant> c2 = {q.c, pp.c};
        const shelf_ration_plan pl = plan_short_shelves(w, c2);
        const sim_result r = sim_phase(w, M, ds, true);
        std::printf("   S1.3: Q hopeless %d, P floor %.2f, runs Q %.2f P %.2f, Y left %.2f\n",
                    pl.hopeless[0], pl.floor[1][0], r.run[0], r.run[1], r.left_y);
        check(pl.hopeless[0] && !pl.hopeless[1] && r.run[1] == 1.0f && r.left_y == 0.0f && r.stranded == 0,
              "S1.3 an empty co-input shelf marks a draw hopeless up front; the other draw takes the stock (nothing stranded)");
    }

    // S1.4 (the equal share that runs nobody): threshold 0.6, shelf 30 against
    // 10/20/30 — every floor is a 0.5 share. The released floors accumulate down
    // the order until a draw can run; nothing is left stranded.
    {
        w.markets[M].inventory[0] = 30.0f;
        w.markets[M].inventory[1] = 0.0f;
        std::vector<sim_draw> ds = {sd(1, M, 10, 0, 0.6f), sd(2, M, 20, 0, 0.6f), sd(3, M, 30, 0, 0.6f)};
        const sim_result r = sim_phase(w, M, ds, true);
        std::printf("   S1.4: threshold 0.6: runs %.3f %.3f %.3f, Y left %.3f, stranded %d\n",
                    r.run[0], r.run[1], r.run[2], r.left_y, r.stranded);
        check(r.left_y < 1e-4f && r.stranded == 0 && (r.run[0] > 0.0f || r.run[1] > 0.0f || r.run[2] > 0.0f),
              "S1.4 an equal share too small to run anybody passes down the order until a draw runs; the shelf is not stranded");
    }

    // S1.5 (finding 2): mid-phase own production. A (first) and B share Y = 20,
    // each wanting 20 (floors 10). B's corp mine D, visited between them,
    // feeds B 20 Y — invisible to the pre-pass. B takes none of its floor; A,
    // already visited at 0.5, tops up to the whole shelf.
    {
        w.markets[M].inventory[0] = 20.0f;
        sim_draw a = sd(1, M, 20, 0, 0.2f);
        sim_draw d; d.c.building = 3; d.c.market = M; d.c.threshold = 0.0f; d.run = 0.0f;
        d.c.need[2] = 1.0f; d.pool[2] = 1.0f; d.c.gates[2] = true; // runs off its own pool
        d.feeds_to = 2; d.feeds_good = 0; d.feeds_qty = 20.0f;
        sim_draw b = sd(2, M, 20, 0, 0.2f);
        std::vector<sim_draw> ds = {a, d, b};
        const sim_result no_top = sim_phase(w, M, ds, false);
        const sim_result r = sim_phase(w, M, ds, true);
        std::printf("   S1.5: own production mid-phase: without top-up A %.2f B %.2f Y left %.2f stranded %d | with: A %.2f B %.2f Y left %.2f stranded %d\n",
                    no_top.run[0], no_top.run[2], no_top.left_y, no_top.stranded, r.run[0], r.run[2], r.left_y, r.stranded);
        check(no_top.stranded > 0 && r.stranded == 0 && r.run[0] == 1.0f && r.run[2] == 1.0f && r.left_y < 1e-4f,
              "S1.5 a draw fed by its own corp mid-phase releases its floor; the earlier draw tops up (stranded without the top-up, none with it)");
    }

    // S1.6 (finding 3): a partial run releases the rest of its floor. Three
    // draws want 20 Y each off a shelf of 20 (floors 6.67). C, visited LAST,
    // holds only 2 of the 10 X it needs (pool only, X not on the shelf): it runs
    // 0.2 and uses 4 of its floor. The 2.67 it cannot use stays on the shelf
    // while E and F, visited first, sit at a 1/3 run — until the top-up.
    {
        w.markets[M].inventory[0] = 20.0f;
        w.markets[M].inventory[1] = 0.0f;
        sim_draw c = sd(3, M, 20, 0, 0.2f);
        c.c.need[1] = 10.0f; c.c.own[1] = 2.0f; c.pool[1] = 2.0f; c.c.gates[1] = true;
        std::vector<sim_draw> ds = {sd(1, M, 20, 0, 0.2f), sd(2, M, 20, 0, 0.2f), c};
        const sim_result no_top = sim_phase(w, M, ds, false);
        const sim_result r = sim_phase(w, M, ds, true);
        std::printf("   S1.6: partial run: without top-up runs %.3f %.3f %.3f Y left %.3f stranded %d | with: %.3f %.3f %.3f Y left %.3f stranded %d\n",
                    no_top.run[0], no_top.run[1], no_top.run[2], no_top.left_y, no_top.stranded,
                    r.run[0], r.run[1], r.run[2], r.left_y, r.stranded);
        check(no_top.stranded > 0 && r.stranded == 0 && r.left_y < 1e-4f && std::fabs(r.run[2] - 0.2f) < 1e-5f,
              "S1.6 a partial run takes only what it can use; the rest of its floor reaches the other draws (shelf emptied, nothing stranded)");
    }
}

// ---- S5: the REAL processor pass on a small built world ------------------
// One market, an iron shelf of 8 under the ceiling. Corp A: a steel plant P_a
// (8 iron for a full run). Corp B: an ore works M_b (no input, 8 iron into B's
// pool) visited BEFORE its own steel plant P_b. The pre-pass sees both plants
// wanting 8 off a shelf of 8: contended, floors 4 each. P_a (first) is offered
// 8 - 4 = 4 and runs half. M_b then fills B's pool, so P_b runs whole from its
// pool and takes none of its floor — 4 iron is left on the shelf with P_a
// short. Only the top-up can give it to P_a: run_economy_step's real
// run_processing / top_up_processing must leave P_a at a full run, the shelf
// empty, the top-up counted and the independent audit at 0.
void s5()
{
    const std::size_t IRON = static_cast<std::size_t>(resource_type::iron_ore);
    const std::size_t STEEL = static_cast<std::size_t>(resource_type::steel);
    recipe_registry reg;
    reg.set_thresholds(1.0f, 0.2f);
    building_economics pr;
    pr.base_rate = 8.0f; pr.maintenance = 10.0f; pr.base_wage = 12.0f;
    pr.build_cost = 300.0f; pr.build_duration_ticks = 3.0f;
    reg.set_economics(building_type::processing_facility, pr);
    recipe steel; steel.name = "steel"; steel.inputs[IRON] = 2.0f; steel.outputs[STEEL] = 1.0f;
    const uint16_t steel_id = reg.add_recipe(steel);
    recipe ore; ore.name = "ore"; ore.outputs[IRON] = 1.0f;
    const uint16_t ore_id = reg.add_recipe(ore);
    price_band_params pb;
    pb.floor_mult = 0.25f; pb.ceil_mult = 10.0f; pb.reservation_mult = 2.0f; pb.shelf_supply_ticks = 1.0f;
    reg.set_price_band(pb);

    world w;
    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};
    auto tile = [&]() {
        const entity_id t = w.create_entity();
        tile_component tc{};
        tc.body = body; tc.substrate = terrain_substrate::sedimentary;
        tc.cover = terrain_cover::grass; tc.cover_density = 150;
        w.tiles[t] = tc;
        return t;
    };
    const entity_id t0 = tile();
    const entity_id M = w.create_entity();
    {
        market_component mc;
        mc.body = body; mc.centre_tile = t0;
        mc.base_price[IRON] = 2.5f; mc.price[IRON] = 2.5f * 1.5f; mc.inventory[IRON] = 8.0f;
        mc.base_price[STEEL] = 8.0f; mc.price[STEEL] = 8.0f;
        w.markets[M] = mc;
    }
    auto building = [&](uint16_t recipe_id, float wf) {
        const entity_id b = w.create_entity();
        building_component bc{};
        bc.tile = tile(); bc.type = building_type::processing_facility;
        bc.workforce_assigned = wf; bc.recipe = recipe_id;
        w.buildings[b] = bc;
        return b;
    };
    auto corp = [&](std::vector<entity_id> assets) {
        const entity_id c = w.create_entity();
        corporation_component cc;
        cc.name = "Test Co"; cc.is_player = true;
        cc.starting_capital = 10000.0f; cc.balance = 10000.0f;
        cc.assets = assets;
        w.corporations[c] = cc;
        return c;
    };
    const entity_id Pa = building(steel_id, 0.5f);
    const entity_id Mb = building(ore_id, 1.0f);
    const entity_id Pb = building(steel_id, 0.5f);
    corp({Pa});
    corp({Mb, Pb});

    const economy_report rep = run_economy_step(w, reg);
    float run_a = -1.0f, run_b = -1.0f, out_a = -1.0f;
    for (const building_report& br : rep.buildings)
    {
        if (br.building == Pa) { run_a = br.run; out_a = br.output_quantity; }
        if (br.building == Pb) run_b = br.run;
    }
    const shelf_phase_audit& au = rep.shelf_audit[1];
    std::printf("   S5: built world: P_a run %.3f (output %.2f steel), P_b run %.3f, iron left %.3f; contended draws %d, top-ups %d, stranded %d / open %d\n",
                run_a, out_a, run_b, w.markets.at(M).inventory[IRON], au.claimants, au.topups, au.stranded, au.stranded_open);
    check(au.claimants == 2 && au.topups >= 1, "S5.1 the real processor pass: the shelf is contended and the top-up fires");
    check(std::fabs(run_a - 1.0f) < 1e-5f && std::fabs(out_a - 4.0f) < 1e-4f && std::fabs(run_b - 1.0f) < 1e-5f
              && w.markets.at(M).inventory[IRON] < 1e-4f,
          "S5.2 the floor P_b did not need reaches P_a: both run whole, the shelf is emptied");
    check(au.stranded == 0 && au.stranded_open == 0, "S5.3 the independent audit reads 0 stranded on the real pass");
}

} // namespace

int main(int argc, char** argv)
{
    int seed = 0;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--seed" && i + 1 < argc) seed = std::atoi(argv[++i]);
        else { std::fprintf(stderr, "unknown arg %s\n", a.c_str()); return 2; }
    }
    std::printf("shelf_share — BL-1209 (shelf sees silenced want), seed %d\n", seed);
    s0();
    s1();
    s5();

    lua_state lua;
    world_params p;
    p.seed = static_cast<std::uint32_t>(seed);
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, p, *start);
    world& w0 = start->w;
    const world w_pre = w0; // before the settle: S3 settles its own copy at k = 0
    for (int step = 0; step < k_campaign_settle_ticks; ++step)
        run_settle_tick(w0, start->reg, step, 0, true);
    seat_player_corporation(w0, static_cast<std::uint32_t>(seed), start->land.search.winner_score);
    std::printf("   shipped k = %g (Ben, 2026-10-07: k = 1)\n",
                static_cast<double>(start->reg.price_band().shelf_supply_ticks));

    // ---- S2: k = 0 is inert on a real world ------------------------------
    {
        const int ticks = 8;
        for (const float k : {0.0f, 1.0f})
        {
            recipe_registry reg = with_k(start->reg, k);
            reg.set_hauler_room(false);
            world a = w0, b = w0;
            scramble_register(b);
            bool same = true;
            int first = -1;
            std::string where;
            for (int t = 1; t <= ticks && same; ++t)
            {
                play_tick(a, reg, t);
                play_tick(b, reg, t);
                if (!state_equal(a, b, where)) { same = false; first = t; }
                scramble_register(b);
            }
            if (k == 0.0f)
            {
                if (!same) std::printf("   k 0: first differing tick %d (%s)\n", first, where.c_str());
                check(same, "S2.1 k = 0: register scrambled every tick vs kept, prices/demand/supply/shelves/balances bit-identical for 8 ticks");
            }
            else
            {
                std::printf("   k 1: %s\n", same ? "no divergence in 8 ticks" : ("first differing tick " + std::to_string(first) + " (" + where + ")").c_str());
                check(!same, "S2.2 k = 1: the same scramble moves the world (the k > 0 path reads the register; S2.1 is not vacuous)");
            }
        }
    }

    // ---- S3: the cycle breaks -------------------------------------------
    {
        // The cycle lives in a world settled WITHOUT the rule (k = 0): at the
        // shipped k the settle already breaks most of it. Settle and seat a copy
        // at k = 0, then play it on at k = 0 and at k = 1.
        world wk0 = w_pre;
        {
            const recipe_registry reg0 = with_k(start->reg, 0.0f);
            for (int step = 0; step < k_campaign_settle_ticks; ++step)
                run_settle_tick(wk0, reg0, step, 0, true);
            seat_player_corporation(wk0, static_cast<std::uint32_t>(seed), start->land.search.winner_score);
        }
        const float res = start->reg.price_band().reservation_mult;
        std::vector<std::pair<entity_id, std::size_t>> pairs;
        for (const auto& [mid, mc] : wk0.markets)
            for (std::size_t r = 0; r < resource_count; ++r)
                if (mc.base_price[r] > 0.0f && mc.inventory[r] > 0.0f && mc.hauler_want[r] > 0.0f
                    && posted_price(mc, r) > res * mc.base_price[r])
                    pairs.emplace_back(mid, r);
        std::sort(pairs.begin(), pairs.end());
        std::printf("   S3: %zu stocked (market, good) pairs over the ceiling with suppressed want at the seat (settled at k = 0)\n", pairs.size());
        check(!pairs.empty(), "S3.0 the k = 0 seated world carries the cycle (vacuity)");
        int resumed[2] = {0, 0};
        int idx = 0;
        for (const float k : {0.0f, 1.0f})
        {
            recipe_registry reg = with_k(start->reg, k);
            world a = wk0;
            std::vector<int> when(pairs.size(), -1);
            for (int t = 1; t <= 4; ++t)
            {
                play_tick(a, reg, t);
                for (std::size_t i = 0; i < pairs.size(); ++i)
                {
                    if (when[i] >= 0) continue;
                    const market_component& mc = a.markets.at(pairs[i].first);
                    const std::size_t r = pairs[i].second;
                    if (posted_price(mc, r) <= res * mc.base_price[r] && mc.demand[r] > 0.0f)
                        when[i] = t;
                }
            }
            int by_tick[5] = {0, 0, 0, 0, 0};
            for (const int t : when) if (t > 0) { ++resumed[idx]; ++by_tick[t]; }
            std::printf("   S3 k %g: under the ceiling with buyers bidding within 4 ticks: %d of %zu (by tick 1/2/3/4: %d/%d/%d/%d)\n",
                        static_cast<double>(k), resumed[idx], pairs.size(), by_tick[1], by_tick[2], by_tick[3], by_tick[4]);
            ++idx;
        }
        check(resumed[1] > resumed[0] && resumed[1] * 2 > static_cast<int>(pairs.size()),
              "S3.1 k = 1 (the ruled k): most stocked over-ceiling shelves price down and their buyers resume within 4 ticks, more than at k = 0");
    }

    // ---- S4: the share on a real world ----------------------------------
    {
        world a = w0;
        long rows[2] = {0, 0}, groups3 = 0, groups3_equal = 0;
        long claimants[2] = {0, 0}, topups[2] = {0, 0}, stranded[2] = {0, 0}, open_str[2] = {0, 0}, hopeless = 0;
        double open_units = 0.0;
        double worst = 0.0;
        for (int t = 1; t <= 12; ++t)
        {
            const settle_tick_result res = play_tick(a, start->reg, t);
            for (int ph = 0; ph < 2; ++ph)
            {
                claimants[ph] += res.report.shelf_audit[ph].claimants;
                topups[ph]    += res.report.shelf_audit[ph].topups;
                stranded[ph]  += res.report.shelf_audit[ph].stranded;
                open_str[ph]  += res.report.shelf_audit[ph].stranded_open;
                open_units    += res.report.shelf_audit[ph].stranded_units;
            }
            std::map<std::tuple<char, entity_id, std::uint16_t>, std::vector<const shelf_ration_row*>> g;
            for (const shelf_ration_row& row : res.report.shelf_rations)
            {
                ++rows[row.phase == 'c' ? 0 : 1];
                if (row.hopeless) { ++hopeless; continue; }
                g[{row.phase, row.market, row.r}].push_back(&row);
            }
            for (const auto& [key, v] : g)
            {
                std::vector<double> shares;
                for (const shelf_ration_row* row : v)
                    if (row->claim > 0.0f)
                        shares.push_back(static_cast<double>(row->floor) / row->claim);
                if (shares.size() < 3) continue;
                ++groups3;
                const auto [lo, hi] = std::minmax_element(shares.begin(), shares.end());
                const double spread = (*hi - *lo) / std::max(*hi, 1e-30);
                worst = std::max(worst, spread);
                if (spread < 1e-5) ++groups3_equal;
            }
        }
        std::printf("   S4: 12 ticks at the shipped k: contended draws construction %ld / processing %ld (rows %ld / %ld, hopeless rows %ld); top-ups %ld / %ld; stranded after the phase %ld / %ld\n",
                    claimants[0], claimants[1], rows[0], rows[1], hopeless, topups[0], topups[1], stranded[0], stranded[1]);
        std::printf("   S4: independent audit (real pools and shelves, every processor): stranded on a contended shelf %ld, on an uncontended shelf %ld (%.1f units, both phases)\n",
                    stranded[1], open_str[1], open_units);
        std::printf("   S4: %ld contended shelves with 3+ hopeful draws, %ld reserve the same share (worst relative spread %.2e)\n",
                    groups3, groups3_equal, worst);
        check(groups3 > 0 && groups3_equal == groups3,
              "S4.1 every contended shelf with three or more draws reserves each the same share of its want");
        check(claimants[0] + claimants[1] > 0 && stranded[0] == 0 && stranded[1] == 0,
              "S4.2 THE INVARIANT, both phases, 12 ticks (processing audited independently of the top-up): no CONTENDED shelf ends a phase holding stock while an admitted draw left short could have used it");
        check(topups[0] + topups[1] > 0, "S4.3 the top-up turn did work on the real world (S4.2 is not vacuous)");
    }

    std::printf("\n=== shelf_share: %d failure(s) ===\n", g_fail);
    return g_fail ? 1 : 0;
}
