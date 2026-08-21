// ---------------------------------------------------------------------------
// Earth-like tile census (stage 3 of the earth-like battery; no SDL / Lua)
// ---------------------------------------------------------------------------
// A MEASUREMENT tool, not a pass/fail check.
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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr int gw = 180, gh = 84;
constexpr int k_cells = gw * gh;

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
            if (scratch.tiles.at(probe[i]).substrate == terrain_substrate::ocean) pocean[i] = 1;
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
        chosen_seed, 1.0f, &st, &rec, &cs.height_bias, &cs.convergent);

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
        if (t.substrate == terrain_substrate::ocean) continue;

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

} // namespace

int main(int argc, char** argv)
{
    const int n = (argc > 1) ? std::atoi(argv[1]) : 120;

    std::printf("=== Earth-like tile census: %d campaign seeds through Kepler's pipeline ===\n", n);
    std::printf("Grid %dx%d. Percentages of LAND unless stated. Report-only: nothing is asserted.\n\n",
                gw, gh);

    std::vector<seed_metrics> all;
    all.reserve(static_cast<std::size_t>(n));
    for (int s = 0; s < n; ++s) all.push_back(census_one(static_cast<uint32_t>(s)));

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
    report("coal top-10%",      col(&seed_metrics::coal_top10),   "(concentration)");
    report("petroleum top-10%", col(&seed_metrics::oil_top10),    "(concentration)");
    report("copper top-10%",    col(&seed_metrics::copper_top10), "(concentration)");
    report("iron top-10%",      col(&seed_metrics::iron_top10),   "(concentration)");
    std::printf("  world totals (for a regions-on/off conservation check):\n");
    report("coal total",      col(&seed_metrics::coal_total),   "(sum, not a rate)");
    report("petroleum total", col(&seed_metrics::oil_total),    "(sum, not a rate)");
    report("copper total",    col(&seed_metrics::copper_total), "(sum, not a rate)");
    report("iron total",      col(&seed_metrics::iron_total),   "(sum, not a rate)");

    std::printf("\nEarth figures are ORIENTATION, not targets. A generator that hit them all\n");
    std::printf("exactly would be reproducing one planet, not generating earth-LIKE ones.\n");
    std::printf("\n=== census complete (%d seeds) ===\n", n);
    return 0;
}
