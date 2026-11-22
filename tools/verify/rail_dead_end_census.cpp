// ---------------------------------------------------------------------------
// rail_dead_end_census — BL-1731 (rail ends without station), a REPORT
// ---------------------------------------------------------------------------
// docs/economy/LOGISTICS.md § 4c, INDUSTRIALISATION.md § Who lays rail. The second
// playtest saw railways ending with no railway station at the end. This counts them
// on the SHIPPED start (harness_params.hpp's app mirror) and attributes each one.
//
// The world is built as build_app_start_world builds it, with ONE change of shape and
// none of substance: the finish's tail from the rail handover on (harness_params.hpp's
// apply_shipped_landscape, the `!skip_rail_handover` block, itself finish_campaign_
// world's sequence) is run HERE, call for call, so the station set can be read between
// the stages. `--check-path` builds the seed both ways and compares state_hash, so the
// copy is proven to be the shipped sequence rather than asserted to be.
//
// A line END is a track tile with at most one track neighbour on the walk the rail
// network uses (four cardinal steps, columns wrapping: logistics.hpp § Railways). An
// end is SERVED when a railway station stands on it or on a cardinal neighbour (the
// station's placement rule, "a city tile touching a rail line", and the network's
// "stations touching track"). Read per stage, then after the validation settle + seat
// and a live window of `--ticks` economy ticks (default 60).
//
// Every unserved end at the start is attributed to ONE cause:
//   taken_up:<stage>   a station served it after the rail handover and the stage named
//                      removed it (the track stays)
//   cap_refused        a city tile touches the end but no station stood after the
//                      handover (hand_over_rail's stack-cap refusal)
//   water              no city touches the end and a cardinal neighbour is water (the
//                      handover's path crossed water it does not lay, or a strait)
//   centre_not_city    no city touches it; the end is a market centre tile (the city
//                      snap's fallback when no city stood within 4 cells)
//   other              none of the above
//
// Build: bash tools/verify/build_lua_harness.sh rail_dead_end_census
// Run:   build_gen/verify/rail_dead_end_census.exe [--ticks N] [--check-path] [--ends] [seed ...]
//        (default: the 16 seeds of docs/generation/seed_library.json)
// REPORT-only: exits non-zero only when --check-path finds the copy diverged.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/finish_campaign_world.hpp"
#include "world/logistic_points.hpp"
#include "world/logistics.hpp"
#include "world/opening_kit.hpp"
#include "world/sea_combat.hpp"
#include "world/spawn_seat.hpp"
#include "world/state_industry.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

bool g_list_ends = false;

std::array<entity_id, 4> cardinal(world& w, entity_id tile)
{
    std::array<entity_id, 4> out{null_entity, null_entity, null_entity, null_entity};
    const tile_component& tc = w.tiles.at(tile);
    const body_component& bc = w.bodies.at(tc.body);
    const int gw = bc.grid_width, gh = bc.grid_height, x = tc.grid_x, y = tc.grid_y;
    const std::vector<entity_id>& grid = body_tile_grid(w, tc.body);
    const int c[4] = {y > 0 ? (y - 1) * gw + x : -1, y * gw + (x + 1) % gw,
                      y + 1 < gh ? (y + 1) * gw + x : -1, y * gw + (x + gw - 1) % gw};
    for (int k = 0; k < 4; ++k)
        if (c[k] >= 0 && static_cast<std::size_t>(c[k]) < grid.size())
            out[static_cast<std::size_t>(k)] = grid[static_cast<std::size_t>(c[k])];
    return out;
}

std::set<entity_id> station_tiles(const world& w)
{
    std::set<entity_id> s;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::railway_station)
            s.insert(b.tile);
    return s;
}

std::set<entity_id> city_tiles(const world& w)
{
    std::set<entity_id> s;
    for (const auto& [c, t] : w.population_centre_tile)
        s.insert(t);
    return s;
}

struct stage_snap
{
    std::string name;
    std::set<entity_id> stations;
};

struct census
{
    int track = 0, ends = 0, isolated = 0;
    int no_station_on = 0, unserved = 0, no_station_cheb1 = 0;
    int unserved_major = 0, unserved_minor = 0, unserved_unowned = 0;
    int unserved_dead_line = 0; ///< unserved end whose whole line (component) has no station
    int stations = 0;
    std::vector<entity_id> unserved_ends;
};

census read_census(world& w)
{
    census c;
    const entity_id body = w.home_body;
    const std::set<entity_id> st = station_tiles(w);
    c.stations = static_cast<int>(st.size());
    // Components over track, so a stub on a served line reads apart from a dead line.
    std::map<entity_id, int> comp;
    std::map<int, bool> comp_served;
    int ncomp = 0;
    for (const auto& [t, o] : w.rail_track)
    {
        (void)o;
        if (w.tiles.at(t).body != body || comp.count(t)) continue;
        std::vector<entity_id> f{t};
        comp[t] = ncomp;
        bool served = false;
        for (std::size_t h = 0; h < f.size(); ++h)
        {
            if (st.count(f[h])) served = true;
            for (const entity_id n : cardinal(w, f[h]))
            {
                if (n == null_entity) continue;
                if (st.count(n)) served = true;
                if (w.rail_track.count(n) && !comp.count(n)) { comp[n] = ncomp; f.push_back(n); }
            }
        }
        comp_served[ncomp++] = served;
    }
    for (const auto& [t, owner] : w.rail_track)
    {
        const tile_component& tc = w.tiles.at(t);
        if (tc.body != body) continue;
        ++c.track;
        int nb = 0;
        bool served = st.count(t) != 0;
        for (const entity_id n : cardinal(w, t))
        {
            if (n == null_entity) continue;
            if (w.rail_track.count(n)) ++nb;
            if (st.count(n)) served = true;
        }
        if (nb > 1) continue;
        ++c.ends;
        if (nb == 0) ++c.isolated;
        if (!st.count(t)) ++c.no_station_on;
        // Chebyshev 1 (the eight around it, columns wrapping).
        bool cheb = false;
        {
            const body_component& bc = w.bodies.at(body);
            const std::vector<entity_id>& grid = body_tile_grid(w, body);
            for (int dy = -1; dy <= 1 && !cheb; ++dy)
                for (int dx = -1; dx <= 1 && !cheb; ++dx)
                {
                    const int y = tc.grid_y + dy;
                    if (y < 0 || y >= bc.grid_height) continue;
                    const int x = (tc.grid_x + dx + bc.grid_width) % bc.grid_width;
                    cheb = st.count(grid[static_cast<std::size_t>(y * bc.grid_width + x)]) != 0;
                }
        }
        if (!cheb) ++c.no_station_cheb1;
        if (served) continue;
        ++c.unserved;
        c.unserved_ends.push_back(t);
        if (!comp_served[comp[t]]) ++c.unserved_dead_line;
        if (owner == null_entity) ++c.unserved_unowned;
        else if (is_major_nation(w, owner)) ++c.unserved_major;
        else ++c.unserved_minor;
    }
    return c;
}

void print_census(const char* label, const census& c)
{
    std::printf("  %-22s track %4d  stations %3d  ends %3d (isolated %2d)  no-station-on %3d  "
                "unserved %3d (major %3d minor %3d unowned %2d; whole line unstationed %3d)  "
                "none-within-1 %3d\n",
                label, c.track, c.stations, c.ends, c.isolated, c.no_station_on, c.unserved,
                c.unserved_major, c.unserved_minor, c.unserved_unowned, c.unserved_dead_line,
                c.no_station_cheb1);
}

bool touches(world& w, entity_id t, const std::set<entity_id>& s)
{
    if (s.count(t)) return true;
    for (const entity_id n : cardinal(w, t))
        if (n != null_entity && s.count(n)) return true;
    return false;
}

std::string attribute(world& w, entity_id end, const std::vector<stage_snap>& snaps,
                      const std::set<entity_id>& cities, const std::set<entity_id>& centres)
{
    // snaps[0] is right after the rail handover; a later stage that drops the last
    // serving station is the cause.
    for (std::size_t k = 1; k < snaps.size(); ++k)
        if (touches(w, end, snaps[k - 1].stations) && !touches(w, end, snaps[k].stations))
            return "taken_up:" + snaps[k].name;
    if (touches(w, end, cities)) return "cap_refused";
    for (const entity_id n : cardinal(w, end))
        if (n != null_entity && is_water(w.tiles.at(n).substrate)) return "water";
    if (centres.count(end)) return "centre_not_city";
    return "other";
}

/// The finish's tail from the rail handover on: harness_params.hpp's
/// apply_shipped_landscape `!skip_rail_handover` block, call for call (prints dropped),
/// with a station snapshot after each stage that can move a station.
void finish_tail(app_start_world& out, std::vector<stage_snap>* snaps)
{
    world& w = out.w;
    const recipe_registry& reg = out.reg;
    const opening_kit_config* const kit = out.cfg.opening_kit.active_config();
    const auto snap = [&](const char* n) {
        if (snaps) snaps->push_back({n, station_tiles(w)});
    };
    out.land.rail = hand_over_rail(w);
    snap("rail_handover");
    (void)place_colonial_ports(w);
    snap("colonial_ports");
    if (kit != nullptr)
        out.land.kit = lay_major_opening_kits(w, reg, *kit);
    snap("opening_kit");
    lay_site_roads_every_body(w);
    out.land.lp = seed_logistics_sources(w);
    snap("lp_seeding");
    if (kit != nullptr && kit->logistics_capped())
        cap_major_logistics(w, reg, *kit, out.land.kit);
    snap("logistics_cap");
    (void)fold_to_ground(w);
    snap("fold");
    if (kit != nullptr)
    {
        for (auto& [nid, nc] : w.nations)
            if (nc.is_major)
                nc.treasury = kit->treasury;
        w.handoff_chest = kit->handoff_treasury;
        release_kit_foundings(w, out.land.state);
    }
    if (kit != nullptr && kit->has_army())
        (void)seed_major_opening_armies(w, kit->army_capital, kit->army_border, kit->army_men,
                                        kit->land_recruits);
    (void)seed_capital_military_bases(w);
    out.land.reserve = reserve_state_first_tick(w, reg, out.land.state);
    stack_logistics_hubs(w, reg, out.land.lp, kit != nullptr && kit->logistics_capped());
    snap("stack_hubs");
    out.land.cut = size_opening_state_to_chest(w, reg, /*skip_majors=*/kit != nullptr);
    snap("state_cut");
    print_rail_prune(prune_rail_stubs(w)); // BL-1731: finish_campaign_world's, verbatim
    snap("rail_prune");
    lp_seed_read_at_gate(w, reg, out.land.lp);
    lay_site_roads_every_body(w);
    if (kit != nullptr && kit->has_fleet())
        (void)seed_major_opening_fleets(w, kit->fleet, kit->fleet_per_port, kit->sea_recruits);
    seed_arms_reserve(w);
    snap("finish");
}

int g_diverged = 0;

void run_seed(lua_state& lua, std::uint32_t seed, int ticks, bool check_path)
{
    world_params params = arc_params(world_arc::shipped);
    params.seed = seed;

    app_start_world out;
    build_app_base_world(lua, params, out);
    harness_charter_input ci;
    ci.skip_rail_handover = true; // the tail runs below, stage by stage
    apply_app_start_landscape(out, ci);
    world& w = out.w;

    std::vector<stage_snap> snaps;
    finish_tail(out, &snaps);

    if (check_path)
    {
        app_start_world ref;
        build_app_start_world(lua, params, ref);
        const std::uint64_t a = w.state_hash(0), b = ref.w.state_hash(0);
        std::printf("[check-path] seed %u: instrumented %016llX shipped %016llX %s\n",
                    static_cast<unsigned>(seed), static_cast<unsigned long long>(a),
                    static_cast<unsigned long long>(b), a == b ? "SAME" : "DIVERGED");
        if (a != b) ++g_diverged;
    }

    std::printf("\n=== seed %u ===  [rail] %d links laid of %d, %d broken at water, %d stations "
                "(%d refused by the stack cap)\n",
                static_cast<unsigned>(seed), out.land.rail.links_laid, out.land.rail.links,
                out.land.rail.links_broken, out.land.rail.stations, out.land.rail.stations_refused_cap);
    std::printf("  station tiles (all bodies) per stage:");
    for (const stage_snap& s : snaps)
        std::printf(" %s=%zu", s.name.c_str(), s.stations.size());
    std::printf("\n");

    const census at_start = read_census(w);
    print_census("start (after finish)", at_start);

    const std::set<entity_id> cities = city_tiles(w);
    std::set<entity_id> centres;
    for (const auto& [mid, mc] : w.markets)
        centres.insert(mc.centre_tile);
    std::map<std::string, int> cause;
    std::map<std::string, int> cause_major;
    for (const entity_id e : at_start.unserved_ends)
    {
        const std::string why = attribute(w, e, snaps, cities, centres);
        ++cause[why];
        const entity_id o = w.rail_track.at(e);
        if (o != null_entity && is_major_nation(w, o)) ++cause_major[why];
        if (g_list_ends)
        {
            const tile_component& tc = w.tiles.at(e);
            std::printf("    end (%d,%d) owner %u major %d: %s\n", tc.grid_x, tc.grid_y,
                        static_cast<unsigned>(o), o != null_entity && is_major_nation(w, o) ? 1 : 0,
                        why.c_str());
        }
    }
    std::printf("  CAUSE seed %u:", static_cast<unsigned>(seed));
    for (const auto& [k, v] : cause)
        std::printf(" %s=%d(major %d)", k.c_str(), v, cause_major[k]);
    std::printf("\n");

    // The settle, the seat, and a live window.
    run_app_validation_settle(w, out.reg);
    (void)seat_player_corporation(w, seed, out.land.search.winner_score);
    const census at_seat = read_census(w);
    print_census("after settle+seat", at_seat);
    if (ticks > 0)
    {
        run_app_live_window(w, out.reg, k_app_validation_ticks, ticks);
        char lab[48];
        std::snprintf(lab, sizeof lab, "after %d live ticks", ticks);
        print_census(lab, read_census(w));
    }
    std::printf("  ROW seed %u: track %d ends %d no-station-on %d unserved %d none-within-1 %d "
                "unserved-major %d unserved-minor %d\n",
                static_cast<unsigned>(seed), at_start.track, at_start.ends, at_start.no_station_on,
                at_start.unserved, at_start.no_station_cheb1, at_start.unserved_major,
                at_start.unserved_minor);
    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    int ticks = 60;
    bool check_path = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) { ticks = std::atoi(argv[++i]); continue; }
        if (std::strcmp(argv[i], "--check-path") == 0) { check_path = true; continue; }
        if (std::strcmp(argv[i], "--ends") == 0) { g_list_ends = true; continue; }
        seeds.push_back(static_cast<std::uint32_t>(std::strtoul(argv[i], nullptr, 10)));
    }
    if (seeds.empty())
        seeds = {0, 46, 7, 3, 29, 92, 2, 54, 17, 27, 31, 22, 13, 64, 63, 60}; // seed_library.json
    lua_state lua;
    for (const std::uint32_t s : seeds)
        run_seed(lua, s, ticks, check_path);
    return g_diverged == 0 ? 0 : 1;
}
