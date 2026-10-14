#pragma once

// ---------------------------------------------------------------------------
// world_war_rules — THE RULED PREDICATE of the world war (BL-1289)
// ---------------------------------------------------------------------------
//
// INDUSTRIALISATION.md sec A world war in every world (Ben, 2026-10-10/11/12):
// every world fights one, inside the fixed late window 1900-1930, formed by the
// mutual-defence spread; where the treaty graph sparks nothing by the window a
// contest for a subject between the two largest powers fires, and where the
// lean still misses it escalates in bounded, recorded steps until the
// detector passes.
//
// ONE PLACE. The sim's war (history_sim.cpp, the world war block) and the
// detector (history_pattern_detectors.cpp, read by the history_patterns gate
// and the nation card) both read these, so the world is pressed toward exactly
// the predicate it is judged by -- the rise and fall's discipline
// (rise_fall_rules.hpp).

inline constexpr int world_war_window_start = 1900; ///< ruled (Ben, 2026-10-11)
inline constexpr int world_war_window_end   = 1930; ///< ruled, inclusive
/// The treaty graph has until this year to spark the war on its own (a battle
/// between living realms where the defender's mutual-defence partners are
/// drawn in and a great power stands on either side). At the first decision
/// round at or after it, with nothing sparked, the contest for a subject
/// between the two largest powers fires (Ben, 2026-10-12). PROVISIONAL: the
/// ruling says "by the window"; two rounds of grace inside it are the reading.
inline constexpr int world_war_spark_deadline = 1908;
/// "The top powers": the most populous living realms at the window's open.
/// PROVISIONAL (the detector's reading, BL-1286).
inline constexpr int world_war_top_k          = 6;
/// "Most of the great powers": how many of those must be belligerents.
/// PROVISIONAL (the detector's reading, BL-1286).
inline constexpr int world_war_min_engaged    = 4;
