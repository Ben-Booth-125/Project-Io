#pragma once

// ---------------------------------------------------------------------------
// BL-1450 — the air wing on the UI (docs/ui/ledgers/military.md § Grouped by
// theatre; docs/military/MILITARY.md § Air)
// ---------------------------------------------------------------------------
// A wing is selectable: its card on the Selection band sets the mission (only the
// missions its class flies, and None) and lists the provinces assigned, each with
// a remove press. With a wing selected, a right-click on a province of the
// Planetary canvas assigns it on the wing's current mission — `preview_wing_assign`
// is that click's answer, shared by the canvas's hover line and its press, and it
// reads `air_mission_check` so the UI cannot disagree with the seam.
//
// Every press queues `ui_state::wing.pending_*`, which app::render sends as one
// `assign_air_mission`. Nothing here mutates the world.
// ---------------------------------------------------------------------------

#include "ui_state.hpp"

#include "world/components.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ui {

/// "Fighter", "Strike / multirole", ... — MILITARY.md § Air's class names.
const char* air_class_label(air_class c);

/// "None", "Air superiority", "Tactical bombing", "Strategic bombing", "Recon".
const char* air_mission_label(air_mission m);

/// The wing's display name: its roster row's ("Interceptor Wing").
const char* wing_display_name(const world& w, entity_id wing);

/// Drop held missions (`wing_ui_state::intent`) whose wing is gone or already
/// flies: a held mission is the card's only while its wing flies nothing. The
/// canvas calls it every frame, the card on every draw.
void prune_wing_intents(const world& w, ui_state& s);

/// Drop the wing card's held missions, its queued order and its answer, and the
/// ledger's queued air Raise: called beside `reset_unit_designer` when the world
/// is replaced (a load, a new campaign).
void reset_wing_ui(ui_state& s);

/// The mission a right-click assigns @p wing on: the one it flies, or — while it
/// flies none — the mission its card holds for it (`wing_ui_state::intent`).
air_mission wing_current_mission(const ui_state& s, const world& w, entity_id wing);

/// What a right-click on @p province would do for the selected wing.
struct wing_assign_preview
{
    int                   kind = 0;  ///< 0 nothing to say, 1 assign, 2 refused.
    std::string           text;      ///< The hover line, in the player's words.
    air_mission           mission = air_mission::none;
    std::vector<uint32_t> provinces; ///< The list the press would send (kind 1).
};

/// Is @p wing the player's own? (Only the player's wings take orders.)
bool wing_is_players(const world& w, entity_id wing);

/// The right-click's answer for @p wing over @p province (0 = no province).
wing_assign_preview preview_wing_assign(const world& w, const recipe_registry& reg,
                                        const ui_state& s, entity_id wing, uint32_t province);

/// Draw the wing card into the Selection band's window. Returns false when the
/// selection is not a live wing (the caller then falls through).
bool draw_wing_selection(const world& w, const recipe_registry& reg, ui_state& s);

} // namespace ui
