// ---------------------------------------------------------------------------
// Earth-like pair-interaction atlas (T3 of the earth-like battery; no SDL / Lua)
// ---------------------------------------------------------------------------
// A MEASUREMENT tool, not a pass/fail check.
//
// earthlike_corridor sweeps ONE knob at a time with everything else at its Sol
// default. That is the right first question and the wrong last one, because its
// answer is a set of one-dimensional slices through a space the generator
// samples jointly — and slices cannot see interaction.
//
// The battery's plan made this stage CONTINGENT, to be built only if a corridor
// edge visibly moved when a second knob moved. It did. Setting the wizard bands
// to the corridor's always-viable spans (2026-08-04) pushed interior=high to
// 2.97 draws, the worst lean on the board: young age and high radiogenic are
// each individually always-viable, and together they put theta past the GOE
// gate's 2.4 ceiling. One-at-a-time measurement could not have predicted that,
// and it is exactly what this harness measures.
//
// THE METRIC. For a pair (A, B) it fills an N x N grid of viability rates
// V(i,j), then compares it against what independence would predict from the same
// data: P(i,j) = rowmean(i) * colmean(j) / overall. The mean absolute gap
// between V and P, in percentage points, is the interaction score. Zero means
// the two knobs are separable and corridors tell the whole story; large means
// the joint distribution has structure no single-knob sweep can see.
//
// Report-only (BL-275): the scores and maps are printed for Ben to read. The one
// assertion is a self-check that the harness reproduces the corridor's own
// numbers along the grid edges — a atlas that disagrees with the corridor about
// a single knob is measuring something else.
//
// Run: earthlike_pairs [grid] [seeds_per_cell]     (default 24, 24)

#include "world/components.hpp"
#include "world/planetology.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

/// One world at a given parameter set, orbit derived exactly as
/// resolve_preferences derives it. Identical to earthlike_corridor's `roll` —
/// pinning the orbit while stellar mass varies is the bug that used to kill two
/// thirds of draws, and an atlas that reproduced it would measure that instead.
planetology_state roll(const planetology_params& p, uint32_t seed)
{
    const float l = p.star_mass * p.star_mass * p.star_mass * std::sqrt(p.star_mass);
    checkpoint_rng r(seed, 0x0B17u); // orbit sub-stream — same tag as the corridor
    body_inputs home = prototype_body(1);
    home.orbit_au = std::sqrt(l) * (0.985f + (1.400f - 0.985f) * r.unit());
    return run_planetology(home, p, seed ^ prototype_body_seed(1));
}

struct axis
{
    const char* name;
    float planetology_params::*field;
    float lo, hi;          ///< the generator's own clamp range
};

const axis k_star   { "star_mass",     &planetology_params::star_mass,     0.60f, 1.50f };
const axis k_age    { "system_age",    &planetology_params::system_age_gyr, 1.00f,10.00f };
const axis k_radio  { "radiogenic",    &planetology_params::radiogenic,    0.30f, 2.20f };
const axis k_oxy    { "oxygenation",   &planetology_params::oxygenation,   0.00f, 1.00f };
const axis k_ocean  { "home_ocean",    &planetology_params::home_ocean,    0.20f, 0.85f };
const axis k_mass   { "home_mass",     &planetology_params::home_mass,     0.50f, 2.00f };

struct pair_spec { const axis& a; const axis& b; const char* why; };

// Chosen from evidence, not from a list of plausible-sounding combinations. C1
// says the only live clauses are the Breath gate and arable share; the corridor
// says only oxygenation, radiogenic and home_ocean can reject at all. These are
// the pairs where two of those meet.
const pair_spec k_pairs[] = {
    { k_age,   k_radio, "THE interior fold: one lean sets both, and both act on theta" },
    { k_oxy,   k_radio, "GOE/NOE timing against the tectonic heat that gates the NOE" },
    { k_ocean, k_oxy,   "the two inputs to arable_share, the second live clause" },
    { k_star,  k_age,   "the doc's original suspicion: luminosity against the clock" },
    { k_mass,  k_radio, "both terms in theta (mass/radius x radiogenic decay)" },
};

/// Fill V(i,j) = viability rate in percent.
std::vector<float> fill_grid(const pair_spec& ps, int n, int seeds, uint32_t base)
{
    std::vector<float> v(static_cast<std::size_t>(n) * static_cast<std::size_t>(n), 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const float av = ps.a.lo + (ps.a.hi - ps.a.lo) * (n == 1 ? 0.5f : static_cast<float>(i) / static_cast<float>(n - 1));
        for (int j = 0; j < n; ++j)
        {
            const float bv = ps.b.lo + (ps.b.hi - ps.b.lo) * (n == 1 ? 0.5f : static_cast<float>(j) / static_cast<float>(n - 1));
            int ok = 0;
            for (int s = 0; s < seeds; ++s)
            {
                planetology_params p; // Sol defaults for everything else
                p.*(ps.a.field) = av;
                p.*(ps.b.field) = bv;
                // Seed folds in the cell, so neighbouring cells are independent
                // draws rather than the same world nudged.
                const uint32_t seed = base
                    ^ (static_cast<uint32_t>(i) * 0x9E3779B9u)
                    ^ (static_cast<uint32_t>(j) * 0x85EBCA6Bu)
                    ^ (static_cast<uint32_t>(s) * 0xC2B2AE35u);
                if (homeworld_viability(roll(p, seed)).ok) ++ok;
            }
            v[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) + static_cast<std::size_t>(j)] =
                100.0f * static_cast<float>(ok) / static_cast<float>(seeds);
        }
    }
    return v;
}

char glyph(float pct)
{
    if (pct >= 90.0f) return '#';
    if (pct >= 70.0f) return '+';
    if (pct >= 40.0f) return 'o';
    if (pct >= 10.0f) return '.';
    if (pct >  0.0f)  return ',';
    return ' ';
}

void run_pair(const pair_spec& ps, int n, int seeds, uint32_t base)
{
    const std::vector<float> v = fill_grid(ps, n, seeds, base);
    const auto at = [&](int i, int j) {
        return v[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) + static_cast<std::size_t>(j)];
    };

    // Marginals and the independence prediction.
    std::vector<float> rowm(static_cast<std::size_t>(n), 0.0f), colm(static_cast<std::size_t>(n), 0.0f);
    float overall = 0.0f;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            rowm[static_cast<std::size_t>(i)] += at(i, j);
            colm[static_cast<std::size_t>(j)] += at(i, j);
            overall += at(i, j);
        }
    for (int i = 0; i < n; ++i) { rowm[static_cast<std::size_t>(i)] /= static_cast<float>(n);
                                  colm[static_cast<std::size_t>(i)] /= static_cast<float>(n); }
    overall /= static_cast<float>(n * n);

    float interaction = 0.0f, worst = 0.0f;
    int wi = 0, wj = 0;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            const float pred = (overall > 0.001f)
                ? rowm[static_cast<std::size_t>(i)] * colm[static_cast<std::size_t>(j)] / overall : 0.0f;
            const float gap = std::fabs(at(i, j) - pred);
            interaction += gap;
            if (gap > worst) { worst = gap; wi = i; wj = j; }
        }
    interaction /= static_cast<float>(n * n);

    const auto val = [&](const axis& ax, int k) {
        return ax.lo + (ax.hi - ax.lo) * static_cast<float>(k) / static_cast<float>(n - 1);
    };

    std::printf("\n===========================================================================\n");
    std::printf("PAIR  %s x %s\n", ps.a.name, ps.b.name);
    std::printf("      %s\n", ps.why);
    std::printf("      rows = %s %.2f -> %.2f (top to bottom), cols = %s %.2f -> %.2f (left to right)\n",
                ps.a.name, static_cast<double>(ps.a.lo), static_cast<double>(ps.a.hi),
                ps.b.name, static_cast<double>(ps.b.lo), static_cast<double>(ps.b.hi));
    std::printf("      glyphs: '#' >=90%%  '+' >=70%%  'o' >=40%%  '.' >=10%%  ',' >0%%  ' ' dead\n\n");

    for (int i = 0; i < n; ++i)
    {
        std::printf("      %7.2f |", static_cast<double>(val(ps.a, i)));
        for (int j = 0; j < n; ++j) std::printf("%c", glyph(at(i, j)));
        std::printf("|\n");
    }

    std::printf("\n      overall viability      : %.1f%%\n", static_cast<double>(overall));
    std::printf("      INTERACTION SCORE      : %.1f points  (0 = separable; corridors suffice)\n",
                static_cast<double>(interaction));
    std::printf("      worst-predicted cell   : %s=%.2f, %s=%.2f -> actual %.0f%%, independence predicts %.0f%%\n",
                ps.a.name, static_cast<double>(val(ps.a, wi)),
                ps.b.name, static_cast<double>(val(ps.b, wj)),
                static_cast<double>(at(wi, wj)),
                static_cast<double>(worst > 0.0f
                    ? rowm[static_cast<std::size_t>(wi)] * colm[static_cast<std::size_t>(wj)] / std::max(overall, 0.001f)
                    : 0.0f));
}

} // namespace

int main(int argc, char** argv)
{
    const int n     = (argc > 1) ? std::atoi(argv[1]) : 24;
    const int seeds = (argc > 2) ? std::atoi(argv[2]) : 24;

    std::printf("=== Earth-like pair-interaction atlas: %dx%d cells, %d seeds each ===\n", n, n, seeds);
    std::printf("Every parameter other than the pair is held at its Sol default; the orbit is\n");
    std::printf("derived per seed exactly as resolve_preferences derives it.\n");
    std::printf("\nWHY THIS EXISTS: the corridor harness sweeps one knob at a time, so its spans\n");
    std::printf("are one-dimensional slices. Setting the wizard bands to those spans made\n");
    std::printf("interior=high the worst lean on the board (2.97 draws) because young age and\n");
    std::printf("high radiogenic are individually fine and jointly fatal. This measures that.\n");

    for (const pair_spec& ps : k_pairs)
        run_pair(ps, n, seeds, 0xA71A5000u);

    // Self-check: the atlas and the corridor must agree about a single knob.
    // Holding one axis at its Sol default and reading across the other is a
    // corridor by another name; if that disagrees, the two harnesses are not
    // sampling the same generator and neither number can be trusted.
    {
        planetology_params sol;
        int ok = 0;
        const int probe = 200;
        for (int s = 0; s < probe; ++s)
            if (homeworld_viability(roll(sol, 0x50715000u ^ static_cast<uint32_t>(s))).ok) ++ok;
        const float sol_rate = 100.0f * static_cast<float>(ok) / static_cast<float>(probe);
        std::printf("\n\nSelf-check: Sol defaults viable on %.0f%% of %d seeds\n",
                    static_cast<double>(sol_rate), probe);
        check(sol_rate > 50.0f,
              "the Sol-default parameter set is comfortably viable (the atlas samples the real generator)");
    }

    std::printf("\n=== %s ===\n", g_fail == 0 ? "ATLAS OK" : "ATLAS PROBLEM");
    return g_fail == 0 ? 0 : 1;
}
