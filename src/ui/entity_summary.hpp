#pragma once

#include "ui_state.hpp"
#include "world/world.hpp"

#include <imgui.h>

/// @file
/// Shared per-entity-kind "stat block" content builders described in
/// docs/ui/SELECTION.md (§ Shared content builders).
///
/// Each function renders the compact inner ImGui widgets that summarise one
/// selected entity — name, type, and the headline fields for its kind. They emit
/// *content only* (no window, no tooltip frame), so a caller can place the block
/// inside whatever frame it owns: the pinned Selection info element, a Tile
/// Ledger row, or the hover card (hover_card.hpp). The frame differs per caller;
/// the content does not.
///
/// They lean on the existing presentation layer (presentation.hpp names, identity
/// colours, value_colour) and format.hpp number formatting, mirroring the deposit
/// and market idioms already in tile_inspector.cpp and body_surface_canvas.cpp.
///
/// Every builder tolerates a stale or missing id: if @p id is not present in the
/// relevant world component map it renders a disabled em-dash and returns, since
/// selections can outlive the entity they point at.

namespace ui {

/// Stat block for a celestial body: name, type, orbital radius, parent, grid.
///
/// @param w  Read-only world state.
/// @param id Entity id expected in @p w.bodies.
void draw_body_summary(const world& w, entity_id id);

/// Canvas-aware body stat block. Identical to @ref draw_body_summary but adds
/// a rung-relative distance field: on Solar the reference is the star; on
/// Circumplanetary the reference is the active parent body (@p s.active_body).
///
/// @param w  Read-only world state.
/// @param s  Current canvas/nav state (determines which rung is active).
/// @param id Entity id expected in @p w.bodies.
void draw_body_summary(const world& w, const ui_state& s, entity_id id);

/// Stat block for a tile: coordinates, composition × landform, hazard,
/// habitability, and its non-zero resource deposits with identity swatches.
///
/// @param w  Read-only world state.
/// @param id Entity id expected in @p w.tiles.
void draw_tile_summary(const world& w, entity_id id);

/// Stat block for a surface installation: type, host tile coordinates, and
/// assigned workforce percentage.
///
/// @param w  Read-only world state.
/// @param id Entity id expected in @p w.buildings.
void draw_building_summary(const world& w, entity_id id);

/// Stat block for a body's local market: the host body's name and a compact
/// per-resource supply / demand / price table.
///
/// @param w  Read-only world state.
/// @param id Entity id expected in @p w.markets (the market entity itself).
void draw_market_summary(const world& w, entity_id id);

/// Stat block for a deployable unit group: unit count and the body it occupies.
///
/// @param w  Read-only world state.
/// @param id Entity id expected in @p w.units.
void draw_unit_summary(const world& w, entity_id id);

/// Stat block for a nation: an identity-swatched name, controlled tile count,
/// ideology / expansionism / economic focus, and its top resources by abundance.
///
/// @param w  Read-only world state.
/// @param id Entity id expected in @p w.nations.
void draw_nation_summary(const world& w, entity_id id);

/// Stat block for a corporation: name (flagged when it is the player's), home
/// nation, industrial focus, starting capital, and starting asset count.
///
/// @param w  Read-only world state.
/// @param id Entity id expected in @p w.corporations.
void draw_corporation_summary(const world& w, entity_id id);

} // namespace ui
