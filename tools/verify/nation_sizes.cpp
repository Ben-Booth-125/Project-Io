// ---------------------------------------------------------------------------
// nation_sizes — how big each nation is at the handoff (a pure reading)
// ---------------------------------------------------------------------------
// Ben's live click, 2026-10-17: "many minor powers are just very small, and
// the world is dominated by much larger powers". This prints, per seed and per
// target pair, every homeworld nation's tile count (largest first, majors
// marked), the size shares, how many sit under the 80-tile floor, and the
// closing rounds' own counts (floor absorptions, target absorptions,
// secessions, secessions taken below the floor). Built on the shipped arc and
// finished as the app finishes it, so majors are the nations a non-background
// corporation calls home.
//
// It reads only fields that exist on both sides of BL-1310 (nations
// contiguous), so the same source builds on a pre-BL-1310 tree for an A/B.
//
// Usage (repo root): build_gen/verify/nation_sizes.exe --seeds 0,28 [--pairs 20:4,6:1]
// Build: bash tools/verify/build_lua_harness.sh nation_sizes
// ---------------------------------------------------------------------------

#include "harness_params.hpp"
#include "world/components.hpp"
#include "world/era_minus_one.hpp"
#include "world/finish_campaign_world.hpp"
#include "world/hard_coded_world.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"
#include "world/works_roster.hpp"
#include "world/world_gen_config.hpp"
#include "scripting/lua_state.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
std::vector<int> parse_ints(const char* s, char sep)
{
    std::vector<int> out;
    std::string cur;
    for (const char* p = s;; ++p)
    {
        if (*p == sep || *p == '\0')
        {
            if (!cur.empty()) out.push_back(std::atoi(cur.c_str()));
            cur.clear();
            if (*p == '\0') break;
        }
        else cur.push_back(*p);
    }
    return out;
}
} // namespace

int main(int argc, char** argv)
{
    std::vector<int> seeds{ 0 };
    std::vector<std::pair<int, int>> pairs{ { 20, 4 } };
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = parse_ints(argv[++i], ',');
        else if (!std::strcmp(argv[i], "--pairs") && i + 1 < argc)
        {
            pairs.clear();
            const std::string all = argv[++i];
            std::size_t at = 0;
            while (at < all.size())
            {
                const std::size_t comma = all.find(',', at);
                const std::string one = all.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
                const std::size_t colon = one.find(':');
                if (colon != std::string::npos)
                    pairs.emplace_back(std::atoi(one.substr(0, colon).c_str()), std::atoi(one.substr(colon + 1).c_str()));
                if (comma == std::string::npos) break;
                at = comma + 1;
            }
        }
    }

    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
    recipe_registry  reg;
    lua.load("scripts/world_gen.lua");
    cfg.load_from_lua(lua);
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    reg.load_from_lua(lua);

    constexpr int k_floor = 80; // nation_params::min_nation_tiles' default
    for (const int seed : seeds)
        for (const auto& [tt, mt] : pairs)
        {
            world_params p = arc_params(world_arc::shipped);
            p.seed                = static_cast<uint32_t>(seed);
            p.nation_total_target = tt;
            p.major_target        = mt;
            generation_report     rep{};
            era_minus_one_fixture fx{};
            world w = make_hard_coded_world(p, &rep, cfg, nullptr, &works, &fx);
            recipe_registry seed_reg = reg;
            (void)finish_campaign_world(w, rep, seed_reg, p, cfg, nullptr);

            entity_id home = null_entity;
            for (const generation_report::body_entry& b : rep.bodies)
                if (b.is_homeworld) { home = b.id; break; }

            std::set<entity_id> majors;
            for (const auto& kv : w.nations) // BL-1407: majors are a saved fact (is_major)
                if (kv.second.is_major)
                    majors.insert(kv.first);

            struct row { int tiles; bool major; };
            std::vector<row> rows;
            long total_tiles = 0;
            for (const auto& kv : w.nations)
            {
                const nation_component& n = kv.second;
                if (n.tiles.empty()) continue;
                const auto t = w.tiles.find(n.tiles.front());
                if (t == w.tiles.end() || t->second.body != home) continue;
                rows.push_back({ static_cast<int>(n.tiles.size()), majors.count(kv.first) > 0 });
                total_tiles += static_cast<long>(n.tiles.size());
            }
            std::sort(rows.begin(), rows.end(), [](const row& a, const row& b) { return a.tiles > b.tiles; });

            long major_tiles = 0, top4 = 0;
            int  under_floor = 0, minors = 0;
            std::vector<int> minor_sizes;
            for (std::size_t i = 0; i < rows.size(); ++i)
            {
                if (rows[i].major) major_tiles += rows[i].tiles;
                else { ++minors; minor_sizes.push_back(rows[i].tiles); }
                if (i < 4) top4 += rows[i].tiles;
                if (rows[i].tiles < k_floor) ++under_floor;
            }
            const int median_minor = minor_sizes.empty() ? 0 : minor_sizes[minor_sizes.size() / 2];
            const history_sim_state& hs = fx.industrialisation_state;

            std::printf("seed %2d %2d:%d  nations %2zu  land %6ld  majors hold %5.1f%%  top4 hold %5.1f%%  "
                        "minors %2d median %4d  under floor(80) %2d  | closing: floor-abs %d target-abs %d "
                        "secede %d (subfloor %d)\n",
                        seed, tt, mt, rows.size(), total_tiles,
                        total_tiles ? 100.0 * major_tiles / total_tiles : 0.0,
                        total_tiles ? 100.0 * top4 / total_tiles : 0.0, minors, median_minor, under_floor,
                        hs.closing_floor_absorptions, hs.closing_target_absorptions, hs.closing_target_secessions,
                        hs.closing_subfloor_secessions);
            std::printf("        sizes:");
            for (const row& r : rows) std::printf(" %d%s", r.tiles, r.major ? "*" : "");
            std::printf("\n");
            std::fflush(stdout);
        }
    return 0;
}
