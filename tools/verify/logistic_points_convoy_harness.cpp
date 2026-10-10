// ---------------------------------------------------------------------------
// Headless passive-Logistic-Points harness (BL-597, LP_PASSIVE_CONVOYS)
// No SDL / Lua / ImGui — builds over the world/* logic alone.
// ---------------------------------------------------------------------------
// LOGISTICS.md § Logistic Points is the authority. BL-596 landed the ACTIVE
// half (militaries draw against a per-anchor pool in `run_unit_march`); this
// is the PASSIVE half — automatic trading (`commit_convoy`, the funnel BOTH
// `dispatch_convoys`' auto-scan and the player's `dispatch_convoy` verb go
// through) admissibility-gated against the SAME per-anchor pool, contested
// with the active draw within one tick when a caller shares one `lp_pool_map`
// instance across both (BL-596's `run_unit_march` and this file's
// `commit_convoy`/`dispatch_convoys`, both now take an optional
// `lp_pool_map*`).
//
// BL-1265/1266 (MARKETS.md § The shelf economy; TRADE.md): the dispatcher,
// `commit_convoy` and corporation pools retired. Every shipment is a TRADE
// committed through `commit_trade_shipment` (which always trims to the anchor,
// BL-1186 E1) and the auto-scan is `run_trades`; stock sits on the source
// market's SHELF. The rows below keep their claims on that funnel.
//
// This file does NOT re-assert BL-596's own anchor-pool-determinism claims
// (logistic_points_harness.cpp owns those). It asserts only what BL-597 adds:
//
//   P1  A leg within the anchor's pool COMMITS normally: source pool debited
//       by qty, balance debited by haulage cost ONLY (no second LP credit
//       charge — LP is a cap, not a price), convoy created, and the anchor's
//       pool is drawn down by exactly the leg's distance.
//   P2  A leg that would EXCEED the pool is REFUSED OUTRIGHT: balance, source
//       pool and the convoy list are all byte-identical to the pre-call
//       snapshot, and the refusal is counted (`out_refused_no_lp`).
//   P3  An anchorless body refuses every intra-body leg — no passive LP
//       exists to draw against (mirrors BL-596's L4).
//   P4  A SPACE (inter-body) leg is EXEMPT from the gate entirely — commits
//       regardless of the (irrelevant, empty) LP state, matching BL-596's own
//       march gate, which likewise only fires for a tile-grounded move.
//   P5  dispatch_convoys' own counters (`dispatched`, `refused_no_lp`) agree
//       with what actually happened to `world.convoys`.
//   P6  CONTENTION — "war flips the queue" made real. A mobilised corp's
//       march (BL-596, run_unit_march) and a convoy dispatch (BL-597,
//       commit_convoy) drawing against the SAME anchor within ONE shared
//       `lp_pool_map` resolve deterministically and IDENTICALLY across two
//       independent runs — the row that proves the shared pool is really
//       shared, not two disconnected economies wearing one name.
//   P7  Passing NO shared pool (the default) reproduces a private, always-
//       fresh pool per call — commit_convoy in isolation is unaffected by
//       any earlier call in the same process (no hidden carry-over).
//
// The process exits non-zero if any assertion FAILs.

#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/trade.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <tuple>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

bool approx(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

constexpr std::size_t r_iron = static_cast<std::size_t>(resource_type::iron_ore);

// ---------------------------------------------------------------------------
// Fixture: one body, a plains column, a corp anchored at (0,0), a market at
// (0,3) three tiles away — the same shape convoy_command.cpp's own
// `make_scenario` uses, so a reader who knows that file recognises this one.
// ---------------------------------------------------------------------------
struct scenario
{
    world     w;
    entity_id body       = null_entity;
    entity_id corp       = null_entity;
    entity_id src_market = null_entity;
    entity_id dst_market = null_entity;
};

entity_id tile_at(world& w, entity_id body, int c, int r)
{
    const int gw = w.bodies.at(body).grid_width;
    return body_tile_grid(w, body)[static_cast<std::size_t>(r) * static_cast<std::size_t>(gw)
                                   + static_cast<std::size_t>(c)];
}

/// `add_city` places a population centre (a supply anchor, LOGISTICS.md's
/// "cities are the locus") at a tile OFF the corp's straight dispatch route
/// so the BL-148/149 node discount (which shares `population_centre_tile`)
/// never touches this file's exact-cost assertions — same reasoning as
/// convoy_command.cpp's own BL-597 fixture fix.
void add_city(world& w, entity_id tile)
{
    w.population_centre_tile[w.create_entity()] = tile;
}

scenario make_scenario(float stock = 100.0f, float balance = 1000.0f, bool with_anchor = true)
{
    scenario s;

    s.body = s.w.create_entity();
    body_component bc{};
    bc.name              = "Anvil";
    bc.type              = body_type::planet;
    bc.orbital_radius_au = 1.0f;
    bc.grid_width        = 32;
    bc.grid_height       = 4;
    s.w.bodies[s.body] = bc;
    for (int r = 0; r < bc.grid_height; ++r)
        for (int c = 0; c < bc.grid_width; ++c)
        {
            const entity_id t = s.w.create_entity();
            tile_component tc{};
            tc.body        = s.body;
            tc.grid_x      = c;
            tc.grid_y      = r;
            tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass; tc.cover_density = 150;
            tc.landform    = terrain_landform::plains;
            s.w.tiles[t] = tc;
        }

    s.corp = s.w.create_entity();
    corporation_component cc;
    cc.balance   = balance;
    cc.is_player = true;

    const entity_id origin = tile_at(s.w, s.body, 0, 0);
    const entity_id bld    = s.w.create_entity();
    building_component b{};
    b.tile = origin;
    b.type = building_type::extraction_site;
    s.w.buildings[bld] = b;
    cc.assets.push_back(bld);
    s.w.corporations[s.corp] = cc;
    s.w.player_entity = s.corp;

    if (with_anchor)
        add_city(s.w, tile_at(s.w, s.body, 1, 0)); // one column over — off the route

    s.src_market = s.w.create_entity();
    market_component sm{};
    sm.body          = s.body;
    sm.centre_tile   = origin;
    sm.base_price[r_iron] = 5.0f;
    sm.price         = sm.base_price;
    s.w.markets[s.src_market] = sm;

    s.dst_market = s.w.create_entity();
    market_component dm{};
    dm.body          = s.body;
    dm.centre_tile   = tile_at(s.w, s.body, 0, 3);
    dm.base_price[r_iron] = 5.0f;
    dm.price         = dm.base_price;
    s.w.markets[s.dst_market] = dm;

    // BL-1265: the stock is on the SOURCE MARKET's shelf (no corporation pools).
    s.w.markets.at(s.src_market).inventory[r_iron] = stock;
    return s;
}

recipe_registry make_registry(float lp_per_anchor)
{
    recipe_registry reg;
    military_capability_params mp = reg.military();
    mp.active_lp_per_anchor_tick = lp_per_anchor;
    // Deliberately zero: P1 asserts the balance debit equals `leg.cost`
    // EXACTLY — a nonzero active-LP credit rate is BL-596's own price for a
    // MARCH draw, never a convoy's (LOGISTICS.md rule 1: LP is the cap here,
    // `leg.cost` alone is the price).
    mp.active_lp_credit_per_unit_distance = 0.0f;
    reg.set_military(mp);
    return reg;
}

/// BL-1266: the dispatcher's auto-scan retired; AUTO TRADE is what ships now.
/// Give the scenario's corp a staffed Planetary Marketplace (a trade building
/// that is NOT a supply anchor, so the LP topology the rows pin is unchanged)
/// and author iron's trade capacity, so `run_trades` has points to spend.
void enable_auto_trade(scenario& s, recipe_registry& reg)
{
    trade_params tp;
    tp.capacity[r_iron]   = 1000.0f; // one point moves more than any row needs
    tp.marketplace_points = 1.0f;
    reg.set_trade(tp);
    const entity_id mp = s.w.create_entity();
    building_component b{};
    // Far off every route, and in the SOURCE market's catchment: NR-1018 (Ben,
    // 2026-10-10) spends a building's points only on trades leaving the market
    // it stands in. (20, 0) is 12 wrapped columns from both centres, 0 rows from
    // the source and 3 from the destination; the (20, 2) it stood on before the
    // ruling routes to the destination, and auto trade then shipped nothing.
    b.tile               = tile_at(s.w, s.body, 20, 0);
    b.type               = building_type::planetary_marketplace;
    b.workforce_assigned = 1.0f;
    s.w.buildings[mp] = b;
    s.w.corporations.at(s.corp).assets.push_back(mp);
}

/// The source SHELF's iron (BL-1265: was the corporation's pool).
float pool_iron(const scenario& s)
{
    return s.w.markets.at(s.src_market).inventory[r_iron];
}

logistics_nodes nodes_of(world& w) { return collect_logistics_nodes(w); }

/// One trade shipment of @p qty iron, source -> destination (BL-1266:
/// `commit_trade_shipment` is THE funnel every shipment goes through).
bool ship(scenario& s, const recipe_registry& reg, const convoy_leg& leg, float qty,
          lp_pool_map* pools, bool* refused, float* sent = nullptr)
{
    economy_report rep; // the purchase is billed at a clear this harness never runs
    return commit_trade_shipment(s.w, reg, rep, s.corp, s.src_market, s.dst_market, r_iron, qty,
                                 leg, pools, refused, sent);
}

} // namespace

// ---------------------------------------------------------------------------
// P1 — a leg within the pool commits normally, drawing down exactly the CARGO
// QUANTITY (Ben, 2026-08-25, NR-620: not distance — see commit_trade_shipment).
// ---------------------------------------------------------------------------

void p1_granted_draw()
{
    std::printf("\n-- P1  a leg within the anchor's pool commits, drawing exactly the cargo qty --\n");

    scenario s = make_scenario(100.0f, 1000.0f);
    recipe_registry reg = make_registry(/*lp_per_anchor*/ 50.0f);
    const logistics_nodes nodes = nodes_of(s.w);

    const convoy_leg leg = price_trade_leg(s.w, reg, nodes, s.corp, s.src_market, s.dst_market,
                                           r_iron, 25.0f);
    check(leg.viable, "the leg prices viable (a plains column, no water)");
    check(leg.mode == convoy_mode::land, "an all-plains intra-body lane is land mode");

    lp_pool_map pools;
    const float balance_before = s.w.corporations.at(s.corp).balance;
    bool refused = false;
    const bool ok = ship(s, reg, leg, 25.0f, &pools, &refused);

    check(ok, "the leg commits");
    check(!refused, "no refusal reported");
    check(s.w.convoys.size() == 1, "exactly one convoy is created");
    check(approx(pool_iron(s), 75.0f), "the source shelf is debited by exactly the quantity");
    check(approx(s.w.corporations.at(s.corp).balance, balance_before - leg.cost),
          "the balance is debited by leg.cost ONLY — no second LP-specific credit charge "
          "(the purchase is billed at the clear)");

    // The anchor nearest the source centre — one column over — should show the
    // rate minus the CARGO QUANTITY (25), and nothing to do with the route's
    // length. This row is the load-bearing one for NR-620: it fails if the draw
    // ever goes back to being distance-proportional, since a few plains hops
    // and 25 units of iron are different numbers.
    const entity_id anchor = tile_at(s.w, s.body, 1, 0);
    check(pools.count(s.body) == 1 && pools.at(s.body).count(anchor) == 1,
          "the shared pool now holds an entry for this body's anchor");
    if (pools.count(s.body) == 1 && pools.at(s.body).count(anchor) == 1)
        check(approx(pools.at(s.body).at(anchor), 50.0f - 25.0f),
              "the anchor's pool is drawn down by exactly the cargo quantity (25), not the distance");
}

// ---------------------------------------------------------------------------
// P2 — a leg the pool cannot admit even one unit of is refused, mutating nothing
// ---------------------------------------------------------------------------

void p2_refused_mutates_nothing()
{
    std::printf("\n-- P2  a leg the pool cannot admit is refused outright --\n");

    scenario s = make_scenario(100.0f, 1000.0f);
    // A rate so small it is under the one-unit floor (BL-1186 E1).
    recipe_registry reg = make_registry(/*lp_per_anchor*/ 0.5f);
    const logistics_nodes nodes = nodes_of(s.w);

    const convoy_leg leg = price_trade_leg(s.w, reg, nodes, s.corp, s.src_market, s.dst_market,
                                           r_iron, 25.0f);
    check(leg.viable, "fixture: the leg prices viable, and its 25 units exceed the tiny pool");

    lp_pool_map pools;
    const float    balance_before = s.w.corporations.at(s.corp).balance;
    const float    pool_before    = pool_iron(s);
    const std::size_t convoys_before = s.w.convoys.size();

    bool refused = false;
    const bool ok = ship(s, reg, leg, 25.0f, &pools, &refused);

    check(!ok, "the leg is refused");
    check(refused, "the refusal is attributed to want of passive LP specifically");
    check(approx(s.w.corporations.at(s.corp).balance, balance_before), "balance is untouched");
    check(approx(pool_iron(s), pool_before), "the source shelf is untouched");
    check(s.w.convoys.size() == convoys_before, "no convoy was created");
}

// ---------------------------------------------------------------------------
// P3 — an anchorless body refuses every intra-body leg
// ---------------------------------------------------------------------------

void p3_no_anchor_no_dispatch()
{
    std::printf("\n-- P3  an anchorless body has no passive LP to draw against --\n");

    scenario s = make_scenario(100.0f, 1000.0f, /*with_anchor*/ false);
    recipe_registry reg = make_registry(/*lp_per_anchor*/ 1.0e6f); // generous — doesn't matter
    const logistics_nodes nodes = nodes_of(s.w);

    const convoy_leg leg = price_trade_leg(s.w, reg, nodes, s.corp, s.src_market, s.dst_market,
                                           r_iron, 25.0f);
    check(leg.viable, "fixture: the leg still prices (LP is a separate gate from routability)");

    lp_pool_map pools;
    bool refused = false;
    const bool ok = ship(s, reg, leg, 25.0f, &pools, &refused);

    check(!ok && refused, "refused — no anchor exists on this body at all");
    check(s.w.convoys.empty(), "no convoy was created");
}

// ---------------------------------------------------------------------------
// P4 — a space (inter-body) leg is exempt from the gate entirely
// ---------------------------------------------------------------------------

void p4_space_leg_exempt()
{
    std::printf("\n-- P4  a space leg is exempt — no anchor, no rate, still commits --\n");

    world w;
    // Two distinct bodies, no anchors placed anywhere, LP rate zero.
    entity_id src_body = w.create_entity();
    body_component bsrc; bsrc.type = body_type::planet; bsrc.orbital_radius_au = 1.0f;
    w.bodies[src_body] = bsrc;
    entity_id dest_body = w.create_entity();
    body_component bdst; bdst.type = body_type::planet; bdst.orbital_radius_au = 2.0f;
    w.bodies[dest_body] = bdst;

    entity_id corp = w.create_entity();
    corporation_component cc; cc.balance = 5000.0f;
    entity_id tile = w.create_entity();
    tile_component tc{}; tc.body = src_body;
    w.tiles[tile] = tc;
    entity_id pad = w.create_entity();
    building_component pb{}; pb.tile = tile; pb.type = building_type::launchpad;
    w.buildings[pad] = pb;
    cc.assets.push_back(pad);
    w.corporations[corp] = cc;

    entity_id src_mkt = w.create_entity();
    market_component sm{}; sm.body = src_body; w.markets[src_mkt] = sm;
    entity_id dst_mkt = w.create_entity();
    market_component dm{}; dm.body = dest_body; w.markets[dst_mkt] = dm;

    // BL-1265: the stock and the launch's propellant sit on the source SHELF.
    w.markets.at(src_mkt).inventory[r_iron] = 50.0f;
    w.markets.at(src_mkt).inventory[static_cast<std::size_t>(resource_type::propellant)] = 5.0f;

    recipe_registry reg = make_registry(/*lp_per_anchor*/ 0.0f); // no LP anywhere
    const logistics_nodes nodes = collect_logistics_nodes(w);
    const convoy_leg leg = price_trade_leg(w, reg, nodes, corp, src_mkt, dst_mkt, r_iron, 10.0f);
    check(leg.viable && leg.mode == convoy_mode::space, "fixture: an inter-body leg prices as space mode");

    lp_pool_map pools;
    bool refused = false;
    economy_report rep;
    const bool ok = commit_trade_shipment(w, reg, rep, corp, src_mkt, dst_mkt, r_iron, 10.0f, leg,
                                          &pools, &refused);

    check(ok, "a space leg commits despite zero LP anywhere — LOGISTICS.md's design is tile-grounded");
    check(!refused, "no LP refusal is ever attributed to a space leg");
    check(pools.empty(), "the space path never touches lp_pool_for_body at all");
}

// ---------------------------------------------------------------------------
// P5 — the trade pass's own counters agree with what happened
// ---------------------------------------------------------------------------

void p5_dispatch_counters()
{
    std::printf("\n-- P5  run_trades' shipment/refused_no_lp counters are honest --\n");

    scenario s = make_scenario(100.0f, 1000.0f);
    // A pool that comfortably covers ONE shortfall haul but not a second.
    recipe_registry reg = make_registry(/*lp_per_anchor*/ 10.0f);
    enable_auto_trade(s, reg);

    // Auto trade chases a margin, so the destination must price iron above
    // home (10 vs 5) for the haul to happen at all.
    s.w.markets.at(s.dst_market).price[r_iron]  = 10.0f;
    s.w.markets.at(s.dst_market).demand[r_iron] = 30.0f;
    s.w.markets.at(s.dst_market).supply[r_iron] = 0.0f;

    economy_report rep1;
    const trade_tick t1 = run_trades(s.w, reg, rep1);
    check(t1.auto_shipments == static_cast<int>(s.w.convoys.size()) && t1.auto_shipments >= 1,
          "auto_shipments counts exactly the convoys this pass created (and it shipped)");

    // Drive the SAME shortfall again with a pool too small this time.
    recipe_registry tiny_reg = make_registry(/*lp_per_anchor*/ 0.01f);
    scenario s2 = make_scenario(100.0f, 1000.0f);
    enable_auto_trade(s2, tiny_reg);
    s2.w.markets.at(s2.dst_market).price[r_iron]  = 10.0f;
    s2.w.markets.at(s2.dst_market).demand[r_iron] = 30.0f;
    s2.w.markets.at(s2.dst_market).supply[r_iron] = 0.0f;
    economy_report rep2;
    const trade_tick t2 = run_trades(s2.w, tiny_reg, rep2);
    check(t2.auto_shipments == 0 && s2.w.convoys.empty(),
          "a starved pool ships nothing");
    check(t2.refused_no_lp >= 1, "the refusal is counted on the tick summary");
}

// ---------------------------------------------------------------------------
// P6 — CONTENTION: a mobilised march and the trade pass share one pool
// ---------------------------------------------------------------------------

void p6_shared_pool_contention()
{
    std::printf("\n-- P6  war flips the queue: a march and the trade pass genuinely contend --\n");

    // A mobilised rival unit sits on the anchor tile, ready to march away. The
    // real tick order runs the march (inside run_economy_step) BEFORE the trade
    // pass, so armies claim the anchor first; the shipment then takes what the
    // march left (BL-1186 E1: trimmed, not refused).
    auto run_scenario = [&](float lp) -> std::tuple<bool, bool, float, bool> {
        scenario s = make_scenario(100.0f, 1000.0f);
        const entity_id rival = s.w.create_entity();
        corporation_component rc; rc.balance = 1000.0f;
        s.w.corporations[rival] = rc;
        s.w.corp_hostile_pairs.insert({ std::min(s.corp, rival), std::max(s.corp, rival) });

        const entity_id anchor_tile = tile_at(s.w, s.body, 1, 0);
        const entity_id unit = s.w.create_entity();
        unit_component uc{};
        uc.owner    = rival;
        uc.position = anchor_tile;
        uc.count    = 50;
        uc.type     = 0; // Levy Spear — unit_class::infantry
        movement_order mo;
        mo.dest       = tile_at(s.w, s.body, 5, 0);
        mo.next_index = 1;
        for (int c = 0; c <= 5; ++c) mo.path.push_back(tile_at(s.w, s.body, c, 0));
        uc.order = mo;
        s.w.units[unit] = uc;

        recipe_registry reg = make_registry(lp);
        military_capability_params mp = reg.military();
        mp.march_points_per_class[static_cast<std::size_t>(unit_class::infantry)] = 2.0f;
        reg.set_military(mp);
        enable_auto_trade(s, reg);

        lp_pool_map shared_pool;
        s.w.markets.at(s.dst_market).price[r_iron]  = 10.0f;
        s.w.markets.at(s.dst_market).demand[r_iron] = 1000.0f; // deep: the room never binds, the LP does
        s.w.markets.at(s.dst_market).supply[r_iron] = 0.0f;
        const unit_march_tick mt = run_unit_march(s.w, reg, &shared_pool);
        economy_report rep;
        const trade_tick tt = run_trades(s.w, reg, rep, &shared_pool);

        bool cap_held = true;
        for (const auto& [body, pools] : shared_pool)
            for (const auto& [anchor, left] : pools)
                if (left < -1e-4f)
                    cap_held = false;
        float cargo = 0.0f;
        for (const convoy_component& c : s.w.convoys)
            cargo += c.cargo_qty;
        return { mt.marching == 1 && mt.refused_no_lp == 0,
                 tt.auto_shipments == 1 && tt.refused_no_lp == 0, cargo, cap_held };
    };

    // A pool of 3.0: the march's 2 points first, then the shipment the 1 left.
    for (int run = 0; run < 2; ++run)
    {
        const auto [march_ok, ship_ok, cargo, cap_held] = run_scenario(3.0f);
        (void)march_ok; (void)ship_ok;
        const std::string tag = " (run " + std::to_string(run) + ")";
        check(cap_held && cargo <= 1.0f + 1e-3f,
              ("the shipment and the march together never draw past the pool (cap held)" + tag)
                  .c_str());
    }

    // Determinism proper, with numbers pinned: the pool is sized to the
    // SHIPMENT'S WANT (deep demand, 100 on the shelf): the 2.0-point march draws
    // FIRST and is granted, leaving 28; the 30-unit shipment is TRIMMED to 28.
    const auto a = run_scenario(30.0f);
    const auto b = run_scenario(30.0f);
    check(a == b, "the contested outcome is IDENTICAL across two independent runs");
    check(std::get<0>(a), "the march (drawn first, the real tick order) is granted");
    check(std::get<1>(a) && std::fabs(std::get<2>(a) - 28.0f) < 1e-3f,
          "...leaving 28 of 30 LP, so the 30-unit shipment is TRIMMED to 28 (BL-1186 E1), not refused");
}

// ---------------------------------------------------------------------------
// P7 — no shared pool (default) reproduces a private, always-fresh pool
// ---------------------------------------------------------------------------

void p7_default_is_private_and_fresh()
{
    std::printf("\n-- P7  no shared pool given: private, always-fresh, no cross-call carry-over --\n");

    scenario s = make_scenario(100.0f, 1000.0f);
    recipe_registry reg = make_registry(/*lp_per_anchor*/ 5.0f);
    const logistics_nodes nodes = nodes_of(s.w);

    // Two legs, each individually within the pool (5 units each), committed
    // with NO shared pool passed — if any hidden state persisted between the
    // two calls, the second would see the first's draw and be trimmed or refused.
    const convoy_leg leg1 = price_trade_leg(s.w, reg, nodes, s.corp, s.src_market, s.dst_market,
                                            r_iron, 5.0f);
    bool refused1 = false;
    float sent1 = 0.0f;
    const bool ok1 = ship(s, reg, leg1, 5.0f, nullptr, &refused1, &sent1);
    check(ok1 && !refused1 && approx(sent1, 5.0f), "first call (no shared pool) commits whole");

    const convoy_leg leg2 = price_trade_leg(s.w, reg, nodes, s.corp, s.src_market, s.dst_market,
                                            r_iron, 5.0f);
    bool refused2 = false;
    float sent2 = 0.0f;
    const bool ok2 = ship(s, reg, leg2, 5.0f, nullptr, &refused2, &sent2);
    check(ok2 && !refused2 && approx(sent2, 5.0f),
          "second call ALSO commits whole — a private local pool means no draw carries over "
          "from the first call (each call rebuilds the full rate fresh)");
}

// ---------------------------------------------------------------------------
// P8 — BL-1186 E1: a shipment sends what the anchor admits.
// ---------------------------------------------------------------------------

void p8_partial_lp_sends()
{
    std::printf("\n-- P8  BL-1186 E1: a cargo over the pool is trimmed, not refused --\n");

    // P8.1 AN AUTO TRADE is trimmed to the anchor's pool: 100 on the source
    // shelf, the destination short 30 at twice the price, the anchor (the city
    // at (1,0)) holds 20. (BL-1266: this row was a market's own shelf export,
    // which retired; a trade is now what ships a shelf, by the same rule.)
    {
        scenario s = make_scenario(/*stock=*/100.0f, 1000.0f);
        recipe_registry reg = make_registry(20.0f);
        enable_auto_trade(s, reg);
        s.w.markets.at(s.dst_market).price[r_iron]     = 10.0f;
        s.w.markets.at(s.dst_market).demand[r_iron]    = 30.0f;
        s.w.markets.at(s.dst_market).supply[r_iron]    = 0.0f;
        economy_report rep;
        const trade_tick ct = run_trades(s.w, reg, rep);
        float cargo = 0.0f;
        for (const convoy_component& c : s.w.convoys)
            cargo += c.cargo_qty;
        check(ct.auto_shipments == 1 && ct.refused_no_lp == 0 && approx(cargo, 20.0f)
                  && approx(s.w.markets.at(s.src_market).inventory[r_iron], 80.0f),
              "P8.1 an auto trade sized 30 leaves at the anchor's 20, shelf 100 -> 80");
    }

    // The direct commit rows: one leg of `qty` priced, committed with no shared
    // pool (each call a fresh pool at the rate). @p src_price is the source
    // shelf's posted price — the purchase the solvency gate also weighs.
    struct outcome
    {
        bool  ok;
        bool  refused_lp;
        float sent;
        float spent;
        std::size_t convoys;
    };
    const auto commit = [](float lp, float qty, float balance, float src_price = 5.0f) -> outcome {
        scenario s = make_scenario(100.0f, balance);
        s.w.markets.at(s.src_market).base_price[r_iron] = src_price;
        s.w.markets.at(s.src_market).price[r_iron]      = src_price;
        recipe_registry reg = make_registry(lp);
        const logistics_nodes nodes = nodes_of(s.w);
        const convoy_leg leg = price_trade_leg(s.w, reg, nodes, s.corp, s.src_market,
                                               s.dst_market, r_iron, qty);
        bool  refused = false;
        float sent    = 0.0f;
        const float before = s.w.corporations.at(s.corp).balance;
        const bool ok = ship(s, reg, leg, qty, nullptr, &refused, &sent);
        return {ok, refused, sent, before - s.w.corporations.at(s.corp).balance,
                s.w.convoys.size()};
    };

    // P8.2 THE ONE-UNIT FLOOR: an anchor holding 0.5 does not make a convoy of a
    // 30-unit cargo — refused for want of LP, nothing mutated.
    {
        const outcome o = commit(0.5f, 30.0f, 1000.0f);
        check(!o.ok && o.refused_lp && o.convoys == 0 && o.spent == 0.0f,
              "P8.2 pool 0.5, cargo 30: under the one-unit floor -> refused for LP, nothing spent");
    }

    // P8.3 A CARGO UNDER ONE UNIT: the floor is min(cargo, 1), so a 0.8 cargo needs
    // the whole 0.8 — a 0.5 pool refuses it, a 0.9 pool sends it whole.
    {
        const outcome short_pool = commit(0.5f, 0.8f, 1000.0f);
        check(!short_pool.ok && short_pool.refused_lp && short_pool.convoys == 0,
              "P8.3a cargo 0.8, pool 0.5: refused (a sub-unit cargo is not trimmed further)");
        const outcome enough = commit(0.9f, 0.8f, 1000.0f);
        check(enough.ok && approx(enough.sent, 0.8f),
              "P8.3b cargo 0.8, pool 0.9: sent whole");
    }

    // P8.4 THE SOLVENCY GATE WEIGHS THE TRIMMED COST. Cargo 30 down the 3-edge
    // plains column hauls for 0.02 x 3 x 30 = 1.8; the pool admits 10, a third:
    // 0.6. The source prices iron at 0.01, so the purchase the gate also weighs
    // (BL-1266: the trader pays haul + purchase) is 0.1 trimmed, 0.3 whole. A
    // balance of 1.0 affords the trimmed shipment (0.7) but not the whole one
    // (2.1): it sends 10, paying the 0.6 haul now.
    // RETIRED: P8.4b (the same cargo committed WHOLE — a commanded leg — is
    // refused): the commanded `dispatch_convoy` verb retired with BL-1266, and
    // every shipment now trims.
    {
        const outcome partial = commit(10.0f, 30.0f, 1.0f, /*src_price=*/0.01f);
        check(partial.ok && approx(partial.sent, 10.0f) && approx(partial.spent, 0.6f),
              "P8.4 balance 1.0 between trimmed (0.7) and full (2.1) cost: sends 10, paying 0.6 haul");
        const outcome broke = commit(10.0f, 30.0f, 0.5f, /*src_price=*/0.01f);
        check(!broke.ok && broke.convoys == 0 && broke.spent == 0.0f,
              "P8.4 a balance under even the trimmed cost (0.5 < 0.7) is refused, nothing spent");
    }
}

// ---------------------------------------------------------------------------
// P9 — the trade pass records what each market's anchor has LEFT
// ---------------------------------------------------------------------------
// `market_component::trade_lp_spare` (read by the rival Marketplace build's
// estimate, AI_OPPONENT.md § 11) is the anchor's pool AFTER the pass's
// draws — not the fresh pool. P8.1's fixture, an anchor of 50: the shipment
// draws it down and the source market reads 50 less the cargo sent.
void p9_spare_recorded()
{
    std::printf("\n-- P9  run_trades records each market's Logistic Points left --\n");
    scenario s = make_scenario(/*stock=*/100.0f, 1000.0f);
    recipe_registry reg = make_registry(50.0f);
    enable_auto_trade(s, reg);
    s.w.markets.at(s.dst_market).price[r_iron]  = 10.0f;
    s.w.markets.at(s.dst_market).demand[r_iron] = 30.0f;
    s.w.markets.at(s.dst_market).supply[r_iron] = 0.0f;
    economy_report rep;
    run_trades(s.w, reg, rep);
    float cargo = 0.0f;
    for (const convoy_component& c : s.w.convoys)
        cargo += c.cargo_qty;
    const float spare = s.w.markets.at(s.src_market).trade_lp_spare;
    std::printf("   cargo %.3f, source spare %.3f\n", cargo, spare);
    check(cargo > 0.0f && approx(spare, 50.0f - cargo),
          "P9.1 the source market's trade_lp_spare is its anchor's 50 less the cargo the pass sent");

    // P8.1's anchor of 20, all of it drawn: nothing left.
    scenario s2 = make_scenario(100.0f, 1000.0f);
    recipe_registry reg2 = make_registry(20.0f);
    enable_auto_trade(s2, reg2);
    s2.w.markets.at(s2.dst_market).price[r_iron]  = 10.0f;
    s2.w.markets.at(s2.dst_market).demand[r_iron] = 30.0f;
    s2.w.markets.at(s2.dst_market).supply[r_iron] = 0.0f;
    economy_report rep2;
    run_trades(s2.w, reg2, rep2);
    check(approx(s2.w.markets.at(s2.src_market).trade_lp_spare, 0.0f),
          "P9.2 an anchor the pass drew dry records nothing left");
}

int main()
{
    std::printf("=== logistic_points_convoy_harness (BL-597, LP_PASSIVE_CONVOYS) ===\n");

    p1_granted_draw();
    p2_refused_mutates_nothing();
    p3_no_anchor_no_dispatch();
    p4_space_leg_exempt();
    p5_dispatch_counters();
    p6_shared_pool_contention();
    p7_default_is_private_and_fresh();
    p8_partial_lp_sends();
    p9_spare_recorded();

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
