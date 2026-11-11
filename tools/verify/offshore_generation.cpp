// offshore_generation.cpp — BL-1522 (offshore drilling), cold review fix 1. A GATE.
//
// Generation never places a building on water (TILES.md § Water kinds; the Oil Platform
// is admitted by the build door alone - placement_rules::can_place_site). Shelf sea now
// carries petroleum, and every generation placer asks the bare `can_place`, which keeps
// refusing water; this harness proves it on the shipped start. On each curated seed it
// builds the app's start (build_app_start_world: generation, the fold to ground, the
// charters and the opening kit) and counts the buildings standing on a water tile; then
// runs the app's validation settle (run_app_validation_settle) and counts again.
//
//   W1  after the start (fold + charters + kit): no building stands on water.
//   W2  after the settle: no building stands on water save a LEGAL Oil Platform - an
//       extraction site working petroleum on shelf sea, owned by a nation that holds
//       Offshore Drilling (a rival's scored build through the build door). Reported.
//
//   build_gen/verify/offshore_generation.exe [--seeds a,b,c] [--no-settle]
//
// Build: bash tools/verify/build_lua_harness.sh offshore_generation. Run from the repo root.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/components.hpp"
#include "world/nation_tech.hpp"
#include "world/placement_rules.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

struct wet_count
{
    int buildings = 0; ///< every building on the world
    int on_water  = 0; ///< standing on any water tile
    int legal_rig = 0; ///< of those, an Oil Platform its owner may legally stand there
};

wet_count count_wet(const world& w)
{
    wet_count c;
    for (const auto& [bid, b] : w.buildings)
    {
        (void)bid;
        ++c.buildings;
        const auto tit = w.tiles.find(b.tile);
        if (tit == w.tiles.end() || !is_water(tit->second.substrate))
            continue;
        ++c.on_water;
        const auto own = w.tile_to_nation.find(b.tile);
        if (placement_rules::is_offshore_rig(tit->second.substrate, b.type, b.target_resource)
            && own != w.tile_to_nation.end()
            && nation_mechanic(w, own->second, placement_rules::k_offshore_mechanic))
            ++c.legal_rig;
        else
            std::printf("    building %llu type %d on water tile %llu\n",
                        static_cast<unsigned long long>(bid),
                        static_cast<int>(b.type), static_cast<unsigned long long>(b.tile));
    }
    return c;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    bool settle = true;
    for (int a = 1; a < argc; ++a)
    {
        const std::string s = argv[a];
        if (s == "--seeds" && a + 1 < argc)
        {
            std::string list = argv[++a], t;
            for (std::size_t i = 0; i <= list.size(); ++i)
            {
                if (i == list.size() || list[i] == ',')
                {
                    if (!t.empty()) seeds.push_back(static_cast<uint32_t>(std::strtoul(t.c_str(), nullptr, 10)));
                    t.clear();
                }
                else t += list[i];
            }
        }
        else if (s == "--no-settle") settle = false;
        else { std::printf("usage: offshore_generation [--seeds a,b,c] [--no-settle]\n"); return 2; }
    }
    // The sixteen curated seeds (docs/generation/seed_library.json), offshore_drilling's set.
    if (seeds.empty()) seeds = { 46, 28, 11, 31, 40, 12, 37, 13, 41, 43, 32, 10, 25, 38, 9, 0 };

    std::printf("offshore_generation - BL-1522: no generated building on water (the shipped start)\n");
    lua_state lua;
    int start_wet = 0, settle_bad = 0, settle_rigs = 0, shelf_oil_owned = 0;
    for (const uint32_t seed : seeds)
    {
        world_params params{};
        params.seed = seed;
        app_start_world out;
        build_app_start_world(lua, params, out);
        world& w = out.w;
        int owned_oil = 0;
        for (const auto& [tid, tc] : w.tiles)
            if (is_coastal_water(tc.substrate)
                && tc.resource_deposit[static_cast<std::size_t>(resource_type::petroleum)] > 0.0f
                && w.tile_to_nation.count(tid) != 0)
                ++owned_oil;
        shelf_oil_owned += owned_oil;
        const wet_count s = count_wet(w);
        start_wet += s.on_water;
        int after_wet = -1, after_rig = -1, after_n = -1;
        if (settle)
        {
            run_app_validation_settle(w, out.reg);
            const wet_count t = count_wet(w);
            after_wet = t.on_water; after_rig = t.legal_rig; after_n = t.buildings;
            settle_bad  += t.on_water - t.legal_rig;
            settle_rigs += t.legal_rig;
        }
        std::printf("[offshore_generation] seed %u: owned shelf oil tiles %d; start %d buildings, "
                    "%d on water; settle %d buildings, %d on water (%d legal rigs)\n",
                    seed, owned_oil, s.buildings, s.on_water, after_n, after_wet, after_rig);
        std::fflush(stdout);
    }
    std::printf("total: owned shelf oil tiles %d; start on-water %d; settle illegal on-water %d, "
                "legal rigs %d\n", shelf_oil_owned, start_wet, settle_bad, settle_rigs);
    check(shelf_oil_owned > 0, "W0 the start's nations own shelf oil (the fix is exercised)");
    check(start_wet == 0, "W1 the shipped start (fold, charters, kit) places no building on water");
    if (settle)
        check(settle_bad == 0, "W2 after the settle, only a legal Oil Platform stands on water");
    std::printf("\n%s: %d failure(s)\n", g_fail == 0 ? "PASS" : "FAIL", g_fail);
    return g_fail == 0 ? 0 : 1;
}
