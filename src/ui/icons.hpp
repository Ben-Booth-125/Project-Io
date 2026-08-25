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
/// BL-429: for `extraction_site` and `processing_facility`, @p identity picks a
/// NAMED-building glyph over the generic ore-chunk/square — the extraction
/// site's target resource, or a processing facility's PRIMARY OUTPUT resource
/// (see `primary_output_resource`, recipe_registry.hpp). A resource with no
/// bespoke glyph authored falls back to the generic shape, so this is always
/// safe to pass. Ignored for every other type.
///
/// @param dl       Draw list to render into.
/// @param centre   Glyph centre, screen pixels.
/// @param r        Half-extent (radius) of the glyph, screen pixels.
/// @param type     Building type to depict.
/// @param identity Extraction target / processing primary-output resource;
///                 ignored for non-extraction, non-processing types.
/// @param fill     Fill colour.
void building(ImDrawList* dl, ImVec2 centre, float r, building_type type,
             resource_type identity, ImU32 fill);

/// Draw the **stacked-tile ring** — one coloured arc per building KIND standing on
/// a tile, laid around the inside of the hex rim (BL-596). The dominant kind's own
/// building glyph draws in the centre, so the ring names the tile's contents while
/// the centre names which of them leads. Read CLOCKWISE FROM THE TOP: the first
/// segment (12 o'clock) is the dominant kind, i.e. the one the centre glyph depicts.
///
/// **Three things keep it off the rim's existing vocabulary** — the player-footprint
/// outline and, under the Country lens, the nation-border segments both live there.
/// The ring is a CIRCLE among hexagons (curved, never straight-sided), it is INSET to
/// 0.76 r so it clears both the hex edges and their midpoints, and it is BROKEN by a
/// gap between every segment, which no border pass ever is.
///
/// **Draws nothing below two kinds.** A single-kind tile is already fully described by
/// its centre glyph, and a ring on every built tile in the world would be chrome, not
/// information. The caller owns the level-of-detail gate (see PLANETARY.md § Building
/// markers): below it the tile degrades to the dominant glyph alone, never to nothing.
///
/// @param dl            Draw list to render into.
/// @param centre        Hex centre, screen pixels.
/// @param r             Half-extent — the hex circumradius the ring fits inside.
/// @param kind_colours  One colour per kind, dominant FIRST, then the remainder in a
///                      stable order. Source: palette::building_kind_colour.
/// @param kinds         Number of entries in @p kind_colours. Under 2 draws nothing.
void stack_ring(ImDrawList* dl, ImVec2 centre, float r,
                const ImU32* kind_colours, int kinds);

/// Draw a resource pip — a small filled diamond in the resource's identity
/// colour (see presentation_of). For resource strips and deposit markers.
///
/// @param dl     Draw list to render into.
/// @param centre Pip centre, screen pixels.
/// @param r      Half-extent of the pip, screen pixels.
/// @param res    Resource whose identity colour to use.
void resource(ImDrawList* dl, ImVec2 centre, float r, resource_type res);

/// Draw an under-construction marker — a crane silhouette — in @p colour. Drawn
/// IN PLACE OF a building's type silhouette while `ticks_remaining > 0` (BL-327,
/// replacing the BL-323 S4 desaturation treatment: dimming read as "faded", not
/// "being built"). Stroke-only, like the landform family — a filled silhouette
/// says "something is installed here", which is exactly the wrong claim for a
/// site that is not yet finished.
///
/// @param dl     Draw list to render into.
/// @param centre Marker centre, screen pixels.
/// @param r      Half-extent of the marker, screen pixels.
/// @param colour Stroke colour (the owner-tinted marker colour, so identity
///               still reads while the type does not).
void under_construction(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

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

/// Draw a BATTLE-IN-PROGRESS glyph — two crossed blades — in @p colour, over the
/// province a fight is being fought in (BL-469).
///
/// WHY IT EARNS ITS PLACE, since the deleted unit chevron (BL-294) is the
/// cautionary precedent for a marker that did not: this one is an ALERT, not a
/// label. A battle is the only thing on the map that is both time-limited and
/// reversible by a decision — the withdrawal window is open for about two ticks
/// — so a fight the player does not notice is a decision they never got to make.
/// A marker that pulls the eye is the difference.
///
/// Crossed blades rather than a flame or a burst: the vocabulary is hand-drawn
/// and literal everywhere else (a chart for a market, a chevron for a convoy),
/// and two things meeting at an angle is the plainest possible drawing of two
/// forces in contact.
///
/// @param dl     Draw list to render into.
/// @param centre Marker centre, screen pixels.
/// @param r      Half-extent of the marker, screen pixels.
/// @param colour Stroke colour. Identity colours live in presentation.hpp, never
///               here — the caller tints, so a player's own fight and a rival's
///               can read differently without this glyph knowing about either.
void battle(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

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
/// square (the inner dot), the extraction diamond, and the port triangle.
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

/// Draw the Continent-lens glyph — two interlocking plates split by a jagged
/// seam — in @p colour. For the overlay-lens control strip; reads as "the crust
/// is in pieces, and this is where they meet", which is the lens's whole subject.
/// The seam is the load-bearing shape: it distinguishes this from the Country
/// glyph (a bordered territory) and from any solid landmass blob, because what
/// the lens shows is the *boundary*, not the area. See LENSES.md § Continent
/// lens (BL-226).
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Fill colour of both plates.
void continent(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw the **Throughput** lens glyph (BL-605) — a **truck** in profile, facing
/// right: a long cargo box, a stepped-down cab with a raked windscreen, and two
/// wheels on the axle line. Filled silhouette with the family's dark outline, the
/// same idiom as `industry` (Ben, 2026-08-25: *"just use a truck as the glyph"*).
///
/// Two abstract cuts were tried first and both failed at strip size (~21px): a
/// funnel narrowing to a node read as a bowtie — an X, which is already the
/// "closed" affordance in this vocabulary — and a ringed node with flow stubs read
/// as a lone ring. A truck needs no decoding, which beats metaphorical fidelity
/// here.
///
/// Distinct from `convoy` (a bare chevron) and `supply` (two parallels) by being a
/// THING rather than a mark. The Throughput lens borrowed the convoy chevron while
/// it was keyboard-only; that stopped being tenable on the strip, where every
/// on-screen lens carries one distinct glyph (LENSES.md).
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Fill colour of the body and wheels; hubs take the dark outline.
void throughput(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw a **landform** marker for @p lf — the terrain-shape glyph family (BL-231).
/// Stroke-only, in @p colour, so it reads as *engraved terrain* rather than as one
/// more entity marker sitting on the tile; the caller picks a colour that contrasts
/// with the hex beneath (see `ui::contrast_ink`).
///
/// Only the four **dramatic** landforms draw: mountain, canyon, crater and rift.
/// Plains, highland and valley draw **nothing** — they are the common ground
/// (measured at ~95 % of land tiles between plains and valley alone) and are carried
/// by the relief tint in `hex_render`, not by a glyph. Drawing them would put an icon
/// on nearly every tile, which is far denser than any other glyph family and would
/// fight the building silhouette for the hex centre.
///
/// Silhouettes are chosen against the existing vocabulary: the mountain **range** is
/// stroke-only twin peaks with no baseline (so it never reads as the *filled* port
/// triangle or the production up-triangle); the canyon is a pair of rims split by a
/// narrow **incision**; the crater is a **flattened bowl** with a raised near rim (not
/// concentric circles — that is the activity pulse); and the rift is a single jagged
/// fissure, the only zigzag in the set.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param lf     Landform to depict; plains/highland/valley draw nothing.
/// @param colour Stroke colour.
void landform(ImDrawList* dl, ImVec2 centre, float r, terrain_landform lf, ImU32 colour);

/// True when @p lf is a **linear** landform whose contiguous runs should be bridged
/// into one spanning marker rather than drawn as a repeated per-tile glyph (BL-232).
/// Mountain, rift and canyon are lines; crater is a basin — a blob, not a line — so it
/// always keeps its centred glyph.
bool landform_spans(terrain_landform lf);

/// Draw **one half** of a spanning landform ridge: from a tile centre @p from to
/// @p mid, the midpoint of the edge it shares with a same-landform neighbour (BL-232).
///
/// This is BL-172's road span/symmetry idiom applied to terrain. Each tile draws only
/// its own half, so the neighbour's half meets this one exactly at @p mid — one
/// continuous marker, identical whichever tile is "from", needing no cross-tile state,
/// and clipping cleanly against the survey fog because a masked neighbour simply draws
/// nothing.
///
/// Each landform's span echoes the silhouette of its own centred glyph, so a run and a
/// lone tile read as the same feature: mountain draws **peaked teeth**, rift a **jagged
/// crack**, canyon **two parallel rims**. The waveform's perpendicular is canonicalised
/// (never derived from the direction of travel alone), so two halves meeting at @p mid
/// deflect to the *same* side instead of mirroring into a kink.
///
/// Roads use this exact geometry in warm tan, so these must not read as roads: the
/// caller supplies a contrasting ink and the shapes are deliberately non-smooth.
///
/// @param dl     Draw list to render into.
/// @param from   Tile centre, screen pixels.
/// @param mid    Shared-edge midpoint, screen pixels.
/// @param amp    Waveform amplitude, screen pixels (a fraction of the hex radius).
/// @param thick  Stroke thickness, screen pixels.
/// @param lf     Landform to depict; a non-spanning landform draws nothing.
/// @param colour Stroke colour.
void landform_span(ImDrawList* dl, ImVec2 from, ImVec2 mid, float amp, float thick,
                   terrain_landform lf, ImU32 colour);

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

/// Draw the Strategy-readout nav-rail glyph — a left axis stroke with three
/// left-anchored horizontal tally bars of descending length — in @p colour.
/// Reads as "counts compared": the axis anchor keeps it distinct from the
/// supply route lines and the resource strata (both anchorless), and the
/// horizontal orientation keeps it clear of the market lens's vertical bars.
/// Nav rail slot 12, the Strategy readout ledger (BL-411). See ICONS.md
/// § Nav-rail affordances.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Stroke colour of the axis and fill of the bars.
void readout(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

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

/// Draw the Contracts nav-rail glyph — a page with its top-right corner cut
/// away (a dog-ear fold) plus a short check mark near the bottom (a signed
/// document motif) — in @p colour. Distinct from `ledger` (a plain ruled
/// box) by the fold and the check, and from `history` (the hourglass) by
/// having a baseline rectangle at all. Nav rail slot 13, the Contracts
/// ledger (BL-576). See ICONS.md § Nav-rail affordances.
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Stroke colour of the page outline and the check mark.
void contract(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

/// Draw a unit marker — a humanoid silhouette (a filled circle "head" over a
/// triangle "body") in @p fill, with the standard dark outline — in @p fill.
/// Echoes `glyph_soldier`, the unit card's left-column placeholder
/// (`selection_panel.cpp`), so the canvas marker and the card read as the same
/// vocabulary; distinct from every building silhouette (diamond/square/
/// triangle/hexagon/shield) and from the market circle+cross.
///
/// @param dl        Draw list to render into.
/// @param centre    Marker centre, screen pixels.
/// @param r         Half-extent (radius) of the glyph, screen pixels.
/// @param fill      Fill colour — the owning corp/nation's identity colour.
/// @param committed BL-575 stub: when true, draws an additional outer ring
///                  marking a contract-committed unit. No writer sets this
///                  true yet (BL-573, a later wave of the same batch, adds
///                  the real per-unit flag) — the ring is wired now so that
///                  landing needs no further change here.
void unit_marker(ImDrawList* dl, ImVec2 centre, float r, ImU32 fill, bool committed);

/// Draw a market-centre marker — a small circle with a centred cross (+) — in
/// @p colour. Distinct from the building square/diamond/triangle glyphs.
/// Used as an on-canvas selectable marker for market entities
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

/// Draw a razed-settlement marker (BL-624's razed tier; BL-625) — the skyline
/// reduced to ruin: two hollow, outline-only tower shells of unequal height on a
/// faint rubble baseline, no fill. Reads as "a settlement stood here" without
/// reading as a live one; same civic-neutral colour contract as `settlement`
/// (the alpha dim is applied inside).
///
/// @param dl     Draw list to render into.
/// @param centre Glyph centre, screen pixels.
/// @param r      Half-extent of the glyph, screen pixels.
/// @param colour Civic colour; drawn dimmed, outline-only.
void settlement_razed(ImDrawList* dl, ImVec2 centre, float r, ImU32 colour);

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
