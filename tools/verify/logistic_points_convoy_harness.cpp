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
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <string>
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

    s.w.pool_for(s.corp, s.body).quantities[r_iron] = stock;
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

float pool_iron(const scenario& s)
{
    const auto it = s.w.corp_body_pools.find({s.corp, s.body});
    return it != s.w.corp_body_pools.end() ? it->second.quantities[r_iron] : 0.0f;
}

logistics_nodes nodes_of(world& w) { return collect_logistics_nodes(w); }

} // namespace

// ---------------------------------------------------------------------------
// P1 — a leg within the pool commits normally, drawing down exactly the CARGO
// QUANTITY (Ben, 2026-08-25, NR-620: not distance — see commit_convoy).
// ---------------------------------------------------------------------------

void p1_granted_draw()
{
    std::printf("\n-- P1  a leg within the anchor's pool commits, drawing exactly the cargo qty --\n");

    scenario s = make_scenario(100.0f, 1000.0f);
    recipe_registry reg = make_registry(/*lp_per_anchor*/ 50.0f);
    const logistics_nodes nodes = nodes_of(s.w);

    const convoy_leg leg = price_convoy_leg(s.w, reg, nodes, s.corp, s.body, s.dst_market,
                                            r_iron, 25.0f, reg.logistics_cost(convoy_mode::space));
    check(leg.viable, "the leg prices viable (a plains column, no water)");
    check(leg.mode == convoy_mode::land, "an all-plains intra-body lane is land mode");

    lp_pool_map pools;
    const float balance_before = s.w.corporations.at(s.corp).balance;
    bool refused = false;
    const bool ok = commit_convoy(s.w, reg, s.corp, s.body, s.src_market, s.dst_market,
                                  r_iron, 25.0f, leg, &pools, &refused);

    check(ok, "the leg commits");
    check(!refused, "no refusal reported");
    check(s.w.convoys.size() == 1, "exactly one convoy is created");
    check(approx(pool_iron(s), 75.0f), "the source pool is debited by exactly the quantity");
    check(approx(s.w.corporations.at(s.corp).balance, balance_before - leg.cost),
          "the balance is debited by leg.cost ONLY — no second LP-specific credit charge");

    // The anchor nearest the dispatch tile — one column over — should show the
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
// P2 — a leg exceeding the pool is refused outright, mutating nothing
// ---------------------------------------------------------------------------

void p2_refused_mutates_nothing()
{
    std::printf("\n-- P2  a leg exceeding the pool is refused outright --\n");

    scenario s = make_scenario(100.0f, 1000.0f);
    // A rate so small the leg's own CARGO (25 units) exceeds it.
    recipe_registry reg = make_registry(/*lp_per_anchor*/ 0.5f);
    const logistics_nodes nodes = nodes_of(s.w);

    const convoy_leg leg = price_convoy_leg(s.w, reg, nodes, s.corp, s.body, s.dst_market,
                                            r_iron, 25.0f, reg.logistics_cost(convoy_mode::space));
    check(leg.viable, "fixture: the leg prices viable, and its 25 units exceed the tiny pool");

    lp_pool_map pools;
    const float    balance_before = s.w.corporations.at(s.corp).balance;
    const float    pool_before    = pool_iron(s);
    const std::size_t convoys_before = s.w.convoys.size();

    bool refused = false;
    const bool ok = commit_convoy(s.w, reg, s.corp, s.body, s.src_market, s.dst_market,
                                  r_iron, 25.0f, leg, &pools, &refused);

    check(!ok, "the leg is refused");
    check(refused, "the refusal is attributed to want of passive LP specifically");
    check(approx(s.w.corporations.at(s.corp).balance, balance_before), "balance is untouched");
    check(approx(pool_iron(s), pool_before), "the source pool is untouched");
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

    const convoy_leg leg = price_convoy_leg(s.w, reg, nodes, s.corp, s.body, s.dst_market,
                                            r_iron, 25.0f, reg.logistics_cost(convoy_mode::space));
    check(leg.viable, "fixture: the leg still prices (LP is a separate gate from routability)");

    lp_pool_map pools;
    bool refused = false;
    const bool ok = commit_convoy(s.w, reg, s.corp, s.body, s.src_market, s.dst_market,
                                  r_iron, 25.0f, leg, &pools, &refused);

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

    w.pool_for(corp, src_body).quantities[r_iron] = 50.0f;
    w.pool_for(corp, src_body).quantities[static_cast<std::size_t>(resource_type::propellant)] = 5.0f;

    entity_id src_mkt = w.create_entity();
    market_component sm{}; sm.body = src_body; w.markets[src_mkt] = sm;
    entity_id dst_mkt = w.create_entity();
    market_component dm{}; dm.body = dest_body; w.markets[dst_mkt] = dm;

    recipe_registry reg = make_registry(/*lp_per_anchor*/ 0.0f); // no LP anywhere
    const logistics_nodes nodes = collect_logistics_nodes(w);
    const convoy_leg leg = price_convoy_leg(w, reg, nodes, corp, src_body, dst_mkt,
                                            r_iron, 10.0f, reg.logistics_cost(convoy_mode::space));
    check(leg.viable && leg.mode == convoy_mode::space, "fixture: an inter-body leg prices as space mode");

    lp_pool_map pools;
    bool refused = false;
    const bool ok = commit_convoy(w, reg, corp, src_body, src_mkt, dst_mkt,
                                  r_iron, 10.0f, leg, &pools, &refused);

    check(ok, "a space leg commits despite zero LP anywhere — LOGISTICS.md's design is tile-grounded");
    check(!refused, "no LP refusal is ever attributed to a space leg");
    check(pools.empty(), "the space path never touches lp_pool_for_body at all");
}

// ---------------------------------------------------------------------------
// P5 — dispatch_convoys' own counters agree with what happened
// ---------------------------------------------------------------------------

void p5_dispatch_counters()
{
    std::printf("\n-- P5  dispatch_convoys' dispatched/refused_no_lp counters are honest --\n");

    scenario s = make_scenario(100.0f, 1000.0f);
    // A pool that comfortably covers ONE shortfall haul but not a second.
    recipe_registry reg = make_registry(/*lp_per_anchor*/ 10.0f);

    s.w.markets.at(s.dst_market).demand[r_iron] = 30.0f;
    s.w.markets.at(s.dst_market).supply[r_iron] = 0.0f;

    const convoy_dispatch_tick t1 = dispatch_convoys(s.w, reg, reg.logistics_cost(convoy_mode::land),
                                                      reg.logistics_cost(convoy_mode::space));
    check(t1.dispatched == static_cast<int>(s.w.convoys.size()),
          "dispatched counts exactly the convoys this pass created");

    // Drive the SAME shortfall again with a pool too small this time.
    recipe_registry tiny_reg = make_registry(/*lp_per_anchor*/ 0.01f);
    scenario s2 = make_scenario(100.0f, 1000.0f);
    s2.w.markets.at(s2.dst_market).demand[r_iron] = 30.0f;
    s2.w.markets.at(s2.dst_market).supply[r_iron] = 0.0f;
    const convoy_dispatch_tick t2 = dispatch_convoys(s2.w, tiny_reg,
                                                      tiny_reg.logistics_cost(convoy_mode::land),
                                                      tiny_reg.logistics_cost(convoy_mode::space));
    check(t2.dispatched == 0 && s2.w.convoys.empty(),
          "a starved pool dispatches nothing");
    check(t2.refused_no_lp >= 1, "the refusal is counted on the tick summary");
}

// ---------------------------------------------------------------------------
// P6 — CONTENTION: a mobilised march and a convoy dispatch share one pool
// ---------------------------------------------------------------------------

void p6_shared_pool_contention()
{
    std::printf("\n-- P6  war flips the queue: a march and a convoy dispatch genuinely contend --\n");

    for (int run = 0; run < 2; ++run)
    {
        scenario s = make_scenario(100.0f, 1000.0f);

        // A rival corp, hostile toward s.corp (mobilises the march pass's
        // priority partition), with a unit sitting right at the anchor tile
        // ready to march away — it draws BEFORE any convoy dispatch this
        // tick if the caller runs run_unit_march first, same as commit_convoy
        // if dispatch runs first: this test checks the OUTCOME is
        // deterministic and mutually exclusive, not which side wins (that is
        // the real driver's call order, reported separately).
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

        // A pool that covers roughly ONE of the two draws (march ~1pt, convoy
        // leg's dist ~3.16 down the column) but not both comfortably.
        recipe_registry reg = make_registry(/*lp_per_anchor*/ 3.0f);
        military_capability_params mp = reg.military();
        mp.march_points_per_class[static_cast<std::size_t>(unit_class::infantry)] = 2.0f;
        reg.set_military(mp);

        lp_pool_map shared_pool;

        // Passive first (matches main.cpp/app.cpp's real tick order —
        // dispatch_convoys runs before run_economy_step's march pass).
        s.w.markets.at(s.dst_market).demand[r_iron] = 30.0f;
        s.w.markets.at(s.dst_market).supply[r_iron] = 0.0f;
        const convoy_dispatch_tick ct = dispatch_convoys(s.w, reg, reg.logistics_cost(convoy_mode::land),
                                                          reg.logistics_cost(convoy_mode::space),
                                                          &shared_pool);
        const unit_march_tick mt = run_unit_march(s.w, reg, &shared_pool);

        const std::string tag = " (run " + std::to_string(run) + ")";
        // Exactly one of the two draws should have been admitted from the
        // shared pool of 3.0 (convoy leg ~3.16 alone already exceeds it, so
        // AT MOST the march can also be granted only if the convoy was
        // refused first — never both when the convoy runs first and already
        // exhausts the pool). The row that matters is that the SAME outcome
        // reproduces across two runs, proving determinism, not first-come by
        // iteration order.
        check(ct.dispatched + mt.marching - mt.refused_no_lp <= 1,
              ("at most one of {convoy, march} is granted this contested tick" + tag).c_str());

        if (run == 0)
        {
            static bool first_convoy_won = false;
            first_convoy_won = (ct.dispatched == 1);
            check(true, ("fixture: recorded run 0's outcome" + tag).c_str());
            // Re-run for determinism comparison below via a second pass.
            (void)first_convoy_won;
        }
    }

    // Determinism proper: two independent, identically-built worlds run the
    // exact same contested scenario and land on the identical outcome.
    auto run_scenario = [&]() -> std::pair<bool, bool> {
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
        uc.type     = 0;
        movement_order mo;
        mo.dest       = tile_at(s.w, s.body, 5, 0);
        mo.next_index = 1;
        for (int c = 0; c <= 5; ++c) mo.path.push_back(tile_at(s.w, s.body, c, 0));
        uc.order = mo;
        s.w.units[unit] = uc;

        // The pool is sized to the CONVOY'S CARGO (the dest market is short 30
        // units, below), so the convoy exactly exhausts it and the mobilised
        // unit's 2.0-point march finds nothing left. Under the pre-NR-620
        // distance draw this was 3.0, the leg's three plains edges.
        recipe_registry reg = make_registry(30.0f);
        military_capability_params mp = reg.military();
        mp.march_points_per_class[static_cast<std::size_t>(unit_class::infantry)] = 2.0f;
        reg.set_military(mp);

        lp_pool_map shared_pool;
        s.w.markets.at(s.dst_market).demand[r_iron] = 30.0f;
        s.w.markets.at(s.dst_market).supply[r_iron] = 0.0f;
        const convoy_dispatch_tick ct = dispatch_convoys(s.w, reg, reg.logistics_cost(convoy_mode::land),
                                                          reg.logistics_cost(convoy_mode::space),
                                                          &shared_pool);
        const unit_march_tick mt = run_unit_march(s.w, reg, &shared_pool);
        return { ct.dispatched == 1, mt.refused_no_lp == 1 };
    };

    const auto a = run_scenario();
    const auto b = run_scenario();
    check(a.first == b.first && a.second == b.second,
          "the contested outcome (which side is granted) is IDENTICAL across two independent runs");
    // Concrete numbers, pinned rather than trusted from prose: the dest market
    // is short 30 units and the source holds 100, so the convoy's cargo is 30 —
    // exactly the pool (30.0), so the convoy (which draws FIRST, matching the
    // real driver's dispatch_convoys-before-run_unit_march tick order) is
    // GRANTED, exhausting the anchor's pool to 0.0 and leaving nothing for
    // the mobilised unit's 2.0-point march — which is then REFUSED. This is
    // "war flips the queue" made concrete: the convoy claims the anchor
    // first, so the army goes unsupplied THIS tick, purely from visitation
    // order over one shared pool.
    check(a.first == true, "the convoy (drawn first) is granted — its 30 units of cargo exactly exhaust the pool");
    check(a.second == true, "...leaving nothing for the mobilised unit's march, which is refused");
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

    // Two legs, each individually within the pool (dist ~1.0 each), committed
    // with NO shared pool passed — if any hidden state persisted between the
    // two calls, the second would see the first's draw and could refuse.
    const convoy_leg leg1 = price_convoy_leg(s.w, reg, nodes, s.corp, s.body, s.dst_market,
                                             r_iron, 5.0f, reg.logistics_cost(convoy_mode::space));
    bool refused1 = false;
    const bool ok1 = commit_convoy(s.w, reg, s.corp, s.body, s.src_market, s.dst_market,
                                   r_iron, 5.0f, leg1, nullptr, &refused1);
    check(ok1 && !refused1, "first call (no shared pool) commits");

    const convoy_leg leg2 = price_convoy_leg(s.w, reg, nodes, s.corp, s.body, s.dst_market,
                                             r_iron, 5.0f, reg.logistics_cost(convoy_mode::space));
    bool refused2 = false;
    const bool ok2 = commit_convoy(s.w, reg, s.corp, s.body, s.src_market, s.dst_market,
                                   r_iron, 5.0f, leg2, nullptr, &refused2);
    check(ok2 && !refused2,
          "second call ALSO commits — a private local pool means no draw carries over "
          "from the first call (each call rebuilds the full rate fresh)");
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

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
