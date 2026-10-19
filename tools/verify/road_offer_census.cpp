// ---------------------------------------------------------------------------
// road_offer_census — BL-1385: can the player build a road anywhere?
// ---------------------------------------------------------------------------
// Ben, in play 2026-10-18: "I can't seem to find anywhere that's valid to build a
// road. Every tile with a road I try to press has the build button greyed out with
// 'nowhere left to cross from this tile at this tier'."
//
// The press is muted when `ui::road_crossing_targets` is empty, and that set is
// exactly what `placement_rules::can_place_road` admits. So this walks the SHIPPED
// start world and asks the rules the same question the UI asks, tile by tile and
// tier by tier, as the PLAYER's own corporation — then counts not only how often
// the answer is "nowhere" but WHICH RULE said so, which is the thing a player
// cannot see and the explainer never used to say.
//
//   C1  Of every tile, and of every ROADED tile (the ones a player presses),
//       how many admit at least one crossing at Track, Road and Highway.
//   C2  The refusal breakdown over roaded tiles: foreign ground (BL-1357),
//       nothing here to build from (BL-1348), already at this tier, or no
//       crossable side at all.
//   C3  The same, restricted to the PLAYER'S OWN NATION's ground — the only
//       ground BL-1357 lets them build out of, and so the only honest denominator
//       for "can the player build a road".
//
// It REPORTS; it asserts only that the world generated and that the player exists.
// What counts as too tight is Ben's call, and the numbers are what he rules on.
//
// Build: bash tools/verify/build_lua_harness.sh road_offer_census
// Run:   build_gen/verify/road_offer_census.exe [--seeds a,b,...]   (default 0)

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "ui/road_tool.hpp"
// The target walk lives in src/ui and the harness build compiles src/world only, so the
// one SDL-free translation unit comes in directly - the same idiom lp_pool_harness uses
// for agent_protocol.cpp. Asking a copy of the rules instead would be the very drift
// this census exists to measure.
#include "../../src/ui/road_tool.cpp"
#include "world/components.hpp"
#include "world/placement_rules.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_fail = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++g_fail;
}

struct tier_counts
{
    int tiles = 0;        ///< tiles asked
    int roaded = 0;       ///< ...carrying a crossing, so a player would press them
    int admits = 0;       ///< ...admitting at least one crossing
    int roaded_admits = 0;
    int foreign = 0, no_road = 0, already = 0, uncrossable = 0; ///< the refusal of a roaded tile
};

void census(const world& w, entity_id body, entity_id actor, entity_id own_nation,
            bool own_ground_only, tier_counts out[3])
{
    using reason = placement_rules::placement_reason;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body)
            continue;
        if (own_ground_only)
        {
            const auto it = w.tile_to_nation.find(tid);
            if (it == w.tile_to_nation.end() || it->second != own_nation)
                continue;
        }
        const bool roaded = tile_has_road(tc);
        for (int t = 1; t <= 3; ++t)
        {
            tier_counts& c = out[t - 1];
            ++c.tiles;
            if (roaded) ++c.roaded;
            const std::vector<ui::road_target> targets =
                ui::road_crossing_targets(w, tid, static_cast<std::uint8_t>(t), actor);
            if (!targets.empty())
            {
                ++c.admits;
                if (roaded) ++c.roaded_admits;
                continue;
            }
            if (!roaded)
                continue; // an unroaded tile refuses for BL-1348 by construction
            switch (ui::road_crossing_refusal(w, tid, static_cast<std::uint8_t>(t), actor))
            {
                case reason::foreign_ground:    ++c.foreign;     break;
                case reason::no_road_to_extend: ++c.no_road;     break;
                case reason::ocean:             ++c.uncrossable; break;
                default:                        ++c.already;     break;
            }
        }
    }
}

void print_rows(const char* label, const tier_counts c[3])
{
    static const char* names[3] = {"Track  ", "Road   ", "Highway"};
    for (int t = 0; t < 3; ++t)
    {
        const tier_counts& x = c[t];
        const double pct = x.roaded > 0 ? 100.0 * x.roaded_admits / x.roaded : 0.0;
        std::printf("  %-18s %s: %d of %d ROADED tiles admit a crossing (%.1f%%)"
                    " | refused: foreign %d, no road here %d, already at tier %d, no crossable side %d"
                    " | all tiles %d of %d\n",
                    label, names[t], x.roaded_admits, x.roaded, pct,
                    x.foreign, x.no_road, x.already, x.uncrossable, x.admits, x.tiles);
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds{0};
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            seeds.clear();
            char* p = argv[++i];
            while (*p)
            {
                seeds.push_back(static_cast<std::uint32_t>(std::strtoul(p, &p, 10)));
                if (*p == ',') ++p; else break;
            }
        }

    lua_state lua;
    for (const std::uint32_t seed : seeds)
    {
        std::printf("\n=== road_offer_census seed %u ===\n", static_cast<unsigned>(seed));
        world_params params = arc_params(world_arc::shipped);
        params.seed = seed;
        app_start_world out;
        build_app_start_world(lua, params, out);
        world& w = out.w;

        const entity_id actor = w.player_entity;
        entity_id own_nation = null_entity;
        if (const auto cit = w.corporations.find(actor); cit != w.corporations.end())
            own_nation = cit->second.home_nation;
        check(actor != null_entity, "the world has a player to ask as");

        // The body the player stands on: the one their first asset sits on.
        entity_id body = null_entity;
        if (const auto cit = w.corporations.find(actor); cit != w.corporations.end())
            for (const entity_id bid : cit->second.assets)
                if (const auto bit = w.buildings.find(bid); bit != w.buildings.end())
                    if (const auto tit = w.tiles.find(bit->second.tile); tit != w.tiles.end())
                    { body = tit->second.body; break; }
        check(body != null_entity, "the player holds ground on some body");
        if (body == null_entity)
            continue;

        tier_counts all[3]{}, mine[3]{};
        census(w, body, actor, own_nation, /*own_ground_only=*/false, all);
        census(w, body, actor, own_nation, /*own_ground_only=*/true,  mine);

        std::printf("  player corp %u, home nation %u\n",
                    static_cast<unsigned>(actor), static_cast<unsigned>(own_nation));
        print_rows("whole body", all);
        print_rows("own nation only", mine);

        // The one thing that would make the control unusable rather than tight: no
        // tile anywhere on the player's own ground admits a crossing at any tier.
        const bool any = mine[0].roaded_admits + mine[1].roaded_admits + mine[2].roaded_admits > 0;
        check(any, "the player can build SOME crossing on their own ground");
    }

    std::printf("\n%s\n", g_fail == 0 ? "road_offer_census: OK" : "road_offer_census: FAILURES");
    return g_fail == 0 ? 0 : 1;
}
