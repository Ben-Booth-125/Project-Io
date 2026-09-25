#pragma once

#include "format.hpp"
#include "world/components.hpp"

#include <imgui.h>

#include <string>
#include <utility>
#include <vector>

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

/// Display name for a tile's SUBSTRATE alone (e.g. "Rock", "Ice").
///
/// A noun, not an adjective, because `terrain_name` composes it with a cover
/// adjective. Use `terrain_name` for anything the player reads as "what is this
/// tile"; this one is for a ledger column that means the ground specifically.
const char* substrate_name(terrain_substrate s);

/// Display name for a tile's COVER alone (e.g. "Forest", "Snow"), or "None".
const char* cover_name(terrain_cover c);

/// One word for how heavy a cover is: "Sparse", "Moderate" or "Dense".
/// Returns nullptr when there is no cover to qualify.
const char* cover_density_word(std::uint8_t density);

/// What the player reads as "what is this tile" — the two axes composed into one
/// phrase (BL-519).
///
/// THE FAMILIAR NAMES SURVIVE. A player who knew "Grassland", "Forest",
/// "Wetland" still sees exactly those words: the four canonical covers on
/// sedimentary ground keep their pre-split label, because renaming terrain the
/// player already recognises would have been a cost the split never needed to
/// impose. What is new is that the pairs the old model could not express now
/// have names too — "Forested Rock", "Snowy Ice", "Dune Barrens".
std::string terrain_name(terrain_substrate sub, terrain_cover cov, std::uint8_t density);

/// Convenience overload reading the axes straight off a tile.
std::string terrain_name(const tile_component& t);

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

/// The NAMED building an `extraction_site` targeting @p r reads as — "Quarry"
/// rather than "Extraction: Stone" (BL-429). Falls back to the resource's own
/// display name for anything with no bespoke identity.
///
/// One vocabulary, three surfaces: the Build door offers these names, the
/// Construction ledger's Buildings view groups the player's estate under them,
/// and both mean the same building. It lives here beside `building_type_name`
/// rather than file-local to one of them, because a second copy is how the door
/// and the roster would come to call the same building two things.
const char* extraction_building_name(resource_type r);

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

/// One rung of a LINEAGE PALETTE (BL-919): the colour a culture is painted in
/// on the New World wizard's Culture round, given its place on the hue wheel
/// and its depth below its root cradle. The wheel rule lives here beside the
/// other identity palettes; the TREE walk that assigns each culture a hue
/// (root cradles spread evenly, each daughter a fixed step off its parent) is
/// `ui::build_lineage_palette` in history_lapse.hpp, which is the only caller.
///
/// WHY LIGHTNESS BY DEPTH. Hundreds of cultures under twelve identity colours
/// is plaid. A family must be recognisable at a glance and its members told
/// apart on a second look, so the family is the hue and the generation is the
/// lightness: a root is bright, each generation a fixed step darker, bounded at
/// `lineage_depth_cap` so a deep lineage stays readable over the sea backdrop.
///
/// @param hue   Position on the wheel, 0-1, wrapping.
/// @param depth Generations below the root cradle; 0 for a cradle culture.
ImU32 lineage_colour(float hue, int depth);

/// Deepest generation the lightness step still descends for; anything deeper
/// draws at this rung. Four rungs is the most the eye separates within one hue.
inline constexpr int lineage_depth_cap = 4;

/// Number of slots in the LAPSE POLITY palette — the colours the New World
/// wizard's Culture and Empires maps paint the recorded era's powers in
/// (BL-915). Larger than `nation_slot_count` because a recorded age holds
/// 24-60 powers and the map assigns slots by GREEDY COLOURING over the polity
/// adjacency graph rather than by hashing the id, so no two neighbours share a
/// hue: the palette must be wider than the largest neighbourhood the greedy
/// walk meets, and twelve was not.
inline constexpr int lapse_polity_slot_count = 20;

/// The lapse polity palette, by SLOT — the slot is what the greedy colouring
/// hands out (`ui::history_lapse::polity_slot`), never a polity id, so this is
/// a table lookup and carries no hash. Wraps modulo the count so any index is
/// safe. Sits here rather than in history_lapse.cpp because identity colour is
/// presentation's job; a sibling item derives CULTURE hue families from the
/// culture tree, and will feed a family into the slot assignment rather than
/// into this table.
///
/// @param slot Palette slot from the colouring.
/// @return     That slot's identity colour.
///
/// THE FALLBACK TABLE since BL-1087: a record with a culture tree paints its
/// realms by `polity_slot_colour` below, from the founding family's wedge;
/// this twenty-slot table is what a record with NO tree (no lineage to read a
/// wedge from) still colours by.
ImU32 lapse_polity_colour(int slot);

/// A REALM'S COLOUR FROM ITS FAMILY'S WEDGE (BL-1087; Ben, 2026-09-24, R7):
/// the slot encodes a hue-family wedge and an offset inside it
/// (`polity_slot_offsets`, world/polity_identity.hpp). The wedge is the founding
/// culture's root cradle's — the same wedge the lineage palette gives that
/// family, so kin realms and the culture base under them read as one hue —
/// and the offset nudges the hue inside the wedge and steps the lightness, so
/// two adjacent kin never share a swatch. Wedges alternate a base lightness
/// by parity, the CVD-safe axis, so two wedges whose hues collapse under a
/// deficiency still separate (the twelve-slot nation table's reasoning:
/// widen by lightness within safe hues, not by new hues). `rung` darkens the
/// colour one step per shade rung (R8's ratchet). With `family_count <= 0`
/// the slot indexes the fallback table above instead.
///
/// @param slot         The realm's slot from `assign_polity_identity`.
/// @param family_count The record's wedge count (`lineage_wedges::count`).
/// @param rung         Shade rungs earned (0 = none), clamped to two.
ImU32 polity_slot_colour(int slot, int family_count, int rung);

/// ONE RUNG DARKER (BL-1087, R8): the same colour with its value stepped down,
/// as the ratchet draws it. 0 returns @p c unchanged; clamped to two rungs.
ImU32 shade_rung(ImU32 c, int rung);

/// THE CULTURE BASE under the polity fill on the Empires, Exploration and
/// Industrialisation rounds (BL-1087, R7): the lineage hue of a region's
/// plurality people, DULL — low saturation, mid value — so the realm tint
/// over it stays the political read and the base is context. Same hue and
/// depth arguments as `lineage_colour`, so a family's base and its fill agree.
ImU32 culture_base_colour(float hue, int depth);

/// THE PER-WORLD NATION -> COLOUR TABLE (BL-1089; Ben, 2026-09-24, R6). A
/// nation's colour is its founding realm's — the slot the wizard's rounds
/// pinned by id — and this table, set at Begin and again on load from the
/// saved report, is what `nation_colour` reads first; the hash below is only
/// the fallback for an id the table does not hold (a nation of ownerless
/// ground, a hand-built harness world, a world with no history). One table,
/// so the national border band, the seat map, the loading carve and the Ages
/// view cannot disagree. Cleared with the world.
void set_nation_colour_table(const std::vector<std::pair<entity_id, ImU32>>& table);
void clear_nation_colour_table();
/// True when @p id is in the table (the colour is the realm's, not the hash).
bool nation_colour_pinned(entity_id id);

/// THE REALM -> COLOUR TABLE by polity id (BL-1089), set beside the nation
/// table from the same derivation, for the surfaces that hold a POLITY id
/// rather than a nation entity: the loading carve (before the entities
/// exist) and the Ages view's replay. @p found reports a miss; the colour
/// returned on a miss is the fallback table's slot for the id.
void set_realm_colour_table(const std::vector<uint32_t>& colour_by_polity);
ImU32 realm_colour(int polity, bool* found = nullptr);

/// Identity colour for a building **kind** — the colour a segment of the stacked-tile
/// ring is drawn in (`ui::icons::stack_ring`, PLANETARY.md § Building markers).
///
/// A KIND is `building_type`, not a named building: the ring answers "which kinds
/// stand here", and two extraction sites working different deposits are one kind
/// standing twice (the "+N" count badge is what says how many). The palette is a
/// hand-picked, hue-separated set rather than a hash, because the set is small,
/// closed and enumerated — a hash would put two adjacent hues on the one tile that
/// needs them told apart. Mid-to-light luminance throughout, so a segment reads over
/// dark terrain; the ring's own dark under-stroke carries it over light terrain.
///
/// Lives here rather than in icons.cpp because identity colour is presentation's
/// job and the glyph layer takes colour as a parameter (ICONS.md § Shared conventions).
///
/// @param type Building type.
/// @return     That kind's ring-segment colour.
ImU32 building_kind_colour(building_type type);

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
