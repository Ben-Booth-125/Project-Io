#pragma once

#include "world/era_timelapse.hpp"
#include "world/rise_fall_rules.hpp" // BL-1287: the RF constants, one place with the sim

#include <cstdint>
#include <vector>

struct settlement_state;

// ---------------------------------------------------------------------------
// history_pattern_detectors — the five history-pattern detectors (BL-1286)
// ---------------------------------------------------------------------------
//
// GENERATION_STRATEGY.md sec History patterns (Ben, 2026-10-10): "Each pattern
// has a DETECTOR — a predicate over the history record that proves the target
// was hit and names the polities it touched. The detector is both the gate and
// the record: the polities it names carry a PATTERN TAG into the nation they
// fold into."
//
// PURE AND RECORD-ONLY. Every function here reads the recorded history alone —
// the Empires, Exploration and Industrialisation `era_timelapse` records and
// the post-sim settlement — and writes nothing but its return value. Nothing in
// the sim or at world setup reads a detector, so a detector can never steer the
// world it describes; the gates (tools/verify/history_patterns.cpp) and the
// nation card (STARTUP.md sec The seat) are its readers. Ordered containers and
// index-ordered walks throughout: the same record gives the same reading.
//
// THE READINGS ARE THE RULED ONES:
//   rise and fall  CIVILISATION.md sec How an empire actually falls (Ben,
//                  2026-10-11/12): the realm largest at 200 BCE peaks by 400 CE
//                  at a share of held regions at or above the floor and under
//                  the 500 per mille non-hegemony cap, and its share halves
//                  within 300 years of the peak, successors breaking away.
//                  Where that realm is boxed (Ben, 2026-10-12) the rise falls
//                  to the largest realm whose reachable ground can hold the
//                  floor; the record names it (the first `rise_pressed`).
//   late culture   EXPLORATION.md sec The late culture (Ben, 2026-10-10/11): the
//                  culture most remote when the Culture round closes, lagging,
//                  and bound in Exploration by a polity of another culture.
//   world war      INDUSTRIALISATION.md sec A world war in every world (Ben,
//                  2026-10-11): inside 1900-1930, the top powers drawn in.
//   civil war      CIVILISATION.md, "What counts as one" (Ben, 2026-10-11): a
//                  successor breaking away with at least a quarter of what its
//                  parent kept (3+ regions), and the two then taking seats from
//                  or ending each other within 50 years.
//   revolution     INDUSTRIALISATION.md sec Revolution (Ben, 2026-10-12): a
//                  takeover, recorded as a revolution event.
// Where a ruling leaves a number to measurement (the rise's floor inside 25-35
// percent, the late culture's lag, the world war's count of powers) the
// constant below is PROVISIONAL and says so; the readings beside each verdict
// let a different threshold be read off the same record.

/// The five patterns, as the bit each takes in a pattern-tag mask.
enum class history_pattern : uint8_t
{
    rise_and_fall = 0, ///< tag: "rose and fell"
    late_culture  = 1, ///< tag: "colonised" (the late culture's bound polities)
    world_war     = 2, ///< tag: "world war"
    civil_war     = 3, ///< tag: "survived civil war" (both halves)
    revolution    = 4, ///< tag: "revolution"
    count
};

inline constexpr uint8_t history_pattern_bit(history_pattern p)
{
    return static_cast<uint8_t>(1u << static_cast<unsigned>(p));
}

/// Provisional thresholds (named so the gate and the card read one number).
// The rise and fall's constants ARE the sim's (`rise_fall_rules.hpp`, BL-1287):
// aliased, never restated, so the detector and the backstop judge one predicate.
inline constexpr int hp_rf_front_runner_year = rise_fall_pick_year;    ///< the lean backs the realm largest here
inline constexpr int hp_rf_peak_by_year      = rise_fall_peak_by_year; ///< ...which peaks by this year
inline constexpr int hp_rf_floor_q           = rise_fall_floor_pm;     ///< ruled: 25 percent (Ben, 2026-10-12)
inline constexpr int hp_rf_hegemony_q        = rise_fall_hegemony_pm;  ///< the non-hegemony cap (BL-224), not overturned
inline constexpr int hp_rf_fall_window_y     = rise_fall_window_years; ///< share halves within this many years of the peak
inline constexpr int hp_lc_lag_x100          = 85;   ///< PROVISIONAL: materials mean <= 85% of the world's at the Empires close
inline constexpr int hp_ww_window_start      = 1900;
inline constexpr int hp_ww_window_end        = 1930; ///< inclusive
inline constexpr int hp_ww_top_k             = 6;    ///< PROVISIONAL: "the top powers" = the six most populous at the window's open
inline constexpr int hp_ww_min_belligerents  = 4;    ///< PROVISIONAL: of those six
inline constexpr int hp_cw_min_half_q        = 250;  ///< a quarter of what the parent kept
inline constexpr int hp_cw_min_regions       = 3;
inline constexpr int hp_cw_window_y          = 50;

/// What the detectors read, and what each found. `*_polities` are the
/// polities a HIT names (sorted, unique); empty on a miss. The other fields
/// are the readings, kept on a miss too so a gate can say how near it came.
struct history_pattern_reading
{
    // --- rise and fall
    bool rf_hit = false;
    int  rf_polity = -1;        ///< the front-runner at 200 BCE (-1: no record)
    int  rf_front_q = 0;        ///< its share then, per mille of held regions
    int  rf_peak_q = 0;         ///< its peak share
    int  rf_peak_year = 0;
    int  rf_fall_years = -1;    ///< peak -> first step at or under half the peak; -1 never
    int  rf_breakaways = 0;     ///< successors that broke from it between peak and fall (the hit needs one)
    int  rf_boxed = -1;         ///< the boxed front-runner the rise gave way from, read off the record (-1: none)
    int  rf_fall_q = -1;        ///< its share at the fall step
    int  rf_margin_x100 = 0;    ///< its peak share over the next realm's at that step, x100
    int  rf_captures = 0;       ///< seats it took up to its peak
    int  rf_rise_level = 0;     ///< the highest rise level the record presses (`rise_pressed`)
    int  rf_fall_level = 0;     ///< the highest overextension level (`overextended`)
    bool rf_ended = false;      ///< it ended within the span
    std::vector<int32_t> rf_polities;

    // --- late culture
    bool lc_hit = false;
    int  lc_culture = -1;       ///< the most remote founding culture at the Culture close
    int  lc_gap = 0;            ///< its isolation, grid distance squared to the nearest foreign region
    int  lc_second_gap = 0;     ///< the next most remote culture's, for scale
    int  lc_lag_x100 = -1;      ///< its polities' materials mean over the world's at the Empires close; -1 none standing
    int  lc_bound = 0;          ///< its polities bound or bought by another culture in Exploration
    std::vector<int32_t> lc_polities;

    // --- world war
    bool ww_hit = false;
    int  ww_belligerents = 0;   ///< of the top-k at the window's open, how many a conflict moment touched
    int  ww_moments = 0;        ///< conflict moments inside the window
    std::vector<int32_t> ww_polities;

    // --- civil war
    int  cw_splits = 0;         ///< splits that were halves
    int  cw_count = 0;          ///< of those, the ones whose halves then fought
    std::vector<int32_t> cw_polities;

    // --- revolution
    int  rv_count = 0;          ///< takeovers recorded (`revolution` events on the Industrialisation record, BL-1294)
    int  rv_proxy = 0;          ///< READING ONLY: Industrialisation successors that took or ended their parent
    std::vector<int32_t> rv_polities;

    /// Every (polity, pattern) a hit names, DATED: the year the pattern touched
    /// that polity (rise and fall: its peak; late culture: the binding; world
    /// war: the polity's first conflict moment in the window; civil war: the
    /// fight that made it one; revolution: the takeover). The tag fold replays the record's endings and
    /// splits after these dates (`fold_history_pattern_tags`), so a tag passes
    /// only to realms that inherit or end its holder AFTER it was earned.
    struct grant { int32_t polity = -1; int32_t year = 0; uint8_t pattern = 0; };
    std::vector<grant> grants;
};

/// Run all five detectors over one body's records. @p gw is the settlement
/// grid's width (columns wrap). Pure.
history_pattern_reading detect_history_patterns(const era_timelapse& empires,
                                                const era_timelapse& exploration,
                                                const era_timelapse& industrialisation,
                                                const settlement_state& settlement,
                                                int gw);

/// THE FOLD (NATION_GENERATION.md sec Pass 2d; GENERATION_STRATEGY.md sec
/// History patterns, Ben 2026-10-12: "a realm that ends passes its tags to
/// whoever ended it, and a successor carries its parent line's").
///
/// First the tags TRAVEL, replaying the three records in order (Empires, then
/// Exploration, then Industrialisation, each in its own event order), with
/// every grant applied before the first event dated after it:
///   - a SPLIT (`broke_away`, `schism`, `seceded_at_close`): the successor
///     takes its parent's tags as they stand at the split;
///   - an END (`realm_ended` with a killer; `absorbed_at_close` with an
///     absorber): the killer / absorber takes the dead realm's tags.
/// Then each nation's mask is the union of the tags its own realm
/// (`nation_polity[n]`) and every realm the size floor folded into it
/// (`nation_absorbed[first[n] .. first[n]+count[n])`) holds at the end.
/// Parallel to `nation_polity`. @p unfolded, when given, receives the number
/// of (polity, pattern) tags held by a realm that is no nation, was folded
/// into none, and passed them to nobody (a realm that ended with no killer) —
/// counted, never dropped silently. Pure and deterministic.
std::vector<uint8_t> fold_history_pattern_tags(const history_pattern_reading& reading,
                                               const era_timelapse& empires,
                                               const era_timelapse& exploration,
                                               const era_timelapse& industrialisation,
                                               const std::vector<int32_t>& nation_polity,
                                               const std::vector<int32_t>& absorbed_first,
                                               const std::vector<int32_t>& absorbed_count,
                                               const std::vector<int32_t>& absorbed,
                                               int* unfolded = nullptr);
