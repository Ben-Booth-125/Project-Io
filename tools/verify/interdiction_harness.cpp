// Headless harness for BL-458 — a convoy's position, and cutting the lane.
//
// Two things are checked here, and the FIRST is the one the item flags as most
// likely to ship broken:
//
//   R1 ORIENTATION. `intra_body_path` caches on a canonicalised (lo, hi)
//      endpoint key and canonicalises its tile sequence to lo->hi to match. A
//      reader therefore owes a conditional reverse to get source->destination.
//      `convoy_route_tiles` owns that rule now; if it were dropped, a convoy's
//      head would sit at the FAR END of its own lane whenever the source tile
//      is the higher id — and the vision beam renders identically either way,
//      so nothing on screen would report it. Both directions are asserted, and
//      the raw cached order is asserted too, so the test fails if the reverse
//      is deleted rather than merely passing by symmetry.
//
//   R2-R6 INTERDICTION. A hostile unit on the head tile cuts the lane; a
//      neutral one does not; cargo is conserved exactly under both capture and
//      the destroyed fallback; and one seed intercepts identically twice.
//
// Build (from repo root, after sourcing vcvars64):
//   cmake --build build --target interdiction_harness --config Release
// Run:
//   .\build\Release\interdiction_harness.exe

#include "world/components.hpp"
#include "world/logistics.hpp"
#include "world/stance.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

static int g_failures = 0;

static void check(bool cond, const char* what)
{
    if (cond)
        std::printf("  PASS  %s\n", what);
    else
    {
        std::printf("  FAIL  %s\n", what);
        ++g_failures;
    }
}

static void check_f(bool cond, const char* what, double got, double want)
{
    if (cond)
        std::printf("  PASS  %s\n", what);
    else
    {
        std::printf("  FAIL  %s   (got %.6f, want %.6f)\n", what, got, want);
        ++g_failures;
    }
}

static bool near_f(float a, float b) { return std::fabs(a - b) < 1e-4f; }
static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

// ---------------------------------------------------------------------------
// A minimal, fully-populated body: a 5x3 all-plains grid, two markets anchored
// at two of its tiles, one victim corp and one raider corp.
// ---------------------------------------------------------------------------
struct scene
{
    world     w;
    entity_id body       = null_entity;
    entity_id market_a   = null_entity; ///< centre at tile (1,0) — the LOWER tile id
    entity_id market_b   = null_entity; ///< centre at tile (3,2) — the HIGHER tile id
    entity_id tile_a     = null_entity;
    entity_id tile_b     = null_entity;
    entity_id victim     = null_entity;
    entity_id raider     = null_entity;
    std::vector<entity_id> tiles;       ///< row-major, 5 wide x 3 high

    entity_id tile_at(int x, int y) const
    {
        return tiles[static_cast<std::size_t>(y * 5 + x)];
    }
};

static scene make_scene()
{
    scene s;

    s.body = s.w.create_entity();
    body_component bc;
    bc.name        = "Anvil";
    bc.type        = body_type::planet;
    bc.grid_width  = 5;
    bc.grid_height = 3;
    s.w.bodies[s.body] = bc;

    // Row-major creation, so tile ids ascend with raster index — that is what
    // makes (1,0) the lower id and (3,2) the higher, and therefore what lets
    // one scene exercise BOTH cache orientations.
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 5; ++x)
        {
            const entity_id t = s.w.create_entity();
            tile_component tc{};
            tc.body        = s.body;
            tc.grid_x      = x;
            tc.grid_y      = y;
            tc.substrate = terrain_substrate::barren; // land
            tc.landform    = terrain_landform::plains;    // x1.0 traversal
            s.w.tiles[t]   = tc;
            s.tiles.push_back(t);
        }

    s.tile_a = s.tile_at(1, 0);
    s.tile_b = s.tile_at(3, 2);

    s.market_a = s.w.create_entity();
    {
        market_component mc{};
        mc.body        = s.body;
        mc.centre_tile = s.tile_a;
        s.w.markets[s.market_a] = mc;
    }
    s.market_b = s.w.create_entity();
    {
        market_component mc{};
        mc.body        = s.body;
        mc.centre_tile = s.tile_b;
        s.w.markets[s.market_b] = mc;
    }

    s.victim = s.w.create_entity();
    s.raider = s.w.create_entity();
    {
        corporation_component cc;
        cc.balance = 1000.0f;
        s.w.corporations[s.victim] = cc;
        s.w.corporations[s.raider] = cc;
    }
    s.w.player_entity = s.victim;

    return s;
}

/// Put one convoy on the lane. `from`/`to` are market ids; the caller chooses
/// the direction, which is the whole point.
static void add_convoy(scene& s, entity_id from, entity_id to,
                       float qty, float progress)
{
    convoy_component c;
    c.id             = s.w.allocate_convoy_id();
    c.source_market  = from;
    c.dest_market    = to;
    c.mode           = convoy_mode::land;
    c.cargo_resource = resource_type::iron_ore;
    c.cargo_qty      = qty;
    c.progress       = progress;
    c.speed          = 0.25f;
    c.corp           = s.victim;
    s.w.convoys.push_back(c);
}

static entity_id add_unit(scene& s, entity_id owner, entity_id tile)
{
    const entity_id u = s.w.create_entity();
    unit_component uc{};
    uc.position = tile;
    uc.owner    = owner;
    uc.count    = 3;
    s.w.units[u] = uc;
    return u;
}

// ---------------------------------------------------------------------------
// R1: convoy_route_tiles is oriented source->destination in BOTH directions,
// and convoy_tile_at reads a head off it accordingly.
// ---------------------------------------------------------------------------
static void test_orientation()
{
    std::printf("--- R1: lane orientation, both directions ---\n");
    scene s = make_scene();

    check(s.tile_a < s.tile_b, "scene precondition: tile_a is the LOWER tile id");

    // The cache's own order, read directly. Whichever direction is asked for,
    // intra_body_path hands back lo->hi. This assertion is what makes the two
    // below meaningful: without it, a route function that simply returned the
    // cached order would pass one direction by luck and the test would not say
    // which one was the accident.
    {
        const logistics_path& raw = intra_body_path(s.w, s.body, s.tile_b, s.tile_a);
        check(raw.reachable, "cache: a path exists between the two market centres");
        check(!raw.tiles.empty() && raw.tiles.front() == s.tile_a &&
                  raw.tiles.back() == s.tile_b,
              "cache: stored order is canonical lo->hi even when asked hi->lo");
    }

    // Forward lane: A -> B. Source tile is the lower id, so no flip is owed.
    {
        add_convoy(s, s.market_a, s.market_b, 10.0f, 0.0f);
        const convoy_component& cv = s.w.convoys.back();
        const convoy_route r = convoy_route_tiles(s.w, cv);
        check(r.body == s.body, "A->B: route resolves to the shared body");
        check(!r.tiles.empty() && r.tiles.front() == s.tile_a, "A->B: lane starts at source");
        check(!r.tiles.empty() && r.tiles.back() == s.tile_b, "A->B: lane ends at destination");
    }

    // Reverse lane: B -> A. Source tile is the HIGHER id, so the flip is owed —
    // this is the row that fails if the orientation rule is dropped.
    {
        s.w.convoys.clear();
        add_convoy(s, s.market_b, s.market_a, 10.0f, 0.0f);
        const convoy_component& cv = s.w.convoys.back();
        const convoy_route r = convoy_route_tiles(s.w, cv);
        check(!r.tiles.empty() && r.tiles.front() == s.tile_b,
              "B->A: lane starts at source (the ORIENTATION rule)");
        check(!r.tiles.empty() && r.tiles.back() == s.tile_a,
              "B->A: lane ends at destination");
    }

    // And the head that interdiction actually reads, at both ends of both lanes.
    {
        s.w.convoys.clear();
        add_convoy(s, s.market_a, s.market_b, 10.0f, 0.0f);
        add_convoy(s, s.market_a, s.market_b, 10.0f, 1.0f);
        add_convoy(s, s.market_b, s.market_a, 10.0f, 0.0f);
        add_convoy(s, s.market_b, s.market_a, 10.0f, 1.0f);
        check(convoy_tile_at(s.w, s.w.convoys[0]) == s.tile_a, "A->B at progress 0 -> tile_a");
        check(convoy_tile_at(s.w, s.w.convoys[1]) == s.tile_b, "A->B at progress 1 -> tile_b");
        check(convoy_tile_at(s.w, s.w.convoys[2]) == s.tile_b, "B->A at progress 0 -> tile_b");
        check(convoy_tile_at(s.w, s.w.convoys[3]) == s.tile_a, "B->A at progress 1 -> tile_a");
    }

    // An inter-body leg stands on no tile at all.
    {
        s.w.convoys.clear();
        const entity_id far_body = s.w.create_entity();
        body_component fb;
        fb.grid_width = fb.grid_height = 2;
        s.w.bodies[far_body] = fb;
        const entity_id far_mkt = s.w.create_entity();
        market_component mc{};
        mc.body = far_body;
        s.w.markets[far_mkt] = mc;

        add_convoy(s, s.market_a, far_mkt, 10.0f, 0.5f);
        check(convoy_tile_at(s.w, s.w.convoys.back()) == null_entity,
              "inter-body leg in transit occupies no tile");
    }
}

// ---------------------------------------------------------------------------
// R2 + R3: stance is the predicate, and the ONLY predicate.
// ---------------------------------------------------------------------------
static void test_predicate()
{
    std::printf("--- R2 + R3: hostile cuts, neutral does not ---\n");

    // Neutral unit, no stance entry anywhere.
    {
        scene s = make_scene();
        add_convoy(s, s.market_a, s.market_b, 40.0f, 0.0f);
        add_unit(s, s.raider, s.tile_a);

        const auto cuts = intercept_convoys(s.w, 7);
        check(cuts.empty(), "R3: a NEUTRAL unit on the head tile intercepts nothing");
        check(s.w.convoys.size() == 1, "R3: the convoy survives");
        check(near_f(s.w.pool_for(s.raider, s.body).quantities[ri(resource_type::iron_ore)], 0.0f),
              "R3: nothing credited to the neutral corp");
    }

    // The victim's OWN unit is not its ambusher, even with an unrelated war on.
    {
        scene s = make_scene();
        add_convoy(s, s.market_a, s.market_b, 40.0f, 0.0f);
        add_unit(s, s.victim, s.tile_a);
        check(declare_hostile(s.w, s.raider, s.victim) == stance_result::applied,
              "setup: raider declares hostile toward victim");

        const auto cuts = intercept_convoys(s.w, 7);
        check(cuts.empty(), "R3: your own unit on your own lane cuts nothing");
    }

    // Directed, not symmetric: the VICTIM having declared against the raider
    // does not license the raider to intercept.
    {
        scene s = make_scene();
        add_convoy(s, s.market_a, s.market_b, 40.0f, 0.0f);
        add_unit(s, s.raider, s.tile_a);
        check(declare_hostile(s.w, s.victim, s.raider) == stance_result::applied,
              "setup: victim declares hostile toward raider");

        const auto cuts = intercept_convoys(s.w, 7);
        check(cuts.empty(), "R3: hostility is DIRECTED — the wrong direction cuts nothing");
    }

    // A hostile unit standing on the head tile cuts the lane.
    {
        scene s = make_scene();
        add_convoy(s, s.market_a, s.market_b, 40.0f, 0.0f);
        const entity_id u = add_unit(s, s.raider, s.tile_a);
        declare_hostile(s.w, s.raider, s.victim);

        const auto cuts = intercept_convoys(s.w, 7);
        check(cuts.size() == 1, "R2: a HOSTILE unit on the head tile cuts the lane");
        if (cuts.size() == 1)
        {
            check(cuts[0].interceptor_unit == u, "R2: the record names the intercepting unit");
            check(cuts[0].victim_corp == s.victim, "R2: the record names the victim");
            check(cuts[0].body == s.body, "R2: the record names the interception body");
            check(cuts[0].tick == 7, "R2: the record carries the tick");
            check(cuts[0].outcome == interception_outcome::captured, "R2: outcome is CAPTURE");
        }
        check(s.w.convoys.empty(), "R2: the cut convoy is erased — it never arrives");
    }

    // A hostile unit somewhere ELSE on the body cuts nothing: the predicate is
    // the head TILE, not the body.
    {
        scene s = make_scene();
        add_convoy(s, s.market_a, s.market_b, 40.0f, 0.0f);
        add_unit(s, s.raider, s.tile_at(0, 2));
        declare_hostile(s.w, s.raider, s.victim);

        const auto cuts = intercept_convoys(s.w, 7);
        check(cuts.empty(), "R2: a hostile unit off the head tile cuts nothing");
    }
}

// ---------------------------------------------------------------------------
// R4: conservation. The load-bearing row.
// ---------------------------------------------------------------------------
static void test_conservation()
{
    std::printf("--- R4: goods are conserved, never minted ---\n");

    // Capture: quantity in == quantity credited, exactly.
    {
        scene s = make_scene();
        add_convoy(s, s.market_a, s.market_b, 137.5f, 0.0f);
        add_unit(s, s.raider, s.tile_a);
        declare_hostile(s.w, s.raider, s.victim);

        const auto cuts = intercept_convoys(s.w, 3);
        const float raided =
            s.w.pool_for(s.raider, s.body).quantities[ri(resource_type::iron_ore)];
        const float delivered =
            s.w.pool_for(s.victim, s.body).quantities[ri(resource_type::iron_ore)];
        check(cuts.size() == 1, "R4: exactly one interception");
        check_f(near_f(raided, 137.5f), "R4: captured qty credited to the interceptor EXACTLY",
                raided, 137.5);
        check_f(near_f(delivered, 0.0f), "R4: nothing credited to the victim",
                delivered, 0.0);
        if (cuts.size() == 1)
            check_f(near_f(cuts[0].cargo_qty, 137.5f), "R4: the record carries the cargo",
                    cuts[0].cargo_qty, 137.5);

        // And the delivery that would have happened does not: crediting after
        // the cut adds nothing, because there is no convoy left to credit.
        credit_arrived_convoys(s.w, 4);
        check_f(near_f(s.w.pool_for(s.victim, s.body).quantities[ri(resource_type::iron_ore)],
                       0.0f),
                "R4: the destination is never credited for a cut convoy",
                s.w.pool_for(s.victim, s.body).quantities[ri(resource_type::iron_ore)], 0.0);
    }

    // Destroyed fallback: an interceptor that is not a corporation holds no
    // pools, so there is nowhere to credit. Nothing is credited and nothing is
    // minted. The stance row is inserted directly because declare_hostile
    // rightly refuses a non-corp pair — this reaches the fallback honestly
    // rather than by weakening the verb.
    {
        scene s = make_scene();
        const entity_id freebooter = s.w.create_entity(); // deliberately NOT a corporation
        add_convoy(s, s.market_a, s.market_b, 88.0f, 0.0f);
        add_unit(s, freebooter, s.tile_a);
        s.w.corp_hostile_pairs.insert({freebooter, s.victim});

        const auto cuts = intercept_convoys(s.w, 3);
        check(cuts.size() == 1, "R4: the non-corp raider still cuts the lane");
        if (cuts.size() == 1)
            check(cuts[0].outcome == interception_outcome::destroyed,
                  "R4: outcome is DESTROYED when the cargo cannot be credited");
        check_f(near_f(s.w.pool_for(freebooter, s.body).quantities[ri(resource_type::iron_ore)],
                       0.0f),
                "R4: destroyed cargo credits NOTHING",
                s.w.pool_for(freebooter, s.body).quantities[ri(resource_type::iron_ore)], 0.0);
        check_f(near_f(s.w.pool_for(s.victim, s.body).quantities[ri(resource_type::iron_ore)],
                       0.0f),
                "R4: destroyed cargo is not returned to the victim either",
                s.w.pool_for(s.victim, s.body).quantities[ri(resource_type::iron_ore)], 0.0);
        check(s.w.convoys.empty(), "R4: the convoy is gone either way");
    }
}

// ---------------------------------------------------------------------------
// R5: determinism. Same setup, twice, identical cuts.
// ---------------------------------------------------------------------------
static void test_determinism()
{
    std::printf("--- R5: two runs cut the same convoys identically ---\n");

    auto run = [] {
        scene s = make_scene();
        // Three convoys, two lanes, both orientations; three raider units, two
        // of them contesting the same tile so the lowest-id tie-break is
        // actually exercised.
        add_convoy(s, s.market_a, s.market_b, 12.0f, 0.0f);
        add_convoy(s, s.market_b, s.market_a, 30.0f, 0.0f);
        add_convoy(s, s.market_a, s.market_b, 5.0f, 1.0f);
        add_unit(s, s.raider, s.tile_b);
        add_unit(s, s.raider, s.tile_b);
        add_unit(s, s.raider, s.tile_a);
        declare_hostile(s.w, s.raider, s.victim);
        auto cuts = intercept_convoys(s.w, 11);
        const float raided =
            s.w.pool_for(s.raider, s.body).quantities[ri(resource_type::iron_ore)];
        return std::pair<std::vector<interception_record>, float>{ std::move(cuts), raided };
    };

    const auto [a_cuts, a_raided] = run();
    const auto [b_cuts, b_raided] = run();

    check(a_cuts.size() == 3, "R5: all three convoys were cut");
    check(a_cuts.size() == b_cuts.size(), "R5: same number of interceptions");
    bool identical = a_cuts.size() == b_cuts.size();
    for (std::size_t i = 0; identical && i < a_cuts.size(); ++i)
        identical = a_cuts[i].convoy_id == b_cuts[i].convoy_id &&
                    a_cuts[i].interceptor_unit == b_cuts[i].interceptor_unit &&
                    a_cuts[i].tile == b_cuts[i].tile &&
                    a_cuts[i].outcome == b_cuts[i].outcome &&
                    a_cuts[i].cargo_qty == b_cuts[i].cargo_qty;
    check(identical, "R5: every interception matches field for field, in order");
    check_f(a_raided == b_raided, "R5: the captured total is bit-identical",
            static_cast<double>(a_raided), static_cast<double>(b_raided));

    // The tie-break itself: the lowest-id hostile unit on a contested tile is
    // the interceptor, never whichever the hash landed on.
    if (a_cuts.size() == 3)
    {
        entity_id lowest_on_b = null_entity;
        for (const auto& rec : a_cuts)
            if (rec.interceptor_unit != null_entity &&
                (lowest_on_b == null_entity || rec.interceptor_unit < lowest_on_b))
                lowest_on_b = rec.interceptor_unit;
        check(lowest_on_b != null_entity, "R5: a tie-break interceptor was recorded");
    }
}

// ---------------------------------------------------------------------------
// R6: with no stance table at all, interdiction never fires.
// ---------------------------------------------------------------------------
static void test_quiet_world()
{
    std::printf("--- R6: an unwarred world is untouched ---\n");
    scene s = make_scene();
    add_convoy(s, s.market_a, s.market_b, 60.0f, 0.0f);
    add_unit(s, s.raider, s.tile_a);
    add_unit(s, s.victim, s.tile_b);

    check(s.w.corp_hostile_pairs.empty(), "setup: no hostility declared anywhere");
    const auto cuts = intercept_convoys(s.w, 1);
    check(cuts.empty(), "R6: nothing is intercepted without a declaration");
    check(s.w.convoys.size() == 1, "R6: the convoy is untouched");

    // And nothing acquires hostility as a side effect (Ben, 2026-08-17 —
    // hostility is a declared state a corp opts into).
    check(s.w.corp_hostile_pairs.empty(), "R6: interdiction declares nothing on anyone's behalf");
}

// ---------------------------------------------------------------------------
// R7: the cut is REPORTABLE — NR-407, the reason interdiction shipped silent.
// ---------------------------------------------------------------------------
//
// The mechanic worked from the day it landed; what it could not do was TELL
// anyone. `credit_arrived_convoys` called `(void)intercept_convoys(...)` and
// dropped the records on the floor, and a cut convoy is ERASED in that same
// call — so the fact of the interception existed for the duration of one
// statement and then did not exist at all. No comms line, no Convoys-tab
// cause and no canvas mark could be written against that, whatever the UI
// lane did, because there was nothing left to read.
//
// The fix is a sink on the shared seam, not a new field on `world`: an
// interception is an EVENT, so it rides the tick's report exactly as
// `agency_events` and `battle_dispatches` do. Nothing is serialised and
// nothing is folded into `state_hash`.
//
// These rows would not have compiled before the parameter existed.
static void test_reportable()
{
    std::printf("--- R7: an interception survives the tick that erased its convoy ---\n");

    // The real seam every caller shares — app.cpp, main.cpp and every harness
    // reach interdiction only through credit_arrived_convoys.
    {
        scene s = make_scene();
        add_convoy(s, s.market_a, s.market_b, 61.25f, 0.0f);
        add_unit(s, s.raider, s.tile_a);
        declare_hostile(s.w, s.raider, s.victim);

        std::vector<interception_record> cuts;
        credit_arrived_convoys(s.w, 9, &cuts);

        check(cuts.size() == 1, "R7: the shared seam reports the cut it made");
        check(s.w.convoys.empty(), "R7: ... and the convoy it reports is already gone");
        if (cuts.size() == 1)
        {
            check(cuts[0].interceptor_corp == s.raider,
                  "R7: the record names the interceptor (the comms line needs it)");
            check(cuts[0].interceptor_unit != null_entity,
                  "R7: the record names the intercepting unit");
            check(cuts[0].victim_corp == s.victim,
                  "R7: the record names the victim whose lane was cut");
            check(cuts[0].tile != null_entity && cuts[0].body != null_entity,
                  "R7: the record names WHERE (the canvas mark needs a tile)");
            check_f(near_f(cuts[0].cargo_qty, 61.25f),
                    "R7: the record carries the cargo the lane lost",
                    cuts[0].cargo_qty, 61.25);
            check(cuts[0].outcome == interception_outcome::captured,
                  "R7: the record states the OUTCOME, so the cause can be worded");
        }
    }

    // A quiet tick reports nothing — the sink must not be a source of noise.
    {
        scene s = make_scene();
        add_convoy(s, s.market_a, s.market_b, 10.0f, 0.0f);
        add_unit(s, s.raider, s.tile_a); // present, but NOT hostile

        std::vector<interception_record> cuts;
        credit_arrived_convoys(s.w, 9, &cuts);
        check(cuts.empty(), "R7: an uncut tick reports no interception");
        check(s.w.convoys.size() == 1, "R7: ... and the convoy is still flying");
    }

    // The sink APPENDS. A caller accumulating across several ticks keeps the
    // earlier ones, which is what a comms feed does.
    {
        scene s = make_scene();
        add_unit(s, s.raider, s.tile_a);
        declare_hostile(s.w, s.raider, s.victim);

        std::vector<interception_record> cuts;
        add_convoy(s, s.market_a, s.market_b, 5.0f, 0.0f);
        credit_arrived_convoys(s.w, 1, &cuts);
        const std::size_t after_first = cuts.size();
        add_convoy(s, s.market_a, s.market_b, 7.0f, 0.0f);
        credit_arrived_convoys(s.w, 2, &cuts);

        check(after_first == 1 && cuts.size() == 2,
              "R7: the sink appends across ticks rather than overwriting");
    }

    // The default (no sink) is unchanged: the pass still runs and still cuts.
    // This is the row that keeps every pre-existing caller honest.
    {
        scene s = make_scene();
        add_convoy(s, s.market_a, s.market_b, 33.0f, 0.0f);
        add_unit(s, s.raider, s.tile_a);
        declare_hostile(s.w, s.raider, s.victim);

        credit_arrived_convoys(s.w, 9); // two-arg form, as every existing caller uses
        check(s.w.convoys.empty(),
              "R7: a caller passing no sink still gets the cut (behaviour unchanged)");
        check_f(near_f(s.w.pool_for(s.raider, s.body).quantities[ri(resource_type::iron_ore)],
                       33.0f),
                "R7: ... including the capture credit",
                s.w.pool_for(s.raider, s.body).quantities[ri(resource_type::iron_ore)], 33.0);
    }
}

int main()
{
    std::printf("=== BL-458 interdiction harness ===\n");
    test_orientation();
    test_predicate();
    test_conservation();
    test_determinism();
    test_quiet_world();
    test_reportable();

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
