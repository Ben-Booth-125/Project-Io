// ---------------------------------------------------------------------------
// sim_water_census — BL-755 and BL-756, the "measure first" both items demand.
//
// Two claims were made about the Era -1 sim while scoping the sea-leg work, and
// both were read out of the code rather than measured:
//
//   BL-755  region adjacency is Chebyshev at neighbour_radius and WATER-BLIND,
//           so a campaign can already cross open ocean for free. Sea reach is
//           unpriced, not absent — and every tuning constant in
//           history_sim_params was measured with that happening.
//   BL-756  the Settle verb applies no terrain test, so a region can be founded
//           ON water, where terrain_combat returns 0 defence and 0 forage. It
//           would be silently undefendable, and nothing reports it.
//
// Both items say to MEASURE BEFORE ACTING, because the size of the number
// decides the design: if founding-on-water never happens, BL-756's fix is a free
// guard; if it happens constantly, the fix moves every world and becomes Ben's
// call. Likewise BL-755 — the "before" figure for any sea-leg work is this
// number, and it is not zero by assumption.
//
// It measures generation's OWN run via era_minus_one_fixture, not a
// reconstruction (the BL-462 discipline, and BL-757's lesson).
//
// REPORTS, NEVER GATES. There is no target for any of these counts.
// ---------------------------------------------------------------------------

#include "world/components.hpp"
#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/settlement.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

const generation_report::body_entry* kepler_of(const generation_report& r)
{
    for (const generation_report::body_entry& b : r.bodies)
        if (b.is_homeworld) return &b;
    return r.bodies.empty() ? nullptr : &r.bodies.front();
}

/// The sim's own adjacency metric: Chebyshev, columns wrapped, rows not.
/// Restated here rather than reached for, because history_sim.cpp's copy is in
/// an anonymous namespace — and restating it is safe only because it is four
/// lines with no state. If it ever grows, promote it instead.
int chebyshev(int ca, int ra, int cb, int rb, int gw)
{
    int dc = ca - cb; if (dc < 0) dc = -dc;
    if (gw > 0 && dc > gw / 2) dc = gw - dc;
    int dr = ra - rb; if (dr < 0) dr = -dr;
    return dc > dr ? dc : dr;
}

struct seed_row
{
    std::uint32_t seed = 0;
    int regions = 0;
    int on_water = 0;        ///< anchor is any water (is_water — lakes included)
    int on_sea = 0;          ///< anchor is sea proper (is_sea — lakes excluded)
    int on_open_ocean = 0;
    long long edges = 0;         ///< adjacency pairs inside neighbour_radius
    long long edges_over_water = 0;
    long long edges_over_sea = 0;
    /// BL-777: the STORED `region::domain`, tallied independently of the
    /// substrate columns above, plus the count of regions where the two
    /// disagree. The field is written once at founding and never recomputed,
    /// so "it cannot desynchronise" is a claim worth checking rather than one
    /// to assert in a comment.
    int dom_land = 0;
    int dom_coastal = 0;
    int dom_open = 0;
    int dom_mismatch = 0;
};

} // namespace

int main(int argc, char** argv)
{
    int seeds = 3;
    for (int i = 1; i < argc; ++i)
        if (std::atoi(argv[i]) > 0) seeds = std::atoi(argv[i]);

    std::printf("=== sim_water_census (BL-755 / BL-756) — %d seed%s, generation's own era ===\n\n",
                seeds, seeds == 1 ? "" : "s");

    const history_sim_params defaults;
    const int radius = defaults.neighbour_radius;
    std::printf("     neighbour_radius %d (the range a campaign target may sit at)\n\n", radius);

    std::vector<seed_row> rows;
    bool all_ran = true;

    for (int s = 0; s < seeds; ++s)
    {
        world_params wp;
        wp.seed = static_cast<std::uint32_t>(s);

        generation_report     rep;
        era_minus_one_fixture fx;
        const world w = make_hard_coded_world(wp, &rep, world_gen_config{},
                                              /*progress=*/nullptr, /*works=*/nullptr, &fx);
        (void)w;

        const generation_report::body_entry* k = kepler_of(rep);
        if (!k || !fx.ran) { all_ran = false; continue; }

        // The settlement AFTER the sim — this asks what the run LEFT standing,
        // which is what the campaign inherits and what BL-756 is about.
        const settlement_state& ss = k->settlement;
        const sim_terrain_view  tv = fx.terrain.view();
        if (tv.substrate == nullptr) { all_ran = false; continue; }
        const std::vector<terrain_substrate>& sub = *tv.substrate;

        seed_row row;
        row.seed    = wp.seed;
        row.regions = static_cast<int>(ss.regions.size());

        for (const region& p : ss.regions)
        {
            const std::size_t a = static_cast<std::size_t>(p.anchor);
            if (a >= sub.size()) continue;
            if (is_water(sub[a]))      ++row.on_water;
            if (is_sea(sub[a]))        ++row.on_sea;
            if (is_open_ocean(sub[a])) ++row.on_open_ocean;

            // BL-777. Two INDEPENDENT reads of one fact: what the ground IS
            // (the substrate, above) and what the region SAYS it is (the
            // stored field). W3 binds them together.
            switch (p.domain)
            {
                case region_domain::land:          ++row.dom_land; break;
                case region_domain::coastal_water: ++row.dom_coastal; break;
                case region_domain::open_ocean:    ++row.dom_open; break;
            }
            if (p.domain != region_domain_of(sub[a])) ++row.dom_mismatch;
        }

        // BL-755's precondition: adjacency pairs whose straight line crosses
        // water. A campaign can only cross water where an EDGE does, so this
        // bounds how much free sea reach the sim actually has. Sampled along the
        // line rather than pathfound — the sim has no path either.
        for (std::size_t i = 0; i < ss.regions.size(); ++i)
            for (std::size_t j = i + 1; j < ss.regions.size(); ++j)
            {
                const region& A = ss.regions[i];
                const region& B = ss.regions[j];
                if (chebyshev(A.col, A.row, B.col, B.row, fx.gw) > radius) continue;
                ++row.edges;

                bool wet = false, sea = false;
                const int steps = radius * 2;
                for (int t = 1; t < steps && !sea; ++t)
                {
                    const int c = A.col + (B.col - A.col) * t / steps;
                    const int r = A.row + (B.row - A.row) * t / steps;
                    if (c < 0 || r < 0 || c >= fx.gw || r >= fx.gh) continue;
                    const std::size_t idx = static_cast<std::size_t>(c + r * fx.gw);
                    if (idx >= sub.size()) continue;
                    if (is_water(sub[idx])) wet = true;
                    if (is_sea(sub[idx]))   sea = true;
                }
                if (wet) ++row.edges_over_water;
                if (sea) ++row.edges_over_sea;
            }

        rows.push_back(row);
        std::printf("  seed %u: %d regions | ON WATER %d (sea %d, open ocean %d)\n",
                    row.seed, row.regions, row.on_water, row.on_sea, row.on_open_ocean);
        std::printf("           domain field: land %d, coastal_water %d, open_ocean %d"
                    " | disagreeing with the substrate %d\n",
                    row.dom_land, row.dom_coastal, row.dom_open, row.dom_mismatch);
        std::printf("           %lld adjacency edges | crossing water %lld (%lld%%)"
                    " | crossing SEA %lld (%lld%%)\n",
                    row.edges, row.edges_over_water,
                    row.edges ? row.edges_over_water * 100 / row.edges : 0,
                    row.edges_over_sea,
                    row.edges ? row.edges_over_sea * 100 / row.edges : 0);
    }

    std::printf("\n--- what the numbers mean for the two items ---\n");
    long long tot_water = 0, tot_sea = 0, tot_edges = 0, tot_edge_sea = 0;
    long long tot_open = 0, tot_dom_coastal = 0, tot_dom_mismatch = 0, tot_regions = 0;
    for (const seed_row& r : rows)
    { tot_water += r.on_water; tot_sea += r.on_sea; tot_edges += r.edges; tot_edge_sea += r.edges_over_sea;
      tot_open += r.on_open_ocean; tot_dom_coastal += r.dom_coastal;
      tot_dom_mismatch += r.dom_mismatch; tot_regions += r.regions; }

    std::printf("  BL-756 regions founded on water: %lld across %d seeds (%lld on sea proper)\n",
                tot_water, static_cast<int>(rows.size()), tot_sea);
    std::printf("         -> zero means the terrain guard lands FREE; non-zero means adding it\n"
                "            moves every world and is Ben's call, not a quiet fix.\n");
    std::printf("  BL-755 adjacency edges crossing sea: %lld of %lld (%lld%%)\n",
                tot_edge_sea, tot_edges, tot_edges ? tot_edge_sea * 100 / tot_edges : 0);
    std::printf("         -> this BOUNDS the free sea reach. Zero would mean the water-blind\n"
                "            radius is harmless in practice and BL-755 is theoretical.\n");
    std::printf("  BL-777 regions by STORED domain: coastal_water %lld, open_ocean %lld,"
                " of %lld\n", tot_dom_coastal, tot_open, tot_regions);
    std::printf("         -> W4 is the ruling: open ocean has no owner, so nothing founds\n"
                "            there. Coastal water is NOT refused - the shore has an owner\n"
                "            (BL-776) - so a non-zero coastal count is the fix working.\n");

    std::printf("\n");
    check(all_ran && !rows.empty(), "W1  every seed generated and ran its era (the fixture gate)");
    check(tot_edges > 0, "W2  the census actually examined adjacency edges (non-vacuous)");
    // BL-777 R1. The field is derived from the substrate and then STORED, which
    // is safe only because of three sentences: it is written once at founding,
    // never recomputed, and nothing ever moves an anchor. One disagreement means
    // one of the three stopped being true.
    check(tot_dom_mismatch == 0,
          "W3  every region's stored domain agrees with the substrate under its anchor");
    // BL-777 R2. Deliberately NOT `tot_water == 0`: a blanket water ban would
    // delete the coastal foundings BL-776 made legitimate, and the distinction
    // IS the design. Measured from the SUBSTRATE rather than from the field, so
    // a mislabelled field cannot make it pass.
    check(tot_open == 0,
          "W4  no region anchors on OPEN OCEAN (coastal water is legitimate, and untouched)");

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
