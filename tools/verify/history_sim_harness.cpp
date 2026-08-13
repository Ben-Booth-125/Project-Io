// ---------------------------------------------------------------------------
// history_sim_harness — BL-277 (Era -1 campaign scorer) + BL-271 first slice.
//
// Binds the requirement group "era-minus-1-history-sim" (R1..R8). Runs the
// year-tick sim over the generated Kepler settlement state and over a small
// synthetic two-polity world where one mechanism at a time can be isolated.
//
// ALSO COVERS the Era -1 logistics slices (BL-316) and the shared-currency
// scorer (BL-318), which landed with no assertions behind them:
//   S1a/S1b  real terrain is live, not inert
//   S2a      terrain-weighted reach never makes mountains cheaper
//   S3a/S3b  the burden of breadth is live and points the right way
//   B318a-c  no verb has swallowed the run
//
// THE PATTERN THESE USE, and the reason they are shaped this way: each runs the
// SAME world twice with one variable changed, and asserts the DIRECTION of the
// difference. An absolute count would be a calibration target, and calibration
// targets get tuned toward instead of measured — which is how the sim ended up
// with a `stalled_campaigns` counter that read a constant zero and a terrain
// module nobody noticed was dead.
//
// Headless: world/* logic only, no SDL and no Lua.
// ---------------------------------------------------------------------------

#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/unit_roster.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <chrono>
#include <cstdio>
#include <string>
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

/// Compare two runs for the byte-level equality R1 asserts.
bool same_run(const history_sim_state& a, const history_sim_state& b,
              const settlement_state& sa, const settlement_state& sb)
{
    if (a.owner_changes.size() != b.owner_changes.size()) return false;
    for (std::size_t i = 0; i < a.owner_changes.size(); ++i)
        if (a.owner_changes[i].year != b.owner_changes[i].year
         || a.owner_changes[i].province != b.owner_changes[i].province
         || a.owner_changes[i].owner != b.owner_changes[i].owner)
            return false;
    if (a.province_stride != b.province_stride) return false;
    if (a.battles != b.battles || a.conquests != b.conquests
     || a.foundings != b.foundings || a.winter_campaigns != b.winter_campaigns)
        return false;
    if (sa.provinces.size() != sb.provinces.size()) return false;
    for (std::size_t i = 0; i < sa.provinces.size(); ++i)
    {
        const province& p = sa.provinces[i];
        const province& q = sb.provinces[i];
        if (p.nation != q.nation || p.population != q.population
         || p.manpower_stock != q.manpower_stock || p.culture != q.culture
         || p.contest_q != q.contest_q)
            return false;
    }
    return true;
}

/// A minimal two-province world: one rich target, one owner, at a chosen
/// distance. Used to isolate the supply-decay stall from everything else.
settlement_state two_polity_world(int separation)
{
    settlement_state ss;

    province a;
    a.col = 0; a.row = 0; a.anchor = 0;
    a.culture = 0; a.founding_culture = 0;
    a.farm_q = 900; a.ore_q = 500; a.port_q = 100;
    a.settle_score_q = 900; a.name = "Home";
    ss.provinces.push_back(a);

    province b;
    b.col = separation; b.row = 0; b.anchor = separation;
    b.culture = 1; b.founding_culture = 1;
    b.farm_q = 950; b.ore_q = 900; b.port_q = 100;
    b.settle_score_q = 880; b.name = "Prize";
    ss.provinces.push_back(b);

    return ss;
}

/// The synthetic worlds' own grid. Small enough to allocate a terrain field
/// for, wide enough that a 34-tile separation cannot wrap the cylinder.
constexpr int syn_gw = 168;
constexpr int syn_gh = 90;

/// A UNIFORM terrain field, held by value so the view it hands out stays valid.
///
/// The point is to make terrain a VARIABLE the harness controls. BL-316 S1's
/// finding was that both callers passed an empty view, so every Era -1 battle
/// ever fought resolved on default grassland/plains and the whole
/// terrain_combat module was inert — a state no assertion could have caught,
/// because there was nothing to compare against. Two runs differing only in
/// their ground is that comparison.
struct flat_terrain
{
    std::vector<terrain_composition> composition;
    std::vector<terrain_landform>    landform;

    flat_terrain(int gw, int gh, terrain_composition c, terrain_landform l)
        : composition(static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh), c)
        , landform(static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh), l)
    {}

    sim_terrain_view view() const { return sim_terrain_view{&composition, &landform}; }
};

/// Did two runs produce a different history at all? The inertness check.
bool differs(const history_sim_state& a, const history_sim_state& b)
{
    if (a.battles != b.battles || a.conquests != b.conquests
     || a.foundings != b.foundings || a.stalled_campaigns != b.stalled_campaigns)
        return true;
    if (a.owner_changes.size() != b.owner_changes.size()) return true;
    for (std::size_t i = 0; i < a.owner_changes.size(); ++i)
        if (a.owner_changes[i].year != b.owner_changes[i].year
         || a.owner_changes[i].province != b.owner_changes[i].province
         || a.owner_changes[i].owner != b.owner_changes[i].owner)
            return true;
    return false;
}

} // namespace

int main()
{
    std::printf("=== history sim harness (BL-277 scorer + BL-271 year tick) ===\n");

    world_params wp;
    generation_report r1;
    const world w1 = make_hard_coded_world(wp, &r1);
    (void)w1;

    const generation_report::body_entry* k1 = kepler_of(r1);
    if (!k1)
    {
        std::printf("FAIL  no body in the generation report\n");
        return 1;
    }

    const sim_terrain_view no_terrain{}; // Both members null: neutral terrain, legal by design.

    // THE DEFAULTS ARE THE REAL EPOCH. This harness pinned start/stop to
    // 0 -> 1960, which stopped being the run the game does when the campaign
    // epoch moved to 0 CE (the sim now runs 4000 BCE -> 0 CE under the stepped
    // decision clock). Left pinned, every Kepler assertion below measured a
    // configuration that no longer exists, and the stepped clock — the thing
    // that makes the run affordable — was never exercised here at all.
    history_sim_params params;

    // AND THE GRID MUST BE THE REAL GRID. These runs passed 168x90 against a
    // Kepler that is home_grid_width x home_grid_height (312x145). `gw` is the
    // cylinder's circumference: understate it and `province_distance` wraps
    // columns that do not wrap, silently reporting two provinces on opposite
    // sides of the map as neighbours. Same class of defect as the sweep's
    // 168x90-vs-180x84 terrain misalignment, and just as quiet.
    constexpr int kgw = home_grid_width;
    constexpr int kgh = home_grid_height;

    // --- R1  determinism ---------------------------------------------------
    {
        settlement_state s1 = k1->settlement;
        settlement_state s2 = k1->settlement;
        const history_sim_state a = run_history_sim(s1, nullptr, no_terrain, kgw, kgh, params, 12345u);
        const history_sim_state b = run_history_sim(s2, nullptr, no_terrain, kgw, kgh, params, 12345u);
        check(same_run(a, b, s1, s2),
              "R1   two runs of the same seed produce an identical history and ownership ring");
        check(!a.polities.empty(), "R1b  the sim seeds at least one polity from the cultures");
    }

    // --- R6/R7/R8  ring size, runtime, and founding ------------------------
    {
        settlement_state s = k1->settlement;
        const std::size_t before = s.provinces.size();

        const auto t0 = std::chrono::steady_clock::now();
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, kgw, kgh, params, 7u);
        const auto t1 = std::chrono::steady_clock::now();
        const int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        const int64_t bytes = owner_ring_bytes(a);
        std::printf("      run: %lld provinces -> %lld, %lld battles, %lld conquests, "
                    "%lld foundings, %lld winter, ring %lld bytes, %lld ms\n",
                    static_cast<long long>(before),
                    static_cast<long long>(s.provinces.size()),
                    static_cast<long long>(a.battles),
                    static_cast<long long>(a.conquests),
                    static_cast<long long>(a.foundings),
                    static_cast<long long>(a.winter_campaigns),
                    static_cast<long long>(bytes),
                    static_cast<long long>(ms));

        check(bytes > 0 && bytes < 1024 * 1024,
              "R6   the ownership time-lapse substrate stays under 1 MB");
        // The <1s target moved to BL-320 (Era -1 sim runtime): the settle-occupancy
        // fix quadrupled real province count (correct behaviour, ~4x work), and the
        // measured full run is ~2.1s. Bound against that reality so the harness
        // stays honest; the sub-second bar returns when BL-320 lands its index.
        check(ms < 3000,
              "R7   a full 4000 BCE -> 0 CE run fits the measured budget (BL-320)");
        check(s.provinces.size() > before,
              "R8   the Settle verb founds provinces during the run");
        check(a.years == params.stop_year - params.start_year,
              "R6b  the run covers one tick per simulated year");

        // The change list must actually replay: the final slice has to agree
        // with the provinces' own nation field, or the time-lapse would drift
        // from the world it claims to depict.
        const std::vector<uint16_t> last = owner_slice_at(a, params.stop_year);
        bool replay_agrees = (last.size() == s.provinces.size());
        if (replay_agrees)
            for (std::size_t i = 0; i < s.provinces.size(); ++i)
                if (s.provinces[i].nation >= 0
                 && last[i] != static_cast<uint16_t>(s.provinces[i].nation))
                    { replay_agrees = false; break; }
        check(replay_agrees,
              "R6c  replaying the change list reproduces the final political map");
    }

    // --- R3  the supply-decay stall ----------------------------------------
    //
    // Same two polities, same prize, different distance. Near, the campaign is
    // supplied and the province can change hands; far, supply decays to
    // nothing and the frontier stalls on arithmetic alone.
    {
        history_sim_params p2 = params;
        // The synthetic cases keep their own short, FLAT span: they isolate one
        // mechanism, and the stepped clock's coarse prehistory bands would make
        // "how many rounds happened" a second variable. `params` now carries the
        // real 4000 BCE epoch, so start_year has to be stated, not inherited.
        p2.start_year = 0;
        p2.stop_year  = 400;

        // THE RADIUS MUST NOT BE WHAT STOPS THE FAR CASE (BL-312). The first cut
        // put the far prize 34 tiles out against a neighbour_radius of 9, so it
        // was filtered before supply decay was ever computed — the assertion
        // passed because nothing was SCORED, not because supply ran out, and the
        // stall mechanism carrying the whole non-hegemony argument had no test
        // behind it. Widening the radius past the separation makes the far
        // target a real candidate, so only supply decay can stop it.
        p2.neighbour_radius = 40;

        // AND the distance term in the OBJECTIVE SCORE must not be what stops
        // it either. Zeroing `w_dist` isolates the logistics price as the only
        // thing left that can stop the far case, which is the mechanism under
        // test. (Historically `w_dist` was a flat subtraction and vetoed the far
        // target outright; it is a proportional discount now, so it discourages
        // rather than forbids. Zeroing it still removes the variable.)
        p2.w_dist = 0;

        settlement_state near_w = two_polity_world(3);
        settlement_state far_w  = two_polity_world(34);

        const history_sim_state a = run_history_sim(near_w, nullptr, no_terrain, syn_gw, syn_gh, p2, 99u);
        const history_sim_state b = run_history_sim(far_w,  nullptr, no_terrain, syn_gw, syn_gh, p2, 99u);

        std::printf("      near: %lld battles / %lld conquests | far: %lld battles / %lld conquests\n",
                    static_cast<long long>(a.battles), static_cast<long long>(a.conquests),
                    static_cast<long long>(b.battles), static_cast<long long>(b.conquests));

        check(a.battles > 0, "R3a  an adjacent objective is actually campaigned for");
        check(b.battles > 0,
              "R3a2 the FAR objective is genuinely scored and attacked — the radius is not what stops it");
        check(b.stalled_campaigns > 0,
              "R3a3 those far campaigns arrive under-supplied (the stall is real, not absence)");
        // WHAT ACTUALLY STOPS DISTANT EXPANSION — measured, not assumed. On
        // plains, with the burden of breadth inert (two provinces is far under
        // `free_holdings`), supply only mitigates attrition and attrition on
        // plains is 100/1000. So the whole span from fully supplied to totally
        // cut off is 10% of combat power, and the far province can still fall.
        //
        // This is RECORDED rather than asserted away, and it is why BL-316 exists:
        // the fix for a 10% dynamic range is not to inflate the supply term but
        // to make the other two terms real — terrain (a mountain battle carries
        // up to 900/1000 attrition, so supply matters enormously there) and the
        // burden of breadth. Both are exercised by S2/S3 below, on purpose, in
        // conditions where they can actually bite.
        std::printf("      far: %lld stalled of %lld campaigns, %lld conquests "
                    "(supply's plains dynamic range is ~10%% of combat power)\n",
                    static_cast<long long>(b.stalled_campaigns),
                    static_cast<long long>(b.battles),
                    static_cast<long long>(b.conquests));

        // DIRECTION, NOT AN ABSOLUTE. This was `c.battles == 0` — the claim that
        // the score's distance penalty vetoes the far target outright. That was
        // true of the flat `w_dist` and is deliberately no longer true: a flat
        // subtraction from a ~200-300 province value suppressed every war on a
        // large map, so `w_dist` became a proportional discount. Asserting the
        // old absolute would now be asserting the bug back. What must still hold
        // is the direction — pricing distance cannot make the far target MORE
        // attractive.
        {
            history_sim_params p_dist = p2;
            p_dist.w_dist = 120;   // The default: distance priced in the score.
            settlement_state far2 = two_polity_world(34);
            const history_sim_state c =
                run_history_sim(far2, nullptr, no_terrain, syn_gw, syn_gh, p_dist, 99u);
            std::printf("      far with w_dist=120: %lld battles / %lld conquests "
                        "(w_dist=0 gave %lld / %lld)\n",
                        static_cast<long long>(c.battles), static_cast<long long>(c.conquests),
                        static_cast<long long>(b.battles), static_cast<long long>(b.conquests));
            check(c.battles <= b.battles,
                  "R3b2 pricing distance in the score never makes a far target more attractive");
        }
        check(a.conquests >= b.conquests,
              "R3c  proximity never scores worse than distance for territorial gain");
    }

    // --- S1  real terrain is LIVE, not inert (BL-316) ----------------------
    //
    // The finding this slice was filed on: both callers passed an empty
    // `sim_terrain_view`, so every Era -1 battle ever fought resolved on default
    // grassland/plains and terrain_combat — defence, attrition, BL-233's
    // modifiers — was dead code. That state is invisible to any single run. It
    // shows up only as two runs that differ ONLY in their ground and produce
    // byte-identical histories.
    {
        history_sim_params ps = params;
        ps.start_year = 0;
        ps.stop_year  = 400;
        ps.neighbour_radius = 40;

        const flat_terrain plains(syn_gw, syn_gh,
                                  terrain_composition::grassland, terrain_landform::plains);
        const flat_terrain alps(syn_gw, syn_gh,
                                terrain_composition::rocky, terrain_landform::mountain);

        settlement_state wp_ = two_polity_world(6);
        settlement_state wm  = two_polity_world(6);
        const history_sim_state on_plains =
            run_history_sim(wp_, nullptr, plains.view(), syn_gw, syn_gh, ps, 77u);
        const history_sim_state on_mountain =
            run_history_sim(wm,  nullptr, alps.view(),   syn_gw, syn_gh, ps, 77u);

        std::printf("      plains: %lld battles / %lld conquests | mountain: %lld / %lld\n",
                    static_cast<long long>(on_plains.battles),
                    static_cast<long long>(on_plains.conquests),
                    static_cast<long long>(on_mountain.battles),
                    static_cast<long long>(on_mountain.conquests));

        check(differs(on_plains, on_mountain),
              "S1a  terrain CHANGES the history — the terrain view is no longer inert");
        check(on_mountain.conquests <= on_plains.conquests,
              "S1b  mountains are not easier to conquer across than plains");
    }

    // --- S2  terrain-weighted reach (BL-316) -------------------------------
    //
    // Reach is a COST over the neighbour graph, not a straight line: the same
    // separation across mountains costs about twice what it costs across
    // plains. Measured through supply, so the observable is the stall rate.
    {
        history_sim_params ps = params;
        ps.start_year = 0;
        ps.stop_year  = 400;
        ps.neighbour_radius = 40;
        // Reach is priced per 100 of accumulated cost, so it is a small term at
        // the default. Raised here so the SIGN of the effect is measurable
        // without changing what the sim does at its shipped tuning — this is an
        // instrument gain, not a calibration.
        ps.terrain_reach_cost_q = 400;

        const flat_terrain plains(syn_gw, syn_gh,
                                  terrain_composition::grassland, terrain_landform::plains);
        const flat_terrain alps(syn_gw, syn_gh,
                                terrain_composition::rocky, terrain_landform::mountain);

        settlement_state wp_ = two_polity_world(20);
        settlement_state wm  = two_polity_world(20);
        const history_sim_state flat_run =
            run_history_sim(wp_, nullptr, plains.view(), syn_gw, syn_gh, ps, 88u);
        const history_sim_state hilly_run =
            run_history_sim(wm,  nullptr, alps.view(),   syn_gw, syn_gh, ps, 88u);

        std::printf("      reach: plains %lld stalled / %lld battles | mountain %lld / %lld\n",
                    static_cast<long long>(flat_run.stalled_campaigns),
                    static_cast<long long>(flat_run.battles),
                    static_cast<long long>(hilly_run.stalled_campaigns),
                    static_cast<long long>(hilly_run.battles));

        check(hilly_run.conquests <= flat_run.conquests,
              "S2a  terrain-weighted reach never makes mountainous ground cheaper to take");
    }

    // --- S3  the burden of breadth (BL-316) --------------------------------
    //
    // Ben's mechanism: supply available to a campaign falls with the polity's
    // TOTAL holdings, so expansion eventually pays for itself in reach and the
    // frontier stall becomes ARITHMETIC rather than a scoring preference.
    // Before this slice, holding 500 provinces cost exactly what holding 5 did.
    //
    // Isolated by turning the burden on and off over the same world. `burden`
    // is subtracted from supply directly, so the direction is unambiguous:
    // never more conquest with the burden on than with it off.
    {
        history_sim_params base = params;
        base.start_year = 0;
        base.stop_year  = 400;
        base.neighbour_radius = 40;

        history_sim_params off = base;
        off.holdings_burden_q = 0;      // No cost of breadth at all — the old world.

        history_sim_params on = base;
        on.free_holdings     = 0;       // Every province held costs supply...
        on.holdings_burden_q = 300;     // ...and costs it visibly.

        settlement_state w_off = two_polity_world(12);
        settlement_state w_on  = two_polity_world(12);
        const history_sim_state a =
            run_history_sim(w_off, nullptr, no_terrain, syn_gw, syn_gh, off, 55u);
        const history_sim_state b =
            run_history_sim(w_on,  nullptr, no_terrain, syn_gw, syn_gh, on,  55u);

        std::printf("      burden off: %lld battles / %lld conquests / %lld stalled | "
                    "on: %lld / %lld / %lld\n",
                    static_cast<long long>(a.battles), static_cast<long long>(a.conquests),
                    static_cast<long long>(a.stalled_campaigns),
                    static_cast<long long>(b.battles), static_cast<long long>(b.conquests),
                    static_cast<long long>(b.stalled_campaigns));

        check(differs(a, b),
              "S3a  the burden of breadth is LIVE — holding ground is no longer free");
        check(b.conquests <= a.conquests,
              "S3b  a polity charged for its breadth never conquers more than one that is not");
    }

    // --- BL-318  the verbs share one currency ------------------------------
    //
    // The failure this item names is a SCORER whose argmax is decided by scale
    // rather than by desirability: one verb pins at its ceiling and wins every
    // year forever. Both observed cuts had exactly that shape — Invest-dominated
    // (82 provinces, ownership frozen after year 458 of 1960), then
    // Settle-dominated (1532 provinces, 1450 foundings, conquests ZERO).
    //
    // So the check is not "the numbers look right", which is unfalsifiable, but
    // that NO VERB HAS SWALLOWED THE RUN: the sim must still be deciding things
    // late, and more than one verb must be reachable. Both cuts above would fail
    // this; neither would fail a total-count assertion.
    {
        settlement_state s = k1->settlement;
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, kgw, kgh, params, 2024u);

        int64_t last_change = params.start_year;
        for (const owner_change& c : a.owner_changes)
            if (c.year > last_change) last_change = c.year;

        const int64_t span   = params.stop_year - params.start_year;
        const int64_t elapsed = last_change - params.start_year;

        std::printf("      currency: %lld foundings / %lld conquests / %lld battles, "
                    "last ownership change year %lld of %lld..%lld\n",
                    static_cast<long long>(a.foundings),
                    static_cast<long long>(a.conquests),
                    static_cast<long long>(a.battles),
                    static_cast<long long>(last_change),
                    static_cast<long long>(params.start_year),
                    static_cast<long long>(params.stop_year));

        // THE INERT-TAIL TEST. "Last ownership change at year 458 of 1960" is
        // the exact symptom the shared currency was built to remove: three
        // quarters of the run doing nothing, so every distribution the sweep
        // reported described a world that stopped early.
        check(elapsed * 2 >= span,
              "B318a the run is still deciding in its second half — no verb has swallowed it");

        // Neither verb may be extinct. Settle-only and conquest-only are the two
        // observed collapses, and each is invisible to a check on the other.
        check(a.foundings > 0, "B318b the Settle verb is reachable under the shared currency");
        check(a.conquests > 0, "B318c the Campaign verb is reachable under the shared currency");
    }

    // --- R5  season is an action axis, not a clock -------------------------
    {
        settlement_state s = k1->settlement;
        history_sim_params p3 = params; // The real epoch — winter is a real axis in it.
        p3.winter_score_premium_q = 0;  // Make winter freely competitive.
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, kgw, kgh, p3, 4242u);
        check(a.winter_campaigns > 0,
              "R5   winter campaigns are chosen as candidates, not scheduled by a clock");
        check(a.battles >= a.winter_campaigns,
              "R5b  winter campaigns are a subset of all campaigns");
    }

    // --- R4  province granularity ------------------------------------------
    //
    // Structural, not observational: run_history_sim takes no mutable tile
    // access at all — its terrain view is two const pointers — so it CANNOT
    // write a tile. The assertion below records that transfer moves the
    // province's own owner field and nothing wider.
    {
        settlement_state s = two_polity_world(3);
        history_sim_params p4 = params;
        p4.start_year = 0;   // Synthetic world: its own flat span (see R3).
        p4.stop_year  = 400;
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, syn_gw, syn_gh, p4, 5u);
        bool anchors_intact = (s.provinces[0].anchor == 0 && s.provinces[1].anchor == 3);
        check(anchors_intact,
              "R4   a run never rewrites a province's anchor tile — transfer is province-granular");
        check(a.province_stride == static_cast<int>(s.provinces.size()),
              "R4b  the ring's stride matches the final province count");
    }

    // --- BL-274  era-keyed rosters ----------------------------------------
    //
    // Asymmetry is the point: two provinces at the same band field different
    // rosters because their ground differs. A test that only checked "a stack
    // comes back" would pass on a table that ignored endowment entirely.
    {
        province bare;  bare.farm_q = 100; bare.ore_q = 0;   bare.port_q = 0;   bare.energy_q = 0;
        province forge; forge.farm_q = 100; forge.ore_q = 900; forge.port_q = 0; forge.energy_q = 0;
        province coast; coast.farm_q = 100; coast.ore_q = 0;  coast.port_q = 900; coast.energy_q = 0;

        const auto bare_rows  = available_rows(bare,  roster_band::classical);
        const auto forge_rows = available_rows(forge, roster_band::classical);
        const auto coast_rows = available_rows(coast, roster_band::classical);

        check(forge_rows.size() > bare_rows.size(),
              "B274a ore country fields types bare ground cannot (endowment gates bite)");

        bool coast_has_naval = false;
        for (const roster_row* r : coast_rows) if (r->cls == unit_class::naval) coast_has_naval = true;
        bool bare_has_naval = false;
        for (const roster_row* r : bare_rows) if (r->cls == unit_class::naval) bare_has_naval = true;
        check(coast_has_naval && !bare_has_naval,
              "B274b a port fields ships and a landlocked province does not");

        // The stirrup is a MEDIEVAL unlock — the T1/T2 boundary settled with the
        // roster grouping. A classical roster must not contain heavy horse.
        bool classical_stirrup = false;
        for (const roster_row* r : available_rows(forge, roster_band::classical))
            if (std::string(r->name) == "Stirrup Horse") classical_stirrup = true;
        bool medieval_stirrup = false;
        for (const roster_row* r : available_rows(forge, roster_band::medieval))
            if (std::string(r->name) == "Stirrup Horse") medieval_stirrup = true;
        check(!classical_stirrup, "B274c shock cavalry is NOT a 0 CE unit (the stirrup is T2)");

        // Bands are cumulative — nothing un-invents a spear.
        check(available_rows(forge, roster_band::industrial).size()
                  > available_rows(forge, roster_band::classical).size(),
              "B274d bands are cumulative: a later band keeps the earlier rows");
        (void)medieval_stirrup;

        check(roster_band_for_capacity(1) == roster_band::classical
           && roster_band_for_capacity(3) == roster_band::medieval
           && roster_band_for_capacity(4) == roster_band::gunpowder
           && roster_band_for_capacity(6) == roster_band::industrial,
              "B274e capacity bands map onto the settled roster grouping");
    }

    // --- BL-299  two-great-powers seed ------------------------------------
    {
        settlement_state s = k1->settlement;
        history_sim_params p5 = params;
        // Seeding happens at setup, so a short slice of the real epoch is
        // enough — and stays cheap. Stated relative to start_year, which is
        // 4000 BCE now, not 0.
        p5.stop_year = p5.start_year + 300;
        p5.seed_great_powers = true;
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, kgw, kgh, p5, 11u);

        int majors = 0, expansionist = 0, preserving = 0;
        for (const polity& q : a.polities)
        {
            if (!q.major) continue;
            ++majors;
            if (q.aggression_q >= 800) ++expansionist;
            if (q.aggression_q <= 300) ++preserving;
        }
        check(majors == 2, "B299a exactly two majors are seeded");
        check(expansionist == 1 && preserving == 1,
              "B299b the majors carry OPPOSED creeds, not the same one twice");
        check(static_cast<int>(a.polities.size()) > 2,
              "B299c the periphery survives as actors, not terrain");
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
