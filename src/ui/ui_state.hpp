#pragma once

#include "world/components.hpp" // building_type / resource_type / sell_order
#include "world/entity.hpp"

#include <imgui.h>
#include <string>
#include <vector>

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
    market,      ///< Market / price lens (per-body price wash; see LENSES.md § Market lens).
    country,     ///< Country (nation) territory tint + owner borders. See LENSES.md § Country lens.
    corporation, ///< Corporate-owned tiles (per-corp tint; player-corp border). See LENSES.md.
    resource,    ///< Single-resource deposit lens: flat fill over the contiguous deposit. See LENSES.md § Resource lens.
    population,  ///< Per-tile habitability tint (dark → liveable green). See LENSES.md § Population lens.
    opportunity, ///< Per-tile best-building net-margin surface (red loss → green profit). See LENSES.md § Opportunity lens.
    production,  ///< Per-tile production-intensity surface (Σ output×price, log scale). See LENSES.md § Production lens.
    scarcity,    ///< Per-market supply-shortfall blocks (hot where demand outran supply). See LENSES.md § Scarcity lens.
    industry,    ///< Per-tile nation-substrate throughput field (occupation × terrain richness). See LENSES.md § Industry lens (BL-084).
};

/// A selectable on-canvas marker registered each frame by the Planetary canvas
/// draw pass. Hit-test priority (highest first): building > market_centre > tile.
/// Cleared and rebuilt every frame by body_surface_canvas. See BL-059, BL-031.
struct marker_hit_zone
{
    entity_id id = null_entity; ///< The entity this marker represents (building or market).
    enum class kind : uint8_t { building, market_centre, unit } kind = kind::building;
    ImVec2    centre{};         ///< Marker centre in screen pixels (this frame).
    float     radius = 0.0f;   ///< Hit-test radius in screen pixels.
};

/// Construction (building-placement) interaction state. When `active`, the
/// Planetary canvas enters placement mode: a ghost marker of `type` follows the
/// cursor, tinted by `placement_rules::can_place`, and the select-on-click is
/// suppressed; a click enqueues a construction request (see `pending_tile`) the
/// app executes against the world (build-cost spend + world mutation, via
/// construction.hpp). The per-tile build entry also lives on the tile Selection
/// element (the build front door, SELECTION.md).
struct construction_state
{
    bool          active = false;                          ///< Whether placement mode is engaged (set by the construction panel's Build section).
    building_type type   = building_type::extraction_site; ///< The building type being placed.
    resource_type target = resource_type::iron_ore;        ///< Extraction target for the placed building; meaningful only for extraction_site.

    /// Pending construction request — set by the build front door (the tile
    /// Selection element, SELECTION.md) or a placement-mode canvas click, and
    /// executed by `app::render` against the mutable world (the UI surfaces hold
    /// only `const world&`, so the mutation is centralised in app). `null_entity`
    /// = nothing pending; `pending_type` / `pending_target` carry the request.
    entity_id     pending_tile   = null_entity;
    building_type pending_type   = building_type::extraction_site;
    resource_type pending_target = resource_type::iron_ore;

    /// Last construction outcome, set by app after executing a request — a short
    /// human string shown by the build UI ("Built.", "Can't afford it.", …).
    std::string   last_message;

    /// Which bounded sub-view the Construction window shows (2026-07-06 tabbed
    /// redesign — Build / Manage / Sell Orders are three different questions,
    /// this is the nav selector between them). 0=Build, 1=Manage, 2=Sell Orders.
    int           panel_view = 0;
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
    overlay_mode overlay       = overlay_mode::none; ///< Active canvas overlay lens; toggled by the bottom overlay control strip. Defaults to none — the plain canvas — at campaign start (reverses BL-013's Corporation-default): a click never re-skins the canvas, so it opens unskinned. Single-select with a null state (re-clicking clears to none).

    // --- lens-local selector state (Resource / Market / Scarcity lenses) ---
    // One shared "which resource" selection drives the Resource lens's contiguous
    // deposit fill, the Market lens's price surface, and the Scarcity lens's
    // shortfall surface (LENSES.md says the selectors share a form). Only the
    // active lens reads it. The Resource lens is always single-resource (BL-019),
    // so it carries no highest-value toggle.
    resource_type lens_resource = resource_type::iron_ore; ///< Selected resource (Resource deposit / Scarcity shortfall) / good (Market price surface).

    // --- navigation pane state ---
    // Policy: all ledgers start closed. The player opens them deliberately from
    // the navigation pane; none are shown on a fresh session.
    bool show_tile_ledger = false; ///< Whether the Tile Ledger window is open. Toggled by the nav pane tab and the window's close button.
    bool show_economy_panel = false; ///< Whether the Layer 3 economy panel is open. Toggled by the nav pane tab and the window's close button.
    bool show_construction_panel = false; ///< Whether the Layer 4 construction / building-management panel is open. Toggled by the nav pane tab and the window's close button.
    bool show_market_ledger = false; ///< Whether the Market Ledger is open.
    bool show_balance_ledger = false; ///< Whether the Balance Ledger is open.
    bool show_corporation_panel = false; ///< Whether the Corporation Overview Dashboard is open.

    // --- application / system menu (BL-070) ---
    // The corner gear popup for session control (Pause/Resume, Exit Game). Opened
    // by the gear button and toggled by Esc; confirm_exit_pending arms the inline
    // "Really quit?" confirm before the destructive quit (there is no save). Drawn
    // and acted on in app::render, which owns the quit flag and the sim pause. See
    // docs/ui/MENU.md § Application / system menu.
    bool show_system_menu     = false; ///< Whether the corner system-menu popup is open.
    bool confirm_exit_pending = false; ///< Within the popup, whether Exit is armed to the "Really quit?" confirm.

    /// Per-frame list of on-canvas markers (buildings, market centres). Cleared at
    /// the top of body_surface_canvas each frame and rebuilt during the draw pass
    /// so click/hover handling can hit-test in priority order. See marker_hit_zone.
    std::vector<marker_hit_zone> marker_hit_zones;

    /// Building-placement interaction state (Layer 4 UI groundwork scaffold). See construction_state.
    construction_state construction;

    /// Pending survey dispatch (BL-067). The Selection-panel Survey button sets this
    /// to the body to survey; app::render executes dispatch_survey() and clears it,
    /// mirroring the construction request seam. Keeps the survey mutation in app while
    /// the UI surfaces hold a const world. null_entity = nothing pending.
    entity_id pending_survey_dispatch = null_entity;

    /// Player-authored standing sell orders (the manual market side). Re-evaluated
    /// every economy tick — passed to `clear_markets` by `app::step_economy`. Held
    /// here as player game-intent; authored from the construction / building-
    /// management panel. See sell_order, docs/SYSTEMS.md § Trade.
    std::vector<sell_order> sell_orders;

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

    // --- hover-card state (BL-060) ---
    entity_id hovered_entity = null_entity; ///< Entity the cursor rested on last frame; used to detect stable hover.
    int       hover_ticks    = 0;           ///< Consecutive frames of stable hover over hovered_entity; resets on entity change.

    // --- application-driven mouse (BL-061) ---
    // Canvases read this instead of ImGui::GetIO().MousePos so the cursor
    // position can be suppressed or overridden by the verify harness.
    // In the live app, render() copies the real OS cursor each frame (active=true).
    // In --verify, active defaults to false (hover suppressed); a script opts in
    // with verify.mouse(x,y) or verify.hover_tile(col,row).
    struct { float x = 0.f; float y = 0.f; bool active = false; } mouse;

    // --- pending centre request (verify harness) ---
    // verify.center_tile() sets these; the Planetary canvas consumes them on its
    // next draw, where the exact grid transform is known, and computes the pan that
    // centres the tile. This keeps the pan-centring math in one place (the canvas)
    // rather than replicated in Lua. See body_surface_canvas.cpp.
    bool planetary_center_pending = false; ///< True when a centre-on-tile request is waiting to be consumed.
    int  planetary_center_col     = 0;     ///< Grid column to centre; valid only while planetary_center_pending.
    int  planetary_center_row     = 0;     ///< Grid row to centre; valid only while planetary_center_pending.
};
