#pragma once

#include "detail_level.hpp"     // fold_state — the drill-through disclosure target (BL-214)
#include "world/components.hpp"   // building_type / resource_type
#include "world/corp_command.hpp" // corp_command — the seam the ledger presses queue onto
#include "world/entity.hpp"

#include <imgui.h>
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vector>
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
    // `country` RETIRED (BL-601, Ben 2026-08-24): "National borders should not
    // diffuse together, instead they should borders extending their colour
    // inwards. With this, we can drop the nation lens." The lens dissolved
    // rather than being deleted — its content is always-on chrome now, the
    // national border band in body_surface_canvas.cpp, drawn like roads. The
    // selection route it owned (hovered tile → owning nation → Nation ledger)
    // moved onto the band itself: click the border, get the nation.
    // BL-1406 (Ben, 2026-10-23): the Corporation lens RETIRED with the
    // corporation (LENSES.md § Corporation and Company lenses — retired). The
    // slot is KEPT because the save's view bindings hold the enum's integer:
    // deleting it would re-point every later lens in an old save. A retired
    // slot is never drawn, never on the strip and skipped by the cycle
    // (`overlay_is_retired`); a save that held it loads as `none`.
    retired_corporation_, ///< RETIRED (BL-1406). Never a live lens.
    resource,    ///< Single-resource deposit lens: flat fill over the contiguous deposit. See LENSES.md § Resource lens.
    population,  ///< Per-tile habitability tint (dark → liveable green). See LENSES.md § Population lens.
    // Opportunity and Production were `overlay_mode` values here (BL-604, Ben
    // 2026-08-24: "retire the opportunity lens and the production intensity
    // lens"). Every lens costs a bar slot, a key, three doc rows and a
    // selection-routing rule permanently; these two were per-tile value
    // surfaces whose read the Scarcity and Industry lenses already carry
    // between them. The sentinel below is what the cycle counts, so the roster
    // re-numbers itself with no hand-kept constant to follow.
    scarcity,    ///< Per-market supply-shortfall blocks (hot where demand outran supply). See LENSES.md § Scarcity lens.
    industry,    ///< Per-tile nation-substrate throughput field (occupation × terrain richness). See LENSES.md § Industry lens (BL-084).
    reach,       ///< Body-level commercial reach: bodies connected via the corp's trade_route entries, tiered by recency. BL-011. See LENSES.md § Reach lens.
    continent,   ///< Tectonic plates from the Continents/Drift pass: per-plate tint + boundary emphasis, read from the generation report. BL-226. See LENSES.md § Continent lens.
    supply_routes, ///< Aggregated trade_route graph: one edge per body pair, thickness from log-scaled convoy_count, colour from recency tier. BL-014. See LENSES.md § Supply-routes lens.
    throughput,    ///< Active Logistic Points: the reach envelope (served / beyond reach) plus a per-anchor LP magnitude disc. BL-598. See LENSES.md § Throughput lens.
    // Ben, 2026-08-28: corporation and company are now DIFFERENT WORDS (GLOSSARY.md).
    // A corporation is a named operating firm — the player and its rivals; a company
    // is a BL-365 background firm (`corporation_component::is_background`). The
    // Corporation lens narrowed to the former on the same ruling, so this is the
    // only way to see the latter. Appended at the end rather than beside
    // `corporation` because the value is serialised in the save's view bindings —
    // inserting mid-enum would silently re-point a saved lens.
    // BL-1406: RETIRED with the company, slot kept for the same save reason.
    retired_company_,  ///< RETIRED (BL-1406). Never a live lens.
    trade_flow,    ///< The player's dispatcher: market-to-market shipment arrows + a marker per short market coloured by why the surplus did not go. BL-1222. See LENSES.md § Trade-flow lens.
    // The Nations lens (BL-1323, Ben 2026-10-14) is a NEW value, appended, never
    // a revival of the retired `country` above: the save's view bindings hold
    // the enum's integer, so re-using a slot would re-point a saved lens. It
    // paints each nation's whole territory; the border band stays the plain
    // map's chrome. See LENSES.md § The Nations lens.
    nations,       ///< Each nation-owned tile filled in its nation's identity colour. BL-1323.
    count,         ///< Sentinel — keep last. The lens-cycle wrap (canvas_command.cpp) derives its modulus from this, so a new lens above is reachable without touching a hand-kept count.
};

/// BL-1406: true for a lens slot kept only so a saved integer keeps its meaning.
/// A retired slot is never drawn; the cycle steps over it and a load maps it to
/// `overlay_mode::none`.
constexpr bool overlay_is_retired(overlay_mode m)
{
    return m == overlay_mode::retired_corporation_ || m == overlay_mode::retired_company_;
}

/// A selectable on-canvas marker registered each frame by the Planetary canvas
/// draw pass. Hit-test priority (highest first): unit > building > tile. (No
/// market-centre kind: its glyph and hit zone retired, Ben 2026-10-10.) Unit outranks building (BL-575) to match the repeat-click cycle's own
/// order (battle, unit, province, building, tile — SELECTION.md), so a unit
/// standing on a built tile is reachable on the FIRST click rather than only
/// after cycling past the building.
/// Cleared and rebuilt every frame by body_surface_canvas. See BL-059, BL-031.
struct marker_hit_zone
{
    entity_id id = null_entity; ///< The entity this marker represents (building, or — for `kind::unit` — the lowest-id unit in the province/owner group the marker draws).
    enum class kind : uint8_t { building, unit } kind = kind::building;
    ImVec2    centre{};         ///< Marker centre in screen pixels (this frame).
    float     radius = 0.0f;   ///< Hit-test radius in screen pixels.
};

/// The grain of a STRUCTURE selection: a region-scale entity the player selects
/// by its BOUNDARY rather than by anything sitting on a tile.
///
/// Deliberately a family and not a nation flag (BL-601, Ben 2026-08-24). The
/// national border band is its first member — the always-on band is what carries
/// a nation on screen now that the Country lens has retired, so the band is what
/// opens it — but nothing about the mechanism is nation-shaped: a plate boundary,
/// a market catchment rim or a corp territory edge is one more value here and one
/// more producer of zones, with the resolver and the click path untouched. BL-603
/// generalises exactly this pattern, which is why it is not a special case.
enum class structure_kind : uint8_t
{
    none,        ///< No structure under the pointer, or the lens has none.
    nation,      ///< A generated nation, selected by its national border band.
    market,      ///< A market and its whole catchment (Market and Scarcity lenses).
    // BL-1406: the corporation and company structures retired with their lenses.
    // The two NON-ENTITY structures (Ben, 2026-08-28, choosing option A). Both
    // are regions the lens already draws and the player could not select: a
    // deposit is every tile carrying the lens's resource, a plate is a Voronoi
    // region of `continent_state::plate_id`. Neither has an entity id, so
    // neither travels in `selected_entity` — they get their own fields below, on
    // the precedent BL-511 set with `selected_province` and BL-469 with the
    // battle triple. `structure_hit_zone::id` carries a SYNTHETIC key for these
    // (resource index / plate index, both +1 so 0 stays "none"), which is safe
    // because every comparison also tests the kind.
    deposit,     ///< A resource deposit — every tile carrying the lens's resource (Resource lens).
    plate,       ///< A tectonic plate (Continent lens).
    // THE GROUND, for a HOVER READ ONLY (Ben, 2026-10-20, BL-1353 option A). The
    // Throughput lens's field is the player nation's per-tile coverage, so the
    // pointer resolves to the tile itself and the card reads why the figure is what
    // it is. The id is the tile. A click on it is INERT, exactly as on ground the
    // lens has no answer for - the click path treats this kind as no answer.
    ground,      ///< The player nation's ground under the Throughput lens (hover only).
};

/// One segment of a structure's boundary, registered by the draw pass so the
/// click and hover paths can hit-test it.
///
/// A segment, not a point, because a boundary is a line — and a line is not
/// clickable at play zoom, so `half_width` gives it a real corridor, independent
/// of how thick the border is drawn. Cleared and rebuilt every frame by
/// body_surface_canvas, alongside `marker_hit_zones`.
struct structure_hit_zone
{
    entity_id      id   = null_entity;          ///< The structure entity (a nation, today).
    structure_kind kind = structure_kind::nation;
    ImVec2    a{};                              ///< Segment start, screen px (this frame).
    ImVec2    b{};                              ///< Segment end, screen px (this frame).
    float     half_width = 0.0f;                ///< Hit corridor half-width, screen px.
};

/// One (province, owner) GROUP of units, built each frame for the Planetary
/// canvas's unit marker (BL-575). Units sharing a province and an owner draw as
/// ONE marker with a count badge rather than one per unit/tile — the province is
/// now a unit's command grain (BL-511's march_unit retarget; corp_command.hpp),
/// exactly as the existing building "+N" stack badge already groups several
/// buildings sharing one tile.
struct unit_marker_summary
{
    entity_id owner       = null_entity; ///< The corp/nation entity owning this group.
    entity_id sample_unit = null_entity; ///< Lowest-id unit in the group — the marker's click/hover target (marker_hit_zone::id).
    int       count       = 0;           ///< Number of unit ENTITIES in the group (not summed manpower).

    // BL-1425: the counter's two bars, summed over the group's units.
    long long men           = 0; ///< Men standing (HP).
    long long establishment = 0; ///< The count the units are raised to (unit_establishment).
    long long ready_men     = 0; ///< Sum of men x readiness_permille: / men / 1000 is the men-weighted readiness.

    /// Contract-committed stub (BL-575, wave 1). Always false — no writer sets
    /// it true yet. BL-573 (wave 4 of the same batch) adds the real per-unit
    /// committed flag this will fold from; left here now, with the ring already
    /// wired to read it in icons::unit_marker, so BL-576 (the contracts ledger)
    /// needs no further UI plumbing change once that flag exists.
    bool      committed   = false;
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

    /// Processing recipe the request was priced on (BL-162). The construction ledger
    /// lists one row per recipe, so the recipe the player chose has to survive to
    /// `construct_building`; `no_recipe` (the default, and what a canvas placement
    /// leaves) falls back to the seeded "steel". Cleared with `pending_tile`.
    std::uint16_t pending_recipe = no_recipe;

    /// Pending road-placement request (BL-147 core, BL-172 tier) — set by the build front door's
    /// Track/Road/Highway affordances and executed by `app::render` via `place_road` (a road is a
    /// per-tile mutation, not a building, so it takes a distinct path from `pending_tile`).
    /// `null_entity` = nothing pending; `pending_road_tier` carries which tier (1/2/3) to place.
    entity_id     pending_road_tile = null_entity;
    std::uint8_t  pending_road_tier = 1;
    /// BL-1370: the hex SIDE of `pending_road_tile` the crossing runs across —
    /// a road is an edge, so the request names the neighbour (LOGISTICS.md § 4).
    /// `k_road_side_auto` is no longer written by any player surface: the armed
    /// second press resolves a real side, and the auto guess is what made a
    /// roaded tile answer "this tile already has an equal or better road".
    std::uint8_t  pending_road_side = k_road_side_auto;

    /// ARMED ROAD TARGETING (BL-1370; construction.md § "A road is built from a
    /// road to a tile, in two presses"). The road row's Build press arms rather
    /// than builds: it records the tile the crossing starts FROM and the tier,
    /// the Planetary canvas highlights every neighbour
    /// `placement_rules::can_place_road` admits, and the next press on one of
    /// them resolves the side into `pending_road_*` above. A press anywhere else
    /// disarms. `null_entity` = not armed. VIEW state, not serialised.
    entity_id     road_arm_tile = null_entity;
    std::uint8_t  road_arm_tier = 1;

    /// BL-1255 (rail construction UI). Rail is built from the per-tile building
    /// list like anything else (LOGISTICS.md 4c). `pending_track_tile` is a Track
    /// row's Build press (one lay_track for the seat nation); `pending_station_tile`
    /// a Railway Station row's (build type 11 for the nation). app::render executes
    /// both through apply_corp_command; `rail_message` words the last outcome.
    entity_id              pending_track_tile   = null_entity;
    entity_id              pending_station_tile = null_entity;
    std::string            rail_message;

    /// Pending hire-unit request (BL-324) — set by the build front door's Hire
    /// affordance (the construction ledger's roster section) and executed by
    /// `app::render` via `apply_corp_command`'s `hire_unit` verb. Same deferred
    /// path as `pending_tile`, for the same reason (const-world UI surfaces).
    /// `null_entity` = nothing pending; `pending_hire_unit_type` is an index
    /// into `unit_roster_table()`.
    entity_id     pending_hire_tile      = null_entity;
    std::uint16_t pending_hire_unit_type = 0;

    /// Pending demolition request — set by the building Selection element's Demolish
    /// control and executed by `app::render` via `demolish_building`. Takes the same
    /// deferred path as `pending_tile` for the same reason (UI surfaces hold only
    /// `const world&`), and additionally because erasing from `w.buildings` while a
    /// draw pass is iterating it would invalidate the iteration.
    entity_id     pending_demolish = null_entity;

    /// Pending withdraw-from-battle request (BL-469) — set by the battle card's
    /// Withdraw press and executed by `app::render` via `apply_corp_command`'s
    /// `withdraw_from_battle` verb. Same deferred path as `pending_tile`, for the
    /// same reason (UI surfaces hold only `const world&`).
    ///
    /// Both fields are needed: a corp can be in more than one battle in one
    /// province, so the counterparty NAMES which fight is being left. 0 / null =
    /// nothing pending.
    uint32_t      pending_withdraw_province = 0;
    entity_id     pending_withdraw_against  = null_entity;

    // (BL-343's pending_law_toggle lived here until BL-480: enactment is the
    // author nation's act now, so no player surface enqueues a law flip.)

    /// Last construction outcome, set by app after executing a request — a short
    /// human string shown by the build UI ("Built.", "Can't afford it.", …).
    std::string   last_message;

    /// Which bounded sub-view the nav-rail slot-3 **Construction ledger** shows.
    ///
    /// **0 = Construction** — the collapsible build QUEUE ("what is under way, and
    /// what is it costing?") sitting above the tile-selection build bar
    /// (`draw_construction_ledger_body`, selection_panel.cpp). ONE construction
    /// element with TWO DOORS: the rail slot and the tile Selection element's
    /// Construct button both open this view, so there is a single build bar
    /// implementation rather than a panel copy and a card copy.
    /// **1 = Buildings** — the player's estate grouped by building type, one row
    /// per type with a count and a total profit; expanding a row lists that
    /// type's own buildings, and pressing one selects it and draws its levers.
    ///
    /// Defaults to **Buildings**: the queue is empty most of the time, so opening
    /// on it makes the ledger's front door an empty room, while the player always
    /// owns buildings.
    int           panel_view = 1;

    /// Which type group the Buildings view has expanded — the group's display
    /// name, empty for none. Held here rather than left to ImGui's own
    /// `CollapsingHeader` storage so it is one-at-a-time (an accordion), stable
    /// across a re-layout, and drivable/readable by a verify script. Re-pressing
    /// the open header closes it, which is the Toggle rule a header obeys by
    /// construction. VIEW state, not serialised.
    std::string   buildings_expanded;

    /// Whether the Construction view's queue section is expanded. Collapsed by
    /// default and collapsed most of the time, because the queue is usually
    /// empty: closed it costs one line, and the build bar below it is then what
    /// the player meets. VIEW state, not serialised.
    bool          queue_open = false;
};

/// One construction-grid COLUMN as it was drawn this frame (BL-1368): which
/// column it is, the explainer its own hover card carries, and the centre of its
/// cell so a check can hover that column and nothing else.
///
/// A construction row is one line of glyphs and abbreviations and each column
/// explains itself in the hover, so neither half is provable from a capture: the
/// surface reports its own columns instead. VIEW state, not serialised.
struct ui_grid_col
{
    std::string key;         ///< "name", "cost", "makes", "stack", "state", "bar", "act", "eta", "count".
    std::string text;        ///< That column's explainer — one or two short sentences.
    float       x = -1.0f;   ///< The cell's centre, screen px.
    float       y = -1.0f;
};

/// Where the Construction ledger's own controls landed on screen this frame, so a
/// verify script can press the REAL control rather than a computed guess.
///
/// PUBLISHED RATHER THAN RE-DERIVED, for the same reason `planetary_center_screen` is:
/// the surface that drew the thing is the only honest source for where it landed,
/// and a script re-deriving a row's y from `shell_column_width` plus a guessed line
/// count would be asserting against its own arithmetic. Every field is rewritten
/// each frame the surface draws and reset to "absent" (`x < 0`) when it does not.
/// VIEW state, not serialised.
struct construction_controls
{
    float     rail_track_x   = -1.0f; ///< BL-1255: the tile list's Track row Build press.
    float     rail_track_y   = -1.0f;
    float     rail_station_x = -1.0f; ///< BL-1255: the Railway Station row's Build press.
    float     rail_station_y = -1.0f;
    float     tab_x[2]      = {-1.0f, -1.0f}; ///< The two tab buttons; index = panel_view (0 Construction, 1 Buildings).
    float     tab_y[2]      = {-1.0f, -1.0f};

    float     group_x       = -1.0f; ///< First type-group header row on the Buildings view.
    float     group_y       = -1.0f;

    float     member_x      = -1.0f; ///< First building row inside the EXPANDED group; absent when nothing is expanded.
    float     member_y      = -1.0f;
    entity_id member        = null_entity; ///< Which building that row selects.

    /// The TILE Selection card's Construct button — door two onto the same
    /// Construction view the rail slot opens. Published so a check can press the
    /// real control and assert that both doors land on one surface, which is the
    /// load-bearing half of the reconciliation. Absent (`x < 0`) when no tile card
    /// draws, or when nothing is placeable on the tile (the button disables).
    float     construct_x   = -1.0f;
    float     construct_y   = -1.0f;

    /// The building Selection card's fourth action-grid button — "open this
    /// building in the Buildings ledger". Absent (`x < 0`) on a rival's card,
    /// which is the property a check asserts as a positive absence.
    float     open_ledger_x = -1.0f;
    float     open_ledger_y = -1.0f;

    /// Which building the Buildings view actually DREW LEVERS FOR this frame,
    /// `null_entity` for none. The surface reporting what it drew, so "the levers
    /// appeared" is an assertion rather than an inference from a capture — the
    /// harness's clipping check is vacuous on this class of surface (NR-663), so a
    /// green picture proves nothing on its own.
    entity_id levers_for = null_entity;

    /// The Build door's processing rows as the door LISTED them this frame: the
    /// ABSOLUTE recipe ids that survived its filters (era, tech, the body's air),
    /// and the tile they were listed for (`null_entity` when the door did not
    /// draw). The door folds rows by building group, so a capture cannot show
    /// which recipe sits behind a folded row; this is the surface reporting its
    /// own list, read by the `air_gate.lua` check. VIEW state, not serialised.
    std::vector<std::uint16_t> door_recipes;
    entity_id                  door_tile = null_entity;

    /// BL-1365: one entry per construction GRID row drawn this frame — the build
    /// list's rows and the queue's. A one-line row carries its figures as glyphs
    /// and abbreviations, so a capture cannot prove the words survived; the
    /// surface publishes its own hover-card text instead, and a check hovers the
    /// REAL row at (`x`, `y`) and asserts the card names the stall's good.
    /// VIEW state, not serialised.
    struct grid_row
    {
        std::string name;        ///< The row's name, unelided.
        std::string hover;       ///< Every column's explainer, newline-joined.
        float       x = -1.0f;   ///< A point inside the row, in screen pixels.
        float       y = -1.0f;
        /// The height this row was DRAWN at (BL-1366): the rail rows are a
        /// different draw site from the build list's, so "they are the same grid"
        /// is only an assertion if each row reports its own metric.
        float       h = 0.0f;
        /// BL-1371: how many lines the NAME column drew (1 or 2), and whether it
        /// still had to elide. The two-line row exists to stop a name losing its
        /// tail, so "it wrapped rather than eliding" is the assertion — and a
        /// capture cannot make it, because the elided and the wrapped row are the
        /// same height by construction.
        int         name_lines  = 0;
        bool        name_elided = false;
        /// BL-1368: one entry per COLUMN, in drawn order — the hover is per column
        /// now, so a check hovers each cell's own centre and asserts that column's
        /// own explainer. `hover` above stays the union of these, which is the one
        /// assertion that nothing was lost when the row-wide card was split.
        std::vector<ui_grid_col> cols;
    };
    std::vector<grid_row> queue_rows; ///< The in-flight queue's rows.
    std::vector<grid_row> door_rows;  ///< The build list's rows, in drawn order.
    /// BL-1366: the two RAIL rows (Track, Railway Station) as grid rows. Separate
    /// from `door_rows` only because they are drawn by rail_tool.cpp outside the
    /// candidate loop; they are the same grid, the same metric and the same
    /// per-column cards, which is the property the check asserts.
    std::vector<grid_row> rail_rows;
    float row_h  = 0.0f; ///< The grid's row height as drawn (two lines plus padding, BL-1371).
    float line_h = 0.0f; ///< One text line, so a check can assert row_h is N of them.

    /// BL-1370: the road ladder's three rows as they drew this frame — where the
    /// press landed, what it charges, and whether it will arm at all. A press
    /// that ARMS has no visible result of its own (the result is a highlight on
    /// the canvas), so a check has to press the real control and then read the
    /// armed state back; this is where it finds the control.
    struct road_row
    {
        std::uint8_t tier    = 0;     ///< 1=Track, 2=Road, 3=Highway.
        std::string  name;            ///< "Track" / "Road" / "Highway".
        float        x       = -1.0f; ///< The Build press's centre, screen px.
        float        y       = -1.0f;
        float        cost    = 0.0f;  ///< Credits the press charges (materials priced in).
        bool         enabled = false; ///< Whether the press will arm (targets + funds).
        int          targets = 0;     ///< How many crossings the rules admit at this tier.
        std::string  explainer;       ///< The press's own one-or-two-sentence card.
    };
    std::vector<road_row> road_rows;

    /// BL-1370: the armed targeting state as the CANVAS resolved it this frame —
    /// the tile the crossing starts from, the tier, and the grid position of every
    /// tile it highlighted. Resolved from the rules BEFORE the draw loop, so the
    /// set is independent of which tiles happened to be on screen; `drawn` counts
    /// how many of them the loop actually outlined.
    struct road_highlight
    {
        int x = 0, y = 0; ///< The target tile's grid position.
        int side = -1;    ///< The armed tile's hex side the crossing runs across.
    };
    std::vector<road_highlight> road_highlights;
    entity_id    road_arm_tile = null_entity; ///< Armed tile, null when not armed.
    std::uint8_t road_arm_tier = 0;
    /// How many target outlines the draw loop actually emitted - one per visible
    /// WRAP COPY of a target tile, so it is a "the mark was drawn" count rather
    /// than a second copy of `road_highlights.size()`.
    int          road_drawn    = 0;

    /// THE BUILD-PROGRESS CIRCLE (Ben, 2026-10-26; construction.md § The progress
    /// circle): one entry per circle the Planetary canvas drew this frame over a
    /// player site under construction, nearest wrap copy first, in building-id
    /// order. `x`/`y` are SCREEN pixels (after the camera squash), so a check can
    /// hover the real mark. VIEW state, not serialised.
    struct site_mark
    {
        entity_id building = null_entity;
        int   col = 0, row = 0;   ///< The site's tile.
        int   days_done  = 0;     ///< Days of work built.
        int   days_total = 0;     ///< Days the whole build takes at full rate.
        float x = -1.0f, y = -1.0f;
        float r = 0.0f;           ///< Drawn radius, ground px.
    };
    std::vector<site_mark> site_marks;
    /// The site card drawn this frame (the circle's hover or the queue row's),
    /// as text: the building whose card it was and every line it carried, so a
    /// check asserts it names the materials. Empty when no site card drew.
    entity_id   site_card_for = null_entity;
    std::string site_card_text;
    int         site_card_frame = -1; ///< `ImGui::GetFrameCount()` when it drew.
    /// The levers' measured height last frame, so the Buildings roster takes the
    /// whole tab less exactly what the levers below it need (Ben, 2026-10-26).
    float       levers_h = 0.0f;
    /// The roster's drawn height and the tab height it had to fill, this frame.
    float       roster_h = 0.0f;
    float       roster_avail = 0.0f;
};

// --- The SITE FINDER (Ben, 2026-10-25; docs/ui/ledgers/construction.md § The site
// --- ledger; LENSES.md § The placement lens) ----------------------------------
// BUILDING FIRST: the player picks a building in the Construction view, the
// Planetary canvas marks every tile of the player's own ground the Build door would
// accept it on (and flags the ones no Logistic Point source reaches), and a ledger
// lists those tiles sorted by estimated profit. Everything here is VIEW state: the
// choice survives a frame, the rest is recomputed from the world and never saved.

/// One tile the site ledger lists - a tile the Build door accepts the chosen
/// building on, priced exactly as the door prices it.
struct site_row
{
    entity_id     tile     = null_entity;
    int           x = 0, y = 0;    ///< Grid position (framing, and the checks; never on screen).
    /// The row's identity on screen (Ben, 2026-10-26: no x,y column): the named
    /// population centre whose province holds the tile, or the nearest one when
    /// its province has none (`place_near`).
    std::string   place;
    bool          place_near = false;
    bool          priced   = false; ///< `estimate_prospective_profit(...).has_data`.
    float         profit   = 0.0f;  ///< Est. net / qtr, Logistic Point cover applied.
    float         profit_full_cover = 0.0f; ///< The same estimate at cover 1.
    float         capex    = 0.0f;  ///< `construction_capex` - what the press charges.
    float         flat     = 0.0f;  ///< The flat build cost x the site multiplier (the credits half).
    float         site_mult = 1.0f; ///< `construction_site_multiplier` (landform x reach x stack).
    entity_id     market   = null_entity;
    std::string   market_name;
    float         cover    = 0.0f;  ///< `lp_route_to(...).cover`, [0,1].
    bool          lp_blocked   = false; ///< A source reaches it but has no chain to the capital.
    bool          no_logistics = false; ///< cover == 0: no Logistic Points operate here.
    bool          has_yield = false;
    float         yield     = 0.0f; ///< Units of what it makes per qtr, at this cover.
    float         deposit   = 0.0f; ///< Extraction: the tile's richness of the target.
    float         remaining = 0.0f; ///< Extraction: the tile's reserve left.
    bool          depositless = false; ///< A Well or a Fishing Wharf: no deposit, never tapers.
};

/// One building the finder offers - in the Build door's own vocabulary.
struct site_option
{
    building_type type   = building_type::extraction_site;
    resource_type target = resource_type::iron_ore;
    std::uint16_t recipe = 0xFFFFu; ///< `no_recipe` unless a processing facility.
    std::string   name;             ///< The door's row name ("Iron Mine", "Bloomery", "Port").
    std::string   category;         ///< The door's fold ("Extraction", "Processing", ...).
};

struct site_finder_state
{
    // The choice (persists across frames; VIEW state, not serialised).
    bool          armed = false;    ///< A building is chosen.
    site_option   choice;

    /// Sort column (0 place, 1 profit, 2 cost, 3 market, 4 logistics, 5 yield) and
    /// direction. Default: profit, descending; every tie broken by tile id ascending.
    int           sort_col  = 1;
    bool          sort_desc = true;

    // Published by the evaluation, rebuilt only when its key moves.
    bool          live = false;     ///< The lens draws THIS frame (ledger on the Construction view, a building chosen).
    entity_id     body = null_entity;
    int           gw = 0, gh = 0;
    /// Per grid cell (y * gw + x): 0 = not a site for this building (dimmed),
    /// 1 = a site with Logistic Points operating, 2 = a site with none (flagged).
    std::vector<std::uint8_t> cls;
    std::vector<site_row>     rows; ///< In the ledger's CURRENT sort order.
    int           own_tiles    = 0; ///< Tiles of the player's own ground on this body.
    int           unaffordable = 0; ///< Tiles that would take it but cost more than the treasury.
    int           no_lp_rows   = 0; ///< Rows flagged no-logistics.
    double        eval_ms      = 0.0; ///< Wall time of the last evaluation.
    int           evaluations  = 0;   ///< How many times the cache rebuilt (proves the cache holds).

    /// THE TIE MEASUREMENT (for Ben's province-price decision): rows whose
    /// displayed profit (whole credits) equals another row's - at full cover, at the
    /// real cover, and with the cost as a second key.
    int           ties_full_cover  = 0;
    int           ties_profit      = 0;
    int           ties_profit_cost = 0;
    int           covered_rows     = 0; ///< Rows with cover > 0.
    int           ties_covered     = 0; ///< Ties on net among those rows alone.

    /// Where the ledger's controls landed this frame (absent: x < 0).
    float         picker_x = -1.0f, picker_y = -1.0f;
    struct drawn_row { entity_id tile = null_entity; float x = -1.0f, y = -1.0f; };
    std::vector<drawn_row> drawn;   ///< Visible rows, in drawn order.
    int           map_marks = 0;    ///< No-logistics marks the canvas drew this frame.
    /// The class the canvas WASHED the selected tile with this frame (-1: it is
    /// not on the active body or the lens is down). A selected site keeps its
    /// "build here" wash under the selection mark (Ben, 2026-10-26).
    int           selected_wash = -1;
    /// Each header as drawn: its label, the width it needs unelided (text plus
    /// the sort arrow) and the width the column gave it. Every header must fit.
    struct header_fit { std::string label; float need = 0.0f, have = 0.0f; };
    std::vector<header_fit> headers;
};

// --- Baked ground view (BL-732, docs/ui/RENDERING.md) ----------------------
// The frame contract between the SDL-side chunk cache (core/ground_layer) and
// the Planetary canvas. app fills `ui_state::ground` each frame from the cache;
// the canvas draws the listed textures under everything and writes
// `ui_state::ground_request` (the canonical-space rect it can see) for the
// cache to bake against NEXT frame. Plain types only — no SDL in this header;
// `tex` is the ImTextureID the SDLRenderer3 backend takes (an SDL_Texture*).

/// One baked chunk, ready to draw: canonical-space rect + its texture.
struct ground_chunk_view
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0; ///< Canonical units (hex circumradius = 1).
    void* tex = nullptr;
};

/// Everything the canvas needs to draw the baked ground for one body: the far
/// page under everything, plus the drawn LEVEL's chunks over it — one level of
/// the body's one master (RENDERING.md § Level of detail, BL-1246).
struct ground_view
{
    entity_id body = null_entity;    ///< Body the bake describes; canvas ignores a mismatch.
    double    level_ppr = 0.0;       ///< Drawn level's px per hex circumradius (0 = far page only).
    std::vector<ground_chunk_view> chunks; ///< The drawn level's chunks on the GPU, in view.
    ground_chunk_view far;           ///< Low-res whole-body page (tex null until baked).
    bool far_ready = false;
    /// BL-1241: the Selection band's neighbourhood view, baked as its own small
    /// page around `neigh_tile` (tex null until baked) — so the zoomed view shows
    /// the same baked ground, structures included, as the canvas.
    ground_chunk_view neigh;
    entity_id neigh_tile = null_entity;
};

/// The Selection band's neighbourhood-page request (BL-1241): the tile the
/// view centres on and its ring radius. Written by the panel each frame it
/// draws the view; read by ground_layer next frame.
struct ground_neigh_request
{
    entity_id body = null_entity;
    entity_id tile = null_entity;
    int col = 0, row = 0, radius = 2;
    bool valid = false;
};

/// The canvas's bake request: the canonical-space rect visible this frame plus
/// the drawn hex radius (which picks the level). Read by ground_layer next
/// frame — one frame of latency, covered by the far page / vector fallback.
/// No tilt field: the camera has one angle (RENDERING.md § One angle).
struct ground_request
{
    entity_id body = null_entity;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    float draw_r = 0;                ///< Drawn hex circumradius, screen px.
    bool  valid = false;
    /// The Logistics lens is up (Ben, 2026-10-25): bake the ground with no route
    /// surface painted, because the canvas draws the network itself in the lens's
    /// colours (RENDERING.md § Roads and sea lanes).
    bool  hide_routes = false;
};

/// Shared selection and view state for the three primary canvases.
///
/// Held by app and passed by reference to the canvas drawing functions. The
/// canvases read it to know what is selected and which rung is foregrounded,
/// and write to it in response to body clicks (descend), tile clicks, and
/// minimap clicks (ascend).
struct ui_state
{
    // --- baked ground (BL-732) ---
    ground_view    ground;          ///< Filled by app each frame from core/ground_layer.
    ground_request ground_req;      ///< Written by the Planetary canvas each frame.
    ground_neigh_request ground_neigh_req; ///< BL-1241: written by the Selection band's neighbourhood view.
    /// Verify-only: suppress the national border band so a ground-judgement
    /// capture shows the bake bare (BL-732 R1; the band's weight over painterly
    /// ground is BL-734's open call). No player control sets this.
    bool dbg_hide_border_band = false;
    /// Verify-only: a multiplier on the Planetary canvas's road and sea-lane
    /// stroke widths (the named per-tier constants, RENDERING.md § Roads and
    /// sea lanes), so one capture run can frame a width pick side by side
    /// (2026-10-03, roads as thinner curves). 1 is the shipped width. No
    /// player control sets this.
    float dbg_route_width_scale = 1.0f;
    /// Wheel accumulator for the stepped Planetary zoom: precision wheels and
    /// trackpads deliver fractional deltas per event, and each must NOT fire a
    /// full x2 rung — deltas accumulate here and a step fires per whole notch.
    float planetary_wheel_accum = 0.0f;

    entity_id    active_body   = null_entity;         ///< Navigation anchor: drives the lower rungs (circumplanetary anchor and surface). Changed by *navigation* (double-click / focus), not by selection. null_entity = no anchor.
    entity_id    selected_entity = null_entity;       ///< The entity the player single-clicked to inspect — drives the Selection info element. Distinct from the active_* anchors: selecting never moves the canvas. null_entity = nothing selected. See SELECTION.md, ui/selection.hpp.
    canvas_level primary_level = canvas_level::solar; ///< Which canvas rung fills the window.
    overlay_mode overlay       = overlay_mode::none; ///< Active canvas overlay lens; toggled by the bottom overlay control strip. Defaults to none — the plain canvas — at campaign start (reverses BL-013's Corporation-default): a click never re-skins the canvas, so it opens unskinned. Single-select with a null state (re-clicking clears to none).

    // --- lens-local selector state (Resource / Market / Scarcity lenses) ---
    // One shared "which resource" selection drives the Resource lens's contiguous
    // deposit fill, the Market lens's price surface, and the Scarcity lens's
    // shortfall surface (LENSES.md says the selectors share a form). Only the
    // active lens reads it. The Resource lens is always single-resource (BL-019),
    // so it carries no highest-value toggle.
    resource_type lens_resource = resource_type::iron_ore; ///< Selected good for the Market price surface and the Scarcity shortfall surface.
    /// The Resource lens's OWN selection (Ben, 2026-10-04): a toggled SET of
    /// extractable resources, in toggle order, at most k_lens_resource_cap — one per
    /// hex edge, since a tile carrying several splits into that many pie wedges. It
    /// is separate from `lens_resource` because Market and Scarcity stay single-good.
    static constexpr int k_lens_resource_cap = 6;
    std::vector<resource_type> lens_resources{ resource_type::iron_ore };
    /// The Resource lens legend's search box text (filters its goods checklist).
    char lens_resource_filter[32] = {};
    // BL-1406: the owner multi-select (BL-1240) retired with the Corporation and
    // Company lenses it served (LENSES.md § Corporation and Company lenses — retired).
    /// Is the lens legend expanded? BL-533 (Ben, 2026-08-22) re-homed every
    /// legend into the right chrome column above the minimap and made it a
    /// dropdown that is COLLAPSED BY DEFAULT — so a lens switch no longer
    /// throws a forty-row list over the map. Only the header shows until the
    /// player opens it. One flag serves every legend: they are mutually
    /// exclusive by construction (a lens draws at most one).
    bool lens_key_open = false;
    /// BL-1222: what the Trade-flow lens drew last frame — its arrows, its refusal
    /// markers, and how many dispatcher passes of the HELD corporation it read.
    /// Written by the Planetary canvas every frame (zero under any other lens);
    /// read only by the verify API, so a script can assert a seat change shows
    /// nothing of the corporation left behind.
    int trade_flow_arrows  = 0;
    int trade_flow_markers = 0;
    int trade_flow_passes  = 0;
    /// What the Trade-flow lens's ROUTED paths drew last frame (Ben, 2026-10-26): the
    /// flows drawn along a network route, and those with no route known (drawn as
    /// the straight arrow, the fallback).
    int trade_flow_routed   = 0;
    /// THE ROUTE EACH FLOW RIDES (Ben, 2026-10-26; LENSES.md § Trade-flow lens): per
    /// (source market, destination market, good) of the window, the tiles of the newest
    /// shipment's priced route (`convoy_route_tiles` over its recorded waypoints), source
    /// to destination. Rebuilt by `update_trade_flow_routes` (it needs a mutable world for
    /// the path cache); empty off the lens. Sorted by (src, dest, good). View state only.
    struct trade_flow_route
    {
        entity_id              src  = null_entity;
        entity_id              dest = null_entity;
        std::uint16_t          good = 0;
        std::vector<entity_id> tiles;
    };
    std::vector<trade_flow_route> tf_routes;
    int trade_flow_unrouted = 0;
    /// WHERE EACH ROUTED HEAD LANDED last frame (Ben, 2026-10-26: "arrowheads end on
    /// ordinary tiles"), as counts for the verify hook: routed heads drawn; of them, the
    /// ones whose tip AND base lie inside the destination market centre's hex; the routes
    /// whose last tile IS that centre; and the worst tip's distance from the centre in
    /// hex radii.
    int   tf_heads_drawn       = 0;
    int   tf_heads_in_dest_hex = 0;
    int   tf_routes_end_at_centre = 0;
    float tf_head_worst_r      = 0.0f;
    /// Lane-to-port joins on the active body the Planetary canvas draws (a lane tile's
    /// curve running on into a standing port's tile centre; Ben, 2026-10-26).
    int lane_port_joins = 0;
    /// The time panel's measured height, published each frame by time_panel so
    /// the rest of the right chrome column knows where its own ceiling is.
    /// It is computed from font metrics inside time_panel and was previously
    /// visible nowhere else — which is why shell_metrics::pinned_panel_rect,
    /// the one function that needs it, has never had a caller. BL-533 needs the
    /// same number: an expanded legend grows upward out of the minimap and must
    /// stop below the time panel rather than drawing over it.
    float time_panel_h = 0.0f;

    // --- navigation pane state ---
    // Policy: all ledgers start closed. The player opens them deliberately from
    // the navigation pane; none are shown on a fresh session.
    bool show_tile_ledger = false; ///< Whether the Tile Ledger window is open. Toggled by the nav pane tab and the window's close button.
    /// Whether the nav-rail slot-3 **Construction ledger** is open. Toggled by the
    /// rail slot, and opened by the tile Selection element's Construct button —
    /// which aims it at `construction.panel_view == 0`, the build bar's own view.
    /// There is no separate `show_build_ledger`: the tile-contextual build bar is
    /// a SECTION of this ledger's Construction view, not a second column tenant,
    /// so the two doors cannot show two different build bars.
    bool show_construction_panel = false;
    bool show_market_ledger = false; ///< Whether the Market Ledger is open.
    /// Last frame's open state of each lens-paired ledger (Market, Convoys, History),
    /// so the nav pane sees the CLOSE edge whatever closed it (rail re-click, another
    /// slot, Esc, the ledger's own close) and disarms the lens the ledger armed.
    bool market_ledger_was_open  = false;
    bool convoys_ledger_was_open = false;
    bool tile_ledger_was_open    = false;

    /// Whether the Convoys ledger is open (BL-689). Nav-rail slot 6, directly
    /// after Market. It was the Market ledger's third tab until the Goods
    /// flattening deleted that strip, and it left rather than being re-homed
    /// because it was never a market question: a convoy is cargo in transit and
    /// belongs to `SUPPLY.md`, not to the doc that owns clearing and the order
    /// book. Its own slot also lets it arm `supply_routes` on open — the lane
    /// overlay is the literal map twin of the list, and a tab strip could never
    /// give it that, since opening the Market ledger armed the price wash.
    bool show_convoys_ledger = false;

    // BL-1406: the Company ledger, its firm payload and the corporations table's
    // aimed row retired with the company and the corporation (Ben, 2026-10-23).
    bool show_balance_ledger = false; ///< Whether the Balance Ledger is open.

    // BL-1406: the Acquisitions ledger (rail slot 5, itself retired 2026-10-24) and every flag that drove it
    // retired with the buyout (docs/ui/ledgers/acquisitions.md). None of it was
    // ever serialised, so removing it does not move the save envelope.

    /// Where the Construction ledger's tabs, type-group headers, building rows and
    /// the building card's open-in-ledger button landed this frame. Same reasoning
    /// as `planetary_center_screen`: the surface that drew it is the honest source.
    construction_controls construction_ui;

    /// The site finder (Ben, 2026-10-25): the building chosen first, the tiles that
    /// take it, and the ledger's sort. See `site_finder_state`.
    site_finder_state site_finder;


    // THE CONTRACTS LEDGER'S VIEW STATE IS GONE (BL-693). The mercenary
    // contract — the SELL side of CONTRACTS.md — is retired, so the ledger, its
    // rail slot and every flag that drove them are deleted rather than left
    // dormant: view state has no reason to outlive the surface it described, and
    // none of it was ever serialised (save_game.hpp's ui_state slice is the
    // canvas rung, the selection and the pan/zoom, nothing else), so removing it
    // does not move the save envelope.
    //
    // The WORLD-side record is a different matter and is still here — see
    // `selected_contract_id` below, and `mercenary_offer` in components.hpp.
    // Procurement, the BUY side, never had view state on `ui_state` at all.

    /// Whether the Generation Ledger is open (BL-303). A DEVELOPER TUNING surface,
    /// not shipped chrome — it explains why a tile generated as it did — so like
    /// every other ledger it starts closed and nothing opens it but a deliberate
    /// press. See ui/generation_ledger.hpp, docs/generation/GENERATION_LEDGER.md.
    bool show_generation_ledger = false;

    /// Generation Ledger section disclosure. The ledger is ONE flat panel of
    /// stacked sections (the Balance ledger's shape) rather than a tab strip: the
    /// Tile view was retired 2026-08-30 and there is no second view left to name.
    ///
    /// Held here rather than in ImGui's own storage for the construction_panel
    /// reason - the state is then stable across a rebuild and drivable by a verify
    /// script, which ImGui's internal id storage is not. A `CollapsingHeader` is a
    /// toggle by construction, so the standing Toggle rule needs no second control.
    ///
    /// Profile and Thresholds open by default: they are short, and they are what
    /// the other four sections are read AGAINST - a histogram that surprises is
    /// traced back to the profile that asked for it.
    bool gen_profile_open   = true;
    bool gen_thresholds_open = true;
    bool gen_bands_open     = false;
    bool gen_substrate_open = false;
    bool gen_cover_open     = false;
    bool gen_landform_open  = false;

    // --- AI decision feed (BL-407) ---
    // A reader over stores that have always been populated and never surfaced:
    // world::ai_decisions (the 256-entry strategic ring, with scores) and
    // world::history_log's `decision` topic (permanent, prose, no scores).
    // The `agency` topic is deliberately NOT read — it re-narrates the same
    // decisions one-for-one and would double every row (NR-227).
    // Like every ledger it starts closed. See ui/decision_feed.hpp.

    bool show_decision_feed = false; ///< Whether the AI decision feed is open.

    /// Corp filter: null_entity = every corporation. Set by the feed's own
    /// selector, not by canvas selection — a spectator reading one corp's run
    /// should not lose their filter by clicking a tile.
    entity_id decision_feed_corp = null_entity;

    /// Reason filter: -1 = every reason, otherwise a `corp_decision_reason`
    /// value. Held as int so "all" has a home the enum does not have to invent.
    int decision_feed_reason = -1;

    // --- Strategy readout (BL-411) ---
    // The aggregate companion to the feed above: verb mix, spend split across
    // the priority buckets, and reason tally per corp over a rolling window.
    // Score/margin fields are deliberately absent from it (NR-226 fence).
    // Like every ledger it starts closed. See ui/strategy_readout.hpp.

    bool show_strategy_readout = false; ///< Whether the Strategy readout is open.

    /// Corp selector: null_entity = the all-corporations comparison view,
    /// otherwise one corp's full profile. Same persistence rationale as
    /// decision_feed_corp — canvas selection never clears it.
    entity_id strategy_readout_corp = null_entity;


    /// Unit Selection card accordion page index — Strength / Roster, whichever apply.
    int selection_unit_page = 0;

    /// The province the CURRENT SELECTION stands in, or 0 when the selection is
    /// not a tile. A DERIVED MIRROR, not a selection channel (BL-598).
    ///
    /// It was a selection channel: a click on bare ground selected the province
    /// and left `selected_entity` null, and the two fields were mutually
    /// exclusive. The province is now a SECTION of the tile element rather than
    /// a kind of its own, so there is no longer any gesture that selects a
    /// province without also selecting a tile, and the exclusivity is gone —
    /// both are set together, or neither is.
    ///
    /// What it is still FOR is the canvas: the province outline traces the cell
    /// the selected tile belongs to, which is the affordance that says "the
    /// Deposits / Buildings / Population sections you are reading are about
    /// THIS ground". Province ids are derived, never allocated
    /// (world/province.hpp), so this is a plain id, not an entity.
    ///
    /// It has exactly two writers, both in body_surface_canvas.cpp: the click
    /// handler, and the reconciliation arm that catches a selection some OTHER
    /// surface made (a ledger row, a just-built building). No surface reads it
    /// to decide WHAT to draw — the Selection element derives the province from
    /// `selected_entity` itself, so a stale mirror can never mis-route a card.
    /// See SELECTION.md § The tile element's accordion.
    uint32_t selected_province = 0;

    /// The province under the cursor this frame, or 0. Written by the Planetary
    /// canvas draw pass from the hovered tile; drives the province hover outline.
    uint32_t hovered_province = 0;


    // --- The selected battle (BL-469) ------------------------------------
    // Keyed by (province, attacker, defender), exactly as `active_battle` is,
    // because a battle HAS NO ENTITY ID to select. `selected_province` is the
    // precedent: BL-511 established that a selection whose subject is not an
    // entity lives in its own fields rather than being forced through
    // `selected_entity`.
    //
    // All three are needed, not just the province: a corp can be in more than one
    // battle in one province, since a third corp arriving opens its OWN battles
    // against each participant rather than joining theirs. Keying on the province
    // alone would silently select whichever sorted lowest.
    //
    // MUTUALLY EXCLUSIVE with `selected_entity` — and the Selection element
    // dispatches on the battle FIRST, because a fight on ground you are
    // looking at is the most urgent thing that ground has to say. (It is NOT
    // exclusive with `selected_province`, which since BL-598 is a derived
    // mirror of the selection's ground rather than a selection of its own.)
    uint32_t  selected_battle_province = 0;
    entity_id selected_battle_attacker = null_entity;
    entity_id selected_battle_defender = null_entity;

    /// True iff a battle is currently selected — all three key fields set.
    bool has_battle_selection() const
    {
        return selected_battle_province != 0
            && selected_battle_attacker != null_entity
            && selected_battle_defender != null_entity;
    }

    /// Clear the battle selection. Called by every other selection path, so the
    /// Selection element never has two things to draw.
    void clear_battle_selection()
    {
        selected_battle_province = 0;
        selected_battle_attacker = null_entity;
        selected_battle_defender = null_entity;
    }

    // --- The selected deposit and plate (BL-659 / BL-660) -----------------
    // Ben, 2026-08-28, choosing option A: follow the battle precedent rather
    // than give deposits and plates entity ids in world/. Both are regions the
    // lens DRAWS and the player could not previously select, and
    // `lens_structure_of_tile` recorded why — "a structure must be an ENTITY
    // here, because that is what a selection is ... highlighting a region the
    // player then cannot select would promise a pivot that does not arrive."
    // These two fields are what make the pivot arrive.
    //
    // A deposit is keyed by its RESOURCE, not by a region id, because that is
    // what the lens draws: the Resource lens fills every tile whose deposit of
    // the selected resource is non-zero, contiguous or not. Keying it any finer
    // than the drawing would break the rule the routing table is built on — the
    // selection grain follows the drawing.
    int selected_deposit_resource = -1; ///< resource_type index, -1 = none.
    int selected_plate            = -1; ///< continent_state::plate_id index, -1 = none.

    /// What the Selection element says about the selected PLATE, cached by the
    /// canvas at press time (BL-671).
    ///
    /// CACHED RATHER THAN LOOKED UP, which is the interesting half. A plate lives
    /// in the GENERATION REPORT, not in `world` — the band's draw signature takes
    /// a world and does not have the report, and threading one through four call
    /// sites to read five numbers would put a presentation-only dependency into
    /// every selection card. The canvas already holds the report's
    /// `continent_state` for the lens it is drawing, so it fills this once, on the
    /// press that made the selection.
    ///
    /// Safe to cache because plates DO NOT CHANGE after generation: the drift pass
    /// runs once and its output is retained, so there is no later state for this
    /// to go stale against. That is the property that makes the shortcut honest,
    /// and it is the thing to re-check if plates ever become dynamic.
    struct plate_summary
    {
        int   tiles            = 0;     ///< Tiles this plate owns on the body.
        int   convergent_tiles = 0;     ///< Of those, tiles on a collision boundary.
        int   divergent_tiles  = 0;     ///< Of those, tiles on a rift boundary. NOT
                                        ///< exclusive with the above: a tile at a
                                        ///< junction sits on both, and the height
                                        ///< bias has always taken both terms there.
        bool  oceanic          = false; ///< Oceanic plates bias the height field down.
        float drift_col        = 0.0f;  ///< Per-epoch drift, grid columns.
        float drift_row        = 0.0f;  ///< Per-epoch drift, grid rows.
    };
    plate_summary selected_plate_facts;

    /// Clear both non-entity lens selections. Called by every other selection
    /// path for the same reason `clear_battle_selection` is: the Selection
    /// element must never have two things to draw.
    void clear_lens_region_selection()
    {
        selected_deposit_resource = -1;
        selected_plate            = -1;
    }

    /// Is a deposit / a plate the thing the Selection element should draw?
    /// The battle and contract predicates' shape, and for the same reason: these
    /// are not entities, so `selection_kind_of` cannot see them and the band's
    /// dispatcher has to be told before it asks.
    bool has_deposit_selection() const { return selected_deposit_resource >= 0; }
    bool has_plate_selection()   const { return selected_plate >= 0; }

    // --- The selected mercenary contract (BL-577) ------------------------
    // A `mercenary_contract` has no entity id — it lives in
    // `world::mercenary_contracts`, keyed by its own stable `id` — so, exactly
    // like the battle triple above, it cannot travel in `selected_entity` and
    // gets its own field. A single scalar suffices here (unlike the battle's
    // three-part key): a contract id is already globally unique, with no
    // second axis like "which of a province's several fights" to disambiguate.
    //
    // MUTUALLY EXCLUSIVE with `selected_entity` and the battle triple, on the
    // same "whichever is set last clears the others" rule.
    //
    /// The value of `selected_entity` the Planetary canvas last wrote, so the
    /// canvas can tell its OWN selection from one some other surface made — a
    /// ledger row, a corp list, a just-built building. On a mismatch the canvas
    /// re-derives `selected_province` from the new selection and drops any stale
    /// battle selection; see body_surface_canvas.cpp.
    entity_id province_sync_entity = null_entity;

    // --- Tile repeat-click selection cycle ------------------------------
    // Which tile the loop is currently anchored to, and which of the four fixed
    // stages the selection currently sits on. A click on hovered_tile ==
    // selection_cycle_tile advances the stage (skipping stages with nothing
    // there); a click elsewhere reseeds both. Uses a per-selection reset
    // idiom rather than adding a separate "is this a repeat
    // click" flag.
    ///
    /// Stage numbering: **0 = battle, 1 = unit, 2 = building, 3 = tile.**
    ///
    /// BL-598 (Ben, 2026-08-24) DISSOLVED the province rung. It sat between unit
    /// and building and expressed itself as `null_entity` plus a province id;
    /// the province is now a SECTION of the tile element, so the tile rung
    /// reaches it and a rung of its own would select the same ground twice.
    /// Rungs 1..3 are entities again, so a null check IS their liveness test;
    /// the `stage_live[]` table stays only because rung 0 (the battle) is still
    /// not an entity and cannot be tested that way.
    ///
    /// CONSEQUENCE, stated rather than discovered: on bare ground with no unit,
    /// no building and no battle, exactly ONE rung is live, so a repeat click
    /// re-selects the same tile. That is the honest reading — there is nothing
    /// else on that ground to cycle to.
    ///
    /// This is the CYCLE order only; the canvas hit-test still resolves
    /// most-specific-first, so a first click on a building selects the building
    /// rather than the tile under it.
    entity_id selection_cycle_tile  = null_entity;
    int       selection_cycle_stage = 0;

    // --- Unit march / halt / disband (BL-575) ---------------------------
    // The unit card's three presses. Halt and Disband act on the unit ALREADY
    // selected and take effect next frame (deferred like every other world
    // mutation — UI surfaces hold only `const world&`; app::render applies
    // them and clears the field). March is two-step: the press below only
    // ARMS the destination pick, it does not move anything by itself.

    /// Set by the unit card's March press. Non-null means the Planetary canvas
    /// is in province-picking mode for THIS unit: the next left-click on a
    /// province consumes the click (suppressing ordinary selection, exactly as
    /// `construction.active` suppresses it for build placement) and fills
    /// `pending_march_dest_province` instead. app::render dispatches
    /// `corp_verb::march_unit {subject: pending_march_unit, province:
    /// pending_march_dest_province}` once both are set, then clears both.
    entity_id pending_march_unit         = null_entity;
    /// The province id the canvas wrote on the qualifying click, or `0` while
    /// nothing has been picked yet — the same "no province" sentinel
    /// `selected_province` uses (a real province id is never 0, province.hpp).
    /// Kept distinct from `selected_province`: picking a march destination
    /// must NOT also change what the Selection element is showing.
    uint32_t  pending_march_dest_province = 0;

    /// Set by the unit card's Halt press — the unit to clear the movement
    /// order of. `null_entity` = nothing pending.
    entity_id pending_halt_unit    = null_entity;
    /// Set by the unit card's Disband press — the unit to erase outright.
    /// `null_entity` = nothing pending. No confirm step here; the seam gives
    /// no refund either way (MILITARY.md § Marching), so a confirm popup, if
    /// ever wanted, is the unit card's call, not this field's.
    entity_id pending_disband_unit = null_entity;

    /// Spectator mode (BL-409): no human seat. Presentation-side flag only —
    /// `world/*` never reads it; the sim's copy travels as corp_ai_params
    /// .spectating through run_economy_step's defaulted argument.
    bool spectating = false;

    /// Spectator god view (BL-408): lift the survey mask and the BL-068
    /// rival-internals redaction — in the UI layer ONLY. The flag is read at
    /// the draw call, never at the source: `world/*`, `survey_tile_visible`,
    /// the activity-fog store and `export_corp_blackboard` never see it, so
    /// the AI stays exactly as visibility-honest while the watcher does not.
    /// Meaningful only under `spectating` — every read-site tests the PAIR
    /// (`spectating && god_view`), and the toggle (system menu, time_panel.cpp)
    /// only renders while spectating, so a played session cannot reach it.
    /// Defaults false: with it off, every gated surface renders byte-identical
    /// to the pre-BL-408 build.
    bool god_view = false;

    // --- Budget ledger policy levers (BL-171 UI) ---
    // Taxes is the player's NATION's tax tier (BL-1335): the control reads
    // `nation_component::tax_tier` and a press is a set_tax_tier command, so it
    // holds no state here. The Wages selector is still a stub with NO economic
    // effect (a cost<->workforce trade-off designed under BL-155). Tiers run 1–5
    // (I–V); the Wages default matches the mockup (II).
    int  budget_wage_tier = 2;
    bool show_corporation_panel = false; ///< Whether the Corporation Overview Dashboard is open.

    // BL-1406: the corporation stance table (rail slot 8 now, Diplomacy) retired with
    // the corporation (docs/ui/ledgers/corporation.md § Slot 8, retired).

    /// F9 mock tech-tree viewer (BL-087), also reachable from nav rail slot 4
    /// (BL-310, 2026-08-06 — the slot was a hard-disabled placeholder before).
    /// Moved here from app.hpp's m_show_tech_tree so nav_pane.cpp can toggle it
    /// the same way as every other panel flag.
    bool show_tech_tree = false;

    // --- one-question-per-view nav selectors (BL-117 sweep) ---
    // Each fold-out ledger with more than one question splits its content into
    // button-strip views (ui::nav_button_strip); this is the selected view per panel,
    // persisted so a panel reopens where the player left it. See the Construction
    // panel's construction.panel_view for the template.
    /// Market Ledger: 0=Goods (BL-686 — one row per traded good, the 8-quarter
    /// graph flattened into the row), 1=Sell Orders (BL-159 — sell-order
    /// management relocated here from the Construction/Building panel).
    ///
    /// There is no view 2. Convoys held it (BL-453) until BL-689 gave it rail
    /// slot 6 (7 until 2026-10-24) and its own ledger; the draw clamps a stale 2 back to 0 so a save
    /// or a verify hook carrying the old index lands on Goods rather than on
    /// nothing. View 1 becomes TRADES in a later slice (BL-687), which waits on
    /// an exchange record the world does not keep yet.
    int  market_ledger_view = 0;

    /// Whether the Goods table draws the `body average price` column.
    ///
    /// DEFAULTS OFF ON A MEASUREMENT, not a preference. The design nominated this
    /// as the FIRST COLUMN TO DROP if the row will not fit, and required the fit
    /// be measured rather than assumed — expecting the pressure at 384 px to be
    /// on the graph's width rather than the column count. Measured at 1920x1080
    /// over the real 45-good roster (`scripts/verify/goods_table.lua`, which
    /// prints both):
    ///
    ///     with the column:     name column 42 px, 13 of 45 names fit
    ///     without the column:  name column 98 px, 36 of 45 names fit
    ///
    /// So the expectation was wrong and the design's own rule applies: dropping
    /// one text column more than doubles the name width, because the graph is
    /// only ~3.4em to begin with. At 42 px "Iron Ore" and "Iron-something" BOTH
    /// elide to "Iron ...", which defeats the one thing a price board is for.
    /// Kept as a dial so the call is one line to revert, and the script reports
    /// both numbers on every run.
    bool market_goods_show_body = false;

    // --- BL-1239 (tile production section): the Market ledger's AIM ---------
    // A drill-through door (DRILL_THROUGH.md) from the tile element's
    // Production section: a good row opens the Market ledger aimed at the
    // market whose catchment holds the tile, and at that good. The ledger
    // already follows a selected MARKET entity (BL-159), but the door must not
    // replace the tile selection it was pressed from, so it aims through these
    // instead. `market_ledger_aim_market` is ONE-SHOT: the ledger consumes it
    // (jumps its body/market combos) and clears it, so the player can browse
    // away afterwards. `market_ledger_aim_resource` is the good the Goods view
    // highlights; it persists while the ledger shows that market, and
    // `market_ledger_aim_scroll` is the one-shot request to scroll it into view.
    entity_id market_ledger_aim_market   = null_entity;
    int       market_ledger_aim_resource = -1;   ///< resource_type index, -1 = none.
    bool      market_ledger_aim_scroll   = false;
    /// The market the ledger last consumed an aim for — PERSISTENT, unlike
    /// `market_ledger_aim_market`. The aimed good highlights only on this
    /// market, and the Production row reads "aimed" (and so toggles the ledger
    /// shut on a second press) only when its own market is this one.
    entity_id market_ledger_aimed_market = null_entity;

    /// Verify-only readout of the tile element AS DRAWN last frame (BL-1312,
    /// tile card split). Reset by the Selection dispatcher every frame, so
    /// `drawn` is false whenever the tile element did not draw. Exists so a script
    /// can press the tile card's building buttons and back control at their real
    /// positions, and assert what a RIVAL button and card withhold (DISCOVERY.md,
    /// the operational fog) — an absence a capture cannot prove. No player
    /// surface reads it.
    struct tile_card_button
    {
        std::string label;                 ///< Name as drawn, with " ×N" for a stack above one.
        std::string detail;                ///< The third line: profit / "private" / ETA.
        std::string good;                  ///< The good the stack makes, "" when not shown.
        bool        open  = false;         ///< Internals shown (own, or god view).
        int         count = 1;
        entity_id   first = null_entity;   ///< The member a press selects.
        float       x = -1.0f, y = -1.0f;  ///< Button centre, screen px.
        /// BL-1371: how many lines the NAME column drew, and whether it elided
        /// anyway — the two-line row's own assertion (see `grid_row`).
        int         name_lines  = 0;
        bool        name_elided = false;
        /// BL-1368: the stack row's per-COLUMN explainers, in drawn order.
        std::vector<ui_grid_col> cols;
    };
    struct tile_card_readout
    {
        bool        drawn    = false;
        entity_id   tile     = null_entity;
        entity_id   expanded = null_entity; ///< The expanded building, null when the grid shows.
        bool        expanded_player = false;
        bool        charts   = false;       ///< The profit row drew.
        std::string workforce;              ///< The read-only workforce row as drawn ("75%"), "" when not drawn (BL-1322).
        int         count    = 1;           ///< The expanded stack's member count.
        std::string title;                  ///< The expanded header's name + " ×N" as drawn.
        std::string method;                 ///< The production method line, "" when none.
        /// BL-1365: the stack LIST's row height and one text line, as drawn —
        /// so a check can assert the construction grid's "one line a row" rule
        /// on this surface too rather than inferring it from a capture.
        float       row_h  = 0.0f;
        float       line_h = 0.0f;
        std::string net;                    ///< Net per quarter as drawn, "" when not drawn.
        std::string expenses;               ///< Expenses per quarter as drawn.
        int         population = 0;         ///< The province card's header figure, thousands.
        float       province_scroll_max = 0.0f; ///< The province card's OWN window scroll range (must be 0).
        std::string status;                 ///< The expanded card's status line, "" when none.
        /// BL-1368: the EXPANDED site row's per-column explainers (the under-
        /// construction card is one grid row), in drawn order. Empty on a card
        /// whose building is finished, which has no grid row at all.
        std::vector<ui_grid_col> site_cols;
        float       back_x = -1.0f, back_y = -1.0f;         ///< The back control's centre.
        int         province_standing = 0;
        int         province_ceiling  = -1;
        float       capacity_x = -1.0f, capacity_y = -1.0f; ///< The capacity bar's centre.
        float       population_x = -1.0f, population_y = -1.0f; ///< The population figure's centre.
        std::vector<std::string> province_rows;             ///< Resource names, table order.
        std::vector<tile_card_button> buttons;
        float       preview_w = 0.0f, province_w = 0.0f, tile_w = 0.0f, actions_w = 0.0f, height = 0.0f;
        /// BL-1354: the Logistics section's figures as drawn - what a source here makes,
        /// what this tile's (counted) buildings are granted and need, and how many rival
        /// buildings were left out of the count for visibility. `lp_drawn` is false when
        /// the section did not draw (a building expanded, or no resolve yet).
        bool        lp_drawn         = false;
        float       lp_makes         = 0.0f;
        float       lp_uses          = 0.0f;
        float       lp_need          = 0.0f;
        int         lp_rivals_hidden = 0;
    };
    tile_card_readout tile_card;

    /// History ledger: 0=Story (the body's biography), 1=Chain (the generation
    /// charts), 2=Ages (the Era -1 political time-lapse, BL-277), 3=Tectonics
    /// (the plate view, BL-660). BL-211. The names are `history_view_id` in
    /// ui/tile_inspector.hpp — this comment is the mirror, not the source, and
    /// it read "2=Tiles, 3=Ages" against a Tiles view that no longer exists
    /// until 2026-08-30 (NR-713).
    int  history_view = 0;

    /// Ages view: the year currently scrubbed to, and whether playback is
    /// running. VIEW state — not serialised; where the scrubber was parked is a
    /// display detail, not part of the campaign.
    int  ages_year    = 0;
    bool ages_playing = false;
    /// BL-1295: the Ages view FILTERED TO ONE NATION -- set by the nation
    /// selection card's history link (STARTUP.md § The seat). Regions held by
    /// the nation's lineage (ui::nation_lineage) stay lit, every other realm's
    /// dims. null_entity = no filter. VIEW state, not serialised.
    entity_id history_nation_filter = null_entity;

    /// UNREAD. The research panel has no views: hover reads a node and SHIFT +
    /// left click queues it (Ben, 2026-10-10, retiring BL-1276's Inspect/Plan
    /// strip). Kept so `verify.panel_view("tech_tree", n)` in older scripts
    /// stays a harmless write.
    int  tech_tree_view = 0;

    /// The ring's camera: pan offset of the ring's centre from the canvas
    /// centre (screen px) and scroll-wheel zoom, the planetary_pan_x /
    /// planetary_zoom idiom (body_surface_canvas.cpp). View state, not saved.
    float tech_tree_pan_x = 0.0f;
    float tech_tree_pan_y = 0.0f;
    float tech_tree_zoom  = 1.0f;

    /// The node under the cursor and the shared hover card's glance-then-stick
    /// state for it (TOOLTIP.md § The model). -1 = none. View state only.
    int   tech_tree_hover_node  = -1;
    float tech_tree_hover_secs  = 0.0f;
    bool  tech_tree_hover_stuck = false;
    float tech_tree_card_anchor_x = 0.0f, tech_tree_card_anchor_y = 0.0f;
    float tech_tree_card_min_x = 0.0f, tech_tree_card_min_y = 0.0f;
    float tech_tree_card_max_x = 0.0f, tech_tree_card_max_y = 0.0f;

    /// Each node's screen centre as last drawn, (x, y) per `io::dg` index;
    /// empty when the ring is not on screen. Published for the verify API
    /// (`verify.tech_tree_node`), which aims its hover and press at a node by
    /// id rather than re-deriving the ring's layout in Lua.
    std::vector<float> tech_tree_node_screen;

    /// Research-queue presses from the panel (queue a node, remove the last,
    /// clear), applied by `app::render` through `apply_corp_command` — the
    /// panel holds a const world. Drained in order each frame.
    std::vector<corp_command> pending_research_commands;

    /// The last research press's outcome, shown under the queue: the refusal
    /// reason when a press was refused ("Mass Surveillance is closed: its pair,
    /// Mass Television, is queued"), or what a press queued.
    std::string research_message;

    // --- drill-through disclosure (BL-214, revised BL-265) ---
    // The one idiom every dense surface obeys. BL-214's model was BINARY — folded, or
    // a full-WINDOW overlay — and this comment used to reason from it: because expanded
    // IS an overlay, only one thing can be expanded, so a single target suffices.
    //
    // BL-265 split that into TWO controls and the reasoning no longer holds as one rule:
    //   * the TAKEOVER is still single-target, and for the original reason — it owns one
    //     rectangle (now the CANVAS, not the window), so opening a second replaces the
    //     first. That is what this member tracks.
    //   * IN-PLACE expansion is a SET, not a target (fold_state::in_place). A surface
    //     that grows where it sits does not displace its siblings, and a single-target
    //     in-place expander would make an accordion unbuildable.
    // A canvas-bounded takeover may therefore coexist with an open ledger column, which
    // is intended rather than a leak.
    //
    // VIEW state — not serialised, and deliberately not remembered: which card was last
    // open is a display preference, not something to restore. See ui/detail_level.hpp.
    fold_state expanded{};

    /// One-shot latch: has the building-management view seeded its default open
    /// section yet (BL-229)? Production rests OPEN, which is the answer to variant C's
    /// own objection — the control a player touches constantly should not start behind
    /// a click. It is a LATCH rather than a per-frame default so that folding Production
    /// away STAYS folded; re-defaulting every frame would make the section unclosable.
    /// VIEW state, not serialised, like everything else here.
    bool building_section_seeded = false;

    /// Which row of the expanded Corporation-dashboard roll-up is drilled into
    /// (BL-248); -1 = the roll-up itself. One index is enough because only one card
    /// can be expanded at a time, and a drill can only be reached from inside an
    /// expanded card. Cleared whenever a card folds.
    int        corp_rollup_drill = -1;


    /// Within the History ledger's Chain view, which chain round's stages are
    /// listed (0..chain_round_count-1). Held here rather than as a function-local
    /// static so a verify script can park the ledger on a given round.
    int  history_round = 0;

    // --- application / system menu (BL-070) ---
    // The corner gear popup for session control (Pause/Resume, Exit Game). Opened
    // by the gear button and toggled by Esc; confirm_exit_pending arms the inline
    // "Really quit?" confirm before the destructive quit (there is no save). Drawn
    // and acted on in app::render, which owns the quit flag and the sim pause. See
    // docs/ui/MENU.md § Application / system menu.
    bool show_system_menu     = false; ///< Whether the corner system-menu popup is open.
    bool confirm_exit_pending = false; ///< Within the popup, whether Exit is armed to the "Really quit?" confirm.

    // --- frame-budget HUD (BL-249) ---
    // The v0.1.0 quality audit's frame-time instrument, toggled by F11 (app.cpp
    // handle_key_down) and by its own close button. An AUDIT INSTRUMENT, not shipped
    // chrome: it starts closed, so every golden capture renders without it. Lives
    // here rather than as an app member so the toggle sits with the other show_*
    // flags — and so a verify script can park it open. See ui/frame_stats.hpp.
    bool show_frame_hud = false; ///< Whether the frame-budget HUD is drawn.

    /// Per-frame list of on-canvas markers (buildings, market centres). Cleared at
    /// the top of body_surface_canvas each frame and rebuilt during the draw pass
    /// so click/hover handling can hit-test in priority order. See marker_hit_zone.
    std::vector<marker_hit_zone> marker_hit_zones;

    /// Per-frame list of on-canvas STRUCTURE boundaries (national borders today).
    /// Cleared and rebuilt with `marker_hit_zones`, and hit-tested after it: a
    /// marker is a specific thing the player aimed at, a boundary is a region.
    /// See structure_hit_zone.
    std::vector<structure_hit_zone> structure_hit_zones;

    /// BL-603 — the STRUCTURE under the pointer, as the active lens defines it.
    ///
    /// Ben, 2026-08-24: "When a lens reveals any large structure, the selection
    /// should pivot to the entire structure, and no longer provinces. So for the
    /// market lens, the entire market gets highlighted on mouse over, and clicking
    /// opens up our market ledger for that market."
    ///
    /// This is the AREA counterpart to `structure_hit_zones`, which resolves a
    /// structure by its BOUNDARY (the national border band). A catchment has no
    /// boundary the player aims at — they aim at ground inside it — so the lens
    /// answers "which structure is this tile part of?" instead.
    ///
    /// Written by the canvas AFTER its tile loop, so the highlight it feeds is one
    /// frame behind, exactly as `hovered_province` already is and for the same
    /// reason: the loop must know the answer before it has drawn the tile that
    /// produces it. Invisible at any frame rate the canvas runs at.
    entity_id      hovered_structure      = null_entity;
    structure_kind hovered_structure_kind = structure_kind::none;

    /// Building-placement interaction state (Layer 4 UI groundwork scaffold). See construction_state.
    construction_state construction;

    /// Pending survey dispatch (BL-067). The Selection-panel Survey button sets this
    /// to the body to survey; app::render executes dispatch_survey() and clears it,
    /// mirroring the construction request seam. Keeps the survey mutation in app while
    /// the UI surfaces hold a const world. null_entity = nothing pending.
    entity_id pending_survey_dispatch = null_entity;

    /// Pending ledger commands (BL-293; the trade presses since BL-1269 —
    /// `set_trade`, `clear_trade`, `set_trade_reserve`). What lives here is the
    /// REQUEST, for the reason `pending_survey_dispatch` above lives here — the
    /// ledgers do not mutate the world, so they enqueue and `app::render` applies.
    ///
    /// Applied through `apply_corp_command`, the same seam the rival-corp AI
    /// drives, so the player's press and the AI's command are the same code path
    /// by construction rather than by two implementations agreeing. A queue rather
    /// than a single slot because a frame can carry more than one press (add an
    /// order, remove another); drained in order each frame.
    std::vector<corp_command> pending_order_commands;

    /// BL-1269: the seam's answer to the last TRADE press (`set_trade`,
    /// `clear_trade`, `set_trade_reserve`), written by `app::render` when it drains
    /// `pending_order_commands` and shown under the Trades tab's form. A refusal
    /// mutates nothing, so the player is told why. Empty = nothing to say.
    std::string trade_message;

    /// BL-1269: an explicit request to aim the Market ledger at this market (the
    /// market Selection card's Trades door). Consumed by the ledger's next draw;
    /// unlike following `selected_entity`, it fires even when the selection has
    /// not changed since the ledger last looked. null_entity = no request.
    entity_id market_ledger_focus = null_entity;

    /// BL-323 S2b: the logistics-reach budget the UI must filter on, mirrored here
    /// from `recipe_registry::construction().max_logistics_reach` at load time.
    ///
    /// Carried on ui_state rather than threaded through every draw signature for a
    /// specific reason: the placement surfaces disagree about what they hold — the
    /// canvas and the construction ledger take a `const world&` plus a registry,
    /// while `draw_tile_selection` takes a mutable world and no registry. One
    /// mirrored float lets all of them apply the SAME rule the authoritative gate
    /// applies, which is the point: a tile offered and then refused at commit reads
    /// as a broken build rather than a rule. Negative disables, as everywhere else.
    float max_logistics_reach = -1.0f;

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

    // --- intra-body vision model (BL-151/152/154) ---
    // The Planetary canvas reads three vision layers, all derived VIEW state — never
    // serialised, never fed back into the simulation, so world/* stays deterministic.
    // Refreshed each frame by ui::update_body_vision for the active body.

    /// Permanently-lit tiles: radius-2 pockets around the player's own building tiles
    /// (your installations are always visible) + 3-wide corridors from the corp centre
    /// of operation to each market centre the player operates in. Rendered at full
    /// vision (no fade).
    std::unordered_set<entity_id> permanent_vision;

    /// A live player intra-body convoy, for the render-time moving beam. The path is
    /// the convoy's tile route in travel order (src→dst); progress/speed drive a head
    /// that interpolates smoothly along it between econ steps, with a tail that lags
    /// and dims one econ tick's travel behind the head.
    struct convoy_beam { std::vector<entity_id> path; float progress = 0.0f; float speed = 0.0f;
                         std::vector<float> at; /* BL-1195: the lane's clock (convoy_route::at) */ };
    std::vector<convoy_beam> convoy_beams;

    // --- Throughput lens: this frame's active-LP anchor pools (BL-598) ---
    // LOGISTICS.md § Logistic Points: LP is a per-tick RATE, never a stock. These
    // three fields are DERIVED VIEW STATE with exactly the same standing as
    // permanent_vision above — recomputed from scratch every frame by
    // ui::update_body_throughput (which needs a non-const world, so the const draw
    // cannot do it), never serialised, never entering state_hash, never read back
    // by the simulation. Nothing here crosses a tick boundary as game state: it is
    // a photograph of this tick's rate, retaken each frame.
    //
    /// The active body's supply anchors and their active LP for this tick, SORTED
    /// ASCENDING BY TILE ID. Sorted rather than hashed for two reasons: the draw
    /// does a lower_bound per visible tile, and the reductions below are float
    /// sums, which are order-dependent — an unordered_map's iteration order would
    /// make the number in the legend depend on the hash layout.
    struct lp_anchor { entity_id tile = null_entity; float lp = 0.0f; };
    std::vector<lp_anchor> lp_anchors;
    float lp_anchor_total = 0.0f; ///< Σ lp over lp_anchors, summed in tile-id order.
    float lp_anchor_max   = 0.0f; ///< Largest single anchor's lp; the anchor-mark ramp's ceiling.

    /// Largest FINITE reach cost on the active body — the denominator the lens's
    /// field ramp normalises against, so the shading is body-relative rather than
    /// calibrated to an absolute cost nobody can read. 0 when no field exists.
    float lp_reach_max = 0.0f;

    /// The 90th percentile of the same field, and the ramp's actual denominator
    /// (`throughput_field_census`, 2026-08-28). `lp_reach_max` stays because the
    /// legend still reports the body's true worst case — but normalising the
    /// COLOUR against it spends half the ramp on the far decile, which holds a
    /// tenth of the tiles. Zero when the field is absent; the ramp falls back to
    /// the max, which is the pre-2026-08-28 behaviour rather than a blank map.
    float lp_reach_p90 = 0.0f;

    // --- BL-1353: the lens shows the POINTS, for the player's own nation ---------
    // Ben, 2026-10-16: "give a number to each tile, so we can see exactly how many
    // points we have. The lens should also be player facing, so we can see only the
    // player's home nation's road network and points."
    //
    // LENSES.md argued a per-tile points figure could not vary, because every anchor
    // made the same authored rate and every tile had a finite reach cost. BL-1309
    // overtook that: points are made by hubs, stations and ports, they fade with
    // distance along their own network. ABSOLUTE POINTS, NOT A SHARE (Ben,
    // 2026-10-24: "the logistics lens should show absolute numbers rather than a
    // percentage. It should also show both the amount generated, and the amount
    // consumed"). So the field below is the LOGISTIC POINTS REACHING each tile from
    // its nation's qualifying sources (`lp_points_reaching`), and the number drawn on
    // a tile is that figure in LP/tick.
    //
    /// LP/tick reaching each cell of the active body, row-major (y * gw + x), for the
    /// PLAYER'S OWN NATION's ground only; -1 means "not this nation's" and is drawn as
    /// bare ground. Empty when the lens is off. Derived view state on the same footing
    /// as the anchors above: rebuilt from the tick's own resolve, never serialised,
    /// never in state_hash, never read back by the simulation.
    std::vector<float> lp_reaching;
    int lp_reaching_gw = 0, lp_reaching_gh = 0;

    // --- The Logistics lens's TWO NUMBERS per tile (Ben, 2026-10-24) -------------
    // Same raster and footing as `lp_reaching` (-1 = not the player nation's ground).
    // SUPPLIED over REQUESTED, both LP/tick. On a ROAD tile they are the routed flow
    // through it (`lp_route_flow`: what the chain could deliver there - each source
    // whose walk crosses the road, once, its points x its reach - over every request
    // whose route to the capital crosses it) and the pair colours the road;
    // on any other tile they are the points reaching it and its own buildings' need.
    std::vector<float> lp_supplied;
    std::vector<float> lp_requested;
    /// What LINK of the player's chain the cell is (Ben, 2026-10-25: "a lens that shows
    /// each chain link"): 0 none, 1 the player's ROAD tile, 2 a RAIL leg's track, 3 a
    /// SEA leg's water (LENSES.md § Logistics lens, "Every link of the chain").
    std::vector<std::uint8_t> lp_on_road;
    std::vector<float> lp_granted; ///< on a link: what its routed requests were granted (-1 elsewhere)
    /// REACH, KEPT APART FROM TRAFFIC (Ben, 2026-10-25): per cell, 0 = off the chain (no
    /// connected source reaches it - `lp_route_to`'s `reached`), 1 = CONNECTED, IDLE (on
    /// the chain, no routed request crosses it), 2 = carrying a routed request. Read
    /// only where `lp_on_road` names a link.
    std::vector<std::uint8_t> lp_chain;
    /// Every rail-track cell of the active body (1), so the lens can draw the line the
    /// bake no longer paints under it (RENDERING.md § Roads and sea lanes).
    std::vector<std::uint8_t> lp_track;
    /// One rail or sea LEG of the player's chain, as raster cells, for the lens's stroke
    /// along it (the per-cell figures above carry its numbers). Plain fields so this
    /// header does not pull in the resolver.
    struct lp_leg_view
    {
        std::uint8_t     network = 1;  ///< 1 rail, 2 sea (lp_network as an int)
        std::uint8_t     chain   = 1;  ///< 1 connected idle, 2 carrying
        std::vector<int> cells;        ///< raster indices, end to end
        float            requested = 0.0f, granted = 0.0f, available = 0.0f;
    };
    std::vector<lp_leg_view> lp_legs;
    /// Which NETWORKS a link cell carries, as bits (1 road, 2 rail leg, 4 sea leg). A
    /// cell with more than one is ONE tile of the lens all the same (Ben, 2026-10-26:
    /// "one colour per tile, showing both networks added together"): its supplied and
    /// requested above are the networks' sum, and it is drawn in the one colour.
    std::vector<std::uint8_t> lp_links;
    /// The census of those combined cells for the verify hook (counts and totals only):
    /// how many, their figures, and the road's and the legs' parts of them.
    int    lp_both_cells = 0, lp_both_added = 0;
    double lp_both_supplied = 0.0, lp_both_requested = 0.0;
    double lp_both_road_supplied = 0.0, lp_both_road_requested = 0.0;
    double lp_both_leg_supplied = 0.0, lp_both_leg_requested = 0.0;
    /// What the leg pass drew last frame: the cells it stroked, each in its tile's colour.
    int    lp_leg_cells_drawn = 0;

    /// THE CAPITAL STAR (Ben, 2026-10-25): how many stars the Planetary canvas drew
    /// last frame (one per visible wrap copy), and whether the player's own was among
    /// them - "the mark was drawn", for the verify hook, not a second copy of the list.
    int  capital_stars_drawn  = 0;
    bool capital_star_player  = false;

    /// The player nation's own figures for the key, all in LP/tick: what its connected
    /// sources GENERATED (the pool), how many sources made it, and what its buildings
    /// CONSUMED - production's grants, the marches' grants and what shipments drew -
    /// beside what production asked for. The numbers a player can act on - everything
    /// else on the body is somebody else's business.
    float lp_player_pool     = 0.0f; ///< generated this tick
    float lp_player_claim    = 0.0f; ///< production's claim (need x cover)
    float lp_player_need     = 0.0f; ///< production's need at its run, cover aside
    float lp_player_prod     = 0.0f; ///< consumed by production (granted)
    float lp_player_march    = 0.0f; ///< consumed by marches (granted)
    float lp_player_shipping = 0.0f; ///< consumed by shipments (drawn)
    int   lp_player_sources  = 0;
    bool  lp_player_known    = false; ///< false when the player has no nation to show

    /// BL-1353: the WHY behind the hovered tile's figure, for the Throughput ground
    /// card (SELECTION.md: Throughput takes a hover read, and still no selection).
    /// Computed each frame by update_body_throughput off its own coverage memo - the
    /// card cannot, since it holds a const world - and cleared off the lens.
    /// `lp_hover_network` is an lp_network as an int (0 road, 1 rail, 2 sea), kept
    /// plain so this header does not pull in the resolver.
    entity_id lp_hover_tile     = null_entity;
    float     lp_hover_reaching = 0.0f; ///< LP/tick reaching the tile (lp_points_reaching)
    int       lp_hover_sources  = 0;    ///< how many sources that came from
    float     lp_hover_consumed = 0.0f; ///< LP/tick the tile's buildings were granted
    float     lp_hover_need     = 0.0f; ///< LP/tick those buildings need at their run
    int       lp_hover_consumers = 0;   ///< producing buildings on the tile
    bool      lp_hover_reached  = false;
    int       lp_hover_network  = 0;
    float     lp_hover_distance = 0.0f;
    int       lp_hover_used     = 0;     ///< BL-1356: sources the cover was topped up from
    bool      lp_hover_blocked  = false; ///< BL-1356: a source reaches here but has no chain to the capital
    float     lp_hover_supplied  = 0.0f;  ///< the tile's SUPPLIED figure (lp_supplied's cell)
    float     lp_hover_requested = 0.0f;  ///< the tile's REQUESTED figure (lp_requested's cell)
    bool      lp_hover_road      = false; ///< the pair is a link's routed flow, not the tile's own
    float     lp_hover_granted   = 0.0f;  ///< on a link: what the routed requests were granted
    int       lp_hover_link      = 0;     ///< lp_on_road's kind under the pointer (0 none, 1 road, 2 rail, 3 sea)
    int       lp_hover_links     = 0;     ///< lp_links's bits under the pointer (1 road, 2 rail, 4 sea)
    int       lp_hover_chain     = 0;     ///< lp_chain's state under the pointer (0 off, 1 idle, 2 carrying)

    double sim_now_days = 0.0; ///< Latest continuous sim time (elapsed days); the beam-motion clock.

    // --- hover-card state (BL-060) ---
    entity_id hovered_entity = null_entity; ///< Entity the cursor rested on last frame; used to detect stable hover.
    float     hover_seconds  = 0.0f;        ///< Seconds of stable hover over hovered_entity; resets on entity change. Governs the transient glance (draw_hover_card).

    /// Advance hover (and any future dwell clock) by a fixed 1/60 s per frame
    /// instead of the wall-clock delta. Set only by `--verify`, whose golden
    /// captures step whole presentation frames: real deltas there carry vsync
    /// jitter, which would make a seconds-based threshold land nondeterministically.
    bool fixed_frame_clock = false;

    // --- hover card (BL-228/230, retires BL-200 dwell-to-open) ---
    // Hovering no longer OPENS anything. The card has two phases: a GLANCE that
    // appears after kHoverAppearDelaySec and still tracks the live cursor, then a
    // STUCK freeze after kHoverStickDelaySec that stops following the pointer and
    // stays put until the cursor leaves its bounds — so the player can read a
    // long line to its end without the card sliding away. Opening the
    // Selection band is the click's job alone — one gesture, one meaning.
    entity_id hover_card_entity = null_entity;   ///< Subject of the card (glance or stuck); null_entity = no card up.
    bool      hover_card_stuck  = false;         ///< false = glance (tracks cursor); true = frozen (dismiss-by-leaving-rect).
    ImVec2    hover_card_anchor { -1.0f, -1.0f }; ///< Draw position: live cursor while glancing, frozen once stuck.
    ImVec2    hover_card_min    { 0.0f, 0.0f };   ///< Last drawn card rect (screen px) — hit-tested next frame to decide dismissal.
    ImVec2    hover_card_max    { 0.0f, 0.0f };


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

    /// Screen position (px) the last CONSUMED centre-on-tile request placed that
    /// tile's centre at — written by the Planetary canvas at the moment it applies
    /// the pan, read by the verify harness's click injection (BL-521) so a scripted
    /// click can land on a KNOWN tile without re-deriving the canvas transform in
    /// Lua. Presentation-only feedback, never serialised. {-1,-1} = never resolved.
    ImVec2 planetary_center_screen { -1.0f, -1.0f };
};
