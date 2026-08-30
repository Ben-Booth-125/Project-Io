// Headless harness for BL-685 (the exchange record). No SDL / Lua / ImGui.
//
// The clearing tick retains one row per exchange in `world::exchanges`
// (`exchange_record_ring`, src/world/components.hpp). Authority:
// docs/economy/MARKETS.md § The exchange record.
//
//   E1  Clearing PRODUCES rows -- a tick that moves goods writes history.
//   E2  The rows MATCH WHAT MOVED: per corp, the sum of quantity * unit_price
//       over its rows is exactly the income/expenditure clearing accrued, and
//       the summed quantity is exactly what left the pool.
//   E3  `unit_price` is the price CLEARING RESOLVED, not the floor the order
//       carried -- an order listed well under the market clears at the market's
//       price, and the record must carry what the seller GOT.
//   E4  The append order is DETERMINISTIC: two runs of the same world produce
//       identical row sequences, AND a world whose markets were inserted into
//       the unordered_map in the opposite order produces the same sequence.
//   E5  The ring CAPS rather than growing without bound, and `oldest_first`
//       still reads chronologically across the wrap.
//   E6  Measurement, not an assertion: exchanges per econ tick FOR THIS SMALL
//       FIXTURE, which is a lower bound on the shipped world and not the number
//       `exchange_record_ring::capacity` should ultimately be sized against.
//       The shipped figure needs a generated world and a live recipe registry
//       (the substrate_census tier) that this harness deliberately does not pull
//       in; `data_creep_harness` samples `world.exchanges.total` over 4500 ticks
//       of the real world and is the place that number comes from.
//
// ON WHAT IS *NOT* HERE. There is no profit row, and its absence is the point:
// `stockpile_component` is `quantities[]` and nothing else, so no cost basis
// exists anywhere in the model and a margin is not derivable from a sale. E2
// asserts REVENUE against the money loop, which is the honest figure.
//
// Kept outside src/ so the CMake game glob does not pull it into the build.

#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <map>
#include <vector>

namespace {

int g_failures = 0;

void check(bool cond, const char* what)
{
    std::printf("  %s  %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond)
        ++g_failures;
}

bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

/// Field-wise equality, so a reordered or mistyped field cannot hide behind a
/// struct comparison the compiler does not generate.
bool same_row(const exchange_record& a, const exchange_record& b)
{
    return a.tick == b.tick && a.market == b.market && a.resource == b.resource
        && a.quantity == b.quantity && a.unit_price == b.unit_price && a.seller == b.seller
        && a.buyer == b.buyer;
}

bool same_sequence(const world& a, const world& b)
{
    if (a.exchanges.size() != b.exchanges.size())
        return false;
    for (std::size_t i = 0; i < a.exchanges.size(); ++i)
        if (!same_row(a.exchanges.oldest_first(i), b.exchanges.oldest_first(i)))
            return false;
    return true;
}

/// One market on one body, two corps holding pools against it.
///
/// `reverse_market_insert` decides only the order the market entities are
/// INSERTED into `world::markets`, which is an unordered_map: the logical world
/// is identical either way, and E4 leans on that -- a walk that read the hash
/// container's own iteration order would come out differently between the two.
struct fixture
{
    entity_id body = null_entity;
    entity_id m1 = null_entity, m2 = null_entity;
    entity_id c1 = null_entity, c2 = null_entity;
};

fixture build(world& w, bool reverse_market_insert)
{
    fixture f;

    f.body = w.create_entity();
    w.bodies[f.body] = body_component{};

    // Two markets on one body, so the walk has something to order.
    f.m1 = w.create_entity();
    f.m2 = w.create_entity();

    market_component a;
    a.body                              = f.body;
    a.base_price[ri(resource_type::iron_ore)] = 4.0f;
    a.base_price[ri(resource_type::coal)]     = 2.0f;
    a.price                             = a.base_price;

    market_component b = a;
    b.base_price[ri(resource_type::iron_ore)] = 6.0f;
    b.price                             = b.base_price;

    if (reverse_market_insert)
    {
        w.markets[f.m2] = b;
        w.markets[f.m1] = a;
    }
    else
    {
        w.markets[f.m1] = a;
        w.markets[f.m2] = b;
    }

    f.c1 = w.create_entity();
    f.c2 = w.create_entity();
    { corporation_component cc; cc.balance = 10000.0f; cc.is_player = true;  w.corporations[f.c1] = cc; }
    { corporation_component cc; cc.balance = 10000.0f; cc.is_player = false; w.corporations[f.c2] = cc; }

    // Surplus in both corps' pools, in two resources, so the per-resource walk
    // has something to order too.
    w.pool_for(f.c1, f.body).quantities[ri(resource_type::iron_ore)] = 40.0f;
    w.pool_for(f.c1, f.body).quantities[ri(resource_type::coal)]     = 25.0f;
    w.pool_for(f.c2, f.body).quantities[ri(resource_type::iron_ore)] = 10.0f;

    w.current_econ_tick = 7;
    return f;
}

} // namespace

int main()
{
    recipe_registry reg;
    const economy_report empty_report;

    std::printf("The exchange record (BL-685) harness\n");

    // -----------------------------------------------------------------------
    // E1/E2 -- rows are produced, and they match what moved
    // -----------------------------------------------------------------------
    {
        world w;
        const fixture f = build(w, false);

        const float pool_before_c1_iron = w.pool_for(f.c1, f.body).quantities[ri(resource_type::iron_ore)];
        const float pool_before_c1_coal = w.pool_for(f.c1, f.body).quantities[ri(resource_type::coal)];
        const float pool_before_c2_iron = w.pool_for(f.c2, f.body).quantities[ri(resource_type::iron_ore)];

        const auto flows = clear_markets(w, reg, empty_report);

        check(w.exchanges.size() > 0, "E1 a clearing tick that moves goods writes rows");
        check(w.exchanges.total == w.exchanges.size(),
              "E1 the lifetime counter agrees with the retained count before any wrap");

        bool tick_ok = true, market_known = true, price_positive = true;
        std::map<entity_id, float> revenue, qty_by_seller;
        for (std::size_t i = 0; i < w.exchanges.size(); ++i)
        {
            const exchange_record& e = w.exchanges.oldest_first(i);
            tick_ok      = tick_ok && e.tick == w.current_econ_tick;
            market_known = market_known && w.markets.count(e.market) == 1;
            price_positive = price_positive && e.unit_price > 0.0f;
            if (e.seller != null_entity)
            {
                revenue[e.seller]      += e.quantity * e.unit_price;
                qty_by_seller[e.seller] += e.quantity;
            }
        }
        check(tick_ok, "E2 every row carries the econ tick it cleared on");
        check(market_known, "E2 every row names a market that exists");
        check(price_positive, "E2 every row carries a positive unit price");

        // The whole point of writing the row beside the cash accrual: the two
        // cannot drift. A mismatch here means the record and the money loop are
        // reading different quantities or different prices.
        bool revenue_matches = true;
        for (const auto& [corp, rev] : revenue)
        {
            const auto fit = flows.find(corp);
            const float income = (fit == flows.end()) ? 0.0f : fit->second.income;
            if (!near(rev, income, 1e-2f))
            {
                revenue_matches = false;
                std::printf("        %u: rows say %.4f, clearing paid %.4f\n",
                            (unsigned)corp, (double)rev, (double)income);
            }
        }
        check(revenue_matches,
              "E2 sum(quantity * unit_price) per seller EQUALS the income clearing accrued");

        // ...and the quantity is what actually left the pool.
        const float moved_c1 = (pool_before_c1_iron + pool_before_c1_coal)
            - (w.pool_for(f.c1, f.body).quantities[ri(resource_type::iron_ore)]
               + w.pool_for(f.c1, f.body).quantities[ri(resource_type::coal)]);
        const float moved_c2 = pool_before_c2_iron
            - w.pool_for(f.c2, f.body).quantities[ri(resource_type::iron_ore)];
        check(near(qty_by_seller[f.c1], moved_c1, 1e-2f),
              "E2 the recorded quantity is exactly what left the seller's pool (corp 1)");
        check(near(qty_by_seller[f.c2], moved_c2, 1e-2f),
              "E2 the recorded quantity is exactly what left the seller's pool (corp 2)");

        // The market as counterparty. Auto-surplus has no buying corp, so the
        // buyer side is null_entity BY DESIGN and a reader must render it as the
        // market -- asserting it here is what stops a later change quietly
        // filling it with something that looks like a corp id.
        bool market_side_null = false;
        for (std::size_t i = 0; i < w.exchanges.size(); ++i)
            if (w.exchanges.oldest_first(i).buyer == null_entity)
                market_side_null = true;
        check(market_side_null,
              "E2 an auto-surplus sale records null_entity for the buyer (the market itself)");
    }

    // -----------------------------------------------------------------------
    // E3 -- unit_price is what clearing RESOLVED, not the order's floor
    // -----------------------------------------------------------------------
    // A standing sell order carrying a floor far BELOW the market's resolved
    // price. It clears (the floor is a reservation price, not an ask the market
    // is held to), and the row must carry the resolved price -- recording the
    // floor would under-report the seller's revenue by 75% here, and would not
    // reconcile against the income clearing actually paid.
    {
        world w;
        const fixture f = build(w, false);

        sell_order o;
        o.id          = w.next_order_id++;
        o.corp        = f.c1;
        o.body        = f.body;
        o.resource    = resource_type::iron_ore;
        o.quantity    = 10.0f;
        o.floor_price = 1.0f; // the market's base price for iron is 4.0
        w.sell_orders.push_back(o);

        const auto flows = clear_markets(w, reg, empty_report);

        bool found = false, carries_resolved = false, carries_floor = false;
        float recorded = 0.0f;
        for (std::size_t i = 0; i < w.exchanges.size(); ++i)
        {
            const exchange_record& e = w.exchanges.oldest_first(i);
            if (e.seller != f.c1 || e.resource != resource_type::iron_ore)
                continue;
            found = true;
            recorded = e.unit_price;
            if (near(e.unit_price, o.floor_price))
                carries_floor = true;
            else
                carries_resolved = true;
        }
        check(found, "E3 the order's sale is recorded");
        check(!carries_floor && carries_resolved,
              "E3 the row carries the RESOLVED price, never the floor the order carried");
        std::printf("        floor asked %.3f, price recorded %.3f\n",
                    (double)o.floor_price, (double)recorded);

        // And the reconciliation still holds with an order in play, which is the
        // check that would catch recording the floor by another route.
        float rev = 0.0f;
        for (std::size_t i = 0; i < w.exchanges.size(); ++i)
        {
            const exchange_record& e = w.exchanges.oldest_first(i);
            if (e.seller == f.c1)
                rev += e.quantity * e.unit_price;
        }
        const auto fit = flows.find(f.c1);
        check(fit != flows.end() && near(rev, fit->second.income, 1e-2f),
              "E3 revenue still reconciles against the income clearing accrued");
    }

    // -----------------------------------------------------------------------
    // E4 -- the append order is a total order over the clearing walk
    // -----------------------------------------------------------------------
    {
        world a, b, c;
        build(a, false);
        build(b, false);
        build(c, true); // same world, markets inserted into the hash map reversed

        clear_markets(a, reg, empty_report);
        clear_markets(b, reg, empty_report);
        clear_markets(c, reg, empty_report);

        check(a.exchanges.size() > 1, "E4 the fixture produces more than one row to order");
        check(same_sequence(a, b), "E4 two runs of the same world produce identical row sequences");
        check(same_sequence(a, c),
              "E4 markets inserted into the unordered_map in the OPPOSITE order "
              "produce the same sequence");

        // Ten more ticks on `a`, replayed on `b`: an ordering that only diverges
        // once the containers have grown would be missed by a single tick.
        for (int t = 0; t < 10; ++t)
        {
            a.current_econ_tick = 100 + t;
            b.current_econ_tick = 100 + t;
            clear_markets(a, reg, empty_report);
            clear_markets(b, reg, empty_report);
        }
        check(same_sequence(a, b), "E4 the sequences still agree after eleven ticks");
    }

    // -----------------------------------------------------------------------
    // E5 -- the ring caps, and reads chronologically across the wrap
    // -----------------------------------------------------------------------
    {
        exchange_record_ring ring;
        const std::size_t cap  = exchange_record_ring::capacity;
        const std::size_t over = cap + 37;

        for (std::size_t i = 0; i < over; ++i)
        {
            exchange_record e;
            e.tick = static_cast<int>(i); // the row's identity, for the wrap check
            ring.push(e);
        }

        check(ring.size() == cap, "E5 the ring caps at its capacity rather than growing");
        check(ring.total == over, "E5 the lifetime counter keeps counting past the cap");

        // After a wrap the first 37 rows are gone and row 37 is the oldest held.
        check(ring.oldest_first(0).tick == static_cast<int>(over - cap),
              "E5 oldest_first(0) is the oldest SURVIVING row after the wrap");
        check(ring.oldest_first(cap - 1).tick == static_cast<int>(over - 1),
              "E5 oldest_first(size-1) is the newest row after the wrap");

        bool monotonic = true;
        for (std::size_t i = 1; i < ring.size(); ++i)
            monotonic = monotonic && ring.oldest_first(i).tick == ring.oldest_first(i - 1).tick + 1;
        check(monotonic, "E5 oldest_first reads chronologically straight through the wrap");

        // And the same bound holds under real clearing rather than hand pushes.
        world w;
        build(w, false);
        for (int t = 0; t < 40; ++t)
        {
            w.current_econ_tick = t;
            clear_markets(w, reg, empty_report);
        }
        check(w.exchanges.size() <= cap, "E5 forty ticks of clearing stay inside the cap");
    }

    // -----------------------------------------------------------------------
    // E6 -- measurement: rows per econ tick FOR THIS FIXTURE (a lower bound)
    // -----------------------------------------------------------------------
    {
        world w;
        build(w, false);
        std::printf("\n  tick   rows this tick   ring size\n");
        std::size_t prev = 0;
        for (int t = 0; t < 5; ++t)
        {
            w.current_econ_tick = t;
            // Re-stock, so each tick has something to clear rather than trailing
            // off to zero -- the measurement wants a BUSY tick, not a quiet one.
            for (auto& [key, pool] : w.corp_body_pools)
                pool.quantities[static_cast<std::size_t>(resource_type::iron_ore)] += 20.0f;
            clear_markets(w, reg, empty_report);
            std::printf("  %4d   %14llu   %9llu\n", t,
                        (unsigned long long)(w.exchanges.total - prev),
                        (unsigned long long)w.exchanges.size());
            prev = w.exchanges.total;
        }
        std::printf("  ring capacity: %llu rows\n\n",
                    (unsigned long long)exchange_record_ring::capacity);
    }

    std::printf("\n%s (%d failure(s))\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
