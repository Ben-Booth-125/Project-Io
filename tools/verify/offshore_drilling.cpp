// offshore_drilling — BL-1522 (offshore drilling, supplied by nearby ports). Headless,
// no SDL / Lua / ImGui.
//
// The rules (Ben, 2026-11-10; TILES.md § Water kinds and § Shelf petroleum, PRODUCTION.md
// § Extraction buildings, LOGISTICS.md § Logistic Points "A rig is supplied through its
// own port"):
//   * an Oil Platform (extraction_site -> petroleum) may stand on SHELF sea (coast water)
//     carrying petroleum, and nowhere else on water - never open ocean, never a lake;
//   * only on its own nation's water, and only once the nation holds Offshore Drilling;
//   * it is supplied with Logistic Points through a standing port of its own nation
//     within lp_rig_port_reach water tiles; out of reach it is unsupplied (cover 0) but
//     placeable, as a land building with a road and no cover is;
//   * generation lays petroleum on shelf tiles (and on no other water).
//
// FIXTURE (a 20 x 6 hand-built body; columns wrap):
//   G1-G9   placement: tile-level and world-level, tech, ownership, the build door
//   L1-L5   supply: the port the rig pays at, the reach boundary, the resolver, a
//           foreign port, the cover a site read sees
//   S1      save round trip: the rig, its tile and its supply survive write/read
// CENSUS (the curated seeds, make_hard_coded_world with no prehistory):
//   C1      per seed, the home body: shelf tiles, shelf tiles carrying petroleum, their
//           total, how many lie on owned water; and that NO ocean or lake tile, and no
//           shelf tile save for petroleum, carries any deposit
//
// Build:  node tools/verify/build_harness.js offshore_drilling
// Run:    build_gen/verify/offshore_drilling.exe [--seeds 46,28,...] [--no-census]

#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/digitisation_tree_data.hpp"
#include "world/hard_coded_world.hpp"
#include "world/logistic_points.hpp"
#include "world/logistics.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

constexpr std::size_t k_oil = static_cast<std::size_t>(resource_type::petroleum);

constexpr int gw = 20, gh = 6;
entity_id g_tile[gh][gw];

entity_id add_nation(world& w)
{
    const entity_id n = w.create_entity();
    nation_component nc{};
    nc.treasury = 1.0e7f;
    w.nations[n] = nc;
    return n;
}

void own(world& w, entity_id tile, entity_id nation)
{
    w.tile_to_nation[tile] = nation;
    w.nations.at(nation).tiles.push_back(tile);
}

entity_id add_building(world& w, entity_id tile, building_type type, resource_type target,
                       entity_id owner)
{
    const entity_id b = w.create_entity();
    building_component bc{};
    bc.type               = type;
    bc.tile               = tile;
    bc.target_resource    = target;
    bc.ticks_remaining    = 0;
    bc.workforce_assigned = 1.0f;
    bc.workforce_target   = 100.0f;
    w.buildings[b] = bc;
    w.nations.at(owner).state_assets.push_back(b);
    return b;
}

const lp_building_row* row_of(const lp_tick& t, entity_id b)
{
    for (const lp_building_row& r : t.buildings)
        if (r.building == b)
            return &r;
    return nullptr;
}

void fixture()
{
    std::printf("offshore_drilling - BL-1522 fixture\n");

    recipe_registry reg;
    {
        building_economics ex;
        ex.base_rate = 40.0f; ex.maintenance = 2.0f; ex.base_wage = 4.0f; ex.build_cost = 100.0f;
        ex.richness_reference = 24.9f;
        reg.set_economics(building_type::extraction_site, ex);
    }

    world w;
    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};
    w.bodies[body].grid_width  = gw;
    w.bodies[body].grid_height = gh;
    // Rows 0-2 roaded rocky land, row 3 the shelf (coast water), rows 4-5 open ocean.
    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body = body; tc.grid_x = x; tc.grid_y = y;
            tc.substrate = y <= 2 ? terrain_substrate::rocky
                         : y == 3 ? terrain_substrate::coast
                                  : terrain_substrate::ocean;
            tc.habitability = 0.6f;
            // Petroleum on every shelf tile, on one ocean tile and on one land tile.
            if (y == 3 || (y == 5 && x == 5) || (y == 1 && x == 4))
            {
                tc.resource_deposit[k_oil]   = 60.0f;
                tc.resource_remaining[k_oil] = 6000.0f;
            }
            w.tiles[t] = tc;
            g_tile[y][x] = t;
        }
    // A lake (enclosed water) carrying petroleum, inland.
    const entity_id lake = g_tile[1][10];
    w.tiles[lake].substrate  = terrain_substrate::lake;
    w.tiles[lake].resource_deposit[k_oil] = 60.0f;
    // A Track on every land-to-land crossing, laid as edges so both halves agree (a save
    // refuses an unmirrored road). Water carries none: a rig needs no road.
    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x)
        {
            const entity_id t = g_tile[y][x];
            if (is_water(w.tiles.at(t).substrate)) continue;
            for (int side = 0; side < 6; ++side)
            {
                const entity_id n = road_edge_neighbour(w, t, side);
                if (n != null_entity && !is_water(w.tiles.at(n).substrate))
                    lay_road_edge(w, t, side, 1);
            }
        }
    const entity_id dry_shelf = g_tile[3][8];
    w.tiles[dry_shelf].resource_deposit[k_oil] = 0.0f; // a shelf tile with no oil

    const entity_id a = add_nation(w);
    const entity_id b = add_nation(w);
    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x)
        {
            if (y >= 4) continue;                    // open ocean: nobody's
            if (y == 3 && x == 16) continue;         // one unowned shelf tile
            own(w, g_tile[y][x], (x == 13 && y == 2) ? b : a);
        }
    // A's port at (1,2); B's port at (13,2), beside A's shelf.
    const entity_id port_a = add_building(w, g_tile[2][1], building_type::port, resource_type::iron_ore, a);
    add_building(w, g_tile[2][13], building_type::port, resource_type::iron_ore, b);

    using placement_rules::can_place;
    using placement_rules::can_place_in_world;
    using placement_rules::placement_reason;
    const auto tile_reason = [&](entity_id t, building_type ty, resource_type r) {
        return placement_rules::can_place_site(w.tiles.at(t), ty, r).reason; // the build door's
    };
    const entity_id near_rig = g_tile[3][2];
    const entity_id far_rig  = g_tile[3][10];
    const entity_id ocean    = g_tile[5][5];
    const entity_id unowned  = g_tile[3][16];
    const entity_id land_oil = g_tile[1][4];
    const auto ex = building_type::extraction_site;
    const auto oil = resource_type::petroleum;

    // G1-G4: the tile-level rule.
    check(tile_reason(near_rig, ex, oil) == placement_reason::ok,
          "G1 shelf tile carrying petroleum: an Oil Platform is valid (tile level)");
    check(tile_reason(ocean, ex, oil) == placement_reason::ocean
              && tile_reason(lake, ex, oil) == placement_reason::ocean,
          "G2 open ocean and a lake refuse a rig, though both carry petroleum");
    check(tile_reason(dry_shelf, ex, oil) == placement_reason::no_deposit,
          "G3 a shelf tile with no petroleum: no_deposit");
    check(tile_reason(near_rig, ex, resource_type::iron_ore) == placement_reason::ocean
              && tile_reason(near_rig, building_type::port, resource_type::iron_ore) == placement_reason::ocean
              && tile_reason(near_rig, building_type::processing_facility, oil) == placement_reason::ocean,
          "G4 the rig is the ONE building on water: any other type or target on the shelf is refused");
    check(tile_reason(land_oil, ex, oil) == placement_reason::ok,
          "G4b the land Oil Platform is unchanged");
    // G0 (cold review, fix 1): the BARE check - what every generation placer asks - keeps
    // refusing water, a rig included; only the build door's can_place_site admits one.
    check(can_place(w.tiles.at(near_rig), ex, oil).reason == placement_reason::ocean
              && can_place(w.tiles.at(land_oil), ex, oil).ok(),
          "G0 bare can_place (generation's check) refuses a rig on the shelf; land oil unchanged");

    // G5-G7: the world-level rule, asked as nation A.
    check(can_place_in_world(w, near_rig, ex, oil, 24.0f, a).reason == placement_reason::tech_locked,
          "G5 without Offshore Drilling the rig is tech_locked");
    const int node = io::dg::find_node("DG-PR-EX-A");
    check(node >= 0, "G5b the store names DG-PR-EX-A");
    w.nation_tech[a].held.set(static_cast<std::size_t>(node));
    check(can_place_in_world(w, near_rig, ex, oil, 24.0f, a).ok(),
          "G6 holding Offshore Drilling: the rig on A's own shelf places (no road, reach or province test)");
    check(can_place_in_world(w, far_rig, ex, oil, 24.0f, a).ok(),
          "G6b a rig out of reach of any own port still places (unsupplied, as a roaded land site with no cover)");
    check(can_place_in_world(w, unowned, ex, oil, 24.0f, a).reason == placement_reason::not_own_ground,
          "G7 water no nation owns: refused not_own_ground");
    check(can_place_in_world(w, ocean, ex, oil, 24.0f, a).reason == placement_reason::ocean,
          "G7b open ocean refused even with the tech");
    check(can_place_in_world(w, near_rig, ex, oil).ok(),
          "G7c the tile read with no builder named passes (the gate asks an actor)");

    // G8-G9: the build door, the legal verb both the player and a rival issue.
    entity_id built = null_entity, refused = null_entity;
    const construction_result r_ok  = construct_building(w, reg, a, near_rig, ex, oil, built);
    const construction_result r_bad = construct_building(w, reg, a, ocean, ex, oil, refused);
    check(r_ok == construction_result::placed && w.buildings.count(built) == 1,
          "G8 construct_building places the rig on the shelf");
    check(r_bad == construction_result::invalid_tile && refused == null_entity,
          "G9 construct_building refuses a rig on open ocean");

    // L1-L3: the port a rig pays at.
    int steps = -2;
    const entity_id paid = rig_supply_port_tile(w, a, near_rig, &steps);
    check(paid == g_tile[2][1] && steps >= 1 && steps <= lp_rig_port_reach,
          "L1 a rig beside A's port is paid at that port");
    std::printf("    L1 near rig: %d water step(s) to its port\n", steps);
    check(rig_supply_port_tile(w, a, far_rig) == null_entity,
          "L2 a rig with only ANOTHER nation's port in reach is unsupplied (ports are own-nation only)");
    {
        // The reach boundary: every one of A's shelf tiles is supplied exactly when the
        // any-water walk from it lands on A's port within lp_rig_port_reach.
        const auto walk_all = lp_sea_walk(w, body, {g_tile[2][1]}, 100.0f);
        int agree = 0, total = 0, in_reach = 0, max_in = -1, min_out = 1000;
        for (int x = 0; x < gw; ++x)
        {
            const entity_id t = g_tile[3][x];
            if (w.tile_to_nation.count(t) == 0 || w.tile_to_nation.at(t) != a)
                continue;
            ++total;
            const auto it = walk_all.find(t);
            const int d = it == walk_all.end() ? 1000 : static_cast<int>(std::lround(it->second));
            const bool sup = rig_supply_port_tile(w, a, t) != null_entity;
            if (sup == (d <= lp_rig_port_reach)) ++agree;
            if (sup) { ++in_reach; if (d > max_in) max_in = d; }
            else if (d < min_out) min_out = d;
        }
        std::printf("    L3 A's shelf: %d tiles, %d in reach, furthest supplied %d, nearest unsupplied %d\n",
                    total, in_reach, max_in, min_out);
        check(agree == total && in_reach > 0 && in_reach < total
                  && max_in == lp_rig_port_reach && min_out == lp_rig_port_reach + 1,
              "L3 supplied exactly within lp_rig_port_reach water tiles of an own port, and not one tile past");
    }

    // L4: the resolver. Complete the built rig and add one far out.
    w.buildings.at(built).ticks_remaining = 0;
    w.buildings.at(built).workforce_assigned = 1.0f;
    w.buildings.at(built).workforce_target   = 100.0f;
    const entity_id far_b = add_building(w, far_rig, ex, oil, a);
    const lp_tick t = resolve_logistic_points(w);
    const lp_building_row* rn = row_of(t, built);
    const lp_building_row* rf = row_of(t, far_b);
    std::printf("    L4 near rig: coverage %.3f fraction %.3f; far rig: coverage %.3f fraction %.3f\n",
                rn ? rn->coverage : -1.0f, rn ? rn->fraction : -1.0f,
                rf ? rf->coverage : -1.0f, rf ? rf->fraction : -1.0f);
    check(rn != nullptr && rn->coverage >= 0.999f && rn->fraction >= 0.999f,
          "L4 the rig in reach of A's port is supplied in full");
    check(rf != nullptr && rf->coverage == 0.0f && rf->fraction == 0.0f,
          "L4b the rig out of reach is unsupplied");
    {
        lp_reach_memo memo;
        check(lp_coverage_at(w, memo, near_rig) >= 0.999f && lp_coverage_at(w, memo, far_rig) == 0.0f,
              "L5 lp_coverage_at (what the site reads) agrees with the resolver on both rigs");
    }
    {
        // L6: the Logistics lens's flow routes a rig at its port, the resolver's rule: the
        // supplied rig's request is drawn from the port tile (never its own water tile),
        // and the unsupplied rig's lies on its own tile with nothing granted.
        lp_reach_memo memo;
        const auto flow = lp_route_flow(w, memo, t, a, body);
        const auto at_port = flow.find(g_tile[2][1]);
        const auto at_near = flow.find(near_rig);
        const auto at_far  = flow.find(far_rig);
        std::printf("    L6 flow: port requested %.3f granted %.3f; near rig tile %s; far rig requested %.3f granted %.3f\n",
                    at_port != flow.end() ? at_port->second.requested : -1.0f,
                    at_port != flow.end() ? at_port->second.granted : -1.0f,
                    at_near != flow.end() ? "has flow" : "no flow",
                    at_far != flow.end() ? at_far->second.requested : -1.0f,
                    at_far != flow.end() ? at_far->second.granted : -1.0f);
        check(at_port != flow.end() && at_port->second.requested > 0.0f && at_port->second.granted > 0.0f
                  && at_near == flow.end(),
              "L6 the lens flow routes the supplied rig at its port tile, not its own water tile");
        check(at_far != flow.end() && at_far->second.requested > 0.0f && at_far->second.granted == 0.0f,
              "L6b the unsupplied rig's request lies on its own tile, ungranted");
    }
    {
        // A decommissioned port is no gateway.
        w.buildings.at(port_a).decommissioned = true;
        check(rig_supply_port_tile(w, a, near_rig) == null_entity,
              "L5b a decommissioned port supplies no rig");
        w.buildings.at(port_a).decommissioned = false;
    }

    // S1: save round trip.
    {
        std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
        write_world_snapshot(w, ss);
        world r;
        const bool read = read_world_snapshot(r, ss);
        bool same = read && r.buildings.count(built) == 1 && r.buildings.count(far_b) == 1;
        if (same)
        {
            const building_component& rb = r.buildings.at(built);
            same = rb.tile == near_rig && rb.type == ex && rb.target_resource == oil
                && r.tiles.at(near_rig).substrate == terrain_substrate::coast
                && r.tiles.at(near_rig).resource_deposit[k_oil] == w.tiles.at(near_rig).resource_deposit[k_oil]
                && r.tile_to_nation.at(near_rig) == a
                && r.nation_tech.at(a).held.test(static_cast<std::size_t>(node));
        }
        float fn = -1.0f, ff = -1.0f;
        if (same)
        {
            const lp_tick rt = resolve_logistic_points(r);
            const lp_building_row* a1 = row_of(rt, built);
            const lp_building_row* a2 = row_of(rt, far_b);
            fn = a1 ? a1->fraction : -1.0f;
            ff = a2 ? a2->fraction : -1.0f;
            same = a1 && a2 && fn == rn->fraction && ff == rf->fraction
                && can_place_in_world(r, g_tile[3][3], ex, oil, 24.0f, a).ok();
        }
        std::printf("    S1 read %s; after load near %.3f far %.3f\n", read ? "ok" : "FAILED", fn, ff);
        check(same, "S1 save round trip: the rig, its shelf tile, its tech and its supply survive");
    }
}

// C1 - the curated seeds.
void census(const std::vector<unsigned>& seeds)
{
    std::printf("\nC1 shelf petroleum on the curated seeds (home body, no prehistory)\n");
    long all_shelf = 0, all_oil = 0, all_owned = 0;
    double all_sum = 0.0, all_land_sum = 0.0;
    int  bad = 0, seeds_with_oil = 0;
    for (const unsigned seed : seeds)
    {
        world_params p;
        p.seed             = seed;
        p.prehistory_years = 0;
        world w = make_hard_coded_world(p);
        int shelf = 0, shelf_oil = 0, owned_oil = 0, wet_other = 0, shelf_other = 0, land_oil = 0;
        double sum = 0.0, land_sum = 0.0;
        for (const auto& [tid, tc] : w.tiles)
        {
            if (tc.body != w.home_body) continue;
            float other = 0.0f;
            for (std::size_t r = 0; r < resource_count; ++r)
                if (r != k_oil) other += tc.resource_deposit[r];
            if (is_coastal_water(tc.substrate))
            {
                ++shelf;
                if (tc.resource_deposit[k_oil] > 0.0f)
                {
                    ++shelf_oil;
                    sum += tc.resource_deposit[k_oil];
                    if (w.tile_to_nation.count(tid) != 0) ++owned_oil;
                }
                if (other > 0.0f) ++shelf_other;
            }
            else if (is_water(tc.substrate))
            {
                if (other + tc.resource_deposit[k_oil] > 0.0f) ++wet_other;
            }
            else if (tc.resource_deposit[k_oil] > 0.0f)
            {
                ++land_oil;
                land_sum += tc.resource_deposit[k_oil];
            }
        }
        std::printf("C seed=%u shelf=%d shelf_oil=%d shelf_oil_sum=%.0f owned_shelf_oil=%d "
                    "land_oil=%d land_oil_sum=%.0f ocean_or_lake_deposits=%d shelf_non_oil=%d\n",
                    seed, shelf, shelf_oil, sum, owned_oil, land_oil, land_sum, wet_other, shelf_other);
        all_shelf += shelf; all_oil += shelf_oil; all_owned += owned_oil;
        all_sum += sum; all_land_sum += land_sum;
        if (shelf_oil > 0) ++seeds_with_oil;
        if (wet_other != 0 || shelf_other != 0) ++bad;
    }
    const double tile_share = all_shelf > 0 ? static_cast<double>(all_oil) / static_cast<double>(all_shelf) : 0.0;
    const double sum_share  = all_land_sum > 0.0 ? all_sum / all_land_sum : 0.0;
    std::printf("C total shelf=%ld shelf_oil=%ld (%.1f%% of shelf) owned=%ld shelf_oil_sum=%.0f "
                "land_oil_sum=%.0f (shelf = %.1f%% of land) seeds_with_shelf_oil=%d/%zu\n",
                all_shelf, all_oil, 100.0 * tile_share, all_owned, all_sum, all_land_sum,
                100.0 * sum_share, seeds_with_oil, seeds.size());
    check(bad == 0, "C1 no ocean or lake tile carries a deposit; a shelf tile carries petroleum only");
    check(all_oil > 0, "C1b shelf petroleum exists on the curated seeds");
    // The calibration band (2026-11-11, BL-1522): basin-like, not a blanket. Gated only on
    // the full curated set, where the band was set; a subset reports.
    if (seeds.size() >= 16)
        check(tile_share >= 0.15 && tile_share <= 0.25 && sum_share >= 0.10 && sum_share <= 0.20,
              "C1c pooled: 15-25% of shelf tiles bear oil, worth 10-20% of the land's oil");
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<unsigned> seeds = { 46, 28, 11, 31, 40, 12, 37, 13, 41, 43, 32, 10, 25, 38, 9, 0 };
    bool run_census = true;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            seeds.clear();
            std::stringstream ss(argv[++i]);
            std::string t;
            while (std::getline(ss, t, ','))
                if (!t.empty()) seeds.push_back(static_cast<unsigned>(std::strtoul(t.c_str(), nullptr, 10)));
        }
        else if (std::strcmp(argv[i], "--no-census") == 0)
            run_census = false;
    }
    fixture();
    if (run_census)
        census(seeds);
    std::printf("\n%s: %d failure(s)\n", g_fail == 0 ? "PASS" : "FAIL", g_fail);
    return g_fail == 0 ? 0 : 1;
}
