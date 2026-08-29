#pragma once

#include "detail_level.hpp"     // fold_state — the drill-through disclosure target (BL-214)
#include "world/components.hpp"   // building_type / resource_type / sell_order
#include "world/corp_command.hpp" // corp_command — the seam the order-book presses queue onto
#include "world/entity.hpp"

#include <imgui.h>
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    corporation,///< Corporate-owned tiles (per-corp tint; player-corp border). See LENSES.md.
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
    company,       ///< Background-firm holdings, drawn exactly as the Corporation lens draws corporations. See LENSES.md § Company lens.
    count,         ///< Sentinel — keep last. The lens-cycle wrap (canvas_command.cpp) derives its modulus from this, so a new lens above is reachable without touching a hand-kept count.
};

/// A selectable on-canvas marker registered each frame by the Planetary canvas
/// draw pass. Hit-test priority (highest first): unit > building > market_centre
/// > tile. Unit outranks building (BL-575) to match the repeat-click cycle's own
/// order (battle, unit, province, building, tile — SELECTION.md), so a unit
/// standing on a built tile is reachable on the FIRST click rather than only
/// after cycling past the building.
/// Cleared and rebuilt every frame by body_surface_canvas. See BL-059, BL-031.
struct marker_hit_zone
{
    entity_id id = null_entity; ///< The entity this marker represents (building, market, or — for `kind::unit` — the lowest-id unit in the province/owner group the marker draws).
    enum class kind : uint8_t { building, market_centre, unit } kind = kind::building;
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
    corporation, ///< A corporation's holdings on the active body (Corporation lens).
    // A COMPANY IS ITS OWN KIND, not a corporation with a flag (BL-665). The two
    // are different words since the 2026-08-28 split (GLOSSARY.md) and Ben ruled
    // they reach different destinations, so the difference belongs in the answer
    // the resolver gives rather than being re-derived by every routing site.
    company,     ///< A background firm's holdings on the active body (Company lens).
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

    /// Pending hire-unit request (BL-324) — set by the build front door's Hire
    /// affordance (the construction ledger's roster section) and executed by
    /// `app::render` via `apply_corp_command`'s `hire_unit` verb. Same deferred
    /// path as `pending_tile`, for the same reason (const-world UI surfaces).
    /// `null_entity` = nothing pending; `pending_hire_unit_type` is an index
    /// into `unit_roster_table()`.
    entity_id     pending_hire_tile      = null_entity;
    std::uint16_t pending_hire_unit_type = 0;

    /// Pending player convoy-dispatch request (BL-601) — set by the market
    /// Selection card's dispatch form and executed by `app::render` via
    /// `apply_corp_command`'s `dispatch_convoy` verb (BL-452's own player seam;
    /// the form is the front door onto it SUPPLY.md always specified but nothing
    /// built). Same deferred path as `pending_tile`, for the same reason (UI
    /// surfaces hold only `const world&`). `pending_dispatch_source` = null_entity
    /// means nothing pending; the other three fields are meaningless until it is set.
    entity_id     pending_dispatch_source = null_entity; ///< Source market (the card's own selection).
    entity_id     pending_dispatch_dest   = null_entity; ///< Destination market.
    resource_type pending_dispatch_good   = resource_type::iron_ore;
    float         pending_dispatch_qty    = 0.0f;

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

/// Where the Construction ledger's own controls landed on screen this frame, so a
/// verify script can press the REAL control rather than a computed guess.
///
/// PUBLISHED RATHER THAN RE-DERIVED, for the same reason `acquisitions_buy_x` is:
/// the surface that drew the thing is the only honest source for where it landed,
/// and a script re-deriving a row's y from `shell_column_width` plus a guessed line
/// count would be asserting against its own arithmetic. Every field is rewritten
/// each frame the surface draws and reset to "absent" (`x < 0`) when it does not.
/// VIEW state, not serialised.
struct construction_controls
{
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
    entity_id    selected_entity = null_entity;       ///< The entity the player single-clicked to inspect — drives the Selection info element. Distinct from the active_* anchors: selecting never moves the canvas. null_entity = nothing selected. See SELECTION.md, ui/selection.hpp.
    canvas_level primary_level = canvas_level::solar; ///< Which canvas rung fills the window.
    overlay_mode overlay       = overlay_mode::none; ///< Active canvas overlay lens; toggled by the bottom overlay control strip. Defaults to none — the plain canvas — at campaign start (reverses BL-013's Corporation-default): a click never re-skins the canvas, so it opens unskinned. Single-select with a null state (re-clicking clears to none).

    // --- lens-local selector state (Resource / Market / Scarcity lenses) ---
    // One shared "which resource" selection drives the Resource lens's contiguous
    // deposit fill, the Market lens's price surface, and the Scarcity lens's
    // shortfall surface (LENSES.md says the selectors share a form). Only the
    // active lens reads it. The Resource lens is always single-resource (BL-019),
    // so it carries no highest-value toggle.
    resource_type lens_resource = resource_type::iron_ore; ///< Selected resource (Resource deposit / Scarcity shortfall) / good (Market price surface).
    /// Is the lens legend expanded? BL-533 (Ben, 2026-08-22) re-homed every
    /// legend into the right chrome column above the minimap and made it a
    /// dropdown that is COLLAPSED BY DEFAULT — so a lens switch no longer
    /// throws a forty-row list over the map. Only the header shows until the
    /// player opens it. One flag serves every legend: they are mutually
    /// exclusive by construction (a lens draws at most one).
    bool lens_key_open = false;
    /// Which section of the tile Selection accordion stands OPEN: 0 Buildings,
    /// 1 Deposits, 2 Resources, 3 Population, 4 Terrain — or -1 for none open
    /// (BL-598, Ben 2026-08-24). One section at a time: the band is a fixed
    /// ~260 px tall, so five open bodies would each get a sliver.
    ///
    /// -1 is REACHABLE, and it has to be: a section header shows its own open
    /// state, which makes it a toggle under the standing Toggle rule, so
    /// pressing the open header closes it. The five names run from what the
    /// player can act on to what the ground merely is; the province is a
    /// SECTION here rather than a selection of its own.
    ///
    /// card_resource_page still selects WHICH resource inside the Resources
    /// section, through a dropdown.
    int card_tile_view = 0;
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

    /// Whether the Company ledger is open (BL-666). A column occupant with NO
    /// nav-rail slot: a company is not a population the player
    /// browses, it is the thing under the cursor when a Company-lens holding is
    /// clicked, so the only way in is that click. Closed by close_all_panels
    /// like every other tenant of the fold-out column.
    ///
    /// The surface it opens is a declared PLACEHOLDER — it answers none of the
    /// five axes a real ledger design answers (docs/ui/ledgers/README.md) and
    /// says so on its face. It exists because Ben's 2026-08-28 ruling on the
    /// corporation/company split requires the two lenses to land somewhere
    /// DIFFERENT on click, and the company half had no destination at all.
    bool show_company_ledger = false;

    /// Which background firm the Company ledger is about — the click's payload,
    /// held apart from `selected_entity` because the two answer different
    /// questions: `selected_entity` stays the clicked HOLDING (a building, what
    /// the Selection band inspects), while this names the FIRM behind it.
    /// Collapsing them would make the ledger reinterpret a building id as a corp
    /// id every frame. null_entity => the ledger draws its empty state rather
    /// than vanishing, so a stale click reads as "nothing here", not as a bug.
    entity_id selected_company = null_entity;

    /// Which corporation the all-corporations table should aim its row at
    /// (BL-666, the Corporation-lens half of the same ruling). Kept distinct
    /// from `selected_company` for the same reason the two lenses are distinct
    /// words: a corporation is the player or a rival, a company is a background
    /// firm, and the two never share a destination. Written by the canvas click,
    /// read by the corporations table; null_entity => the table aims at no row
    /// in particular and lists as it always has.
    entity_id selected_corporation_dossier = null_entity;
    bool show_balance_ledger = false; ///< Whether the Balance Ledger is open.

    // ── The Acquisitions ledger ───────────────────────────────────────────
    //
    // Nav-rail slot 5, inserted ABOVE Market (Ben, 2026-08-29), which pushes
    // every slot from Market down by one. Two doors reach the same surface:
    // this rail slot, and the Company lens's click destination (a background
    // firm's holdings resolve through to the firm, and the firm's question is
    // "can I buy it").
    //
    // MEASURED BEFORE IT WAS LAID OUT, AND THE NUMBER HAS SINCE MOVED BY FIFTY
    // TIMES. Over a TWELVE-seed sweep of the shipped spawn
    // (`tools/verify/acquisition_viability.cpp` § C), 88 corporations per seed
    // now yield a BUYABLE FIELD of a mean 81.6 firms — Purchasable 35.8,
    // Possible 45.8, neither group empty on any seed. It was 1.6 when this
    // surface was designed, because ownership class was overwhelmingly `closed`
    // at a mean of 84.8 per seed; retiring closure for background firms took
    // that to 4.8. Filing has never been the binding gate — public-but-unfiled
    // is zero on every seed.
    //
    // The layout was built for the one-row case and was NOT padded to look
    // busier. It needed no change to hold eighty, which is the payoff for
    // having measured the field rather than assumed it.
    /// Whether the Acquisitions ledger is open. Toggled by nav-rail slot 5 and
    /// opened by a Company-lens click.
    bool show_acquisitions_ledger = false;

    /// Which of the two groups are expanded. Both open at rest: with a mean of
    /// two rows in the whole field, a collapsed group hides the entire answer.
    bool acquisitions_purchasable_open = true;
    bool acquisitions_possible_open    = true;

    /// The corporation a Company-lens click aimed at, or `null_entity`. The
    /// ledger highlights its row so the click lands somewhere visible rather
    /// than merely opening a list. Never a filter — the field is two rows long
    /// and filtering it to one would answer a question nobody asked.
    entity_id acquisitions_focus_corp = null_entity;

    /// Where the FIRST Purchasable row's Buy button actually is, in screen
    /// pixels, published by the ledger each frame it draws one; `x < 0` means
    /// there is none. `acquisitions_buy_corp` names the firm that button buys.
    ///
    /// PUBLISHED RATHER THAN RE-DERIVED, for the same reason
    /// `planetary_center_screen` is: the ledger is the only honest source for
    /// "the screen point that is this press". A verify script computing it from
    /// `shell_column_width` plus a guessed line count would be asserting against
    /// its own arithmetic, and would silently start clicking empty column the
    /// first time a line of prose was added above the table. VIEW state, not
    /// serialised, rewritten every frame.
    float     acquisitions_buy_x    = -1.0f;
    float     acquisitions_buy_y    = -1.0f;
    entity_id acquisitions_buy_corp = null_entity;

    /// Where the Construction ledger's tabs, type-group headers, building rows and
    /// the building card's open-in-ledger button landed this frame. Same reasoning
    /// as `acquisitions_buy_x` above; see `construction_controls`.
    construction_controls construction_ui;

    /// The profitability fold-out's sort: column index, and direction. VIEW
    /// state, not serialised. -1 is "unsorted" — filed order, which is the
    /// sorted-`entity_id` walk `apply_budget` itself writes in, so the resting
    /// order is deterministic rather than an unordered map's layout.
    int  acquisitions_sort_column    = -1;
    bool acquisitions_sort_ascending = true;

    /// The profitability fold-out's three CROSS-CUTTING SELECTORS (BL-680,
    /// profitability filters): end resource, input resource, body. All three
    /// are supply-chain questions — "who makes the thing I need", "who competes
    /// with me for the input I buy", "who is within reach" — and they combine
    /// as AND.
    ///
    /// They are SELECTORS, NOT VIEWS, so the toggle rule does not bind them,
    /// exactly as it does not bind the Market ledger's Body and Market combos:
    /// a selector switches a target rather than expressing an active state, and
    /// re-picking the current value is a no-op rather than an undo.
    ///
    /// Held here rather than in a function-local static for the same reason the
    /// sort above is: the shell's windows carry `NoSavedSettings`, so ImGui
    /// forgets everything the moment the takeover closes, and a filter that
    /// silently reset on every close would read as the surface losing the
    /// player's question. VIEW state, not serialised.
    ///
    /// `-1` / `null_entity` is "every" in each case — the absence of a filter,
    /// never a value that could be confused with resource index 0.
    int       acquisitions_filter_end   = -1;
    int       acquisitions_filter_input = -1;
    entity_id acquisitions_filter_body  = null_entity;

    /// Where the three filter combos are drawn, and where each one's first real
    /// option lands once its popup is open, in screen pixels. Index 0 = end
    /// resource, 1 = input resource, 2 = body. `x < 0` means not drawn this
    /// frame (the fold-out is closed, or the table had no rows to filter).
    ///
    /// PUBLISHED RATHER THAN RE-DERIVED, for the same reason
    /// `acquisitions_buy_x` is: the surface that drew the control is the only
    /// honest source for where it landed. A verify script that computed a combo
    /// centre from the canvas rect plus a guessed line count would be asserting
    /// against its own arithmetic, and would start clicking empty canvas the
    /// first time a line of prose moved above it. The OPTION position is
    /// published separately because a combo popup is its own window: clicking
    /// the combo only opens it, and selecting a value needs a second real
    /// press on a real item. VIEW state, rewritten every frame.
    float acquisitions_filter_x[3]     = { -1.0f, -1.0f, -1.0f };
    float acquisitions_filter_y[3]     = { -1.0f, -1.0f, -1.0f };
    float acquisitions_filter_opt_x[3] = { -1.0f, -1.0f, -1.0f };
    float acquisitions_filter_opt_y[3] = { -1.0f, -1.0f, -1.0f };

    /// The corporations the profitability table ACTUALLY DREW this frame, in
    /// draw order — after the disclosure listing rule and after all three
    /// filters.
    ///
    /// Published because the alternative is worse: a verify script that
    /// re-applied the filter rule to an unfiltered row set would be asserting
    /// against its own arithmetic, and would keep passing if the surface stopped
    /// filtering altogether. This is the surface reporting what it put on
    /// screen, which is the only thing worth checking. Empty when the fold-out
    /// is closed, or when a filter combination excluded every row. VIEW state,
    /// not serialised, rewritten every frame.
    std::vector<entity_id> acquisitions_profit_shown;

    /// Whether the Contracts ledger is open (BL-576). Nav-rail slot 13 —
    /// the curated nine and the developer/observability tail (slots 1-12,
    /// MENU.md) were already full when this landed, so Contracts is a new
    /// PLAYER-system slot appended after the tail rather than inside it (see
    /// nav_pane.cpp's own comment on slot 13 for the reasoning).
    bool show_contracts_ledger = false;

    /// Contracts ledger view tab: 0=Offers, 1=Active, 2=History — the same
    /// `ui::nav_button` button-strip idiom every split ledger uses
    /// (LAYOUT.md § One-question-per-view splits).
    int contracts_ledger_view = 0;

    /// The Accept press's force picker (BL-576). Non-zero names the
    /// `mercenary_offer::id` currently being staffed; the popup lists the
    /// player's own uncommitted units with a checkbox each, writing into
    /// `contracts_picker_units` below. Reset to 0 when the popup closes
    /// (Confirm or Cancel) — there is no cross-frame "armed" state visible
    /// outside the popup itself, unlike March's canvas-spanning two-step,
    /// because picking a force needs no second surface: everything the
    /// player touches lives inside this one ledger.
    uint32_t contracts_picker_offer = 0;

    /// Which owned units are checked in the open force picker, indexed
    /// arbitrarily (unused slots stay `null_entity`) — the exact shape
    /// `corp_command::units` and `mercenary_contract::units` already carry,
    /// so Confirm copies this straight into the command with no translation.
    /// Cleared whenever `contracts_picker_offer` changes to a different
    /// offer (or to 0), so a stale pick from one offer cannot leak into
    /// another's Accept.
    std::array<entity_id, mercenary_contract_max_units> contracts_picker_units{};

    /// Whether the Generation Ledger is open (BL-303). A DEVELOPER TUNING surface,
    /// not shipped chrome — it explains why a tile generated as it did — so like
    /// every other ledger it starts closed and nothing opens it but a deliberate
    /// press. See ui/generation_ledger.hpp, docs/generation/GENERATION_LEDGER.md.
    bool show_generation_ledger = false;

    /// Generation Ledger: 0=Body (histograms, thresholds, profile echo),
    /// 1=Tile (the per-tile derivation breadcrumb). In ui_state, like every other
    /// panel view index, so a verify script can park the ledger on a view.
    int  generation_ledger_view = 0;

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

    // --- Building Selection card accordion (supersedes BL-431's three toggles) ---
    // The building card now takes the tile card's 3-column band shape: Profitability /
    // Method / Chain / Depth (whichever apply) each get a PAGE rather than a
    // stack of independently-toggled sections, so one pager index replaces the
    // former selection_method_open / selection_chain_open / selection_chain_target /
    // selection_depth_open quartet — a page is always fully rendered while shown,
    // there is nothing left to hide-and-reveal within it. Not clamped to the
    // current building's page count here (that vector is rebuilt per-frame); the
    // draw site clamps against it, same as the tile card's card_resource_page.
    int selection_building_page = 0;

    /// Unit Selection card accordion page index (mirrors selection_building_page's
    /// role for the new Soldier card) — Strength / Roster, whichever apply.
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
    // There is no canvas marker for a contract (CONTRACTS.md's ledger-and-map
    // framing puts a contract's PROVINCE on the map, not the contract itself),
    // so the setter is a future ledger row press (BL-576, Contracts ledger) —
    // whichever surface sets this must clear the others, the same duty every
    // other selecting surface already carries.
    uint32_t selected_contract_id = 0;

    bool has_contract_selection() const { return selected_contract_id != 0; }

    void clear_contract_selection() { selected_contract_id = 0; }

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
    // there); a click elsewhere reseeds both. Mirrors card_resource_page's
    // per-selection reset idiom rather than adding a separate "is this a repeat
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

    // --- Budget ledger stubbed policy levers (BL-171 UI; mechanics owed to BL-155) ---
    // The Tax and Wages tier selectors are drawn and selectable, but have NO economic
    // effect yet — they carry the intended player levers (Tax = a player-set policy;
    // Wages = a cost↔workforce trade-off) whose mechanics are designed under BL-155.
    // Tiers run 1–5 (I–V); defaults match the mockup (Tax IV, Wages II).
    int  budget_tax_tier  = 4;
    int  budget_wage_tier = 2;
    bool show_corporation_panel = false; ///< Whether the Corporation Overview Dashboard is open.

    /// Whether the all-corporations balance table is open (corporation_panel.cpp).
    ///
    /// PROVISIONAL HOME. This table used to occupy nav slot 1, and was deleted by
    /// BL-248 as a duplicate of a second aggregate view. Ben restored it
    /// (NEEDS_REVIEW NR-012, 2026-08-01) — the deletion was not intended — and parked
    /// it on slot 8 (Diplomacy) so it is reachable while its real home is chosen.
    /// Slot 8 is otherwise unbuilt, so
    /// nothing is displaced; when Diplomacy is actually designed this occupant moves.
    bool show_corporations_table = false;

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
    /// Market Ledger: 0=Prices, 1=Sell Orders (BL-159 — sell-order management
    /// relocated here from the Construction/Building panel), 2=Convoys (BL-453 —
    /// the player's cargo in flight, with its ticks-to-arrival; drawn on three
    /// canvases before this and listed on none).
    int  market_ledger_view = 0;

    /// History ledger: 0=Story (the body's biography), 1=Chain (the generation
    /// charts), 2=Tiles (the tile/building/market tables), 3=Ages (the Era -1
    /// political time-lapse, BL-277). BL-211.
    int  history_view = 0;

    /// Ages view: the year currently scrubbed to, and whether playback is
    /// running. VIEW state — not serialised; where the scrubber was parked is a
    /// display detail, not part of the campaign.
    int  ages_year    = 0;
    bool ages_playing = false;

    /// F9 mock tech-tree viewer: one era per view — 0=Era -1 Antiquity (placeholder),
    /// 1=Era 0, 2=Era 1, 3=Standing lines. Defaults to Era 0, the campaign's era.
    int  tech_tree_view = 1;

    /// Radial constellation canvas (BL-310) — views 1/2 only. Pan offset of the
    /// web centre from the canvas centre (screen px) and scroll-wheel zoom,
    /// same idiom as planetary_pan_x/planetary_zoom (body_surface_canvas.cpp).
    /// Kept per-view-agnostic (one camera for both era tabs) since switching
    /// tabs is a content change, not a navigation the player needs to retain.
    float tech_tree_pan_x = 0.0f;
    float tech_tree_pan_y = 0.0f;
    float tech_tree_zoom  = 1.0f;

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

    /// Pending order-book commands (BL-293). The standing orders themselves moved
    /// to `world::sell_orders` on 2026-08-07; what lives here is the REQUEST, for
    /// the reason `pending_survey_dispatch` above lives here — the Market Ledger
    /// holds a `const world&` and cannot mutate it, so it enqueues and
    /// `app::render` applies.
    ///
    /// Applied through `apply_corp_command`, the same seam the rival-corp AI
    /// drives, so the player's press and the AI's command are the same code path
    /// by construction rather than by two implementations agreeing. A queue rather
    /// than a single slot because a frame can carry more than one press (add an
    /// order, remove another); drained in order each frame.
    std::vector<corp_command> pending_order_commands;

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
    struct convoy_beam { std::vector<entity_id> path; float progress = 0.0f; float speed = 0.0f; };
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

    // --- selection band recursive drill-down (BL-196) ---
    // One drilled frame: a resource time-series view opened from a tile card's
    // per-resource graph. The card shows a STACK of these over the root selection
    // (empty = the root). Esc / the card's back button pop one; at the root a
    // dismiss hides the card. Acyclic + depth-capped (see selection_panel.cpp).
    struct card_drill
    {
        entity_id tile     = null_entity; ///< The tile whose resource series this frame plots.
        int       resource = -1;          ///< Resource index (into resource_deposit[]).
        int       scroll   = -1;          ///< Left edge (sample index) of the visible 8-quarter window; -1 = rightmost (most recent). Set by the chart's horizontal scrollbar.
    };
    std::vector<card_drill> card_stack;   ///< Drill frames over the root selection; empty = root.

    /// Which deposited resource the tile card's bottom-third accordion currently
    /// shows (Ben's 2026-07-23: page left/right through the tile's resources rather
    /// than infinite-scroll). An index into the tile's *deposited* resources (not the
    /// raw resource enum); clamped to that list each frame, reset to 0 on a new selection.
    int card_resource_page = 0;

    /// Per-frame request from the card: begin recording this tile's per-resource
    /// deposit series (BL-198 lazy tracking). app drains it into its tracked-tile
    /// set each frame; null_entity = no request. Idempotent — set while a resource
    /// drill for the tile is open, so the tile stays tracked from first drill on.
    entity_id card_track_tile = null_entity;

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
