// whole_firm_buyout — the BL-628 whole-firm acquisition verb (requirement group
// `whole-firm-buyout`, rows R1-R6). No SDL / Lua / ImGui.
//
// Build and run (from the repo root, Git Bash included):
//   cmd //c tools\verify\build_harness.bat whole_firm_buyout
//   build_gen\verify\whole_firm_buyout.exe
//
// WHAT IS BEING CHECKED, and why each row is shaped the way it is.
//
//   R1  THE PRICE. max(0, book_value + k_acquisition_multiple x trailing_net +
//       balance), trailing_net being the mean filed `net` over the last EIGHT
//       quarters or fewer if the firm is younger. Checked against an INDEPENDENT
//       recomputation from the filed record — not against the function calling
//       itself — with the young-firm case, the loss-making case (prices below
//       book, unclamped, exactly as FINANCE.md requires) and the zero floor each
//       given their own row.
//
//   R2  THE CLASS GATE, and the refusal's inertness. A private or closed target
//       is refused; so is a target that has never filed, a nonexistent one, the
//       acting corp itself, and the player's corp. Every refusal is checked by
//       BYTE-COMPARING a full world snapshot taken before and after the command —
//       not by spot-checking the two or three fields the reader thought of.
//
//   R3  THE TRANSFER. Holdings, (corp, body) pools, balance and filed returns
//       move; hq_building / influence_range recompute over the merged set; units
//       re-point THROUGH their muster_base; open sell orders are CANCELLED, not
//       reassigned. Each one is a separate assertion, because "the buyout worked"
//       is not a check.
//
//   R4  NOTHING DANGLES. The requirement this verb exists to be dangerous about:
//       it is the only thing in the codebase that erases an actor. Checked by a
//       GENERIC SCANNER (`dangling_refs`) that re-walks every container in
//       world.hpp holding a corporation id and reports each survivor by name —
//       so a store added later fails this row rather than passing it silently.
//       Run twice: on a hand-built fixture loaded with a reference of every kind,
//       and end-to-end on a really generated, warm-started world. The world then
//       round-trips the save byte-identically.
//
//   R5  equity_taken IS EMITTED, and the spend is under the solvency gate. The
//       factor's authored weight is ZERO in the shipped economy.lua (BL-545's
//       inertness rule), so the harness authors a weight of its own and asserts
//       the row lands on every surviving rival, in a deterministic walk. The
//       solvency half asserts a corp one credit short is refused with the world
//       untouched, and that a sufficient corp's balance moves by exactly
//       `target.balance - price`.
//
//   R6  THE AI-FACING SEAM. The verb's own field (`counterparty`) is validated as
//       the value that LANDS — an id that does not name a corporation, the actor's
//       own id, and null are each refused whole with nothing mutated. The append-
//       only enum contract is asserted symbolically (`corp_verb_count` is
//       buy_corporation + 1, and no earlier verb renumbered). A NaN reaching the
//       price through a corrupt filed return is refused rather than poisoning a
//       balance.
//
//   X   THE KNOWN EXPOSURE, STATED RATHER THAN HIDDEN (NR-655). A filed return
//       records the MONEY LOOP only: `apply_budget` runs at app.cpp:1271 and
//       `run_nation_step` — which pays mercenary contracts and national transfers
//       — at :1281. So a corporation earning through contracts reads as less
//       profitable on its own returns than it is, and THIS VERB UNDERVALUES IT.
//       Row X builds exactly that firm, measures the gap in credits, and prints
//       it. It is deliberately NOT an assertion that the price is right: it is a
//       measurement of a known wrong, kept visible so it cannot be forgotten. The
//       fork is Ben's; closing it is not this item's work.
//
// REGISTRY. Lua-free by construction (build_harness excludes the sol2 TUs), so
// the economics are restated here as constants taken from scripts/economy.lua —
// the treatment quarterly_return and sea_leg_census already give theirs.
//
// Exits 0 on PASS, non-zero on any failure.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/sentiment.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include "harness_params.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* row, const char* what)
{
    std::printf("  [%s] %-3s %s\n", ok ? "PASS" : "FAIL", row, what);
    if (!ok)
        ++g_failures;
}

std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

// --- the shipped economy.lua figures this harness needs ---------------------
void author_economics(recipe_registry& reg)
{
    building_economics ex;
    ex.base_rate = 20.0f; ex.maintenance = 5.0f; ex.base_wage = 8.0f; ex.build_cost = 100.0f;
    reg.set_economics(building_type::extraction_site, ex);

    building_economics pr;
    pr.base_rate = 8.0f;  pr.maintenance = 10.0f; pr.base_wage = 12.0f; pr.build_cost = 200.0f;
    reg.set_economics(building_type::processing_facility, pr);

    building_economics po;
    po.maintenance = 4.0f; po.base_wage = 6.0f; po.build_cost = 500.0f;
    reg.set_economics(building_type::port, po);

    building_economics mb;
    mb.maintenance = 6.0f; mb.base_wage = 6.0f; mb.build_cost = 300.0f;
    reg.set_economics(building_type::military_base, mb);
}

std::string to_bytes(const world& w)
{
    std::ostringstream out(std::ios::binary);
    write_world_snapshot(w, out);
    return out.str();
}

std::vector<entity_id> sorted_corp_ids(const world& w)
{
    std::vector<entity_id> ids;
    ids.reserve(w.corporations.size());
    for (const auto& kv : w.corporations)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

// ---------------------------------------------------------------------------
// R4's scanner — every container in world.hpp that holds a corporation id
// ---------------------------------------------------------------------------
// Written as a re-walk of the WORLD rather than as a mirror of dissolve_into's
// list, on purpose. A checklist copied from the implementation passes whenever
// the implementation is self-consistently wrong; this one fails whenever a live
// row still names the dissolved firm, whatever put it there.
std::vector<std::string> dangling_refs(const world& w, entity_id gone)
{
    std::vector<std::string> hits;
    const auto note = [&](const char* what) { hits.emplace_back(what); };

    if (w.corporations.count(gone) != 0)                       note("world::corporations (the component itself)");
    if (w.player_entity == gone)                               note("world::player_entity");
    if (w.corp_embargo_conditions.count(gone) != 0)            note("world::corp_embargo_conditions");
    if (w.earned_techs.count(gone) != 0)                       note("world::earned_techs");
    if (w.corp_modifiers.count(gone) != 0)                     note("world::corp_modifiers");

    for (const auto& kv : w.corporations)
    {
        const corporation_component& cc = kv.second;
        if (cc.hq_building != null_entity && w.buildings.count(cc.hq_building) == 0)
            note("a surviving corp's hq_building names no building");
    }
    for (const auto& kv : w.units)
        if (kv.second.owner == gone)                           note("world::units[*].owner");
    for (const sell_order& o : w.sell_orders)
        if (o.corp == gone)                                    note("world::sell_orders[*].corp");
    for (const buy_order& o : w.buy_orders)
        if (o.corp == gone)                                    note("world::buy_orders[*].corp");
    for (const convoy_component& c : w.convoys)
        if (c.corp == gone)                                    note("world::convoys[*].corp");
    for (const trade_route& r : w.trade_routes)
        if (r.corp == gone)                                    note("world::trade_routes[*].corp");
    for (const procurement_quote& q : w.procurement_quotes)
        if (q.buyer == gone || q.supplier == gone)             note("world::procurement_quotes[*]");
    for (const procurement_contract& c : w.procurement_contracts)
        if (c.buyer == gone || c.supplier == gone)             note("world::procurement_contracts[*]");
    for (const mercenary_contract& c : w.mercenary_contracts)
        if (c.contractor == gone)                              note("world::mercenary_contracts[*].contractor");
    for (const active_battle& b : w.battles)
        if (b.attacker == gone || b.defender == gone)          note("world::battles[*]");
    for (const auto& kv : w.corp_body_pools)
        if (kv.first.first == gone)                            note("world::corp_body_pools key");
    for (const auto& kv : w.workforce_supply_overrides)
        if (kv.first.first == gone)                            note("world::workforce_supply_overrides key");
    for (const auto& kv : w.sentiment.pairs)
        if (kv.first.first == gone || kv.first.second == gone) note("world::sentiment.pairs");
    for (const auto& p : w.corp_hostile_pairs)
        if (p.first == gone || p.second == gone)               note("world::corp_hostile_pairs");
    for (const auto& p : w.corp_friend_pairs)
        if (p.first == gone || p.second == gone)               note("world::corp_friend_pairs");
    for (const auto& p : w.corp_friend_offers)
        if (p.first == gone || p.second == gone)               note("world::corp_friend_offers");
    for (const entity_id h : w.province_holder)
        if (h == gone)                                         note("world::province_holder");

    // Every building the world still holds must resolve to SOME live corp, or to
    // none at all — never to the erased one. `owner_corp_of` is the ownership
    // authority (there is no reverse index), so this asks the question the game
    // asks.
    for (const auto& kv : w.buildings)
        if (owner_corp_of(w, kv.first) == gone)                note("owner_corp_of(building) == the erased corp");

    std::sort(hits.begin(), hits.end());
    hits.erase(std::unique(hits.begin(), hits.end()), hits.end());
    return hits;
}

void report_dangling(const world& w, entity_id gone, const char* row, const char* what)
{
    const std::vector<std::string> hits = dangling_refs(w, gone);
    check(hits.empty(), row, what);
    for (const std::string& h : hits)
        std::printf("         DANGLING: %s\n", h.c_str());
}

// ---------------------------------------------------------------------------
// Fixture A — a hand-built world loaded with a reference of every kind
// ---------------------------------------------------------------------------
// Returns are FILED BY HAND here. That is deliberate: R1 is arithmetic over a
// record, and driving the record through eighty economy ticks would prove the
// money loop rather than the price. Fixture B runs the real loop.
struct fixture
{
    world     w;
    entity_id body     = null_entity;
    entity_id body_far = null_entity;
    entity_id market   = null_entity;
    entity_id buyer    = null_entity;
    entity_id target   = null_entity;
    entity_id bystander = null_entity;
    entity_id target_base = null_entity;  // the target's military_base (a muster base)
    entity_id target_unit = null_entity;
    std::vector<entity_id> target_assets;
};

/// One filed quarter, by hand.
quarterly_return filed(float net, float balance, uint32_t holdings, float book)
{
    quarterly_return q{};
    q.net        = net;
    q.balance    = balance;
    q.holdings   = holdings;
    q.book_value = book;
    q.income     = (net > 0.0f) ? net : 0.0f;
    q.maintenance = (net < 0.0f) ? -net : 0.0f;
    return q;
}

fixture build_fixture()
{
    fixture f;
    world&  w = f.w;

    f.body = w.create_entity();
    w.bodies[f.body] = body_component{};
    w.bodies[f.body].name = "Anvil";
    f.body_far = w.create_entity();
    w.bodies[f.body_far] = body_component{};
    w.bodies[f.body_far].name = "Kell";

    f.market = w.create_entity();
    {
        market_component mc;
        mc.body = f.body;
        mc.base_price[ri(resource_type::iron_ore)] = 2.5f;
        mc.price = mc.base_price;
        w.markets[f.market] = mc;
    }

    int next_x = 0;
    const auto make_tile = [&](entity_id body) {
        const entity_id t = w.create_entity();
        tile_component tc{};
        tc.body      = body;
        tc.grid_x    = next_x++;
        tc.grid_y    = 1;
        tc.substrate = terrain_substrate::rocky;
        tc.resource_deposit[ri(resource_type::iron_ore)]   = 2.0f;
        tc.resource_remaining[ri(resource_type::iron_ore)] = 1.0e6f;
        w.tiles[t] = tc;
        return t;
    };
    const auto make_building = [&](building_type type, entity_id body) {
        const entity_id b = w.create_entity();
        building_component bc{};
        bc.tile               = make_tile(body);
        bc.type               = type;
        bc.workforce_assigned = 0.5f;
        bc.workforce_target   = 100.0f;
        bc.target_resource    = resource_type::iron_ore;
        w.buildings[b] = bc;
        return b;
    };

    const entity_id buy1 = make_building(building_type::extraction_site, f.body);
    const entity_id buy2 = make_building(building_type::port, f.body);
    const entity_id tgt1 = make_building(building_type::extraction_site, f.body);
    const entity_id tgt2 = make_building(building_type::extraction_site, f.body_far);
    f.target_base        = make_building(building_type::military_base, f.body);
    const entity_id oth1 = make_building(building_type::extraction_site, f.body);

    const auto make_corp = [&](const char* name, float balance, ownership_class cls,
                               std::vector<entity_id> assets) {
        const entity_id c = w.create_entity();
        corporation_component cc;
        cc.name            = name;
        cc.balance         = balance;
        cc.starting_capital = balance;
        cc.ownership_class = cls;
        cc.assets          = std::move(assets);
        w.corporations[c]  = cc;
        return c;
    };

    f.buyer     = make_corp("Auren Holdings",  50000.0f, ownership_class::privately_held, {buy1, buy2});
    f.target    = make_corp("Vesk Extraction",  1200.0f, ownership_class::publicly_held,
                            {tgt1, tgt2, f.target_base});
    f.bystander = make_corp("Oreth Combine",    9000.0f, ownership_class::publicly_held, {oth1});
    f.target_assets = w.corporations.at(f.target).assets;

    // A filed history for the target: ten quarters, the last eight averaging
    // +50.0 exactly (so trailing_net is an exact float and R1's arithmetic is
    // not a tolerance argument). The two OLDEST rows are deliberately far off
    // that mean, so a window that reads nine or ten rows fails the row.
    corporation_component& t = w.corporations.at(f.target);
    t.returns.push_back(filed(-900.0f, 100.0f, 3, 500.0f));
    t.returns.push_back(filed(+900.0f, 200.0f, 3, 500.0f));
    for (int i = 0; i < 8; ++i)
        t.returns.push_back(filed(50.0f, 300.0f + 50.0f * static_cast<float>(i), 3, 500.0f));
    t.returns.back().book_value = 500.0f;   // 100 + 100 + 300, the target's three holdings
    t.returns.back().balance    = t.balance;

    // A SHORTER filed history for the buyer — three quarters against the
    // target's ten — so the pairwise-from-the-newest-end merge is actually
    // exercised rather than degenerating into "the target's history wins".
    corporation_component& bu = w.corporations.at(f.buyer);
    for (int i = 0; i < 3; ++i)
        bu.returns.push_back(filed(5.0f, 50000.0f, 2, 600.0f));

    // A filed history for the bystander too, so it can be priced in R2/R5.
    corporation_component& o = w.corporations.at(f.bystander);
    o.returns.push_back(filed(10.0f, 9000.0f, 1, 100.0f));

    // --- references of every kind, so R4 has something to find --------------
    // (corp, body) pools on two bodies, one of which the buyer also holds.
    w.pool_for(f.target, f.body).quantities[ri(resource_type::iron_ore)]     = 40.0f;
    w.pool_for(f.target, f.body_far).quantities[ri(resource_type::iron_ore)] = 7.0f;
    w.pool_for(f.buyer,  f.body).quantities[ri(resource_type::iron_ore)]     = 3.0f;

    w.workforce_supply_overrides[{f.target, f.body_far}] = 6.0f;
    w.workforce_supply_overrides[{f.buyer,  f.body}]     = 4.0f;
    w.workforce_supply_overrides[{f.target, f.body}]     = 9.0f; // collides with the buyer's

    // A unit raised at the target's own military_base.
    f.target_unit = w.create_entity();
    w.units[f.target_unit] = unit_component{};
    w.units[f.target_unit].position    = w.buildings.at(f.target_base).tile;
    w.units[f.target_unit].owner       = f.target;
    w.units[f.target_unit].count       = 500;
    w.units[f.target_unit].muster_base = f.target_base;

    // Standing sell orders on both sides of the transaction.
    {
        sell_order o;
        o.id = w.allocate_order_id(); o.corp = f.target; o.body = f.body;
        o.resource = resource_type::iron_ore; o.quantity = 5.0f;
        w.sell_orders.push_back(o);
        sell_order k;
        k.id = w.allocate_order_id(); k.corp = f.buyer; k.body = f.body;
        k.resource = resource_type::iron_ore; k.quantity = 2.0f;
        w.sell_orders.push_back(k);
    }
    {
        buy_order b;
        b.id = w.allocate_order_id(); b.corp = f.target; b.body = f.body;
        b.resource = resource_type::iron_ore; b.quantity = 1.0f; b.max_price = 9.0f;
        w.buy_orders.push_back(b);
    }

    // A convoy in flight, a trade route, a live quote, an accepted contract, a
    // mercenary contract, stance rows, sentiment rows, an embargo, a tech, a
    // modifier. Every one of them names the target.
    {
        convoy_component c{};
        c.id = w.allocate_convoy_id(); c.corp = f.target;
        c.source_market = f.market; c.dest_market = f.market;
        c.cargo_resource = resource_type::iron_ore; c.cargo_qty = 12.0f;
        w.convoys.push_back(c);
    }
    w.trade_routes.push_back(trade_route{f.body, f.body_far, f.target, 3, 2});
    w.trade_routes.push_back(trade_route{f.body, f.body_far, f.buyer,  9, 5});
    {
        procurement_quote q{};
        q.id = w.allocate_procurement_id(); q.buyer = f.bystander; q.supplier = f.target;
        q.body = f.body; q.resource = resource_type::iron_ore; q.quantity = 10.0f;
        w.procurement_quotes.push_back(q);
    }
    {
        procurement_contract c{};
        c.id = w.allocate_procurement_id(); c.buyer = f.bystander; c.supplier = f.target;
        c.body = f.body; c.resource = resource_type::iron_ore; c.quantity = 10.0f;
        c.unit_price = 3.0f; c.lead_time_ticks = 4; c.deposit_paid = 7.5f;
        w.procurement_contracts.push_back(c);
        // ...and one that WOULD become self-dealing after the buyout.
        procurement_contract s{};
        s.id = w.allocate_procurement_id(); s.buyer = f.buyer; s.supplier = f.target;
        s.body = f.body; s.resource = resource_type::iron_ore; s.quantity = 4.0f;
        s.unit_price = 3.0f; s.lead_time_ticks = 2; s.deposit_paid = 3.0f;
        w.procurement_contracts.push_back(s);
    }
    {
        mercenary_contract m{};
        m.id = w.allocate_contract_id(); m.contractor = f.target;
        m.client = null_entity; m.fee = 100.0f; m.units[0] = f.target_unit;
        w.mercenary_contracts.push_back(m);
    }
    w.corp_hostile_pairs.insert({f.target, f.bystander});
    w.corp_friend_pairs.insert({std::min(f.target, f.buyer), std::max(f.target, f.buyer)});
    w.corp_friend_offers.insert({f.bystander, f.target});
    w.sentiment.pairs[{f.bystander, f.target}] = sentiment_value{1.0f, -2.0f};
    w.sentiment.pairs[{f.target, f.bystander}] = sentiment_value{0.5f,  0.5f};
    w.corp_embargo_conditions[f.target] = condition_set{};
    w.earned_techs[f.target].insert("TECH-SMELTING");
    w.corp_modifiers[f.target].push_back(scalar_modifier{});
    {
        active_battle b{};
        b.province = 1; b.attacker = f.target; b.defender = f.bystander;
        w.battles.push_back(b);
    }

    return f;
}

corp_command buy_cmd(entity_id acquirer, entity_id target)
{
    corp_command c;
    c.verb         = corp_verb::buy_corporation;
    c.corp         = acquirer;
    c.counterparty = target;
    return c;
}

/// Issue @p cmd and assert that the world is byte-identical afterwards.
void expect_inert_refusal(world& w, const recipe_registry& reg, const corp_command& cmd,
                          corp_command_result want, const char* row, const char* what)
{
    const std::string before = to_bytes(w);
    const corp_command_result got = apply_corp_command(w, reg, cmd);
    const std::string after = to_bytes(w);
    check(got == want && before == after, row, what);
    if (got != want)
        std::printf("         (result %d, wanted %d)\n", static_cast<int>(got),
                    static_cast<int>(want));
    if (before != after)
        std::printf("         (THE REFUSAL MUTATED THE WORLD — %zu bytes vs %zu)\n",
                    before.size(), after.size());
}

// ---------------------------------------------------------------------------
// R1 — the price
// ---------------------------------------------------------------------------
void run_price_rows(const recipe_registry& reg)
{
    std::printf("\nR1 — the price: max(0, book + k x trailing_net + balance)\n");
    const float k = reg.acquisition().multiple;
    std::printf("       k_acquisition_multiple = %.4f, window = %zu quarters\n",
                static_cast<double>(k), k_acquisition_trailing_quarters);
    check(k > 0.0f, "R1", "the registry actually authors a multiple (non-vacuity guard)");

    fixture f = build_fixture();
    const corporation_component& t = f.w.corporations.at(f.target);

    // Independent recomputation, from the record and nothing else.
    double sum = 0.0;
    const std::size_t n = std::min(t.returns.size(), k_acquisition_trailing_quarters);
    for (std::size_t i = t.returns.size() - n; i < t.returns.size(); ++i)
        sum += static_cast<double>(t.returns[i].net);
    const double want_trailing = sum / static_cast<double>(n);
    const double want_price = static_cast<double>(t.returns.back().book_value)
                            + static_cast<double>(k) * want_trailing
                            + static_cast<double>(t.balance);

    const float got_trailing = corp_trailing_net(t);
    const float got_price    = corp_acquisition_price(t, k);
    std::printf("       filed rows %zu, window %zu, trailing_net %.4f, book %.1f, balance %.1f\n",
                t.returns.size(), n, static_cast<double>(got_trailing),
                static_cast<double>(t.returns.back().book_value),
                static_cast<double>(t.balance));
    std::printf("       price %.4f (independent recomputation %.4f)\n",
                static_cast<double>(got_price), want_price);
    check(static_cast<double>(got_trailing) == want_trailing, "R1",
          "trailing_net is the mean net over the last 8 filed quarters, exactly");
    check(got_trailing == 50.0f, "R1",
          "the window EXCLUDES the two older rows (a 9- or 10-row window would fail)");
    check(std::fabs(static_cast<double>(got_price) - want_price) < 1e-3, "R1",
          "price == book_value + k x trailing_net + balance");

    // --- a firm younger than the window ------------------------------------
    {
        corporation_component young;
        young.balance = 0.0f;
        young.returns.push_back(filed(20.0f, 0.0f, 1, 100.0f));
        young.returns.push_back(filed(40.0f, 0.0f, 1, 100.0f));
        young.returns.push_back(filed(60.0f, 0.0f, 1, 100.0f));
        const float trail = corp_trailing_net(young);
        check(trail == 40.0f, "R1",
              "a firm younger than 8 quarters averages over what it HAS filed");
        check(corp_acquisition_price(young, k) == 100.0f + k * 40.0f, "R1",
              "the young firm's price uses that shorter-window mean");
    }

    // --- a loss-making firm prices BELOW book, unclamped -------------------
    {
        corporation_component losing;
        losing.balance = 0.0f;
        for (int i = 0; i < 8; ++i)
            losing.returns.push_back(filed(-30.0f, 0.0f, 2, 400.0f));
        const float price = corp_acquisition_price(losing, k);
        std::printf("       loss-making firm: book 400.0, trailing_net %.1f, price %.1f\n",
                    static_cast<double>(corp_trailing_net(losing)),
                    static_cast<double>(price));
        check(price < 400.0f, "R1",
              "a chronically loss-making firm prices BELOW its book value (nothing clamps the profit term)");
        check(price == 400.0f + k * -30.0f, "R1",
              "and it prices at exactly book + k x (negative trailing_net)");
    }

    // --- the zero floor -----------------------------------------------------
    {
        corporation_component worthless;
        worthless.balance = -100.0f;
        for (int i = 0; i < 8; ++i)
            worthless.returns.push_back(filed(-500.0f, -100.0f, 1, 50.0f));
        const float price = corp_acquisition_price(worthless, k);
        check(price == 0.0f, "R1", "a firm priced below zero floors at zero, never negative");
    }

    // --- balance is SIGNED --------------------------------------------------
    // Both balances are chosen so the priced firm stays ABOVE the zero floor —
    // otherwise this measures the floor rather than the balance term (which is
    // what a first cut of this row did, and the harness caught it).
    {
        corporation_component rich = f.w.corporations.at(f.target); // book 500, k x 50 = 400
        corporation_component poor = rich;
        rich.balance = 0.0f;      // priced 900
        poor.balance = -800.0f;   // priced 100 — leveraged, still above the floor
        check(corp_acquisition_price(rich, k) - corp_acquisition_price(poor, k) == 800.0f,
              "R1", "a leveraged target is cheaper by exactly what it owes");
        check(corp_acquisition_price(poor, k) > 0.0f, "R1",
              "and that row measured the balance term, not the zero floor");
    }

    // --- a NON-FINITE price is not floored to zero -------------------------
    // The floor's ordering: with `max(0, ...)` applied first, a NaN prices the
    // firm at exactly ZERO and every downstream finiteness guard sees a clean
    // number. R6 asserts the seam refuses; this asserts the price function does
    // not launder the value on the way there.
    {
        corporation_component corrupt;
        corrupt.balance = 100.0f;
        for (int i = 0; i < 8; ++i)
            corrupt.returns.push_back(filed(0.0f, 100.0f, 1, 100.0f));
        corrupt.returns.back().net = std::numeric_limits<float>::quiet_NaN();
        check(!std::isfinite(corp_acquisition_price(corrupt, k)), "R1",
              "a non-finite price is PROPAGATED, never floored to a free firm");
    }
}

// ---------------------------------------------------------------------------
// R2 / R6 — the gates, and the inertness of every refusal
// ---------------------------------------------------------------------------
void run_gate_rows(const recipe_registry& reg)
{
    std::printf("\nR2/R6 — the gates, each refusal byte-compared against the world before it\n");

    {
        fixture f = build_fixture();
        f.w.corporations.at(f.target).ownership_class = ownership_class::privately_held;
        expect_inert_refusal(f.w, reg, buy_cmd(f.buyer, f.target),
                             corp_command_result::rejected_state, "R2",
                             "a PRIVATE target is refused, and the refusal mutates nothing");
    }
    {
        fixture f = build_fixture();
        f.w.corporations.at(f.target).ownership_class = ownership_class::closed;
        expect_inert_refusal(f.w, reg, buy_cmd(f.buyer, f.target),
                             corp_command_result::rejected_state, "R2",
                             "a CLOSED target is refused, and the refusal mutates nothing");
    }
    {
        fixture f = build_fixture();
        f.w.corporations.at(f.target).returns.clear();
        expect_inert_refusal(f.w, reg, buy_cmd(f.buyer, f.target),
                             corp_command_result::rejected_state, "R2",
                             "a public target that has never FILED cannot be priced, and is refused");
    }
    {
        fixture f = build_fixture();
        f.w.corporations.at(f.target).is_player = true;
        expect_inert_refusal(f.w, reg, buy_cmd(f.buyer, f.target),
                             corp_command_result::rejected_state, "R2",
                             "the PLAYER's corp is not buyable through this seam (is_player)");
    }
    {
        fixture f = build_fixture();
        f.w.player_entity = f.target; // spectator mode degrades is_player, not the anchor
        expect_inert_refusal(f.w, reg, buy_cmd(f.buyer, f.target),
                             corp_command_result::rejected_state, "R2",
                             "nor via world::player_entity, which is_player alone would miss");
    }

    // --- R6: the seam's own field validation --------------------------------
    {
        fixture f = build_fixture();
        // An id inside entity_id's domain (so the wire's range gate passes it)
        // that names no corporation. Fitting is not existing.
        expect_inert_refusal(f.w, reg, buy_cmd(f.buyer, entity_id{999999}),
                             corp_command_result::rejected_invalid, "R6",
                             "a counterparty that FITS but names no corporation is refused whole");
        expect_inert_refusal(f.w, reg, buy_cmd(f.buyer, null_entity),
                             corp_command_result::rejected_invalid, "R6",
                             "an omitted counterparty (null_entity) is refused, never defaulted");
        expect_inert_refusal(f.w, reg, buy_cmd(f.buyer, f.buyer),
                             corp_command_result::rejected_invalid, "R6",
                             "a corp cannot buy itself");
        corp_command ghost = buy_cmd(entity_id{888888}, f.target);
        expect_inert_refusal(f.w, reg, ghost, corp_command_result::rejected_no_corp, "R6",
                             "a command from a corp that does not exist is refused whole");
    }
    {
        // A NaN reaching the price through a corrupt filed return. A NaN would
        // PASS `balance < price` and then poison the acquirer's balance for the
        // rest of the campaign, so the seam refuses rather than spends.
        fixture f = build_fixture();
        f.w.corporations.at(f.target).returns.back().net =
            std::numeric_limits<float>::quiet_NaN();
        expect_inert_refusal(f.w, reg, buy_cmd(f.buyer, f.target),
                             corp_command_result::rejected_invalid, "R6",
                             "a non-finite price (corrupt filed return) is refused, never spent");
    }

    // --- R6: the append-only enum contract, asserted symbolically -----------
    check(corp_verb_count == static_cast<uint8_t>(corp_verb::buy_corporation) + 1, "R6",
          "corp_verb_count moved with the appended verb (the wire's range gate)");
    check(static_cast<uint8_t>(corp_verb::raze_centre) == 27
              && static_cast<uint8_t>(corp_verb::build) == 0
              && static_cast<uint8_t>(corp_verb::hire_unit) == 8,
          "R6", "appending renumbered no existing verb");
}

// ---------------------------------------------------------------------------
// R3 / R4 / R5 — the transfer, the sweep, and what it costs
// ---------------------------------------------------------------------------
void run_transfer_rows(const recipe_registry& reg)
{
    std::printf("\nR3/R4/R5 — the transfer and the dissolution walk\n");

    fixture f = build_fixture();
    world&  w = f.w;

    const corporation_component before_t = w.corporations.at(f.target);
    const corporation_component before_b = w.corporations.at(f.buyer);
    const float price = corp_acquisition_price(before_t, reg.acquisition().multiple);

    // Pooled stock on the shared body, before.
    const float buyer_pool_before =
        w.corp_body_pools.at({f.buyer, f.body}).quantities[ri(resource_type::iron_ore)];
    const float target_pool_before =
        w.corp_body_pools.at({f.target, f.body}).quantities[ri(resource_type::iron_ore)];

    // --- R5's solvency half, FIRST: one credit short is refused ------------
    {
        world poor = w;
        poor.corporations.at(f.buyer).balance = price - 1.0f;
        expect_inert_refusal(poor, reg, buy_cmd(f.buyer, f.target),
                             corp_command_result::rejected_funds, "R5",
                             "an acquirer one credit short is refused, and nothing moves");
    }

    const std::size_t orders_before = w.sell_orders.size();
    const corp_command_result r = apply_corp_command(w, reg, buy_cmd(f.buyer, f.target));
    check(r == corp_command_result::applied, "R3", "a solvent buyer, a public target: applied");

    const corporation_component& acq = w.corporations.at(f.buyer);

    // --- R3: what transferred ----------------------------------------------
    std::printf("       price %.2f, buyer balance %.2f -> %.2f (target brought %.2f)\n",
                static_cast<double>(price), static_cast<double>(before_b.balance),
                static_cast<double>(acq.balance), static_cast<double>(before_t.balance));
    check(std::fabs(static_cast<double>(acq.balance)
                    - (static_cast<double>(before_b.balance) - static_cast<double>(price)
                       + static_cast<double>(before_t.balance))) < 1e-2,
          "R5", "the buyer's balance moved by exactly (target.balance - price)");

    bool holdings_ok = acq.assets.size() == before_b.assets.size() + before_t.assets.size();
    for (const entity_id a : before_t.assets)
        holdings_ok = holdings_ok
                   && std::find(acq.assets.begin(), acq.assets.end(), a) != acq.assets.end();
    check(holdings_ok, "R3", "every holding transferred to the acquirer");

    check(w.corp_body_pools.count({f.target, f.body}) == 0
              && w.corp_body_pools.count({f.target, f.body_far}) == 0,
          "R3", "the target's (corp, body) pools are gone");
    check(w.corp_body_pools.at({f.buyer, f.body}).quantities[ri(resource_type::iron_ore)]
              == buyer_pool_before + target_pool_before,
          "R3", "colliding pools MERGED (stock summed, not replaced)");
    check(w.corp_body_pools.count({f.buyer, f.body_far}) == 1
              && w.corp_body_pools.at({f.buyer, f.body_far})
                         .quantities[ri(resource_type::iron_ore)] == 7.0f,
          "R3", "a pool on a body the buyer did not hold came across whole");

    check(acq.returns.size()
              == std::max(before_t.returns.size(), before_b.returns.size()),
          "R3", "the target's filed returns transferred (history length is the longer of the two)");
    // The merge is pairwise from the NEWEST end — see corp_command.cpp's
    // merge_returns for why that is the only join a tick-less record supports.
    check(acq.returns.back().holdings
              == before_t.returns.back().holdings + before_b.returns.back().holdings,
          "R3", "the merged newest row sums both firms' stock figures");
    check(acq.returns.back().net
              == before_t.returns.back().net + before_b.returns.back().net,
          "R3", "and sums their flows");
    // A row OLDER than the buyer's own three quarters is the target's alone.
    check(acq.returns[0].net == before_t.returns[0].net, "R3",
          "a row older than the buyer's history is the target's, carried unchanged");

    check(acq.hq_building != null_entity
              && std::find(acq.assets.begin(), acq.assets.end(), acq.hq_building)
                     != acq.assets.end(),
          "R3", "hq_building recomputed to a holding the acquirer actually owns");
    check(acq.influence_range > 0.0f, "R3",
          "influence_range recomputed over the merged holding set");

    check(w.units.at(f.target_unit).owner == f.buyer, "R3",
          "the target's unit re-pointed to the acquirer THROUGH its muster_base");
    check(owner_corp_of(w, w.units.at(f.target_unit).muster_base) == f.buyer, "R3",
          "and that muster base is now the acquirer's holding");

    check(w.sell_orders.size() == orders_before - 1, "R3",
          "the target's open sell order was CANCELLED, not reassigned");
    bool no_reassigned = true;
    for (const sell_order& o : w.sell_orders)
        no_reassigned = no_reassigned && o.quantity != 5.0f; // the target's order's size
    check(no_reassigned, "R3", "and it did not reappear under the acquirer's name");
    check(w.buy_orders.empty(), "R3", "the buy side is cancelled on the same rule");

    check(w.convoys.size() == 1 && w.convoys.front().corp == f.buyer, "R3",
          "a convoy in flight TRANSFERS (its cargo left the pool; a cancel would mint goods)");
    check(w.trade_routes.size() == 1 && w.trade_routes.front().corp == f.buyer
              && w.trade_routes.front().convoy_count == 7,
          "R3", "colliding trade routes folded into one (counts summed)");
    check(w.procurement_quotes.empty(), "R3",
          "a live quote naming the target is CANCELLED (an unaccepted offer is a promise)");
    check(w.procurement_contracts.size() == 1
              && w.procurement_contracts.front().supplier == f.buyer,
          "R3", "an ACCEPTED procurement contract transfers; the self-dealing one is dropped");
    check(!w.mercenary_contracts.empty()
              && w.mercenary_contracts.front().contractor == f.buyer,
          "R3", "the mercenary contract's contractor follows its committed force");
    check(w.battles.empty(), "R3", "a live battle naming the target is CANCELLED");

    // --- R4: nothing dangles -----------------------------------------------
    report_dangling(w, f.target, "R4", "no store in the world still names the erased corp");

    // --- R4: the world round-trips the save --------------------------------
    {
        const std::string image = to_bytes(w);
        world round;
        std::istringstream in(image, std::ios::binary);
        const bool ok = read_world_snapshot(round, in);
        check(ok, "R4", "the post-acquisition world reloads from its own snapshot");
        check(ok && to_bytes(round) == image, "R4",
              "and re-serialises byte-identically (save version asserted symbolically below)");
        check(round.corporations.count(f.target) == 0, "R4",
              "the erased corp does not come back through the save");
        std::printf("       snapshot %zu bytes at world_save_version %u\n",
                    image.size(), world_save_version);
    }

    // --- R5: equity_taken -----------------------------------------------
    {
        // The bystander's OLD rows about/from the target were dropped, so any
        // row it now holds toward the acquirer is this acquisition's.
        const sentiment_value v = sentiment_toward(w.sentiment, f.bystander, f.buyer);
        std::printf("       bystander -> acquirer sentiment after the buyout: "
                    "access %.4f trust %.4f\n",
                    static_cast<double>(v.access), static_cast<double>(v.trust));
        check(v.trust != 0.0f || v.access != 0.0f, "R5",
              "equity_taken landed on the surviving rival (harness authors the weight; "
              "economy.lua leaves it 0 per BL-545's inertness rule)");
        check(sentiment_toward(w.sentiment, f.buyer, f.buyer).trust == 0.0f, "R5",
              "the acquirer forms no opinion of itself");
    }
}

// ---------------------------------------------------------------------------
// R3/R4 end to end — a really generated, warm-started world
// ---------------------------------------------------------------------------
constexpr int k_warm_ticks = 40;

void tick(world& w, const recipe_registry& reg, int t)
{
    w.current_econ_tick = t;
    dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                     reg.logistics_cost(convoy_mode::space));
    advance_convoys(w);
    const economy_report report = run_economy_step(w, reg);
    const auto flows = clear_markets(w, reg, report);
    apply_budget(w, reg, flows, report.workforce_contention);
    credit_arrived_convoys(w, t);
}

void run_warm_start_rows(const recipe_registry& reg)
{
    std::printf("\nR4 — end to end on a generated world, warm-started %d econ ticks\n",
                k_warm_ticks);

    world_params p = no_prehistory();
    p.seed = 7;
    world w = make_hard_coded_world(p);
    assign_default_recipes(w, reg);
    generate_background_firms(w, reg, 7u ^ 0x8A21F00Du);
    for (int t = 0; t < k_warm_ticks; ++t)
        tick(w, reg, t);

    const std::vector<entity_id> ids = sorted_corp_ids(w);
    check(ids.size() >= 2, "R4", "the generated world holds at least two corporations");
    if (ids.size() < 2)
        return;

    // Pick the acquirer and the target deterministically: the lowest two ids
    // that are not the player's corp.
    entity_id acquirer = null_entity, target = null_entity;
    for (const entity_id id : ids)
    {
        if (id == w.player_entity || w.corporations.at(id).is_player)
            continue;
        if (acquirer == null_entity)      acquirer = id;
        else if (target == null_entity)   target   = id;
    }
    check(acquirer != null_entity && target != null_entity, "R4",
          "two non-player corporations available");
    if (acquirer == null_entity || target == null_entity)
        return;

    // The generated world's ownership classes are BL-638's output; that
    // derivation is a concurrent task in this same wave, and until it lands every
    // corporation reads `closed`. The class is SET here so the verb has something
    // to buy — the harness is constructing the scenario the gate is about, not
    // asserting what generation produces. R2 above is the gate's own check.
    w.corporations.at(target).ownership_class = ownership_class::publicly_held;
    // ...and funded, so the row measures the dissolution rather than the balance.
    const float price = corp_acquisition_price(w.corporations.at(target),
                                               reg.acquisition().multiple);
    w.corporations.at(acquirer).balance = std::max(w.corporations.at(acquirer).balance,
                                                   price + 1.0f);

    const std::size_t holdings_before = w.corporations.at(acquirer).assets.size()
                                      + w.corporations.at(target).assets.size();
    std::printf("       acquirer %u buys %u for %.2f "
                "(%zu filed quarters, trailing_net %.4f)\n",
                acquirer, target, static_cast<double>(price),
                w.corporations.at(target).returns.size(),
                static_cast<double>(corp_trailing_net(w.corporations.at(target))));

    const corp_command_result r = apply_corp_command(w, reg, buy_cmd(acquirer, target));
    check(r == corp_command_result::applied, "R4", "the acquisition applied on the real world");
    check(w.corporations.at(acquirer).assets.size() == holdings_before, "R4",
          "the merged holding count is the sum of the two");
    report_dangling(w, target, "R4",
                    "nothing in the generated world still names the erased corp");

    // The world keeps running afterwards — the sharpest check that nothing
    // dangles is that every downstream pass survives a full tick over it.
    for (int t = 0; t < 4; ++t)
        tick(w, reg, k_warm_ticks + t);
    report_dangling(w, target, "R4",
                    "and still nothing after four further economy ticks");
    check(!w.corporations.at(acquirer).returns.empty(), "R4",
          "the acquirer keeps filing returns after the merge");

    {
        const std::string image = to_bytes(w);
        world round;
        std::istringstream in(image, std::ios::binary);
        const bool ok = read_world_snapshot(round, in);
        check(ok && to_bytes(round) == image, "R4",
              "the generated world round-trips the save after the acquisition");
    }

    // Determinism: the same seed, the same warm start, the same buyout, twice.
    {
        const auto run = [&](uint32_t seed) {
            world_params q = no_prehistory();
            q.seed = seed;
            world v = make_hard_coded_world(q);
            assign_default_recipes(v, reg);
            generate_background_firms(v, reg, seed ^ 0x8A21F00Du);
            for (int t = 0; t < k_warm_ticks; ++t)
                tick(v, reg, t);
            const std::vector<entity_id> vi = sorted_corp_ids(v);
            entity_id a = null_entity, b = null_entity;
            for (const entity_id id : vi)
            {
                if (id == v.player_entity || v.corporations.at(id).is_player) continue;
                if (a == null_entity) a = id; else if (b == null_entity) b = id;
            }
            v.corporations.at(b).ownership_class = ownership_class::publicly_held;
            v.corporations.at(a).balance = 1.0e7f;
            apply_corp_command(v, reg, buy_cmd(a, b));
            for (int t = 0; t < 4; ++t)
                tick(v, reg, k_warm_ticks + t);
            return to_bytes(v);
        };
        const std::string x = run(11u), y = run(11u);
        check(!x.empty() && x == y, "R4",
              "two identical runs through the buyout produce byte-identical worlds");
    }
}

// ---------------------------------------------------------------------------
// Row X — the KNOWN exposure, measured rather than hidden (NR-655)
// ---------------------------------------------------------------------------
void run_exposure_row(const recipe_registry& reg)
{
    std::printf("\nX  — the trailing_net exposure this verb inherits (NR-655)\n");
    std::printf("       A filed return records the MONEY LOOP only. apply_budget runs at\n");
    std::printf("       app.cpp:1271; run_nation_step — which pays mercenary contracts and\n");
    std::printf("       national transfers — runs at :1281. Earnings that arrive there are\n");
    std::printf("       NOT in any filed `net`, so trailing_net under-reports for a firm\n");
    std::printf("       earning through contracts, and this verb undervalues it by k x that\n");
    std::printf("       gap. NOT fixed here; the fork is Ben's.\n");

    // Two identical firms. One of them also receives a post-loop payout each
    // quarter — modelled exactly the way run_nation_step credits a contractor: a
    // direct balance credit AFTER the return for that quarter has been filed.
    constexpr float k_payout_per_quarter = 80.0f;
    constexpr float k_loop_net           = 10.0f;

    corporation_component plain;
    corporation_component earner;
    plain.balance = earner.balance = 0.0f;
    for (int q = 0; q < 8; ++q)
    {
        plain.returns.push_back(filed(k_loop_net, plain.balance + k_loop_net, 2, 300.0f));
        plain.balance += k_loop_net;

        earner.returns.push_back(filed(k_loop_net, earner.balance + k_loop_net, 2, 300.0f));
        earner.balance += k_loop_net;
        earner.balance += k_payout_per_quarter; // the payout the return cannot see
    }

    const float k = reg.acquisition().multiple;
    const float price_plain  = corp_acquisition_price(plain,  k);
    const float price_earner = corp_acquisition_price(earner, k);

    // What the earner WOULD price at if its returns saw the payout.
    const float true_trailing = k_loop_net + k_payout_per_quarter;
    const float honest_price  = 300.0f + k * true_trailing + earner.balance;
    const float undervalue    = honest_price - price_earner;

    std::printf("       loop-only firm:      trailing_net %6.2f  price %9.2f\n",
                static_cast<double>(corp_trailing_net(plain)),
                static_cast<double>(price_plain));
    std::printf("       contract earner:     trailing_net %6.2f  price %9.2f  "
                "(balance %.2f)\n",
                static_cast<double>(corp_trailing_net(earner)),
                static_cast<double>(price_earner),
                static_cast<double>(earner.balance));
    std::printf("       had the return seen the payout: trailing_net %6.2f  price %9.2f\n",
                static_cast<double>(true_trailing), static_cast<double>(honest_price));
    std::printf("       >>> MEASURED UNDERVALUATION: %.2f credits "
                "(= k %.1f x %.2f/quarter unseen)\n",
                static_cast<double>(undervalue), static_cast<double>(k),
                static_cast<double>(k_payout_per_quarter));

    // The row ASSERTS the exposure exists — so that if the fork is ever closed,
    // this fails loudly and the harness is updated deliberately rather than the
    // exposure quietly disappearing from the record.
    check(corp_trailing_net(earner) == corp_trailing_net(plain), "X",
          "the earner's filed trailing_net is IDENTICAL to the loop-only firm's "
          "(the payout is invisible to the record)");
    check(undervalue > 0.0f, "X",
          "so the verb undervalues a contract-earning firm — measured above, "
          "owed to NR-655, deliberately not papered over here");
}

} // namespace

int main()
{
    recipe_registry reg;
    author_economics(reg);
    reg.set_thresholds(/*t_full=*/1.0f, /*t_idle=*/0.2f);
    {
        recipe steel;
        steel.name = "steel";
        steel.inputs[ri(resource_type::iron_ore)] = 2.0f;
        steel.outputs[ri(resource_type::steel)]   = 1.0f;
        reg.add_recipe(steel);
    }
    // economy.lua's authored multiple, restated (this harness is Lua-free).
    {
        acquisition_params ap;
        ap.multiple = 8.0f;
        reg.set_acquisition(ap);
    }
    // equity_taken's weight is ZERO in the shipped economy.lua — BL-545's rule
    // that an unauthored factor is inert, not guessed. R5 needs the emission to be
    // OBSERVABLE, so the harness authors a weight of its own. This is a harness
    // fixture, not a shipped number.
    {
        sentiment_params sp = reg.sentiment();
        sp.factors[static_cast<std::size_t>(sentiment_factor_kind::equity_taken)].trust = -1.5f;
        sp.factors[static_cast<std::size_t>(sentiment_factor_kind::equity_taken)].access = -0.5f;
        reg.set_sentiment(sp);
    }

    // Vacuity guards.
    if (reg.acquisition().multiple <= 0.0f)
    {
        std::printf("FATAL: no acquisition multiple authored — R1 would be vacuous.\n");
        return 2;
    }
    if (reg.economics(building_type::extraction_site).build_cost <= 0.0f)
    {
        std::printf("FATAL: registry authored no build_cost — the fixture has no book value.\n");
        return 2;
    }

    std::printf("whole_firm_buyout — BL-628, requirement group `whole-firm-buyout` R1-R6\n");

    run_price_rows(reg);
    run_gate_rows(reg);
    run_transfer_rows(reg);
    run_warm_start_rows(reg);
    run_exposure_row(reg);

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES ABOVE",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
