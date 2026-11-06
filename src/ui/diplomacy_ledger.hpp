#pragma once

#include "ui_state.hpp"
#include "world/economy_system.hpp"  // economy_report — the GDP read
#include "world/recipe_registry.hpp"
#include "world/standing.hpp"        // corp_standing — reach and market share
#include "world/world.hpp"

#include <vector>

namespace ui {

/// Draws the Diplomacy ledger — nav rail slot 8 (Ben, 2026-10-30). Design:
/// docs/ui/ledgers/diplomacy.md.
///
/// "Who could I fight, who might fight me, and what would it cost?" Two views
/// behind a `nav_button` strip:
///
/// - **Nations** (default): one folded row per nation — stance chips both ways and
///   its Access / Trust / Alarm toward the player, each a signed bar with a trend
///   arrow. Expanding a row in place adds the player's sentiment toward it, the
///   power comparison and front strength, trade exposure, friendship, the embargo
///   press and Declare war. Declare war turns the row into THE DECLARATION: the
///   canvas marks the target's provinces and a click toggles up to three goals;
///   the justifications and the live `declaration_price` follow the goals.
/// - **Wars**: every open war, the player's first, with goals and who holds each,
///   the score as a centred bar, exhaustion and days since it opened; Answer, the
///   peace composer (offer / impose, white or cession), Accept / Refuse; truces.
///
/// NO PRESS MUTATES THE WORLD HERE. Each queues a `corp_command` on
/// `s.diplomacy.pending` (or, for a declare-and-attack, the unit-order request),
/// which `app::render` sends through `player_command` — the seam every agent uses.
///
/// Called every frame, open or not: it samples the UI-side history (the trend
/// arrows' sentiment and the trade-exposure landings) once per econ tick before
/// returning early when closed, so the window has filled by the time it opens.
void draw_diplomacy_ledger(const world& w, const recipe_registry& reg, const economy_report& rep,
                           const std::vector<corp_standing>& standings, ui_state& s, bool& open);

/// Opens the declaration against @p target, as Declare war does — or, when
/// @p target has declared war on the player and the player has not answered, the
/// ANSWER's goal picker. Used by the canvas's right-click declare-and-attack
/// (diplomacy.md § The declaration: "the right-click declare-and-attack opens this
/// same declaration, with the attacked province chosen"): @p attack_tile names the
/// attacked tile, whose goal province (`attack_goal_province`) starts chosen, and
/// @p units are the selection Declare then marches onto it. Opens the ledger,
/// closing any other column occupant.
void open_declaration(const world& w, ui_state& s, entity_id target, entity_id attack_tile = null_entity,
                      const std::vector<entity_id>& units = {});

/// Closes the declaration without effect (Cancel, Escape): no goal, no attack, nothing queued.
void close_declaration(ui_state& s);

/// A press on @p nation under the Nations lens (BL-1511; diplomacy.md § 3): the ledger's
/// Nations view chooses it, opening the ledger if it is closed. The player's own nation
/// has no entry in the selector, and an open declaration keeps the ledger where it is
/// (a press there is its goal pick), so neither moves anything.
void navigate_diplomacy_to(const world& w, ui_state& s, entity_id nation);

/// Clears every piece of the ledger's view state and UI-side history — the
/// declaration, its attack and goals, the peace composer, the trend and trade
/// ring buffers, the derived caches — for a load or a new campaign, whose world
/// shares none of it. Called beside `reset_wing_ui`.
void reset_diplomacy_ui(ui_state& s);

} // namespace ui
