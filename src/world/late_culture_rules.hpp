#pragma once

// ---------------------------------------------------------------------------
// late_culture_rules — THE RULED PREDICATE of the late culture (BL-1288)
// ---------------------------------------------------------------------------
//
// EXPLORATION.md sec A colony is a subject (THE LATE CULTURE IS TARGETED, Ben
// 2026-10-10; the 2026-10-11 rulings under it): the founding culture MOST
// REMOTE BY REACH when the Culture round closes -- one per world -- develops
// slower because development diffuses by contact over reach and it receives
// the least, reaches the Exploration age holding a realm of its own (the same
// isolation slowed its conquest), and is colonised there by a polity of
// another culture through the ordinary subjection verbs.
//
// ONE PLACE. The sim (history_sim.cpp: the choice at the Empires open, the
// diffusion, the backstop) and the detector (history_pattern_detectors.cpp,
// read by the history_patterns gate and the nation card) both read these, so
// the world is pressed toward exactly the predicate it is judged by.
//
// HOW REACH IS MEASURED (the choice). Over the region network the sim's own
// supply walks -- `supply_neighbours`, every region within
// `neighbour_radius` -- each step priced by `edge_step`: the tile distance
// weighted by the two ends' landform (mountain twice plains) and discounted
// by the road this history has walked onto the line (none yet at the close,
// so it is terrain and distance). From all of a culture's land regions
// standing at the close at once (a multi-source Dijkstra, any holder's ground
// passable, as knowledge and armies alike cross it), the walk finds every
// POLITY standing at the close whose seat another people founded: the
// culture's CONTACT is the sum over those polities of
//   (horizon - cheapest walk to its ground) / horizon,  per mille,
// inside `history_sim_params::contact_diffusion_horizon` -- the same horizon,
// the same steps and the same "a polity teaches" the diffusion spends, so the
// people chosen is the one the force reaches least. The late culture is the
// culture holding a seat at the close with the LEAST contact; ties on the
// cheapest walk to any other people's land (further first; a culture the
// network joins to nobody, `late_culture_unreached`, furthest), then its
// water (grid distance squared to the nearest foreign region), then the
// lower culture index. Recorded as `late_culture_chosen`, with that cheapest
// walk as its remoteness.

/// The lag the detector requires: the culture's polities' mean materials band
/// at the Empires close at or under this share of the world's mean, x100.
/// PROVISIONAL (the ruling leaves the number to measurement; BL-1286).
inline constexpr int late_culture_lag_x100 = 85;

/// The remoteness recorded for a culture the network joins to no foreign
/// ground at the close (a reach sentinel above any walkable cost).
inline constexpr int late_culture_unreached = 1 << 28;

/// The backstop's course for the lag: a straight line from par at the
/// Empires open to this at its close, under the detector's threshold so the
/// close clears it with room. A read behind the course presses one level.
inline constexpr int late_culture_lag_course_x100 = 75;

/// The press's level ceiling (the backstop's bounded steps), in either span.
inline constexpr int late_culture_max_level = 10;
