// ---------------------------------------------------------------------------
// landscape_score — BL-770 slice 1. Does the phase 6 objective DISCRIMINATE?
//
// This harness exists to be able to FAIL, and that is its whole point. Ben's
// ruling on the market-work form (2026-09-06): "build the SCORER alone, score a
// handful of hand-made candidate rosters, and measure whether these terms
// discriminate between them at all. Flat completeness across every candidate
// means the search has nothing to search on and every line downstream of it is
// wasted."
//
// So it does not assert that the scorer is good. It asserts that the scorer is
// HONEST — deterministic, and able to say plainly whether it can see the thing
// phase 6 would search over.
//
// THE POSITIVE CONTROL IS THE LOAD-BEARING PART. Measuring "candidate A and
// candidate B score the same" proves nothing on its own: a scorer that returns a
// constant would pass that test too. So every run also scores landscapes the
// objective MUST be able to tell apart (different worlds). If the control moves
// and the candidates do not, the finding is specific and real; if neither moves,
// the scorer is broken and says so instead.
//
// Build:  bash tools/verify/build_lua_harness.sh landscape_score_harness
// ---------------------------------------------------------------------------

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/corporation_generation.hpp"
#include "world/hard_coded_world.hpp"
#include "world/landscape_score.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace
{

int failures = 0;

void check(bool ok, const std::string& id, const std::string& what)
{
    std::printf("  [%s] %s  %s\n", ok ? "PASS" : "FAIL", id.c_str(), what.c_str());
    if (!ok)
        ++failures;
}

struct candidate
{
    std::string      label;
    landscape_score  score;
};

void print_row(const candidate& c)
{
    const landscape_score& s = c.score;
    std::printf("  %-26s  complete=%.5f  balance=%.5f  spread=%.5f  composite=%.6f  (%d markets)\n",
                c.label.c_str(), s.mean_completeness, s.mean_balance, s.spread,
                s.composite, s.market_count);
}

/// Largest relative gap between any two candidates on one term.
double relative_range(const std::vector<candidate>& cs, double landscape_score::* term)
{
    if (cs.size() < 2)
        return 0.0;
    double lo = cs[0].score.*term, hi = lo;
    for (const candidate& c : cs)
    {
        const double v = c.score.*term;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    if (hi <= 0.0)
        return 0.0;
    return (hi - lo) / hi;
}

/// A term DISCRIMINATES if the spread between candidates is more than a
/// rounding artefact. 1e-9 is deliberately near-zero: the question here is
/// "does this term move AT ALL", not "does it move enough to tune on".
constexpr double kDiscriminates = 1e-9;

void report_discrimination(const char* what, const std::vector<candidate>& cs)
{
    struct { const char* name; double landscape_score::* p; } terms[] = {
        { "chain completeness", &landscape_score::mean_completeness },
        { "supply:demand balance", &landscape_score::mean_balance },
        { "spread (unevenness)", &landscape_score::spread },
        { "composite", &landscape_score::composite },
    };
    std::printf("\n  discrimination over %s:\n", what);
    for (const auto& t : terms)
    {
        const double r = relative_range(cs, t.p);
        std::printf("    %-24s relative range %.3e   %s\n", t.name, r,
                    r > kDiscriminates ? "DISCRIMINATES" : "FLAT — sees no difference");
    }
}

} // namespace

int main()
{
    std::printf("landscape_score — BL-770 slice 1, does the phase 6 objective discriminate?\n");

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    const world_gen_config gen_cfg = parsed_gen_config(lua);

    // The standing vacuity guard: a registry that loaded nothing would report
    // every recipe as costless and every chain as closed.
    if (reg.recipe_count() == 0)
    {
        std::printf("FATAL: no recipes loaded — run from the repo root.\n");
        return 2;
    }

    // ------------------------------------------------------------------
    // A. CANDIDATE ROSTERS on ONE fixed world — what phase 6 actually varies.
    //    Ben's point 3: candidates vary rosters, placements and road tiers,
    //    NOT whole upstream worlds, so generation runs once and only the
    //    economic layer repeats.
    // ------------------------------------------------------------------
    const world_params wp = no_prehistory();

    std::printf("\nA. candidate ROSTERS on one fixed world (seed default)\n");
    std::vector<candidate> rosters;
    const int roster_counts[] = { 4, 8, 16 };
    for (const int n : roster_counts)
    {
        world w = make_hard_coded_world(wp, nullptr, gen_cfg);
        assign_default_recipes(w, reg);

        corporation_params cp;
        cp.corporation_count = n;
        generate_corporations(w, cp, 0xC0FFEEu);
        generate_background_firms(w, reg, 0xC0FFEEu);

        rosters.push_back({ "corps=" + std::to_string(n), score_landscape(w, reg) });
        print_row(rosters.back());
    }

    // Same count, different placement seed — the other axis Ben named.
    for (const uint32_t s : { 0x1111u, 0x2222u })
    {
        world w = make_hard_coded_world(wp, nullptr, gen_cfg);
        assign_default_recipes(w, reg);

        corporation_params cp;
        cp.corporation_count = 8;
        generate_corporations(w, cp, s);
        generate_background_firms(w, reg, s);

        char lbl[64];
        std::snprintf(lbl, sizeof lbl, "corps=8 placement=%08X", s);
        rosters.push_back({ lbl, score_landscape(w, reg) });
        print_row(rosters.back());
    }

    report_discrimination("CANDIDATE ROSTERS (one world)", rosters);

    // ------------------------------------------------------------------
    // B. THE POSITIVE CONTROL — landscapes the objective MUST tell apart.
    //    Without this, "the candidates score alike" is indistinguishable
    //    from "the scorer returns a constant".
    // ------------------------------------------------------------------
    std::printf("\nB. positive control — DIFFERENT WORLDS (the scorer must see these)\n");
    std::vector<candidate> worlds;
    for (const uint32_t s : { 0xABCDEF01u, 0x5EED0002u, 0x5EED0003u })
    {
        world_params p = wp;
        p.seed = s;
        world w = make_hard_coded_world(p, nullptr, gen_cfg);
        assign_default_recipes(w, reg);

        char lbl[64];
        std::snprintf(lbl, sizeof lbl, "world seed=%08X", s);
        worlds.push_back({ lbl, score_landscape(w, reg) });
        print_row(worlds.back());
    }
    report_discrimination("DIFFERENT WORLDS (the control)", worlds);

    // ------------------------------------------------------------------
    // R3 — purity and determinism. Asserted from the first slice, because it
    // is the binding constraint on the eventual parallel search and is far
    // cheaper to keep than to retrofit.
    // ------------------------------------------------------------------
    std::printf("\nR3. purity and determinism\n");
    {
        world w = make_hard_coded_world(wp, nullptr, gen_cfg);
        assign_default_recipes(w, reg);
        corporation_params cp;
        generate_corporations(w, cp, 0xC0FFEEu);

        const landscape_score a = score_landscape(w, reg);
        const landscape_score b = score_landscape(w, reg);
        const landscape_score c = score_landscape(w, reg);
        const bool same = a.composite == b.composite && b.composite == c.composite
                       && a.mean_completeness == b.mean_completeness
                       && a.mean_balance == b.mean_balance
                       && a.spread == b.spread;
        check(same, "R3.1", "scoring the same world three times is bit-identical");
        check(a.market_count > 0, "R3.2", "the scorer found markets to score at all");
        check(a.mean_completeness > 0.0 || a.mean_balance > 0.0, "R3.3",
              "the objective is not identically zero (a zero scorer discriminates nothing)");
    }

    // ------------------------------------------------------------------
    // The verdict the slice exists to produce.
    // ------------------------------------------------------------------
    const double roster_move  = relative_range(rosters, &landscape_score::composite);
    const double control_move = relative_range(worlds,  &landscape_score::composite);

    std::printf("\n--- THE SLICE'S QUESTION ---\n");
    check(control_move > kDiscriminates, "R2.1",
          "the positive control MOVES — the scorer is not returning a constant");

    if (roster_move > kDiscriminates)
    {
        std::printf("  VERDICT: the objective DISCRIMINATES between candidate rosters\n"
                    "    (composite relative range %.3e over %zu candidates).\n"
                    "    Phase 6 has something to search on; slice 2 is the search.\n",
                    roster_move, rosters.size());
    }
    else
    {
        std::printf("  VERDICT: the objective is FLAT ACROSS CANDIDATE ROSTERS.\n"
                    "    Composite relative range %.3e over %zu candidates, against %.3e\n"
                    "    for the control. The terms are computed from TILES, MARKETS and\n"
                    "    POPULATION — none of which a roster moves — so they measure the\n"
                    "    WORLD'S saturation potential, not a candidate's realisation of it.\n"
                    "    Phase 6 cannot search on this objective as it stands. A roster-aware\n"
                    "    term is owed before the search is worth building.\n",
                    roster_move, rosters.size(), control_move);
    }

    // ------------------------------------------------------------------
    // R4 — the provisional caveat, printed with the figures behind it.
    // ------------------------------------------------------------------
    std::printf("\n--- SCORES HERE ARE ORDINAL AND PROVISIONAL ---\n"
                "  Sprint 33's growth gate is UNMET: on the standard 1960 lapse, valued\n"
                "  production falls 11738 -> 6693 on seed 0 and 5845 -> 3971 on seed 1\n"
                "  (re-based 2026-09-06, BL-759). Ben accepted the gate unmet on the\n"
                "  market-work form. A search over a shrinking field ranks degrees of\n"
                "  failure, so these scores order candidates and DO NOT evidence viability.\n");

    std::printf("\n%s — %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
