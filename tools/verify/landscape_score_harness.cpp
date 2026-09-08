// ---------------------------------------------------------------------------
// landscape_score — BL-770. Does the phase 6 objective DISCRIMINATE?
//
// This harness exists to be able to FAIL, and that is its whole point. Ben's
// ruling on the market-work form (2026-09-06): "build the SCORER alone, score a
// handful of hand-made candidate rosters, and measure whether these terms
// discriminate between them at all. Flat completeness across every candidate
// means the search has nothing to search on and every line downstream of it is
// wasted."
//
// SLICE 1 SAID NO, AND IT WAS RIGHT TO. Five candidates whose fixtures differed
// by 20 corporations and 41 buildings scored IDENTICALLY on every term, because
// every term read tiles, markets and population and none of them read a roster.
// SLICE 2 ADDED THE TERM THAT DOES — actual against potential completeness — and
// the same five candidates now spread 3.2e-01 on the composite. The harness did
// not change its question between the two; only the answer moved.
//
// TWO CONTROLS, AND BOTH ARE LOAD-BEARING. Neither was here at first and each was
// added because its absence made a result unreadable:
//
//   R2.0, THE FIXTURE CONTROL. "The candidates score alike" means nothing unless
//   the candidates actually differ. Without this, a clamped corporation_count or
//   a no-op background pass produces the identical flat output and the identical
//   conclusion, for entirely the wrong reason.
//
//   SECTION B, THE WORLD CONTROL. "Flat" must be distinguishable from "the scorer
//   returns a constant", so it scores landscapes the objective MUST tell apart.
//   It holds the ROSTER CONSTANT and varies the world — generating no corps here
//   would measure "different world AND no roster", which is not what it claims.
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
    /// THE FIXTURE CONTROL. "The objective is flat across candidates" only means
    /// anything if the candidates are actually DIFFERENT. If generate_corporations
    /// clamped the count, or generate_background_firms no-opped, the output would
    /// be identically flat and the conclusion identically stated — for entirely
    /// the wrong reason. So each candidate records what it actually built, and
    /// the run asserts these MOVE before it is allowed to conclude anything from
    /// the scores not moving.
    int corps     = 0;
    int buildings = 0;
};

/// Count what a candidate landscape actually contains, so the fixture can be
/// shown to differ independently of the scorer.
void note_fixture(candidate& c, const world& w)
{
    c.corps = static_cast<int>(w.corporations.size());
    for (const auto& kv : w.buildings)
    {
        (void)kv;
        ++c.buildings;
    }
}

void print_row(const candidate& c)
{
    const landscape_score& s = c.score;
    std::printf("  %-26s  potential=%.5f  ACTUAL=%.5f  realised=%.3f  balance=%.5f  "
                "spread=%.5f  composite=%.6f  (%d mkts, %d corps, %d bldgs)\n",
                c.label.c_str(), s.mean_completeness, s.mean_actual, s.realisation,
                s.mean_balance, s.spread, s.composite, s.market_count,
                c.corps, c.buildings);
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
        return -1.0;   // every candidate scored zero: NOT the same as "all equal"
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
        { "ACTUAL completeness", &landscape_score::mean_actual },
        { "realisation", &landscape_score::realisation },
        { "supply:demand balance", &landscape_score::mean_balance },
        { "spread (unevenness)", &landscape_score::spread },
        { "composite", &landscape_score::composite },
    };
    std::printf("\n  discrimination over %s:\n", what);
    for (const auto& t : terms)
    {
        const double r = relative_range(cs, t.p);
        if (r < 0.0)
            std::printf("    %-24s ALL CANDIDATES ZERO — the term is DEAD, not flat\n", t.name);
        else
            std::printf("    %-24s relative range %.3e   %s\n", t.name, r,
                        r > kDiscriminates ? "DISCRIMINATES" : "FLAT — sees no difference");
    }
}

} // namespace

int main()
{
    std::printf("landscape_score — BL-770, does the phase 6 objective discriminate?\n");

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
        note_fixture(rosters.back(), w);
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
        note_fixture(rosters.back(), w);
        print_row(rosters.back());
    }

    // R2.0 - THE FIXTURE CONTROL, and it gates the conclusion. Assert the
    // candidates genuinely differ BEFORE reading anything into their scores
    // being identical. Without this, "the objective cannot see a roster" is
    // indistinguishable from "there was only ever one roster".
    {
        int cmin = rosters[0].corps, cmax = cmin, bmin = rosters[0].buildings, bmax = bmin;
        for (const candidate& c : rosters)
        {
            if (c.corps < cmin) cmin = c.corps;
            if (c.corps > cmax) cmax = c.corps;
            if (c.buildings < bmin) bmin = c.buildings;
            if (c.buildings > bmax) bmax = c.buildings;
        }
        std::printf("\n  fixture spread: corps %d..%d, buildings %d..%d\n",
                    cmin, cmax, bmin, bmax);
        check(cmax > cmin || bmax > bmin, "R2.0",
              "the CANDIDATES THEMSELVES differ - a flat score is about the objective, "
              "not about an unchanged fixture");
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

        // THE CONTROL HOLDS THE ROSTER CONSTANT AND VARIES THE WORLD. The first
        // cut generated NO corporations here, which made section B measure
        // "different world AND no roster at all" - and once the objective became
        // roster-aware that collapsed every control composite toward zero, so the
        // control stopped controlling for the thing it names. Same corp params,
        // same seed, different world.
        corporation_params cp;
        cp.corporation_count = 8;
        generate_corporations(w, cp, 0xC0FFEEu);
        generate_background_firms(w, reg, 0xC0FFEEu);

        char lbl[64];
        std::snprintf(lbl, sizeof lbl, "world seed=%08X", s);
        worlds.push_back({ lbl, score_landscape(w, reg) });
        note_fixture(worlds.back(), w);
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
                    "    Phase 6 has something to search on. The SEARCH itself is the next slice.\n",
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
