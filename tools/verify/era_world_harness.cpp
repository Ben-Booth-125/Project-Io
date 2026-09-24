// ---------------------------------------------------------------------------
// Headless Era -1 world harness (BL-271 first slice; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Generates the canonical world with world_params::epoch_year = 0 — the 0 CE
// antiquity start — and both VERIFIES the stop and PRINTS the world, because
// "you cannot look at a screenshot and see whether relations are interesting"
// (DEVLOG 2026-08-04 handoff: build the instrument before the feature).
//
//   R1  THE STOP HOLDS. The settlement pass hands the era nothing founded
//       after 0 CE; the finished world founds nothing past the close of the
//       last span generation ran (1200 Empires / 1660 Exploration); none is
//       industrialised, and the median industrial year is 0 (nobody).
//   R2  DEMOGRAPHY IS SEEDED. Every region holds population in
//       (0, carrying_capacity]; manpower sits within its ceiling. The 1960
//       settlement pass hands the era an unseeded table and the era seeds it
//       (graduation is the era's job on both arcs, not the pass's).
//   R3  MULTIPOLAR BY CONSTRUCTION. More than one nation exists at 0 CE —
//       the precondition for BL-309's two-great-powers seed to mean anything.
//   R4  DETERMINISM. Two epoch-0 generations produce byte-identical region
//       tables (the settlement pass's whole deterministic surface).
//   R5  THE 1960 ARC IS UNTOUCHED. An explicit epoch_year = 1960 world still
//       runs Stage 4's endowment gate, lights a furnace somewhere across a
//       four-world sweep, and holds at least as many regions as the 0 CE
//       world.
//
// KNOWN BASELINE (BL-1010, 2026-09-16) — ONE RED IS EXPECTED ON MAIN:
//   R5 "the 1960 arc still industrialises (reachable across the seed sweep)".
//   Cause: no living polity's capacity passes 4 in any domain by 1960, and the
//   Industrial rung needs 5, so no furnace lights on any swept seed. A world
//   defect or a tuning wall, not a harness one; fixing it moves the 1960 world
//   and is Ben's call (NR-807 made the industrial arc the live product). Also
//   listed in DEVELOPMENT_PRACTICES.md § Known harness baselines. Any OTHER
//   red is a regression.
//   (R1 and R2 went red for a different reason — the checks were stale, not
//   the code — and were restated in place; see their comments.)
//   R7  THE 1660 TREASURIES CROSS THE FOLD (BL-975). Every nation's starting
//       treasury is its folded polities' Exploration-span chest through the
//       one per-mille NATION_GENERATION.md § Pass 7 names; a nation folded
//       from a richer polity starts richer, and garrisons differentiate on it.
//
// Exits non-zero on any FAIL. Links the generation TU superset (as
// world_audit / determinism_harness).

#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/logistics.hpp"
#include "world/nation_generation.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

const generation_report::body_entry* kepler_entry(const generation_report& rep)
{
    for (const auto& be : rep.bodies)
        if (be.is_homeworld) return &be; // identity, not display name (BL-257)
    return nullptr;
}

bool regions_identical(const std::vector<region>& a, const std::vector<region>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const region& x = a[i];
        const region& y = b[i];
        if (x.anchor != y.anchor || x.name != y.name || x.culture != y.culture // BL-826: culture_shares::operator==
            || x.founded_year != y.founded_year || x.industrial_year != y.industrial_year
            || x.population != y.population || x.manpower_stock != y.manpower_stock
            || x.farm_q != y.farm_q || x.ore_q != y.ore_q || x.energy_q != y.energy_q
            || x.port_q != y.port_q || x.nation != y.nation
            // BL-766: the urban record is sim output too, so R4's determinism
            // claim has to cover it or a non-deterministic city map passes.
            || x.centres != y.centres || x.centres_razed != y.centres_razed
            || x.urban_population != y.urban_population)
            return false;
    }
    return true;
}

} // namespace

int main()
{
    // --- The 0 CE world (twice, for R4) ------------------------------------
    world_params antiq{};
    antiq.epoch_year = 0;

    generation_report rep_a{}, rep_b{}, rep_1960{};
    era_minus_one_fixture fx_a{};
    world wa = make_hard_coded_world(antiq, &rep_a, {}, nullptr, nullptr, &fx_a);
    world wb = make_hard_coded_world(antiq, &rep_b);

    // R5's control world. NOT `{}` any more: the default epoch became 0 with the
    // ancient refocus (NR-177), so default params now generate the SAME world as
    // `antiq` and every R5 check would fail by construction rather than by
    // regression. The 1960 arc is still reachable — it just has to be asked for.
    world_params modern{};
    modern.epoch_year = 1960;
    // The fixture is asked for so R2 can read the table the settlement pass
    // HANDED the era, before the sim seeded it (BL-1010).
    era_minus_one_fixture fx_60{};
    world w60 = make_hard_coded_world(modern, &rep_1960, {}, nullptr, nullptr, &fx_60);

    const auto* ka = kepler_entry(rep_a);
    const auto* kb = kepler_entry(rep_b);
    const auto* k60 = kepler_entry(rep_1960);
    check(ka && kb && k60, "Kepler entry present in all three reports");
    if (!ka || !kb || !k60) return 1;

    const std::vector<region>& ps = ka->settlement.regions;
    const std::vector<region>& ps60 = k60->settlement.regions;

    // --- R1: the stop holds -------------------------------------------------
    //
    // RESTATED 2026-09-16 (BL-1010), because the check had gone stale, not the
    // code. It asserted `founded_year <= 0` over `ps` — the report's region
    // table, which is the table AFTER the era ran. When this harness was
    // written the era stopped at the epoch, so the two readings coincided.
    // They no longer do: the Empires round closes at `empires_stop_year` (1200,
    // BL-906, Ben 2026-09-11), the Exploration span runs on to
    // `exploration_stop_year` (1660, BL-946), and both FOUND ground as they
    // play (a Settle's daughter carries `founded_year = y`). Measured on seed 0:
    // foundings run -2400 -> 1656, and every one dated after 0 CE is a sim
    // founding inside a span the design says runs past the epoch. The arc is
    // superseded as the product (Ben, NR-807, 2026-09-16) but generation still
    // runs it, so the stop is still worth holding — at the two places it now
    // lives:
    //   R1a  THE SETTLEMENT PASS STOPS AT THE EPOCH. What `run_settlement`
    //        handed the era — the regions it placed AND the founding schedule
    //        (BL-846) — holds nothing dated after 0 CE. This is BL-271's own
    //        claim, read off the fixture, which captures the table before the
    //        sim mutates it.
    //   R1b  THE RUN FOUNDS NOTHING PAST ITS OWN CLOSE. No region in the
    //        finished world is dated after the last span generation ran.
    // BL-1044: the Industrialisation span runs by default, so the last close is its
    // 1960 one where it ran — and it is the span that lights furnaces (the
    // Industry tree opens at its own open year, BL-1038), so "no furnace" is
    // held up to that open year rather than for the whole run.
    bool all_founded = !ps.empty(), none_industrial = true;
    int64_t last_close = fx_a.params.stop_year;
    if (fx_a.exploration_ran) last_close = fx_a.exploration_params.stop_year;
    if (fx_a.industrialisation_ran) last_close = fx_a.industrialisation_params.stop_year;
    const int64_t furnaces_open = fx_a.industrialisation_ran ? fx_a.industrialisation_params.industry_open_year
                                                        : INT64_MAX;
    for (const region& p : ps)
    {
        if (p.founded_year > last_close) all_founded = false;
        if ((p.industrialised || p.industrial_year != 0) && p.industrial_year < furnaces_open)
            none_industrial = false;
    }
    bool handed_by_epoch = fx_a.ran && !fx_a.settlement.regions.empty();
    int64_t handed_latest = INT64_MIN;
    for (const region& p : fx_a.settlement.regions)
        handed_latest = std::max(handed_latest, p.founded_year);
    for (const region& p : fx_a.settlement.pending_foundings)
        handed_latest = std::max(handed_latest, p.founded_year);
    if (handed_latest > antiq.epoch_year) handed_by_epoch = false;
    std::printf("  R1: settlement handed the era %zu regions + %zu scheduled, latest founded %" PRId64
                "; migration ended %" PRId64 "; the run closed at %" PRId64 "\n",
                fx_a.settlement.regions.size(), fx_a.settlement.pending_foundings.size(),
                handed_latest, fx_a.settlement.migration_end_year, last_close);
    check(handed_by_epoch, "R1a the settlement pass hands the era nothing founded after 0 CE");
    check(all_founded, "R1b no 0 CE-arc region is founded after the last span generation ran");
    check(none_industrial, "R1 no 0 CE region lit a furnace before the Industry tree opened "
                           "(the Industrialisation span's own open, BL-1038)");
    check(ka->settlement.median_industrial_year == 0
              || ka->settlement.median_industrial_year >= furnaces_open,
          "R1 median industrial year is 0 (nobody) or inside the Industrialisation span");

    // --- R2: demography seeded ----------------------------------------------
    bool pop_ok = !ps.empty(), mp_ok = true;
    int64_t total_pop = 0, total_mp = 0;
    for (const region& p : ps)
    {
        const int64_t cap = region_carrying_capacity(p.farm_q);
        if (p.population <= 0 || p.population > cap) pop_ok = false;
        if (p.manpower_stock < 0 || p.manpower_stock > manpower_ceiling(p.population)) mp_ok = false;
        total_pop += p.population;
        total_mp += p.manpower_stock;
    }
    check(pop_ok, "R2 every 0 CE region holds population within (0, capacity]");
    check(mp_ok, "R2 manpower sits within its population ceiling");
    // RESTATED 2026-09-16 (BL-1010): the check was stale, not the code. It
    // asserted every region of the FINISHED 1960 world holds zero people. That
    // was true while the era ran only below 1700; since the two-span arc
    // (BL-747, Ben 2026-09-03) a 1960 world runs the SAME sim, and the sim's
    // opening seeds a headcount into every region the pass left at zero
    // (history_sim.cpp, "the graduation path settlement.hpp's demography note
    // leaves to this item"). So graduation is not the SETTLEMENT PASS's job on
    // the 1960 arc — it is the era's, on both arcs. Measured on seed 0: the
    // pass hands the era 206 regions, all at zero; the finished world holds 667,
    // none at zero. The two halves are asserted where each now lives.
    bool unseeded_1960 = fx_60.ran && !fx_60.settlement.regions.empty();
    for (const region& p : fx_60.settlement.regions)
        if (p.population != 0) unseeded_1960 = false;
    check(unseeded_1960, "R2 the 1960 settlement pass hands the era an unseeded table (graduation not its path)");
    bool seeded_1960 = !ps60.empty();
    for (const region& p : ps60)
        if (p.population <= 0 || p.population > region_carrying_capacity(p.farm_q)) seeded_1960 = false;
    check(seeded_1960, "R2 the era seeds it: every 1960 region holds population within (0, capacity]");

    // --- R3: multipolar ------------------------------------------------------
    check(wa.nations.size() > 1, "R3 more than one nation at 0 CE");

    // --- R4: determinism -----------------------------------------------------
    check(regions_identical(ps, kb->settlement.regions),
          "R4 two 0 CE generations produce identical region tables");

    // --- R5: the 1960 arc untouched ------------------------------------------
    // RESTATED 2026-09-16 (BL-1010) — AND STILL RED, AS A NAMED BASELINE.
    //
    // WHAT WENT STALE. The check asserted that THIS ONE world lights a furnace.
    // When it was written Stage 4 DATED every endowed region's furnace inside
    // `run_settlement`, so a 1960 world industrialised by construction. BL-748
    // moved the date into the run: a region lights only once its polity's
    // MATERIALS capacity crosses the Industrial rung, and docs/lore/HISTORY.md
    // § Stage 4 says "a polity that never climbs never lights, and that is a
    // legitimate world". So at one seed the outcome is a reading, not an
    // invariant — and the claim worth failing on is R6d's shape: a SWEEP in
    // which no furnace ever lights means the mechanism is dead.
    //
    // WHY IT IS STILL RED (baseline, recorded in DEVELOPMENT_PRACTICES.md §
    // Known harness baselines). Measured 2026-09-16 at epoch 1960. This sweep
    // (default inputs): 22 / 10 / 36 / 13 regions carry a furnace lag, 0 of 4
    // worlds light one. A one-off probe re-running the era off the fixture read
    // WHY — on seed 0 under default inputs, and on seeds 0-3 under the app's
    // shipped world_gen.lua + works.lua: no living polity's capacity passes 4 in
    // ANY of the seven domains, and the Industrial rung needs 5
    // (`roster_band_for_capacity`). Nobody crosses, so nothing lights. Not fixed here: any fix moves the 1960 world, and whether the
    // industrial arc must industrialise by its epoch is Ben's call now that it
    // is the live product (NR-807).
    int lag_regions_1960 = 0, industrial_regions_1960 = 0;
    for (const region& p : ps60)
    {
        if (p.industrial_lag_years >= 0) ++lag_regions_1960;
        if (p.industrialised) ++industrial_regions_1960;
    }
    std::printf("  R5: 1960 seed 0 — %d regions carry a furnace lag, %d industrialised, median furnace year %" PRId64 "\n",
                lag_regions_1960, industrial_regions_1960, k60->settlement.median_industrial_year);
    check(lag_regions_1960 > 0, "R5 the 1960 arc still runs Stage 4's endowment gate (ground carries a furnace lag)");
    {
        int worlds_lit = industrial_regions_1960 > 0 ? 1 : 0;
        for (std::uint32_t s = 1; s < 4; ++s)
        {
            world_params sp = modern;
            sp.seed = s * 0x9E3779B1u; // R6d's seeds, so the two sweeps read the same worlds
            generation_report rs{};
            make_hard_coded_world(sp, &rs);
            const auto* ks = kepler_entry(rs);
            int lag = 0, lit = 0;
            if (ks != nullptr)
                for (const region& p : ks->settlement.regions)
                {
                    if (p.industrial_lag_years >= 0) ++lag;
                    if (p.industrialised) ++lit;
                }
            std::printf("  R5: 1960 seed %u — %d regions carry a furnace lag, %d industrialised\n", s, lag, lit);
            if (lit > 0) ++worlds_lit;
        }
        std::printf("  R5: a furnace lit in %d of 4 worlds at epoch 1960\n", worlds_lit);
        check(worlds_lit > 0,
              "R5 the 1960 arc still industrialises (reachable across the seed sweep) [BASELINE: no polity passes capacity 4]");
    }
    // R5's region-count comparison is against the SETTLEMENT PASS's 0 CE
    // output, not against `ps`. Since the year-tick sim was wired into
    // generation (2026-08-12) the 0 CE world keeps founding regions for 400
    // years after the settlement pass stops — 1107 of them on a measured run —
    // so `ps` is no longer "what had been settled by year 0" and comparing 1960
    // against it asks the wrong question. The claim worth checking is unchanged:
    // the 1960 arc settles at least as much ground as the ancient stop did.
    world_params antiq_settled = antiq;
    antiq_settled.prehistory_years = 0;
    generation_report rep_settled{};
    make_hard_coded_world(antiq_settled, &rep_settled);
    const auto* k_settled = kepler_entry(rep_settled);
    check(k_settled != nullptr, "R5 the settlement-only 0 CE control generated");
    if (k_settled != nullptr)
        check(ps60.size() >= k_settled->settlement.regions.size(),
              "R5 the 1960 world holds at least as many regions as the 0 CE settlement pass");

    // --- R6: roads and markets FROM the history (BL-768) --------------------
    //
    // THE ONLY PLACE THIS CAN BE ASSERTED. Every road and market census in the
    // project declares `no_prehistory()` — correct for their own subjects, and
    // structurally blind to this one: with no era there are no corridors, so the
    // ancient stamp is a no-op and the market carve's trade term never fires.
    // This harness generates the era-ON world by construction, so the check
    // belongs here rather than beside the passes it measures.
    //
    // Four claims, in the order the phase produces them:
    //   R6a  the history RECORDED where it walked;
    //   R6b  those lines MET somewhere — a network with no junction is a set of
    //        unconnected spokes, and the market term would have nothing to read;
    //   R6c  the ancient roads are ON THE GROUND, not merely recorded;
    //   R6d  a market actually emerges from trade — over a SEED SWEEP, not at one
    //        seed.
    //
    // R6d IS A REACHABILITY CLAIM, and deliberately so: whether a junction
    // happens to hold a centre its nation's own gate would have refused is
    // spatial luck at any one world, exactly as road_generation_harness argues
    // for the Highway tier. A world where trade opened no market is a legitimate
    // outcome (§ Asymmetry is the deliverable, and the five calls' "tune the
    // forces, never the outcome"); a SWEEP where it never happens would mean the
    // term is dead, which is the thing worth failing on.
    {
        std::printf("\n--- R6  roads and markets from the history (BL-768) ---\n");
        std::printf("  seed 0: corridors %" PRId64 "  junctions (degree >= 3) %" PRId64
                    "  markets opened by trade %" PRId64 "\n",
                    rep_a.prehistory_corridors, rep_a.prehistory_junctions,
                    rep_a.markets_from_trade);
        check(rep_a.prehistory_corridors > 0,
              "R6a the era recorded the corridors it supplied and settled along");
        check(rep_a.prehistory_junctions > 0,
              "R6b those corridors MEET somewhere — there are trade junctions to carve on");
        check(rep_a.prehistory_corridors == rep_b.prehistory_corridors
              && rep_a.prehistory_junctions == rep_b.prehistory_junctions
              && rep_a.markets_from_trade == rep_b.markets_from_trade,
              "R6 the record is deterministic across two generations");

        int roaded = 0;
        for (const auto& [tid, tc] : wa.tiles)
            if (tc.body == wa.home_body && tc.road_level > 0 && !is_water(tc.substrate))
                ++roaded;
        std::printf("  roaded land tiles on the 0 CE world: %d\n", roaded);
        check(roaded > 0, "R6c the ancient world opens with roads on the ground");

        int seeds_with_trade_market = 0, trade_markets = 0;
        std::int64_t seed0_trade = rep_a.markets_from_trade;
        for (std::uint32_t s = 1; s < 4; ++s)
        {
            world_params sp{};
            sp.epoch_year = 0;
            sp.seed = s * 0x9E3779B1u;
            generation_report rs{};
            make_hard_coded_world(sp, &rs);
            std::printf("  seed %u: corridors %" PRId64 "  junctions %" PRId64
                        "  markets opened by trade %" PRId64 "\n",
                        s, rs.prehistory_corridors, rs.prehistory_junctions,
                        rs.markets_from_trade);
            if (rs.markets_from_trade > 0) ++seeds_with_trade_market;
            trade_markets += static_cast<int>(rs.markets_from_trade);
        }
        if (seed0_trade > 0) { ++seeds_with_trade_market; trade_markets += static_cast<int>(seed0_trade); }
        std::printf("  markets opened by trade: %d across %d of 4 worlds\n",
                    trade_markets, seeds_with_trade_market);
        check(seeds_with_trade_market > 0,
              "R6d a market emerges where trade concentrated (reachable across the seed sweep)");
    }

    // --- R7: the 1660 treasuries cross the fold (BL-975) ---------------------
    //
    // Derived INDEPENDENTLY of generate_nations' own arithmetic, by the rule
    // NATION_GENERATION.md § Pass 7 states: a polity's chest is the sum of
    // `region::treasury` over the ground flying its flag at 1660 (read off the
    // fixture's handoff struct, where `region::nation` is still the POLITY id),
    // and it lands on the nation that polity FOLDED INTO — the nation holding
    // the polity's lowest-indexed land-anchored region, which is the seed the
    // fold represents it by. A water-seated polity (its capital is a coastal-
    // water region whose tile no nation holds) is why this walks the polity's
    // regions rather than reading its capital tile: seed 0 has one, and its
    // chest belongs to the realm its land became, not to nobody.
    // Converted through the one stated per-mille, read off nation_params'
    // default and never restated here.
    {
        std::vector<entity_id> nids;
        for (const auto& kv : wa.nations) nids.push_back(kv.first);
        std::sort(nids.begin(), nids.end());
        const double per_mille = static_cast<double>(nation_params{}.treasury_credit_per_mille);
        const double floor_v   = static_cast<double>(nation_params{}.treasury_floor);

        // BL-1044: the chest is read off the LAST close that ran — the
        // Industrialisation span's 1960 fold where it ran (setup credits it since
        // BL-1053), else Exploration's 1660 one.
        const std::vector<region>& hx = fx_a.industrialisation_ran ? fx_a.industrialisation_handoff.regions
                                                              : fx_a.exploration_handoff.regions;
        check(fx_a.exploration_ran && hx.size() == ps.size(),
              "R7 the last span ran and its handoff table is the report's region table");

        // Polity id -> (chest, nation index it folded into).
        int max_polity = -1;
        for (const region& r : hx) max_polity = std::max(max_polity, r.nation);
        std::vector<int64_t> polity_chest(static_cast<std::size_t>(max_polity + 1), 0);
        std::vector<int>     polity_nation(static_cast<std::size_t>(max_polity + 1), -1);
        const std::vector<entity_id>& grid = body_tile_grid(wa, ka->id);
        for (std::size_t i = 0; i < hx.size(); ++i)
        {
            const int pol = hx[i].nation;
            if (pol < 0) continue;
            if (hx[i].treasury > 0) polity_chest[static_cast<std::size_t>(pol)] += hx[i].treasury;
            if (polity_nation[static_cast<std::size_t>(pol)] >= 0) continue;
            if (hx[i].domain != region_domain::land) continue;
            if (hx[i].anchor < 0 || static_cast<std::size_t>(hx[i].anchor) >= grid.size()) continue;
            const auto tn = wa.tile_to_nation.find(grid[static_cast<std::size_t>(hx[i].anchor)]);
            if (tn == wa.tile_to_nation.end()) continue;
            const auto it = std::lower_bound(nids.begin(), nids.end(), tn->second);
            if (it != nids.end() && *it == tn->second)
                polity_nation[static_cast<std::size_t>(pol)] = static_cast<int>(it - nids.begin());
        }
        std::vector<int64_t> chest(nids.size(), 0);
        int folded_polities = 0, unplaced_chests = 0;
        for (int pol = 0; pol <= max_polity; ++pol)
        {
            const int ni = polity_nation[static_cast<std::size_t>(pol)];
            if (ni < 0)
            {
                if (polity_chest[static_cast<std::size_t>(pol)] > 0) ++unplaced_chests;
                continue;
            }
            ++folded_polities;
            chest[static_cast<std::size_t>(ni)] += polity_chest[static_cast<std::size_t>(pol)];
        }

        std::vector<double> actual(nids.size()), expected(nids.size());
        double max_abs_err = 0.0;
        int    unfolded    = 0; // nations no polity's chest reached
        for (std::size_t i = 0; i < nids.size(); ++i)
        {
            actual[i]   = static_cast<double>(wa.nations.at(nids[i]).treasury);
            expected[i] = static_cast<double>(chest[i]) * per_mille / 1000.0;
            max_abs_err = std::max(max_abs_err, std::fabs(actual[i] - expected[i]));
            if (chest[i] == 0) ++unfolded;
        }

        // "A nation folded from a richer polity starts richer": every ordered
        // pair, not just the top.
        int pairs = 0, agree = 0;
        std::size_t richest_expected = 0, richest_actual = 0;
        for (std::size_t a = 0; a < nids.size(); ++a)
        {
            if (expected[a] > expected[richest_expected]) richest_expected = a;
            if (actual[a]   > actual[richest_actual])     richest_actual   = a;
            for (std::size_t b = 0; b < nids.size(); ++b)
            {
                if (expected[a] <= expected[b]) continue;
                ++pairs;
                if (actual[a] > actual[b]) ++agree;
            }
        }

        std::vector<double> sorted = actual;
        std::sort(sorted.begin(), sorted.end());
        const double median = sorted.empty() ? 0.0 : sorted[sorted.size() / 2];
        int on_floor = 0;
        for (double v : actual) if (v <= floor_v) ++on_floor;

        // Garrisons: units a NATION owns. Every garrison of one nation is
        // sized alike (seed_nation_garrisons sizes per nation, then places one
        // per target province), so the PER-UNIT count is the treasury-scaled
        // figure; a total headcount would also count how many provinces it
        // garrisons, which is a border fact, not a treasury one.
        std::vector<int64_t> garrison(nids.size(), 0);
        for (const auto& [uid, u] : wa.units)
        {
            (void)uid;
            const auto it = std::lower_bound(nids.begin(), nids.end(), u.owner);
            if (it != nids.end() && *it == u.owner)
            {
                int64_t& g = garrison[static_cast<std::size_t>(it - nids.begin())];
                g = std::max<int64_t>(g, u.count);
            }
        }
        std::vector<int64_t> gs = garrison;
        std::sort(gs.begin(), gs.end());
        const int64_t distinct_garrisons =
            static_cast<int64_t>(std::unique(gs.begin(), gs.end()) - gs.begin());
        int garrison_pairs = 0, garrison_agree = 0;
        for (std::size_t a = 0; a < nids.size(); ++a)
            for (std::size_t b = 0; b < nids.size(); ++b)
            {
                if (actual[a] <= actual[b]) continue;
                ++garrison_pairs;
                if (garrison[a] >= garrison[b]) ++garrison_agree;
            }

        std::printf("\nR7 starting treasuries (BL-975): %zu nations from %d folded polities (%d chests unplaced), per-mille %.3f, floor %.1f cr\n",
                    nids.size(), folded_polities, unplaced_chests, per_mille, floor_v);
        std::printf("  min %.2f  median %.2f  max %.2f cr; %d on the floor (%d with no folded chest)\n",
                    sorted.empty() ? 0.0 : sorted.front(), median,
                    sorted.empty() ? 0.0 : sorted.back(), on_floor, unfolded);
        std::printf("  max |actual - chest x per-mille| = %.4f cr; richer-polity pairs agreeing %d of %d\n",
                    max_abs_err, agree, pairs);
        std::printf("  garrisons: %" PRId64 " distinct sizes across %zu nations; richer-nation pairs with >= garrison %d of %d\n",
                    distinct_garrisons, nids.size(), garrison_agree, garrison_pairs);
        std::printf("  %-30s %12s %12s %8s\n", "nation", "treasury cr", "chest", "garrison");
        {
            std::vector<std::size_t> order(nids.size());
            for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
            std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
                return actual[a] != actual[b] ? actual[a] > actual[b] : nids[a] < nids[b];
            });
            for (std::size_t k = 0; k < order.size(); ++k)
            {
                const std::size_t i = order[k];
                std::printf("  %-30s %12.2f %12" PRId64 " %8" PRId64 "\n",
                            wa.nations.at(nids[i]).name.c_str(), actual[i], chest[i], garrison[i]);
            }
        }

        check(max_abs_err < 0.5, "R7a every nation's starting treasury is its folded polities' chest at the last close (1960 with the span on) through the one per-mille");
        check(pairs > 0 && agree == pairs, "R7b a nation folded from a richer polity starts richer (every pair)");
        check(richest_expected == richest_actual, "R7c the richest polity at the last close folds into the richest nation");
        check(sorted.size() > 1 && sorted.back() > median && median > floor_v,
              "R7d treasuries spread: the median nation is off the floor and the top is above it");
        check(distinct_garrisons > 1, "R7e garrisons differentiate: more than one garrison size");
        check(garrison_pairs > 0 && garrison_agree == garrison_pairs,
              "R7f a richer nation never garrisons fewer than a poorer one");
    }

    // --- The dossier ---------------------------------------------------------
    std::printf("\n=== KEPLER AT 0 CE ===\n");
    // Signed difference deliberately: the 0 CE world now out-founds the 1960 one
    // (the year-tick sim keeps settling for 400 years), so an unsigned subtraction
    // here wrapped to a nonsense number.
    std::printf("regions: %zu at 0 CE (the 1960 arc reaches %zu — a difference of %" PRId64 ")\n",
                ps.size(), ps60.size(),
                static_cast<int64_t>(ps60.size()) - static_cast<int64_t>(ps.size()));
    std::printf("nations:   %zu\n", wa.nations.size());
    std::printf("people:    %" PRId64 " across all regions; %" PRId64 " recruitable under arms\n",
                total_pop, total_mp);

    int64_t oldest = 0, youngest = -100000;
    for (const region& p : ps)
    {
        oldest = std::min(oldest, p.founded_year);
        youngest = std::max(youngest, p.founded_year);
    }
    std::printf("founding:  %" PRId64 " to %" PRId64 " (negative = years before epoch)\n\n", oldest, youngest);

    std::vector<const region*> by_pop;
    for (const region& p : ps) by_pop.push_back(&p);
    std::sort(by_pop.begin(), by_pop.end(), [](const region* a, const region* b) {
        return a->population != b->population ? a->population > b->population : a->name < b->name;
    });
    std::printf("%-34s %10s %9s %8s %7s\n", "region", "population", "manpower", "founded", "farm_q");
    const std::size_t shown = std::min<std::size_t>(12, by_pop.size());
    for (std::size_t i = 0; i < shown; ++i)
    {
        const region& p = *by_pop[i];
        std::printf("%-34s %10" PRId64 " %9" PRId64 " %8" PRId64 " %7d\n",
                    p.name.c_str(), p.population, p.manpower_stock, p.founded_year, p.farm_q);
    }

    std::printf("\nnations at 0 CE:\n");
    std::vector<std::pair<std::string, std::size_t>> nations;
    for (const auto& [id, n] : wa.nations) nations.push_back({ n.name, n.tiles.size() });
    std::sort(nations.begin(), nations.end(),
              [](const auto& a, const auto& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
    for (const auto& [name, tiles] : nations)
        std::printf("  %-30s %zu tiles\n", name.c_str(), tiles);

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
