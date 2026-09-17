// player_seed_sweep — which seeds give the PLAYER a corp worth playing?
//
// Written 2026-08-16 after a live look at an autostarted campaign found the
// player corp holding no processing facility at all: the Method page (BL-430/431's
// whole surface) had nothing to show, because the seed had handed the player a
// pure-extraction corp. That is not a rendering bug and no existing harness would
// ever have caught it — `seed_sweep_probe` asks only whether generation THREW,
// and `pregame_balance_harness` asks a rich question of exactly one seed.
//
// WHAT "WORTH PLAYING" MEANS HERE, stated so the verdict is arguable rather than
// magic. Two conditions, both about the opening position the player is handed:
//
//   * PROCESSING — the corp owns at least one processing_facility. Without one
//     the chain-depth ladder (BL-428), alternate methods (BL-430) and the whole
//     Method page are unreachable from the start position; the player can build
//     one, but the opening reads as a strictly poorer game.
//   * SOLVENT — the corp's balance after the settle is positive AND it never
//     dipped below zero on the way. Ending up positive after a trip through
//     insolvency is a different (worse) opening than never dipping, so both are
//     reported separately rather than collapsed into one flag.
//
// This harness REPORTS. It deliberately does not filter, hard-code a whitelist,
// or reject seeds at generation — which of those to do is a design call
// (backlog: starting-corp selection), and a sweep that silently enforced one
// would be making that call by implication.
//
// Determinism: every seed is generated from world_params alone and the settle
// is the same fixed sequence pregame_balance_harness uses, so a rerun reproduces
// the table exactly.
//
// Run: .\build\player_seed_sweep.exe [seed_count] [settle_ticks]
//      .\build\player_seed_sweep.exe --seat  [seed_count] [--fast]
//      .\build\player_seed_sweep.exe --guard [seed_count] [--fast]
//      .\build\player_seed_sweep.exe --guard --seeds 46,17,11 [--reproduce N] [--fast]
//
// BL-630 (2026-08-26) ADDED THE MODE THIS FILE NOW LEADS WITH. The two default
// conditions above ("worth playing" == a processor, and solvent) were written
// when the seed alone decided the player's corp. They no longer decide anything:
// the player is SEATED after the settle, on a corp drawn from a viability
// shortlist with a bias toward processing and population. `--seat` measures what
// that draw actually produces and `--guard` asserts the properties it must hold;
// the original sweep is kept below because it still answers the question it was
// written for - how good is the GENERATOR'S opening, before any seat logic.

#include "scripting/lua_state.hpp"
#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/nation_step.hpp"
#include "world/spawn_seat.hpp"
#include "world/supply_system.hpp"
#include "world/tech_gate.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

/// The same tick composition pregame_balance_harness runs, so a balance printed
/// here is directly comparable with the one printed there.
void tick(world& w, const recipe_registry& reg, int t)
{
    dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                     reg.logistics_cost(convoy_mode::space));
    advance_convoys(w);
    const economy_report report = run_economy_step(w, reg);
    const auto flows = clear_markets(w, reg, report);
    apply_budget(w, reg, flows, report.workforce_contention, nullptr);
    credit_arrived_convoys(w, t);
}

void seed_default_recipes(world& w, const recipe_registry& reg)
{
    const entity_id default_recipe = reg.recipe_id("steel");
    for (auto& [id, b] : w.buildings)
        if (b.type == building_type::processing_facility && b.recipe == no_recipe)
            b.recipe = static_cast<uint16_t>(default_recipe);
}

struct seed_row
{
    uint32_t seed        = 0;
    int      processing  = 0;
    int      extraction  = 0;
    int      other       = 0;
    float    opening     = 0.0f;
    float    final_bal   = 0.0f;
    bool     went_negative = false;
    bool     threw       = false;
};

bool playable(const seed_row& r)
{
    return !r.threw && r.processing >= 1 && r.final_bal > 0.0f && !r.went_negative;
}

// --- BL-435: the roster mode ------------------------------------------------
//
// `player_seed_sweep --roster <seed>` prints EVERY corporation's opening for one
// seed, not just the one the generator handed the player. Two jobs, and it is one
// mode because they want exactly the same numbers:
//
//   * It is the measurement behind BL-435. The sweep's headline finding — 13/24
//     seeds give the player a pure-extraction corp — says nothing about whether
//     a BETTER corp existed in the same world to be chosen instead. This answers
//     that directly, and a selection screen is only worth building if it does.
//   * It is the data the selection screen renders. Name, focus, buildings by
//     type, balance, per corp. Producing it here first means the screen is built
//     against a shape already known to be reachable, rather than the other way
//     round.
//
// It deliberately does NOT run the settle: these are the GENERATED openings,
// which is what a player choosing before the first tick would be choosing between.
struct corp_row
{
    entity_id   id         = null_entity;
    std::string name;
    int         processing = 0;
    int         extraction = 0;
    int         other      = 0;
    float       balance    = 0.0f;
    bool        is_player  = false;
    bool        specialist = false;
};

int run_roster(uint32_t seed, const recipe_registry& reg)
{
    world_params p = no_prehistory();
    p.seed = seed;
    world w = make_hard_coded_world(p);
    seed_default_recipes(w, reg);

    // Snapshot the corp set BEFORE background firms are added. The distinction is
    // load-bearing for BL-435 and invisible afterwards: the generator's player
    // pick draws only from these SPECIALIST corps (corporation_generation.cpp's
    // uniform draw over corp_ids), while generate_background_firms (BL-365) adds
    // the mundane industry that saturates the market. "The world contains corps
    // with processors" is true of the background firms almost by construction, so
    // counting them together would answer a question nobody asked.
    std::vector<entity_id> specialists;
    for (const auto& [id, cc] : w.corporations)
        specialists.push_back(id);
    std::sort(specialists.begin(), specialists.end());

    // The landscape-search WINNER, as the app applies it — not the seed candidate
    // (BL-979; apply_shipped_landscape in harness_params.hpp). This also runs the
    // app's second assign_default_recipes pass, which the bare call here skipped.
    apply_shipped_landscape(w, reg, seed);

    std::vector<corp_row> rows;
    for (const auto& [id, cc] : w.corporations)
    {
        corp_row r;
        r.id        = id;
        r.name      = cc.name;
        r.balance   = cc.balance;
        r.is_player = (cc.is_player || id == w.player_entity);
        r.specialist = std::binary_search(specialists.begin(), specialists.end(), id);
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            switch (bit->second.type)
            {
                case building_type::processing_facility: ++r.processing; break;
                case building_type::extraction_site:     ++r.extraction; break;
                default:                                 ++r.other;      break;
            }
        }
        rows.push_back(std::move(r));
    }
    // Sort by entity id: w.corporations is an unordered map, and an unordered
    // walk would make this table differ run to run on the same seed (the BL-406
    // lesson). Ids are assigned in generation order, so this is also the order
    // a selection screen would list them in.
    std::sort(rows.begin(), rows.end(),
              [](const corp_row& a, const corp_row& b) { return a.id < b.id; });

    std::printf("player_seed_sweep --roster %u — every corporation's generated opening\n\n", seed);
    std::printf("  #  kind  proc  extr  other  player  name\n");
    std::printf("---  ----  ----  ----  -----  ------  ----\n");
    int spec_total = 0, spec_proc = 0, bg_total = 0, bg_proc = 0;
    int with_proc = 0, idx = 0;
    for (const corp_row& r : rows)
    {
        if (r.specialist) { ++spec_total; if (r.processing >= 1) ++spec_proc; }
        else              { ++bg_total;   if (r.processing >= 1) ++bg_proc;   }
        if (r.processing >= 1)
            ++with_proc;
        std::printf("%3d  %4s  %4d  %4d  %5d  %6s  %s\n",
                    idx++, r.specialist ? "SPEC" : "bg",
                    r.processing, r.extraction, r.other,
                    r.is_player ? "<<<" : "", r.name.c_str());
    }

    // Ben, 2026-08-16: "we shouldn't be seeding such corporations as the default
    // one, which has no way of making money at all." A port and a military_base
    // both carry base_rate 0 — they produce NOTHING — so a corp holding only
    // those has no income source whatsoever. That is a different and worse
    // failure than "no processor": not a poor opening, a dead one.
    int spec_dead = 0;
    for (const corp_row& r : rows)
        if (r.specialist && r.processing == 0 && r.extraction == 0)
            ++spec_dead;

    std::printf("\n=== seed %u ===\n", seed);
    std::printf("  CANNOT PRODUCE AT ALL (no extraction, no processing): %d of %d specialists%s\n",
                spec_dead, spec_total, spec_dead ? "   <-- DEAD START" : "");
    std::printf("  SPECIALIST corps (the pool the player pick draws from): %2d, %2d with a processor\n",
                spec_total, spec_proc);
    std::printf("  background firms (BL-365, not currently selectable):    %2d, %2d with a processor\n",
                bg_total, bg_proc);
    for (const corp_row& r : rows)
        if (r.is_player)
            std::printf("  generator handed the player: %s [%s] — %d proc, %d extr, %d other\n",
                        r.name.c_str(), r.specialist ? "specialist" : "background",
                        r.processing, r.extraction, r.other);
    std::printf("  BL-435's premise needs spec_proc >= 1 while the player's own proc == 0.\n");
    return 0;
}

// --- BL-630: the seat sweep --------------------------------------------------
//
// `player_seed_sweep --seat  [n] [--fast]`  REPORTS the seat distribution.
// `player_seed_sweep --guard [n] [--fast]`  asserts what the seat must hold.
//
// One implementation, two verdicts, and the split is the same one this file has
// always drawn: the distribution is a DESIGN reading whose answer is arguable,
// so it prints and exits 0; the guard's rows are properties that are not
// arguable, so they fail loudly.
//
// WHY THIS MODE HAD TO REPLACE THE OLD ONE. The retired `--guard` asserted the
// properties a SELECTION SCREEN depended on — "every seed offers a choice of at
// least two", "the specialist pool stays small enough to list". The screen is
// gone (BL-630), so those rows guarded nothing. What replaces them are the
// shortlist's own properties, and above all the one the design leans hardest on:
// THE WEIGHTING IS A BIAS AND NEVER A SECOND GATE. A shortlisted pure-extraction
// corp on thin ground must stay drawable, only less often — S3 below is what
// makes that checkable rather than merely asserted in a comment.
//
// R3 IS MEASURED, NOT ASSERTED (CORPORATION_GENERATION.md: "the weights are a
// first cut, and what matters more than their values is that the sweep REPORTS
// the resulting distribution rather than asserting it against a target"). There
// is deliberately NO row here of the form "a processor is seated at least X% of
// the time". A bias is not a guarantee, and a harness that pretended otherwise
// would turn the first cut into a contract nobody chose.
//
// THE REAL SPAWN BY DEFAULT, unlike this file's older modes. The population
// weight reads the settlement pass's output, so a world with its pre-epoch
// year-tick sim switched off is a different quantity wearing the same name —
// the scope declaration `no_prehistory()` makes elsewhere in this file does not
// hold for this question. `--fast` is for iteration only and says so in its
// header line, exactly as spawn_solvency's does.

/// THE SETTLE IS THE APP'S, through one shared helper (BL-1030, 2026-09-17).
/// `run_app_validation_settle` (harness_params.hpp) is app::poll_worldgen's
/// validation run tick for tick, spectating included (BL-630: nobody is seated
/// through the settle, so every corp is scorer-driven). The local tick this
/// file used to run had drifted from the app in four ways at once — ticks
/// numbered from 1 rather than 0, `world::current_day_tick` written as the tick
/// where the app leaves it untouched, the tick rather than day 0 passed to
/// credit_arrived_convoys (and through it intercept_convoys), and no
/// run_firm_exits — and the world it settled was itself built without the
/// world-gen config, the works registry or the era band. The helper's comment
/// carries the app.cpp line for each.
///
/// The settle: phase 6's single validation run, after which the app seats the
/// player (ERAS.md § The opening position; app::poll_worldgen calls seat_player
/// when it closes). `app::validation_ticks`, through the helper's restatement.
/// NOT a longer history by choice: the subject is the seat the game draws, and
/// the one trailing window the draw reads (k_spawn_trailing_quarters, 8) fits
/// inside the settle whole. Re-read under BL-1008, 2026-09-16 — see THE SETTLE
/// RE-READ (taken on the pre-BL-1030 settle).
constexpr int k_settle_ticks = k_app_validation_ticks;

// THE SETTLE RE-READ (BL-1008, 2026-09-16) — TAKEN BEFORE BL-1020 re-cut the floor
// onto phase 6's static landscape score (merged the same day). The "WHAT MOVED"
// reasoning below describes the trailing-net floor BL-1020 retired; the settle
// length it re-reads (80 -> 12) is unchanged by that re-cut.
//
// (original note) — `--guard 3`, the shipped spawn,
// settle 80 -> 12. A reading, not a re-pin: no row was touched.
//
//   seed  80 ticks: short / balance / trail8 / seat    12 ticks: same columns
//   0     0 UNMET /   37 /  -7 / Borex-JorarHexis      0 UNMET /  406 /  -7 / Orban-SolaxAthix
//   1     3       / 1490 / 350 / OrbanXeris-TeryxDelur 3       / 1576 /  18 / PaxenJorax-Genan
//   2     0 UNMET /   46 /  -7 / Huth Extraction       0 UNMET /  259 /  -7 / IntelNexan
//
// NO GUARD ROW MOVED ON SUBSTANCE. At 12, S1-S5 PASS. At 80, the 3-seed run
// threw std::bad_alloc on seed 0 (a machine under memory pressure, the same
// minute the compiler ran out of heap) and read S1 FAIL; seed 0 re-run alone
// seated a specialist, and S3 over seeds 0-2 carries seed 1's 5.00 weight.
// WHAT MOVED: the seated corp on all three seeds, because both mechanisms read
// the trailing returns the settle filed — the fallback takes the highest
// trailing net, and the floor filters on it — and 12 quarters rank the field
// differently from 80. The floor-unmet split (2 of 3) and the processor share
// (3 of 3) held. Seat balances on the unmet seeds are higher (406 / 259 against
// 37 / 46) because 12 quarters have not yet drained the starting capital.
/// How many seeds get the two-independently-built-worlds treatment (S4).
/// The default; `--reproduce N` overrides it, so a sweep split across processes
/// (BL-1020 ran the sixteen curated seeds as three) does not pay it three times.
constexpr int k_reproduce_seeds  = 4;

struct seat_row
{
    uint32_t  seed          = 0;
    bool      threw         = false;
    entity_id seated        = null_entity;
    bool      seated_is_specialist = false;
    bool      floor_unmet   = false;
    int       specialists   = 0;
    int       shortlisted   = 0;
    /// The seated corp's own facts.
    bool      seat_processor = false;
    float     seat_pop_share = 0.0f;
    float     seat_weight    = 0.0f;
    float     seat_balance   = 0.0f;
    float     seat_trailing  = 0.0f;
    /// The seated corp's static landscape score — what the floor gated on.
    double    seat_landscape = 0.0;
    /// The SHORTLIST's composition, so the distribution can be read against what
    /// was actually on offer rather than against the whole specialist set.
    int       shortlisted_with_proc = 0;
    int       shortlisted_near_pop  = 0;   ///< population_share > 0.
    /// Shortlisted corps whose settle closed at a balance <= 0, or whose trailing
    /// net is negative — the two facts the RETIRED floor gated on, counted so the
    /// sweep shows what moving the gate onto the static score let through
    /// (BL-1020). Information, never a verdict.
    int       shortlisted_insolvent = 0;
    int       shortlisted_trail_neg = 0;
    /// Specialists the RETIRED floor would have passed — solvent AND trailing
    /// net >= 0, the exact test seat_player_corporation applied before BL-1020 —
    /// over the same settled world. The before-and-after of the gate in one
    /// run: the settle does not depend on the seat, so this IS the old shortlist.
    int       retired_floor_passed  = 0;
    /// The smallest weight any shortlisted corp carried, over this seed. S3's
    /// input: it must never reach zero, or the bias has become a gate.
    float     min_shortlist_weight  = 0.0f;
    /// A second, independently built world on the same seed seated the same corp.
    /// Only sampled over the first `k_reproduce_seeds`.
    bool      reproduced        = false;
    bool      reproduce_checked = false;
};

/// Build one world, settle it, seat it — the app's campaign start end to end.
/// Returns the seat result; @p out holds the world (and the registry, config
/// and search it was built from), because the caller needs both to describe
/// what was seated.
spawn_seat_result build_and_seat(lua_state& lua, uint32_t seed, bool fast,
                                 app_start_world& out)
{
    world_params p = fast ? no_prehistory() : world_params{};
    p.seed = seed;
    // app::begin_new_game + app::start_new_game_prelude: config and works, the
    // world, setup_world's writes, load_economy with its era band, the
    // landscape-search WINNER (not the seed candidate — BL-979) and the second
    // recipe pass (BL-1030; harness_params.hpp).
    build_app_start_world(lua, p, out);
    // app::poll_worldgen's validation run (BL-1030; harness_params.hpp).
    run_app_validation_settle(out.w, out.reg);
    // app::seat_player (app.cpp:933): the seat on the WINNER'S STATIC SCORE
    // (BL-1020) — the one the app keeps as `m_landscape_winner_score`.
    return seat_player_corporation(out.w, p.seed, out.land.search.winner_score);
}

int run_seat(const std::vector<uint32_t>& seeds, lua_state& lua, bool fast,
             bool assert_mode, int reproduce_seeds)
{
    const int n_seeds = static_cast<int>(seeds.size());
    std::printf("player_seed_sweep %s — %d seeds, %d settle ticks (app::validation_ticks) in spectate, %s spawn\n",
                assert_mode ? "--guard" : "--seat", n_seeds, k_settle_ticks,
                fast ? "FAST (prehistory OFF — iteration only, NOT the shipped spawn)"
                     : "the shipped");
    std::printf("BL-630. The floor filters; the draw over the shortlist is BIASED, never gated.\n");
    std::printf("BL-1020. The floor reads phase 6's STATIC landscape score; balance and trail8 "
                "are information.\n");
    std::printf("BL-1030. Built and settled in app order: world_gen.lua config, works.lua, "
                "set_era, the config's roster count, the validation run's own tick state.\n\n");

    std::printf("seed  spec  short  old  land    proc  pop%%  weight   balance   trail8  unmet  seated\n");
    std::printf("----  ----  -----  ---  ------  ----  ----  ------  --------  -------  -----  ------\n");

    std::vector<seat_row> rows;
    rows.reserve(static_cast<std::size_t>(n_seeds));

    for (int i = 0; i < n_seeds; ++i)
    {
        seat_row r;
        r.seed = seeds[static_cast<std::size_t>(i)];
        std::string seat_name = "-";
        try
        {
            // Heap-held: a start carries a world, a registry, a config and a
            // generation report, and two of them live at once under S4.
            const auto start = std::make_unique<app_start_world>();
            const spawn_seat_result res = build_and_seat(lua, r.seed, fast, *start);
            const world& w = start->w;

            r.seated      = res.seated;
            r.floor_unmet = res.floor_unmet;
            r.specialists = res.specialist_count;
            r.shortlisted = res.shortlist_size;

            if (const auto cit = w.corporations.find(res.seated); cit != w.corporations.end())
            {
                seat_name = cit->second.name;
                r.seated_is_specialist = !cit->second.is_background;
                r.seat_balance         = cit->second.balance;
            }

            bool first = true;
            for (const spawn_seat_candidate& c : res.candidates)
            {
                if (c.corp == res.seated)
                {
                    r.seat_processor = c.has_processor;
                    r.seat_pop_share = c.population_share;
                    r.seat_weight    = c.weight;
                    r.seat_trailing  = c.trailing_net;
                    r.seat_landscape = c.landscape;
                }
                if (c.solvent && c.trailing_net >= 0.0f)
                    ++r.retired_floor_passed;
                if (!c.shortlisted)
                    continue;
                if (c.has_processor)          ++r.shortlisted_with_proc;
                if (c.population_share > 0.0f) ++r.shortlisted_near_pop;
                if (!c.solvent)               ++r.shortlisted_insolvent;
                if (c.trailing_net < 0.0f)    ++r.shortlisted_trail_neg;
                if (first || c.weight < r.min_shortlist_weight)
                    r.min_shortlist_weight = c.weight;
                first = false;
            }

            // S4's input: the SAME seed, a SECOND INDEPENDENTLY BUILT WORLD.
            // Re-seating the same world in place would prove only that the
            // function is a function; the spectator_determinism convention,
            // applied to a draw.
            //
            // SAMPLED, not exhaustive: a second world doubles the sweep's cost
            // (generation, the landscape search and the settle each), and the property is
            // structural — an unordered walk or an unseeded stream would break
            // on the first seed, not the twentieth. The sample size is stated in
            // S4's own row so nobody reads it as a full sweep.
            if (i < reproduce_seeds)
            {
                const auto start2 = std::make_unique<app_start_world>();
                const spawn_seat_result res2 = build_and_seat(lua, r.seed, fast, *start2);
                // The ranked candidate list too, score for score: the static
                // score is the gate and the order now (BL-1020), so a score that
                // drifted between two builds would move the shortlist silently.
                bool same_rank = res2.candidates.size() == res.candidates.size();
                for (std::size_t k = 0; same_rank && k < res.candidates.size(); ++k)
                    same_rank = res2.candidates[k].corp == res.candidates[k].corp
                             && res2.candidates[k].landscape == res.candidates[k].landscape;
                r.reproduced = (res2.seated == res.seated)
                            && (res2.shortlist_size == res.shortlist_size)
                            && (res2.floor_unmet == res.floor_unmet)
                            && same_rank;
                r.reproduce_checked = true;
            }

            // The whole ranking, so the gate's discrimination is readable per
            // seed: `*` the seat, `x` below the floor. Printed before the row.
            std::printf("      rank:");
            for (const spawn_seat_candidate& c : res.candidates)
                std::printf(" %.4f%s", c.landscape,
                            c.corp == res.seated ? "*" : (c.shortlisted ? "" : "x"));
            std::printf("\n");
        }
        catch (const std::exception& e)
        {
            r.threw = true;
            std::printf("%4u  THREW: %s\n", r.seed, e.what());
        }
        catch (...)
        {
            r.threw = true;
            std::printf("%4u  THREW: unknown\n", r.seed);
        }

        if (!r.threw)
            std::printf("%4u  %4d  %5d  %3d  %6.4f  %4s  %4.0f  %6.2f  %8.0f  %7.0f  %5s  %s%s\n",
                        r.seed, r.specialists, r.shortlisted, r.retired_floor_passed,
                        r.seat_landscape,
                        r.seat_processor ? "YES" : "no",
                        static_cast<double>(r.seat_pop_share * 100.0f),
                        static_cast<double>(r.seat_weight),
                        static_cast<double>(r.seat_balance),
                        static_cast<double>(r.seat_trailing),
                        r.floor_unmet ? "UNMET" : "-",
                        seat_name.c_str(),
                        (r.reproduce_checked && !r.reproduced) ? "   <-- NOT REPRODUCED" : "");
        std::fflush(stdout);
        rows.push_back(r);
    }

    // --- R3: the distribution, reported ------------------------------------
    int done = 0, with_proc = 0, near_pop = 0, both = 0, neither = 0;
    int drawn = 0, drawn_proc = 0, drawn_pop = 0;
    int unmet = 0, non_specialist = 0, unseated = 0, threw = 0;
    int not_reproduced = 0, reproduce_checked = 0;
    int total_short = 0, total_short_proc = 0, total_short_pop = 0, total_spec = 0;
    int total_short_insolvent = 0, total_short_trail_neg = 0, empty_shortlist = 0;
    int total_retired = 0, empty_retired = 0;
    float min_weight = 0.0f;
    bool  have_weight = false;
    for (const seat_row& r : rows)
    {
        if (r.threw) { ++threw; continue; }
        ++done;
        total_spec  += r.specialists;
        total_short += r.shortlisted;
        total_short_proc += r.shortlisted_with_proc;
        total_short_pop  += r.shortlisted_near_pop;
        total_short_insolvent += r.shortlisted_insolvent;
        total_short_trail_neg += r.shortlisted_trail_neg;
        if (r.shortlisted == 0)
            ++empty_shortlist;
        total_retired += r.retired_floor_passed;
        if (r.retired_floor_passed == 0)
            ++empty_retired;
        if (!r.floor_unmet)
        {
            ++drawn;
            if (r.seat_processor)        ++drawn_proc;
            if (r.seat_pop_share > 0.0f) ++drawn_pop;
        }
        if (r.seat_processor)                       ++with_proc;
        if (r.seat_pop_share > 0.0f)                ++near_pop;
        if (r.seat_processor && r.seat_pop_share > 0.0f) ++both;
        if (!r.seat_processor && r.seat_pop_share <= 0.0f) ++neither;
        if (r.floor_unmet)                          ++unmet;
        if (r.seated == null_entity)                ++unseated;
        else if (!r.seated_is_specialist)           ++non_specialist;
        if (r.reproduce_checked) { ++reproduce_checked; if (!r.reproduced) ++not_reproduced; }
        if (r.shortlisted > 0 && (!have_weight || r.min_shortlist_weight < min_weight))
        {
            min_weight  = r.min_shortlist_weight;
            have_weight = true;
        }
    }

    auto pct = [](int a, int b) { return b > 0 ? 100.0 * a / b : 0.0; };

    // WHICH MECHANISM ACTUALLY DECIDED THE SEAT. Printed FIRST and unconditionally,
    // because every percentage below is meaningless without it: when the shortlist
    // is empty the WEIGHTED DRAW NEVER RUNS and the seat is the floor-unmet
    // fallback (first in rank order), which reads no weight at all. A sweep that
    // reported a "seat distribution" over seeds the draw never touched would be
    // attributing the fallback's behaviour to the bias.
    std::printf("\n=== which mechanism decided the seat ===\n");
    std::printf("  the WEIGHTED DRAW ran on ......... %3d/%-3d seeds  (%.1f%%)\n",
                done - unmet, done, pct(done - unmet, done));
    std::printf("  the FLOOR-UNMET FALLBACK ran on .. %3d/%-3d seeds  (%.1f%%)\n",
                unmet, done, pct(unmet, done));
    std::printf("  Only the first group measures the weights. Read the R3 rows below\n"
                "  against this split, never on their own.\n");

    std::printf("\n=== R3 — the seat distribution over %d seeds (REPORTED, not asserted) ===\n", done);
    std::printf("  seated corp HAS A PROCESSOR ............ %3d/%-3d  (%.1f%%)\n",
                with_proc, done, pct(with_proc, done));
    std::printf("  seated corp NEAR POPULATED GROUND ...... %3d/%-3d  (%.1f%%)\n",
                near_pop, done, pct(near_pop, done));
    std::printf("  seated corp BOTH ...................... %3d/%-3d  (%.1f%%)\n",
                both, done, pct(both, done));
    std::printf("  seated corp NEITHER ................... %3d/%-3d  (%.1f%%)\n",
                neither, done, pct(neither, done));
    std::printf("\n  what was ON OFFER, for the comparison the percentages above need:\n");
    std::printf("    shortlisted / specialists ........... %d/%d  (%.1f%%)\n",
                total_short, total_spec, pct(total_short, total_spec));
    std::printf("    of the shortlisted, with a processor . %d/%d  (%.1f%%)\n",
                total_short_proc, total_short, pct(total_short_proc, total_short));
    std::printf("    of the shortlisted, near population .. %d/%d  (%.1f%%)\n",
                total_short_pop, total_short, pct(total_short_pop, total_short));
    std::printf("    the RETIRED floor (solvent, trail8 >= 0) would pass %d/%d, and be EMPTY on "
                "%d/%d seeds\n",
                total_retired, total_spec, empty_retired, done);
    std::printf("    of the shortlisted, balance <= 0 ..... %d/%d  (%.1f%%)   [information — the\n"
                "    of the shortlisted, trail8 < 0 ....... %d/%d  (%.1f%%)    retired floor's inputs]\n",
                total_short_insolvent, total_short, pct(total_short_insolvent, total_short),
                total_short_trail_neg, total_short, pct(total_short_trail_neg, total_short));
    std::printf("\n  The bias is legible as the GAP between each seated row and its\n"
                "  on-offer row. Equal shares would mean the weights did nothing;\n"
                "  100%% would mean they had become a gate, which they must not be.\n");
    std::printf("\n  viability floor UNMET on %d/%d seeds%s\n", unmet, done,
                unmet ? "   (first-ranked specialist seated; the fact stands, nothing was patched)"
                      : "");
    std::printf("  OF THE SEEDS THE DRAW ACTUALLY RAN ON (%d): processor %d (%.1f%%), "
                "near population %d (%.1f%%)\n",
                drawn, drawn_proc, pct(drawn_proc, drawn), drawn_pop, pct(drawn_pop, drawn));
    std::printf("  historical comparison: the retired selection screen was built because a\n"
                "  UNIFORM draw handed the player a pure-extraction corp on 13 of 24 seeds\n"
                "  (54.2%%) — i.e. a processor %.1f%% of the time, against %.1f%% here.\n",
                100.0 - 54.2, pct(with_proc, done));

    if (!assert_mode)
        return threw ? 1 : 0;

    // --- the guard ----------------------------------------------------------
    auto row = [](const char* id, bool ok, const char* what) {
        std::printf("%s  %-3s  %s\n", ok ? "PASS" : "FAIL", id, what);
        return ok;
    };

    std::printf("\n=== guard — the properties the seat must hold ===\n");
    bool all = true;
    char buf[320];

    std::snprintf(buf, sizeof buf, "every seed seats somebody (%d unseated, %d threw)",
                  unseated, threw);
    all &= row("S1", threw == 0 && unseated == 0 && done == n_seeds, buf);

    std::snprintf(buf, sizeof buf,
                  "the seat is always a SPECIALIST, never a background firm (%d violations)",
                  non_specialist);
    all &= row("S2", non_specialist == 0, buf);

    std::snprintf(buf, sizeof buf,
                  "the weighting is a BIAS, never a gate: the smallest weight any "
                  "shortlisted corp carried is %.2f, and must be > 0 (a shortlisted "
                  "pure-extraction corp on thin ground scores exactly 1.00)",
                  static_cast<double>(min_weight));
    all &= row("S3", have_weight && min_weight > 0.0f, buf);

    std::snprintf(buf, sizeof buf,
                  "the draw is REPRODUCIBLE — same seed, two independently built worlds, "
                  "same seat (%d of the first %d seeds checked, %d disagreed)",
                  reproduce_checked, reproduce_seeds, not_reproduced);
    all &= row("S4", reproduce_checked > 0 && not_reproduced == 0, buf);

    std::snprintf(buf, sizeof buf,
                  "an unmet floor is RECORDED rather than hidden: %d/%d seeds unmet, and "
                  "every one of them still seated a specialist (this row REPORTS the "
                  "count — an unmet floor is a viability signal, not a failure)",
                  unmet, done);
    all &= row("S5", unmet == 0 || non_specialist == 0, buf);

    // S6 — BL-1020. The player picks a seat FROM this shortlist, so an empty one
    // is a broken opening rather than a tuning nit (Ben, 2026-09-16, NR-881). The
    // mechanism still patches nothing to make it non-empty; this row is what
    // notices when the ground stops carrying one.
    std::snprintf(buf, sizeof buf,
                  "the shortlist is NON-EMPTY on every seed — the floor reads phase 6's "
                  "static landscape score, not a trading record the settle cannot file "
                  "(%d/%d seeds empty; %d shortlisted of %d specialists)",
                  empty_shortlist, done, total_short, total_spec);
    all &= row("S6", done > 0 && empty_shortlist == 0, buf);

    std::printf("\n%s\n", all ? "ALL PASS" : "FAILURES ABOVE");
    return all ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    const bool roster_mode = (argc > 1 && std::string(argv[1]) == "--roster");
    const bool guard_mode  = (argc > 1 && std::string(argv[1]) == "--guard");
    const bool seat_mode   = (argc > 1 && std::string(argv[1]) == "--seat");
    const bool mode_arg = roster_mode || guard_mode || seat_mode;
    bool fast = false;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--fast")
            fast = true;
    const int n_seeds   = (!mode_arg && argc > 1) ? std::atoi(argv[1]) : 24;
    const int settle_ticks = (!mode_arg && argc > 2) ? std::atoi(argv[2]) : k_settle_ticks;
    if (!mode_arg && (n_seeds <= 0 || settle_ticks <= 0))
    {
        std::printf("usage: %s [seed_count] [settle_ticks] (both positive)\n"
                    "       %s --roster <seed>              (every corp's opening, one seed)\n"
                    "       %s --seat  [seed_count] [--fast] (REPORT the seat distribution)\n"
                    "       %s --guard [seed_count] [--fast] (assert what the seat holds)\n",
                    argv[0], argv[0], argv[0], argv[0]);
        return 2;
    }

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    // Vacuity guard (the standing lesson from interbody_pull_harness): a registry
    // that loaded nothing would make every seed look identically poor.
    if (reg.recipe_count(building_type::processing_facility) == 0)
    {
        std::printf("FATAL: no recipes loaded — run from the repo root.\n");
        return 2;
    }

    // BL-435 roster mode. Placed after the vacuity guard on purpose: a roster
    // printed from an empty registry would show every corp as equally poor.
    if (roster_mode)
        return run_roster(static_cast<uint32_t>(argc > 2 ? std::atoi(argv[2]) : 0), reg);

    // Same placement, same reason: a seat sweep against an empty registry would
    // measure a world where nothing can be processed, and every corp would fail
    // the viability floor for a reason that is not the world's.
    if (seat_mode || guard_mode)
    {
        // `--seeds a,b,c` names the worlds outright — the curated library
        // (`node tools/session/seed_library.js`) is sixteen chosen seeds, not
        // 0..15. Otherwise the positional count sweeps 0..n-1 as it always has.
        std::vector<uint32_t> seeds;
        int reproduce = k_reproduce_seeds;
        for (int a = 2; a + 1 < argc; ++a)
            if (std::string(argv[a]) == "--reproduce")
                reproduce = std::max(1, std::atoi(argv[a + 1]));
        for (int a = 2; a + 1 < argc; ++a)
            if (std::string(argv[a]) == "--seeds")
            {
                const std::string csv = argv[a + 1];
                std::size_t at = 0;
                while (at <= csv.size())
                {
                    std::size_t comma = csv.find(',', at);
                    if (comma == std::string::npos)
                        comma = csv.size();
                    if (comma > at)
                        seeds.push_back(static_cast<uint32_t>(
                            std::strtoul(csv.substr(at, comma - at).c_str(), nullptr, 10)));
                    at = comma + 1;
                }
            }
        if (seeds.empty())
        {
            int g_seeds = 24;
            if (argc > 2 && std::string(argv[2]).rfind("--", 0) != 0)
                g_seeds = std::atoi(argv[2]);
            if (g_seeds <= 0)
                g_seeds = 24;
            for (int i = 0; i < g_seeds; ++i)
                seeds.push_back(static_cast<uint32_t>(i));
        }
        // The same Lua state, as the app keeps one m_lua: build_app_start_world
        // reloads world_gen.lua, works.lua, recipes.lua and economy.lua into it
        // per start, in app order, and builds each start's own registry.
        return run_seat(seeds, lua, fast, guard_mode, reproduce);
    }

    std::printf("player_seed_sweep — %d seeds, %d settle ticks (%.2f in-game years)\n\n",
                n_seeds, settle_ticks, settle_ticks / 4.0);
    std::printf("seed  proc  extr  other   opening      final  dipped  verdict\n");
    std::printf("----  ----  ----  -----  --------  ---------  ------  -------\n");

    std::vector<seed_row> rows;
    rows.reserve(static_cast<std::size_t>(n_seeds));

    for (int i = 0; i < n_seeds; ++i)
    {
        seed_row r;
        r.seed = static_cast<uint32_t>(i);
        try
        {
            world_params p = no_prehistory();
            p.seed = r.seed;
            world w = make_hard_coded_world(p);
            seed_default_recipes(w, reg);
            // The landscape-search WINNER, not the seed candidate (BL-979).
            apply_shipped_landscape(w, reg, r.seed);

            const entity_id corp = w.player_entity;
            const auto      cit  = w.corporations.find(corp);
            if (cit == w.corporations.end())
            {
                r.threw = true;
            }
            else
            {
                for (const entity_id bid : cit->second.assets)
                {
                    const auto bit = w.buildings.find(bid);
                    if (bit == w.buildings.end())
                        continue;
                    switch (bit->second.type)
                    {
                        case building_type::processing_facility: ++r.processing; break;
                        case building_type::extraction_site:     ++r.extraction; break;
                        default:                                 ++r.other;      break;
                    }
                }
                r.opening = cit->second.balance;
                for (int t = 1; t <= settle_ticks; ++t)
                {
                    tick(w, reg, t);
                    if (w.corporations[corp].balance < 0.0f)
                        r.went_negative = true;
                }
                r.final_bal = w.corporations[corp].balance;
            }
        }
        catch (const std::exception& e)
        {
            r.threw = true;
            std::printf("%4u  THREW: %s\n", r.seed, e.what());
        }
        catch (...)
        {
            r.threw = true;
            std::printf("%4u  THREW: unknown\n", r.seed);
        }

        if (!r.threw)
            std::printf("%4u  %4d  %4d  %5d  %8.1f  %9.1f  %6s  %s\n",
                        r.seed, r.processing, r.extraction, r.other,
                        static_cast<double>(r.opening), static_cast<double>(r.final_bal),
                        r.went_negative ? "YES" : "no",
                        playable(r) ? "PLAYABLE" : "-");
        std::fflush(stdout);
        rows.push_back(r);
    }

    // --- Summary: why each rejected seed was rejected ------------------------
    int ok = 0, no_proc = 0, insolvent = 0, dipped = 0, threw = 0;
    std::vector<uint32_t> playable_seeds;
    for (const seed_row& r : rows)
    {
        if (r.threw)                { ++threw;      continue; }
        if (playable(r))            { ++ok; playable_seeds.push_back(r.seed); continue; }
        if (r.processing < 1)         ++no_proc;
        if (r.final_bal <= 0.0f)      ++insolvent;
        if (r.went_negative)          ++dipped;
    }

    std::printf("\n=== %d/%d playable ===\n", ok, n_seeds);
    std::printf("  rejected: %d no processing facility, %d ended insolvent, "
                "%d dipped negative, %d threw\n", no_proc, insolvent, dipped, threw);
    std::printf("  (a seed can be rejected for more than one reason, so these overlap)\n");

    std::printf("\nplayable seeds: ");
    if (playable_seeds.empty())
        std::printf("(none)\n");
    else
    {
        for (std::size_t i = 0; i < playable_seeds.size(); ++i)
            std::printf("%s%u", i ? ", " : "", playable_seeds[i]);
        std::printf("\n");
    }

    // Reports rather than gates: exit 0 unless generation actually threw, which is
    // seed_sweep_probe's failure condition and a real defect either way.
    return threw ? 1 : 0;
}
