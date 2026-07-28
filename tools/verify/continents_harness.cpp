// ---------------------------------------------------------------------------
// Continents/Drift harness (BL-210 first slice; no SDL / Lua / ImGui)
//
// Verifies the plate-drift sibling pass in isolation: determinism, the
// stagnant-lid special case, and that boundary classification actually
// produces both convergent and divergent history lines somewhere in the
// prototype body set (not just one, which would mean the classifier has a
// sign bug that happens to look plausible on a single body).
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

    // --- R5: every history line carries a consequence (the presentation rule) ---
    bool all_have_consequence = true;
    for (const history_event& e : a.history)
        if (e.consequence.empty()) { all_have_consequence = false; break; }
    check(all_have_consequence, "R5 every continents history line names its consequence");

    std::printf("\n%s\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_fail == 0 ? 0 : 1;
}
