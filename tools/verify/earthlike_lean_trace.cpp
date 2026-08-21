// ---------------------------------------------------------------------------
// Earth-like lean -> outcome trace (T5 of the earth-like battery; no SDL / Lua)
// ---------------------------------------------------------------------------
// A MEASUREMENT tool, not a pass/fail check.
//
// The last unbuilt stage of the battery, and the only one that asks about the
// PLAYER rather than the generator: does the wizard's language deliver?
//
// STARTUP.md's wizard offers eight preferences in words — "oceanic", "metal-rich
// nebula", "cold and old" — and each one narrows a sampling band. Nothing has
// ever checked that narrowing a band changes the world the player then plays on.
// A preference that reads as a choice and produces the same map either way is a
// lie in the UI, and a more expensive one than a preference that is merely
// costly to satisfy: planetology_sweep's R2 already measures reroll COST, but a
// cheap lean that changes nothing passes R2 perfectly.
//
// So this runs each of the 8 axes x 3 leans through the FULL Kepler tile
// pipeline — the same wiring earthlike_tile_census uses, sea gate and rivers
// included — and reports the tile-level outcome per lean.
//
// THE METRIC IS SIGNAL AGAINST NOISE. Reporting three medians proves nothing on
// its own: every campaign seed produces a different map, so a lean has to move
// the outcome by more than the seed already does. For each (axis, metric) it
// reports
//     signal = spread of the three lean medians
//     noise  = median within-lean p05->p95 spread
// and their ratio. Below ~0.25 the preference is doing less than the seed and is
// effectively cosmetic on that metric; above ~1.0 it dominates.
//
// Report-only (BL-275). Nothing here is asserted: which preferences SHOULD be
// load-bearing is a design question, and the point of a first run is to show Ben
// which ones currently are.
//
// Run: earthlike_lean_trace [seeds_per_lean]     (default 10)
//
// Default sized against the 60 s CTest cap: 24 configurations x seeds x up to
// seven tile generations each (16 seeds measured 55 s, uncomfortably close).
// Pass a larger number by hand when reading the table properly — at 10 seeds the
// within-lean noise is itself noisy.

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

// The metrics carried through, chosen so each wizard axis has at least one it
// could plausibly move.
// Ore metrics are here because without them this harness is unfair to the
// `metal` axis: metallicity drives the ore endowment and touches no terrain at
// all, so a terrain-only metric set reports it as inert when it is simply being
// measured on the wrong axis.
enum metric { M_LAND, M_LARGEST, M_MASSES, M_FOREST, M_BARREN, M_ICY, M_RELIEF,
              M_COAL, M_IRON, M_COPPER, M_COUNT };
const char* k_metric_name[M_COUNT] = { "land%", "largest%", "masses", "forest%", "barren%",
                                       "icy%", "relief%", "coal/til", "iron/til", "copp/til" };

using sample = std::array<float, M_COUNT>;

float pct(std::vector<float> v, float q)
{
    if (v.empty()) return 0.0f;
    const std::size_t k = static_cast<std::size_t>(q * static_cast<float>(v.size() - 1));
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
    return v[k];
}

/// One campaign, generated exactly as hard_coded_world generates Kepler.
sample run_one(const world_preferences& pref, uint32_t campaign_seed)
{
    const resolved_world rw = resolve_preferences(pref, campaign_seed);
    body_inputs home = prototype_body(1);
    home.orbit_au = rw.home_orbit_au;
    const uint32_t body_seed = campaign_seed ^ prototype_body_seed(1);
    const planetology_state st = run_planetology(home, rw.params, body_seed);
    const continent_state cs = run_continents(st, gw, gh, body_seed ^ 0xC0117E57u);

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
    generate_rivers(w, ids, gw, gh, rec.height, campaign_seed ^ 0x52490001u);

    std::vector<char> land(static_cast<std::size_t>(k_cells), 0);
    int n_land = 0, n_forest = 0, n_barren = 0, n_icy = 0, n_relief = 0;
    double coal_sum = 0.0, iron_sum = 0.0, copper_sum = 0.0;
    for (int i = 0; i < static_cast<int>(ids.size()); ++i)
    {
        const auto& t = w.tiles.at(ids[static_cast<std::size_t>(i)]);
        if (t.substrate == terrain_substrate::ocean) continue;
        land[static_cast<std::size_t>(i)] = 1;
        ++n_land;
        if (t.cover == terrain_cover::forest) ++n_forest;
        if (t.substrate == terrain_substrate::barren) ++n_barren;
        if (t.substrate == terrain_substrate::icy)    ++n_icy;
        if (t.landform == terrain_landform::mountain || t.landform == terrain_landform::highland) ++n_relief;
        coal_sum   += static_cast<double>(t.resource_deposit[static_cast<std::size_t>(resource_type::coal)]);
        iron_sum   += static_cast<double>(t.resource_deposit[static_cast<std::size_t>(resource_type::iron_ore)]);
        copper_sum += static_cast<double>(t.resource_deposit[static_cast<std::size_t>(resource_type::copper_ore)]);
    }

    std::vector<int> sizes;
    label(land, sizes);
    int biggest = 0, masses = 0;
    for (int s : sizes) { if (s >= 100) ++masses; biggest = std::max(biggest, s); }

    const float lf = static_cast<float>(std::max(1, n_land));
    sample m{};
    m[M_LAND]    = 100.0f * static_cast<float>(n_land) / static_cast<float>(k_cells);
    m[M_LARGEST] = 100.0f * static_cast<float>(biggest) / lf;
    m[M_MASSES]  = static_cast<float>(masses);
    m[M_FOREST]  = 100.0f * static_cast<float>(n_forest) / lf;
    m[M_BARREN]  = 100.0f * static_cast<float>(n_barren) / lf;
    m[M_ICY]     = 100.0f * static_cast<float>(n_icy) / lf;
    m[M_RELIEF]  = 100.0f * static_cast<float>(n_relief) / lf;
    m[M_COAL]    = static_cast<float>(coal_sum / static_cast<double>(lf));
    m[M_IRON]    = static_cast<float>(iron_sum / static_cast<double>(lf));
    m[M_COPPER]  = static_cast<float>(copper_sum / static_cast<double>(lf));
    return m;
}

struct axis { const char* name; lean world_preferences::*field; const char* words; };
const axis k_axes[] = {
    { "star",         &world_preferences::star,         "dimmer / sun-like / brighter" },
    { "world_size",   &world_preferences::world_size,   "small / earth-like / large" },
    { "interior",     &world_preferences::interior,     "cold+old / moderate / young+vigorous" },
    { "metal",        &world_preferences::metal,        "metal-poor / normal / metal-rich" },
    { "ocean",        &world_preferences::ocean,        "continental / balanced / oceanic" },
    { "oxygen_story", &world_preferences::oxygen_story, "early / mixed / late oxygenation" },
    { "coal_basins",  &world_preferences::coal_basins,  "seasonal / mixed / everwet" },
    { "drawdown",     &world_preferences::drawdown,     "barely touched / worked / stripped" },
};

} // namespace

int main(int argc, char** argv)
{
    const int seeds = (argc > 1) ? std::atoi(argv[1]) : 10;

    std::printf("=== Lean -> outcome trace: 8 axes x 3 leans x %d campaign seeds ===\n", seeds);
    std::printf("Each configuration runs the FULL Kepler pipeline (sea gate + rivers).\n");
    std::printf("The question is not whether a lean is cheap — planetology_sweep R2 measures\n");
    std::printf("that — but whether it CHANGES THE MAP more than the campaign seed already does.\n");
    std::printf("signal = spread of the three lean medians; noise = median within-lean p05-p95.\n");
    std::printf("ratio < 0.25 means the preference is doing less than the seed on that metric.\n");

    for (const axis& a : k_axes)
    {
        std::printf("\n===========================================================================\n");
        std::printf("AXIS %-13s (%s)\n\n", a.name, a.words);
        std::printf("   lean  ");
        for (int m = 0; m < M_COUNT; ++m) std::printf("%10s", k_metric_name[m]);
        std::printf("\n");

        std::array<sample, 3> med{};
        std::array<sample, 3> spread{};
        int li = 0;
        for (lean l : { lean::low, lean::mid, lean::high })
        {
            std::vector<sample> runs;
            runs.reserve(static_cast<std::size_t>(seeds));
            for (int s = 0; s < seeds; ++s)
            {
                world_preferences pref;
                pref.*(a.field) = l;
                runs.push_back(run_one(pref, 0x1EA70000u + static_cast<uint32_t>(s)));
            }
            for (int m = 0; m < M_COUNT; ++m)
            {
                std::vector<float> col;
                col.reserve(runs.size());
                for (const sample& r : runs) col.push_back(r[static_cast<std::size_t>(m)]);
                med[static_cast<std::size_t>(li)][static_cast<std::size_t>(m)]    = pct(col, 0.50f);
                spread[static_cast<std::size_t>(li)][static_cast<std::size_t>(m)] = pct(col, 0.95f) - pct(col, 0.05f);
            }
            std::printf("   %-5s ", l == lean::low ? "low" : (l == lean::mid ? "mid" : "high"));
            for (int m = 0; m < M_COUNT; ++m)
                std::printf("%10.2f", static_cast<double>(med[static_cast<std::size_t>(li)][static_cast<std::size_t>(m)]));
            std::printf("\n");
            ++li;
        }

        std::printf("   ratio ");
        for (int m = 0; m < M_COUNT; ++m)
        {
            float hi = med[0][static_cast<std::size_t>(m)], lo = hi;
            for (int k = 1; k < 3; ++k)
            {
                hi = std::max(hi, med[static_cast<std::size_t>(k)][static_cast<std::size_t>(m)]);
                lo = std::min(lo, med[static_cast<std::size_t>(k)][static_cast<std::size_t>(m)]);
            }
            std::vector<float> nz{ spread[0][static_cast<std::size_t>(m)],
                                   spread[1][static_cast<std::size_t>(m)],
                                   spread[2][static_cast<std::size_t>(m)] };
            std::sort(nz.begin(), nz.end());
            const float noise = nz[1];
            std::printf("%10.2f", static_cast<double>(noise > 0.0001f ? (hi - lo) / noise : 0.0f));
        }
        std::printf("\n");
    }

    std::printf("\n=== trace complete ===\n");
    return 0;
}
