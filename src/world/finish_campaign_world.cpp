#include "world/finish_campaign_world.hpp"

#include "world/campaign_settle.hpp"
#include "world/corporation_generation.hpp" // assign_default_recipes
#include "world/economy_system.hpp"        // economy_step_phase_clock (BL-1117)
#include "world/era_timelapse.hpp"          // lapse_event_kind (BL-1278 lane C)
#include "world/hard_coded_world.hpp"       // generation_progress, generation_step_cost_ms
#include "world/history_log.hpp"            // seed_genesis_history
#include "world/recipe_registry.hpp"
#include "world/road_generation.hpp"        // BL-1347: lay_site_roads, after the stampers
#include "world/survey_system.hpp"          // init_survey_states
#include "world/world.hpp"
#include "world/world_gen_config.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>

namespace {

/// The two captioned steps this file adds to a wait (hard_coded_world.hpp's
/// label table): the search and the settle.
constexpr int k_label_search = 16; // "Searching the landscape"
constexpr int k_label_settle = 17; // "Proving the field"

using fin_clock = std::chrono::steady_clock;

std::int64_t ms_between(fin_clock::time_point a, fin_clock::time_point b)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
}

/// Enter a captioned step on the caller's sink, exactly as generation enters
/// its own (`enter_step` in hard_coded_world.cpp): fold the finished step's
/// weight into the sum, publish the new step's weight and caption, clear the
/// inner bar first. Null-safe; nothing is read back into the world.
void enter_step(generation_progress* p, int label, const world_params& params)
{
    if (p == nullptr) return;
    p->sub_total.store(0, std::memory_order_relaxed);
    p->sub_progress.store(0, std::memory_order_relaxed);
    p->weight_done.store(p->weight_done.load(std::memory_order_relaxed)
                             + p->weight_now.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);
    p->weight_now.store(generation_step_cost_ms(label, params), std::memory_order_relaxed);
    p->label.store(label, std::memory_order_relaxed);
}

/// The wait is whole: the bar reaches its end.
void close_steps(generation_progress* p)
{
    if (p == nullptr) return;
    p->sub_total.store(0, std::memory_order_relaxed);
    p->weight_now.store(0, std::memory_order_relaxed);
    p->weight_done.store(p->weight_total.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);
}

void report_step_ms(generation_progress* p, int label, std::int64_t ms)
{
    if (p == nullptr) return;
    p->ms_step[static_cast<std::size_t>(label)].store(static_cast<int32_t>(ms),
                                                      std::memory_order_relaxed);
}

/// BL-1278 lane C -- the majors' corrections, written into the homeworld's
/// Industrialisation record as history events DATED BACK into its closing
/// round: the year of its last recorded step (else its last year; else, on a
/// world the span never ran for, the epoch is all there is and the events go
/// unwritten -- the corrections still stand in the world and the report).
/// `polity` / `other` name the realms through the polity fold's own record
/// (`body_entry::nation_polity`); a nation the fold never named takes
/// `lapse_event_none`. Inserted at their year's end, so the record stays
/// ascending. Returns the year written (0 when none was).
std::int32_t record_major_events(generation_report& report, const world& w,
                                 const major_reconciliation_report& rec)
{
    if (rec.records.empty())
        return 0;
    generation_report::body_entry* home = nullptr;
    for (generation_report::body_entry& be : report.bodies)
        if (be.is_homeworld)
        {
            home = &be;
            break;
        }
    if (home == nullptr)
        return 0;
    era_timelapse& t = home->industrialisation_timelapse;
    if (t.years <= 0 && t.steps.empty())
        return 0;
    const std::int32_t year = !t.steps.empty() ? t.steps.back().year : t.start_year + t.years;
    const auto polity_of = [&](entity_id n) -> std::uint16_t {
        if (n == null_entity)
            return lapse_event_none;
        for (std::size_t i = 0; i < home->nation_ids.size() && i < home->nation_polity.size(); ++i)
            if (home->nation_ids[i] == n)
            {
                const std::int32_t p = home->nation_polity[i];
                return (p >= 0 && p < static_cast<std::int32_t>(lapse_event_none))
                           ? static_cast<std::uint16_t>(p) : lapse_event_none;
            }
        return lapse_event_none;
    };
    (void)w;
    for (const major_reconciliation_record& r : rec.records)
    {
        lapse_event e;
        e.year   = year;
        e.kind   = r.kind;
        e.region = (r.region >= 0 && r.region < static_cast<std::int32_t>(lapse_event_none))
                       ? static_cast<std::uint16_t>(r.region) : lapse_event_none;
        e.polity = polity_of(r.nation);
        e.other  = r.kind == static_cast<std::uint8_t>(lapse_event_kind::champion_bought_out)
                       ? polity_of(r.buyer_nation) : lapse_event_none;
        const auto at = std::upper_bound(t.events.begin(), t.events.end(), year,
                                         [](std::int32_t y, const lapse_event& x) { return y < x.year; });
        t.events.insert(at, e);
    }
    return year;
}

} // namespace

era_band campaign_band_from_world(const world& /*w*/)
{
    // EVERY NATION OPENS ON THE INDUSTRIAL BAND (BL-1273, Ben 2026-10-10;
    // ERAS.md § Where the ladder starts). The world's stored band (BL-1101's
    // history-derived verdict) is no longer read: a seed whose history never
    // industrialised opens on the industrial roster like every other, and the
    // asymmetry between starts lives in geography, history, population and the
    // corporate web, never in tech (RESEARCH.md § Who holds technology).
    return era_band::industrial;
}

landscape_search_params campaign_search_params(std::uint32_t world_seed, int corporation_count)
{
    landscape_search_params sp;
    sp.regenerate_specialists  = true;
    sp.seed                    = world_seed ^ 0x8A21F00Du;
    sp.start.placement_seed    = sp.seed;
    sp.start.corporation_count = corporation_count;
    // BL-1136 (Ben, 2026-09-26; GENERATION_STRATEGY.md § Phase 6, "The round
    // count is kept at six"): a round's proposals are scored on two threads.
    // A budget world proposes two per round (placement and road tier), so two
    // threads halve the search's wall time; the winner, every scored term and
    // the whole path are the serial walk's, bit for bit (the determinism
    // clauses in landscape_search.hpp; `landscape_search_harness --curve
    // --check-k 6 --check-threads 2` proves it on the curated seeds, all six
    // rounds -- --check-k defaults to 3, which compares only half the walk --
    // and the harness's default run holds a threaded budget-world row, R6).
    sp.thread_count            = k_campaign_search_threads;
    return sp;
}

std::int64_t finish_campaign_weight_ms(const world_params& params)
{
    return generation_step_cost_ms(k_label_search, params)
         + generation_step_cost_ms(k_label_settle, params);
}

finish_campaign_result finish_campaign_world(world& w, generation_report& report,
                                             recipe_registry& reg,
                                             const world_params& params,
                                             const world_gen_config& cfg,
                                             generation_progress* progress)
{
    finish_campaign_result out;

    // 1. The world-only halves of setup. Bridge PLANETOLOGY's per-body dated
    //    history and checkpoints into the world history log's genesis and
    //    checkpoint chapters (BL-208): exactly once per generation, it is not
    //    idempotent. Then the survey states (BL-067): home and the star open
    //    surveyed, every other body hidden until a survey is dispatched.
    seed_genesis_history(w, report);
    init_survey_states(w);

    // 2. The band -- after the load (which resets the band to `any`) and
    //    before anything browses recipes; the default-recipe pass below is the
    //    first such reader. Ids are untouched: the filter masks, it does not
    //    remove.
    reg.set_era(campaign_band_from_world(w));

    // 3. Author processing recipes onto the generated assets. The recipe id is
    //    a registry index, unknown at generation time, so it is assigned once
    //    the registry exists. A world-generation invariant that used to be
    //    enforced by the UI's startup sequence (2026-08-17), which is how every
    //    headless path once ran processors that could never produce.
    //    BL-1217 D6: an unwanted default unplaces its processor instead.
    assign_default_recipes(w, reg, "finish: before the search");

    // 4. PHASE 6 -- the landscape is SEARCHED, not simply generated (BL-770).
    //    Static scores over candidate landscapes -- no clock, no ticks -- and
    //    the winner applied by a deterministic argmax (GENERATION_STRATEGY.md
    //    § The eight phases). ALL THREE AXES ARE LIVE (BL-977): every candidate
    //    REPLACES the world-gen specialists and lays its background firms; the
    //    winner is applied the same way. The seat is drawn AFTER this from the
    //    surviving specialists, so replacing the roster here orphans nothing.
    //
    //    THE CHARTER BUDGET (BL-1042): the world's own industry-point
    //    stockpile (Beat 1, INDUSTRIALISATION.md Part III), split over the
    //    carved centres by `build_stockpile_budget` and charged at
    //    `stockpile_charter_spend` -- its firm price DERIVED from the whole
    //    stockpile (BL-1064, NR-907), its other constants named and ruled in
    //    stockpile_budget.hpp (NR-910). ONE budget and ONE spend reach BOTH the
    //    search and the winner's apply, or the search would score one world and
    //    the app lay another. A budget no centre can buy a specialist with
    //    falls back to the no-budget world in both (NR-910); on a world the
    //    span did not run on the budget is EMPTY and the apply's legacy branch
    //    runs first.
    enter_step(progress, k_label_search, params);
    const fin_clock::time_point t_search = fin_clock::now(); // reported only
    landscape_search_params sp = campaign_search_params(params.seed, cfg.corporation_count);
    sp.progress   = progress; // the inner bar, per evaluation (write-only)
    out.stockpile = build_stockpile_budget(w);
    out.spend     = stockpile_charter_spend(out.stockpile);
    sp.budget     = &out.stockpile.budget;
    sp.spend      = out.spend;
    out.search    = search_landscape(w, reg, sp);
    apply_landscape_candidate(w, reg, out.search.winner, /*regenerate_specialists=*/true,
                              &out.stockpile.budget, out.spend, &out.charter);
    // BL-1278 lane C -- THE MAJORS RECONCILED AFTER THE WINNER IS APPLIED
    // (NATION_GENERATION.md § The nation count is a target, Ben 2026-10-11): a
    // major is a nation holding a champion, and champions are chartered by the
    // search, so the major target is met HERE -- a champion chartered in the
    // richest minor (a state charter where its treasury makes up the price), or
    // the poorest surplus major demoted to a minor (BL-1404, NR-1065). Each
    // correction is written into the Industrialisation record, dated back into
    // its closing round, so the record reads as one history. Unset (0): nothing
    // runs and nothing moves. Before the charter web is published and before
    // the retrofit, so both read the reconciled roster.
    out.majors = reconcile_majors(w, reg, out.stockpile.budget, out.spend, out.charter,
                                  params.major_target, out.search.winner.placement_seed);
    const std::int32_t reconcile_year = record_major_events(report, w, out.majors);
    // BL-1086 (the review's R4): a budget world laid no roster in generation,
    // so the loading screen's charter ledger is published HERE, from the
    // winner's web as it was just chartered. Write-only; a report the legacy
    // branch left untouched holds no charter and publishes nothing, so a world
    // with no budget keeps the rows generation published.
    publish_charter_web(progress, w, out.charter);
    // BL-1268 -- THE RETROFIT (TRADE.md § Trade in generation): the history's
    // trade record is spent on completed Planetary Marketplaces, on the markets
    // whose trade it carried, owned by the corporations chartered there. After
    // the winner's charters (the owners exist) and before the settle (which
    // trades on them); not inside the search, so no candidate is scored on them.
    out.retrofit = retrofit_marketplaces(w, reg);
    print_marketplace_retrofit(out.retrofit);
    // BL-1099: THE CHARTERS ARE DATED against the Industrialisation record --
    // the cradle's own, the one body the span ran for (every other entry is
    // empty, and an empty record dates every firm at the epoch). Once, on the
    // winner's report, after its apply: the walk stamps origins, this stamps
    // years (`date_chartered_firms`). Read-only on the report; nothing here
    // feeds the search, the settle or a digest.
    {
        const std::vector<lapse_event>* events = nullptr;
        for (const generation_report::body_entry& be : report.bodies)
            if (!be.industrialisation_timelapse.events.empty())
            {
                events = &be.industrialisation_timelapse.events;
                break;
            }
        static const std::vector<lapse_event> none;
        date_chartered_firms(w, out.charter, events ? *events : none,
                             static_cast<int32_t>(params.epoch_year));
    }
    // BL-1278 lane C: a champion the reconciliation chartered was founded the
    // year its event is dated, not on its region's works notes.
    for (const major_reconciliation_record& rec : out.majors.records)
        if (reconcile_year != 0
            && rec.kind != static_cast<std::uint8_t>(lapse_event_kind::champion_bought_out)
            && rec.kind != static_cast<std::uint8_t>(lapse_event_kind::major_demoted)) // BL-1404
            if (const auto c = w.gen_charters.find(rec.corp); c != w.gen_charters.end())
            {
                c->second.founded_year = reconcile_year;
                for (charter_record& cr : out.charter.charters)
                    if (cr.corp == rec.corp)
                        cr.founded_year = reconcile_year;
            }
    if (out.majors.target > 0)
    {
        int chartered = 0, state = 0, bought = 0;
        float shortfall = 0.0f, paid = 0.0f;
        for (const major_reconciliation_record& rec : out.majors.records)
        {
            const auto k = static_cast<lapse_event_kind>(rec.kind);
            chartered += k == lapse_event_kind::champion_chartered;
            state     += k == lapse_event_kind::state_charter;
            bought    += k == lapse_event_kind::major_demoted; // BL-1404: demoted, never bought
            shortfall += rec.treasury_shortfall;
            if (k == lapse_event_kind::state_charter) paid += rec.credits_moved;
        }
        std::printf("[majors] target %d: natural %d -> %d (%d champion charter(s), %d state "
                    "charter(s), %d demotion(s), dated %d)%s",
                    out.majors.target, out.majors.natural, out.majors.majors, chartered, state,
                    bought, static_cast<int>(reconcile_year), out.majors.missed ? " MISSED" : "");
        if (shortfall > 0.0f)
            std::printf("; treasuries paid %.2f of %.2f credits owed (short %.2f)",
                        static_cast<double>(paid), static_cast<double>(paid + shortfall),
                        static_cast<double>(shortfall));
        std::printf("\n");
        std::fflush(stdout);
        // OFF TARGET IS A DEFECT, NEVER A QUIET OUTCOME (NATION_GENERATION.md:
        // both targets are always hit): a reconciliation that could not reach
        // the major target is recorded on the report's handoff verdict, as the
        // history's own handoff validators record theirs, and said on stderr.
        if (out.majors.missed)
        {
            char msg[160];
            std::snprintf(msg, sizeof msg, "majors: target %d, reached %d (no minor left with "
                          "ground for a champion)", out.majors.target, out.majors.majors);
            report.handoff_invalid = true;
            if (!report.handoff_violation.empty())
                report.handoff_violation += "; ";
            report.handoff_violation += msg;
            std::fprintf(stderr, "[handoff] %s\n", msg);
        }
    }
    if (out.charter.champions_fallen_back > 0)
        std::printf("[charter] %d champion(s) fell back to their nation's next affording centre\n",
                    out.charter.champions_fallen_back);
    if (out.stockpile.points_total != 0 || out.stockpile.rejected)
    {
        const stockpile_budget& sb = out.stockpile;
        const auto why = [&](stockpile_unspent_reason k) {
            return static_cast<long long>(sb.unspent[static_cast<std::size_t>(k)]);
        };
        // BL-1064: an empty budget (rejected, or every point unspent) prices nothing.
        char price[192] = "no price (an empty budget)";
        if (!sb.budget.empty())
        {
            // BL-1168: a centre pays its trade reach's price; the world's is the dearest.
            std::int32_t lo = sb.firm_price_points;
            for (const auto& kv : sb.centre_firm_price)
                lo = std::min(lo, kv.second);
            std::snprintf(price, sizeof price,
                          "firm price %d (the stock / %lld), specialist %lld; priced by %s reach "
                          "(%zu reaches, firm price %d..%d, %d centres unreached)",
                          static_cast<int>(sb.firm_price_points),
                          static_cast<long long>(sb.price_divisor),
                          static_cast<long long>(out.spend.specialist_price_points()),
                          charter_price_reach_name(sb.reach), sb.reach_stock.size(),
                          static_cast<int>(lo), static_cast<int>(sb.firm_price_points),
                          sb.centres_unreached);
        }
        std::printf("[stockpile_budget] %lld points: %lld to %zu centres, %lld unspent "
                    "(carve_dropped %lld, carve_no_tile %lld, razed %lld, "
                    "no_carved_centre %lld, rejected %lld)%s%s; %s; spent %lld of %lld%s\n",
                    static_cast<long long>(sb.points_total),
                    static_cast<long long>(sb.points_to_centres),
                    sb.budget.points().size(),
                    static_cast<long long>(sb.points_unspent()),
                    why(stockpile_unspent_reason::carve_dropped),
                    why(stockpile_unspent_reason::carve_no_tile),
                    why(stockpile_unspent_reason::razed),
                    why(stockpile_unspent_reason::no_carved_centre),
                    why(stockpile_unspent_reason::rejected),
                    sb.rejected ? " REJECTED: " : "",
                    sb.rejected ? sb.rejection.c_str() : "",
                    price,
                    static_cast<long long>(out.charter.points_spent),
                    static_cast<long long>(out.charter.points_budgeted),
                    out.charter.refused     ? " (spend REFUSED)"
                    : out.charter.fell_back ? " (NO SPECIALIST affordable: the no-budget "
                                              "world, NR-910)"
                                            : "");
    }
    std::printf("[landscape_search] winner corps=%d placement=%08X tier=%u  "
                "accepted roster=%d placement=%d road_tier=%d of %d rounds\n",
                out.search.winner.corporation_count, out.search.winner.placement_seed,
                static_cast<unsigned>(out.search.winner.road_tier),
                out.search.accepted_by_axis[0], out.search.accepted_by_axis[1],
                out.search.accepted_by_axis[2], sp.rounds);
    std::fflush(stdout);

    // 5. AGAIN, and this one is not belt-and-braces (2026-08-17): the pass
    //    above ran BEFORE the winner laid its firms, so every processor a
    //    background firm authored would keep `no_recipe` for the whole
    //    campaign -- paying maintenance every tick and never producing,
    //    reported as ordinary idleness. Idempotent; one map walk.
    //    BL-1217 D6: an unwanted default unplaces its processor instead.
    assign_default_recipes(w, reg, "finish: after the winner");
    // BL-1296 -- THE HANDOVER (NATIONS.md § State industry, "Handed over at the
    // fold", Ben 2026-10-13): the strategic buildings background firms placed
    // on a major's ground pass to that nation as its state industry; the firms
    // keep the rest and a champion keeps all its own. After the recipes are
    // assigned (the sector is read off the recipe) and before the settle, so
    // the opening position play receives was earned with the state running.
    out.state = hand_over_state_industry(w, reg);
    print_state_handover(out.state);
    // BL-1255 -- THE RAIL HANDOVER (INDUSTRIALISATION.md § Who lays rail): the
    // track the Industrialisation span laid passes to the major whose ground it
    // stands on, or to a minor's background firm, and a station stands at each
    // city a line serves. After the state handover (a station is never a firm's
    // on a major's ground) and before the settle, so the opening position is
    // earned with the rail running.
    out.rail = hand_over_rail(w);
    print_rail_handover(out.rail);
    // PORTS IN HISTORY (Ben, 2026-10-26; LOGISTICS.md § 4b): where a colonial lane lands,
    // the past built a harbour - a port of the nation holding that ground, free. After the
    // rail handover and BEFORE the site roads and the seeding, so the site pass roads it,
    // the seeding's opening `lay_all_port_lanes` lanes it and the chain may carry it.
    out.colonial_ports = place_colonial_ports(w);
    print_colonial_ports(out.colonial_ports);
    // BL-1347 -- A CROSSING TO EVERY SITE STAMPED SINCE GENERATION'S OWN PASS
    // (PRODUCTION.md § A building needs a road; LOGISTICS.md § 4, "Generation roads every
    // building site"). Generation roaded the sites it had laid by its tail; everything
    // between that point and here stamps more -- the winner's charters, the state
    // handover's transfers (same tiles, so free), the rail stations.
    // THE NETWORK IS THE ROADS AND THE RAIL (Ben, 2026-10-24): BEFORE THE SEEDING, and
    // joining every site to its own nation's capital network over its own ground. The
    // seeding covers only what a road reaches, so a site roaded after it (the order this
    // call used to run in) was counted unconnectable: 128 of 360 consumers on seed 0.
    out.site_roads = lay_site_roads_every_body(w);
    std::printf("[site roads] finish: %d site tile(s), %d unroaded -> %d joined, "
                "%d unreachable, %d rejoined to their capital's network, %d cut off by "
                "water (%d crossing(s))\n",
                out.site_roads.sites, out.site_roads.sites_roadless,
                out.site_roads.sites_joined, out.site_roads.sites_failed,
                out.site_roads.sites_rejoined, out.site_roads.sites_cut_off,
                out.site_roads.crossings_laid);
    std::fflush(stdout);
    // BL-1309 -- THE LOGISTIC POINT SOURCES (LOGISTICS.md § Logistic Points,
    // "One pool"): production consumes its host nation's points, so the fold
    // stands enough logistics hubs that every producing building is reached and
    // every pool meets its need. After the rail handover (stations are sources).
    out.lp = seed_logistics_sources(w);
    // BL-1402 -- THE FOLD TO GROUND (Ben, 2026-10-23; NATIONS.md § What a nation owns):
    // every corporation's buildings pass to the nation whose ground they stand on,
    // balances to home treasuries, units, track, trades and convoys with them. After
    // the rail handover and the Logistic Point seeding (both may stand a firm's node)
    // and before the opening reserve, so the bill and the settle read a world in which
    // every building is a nation's.
    print_fold(fold_to_ground(w));
    // BL-1382 economy follow-up (NATION_GENERATION.md § Pass 7, Ben 2026-10-19:
    // no major leaves generation on a zero treasury) -- THE FOUNDINGS KEEP THE
    // SETTLE AND A YEAR OF THE STATE BILL, counted from the opening (BL-1386:
    // `k_state_founding_reserve_ticks`, the twelve settle ticks below plus
    // four). After the seeding, so the bill counts the state
    // hubs; then the seeding's reading is retaken at the funding gate, so the
    // printed coverage is what the first tick runs.
    out.reserve = reserve_state_first_tick(w, reg, out.state);
    print_state_reserve(out.reserve);
    // STACKED HUBS (Ben, 2026-10-26; LOGISTICS.md § Where generation puts the sources):
    // after the reserve, so a stacked hub is one the treasury funds with its year kept.
    stack_logistics_hubs(w, reg, out.lp);
    // BL-1398's firm top-up (fund_state_nodes_from_firms) RETIRED with BL-1402: the
    // fold above moved every firm's balance into its home treasury, so the opening node
    // bill is paid from the nation's whole treasury and "no capital starts dark" stands
    // (LOGISTICS.md, Ben 2026-10-23).
    lp_seed_read_at_gate(w, reg, out.lp);
    print_lp_seed_report(out.lp);
    // BL-1347, the SECOND site-road call: the seeding above stood hubs and ports, and a
    // port it landed on an island coast may carry no road yet. Idempotent: every site the
    // pre-seeding call joined is skipped. Before the settle, so the twelve econ ticks
    // play on a world the rule already holds for.
    {
        const site_road_stats s = lay_site_roads_every_body(w);
        out.site_roads.sites_roadless += s.sites_roadless;
        out.site_roads.sites_joined   += s.sites_joined;
        out.site_roads.sites_failed   += s.sites_failed;
        out.site_roads.crossings_laid += s.crossings_laid;
        out.site_roads.long_crossing  += s.long_crossing;
        out.site_roads.half_crossings_refused += s.half_crossings_refused;
        out.site_roads.sites_rejoined += s.sites_rejoined;
        out.site_roads.sites_cut_off  += s.sites_cut_off;
        std::printf("[site roads] after seeding: %d unroaded -> %d joined, %d unreachable "
                    "(%d crossing(s))\n",
                    s.sites_roadless, s.sites_joined, s.sites_failed, s.crossings_laid);
        std::fflush(stdout);
    }
    out.ms_search = ms_between(t_search, fin_clock::now());
    report_step_ms(progress, k_label_search, out.ms_search);

    // 6. THE SETTLE (BL-978, warm start retired): phase 6's "validate
    //    dynamically, once" -- twelve real econ ticks on the winner, in
    //    spectate with nobody seated. The balances, pools, returns and prices
    //    they leave are the opening position play receives (ERAS.md § The
    //    opening position); the clock is rebased at Begin, so they have no
    //    calendar meaning.
    enter_step(progress, k_label_settle, params);
    const fin_clock::time_point t_settle = fin_clock::now(); // reported only
    // Per-tick wall clock through a READ hook at the last lap: reported only,
    // so the slowest tick can be named below (see the result's field).
    // The lap clock rides the same hook (BL-1117, NR-932): each tick's six lap
    // times are kept so the slowest tick's line names the lap that costs it.
    //
    // BL-1117, THE PHASE CLOCK: lap 1 split by run_economy_step's own phases
    // (`economy_step_phase_clock`, economy_system.hpp) plus the dispatch
    // that closes the lap, and the count of logistics flood fields lap 1 built
    // (a size read of a const world). Reported only.
    //
    // WHAT THESE CLOCKS FOUND (2026-09-25, Release, seeds 0 and 28), kept so a
    // regression reads against it. The slow tick WAS the first tick in which any
    // convoy is committed (tick 0 commits none: no market has cleared, so every
    // destination's room is zero). That commit's passive-LP gate asked
    // `nearest_lp_anchor` for the origin's nearest anchor by a per-pair loop over
    // every city on the body, and each pair flooded a whole-body Dijkstra: ~6,000-
    // 9,000 floods, 65-88 s, landing in `corp_strategic` when a rival's directed
    // dispatch committed first (seed 28), in `dispatch` when the auto-dispatcher
    // did (seed 0). BL-1117 S2 answers it from one field per body; tick 1 now
    // costs about what the others do. If a tick again runs 30x its neighbours,
    // read both columns and the flood count first.
    using econ_row = std::array<std::int64_t, k_economy_step_phase_count + 1>; // + dispatch, us
    struct tick_clock
    {
        fin_clock::time_point      last;
        std::vector<std::int64_t>* ms;
        std::array<fin_clock::time_point, k_campaign_settle_lap_count + 1> laps{};
        std::vector<std::array<std::int64_t, k_campaign_settle_lap_count>> lap_ms;
        economy_step_phase_clock   econ{};
        std::vector<econ_row>      econ_us;
        std::size_t                floods_before = 0;
        std::vector<std::size_t>   floods_built;
        std::vector<std::size_t>   floods_alive; ///< After each tick: the memory the fields hold.
    } tc{t_settle, &out.settle_tick_ms};
    settle_tick_hooks hooks;
    hooks.ctx       = &tc;
    hooks.lap_clock = &tc.laps;
    hooks.after_lap = [](const world& hw, int lap, void* ctx) {
        auto* c = static_cast<tick_clock*>(ctx);
        if (lap == 0)
        {
            c->floods_before = hw.logistics_flood_fields.size();
            return;
        }
        if (lap == 1)
        {
            const auto us = [](fin_clock::time_point a, fin_clock::time_point b) {
                return static_cast<std::int64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(b - a).count());
            };
            econ_row row{};
            for (int i = 0; i < k_economy_step_phase_count; ++i)
                row[static_cast<std::size_t>(i)] =
                    us(c->econ.stamps[static_cast<std::size_t>(i)],
                       c->econ.stamps[static_cast<std::size_t>(i) + 1]);
            row[k_economy_step_phase_count] =
                us(c->econ.stamps[k_economy_step_phase_count], c->laps[2]); // dispatch
            c->econ_us.push_back(row);
            const std::size_t now_floods = hw.logistics_flood_fields.size();
            // A cache clear inside the lap shrinks the map: then report what stands.
            c->floods_built.push_back(now_floods >= c->floods_before ? now_floods - c->floods_before
                                                                     : now_floods);
            return;
        }
        if (lap != k_campaign_settle_lap_count - 1) return;
        c->floods_alive.push_back(hw.logistics_flood_fields.size());
        const fin_clock::time_point now = fin_clock::now();
        c->ms->push_back(ms_between(c->last, now));
        c->last = now;
        std::array<std::int64_t, k_campaign_settle_lap_count> row{};
        for (int i = 0; i < k_campaign_settle_lap_count; ++i)
            row[static_cast<std::size_t>(i)] =
                ms_between(c->laps[static_cast<std::size_t>(i)], c->laps[static_cast<std::size_t>(i) + 1]);
        c->lap_ms.push_back(row);
    };
    {
        // Armed for the settle only, and disarmed on every exit: the sink is
        // thread-local and `tc` is this frame's, so a throw must not leave a
        // later tick on this thread stamping into a dead frame.
        struct arm_guard
        {
            explicit arm_guard(economy_step_phase_clock* c) { economy_step_phase_clock_sink() = c; }
            ~arm_guard() { economy_step_phase_clock_sink() = nullptr; }
            arm_guard(const arm_guard&)            = delete;
            arm_guard& operator=(const arm_guard&) = delete;
        } armed{&tc.econ};
        run_settle(w, reg, k_campaign_settle_ticks, &hooks, progress, &out.readings);
    }
    out.ms_settle = ms_between(t_settle, fin_clock::now());
    report_step_ms(progress, k_label_settle, out.ms_settle);
    close_steps(progress);

    int          slowest    = -1;
    std::int64_t slowest_ms = -1;
    for (std::size_t i = 0; i < out.settle_tick_ms.size(); ++i)
        if (out.settle_tick_ms[i] > slowest_ms)
        {
            slowest_ms = out.settle_tick_ms[i];
            slowest    = static_cast<int>(i);
        }
    std::printf("[finish_campaign_world] searched %lld ms, settled %d ticks in %lld ms "
                "(slowest tick %d: %lld ms; seed %u, %zu nations)\n",
                static_cast<long long>(out.ms_search), k_campaign_settle_ticks,
                static_cast<long long>(out.ms_settle), slowest,
                static_cast<long long>(slowest_ms), params.seed, w.nations.size());
    if (slowest >= 0 && static_cast<std::size_t>(slowest) < tc.lap_ms.size())
    {
        std::printf("[finish_campaign_world] slowest tick %d by lap:", slowest);
        for (int i = 0; i < k_campaign_settle_lap_count; ++i)
            std::printf(" %s %lld ms%s", k_campaign_settle_lap_names[i],
                        static_cast<long long>(tc.lap_ms[static_cast<std::size_t>(slowest)]
                                                        [static_cast<std::size_t>(i)]),
                        i + 1 < k_campaign_settle_lap_count ? ";" : "");
        std::printf("\n");
    }
    // BL-1117: lap 1 by phase for the slowest tick and for the median one (the
    // typical tick the slow one is read against).
    const auto print_phases = [&](int t, const char* which) {
        if (t < 0 || static_cast<std::size_t>(t) >= tc.econ_us.size()) return;
        const econ_row& r = tc.econ_us[static_cast<std::size_t>(t)];
        std::printf("[finish_campaign_world] %s tick %d lap 1 by phase:", which, t);
        for (int i = 0; i < k_economy_step_phase_count; ++i)
            std::printf(" %s %lld ms;", k_economy_step_phase_names[i],
                        static_cast<long long>(r[static_cast<std::size_t>(i)] / 1000));
        std::printf(" dispatch %lld ms; flood fields built %zu\n",
                    static_cast<long long>(r[k_economy_step_phase_count] / 1000),
                    tc.floods_built[static_cast<std::size_t>(t)]);
    };
    print_phases(slowest, "slowest");
    {
        std::vector<int> order(out.settle_tick_ms.size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            const std::int64_t ma = out.settle_tick_ms[static_cast<std::size_t>(a)];
            const std::int64_t mb = out.settle_tick_ms[static_cast<std::size_t>(b)];
            return ma != mb ? ma < mb : a < b;
        });
        if (!order.empty() && order[order.size() / 2] != slowest)
            print_phases(order[order.size() / 2], "median");
    }
    std::printf("[finish_campaign_world] each settle tick, ms:");
    for (const std::int64_t ms : out.settle_tick_ms)
        std::printf(" %lld", static_cast<long long>(ms));
    std::printf("\n[finish_campaign_world] each settle tick's lap 1 dispatch, ms:");
    for (const econ_row& r : tc.econ_us)
        std::printf(" %lld", static_cast<long long>(r[k_economy_step_phase_count] / 1000));
    std::printf("\n");
    if (!tc.floods_alive.empty())
    {
        std::printf("[finish_campaign_world] flood fields alive after each settle tick:");
        for (const std::size_t n : tc.floods_alive)
            std::printf(" %zu", n);
        std::printf("\n");
    }
    std::fflush(stdout);
    return out;
}
