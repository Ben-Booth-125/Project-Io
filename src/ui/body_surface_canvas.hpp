#pragma once

#include "ui_state.hpp"
#include "world/economy_system.hpp" // economy_report (Production lens)
#include "world/hard_coded_world.hpp" // generation_report (Continent lens)
#include "world/recipe_registry.hpp" // recipe_registry (Production / Opportunity lenses)
#include "world/world.hpp"

#include <imgui.h>

namespace ui {

/// Draw the Body Surface Canvas into the given region using the ImGui
/// background draw list.
///
/// A pointy-top hex grid for state.active_body: each tile is a coloured
/// hexagon keyed to its terrain, buildings are marked with a small square,
/// and hovering a tile shows its full data. The player can pan (middle mouse)
/// and zoom (scroll wheel).
///
/// This is the bottom rung of the canvas ladder, so it is only ever drawn as
/// the primary viewport — never the minimap. All sizes are derived from @p size.
///
/// @param w             Read-only world state.
/// @param state         Shared UI state; mutated on tile click and pan/zoom.
/// @param reg           Recipe / economy registry — read where a lens or marker needs
///                      to value outputs and recipes.
/// @param report        Most recent economy-step report — the Industry lens reads
///                      per-building output quantities from it.
/// @param gen           The world's generation report (app::m_generation_report) — the
///                      Continent lens reads the active body's retained plate field from
///                      it. Presentation data, matched by body name (BL-226); the same
///                      source the Tile Ledger's biography reads.
/// @param origin        Top-left of the region, in screen pixels.
/// @param size          Width and height of the region, in screen pixels.
/// @param input_enabled When true, hover and click are processed. The caller
///                      disables input for whichever canvas the mouse is not over.
///
/// The active lens's key takes NO position argument (BL-602). It goes in the one lens
/// chrome region — the minimap's header, top right — which `ui::lens_chrome_rect`
/// owns, so the canvas neither knows nor passes where its legend lands.
void draw_body_surface_canvas(const world& w, ui_state& state, const recipe_registry& reg,
                              const economy_report& report, const generation_report& gen,
                              ImVec2 origin, ImVec2 size, bool input_enabled);

/// Refresh the intra-body vision model for state.active_body (BL-151/152/154). Rebuilds
/// state.permanent_vision (radius-2 pockets around the player's building tiles + 3-wide
/// corridors from the corp centre of operation to each market centre it operates in) and
/// state.convoy_beams (the tile path + progress/speed of each live player intra-body
/// convoy, for the render-time moving head/tail beam). Called every frame from the app
/// loop — it needs a non-const world for the (cached) pathfinder, so it cannot run in
/// the const-world draw; per-frame keeps it correct across a body switch, and the beam
/// paths are route-cache hits after the first build.
///
/// @param w        World (non-const: the intra-body pathfinder mutates its route cache).
/// @param state    Shared UI state; permanent_vision and convoy_beams are rebuilt.
/// @param now_days Current continuous sim time in elapsed days (stored as the beam clock).
void update_body_vision(world& w, ui_state& state, double now_days);

/// Refresh the Throughput lens's active-LP anchor pools for state.active_body
/// (BL-606; LOGISTICS.md § Logistic Points). A no-op unless that lens is active.
///
/// Here, and non-const, for exactly the reason update_body_vision is: the draw
/// holds a `const world&` and must not be the thing that populates a cache.
/// `active_lp_anchor_pools` is pure and uncached — it takes a mutable world only
/// because it may build the body's raster index — so this runs no Dijkstra and
/// no A*, and the reach field the lens's tile pass reads through the const
/// `tile_reach_cost` is warmed by the caller (app.cpp) a line earlier.
///
/// LP IS A PER-TICK RATE, NEVER A STOCK. What lands in `ui_state` is derived VIEW
/// state on the same footing as permanent_vision: rebuilt from scratch on every
/// call, never serialised, never in state_hash, never read back by the sim. It is
/// a photograph of this tick's rate, retaken each frame — it does not carry one
/// tick's points into the next.
///
/// @param w      World (non-const: the anchor enumeration may build the raster index).
/// @param state  Shared UI state; lp_anchors and its two reductions are rebuilt.
/// @param reg    Recipe registry — the authored `military().active_lp_per_anchor_tick`.
void update_body_throughput(world& w, ui_state& state, const recipe_registry& reg);

} // namespace ui
