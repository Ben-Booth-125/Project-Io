// ---------------------------------------------------------------------------
// Notable worlds — does generation produce specific, interesting seeds?
// ---------------------------------------------------------------------------
// A SEARCH tool, not a pass/fail check.
//
// Every other harness in the earth-like battery asks about the DISTRIBUTION:
// what fraction of worlds clear the floor, how much relief the median world has,
// whether a lean moves the median. Those questions are the right ones for
// calibration and the wrong ones for a game. A player does not experience a
// distribution; they experience one world, and the question that matters is
// whether any particular world is worth playing.
//
// So this one runs the search the other direction. It generates N campaigns,
// scores each against a named geographic archetype, and prints the best seeds
// with a map, so a specific world can be found, looked at, and played.
//
// THE ROME TEST is the archetype that motivated it. BL-276 built the rift-basin
// sea because Ben wanted somewhere Rome could happen: an enclosed sea with land
// around it, defensible relief, and enough arable ground to feed a city. The
// acceptance gate guarantees a SEA. It does not guarantee that the sea has the
// rest of the geography that made the Mediterranean matter, and nothing has ever
// measured whether it does.
//
// Scores are deliberately crude and additive. A weighted composite tuned to
// agree with my own taste would prove nothing; the point is to surface
// candidates for Ben to look at, not to decide for him which world is best.
//
// Run: notable_worlds [seeds] [show]      (default 240 seeds, top 3 shown)

#include "world/continents.hpp"
#include "world/planetology.hpp"
#include "world/river_generation.hpp"
#include "world/tile_generation.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
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

struct world_score
{
    uint32_t seed = 0;
    int   sea_tiles = 0;       ///< the largest enclosed (non-main) sea
    int   sea_shores = 0;      ///< distinct landmasses touching that sea
    float coast_arable = 0.0f; ///< % of tiles around the sea that are farmable
    float coast_relief = 0.0f; ///< % of tiles around the sea that are mountain/highland
    int   sea_rivers = 0;      ///< river tiles draining to the sea's shore
    float rome = 0.0f;         ///< the composite
    // Kept only for the seeds that get printed.
    std::vector<char> ocean, land;
    std::vector<int>  sea_comp;
    int               sea_id = -1;
};

/// Generate one campaign exactly as hard_coded_world generates Kepler, then
/// score its geography.
world_score score_seed(uint32_t campaign_seed, bool keep_map)
{
    world_score ws;
    ws.seed = campaign_seed;

    const resolved_world rw = resolve_preferences(world_preferences{}, campaign_seed);
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
        std::vector<char> po(probe.size(), 0);
        for (std::size_t i = 0; i < probe.size(); ++i)
            if (scratch.tiles.at(probe[i]).composition == terrain_composition::ocean) po[i] = 1;
        std::vector<int> psz;
        label(po, psz);
        int pmain = -1, pmain_sz = 0;
        for (int i = 0; i < static_cast<int>(psz.size()); ++i)
            if (psz[static_cast<std::size_t>(i)] > pmain_sz) { pmain_sz = psz[static_cast<std::size_t>(i)]; pmain = i; }
        int best = 0;
        for (int i = 0; i < static_cast<int>(psz.size()); ++i)
            if (i != pmain) best = std::max(best, psz[static_cast<std::size_t>(i)]);
        return best;
    };

    uint32_t chosen = campaign_seed ^ 0xE471001u;
    {
        uint32_t floor_seed = 0;
        bool have_floor = false, have_arena = false;
        for (int attempt = 0; attempt < 6; ++attempt)
        {
            const uint32_t cand = (campaign_seed ^ 0xE471001u) ^ (static_cast<uint32_t>(attempt) * 0x9E3779B9u);
            const int sea = probe_sea(cand);
            if (attempt < 3 && sea >= 300) { chosen = cand; have_arena = true; break; }
            if (!have_floor && sea >= 30) { floor_seed = cand; have_floor = true; }
            if (attempt >= 2 && have_floor) break;
        }
        if (!have_arena && have_floor) chosen = floor_seed;
    }

    world w;
    const entity_id fb = w.create_entity();
    generation_record rec;
    const std::vector<entity_id> ids = generate_body_tiles(w, fb, gw, gh, st.profile,
        chosen, 1.0f, &st, &rec, &cs.height_bias, &cs.convergent);
    generate_rivers(w, ids, gw, gh, rec.height, campaign_seed ^ 0x52490001u);

    std::vector<char> ocean(static_cast<std::size_t>(k_cells), 0), land(static_cast<std::size_t>(k_cells), 0);
    for (int i = 0; i < static_cast<int>(ids.size()); ++i)
    {
        const auto& t = w.tiles.at(ids[static_cast<std::size_t>(i)]);
        if (t.composition == terrain_composition::ocean) ocean[static_cast<std::size_t>(i)] = 1;
        else                                             land[static_cast<std::size_t>(i)]  = 1;
    }

    // The enclosed sea: the largest ocean component that is not the world ocean.
    std::vector<int> osz;
    const std::vector<int> ocomp = label(ocean, osz);
    int main_id = -1, main_sz = 0;
    for (int i = 0; i < static_cast<int>(osz.size()); ++i)
        if (osz[static_cast<std::size_t>(i)] > main_sz) { main_sz = osz[static_cast<std::size_t>(i)]; main_id = i; }
    int sea_id = -1, sea_sz = 0;
    for (int i = 0; i < static_cast<int>(osz.size()); ++i)
        if (i != main_id && osz[static_cast<std::size_t>(i)] > sea_sz)
        { sea_sz = osz[static_cast<std::size_t>(i)]; sea_id = i; }
    ws.sea_tiles = sea_sz;

    if (sea_id >= 0)
    {
        // The shore: land tiles touching that sea. Everything below is measured
        // on the shore rather than on the world, because "Rome-shaped" is a
        // claim about the ground AROUND a sea, not about global averages.
        std::vector<char> shore(static_cast<std::size_t>(k_cells), 0);
        int shore_n = 0, arable_n = 0, relief_n = 0, river_n = 0;
        for (int i = 0; i < k_cells; ++i)
        {
            if (!land[static_cast<std::size_t>(i)]) continue;
            int nc[6], nr[6];
            neighbours(i % gw, i / gw, nc, nr);
            bool touches = false;
            for (int k = 0; k < 6; ++k)
            {
                if (nr[k] < 0 || nr[k] >= gh) continue;
                const int n = nr[k] * gw + nc[k];
                if (ocomp[static_cast<std::size_t>(n)] == sea_id) { touches = true; break; }
            }
            if (!touches) continue;
            shore[static_cast<std::size_t>(i)] = 1;
            ++shore_n;
            const auto& t = w.tiles.at(ids[static_cast<std::size_t>(i)]);
            if (t.composition == terrain_composition::grassland
                || t.composition == terrain_composition::forest
                || t.composition == terrain_composition::wetland) ++arable_n;
            if (t.landform == terrain_landform::mountain
                || t.landform == terrain_landform::highland) ++relief_n;
            if (t.river_edges != 0) ++river_n;
        }
        const float sn = static_cast<float>(std::max(1, shore_n));
        ws.coast_arable = 100.0f * static_cast<float>(arable_n) / sn;
        ws.coast_relief = 100.0f * static_cast<float>(relief_n) / sn;
        ws.sea_rivers   = river_n;

        // How many distinct landmasses the sea touches. The Mediterranean's
        // three continents are why it was a trade basin rather than a lake.
        std::vector<int> lsz;
        const std::vector<int> lcomp = label(land, lsz);
        std::vector<char> seen(lsz.size(), 0);
        for (int i = 0; i < k_cells; ++i)
            if (shore[static_cast<std::size_t>(i)] && lcomp[static_cast<std::size_t>(i)] >= 0)
                seen[static_cast<std::size_t>(lcomp[static_cast<std::size_t>(i)])] = 1;
        for (char c : seen) if (c) ++ws.sea_shores;

        if (keep_map) { ws.ocean = ocean; ws.land = land; ws.sea_comp = ocomp; ws.sea_id = sea_id; }
    }

    // The composite. Additive and blunt on purpose — see the header.
    //  - a sea big enough to sail but small enough to cross
    //  - more than one shore, so it is a basin rather than a bay
    //  - farmland to feed a city, relief to defend it, rivers to reach inland
    const float size_term  = (ws.sea_tiles <= 0) ? 0.0f
        : 1.0f - std::fabs(static_cast<float>(ws.sea_tiles) - 700.0f) / 900.0f;
    ws.rome = 40.0f * std::max(0.0f, std::min(1.0f, size_term))
            + 14.0f * static_cast<float>(std::min(ws.sea_shores, 4))
            + 0.35f * ws.coast_arable
            + 0.25f * ws.coast_relief
            + 0.30f * static_cast<float>(std::min(ws.sea_rivers, 40));
    return ws;
}

/// Draw the region around the sea: '~' world ocean, '=' the enclosed sea,
/// '^' relief, '#' other land, ' ' off-map.
void draw_region(const world_score& ws)
{
    if (ws.sea_id < 0 || ws.ocean.empty()) return;
    int minc = gw, maxc = 0, minr = gh, maxr = 0;
    for (int i = 0; i < k_cells; ++i)
        if (ws.sea_comp[static_cast<std::size_t>(i)] == ws.sea_id)
        {
            minc = std::min(minc, i % gw); maxc = std::max(maxc, i % gw);
            minr = std::min(minr, i / gw); maxr = std::max(maxr, i / gw);
        }
    minc = std::max(0, minc - 8); maxc = std::min(gw - 1, maxc + 8);
    minr = std::max(0, minr - 4); maxr = std::min(gh - 1, maxr + 4);

    for (int r = minr; r <= maxr; ++r)
    {
        std::printf("      ");
        for (int c = minc; c <= maxc; ++c)
        {
            const int i = c + r * gw;
            if (ws.sea_comp[static_cast<std::size_t>(i)] == ws.sea_id) std::printf("=");
            else if (ws.ocean[static_cast<std::size_t>(i)])            std::printf("~");
            else                                                       std::printf("#");
        }
        std::printf("\n");
    }
}

} // namespace

int main(int argc, char** argv)
{
    const int n    = (argc > 1) ? std::atoi(argv[1]) : 240;
    const int show = (argc > 2) ? std::atoi(argv[2]) : 3;

    std::printf("=== Notable worlds: searching %d campaign seeds ===\n", n);
    std::printf("Not a distribution — a SEARCH. A player experiences one world, so the\n");
    std::printf("question is whether any particular seed is worth playing, not what the\n");
    std::printf("median seed looks like.\n\n");
    std::printf("THE ROME TEST: BL-276 guarantees an enclosed sea. It does not guarantee the\n");
    std::printf("rest of the geography that made the Mediterranean matter — several shores,\n");
    std::printf("farmland to feed a city, relief to defend it, rivers to reach inland.\n\n");

    std::vector<world_score> all;
    all.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        all.push_back(score_seed(static_cast<uint32_t>(i), false));

    std::sort(all.begin(), all.end(),
              [](const world_score& a, const world_score& b) { return a.rome > b.rome; });

    // How rare is Rome-shaped? Report the spread, then the exemplars.
    int with_sea = 0, multi_shore = 0, strong = 0;
    for (const world_score& w : all)
    {
        if (w.sea_tiles >= 300) ++with_sea;
        if (w.sea_shores >= 2)  ++multi_shore;
        if (w.rome >= 70.0f)    ++strong;
    }
    std::printf("  sea of 300+ tiles        : %5.1f%% of seeds\n", 100.0 * with_sea / n);
    std::printf("  that sea has 2+ shores   : %5.1f%%\n", 100.0 * multi_shore / n);
    std::printf("  composite >= 70          : %5.1f%%\n\n", 100.0 * strong / n);

    std::printf("  %-8s %8s %7s %8s %8s %7s %8s\n",
                "seed", "rome", "sea", "shores", "arable%", "relief%", "rivers");
    for (int i = 0; i < std::min(12, static_cast<int>(all.size())); ++i)
    {
        const world_score& w = all[static_cast<std::size_t>(i)];
        std::printf("  %-8u %8.1f %7d %8d %8.1f %7.1f %8d\n",
                    w.seed, static_cast<double>(w.rome), w.sea_tiles, w.sea_shores,
                    static_cast<double>(w.coast_arable), static_cast<double>(w.coast_relief),
                    w.sea_rivers);
    }

    for (int i = 0; i < std::min(show, static_cast<int>(all.size())); ++i)
    {
        const world_score& w = score_seed(all[static_cast<std::size_t>(i)].seed, true);
        std::printf("\n  --- seed %u : sea %d tiles, %d shores, arable %.0f%%, relief %.0f%%, %d river tiles ---\n",
                    w.seed, w.sea_tiles, w.sea_shores,
                    static_cast<double>(w.coast_arable), static_cast<double>(w.coast_relief), w.sea_rivers);
        std::printf("      ('=' the enclosed sea, '~' world ocean, '#' land)\n");
        draw_region(w);
    }

    std::printf("\n=== search complete ===\n");
    return 0;
}
