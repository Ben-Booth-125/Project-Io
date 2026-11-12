#pragma once

#include "ui_state.hpp"
#include "world/world.hpp"

#include <imgui.h>

#include <vector>

/// @file
/// The pinned-items watch list (docs/ui/LAYOUT.md § Pinned items — right column,
/// middle): the residual middle of the right chrome column, between the time panel
/// and the minimap. Fixed shell chrome, not a ledger — no nav-rail slot, absent
/// from `close_all_panels` / `any_panel_open`, like the comms dock and the
/// Selection band.
///
/// Two kinds of card, front pins first:
///   - A **front pin** (Ben, 2026-11-10) for every body where a war the player's
///     nation is party to has a front. Placed and removed by the war, never by the
///     player; derived from world state each frame and never stored.
///   - The player's own **pins**: watch cards (kind glyph, kind, and the shared
///     `entity_summary` stat block), placed by the pin toggle on the Selection band.
///
/// The click model is SELECTION.md's: a single click selects (the Selection band
/// then shows the full action content), a double click navigates. A pin is a
/// watch card, not a second action surface.

namespace ui {

/// One front of a war the player's nation is party to, on one body.
///
/// Until wars reach other bodies (BL-1602) a front is read as: a body where the
/// player's nation and the enemy both HOLD land provinces (`province_holder`).
struct front_pin
{
    entity_id body  = null_entity;
    entity_id enemy = null_entity;
    int mine_held    = 0; ///< Land provinces on the body the player's nation holds.
    int theirs_held  = 0; ///< ... the enemy holds.
    int mine_lost    = 0; ///< The player's own provinces (by owner) the enemy holds.
    int theirs_taken = 0; ///< The enemy's provinces the player's nation holds.
    int war_score    = 0; ///< `war_score`, from the player's side: + is winning.
    /// Where a double click lands: the middle of the player's land front toward the
    /// enemy on this body; else a contested province; else the enemy's province
    /// nearest the player's ground. `null_entity` frames the body's surface.
    entity_id target_tile = null_entity;
    /// The key of the player's land front toward the enemy on this body, when one
    /// exists (fronts.hpp): what a single click selects. Else the click selects
    /// the enemy nation.
    entity_id front_key = null_entity;
};

/// Every front pin, in (war key, body) order. Empty when the player's nation is at
/// war with nobody, or when no player is seated. Pure read of the world.
std::vector<front_pin> derive_front_pins(const world& w);

/// Whether @p id is among the player's pins.
bool is_pinned(const ui_state& ui, entity_id id);

/// Pin @p id, or unpin it if it is pinned (the toggle rule).
void toggle_pin(ui_state& ui, entity_id id);

/// What the pin toggle would pin: the entity the Selection band shows, when that is
/// an entity (not a province, front, battle, deposit, plate or convoy, and not the
/// resting state). `null_entity` when there is nothing to pin.
entity_id pin_subject(const world& w, const ui_state& ui);

/// The double-click on a player pin: frames the canvas on @p id (`focus_on_entity`,
/// a tile through `focus_on_tile`, a nation on its capital tile).
void navigate_to_pin(const world& w, ui_state& ui, entity_id id);

/// The double-click on a front pin: that body's Planetary view at the front.
void navigate_to_front(const world& w, ui_state& ui, const front_pin& f);

/// Draw the panel into `pinned_panel_rect(disp, ui.time_panel_h)`, stopping one
/// margin above the lens chrome when a lens key has opened it this frame. Draws
/// nothing (and no window) when there are no pins and no fronts. Writes only its
/// own `ui.pinned` state and, on a press, the selection or the canvas framing.
void draw_pinned_panel(const world& w, ui_state& ui, ImVec2 disp);

/// Draw the pin toggle at the Selection band's top-right corner, over the band's
/// header row. Drawn only when `pin_subject` names something.
void draw_pin_toggle(const world& w, ui_state& ui, ImVec2 band_origin, ImVec2 band_size);

} // namespace ui
