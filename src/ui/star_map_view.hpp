#pragma once

#include "imgui.h"

namespace ui {

/// Draw the sky over the homeworld into a rectangle — the Solar minimap's inset.
///
/// THE LADDER WRAPS. Solar is the top rung, so its minimap has no zoom-out
/// neighbour; MINIMAP.md § The top rung recorded the slot as a branding
/// placeholder drawing a flat dark fill. Rather than invent a further rung, the
/// panel loops back to where the player is standing — the night sky from the top
/// of a hill. Zoom all the way out and you are looking up again.
///
/// It stays a MINIMAP, not a fourth canvas: nothing here is navigable, and the
/// galaxy appears in the picture (the core as a bright bulge in the band) rather
/// than as a separate map of itself.
///
/// @param day_of_year Turns the sky. One revolution per year, so the panel shows
///                    the constellations up on THIS day rather than a fixed
///                    poster. Pass the campaign date; any value works, it wraps.
///
/// Purely presentational: reads the authored table, draws, touches no world
/// state and no seed.
void draw_star_map(ImVec2 origin, ImVec2 size, float day_of_year);

} // namespace ui
