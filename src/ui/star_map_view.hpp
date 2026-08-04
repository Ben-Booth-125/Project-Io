#pragma once

#include "imgui.h"

namespace ui {

/// Draw the fixed star map into a rectangle — the Solar minimap's inset.
///
/// Solar is the top rung of the zoom ladder, so its minimap has no zoom-out
/// neighbour to show; MINIMAP.md § The top rung recorded the slot as a branding
/// placeholder drawing a flat dark fill. The rung above a solar system is the
/// galaxy it sits in, and unlike everything else on screen that galaxy is the
/// SAME every campaign (src/world/star_map.hpp) — the one fixture a player
/// carries between worlds.
///
/// Purely presentational: reads the authored table, draws, touches no world
/// state and no seed.
void draw_star_map(ImVec2 origin, ImVec2 size);

} // namespace ui
