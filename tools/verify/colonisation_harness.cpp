// ---------------------------------------------------------------------------
// colonisation_harness — BL-846 / BL-847 / BL-848 / BL-850.
// ---------------------------------------------------------------------------
//
// WHAT THIS HARNESS IS FOR, and what it deliberately refuses to do.
//
// It asserts STRUCTURE. `docs/generation/COLONISATION.md` § The span is
// explicit that every MAGNITUDE in this layer — the boundary year, the
// predation coefficient, the tiles-per-year rate — is `history_sweep`'s to
// argue and never a harness's, on the standing rule HISTORY.md § Settlement
// states: calibration is the sweep's, not the harness's. So nothing below
// asserts a tuned number. What it asserts is that the mechanisms have the
// SHAPE the design says they have:
//
//   C1  the walk is deterministic, twice, bit for bit
//   C2  no diffusion runs after the boundary year (the structural half of the
//       span's own definition)
//   C3  CULTURE ARRIVES BY ROUTE, NOT BY PROXIMITY — the god map is not
//       reproducible by a Voronoi of cradles. This is BL-848's whole
//       deliverable and it is the assertion this file exists for.
//   C4  ground no package suits is crossed but never farmable — emptiness is a
//       real outcome, not a failure to fill
//   C5  breadth spreads rather than clustering — the asymmetry generator
//       actually generates asymmetry
//   C6  crossing is a FLOORED UNION: a daughter is never better than the
//       better parent
//   C7  predation decays LOGARITHMICALLY: every doubling buys the same fixed
//       reduction, and it never reaches zero
//   C8  predation reads CURRENT population, so a sack re-wilds the ground
//   C9  the field's size is bounded and stated
//
// SYNTHETIC MAPS, ON PURPOSE, for C2-C4 and C6-C8. A generated world cannot
// isolate "the long way round by the coast beat the short way over the
// mountains" — it either happens to contain that case or it does not, and a
// check that cannot aim is not a check. So the route assertions run on a map
// built to contain exactly one mountain wall and exactly one coastal corridor.
// C5 and C9 then run against REAL generated worlds, because a spread and a size
// are only meaningful at the scale the game actually runs.
//
// Build: node tools/verify/build_harness.js colonisation_harness
// Run:   build_gen/verify/colonisation_harness.exe [seed_count]
// ---------------------------------------------------------------------------

#include "world/colonisation.hpp"
#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{

int g_failures = 0;

/// The outcome mix across every swept world — BL-851's reading, summed so the
/// spread is visible even where one seed happens to be uniform.
int g_outcome_total[5] = {0, 0, 0, 0, 0};

void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

/// A synthetic map: land everywhere, with the features each case needs written
/// onto it explicitly. Small enough to reason about by hand.
struct test_map
{
    int gw = 0, gh = 0;
    std::vector<terrain_substrate> substrate;
    std::vector<terrain_cover>     cover;
    std::vector<terrain_landform>  landform;

    test_map(int w, int h) : gw(w), gh(h)
    {
        const std::size_t n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
        substrate.assign(n, terrain_substrate::sedimentary);
        cover.assign(n, terrain_cover::grass);
        landform.assign(n, terrain_landform::plains);
    }

    std::size_t at(int col, int row) const
    {
        return static_cast<std::size_t>(row) * static_cast<std::size_t>(gw)
             + static_cast<std::size_t>(col);
    }

    colonisation_input input(int64_t boundary) const
    {
        colonisation_input in;
        in.substrate     = &substrate;
        in.cover         = &cover;
        in.landform      = &landform;
        in.gw            = gw;
        in.gh            = gh;
        in.boundary_year = boundary;
        return in;
    }
};

/// A package that farms everything, so a case testing ROUTES is not accidentally
/// testing affinity.
domestication_package omnivorous()
{
    domestication_package p;
    for (int i = 0; i < farm_class_count; ++i)
        p.affinity[static_cast<std::size_t>(i)] = 1000;
    p.breadth = static_cast<uint8_t>(farm_class_count);
    return p;
}

bool fields_identical(const colonisation_field& a, const colonisation_field& b)
{
    return a.gw == b.gw && a.gh == b.gh
        && a.arrival_year  == b.arrival_year
        && a.source_region == b.source_region
        && a.culture       == b.culture
        && a.farmable      == b.farmable;
}

int grid_dist_flat(int ax, int ay, int bx, int by, int gw)
{
    int dx = ax - bx;
    if (dx < 0) dx = -dx;
    if (dx > gw / 2) dx = gw - dx;
    int dy = ay - by;
    if (dy < 0) dy = -dy;
    return dx > dy ? dx : dy;
}

// ---------------------------------------------------------------------------
// C1 / C2 — determinism, and the boundary
// ---------------------------------------------------------------------------

void case_determinism_and_boundary()
{
    test_map m(60, 30);
    std::vector<colonisation_source> src;
    src.push_back(colonisation_source{static_cast<int32_t>(m.at(5, 15)), 0, 0, -4000,
                                      omnivorous()});
    src.push_back(colonisation_source{static_cast<int32_t>(m.at(50, 15)), 1, 1, -4000,
                                      omnivorous()});

    const colonisation_input in = m.input(-2000);
    const colonisation_field a = run_colonisation(in, src);
    const colonisation_field b = run_colonisation(in, src);

    check(fields_identical(a, b),
          "C1  the walk is deterministic — two runs of the same input agree bit for bit");

    bool past_boundary = false;
    int  reached = 0;
    for (std::size_t i = 0; i < a.arrival_year.size(); ++i)
    {
        if (a.arrival_year[i] == colonisation_never_reached) continue;
        ++reached;
        if (a.arrival_year[i] > -2000) past_boundary = true;
    }
    check(!past_boundary,
          "C2  NO DIFFUSION RUNS AFTER THE BOUNDARY — nothing arrives past the stated year");
    check(reached > 0, "C2b the walk actually reached ground (the case is not vacuous)");
}

// ---------------------------------------------------------------------------
// C3 — culture arrives by ROUTE, not by proximity. BL-848's deliverable.
// ---------------------------------------------------------------------------
//
// THE MAP. A mountain wall runs down column 30, floor to ceiling, EXCEPT for a
// coastal corridor along the bottom two rows where the wall is broken and the
// ground is shoreline (cheap). Cradle A sits west of the wall at (16, 4).
// Cradle B sits far to the south-east at (55, 28), on the corridor.
//
// The target tile is (34, 4): two tiles east of the wall, level with A. By
// STRAIGHT-LINE DISTANCE it is A's — 18 tiles from A against 24 from B. By
// ROUTE it is B's, because A must pay six times base cost to climb the wall
// while B walks the cheap shore and turns north over open ground.
//
// A'S POSITION IS LOAD-BEARING AND WAS WRONG ONCE. With A at (10, 4) the target
// is EQUIDISTANT under the wrapped Chebyshev metric — 24 either way — so C3a
// asserted a strict inequality that was a tie, and the case proved nothing about
// proximity even while C3 passed. The margin is now six tiles, not zero.
//
// If this passes, the god map is NOT a Voronoi of cradles. That is the whole
// claim.

void case_culture_by_route()
{
    test_map m(60, 30);

    // The wall.
    for (int row = 0; row < 30; ++row)
        for (int col = 28; col <= 32; ++col)
            m.landform[m.at(col, row)] = terrain_landform::mountain;

    // The corridor: the bottom two rows are ordinary ground, and the row below
    // them is water, so the corridor reads as shoreline and is cheap.
    for (int col = 0; col < 60; ++col)
    {
        m.landform[m.at(col, 28)] = terrain_landform::plains;
        m.landform[m.at(col, 27)] = terrain_landform::plains;
        m.substrate[m.at(col, 29)] = terrain_substrate::ocean;
    }

    std::vector<colonisation_source> src;
    src.push_back(colonisation_source{static_cast<int32_t>(m.at(16, 4)),  0, 0, -4000,
                                      omnivorous()});
    src.push_back(colonisation_source{static_cast<int32_t>(m.at(55, 28)), 1, 1, -4000,
                                      omnivorous()});

    const colonisation_field f = run_colonisation(m.input(0), src);

    const std::size_t target = m.at(34, 4);
    const int d_a = grid_dist_flat(34, 4, 16, 4,  m.gw);
    const int d_b = grid_dist_flat(34, 4, 55, 28, m.gw);

    check(d_a < d_b,
          "C3a the target is NEARER cradle A than cradle B (a Voronoi would give it to A)");
    check(f.culture[target] == 1,
          "C3  CULTURE ARRIVES BY ROUTE — the far-side ground is B's, because B's stream "
          "came the long way round by the coast and got there first");

    // And the same map read the other way: A still holds its own side of the
    // wall, so the route rule has not simply handed the world to whoever is
    // cheapest — it has handed each tile to whoever ARRIVED.
    check(f.culture[m.at(18, 4)] == 0,
          "C3b A still holds the ground on its own side of the wall");

    // The count that makes the claim quantitative rather than anecdotal: how
    // much of the map a Voronoi would have got wrong.
    int disagree = 0, land = 0;
    for (int row = 0; row < 30; ++row)
        for (int col = 0; col < 60; ++col)
        {
            const std::size_t i = m.at(col, row);
            if (is_water(m.substrate[i])) continue;
            if (f.culture[i] < 0) continue;
            ++land;
            const int da = grid_dist_flat(col, row, 16, 4,  m.gw);
            const int db = grid_dist_flat(col, row, 55, 28, m.gw);
            const int voronoi = da <= db ? 0 : 1;
            if (f.culture[i] != voronoi) ++disagree;
        }
    std::printf("      route vs Voronoi: %d of %d reached land tiles disagree (%d%%)\n",
                disagree, land, land ? (disagree * 100) / land : 0);
    check(disagree > 0,
          "C3c the route map is NOT reproducible by a Voronoi of cradles (the disagreement "
          "is non-empty)");
}

// ---------------------------------------------------------------------------
// C4 — ground no package suits is crossed, but never settled
// ---------------------------------------------------------------------------

void case_emptiness_is_an_outcome()
{
    test_map m(40, 20);
    // A band of forest across the middle. A grassland package crosses it — a
    // people walks through a wood — but cannot farm it.
    for (int row = 8; row <= 11; ++row)
        for (int col = 0; col < 40; ++col)
            m.cover[m.at(col, row)] = terrain_cover::forest;

    domestication_package grass_only;
    grass_only.affinity[static_cast<std::size_t>(farm_class::grassland)] = 900;
    grass_only.breadth = 1;

    std::vector<colonisation_source> src;
    src.push_back(colonisation_source{static_cast<int32_t>(m.at(20, 2)), 0, 0, -4000,
                                      grass_only});

    const colonisation_field f = run_colonisation(m.input(4000), src);

    const std::size_t wood = m.at(20, 9);
    const std::size_t past = m.at(20, 16);

    check(f.arrival_year[wood] != colonisation_never_reached,
          "C4a a stream CROSSES ground it cannot farm (the wood is reached)");
    check(f.farmable[wood] == 0,
          "C4  GROUND NO PACKAGE SUITS IS NOT SETTLED — the wood is reached and not farmable");
    check(f.arrival_year[past] != colonisation_never_reached && f.farmable[past] == 1,
          "C4b and the grassland beyond it IS farmable — crossing is not blocking");
}

// ---------------------------------------------------------------------------
// C6 — crossing is a floored union
// ---------------------------------------------------------------------------

void case_crossing_is_floored()
{
    domestication_package a, b;
    a.affinity[static_cast<std::size_t>(farm_class::grassland)]  = 800;
    a.affinity[static_cast<std::size_t>(farm_class::floodplain)] = 300;
    a.breadth = 2;
    b.affinity[static_cast<std::size_t>(farm_class::floodplain)] = 700;
    b.affinity[static_cast<std::size_t>(farm_class::woodland)]   = 400;
    b.breadth = 2;

    const domestication_package d = cross_packages(a, b);

    bool never_better = true;
    for (int i = 0; i < farm_class_count; ++i)
    {
        const std::size_t k = static_cast<std::size_t>(i);
        const uint16_t best = a.affinity[k] > b.affinity[k] ? a.affinity[k] : b.affinity[k];
        if (d.affinity[k] != best) never_better = false;
    }
    check(never_better,
          "C6  CROSSING IS A FLOORED UNION — a daughter is never better than the better parent");
    check(d.breadth == 3,
          "C6b and the daughter's breadth is the union's, so a stalled frontier can move again");
}

// ---------------------------------------------------------------------------
// C7 / C8 — predation
// ---------------------------------------------------------------------------

void case_predation()
{
    const int base = predation_base_q(1000, terrain_cover::forest, terrain_landform::plains);
    check(base > 0, "C7a dense forest on a living world carries real predation");
    check(predation_base_q(0, terrain_cover::forest, terrain_landform::plains) == 0,
          "C7b a world that never reached land animals carries NONE of it — not a little");

    // THE LOG SHAPE. Each doubling must buy the SAME fixed reduction, so the
    // successive differences are equal until the floor bites. This is the
    // assertion that distinguishes a logarithm from any other decay, and it is
    // the form Ben settled on 2026-09-09.
    const int p1  = predation_now_q(base, 1024,     predation_per_doubling_default_q);
    const int p2  = predation_now_q(base, 2048,     predation_per_doubling_default_q);
    const int p3  = predation_now_q(base, 4096,     predation_per_doubling_default_q);
    const int d12 = p1 - p2, d23 = p2 - p3;
    std::printf("      predation on forest: base %d, at 1k/2k/4k heads = %d/%d/%d "
                "(steps %d, %d)\n", base, p1, p2, p3, d12, d23);
    check(d12 > 0 && d23 > 0 && (d12 - d23 <= 1) && (d23 - d12 <= 1),
          "C7  THE DECAY IS LOGARITHMIC — each doubling of population buys the same "
          "fixed reduction");

    const int huge = predation_now_q(base, 1'000'000'000LL,
                                     predation_per_doubling_default_q);
    check(huge > 0,
          "C7c it is NEVER fully bought off — a log curve has no population at which it "
          "is done, so wild country stays marginally wild");

    // A SACK RE-WILDS THE GROUND. Predation reads CURRENT population, so losing
    // the heads that were holding the wild down raises it again — for free, out
    // of a rule written for something else.
    const int settled = predation_now_q(base, 500'000, predation_per_doubling_default_q);
    const int sacked  = predation_now_q(base,  20'000, predation_per_doubling_default_q);
    check(sacked > settled,
          "C8  CONQUEST RE-WILDS GROUND — predation rises again when a sack takes the heads");

    check(predation_capacity_mult_q(0) == 1000 && predation_capacity_mult_q(1000) < 1000,
          "C8b predation holds sustainable population below the ground's carrying capacity");
}

// ---------------------------------------------------------------------------
// C10 — the four cradle outcomes are SEPARATED (BL-851)
// ---------------------------------------------------------------------------
//
// Sessile-forever is a NORMAL and frequent outcome by design, so "N cradles
// stopped" carries no information at all: it cannot separate the design working
// from a cradle that was never viable. Each case below builds the ONE map that
// produces its outcome and nothing else, because that is the only way to show
// the four are actually distinguished rather than merely enumerated.

void case_cradle_outcomes()
{
    // --- STERILITY: the stream walks a long way and can farm none of it ----
    {
        test_map m(40, 20);
        for (std::size_t i = 0; i < m.cover.size(); ++i)
            m.cover[i] = terrain_cover::forest;      // Woodland everywhere...
        m.cover[m.at(20, 10)] = terrain_cover::grass; // ...except the anchor.

        domestication_package grass_only;
        grass_only.affinity[static_cast<std::size_t>(farm_class::grassland)] = 900;
        grass_only.breadth = 1;

        std::vector<colonisation_source> src{
            colonisation_source{static_cast<int32_t>(m.at(20, 10)), 0, 0, -4000, grass_only}};
        const colonisation_field f = run_colonisation(m.input(4000), src);
        const auto o = classify_cradle_outcomes(f, src, {});
        check(o[0] == cradle_outcome::sterility,
              "C10a STERILITY — the stream crossed real ground and could farm none of it");
    }

    // --- ENCIRCLEMENT: affinity is fine, but there is no way out -----------
    {
        test_map m(40, 20);
        // Ring the anchor in ocean: every exit is impassable, so the stream
        // never moves at all. Affinity is untouched — it farms everything.
        //
        // ONE TILE, NOT A 2x2 BLOCK, and that mattered: a two-by-two island
        // gives the source farmable ground BEYOND its own anchor, so it read as
        // SPREAD and the case tested nothing. An encircled cradle is one that
        // holds its anchor and nothing else.
        for (int row = 0; row < 20; ++row)
            for (int col = 0; col < 40; ++col)
                if (!(col == 20 && row == 10))
                    m.substrate[m.at(col, row)] = terrain_substrate::ocean;

        std::vector<colonisation_source> src{
            colonisation_source{static_cast<int32_t>(m.at(20, 10)), 0, 0, -4000, omnivorous()}};
        const colonisation_field f = run_colonisation(m.input(4000), src);
        const auto o = classify_cradle_outcomes(f, src, {});
        check(o[0] == cradle_outcome::encirclement,
              "C10b ENCIRCLEMENT — affinity is fine and the stream never left its anchor");
    }

    // --- DILUTION: a rival got to the ground this package could have used --
    {
        test_map m(40, 20);
        // Two sources side by side on identical open ground. The second starts
        // 2,000 years earlier, so it takes almost everything; the first is left
        // holding its anchor beside ground it could plainly have farmed.
        std::vector<colonisation_source> src{
            colonisation_source{static_cast<int32_t>(m.at(20, 10)), 0, 0,  -1000, omnivorous()},
            colonisation_source{static_cast<int32_t>(m.at(22, 10)), 1, 1,  -4000, omnivorous()}};
        const colonisation_field f = run_colonisation(m.input(4000), src);
        const auto o = classify_cradle_outcomes(f, src, {});
        check(o[0] == cradle_outcome::dilution,
              "C10c DILUTION — a rival's stream took ground this package could have farmed");
        check(o[1] == cradle_outcome::spread,
              "C10d and the rival that took it reads as SPREAD, not as anything else");
    }

    // --- THE PREDATION FLOOR: the only one that is death -------------------
    {
        test_map m(40, 20);
        std::vector<colonisation_source> src{
            colonisation_source{static_cast<int32_t>(m.at(20, 10)), 0, 0, -4000, omnivorous()}};
        const colonisation_field f = run_colonisation(m.input(4000), src);

        // Sustainable population below the density Stage 0 needs for surplus.
        std::vector<cradle_vitals> vitals{cradle_vitals{800, 2000}};
        const auto o = classify_cradle_outcomes(f, src, vitals);
        check(o[0] == cradle_outcome::predation_floor,
              "C10e THE PREDATION FLOOR outranks every other reading — a cradle that is both "
              "penned and dying is reported as dying");

        // And it is NOT reported when the caller supplied no population facts:
        // silence is the honest answer, not a zero that would read as 'nobody died'.
        const auto o2 = classify_cradle_outcomes(f, src, {});
        check(o2[0] != cradle_outcome::predation_floor,
              "C10f with no vitals supplied the floor is never reported (silence, not a "
              "silent zero)");
    }
}

// ---------------------------------------------------------------------------
// C5 / C9 — against real generated worlds
// ---------------------------------------------------------------------------

const generation_report::body_entry* kepler_of(const generation_report& r)
{
    for (const generation_report::body_entry& b : r.bodies)
        if (b.is_homeworld) return &b;
    return r.bodies.empty() ? nullptr : &r.bodies.front();
}

void case_real_worlds(int seed_count)
{
    std::printf("\n--- C5 / C9: against real generated worlds -------------------\n");

    int wide_enough = 0, worlds = 0;
    int64_t bytes_max = 0;

    for (int s = 0; s < seed_count; ++s)
    {
        world_params wp;
        wp.seed = static_cast<uint32_t>(s);
        wp.prehistory_years = 400; // The era must run for the fixture to be captured.

        generation_report     rep;
        era_minus_one_fixture fx;
        const world w = make_hard_coded_world(wp, &rep, world_gen_config{},
                                              nullptr, nullptr, &fx);
        (void)w;

        const generation_report::body_entry* k = kepler_of(rep);
        if (k == nullptr || !fx.ran)
        {
            std::printf("seed %u: the era did not run — skipped.\n", wp.seed);
            continue;
        }
        ++worlds;

        const std::vector<terrain_substrate>& sub = fx.terrain.substrate;
        const std::vector<terrain_cover>&     cov = fx.terrain.cover;
        const std::vector<terrain_landform>&  lf  = fx.terrain.landform;

        // COIN A PACKAGE AT EVERY SETTLED REGION'S ANCHOR. Real ground, real
        // windows — the spread below is the spread the game would actually get.
        std::vector<colonisation_source> src;
        std::vector<int> breadths;
        for (std::size_t ri = 0; ri < fx.settlement.regions.size(); ++ri)
        {
            const region& r = fx.settlement.regions[ri];
            const domestication_package pkg =
                coin_package(sub, cov, lf, fx.gw, fx.gh, r.col, r.row, /*window_radius=*/6);
            breadths.push_back(pkg.breadth);
            src.push_back(colonisation_source{
                static_cast<int32_t>(r.row * fx.gw + r.col),
                static_cast<int32_t>(ri), r.culture.plurality(), r.founded_year, pkg});
        }

        colonisation_input in;
        in.substrate     = &sub;
        in.cover         = &cov;
        in.landform      = &lf;
        in.gw            = fx.gw;
        in.gh            = fx.gh;
        in.boundary_year = 0;

        const colonisation_field f = run_colonisation(in, src);

        // BREADTH SPREAD — BL-847's "wide rather than clustered". The check is
        // that the distribution has real range, not that it hits a number.
        int bmin = 99, bmax = 0;
        int64_t bsum = 0;
        for (int b : breadths) { if (b < bmin) bmin = b; if (b > bmax) bmax = b; bsum += b; }
        const int bn = static_cast<int>(breadths.size());
        if (bn == 0) { std::printf("seed %u: no regions — skipped.\n", wp.seed); continue; }
        const int bmean = static_cast<int>(bsum / bn);
        if (bmax - bmin >= 3) ++wide_enough;

        int reached = 0, farmable = 0, land = 0;
        for (std::size_t i = 0; i < f.arrival_year.size(); ++i)
        {
            if (i < sub.size() && is_water(sub[i])) continue;
            ++land;
            if (f.arrival_year[i] != colonisation_never_reached) ++reached;
            if (f.farmable[i]) ++farmable;
        }

        // WHICH GROUND NOBODY CAN FARM (BL-859). The habitable share is a single
        // number and a single number cannot be acted on: "43% farmable" says the
        // map is too empty and not what to do about it. This breaks the
        // unfarmable remainder down by farm class, which turns an unbounded
        // tuning question -- lower the affinity floor until it looks full -- into
        // a bounded one: if one or two classes account for most of the gap, the
        // answer is what packages can farm THAT ground, and if it is spread
        // evenly across all twelve then breadth itself is the constraint.
        //
        // REPORTED, NEVER ASSERTED. Which classes go unfarmed is a property of
        // the world and the sweep's to argue, exactly like every other magnitude
        // in this layer.
        {
            int unfarmed[farm_class_count] = {};
            int reached_by_class[farm_class_count] = {};
            for (std::size_t i = 0; i < f.arrival_year.size(); ++i)
            {
                if (i < sub.size() && is_water(sub[i])) continue;
                if (f.arrival_year[i] == colonisation_never_reached) continue;
                const int fc = static_cast<int>(f.ground[i]);
                if (fc < 0 || fc >= farm_class_count) continue;
                ++reached_by_class[fc];
                if (!f.farmable[i]) ++unfarmed[fc];
            }
            static const char* nm[farm_class_count] = {
                "floodplain","grassland","woodland","valley","highland","montane",
                "steppe","arid","coastal","volcanic","boreal","stone"};
            std::printf("        unfarmed by class:");
            for (int c = 0; c < farm_class_count; ++c)
                if (unfarmed[c] > 0)
                    std::printf(" %s %d/%d", nm[c], unfarmed[c], reached_by_class[c]);
            std::printf("\n");
        }

        const int64_t bytes = colonisation_field_bytes(f);
        if (bytes > bytes_max) bytes_max = bytes;

        std::printf("seed %u  regions %d  breadth min/mean/max %d/%d/%d  "
                    "land %d  reached %d (%d%%)  farmable %d (%d%%)  field %lld KB\n",
                    wp.seed, bn, bmin, bmean, bmax, land,
                    reached, land ? (reached * 100) / land : 0,
                    farmable, land ? (farmable * 100) / land : 0,
                    static_cast<long long>(bytes / 1024));

        // BL-851's DELIVERABLE, and the reason it is printed rather than
        // asserted: the outcome MIX is what history_sweep argues the boundary
        // year and the predation coefficient against, and a harness that pinned
        // it would be asserting a magnitude this layer puts in the sweep's
        // hands. What IS asserted below is that the four are separated at all.
        std::vector<cradle_vitals> vitals;
        vitals.reserve(fx.settlement.regions.size());
        for (const region& r : fx.settlement.regions)
        {
            const int pred = predation_base_q(
                1000,
                (static_cast<std::size_t>(r.row * fx.gw + r.col) < cov.size())
                    ? cov[static_cast<std::size_t>(r.row * fx.gw + r.col)] : terrain_cover::none,
                (static_cast<std::size_t>(r.row * fx.gw + r.col) < lf.size())
                    ? lf[static_cast<std::size_t>(r.row * fx.gw + r.col)] : terrain_landform::plains);
            const int64_t cap = region_carrying_capacity(r.farm_q);
            const int64_t sustainable =
                (cap * predation_capacity_mult_q(
                           predation_now_q(pred, r.population,
                                           predation_per_doubling_default_q))) / 1000;
            // Stage 0's density requirement, as the founding band: a people
            // that cannot hold what it was founded with raises no surplus.
            vitals.push_back(cradle_vitals{sustainable, 2000});
        }

        const std::vector<cradle_outcome> oc =
            classify_cradle_outcomes(f, src, vitals);
        int tally[5] = {0, 0, 0, 0, 0};
        for (cradle_outcome o : oc) ++tally[static_cast<int>(o)];
        std::printf("        outcomes: spread %d  sterility %d  encirclement %d  "
                    "dilution %d  PREDATION FLOOR %d\n",
                    tally[0], tally[1], tally[2], tally[3], tally[4]);
        if (tally[4] > 0)
            std::printf("        NOTE: a cradle on the predation floor is a finding about "
                        "CRADLE SELECTION (agrarian_score reads predation), not about the span.\n");
        for (int k = 0; k < 5; ++k) g_outcome_total[k] += tally[k];
    }

    if (worlds == 0)
    {
        check(false, "C5  no world ran — the real-world cases are vacuous");
        return;
    }

    // BL-851's DONE-WHEN: the four outcomes are reported SEPARATELY, and the
    // reading distinguishes a cradle that filled its window from one that never
    // had one. Asserted as "more than one outcome occurs across the sweep" —
    // a classifier that answered `spread` for everything would report the same
    // no-information the item was written about.
    int kinds = 0;
    for (int k = 0; k < 5; ++k) if (g_outcome_total[k] > 0) ++kinds;
    std::printf("      outcome mix across the sweep: spread %d, sterility %d, "
                "encirclement %d, dilution %d, predation floor %d (%d distinct)\n",
                g_outcome_total[0], g_outcome_total[1], g_outcome_total[2],
                g_outcome_total[3], g_outcome_total[4], kinds);
    std::printf(
                "      READ THAT MIX WITH ITS SCALE IN MIND. Every SETTLED REGION is a source\n"
                "      here (149-195 of them), not the handful of CRADLES the span will actually\n"
                "      start from, so dilution dominates by construction: with two hundred\n"
                "      streams on one map almost every one meets a rival before it meets a wall.\n"
                "      Sterility reading ZERO is a consequence of the same thing - a region-grain\n"
                "      package is coined on the ground it already sits on, so it can always farm\n"
                "      home. The MIX is history_sweep's to argue once the span runs from cradles;\n"
                "      what this case establishes is that the four are SEPARABLE at all.\n");

    check(kinds >= 2,
          "C10 THE FOUR OUTCOMES ARE SEPARATED on real worlds — the sweep can tell a cradle "
          "that filled its window from one that never had one");

    check(wide_enough * 2 >= worlds,
          "C5  BREADTH SPREADS RATHER THAN CLUSTERING — most worlds show a range of at "
          "least 3 farm classes between their narrowest and broadest package");

    // C9: the field is one entry per tile on a fixed grid, so its size is a
    // function of the grid alone and cannot grow with the history. STATED, the
    // way owner_ring_bytes states the time-lapse's.
    std::printf("      field size at the homeworld grid: %lld KB "
                "(%d bytes/tile, fixed — it cannot grow with the history)\n",
                static_cast<long long>(bytes_max / 1024),
                static_cast<int>(sizeof(int64_t) + sizeof(int32_t) * 2
                                 + sizeof(farm_class) + sizeof(uint8_t)));
    check(bytes_max > 0 && bytes_max < 4 * 1024 * 1024,
          "C9  the field's size is bounded and stated (under 4 MB on a homeworld grid)");
}


// ---------------------------------------------------------------------------
// C11 — BL-848's ACCEPTANCE TEST, on real generated worlds
// ---------------------------------------------------------------------------
//
// C3 proves culture-by-route on a map BUILT to contain the case. This proves it
// on the worlds the game actually generates, which is what BL-848's done-when
// asks for: "a harness shows the god map is not reproducible by a Voronoi of
// cradles."
//
// It reads generation's OWN settlement -- the regions run_settlement produced
// after the change -- and asks two questions of it. First, how many regions
// carry a culture that a nearest-cradle assignment would NOT have given them;
// that share is the whole of what "earned rather than assigned" buys. Second,
// whether founding dates SPREAD across the span rather than clustering, since a
// route-derived date that still lands everything in one century would mean the
// walk is not costing anything.

void case_route_on_real_worlds(int seed_count)
{
    std::printf("\n--- C11: the god map against a Voronoi, on real worlds --------\n");

    int worlds = 0, worlds_disagreeing = 0;

    for (int s = 0; s < seed_count; ++s)
    {
        world_params wp;
        wp.seed = static_cast<uint32_t>(s);
        wp.prehistory_years = 400;

        generation_report     rep;
        era_minus_one_fixture fx;
        const world w = make_hard_coded_world(wp, &rep, world_gen_config{},
                                              nullptr, nullptr, &fx);
        (void)w;
        const generation_report::body_entry* k = kepler_of(rep);
        if (k == nullptr || !fx.ran) continue;
        ++worlds;

        // EACH CULTURE'S ORIGIN, RECOVERED FROM THE SETTLEMENT ITSELF: the
        // region of that culture founded EARLIEST is where its stream started.
        //
        // This is a stronger test than comparing against the ladder's cradle
        // coordinates would have been, and it is also the only one available -
        // generation_report does not carry the ladder. It asks: given the
        // origins the map ITSELF shows, does a nearest-origin assignment
        // reproduce the culture map? If it does not, the distribution is a
        // record of routes rather than a partition of space, which is exactly
        // BL-848's claim.
        constexpr int max_cultures = 64;
        int   org_col[max_cultures], org_row[max_cultures];
        int64_t org_year[max_cultures];
        bool  org_has[max_cultures];
        for (int i = 0; i < max_cultures; ++i) { org_has[i] = false; org_year[i] = 0; }

        for (const region& r : fx.settlement.regions)
        {
            const int c = r.culture.plurality();
            if (c < 0 || c >= max_cultures) continue;
            if (!org_has[c] || r.founded_year < org_year[c])
            {
                org_has[c]  = true;
                org_year[c] = r.founded_year;
                org_col[c]  = r.col;
                org_row[c]  = r.row;
            }
        }

        int disagree = 0, scored = 0;
        int64_t ymin = 1LL << 40, ymax = -(1LL << 40);

        for (const region& r : fx.settlement.regions)
        {
            if (r.founded_year < ymin) ymin = r.founded_year;
            if (r.founded_year > ymax) ymax = r.founded_year;

            int best_c = -1, best_d = 1 << 30;
            for (int c = 0; c < max_cultures; ++c)
            {
                if (!org_has[c]) continue;
                const int d = grid_dist_flat(r.col, r.row, org_col[c], org_row[c], fx.gw);
                if (d < best_d) { best_d = d; best_c = c; }
            }
            if (best_c < 0) continue;
            ++scored;
            if (r.culture.plurality() != best_c) ++disagree;
        }

        if (scored == 0) continue;
        if (disagree > 0) ++worlds_disagreeing;

        std::printf("seed %u  regions %d  route disagrees with a Voronoi on %d (%d%%)  "
                    "founded_year %lld .. %lld (span %lld yr)\n",
                    wp.seed, scored, disagree, (disagree * 100) / scored,
                    static_cast<long long>(ymin), static_cast<long long>(ymax),
                    static_cast<long long>(ymax - ymin));
    }

    if (worlds == 0) { check(false, "C11 no world ran - the case is vacuous"); return; }

    check(worlds_disagreeing == worlds,
          "C11 THE GOD MAP IS NOT A VORONOI OF CRADLES on any generated world - culture is "
          "earned by arrival, not assigned by distance (BL-848's acceptance test)");
}


// ---------------------------------------------------------------------------
// C12 — THE FOUNDING SCHEDULE FILLS THE SPAN (BL-846)
// ---------------------------------------------------------------------------
//
// The acceptance test for "phase 4 should not read as combined colonisation and
// conquest" (Ben, 2026-09-09). Everything else in this file tests the walk; this
// tests whether the walk's schedule actually reaches the RECORD, which is the
// only thing a player ever sees.
//
// WHAT IT ASKS. Build a world at a span long enough to contain the diffusion,
// then read the recorded ownership changes and ask WHEN they happened. If every
// change is dated to the run's first year, the map was full at tick zero and the
// time-lapse has no origin -- which is precisely the defect this item exists to
// close. What we want is a substantial share of foundings landing INSIDE the
// span, spread across it.
//
// IT REPORTS THE SHAPE RATHER THAN ASSERTING A MAGNITUDE. How many foundings
// fall in which century is history_sweep's to argue; what a harness may assert
// is that the span is not degenerate -- that the record contains a filling
// phase at all.

void case_founding_schedule(int span_years)
{
    std::printf("\n--- C12: does the record contain the world filling? -----------\n");

    world_params wp;
    wp.seed             = 0;
    wp.prehistory_years = span_years;

    generation_report rep;
    const world w = make_hard_coded_world(wp, &rep, world_gen_config{}, nullptr, nullptr,
                                          nullptr);
    (void)w;

    const generation_report::body_entry* k = kepler_of(rep);
    if (k == nullptr) { check(false, "C12 no homeworld -- the case is vacuous"); return; }

    const era_timelapse& t = k->prehistory_timelapse;
    if (t.empty()) { check(false, "C12 the era recorded nothing"); return; }

    const int64_t first = t.start_year;
    const int64_t last  = t.start_year + t.years;

    // How many ownership changes are dated to the very first year -- the world
    // as it stood before the sim began -- against those that happened during it.
    int at_open = 0, during = 0;
    for (const owner_change& c : t.changes)
        if (c.year <= first) ++at_open; else ++during;

    // And the spread: how many DISTINCT years carry a change. A record whose
    // changes all land in one or two years is a jump cut, not a time-lapse.
    std::vector<int32_t> years;
    for (const owner_change& c : t.changes)
        if (c.year > first) years.push_back(c.year);
    std::sort(years.begin(), years.end());
    years.erase(std::unique(years.begin(), years.end()), years.end());

    std::printf("span %lld yr (%lld -> %lld)  changes %d  at the open %d  during %d "
                "(%d%%)  across %d distinct years\n",
                static_cast<long long>(span_years),
                static_cast<long long>(first), static_cast<long long>(last),
                static_cast<int>(t.changes.size()), at_open, during,
                t.changes.empty() ? 0
                                  : static_cast<int>((during * 100) / t.changes.size()),
                static_cast<int>(years.size()));
    std::printf("      regions at the epoch %d   foundings reported %lld\n",
                static_cast<int>(k->settlement.regions.size()),
                static_cast<long long>(rep.prehistory_foundings));

    check(during > 0,
          "C12  THE RECORD CONTAINS THE WORLD FILLING -- ownership changes happen "
          "DURING the span, not only at its opening frame");
    check(static_cast<int>(years.size()) >= 10,
          "C12b and they are spread across the span rather than bunched into a jump cut");

    // THE SETTLEMENT IS COMPLETE BY THE EPOCH. The schedule is drained by the
    // sim, never carried past it -- which is what keeps it off the save seam.
    check(k->settlement.pending_foundings.empty(),
          "C12c the founding schedule is fully drained by the epoch (nothing is left "
          "un-founded, so no half-built settlement reaches the report)");
}


// ---------------------------------------------------------------------------
// C13 — MIGRATION SPAWNS CULTURES (BL-856)
// ---------------------------------------------------------------------------
//
// The acceptance test for Ben's "spawn new cultures". A homeworld that ends with
// its cradle count and no more is a map of where agriculture started; one that
// ends with a FAMILY of related peoples grouped by route is a map of a
// migration, which is what the round is for.
//
// It asserts the SHAPE and reports the magnitude: how many peoples a world ends
// with is history_sweep's to argue, exactly like the 600-year split threshold
// that produces it.

void case_spawned_cultures(int seed_count)
{
    std::printf("\n--- C13: does the migration coin new peoples? -----------------\n");

    int worlds = 0, worlds_spawning = 0;

    for (int s = 0; s < seed_count; ++s)
    {
        world_params wp;
        wp.seed = static_cast<uint32_t>(s);
        wp.prehistory_years = 400;

        generation_report     rep;
        era_minus_one_fixture fx;
        const world w = make_hard_coded_world(wp, &rep, world_gen_config{}, nullptr, nullptr, &fx);
        (void)w;
        if (!fx.ran) continue;
        ++worlds;

        const int spawned = static_cast<int>(fx.settlement.spawned_cultures.size());
        const int total   = static_cast<int>(fx.creeds.cultures.size());
        const int cradles = total - spawned;
        if (spawned > 0) ++worlds_spawning;

        // HOW MANY DISTINCT PEOPLES ACTUALLY HOLD GROUND -- the number that
        // matters, since a culture nobody carries is a record and not a people.
        std::vector<int> held;
        for (const region& r : fx.settlement.regions)
        {
            const int c = r.culture.plurality();
            if (c < 0) continue;
            bool seen = false;
            for (int h : held) if (h == c) { seen = true; break; }
            if (!seen) held.push_back(c);
        }

        std::printf("seed %u  cradle cultures %d  +%d coined by migration = %d  "
                    "distinct peoples holding ground %d\n",
                    wp.seed, cradles, spawned, total, static_cast<int>(held.size()));

        // NAMES STAY IN THE FAMILY AND STAY COINED. Print a couple so a human
        // can see they are sci-fi/fantasy out of the seeded banks and read as
        // kin to their parent -- never an Earth proper noun, which is exactly
        // where culture naming is most tempted.
        for (int i = 0; i < spawned && i < 3; ++i)
            std::printf("        coined: %s\n",
                        fx.settlement.spawned_cultures[static_cast<std::size_t>(i)].name.c_str());

        // Every daughter must be a REAL record, not a default-constructed hole.
        // This is the check on the parent-resolution order: a daughter whose
        // parent is itself a daughter is read out of a vector still being
        // appended to, and gets an empty name if that order is ever wrong.
        int empty_named = 0;
        for (const culture& c : fx.settlement.spawned_cultures)
            if (c.name.empty()) ++empty_named;
        check(empty_named == 0,
              "C13a every coined culture is a real derived record, not an empty hole "
              "(the daughter-of-a-daughter resolution order holds)");
    }

    if (worlds == 0) { check(false, "C13 no world ran - the case is vacuous"); return; }

    check(worlds_spawning == worlds,
          "C13  THE MIGRATION COINS NEW PEOPLES on every generated world - a homeworld "
          "ends with a family of related cultures, not just its cradles");
}


// ---------------------------------------------------------------------------
// C14 — THE FAMILY TREE SURVIVES THE MIGRATION (BL-865)
// ---------------------------------------------------------------------------
//
// The migration builds a tree of peoples and used to throw it away, so by the
// time the empire phase ran, the fact that two peoples were cousins was
// unrecoverable. CIVILISATION.md makes kinship the substrate for how alike two
// cultures are, so this asserts the tree is actually there and actually walkable.
//
// IT ALSO ASSERTS TERMINATION RATHER THAN HOPING FOR IT. Ids are handed out in
// arrival order, so a daughter's parent is always at a LOWER index and a walk
// toward the root strictly decreases. That is what makes a cycle impossible, and
// it is worth checking rather than trusting: if the allocation order ever
// changed, this walk is where it would hang.

void case_family_tree(int seed_count)
{
    std::printf("\n--- C14: does the family tree survive the migration? ----------\n");

    int worlds = 0, worlds_ok = 0;

    for (int s = 0; s < seed_count; ++s)
    {
        world_params wp;
        wp.seed = static_cast<uint32_t>(s);
        wp.prehistory_years = 4000;

        generation_report     rep;
        era_minus_one_fixture fx;
        const world w = make_hard_coded_world(wp, &rep, world_gen_config{}, nullptr, nullptr, &fx);
        (void)w;
        if (!fx.ran) continue;
        ++worlds;

        const std::vector<culture>& cs = fx.creeds.cultures;
        int rooted = 0, deepest = 0, with_class = 0, monotonic = 1;
        int with_year = 0, time_monotonic = 1, roots_at_span_start = 1;
        int deepest_hops = -1;
        int64_t deepest_years = 0;

        for (std::size_t i = 0; i < cs.size(); ++i)
        {
            if (cs[i].origin_farm_class >= 0) ++with_class;
            if (cs[i].coined_year != INT64_MIN) ++with_year;

            // Walk to the root, bounded by the culture count so a broken tree
            // fails the assertion rather than hanging the harness.
            int depth = 0, at = static_cast<int>(i);
            std::size_t guard = 0;
            while (at >= 0 && cs[static_cast<std::size_t>(at)].parent >= 0
                   && guard++ <= cs.size())
            {
                const int up = cs[static_cast<std::size_t>(at)].parent;
                if (up >= at) monotonic = 0;   // must strictly decrease
                // TIME MOVES FORWARD DOWN THE TREE: a parent's coining year
                // must be no later than its child's (NR-816 — kinship as years
                // since the common ancestor is worthless if time can run backward).
                if (cs[static_cast<std::size_t>(up)].coined_year
                    > cs[static_cast<std::size_t>(at)].coined_year)
                    time_monotonic = 0;
                at = up;
                ++depth;
            }
            const bool this_rooted = at >= 0 && cs[static_cast<std::size_t>(at)].parent < 0;
            if (this_rooted)
            {
                ++rooted;
                // THE CRADLE-YEAR CHECK (BL-873 DONE-WHEN): the root of every
                // walk must carry the span's start, never a sentinel.
                if (cs[static_cast<std::size_t>(at)].coined_year != colonisation_start_year)
                    roots_at_span_start = 0;
            }
            if (depth > deepest && this_rooted)
            {
                deepest        = depth;
                deepest_hops   = depth;
                // YEARS SINCE THE COMMON ANCESTOR (NR-816) — the actual measure
                // hop count was standing in for. Printed alongside the hop count
                // so the two are visible side by side.
                deepest_years  = cs[i].coined_year - cs[static_cast<std::size_t>(at)].coined_year;
            }
        }

        std::printf("seed %u  cultures %d  reach a cradle %d  deepest descent %d  "
                    "carry an origin class %d  carry a coined year %d  "
                    "deepest hops %d  deepest years %lld\n",
                    wp.seed, static_cast<int>(cs.size()), rooted, deepest, with_class,
                    with_year, deepest_hops, static_cast<long long>(deepest_years));

        if (rooted == static_cast<int>(cs.size()) && monotonic && time_monotonic
            && roots_at_span_start
            && with_class == static_cast<int>(cs.size())
            && with_year == static_cast<int>(cs.size()))
            ++worlds_ok;
    }

    if (worlds == 0) { check(false, "C14 no world ran - the case is vacuous"); return; }

    check(worlds_ok == worlds,
          "C14  THE FAMILY TREE SURVIVES: every culture walks back to a cradle at the span's "
          "start, parents are strictly lower-indexed and no later in time than their children, "
          "and every people records both the country and the year it was coined on");
}

} // namespace

int main(int argc, char** argv)
{
    const int seed_count = argc > 1 ? std::atoi(argv[1]) : 3;
    const int span_years = argc > 2 ? std::atoi(argv[2]) : 4000;

    std::printf("\n=== colonisation_harness — BL-846/847/848/850 ===\n");
    std::printf("Asserts STRUCTURE only. Every magnitude in this layer is history_sweep's\n"
                "to argue, never a harness's (COLONISATION.md; HISTORY.md sec Settlement).\n\n");

    case_determinism_and_boundary();
    case_culture_by_route();
    case_emptiness_is_an_outcome();
    case_crossing_is_floored();
    case_predation();
    case_cradle_outcomes();
    case_real_worlds(seed_count);
    case_route_on_real_worlds(seed_count);
    case_founding_schedule(span_years);
    case_spawned_cultures(seed_count);
    case_family_tree(seed_count);

    std::printf("\n=== colonisation_harness: %d failure(s) ===\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
