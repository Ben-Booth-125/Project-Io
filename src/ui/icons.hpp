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

} // namespace ui::icons
