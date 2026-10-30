// ---------------------------------------------------------------------------
// railway_harness — BL-1255 (railways), the headless proof
// ---------------------------------------------------------------------------
// docs/economy/LOGISTICS.md § 4c (Ben, 2026-10-13), PRODUCTION.md § Infrastructure
// buildings (the station), NATIONS.md § State industry, INDUSTRIALISATION.md § Who
// lays rail. On the SHIPPED start (harness_params.hpp's app mirror), per seed:
//
//   R1  GENERATION. The Industrialisation span's links are stamped as track and
//       the fold hands them over: track tiles and stations reported per seed and
//       per owner; every track tile's owner is null, a MAJOR nation (state
//       industry, on its own ground) or a minor's BACKGROUND firm; every station
//       stands on a CITY tile TOUCHING track, is owned once (a major's in its
//       state_assets), zero-staffed and complete.
//   R2  A HAUL BOARDS ONLY AT STATIONS. Every market pair the router sends by rail
//       names two stations of the network; its rail leg's interior is track, end
//       to end; the convoy lane built from it passes both stations.
//   R3  A CUT LINE STRANDS STATIONS. A hostile unit stood on a rail leg's interior
//       tile cuts it: the network's step count between those stations rises or
//       goes to -1, and the pair is no longer priced over the cut tile. The unit
//       leaves: the network is restored.
//   R4  THE PLAYER'S VERBS through agent_protocol::service_line (the wire's parser
//       and actor gate) as the seat: every malformed, unauthorised or illegal
//       rail command is rejected and leaves state_hash unchanged; lay_track at a
//       city and onward applies, charging exactly `track_tile_cost` from the
//       treasury per tile; a station on the new line applies and joins the
//       nation's state_assets. A rival champion's lay_track (in-process) and a
//       corporation's own station build are refused. Short of funds: refused.
//   R5  UPKEEP. apply_budget charges each owner `track_upkeep_per_tile` per tile
//       (a nation's ledger maintenance carries it; never below zero).
//   R6  THE SAVE. v49 round-trips the track field and a riding convoy's stations
//       with an equal state_hash; a v48 stream (no rail section) reads, no rail.
//   M   THE HAULAGE SHIFT (the item's own requirement, REPORT): every ordered
//       pair of markets on the home body priced at one unit through
//       price_market_leg — the router scorer and execution share — with the rail
//       handover held back (BEFORE: no stations) and done (AFTER). Pairs bucketed
//       by their all-road path cost; the share sent by rail per bucket, and the
//       share of LONG land hauls (all-road cost >= --long, default 40) on rail
//       where a line serves them. --sweep re-reads AFTER at several rail prices.
//
// Build: bash tools/verify/build_lua_harness.sh railway_harness
// Run:   build_gen/verify/railway_harness.exe [--sweep] [--long N] [--serve C] [--measure-only] [--quiet] [seed ...]
//        (default seed 2; the mechanics rows run on the FIRST seed, M on every seed)
// Exits non-zero on any FAIL.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/budget_system.hpp"
#include "world/campaign_settle.hpp"
#include "world/construction.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/stance.hpp"
#include "world/state_industry.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include "../../src/core/agent_protocol.cpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

int  g_fail = 0;
int  g_pass = 0;
bool g_sweep = false;
bool g_measure_only = false;
bool g_quiet = false;
int  g_time_ticks = 0;
float g_long = 40.0f;
float g_serve = 15.0f; ///< a station "serves" a centre within this land path cost

void check(bool ok, const char* row, const std::string& what)
{
    std::printf("  [%s] %s %s\n", ok ? "PASS" : "FAIL", row, what.c_str());
    if (ok) ++g_pass; else ++g_fail;
}

std::string fmt(const char* f, ...)
{
    char buf[640];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

std::string serve(world& w, const recipe_registry& reg, const agent_protocol::session& s,
                  const std::string& line)
{
    std::string out;
    agent_protocol::service_line(line, w, reg, w.current_econ_tick, s, out);
    return out;
}

bool result_is(const std::string& out, const char* r)
{
    return out.find(std::string("result=") + r + " ") != std::string::npos
        || out.find(std::string("result=") + r + "\n") != std::string::npos;
}

std::string to_bytes(const world& w)
{
    std::ostringstream os(std::ios::binary);
    write_world_snapshot(w, os);
    return os.str();
}

bool from_bytes(const std::string& b, world& w)
{
    std::istringstream is(b, std::ios::binary);
    return read_world_snapshot(w, is);
}

bool is_city(const world& w, entity_id tile)
{
    for (const auto& [c, t] : w.population_centre_tile)
        if (t == tile) return true;
    return false;
}

std::vector<entity_id> sorted_markets_on(const world& w, entity_id body)
{
    std::vector<entity_id> ids;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body && mc.centre_tile != null_entity) ids.push_back(mid);
    std::sort(ids.begin(), ids.end());
    return ids;
}

/// The neighbouring tiles of @p tile (N, E, S, W), columns wrapping.
std::vector<entity_id> neighbours_of(world& w, entity_id tile)
{
    std::vector<entity_id> out;
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end()) return out;
    const body_component& b = w.bodies.at(tit->second.body);
    const std::vector<entity_id>& grid = body_tile_grid(w, tit->second.body);
    const int x = tit->second.grid_x, y = tit->second.grid_y, gw = b.grid_width, gh = b.grid_height;
    const int c[4] = {y > 0 ? (y - 1) * gw + x : -1, y * gw + (x + 1) % gw,
                      y + 1 < gh ? (y + 1) * gw + x : -1, y * gw + (x + gw - 1) % gw};
    for (const int i : c)
        if (i >= 0 && static_cast<std::size_t>(i) < grid.size()) out.push_back(grid[static_cast<std::size_t>(i)]);
    return out;
}

// ---------------------------------------------------------------------------
// M -- the haulage shift
// ---------------------------------------------------------------------------
struct shift_reading
{
    static constexpr int nb = 6;
    int pairs[nb] = {};
    int rail[nb]  = {};
    int served[nb] = {}; ///< pairs where both centres reach a station by land
    int served_rail[nb] = {};
    int long_pairs = 0, long_rail = 0, long_served = 0, long_served_rail = 0;
    int short_pairs = 0, short_rail = 0;
};

int bucket_of(float c)
{
    if (c < 10.0f) return 0;
    if (c < 20.0f) return 1;
    if (c < 40.0f) return 2;
    if (c < 80.0f) return 3;
    if (c < 160.0f) return 4;
    return 5;
}
const char* const k_bucket[shift_reading::nb] = {"<10", "10-20", "20-40", "40-80", "80-160", "160+"};

shift_reading read_shift(world& w, const recipe_registry& reg)
{
    shift_reading r;
    const logistics_nodes nodes = collect_logistics_nodes(w);
    // The all-road reference: the same router with the rail priced out of reach.
    recipe_registry no_rail = reg;
    {
        recipe_registry::rail_params rp = reg.rail();
        rp.cost_per_step = 1.0e6f;
        no_rail.set_rail(rp);
    }
    const std::vector<entity_id> mids = sorted_markets_on(w, w.home_body);
    const world::rail_network net = body_rail_network(w, w.home_body);
    for (const entity_id a : mids)
        for (const entity_id b : mids)
        {
            if (a == b) continue;
            const convoy_leg road = price_market_leg(w, no_rail, nodes, a, b, 1.0f);
            if (!road.viable || road.port_a != null_entity) continue; // land hauls only
            // The all-road path cost, as the router priced it (the leg cost over the land rate).
            const float c = road.cost / std::max(1.0e-9f, reg.logistics_cost(convoy_mode::land));
            const int k = bucket_of(c);
            const convoy_leg got = price_market_leg(w, reg, nodes, a, b, 1.0f);
            const bool on_rail = got.viable && got.station_a != null_entity;
            ++r.pairs[k];
            if (on_rail) ++r.rail[k];
            // SERVED BY A LINE: two stations joined by track, each within a short
            // land leg (path cost <= g_serve) of its market centre.
            bool served = false;
            if (net.stations.size() >= 2)
            {
                const entity_id ca = w.markets.at(a).centre_tile, cb = w.markets.at(b).centre_tile;
                std::vector<char> near_a(net.stations.size(), 0), near_b(net.stations.size(), 0);
                for (std::size_t i = 0; i < net.stations.size(); ++i)
                {
                    const logistics_path& pa = intra_body_leg_path(w, w.home_body, ca, net.stations[i], leg_domain::land);
                    near_a[i] = pa.reachable && pa.cost <= g_serve;
                    const logistics_path& pb = intra_body_leg_path(w, w.home_body, net.stations[i], cb, leg_domain::land);
                    near_b[i] = pb.reachable && pb.cost <= g_serve;
                }
                for (std::size_t i = 0; i < net.stations.size() && !served; ++i)
                    for (std::size_t j = 0; j < net.stations.size() && !served; ++j)
                        served = i != j && near_a[i] && near_b[j] && net.steps[i][j] >= 0;
            }
            if (served) { ++r.served[k]; if (on_rail) ++r.served_rail[k]; }
            if (c >= g_long) { ++r.long_pairs; if (on_rail) ++r.long_rail; if (served) { ++r.long_served; if (on_rail) ++r.long_served_rail; } }
            if (c < 20.0f)   { ++r.short_pairs; if (on_rail) ++r.short_rail; }
        }
    return r;
}

void print_shift(const char* label, const shift_reading& r)
{
    std::printf("    %-22s", label);
    for (int k = 0; k < shift_reading::nb; ++k)
        std::printf(" %s:%d/%d", k_bucket[k], r.rail[k], r.pairs[k]);
    std::printf("\n    %-22s served by a line:", "");
    for (int k = 0; k < shift_reading::nb; ++k)
        std::printf(" %s:%d/%d", k_bucket[k], r.served_rail[k], r.served[k]);
    const double ls = r.long_pairs ? 100.0 * r.long_rail / r.long_pairs : 0.0;
    const double ss = r.short_pairs ? 100.0 * r.short_rail / r.short_pairs : 0.0;
    std::printf("\n    %-22s long (>= %.0f) on rail %d/%d = %.1f%%  (served: %d/%d on rail)  "
                "short (<20) on rail %d/%d = %.1f%%\n",
                "", static_cast<double>(g_long), r.long_rail, r.long_pairs, ls, r.long_served_rail,
                r.long_served, r.short_rail, r.short_pairs, ss);
}

// ---------------------------------------------------------------------------

void run_seed(lua_state& lua, std::uint32_t seed, bool mechanics)
{
    std::printf("\n=== seed %u ===\n", static_cast<unsigned>(seed));
    world_params params = arc_params(world_arc::shipped);
    params.seed = seed;

    app_start_world out;
    build_app_base_world(lua, params, out);
    harness_charter_input ci;
    ci.skip_rail_handover = true; // held back: BEFORE first, on the same world
    apply_app_start_landscape(out, ci);
    world& w = out.w;
    const recipe_registry& reg = out.reg;

    // ---- M (before) -------------------------------------------------------
    const std::size_t laid = w.rail_track.size();
    std::printf("--- M the haulage shift (rail.cost_per_step %.4f, station_handling %.3f) ---\n",
                static_cast<double>(reg.rail().cost_per_step), static_cast<double>(reg.rail().station_handling));
    const shift_reading before = read_shift(w, reg);
    print_shift("BEFORE (no stations)", before);

    const rail_handover_report rh = hand_over_rail(w);
    print_rail_handover(rh);
    {
        // The network as the router reads it: stations, joined pairs, components,
        // and how many market centres ARE stations (a haul boards nowhere else).
        const world::rail_network net = body_rail_network(w, w.home_body);
        std::size_t joined = 0;
        std::vector<int> comp(net.stations.size(), -1);
        int ncomp = 0;
        for (std::size_t i = 0; i < net.stations.size(); ++i)
        {
            for (std::size_t j = 0; j < net.stations.size(); ++j)
                if (i != j && net.steps[i][j] >= 0) ++joined;
            if (comp[i] >= 0) continue;
            comp[i] = ncomp;
            for (std::size_t j = 0; j < net.stations.size(); ++j)
                if (j != i && net.steps[i][j] >= 0) comp[j] = ncomp;
            ++ncomp;
        }
        int centre_st = 0, mk = 0;
        for (const entity_id m : sorted_markets_on(w, w.home_body))
        {
            ++mk;
            if (std::binary_search(net.stations.begin(), net.stations.end(), w.markets.at(m).centre_tile)) ++centre_st;
        }
        for (const entity_id m : sorted_markets_on(w, w.home_body))
        {
            if (g_quiet) break;
            const entity_id ct = w.markets.at(m).centre_tile;
            const tile_component& tc = w.tiles.at(ct);
            int best = 9999;
            for (const auto& [t, o] : w.rail_track)
            {
                const tile_component& tt = w.tiles.at(t);
                if (tt.body != tc.body) continue;
                int dx = std::abs(tt.grid_x - tc.grid_x);
                dx = std::min(dx, w.bodies.at(tc.body).grid_width - dx);
                best = std::min(best, std::max(dx, std::abs(tt.grid_y - tc.grid_y)));
            }
            const auto tn = w.tile_to_nation.find(ct);
            const entity_id nat = tn != w.tile_to_nation.end() ? tn->second : null_entity;
            std::printf("      market %u centre (%d,%d) city %d nation %u major %d: nearest track %d cells\n",
                        static_cast<unsigned>(m), tc.grid_x, tc.grid_y, is_city(w, ct) ? 1 : 0,
                        static_cast<unsigned>(nat), nat != null_entity && is_major_nation(w, nat) ? 1 : 0, best);
        }
        std::printf("    network: %zu stations, %zu joined ordered pairs, %d lines (components); "
                    "%d of %d market centres are stations\n",
                    net.stations.size(), joined, ncomp, centre_st, mk);
    }
    const shift_reading after = read_shift(w, reg);
    print_shift("AFTER", after);
    std::printf("    SHIFT seed %u: track %d tiles, stations %d; long hauls on rail %.1f%% -> %.1f%%, "
                "short %.1f%% -> %.1f%%\n",
                static_cast<unsigned>(seed), rh.track_tiles, rh.stations,
                before.long_pairs ? 100.0 * before.long_rail / before.long_pairs : 0.0,
                after.long_pairs ? 100.0 * after.long_rail / after.long_pairs : 0.0,
                before.short_pairs ? 100.0 * before.short_rail / before.short_pairs : 0.0,
                after.short_pairs ? 100.0 * after.short_rail / after.short_pairs : 0.0);
    check(before.long_rail == 0 && before.short_rail == 0, "M",
          "with no stations no haul rides rail (the BEFORE world prices as roads alone)");
    if (g_sweep)
    {
        for (const float cps : {0.002f, 0.0025f, 0.003f, 0.0035f})
            for (const float sh : {0.03f, 0.04f, 0.05f})
            {
                recipe_registry r2 = reg;
                recipe_registry::rail_params rp = reg.rail();
                rp.cost_per_step = cps;
                rp.station_handling = sh;
                r2.set_rail(rp);
                const shift_reading s = read_shift(w, r2);
                print_shift(fmt("SWEEP %.4f / %.2f", static_cast<double>(cps), static_cast<double>(sh)).c_str(), s);
            }
    }
    if (g_time_ticks > 0)
    {
        // THE COST OF THE RAIL SEARCH: the same live ticks on a copy without the
        // handover's stations (no network) and on the world with them.
        const auto run = [&](world c, const char* label) {
            const auto t0 = std::chrono::steady_clock::now();
            for (int k = 0; k < g_time_ticks; ++k)
                (void)run_settle_tick(c, reg, k_campaign_settle_ticks + k, 90 * (k + 1), /*spectating=*/true); // live ticks (BL-1452: below 12 is the settle)
            const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            std::printf("    TIME %-18s %d ticks %.2f s\n", label, g_time_ticks, s);
        };
        world bare = w;
        for (auto it = bare.buildings.begin(); it != bare.buildings.end();)
        {
            if (it->second.type == building_type::railway_station)
            {
                for (auto& [nid, nc] : bare.nations)
                    nc.state_assets.erase(std::remove(nc.state_assets.begin(), nc.state_assets.end(), it->first), nc.state_assets.end());
                for (auto& [cid, cc] : bare.nations)
                    cc.state_assets.erase(std::remove(cc.state_assets.begin(), cc.state_assets.end(), it->first), cc.state_assets.end());
                bare.stockpiles.erase(it->first);
                it = bare.buildings.erase(it);
            }
            else
                ++it;
        }
        invalidate_logistics_caches(bare);
        run(bare, "no stations");
        run(w, "with stations");
    }
    if (g_measure_only || !mechanics)
        return;

    // ---- R1 generation ------------------------------------------------------
    std::printf("--- R1 generation ---\n");
    (void)laid;
    check(rh.track_tiles > 0, "R1", fmt("the fold laid the span's links as track (%d tiles, %d links between market cities, %d broken at water)",
                                       rh.track_tiles, rh.links_laid, rh.links_broken));
    check(rh.unowned == 0, "R1/B", fmt("no track is ownerless (%d of %d tiles owned along the line)", rh.along_the_line, rh.track_tiles));
    check(rh.stations > 0, "R1", fmt("stations stood at the cities a line serves (%d)", rh.stations));
    {
        bool own_ok = true;
        std::string why;
        for (const auto& [tile, owner] : w.rail_track)
        {
            if (owner == null_entity) { own_ok = false; why = "an ownerless tile"; continue; }
            const auto tn = w.tile_to_nation.find(tile);
            const entity_id ground = tn != w.tile_to_nation.end() ? tn->second : null_entity;
            // BL-1407: every owner is a NATION. A major's ground carries only its
            // own track; a minor's line runs wherever its retired background firm laid it.
            if (w.nations.count(owner) == 0) { own_ok = false; why = "an owner that is no nation"; continue; }
            if (ground != null_entity && is_major_nation(w, ground) && ground != owner)
            { own_ok = false; why = "a major's ground carrying another's track"; }
        }
        check(own_ok, "R1", "every track tile is owned by a nation, a major's ground by that major"
                            + (own_ok ? std::string() : " -- " + why));
        for (const auto& [o, n] : rh.per_owner_tiles)
            std::printf("    owner %u (%s): %d track tiles\n", static_cast<unsigned>(o),
                        is_major_nation(w, o) ? "major" : "minor", n);
        bool st_ok = true;
        int n = 0;
        for (const auto& [bid, b] : w.buildings)
        {
            if (b.type != building_type::railway_station) continue;
            ++n;
            const building_owner own = building_owner_of(w, bid);
            if (!is_city(w, b.tile) || !tile_touches_track(w, b.tile) || own.kind == owner_kind::none
                || b.ticks_remaining != 0 || b.workforce_assigned != 0.0f)
                st_ok = false;
        }
        check(st_ok && n == rh.stations, "R1",
              fmt("every station is on a city tile touching track, owned, complete, zero-staffed (%d)", n));
    }

    // ---- R2 boarding only at stations -------------------------------------
    std::printf("--- R2 a haul boards only at stations ---\n");
    entity_id cut_a = null_entity, cut_b = null_entity, cut_tile = null_entity;
    entity_id pair_src = null_entity, pair_dst = null_entity;
    {
        const logistics_nodes nodes = collect_logistics_nodes(w);
        const world::rail_network net = body_rail_network(w, w.home_body);
        const std::set<entity_id> stations(net.stations.begin(), net.stations.end());
        const std::vector<entity_id> mids = sorted_markets_on(w, w.home_body);
        int riding = 0;
        bool ok = true;
        for (const entity_id a : mids)
            for (const entity_id b : mids)
            {
                if (a == b) continue;
                const convoy_leg leg = price_market_leg(w, reg, nodes, a, b, 1.0f);
                if (!leg.viable || leg.station_a == null_entity) continue;
                ++riding;
                if (!stations.count(leg.station_a) || !stations.count(leg.station_b) || leg.station_a == leg.station_b)
                    ok = false;
                const logistics_path rp = rail_leg_path(w, w.home_body, leg.station_a, leg.station_b);
                if (!rp.reachable || rp.tiles.front() != leg.station_a || rp.tiles.back() != leg.station_b)
                    ok = false;
                for (std::size_t i = 1; i + 1 < rp.tiles.size(); ++i)
                    if (!tile_has_track(w, rp.tiles[i])) ok = false;
                // The convoy lane passes both stations.
                convoy_component cv;
                cv.source_market = a; cv.dest_market = b; cv.origin_tile = w.markets.at(a).centre_tile;
                cv.station_a = leg.station_a; cv.station_b = leg.station_b;
                const convoy_route lane = convoy_route_tiles(w, cv);
                const auto ia = std::find(lane.tiles.begin(), lane.tiles.end(), leg.station_a);
                if (ia == lane.tiles.end()
                    || std::find(lane.tiles.begin(), lane.tiles.end(), leg.station_b) == lane.tiles.end())
                    ok = false;
                // F (one price): the lane's rail stretch IS the path the price read.
                else if (static_cast<std::size_t>(lane.tiles.end() - ia) < rp.tiles.size()
                         || !std::equal(rp.tiles.begin(), rp.tiles.end(), ia))
                    ok = false;
                if (cut_tile == null_entity && rp.tiles.size() >= 5)
                {
                    cut_a = leg.station_a; cut_b = leg.station_b;
                    cut_tile = rp.tiles[rp.tiles.size() / 2];
                    pair_src = a; pair_dst = b;
                }
            }
        check(riding > 0, "R2", fmt("some market pairs ride the rail (%d)", riding));
        check(ok, "R2", "every rail route boards and alights at two network stations over unbroken track, "
                        "and its convoy lane rides the very rail path the price read (F)");
    }

    // ---- R3 a cut strands ---------------------------------------------------
    std::printf("--- R3 a cut line strands stations ---\n");
    if (cut_tile != null_entity)
    {
        world c = w;
        const entity_id op = rail_operator(c, c.rail_track.at(cut_tile));
        entity_id raider = null_entity;
        {
            std::vector<entity_id> ids;
            for (const auto& [cid, cc] : c.nations) ids.push_back(cid);
            std::sort(ids.begin(), ids.end());
            for (const entity_id cid : ids) if (cid != op) { raider = cid; break; }
        }
        const world::rail_network n0 = body_rail_network(c, c.home_body);
        const auto idx = [&](const world::rail_network& n, entity_id t) {
            return static_cast<std::size_t>(std::find(n.stations.begin(), n.stations.end(), t) - n.stations.begin());
        };
        const int steps0 = n0.steps[idx(n0, cut_a)][idx(n0, cut_b)];
        declare_hostile(c, raider, op);
        const entity_id uid = c.create_entity();
        unit_component u{};
        u.position = cut_tile; u.owner = raider; u.count = 1;
        c.units[uid] = u;
        const world::rail_network n1 = body_rail_network(c, c.home_body);
        const int steps1 = n1.steps[idx(n1, cut_a)][idx(n1, cut_b)];
        check(steps1 < 0 || steps1 > steps0, "R3",
              fmt("a hostile unit on the line cuts it: steps %d -> %d between the two stations", steps0, steps1));
        const logistics_nodes nodes = collect_logistics_nodes(c);
        const convoy_leg leg = price_market_leg(c, reg, nodes, pair_src, pair_dst, 1.0f);
        bool avoids = true;
        if (leg.viable && leg.station_a != null_entity)
        {
            const logistics_path rp = rail_leg_path(c, c.home_body, leg.station_a, leg.station_b);
            avoids = std::find(rp.tiles.begin(), rp.tiles.end(), cut_tile) == rp.tiles.end();
        }
        check(avoids, "R3", "the pair is no longer priced over the cut tile");
        c.units.erase(uid);
        const world::rail_network n2 = body_rail_network(c, c.home_body);
        check(n2.steps[idx(n2, cut_a)][idx(n2, cut_b)] == steps0, "R3", "the unit leaves: the line is whole again");
    }
    else
        check(false, "R3", "no rail route long enough to cut was found");

    // ---- R4 the player's verbs ---------------------------------------------
    std::printf("--- R4 the player's verbs ---\n");
    (void)seat_player_corporation(w, seed, out.land.search.winner_score);
    const entity_id P  = w.player_entity;
    const entity_id pn = player_nation(w);
    std::printf("    seat: nation %u (player nation %u, major %d)\n", static_cast<unsigned>(P),
                static_cast<unsigned>(pn), is_major_nation(w, pn) ? 1 : 0);
    // BL-1407: the player IS its nation (no champion corporation exists).
    if (pn != null_entity && pn == P && is_major_nation(w, pn))
    {
        agent_protocol::session ps;
        ps.actor = P;
        const unsigned LT = static_cast<unsigned>(corp_verb::lay_track);
        const auto P_ = [&](const std::string& rest) { return fmt("COMMAND corp=%u ", static_cast<unsigned>(P)) + rest; };
        // A city of the player's nation with an own-ground land neighbour carrying no track.
        entity_id city = null_entity, next = null_entity;
        std::vector<entity_id> cts;
        for (const auto& [cid, t] : w.population_centre_tile) cts.push_back(t);
        std::sort(cts.begin(), cts.end());
        for (const entity_id t : cts)
        {
            const auto tn = w.tile_to_nation.find(t);
            if (tn == w.tile_to_nation.end() || tn->second != pn || tile_has_track(w, t)) continue;
            for (const entity_id nb : neighbours_of(w, t))
            {
                const auto nn = w.tile_to_nation.find(nb);
                if (nn != w.tile_to_nation.end() && nn->second == pn && !tile_has_track(w, nb)
                    && !is_water(w.tiles.at(nb).substrate) && !is_city(w, nb))
                { city = t; next = nb; break; }
            }
            if (city != null_entity) break;
        }
        entity_id water = null_entity, foreign = null_entity, lone = null_entity, other_n = null_entity;
        {
            std::vector<entity_id> tids;
            for (const auto& [tid, tc] : w.tiles) if (tc.body == w.home_body) tids.push_back(tid);
            std::sort(tids.begin(), tids.end());
            for (const entity_id t : tids)
            {
                const auto tn = w.tile_to_nation.find(t);
                if (water == null_entity && is_water(w.tiles.at(t).substrate)) water = t;
                if (foreign == null_entity && tn != w.tile_to_nation.end() && tn->second != pn
                    && !is_water(w.tiles.at(t).substrate) && is_city(w, t))
                { foreign = t; other_n = tn->second; }
                if (lone == null_entity && tn != w.tile_to_nation.end() && tn->second == pn
                    && !is_water(w.tiles.at(t).substrate) && !is_city(w, t) && !tile_touches_track(w, t))
                    lone = t;
            }
        }
        check(city != null_entity && next != null_entity, "R4", "a city of the player's nation to open a line at");
        struct rej { std::string line; const char* want; const char* what; };
        std::vector<rej> rows;
        if (water != null_entity)   rows.push_back({P_(fmt("verb=%u tile=%u nation=%u", LT, water, pn)), "rejected_placement", "track on water"});
        if (foreign != null_entity) rows.push_back({P_(fmt("verb=%u tile=%u nation=%u", LT, foreign, pn)), "rejected_placement", "track on another nation's ground"});
        if (lone != null_entity)    rows.push_back({P_(fmt("verb=%u tile=%u nation=%u", LT, lone, pn)), "rejected_placement", "track neither at a city nor extending a line"});
        if (other_n != null_entity) rows.push_back({P_(fmt("verb=%u tile=%u nation=%u", LT, foreign, other_n)), "rejected_not_owner", "track for another nation (actor gate)"});
        if (city != null_entity)
        {
            rows.push_back({P_(fmt("verb=%u tile=%u", LT, city)), "rejected_invalid", "track naming no nation"});
            rows.push_back({P_(fmt("verb=%u tile=%u nation=abc", LT, city)), "rejected_invalid", "a malformed nation id"});
            rows.push_back({P_(fmt("verb=%u tile=%llu nation=%u", LT, 1ull << 40, pn)), "rejected_invalid", "a tile id past the entity domain"});
            rows.push_back({P_(fmt("verb=0 tile=%u type=11 nation=%u", city, pn)), "rejected_placement", "a station on a city touching no track"});
            // BL-1309: "anyone may build" a station, so a corporation's book is a
            // legal door now; off the line the build door still refuses it.
            rows.push_back({P_(fmt("verb=0 tile=%u type=11", city)), "rejected_placement", "a station naming no nation, on a city touching no track"});
            rows.push_back({P_(fmt("verb=0 tile=%u type=12 nation=%u", city, pn)), "rejected_invalid", "a building type past the roster"});
        }
        rows.push_back({P_(fmt("verb=%u tile=0 nation=%u", LT, pn)), "rejected_invalid", "track on no tile"});
        rows.push_back({P_(fmt("verb=%u tile=1 nation=%u", static_cast<unsigned>(corp_verb_count), pn)), "rejected_invalid", "a verb past the dictionary"});
        bool all_rej = true;
        for (const rej& r : rows)
        {
            const std::uint64_t h0 = w.state_hash(0);
            const float t0 = w.nations.at(pn).treasury;
            const std::size_t tr0 = w.rail_track.size(), b0 = w.buildings.size();
            const std::string o = serve(w, reg, ps, r.line);
            const bool ok = result_is(o, r.want) && w.state_hash(0) == h0 && w.nations.at(pn).treasury == t0
                         && w.rail_track.size() == tr0 && w.buildings.size() == b0;
            if (!ok) { all_rej = false; std::printf("    REJECT ROW FAILED: %s -> %s", r.what, o.c_str()); }
        }
        check(all_rej, "R4", fmt("%zu malformed / unauthorised / illegal rail commands rejected, nothing mutated", rows.size()));

        if (city != null_entity)
        {
            w.nations.at(pn).treasury = 2.0e4f; // fund the fixture (small: float resolution at 1e7 is a whole credit)
            // Track buys real steel off the tile market's shelf (the fix round): stock it.
            for (const entity_id t : {city, next})
                if (const entity_id mid = market_for_tile(w, t); mid != null_entity)
                    w.markets.at(mid).inventory[static_cast<std::size_t>(resource_type::steel)] += 50.0f;
            const float cost1 = track_tile_cost(w, reg, city);
            const float t0 = state_uncommitted_treasury(w, pn), tr0 = w.nations.at(pn).treasury;
            const std::string o1 = serve(w, reg, ps, P_(fmt("verb=%u tile=%u nation=%u", LT, city, pn)));
            const float tr1 = w.nations.at(pn).treasury;
            check(result_is(o1, "applied") && w.rail_track.count(city) && w.rail_track.at(city) == pn
                      && std::fabs((tr0 - tr1) - cost1) <= 1e-3f * std::max(1.0f, cost1),
                  "R4", fmt("lay_track at a city applies, owned by the nation, charging %.2f (paid %.2f; uncommitted %.1f)",
                            static_cast<double>(cost1), static_cast<double>(tr0 - tr1), static_cast<double>(t0)));
            const float cost2 = track_tile_cost(w, reg, next);
            const std::string o2 = serve(w, reg, ps, P_(fmt("verb=%u tile=%u nation=%u", LT, next, pn)));
            const float tr2 = w.nations.at(pn).treasury;
            check(result_is(o2, "applied") && w.rail_track.count(next)
                      && std::fabs((tr1 - tr2) - cost2) <= 1e-3f * std::max(1.0f, cost2),
                  "R4", "lay_track extending the nation's own line applies at its price");
            {
                const std::uint64_t h0 = w.state_hash(0);
                const std::string o3 = serve(w, reg, ps, P_(fmt("verb=%u tile=%u nation=%u", LT, next, pn)));
                check(result_is(o3, "rejected_placement") && w.state_hash(0) == h0, "R4", "a second tile of track on the same tile is refused");
            }
            const std::size_t sa0 = w.nations.at(pn).state_assets.size();
            const std::string o4 = serve(w, reg, ps, P_(fmt("verb=0 tile=%u type=11 nation=%u", city, pn)));
            bool st = result_is(o4, "applied") && w.nations.at(pn).state_assets.size() == sa0 + 1;
            if (st)
            {
                const building_component& b = w.buildings.at(w.nations.at(pn).state_assets.back());
                st = b.type == building_type::railway_station && b.tile == city;
            }
            check(st, "R4", "a station on the player's new line applies and joins the nation's state industry");
            {
                const std::uint64_t h0 = w.state_hash(0);
                const std::string o5 = serve(w, reg, ps, P_(fmt("verb=0 tile=%u type=11 nation=%u", city, pn)));
                check(result_is(o5, "rejected_placement") && w.state_hash(0) == h0, "R4", "a second station on the tile is refused");
            }
            {
                world c = w;
                c.nations.at(pn).treasury = 0.0f;
                entity_id nn = null_entity;
                for (const entity_id t : neighbours_of(c, next))
                {
                    const auto tn = c.tile_to_nation.find(t);
                    if (tn != c.tile_to_nation.end() && tn->second == pn && !tile_has_track(c, t) && !is_water(c.tiles.at(t).substrate))
                    { nn = t; break; }
                }
                if (nn != null_entity)
                {
                    const std::uint64_t h0 = c.state_hash(0);
                    const std::string o6 = serve(c, reg, ps, P_(fmt("verb=%u tile=%u nation=%u", LT, nn, pn)));
                    check(result_is(o6, "rejected_funds") && c.state_hash(0) == h0, "R4", "an empty treasury lays no track (rejected_funds, nothing mutated)");
                }
            }
        }
        // A rival nation's lay_track, in-process (the scorer's door): refused.
        // BL-1407: the rival is the major nation itself (no champion exists).
        {
            entity_id rn = null_entity, rc = null_entity;
            std::vector<entity_id> nids;
            for (const auto& [nid, nc] : w.nations) nids.push_back(nid);
            std::sort(nids.begin(), nids.end());
            for (const entity_id nid : nids)
                if (nid != pn && is_major_nation(w, nid))
                { rn = nid; rc = nid; break; }
            if (rn != null_entity)
            {
                entity_id rt = null_entity;
                for (const entity_id t : cts)
                {
                    const auto tn = w.tile_to_nation.find(t);
                    if (tn != w.tile_to_nation.end() && tn->second == rn && !tile_has_track(w, t)) { rt = t; break; }
                }
                corp_command cmd;
                cmd.corp = rc; cmd.as_nation = rn; cmd.verb = corp_verb::lay_track; cmd.tile = rt;
                const std::uint64_t h0 = w.state_hash(0);
                const corp_command_result r1 = apply_corp_command(w, reg, cmd);
                corp_command sc;
                sc.corp = rc; sc.as_nation = rn; sc.verb = corp_verb::build; sc.type = building_type::railway_station; sc.tile = rt;
                const corp_command_result r2 = apply_corp_command(w, reg, sc);
                const bool unmoved = w.state_hash(0) == h0;
                // BL-1407: the retired third row (a corporation's own-book station)
                // has no subject — every builder is a nation, asked above.
                std::printf("    rival nation %u: lay_track -> %d, station -> %d\n", static_cast<unsigned>(rn),
                            static_cast<int>(r1), static_cast<int>(r2));
                check(r1 == corp_command_result::rejected_not_owner && r2 == corp_command_result::rejected_not_owner
                          && unmoved,
                      "R4", "a rival nation may not lay track or build a state station through the command seam");
            }
        }
    }
    else
        check(false, "R4", "the seat is its major nation");

    // ---- R5 upkeep ------------------------------------------------------------
    std::printf("--- R5 upkeep ---\n");
    {
        world c = w;
        // Isolate the track's bill: run apply_budget with no flows and compare one
        // owner nation's ledger maintenance against its buildings' plus the track's.
        std::map<entity_id, int> tiles_of;
        for (const auto& [t, o] : c.rail_track) if (o != null_entity) ++tiles_of[o];
        entity_id nat = null_entity;
        for (const auto& [o, n] : tiles_of) if (c.nations.count(o)) { nat = o; break; }
        if (nat != null_entity)
        {
            // Strip its state buildings so the ledger's maintenance is the track's alone.
            std::vector<entity_id> keep = c.nations.at(nat).state_assets;
            c.nations.at(nat).state_assets.clear();
            for (const entity_id b : keep) c.buildings.erase(b);
            c.nations.at(nat).treasury = 1.0e6f;
            const float tr0 = c.nations.at(nat).treasury;
            std::unordered_map<entity_id, corp_cash_flow> flows;
            std::map<std::pair<entity_id, entity_id>, float> contention;
            apply_budget(c, reg, flows, contention);
            const float want = reg.rail().track_upkeep_per_tile * static_cast<float>(tiles_of.at(nat));
            const float paid = tr0 - c.nations.at(nat).treasury;
            check(std::fabs(paid - want) <= 1e-2f && std::fabs(c.state_ledger.at(nat).maintenance - want) <= 1e-2f,
                  "R5", fmt("a nation's track bills %d tiles x %.3f = %.2f (paid %.2f, ledger %.2f)", tiles_of.at(nat),
                            static_cast<double>(reg.rail().track_upkeep_per_tile), static_cast<double>(want),
                            static_cast<double>(paid), static_cast<double>(c.state_ledger.at(nat).maintenance)));
            c.nations.at(nat).treasury = 0.5f;
            apply_budget(c, reg, flows, contention);
            check(c.nations.at(nat).treasury >= 0.0f, "R5", "a treasury short of the bill pays what it holds, never below zero");
        }
        else
            check(false, "R5", "a nation owns track to bill");
    }

    // ---- R7 the fix round: steel, idle track and stations, territory --------
    std::printf("--- R7 real steel, unpaid track idles, idle stations, ownership follows territory ---\n");
    {
        const entity_id pn7 = player_nation(w);
        // C: a tile extending the player's own line (R4 laid one).
        entity_id ext = null_entity;
        for (const auto& [t, o] : w.rail_track)
        {
            if (o != pn7 || ext != null_entity) continue;
            for (const entity_id nb : neighbours_of(w, t))
            {
                const auto tn = w.tile_to_nation.find(nb);
                if (tn != w.tile_to_nation.end() && tn->second == pn7 && !tile_has_track(w, nb)
                    && !is_water(w.tiles.at(nb).substrate) && market_for_tile(w, nb) != null_entity)
                { ext = nb; break; }
            }
        }
        if (ext != null_entity)
        {
            const std::size_t si = static_cast<std::size_t>(resource_type::steel);
            const float qty = reg.rail().track_steel_per_tile;
            {
                world c = w;
                c.nations.at(pn7).treasury = 2.0e4f;
                market_component& m = c.markets.at(market_for_tile(c, ext));
                m.inventory[si] = qty + 3.0f;
                const float cost = track_tile_cost(c, reg, ext);
                const float tr0 = c.nations.at(pn7).treasury;
                const construction_result r = lay_track(c, reg, pn7, ext);
                check(r == construction_result::placed && std::fabs(c.markets.at(market_for_tile(c, ext)).inventory[si] - 3.0f) < 1e-4f
                          && std::fabs((tr0 - c.nations.at(pn7).treasury) - cost) < 1e-3f,
                      "R7/C", fmt("laying track buys its %.1f steel off the shelf (stock drawn) and the treasury pays %.2f", static_cast<double>(qty), static_cast<double>(cost)));
            }
            {
                world c = w;
                c.nations.at(pn7).treasury = 2.0e4f;
                c.markets.at(market_for_tile(c, ext)).inventory[si] = qty * 0.5f;
                const std::uint64_t h0 = c.state_hash(0);
                const construction_result r = lay_track(c, reg, pn7, ext);
                check(r == construction_result::insufficient_funds && c.state_hash(0) == h0,
                      "R7/C", "a shelf short of the steel lays no track (refused, nothing mutated)");
            }
        }
        else
            check(false, "R7/C", "a tile extending the player's line to test the steel purchase");

        // D: an owner whose track upkeep goes unfunded idles its track. The pair:
        // two joined stations whose rail path crosses track a NATION owns.
        if (cut_a != null_entity)
        {
            entity_id sa = null_entity, sb = null_entity, nat = null_entity, via = null_entity;
            {
                const world::rail_network n0 = body_rail_network(w, w.home_body);
                for (std::size_t i = 0; i < n0.stations.size() && nat == null_entity; ++i)
                    for (std::size_t j = 0; j < n0.stations.size() && nat == null_entity; ++j)
                    {
                        if (i == j || n0.steps[i][j] < 2) continue;
                        const logistics_path p = rail_leg_path(w, w.home_body, n0.stations[i], n0.stations[j]);
                        for (std::size_t k = 1; k + 1 < p.tiles.size(); ++k)
                        {
                            const auto rt = w.rail_track.find(p.tiles[k]);
                            if (rt != w.rail_track.end() && w.nations.count(rt->second))
                            { sa = n0.stations[i]; sb = n0.stations[j]; nat = rt->second; via = p.tiles[k]; break; }
                        }
                    }
            }
            if (nat != null_entity)
            {
                world c = w;
                c.nations.at(nat).treasury = 0.0f;
                plan_state_funding(c, reg);
                const bool idle = c.rail_idle.count(nat) != 0;
                const logistics_path live = rail_leg_path(c, c.home_body, sa, sb);
                const logistics_path phys = rail_leg_path(c, c.home_body, sa, sb, false);
                bool avoids = true;
                for (const entity_id t : live.tiles)
                    if (const auto rt = c.rail_track.find(t); rt != c.rail_track.end() && rt->second == nat) avoids = false;
                std::unordered_map<entity_id, corp_cash_flow> flows;
                std::map<std::pair<entity_id, entity_id>, float> contention;
                apply_budget(c, reg, flows, contention);
                check(idle && avoids && phys.reachable && c.nations.at(nat).treasury == 0.0f, "R7/D",
                      fmt("a nation (%u) with nothing to pay its track upkeep: its track idles (no rail route over it, tile %u), and pays nothing",
                          static_cast<unsigned>(nat), static_cast<unsigned>(via)));
                c.nations.at(nat).treasury = 1.0e6f;
                plan_state_funding(c, reg);
                check(c.rail_idle.count(nat) == 0 && rail_leg_path(c, c.home_body, sa, sb).reachable, "R7/D",
                      "funded again, the track runs again");
            }
            else
                check(false, "R7/D", "a rail path over state-owned track to idle");

            // E: an idle station boards nothing.
            world e = w;
            for (auto& [bid, b] : e.buildings)
                if (b.type == building_type::railway_station && b.tile == cut_a)
                    b.state_idle = true;
            e.rail_networks.clear();
            const world::rail_network ne = body_rail_network(e, e.home_body);
            check(std::find(ne.stations.begin(), ne.stations.end(), cut_a) == ne.stations.end(), "R7/E",
                  "an unfunded (state_idle) station leaves the network");
        }

        // G: ownership follows territory.
        {
            world c = w;
            entity_id tile = null_entity, x = null_entity, y = null_entity;
            for (const auto& [t, o] : c.rail_track)
                if (c.nations.count(o)) { tile = t; x = o; break; }
            std::vector<entity_id> nids;
            for (const auto& [nid, nc] : c.nations) nids.push_back(nid);
            std::sort(nids.begin(), nids.end());
            for (const entity_id nid : nids)
                if (nid != x && is_major_nation(c, nid)) { y = nid; break; }
            if (tile != null_entity && y != null_entity)
            {
                c.tile_to_nation[tile] = y;
                const int moved = assign_rail_owners(c);
                check(c.rail_track.at(tile) == y && moved >= 1, "R7/G",
                      "track on ground that changes hands passes to the new holder's state");
            }
            else
                check(false, "R7/G", "a state-owned tile and a second major to test territory");
        }
    }

    // ---- R6 the save -----------------------------------------------------------
    std::printf("--- R6 the save ---\n");
    {
        world c = w;
        convoy_component cv;
        cv.id = c.allocate_convoy_id();
        if (cut_a != null_entity && pair_src != null_entity)
        {
            cv.source_market = pair_src; cv.dest_market = pair_dst; cv.corp = P;
            cv.origin_tile = c.markets.at(pair_src).centre_tile;
            cv.station_a = cut_a; cv.station_b = cut_b; cv.cargo_qty = 1.0f; cv.speed = 0.5f;
            c.convoys.push_back(cv);
        }
        const std::string b = to_bytes(c);
        world back;
        const bool ok = from_bytes(b, back);
        bool same = ok && back.rail_track == c.rail_track && back.state_hash(0) == c.state_hash(0);
        if (same && !c.convoys.empty())
            same = back.convoys.back().station_a == cut_a && back.convoys.back().station_b == cut_b;
        check(same, "R6", fmt("v49 round-trips the track (%zu tiles) and a riding convoy's stations, equal state_hash",
                              c.rail_track.size()));
        check(to_bytes(back) == b, "R6", "re-serialising reproduces the stream byte for byte");
        // A v48 stream: a v55 stream (BL-1407: the verification writer, so the
        // corporation store a pre-v56 stream carries is present — empty) less the
        // rail section and everything after it, stamped 48.
        std::string b55;
        {
            std::ostringstream o55(std::ios::binary);
            write_world_snapshot_legacy(c, {}, o55, k_world_save_version_pre_books);
            b55 = o55.str();
        }
        std::size_t tail = 2 * sizeof(uint32_t) + c.rail_track.size() * 2 * sizeof(entity_id);
        for (const convoy_component& k : c.convoys)
            if (k.station_a != null_entity) tail += sizeof(uint32_t) + 2 * sizeof(entity_id);
        tail += sizeof(uint32_t) + c.rail_idle.size() * sizeof(entity_id);
        // The sections written AFTER this one: v50's share rules (count + each set
        // rule's id and three floats) and v51's space assets (count + 35 bytes each:
        // id, owner, class, body, slot, state, launch, arrival, upkeep, unpaid, cost).
        tail += sizeof(uint32_t);
        for (const auto& [lnid, lnc] : c.nations)
            if (lnc.lp_shares.set) tail += sizeof(entity_id) + 3 * sizeof(float);
        tail += sizeof(uint32_t) + c.space_assets.size() * 35u;
        // BL-1335 (v52): the tax section after them (count + 9 bytes a nation
        // whose tier or Discontent moved off the default).
        tail += world_save_tax_section_bytes(c);
        tail += world_save_net_section_bytes(c); // BL-1337 (v54): the exact nets
        tail += world_save_major_section_bytes(c); // BL-1404 (v55): the majors, last
        std::string v48 = b55.substr(0, b55.size() - tail);
        const uint32_t ver = k_world_save_version_pre_rail;
        std::memcpy(&v48[4], &ver, sizeof ver);
        world old;
        check(from_bytes(v48, old) && old.rail_track.empty(), "R6", "a v48 stream reads, as a world with no rail");
        world tampered;
        std::string bad = b55; // BL-1407: offsets in the v55 layout above
        if (!c.rail_track.empty())
        {
            // The first track entry's owner set to an id no nation or corporation holds.
            const std::size_t off = b55.size() - tail + sizeof(uint32_t) + sizeof(entity_id);
            const entity_id junk = 0x7FFFFFF0u;
            std::memcpy(&bad[off], &junk, sizeof junk);
            check(!from_bytes(bad, tampered), "R6", "a track owner naming no actor is refused whole");
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--sweep") == 0) { g_sweep = true; continue; }
        if (std::strcmp(argv[i], "--measure-only") == 0) { g_measure_only = true; continue; }
        if (std::strcmp(argv[i], "--long") == 0 && i + 1 < argc) { g_long = static_cast<float>(std::atof(argv[++i])); continue; }
        if (std::strcmp(argv[i], "--serve") == 0 && i + 1 < argc) { g_serve = static_cast<float>(std::atof(argv[++i])); continue; }
        if (std::strcmp(argv[i], "--quiet") == 0) { g_quiet = true; continue; }
        if (std::strcmp(argv[i], "--time-ticks") == 0 && i + 1 < argc) { g_time_ticks = std::atoi(argv[++i]); continue; }
        seeds.push_back(static_cast<std::uint32_t>(std::strtoul(argv[i], nullptr, 10)));
    }
    if (seeds.empty())
        seeds.push_back(2); // a world whose lines carry market traffic (after the fix round)

    lua_state lua;
    for (std::size_t i = 0; i < seeds.size(); ++i)
        run_seed(lua, seeds[i], /*mechanics=*/i == 0);

    std::printf("\nrailway_harness: %d pass, %d fail -- %s\n", g_pass, g_fail, g_fail == 0 ? "ALL PASS" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}
