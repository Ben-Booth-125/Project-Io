#pragma once

#include "world/components.hpp"

#include <imgui.h>

namespace ui::icons {

/// Draw a building-type marker as a vector glyph centred at @p centre with
/// half-extent @p r. Each type has a distinct silhouette so a glance reads the
/// installation kind: extraction = faceted ore/mineral chunk (eight-sided,
/// wider than tall), processing = square, port = triangle, none = dot. Filled
/// in @p fill with a thin dark outline for contrast on any terrain.
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

/// Supply / convoy glyph — a rightward chevron (→) symbolising goods in transit.
/// Used as an on-canvas convoy marker in the Supply lens.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Stroke colour.
void convoy(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the Market-lens glyph — a three-bar ascending chart outline — in
/// @p colour. For the overlay-lens control strip.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Stroke colour.
void market(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the Country-lens glyph — a downward-pointing shield silhouette — in
/// @p colour. For the overlay-lens control strip. (Renamed from `faction`,
/// BL-052: the lens reads national territory.)
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Fill colour.
void country(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the Opportunity-lens glyph — an open circle with an inner "+" (potential
/// margin / where value could be made). For the overlay-lens control strip.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Stroke colour.
void opportunity(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the Production-lens glyph — a filled upward triangle over a baseline
/// (output / throughput rising). For the overlay-lens control strip.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Fill colour.
void production(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

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

/// Draw the Resource-lens glyph — a stack of three short horizontal strata of
/// increasing width (a gradient / deposit-density motif) — in @p colour. Reads
/// as layered density, distinct from the supply pair of full-width rules, the
/// market ascending bars, and the resource *pip* diamond. For the overlay-lens
/// control strip. See LENSES.md § Resource lens.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Fill colour of the strata (drawn at descending opacity).
void resource(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the Population-lens glyph — a small figure (round head over a tapered
/// torso) — in @p colour. Reads as "people / habitability", distinct from the
/// other lens glyphs. For the overlay-lens control strip. See LENSES.md
/// § Population lens.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Fill colour of the figure.
void population(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the Scarcity-lens glyph — a hollow downward-pointing triangle (an
/// "empty / depleted" motif, the inverse of the filled resource pip) — in
/// @p colour. For the overlay-lens control strip. See LENSES.md § Scarcity lens.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Stroke colour of the outline.
void scarcity(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the Industry-lens glyph — a factory silhouette (a block with a sawtooth
/// roof and a chimney) — in @p colour. For the overlay-lens control strip; reads as
/// "industry / throughput", distinct from the production up-triangle and the market
/// bars. See LENSES.md § Industry lens (BL-084).
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Fill colour of the silhouette.
void industry(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the History nav-rail glyph — an hourglass (a down-triangle over an
/// up-triangle meeting at a waist) — in @p colour. Slot 9 previously drew the
/// same @ref ledger glyph as the Budget slot, so the two were indistinguishable;
/// the joined pair is unmistakably "time" and collides with neither the hollow
/// scarcity down-triangle nor the filled production up-triangle, because it is
/// the meeting of the two that reads. See ICONS.md § Nav-rail affordances (BL-174).
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Stroke colour of both triangles.
void history(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the Research nav-rail glyph — an upward branching tree (a stem that
/// splits into two diagonals, each capped with a terminal node) — in @p colour.
/// Reads as "tech tree / lines of enquiry"; nothing else in the vocabulary
/// branches, so it stays distinct from the production up-triangle and the
/// market bars. Reserved-slot glyph, drawn dim. See ICONS.md § Nav-rail
/// affordances (BL-174).
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Stroke colour of the stem, branches and nodes.
void research(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the Corp. Strategy nav-rail glyph — a pennant on a pole (a vertical
/// staff with a filled right-triangle flag at its head) — in @p colour. Reads
/// as "a planted objective"; the flag hangs off the staff's top rather than
/// sitting on a baseline, which keeps it distinct from the production
/// up-triangle. Reserved-slot glyph, drawn dim. See ICONS.md § Nav-rail
/// affordances (BL-174).
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Stroke colour of the staff and fill of the pennant.
void strategy(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the Diplomacy nav-rail glyph — two overlapping circle outlines (a
/// two-parties-meeting motif) — in @p colour. Distinct from the single
/// market-centre circle and the concentric activity pulse because the overlap
/// is the point. Reserved-slot glyph, drawn dim. See ICONS.md § Nav-rail
/// affordances (BL-174).
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph pair, screen pixels.
/// @param colour Stroke colour of both outlines.
void diplomacy(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw a market-centre marker — a small circle with a centred cross (+) — in
/// @p colour. Distinct from the building square/diamond/triangle glyphs and the
/// unit chevron. Used as an on-canvas selectable marker for market entities
/// (BL-059, BL-031). For hit-zone display on the Planetary canvas.
///
/// @param dl     Draw list to render into.
/// @param centre Marker centre, screen pixels.
/// @param r      Half-extent (radius) of the glyph, screen pixels.
/// @param colour Stroke colour.
void market_centre(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw a settlement marker — a small skyline whose number of towers grows with
/// @p tier (1 = outpost … 5 = metropolis) — in @p colour. The civilisation read on
/// the Planetary canvas (population centres, BL-083); tier is carried by the glyph
/// size/complexity, not colour (see palette::settlement). Distinct from the building
/// square/diamond/triangle and the market-centre circle+cross.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param tier   Settlement scale 1–5 (clamped); drives the tower count and height.
/// @param colour Fill colour of the towers.
void settlement(ImDrawList* dl, ImVec2 centre, float r, int tier, ImU32 colour);

/// Draw the player's headquarters marker — a ringed eight-point star — in @p colour.
/// Distinguishes the player's HQ building from ordinary holdings on the Planetary
/// canvas (BL-085, folding BL-092). Distinct from the building glyphs, the market
/// circle+cross, and the settlement skyline.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the star, screen pixels.
/// @param colour Fill/stroke colour (the player identity colour).
void hq(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw a corporation identity emblem — one of corp_emblem_shape_count geometric
/// primitives (circle / square / triangle / diamond / hexagon / pentagon), chosen
/// by @p shape (see palette::corp_emblem_shape), filled in @p fill (the corp's
/// identity colour, see palette::corp_identity_colour). The shared faction-identity
/// glyph: it reads consistently at portrait size on the identity card, at header
/// size in the Selection panel, and as a small on-canvas identity tag beside a
/// building/HQ marker (BL-090). Distinct in *role* from the building silhouette,
/// the market circle+cross, and the settlement skyline — it names *whose* it is,
/// not *what* it is.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent (circumradius) of the emblem, screen pixels.
/// @param shape  Emblem shape index (wrapped modulo corp_emblem_shape_count).
/// @param fill   Fill colour (the corporation's identity colour).
void corp_emblem(ImDrawList* dl, ImVec2 centre, float r, int shape, ImU32 fill);

/// Draw the "unknown / unsurveyed" glyph — a question mark — in @p colour. Marks a
/// body whose surface and deposits are still hidden (survey system, BL-067). Shown
/// dimmed as the Solar-canvas survey badge for `survey_phase::hidden`.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent (radius) of the glyph, screen pixels.
/// @param colour Stroke colour.
void unknown(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the commercial-activity badge — a concentric pulse (a filled core ringed by
/// a signal ring) — in @p colour. Marks a body's activity-fog tier on the Solar
/// canvas (BL-089): fresh commerce, gone-cold, or a live lane. Deliberately distinct
/// from the survey magnifier and the unknown "?" so the two fogs read apart.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Stroke/fill colour (the activity-tier colour).
void activity(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the survey badge — a magnifying glass — in @p colour. Marks a body whose
/// survey is in progress (`in_transit` / `scanning`); the canvas overlays a small
/// progress read (k∕N) alongside it (survey system, BL-067).
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent (radius) of the glyph, screen pixels.
/// @param colour Stroke colour.
void survey_badge(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw a value-lens tile mark — a single filled dot in @p colour — for the
/// Workforce and Opportunity lenses (BL-135). These lenses read a per-tile
/// magnitude on a red→green scale (caller resolves the colour); the mark
/// replaces both the terrain-tint wash and, on occupied tiles, the building
/// glyph while either lens is active.
///
/// @param dl     Draw list to render into.
/// @param centre Mark centre, screen pixels.
/// @param r      Radius of the dot, screen pixels.
/// @param colour Fill colour (caller's red→green ramp sample).
void value_mark(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

} // namespace ui::icons
