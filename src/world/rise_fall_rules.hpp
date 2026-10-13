#pragma once

// ---------------------------------------------------------------------------
// rise_fall_rules — THE RULED PREDICATE of the rise and fall (BL-1287)
// ---------------------------------------------------------------------------
//
// CIVILISATION.md sec How an empire actually falls (Ben, 2026-10-10/11/12):
// the realm largest at 200 BCE -- or, where its reachable ground cannot hold
// the floor, the largest realm whose ground can ("a boxed front-runner gives
// way") -- peaks by 400 CE at a share of held regions at or above the floor and
// under the non-hegemony cap, and its share halves within 300 years of the
// peak, successors breaking away.
//
// ONE PLACE. The sim's backstop (history_sim.cpp, the rise and fall block) and
// the detector (history_pattern_detectors.cpp, read by the history_patterns
// gate and the nation card) both read these, so the world is pressed toward
// exactly the predicate it is judged by.

inline constexpr int rise_fall_pick_year    = -200; ///< the front-runner is read here
inline constexpr int rise_fall_peak_by_year = 400;  ///< its share peaks at or before this year
inline constexpr int rise_fall_window_years = 300;  ///< and halves within this many years of the peak
inline constexpr int rise_fall_hegemony_pm  = 500;  ///< the non-hegemony cap (BL-224), never reached
/// The floor its peak must clear, per mille of held regions: 25 percent,
/// ruled (Ben, 2026-10-12) inside the 25-35% band from the measured peaks.
inline constexpr int rise_fall_floor_pm     = 250;
