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

/// Number of reserved faction colour slots. The data model already allows
/// multiple factions (unit_component.owner); these are their on-canvas colours.
inline constexpr int faction_slot_count = 6;

/// On-canvas identity colour for faction slot @p slot, wrapping modulo
/// faction_slot_count so any index is safe. Slot 0 is the player's corporation.
///
/// @param slot Faction index (player == 0).
/// @return     The faction's reserved colour.
ImU32 faction_colour(int slot);

/// Number of distinct nation identity colours. Larger than faction_slot_count
/// because a generated world holds many nations; the Faction-lens tile tint
/// keys off this palette via nation_colour.
inline constexpr int nation_slot_count = 12;

/// Stable on-canvas identity colour for a nation, keyed by its entity id. A
/// multiplicative (Knuth) hash spreads sequential ids across the palette so
/// neighbouring nations (often consecutive ids) rarely share or sit adjacent in
/// hue. Distinct from the faction palette: nations tint territory, factions mark
/// ownership.
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
