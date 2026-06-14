#pragma once

#include "world/entity.hpp"

/// Which rung of the canvas zoom ladder currently fills the primary viewport.
/// The minimap shows the rung one step *out* (towards solar) from this.
enum class canvas_level
{
    solar,           ///< The whole system: the star and every body orbiting it.
    circumplanetary, ///< One planet and its moons / local space.
    planetary,       ///< One body's hex tile surface.
};

/// Which data overlay (if any) is drawn on top of the canvases. The minimap
/// mode bar toggles these; overlay_mode::none is the plain canvas. Reserved for
/// the economic/military lenses later layers add — Layer 5 supply routes are the
/// first hard requirement. See CANVASES.md ("What is deferred"), MINIMAP.md
/// (mode bar), and ui/overlay.hpp.
enum class overlay_mode
{
    none = 0,    ///< No overlay; the plain canvas.
    supply,      ///< Supply routes / convoy paths (Layer 5).
    market,      ///< Market / price lens.
    faction,     ///< Faction presence.
    corporation, ///< Corporate-owned tiles (per-corp tint; player-corp border). See LENSES.md.
};

/// Shared selection and view state for the three primary canvases.
///
/// Held by app and passed by reference to the canvas drawing functions. The
/// canvases read it to know what is selected and which rung is foregrounded,
/// and write to it in response to body clicks (descend), tile clicks, and
/// minimap clicks (ascend).
struct ui_state
{
    entity_id    active_body   = null_entity;         ///< Navigation anchor: drives the lower rungs (circumplanetary anchor and surface). Changed by *navigation* (double-click / focus), not by selection. null_entity = no anchor.
    entity_id    active_tile   = null_entity;         ///< Navigation anchor for the surface rung; set by a tile navigation. Distinct from the selection.
    entity_id    selected_entity = null_entity;       ///< The entity the player single-clicked to inspect — drives the Selection info element. Distinct from the active_* anchors: selecting never moves the canvas. null_entity = nothing selected. See SELECTION.md, ui/selection.hpp.
    entity_id    selection_hidden_for = null_entity;   ///< The selection the player dismissed with the panel's close button. The Selection info element stays hidden while selected_entity equals this; a *new* selection re-shows it. See SELECTION.md (close hides, does not destroy).
    canvas_level primary_level = canvas_level::solar; ///< Which canvas rung fills the window.
    overlay_mode overlay       = overlay_mode::supply; ///< Active canvas overlay lens; toggled by the bottom overlay control strip. Defaults to the supply lens (the first Layer 5 requirement) rather than none.

    // --- navigation pane state ---
    // Policy: all ledgers start closed. The player opens them deliberately from
    // the navigation pane; none are shown on a fresh session.
    bool show_tile_ledger = false; ///< Whether the Tile Ledger window is open. Toggled by the nav pane tab and the window's close button.
    bool show_economy_panel = false; ///< Whether the Layer 3 economy panel is open. Toggled by the nav pane tab and the window's close button.

    // --- solar system canvas view (primary only; the minimap always shows the
    // default framing) ---
    float solar_zoom  = 1.0f; ///< Scroll-wheel zoom factor. 1.0 = default auto-fit framing.
    float solar_pan_x = 0.0f; ///< Pan offset of the system centre from the canvas centre, screen px.
    float solar_pan_y = 0.0f; ///< Pan offset of the system centre from the canvas centre, screen px.

    // --- circumplanetary canvas view (primary only; the minimap always shows
    // the default framing) ---
    float circum_zoom  = 1.0f; ///< Scroll-wheel zoom factor. 1.0 = default auto-fit framing.
    float circum_pan_x = 0.0f; ///< Pan offset of the anchor centre from the canvas centre, screen px.
    float circum_pan_y = 0.0f; ///< Pan offset of the anchor centre from the canvas centre, screen px.

    // --- planetary canvas view (primary only; the minimap always shows the
    // default framing) ---
    float planetary_zoom  = 4.0f / 3.0f; ///< Scroll-wheel zoom factor. 4/3 shows 3/4 of the grid height on load.
    float planetary_pan_x = 0.0f; ///< Pan offset of the grid centre from the canvas centre, screen px.
    float planetary_pan_y = 0.0f; ///< Pan offset of the grid centre from the canvas centre, screen px.

    // --- pending centre request (verify harness) ---
    // verify.center_tile() sets these; the Planetary canvas consumes them on its
    // next draw, where the exact grid transform is known, and computes the pan that
    // centres the tile. This keeps the pan-centring math in one place (the canvas)
    // rather than replicated in Lua. See body_surface_canvas.cpp.
    bool planetary_center_pending = false; ///< True when a centre-on-tile request is waiting to be consumed.
    int  planetary_center_col     = 0;     ///< Grid column to centre; valid only while planetary_center_pending.
    int  planetary_center_row     = 0;     ///< Grid row to centre; valid only while planetary_center_pending.
};
