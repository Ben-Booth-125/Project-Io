#pragma once

#include <cstdint>

#include <imgui.h>

namespace ui {

/// The interaction state an entity is shown in, expressed in one shared visual
/// language so selection, hover, and pinning read the same on every canvas and
/// ledger. A single dominant state is drawn per entity; resolve_highlight
/// applies the precedence when several apply at once. Colours come from the
/// semantic palette (see ui/presentation.hpp).
enum class highlight : uint8_t
{
    none = 0,
    hovered,  ///< Under the cursor.
    pinned,   ///< Kept in the Explorer's working set.
    selected, ///< The active selection.
};

/// Collapse the set of applicable states to the one that should be drawn, using
/// the convention's precedence: selected > pinned > hovered. Pinned outranks
/// hover so a pinned entity keeps its identity colour while the cursor is over
/// it; selection outranks both because it is the player's current focus.
///
/// @param selected Whether the entity is the active selection.
/// @param hovered  Whether the cursor is over the entity.
/// @param pinned   Whether the entity is pinned in the Explorer.
/// @return         The dominant highlight to draw.
highlight resolve_highlight(bool selected, bool hovered, bool pinned);

/// Draw the standard ring highlight around a circular body marker (Solar /
/// Circumplanetary canvases). Nothing is drawn for highlight::none. The ring
/// sits a fixed gap outside the marker and keeps a constant pixel footprint
/// regardless of zoom, matching the existing body-outline treatment.
///
/// @param dl          Draw list to render into.
/// @param centre      Body centre, screen pixels.
/// @param body_radius Drawn radius of the body marker, screen pixels.
/// @param h           Interaction state to depict.
void draw_body_highlight(ImDrawList* dl, ImVec2 centre, float body_radius, highlight h);

/// Draw the standard outline highlight around a hex tile (Planetary canvas).
/// Nothing is drawn for highlight::none.
///
/// @param dl    Draw list to render into.
/// @param verts The six pointy-top hex vertices, screen pixels.
/// @param h     Interaction state to depict.
void draw_hex_highlight(ImDrawList* dl, const ImVec2 verts[6], highlight h);

} // namespace ui
