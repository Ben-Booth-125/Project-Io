// ---------------------------------------------------------------------------
// hauler_room — BL-1203 (water reaches dry markets): the hauler's room
// ---------------------------------------------------------------------------
// THE RULING (Ben, 2026-10-05; SUPPLY.md § Dispatch trigger, "What a hauler
// sees as unmet demand"). The dispatcher's room at a destination reads
// (1) the households' bid re-read at the cargo's LANDED price and (2) a
// HAULER-ONLY want register — processor input and construction material want
// that went unbid because the posted price stood over the buyer's fair-price
// ceiling — counted only when the landed cost is under that ceiling. The price
// law never reads the register (BL-1172 unchanged).
//
// WHAT THIS PROVES, on a real seated world (the app's start world, its 12-tick
// settle, the seat; then play ticks through run_settle_tick):
//
//   H1 THE PRICE LAW NEVER READS THE REGISTER. Two copies of the same world, the
//      dispatcher's hauler view switched OFF in both (`set_hauler_room(false)`),
//      tick in lockstep; copy B has `household_weight` and `hauler_want` zeroed
//      on every market after every tick (the clear writes them; the next tick's
//      dispatch is their only reader). Every market's price, demand, supply and
//      inventory, and every corporation's balance, agree BIT FOR BIT on every
//      tick. With the switch off nothing reads the fields, so this is the
//      register's content not reaching any price.
//   H2 THE CEILING GATE, read at the function. For every (market, good) whose
//      `hauler_want` is positive on a sampled tick: dispatch_absorbable with a
//      landed cost just ABOVE reservation_mult x base equals the same call with
//      the register zeroed; at a landed cost AT the ceiling it is never less, and
//      larger on at least one (where listings already exceed the sized target
//      both read 0). Vacuity guard: at least one such pair is seen.
//   H3 A SHORT MARKET RECEIVES A SHIPMENT. Over the play ticks, with the hauler
//      view ON (the shipped game) against OFF, from the same world: the units of
//      a good dispatched INTO a market whose `hauler_want` for that good was
//      positive at the decision, landing (source price + haul per unit) at or
//      under that market's ceiling. ON must move more than OFF, and at least
//      one unit (a multi-tick row: the world under ON diverges as it ships).
//   H4 DETERMINISM. The ON run repeated from the same copy gives the same
//      shipped units and the same final state hash.
//
// Usage (repo root): build_gen/verify/hauler_room.exe [--seed 0] [--ticks 20]
// Build: bash tools/verify/build_lua_harness.sh hauler_room
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

constexpr int k_econ_tick_days = 90;

void play_tick(world& w, const recipe_registry& reg, int k)
{
    const int day = k * k_econ_tick_days;
    advance_orbits(w, static_cast<double>(k_econ_tick_days));
    advance_surveys(w, k_econ_tick_days);
    w.current_day_tick = day;
    run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), day, /*spectating=*/false);
}

/// The price law's outputs and inputs, and the money: what H1 compares.
bool price_state_equal(const world& a, const world& b, std::string& where)
{
    if (a.markets.size() != b.markets.size()) { where = "market count"; return false; }
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

void zero_register(world& w)
{
    for (auto& [mid, mc] : w.markets)
    {
        (void)mid;
        mc.household_weight.fill(0.0f);
        mc.hauler_want.fill(0.0f);
    }
}

struct ship_tally
{
    double units = 0.0;
    long   convoys = 0;
};

/// H3: run `ticks` play ticks from `w`, counting units dispatched into a
/// (market, good) whose hauler_want was positive at the decision (the register
/// the dispatch read: written by the previous clear), landing at or under that
/// market's ceiling.
ship_tally run_and_count(world& w, const recipe_registry& reg, int ticks)
{
    ship_tally t;
    const float res = reg.price_band().reservation_mult;
    std::uint32_t max_id = 0;
    for (const convoy_component& c : w.convoys) max_id = std::max(max_id, c.id);
    for (int k = 1; k <= ticks; ++k)
    {
        // The register and the prices dispatch reads this tick: last clear's.
        std::map<entity_id, std::array<float, resource_count>> want, price;
        for (const auto& [mid, mc] : w.markets) { want[mid] = mc.hauler_want; price[mid] = mc.price; }
        play_tick(w, reg, k);
        for (const convoy_component& c : w.convoys)
        {
            if (c.id <= max_id) continue;
            const std::size_t r = static_cast<std::size_t>(c.cargo_resource);
            const auto wi = want.find(c.dest_market);
            if (wi == want.end() || !(wi->second[r] > 0.0f) || !(c.cargo_qty > 0.0f)) continue;
            const market_component& dm = w.markets.at(c.dest_market);
            const auto si = price.find(c.source_market);
            const float p_src = (si != price.end() && si->second[r] > 0.0f)
                                    ? si->second[r]
                                    : (w.markets.count(c.source_market) ? w.markets.at(c.source_market).base_price[r] : 0.0f);
            const float landed = p_src + c.cost_paid / c.cargo_qty;
            if (landed <= res * dm.base_price[r])
            {
                t.units += c.cargo_qty;
                ++t.convoys;
            }
        }
        for (const convoy_component& c : w.convoys) max_id = std::max(max_id, c.id);
    }
    return t;
}

} // namespace

int main(int argc, char** argv)
{
    int seed = 0, ticks = 20;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--seed" && i + 1 < argc) seed = std::atoi(argv[++i]);
        else if (a == "--ticks" && i + 1 < argc) ticks = std::atoi(argv[++i]);
        else { std::fprintf(stderr, "unknown arg %s\n", a.c_str()); return 2; }
    }

    lua_state lua;
    world_params p;
    p.seed = static_cast<std::uint32_t>(seed);
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, p, *start);
    world& w0 = start->w;
    for (int step = 0; step < k_campaign_settle_ticks; ++step)
        run_settle_tick(w0, start->reg, step, 0, true);
    seat_player_corporation(w0, static_cast<std::uint32_t>(seed), start->land.search.winner_score);
    std::printf("hauler_room: seed %d, %d play ticks from the seat\n", seed, ticks);

    recipe_registry reg_on  = start->reg; // shipped: hauler view on
    recipe_registry reg_off = start->reg;
    reg_off.set_hauler_room(false);
    check(reg_on.hauler_room(), "H0 the shipped registry has the hauler view ON");

    // ---- H1: the price law never reads the register ----------------------
    {
        world a = w0, b = w0;
        zero_register(b);
        bool same = true;
        int first = -1;
        std::string where;
        long nonzero_seen = 0;
        for (int k = 1; k <= ticks && same; ++k)
        {
            play_tick(a, reg_off, k);
            play_tick(b, reg_off, k);
            for (const auto& [mid, mc] : b.markets)
                for (std::size_t r = 0; r < resource_count; ++r)
                    if (mc.hauler_want[r] > 0.0f || mc.household_weight[r] > 0.0f) { ++nonzero_seen; break; }
            if (!price_state_equal(a, b, where)) { same = false; first = k; }
            zero_register(b);
        }
        if (!same) std::printf("   first differing tick %d (%s)\n", first, where.c_str());
        check(nonzero_seen > 0, "H1.0 the clear writes the register (it is non-zero on some market) (vacuity)");
        check(same, "H1 register zeroed every tick vs kept: price, demand, supply, inventory, balances bit-identical on every tick (the price law never reads it)");
    }

    // ---- H2: the ceiling gate at the function -----------------------------
    {
        world a = w0;
        long pairs = 0, above_equal = 0, at_larger = 0, at_not_less = 0;
        const float res = reg_on.price_band().reservation_mult;
        for (int k = 1; k <= ticks; ++k)
        {
            play_tick(a, reg_on, k);
            std::vector<entity_id> mids;
            for (const auto& kv : a.markets) mids.push_back(kv.first);
            std::sort(mids.begin(), mids.end());
            for (const entity_id m : mids)
            {
                market_component& mc = a.markets.at(m);
                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    if (!(mc.hauler_want[r] > 0.0f) || !(mc.base_price[r] > 0.0f)) continue;
                    ++pairs;
                    const float ceil_l = res * mc.base_price[r];
                    const float above = std::nextafter(ceil_l, 1e30f);
                    const float keep = mc.hauler_want[r];
                    const float on_above = dispatch_absorbable(a, reg_on, m, r, above);
                    const float on_at    = dispatch_absorbable(a, reg_on, m, r, ceil_l);
                    mc.hauler_want[r] = 0.0f;
                    const float off_above = dispatch_absorbable(a, reg_on, m, r, above);
                    const float off_at    = dispatch_absorbable(a, reg_on, m, r, ceil_l);
                    mc.hauler_want[r] = keep;
                    if (on_above == off_above) ++above_equal;
                    if (on_at > off_at) ++at_larger;
                    if (on_at >= off_at) ++at_not_less;
                }
            }
        }
        std::printf("   H2: %ld (market, good) pairs carried hauler want over %d ticks\n", pairs, ticks);
        check(pairs > 0, "H2.0 some market carries ceiling-suppressed want (vacuity)");
        check(pairs > 0 && above_equal == pairs,
              "H2.1 landed just above the buyer's ceiling: the register adds no room");
        std::printf("   H2: at the ceiling the register added room on %ld of them (the rest: the market's listed supply already exceeds the sized target either way)\n", at_larger);
        check(pairs > 0 && at_not_less == pairs && at_larger > 0,
              "H2.2 landed at the buyer's ceiling: the register adds room (never less)");
    }

    // ---- H3 / H4: a short market receives a shipment ----------------------
    {
        world on = w0, on2 = w0, off = w0;
        const ship_tally t_on  = run_and_count(on, reg_on, ticks);
        const ship_tally t_off = run_and_count(off, reg_off, ticks);
        const ship_tally t_on2 = run_and_count(on2, reg_on, ticks);
        std::printf("   H3: into ceiling-suppressed (market, good), landed under the ceiling: ON %ld convoys %.1f u | OFF %ld convoys %.1f u\n",
                    t_on.convoys, t_on.units, t_off.convoys, t_off.units);
        check(t_on.units > 0.0, "H3.1 with the hauler view ON a short market's suppressed want receives a shipment");
        check(t_on.units > t_off.units, "H3.2 ON ships more into suppressed want than OFF");
        check(t_on.units == t_on2.units && on.state_hash(0) == on2.state_hash(0),
              "H4 the ON run repeats bit for bit (shipped units and state hash)");
    }

    std::printf("\n=== hauler_room: %d failure(s) ===\n", g_fail);
    return g_fail ? 1 : 0;
}
