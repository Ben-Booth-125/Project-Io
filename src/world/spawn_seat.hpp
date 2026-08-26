#pragma once

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
//   2. Warm-start in spectate — the pre-game ticks run with NO SEATED CORP,
//      under `corp_ai_params::spectating`. BL-409 settled that under spectate
//      the no-auto-act prohibition has no SUBJECT rather than an exception, so
//      this reorder introduces no new concept: every corp evaluates on the same
//      staggered cadence, and the cadence index is over the SORTED corp set, so
//      admitting one more shifts no rival's slot.
//   3. Shortlist — every SPECIALIST whose filed returns clear the viability
//      floor (`seat_shortlisted` below).
//   4. Seat — one is drawn from the shortlist against the world seed, and
//      `is_player` / `world::player_entity` are re-pointed onto it.
//
// WHAT THIS DELIBERATELY DOES NOT TOUCH. The generator's own uniform draw
// (corporation_generation.cpp § Player corporation flag) still runs and still
// flags a corp. It is now a PROVISIONAL pick that this function overwrites, and
// leaving it in place is what keeps the generation RNG stream — and therefore
// every generation golden and every headless harness that never runs a warm
// start — bit-identical. Seating RE-POINTS an existing decision rather than
// replacing the mechanism that made it.
//
// DETERMINISM. The draw consumes the WORLD SEED through a local `std::mt19937`;
// it reads no wall clock, no global state, and walks only sorted vectors. One
// seed reproduces one seat, on every machine, every run.

/// How many filed quarters the viability floor's trailing-net window reads.
///
/// EIGHT — two years of the forty `k_quarterly_return_retention` keeps. Its own
/// constant rather than a reuse of `k_acquisition_trailing_quarters`, which
/// holds the same value today: that one is the whole-firm acquisition price's
/// statement of how much history a PRICE is entitled to read, this one is the
/// spawn floor's statement of how much history a VIABILITY VERDICT is. They
/// answer different questions and must be free to move apart.
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

/// One specialist's spawn record — every number the floor and the draw read,
/// kept whether or not the corp cleared, so a sweep can say WHY.
struct spawn_seat_candidate
{
    entity_id corp = null_entity;

    /// Closing `corporation_component::balance` at the end of the warm start.
    float balance = 0.0f;
    /// True when `balance > 0` — the floor's solvency half, the same test
    /// `spawn_solvency` (BL-635) applies.
    bool  solvent = false;

    /// Sum of `net` over the last `k_spawn_trailing_quarters` filed returns
    /// (or over every filed return, when the corp has filed fewer).
    float trailing_net = 0.0f;
    /// How many returns that sum actually covered.
    int   quarters_read = 0;

    /// Cleared the floor: solvent AND `trailing_net >= 0`.
    bool  shortlisted = false;

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

    /// THE FLOOR WENT UNMET — no specialist cleared, so the highest trailing-net
    /// specialist was seated instead. A viability signal to be READ, not a
    /// failure to be hidden, and never a licence to conjure or patch a corp to
    /// make the shortlist non-empty (the position § Pass 2's diversity floor
    /// takes). False on an ordinary world.
    bool floor_unmet = false;

    int specialist_count = 0;
    int shortlist_size   = 0;

    /// Every specialist, sorted by entity id — the walk order the draw used.
    std::vector<spawn_seat_candidate> candidates;
};

/// Shortlist the viable specialists, draw one weighted against @p seed, and
/// re-point `is_player` / `world::player_entity` onto it.
///
/// Call AFTER the pre-game warm start (the returns it reads are what the warm
/// start filed) and after `generate_background_firms` (which is what makes the
/// specialist/background split meaningful).
///
/// Clears every `is_player` flag before setting one, so `world.hpp`'s stated
/// invariant — exactly one entry has `is_player == true`, and `player_entity`
/// equals that entry's key — holds even if this is somehow called twice.
///
/// @param w      The warm-started world. Mutated only through the two player
///               fields; nothing else is touched.
/// @param seed   The world seed. The draw's only entropy.
/// @param params Draw tunables; the defaults are the shipped first cut.
spawn_seat_result seat_player_corporation(world& w, std::uint32_t seed,
                                          spawn_seat_params params = {});
