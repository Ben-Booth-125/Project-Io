// port_lane_probe — do the sea lanes reach the ports? (Ben, 2026-10-26: "sea lanes not
// connected to ports"). A PURE READER over the shipped campaign start.
//
// Two readings per seed, the fold's lane writers between them:
//   PRE  — after the landscape, with the state handover and the Logistic Point seeding
//          held back: the lanes on the water are the colonial spans' (`stamp_sea_lanes`).
//   POST — after finish_campaign_world's own order (state handover, rail handover, site
//          roads, `seed_logistics_sources`, which opens with `lay_all_port_lanes` and lays
//          a lane for each port it lands): the colonial lanes plus every port lane.
//
// Per standing port: its MOUTHS (water tiles on its six hex sides), whether any is
// laned, and whether a lane partner stands within fifty (`lp_sea_walk`, the measure
// `lay_port_lanes` reads). A port with a partner and no laned mouth is a DEFECT.
//
// Per lane END (a laned water tile with at most one laned 4-neighbour - the lane walk
// steps the four cardinal sides): PORT (a port's mouth), COAST (beside land with no
// port - a colonial landing, history), or OPEN (no land beside it). Each non-port end
// carries its grid distance to the nearest port. Ends present PRE are colonial; ends new
// POST are the port lanes', and every one of those must be a PORT end.
//
// Build:  bash tools/verify/build_lua_harness.sh port_lane_probe
// Run:    build_gen/verify/port_lane_probe.exe [--seeds 0,1,28,46]

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/hex_neighbors.hpp"
#include "world/logistic_points.hpp"
#include "world/logistics.hpp"
#include "world/road_generation.hpp"
#include "world/state_industry.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct lane_end { entity_id tile; int kind; int port_dist; }; // kind 0 port, 1 coast, 2 open

struct reading
{
    int ports = 0, with_partner = 0, touched = 0, partner_untouched = 0, lonely = 0;
    std::set<entity_id> ends;
    std::map<entity_id, lane_end> end_info;
    std::vector<entity_id> untouched_ports;
};

reading read(world& w, entity_id body)
{
    reading r;
    const auto bit = w.bodies.find(body);
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    const auto at = [&](int x, int y) -> entity_id {
        if (y < 0 || y >= gh) return null_entity;
        const int xx = ((x % gw) + gw) % gw;
        return grid[static_cast<std::size_t>(y * gw + xx)];
    };
    const auto laned = [&](entity_id t) {
        if (t == null_entity) return false;
        const tile_component& tc = w.tiles.at(t);
        return is_water(tc.substrate) && tc.lane_level > 0;
    };
    // Standing ports on this body, ascending.
    std::vector<std::pair<entity_id, entity_id>> ports;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::port && b.ticks_remaining <= 0 && !b.decommissioned
            && w.tiles.at(b.tile).body == body)
            ports.push_back({bid, b.tile});
    std::sort(ports.begin(), ports.end());
    std::set<entity_id> port_tiles, mouths;
    for (const auto& [bid, pt] : ports)
    {
        port_tiles.insert(pt);
        const tile_component& tc = w.tiles.at(pt);
        bool touched = false;
        for (int side = 0; side < 6; ++side)
        {
            const hex_neighbors::coord nb = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, side);
            const entity_id m = at(nb.gx, nb.gy);
            if (m == null_entity || !is_water(w.tiles.at(m).substrate)) continue;
            mouths.insert(m);
            if (laned(m)) touched = true;
        }
        const auto f = lp_sea_walk(w, body, {pt}, port_lane_limit());
        bool partner = false;
        for (const auto& [ob, ot] : ports)
        {
            if (ob == bid) continue;
            const auto it = f.find(ot);
            if (it != f.end() && it->second > 0.0f && it->second <= port_lane_limit() + lp_full_eps)
                partner = true;
        }
        ++r.ports;
        if (partner) ++r.with_partner; else ++r.lonely;
        if (touched) ++r.touched;
        if (partner && !touched) { ++r.partner_untouched; r.untouched_ports.push_back(bid); }
    }
    // Lane ends.
    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x)
        {
            const entity_id t = at(x, y);
            if (!laned(t)) continue;
            int deg = 0;
            const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
            for (int k = 0; k < 4; ++k)
                if (laned(at(x + dx[k], y + dy[k]))) ++deg;
            if (deg > 1) continue;
            lane_end e{t, 2, -1};
            if (mouths.count(t) != 0)
                e.kind = 0;
            else
            {
                const tile_component& tc = w.tiles.at(t);
                for (int side = 0; side < 6; ++side)
                {
                    const hex_neighbors::coord nb = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, side);
                    const entity_id m = at(nb.gx, nb.gy);
                    if (m != null_entity && !is_water(w.tiles.at(m).substrate)) { e.kind = 1; break; }
                }
                int best = -1;
                for (const entity_id pt : port_tiles)
                {
                    const tile_component& pc = w.tiles.at(pt);
                    int ddx = std::abs(pc.grid_x - x); if (ddx > gw - ddx) ddx = gw - ddx;
                    const int d = ddx + std::abs(pc.grid_y - y);
                    if (best < 0 || d < best) best = d;
                }
                e.port_dist = best;
            }
            r.ends.insert(t);
            r.end_info[t] = e;
        }
    return r;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds{0, 1, 28, 46};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc)
        {
            seeds.clear();
            std::stringstream ss(argv[++i]);
            std::string t;
            while (std::getline(ss, t, ','))
                if (!t.empty()) seeds.push_back(static_cast<std::uint32_t>(std::strtoul(t.c_str(), nullptr, 10)));
        }
        else { std::fprintf(stderr, "usage: port_lane_probe [--seeds a,b]\n"); return 2; }
    }
    int defects = 0;
    for (const std::uint32_t seed : seeds)
    {
        lua_state lua;
        world_params p;
        p.seed = seed;
        auto start = std::make_unique<app_start_world>();
        harness_charter_input in;
        in.skip_state_handover = true;
        build_app_base_world(lua, p, *start);
        apply_app_start_landscape(*start, in);
        world& w = start->w;
        std::vector<entity_id> bodies;
        for (const auto& [bid, bc] : w.bodies) { (void)bc; bodies.push_back(bid); }
        std::sort(bodies.begin(), bodies.end());
        std::map<entity_id, reading> pre;
        for (const entity_id b : bodies) pre[b] = read(w, b);

        hand_over_state_industry(w, start->reg);
        hand_over_rail(w);
        lay_site_roads_every_body(w);
        const lp_seed_report rep = seed_logistics_sources(w);

        for (const entity_id b : bodies)
        {
            const reading post = read(w, b);
            const reading& pr = pre[b];
            if (post.ports == 0 && post.ends.empty()) continue;
            int c_port = 0, c_coast = 0, c_open = 0, n_port = 0, n_coast = 0, n_open = 0, near_port = 0;
            for (const auto& [t, e] : post.end_info)
            {
                const bool colonial = pr.ends.count(t) != 0;
                int& slot = e.kind == 0 ? (colonial ? c_port : n_port)
                          : e.kind == 1 ? (colonial ? c_coast : n_coast) : (colonial ? c_open : n_open);
                ++slot;
                // A colonial end within three tiles of a port stops SHORT of it: listed.
                if (colonial && e.kind != 0 && e.port_dist >= 0 && e.port_dist <= 3)
                {
                    const tile_component& tc = w.tiles.at(t);
                    std::printf("CE seed=%u body=%u col=%d row=%d kind=%s port_dist=%d\n", seed,
                                static_cast<unsigned>(b), tc.grid_x, tc.grid_y,
                                e.kind == 1 ? "coast" : "open", e.port_dist);
                    ++near_port;
                }
                if (!colonial && e.kind != 0)
                {
                    const tile_component& tc = w.tiles.at(t);
                    std::printf("NE seed=%u body=%u col=%d row=%d kind=%s port_dist=%d\n", seed,
                                static_cast<unsigned>(b), tc.grid_x, tc.grid_y,
                                e.kind == 1 ? "coast" : "open", e.port_dist);
                }
            }
            for (const entity_id pb : post.untouched_ports)
            {
                const tile_component& tc = w.tiles.at(w.buildings.at(pb).tile);
                std::printf("PU seed=%u body=%u port=%u col=%d row=%d substrate=%d\n", seed,
                            static_cast<unsigned>(b), static_cast<unsigned>(pb), tc.grid_x, tc.grid_y,
                            static_cast<int>(tc.substrate));
            }
            defects += post.partner_untouched + n_coast + n_open;
            std::printf("B seed=%u body=%u PRE ports=%d ends=%d | POST ports=%d with_partner=%d lonely=%d "
                        "touched=%d partner_untouched=%d | colonial_ends port=%d coast=%d open=%d | "
                        "new_ends port=%d coast=%d open=%d | colonial_near_port=%d | lanes_laid=%d unreachable=%d\n",
                        seed, static_cast<unsigned>(b), pr.ports, static_cast<int>(pr.ends.size()),
                        post.ports, post.with_partner, post.lonely, post.touched, post.partner_untouched,
                        c_port, c_coast, c_open, n_port, n_coast, n_open, near_port,
                        rep.port_lanes_laid, rep.port_lanes_unreachable);
        }
        std::fflush(stdout);
    }
    std::printf("PORT_LANES %s: %d defect(s)\n", defects == 0 ? "PASS" : "FAIL", defects);
    return defects == 0 ? 0 : 1;
}
