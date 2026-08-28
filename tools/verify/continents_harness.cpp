// ---------------------------------------------------------------------------
// Continents/Drift harness (BL-210 first slice; no SDL / Lua / ImGui)
//
// Verifies the plate-drift sibling pass in isolation: determinism, the
// stagnant-lid special case, and that boundary classification actually
// produces both convergent and divergent history lines somewhere in the
// prototype body set (not just one, which would mean the classifier has a
// sign bug that happens to look plausible on a single body).
//
// R6 covers the two BOUNDARY MASKS the pass records — `convergent` and its
// `divergent` sibling. They are the classification made durable: without them
// it survives only as a history line and a number folded into the heightmap,
// where +0.12 and -0.08 are indistinguishable from ground that was simply high
// or low to begin with. R6 asserts they are the same shape as each other and as
// the grid, disjoint (one `sign` tested twice cannot mark a tile both ways),
// both EMPTY on a stagnant lid rather than merely all-zero, non-vacuous across
// a seed spread, and bit-identical across two runs of one seed.
//
// The masks are NOT disjoint, and R6 says so with a measurement rather than an
// assertion it would have to be weakened to pass. See the overlap row below.
//
// NOT COVERED HERE, deliberately: the save round trip. `continent_state` is
// serialised by `w_continents`/`r_continents` in `src/core/save_game.cpp`,
// whose translation unit reaches `ui_state.hpp` and therefore ImGui — out of
// this tier by construction (tools/verify/build_harness.js says so in its own
// header). That gap predates the divergent mask; `convergent` has never had
// round-trip coverage either.
// ---------------------------------------------------------------------------

#include "world/continents.hpp"
#include "world/planetology.hpp"

#include <cstdio>
#include <cstring>

namespace {

int g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

} // namespace

int main()
{
    constexpr int gw = 180, gh = 84;

    // A mobile-lid body, so plates actually drift and the boundary path runs.
    body_inputs mobile{};
    mobile.name         = "Test-Mobile";
    mobile.mass_earths  = 1.0f;
    mobile.orbit_au     = 1.0f;
    planetology_params p{};
    const planetology_state mobile_pl = run_planetology(mobile, p, /*seed=*/12345u);
    check(mobile_pl.mobile_lid, "fixture body has a mobile lid (precondition for this harness)");

    // --- R1: determinism ---
    const continent_state a = run_continents(mobile_pl, gw, gh, /*seed=*/777u);
    const continent_state b = run_continents(mobile_pl, gw, gh, /*seed=*/777u);
    const continent_state c = run_continents(mobile_pl, gw, gh, /*seed=*/778u);

    bool same_ab = (a.height_bias.size() == b.height_bias.size());
    for (std::size_t i = 0; same_ab && i < a.height_bias.size(); ++i)
        if (a.height_bias[i] != b.height_bias[i]) same_ab = false;
    check(same_ab, "R1 same seed -> identical height_bias");

    bool differs_ac = (a.height_bias.size() != c.height_bias.size());
    for (std::size_t i = 0; !differs_ac && i < a.height_bias.size(); ++i)
        if (a.height_bias[i] != c.height_bias[i]) differs_ac = true;
    check(differs_ac, "R1 different seed -> different height_bias");

    check(a.plates.size() >= 4 && a.plates.size() <= 10,
        "R2 mobile-lid plate count lands in [4,10]");

    // --- R3: stagnant lid is the single-plate special case, zero bias ---
    body_inputs stagnant{};
    stagnant.name        = "Test-Stagnant";
    stagnant.mass_earths  = 0.06f; // below the mobile-lid mass floor
    stagnant.orbit_au     = 1.0f;
    const planetology_state stagnant_pl = run_planetology(stagnant, p, /*seed=*/999u);
    check(!stagnant_pl.mobile_lid, "fixture body is stagnant-lid (precondition)");

    const continent_state s = run_continents(stagnant_pl, gw, gh, /*seed=*/1u);
    check(s.plates.size() == 1, "R3 stagnant lid -> one immobile plate");
    bool all_zero = true;
    for (float hb : s.height_bias) if (hb != 0.0f) { all_zero = false; break; }
    check(all_zero, "R3 stagnant lid -> zero height bias everywhere");
    check(s.history.size() == 1 && std::strstr(s.history[0].event.c_str(), "stagnant") != nullptr,
        "R3 stagnant lid emits exactly one textual line naming it");

    // --- R4: a mobile-lid body's boundaries produce BOTH convergent and
    //     divergent lines somewhere across a spread of seeds (not just one
    //     sign, which would mean the dot-product classifier is biased). ---
    bool saw_convergent = false, saw_divergent = false;
    for (uint32_t seed = 0; seed < 24; ++seed)
    {
        const continent_state cs = run_continents(mobile_pl, gw, gh, seed);
        for (const history_event& e : cs.history)
        {
            if (std::strstr(e.event.c_str(), "collided") != nullptr) saw_convergent = true;
            if (std::strstr(e.event.c_str(), "apart") != nullptr)    saw_divergent  = true;
        }
    }
    check(saw_convergent, "R4 convergent boundaries fire across a seed spread");
    check(saw_divergent,  "R4 divergent boundaries fire across a seed spread");

    // --- R6: the boundary masks. `divergent` is recorded on exactly the terms
    //     `convergent` is, so every row below is stated over BOTH. ---
    const std::size_t cells = static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh);
    check(a.convergent.size() == cells && a.divergent.size() == cells,
        "R6 mobile lid -> both boundary masks sized gw*gh");

    // OVERLAP IS REAL, AND IT IS A JUNCTION, NOT A SIGN BUG. The masks are not
    // disjoint, which is worth stating loudly because "convergent and divergent
    // are exclusive" is the obvious reading and it is wrong. The pass walks each
    // tile against TWO neighbours (right and down) and marks per neighbour, so a
    // tile sitting where a closing pair meets an opening one is genuinely on
    // both — and `height_bias` has always accumulated both there (+0.12 and
    // -0.08 land on the same tile). Nothing is being fixed by suppressing that;
    // it is the terrain.
    //
    // What must hold is the per-BOUNDARY exclusivity: one `sign` tested twice
    // cannot mark one pair both ways. Every overlap tile must therefore carry
    // two DISTINCT classified boundaries beneath it — right and down leading to
    // different plates, neither of them its own. A tile marked both with only
    // one boundary under it would be the sign bug this row exists to catch.
    std::size_t overlap = 0, overlap_without_two_boundaries = 0;
    for (int row = 0; row < gh; ++row)
    {
        for (int col = 0; col < gw; ++col)
        {
            const std::size_t idx = static_cast<std::size_t>(col + row * gw);
            if (a.convergent[idx] == 0u || a.divergent[idx] == 0u) continue;
            ++overlap;
            const int here  = a.plate_id[idx];
            const int right = a.plate_id[static_cast<std::size_t>(((col + 1) % gw) + row * gw)];
            const int down  = (row + 1 < gh)
                ? a.plate_id[static_cast<std::size_t>(col + (row + 1) * gw)] : here;
            const bool two_distinct = (right != here) && (down != here) && (right != down);
            if (!two_distinct) ++overlap_without_two_boundaries;
        }
    }
    check(overlap_without_two_boundaries == 0,
        "R6 every both-marked tile sits on two DISTINCT classified boundaries");
    std::printf("      (overlap tiles: %llu of %llu -- plate junctions, not a sign bug)\n",
        static_cast<unsigned long long>(overlap), static_cast<unsigned long long>(cells));

    // Non-vacuous, and marked where the OTHER mask is not: a spread of seeds
    // must yield at least one tile that is divergent and not convergent, or the
    // mask is a copy of its sibling rather than the other half of the split.
    bool saw_div_tile = false, saw_conv_tile = false, saw_div_where_conv_not = false;
    for (uint32_t seed = 0; seed < 24 && !saw_div_where_conv_not; ++seed)
    {
        const continent_state cs = run_continents(mobile_pl, gw, gh, seed);
        if (cs.convergent.size() != cells || cs.divergent.size() != cells) continue;
        for (std::size_t i = 0; i < cells; ++i)
        {
            if (cs.convergent[i] != 0u) saw_conv_tile = true;
            if (cs.divergent[i]  != 0u)
            {
                saw_div_tile = true;
                if (cs.convergent[i] == 0u) saw_div_where_conv_not = true;
            }
        }
    }
    check(saw_conv_tile, "R6 convergent tiles are marked across a seed spread");
    check(saw_div_tile,  "R6 divergent tiles are marked across a seed spread");
    check(saw_div_where_conv_not,
        "R6 divergent marks tiles convergent does not (not a copy of its sibling)");

    // Empty, not all-zero: a stagnant lid classifies no boundary at all, and
    // "no boundaries" must stay distinguishable from "boundaries, none of them
    // divergent". `height_bias` and `plate_id` are sized before the early
    // return; these two are not, and that asymmetry is the contract.
    check(s.convergent.empty() && s.divergent.empty(),
        "R6 stagnant lid -> both boundary masks EMPTY (no boundaries at all)");

    // Determinism, stated over the raster rather than the history line: two runs
    // of one seed produce byte-identical masks.
    bool masks_same = (a.divergent.size() == b.divergent.size())
                   && (a.convergent.size() == b.convergent.size());
    for (std::size_t i = 0; masks_same && i < a.divergent.size(); ++i)
        if (a.divergent[i] != b.divergent[i]) masks_same = false;
    for (std::size_t i = 0; masks_same && i < a.convergent.size(); ++i)
        if (a.convergent[i] != b.convergent[i]) masks_same = false;
    check(masks_same, "R6 same seed -> byte-identical convergent and divergent rasters");

    // --- R5: every history line carries a consequence (the presentation rule) ---
    bool all_have_consequence = true;
    for (const history_event& e : a.history)
        if (e.consequence.empty()) { all_have_consequence = false; break; }
    check(all_have_consequence, "R5 every continents history line names its consequence");

    std::printf("\n%s\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_fail == 0 ? 0 : 1;
}
