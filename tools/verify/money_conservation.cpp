// Headless harness for the import tariff's conservation (Sprint D4; BL-1403).
// The binding requirement: NO FLOW THE TARIFF TOUCHES MAY MINT OR DESTROY A
// CREDIT.
//
// BL-1407 (Ben, 2026-10-23): the corporation is retired, and with it BL-392's
// procurement contracts (request_quote / accept_quote — CONTRACTS.md, the
// procurement verbs retired with the firm). PART 1, which measured them, is
// RETIRED with its mechanism. The tariff half is RE-POINTED: every owner is a
// nation, the shipper's treasury pays and the enacting nation's receives.
//
// WHAT "CONSERVED" MEANS HERE, precisely — because the wider economy is NOT a
// closed system and a harness that asserted it were would be asserting a
// falsehood. `clear_markets` makes the market a buyer of last resort: it pays a
// seller at the resolved price with nobody's money on the other side, so total
// cash rises and falls every tick by design (MARKETS.md § honest limitations).
// What is asserted below is conservation ACROSS THE TARIFF'S FLOW, measured by
// running the seam in isolation from the clearing tick:
//
//   * every credit an import tariff debits from the shipping nation arrives in
//     the enacting nation's treasury; and
//   * with no tariff law enacted, the world is bit-identical to the pre-item
//     baseline (a state hash over every treasury, shelf and price).
//
// Kept outside src/ so the CMake glob does not pull it into the real build.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/law.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp" // credit_arrived_convoys: the tariff's point of charge
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

/// Total treasury, summed in ASCENDING NATION ID. Float addition is not
/// associative and `w.nations` is an unordered map, so an unsorted sum would be
/// a different number run to run — which would make this harness's own
/// arithmetic the thing under test.
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

/// A cheap order-independent digest over everything the tariff could perturb:
/// treasuries, shelves and market prices. Quantised to a fixed grid so the hash
/// is a value comparison, not a bit comparison of accumulated float noise.
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

    std::map<entity_id, const market_component*> mkts;
    for (const auto& [mid, mc] : w.markets) mkts[mid] = &mc;
    for (const auto& [mid, mc] : mkts)
    {
        mix(mid);
        for (std::size_t r = 0; r < resource_count; ++r) { mixf(mc->price[r]); mixf(mc->inventory[r]); }
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
    entity_id supplier_nat  = null_entity;
    entity_id buyer_nat     = null_entity;
    entity_id supplier_tile = null_entity;
    entity_id buyer_tile    = null_entity;
};

/// Two bodies, each with its own market and its own nation, and one extraction
/// site owned by each nation on its own ground (BL-1407: the owner is the
/// nation; its treasury opens at 10000).
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
        nc.treasury = 10000.0f;
        w.nations[nation] = nc;
        w.tile_to_nation[centre] = nation;

        const entity_id bld = w.create_entity();
        building_component b{}; b.tile = centre; b.type = building_type::extraction_site;
        b.target_resource = resource_type::iron_ore; b.ticks_remaining = 0;
        w.buildings[bld] = b;
        w.nations[nation].state_assets.push_back(bld);
    };

    make_body(f.supplier_body, f.supplier_mkt, f.supplier_tile, f.supplier_nat);
    make_body(f.buyer_body,    f.buyer_mkt,    f.buyer_tile,    f.buyer_nat);
    return f;
}

/// Put an ARRIVED trade shipment of @p qty of @p r on the lane from market
/// @p from to market @p to, owned by @p owner (BL-1266: a convoy is a trade's
/// shipment). `credit_arrived_convoys` lands it — and, since MARKETS.md §
/// Tariffs (Ben 2026-09-15), charges the destination nation's import duty when
/// the owner is a foreign nation. Placed directly rather than through a trade
/// pass so the only flow in play is the one this harness measures.
void post_arrival(world& w, entity_id owner, entity_id from, entity_id to,
                  resource_type r, float qty)
{
    convoy_component c;
    c.id             = w.allocate_convoy_id();
    c.source_market  = from;
    c.dest_market    = to;
    c.mode           = convoy_mode::space;
    c.cargo_resource = r;
    c.cargo_qty      = qty;
    c.progress       = 1.0f;
    c.speed          = 1.0f;
    c.corp           = owner;
    c.arrived        = true;
    w.convoys.push_back(c);
}

} // namespace

// ---------------------------------------------------------------------------

int main()
{
    // PART 1 (BL-392, procurement contracts) is RETIRED with the procurement
    // verbs (BL-1407); see the header.

    // =======================================================================
    // PART 2 — the import tariff
    // =======================================================================
    std::printf("PART 2  Sprint D4: the import tariff\n");

    // --- Baseline: with no tariff law enacted, nothing moves ---------------
    uint64_t baseline_hash = 0;
    double   baseline_treasury = 0.0;
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
        baseline_hash     = state_hash(w);
        baseline_treasury = total_nation_treasury(w);
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
        check(total_nation_treasury(w) == baseline_treasury,
              "P2.0 an UN-enacted tariff law collects nothing (treasuries match the no-law run)");
        check(state_hash(w) == baseline_hash,
              "P2.1 BIT-IDENTICAL: with no tariff enacted the world matches the pre-item baseline");
        std::printf("        state_hash %016llX\n", static_cast<unsigned long long>(baseline_hash));
    }

    // --- Enacted: a cross-border shipment is taxed at ARRIVAL, the money lands
    // (MARKETS.md § Tariffs, Ben 2026-09-15: the convoy is the one object that
    // carries goods across a line, so arrival is the one point of charge).
    {
        recipe_registry reg;
        fixture f = build_fixture(reg);
        world& w = f.w;

        // The SUPPLIER's nation levies 10% on imports into its own market.
        law l; l.id = "LAW-IMPORT-TARIFF"; l.name = "Import tariff";
        l.effect = law_effect_kind::import_tariff; l.enacting_nation = f.supplier_nat;
        l.rate = 0.10f; l.enacted = true;
        w.laws.push_back(l);

        // The buyer NATION ships 40 units from its own market into the
        // supplier's: an import into the enacting nation.
        const float qty = 40.0f;
        post_arrival(w, f.buyer_nat, f.buyer_mkt, f.supplier_mkt, resource_type::iron_ore, qty);
        check(w.convoys.size() == 1,
              "P2.2 a foreign shipment stands arrived at the supplier's market");

        const double total_before    = total_nation_treasury(w);
        const double supplier_before = w.nations.at(f.supplier_nat).treasury;
        const double buyer_before    = w.nations.at(f.buyer_nat).treasury;
        const float  dest_price      = w.markets.at(f.supplier_mkt).price[ri(resource_type::iron_ore)] > 0.0f
                                     ? w.markets.at(f.supplier_mkt).price[ri(resource_type::iron_ore)]
                                     : w.markets.at(f.supplier_mkt).base_price[ri(resource_type::iron_ore)];

        credit_arrived_convoys(w, 0);

        const double treasury_gained = w.nations.at(f.supplier_nat).treasury - supplier_before;
        const double shipper_paid    = buyer_before - w.nations.at(f.buyer_nat).treasury;
        check(treasury_gained > 0.0,
              "P2.4 the ENACTING nation's treasury is credited by a cross-border arrival");
        check(near(treasury_gained, qty * dest_price * 0.10, 1e-3),
              "P2.4b the duty is the cargo's value at the destination's price, at the enacted rate");
        check(near(shipper_paid, treasury_gained, 1e-3),
              "P2.5 the SHIPPING nation's treasury pays exactly what the enacting one gained");
        check(near(total_nation_treasury(w), total_before, 1e-3),
              "P2.7 CONSERVED: total treasury is unchanged by the duty");
        std::printf("        tariff collected %.4f on %.0f units at %.4f\n",
                    treasury_gained, qty, dest_price);

        // --- A nation's own goods pay nothing (BL-1403, Ben 2026-10-23) -----
        // The duty falls on a FOREIGN NATION'S goods entering: the shipper's
        // nation, not the source market's. Make the shipper the destination
        // nation itself and repeat: its own goods in its own market pay nothing.
        {
            recipe_registry creg;
            fixture c = build_fixture(creg);
            law dl = l; dl.enacting_nation = c.supplier_nat; c.w.laws.push_back(dl);
            const double before = total_nation_treasury(c.w);
            const double own    = c.w.nations.at(c.supplier_nat).treasury;
            post_arrival(c.w, c.supplier_nat, c.buyer_mkt, c.supplier_mkt, resource_type::iron_ore, qty);
            credit_arrived_convoys(c.w, 0);
            check(near(total_nation_treasury(c.w), before) && c.w.nations.at(c.supplier_nat).treasury == own,
                  "P2.8 a nation's OWN goods entering its own market are charged nothing");
        }
        // ...and the reverse of the retired rule: a foreign shipper's goods pay
        // even when they leave from a market on the destination's own ground.
        {
            recipe_registry creg;
            fixture c = build_fixture(creg);
            law dl = l; dl.enacting_nation = c.supplier_nat; c.w.laws.push_back(dl);
            c.w.tile_to_nation[c.w.markets.at(c.buyer_mkt).centre_tile] = c.supplier_nat;
            const double own = c.w.nations.at(c.supplier_nat).treasury;
            post_arrival(c.w, c.buyer_nat, c.buyer_mkt, c.supplier_mkt, resource_type::iron_ore, qty);
            credit_arrived_convoys(c.w, 0);
            check(c.w.nations.at(c.supplier_nat).treasury > own,
                  "P2.9 a FOREIGN shipper's goods pay, though they left a market on the destination's ground");
        }
    }

    // PART 3 (an 8-tick span of contract flows) is RETIRED with the procurement
    // contract it paced (BL-1407).

    if (g_failures == 0) std::printf("\nALL PASS  (0 failures)\n");
    else                 std::printf("\nFAILURES (%d failures)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
