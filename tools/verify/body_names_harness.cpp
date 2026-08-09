// ---------------------------------------------------------------------------
// body_names_harness — BL-257 (generated body names).
//
// Coins the system catalogue across a spread of seeds and asserts the four
// properties that make it a system rather than a shuffle:
//
//   B1  DETERMINISM. The same seed coins the same catalogue, every run.
//   B2  UNIQUENESS. No two bodies in one system share a name, moons included.
//   B3  VARIETY. Different seeds do not converge on the same catalogue.
//   B4  THE REGISTER RULE holds structurally — a moon carries its parent's
//       name, an asteroid carries a catalogue index, and nothing is empty.
//
// It also PRINTS each catalogue, because a name generator is judged by reading
// its output, not by a pass count.
//
// Headless: world/* logic only, no SDL and no Lua.
// ---------------------------------------------------------------------------

#include "world/body_names.hpp"
#include "world/planetology.hpp"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

/// True when @p name begins with @p prefix — the moon rule's structural test
/// is the other way round (parent LAST), so this is used on the tail.
bool ends_with(const std::string& name, const std::string& tail)
{
    return name.size() >= tail.size()
        && name.compare(name.size() - tail.size(), tail.size(), tail) == 0;
}

} // namespace

int main()
{
    std::printf("=== body names (BL-257) ===\n\n");

    const uint32_t seeds[] = { 0u, 1u, 7u, 42u, 1234u, 90210u, 0xC0FFEEu, 0xFFFFFFFFu };
    const int n = prototype_body_count();

    std::vector<std::string> catalogues; // one flattened catalogue per seed, for B3.

    bool unique_ok = true, stable_ok = true, structure_ok = true;

    for (const uint32_t s : seeds)
    {
        const body_naming a = generate_body_names(s);
        const body_naming b = generate_body_names(s);

        std::printf("  seed %-10u star %s\n", s, a.star.c_str());

        std::string flat = a.star;
        std::set<std::string> seen{ a.star };
        int home_index = -1;
        for (int i = 0; i < n; ++i)
            if (prototype_body(i).is_homeworld) home_index = i;

        for (int i = 0; i < n; ++i)
        {
            const body_inputs& in   = prototype_body(i);
            const std::string& name = a.bodies[static_cast<std::size_t>(i)];
            const char* role = in.is_homeworld               ? "home"
                             : (in.parent_mass_earths > 0.0f) ? "moon"
                             : in.core_fragment               ? "asteroid"
                                                              : "planet";
            std::printf("             %-10s %s\n", role, name.c_str());

            if (name.empty()) structure_ok = false;
            if (!seen.insert(name).second) unique_ok = false;
            flat += "|" + name;

            // B4: a moon's name ends with its parent's, so the relationship is
            // readable; an asteroid carries a catalogue index.
            if (in.parent_mass_earths > 0.0f && home_index >= 0
                && !ends_with(name, a.bodies[static_cast<std::size_t>(home_index)]))
                structure_ok = false;
            if (in.core_fragment && name.find_last_of("0123456789") != name.size() - 1)
                structure_ok = false;
        }

        if (b.star != a.star) stable_ok = false;
        for (int i = 0; i < n; ++i)
            if (b.bodies[static_cast<std::size_t>(i)] != a.bodies[static_cast<std::size_t>(i)])
                stable_ok = false;

        catalogues.push_back(flat);
        std::printf("\n");
    }

    check(stable_ok,    "B1 the same seed coins the same catalogue, run to run");
    check(unique_ok,    "B2 no two bodies in one system share a name");

    std::set<std::string> distinct(catalogues.begin(), catalogues.end());
    check(distinct.size() == catalogues.size(),
          "B3 different seeds coin different catalogues");

    check(structure_ok,
          "B4 the register rule holds - moons carry their parent, asteroids an index");

    std::printf("\n%d passed, %d FAILED\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
