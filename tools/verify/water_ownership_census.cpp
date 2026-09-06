// Headless harness for the WATER OWNERSHIP claim (BL-776 coastal water owned,
// BL-777 lakes owned).
//
// THE GAP THIS FILLS. BL-776/BL-777 made coastal water and lakes part of a
// nation's territory and left the open ocean outside it. That was measured once,
// with an ad-hoc probe that was then deleted, so from that day nothing in the
// repo asserted it. An unasserted generation claim becomes UNFALSIFIABLE the
// moment the 0 CE digests are re-blessed — the old world stops being
// reproducible and there is nothing left to compare against. This lands before
// that re-bless so the claim survives it.
//
// The claim, stated as this harness checks it, over a seed sweep:
//   O1  coastal water tiles carry a nation      (owned coast > 0)
//   O2  lake tiles carry a nation               (owned lake  > 0)
//   O3  open ocean tiles carry NO nation        (owned ocean == 0, every seed)
//   V   NON-VACUITY: a non-zero number of tiles of EACH kind was examined.
//
// V is not boilerplate. This project has been bitten by a harness that passed
// vacuously against an empty registry (see CMakeLists' IO_TEST_WORKDIR_HARNESSES
// comments), and a guard against examining nothing is house style now: O1-O3 are
// all satisfied trivially by a world with no water in it, and O3 in particular is
// a NEGATIVE claim that an empty census proves forever.
//
// Deliberately NO hard-coded expected counts. The reference figures from the
// deleted probe (coast 2177 owned, lake 1312 owned, ocean 0 owned against 15336
// unowned) are world CONTENT and the world moves; pinning them here would make
// this a golden rather than a check of the claim. The counts are REPORTED so a
// reader can see the shape, and only the claim itself is asserted.
//
// Ownership is read straight from `world::tile_to_nation`, the territorial map,
// and the substrate straight from `tile_component::substrate` — neither through
// a helper the generation code also uses, so the check does not ask the code
// under test whether it is right.
//
// Registered as the `water_ownership_census` CMake target (verifier-headless):
//   node tools/verify/build_harness.js water_ownership_census --run
//   cmake --build build --target water_ownership_census

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <cstddef>
#include <string>
#include <vector>

// Deliberately NOT including harness_params.hpp: its `no_prehistory()` helper
// drags in scripting/lua_state.hpp and so <sol/sol.hpp>, which the Lua-free
// headless builder (tools/verify/build_harness.js) cannot see. The one line it
// would have contributed is restated here instead.
static world_params no_era(world_params p = {})
{
    p.prehistory_years = 0;   // the Era -1 sim is not this harness's subject
    return p;
}

static int g_failures = 0;

static void check(bool ok, const std::string& label)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", label.c_str());
    if (!ok) ++g_failures;
}

/// The three water kinds this harness distinguishes, re-derived from the
/// substrate enum rather than read out of placement_rules' helpers.
enum class water_kind
{
    coast = 0,
    lake  = 1,
    ocean = 2,
    none  = 3
};

static water_kind classify(terrain_substrate s)
{
    switch (s)
    {
        case terrain_substrate::coast: return water_kind::coast;
        case terrain_substrate::lake:  return water_kind::lake;
        case terrain_substrate::ocean: return water_kind::ocean;
        default:                       return water_kind::none;
    }
}

static const char* kind_name(water_kind k)
{
    switch (k)
    {
        case water_kind::coast: return "coastal water";
        case water_kind::lake:  return "lake";
        case water_kind::ocean: return "open ocean";
        case water_kind::none:  return "land";
    }
    return "?";
}

struct census
{
    std::size_t seen[4]  = { 0, 0, 0, 0 };
    std::size_t owned[4] = { 0, 0, 0, 0 };
};

static census take_census(const world& w)
{
    census c;
    for (const auto& [tid, tile] : w.tiles)
    {
        const std::size_t k = static_cast<std::size_t>(classify(tile.substrate));
        ++c.seen[k];
        if (w.tile_to_nation.find(tid) != w.tile_to_nation.end())
            ++c.owned[k];
    }
    return c;
}

int main()
{
    std::printf("Water ownership census (BL-776 coastal water owned, BL-777 lakes owned)\n\n");

    // Four seeds. The Era -1 sim is not under test here — territory is placed by
    // nation generation, which runs either way — so `no_prehistory()` applies,
    // and four worlds cost what one prehistory world would.
    constexpr int k_seeds = 4;

    census                totals;
    std::vector<std::size_t> ocean_owned_by_seed;

    std::printf("Per-seed census (owned / seen)\n");
    std::printf("  seed |        coast |         lake |    open ocean |          land\n");
    std::printf("  -----+--------------+--------------+---------------+--------------\n");

    for (int sd = 0; sd < k_seeds; ++sd)
    {
        world_params wp{};
        wp.seed = static_cast<uint32_t>(sd);
        world w  = make_hard_coded_world(no_era(wp));

        const census c = take_census(w);
        ocean_owned_by_seed.push_back(c.owned[static_cast<std::size_t>(water_kind::ocean)]);

        std::printf("  %4d | %6zu/%5zu | %6zu/%5zu | %6zu/%6zu | %6zu/%5zu\n",
                    sd,
                    c.owned[0], c.seen[0],
                    c.owned[1], c.seen[1],
                    c.owned[2], c.seen[2],
                    c.owned[3], c.seen[3]);

        for (std::size_t k = 0; k < 4; ++k)
        {
            totals.seen[k]  += c.seen[k];
            totals.owned[k] += c.owned[k];
        }
    }

    std::printf("\nSweep totals over %d seeds\n", k_seeds);
    for (std::size_t k = 0; k < 4; ++k)
        std::printf("  %-14s seen %7zu   owned %7zu   unowned %7zu\n",
                    kind_name(static_cast<water_kind>(k)),
                    totals.seen[k], totals.owned[k], totals.seen[k] - totals.owned[k]);

    // --- V first: NON-VACUITY -------------------------------------------
    // Asserted BEFORE the claims it protects, because a reader who sees O1-O3
    // green needs to know in the same breath that they were green about
    // something. An empty census satisfies all three.
    std::printf("\nV — non-vacuity: the census examined tiles of every kind\n");
    check(totals.seen[static_cast<std::size_t>(water_kind::coast)] > 0,
          "V1  coastal water tiles were examined (non-zero)");
    check(totals.seen[static_cast<std::size_t>(water_kind::lake)] > 0,
          "V2  lake tiles were examined (non-zero)");
    check(totals.seen[static_cast<std::size_t>(water_kind::ocean)] > 0,
          "V3  open ocean tiles were examined (non-zero)");
    check(totals.seen[static_cast<std::size_t>(water_kind::none)] > 0,
          "V4  land tiles were examined (non-zero) — the census walked a real world");

    // --- O1/O2 — water inside the territorial map --------------------------
    // Stated as "some coastal water is owned", not "all of it": an uninhabited
    // body has coast and no nation to own it, so a total claim would be a claim
    // about how many bodies carry nations, which is a different subject.
    std::printf("\nO1/O2 — coastal water and lakes carry a nation\n");
    check(totals.owned[static_cast<std::size_t>(water_kind::coast)] > 0,
          "O1  coastal water tiles carry a nation (BL-776)");
    check(totals.owned[static_cast<std::size_t>(water_kind::lake)] > 0,
          "O2  lake tiles carry a nation (BL-777)");

    // --- O3 — the open ocean is outside every territory --------------------
    // The strict half of the claim, and the one a re-bless would otherwise put
    // out of reach: ZERO cases, on EVERY seed, not merely in aggregate.
    std::printf("\nO3 — the open ocean carries no nation, in zero cases\n");
    {
        bool any = false;
        for (int sd = 0; sd < k_seeds; ++sd)
            if (ocean_owned_by_seed[static_cast<std::size_t>(sd)] > 0)
            {
                any = true;
                std::printf("        seed %d owns %zu open-ocean tiles\n",
                            sd, ocean_owned_by_seed[static_cast<std::size_t>(sd)]);
            }
        check(!any, "O3  no open-ocean tile carries a nation on any seed");
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL CHECKS PASSED" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
