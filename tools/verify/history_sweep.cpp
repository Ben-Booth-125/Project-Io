// ---------------------------------------------------------------------------
// history_sweep — BL-275. Run the 0 -> 2000 CE sim over a seed spread of
// generated worlds and REPORT THE DISTRIBUTIONS the campaign premise's
// philosophical claims are tuned against.
//
// THIS HARNESS REPORTS; IT DOES NOT GATE. That is the item's own instruction
// and it is deliberate: the first job is to show the raw spread, including the
// ugly cases, before anybody writes an assertion that hides them. Ben, filing
// it: "if a problem becomes very hard to solve, I will be fascinated." So the
// only PASS/FAIL rows below are structural (did every seed run, is the sim
// deterministic) — never "hegemony stays under X%", which is a tuning target
// nobody has chosen yet.
//
// Usage:  history_sweep [seed_count]      (default 16)
// Writes: history_sweep.json beside the working directory, plus the table below.
// ---------------------------------------------------------------------------

#include "world/era_minus_one.hpp" // --epoch: derive generation's own sim params
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/law.hpp"            // BL-750: tariff_bands, the enacted duty
#include "world/nation_generation.hpp" // BL-769: nation_params, the size floor
#include "world/sim_terrain_build.hpp"
#include "world/settlement.hpp"
#include "world/works_roster.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <unordered_set>
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
        if (b.is_homeworld) return &b; // identity, not display name (BL-257)
    return r.bodies.empty() ? nullptr : &r.bodies.front();
}

// ---------------------------------------------------------------------------
// The works fixture (BL-321)
// ---------------------------------------------------------------------------
//
// A HAND-BUILT REGISTRY, NOT A SECOND AUTHORING PATH. The real table lives in
// scripts/works.lua and is loaded by works_registry.cpp, the one TU that pulls
// sol2 — and this harness is Lua-free by design, because every `world/*` check
// in the project is. So the fixture below is a small stand-in with the same
// SHAPE as the real table: one ungated reach work, one endowment-gated work per
// axis, one population-gated defence work.
//
// The distinction matters and is worth being explicit about. This is not
// works.lua duplicated — it is deliberately different and deliberately smaller,
// because what these checks assert is that the MECHANISM works (gates fire on
// the ground, effects reach a consumer, two runs agree), never that a
// particular authored magnitude is right. Magnitudes are the sweep's job to
// report and Ben's to tune; a fixture that mirrored works.lua would silently
// become a second copy to keep in step, which is exactly what moving the table
// to Lua was meant to avoid.
works_registry works_fixture()
{
    works_registry reg;

    auto add = [&](const char* name, roster_band band, work_gate g, work_effect e, int weight) {
        work_row r;
        r.name = name; r.band = band; r.gate = g; r.effect = e; r.weight = weight;
        reg.add_row(r);
    };

    // Ungated reach: the row poor ground can still raise. Its existence is the
    // half of the invariant that keeps breadth a decision rather than a ceiling.
    { work_gate g; g.population = 3000; work_effect e; e.reach_mod = 120;
      add("Test Way", roster_band::classical, g, e, 240); }

    { work_gate g; g.farm_q = 300; g.population = 4000; work_effect e;
      e.capacity_mod = 180; e.manpower_mod = 40;
      add("Test Granary", roster_band::classical, g, e, 220); }

    { work_gate g; g.ore_q = 400; g.population = 5000; work_effect e;
      e.manpower_mod = 60; e.industrial_mod = 120;
      add("Test Pits", roster_band::classical, g, e, 170); }

    { work_gate g; g.port_q = 400; g.population = 5000; work_effect e;
      e.reach_mod = 200; e.capacity_mod = 120;
      add("Test Mole", roster_band::classical, g, e, 150); }

    { work_gate g; g.population = 8000; work_effect e;
      e.capacity_mod = 40; e.defence_mod = 250;
      add("Test Wall", roster_band::classical, g, e, 200); }

    // A later band, to check that bands gate and that they are CUMULATIVE.
    { work_gate g; g.ore_q = 350; g.population = 15000; work_effect e;
      e.manpower_mod = 80; e.defence_mod = 480;
      add("Test Fortress", roster_band::medieval, g, e, 210); }

    return reg;
}

/// A minimal one-polity world of `n` regions in a row, each with the ground
/// the caller asks for. Cheap enough to run several of inside a sweep.
settlement_state strip_world(int n, int farm_q, int ore_q, int port_q, int culture)
{
    settlement_state ss;
    for (int i = 0; i < n; ++i)
    {
        region p;
        p.col = i * 3; p.row = 0; p.anchor = i * 3;
        p.culture = culture_shares::pure(culture); p.founding_culture = culture;
        p.farm_q = farm_q; p.ore_q = ore_q; p.port_q = port_q;
        p.settle_score_q = 900 - i;
        p.name = "Strip" + std::to_string(i);
        ss.regions.push_back(p);
    }
    return ss;
}

/// Two rival strips laid end to end — the smallest world in which a campaign
/// has anywhere to go. `strip_world` alone is one culture, so it seeds ONE
/// polity and the campaign verb never has a target; anything asserting about
/// supply needs two.
settlement_state rival_strip(int per_side, int farm_q, int ore_q, int port_q)
{
    settlement_state ss = strip_world(per_side, farm_q, ore_q, port_q, 0);
    settlement_state b  = strip_world(per_side, farm_q, ore_q, port_q, 1);
    for (region& p : b.regions)
    {
        p.col += per_side * 3;      // Continue the row, so the two sides adjoin.
        p.anchor = p.col;
        p.name += "B";
    }
    ss.regions.insert(ss.regions.end(), b.regions.begin(), b.regions.end());
    return ss;
}

/// One region's works mask, so a check can say WHICH works stand rather than
/// only how many.
uint32_t masks_of(const settlement_state& ss)
{
    uint32_t all = 0;
    for (const region& p : ss.regions) all |= p.works_built;
    return all;
}

/// One world's row. Every field is a metric BL-275 named at filing.
struct sweep_row
{
    uint32_t seed = 0;

    int regions_start = 0;
    int regions_end   = 0;
    int powers_start    = 0;
    int powers_end      = 0;

    /// Largest single polity's share of regions at the epoch, per-mille.
    int  top_share_q     = 0;
    /// First year any polity crossed the hegemony threshold, or -1 for never.
    int64_t hegemony_year = -1;
    int  peak_share_q     = 0; ///< The highest share reached at any point.
    /// Regions held by the WEAKEST surviving power at the epoch — how close
    /// the model gets to eliminating anyone (BL-308).
    int  smallest_holding = 0;

    // --- BL-970: THE SAME READINGS BY POPULATION HELD -----------------------
    //
    // Every share above divides REGIONS held by regions live, and regions_end
    // runs 2x-6x regions_start because polities FOUND new ground inside the run
    // (seed 0: 209 -> 1340). A "share of the map" is therefore a share of a map
    // that grew four times under the reading, and a newly founded empty region
    // weighs the same as a captured city. NR-809 already records that region
    // count is not the grain provinces will use. So each reading is taken a
    // second time over POPULATION HELD -- the sum of `region::population` over
    // a polity's regions at the sample -- and the two columns print side by
    // side. The source is the sim's own playback record (`polity_sample::
    // population`, BL-817), which the sim writes and never reads, so the column
    // costs the run nothing, moves no decision and no digest, and needs no new
    // field on the sim. The region column stays; nothing legacy is renamed.
    int     top_share_pop_q      = 0;  ///< Largest polity's share of held population at the epoch, per-mille.
    int     peak_share_pop_q     = 0;  ///< The highest such share at any century sample.
    int64_t hegemony_year_pop    = -1; ///< First century a polity held the threshold share of population, or -1.
    int64_t smallest_holding_pop = 0;  ///< Population held by the weakest surviving power at the epoch.
    bool    pop_recorded         = false; ///< False when the run carried no playback record (column reads 0).

    int64_t battles   = 0;
    int64_t conquests = 0;
    int64_t foundings = 0;

    /// BL-908 — the directed contact table, at the epoch. `unmet_pair` is
    /// true where at least two LIVING polities never raised a contact between
    /// them, i.e. an ad-hoc, world-independent read of "settled and unmet".
    int64_t contact_pairs = 0;
    bool    unmet_pair    = false;
    /// BL-778 / BL-779 — what the water model produced. All three are
    /// CALIBRATION readings, never coverage targets to raise: rare naval
    /// combat is the design (docs/generation/MILITARY_HISTORY.md § Naval).
    int64_t illegal_campaigns = 0; ///< Refused on traversal legality (BL-778).
    int64_t starved_campaigns = 0; ///< Fought at zero supply, could not forage.
    int64_t sea_legs_fed      = 0; ///< BL-899: crossings fed on the reduced ration.
    // --- BL-889: WHY a campaign did not happen, by reason ------------------
    // The sim already counts every one of these; the sweep reported two of
    // them. 812,424 refusals across 16 worlds with a single named reason is
    // not a diagnosis, so the whole funnel is surfaced.
    int64_t mat_trade         = 0; ///< BL-895: materials the network yielded.
    int64_t mat_total         = 0; ///< ...of this much produced in total.
    int64_t mat_upkeep        = 0; ///< BL-895 sink 1: spent standing an army.
    int64_t mat_roads         = 0; ///< BL-895 sink 2: spent widening corridors.
    int64_t mat_campaigns     = 0; ///< BL-867: spent launching campaigns.
    int64_t heads_unpaid      = 0; ///< Heads sent home unpaid.
    int64_t roads_refused     = 0; ///< Corridor promotions refused for want of materials.
    int64_t cross_border_links_open = 0; ///< BL-925: amicable trade links open at run's end.
    int64_t secessions        = 0; ///< BL-896: successor realms the dark age produced.
    int64_t supply_sites_upgraded           = 0; ///< BL-929: supply sites bought outright.
    int64_t supply_sites_upgraded_regions   = 0; ///< ...of which a region's own relief.
    int64_t supply_sites_upgraded_corridors = 0; ///< ...of which a corridor's tier.
    int64_t mat_supply_sites                = 0; ///< BL-929: materials spent buying them.
    int64_t events            = 0; ///< BL-916: typed events the record carries.
    int64_t events_ended      = 0; ///< ...of which realm_ended (the wizard's "destroyed").
    int64_t events_broke      = 0; ///< ...of which broke_away.
    int64_t events_seats      = 0; ///< ...of which seat_captured.
    int64_t fear_leaned       = 0; ///< BL-838: candidates leaned by fear of being next.
    int64_t fear_targets      = 0; ///< ...against this many DISTINCT polities.
    int64_t regions_seceded   = 0; ///< ...and the ground that walked away with them.
    int64_t seceded_graph_cut = 0; ///< BL-922: ...of which had a fed same-realm region within neighbour_radius (graph, not map).
    int64_t creeds_arisen     = 0; ///< BL-897: universalising creeds coined this world.
    int64_t creed_adoptions   = 0; ///< ...realms that adopted one as an institution.
    int64_t creed_converted   = 0; ///< ...peoples whose pantheon faded to residue.
    int64_t creed_reasserted  = 0; ///< ...and peoples whose pantheon won instead.
    int64_t schisms           = 0; ///< BL-944: reasserted realms that fractured over creed.
    int64_t regions_sundered  = 0; ///< ...and the ground that walked away with THEM -- distinct from `regions_seceded`.
    int64_t reach_denied      = 0; ///< Refused by the BL-837 reach gate.
    // BL-922 -- the supply distribution over HELD ground at the epoch, read
    // off `region::network_supply_q` (the last decision round's reading) for
    // every region with an owner. The gate/floors are in this currency, so
    // the share under each floor is what says whether a floor can bite at all.
    int64_t held_regions      = 0; ///< Regions with an owner at the epoch.
    int64_t zero_kin_near     = 0; ///< ...of those, with a FED same-realm region within neighbour_radius (cut off by the graph, not the map).
    int64_t zero_no_link      = 0; ///< ...of those, with NO same-realm region within neighbour_radius at all (cut off by the map).
    int64_t supply_p10        = 0; ///< 10th percentile of the reading.
    int64_t supply_p50        = 0; ///< median.
    int64_t supply_p90        = 0; ///< 90th percentile.
    int64_t supply_band_med[4] = {0, 0, 0, 0}; ///< median by Chebyshev distance to capital: <10, 10-19, 20-29, 30+.
    int64_t supply_band_n[4]   = {0, 0, 0, 0}; ///< ...and how many regions each band holds.
    int64_t era_reach_ms      = 0; ///< `rebuild_reach` time in this run (profile).
    int64_t era_reach_rebuilds= 0; ///< ...and how many rebuilds.
    int64_t era_decisions_ms  = 0; ///< The whole decision round (reach included).
    int64_t campaign_contacts = 0; ///< (own region, foreign neighbour) pairs examined.
    int64_t campaign_scored   = 0; ///< Candidates reaching the score comparison.
    int64_t campaign_cleared  = 0; ///< Candidates clearing campaign_threshold_q.
    int64_t campaign_chosen   = 0; ///< Rounds where Campaign won the verb choice.
    int64_t cleared_rounds    = 0; ///< ROUND-grain: rounds where Campaign was in the running.
    int64_t cleared_lost      = 0; ///< ...of those, rounds it lost the argmax.
    int64_t naval_battles     = 0; ///< Battles with a naval entry on either side.
    int64_t sea_leg_battles   = 0; ///< Battles REACHED over water — the real reading.
    // --- BL-912: the empire tree, as real per-polity state ------------------
    int  empire_polities_alive = 0; ///< Living polities at the run's stop year.
    int  empire_polities_rim   = 0; ///< ...of those, holding EM-SP-4m (the rim).
    int  empire_nodes_mean_q   = 0; ///< Mean nodes held per living polity, x1000.
    int  empire_nodes_max      = 0; ///< The best-climbed polity's node count.
    // --- BL-909: the directed want table -------------------------------------
    int64_t wants_total       = 0; ///< Entries in pass_one_output::wants.
    int64_t wants_via_market  = 0; ///< ...of those, visible through a market.
    /// Works raised over the run (BL-321), and how many regions ended the run
    /// with at least one. Reported rather than gated, like every other metric
    /// here — but a column of zeroes would mean the roster never fired at all,
    /// which is the failure this item is most likely to have.
    int64_t works_raised   = 0;
    int     regions_with_works = 0;
    /// BL-760 (1): raised/fielded DURING the run, split by roster band and by
    /// span. Distinct from the end-of-run per-band works census below — that
    /// one reports what is STANDING, this one what the run actually did, and
    /// only this one can see a span-1 ceiling binding.
    std::array<std::array<int64_t, roster_band_count>, 2> run_works_by_span_band{};
    std::array<std::array<int64_t, roster_band_count>, 2> units_by_span_band{};
    /// BL-757 R4: works standing at the end of the run, PER ROSTER BAND, so
    /// "the roster never fired" can be told apart from "it fired only at the
    /// bottom of the ladder". Derived in the harness from region::works_built
    /// and the registry's own band column — the sim is not touched.
    ///
    /// WHAT THIS IS NOT: R4 also asks for works EITHER SIDE of the boundary
    /// year, and that is not derivable here. The mask is end-state only and
    /// nothing timestamps a work, so splitting by boundary needs the sim to
    /// record the year each row was raised. R4 stays PARTIAL for that half.
    int64_t works_by_band[roster_band_count] = {0, 0, 0, 0};

    /// BL-768 — THE ANCIENT ROAD RECORD, as the sim produced it. The stamping
    /// pass's two constants are read off these columns rather than chosen: the
    /// tier threshold off the traffic histogram (where does the tail start), the
    /// market junction degree off the degree histogram (where does a line stop
    /// being a line). Reported, never asserted into a band — the record is a
    /// consequence of how much history happened, and a world that fought and
    /// settled less legitimately walks fewer roads.
    int64_t corridors        = 0;  ///< Distinct region pairs recorded.
    int64_t corridor_walks   = 0;  ///< Total uses summed over them.
    int64_t corridor_max_use = 0;
    /// Uses histogram, bucketed 1 / 2 / 3 / 4+ — the split the tier rule reads.
    int64_t corridor_use_hist[4] = {0, 0, 0, 0};
    /// Regions by corridor DEGREE, bucketed 0 / 1-2 / 3-4 / 5+ — the split the
    /// market carve's junction rule reads.
    int64_t corridor_degree_hist[4] = {0, 0, 0, 0};
    int64_t corridor_max_degree = 0;

    /// BL-911 — THE SURVIVING NETWORK CROSSES THE HANDOFF, UNEVENLY. Read off
    /// `pass_one_output::surviving_corridors`, never off the raw sim record —
    /// this is the subset a polity still `alive` at the epoch holds an end of.
    /// The SPREAD across surviving polities is the reading the contract is
    /// judged on, never the total (CIVILISATION.md § The network is the
    /// estate, and it crosses): an even split would be the flat map the
    /// design exists to avoid.
    int64_t surv_corridors       = 0; ///< Total corridors crossing (subset of `corridors`).
    int64_t surv_polities_holding = 0; ///< Distinct surviving polities holding >= 1 corridor end.
    int64_t surv_corridors_min   = 0; ///< Fewest corridor-ends held by any one surviving holder.
    int64_t surv_corridors_max   = 0; ///< Most corridor-ends held by any one surviving holder.

    // --- BL-907 -- THE CLOSURE CONTRACT SCOREBOARD, at 1200 CE -------------
    // Two of CIVILISATION.md's seven readings (explorer set / BL-912, directed
    // wants / BL-909) have no source built yet and are NOT measured here --
    // the report prints MISSING for them rather than a false zero. The other
    // five are read off `pass_one_output` below, the same struct the handoff
    // itself carries, never a private re-derivation.
    int64_t caps_total        = 0; ///< Living polities carrying a capital seat.
    int64_t caps_with_market  = 0; ///< ...of those, how many carry a market.
    int64_t grudges_total     = 0; ///< Directed grudge pairs recorded at all.
    int64_t grudges_biting    = 0; ///< ...still carrying a positive standing score.
    int64_t grudge_max_score  = 0; ///< The single highest standing score.

    int64_t peak_population  = 0;
    int64_t peak_year        = 0;
    int64_t epoch_population = 0;

    int lacunae = 0;

    int64_t industrial_first  = 0;
    int64_t industrial_median = 0;
    int64_t industrial_last   = 0;

    // --- BL-748 R1: the furnace, now an event INSIDE the run --------------
    /// Polities that crossed the Industrial rung of the capacity ladder, out of
    /// how many ran at all, and the year the FIRST of them crossed. Reported
    /// separately from `regions_industrial` because the two can disagree: a
    /// polity that crosses two years before the epoch industrialises nothing,
    /// and a single count could not tell that world from one where the rung was
    /// never reached.
    int     polities_total     = 0;
    int     polities_crossed   = 0;
    /// Year the FIRST polity crossed. Meaningless unless `polities_crossed > 0`
    /// — read that, never a magic year value: 0 CE is a real crossing year on
    /// the ancient arc, which is the whole reason `k_never_industrialised` is
    /// INT64_MIN rather than 0.
    int64_t first_cross_year   = 0;
    int     regions_industrial = 0;
    /// The highest and median MATERIALS capacity any polity reached. The
    /// diagnostic for a zero furnace count: band 5 is the Industrial rung, so a
    /// world topping out at 2 is not a furnace problem, it is a ladder problem.
    int mat_cap_max    = 0;
    int mat_cap_median = 0;

    // --- BL-767 R2: does the rise-peak-fall shape occur? ------------------
    /// Polities that ROSE (peak holdings at least double their start, and at
    /// least three regions more), that FELL (ended at or below 60% of their own
    /// peak), and that did both — the shape the item names.
    int shape_rose = 0;
    int shape_fell = 0;
    int shape_rpf  = 0;
    /// The largest single peak any polity reached, as a share of all regions
    /// live at that sample, per-mille. The "how big did the biggest empire
    /// actually get" figure, distinct from `peak_share_q` only in its sampling.
    int shape_top_peak_q = 0;
    /// BL-970: the same four, with a polity's size read as its SHARE of all
    /// held population rather than its region count. Shares, not heads, on
    /// purpose: the body's population grows across the span, so an absolute-
    /// heads rule would call every polity that merely kept its ground a riser.
    int shape_rose_pop       = 0;
    int shape_fell_pop       = 0;
    int shape_rpf_pop        = 0;
    int shape_top_peak_pop_q = 0;

    // --- BL-750: THE TARIFF DISTRIBUTION, off the GENERATED WORLD -----------
    //
    // Read from `w.laws` and `w.nations` — the world `make_hard_coded_world`
    // actually built, not the sweep's own re-run of the sim. That distinction
    // is load-bearing: the tariff is enacted at the HANDOFF, by generation, and
    // a figure derived from the harness's private re-run would be measuring a
    // history nothing downstream ever saw.
    //
    // THE DELIVERABLE IS THIS DISTRIBUTION, not a working tariff (Ben,
    // 2026-09-06). FLATNESS IS THE TRIGGER for reopening the scored-verb form
    // (BL-488): if every world tariffs the same share of its nations at the same
    // rate, the derived scalar carries no variation and the verb is what would
    // give it some. So the columns below have to be able to SHOW flat — which
    // is why the per-band split is here and not just a count.
    int nations_total   = 0;
    int nations_tariffed = 0;
    int tariff_band_low  = 0; ///< rate == tariff_bands::rate_low
    int tariff_band_mid  = 0;
    int tariff_band_high = 0;
    int protection_max_q = 0; ///< Highest posture any surviving polity reached.
    int protection_med_q = 0; ///< Median over the polities that survived.
    /// Nations, and how many of them are single-region city states the size
    /// floor would have absorbed before BL-769.
    int city_states = 0;
    /// THE SCALAR AT ITS OWN GRAIN, so a sparse tariff table can be read back to
    /// its cause. A count of enacted laws cannot tell "the scalar is flat"
    /// (the BL-488 trigger) from "the scalar varies but the upstream signal it
    /// reads does not" — and those want opposite work.
    int polities_alive     = 0;
    int distinct_cross_yrs = 0; ///< Distinct Industrial-rung crossing years among them.
    int protection_pmax    = 0;
    int protection_pmed    = 0;
    int polities_over_floor = 0; ///< Above `tariff_bands::threshold_q`.

    int64_t ms = 0;

    /// DOES THE HISTORY ACTUALLY RECUR? The sim's stated premise is that wars
    /// and expansion recur across the whole run rather than firing once. These
    /// two measure it directly: the last year anything changed hands, and the
    /// share of changes that happened in the first tenth of the run.
    // --- BL-889: does conquest ACCUMULATE, or does the map churn? ---------
    // `battles` and `conquests` count events; neither can tell "many regions
    // taken once and HELD" from "a few regions changing hands over and over".
    // Those are opposite worlds and only the first one builds an empire, so
    // the arc GENERATION_STRATEGY.md sec The asymmetry is POLITICAL asks for
    // is invisible to every column that existed before this block.
    int      regions_touched   = 0; ///< Distinct regions that ever changed hands.
    int      regions_once      = 0; ///< ...that changed hands exactly once (kept).
    int      regions_thrice    = 0; ///< ...that changed hands 3+ times (contested).
    int      max_flips         = 0; ///< The most-fought-over single region.
    int      flips_per_region  = 0; ///< Mean changes per touched region, x100.
    int64_t last_change_year  = 0;
    int      early_change_pct = 0;

    // --- BL-926: THE INSTRUMENT SEES THE ARC ---------------------------------
    //
    // Every figure below is REPORTED, never gated. The item's premise: before
    // any sprint-39 mechanism lands, the sweep has to be able to see what that
    // mechanism is meant to move -- and `powers_end < powers_start` nets births
    // against deaths, so a world where 27 polities died read as a loss of 9.
    //
    /// One polity's whole arc, replayed EXACTLY off `owner_changes` (one
    /// forward walk per seed, never the 40-sample walk above -- a peak that
    /// lasted one round is still a peak). `died` is INT64_MIN while alive.
    struct polity_arc
    {
        int     id          = -1;
        int64_t born        = 0;  ///< Year of its first held region.
        int     peak        = 0;  ///< Most regions held at once.
        int64_t peak_year   = 0;  ///< First year it held that many.
        int     peak_share_q = 0; ///< `peak` over regions live that year, per-mille.
        int     end_hold    = 0;  ///< Regions at the stop year.
        int64_t died        = INT64_MIN;
        /// The polity that took its LAST region, or -1. Derived from the replay
        /// (the change that emptied it), not from a sim field -- there is none.
        int     killer      = -1;
        /// `polity::parent` (BL-926 hook) -- the realm it broke away from, or -1.
        int     parent      = -1;
        bool    alive       = false;
        /// BL-970: the polity's highest share of held POPULATION at any
        /// recorded step, per-mille, and the heads it held there. Read off the
        /// playback record's step cadence, not the exact change replay above.
        int     peak_pop_share_q = 0;
        int64_t peak_pop         = 0;
    };
    std::vector<polity_arc> arcs; ///< ALL polities that ever held ground, by id.
    int polities_ever_held = 0;
    /// GROSS deaths: ever held ground, not alive at the stop year. Sits beside
    /// the net figure (`powers_start - powers_end`) on the face; the gap
    /// between them is the birth count.
    int polities_dead_gross = 0;

    /// BL-896 secession, at event grain: `breakdowns` is parents-that-lost-
    /// a-block; `secessions` (above) is pieces; the sizes are per piece.
    int64_t breakdowns = 0;
    std::vector<int32_t> piece_sizes;

    /// `region::network_supply_q` over HELD regions at the stop year. Shares are
    /// per-mille of `supply_held`. The floors are the ones the run ACTUALLY
    /// used (read off the params in force, never assumed).
    int64_t supply_held        = 0;
    int64_t supply_zero        = 0; ///< == 0: disconnected outright.
    int64_t supply_le_settle   = 0; ///< <= sustainable_settlement_floor_q.
    int64_t supply_le_secede   = 0; ///< <= secession_supply_floor_q.
    int64_t supply_le_campaign = 0; ///< <  sustainable_campaign_floor_q.
    int64_t supply_hist[10]    = {0,0,0,0,0,0,0,0,0,0}; ///< Deciles of 0..1000.
    int floor_settle   = 0;
    int floor_secede   = 0;
    int floor_campaign = 0;

    /// The culture census, off the creeds the run was handed. -1 = no creeds
    /// (the struct-default path runs with a null creed pointer).
    int cultures       = -1;
    int culture_depth  = -1; ///< Longest `parent` chain, roots at 0.
    int cultures_root  = -1; ///< Cultures with no parent.

    int64_t foundings_scheduled = 0;
    int64_t foundings_settled   = 0;
    int64_t civilisations       = 0;

    /// BL-920 -- regions the ORGANISE verb actually took, regions a
    /// crossing threshold promoted to a new city state in its own right, and
    /// (already carried above) `conquests` -- so a world's growth over
    /// unorganised culture ground can be read as organised-vs-conquered.
    int64_t organised          = 0;
    int64_t city_states_risen  = 0;

    /// Polities holding EXACTLY ONE region, at each of the 40-step samples the
    /// rise/peak/fall walk already takes (index 0 = start year).
    std::vector<int> city_states_series;
    int64_t city_states_step = 0;

    /// Which span this row measured -- so a JSON row can say what it is.
    std::string arc_name;
    int64_t     arc_start = 0;
    int64_t     arc_stop  = 0;
};

/// How many polities the per-world arc table prints and the JSON row carries.
/// A bound on OUTPUT, never on what is measured: `sweep_row::arcs` holds all.
constexpr int arc_table_n = 8;

/// A polity holding this share of all regions counts as a hegemon. 0.5 is
/// the plainest reading of "one power dominates the world" and is a REPORTING
/// threshold only — it gates nothing.
constexpr int hegemony_threshold_q = 500;

/// Distinct owners, the largest owner's share, and the SMALLEST holding, in one
/// materialised slice.
///
/// The smallest holding is the diagnostic for BL-308: "elimination rate 0/12"
/// alone cannot distinguish a model that is one region away from killing
/// somebody from one where the weakest power still holds fifty. Those need
/// completely different fixes, so the sweep reports the distance rather than
/// just the binary.
void slice_shape(const std::vector<uint16_t>& slice, int& powers, int& top_share_q,
                 int* smallest_out = nullptr)
{
    std::vector<uint16_t> ids;
    std::vector<int>      counts;
    int live = 0;
    for (uint16_t o : slice)
    {
        if (o == owner_none) continue;
        ++live;
        auto it = std::find(ids.begin(), ids.end(), o);
        if (it == ids.end()) { ids.push_back(o); counts.push_back(1); }
        else                 { ++counts[static_cast<std::size_t>(it - ids.begin())]; }
    }
    powers = static_cast<int>(ids.size());
    int top = 0;
    for (int c : counts) top = c > top ? c : top;
    top_share_q = live > 0 ? (top * 1000) / live : 0;

    if (smallest_out)
    {
        int small = 0;
        for (int c : counts) if (small == 0 || c < small) small = c;
        *smallest_out = small;
    }
}

/// BL-970: the recorded step at or before `year`. Steps are ascending by year,
/// so this is one binary search. A year BEFORE the first recorded step reads the
/// first step -- the opening standing -- rather than an empty map, which is what
/// the region walk reads at the start year off the seeding changes. -1 only
/// when the run carried no playback record at all.
int step_at_or_before(const history_sim_state& s, int64_t year)
{
    if (s.steps.empty()) return -1;
    int lo = 0, hi = static_cast<int>(s.steps.size()) - 1, best = -1;
    while (lo <= hi)
    {
        const int mid = lo + (hi - lo) / 2;
        if (static_cast<int64_t>(s.steps[static_cast<std::size_t>(mid)].year) <= year)
        {
            best = mid; lo = mid + 1;
        }
        else hi = mid - 1;
    }
    return best < 0 ? 0 : best;
}

/// BL-970: what one recorded step holds, by POPULATION. The playback record's
/// `polity_sample::population` is the sum of `region::population` over the
/// polity's held regions at that step (history_sim.cpp, `record_step`), which is
/// exactly the quantity the region-weighted readings should have been dividing.
struct pop_slice
{
    int64_t total    = 0;     ///< Population over every held region.
    int64_t regions  = 0;     ///< Regions held, summed over living polities.
    int64_t top      = 0;     ///< The largest single polity's population.
    int64_t smallest = 0;     ///< The smallest living polity's population.
    int     powers   = 0;     ///< Living polities at the step.
    bool    recorded = false; ///< False when the run carried no record.
};

/// `pop`, when given, is resized to the polity table and filled with each
/// polity's held population at the step.
pop_slice pop_slice_at(const history_sim_state& s, int64_t year, std::vector<int64_t>* pop)
{
    pop_slice out;
    if (pop) pop->assign(s.polities.size(), 0);
    const int si = step_at_or_before(s, year);
    if (si < 0) return out;
    const timelapse_step& st = s.steps[static_cast<std::size_t>(si)];
    out.recorded = true;
    for (int k = 0; k < st.sample_count; ++k)
    {
        const polity_sample& smp = s.samples[static_cast<std::size_t>(st.first_sample + k)];
        out.total   += smp.population;
        out.regions += smp.regions;
        ++out.powers;
        if (smp.population > out.top) out.top = smp.population;
        if (out.powers == 1 || smp.population < out.smallest) out.smallest = smp.population;
        if (pop && smp.polity < pop->size()) (*pop)[smp.polity] += smp.population;
    }
    return out;
}

/// Per-mille of `part` in `whole`, 0 on an empty whole.
int share_q_of(int64_t part, int64_t whole)
{
    return whole > 0 ? static_cast<int>((part * 1000) / whole) : 0;
}

/// Median of a copy — the sweep reports medians rather than means because a
/// single runaway world would drag a mean somewhere no world actually is.
int64_t median_of(std::vector<int64_t> v)
{
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

std::string json_escape(const std::string& s)
{
    std::string out;
    for (char c : s) { if (c == '"' || c == '\\') out += '\\'; out += c; }
    return out;
}


// ---------------------------------------------------------------------------
// `--set <field>=<value>` — the tuning override (BL-767)
// ---------------------------------------------------------------------------
//
// WHY THIS EXISTS RATHER THAN A RECOMPILE PER TRIAL. BL-767 is a tuning item:
// the deliverable is a DISTRIBUTION under a set of forces, and arguing a
// magnitude means running the same spread under several. Editing struct
// defaults and rebuilding the world superset for each trial makes the argument
// unreproducible — nothing in the record says which numbers produced which
// table. An explicit, whitelisted override makes each trial one command line
// that can be pasted back into a report.
//
// IT IS A DELIBERATE DIVERGENCE FROM GENERATION AND SAYS SO. Under any
// override the sweep is no longer measuring the run that builds a world, so
// S1b (the "this IS generation's era" acceptance test) is SKIPPED with its
// reason printed rather than left to fail confusingly. Whitelisted, so a typo
// is an error rather than a silently ignored flag.
struct param_override { std::string name; int value; };

bool apply_override(history_sim_params& p, const std::string& name, int v)
{
    if (name == "invest_yield_q")          { p.invest_yield_q = v;          return true; }
    if (name == "invest_amortise_years")   { p.invest_amortise_years = v;   return true; }
    if (name == "invest_threshold_q")      { p.invest_threshold_q = v;      return true; }
    if (name == "capacity_band_cost")      { p.capacity_band_cost = v;      return true; }
    if (name == "invest_level_pull_q")     { p.invest_level_pull_q = v;     return true; }
    if (name == "invest_ground_pull_q")    { p.invest_ground_pull_q = v;    return true; }
    if (name == "campaign_threshold_q")    { p.campaign_threshold_q = v;    return true; }
    if (name == "settle_threshold_q")      { p.settle_threshold_q = v;      return true; }
    if (name == "settle_pressure_q")       { p.settle_pressure_q = v;       return true; }
    if (name == "consolidate_threshold_q") { p.consolidate_threshold_q = v; return true; }
    if (name == "holdings_burden_q")       { p.holdings_burden_q = v;       return true; }
    if (name == "free_holdings")           { p.free_holdings = v;           return true; }
    if (name == "cohesion_loss_on_defeat_q") { p.cohesion_loss_on_defeat_q = v; return true; }
    if (name == "cohesion_recovery_q")     { p.cohesion_recovery_q = v;     return true; }
    if (name == "settle_cohesion_gate_q")  { p.settle_cohesion_gate_q = v;  return true; }
    if (name == "sack_population_loss_q")  { p.sack_population_loss_q = v;  return true; }
    if (name == "transfer_decisiveness_q") { p.transfer_decisiveness_q = v; return true; }
    if (name == "contest_transfer_relief_q") { p.contest_transfer_relief_q = v; return true; }
    if (name == "campaign_gain_q")         { p.campaign_gain_q = v;         return true; }
    if (name == "work_amortise_years")     { p.work_amortise_years = v;     return true; }
    if (name == "work_threshold_q")        { p.work_threshold_q = v;        return true; }
    if (name == "w_work_capacity")         { p.w_work_capacity = v;         return true; }
    if (name == "w_work_manpower")         { p.w_work_manpower = v;         return true; }
    if (name == "w_work_reach")            { p.w_work_reach = v;            return true; }
    if (name == "w_work_defence")          { p.w_work_defence = v;          return true; }
    if (name == "w_work_industrial")       { p.w_work_industrial = v;       return true; }

    // --- The BL-837 reach GATE, exposed to --set (2026-09-10) ------------
    // `sustainable_campaign_floor_q`'s own comment instructs the reader to
    // "Re-tune from here with `history_sweep`, not by re-guessing a round
    // number" -- and until now this table did not carry it, so every
    // investigation of the gate had to EDIT SOURCE to move it. That is the
    // one dial the sweep is explicitly told to tune, and setting it to 0
    // turns the wall back into a toll, which is the comparison the
    // gate-versus-price question needs.
    if (name == "sustainable_campaign_floor_q") { p.sustainable_campaign_floor_q = v; return true; }
    if (name == "sustainable_garrison_floor_q") { p.sustainable_garrison_floor_q = v; return true; }
    if (name == "terrain_reach_cost_q")    { p.terrain_reach_cost_q = v;    return true; }
    if (name == "road_tier1_uses")         { p.road_tier1_uses = v;         return true; }
    if (name == "road_tier2_uses")         { p.road_tier2_uses = v;         return true; }
    // BL-894: 0/1, so a sweep can measure the Empires-round rule against the
    // migration-era one without deriving generation's whole param set.
    if (name == "settle_requires_razed_ground") { p.settle_requires_razed_ground = v != 0; return true; }
    if (name == "amphibious_weight_crossing")   { p.amphibious_weight_crossing = v != 0;   return true; }
    // BL-899: sea legs. The ration, the creed floor, and the port gate.
    if (name == "sea_legs_ration_q")           { p.sea_legs_ration_q = v;                return true; }
    if (name == "sea_legs_floor_q")            { p.sea_legs_floor_q = v;                 return true; }
    if (name == "sea_legs_port_q")             { p.sea_legs_port_q = v;                  return true; }
    if (name == "trade_income_per_link")       { p.trade_income_per_link = v;            return true; }
    if (name == "army_upkeep_per_1000_heads")  { p.army_upkeep_per_1000_heads = v;       return true; }
    if (name == "unpaid_army_disband_q")       { p.unpaid_army_disband_q = v;            return true; }
    if (name == "road_build_material_cost")    { p.road_build_material_cost = v;         return true; }
    // BL-929: supply sites bought outright from the stockpile.
    if (name == "supply_upgrade_material_cost") { p.supply_upgrade_material_cost = v;    return true; }
    if (name == "supply_upgrade_reach_gain_q")  { p.supply_upgrade_reach_gain_q = v;     return true; }
    if (name == "supply_upgrade_threshold_q")   { p.supply_upgrade_threshold_q = v;      return true; }
    if (name == "secession_supply_floor_q")    { p.secession_supply_floor_q = v;         return true; }
    if (name == "secession_min_regions")       { p.secession_min_regions = v;            return true; }
    // BL-897 -- a creed that spans cultures. The first is the master switch.
    if (name == "universal_creed_humbled_cohesion_q") { p.universal_creed_humbled_cohesion_q = v; return true; }
    if (name == "universal_creed_min_trade_links")    { p.universal_creed_min_trade_links = v;    return true; }
    if (name == "universal_creed_network_floor_q")    { p.universal_creed_network_floor_q = v;    return true; }
    if (name == "universal_creed_hold_years")         { p.universal_creed_hold_years = v;         return true; }
    if (name == "universal_creed_convert_supply_q")   { p.universal_creed_convert_supply_q = v;   return true; }
    if (name == "universal_creed_alien_penalty_q")    { p.universal_creed_alien_penalty_q = v;    return true; }
    if (name == "w_aggr_q")                   { p.w_aggr_q = v;                        return true; }
    if (name == "w_fear_q")                   { p.w_fear_q = v;                        return true; }
    if (name == "fear_reference")             { p.fear_reference = v;                  return true; }
    // BL-887: the centre-chain reach model, whole. `centre_chain_reach=0` is
    // the OLD single-capital Dijkstra, so one build A/Bs the two models rather
    // than two builds being compared and hoped identical elsewhere.
    if (name == "centre_chain_reach")         { p.centre_chain_reach = v != 0;         return true; }
    if (name == "centre_reach_rebate_q")      { p.centre_reach_rebate_q = v;           return true; }
    if (name == "centre_reach_rebate_cap_q")  { p.centre_reach_rebate_cap_q = v;       return true; }
    if (name == "centre_reach_min_centres")   { p.centre_reach_min_centres = v;        return true; }
    // BL-839 -- the turbulence lean. `turbulence_lean` is the SETTING (-1 calm,
    // 0 ordinary, +1 turbulent) and is the one a three-way comparison moves;
    // the three magnitudes beside it are what the comparison TUNES once the
    // settings are shown to separate at all.
    if (name == "turbulence_lean")              { p.turbulence_lean = v;                 return true; }
    if (name == "turbulence_aggression_spread_q") { p.turbulence_aggression_spread_q = v; return true; }
    if (name == "turbulence_fear_q")            { p.turbulence_fear_q = v;               return true; }
    if (name == "turbulence_reach_cost_q")      { p.turbulence_reach_cost_q = v;         return true; }
    return false;
}

/// The overrides, at namespace scope so EVERY run this harness performs sees
/// them. Half-applying `--set` — the seed loop tuned, the synthetic W-checks
/// still on struct defaults — is the same silently-misleading failure the
/// whitelist exists to stop, one layer further in.
std::vector<param_override> g_overrides;

/// Struct defaults with the overrides applied. The synthetic fixtures build
/// their params from this rather than from `history_sim_params{}` directly.
history_sim_params tuned_defaults()
{
    history_sim_params p;
    for (const param_override& o : g_overrides) apply_override(p, o.name, o.value);
    return p;
}

} // namespace

int main(int argc, char** argv)
{
    int seed_count = 16;
    int64_t epoch_year = 0;
    bool derive_from_generation = true;  // BL-900: generation's span is the default
    std::vector<param_override> overrides;

    for (int a = 1; a < argc; ++a)
    {
        const std::string arg = argv[a];
        if (arg == "--set" && a + 1 < argc)
        {
            const std::string kv = argv[++a];
            const std::size_t eq = kv.find('=');
            if (eq == std::string::npos)
            {
                std::printf("FAIL  --set wants <field>=<value>, got \"%s\"\n", kv.c_str());
                return 2;
            }
            overrides.push_back({kv.substr(0, eq), std::atoi(kv.c_str() + eq + 1)});
            continue;
        }
        // `--epoch <year>` derives the sim params through
        // `era_minus_one_sim_params`, i.e. the run GENERATION performs. Opt-in
        // rather than the default, so every number this sweep has ever printed
        // keeps meaning what it meant.
        if (arg == "--epoch" && a + 1 < argc)
        {
            const std::string ey = argv[++a];
            if (ey.rfind("--", 0) == 0)
            {
                std::printf("FAIL  --epoch wants a YEAR, got the flag \"%s\".\n", ey.c_str());
                return 2;
            }
            epoch_year = std::atoll(ey.c_str());
            derive_from_generation = true;
            continue;
        }
        // --struct-default: the OLD behaviour, now opt-IN (BL-900).
        //
        // `history_sim_params{}` is a 4000 BCE -> 0 CE span on a six-band clock
        // (100/50/20/10/5/1-year steps, 136 decision rounds). Generation runs
        // 400 BCE -> 0 CE on ONE 4-year band, 100 rounds. They are different
        // worlds, and until this change the harness measured the first by
        // default while the game ran the second -- BL-462's defect, left half
        // closed because `--epoch` was made opt-in so old numbers kept meaning
        // what they meant.
        //
        // BL-894 turned that from a SPAN difference into a BEHAVIOUR one:
        // `settle_requires_razed_ground` is set by era_minus_one_sim_params and
        // is false in the struct default, so Settle means something different
        // in each. Measured on the same 8 seeds: 810 battles on generation's
        // span against 7 on the struct default, rise-peak-fall in 8/8 worlds
        // against 1/8. A default that reports the second is a default that
        // reports a world nobody plays.
        //
        // The span is still REACHABLE, because colonisation-era questions
        // genuinely need a migration-era arc -- it just has to be asked for,
        // and it says so on the banner when it is.
        if (arg == "--struct-default")
        {
            derive_from_generation = false;
            continue;
        }
        // AN UNRECOGNISED FLAG IS AN ERROR, NOT A SHRUG (2026-09-11).
        //
        // `--epoch` and `--set` both guard on `a + 1 < argc`, so a malformed
        // invocation used to fall through to the seed-count branch, fail
        // `atoi`, and run the DEFAULT sweep while looking like it had done what
        // was asked. Two ways that bit, both in one session:
        //   `8 --epoch`                       -- flag last, silently ignored,
        //                                        struct defaults measured.
        //   `8 --epoch --set field=0`         -- `--set` swallowed as the epoch
        //                                        YEAR, override never applied.
        // The second produced a two-variable comparison read as a one-variable
        // one, and the conclusion drawn from it was backwards. This harness
        // already refuses an unknown `--set` field for exactly this reason
        // ("a silently ignored `--set` would produce a table labelled as a
        // trial of a force that was never changed"); the same argument applies
        // to the flags themselves.
        if (arg.rfind("--", 0) == 0)
        {
            std::printf("FAIL  unknown or malformed flag \"%s\".\n"
                        "      --epoch <year>   (the year is REQUIRED)\n"
                        "      --set <field>=<value>\n"
                        "      --struct-default (the 4000 BCE arc the game does NOT run)\n", arg.c_str());
            return 2;
        }
        const int n = std::atoi(argv[a]);
        if (n > 0) seed_count = n;
    }

    // THE PARAMS THIS SWEEP ACTUALLY RUNS — derived once, printed, then used
    // unchanged in the loop below. Hoisted out of the loop rather than
    // default-constructed per seed because they never depended on the seed, and
    // because a banner that restates a span it does not share with the run is
    // how "0 -> 1960 CE" drifted in the first place.
    world_params sweep_wp;
    sweep_wp.epoch_year = epoch_year;
    history_sim_params params =
        derive_from_generation ? era_minus_one_sim_params(sweep_wp) : history_sim_params{};

    // TRACING ON, BECAUSE THE FUNNEL IS VACUOUS WITHOUT IT (BL-889, 2026-09-10).
    // campaign_contacts / scored / cleared / chosen are ALL gated on
    // `trace_battles` (history_sim.cpp lines ~1774, ~2049, ~2061, ~2330). With
    // it off they read 0 in a run that fought 61 battles and took 35 regions --
    // a set of zeros that looks like a finding and is an artefact. That exact
    // trap cost BL-868 its first three verification passes; sprint 38's risk
    // note records it, and this harness walked into it again the first time
    // the funnel was printed.
    //
    // `illegal_campaigns` and `reach_denied_campaigns` are NOT gated and were
    // trustworthy already -- which is why the reading that the reach gate
    // denies nothing in a generated world survives this correction.
    params.trace_battles = true;

    // The tuning overrides (BL-767), applied to the derived params and printed.
    // A name the whitelist does not know is an ERROR, not a shrug: a silently
    // ignored `--set` would produce a table labelled as a trial of a force that
    // was never changed, which is worse than no trial at all.
    for (const param_override& o : overrides)
        if (!apply_override(params, o.name, o.value))
        {
            std::printf("FAIL  --set: no tunable field named \"%s\"\n", o.name.c_str());
            return 2;
        }
    const bool tuned = !overrides.empty();
    g_overrides = overrides;

    std::printf("=== history sweep (BL-275) - %d seeds, %lld -> %lld  [%s] ===\n",
                seed_count,
                static_cast<long long>(params.start_year),
                static_cast<long long>(params.stop_year),
                derive_from_generation
                    ? "GENERATION'S OWN RUN"
                    : "STRUCT-DEFAULT ARC - NOT the run the game performs");

    // --- WHAT THIS SWEEP MEASURES, STATED ON THE FACE OF THE REPORT ---------
    //
    // WHICH WORLD THIS IS, SAID ON THE FACE OF THE REPORT (BL-900).
    //
    // The default is now GENERATION'S OWN params (`era_minus_one_sim_params`):
    // at the 0 CE epoch, -400 -> 0 on ONE four-year band, 100 rounds. That is
    // the run the game performs. `--struct-default` asks for the old arc
    // instead -- -4000 -> 0 on six bands, 136 rounds -- which spans the
    // migration era and is a legitimate thing to want, but is NOT the shipped
    // run and now says so.
    //
    // This closes the half of BL-462 that was left open. The defect was never
    // that the struct-default arc existed; it was that it was the DEFAULT, so
    // a report describing a world nobody plays looked like a measurement of the
    // game. Three sprints were steered by numbers from it.
    // The span is reported from the params rather than assumed, so a two-span
    // run (BL-747) shows both halves and a single-span run says so outright
    // instead of printing a boundary of INT64_MIN as if it meant something.
    {
        const bool two_span = params.boundary_year != INT64_MIN;
        // Decision ROUNDS, counted the way the sim counts them — by walking
        // `step_for_year` — rather than by dividing the span by a step this
        // harness picked. Same reason `region_distance` is public.
        int64_t rounds_span1 = 0, rounds_span2 = 0;
        for (int64_t y = params.start_year; y < params.stop_year;)
        {
            if (two_span && y < params.boundary_year) ++rounds_span1;
            else                                      ++rounds_span2;
            y += step_for_year(params, y);
        }

        // BE PRECISE ABOUT WHAT --epoch BUYS. Deriving the params closes only
        // axes 1-2 of the six divergences era_minus_one.hpp enumerates (the
        // span and the clock). This loop still passes the BARE seed rather than
        // era_minus_one_sim_seed's fold (axis 3), a null creed pointer that
        // flattens every polity's aggression to 500 (axis 4), the synthetic
        // works_fixture rather than generation's registry (axis 5), and the
        // POST-sim settlement out of the report (axis 6). Calling that
        // "generation's own run" was the exact mislabel BL-757 is filed about,
        // so it says what it is instead.
        std::printf("    params: %s\n",
                    derive_from_generation
                        ? "GENERATION'S OWN ERA via era_minus_one_fixture — span, clock, folded"
                          " seed, creeds, works and PRE-sim settlement all from the real run."
                          " S1b asserts the counts match the report, per seed"
                        : "history_sim_params STRUCT DEFAULTS — *not* the run that builds a world");
        if (two_span)
            std::printf("    spans:  ancient %lld -> %lld (%lld rounds, band ceiling %d)"
                        " | industrial %lld -> %lld (%lld rounds, unrestricted)\n",
                        static_cast<long long>(params.start_year),
                        static_cast<long long>(params.boundary_year),
                        static_cast<long long>(rounds_span1),
                        static_cast<int>(params.span1_band_ceiling),
                        static_cast<long long>(params.boundary_year),
                        static_cast<long long>(params.stop_year),
                        static_cast<long long>(rounds_span2));
        else
            std::printf("    spans:  SINGLE %lld -> %lld (%lld rounds), no boundary year\n",
                        static_cast<long long>(params.start_year),
                        static_cast<long long>(params.stop_year),
                        static_cast<long long>(rounds_span2));
        std::printf("    clock:  %d tick band%s\n",
                    params.tick_band_count, params.tick_band_count == 1 ? "" : "s");
        if (tuned)
        {
            std::printf("    TUNED:  ");
            for (const param_override& o : overrides)
                std::printf("%s=%d  ", o.name.c_str(), o.value);
            std::printf("\n            (a DELIBERATE divergence from the shipped forces — this\n"
                        "             table is a trial, not a measurement of the built world)\n");
        }
        std::printf("\n");
    }

    std::vector<sweep_row> rows;
    rows.reserve(static_cast<std::size_t>(seed_count));

    // BL-757 R1's acceptance test, and it is SELF-CHECKING rather than a pinned
    // literal. generation_report carries the counts generation's own era run
    // produced (prehistory_battles and its two siblings), so a sweep that truly
    // re-runs generation's era must reproduce them for every seed. A literal
    // would rot the moment the world legitimately changed; this cannot.
    bool derived_matches_generation = true;
    int  derived_seeds_checked      = 0;

    // BL-321: every sweep run now carries the works roster, so the distributions
    // below describe a world whose polities could BUILD. That is a deliberate
    // change to what this harness measures — the item's whole claim is that
    // works move the frontier stall from a ceiling to a decision, and a sweep
    // run without them could not show whether they did.
    const works_registry works = works_fixture();

    for (int i = 0; i < seed_count; ++i)
    {
        const auto t0 = std::chrono::steady_clock::now();

        world_params wp;
        wp.seed = static_cast<uint32_t>(i);
        // THE EPOCH REACHES GENERATION (BL-757 R1). Without this the sweep built
        // an ancient world and then ran an industrial span over its settlement,
        // which is a different world from the one --epoch names.
        wp.epoch_year = epoch_year;

        generation_report     rep;
        era_minus_one_fixture fx;
        // Ask for the fixture only on the deriving path, so the default sweep
        // pays nothing for it and stays byte-for-byte the instrument it was.
        const world w = make_hard_coded_world(wp, &rep, world_gen_config{},
                                              /*progress=*/nullptr, /*works=*/nullptr,
                                              derive_from_generation ? &fx : nullptr);

        const generation_report::body_entry* k = kepler_of(rep);
        if (!k) continue;

        if (derive_from_generation && !fx.ran)
        {
            // The fixture's gate is the REAL one, settlement clause included. An
            // unran era would make every number below a struct default, which is
            // the exact failure this item exists to close — so say so and skip.
            std::printf("  seed %d: THE ERA DID NOT RUN (fixture gate false) — skipped.\n", i);
            continue;
        }

        // THE PRE-SIM SETTLEMENT (divergence axis 6). The report's copy is the
        // state AFTER the sim mutated it in place, so re-running from it starts
        // the era from its own ending. The fixture captures what generation
        // actually handed in.
        settlement_state ss = derive_from_generation ? fx.settlement : k->settlement;

        sweep_row row;
        row.seed            = wp.seed;
        row.regions_start = static_cast<int>(ss.regions.size());
        row.lacunae         = k->settlement.lacunae;

        // `params` is now hoisted above the loop and PRINTED in the banner, so
        // what this sweep runs is visible rather than inferred. It is still the
        // struct default unless `--epoch` was passed — see the banner block for
        // why that is not generation's run. Every window below is derived from
        // `params` rather than restating a span.
        const int64_t span = params.stop_year - params.start_year;

        // REAL TERRAIN (BL-316 S1). Kepler's own ground, so mountains cost what
        // mountains cost and terrain_combat is finally live. Grid dims come from
        // the one authority (hard_coded_world.hpp): a wrong-but-same-area pair
        // here (the original 168x90 vs the real 180x84 — both 15120) misaligns
        // every terrain lookup SILENTLY instead of crashing.
        const entity_id kepler_id = k->id; // the report entry names its own entity (BL-257)
        const sim_terrain_arrays terr = build_sim_terrain(w, kepler_id,
                                                          home_grid_width, home_grid_height);

        // BL-757 R1 — SIX AXES, NOT TWO. Deriving the params closes only the
        // span and the clock. The deriving path now also takes generation's
        // FOLDED seed (era_minus_one_sim_seed, not the bare world seed), its
        // REAL creed pointer (a null one flattens every polity's aggression to
        // 500 — in a sweep whose subject is why polities fight), its works
        // pointer, and its pre-sim settlement above. era_minus_one.hpp's header
        // enumerates all six; this is the call that finally honours them.
        //
        // The default path is untouched (R2), deliberately: it remains the
        // struct-default research ladder, labelled as such in the banner.
        //
        // RESIDUAL, and NOT this item's to fix: generation is called here with
        // works = nullptr, which is what world_determinism does and is why the
        // acceptance figures (battles 270 / conquests 207 / foundings 833 on
        // seed ABCDEF01) are reachable. The APP passes its real registry, so
        // the shipped game scores FIVE verbs where every harness scores four.
        // That divergence is real and is recorded on BL-757 rather than papered
        // over here.
        // THE OVERRIDES REACH THE FIXTURE'S PARAMS TOO (BL-767), or a `--set`
        // on the deriving path would print a "TUNED" banner over an untuned
        // run — the silently-ignored-flag failure the whitelist exists to stop,
        // one layer down. The fixture is copied rather than mutated so the
        // acceptance test below still knows what generation actually ran.
        // BL-926: the params the run ACTUALLY used, for the floors the supply
        // histogram is read against. `fp` below is scoped to its branch.
        history_sim_params ran = params;
        history_sim_state sim;
        if (derive_from_generation)
        {
            history_sim_params fp = fx.params;
            // BL-905: the derive_from_generation path builds `fp` from the
            // fixture's OWN params, never from the harness's `params` above --
            // so the `params.trace_battles = true` set for the banner's default
            // path never reached this one. campaign_contacts/scored/cleared/
            // chosen are ALL gated on this flag (history_sim.cpp), so every
            // seed run through generation's own params read them as a harness
            // artifact of zero, indistinguishable from a genuine empty funnel.
            // `illegal_campaigns`/`reach_denied_campaigns` are NOT gated (same
            // file), which is why those two counters stayed trustworthy while
            // the rest of the funnel read as an artifact. Tracing is bookkeeping
            // only -- it changes no decision and the run stays byte-identical
            // with it on or off (history_sim.cpp's own comment on the field).
            fp.trace_battles = true;
            for (const param_override& o : overrides) apply_override(fp, o.name, o.value);
            ran = fp;
            sim = run_history_sim(ss, &fx.creeds, fx.terrain.view(), fx.gw, fx.gh,
                                  fp, fx.seed, nullptr, fx.works);
        }
        else
        {
            sim = run_history_sim(ss, nullptr, terr.view(),
                                  home_grid_width, home_grid_height, params, wp.seed,
                                  nullptr, &works);
        }

        // BL-908 — CONTACT AT THE EPOCH. Cheap: `sim.contacts` is already
        // sorted, so "does this pair have an entry" is a binary search, and
        // the O(polities^2) pair scan below is fine at the polity counts
        // this sim ever reaches (tens, not thousands).
        {
            row.contact_pairs = static_cast<int64_t>(sim.contacts.size());
            for (std::size_t pi = 0; pi < sim.polities.size() && !row.unmet_pair; ++pi)
            {
                if (!sim.polities[pi].alive) continue;
                for (std::size_t pj = pi + 1; pj < sim.polities.size(); ++pj)
                {
                    if (!sim.polities[pj].alive) continue;
                    if (!has_contact(sim, static_cast<int>(pi), static_cast<int>(pj)))
                    { row.unmet_pair = true; break; }
                }
            }
        }

        // BL-912 — THE EMPIRE TREE, MEASURED. `sim.polities` already carries
        // the mask and it crosses into `pass_one_output` untouched (it is
        // copied whole); reading it here rather than there is cheaper and
        // says the same thing at this epoch.
        {
            int64_t node_sum = 0;
            for (const polity& q : sim.polities)
            {
                if (!q.alive) continue;
                ++row.empire_polities_alive;
                const int held_nodes = static_cast<int>(std::popcount(q.empire_mask));
                node_sum += held_nodes;
                if (held_nodes > row.empire_nodes_max) row.empire_nodes_max = held_nodes;
                if (polity_holds_empire_rim(q)) ++row.empire_polities_rim;
            }
            if (row.empire_polities_alive > 0)
                row.empire_nodes_mean_q = static_cast<int>(
                    (node_sum * 1000) / row.empire_polities_alive);
        }

        {
            int64_t early = 0;
            for (const owner_change& c : sim.owner_changes)
            {
                if (c.year > row.last_change_year) row.last_change_year = c.year;
                if (c.year <= params.start_year + span / 10) ++early; // First tenth of the run.
            }
            {
                // CONQUESTS ONLY -- and this correction matters, because the
                // first cut of this block did NOT make it and was vacuous.
                // `owner_changes` records EVERY ownership transition including
                // a FOUNDING (a region gaining its first owner). Measured on
                // this sweep: ~4,827 owner_changes per world against a median
                // of 35 conquests and 2,682 foundings. So a histogram over raw
                // owner_changes reports how SETTLEMENT is distributed and says
                // nothing whatever about conquest -- it read "99% taken once
                // and kept", which is a true statement about founding new
                // ground and a meaningless one about taking someone else's.
                //
                // A region's FIRST change is its founding; every later change
                // is a transfer between owners. Counting only those makes this
                // block answer the question it is named for.
                std::vector<int> seen, flips;
                for (const owner_change& c2 : sim.owner_changes)
                {
                    const std::size_t r2 = static_cast<std::size_t>(c2.region);
                    if (r2 >= seen.size()) { seen.resize(r2 + 1, 0); flips.resize(r2 + 1, 0); }
                    if (seen[r2] == 0) { seen[r2] = 1; continue; } // the founding
                    ++flips[r2];
                }
                int64_t total = 0;
                for (int f2 : flips)
                {
                    if (f2 <= 0) continue;
                    ++row.regions_touched;
                    total += f2;
                    if (f2 == 1) ++row.regions_once;
                    if (f2 >= 3) ++row.regions_thrice;
                    if (f2 > row.max_flips) row.max_flips = f2;
                }
                if (row.regions_touched > 0)
                    row.flips_per_region =
                        static_cast<int>((total * 100) / static_cast<int64_t>(row.regions_touched));
            }
            if (!sim.owner_changes.empty())
                row.early_change_pct =
                    static_cast<int>((early * 100) / static_cast<int64_t>(sim.owner_changes.size()));
        }

        row.regions_end = static_cast<int>(ss.regions.size());
        row.battles       = sim.battles;
        row.conquests     = sim.conquests;
        row.illegal_campaigns = sim.illegal_campaigns;
        row.starved_campaigns = sim.starved_campaigns;
        row.sea_legs_fed      = sim.sea_legs_fed_campaigns;
        row.reach_denied      = sim.reach_denied_campaigns;
        row.mat_trade         = sim.materials_from_trade;
        row.mat_total         = sim.materials_produced;
        row.mat_upkeep        = sim.materials_spent_on_upkeep;
        row.mat_roads         = sim.materials_spent_on_roads;
        row.mat_campaigns     = sim.materials_spent_on_campaigns;
        row.heads_unpaid      = sim.army_heads_unpaid_disbanded;
        row.roads_refused     = sim.road_builds_refused;

        // BL-925 -- HOW MANY CROSS-BORDER LINKS ARE OPEN AT THE END. Net of
        // opens and closes over the whole run, from the event layer alone
        // (`out.events`), the same source `history_lapse.cpp` bakes its
        // corridor draw from -- a fresh open/closed set here, not a read of
        // anything internal to `history_sim.cpp`.
        {
            std::unordered_set<uint64_t> open_links;
            for (const lapse_event& e : sim.events)
            {
                if (e.region == lapse_event_none || e.other == lapse_event_none) continue;
                const uint32_t lo = std::min<uint32_t>(e.region, e.other);
                const uint32_t hi = std::max<uint32_t>(e.region, e.other);
                const uint64_t key = (static_cast<uint64_t>(lo) << 32) | hi;
                if (e.kind == static_cast<uint8_t>(lapse_event_kind::trade_link_opened))
                    open_links.insert(key);
                else if (e.kind == static_cast<uint8_t>(lapse_event_kind::trade_link_closed))
                    open_links.erase(key);
            }
            row.cross_border_links_open = static_cast<int64_t>(open_links.size());
        }
        row.secessions        = sim.secessions;
        row.supply_sites_upgraded           = sim.supply_sites_upgraded;
        row.supply_sites_upgraded_regions   = sim.supply_sites_upgraded_regions;
        row.supply_sites_upgraded_corridors = sim.supply_sites_upgraded_corridors;
        row.mat_supply_sites                = sim.materials_spent_on_supply_sites;
        // BL-916 — the event layer, counted per kind. `realm_ended` is the
        // wizard's "destroyed" figure, so it is the one worth printing beside
        // the sweep's own death count.
        row.events            = static_cast<int64_t>(sim.events.size());
        for (const lapse_event& ev : sim.events)
        {
            if (ev.kind == static_cast<uint8_t>(lapse_event_kind::realm_ended)) ++row.events_ended;
            if (ev.kind == static_cast<uint8_t>(lapse_event_kind::broke_away))  ++row.events_broke;
            if (ev.kind == static_cast<uint8_t>(lapse_event_kind::seat_captured)) ++row.events_seats;
        }
        row.fear_leaned       = sim.fear_leaned_campaigns;
        row.fear_targets      = sim.fear_targets_distinct;
        row.regions_seceded   = sim.regions_seceded;
        row.seceded_graph_cut = sim.regions_seceded_graph_cut;

        // BL-922 -- THE SUPPLY DISTRIBUTION OVER HELD GROUND (bands, percentiles,
        // the graph-vs-map cut-off diagnostic; the floor counts are BL-926 s, below). Read off the
        // post-sim settlement, per region with an owner, in region-index order.
        {
            const history_sim_params& sp = derive_from_generation ? fx.params : params;
            const int sweep_gw = derive_from_generation ? fx.gw : home_grid_width;
            std::vector<int64_t> all;
            std::vector<int64_t> band[4];
            for (const region& p : ss.regions)
            {
                if (p.nation < 0) continue;
                const int64_t v = p.network_supply_q;
                ++row.held_regions;
                all.push_back(v);
                const std::size_t pid = static_cast<std::size_t>(p.nation);
                if (pid < sim.polities.size())
                {
                    const int capi = sim.polities[pid].capital;
                    if (capi >= 0 && static_cast<std::size_t>(capi) < ss.regions.size())
                    {
                        const int d = region_distance(p, ss.regions[static_cast<std::size_t>(capi)], sweep_gw);
                        const int bi = d < 10 ? 0 : d < 20 ? 1 : d < 30 ? 2 : 3;
                        band[bi].push_back(v);
                    }
                }
            }
            // WHY is a region at 0? If a fed region of the SAME realm stands
            // within `neighbour_radius` of it, the map did not cut it off --
            // the degree-capped neighbour graph (BL-855) did. Epoch-only and
            // O(N^2) over ~1,000 regions: cheap, and it is the one reading
            // that tells a graph artefact from a geographic one.
            for (std::size_t i = 0; i < ss.regions.size(); ++i)
            {
                const region& p = ss.regions[i];
                if (p.nation < 0 || p.network_supply_q > 0) continue;
                bool fed_near = false, any_near = false;
                for (std::size_t j = 0; j < ss.regions.size(); ++j)
                {
                    if (j == i) continue;
                    const region& o = ss.regions[j];
                    if (o.nation != p.nation) continue;
                    if (region_distance(p, o, sweep_gw) > sp.neighbour_radius) continue;
                    any_near = true;
                    if (o.network_supply_q > 0) { fed_near = true; break; }
                }
                if (fed_near)      ++row.zero_kin_near;
                else if (!any_near) ++row.zero_no_link;
            }
            if (!all.empty())
            {
                std::vector<int64_t> sorted = all;
                std::sort(sorted.begin(), sorted.end());
                const auto pct = [&](int q) {
                    std::size_t i = (sorted.size() - 1) * static_cast<std::size_t>(q) / 100;
                    return sorted[i]; };
                row.supply_p10 = pct(10);
                row.supply_p50 = pct(50);
                row.supply_p90 = pct(90);
            }
            for (int bi = 0; bi < 4; ++bi)
            {
                row.supply_band_n[bi]   = static_cast<int64_t>(band[bi].size());
                row.supply_band_med[bi] = band[bi].empty() ? -1 : median_of(band[bi]);
            }
            const history_sim_profile& hp = history_sim_last_profile();
            row.era_reach_ms       = hp.ns_reach / 1000000;
            row.era_reach_rebuilds = hp.reach_rebuilds;
            row.era_decisions_ms   = hp.ns_decisions / 1000000;
        }
        row.creeds_arisen     = sim.universal_creeds_arisen;
        row.creed_adoptions   = sim.polities_adopted_creed;
        row.creed_converted   = sim.peoples_converted;
        row.creed_reasserted  = sim.peoples_reasserted;
        row.schisms           = sim.schisms;
        row.regions_sundered  = sim.regions_sundered;
        row.campaign_contacts = sim.campaign_contacts;
        row.campaign_scored   = sim.campaign_scored;
        row.campaign_cleared  = sim.campaign_cleared;
        row.campaign_chosen   = sim.campaign_chosen;
        row.cleared_rounds    = sim.campaign_cleared_rounds;
        row.cleared_lost      = sim.campaign_cleared_lost;
        row.naval_battles     = sim.naval_battles;
        row.sea_leg_battles   = sim.sea_leg_battles;
        row.foundings     = sim.foundings;

        if (derive_from_generation && !tuned)
        {
            ++derived_seeds_checked;
            const bool same = sim.battles   == rep.prehistory_battles
                           && sim.conquests == rep.prehistory_conquests
                           && sim.foundings == rep.prehistory_foundings;
            if (!same)
            {
                derived_matches_generation = false;
                std::printf("  seed %u: RE-RUN DIVERGED from generation's own era —\n"
                            "          sweep  battles %lld conquests %lld foundings %lld\n"
                            "          gen    battles %lld conquests %lld foundings %lld\n",
                            wp.seed,
                            static_cast<long long>(sim.battles),
                            static_cast<long long>(sim.conquests),
                            static_cast<long long>(sim.foundings),
                            static_cast<long long>(rep.prehistory_battles),
                            static_cast<long long>(rep.prehistory_conquests),
                            static_cast<long long>(rep.prehistory_foundings));
            }
        }
        row.peak_population = sim.peak_population;
        row.peak_year       = sim.peak_year;

        row.works_raised = sim.works_raised;
        row.run_works_by_span_band = sim.works_by_span_band;
        row.units_by_span_band     = sim.units_by_span_band;

        // BL-768 — the ancient road record. Derived here from the sim's own
        // output rather than recomputed: a census that re-derived the corridor
        // set its own way would be measuring its own arithmetic.
        {
            row.corridors = static_cast<int64_t>(sim.supply_corridors.size());
            std::vector<int> degree(ss.regions.size(), 0);
            for (const history_corridor& c : sim.supply_corridors)
            {
                row.corridor_walks += c.uses;
                if (c.uses > row.corridor_max_use) row.corridor_max_use = c.uses;
                const int b = c.uses >= 4 ? 3 : (c.uses - 1);
                if (b >= 0 && b < 4) ++row.corridor_use_hist[b];
                if (c.a < degree.size()) ++degree[c.a];
                if (c.b < degree.size()) ++degree[c.b];
            }
            for (const int d : degree)
            {
                if (d > row.corridor_max_degree) row.corridor_max_degree = d;
                const int b = d == 0 ? 0 : (d <= 2 ? 1 : (d <= 4 ? 2 : 3));
                ++row.corridor_degree_hist[b];
            }
        }
        // BL-911 — the surviving network crosses the handoff, unevenly. Fold
        // the SAME sim + settlement output through the real handoff function
        // rather than re-deriving survival here: `make_pass_one_output` is
        // what the stamp pass in generation now reads, so this measures what
        // generation actually produces.
        {
            const pass_one_output o = make_pass_one_output(ss, sim, /*culture_count=*/0);
            row.surv_corridors = static_cast<int64_t>(o.surviving_corridors.size());

            std::vector<int64_t> per_polity(o.polities.size(), 0);
            for (const history_corridor& c : o.surviving_corridors)
            {
                const auto credit = [&](uint16_t idx) {
                    if (idx >= ss.regions.size()) return;
                    const int n = ss.regions[idx].nation;
                    if (n >= 0 && static_cast<std::size_t>(n) < per_polity.size())
                        ++per_polity[static_cast<std::size_t>(n)];
                };
                credit(c.a);
                credit(c.b);
            }
            int64_t mn = -1, mx = 0, holding = 0;
            for (std::size_t pi = 0; pi < per_polity.size(); ++pi)
            {
                if (!o.polities[pi].alive || per_polity[pi] == 0) continue;
                ++holding;
                if (mn < 0 || per_polity[pi] < mn) mn = per_polity[pi];
                if (per_polity[pi] > mx) mx = per_polity[pi];
            }
            row.surv_polities_holding = holding;
            row.surv_corridors_min    = mn < 0 ? 0 : mn;
            row.surv_corridors_max    = mx;

            // BL-907 -- markets standing on capitals (BL-910), read off the
            // SAME `o` rather than a second call. A polity without a seat
            // carries neither capital nor market (BL-910's NO QUOTA clause),
            // so `caps_total` is a count of living polities that actually
            // HAVE a capital, never all living polities.
            for (const polity& p : o.polities)
            {
                if (!p.alive || p.capital < 0) continue;
                ++row.caps_total;
                if (static_cast<std::size_t>(p.capital) < o.regions.size() &&
                    o.regions[static_cast<std::size_t>(p.capital)].has_market)
                    ++row.caps_with_market;
            }

            // BL-907 -- grudges still biting at 1200 CE. `grudge::score` is
            // ALREADY the decayed standing figure (the struct's own comment:
            // "the standing, decayed score"), so a positive score here means
            // the resentment survived to the close rather than decaying to
            // nothing, exactly what CIVILISATION.md's reading asks for.
            row.grudges_total = static_cast<int64_t>(o.grudges.size());
            for (const grudge& g : o.grudges)
            {
                if (g.score > 0) ++row.grudges_biting;
                if (g.score > row.grudge_max_score) row.grudge_max_score = g.score;
            }

            // BL-909 — a one-line diagnostic for the directed want table.
            // The scoreboard section (BL-907) reports on this properly; this
            // is just enough to see the count while verifying in isolation.
            std::size_t via_market = 0;
            for (const want& w : o.wants) if (w.via_market) ++via_market;
            std::printf("  wants: %zu (via_market: %zu)\n", o.wants.size(), via_market);
            row.wants_total      = static_cast<int64_t>(o.wants.size());
            row.wants_via_market = static_cast<int64_t>(via_market);
        }

        for (const region& p : ss.regions)
            if (p.works_built != 0) ++row.regions_with_works;

        // BL-757 R4 — WHICH BAND DID THE ROSTER REACH? Read from the end-state
        // masks against whichever registry was actually in play, so the answer
        // describes the run rather than the table. A null registry means no work
        // was buildable at all, and the honest report of that is four zeroes.
        {
            const works_registry* reg = derive_from_generation ? fx.works : &works;
            if (reg != nullptr)
                for (const region& p : ss.regions)
                    for (std::size_t id = 0; id < reg->size() && id < works_mask_bits; ++id)
                        if ((p.works_built & (1u << id)) != 0)
                        {
                            const work_row* wr = reg->row_at(id);
                            if (wr != nullptr)
                            {
                                const int bi = static_cast<int>(wr->band);
                                if (bi >= 0 && bi < roster_band_count) ++row.works_by_band[bi];
                            }
                        }
        }

        for (const region& p : ss.regions) row.epoch_population += p.population;

        // Shape at the start and at the epoch, plus the highest concentration
        // reached at any century — a world can form a hegemony and lose it
        // again, and only the peak shows that it happened at all.
        int dummy = 0;
        slice_shape(owner_slice_at(sim, params.start_year), row.powers_start, dummy);
        slice_shape(owner_slice_at(sim, params.stop_year), row.powers_end, row.top_share_q,
                    &row.smallest_holding);

        for (int64_t y = params.start_year; y <= params.stop_year; y += 100)
        {
            int p = 0, share = 0;
            slice_shape(owner_slice_at(sim, y), p, share);
            if (share > row.peak_share_q) row.peak_share_q = share;
            if (row.hegemony_year < 0 && share >= hegemony_threshold_q) row.hegemony_year = y;
        }

        std::vector<int64_t> ind;
        for (const region& p : ss.regions)
            if (p.industrialised) ind.push_back(p.industrial_year);
        if (!ind.empty())
        {
            std::sort(ind.begin(), ind.end());
            row.industrial_first  = ind.front();
            row.industrial_median = ind[ind.size() / 2];
            row.industrial_last   = ind.back();
        }
        row.regions_industrial = static_cast<int>(ind.size());

        // --- BL-748 R1: who crossed the rung, and how far the ladder got ---
        //
        // Read off the polities the run actually produced, not re-derived: the
        // sim writes `polity::industrial_year` at the crossing, so this is the
        // same value the furnace fired on.
        {
            std::vector<int64_t> mats;
            row.polities_total = static_cast<int>(sim.polities.size());
            for (const polity& q : sim.polities)
            {
                // THE SENTINEL, READ CORRECTLY. This tested `!= 0` — the sentinel
                // BL-748 replaced, and the trap its own header warns about. Since
                // that item "never industrialised" is `k_never_industrialised`
                // (INT64_MIN), which is not zero, so EVERY polity counted as
                // crossed and `first_cross_year` min-reduced to INT64_MIN. The
                // table printed "polities crossing the rung, median 12" over a
                // world where three did, and a first-cross column of
                // -9223372036854775808 next to it. Found while measuring BL-750's
                // tariff distribution, whose timing term reads the same field:
                // the two disagreed, and this side was the one that was wrong.
                if (q.industrial_year != k_never_industrialised)
                {
                    if (row.polities_crossed == 0 || q.industrial_year < row.first_cross_year)
                        row.first_cross_year = q.industrial_year;
                    ++row.polities_crossed;
                }
                const int m = q.capacity[static_cast<int>(sim_domain::materials)];
                mats.push_back(m);
                if (m > row.mat_cap_max) row.mat_cap_max = m;
            }
            if (!mats.empty())
            {
                std::sort(mats.begin(), mats.end());
                row.mat_cap_median = static_cast<int>(mats[mats.size() / 2]);
            }
        }

        // --- BL-970: THE SAME ARC BY POPULATION HELD ------------------------
        //
        // The three walks the region column takes -- the epoch slice, the
        // century walk and the 40-sample shape walk -- taken again over the
        // playback record's population. A sample year reads the last recorded
        // step at or before it (steps are >= record_interval_years apart, 20
        // by default, against the 40-year shape step); the epoch reads the
        // closing step the sim always writes at stop_year. See the row
        // struct's BL-970 block for why the region column alone misreads.
        std::vector<int>     peak_pop_share_of;
        std::vector<int64_t> peak_pop_of;
        {
            const int n_pol = static_cast<int>(sim.polities.size());
            peak_pop_share_of.assign(static_cast<std::size_t>(n_pol), 0);
            peak_pop_of.assign(static_cast<std::size_t>(n_pol), 0);
            std::vector<int64_t> pop;

            // The epoch, as `top_share_q` / `smallest_holding` take it.
            const pop_slice e = pop_slice_at(sim, params.stop_year, &pop);
            row.pop_recorded         = e.recorded;
            row.top_share_pop_q      = share_q_of(e.top, e.total);
            row.smallest_holding_pop = e.smallest;

            // The century walk, as `peak_share_q` / `hegemony_year` take it.
            for (int64_t y = params.start_year; y <= params.stop_year; y += 100)
            {
                const pop_slice c = pop_slice_at(sim, y, nullptr);
                const int share = share_q_of(c.top, c.total);
                if (share > row.peak_share_pop_q) row.peak_share_pop_q = share;
                if (row.hegemony_year_pop < 0 && share >= hegemony_threshold_q)
                    row.hegemony_year_pop = y;
            }

            // The 40-sample shape walk, per polity, in per-mille of held
            // population. ROSE mirrors the region rule exactly: at least
            // doubled, and by at least three AVERAGE REGIONS' worth of share
            // at the first sample (the region rule's "+3 regions", in the same
            // currency as the share). FELL: ended at or under 60% of its peak.
            const int64_t step = std::max<int64_t>(1, span / 40);
            std::vector<int> start_q(static_cast<std::size_t>(n_pol), 0);
            std::vector<int> peak_q(static_cast<std::size_t>(n_pol), 0);
            std::vector<int> end_q(static_cast<std::size_t>(n_pol), 0);
            int  unit_q       = 0;
            bool first_sample = true;
            for (int64_t y = params.start_year; y <= params.stop_year; y += step)
            {
                const pop_slice c = pop_slice_at(sim, y, &pop);
                if (first_sample)
                    unit_q = c.regions > 0 ? static_cast<int>(3000 / c.regions) : 0;
                for (int i = 0; i < n_pol; ++i)
                {
                    const std::size_t ui = static_cast<std::size_t>(i);
                    const int q = share_q_of(pop[ui], c.total);
                    if (first_sample) start_q[ui] = q;
                    if (q > peak_q[ui]) peak_q[ui] = q;
                    if (q > row.shape_top_peak_pop_q) row.shape_top_peak_pop_q = q;
                }
                first_sample = false;
            }
            {
                const pop_slice c = pop_slice_at(sim, params.stop_year, &pop);
                for (int i = 0; i < n_pol; ++i)
                {
                    const std::size_t ui = static_cast<std::size_t>(i);
                    end_q[ui] = share_q_of(pop[ui], c.total);
                }
            }
            for (int i = 0; i < n_pol; ++i)
            {
                const std::size_t ui = static_cast<std::size_t>(i);
                const bool rose = peak_q[ui] > 0
                               && peak_q[ui] >= 2 * start_q[ui]
                               && peak_q[ui] >= start_q[ui] + unit_q;
                const bool fell = peak_q[ui] > 0 && end_q[ui] * 1000 <= peak_q[ui] * 600;
                if (rose) ++row.shape_rose_pop;
                if (fell) ++row.shape_fell_pop;
                if (rose && fell) ++row.shape_rpf_pop;
            }

            // Every recorded step, for the arc table's per-polity peak.
            for (const timelapse_step& st : sim.steps)
            {
                int64_t total = 0;
                for (int k = 0; k < st.sample_count; ++k)
                    total += sim.samples[static_cast<std::size_t>(st.first_sample + k)].population;
                if (total <= 0) continue;
                for (int k = 0; k < st.sample_count; ++k)
                {
                    const polity_sample& smp = sim.samples[static_cast<std::size_t>(st.first_sample + k)];
                    if (smp.polity >= static_cast<uint16_t>(n_pol)) continue;
                    const int q = share_q_of(smp.population, total);
                    if (q > peak_pop_share_of[smp.polity])
                    {
                        peak_pop_share_of[smp.polity] = q;
                        peak_pop_of[smp.polity]       = smp.population;
                    }
                }
            }
        }

        // --- BL-767 R2: the rise-peak-fall shape, per polity ---------------
        //
        // A SEPARATE, FINER WALK than the century sampler above, deliberately.
        // The century walk feeds `hegemony_year` and `peak_share_q` and those
        // are published numbers; re-sampling them to serve this measure would
        // move figures nobody asked to move. Forty samples over the span is
        // fine enough that a polity which rose and fell inside one century is
        // still seen, and each `owner_slice_at` is one forward walk of the
        // change list.
        //
        // WHAT IS AND IS NOT MEASURED. This reports a SHAPE, per polity, and
        // nothing here gates or guarantees it. A world where nothing rises is a
        // legitimate world (the 2026-08-31 asymmetry ruling, the 2026-07-30
        // emergent-nation-count ruling, BL-224's non-hegemony invariant), so
        // the row is a count and the aggregate is a distribution.
        {
            const int      n_pol = static_cast<int>(sim.polities.size());
            const int64_t  step  = std::max<int64_t>(1, span / 40);
            std::vector<int> start(static_cast<std::size_t>(n_pol), 0);
            std::vector<int> peak(static_cast<std::size_t>(n_pol), 0);
            std::vector<int> endh(static_cast<std::size_t>(n_pol), 0);

            bool first_sample = true;
            for (int64_t y = params.start_year; y <= params.stop_year; y += step)
            {
                const std::vector<uint16_t> slice = owner_slice_at(sim, y);
                std::vector<int> count(static_cast<std::size_t>(n_pol), 0);
                int live = 0;
                for (uint16_t o : slice)
                {
                    if (o == owner_none) continue;
                    ++live;
                    if (o < static_cast<uint16_t>(n_pol)) ++count[o];
                }
                int city_states_now = 0; // BL-926: exactly one region held.
                for (int i = 0; i < n_pol; ++i)
                {
                    const std::size_t ui = static_cast<std::size_t>(i);
                    if (first_sample) start[ui] = count[ui];
                    if (count[ui] > peak[ui]) peak[ui] = count[ui];
                    if (count[ui] == 1) ++city_states_now;
                    if (live > 0)
                    {
                        const int share = (count[ui] * 1000) / live;
                        if (share > row.shape_top_peak_q) row.shape_top_peak_q = share;
                    }
                }
                row.city_states_series.push_back(city_states_now);
                row.city_states_step = step;
                first_sample = false;
            }
            {
                const std::vector<uint16_t> slice = owner_slice_at(sim, params.stop_year);
                for (uint16_t o : slice)
                    if (o != owner_none && o < static_cast<uint16_t>(n_pol)) ++endh[o];
            }

            for (int i = 0; i < n_pol; ++i)
            {
                const std::size_t ui = static_cast<std::size_t>(i);
                // ROSE: at least doubled, and by at least three regions — so a
                // polity going from one region to two is not a rise.
                const bool rose = peak[ui] >= 2 * start[ui] && peak[ui] >= start[ui] + 3;
                // FELL: ended at or under 60% of its own peak. A power that is
                // still at its height at the epoch has not fallen; it is simply
                // where the run stopped.
                const bool fell = peak[ui] > 0 && endh[ui] * 1000 <= peak[ui] * 600;
                if (rose) ++row.shape_rose;
                if (fell) ++row.shape_fell;
                if (rose && fell) ++row.shape_rpf;
            }
        }

        // --- BL-926: THE ARC, per polity, replayed EXACTLY -------------------
        //
        // One forward walk of `owner_changes`, tracking every region's owner
        // and every polity's count. Born = first region held; peak = the most
        // held at once and the first year it was; died = the year the count
        // returned to zero; killer = the owner the emptying change handed the
        // last region to. NOTHING HERE READS THE SIM'S DECISION STATE and
        // nothing gates: a world of immortal city states is a legitimate row.
        {
            const int n_pol = static_cast<int>(sim.polities.size());
            std::vector<int> owner_of;                 // region -> polity id, -1 unowned
            std::vector<int> count(static_cast<std::size_t>(n_pol), 0);
            std::vector<sweep_row::polity_arc> arcs(static_cast<std::size_t>(n_pol));
            std::vector<char> ever(static_cast<std::size_t>(n_pol), 0);
            int live = 0;
            for (int i = 0; i < n_pol; ++i)
            {
                arcs[static_cast<std::size_t>(i)].id     = i;
                arcs[static_cast<std::size_t>(i)].parent = sim.polities[static_cast<std::size_t>(i)].parent;
                arcs[static_cast<std::size_t>(i)].alive  = sim.polities[static_cast<std::size_t>(i)].alive;
            }
            // PEAKS ARE READ AT YEAR-END, not per change. The seeding year
            // writes one change per region in sequence, so a share taken
            // mid-batch would credit the first polity written with most of
            // a map that is still being filled in.
            const auto flush_year = [&](int64_t year) {
                for (int i = 0; i < n_pol; ++i)
                {
                    const std::size_t ui = static_cast<std::size_t>(i);
                    if (count[ui] > arcs[ui].peak)
                    {
                        arcs[ui].peak         = count[ui];
                        arcs[ui].peak_year    = year;
                        arcs[ui].peak_share_q = live > 0 ? (count[ui] * 1000) / live : 0;
                    }
                }
            };
            bool    have_year = false;
            int64_t cur_year  = 0;
            for (const owner_change& c : sim.owner_changes)
            {
                if (have_year && c.year != cur_year) flush_year(cur_year);
                cur_year = c.year; have_year = true;

                const std::size_t r = static_cast<std::size_t>(c.region);
                if (r >= owner_of.size()) owner_of.resize(r + 1, -1);
                const int prev = owner_of[r];
                const int next = c.owner == owner_none ? -1 : static_cast<int>(c.owner);
                if (prev == next) continue;
                if (prev < 0 && next >= 0) ++live;
                if (prev >= 0 && next < 0) --live;
                owner_of[r] = next;
                if (prev >= 0 && prev < n_pol)
                {
                    const std::size_t up = static_cast<std::size_t>(prev);
                    if (--count[up] == 0)
                    {
                        arcs[up].died   = c.year;
                        arcs[up].killer = next;
                    }
                }
                if (next >= 0 && next < n_pol)
                {
                    const std::size_t un = static_cast<std::size_t>(next);
                    if (!ever[un]) { ever[un] = 1; arcs[un].born = c.year; }
                    ++count[un];
                    // A polity that emptied and was refilled is not dead.
                    arcs[un].died = INT64_MIN; arcs[un].killer = -1;
                }
            }
            if (have_year) flush_year(cur_year);
            for (int i = 0; i < n_pol; ++i)
            {
                const std::size_t ui = static_cast<std::size_t>(i);
                if (!ever[ui]) continue;
                arcs[ui].end_hold = count[ui];
                arcs[ui].peak_pop_share_q = peak_pop_share_of[ui]; // BL-970
                arcs[ui].peak_pop         = peak_pop_of[ui];
                ++row.polities_ever_held;
                if (!arcs[ui].alive) ++row.polities_dead_gross;
                row.arcs.push_back(arcs[ui]);
            }
            // Top-N by peak share, ties by id -- a stable, seed-independent order.
            std::stable_sort(row.arcs.begin(), row.arcs.end(),
                             [](const sweep_row::polity_arc& a, const sweep_row::polity_arc& b) {
                                 if (a.peak_share_q != b.peak_share_q) return a.peak_share_q > b.peak_share_q;
                                 return a.id < b.id;
                             });

            row.breakdowns  = sim.breakdowns;
            row.piece_sizes = sim.secession_piece_sizes;
            row.foundings_scheduled = sim.foundings_scheduled;
            row.foundings_settled   = sim.foundings_settled;
            row.civilisations       = sim.civilisations_formed;
            row.organised           = sim.organised;          // BL-920
            row.city_states_risen   = sim.city_states_risen;  // BL-920
            row.conquests           = sim.conquests;          // BL-920: read beside `organised`.

            // The supply histogram, over HELD ground at the stop year, against
            // the floors the run actually used.
            row.floor_settle   = ran.sustainable_settlement_floor_q;
            row.floor_secede   = ran.secession_supply_floor_q;
            row.floor_campaign = ran.sustainable_campaign_floor_q;
            for (const region& p : ss.regions)
            {
                if (p.nation < 0) continue;
                ++row.supply_held;
                const int s = p.network_supply_q;
                if (s == 0)                  ++row.supply_zero;
                if (s <= row.floor_settle)   ++row.supply_le_settle;
                if (s <= row.floor_secede)   ++row.supply_le_secede;
                if (s <  row.floor_campaign) ++row.supply_le_campaign;
                const int b = std::clamp(s / 100, 0, 9);
                ++row.supply_hist[b];
            }

            // The culture census: count, roots, and the deepest parent chain.
            // Bounded: a parent is always lower-indexed (creeds.hpp), so the
            // walk cannot loop; a malformed link just stops it.
            if (derive_from_generation)
            {
                const std::vector<culture>& cu = fx.creeds.cultures;
                row.cultures = static_cast<int>(cu.size());
                row.cultures_root = 0;
                row.culture_depth = 0;
                for (std::size_t ci = 0; ci < cu.size(); ++ci)
                {
                    if (cu[ci].parent < 0) ++row.cultures_root;
                    int depth = 0;
                    int at = static_cast<int>(ci);
                    while (at >= 0 && static_cast<std::size_t>(at) < cu.size()
                           && cu[static_cast<std::size_t>(at)].parent >= 0
                           && cu[static_cast<std::size_t>(at)].parent < at)
                    { at = cu[static_cast<std::size_t>(at)].parent; ++depth; }
                    if (depth > row.culture_depth) row.culture_depth = depth;
                }
            }

            row.arc_name  = derive_from_generation ? "generation" : "struct-default";
            if (tuned) row.arc_name += "+tuned";
            row.arc_start = ran.start_year;
            row.arc_stop  = ran.stop_year;
        }

        // --- BL-750: the scalar at POLITY grain -----------------------------
        //
        // Under `--epoch` this `sim` IS generation's own era (S1b asserts the
        // counters match the report), so these figures describe the run whose
        // output the tariff table below was banded from.
        {
            const tariff_bands pb;
            std::vector<int64_t> prot, yrs;
            for (const polity& q : sim.polities)
            {
                if (!q.alive) continue;
                ++row.polities_alive;
                prot.push_back(q.protection_q);
                if (q.protection_q > row.protection_pmax) row.protection_pmax = q.protection_q;
                if (q.protection_q >= pb.threshold_q)     ++row.polities_over_floor;
                if (q.industrial_year != k_never_industrialised) yrs.push_back(q.industrial_year);
            }
            row.protection_pmed = static_cast<int>(median_of(prot));
            std::sort(yrs.begin(), yrs.end());
            yrs.erase(std::unique(yrs.begin(), yrs.end()), yrs.end());
            row.distinct_cross_yrs = static_cast<int>(yrs.size());
        }

        // --- BL-750 / BL-769: read the GENERATED world, not the re-run ------
        //
        // Both items land at the HANDOFF, so both are measured on `w` and on the
        // report's post-sim settlement -- the state generation actually handed to
        // the campaign. The sweep's own `sim` above is a research ladder on
        // struct defaults unless `--epoch` was passed, and reading these numbers
        // off it would report a history no player ever gets.
        {
            row.nations_total = static_cast<int>(w.nations.size());

            const tariff_bands bands; // the shipped defaults; the seeder's own
            for (const law& l : w.laws)
            {
                if (!l.enacted || l.effect != law_effect_kind::import_tariff) continue;
                ++row.nations_tariffed;
                if      (l.rate >= bands.rate_high) ++row.tariff_band_high;
                else if (l.rate >= bands.rate_mid)  ++row.tariff_band_mid;
                else                                ++row.tariff_band_low;
            }

            // The posture itself, off generation's own post-sim settlement. Per
            // REGION rather than per polity, because that is the grain the
            // report carries -- and a region's value IS its holder's, broadcast.
            std::vector<int64_t> post;
            for (const region& p : k->settlement.regions)
            {
                if (p.protection_q > row.protection_max_q)
                    row.protection_max_q = p.protection_q;
                post.push_back(p.protection_q);
            }
            row.protection_med_q = static_cast<int>(median_of(post));

            // BL-769: nations under the size floor are the city states the
            // fold's exemption kept -- before it, no nation could finish below
            // the floor at all. A reporting definition read from OUTSIDE the
            // generator, deliberately not its own mask: if the two ever
            // disagree that is a finding rather than a tautology.
            for (const auto& [nid, nc] : w.nations)
            {
                (void)nid;
                if (static_cast<int>(nc.tiles.size()) < nation_params{}.min_nation_tiles)
                    ++row.city_states;
            }
        }

        const auto t1 = std::chrono::steady_clock::now();
        row.ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        rows.push_back(row);
    }

    // --- The table ---------------------------------------------------------
    // BL-970: every share prints TWICE -- regions held / population held.
    std::printf("seed  prov(0>epoch)  powers(0>epoch)  top%% reg/pop  weakest reg/pop(k)  hegem reg/pop  battles  conq  "
                "peak pop (yr)      epoch pop     ms   works(prov)\n");
    std::printf("----  -------------  ---------------  ------------  ------------------  -------------  -------  ----  "
                "-----------------  ------------  ----  -----------\n");
    for (const sweep_row& r : rows)
    {
        char heg[16], hegp[16];
        if (r.hegemony_year < 0) std::snprintf(heg, sizeof heg, "  -  ");
        else                     std::snprintf(heg, sizeof heg, "%5lld",
                                               static_cast<long long>(r.hegemony_year));
        if (r.hegemony_year_pop < 0) std::snprintf(hegp, sizeof hegp, "  -  ");
        else                         std::snprintf(hegp, sizeof hegp, "%5lld",
                                                   static_cast<long long>(r.hegemony_year_pop));
        std::printf("%4u  %5d > %5d  %6d > %6d  %4d%%/%4d%%   %5d/%9lldk    %s/%s  %7lld  %4lld  %11lld (%4lld)  %12lld  %4lld  %5lld (%4d)\n",
                    r.seed, r.regions_start, r.regions_end,
                    r.powers_start, r.powers_end,
                    r.top_share_q / 10, r.top_share_pop_q / 10,
                    r.smallest_holding, static_cast<long long>(r.smallest_holding_pop / 1000),
                    heg, hegp,
                    static_cast<long long>(r.battles), static_cast<long long>(r.conquests),
                    static_cast<long long>(r.peak_population), static_cast<long long>(r.peak_year),
                    static_cast<long long>(r.epoch_population),
                    static_cast<long long>(r.ms),
                    static_cast<long long>(r.works_raised), r.regions_with_works);
    }

    // --- BL-760 (1): the band ceiling gets an observable ------------------
    //
    // WITHOUT THIS SECTION span1_band_ceiling = medieval and = industrial are
    // indistinguishable from outside the sim. `works_raised` is one scalar with
    // no band split, so if no polity reaches materials capacity 4 before the
    // boundary the clamp never binds and every check passes either way — which
    // is how a two-span requirement came to be marked complete on substituted
    // evidence.
    //
    // READ THE WORKS ROWS WITH BL-757 IN HAND: it measured ZERO works raised
    // across sixteen seeds because build_work never wins the scored contest. A
    // row of zeros here therefore has two causes and this counter cannot
    // separate them. The UNITS rows are the ones carrying signal today.
    {
        static const char* kBand[roster_band_count] =
            { "classical", "medieval", "gunpowder", "industrial" };
        std::array<std::array<int64_t, roster_band_count>, 2> w{}, u{};
        for (const auto& r : rows)
            for (int sp = 0; sp < 2; ++sp)
                for (int b = 0; b < roster_band_count; ++b)
                {
                    w[static_cast<std::size_t>(sp)][static_cast<std::size_t>(b)]
                        += r.run_works_by_span_band[static_cast<std::size_t>(sp)][static_cast<std::size_t>(b)];
                    u[static_cast<std::size_t>(sp)][static_cast<std::size_t>(b)]
                        += r.units_by_span_band[static_cast<std::size_t>(sp)][static_cast<std::size_t>(b)];
                }

        // ------------------------------------------------------------------
        // BL-768 — THE ANCIENT ROAD RECORD, per seed and pooled.
        //
        // This is the instrument the two stamping constants are read off, which
        // is why it prints a distribution rather than a mean. `kAncientRoadUses`
        // (road_generation.cpp) is the point where the traffic histogram's tail
        // starts, and `kMarketJunctionDegree` (hard_coded_world.cpp) is where
        // the degree histogram stops being lines and starts being crossings. A
        // future re-measure is therefore reading two rows off this block rather
        // than reopening an argument.
        //
        // REPORTED, NEVER GATED, on the same terms as the hegemony and
        // elimination rates above: how many corridors a world walks is a
        // consequence of how much history happened in it, and a quiet world
        // legitimately walks fewer. The only assertion is presence — a sim that
        // recorded NOTHING while founding and fighting would mean the record is
        // not wired, which is a defect rather than a quiet world.
        {
            int64_t tot_c = 0, tot_w = 0, uh[4] = {0,0,0,0}, dh[4] = {0,0,0,0};
            int64_t max_use = 0, max_deg = 0;
            for (const auto& r : rows)
            {
                tot_c += r.corridors;
                tot_w += r.corridor_walks;
                for (int i = 0; i < 4; ++i) { uh[i] += r.corridor_use_hist[i];
                                              dh[i] += r.corridor_degree_hist[i]; }
                if (r.corridor_max_use    > max_use) max_use = r.corridor_max_use;
                if (r.corridor_max_degree > max_deg) max_deg = r.corridor_max_degree;
            }
            std::printf("\n--- BL-768  THE ANCIENT ROAD RECORD (all seeds) ---\n");
            std::printf("  corridors recorded   %lld distinct, %lld walks, busiest %lld\n",
                        static_cast<long long>(tot_c), static_cast<long long>(tot_w),
                        static_cast<long long>(max_use));
            std::printf("  traffic histogram    1 use %lld | 2 %lld | 3 %lld | 4+ %lld"
                        "   <- kAncientRoadUses reads this row\n",
                        static_cast<long long>(uh[0]), static_cast<long long>(uh[1]),
                        static_cast<long long>(uh[2]), static_cast<long long>(uh[3]));
            std::printf("  region degree        0 %lld | 1-2 %lld | 3-4 %lld | 5+ %lld"
                        "  (busiest %lld)  <- kMarketJunctionDegree reads this row\n",
                        static_cast<long long>(dh[0]), static_cast<long long>(dh[1]),
                        static_cast<long long>(dh[2]), static_cast<long long>(dh[3]),
                        static_cast<long long>(max_deg));
            check(tot_c > 0,
                  "BL-768 the sim records supply corridors (the ancient road record is wired)");
        }

        // BL-916 — the event layer, one line: total and the three kinds the
        // wizard's ticker and arc readout are judged on. `realm ended` here is
        // the figure the round's "destroyed" must match on the same seed.
        {
            int64_t ev = 0, ended = 0, broke = 0, seats = 0;
            for (const auto& r : rows)
            { ev += r.events; ended += r.events_ended; broke += r.events_broke; seats += r.events_seats; }
            std::printf("  events recorded      %lld (%lld realm ended, %lld broke away, %lld seats captured)"
                        "   <- BL-916 the wizard's ticker reads this\n",
                        static_cast<long long>(ev), static_cast<long long>(ended),
                        static_cast<long long>(broke), static_cast<long long>(seats));
            check(ev > 0, "BL-916 the sim records typed events (the event layer is wired)");
        }

        // ------------------------------------------------------------------
        // BL-911 — THE SURVIVING NETWORK CROSSES THE HANDOFF, UNEVENLY.
        //
        // Read off `pass_one_output::surviving_corridors`, per seed, so this
        // reports what the stamp pass in generation now actually consumes.
        // The SPREAD (min vs max corridor-ends held by a surviving polity) is
        // the reading the contract is judged on, never the total — an even
        // split across holders is the flat map the design exists to avoid.
        {
            int64_t tot_sc = 0, tot_c2 = 0;
            std::vector<int64_t> spreads; // max - min per seed, holders >= 2 only
            for (const auto& r : rows)
            {
                tot_sc += r.surv_corridors;
                tot_c2 += r.corridors;
                std::printf("  seed row: surviving %lld/%lld corridors held by %lld polities"
                            "  (min %lld, max %lld)\n",
                            static_cast<long long>(r.surv_corridors),
                            static_cast<long long>(r.corridors),
                            static_cast<long long>(r.surv_polities_holding),
                            static_cast<long long>(r.surv_corridors_min),
                            static_cast<long long>(r.surv_corridors_max));
                if (r.surv_polities_holding >= 2)
                    spreads.push_back(r.surv_corridors_max - r.surv_corridors_min);
            }
            std::printf("\n--- BL-911  THE SURVIVING NETWORK CROSSES THE HANDOFF (all seeds) ---\n");
            std::printf("  surviving corridors   %lld of %lld recorded (the rest died with their realm)\n",
                        static_cast<long long>(tot_sc), static_cast<long long>(tot_c2));
            if (!spreads.empty())
            {
                std::sort(spreads.begin(), spreads.end());
                const int64_t med = spreads[spreads.size() / 2];
                std::printf("  spread (max-min held) across %zu multi-holder seeds:"
                            " smallest %lld, median %lld, largest %lld\n",
                            spreads.size(), static_cast<long long>(spreads.front()),
                            static_cast<long long>(med), static_cast<long long>(spreads.back()));
                check(spreads.back() > 0,
                      "BL-911 the surviving network is held UNEVENLY across surviving polities");
            }
            else
            {
                std::printf("  no seed left two or more surviving polities holding network --"
                            " spread unmeasurable this run\n");
            }
        }

        // ------------------------------------------------------------------
        // BL-907 -- THE CLOSURE CONTRACT SCOREBOARD, at 1200 CE.
        //
        // CIVILISATION.md sec "What the closure is judged on" names SEVEN
        // readings a mechanism inside this phase is judged by whether it
        // moves. Every reading below is read OVER THE SEED SPREAD, never
        // per-world -- a seed refusing a reading is legitimate, a SPREAD
        // refusing one is not, and the only PASS/FAIL rows are asserted at
        // the spread level for that reason. NO TARGET NUMBERS: this table
        // is the measurement the admissible band gets argued from later, not
        // an argument for one already.
        {
            std::printf("\n=== BL-907  CLOSURE CONTRACT SCOREBOARD (1200 CE, %d seeds) ===\n",
                        static_cast<int>(rows.size()));

            // Reading 1 -- EXPLORER SET (BL-912, EMPIRE_TREE_WIRED_TO_SIM).
            // The rim milestone is a per-polity boolean read off the empire
            // tree's mask (EM-SP-4m), already gathered into
            // row.empire_polities_alive/empire_polities_rim above.
            {
                int64_t alive_t = 0, rim_t = 0;
                int seeds_with_rim = 0, seeds_all_rim = 0;
                for (const auto& r : rows)
                {
                    alive_t += r.empire_polities_alive;
                    rim_t   += r.empire_polities_rim;
                    if (r.empire_polities_rim > 0) ++seeds_with_rim;
                    if (r.empire_polities_alive > 0
                        && r.empire_polities_rim == r.empire_polities_alive)
                        ++seeds_all_rim;
                }
                std::printf("  1. explorer set        %lld/%lld surviving polities hold the rim"
                            " (EM-SP-4m) across all seeds  |  %d/%zu seeds show at least one"
                            "  |  %d/%zu seeds show EVERY survivor holding it\n",
                            static_cast<long long>(rim_t), static_cast<long long>(alive_t),
                            seeds_with_rim, rows.size(), seeds_all_rim, rows.size());
                check(seeds_with_rim > 0,
                      "BL-907.1 at least some seeds see a surviving polity cross the rim");
                check(seeds_all_rim < static_cast<int>(rows.size()),
                      "BL-907.1 the rim discriminates -- not every seed hands it to everyone");
            }

            // Reading 2 -- STRENGTH SPREAD. Surviving realms of unequal size,
            // off the SAME top_share_q / smallest_holding this report already
            // takes at stop_year (the epoch), never a private re-slice.
            //
            // BL-970: the ASSERTION reads the POPULATION-weighted column
            // (`top_share_pop_q` / `smallest_holding_pop`); the region-weighted
            // column stays printed beside it. See the row struct for why a
            // share of regions on a map that grew 2x-6x under the reading is
            // not a share of the world.
            {
                std::vector<int64_t> tops, smalls2, survivors, tops_p, smalls_p;
                for (const auto& r : rows)
                {
                    tops.push_back(r.top_share_q);
                    smalls2.push_back(r.smallest_holding);
                    survivors.push_back(r.powers_end);
                    tops_p.push_back(r.top_share_pop_q);
                    smalls_p.push_back(r.smallest_holding_pop);
                }
                const int64_t top_med   = median_of(tops);
                const int64_t small_med = median_of(smalls2);
                const int64_t surv_med  = median_of(survivors);
                const int64_t top_pmed  = median_of(tops_p);
                const int64_t small_pmed = median_of(smalls_p);
                std::sort(tops.begin(), tops.end());
                std::sort(smalls2.begin(), smalls2.end());
                std::sort(tops_p.begin(), tops_p.end());
                std::sort(smalls_p.begin(), smalls_p.end());
                std::printf("  2. strength spread     surviving polities/world: median %lld\n"
                            "       by REGIONS held     largest holder share (per-mille): min %lld, median %lld, max %lld"
                            "  |  smallest survivor's regions: min %lld, median %lld, max %lld\n"
                            "       by POPULATION held  largest holder share (per-mille): min %lld, median %lld, max %lld"
                            "  |  smallest survivor's heads: min %lld, median %lld, max %lld   <- asserted (BL-970)\n",
                            static_cast<long long>(surv_med),
                            static_cast<long long>(tops.empty() ? 0 : tops.front()),
                            static_cast<long long>(top_med),
                            static_cast<long long>(tops.empty() ? 0 : tops.back()),
                            static_cast<long long>(smalls2.empty() ? 0 : smalls2.front()),
                            static_cast<long long>(small_med),
                            static_cast<long long>(smalls2.empty() ? 0 : smalls2.back()),
                            static_cast<long long>(tops_p.empty() ? 0 : tops_p.front()),
                            static_cast<long long>(top_pmed),
                            static_cast<long long>(tops_p.empty() ? 0 : tops_p.back()),
                            static_cast<long long>(smalls_p.empty() ? 0 : smalls_p.front()),
                            static_cast<long long>(small_pmed),
                            static_cast<long long>(smalls_p.empty() ? 0 : smalls_p.back()));
                check(!tops_p.empty() && tops_p.back() < 1000,
                      "BL-907.2 at least some seeds show unequal strength (largest holder < 100% of population held)");
            }

            // Reading 3 -- CONTACT. At least one pair unmet, so there is
            // somewhere to go, read off `row.unmet_pair` already computed
            // above from `sim.contacts` (BL-908).
            {
                int unmet_seeds = 0;
                std::vector<int64_t> pairs2;
                for (const auto& r : rows)
                {
                    if (r.unmet_pair) ++unmet_seeds;
                    pairs2.push_back(r.contact_pairs);
                }
                std::printf("  3. contact              %d/%zu seeds carry at least one unmet living"
                            " pair  |  contact_pairs recorded: median %lld\n",
                            unmet_seeds, rows.size(),
                            static_cast<long long>(median_of(pairs2)));
                check(unmet_seeds > 0,
                      "BL-907.3 at least some seeds leave a living pair unmet at 1200 CE");
            }

            // Reading 4 -- DIRECTED WANTS (BL-909, DIRECTED_WANT_TABLE). Read
            // off row.wants_total/wants_via_market, already gathered above.
            {
                int64_t wants_t = 0, wants_m = 0;
                int seeds_with_wants = 0;
                for (const auto& r : rows)
                {
                    wants_t += r.wants_total;
                    wants_m += r.wants_via_market;
                    if (r.wants_total > 0) ++seeds_with_wants;
                }
                std::printf("  4. directed wants       %lld wants recorded across all seeds"
                            "  |  %lld via a market  |  %d/%zu seeds carry at least one want\n",
                            static_cast<long long>(wants_t), static_cast<long long>(wants_m),
                            seeds_with_wants, rows.size());
                check(seeds_with_wants > 0,
                      "BL-907.4 at least some seeds carry a directed want at 1200 CE");
            }

            // Reading 5 -- MARKETS. Standing on capitals (BL-910), off
            // `row.caps_total` / `row.caps_with_market` above.
            {
                int64_t caps_t = 0, caps_m = 0;
                int seeds_with_market = 0;
                for (const auto& r : rows)
                {
                    caps_t += r.caps_total;
                    caps_m += r.caps_with_market;
                    if (r.caps_with_market > 0) ++seeds_with_market;
                }
                std::printf("  5. markets              %lld/%lld capitals carry a market across all"
                            " seeds  |  %d/%zu seeds show at least one market standing\n",
                            static_cast<long long>(caps_m), static_cast<long long>(caps_t),
                            seeds_with_market, rows.size());
                check(caps_t > 0,
                      "BL-907.5 at least some seeds leave a living polity holding a capital");
                check(seeds_with_market > 0,
                      "BL-907.5 at least some seeds stand a market on a capital by 1200 CE");
            }

            // Reading 6 -- INHERITED NETWORK. Surviving roads held unevenly
            // across the fragments, off the BL-911 spread already reported
            // just above -- restated here so the whole contract prints as
            // one table rather than requiring the reader to scroll back.
            {
                int multi_holder_seeds = 0;
                for (const auto& r : rows) if (r.surv_polities_holding >= 2) ++multi_holder_seeds;
                std::printf("  6. inherited network    %d/%zu seeds leave 2+ surviving polities"
                            " holding network (see BL-911 spread above for the held-unevenly figure)\n",
                            multi_holder_seeds, rows.size());
                check(multi_holder_seeds > 0,
                      "BL-907.6 at least some seeds leave the network split across multiple survivors");
            }

            // Reading 7 -- GRUDGES. Still biting at 1200 CE -- carried, not
            // decayed to nothing, off `grudge::score`, the decayed standing
            // figure the struct itself carries.
            {
                int64_t gt = 0, gb = 0, gmax = 0;
                int seeds_biting = 0;
                for (const auto& r : rows)
                {
                    gt += r.grudges_total;
                    gb += r.grudges_biting;
                    if (r.grudge_max_score > gmax) gmax = r.grudge_max_score;
                    if (r.grudges_biting > 0) ++seeds_biting;
                }
                std::printf("  7. grudges              %lld/%lld recorded pairs still biting"
                            " (positive standing score) at 1200 CE across all seeds"
                            "  |  highest single standing score: %lld  |  %d/%zu seeds carry a biting"
                            " grudge\n",
                            static_cast<long long>(gb), static_cast<long long>(gt),
                            static_cast<long long>(gmax), seeds_biting, rows.size());
                check(seeds_biting > 0,
                      "BL-907.7 at least some seeds carry a grudge that has not decayed to nothing");
            }

            std::printf("\n  All seven readings above are live -- the closure contract's baseline,\n"
                        "  measured over this sweep's seed spread rather than argued in prose.\n");
        }

        std::printf("\n--- raised and fielded during the run, SPAN x BAND (all seeds) ---\n");
        for (int sp = 0; sp < 2; ++sp)
        {
            std::printf("  span %d (%s)\n", sp, sp == 0 ? "ancient" : "industrial");
            for (int b = 0; b < roster_band_count; ++b)
                std::printf("    %-11s works %8lld   units %12lld\n", kBand[b],
                            static_cast<long long>(w[static_cast<std::size_t>(sp)][static_cast<std::size_t>(b)]),
                            static_cast<long long>(u[static_cast<std::size_t>(sp)][static_cast<std::size_t>(b)]));
        }

        int64_t span1_total = 0;
        for (int b = 0; b < roster_band_count; ++b)
            span1_total += u[1][static_cast<std::size_t>(b)] + w[1][static_cast<std::size_t>(b)];

        // B1 ASSERTS ONLY WHERE THE CEILING IS ACTUALLY SET, and that gate is
        // the honest half. On a single-span arc sim_band_ceiling returns
        // `industrial` for every year - i.e. NO restriction - so asserting
        // "nothing above medieval" there would be asserting a property the sim
        // does not enforce, and it would pass today only because no polity in
        // these seeds reaches military capacity 4. One capacity step and it
        // would report a ceiling breach on a run with no ceiling.
        if (span1_total > 0)
            check(u[0][2] == 0 && u[0][3] == 0 && w[0][2] == 0 && w[0][3] == 0,
                  "B1   with an industrial span present, span 0 stays at or below medieval");
        else
            std::printf("  [SKIP] B1   single-span arc: sim_band_ceiling is unrestricted here,\n"
                        "              so there is no ceiling to assert. Run with --epoch 1960.\n");
    }

    // --- BL-912: THE EMPIRE TREE, ACROSS THE SWEEP --------------------------
    //
    // The measured claim the item asks for: SOME surviving realms hold the
    // rim (EM-SP-4m) and NOT ALL. Either extreme — 0 seeds or every seed at
    // 100% — is a finding that the mechanism is not discriminating, reported
    // rather than papered over.
    if (!rows.empty())
    {
        int worlds_with_rim = 0, worlds_all_rim = 0;
        int64_t total_alive = 0, total_rim = 0;
        std::printf("\n--- BL-912  THE EMPIRE TREE (all seeds) ---\n");
        std::printf("  seed  alive  rim  mean nodes  best nodes\n");
        for (const sweep_row& r : rows)
        {
            std::printf("  %4u  %5d  %3d  %9d.%1d  %10d\n",
                        r.seed, r.empire_polities_alive, r.empire_polities_rim,
                        r.empire_nodes_mean_q / 1000, (r.empire_nodes_mean_q / 100) % 10,
                        r.empire_nodes_max);
            if (r.empire_polities_rim > 0) ++worlds_with_rim;
            if (r.empire_polities_alive > 0 && r.empire_polities_rim == r.empire_polities_alive)
                ++worlds_all_rim;
            total_alive += r.empire_polities_alive;
            total_rim   += r.empire_polities_rim;
        }
        std::printf("  worlds with >=1 realm holding the rim: %d/%d\n",
                    worlds_with_rim, static_cast<int>(rows.size()));
        std::printf("  worlds where EVERY living realm holds it: %d/%d\n",
                    worlds_all_rim, static_cast<int>(rows.size()));
        std::printf("  rim share across all living polities: %lld/%lld\n",
                    static_cast<long long>(total_rim), static_cast<long long>(total_alive));
        // REPORTED, not gated on "at least one realm reaches it" -- the
        // default `--epoch 0` fixture runs generation's own SHORT ancient
        // span (400 BCE -> 0 CE, 100 rounds; era_minus_one.hpp), not the
        // 1200 CE / 1,600-year Empire span BL-906 names. Climbing all four
        // rings of a ~48-node tree in 400 years is a real reach; a world
        // that falls short here is a span-length finding (BL-906's own,
        // still pending in this base), not a defect in the scorer or the
        // mask. `total_rim < total_alive` is asserted where it CAN discriminate.
        if (total_alive > 0)
            check(total_rim < total_alive,
                  "BL912b  the rim DISCRIMINATES -- not every living polity holds it "
                  "(when any do; see the share line above for whether any did)");
    }

    // --- The distributions -------------------------------------------------
    std::printf("\n--- distributions over %d worlds ---\n", static_cast<int>(rows.size()));
    if (!rows.empty())
    {
        std::vector<int64_t> powers, tops, tops_pop, peaks_pop, battles, conq, ends;
        int hegemonies = 0, hegemonies_pop = 0, eliminations = 0, unrecorded = 0;
        for (const sweep_row& r : rows)
        {
            powers.push_back(r.powers_end);
            tops.push_back(r.top_share_q);
            tops_pop.push_back(r.top_share_pop_q);
            peaks_pop.push_back(r.peak_share_pop_q);
            battles.push_back(r.battles);
            conq.push_back(r.conquests);
            ends.push_back(r.regions_end);
            if (r.hegemony_year >= 0) ++hegemonies;
            if (r.hegemony_year_pop >= 0) ++hegemonies_pop;
            if (r.powers_end < r.powers_start) ++eliminations;
            if (!r.pop_recorded) ++unrecorded;
        }
        auto span = [](std::vector<int64_t> v) {
            std::sort(v.begin(), v.end());
            return std::pair<int64_t, int64_t>{v.front(), v.back()};
        };
        const auto ps = span(powers), ts = span(tops), bs = span(battles);
        const auto tps = span(tops_pop), pps = span(peaks_pop);

        std::printf("  powers at epoch      median %lld   range %lld..%lld\n",
                    static_cast<long long>(median_of(powers)),
                    static_cast<long long>(ps.first), static_cast<long long>(ps.second));
        std::printf("  largest share        median %lld%%   range %lld%%..%lld%%   (of REGIONS held)\n",
                    static_cast<long long>(median_of(tops) / 10),
                    static_cast<long long>(ts.first / 10), static_cast<long long>(ts.second / 10));
        std::printf("  largest share (pop)  median %lld%%   range %lld%%..%lld%%   (of POPULATION held -- BL-970)\n",
                    static_cast<long long>(median_of(tops_pop) / 10),
                    static_cast<long long>(tps.first / 10), static_cast<long long>(tps.second / 10));
        std::printf("  peak share (pop)     median %lld%%   range %lld%%..%lld%%   (highest at any century)\n",
                    static_cast<long long>(median_of(peaks_pop) / 10),
                    static_cast<long long>(pps.first / 10), static_cast<long long>(pps.second / 10));
        if (unrecorded > 0)
            std::printf("  (BL-970: %d world(s) carried NO playback record; their population column reads 0)\n",
                        unrecorded);
        std::printf("  battles per world    median %lld   range %lld..%lld\n",
                    static_cast<long long>(median_of(battles)),
                    static_cast<long long>(bs.first), static_cast<long long>(bs.second));
        std::printf("  conquests per world  median %lld\n",
                    static_cast<long long>(median_of(conq)));

        // --- BL-778 / BL-779  THE WATER MODEL, all seeds ------------------
        //
        // Reported, never asserted upward. `illegal` is the free overseas
        // reach BL-755 measured, now refused; `starved` is the forage
        // simplification biting; `naval battles` is how often the rare case
        // the water model exists for actually happens.
        {
            long long ill = 0, starved = 0, nav = 0, sea = 0, batt = 0;
            for (const sweep_row& r : rows)
            {
                ill     += r.illegal_campaigns;
                starved += r.starved_campaigns;
                nav     += r.naval_battles;
                sea     += r.sea_leg_battles;
                batt    += r.battles;
            }
            std::printf("\n--- BL-778 / BL-779  THE WATER MODEL (all seeds) ---\n");
            std::printf("  campaigns REFUSED on traversal legality   %lld\n", ill);
            std::printf("  campaigns fought STARVING (no forage)     %lld\n", starved);
            std::printf("  battles with a naval CONTINGENT present   %lld of %lld (%lld%%)\n",
                        nav, batt, batt ? nav * 100 / batt : 0);
            std::printf("  battles REACHED OVER A SEA LEG            %lld of %lld (%lld%%)  <- naval combat\n",
                        sea, batt, batt ? sea * 100 / batt : 0);
            std::printf("  (THE SECOND LINE IS THE ONE THAT ANSWERS \"how often does naval combat\n"
                        "   occur\". The first is high because `roster_stack` composes EVERY\n"
                        "   available row into a stack, so any polity clearing port_q carries a\n"
                        "   galley contingent into inland fights too — it measures how COASTAL\n"
                        "   the powers are. Rare is the DESIGN, not a shortfall\n"
                        "   (MILITARY_HISTORY.md § Naval).\n"
                        "   A non-zero refusal count is BL-778 working: those campaigns used to\n"
                        "   cross open ocean for free.)\n");
        }
        std::printf("  regions at epoch   median %lld\n",
                    static_cast<long long>(median_of(ends)));
        // BL-757 R4: the band the roster actually reached, summed over the
        // sweep. All zeroes means build_work never won a round — which is the
        // measured state as of 2026-09-03 and the thing BL-767 and BL-768 both
        // need to see move.
        {
            int64_t band_tot[roster_band_count] = {0, 0, 0, 0};
            for (const sweep_row& r : rows)
                for (int bi = 0; bi < roster_band_count; ++bi) band_tot[bi] += r.works_by_band[bi];
            std::printf("  works by band        classical %lld  medieval %lld  gunpowder %lld"
                        "  industrial %lld\n",
                        static_cast<long long>(band_tot[0]), static_cast<long long>(band_tot[1]),
                        static_cast<long long>(band_tot[2]), static_cast<long long>(band_tot[3]));
        }
        std::vector<int64_t> lasts;
        for (const sweep_row& r : rows) lasts.push_back(r.last_change_year);
        int64_t early_sum = 0;
        for (const sweep_row& r : rows) early_sum += r.early_change_pct;
        std::printf("\n  LAST CHANGE YEAR     median %lld   (does history recur, or stop?)\n",
                    static_cast<long long>(median_of(lasts)));
        std::printf("  CHANGES IN 1st 10%%   %lld%% on average\n",
                    static_cast<long long>(early_sum / static_cast<int64_t>(rows.size())));
        // --- BL-889: WHY A CAMPAIGN DID NOT HAPPEN, BY REASON --------------
        //
        // The sweep used to report ONE refusal reason (traversal legality) and
        // it dominates: ~50,000 per world against ~61 battles fought. A single
        // named reason at that scale is not a diagnosis -- it cannot say
        // whether the others are also firing, nor where in the funnel the
        // candidates actually die. Every counter below already existed on
        // history_sim_state; only the reporting was missing.
        {
            std::vector<int64_t> con, sco, cle, cho, ill, rch;
            for (const sweep_row& r : rows)
            {
                con.push_back(r.campaign_contacts); sco.push_back(r.campaign_scored);
                cle.push_back(r.campaign_cleared);  cho.push_back(r.campaign_chosen);
                ill.push_back(r.illegal_campaigns); rch.push_back(r.reach_denied);
            }
            {
            std::vector<int64_t> mt, mp, mu, mr, mc, hu, rr;
            for (const sweep_row& r : rows) {
                mt.push_back(r.mat_trade);   mp.push_back(r.mat_total);
                mu.push_back(r.mat_upkeep);  mr.push_back(r.mat_roads);
                mc.push_back(r.mat_campaigns);
                hu.push_back(r.heads_unpaid); rr.push_back(r.roads_refused);
            }
            std::printf("\n--- BL-895  DID THE NETWORK ACTUALLY PAY? ---\n");
            std::printf("  materials from TRADE   median %lld per world\n", static_cast<long long>(median_of(mt)));
            std::printf("  materials produced     median %lld\n", static_cast<long long>(median_of(mp)));
            {
                const int64_t mmt = median_of(mt), mmp = median_of(mp);
                std::printf("  trade's share of production  %lld.%02lld%%\n",
                            static_cast<long long>(mmp > 0 ? (mmt * 100) / mmp : 0),
                            static_cast<long long>(mmp > 0 ? ((mmt * 10000) / mmp) % 100 : 0));
            }
            std::printf("  (A trade figure of ZERO means the mechanism never fired -- a wiring\n"
                        "   question, not a balance one. REPORTED, not gated.)\n");

            // BL-925 -- AMICABLE CROSS-BORDER LINKS, PER WORLD. Read only: no
            // target is set here, the design's DONE WHEN asks the number be
            // visible, not that it clear a bar.
            {
                std::vector<int64_t> cbl;
                for (const sweep_row& r : rows) cbl.push_back(r.cross_border_links_open);
                std::printf("\n--- BL-925  AMICABLE CROSS-BORDER TRADE ---\n");
                std::printf("  cross-border links open at run's end, median %lld, per world:",
                            static_cast<long long>(median_of(cbl)));
                for (int64_t v : cbl) std::printf(" %lld", static_cast<long long>(v));
                std::printf("\n");
            }

            // THE SINKS (BL-895, Ben's ruling 2026-09-11). The income half of
            // this item could not gate anything while campaigns were the only
            // thing materials were ever spent on, so what matters here is the
            // SHARE of production the three sinks between them claim -- never
            // any one of them read on its own.
            {
                const int64_t up = median_of(mu), rd = median_of(mr), cp = median_of(mc);
                const int64_t prod = median_of(mp);
                std::printf("\n--- BL-895  WHAT DID THE MATERIALS GO ON? ---\n");
                std::printf("  spent on UPKEEP        median %lld per world\n", static_cast<long long>(up));
                std::printf("  spent on ROADS         median %lld\n", static_cast<long long>(rd));
                std::printf("  spent on CAMPAIGNS     median %lld\n", static_cast<long long>(cp));
                std::printf("  all sinks / produced   %lld%% of %lld\n",
                            static_cast<long long>(prod > 0 ? ((up + rd + cp) * 100) / prod : 0),
                            static_cast<long long>(prod));
                std::printf("  heads sent home UNPAID median %lld   (the strangling channel)\n",
                            static_cast<long long>(median_of(hu)));
                std::printf("  road builds REFUSED    median %lld   (poverty delays a road)\n",
                            static_cast<long long>(median_of(rr)));
            }

            // BL-929 -- DID A REALM SPEND THE STOCKPILE ON REACH ITSELF?
            // The verb's own "done when": upgrades and materials spent on
            // them, reported beside the three sinks above (upkeep, roads,
            // campaigns) so this is read as a FOURTH claim on production
            // rather than in isolation. REPORTS, never gates -- zero here is
            // a legitimate reading (no held region ever fell to the floor)
            // and not by itself a defect.
            {
                std::vector<int64_t> su, sur, suc, ms;
                for (const sweep_row& r : rows)
                {
                    su.push_back(r.supply_sites_upgraded);
                    sur.push_back(r.supply_sites_upgraded_regions);
                    suc.push_back(r.supply_sites_upgraded_corridors);
                    ms.push_back(r.mat_supply_sites);
                }
                const int64_t prod = median_of(mp);
                std::printf("\n--- BL-929  DID A REALM BUY REACH FROM THE STOCKPILE? ---\n");
                std::printf("  supply sites UPGRADED  median %lld per world  (region %lld / corridor %lld)\n",
                            static_cast<long long>(median_of(su)),
                            static_cast<long long>(median_of(sur)),
                            static_cast<long long>(median_of(suc)));
                std::printf("  materials spent on them median %lld\n", static_cast<long long>(median_of(ms)));
                std::printf("  as a share of produced  %lld%% of %lld\n",
                            static_cast<long long>(prod > 0 ? (median_of(ms) * 100) / prod : 0),
                            static_cast<long long>(prod));

                // TRADE INCOME vs UPGRADE COUNT, ACROSS WORLDS -- the closest
                // this sweep can come to the item's "realms with higher trade
                // income upgrade more sites" before BL-925 lands trade across
                // borders and a PER-POLITY figure exists to correlate against
                // its OWN upgrade count. This correlates one WORLD's total
                // trade yield against that same world's total upgrade count,
                // which is a coarser claim -- a world is not a realm -- and is
                // reported as such rather than dressed up as the per-realm
                // reading the design actually asks for.
                if (rows.size() >= 4)
                {
                    std::vector<std::pair<int64_t, int64_t>> by_trade; // (mat_trade, upgrades)
                    for (const sweep_row& r : rows)
                        by_trade.push_back({r.mat_trade, r.supply_sites_upgraded});
                    std::sort(by_trade.begin(), by_trade.end());
                    const std::size_t half = by_trade.size() / 2;
                    std::vector<int64_t> lo, hi;
                    for (std::size_t i = 0; i < by_trade.size(); ++i)
                        (i < half ? lo : hi).push_back(by_trade[i].second);
                    std::printf("  upgrades, LOW-trade half of worlds   median %lld\n",
                                static_cast<long long>(median_of(lo)));
                    std::printf("  upgrades, HIGH-trade half of worlds  median %lld\n",
                                static_cast<long long>(median_of(hi)));
                    std::printf("  (WORLD-level correlation only -- a world is not a realm; the true\n"
                                "   per-realm reading wants BL-925's trade-across-borders figure.)\n");
                }
            }

            // BL-899 -- DID SEA LEGS FEED ANY CROSSING AT ALL?
            // The question BL-893 could not answer about itself: it opened
            // LEGALITY and changed no outcome. FED at zero with STARVED above
            // zero means the mechanism is wired but nobody clears the creed
            // floor or holds the port; both at zero means no crossing was ever
            // launched, which is a geography answer and not a balance one.
            // REPORTS, never gates.
            {
                std::vector<int64_t> sl, st;
                for (const sweep_row& r : rows)
                { sl.push_back(r.sea_legs_fed); st.push_back(r.starved_campaigns); }
                int64_t sl_tot = 0, st_tot = 0;
                for (const sweep_row& r : rows)
                { sl_tot += r.sea_legs_fed; st_tot += r.starved_campaigns; }
                std::printf("\n--- BL-899  DID SEA LEGS FEED ANY CROSSING? ---\n");
                std::printf("  crossings FED on ration median %lld per world  (total %lld)\n",
                            static_cast<long long>(median_of(sl)),
                            static_cast<long long>(sl_tot));
                std::printf("  crossings that STARVED   median %lld            (total %lld)\n",
                            static_cast<long long>(median_of(st)),
                            static_cast<long long>(st_tot));
                std::printf("  fed share of crossings   %lld%%\n",
                            static_cast<long long>((sl_tot + st_tot) > 0
                                                   ? (sl_tot * 100) / (sl_tot + st_tot) : 0));
                std::printf("  (SOME peoples cross fed and MOST do not is the design. A 100%%\n"
                            "   share means the floor is too low. REPORTED, not gated.)\n");
            }

            // BL-896 -- DID EMPIRES FRAGMENT, AND DID THE PIECES DIFFER?
            // Ben's ruling: collapse is NETWORK FAILURE. Both numbers zero is
            // a world whose networks always held -- a legitimate outcome and
            // not a broken mechanism -- so this REPORTS and does not gate.
            {
                std::vector<int64_t> sc, rs;
                for (const sweep_row& r : rows)
                { sc.push_back(r.secessions); rs.push_back(r.regions_seceded); }
                std::printf("\n--- BL-896  DID THE NETWORK FAILURE FRAGMENT ANYONE? ---\n");
                std::printf("  SECESSIONS             median %lld per world\n",
                            static_cast<long long>(median_of(sc)));
                std::printf("  regions that walked    median %lld\n",
                            static_cast<long long>(median_of(rs)));
                {
                    std::vector<int64_t> gc;
                    for (const sweep_row& r : rows) gc.push_back(r.seceded_graph_cut);
                    std::printf("  ...with a FED same-realm region within neighbour_radius  median %lld   (BL-922: a fed neighbour stood within radius -- the residual the uncapped supply index leaves)\n",
                                static_cast<long long>(median_of(gc)));
                }
                std::printf("  (ZERO of both is a world whose realms never outran their\n"
                            "   own reach. REPORTED, not gated.)\n");
            }

            // BL-922 -- IS THERE A GRADIENT? The supply currency the three
            // floors and the gate are priced in, over every held region. The
            // distance bands are the item's own DONE WHEN: a connected region
            // 30+ tiles from its capital must read materially below one
            // beside it. REPORTED, not gated: the magnitude is measured here,
            // never chosen to hit a target.
            {
                std::printf("\n--- BL-922  SUPPLY OVER HELD GROUND (network_supply_q, 0-1000; floors are the run's own) ---\n");
                std::printf("  seed   held   at 0   <=settle  <=secede  <=campaign   p10  p50  p90   | median by tiles from capital: <10 (n)   10-19 (n)   20-29 (n)   30+ (n)   | reach ms / rebuilds / decisions ms\n");
                int64_t held_t = 0, zero_t = 0, settle_t = 0, secede_t = 0, camp_t = 0;
                for (const sweep_row& r : rows)
                {
                    held_t += r.held_regions; zero_t += r.supply_zero;
                    settle_t += r.supply_le_settle; secede_t += r.supply_le_secede;
                    camp_t += r.supply_le_campaign;
                    std::printf("  %4d  %5lld  %5lld  %8lld  %8lld  %10lld   %4lld %4lld %4lld   | %4lld (%4lld)  %4lld (%4lld)  %4lld (%4lld)  %4lld (%4lld)   | %lld / %lld / %lld\n",
                                r.seed,
                                static_cast<long long>(r.held_regions),
                                static_cast<long long>(r.supply_zero),
                                static_cast<long long>(r.supply_le_settle),
                                static_cast<long long>(r.supply_le_secede),
                                static_cast<long long>(r.supply_le_campaign),
                                static_cast<long long>(r.supply_p10),
                                static_cast<long long>(r.supply_p50),
                                static_cast<long long>(r.supply_p90),
                                static_cast<long long>(r.supply_band_med[0]), static_cast<long long>(r.supply_band_n[0]),
                                static_cast<long long>(r.supply_band_med[1]), static_cast<long long>(r.supply_band_n[1]),
                                static_cast<long long>(r.supply_band_med[2]), static_cast<long long>(r.supply_band_n[2]),
                                static_cast<long long>(r.supply_band_med[3]), static_cast<long long>(r.supply_band_n[3]),
                                static_cast<long long>(r.era_reach_ms),
                                static_cast<long long>(r.era_reach_rebuilds),
                                static_cast<long long>(r.era_decisions_ms));
                }
                if (held_t > 0)
                    std::printf("  SHARE of held ground   at 0: %lld%%   under settlement floor: %lld%%   under secession floor: %lld%%   under campaign floor: %lld%%\n",
                                static_cast<long long>(zero_t * 100 / held_t),
                                static_cast<long long>(settle_t * 100 / held_t),
                                static_cast<long long>(secede_t * 100 / held_t),
                                static_cast<long long>(camp_t * 100 / held_t));
                {
                    int64_t kin = 0, nolink = 0;
                    for (const sweep_row& r : rows) { kin += r.zero_kin_near; nolink += r.zero_no_link; }
                    std::printf("  of the %lld at 0:  %lld have a FED same-realm region within neighbour_radius (the graph cut them off), %lld have NO same-realm region that close (the map did)\n",
                                static_cast<long long>(zero_t), static_cast<long long>(kin), static_cast<long long>(nolink));
                }
                std::printf("  (Read the distance bands against each other. A 30+ band that\n"
                            "   reads level with the <10 band is a currency with no gradient,\n"
                            "   and a floor sited in it cannot bite on connected ground.\n"
                            "   REPORTED, not gated.)\n");
            }

            // BL-897 -- DID A CREED THAT SPANS CULTURES ARISE, AND DID IT HOLD?
            // Ben's ruling: it arises from HUMILIATION and from DENSITY, and
            // needs both. So a world of intact, isolated realms producing none
            // is the design speaking, not a broken mechanism -- CREEDS.md calls
            // that "a legitimate world rather than a failed one". REPORTS, and
            // does not gate.
            //
            // THE LAST TWO LINES ARE THE FAULT LINE, and they are why four
            // counters are printed rather than one. Adoptions are the
            // INSTITUTION; converted and reasserted are the PEOPLES. The two
            // grains are expected to disagree, and the size of the disagreement
            // is what a later schism slice would cut along.
            {
                std::vector<int64_t> ca, cd, cv, cr;
                int worlds_with_creed = 0;
                for (const sweep_row& r : rows)
                {
                    ca.push_back(r.creeds_arisen);   cd.push_back(r.creed_adoptions);
                    cv.push_back(r.creed_converted); cr.push_back(r.creed_reasserted);
                    if (r.creeds_arisen > 0) ++worlds_with_creed;
                }
                std::printf("\n--- BL-897  DID A CREED THAT SPANS CULTURES ARISE? ---\n");
                std::printf("  worlds with a creed    %d of %d\n",
                            worlds_with_creed, static_cast<int>(rows.size()));
                // TOTALS BESIDE THE MEDIANS, and the pair is the point. A
                // mechanism designed to fire in SOME worlds and not others has
                // a median of zero the moment it fires in fewer than half of
                // them -- which is the intended shape, not a null result. The
                // sweep total is the only line that can tell "rare" from
                // "never", so both are printed on every row.
                const auto tot = [](const std::vector<int64_t>& v) {
                    int64_t t = 0; for (int64_t x : v) t += x; return t; };
                std::printf("  creeds COINED          median %lld per world, %lld across the sweep\n",
                            static_cast<long long>(median_of(ca)), static_cast<long long>(tot(ca)));
                std::printf("  realms that ADOPTED    median %lld, %lld total   (the institution)\n",
                            static_cast<long long>(median_of(cd)), static_cast<long long>(tot(cd)));
                std::printf("  peoples CONVERTED      median %lld, %lld total   (pantheon -> residue)\n",
                            static_cast<long long>(median_of(cv)), static_cast<long long>(tot(cv)));
                std::printf("  peoples REASSERTED     median %lld, %lld total   (the pantheon won)\n",
                            static_cast<long long>(median_of(cr)), static_cast<long long>(tot(cr)));
                std::printf("  (ALL sixteen or NONE is the reading to distrust. A creed that\n"
                            "   arises everywhere is not a consequence of humiliation, and one\n"
                            "   that arises nowhere is a gate nothing clears. REPORTED, not gated.)\n");
            }

            // BL-944 -- DID FAITH FRACTURE A REALM, AND IS IT DISTINCT FROM
            // A NETWORK-FAILURE SECESSION? Printed right beside BL-896's own
            // reading so the two causes can be checked against each other by
            // eye: a schism total that tracks `secessions` one-for-one would
            // say the two mechanisms are firing on the same ground, which
            // they must not -- one is grouped by SEAT/REACH, the other by
            // RESIDUE CULTURE, and the pair below is the reasserted supply
            // this fracture is cut from. REPORTED, never gated: a world with
            // reassertion but no schism (the group never reached
            // `schism_min_regions`) is a legitimate world, per CREEDS.md's
            // own "reading is a requirement, not a target" discipline.
            {
                std::vector<int64_t> sc, rs;
                int worlds_with_schism = 0;
                for (const sweep_row& r : rows)
                {
                    sc.push_back(r.schisms); rs.push_back(r.regions_sundered);
                    if (r.schisms > 0) ++worlds_with_schism;
                }
                const auto tot = [](const std::vector<int64_t>& v) {
                    int64_t t = 0; for (int64_t x : v) t += x; return t; };
                std::printf("\n--- BL-944  DID FAITH FRACTURE A REALM (a SCHISM, not a network failure)? ---\n");
                std::printf("  worlds with a schism   %d of %d\n",
                            worlds_with_schism, static_cast<int>(rows.size()));
                std::printf("  SCHISMS                median %lld, %lld total   (a reasserted people breaking away over creed)\n",
                            static_cast<long long>(median_of(sc)), static_cast<long long>(tot(sc)));
                std::printf("  regions SUNDERED       median %lld, %lld total   (walked away with them -- distinct from regions_seceded)\n",
                            static_cast<long long>(median_of(rs)), static_cast<long long>(tot(rs)));
                std::printf("  (Read this against BL-896's SECESSIONS line above. The two should\n"
                            "   NOT move together -- a schism groups a realm's REASSERTED ground by\n"
                            "   its residue CULTURE, never by seat or reach, so a world can show\n"
                            "   either, both or neither without the two ever being one cause read\n"
                            "   twice. REPORTED, not gated.)\n");
            }

            // BL-838 -- DID FEAR OF BEING NEXT MOVE ANY CAMPAIGN?
            // REPORTED, NOT GATED, and the two numbers must be read together.
            // A large lean count against ONE polity is the coalition this item
            // is about; the same count spread across dozens is diffuse
            // resentment and means something else. Zero of both is a legitimate
            // world -- one in which nobody accumulated a reputation among
            // anybody's kin -- and never a reason to tune the weight up.
            {
                std::vector<int64_t> fl, ft;
                for (const sweep_row& r : rows)
                { fl.push_back(r.fear_leaned); ft.push_back(r.fear_targets); }
                std::printf("\n--- BL-838  DID FEAR OF BEING NEXT MOVE ANY CAMPAIGN? ---\n");
                std::printf("  candidates LEANED      median %lld per world\n",
                            static_cast<long long>(median_of(fl)));
                std::printf("  DISTINCT feared realms median %lld\n",
                            static_cast<long long>(median_of(ft)));
                // A MEDIAN ALONE HIDES THIS ONE. Fear is rare and clustered:
                // the first reading had a median of 0 in both arms while the
                // conquest totals plainly differed between them, which is a
                // median reporting the quiet half of the sweep rather than the
                // mechanism being inert. The count and the peak say which.
                {
                    int64_t worlds = 0, peak = 0, peak_t = 0;
                    for (const sweep_row& r : rows)
                    {
                        if (r.fear_leaned > 0) ++worlds;
                        if (r.fear_leaned > peak)   peak   = r.fear_leaned;
                        if (r.fear_targets > peak_t) peak_t = r.fear_targets;
                    }
                    std::printf("  worlds with ANY lean    %lld of %lld\n",
                                static_cast<long long>(worlds),
                                static_cast<long long>(rows.size()));
                    std::printf("  WORST world             %lld leans against %lld realms\n",
                                static_cast<long long>(peak),
                                static_cast<long long>(peak_t));
                }
                std::printf("  (The trigger is what a polity DID, read from the grudge ledger\n"
                            "   held against it by the decider's OWN people -- never its size.\n"
                            "   REPORTED, not gated.)\n");
            }
        }

        std::printf("\n--- BL-889  WHY A CAMPAIGN DID NOT HAPPEN, BY REASON ---\n");
            std::printf("  contacts examined      median %lld per world\n", static_cast<long long>(median_of(con)));
            std::printf("    REFUSED traversal    median %lld   (BL-778 water gate)\n", static_cast<long long>(median_of(ill)));
            std::printf("    REFUSED reach gate   median %lld   (BL-837)\n", static_cast<long long>(median_of(rch)));
            std::printf("  reached scoring        median %lld\n", static_cast<long long>(median_of(sco)));
            std::printf("  cleared the threshold  median %lld\n", static_cast<long long>(median_of(cle)));
            std::printf("  Campaign won the verb  median %lld rounds\n", static_cast<long long>(median_of(cho)));
            std::vector<int64_t> crd, cls;
            for (const sweep_row& r : rows) { crd.push_back(r.cleared_rounds); cls.push_back(r.cleared_lost); }
            std::printf("  --- ROUND grain (the two above are CANDIDATE grain; do not ratio them) ---\n");
            std::printf("  Campaign in the running median %lld rounds\n", static_cast<long long>(median_of(crd)));
    std::printf("    ...WON the argmax     median %lld\n", static_cast<long long>(median_of(cho)));
            std::printf("    ...LOST the argmax    median %lld\n", static_cast<long long>(median_of(cls)));
            std::printf("  (Read top down. Candidates dying at TRAVERSAL or REACH are a\n"
                        "   geography/adjacency problem; dying between SCORED and CLEARED is a\n"
                        "   threshold problem; cleared-but-never-chosen is verb competition.\n"
                        "   REPORTED, not gated.)\n");
        }

        // --- BL-889: churn or accumulation --------------------------------
        //
        // battles and conquests count EVENTS. Neither can tell "many regions
        // taken once and HELD" from "a few regions changing hands over and
        // over" -- opposite worlds, of which only the first builds an empire.
        // The arc GENERATION_STRATEGY.md sec The asymmetry is POLITICAL asks
        // for is invisible to every column that existed before this block.
        {
            std::vector<int64_t> touched, once_pct, thrice_pct, per_reg, maxf;
            for (const sweep_row& r : rows)
            {
                const int64_t t2 = r.regions_touched > 0 ? r.regions_touched : 1;
                touched.push_back(r.regions_touched);
                once_pct.push_back(static_cast<int64_t>(r.regions_once) * 100 / t2);
                thrice_pct.push_back(static_cast<int64_t>(r.regions_thrice) * 100 / t2);
                per_reg.push_back(r.flips_per_region);
                maxf.push_back(r.max_flips);
            }
            const int64_t pr = median_of(per_reg);
            std::printf("\n--- BL-889  DOES CONQUEST ACCUMULATE, OR DOES THE MAP CHURN? ---\n");
            std::printf("  regions ever CONQUERED median %lld per world (foundings excluded)\n", static_cast<long long>(median_of(touched)));
            std::printf("  TAKEN ONCE AND KEPT    median %lld%% of them\n", static_cast<long long>(median_of(once_pct)));
            std::printf("  TAKEN 3+ TIMES         median %lld%% of them\n", static_cast<long long>(median_of(thrice_pct)));
            std::printf("  changes per region     median %lld.%02lld\n",
                        static_cast<long long>(pr / 100), static_cast<long long>(pr % 100));
            std::printf("  most-fought-over one   median %lld changes   worst %lld\n",
                        static_cast<long long>(median_of(maxf)), static_cast<long long>(span(maxf).second));
            std::printf("  (HIGH kept + LOW 3+ = conquest accumulates and an empire can form.\n"
                        "   LOW kept + HIGH 3+ = the same ground trading hands, which logs\n"
                        "   conquests without ever moving the political map. REPORTED, not gated.)\n");
        }

        std::printf("\n  HEGEMONY RATE        %d / %d worlds reached %d%% single-power share of REGIONS\n",
                    hegemonies, static_cast<int>(rows.size()), hegemony_threshold_q / 10);
        std::printf("  HEGEMONY RATE (pop)  %d / %d worlds reached %d%% single-power share of POPULATION\n",
                    hegemonies_pop, static_cast<int>(rows.size()), hegemony_threshold_q / 10);
        std::vector<int64_t> smalls, smalls_pop;
        int eliminations_gross = 0;
        for (const sweep_row& r : rows)
        {
            smalls.push_back(r.smallest_holding);
            smalls_pop.push_back(r.smallest_holding_pop);
            if (r.polities_dead_gross > 0) ++eliminations_gross;
        }
        const auto ss_ = span(smalls), sp_ = span(smalls_pop);
        std::printf("  ELIMINATION RATE     %d / %d worlds lost even one power (net)   %d / %d (gross: any polity that ever held ground died)\n",
                    eliminations, static_cast<int>(rows.size()),
                    eliminations_gross, static_cast<int>(rows.size()));
        std::printf("  WEAKEST POWER holds  median %lld regions   range %lld..%lld\n",
                    static_cast<long long>(median_of(smalls)),
                    static_cast<long long>(ss_.first), static_cast<long long>(ss_.second));
        std::printf("  WEAKEST POWER (pop)  median %lld heads     range %lld..%lld\n",
                    static_cast<long long>(median_of(smalls_pop)),
                    static_cast<long long>(sp_.first), static_cast<long long>(sp_.second));
        std::printf("\n  (Both rates are REPORTED, not asserted — BL-224's non-hegemony becomes a\n"
                    "   tuning target read off this spread, not a construction guarantee.)\n");

        // --- BL-748 R1: the furnace distribution --------------------------
        //
        // Since BL-748 the furnace date is an EVENT INSIDE THE RUN — the year a
        // polity's materials capacity crossed the Industrial rung, plus the lag
        // its ground imposes — rather than a date `run_settlement` fixed before
        // the loop started. What R1 asks of this table is that the spread be
        // WIDE: some worlds late, some never, rather than everything clustered
        // at the span boundary because the rung unlocks there.
        std::printf("\n--- BL-748  THE FURNACE, per world ---\n");
        std::printf("  seed   polities  crossed  first cross   regions lit   first / median / last"
                    "   mat cap (max/med)\n");
        for (const sweep_row& r : rows)
        {
            char cross[24], lit[48];
            if (r.polities_crossed == 0) std::snprintf(cross, sizeof cross, "%11s", "never");
            else std::snprintf(cross, sizeof cross, "%11lld",
                               static_cast<long long>(r.first_cross_year));
            if (r.regions_industrial == 0) std::snprintf(lit, sizeof lit, "%22s", "-");
            else std::snprintf(lit, sizeof lit, "%6lld /%6lld /%6lld",
                               static_cast<long long>(r.industrial_first),
                               static_cast<long long>(r.industrial_median),
                               static_cast<long long>(r.industrial_last));
            std::printf("  %4u   %8d  %7d  %s   %11d   %s   %3d / %3d\n",
                        r.seed, r.polities_total, r.polities_crossed, cross,
                        r.regions_industrial, lit, r.mat_cap_max, r.mat_cap_median);
        }
        {
            int worlds_any = 0, worlds_none = 0;
            std::vector<int64_t> firsts, crossed_n, lit_n, capmax;
            for (const sweep_row& r : rows)
            {
                if (r.regions_industrial > 0) { ++worlds_any; firsts.push_back(r.industrial_first); }
                else ++worlds_none;
                crossed_n.push_back(r.polities_crossed);
                lit_n.push_back(r.regions_industrial);
                capmax.push_back(r.mat_cap_max);
            }
            std::printf("\n  WORLDS THAT INDUSTRIALISED   %d / %d   (never: %d)\n",
                        worlds_any, static_cast<int>(rows.size()), worlds_none);
            std::printf("  polities crossing the rung   median %lld\n",
                        static_cast<long long>(median_of(crossed_n)));
            std::printf("  regions lit per world        median %lld\n",
                        static_cast<long long>(median_of(lit_n)));
            std::printf("  highest materials capacity   median %lld   "
                        "(the Industrial rung is 5)\n",
                        static_cast<long long>(median_of(capmax)));
            if (!firsts.empty())
            {
                const auto fs = span(firsts);
                std::printf("  FIRST FURNACE YEAR           median %lld   range %lld..%lld"
                            "   (boundary %lld, epoch %lld)\n",
                            static_cast<long long>(median_of(firsts)),
                            static_cast<long long>(fs.first), static_cast<long long>(fs.second),
                            static_cast<long long>(params.boundary_year == INT64_MIN
                                                   ? params.start_year : params.boundary_year),
                            static_cast<long long>(params.stop_year));
            }
            else
            {
                std::printf("  FIRST FURNACE YEAR           NONE — no polity reached the rung.\n"
                            "                               Read the materials-capacity column:"
                            " a world topping out below 5\n"
                            "                               is a LADDER finding, not a furnace"
                            " one.\n");
            }
            std::printf("  (A world with no furnace is a legitimate outcome and is reported, never\n"
                        "   filled in. R1 asks that the spread be WIDE, not that it be complete.)\n");
        }

        // --- BL-767 R2: the rise-peak-fall shape --------------------------
        //
        // REPORTED, NEVER GUARANTEED. Tuning moves FORCES — the weights in
        // `history_sim_params` — and this table is how the movement is read.
        // A world with no empire stays legitimate (BL-224's non-hegemony
        // invariant, the 2026-07-30 emergent-nation-count ruling), so nothing
        // below is asserted and no term anywhere forces the shape.
        std::printf("\n--- BL-767  RISE / PEAK / FALL, per world  (BL-970: regions held | population held) ---\n");
        std::printf("  seed   polities   rose   fell   rose+fell   biggest peak share  |  rose   fell   rose+fell   biggest peak share (pop)\n");
        for (const sweep_row& r : rows)
            std::printf("  %4u   %8d   %4d   %4d   %9d   %16d%%  |  %4d   %4d   %9d   %22d%%\n",
                        r.seed, r.polities_total, r.shape_rose, r.shape_fell,
                        r.shape_rpf, r.shape_top_peak_q / 10,
                        r.shape_rose_pop, r.shape_fell_pop, r.shape_rpf_pop,
                        r.shape_top_peak_pop_q / 10);
        {
            int worlds_with_shape = 0, worlds_with_shape_pop = 0;
            std::vector<int64_t> rpf, rose, peaks, rpf_p, rose_p, peaks_p;
            for (const sweep_row& r : rows)
            {
                if (r.shape_rpf > 0) ++worlds_with_shape;
                if (r.shape_rpf_pop > 0) ++worlds_with_shape_pop;
                rpf.push_back(r.shape_rpf);
                rose.push_back(r.shape_rose);
                peaks.push_back(r.shape_top_peak_q);
                rpf_p.push_back(r.shape_rpf_pop);
                rose_p.push_back(r.shape_rose_pop);
                peaks_p.push_back(r.shape_top_peak_pop_q);
            }
            const auto pk = span(peaks), pkp = span(peaks_p);
            std::printf("\n                               regions held          population held\n");
            std::printf("  WORLDS SHOWING THE SHAPE     %d / %d                %d / %d   "
                        "(at least one polity rose, peaked and fell)\n",
                        worlds_with_shape, static_cast<int>(rows.size()),
                        worlds_with_shape_pop, static_cast<int>(rows.size()));
            std::printf("  polities that ROSE           median %lld per world   median %lld per world\n",
                        static_cast<long long>(median_of(rose)),
                        static_cast<long long>(median_of(rose_p)));
            std::printf("  polities that ROSE AND FELL  median %lld per world   median %lld per world\n",
                        static_cast<long long>(median_of(rpf)),
                        static_cast<long long>(median_of(rpf_p)));
            std::printf("  BIGGEST PEAK SHARE reached   median %lld%% (%lld%%..%lld%%)   median %lld%% (%lld%%..%lld%%)\n",
                        static_cast<long long>(median_of(peaks) / 10),
                        static_cast<long long>(pk.first / 10),
                        static_cast<long long>(pk.second / 10),
                        static_cast<long long>(median_of(peaks_p) / 10),
                        static_cast<long long>(pkp.first / 10),
                        static_cast<long long>(pkp.second / 10));
            std::printf("  (ROSE = peak at least double the start and +3 regions. FELL = ended at\n"
                        "   or under 60%% of its own peak. Both are reporting definitions. The\n"
                        "   population column reads a polity's SHARE of all held population, with\n"
                        "   +3 average regions' worth of share in place of +3 regions -- BL-970.)\n");
        }

        // --- BL-750: THE TARIFF DISTRIBUTION ------------------------------
        //
        // THIS TABLE IS THE ITEM'S DELIVERABLE. Ben, 2026-09-06: the derived
        // form ships, and the VERB form (BL-488) is held as a fallback with one
        // named trigger -- FLATNESS. If every world tariffs the same share of
        // its nations in the same band, the protection scalar carries no real
        // variation and the verb is what would give it some. So read the RANGE
        // columns, not the medians: a tight range across the spread is the
        // finding, and it is reported whether or not it is the pleasing answer.
        //
        // NOTHING HERE IS GATED. A world with no tariff is a legitimate outcome
        // -- a world whose polities never industrialised has no industrial
        // competitor to protect against -- exactly as a world with no empire is.
        std::printf("\n--- BL-750  TARIFF POSTURE, per world (the GENERATED world) ---\n");
        std::printf("  seed   nations   tariffed   %%5   %%10   %%20   "
                    "protection max / median   sub-floor nations\n");
        for (const sweep_row& r : rows)
            std::printf("  %4u   %7d   %8d   %3d   %4d   %4d   %11d / %-9d   %17d\n",
                        r.seed, r.nations_total, r.nations_tariffed,
                        r.tariff_band_low, r.tariff_band_mid, r.tariff_band_high,
                        r.protection_max_q, r.protection_med_q, r.city_states);
        {
            std::vector<int64_t> tar, share, pmax, cs;
            int64_t low = 0, mid = 0, high = 0;
            int worlds_with_tariff = 0;
            for (const sweep_row& r : rows)
            {
                if (r.nations_tariffed > 0) ++worlds_with_tariff;
                tar.push_back(r.nations_tariffed);
                share.push_back(r.nations_total > 0
                                ? (r.nations_tariffed * 1000) / r.nations_total : 0);
                pmax.push_back(r.protection_max_q);
                cs.push_back(r.city_states);
                low  += r.tariff_band_low;
                mid  += r.tariff_band_mid;
                high += r.tariff_band_high;
            }
            const auto sp_share = span(share);
            const auto sp_pmax  = span(pmax);
            const auto sp_tar   = span(tar);
            std::printf("\n  WORLDS WITH ANY TARIFF       %d / %d\n",
                        worlds_with_tariff, static_cast<int>(rows.size()));
            std::printf("  TARIFFED NATIONS             median %lld   range %lld..%lld\n",
                        static_cast<long long>(median_of(tar)),
                        static_cast<long long>(sp_tar.first),
                        static_cast<long long>(sp_tar.second));
            std::printf("  TARIFFED SHARE of nations    median %lld%%  range %lld%%..%lld%%\n",
                        static_cast<long long>(median_of(share) / 10),
                        static_cast<long long>(sp_share.first / 10),
                        static_cast<long long>(sp_share.second / 10));
            std::printf("  BAND SPLIT (all seeds)       %%5 %lld   %%10 %lld   %%20 %lld\n",
                        static_cast<long long>(low),
                        static_cast<long long>(mid),
                        static_cast<long long>(high));
            std::printf("  PEAK PROTECTION reached      median %lld   range %lld..%lld  (of 1000)\n",
                        static_cast<long long>(median_of(pmax)),
                        static_cast<long long>(sp_pmax.first),
                        static_cast<long long>(sp_pmax.second));
            std::printf("  SUB-FLOOR NATIONS (BL-769)   median %lld per world\n",
                        static_cast<long long>(median_of(cs)));
            std::printf("\n  the scalar at POLITY grain (its own, before the handoff):\n");
            std::printf("  seed   alive   distinct crossing years   protection max / median"
                        "   over floor\n");
            for (const sweep_row& r : rows)
                std::printf("  %4u   %5d   %22d   %11d / %-9d   %10d\n",
                            r.seed, r.polities_alive, r.distinct_cross_yrs,
                            r.protection_pmax, r.protection_pmed, r.polities_over_floor);
            std::printf("  (DISTINCT CROSSING YEARS is the diagnostic. The timing term reads a\n"
                        "   polity's furnace year against the field; if a world crosses the rung\n"
                        "   in one or two years flat, that term has nothing to read and a sparse\n"
                        "   tariff table is an UPSTREAM finding about the furnace, not about the\n"
                        "   banding and not, on its own, the BL-488 trigger.)\n");
            std::printf("  (FLAT = every world at the same share in the same band. That, and only\n"
                        "   that, is what reopens the scored-verb form of protection -- BL-488.)\n");
        }
    }

    // --- BL-926: THE INSTRUMENT SEES THE ARC ---------------------------------
    //
    // REPORTED, NEVER GATED -- the item's own rule and the harness's. This is
    // the baseline every later sprint-39 mechanism is read against, so every
    // line prints whether or not the number is flattering.
    if (!rows.empty())
    {
        std::printf("\n=== BL-926  THE ARC -- lifespans, gross deaths, pieces, supply, cultures ===\n");
        std::printf("  arc measured: %s  %lld -> %lld\n", rows.front().arc_name.c_str(),
                    static_cast<long long>(rows.front().arc_start),
                    static_cast<long long>(rows.front().arc_stop));

        // (1) Deaths, GROSS beside NET, and the secession event grain.
        std::printf("\n--- BL-926.1  DEATHS (gross beside net) and BREAKDOWNS, per world ---\n");
        std::printf("  seed   ever held   alive   dead GROSS   net loss   breakdowns   pieces   "
                    "regions walked   piece sizes\n");
        for (const sweep_row& r : rows)
        {
            std::string sizes;
            for (std::size_t i = 0; i < r.piece_sizes.size() && i < 12; ++i)
            {
                if (i) sizes += ' ';
                sizes += std::to_string(r.piece_sizes[i]);
            }
            if (r.piece_sizes.size() > 12) sizes += " ...";
            if (r.piece_sizes.empty()) sizes = "-";
            std::printf("  %4u   %9d   %5d   %10d   %8d   %10lld   %6lld   %14lld   %s\n",
                        r.seed, r.polities_ever_held, r.polities_alive, r.polities_dead_gross,
                        r.powers_start - r.powers_end,
                        static_cast<long long>(r.breakdowns),
                        static_cast<long long>(r.secessions),
                        static_cast<long long>(r.regions_seceded), sizes.c_str());
        }
        {
            std::vector<int64_t> gross, net, bd, pc;
            int64_t pieces_all = 0, piece_max = 0;
            for (const sweep_row& r : rows)
            {
                gross.push_back(r.polities_dead_gross);
                net.push_back(r.powers_start - r.powers_end);
                bd.push_back(r.breakdowns);
                pc.push_back(r.secessions);
                for (int32_t s : r.piece_sizes) { ++pieces_all; if (s > piece_max) piece_max = s; }
            }
            std::printf("\n  DEAD, GROSS            median %lld per world   (NET loss median %lld --"
                        " the gap is births)\n",
                        static_cast<long long>(median_of(gross)), static_cast<long long>(median_of(net)));
            std::printf("  BREAKDOWNS             median %lld   PIECES median %lld   "
                        "largest piece %lld regions (%lld pieces pooled)\n",
                        static_cast<long long>(median_of(bd)), static_cast<long long>(median_of(pc)),
                        static_cast<long long>(piece_max), static_cast<long long>(pieces_all));
        }

        // (2) The per-polity table, top N by peak share.
        std::printf("\n--- BL-926.2  THE TOP %d POLITIES BY PEAK SHARE, per world ---\n", arc_table_n);
        std::printf("  (born = first region held; peak = most held at once, and the first year;"
                    " died = year emptied, or ALIVE;\n   killer = who took the last region;"
                    " parent = the realm it broke from. '-' = the sim records none.)\n");
        for (const sweep_row& r : rows)
        {
            std::printf("  seed %u:\n", r.seed);
            std::printf("    id    born    peak  (share)   peak yr    end    died      killer   parent   pop peak (share)\n");
            for (std::size_t i = 0; i < r.arcs.size() && i < static_cast<std::size_t>(arc_table_n); ++i)
            {
                const sweep_row::polity_arc& a = r.arcs[i];
                char died[16], killer[8], parent[8];
                if (a.alive || a.died == INT64_MIN) std::snprintf(died, sizeof died, "  ALIVE");
                else std::snprintf(died, sizeof died, "%7lld", static_cast<long long>(a.died));
                if (a.killer < 0) std::snprintf(killer, sizeof killer, "-");
                else              std::snprintf(killer, sizeof killer, "%d", a.killer);
                if (a.parent < 0) std::snprintf(parent, sizeof parent, "-");
                else              std::snprintf(parent, sizeof parent, "%d", a.parent);
                std::printf("    %3d  %6lld  %6d  (%3d%%)   %7lld  %5d  %s   %6s   %6s   %10lld (%3d%%)\n",
                            a.id, static_cast<long long>(a.born), a.peak, a.peak_share_q / 10,
                            static_cast<long long>(a.peak_year), a.end_hold, died, killer, parent,
                            static_cast<long long>(a.peak_pop), a.peak_pop_share_q / 10);
            }
        }

        // (3) The supply histogram over held ground at the stop year.
        std::printf("\n--- BL-926.3  network_supply_q OVER HELD GROUND at the stop year ---\n");
        std::printf("  floors in force: settlement %d   secession %d   campaign %d\n",
                    rows.front().floor_settle, rows.front().floor_secede, rows.front().floor_campaign);
        std::printf("  seed   held   ==0    <=settle   <=secede   <campaign   "
                    "deciles 0..9 (per-mille of held)\n");
        for (const sweep_row& r : rows)
        {
            const auto pm = [&](int64_t n) {
                return r.supply_held > 0 ? (n * 1000) / r.supply_held : 0; };
            std::printf("  %4u   %4lld   %3lld%%   %7lld%%   %7lld%%   %8lld%%   ",
                        r.seed, static_cast<long long>(r.supply_held),
                        static_cast<long long>(pm(r.supply_zero) / 10),
                        static_cast<long long>(pm(r.supply_le_settle) / 10),
                        static_cast<long long>(pm(r.supply_le_secede) / 10),
                        static_cast<long long>(pm(r.supply_le_campaign) / 10));
            for (int b = 0; b < 10; ++b)
                std::printf("%4lld", static_cast<long long>(pm(r.supply_hist[b])));
            std::printf("\n");
        }
        std::printf("  (A '<=secede' column at 0%% in every world means the BL-896 floor never bit;\n"
                    "   a '==0' column that is most of the map means the network is disconnected.\n"
                    "   Both are FINDINGS. REPORTED, not gated.)\n");

        // (4) Cultures, foundings by source, civilisations, city states over time.
        std::printf("\n--- BL-926.4  CULTURES, FOUNDINGS BY SOURCE, CIVILISATIONS, CITY STATES ---\n");
        std::printf("  seed   cultures   roots   tree depth   foundings sched / settle   "
                    "civilisations   city states over time (start .. stop)\n");
        for (const sweep_row& r : rows)
        {
            char cu[12], ro[12], de[12];
            if (r.cultures < 0) { std::snprintf(cu, sizeof cu, "-"); std::snprintf(ro, sizeof ro, "-");
                                  std::snprintf(de, sizeof de, "-"); }
            else { std::snprintf(cu, sizeof cu, "%d", r.cultures);
                   std::snprintf(ro, sizeof ro, "%d", r.cultures_root);
                   std::snprintf(de, sizeof de, "%d", r.culture_depth); }
            std::printf("  %4u   %8s   %5s   %10s   %9lld / %-9lld   %13lld   ",
                        r.seed, cu, ro, de,
                        static_cast<long long>(r.foundings_scheduled),
                        static_cast<long long>(r.foundings_settled),
                        static_cast<long long>(r.civilisations));
            // Five points across the series: start, quarter, half, three-quarter, stop.
            const std::size_t n = r.city_states_series.size();
            for (int k = 0; k < 5; ++k)
            {
                if (n == 0) break;
                const std::size_t at = std::min(n - 1, (n - 1) * static_cast<std::size_t>(k) / 4);
                std::printf("%s%d", k ? " " : "", r.city_states_series[at]);
            }
            std::printf("\n");
        }
        {
            std::vector<int64_t> cu, dp, civ, sched, settled;
            for (const sweep_row& r : rows)
            {
                if (r.cultures >= 0) { cu.push_back(r.cultures); dp.push_back(r.culture_depth); }
                civ.push_back(r.civilisations);
                sched.push_back(r.foundings_scheduled);
                settled.push_back(r.foundings_settled);
            }
            if (!cu.empty())
                std::printf("\n  CULTURES               median %lld per world   tree depth median %lld\n",
                            static_cast<long long>(median_of(cu)), static_cast<long long>(median_of(dp)));
            std::printf("  FOUNDINGS by source    schedule median %lld   Settle verb median %lld\n",
                        static_cast<long long>(median_of(sched)), static_cast<long long>(median_of(settled)));
            std::printf("  CIVILISATIONS formed   median %lld per world\n",
                        static_cast<long long>(median_of(civ)));
            std::printf("  (city states = polities holding EXACTLY ONE region, sampled every %lld years.)\n",
                        static_cast<long long>(rows.front().city_states_step));
        }

        // BL-920 -- ground taken by ORGANISE beside ground taken by
        // CONQUEST, and the count of city states that ROSE (crossed
        // `city_state_population_threshold` on unorganised ground) rather
        // than opened the run already above it. Read before and after, no
        // target set -- CIVILISATION.md sec "A city state spawns where a
        // region's population is above a threshold".
        std::printf("\n--- BL-920  ORGANISED vs CONQUERED, and CITY STATES THAT ROSE ---\n");
        std::printf("  seed   organised   conquests   organised share   city states risen\n");
        {
            std::vector<int64_t> org_share, risen;
            for (const sweep_row& r : rows)
            {
                const int64_t total = r.organised + r.conquests;
                const int64_t share_pm = total > 0 ? (r.organised * 1000) / total : 0;
                std::printf("  %4u   %9lld   %9lld   %14lld.%1lld%%   %17lld\n",
                            r.seed,
                            static_cast<long long>(r.organised),
                            static_cast<long long>(r.conquests),
                            static_cast<long long>(share_pm / 10),
                            static_cast<long long>(share_pm % 10),
                            static_cast<long long>(r.city_states_risen));
                org_share.push_back(share_pm);
                risen.push_back(r.city_states_risen);
            }
            std::printf("  ORGANISED SHARE of (organised + conquests)   median %lld.%1lld%%\n",
                        static_cast<long long>(median_of(org_share) / 10),
                        static_cast<long long>(median_of(org_share) % 10));
            std::printf("  CITY STATES RISEN (crossed the threshold mid-span)   median %lld per world\n",
                        static_cast<long long>(median_of(risen)));
        }
    }

    // --- JSON ---------------------------------------------------------------
    if (FILE* f = std::fopen("history_sweep.json", "w"))
    {
        std::fprintf(f, "{\n \"_note\": \"%s\",\n \"threshold_q\": %d,\n \"worlds\": [\n",
                     json_escape("BL-275 history sweep. Reported, not gated — see the harness "
                                 "header. One row per seed; shares are per-mille. BL-970: every "
                                 "share is carried twice, by regions held (legacy key) and by "
                                 "population held (*_pop / *_pop_q sibling).").c_str(),
                     hegemony_threshold_q);
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const sweep_row& r = rows[i];
            std::fprintf(f,
                "  {\"seed\": %u, \"regions_start\": %d, \"regions_end\": %d, "
                "\"powers_start\": %d, \"powers_end\": %d, \"top_share_q\": %d, "
                "\"peak_share_q\": %d, \"smallest_holding\": %d, \"hegemony_year\": %lld, \"battles\": %lld, "
                "\"conquests\": %lld, \"foundings\": %lld, \"peak_population\": %lld, "
                "\"peak_year\": %lld, \"epoch_population\": %lld, \"lacunae\": %d, "
                "\"industrial_first\": %lld, \"industrial_median\": %lld, "
                "\"industrial_last\": %lld, \"ms\": %lld,\n",
                r.seed, r.regions_start, r.regions_end, r.powers_start, r.powers_end,
                r.top_share_q, r.peak_share_q, r.smallest_holding,
                static_cast<long long>(r.hegemony_year),
                static_cast<long long>(r.battles), static_cast<long long>(r.conquests),
                static_cast<long long>(r.foundings), static_cast<long long>(r.peak_population),
                static_cast<long long>(r.peak_year), static_cast<long long>(r.epoch_population),
                r.lacunae, static_cast<long long>(r.industrial_first),
                static_cast<long long>(r.industrial_median),
                static_cast<long long>(r.industrial_last), static_cast<long long>(r.ms));

            // BL-926 -- the arc, on the row. Everything the face prints above
            // and enough of the funnel/churn/secession/creed figures that two
            // runs can be diffed off disk.
            std::fprintf(f,
                "   \"top_share_pop_q\": %d, \"peak_share_pop_q\": %d, \"hegemony_year_pop\": %lld, "
                "\"smallest_holding_pop\": %lld, \"pop_recorded\": %s,\n"
                "   \"arc\": \"%s\", \"arc_start\": %lld, \"arc_stop\": %lld,\n"
                "   \"polities_ever_held\": %d, \"polities_alive\": %d, \"polities_dead_gross\": %d, "
                "\"net_loss\": %d,\n"
                "   \"breakdowns\": %lld, \"secessions\": %lld, \"regions_seceded\": %lld, "
                "\"piece_sizes\": [",
                r.top_share_pop_q, r.peak_share_pop_q,
                static_cast<long long>(r.hegemony_year_pop),
                static_cast<long long>(r.smallest_holding_pop),
                r.pop_recorded ? "true" : "false",
                json_escape(r.arc_name).c_str(),
                static_cast<long long>(r.arc_start), static_cast<long long>(r.arc_stop),
                r.polities_ever_held, r.polities_alive, r.polities_dead_gross,
                r.powers_start - r.powers_end,
                static_cast<long long>(r.breakdowns), static_cast<long long>(r.secessions),
                static_cast<long long>(r.regions_seceded));
            for (std::size_t k = 0; k < r.piece_sizes.size(); ++k)
                std::fprintf(f, "%s%d", k ? "," : "", r.piece_sizes[k]);
            std::fprintf(f,
                "],\n"
                "   \"supply_held\": %lld, \"supply_zero\": %lld, \"supply_le_settle\": %lld, "
                "\"supply_le_secede\": %lld, \"supply_lt_campaign\": %lld,\n"
                "   \"floor_settle\": %d, \"floor_secede\": %d, \"floor_campaign\": %d, "
                "\"supply_hist\": [",
                static_cast<long long>(r.supply_held), static_cast<long long>(r.supply_zero),
                static_cast<long long>(r.supply_le_settle), static_cast<long long>(r.supply_le_secede),
                static_cast<long long>(r.supply_le_campaign),
                r.floor_settle, r.floor_secede, r.floor_campaign);
            for (int b = 0; b < 10; ++b)
                std::fprintf(f, "%s%lld", b ? "," : "", static_cast<long long>(r.supply_hist[b]));
            std::fprintf(f,
                "],\n"
                "   \"cultures\": %d, \"cultures_root\": %d, \"culture_depth\": %d,\n"
                "   \"foundings_scheduled\": %lld, \"foundings_settled\": %lld, "
                "\"civilisations\": %lld,\n"
                "   \"city_states_step\": %lld, \"city_states_series\": [",
                r.cultures, r.cultures_root, r.culture_depth,
                static_cast<long long>(r.foundings_scheduled),
                static_cast<long long>(r.foundings_settled),
                static_cast<long long>(r.civilisations),
                static_cast<long long>(r.city_states_step));
            for (std::size_t k = 0; k < r.city_states_series.size(); ++k)
                std::fprintf(f, "%s%d", k ? "," : "", r.city_states_series[k]);
            // The funnel, churn and creed figures that were face-only before.
            std::fprintf(f,
                "],\n"
                "   \"shape_rose\": %d, \"shape_fell\": %d, \"shape_rpf\": %d, "
                "\"shape_top_peak_q\": %d,\n"
                "   \"shape_rose_pop\": %d, \"shape_fell_pop\": %d, \"shape_rpf_pop\": %d, "
                "\"shape_top_peak_pop_q\": %d,\n"
                "   \"campaign_contacts\": %lld, \"campaign_scored\": %lld, "
                "\"campaign_cleared\": %lld, \"campaign_chosen\": %lld, "
                "\"illegal_campaigns\": %lld, \"starved_campaigns\": %lld, \"reach_denied\": %lld,\n"
                "   \"regions_touched\": %d, \"regions_once\": %d, \"regions_thrice\": %d, "
                "\"max_flips\": %d,\n"
                "   \"creeds_arisen\": %lld, \"creed_adoptions\": %lld, "
                "\"creed_converted\": %lld, \"creed_reasserted\": %lld, "
                "\"schisms\": %lld, \"regions_sundered\": %lld,\n"
                "   \"fear_leaned\": %lld, \"fear_targets\": %lld,\n"
                "   \"polities\": [",
                r.shape_rose, r.shape_fell, r.shape_rpf, r.shape_top_peak_q,
                r.shape_rose_pop, r.shape_fell_pop, r.shape_rpf_pop, r.shape_top_peak_pop_q,
                static_cast<long long>(r.campaign_contacts), static_cast<long long>(r.campaign_scored),
                static_cast<long long>(r.campaign_cleared), static_cast<long long>(r.campaign_chosen),
                static_cast<long long>(r.illegal_campaigns), static_cast<long long>(r.starved_campaigns),
                static_cast<long long>(r.reach_denied),
                r.regions_touched, r.regions_once, r.regions_thrice, r.max_flips,
                static_cast<long long>(r.creeds_arisen), static_cast<long long>(r.creed_adoptions),
                static_cast<long long>(r.creed_converted), static_cast<long long>(r.creed_reasserted),
                static_cast<long long>(r.schisms), static_cast<long long>(r.regions_sundered),
                static_cast<long long>(r.fear_leaned), static_cast<long long>(r.fear_targets));
            for (std::size_t k = 0; k < r.arcs.size() && k < static_cast<std::size_t>(arc_table_n); ++k)
            {
                const sweep_row::polity_arc& a = r.arcs[k];
                std::fprintf(f,
                    "%s\n    {\"id\": %d, \"born\": %lld, \"peak\": %d, \"peak_year\": %lld, "
                    "\"peak_share_q\": %d, \"end_hold\": %d, \"alive\": %s, \"died\": %s, "
                    "\"killer\": %d, \"parent\": %d, \"peak_pop_share_q\": %d, \"peak_pop\": %lld}",
                    k ? "," : "", a.id, static_cast<long long>(a.born), a.peak,
                    static_cast<long long>(a.peak_year), a.peak_share_q, a.end_hold,
                    a.alive ? "true" : "false",
                    (a.alive || a.died == INT64_MIN) ? "null"
                        : std::to_string(static_cast<long long>(a.died)).c_str(),
                    a.killer, a.parent, a.peak_pop_share_q, static_cast<long long>(a.peak_pop));
            }
            std::fprintf(f, "\n   ]}%s\n", (i + 1 < rows.size()) ? "," : "");
        }
        std::fprintf(f, " ]\n}\n");
        std::fclose(f);
        std::printf("\nWrote history_sweep.json (%d rows)\n", static_cast<int>(rows.size()));
    }

    // BL-908 — CONTACT, print-only (BL-907 owns surfacing this in the report
    // face proper). One line per seed, so a run can be pointed at as evidence
    // that the record populates and that "settled and unmet" is reachable.
    {
        std::printf("\n--- BL-908  CONTACT AT THE EPOCH (all seeds) ---\n");
        int any_unmet = 0;
        for (const sweep_row& r : rows)
        {
            std::printf("  seed %5u: %4lld contact pairs   unmet pair: %s\n",
                        r.seed, static_cast<long long>(r.contact_pairs),
                        r.unmet_pair ? "YES" : "no");
            if (r.unmet_pair) ++any_unmet;
        }
        std::printf("  %d of %d seeds show at least one unmet pair among living polities\n",
                    any_unmet, static_cast<int>(rows.size()));
    }

    // --- Structural checks only --------------------------------------------
    std::printf("\n");
    check(static_cast<int>(rows.size()) == seed_count,
          "S1   every seed produced a world and completed its run");

    if (derive_from_generation && !tuned)
        check(derived_matches_generation && derived_seeds_checked > 0,
              "S1b  --epoch: the re-run IS generation's own era (counts match the report)");
    else if (derive_from_generation)
        std::printf("[SKIP] S1b  --set overrode the forces, so this run is DELIBERATELY not\n"
                    "            generation's own era. The acceptance test would fail by\n"
                    "            construction; it is skipped rather than left to mislead.\n");

    // Determinism across the sweep boundary: re-running one seed reproduces it.
    if (!rows.empty())
    {
        // The epoch too, not just the seed: without it the recheck built its
        // world at epoch 0 while the rows were built at --epoch, so S2 compared
        // a run against a different world and called the agreement determinism.
        world_params wp; wp.seed = rows.front().seed; wp.epoch_year = epoch_year;
        generation_report     rep;
        era_minus_one_fixture fx;
        // SAME ROUTE AS THE ROWS. The rows take the fixture under --epoch; a
        // recheck that took the other route would compare generation's era
        // against the struct-default one and call the disagreement
        // non-determinism. That is the BL-757 defect reappearing inside the
        // check meant to catch it.
        const world w = make_hard_coded_world(wp, &rep, world_gen_config{},
                                              /*progress=*/nullptr, /*works=*/nullptr,
                                              derive_from_generation ? &fx : nullptr);
        const generation_report::body_entry* k = kepler_of(rep);
        settlement_state ss = (derive_from_generation && fx.ran) ? fx.settlement
                                                                 : k->settlement;
        // The SAME params the sweep rows above ran on — a recheck that ran a
        // different span would not be a recheck. This used to re-declare
        // `history_sim_params params;` here, which shadowed the hoisted one
        // (MSVC C4456) and silently made the recheck a -4000 -> 0 six-band run
        // whenever --epoch put the rows on a two-span clock. It passed only
        // because both sides were vacuously zero at the default epoch.
        // The re-run must be the SAME run: real terrain, real dims. The original
        // recheck passed an empty sim_terrain_view against real-terrain rows —
        // a guaranteed false FAIL the moment terrain changes any decision.
        const entity_id kepler_id = k->id; // the report entry names its own entity (BL-257)
        const sim_terrain_arrays terr = build_sim_terrain(w, kepler_id,
                                                          home_grid_width, home_grid_height);
        // The re-run must carry the SAME works registry for the same reason it
        // must carry the same terrain: a re-run that differs in an input is not
        // a determinism check, it is a guaranteed false FAIL.
        // THE RE-RUN MUST CARRY THE OVERRIDES TOO (2026-09-11). The seed loop
        // runs the deriving path on a COPY of `fx.params` with `--set` applied
        // (see "THE OVERRIDES REACH THE FIXTURE'S PARAMS TOO" above), while the
        // fixture itself is deliberately left untouched so the acceptance test
        // still knows what generation actually ran. This re-run read the
        // UNTOUCHED fixture, so under any `--set` it compared an overridden row
        // against an un-overridden re-run and failed every time. Confirmed with
        // an unrelated tunable: `--epoch 0 --set holdings_burden_q=40` fails S2
        // exactly the same way.
        //
        // That is the failure mode the comment below already names, and it was
        // dangerous rather than merely noisy: a check that ALWAYS goes red under
        // `--set` cannot report a real determinism break on that path, because
        // nobody would believe it.
        history_sim_params re_fp = fx.params;
        for (const param_override& o : g_overrides) apply_override(re_fp, o.name, o.value);
        const history_sim_state again = (derive_from_generation && fx.ran)
            ? run_history_sim(ss, &fx.creeds, fx.terrain.view(), fx.gw, fx.gh,
                              re_fp, fx.seed, nullptr, fx.works)
            : run_history_sim(ss, nullptr, terr.view(),
                            home_grid_width, home_grid_height, params, wp.seed,
                            nullptr, &works);
        check(again.battles == rows.front().battles
           && again.conquests == rows.front().conquests
           && again.works_raised == rows.front().works_raised,
              "S2   re-running a swept seed reproduces its row exactly");
    }

    bool spread = false;
    for (const sweep_row& r : rows)
        if (r.battles != rows.front().battles) { spread = true; break; }
    check(spread || rows.size() <= 1,
          "S3   seeds actually diverge — the sweep measures a spread, not one world N times");

    // --- F: fear of being next (BL-838) -------------------------------------
    //
    // THESE ARE THE ITEM'S DONE-WHEN, AND THEY GATE. AI_OPPONENT.md sec 11's
    // 2026-09-11 grant lets the Era -1 scorer read the grudge ledger for one
    // question only, and says the check that the scope held is BEHAVIOURAL
    // rather than structural: "a large but peaceful polity must attract no
    // coalition, while a smaller aggressive one does."
    //
    // WHY A HAND-BUILT LEDGER RATHER THAN A RUN'S AGGREGATES. The failure this
    // guards against is a size coefficient smuggled in by the back door, and a
    // size coefficient CORRELATES with the behavioural one in any real world --
    // realms that conquer a lot are also large. Only a fixture that separates
    // the two can tell them apart, so this one does by construction.
    std::printf("\n");
    {
        history_sim_params fp = tuned_defaults();
        fp.w_fear_q       = 400;
        fp.fear_reference = 2000;

        history_sim_state fs;
        // Four polities. 0 = the DECIDER. 1 = its KIN, same culture. 2 = a
        // LARGE, PEACEFUL realm. 3 = a SMALL, AGGRESSIVE one.
        for (int i = 0; i < 4; ++i)
        {
            polity q; q.id = i; q.alive = true;
            q.culture = (i <= 1) ? 7 : (10 + i);
            fs.polities.push_back(q);
        }
        // THE ONLY DIFFERENCE BETWEEN 2 AND 3 THAT THIS CODE CAN SEE is the
        // ledger. Nothing in `fear_of_next_q` reads holdings, so there is no
        // size field to set here at all -- which is itself the point.
        //
        // Sorted by (from, to), as the real table is.
        const auto g = [](int from, int to, int score) {
            grudge x; x.from = static_cast<uint16_t>(from);
            x.to = static_cast<uint16_t>(to); x.score = score; x.peak = score;
            return x;
        };
        fs.grudges.push_back(g(0, 2, 4000)); // The decider's OWN grudge against the peaceful realm.
        fs.grudges.push_back(g(1, 3, 1200)); // Kin resent the aggressive one.
        fs.grudges.push_back(g(2, 3,  900)); // ...and so does a stranger.

        const int fear_peaceful  = fear_of_next_q(fs, fp, 0, 2);
        const int fear_aggressor = fear_of_next_q(fs, fp, 0, 3);

        check(fear_peaceful == 0,
              "F1   a PEACEFUL polity attracts no fear -- and the decider's OWN 4000-point "
              "grudge against it is NOT read (the revenge term BL-827 declined)");
        check(fear_aggressor > 0,
              "F2   an AGGRESSIVE polity does -- what it did to the decider's people is read");
        check(fear_aggressor == 600,
              "F3   ...and only the KIN grudge counts: 1200/2000 = 600, the stranger's 900 ignored");

        // F4 -- THE SIZE-COEFFICIENT TRAP, PUT DIRECTLY. Kill every grudge and
        // leave the polities exactly as they were. If anything in this term
        // read rank, the answer would still separate them.
        {
            history_sim_state nf = fs;
            nf.grudges.clear();
            check(fear_of_next_q(nf, fp, 0, 2) == 0 && fear_of_next_q(nf, fp, 0, 3) == 0,
                  "F4   with the LEDGER EMPTY nobody is feared -- the trigger is behaviour, "
                  "never size or rank (the exclusion the grant exists to enforce)");
        }

        // F5 -- a dead aggrieved party's resentment does not vote, matching
        // `extinguish_polity`: the dead leave a grudge in their kin, not a
        // standing entry of their own.
        {
            history_sim_state nf = fs;
            nf.polities[1].alive = false;
            check(fear_of_next_q(nf, fp, 0, 3) == 0,
                  "F5   a DEAD kin's grudge does not vote");
        }

        // F6 -- w_fear_q 0 is genuinely off: the struct default must leave
        // every existing fixture in this repo meaning what it meant.
        check(history_sim_params{}.w_fear_q == 0,
              "F6   the struct default is OFF, so no existing fixture changes meaning");
    }

    // --- Turbulence-lean checks (BL-839) -----------------------------------
    //
    // THESE GATE, and they are deliberately about the three resolver functions
    // rather than about a run's aggregates. The item's binding claim is a claim
    // about PROPERTIES -- ordinary re-bases nothing, the lean is a spread and
    // not a dial, and it scales a force it never introduces -- and a property
    // asserted on 16 noisy worlds is not asserted at all. Whether the settings
    // actually SEPARATE is a distribution question and is reported, never
    // gated: GENERATION_STRATEGY.md sec Asymmetry is the deliverable forbids
    // picking the threshold before the spread is measured, and a measured null
    // result is a finding, not a failure.
    std::printf("\n");
    {
        // T1 -- ORDINARY IS THE IDENTITY, on all three forces. This is the
        // whole basis on which the item claims no existing fixture changes
        // meaning, so it is asserted rather than argued.
        history_sim_params ord = tuned_defaults();
        ord.turbulence_lean = 0;
        ord.w_fear_q = 400;
        bool identity = (leaned_w_fear_q(ord) == ord.w_fear_q)
                        && (leaned_terrain_reach_cost_q(ord) == ord.terrain_reach_cost_q);
        for (int a = 0; a <= 1000 && identity; a += 25)
            if (leaned_aggression_q(ord, a) != a) identity = false;
        check(identity, "T1   ORDINARY is the identity on all three forces");
        check(history_sim_params{}.turbulence_lean == 0,
              "T1b  ordinary is the STRUCT DEFAULT, so no existing fixture is re-based");

        // T2 -- IT IS A SPREAD, NOT A DIAL. The 500 neutral is a fixed point at
        // every setting, and a culture above it moves the OPPOSITE way from one
        // below it. A dial would move both the same way, and that is the defect
        // this check exists to catch -- it would pass every aggregate the sweep
        // prints while being a different design.
        history_sim_params turb = ord; turb.turbulence_lean =  1;
        history_sim_params calm = ord; calm.turbulence_lean = -1;
        check(leaned_aggression_q(turb, 500) == 500 && leaned_aggression_q(calm, 500) == 500,
              "T2   the 500 neutral is a FIXED POINT at every setting");
        check(leaned_aggression_q(turb, 800) > 800 && leaned_aggression_q(turb, 200) < 200,
              "T2b  turbulent WIDENS -- the warlike more so, the placid more so");
        check(leaned_aggression_q(calm, 800) < 800 && leaned_aggression_q(calm, 200) > 200,
              "T2c  calm NARROWS, from both sides toward the neutral");
        check(leaned_aggression_q(turb, 0) >= 0 && leaned_aggression_q(turb, 1000) <= 1000,
              "T2d  the widened value stays inside the 0-1000 currency");

        // T3 -- IT SCALES, IT NEVER INTRODUCES. A run whose w_fear_q is 0 --
        // the struct default and every isolating fixture in this repo -- must
        // stay at 0 under BOTH settings. If this ever fails, the lean has
        // quietly switched on a force under AI_OPPONENT.md sec 11's grant that
        // the run in hand had deliberately switched off.
        history_sim_params off = ord; off.w_fear_q = 0;
        history_sim_params off_t = off; off_t.turbulence_lean = 1;
        history_sim_params off_c = off; off_c.turbulence_lean = -1;
        check(leaned_w_fear_q(off_t) == 0 && leaned_w_fear_q(off_c) == 0,
              "T3   a run with w_fear_q OFF stays off at every setting");
        check(leaned_w_fear_q(turb) > ord.w_fear_q && leaned_w_fear_q(calm) < ord.w_fear_q,
              "T3b  turbulent coalesces harder against a riser, calm less");
        check(leaned_terrain_reach_cost_q(turb) > ord.terrain_reach_cost_q
                  && leaned_terrain_reach_cost_q(calm) < ord.terrain_reach_cost_q
                  && leaned_terrain_reach_cost_q(calm) >= 0,
              "T3c  turbulent makes reach DEARER, calm cheaper, neither negative");

        // T4 -- AN OUT-OF-RANGE SETTING IS CLAMPED, not scaled. `--set
        // turbulence_lean=7` is one keystroke away and must not silently
        // produce a force multiplied by 3.8 under a table labelled "turbulent".
        history_sim_params wild = ord; wild.turbulence_lean = 7;
        check(leaned_w_fear_q(wild) == leaned_w_fear_q(turb)
                  && leaned_aggression_q(wild, 800) == leaned_aggression_q(turb, 800),
              "T4   a lean outside -1..+1 clamps to the named setting");

        // T5 -- NOTHING HERE COUNTS POLITIES. Stated as a check because it is
        // the item's absolute rule and the one a later edit is most likely to
        // break: all three resolvers are pure in `history_sim_params` alone and
        // cannot see a `history_sim_state`, so no post-hoc count correction can
        // be written into them without changing their signatures. The check is
        // structural -- it compiles or it does not.
        static_assert(
            std::is_same_v<decltype(&leaned_w_fear_q), int (*)(const history_sim_params&)>,
            "BL-839: the turbulence resolvers must never see world state -- "
            "a signature taking history_sim_state is a count correction waiting to happen");
        check(true, "T5   the resolvers are pure in params -- they CANNOT count polities");
    }

    // --- Works checks (BL-321) ---------------------------------------------
    //
    // These DO gate, unlike the distribution rows above, and the difference is
    // principled: S1-S3 refuse to assert tuning targets nobody has chosen, but
    // "the gate fires on the ground it names" and "the effect reaches a
    // consumer" are not tuning — they are the mechanism either working or not.
    std::printf("\n");
    {
        const sim_terrain_view no_terrain{};

        // W1 — gates read the ground they name. An ore-gated work is offered on
        // ore-rich land and withheld on ore-poor land, at the same population.
        {
            settlement_state rich = strip_world(1, 900, 900, 900, 0);
            settlement_state poor = strip_world(1, 900,   0, 900, 0);
            rich.regions[0].population = 50000;
            poor.regions[0].population = 50000;

            const auto ra = works.available(rich.regions[0], roster_band::classical);
            const auto pa = works.available(poor.regions[0], roster_band::classical);

            bool rich_has_pits = false, poor_has_pits = false;
            for (const work_row* r : ra) if (r->name == "Test Pits") rich_has_pits = true;
            for (const work_row* r : pa) if (r->name == "Test Pits") poor_has_pits = true;

            check(rich_has_pits && !poor_has_pits,
                  "W1   an ore-gated work is offered on ore-rich ground and withheld on ore-poor");

            // The other half of the gate invariant: poor ground is not shut out
            // entirely. If it were, breadth would be a ceiling again for exactly
            // the polities that most need to buy their way past it.
            bool poor_has_reach = false;
            for (const work_row* r : pa) if (r->effect.reach_mod > 0) poor_has_reach = true;
            check(poor_has_reach,
                  "W1b  ore-poor ground can still raise a reach work — breadth stays a decision");
        }

        // W2 — the population floor gates independently of the endowment.
        {
            settlement_state small = strip_world(1, 900, 900, 900, 0);
            small.regions[0].population = 1000; // Under every population floor here.
            const auto sa = works.available(small.regions[0], roster_band::classical);
            check(sa.empty(),
                  "W2   a region under every population floor is offered nothing");
        }

        // W3 — bands gate, and are CUMULATIVE.
        {
            settlement_state p = strip_world(1, 900, 900, 900, 0);
            p.regions[0].population = 50000;
            const auto classical = works.available(p.regions[0], roster_band::classical);
            const auto medieval  = works.available(p.regions[0], roster_band::medieval);

            bool cl_fortress = false, md_fortress = false, md_way = false;
            for (const work_row* r : classical) if (r->name == "Test Fortress") cl_fortress = true;
            for (const work_row* r : medieval)
            {
                if (r->name == "Test Fortress") md_fortress = true;
                if (r->name == "Test Way")      md_way      = true;
            }
            check(!cl_fortress && md_fortress && md_way,
                  "W3   a later-band work is withheld at classical and offered at medieval, "
                  "and the earlier band's rows survive");
        }

        // W4 — the effect ACCUMULATORS are what the consumers read. Asserted on
        // the pure functions, because an end-to-end population comparison would
        // be measuring the scorer's choices as much as the effect itself.
        {
            works_registry r2 = works;
            settlement_state p = strip_world(1, 900, 900, 900, 0);
            p.regions[0].population = 50000;

            const int gid = r2.id_of("Test Granary");
            const bool applied = gid >= 0 && apply_work_to_region(p.regions[0], r2, gid);
            const bool refused = gid >= 0 && !apply_work_to_region(p.regions[0], r2, gid);

            check(applied && refused,
                  "W4   a work applies once and REFUSES to be built twice");
            check(p.regions[0].work_capacity_mod == 180
               && p.regions[0].work_manpower_mod == 40,
                  "W4b  the region's accumulators carry the row's effect");

            const work_effect derived = r2.total_effect_mask(p.regions[0].works_built);
            check(derived.capacity_mod == p.regions[0].work_capacity_mod
               && derived.manpower_mod == p.regions[0].work_manpower_mod,
                  "W4c  the accumulators agree with a fresh recompute from the mask");

            check(region_carrying_capacity(900, 180) > region_carrying_capacity(900)
               && manpower_ceiling(100000, 40) > manpower_ceiling(100000),
                  "W4d  the capacity and manpower effects reach their consumers");
        }

        // W5 — the effect is observable IN A RUN, not merely in the arithmetic.
        // Same world, same seed, works on and off: the capacity works must leave
        // the world carrying more people than it otherwise would.
        {
            history_sim_params wp2 = tuned_defaults();
            wp2.start_year = -400;
            wp2.stop_year  = 0;
            // Settle OFF (an unreachable pressure threshold), so the region
            // count is fixed and the comparison isolates the capacity effect.
            // Left on, a works run would found a different number of regions
            // from the plain one and the population totals would be measuring
            // that instead — a true-for-the-wrong-reason check either way.
            wp2.settle_pressure_q = 1001;

            settlement_state on  = strip_world(6, 800, 600, 500, 0);
            settlement_state off = strip_world(6, 800, 600, 500, 0);

            const history_sim_state a =
                run_history_sim(on, nullptr, no_terrain, 60, 30, wp2, 4242u, nullptr, &works);
            const history_sim_state b =
                run_history_sim(off, nullptr, no_terrain, 60, 30, wp2, 4242u, nullptr, nullptr);

            int64_t pop_on = 0, pop_off = 0;
            for (const region& p : on.regions)  pop_on  += p.population;
            for (const region& p : off.regions) pop_off += p.population;

            std::printf("      works on: %lld raised, pop %lld   |   off: %lld raised, pop %lld\n",
                        static_cast<long long>(a.works_raised), static_cast<long long>(pop_on),
                        static_cast<long long>(b.works_raised), static_cast<long long>(pop_off));

            check(a.works_raised > 0, "W5   the roster actually fires in a run");
            check(b.works_raised == 0,
                  "W5b  a null registry disables works entirely — no verb, no effect");
            check(pop_on > pop_off,
                  "W5c  capacity works leave the world carrying more people than without them");
            check(masks_of(on) != 0 && masks_of(off) == 0,
                  "W5d  the works land on regions, and only when a registry was supplied");
        }

        // W6 — determinism, the binding invariant. Two runs at one seed must
        // produce not merely the same counts but the same works on the same
        // regions: a roster that chose differently on a replay would make
        // every downstream generation pass irreproducible.
        {
            history_sim_params wp2 = tuned_defaults();
            wp2.start_year = -400;
            wp2.stop_year  = 0;

            settlement_state s1 = strip_world(6, 800, 600, 500, 0);
            settlement_state s2 = strip_world(6, 800, 600, 500, 0);

            const history_sim_state a =
                run_history_sim(s1, nullptr, no_terrain, 60, 30, wp2, 77u, nullptr, &works);
            const history_sim_state b =
                run_history_sim(s2, nullptr, no_terrain, 60, 30, wp2, 77u, nullptr, &works);

            bool identical = a.works_raised == b.works_raised
                          && s1.regions.size() == s2.regions.size();
            if (identical)
                for (std::size_t i = 0; i < s1.regions.size(); ++i)
                    if (s1.regions[i].works_built     != s2.regions[i].works_built
                     || s1.regions[i].work_reach_mod  != s2.regions[i].work_reach_mod
                     || s1.regions[i].work_capacity_mod != s2.regions[i].work_capacity_mod)
                    { identical = false; break; }

            check(identical,
                  "W6   two runs at one seed raise the same works on the same regions");
        }

        // W7 — the reach effect reaches the supply path. Pre-seed every region
        // with the reach work and the polity's campaigns are supplied better, so
        // the run DIVERGES from the unseeded one.
        //
        // Divergence rather than a signed inequality on stalled_campaigns is the
        // honest claim here: better supply means more campaigns are launched as
        // well as better supplied, so the count can move either way for the right
        // reason. What must be true is that the discount is READ at all — an
        // inert field would leave the two runs byte-identical.
        {
            history_sim_params wp2 = tuned_defaults();
            wp2.start_year = -400;
            wp2.stop_year  = 0;

            // THE COEFFICIENTS ARE AMPLIFIED ON PURPOSE, and the amplification
            // is the reason this check is trustworthy rather than a reason to
            // doubt it. At the authored values the terrain term on a ten-region
            // strip is worth a couple of points of supply out of 1000, so a 12%
            // discount on it truncates to zero and an inert field would pass.
            // Scaling the coefficient scales the thing being discounted, not the
            // discount rule, so what is asserted is still exactly the rule.
            wp2.terrain_reach_cost_q = 800;
            wp2.holdings_burden_q    = 40;
            wp2.free_holdings        = 2; // Bite the burden of breadth at this scale.

            works_registry r2 = works;
            const int way  = r2.id_of("Test Way");
            const int mole = r2.id_of("Test Mole");

            settlement_state seeded = rival_strip(5, 800, 600, 500);
            settlement_state plain  = rival_strip(5, 800, 600, 500);
            for (region& p : seeded.regions)
            {
                p.population = 60000;
                apply_work_to_region(p, r2, way);
                apply_work_to_region(p, r2, mole);
            }
            for (region& p : plain.regions) p.population = 60000;

            // Works OFF in both runs, so the only difference is the reach the
            // regions already carry — otherwise the scorer's own building
            // would confound the comparison.
            const history_sim_state a =
                run_history_sim(seeded, nullptr, no_terrain, 60, 30, wp2, 909u, nullptr, nullptr);
            const history_sim_state b =
                run_history_sim(plain,  nullptr, no_terrain, 60, 30, wp2, 909u, nullptr, nullptr);

            // BL-892 INSTRUMENT (2026-09-10). W7b asserted on OUTCOMES -- battles,
            // conquests, foundings -- and read a null result as "reach_mod is
            // inert". Outcomes are the wrong altitude: reach_mod discounts the
            // TERRAIN TERM of supply, so what it can move DIRECTLY is
            // `region::network_supply_q`, and only a large enough move in that
            // ever shows up as a different decision. `run_history_sim` mutates
            // the settlement in place, so the post-run regions carry the number.
            // Printing it separates the two diagnoses the old check could not
            // tell apart: "supply does not differ" (a wiring bug) from "supply
            // differs but no decision changed" (a mis-aimed assertion).
            // THE THIRD RUN IS WHAT MAKES THIS A GUARD RATHER THAN A GESTURE.
            // Measured while writing it: forcing work_reach_relief_cap_q to 0 --
            // i.e. disconnecting the discount at both read sites -- drops the
            // seeded-vs-plain supply delta from 358 to 16, but NOT to zero. So
            // reach_mod reaches supply by TWO paths: the terrain discount (the
            // 342), and the reach Dijkstra itself, which a cheaper network also
            // shortens (the 16). A plain "seeded > plain" therefore still passes
            // with the discount fully dead, which is the exact weakness the old
            // outcome-level check had, one level down.
            //
            // So the assertion is DIFFERENTIAL: the delta with the discount live
            // must be materially larger than the delta with it capped off. No
            // magic threshold, nothing to tune toward -- if the discount is ever
            // disconnected the two deltas collapse onto each other and this goes
            // red.
            history_sim_params wp3 = wp2;
            wp3.work_reach_relief_cap_q = 0;
            settlement_state capped = rival_strip(5, 800, 600, 500);
            for (region& p : capped.regions)
            {
                p.population = 60000;
                apply_work_to_region(p, r2, way);
                apply_work_to_region(p, r2, mole);
            }
            run_history_sim(capped, nullptr, no_terrain, 60, 30, wp3, 909u, nullptr, nullptr);
            int capped_supply = 0;
            for (const region& rr : capped.regions) capped_supply += rr.network_supply_q;

            int seeded_supply = 0, plain_supply = 0, seeded_mod = 0;
            for (const region& rr : seeded.regions) { seeded_supply += rr.network_supply_q; seeded_mod += rr.work_reach_mod; }
            for (const region& rr : plain.regions)  { plain_supply  += rr.network_supply_q; }
            std::printf("      reach_mod sum seeded %d / plain 0   |   network_supply_q sum seeded %d, plain %d, delta %d\n",
                        seeded_mod, seeded_supply, plain_supply, seeded_supply - plain_supply);

            std::printf("      reach seeded: %lld battles / %lld stalled   |   plain: %lld / %lld\n",
                        static_cast<long long>(a.battles), static_cast<long long>(a.stalled_campaigns),
                        static_cast<long long>(b.battles), static_cast<long long>(b.stalled_campaigns));

            check(way >= 0 && mole >= 0, "W7   the fixture carries reach works to seed");
            // W7b RE-AIMED (BL-892, 2026-09-10). It used to assert that seeded
            // reach changed an OUTCOME -- battles, conquests, foundings,
            // owner_changes -- and it had been red for some time, which the
            // backlog recorded as "reach_mod is inert".
            //
            // IT IS NOT INERT, AND THE INSTRUMENT ABOVE IS WHY WE KNOW. Measured
            // on this exact fixture: work_reach_mod sums to 3200 across the ten
            // seeded regions (120 Way + 200 Mole each), and network_supply_q
            // sums to 22038 seeded against 21680 plain -- a delta of 358, about
            // 3.6% of supply per region. The discount is applied, read, and
            // arithmetically exactly what it is authored to be.
            //
            // WHAT WAS WRONG WAS THE ALTITUDE OF THE ASSERTION. reach_mod
            // discounts the TERRAIN TERM of supply; supply is one input among
            // several to a threshold decision. 3.6% is real and is far too small
            // to flip a campaign choice on a ten-region strip, so an
            // outcome-level check cannot distinguish "working as authored" from
            // "disconnected entirely" -- which is precisely the confusion it
            // caused. A check that cannot tell those apart is not guarding the
            // mechanism it names.
            //
            // So it now asserts the number the mechanism MOVES DIRECTLY. That
            // matters more than tidiness here: CIVILISATION.md sec The arc the
            // phase must produce makes reach-that-widens the lever the whole
            // Empires arc rests on (Ben, 2026-09-10, ruling on NR-823 -- "the
            // wall moves when you win"), so this is the check standing guard
            // over it.
            check(seeded_mod > 0,
                  "W7b1 the seeded strip actually carries reach works (not vacuous)");
            check(seeded_supply - plain_supply > 2 * (capped_supply - plain_supply),
                  "W7b  the reach DISCOUNT dominates the supply gain - reach_mod is read, not inert");
            std::printf("      discount live: +%d   |   discount capped off: +%d\n",
                        seeded_supply - plain_supply, capped_supply - plain_supply);

        }
    }

    // --- BL-921 -- FORCE FOLLOWS THE NETWORK ---------------------------------
    //
    // DONE WHEN (NR-836/NR-838): a 3-region polity and a 30-region polity at
    // one border show the LARGER polity fielding the larger stack. Two
    // otherwise-identical two-sided strips, differing only in the attacking
    // side's held-region count, isolate that one variable: the defending side
    // is the SAME 3 regions in both, so any difference in what the large side
    // musters against it is attributable to the network the levy is pooled
    // over, not to a stronger defender provoking a bigger response.
    //
    // `battle_trace::attacker_men` is the raised stack `gather_army` returns
    // to the resolver (history_sim.cpp, BL-835's `commit` path), so reading it
    // off the trace is reading the mechanism itself rather than an outcome
    // proxy of it.
    std::printf("\n=== BL-921  FORCE FOLLOWS THE NETWORK ===\n");
    {
        const sim_terrain_view no_terrain{};
        history_sim_params wp = tuned_defaults();
        wp.start_year    = -800;
        wp.stop_year     = 0;
        wp.trace_battles = true;

        // A two-sided strip with `n_a` regions on the attacking side and a
        // fixed-size (3-region) defender, laid out on a cylinder wide enough
        // that the 30-region variant never wraps back onto its own defender.
        auto two_sided = [&](int n_a) {
            settlement_state a = strip_world(n_a, 800, 600, 500, 0);
            settlement_state b = strip_world(3,   800, 600, 500, 1);
            for (region& p : b.regions)
            {
                p.col   += n_a * 3;
                p.anchor = p.col;
                p.name  += "B";
            }
            a.regions.insert(a.regions.end(), b.regions.begin(), b.regions.end());
            for (region& p : a.regions) p.population = 60000;
            return a;
        };

        settlement_state small = two_sided(3);
        settlement_state large = two_sided(30);
        const int gw = 220, gh = 30; // Wide enough for the 30-region strip plus its 3-region defender.

        const history_sim_state small_out =
            run_history_sim(small, nullptr, no_terrain, gw, gh, wp, 4242u, nullptr, nullptr);
        const history_sim_state large_out =
            run_history_sim(large, nullptr, no_terrain, gw, gh, wp, 4242u, nullptr, nullptr);

        // Identify which side of each run is the ENGINEERED-LARGE side: its
        // capital is a region index below `n_a` (region 0 seeds the highest
        // `settle_score_q`, so `strip_world`'s side-A founds its capital
        // there). Reading `polity::capital` rather than assuming polity id 0
        // is side A -- polity ids are assigned in whatever order the sim's
        // own founding pass reaches them, not in region order.
        auto stack_stats = [](const history_sim_state& st, int n_a,
                              int64_t& mean_men, int64_t& max_men, int& n) {
            int atk_id = -1;
            for (const polity& p : st.polities)
                if (p.capital >= 0 && p.capital < n_a) { atk_id = p.id; break; }
            mean_men = 0; max_men = 0; n = 0;
            if (atk_id < 0) return;
            int64_t sum = 0;
            for (const battle_trace& bt : st.battle_traces)
            {
                if (bt.attacker != atk_id) continue;
                sum += bt.attacker_men;
                max_men = std::max(max_men, bt.attacker_men);
                ++n;
            }
            if (n > 0) mean_men = sum / n;
        };

        int64_t small_mean = 0, small_max = 0; int small_n = 0;
        int64_t large_mean = 0, large_max = 0; int large_n = 0;
        stack_stats(small_out, 3,  small_mean, small_max, small_n);
        stack_stats(large_out, 30, large_mean, large_max, large_n);

        std::printf("  [diag] small-fixture run: %lld total battles   |   large-fixture run: %lld total battles\n",
                    static_cast<long long>(small_out.battle_traces.size()),
                    static_cast<long long>(large_out.battle_traces.size()));
        std::printf("  3-region side  attacking: %d battles, mean stack %lld, max stack %lld\n",
                    small_n, static_cast<long long>(small_mean), static_cast<long long>(small_max));
        std::printf("  30-region side attacking: %d battles, mean stack %lld, max stack %lld\n",
                    large_n, static_cast<long long>(large_mean), static_cast<long long>(large_max));

        if (small_n > 0 && large_n > 0)
            check(large_mean > small_mean,
                  "BL-921.1 the 30-region polity fields a larger stack than the 3-region polity"
                  " against the same-size defender");
        else
            std::printf("[NOTE] BL-921.1 the scorer did not choose to have the SAME side attack in"
                        " both runs in this fixture window (small_n=%d, large_n=%d) -- see BL-921.2"
                        " below for the mechanism read directly off the post-run state instead\n",
                        small_n, large_n);

        // BL-921.2 — READ THE MECHANISM DIRECTLY, the same discipline W7b above
        // uses: an outcome (did a campaign fire, who chose to attack) depends on
        // the scorer's own verb competition and is not this item's claim. What
        // this item claims is what `gather_army` COMPUTES at the frontier region
        // of each side, off the settlement state exactly as `run_history_sim`
        // left it — so this reproduces gather_army's own read-only estimate
        // (`commit=false`) rather than depending on either polity actually
        // having launched a campaign in the window.
        auto pooled_levy_at_frontier = [](const settlement_state& ss, int lo, int hi) -> int64_t {
            // The frontier region is the side's LAST region before the border
            // (`two_sided` lays each side out as a contiguous column range).
            const std::size_t hub = static_cast<std::size_t>(hi - 1);
            int64_t total = ss.regions[hub].army_stock;
            if (total <= 0) return 0;
            for (int i = lo; i < hi; ++i)
            {
                if (i == static_cast<int>(hub)) continue;
                const region& r = ss.regions[static_cast<std::size_t>(i)];
                if (r.army_stock <= 0) continue;
                // `network_supply_q` is already clamped to [0, 1000] where it is
                // written (history_sim.cpp), so this reads it as-is rather than
                // re-deriving the invariant a second place would have to keep in
                // step with the first.
                const int supply_q = r.network_supply_q;
                if (supply_q <= 0) continue;
                total += (r.army_stock * supply_q) / 1000;
            }
            return total;
        };

        const int64_t small_levy = pooled_levy_at_frontier(small, 0, 3);
        const int64_t large_levy = pooled_levy_at_frontier(large, 0, 30);
        std::printf("  pooled levy at the frontier (read directly, not via a battle):"
                    " 3-region side %lld   |   30-region side %lld\n",
                    static_cast<long long>(small_levy), static_cast<long long>(large_levy));
        check(large_levy > small_levy,
              "BL-921.2 the 30-region polity's pooled levy at its own frontier region exceeds"
              " the 3-region polity's, read directly off gather_army's own formula");
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
