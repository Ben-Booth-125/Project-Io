// march_detour_probe -- does a move order's path take the march's own fastest route?
//
// Ben, 2026-11-10: "sometimes units will go across a road when they can go directly to
// the targeted tile (off road either way)". A move order's path is solved by
// `march_path`, which borrows the logistics route finder (`intra_body_path`), while the
// march itself pays `march_hop_cost` per hop (the logistics edge times the class's
// terrain MOVE row and any air-superiority slow). This probe asks, for the seat's land
// units on each seed and every off-road land destination within a few tiles of each:
//
//   chosen  = march days along march_path's route, priced hop by hop as the march pays
//   best    = a Dijkstra over the same topology priced by march_hop_cost itself
//
// A row where chosen > best is a route the march would beat (the route finder and the
// march disagree). A row where the route touches a road and chosen == best is a detour
// that IS the fastest (the model's road discount, not a pathing bug).
//
// Run from the repo root: build_gen/verify/march_detour_probe.exe [--seeds a,b] [--radius R]
// Output: one ROW per seed and EX lines for the worst disagreements. Reports; never gates.
#include "harness_params.hpp"

#include "scripting/lua_state.hpp"
#include "world/battle_system.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/logistics.hpp"
#include "world/province.hpp"
#include "world/settlement.hpp"
#include "world/spawn_seat.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <vector>


namespace {

bool roaded(const tile_component& t) { return max_road_tier(t) > 0; }

float path_days(const world& w, const recipe_registry& reg, unit_component probe,
                const std::vector<entity_id>& path)
{
    float sum = 0.0f;
    for (std::size_t i = 1; i < path.size(); ++i)
    {
        const auto a = w.tiles.find(path[i - 1]);
        const auto b = w.tiles.find(path[i]);
        if (a == w.tiles.end() || b == w.tiles.end())
            return -1.0f;
        probe.position = path[i - 1];
        sum += march_hop_cost(w, reg, probe, b->second, path[i], a->second);
    }
    return sum;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds = { 0, 46, 7, 3, 29, 92, 2, 54 };
    int radius = 6;
    bool old_route = false; // --old: the network route, as before 2026-11-10
    bool hex6 = false;      // --hex6: best over all six hex sides, not the route's four
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--old") == 0)
            old_route = true;
        else if (std::strcmp(argv[i], "--hex6") == 0)
            hex6 = true;
        else if (std::strcmp(argv[i], "--radius") == 0 && i + 1 < argc)
            radius = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            seeds.clear();
            std::string s = argv[++i];
            std::size_t at = 0;
            while (at <= s.size())
            {
                const std::size_t c = s.find(',', at);
                const std::string tok = s.substr(at, c == std::string::npos ? std::string::npos : c - at);
                if (!tok.empty()) seeds.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
                if (c == std::string::npos) break;
                at = c + 1;
            }
        }
    }

    lua_state lua;
    long long all_pairs = 0, all_worse = 0, all_road_detour = 0;
    for (uint32_t seed : seeds)
    {
        world_params p = arc_params(world_arc::shipped);
        p.seed = seed;
        auto out = std::make_unique<app_start_world>();
        era_minus_one_fixture fx;
        out->params = p;
        lua.load("scripts/world_gen.lua");
        out->cfg = world_gen_config{};
        out->cfg.load_from_lua(lua);
        if (out->works.size() == 0)
        {
            lua.load("scripts/works.lua");
            out->works.load_from_lua(lua);
        }
        out->report = generation_report{};
        out->w = make_hard_coded_world(p, &out->report, out->cfg, nullptr, &out->works, &fx);
        seed_genesis_history(out->w, out->report);
        init_survey_states(out->w);
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        out->reg.load_from_lua(lua);
        band_registry_from_world(out->reg, out->w);
        assign_default_recipes(out->w, out->reg);
        apply_app_start_landscape(*out);
        run_app_validation_settle(out->w, out->reg);
        (void)seat_player_corporation(out->w, seed, out->land.search.winner_score);

        world& w = out->w;
        const recipe_registry& reg = out->reg;
        const military_capability_params& mil = reg.military();
        const entity_id seat = player_nation(w);

        // Grid lookup on each body, for the march neighbours (any tile road_side_between
        // names a side for).
        std::map<std::tuple<entity_id, int, int>, entity_id> at;
        std::map<entity_id, int> body_w;
        for (const auto& [tid, t] : w.tiles)
        {
            at[{ t.body, t.grid_x, t.grid_y }] = tid;
            body_w[t.body] = std::max(body_w[t.body], t.grid_x + 1);
        }
        const auto march_neighbours = [&](entity_id cur) {
            std::vector<entity_id> outv;
            const tile_component& t = w.tiles.at(cur);
            const int gw = body_w[t.body];
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (dx == 0 && dy == 0) continue;
                    if (!hex6 && dx != 0 && dy != 0) continue; // the route's own 4 offsets
                    const int x = ((t.grid_x + dx) % gw + gw) % gw;
                    const auto it = at.find({ t.body, x, t.grid_y + dy });
                    if (it != at.end() && road_side_between(w, cur, it->second) >= 0)
                        outv.push_back(it->second);
                }
            return outv;
        };

        // One unit per class the seat fields, standing off-road on its home body.
        std::map<uint16_t, entity_id> by_type;
        for (const auto& [uid, u] : w.units)
        {
            if (u.owner != seat) continue;
            const auto t = w.tiles.find(u.position);
            if (t == w.tiles.end() || is_water(t->second.substrate)) continue;
            if (march_days_per_tile(mil, u.type) <= 0.0f) continue;
            by_type.emplace(u.type, uid);
        }

        long long pairs = 0, worse = 0, road_detour = 0, road_touch = 0, ties_longer = 0;
        float worst_gap = 0.0f;
        std::vector<std::string> examples;
        for (const auto& [type, uid] : by_type)
        {
            const unit_component& u = w.units.at(uid);
            const entity_id body = w.tiles.at(u.position).body;
            const std::set<entity_id> foreign = foreign_held_tiles(w, u.owner);
            const std::function<bool(const tile_component&)> cwall = class_wall(mil, u.type);
            const std::function<bool(const tile_component&)> wall = march_wall(u.type, cwall);
            const auto walled = [&](entity_id t) {
                const auto it = w.tiles.find(t);
                return it == w.tiles.end() || (wall && wall(it->second));
            };

            // Dijkstra from the unit over the march topology (neighbours a road side
            // can join), priced by the march's own hop cost.
            std::map<entity_id, float> dist;
            std::map<entity_id, int> hops;
            using qe = std::pair<float, entity_id>;
            std::priority_queue<qe, std::vector<qe>, std::greater<qe>> q;
            dist[u.position] = 0.0f;
            hops[u.position] = 0;
            q.push({ 0.0f, u.position });
            while (!q.empty())
            {
                const auto [d, cur] = q.top();
                q.pop();
                if (d > dist[cur]) continue;
                if (hops[cur] >= radius * 3) continue;
                unit_component probe = u;
                probe.position = cur;
                const tile_component& ct = w.tiles.at(cur);
                for (const entity_id n : march_neighbours(cur))
                {
                    if (walled(n) || foreign.count(n)) continue;
                    const float nd = d + march_hop_cost(w, reg, probe, w.tiles.at(n), n, ct);
                    const auto it = dist.find(n);
                    if (it == dist.end() || nd < it->second - 1e-6f)
                    {
                        dist[n] = nd;
                        hops[n] = hops[cur] + 1;
                        q.push({ nd, n });
                    }
                }
            }

            for (const auto& [dest, best] : dist)
            {
                if (dest == u.position || hops[dest] > radius) continue;
                const tile_component& dt = w.tiles.at(dest);
                if (roaded(dt) || roaded(w.tiles.at(u.position))) continue; // off road either way
                const std::vector<entity_id> path =
                    march_path(w, body, u.position, dest, foreign, wall ? &wall : nullptr,
                               old_route ? nullptr : &mil, old_route ? nullptr : &u);
                if (path.size() < 2) continue;
                ++pairs;
                const float chosen = path_days(w, reg, u, path);
                bool touches = false;
                for (std::size_t i = 1; i + 1 < path.size(); ++i)
                    touches = touches || roaded(w.tiles.at(path[i]));
                if (touches) ++road_touch;
                const float gap = chosen - best;
                if (gap > 1e-3f * std::max(1.0f, best))
                {
                    ++worse;
                    if (gap > worst_gap) worst_gap = gap;
                    if (examples.size() < 6)
                    {
                        const tile_component& st = w.tiles.at(u.position);
                        char buf[256];
                        std::snprintf(buf, sizeof buf,
                                      "EX seed=%u type=%u from=%d,%d to=%d,%d chosen=%.3f best=%.3f path_tiles=%zu best_hops=%d road=%d",
                                      seed, static_cast<unsigned>(type), st.grid_x, st.grid_y, dt.grid_x, dt.grid_y,
                                      chosen, best, path.size() - 1, hops[dest], touches ? 1 : 0);
                        examples.push_back(buf);
                    }
                }
                else if (touches)
                {
                    ++road_detour;
                    if (static_cast<int>(path.size()) - 1 > hops[dest]) ++ties_longer;
                }
            }
        }
        std::printf("ROW seed=%u classes=%zu pairs=%lld worse=%lld worst_gap=%.3f road_touch=%lld road_fastest=%lld\n",
                    seed, by_type.size(), pairs, worse, worst_gap, road_touch, road_detour);
        for (const std::string& e : examples)
            std::printf("%s\n", e.c_str());
        std::fflush(stdout);
        all_pairs += pairs;
        all_worse += worse;
        all_road_detour += road_detour;
        (void)ties_longer;
    }
    std::printf("TOTAL pairs=%lld worse=%lld road_fastest=%lld\n", all_pairs, all_worse, all_road_detour);
    return 0;
}
