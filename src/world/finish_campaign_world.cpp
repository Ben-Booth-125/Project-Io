#include "world/finish_campaign_world.hpp"

#include "world/campaign_settle.hpp"
#include "world/corporation_generation.hpp" // assign_default_recipes
#include "world/hard_coded_world.hpp"       // generation_progress, generation_step_cost_ms
#include "world/history_log.hpp"            // seed_genesis_history
#include "world/recipe_registry.hpp"
#include "world/survey_system.hpp"          // init_survey_states
#include "world/world.hpp"
#include "world/world_gen_config.hpp"

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

} // namespace

era_band campaign_band_from_world(const world& w)
{
    // THE BAND IS THE WORLD'S OWN (BL-1101, Ben 2026-09-24): derived once at
    // the Industrialisation fold from the history's industry state
    // (`derive_campaign_band`, history_sim.hpp) and carried on
    // `world::campaign_band`, never from the epoch. A world whose fold never ran
    // -- a harness fixture built without the prehistory, or a descriptor with
    // the span switched off -- carries `any`, which would ADMIT both rosters at
    // once; it bands as the shipped 1960 default, `industrial`, so a fixture
    // never sees a roster no campaign can (the review's fix round on BL-1101).
    // The shipped descriptor always runs the fold, so the fallback is a
    // harness fact, not a design one.
    return w.campaign_band == era_band::any ? era_band::industrial : w.campaign_band;
}

landscape_search_params campaign_search_params(std::uint32_t world_seed, int corporation_count)
{
    landscape_search_params sp;
    sp.regenerate_specialists  = true;
    sp.seed                    = world_seed ^ 0x8A21F00Du;
    sp.start.placement_seed    = sp.seed;
    sp.start.corporation_count = corporation_count;
    return sp;
}

std::int64_t finish_campaign_weight_ms(const world_params& params)
{
    return generation_step_cost_ms(k_label_search, params)
         + generation_step_cost_ms(k_label_settle, params);
}

finish_campaign_result finish_campaign_world(world& w, const generation_report& report,
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
    assign_default_recipes(w, reg);

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
    if (out.stockpile.points_total != 0 || out.stockpile.rejected)
    {
        const stockpile_budget& sb = out.stockpile;
        const auto why = [&](stockpile_unspent_reason k) {
            return static_cast<long long>(sb.unspent[static_cast<std::size_t>(k)]);
        };
        // BL-1064: an empty budget (rejected, or every point unspent) prices nothing.
        char price[96] = "no price (an empty budget)";
        if (!sb.budget.empty())
            std::snprintf(price, sizeof price, "firm price %d (the stock / %lld), specialist %lld",
                          static_cast<int>(sb.firm_price_points),
                          static_cast<long long>(sb.price_divisor),
                          static_cast<long long>(out.spend.specialist_price_points()));
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
    assign_default_recipes(w, reg);
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
    struct tick_clock
    {
        fin_clock::time_point      last;
        std::vector<std::int64_t>* ms;
    } tc{t_settle, &out.settle_tick_ms};
    settle_tick_hooks hooks;
    hooks.ctx       = &tc;
    hooks.after_lap = [](const world&, int lap, void* ctx) {
        if (lap != k_campaign_settle_lap_count - 1) return;
        auto* c = static_cast<tick_clock*>(ctx);
        const fin_clock::time_point now = fin_clock::now();
        c->ms->push_back(ms_between(c->last, now));
        c->last = now;
    };
    run_settle(w, reg, k_campaign_settle_ticks, &hooks, progress);
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
                "(slowest tick %d: %lld ms; seed %u, %zu corporations)\n",
                static_cast<long long>(out.ms_search), k_campaign_settle_ticks,
                static_cast<long long>(out.ms_settle), slowest,
                static_cast<long long>(slowest_ms), params.seed, w.corporations.size());
    std::fflush(stdout);
    return out;
}
