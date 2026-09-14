// ---------------------------------------------------------------------------
// exploration_sweep — BL-937. Run the Exploration span (1200 -> 1660 CE) over
// a seed spread and report the ELEVEN readings EXPLORATION.md sec "What the
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
// WHAT EACH READING IS READ OFF. Every one of the ten readings is computed
// off real sim output. Readings 1-2 (displacement, conflict-persists) and
// 3-7 read a traced, disposable re-run of the Exploration span (battle
// traces, treaties, subjects, fleets, and the 1660 regions/cultures the
// re-run leaves behind); readings 8-9 read the treasury/corridor state of
// that re-run and of generation's own untraced run; reading 10 reads the
// 1200 CE handoff state directly. Where a reading has no data on a spread it
// says so in its own printed line (NOT MEASURED), never by a missing row.
//
// ONE GENERATION PER SEED (BL-952). `make_hard_coded_world` is the dominant
// cost of this harness, so it is called exactly once per seed, in the main
// loop, and everything any reading needs is captured onto `exploration_row`
// there. The report sections below read `rows` only; none regenerates a
// world. The `[gen budget]` line count therefore equals the seed count.
//
// Usage:  exploration_sweep [seed_count] [--w_want_q=N]   (default 8)
//
//   --w_want_q=N  BL-953 TUNING ONLY: re-runs the traced span with the want
//                 lean at N instead of generation's own value. Readings 1, 2,
//                 4-7 then describe the overridden run; the traced-vs-untraced
//                 structural check is skipped (it compares against a run made
//                 with different params by construction).
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
#include <functional>
#include <cstring>
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

    // --- Reading 4: treaty depth --------------------------------------------
    int64_t treaties_formed_total  = 0;
    int64_t treaties_broken_total  = 0;
    int64_t treaty_blocked_campaigns = 0;
    std::vector<int64_t> treaty_years_left; ///< one per standing non-aggression pair at 1660.

    // --- Reading 5: colonial asymmetry --------------------------------------
    int64_t subjects_alive       = 0; ///< living polities with `overlord >= 0`.
    int64_t overlords_alive      = 0; ///< distinct living polities holding >= 1 subject.
    int64_t alive_polities       = 0;
    int64_t max_subject_distance = 0; ///< Chebyshev, capital to overlord's capital.
    int64_t subjections_formed   = 0;
    int64_t subjections_freed    = 0;
    int64_t tribute_remitted     = 0;

    // --- Reading 6: subject friction (operationalized -- see the printed
    // note beside readings 1-2's own operationalization disclosure). A
    // subject's contact graph is read as a proxy for its want graph (no live
    // want table is maintained during the span; contact is the want table's
    // own precondition -- CIVILISATION.md sec The directed want: "a want
    // requires contact"): friction is counted where a subject has met a
    // THIRD polity its overlord has never met.
    int64_t subjects_with_friction = 0;

    /// BL-953: subjects whose TOP WANT (argmax good of `polity_good_want_q`,
    /// off the LIVE preference derived from the traced run's own 1660 state)
    /// differs from their overlord's -- the want graph itself rather than the
    /// contact-graph proxy above. "No want at all" counts as its own value.
    int64_t subjects_with_divergent_top_want = 0;

    /// BL-953: (culture, good) preference entries derived LIVE from the traced
    /// run's 1660 close, beside reading 10's 1200 CE derivation.
    int64_t live_pref_entries_1660 = 0;

    // --- Reading 7: fleets ---------------------------------------------------
    int64_t navy_holders          = 0; ///< living polities with navy_stock > 0 at 1660.
    int64_t treasury_spent_on_navies = 0;
    int64_t treasury_spent_on_ports  = 0;
    int64_t treasury_spent_on_standing_armies = 0;
    /// BL-955: navies at 1660 split by the holder's EXPANSION RANK at 1660
    /// (`exploration_lean_ranks`): top half = rank >= 500.
    int64_t navy_holders_expn_top    = 0;
    int64_t navy_holders_expn_bottom = 0;
    /// BL-955: stock steps bought over the span, by kind (one choice per round).
    int64_t port_steps = 0, navy_steps = 0, army_steps = 0;
    /// BL-955: polities whose navy decayed from a standing fleet to zero at
    /// least once during the span.
    int64_t navies_lapsed = 0;
    /// BL-955: per living polity at 1660, its paid standing army (summed over
    /// held ground) per held region, and its mean garrison_target per held
    /// region -- the scale the saturation reference is read against.
    std::vector<int64_t> standing_per_region;
    std::vector<int64_t> garrison_per_region;
    int64_t standing_holders = 0; ///< living polities with any paid standing heads at 1660.
    int64_t army_saturated   = 0; ///< of those, seat standing above the saturation cap.

    // --- Reading 3: both strategies pay -------------------------------------
    /// One realm in a seed's top 3, with every quantity either ranking reads.
    struct strength_entry
    {
        int     id            = -1;
        int64_t treasury      = 0; ///< capital region's `treasury` at 1660.
        int     mean_supply_q = 0; ///< mean `network_supply_q` over held regions.
        int64_t regions       = 0; ///< held region count at 1660.
        int     cons_q        = 0; ///< consolidator_lean_q of its culture.
        int     expn_q        = 0; ///< expansion_lean_q of its culture.
    };
    bool strength_measured = false; ///< >= 2 living polities at the traced re-run's close.
    /// PRIMARY ranking (BL-951): capital treasury, tie-break mean supply.
    std::vector<strength_entry> top_by_treasury;
    bool top_has_consolidator = false;
    bool top_has_expansionist = false;
    /// COMPARISON ranking: held region count (the pre-BL-951 metric).
    std::vector<strength_entry> top_by_regions;
    bool regions_top_has_consolidator = false;
    bool regions_top_has_expansionist = false;

    // --- Reading 9: throughput, off generation's own (untraced) run ---------
    std::vector<int32_t> corridor_uses;
    int64_t post_roads_built        = 0;
    int64_t treasury_spent_on_roads = 0;

    // --- Reading 10: preference, off the 1200 CE handoff state --------------
    std::vector<int16_t> preference_weights;

    // --- Reading 11: trade (BL-954) -----------------------------------------
    int64_t flow_count            = 0;
    int64_t flow_volume           = 0;
    int64_t cross_landmass_volume = 0; ///< seller and buyer capitals on different landmasses.
    int64_t unknown_landmass_volume = 0; ///< a capital tile that reads as water: landmass undefined.
    int64_t flows_without_clause  = 0; ///< must be 0: every flow stands on a trade_access clause.
    int64_t trade_bound_pairs     = 0; ///< distinct pairs holding trade_access at 1660.
    std::vector<int64_t> pair_volume; ///< one per trade_access-bound pair, zero-flow pairs included.
};

/// LANDMASS IDENTITY for reading 11. `region::domain` only says land /
/// coastal water / open ocean -- it carries no mass id, and `pass_one_output`
/// says landmass identity is derived by a consumer. Derived here as the
/// 4-connected components of non-water tiles over the sim's own terrain
/// raster, columns wrapping (the map is a cylinder, history_sim.cpp's own
/// distance helper). -1 on water.
std::vector<int32_t> label_landmasses(const std::vector<terrain_substrate>& sub, int gw, int gh)
{
    std::vector<int32_t> mass(sub.size(), -1);
    if (gw <= 0 || gh <= 0 || sub.size() < static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
        return mass;
    int32_t next = 0;
    std::vector<int> stack;
    for (int start = 0; start < gw * gh; ++start)
    {
        if (mass[static_cast<std::size_t>(start)] >= 0 || is_water(sub[static_cast<std::size_t>(start)]))
            continue;
        mass[static_cast<std::size_t>(start)] = next;
        stack.push_back(start);
        while (!stack.empty())
        {
            const int idx = stack.back(); stack.pop_back();
            const int c = idx % gw, r = idx / gw;
            const int nb[4][2] = { {(c + 1) % gw, r}, {(c + gw - 1) % gw, r}, {c, r + 1}, {c, r - 1} };
            for (const auto& n : nb)
            {
                if (n[1] < 0 || n[1] >= gh) continue;
                const int ni = n[1] * gw + n[0];
                if (mass[static_cast<std::size_t>(ni)] >= 0 || is_water(sub[static_cast<std::size_t>(ni)]))
                    continue;
                mass[static_cast<std::size_t>(ni)] = next;
                stack.push_back(ni);
            }
        }
        ++next;
    }
    return mass;
}

/// Century-scaled rate, avoiding a divide-by-zero span.
double per_century(int64_t count, int64_t years)
{
    return years > 0 ? (static_cast<double>(count) * 100.0) / static_cast<double>(years) : 0.0;
}

} // namespace

int main(int argc, char** argv)
{
    int seed_count = 8;
    bool want_override = false;
    int  want_override_q = 0;
    for (int a = 1; a < argc; ++a)
    {
        if (std::strncmp(argv[a], "--w_want_q=", 11) == 0)
        {
            want_override   = true;
            want_override_q = std::atoi(argv[a] + 11);
            continue;
        }
        const int n = std::atoi(argv[a]);
        if (n > 0) seed_count = n;
    }
    if (want_override)
        std::printf("NOTE: --w_want_q=%d overrides the traced re-run's want lean (tuning only).\n",
                    want_override_q);

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

        // --- Reading 9 capture: generation's own (untraced) run -------------
        // `fx.exploration_state.supply_corridors`, finalised at that run's own
        // close, and the two road-ladder counters BL-940 added.
        for (const history_corridor& c : fx.exploration_state.supply_corridors)
            row.corridor_uses.push_back(c.uses);
        row.post_roads_built        = fx.exploration_state.post_roads_built;
        row.treasury_spent_on_roads = fx.exploration_state.treasury_spent_on_roads;

        // --- Reading 10 capture: the PRE-EXPLORATION handoff state ---------
        // `derive_culture_preference` is pure and needs no traced re-run of
        // its own — it takes the same regions/contacts/polities `derive_wants`
        // already reads at 1200 CE. Taken BEFORE the traced re-run below so it
        // reads the fixture exactly as generation left it.
        {
            const auto prefs = derive_culture_preference(
                fx.pre_exploration_settlement.regions, fx.pre_exploration_contacts,
                fx.pre_exploration_polities, static_cast<int>(fx.pre_exploration_creeds.cultures.size()));
            for (const auto& p : prefs) row.preference_weights.push_back(p.weight_q);
        }

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
        if (want_override) ep2.w_want_q = want_override_q;

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

        row.traced_matches_untraced = want_override ||
            (traced.battles == fx.exploration_state.battles &&
             traced.conquests == fx.exploration_state.conquests &&
             traced.foundings == fx.exploration_state.foundings);

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

        // --- BL-933/934/935 -- readings 4, 5, 6, 7, off the traced re-run ---
        row.treaties_formed_total     = traced.treaties_formed;
        row.treaties_broken_total     = traced.treaties_broken;
        row.treaty_blocked_campaigns  = traced.treaty_blocked_campaigns;
        row.subjections_formed        = traced.subjections_formed;
        row.subjections_freed         = traced.subjections_freed;
        row.tribute_remitted          = traced.tribute_remitted;
        row.treasury_spent_on_navies          = traced.treasury_spent_on_navies;
        row.treasury_spent_on_ports           = traced.treasury_spent_on_ports;
        row.treasury_spent_on_standing_armies = traced.treasury_spent_on_standing_armies;

        {
            std::vector<std::pair<uint16_t, uint16_t>> active_pairs;
            for (const dated_object& o : traced.dated_objects)
                if (o.kind == static_cast<int32_t>(treaty_clause::non_aggression))
                    active_pairs.push_back({o.a, o.b});
            for (const auto& pr : active_pairs)
            {
                for (const dated_object& o : traced.dated_objects)
                    if (o.kind == static_cast<int32_t>(treaty_clause::non_aggression)
                     && o.a == pr.first && o.b == pr.second)
                    {
                        row.treaty_years_left.push_back(o.expires_year - (ep2.start_year + traced.years));
                        break;
                    }
            }
        }

        // BL-953: the live preference at the traced run's own close, derived
        // once, for reading 6's want-divergence count and reading 10's live line.
        const std::vector<culture_good_preference> live_prefs = derive_culture_preference(
            ss_copy.regions, traced.contacts, traced.polities,
            static_cast<int>(cs_copy.cultures.size()));
        row.live_pref_entries_1660 = static_cast<int64_t>(live_prefs.size());
        const auto top_want = [&](int polity_id) {
            const region_class goods[4] = { region_class::farm, region_class::ore,
                                            region_class::energy, region_class::port };
            int best = -1, best_q = 0;
            for (int g = 0; g < 4; ++g)
            {
                const int w = polity_good_want_q(ss_copy.regions, traced.polities, live_prefs,
                                                 polity_id, goods[g]);
                if (w > best_q) { best_q = w; best = g; } // ties keep the lower good index
            }
            return best;
        };

        for (const polity& q : traced.polities)
        {
            if (!q.alive) continue;
            ++row.alive_polities;
            if (q.overlord >= 0 && static_cast<std::size_t>(q.overlord) < traced.polities.size()
             && traced.polities[static_cast<std::size_t>(q.overlord)].alive)
            {
                ++row.subjects_alive;
                if (q.capital >= 0
                 && traced.polities[static_cast<std::size_t>(q.overlord)].capital >= 0
                 && static_cast<std::size_t>(q.capital) < ss_copy.regions.size()
                 && static_cast<std::size_t>(traced.polities[static_cast<std::size_t>(q.overlord)].capital)
                        < ss_copy.regions.size())
                {
                    const region& sc = ss_copy.regions[static_cast<std::size_t>(q.capital)];
                    const region& oc = ss_copy.regions[static_cast<std::size_t>(
                        traced.polities[static_cast<std::size_t>(q.overlord)].capital)];
                    int dc = std::abs(sc.col - oc.col), dr = std::abs(sc.row - oc.row);
                    row.max_subject_distance = std::max<int64_t>(row.max_subject_distance,
                                                                   std::max(dc, dr));
                }
                // Reading 6: does this subject know a THIRD polity its
                // overlord has never met?
                bool friction = false;
                for (const contact& c : traced.contacts)
                {
                    if (c.from != q.id) continue;
                    if (c.to == q.overlord) continue;
                    if (!contact_exists(traced.contacts, q.overlord, c.to)) { friction = true; break; }
                }
                if (friction) ++row.subjects_with_friction;
                if (top_want(q.id) != top_want(q.overlord)) ++row.subjects_with_divergent_top_want;
            }
        }
        {
            std::vector<bool> is_overlord(traced.polities.size(), false);
            for (const polity& q : traced.polities)
                if (q.alive && q.overlord >= 0 && static_cast<std::size_t>(q.overlord) < is_overlord.size())
                    is_overlord[static_cast<std::size_t>(q.overlord)] = true;
            for (bool v : is_overlord) if (v) ++row.overlords_alive;
        }
        {
            std::vector<int> expn_rank, cons_rank;
            exploration_lean_ranks(traced.polities, &cs_copy, expn_rank, cons_rank);
            for (std::size_t p = 0; p < traced.polities.size(); ++p)
            {
                const polity& q = traced.polities[p];
                if (!q.alive || q.navy_stock <= 0) continue;
                ++row.navy_holders;
                if (expn_rank[p] >= 500) ++row.navy_holders_expn_top;
                else                     ++row.navy_holders_expn_bottom;
            }
            row.port_steps = traced.port_steps_bought;
            row.navy_steps = traced.navy_steps_bought;
            row.army_steps = traced.army_steps_bought;
            for (uint8_t v : traced.navy_lapsed) if (v) ++row.navies_lapsed;

            const std::size_t np = traced.polities.size();
            std::vector<int64_t> held(np, 0), standing(np, 0), garrison(np, 0);
            for (const region& r : ss_copy.regions)
            {
                if (r.nation < 0 || static_cast<std::size_t>(r.nation) >= np) continue;
                const std::size_t n = static_cast<std::size_t>(r.nation);
                ++held[n];
                standing[n] += standing_army_heads(r);
                garrison[n] += garrison_target(r, ep2.garrison_fraction_q);
            }
            for (std::size_t p = 0; p < np; ++p)
            {
                const polity& q = traced.polities[p];
                if (!q.alive || held[p] <= 0) continue;
                row.standing_per_region.push_back(standing[p] / held[p]);
                row.garrison_per_region.push_back(garrison[p] / held[p]);
                if (standing[p] > 0) ++row.standing_holders;
                if (q.capital >= 0 && static_cast<std::size_t>(q.capital) < ss_copy.regions.size()
                 && standing_army_heads(ss_copy.regions[static_cast<std::size_t>(q.capital)])
                        > ep2.army_saturation_per_region * held[p])
                    ++row.army_saturated;
            }
        }

        // --- Reading 3 capture, off the traced re-run's 1660 close ---------
        // (`ss_copy`/`cs_copy` as the re-run left them).
        //
        // BL-951 — WHICH REALMS ARE "STRONGEST". Held region count favours
        // expansion by construction: a consolidator holds the same ground
        // worked harder, so a count of ground cannot see it. The PRIMARY
        // ranking is therefore the capital's `treasury` (EXPLORATION.md: the
        // treasury sits at the capital seat and is fed by what the polity
        // already holds and moves), tie-broken by mean `network_supply_q`
        // over held ground (how well the network feeds what it holds), then
        // by id. Region count is kept as a printed COMPARISON ranking.
        {
            const std::size_t np = traced.polities.size();
            std::vector<int64_t> region_count(np, 0), supply_sum(np, 0);
            for (const region& r : ss_copy.regions)
                if (r.nation >= 0 && static_cast<std::size_t>(r.nation) < np)
                {
                    ++region_count[static_cast<std::size_t>(r.nation)];
                    supply_sum[static_cast<std::size_t>(r.nation)] += r.network_supply_q;
                }

            std::vector<exploration_row::strength_entry> entries;
            for (std::size_t p = 0; p < np; ++p)
            {
                const polity& q = traced.polities[p];
                if (!q.alive) continue;
                exploration_row::strength_entry e;
                e.id      = static_cast<int>(p);
                e.regions = region_count[p];
                e.mean_supply_q = region_count[p] > 0
                    ? static_cast<int>(supply_sum[p] / region_count[p]) : 0;
                if (q.capital >= 0 && static_cast<std::size_t>(q.capital) < ss_copy.regions.size())
                    e.treasury = ss_copy.regions[static_cast<std::size_t>(q.capital)].treasury;
                if (q.culture >= 0 && static_cast<std::size_t>(q.culture) < cs_copy.cultures.size())
                {
                    const culture& cu = cs_copy.cultures[static_cast<std::size_t>(q.culture)];
                    e.cons_q = consolidator_lean_q(cu);
                    e.expn_q = expansion_lean_q(cu);
                }
                else
                {
                    e.cons_q = e.expn_q = -1; // no culture: neither lean, excluded below
                }
                entries.push_back(e);
            }

            if (entries.size() >= 2)
            {
                const std::size_t top_n = std::min<std::size_t>(3, entries.size());
                auto take_top = [&](std::vector<exploration_row::strength_entry> v,
                                    bool by_treasury,
                                    std::vector<exploration_row::strength_entry>& out,
                                    bool& has_cons, bool& has_expn) {
                    std::sort(v.begin(), v.end(), [&](const auto& a, const auto& b) {
                        if (by_treasury)
                        {
                            if (a.treasury != b.treasury) return a.treasury > b.treasury;
                            if (a.mean_supply_q != b.mean_supply_q) return a.mean_supply_q > b.mean_supply_q;
                        }
                        else if (a.regions != b.regions) return a.regions > b.regions;
                        return a.id < b.id; // explicit tie-break
                    });
                    for (std::size_t k = 0; k < top_n; ++k)
                    {
                        out.push_back(v[k]);
                        if (v[k].cons_q > v[k].expn_q)      has_cons = true;
                        else if (v[k].expn_q > v[k].cons_q) has_expn = true;
                    }
                };
                take_top(entries, true,  row.top_by_treasury,
                         row.top_has_consolidator, row.top_has_expansionist);
                take_top(entries, false, row.top_by_regions,
                         row.regions_top_has_consolidator, row.regions_top_has_expansionist);
                row.strength_measured = true;
            }
        }

        // --- BL-954 -- reading 11, off the traced re-run's final round ----
        {
            const std::vector<int32_t> mass =
                label_landmasses(fx.terrain.substrate, fx.gw, fx.gh);
            const auto mass_of_capital = [&](int pid) -> int32_t {
                if (pid < 0 || static_cast<std::size_t>(pid) >= traced.polities.size()) return -1;
                const int cap = traced.polities[static_cast<std::size_t>(pid)].capital;
                if (cap < 0 || static_cast<std::size_t>(cap) >= ss_copy.regions.size()) return -1;
                const region& rg = ss_copy.regions[static_cast<std::size_t>(cap)];
                if (rg.col < 0 || rg.row < 0 || rg.col >= fx.gw || rg.row >= fx.gh) return -1;
                return mass[static_cast<std::size_t>(rg.row * fx.gw + rg.col)];
            };

            std::vector<std::pair<uint16_t, uint16_t>> bound;
            for (const dated_object& o : traced.dated_objects)
                if (o.kind == static_cast<int32_t>(treaty_clause::trade_access))
                    bound.push_back({static_cast<uint16_t>(std::min(o.a, o.b)),
                                     static_cast<uint16_t>(std::max(o.a, o.b))});
            std::sort(bound.begin(), bound.end());
            bound.erase(std::unique(bound.begin(), bound.end()), bound.end());
            row.trade_bound_pairs = static_cast<int64_t>(bound.size());
            row.pair_volume.assign(bound.size(), 0);

            for (const trade_flow& f : traced.trade_flows)
            {
                ++row.flow_count;
                row.flow_volume += f.volume_q;
                const std::pair<uint16_t, uint16_t> key{std::min(f.seller, f.buyer),
                                                        std::max(f.seller, f.buyer)};
                const auto it = std::lower_bound(bound.begin(), bound.end(), key);
                if (it == bound.end() || *it != key) ++row.flows_without_clause;
                else row.pair_volume[static_cast<std::size_t>(it - bound.begin())] += f.volume_q;
                const int32_t ms = mass_of_capital(f.seller), mb = mass_of_capital(f.buyer);
                if (ms < 0 || mb < 0)  row.unknown_landmass_volume += f.volume_q;
                else if (ms != mb)     row.cross_landmass_volume   += f.volume_q;
            }
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
    // Read off each seed's own (untraced, real) exploration run, captured
    // onto the row in the main loop.
    // -----------------------------------------------------------------------
    {
        std::vector<int32_t> uses;
        int64_t total_post_roads_built = 0, total_treasury_spent = 0;
        int seeds_with_post_road = 0;
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            uses.insert(uses.end(), r.corridor_uses.begin(), r.corridor_uses.end());
            if (r.post_roads_built > 0) ++seeds_with_post_road;
            total_post_roads_built += r.post_roads_built;
            total_treasury_spent   += r.treasury_spent_on_roads;
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
    // READING 4 — TREATY DEPTH (BL-933).
    // -----------------------------------------------------------------------
    std::printf("\n--- reading 4: treaty depth ---\n");
    {
        int64_t formed = 0, broken = 0, blocked_campaigns = 0;
        std::vector<int64_t> years_left;
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            formed += r.treaties_formed_total;
            broken += r.treaties_broken_total;
            blocked_campaigns += r.treaty_blocked_campaigns;
            for (int64_t yl : r.treaty_years_left) years_left.push_back(yl);
        }
        std::printf("  treaties formed=%lld broken=%lld  non-aggression-blocked campaigns=%lld  "
                    "(across %d seeds)\n",
                    static_cast<long long>(formed), static_cast<long long>(broken),
                    static_cast<long long>(blocked_campaigns), seed_count);
        if (years_left.empty())
        {
            std::printf("  no treaty stood at 1660 on this spread — NOT MEASURED (or the "
                        "formation threshold sat too high; see treaty_formation_threshold_q).\n");
        }
        else
        {
            std::sort(years_left.begin(), years_left.end());
            std::printf("  %zu standing at 1660, years-left min=%lld median=%lld max=%lld\n",
                        years_left.size(), static_cast<long long>(years_left.front()),
                        static_cast<long long>(years_left[years_left.size() / 2]),
                        static_cast<long long>(years_left.back()));
            std::printf("  %s\n", years_left.size() > 0 && broken > 0
                ? "treaties both stand AND break on this spread — depth without a frozen map."
                : "treaties stand but none broke on this spread — report to Ben, do not re-tune silently.");
        }
    }

    // -----------------------------------------------------------------------
    // READING 5 — COLONIAL ASYMMETRY (BL-934).
    // -----------------------------------------------------------------------
    std::printf("\n--- reading 5: colonial asymmetry ---\n");
    {
        int64_t subjects = 0, overlords = 0, alive = 0, formed = 0, freed = 0, tribute = 0;
        int64_t max_dist = 0;
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            subjects += r.subjects_alive; overlords += r.overlords_alive; alive += r.alive_polities;
            formed += r.subjections_formed; freed += r.subjections_freed; tribute += r.tribute_remitted;
            max_dist = std::max(max_dist, r.max_subject_distance);
        }
        std::printf("  subjects=%lld overlords=%lld alive-polities=%lld (formed=%lld freed=%lld) "
                    "tribute remitted total=%lld  max subject-overlord distance=%lld\n",
                    static_cast<long long>(subjects), static_cast<long long>(overlords),
                    static_cast<long long>(alive), static_cast<long long>(formed),
                    static_cast<long long>(freed), static_cast<long long>(tribute),
                    static_cast<long long>(max_dist));
        std::printf("  %s\n", (subjects > 0 && subjects < alive)
            ? "some polities hold subjects, most do not — asymmetry measured."
            : "no asymmetry measured on this spread — report to Ben (subjection_reach_q / "
              "subjection_treasury_margin_q may sit wrong for this seed spread).");
    }

    // -----------------------------------------------------------------------
    // READING 6 — SUBJECT FRICTION (BL-934), OPERATIONALIZED.
    // -----------------------------------------------------------------------
    //
    // No live want table is maintained during the span (§ file header); a
    // subject's CONTACT graph diverging from its overlord's is read as the
    // proxy, since contact is the want table's own precondition
    // (CIVILISATION.md sec The directed want: "a want requires contact").
    // Flagged exactly as readings 1-2's own operationalization is.
    std::printf("\n--- reading 6: subject friction (operationalized as contact-graph divergence) ---\n");
    {
        int64_t friction = 0, divergent = 0, subjects = 0;
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            friction  += r.subjects_with_friction;
            divergent += r.subjects_with_divergent_top_want;
            subjects  += r.subjects_alive;
        }
        std::printf("  subjects whose contact graph names a polity their overlord never met: %lld\n",
                    static_cast<long long>(friction));
        std::printf("  subjects whose top want (argmax good, live 1660 preference) differs from "
                    "their overlord's: %lld of %lld\n",
                    static_cast<long long>(divergent), static_cast<long long>(subjects));
        std::printf("  %s\n", friction > 0
            ? "at least one subject's own graph points somewhere its overlord's does not."
            : "no friction measured on this spread — NOT MEASURED, do not read as a failure "
              "without more seeds (this reading depends on subjects existing at all).");
    }

    // -----------------------------------------------------------------------
    // READING 7 — FLEETS (BL-935).
    // -----------------------------------------------------------------------
    std::printf("\n--- reading 7: fleets ---\n");
    {
        int64_t navy_holders = 0, spent_navies = 0, spent_ports = 0, spent_armies = 0;
        int64_t top = 0, bottom = 0, port_steps = 0, navy_steps = 0, army_steps = 0, lapsed = 0;
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            navy_holders += r.navy_holders;
            spent_navies += r.treasury_spent_on_navies;
            spent_ports  += r.treasury_spent_on_ports;
            spent_armies += r.treasury_spent_on_standing_armies;
            top          += r.navy_holders_expn_top;
            bottom       += r.navy_holders_expn_bottom;
            port_steps   += r.port_steps;
            navy_steps   += r.navy_steps;
            army_steps   += r.army_steps;
            lapsed       += r.navies_lapsed;
        }
        std::printf("  polities holding a navy at 1660: %lld  treasury spent -- ports=%lld "
                    "navies=%lld standing armies=%lld\n",
                    static_cast<long long>(navy_holders), static_cast<long long>(spent_ports),
                    static_cast<long long>(spent_navies), static_cast<long long>(spent_armies));
        // BL-955 -- who holds the fleets, what was bought, and what was let go.
        std::printf("  navies at 1660 by holder's expansion rank (1660): top half (>=500)=%lld  "
                    "bottom half=%lld\n", static_cast<long long>(top), static_cast<long long>(bottom));
        std::printf("  stock steps bought over the span: port=%lld navy=%lld standing army=%lld\n",
                    static_cast<long long>(port_steps), static_cast<long long>(navy_steps),
                    static_cast<long long>(army_steps));
        std::printf("  polities that held a navy and let it decay to zero at least once: %lld\n",
                    static_cast<long long>(lapsed));
        {
            std::vector<int64_t> st, ga;
            int64_t holders = 0, saturated = 0, living = 0;
            for (const exploration_row& r : rows)
            {
                if (!r.ok) continue;
                st.insert(st.end(), r.standing_per_region.begin(), r.standing_per_region.end());
                ga.insert(ga.end(), r.garrison_per_region.begin(), r.garrison_per_region.end());
                holders   += r.standing_holders;
                saturated += r.army_saturated;
            }
            living = static_cast<int64_t>(st.size());
            const auto pct = [](std::vector<int64_t> v, int p) -> long long {
                if (v.empty()) return 0;
                std::sort(v.begin(), v.end());
                return static_cast<long long>(v[(v.size() - 1) * static_cast<std::size_t>(p) / 100]);
            };
            std::printf("  paid standing army at 1660, heads per held region over %lld living polities: "
                        "p50=%lld p75=%lld p90=%lld max=%lld  (holders=%lld, seat above cap=%lld)\n",
                        static_cast<long long>(living), pct(st, 50), pct(st, 75), pct(st, 90),
                        pct(st, 100), static_cast<long long>(holders), static_cast<long long>(saturated));
            std::printf("  garrison_target per held region: p50=%lld p75=%lld p90=%lld max=%lld\n",
                        pct(ga, 50), pct(ga, 75), pct(ga, 90), pct(ga, 100));
        }
        std::printf("  %s\n", spent_navies > 0
            ? "at least one polity funded a navy on this spread (decay itself is confirmed by "
              "exploration_sim_harness R5.8/R5.9, not by this aggregate sweep)."
            : "no polity ever funded a navy on this spread — report to Ben rather than "
              "re-tuning navy_build_cost_q/navy_min_port_stock_q silently.");
    }

    // -----------------------------------------------------------------------
    // READING 3 — BOTH STRATEGIES PAY (BL-942). Consolidators AND
    // expansionists both among a seed's strongest realms, each traceable to
    // its creed's recorded deeds (`zeal`/`dominion`/`sea_legs_q`).
    // "Strongest" is ranked by CAPITAL TREASURY at 1660, tie-broken by mean
    // held-region `network_supply_q` (BL-951 — see the capture's comment for
    // why region count cannot see a consolidator). The region-count ranking
    // is printed beside it for comparison; the verdict line reads the
    // treasury ranking.
    // -----------------------------------------------------------------------
    std::printf("\n--- reading 3: both strategies pay (consolidator/expansionist, by creed) ---\n");
    std::printf("  metric: top-3 realms per seed ranked by CAPITAL TREASURY at 1660, tie-break mean "
                "held-region network_supply_q, then polity id (region-count ranking shown for comparison)\n");
    {
        auto lean_tag = [](const exploration_row::strength_entry& e) {
            return e.cons_q > e.expn_q ? "C" : (e.expn_q > e.cons_q ? "E" : "-");
        };
        auto print_top = [&](const char* label, const std::vector<exploration_row::strength_entry>& v) {
            std::printf("    %-9s", label);
            for (const auto& e : v)
                std::printf("  [p%d %s trs=%lld sup=%d reg=%lld cons=%d expn=%d]", e.id, lean_tag(e),
                            static_cast<long long>(e.treasury), e.mean_supply_q,
                            static_cast<long long>(e.regions), e.cons_q, e.expn_q);
            std::printf("\n");
        };

        int64_t seeds_measured = 0, seeds_with_consolidator_top = 0,
                seeds_with_expansionist_top = 0, seeds_with_both = 0;
        int64_t reg_cons = 0, reg_expn = 0, reg_both = 0;
        for (const exploration_row& r : rows)
        {
            if (!r.ok || !r.strength_measured) continue;
            ++seeds_measured;
            if (r.top_has_consolidator) ++seeds_with_consolidator_top;
            if (r.top_has_expansionist) ++seeds_with_expansionist_top;
            if (r.top_has_consolidator && r.top_has_expansionist) ++seeds_with_both;
            if (r.regions_top_has_consolidator) ++reg_cons;
            if (r.regions_top_has_expansionist) ++reg_expn;
            if (r.regions_top_has_consolidator && r.regions_top_has_expansionist) ++reg_both;

            std::printf("  seed %u (C = consolidator-leaning creed, E = expansionist, - = neither):\n", r.seed);
            print_top("treasury:", r.top_by_treasury);
            print_top("regions:",  r.top_by_regions);
        }
        std::printf("  seeds measured=%lld  top-3-by-TREASURY include a consolidator-leaning "
                    "creed=%lld  include an expansionist-leaning creed=%lld  BOTH present=%lld\n",
                    static_cast<long long>(seeds_measured), static_cast<long long>(seeds_with_consolidator_top),
                    static_cast<long long>(seeds_with_expansionist_top), static_cast<long long>(seeds_with_both));
        std::printf("  (comparison) top-3-by-REGIONS include a consolidator-leaning creed=%lld  "
                    "include an expansionist-leaning creed=%lld  BOTH present=%lld\n",
                    static_cast<long long>(reg_cons), static_cast<long long>(reg_expn),
                    static_cast<long long>(reg_both));
        std::printf("  %s\n", seeds_with_both > 0
            ? "at least one seed's strongest realms include both a consolidator and an "
              "expansionist, each traceable to its own creed."
            : "no seed on this spread shows both strategies among its strongest realms -- "
              "report to Ben rather than forcing the mapping (EXPLORATION.md sec Two ways to be "
              "strong: if the creed axes do not separate them, the fix is upstream, not a flag here).");
    }

    // -----------------------------------------------------------------------
    // READING 10 — PREFERENCE (BL-936). Goods wanted differently by
    // different cultures, with the difference traceable to route. Read off
    // the PRE-EXPLORATION handoff state, captured onto the row in the main
    // loop (`derive_culture_preference` over the 1200 CE fixture).
    // -----------------------------------------------------------------------
    std::printf("\n--- reading 10: preference (goods wanted differently by culture, by route) ---\n");
    {
        int64_t live_entries = 0;
        for (const exploration_row& r : rows) if (r.ok) live_entries += r.live_pref_entries_1660;
        std::printf("  LIVE at 1660 (the preference the span's scorer reads, off the traced run): "
                    "total (culture, good) entries=%lld\n", static_cast<long long>(live_entries));
    }
    {
        int64_t total_entries = 0, seeds_with_spread = 0, seeds_measured = 0;
        std::vector<int16_t> weights;
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            ++seeds_measured;

            total_entries += static_cast<int64_t>(r.preference_weights.size());
            const std::vector<int16_t>& seed_weights = r.preference_weights;
            if (!seed_weights.empty())
            {
                const auto mm = std::minmax_element(seed_weights.begin(), seed_weights.end());
                if (*mm.second > *mm.first) ++seeds_with_spread;
            }
            weights.insert(weights.end(), seed_weights.begin(), seed_weights.end());
        }
        std::printf("  seeds measured=%lld  total (culture, good) preference entries=%lld  "
                    "seeds with a non-uniform weight spread=%lld\n",
                    static_cast<long long>(seeds_measured), static_cast<long long>(total_entries),
                    static_cast<long long>(seeds_with_spread));
        if (weights.empty())
        {
            std::printf("  no (culture, good) preference entry derived on this spread -- NOT "
                        "MEASURED (no contact-exposed absence existed at 1200 CE on any seed).\n");
        }
        else
        {
            std::sort(weights.begin(), weights.end());
            std::printf("  weight_q across the spread: min=%d median=%d max=%d\n",
                        weights.front(), weights[weights.size() / 2], weights.back());
            std::printf("  %s\n", (weights.back() > weights.front())
                ? "goods are weighted differently by different cultures on this spread."
                : "every derived preference weighs the same -- no spread measured.");
        }
    }

    // -----------------------------------------------------------------------
    // READING 11 — TRADE (BL-954). "Flow crosses between polities on
    // different landmasses, unevenly — some pairs carry most of it, most
    // carry none — and every flow stands on a trade-access clause." Read off
    // the traced re-run's `trade_flows`, which hold the span's FINAL decision
    // round (flows are rebuilt each round, never accumulated).
    // -----------------------------------------------------------------------
    std::printf("\n--- reading 11: trade (flows at the final decision round) ---\n");
    {
        std::printf("%-6s %8s %10s %10s %10s %10s %12s\n",
                    "seed", "flows", "volume", "cross-mass", "unk-mass", "tr-pairs", "pairs-w/flow");
        int64_t flows = 0, volume = 0, cross = 0, unknown = 0, orphan = 0;
        std::vector<int64_t> pooled_pairs;
        for (const exploration_row& r : rows)
        {
            if (!r.ok) continue;
            int64_t with_flow = 0;
            for (int64_t v : r.pair_volume) if (v > 0) ++with_flow;
            std::printf("%-6u %8lld %10lld %10lld %10lld %10lld %12lld\n", r.seed,
                        static_cast<long long>(r.flow_count), static_cast<long long>(r.flow_volume),
                        static_cast<long long>(r.cross_landmass_volume),
                        static_cast<long long>(r.unknown_landmass_volume),
                        static_cast<long long>(r.trade_bound_pairs), static_cast<long long>(with_flow));
            flows += r.flow_count; volume += r.flow_volume; cross += r.cross_landmass_volume;
            unknown += r.unknown_landmass_volume; orphan += r.flows_without_clause;
            pooled_pairs.insert(pooled_pairs.end(), r.pair_volume.begin(), r.pair_volume.end());
        }
        std::printf("  total: flows=%lld volume=%lld  cross-landmass share=%.3f  "
                    "(capital-on-water, landmass undefined: %.3f)\n",
                    static_cast<long long>(flows), static_cast<long long>(volume),
                    volume > 0 ? static_cast<double>(cross) / static_cast<double>(volume) : 0.0,
                    volume > 0 ? static_cast<double>(unknown) / static_cast<double>(volume) : 0.0);

        if (!pooled_pairs.empty() && volume > 0)
        {
            std::sort(pooled_pairs.begin(), pooled_pairs.end(), std::greater<int64_t>());
            const std::size_t decile = std::max<std::size_t>(1, (pooled_pairs.size() + 9) / 10);
            int64_t top = 0, zero = 0;
            for (std::size_t k = 0; k < decile; ++k) top += pooled_pairs[k];
            for (int64_t v : pooled_pairs) if (v == 0) ++zero;
            std::printf("  %zu trade-access pairs pooled: top-decile (%zu pairs) share of volume=%.3f  "
                        "pairs carrying none=%lld (%.3f)\n",
                        pooled_pairs.size(), decile,
                        static_cast<double>(top) / static_cast<double>(volume),
                        static_cast<long long>(zero),
                        static_cast<double>(zero) / static_cast<double>(pooled_pairs.size()));
        }
        else
        {
            std::printf("  no trade volume on this spread -- NOT MEASURED; report to Ben rather than "
                        "re-tuning a weight silently.\n");
        }
        std::printf("  flows whose pair holds no trade_access clause at 1660: %lld\n",
                    static_cast<long long>(orphan));
        check(orphan == 0, "reading 11: every flow stands on a trade_access clause");
    }

    std::printf("\n%d failure(s) in structural checks.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
