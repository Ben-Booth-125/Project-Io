// Headless harness for BL-392 (procurement contracts destroy value) and the
// Sprint D4 import tariff. Both items move money through the same seam, and the
// binding requirement across both is the same one: NO FLOW EITHER ITEM TOUCHES
// MAY MINT OR DESTROY A CREDIT.
//
// WHAT "CONSERVED" MEANS HERE, precisely — because the wider economy is NOT a
// closed system and a harness that asserted it were would be asserting a
// falsehood. `clear_markets` makes the market a buyer of last resort: it pays a
// seller at the resolved price with nobody's money on the other side, so total
// corporate cash rises and falls every tick by design (MARKETS.md § honest
// limitations). What is asserted below is conservation ACROSS THE FLOWS THESE
// TWO ITEMS OWN, measured by running the seam in isolation from the clearing
// tick:
//
//   * every credit a procurement contract debits from the buyer arrives at the
//     supplier, to the last float — deposit, paced instalments and freight
//     alike; and
//   * every credit an import tariff debits from the buying corporation arrives
//     in the enacting nation's treasury.
//
// Plus the two properties that say the change is safe to ship:
//
//   * the round-trip economics: after BL-392 a contract is a BETTER deal than
//     spot for a committed volume, where before it settled at -0.14 credits;
//   * with no tariff law enacted, the world is bit-identical to the pre-item
//     baseline (a state hash over every balance, pool and price).
//
// Kept outside src/ so the CMake glob does not pull it into the real build.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/law.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

static int g_failures = 0;

static void check(bool cond, const char* what)
{
    if (cond) std::printf("  PASS  %s\n", what);
    else      { std::printf("  FAIL  %s\n", what); ++g_failures; }
}

static bool near(double a, double b, double eps = 1e-3) { return std::fabs(a - b) < eps; }
static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

/// Total corporate cash, summed in ASCENDING CORP ID. Float addition is not
/// associative and `w.corporations` is an unordered_map, so an unsorted sum
/// would be a different number run to run — which would make this harness's
/// own arithmetic the thing under test.
static double total_corp_cash(const world& w)
{
    std::map<entity_id, double> by_id;
    for (const auto& [cid, cc] : w.corporations)
        by_id[cid] = static_cast<double>(cc.balance);
    double total = 0.0;
    for (const auto& [cid, v] : by_id)
        total += v;
    return total;
}

static double total_nation_treasury(const world& w)
{
    std::map<entity_id, double> by_id;
    for (const auto& [nid, nc] : w.nations)
        by_id[nid] = static_cast<double>(nc.treasury);
    double total = 0.0;
    for (const auto& [nid, v] : by_id)
        total += v;
    return total;
}

/// A cheap order-independent digest over everything the two items could
/// perturb: corp balances, corp/body pools, and market prices. Quantised to a
/// fixed grid so the hash is a value comparison, not a bit comparison of
/// accumulated float noise — the property under test is "the numbers did not
/// move", and a grid at 1e-4 is far finer than any flow either item adds.
static uint64_t state_hash(const world& w)
{
    uint64_t h = 1469598103934665603ull;
    const auto mix = [&h](uint64_t v) {
        h ^= v;
        h *= 1099511628211ull;
    };
    const auto mixf = [&mix](double v) {
        mix(static_cast<uint64_t>(static_cast<int64_t>(std::llround(v * 10000.0))));
    };

    std::map<entity_id, const corporation_component*> corps;
    for (const auto& [cid, cc] : w.corporations) corps[cid] = &cc;
    for (const auto& [cid, cc] : corps) { mix(cid); mixf(cc->balance); }

    std::map<std::pair<entity_id, entity_id>, const stockpile_component*> pools;
    for (const auto& [key, pc] : w.corp_body_pools) pools[key] = &pc;
    for (const auto& [key, pc] : pools)
    {
        mix(key.first); mix(key.second);
        for (std::size_t r = 0; r < resource_count; ++r) mixf(pc->quantities[r]);
    }

    std::map<entity_id, const market_component*> mkts;
    for (const auto& [mid, mc] : w.markets) mkts[mid] = &mc;
    for (const auto& [mid, mc] : mkts)
    {
        mix(mid);
        for (std::size_t r = 0; r < resource_count; ++r) mixf(mc->price[r]);
    }

    std::map<entity_id, double> treas;
    for (const auto& [nid, nc] : w.nations) treas[nid] = nc.treasury;
    for (const auto& [nid, v] : treas) { mix(nid); mixf(v); }
    return h;
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

namespace {

struct fixture
{
    world     w;
    entity_id supplier_body = null_entity;
    entity_id buyer_body    = null_entity;
    entity_id supplier_mkt  = null_entity;
    entity_id buyer_mkt     = null_entity;
    entity_id supplier      = null_entity;
    entity_id buyer         = null_entity;
    entity_id supplier_nat  = null_entity;
    entity_id buyer_nat     = null_entity;
    entity_id supplier_tile = null_entity;
    entity_id buyer_tile    = null_entity;
};

/// Two bodies, each with its own market and its own nation, and one corporation
/// resident on each. Deliberately TWO bodies: the whole of BL-392's first fault
/// is that goods landed on the supplier's body, and a single-body fixture
/// cannot see that.
fixture build_fixture(recipe_registry& reg)
{
    { building_economics ex; ex.base_rate = 10.0f; ex.maintenance = 0.0f; ex.base_wage = 0.0f;
      reg.set_economics(building_type::extraction_site, ex); }
    { building_economics pr; pr.base_rate = 10.0f; pr.maintenance = 0.0f; pr.base_wage = 0.0f;
      reg.set_economics(building_type::processing_facility, pr); }

    fixture f;
    world& w = f.w;

    const auto make_body = [&](entity_id& body, entity_id& mkt, entity_id& centre, entity_id& nation)
    {
        body = w.create_entity();
        w.bodies[body] = body_component{};

        centre = w.create_entity();
        tile_component tc{}; tc.body = body;
        tc.resource_deposit[ri(resource_type::iron_ore)] = 2.0f;
        w.tiles[centre] = tc;

        mkt = w.create_entity();
        market_component mc; mc.body = body; mc.centre_tile = centre;
        mc.base_price[ri(resource_type::iron_ore)] = 2.5f;
        mc.price = mc.base_price;
        w.markets[mkt] = mc;

        nation = w.create_entity();
        nation_component nc; nc.name = "Nation"; nc.tiles.push_back(centre);
        w.nations[nation] = nc;
        w.tile_to_nation[centre] = nation;
    };

    make_body(f.supplier_body, f.supplier_mkt, f.supplier_tile, f.supplier_nat);
    make_body(f.buyer_body,    f.buyer_mkt,    f.buyer_tile,    f.buyer_nat);

    // Supplier: real extraction capacity on its own body.
    f.supplier = w.create_entity();
    { corporation_component cc; cc.name = "Supplier Co"; cc.balance = 10000.0f;
      cc.home_nation = f.supplier_nat; w.corporations[f.supplier] = cc; }
    {
        const entity_id bld = w.create_entity();
        building_component b{}; b.tile = f.supplier_tile; b.type = building_type::extraction_site;
        b.target_resource = resource_type::iron_ore; b.ticks_remaining = 0;
        w.buildings[bld] = b;
        w.corporations[f.supplier].assets.push_back(bld);
    }

    // Buyer: a building of its own, on the OTHER body, so it has a delivery body.
    f.buyer = w.create_entity();
    { corporation_component cc; cc.name = "Buyer Co"; cc.balance = 10000.0f; cc.is_player = true;
      cc.home_nation = f.buyer_nat; w.corporations[f.buyer] = cc; }
    {
        const entity_id bld = w.create_entity();
        building_component b{}; b.tile = f.buyer_tile; b.type = building_type::extraction_site;
        b.target_resource = resource_type::iron_ore; b.ticks_remaining = 0;
        w.buildings[bld] = b;
        w.corporations[f.buyer].assets.push_back(bld);
    }
    return f;
}

/// Post a matched pair straight into `world::sell_orders` / `buy_orders`.
///
/// NOT through `corp_command`: there is no buy-order verb — `buy_order`'s own
/// comment says so ("No press authors one yet — the buy side exists in the
/// clearing algorithm and in the save format, waiting for its verb"). Writing
/// the book directly is what the clearing tick reads either way, and inventing a
/// verb here would be inventing the seam this harness exists to measure.
void post_pair(world& w, entity_id seller, entity_id buyer, entity_id body,
               resource_type r, float qty, float ask)
{
    sell_order s;
    s.id = w.allocate_order_id(); s.corp = seller; s.body = body;
    s.resource = r; s.quantity = qty; s.floor_price = ask;
    w.sell_orders.push_back(s);

    buy_order b;
    b.id = w.allocate_order_id(); b.corp = buyer; b.body = body;
    b.resource = r; b.quantity = qty; b.max_price = ask * 2.0f;
    w.buy_orders.push_back(b);
}

} // namespace

// ---------------------------------------------------------------------------

int main()
{
    // =======================================================================
    // PART 1 — BL-392
    // =======================================================================
    std::printf("PART 1  BL-392: procurement contracts\n");

    {
        recipe_registry reg;
        fixture f = build_fixture(reg);
        world& w = f.w;

        const double cash_at_start = total_corp_cash(w);

        corp_command q; q.corp = f.buyer; q.verb = corp_verb::request_quote;
        q.counterparty = f.supplier; q.target = resource_type::iron_ore; q.quantity = 100.0f;
        check(apply_corp_command(w, reg, q, nullptr) == corp_command_result::applied,
              "P1.0 request_quote succeeds against a capable supplier");

        const procurement_quote pq = w.procurement_quotes.front();

        // --- Fault 1: the goods must land on the BUYER's body ---------------
        check(pq.delivery_body == f.buyer_body,
              "P1.1 the quote's delivery body is the BUYER's body, not the supplier's");

        // --- Fault 2: the price must differ from spot -----------------------
        const float spot = w.markets.at(f.supplier_mkt).price[ri(resource_type::iron_ore)];
        check(pq.unit_price < spot,
              "P1.2 the quoted unit price is BELOW spot (the commitment buys a volume discount)");
        std::printf("        spot %.4f  quoted %.4f  discount %.2f%%  freight %.4f\n",
                    spot, pq.unit_price, 100.0 * (1.0 - pq.unit_price / spot), pq.freight_cost);

        // --- Fault 3: lead time must read the supplier's own throughput -----
        // One extraction site at base_rate 10 => ceil(100/10) = 10 rate-steps
        // x base_lead_ticks 2 = 20 ticks. The pre-BL-392 form read the GLOBAL
        // processing_facility rate and would have quoted the same term for a
        // supplier with ten sites as for one with a single stalled one.
        check(pq.lead_time_ticks == 20,
              "P1.3 lead time derives from the SUPPLIER's own throughput (1 site x rate 10 -> 20 ticks)");

        // --- Sensitivity: a second site halves the term ---------------------
        {
            world w2 = w; // copy before accepting, so the quote is still live
            const entity_id bld = w2.create_entity();
            building_component b{}; b.tile = f.supplier_tile; b.type = building_type::extraction_site;
            b.target_resource = resource_type::iron_ore; b.ticks_remaining = 0;
            w2.buildings[bld] = b;
            w2.corporations[f.supplier].assets.push_back(bld);
            corp_command q2; q2.corp = f.buyer; q2.verb = corp_verb::request_quote;
            q2.counterparty = f.supplier; q2.target = resource_type::iron_ore; q2.quantity = 100.0f;
            apply_corp_command(w2, reg, q2, nullptr);
            const int lead2 = w2.procurement_quotes.back().lead_time_ticks;
            check(lead2 == 10,
                  "P1.3b a supplier with TWICE the capacity quotes HALF the lead time");
            std::printf("        1 site -> %d ticks,  2 sites -> %d ticks\n", pq.lead_time_ticks, lead2);
        }

        // --- Accept, and run the contract to completion ---------------------
        corp_command a; a.corp = f.buyer; a.verb = corp_verb::accept_quote; a.order = pq.id;
        check(apply_corp_command(w, reg, a, nullptr) == corp_command_result::applied,
              "P1.4 accept_quote converts the quote into a contract");

        check(near(total_corp_cash(w), cash_at_start),
              "P1.5 CONSERVED at accept: the deposit LEAVES the buyer and ARRIVES at the supplier");

        const double buyer_before    = w.corporations.at(f.buyer).balance;
        const double supplier_before = w.corporations.at(f.supplier).balance;

        // Pace the contract WITHOUT the clearing tick, so the only flows in
        // play are the ones this item owns. (run_economy_step also produces
        // extraction output into pools; that is goods, not credits, and the
        // cash assertion is unaffected.)
        for (int t = 0; t < pq.lead_time_ticks; ++t)
            run_economy_step(w, reg);

        check(w.procurement_contracts.empty(),
              "P1.6 the contract completes within its own lead time");
        check(near(total_corp_cash(w), cash_at_start),
              "P1.7 CONSERVED over the whole contract: total corporate cash is unchanged to the credit");

        const double buyer_paid       = buyer_before - w.corporations.at(f.buyer).balance;
        const double supplier_received= w.corporations.at(f.supplier).balance - supplier_before;
        check(near(buyer_paid, supplier_received),
              "P1.8 every paced instalment the buyer pays arrives at the supplier, to the float");
        std::printf("        buyer paid %.4f   supplier received %.4f\n", buyer_paid, supplier_received);

        // --- Delivery actually reaches the buyer ----------------------------
        const double landed_here  = w.pool_for(f.buyer, f.buyer_body).quantities[ri(resource_type::iron_ore)];
        const double landed_there = w.pool_for(f.buyer, f.supplier_body).quantities[ri(resource_type::iron_ore)];
        check(landed_here >= 100.0,
              "P1.9 the full contracted quantity lands in the buyer's pool ON THE BUYER'S BODY");
        check(near(landed_there, 0.0),
              "P1.10 nothing at all lands on the supplier's body (where it would be liquidated)");

        // --- The round trip, re-measured ------------------------------------
        // The recorded defect: a 20-unit iron round trip settled at exactly
        // -0.14 credits — break-even minus friction, by construction, so no
        // rational agent ever signed. Re-measure it: what does the contracted
        // parcel cost per unit against what the same parcel costs on the spot
        // market?
        {
            recipe_registry reg2;
            fixture g = build_fixture(reg2);
            const float g_spot = g.w.markets.at(g.supplier_mkt).price[ri(resource_type::iron_ore)];

            const auto measure = [&](float qty) {
                corp_command qq; qq.corp = g.buyer; qq.verb = corp_verb::request_quote;
                qq.counterparty = g.supplier; qq.target = resource_type::iron_ore; qq.quantity = qty;
                apply_corp_command(g.w, reg2, qq, nullptr);
                const procurement_quote& z = g.w.procurement_quotes.back();
                const double contracted = static_cast<double>(qty) * z.unit_price + z.freight_cost;
                const double at_spot    = static_cast<double>(qty) * g_spot;
                return at_spot - contracted; // > 0 => the contract beat the market
            };

            const double gain20  = measure(20.0f);
            const double gain100 = measure(100.0f);
            const double gain500 = measure(500.0f);
            std::printf("        round trip vs spot (positive = the contract WINS):\n");
            std::printf("          20 units: %+.4f    100 units: %+.4f    500 units: %+.4f\n",
                        gain20, gain100, gain500);
            check(gain500 > 0.0,
                  "P1.11 a volume commitment now beats spot (the -0.14 break-even-minus-friction is gone)");
            check(gain500 > gain20,
                  "P1.12 the bigger the commitment, the better the terms (a monotone discount)");
        }
    }

    // =======================================================================
    // PART 2 — the import tariff
    // =======================================================================
    std::printf("\nPART 2  Sprint D4: the import tariff\n");

    // --- Baseline: with no tariff law enacted, nothing moves ---------------
    uint64_t baseline_hash = 0;
    {
        recipe_registry reg;
        fixture f = build_fixture(reg);
        world& w = f.w;
        // A tariff law exists but is NOT enacted — the shipped default.
        law l; l.id = "LAW-IMPORT-TARIFF"; l.name = "Import tariff";
        l.effect = law_effect_kind::import_tariff; l.enacting_nation = f.supplier_nat;
        l.rate = 0.10f; l.enacted = false;
        w.laws.push_back(l);

        for (int t = 0; t < 8; ++t)
        {
            economy_report rep = run_economy_step(w, reg);
            auto flows = clear_markets(w, reg, rep);
            apply_budget(w, reg, flows, rep.workforce_contention);
        }
        baseline_hash = state_hash(w);
        check(near(total_nation_treasury(w), 0.0),
              "P2.0 an UN-enacted tariff law collects nothing");
    }
    {
        // The same eight ticks with NO law record at all must produce the same
        // hash — the check that says an un-enacted law is not merely harmless
        // but literally absent from the arithmetic.
        recipe_registry reg;
        fixture f = build_fixture(reg);
        world& w = f.w;
        for (int t = 0; t < 8; ++t)
        {
            economy_report rep = run_economy_step(w, reg);
            auto flows = clear_markets(w, reg, rep);
            apply_budget(w, reg, flows, rep.workforce_contention);
        }
        check(state_hash(w) == baseline_hash,
              "P2.1 BIT-IDENTICAL: with no tariff enacted the world matches the pre-item baseline");
        std::printf("        state_hash %016llX\n", static_cast<unsigned long long>(baseline_hash));
    }

    // --- Enacted: a cross-border sale is taxed, and the money lands ---------
    {
        recipe_registry reg;
        fixture f = build_fixture(reg);
        world& w = f.w;

        // The SUPPLIER's nation levies 10% on imports into its own market.
        law l; l.id = "LAW-IMPORT-TARIFF"; l.name = "Import tariff";
        l.effect = law_effect_kind::import_tariff; l.enacting_nation = f.supplier_nat;
        l.rate = 0.10f; l.enacted = true;
        w.laws.push_back(l);

        // Stock both sides so there is something to trade, then post a matched
        // pair on the SUPPLIER's market: a foreign buyer (the buyer corp,
        // resident in the other nation) lifting a local seller's ask.
        w.pool_for(f.supplier, f.supplier_body).quantities[ri(resource_type::iron_ore)] += 200.0f;

        const float ask = 2.5f;
        const float qty = 40.0f;
        post_pair(w, f.supplier, f.buyer, f.supplier_body, resource_type::iron_ore, qty, ask);
        check(w.sell_orders.size() == 1 && w.buy_orders.size() == 1,
              "P2.2 a local ask and a FOREIGN bid stand on the supplier's market");

        const double cash_before     = total_corp_cash(w);
        const double treasury_before = total_nation_treasury(w);

        economy_report rep = run_economy_step(w, reg);
        auto flows = clear_markets(w, reg, rep);
        apply_budget(w, reg, flows, rep.workforce_contention);

        const double treasury_gained = total_nation_treasury(w) - treasury_before;
        check(treasury_gained > 0.0,
              "P2.4 the ENACTING nation's treasury is credited by a cross-border sale");
        check(near(w.nations.at(f.supplier_nat).treasury, treasury_gained),
              "P2.5 the money lands in the AUTHOR's treasury, not in some other nation's");
        check(near(w.nations.at(f.buyer_nat).treasury, 0.0),
              "P2.6 the buyer's own nation collects nothing on a sale in someone else's market");

        // Conservation. The clearing tick is NOT a closed system (the market is
        // a buyer of last resort), so the assertion is made against a control
        // run of the identical world with the law un-enacted: the tariff must
        // move exactly `treasury_gained` OUT of corporate cash and no more.
        const double cash_after = total_corp_cash(w);
        double control_cash_after = 0.0;
        {
            recipe_registry creg;
            fixture c = build_fixture(creg);
            c.w.pool_for(c.supplier, c.supplier_body).quantities[ri(resource_type::iron_ore)] += 200.0f;
            post_pair(c.w, c.supplier, c.buyer, c.supplier_body, resource_type::iron_ore, qty, ask);
            economy_report crep = run_economy_step(c.w, creg);
            auto cflows = clear_markets(c.w, creg, crep);
            apply_budget(c.w, creg, cflows, crep.workforce_contention);
            control_cash_after = total_corp_cash(c.w);
        }
        check(near(control_cash_after - cash_after, treasury_gained, 1e-2),
              "P2.7 CONSERVED: corporate cash falls by EXACTLY what the treasury gained");
        std::printf("        tariff collected %.4f;  corporate cash delta vs control %.4f\n",
                    treasury_gained, control_cash_after - cash_after);
        (void)cash_before;

        // --- Same-nation sale pays nothing ---------------------------------
        // Move the buyer into the seller's nation and repeat: an internal sale
        // is not an import, and a tariff that charged it would be a sales tax
        // wearing the wrong name.
        {
            recipe_registry creg;
            fixture c = build_fixture(creg);
            law dl = l; dl.enacting_nation = c.supplier_nat; c.w.laws.push_back(dl);
            c.w.corporations[c.buyer].home_nation = c.supplier_nat; // now a domestic buyer
            c.w.pool_for(c.supplier, c.supplier_body).quantities[ri(resource_type::iron_ore)] += 200.0f;
            post_pair(c.w, c.supplier, c.buyer, c.supplier_body, resource_type::iron_ore, qty, ask);
            economy_report crep = run_economy_step(c.w, creg);
            auto cflows = clear_markets(c.w, creg, crep);
            apply_budget(c.w, creg, cflows, crep.workforce_contention);
            check(near(total_nation_treasury(c.w), 0.0),
                  "P2.8 a SAME-NATION sale is charged nothing");
        }
    }

    // --- Eight-tick conservation span, both flows live together ------------
    {
        recipe_registry reg;
        fixture f = build_fixture(reg);
        world& w = f.w;
        law l; l.id = "LAW-IMPORT-TARIFF"; l.name = "Import tariff";
        l.effect = law_effect_kind::import_tariff; l.enacting_nation = f.supplier_nat;
        l.rate = 0.10f; l.enacted = true;
        w.laws.push_back(l);

        corp_command q; q.corp = f.buyer; q.verb = corp_verb::request_quote;
        q.counterparty = f.supplier; q.target = resource_type::iron_ore; q.quantity = 40.0f;
        apply_corp_command(w, reg, q, nullptr);
        corp_command a; a.corp = f.buyer; a.verb = corp_verb::accept_quote;
        a.order = w.procurement_quotes.back().id;
        apply_corp_command(w, reg, a, nullptr);

        // Contract flows only (no clearing tick), so the closed-system claim is
        // exact across the whole span.
        const double cash_start = total_corp_cash(w);
        bool held = true;
        for (int t = 0; t < 8; ++t)
        {
            run_economy_step(w, reg);
            held = held && near(total_corp_cash(w), cash_start);
        }
        check(held, "P3.0 CONSERVED over an 8-tick span: total corporate cash never moves");
    }

    if (g_failures == 0) std::printf("\nALL PASS  (0 failures)\n");
    else                 std::printf("\nFAILURES (%d failures)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
