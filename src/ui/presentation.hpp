#pragma once

#include "format.hpp"
#include "world/components.hpp"

#include <imgui.h>

namespace ui {

/// Display metadata for one resource type — the single source of truth for how a
/// resource is named and coloured wherever it appears (ledger headers, tile
/// deposits, tooltips, the header strip, and future overlays/icons). Replaces
/// the per-translation-unit resource_labels[] tables that previously duplicated
/// the names in tile_inspector.cpp and body_surface_canvas.cpp.
struct resource_presentation
{
    const char* name;   ///< Full display name, e.g. "Iron Ore".
    const char* abbrev; ///< Short label for tight columns and strips, e.g. "Fe".
    ImU32       colour;  ///< Identity colour for swatches, deposit bars, and icons.
};

/// Presentation metadata for @p r.
///
/// @param r Resource type to look up.
/// @return  Reference to the static metadata for @p r.
const resource_presentation& presentation_of(resource_type r);

/// Full display name for @p r. Convenience over presentation_of(r).name.
///
/// @param r Resource type to name.
/// @return  Null-terminated display name.
const char* resource_name(resource_type r);

/// Display name for a tile's material composition (e.g. "Grassland").
///
/// @param c Composition to name.
/// @return  Null-terminated display name.
const char* composition_name(terrain_composition c);

/// Display name for a tile's landform (e.g. "Mountain").
///
/// @param l Landform to name.
/// @return  Null-terminated display name.
const char* landform_name(terrain_landform l);

/// Display name for a celestial body type (e.g. "Planet", "Star").
///
/// @param t Body type to name.
/// @return  Null-terminated display name.
const char* body_type_name(body_type t);

/// Display name for a surface installation type (e.g. "Extraction Site").
/// Returns "None" for building_type::none.
///
/// @param t Building type to name.
/// @return  Null-terminated display name.
const char* building_type_name(building_type t);

/// Semantic palette — meaning-driven colours shared across the whole UI so the
/// data-dense surfaces read consistently and a restyle is a one-file change.
/// Grouped by role rather than by hue.
namespace palette {

// --- value direction (budget deltas, price moves, supply/demand balance) ---
inline constexpr ImU32 positive = IM_COL32(110, 200, 120, 255); ///< Profit, surplus, gain.
inline constexpr ImU32 negative = IM_COL32(216, 100,  96, 255); ///< Loss, deficit, drop.
inline constexpr ImU32 neutral  = IM_COL32(170, 175, 185, 255); ///< No change / not applicable.

// --- interaction states (the shared selection / hover / pinned convention) ---
inline constexpr ImU32 selection = IM_COL32(255, 255, 255, 255); ///< The selected entity.
inline constexpr ImU32 hover     = IM_COL32(120, 190, 255, 255); ///< The entity under the cursor.
inline constexpr ImU32 pinned    = IM_COL32(255, 200,  90, 255); ///< An Explorer-pinned entity.

// --- text legibility (BL-063 contrast audit, 2026-07-08) ---
// ImGui's default ImGuiCol_TextDisabled (mid grey 128,128,128) measures ~4.8:1
// against the dark theme's window background (StyleColorsDark, app.cpp) — it
// grazes WCAG AA's 4.5:1 floor with no margin, and several dim/secondary labels
// (header BALANCE/STOCKPILE/NET captions, the Selection element's Parent/Focus/
// Territory lines, the EXPLORER placeholder) read at or under that line once
// window transparency and font hinting are accounted for. text_secondary is the
// AA-safe replacement for those tokens (~10:1 against the dark theme) — apply it
// via ImGui::PushStyleColor(ImGuiCol_Text, text_secondary) in place of
// TextDisabled at a call site, rather than lightening TextDisabled itself, since
// TextDisabled is also used for genuinely inactive/greyed-out controls where the
// dimmer default is the correct affordance.
inline constexpr ImU32 text_secondary = IM_COL32(190, 194, 202, 255); ///< AA-safe dim/label text.

// --- civic (settlements; BL-083) ---
// Population-centre markers are civic-neutral: settlements are not corp-owned, so a
// corp tint would misread as ownership, and tier is carried by the glyph size, not
// colour. The host-nation tint is applied only under the Country lens.
inline constexpr ImU32 settlement = IM_COL32(216, 206, 182, 255); ///< Civic-neutral settlement marker (parchment).

// --- commercial-sphere activity fog (BL-089) ---
// The activity badge on the Solar canvas, distinct in hue from the survey badge
// (cyan magnifier). Keyed by activity_vis: known = fresh commerce, stale = gone
// cold (greyed), visible = a live lane or the player's own presence.
inline constexpr ImU32 activity_known   = IM_COL32(120, 205, 160, 255); ///< Fresh player route reaches it.
inline constexpr ImU32 activity_stale   = IM_COL32(140, 142, 150, 210); ///< Route gone cold (greyed).
inline constexpr ImU32 activity_visible = IM_COL32(150, 230, 190, 255); ///< Live lane / player presence.
inline constexpr ImU32 activity_corridor= IM_COL32(120, 205, 160, 130); ///< Lit trade corridor between bodies.

// --- activity fog as a dim shadow (BL-150) ---
// The activity fog (BL-089) is absence-by-default: an un-networked body is a plain
// astronomy dot, so with little player commerce the whole map reads as "no fog".
// BL-150 inverts that: a body outside the player's commercial network is drawn
// DIMMED (body + label brightness scaled below), and the dimmest tiers also get a
// translucent dark wash cast over the body, so the un-networked map reads as fogged
// and brightens as commerce reaches it. Indexed by activity_vis ordinal
// (unknown, known_stale, known, visible) — a monotonic ramp to full brightness.
// Kept visually distinct from the geographic survey fog (the '?' badge, BL-067):
// this is a brightness/shadow treatment, not a glyph. Home is always full-bright.
inline constexpr float activity_fog_brightness[4] = { 0.36f, 0.60f, 0.84f, 1.0f };
inline constexpr int   activity_fog_shadow_alpha[4] = { 105, 50, 0, 0 };

/// Scale an ImU32 colour's RGB channels by @p b (0..1), preserving its alpha. Used
/// for the activity-fog dim (BL-150): a dimmed body/label keeps its hue and opacity
/// but reads darker. @p b is clamped to [0, 1].
inline ImU32 dim_rgb(ImU32 c, float b)
{
    if (b > 1.0f) b = 1.0f;
    if (b < 0.0f) b = 0.0f;
    const int a = (c >> IM_COL32_A_SHIFT) & 0xFF;
    const int r = static_cast<int>(((c >> IM_COL32_R_SHIFT) & 0xFF) * b);
    const int g = static_cast<int>(((c >> IM_COL32_G_SHIFT) & 0xFF) * b);
    const int bl= static_cast<int>(((c >> IM_COL32_B_SHIFT) & 0xFF) * b);
    return IM_COL32(r, g, bl, a);
}

/// Number of reserved corporation colour slots. These are the on-canvas identity
/// colours for corporations (player vs. rivals); slot 0 is the player's corp.
/// (Renamed from faction_slot_count, BL-052 — this palette is corporation
/// identity, not nation territory, which keys off nation_colour.)
inline constexpr int corp_slot_count = 6;

/// Number of distinct geometric corp emblem shapes. A corporation's emblem is
/// (shape, identity colour); the shape is chosen deterministically from the corp
/// entity id (see corp_emblem_shape), so it is stable for a campaign and distinct
/// between corps. Shared across the identity card, the Selection header, the
/// on-canvas markers, and the rival hover card (BL-090).
inline constexpr int corp_emblem_shape_count = 6;

/// On-canvas identity colour for corporation slot @p slot, wrapping modulo
/// corp_slot_count so any index is safe. Slot 0 is the player's corporation.
///
/// @param slot Corporation index (player == 0).
/// @return     The corporation's reserved colour.
ImU32 corp_colour(int slot);

/// Deterministic emblem *shape* index for a corporation, in
/// [0, corp_emblem_shape_count). A multiplicative (Knuth) hash of the corp entity
/// id, so the shape is a pure function of identity — stable for a campaign, no RNG
/// or time. Pair with corp_identity_colour to form the full emblem (BL-090).
///
/// @param corp Corporation entity id.
/// @return     Emblem shape index for ui::icons::corp_emblem.
int corp_emblem_shape(entity_id corp);

/// Identity colour for a corporation — the single source of truth shared by the
/// on-canvas tile tint, building/HQ markers, the identity card, and the Selection
/// header. The player's corp (@p corp == @p player) is corp slot 0; a rival gets a
/// stable per-corp slot via a multiplicative hash, bumped off slot 0 so a rival
/// never collides with the player's colour (BL-090).
///
/// @param corp   Corporation entity id.
/// @param player The player corporation's entity id (world::player_entity).
/// @return       The corporation's identity colour.
ImU32 corp_identity_colour(entity_id corp, entity_id player);

/// Number of distinct nation identity colours. Larger than corp_slot_count
/// because a generated world holds many nations; the Country-lens tile tint
/// keys off this palette via nation_colour.
inline constexpr int nation_slot_count = 12;

/// Stable on-canvas identity colour for a nation, keyed by its entity id. A
/// multiplicative (Knuth) hash spreads sequential ids across the palette so
/// neighbouring nations (often consecutive ids) rarely share or sit adjacent in
/// hue. Distinct from the corporation palette: nations tint territory,
/// corporations mark ownership.
///
/// @param id Nation entity id.
/// @return   The nation's identity colour.
ImU32 nation_colour(entity_id id);

} // namespace palette

/// Semantic colour for a value's direction: positive/negative/neutral.
///
/// @param s Sign classification (see fmt::sign_of).
/// @return  The matching palette colour.
ImU32 value_colour(fmt::sign s);

/// Semantic colour for a signed value, classified via fmt::sign_of.
///
/// @param v The signed value (e.g. a price move or net income).
/// @return  positive/negative/neutral palette colour.
ImU32 value_colour(double v);

} // namespace ui
