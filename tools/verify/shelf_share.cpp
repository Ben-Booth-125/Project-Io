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
//   S1 THE SHARE (pure, `ration_short_shelves`):
//      S1.1 three draws on a short shelf each get the same share of their want,
//           and the caps sum to the shelf; S1.2 an unshort shelf caps nobody;
//      S1.3 a draw that cannot run on its full want (another good short) is
//           dropped and the rest share; S1.4 an equal share under the run
//           threshold drops the LAST in visit order until the rest run.
//   S2 k = 0 IS INERT ON A REAL WORLD (multi-tick). Two copies of the seated
//      world, hauler view OFF in both; copy B's register is overwritten with
//      large values after every tick (its only cross-tick reader is dispatch's
//      projection of the price law). Bit-identical every tick at k = 0; at
//      k = 2 they part (non-vacuity: the k > 0 path does read it).
//   S3 THE CYCLE BREAKS (multi-tick). Every (market, good) on the seated world
//      whose shelf is stocked, priced over the fair-price ceiling, with
//      suppressed want: at k = 0 and k = 2 from the same world, the share that
//      is back under the ceiling WITH its buyers bidding (demand > 0) within 4
//      ticks.
//   S4 THE SHARE ON A REAL WORLD (multi-tick): every rationed draw in 12 play
//      ticks drew at most its cap; and on every short shelf with three or more
//      draws that drew their whole cap, each drew the same share of its want.
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
shelf_claimant mk(entity_id b, entity_id mkt, float need, float claim, float thr)
{
    shelf_claimant c;
    c.building = b; c.market = mkt; c.threshold = thr;
    c.need[0] = need; c.claim[0] = claim; c.gates[0] = true;
    return c;
}

void s1()
{
    world w;
    const entity_id M = 7;
    w.markets[M] = market_component{};
    w.markets[M].inventory[0] = 30.0f;

    std::vector<shelf_claimant> cl = {mk(1, M, 10, 10, 0.1f), mk(2, M, 20, 20, 0.1f), mk(3, M, 30, 30, 0.1f)};
    auto caps = ration_short_shelves(w, cl);
    const float f0 = caps[0][0] / 10.0f, f1 = caps[1][0] / 20.0f, f2 = caps[2][0] / 30.0f;
    std::printf("   S1.1: shelf 30, wants 10/20/30: caps %.3f %.3f %.3f (shares %.4f %.4f %.4f)\n",
                caps[0][0], caps[1][0], caps[2][0], f0, f1, f2);
    check(std::fabs(f0 - 0.5f) < 1e-6f && std::fabs(f1 - 0.5f) < 1e-6f && std::fabs(f2 - 0.5f) < 1e-6f
              && caps[0][0] + caps[1][0] + caps[2][0] <= 30.0f * (1.0f + 1e-6f),
          "S1.1 a short shelf with three admitted draws gives each the same share of its want; caps sum to the shelf");

    w.markets[M].inventory[0] = 60.0f;
    caps = ration_short_shelves(w, cl);
    bool all_inf = true;
    for (const auto& c : caps)
        for (const float v : c)
            if (v < std::numeric_limits<float>::infinity()) all_inf = false;
    check(all_inf, "S1.2 a shelf that meets every draw caps nobody (the draw is the pre-BL-1209 draw)");

    // S1.3: claimant 2 also needs good 1, which it holds none of and cannot buy.
    w.markets[M].inventory[0] = 30.0f;
    std::vector<shelf_claimant> cl3 = cl;
    cl3[1].need[1] = 5.0f; cl3[1].gates[1] = true;
    caps = ration_short_shelves(w, cl3);
    std::printf("   S1.3: draw 2 cannot run (good 1 short): caps %.3f %.3f %.3f\n", caps[0][0], caps[1][0], caps[2][0]);
    check(caps[1][0] == 0.0f && std::fabs(caps[0][0] / 10.0f - 0.75f) < 1e-6f && std::fabs(caps[2][0] / 30.0f - 0.75f) < 1e-6f,
          "S1.3 a draw that cannot run on its full want is dropped; the rest share the shelf equally (30/40 each)");

    // S1.4: run threshold 0.6, equal share 0.5 runs nobody: drop the last (30), the rest fit whole.
    std::vector<shelf_claimant> cl4 = {mk(1, M, 10, 10, 0.6f), mk(2, M, 20, 20, 0.6f), mk(3, M, 30, 30, 0.6f)};
    caps = ration_short_shelves(w, cl4);
    std::printf("   S1.4: threshold 0.6: caps %g %g %g\n", caps[0][0], caps[1][0], caps[2][0]);
    check(caps[2][0] == 0.0f && std::isinf(caps[0][0]) && std::isinf(caps[1][0]),
          "S1.4 an equal share under the run threshold drops the last in visit order until the rest run");
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

    lua_state lua;
    world_params p;
    p.seed = static_cast<std::uint32_t>(seed);
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, p, *start);
    world& w0 = start->w;
    for (int step = 0; step < k_campaign_settle_ticks; ++step)
        run_settle_tick(w0, start->reg, step, 0, true);
    seat_player_corporation(w0, static_cast<std::uint32_t>(seed), start->land.search.winner_score);
    std::printf("   shipped k = %g (left as shipped by this item)\n",
                static_cast<double>(start->reg.price_band().shelf_supply_ticks));

    // ---- S2: k = 0 is inert on a real world ------------------------------
    {
        const int ticks = 8;
        for (const float k : {0.0f, 2.0f})
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
                std::printf("   k 2: %s\n", same ? "no divergence in 8 ticks" : ("first differing tick " + std::to_string(first) + " (" + where + ")").c_str());
                check(!same, "S2.2 k = 2: the same scramble moves the world (the k > 0 path reads the register; S2.1 is not vacuous)");
            }
        }
    }

    // ---- S3: the cycle breaks -------------------------------------------
    {
        const float res = start->reg.price_band().reservation_mult;
        std::vector<std::pair<entity_id, std::size_t>> pairs;
        for (const auto& [mid, mc] : w0.markets)
            for (std::size_t r = 0; r < resource_count; ++r)
                if (mc.base_price[r] > 0.0f && mc.inventory[r] > 0.0f && mc.hauler_want[r] > 0.0f
                    && posted_price(mc, r) > res * mc.base_price[r])
                    pairs.emplace_back(mid, r);
        std::sort(pairs.begin(), pairs.end());
        std::printf("   S3: %zu stocked (market, good) pairs over the ceiling with suppressed want at the seat\n", pairs.size());
        check(!pairs.empty(), "S3.0 the seated world carries the cycle (vacuity)");
        int resumed[2] = {0, 0};
        int idx = 0;
        for (const float k : {0.0f, 2.0f})
        {
            recipe_registry reg = with_k(start->reg, k);
            world a = w0;
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
              "S3.1 k = 2: most stocked over-ceiling shelves price down and their buyers resume within 4 ticks, more than at k = 0");
    }

    // ---- S4: the share on a real world ----------------------------------
    {
        world a = w0;
        long rows = 0, over_cap = 0, groups3 = 0, groups3_equal = 0;
        double worst = 0.0;
        for (int t = 1; t <= 12; ++t)
        {
            const settle_tick_result res = play_tick(a, start->reg, t);
            std::map<std::tuple<char, entity_id, std::uint16_t>, std::vector<const shelf_ration_row*>> g;
            for (const shelf_ration_row& row : res.report.shelf_rations)
            {
                ++rows;
                if (row.drawn > row.cap * (1.0f + 1e-6f)) ++over_cap;
                g[{row.phase, row.market, row.r}].push_back(&row);
            }
            for (const auto& [key, v] : g)
            {
                std::vector<double> shares;
                for (const shelf_ration_row* row : v)
                    if (row->claim > 0.0f && row->drawn >= row->cap * (1.0f - 1e-6f) && row->cap > 0.0f)
                        shares.push_back(static_cast<double>(row->drawn) / row->claim);
                if (shares.size() < 3) continue;
                ++groups3;
                const auto [lo, hi] = std::minmax_element(shares.begin(), shares.end());
                const double spread = (*hi - *lo) / std::max(*hi, 1e-30);
                worst = std::max(worst, spread);
                if (spread < 1e-5) ++groups3_equal;
            }
        }
        std::printf("   S4: 12 ticks: %ld rationed draws; %ld short shelves with 3+ draws filled to their cap, %ld equal (worst relative spread %.2e)\n",
                    rows, groups3, groups3_equal, worst);
        check(rows > 0 && over_cap == 0, "S4.1 every rationed draw on the real world drew at most its pro-rata cap");
        check(groups3 > 0 && groups3_equal == groups3,
              "S4.2 every short shelf with three or more draws filled each the same share of its want");
    }

    std::printf("\n=== shelf_share: %d failure(s) ===\n", g_fail);
    return g_fail ? 1 : 0;
}
