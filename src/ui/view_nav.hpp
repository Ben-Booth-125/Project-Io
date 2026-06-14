#pragma once

#include "ui_state.hpp"
#include "world/world.hpp"

namespace ui {

/// Focus the canvases on a body, framing it in its orbital / local view: select
/// it, switch to the rung where the body is the centre, and reset that rung's
/// pan (and zoom) so the body sits centred at a default framing.
///
/// Rung choice: a star frames in the Solar view (it is the system centre); a
/// planet, asteroid, or station frames in its own Circumplanetary view; a moon
/// frames in its parent planet's Circumplanetary view with the moon selected (a
/// moon has no view of its own). Choosing the rung where the target is the
/// anchor is what makes "centre on it" well-defined for an orbiting body. To
/// look at something *on* a surface instead, use focus_on_surface / _tile /
/// _entity. A stale id is ignored.
///
/// @param w    Read-only world state.
/// @param ui   UI state to retarget.
/// @param body Body to focus.
void focus_on_body(const world& w, ui_state& ui, entity_id body);

/// Focus the Planetary surface of a body: select it, bring the surface rung
/// forward, and centre it at the default framing. Used for the opening view and
/// as the landing rung for things that sit on a surface. A stale id is ignored.
///
/// @param w    Read-only world state.
/// @param ui   UI state to retarget.
/// @param body Body whose surface to show.
void focus_on_surface(const world& w, ui_state& ui, entity_id body);

/// Focus the surface the tile belongs to and select the tile. A stale id is
/// ignored.
///
/// @param w    Read-only world state.
/// @param ui   UI state to retarget.
/// @param tile Tile to focus and select.
void focus_on_tile(const world& w, ui_state& ui, entity_id tile);

/// Focus an arbitrary entity, resolving it to the most informative view:
///   - body            → focus_on_surface (its Planetary tile surface — the most
///                       informative rung; this is what 'go to' on a body does)
///   - tile            → no-op: a tile is selected from the surface it lives on,
///                       so there is nothing to descend to (pan-to-tile is out of
///                       scope for now)
///   - building        → that building's host-tile surface
///   - unit / market   → that body's surface
///
/// This is the single call the Selection panel's 'go to' button (and a
/// double-click) routes through, and that later alerts / notifications will use
/// instead of poking ui_state directly. Unknown, null, or stale ids are ignored.
///
/// @param w      Read-only world state.
/// @param ui     UI state to retarget.
/// @param entity Any entity to focus.
void focus_on_entity(const world& w, ui_state& ui, entity_id entity);

} // namespace ui
