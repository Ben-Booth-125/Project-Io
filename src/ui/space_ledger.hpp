#pragma once

#include "ui_state.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

namespace ui {

/// Draws the Space ledger — nav rail slot 9. Design: docs/ui/ledgers/space.md.
///
/// "What does my nation have in space, and what can it launch next?" Four views
/// behind a `nav_button` strip (re-clicking the active tab closes the ledger):
///
/// - **Launch** (default): the class picker (satellite with its role, station
///   core, crewed mission, rover), the destination picker — orbital nodes the
///   class may go to, with free slots and the path's thrust cost and days from
///   the home surface — and the quote: goods off the home shelf, credits, travel,
///   crew drawn. A refused launch says why, in the seam's own refusal.
/// - **Assets**: one row per satellite, station and mission — class, role, node,
///   state, upkeep, comms coverage; a satellite row expands in place to Retask it.
/// - **Stations**: the station picker and the designer — the grid drawn as it is,
///   core at the centre, edge slots marked; the budgets above it; the module list
///   of a pressed empty slot; Remove / Move for a filled one; templates; Upgrade.
/// - **Crew**: the astronaut pool, its cap, the training centres and their rate,
///   and crew needed against crew aboard.
///
/// Under the tabs, on every view, the pending line: orders waiting on a signal.
///
/// NOT A SECOND WRITE PATH. Every press queues a `space_command` on
/// `s.space.pending`; `app::render` sends it through `apply_space_command`
/// (src/world/space_command.hpp) and writes the answer to `s.space.message`.
///
/// @param w    Read-only world.
/// @param reg  Registry: the space-asset and station dials, and the fair-price ceiling.
/// @param s    UI state — the ledger's view state and its queued presses.
/// @param open Open/closed flag; toggled by the nav rail.
void draw_space_ledger(const world& w, const recipe_registry& reg, ui_state& s, bool& open);

/// The seam's answer to a press, in the ledger's words (app writes it to `message`).
std::string space_outcome_text(const space_command& c, const space_command_outcome& o);

} // namespace ui
