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

    // SEA (BL-1396, Ben 2026-10-21): a port stretches 50 water tiles at full strength,
    // gone at 100, and over water only - its points land on the coast they reach and go
    // no further inland. A port at column 0, sea 1-120, land 121-130, sea after.
    {
        world ws;
        const entity_id bs = make_row_body(ws, 300);
        for (int col = 1; col <= 120; ++col)
            ws.tiles.at(tile_at(ws, bs, col)).substrate = terrain_substrate::coast;
        for (int col = 131; col <= 299; ++col)
            ws.tiles.at(tile_at(ws, bs, col)).substrate = terrain_substrate::coast;
        const auto f = lp_distance_field(ws, bs, lp_network::sea, {tile_at(ws, bs, 0)});
        const auto reach_at = [&](int col) {
            const auto it = f.find(tile_at(ws, bs, col));
            return it == f.end() ? 0.0f : lp_reach(lp_network::sea, it->second);
        };
        check(reach_at(50) >= 1.0f, "sea: full strength 50 water tiles out");
        check(approx(reach_at(75), 0.5f, 1e-3f), "sea: half strength 75 out, midway through the fade");
        check(f.count(tile_at(ws, bs, 100)) == 1 && approx(f.at(tile_at(ws, bs, 100)), 100.0f)
                  && approx(reach_at(100), 0.0f, 1e-3f),
              "sea: the walk reaches 100 and nothing is left there, double the full reach");
        // A short strait: water 1-9, land 10-20 (coast at 10, inland behind it).
        world wc;
        const entity_id bc = make_row_body(wc, 300);
        for (int col = 1; col <= 9; ++col)
            wc.tiles.at(tile_at(wc, bc, col)).substrate = terrain_substrate::coast;
        for (int col = 21; col <= 299; ++col)
            wc.tiles.at(tile_at(wc, bc, col)).substrate = terrain_substrate::coast;
        const auto fc2 = lp_distance_field(wc, bc, lp_network::sea, {tile_at(wc, bc, 0)});
        check(fc2.count(tile_at(wc, bc, 10)) == 1, "sea: a port's points land on the coast across the water");

        // Two dimensions, 5 x 5, all land but ONE sea tile. Odd-r hexes: on an even row,
        // (x-1, y-1) is a diagonal (NW) side a four-way walk never crosses.
        world wd;
        const entity_id bd = wd.create_entity();
        { body_component b{}; b.grid_width = 5; b.grid_height = 5; wd.bodies[bd] = b; }
        std::vector<entity_id> g2(25);
        for (int y = 0; y < 5; ++y)
            for (int x = 0; x < 5; ++x)
            {
                const entity_id t2 = wd.create_entity();
                tile_component tc{}; tc.body = bd; tc.grid_x = x; tc.grid_y = y;
                tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass;
                wd.tiles[t2] = tc;
                g2[static_cast<std::size_t>(y * 5 + x)] = t2;
            }
        const auto at2 = [&](int x, int y) { return g2[static_cast<std::size_t>(y * 5 + x)]; };
        wd.tiles.at(at2(1, 1)).substrate = terrain_substrate::coast; // NW of (2,2), an even row
        const auto fd = lp_distance_field(wd, bd, lp_network::sea, {at2(2, 2)});
        check(fd.count(at2(1, 1)) == 1,
              "sea: a port with sea only on a diagonal hex side still reaches it (all six sides)");
        // (1,2) sits beside that sea tile on the odd row, so it is a coast the points land
        // on; (3,2) and (2,3) are the port's own land neighbours, touching no sea.
        check(fd.count(at2(1, 2)) == 1, "sea: a coast beside the sea tile is landed on");
        check(fd.count(at2(3, 2)) == 0 && fd.count(at2(2, 3)) == 0,
              "sea: a port's own land neighbours that touch no sea are never reached - land to land is no sea step");
        check(fc2.count(tile_at(wc, bc, 11)) == 0 && fc2.count(tile_at(wc, bc, 15)) == 0,
              "sea: and go no further inland - that ground is a hub's");
        // LAKES TOO (Ben, 2026-10-21, NR-1061): the same strait as a lake is crossed.
        world wl;
        const entity_id bl = make_row_body(wl, 300);
        for (int col = 1; col <= 9; ++col)
            wl.tiles.at(tile_at(wl, bl, col)).substrate = terrain_substrate::lake;
        for (int col = 21; col <= 299; ++col)
            wl.tiles.at(tile_at(wl, bl, col)).substrate = terrain_substrate::coast;
        const auto fl = lp_distance_field(wl, bl, lp_network::sea, {tile_at(wl, bl, 0)});
        check(fl.count(tile_at(wl, bl, 10)) == 1, "sea: a port's points cross a lake and land on its far shore");
    }

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

// L8 - BL-1356 (LOGISTICS.md § How a tile's points get to it, the four mechanism
// rules, Ben 2026-10-20; the capital as anchor, 2026-10-21). Every fixture is a
// 60-wide unroaded row, so one step costs lp_offnet_step (3) and a hub's road field
// is full to 2 tiles and gone by 4. The nation's CAPITAL is where "connected back" is
// measured; a market is placed elsewhere on purpose, to prove it is not the anchor.
void set_capital(world& w, entity_id nation, entity_id tile)
{
    w.nations.at(nation).capital_tile = tile;
}

entity_id add_market(world& w, entity_id body, entity_id centre)
{
    const entity_id m = w.create_entity();
    market_component mc{};
    mc.body        = body;
    mc.centre_tile = centre;
    w.markets[m]   = mc;
    return m;
}

const lp_building_row* row_of(const lp_tick& t, entity_id building)
{
    for (const lp_building_row& r : t.buildings)
        if (r.building == building)
            return &r;
    return nullptr;
}

void l8_routed_requests()
{
    std::printf("\n-- L8: requests are routed (BL-1356)\n");

    // NO CAPITAL ON THIS BODY: nothing to be connected back to, so every source passes
    // the test there - the same isolated hub that supplies nothing below supplies here.
    {
        world w;
        const entity_id body   = make_row_body(w, 60);
        const entity_id nation = add_nation(w, body, 0, 59); // capital_tile left null
        const entity_id corp = add_corp(w, "Producer", 100000.0f, nation);
        add_hub(w, tile_at(w, body, 20));
        const entity_id c = add_consumer(w, tile_at(w, body, 21), corp, 10.0f, 100.0f);
        const lp_tick t = resolve_logistic_points(w);
        const lp_building_row* r = row_of(t, c);
        check(r != nullptr && approx(r->coverage, 1.0f),
              "no capital on the body: every source passes, so a lone hub supplies in full");
    }

    // ONLY CONNECTED SOURCES FEED THE POOL (Ben, 2026-10-21, NR-1057): a hub at the
    // capital and a second, isolated, far down the row - the pool is the first's alone.
    {
        world w;
        const entity_id body   = make_row_body(w, 60);
        const entity_id nation = add_nation(w, body, 0, 59);
        set_capital(w, nation, tile_at(w, body, 0));
        add_hub(w, tile_at(w, body, 0));
        add_hub(w, tile_at(w, body, 30)); // touches neither the capital nor the chain
        const lp_tick t = resolve_logistic_points(w);
        float pool = -1.0f;
        for (const lp_nation_row& n : t.nations)
            if (n.nation == nation)
                pool = n.pool;
        int linked = 0;
        for (const lp_source_row& s : t.sources)
            if (s.connected)
                ++linked;
        check(approx(pool, lp_source_points[0]) && linked == 1,
              "an isolated hub adds nothing to the pool: it is the connected hub's points alone");
    }

    // A LINK COUNTS EITHER WAY (Ben, 2026-10-21, NR-1058): a station's own field holds
    // only stations, so it can join the chain only because a CONNECTED road hub's reach
    // covers its tile - and then the line carries the chain to the station down it.
    for (const bool hub : {false, true})
    {
        world w;
        const entity_id body   = make_row_body(w, 60);
        const entity_id nation = add_nation(w, body, 0, 59);
        set_capital(w, nation, tile_at(w, body, 0));
        for (int col = 5; col <= 25; ++col)
            w.rail_track[tile_at(w, body, col)] = nation;
        add_building(w, tile_at(w, body, 5), building_type::railway_station);
        add_building(w, tile_at(w, body, 20), building_type::railway_station);
        if (hub)
            add_hub(w, tile_at(w, body, 3)); // touches the capital (9) and the station at 5 (6)
        const entity_id corp = add_corp(w, "Producer", 100000.0f, nation);
        const entity_id c = add_consumer(w, tile_at(w, body, 20), corp, 10.0f, 100.0f);
        const lp_tick t = resolve_logistic_points(w);
        const lp_building_row* r = row_of(t, c);
        check(r != nullptr && approx(r->coverage, hub ? 1.0f : 0.0f),
              hub ? "a connected road hub covering a station puts the line on the chain, and its far station supplies"
                  : "the same line with no connected source covering it supplies nothing");
    }

    // GENERATION LAYS THE CHAIN: a consumer 25 tiles from the capital, no hub anywhere.
    // One hub reaches 4 unroaded tiles, so covering it takes a chain laid outward.
    {
        world w;
        const entity_id body   = make_row_body(w, 60);
        const entity_id nation = add_nation(w, body, 0, 59);
        set_capital(w, nation, tile_at(w, body, 0));
        const entity_id corp = add_corp(w, "Producer", 100000.0f, nation);
        const entity_id c = add_consumer(w, tile_at(w, body, 25), corp, 10.0f, 100.0f);
        const lp_seed_report rep = seed_logistics_sources(w);
        const lp_tick t = resolve_logistic_points(w);
        const lp_building_row* r = row_of(t, c);
        check(r != nullptr && approx(r->coverage, 1.0f) && rep.unconnectable == 0,
              "generation lays a chain out from the capital to a far consumer and covers it");
        check(rep.hubs_placed >= 2,
              "and it took more than one hub (a chain, not a single isolated one)");
    }

    // THE SEA BRIDGE: the consumer's own nation holds an island across a five-tile strait
    // (columns 20-24 water, and 45-59 so the row's wrap is a wider sea). The road chain
    // reaches the home shore and can go no further; generation lands a home port on the
    // shore NEAREST the island by sea (column 19, 8 away - not the capital tile, 18 away
    // round the wrap) and an island port, then lays the road chain on to the consumer.
    {
        world w;
        const entity_id body   = make_row_body(w, 60);
        for (int col = 20; col <= 24; ++col)
            w.tiles.at(tile_at(w, body, col)).substrate = terrain_substrate::coast;
        for (int col = 45; col <= 59; ++col)
            w.tiles.at(tile_at(w, body, col)).substrate = terrain_substrate::coast;
        const entity_id nation = add_nation(w, body, 0, 59);
        set_capital(w, nation, tile_at(w, body, 0));
        const entity_id corp = add_corp(w, "Producer", 100000.0f, nation);
        const entity_id c = add_consumer(w, tile_at(w, body, 35), corp, 10.0f, 100.0f);
        const lp_seed_report rep = seed_logistics_sources(w);
        const lp_tick t = resolve_logistic_points(w);
        const lp_building_row* r = row_of(t, c);
        std::printf("        (bridge fixture: hubs %d, ports %d, bridged %d, unconnectable %d, cut off %d, coverage %.2f)\n",
                    rep.hubs_placed, rep.ports_placed, rep.islands_bridged, rep.unconnectable,
                    rep.unconnectable_cut_off, r ? static_cast<double>(r->coverage) : -1.0);
        check(r != nullptr && approx(r->coverage, 1.0f) && rep.unconnectable == 0,
              "generation bridges a strait by sea and covers a consumer cut off from the capital");
        check(rep.islands_bridged == 1 && rep.ports_placed == 2,
              "with one bridge: a home port and an island port");
        bool home_at_19 = false;
        for (const auto& [bid, bb] : w.buildings)
            if (bb.type == building_type::port && bb.tile == tile_at(w, body, 19))
                home_at_19 = true;
        check(home_at_19, "the home port stands on the shore nearest the island by sea (column 19)");
    }

    // ISLAND-HOPPING, IN EITHER ID ORDER (cold review): the far island I2 (155-169) is
    // beyond any port's reach of the capital's coast but within reach of I1 (80-94). Its
    // consumer is made FIRST (the lower id), so the first pass tries I2 before I1 exists
    // on the chain and fails; the second pass, after I1 is bridged, bridges I2 from it.
    {
        world w;
        // A port reaches 100 (BL-1396), so the gaps are wide: I1 sits 60 sea tiles off the
        // capital's coast (in reach), I2 another 60 beyond I1 (in reach of I1, 120+ from
        // the capital's shore), and 110 tiles of sea round the wrap back to column 0.
        const entity_id body = make_row_body(w, 300);
        for (int col = 20; col <= 79; ++col)
            w.tiles.at(tile_at(w, body, col)).substrate = terrain_substrate::coast;
        for (int col = 95; col <= 154; ++col)
            w.tiles.at(tile_at(w, body, col)).substrate = terrain_substrate::coast;
        for (int col = 170; col <= 299; ++col)
            w.tiles.at(tile_at(w, body, col)).substrate = terrain_substrate::coast;
        const entity_id nation = add_nation(w, body, 0, 299);
        set_capital(w, nation, tile_at(w, body, 0));
        const entity_id corp = add_corp(w, "Producer", 100000.0f, nation);
        const entity_id far  = add_consumer(w, tile_at(w, body, 162), corp, 10.0f, 100.0f); // I2, first
        const entity_id near = add_consumer(w, tile_at(w, body, 87), corp, 10.0f, 100.0f);  // I1
        const lp_seed_report rep = seed_logistics_sources(w);
        const lp_tick t = resolve_logistic_points(w);
        const lp_building_row* rf = row_of(t, far);
        const lp_building_row* rn = row_of(t, near);
        check(rf != nullptr && rn != nullptr && approx(rf->coverage, 1.0f) && approx(rn->coverage, 1.0f)
                  && rep.islands_bridged == 2 && rep.unconnectable == 0,
              "island-hopping: the far island is bridged from the near one, whatever order the ids put them in");
    }

    // A SEA TOO WIDE: the island lies 120 tiles of water out (and 100 round the wrap) -
    // beyond a port's reach, which fades to nothing at 100 (BL-1396) - so no bridge is
    // placed and the consumer is counted as cut off, with no dead ports.
    {
        world w;
        const entity_id body = make_row_body(w, 260);
        for (int col = 20; col <= 139; ++col)
            w.tiles.at(tile_at(w, body, col)).substrate = terrain_substrate::coast;
        for (int col = 160; col <= 259; ++col)
            w.tiles.at(tile_at(w, body, col)).substrate = terrain_substrate::coast;
        const entity_id nation = add_nation(w, body, 0, 259);
        set_capital(w, nation, tile_at(w, body, 0));
        const entity_id corp = add_corp(w, "Producer", 100000.0f, nation);
        add_consumer(w, tile_at(w, body, 150), corp, 10.0f, 100.0f);
        const lp_seed_report rep = seed_logistics_sources(w);
        check(rep.unconnectable == 1 && rep.unconnectable_cut_off == 1 && rep.ports_placed == 0,
              "a sea wider than a port's reach: counted as cut off, and no port placed that would deliver nothing");
    }

    // THE PLANNER (BL-1395): the chain lp_plan_chain proposes for a far consumer, built
    // hub by hub, covers it - and it is the same chain generation lays (generation now
    // calls it). An unreachable target is planned as nothing, and a covered one as empty.
    {
        world w;
        const entity_id body   = make_row_body(w, 60);
        const entity_id nation = add_nation(w, body, 0, 59);
        set_capital(w, nation, tile_at(w, body, 0));
        const entity_id corp = add_corp(w, "Producer", 100000.0f, nation);
        const entity_id c = add_consumer(w, tile_at(w, body, 25), corp, 10.0f, 100.0f);
        const auto anywhere = [&](entity_id t) {
            return !is_water(w.tiles.at(t).substrate) && w.tile_to_nation.count(t) != 0;
        };
        lp_reach_memo memo;
        const lp_chain_plan plan = lp_plan_chain(w, memo, nation, body, tile_at(w, body, 25), anywhere);
        check(plan.reachable && plan.tiles.size() >= 2, "the planner proposes a chain of hubs to a far consumer");
        for (const entity_id t : plan.tiles)
            add_hub(w, t);
        const lp_tick t = resolve_logistic_points(w);
        const lp_building_row* r = row_of(t, c);
        check(r != nullptr && approx(r->coverage, 1.0f), "and building exactly that chain covers it");
        lp_reach_memo memo2;
        const lp_chain_plan again = lp_plan_chain(w, memo2, nation, body, tile_at(w, body, 25), anywhere);
        check(again.reachable && again.tiles.empty(), "a covered target plans no hub at all");
    }
    {
        world w;
        const entity_id body = make_row_body(w, 60);
        for (int col = 20; col <= 59; ++col)
            w.tiles.at(tile_at(w, body, col)).substrate = terrain_substrate::coast;
        const entity_id nation = add_nation(w, body, 0, 19);
        set_capital(w, nation, tile_at(w, body, 0));
        // A hub may not stand within four tiles of the capital: the chain cannot start.
        const auto far_only = [&](entity_id t) {
            return !is_water(w.tiles.at(t).substrate) && w.tiles.at(t).grid_x > 4;
        };
        lp_reach_memo memo;
        const lp_chain_plan plan = lp_plan_chain(w, memo, nation, body, tile_at(w, body, 15), far_only);
        check(!plan.reachable && plan.tiles.empty(),
              "a target no legal chain can reach is planned as nothing - no hub that would deliver nothing");
    }

    // THE CHAIN STOPS AT A FRONTIER: the consumer's own nation holds both ends of the row
    // but another nation holds the ground between (both ways round the wrap), so no hub
    // of its nation can carry the chain to it. Left uncovered and counted, not faked.
    {
        world w;
        const entity_id body = make_row_body(w, 60);
        const entity_id a = add_nation(w, body, 0, 29, "Home");
        const entity_id b = add_nation(w, body, 30, 39, "Between");
        for (int col = 40; col <= 49; ++col)
        {
            const entity_id t2 = tile_at(w, body, col);
            w.tile_to_nation[t2] = a;
            w.nations.at(a).tiles.push_back(t2);
        }
        for (int col = 50; col <= 59; ++col)
        {
            const entity_id t2 = tile_at(w, body, col);
            w.tile_to_nation[t2] = b;
            w.nations.at(b).tiles.push_back(t2);
        }
        set_capital(w, a, tile_at(w, body, 0));
        const entity_id corp = add_corp(w, "Producer", 100000.0f, a);
        const entity_id c = add_consumer(w, tile_at(w, body, 45), corp, 10.0f, 100.0f);
        const lp_seed_report rep = seed_logistics_sources(w);
        const lp_tick t = resolve_logistic_points(w);
        const lp_building_row* r = row_of(t, c);
        check(r != nullptr && approx(r->coverage, 0.0f) && rep.unconnectable == 1
                  && rep.unconnectable_cut_off == 1 && rep.ports_placed == 0,
              "a consumer the chain cannot reach across another nation's ground is left uncovered and counted");
    }

    // CONNECTED BACK (the chain to the capital, Ben 2026-10-20/21): a hub touching
    // neither the capital nor a connected source supplies nothing, though it reaches the
    // building; one that touches the capital does; and one touching a CONNECTED hub too.
    {
        world w;
        const entity_id body   = make_row_body(w, 60);
        const entity_id nation = add_nation(w, body, 0, 59);
        set_capital(w, nation, tile_at(w, body, 0));
        add_market(w, body, tile_at(w, body, 50)); // NOT the anchor
        const entity_id corp = add_corp(w, "Producer", 100000.0f, nation);
        add_hub(w, tile_at(w, body, 20));
        const entity_id far = add_consumer(w, tile_at(w, body, 21), corp, 10.0f, 100.0f);
        const lp_tick t = resolve_logistic_points(w);
        const lp_building_row* r = row_of(t, far);
        check(r != nullptr && approx(r->coverage, 0.0f) && approx(r->granted, 0.0f),
              "an isolated hub (no chain back to the capital) supplies nothing");
    }
    {
        world w;
        const entity_id body   = make_row_body(w, 60);
        const entity_id nation = add_nation(w, body, 0, 59);
        set_capital(w, nation, tile_at(w, body, 0));
        add_market(w, body, tile_at(w, body, 50)); // NOT the anchor
        const entity_id corp = add_corp(w, "Producer", 100000.0f, nation);
        add_hub(w, tile_at(w, body, 3)); // 9 from the capital: fading, but touching it
        const entity_id near = add_consumer(w, tile_at(w, body, 5), corp, 10.0f, 100.0f);
        const lp_tick t = resolve_logistic_points(w);
        const lp_building_row* r = row_of(t, near);
        check(r != nullptr && approx(r->coverage, 1.0f) && approx(r->fraction, 1.0f),
              "a hub whose reach touches the capital supplies the building it reaches, in full");
    }
    for (const bool link : {false, true})
    {
        // The hub at 6 is 18 from the capital (beyond its 12) but 9 from the hub at 3,
        // which touches the capital: with that link it is connected, without it isolated.
        world w;
        const entity_id body   = make_row_body(w, 60);
        const entity_id nation = add_nation(w, body, 0, 59);
        set_capital(w, nation, tile_at(w, body, 0));
        add_market(w, body, tile_at(w, body, 50)); // NOT the anchor
        const entity_id corp = add_corp(w, "Producer", 100000.0f, nation);
        if (link)
            add_hub(w, tile_at(w, body, 3));
        add_hub(w, tile_at(w, body, 6));
        const entity_id c = add_consumer(w, tile_at(w, body, 8), corp, 10.0f, 100.0f);
        const lp_tick t = resolve_logistic_points(w);
        const lp_building_row* r = row_of(t, c);
        check(r != nullptr && approx(r->coverage, link ? 1.0f : 0.0f),
              link ? "a hub touching a CONNECTED hub joins the chain and supplies in full"
                   : "the same hub with no link back to the capital supplies nothing");
    }

    // TOP-UP (rule A on the fade): each source carries reach x the request, and the
    // next tops up. Two hubs each at half strength cover the whole request; one does not.
    for (const bool both : {false, true})
    {
        world w;
        const entity_id body   = make_row_body(w, 60);
        const entity_id nation = add_nation(w, body, 0, 59);
        set_capital(w, nation, tile_at(w, body, 10));
        add_market(w, body, tile_at(w, body, 50)); // NOT the anchor
        const entity_id corp = add_corp(w, "Producer", 100000.0f, nation);
        add_hub(w, tile_at(w, body, 7)); // 9 away: half strength
        if (both)
            add_hub(w, tile_at(w, body, 13)); // 9 away the other side: half strength
        const entity_id c = add_consumer(w, tile_at(w, body, 10), corp, 10.0f, 100.0f);
        const lp_tick t = resolve_logistic_points(w);
        const lp_building_row* r = row_of(t, c);
        check(r != nullptr && approx(r->coverage, both ? 1.0f : 0.5f, 1e-3f),
              both ? "two half-strength sources top each other up to the whole request"
                   : "one half-strength source carries half the request");
    }

    // THE QUEUE: when the pool is short, the request inside the falloff is met whole
    // and the one beyond it gets what is left - not both the same thin share. The
    // far request's claim is its routed share (half its need), never more: distance
    // orders the queue and never raises the bill.
    {
        world w;
        const entity_id body   = make_row_body(w, 60);
        const entity_id nation = add_nation(w, body, 0, 59);
        set_capital(w, nation, tile_at(w, body, 10));
        add_market(w, body, tile_at(w, body, 50)); // NOT the anchor
        const entity_id corp = add_corp(w, "Producer", 100000.0f, nation);
        add_hub(w, tile_at(w, body, 10)); // pool lp_source_points[0] = 120
        const entity_id near = add_consumer(w, tile_at(w, body, 10), corp, 10.0f, 100.0f); // need 100, full
        const entity_id far  = add_consumer(w, tile_at(w, body, 13), corp, 10.0f, 100.0f); // need 100, half
        const lp_tick t = resolve_logistic_points(w);
        const lp_building_row* rn = row_of(t, near);
        const lp_building_row* rf = row_of(t, far);
        const float pool = lp_source_points[0];
        check(rn != nullptr && approx(rn->granted, 100.0f, 1e-2f),
              "a short pool meets the request inside the falloff whole (100 of 100)");
        check(rf != nullptr && approx(rf->granted, pool - 100.0f, 1e-2f),
              "and the request beyond it gets what is left (20), not a pro-rata share");
        float total = 0.0f;
        for (const lp_nation_row& n : t.nations)
            if (n.nation == nation)
                total = n.granted[0] + n.granted[1] + n.granted[2];
        check(approx(total, pool, 1e-2f), "the nation's ledger reports exactly what the queue handed out");
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
    l8_routed_requests();

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
