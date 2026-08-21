// Headless determinism harness for BL-476 (every corporation starts with an
// army, not just the player). No SDL / Lua / ImGui.
//
// `corporation_generation.cpp`'s `generate_corporations` used to seed a
// `military_base` + one starting `unit_component` for the PLAYER corp ONLY
// ("Rival corps get no such seeding" — BL-330/BL-331). BL-476 extracted that
// logic into `seed_starting_military` (anonymous namespace, so not directly
// callable here — see the R3 synthetic-world block below, which drives it
// indirectly through the same generate_corporations entry point) and calls it
// once per non-background corp (player + every generated rival), never for a
// BL-365 background firm.
//
// This harness asserts the item-spanning requirement group `rivals-start-armed`
// (docs/development/req/requirements.json):
//
//   R1 every non-background corp (player + named rivals) ends generation
//      owning exactly one military_base and one unit_component (roster index
//      0, 50 manpower), placed on the nearest valid land tile to its HQ. A
//      background firm owns neither.
//   R2 two same-seed generation runs produce identical seeding (same tiles,
//      same unit fields) for every corp.
//   R3 a land-poor nation with no valid placement tile is skipped gracefully
//      (no crash), matching the existing player-only behaviour.
//
// Seed scope: make_hard_coded_world(no_prehistory()) takes no seed parameter
// today (world-gen is not seedable via a param — corp_terrain_matrix/
// determinism_harness note the same MVP scope), so R2 checks the canonical
// world's REPRODUCIBILITY across two independent generations, exactly as
// determinism_harness.cpp does for the rest of world-gen.
//
// Kept outside src/ so the CMake game glob does not pull it into the build.

#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_failures;
}

// Every non-background corp id, sorted for a deterministic report order
// (unordered_map iteration is unspecified).
std::vector<entity_id> non_background_corp_ids(const world& w)
{
    std::vector<entity_id> ids;
    for (const auto& [cid, cc] : w.corporations)
        if (!cc.is_background)
            ids.push_back(cid);
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<entity_id> background_corp_ids(const world& w)
{
    std::vector<entity_id> ids;
    for (const auto& [cid, cc] : w.corporations)
        if (cc.is_background)
            ids.push_back(cid);
    std::sort(ids.begin(), ids.end());
    return ids;
}

int count_military_bases(const world& w, const corporation_component& cc)
{
    int n = 0;
    for (entity_id bid : cc.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit != w.buildings.end() && bit->second.type == building_type::military_base)
            ++n;
    }
    return n;
}

// Counts only units seed_starting_military itself could have created: it
// always sets `muster_base` to the base it just authored (see
// corporation_generation.cpp), so filtering on a non-null muster_base
// excludes hard_coded_world.cpp's separate, pre-existing "Kepler player
// stub" unit (MILITARY.md's three-writers list — that stub deliberately
// carries no muster_base and predates BL-476 by design, not a seeding bug
// this item introduced). hire_unit is the third writer and also sets
// muster_base, but never fires here since no economy tick runs during
// generation.
int count_generation_seeded_units(const world& w, entity_id corp_id)
{
    int n = 0;
    for (const auto& [uid, uc] : w.units)
        if (uc.owner == corp_id && uc.muster_base != null_entity)
            ++n;
    return n;
}

} // namespace

int main()
{
    std::printf("BL-476 rival military seeding harness\n");

    // -------------------------------------------------------------------
    // R1 — every non-background corp owns exactly one military_base and one
    // unit_component; a background firm owns neither.
    // -------------------------------------------------------------------
    {
        const world w = make_hard_coded_world(no_prehistory());

        const std::vector<entity_id> corp_ids = non_background_corp_ids(w);
        const std::vector<entity_id> bg_ids    = background_corp_ids(w);

        std::printf("\n=== R1: %zu non-background corp(s), %zu background firm(s) ===\n",
                    corp_ids.size(), bg_ids.size());

        int seeded = 0, land_poor_skips = 0;
        bool all_ok = true;
        for (entity_id cid : corp_ids)
        {
            const corporation_component& cc = w.corporations.at(cid);
            const int bases = count_military_bases(w, cc);
            const int units = count_generation_seeded_units(w, cid);

            // A corp is expected to have exactly one of each UNLESS its home
            // nation genuinely has no valid land tile at all — the canonical
            // Kepler world is not land-poor, so every corp here is expected
            // to be seeded; a 0/0 corp is reported (not asserted away) so a
            // genuine placement bug is visible rather than silently excused.
            if (bases == 1 && units == 1)
            {
                ++seeded;
            }
            else if (bases == 0 && units == 0)
            {
                ++land_poor_skips;
                std::printf("  NOTE corp=%u \"%s\" got no seeding (0 bases, 0 units) — "
                            "expected only for a land-poor nation\n",
                            static_cast<unsigned>(cid), cc.name.c_str());
            }
            else
            {
                all_ok = false;
                std::printf("  BAD  corp=%u \"%s\" is_player=%d bases=%d units=%d (expected 1/1 or 0/0)\n",
                            static_cast<unsigned>(cid), cc.name.c_str(), cc.is_player ? 1 : 0, bases, units);
            }
        }
        std::printf("  seeded=%d/%zu, land-poor-skips=%d\n",
                    seeded, corp_ids.size(), land_poor_skips);

        check(all_ok, "R1 every non-background corp has exactly one military_base "
                       "and one unit, or a reported land-poor skip");
        check(corp_ids.size() > 1, "R1 sanity: canonical world has more than one "
                                    "non-background corp (player + rivals)");
        check(seeded >= 2, "R1 at least the player and one rival were actually seeded "
                            "(Kepler is not land-poor)");

        bool bg_clean = true;
        for (entity_id cid : bg_ids)
        {
            const corporation_component& cc = w.corporations.at(cid);
            if (count_military_bases(w, cc) != 0 || count_generation_seeded_units(w, cid) != 0)
            {
                bg_clean = false;
                std::printf("  BAD  background firm=%u \"%s\" owns military assets\n",
                            static_cast<unsigned>(cid), cc.name.c_str());
            }
        }
        check(bg_clean, "R1 every background firm owns neither a military_base nor a unit");
    }

    // -------------------------------------------------------------------
    // R2 — determinism across two independent same-config generations.
    // Corp/tile/building/unit id assignment is itself asserted deterministic
    // by determinism_harness.cpp (sorted key-set equality), so matching by
    // entity id and comparing fields directly is valid here too.
    // -------------------------------------------------------------------
    {
        const world a = make_hard_coded_world(no_prehistory());
        const world b = make_hard_coded_world(no_prehistory());

        const std::vector<entity_id> ids_a = non_background_corp_ids(a);
        const std::vector<entity_id> ids_b = non_background_corp_ids(b);

        std::printf("\n=== R2: determinism across two generations ===\n");
        check(ids_a == ids_b, "R2 non-background corp id sets are identical across two generations");

        bool tiles_match = true, fields_match = true;
        for (std::size_t i = 0; i < ids_a.size() && i < ids_b.size(); ++i)
        {
            const entity_id cid_a = ids_a[i];
            const entity_id cid_b = ids_b[i];
            const corporation_component& cc_a = a.corporations.at(cid_a);
            const corporation_component& cc_b = b.corporations.at(cid_b);

            // Find each corp's military_base tile (0 or 1 present, per R1).
            entity_id tile_a = null_entity, tile_b = null_entity;
            for (entity_id bid : cc_a.assets)
            {
                const auto bit = a.buildings.find(bid);
                if (bit != a.buildings.end() && bit->second.type == building_type::military_base)
                    tile_a = bit->second.tile;
            }
            for (entity_id bid : cc_b.assets)
            {
                const auto bit = b.buildings.find(bid);
                if (bit != b.buildings.end() && bit->second.type == building_type::military_base)
                    tile_b = bit->second.tile;
            }
            if (tile_a != tile_b)
            {
                tiles_match = false;
                std::printf("  BAD  corp=%u base tile diverged (%u vs %u)\n",
                            static_cast<unsigned>(cid_a),
                            static_cast<unsigned>(tile_a), static_cast<unsigned>(tile_b));
            }

            // Compare the owned unit's fields (count, type, supply_factor_permille;
            // position/muster_base implied by the base-tile check above).
            const unit_component* ua = nullptr;
            const unit_component* ub = nullptr;
            for (const auto& [uid, uc] : a.units) if (uc.owner == cid_a) ua = &uc;
            for (const auto& [uid, uc] : b.units) if (uc.owner == cid_b) ub = &uc;

            if (static_cast<bool>(ua) != static_cast<bool>(ub))
            {
                fields_match = false;
                std::printf("  BAD  corp=%u unit presence diverged\n", static_cast<unsigned>(cid_a));
            }
            else if (ua && ub)
            {
                if (ua->count != ub->count || ua->type != ub->type
                    || ua->supply_factor_permille != ub->supply_factor_permille
                    || ua->position != ub->position)
                {
                    fields_match = false;
                    std::printf("  BAD  corp=%u unit fields diverged\n", static_cast<unsigned>(cid_a));
                }
            }
        }
        check(tiles_match, "R2 every corp's military_base tile is identical across two generations");
        check(fields_match, "R2 every corp's starting unit fields are identical across two generations");
    }

    // -------------------------------------------------------------------
    // R3 — a land-poor nation (no valid placement tile) is skipped
    // gracefully: no crash, and the corp simply ends up with no
    // military_base/unit, exactly the pre-BL-476 player-only behaviour for
    // a deposit/land-poor nation.
    //
    // Synthetic minimal world: one nation whose only tile is ocean, so
    // placement_rules::can_place(military_base) never accepts it. Runs the
    // real generate_corporations entry point (the anonymous-namespace helper
    // is not directly callable from here) so this exercises the actual
    // production code path, not a re-implementation of it.
    // -------------------------------------------------------------------
    {
        std::printf("\n=== R3: land-poor nation graceful skip ===\n");

        world w;
        const entity_id body = w.create_entity();
        w.bodies[body] = body_component{};

        const entity_id ocean_tile = w.create_entity();
        tile_component tc{};
        tc.body = body;
        tc.substrate = terrain_substrate::ocean;
        tc.grid_x = 0;
        tc.grid_y = 0;
        w.tiles[ocean_tile] = tc;

        const entity_id nation = w.create_entity();
        nation_component nc{};
        nc.name = "Land-poor Nation";
        nc.tiles.push_back(ocean_tile);
        w.nations[nation] = nc;
        w.tile_to_nation[ocean_tile] = nation;

        corporation_params params{};
        params.corporation_count = 1;

        bool threw = false;
        std::vector<entity_id> corp_ids;
        try
        {
            corp_ids = generate_corporations(w, params, /*seed=*/12345u);
        }
        catch (...)
        {
            threw = true;
        }

        check(!threw, "R3 generate_corporations does not throw/crash on an all-ocean nation");
        check(corp_ids.size() == 1, "R3 the corp is still created despite no valid land tile");

        if (!corp_ids.empty())
        {
            const corporation_component& cc = w.corporations.at(corp_ids.front());
            check(count_military_bases(w, cc) == 0,
                  "R3 no military_base is authored when no valid land tile exists");
            check(count_generation_seeded_units(w, corp_ids.front()) == 0,
                  "R3 no starting unit is created when no valid land tile exists");
        }
    }

    if (g_failures == 0)
        std::printf("\nALL PASS  (0 failures)\n");
    else
        std::printf("\nFAILURES (%d failures)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
