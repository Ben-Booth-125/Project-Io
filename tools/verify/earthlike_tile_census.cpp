// ---------------------------------------------------------------------------
// Earth-like tile census (stage 3 of the earth-like battery; no SDL / Lua)
// ---------------------------------------------------------------------------
// A MEASUREMENT tool with ONE asserted block (the ore-field concentration rows,
// BL-966); everything else is report-only.
//
// C1 (planetology_sweep) asks which floor clause rejects a homeworld, and the
// corridor harness asks where each knob's edges are. Both stop at the BODY
// level — a world that clears homeworld_viability is "Earth-like" by a set of
// scalars: oxygen, temperature, ocean fraction, arable share.
//
// None of that says the world LOOKS like Earth. A body can pass every scalar
// and still generate as an archipelago of four hundred islands, or one
// unbroken supercontinent, or a coastline with no inlets anywhere. This
// harness measures the map that actually gets played on.
//
// It replicates hard_coded_world's Kepler wiring exactly, including the BL-276
// two-bar sea-acceptance gate, so it measures the SHIPPED pipeline rather than
// an idealised one. Keep that gate in sync with hard_coded_world.cpp — it is
// duplicated here for the same reason mediterranean_sweep duplicates it.
//
// REPORT, DO NOT GATE (BL-275's discipline). Earth reference values are printed
// beside the measurements for orientation, and deliberately NOT asserted: the
// point of a first run is to show Ben the spread so he can decide what a band
// should be, not to encode a guess as a check.
//
// THE EXCEPTION IS THE ORE-FIELD BLOCK (BL-966). Terrain was covered
// geometrically and for determinism and economically not at all: the four
// top-10% concentration figures were the instrument that caught BL-765's +68%
// petroleum, and they caught it only because a human read them. So those four
// rows are now ASSERTED over the sweep, in wide bands derived from the measured
// distribution (the numbers and the date are in the assertion text), plus a
// minimum interquartile range per resource so a field that went FLAT across
// seeds — every world concentrated the same — fails as loudly as one that went
// uniform within a world. A band chosen from the data is a requirement that the
// distribution stays where it was measured, not a target to tune toward. The
// rest of the census stays report-only, and the sweep count stays at 120: the
// harness prints its own wall-clock so the cost of the gate is on the record.
//
// Run: earthlike_tile_census [seeds]      (default 120)
//
// Default sized against the 60 s CTest cap, not against statistical taste: each
// seed costs up to seven full tile generations (six sea-gate probes plus the
// accepted one), which measured ~0.36 s/seed. Pass a bigger number by hand when
// reading the distribution properly.

#include "world/continents.hpp"
#include "world/planetology.hpp"
#include "world/river_generation.hpp"
#include "world/tile_generation.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr int gw = 180, gh = 84;
constexpr int k_cells = gw * gh;

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

// Odd-r offset hex neighbours (odd rows shifted right), columns wrap.
void neighbours(int c, int r, int out_c[6], int out_r[6])
{
    static const int even_d[6][2] = { {1,0},{-1,0},{0,-1},{-1,-1},{0,1},{-1,1} };
    static const int odd_d[6][2]  = { {1,0},{-1,0},{1,-1},{0,-1},{1,1},{0,1} };
    const auto& d = (r & 1) ? odd_d : even_d;
    for (int i = 0; i < 6; ++i)
    {
        out_c[i] = ((c + d[i][0]) % gw + gw) % gw;
        out_r[i] = r + d[i][1];
    }
}

/// Label 6-connected components over `mask`. Same routine mediterranean_sweep
/// runs over the OCEAN mask; here it runs over the LAND mask, which is the
/// measurement nothing in the repo had yet.
std::vector<int> label(const std::vector<char>& mask, std::vector<int>& sizes)
{
    std::vector<int> comp(mask.size(), -1);
    sizes.clear();
    std::vector<int> stack;
    for (int i = 0; i < static_cast<int>(mask.size()); ++i)
    {
        if (!mask[i] || comp[i] != -1) continue;
        const int id = static_cast<int>(sizes.size());
        sizes.push_back(0);
        stack.push_back(i);
        comp[i] = id;
        while (!stack.empty())
        {
            const int t = stack.back();
            stack.pop_back();
            ++sizes[id];
            int nc[6], nr[6];
            neighbours(t % gw, t / gw, nc, nr);
            for (int k = 0; k < 6; ++k)
            {
                if (nr[k] < 0 || nr[k] >= gh) continue;
                const int n = nr[k] * gw + nc[k];
                if (mask[n] && comp[n] == -1) { comp[n] = id; stack.push_back(n); }
            }
        }
    }
    return comp;
}

struct seed_metrics
{
    float land_pct = 0.0f;
    int   landmasses = 0;      ///< land components >= 100 tiles ("continents")
    float largest_share = 0.0f;///< biggest landmass as % of all land
    float coast_pct = 0.0f;    ///< land tiles touching ocean, as % of land
    float forest_pct = 0.0f, grass_pct = 0.0f, desert_pct = 0.0f;
    float ice_pct = 0.0f, wetland_pct = 0.0f;
    float mountain_pct = 0.0f;
    float highland_pct = 0.0f; ///< Counted separately: see the report note on relief.
    float relief_pct = 0.0f;   ///< mountain + highland, the fair comparison to Earth's figure.
    float river_pct = 0.0f;
    float temperate_forest_pct = 0.0f; ///< forest as % of land in the middle latitudes

    // ore fields (Open calls 4). `top10` is the share of a resource's whole
    // world total sitting in its richest 10% of bearing tiles — a flat endowment
    // lands near 10-20%, a region model much higher. `total` is the world sum,
    // carried so a regions-on/off comparison can confirm the field only
    // REDISTRIBUTES ore rather than creating it.
    float coal_top10 = 0.0f, oil_top10 = 0.0f, copper_top10 = 0.0f, iron_top10 = 0.0f;
    float coal_total = 0.0f, oil_total = 0.0f, copper_total = 0.0f, iron_total = 0.0f;
};

/// One asserted concentration row (BL-966). `lo`/`hi` bound the sweep MEDIAN;
/// `iqr_floor` is the least p75-p25 spread across seeds the row may show.
struct concentration_band
{
    const char* name;
    float seed_metrics::*field;
    float measured_median; ///< What the 2026-09-15 run read, printed beside the band.
    float lo, hi;          ///< Band on the median of top-10% share, in %.
    float iqr_floor;       ///< Minimum interquartile range across seeds, in % points.
};

// MEASURED 2026-09-15 over the default 120 seeds (min / p05 / p25 / median /
// p75 / p95 / max, in % of the world total held by the richest 10% of bearing
// tiles):
//   coal       25.3 / 28.9 / 37.7 / 41.5 / 46.3 / 50.5 / 54.0   (IQR 8.6)
//   petroleum  52.3 / 54.9 / 61.6 / 64.9 / 67.1 / 70.3 / 74.1   (IQR 5.4)
//   copper     15.9 / 16.3 / 66.9 / 68.8 / 70.7 / 75.1 / 76.7   (IQR 3.8)
//   iron       43.2 / 45.9 / 57.0 / 61.5 / 65.0 / 69.0 / 76.9   (IQR 8.0)
//
// The bands are GENEROUS by construction — the measured median plus or minus
// roughly twice its distance to the p05/p95 tails — so ordinary drift passes and
// only a change of the kind BL-765 was (+68% on one resource) or a field that
// stopped concentrating at all (a flat endowment lands at 10-20%) fails. The
// IQR floors are about half the measured spread. Copper's low tail is real and
// KEPT: its p05 sits near the flat line because copper's region model needs a
// mobile lid, and a stagnant-lid world is honestly flat on copper — that is a
// tail the band must contain, not a defect to trim.
constexpr concentration_band k_bands[] = {
    { "coal",      &seed_metrics::coal_top10,   41.5f, 25.0f, 60.0f, 4.0f },
    { "petroleum", &seed_metrics::oil_top10,    64.9f, 50.0f, 80.0f, 2.5f },
    { "copper",    &seed_metrics::copper_top10, 68.8f, 50.0f, 85.0f, 2.0f },
    { "iron",      &seed_metrics::iron_top10,   61.5f, 45.0f, 80.0f, 4.0f },
};

/// Share of the total held by the richest 10% of non-zero entries.
void concentration(std::vector<float> v, float& top10_out, float& total_out)
{
    total_out = 0.0f;
    top10_out = 0.0f;
    if (v.empty()) return;
    for (float x : v) total_out += x;
    if (total_out <= 0.0f) return;
    std::sort(v.begin(), v.end(), std::greater<float>());
    const std::size_t k = std::max<std::size_t>(1, v.size() / 10);
    float top = 0.0f;
    for (std::size_t i = 0; i < k; ++i) top += v[i];
    top10_out = 100.0f * top / total_out;
}

/// Percentile over a copy — the spread is the deliverable, not the mean.
float pct(std::vector<float> v, float q)
{
    if (v.empty()) return 0.0f;
    const std::size_t k = static_cast<std::size_t>(q * static_cast<float>(v.size() - 1));
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
    return v[k];
}

seed_metrics census_one(uint32_t campaign_seed)
{
    const resolved_world rw = resolve_preferences(world_preferences{}, campaign_seed);
    body_inputs home = prototype_body(1);
    home.orbit_au = rw.home_orbit_au;
    const uint32_t body_seed = campaign_seed ^ prototype_body_seed(1);
    const planetology_state st = run_planetology(home, rw.params, body_seed);
    const continent_state cs = run_continents(st, gw, gh, body_seed ^ 0xC0117E57u);

    // --- BL-276 two-bar sea gate, mirrored from hard_coded_world.cpp ---------
    auto probe_sea = [&](uint32_t tile_seed)
    {
        world scratch;
        const entity_id body = scratch.create_entity();
        auto probe = generate_body_tiles(scratch, body, gw, gh, st.profile,
            tile_seed, 1.0f, &st, nullptr, &cs.height_bias, &cs.convergent);
        std::vector<char> pocean(probe.size(), 0);
        for (std::size_t i = 0; i < probe.size(); ++i)
            if (is_water(scratch.tiles.at(probe[i]).substrate)) pocean[i] = 1;
        std::vector<int> psizes;
        label(pocean, psizes);
        int pmain = -1, pmain_sz = 0;
        for (int i = 0; i < static_cast<int>(psizes.size()); ++i)
            if (psizes[static_cast<std::size_t>(i)] > pmain_sz) { pmain_sz = psizes[static_cast<std::size_t>(i)]; pmain = i; }
        int best = 0;
        for (int i = 0; i < static_cast<int>(psizes.size()); ++i)
            if (i != pmain) best = std::max(best, psizes[static_cast<std::size_t>(i)]);
        return best;
    };

    uint32_t chosen_seed = campaign_seed ^ 0xE471001u;
    {
        uint32_t floor_seed = 0;
        bool have_floor = false, have_arena = false;
        for (int attempt = 0; attempt < 6; ++attempt)
        {
            const uint32_t candidate = (campaign_seed ^ 0xE471001u) ^ (static_cast<uint32_t>(attempt) * 0x9E3779B9u);
            const int sea = probe_sea(candidate);
            if (attempt < 3 && sea >= 300) { chosen_seed = candidate; have_arena = true; break; }
            if (!have_floor && sea >= 30) { floor_seed = candidate; have_floor = true; }
            if (attempt >= 2 && have_floor) break;
        }
        if (!have_arena && have_floor) chosen_seed = floor_seed;
    }

    world w;
    const entity_id fb = w.create_entity();
    generation_record rec;
    const std::vector<entity_id> ids = generate_body_tiles(w, fb, gw, gh, st.profile,
        chosen_seed, 1.0f, &st, &rec, &cs.height_bias, &cs.convergent, &cs);

    // Rivers are a SIBLING pass (BL-051 convention), not part of the six — so a
    // harness that calls generate_body_tiles alone measures a world with no
    // rivers at all. Mirrors hard_coded_world.cpp's call, seed included.
    generate_rivers(w, ids, gw, gh, rec.height, campaign_seed ^ 0x52490001u);

    // --- measure -------------------------------------------------------------
    std::vector<char> land(static_cast<std::size_t>(k_cells), 0);
    int n_land = 0, n_forest = 0, n_grass = 0, n_desert = 0, n_ice = 0, n_wet = 0;
    int n_mountain = 0, n_highland = 0, n_river = 0;
    int n_temp_land = 0, n_temp_forest = 0;

    // "Middle latitudes" is the central half of the grid by row — a fixed
    // geometric band, NOT the body's own temperature bands, whose widths shift
    // with temperature_class and would make the metric self-referential.
    const int temp_lo = gh / 4, temp_hi = gh - gh / 4;

    for (int i = 0; i < static_cast<int>(ids.size()); ++i)
    {
        const auto& t = w.tiles.at(ids[static_cast<std::size_t>(i)]);
        if (is_water(t.substrate)) continue;

        land[static_cast<std::size_t>(i)] = 1;
        ++n_land;
        const int row = i / gw;
        const bool temperate_row = (row >= temp_lo && row < temp_hi);
        if (temperate_row) ++n_temp_land;

        // BL-519: the earthlike bands split across the two axes, and which axis
        // each one lives on is the whole content of this block.
        //
        // FOREST / GRASS / WET are COVER questions — what grows here. Counting
        // them on that axis is a real gain: a forested upland now counts as
        // forest, where before it had to give up being rocky to say so.
        //
        // DESERT / ICE are GROUND questions, and they are read from the substrate
        // REGARDLESS of cover. That distinction is not pedantry — reading them
        // from the cover instead censored them, because Pass 4d dusts cold ground
        // with snow and dry ground with dunes, and an ice cap with snow on it is
        // still an ice cap. (Measured: doing it the wrong way round dropped the
        // median icy share from 24.8% to 10.2%.)
        switch (t.cover)
        {
            case terrain_cover::forest: ++n_forest; if (temperate_row) ++n_temp_forest; break;
            case terrain_cover::grass:  ++n_grass; break;
            case terrain_cover::marsh:  ++n_wet; break;
            default: break;
        }
        switch (t.substrate)
        {
            case terrain_substrate::barren: ++n_desert; break;
            case terrain_substrate::icy:    ++n_ice; break;
            default: break;
        }
        if (t.landform == terrain_landform::mountain) ++n_mountain;
        if (t.landform == terrain_landform::highland) ++n_highland;
        if (t.river_edges != 0) ++n_river;
    }

    seed_metrics m;
    const float land_f = static_cast<float>(std::max(1, n_land));
    m.land_pct  = 100.0f * static_cast<float>(n_land) / static_cast<float>(k_cells);
    m.forest_pct = 100.0f * static_cast<float>(n_forest) / land_f;
    m.grass_pct  = 100.0f * static_cast<float>(n_grass) / land_f;
    m.desert_pct = 100.0f * static_cast<float>(n_desert) / land_f;
    m.ice_pct    = 100.0f * static_cast<float>(n_ice) / land_f;
    m.wetland_pct= 100.0f * static_cast<float>(n_wet) / land_f;
    m.mountain_pct = 100.0f * static_cast<float>(n_mountain) / land_f;
    m.highland_pct = 100.0f * static_cast<float>(n_highland) / land_f;
    m.relief_pct   = 100.0f * static_cast<float>(n_mountain + n_highland) / land_f;
    m.river_pct    = 100.0f * static_cast<float>(n_river) / land_f;
    m.temperate_forest_pct = (n_temp_land > 0)
        ? 100.0f * static_cast<float>(n_temp_forest) / static_cast<float>(n_temp_land) : 0.0f;

    // Landmass structure — the metric the ocean-only sweep could never give.
    std::vector<int> sizes;
    label(land, sizes);
    int biggest = 0;
    for (int s : sizes) { if (s >= 100) ++m.landmasses; biggest = std::max(biggest, s); }
    m.largest_share = 100.0f * static_cast<float>(biggest) / land_f;

    // Coastline: land touching ocean.
    int n_coast = 0;
    for (int i = 0; i < k_cells; ++i)
    {
        if (!land[static_cast<std::size_t>(i)]) continue;
        int nc[6], nr[6];
        neighbours(i % gw, i / gw, nc, nr);
        for (int k = 0; k < 6; ++k)
        {
            if (nr[k] < 0 || nr[k] >= gh) continue;
            if (!land[static_cast<std::size_t>(nr[k] * gw + nc[k])]) { ++n_coast; break; }
        }
    }
    m.coast_pct = 100.0f * static_cast<float>(n_coast) / land_f;

    // Region concentration, per resource that has a placement mechanism.
    {
        std::vector<float> coal, oil, cu, fe;
        for (int i = 0; i < static_cast<int>(ids.size()); ++i)
        {
            const auto& t = w.tiles.at(ids[static_cast<std::size_t>(i)]);
            const auto d = [&](resource_type r) { return t.resource_deposit[static_cast<std::size_t>(r)]; };
            if (d(resource_type::coal)       > 0.0f) coal.push_back(d(resource_type::coal));
            if (d(resource_type::petroleum)  > 0.0f) oil.push_back(d(resource_type::petroleum));
            if (d(resource_type::copper_ore) > 0.0f) cu.push_back(d(resource_type::copper_ore));
            if (d(resource_type::iron_ore)   > 0.0f) fe.push_back(d(resource_type::iron_ore));
        }
        concentration(coal, m.coal_top10,   m.coal_total);
        concentration(oil,  m.oil_top10,    m.oil_total);
        concentration(cu,   m.copper_top10, m.copper_total);
        concentration(fe,   m.iron_top10,   m.iron_total);
    }
    return m;
}

void report(const char* name, std::vector<float> v, const char* earth)
{
    std::printf("  %-22s p05 %7.1f   median %7.1f   p95 %7.1f    | Earth ~%s\n",
                name, static_cast<double>(pct(v, 0.05f)), static_cast<double>(pct(v, 0.50f)),
                static_cast<double>(pct(v, 0.95f)), earth);
}

/// The asserted rows get the whole distribution, because the band is read off
/// it and a reader re-deriving the band needs the same seven numbers.
void report_full(const char* name, const std::vector<float>& v)
{
    std::printf("  %-18s min %5.1f  p05 %5.1f  p25 %5.1f  median %5.1f  p75 %5.1f  p95 %5.1f  max %5.1f\n",
                name,
                static_cast<double>(*std::min_element(v.begin(), v.end())),
                static_cast<double>(pct(v, 0.05f)), static_cast<double>(pct(v, 0.25f)),
                static_cast<double>(pct(v, 0.50f)), static_cast<double>(pct(v, 0.75f)),
                static_cast<double>(pct(v, 0.95f)),
                static_cast<double>(*std::max_element(v.begin(), v.end())));
}

} // namespace

int main(int argc, char** argv)
{
    const int n = (argc > 1) ? std::atoi(argv[1]) : 120;

    std::printf("=== Earth-like tile census: %d campaign seeds through Kepler's pipeline ===\n", n);
    std::printf("Grid %dx%d. Percentages of LAND unless stated. Report-only, except the\n"
                "ORE FIELDS concentration rows, which are asserted in bands (BL-966).\n\n",
                gw, gh);

    // Wall-clock for the sweep alone, so the cost of running this as a gate is
    // printed beside the result rather than guessed. Harness-side only; nothing
    // generated depends on it.
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<seed_metrics> all;
    all.reserve(static_cast<std::size_t>(n));
    for (int s = 0; s < n; ++s) all.push_back(census_one(static_cast<uint32_t>(s)));
    const double sweep_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    auto col = [&](float seed_metrics::*f) {
        std::vector<float> v;
        v.reserve(all.size());
        for (const seed_metrics& m : all) v.push_back(m.*f);
        return v;
    };

    std::printf("SHAPE OF THE WORLD\n");
    report("land % of surface",  col(&seed_metrics::land_pct),      "29 %");
    report("largest landmass %", col(&seed_metrics::largest_share), "57 % of land");
    report("coastal % of land",  col(&seed_metrics::coast_pct),     "(no clean figure)");
    {
        std::vector<float> v;
        for (const seed_metrics& m : all) v.push_back(static_cast<float>(m.landmasses));
        report("landmasses >=100", v, "4-6 continents");
    }

    std::printf("\nWHAT COVERS THE LAND\n");
    report("forest %",   col(&seed_metrics::forest_pct),   "31 %");
    report("grassland %", col(&seed_metrics::grass_pct),   "26 %");
    report("barren %",   col(&seed_metrics::desert_pct),   "33 %");
    report("icy %",      col(&seed_metrics::ice_pct),      "10 %");
    report("wetland %",  col(&seed_metrics::wetland_pct),  "6 %");

    std::printf("\nRELIEF AND WATER\n");
    // Reported as three rows, not one. The first version of this harness compared
    // mountain-ONLY against Earth's ~24% mountainous-land figure, which counts
    // rugged highland too — so it overstated how flat the generated worlds are.
    // The honest comparison is the combined relief row.
    report("mountain %", col(&seed_metrics::mountain_pct), "(steep peaks only)");
    report("highland %", col(&seed_metrics::highland_pct), "(plateau/upland)");
    report("relief % (mtn+high)", col(&seed_metrics::relief_pct), "24 % mountainous land");
    report("river-tile %", col(&seed_metrics::river_pct),  "(no clean figure)");
    report("forest % in mid-lat", col(&seed_metrics::temperate_forest_pct), "(orientation only)");

    std::printf("\nORE FIELDS — share of the world total in the richest 10%% of bearing tiles\n");
    std::printf("(a flat endowment lands near 10-20%%; a region model well above it)\n");
    for (const concentration_band& b : k_bands)
    {
        char label[32];
        std::snprintf(label, sizeof label, "%s top-10%%", b.name);
        report_full(label, col(b.field));
    }
    std::printf("  world totals (for a regions-on/off conservation check):\n");
    report("coal total",      col(&seed_metrics::coal_total),   "(sum, not a rate)");
    report("petroleum total", col(&seed_metrics::oil_total),    "(sum, not a rate)");
    report("copper total",    col(&seed_metrics::copper_total), "(sum, not a rate)");
    report("iron total",      col(&seed_metrics::iron_total),   "(sum, not a rate)");

    std::printf("\nEarth figures are ORIENTATION, not targets. A generator that hit them all\n");
    std::printf("exactly would be reproducing one planet, not generating earth-LIKE ones.\n");

    // --- The asserted block (BL-966) ---------------------------------------
    std::printf("\nASSERTED: ore-field concentration over the sweep (bands from the 2026-09-15 run, %d seeds)\n", n);
    for (const concentration_band& b : k_bands)
    {
        const std::vector<float> v = col(b.field);
        const float med = pct(v, 0.50f);
        const float iqr = pct(v, 0.75f) - pct(v, 0.25f);
        char label[160];
        std::snprintf(label, sizeof label,
                      "C1 %-9s median top-10%% share %5.1f%% inside [%.0f, %.0f] (measured %.1f, 2026-09-15)",
                      b.name, static_cast<double>(med), static_cast<double>(b.lo),
                      static_cast<double>(b.hi), static_cast<double>(b.measured_median));
        check(med >= b.lo && med <= b.hi, label);
        std::snprintf(label, sizeof label,
                      "C2 %-9s interquartile range across seeds %5.1f pts >= %.1f (not flat across worlds)",
                      b.name, static_cast<double>(iqr), static_cast<double>(b.iqr_floor));
        check(iqr >= b.iqr_floor, label);
    }

    std::printf("\n=== census complete (%d seeds, sweep %.1f s) — %d FAIL ===\n", n, sweep_s, g_failures);
    return g_failures == 0 ? 0 : 1;
}
