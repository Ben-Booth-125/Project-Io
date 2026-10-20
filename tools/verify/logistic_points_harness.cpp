// ---------------------------------------------------------------------------
// Headless march Logistic Points harness (BL-1352, LP_ONE_SYSTEM)
// No SDL / Lua / ImGui — builds over the world/* logic alone.
// ---------------------------------------------------------------------------
// LOGISTICS.md § Logistic Points is the authority, and § How a tile's points get
// to it holds the rule under test: "A march draws on the nation's pool too, and
// claims first". A marching unit draws LOGISTIC POINTS FROM ITS HOST NATION'S
// POOL — the one pool hubs, stations and ports make and production consumes —
// ahead of every producer, and a granted march is priced in credits separately
// from the points themselves.
//
// WHAT THIS FILE USED TO BE (BL-596): the same six rows against
// `active_lp_anchor_pools`, a flat rate at EVERY POPULATION CENTRE. That was a
// second, unrelated Logistic Point system, and BL-1352 retired it — so these rows
// are rewritten rather than deleted: the questions they asked are still the right
// ones, they are just asked of the pool that exists.
//
//   L1  ONE SYSTEM: a city makes NO points (LOGISTICS.md, "Cities make none"),
//       a running hub does, and a nation's pool is exactly its sources' points.
//   L2  A march WITHIN the pool: the unit moves exactly as
//       march_points_per_class alone would predict, credits are debited, and the
//       nation's row records the claim as granted.
//   L3  A march the pool cannot cover is refused outright: the unit's
//       order/position/progress and the owning corp's balance are all
//       BYTE-IDENTICAL to the pre-tick snapshot, and the refusal is counted
//       (unit_march_tick::refused_no_lp).
//   L4  WHOSE POOL: THE MOVER'S. A nation with no source refuses its own marches;
//       a unit standing on UNOWNED ground still marches on its own nation's points
//       (it is not stranded there); and a unit invading another nation draws ITS
//       OWN nation's pool, never the invaded nation's.
//   L5  CONTENTION, in the march's own order: two corps' claims against a pool
//       that covers only one are served mobilised-first then by ascending unit
//       id, identically across two independent runs.
//   L6  THE MARCH CLAIMS FIRST: with a consumer on the same ground, production's
//       granted fraction falls by exactly the points the march took, and the
//       same scenario run twice in one process gives the same answer both times
//       (the pool is a rate, recomputed fresh, never inherited).
//
// The process exits non-zero if any assertion FAILs.

#include "world/economy_system.hpp"
#include "world/logistic_points.hpp"
#include "world/logistics.hpp"
#include "world/recipe_registry.hpp"
#include "world/river_generation.hpp" // hex_side_for_offset - L7 lays its roads by side
#include "world/stance.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

bool approx(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

constexpr uint16_t ROW_INFANTRY = 0; // Levy Spear — unit_class::infantry (unit_roster.cpp)

/// A gw-wide single-row land body, mirroring unit_march_harness.cpp's own
/// make_row_body. No nation and no sources: every test says what ground belongs
/// to whom and what stands on it, because that is what it is testing.
entity_id make_row_body(world& w, int gw, terrain_landform lf = terrain_landform::plains)
{
    const entity_id body = w.create_entity();
    body_component bc{};
    bc.grid_width  = gw;
    bc.grid_height = 1;
    w.bodies[body] = bc;
    for (int c = 0; c < gw; ++c)
    {
        const entity_id t = w.create_entity();
        tile_component tc{};
        tc.body        = body;
        tc.grid_x      = c;
        tc.grid_y      = 0;
        tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass; tc.cover_density = 150;
        tc.landform    = lf;
        w.tiles[t]     = tc;
    }
    return body;
}

entity_id tile_at(world& w, entity_id body, int c)
{
    return body_tile_grid(w, body)[static_cast<std::size_t>(c)];
}

/// A nation holding columns [from, to] of @p body's row.
entity_id add_nation(world& w, entity_id body, int from, int to, const char* name = "Fixture")
{
    const entity_id n = w.create_entity();
    nation_component nc{};
    nc.name = name;
    for (int c = from; c <= to; ++c)
    {
        const entity_id t = tile_at(w, body, c);
        nc.tiles.push_back(t);
        w.tile_to_nation[t] = n;
    }
    w.nations[n] = nc;
    return n;
}

/// A city on @p tile. It makes NO Logistic Points — that is L1's subject — and is
/// here so the harness can prove it rather than assume it.
void add_city(world& w, entity_id tile)
{
    w.population_centre_tile[w.create_entity()] = tile;
}

/// A complete, running building of @p type on @p tile.
entity_id add_building(world& w, entity_id tile, building_type type)
{
    const entity_id b = w.create_entity();
    building_component bc{};
    bc.type            = type;
    bc.tile            = tile;
    bc.ticks_remaining = 0;
    w.buildings[b]     = bc;
    return b;
}

/// A running inland logistics hub: a ROAD-network source of `lp_source_points[0]`.
entity_id add_hub(world& w, entity_id tile)
{
    return add_building(w, tile, building_type::inland_logistics_hub);
}

/// A producing extraction site asking for `lp_need_per_run x assigned x target/100`
/// points, owned by @p owner (so the resolver counts its claim).
entity_id add_consumer(world& w, entity_id tile, entity_id owner, float assigned, float target)
{
    const entity_id b = add_building(w, tile, building_type::extraction_site);
    building_component& bc = w.buildings.at(b);
    bc.workforce_assigned  = assigned;
    bc.workforce_target    = target;
    if (const auto cit = w.corporations.find(owner); cit != w.corporations.end())
        cit->second.assets.push_back(b);
    return b;
}

entity_id add_corp(world& w, const char* name, float balance = 10000.0f,
                   entity_id home_nation = null_entity)
{
    const entity_id c = w.create_entity();
    corporation_component cc{};
    cc.name         = name;
    cc.balance      = balance;
    cc.home_nation  = home_nation; // BL-1352: whose pool its marches draw
    w.corporations[c] = cc;
    return c;
}

entity_id add_unit(world& w, entity_id corp, entity_id tile, int count = 50,
                   uint16_t type = ROW_INFANTRY)
{
    const entity_id u = w.create_entity();
    unit_component uc{};
    uc.owner    = corp;
    uc.position = tile;
    uc.count    = count;
    uc.type     = type;
    w.units[u]  = uc;
    return u;
}

/// A RAW legacy-grain order (dest_province == 0 — the back-compat tile path
/// unit_march_harness.cpp's own M2 uses), so this file needs no province
/// partition at all: what is under test here is the points gate, not march_unit
/// or the province seam.
void order_along_row(world& w, entity_id unit, entity_id body, int from_col, int to_col)
{
    std::vector<entity_id> path;
    const int step = (to_col >= from_col) ? 1 : -1;
    for (int c = from_col; ; c += step)
    {
        path.push_back(tile_at(w, body, c));
        if (c == to_col) break;
    }
    movement_order mo;
    mo.dest       = path.back();
    mo.path       = path;
    mo.next_index = 1;
    mo.progress   = 0.0f;
    w.units.at(unit).order = mo;
}

/// A registry with infantry march points and the credit price set explicitly —
/// nothing defaults from economy.lua (this harness is Lua-free), so every number
/// in a test is named at the call site. There is no per-anchor capacity rate to
/// author any more (BL-1352): the capacity is the nation's pool.
recipe_registry make_registry(float march_pts, float credit_per_unit_dist)
{
    recipe_registry reg;
    military_capability_params mp = reg.military();
    mp.march_points_per_class[static_cast<std::size_t>(unit_class::infantry)] = march_pts;
    mp.active_lp_credit_per_unit_distance = credit_per_unit_dist;
    reg.set_military(mp);
    return reg;
}

bool order_equal(const movement_order& a, const movement_order& b)
{
    return a.dest == b.dest && a.dest_province == b.dest_province
        && a.path == b.path && a.next_index == b.next_index
        && approx(a.progress, b.progress, 1e-6f);
}

/// One tick of the two passes under test, in the tick's own order: the claims are
/// gathered, the resolve serves them ahead of production, the march spends the
/// answer. Exactly what `run_economy_step` does at phases 4 and 11.
struct march_tick_result
{
    lp_tick         lp;
    unit_march_tick march;
};
march_tick_result march_tick(world& w, const recipe_registry& reg)
{
    march_tick_result out;
    out.lp    = resolve_logistic_points(w, collect_march_claims(w, reg));
    out.march = run_unit_march(w, reg, &out.lp);
    return out;
}

const lp_nation_row* nation_row(const lp_tick& t, entity_id nation)
{
    for (const lp_nation_row& r : t.nations)
        if (r.nation == nation)
            return &r;
    return nullptr;
}

float fraction_of(const lp_tick& t, entity_id building)
{
    const auto it = t.fraction.find(building);
    return it != t.fraction.end() ? it->second : -1.0f;
}

} // namespace

// ---------------------------------------------------------------------------
// L1 — one system: cities make no points, sources do
// ---------------------------------------------------------------------------

void l1_one_system()
{
    std::printf("\n-- L1  one system: a city makes no points, a hub does --\n");

    world w;
    const entity_id body   = make_row_body(w, 10);
    const entity_id nation = add_nation(w, body, 0, 9);
    add_city(w, tile_at(w, body, 2));
    add_city(w, tile_at(w, body, 7));

    const lp_tick city_only = resolve_logistic_points(w);
    check(city_only.sources.empty(), "two cities on a nation's ground are not sources");
    check(nation_row(city_only, nation) == nullptr
              || approx(nation_row(city_only, nation)->pool, 0.0f),
          "a nation with cities and no hub has a pool of zero");

    add_hub(w, tile_at(w, body, 2));
    const lp_tick with_hub = resolve_logistic_points(w);
    check(with_hub.sources.size() == 1, "one running hub is one source");
    const lp_nation_row* r = nation_row(with_hub, nation);
    check(r != nullptr && approx(r->pool, lp_source_points[0]),
          "the pool is exactly the hub's points, with nothing added by the cities");

    // Two independent resolves of an unchanged world agree: a rate, recomputed.
    const lp_tick again = resolve_logistic_points(w);
    const lp_nation_row* r2 = nation_row(again, nation);
    check(r != nullptr && r2 != nullptr && approx(r->pool, r2->pool),
          "two independent resolves of the same world agree on the pool");
}

// ---------------------------------------------------------------------------
// L2 — a granted march moves, pays credits, and shows in the nation's row
// ---------------------------------------------------------------------------

void l2_granted_draw()
{
    std::printf("\n-- L2  a march within the pool: moves, pays, recorded --\n");

    world w;
    const entity_id body   = make_row_body(w, 10);
    const entity_id nation = add_nation(w, body, 0, 9);
    add_hub(w, tile_at(w, body, 0));
    const entity_id corp = add_corp(w, "Mover", 1000.0f, nation);
    const entity_id unit = add_unit(w, corp, tile_at(w, body, 1), /*count=*/50);
    order_along_row(w, unit, body, 1, 4);

    const recipe_registry reg = make_registry(/*march_pts=*/2.0f, /*credit=*/0.1f);
    const float balance_before = w.corporations.at(corp).balance;

    const march_tick_result t = march_tick(w, reg);

    check(t.march.refused_no_lp == 0, "the march is not refused: the pool covers it");
    const lp_nation_row* r = nation_row(t.lp, nation);
    check(r != nullptr && approx(r->march_claim, 2.0f), "the nation's row records the claim (2.0)");
    check(r != nullptr && approx(r->march_granted, 2.0f), "and records it as granted");

    // Plains cost 1.0 a tile, so 2.0 points move exactly two tiles.
    check(w.units.at(unit).position == tile_at(w, body, 3),
          "the unit moved exactly as its march points alone would predict (two plains tiles)");
    const float paid = balance_before - w.corporations.at(corp).balance;
    check(approx(paid, 0.1f * 2.0f * 50.0f),
          "credits debited = price x points x head (the price, separate from the cap)");
}

// ---------------------------------------------------------------------------
// L3 — a refused march mutates nothing
// ---------------------------------------------------------------------------

void l3_refused_mutates_nothing()
{
    std::printf("\n-- L3  a march the pool cannot cover: refused, nothing moves --\n");

    world w;
    const entity_id body   = make_row_body(w, 10);
    const entity_id nation = add_nation(w, body, 0, 9);
    // A hub whose upkeep is unmet to the point of making almost nothing: a pool
    // of 1.2 points against a march asking for 3.0.
    const entity_id hub = add_hub(w, tile_at(w, body, 0));
    w.buildings.at(hub).supply_factor_permille = 10; // 1% of 120 = 1.2 points
    const entity_id corp = add_corp(w, "Stuck", 1000.0f, nation);
    const entity_id unit = add_unit(w, corp, tile_at(w, body, 1));
    order_along_row(w, unit, body, 1, 6);

    const recipe_registry reg = make_registry(/*march_pts=*/3.0f, /*credit=*/0.1f);
    const movement_order order_before   = w.units.at(unit).order;
    const entity_id      position_before = w.units.at(unit).position;
    const float          balance_before = w.corporations.at(corp).balance;

    const march_tick_result t = march_tick(w, reg);

    check(t.march.refused_no_lp == 1, "the refusal is counted");
    check(w.units.at(unit).position == position_before, "the unit did not move");
    check(order_equal(w.units.at(unit).order, order_before),
          "its order, path, index and banked progress are byte-identical");
    check(approx(w.corporations.at(corp).balance, balance_before),
          "and no credits were charged for a march that did not happen");
    const lp_nation_row* r = nation_row(t.lp, nation);
    check(r != nullptr && approx(r->march_claim, 3.0f) && approx(r->march_granted, 0.0f),
          "the claim is recorded and granted nothing - no partial draw");
}

// ---------------------------------------------------------------------------
// L4 — no pool, no march
// ---------------------------------------------------------------------------

void l4_whose_pool()
{
    std::printf("\n-- L4  whose pool: the mover's, not the ground's --\n");

    // (a) A nation with NO SOURCE cannot move its own units.
    {
        world w;
        const entity_id body   = make_row_body(w, 8);
        const entity_id nation = add_nation(w, body, 0, 7); // ground held, no source on it
        add_city(w, tile_at(w, body, 3));                   // and a city, which makes nothing
        const entity_id corp = add_corp(w, "Sourceless", 10000.0f, nation);
        const entity_id unit = add_unit(w, corp, tile_at(w, body, 1));
        order_along_row(w, unit, body, 1, 5);
        const entity_id before = w.units.at(unit).position;
        const recipe_registry reg = make_registry(1.0f, 0.1f);
        const march_tick_result t = march_tick(w, reg);
        check(t.march.refused_no_lp == 1, "a nation with no source refuses its own march");
        check(w.units.at(unit).position == before, "and the unit stayed where it was");
    }

    // (b) UNOWNED GROUND DOES NOT STRAND A UNIT. Its own nation's hub is miles away on
    // its own ground; the unit stands on wilderness and marches anyway. The first cut
    // read the GROUND's nation, which refused - and because a refusal moves nothing, it
    // refused again every tick for ever.
    {
        world w;
        const entity_id body   = make_row_body(w, 10);
        const entity_id nation = add_nation(w, body, 0, 2); // columns 3..9 belong to nobody
        add_hub(w, tile_at(w, body, 0));
        const entity_id corp = add_corp(w, "Expedition", 10000.0f, nation);
        const entity_id unit = add_unit(w, corp, tile_at(w, body, 6)); // out in the wilderness
        order_along_row(w, unit, body, 6, 9);
        const recipe_registry reg = make_registry(/*march_pts=*/2.0f, /*credit=*/0.0f);
        const march_tick_result t = march_tick(w, reg);
        check(t.march.refused_no_lp == 0, "a unit on unowned ground is not refused");
        check(w.units.at(unit).position == tile_at(w, body, 8),
              "it marches on its OWN nation's points, two tiles, and is not stranded");
        const lp_nation_row* r = nation_row(t.lp, nation);
        check(r != nullptr && approx(r->march_granted, 2.0f),
              "and the points came out of its own nation's pool");
    }

    // (c) AN INVADER PAYS ITS OWN POINTS. Two nations, a hub each; the invader's unit
    // stands on the defender's ground. The defender's pool must be untouched.
    {
        world w;
        const entity_id body     = make_row_body(w, 12);
        const entity_id attacker = add_nation(w, body, 0, 5,  "Attacker");
        const entity_id defender = add_nation(w, body, 6, 11, "Defender");
        add_hub(w, tile_at(w, body, 0));  // attacker's
        add_hub(w, tile_at(w, body, 11)); // defender's
        const entity_id corp = add_corp(w, "Invader", 10000.0f, attacker);
        const entity_id unit = add_unit(w, corp, tile_at(w, body, 7)); // on defender's ground
        order_along_row(w, unit, body, 7, 10);
        const recipe_registry reg = make_registry(/*march_pts=*/2.0f, /*credit=*/0.0f);
        const march_tick_result t = march_tick(w, reg);
        const lp_nation_row* ar = nation_row(t.lp, attacker);
        const lp_nation_row* dr = nation_row(t.lp, defender);
        check(t.march.refused_no_lp == 0, "the invading march is served");
        check(ar != nullptr && approx(ar->march_granted, 2.0f),
              "out of the ATTACKER's pool");
        check(dr != nullptr && approx(dr->march_granted, 0.0f) && approx(dr->march_claim, 0.0f),
              "and the defender's pool is untouched - an invasion is not funded by its victim");
    }

    // (d) An OWNERLESS unit - no corporation, no nation - has no pool and is refused.
    {
        world w;
        const entity_id body = make_row_body(w, 8);
        add_nation(w, body, 0, 7);
        add_hub(w, tile_at(w, body, 0));
        const entity_id unit = add_unit(w, /*corp=*/null_entity, tile_at(w, body, 1));
        order_along_row(w, unit, body, 1, 5);
        const entity_id before = w.units.at(unit).position;
        const recipe_registry reg = make_registry(1.0f, 0.1f);
        const march_tick_result t = march_tick(w, reg);
        check(t.march.refused_no_lp == 1, "a unit belonging to nobody has no pool to draw");
        check(w.units.at(unit).position == before, "and does not move on someone else's points");
    }
}

// ---------------------------------------------------------------------------
// L5 — contention resolves in the march's own order, reproducibly
// ---------------------------------------------------------------------------

void l5_contention_order()
{
    std::printf("\n-- L5  contention: served in the march's order, twice alike --\n");

    // Two corps, each one unit, on one nation's ground, against a pool that
    // covers one march and not both. The hostile corp is created SECOND and so
    // holds the HIGHER unit id — if the order were id alone it would lose, and
    // the mobilised-first partition is what makes it win.
    const auto run = [](bool* out_hostile_moved, bool* out_peaceful_moved) {
        world w;
        const entity_id body   = make_row_body(w, 10);
        const entity_id nation = add_nation(w, body, 0, 9);
        const entity_id hub    = add_hub(w, tile_at(w, body, 0));
        w.buildings.at(hub).supply_factor_permille = 25; // 2.5% of 120 = 3.0 points

        const entity_id peaceful = add_corp(w, "Peaceful", 10000.0f, nation);
        const entity_id hostile  = add_corp(w, "Hostile",  10000.0f, nation);
        const entity_id enemy    = add_corp(w, "Enemy");
        w.corp_hostile_pairs.insert({hostile, enemy});

        const entity_id u_peaceful = add_unit(w, peaceful, tile_at(w, body, 2));
        const entity_id u_hostile  = add_unit(w, hostile,  tile_at(w, body, 5));
        order_along_row(w, u_peaceful, body, 2, 4);
        order_along_row(w, u_hostile,  body, 5, 7);

        const recipe_registry reg = make_registry(/*march_pts=*/2.0f, /*credit=*/0.0f);
        const entity_id peaceful_from = w.units.at(u_peaceful).position;
        const entity_id hostile_from  = w.units.at(u_hostile).position;
        const march_tick_result t = march_tick(w, reg);
        *out_hostile_moved  = w.units.at(u_hostile).position  != hostile_from;
        *out_peaceful_moved = w.units.at(u_peaceful).position != peaceful_from;
        return t.march.refused_no_lp;
    };

    bool h1 = false, p1 = false, h2 = false, p2 = false;
    const int refused_1 = run(&h1, &p1);
    const int refused_2 = run(&h2, &p2);

    check(refused_1 == 1, "a pool covering one march of two refuses exactly one");
    check(h1 && !p1, "the mobilised corp's unit is served first, the peaceful one waits");
    check(refused_1 == refused_2 && h1 == h2 && p1 == p2,
          "two independent runs of the same scenario resolve identically");
}

// ---------------------------------------------------------------------------
// L6 — the march claims first, and nothing carries over
// ---------------------------------------------------------------------------

void l6_march_claims_first()
{
    std::printf("\n-- L6  the march claims first; no carry-over --\n");

    // One nation, one hub, one producing building asking for more points than the
    // pool holds, so production is already short and every point the march takes
    // is a point production does not get.
    const auto build = [](world& w, bool with_march, entity_id* out_consumer) {
        const entity_id body   = make_row_body(w, 10);
        const entity_id nation = add_nation(w, body, 0, 9);
        add_hub(w, tile_at(w, body, 0));
        const entity_id corp = add_corp(w, "Producer", 100000.0f, nation);
        // need = 10 x 30 x 1.0 = 300 points against a pool of 120: short either way.
        *out_consumer = add_consumer(w, tile_at(w, body, 1), corp, /*assigned=*/30.0f,
                                     /*target=*/100.0f);
        if (with_march)
        {
            const entity_id mover = add_corp(w, "Mover", 1000.0f, nation);
            const entity_id unit  = add_unit(w, mover, tile_at(w, body, 3));
            order_along_row(w, unit, body, 3, 6);
        }
    };

    const recipe_registry reg = make_registry(/*march_pts=*/12.0f, /*credit=*/0.0f);

    world quiet;    entity_id quiet_consumer = null_entity;
    build(quiet, /*with_march=*/false, &quiet_consumer);
    const march_tick_result tq = march_tick(quiet, reg);

    world marching; entity_id marching_consumer = null_entity;
    build(marching, /*with_march=*/true, &marching_consumer);
    const march_tick_result tm = march_tick(marching, reg);

    const float f_quiet    = fraction_of(tq.lp, quiet_consumer);
    const float f_marching = fraction_of(tm.lp, marching_consumer);

    check(tm.march.refused_no_lp == 0, "the march is served out of the pool");
    check(f_quiet > 0.0f && f_marching > 0.0f, "the consumer produces in both worlds");
    check(f_marching < f_quiet, "production gets LESS when a march has claimed first");

    // The claim is 12 points of a 120 pool against a claim of 300, so the granted
    // fraction is pool/claim either way: 120/300 quiet, 108/300 marching.
    check(approx(f_quiet, lp_source_points[0] / 300.0f, 1e-3f),
          "quiet: the fraction is the whole pool over the claim");
    check(approx(f_marching, (lp_source_points[0] - 12.0f) / 300.0f, 1e-3f),
          "marching: it is the pool LESS the march's 12 points over the same claim");

    // NO CARRY-OVER. The question is whether one tick leaves anything behind for
    // the next, so it is asked of a SECOND, identically built world inside the same
    // process - not of the first world after its march, which has moved its unit and
    // is therefore a different world (that re-resolve measured the move, not a
    // carry-over, and said FAIL for the wrong reason).
    world repeat; entity_id repeat_consumer = null_entity;
    build(repeat, /*with_march=*/true, &repeat_consumer);
    const march_tick_result tr = march_tick(repeat, reg);
    check(approx(fraction_of(tr.lp, repeat_consumer), f_marching, 1e-6f)
              && tr.march.refused_no_lp == tm.march.refused_no_lp,
          "the identical scenario run again gives the identical grant: a rate, not a stock");
}

// L7 - BL-1360 (Ben, 2026-10-16; LOGISTICS.md § One pool): a better road carries
// points further, and rail is point to point.
//
// ROADS: one hub at column 0 of a long row, a road of one tier laid east along it.
// Track reaches 6 tiles at full strength, Road 8, Highway 10, and each fades to zero
// at DOUBLE its distance. The row is 60 wide so the wrap-around (columns wrap on the
// grid) is far off-road and cannot short-cut back to the far tiles.
//
// RAIL: track along the row, stations at columns 0, 15, 24 and 31. From the station
// at 0 the line delivers to the station tiles at 15 (full) and 24 (fading), not to
// the one at 31 (beyond 20 + 8), and NEVER to a track tile it merely passes. This
// asks the field directly with ONE seed: on a fixture every station is itself a
// source, so a coverage read would only ever see each station's own tile.
void l7_reach_by_tier_and_rail()
{
    std::printf("\n-- L7: road tier extends reach; rail delivers only where it connects (BL-1360)\n");
    const int east = hex_side_for_offset(1, 0, /*odd_row=*/false);

    struct tier_case { std::uint8_t tier; const char* name; int full; };
    const tier_case cases[] = { {1, "Track", 6}, {2, "Road", 8}, {3, "Highway", 10} };
    for (const tier_case& tc : cases)
    {
        world w;
        const entity_id body = make_row_body(w, 60);
        for (int c = 0; c < 2 * tc.full + 2; ++c)
            lay_road_edge(w, tile_at(w, body, c), east, tc.tier);
        const auto f = lp_distance_field(w, body, lp_network::road, {tile_at(w, body, 0)});
        const auto reach_at = [&](int c) {
            const auto it = f.find(tile_at(w, body, c));
            return it == f.end() ? 0.0f : lp_reach(lp_network::road, it->second);
        };
        char msg[160];
        // EXACTLY 1, not approx: the callers that ask "fully covered?" test >= 1.0f
        // (corp_ai, seed_logistics_sources).
        std::snprintf(msg, sizeof msg, "%s: full strength (exactly 1) at %d tiles", tc.name, tc.full);
        check(reach_at(tc.full) >= 1.0f, msg);
        // And the DISTANCE reads as inside full reach, which is what the ground card's
        // wording asks (lp_full_strength). The printed distance is the evidence: ten
        // 0.6 steps measured 5.9999995, under 6, so this row holds with or without
        // lp_full_eps - it pins the card and the figure to one predicate.
        {
            const auto it = f.find(tile_at(w, body, tc.full));
            std::snprintf(msg, sizeof msg, "%s: tile %d's distance reads as full strength",
                          tc.name, tc.full);
            check(it != f.end() && lp_full_strength(lp_network::road, it->second), msg);
            std::printf("        (%s: distance at %d tiles = %.9f)\n", tc.name, tc.full,
                        it != f.end() ? static_cast<double>(it->second) : -1.0);
        }
        std::snprintf(msg, sizeof msg, "%s: already fading one tile further (%d)", tc.name, tc.full + 1);
        check(reach_at(tc.full + 1) < 1.0f && reach_at(tc.full + 1) > 0.0f, msg);
        std::snprintf(msg, sizeof msg, "%s: half strength midway through the fade (%d tiles)",
                      tc.name, tc.full + tc.full / 2);
        check(approx(reach_at(tc.full + tc.full / 2), 0.5f, 1e-3f), msg);
        std::snprintf(msg, sizeof msg, "%s: nothing left at double (%d tiles)", tc.name, 2 * tc.full);
        check(approx(reach_at(2 * tc.full), 0.0f, 1e-3f), msg);
    }

    world w;
    const entity_id body   = make_row_body(w, 60);
    const entity_id nation = add_nation(w, body, 0, 59);
    for (int c = 0; c <= 40; ++c)
        w.rail_track[tile_at(w, body, c)] = nation;
    for (const int c : {0, 15, 24, 31})
        add_building(w, tile_at(w, body, c), building_type::railway_station);
    const auto f = lp_distance_field(w, body, lp_network::rail, {tile_at(w, body, 0)});
    const auto dist_at = [&](int c) {
        const auto it = f.find(tile_at(w, body, c));
        return it == f.end() ? -1.0f : it->second;
    };
    check(approx(dist_at(0), 0.0f), "rail: the station's own tile is served");
    check(approx(dist_at(15), 15.0f) && approx(lp_reach(lp_network::rail, 15.0f), 1.0f),
          "rail: a station 15 steps down the line is served at full strength");
    check(approx(dist_at(24), 24.0f) && approx(lp_reach(lp_network::rail, 24.0f), 0.5f, 1e-3f),
          "rail: a station 24 steps down the line is past the 20 and fading (half)");
    check(dist_at(31) < 0.0f, "rail: a station 31 steps away is beyond 20 + 8 and not served");
    check(dist_at(7) < 0.0f && dist_at(20) < 0.0f,
          "rail: track the line merely PASSES (7, 20) is never served");
    check(dist_at(45) < 0.0f, "rail: ground off the line is never served");

    // What the line does NOT carry, read off the same rail network the router uses
    // (LOGISTICS.md § 4c): an unfunded station, idle track, and a station on no line.
    {
        // An unfunded (state_idle) station at 15 drops off the line.
        world wi;
        const entity_id bi = make_row_body(wi, 60);
        const entity_id ni = add_nation(wi, bi, 0, 59);
        for (int c = 0; c <= 40; ++c)
            wi.rail_track[tile_at(wi, bi, c)] = ni;
        add_building(wi, tile_at(wi, bi, 0), building_type::railway_station);
        const entity_id s15 = add_building(wi, tile_at(wi, bi, 15), building_type::railway_station);
        wi.buildings.at(s15).state_idle = true;
        const auto fi = lp_distance_field(wi, bi, lp_network::rail, {tile_at(wi, bi, 0)});
        check(fi.count(tile_at(wi, bi, 15)) == 0, "rail: an unfunded station is not served");
    }
    {
        // Idle track (its owner unfunded) carries nothing: the station at 15 is stranded.
        world wt;
        const entity_id bt = make_row_body(wt, 60);
        const entity_id nt = add_nation(wt, bt, 0, 59);
        for (int c = 0; c <= 40; ++c)
            wt.rail_track[tile_at(wt, bt, c)] = nt;
        wt.rail_idle.insert(nt);
        add_building(wt, tile_at(wt, bt, 0), building_type::railway_station);
        add_building(wt, tile_at(wt, bt, 15), building_type::railway_station);
        const auto ft = lp_distance_field(wt, bt, lp_network::rail, {tile_at(wt, bt, 0)});
        check(ft.count(tile_at(wt, bt, 15)) == 0, "rail: idle track carries nothing");
        check(ft.count(tile_at(wt, bt, 0)) == 1, "rail: the station still serves its own tile");
    }
    {
        // A station touching no track is on no line: its own tile only.
        world wn;
        const entity_id bn = make_row_body(wn, 60);
        add_nation(wn, bn, 0, 59);
        add_building(wn, tile_at(wn, bn, 0), building_type::railway_station);
        const auto fn = lp_distance_field(wn, bn, lp_network::rail, {tile_at(wn, bn, 0)});
        check(fn.size() == 1 && fn.count(tile_at(wn, bn, 0)) == 1,
              "rail: a station on no line serves its own tile and nothing else");
    }
}

int main()
{
    std::printf("=== logistic_points_harness (BL-1352, LP_ONE_SYSTEM) ===\n");

    l1_one_system();
    l2_granted_draw();
    l3_refused_mutates_nothing();
    l4_whose_pool();
    l5_contention_order();
    l6_march_claims_first();
    l7_reach_by_tier_and_rail();

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
