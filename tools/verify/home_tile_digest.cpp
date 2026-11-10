// ---------------------------------------------------------------------------
// home_tile_digest — the homeworld's ground, as one number per seed.
//
// Built for BL-1638 (generated body mass): a change that rolls the OTHER bodies'
// physique must leave the homeworld and its tiles byte-identical. Run it on the
// tree before and after such a change and compare the rows. It prints, per seed:
//
//   home   — every tile on the home body, field by field (FNV-1a 64), plus the
//            home body_component's grid and mass;
//   others — the same fold over every non-home body's tiles and mass, which a
//            change to the other bodies is EXPECTED to move.
//
// Usage: home_tile_digest [seed ...]   (default 0 46 20260911)
// Headless: world/* logic only, no SDL and no Lua.
// ---------------------------------------------------------------------------

#include "world/hard_coded_world.hpp"
#include "world_digest.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

void fold_tile(fnv1a64& f, const tile_component& t)
{
    f.scalar(t.body);
    f.scalar(t.grid_x);
    f.scalar(t.grid_y);
    f.scalar(t.substrate);
    f.scalar(t.cover);
    f.scalar(t.cover_density);
    f.scalar(t.landform);
    for (float v : t.resource_deposit) f.scalar(v);
    for (float v : t.resource_remaining) f.scalar(v);
    f.scalar(t.substrate_density);
    f.scalar(t.road_edges);
    f.scalar(t.river_edges);
    f.scalar(t.river_downstream);
    f.scalar(t.height);
    f.scalar(t.lane_level);
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    for (int i = 1; i < argc; ++i) seeds.push_back(static_cast<uint32_t>(std::strtoul(argv[i], nullptr, 10)));
    if (seeds.empty()) seeds = { 0u, 46u, 20260911u };

    for (uint32_t seed : seeds)
    {
        world_params wp;
        wp.seed = seed;
        const world w = make_hard_coded_world(wp);

        std::vector<entity_id> ids;
        ids.reserve(w.tiles.size());
        for (const auto& [id, t] : w.tiles) ids.push_back(id);
        std::sort(ids.begin(), ids.end());

        fnv1a64 home, others;
        std::size_t n_home = 0, n_other = 0;
        for (entity_id id : ids)
        {
            const tile_component& t = w.tiles.at(id);
            if (t.body == w.home_body) { fold_tile(home, t); ++n_home; }
            else                       { fold_tile(others, t); ++n_other; }
        }
        std::vector<entity_id> bodies;
        for (const auto& [id, b] : w.bodies) bodies.push_back(id);
        std::sort(bodies.begin(), bodies.end());
        for (entity_id id : bodies)
        {
            const body_component& b = w.bodies.at(id);
            fnv1a64& f = (id == w.home_body) ? home : others;
            f.scalar(id);
            f.scalar(b.grid_width);
            f.scalar(b.grid_height);
            f.scalar(b.mass_earths);
            f.scalar(b.atmosphere);
        }
        std::printf("seed %u  home tiles=%zu digest=%016llx  mass=%.6f | others tiles=%zu digest=%016llx\n",
                    seed, n_home, static_cast<unsigned long long>(home.h),
                    w.bodies.at(w.home_body).mass_earths,
                    n_other, static_cast<unsigned long long>(others.h));
        for (entity_id id : bodies)
        {
            const body_component& b = w.bodies.at(id);
            if (b.type == body_type::star) continue;
            std::printf("      body %u %-12s type=%d mass=%.7f atm=%d\n", static_cast<unsigned>(id),
                        b.name.c_str(), static_cast<int>(b.type), b.mass_earths, static_cast<int>(b.atmosphere));
        }
        std::fflush(stdout);
    }
    return 0;
}
