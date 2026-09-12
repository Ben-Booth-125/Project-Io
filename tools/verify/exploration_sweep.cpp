// ---------------------------------------------------------------------------
// exploration_sweep — BL-937. Run the Exploration span (1200 -> 1660 CE) over
// a seed spread and report the TEN readings EXPLORATION.md sec "What the
// phase is judged on" names, all taken at 1660, all over the spread, NEVER
// per world (the same distributional discipline history_sweep applies to the
// Empire handoff).
//
// WHY A SIBLING HARNESS RATHER THAN A NEW BLOCK IN history_sweep.cpp. That
// file is already ~3,800 lines and its own subject is the Empire span's
// closure contract (BL-275/BL-907). This item's subject is a different span
// with a different judgement (EXPLORATION.md, not CIVILISATION.md), so a
// sibling keeps each harness readable against the one doc it measures,
// exactly as history_sweep itself argues for one report per closure.
//
// THIS HARNESS REPORTS; IT DOES NOT GATE, on the same instruction
// history_sweep's own banner states: EXPLORATION.md is explicit that "a
// reading is a requirement, not a target" — a seed that refuses one is a
// legitimate world, a spread that refuses one is a phase that did not do its
// job, and that is Ben's judgement to make off the printed table, not this
// harness's to assert into a PASS/FAIL line.
//
// WHAT IS REAL AND WHAT IS SCAFFOLDING. Readings 1-2 (displacement,
// conflict-persists) are computed off real sim output — BL-931's bare span
// already produces battles tagged by attacker/defender polity, and the
// directed contact table already says who had met whom by 1200. Readings
// 3-10 have no mechanism yet (BL-932 through BL-942 land it wave by wave);
// each still gets its row, printed as NOT YET MEASURABLE with the item that
// owes it, so the gap is visible in the report rather than silently absent.
//
// Usage:  exploration_sweep [seed_count]      (default 8)
// ---------------------------------------------------------------------------

#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/sim_terrain_build.hpp"
#include "world/settlement.hpp"
#include "world/works_roster.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
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

// NO WORKS FIXTURE HERE, DELIBERATELY. `make_hard_coded_world` is called
// below with no registry (works = nullptr), on the same footing as
// history_sweep.cpp's own generation-deriving path — see that file's S1b
// comment: "generation is called here with works = nullptr... the APP passes
// its real registry, so the shipped game scores FIVE verbs where every
// harness scores four." A traced re-run of the Exploration span must use
// EXACTLY what generation's own run used (`era_minus_one_fixture::works`,
// captured below), not a harness-invented registry — the first draft of this
// file passed one and it broke the acceptance check for that reason.

/// Does `contacts` carry a directed pair naming both @p a and @p b, in either
/// direction? `contact` is recorded in BOTH directions when a pair first
/// meets (history_sim.hpp's own comment on the struct), so one direction is
/// technically enough — checked both ways here rather than trusted, because
/// asserting an invariant a different item guarantees is how this file avoids
/// silently depending on it.
bool contact_exists(const std::vector<contact>& contacts, int a, int b)
{
    for (const contact& c : contacts)
        if ((c.from == a && c.to == b) || (c.from == b && c.to == a)) return true;
    return false;
}

// ---------------------------------------------------------------------------
// ONE SEED'S READING.
// ---------------------------------------------------------------------------
struct exploration_row
{
    uint32_t seed = 0;
    bool     ok   = false; ///< False if the span did not run at all for this seed.

    // --- The Empires round's own close, for reading 2's baseline -----------
    int64_t empire_battles    = 0;
    int64_t empire_years      = 0;

    // --- The Exploration span, as generation's own (untraced) run produced -
    int64_t expl_battles      = 0;
    int64_t expl_conquests    = 0;
    int64_t expl_foundings    = 0;
    int64_t expl_years        = 0;

    // --- Readings 1-2, off the traced re-run --------------------------------
    int64_t neighbour_wars    = 0; ///< Battles between pairs already in contact by 1200.
    int64_t frontier_skirmishes = 0; ///< Battles between pairs that first met DURING the span.
    int64_t ambiguous_battles = 0; ///< defender id 0 — unowned ground or polity 0, indistinguishable (see note below).

    bool traced_matches_untraced = false; ///< The acceptance check: re-run reproduces battles/conquests/foundings bit for bit.

    /// BL-932 DIAGNOSTIC (not yet reading 8's formal row — that is BL-940's,
    /// once throughput exists too): (capital treasury, corridors touching
    /// held ground) at the traced run's own close, one pair per living
    /// polity. A cheap proxy for "the inherited corridor network" ahead of
    /// BL-940 giving throughput its own real quantity.
    std::vector<std::pair<int64_t, int64_t>> polity_treasury_corridor;
};

/// Century-scaled rate, avoiding a divide-by-zero span.
double per_century(int64_t count, int64_t years)
{
    return years > 0 ? (static_cast<double>(count) * 100.0) / static_cast<double>(years) : 0.0;
}

} // namespace

int main(int argc, char** argv)
{
    int seed_count = 8;
    for (int a = 1; a < argc; ++a)
    {
        const int n = std::atoi(argv[a]);
        if (n > 0) seed_count = n;
    }

    std::printf("=== exploration sweep (BL-937) - %d seeds, 1200 -> 1660 CE ===\n\n", seed_count);

    std::vector<exploration_row> rows;
    rows.reserve(static_cast<std::size_t>(seed_count));

    for (int i = 0; i < seed_count; ++i)
    {
        world_params wp;
        wp.seed = static_cast<uint32_t>(i);
        wp.exploration_sim_enabled = true; // BL-937: the whole point of this sweep.

        generation_report     rep;
        era_minus_one_fixture fx;
        const world w = make_hard_coded_world(wp, &rep, world_gen_config{},
                                              /*progress=*/nullptr, /*works=*/nullptr, &fx);
        (void)w;

        exploration_row row;
        row.seed = wp.seed;

        if (!fx.ran)
        {
            std::printf("  seed %d: THE EMPIRES ROUND DID NOT RUN — skipped.\n", i);
            rows.push_back(row);
            continue;
        }
        row.empire_battles = fx.battles;
        row.empire_years   = fx.years;

        if (!fx.exploration_ran)
        {
            // Real and worth seeing: BL-931's own gate is
            // `!kepler_pass_one.polities.empty()`, so a seed whose Empires
            // round eliminated every polity before 1200 (a hegemon or a
            // total collapse) legitimately produces no Exploration span at
            // all. EXPLORATION.md's own discipline: a seed that refuses a
            // reading is a legitimate world.
            std::printf("  seed %d: EMPIRES ROUND RAN, EXPLORATION SPAN DID NOT (no surviving polity at 1200).\n", i);
            rows.push_back(row);
            continue;
        }

        row.expl_battles   = fx.exploration_state.battles;
        row.expl_conquests = fx.exploration_state.conquests;
        row.expl_foundings = fx.exploration_state.foundings;
        row.expl_years     = fx.exploration_state.years;

        // --- THE TRACED RE-RUN (readings 1-2 need per-battle attacker/ -----
        // defender pairs, which only `battle_trace` carries, and tracing is
        // deliberately never turned on in generation's own run — see the
        // field comment on `era_minus_one_fixture::pre_exploration_settlement`
        // for why this is a second, disposable run rather than a flag flipped
        // on the real one.
        history_sim_params ep2 = fx.exploration_params;
        ep2.trace_battles = true;
        ep2.resume_polities  = &fx.pre_exploration_polities;
        ep2.resume_grudges   = &fx.pre_exploration_grudges;
        ep2.resume_contacts  = &fx.pre_exploration_contacts;
        ep2.resume_corridors = &fx.pre_exploration_corridors;

        settlement_state ss_copy = fx.pre_exploration_settlement;
        creed_state       cs_copy = fx.pre_exploration_creeds;
        // `fx.works` — WHATEVER GENERATION'S OWN RUN ACTUALLY USED (nullptr in
        // this harness, since `make_hard_coded_world` is called below with no
        // registry — the same known divergence history_sweep.cpp's own S1b
        // comment names: "the APP passes its real registry, so the shipped
        // game scores FIVE verbs where every harness scores four"). Passing
        // this harness's own `works_fixture()` here instead of `fx.works`
        // made the FIRST run of this harness fail its own acceptance check —
        // Build became a scoreable verb in the re-run and not in the
        // original, which is exactly the "second construction drifts" defect
        // era_minus_one.hpp exists to close. Fixed by reading the capture.
        const history_sim_state traced = run_history_sim(
            ss_copy, &cs_copy, fx.terrain.view(), fx.gw, fx.gh, ep2,
            fx.exploration_seed, /*year_progress=*/nullptr, fx.works, /*tap=*/nullptr);

        row.traced_matches_untraced =
            traced.battles == fx.exploration_state.battles &&
            traced.conquests == fx.exploration_state.conquests &&
            traced.foundings == fx.exploration_state.foundings;

        for (const battle_trace& bt : traced.battle_traces)
        {
            if (bt.defender == 0 && bt.attacker == 0)
            {
                // Both id 0 is not a real Campaign battle this harness can
                // classify (see the note on `contact_exists`'s caller below)
                // — count it separately rather than guess.
                ++row.ambiguous_battles;
                continue;
            }
            const bool already_known =
                contact_exists(fx.pre_exploration_contacts, bt.attacker, bt.defender);
            if (already_known) ++row.neighbour_wars;
            else               ++row.frontier_skirmishes;
        }

        // BL-932 DIAGNOSTIC — read straight off the traced re-run above,
        // which already mutated `ss_copy` in place and is disposable.
        for (const polity& q : traced.polities)
        {
            if (!q.alive || q.capital < 0
             || static_cast<std::size_t>(q.capital) >= ss_copy.regions.size())
                continue;
            int64_t touch = 0;
            for (const history_corridor& c : traced.supply_corridors)
            {
                const bool a_held = static_cast<std::size_t>(c.a) < ss_copy.regions.size()
                                  && ss_copy.regions[c.a].nation == q.id;
                const bool b_held = static_cast<std::size_t>(c.b) < ss_copy.regions.size()
                                  && ss_copy.regions[c.b].nation == q.id;
                if (a_held || b_held) ++touch;
            }
            row.polity_treasury_corridor.push_back(
                {ss_copy.regions[static_cast<std::size_t>(q.capital)].treasury, touch});
        }

        row.ok = true;
        rows.push_back(row);
    }

    // -----------------------------------------------------------------------
    // STRUCTURAL CHECKS
    // -----------------------------------------------------------------------
    std::printf("\n--- structural ---\n");
    int ran_count = 0;
    for (const exploration_row& r : rows) if (r.ok) ++ran_count;
    check(ran_count > 0, "at least one seed ran the Exploration span");

    bool all_traces_match = true;
    for (const exploration_row& r : rows)
        if (r.ok && !r.traced_matches_untraced) all_traces_match = false;
    check(all_traces_match,
        "traced re-run reproduces generation's own (untraced) battles/conquests/foundings, every ran seed");

    // -----------------------------------------------------------------------
    // READINGS 1-2 — DISPLACEMENT AND CONFLICT-PERSISTENCE, THE ONE TEST.
    // -----------------------------------------------------------------------
    //
    // OPERATIONALIZATION, STATED EXPLICITLY (this is a judgement call, not a
    // literal reading off EXPLORATION.md, and it is flagged in NEEDS_REVIEW
    // as such): a battle's pair is a LONG-CONTACTED NEIGHBOUR if
    // `pre_exploration_contacts` (the directed contact table exactly as the
    // Empires round left it at 1200) already names the pair; it is a
    // NEWLY-CONTACTED, frontier pair otherwise — i.e. the two polities met
    // for the first time at some point during 1200-1660. The RATIO compares
    // the two resulting rates (per century) against EACH OTHER within the
    // span, rather than each independently against the Empires round's own
    // rate — the doc's "falls relative to" reads most naturally as a
    // within-span comparison, and the trap it exists to catch (a world that
    // "simply stopped fighting") is caught regardless of which comparison is
    // taken, because reading 2 (conflict persists, compared against the
    // Empires round explicitly) is read ALONGSIDE it, exactly as the doc
    // insists.
    std::printf("\n--- readings 1-2 (read together) ---\n");
    std::printf("%-6s %10s %10s %8s %8s %8s %10s | %10s %10s %8s\n",
                "seed", "emp.battl", "emp.yrs", "emp/cent",
                "expl.bat", "expl.yrs", "expl/cent",
                "neigh/c", "front/c", "ratio");
    std::vector<double> ratios;
    std::vector<double> empire_rates, expl_rates;
    for (const exploration_row& r : rows)
    {
        if (!r.ok) continue;
        const double empire_rate = per_century(r.empire_battles, r.empire_years);
        const double expl_rate   = per_century(r.expl_battles, r.expl_years);
        const double neigh_rate  = per_century(r.neighbour_wars, r.expl_years);
        const double front_rate  = per_century(r.frontier_skirmishes, r.expl_years);
        const double ratio       = neigh_rate > 0.0 ? front_rate / neigh_rate : -1.0; // -1 => no neighbour wars at all

        empire_rates.push_back(empire_rate);
        expl_rates.push_back(expl_rate);
        if (ratio >= 0.0) ratios.push_back(ratio);

        std::printf("%-6u %10lld %10lld %8.2f %8lld %8lld %10.2f | %10.2f %10.2f %8s\n",
                    r.seed,
                    static_cast<long long>(r.empire_battles), static_cast<long long>(r.empire_years), empire_rate,
                    static_cast<long long>(r.expl_battles), static_cast<long long>(r.expl_years), expl_rate,
                    neigh_rate, front_rate,
                    ratio >= 0.0 ? (std::to_string(ratio).substr(0, 6)).c_str() : "no-nb");
    }

    if (!ratios.empty())
    {
        std::sort(ratios.begin(), ratios.end());
        const double median_ratio = ratios[ratios.size() / 2];
        std::printf("\n  DISPLACEMENT (frontier-rate / neighbour-rate), median across seeds with any "
                    "neighbour war: %.2f\n", median_ratio);
        std::printf("  %s\n", median_ratio > 1.0
            ? "> 1: frontier-skirmish rate exceeds neighbour-war rate — displacement, not mere calming."
            : "<= 1: neighbour-war rate still leads — no displacement measured on this spread.");
    }
    else
    {
        std::printf("\n  DISPLACEMENT: no seed in this spread fought a single neighbour-war — "
                    "the ratio is undefined, not zero. Report this spread's raw counts to Ben rather "
                    "than reading a ratio into an empty denominator.\n");
    }

    if (!empire_rates.empty())
    {
        std::sort(empire_rates.begin(), empire_rates.end());
        std::sort(expl_rates.begin(), expl_rates.end());
        const double med_empire = empire_rates[empire_rates.size() / 2];
        const double med_expl   = expl_rates[expl_rates.size() / 2];
        std::printf("\n  CONFLICT PERSISTS: median Empires battle rate %.2f/century vs median "
                    "Exploration battle rate %.2f/century.\n", med_empire, med_expl);
        std::printf("  %s\n",
            (med_expl < med_empire && med_expl > 0.05)
                ? "Below Empires' and not near zero — the handoff's own bar."
                : (med_expl <= 0.05
                    ? "NEAR ZERO — a frozen map. This is the named failure mode."
                    : "NOT below Empires' rate — conflict did not fall at all."));
    }

    int ambiguous_total = 0;
    for (const exploration_row& r : rows) ambiguous_total += static_cast<int>(r.ambiguous_battles);
    if (ambiguous_total > 0)
        std::printf("\n  NOTE: %d battle(s) traced with BOTH attacker and defender id 0 across the "
                    "spread, excluded from both counts above (battle_trace's own comment: defender "
                    "0 is the sentinel for \"unowned\", indistinguishable from a real polity id 0 — "
                    "a pre-existing ambiguity in the struct, not introduced here).\n", ambiguous_total);

    // -----------------------------------------------------------------------
    // READING 8 — TREASURY SPREAD VS. INHERITED CORRIDOR TOUCH (BL-932/940).
    // -----------------------------------------------------------------------
    {
        std::vector<std::pair<int64_t, int64_t>> pairs; // (treasury, corridor_touch)
        for (const exploration_row& r : rows)
            for (const auto& tc : r.polity_treasury_corridor) pairs.push_back(tc);

        std::printf("\n--- reading 8: treasury vs. inherited corridor touch ---\n");
        if (pairs.size() < 2)
        {
            std::printf("  fewer than two living polities across the spread — no spread to read.\n");
        }
        else
        {
            int64_t min_t = pairs[0].first, max_t = pairs[0].first, sum_t = 0;
            for (const auto& p : pairs) { min_t = std::min(min_t, p.first);
                                          max_t = std::max(max_t, p.first); sum_t += p.first; }
            const double mean_t = static_cast<double>(sum_t) / static_cast<double>(pairs.size());

            // Pearson correlation, integer inputs, double accumulation --
            // a report figure, not a decision input, so float is fine here.
            double mean_c = 0.0;
            for (const auto& p : pairs) mean_c += static_cast<double>(p.second);
            mean_c /= static_cast<double>(pairs.size());
            double cov = 0.0, var_t = 0.0, var_c = 0.0;
            for (const auto& p : pairs)
            {
                const double dt = static_cast<double>(p.first)  - mean_t;
                const double dc = static_cast<double>(p.second) - mean_c;
                cov += dt * dc; var_t += dt * dt; var_c += dc * dc;
            }
            const double corr = (var_t > 0.0 && var_c > 0.0) ? cov / std::sqrt(var_t * var_c) : 0.0;

            std::printf("  %zu living polities across the spread: treasury min=%lld max=%lld mean=%.1f\n",
                        pairs.size(), static_cast<long long>(min_t), static_cast<long long>(max_t), mean_t);
            std::printf("  correlation(treasury, corridor-touch) = %.3f\n", corr);
            std::printf("  %s\n", corr > 0.2
                ? "positive — treasury tracks the inherited/grown corridor network, as the done-when criterion asks."
                : "not clearly positive on this spread — report to Ben rather than re-tuning the formula silently.");
        }
    }

    // -----------------------------------------------------------------------
    // READING 9 — THROUGHPUT: corridors carrying materially different
    // volumes, with the road ladder visible in the difference (BL-940).
    // Read off each seed's own (untraced, real) exploration run —
    // `fx.exploration_state.supply_corridors`, finalised at that run's own
    // close, and the two road-ladder counters BL-940 added.
    // -----------------------------------------------------------------------
    {
        std::vector<int32_t> uses;
        int64_t total_post_roads_built = 0, total_treasury_spent = 0;
        int seeds_with_post_road = 0;
        for (int i = 0; i < seed_count; ++i)
        {
            // Re-derive nothing: `rows` does not keep the fixture, so this
            // reading re-runs generation once more per seed, same cost class
            // as the main loop above and paid once, at report time.
            world_params wp2;
            wp2.seed = static_cast<uint32_t>(i);
            wp2.exploration_sim_enabled = true;
            generation_report     rep2;
            era_minus_one_fixture fx2;
            const world w2 = make_hard_coded_world(wp2, &rep2, world_gen_config{},
                                                   nullptr, nullptr, &fx2);
            (void)w2;
            if (!fx2.ran || !fx2.exploration_ran) continue;
            for (const history_corridor& c : fx2.exploration_state.supply_corridors)
                uses.push_back(c.uses);
            if (fx2.exploration_state.post_roads_built > 0) ++seeds_with_post_road;
            total_post_roads_built += fx2.exploration_state.post_roads_built;
            total_treasury_spent   += fx2.exploration_state.treasury_spent_on_roads;
        }

        std::printf("\n--- reading 9: corridor throughput and the road ladder's third rung ---\n");
        if (uses.empty())
        {
            std::printf("  no seed in this spread produced a single corridor — no spread to read.\n");
        }
        else
        {
            std::sort(uses.begin(), uses.end());
            const int32_t min_u = uses.front(), max_u = uses.back();
            const int32_t med_u = uses[uses.size() / 2];
            std::printf("  %zu corridors across the spread: uses min=%d median=%d max=%d\n",
                        uses.size(), min_u, med_u, max_u);
            std::printf("  %s\n", (max_u > min_u)
                ? "corridors carry materially different volumes on this spread."
                : "every corridor carries the same volume — no spread measured.");
            std::printf("  post_roads_built total=%lld (in %d/%d seeds) treasury_spent_on_roads total=%lld\n",
                        static_cast<long long>(total_post_roads_built), seeds_with_post_road, seed_count,
                        static_cast<long long>(total_treasury_spent));
            std::printf("  %s\n", total_post_roads_built > 0
                ? "the road ladder's third rung fired at least once on this spread."
                : "the third rung never fired on this spread — report to Ben rather than "
                  "re-tuning post_road_treasury_cost silently.");
        }
    }

    // -----------------------------------------------------------------------
    // READINGS 3-7, 10 — SCAFFOLDING. Each has no mechanism yet; the slot is
    // real, the number is not, and the report says so rather than omitting
    // the row.
    // -----------------------------------------------------------------------
    std::printf("\n--- readings 3-7, 10 (scaffolding — mechanism lands in a later wave) ---\n");
    struct owed_reading { const char* name; const char* owner; };
    const owed_reading owed[] = {
        {"3. Both strategies pay (consolidator/expansionist, by creed)", "BL-942"},
        {"4. Treaty depth (standing at 1660, years left to run)",         "BL-933"},
        {"5. Colonial asymmetry (some hold subjects, most don't)",        "BL-934"},
        {"6. Subject friction (a subject's wants diverge from overlord)", "BL-934"},
        {"7. Fleets (uneven, at least one built-then-decayed)",           "BL-935"},
        {"10. Preference (goods wanted differently by culture, by route)","BL-936"},
    };
    for (const owed_reading& o : owed)
        std::printf("  %-58s NOT YET MEASURABLE — mechanism lands in %s\n", o.name, o.owner);

    std::printf("\n%d failure(s) in structural checks.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
