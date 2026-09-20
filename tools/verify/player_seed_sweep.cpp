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
// WHICH MODES ARE THE SHIPPED START (BL-1030, cold review 2026-09-17). --seat,
// --guard, --digest and --digest-check build and settle the world in app order
// (harness_params.hpp build_app_start_world / run_app_validation_settle). The
// DEFAULT table and --roster still build make_hard_coded_world(p) bare, with
// fallback prices, no works registry, no set_era and their own settle: a
// different world under the same seed. Both print that on their face.
//
// Run: .\build\player_seed_sweep.exe [seed_count] [settle_ticks]
//      .\build\player_seed_sweep.exe --seat  [seed_count] [--fast]
//      .\build\player_seed_sweep.exe --guard [seed_count] [--fast]
//      .\build\player_seed_sweep.exe --guard --seeds 46,17,11 [--reproduce N] [--fast]
//      .\build\player_seed_sweep.exe --digest       [--seeds 46,17,11]   (BL-1031)
//      .\build\player_seed_sweep.exe --digest-check [--seeds 46,17,11]   (BL-1031)
//      ... --digest / --digest-check [--charter-budget none|empty|zero|synthetic|refused|stockpile]
//                                    [--charter-scale X]                    (BL-1032)
//                                    [--resource-cap on|off]                (BL-1039: the two
//                                     legacy cap rules on a synthetic budget, for their digests)
//      .\build\player_seed_sweep.exe --charter-cost [--seeds 0,28,46] [--budget-scales 1,2,4]
//                                    [--resource-cap on|off|both|sqrt|<list>]
//                                    [--density-ceilings 120,160]           (BL-1039, sqrt only)
//                                    [--province-cap on|off|both]
//                                    [--specialist-prices 4,8] [--ladder-scales 2|all]
//                                    [--no-extra] [--no-forced] [--forced-only]
//                                    [--forced-radius 4] [--forced-pick sparse|richest]
//                                    [--live-ticks 8]
//                                    [--out file.json] [--note TEXT]        (BL-1033)
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
#include "world/resource_names.hpp" // name_of — per-good tallies (BL-1039)
#include "world/settlement.hpp"   // nearest_region (the forced row's pick)
#include "world/nation_step.hpp"
#include "world/spawn_seat.hpp"
#include "world/supply_system.hpp"
#include "world/tech_gate.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include "world_digest.hpp"       // fnv1a64, hash_snapshot, world_state_digest (BL-1034)

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
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

    std::printf("player_seed_sweep --roster %u — every corporation's generated opening\n", seed);
    std::printf("NOT THE SHIPPED START: built bare (no world_gen.lua, works.lua or set_era, no landscape "
                "search); --seat/--guard/--digest build the app's world.\n\n");
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

// --- BL-1031: the world-bytes pin --------------------------------------------
//
// `player_seed_sweep --digest       [--seeds a,b,c]`  PRINTS four digests per seed.
// `player_seed_sweep --digest-check [--seeds a,b,c]`  RECOMPUTES them and FAILS
//                                                     (exit 1) on any row that
//                                                     differs from the pin below.
//
// WHY (Sprint 44). The charter-budget seam must leave a world with an EMPTY
// budget byte for byte where it was, proven by a check rather than a re-bless.
// Nothing here could prove that: world_determinism and determinism_harness never
// call the landscape search; landscape_search_harness R2/R3 and S4 above are
// same-binary A/A (two builds in ONE process agree with each other, which a
// change to the code moves in lockstep); and `state_hash` cannot see a
// building's tile, type or assets. A pin taken on the tree BEFORE the seam and
// compared on the tree after it is the check that sees both sides OF world/*.
//
// WHAT IT DOES NOT SEE (cold review, 2026-09-17). Every digest is computed
// through harness_params.hpp's hand-kept copy of app.cpp's new-game path, not
// through app.cpp. A change to app.cpp that the copy does not mirror (or a copy
// edit that cancels a world change) passes this check. So a change that touches
// app::start_new_game_prelude or poll_worldgen's validation run is reviewed
// together with the copy, diff against diff; the pin covers world/* only.
//
// FOUR DIGESTS, one per app phase, so a failing row names WHERE the world moved:
//   D_search  the walk — the winner candidate and every term of its score, the
//             evaluation and acceptance counts, and every path step's round,
//             axis, proposal, full score and taken flag; plus the seed
//             candidate, its score and the per-axis acceptance counts. NOT
//             `round_ms`: that is wall time, and a digest over it would fail on
//             a busy machine.
//   D_land    write_world_snapshot's bytes the moment the landscape is applied —
//             build_app_start_world returns with the winner laid and the second
//             recipe pass run (app.cpp:1092), before any validation tick.
//   D_settle  the snapshot bytes after the validation run, then `state_hash`
//             (at `current_day_tick`, the tick the app's verify API hashes at).
//   D_seat    the seat — the seated corp, the floor flag, the specialist and
//             shortlist counts, and every ranked candidate's every field (corp,
//             static landscape score, trailing net, weight, the shortlist flag
//             and the rest) — then the snapshot bytes after the seat.
//
// FNV-1a 64 over RAW bytes, floats and doubles included, so a digest moves on a
// last-bit change a printed `%.4f` would hide. Records are hashed FIELD BY FIELD,
// never as a block, so struct padding cannot enter a digest (and `scalar`
// refuses anything that is not a number or an enum). The hasher and the
// snapshot/settle recipe live in world_digest.hpp (BL-1034), shared with
// world_copy_determinism so both instruments hash a world the same way.
//
// A PIN IS A CONTRACT (src/world/CLAUDE.md). A failing row is a finding to report
// with its cause; it is never re-pinned by the change that moved it.

void hash_candidate(fnv1a64& f, const landscape_candidate& c)
{
    f.scalar(c.corporation_count);
    f.scalar(c.placement_seed);
    f.scalar(c.road_tier);
}

/// Every term of a landscape score, per-market readings first.
void hash_score(fnv1a64& f, const landscape_score& s)
{
    f.count(s.markets.size());
    for (const market_score& m : s.markets)
    {
        f.scalar(m.market);
        f.scalar(m.body);
        f.scalar(m.completeness);
        f.scalar(m.actual);
        f.scalar(m.actual_closed);
        f.scalar(m.extractors);
        f.scalar(m.processors);
        f.scalar(m.balanced);
        f.scalar(m.glutted);
        f.scalar(m.starved);
        f.scalar(m.rated);
        f.scalar(m.balance);
        f.scalar(m.reach);
    }
    f.scalar(s.mean_completeness);
    f.scalar(s.mean_actual);
    f.scalar(s.mean_balance);
    f.scalar(s.mean_reach);
    f.scalar(s.realisation);
    f.scalar(s.completeness_spread);
    f.scalar(s.balance_spread);
    f.scalar(s.reach_spread);
    f.scalar(s.spread);
    f.scalar(s.composite);
    f.scalar(s.market_count);
}

struct world_digests
{
    std::uint64_t search = 0;
    std::uint64_t land   = 0;
    std::uint64_t settle = 0;
    std::uint64_t seat   = 0;
    std::size_t   land_bytes   = 0;
    std::size_t   settle_bytes = 0;
    std::size_t   seat_bytes   = 0;
};

std::uint64_t digest_search(const landscape_search_result& r)
{
    fnv1a64 f;
    hash_candidate(f, r.winner);
    hash_score(f, r.winner_score);
    f.scalar(r.evaluations);
    f.scalar(r.accepted);
    f.count(r.path.size());
    for (const landscape_search_step& s : r.path)
    {
        f.scalar(s.round);
        f.scalar(s.axis);
        hash_candidate(f, s.proposal);
        hash_score(f, s.score);
        f.flag(s.accepted);
    }
    hash_candidate(f, r.seed_candidate);
    hash_score(f, r.seed_score);
    for (const int n : r.accepted_by_axis)
        f.scalar(n);
    return f.h;
}

/// The four BL-1031 digest seams, one function each, so every mode that takes a
/// digest hashes the same fields in the same order (BL-1033's cost mode takes
/// them on its `none` row to prove the world it measures is the shipped start).
void digest_at_land(const app_start_world& out, world_digests& dig)
{
    dig.search = digest_search(out.land.search);
    fnv1a64 f;
    dig.land_bytes = hash_snapshot(f, out.w);
    dig.land = f.h;
}

void digest_at_settle(const app_start_world& out, world_digests& dig)
{
    dig.settle = world_state_digest(out.w, &dig.settle_bytes);
}

void digest_at_seat(const app_start_world& out, const spawn_seat_result& res, world_digests& dig)
{
    fnv1a64 f;
    f.scalar(res.seated);
    f.flag(res.floor_unmet);
    f.scalar(res.specialist_count);
    f.scalar(res.shortlist_size);
    f.count(res.candidates.size());
    for (const spawn_seat_candidate& c : res.candidates)
    {
        f.scalar(c.corp);
        f.scalar(c.landscape);
        f.scalar(c.holdings_scored);
        f.flag(c.shortlisted);
        f.scalar(c.balance);
        f.flag(c.solvent);
        f.scalar(c.trailing_net);
        f.scalar(c.quarters_read);
        f.flag(c.has_processor);
        f.scalar(c.holdings);
        f.scalar(c.holdings_near_pop);
        f.scalar(c.population_share);
        f.scalar(c.weight);
    }
    dig.seat_bytes = hash_snapshot(f, out.w);
    dig.seat = f.h;
}

/// Build one world, settle it, seat it — the app's campaign start end to end.
/// Returns the seat result; @p out holds the world (and the registry, config
/// and search it was built from), because the caller needs both to describe
/// what was seated. With @p dig, the four BL-1031 digests are taken at the
/// seams between the phases; without it the path is exactly the seat sweep's.
spawn_seat_result build_and_seat(lua_state& lua, uint32_t seed, bool fast,
                                 app_start_world& out, world_digests* dig = nullptr,
                                 const harness_charter_input& charter = {},
                                 bool digitisation_span = false)
{
    world_params p = fast ? no_prehistory() : world_params{};
    p.seed = seed;
    // BL-1042: `--charter-budget stockpile` runs the Digitisation span, so the
    // shipped path's own stockpile budget has points in it. Off otherwise —
    // the shipped default, and the world every pin was taken on.
    p.digitisation_span_enabled = digitisation_span;
    // app::begin_new_game + app::start_new_game_prelude: config and works, the
    // world, setup_world's writes, load_economy with its era band, the
    // landscape-search WINNER (not the seed candidate — BL-979) and the second
    // recipe pass (BL-1030; harness_params.hpp). `charter` is none unless a
    // digest run asked for a budget (BL-1032); none is the app's own call.
    build_app_start_world(lua, p, out, charter);
    if (dig != nullptr)
        digest_at_land(out, *dig);
    // app::poll_worldgen's validation run (BL-1030; harness_params.hpp).
    run_app_validation_settle(out.w, out.reg);
    if (dig != nullptr)
        digest_at_settle(out, *dig);
    // app::seat_player (app.cpp:933): the seat on the WINNER'S STATIC SCORE
    // (BL-1020) — the one the app keeps as `m_landscape_winner_score`.
    spawn_seat_result res = seat_player_corporation(out.w, p.seed, out.land.search.winner_score);
    if (dig != nullptr)
        digest_at_seat(out, res, *dig);
    return res;
}

/// One pinned row.
struct world_digest_pin
{
    std::uint32_t seed;
    std::uint64_t search, land, settle, seat;
};

// THE PINS — the sixteen seed-library worlds (docs/generation/seed_library.json,
// `node tools/session/seed_library.js --seed-list`), in library order.
//
// PROVENANCE. Taken 2026-09-17 by `player_seed_sweep --digest --seeds
// 46,28,11,31,40,12,37,13,41,43,32,10,25,38,9,0` (2638 s), built on commit
// 0133ee2b (BL-1030, the app-order helpers) with this item's digest code on top
// and nothing else: src/ is byte-identical to 03c50ee4 (Sprint 44 cut), the
// tree BEFORE the charter-budget seam. TOOLCHAIN: MSVC cl 14.44.35207 (VS 2022
// BuildTools), Windows SDK 10.0.26100.0, Release /O2 /MD /DNDEBUG via
// `bash tools/verify/build_lua_harness.sh player_seed_sweep`. The pins bind to
// that toolchain; a different compiler or optimisation level is a different
// floating-point program and may legitimately differ.
//
// NEVER RE-PINNED by the change a pin exists to check (BL-1032, the charter
// budget seam). A row that fails is reported with its digest and its cause.
const std::vector<world_digest_pin> k_world_digest_pins = {
    //  seed  D_search               D_land                 D_settle               D_seat
    { 46u, 0xE0620F5777CB3637ull, 0x326DFD72ED01E15Dull, 0x05B4865F46884E7Cull, 0x496E75B156DC9208ull },
    { 28u, 0xA99FFD1314AFDD65ull, 0x7271F0D606D576C8ull, 0x265C48A23E313B1Aull, 0xAA35460CE5894594ull },
    { 11u, 0x1F277B425CC6D6F5ull, 0x6D66DCD90344A565ull, 0x2E8907B0BBE768E7ull, 0x82A858E16FE9CA69ull },
    { 31u, 0x4DCC349DEBD4278Dull, 0x568DBED7FCAB0473ull, 0x4AEB84A2E62A4536ull, 0xB58F31B1D2761E4Dull },
    { 40u, 0xACCB76968FC11F44ull, 0x350CE11A11C2EF16ull, 0x4987C80D094C8DAEull, 0x2B509E9C965DD8BFull },
    { 12u, 0x9171B81F5F1DB6CEull, 0x14FC25A425F1D81Eull, 0xFC9F8D4246024A2Full, 0x81E9BB11AB34278Bull },
    { 37u, 0xE55EBAB721B6A6E6ull, 0xF38B46012662363Cull, 0xF175EAB9B7F2BF41ull, 0x94B3B7C1A92369E5ull },
    { 13u, 0x8B45E33F6171F121ull, 0xBEBED327CE2A955Cull, 0x72797C2C57E94EB1ull, 0x0D38309D62D10FE3ull },
    { 41u, 0x19A91514D3C43BCAull, 0x8DFD164F25D5150Bull, 0xC0EDD8B3B38193C6ull, 0x9CB3AC1F2EC21D7Full },
    { 43u, 0xCB2F7D3D81A8A0C1ull, 0x53BB483612EFB3CDull, 0x557E96CF9F2BA372ull, 0x03D4B5D542CB228Bull },
    { 32u, 0x6D64F3AD914488BDull, 0x4EDBFF18370691B6ull, 0xAE3347D83E077849ull, 0xFD384DA6A173808Full },
    { 10u, 0xF8244965F92A0FF1ull, 0x809C8D803DF19C14ull, 0x8EA4043497A22AB7ull, 0x59D340FE15B12642ull },
    { 25u, 0x63A5BE80FB7DF06Aull, 0x84A597D8EBE7EDFFull, 0xFEFD82C8BCDD4D22ull, 0x2F145BF320CFB58Bull },
    { 38u, 0x4C17AE81C065C5F2ull, 0xAA0F18A56767FC70ull, 0x73239E8FE1A24AA3ull, 0x90D2AC55FC74A8D3ull },
    {  9u, 0x0E9AD780ACBB9B84ull, 0x7C85420229BFEE4Dull, 0x23DBD6FA7E7D5955ull, 0xA2B82933E77D219Aull },
    {  0u, 0x893B6977B1E9DC1Full, 0x1A24D230FDDF2C7Eull, 0xA392EFF987F374E2ull, 0x8BBEAB8453901456ull },
};

// --- BL-1032: the charter-budget seam, driven through the digest modes -------
//
// `--charter-budget none|empty|zero|synthetic` (default none) hands the search a
// budget; the digests are taken exactly as without one, so a row can be checked
// against the SAME pins:
//   none       no budget handed in — the shipped path (R1), which since BL-1042
//              builds the world's own stockpile budget; with the span off (every
//              pinned world) it is EMPTY, and a row whose stockpile holds a
//              point FAILS.
//   empty      an empty budget reaches the seam (R2).
//   zero       a budget of zero entries reaches the seam; the type drops them, so
//              it is the empty budget by construction (R2).
//   synthetic  SYNTHETIC TEST INPUT (harness_params.hpp): scale x the legacy
//              corporation count, measured on a legacy build of the same seed in
//              this process, over the non-razed centres by seeded weights (R4 —
//              the non-vacuity reading; its digests must NOT match the pins).
//   stockpile  BL-1042: the Digitisation span ON, and NO budget handed in, so
//              the SHIPPED PATH's own stockpile budget (`build_stockpile_budget`
//              at `stockpile_charter_spend`, harness_params.hpp's mirror of
//              app.cpp) reaches the search and the apply. Its digests are
//              EXPECTED to differ from the pins (the span moves the world); a
//              row FAILS when the budget is empty (R7's non-vacuity) or when any
//              point is unaccounted for — the region stock must equal the spend
//              plus the charter reasons plus the stockpile's own reasons.
//   refused    the SAME synthetic budget — non-empty, and one that moves the
//              world when priced (see `synthetic`) — with the firm price ZEROED,
//              so `charter_spend_refusal` refuses it. A refusal mutates nothing
//              beyond today's world, so its digests MUST equal the pins, and the
//              search and the apply must both report the refusal.
// empty and zero pass the DEFAULT spend, whose prices are 0: a non-empty budget
// with those prices would be refused, so a PASS also shows the refusal never
// reads an empty budget.

enum class charter_mode { none, empty, zero, synthetic, refused, stockpile };

const char* charter_mode_name(charter_mode m)
{
    switch (m)
    {
    case charter_mode::none:      return "none";
    case charter_mode::empty:     return "empty";
    case charter_mode::zero:      return "zero";
    case charter_mode::synthetic: return "synthetic";
    case charter_mode::refused:   return "refused";
    case charter_mode::stockpile: return "stockpile";
    }
    return "?";
}

/// The ids the `zero` budget is built over. Every one carries 0 points, so the
/// budget drops them all; the range is wide enough to cover every centre id a
/// generated world hands out.
constexpr entity_id k_zero_budget_ids = 65536;

/// Squared, column-wrapped grid distance between two tiles, or -1 when either
/// cannot be resolved — the report's independent re-measure of "in the window".
long long charter_tile_d2(const world& w, entity_id a, entity_id b)
{
    const auto ta = w.tiles.find(a);
    const auto tb = w.tiles.find(b);
    if (ta == w.tiles.end() || tb == w.tiles.end() || ta->second.body != tb->second.body)
        return -1;
    const auto body = w.bodies.find(ta->second.body);
    const int gw = (body != w.bodies.end()) ? body->second.grid_width : 0;
    long long dx = std::abs(ta->second.grid_x - tb->second.grid_x);
    if (gw > 0 && dx > gw / 2)
        dx = gw - dx;
    const long long dy = ta->second.grid_y - tb->second.grid_y;
    return dx * dx + dy * dy;
}

/// Every charter's holdings re-measured to its centre — anchors and secondary
/// holdings, inside and outside @p radius — from the tiles the record kept.
struct charter_spill
{
    int anchor_in = 0, anchor_out = 0, second_in = 0, second_out = 0, unresolved = 0;
    long long far_d2 = 0;
};

void measure_charter_spill(const world& w, const charter_spend_report& rep, int radius,
                           charter_spill& spec_sp, charter_spill& firm_sp)
{
    const long long r2 = static_cast<long long>(radius) * radius;
    for (const charter_record& r : rep.charters)
    {
        charter_spill& s = r.specialist ? spec_sp : firm_sp;
        const auto ct = w.population_centre_tile.find(r.centre);
        for (std::size_t h = 0; h < r.holdings.size(); ++h)
        {
            const long long d2 = (ct != w.population_centre_tile.end())
                ? charter_tile_d2(w, r.holdings[h], ct->second) : -1;
            if (d2 < 0)
            {
                ++s.unresolved;
                continue;
            }
            s.far_d2 = std::max(s.far_d2, d2);
            const bool inside = d2 <= r2;
            if (h == 0)
                (inside ? s.anchor_in : s.anchor_out)++;
            else
                (inside ? s.second_in : s.second_out)++;
        }
    }
}

/// BL-1039 — "good:n" for every good with a firm, in resource order; "-" for none.
std::string firms_by_good_text(const std::vector<std::int32_t>& by_good)
{
    std::string out;
    for (std::size_t r = 0; r < by_good.size(); ++r)
        if (by_good[r] > 0)
        {
            if (!out.empty())
                out += ' ';
            out += resource_names::name_of(static_cast<resource_type>(r));
            out += ':';
            out += std::to_string(by_good[r]);
        }
    return out.empty() ? std::string("-") : out;
}

/// BL-1039 — the TURN'S READING for one body (square-root rule): how many goods
/// with demand hold no firm, and the spread (most - fewest) of firms over the
/// goods in the turn — G without construction capacity. The turn fills a firm
/// per good each pass, so where the ceiling binds the spread is small and every
/// good that stayed short holds firms. A reading, not a check: a good whose gap
/// closes early legitimately stops short of the others.
struct charter_turn_reading
{
    int goods_in_turn = 0, goods_without_firm = 0, fewest = 0, most = 0;
};

charter_turn_reading read_turn(const charter_body_record& b)
{
    charter_turn_reading t;
    const std::size_t cap_good = static_cast<std::size_t>(resource_type::construction_capacity);
    bool first = true;
    for (const std::uint16_t g : b.goods)
    {
        if (g == cap_good)
            continue;
        const int n = g < b.firms_by_good.size() ? static_cast<int>(b.firms_by_good[g]) : 0;
        ++t.goods_in_turn;
        if (n == 0)
            ++t.goods_without_firm;
        t.fewest = first ? n : std::min(t.fewest, n);
        t.most   = first ? n : std::max(t.most, n);
        first = false;
    }
    return t;
}

/// BL-1039 — one line per body: the density rule the spend fixed before its walk
/// (B, G, B_ref, the per-good cap, the ceiling) and the firms it chartered, per good.
void print_charter_bodies(const charter_spend_report& rep, const char* indent)
{
    for (const charter_body_record& b : rep.bodies)
    {
        std::string goods;
        for (const std::uint16_t g : b.goods)
        {
            if (!goods.empty())
                goods += ',';
            goods += resource_names::name_of(static_cast<resource_type>(g));
        }
        std::printf("%sbody %u: B %lld pts on firms; G %d goods with demand [%s]; B_ref %lld pts; per-good cap %d%s; "
                    "density ceiling %d%s; firms %d — by good: %s\n",
                    indent, b.body, static_cast<long long>(b.firm_points), b.goods_with_demand,
                    goods.c_str(), static_cast<long long>(b.reference_points),
                    static_cast<int>(b.per_good_cap), b.per_good_cap < 0 ? " (none: lifted)" : "",
                    static_cast<int>(b.density_ceiling), b.density_ceiling == 0 ? " (none)" : "",
                    static_cast<int>(b.firms), firms_by_good_text(b.firms_by_good).c_str());
        if (rep.cap_rule == charter_cap_rule::sqrt_capital)
        {
            const charter_turn_reading t = read_turn(b);
            char share[128];
            if (b.even_share > 0)
                std::snprintf(share, sizeof share, "%d each, %d goods +1, reserved of the ceiling less "
                              "%d yard places (NR-905, NR-906)", static_cast<int>(b.even_share),
                              static_cast<int>(b.even_share_extra), static_cast<int>(b.yard_places));
            else
                std::snprintf(share, sizeof share, "none (the ceiling does not bind; yard places %d)",
                              static_cast<int>(b.yard_places));
            std::printf("%s  the turn (G without construction capacity): %d goods, %d without a firm; "
                        "firms per good %d..%d; even share %s\n", indent, t.goods_in_turn,
                        t.goods_without_firm, t.fewest, t.most, share);
        }
    }
}

/// BL-1039 fix round — THE BALANCE, as a failing check on every budget row
/// (refused rows included). Empty when balanced; otherwise every clause that
/// broke. Re-derived from the INPUT and the records, not from the report's totals
/// alone:
///  * points budgeted = the budget's own total;
///  * points spent = the prices on the charter records;
///  * points unspent = the unspent rows summed BY REASON;
///  * budgeted = spent + unspent;
///  * PER CENTRE, the budget's points minus its charters' prices = its unspent rows.
std::string charter_balance_failure(const charter_budget& budget, const charter_spend_report& rep)
{
    std::string out;
    char buf[200];
    const auto add = [&](const char* s) { if (out.size() < 1200) { out += ' '; out += s; out += ';'; } };

    long long by_records = 0;
    std::map<entity_id, long long> left;
    for (const auto& [centre, pts] : budget.points())
        left[centre] = pts;
    for (const charter_record& r : rep.charters)
    {
        by_records += r.price;
        left[r.centre] -= r.price;
    }
    long long by_reason = 0;
    std::map<entity_id, long long> rows;
    for (const charter_unspent& u : rep.unspent)
    {
        by_reason += u.points;
        rows[u.centre] += u.points;
    }
    if (rep.points_budgeted != budget.total())
    {
        std::snprintf(buf, sizeof buf, "budgeted %lld, the budget totals %lld",
                      static_cast<long long>(rep.points_budgeted), static_cast<long long>(budget.total()));
        add(buf);
    }
    if (rep.points_spent != by_records)
    {
        std::snprintf(buf, sizeof buf, "spent %lld, the charters' prices sum to %lld",
                      static_cast<long long>(rep.points_spent), by_records);
        add(buf);
    }
    if (rep.points_unspent != by_reason)
    {
        std::snprintf(buf, sizeof buf, "unspent %lld, the reasons sum to %lld",
                      static_cast<long long>(rep.points_unspent), by_reason);
        add(buf);
    }
    if (budget.total() != by_records + by_reason)
    {
        std::snprintf(buf, sizeof buf, "the budget's %lld != %lld spent + %lld unspent by reason",
                      static_cast<long long>(budget.total()), by_records, by_reason);
        add(buf);
    }
    for (const auto& [centre, l] : left)
    {
        const long long r = rows.count(centre) ? rows[centre] : 0;
        if (l != r)
        {
            std::snprintf(buf, sizeof buf, "centre %u: budget minus its charters' prices %lld, its "
                          "unspent rows %lld", centre, l, r);
            add(buf);
        }
    }
    for (const auto& [centre, r] : rows)
        if (left.count(centre) == 0)
        {
            std::snprintf(buf, sizeof buf, "centre %u: %lld unspent points on a centre the budget "
                          "never named", centre, r);
            add(buf);
        }
    return out;
}

/// BL-1039 — the spend's rules, RE-DERIVED from the budget, the spend's prices,
/// the report's own records and the world, never read back from the numbers
/// under test alone:
///  * B per body: each nation-resolved centre's points net of the specialist
///    price where it affords one, in whole firm charters (Ben, 2026-09-18: B
///    and B_ref in the same units, points on FIRMS); B_ref = c x |G| x firm
///    price; the per-good cap by its DEFINITION as an integer inequality
///    (k^2 B_ref <= c^2 B < (k+1)^2 B_ref, or c when that k <= c), not by
///    calling the function that set it — and, on every square-root body with
///    demand, that the rule gives exactly c at B = B_ref (the ruling's anchor);
///  * firms per good re-tallied from the firm records (each record's good and
///    its anchor tile's body) against the body rows; no good past its cap, no
///    body past its ceiling or its guard.
/// The points balance is `charter_balance_failure`'s, a failing check on its own.
struct charter_rule_check
{
    bool        pass = true;
    std::string fail;
};

charter_rule_check check_charter_rules(const world& w, const charter_budget& budget,
                                       const charter_spend_report& rep,
                                       const charter_spend_params& spend)
{
    charter_rule_check out;
    const auto failed = [&](const std::string& why) {
        out.pass = false;
        if (out.fail.size() < 1200)
            out.fail += " " + why + ";";
    };
    char buf[256];

    // --- B, re-derived: points on FIRMS, net of the specialist price ---
    const long long fp = spend.firm_price_points;
    const long long sp = static_cast<long long>(spend.firm_price_points)
                       * static_cast<long long>(spend.specialist_firm_charters);
    std::map<entity_id, long long> firm_points_by_body;
    for (const auto& [centre, pts] : budget.points())
    {
        const auto ct = w.population_centre_tile.find(centre);
        if (ct == w.population_centre_tile.end())
            continue;
        const auto t = w.tiles.find(ct->second);
        const auto own = w.tile_to_nation.find(ct->second);
        if (t == w.tiles.end() || own == w.tile_to_nation.end() || w.nations.count(own->second) == 0)
            continue;
        long long left = pts;
        if (left >= sp)
            left -= sp;
        firm_points_by_body[t->second.body] += fp > 0 ? (left / fp) * fp : 0;
    }
    if (firm_points_by_body.size() != rep.bodies.size())
    {
        std::snprintf(buf, sizeof buf, "%zu bodies hold nation-resolved budget, report has %zu",
                      firm_points_by_body.size(), rep.bodies.size());
        failed(buf);
    }

    // --- firms per good, re-tallied from the records ---
    std::map<entity_id, std::vector<std::int32_t>> tally;
    for (const charter_record& r : rep.charters)
    {
        if (r.specialist)
            continue;
        const auto t = w.tiles.find(r.anchor_tile);
        if (t == w.tiles.end() || r.good >= resource_count)
        {
            failed("a firm record with no anchor body or no good");
            continue;
        }
        auto& v = tally[t->second.body];
        v.resize(resource_count, 0);
        ++v[r.good];
    }

    const long long c = spend.per_resource_firm_cap;
    for (const charter_body_record& b : rep.bodies)
    {
        const auto cb = firm_points_by_body.find(b.body);
        if (cb == firm_points_by_body.end() || cb->second != b.firm_points)
        {
            std::snprintf(buf, sizeof buf, "body %u: B %lld, re-derived %lld", b.body,
                          static_cast<long long>(b.firm_points),
                          cb == firm_points_by_body.end() ? -1LL : cb->second);
            failed(buf);
        }
        const long long g = b.goods_with_demand;
        if (g != static_cast<long long>(b.goods.size()))
            failed("goods_with_demand disagrees with the goods list");
        const long long bref = g > 0 ? c * g * fp : 0;
        if (bref != b.reference_points)
        {
            std::snprintf(buf, sizeof buf, "body %u: B_ref %lld, c x |G| x fp = %lld", b.body,
                          static_cast<long long>(b.reference_points), bref);
            failed(buf);
        }
        // The per-good cap, by its definition.
        const long long k = b.per_good_cap;
        switch (spend.resource_cap_rule)
        {
        case charter_cap_rule::fixed:
            if (k != c) failed("fixed rule: per-good cap is not per_resource_firm_cap");
            break;
        case charter_cap_rule::lifted:
            if (k != -1) failed("lifted rule: a per-good cap is set");
            break;
        case charter_cap_rule::sqrt_capital:
        {
            const long long B = b.firm_points;
            // k^2 B_ref <= c^2 B < (k+1)^2 B_ref, divided through by c |G| fp:
            // k^2 <= y < (k+1)^2 with y = floor(c B / (|G| fp)) — the same
            // inequality, exact for integers, but c^2 B itself can pass int64 on
            // a spend the refusal accepts (BL-1060). c x B cannot (the refusal
            // bounds it by the budget's total), nor can (k+1)^2 with k an int32.
            bool ok = true;
            if (g == 0 || B <= 0)
                ok = (k == c);
            else if (c > 0 && B > std::numeric_limits<long long>::max() / c)
                ok = false;   // c x B past int64: charter_spend_refusal should have refused it
            else
            {
                const long long y = (c * B) / (g * fp);
                if (k > c)
                    ok = k * k <= y && y < (k + 1) * (k + 1);
                else
                    ok = (k == c) && y < (c + 1) * (c + 1);
            }
            if (!ok)
            {
                std::snprintf(buf, sizeof buf, "body %u: per-good cap %lld is not max(c, floor(c "
                              "sqrt(B/B_ref))) for c %lld, B %lld, B_ref %lld", b.body, k, c, B, bref);
                failed(buf);
            }
            // THE ANCHOR (Ben, 2026-09-18): at the legacy firm spend the rule
            // gives the legacy cap, exactly, on this body's own G.
            if (g > 0 && charter_sqrt_per_good_cap(static_cast<std::int32_t>(c), bref,
                                                   static_cast<int>(g),
                                                   static_cast<std::int32_t>(fp)) != c)
            {
                std::snprintf(buf, sizeof buf, "body %u: cap(B_ref = %lld) is not c = %lld", b.body,
                              bref, c);
                failed(buf);
            }
            if (b.density_ceiling != spend.density_ceiling)
                failed("sqrt rule: the body's ceiling is not the spend's");
            break;
        }
        }
        if (spend.resource_cap_rule != charter_cap_rule::sqrt_capital && b.density_ceiling != 0)
            failed("a legacy rule carries a density ceiling");

        std::vector<std::int32_t> mine = tally.count(b.body) ? tally[b.body]
                                                             : std::vector<std::int32_t>(resource_count, 0);
        long long sum = 0;
        for (std::size_t r = 0; r < b.firms_by_good.size(); ++r)
        {
            sum += b.firms_by_good[r];
            if (r >= mine.size() || mine[r] != b.firms_by_good[r])
            {
                std::snprintf(buf, sizeof buf, "body %u: %s firms %d, records say %d", b.body,
                              resource_names::name_of(static_cast<resource_type>(r)).c_str(),
                              static_cast<int>(b.firms_by_good[r]),
                              r < mine.size() ? static_cast<int>(mine[r]) : -1);
                failed(buf);
            }
            if (k >= 0 && b.firms_by_good[r] > k)
            {
                std::snprintf(buf, sizeof buf, "body %u: %s holds %d firms, past its cap %lld", b.body,
                              resource_names::name_of(static_cast<resource_type>(r)).c_str(),
                              static_cast<int>(b.firms_by_good[r]), k);
                failed(buf);
            }
        }
        // THE EVEN SHARE (NR-905, NR-906), re-derived from the body's own row:
        // the ceiling binds when the body holds more firm charters than the
        // ceiling AND the turn's per-good caps sum past it; then (ceiling - the
        // yards' places) / |turn| each, the remainder one more to the first goods
        // of the turn. A share is a RESERVATION, not a cap, so a good may end
        // past it — only the per-good cap (above) bounds a good.
        if (spend.resource_cap_rule == charter_cap_rule::sqrt_capital)
        {
            const std::size_t cap_good = static_cast<std::size_t>(resource_type::construction_capacity);
            long long n_turn = 0;
            for (const std::uint16_t gd : b.goods)
                if (gd != cap_good)
                    ++n_turn;
            const long long ceil_n = b.density_ceiling;
            const long long room   = ceil_n - b.yard_places;
            if (b.yard_places < 0 || b.yard_places > ceil_n)
                failed("the yards' places are outside 0..ceiling");
            // BL-1060 round 4 (the cold review's finding 4): the one reading of
            // the yards' places that does NOT come from the code under test —
            // the walk must not provision more yards than the places the shares
            // were cut from, or the ceiling is over-subscribed and a reserved
            // place was eaten. The report already carries both numbers.
            const long long yards_built = b.firms_by_good[cap_good];
            if (yards_built > b.yard_places)
            {
                std::snprintf(buf, sizeof buf,
                              "body %u: %lld construction yards chartered but only %d places were "
                              "reserved for them, so a good's share was eaten", b.body, yards_built,
                              static_cast<int>(b.yard_places));
                failed(buf);
            }
            const bool binds = n_turn > 0 && room >= n_turn && fp > 0 && b.firm_points / fp > ceil_n
                            && n_turn * k > ceil_n;
            const long long want_share = binds ? room / n_turn : 0;
            const long long want_extra = binds ? room % n_turn : 0;
            if (b.even_share != want_share || b.even_share_extra != want_extra)
            {
                std::snprintf(buf, sizeof buf, "body %u: even share %d (+1 on %d), re-derived %lld "
                              "(+1 on %lld) from ceiling %lld less %d yard places", b.body,
                              static_cast<int>(b.even_share), static_cast<int>(b.even_share_extra),
                              want_share, want_extra, ceil_n, static_cast<int>(b.yard_places));
                failed(buf);
            }
        }
        else if (b.even_share != 0 || b.even_share_extra != 0 || b.yard_places != 0)
            failed("a legacy rule carries an even share or yard places");
        if (sum != b.firms)
            failed("firms by good do not sum to the body's firms");
        if (b.density_ceiling > 0 && b.firms > b.density_ceiling)
            failed("a body past its density ceiling");
        if (b.firms > spend.max_firms_per_body)
            failed("a body past its runaway guard");
    }
    return out;
}

void print_charter_report(const world& w, charter_mode mode, const charter_budget& budget,
                          const charter_spend_params& spend, const charter_spend_report& rep,
                          const landscape_search_result& search,
                          std::size_t legacy_specialists, std::size_t legacy_firms, double scale)
{
    if (budget.empty())
    {
        std::printf("      charter budget %s: EMPTY after construction (entries <= 0 dropped) -> "
                    "the legacy branch; prices unset (firm %d, specialist %d firm charters); "
                    "no report; search refused flag %s\n",
                    charter_mode_name(mode), spend.firm_price_points, spend.specialist_firm_charters,
                    search.charter_refused ? "SET (unexpected)" : "clear");
        return;
    }
    if (mode == charter_mode::stockpile)
        std::printf("      charter budget stockpile — the Digitisation stockpile (BL-1042): %lld "
                    "points over %zu carved centres; firm price %d, specialist %d firm charters "
                    "(= %lld points), window radius %d, province cap %s, cap rule %s (per-good cap "
                    "%d, density ceiling %d, guard %d) — PROVISIONAL prices (BL-1044)\n",
                    static_cast<long long>(budget.total()), budget.points().size(),
                    spend.firm_price_points, spend.specialist_firm_charters,
                    static_cast<long long>(spend.specialist_price_points()), spend.window_radius,
                    spend.province_cap ? "on" : "off",
                    charter_cap_rule_name(spend.resource_cap_rule),
                    static_cast<int>(spend.per_resource_firm_cap),
                    static_cast<int>(spend.density_ceiling),
                    static_cast<int>(spend.max_firms_per_body));
    else  // the synthetic header, unchanged
    std::printf("      charter budget %s — SYNTHETIC TEST INPUT (seeded weights, never population): "
                "scale %.2f x 1x %zu legacy corporations (%zu specialists + %zu firms) = %lld points "
                "over %zu centres; firm price %d, specialist %d firm charters (= %lld points), "
                "window radius %d, province cap %s, resource cap %s (per-good cap %d, density "
                "ceiling %d, guard %d); specialist capital: today's draw\n",
                charter_mode_name(mode), scale, legacy_specialists + legacy_firms, legacy_specialists,
                legacy_firms, static_cast<long long>(budget.total()), budget.points().size(),
                spend.firm_price_points, spend.specialist_firm_charters,
                static_cast<long long>(spend.specialist_price_points()), spend.window_radius,
                spend.province_cap ? "on" : "off",
                spend.resource_cap_rule == charter_cap_rule::fixed    ? "on (fixed)"
                : spend.resource_cap_rule == charter_cap_rule::lifted ? "off (lifted)"
                                                                      : "sqrt_capital",
                static_cast<int>(spend.per_resource_firm_cap), static_cast<int>(spend.density_ceiling),
                static_cast<int>(spend.max_firms_per_body));
    {
        std::int32_t richest = 0;
        int affords = 0;
        const bool priced = charter_spend_refusal(budget, spend) == nullptr;
        for (const auto& kv : budget.points())
        {
            richest = std::max(richest, kv.second);
            if (priced && kv.second >= spend.specialist_price_points())
                ++affords;
        }
        int non_razed = 0;
        for (const auto& kv : w.population_centres)
            if (!kv.second.razed)
                ++non_razed;
        if (priced)
            std::printf("      budget shape: %d non-razed centres in the world, %zu hold points; "
                        "richest centre %d points; %d centres afford a specialist\n",
                        non_razed, budget.points().size(), richest, affords);
        else
            std::printf("      budget shape: %d non-razed centres in the world, %zu hold points; "
                        "richest centre %d points; prices refused, so nothing is affordable\n",
                        non_razed, budget.points().size(), richest);
    }
    // BOTH halves of a refusal, read separately: the search's flag and the
    // apply's report. Either missing is a finding.
    std::printf("      search: %s%s%s (%d evaluations, %zu path steps)\n",
                search.charter_refused ? "REFUSED — " : "budget world, roster axis skipped",
                search.charter_refused ? search.charter_refusal.c_str() : "",
                search.charter_refused ? "; the no-budget search ran" : "",
                search.evaluations, search.path.size());
    if (rep.refused)
        std::printf("      apply: REFUSED — %s; the legacy calls ran, nothing chartered\n",
                    rep.refusal.c_str());
    else
        std::printf("      apply: spent (not refused)\n");

    const long long r2 = static_cast<long long>(spend.window_radius) * spend.window_radius;
    int spec_cw = 0, spec_rw = 0, spec_in = 0, firm_cw = 0, firm_rw = 0, firm_in = 0;
    std::vector<entity_id> centres_with_both;
    std::map<entity_id, std::pair<int, int>> in_window_by_centre;   // centre -> (specialists, firms)
    for (const charter_record& r : rep.charters)
    {
        const auto ct = w.population_centre_tile.find(r.centre);
        const long long d2 = (ct != w.population_centre_tile.end())
            ? charter_tile_d2(w, r.anchor_tile, ct->second) : -1;
        const bool inside = d2 >= 0 && d2 <= r2;
        if (r.specialist)
        {
            (r.rung == charter_rung::centre_window ? spec_cw : spec_rw)++;
            if (inside) { ++spec_in; ++in_window_by_centre[r.centre].first; }
        }
        else
        {
            (r.rung == charter_rung::centre_window ? firm_cw : firm_rw)++;
            if (inside) { ++firm_in; ++in_window_by_centre[r.centre].second; }
        }
    }
    int both = 0;
    for (const auto& [centre, n] : in_window_by_centre)
        if (n.first > 0 && n.second > 0)
            ++both;

    std::printf("      specialists %zu (rung centre_window %d, region_window %d; anchor re-measured "
                "inside radius %d of its centre: %d); player %u%s\n",
                rep.specialists.size(), spec_cw, spec_rw, spend.window_radius, spec_in,
                rep.player, rep.no_specialists ? " — no_specialists, nobody seated by the spend" : "");
    std::printf("      background firms %zu (rung centre_window %d, region_window %d; anchor "
                "re-measured inside radius: %d); centres holding a specialist AND a firm inside "
                "their window: %d\n",
                rep.firms.size(), firm_cw, firm_rw, firm_in, both);

    // HOLDING SPILL. The rungs bound the ANCHOR only; `place_starting_assets`
    // walks the secondary holdings outward from it across the whole nation. So
    // EVERY holding — anchor and secondary — is re-measured here to its centre on
    // the wrapped metric, from the tiles the record kept as placed (the world
    // printed here has settled, and a firm may since have exited).
    using spill = charter_spill;
    spill spec_sp, firm_sp;
    measure_charter_spill(w, rep, spend.window_radius, spec_sp, firm_sp);
    const auto print_spill = [&](const char* role, const spill& s) {
        std::printf("      %s holdings %d re-measured to their centre (radius %d): anchors %d inside / "
                    "%d outside; secondary %d inside / %d outside; unresolved %d; farthest %.2f tiles\n",
                    role, s.anchor_in + s.anchor_out + s.second_in + s.second_out + s.unresolved,
                    spend.window_radius, s.anchor_in, s.anchor_out, s.second_in, s.second_out,
                    s.unresolved, std::sqrt(static_cast<double>(s.far_d2)));
    };
    print_spill("specialist", spec_sp);
    print_spill("firm", firm_sp);
    std::printf("      ALL holdings: %d inside radius %d, %d outside (%d unresolved)\n",
                spec_sp.anchor_in + spec_sp.second_in + firm_sp.anchor_in + firm_sp.second_in,
                spend.window_radius,
                spec_sp.anchor_out + spec_sp.second_out + firm_sp.anchor_out + firm_sp.second_out,
                spec_sp.unresolved + firm_sp.unresolved);

    std::array<long long, charter_unspent_reason_count> pts{};
    std::array<int, charter_unspent_reason_count> ctr{};
    for (const charter_unspent& u : rep.unspent)
    {
        pts[static_cast<std::size_t>(u.reason)] += u.points;
        ++ctr[static_cast<std::size_t>(u.reason)];
    }
    const std::string unbalanced = charter_balance_failure(budget, rep);
    std::printf("      points: budgeted %lld = spent %lld + unspent %lld [%s%s]; unspent by reason:",
                static_cast<long long>(rep.points_budgeted), static_cast<long long>(rep.points_spent),
                static_cast<long long>(rep.points_unspent),
                unbalanced.empty() ? "balanced" : "UNBALANCED:", unbalanced.c_str());
    for (int r = 0; r < charter_unspent_reason_count; ++r)
        std::printf(" %s %lld (%d centres)%s",
                    charter_unspent_reason_name(static_cast<charter_unspent_reason>(r)),
                    pts[static_cast<std::size_t>(r)], ctr[static_cast<std::size_t>(r)],
                    r + 1 < charter_unspent_reason_count ? "," : "\n");
    if (!rep.refused)
        print_charter_bodies(rep, "      ");
}

/// BL-1042 — THE STOCKPILE'S ACCOUNT, as a failing check. Empty when every
/// point of the region stock is accounted for; otherwise each clause that broke.
/// Re-derived from the rows, never from the totals alone:
///  * the stockpile's own account closes (`stockpile_budget::balanced`);
///  * each region row closes (its points = its centres' shares + its reasons);
///  * the budget's points = what the charter report budgeted (when non-empty);
///  * the region stock = points spent + the charter's unspent reasons + the
///    stockpile's own reasons — "every point accounted for by spend or reason".
std::string stockpile_account_failure(const stockpile_budget& sb, const charter_spend_report& rep)
{
    std::string out;
    char buf[200];
    if (!sb.balanced())
    {
        std::snprintf(buf, sizeof buf, " stockpile %lld != %lld to centres + %lld unspent (budget %lld);",
                      static_cast<long long>(sb.points_total),
                      static_cast<long long>(sb.points_to_centres),
                      static_cast<long long>(sb.points_unspent()),
                      static_cast<long long>(sb.budget.total()));
        out += buf;
    }
    long long rows = 0;
    for (const stockpile_region_row& r : sb.regions)
    {
        long long u = 0;
        for (const std::int64_t v : r.unspent)
            u += v;
        rows += r.points;
        if (r.points != r.to_centres + u && out.size() < 1200)
        {
            std::snprintf(buf, sizeof buf, " region %d: %lld points, %lld to centres + %lld unspent;",
                          r.region, static_cast<long long>(r.points),
                          static_cast<long long>(r.to_centres), u);
            out += buf;
        }
    }
    if (!sb.rejected && rows != sb.points_total)
    {
        std::snprintf(buf, sizeof buf, " the region rows sum to %lld, the stockpile totals %lld;",
                      rows, static_cast<long long>(sb.points_total));
        out += buf;
    }
    if (!sb.budget.empty())
    {
        long long ch_unspent = 0;
        for (const charter_unspent& u : rep.unspent)
            ch_unspent += u.points;
        if (rep.points_budgeted != sb.budget.total())
        {
            std::snprintf(buf, sizeof buf, " the charter report budgeted %lld, the stockpile budget is %lld;",
                          static_cast<long long>(rep.points_budgeted),
                          static_cast<long long>(sb.budget.total()));
            out += buf;
        }
        if (sb.points_total != rep.points_spent + ch_unspent + sb.points_unspent())
        {
            std::snprintf(buf, sizeof buf, " stock %lld != spent %lld + charter unspent %lld + "
                          "stockpile unspent %lld;",
                          static_cast<long long>(sb.points_total),
                          static_cast<long long>(rep.points_spent), ch_unspent,
                          static_cast<long long>(sb.points_unspent()));
            out += buf;
        }
    }
    return out;
}

/// One block per row: the stock, where it went, and the whole account.
void print_stockpile_account(const world& w, const stockpile_budget& sb,
                             const charter_spend_report& rep)
{
    std::printf("      stockpile (BL-1042): %lld points on %zu regions -> %lld to %zu centres",
                static_cast<long long>(sb.points_total), sb.regions.size(),
                static_cast<long long>(sb.points_to_centres), sb.budget.points().size());
    for (int r = 0; r < stockpile_unspent_reason_count; ++r)
        std::printf(", %s %lld",
                    stockpile_unspent_reason_name(static_cast<stockpile_unspent_reason>(r)),
                    static_cast<long long>(sb.unspent[static_cast<std::size_t>(r)]));
    if (sb.rejected)
        std::printf("; REJECTED: %s", sb.rejection.c_str());
    std::printf("\n");
    if (sb.points_total == 0)
        return;
    int founded = 0, dropped = 0;
    for (const stockpile_region_row& r : sb.regions)
    {
        founded += r.founded;
        dropped += r.dropped;
    }
    std::int32_t richest = 0;
    for (const auto& kv : sb.budget.points())
        richest = std::max(richest, kv.second);
    long long ch_unspent = 0;
    for (const charter_unspent& u : rep.unspent)
        ch_unspent += u.points;
    std::printf("      stockpile slots: %d founded, %d dropped over the point-holding regions; "
                "richest centre %d points\n", founded, dropped, static_cast<int>(richest));
    // WHY a region holding points carved no centre (NR-901): per region, its
    // points, the part the treasury paid in (`industry_points_from_treasury`),
    // the towns history razed there (`centres_razed`) and what stands at the
    // epoch — so the cause is on the page, not inferred. `razed` rows lost
    // their towns to war; a `no_carved_centre` (residual) row is a finding.
    if (const settlement_state* ss = w.gen_settlement.get())
    {
        constexpr int k_max_rows = 40;
        int shown = 0, n = 0;
        for (const stockpile_region_row& r : sb.regions)
        {
            const std::int64_t razed =
                r.unspent[static_cast<std::size_t>(stockpile_unspent_reason::razed)];
            const std::int64_t resid =
                r.unspent[static_cast<std::size_t>(stockpile_unspent_reason::no_carved_centre)];
            if (razed == 0 && resid == 0)
                continue;
            ++n;
            if (shown >= k_max_rows)
                continue;
            ++shown;
            const region& rg = ss->regions[static_cast<std::size_t>(r.region)];
            std::printf("        region %5d %-16s points %10lld (from treasury %10lld) centres_razed %3d | "
                        "at the epoch: centres %d, urban %lld, population %lld\n",
                        r.region, razed != 0 ? "razed" : "NO_CARVED_CENTRE",
                        static_cast<long long>(r.points),
                        static_cast<long long>(rg.industry_points_from_treasury),
                        rg.centres_razed, rg.centres, static_cast<long long>(rg.urban_population),
                        static_cast<long long>(rg.population));
        }
        if (n > shown)
            std::printf("        ... %d more such regions not printed\n", n - shown);
    }
    std::printf("      ACCOUNT: stock %lld = spent %lld + charter unspent %lld + stockpile unspent %lld "
                "(%s)\n",
                static_cast<long long>(sb.points_total), static_cast<long long>(rep.points_spent),
                ch_unspent, static_cast<long long>(sb.points_unspent()),
                sb.points_total == rep.points_spent + ch_unspent + sb.points_unspent()
                    ? "closes" : "DOES NOT CLOSE");
}

int run_digest(const std::vector<uint32_t>& seeds, lua_state& lua, bool check,
               charter_mode mode = charter_mode::none, double charter_scale = 1.0,
               bool resource_cap = true)
{
    std::printf("player_seed_sweep %s — %zu seeds, the shipped spawn built, settled (%d ticks) "
                "and seated in app order (BL-1030)\n",
                check ? "--digest-check" : "--digest", seeds.size(), k_settle_ticks);
    std::printf("BL-1032. --charter-budget %s%s\n", charter_mode_name(mode),
                mode == charter_mode::none ? " (the shipped path: the world's own stockpile "
                                             "budget, empty with the span off)"
                : mode == charter_mode::synthetic
                    ? " — SYNTHETIC TEST INPUT; its digests are EXPECTED to differ from the pins"
                : mode == charter_mode::refused
                    ? " — the synthetic budget with a ZERO firm price: REFUSED, so the digests "
                      "must equal the pins"
                : mode == charter_mode::stockpile
                    ? " — the Digitisation span ON and the shipped path's own stockpile budget "
                      "(BL-1042); its digests are EXPECTED to differ from the pins"
                    : " (an empty budget reaches the seam; the digests must equal the pins)");
    std::printf("BL-1031. FNV-1a 64: D_search the walk; D_land the snapshot as the landscape "
                "lands; D_settle the snapshot + state_hash after the validation run; D_seat the "
                "seat + the snapshot after it.\n");
    if (check)
        std::printf("Checked against %zu pinned rows in this source. A differing row FAILS and "
                    "names its digest; it is never re-pinned by the change that moved it.\n",
                    k_world_digest_pins.size());
    std::printf("\nseed  D_search          D_land            D_settle          D_seat            "
                "snapshot MB land/settle/seat%s\n", check ? "  verdict" : "");
    std::fflush(stdout);

    int failed = 0, threw = 0, passed = 0, unbalanced_rows = 0;
    std::vector<world_digests> got(seeds.size());
    std::vector<bool>          ok(seeds.size(), false);
    for (std::size_t i = 0; i < seeds.size(); ++i)
    {
        const uint32_t seed = seeds[i];
        // BL-1032's budget for this seed, and the report its winner leaves.
        charter_budget       budget;
        charter_spend_report report;
        harness_charter_input charter;
        std::size_t legacy_specialists = 0, legacy_firms = 0;
        std::unique_ptr<app_start_world> start;
        try
        {
            if (mode == charter_mode::zero)
            {
                std::map<entity_id, std::int32_t> zeros;
                for (entity_id id = 1; id <= k_zero_budget_ids; ++id)
                    zeros[id] = 0;
                budget = charter_budget(zeros);
            }
            else if (mode == charter_mode::synthetic || mode == charter_mode::refused)
            {
                // 1x, measured: the legacy landscape on this seed, built in this
                // process exactly as the none row is, and freed before the budget
                // world is built.
                const auto legacy = std::make_unique<app_start_world>();
                world_params lp{};
                lp.seed = seed;
                build_app_start_world(lua, lp, *legacy);
                legacy_specialists = legacy->land.specialists.size();
                legacy_firms       = legacy->land.firms.size();
                budget = synthetic_charter_budget(
                    legacy->w, seed,
                    static_cast<std::int64_t>(legacy_specialists + legacy_firms), charter_scale);
                charter.spend = synthetic_charter_spend();
                // BL-1039: `--resource-cap on|off` selects the two LEGACY rules,
                // BL-1033's cap kept (fixed) and cap lifted, so their digests can
                // be recorded and re-checked across a change to the budget path.
                charter.spend.resource_cap_rule =
                    resource_cap ? charter_cap_rule::fixed : charter_cap_rule::lifted;
                // Lifted reads no per-good cap, so it carries none (a set one is
                // refused). Unread by the spend, so the digests do not move.
                if (!resource_cap)
                    charter.spend.per_resource_firm_cap = 0;
                // `refused`: the same budget, and a price with no default left
                // at its default — the refusal a caller that forgets a price gets.
                if (mode == charter_mode::refused)
                    charter.spend.firm_price_points = 0;
            }
            if (mode != charter_mode::none && mode != charter_mode::stockpile)
                charter.budget = &budget;
            // BL-1042: the shipped path (none, stockpile) hands in no budget, so
            // the mirror builds the stockpile's; every mode takes the report.
            charter.report = &report;
            start = std::make_unique<app_start_world>();
            build_and_seat(lua, seed, /*fast=*/false, *start, &got[i], charter,
                           /*digitisation_span=*/mode == charter_mode::stockpile);
            if (start->land.stockpile_path)
                budget = start->land.stockpile.budget;   // the budget the shipped path passed
            ok[i] = true;
        }
        catch (const std::exception& e)
        {
            ++threw;
            std::printf("%4u  THREW: %s\n", seed, e.what());
            std::fflush(stdout);
            continue;
        }
        catch (...)
        {
            ++threw;
            std::printf("%4u  THREW: unknown\n", seed);
            std::fflush(stdout);
            continue;
        }

        const world_digests& d = got[i];
        std::printf("%4u  %016" PRIX64 "  %016" PRIX64 "  %016" PRIX64 "  %016" PRIX64
                    "  %.2f/%.2f/%.2f",
                    seed, d.search, d.land, d.settle, d.seat,
                    d.land_bytes / 1048576.0, d.settle_bytes / 1048576.0,
                    d.seat_bytes / 1048576.0);
        // BL-1039 fix round: a budget row that does not balance FAILS, in both
        // modes — checked from the budget and the records, not the report's totals.
        std::string unbalanced = (!budget.empty())
            ? charter_balance_failure(budget, report) : std::string();
        // BL-1042: the stockpile's own account, and the two closed together.
        if (start->land.stockpile_path)
        {
            unbalanced += stockpile_account_failure(start->land.stockpile, report);
            if (mode == charter_mode::none && start->land.stockpile.points_total != 0)
                unbalanced += " the span is off but the stockpile holds points;";
            if (mode == charter_mode::stockpile && budget.empty())
                unbalanced += " the span is on but the stockpile budget is EMPTY (R7 needs a "
                              "non-empty budget);";
        }
        if (!unbalanced.empty())
            ++unbalanced_rows;
        if (check)
        {
            const world_digest_pin* pin = nullptr;
            for (const world_digest_pin& p : k_world_digest_pins)
                if (p.seed == seed)
                    pin = &p;
            if (pin == nullptr)
            {
                ++failed;
                std::printf("  FAIL no pinned row for this seed\n");
            }
            else
            {
                std::string diffs;
                auto cmp = [&](const char* name, std::uint64_t pinned, std::uint64_t now) {
                    if (pinned == now)
                        return;
                    char buf[96];
                    std::snprintf(buf, sizeof buf, " %s (pinned %016" PRIX64 ")", name, pinned);
                    diffs += buf;
                };
                cmp("D_search", pin->search, d.search);
                cmp("D_land",   pin->land,   d.land);
                cmp("D_settle", pin->settle, d.settle);
                cmp("D_seat",   pin->seat,   d.seat);
                // The refusal is part of the contract, not only the bytes (cold
                // re-review, 2026-09-17): a refused budget must RAISE both flags,
                // and an empty or all-zero one must raise neither and write no
                // report, or a digest-identical world hides a broken seam.
                const bool search_refused = start->land.search.charter_refused;
                const bool report_written = report.refused || report.points_budgeted != 0
                                         || !report.charters.empty();
                if (mode == charter_mode::refused && !(search_refused && report.refused))
                    diffs += " REFUSAL-FLAG (the search or the apply did not report the refusal)";
                // BL-1042 fix round: `none` too — every pinned world is span
                // off, so the shipped path's stockpile budget is empty there
                // and must raise no flag and write no report either.
                if ((mode == charter_mode::empty || mode == charter_mode::zero
                     || mode == charter_mode::none)
                    && (search_refused || report_written))
                    diffs += " EMPTY-FLAG (an empty budget raised the refusal or wrote a report)";
                if (!unbalanced.empty())
                    diffs += " UNBALANCED:" + unbalanced;
                if (diffs.empty())
                {
                    ++passed;
                    std::printf("  PASS\n");
                }
                else
                {
                    ++failed;
                    std::printf("  FAIL%s\n", diffs.c_str());
                }
            }
        }
        else
        {
            std::printf("%s%s\n", unbalanced.empty() ? "" : "  UNBALANCED:", unbalanced.c_str());
        }
        if (start->land.stockpile_path)
            print_stockpile_account(start->w, start->land.stockpile, report);
        if (mode != charter_mode::none)
            print_charter_report(start->w, mode, budget,
                                 start->land.stockpile_path ? stockpile_charter_spend() : charter.spend,
                                 report, start->land.search,
                                 legacy_specialists, legacy_firms, charter_scale);
        std::fflush(stdout);
    }

    if (!check)
    {
        // Paste-ready: exactly the row shape of k_world_digest_pins.
        std::printf("\n// pin rows\n");
        for (std::size_t i = 0; i < seeds.size(); ++i)
            if (ok[i])
                std::printf("    { %2uu, 0x%016" PRIX64 "ull, 0x%016" PRIX64 "ull, 0x%016" PRIX64
                            "ull, 0x%016" PRIX64 "ull },\n",
                            seeds[i], got[i].search, got[i].land, got[i].settle, got[i].seat);
        if (unbalanced_rows > 0)
            std::printf("\n%d budget row(s) UNBALANCED — a failing check (BL-1039 fix round)\n",
                        unbalanced_rows);
        return (threw || unbalanced_rows > 0) ? 1 : 0;
    }

    // Coverage (cold review, 2026-09-17): a run over a subset of the pinned seeds
    // is a quick check, not the proof. Only full coverage prints PASS and exits 0.
    std::size_t covered = 0;
    for (const world_digest_pin& p : k_world_digest_pins)
        for (uint32_t s : seeds)
            if (s == p.seed) { ++covered; break; }
    const bool clean   = failed == 0 && threw == 0 && passed > 0;
    const bool full    = covered == k_world_digest_pins.size();
    std::printf("\n%d/%zu rows PASS, %d FAIL, %d threw; %zu of %zu pinned seeds checked\n%s\n",
                passed, seeds.size(), failed, threw, covered, k_world_digest_pins.size(),
                !clean ? "DIGEST CHECK FAILED"
                       : full ? "DIGEST CHECK PASS"
                              : "DIGEST CHECK PARTIAL - the rows checked pass, but pinned seeds were left out");
    return !clean ? 1 : full ? 0 : 3;
}

// --- BL-1033: what a denser corporate web costs --------------------------------
//
// `player_seed_sweep --charter-cost [--seeds a,b,c] [--budget-scales 1,2,4]
//       [--resource-cap on|off|both] [--province-cap on|off|both]
//       [--specialist-prices 4,8] [--ladder-scales 2|all] [--no-extra]
//       [--no-forced] [--forced-only] [--forced-radius N] [--forced-pick sparse|richest]
//       [--live-ticks N]
//       [--out file.json] [--note TEXT]`
//
// WHY (DIGITISATION.md hard part 7, § Open questions; Ben, NR-889). The charter
// prices are measured against live-play cost before either is fixed, and the
// per-resource firm cap is measured KEPT and LIFTED before the ruling on which
// gives way. This mode REPORTS; nothing in it is a verdict on a price or a cap.
//
// THE PRICE. A firm is 1 point (synthetic_charter_spend). A specialist costs a
// whole number of firm charters (DIGITISATION.md § 1), read from
// --specialist-prices (each > 0; default 4,8). The FIRST entry is the BASE price;
// every later entry is a LADDER rung. Every row carries its price in its label,
// and the table and the JSON write firm_price_points, specialist_firm_charters
// and the specialist's price in points on every row (the none row, which spends
// no budget, writes '-' and null).
//
// THE MATRIX, per seed. Every configuration is a full campaign start in app
// order — the landscape search and its winner, the validation run, the seat —
// and then a LIVE WINDOW after the seat:
//   none                         the legacy baseline, no budget (the shipped path)
//   synthetic Sx rcap R pcap P sp<base>
//                                for each --budget-scales S x --resource-cap R x
//                                --province-cap P, at the BASE price (defaults
//                                1,2,4 x on,off x on, at 4)
//   synthetic Lx rcap R pcap P sp<rung>
//                                for each LADDER rung x --ladder-scales L x R x P
//                                (defaults: rung 8 x 2 x on,off x on). `all` takes
//                                the --budget-scales list, widening to the full cross.
//   synthetic 4x rcap off pcap off sp<base>   the extra row (--no-extra drops it)
//   forced province cap          at the BASE price: the 1x total on ONE centre, window radius
//                                --forced-radius (default 4): BUILD ONLY (the
//                                reason is decided at the apply), and the one row
//                                with a verdict — province_cap > 0 and the points
//                                balanced (--no-forced drops it). --forced-only runs
//                                the none row build-only (it measures 1x) and this.
//                                The centre is `--forced-pick sparse` (default:
//                                pick_sparse_province_centre) or `richest`.
//
// WHY NOT THE RICHEST CENTRE AT RADIUS 1 (measured 2026-09-17, seed 28, both
// readings reproducible with --forced-pick richest --forced-radius 1|4). At radius
// 1 the window is 5 tiles and the specialist and three firms filled all five:
// window_exhausted 80, province_cap 0. At radius 4 the 49-tile window took 21
// firms and filled whole — the provinces around that centre are so small that the
// tiles ran out before any province held two firms: window_exhausted 62,
// province_cap 0. The cap binds only where a window is wider than the provinces
// it spans and no second rung absorbs the charters, so the default pick reads
// exactly that ground.
//
// BL-1039 — THE RULED RULES, as more matrix axes. `--resource-cap` also takes
// `sqrt` (the square-root per-good cap, DIGITISATION.md § 1), which runs once per
// `--density-ceilings` entry and has no default ceiling; `on` and `off` are the
// legacy fixed and lifted rules, unchanged (biggest gap first); `sqrt` fills the
// goods IN TURN (Ben, 2026-09-18). A specialist's capital is today's draw on
// every row. Every budget row prints its rule, one line per body (B on firms, G,
// B_ref, the per-good cap, firms PER GOOD, and under `sqrt` the turn's spread),
// and checks them at land (`check_charter_rules`: a FAIL fails the run); every
// full row prints the seated corporation's balance and solvent flag. The default
// matrix is BL-1033's, unchanged.
//
// THE DEFAULT MATRIX is eleven rows per seed — ten full rows and one build-only
// row, sized so three seeds stay affordable:
//   none; 1x, 2x, 4x x rcap on/off at sp4 (pcap on); 2x x rcap on/off at sp8
//   (pcap on); 4x rcap off pcap off at sp4; forced province cap at sp4.
// Flags widen it (--province-cap both, --ladder-scales all, more prices).
//
// THE BUDGET IS SYNTHETIC TEST INPUT (harness_params.hpp): seeded weights, never
// population, 1x = the none row's chartered count on the same seed. Every header
// says so, and no reading here is a density-follows-cities result. A scale whose
// synthetic total rounds to 0 on a seed (llround(scale x 1x) == 0) would be an
// EMPTY budget — today's world under a budget label — so the run REFUSES it with
// exit 2 as soon as that seed's 1x is measured, before any row settles.
//
// EVERY ROW BUILDS ITS OWN WORLD, generation included, and never copies one.
// MEASURED 2026-09-17 (seed 28): a pre-landscape world built once and COPIED per
// row matched the pins at D_search and D_land and DIFFERED at D_settle and D_seat
// — a copied world does not settle byte for byte as the one it was copied from
// (the `world` copy, not the registry's: harness_params.hpp build_app_base_world
// records the probe). So the row is `build_app_base_world` then `apply_app_start_landscape` on the
// same object (exactly `build_app_start_world`, split only so the landscape
// phase is timed on its own). The none row takes the four BL-1031 digests and,
// on a pinned seed, must match the pins — the proof this mode measures the
// shipped start. A mismatch fails the run.
//
// TIMINGS ARE WALL CLOCK and only as clean as the machine was quiet. `--note`
// records the run conditions into the JSON; the build type prints on every header.
//   search   ms per evaluation at thread_count 1 (the app's): round_ms[0] is the
//            seed candidate's evaluation; the rest of round_ms over the rest of
//            the evaluations is the per-proposal mean.
//   val      ms per economy tick over the 12 validation ticks (run_app_validation_settle).
//   live     ms per economy tick over the live window after the seat
//            (run_app_live_window — app.cpp:380-423 into step_economy, not
//            spectating).
//   BOTH ARE A LOWER BOUND on the app's step_economy. They time laps 0-5 —
//   phase 5 is the app's "standings + convoy credit + exits" lap, with
//   compute_corp_standings mirrored — and NOT laps 6-8: the agency comms, battle
//   dispatches, persona counsel, the history recorders and the strategy readout
//   are neither run nor timed (src/core, src/ui). That gap widens with density.
//   evalsDue a COUNT, not a cost: corporations corp_strategic_eval_due names at the
//            live window's day tick. post_persona_counsel evaluates at most the
//            one open-channel corporation per tick (BL-398,
//            session_history.cpp:206-220), so its live cost does not scale with
//            corporation count; the column is printed away from the timings.

struct cost_config
{
    enum class kind { none, synthetic, forced };
    kind   k             = kind::none;
    double scale         = 1.0;
    /// The per-good cap rule (BL-1039): `fixed` is BL-1033's "rcap on", `lifted`
    /// its "rcap off", `sqrt_capital` the ruled square root under `density_ceiling`.
    charter_cap_rule resource_cap_rule = charter_cap_rule::fixed;
    std::int32_t     density_ceiling   = 0;   ///< sqrt_capital only
    bool   province_cap  = true;
    int    window_radius = 4;
    /// The specialist's price as a whole number of firm charters (a --specialist-prices
    /// entry). The firm's price is synthetic_charter_spend's. Unused on the none row.
    std::int32_t specialist_firm_charters = 4;
    /// Stop after the landscape lands (no settle, seat or live window). Always
    /// true for `forced`; true for `none` only under --forced-only.
    bool   build_only    = false;
    /// The 1x the budget was built from, for the forced row's charter report only.
    std::size_t legacy_specialists = 0, legacy_firms = 0;
    /// The forced row's pick (`--forced-pick`) and centre, recorded for the JSON.
    bool        forced_pick_sparse = true;
    entity_id   forced_centre      = null_entity;
};

/// "on" / "off" for the two legacy rules — BL-1033's labels, kept so its rows
/// read the same — and "sqrt cN" for the square root under ceiling N.
std::string cost_cap_label(const cost_config& c)
{
    switch (c.resource_cap_rule)
    {
    case charter_cap_rule::fixed:  return "on";
    case charter_cap_rule::lifted: return "off";
    case charter_cap_rule::sqrt_capital:
        return "sqrt c" + std::to_string(static_cast<int>(c.density_ceiling));
    }
    return "?";
}

std::string cost_config_label(const cost_config& c)
{
    char buf[160];
    switch (c.k)
    {
    case cost_config::kind::none:
        return "none (legacy)";
    case cost_config::kind::synthetic:
        std::snprintf(buf, sizeof buf, "synthetic %gx rcap %s pcap %s sp%d", c.scale,
                      cost_cap_label(c).c_str(), c.province_cap ? "on" : "off",
                      static_cast<int>(c.specialist_firm_charters));
        return buf;
    case cost_config::kind::forced:
        std::snprintf(buf, sizeof buf, "forced pcap (%gx on 1 centre, r%d, sp%d)", c.scale,
                      c.window_radius, static_cast<int>(c.specialist_firm_charters));
        return buf;
    }
    return "?";
}

struct tick_summary
{
    int    n = 0;
    double median = 0.0, mean = 0.0, min = 0.0, max = 0.0;
};

tick_summary summarise(const std::vector<double>& v)
{
    tick_summary s;
    s.n = static_cast<int>(v.size());
    if (v.empty())
        return s;
    std::vector<double> sorted = v;
    std::sort(sorted.begin(), sorted.end());
    const std::size_t n = sorted.size();
    s.median = (n % 2 == 1) ? sorted[n / 2] : 0.5 * (sorted[n / 2 - 1] + sorted[n / 2]);
    double sum = 0.0;
    for (const double x : sorted)
        sum += x;
    s.mean = sum / static_cast<double>(n);
    s.min  = sorted.front();
    s.max  = sorted.back();
    return s;
}

struct body_counts
{
    int corps = 0, background = 0;
    /// The body carrying the most corporations (ties to the lower id), and its counts.
    entity_id busiest = null_entity;
    int busiest_corps = 0, busiest_background = 0;
};

/// Corporations in the world and on its busiest body. A corporation's body is
/// the body of its first holding's tile; one with no resolvable holding counts
/// in the world total only.
body_counts count_corporations(const world& w)
{
    body_counts out;
    std::map<entity_id, std::pair<int, int>> by_body;
    for (const auto& [cid, cc] : w.corporations)
    {
        ++out.corps;
        if (cc.is_background)
            ++out.background;
        if (cc.assets.empty())
            continue;
        const auto b = w.buildings.find(cc.assets.front());
        if (b == w.buildings.end())
            continue;
        const auto t = w.tiles.find(b->second.tile);
        if (t == w.tiles.end())
            continue;
        auto& n = by_body[t->second.body];
        ++n.first;
        if (cc.is_background)
            ++n.second;
    }
    for (const auto& [body, n] : by_body)   // ascending id: a tie keeps the lower
        if (n.first > out.busiest_corps)
        {
            out.busiest            = body;
            out.busiest_corps      = n.first;
            out.busiest_background = n.second;
        }
    return out;
}

struct cost_row
{
    cost_config cfg;
    std::string label;
    bool        threw = false;
    std::string error;
    double      wall_ms = 0.0;
    double      base_ms = 0.0;   ///< generation through load_economy (build_app_base_world)
    bool        build_only = false;

    /// Set when the at-land hook refused the row (a zero synthetic total): the
    /// row stopped at the landscape and nothing after it was measured.
    bool        stopped_at_land = false;

    // --- the budget and its spend (budget rows only) ---
    bool        budget_row = false;
    /// The prices the row's spend ran on, read back from the spend params it
    /// handed the seam (budget rows only; the none row spends nothing).
    std::int32_t firm_price_points = 0, specialist_firm_charters = 0;
    long long    specialist_price_points = 0;
    long long   points_budgeted = 0, points_spent = 0, points_unspent = 0;
    bool        balanced = true;
    std::string balance_fail;   ///< every broken clause (charter_balance_failure); empty = balanced
    bool        refused  = false;
    std::array<long long, charter_unspent_reason_count> unspent_by_reason{};
    int         budget_centres = 0;
    int         richest_centre_points = 0;
    int         centres_affording_specialist = 0;
    charter_spill spec_spill, firm_spill;

    // --- BL-1039: the rules the spend ran on, and what they did ---
    std::int32_t per_resource_firm_cap = 0, max_firms_per_body = 0, density_ceiling = 0;
    std::vector<charter_body_record> bodies;
    charter_rule_check rule_check;              ///< taken AS THE LANDSCAPE LANDS
    /// Every budget specialist as chartered, ascending corp id (for the seat's
    /// solvency count).
    std::vector<entity_id> budget_specialists;

    // --- the landscape as it landed ---
    std::size_t specialists = 0, firms = 0;
    bool        any_specialist = false;
    double      largest_nation_share = 0.0;   ///< of specialists, by home nation
    entity_id   largest_nation = null_entity;
    int         specialist_nations = 0;
    body_counts at_land, at_seat;

    // --- the search ---
    int    evaluations = 0;
    int    accepted = 0;
    double seed_eval_ms = 0.0, proposal_mean_ms = 0.0, search_ms = 0.0, landscape_ms = 0.0;
    std::vector<double> round_ms;

    // --- ticks ---
    app_tick_timing val, live;
    tick_summary    val_sum, live_sum;
    /// Mean of live.strategic_evals_due — a COUNT, not a cost (see the section note).
    double          strategic_evals_due_mean = 0.0;

    // --- the seat ---
    entity_id seated = null_entity;
    bool      floor_unmet = false;
    int       seat_specialists = 0, shortlist = 0;
    int       trail_n = 0;
    double    trail_min = 0.0, trail_median = 0.0, trail_max = 0.0, trail_neg_share = 0.0;
    /// BL-1039: the SEATED corporation's balance at the seat and its solvent flag
    /// (spawn_seat_candidate's: balance > 0), and how many of the spend's
    /// specialists stand solvent at the seat (budget rows).
    float     seat_balance = 0.0f;
    bool      seat_solvent = false, seat_found = false;
    int       specialists_at_seat = 0, specialists_solvent_at_seat = 0;

    // --- the none row's fidelity check ---
    bool          digested = false, pinned = false, digest_match = false;
    world_digests dig;
    std::string   digest_diff;

    // --- the forced row's verdict ---
    bool forced_pass = false;
};

/// Run one configuration: its own world built in app order, then measured.
/// @p at_land runs as the landscape lands; returning false stops the row there
/// (`stopped_at_land`), before any settle, seat or live window.
void run_cost_config(lua_state& lua, uint32_t seed, const cost_config& cfg,
                     const charter_budget* budget, int live_ticks, cost_row& row,
                     const std::function<bool(const app_start_world&)>& at_land = {})
{
    using clk = std::chrono::steady_clock;
    const auto wall0 = clk::now();
    row.cfg        = cfg;
    row.label      = cost_config_label(cfg);
    row.build_only = cfg.build_only || cfg.k == cost_config::kind::forced;
    row.budget_row = cfg.k != cost_config::kind::none;

    // The spend this row hands the seam, and the prices it records — set first,
    // so even a row that throws below carries the price it was run at.
    charter_spend_report  report;
    harness_charter_input charter;
    if (row.budget_row)
    {
        charter.budget              = budget;
        charter.spend               = synthetic_charter_spend();
        charter.spend.resource_cap_rule = cfg.resource_cap_rule;
        // Lifted reads no per-good cap, so it carries none (a set one is refused)
        // and reports no B_ref; unread by the spend, so the world does not move.
        if (cfg.resource_cap_rule == charter_cap_rule::lifted)
            charter.spend.per_resource_firm_cap = 0;
        charter.spend.density_ceiling   =
            cfg.resource_cap_rule == charter_cap_rule::sqrt_capital ? cfg.density_ceiling : 0;
        charter.spend.province_cap  = cfg.province_cap;
        charter.spend.window_radius = cfg.window_radius;
        charter.spend.specialist_firm_charters = cfg.specialist_firm_charters;
        charter.report              = &report;
        row.firm_price_points        = charter.spend.firm_price_points;
        row.specialist_firm_charters = charter.spend.specialist_firm_charters;
        row.specialist_price_points  = static_cast<long long>(charter.spend.specialist_price_points());
        row.per_resource_firm_cap    = charter.spend.per_resource_firm_cap;
        row.max_firms_per_body       = charter.spend.max_firms_per_body;
        row.density_ceiling          = charter.spend.density_ceiling;
    }

    // A budget row with no points would take the legacy branch and measure
    // TODAY'S world under a budget label. run_charter_cost refuses such a scale
    // (exit 2) before any budget row runs; this is the backstop.
    if (row.budget_row && (budget == nullptr || budget->empty()))
        throw std::runtime_error("budget row '" + row.label + "' has an empty budget: it would "
                                 "measure today's world under a budget label");

    // The row's own world, from generation — never a copy (see the section note).
    auto run = std::make_unique<app_start_world>();
    {
        world_params p{};
        p.seed = seed;
        const auto g0 = clk::now();
        build_app_base_world(lua, p, *run);
        row.base_ms = std::chrono::duration<double, std::milli>(clk::now() - g0).count();
    }

    const auto land0 = clk::now();
    apply_app_start_landscape(*run, charter);
    row.landscape_ms = std::chrono::duration<double, std::milli>(clk::now() - land0).count();
    if (at_land && !at_land(*run))
    {
        row.stopped_at_land = true;
        row.wall_ms = std::chrono::duration<double, std::milli>(clk::now() - wall0).count();
        return;
    }

    const landscape_search_result& sr = run->land.search;
    row.evaluations  = sr.evaluations;
    row.accepted     = sr.accepted;
    row.round_ms     = sr.round_ms;
    row.seed_eval_ms = sr.round_ms.empty() ? 0.0 : sr.round_ms.front();
    for (const double ms : sr.round_ms)
        row.search_ms += ms;
    row.proposal_mean_ms = (sr.evaluations > 1)
        ? (row.search_ms - row.seed_eval_ms) / static_cast<double>(sr.evaluations - 1) : 0.0;

    const world& w = run->w;
    row.specialists    = run->land.specialists.size();
    row.firms          = run->land.firms.size();
    row.any_specialist = row.specialists > 0;
    {
        std::map<entity_id, int> by_nation;
        for (const entity_id sid : run->land.specialists)
            ++by_nation[w.corporations.at(sid).home_nation];
        row.specialist_nations = static_cast<int>(by_nation.size());
        int most = 0;
        for (const auto& [nation, n] : by_nation)   // ascending id: a tie keeps the lower
            if (n > most)
            {
                most               = n;
                row.largest_nation = nation;
            }
        row.largest_nation_share = row.specialists > 0
            ? static_cast<double>(most) / static_cast<double>(row.specialists) : 0.0;
    }
    row.at_land = count_corporations(w);

    if (row.budget_row)
    {
        row.refused         = report.refused;
        row.points_budgeted = report.points_budgeted;
        row.points_spent    = report.points_spent;
        row.points_unspent  = report.points_unspent;
        // BL-1039 fix round: re-derived from the budget and the records, and a
        // FAILING check on every budget row (run_charter_cost), not the forced
        // row's alone.
        row.balance_fail    = charter_balance_failure(*budget, report);
        row.balanced        = row.balance_fail.empty();
        for (const charter_unspent& u : report.unspent)
            row.unspent_by_reason[static_cast<std::size_t>(u.reason)] += u.points;
        row.budget_centres = static_cast<int>(budget->points().size());
        for (const auto& kv : budget->points())
        {
            row.richest_centre_points = std::max(row.richest_centre_points, kv.second);
            if (kv.second >= charter.spend.specialist_price_points())
                ++row.centres_affording_specialist;
        }
        measure_charter_spill(w, report, charter.spend.window_radius, row.spec_spill,
                              row.firm_spill);

        // BL-1039: the rules, checked NOW — as the landscape lands (a refused
        // row has no records).
        row.bodies             = report.bodies;
        row.budget_specialists = report.specialists;
        if (!report.refused)
            row.rule_check = check_charter_rules(w, *budget, report, charter.spend);
    }

    if (cfg.k == cost_config::kind::none)
    {
        row.digested = true;
        digest_at_land(*run, row.dig);
    }

    if (row.build_only && !row.budget_row)
    {
        row.wall_ms = std::chrono::duration<double, std::milli>(clk::now() - wall0).count();
        return;
    }
    if (row.build_only)
    {
        const long long pcap =
            row.unspent_by_reason[static_cast<std::size_t>(charter_unspent_reason::province_cap)];
        row.forced_pass = row.budget_row && !row.refused && row.balanced && pcap > 0;
        std::printf("      FORCED PROVINCE CAP — SYNTHETIC TEST INPUT, build only:\n");
        print_charter_report(w, charter_mode::synthetic, *budget, charter.spend, report, sr,
                             cfg.legacy_specialists, cfg.legacy_firms, cfg.scale);
        std::printf("      %s  province_cap unspent %lld points (must be > 0); points %s; %s\n",
                    row.forced_pass ? "PASS" : "FAIL", pcap,
                    row.balanced ? "balanced" : "UNBALANCED",
                    row.refused ? "REFUSED (unexpected)" : "not refused");
        row.wall_ms = std::chrono::duration<double, std::milli>(clk::now() - wall0).count();
        return;
    }

    run_app_validation_settle(run->w, run->reg, k_settle_ticks, &row.val);
    if (row.digested)
        digest_at_settle(*run, row.dig);

    // app::seat_player (app.cpp:933), as build_and_seat does.
    const spawn_seat_result res =
        seat_player_corporation(run->w, run->params.seed, run->land.search.winner_score);
    if (row.digested)
        digest_at_seat(*run, res, row.dig);
    row.seated           = res.seated;
    row.floor_unmet      = res.floor_unmet;
    row.seat_specialists = res.specialist_count;
    row.shortlist        = res.shortlist_size;
    for (const spawn_seat_candidate& c : res.candidates)
        if (c.corp == res.seated)
        {
            row.seat_found   = true;
            row.seat_balance = c.balance;
            row.seat_solvent = c.solvent;
        }
    for (const entity_id sid : row.budget_specialists)
    {
        const auto it = run->w.corporations.find(sid);
        if (it == run->w.corporations.end())
            continue;
        ++row.specialists_at_seat;
        if (it->second.balance > 0.0f)   // spawn_seat_candidate's `solvent`
            ++row.specialists_solvent_at_seat;
    }
    {
        std::vector<double> trail;
        for (const spawn_seat_candidate& c : res.candidates)
            if (c.shortlisted)
                trail.push_back(static_cast<double>(c.trailing_net));
        row.trail_n = static_cast<int>(trail.size());
        if (!trail.empty())
        {
            const tick_summary t = summarise(trail);
            row.trail_min    = t.min;
            row.trail_median = t.median;
            row.trail_max    = t.max;
            int neg = 0;
            for (const double x : trail)
                if (x < 0.0)
                    ++neg;
            row.trail_neg_share = static_cast<double>(neg) / static_cast<double>(trail.size());
        }
    }
    row.at_seat = count_corporations(run->w);

    // The live window, after the seat: econ ticks continue from the validation run.
    run_app_live_window(run->w, run->reg, /*first_econ_step=*/k_settle_ticks, live_ticks, &row.live);

    row.val_sum  = summarise(row.val.tick_ms);
    row.live_sum = summarise(row.live.tick_ms);
    if (!row.live.strategic_evals_due.empty())
    {
        double due = 0.0;
        for (const int n : row.live.strategic_evals_due)
            due += n;
        row.strategic_evals_due_mean = due / static_cast<double>(row.live.strategic_evals_due.size());
    }
    row.wall_ms = std::chrono::duration<double, std::milli>(clk::now() - wall0).count();
}

std::string json_escape(const std::string& in)
{
    std::string out;
    out.reserve(in.size() + 8);
    for (const char ch : in)
    {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '"' || c == '\\') { out += '\\'; out += ch; }
        else if (c == '\n')        out += "\\n";
        else if (c < 0x20)
        {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04x", c);
            out += buf;
        }
        else
            out += ch;
    }
    return out;
}

struct cost_options
{
    std::vector<double> scales = { 1.0, 2.0, 4.0 };
    /// --resource-cap on|off|both|sqrt, or a comma list of on/off/sqrt (BL-1039).
    /// `sqrt` runs once per --density-ceilings entry; the ceiling has no default.
    std::vector<charter_cap_rule> resource_caps = { charter_cap_rule::fixed, charter_cap_rule::lifted };
    std::vector<std::int32_t>     density_ceilings;
    std::vector<bool>   province_caps = { true };
    /// --specialist-prices: firm charters per specialist. [0] is the BASE price
    /// (the full scale cross, the extra row, the forced row); the rest are LADDER
    /// rungs, each run at `ladder_scales`.
    std::vector<std::int32_t> specialist_prices = { 4, 8 };
    /// --ladder-scales: the scales a ladder rung runs at (default 2x).
    std::vector<double> ladder_scales = { 2.0 };
    /// --ladder-scales all: a ladder rung runs at every --budget-scales entry.
    bool        ladder_all = false;
    bool        extra   = true;
    bool        forced  = true;
    bool        forced_only = false;
    int         forced_radius = 4;
    bool        forced_sparse = true;   ///< --forced-pick sparse (default) | richest
    int         live_ticks = 8;
    std::string out_path;
    std::string note;
    std::vector<std::string> argv;
};

struct cost_seed
{
    uint32_t    seed = 0;
    long long   legacy_1x = 0;
    std::size_t legacy_specialists = 0, legacy_firms = 0;
    bool        threw = false;
    std::string error;
    std::vector<cost_row> rows;
};

const char* build_type_label()
{
#ifdef NDEBUG
    return "Release (NDEBUG)";
#else
    return "DEBUG (NDEBUG unset) - these timings are NOT Release timings";
#endif
}

void write_cost_json(const std::string& path, const cost_options& opt,
                     const std::vector<cost_seed>& seeds,
                     // BL-1033 cold re-review: a refused scale used to exit before the
                     // JSON was written, leaving an --out file that read like a complete
                     // shorter run. An aborted run now says so on its face.
                     const char* aborted_reason = nullptr, uint32_t aborted_seed = 0)
{
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr)
    {
        std::printf("--out: cannot open '%s' for writing\n", path.c_str());
        return;
    }
    const auto b = [](bool v) { return v ? "true" : "false"; };
    std::fprintf(f, "{\n  \"tool\": \"player_seed_sweep --charter-cost\",\n");
    std::fprintf(f, "  \"item\": \"BL-1033, BL-1039\",\n");
    if (aborted_reason != nullptr)
        std::fprintf(f, "  \"aborted\": {\"reason\": \"%s\", \"seed\": %u, \"complete\": false},\n",
                     json_escape(aborted_reason).c_str(), aborted_seed);
    else
        std::fprintf(f, "  \"aborted\": null,\n");
    std::fprintf(f, "  \"synthetic_test_input\": true,\n");
    std::fprintf(f, "  \"synthetic_note\": \"%s\",\n",
                 json_escape("SYNTHETIC TEST INPUT: seeded weights, never population; 1x = the none "
                             "row's chartered count on the same seed. Never a density-follows-cities "
                             "reading.").c_str());
    std::fprintf(f, "  \"build\": \"%s\",\n", build_type_label());
#ifdef _MSC_FULL_VER
    std::fprintf(f, "  \"msc_full_ver\": %lld,\n", static_cast<long long>(_MSC_FULL_VER));
#endif
    std::fprintf(f, "  \"note\": \"%s\",\n", json_escape(opt.note).c_str());
    std::fprintf(f, "  \"argv\": [");
    for (std::size_t i = 0; i < opt.argv.size(); ++i)
        std::fprintf(f, "%s\"%s\"", i ? ", " : "", json_escape(opt.argv[i]).c_str());
    std::fprintf(f, "],\n");
    std::fprintf(f, "  \"validation_ticks\": %d,\n  \"live_ticks\": %d,\n  \"search_thread_count\": 1,\n",
                 k_settle_ticks, opt.live_ticks);
    std::fprintf(f, "  \"firm_price_points\": %d,\n  \"specialist_prices_firm_charters\": [",
                 static_cast<int>(synthetic_charter_spend().firm_price_points));
    for (std::size_t i = 0; i < opt.specialist_prices.size(); ++i)
        std::fprintf(f, "%s%d", i ? ", " : "", static_cast<int>(opt.specialist_prices[i]));
    std::fprintf(f, "],\n  \"ladder_scales\": ");
    if (opt.ladder_all)
        std::fprintf(f, "\"all\"");
    else
    {
        std::fprintf(f, "[");
        for (std::size_t i = 0; i < opt.ladder_scales.size(); ++i)
            std::fprintf(f, "%s%g", i ? ", " : "", opt.ladder_scales[i]);
        std::fprintf(f, "]");
    }
    std::fprintf(f, ",\n  \"price_note\": \"%s\",\n",
                 json_escape("Every row carries firm_price_points, specialist_firm_charters and "
                             "specialist_price_points (= firm x charters); null on the none row, "
                             "which spends no budget. specialist_prices_firm_charters[0] is the base "
                             "price; later entries are ladder rungs run at ladder_scales.").c_str());
    std::fprintf(f, "  \"timing_note\": \"%s\",\n",
                 json_escape("tick_ms times app::step_economy laps 0-5 only (phase 5 = standings + "
                             "convoy credit + firm exits, compute_corp_standings mirrored). Laps 6-8 "
                             "- agency comms, battle dispatches, persona counsel, the history "
                             "recorders, the strategy readout - are NOT run and NOT timed, so val and "
                             "live ms per tick are a LOWER BOUND on the app's step_economy that "
                             "widens with density. Wall clock: only as clean as the machine was quiet.").c_str());
    std::fprintf(f, "  \"strategic_evals_due_note\": \"%s\",\n",
                 json_escape("A COUNT, not a cost driver: corporations corp_strategic_eval_due names "
                             "at each live tick's day tick. post_persona_counsel evaluates at most the "
                             "one open-channel corporation per tick (BL-398, "
                             "session_history.cpp:206-220), so its live cost does not scale with "
                             "corporation count.").c_str());
    std::fprintf(f, "  \"rules_note\": \"%s\",\n",
                 json_escape("BL-1039 (Ben's rulings 2026-09-18). resource_cap_rule: fixed = BL-1033's "
                             "rcap on (per_resource_firm_cap per good per body), lifted = rcap off (no "
                             "per-good cap); both fill biggest gap first. sqrt_capital = max(c, floor(c "
                             "sqrt(B / B_ref))) under density_ceiling, goods filled IN TURN (a firm per "
                             "good each pass, ascending resource index, up to its cap; construction "
                             "capacity provisioned first, outside the turn), with c = "
                             "per_resource_firm_cap, B = the body's points on FIRMS (each "
                             "nation-resolved centre's points net of the specialist price it affords, in "
                             "whole firm charters), G = goods whose consumer + upkeep + construction "
                             "demand is > 0 on the body BEFORE the walk, B_ref = c x |G| x firm price, so "
                             "cap(B_ref) == c exactly. A specialist's capital is today's draw (400 +/- "
                             "40%). bodies[].firms_by_good: firms per good as the selection chartered "
                             "them; bodies[].turn: the turn's spread (sqrt only). checks: taken as the "
                             "landscape lands. seat: the seated corporation's balance after the "
                             "validation run and solvent = balance > 0.").c_str());
    std::fprintf(f, "  \"tick_phases\": [");
    for (int i = 0; i < k_app_tick_phase_count; ++i)
        std::fprintf(f, "%s\"%s\"", i ? ", " : "", k_app_tick_phase_names[i]);
    std::fprintf(f, "],\n  \"unspent_reasons\": [");
    for (int i = 0; i < charter_unspent_reason_count; ++i)
        std::fprintf(f, "%s\"%s\"", i ? ", " : "",
                     charter_unspent_reason_name(static_cast<charter_unspent_reason>(i)));
    std::fprintf(f, "],\n  \"seeds\": [\n");
    for (std::size_t si = 0; si < seeds.size(); ++si)
    {
        const cost_seed& s = seeds[si];
        std::fprintf(f, "    {\n      \"seed\": %u,\n      \"threw\": %s,\n      \"error\": \"%s\",\n",
                     s.seed, b(s.threw), json_escape(s.error).c_str());
        std::fprintf(f, "      \"legacy_1x\": %lld,\n"
                        "      \"legacy_specialists\": %zu,\n      \"legacy_firms\": %zu,\n"
                        "      \"configs\": [\n",
                     s.legacy_1x, s.legacy_specialists, s.legacy_firms);
        for (std::size_t ri = 0; ri < s.rows.size(); ++ri)
        {
            const cost_row& r = s.rows[ri];
            const char* kind = r.cfg.k == cost_config::kind::none ? "none"
                             : r.cfg.k == cost_config::kind::synthetic ? "synthetic" : "forced";
            const char* sep  = ri + 1 < s.rows.size() ? "," : "";
            std::fprintf(f, "        {\n");
            // `resource_cap` keeps BL-1033's boolean for the two legacy rules and
            // is null under sqrt_capital, which is neither.
            const char* rcap = r.cfg.resource_cap_rule == charter_cap_rule::fixed    ? "true"
                             : r.cfg.resource_cap_rule == charter_cap_rule::lifted   ? "false"
                                                                                     : "null";
            std::fprintf(f, "          \"label\": \"%s\", \"kind\": \"%s\", \"scale\": %g, "
                            "\"resource_cap\": %s, \"resource_cap_rule\": \"%s\", "
                            "\"density_ceiling\": %d, "
                            "\"province_cap\": %s, \"window_radius\": %d,\n",
                         json_escape(r.label).c_str(), kind, r.cfg.scale, rcap,
                         charter_cap_rule_name(r.cfg.resource_cap_rule),
                         static_cast<int>(r.cfg.resource_cap_rule == charter_cap_rule::sqrt_capital
                                              ? r.cfg.density_ceiling : 0),
                         b(r.cfg.province_cap), r.cfg.window_radius);
            if (r.budget_row)
                std::fprintf(f, "          \"firm_price_points\": %d, \"specialist_firm_charters\": %d, "
                                "\"specialist_price_points\": %lld,\n",
                             static_cast<int>(r.firm_price_points),
                             static_cast<int>(r.specialist_firm_charters), r.specialist_price_points);
            else
                std::fprintf(f, "          \"firm_price_points\": null, \"specialist_firm_charters\": null, "
                                "\"specialist_price_points\": null,\n");
            std::fprintf(f, "          \"threw\": %s, \"error\": \"%s\", \"wall_ms\": %.1f, "
                            "\"generation_ms\": %.1f, \"build_only\": %s, \"stopped_at_land\": %s",
                         b(r.threw), json_escape(r.error).c_str(), r.wall_ms, r.base_ms,
                         b(r.build_only), b(r.stopped_at_land));
            if (r.threw || r.stopped_at_land)
            {
                std::fprintf(f, "\n        }%s\n", sep);
                continue;
            }
            if (r.budget_row)
            {
                std::fprintf(f, ",\n          \"budget\": { \"points_budgeted\": %lld, \"points_spent\": %lld, "
                                "\"points_unspent\": %lld, \"balanced\": %s, \"balance_fail\": \"%s\", "
                                "\"refused\": %s, "
                                "\"centres\": %d, \"richest_centre_points\": %d, "
                                "\"centres_affording_specialist\": %d, \"unspent_by_reason\": {",
                             r.points_budgeted, r.points_spent, r.points_unspent, b(r.balanced),
                             json_escape(r.balance_fail).c_str(),
                             b(r.refused), r.budget_centres, r.richest_centre_points,
                             r.centres_affording_specialist);
                for (int i = 0; i < charter_unspent_reason_count; ++i)
                    std::fprintf(f, "%s\"%s\": %lld", i ? ", " : " ",
                                 charter_unspent_reason_name(static_cast<charter_unspent_reason>(i)),
                                 r.unspent_by_reason[static_cast<std::size_t>(i)]);
                const auto spill = [&](const charter_spill& sp) {
                    std::fprintf(f, "{ \"anchor_in\": %d, \"anchor_out\": %d, \"secondary_in\": %d, "
                                    "\"secondary_out\": %d, \"unresolved\": %d, \"farthest_tiles\": %.2f }",
                                 sp.anchor_in, sp.anchor_out, sp.second_in, sp.second_out,
                                 sp.unresolved, std::sqrt(static_cast<double>(sp.far_d2)));
                };
                std::fprintf(f, " },\n            \"holdings_specialist\": ");
                spill(r.spec_spill);
                std::fprintf(f, ", \"holdings_firm\": ");
                spill(r.firm_spill);
                std::fprintf(f, ",\n            \"holdings_inside_window\": %d, \"holdings_outside_window\": %d }",
                             r.spec_spill.anchor_in + r.spec_spill.second_in
                                 + r.firm_spill.anchor_in + r.firm_spill.second_in,
                             r.spec_spill.anchor_out + r.spec_spill.second_out
                                 + r.firm_spill.anchor_out + r.firm_spill.second_out);
                // BL-1039: the rules, per body, and the at-land checks.
                std::fprintf(f, ",\n          \"rules\": { \"per_resource_firm_cap\": %d, "
                                "\"max_firms_per_body\": %d, \"density_ceiling\": %d, "
                                "\"fill\": \"%s\" }",
                             static_cast<int>(r.per_resource_firm_cap),
                             static_cast<int>(r.max_firms_per_body), static_cast<int>(r.density_ceiling),
                             r.cfg.resource_cap_rule == charter_cap_rule::sqrt_capital
                                 ? "in_turn" : "biggest_gap_first");
                std::fprintf(f, ",\n          \"bodies\": [");
                for (std::size_t bi = 0; bi < r.bodies.size(); ++bi)
                {
                    const charter_body_record& br = r.bodies[bi];
                    std::fprintf(f, "%s\n            { \"body\": %u, \"firm_points\": %lld, "
                                    "\"goods_with_demand\": %d, \"goods\": [",
                                 bi ? "," : "", br.body, static_cast<long long>(br.firm_points),
                                 br.goods_with_demand);
                    for (std::size_t gi = 0; gi < br.goods.size(); ++gi)
                        std::fprintf(f, "%s\"%s\"", gi ? ", " : "",
                                     resource_names::name_of(static_cast<resource_type>(br.goods[gi])).c_str());
                    std::fprintf(f, "], \"reference_points\": %lld, \"per_good_cap\": %d, "
                                    "\"density_ceiling\": %d, \"yard_places\": %d, \"even_share\": %d, "
                                    "\"even_share_extra\": %d, \"firms\": %d, \"firms_by_good\": {",
                                 static_cast<long long>(br.reference_points),
                                 static_cast<int>(br.per_good_cap), static_cast<int>(br.density_ceiling),
                                 static_cast<int>(br.yard_places),
                                 static_cast<int>(br.even_share), static_cast<int>(br.even_share_extra),
                                 static_cast<int>(br.firms));
                    bool first_good = true;
                    for (std::size_t g = 0; g < br.firms_by_good.size(); ++g)
                        if (br.firms_by_good[g] > 0)
                        {
                            std::fprintf(f, "%s\"%s\": %d", first_good ? " " : ", ",
                                         resource_names::name_of(static_cast<resource_type>(g)).c_str(),
                                         static_cast<int>(br.firms_by_good[g]));
                            first_good = false;
                        }
                    std::fprintf(f, " }");
                    if (r.cfg.resource_cap_rule == charter_cap_rule::sqrt_capital)
                    {
                        const charter_turn_reading t = read_turn(br);
                        std::fprintf(f, ", \"turn\": { \"goods\": %d, \"goods_without_firm\": %d, "
                                        "\"fewest\": %d, \"most\": %d }",
                                     t.goods_in_turn, t.goods_without_firm, t.fewest, t.most);
                    }
                    std::fprintf(f, " }");
                }
                std::fprintf(f, "%s]", r.bodies.empty() ? "" : "\n          ");
                std::fprintf(f, ",\n          \"checks_at_land\": { \"pass\": %s, \"fail\": \"%s\" }",
                             b(r.rule_check.pass), json_escape(r.rule_check.fail).c_str());
                if (r.cfg.k == cost_config::kind::forced)
                    std::fprintf(f, ",\n          \"forced_province_cap_pass\": %s, \"forced_pick\": \"%s\", "
                                    "\"forced_centre\": %u",
                                 b(r.forced_pass), r.cfg.forced_pick_sparse ? "sparse" : "richest",
                                 r.cfg.forced_centre);
            }
            std::fprintf(f, ",\n          \"landscape\": { \"specialists\": %zu, \"firms\": %zu, "
                            "\"any_specialist\": %s, \"specialist_nations\": %d, "
                            "\"largest_nation_share\": %.4f, \"largest_nation\": %u,\n"
                            "            \"corporations_at_land\": %d, \"background_at_land\": %d, "
                            "\"busiest_body_at_land\": %u, \"busiest_body_corporations_at_land\": %d, "
                            "\"busiest_body_background_at_land\": %d }",
                         r.specialists, r.firms, b(r.any_specialist), r.specialist_nations,
                         r.largest_nation_share, r.largest_nation, r.at_land.corps,
                         r.at_land.background, r.at_land.busiest, r.at_land.busiest_corps,
                         r.at_land.busiest_background);
            std::fprintf(f, ",\n          \"search\": { \"evaluations\": %d, \"accepted\": %d, "
                            "\"seed_eval_ms\": %.1f, \"proposal_mean_ms\": %.1f, \"search_ms\": %.1f, "
                            "\"landscape_phase_ms\": %.1f, \"round_ms\": [",
                         r.evaluations, r.accepted, r.seed_eval_ms, r.proposal_mean_ms, r.search_ms,
                         r.landscape_ms);
            for (std::size_t i = 0; i < r.round_ms.size(); ++i)
                std::fprintf(f, "%s%.1f", i ? ", " : "", r.round_ms[i]);
            std::fprintf(f, "] }");
            if (r.build_only)
            {
                if (r.digested)
                    std::fprintf(f, ",\n          \"digests\": { \"search\": \"%016" PRIX64 "\", \"land\": \"%016"
                                    PRIX64 "\", \"pinned\": %s, \"match_search_and_land\": %s }",
                                 r.dig.search, r.dig.land, b(r.pinned), b(r.digest_match));
                std::fprintf(f, "\n        }%s\n", sep);
                continue;
            }
            const auto ticks = [&](const char* name, const app_tick_timing& t, const tick_summary& ts,
                                   bool live) {
                std::fprintf(f, ",\n          \"%s\": { \"n\": %d, \"median_ms\": %.2f, \"mean_ms\": %.2f, "
                                "\"min_ms\": %.2f, \"max_ms\": %.2f, \"tick_ms\": [",
                             name, ts.n, ts.median, ts.mean, ts.min, ts.max);
                for (std::size_t i = 0; i < t.tick_ms.size(); ++i)
                    std::fprintf(f, "%s%.2f", i ? ", " : "", t.tick_ms[i]);
                std::fprintf(f, "], \"phase_ms_sum\": {");
                for (int i = 0; i < k_app_tick_phase_count; ++i)
                    std::fprintf(f, "%s\"%s\": %.2f", i ? ", " : " ", k_app_tick_phase_names[i],
                                 t.phase_ms[i]);
                std::fprintf(f, " }");
                if (live)
                    std::fprintf(f, ", \"between_ticks_ms_sum\": %.2f", t.between_ms);
                std::fprintf(f, ", \"lower_bound\": true }");
            };
            ticks("validation_ticks", r.val, r.val_sum, false);
            ticks("live_ticks", r.live, r.live_sum, true);
            char seat_bal[48] = "null";
            if (r.seat_found && std::isfinite(r.seat_balance))
                std::snprintf(seat_bal, sizeof seat_bal, "%.4f", static_cast<double>(r.seat_balance));
            std::fprintf(f, ",\n          \"seat\": { \"seated\": %u, \"floor_unmet\": %s, "
                            "\"specialists\": %d, \"shortlist\": %d, \"trailing_net_n\": %d, "
                            "\"trailing_net_min\": %.1f, \"trailing_net_median\": %.1f, "
                            "\"trailing_net_max\": %.1f, \"trailing_net_negative_share\": %.4f, "
                            "\"corporations_at_seat\": %d, \"background_at_seat\": %d,\n"
                            "            \"busiest_body_at_seat\": %u, "
                            "\"busiest_body_corporations_at_seat\": %d, "
                            "\"busiest_body_background_at_seat\": %d,\n"
                            "            \"seat_balance\": %s, \"seat_solvent\": %s, "
                            "\"budget_specialists_standing\": %d, \"budget_specialists_solvent\": %d }",
                         r.seated, b(r.floor_unmet), r.seat_specialists, r.shortlist, r.trail_n,
                         r.trail_min, r.trail_median, r.trail_max, r.trail_neg_share,
                         r.at_seat.corps, r.at_seat.background, r.at_seat.busiest,
                         r.at_seat.busiest_corps, r.at_seat.busiest_background, seat_bal,
                         r.seat_found ? b(r.seat_solvent) : "null",
                         r.specialists_at_seat, r.specialists_solvent_at_seat);
            // A COUNT, kept apart from the timings (strategic_evals_due_note).
            std::fprintf(f, ",\n          \"strategic_evals_due\": { \"count_not_cost\": true, "
                            "\"mean\": %.2f, \"per_live_tick\": [", r.strategic_evals_due_mean);
            for (std::size_t i = 0; i < r.live.strategic_evals_due.size(); ++i)
                std::fprintf(f, "%s%d", i ? ", " : "", r.live.strategic_evals_due[i]);
            std::fprintf(f, "] }");
            if (r.digested)
                std::fprintf(f, ",\n          \"digests\": { \"search\": \"%016" PRIX64 "\", \"land\": \"%016"
                                PRIX64 "\", \"settle\": \"%016" PRIX64 "\", \"seat\": \"%016" PRIX64
                                "\", \"pinned\": %s, \"match\": %s, \"diff\": \"%s\" }",
                             r.dig.search, r.dig.land, r.dig.settle, r.dig.seat, b(r.pinned),
                             b(r.digest_match), json_escape(r.digest_diff).c_str());
            std::fprintf(f, "\n        }%s\n", sep);
        }
        std::fprintf(f, "      ]\n    }%s\n", si + 1 < seeds.size() ? "," : "");
    }
    std::fprintf(f, "  ]\n}\n");
    std::fclose(f);
}

/// The table's legend and column heads. Every price in it is read from the spend
/// params the rows run on (synthetic_charter_spend's firm price, the
/// --specialist-prices ladder), never restated as a literal.
void print_cost_table_header(const cost_options& opt)
{
    const charter_spend_params spend = synthetic_charter_spend();
    std::string ladder;
    for (std::size_t i = 0; i < opt.specialist_prices.size(); ++i)
    {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%s%d firm charters = %lld points%s", i ? ", " : "",
                      static_cast<int>(opt.specialist_prices[i]),
                      static_cast<long long>(spend.firm_price_points)
                          * static_cast<long long>(opt.specialist_prices[i]),
                      i == 0 ? " (base)" : "");
        ladder += buf;
    }
    std::printf("  SYNTHETIC TEST INPUT on every budget row (seeded weights, never population). "
                "PRICES per row: firm = %d point(s); specialist ladder %s. fP firm price in points, "
                "sFC specialist price in firm charters, sPts specialist price in points ('-' on the "
                "none row: it spends no budget) | unspent points by reason | anyS a specialist exists, "
                "natSh largest one-nation share of specialists | holdings inside/outside the window | "
                "corps/bg at land | search evaluations, seed-candidate ms, per-proposal mean ms | val "
                "and live ms per economy tick over step_economy laps 0-5 ONLY — a LOWER BOUND on the "
                "app's tick that widens with density: agency comms, battle dispatches, persona "
                "counsel, the history recorders and the strategy readout are NOT timed | shortlist, "
                "trailing net over it, negative share | evalsDue = strategic evals due per live tick "
                "(count) — NOT a cost: BL-398 bounds counsel's export and evaluation to the one "
                "open-channel corporation per tick, so only its sort and channel walk follow density. "
                "BL-1039: ceil = unspent as density_ceiling; BL-1060: late = late_shortfall, refsd = "
                "refused, share = share_unplaced (NR-905) — the ten reason columns sum to the row's "
                "unspent total; under each budget row, the rule line "
                "(per-good cap rule and fill order, ceiling, guard), one line per body (B on firms, "
                "G, B_ref, per-good cap, firms per good, and under sqrt the turn's spread) and the "
                "at-land checks, and on every full row the seated corporation's balance and solvent "
                "flag\n",
                static_cast<int>(spend.firm_price_points), ladder.c_str());
    std::printf("  %-52s %3s %3s %4s | %4s %5s | %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s | %4s %5s | "
                "%11s | %9s | %5s %7s %7s | %15s | %15s | %5s %26s %5s | %8s\n",
                "config", "fP", "sFC", "sPts", "spec", "firms", "no_gap", "prov", "window", "body",
                "ceil", "remain", "nonat", "late", "refsd", "share", "anyS", "natSh", "hold in/out",
                "corps/bg", "evals",
                "seed_ms", "prop_ms", "val med/mean", "live med/mean", "short", "trail8 min/med/max",
                "neg%", "evalsDue");
}

/// BL-1039 — the rows' rule, body and check lines (budget rows only).
void print_cost_row_rules(const cost_row& r)
{
    if (!r.budget_row || r.refused)
        return;
    std::printf("  %-52s   rule: per-good cap %s (c %d), %s, density ceiling %d, guard %d; "
                "specialist capital: today's draw\n", "",
                charter_cap_rule_name(r.cfg.resource_cap_rule), static_cast<int>(r.per_resource_firm_cap),
                r.cfg.resource_cap_rule == charter_cap_rule::sqrt_capital ? "goods IN TURN"
                                                                          : "biggest gap first",
                static_cast<int>(r.density_ceiling), static_cast<int>(r.max_firms_per_body));
    char indent[80];
    std::snprintf(indent, sizeof indent, "  %-52s   ", "");
    charter_spend_report bodies_only;
    bodies_only.cap_rule = r.cfg.resource_cap_rule;
    bodies_only.bodies   = r.bodies;
    print_charter_bodies(bodies_only, indent);
    std::printf("  %-52s   checks at land (B, B_ref, the cap by definition, firms per good re-tallied, "
                "ceiling, guard, cap(B_ref) == c): %s%s\n", "",
                r.rule_check.pass ? "PASS" : "FAIL:", r.rule_check.pass ? "" : r.rule_check.fail.c_str());
}

void print_cost_row(const cost_row& r)
{
    if (r.threw)
    {
        std::printf("  %-52s THREW: %s\n", r.label.c_str(), r.error.c_str());
        return;
    }
    char fp[16] = "-", sfc[16] = "-", spts[24] = "-";
    if (r.budget_row)
    {
        std::snprintf(fp, sizeof fp, "%d", static_cast<int>(r.firm_price_points));
        std::snprintf(sfc, sizeof sfc, "%d", static_cast<int>(r.specialist_firm_charters));
        std::snprintf(spts, sizeof spts, "%lld", r.specialist_price_points);
    }
    if (r.stopped_at_land)
    {
        std::printf("  %-52s %3s %3s %4s | stopped as the landscape landed: nothing measured\n",
                    r.label.c_str(), fp, sfc, spts);
        return;
    }
    const auto u = [&](charter_unspent_reason why) {
        return r.unspent_by_reason[static_cast<std::size_t>(why)];
    };
    char hold[32] = "-";
    if (r.budget_row)
        std::snprintf(hold, sizeof hold, "%d/%d",
                      r.spec_spill.anchor_in + r.spec_spill.second_in + r.firm_spill.anchor_in
                          + r.firm_spill.second_in,
                      r.spec_spill.anchor_out + r.spec_spill.second_out + r.firm_spill.anchor_out
                          + r.firm_spill.second_out);
    char corps[32];
    std::snprintf(corps, sizeof corps, "%d/%d", r.at_land.corps, r.at_land.background);
    // EVERY reason has a column, so the row sums to its unspent total; a reason
    // appended to the enum must add its column here.
    static_assert(charter_unspent_reason_count == 10, "a charter_unspent_reason has no cost-table column");
    std::printf("  %-52s %3s %3s %4s | %4zu %5zu | %6lld %6lld %6lld %6lld %6lld %6lld %6lld %6lld %6lld "
                "%6lld | %4s %5.2f | %11s | %9s | %5d %7.0f %7.0f",
                r.label.c_str(), fp, sfc, spts, r.specialists, r.firms,
                u(charter_unspent_reason::no_gap), u(charter_unspent_reason::province_cap),
                u(charter_unspent_reason::window_exhausted), u(charter_unspent_reason::body_cap),
                u(charter_unspent_reason::density_ceiling),
                u(charter_unspent_reason::remainder), u(charter_unspent_reason::no_nation),
                u(charter_unspent_reason::late_shortfall), u(charter_unspent_reason::refused),
                u(charter_unspent_reason::share_unplaced),
                r.any_specialist ? "yes" : "NO", r.largest_nation_share, hold, corps,
                r.evaluations, r.seed_eval_ms, r.proposal_mean_ms);
    if (r.build_only)
    {
        std::printf(" | %15s | %15s | build only%s\n", "-", "-",
                    !r.budget_row ? "" : r.forced_pass ? ": PASS" : ": FAIL");
        std::printf("  %-52s   busiest body AT LAND %u: %d corps, %d bg; AT SEAT: not reached (build only)\n",
                    "", r.at_land.busiest, r.at_land.busiest_corps, r.at_land.busiest_background);
        print_cost_row_rules(r);
        if (r.digested)
            std::printf("  %-52s   digests D_search %016" PRIX64 " D_land %016" PRIX64 " — %s\n", "",
                        r.dig.search, r.dig.land,
                        !r.pinned ? "no pinned row for this seed"
                        : r.digest_match ? "MATCH the BL-1031 pins (search and land only: build-only row)"
                                         : "DIFFER from the pins");
        return;
    }
    // The timings, then the seat, then the due COUNT last — apart from the timings.
    std::printf(" | %7.1f/%-7.1f | %7.1f/%-7.1f | %5d %8.0f/%8.0f/%8.0f %5.1f | %8.1f\n",
                r.val_sum.median, r.val_sum.mean, r.live_sum.median, r.live_sum.mean,
                r.shortlist, r.trail_min, r.trail_median, r.trail_max, 100.0 * r.trail_neg_share,
                r.strategic_evals_due_mean);
    std::printf("  %-52s   points %lld = %lld + %lld [%s%s]; generation %.0f ms; landscape phase %.0f ms "
                "(search %.0f ms); row wall %.0f ms; seated %u%s\n",
                "", r.points_budgeted, r.points_spent, r.points_unspent,
                r.budget_row ? (r.balanced ? "balanced" : "UNBALANCED — FAILS the run:") : "no budget",
                r.budget_row ? r.balance_fail.c_str() : "",
                r.base_ms, r.landscape_ms, r.search_ms, r.wall_ms,
                r.seated, r.floor_unmet ? " (floor UNMET)" : "");
    std::printf("  %-52s   corps/bg AT LAND %d/%d, AT SEAT %d/%d; busiest body AT LAND %u: %d corps, "
                "%d bg; busiest body AT SEAT %u: %d corps, %d bg\n",
                "", r.at_land.corps, r.at_land.background, r.at_seat.corps, r.at_seat.background,
                r.at_land.busiest, r.at_land.busiest_corps, r.at_land.busiest_background,
                r.at_seat.busiest, r.at_seat.busiest_corps, r.at_seat.busiest_background);
    // BL-1039: the seat's balance and solvent flag, on every full row.
    if (r.seat_found)
        std::printf("  %-52s   seat: corp %u balance %.1f, solvent %s", "", r.seated,
                    static_cast<double>(r.seat_balance), r.seat_solvent ? "yes" : "NO");
    else
        std::printf("  %-52s   seat: corp %u not among the ranked candidates", "", r.seated);
    if (r.budget_row)
        std::printf("; budget specialists solvent at the seat: %d of %d standing\n",
                    r.specialists_solvent_at_seat, r.specialists_at_seat);
    else
        std::printf("\n");
    print_cost_row_rules(r);
    const auto phases = [&](const char* name, const app_tick_timing& t) {
        std::printf("  %-52s   %s phase ms sums (lower bound):", "", name);
        for (int i = 0; i < k_app_tick_phase_count; ++i)
            std::printf(" %s %.0f", k_app_tick_phase_names[i], t.phase_ms[i]);
        std::printf("\n");
    };
    phases("val ", r.val);
    phases("live", r.live);
    if (r.digested)
    {
        const std::string verdict = !r.pinned ? "no pinned row for this seed (copy fidelity unchecked)"
            : r.digest_match ? "MATCH the BL-1031 pins: this row is the shipped start"
                             : "DIFFER from the pins:" + r.digest_diff;
        std::printf("  %-52s   digests %016" PRIX64 " %016" PRIX64 " %016" PRIX64 " %016" PRIX64
                    " — %s\n", "", r.dig.search, r.dig.land, r.dig.settle, r.dig.seat,
                    verdict.c_str());
    }
}

/// The forced row's centre, and why it was picked.
struct forced_pick
{
    entity_id centre = null_entity;
    bool      sparse = true;          ///< false: the synthetic budget's richest centre
    int       window_tiles = 0;       ///< the centre nation's tiles inside the window
    int       window_provinces = 0;   ///< distinct provinces among them
    bool      rung2_empty = false;    ///< nearest region is not the centre nation's own
};

/// SYNTHETIC TEST INPUT — where the forced row puts its points under
/// `--forced-pick sparse` (the default). The province cap stops a spend only where
/// a centre's window is WIDER than the provinces it spans and nothing wider can
/// absorb the charters, so the pick reads exactly that geometry and nothing else:
/// among non-razed centres on nation-owned tiles, prefer one whose rung 2 is EMPTY
/// (its nearest region is not its nation's own, so charter_place has only the
/// centre window), then the most nation tiles per distinct province inside the
/// window (radius @p radius, column-wrapped), then more tiles, then the lower id.
/// It never reads population and it is not a density reading — a fixture that
/// makes the `province_cap` branch reachable, chosen for its ground.
forced_pick pick_sparse_province_centre(const world& w, int radius)
{
    forced_pick best;
    std::unordered_map<std::uint64_t, entity_id> at;   // lookups only, never iterated
    at.reserve(w.tiles.size());
    const auto key = [](entity_id body, int x, int y) {
        return (static_cast<std::uint64_t>(body) << 32)
             ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) << 16)
             ^ static_cast<std::uint64_t>(static_cast<std::uint32_t>(x));
    };
    for (const auto& [tid, t] : w.tiles)
        at[key(t.body, t.grid_x, t.grid_y)] = tid;

    std::vector<entity_id> nation_ids;
    for (const auto& kv : w.nations)
        nation_ids.push_back(kv.first);
    std::sort(nation_ids.begin(), nation_ids.end());
    const settlement_state* ss = w.gen_settlement.get();

    std::vector<entity_id> centres;
    for (const auto& [cid, pc] : w.population_centres)
        if (!pc.razed)
            centres.push_back(cid);
    std::sort(centres.begin(), centres.end());

    const long long r2 = static_cast<long long>(radius) * radius;
    for (const entity_id cid : centres)
    {
        const auto ct = w.population_centre_tile.find(cid);
        if (ct == w.population_centre_tile.end())
            continue;
        const auto t = w.tiles.find(ct->second);
        if (t == w.tiles.end())
            continue;
        const auto own = w.tile_to_nation.find(ct->second);
        if (own == w.tile_to_nation.end() || w.nations.count(own->second) == 0)
            continue;
        const entity_id nation = own->second;
        const auto body = w.bodies.find(t->second.body);
        const int gw = (body != w.bodies.end()) ? body->second.grid_width : 0;
        if (gw <= 0)
            continue;

        bool rung2_empty = true;
        if (ss != nullptr && !ss->regions.empty())
        {
            const int pi = nearest_region(*ss, t->second.grid_x, t->second.grid_y, gw);
            if (pi >= 0)
            {
                const int ni = ss->regions[static_cast<std::size_t>(pi)].nation;
                rung2_empty = !(ni >= 0 && ni < static_cast<int>(nation_ids.size())
                                && nation_ids[static_cast<std::size_t>(ni)] == nation);
            }
        }

        int tiles = 0;
        std::vector<std::uint32_t> provs;
        for (int dy = -radius; dy <= radius; ++dy)
            for (int dx = -radius; dx <= radius; ++dx)
            {
                if (static_cast<long long>(dx) * dx + static_cast<long long>(dy) * dy > r2)
                    continue;
                const int x = ((t->second.grid_x + dx) % gw + gw) % gw;
                const auto hit = at.find(key(t->second.body, x, t->second.grid_y + dy));
                if (hit == at.end())
                    continue;
                const auto tn = w.tile_to_nation.find(hit->second);
                if (tn == w.tile_to_nation.end() || tn->second != nation)
                    continue;
                ++tiles;
                const std::uint32_t prov = w.provinces.province_of(hit->second);
                if (prov != 0 && std::find(provs.begin(), provs.end(), prov) == provs.end())
                    provs.push_back(prov);
            }
        const int n_provs = static_cast<int>(provs.size());
        if (n_provs == 0)
            continue;

        bool better = best.centre == null_entity;
        if (!better && rung2_empty != best.rung2_empty)
            better = rung2_empty;
        else if (!better)
        {
            const long long lhs = static_cast<long long>(tiles) * best.window_provinces;
            const long long rhs = static_cast<long long>(best.window_tiles) * n_provs;
            better = lhs > rhs || (lhs == rhs && tiles > best.window_tiles);
        }
        if (better)   // ascending id: a full tie keeps the lower
        {
            best.centre           = cid;
            best.window_tiles     = tiles;
            best.window_provinces = n_provs;
            best.rung2_empty      = rung2_empty;
        }
    }
    return best;
}

int run_charter_cost(const std::vector<uint32_t>& seeds, lua_state& lua, const cost_options& opt)
{
    using clk = std::chrono::steady_clock;
    const auto run0 = clk::now();

    std::printf("player_seed_sweep --charter-cost — BL-1033: what a denser corporate web costs\n");
    std::printf("SYNTHETIC TEST INPUT: every budget is seeded weights over non-razed centres, never "
                "population; 1x = the none row's chartered count. NOT a density-follows-cities reading.\n");
    std::printf("build: %s", build_type_label());
#ifdef _MSC_FULL_VER
    std::printf(", _MSC_FULL_VER %lld", static_cast<long long>(_MSC_FULL_VER));
#endif
    std::printf("; search thread_count 1 (the app's); %d validation ticks; %d live ticks after the "
                "seat, not spectating\n", k_settle_ticks, opt.live_ticks);
    std::printf("TIMINGS ARE WALL CLOCK — only as clean as the machine was quiet.%s%s\n",
                opt.note.empty() ? "" : " note: ", opt.note.c_str());
    std::printf("TIMINGS ARE A LOWER BOUND on the app's step_economy: laps 0-5 only (phase 5 = "
                "standings + convoy credit + firm exits); agency comms, battle dispatches, persona "
                "counsel, the history recorders and the strategy readout are NOT timed, and that gap "
                "widens with density.\n");

    // The matrix line, built from the options and the spend params, never a literal.
    const charter_spend_params base_spend = synthetic_charter_spend();
    const std::int32_t base_price = opt.specialist_prices.front();
    const auto caps_text = [&]() {
        std::string t = " x resource cap";
        for (const charter_cap_rule v : opt.resource_caps)
        {
            if (v != charter_cap_rule::sqrt_capital)
            {
                t += v == charter_cap_rule::fixed ? " on" : " off";
                continue;
            }
            t += " sqrt(c=" + std::to_string(static_cast<int>(base_spend.per_resource_firm_cap))
               + ", ceilings";
            for (const std::int32_t c : opt.density_ceilings)
                t += " " + std::to_string(static_cast<int>(c));
            t += ", guard " + std::to_string(static_cast<int>(base_spend.max_firms_per_body)) + ")";
        }
        t += " x province cap";
        for (const bool v : opt.province_caps)
            t += v ? " on" : " off";
        return t;
    };
    const std::vector<double>& ladder_scales = opt.ladder_all ? opt.scales : opt.ladder_scales;
    std::printf("prices: firm %d point(s); specialist base %d firm charters (%lld points)",
                static_cast<int>(base_spend.firm_price_points), static_cast<int>(base_price),
                static_cast<long long>(base_spend.firm_price_points) * base_price);
    for (std::size_t i = 1; i < opt.specialist_prices.size(); ++i)
        std::printf("; ladder rung %d firm charters (%lld points)",
                    static_cast<int>(opt.specialist_prices[i]),
                    static_cast<long long>(base_spend.firm_price_points) * opt.specialist_prices[i]);
    std::printf("\nmatrix: none");
    if (!opt.forced_only)
    {
        std::printf("; scales");
        for (const double s : opt.scales)
            std::printf(" %gx", s);
        std::printf("%s at sp%d", caps_text().c_str(), static_cast<int>(base_price));
        for (std::size_t i = 1; i < opt.specialist_prices.size(); ++i)
        {
            std::printf("; scales");
            for (const double s : ladder_scales)
                std::printf(" %gx", s);
            std::printf("%s at sp%d", caps_text().c_str(), static_cast<int>(opt.specialist_prices[i]));
        }
        if (opt.extra)
            std::printf("; + 4x rcap off pcap off at sp%d", static_cast<int>(base_price));
    }
    if (opt.forced)
        std::printf("; + forced province cap (1x on one %s centre, radius %d, sp%d, build only)",
                    opt.forced_sparse ? "SPARSE-province" : "richest", opt.forced_radius,
                    static_cast<int>(base_price));
    std::printf("%s\n", opt.forced_only
                    ? " — FORCED ONLY: the none row is build-only and the matrix is skipped" : "");
    std::fflush(stdout);

    std::vector<cost_seed> results;
    bool any_threw = false, forced_failed = false, digest_failed = false, rules_failed = false,
         balance_failed = false;

    for (const uint32_t seed : seeds)
    {
        cost_seed cs;
        cs.seed = seed;
        std::printf("\n=== seed %u — SYNTHETIC TEST INPUT on every budget row (%s) ===\n", seed,
                    build_type_label());
        std::fflush(stdout);
        try
        {

            // THE NONE ROW FIRST: it is the baseline and it measures 1x. The
            // synthetic budgets are built from its world as the landscape lands —
            // the same world the digest mode's `synthetic` builds them from.
            std::vector<double> needed = opt.forced_only ? std::vector<double>{} : opt.scales;
            if (!opt.forced_only && opt.specialist_prices.size() > 1)
                needed.insert(needed.end(), ladder_scales.begin(), ladder_scales.end());
            if (opt.extra && !opt.forced_only) needed.push_back(4.0);
            if (opt.forced) needed.push_back(1.0);
            std::sort(needed.begin(), needed.end());
            needed.erase(std::unique(needed.begin(), needed.end()), needed.end());
            std::map<double, charter_budget> budgets;
            forced_pick pick;
            // A scale whose synthetic total rounds to 0 on THIS seed's 1x: its
            // budget would be empty, i.e. today's world under a budget label.
            std::vector<double> zero_scales;

            cost_row none;
            cost_config none_cfg;
            none_cfg.build_only = opt.forced_only;
            run_cost_config(lua, seed, none_cfg, nullptr, opt.live_ticks, none,
                            [&](const app_start_world& landed) {
                                cs.legacy_specialists = landed.land.specialists.size();
                                cs.legacy_firms       = landed.land.firms.size();
                                cs.legacy_1x = static_cast<long long>(cs.legacy_specialists
                                                                      + cs.legacy_firms);
                                for (const double s : needed)
                                    if (std::llround(static_cast<double>(cs.legacy_1x) * s) <= 0)
                                        zero_scales.push_back(s);
                                if (!zero_scales.empty())
                                    return false;   // refused below, before any row settles
                                for (const double s : needed)
                                    budgets[s] = synthetic_charter_budget(landed.w, seed,
                                                                          cs.legacy_1x, s);
                                if (opt.forced && opt.forced_sparse)
                                    pick = pick_sparse_province_centre(landed.w, opt.forced_radius);
                                return true;
                            });
            if (none.stopped_at_land)
            {
                for (const double s : zero_scales)
                    std::printf("--budget-scales/--ladder-scales: scale %g on seed %u gives a synthetic total of "
                                "llround(%g x 1x %lld) = 0 points — an EMPTY budget, which would "
                                "measure today's world under a budget label. Refused; nothing "
                                "measured on this seed.\n",
                                s, seed, s, cs.legacy_1x);
                std::fflush(stdout);
                if (!opt.out_path.empty())
                {
                    results.push_back(cs);
                    write_cost_json(opt.out_path, opt, results,
                                    "a --budget-scales/--ladder-scales value rounded to an empty "
                                    "budget on this seed; nothing was measured under a budget label",
                                    seed);
                }
                return 2;
            }
            for (const world_digest_pin& pin : k_world_digest_pins)
                if (pin.seed == seed)
                {
                    none.pinned = true;
                    std::string d;
                    if (pin.search != none.dig.search) d += " D_search";
                    if (pin.land   != none.dig.land)   d += " D_land";
                    // A build-only none row (--forced-only) stops at the landscape.
                    if (!none.build_only && pin.settle != none.dig.settle) d += " D_settle";
                    if (!none.build_only && pin.seat   != none.dig.seat)   d += " D_seat";
                    none.digest_match = d.empty();
                    none.digest_diff  = d;
                    if (!d.empty())
                        digest_failed = true;
                }

            std::printf("  1x = %lld legacy corporations (%zu specialists + %zu firms). SYNTHETIC budgets "
                        "(firm %d point(s); specialist ladder",
                        cs.legacy_1x, cs.legacy_specialists, cs.legacy_firms,
                        static_cast<int>(base_spend.firm_price_points));
            for (const std::int32_t sp : opt.specialist_prices)
                std::printf(" %d", static_cast<int>(sp));
            std::printf(" firm charters):");
            for (const auto& [s, bud] : budgets)
            {
                std::int32_t richest = 0;
                for (const auto& kv : bud.points())
                    richest = std::max(richest, kv.second);
                std::printf(" %gx %lld pts over %zu centres (richest %d; afford a specialist:",
                            s, static_cast<long long>(bud.total()), bud.points().size(), richest);
                for (const std::int32_t sp : opt.specialist_prices)
                {
                    const long long price = static_cast<long long>(base_spend.firm_price_points) * sp;
                    int affords = 0;
                    for (const auto& kv : bud.points())
                        if (kv.second >= price)
                            ++affords;
                    std::printf(" sp%d %d", static_cast<int>(sp), affords);
                }
                std::printf(");");
            }
            std::printf("\n");
            print_cost_table_header(opt);
            print_cost_row(none);
            std::fflush(stdout);
            cs.rows.push_back(std::move(none));

            // The matrix, in the order the section note gives: the base-price
            // cross, each ladder rung at its scales, the extra row, then forced.
            std::vector<cost_config> matrix;
            const auto add_cross = [&](const std::vector<double>& scales, std::int32_t price) {
                for (const double s : scales)
                    for (const charter_cap_rule rc : opt.resource_caps)
                    {
                        // `sqrt` runs once per ceiling; a legacy rule carries none.
                        const std::vector<std::int32_t> ceilings =
                            rc == charter_cap_rule::sqrt_capital ? opt.density_ceilings
                                                                 : std::vector<std::int32_t>{ 0 };
                        for (const std::int32_t ceiling : ceilings)
                            for (const bool pc : opt.province_caps)
                            {
                                cost_config c;
                                c.k                 = cost_config::kind::synthetic;
                                c.scale             = s;
                                c.resource_cap_rule = rc;
                                c.density_ceiling   = ceiling;
                                c.province_cap      = pc;
                                c.specialist_firm_charters = price;
                                matrix.push_back(c);
                            }
                    }
            };
            if (!opt.forced_only)
            {
                add_cross(opt.scales, base_price);
                for (std::size_t i = 1; i < opt.specialist_prices.size(); ++i)
                    add_cross(ladder_scales, opt.specialist_prices[i]);
            }
            if (opt.extra && !opt.forced_only)
            {
                const bool present = std::any_of(matrix.begin(), matrix.end(), [&](const cost_config& c) {
                    return c.scale == 4.0 && c.resource_cap_rule == charter_cap_rule::lifted
                        && !c.province_cap && c.specialist_firm_charters == base_price;
                });
                if (!present)
                {
                    cost_config c;
                    c.k                 = cost_config::kind::synthetic;
                    c.scale             = 4.0;
                    c.resource_cap_rule = charter_cap_rule::lifted;
                    c.province_cap      = false;
                    c.specialist_firm_charters = base_price;
                    matrix.push_back(c);
                }
            }
            for (const cost_config& c : matrix)
            {
                cost_row row;
                try
                {
                    run_cost_config(lua, seed, c, &budgets.at(c.scale), opt.live_ticks, row);
                    if (!row.rule_check.pass)
                        rules_failed = true;
                    if (row.budget_row && !row.stopped_at_land && !row.balanced)
                        balance_failed = true;
                }
                catch (const std::exception& e)
                {
                    row.cfg   = c;
                    row.label = cost_config_label(c);
                    row.threw = true;
                    row.error = e.what();
                    any_threw = true;
                }
                print_cost_row(row);
                std::fflush(stdout);
                cs.rows.push_back(std::move(row));
            }

            if (opt.forced)
            {
                cost_config c;
                c.k             = cost_config::kind::forced;
                c.scale         = 1.0;
                c.window_radius = opt.forced_radius;
                c.resource_cap_rule = charter_cap_rule::fixed;
                c.province_cap  = true;
                c.specialist_firm_charters = base_price;
                c.legacy_specialists = cs.legacy_specialists;
                c.legacy_firms       = cs.legacy_firms;
                charter_budget concentrated = concentrated_charter_budget(budgets.at(1.0));
                if (opt.forced_sparse)
                {
                    std::map<entity_id, std::int32_t> one;
                    if (pick.centre != null_entity)
                        one[pick.centre] = static_cast<std::int32_t>(budgets.at(1.0).total());
                    concentrated = charter_budget(one);
                    std::printf("      forced pick SPARSE (SYNTHETIC TEST INPUT): centre %u — rung 2 %s; "
                                "%d nation tiles over %d provinces inside radius %d\n",
                                pick.centre, pick.rung2_empty ? "EMPTY" : "not empty",
                                pick.window_tiles, pick.window_provinces, opt.forced_radius);
                }
                else
                {
                    pick.sparse = false;
                    pick.centre = concentrated.points().empty() ? null_entity
                                                                : concentrated.points().begin()->first;
                    std::printf("      forced pick RICHEST (SYNTHETIC TEST INPUT): centre %u, the 1x "
                                "synthetic budget's richest\n", pick.centre);
                }
                c.forced_pick_sparse = pick.sparse;
                c.forced_centre      = pick.centre;
                cost_row row;
                try
                {
                    run_cost_config(lua, seed, c, &concentrated, opt.live_ticks, row);
                    if (!row.forced_pass)
                        forced_failed = true;
                    if (!row.rule_check.pass)
                        rules_failed = true;
                    if (row.budget_row && !row.stopped_at_land && !row.balanced)
                        balance_failed = true;
                }
                catch (const std::exception& e)
                {
                    row.cfg   = c;
                    row.label = cost_config_label(c);
                    row.threw = true;
                    row.error = e.what();
                    any_threw = true;
                    forced_failed = true;
                }
                print_cost_row(row);
                std::fflush(stdout);
                cs.rows.push_back(std::move(row));
            }
        }
        catch (const std::exception& e)
        {
            cs.threw  = true;
            cs.error  = e.what();
            any_threw = true;
            std::printf("  seed %u THREW: %s\n", seed, e.what());
        }
        catch (...)
        {
            cs.threw  = true;
            cs.error  = "unknown";
            any_threw = true;
            std::printf("  seed %u THREW: unknown\n", seed);
        }
        results.push_back(std::move(cs));
        if (!opt.out_path.empty())
            write_cost_json(opt.out_path, opt, results);   // after every seed: a crash keeps the rest
        std::fflush(stdout);
    }

    const double total_s = std::chrono::duration<double>(clk::now() - run0).count();
    const std::string written = opt.out_path.empty() ? std::string("No --out: nothing written.")
                                                     : "JSON: " + opt.out_path;
    std::printf("\n%zu seeds in %.0f s (%.0f s per seed). %s\n", seeds.size(), total_s,
                seeds.empty() ? 0.0 : total_s / static_cast<double>(seeds.size()), written.c_str());
    std::printf("none-row digests: %s. forced province cap: %s. spend rules (BL-1039, checked at "
                "land on every budget row): %s. points balance on every budget row: %s. %s\n",
                digest_failed ? "DIFFER from the pins on some seed (this mode is NOT measuring the shipped start)"
                              : "match wherever a pin exists",
                !opt.forced ? "not run" : forced_failed ? "FAIL on some seed" : "PASS on every seed",
                rules_failed ? "FAIL on some row" : "PASS on every row",
                balance_failed ? "FAIL — UNBALANCED on some row" : "PASS",
                any_threw ? "SOMETHING THREW." : "nothing threw.");
    return (any_threw || forced_failed || digest_failed || rules_failed || balance_failed) ? 1 : 0;
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
    const bool digest_mode = (argc > 1 && std::string(argv[1]) == "--digest");
    const bool check_mode  = (argc > 1 && std::string(argv[1]) == "--digest-check");
    const bool cost_mode   = (argc > 1 && std::string(argv[1]) == "--charter-cost");
    const bool mode_arg = roster_mode || guard_mode || seat_mode || digest_mode || check_mode
                       || cost_mode;
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
                    "       %s --guard [seed_count] [--fast] (assert what the seat holds)\n"
                    "       %s --digest       [--seeds a,b,c]  (print the BL-1031 world digests)\n"
                    "       %s --digest-check [--seeds a,b,c]  (fail on any row differing from the pin)\n"
                    "           both digest modes: [--charter-budget none|empty|zero|synthetic|refused|stockpile] [--charter-scale X]\n"
                    "       %s --charter-cost [--seeds a,b,c] [--budget-scales 1,2,4] [--resource-cap on|off|both]\n"
                    "                         [--province-cap on|off|both] [--specialist-prices 4,8]\n"
                    "                         [--ladder-scales 2|all] [--no-extra] [--no-forced]\n"
                    "                         [--forced-only] [--forced-radius N] [--forced-pick sparse|richest]\n"
                    "                         [--live-ticks N] [--out file.json] [--note TEXT]   (BL-1033)\n",
                    argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0]);
        return 2;
    }

    // `--seeds a,b,c`, shared by the seat and digest modes.
    const auto seeds_arg = [argc, argv]() {
        std::vector<uint32_t> seeds;
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
                    {
                        // A token must be a whole number: strtoul would turn "4l"
                        // into 4 and "x" into seed 0, a pinned row (cold review).
                        const std::string tok = csv.substr(at, comma - at);
                        if (tok.find_first_not_of("0123456789") != std::string::npos)
                        {
                            std::printf("--seeds: '%s' is not a seed number\n", tok.c_str());
                            std::exit(2);
                        }
                        seeds.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
                    }
                    at = comma + 1;
                }
            }
        return seeds;
    };

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

    // BL-1031 digest modes. The pins name the SHIPPED start only, so `--fast`
    // is refused rather than silently compared against a different world. With
    // no `--seeds`, both modes walk the pinned seeds in table order.
    if (digest_mode || check_mode)
    {
        if (fast)
        {
            std::printf("--fast is refused in the digest modes: the pins are the shipped spawn's\n");
            return 2;
        }
        std::vector<uint32_t> seeds = seeds_arg();
        if (seeds.empty())
            for (const world_digest_pin& p : k_world_digest_pins)
                seeds.push_back(p.seed);
        if (seeds.empty())
        {
            std::printf("no seeds: pass --seeds a,b,c (the pin table is empty)\n");
            return 2;
        }
        // BL-1032: `--charter-budget none|empty|zero|synthetic|refused|stockpile`, `--charter-scale X`.
        charter_mode mode  = charter_mode::none;
        double       scale = 1.0;
        bool         rcap  = true;
        for (int a = 2; a < argc; ++a)
        {
            const std::string arg = argv[a];
            if (arg == "--resource-cap")
            {
                if (a + 1 >= argc || (std::string(argv[a + 1]) != "on"
                                      && std::string(argv[a + 1]) != "off"))
                {
                    std::printf("--resource-cap: needs on|off in the digest modes\n");
                    return 2;
                }
                rcap = std::string(argv[++a]) == "on";
                continue;
            }
            if (arg == "--charter-budget" || arg == "--charter-scale")
            {
                if (a + 1 >= argc)
                {
                    std::printf("%s needs a value\n", arg.c_str());
                    return 2;
                }
                const std::string val = argv[++a];
                if (arg == "--charter-budget")
                {
                    if      (val == "none")      mode = charter_mode::none;
                    else if (val == "empty")     mode = charter_mode::empty;
                    else if (val == "zero")      mode = charter_mode::zero;
                    else if (val == "synthetic") mode = charter_mode::synthetic;
                    else if (val == "refused")   mode = charter_mode::refused;
                    else if (val == "stockpile") mode = charter_mode::stockpile;
                    else
                    {
                        std::printf("--charter-budget: '%s' is not none|empty|zero|synthetic|refused|stockpile\n",
                                    val.c_str());
                        return 2;
                    }
                }
                else
                {
                    char* end = nullptr;
                    scale = std::strtod(val.c_str(), &end);
                    if (end == val.c_str() || *end != '\0' || !(scale > 0.0) || scale > 1000.0)
                    {
                        std::printf("--charter-scale: '%s' is not a number in (0, 1000]\n", val.c_str());
                        return 2;
                    }
                }
            }
        }
        return run_digest(seeds, lua, check_mode, mode, scale, rcap);
    }

    // BL-1033 cost mode. The shipped spawn only, like the digest modes, and for
    // the same reason: its none row is checked against the pins.
    if (cost_mode)
    {
        if (fast)
        {
            std::printf("--fast is refused in --charter-cost: its none row is checked against the "
                        "shipped spawn's pins\n");
            return 2;
        }
        cost_options opt;
        for (int a = 0; a < argc; ++a)
            opt.argv.emplace_back(argv[a]);
        std::vector<uint32_t> seeds = seeds_arg();
        if (seeds.empty())
            seeds = { 0u, 28u, 46u };
        const auto on_off = [](const std::string& v, std::vector<bool>& out) {
            if (v == "on")        out = { true };
            else if (v == "off")  out = { false };
            else if (v == "both") out = { true, false };
            else                  return false;
            return true;
        };
        for (int a = 2; a < argc; ++a)
        {
            const std::string arg = argv[a];
            if (arg == "--no-extra")  { opt.extra = false;  continue; }
            if (arg == "--no-forced") { opt.forced = false; continue; }
            if (arg == "--forced-only") { opt.forced_only = true; opt.forced = true; continue; }
            if (arg == "--seeds")     { ++a; continue; }   // read by seeds_arg
            if (arg != "--budget-scales" && arg != "--resource-cap" && arg != "--province-cap"
                && arg != "--live-ticks" && arg != "--out" && arg != "--note"
                && arg != "--forced-radius" && arg != "--forced-pick"
                && arg != "--specialist-prices" && arg != "--ladder-scales"
                && arg != "--density-ceilings")
            {
                std::printf("--charter-cost: unknown argument '%s'\n", arg.c_str());
                return 2;
            }
            if (a + 1 >= argc)
            {
                std::printf("%s needs a value\n", arg.c_str());
                return 2;
            }
            const std::string val = argv[++a];
            // A comma list of scales in (0, 1000]. A scale whose synthetic total
            // rounds to 0 on a seed is refused once that seed's 1x is measured.
            const auto parse_scales = [&](std::vector<double>& out) {
                out.clear();
                std::size_t at = 0;
                while (at <= val.size())
                {
                    std::size_t comma = val.find(',', at);
                    if (comma == std::string::npos)
                        comma = val.size();
                    const std::string tok = val.substr(at, comma - at);
                    char* end = nullptr;
                    const double x = std::strtod(tok.c_str(), &end);
                    if (tok.empty() || end == tok.c_str() || *end != '\0' || !(x > 0.0) || x > 1000.0)
                    {
                        std::printf("%s: '%s' is not a number in (0, 1000]\n", arg.c_str(), tok.c_str());
                        return false;
                    }
                    out.push_back(x);
                    at = comma + 1;
                }
                return true;
            };
            if (arg == "--budget-scales")
            {
                if (!parse_scales(opt.scales))
                    return 2;
            }
            else if (arg == "--ladder-scales")
            {
                if (val == "all")
                    opt.ladder_all = true;
                else
                {
                    opt.ladder_all = false;
                    if (!parse_scales(opt.ladder_scales))
                        return 2;
                }
            }
            else if (arg == "--specialist-prices")
            {
                // Firm charters per specialist, each a whole number > 0; the first
                // is the base price, the rest ladder rungs. No duplicates.
                opt.specialist_prices.clear();
                std::size_t at = 0;
                while (at <= val.size())
                {
                    std::size_t comma = val.find(',', at);
                    if (comma == std::string::npos)
                        comma = val.size();
                    const std::string tok = val.substr(at, comma - at);
                    if (tok.empty() || tok.size() > 4
                        || tok.find_first_not_of("0123456789") != std::string::npos
                        || std::atoi(tok.c_str()) <= 0)
                    {
                        std::printf("--specialist-prices: '%s' is not a whole number of firm charters "
                                    "in [1, 9999] (each price must be > 0)\n", tok.c_str());
                        return 2;
                    }
                    const std::int32_t p = static_cast<std::int32_t>(std::atoi(tok.c_str()));
                    if (std::find(opt.specialist_prices.begin(), opt.specialist_prices.end(), p)
                        != opt.specialist_prices.end())
                    {
                        std::printf("--specialist-prices: %d is listed twice\n", static_cast<int>(p));
                        return 2;
                    }
                    opt.specialist_prices.push_back(p);
                    at = comma + 1;
                }
            }
            else if (arg == "--province-cap")
            {
                if (!on_off(val, opt.province_caps))
                {
                    std::printf("%s: '%s' is not on|off|both\n", arg.c_str(), val.c_str());
                    return 2;
                }
            }
            else if (arg == "--resource-cap")
            {
                // BL-1039: on (fixed) | off (lifted) | both | sqrt, or a comma list
                // of on/off/sqrt. `sqrt` needs --density-ceilings (no default).
                opt.resource_caps.clear();
                std::size_t at = 0;
                while (at <= val.size())
                {
                    std::size_t comma = val.find(',', at);
                    if (comma == std::string::npos)
                        comma = val.size();
                    const std::string tok = val.substr(at, comma - at);
                    std::vector<charter_cap_rule> add;
                    if (tok == "on")        add = { charter_cap_rule::fixed };
                    else if (tok == "off")  add = { charter_cap_rule::lifted };
                    else if (tok == "both") add = { charter_cap_rule::fixed, charter_cap_rule::lifted };
                    else if (tok == "sqrt") add = { charter_cap_rule::sqrt_capital };
                    else
                    {
                        std::printf("--resource-cap: '%s' is not on|off|both|sqrt\n", tok.c_str());
                        return 2;
                    }
                    for (const charter_cap_rule r : add)
                    {
                        if (std::find(opt.resource_caps.begin(), opt.resource_caps.end(), r)
                            != opt.resource_caps.end())
                        {
                            std::printf("--resource-cap: '%s' is listed twice\n",
                                        charter_cap_rule_name(r));
                            return 2;
                        }
                        opt.resource_caps.push_back(r);
                    }
                    at = comma + 1;
                }
            }
            else if (arg == "--density-ceilings")
            {
                // BL-1039: firms per body under the square root, each a whole
                // number in [1, guard - 1] — the ceiling sits below the guard.
                const int guard = static_cast<int>(synthetic_charter_spend().max_firms_per_body);
                opt.density_ceilings.clear();
                std::size_t at = 0;
                while (at <= val.size())
                {
                    std::size_t comma = val.find(',', at);
                    if (comma == std::string::npos)
                        comma = val.size();
                    const std::string tok = val.substr(at, comma - at);
                    const int n = std::atoi(tok.c_str());
                    if (tok.empty() || tok.size() > 6
                        || tok.find_first_not_of("0123456789") != std::string::npos
                        || n <= 0 || n >= guard)
                    {
                        std::printf("--density-ceilings: '%s' is not a whole number in [1, %d] (below "
                                    "the %d-per-body guard)\n", tok.c_str(), guard - 1, guard);
                        return 2;
                    }
                    if (std::find(opt.density_ceilings.begin(), opt.density_ceilings.end(), n)
                        != opt.density_ceilings.end())
                    {
                        std::printf("--density-ceilings: %d is listed twice\n", n);
                        return 2;
                    }
                    opt.density_ceilings.push_back(static_cast<std::int32_t>(n));
                    at = comma + 1;
                }
            }
            else if (arg == "--live-ticks")
            {
                if (val.empty() || val.find_first_not_of("0123456789") != std::string::npos
                    || std::atoi(val.c_str()) <= 0 || std::atoi(val.c_str()) > 400)
                {
                    std::printf("--live-ticks: '%s' is not a whole number in [1, 400]\n", val.c_str());
                    return 2;
                }
                opt.live_ticks = std::atoi(val.c_str());
            }
            else if (arg == "--forced-pick")
            {
                if (val == "sparse")       opt.forced_sparse = true;
                else if (val == "richest") opt.forced_sparse = false;
                else
                {
                    std::printf("--forced-pick: '%s' is not sparse|richest\n", val.c_str());
                    return 2;
                }
            }
            else if (arg == "--forced-radius")
            {
                if (val.empty() || val.find_first_not_of("0123456789") != std::string::npos
                    || std::atoi(val.c_str()) > 64)
                {
                    std::printf("--forced-radius: '%s' is not a whole number in [0, 64]\n", val.c_str());
                    return 2;
                }
                opt.forced_radius = std::atoi(val.c_str());
            }
            else if (arg == "--out")
                opt.out_path = val;
            else
                opt.note = val;
        }
        // BL-1039: the ceiling has no default, and a ceiling nothing reads is refused.
        const bool has_sqrt = std::find(opt.resource_caps.begin(), opt.resource_caps.end(),
                                        charter_cap_rule::sqrt_capital) != opt.resource_caps.end();
        if (has_sqrt && opt.density_ceilings.empty())
        {
            std::printf("--resource-cap sqrt needs --density-ceilings N[,M...]: the density ceiling "
                        "has no default\n");
            return 2;
        }
        if (!has_sqrt && !opt.density_ceilings.empty())
        {
            std::printf("--density-ceilings is read only by --resource-cap sqrt\n");
            return 2;
        }
        return run_charter_cost(seeds, lua, opt);
    }

    // Same placement, same reason: a seat sweep against an empty registry would
    // measure a world where nothing can be processed, and every corp would fail
    // the viability floor for a reason that is not the world's.
    if (seat_mode || guard_mode)
    {
        // `--seeds a,b,c` names the worlds outright — the curated library
        // (`node tools/session/seed_library.js`) is sixteen chosen seeds, not
        // 0..15. Otherwise the positional count sweeps 0..n-1 as it always has.
        std::vector<uint32_t> seeds = seeds_arg();
        int reproduce = k_reproduce_seeds;
        for (int a = 2; a + 1 < argc; ++a)
            if (std::string(argv[a]) == "--reproduce")
                reproduce = std::max(1, std::atoi(argv[a + 1]));
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

    std::printf("player_seed_sweep — %d seeds, %d settle ticks (%.2f in-game years)\n",
                n_seeds, settle_ticks, settle_ticks / 4.0);
    std::printf("NOT THE SHIPPED START: built bare (no world_gen.lua, works.lua or set_era, no landscape "
                "search, its own settle); --seat/--guard/--digest build the app's world.\n\n");
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
