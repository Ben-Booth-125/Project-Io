#pragma once

#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

namespace ui {

/// Draw the Layer 3 economy observability panel — the read-only surface that makes
/// the economy legible (INITIAL_INSTRUCTIONS). Surfaces, per the Layer 3 design:
///   - each corporation's running balance (negative flagged red);
///   - each (corporation, body) pool's per-resource quantities;
///   - each building's current output rate and idle/active state, and for
///     processors the limiting input;
///   - each body market's supply / demand figures.
///
/// Nothing here is edited — workforce and recipe are authored elsewhere. Read-only.
///
/// The window carries a close button; clicking it clears *p_open so the window
/// closes rather than collapses. When *p_open is false the function draws nothing.
///
/// @param w      Read-only world (pools, markets, corporations).
/// @param reg    Loaded registry (recipe names for processor rows).
/// @param report The most recent economy step report (building states).
/// @param p_open Open/closed flag; cleared by the close button.
void draw_economy_panel(const world& w,
                        const recipe_registry& reg,
                        const economy_report& report,
                        bool* p_open);

} // namespace ui
