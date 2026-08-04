// ---------------------------------------------------------------------------
// Earth-like knob corridors (stage 2 of the earth-like battery; no SDL / Lua)
// ---------------------------------------------------------------------------
// A MEASUREMENT tool, not a pass/fail check. planetology_sweep's C1 census
// answers "which floor clause rejects?"; this answers the next question:
// "across a knob's whole range, WHERE are the earth-like edges, and what does
// the world die into beyond them?"
//
// Method: hold every parameter at its Sol default, step ONE across its internal
// clamp range, and run many seeds per step. The chain's own RNG supplies the
// spread, so each step reports a rate rather than a single world. The
// homeworld's orbit is DERIVED per seed exactly as resolve_preferences derives
// it (habitable-band placement) — pinning it at 1 AU while stellar mass varies
// is the bug that used to kill two thirds of draws, and a corridor that
// reproduced it would be measuring that bug instead of the knob.
//
// Two things are drawn on every corridor, and the gap between them is the
// point:
//
//   FLOOR EDGES  — where homeworld_viability starts rejecting.
//   BAND EDGES   — the range resolve_preferences actually samples at lean::any.
//
// The C1 census found the bands sit strictly INSIDE the floor on most axes,
// which is why ten of the floor's fourteen clauses never fire. Seeing both on
// one axis shows how much room the generator is declining to use.
//
// Run: earthlike_corridor [steps] [seeds_per_step] [knob_name]
//      defaults: 33 steps, 96 seeds, all knobs

#include "world/components.hpp"
#include "world/planetology.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

// Every clause homeworld_viability can reject on, in source order. An unknown
// reason is counted and reported rather than silently dropped — the same
// discipline planetology_sweep's census uses.
const char* const k_clauses[] = {
    "not a Cradle", "no civilisation",
    "O2 below breathable/combustion floor", "O2 too high (fire-suppressed)",
    "no liquid surface water", "too dry", "too wet", "too cold", "too hot",
    "not enough arable land", "gravity too low", "gravity too high",
    "no primary fuel", "iron too poor",
};
constexpr int k_clause_count = static_cast<int>(sizeof(k_clauses) / sizeof(k_clauses[0]));
constexpr int k_archetype_count = 16; // 13 original + 3 appended by BL-209

int clause_index(const char* r)
{
    for (int i = 0; i < k_clause_count; ++i)
        if (std::strcmp(r, k_clauses[i]) == 0) return i;
    return -1;
}

/// Short labels, so a corridor row stays one line.
const char* clause_short(int i)
{
    switch (i)
    {
        case 0:  return "not a Cradle";
        case 1:  return "no civilisation";
        case 2:  return "O2 too low";
        case 3:  return "O2 too high";
        case 4:  return "no liquid water";
        case 5:  return "too dry";
        case 6:  return "too wet";
        case 7:  return "too cold";
        case 8:  return "too hot";
        case 9:  return "arable too low";
        case 10: return "gravity too low";
        case 11: return "gravity too high";
        case 12: return "no primary fuel";
        case 13: return "iron too poor";
        default: return "?";
    }
}

struct knob
{
    const char* name;
    float planetology_params::*field;
    float lo, hi;          ///< the clamp range run_planetology enforces
    float band_lo, band_hi;///< what resolve_preferences samples at lean::any
    const char* note;
};

// Clamp ranges are run_planetology's own; band ranges are resolve_preferences'
// lean::any bands. Where the two differ is exactly what this harness exists to
// show. abiogenesis_ease has NO band — resolve_preferences pins it at 1.0 — so
// its band is recorded as a point.
// Band ranges track resolve_preferences' lean::any bands, which since
// 2026-08-04 ARE the always-viable spans this harness measured. So a healthy
// re-run now reports the band and the span coinciding; a band that has drifted
// away from its span is the signal to re-tune.
const knob k_knobs[] = {
    { "oxygenation",    &planetology_params::oxygenation,    0.00f, 1.00f, 0.30f, 0.91f,
      "the dial: GOE/NOE timing and the final O2 level" },
    { "radiogenic",     &planetology_params::radiogenic,     0.30f, 2.20f, 0.57f, 1.73f,
      "tectonic heat -> theta -> mobile lid" },
    { "system_age_gyr", &planetology_params::system_age_gyr, 1.00f,10.00f, 2.12f, 9.02f,
      "enters twice, with opposing signs" },
    { "home_ocean",     &planetology_params::home_ocean,     0.20f, 0.85f, 0.40f, 0.68f,
      "land fraction -> arable share" },
    { "home_mass",      &planetology_params::home_mass,      0.50f, 2.00f, 0.66f, 1.48f,
      "gravity, and the mass term in theta" },
    { "star_mass",      &planetology_params::star_mass,      0.60f, 1.50f, 0.60f, 1.50f,
      "luminosity; the orbit is derived from it" },
    { "metallicity",    &planetology_params::metallicity,    0.30f, 2.20f, 0.30f, 2.20f,
      "ore endowment" },
    { "coal_climate",   &planetology_params::coal_climate,   0.00f, 1.00f, 0.00f, 1.00f,
      "everwet vs seasonal basins" },
    { "drawdown",       &planetology_params::drawdown,       0.00f, 0.95f, 0.00f, 0.95f,
      "how much was already dug up" },
    { "abiogenesis_ease",&planetology_params::abiogenesis_ease,0.00f,1.00f, 1.00f, 1.00f,
      "NOT a preference — pinned at 1.0 by resolve_preferences" },
};
constexpr int k_knob_count = static_cast<int>(sizeof(k_knobs) / sizeof(k_knobs[0]));

struct step_result
{
    float value = 0.0f;
    int   viable = 0, total = 0;
    int   arch[k_archetype_count] = {};
    int   clause[k_clause_count] = {};
    int   unknown = 0;
    double o2 = 0.0, arable = 0.0, temp = 0.0, water = 0.0, theta = 0.0;
};

/// One world at a given parameter set, with the orbit derived as the generator
/// derives it.
planetology_state roll(const planetology_params& p, uint32_t seed)
{
    const float l = p.star_mass * p.star_mass * p.star_mass * std::sqrt(p.star_mass);
    checkpoint_rng r(seed, 0x0B17u); // orbit sub-stream
    body_inputs home = prototype_body(1);
    home.orbit_au = std::sqrt(l) * (0.985f + (1.400f - 0.985f) * r.unit());
    return run_planetology(home, p, seed ^ prototype_body_seed(1));
}

void run_knob(const knob& k, int steps, int seeds)
{
    std::printf("\n===========================================================================\n");
    std::printf("KNOB %s   sweep [%.2f, %.2f]   wizard 'any' band [%.2f, %.2f]\n",
                k.name, static_cast<double>(k.lo), static_cast<double>(k.hi),
                static_cast<double>(k.band_lo), static_cast<double>(k.band_hi));
    std::printf("     %s\n", k.note);
    std::printf("---------------------------------------------------------------------------\n");
    std::printf("  value  in-band  viable%%  %-22s  modal archetype   top rejection\n", "");

    std::vector<step_result> rows;
    rows.reserve(static_cast<std::size_t>(steps));

    for (int s = 0; s < steps; ++s)
    {
        const float t = (steps == 1) ? 0.5f : static_cast<float>(s) / static_cast<float>(steps - 1);
        step_result r;
        r.value = k.lo + (k.hi - k.lo) * t;

        for (int i = 0; i < seeds; ++i)
        {
            planetology_params p;               // Sol defaults
            p.*(k.field) = r.value;
            // abiogenesis_ease is pinned by resolve_preferences, so hold it
            // pinned here too unless it is the knob under test.
            if (k.field != &planetology_params::abiogenesis_ease) p.abiogenesis_ease = 1.00f;

            const uint32_t seed = 0xC0DE0000u + static_cast<uint32_t>(s * 7919 + i);
            const planetology_state st = roll(p, seed);
            const viability v = homeworld_viability(st);

            ++r.total;
            if (v.ok) ++r.viable;
            else
            {
                const int c = clause_index(v.reason);
                if (c < 0) ++r.unknown; else ++r.clause[c];
            }
            const int a = static_cast<int>(st.archetype);
            if (a >= 0 && a < k_archetype_count) ++r.arch[a];

            r.o2     += st.o2_fraction * 100.0;
            r.arable += st.arable_share * 100.0;
            r.temp   += st.surface_temp_k;
            r.water  += st.profile.water_fraction * 100.0;
            r.theta  += st.theta;
        }
        rows.push_back(r);
    }

    for (const step_result& r : rows)
    {
        const double pct = 100.0 * r.viable / std::max(1, r.total);
        const bool in_band = (r.value >= k.band_lo - 1.0e-4f && r.value <= k.band_hi + 1.0e-4f);

        char bar[23];
        const int fill = static_cast<int>(pct / 100.0 * 22.0 + 0.5);
        for (int i = 0; i < 22; ++i) bar[i] = (i < fill) ? '#' : '.';
        bar[22] = '\0';

        int am = 0; for (int i = 1; i < k_archetype_count; ++i) if (r.arch[i] > r.arch[am]) am = i;
        int cm = -1; for (int i = 0; i < k_clause_count; ++i)
            if (r.clause[i] > 0 && (cm < 0 || r.clause[i] > r.clause[cm])) cm = i;

        std::printf("  %5.2f  %-7s %6.1f%%  %s  %-16s  %s\n",
                    static_cast<double>(r.value), in_band ? "  yes" : "   -", pct, bar,
                    archetype_name(static_cast<body_archetype>(am)),
                    cm < 0 ? "-" : clause_short(cm));
    }

    // Two spans, and the difference between them is what makes the verdict
    // meaningful: ANY is where a viable world is still possible, FULL is where
    // every seed clears the floor. Comparing the band against ANY says almost
    // nothing (a single viable seed at the extreme stretches it to the whole
    // axis); comparing it against FULL says whether the generator ever actually
    // samples a value that can reject.
    int a_first = -1, a_last = -1, f_first = -1, f_last = -1;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
    {
        if (rows[i].viable > 0)              { if (a_first < 0) a_first = i; a_last = i; }
        if (rows[i].viable == rows[i].total) { if (f_first < 0) f_first = i; f_last = i; }
    }

    if (a_first < 0)
    {
        std::printf("  -> no viable world anywhere on this axis at Sol defaults\n");
        return;
    }

    std::printf("  -> any-viable [%.2f, %.2f]   always-viable %s   band [%.2f, %.2f]\n",
                static_cast<double>(rows[a_first].value), static_cast<double>(rows[a_last].value),
                f_first < 0 ? "(none)" : "",
                static_cast<double>(k.band_lo), static_cast<double>(k.band_hi));

    if (f_first >= 0)
        std::printf("     always-viable span [%.2f, %.2f]\n",
                    static_cast<double>(rows[f_first].value), static_cast<double>(rows[f_last].value));

    constexpr float eps = 1.0e-4f;
    std::printf("     VERDICT: ");
    if (k.band_lo == k.band_hi)
        std::printf("band is a POINT - not a preference, nothing to widen\n");
    else if (f_first < 0)
        std::printf("no value is always-viable - this axis rejects everywhere\n");
    else if (k.band_lo < rows[f_first].value - eps || k.band_hi > rows[f_last].value + eps)
        std::printf("band REACHES past the always-viable span - this axis really does reject\n");
    else
        std::printf("band sits INSIDE the always-viable span - it can NEVER reject; headroom unused\n");

    // The outcome scalars, so a corridor edge can be read as a cause.
    std::printf("  %-8s %8s %8s %8s %8s %8s\n", "value", "O2 %", "arable%", "temp K", "ocean%", "theta");
    for (std::size_t i = 0; i < rows.size(); i += static_cast<std::size_t>(std::max(1, steps / 8)))
    {
        const step_result& r = rows[i];
        const double n = std::max(1, r.total);
        std::printf("  %-8.2f %8.1f %8.1f %8.1f %8.1f %8.2f\n",
                    static_cast<double>(r.value), r.o2 / n, r.arable / n,
                    r.temp / n, r.water / n, r.theta / n);
    }
}

} // namespace

int main(int argc, char** argv)
{
    const int steps = (argc > 1) ? std::atoi(argv[1]) : 33;
    const int seeds = (argc > 2) ? std::atoi(argv[2]) : 96;
    const char* only = (argc > 3) ? argv[3] : nullptr;

    std::printf("=== Earth-like knob corridors: %d steps x %d seeds ===\n", steps, seeds);
    std::printf("Every other parameter held at its Sol default; orbit derived per seed.\n");

    int ran = 0;
    for (const knob& k : k_knobs)
    {
        if (only && std::strcmp(only, k.name) != 0) continue;
        run_knob(k, steps, seeds);
        ++ran;
    }

    if (ran == 0) { std::printf("\nno knob matched '%s'\n", only ? only : ""); return 1; }
    std::printf("\n=== %d corridor(s) measured ===\n", ran);
    return 0;
}
