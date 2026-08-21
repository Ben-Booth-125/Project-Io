#pragma once

#include "detail_level.hpp"     // fold_state — the drill-through disclosure target (BL-214)
#include "world/components.hpp"   // building_type / resource_type / sell_order
#include "world/corp_command.hpp" // corp_command — the seam the order-book presses queue onto
#include "world/entity.hpp"

#include <imgui.h>
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
    country,     ///< Country (nation) territory tint + owner borders. See LENSES.md § Country lens.
    corporation, ///< Corporate-owned tiles (per-corp tint; player-corp border). See LENSES.md.
    resource,    ///< Single-resource deposit lens: flat fill over the contiguous deposit. See LENSES.md § Resource lens.
    population,  ///< Per-tile habitability tint (dark → liveable green). See LENSES.md § Population lens.
    opportunity, ///< Per-catchment unmet-demand surface (green = market bid above base = a gap to fill). BL-112. See LENSES.md § Opportunity lens.
    production,  ///< Per-tile production-intensity surface (Σ output×price, log scale). See LENSES.md § Production lens.
    scarcity,    ///< Per-market supply-shortfall blocks (hot where demand outran supply). See LENSES.md § Scarcity lens.
    industry,    ///< Per-tile nation-substrate throughput field (occupation × terrain richness). See LENSES.md § Industry lens (BL-084).
    reach,       ///< Body-level commercial reach: bodies connected via the corp's trade_route entries, tiered by recency. BL-011. See LENSES.md § Reach lens.
    continent,   ///< Tectonic plates from the Continents/Drift pass: per-plate tint + boundary emphasis, read from the generation report. BL-226. See LENSES.md § Continent lens.
    supply_routes, ///< Aggregated trade_route graph: one edge per body pair, thickness from log-scaled convoy_count, colour from recency tier. BL-014. See LENSES.md § Supply-routes lens.
    count,         ///< Sentinel — keep last. The lens-cycle wrap (canvas_command.cpp) derives its modulus from this, so a new lens above is reachable without touching a hand-kept count.
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

    /// Pending demolition request — set by the building Selection element's Demolish
    /// control and executed by `app::render` via `demolish_building`. Takes the same
    /// deferred path as `pending_tile` for the same reason (UI surfaces hold only
    /// `const world&`), and additionally because erasing from `w.buildings` while a
    /// draw pass is iterating it would invalidate the iteration.
    entity_id     pending_demolish = null_entity;

    // (BL-343's pending_law_toggle lived here until BL-480: enactment is the
    // author nation's act now, so no player surface enqueues a law flip.)

    /// Last construction outcome, set by app after executing a request — a short
    /// human string shown by the build UI ("Built.", "Can't afford it.", …).
    std::string   last_message;

    /// Which bounded sub-view the Building window shows (2026-07-06 tabbed
    /// redesign; slimmed to two by BL-143 when the Build front door moved to the
    /// tile Selection element and Sell Orders moved to the Market Ledger).
    /// **0 = Construction** (the queue: "what's building?"),
    /// **1 = Buildings** ("what do I own?", plus the inline recipe/workforce
    /// detail for the selected row).
    ///
    /// Defaults to **Buildings** (BL-176): the queue is empty most of the time,
    /// so opening on it made the panel's front door an empty room, while the
    /// player always owns buildings. The old default of 0 predates the BL-143
    /// slim, when view 0 was the Build front door rather than a queue.
    int           panel_view = 1;

    /// Last building the panel auto-focused on (BL-176). The Buildings tab keys
    /// its selected row off the shared `ui_state::selected_entity`, so selecting
    /// a building anywhere already selects its row here; this field only tracks
    /// the EDGE, so newly selecting a building snaps the panel to the Buildings
    /// view once, without pinning it there every frame (which would stop the
    /// player ever reaching the queue).
    entity_id     panel_focus_building = null_entity;
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

    // --- navigation pane state ---
    // Policy: all ledgers start closed. The player opens them deliberately from
    // the navigation pane; none are shown on a fresh session.
    bool show_tile_ledger = false; ///< Whether the Tile Ledger window is open. Toggled by the nav pane tab and the window's close button.
    bool show_economy_panel = false; ///< Whether the Layer 3 economy panel is open. Toggled by the nav pane tab and the window's close button.
    bool show_construction_panel = false; ///< Whether the Layer 4 construction / building-management panel is open. Toggled by the nav pane tab and the window's close button.
    bool show_build_ledger = false;       ///< Whether the tile-contextual construction ledger is open (BL-162). Opened by the tile Selection element's "Construct Buildings" button; reads selected_entity as the target tile. Not a nav-rail ledger — closed by close_all_panels and by selecting a new entity. The Selection element itself lives in the bottom band (BL-213), not the fold-out column.
    bool show_market_ledger = false; ///< Whether the Market Ledger is open.
    bool show_balance_ledger = false; ///< Whether the Balance Ledger is open.

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

    /// Province Selection accordion page index (same role again) — Tiles /
    /// Deposits / Buildings. BL-511's refold (2026-08-21) moved the province off
    /// its own card and onto the shared Selection element's three-column band, so
    /// its three readings page exactly as every other selection's do. Clamped at
    /// the draw site, like the others.
    int selection_province_page = 0;

    /// The province (BL-511) the player single-clicked, or 0 for none. The
    /// province is the SELECTED unit on the Planetary canvas — a click that hits
    /// no marker lands here rather than on a tile — while the tile stays the data
    /// grain the Selection element then lists (deposits, terrain, buildings all remain
    /// tile-keyed). Province ids are derived, never allocated (world/province.hpp),
    /// so this is a plain id, not an entity: `selected_entity` and this field are
    /// mutually exclusive, and whichever is set last clears the other. The
    /// Selection element dispatches on this BEFORE `selection_kind_of`, so the
    /// band's "substitute the player corp when nothing is selected" rule (BL-266)
    /// cannot swallow a province selection. See SELECTION.md § Province.
    uint32_t selected_province = 0;

    /// The province under the cursor this frame, or 0. Written by the Planetary
    /// canvas draw pass from the hovered tile; drives the province hover outline.
    uint32_t hovered_province = 0;

    /// The value of `selected_entity` the Planetary canvas last wrote, so the
    /// canvas can tell "the player clicked a province" from "some OTHER surface
    /// — a ledger row, a corp list, a just-built building — moved the entity
    /// selection". The canvas is the only writer of `selected_province`, so
    /// without this the two fields could both read as set at once and the
    /// Selection element would have two things to draw. On a mismatch the entity
    /// selection wins and the province clears; see body_surface_canvas.cpp.
    entity_id province_sync_entity = null_entity;

    // --- Tile repeat-click selection cycle (placeholder unit-loop scaffolding) ---
    // Which tile the loop is currently anchored to, and which of the three fixed
    // stages (0 = unit, 1 = building, 2 = province) the selection currently sits
    // on. BL-511 moved the terminal stage from the tile to its province; a tile is
    // reached from the Selection element's Tiles page instead.
    // A click on hovered_tile == selection_cycle_tile advances the stage
    // (skipping stages with nothing there); a click elsewhere reseeds both.
    // Mirrors card_resource_page's per-selection reset idiom rather than adding a
    // separate "is this a repeat click" flag.
    entity_id selection_cycle_tile  = null_entity;
    int       selection_cycle_stage = 0;

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
    /// BL-248 as a duplicate of the Economy panel's Corps view. Ben restored it
    /// (NEEDS_REVIEW NR-012, 2026-08-01) — the deletion was not intended — and parked
    /// it on slot 8 (Diplomacy) so it is reachable and can be compared against the
    /// Corps view before its real home is chosen. Slot 8 is otherwise unbuilt, so
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
    int  economy_view = 0; ///< Economy panel: 0=Corps, 1=Holdings, 2=Markets (BL-117).

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
};
