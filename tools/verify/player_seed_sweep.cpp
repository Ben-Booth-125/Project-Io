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
//      ... --digest / --digest-check [--charter-budget none|empty|zero|synthetic|refused]
//                                    [--charter-scale X]                    (BL-1032)
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
#include "world/world_save.hpp"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
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
// refuses anything that is not a number or an enum).
//
// A PIN IS A CONTRACT (src/world/CLAUDE.md). A failing row is a finding to report
// with its cause; it is never re-pinned by the change that moved it.

struct fnv1a64
{
    std::uint64_t h = 0xCBF29CE484222325ull;

    void bytes(const void* p, std::size_t n)
    {
        const auto* b = static_cast<const unsigned char*>(p);
        for (std::size_t i = 0; i < n; ++i)
        {
            h ^= b[i];
            h *= 0x00000100000001B3ull;
        }
    }
    template <class T>
    void scalar(T v)
    {
        static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>,
                      "hash a record field by field, never as a block");
        bytes(&v, sizeof v);
    }
    void flag(bool v) { scalar(static_cast<std::uint8_t>(v ? 1u : 0u)); }
    void count(std::size_t n) { scalar(static_cast<std::uint64_t>(n)); }
};

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

/// The world's flat-binary save bytes, into @p f. Returns the byte count, so a
/// digest row can show it hashed megabytes rather than an empty stream.
std::size_t hash_snapshot(fnv1a64& f, const world& w)
{
    std::ostringstream os(std::ios::out | std::ios::binary);
    write_world_snapshot(w, os);
    const std::string bytes = os.str();
    f.bytes(bytes.data(), bytes.size());
    return bytes.size();
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

/// Build one world, settle it, seat it — the app's campaign start end to end.
/// Returns the seat result; @p out holds the world (and the registry, config
/// and search it was built from), because the caller needs both to describe
/// what was seated. With @p dig, the four BL-1031 digests are taken at the
/// seams between the phases; without it the path is exactly the seat sweep's.
spawn_seat_result build_and_seat(lua_state& lua, uint32_t seed, bool fast,
                                 app_start_world& out, world_digests* dig = nullptr,
                                 const harness_charter_input& charter = {})
{
    world_params p = fast ? no_prehistory() : world_params{};
    p.seed = seed;
    // app::begin_new_game + app::start_new_game_prelude: config and works, the
    // world, setup_world's writes, load_economy with its era band, the
    // landscape-search WINNER (not the seed candidate — BL-979) and the second
    // recipe pass (BL-1030; harness_params.hpp). `charter` is none unless a
    // digest run asked for a budget (BL-1032); none is the app's own call.
    build_app_start_world(lua, p, out, charter);
    if (dig != nullptr)
    {
        dig->search = digest_search(out.land.search);
        fnv1a64 f;
        dig->land_bytes = hash_snapshot(f, out.w);
        dig->land = f.h;
    }
    // app::poll_worldgen's validation run (BL-1030; harness_params.hpp).
    run_app_validation_settle(out.w, out.reg);
    if (dig != nullptr)
    {
        fnv1a64 f;
        dig->settle_bytes = hash_snapshot(f, out.w);
        f.scalar(out.w.state_hash(out.w.current_day_tick));
        dig->settle = f.h;
    }
    // app::seat_player (app.cpp:933): the seat on the WINNER'S STATIC SCORE
    // (BL-1020) — the one the app keeps as `m_landscape_winner_score`.
    spawn_seat_result res = seat_player_corporation(out.w, p.seed, out.land.search.winner_score);
    if (dig != nullptr)
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
        dig->seat_bytes = hash_snapshot(f, out.w);
        dig->seat = f.h;
    }
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
//   none       no budget at all — the shipped path (R1).
//   empty      an empty budget reaches the seam (R2).
//   zero       a budget of zero entries reaches the seam; the type drops them, so
//              it is the empty budget by construction (R2).
//   synthetic  SYNTHETIC TEST INPUT (harness_params.hpp): scale x the legacy
//              corporation count, measured on a legacy build of the same seed in
//              this process, over the non-razed centres by seeded weights (R4 —
//              the non-vacuity reading; its digests must NOT match the pins).
//   refused    the SAME synthetic budget — non-empty, and one that moves the
//              world when priced (see `synthetic`) — with the firm price ZEROED,
//              so `charter_spend_refusal` refuses it. A refusal mutates nothing
//              beyond today's world, so its digests MUST equal the pins, and the
//              search and the apply must both report the refusal.
// empty and zero pass the DEFAULT spend, whose prices are 0: a non-empty budget
// with those prices would be refused, so a PASS also shows the refusal never
// reads an empty budget.

enum class charter_mode { none, empty, zero, synthetic, refused };

const char* charter_mode_name(charter_mode m)
{
    switch (m)
    {
    case charter_mode::none:      return "none";
    case charter_mode::empty:     return "empty";
    case charter_mode::zero:      return "zero";
    case charter_mode::synthetic: return "synthetic";
    case charter_mode::refused:   return "refused";
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
    std::printf("      charter budget %s — SYNTHETIC TEST INPUT (seeded weights, never population): "
                "scale %.2f x 1x %zu legacy corporations (%zu specialists + %zu firms) = %lld points "
                "over %zu centres; firm price %d, specialist %d firm charters (= %lld points), "
                "window radius %d, province cap %s\n",
                charter_mode_name(mode), scale, legacy_specialists + legacy_firms, legacy_specialists,
                legacy_firms, static_cast<long long>(budget.total()), budget.points().size(),
                spend.firm_price_points, spend.specialist_firm_charters,
                static_cast<long long>(spend.specialist_price_points()), spend.window_radius,
                spend.province_cap ? "on" : "off");
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
    struct spill
    {
        int anchor_in = 0, anchor_out = 0, second_in = 0, second_out = 0, unresolved = 0;
        long long far_d2 = 0;
    };
    spill spec_sp, firm_sp;
    for (const charter_record& r : rep.charters)
    {
        spill& s = r.specialist ? spec_sp : firm_sp;
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
    std::printf("      points: budgeted %lld = spent %lld + unspent %lld [%s]; unspent by reason:",
                static_cast<long long>(rep.points_budgeted), static_cast<long long>(rep.points_spent),
                static_cast<long long>(rep.points_unspent),
                rep.points_budgeted == rep.points_spent + rep.points_unspent ? "balanced" : "UNBALANCED");
    for (int r = 0; r < charter_unspent_reason_count; ++r)
        std::printf(" %s %lld (%d centres)%s",
                    charter_unspent_reason_name(static_cast<charter_unspent_reason>(r)),
                    pts[static_cast<std::size_t>(r)], ctr[static_cast<std::size_t>(r)],
                    r + 1 < charter_unspent_reason_count ? "," : "\n");
}

int run_digest(const std::vector<uint32_t>& seeds, lua_state& lua, bool check,
               charter_mode mode = charter_mode::none, double charter_scale = 1.0)
{
    std::printf("player_seed_sweep %s — %zu seeds, the shipped spawn built, settled (%d ticks) "
                "and seated in app order (BL-1030)\n",
                check ? "--digest-check" : "--digest", seeds.size(), k_settle_ticks);
    std::printf("BL-1032. --charter-budget %s%s\n", charter_mode_name(mode),
                mode == charter_mode::none ? " (the shipped path: no budget reaches the search)"
                : mode == charter_mode::synthetic
                    ? " — SYNTHETIC TEST INPUT; its digests are EXPECTED to differ from the pins"
                : mode == charter_mode::refused
                    ? " — the synthetic budget with a ZERO firm price: REFUSED, so the digests "
                      "must equal the pins"
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

    int failed = 0, threw = 0, passed = 0;
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
                // `refused`: the same budget, and a price with no default left
                // at its default — the refusal a caller that forgets a price gets.
                if (mode == charter_mode::refused)
                    charter.spend.firm_price_points = 0;
            }
            if (mode != charter_mode::none)
            {
                charter.budget = &budget;
                charter.report = &report;
            }
            start = std::make_unique<app_start_world>();
            build_and_seat(lua, seed, /*fast=*/false, *start, &got[i], charter);
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
            std::printf("\n");
        }
        if (mode != charter_mode::none)
            print_charter_report(start->w, mode, budget, charter.spend, report, start->land.search,
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
        return threw ? 1 : 0;
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
    const bool mode_arg = roster_mode || guard_mode || seat_mode || digest_mode || check_mode;
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
                    "           both digest modes: [--charter-budget none|empty|zero|synthetic|refused] [--charter-scale X]\n",
                    argv[0], argv[0], argv[0], argv[0], argv[0], argv[0]);
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
        // BL-1032: `--charter-budget none|empty|zero|synthetic|refused`, `--charter-scale X`.
        charter_mode mode  = charter_mode::none;
        double       scale = 1.0;
        for (int a = 2; a < argc; ++a)
        {
            const std::string arg = argv[a];
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
                    else
                    {
                        std::printf("--charter-budget: '%s' is not none|empty|zero|synthetic|refused\n",
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
        return run_digest(seeds, lua, check_mode, mode, scale);
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
