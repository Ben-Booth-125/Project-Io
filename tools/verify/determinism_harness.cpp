// Headless determinism harness (BL-106). No SDL / Lua / ImGui.
//
// Generates the single canonical world (make_hard_coded_world) TWICE and asserts
// the two are identical across the deterministic surface: component-store sizes
// and entity-id key sets (bodies, tiles, buildings, markets, nations,
// corporations, population centres, units, stockpiles), the tile->nation and
// population-centre->tile mappings, the (corp,body) stockpile-pool keys, and the
// well-known entities (player / star / home) plus the asteroid belt. A divergence
// in any of these is a determinism regression — a clock/rand leak or an
// unordered_map iteration-order dependence leaking into world generation.
//
// Scope: make_hard_coded_world() takes no seed (world-gen is not seedable today,
// per corp_terrain_matrix), so this guards the canonical world's REPRODUCIBILITY
// across two calls — the property that actually protects determinism now. It
// widens to a true seed sweep unchanged once a seed is threaded through world
// generation.
//
// Exits 0 on PASS, non-zero on any divergence (naming it). Kept outside src/ so
// the CMake game glob does not pull it into the build.

#include "world/world.hpp"
#include "world/hard_coded_world.hpp"

#include <algorithm>
#include <cstdio>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_failures;
}

// Sorted entity-id key vector of any unordered_map<entity_id, V>.
template <class Map>
std::vector<entity_id> sorted_keys(const Map& m)
{
    std::vector<entity_id> k;
    k.reserve(m.size());
    for (const auto& kv : m) k.push_back(kv.first);
    std::sort(k.begin(), k.end());
    return k;
}

// Sorted (key,value) vector of an entity_id -> entity_id map.
std::vector<std::pair<entity_id, entity_id>>
sorted_pairs(const std::unordered_map<entity_id, entity_id>& m)
{
    std::vector<std::pair<entity_id, entity_id>> v;
    v.reserve(m.size());
    for (const auto& kv : m) v.emplace_back(kv.first, kv.second);
    std::sort(v.begin(), v.end());
    return v;
}

} // namespace

int main()
{
    const world a = make_hard_coded_world();
    const world b = make_hard_coded_world();

    // Well-known entities + asteroid belt.
    check(a.player_entity == b.player_entity, "player_entity identical");
    check(a.star_body == b.star_body,         "star_body identical");
    check(a.home_body == b.home_body,         "home_body identical");
    check(a.belt.inner_radius_au == b.belt.inner_radius_au &&
          a.belt.outer_radius_au == b.belt.outer_radius_au, "asteroid belt identical");

    // Component-store sizes.
    check(a.bodies.size()             == b.bodies.size(),             "bodies count identical");
    check(a.tiles.size()              == b.tiles.size(),              "tiles count identical");
    check(a.buildings.size()          == b.buildings.size(),          "buildings count identical");
    check(a.markets.size()            == b.markets.size(),            "markets count identical");
    check(a.nations.size()            == b.nations.size(),            "nations count identical");
    check(a.corporations.size()       == b.corporations.size(),       "corporations count identical");
    check(a.population_centres.size() == b.population_centres.size(), "population-centre count identical");
    check(a.units.size()              == b.units.size(),              "units count identical");
    check(a.stockpiles.size()         == b.stockpiles.size(),         "stockpiles count identical");

    // Entity-id key sets (catches divergent id assignment / placement).
    check(sorted_keys(a.bodies)             == sorted_keys(b.bodies),             "body id set identical");
    check(sorted_keys(a.tiles)              == sorted_keys(b.tiles),              "tile id set identical");
    check(sorted_keys(a.buildings)          == sorted_keys(b.buildings),          "building id set identical");
    check(sorted_keys(a.markets)            == sorted_keys(b.markets),            "market id set identical");
    check(sorted_keys(a.nations)            == sorted_keys(b.nations),            "nation id set identical");
    check(sorted_keys(a.corporations)       == sorted_keys(b.corporations),       "corporation id set identical");
    check(sorted_keys(a.population_centres) == sorted_keys(b.population_centres), "population-centre id set identical");

    // Derived mappings (catches nation-ownership / pop-placement value divergence).
    check(sorted_pairs(a.tile_to_nation)         == sorted_pairs(b.tile_to_nation),         "tile->nation mapping identical");
    check(sorted_pairs(a.population_centre_tile) == sorted_pairs(b.population_centre_tile), "pop-centre->tile mapping identical");

    // (corp,body) stockpile-pool keys.
    {
        std::vector<std::pair<entity_id, entity_id>> ka, kb;
        ka.reserve(a.corp_body_pools.size());
        kb.reserve(b.corp_body_pools.size());
        for (const auto& kv : a.corp_body_pools) ka.push_back(kv.first);
        for (const auto& kv : b.corp_body_pools) kb.push_back(kv.first);
        std::sort(ka.begin(), ka.end());
        std::sort(kb.begin(), kb.end());
        check(ka == kb, "corp-body pool keys identical");
    }

    if (g_failures == 0)
        std::printf("\nDETERMINISM OK - canonical world is stable across two generations.\n");
    else
        std::printf("\nDETERMINISM FAIL - %d divergence(s); world generation is not reproducible.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
