#pragma once

// ---------------------------------------------------------------------------
// BL-1431 — the missile section of a card (docs/military/MILITARY.md § Missiles)
// ---------------------------------------------------------------------------
// Drawn inside the player's own land unit, ship or wing card. A launcher lists each
// kind it carries with the nation's magazine count, a Buy press (stock_missiles), the
// targets it may strike now (`missile_targets`, the seam's own legs) and a Fire press
// (fire_missile). An air defence unit lists its interceptors and a Stock press
// (stock_interceptors). Every press queues a command on `ui_state::missile.pending`,
// which app::render sends through the seam. Nothing here mutates the world. Default
// ImGui widget sizes only: no hard-coded pixel chrome.
// ---------------------------------------------------------------------------

#include "ui_state.hpp"

#include "world/corp_command.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <string>

namespace ui {

/// Draw the section for @p entity (a unit, ship or wing) when it is the player's and it
/// launches or intercepts anything; draws nothing and returns false otherwise.
bool draw_missile_section(const world& w, const recipe_registry& reg, ui_state& ui, entity_id entity);

/// The answer line for a missile verb's result.
std::string missile_order_answer(corp_verb verb, corp_command_result r);

} // namespace ui
