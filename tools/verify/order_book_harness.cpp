// Headless order-book harness (BL-293; no SDL / Lua / ImGui).
//
// BL-293 moved the order book out of ui_state and into world state, put its
// matching inside the economy tick, and put three presses onto the corp-command
// seam. Each of those three moves has a way of being subtly wrong that a build
// does not catch, and this harness is the assertion for each.
//
// Style follows history_log_harness.cpp (hand-built registry + synthetic bodies,
// not the generated world) — the behaviours under test are all seam-level, and a
// synthetic world states the preconditions in the test rather than hoping the
// generator produced them.
//
//   R0  ITEM-SPANNING. A corp with a surplus places, holds and removes a
//       standing order entirely through apply_corp_command, with no UI: the
//       order lands in world::sell_orders, the economy tick sells against it
//       with nobody passing it in, and the removal takes it back out.
//
//   R1  WORLD OWNERSHIP. clear_markets reads the book from the world. The proof
//       that this is not just a moved field: an order placed and never handed to
//       anyone still debits the pool when the tick runs.
//
//   R2  ROUND-TRIP + REJECTION. write_order_book/read_order_book reproduce the
//       book field-for-field INCLUDING ITS ORDER, and a wrong-magic,
//       wrong-version or truncated stream is refused, leaving the destination
//       world's book untouched rather than half-written.
//
//   R3  VALIDATION. Each rejection reason of place_sell_order /
//       remove_sell_order / set_workforce_auto is distinguishable, and every
//       rejected command mutates nothing.
//
//   R4  DETERMINISM (the assertion the move earns). Two same-seed runs of the
//       same tick sequence, WITH order-book traffic on both, produce identical
//       world::state_hash at every sampled tick — and the hash MOVES when a
//       standing order changes, so the book is genuinely inside the
//       canonicalisation rather than invisible to it. Matching is price-time
//       priority, so this is specifically the assertion that iteration order over
//       the book is stable; if it ever stops being, the sim stops being
//       replayable and R4 is what says so.
//
//   R5  AI REACH + CONSERVATISM. The scored-utility layer reaches the trade verb
//       (a rival corp sitting on stock places an order on its own), and its first
//       cut is conservative: never below the market's rarity floor, never on a
//       pool without surplus past the hold threshold, never a duplicate order on
//       a triple that already has one.
//
//   R6  RESERVATION FLOOR (BL-386). The buyer-of-last-resort pass pays the
//       RESOLVED price, never the floor: an order whose floor exceeds the
//       resolved price clears nothing and keeps its stock, one at or below it
//       clears at the resolved price exactly, and no corp's clearing income
//       exceeds its cleared quantity × that price. The floor is a reservation
//       price, not a self-declared payout.
//
// The process exits non-zero if any assertion FAILs.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/order_book.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

// ---------------------------------------------------------------------------
// Hand-built registry — history_log_harness.cpp's make_registry(), trimmed to
// what the order book actually needs (an extraction economics so the strategic
// tier has something to score, and one recipe so a processor is legal).
// ---------------------------------------------------------------------------
recipe_registry make_registry()
{
    recipe_registry reg;

    building_economics extraction;
    extraction.base_rate            = 20.0f;
    extraction.maintenance          = 5.0f;
    extraction.base_wage            = 8.0f;
    extraction.build_cost           = 100.0f;
    extraction.build_duration_ticks = 2.0f;
    reg.set_economics(building_type::extraction_site, extraction);

    building_economics processing;
    processing.base_rate            = 8.0f;
    processing.maintenance          = 10.0f;
    processing.base_wage            = 12.0f;
    processing.build_cost           = 200.0f;
    processing.build_duration_ticks = 3.0f;
    reg.set_economics(building_type::processing_facility, processing);

    recipe steel;
    steel.name = "steel";
    steel.inputs[ri(resource_type::iron_ore)] = 2.0f;
    steel.outputs[ri(resource_type::steel)]   = 1.0f;
    reg.add_recipe(steel);

    return reg;
}

/// A body carrying one market that prices steel, plus a corp holding stock on it.
struct scenario
{
    world     w;
    entity_id body   = null_entity;
    entity_id market = null_entity;
    entity_id corp   = null_entity;
};

/// Build the shared fixture. `stock` units of steel sit in the (corp, body) pool;
/// the market prices steel at `base` with the resolved price seeded to match.
scenario make_scenario(float stock, float base = 8.0f)
{
    scenario s;

    s.body = s.w.create_entity();
    body_component bc{};
    bc.name              = "Anvil";
    bc.type              = body_type::planet;
    bc.orbital_radius_au = 1.0f;
    bc.grid_width = bc.grid_height = 4;
    s.w.bodies[s.body] = bc;

    s.market = s.w.create_entity();
    market_component mc{};
    mc.body = s.body;
    mc.base_price[ri(resource_type::steel)] = base;
    mc.price = mc.base_price;
    s.w.markets[s.market] = mc;

    s.corp = s.w.create_entity();
    corporation_component cc;
    cc.balance = 1000.0f;
    s.w.corporations[s.corp] = cc;

    s.w.pool_for(s.corp, s.body).quantities[ri(resource_type::steel)] = stock;
    return s;
}

/// A place_sell_order command against the fixture's body.
corp_command place_cmd(const scenario& s, float qty, float floor,
                       resource_type r = resource_type::steel)
{
    corp_command cmd;
    cmd.corp        = s.corp;
    cmd.verb        = corp_verb::place_sell_order;
    cmd.subject     = s.body;
    cmd.target      = r;
    cmd.quantity    = qty;
    cmd.floor_price = floor;
    return cmd;
}

float pool_steel(const scenario& s)
{
    return s.w.corp_body_pools.at(std::make_pair(s.corp, s.body)).quantities[ri(resource_type::steel)];
}

} // namespace

int main()
{
    const recipe_registry reg = make_registry();

    // -----------------------------------------------------------------------
    // R0 / R1 — the whole loop through the seam, with no UI and nothing passed in
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario(100.0f);

        const corp_command_result r = apply_corp_command(s.w, reg, place_cmd(s, 40.0f, 5.0f));
        check(r == corp_command_result::applied, "R0.1 place_sell_order applies through the seam");
        check(s.w.sell_orders.size() == 1, "R0.2 the order lands in world::sell_orders");
        check(!s.w.sell_orders.empty() && s.w.sell_orders[0].id != 0,
              "R0.3 the placed order carries a nonzero stable id");

        // The tick sells against the book with NOBODY handing it over — this is
        // the whole point of the item, so it gets its own assertion.
        const float before = pool_steel(s);
        economy_report empty;
        const auto flows = clear_markets(s.w, reg, empty);
        const float after = pool_steel(s);

        check(after < before, "R1.1 the economy tick sells against the standing order unprompted");
        check(before - after <= 40.0f + 0.01f, "R1.2 it sells no more than the order's quantity");
        check(flows.count(s.corp) && flows.at(s.corp).income > 0.0f,
              "R1.3 the sale credits the corp's income");

        const uint32_t id = s.w.sell_orders[0].id;
        corp_command rm;
        rm.corp = s.corp; rm.verb = corp_verb::remove_sell_order; rm.order = id;
        check(apply_corp_command(s.w, reg, rm) == corp_command_result::applied,
              "R0.4 remove_sell_order applies through the seam");
        check(s.w.sell_orders.empty(), "R0.5 the order leaves the book");
    }

    // -----------------------------------------------------------------------
    // R1b — OVERLAPPING ORDERS CANNOT SELL STOCK THAT IS NOT THERE
    // -----------------------------------------------------------------------
    // The seam permits several orders on one (corp, body, resource) — only the
    // AI declines to duplicate. Each is listed against the pool independently, so
    // without a cap the pool is debited once per order and the corp is paid for
    // goods it never held. Found by review, not by a failing build: it was always
    // reachable by a player adding two overlapping orders in the ledger, and
    // opening the command seam widened the door considerably.
    {
        scenario s = make_scenario(10.0f);
        apply_corp_command(s.w, reg, place_cmd(s, 10.0f, 1.0f));
        apply_corp_command(s.w, reg, place_cmd(s, 10.0f, 1.0f)); // same triple, whole pool again

        economy_report empty;
        const auto flows = clear_markets(s.w, reg, empty);

        check(pool_steel(s) >= -0.001f,
              "R1b.1 overlapping orders never drive the pool NEGATIVE (no goods from nothing)");
        check(flows.count(s.corp) && flows.at(s.corp).income <= 10.0f * 8.0f + 0.01f,
              "R1b.2 the corp is paid for at most the stock it actually held");
    }

    // -----------------------------------------------------------------------
    // R2 — round-trip and rejection
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario(200.0f);
        apply_corp_command(s.w, reg, place_cmd(s, 10.0f, 1.0f));
        apply_corp_command(s.w, reg, place_cmd(s, 20.0f, 2.0f, resource_type::steel));
        // Second order on the same triple is legal at the seam (only the AI
        // declines to duplicate), so this gives us a two-record book to order-test.
        buy_order b;
        b.id = s.w.allocate_order_id();
        b.corp = s.corp; b.body = s.body; b.resource = resource_type::steel;
        b.quantity = 5.0f; b.max_price = 12.0f;
        s.w.buy_orders.push_back(b);

        std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
        write_order_book(s.w, ss);

        world dest;
        const bool ok = read_order_book(dest, ss);
        check(ok, "R2.1 read_order_book succeeds on a stream it wrote itself");
        check(dest.sell_orders.size() == s.w.sell_orders.size() &&
              dest.buy_orders.size()  == s.w.buy_orders.size() &&
              dest.next_order_id      == s.w.next_order_id,
              "R2.2 counts and the id counter survive the round-trip");

        bool identical = dest.sell_orders.size() == s.w.sell_orders.size();
        for (std::size_t i = 0; identical && i < s.w.sell_orders.size(); ++i)
        {
            const sell_order& a = s.w.sell_orders[i];
            const sell_order& c = dest.sell_orders[i];
            identical = a.id == c.id && a.corp == c.corp && a.body == c.body &&
                        a.resource == c.resource && a.quantity == c.quantity &&
                        a.floor_price == c.floor_price;
        }
        // Field-for-field AND position-for-position: stored order is price-time
        // priority, so a round-trip that reordered the book would clear differently.
        check(identical, "R2.3 every sell order round-trips field-for-field, IN ITS STORED POSITION");

        // Wrong magic.
        {
            std::stringstream bad(std::ios::in | std::ios::out | std::ios::binary);
            write_order_book(s.w, bad);
            std::string bytes = bad.str();
            bytes[0] = 'X';
            std::stringstream in(bytes, std::ios::in | std::ios::binary);
            world victim;
            victim.sell_orders.push_back(sell_order{});
            const bool refused = !read_order_book(victim, in);
            check(refused && victim.sell_orders.size() == 1,
                  "R2.4 a wrong-magic stream is refused and leaves the destination book untouched");
        }

        // Wrong version.
        {
            std::stringstream bad(std::ios::in | std::ios::out | std::ios::binary);
            write_order_book(s.w, bad);
            std::string bytes = bad.str();
            bytes[4] = static_cast<char>(order_book_version + 7); // version's low byte
            std::stringstream in(bytes, std::ios::in | std::ios::binary);
            world victim;
            const bool refused = !read_order_book(victim, in);
            check(refused && victim.sell_orders.empty(),
                  "R2.5 a wrong-version stream is refused rather than reinterpreted");
        }

        // Truncated body.
        {
            std::stringstream bad(std::ios::in | std::ios::out | std::ios::binary);
            write_order_book(s.w, bad);
            std::string bytes = bad.str();
            bytes.resize(bytes.size() / 2);
            std::stringstream in(bytes, std::ios::in | std::ios::binary);
            world victim;
            victim.next_order_id = 99;
            const bool refused = !read_order_book(victim, in);
            check(refused && victim.sell_orders.empty() && victim.next_order_id == 99,
                  "R2.6 a truncated stream is refused and writes nothing partial");
        }
    }

    // -----------------------------------------------------------------------
    // R3 — player-grade validation; every rejection mutates nothing
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario(100.0f);

        {
            corp_command c = place_cmd(s, 10.0f, 1.0f);
            c.corp = s.w.create_entity(); // not a corporation
            check(apply_corp_command(s.w, reg, c) == corp_command_result::rejected_no_corp,
                  "R3.1 an unknown corp is rejected_no_corp");
        }
        {
            corp_command c = place_cmd(s, 10.0f, 1.0f);
            c.subject = s.w.create_entity(); // not a body
            check(apply_corp_command(s.w, reg, c) == corp_command_result::rejected_invalid,
                  "R3.2 a subject that is not a body is rejected_invalid");
        }
        {
            // A resource the market does not price — the UI's own precondition.
            corp_command c = place_cmd(s, 10.0f, 1.0f, resource_type::petroleum);
            check(apply_corp_command(s.w, reg, c) == corp_command_result::rejected_invalid,
                  "R3.3 an unpriced resource is rejected_invalid");
        }
        {
            corp_command c = place_cmd(s, 0.0f, 1.0f);
            check(apply_corp_command(s.w, reg, c) == corp_command_result::rejected_invalid,
                  "R3.4 a non-positive quantity is rejected_invalid");
        }
        {
            corp_command c = place_cmd(s, 10.0f, -1.0f);
            check(apply_corp_command(s.w, reg, c) == corp_command_result::rejected_invalid,
                  "R3.5 a negative floor price is rejected_invalid");
        }
        check(s.w.sell_orders.empty(), "R3.6 every rejected placement mutated nothing");

        // Removal of someone else's order, and of one that does not exist.
        apply_corp_command(s.w, reg, place_cmd(s, 10.0f, 1.0f));
        const uint32_t id = s.w.sell_orders.at(0).id;
        {
            const entity_id other = s.w.create_entity();
            s.w.corporations[other] = corporation_component{};
            corp_command c;
            c.corp = other; c.verb = corp_verb::remove_sell_order; c.order = id;
            // BL-397: deliberately the SAME code as a nonexistent id (R3.8) — a
            // distinguishable rejection was an oracle that let an id sweep map
            // the whole global book without placing an order.
            check(apply_corp_command(s.w, reg, c) == corp_command_result::rejected_invalid,
                  "R3.7 removing another corp's order is rejected_invalid (indistinguishable from R3.8)");
        }
        {
            corp_command c;
            c.corp = s.corp; c.verb = corp_verb::remove_sell_order; c.order = id + 999;
            check(apply_corp_command(s.w, reg, c) == corp_command_result::rejected_invalid,
                  "R3.8 removing a nonexistent order is rejected_invalid");
        }
        check(s.w.sell_orders.size() == 1, "R3.9 every rejected removal mutated nothing");

        // The per-corp cap keeps a runaway from growing the save format.
        while (s.w.sell_orders.size() < max_sell_orders_per_corp)
            if (apply_corp_command(s.w, reg, place_cmd(s, 1.0f, 1.0f)) != corp_command_result::applied)
                break;
        check(s.w.sell_orders.size() == max_sell_orders_per_corp,
              "R3.10 the book fills to the per-corp cap");
        check(apply_corp_command(s.w, reg, place_cmd(s, 1.0f, 1.0f)) == corp_command_result::rejected_state,
              "R3.11 placing past the cap is rejected_state, not silently dropped");

        // set_workforce_auto and the set_workforce pin it pairs with.
        {
            scenario t = make_scenario(0.0f);
            const entity_id tile = t.w.create_entity();
            tile_component tc{}; tc.body = t.body; t.w.tiles[tile] = tc;
            const entity_id bld = t.w.create_entity();
            building_component bc{};
            bc.type = building_type::extraction_site;
            bc.tile = tile;
            bc.workforce_auto   = true;
            bc.workforce_target = 100;
            t.w.buildings[bld] = bc;
            t.w.corporations[t.corp].assets.push_back(bld);

            corp_command sw;
            sw.corp = t.corp; sw.verb = corp_verb::set_workforce; sw.subject = bld;
            sw.workforce = 100; // the value it ALREADY holds, but auto is on
            check(apply_corp_command(t.w, reg, sw) == corp_command_result::applied,
                  "R3.12 set_workforce at the held value still applies while auto is on (it pins)");
            check(!t.w.buildings.at(bld).workforce_auto,
                  "R3.13 set_workforce clears workforce_auto, as the press always has");
            check(apply_corp_command(t.w, reg, sw) == corp_command_result::rejected_state,
                  "R3.14 repeating it once pinned is rejected_state (a real no-op)");

            corp_command au;
            au.corp = t.corp; au.verb = corp_verb::set_workforce_auto; au.subject = bld;
            check(apply_corp_command(t.w, reg, au) == corp_command_result::applied,
                  "R3.15 set_workforce_auto hands the dial back");
            check(t.w.buildings.at(bld).workforce_auto, "R3.16 workforce_auto is set");
            check(apply_corp_command(t.w, reg, au) == corp_command_result::rejected_state,
                  "R3.17 setting auto when already auto is rejected_state");
        }
    }

    // -----------------------------------------------------------------------
    // R4 — determinism: the assertion moving the book into the tick earns
    // -----------------------------------------------------------------------
    {
        // Same scripted tick sequence, run twice, with order-book traffic in it.
        auto run = [&reg](std::vector<uint64_t>& hashes) {
            scenario s = make_scenario(300.0f);
            for (int t = 0; t < 8; ++t)
            {
                // Deterministic traffic: place on even ticks, withdraw the oldest
                // on tick 5 — so the book both grows and shrinks during the run.
                if (t % 2 == 0)
                    apply_corp_command(s.w, reg, place_cmd(s, 5.0f + static_cast<float>(t), 2.0f));
                if (t == 5 && !s.w.sell_orders.empty())
                {
                    corp_command rm;
                    rm.corp = s.corp; rm.verb = corp_verb::remove_sell_order;
                    rm.order = s.w.sell_orders.front().id;
                    apply_corp_command(s.w, reg, rm);
                }
                s.w.current_day_tick = t;
                const economy_report rep = run_economy_step(s.w, reg);
                const auto flows = clear_markets(s.w, reg, rep);
                apply_budget(s.w, reg, flows, rep.workforce_contention, nullptr);
                hashes.push_back(s.w.state_hash(t));
            }
        };

        std::vector<uint64_t> a, b;
        run(a);
        run(b);
        check(!a.empty() && a.size() == b.size(), "R4.1 both runs sampled the same tick count");
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); ++i)
            if (a[i] != b[i]) same = false;
        check(same, "R4.2 state_hash is IDENTICAL at every tick across two same-seed runs with order traffic");

        // The hash must actually SEE the book — otherwise R4.2 passes vacuously.
        {
            scenario s = make_scenario(100.0f);
            const uint64_t bare = s.w.state_hash(0);
            apply_corp_command(s.w, reg, place_cmd(s, 10.0f, 3.0f));
            const uint64_t with_order = s.w.state_hash(0);
            check(bare != with_order, "R4.3 placing an order MOVES state_hash (the book is inside it)");

            // And the sequence is state, not just the contents: two books holding
            // the same two orders in a different order are different worlds.
            scenario x = make_scenario(100.0f);
            apply_corp_command(x.w, reg, place_cmd(x, 1.0f, 1.0f));
            apply_corp_command(x.w, reg, place_cmd(x, 2.0f, 2.0f));
            scenario y = make_scenario(100.0f);
            apply_corp_command(y.w, reg, place_cmd(y, 1.0f, 1.0f));
            apply_corp_command(y.w, reg, place_cmd(y, 2.0f, 2.0f));
            check(x.w.state_hash(0) == y.w.state_hash(0),
                  "R4.4 identical books built identically hash identically");
            std::swap(y.w.sell_orders[0], y.w.sell_orders[1]);
            check(x.w.state_hash(0) != y.w.state_hash(0),
                  "R4.5 the same orders in a different sequence hash DIFFERENTLY (price-time priority is state)");
        }
    }

    // -----------------------------------------------------------------------
    // R5 — the AI reaches the verb, conservatively
    // -----------------------------------------------------------------------
    {
        corp_ai_params p;
        p.cadence_k = 1; // every corp evaluates every tick — no waiting on the stagger

        scenario s = make_scenario(0.0f);
        // A rival (non-player) corp sitting on far more stock than the hold
        // threshold. No buildings, so trade is the only candidate available:
        // this isolates the trade scorer from build/dial/survey competition.
        s.w.player_entity = null_entity;
        s.w.pool_for(s.corp, s.body).quantities[ri(resource_type::steel)] =
            p.trade_hold_threshold + 100.0f;

        economy_report rep;
        run_corp_strategic_step(s.w, reg, rep, 0, p);

        check(s.w.sell_orders.size() == 1, "R5.1 a rival corp places a standing order on its own");
        if (!s.w.sell_orders.empty())
        {
            const sell_order& o = s.w.sell_orders.front();
            const float base = s.w.markets.at(s.market).base_price[ri(resource_type::steel)];
            check(o.corp == s.corp && o.body == s.body && o.resource == resource_type::steel,
                  "R5.2 the order names the corp's own pool");
            check(o.floor_price >= base * p.trade_floor_multiple - 0.001f,
                  "R5.3 CONSERVATISM: it never lists below the market's rarity floor");
            check(o.quantity > 0.0f && o.quantity <= 100.0f,
                  "R5.4 CONSERVATISM: it lists only a fraction of the excess, never the whole pool");
        }

        // A second evaluation must not stack another order on the same triple.
        const std::size_t after_first = s.w.sell_orders.size();
        economy_report rep2;
        run_corp_strategic_step(s.w, reg, rep2, 1, p);
        check(s.w.sell_orders.size() == after_first,
              "R5.5 CONSERVATISM: a later evaluation does not duplicate the standing order");

        // And a corp with nothing spare places nothing at all.
        scenario q = make_scenario(0.0f);
        q.w.player_entity = null_entity;
        q.w.pool_for(q.corp, q.body).quantities[ri(resource_type::steel)] =
            p.trade_hold_threshold - 1.0f; // just under the threshold
        economy_report rep3;
        run_corp_strategic_step(q.w, reg, rep3, 0, p);
        check(q.w.sell_orders.empty(),
              "R5.6 CONSERVATISM: stock below the hold threshold is not listed");

        // The player's corp is never traded on its behalf — the standing rule
        // that survives this item's widening of the AI's verbs.
        scenario pl = make_scenario(0.0f);
        pl.w.player_entity = pl.corp;
        pl.w.corporations[pl.corp].is_player = true;
        pl.w.pool_for(pl.corp, pl.body).quantities[ri(resource_type::steel)] =
            p.trade_hold_threshold + 500.0f;
        economy_report rep4;
        run_corp_strategic_step(pl.w, reg, rep4, 0, p);
        check(pl.w.sell_orders.empty(),
              "R5.7 the PLAYER's corp is never traded on its behalf (io-standing-rules.md)");
    }

    // -----------------------------------------------------------------------
    // R6 — the floor is a reservation price, not a self-declared payout (BL-386)
    // -----------------------------------------------------------------------
    // The defect this section pins down: the buyer-of-last-resort pass paid
    // max(ref_price, floor) with no counterparty debited, so clearing income was
    // linear in whatever floor the seller named — a money printer reachable by
    // any corp with one standing order.
    {
        // A floor above the resolved price clears NOTHING and keeps its stock.
        // With supply 40 against demand 0 the resolved price eases from 8 toward
        // the band floor (2.0), landing at 5.0 — so a floor of 6.0 sits above it.
        {
            scenario s = make_scenario(100.0f);
            apply_corp_command(s.w, reg, place_cmd(s, 40.0f, 6.0f));
            economy_report empty;
            const auto flows = clear_markets(s.w, reg, empty);
            const float rp = s.w.markets.at(s.market).price[ri(resource_type::steel)];
            check(rp < 6.0f, "R6.1 fixture: the resolved price sits below the order's floor");
            check(!flows.count(s.corp) || flows.at(s.corp).income == 0.0f,
                  "R6.2 HOLD: an order with floor above the resolved price earns nothing");
            check(pool_steel(s) == 100.0f,
                  "R6.3 HOLD: ...and its pool quantity is retained in full");
        }

        // The exploit shape itself: an absurd floor earns nothing at all, rather
        // than income linear in the named floor.
        {
            scenario s = make_scenario(100.0f);
            apply_corp_command(s.w, reg, place_cmd(s, 40.0f, 1.0e12f));
            economy_report empty;
            const auto flows = clear_markets(s.w, reg, empty);
            check((!flows.count(s.corp) || flows.at(s.corp).income == 0.0f) &&
                  pool_steel(s) == 100.0f,
                  "R6.4 a floor of 1e12 prints no money and sells no stock");
        }

        // A floor at or below the resolved price clears AT the resolved price —
        // not at the floor, not above it.
        {
            scenario s = make_scenario(100.0f);
            apply_corp_command(s.w, reg, place_cmd(s, 40.0f, 3.0f));
            economy_report empty;
            const auto flows = clear_markets(s.w, reg, empty);
            const float rp      = s.w.markets.at(s.market).price[ri(resource_type::steel)];
            const float cleared = 100.0f - pool_steel(s);
            check(cleared > 39.99f && cleared < 40.01f,
                  "R6.5 an order at/below the resolved price clears its full quantity");
            check(flows.count(s.corp) &&
                  std::fabs(flows.at(s.corp).income - cleared * rp) < 0.01f,
                  "R6.6 ...and is paid the RESOLVED price exactly (not the floor, not above)");
            check(flows.count(s.corp) && flows.at(s.corp).income <= cleared * rp + 0.01f,
                  "R6.7 INVARIANT: clearing income never exceeds cleared quantity x resolved price");
        }
    }

    // -----------------------------------------------------------------------
    // R7 — market inventory is credited with what LEFT a pool, never with what
    //      was merely listed (BL-422)
    // -----------------------------------------------------------------------
    // R6 above establishes that a held order earns nothing and keeps its stock.
    // That left half the reservation rule unenforced: clear_markets credited the
    // LISTED quantity to market_component::inventory before clearing ran, so a
    // held order still put stock on the shelf. inventory is not a display field —
    // economy_system draws processor inputs from it — so the phantom was bought,
    // decremented, and never paid for by anyone.
    //
    // The assertions are stated as an EQUALITY between inventory gain and pool
    // loss, so an under-credit fails them as loudly as an over-credit.
    {
        const std::size_t r_steel = ri(resource_type::steel);
        auto inventory_steel = [&](const scenario& s) {
            return s.w.markets.at(s.market).inventory[r_steel];
        };

        // HOLD: floor 6.0 against a resolved price of 5.0 (the R6.1 fixture).
        {
            scenario s = make_scenario(100.0f);
            apply_corp_command(s.w, reg, place_cmd(s, 40.0f, 6.0f));
            economy_report empty;
            clear_markets(s.w, reg, empty);
            check(s.w.markets.at(s.market).price[r_steel] < 6.0f,
                  "R7.1 fixture: the resolved price sits below the order's floor");
            check(inventory_steel(s) == 0.0f,
                  "R7.2 HOLD: a held order puts NO stock into the market's real inventory");
            check(pool_steel(s) == 100.0f && inventory_steel(s) == 0.0f,
                  "R7.3 HOLD: nothing left the pool, so nothing arrived on the shelf");
        }

        // CLEARS: floor 3.0, below the resolved price — the mirror case.
        {
            scenario s = make_scenario(100.0f);
            apply_corp_command(s.w, reg, place_cmd(s, 40.0f, 3.0f));
            economy_report empty;
            clear_markets(s.w, reg, empty);
            const float pool_loss = 100.0f - pool_steel(s);
            check(pool_loss > 39.99f && pool_loss < 40.01f,
                  "R7.4 fixture: the clearing order emptied its listed quantity from the pool");
            check(std::fabs(inventory_steel(s) - pool_loss) < 0.01f,
                  "R7.5 CONSERVATION: inventory gains exactly what left the pool");
        }

        // AUTO-SURPLUS: no order at all, so the greedy path sells the whole pool.
        // The third credit site, and the one that was already correct — asserted
        // so a later tidy of the three cannot quietly drop it.
        {
            scenario s = make_scenario(100.0f);
            economy_report empty;
            clear_markets(s.w, reg, empty);
            const float pool_loss = 100.0f - pool_steel(s);
            check(pool_loss > 0.0f,
                  "R7.6 fixture: with no standing order the auto-surplus path sells");
            check(std::fabs(inventory_steel(s) - pool_loss) < 0.01f,
                  "R7.7 CONSERVATION: the auto-surplus path credits exactly what it sold");
        }

        // MIXED TICK: a held order and a clearing order against the same
        // (market, resource). The single-order cases both pass if the credit is
        // computed per-resource rather than per-order; this one does not.
        {
            scenario s = make_scenario(100.0f);
            apply_corp_command(s.w, reg, place_cmd(s, 40.0f, 6.0f));  // holds
            apply_corp_command(s.w, reg, place_cmd(s, 30.0f, 1.0f));  // clears
            economy_report empty;
            clear_markets(s.w, reg, empty);
            const float rp = s.w.markets.at(s.market).price[r_steel];
            check(rp > 1.0f && rp < 6.0f,
                  "R7.8 fixture: the resolved price separates the two orders");
            const float pool_loss = 100.0f - pool_steel(s);
            check(pool_loss > 29.99f && pool_loss < 30.01f,
                  "R7.9 MIXED: only the clearing order's 30 units leave the pool");
            check(std::fabs(inventory_steel(s) - 30.0f) < 0.01f,
                  "R7.10 MIXED: the held order is not rescued by its clearing neighbour");
        }

        // MATCHED TRADE: the second credit site. No press authors a buy order yet
        // (components.hpp), so the buy side is pushed directly — the same
        // hand-built-world licence the rest of this harness runs on.
        {
            scenario s = make_scenario(100.0f);
            apply_corp_command(s.w, reg, place_cmd(s, 40.0f, 3.0f));

            const entity_id buyer = s.w.create_entity();
            corporation_component bc;
            bc.balance = 1000.0f;
            s.w.corporations[buyer] = bc;

            buy_order bo;
            bo.corp      = buyer;
            bo.body      = s.body;
            bo.resource  = resource_type::steel;
            bo.quantity  = 25.0f;
            bo.max_price = 999.0f;
            s.w.buy_orders.push_back(bo);

            economy_report empty;
            const auto flows = clear_markets(s.w, reg, empty);
            check(flows.count(buyer) && flows.at(buyer).expenditure > 0.0f,
                  "R7.11 fixture: the buy order matched and the buyer paid");
            const float pool_loss = 100.0f - pool_steel(s);
            check(std::fabs(inventory_steel(s) - pool_loss) < 0.01f,
                  "R7.12 CONSERVATION: matched fills and the auto-cleared remainder both credit once");
        }
    }

    std::printf("\n%s  (%d passed, %d failed)\n",
                g_fail == 0 ? "ALL PASS" : "FAILURES", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
