#pragma once

#include "ui_state.hpp"
#include "world/world.hpp"
#include "world/entity.hpp"

namespace ui {

/// Lens-contextual hover-card content builder (BL-020).
///
/// Renders condensed "why not what" ImGui content for the entity under the
/// cursor, framed by the active canvas lens. The contract is:
///   title line  (entity name / type)
///   ≤2 stat lines
///   1 why-line  (explains behaviour, not just numbers)
///
/// Callers place this inside a draw_hover_card content lambda; the function
/// emits ImGui widgets only and owns no window.
///
/// Dispatch table (entity kind × lens — the 3 implemented exemplars):
///   tile       × resource  → deposit richness for lens_resource + why label
///   building   × supply    → type + output rate + operational status
///   market     × market    → price for lens_resource + supply/demand why
///
/// Any unimplemented (kind, lens) pair falls back to a brief type label so
/// the card is never blank.
///
/// @param w    Read-only world state.
/// @param ui   Current canvas/nav/lens state.
/// @param eid  The entity currently under the cursor.
void draw_hover_content(const world& w, const ui_state& ui, entity_id eid);

} // namespace ui
