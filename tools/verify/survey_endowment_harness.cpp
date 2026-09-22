// ---------------------------------------------------------------------------
// survey_endowment harness (BL-966; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// `survey_endowment` (src/world/settlement.cpp) is where terrain becomes an
// economic fact: it reads the tiles under a region's window into the four raw
// class endowments — farm, ore, energy, water — that `score_against` turns into
// `region::farm_q` and its siblings, which in turn drive cradle placement, the
// region class argmax and corporate focus. Until this harness nothing read it:
// deposit_origin names it only to explain why an item could not be delivered.
//
// Four properties, on the REFERENCE SEED's homeworld (world_params{} through
// make_hard_coded_world with the pre-history sim off — `no_prehistory()`, a
// scope declaration: the era is not this harness's subject):
//
//   E1  DETERMINISM. Two calls on the same window return identical scores, and
//       the same call on a SECOND generation of the same params returns the
//       same again. The standing invariant of src/world, stated for this read.
//
//   E2  ORDERING. A region over ground rich in a class scores above one over
//       barren ground. The comparison is built from the REAL regions: an
//       independent tile read (this file's own window sum, written against the
//       tile fields rather than the survey) picks the richest and barrenest
//       region per class, and the survey must rank them the same way, strictly.
//       E2c closes the loop to what generation actually STORED: the region the
//       survey ranks richest must carry the highest `*_q` score of the set, so
//       the function under test is the one the shipped regions were scored by.
//
//   E3  THE ENDOWMENT-ZERO RULE. A body whose planetology endowment for a
//       resource is 0.0 contributes 0 for that resource in every window —
//       "no life, no coal" is a hard zero, not a thin field. Checked on every
//       generated body: for each survey-read resource at 0.0 the tile sum is 0,
//       and on a body with a WHOLE class at 0.0 the class score is 0 in every
//       window sampled across its grid. The homeworld has none of its read
//       resources at 0.0 (reported), so the rule is exercised on the airless
//       bodies — which body, and which channels, is printed.
//
//   E4  A NON-FLAT SPREAD. The point of the read (GENERATION_STRATEGY.md §
//       Asymmetry is the deliverable) is that regions DIFFER. The interquartile
//       range of each class across the homeworld's regions must clear a floor
//       measured on the reference seed and stated in the assertion text.
//
// The process exits non-zero if any assertion FAILs.
//
//   node tools/verify/build_harness.js survey_endowment_harness --run
// ---------------------------------------------------------------------------
#include "harness_params.hpp"
#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/planetology.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

/// The body's tiles in raster order (row * gw + col), rebuilt from each tile's
/// own grid position rather than from container order — `world::tiles` is an
/// unordered_map, and this is exactly what `generate_body_tiles` returned to
/// generation. Every slot must be filled; a hole is a harness bug.
std::vector<entity_id> raster_ids(const world& w, entity_id body, int gw, int gh)
{
    std::vector<entity_id> ids(static_cast<std::size_t>(gw * gh), null_entity);
    for (const auto& [id, t] : w.tiles)
    {
        if (t.body != body) continue;
        if (t.grid_x < 0 || t.grid_x >= gw || t.grid_y < 0 || t.grid_y >= gh) continue;
        ids[static_cast<std::size_t>(t.grid_y * gw + t.grid_x)] = id;
    }
    return ids;
}

int wrapc(int c, int gw) { return ((c % gw) + gw) % gw; }

/// The independent read for E2: this file's own sum of a class's deposits over
/// the same window shape the survey uses (`win = max(3, gw / 45)`), written
/// against the tile fields directly. Water tiles are skipped — a harbour is not
/// barren ore ground, it is no ground — and the sum is NOT divided, so it is a
/// different quantity from the survey's per-tile mean rather than a copy of it.
struct class_sum
{
    double farm = 0.0, ore = 0.0, energy = 0.0;
};

class_sum window_sum(const world& w, const std::vector<entity_id>& ids,
                     int col, int row, int gw, int gh)
{
    const int win = std::max(3, gw / 45);
    class_sum s;
    for (int dr = -win; dr <= win; ++dr)
    {
        const int r = row + dr;
        if (r < 0 || r >= gh) continue;
        for (int dc = -win; dc <= win; ++dc)
        {
            const entity_id id = ids[static_cast<std::size_t>(wrapc(col + dc, gw) + r * gw)];
            const auto it = w.tiles.find(id);
            if (it == w.tiles.end() || is_water(it->second.substrate)) continue;
            const auto& d = it->second.resource_deposit;
            const auto at = [&](resource_type rt) { return static_cast<double>(d[static_cast<std::size_t>(rt)]); };
            s.farm   += at(resource_type::agricultural_produce);
            s.ore    += at(resource_type::iron_ore) + at(resource_type::copper_ore) + at(resource_type::rare_earth_ore);
            s.energy += at(resource_type::coal) + at(resource_type::petroleum);
        }
    }
    return s;
}

/// Percentile over a copy of an integer series.
int pct(std::vector<int> v, double q)
{
    if (v.empty()) return 0;
    const std::size_t k = static_cast<std::size_t>(q * static_cast<double>(v.size() - 1));
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
    return v[k];
}

/// The resources the survey reads, by class, so E3 can walk them by name.
struct read_channel { resource_type res; const char* name; const char* cls; };
constexpr read_channel k_channels[] = {
    { resource_type::agricultural_produce, "agricultural_produce", "farm"   },
    { resource_type::iron_ore,             "iron_ore",             "ore"    },
    { resource_type::copper_ore,           "copper_ore",           "ore"    },
    { resource_type::rare_earth_ore,       "rare_earth_ore",       "ore"    },
    { resource_type::coal,                 "coal",                 "energy" },
    { resource_type::petroleum,            "petroleum",            "energy" },
};

const generation_report::body_entry* homeworld(const generation_report& r)
{
    for (const auto& b : r.bodies)
        if (b.is_homeworld) return &b;
    return nullptr;
}

} // namespace

int main()
{
    std::printf("=== survey_endowment harness (BL-966): the reference seed's homeworld ===\n");

    const auto t0 = std::chrono::steady_clock::now();
    generation_report r1, r2;
    const world w1 = make_hard_coded_world(no_prehistory(), &r1);
    const double gen_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const world w2 = make_hard_coded_world(no_prehistory(), &r2);
    std::printf("two worlds generated (%.1f s for the first; prehistory off)\n\n", gen_s);

    const auto* h1 = homeworld(r1);
    const auto* h2 = homeworld(r2);
    if (!h1 || !h2)
    {
        std::printf("FAIL  the homeworld is missing from the generation report\n");
        return 1;
    }
    const body_component& hb = w1.bodies.at(h1->id);
    const int gw = hb.grid_width, gh = hb.grid_height;
    const std::vector<entity_id> ids1 = raster_ids(w1, h1->id, gw, gh);
    const std::vector<entity_id> ids2 = raster_ids(w2, h2->id, gw, gh);
    check(std::none_of(ids1.begin(), ids1.end(), [](entity_id e) { return e == null_entity; }),
          "E0a  every raster slot of the homeworld grid holds a tile");

    const std::vector<region>& regions = h1->settlement.regions;
    std::printf("homeworld %s: grid %dx%d, %zu regions, survey window half-width %d\n\n",
                hb.name.c_str(), gw, gh, regions.size(), std::max(3, gw / 45));
    check(regions.size() >= 8, "E0b  the homeworld settles at least eight regions (enough for a spread)");
    if (regions.size() < 2) return 1;

    // --- E1 determinism -------------------------------------------------------
    {
        bool same_call = true, same_world = true;
        for (const region& p : regions)
        {
            const endowment a = survey_endowment(w1, ids1, p.col, p.row, gw, gh);
            const endowment b = survey_endowment(w1, ids1, p.col, p.row, gw, gh);
            const endowment c = survey_endowment(w2, ids2, p.col, p.row, gw, gh);
            if (a.farm != b.farm || a.ore != b.ore || a.energy != b.energy || a.water != b.water) same_call = false;
            if (a.farm != c.farm || a.ore != c.ore || a.energy != c.energy || a.water != c.water) same_world = false;
        }
        check(same_call,  "E1a  two calls on the same window return identical scores (every region)");
        check(same_world, "E1b  the same window on a second generation of the same params scores identically");
        check(h2->settlement.regions.size() == regions.size(),
              "E1c  the second generation settled the same number of regions");
    }

    // --- E2 ordering ------------------------------------------------------------
    std::vector<endowment> surveyed;
    surveyed.reserve(regions.size());
    for (const region& p : regions) surveyed.push_back(survey_endowment(w1, ids1, p.col, p.row, gw, gh));

    {
        std::vector<class_sum> indep;
        indep.reserve(regions.size());
        for (const region& p : regions) indep.push_back(window_sum(w1, ids1, p.col, p.row, gw, gh));

        struct axis
        {
            const char* name;
            double class_sum::*raw;
            int endowment::*score;
            int region::*stored;
        };
        const axis axes[] = {
            { "farm",   &class_sum::farm,   &endowment::farm,   &region::farm_q   },
            { "ore",    &class_sum::ore,    &endowment::ore,    &region::ore_q    },
            { "energy", &class_sum::energy, &endowment::energy, &region::energy_q },
        };
        char label[200];
        for (const axis& ax : axes)
        {
            std::size_t hi = 0, lo = 0;
            for (std::size_t i = 1; i < indep.size(); ++i)
            {
                if (indep[i].*(ax.raw) > indep[hi].*(ax.raw)) hi = i;
                if (indep[i].*(ax.raw) < indep[lo].*(ax.raw)) lo = i;
            }
            const int s_hi = surveyed[hi].*(ax.score), s_lo = surveyed[lo].*(ax.score);
            std::printf("  %-6s richest by tile read: %-28s (sum %8.1f, survey %5d, stored q %4d)\n",
                        ax.name, regions[hi].name.c_str(), indep[hi].*(ax.raw), s_hi, regions[hi].*(ax.stored));
            std::printf("  %-6s barrenest by tile read: %-26s (sum %8.1f, survey %5d, stored q %4d)\n",
                        ax.name, regions[lo].name.c_str(), indep[lo].*(ax.raw), s_lo, regions[lo].*(ax.stored));

            std::snprintf(label, sizeof label,
                          "E2a %-6s the survey ranks the richest window above the barrenest (%d > %d)",
                          ax.name, s_hi, s_lo);
            check(s_hi > s_lo, label);
            std::snprintf(label, sizeof label,
                          "E2b %-6s rich is rich and barren is barren: survey %d >= 2x %d",
                          ax.name, s_hi, s_lo);
            check(s_hi >= 2 * s_lo && s_hi > 0, label);

            // The stored score is a monotone transform of the survey (score_against,
            // then a clamp at 1000), so the survey's richest carries the top stored
            // score and the barrenest the bottom — unless the clamp flattened the
            // top, which the >= over the set tolerates.
            int max_q = 0, min_q = 1000;
            for (const region& p : regions) { max_q = std::max(max_q, p.*(ax.stored)); min_q = std::min(min_q, p.*(ax.stored)); }
            std::snprintf(label, sizeof label,
                          "E2c %-6s the shipped region scores agree: richest holds q %d (= set max %d), barrenest q %d (= set min %d)",
                          ax.name, regions[hi].*(ax.stored), max_q, regions[lo].*(ax.stored), min_q);
            check(regions[hi].*(ax.stored) == max_q && regions[lo].*(ax.stored) == min_q, label);
        }
    }

    // --- E3 the endowment-zero rule ------------------------------------------
    std::printf("\nendowment channels the survey reads, per body (0.0 = the rule applies):\n");
    {
        char label[220];
        for (const auto& b : r1.bodies)
        {
            const auto bit = w1.bodies.find(b.id);
            if (bit == w1.bodies.end()) continue;
            const int bgw = bit->second.grid_width, bgh = bit->second.grid_height;
            const std::vector<entity_id> bids = raster_ids(w1, b.id, bgw, bgh);
            if (std::any_of(bids.begin(), bids.end(), [](entity_id e) { return e == null_entity; }))
            {
                std::printf("  %-8s has no tile grid in this world; skipped\n", b.name.c_str());
                continue;
            }

            std::printf("  %-14s%s", b.name.c_str(), b.is_homeworld ? " (homeworld)  " : "              ");
            for (const read_channel& ch : k_channels)
                std::printf(" %s=%.2f", ch.name, static_cast<double>(b.state.endowment[static_cast<std::size_t>(ch.res)]));
            std::printf("\n");

            bool farm_zero = true, ore_zero = true, energy_zero = true;
            int zero_channels = 0;
            for (const read_channel& ch : k_channels)
            {
                const float e = b.state.endowment[static_cast<std::size_t>(ch.res)];
                if (e != 0.0f)
                {
                    if (ch.cls[0] == 'f') farm_zero = false;
                    else if (ch.cls[0] == 'o') ore_zero = false;
                    else energy_zero = false;
                    continue;
                }
                ++zero_channels;
                double on_tiles = 0.0;
                for (const entity_id id : bids)
                    on_tiles += static_cast<double>(w1.tiles.at(id).resource_deposit[static_cast<std::size_t>(ch.res)]);
                std::snprintf(label, sizeof label,
                              "E3a %s: %s endowment 0.0 -> its deposit sums to 0 over every tile (contribution 0 in any window)",
                              b.name.c_str(), ch.name);
                check(on_tiles == 0.0, label);
            }

            if (farm_zero || ore_zero || energy_zero)
            {
                // Sample windows on a stride of one window width across the whole grid.
                const int win = std::max(3, bgw / 45);
                const int stride = 2 * win + 1;
                int windows = 0;
                bool farm_ok = true, ore_ok = true, energy_ok = true;
                for (int row = win; row < bgh; row += stride)
                    for (int col = 0; col < bgw; col += stride)
                    {
                        const endowment e = survey_endowment(w1, bids, col, row, bgw, bgh);
                        ++windows;
                        if (farm_zero   && e.farm   != 0) farm_ok   = false;
                        if (ore_zero    && e.ore    != 0) ore_ok    = false;
                        if (energy_zero && e.energy != 0) energy_ok = false;
                    }
                if (farm_zero)
                {
                    std::snprintf(label, sizeof label, "E3b %s: farm class wholly at 0.0 -> survey farm == 0 in all %d sampled windows", b.name.c_str(), windows);
                    check(farm_ok, label);
                }
                if (ore_zero)
                {
                    std::snprintf(label, sizeof label, "E3b %s: ore class wholly at 0.0 -> survey ore == 0 in all %d sampled windows", b.name.c_str(), windows);
                    check(ore_ok, label);
                }
                if (energy_zero)
                {
                    std::snprintf(label, sizeof label, "E3b %s: energy class wholly at 0.0 -> survey energy == 0 in all %d sampled windows", b.name.c_str(), windows);
                    check(energy_ok, label);
                }
            }
            if (b.is_homeworld)
                std::printf("  (homeworld has %d read channel(s) at 0.0 — the rule is exercised on the other bodies)\n",
                            zero_channels);
        }
    }

    // --- E4 spread --------------------------------------------------------------
    std::printf("\nper-region raw endowment across the homeworld's %zu regions (per-tile mean x1000):\n",
                regions.size());
    {
        struct spread_axis { const char* name; int endowment::*f; int measured_iqr; int floor; };
        // MEASURED 2026-09-15 on the reference seed (world_params{}), prehistory
        // off, 209 regions on a 261x121 grid (per-tile mean x1000):
        //   farm    min 0  p25   1581  median 12849  p75  22636  max   45231   IQR 21055
        //   ore     min 0  p25  24807  median 67829  p75 110580  max 1945784   IQR 85773
        //   energy  min 0  p25   1495  median  8660  p75  16271  max  809056   IQR 14776
        // The floors below are about half the interquartile range read on that
        // run, and the measured IQR is printed in the assertion text. A floor
        // chosen from the data is a requirement that the spread stays, not a
        // target; a field that collapsed to within half its measured spread
        // has stopped telling regions apart.
        const spread_axis axes[] = {
            { "farm",   &endowment::farm,   21055, 10000 },
            { "ore",    &endowment::ore,    85773, 40000 },
            { "energy", &endowment::energy, 14776,  7000 },
        };
        char label[200];
        for (const spread_axis& ax : axes)
        {
            std::vector<int> v;
            v.reserve(surveyed.size());
            for (const endowment& e : surveyed) v.push_back(e.*(ax.f));
            const int p25 = pct(v, 0.25), p50 = pct(v, 0.50), p75 = pct(v, 0.75);
            const int lo = *std::min_element(v.begin(), v.end());
            const int hi = *std::max_element(v.begin(), v.end());
            std::printf("  %-6s min %6d  p25 %6d  median %6d  p75 %6d  max %6d   IQR %6d\n",
                        ax.name, lo, p25, p50, p75, hi, p75 - p25);
            std::snprintf(label, sizeof label,
                          "E4  %-6s interquartile range across regions %d >= floor %d (measured %d, 2026-09-15; not flat)",
                          ax.name, p75 - p25, ax.floor, ax.measured_iqr);
            check(p75 - p25 >= ax.floor, label);
        }
    }

    // --- E5 the span-open survey on a COAL-POOR body (BL-1041) -----------------
    // `survey_regions_at_span_open` scores `survey_fuel_q` against the mean
    // over every region it surveys. Before BL-1041 that mean was an integer
    // (`sum / n`), which is harmless on a body whose raw fuel runs into the
    // thousands and ruinous on a coal-poor one: a true mean of 1.52 truncated
    // to 1 and scored an about-average region at 1000, the ceiling. A
    // SYNTHETIC body isolates exactly that: 25 regions on a 40x40 all-land
    // grid, windows disjoint (half-width 3), and ONE coal tile under each
    // anchor sized so the raw window read is 1 on twelve regions and 2 on
    // thirteen -- sum 38, true mean 1.52. The expected score is computed here
    // from `survey_endowment`'s own raw reads with the exact rational mean,
    // `raw * 500 * n / sum`, and every region must carry it; the truncated
    // rule would give 500 / 1000, which E5b and E5c pin as refused.
    std::printf("\nE5: the span-open survey on a synthetic coal-poor body (25 regions, true fuel mean 1.52)\n");
    {
        constexpr int sgw = 40, sgh = 40;
        world sw;
        const entity_id sbody = sw.create_entity();
        std::vector<entity_id> sids(static_cast<std::size_t>(sgw * sgh), null_entity);
        for (int r = 0; r < sgh; ++r)
            for (int c = 0; c < sgw; ++c)
            {
                const entity_id id = sw.create_entity();
                tile_component t{};
                t.body         = sbody;
                t.grid_x       = c;
                t.grid_y       = r;
                t.substrate    = terrain_substrate::sedimentary;
                t.landform     = terrain_landform::plains;
                t.hazard_level = 0.0f;
                t.habitability = 1.0f;
                sw.tiles.emplace(id, t);
                sids[static_cast<std::size_t>(r * sgw + c)] = id;
            }
        std::vector<region> sregions;
        int k = 0;
        for (int gy = 0; gy < 5; ++gy)
            for (int gx = 0; gx < 5; ++gx, ++k)
            {
                region rg;
                rg.col    = 3 + gx * 8; // windows [col-3, col+3] never overlap
                rg.row    = 3 + gy * 8;
                rg.anchor = rg.row * sgw + rg.col;
                sregions.push_back(rg);
                // 0.06 x 1000 / 49 cells = raw 1; 0.10 x 1000 / 49 = raw 2.
                tile_component& at = sw.tiles.at(sids[static_cast<std::size_t>(rg.anchor)]);
                at.resource_deposit[static_cast<std::size_t>(resource_type::coal)] = (k < 12) ? 0.06f : 0.10f;
            }

        std::vector<int> raw;
        int64_t sum = 0;
        for (const region& rg : sregions)
        {
            raw.push_back(survey_endowment(sw, sids, rg.col, rg.row, sgw, sgh).energy);
            sum += raw.back();
        }
        const int64_t n = static_cast<int64_t>(sregions.size());
        survey_regions_at_span_open(sw, sids, sgw, sgh, sregions);

        int ones = 0, twos = 0, exact = 0, twos_below_ceiling = 0;
        for (std::size_t i = 0; i < sregions.size(); ++i)
        {
            const int64_t expect = sum > 0 ? std::min<int64_t>((raw[i] * 500LL * n) / sum, 1000) : 0;
            if (sregions[i].survey_fuel_q == expect) ++exact;
            if (raw[i] == 1) ++ones;
            if (raw[i] == 2) { ++twos; if (sregions[i].survey_fuel_q < 1000) ++twos_below_ceiling; }
        }
        std::printf("  raw fuel reads: %d at 1, %d at 2, sum %lld over %lld regions (integer mean %lld)\n",
                    ones, twos, static_cast<long long>(sum), static_cast<long long>(n),
                    static_cast<long long>(sum / n));
        check(ones == 12 && twos == 13 && sum == 38,
              "E5a  the synthetic body reads as built: 12 regions at raw 1, 13 at raw 2 (sum 38, mean 1.52)");
        check(exact == static_cast<int>(n),
              "E5b  every region's survey_fuel_q is raw*500*n/sum against the EXACT mean (raw 1 -> 328, raw 2 -> 657)");
        check(twos_below_ceiling == twos,
              "E5c  an about-average region (raw 2, 1.3x the mean) reads below 1000 -- the truncated mean of 1 put it at the ceiling");
    }

    // --- E6 the forest score, scored as fuel is (Ben, 2026-09-18, wave 1 form) --
    // `survey_forest_q` is the window's land share under forest SCORED against
    // the exact mean share of every region at the open -- the fuel rule. A
    // synthetic body with no fuel at all and thin forest: 25 disjoint windows
    // of 49 land tiles, one forest tile in twelve of them (share 20 per mille)
    // and two in thirteen (40): sum 760, true mean 30.4. Exact scores 328 and
    // 657; the truncated mean (30) would give 333 and 666, which E6c refuses.
    // No coal anywhere: every region's fuel scores 0 (surveyed, not -1).
    std::printf("\nE6: the span-open forest score on a synthetic forest-poor, fuel-less body (true share mean 30.4)\n");
    {
        constexpr int sgw = 40, sgh = 40;
        world sw;
        const entity_id sbody = sw.create_entity();
        std::vector<entity_id> sids(static_cast<std::size_t>(sgw * sgh), null_entity);
        for (int r = 0; r < sgh; ++r)
            for (int c = 0; c < sgw; ++c)
            {
                const entity_id id = sw.create_entity();
                tile_component t{};
                t.body         = sbody;
                t.grid_x       = c;
                t.grid_y       = r;
                t.substrate    = terrain_substrate::sedimentary;
                t.landform     = terrain_landform::plains;
                t.hazard_level = 0.0f;
                t.habitability = 1.0f;
                sw.tiles.emplace(id, t);
                sids[static_cast<std::size_t>(r * sgw + c)] = id;
            }
        std::vector<region> sregions;
        int k = 0;
        for (int gy = 0; gy < 5; ++gy)
            for (int gx = 0; gx < 5; ++gx, ++k)
            {
                region rg;
                rg.col    = 3 + gx * 8;
                rg.row    = 3 + gy * 8;
                rg.anchor = rg.row * sgw + rg.col;
                sregions.push_back(rg);
                const int wooded_tiles = (k < 12) ? 1 : 2;
                for (int f = 0; f < wooded_tiles; ++f)
                {
                    tile_component& ft = sw.tiles.at(sids[static_cast<std::size_t>(rg.row * sgw + rg.col + 1 + f)]);
                    ft.cover         = terrain_cover::forest;
                    ft.cover_density = 128;
                }
            }

        std::vector<int> share;
        int64_t sum = 0;
        for (const region& rg : sregions)
        {
            share.push_back(survey_endowment(sw, sids, rg.col, rg.row, sgw, sgh).forest);
            sum += share.back();
        }
        const int64_t n = static_cast<int64_t>(sregions.size());
        survey_regions_at_span_open(sw, sids, sgw, sgh, sregions);

        int at20 = 0, at40 = 0, exact = 0, not_truncated = 0, fuel_zero = 0;
        for (std::size_t i = 0; i < sregions.size(); ++i)
        {
            const int64_t expect = sum > 0 ? std::min<int64_t>((share[i] * 500LL * n) / sum, 1000) : 0;
            const int64_t truncated = std::min<int64_t>((share[i] * 500LL) / std::max<int64_t>(1, sum / n), 1000);
            if (sregions[i].survey_forest_q == expect) ++exact;
            if (sregions[i].survey_forest_q != truncated) ++not_truncated;
            if (share[i] == 20) ++at20;
            if (share[i] == 40) ++at40;
            if (sregions[i].survey_fuel_q == 0) ++fuel_zero;
        }
        std::printf("  forest shares: %d at 20, %d at 40, sum %lld over %lld regions (integer mean %lld);"
                    " scores %d / %d\n", at20, at40, static_cast<long long>(sum), static_cast<long long>(n),
                    static_cast<long long>(sum / n), sregions[0].survey_forest_q, sregions[24].survey_forest_q);
        check(at20 == 12 && at40 == 13 && sum == 760,
              "E6a  the synthetic body reads as built: 12 windows at share 20, 13 at 40 (sum 760, mean 30.4)");
        check(exact == static_cast<int>(n),
              "E6b  every region's survey_forest_q is share*500*n/sum against the EXACT mean (20 -> 328, 40 -> 657)");
        check(not_truncated == static_cast<int>(n),
              "E6c  no region reads the truncated-mean score (333 / 666): forest is scored with fuel's exact-mean fix");
        check(fuel_zero == static_cast<int>(n),
              "E6d  a body with no fuel at all scores every surveyed region's fuel 0 (surveyed, never -1)");
    }

    std::printf("\n=== survey_endowment harness: %d PASS, %d FAIL ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
