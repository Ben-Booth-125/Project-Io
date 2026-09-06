// ---------------------------------------------------------------------------
// Headless centre/region binding harness (BL-783, a centre stands where its
// region stood; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// BL-766 made cities SIM-GRAIN state: `region::centres`, `centres_razed` and
// `urban_population` are drawn before the Era -1 sim and then grown and sacked
// by it. `generate_population_centres` materialises the campaign entities from
// that record. Before BL-783 that materialisation was BODY-WIDE — only the
// count and the scale were region-derived — so nothing bound a placed city back
// to the farmland that grew it, and the causal story died at the last step.
//
//   R1  BOUND. Every materialised centre stands in the region that grew it.
//       Measured as the per-region materialised count against `region::centres`
//       (the Voronoi over anchors IS a region's extent, and `nearest_region` is
//       the canonical read of it — the same one city_names.cpp names from).
//       A region whose own ground is built out SPILLS to its nearest neighbour
//       rather than losing the settlement; spill is counted, not hidden.
//   R2  THE CAUSAL STORY SURVIVES. Within a farming band, a region history
//       sacked carries fewer and smaller cities than one it did not. Reported
//       as a full table AND asserted, because this is the item's whole point
//       rather than a side effect.
//   R3  DETERMINISTIC AND SEEDLESS. Two generations on one seed produce an
//       identical (tile, scale) centre table; the campaign placement path draws
//       no RNG at all.
//
// Exits non-zero on any FAIL. Links the generation TU superset (as
// era_world_harness / world_audit):  node tools/verify/build_harness.js centre_region_bind

#include "world/hard_coded_world.hpp"
#include "world/population_generation.hpp"
#include "world/placement_rules.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <utility>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

const generation_report::body_entry* home_entry(const generation_report& rep)
{
    for (const auto& be : rep.bodies)
        if (be.is_homeworld) return &be; // identity, not display name (BL-257)
    return nullptr;
}

/// One materialised centre, reduced to what this harness compares.
struct placed
{
    int raster = 0;   ///< row * gw + col on the home body.
    int scale  = 0;
    int region = -1;  ///< nearest_region of its tile.
};

/// The body's centres in raster order — a total order that does not depend on
/// entity ids, so two runs are comparable even if allocation shifted.
std::vector<placed> centres_of(const world& w, entity_id body,
                               const settlement_state& ss, int gw)
{
    std::vector<placed> out;
    for (const auto& [cid, tid] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tid);
        if (tit == w.tiles.end() || tit->second.body != body) continue;
        const auto pit = w.population_centres.find(cid);
        if (pit == w.population_centres.end()) continue;
        placed p;
        p.raster = tit->second.grid_y * gw + tit->second.grid_x;
        p.scale  = pit->second.scale;
        p.region = nearest_region(ss, tit->second.grid_x, tit->second.grid_y, gw);
        out.push_back(p);
    }
    // Sorted, so the unordered_map walk above cannot leak its layout into the
    // comparison (src/world/CLAUDE.md — no hash-layout-dependent ordering).
    std::sort(out.begin(), out.end(), [](const placed& a, const placed& b) {
        if (a.raster != b.raster) return a.raster < b.raster;
        return a.scale < b.scale;
    });
    return out;
}

/// Strip every population centre on @p body, so the pass under test can be run
/// in isolation.
///
/// WHY THE HARNESS MUST DO THIS. A finished world carries centres from THREE
/// passes: `generate_population_centres` (the one BL-783 changed), then
/// `ensure_national_population_centres` (BL-463) and
/// `ensure_province_anchor_centres` (BL-611), both of which found centres on
/// their own argmax and are deliberately region-blind — a province anchor
/// exists because a province exists, not because a region grew it. Measuring
/// the binding over the finished world scores this item against two passes it
/// does not own, and the first cut of this harness did exactly that: 1,805
/// centres against 812 carved ones, a 71% "drift" that was almost entirely
/// province anchors.
void strip_centres(world& w, entity_id body)
{
    std::vector<entity_id> drop;
    for (const auto& [cid, tid] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tid);
        if (tit != w.tiles.end() && tit->second.body == body) drop.push_back(cid);
    }
    std::sort(drop.begin(), drop.end()); // erase order cannot matter, but be explicit
    for (const entity_id cid : drop)
    {
        w.population_centres.erase(cid);
        w.population_centre_tile.erase(cid);
        w.population_centre_name.erase(cid);
    }
}

/// One seed's razed roll call, for the R2 sweep.
///
/// WHY A SWEEP AT ALL. Razing is DESIGNED to be rare — an occupier prefers to
/// occupy (POPULATION.md § Growth, decline and razing) — so a single seed can
/// hand this check one razed region, and one pair is an anecdote rather than a
/// measurement. Widening the SEED spread is the honest way to get a sample
/// without touching the rarity, which is the design and not a defect.
struct razed_tally { int pairs = 0; int weaker = 0; int stronger = 0; int razed_regions = 0; };

razed_tally sweep_seed(uint32_t seed, bool print_rows)
{
    razed_tally t;
    world_params wp{};
    wp.seed = seed;
    wp.epoch_year = 0;

    generation_report rep{};
    world w = make_hard_coded_world(wp, &rep);
    const generation_report::body_entry* be = home_entry(rep);
    if (be == nullptr) return t;

    int gw = 0;
    for (const auto& [bid, bc] : w.bodies)
        if (bid == be->id) gw = bc.grid_width;
    if (gw <= 0) return t;

    const settlement_state& ss = be->settlement;
    if (ss.regions.empty()) return t;

    strip_centres(w, be->id);
    generate_population_centres(w, be->id, /*seed=*/seed ^ 0x70701001u, &ss);
    const std::vector<placed> ps = centres_of(w, be->id, ss, gw);

    std::vector<int> got(ss.regions.size(), 0), top(ss.regions.size(), 0);
    for (const placed& p : ps)
        if (p.region >= 0 && p.region < static_cast<int>(ss.regions.size()))
        {
            const std::size_t r = static_cast<std::size_t>(p.region);
            ++got[r];
            top[r] = std::max(top[r], p.scale);
        }

    for (std::size_t i = 0; i < ss.regions.size(); ++i)
    {
        const region& p = ss.regions[i];
        if (p.population <= 0 || p.centres_razed <= 0) continue;
        ++t.razed_regions;

        int best = -1, best_d = 1 << 30;
        for (std::size_t j = 0; j < ss.regions.size(); ++j)
        {
            const region& q = ss.regions[j];
            if (q.population <= 0 || q.centres_razed > 0) continue;
            const int d = std::abs(q.farm_q - p.farm_q);
            if (d < best_d) { best_d = d; best = static_cast<int>(j); }
        }
        if (best < 0) continue;
        const std::size_t b = static_cast<std::size_t>(best);
        if (print_rows)
            std::printf("   %08X  %6d  %5d  %7d  %3d  %10" PRId64
                        "  |  %6d  %7d  %3d  %10" PRId64 "\n",
                        seed, p.farm_q, p.centres_razed, got[i], top[i], p.urban_population,
                        ss.regions[b].farm_q, got[b], top[b], ss.regions[b].urban_population);
        ++t.pairs;
        if (got[i] < got[b] || top[i] < top[b]) ++t.weaker;
        if (got[i] > got[b] && top[i] > top[b]) ++t.stronger;
    }
    return t;
}

} // namespace

int main()
{
    // Seed ABCDEF01 — the seed BL-766's own urban baseline was measured on, so
    // the sim-grain figures printed here are directly comparable to it.
    world_params wp{};
    wp.seed = 0xABCDEF01u;
    wp.epoch_year = 0;

    generation_report rep_a{}, rep_b{};
    world wa = make_hard_coded_world(wp, &rep_a);
    world wb = make_hard_coded_world(wp, &rep_b);

    const auto* ka = home_entry(rep_a);
    const auto* kb = home_entry(rep_b);
    check(ka != nullptr && kb != nullptr, "homeworld entry present in both reports");
    if (ka == nullptr || kb == nullptr) return 1;

    // The home body's grid, from the world itself.
    entity_id home = null_entity;
    int gw = 0;
    for (const auto& [bid, bc] : wa.bodies)
        if (bid == ka->id) { home = bid; gw = bc.grid_width; }
    check(home != null_entity && gw > 0, "home body and grid width resolved");
    if (home == null_entity || gw <= 0) return 1;

    const settlement_state& ss = ka->settlement;
    const std::vector<region>& regions = ss.regions;
    check(!regions.empty() && ss.urban_map_drawn,
          "the home body carries a drawn urban map with regions");
    if (regions.empty()) return 1;

    // THE PASS UNDER TEST, IN ISOLATION. Strip the home body's centres from a
    // finished world and re-materialise them from the same settlement record,
    // so the binding is measured against `generate_population_centres` alone
    // rather than against the two later region-blind coverage passes.
    strip_centres(wa, home);
    generate_population_centres(wa, home, /*seed=*/wp.seed ^ 0x70701001u, &ss);
    const std::vector<placed> pa = centres_of(wa, home, ss, gw);

    // --- The sim-grain record, for the report header -----------------------
    int64_t sim_centres = 0, sim_razed = 0, sim_urban = 0;
    int living = 0;
    for (const region& p : regions)
    {
        sim_razed += p.centres_razed;
        if (p.population <= 0) continue;
        ++living;
        sim_centres += p.centres;
        sim_urban   += p.urban_population;
    }
    std::printf("\n-- sim grain (seed ABCDEF01, epoch 0) --\n");
    std::printf("   regions %d (%d living)   centres %" PRId64
                "   razed %" PRId64 "   urban heads %" PRId64 "\n",
                static_cast<int>(regions.size()), living, sim_centres, sim_razed, sim_urban);
    std::printf("   materialised centres on the home body: %d\n",
                static_cast<int>(pa.size()));

    // --- R1: bound to the region that grew it ------------------------------
    std::vector<int> got(regions.size(), 0);
    int unbound = 0; // a centre whose nearest region is not a region at all
    for (const placed& p : pa)
    {
        if (p.region < 0 || p.region >= static_cast<int>(regions.size())) { ++unbound; continue; }
        ++got[static_cast<std::size_t>(p.region)];
    }

    // HOW MUCH GROUND EACH REGION ACTUALLY HAS. A region cannot host more
    // cities than it has tiles able to take one, so part of any shortfall is
    // physical rather than a failure to bind. Same partition the pass uses:
    // the placement gate over the body's tiles, keyed by `nearest_region`.
    std::vector<int> cand(regions.size(), 0);
    for (const auto& [tid, tc] : wa.tiles)
    {
        if (tc.body != home) continue;
        if (!placement_rules::can_place_population_centre(tc)) continue;
        const int ri = nearest_region(ss, tc.grid_x, tc.grid_y, gw);
        if (ri >= 0 && ri < static_cast<int>(regions.size()))
            ++cand[static_cast<std::size_t>(ri)];
    }

    int exact = 0, over = 0, under = 0, drift = 0, floor_spill = 0;
    for (std::size_t i = 0; i < regions.size(); ++i)
    {
        const int want = (regions[i].population > 0) ? regions[i].centres : 0;
        const int have = got[i];
        floor_spill += std::max(0, want - cand[i]); // physically impossible here
        if (have == want) ++exact;
        else if (have > want) { ++over; drift += have - want; }
        else                  { ++under; drift += want - have; }
    }
    // Over- and under-counts are two views of the same displaced centres, so
    // the number of DISPLACED centres is half the summed absolute drift.
    const int displaced = drift / 2;

    std::printf("\n-- R1 binding --\n");
    std::printf("   regions matching their own centre count exactly: %d of %d\n",
                exact, static_cast<int>(regions.size()));
    std::printf("   over %d   under %d   displaced %d of %d centres (%.2f%%)\n",
                over, under, displaced, static_cast<int>(pa.size()),
                pa.empty() ? 0.0 : 100.0 * displaced / static_cast<double>(pa.size()));
    std::printf("   of which UNAVOIDABLE (region wants more cities than it has ground): %d\n",
                floor_spill);
    std::printf("   centres landing outside any region: %d\n", unbound);

    // The five worst-served regions, so a shortfall that is NOT ground
    // exhaustion shows up by name rather than hiding inside an aggregate.
    {
        std::vector<std::pair<int, int>> worst; // (shortfall, region)
        for (std::size_t i = 0; i < regions.size(); ++i)
        {
            const int want = (regions[i].population > 0) ? regions[i].centres : 0;
            if (want > got[i]) worst.push_back({ want - got[i], static_cast<int>(i) });
        }
        std::sort(worst.begin(), worst.end(),
                  [](const std::pair<int,int>& a, const std::pair<int,int>& b) {
                      if (a.first != b.first) return a.first > b.first;
                      return a.second < b.second;
                  });
        std::printf("   worst-served regions (wanted / got / own ground):\n");
        for (std::size_t k = 0; k < worst.size() && k < 5; ++k)
        {
            const std::size_t i = static_cast<std::size_t>(worst[k].second);
            std::printf("      region %4d: wanted %3d  got %3d  candidate tiles %3d\n",
                        worst[k].second, regions[i].centres, got[i], cand[i]);
        }
    }

    check(unbound == 0, "R1 every materialised centre falls inside some region");
    // THE CLAIM. A centre stands in the region that grew it unless that region
    // has no ground left to stand it on, in which case it spills to the
    // nearest region that has — counted, not dropped. So the displacement must
    // be explained by ground exhaustion, and a body-wide scatter (which is what
    // this pass did before BL-783) cannot pass: it would displace nearly every
    // centre while `floor_spill` stayed where it is.
    check(displaced <= floor_spill,
          "R1 every displaced centre is explained by its region running out of ground");
    check(exact * 4 >= static_cast<int>(regions.size()) * 3,
          "R1 at least three regions in four materialise their exact centre count");

    // --- R2: the causal story survives to the map --------------------------
    // Farming bands, so a sacked region is compared against ground of the same
    // quality rather than against the whole body.
    const int k_bands = 5;
    auto band_of = [&](int farm_q) {
        const int b = farm_q * k_bands / 1001;
        return std::clamp(b, 0, k_bands - 1);
    };

    struct agg { int regions = 0; int centres = 0; int top_scale_sum = 0; int64_t urban = 0; };
    std::map<std::pair<int, int>, agg> bands; // (band, sacked?) -> aggregate

    // Top scale materialised per region.
    std::vector<int> top(regions.size(), 0);
    for (const placed& p : pa)
        if (p.region >= 0 && p.region < static_cast<int>(regions.size()))
            top[static_cast<std::size_t>(p.region)] =
                std::max(top[static_cast<std::size_t>(p.region)], p.scale);

    for (std::size_t i = 0; i < regions.size(); ++i)
    {
        const region& p = regions[i];
        if (p.population <= 0) continue;
        const int sacked = (p.centres_razed > 0) ? 1 : 0;
        agg& a = bands[{ band_of(p.farm_q), sacked }];
        ++a.regions;
        a.centres       += got[i];
        a.top_scale_sum += top[i];
        a.urban         += p.urban_population;
    }

    std::printf("\n-- R2 sacked vs untouched, by farming band --\n");
    std::printf("   band   sacked  regions   mean centres   mean top scale   mean urban heads\n");
    for (int b = 0; b < k_bands; ++b)
        for (int s = 0; s <= 1; ++s)
        {
            const auto it = bands.find({ b, s });
            if (it == bands.end()) continue;
            const agg& a = it->second;
            std::printf("   %d      %s     %5d   %12.2f   %14.2f   %16" PRId64 "\n",
                        b, s ? "yes" : " no ", a.regions,
                        a.centres / static_cast<double>(a.regions),
                        a.top_scale_sum / static_cast<double>(a.regions),
                        a.urban / a.regions);
        }

    // The per-region roll call, because razing is DESIGNED to be rare (an
    // occupier prefers to occupy — POPULATION.md § Growth, decline and razing)
    // and an aggregate over a handful of regions hides more than it shows.
    std::printf("\n-- every sacked region, against its closest untouched match --\n");
    std::printf("   farm_q  razed  centres  top  urban       |  match farm_q  centres  top  urban\n");
    int pairs = 0, weaker = 0, stronger = 0;
    for (std::size_t i = 0; i < regions.size(); ++i)
    {
        const region& p = regions[i];
        if (p.population <= 0 || p.centres_razed <= 0) continue;

        // Closest untouched region by farm_q; ties to the lower region index,
        // so the pairing is a pure function of the record.
        int best = -1, best_d = 1 << 30;
        for (std::size_t j = 0; j < regions.size(); ++j)
        {
            const region& q = regions[j];
            if (q.population <= 0 || q.centres_razed > 0) continue;
            const int d = std::abs(q.farm_q - p.farm_q);
            if (d < best_d) { best_d = d; best = static_cast<int>(j); }
        }
        if (best < 0) continue;
        const region& m = regions[static_cast<std::size_t>(best)];
        std::printf("   %6d  %5d  %7d  %3d  %10" PRId64 "  |  %11d  %7d  %3d  %10" PRId64 "\n",
                    p.farm_q, p.centres_razed, got[i], top[i], p.urban_population,
                    m.farm_q, got[static_cast<std::size_t>(best)],
                    top[static_cast<std::size_t>(best)], m.urban_population);
        ++pairs;
        const bool fewer_or_smaller =
            got[i] < got[static_cast<std::size_t>(best)]
            || top[i] < top[static_cast<std::size_t>(best)];
        const bool more_and_larger =
            got[i] > got[static_cast<std::size_t>(best)]
            && top[i] > top[static_cast<std::size_t>(best)];
        if (fewer_or_smaller) ++weaker;
        if (more_and_larger)  ++stronger;
    }
    std::printf("   pairs %d:  sacked carries fewer or smaller %d,  more AND larger %d\n",
                pairs, weaker, stronger);

    // THE STRUCTURAL CLAIM, and the one that does not rest on a handful of
    // razings: the top scale a region materialises must RISE with its urban
    // headcount, which is exactly the quantity a sack cuts. If this holds, a
    // sacked region necessarily carries smaller cities than the same ground
    // unsacked, whether or not this seed happened to raze many.
    {
        int64_t lo_urban = 0, hi_urban = 0;
        int lo_n = 0, hi_n = 0, lo_top = 0, hi_top = 0;
        std::vector<int64_t> u;
        for (std::size_t i = 0; i < regions.size(); ++i)
            if (regions[i].population > 0 && got[i] > 0)
                u.push_back(regions[i].urban_population);
        std::sort(u.begin(), u.end());
        const int64_t median = u.empty() ? 0 : u[u.size() / 2];
        for (std::size_t i = 0; i < regions.size(); ++i)
        {
            if (regions[i].population <= 0 || got[i] <= 0) continue;
            if (regions[i].urban_population >= median)
            { ++hi_n; hi_top += top[i]; hi_urban += regions[i].urban_population; }
            else
            { ++lo_n; lo_top += top[i]; lo_urban += regions[i].urban_population; }
        }
        const double lo_mean = lo_n ? lo_top / static_cast<double>(lo_n) : 0.0;
        const double hi_mean = hi_n ? hi_top / static_cast<double>(hi_n) : 0.0;
        std::printf("\n-- R2 structural: top scale against urban headcount --\n");
        std::printf("   below median urban (%d regions, mean %" PRId64 " heads): mean top scale %.3f\n",
                    lo_n, lo_n ? lo_urban / lo_n : 0, lo_mean);
        std::printf("   above median urban (%d regions, mean %" PRId64 " heads): mean top scale %.3f\n",
                    hi_n, hi_n ? hi_urban / hi_n : 0, hi_mean);
        check(hi_mean > lo_mean,
              "R2 a region with more urban heads materialises a larger top city");
    }

    // --- R2 widened over three seeds ---------------------------------------
    {
        std::printf("\n-- R2 razed roll call over three seeds --\n");
        std::printf("   seed      farm_q  razed  centres  top  urban       "
                    "|  match farm_q  centres  top  urban\n");
        razed_tally t;
        t.pairs = pairs; t.weaker = weaker; t.stronger = stronger;
        t.razed_regions = pairs;
        for (std::size_t i = 0; i < regions.size(); ++i) // seed 1's rows, re-printed in the sweep's shape
        {
            const region& p = regions[i];
            if (p.population <= 0 || p.centres_razed <= 0) continue;
            int best = -1, best_d = 1 << 30;
            for (std::size_t j = 0; j < regions.size(); ++j)
            {
                const region& q = regions[j];
                if (q.population <= 0 || q.centres_razed > 0) continue;
                const int d = std::abs(q.farm_q - p.farm_q);
                if (d < best_d) { best_d = d; best = static_cast<int>(j); }
            }
            if (best < 0) continue;
            const std::size_t b = static_cast<std::size_t>(best);
            std::printf("   %08X  %6d  %5d  %7d  %3d  %10" PRId64
                        "  |  %6d  %7d  %3d  %10" PRId64 "\n",
                        wp.seed, p.farm_q, p.centres_razed, got[i], top[i], p.urban_population,
                        regions[b].farm_q, got[b], top[b], regions[b].urban_population);
        }
        for (const uint32_t s : { 0x13579BDFu, 0x2468ACE0u })
        {
            const razed_tally r = sweep_seed(s, /*print_rows=*/true);
            t.pairs += r.pairs; t.weaker += r.weaker; t.stronger += r.stronger;
            t.razed_regions += r.razed_regions;
        }
        std::printf("   pairs %d:  sacked carries fewer or smaller %d,  more AND larger %d\n",
                    t.pairs, t.weaker, t.stronger);
        check(t.pairs > 0, "R2 the sweep found at least one razed region to compare");
        check(t.stronger == 0,
              "R2 no sacked region outbuilds its untouched farming match in count AND size");
        check(t.weaker * 2 >= t.pairs,
              "R2 at least half of sacked regions carry fewer or smaller cities than their match");
    }

    // --- R2 the controlled experiment --------------------------------------
    // The roll call above is exact but thin: razing is rare BY DESIGN, and no
    // seed spread fixes that without touching the design. So sack the regions
    // directly and re-materialise the same world. This is the comparison the
    // item actually asks for — same ground, same farming quality, same
    // everything except the war — and it does not depend on a seed happening to
    // raze anything.
    {
        settlement_state ss2 = ss;
        std::vector<int> sacked_idx;
        for (std::size_t i = 0; i < ss2.regions.size(); ++i)
            if (got[i] >= 2)
            {
                // Every OTHER eligible region, so the rest stay as controls.
                if ((i % 2) != 0) continue;
                sack_region_urban(ss2.regions[i], /*population_loss_q=*/600);
                sacked_idx.push_back(static_cast<int>(i));
            }

        strip_centres(wa, home);
        generate_population_centres(wa, home, /*seed=*/wp.seed ^ 0x70701001u, &ss2);
        const std::vector<placed> p2 = centres_of(wa, home, ss2, gw);

        std::vector<int> got2(regions.size(), 0), top2(regions.size(), 0);
        for (const placed& p : p2)
            if (p.region >= 0 && p.region < static_cast<int>(regions.size()))
            {
                const std::size_t r = static_cast<std::size_t>(p.region);
                ++got2[r];
                top2[r] = std::max(top2[r], p.scale);
            }

        int fell = 0, rose = 0, flat = 0, before = 0, after = 0;
        for (const int i : sacked_idx)
        {
            const std::size_t r = static_cast<std::size_t>(i);
            before += got[r];
            after  += got2[r];
            if (got2[r] < got[r] || top2[r] < top[r]) ++fell;
            else if (got2[r] > got[r] || top2[r] > top[r]) ++rose;
            else ++flat;
        }
        // The controls, so a body-wide collapse cannot masquerade as the effect.
        int c_before = 0, c_after = 0, c_n = 0;
        for (std::size_t i = 0; i < regions.size(); ++i)
        {
            if (got[i] < 2 || (i % 2) == 0) continue;
            ++c_n; c_before += got[i]; c_after += got2[i];
        }

        std::printf("\n-- R2 controlled: sack half the city-bearing regions, re-materialise --\n");
        std::printf("   sacked %d regions at 600 per-mille countryside loss\n",
                    static_cast<int>(sacked_idx.size()));
        std::printf("   sacked   centres %d -> %d   fewer-or-smaller %d, unchanged %d, larger %d\n",
                    before, after, fell, flat, rose);
        std::printf("   control  centres %d -> %d over %d untouched regions\n",
                    c_before, c_after, c_n);
        std::printf("   sacked lose %.1f%% of their cities; controls %.1f%%\n",
                    before ? 100.0 * (before - after) / before : 0.0,
                    c_before ? 100.0 * (c_before - c_after) / c_before : 0.0);

        check(!sacked_idx.empty(), "R2 the controlled experiment had regions to sack");
        check(after < before,
              "R2 sacking a region costs it cities on the materialised map");
        check(fell > rose,
              "R2 far more sacked regions shrink than grow");
        // The controls move a little, and the reason is worth stating rather
        // than tolerating: a control's OWN `centres` is untouched by the sack,
        // but some of what it was carrying was SPILL from a neighbour that had
        // outgrown its ground. Sack that neighbour and it stops spilling, so a
        // control gives back borrowed cities. That is the binding working, not
        // a body-wide collapse — so the claim is that the control's loss is an
        // order of magnitude smaller than the sacked regions' own.
        check((c_before - c_after) * 10 <= (before - after),
              "R2 the loss is LOCAL: controls give up under a tenth of what the sacked lose");
    }

    // --- R3: deterministic, and the campaign path is seedless ---------------
    // (a) The WHOLE generation, twice: the centre table a player would see.
    {
        const std::vector<placed> full_a = centres_of(wb, kb->id, kb->settlement, gw);
        world wb2 = make_hard_coded_world(wp, nullptr);
        const std::vector<placed> full_b = centres_of(wb2, kb->id, kb->settlement, gw);
        bool identical = full_a.size() == full_b.size();
        for (std::size_t i = 0; identical && i < full_a.size(); ++i)
            if (full_a[i].raster != full_b[i].raster || full_a[i].scale != full_b[i].scale)
                identical = false;
        std::printf("\n-- R3 --\n   full-generation centres: %d vs %d\n",
                    static_cast<int>(full_a.size()), static_cast<int>(full_b.size()));
        check(identical, "R3 two generations on one seed place an identical centre table");
    }

    // (b) SEEDLESS: the campaign path must ignore the placement seed entirely,
    // so re-materialising the SAME world under a foreign seed reproduces the
    // same table. `wb` still holds its generated centres, so strip first.
    {
        strip_centres(wb, kb->id);
        generate_population_centres(wb, kb->id, /*seed=*/0x5EED5EEDu, &ss);
        const std::vector<placed> pc = centres_of(wb, kb->id, ss, gw);
        bool seedless = pc.size() == pa.size();
        for (std::size_t i = 0; seedless && i < pc.size(); ++i)
            if (pc[i].raster != pa[i].raster || pc[i].scale != pa[i].scale)
                seedless = false;
        check(seedless,
              "R3 the campaign placement path is SEEDLESS (a foreign seed changes nothing)");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
