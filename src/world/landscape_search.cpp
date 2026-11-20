#include "landscape_search.hpp"

#include "corporation_generation.hpp"
#include "hard_coded_world.hpp" // generation_progress — the inner bar's write-only tap (BL-1085)
#include "logistics.hpp"
#include "planetology.hpp"   // checkpoint_rng — the project's one splitmix64 sub-stream
#include "recipe_registry.hpp"
#include "world.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>   // round timings — printed diagnostics only, never read by the walk
#include <cstdio>
#include <thread>

namespace
{

/// Deterministic sub-stream for one (round, axis) pair.
///
/// KEYED, NOT SEQUENCED, and that is the whole reason a round's proposals can be
/// scored in any order. A sequential stream would make one axis's draw depend on
/// whether another drew first, so a reordering (or a thread) would silently
/// change WHICH candidates exist rather than merely when they are scored. The
/// stage tag folds both coordinates so no two (round, axis) pairs collide within
/// a search. The stride stays `landscape_axis_count` (3) with placement alone
/// live, so every placement draw is the one the three-axis walk made.
checkpoint_rng axis_rng(std::uint32_t seed, int round, landscape_axis axis)
{
    const std::uint32_t tag =
        static_cast<std::uint32_t>(round) * static_cast<std::uint32_t>(landscape_axis_count)
        + static_cast<std::uint32_t>(static_cast<int>(axis));
    return checkpoint_rng(seed, tag);
}

/// Strict weak ordering on one term, folded into the lexicographic chain below.
/// Returns 0 when the two are bit-identical, which is the only thing that lets
/// `compare_landscape` report a genuine tie.
int cmp_double(double a, double b)
{
    if (a < b) return -1;
    if (a > b) return  1;
    return 0;
}

/// Perturb the incumbent's PLACEMENT, the one live axis (Ben, 2026-11-12).
/// Pure in (incumbent, rng); every other field is carried unchanged, so the
/// roster count and road tier stay at the seed candidate's for the whole walk.
///
/// Placement has no neighbourhood — a seed one apart stakes a wholly different
/// set of holdings — so the proposal is a redraw by construction, not by
/// choice. Folded from the incumbent's own seed so the walk's history is
/// carried rather than discarded. The arithmetic and the draw are the
/// three-axis walk's placement case, unchanged, so with the tag stride held
/// (`axis_rng`) a proposal here is the one that walk drew.
landscape_candidate perturb_placement(const landscape_candidate& in, checkpoint_rng& rng)
{
    landscape_candidate out = in;
    const std::uint32_t draw = static_cast<std::uint32_t>(rng.index(1 << 30));
    out.placement_seed = in.placement_seed * 1664525u + 1013904223u + draw;
    return out;
}

/// One scored proposal, held by index so the argmax never reads completion order.
struct scored
{
    landscape_candidate cand{};
    landscape_score     score{};
};

void score_one(const world& base, const recipe_registry& reg,
               const landscape_search_params& p, scored& s)
{
    world w = base;                       // its OWN world — score_landscape memoises into it
    // BL-1032: the spend params are COPIED here, per evaluation — the scoring
    // threads share `p`, so nothing any evaluation could mutate may live in it.
    // The budget itself is read-only. With no budget (or an empty one) the
    // overload forwards to the legacy 4-argument call, so this is today's path.
    const charter_spend_params spend = p.spend;
    apply_landscape_candidate(w, reg, s.cand, p.regenerate_specialists,
                              p.budget, spend, /*report=*/nullptr);
    s.score = score_landscape(w, reg, p.score);
}

/// BL-1404 -- THE LEGACY PATH RETIRES (Ben, 2026-10-23). A world with no
/// charter budget -- none, a refused spend, or one that opens no specialist --
/// once fell back to the legacy overload (`generate_corporations`' weighted
/// 8-corp draw plus the background firms). It now CHARTERS NOTHING: the road
/// tier axis and the default recipes, as every apply does; the roster removed
/// when the caller regenerates it; no specialist, no firm, nobody seated. An
/// empty economy is the honest result of a world whose history left no
/// industry to charter with. Pure and draw-free. (The road-tier raise below
/// reads the candidate's tier, which the search no longer varies.)
void lay_unchartered_world(world& w, const recipe_registry& reg,
                           const landscape_candidate& c, bool regenerate_specialists)
{
    bool moved = false;
    for (auto& kv : w.tiles)
    {
        tile_component& t = kv.second;
        for (int s = 0; s < 6; ++s) // BL-1340: the axis raises EDGES (see the legacy body)
        {
            const std::uint8_t cur = road_tier_on_side(t, s);
            if (cur > 0 && cur < c.road_tier)
            {
                set_road_tier_on_side(t, s, c.road_tier);
                moved = true;
            }
        }
    }
    if (moved)
        invalidate_logistics_caches(w);
    assign_default_recipes(w, reg, "landscape apply (unchartered)");
    if (regenerate_specialists)
        remove_specialist_roster(w);   // clears the majors with the roster
    invalidate_logistics_caches(w);
}

} // namespace

const char* landscape_axis_name(landscape_axis a)
{
    switch (a)
    {
    case landscape_axis::roster:    return "roster";
    case landscape_axis::placement: return "placement";
    case landscape_axis::road_tier: return "road_tier";
    }
    return "?";
}

bool candidate_key_less(const landscape_candidate& a, const landscape_candidate& b)
{
    if (a.corporation_count != b.corporation_count)
        return a.corporation_count < b.corporation_count;
    if (a.placement_seed != b.placement_seed)
        return a.placement_seed < b.placement_seed;
    return a.road_tier < b.road_tier;
}

int compare_landscape(const landscape_score& a, const landscape_score& b)
{
    // THE TOTAL ORDER, EXPLICIT AND LEXICOGRAPHIC. `composite` already carries
    // the ruled objective; the terms below it exist so that two landscapes
    // reaching the same composite by different routes still order the same way
    // on every machine. Their sequence is the objective's own priority:
    // realisation (the only roster-aware term) before potential, potential
    // before unevenness, unevenness before the balance level.
    int c = cmp_double(a.composite, b.composite);           if (c) return c;
    c = cmp_double(a.realisation, b.realisation);           if (c) return c;
    c = cmp_double(a.mean_actual, b.mean_actual);           if (c) return c;
    c = cmp_double(a.mean_completeness, b.mean_completeness); if (c) return c;
    c = cmp_double(a.spread, b.spread);                     if (c) return c;
    c = cmp_double(a.reach_spread, b.reach_spread);         if (c) return c;
    c = cmp_double(a.mean_balance, b.mean_balance);         if (c) return c;
    // Market count last: a landscape scoring identically over more markets is
    // the better-founded reading of the same number.
    if (a.market_count != b.market_count)
        return a.market_count < b.market_count ? -1 : 1;
    return 0;
}

void apply_landscape_candidate(world& w, const recipe_registry& reg,
                               const landscape_candidate& c)
{
    apply_landscape_candidate(w, reg, c, /*regenerate_specialists=*/true);
}

void apply_landscape_candidate(world& w, const recipe_registry& reg,
                               const landscape_candidate& c,
                               bool regenerate_specialists)
{
    // --- the candidate's ROAD TIER (a retired search axis, 2026-11-12) -------
    // The search holds every candidate at the seed's tier, so this raise is the
    // same for every candidate (a no-op at tier 1). Purely additive and
    // idempotent: a road already at a higher tier is never downgraded, and a
    // tile with no road never gains one.
    //
    // The walk is over an unordered_map and that is safe HERE, unlike a sum:
    // every write is `max` on an integer field of the tile it is keyed by, so
    // neither the visit order nor floating-point associativity can reach it.
    bool moved = false;
    for (auto& kv : w.tiles)
    {
        tile_component& t = kv.second;
        // BL-1340: the axis raises EDGES. Each crossing is written from both of its tiles
        // with the same tier, so the walk still commutes and the mirror invariant holds
        // whatever order the unordered_map hands the tiles over in.
        for (int s = 0; s < 6; ++s)
        {
            const std::uint8_t cur = road_tier_on_side(t, s);
            if (cur > 0 && cur < c.road_tier)
            {
                set_road_tier_on_side(t, s, c.road_tier);
                moved = true;
            }
        }
    }
    if (moved)
    {
        // The reach field, the A* cache and the flood fields are all functions of the
        // road field. Leaving them warm would score the new tier against the old
        // network — and, worse, would do so only for a world that had already been
        // scored, which is exactly the order-dependence this search must not have.
        invalidate_logistics_caches(w);
    }

    // --- the roster (count held) and its PLACEMENT ---------------------------
    // The same two calls, in the same order, that the slice-1/2 harness measured
    // discrimination on — so the search walks the fixture the objective was shown
    // to be able to see, not a second one invented here.
    // BL-1217 D6: a recipe-less processor whose default output is unwanted is
    // unplaced here (before the roster's chain enforcement, which re-seats).
    assign_default_recipes(w, reg, "landscape apply (legacy)");

    if (regenerate_specialists)
    {
        // REPLACE, not append (BL-977). World-gen's roster goes first, then the
        // candidate's is laid from the SAME settlement record world-gen read, so
        // a searched specialist keeps its region-derived focus and ownership
        // class rather than falling to the national-character fallback.
        //
        // The seed is the candidate's placement seed, exactly as the background
        // pass below takes it: the two passes already draw from distinct
        // xor-offset streams of one seed, and that seed is itself drawn from the
        // search's (round, axis)-keyed stream on the placement axis. The count
        // is the candidate's, which the search holds at the seed's.
        remove_specialist_roster(w);
        corporation_params cp;
        cp.corporation_count = c.corporation_count;
        // BL-1188: with the registry, so the specialists' processors are
        // chain-feasible (Pass 3); world-gen's own call has none.
        generate_corporations(w, cp, c.placement_seed, w.gen_settlement.get(),
                              /*progress=*/nullptr, &reg);
        // BL-1185 review: with the chain rule a slot every rung refused is no
        // corporation, so a roster can come back EMPTY — no specialist, no seat.
        // Reported, and handled downstream as the no-specialist world is (the
        // seat path seats nobody rather than a null id).
        if (w.nations.count(w.player_entity) == 0) // BL-1407: the seat names a nation
        {
            w.player_entity = null_entity;
            std::printf("[landscape_apply] no specialist chartered (placement %08X): the roster "
                        "is empty, nobody is seated\n", c.placement_seed);
        }
    }
    else
    {
        // BL-1185: the kept world-gen roster was laid before the registry
        // existed; its processors are made chain-feasible here, and the result
        // is reported where it changed anything (the search applies many
        // candidates, so an unchanged roster stays quiet).
        const chain_roster_enforcement cre =
            enforce_chain_feasible_roster(w, reg, c.placement_seed ^ 0x4A71012u);
        if (cre.processors_unplaced > 0 || cre.holdless > 0 || cre.seat_redrawn)
            std::printf("[landscape_apply] chain roster (placement %08X): %d re-decided, %d "
                        "unplaced, %d holdless; seat %u%s\n",
                        c.placement_seed, cre.processors_redecided, cre.processors_unplaced,
                        cre.holdless, static_cast<unsigned>(cre.seat),
                        cre.seat_redrawn ? " (redrawn)" : "");
    }
    generate_background_firms(w, reg, c.placement_seed);

    // Either pass can author a port or an inland hub, and each is a supply
    // anchor. The base world's reach field was warmed by its own placement, so
    // a copy scored without this would read a catchment that ignores every
    // anchor the candidate just built. Unconditional: an over-clear costs one
    // Dijkstra, a missed one is a reach field that lies.
    invalidate_logistics_caches(w);
}

void apply_landscape_candidate(world& w, const recipe_registry& reg,
                               const landscape_candidate& c,
                               bool regenerate_specialists,
                               const charter_budget* budget,
                               const charter_spend_params& spend,
                               charter_spend_report* report)
{
    // --- THE ONE BRANCH (BL-1032) ------------------------------------------
    // BL-1404 (Ben, 2026-10-23): the three non-budget readings below no longer
    // lay the legacy overload; each CHARTERS NOTHING (`lay_unchartered_world`).
    // Where the comments below say "the legacy overload", read that.
    // No budget, or an empty one (the all-zero budget is the same state: the
    // type drops entries <= 0), is TODAY'S WORLD: the legacy overload, called
    // verbatim, with nothing before it and nothing after it. Nothing below this
    // line runs for such a world, and no spend param or price is read.
    //
    // THE ONE PREDICATE (BL-1086 review): `read_budget_world` decides it, in
    // the order this function always read it, on `w` as it arrives and before
    // any mutation — the same test the search and generation's bump 11 ask.
    const budget_world_reading bw = read_budget_world(w, budget, spend, &reg);
    if (bw.kind == budget_world_kind::no_budget)
    {
        lay_unchartered_world(w, reg, c, regenerate_specialists); // BL-1404: was the legacy overload
        return;
    }

    // A REFUSED spend is decided HERE, before any mutation, and it is today's
    // world too: the legacy overload verbatim, exactly as with no budget, and a
    // report that says every point went unspent and why. No roster is removed,
    // nothing is chartered — a refusal mutates nothing beyond today's world.
    // BL-1060: the params' refusal, then the one only the world can decide (a
    // ceiling too small for a body's turn and yards), both read on `w` as it
    // arrives.
    if (bw.kind == budget_world_kind::refused)
    {
        lay_unchartered_world(w, reg, c, regenerate_specialists); // BL-1404: was the legacy overload
        if (report != nullptr)
            *report = charter_refused_report(*budget, bw.refusal);
        return;
    }

    // THE NO-SPECIALIST WORLD (Ben, 2026-09-21, NR-910) falls back exactly as a
    // refused spend does: decided here, from the budget, before anything is
    // chartered, and laid as the legacy overload verbatim — so the player is
    // seated from the no-budget world's own roster. Not a refusal: the report
    // says it fell back, every point unspent as `no_specialist`.
    if (bw.kind == budget_world_kind::no_specialist)
    {
        lay_unchartered_world(w, reg, c, regenerate_specialists); // BL-1404: was the legacy overload
        if (report != nullptr)
            *report = charter_fallback_report(*budget);
        return;
    }

    // --- a BUDGET WORLD ------------------------------------------------------
    // The road tier, copied from the legacy body rather than shared with it —
    // a shared helper is a refactor of the legacy path, and the legacy path's
    // bytes are the contract (BL-1031's pins).
    bool moved = false;
    for (auto& kv : w.tiles)
    {
        tile_component& t = kv.second;
        for (int s = 0; s < 6; ++s) // BL-1340: the axis raises EDGES (see the legacy body)
        {
            const std::uint8_t cur = road_tier_on_side(t, s);
            if (cur > 0 && cur < c.road_tier)
            {
                set_road_tier_on_side(t, s, c.road_tier);
                moved = true;
            }
        }
    }
    if (moved)
        invalidate_logistics_caches(w);

    // BL-1217 D6: an unwanted default unplaces (the roster goes next anyway).
    assign_default_recipes(w, reg, "landscape apply (budget)");

    // THE BUDGET CHARTERS THE WHOLE WEB (INDUSTRIALISATION.md § 1, Ben 2026-09-17):
    // world-gen's specialist roster goes, and every specialist and background
    // firm the candidate carries is bought from a centre's budget. So there is
    // no `regenerate_specialists = false` reading here — the roster is not the
    // world-gen one on a budget world under any flag.
    (void)regenerate_specialists;
    remove_specialist_roster(w);
    charter_web_from_budget(w, reg, *budget, spend, c.placement_seed,
                            w.gen_settlement.get(), report);

    // As the legacy body: a chartered port or hub is a supply anchor.
    invalidate_logistics_caches(w);
}

landscape_search_result search_landscape(const world& base, const recipe_registry& reg,
                                         const landscape_search_params& p_in)
{
    landscape_search_result out;
    out.seed_candidate = p_in.start;

    // BL-1032 — A REFUSAL, decided before anything else. A non-empty budget
    // whose spend params are refused (a price <= 0 — the prices have no shipped
    // default) is NOT a budget world: the search below runs on a copy of the
    // params with the budget removed, which is exactly the no-budget search
    // (the seed candidate alone, its score real), and the result says it was refused.
    // `p` is that copy on a refusal and `p_in` itself otherwise.
    // BL-1060: then the refusal only the world can decide, on the base world
    // every candidate is applied to (`charter_spend_world_refusal`).
    //
    // THE ONE PREDICATE (BL-1086 review): `read_budget_world`, the test the
    // apply and generation's bump 11 ask, read once here on the base world.
    const budget_world_reading bw = read_budget_world(base, p_in.budget, p_in.spend, &reg);
    const char* refusal = bw.kind == budget_world_kind::refused ? bw.refusal : nullptr;
    landscape_search_params refused_params;
    if (refusal != nullptr)
    {
        refused_params        = p_in;
        refused_params.budget = nullptr;
        out.charter_refused   = true;
        out.charter_refusal   = refusal;
        // Unconditional, not a round diagnostic: a refused budget is a caller
        // error that must not pass silently as today's world.
        std::printf("[landscape_search] charter budget REFUSED (%lld points over %zu centres); "
                    "running the no-budget search: %s\n",
                    static_cast<long long>(p_in.budget->total()), p_in.budget->points().size(),
                    refusal);
    }
    // NR-910 — THE NO-SPECIALIST WORLD, the same route: a non-empty budget on
    // which no centre a nation owns affords a specialist runs the no-budget
    // search, decided on the base world before any candidate charters, exactly
    // as the apply decides it (`apply_landscape_candidate`'s budget overload).
    const bool fell_back = bw.kind == budget_world_kind::no_specialist;
    if (fell_back)
    {
        refused_params         = p_in;
        refused_params.budget  = nullptr;
        out.charter_fell_back  = true;
        // Unconditional, as the refusal's line is: the world a player is handed
        // changed branch, and that is never silent.
        std::printf("[landscape_search] charter budget opens NO specialist (%lld points over %zu "
                    "centres, specialist %lld); running the no-budget search (NR-910)\n",
                    static_cast<long long>(p_in.budget->total()), p_in.budget->points().size(),
                    static_cast<long long>(p_in.spend.specialist_price_points()));
    }
    const landscape_search_params& p = (refusal != nullptr || fell_back) ? refused_params : p_in;

    // BL-1032. On a BUDGET WORLD the budget decides the roster; a null, empty
    // or refused budget is not a budget world, and only a budget world walks
    // (below: placement moves nothing on a world that charters nothing).
    const bool budget_world = bw.budget_world();
    if (budget_world && p.print_rounds)
        std::printf("[landscape_search] charter budget: %zu centres, %lld points; the budget "
                    "decides the roster, the walk varies placement\n",
                    p.budget->points().size(),
                    static_cast<long long>(p.budget->total()));

    // NO BUDGET, NO WALK (BL-1646 review). Off a budget world every candidate
    // is laid by `lay_unchartered_world`, which charters nothing and never
    // reads `placement_seed`, so a placement proposal scores bit-identically to
    // the incumbent and the strict-improvement rule can never take it. The walk
    // would be `p.rounds` wasted evaluations returning the seed candidate; it is
    // skipped, and the result is exactly what those rounds would have returned
    // (winner = seed candidate) minus the path steps that never moved.
    const int rounds = budget_world ? std::max(0, p.rounds) : 0;
    if (!budget_world && p.rounds > 0 && p.print_rounds)
        std::printf("[landscape_search] no charter budget: the seed candidate alone (placement "
                    "moves nothing on a world that charters nothing)\n");

    using clock = std::chrono::steady_clock;
    const auto ms_since = [](clock::time_point t0)
    {
        return std::chrono::duration<double, std::milli>(clock::now() - t0).count();
    };

    // --- THE WALK, SCORED AHEAD (2026-11-19) ---------------------------------
    // The walk is serial in its DECISIONS, not in its candidates. A round's
    // proposal is `perturb_placement(incumbent, axis_rng(seed, r))`: keyed,
    // never sequenced, so it depends only on WHICH candidate is the incumbent,
    // and after k rounds the incumbent is one of at most 2^k candidates (each
    // round keeps it or takes its proposal). The whole tree of proposals for the
    // next D rounds is therefore known before any of them is scored. A WAVE
    // scores that tree at once (2^D - 1 proposals, plus the seed in the first
    // wave), then the serial walk is REPLAYED over the stored scores: the same
    // comparisons in the same order, so the winner, every score and the whole
    // path are the serial walk's, bit for bit. The branch not taken is scored
    // and thrown away -- that is the price, paid in CPU and in peak memory (one
    // world copy per worker), never in the result. D is the largest depth whose
    // tree fits `thread_count` workers; one or two threads give D = 1, which is
    // one proposal per wave, the old walk.
    const int threads = std::max(1, p.thread_count);
    int depth = 1;
    while (depth < 8 && ((1 << (depth + 1)) - 1) <= threads)
        ++depth;

    // The inner bar counts every evaluation run, taken branch or not, over every
    // evaluation planned (computed up front, so the bar never runs backwards).
    int planned_runs = 1;
    for (int r0 = 0; r0 < rounds; r0 += depth)
        planned_runs += (1 << std::min(depth, rounds - r0)) - 1;
    std::atomic<int> runs_done{0};
    const auto report_run = [&] {
        const int done = runs_done.fetch_add(1, std::memory_order_relaxed) + 1;
        if (p.progress != nullptr)
            p.progress->report_sub(done, planned_runs);
    };

    // Score every job, on up to `threads` workers. Each job writes only its own
    // slot. A worker that cannot start, or that throws, leaves its jobs undone;
    // the calling thread then scores whatever is left, in index order, so a
    // missing thread costs wall time and never a result. An exception from that
    // serial pass propagates as it always did.
    struct job { scored* s; double ms; bool done; };
    const auto run_jobs = [&](std::vector<job>& jobs)
    {
        const auto run_one = [&](job& j)
        {
            const auto t0 = clock::now();
            score_one(base, reg, p, *j.s);
            j.ms   = ms_since(t0);
            j.done = true;
            report_run();
        };
        const int n = static_cast<int>(jobs.size());
        const int spawn = std::min(threads, n);
        if (spawn > 1)
        {
            std::vector<std::thread> pool;
            pool.reserve(static_cast<std::size_t>(spawn));
            for (int t = 0; t < spawn; ++t)
            {
                try
                {
                    pool.emplace_back([&, t]
                    {
                        try
                        {
                            for (int i = t; i < n; i += spawn)
                                run_one(jobs[static_cast<std::size_t>(i)]);
                        }
                        catch (...) {} // its jobs stay undone; the serial pass redoes them
                    });
                }
                catch (...) { break; } // could not start: the serial pass covers it
            }
            for (std::thread& th : pool)
                th.join();
        }
        for (job& j : jobs)
            if (!j.done)
            {
                // A worker that threw mid-job may have left a partial score;
                // the job is scored again from its candidate.
                j.s->score = landscape_score{};
                run_one(j);
            }
    };

    // --- the seed candidate -------------------------------------------------
    scored seed;
    seed.cand = p.start;
    double seed_ms = 0.0;
    bool seed_scored = false;

    landscape_candidate incumbent = p.start;
    for (int r0 = 0; r0 < rounds || !seed_scored; r0 += depth)
    {
        const int d = std::max(0, std::min(depth, rounds - r0));

        // levels[k] holds round r0+k's proposals, one per incumbent that round
        // can have, ordered so incumbent index i after round k becomes 2i (kept)
        // and 2i+1 (its proposal taken) after round k+1.
        std::vector<std::vector<scored>> levels(static_cast<std::size_t>(d));
        {
            std::vector<landscape_candidate> incs{incumbent};
            for (int k = 0; k < d; ++k)
            {
                const landscape_axis axis = landscape_axis::placement;
                std::vector<landscape_candidate> next;
                next.reserve(incs.size() * 2);
                levels[static_cast<std::size_t>(k)].resize(incs.size());
                for (std::size_t i = 0; i < incs.size(); ++i)
                {
                    checkpoint_rng rng = axis_rng(p.seed, r0 + k, axis);
                    scored& s = levels[static_cast<std::size_t>(k)][i];
                    s.cand = perturb_placement(incs[i], rng);
                    next.push_back(incs[i]);
                    next.push_back(s.cand);
                }
                incs = std::move(next);
            }
        }

        std::vector<job> jobs;
        if (!seed_scored)
            jobs.push_back(job{&seed, 0.0, false});
        for (auto& lv : levels)
            for (scored& s : lv)
                jobs.push_back(job{&s, 0.0, false});
        const auto wave_t0 = clock::now();
        run_jobs(jobs);
        // `round_ms` stays WALL TIME (its readers sum it into a walk's cost):
        // the wave's wall time, split evenly over the entries it decided, so
        // the entries still sum to the walk's wall time. The printed line
        // gives each evaluation's own time instead.
        const double wave_share = ms_since(wave_t0)
            / static_cast<double>(std::max(1, d + (seed_scored ? 0 : 1)));
        std::size_t ji = 0;

        if (!seed_scored)
        {
            seed_ms = jobs[ji++].ms;
            seed_scored = true;
            out.seed_score   = seed.score;
            out.winner       = seed.cand;
            out.winner_score = seed.score;
            out.evaluations  = 1;
            out.round_ms.push_back(wave_share);
            if (p.print_rounds)
                std::printf("[landscape_search] seed   corps=%d placement=%08X tier=%u  "
                            "composite=%.6f  (%.0f ms)\n",
                            seed.cand.corporation_count, seed.cand.placement_seed,
                            static_cast<unsigned>(seed.cand.road_tier), seed.score.composite,
                            seed_ms);
        }

        // --- the replay: the serial walk, read off the stored scores --------
        std::vector<double> ms_by_job;
        for (const job& j : jobs)
            ms_by_job.push_back(j.ms);
        std::size_t idx = 0;
        std::size_t level_base = ji; // job index of levels[k][0]
        for (int k = 0; k < d; ++k)
        {
            const int r = r0 + k;
            const scored& prop = levels[static_cast<std::size_t>(k)][idx];
            const double prop_ms = ms_by_job[level_base + idx];
            level_base += levels[static_cast<std::size_t>(k)].size();
            ++out.evaluations;

            // --- replace only on a STRICT improvement ------------------------
            // A tie leaves the incumbent standing. Consulting the candidate key
            // here would make an equal-scoring proposal unseat the incumbent and
            // let the walk churn between equals for the rest of its rounds.
            const std::size_t slot = static_cast<std::size_t>(static_cast<int>(landscape_axis::placement));
            const double incumbent_composite = out.winner_score.composite;
            const bool accept = compare_landscape(prop.score, out.winner_score) > 0;
            if (accept)
            {
                out.winner       = prop.cand;
                out.winner_score = prop.score;
                ++out.accepted;
                ++out.accepted_by_axis[slot];
            }

            out.round_ms.push_back(wave_share);
            if (p.print_rounds)
                std::printf("[landscape_search] round %d  %-9s %s  corps=%d placement=%08X tier=%u  "
                            "composite=%.6f vs incumbent %.6f  (%.0f ms, %.0f ms/eval)\n",
                            r, landscape_axis_name(landscape_axis::placement),
                            accept ? "TAKEN" : "held ",
                            prop.cand.corporation_count, prop.cand.placement_seed,
                            static_cast<unsigned>(prop.cand.road_tier), prop.score.composite,
                            incumbent_composite, prop_ms, prop_ms);

            landscape_search_step step;
            step.round    = r;
            step.axis     = landscape_axis::placement;
            step.proposal = prop.cand;
            step.score    = prop.score;
            step.accepted = accept;
            out.path.push_back(step);

            idx = idx * 2 + (accept ? 1u : 0u);
        }
        incumbent = out.winner;
    }
    if (p.print_rounds && planned_runs > out.evaluations)
        std::printf("[landscape_search] scored ahead: %d evaluations run for the %d the walk "
                    "read (%d workers, %d rounds per wave)\n",
                    planned_runs, out.evaluations, threads, depth);

    return out;
}
