// ---------------------------------------------------------------------------
// cession_harness — BL-1460 (cession moves territory), requirement group
// `cession-moves-territory` (R1-R4). docs/politics/RELATIONS.md § 7, "Cession moves
// the territory" and "Ceding a capital moves it" (Ben, 2026-10-30, NR-1102).
// ---------------------------------------------------------------------------
// On the SHIPPED start of a library seed (harness_params.hpp's app mirror; default
// seed 28, "the quiet world" — the fewest polities, so the cheapest real start), a
// real war is declared and concluded by a real ceding peace, and every consumer of
// `tile_to_nation` is walked:
//
//   C1  THE GROUND AND WHAT IT DECIDES (R1): the ceded provinces' tiles are the
//       gainer's in `tile_to_nation` and in both tile lists; every building on them
//       changed owner (and the whole world keeps "a nation's buildings stand on its
//       ground"); the population centres, the markets and the jurisdiction they
//       answer to follow; the road tiles count to the gainer's network; fronts
//       re-derive; the loser's units on the ceded ground withdrew by the retreat
//       rule; no unit keeps a muster base another nation owns.
//   C2  NO STALE CACHE (R4): every derived read over the ceded ground and its ring —
//       the market catchment, Logistic Point coverage, the reach field, fronts, the
//       free source — on the live world (whose caches were WARMED before the peace)
//       equals the same read on a freshly loaded copy (whose caches are empty), and
//       the counterfactual: those reads did change across the peace.
//   C3  A CEDED CAPITAL MOVES (R2): to the remaining province with the largest
//       population centre (independently recomputed), its free hub with it; and a
//       peace that would cede the LAST province is refused, mutating nothing — after
//       real cessions in earlier wars whittled the loser down to one.
//   C4  WHITE PEACE (R3): occupied goals return to their owners, the occupiers'
//       units withdraw, and nothing changes hands.
//   C5  CONSERVATION, SAVE, DETERMINISM (R4): no treasury moves at a cession; the
//       ceded world round-trips the save with an equal state_hash; the same peace
//       on two copies gives byte-identical snapshots.
//
// Occupation itself (a battle moving `province_holder`) is the war harness's and the
// battle system's; here the holder is STAGED, as war_harness stages it, and every
// cession is a real `offer_peace` + `accept_peace`.
//
// Build: bash tools/verify/build_lua_harness.sh cession_harness --run
// Run:   build_gen/verify/cession_harness.exe [seed ...]   (default: 28)
// Exits non-zero on any FAIL.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/air_combat.hpp"
#include "world/battle_system.hpp"
#include "world/fronts.hpp"
#include "world/hex_neighbors.hpp"
#include "world/logistic_points.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/province.hpp"
#include "world/state_industry.hpp"
#include "world/war.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* row, const std::string& what)
{
    std::printf("  [%s] %s %s\n", ok ? "PASS" : "FAIL", row, what.c_str());
    if (ok) ++g_pass; else ++g_fail;
}

std::string fmt(const char* f, ...)
{
    char buf[900];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

using ull = unsigned long long;

std::string to_bytes(const world& w)
{
    std::ostringstream o(std::ios::binary);
    write_world_snapshot(w, o);
    return o.str();
}

bool from_bytes(const std::string& b, world& w)
{
    std::istringstream in(b, std::ios::binary);
    return read_world_snapshot(w, in);
}

entity_id owner(const world& w, entity_id t)
{
    const auto it = w.tile_to_nation.find(t);
    return it == w.tile_to_nation.end() ? null_entity : it->second;
}

/// The six hex neighbours of @p t that exist (east-west wrap, rows clipped).
std::vector<entity_id> neighbours(world& w, entity_id t)
{
    std::vector<entity_id> out;
    const auto tit = w.tiles.find(t);
    if (tit == w.tiles.end())
        return out;
    const tile_component tc = tit->second;
    const auto bit = w.bodies.find(tc.body);
    if (bit == w.bodies.end() || bit->second.grid_width <= 0)
        return out;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, tc.body);
    for (int side = 0; side < 6; ++side)
    {
        const hex_neighbors::coord c = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, side);
        if (c.gy < 0 || c.gy >= gh)
            continue;
        const int nx = ((c.gx % gw) + gw) % gw;
        const entity_id n = grid[static_cast<std::size_t>(c.gy) * gw + nx];
        if (n != null_entity)
            out.push_back(n);
    }
    return out;
}

/// @p n's land provinces by the territorial map, ascending.
std::vector<uint32_t> land_provinces(const world& w, entity_id n)
{
    std::vector<uint32_t> out;
    for (const province& pr : w.provinces.provinces)
        if (province_kind_of(w, pr) == province_kind::land && province_owner(w, pr.id) == n)
            out.push_back(pr.id);
    return out;
}

std::size_t holder_index(const world& w, uint32_t pid)
{
    for (std::size_t i = 0; i < w.provinces.provinces.size(); ++i)
        if (w.provinces.provinces[i].id == pid) return i;
    return 0;
}

entity_id free_hub_of(const world& w, entity_id n)
{
    for (const entity_id bid : w.nations.at(n).state_assets)
        if (capital_free_source(w, bid))
            return bid;
    return null_entity;
}

/// The capital RELATIONS.md § 7 names for @p n, computed here independently of
/// war.cpp: of its land provinces, the one holding its largest population centre on
/// its own ground (ties to the lower province id; within a province, to the lower
/// centre id); that centre's tile.
entity_id expected_capital(const world& w, entity_id n)
{
    const std::vector<uint32_t> provs = land_provinces(w, n);
    std::vector<entity_id> cids;
    for (const auto& [cid, pc] : w.population_centres) { (void)pc; cids.push_back(cid); }
    std::sort(cids.begin(), cids.end());
    uint32_t best_p = 0; int best_pop = -1; entity_id best_tile = null_entity;
    for (const uint32_t p : provs) // ascending: a tie keeps the lower id
    {
        int pop = -1; entity_id tile = null_entity;
        for (const entity_id cid : cids) // ascending: a tie keeps the lower centre
        {
            const entity_id ct = w.population_centre_tile.at(cid);
            if (w.provinces.province_of(ct) != p || owner(w, ct) != n) continue;
            if (w.population_centres.at(cid).population > pop) { pop = w.population_centres.at(cid).population; tile = ct; }
        }
        if (pop > best_pop) { best_pop = pop; best_p = p; best_tile = tile; }
    }
    (void)best_p;
    return best_tile;
}

bool nation_buildings_on_own_ground(const world& w, std::string& why)
{
    std::set<entity_id> seen;
    std::vector<entity_id> nids;
    for (const auto& [nid, nc] : w.nations) { (void)nc; nids.push_back(nid); }
    std::sort(nids.begin(), nids.end());
    for (const entity_id nid : nids)
        for (const entity_id bid : w.nations.at(nid).state_assets)
        {
            if (!seen.insert(bid).second) { why = fmt("building %llu listed twice", (ull)bid); return false; }
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end()) { why = fmt("dangling building %llu", (ull)bid); return false; }
            if (owner(w, bit->second.tile) != nid)
            {
                why = fmt("building %llu of nation %llu stands off its ground", (ull)bid, (ull)nid);
                return false;
            }
        }
    return true;
}

int shared_tiles(const world& w)
{
    std::map<entity_id, std::set<entity_id>> on;
    for (const auto& [uid, u] : w.units)
        if (u.count > 0 && u.position != null_entity) on[u.position].insert(u.owner);
    int n = 0;
    for (const auto& [t, s] : on) n += s.size() > 1 ? 1 : 0;
    return n;
}

bool same_fronts(const std::vector<front>& a, const std::vector<front>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].nation != b[i].nation || a[i].neighbour != b[i].neighbour || a[i].key != b[i].key
            || a[i].span != b[i].span)
            return false;
    return true;
}

/// Stage @p unit onto a free land tile of @p provs owned by @p ground_owner (no unit
/// stands there). Returns the tile, or null.
entity_id stage_unit(world& w, entity_id unit, const std::vector<uint32_t>& provs, entity_id ground_owner)
{
    std::set<entity_id> taken;
    for (const auto& [uid, u] : w.units) if (u.count > 0) taken.insert(u.position);
    for (const uint32_t p : provs)
        for (const entity_id t : w.provinces.find(p)->tiles)
            if (owner(w, t) == ground_owner && taken.count(t) == 0 && !is_water(w.tiles.at(t).substrate))
            {
                w.units.at(unit).position = t;
                w.units.at(unit).order    = movement_order{};
                return t;
            }
    return null_entity;
}

std::vector<entity_id> units_of(const world& w, entity_id n)
{
    std::vector<entity_id> out;
    for (const auto& [uid, u] : w.units) if (u.owner == n && u.count > 0) out.push_back(uid);
    std::sort(out.begin(), out.end());
    return out;
}

struct pick
{
    entity_id L = null_entity, G = null_entity;
    std::vector<uint32_t> goals;
};

/// The war: a loser L with >= 3 land provinces, its capital among them and a free
/// hub standing there; G the lowest-id nation bordering L's ground. Goals: L's
/// capital province, and L's other province holding the most of L's buildings (ties
/// to the lower id). Lowest-id L that qualifies.
pick choose(world& w)
{
    std::vector<entity_id> nids;
    for (const auto& [nid, nc] : w.nations) { (void)nc; nids.push_back(nid); }
    std::sort(nids.begin(), nids.end());
    for (const entity_id L : nids)
    {
        const nation_component& nc = w.nations.at(L);
        const std::vector<uint32_t> provs = land_provinces(w, L);
        if (provs.size() < 3 || nc.capital_tile == null_entity) continue;
        const uint32_t cap = w.provinces.province_of(nc.capital_tile);
        if (!std::binary_search(provs.begin(), provs.end(), cap) || free_hub_of(w, L) == null_entity) continue;
        std::set<entity_id> borders;
        for (const uint32_t p : provs)
            for (const entity_id t : w.provinces.find(p)->tiles)
                if (owner(w, t) == L)
                    for (const entity_id n : neighbours(w, t))
                        if (const entity_id o = owner(w, n); o != null_entity && o != L) borders.insert(o);
        if (borders.empty()) continue;
        std::map<uint32_t, int> built;
        for (const entity_id bid : nc.state_assets)
            built[w.provinces.province_of(w.buildings.at(bid).tile)] += 1;
        uint32_t second = 0; int most = -1;
        for (const uint32_t p : provs)
            if (p != cap && built[p] > most) { most = built[p]; second = p; }
        pick out;
        out.L = L; out.G = *borders.begin();
        out.goals = { cap, second };
        std::sort(out.goals.begin(), out.goals.end());
        return out;
    }
    return {};
}

/// Every tile of @p provs and its hex ring, ascending.
std::vector<entity_id> ground_and_ring(world& w, const std::vector<uint32_t>& provs)
{
    std::set<entity_id> s;
    for (const uint32_t p : provs)
        for (const entity_id t : w.provinces.find(p)->tiles)
        {
            s.insert(t);
            for (const entity_id n : neighbours(w, t)) s.insert(n);
        }
    return std::vector<entity_id>(s.begin(), s.end());
}

struct reads
{
    std::vector<entity_id> market;
    std::vector<float>     cover;
    std::map<entity_id, std::vector<front>> fronts;
};

reads read_all(world& w, const std::vector<entity_id>& tiles, const std::vector<entity_id>& nations, bool via_cache)
{
    reads r;
    lp_reach_memo memo;
    for (const entity_id t : tiles)
    {
        r.market.push_back(market_for_tile(w, t));
        r.cover.push_back(lp_coverage_at(w, memo, t));
    }
    for (const entity_id n : nations)
        r.fronts[n] = via_cache ? nation_fronts(w, n) : derive_fronts(w, n);
    return r;
}

/// Run the war and its ceding peace on @p w. Returns false if any verb refused.
bool run_cession(world& w, const sentiment_params& sp, const pick& pk)
{
    if (declare_war(w, sp, pk.G, pk.L, pk.goals) != war_result::applied) return false;
    for (const uint32_t p : pk.goals) w.province_holder[holder_index(w, p)] = pk.G; // staged occupation
    peace_terms c; c.kind = peace_kind::cession; c.gainer = pk.G; c.provinces = pk.goals;
    return offer_peace(w, pk.G, pk.L, c) == war_result::applied
        && accept_peace(w, sp, pk.L, pk.G) == war_result::applied;
}

void run_seed(lua_state& lua, std::uint32_t seed)
{
    std::printf("\n=== seed %u ===\n", static_cast<unsigned>(seed));
    world_params params = arc_params(world_arc::shipped);
    params.seed = seed;
    app_start_world out;
    build_app_start_world(lua, params, out);
    world& w = out.w;
    const sentiment_params& sp = out.reg.sentiment();

    const pick pk = choose(w);
    check(pk.L != null_entity, "C0", fmt("a war to cede: loser %llu, gainer %llu, goals %zu", (ull)pk.L, (ull)pk.G, pk.goals.size()));
    if (pk.L == null_entity) return;
    const entity_id L = pk.L, G = pk.G;
    const std::vector<uint32_t> goals = pk.goals;

    // Make sure the withdrawal has a unit to move: if none of L's units stands on the
    // goal ground, stage its lowest-id unit there.
    const auto on_goals = [&](const world& ww, entity_id t) {
        const uint32_t p = ww.provinces.province_of(t);
        return std::binary_search(goals.begin(), goals.end(), p);
    };
    {
        bool any = false;
        for (const entity_id uid : units_of(w, L)) any = any || on_goals(w, w.units.at(uid).position);
        if (!any && !units_of(w, L).empty())
        {
            const entity_id t = stage_unit(w, units_of(w, L).front(), goals, L);
            std::printf("  [info] no unit of the loser stood on the goals; staged unit %llu on tile %llu\n",
                        (ull)units_of(w, L).front(), (ull)t);
        }
    }

    const world pre = w; // the pre-war world, for the copies below
    const entity_id old_capital = w.nations.at(L).capital_tile;
    const entity_id hub = free_hub_of(w, L);
    const std::vector<entity_id> ring = ground_and_ring(w, goals);

    // Warm every cache the reads touch BEFORE the peace, so a missed invalidation
    // would be read stale afterwards.
    std::vector<entity_id> all_n;
    for (const auto& [nid, nc] : w.nations) { (void)nc; all_n.push_back(nid); }
    std::sort(all_n.begin(), all_n.end());
    const reads before = read_all(w, ring, all_n, true);
    reanchor_fronts(w);
    for (const auto& [bid, b] : w.bodies) (void)body_reach_field(w, bid);

    // What stands on the goal ground, before.
    std::vector<entity_id> ceded_tiles;
    for (const uint32_t p : goals)
        for (const entity_id t : w.provinces.find(p)->tiles)
            if (owner(w, t) == L) ceded_tiles.push_back(t);
    std::sort(ceded_tiles.begin(), ceded_tiles.end());
    const auto was_ceded = [&](entity_id t) { return std::binary_search(ceded_tiles.begin(), ceded_tiles.end(), t); };
    std::vector<entity_id> bld_on;
    for (const entity_id bid : w.nations.at(L).state_assets)
        if (was_ceded(w.buildings.at(bid).tile) && bid != hub) bld_on.push_back(bid);
    std::vector<entity_id> centres_on, markets_on, units_on;
    for (const auto& [cid, ct] : w.population_centre_tile) if (was_ceded(ct)) centres_on.push_back(cid);
    for (const auto& [mid, mc] : w.markets) if (was_ceded(mc.centre_tile)) markets_on.push_back(mid);
    for (const entity_id uid : units_of(w, L)) if (was_ceded(w.units.at(uid).position)) units_on.push_back(uid);
    int roads_on = 0;
    for (const entity_id t : ceded_tiles) roads_on += tile_has_road(w.tiles.at(t)) ? 1 : 0;
    const auto road_count = [](const world& ww, entity_id n) {
        int c = 0;
        for (const auto& [t, o] : ww.tile_to_nation) if (o == n && tile_has_road(ww.tiles.at(t))) ++c;
        return c;
    };
    const int roads_L0 = road_count(w, L), roads_G0 = road_count(w, G);
    std::map<entity_id, float> treasury0;
    for (const entity_id n : all_n) treasury0[n] = w.nations.at(n).treasury;
    std::printf("  [info] ceding %zu tiles in %zu provinces: %zu buildings (+ the free hub %llu), %zu centres, "
                "%zu markets, %zu loser units, %d road tiles\n", ceded_tiles.size(), goals.size(), bld_on.size(),
                (ull)hub, centres_on.size(), markets_on.size(), units_on.size(), roads_on);

    const bool ran = run_cession(w, sp, pk);
    check(ran, "C0b", "the war declares and its ceding peace is offered and accepted");
    if (!ran) return;

    // ---- C1: the ground and what it decides ---------------------------------
    {
        bool tiles_ok = true;
        for (const entity_id t : ceded_tiles) tiles_ok = tiles_ok && owner(w, t) == G;
        const std::set<entity_id> lt(w.nations.at(L).tiles.begin(), w.nations.at(L).tiles.end());
        const std::set<entity_id> gt(w.nations.at(G).tiles.begin(), w.nations.at(G).tiles.end());
        bool lists_ok = gt.size() == w.nations.at(G).tiles.size();
        for (const entity_id t : ceded_tiles) lists_ok = lists_ok && lt.count(t) == 0 && gt.count(t) == 1;
        bool prov_ok = true;
        for (const uint32_t p : goals) prov_ok = prov_ok && province_owner(w, p) == G && province_holder_for(w, p) == G;
        check(tiles_ok && lists_ok && prov_ok, "C1a",
              fmt("the %zu ceded tiles are the gainer's in tile_to_nation (%s), in both tile lists (%s), and each "
                  "ceded province is the gainer's by plurality and holder (%s)", ceded_tiles.size(),
                  tiles_ok ? "yes" : "NO", lists_ok ? "yes" : "NO", prov_ok ? "yes" : "NO"));
    }
    {
        int moved = 0;
        for (const entity_id bid : bld_on) moved += owner_nation_of(w, bid) == G ? 1 : 0;
        std::string why;
        const bool ground = nation_buildings_on_own_ground(w, why);
        check(moved == static_cast<int>(bld_on.size()) && ground, "C1b",
              fmt("%d of %zu buildings on the ceded ground changed owner with it; every nation's building stands "
                  "on its own ground, none listed twice (%s)", moved, bld_on.size(), ground ? "yes" : why.c_str()));
    }
    {
        int centres = 0, markets = 0;
        for (const entity_id cid : centres_on) centres += owner(w, w.population_centre_tile.at(cid)) == G ? 1 : 0;
        for (const entity_id mid : markets_on) markets += owner(w, w.markets.at(mid).centre_tile) == G ? 1 : 0;
        // Jurisdiction: the law reads the ground of the flow (law.hpp, `tile_to_nation`),
        // so every building there now answers to the gainer.
        int juris = 0;
        for (const entity_id bid : bld_on) juris += owner(w, w.buildings.at(bid).tile) == G ? 1 : 0;
        check(centres == static_cast<int>(centres_on.size()) && markets == static_cast<int>(markets_on.size())
                  && juris == static_cast<int>(bld_on.size()), "C1c",
              fmt("population centres (%d/%zu), market centres (%d/%zu) and the jurisdiction over the buildings "
                  "(%d/%zu) on the ceded ground follow it to the gainer", centres, centres_on.size(), markets,
                  markets_on.size(), juris, bld_on.size()));
    }
    {
        const int dl = roads_L0 - road_count(w, L), dg = road_count(w, G) - roads_G0;
        check(dl == roads_on && dg == roads_on, "C1d",
              fmt("road network membership: the loser's road tiles fell by %d and the gainer's rose by %d (the %d "
                  "on the ceded ground)", dl, dg, roads_on));
    }
    {
        const std::vector<front> lf = nation_fronts(w, L), gf = nation_fronts(w, G);
        bool off = true;
        for (const front& f : lf) for (const entity_id t : f.span) off = off && !was_ceded(t);
        bool keys = true;
        for (const auto& [uid, u] : w.units) if (u.front != null_entity) keys = keys && owner(w, u.front) == u.owner;
        const bool changed = !same_fronts(before.fronts.at(L), lf);
        check(same_fronts(lf, derive_fronts(w, L)) && same_fronts(gf, derive_fronts(w, G)) && off && keys && changed,
              "C1e", fmt("fronts re-derive: the cached fronts equal a fresh derivation for both parties, no loser front "
                         "runs over ceded ground (%s), every unit's front key is ground its nation holds (%s), and the "
                         "loser's fronts changed (%s)", off ? "yes" : "NO", keys ? "yes" : "NO", changed ? "yes" : "NO"));
    }
    {
        int still = 0, cleared = 0;
        for (const entity_id uid : units_on)
        {
            const auto it = w.units.find(uid);
            if (it == w.units.end()) continue;
            still += was_ceded(it->second.position) ? 1 : 0;
            cleared += it->second.order.path.empty() ? 1 : 0;
        }
        int off_base = 0;
        for (const auto& [uid, u] : w.units)
            if (u.muster_base != null_entity && w.buildings.count(u.muster_base) && owner_nation_of(w, u.muster_base) != u.owner)
                ++off_base;
        check(still == 0 && cleared == static_cast<int>(units_on.size()) && shared_tiles(w) == 0 && off_base == 0,
              "C1f", fmt("the loser's %zu units on the ceded ground withdrew by the retreat rule (%d still on it, "
                         "orders cleared %d); no tile holds two nations' units (%d); no unit keeps a muster base "
                         "another nation owns (%d)", units_on.size(), still, cleared, shared_tiles(w), off_base));
    }

    // ---- C2: no stale cache ----------------------------------------------------
    {
        world fresh;
        const bool loaded = from_bytes(to_bytes(w), fresh);
        const reads live = read_all(w, ring, all_n, true);
        const reads cold = loaded ? read_all(fresh, ring, all_n, false) : reads{};
        int mk_diff = 0, cv_diff = 0, mk_moved = 0, cv_moved = 0, fr_diff = 0;
        for (std::size_t i = 0; loaded && i < ring.size(); ++i)
        {
            mk_diff  += live.market[i] != cold.market[i] ? 1 : 0;
            cv_diff  += live.cover[i]  != cold.cover[i]  ? 1 : 0;
            mk_moved += live.market[i] != before.market[i] ? 1 : 0;
            cv_moved += live.cover[i]  != before.cover[i]  ? 1 : 0;
        }
        for (const entity_id n : all_n) fr_diff += loaded && !same_fronts(live.fronts.at(n), cold.fronts.at(n)) ? 1 : 0;
        bool reach_same = loaded;
        for (const auto& [bid, b] : w.bodies)
            reach_same = reach_same && body_reach_field(w, bid) == body_reach_field(fresh, bid);
        const bool free_same = loaded && capital_free_source(w, hub) == capital_free_source(fresh, hub);
        check(loaded && mk_diff == 0 && cv_diff == 0 && fr_diff == 0 && reach_same && free_same, "C2a",
              fmt("no consumer of tile_to_nation reads stale: over %zu tiles of the ceded ground and its ring, the live "
                  "world (caches warmed before the peace) and a freshly loaded copy agree on the market catchment "
                  "(%d differ), Logistic Point coverage (%d differ), every nation's fronts (%d differ), the reach field "
                  "(%s) and the free source (%s)", ring.size(), mk_diff, cv_diff, fr_diff,
                  reach_same ? "equal" : "DIFFERS", free_same ? "equal" : "DIFFERS"));
        check(cv_moved > 0, "C2b",
              fmt("counterfactual: the peace moved those reads — coverage changed on %d tiles, the catchment on %d — "
                  "so C2a compares answers a stale cache would have got wrong", cv_moved, mk_moved));
    }

    // ---- C3: the capital moves; the last province is never ceded ----------------
    {
        const nation_component& ln = w.nations.at(L);
        const entity_id expect = expected_capital(w, L);
        const bool moved_cap = was_ceded(old_capital) && ln.capital_tile != old_capital && ln.capital_tile == expect;
        const bool hub_ok = hub != null_entity && w.buildings.count(hub) && w.buildings.at(hub).tile == ln.capital_tile
                         && owner_nation_of(w, hub) == L && capital_free_source(w, hub);
        check(moved_cap && hub_ok, "C3a",
              fmt("the ceded capital (tile %llu) moved to tile %llu, the largest centre on the loser's remaining "
                  "provinces (independently: %llu), and the free hub %llu moved with it and is its free source (%s)",
                  (ull)old_capital, (ull)ln.capital_tile, (ull)expect, (ull)hub, hub_ok ? "yes" : "NO"));
    }
    {
        // C8: the ceded capital leaves the loser's endemic demand weights unchanged —
        // they hash the GENERATION capital, which the peace does not move. The
        // counterfactual: hashed from the live (moved) capital, they would differ.
        const nation_component& before_nc = pre.nations.at(L);
        const nation_component& after_nc  = w.nations.at(L);
        nation_component live = after_nc;
        live.generation_capital_tile = null_entity; // read the live capital instead
        const float spread = 0.5f;
        int same = 0, moved_live = 0;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float b = nation_endemic_preference(L, &before_nc, r, spread);
            same       += nation_endemic_preference(L, &after_nc, r, spread) == b ? 1 : 0;
            moved_live += nation_endemic_preference(L, &live, r, spread) != b ? 1 : 0;
        }
        check(same == static_cast<int>(resource_count) && moved_live > 0
                  && after_nc.generation_capital_tile == old_capital && before_nc.generation_capital_tile == old_capital,
              "C8", fmt("a ceded capital leaves the loser's endemic demand weights unchanged (%d of %zu equal; the "
                        "generation capital stays tile %llu); hashed from the moved capital %d would have re-rolled",
                        same, resource_count, (ull)after_nc.generation_capital_tile, moved_live));
        // C9: the v71 section carries it; a v70 stream (no section) loads it equal to
        // the capital.
        world back71, back70;
        const bool r71 = from_bytes(to_bytes(w), back71);
        std::ostringstream o70(std::ios::binary);
        write_world_snapshot_pre_gen_capital(w, o70);
        const std::string b70 = o70.str();
        const bool r70 = from_bytes(b70, back70);
        bool eq70 = r70;
        for (const auto& [nid, nc] : back70.nations) eq70 = eq70 && nc.generation_capital_tile == nc.capital_tile;
        check(r71 && back71.nations.at(L).generation_capital_tile == old_capital && r70 && eq70
                  && b70.size() == to_bytes(w).size() - world_save_gen_capital_section_bytes(w) - world_save_recruits_section_bytes(w) /* BL-1463 (v73) */ - world_save_kit_goods_bytes(w), // BL-1422: v70 arrays are 49 wide
              "C9", "the v71 stream carries the generation capital through a save; a v70 stream (today's less the "
                    "section) loads every nation's generation capital equal to its capital");
    }
    {
        // Whittle the loser down by real cessions to other nations, then refuse the last.
        world v = w;
        std::vector<entity_id> takers;
        for (const entity_id n : all_n) if (n != L && n != G) takers.push_back(n);
        bool ok = true;
        int wars = 0;
        std::size_t ti = 0;
        while (ok && land_provinces(v, L).size() > 1 && wars < 200 && !takers.empty())
        {
            const std::vector<uint32_t> lp = land_provinces(v, L);
            std::vector<uint32_t> g;
            for (const uint32_t p : lp) if (g.size() < std::min<std::size_t>(3, lp.size() - 1)) g.push_back(p);
            const entity_id t = takers[ti++ % takers.size()];
            if (ti % takers.size() == 0) v.current_econ_tick += k_war_truce_ticks;
            pick q; q.L = L; q.G = t; q.goals = g;
            ok = run_cession(v, sp, q);
            ++wars;
        }
        const std::vector<uint32_t> last = land_provinces(v, L);
        check(ok && last.size() == 1, "C3b", fmt("(the loser is down to one province after %d more real cessions)", wars));
        if (ok && last.size() == 1)
        {
            v.current_econ_tick += k_war_truce_ticks;
            const entity_id t = takers.front();
            const bool declared = declare_war(v, sp, t, L, last) == war_result::applied;
            v.province_holder[holder_index(v, last[0])] = t;
            v.wars.at({ t, L }).defender_men_lost = 1; // score 100: impose is reachable
            peace_terms c; c.kind = peace_kind::cession; c.gainer = t; c.provinces = last;
            const std::string b0 = to_bytes(v);
            const uint64_t h0 = v.state_hash(0);
            const bool refused = offer_peace(v, t, L, c) == war_result::rejected_state
                              && impose_peace(v, sp, t, L, c) == war_result::rejected_state;
            check(declared && refused && to_bytes(v) == b0 && v.state_hash(0) == h0 && v.nations.count(L) == 1,
                  "C3c", "a peace that would cede the loser's LAST province is refused, offered or imposed, "
                         "mutating nothing; the nation stays on the map (NR-1102)");
        }
    }

    // ---- C4: white peace ----------------------------------------------------------
    {
        world v = pre;
        const std::vector<uint32_t> gp = land_provinces(v, G);
        const std::vector<uint32_t> answer = { gp.front() };
        bool ok = declare_war(v, sp, G, L, goals) == war_result::applied
               && answer_war(v, L, G, answer) == war_result::applied;
        for (const uint32_t p : goals) v.province_holder[holder_index(v, p)] = G;
        v.province_holder[holder_index(v, answer[0])] = L;
        // Stage occupiers: two of G's units on L's goal ground, one of L's on G's.
        std::vector<entity_id> g_occ, l_occ;
        for (const entity_id uid : units_of(v, G))
        {
            if (g_occ.size() == 2) break;
            if (stage_unit(v, uid, goals, L) != null_entity) g_occ.push_back(uid);
        }
        for (const entity_id uid : units_of(v, L))
        {
            if (on_goals(v, v.units.at(uid).position)) continue;
            if (stage_unit(v, uid, answer, G) != null_entity) { l_occ.push_back(uid); break; }
        }
        std::vector<std::pair<entity_id, entity_id>> map0(v.tile_to_nation.begin(), v.tile_to_nation.end());
        std::sort(map0.begin(), map0.end());
        const std::vector<entity_id> la0 = v.nations.at(L).state_assets, ga0 = v.nations.at(G).state_assets;
        ok = ok && offer_peace(v, L, G, peace_terms{}) == war_result::applied
                && accept_peace(v, sp, G, L) == war_result::applied;
        bool returned = true;
        for (const uint32_t p : goals) returned = returned && province_holder_for(v, p) == L;
        returned = returned && province_holder_for(v, answer[0]) == G;
        int stayed = 0;
        for (const entity_id uid : g_occ)
            stayed += std::binary_search(goals.begin(), goals.end(), v.provinces.province_of(v.units.at(uid).position))
                      && owner(v, v.units.at(uid).position) == L ? 1 : 0;
        for (const entity_id uid : l_occ)
            stayed += v.provinces.province_of(v.units.at(uid).position) == answer[0]
                      && owner(v, v.units.at(uid).position) == G ? 1 : 0;
        std::vector<std::pair<entity_id, entity_id>> map1(v.tile_to_nation.begin(), v.tile_to_nation.end());
        std::sort(map1.begin(), map1.end());
        const bool unchanged = map0 == map1 && v.nations.at(L).state_assets == la0
                            && v.nations.at(G).state_assets == ga0 && v.province_cessions.empty();
        check(ok && returned && stayed == 0 && !g_occ.empty() && !l_occ.empty() && unchanged && shared_tiles(v) == 0,
              "C4", fmt("a white peace returns every occupied goal to its owner (%s); the %zu+%zu staged occupiers "
                        "withdrew by the retreat rule (%d stayed); nothing changed hands — the territorial map, both "
                        "nations' buildings and the cession record are unchanged (%s)", returned ? "yes" : "NO",
                        g_occ.size(), l_occ.size(), stayed, unchanged ? "yes" : "NO"));
    }

    // ---- C6: a cession right after a load = the same cession in an unbroken run -----
    {
        // Its own war: the goal is the L province holding the key of an L front toward
        // G (its key on L's own ground), so the assignment is keyed in the ceded province.
        world v = pre;
        entity_id key = null_entity;
        for (const front& f : derive_fronts(v, L))
            if (f.neighbour == G && owner(v, f.key) == L
                && province_kind_of(v, v.provinces.province_of(f.key)) == province_kind::land)
            {
                key = f.key;
                break;
            }
        const std::vector<uint32_t> g6 = { key != null_entity ? v.provinces.province_of(key) : 0u };
        bool ok = key != null_entity && land_provinces(v, L).size() > 1
               && declare_war(v, sp, G, L, g6) == war_result::applied;
        if (ok) v.province_holder[holder_index(v, g6[0])] = G;
        peace_terms c; c.kind = peace_kind::cession; c.gainer = G; c.provinces = g6;
        ok = ok && offer_peace(v, G, L, c) == war_result::applied;
        const std::vector<entity_id> lu = units_of(v, L);
        entity_id assigned = null_entity;
        if (ok && !lu.empty())
        {
            assigned = lu.front();
            v.units.at(assigned).front = key;
        }
        reanchor_fronts(v); // the unbroken run's cache: built and current before the peace
        world a = v;
        world b;
        const bool loaded = from_bytes(to_bytes(v), b); // a save and load of the same tick
        const bool ra = ok && accept_peace(a, sp, L, G) == war_result::applied;
        const bool rb = ok && loaded && accept_peace(b, sp, L, G) == war_result::applied;
        const entity_id now_a = assigned != null_entity && a.units.count(assigned) ? a.units.at(assigned).front : null_entity;
        const bool equal = ra && rb && to_bytes(a) == to_bytes(b) && a.state_hash(0) == b.state_hash(0);
        check(equal && assigned != null_entity && now_a != null_entity && now_a != key, "C6",
              fmt("the same peace on an unbroken run and on a save+load of the same tick gives byte-identical worlds "
                  "(%s): unit %llu, assigned to the front keyed %llu in the ceded province %u, is remapped to front "
                  "%llu on both paths, not dropped on the loaded one", equal ? "equal" : "DIFFER", (ull)assigned,
                  (ull)key, (unsigned)g6[0], (ull)now_a));
    }

    // ---- C7: air wings at a lost airbase ----------------------------------------------
    {
        const auto add_airbase = [](world& ww, entity_id n, entity_id tile) {
            const entity_id bid = ww.create_entity();
            building_component bc{};
            bc.tile = tile; bc.type = building_type::airbase; bc.ticks_remaining = 0;
            ww.buildings[bid] = bc;
            ww.stockpiles[bid] = stockpile_component{};
            ww.nations.at(n).state_assets.push_back(bid);
            return bid;
        };
        const auto add_wing = [](world& ww, entity_id n, entity_id base, uint32_t prov) {
            const entity_id wid = ww.create_entity();
            air_wing wg{};
            wg.owner = n; wg.airbase = base; wg.mission = air_mission::recon; wg.provinces = { prov };
            ww.wings[wid] = wg;
            return wid;
        };
        // A land tile of L's in a goal (the lost base), and L's remaining land tiles.
        entity_id lost_tile = null_entity;
        std::vector<entity_id> home;
        for (const auto& [t, o] : pre.tile_to_nation)
            if (o == L && !is_water(pre.tiles.at(t).substrate))
            {
                if (on_goals(pre, t)) { if (lost_tile == null_entity || t < lost_tile) lost_tile = t; }
                else home.push_back(t);
            }
        std::sort(home.begin(), home.end());
        std::sort(home.begin(), home.end(), [&](entity_id x, entity_id y) {
            const int dx = air_tile_distance(pre, lost_tile, x), dy = air_tile_distance(pre, lost_tile, y);
            return dx != dy ? dx < dy : x < y;
        });
        if (lost_tile != null_entity && home.size() >= 2)
        {
            // Rebase: two remaining airbases, the FAR one created first (lower id), so the
            // nearest wins on distance, not id.
            world v = pre;
            const entity_id far_b  = add_airbase(v, L, home.back());
            const entity_id near_b = add_airbase(v, L, home.front());
            const entity_id lost_b = add_airbase(v, L, lost_tile);
            const entity_id wid = add_wing(v, L, lost_b, goals.front());
            const bool flew = air_wing_flies(v, v.wings.at(wid));
            const bool ran2 = run_cession(v, sp, pk);
            const bool rebased = v.wings.count(wid) && v.wings.at(wid).airbase == near_b;
            const bool flies = rebased && air_wing_flies(v, v.wings.at(wid));
            air_wing stale = v.wings.count(wid) ? v.wings.at(wid) : air_wing{};
            stale.airbase = lost_b; // the lost base, now the gainer's
            const bool grounded = owner_nation_of(v, lost_b) == G && !air_wing_flies(v, stale);
            check(ran2 && flew && rebased && flies, "C7a",
                  fmt("a loser wing whose airbase passed rebases to the loser's NEAREST own airbase (%llu at %d hex, "
                      "not %llu at %d) and flies from it", (ull)near_b, air_tile_distance(v, lost_tile, home.front()),
                      (ull)far_b, air_tile_distance(v, lost_tile, home.back())));
            check(grounded, "C7b", "a wing based at an airbase another nation owns does not fly (wing_flies checks "
                                   "the base is its owner's)");
            // Disband: the lost base is the loser's only airbase.
            world u = pre;
            const entity_id only_b = add_airbase(u, L, lost_tile);
            const entity_id wid2 = add_wing(u, L, only_b, goals.front());
            const bool ran3 = run_cession(u, sp, pk);
            check(ran3 && u.wings.count(wid2) == 0, "C7c",
                  "a loser wing whose airbase passed, with no airbase of the loser's left, is disbanded (the upkeep "
                  "orphan rule)");
        }
        else
            check(false, "C7", "no tile to stage the airbases on");
    }

    // ---- C5: conservation, save, determinism ---------------------------------------
    {
        int moved = 0;
        for (const entity_id n : all_n) moved += w.nations.at(n).treasury != treasury0.at(n) ? 1 : 0;
        world back;
        const bool read = from_bytes(to_bytes(w), back);
        check(moved == 0, "C5a", fmt("a cession moves no money: %d treasuries changed", moved));
        check(read && back.state_hash(0) == w.state_hash(0) && to_bytes(back) == to_bytes(w), "C5b",
              fmt("the ceded world round-trips the save (read %d, hash %s, bytes %s)", read ? 1 : 0,
                  read && back.state_hash(0) == w.state_hash(0) ? "equal" : "DIFFERS",
                  read && to_bytes(back) == to_bytes(w) ? "equal" : "DIFFER"));
        world a = pre, b = pre;
        const bool ra = run_cession(a, sp, pk), rb = run_cession(b, sp, pk);
        check(ra && rb && to_bytes(a) == to_bytes(b) && a.state_hash(0) == b.state_hash(0), "C5c",
              "deterministic: the same peace on two copies of the pre-war world gives byte-identical snapshots and hashes");
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
        seeds.push_back(static_cast<std::uint32_t>(std::strtoul(argv[i], nullptr, 10)));
    if (seeds.empty())
        seeds.push_back(28);

    lua_state lua;
    for (const std::uint32_t s : seeds)
        run_seed(lua, s);

    std::printf("\ncession_harness: %d pass, %d fail -- %s\n", g_pass, g_fail, g_fail == 0 ? "ALL PASS" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}
