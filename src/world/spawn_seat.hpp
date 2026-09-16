#pragma once

#include "landscape_score.hpp"
#include "world.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// The spawn shortlist, and the seat (BL-630)
// ---------------------------------------------------------------------------
//
// Which corporation the player runs is DRAWN, not picked (Ben, 2026-08-26).
// Authority: docs/generation/CORPORATION_GENERATION.md § The spawn shortlist,
// and the seat; docs/ui/STARTUP.md § The seat.
//
// The sequence this header's one function closes:
//
//   1. Generate — Passes 1-6, no corporation is the player's yet.
//   2. Search and settle — phase 6 scores candidate landscapes STATICALLY and
//      applies the winner, then runs its single validation run in spectate
//      (`corp_ai_params::spectating`, nobody seated). BL-409 settled that under
//      spectate the no-auto-act prohibition has no SUBJECT rather than an
//      exception: every corp evaluates on the same staggered cadence, and the
//      cadence index is over the SORTED corp set, so admitting one more shifts
//      no rival's slot.
//   3. Shortlist — every SPECIALIST whose ground clears the viability floor on
//      the STATIC LANDSCAPE SCORE phase 6 computed (`seat_landscape_score`).
//   4. Seat — one is drawn from the shortlist against the world seed, and
//      `is_player` / `world::player_entity` are re-pointed onto it.
//
// THE FLOOR READS THE GROUND, NOT A TRADING RECORD (Ben, 2026-09-16, NR-881;
// BL-1020, the seat floor reads the static score). It used to read solvency
// plus trailing net over eight filed quarters. The settle is a twelve-tick
// validation run over a field that is still ramping, so no trading record can
// exist at any window, and a Release run seated "shortlist 0 of 8 — VIABILITY
// FLOOR UNMET". What IS knowable before the first convoy runs is what phase 6
// already scored: how well the market a holding stands in closes chains and
// balances supply against demand. The trailing figures are still read and kept
// on every candidate — as information for the seat card, never as the gate.
//
// WHAT THIS DELIBERATELY DOES NOT TOUCH. The generator's own uniform draw
// (corporation_generation.cpp § Player corporation flag) still runs and still
// flags a corp. It is now a PROVISIONAL pick that this function overwrites, and
// leaving it in place is what keeps the generation RNG stream — and therefore
// every generation golden and every headless harness that never seats — bit-
// identical. Seating RE-POINTS an existing decision rather than replacing the
// mechanism that made it.
//
// DETERMINISM. The draw consumes the WORLD SEED through a local `std::mt19937`;
// it reads no wall clock, no global state, and walks only sorted vectors (the
// candidate order is a total order: static score, then entity id). One seed
// reproduces one seat, on every machine, every run.

/// How many filed quarters the seat card's trailing-net figure reads.
///
/// EIGHT — two years of the forty `k_quarterly_return_retention` keeps. It was
/// the viability floor's window until BL-1020 moved the floor onto the static
/// score; the figure is still read, and still DISPLAYED, because a player
/// weighing a seat wants to see what its books have done — it just never
/// decides whether the seat is offered. Its own constant rather than a reuse of
/// `k_acquisition_trailing_quarters`, which holds the same value today: that one
/// is the whole-firm acquisition price's statement of how much history a PRICE
/// is entitled to read. They answer different questions and must be free to
/// move apart.
inline constexpr std::size_t k_spawn_trailing_quarters = 8;

/// Tunables for the weighted draw. All three are a FIRST CUT (Ben, 2026-08-26:
/// "mostly random for now, targeted towards population centres and processing,
/// rather than extraction") — what matters more than their values is that
/// `player_seed_sweep --seat` REPORTS the distribution they produce rather than
/// asserting it against a target.
struct spawn_seat_params
{
    /// Added to a shortlisted corp's weight when its holdings include at least
    /// one `processing_facility`. The DEPTH lever, and the reason the draw is
    /// weighted rather than flat: the retired selection screen existed because a
    /// pure draw handed the player a pure-extraction corp on 13 of 24 seeds, and
    /// a viability floor would never reject one (a shallow corp is usually
    /// perfectly profitable). Depth, NOT wealth — BL-436 measured a processing
    /// facility earning LESS per tick than the extraction site it replaces.
    float processor_bonus = 2.0f;

    /// Multiplied by the SHARE of holdings sitting on or near populated ground,
    /// and added. Graded rather than binary because "near" is a fuzzy fact and a
    /// corp with one holding by a city is not the same opening as one with all
    /// six there. Labour, demand and market access are all where the people are.
    float population_bonus = 2.0f;

    /// Grid radius counted as "near" a population centre. Wrapped squared
    /// grid distance — the codebase's standard proximity measure (see
    /// placement_rules.cpp § centre_within_radius, whose metric this mirrors).
    int population_radius = 4;
};

/// One specialist's spawn record — every number the floor, the order and the
/// draw read, kept whether or not the corp cleared, so a sweep can say WHY and
/// a seat card can show what the player is weighing.
struct spawn_seat_candidate
{
    entity_id corp = null_entity;

    // --- THE GATE AND THE ORDER --------------------------------------------

    /// The seat's STATIC LANDSCAPE SCORE — `seat_landscape_score`: the phase-6
    /// per-market viability (`actual * balance`) of the market each holding
    /// stands in, averaged over the corp's sited holdings. In [0, 1].
    double landscape = 0.0;
    /// Sited holdings that stand in a market the landscape score carries at all.
    /// A holding outside every scored catchment reads as zero viability.
    int    holdings_scored = 0;

    /// Cleared the floor: `landscape > 0`. At least one holding stands in a
    /// market where this roster closes a chain AND some resource sits inside the
    /// pin band — the same necessary condition phase 6's composite applies to a
    /// whole landscape ("a landscape where nothing works is not rescued").
    bool   shortlisted = false;

    // --- INFORMATION for the seat card; NEVER the gate ---------------------

    /// Closing `corporation_component::balance` at the end of the settle.
    float balance = 0.0f;
    /// True when `balance > 0`. Recorded, not gated on.
    bool  solvent = false;

    /// Sum of `net` over the last `k_spawn_trailing_quarters` filed returns
    /// (or over every filed return, when the corp has filed fewer).
    float trailing_net = 0.0f;
    /// How many returns that sum actually covered.
    int   quarters_read = 0;

    // --- the two weights' inputs, recorded for the sweep ---
    bool  has_processor    = false;
    int   holdings         = 0;
    int   holdings_near_pop = 0;
    /// `holdings_near_pop / holdings`, or 0 when the corp holds nothing sited.
    float population_share = 0.0f;

    /// The draw weight. Zero for a corp that did not clear the floor; otherwise
    /// at least 1.0 — the bias never zeroes anybody out.
    float weight = 0.0f;
};

/// What the seat decided, and everything it read to decide it. Returned rather
/// than written into `world`: none of it is simulation state, so it stays OFF
/// the flat-binary serialisation seam, exactly as `generation_report` does.
struct spawn_seat_result
{
    /// The corporation now flagged `is_player`, equal to `w.player_entity`.
    /// `null_entity` only when the world holds no specialist at all.
    entity_id seated = null_entity;

    /// THE FLOOR WENT UNMET — no specialist stands on ground with a non-zero
    /// static score, so the first candidate in rank order (every score zero, so
    /// the lowest entity id) was seated instead. A viability signal to be READ,
    /// not a failure to be hidden, and never a licence to conjure or patch a corp
    /// to make the shortlist non-empty (the position § Pass 2's diversity floor
    /// takes). False on an ordinary world.
    bool floor_unmet = false;

    int specialist_count = 0;
    int shortlist_size   = 0;

    /// Every specialist, RANKED: static landscape score descending, entity id
    /// ascending on a tie. The order the shortlist is offered in, and the walk
    /// order the draw used.
    std::vector<spawn_seat_candidate> candidates;
};

/// The seat's static landscape reading — the ONE place it is derived.
///
/// Phase 6's score (`landscape_score`) is a LANDSCAPE record with per-market
/// readings; it carries no per-corporation number. The seat's reading is taken
/// from those terms and nothing new: for each of @p corp's sited holdings, find
/// the market it clears against (`market_for_tile`, the same catchment partition
/// the score's roster-aware term uses) and read that market's viability,
/// `actual * balance` — the per-market form of the composite's own
/// `viability = mean_actual * mean_balance`. The seat's score is the mean over
/// its holdings. The composite's unevenness multiplier is a property of the
/// whole landscape, identical for every seat in one world, so it cannot move a
/// gate at zero or an order and is not applied.
///
/// @param holdings_scored  Optional out: holdings that stood in a scored market.
double seat_landscape_score(const world& w, const landscape_score& landscape,
                            entity_id corp, int* holdings_scored = nullptr);

/// Shortlist the viable specialists, rank them, draw one weighted against
/// @p seed, and re-point `is_player` / `world::player_entity` onto it.
///
/// Call AFTER phase 6's validation run (the trailing figures it records are what
/// that run filed) and after `generate_background_firms` (which is what makes
/// the specialist/background split meaningful).
///
/// Clears every `is_player` flag before setting one, so `world.hpp`'s stated
/// invariant — exactly one entry has `is_player == true`, and `player_entity`
/// equals that entry's key — holds even if this is somehow called twice.
///
/// @param w         The settled world. Mutated only through the two player
///                  fields; nothing else is touched.
/// @param seed      The world seed. The draw's only entropy.
/// @param landscape Phase 6's score of the WINNING landscape
///                  (`landscape_search_result::winner_score`). An empty score
///                  (no search ran) scores every seat zero, and the floor is
///                  recorded unmet rather than silently passed.
/// @param params    Draw tunables; the defaults are the shipped first cut.
spawn_seat_result seat_player_corporation(world& w, std::uint32_t seed,
                                          const landscape_score& landscape,
                                          spawn_seat_params params = {});
