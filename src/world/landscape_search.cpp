#include "landscape_search.hpp"

#include "corporation_generation.hpp"
#include "logistics.hpp"
#include "planetology.hpp"   // checkpoint_rng — the project's one splitmix64 sub-stream
#include "recipe_registry.hpp"
#include "world.hpp"

#include <algorithm>
#include <chrono>   // round timings — printed diagnostics only, never read by the walk
#include <cstdio>
#include <thread>

namespace
{

/// Deterministic sub-stream for one (round, axis) pair.
///
/// KEYED, NOT SEQUENCED, and that is the whole reason a round's proposals can be
/// scored in any order. A sequential stream would make the placement axis's draw
/// depend on whether the roster axis drew first, so a reordering (or a thread)
/// would silently change WHICH candidates exist rather than merely when they are
/// scored. The stage tag folds both coordinates so no two (round, axis) pairs
/// collide within a search.
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

/// Perturb the incumbent along one axis. Pure in (incumbent, params, rng).
landscape_candidate perturb(const landscape_candidate& in, landscape_axis axis,
                            const landscape_search_params& p, checkpoint_rng& rng)
{
    landscape_candidate out = in;
    switch (axis)
    {
    case landscape_axis::roster:
    {
        // A ladder of signed steps rather than a uniform redraw: greedy
        // refinement wants a NEIGHBOURHOOD of the incumbent, and a redraw over
        // the whole range would make each round an independent sample and throw
        // away the walk. The wide steps are kept so the walk can still cross a
        // flat stretch.
        static const int steps[] = { -4, -2, -1, 1, 2, 4 };
        const int d = steps[rng.index(static_cast<int>(sizeof steps / sizeof steps[0]))];
        out.corporation_count = std::clamp(in.corporation_count + d,
                                           p.min_corporations, p.max_corporations);
        break;
    }
    case landscape_axis::placement:
    {
        // Placement has no neighbourhood — a seed one apart stakes a wholly
        // different set of holdings — so this axis is a redraw by construction,
        // not by choice. Folded from the incumbent's own seed so the walk's
        // history is carried rather than discarded.
        const std::uint32_t draw = static_cast<std::uint32_t>(rng.index(1 << 30));
        out.placement_seed = in.placement_seed * 1664525u + 1013904223u + draw;
        break;
    }
    case landscape_axis::road_tier:
    {
        const int d = (rng.index(2) == 0) ? -1 : 1;
        const int t = std::clamp(static_cast<int>(in.road_tier) + d,
                                 static_cast<int>(p.min_road_tier),
                                 static_cast<int>(p.max_road_tier));
        out.road_tier = static_cast<std::uint8_t>(t);
        break;
    }
    }
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
    // --- ROAD TIER axis -----------------------------------------------------
    // Purely additive and idempotent: a road already at a higher tier is never
    // downgraded, and a tile with no road never gains one. So the walk cannot
    // demolish infrastructure by proposing a lower tier — it can only decline to
    // develop it, which is what "infrastructure tier" means as an axis.
    //
    // The walk is over an unordered_map and that is safe HERE, unlike a sum:
    // every write is `max` on an integer field of the tile it is keyed by, so
    // neither the visit order nor floating-point associativity can reach it.
    bool moved = false;
    for (auto& kv : w.tiles)
    {
        tile_component& t = kv.second;
        if (t.road_level > 0 && t.road_level < c.road_tier)
        {
            t.road_level = c.road_tier;
            moved = true;
        }
    }
    if (moved)
    {
        // The reach field, the A* cache and the flood fields are all functions of
        // road_level. Leaving them warm would score the new tier against the old
        // network — and, worse, would do so only for a world that had already been
        // scored, which is exactly the order-dependence this search must not have.
        invalidate_logistics_caches(w);
    }

    // --- ROSTER and PLACEMENT axes ------------------------------------------
    // The same two calls, in the same order, that the slice-1/2 harness measured
    // discrimination on — so the search walks the fixture the objective was shown
    // to be able to see, not a second one invented here.
    assign_default_recipes(w, reg);

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
        // search's (round, axis)-keyed stream on the placement axis. The roster
        // axis moves the COUNT on a held seed, so its proposal is a neighbour of
        // the incumbent — the same firms plus or minus a few — not a redraw.
        remove_specialist_roster(w);
        corporation_params cp;
        cp.corporation_count = c.corporation_count;
        generate_corporations(w, cp, c.placement_seed, w.gen_settlement.get());
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
    // No budget, or an empty one (the all-zero budget is the same state: the
    // type drops entries <= 0), is TODAY'S WORLD: the legacy overload, called
    // verbatim, with nothing before it and nothing after it. Nothing below this
    // line runs for such a world, and no spend param or price is read.
    if (budget == nullptr || budget->empty())
    {
        apply_landscape_candidate(w, reg, c, regenerate_specialists);
        return;
    }

    // A REFUSED spend is decided HERE, before any mutation, and it is today's
    // world too: the legacy overload verbatim, exactly as with no budget, and a
    // report that says every point went unspent and why. No roster is removed,
    // nothing is chartered — a refusal mutates nothing beyond today's world.
    if (const char* why = charter_spend_refusal(*budget, spend))
    {
        apply_landscape_candidate(w, reg, c, regenerate_specialists);
        if (report != nullptr)
            *report = charter_refused_report(*budget, why);
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
        if (t.road_level > 0 && t.road_level < c.road_tier)
        {
            t.road_level = c.road_tier;
            moved = true;
        }
    }
    if (moved)
        invalidate_logistics_caches(w);

    assign_default_recipes(w, reg);

    // THE BUDGET CHARTERS THE WHOLE WEB (DIGITISATION.md § 1, Ben 2026-09-17):
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
    // (all three axes, every score real), and the result says it was refused.
    // `p` is that copy on a refusal and `p_in` itself otherwise.
    const char* const refusal = (p_in.budget != nullptr)
        ? charter_spend_refusal(*p_in.budget, p_in.spend) : nullptr;
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
    const landscape_search_params& p = (refusal != nullptr) ? refused_params : p_in;

    // BL-1032. A BUDGET WORLD skips the roster axis (the budget decides the
    // roster). A null, empty or refused budget is not a budget world, and
    // everything below runs exactly as it did before the seam.
    const bool budget_world = p.budget != nullptr && !p.budget->empty();
    if (budget_world && p.print_rounds)
        std::printf("[landscape_search] charter budget: %zu centres, %lld points; roster axis "
                    "SKIPPED (the budget decides the roster)\n",
                    p.budget->points().size(),
                    static_cast<long long>(p.budget->total()));

    using clock = std::chrono::steady_clock;
    const auto ms_since = [](clock::time_point t0)
    {
        return std::chrono::duration<double, std::milli>(clock::now() - t0).count();
    };

    // --- the seed candidate -------------------------------------------------
    {
        const auto t0 = clock::now();
        scored s;
        s.cand = p.start;
        score_one(base, reg, p, s);
        out.seed_score   = s.score;
        out.winner       = s.cand;
        out.winner_score = s.score;
        out.evaluations  = 1;
        out.round_ms.push_back(ms_since(t0));
        if (p.print_rounds)
            std::printf("[landscape_search] seed   corps=%d placement=%08X tier=%u  "
                        "composite=%.6f  (%.0f ms)\n",
                        s.cand.corporation_count, s.cand.placement_seed,
                        static_cast<unsigned>(s.cand.road_tier), s.score.composite,
                        out.round_ms.back());
    }

    const int threads = std::max(1, p.thread_count);
    const int rounds  = std::max(0, p.rounds);

    for (int r = 0; r < rounds; ++r)
    {
        const auto round_t0 = clock::now();
        // --- propose, one per axis, in the fixed axis order ------------------
        // `live` holds the axes this round proposes, ascending. On every world
        // but a budget world that is all of them, {0, 1, 2}, and each loop below
        // walks it in exactly the order it walked the axes before BL-1032.
        //
        // A BUDGET WORLD SKIPS ROSTER: no draw, no proposal, no score, no path
        // step. SKIPPED, NEVER RE-KEYED — the draws are keyed by (round, axis),
        // so not drawing roster leaves the placement and road-tier proposals
        // exactly what they are on a legacy world, and the axis count and the
        // tag formula do not move.
        std::vector<scored> props(landscape_axis_count);
        std::vector<std::size_t> live;
        live.reserve(landscape_axis_count);
        for (int a = 0; a < landscape_axis_count; ++a)
        {
            const landscape_axis axis = static_cast<landscape_axis>(a);
            if (budget_world && axis == landscape_axis::roster)
                continue;
            checkpoint_rng rng = axis_rng(p.seed, r, axis);
            props[static_cast<std::size_t>(a)].cand = perturb(out.winner, axis, p, rng);
            live.push_back(static_cast<std::size_t>(a));
        }

        // --- score them INDEPENDENTLY ---------------------------------------
        // Results land at their own index, so nothing downstream can observe
        // which finished first. Each worker copies `base` itself: score_landscape
        // memoises into the world it scores, so a shared world would be both a
        // race and an ordering hazard (landscape_score.hpp § the consequence).
        if (threads <= 1)
        {
            for (const std::size_t i : live)
                score_one(base, reg, p, props[i]);
        }
        else
        {
            const int n = static_cast<int>(live.size());
            const int spawn = std::min(threads, n);
            std::vector<std::thread> pool;
            pool.reserve(static_cast<std::size_t>(spawn));
            for (int t = 0; t < spawn; ++t)
            {
                pool.emplace_back([&, t]
                {
                    for (int i = t; i < n; i += spawn)
                        score_one(base, reg, p, props[live[static_cast<std::size_t>(i)]]);
                });
            }
            for (std::thread& th : pool)
                th.join();
        }
        out.evaluations += static_cast<int>(live.size());

        // --- argmax over the proposals, by the TOTAL order -------------------
        // Proposal-vs-proposal ties are broken by `candidate_key_less`, so an
        // exact tie between two proposals resolves identically everywhere. This
        // key is deliberately NOT consulted against the incumbent below.
        std::size_t best = live.front();
        for (std::size_t k = 1; k < live.size(); ++k)
        {
            const std::size_t i = live[k];
            const int c = compare_landscape(props[i].score, props[best].score);
            if (c > 0 || (c == 0 && candidate_key_less(props[i].cand, props[best].cand)))
                best = i;
        }

        // --- replace only on a STRICT improvement ----------------------------
        // A tie leaves the incumbent standing. Consulting the candidate key here
        // would make an equal-scoring proposal unseat the incumbent and let the
        // walk churn between equals for the rest of its fixed rounds.
        const double incumbent_composite = out.winner_score.composite;
        const bool accept = compare_landscape(props[best].score, out.winner_score) > 0;
        if (accept)
        {
            out.winner       = props[best].cand;
            out.winner_score = props[best].score;
            ++out.accepted;
            ++out.accepted_by_axis[best];
        }

        out.round_ms.push_back(ms_since(round_t0));
        if (p.print_rounds)
        {
            const landscape_candidate& bc = props[best].cand;
            std::printf("[landscape_search] round %d  %-9s %s  corps=%d placement=%08X tier=%u  "
                        "composite=%.6f vs incumbent %.6f  (%.0f ms, %.0f ms/eval)\n",
                        r, landscape_axis_name(static_cast<landscape_axis>(static_cast<int>(best))),
                        accept ? "TAKEN" : "held ",
                        bc.corporation_count, bc.placement_seed,
                        static_cast<unsigned>(bc.road_tier), props[best].score.composite,
                        incumbent_composite, out.round_ms.back(),
                        out.round_ms.back() / static_cast<double>(live.size()));
        }

        for (const std::size_t i : live)
        {
            landscape_search_step step;
            step.round    = r;
            step.axis     = static_cast<landscape_axis>(static_cast<int>(i));
            step.proposal = props[i].cand;
            step.score    = props[i].score;
            step.accepted = accept && i == best;
            out.path.push_back(step);
        }
    }

    return out;
}
