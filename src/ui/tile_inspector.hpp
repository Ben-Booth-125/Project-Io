#pragma once

#include "world/world.hpp"

namespace ui {

/// Draw the Layer 1 tile inspector panel.
///
/// Displays a body selector and a table of every tile on the selected body,
/// showing terrain type, hazard level, habitability, and all resource deposits.
/// Also lists buildings and the local market state for the selected body.
///
/// @param w  Read-only reference to the current world state.
void draw_tile_inspector(const world& w);

} // namespace ui
