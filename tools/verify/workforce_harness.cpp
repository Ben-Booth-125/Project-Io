// Headless harness for population-legibility (BL-069) — no SDL / Lua / ImGui.
// The only place BL-069 touches shipped simulation code is the extraction of the
// habitability→workforce-efficiency curve into world/workforce.hpp. This harness
// is the regression guard: it asserts workforce_efficiency reproduces the prior
// inline economy_system curve EXACTLY across the habitability range, so the
// refactor is provably behaviour-preserving and the re-keyed Population lens reads
// the same multiplier the sim applies.
//
// Build (from repo root):
//   g++ -std=c++20 -I src tools/verify/workforce_harness.cpp -o workforce_harness
//   ./workforce_harness

#include "world/workforce.hpp"

#include <cmath>
#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char* what, double got = 0.0, double want = 0.0)
{
    if (cond) std::printf("  PASS  %s\n", what);
    else { std::printf("  FAIL  %s   (got %.6f, want %.6f)\n", what, got, want); ++g_failures; }
}

// The exact inline expression economy_system.cpp carried before BL-069 extracted
// it. The helper must equal this for every input — bit-for-bit on the same ops.
static float prior_inline_curve(float hab)
{
    return (hab >= 0.6f) ? 1.0f : (0.5f + (hab / 0.6f) * 0.5f);
}

int main()
{
    std::printf("Workforce-efficiency curve harness (BL-069)\n");

    // R: identical to the prior inline curve across [0, 1] at fine resolution.
    std::printf("\nR — workforce_efficiency == prior inline curve across [0,1]\n");
    bool all_equal = true;
    for (int i = 0; i <= 1000; ++i)
    {
        const float h    = static_cast<float>(i) / 1000.0f;
        const float got  = workforce_efficiency(h);
        const float want = prior_inline_curve(h);
        if (got != want)
        {
            all_equal = false;
            check(false, "exact match at sample", got, want);
            std::printf("        (habitability = %.4f)\n", static_cast<double>(h));
            break;
        }
    }
    if (all_equal)
        check(true, "1001 samples bit-identical to the prior inline curve");

    // Spot anchors: the cliff and the floor/ceiling the design names.
    std::printf("\nR — named anchors (cliff at 0.6, floor 0.5x, ceiling 1.0x)\n");
    check(workforce_efficiency(0.0f) == 0.5f, "efficiency(0.0) == 0.5x", workforce_efficiency(0.0f), 0.5);
    check(workforce_efficiency(0.3f) == 0.75f, "efficiency(0.3) == 0.75x", workforce_efficiency(0.3f), 0.75);
    check(std::fabs(workforce_efficiency(0.59f) - (0.5f + (0.59f / 0.6f) * 0.5f)) < 1e-6f,
          "efficiency(0.59) just below the cliff");
    check(workforce_efficiency(0.6f) == 1.0f, "efficiency(0.6) == 1.0x (cliff)", workforce_efficiency(0.6f), 1.0);
    check(workforce_efficiency(1.0f) == 1.0f, "efficiency(1.0) == 1.0x", workforce_efficiency(1.0f), 1.0);

    std::printf("\n%s  (%d failure%s)\n", g_failures ? "FAIL" : "PASS",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
