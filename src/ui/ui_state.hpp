#pragma once

#include "world/entity.hpp"

/// Shared selection and view state for the two primary canvases.
///
/// Held by app and passed by reference to both canvas drawing functions. The
/// canvases read it to know what is selected and which view is foregrounded,
/// and write to it in response to body clicks, tile clicks, and minimap swaps.
struct ui_state
{
    entity_id active_body = null_entity; ///< Drives the Body Surface Canvas. null_entity = nothing selected.
    entity_id active_tile = null_entity; ///< Set by a tile click; consumed by later layers.
    bool      surface_is_primary = false; ///< false = Solar System Canvas is primary, Body Surface is the minimap.

    // --- navigation pane state ---
    bool show_tile_ledger = true; ///< Whether the Tile Ledger window is open. Toggled by the nav pane tab and the window's close button.
};
