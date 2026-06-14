#pragma once

#include "world/components.hpp"

#include <imgui.h>

namespace ui::icons {

/// Draw a building-type marker as a vector glyph centred at @p centre with
/// half-extent @p r. Each type has a distinct silhouette so a glance reads the
/// installation kind: extraction = diamond, processing = square, port =
/// triangle, none = dot. Filled in @p fill with a thin dark outline for
/// contrast on any terrain.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent (radius) of the glyph, screen pixels.
/// @param type   Building type to depict.
/// @param fill   Fill colour.
void building(ImDrawList* dl, ImVec2 centre, float r, building_type type, ImU32 fill);

/// Draw a resource pip — a small filled diamond in the resource's identity
/// colour (see presentation_of). For resource strips and deposit markers.
///
/// @param dl     Draw list to render into.
/// @param centre Pip centre, screen pixels.
/// @param r      Half-extent of the pip, screen pixels.
/// @param res    Resource whose identity colour to use.
void resource(ImDrawList* dl, ImVec2 centre, float r, resource_type res);

/// Draw a unit / convoy marker — an upward chevron — in @p colour. For unit
/// stacks and (Layer 5) convoy heads on the canvases.
///
/// @param dl     Draw list to render into.
/// @param centre Marker centre, screen pixels.
/// @param r      Half-extent of the marker, screen pixels.
/// @param colour Fill colour (e.g. a faction colour).
void unit(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw a ledger glyph — a small ruled table outline — in @p colour. Used by the
/// navigation pane for slots that open a ledger window (e.g. the Tile Ledger).
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Stroke colour.
void ledger(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw a reserved-slot placeholder glyph — a hollow rounded square — in
/// @p colour. Used by the navigation pane for slots not yet assigned a menu.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Stroke colour.
void placeholder(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the Supply-lens glyph — two parallel horizontal lines, a route/convoy
/// shorthand — in @p colour. For the overlay-lens control strip.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Stroke colour.
void supply(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the Market-lens glyph — a three-bar ascending chart outline — in
/// @p colour. For the overlay-lens control strip.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Stroke colour.
void market(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the Faction-lens glyph — a downward-pointing shield silhouette — in
/// @p colour. For the overlay-lens control strip.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Fill colour.
void faction(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the Corporation-lens glyph — a filled square with a centred inner dot, a
/// "seal" silhouette — in @p colour. Distinct from the processing-facility plain
/// square (the inner dot), the extraction diamond, and the port/unit triangle.
/// For the overlay-lens control strip. See LENSES.md § Corporation lens.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Fill colour of the square; the inner dot is drawn dark.
void corporation(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

} // namespace ui::icons
