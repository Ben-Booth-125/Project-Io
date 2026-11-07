#pragma once

// ---------------------------------------------------------------------------
// BL-1534 — the ship on the UI (docs/ui/ledgers/military.md § Ships on the map;
// docs/military/MILITARY.md § Sea)
// ---------------------------------------------------------------------------
// A ship is selectable: its card on the Selection band sets the mission (only the
// missions its class works, and None), lists the sea provinces of its zone, and
// offers the refit. A transport's card lists who is aboard, who may board, and its
// landing. With a ship selected, a right-click on the Planetary canvas is the order:
// a sea province joins a warship's zone, a coast is where a transport lands.
// `preview_ship_order` is that click's answer, shared by the canvas's hover line and
// its press, and it reads the seam's own checks so the UI cannot disagree with them.
//
// Every press queues a command on `ui_state::ship.pending`, which app::render sends
// through the seam. Nothing here mutates the world.
// ---------------------------------------------------------------------------

#include "ui_state.hpp"

#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ui {

/// "Submarine", "Destroyer", "Capital ship", "Transport" (MILITARY.md § Sea).
const char* ship_class_label(ship_class c);

/// "None", "Patrol", "Escort", "Raid", "Blockade".
const char* sea_mission_label(sea_mission m);

/// The ship's display name: its roster row's.
const char* ship_display_name(const world& w, entity_id ship);

/// Is @p ship the player's own? (Only the player's ships take orders.)
bool ship_is_players(const world& w, entity_id ship);

/// Where the ship is and what it is doing, in one line: "Mustering: 12 days", "On station".
std::string ship_status_text(const world& w, const recipe_registry& reg, entity_id ship);

/// May the player see @p ship now: its own, one the last sea day's pass found
/// (`world::ships_found_by_player`), or any at all while spectating?
bool ship_seen_by_player(const world& w, const ui_state& s, entity_id ship);

/// Drop held missions whose ship is gone or already works one. Run every frame the
/// canvas draws and on every card draw.
void prune_ship_intents(const world& w, ui_state& s);

/// Drop the card's held missions, queued orders and answer, and the ledger's queued
/// Raise: called beside `reset_wing_ui` when the world is replaced.
void reset_ship_ui(ui_state& s);

/// What a right-click on @p tile would do for the selected ship.
struct ship_order_preview
{
    int                   kind = 0;  ///< 0 nothing to say, 1 an order, 2 refused.
    std::string           text;      ///< The hover line, in the player's words.
    corp_command          cmd;       ///< The order the press would send (kind 1).
};

/// The right-click's answer for @p ship over @p tile.
ship_order_preview preview_ship_order(const world& w, const recipe_registry& reg, const ui_state& s,
                                      entity_id ship, entity_id tile);

/// The seam's answer to one ship press, in the player's words.
std::string ship_order_answer(corp_verb verb, uint32_t order, corp_command_result r);

/// Draw the ship card into the Selection band's window. Returns false when the
/// selection is not a live ship (the caller then falls through).
bool draw_ship_selection(const world& w, const recipe_registry& reg, ui_state& s);

} // namespace ui
