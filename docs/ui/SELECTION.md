# Project Io — Selection Info Element

The **Selection info element** is a pinned **action surface** that answers one
question about the **current selection** — whatever entity the player last
single-clicked: **"what's my move here?"** It is the opposite of a ledger: a
ledger is exhaustive reference reached deliberately from the rail; this is a
decision prompt that appears when you click. One primary **action** per
selection kind, dominant; a slim line of decision-relevant **facts**; a
**'go to'** for the deep ledger where reference detail actually lives.

It is **not** reachable from the navigation rail — the only way to open it is
to select something — and it is **not** the stat-block encyclopedia it once
was (see § Removed: the stat-block polymorphism below); full per-entity detail
lives in the Tile Ledger, Market Ledger, Balance Ledger, etc., one 'go to' away.

See also: [LAYOUT.md](LAYOUT.md) (where it sits in the shell), [CANVASES.md](CANVASES.md)
(the click model it revises), [TOOLTIP.md](TOOLTIP.md) (the hover card — built), and
`src/ui/view_nav.hpp` (`focus_on_entity`, the 'go to' target).

---

## The three interaction states

This element forces us to name three distinct, previously-conflated pointer
states. They are independent: an entity can be any combination of active,
focused, and selected at once.

| State | Meaning | Lifetime | Drives | Backing |
|---|---|---|---|---|
| **Active** | The navigation **anchor** — which body/tile the canvas rungs are framed around. | Persists until you navigate. | Which Circumplanetary/Planetary rung renders, and around what. | `ui_state.active_body` (a never-read `active_tile` was removed, BL-363). |
| **Focus** | The entity **under the pointer** right now. | Transient, per-frame. | The hover tooltip / hover card (see [`TOOLTIP.md`](TOOLTIP.md)). | **Stored** since the hover card landed: `ui_state.hovered_entity` + `hover_ticks` (stable-hover detection), plus the `hover_card_*` fields (subject, glance/stuck phase, anchor, last rect). |
| **Selection** | The entity the player **single-clicked** to inspect. | Persists until you select something else (or clear). | The Selection info element's contents and its 'go to' target. | **New:** `ui_state.selected_entity`. |

Key consequence: **Selection is distinct from Active.** Selecting a body in the
Solar view fills the panel but does **not** move or re-anchor the canvas. Only
**navigation** (double-click, or the panel's 'go to' button) changes Active.

---

## Click model (revises CANVASES.md)

The canvases today use **single-click-descends**. This element splits that into
two gestures, applied uniformly across all three canvases:

- **Single-click an entity → select it.** Sets `selected_entity`, opens (or
  re-points) the Selection info element. **No view change** — same rung, same
  pan, same zoom. *On the Planetary canvas, a click that hits no marker selects
  the **province** rather than the tile (BL-511) — see § The province element.*
- **Double-click an entity → navigate to it.** The old descend/focus behaviour:
  routes through `ui::focus_on_entity`, which resolves the entity to its most
  informative view (descend a rung, focus a surface/tile, or — for non-spatial
  entities — open the relevant ledger).
- **Single-click empty space → clear the selection.** The band stays open and
  returns to its resting state — the player's own corporation (see § Always
  open below).

The 'go to' button on the panel is exactly equivalent to a double-click on the
current selection.

> **Superseded — dwell-to-open RETIRED (2026-07-30, BL-228/BL-230).** BL-200's
> second opener — holding the pointer still filled a progress bar in the hover
> tooltip and then opened the Selection surface — is removed. Hovering never
> opens the Selection band; **clicking is the only opener** — one gesture, one
> meaning (`hover_card.hpp`'s header comment is the code-side record). Hover
> now drives only the two-phase glance→stick card, TOOLTIP.md § The landed
> model. The original design, for the record: alongside the single-click,
> holding the pointer **still** over an entity filled a thin progress bar in
> the transient hover tooltip and then opened the card on that entity — the
> same open a click there produces. The click still opened instantly; dwell was
> an *addition*, not a replacement (Ben, 2026-07-23), scoped to the Planetary
> surface, the dwell bar hosted by the hover tooltip, never the opened card's
> header.

This is a deliberate behavioural change: a single click no longer descends the
zoom ladder. CANVASES.md and the minimap ascend gesture are updated to match.
(Minimap ascend stays a single click — the minimap has no selection semantics.)

---

## Action + Facts — content by selection type (BL-093)

The panel is **polymorphic by selection kind**, but no longer as a stat block.
The entity's kind is resolved by probing the `world` maps (the same
discrimination `focus_on_entity` already does: `w.tiles`, `w.buildings`,
`w.units`, `w.markets`, `w.bodies`, and later nations / corporations /
logistics vessels), and each kind renders through the same two-column split:

- **Action** (left, dominant, ~58% of the content width) — the ONE primary
  move for this kind, via `draw_selection_action` (`src/ui/selection_panel.cpp`).
- **Facts** (right, muted) — only what informs that action, via
  `draw_selection_facts`. Everything encyclopedic (orbit, composition,
  deposits, prices) has moved to the ledgers.

Each kind routes its 'go to' to the right place:

| Selection kind | Action (hero) | Facts (muted) | 'Go to' target |
|---|---|---|---|
| **Tile** | *(Superseded by the vertical tile layout, BL-123 — see § The tile element's layout below. The two-column action/facts split no longer renders for a tile.)* | | **No-op for now.** A tile is selected from the surface it lives on, so 'go to' has nothing to descend to; pan-to-tile is out of scope. |
| **Body** (planet/moon/asteroid/station/star) | Unsurveyed → **Dispatch Survey** (BL-067). Surveyed → **Go to surface** (descends via `focus_on_entity`). The star carries neither. | **Commercial activity** pulse (activity fog, BL-089) — see below. | Canvas: `focus_on_surface` — descend to the body's **Planetary tile surface**, the most informative rung. (Not `focus_on_body` / the orbital framing: landing on a sparse circumplanetary view reads as "nothing happened", which was the *only-works-for-Kepler* symptom.) |
| **Building** (any) | *(Superseded — see § The building element's layout below. Both player-owned and rival buildings take the 3-column band, not the action/facts split.)* | | Canvas: `focus_on_tile` (host tile). |
| **Market / Unit** | **Go to** — locate on the canvas. | (none yet; stubbed) | Canvas: `focus_on_surface` / entity's position. |
| **Nation / Corporation** | None — **"Open its ledger via [>]."** | (none yet; stubbed) | A ledger (no canvas of its own). |

So 'go to' is itself polymorphic: spatial entities navigate a canvas;
non-spatial entities (nation, corporation) open the relevant ledger. For the
prototype the spatial kinds (body, tile, building) are wired first; the rest are
designed here and stubbed.

### The tile element's layout (BL-123, reshaped BL-213)

A selected **Tile** does **not** use the two-column action/facts split above — it takes a
dedicated layout (`draw_tile_selection`, `src/ui/selection_panel.cpp`). Since the band widened
into three horizontal columns (Ben, 2026-07-28), the layout runs **left to right** rather than
top to bottom:

1. **Left quarter — zoomed hex neighbourhood.** A bordered render of the selected tile and its
   immediate ring (`draw_tile_neighbourhood`, radius 2), the selected tile picked out — this is
   the actual terrain render, not a placeholder image.
2. **Centre half — a paged metric ACCORDION**, one titled graph at a time (‹ Name (i/N) ›
   pager). Pages: every resource **deposited on the tile** (this tile's hazard-adjusted
   production vs. the top-decile tile for that resource, a **clustered column pair** sharing a
   baseline — "Tile" green, "Top 10%" muted, nice 1/2/5-rounded ceiling, dotted gridlines), THEN
   the tile's own **Habitability** and **Hazard** scalars (vs. this body's average — "Body avg"
   muted) so a barren tile still has something to page through. Only the deposited-resource pages
   are click-drillable into a time-series history (BL-196); habitability/hazard have no
   per-tile history tracked yet, so their chart is not a click target. Atmospheric pollution and
   per-tile population are **not modelled** today (population lives on population centres, not
   arbitrary tiles) and so have no page — a real content gap, not an oversight.
3. **Right quarter — a 2×3 action button grid** (2 columns × 3 rows; tried 3×2 first but the
   quarter-width column was too narrow for 3-across to read as "bigger" than the old icon strip —
   2 wide gives chunkier buttons in the same 6-slot count): **Construct Buildings** (opens the
   **tile construction ledger**, BL-162 — `draw_construction_ledger`, which lists the placeable
   building types for this tile and actually builds; see below), **Manage Buildings** (disabled
   unless a building occupies the tile; routes to the management panel), **History** and
   **Supply** (not yet wired — History has no surface yet, real Supply routing is Layer-5-gated
   per LENSES.md), plus **two reserved slots** so the grid's shape doesn't have to change when a
   fifth/sixth action lands.

### Multi-building tiles (BL-367)

BL-366 lifted the capacity-1 rule for non-extraction building types, so a tile can carry a
heterogeneous set — several extraction stacks against different deposits, several processors,
plus infrastructure. Three questions this raised, resolved 2026-08-11 from shipped precedent
rather than a new mockup:

- **Grouped by stack, not a flat list.** `placement_rules::stack_members` already groups a tile's
  buildings by `(type, target)`; the management surface (`draw_selected_section`,
  `src/ui/construction_panel.cpp`) mirrors that grouping instead of inventing a second one. A tile
  with more than one stack (or one stack with more than one member) shows a list — one row per
  stack, e.g. *"Extraction: Iron Ore x3"*, *"Processing: Steel Mill x2"* — before the single-building
  detail. A tile with exactly one building still routes straight to its detail, zero extra clicks
  for the common case.
- **Click model: the tile selects the aggregate; drilling into a row selects that stack.**
  Extends the single-click-selects model above rather than replacing it. On canvas, only ONE
  marker still renders per built tile (BL-231's dominant-silhouette convention) — for exactly one
  building the click still lands on that installation directly (Ben's 2026-07-22 "whole hex
  belongs to the installation" ruling, unchanged); for more than one it now falls through to the
  **tile**, whose Manage Buildings action opens the grouped list above. Selecting a row there sets
  the same `selected_entity` a canvas building-marker click would, so a single-building tile and a
  drilled-into stack reach the identical detail view either way. A **"‹ this tile's buildings"**
  back link on that detail view returns to the list when the tile carries siblings.
- **On-canvas marker: dominant-stack glyph + a "+N" count badge.** No new glyph shape — the
  marker keeps rendering the tile's lowest-id (dominant) building's silhouette, and a tile with
  more than one building gains a small "+N" text badge (N = additional buildings), lower-right,
  staggered past the existing BL-090 corp-identity tag which already sits there — the same k/N
  text-overlay idiom the Solar-canvas survey badge uses for region progress. See ICONS.md.

The whole band widened to make room for this (`shell_column_width` narrowed from its BL-124
~[480,576] range to ~[380,460] — see LAYOUT.md § The shell column; the BL-124 widening was
explicitly to host the Selection sidebar, which no longer lives in that column at all since
BL-213, so the extra width belongs to the band instead).

This **supersedes**, for tiles: the tile's action/facts row in the table above; the *Build front
door* and *affordance readout* subsections immediately below (their placement-suitability logic —
BL-071 Thrives/Valid/Invalid — is **not** shown on this panel and moves to the owed tile-construction
panel); and the BL-139 building sub-element (a building on the tile is now reached via **Manage
Buildings**, not an inline "On this tile" row). The other selection kinds are unaffected and keep the
action/facts form until they get their own mockups.

> The **tile construction ledger** (BL-162, `draw_construction_ledger`) is the surface "Construct
> Buildings" opens: a fold-out-column list of every building type placeable on the selected tile —
> placeholder image, full cost (budget + materials), a reason-coded validity read (e.g. *"A port must
> sit on the coast"*), and a **Build** action that enqueues the construction on the tile (the
> `construction.pending_tile` seam app executes). **First pass** — a follow-on adds the per-candidate
> **expected-profit** chart BL-162 calls for; today's images are placeholders.

### The tile element's action is the build front door *(superseded for tiles by BL-123 — see above; retained as the design of the owed tile-construction panel)*

> The "owed tile-construction panel" this and the next subsection point at is now owned by
> **BL-162 (tile construction panel)** — still open; its first pass, `draw_construction_ledger`,
> landed (the blockquote above). The nav-rail Building ledger's own slimming to
> Construction/Buildings tabs (BL-143, building ledger redesign) landed separately and does not
> absorb this per-tile logic.

The **Tile** selection's hero action is **"Build here"** — the player's primary construction
entry point (`draw_build_front_door`). It lists the building types placeable on the selected
tile (gated by `placement_rules::can_place`) with their **full construction cost — budget *and*
materials** (e.g. `100 cr · 20 Steel`, from the registry `build_cost` + `resource_build_cost`,
BL-044). A type is affordability-gated on **both**: the player corporation's balance *and* its
material pool on that body (`corp_body_pools`) — `construct_building` returns
`insufficient_materials` when the resources are absent, so the requirement is surfaced up front
rather than only on a failed click. Choosing an affordable type enqueues a construction request
that the mutable-world pass executes (`construct_building`). This is the deliberate design choice that
**building on one tile is a targeted action reached through the tile Selection element**, not a
reserved menu — the nav-rail construction surface stays a broad overview (see `docs/ui/MENU.md`,
BACKLOG § Ledger). The equivalent placement-mode canvas click enqueues the same request.

### The tile element's facts are its affordance readout (BL-071) *(superseded for tiles by BL-123 — this readout no longer renders on the Selection panel; it moves to the owed tile-construction panel)*

In the Facts column, a selected tile carries an **always-on affordance readout**
(`draw_tile_affordances`) — the *inverse* of the placement-suitability surface (`LENSES.md`,
BL-010). That surface answers "given an armed building, which tiles?"; this answers "given this
tile, which buildings?", **without arming anything**, so the player can read a tile before
committing to a build — the decision fact the Build action needs. It shows the tile's
**territory owner** and a **Thrives / Valid / Invalid** grouping over the prototype-buildable
types (extraction per deposited resource, processing, port), reading the same `placement_rules`
seam the front door and the armed canvas ghost use.

Rejection is **reason-coded, not silent**. `placement_rules::can_place[_in_world]` return a
`placement_result` — a `placement_reason` enum plus human string, implicitly convertible to
`bool` so existing boolean call sites are untouched — so an invalid type shows *why*
(`Cannot build on water`, `No extractable deposit here`, `A port must sit on the coast`, …). The
same reason string enriches the build front door (replacing the former bare "Cannot build on
water") and follows the cursor as a **"why not here"** label under the armed placement ghost on
the Planetary canvas. One vocabulary, three surfaces.

**Placement-time coexistence with the Construction panel (BL-082).** These affordance surfaces are
read *while a build is armed* — exactly when the Construction panel is open. Now that the Selection
element occupies the fold-out column rather than the bottom-left corner, the two no longer share the
corner; the Construction panel keeps its top-left anchor (see `LAYOUT.md` § Construction-panel
exception). The fix is **reposition, not fold** — folding the reason string into the Construction
panel would have rescued only the reason line, leaving the Thrives/Valid affordance readout
occluded; both must stay visible at the placement moment.

### The building element's layout (supersedes BL-074/BL-431, 2026-08-15 rework)

A selected **Building** — player-owned or rival — no longer takes the action/facts split at all.
It takes the **same 3-column band shape as the tile element** (§ below), just at different
proportions (`draw_building_selection_body`, `src/ui/selection_panel.cpp`), reached through the
Selection element's shared header (icon/title/kind/'go to' — unlike the tile element, the building
does not draw its own header):

1. **Left quarter — a generic placeholder image, keyed by building type** (2026-08-15). Was the
   zoomed hex-neighbourhood render; Ben's call was "generic per building type" over one flat image
   for everything. Draws `ui::icons::building` — the SAME type-keyed glyph vocabulary the Build door
   and the on-canvas markers use — enlarged to fill the panel. Identity (which named silhouette a
   `processing_facility` draws) resolves the same way the on-canvas marker does
   (`body_surface_canvas.cpp`): the active recipe's primary output, falling back to
   `target_resource` for extraction sites and infrastructure types the glyph ignores. Drawn for a
   rival building too — the type is already public (BL-068).
2. **Centre 5/12 — a paged accordion** (‹ Name (i/N) › pager, the same `disclosure_controls`
   full-canvas hook as the tile element's, `detail_surface::building_metric`). **Playtest rework,
   2026-08-15** (supersedes the 2026-08-15 BL-431 page split described above in earlier revisions
   of this doc): Chain and Depth are gone as pages — Chain's content folded into Profitability;
   Depth was cut outright — and Lifecycle is gone as a page too, its two controls moved onto the
   action grid (§ 3 below). `building_pages()` now builds a shorter list per building, in order:
   - **Profitability** (`building_page_kind::profitability`, wrapping `draw_building_profit`) —
     now **chart-based**, not text: no title line ("Profitability (est. / qtr)" is gone; the
     charts speak for themselves). **Reflowed to one screen (2026-08-15 playtest pass)** — no
     scrollbar: a top ROW holds the bars and the line chart side by side (left third / right
     two-thirds) rather than the earlier fully-stacked layout, and the vertical budget is derived
     from `ImGui::GetContentRegionAvail().y` at the top of the function (the accordion page body's
     real remaining height, not a guess — logged `NEEDS_REVIEW.json` NR-250 on the specific clamp
     numbers chosen). Three elements:
     - **Left third — Revenue vs a SEGMENTED Expenses bar** (`draw_revenue_expense_bars`, hand-drawn
       rather than `ui::charts::draw_bars` since draw_bars has no stacked-column notion). Revenue is
       one plain bar; Expenses stacks `input_cost` / `maintenance` / `wages` as three shaded
       segments in one column — the finest split `building_profit.hpp` tracks, since no revenue
       sub-breakdown exists to chart separately (logged NR-248). **Hovering a segment** shows a
       tooltip naming that segment ("Input cost: 12.4", etc.), mirroring the Inputs chart's
       hover-legend idiom below.
     - **Right two-thirds — a "Net, 6 mo." line chart** (`ImGui::PlotLines`) — no per-building
       profit HISTORY is tracked, so this is a smooth deterministic series anchored to the live
       net-profit estimate, 6 points for "6 months" (the same honest-placeholder idiom the
       Workforce trend graph already used, just with 6 points instead of 9; logged NR-249).
     - **Below the row — an "Inputs" bar chart** (`draw_input_basket_chart`) — the former Chain
       page's content, folded in: the active recipe's input quantities as bars, with a **hover
       tooltip** naming each input resource rather than a permanent legend. Absent for a building
       with no recipe (`extraction_site`), in which case the row above gets the full page height.
     Included only once the building is complete (`ticks_remaining <= 0`) **and**
     `estimate_building_profit` reports `has_data`; a still-building building has nothing here yet
     (its **Status** page below covers it instead).
   - **Method** (`building_page_kind::method`, `draw_production_method_section`) — "Which way
     should this building make its output?" Only for `processing_facility`. **Tiled grid layout**
     (2 columns) since the playtest rework, superseding the former single-column list: each
     era-allowed recipe gets its own bordered tile with just the recipe name and its expected
     profit (`estimate_prospective_profit`, priced at the building's real staffing) set in a
     **larger font** — the input-basket / wage-rate detail line under the profit figure is **gone**
     (2026-08-15 playtest pass; that detail is now implied by the Profitability page's Inputs chart
     and labelled Expenses segments instead) — and, for every recipe but the active one, a **big
     glyph Switch button** (`glyph_swap`, a two-arrow swap icon) sized to be the tile's dominant
     visual element. Calls `try_switch_recipe` exactly as `construction_panel.cpp`'s management
     dropdown does. A single-recipe building still gets the page (reads "Only one method
     available.").

     **BL-434 note (2026-08-15): no group label added here, by design.** The tile grid still lists
     every era-allowed recipe regardless of `group` — both the cheap intra-group switch and the
     pricier cross-group retool go through the same `try_switch_recipe` seam and the same Switch
     button, and `try_switch_recipe` already refuses (or silently prices) whichever the corp cannot
     afford. The Build door is where GROUP membership is the organizing axis (PRODUCTION.md § Sub-
     facility groups — one row per group, not per recipe); here on an existing building the axis is
     "every method this specific building could run", which a group label would not sharpen. Revisit
     only if playtest shows a player surprised by a cross-group switch's cost — the fix then is
     pricing the tile's Switch button, not adding a group tag to the grid.
   - **Workforce** (`building_page_kind::workforce`, `draw_building_workforce_page`) — "How much
     workforce, and by whose hand?" **Further trimmed (2026-08-15 playtest pass)**: the Auto button
     is **gone from this page** — moved to the action grid (§ 3 below) alongside a real bug fix (see
     there) — leaving just the placeholder trend graph (unchanged, still no per-building
     history — NR-249) and a single **horizontal 1% slider** (`ImGui::SliderInt`, 0–100) — editing
     it sets `workforce_target` directly and clears `workforce_auto`, the same "manual edit pins the
     target" semantic the old tier buttons and Auto button both had. Applies to any player-owned
     building regardless of type.
   - **Status** (`building_page_kind::status`) — the fallback: construction rate/ETA for a
     still-building building, "Operating." otherwise. **Rival buildings get ONLY this page** — the
     public building type plus (via `draw_rival_building_summary`) owner name, tile, and explicit
     `private` rows for production/stockpile (BL-068); nothing above would resolve for a rival
     without leaking withheld data, so `building_pages()` short-circuits to a single Status page
     for any non-player-owned building rather than testing each page's guard against data it must
     not show.

   `building_pages()` / `draw_building_page()` are the shared list+dispatch pair the in-band
   accordion and the full-canvas takeover (`draw_building_page_expanded`, `selection_card.cpp`)
   both read — the same precedent as the tile element's `tile_metrics` / `draw_tile_metric_chart`.
3. **Right quarter — a 2×3 action grid**, mirroring the tile element's grid. **Manage is gone**
   (playtest rework, 2026-08-15) — Ben's call was he no longer wants this link now that every
   control it used to route to lives on this card already (NR-245, resolved). Its slot plus one
   former reserved slot now hold the former Lifecycle page's two controls, moved here as
   building-level actions:
   - **Mothball** (`glyph_mothball`, a box with a line through it) — the old Close/Reopen toggle,
     relabelled: flips `decommissioned` (reversible — no output, no wages while closed) and
     invalidates the logistics anchor cache. A **toggle button** per the standing Toggle rule — its
     own active state (`decommissioned`) is what re-clicking undoes; the tooltip reads
     "Mothball..."/"Un-mothball..." depending on state.
   - **Dismantle** (`glyph_dismantle`, a simple X) — permanent, asks once via a confirm popup, then
     defers through `ui_state::construction.pending_demolish` for `app::render` to execute
     (`demolish_building`) — erasing mid-draw would dangle the `building_component&` the grid is
     reading.
   - **Auto** (`glyph_auto`, an open circular-arrow "refresh" primitive) — **relocated here from the
     Workforce page (2026-08-15 playtest pass)**, in Mothball's column, one row down: sets
     `b.workforce_auto = true` on press, the same one-way semantic the old page button had (a
     manual slider edit on the Workforce page is what clears it, unchanged). Active state
     (`workforce_auto == true`) is shown with the same accent-ring idiom the tile card's "primed"
     Construct button uses, rather than baking the state into the button's label text — **this was
     the fix for a real bug**: the old Workforce-page button built its label as
     `"Auto  (%d%%)"` with no `##` separator, so ImGui derived the button's identity from that
     text — which changed every tick while autosolving was live (`solve_workforce_target` re-solves
     `workforce_target` continuously), churning the button's ImGui ID frame over frame and
     corrupting its hover/active/focus state. The new grid button's id (`"##bld_auto"`) is stable;
     the percentage is not shown on this button at all (the Workforce page's own trend chart and
     slider still read it).
   Mothball, Dismantle and Auto are all disabled with "Competitor building - intel only" for a
   rival, same as Manage used to be. **Three slots remain reserved** (`glyph_reserved`).

`ui_state::selection_building_page` is the pager index — the sole survivor of the former
`selection_method_open` / `selection_chain_open` / `selection_chain_target` / `selection_depth_open`
quartet, reset to 0 alongside `card_resource_page` on every new selection (`app.cpp`).

### The unit (Soldier) element's layout (placeholder, 2026-08-15)

A selected **Unit** (`selection_kind::unit`) takes the **same 3-column band shape** as the
building/tile elements (`draw_unit_selection_body`, `src/ui/selection_panel.cpp`) instead of the
generic action/facts split — explicit scaffolding: BL-393 notes units are largely inert in the live
economy today, but Ben's direction was to build the card's shape now rather than wait for combat to
land.

1. **Left quarter — a generic humanoid placeholder glyph** (`glyph_soldier`): a filled circle head
   over a triangle body. No unit-type-keyed glyph vocabulary exists yet (unlike buildings'
   `ui::icons::building`), so this is an honest placeholder rather than a faked per-type icon.
2. **Centre half — a paged accordion** (`unit_pages()` / `draw_unit_page()`, the same
   list+dispatch-pair precedent as `tile_metrics` and `building_pages`), reading real
   `unit_component` fields only:
   - **Strength** — `strength` and `count`. NOTE: `unit_component::strength` is documented in
     `components.hpp` as a "fixed-point combat strength scalar (BL-157)", but every current writer
     (`corp_command.cpp`'s `hire_unit`, `corporation_generation.cpp`, `hard_coded_world.cpp`) sets
     it equal to the raw manpower count with no scale factor, and the existing hover-card reader
     (`entity_summary.cpp::draw_unit_summary`) already prints it raw — so this page prints the raw
     value too rather than guess a divisor that would disagree with the one other UI surface that
     shows this number. Logged to `NEEDS_REVIEW.json`.
   - **Roster** — `type` resolved through `unit_roster_table()` (real name: `hire_unit` sets
     `unit_component::type` from that table's own index, so this is a genuine lookup, not a guess;
     an out-of-range index falls back to `"Type %u"`) and `owner` resolved via the same corp-name
     lookup the building card's rival summary uses.
3. **Right quarter — a 2×3 action grid**: only **Go to** (`focus_on_entity`) is wired; the other
   five slots are reserved (`glyph_reserved`), matching the tile/building cards' own pattern.

`ui_state::selection_unit_page` is the pager index, mirroring `selection_building_page`.

### The province element (BL-511, 2026-08-21)

**On the Planetary canvas the selected unit is the PROVINCE, not the tile.** A click that hits no
marker glyph now lands on the hovered tile's province (`world/province.hpp` — the ~4-tile 2×2 cell
BL-466 builds). The tile is **not retired**: deposits, terrain, buildings and richness all stay
tile-keyed, and Ben's ruling is explicit that tiles "are just going to be rendered differently, but
still instrumental unit values". What changed is which of the two the player *presses*.

A province is not an entity, so it cannot travel in `selected_entity`. It has its own field:

| Field | Meaning |
|---|---|
| `ui_state::selected_province` | The selected province id, or `0` for none. |
| `ui_state::hovered_province` | The province under the cursor, driving the hover outline. |
| `ui_state::province_sync_entity` | The `selected_entity` value the canvas last wrote. |

**The two selections are mutually exclusive**, and the canvas is the single place that keeps them
so. `selected_province` has exactly one writer (the Planetary canvas); `selected_entity` has many
(ledger rows, the corporation list, "inspect the thing I just built"). Each frame the canvas
compares `selected_entity` against `province_sync_entity`: a mismatch means some *other* surface
moved the entity selection, that surface wins, and the province clears. Keeping the reconciliation
in one place beats adding a clear-me duty to every selecting surface.

`draw_selection_content` therefore dispatches on `selected_province` **before** `selection_kind_of`.
It has to: the band substitutes the player's corporation whenever the entity selection is empty
(BL-266, § Always open), which is exactly the state a province selection leaves behind, so a later
test would be swallowed by the substitution.

**The card's job is to give the tile back.** The canvas blends four tiles into one soft shape; the
card un-blends them:

- **Header** — `Province [x, y] · N tiles`, anchored on the lowest-id member tile's grid position.
  Province ids are derived and opaque (body rank | block raster | component), so the raw id is not
  a name a player can hold; the anchor coordinate is.
- **Mixture bar** — one segment per member tile, in the province's own ascending-tile-id order,
  each in exactly the colour the canvas gives that tile. This is the blend's legend: "what did that
  gradient just average?" is answered at a glance instead of by zooming in and counting.
- **Tiles** — every member tile as a press. Selecting one clears the province and hands over the
  full tile card (deposits, the neighbourhood hex view, the Construct door). **Building placement
  did not move to province grain**, so this list is also the route to building.
- **Deposits** — summed across the province. A deposit is a stock, and for a player deciding
  whether a locality is worth a mine, four tiles each holding a little iron is one province holding
  that much iron.
- **Buildings** — the roll-up of what already stands here, each a press that selects the building.
  A locality question that used to mean clicking four hexes.

An ocean or otherwise unpartitioned tile has no province and still selects as a **tile**, so
clicking water selects something rather than nothing.

### Tile repeat-click selection cycle (placeholder, 2026-08-15; retargeted BL-511)

`body_surface_canvas.cpp`'s click handler cycles **Soldier → Building → Province → Soldier** on a
**repeat** click at the same tile the selection already sits on, skipping any stage with nothing
there (no unit on the tile skips straight to Building; no building skips to Province).
`ui_state::selection_cycle_tile` / `selection_cycle_stage` track the anchor tile and current stage.

> **BL-511 changed the terminal stage from Tile to Province.** Because a province is expressed as
> `selected_entity = null_entity` **plus** a province id, the stage table's "nothing here, skip it"
> test can no longer be a null check on the last stage — the handler carries an explicit
> `stage_live[]` alongside the entity table. On a tile with no province (ocean) the stage falls back
> to the tile, so the cycle never strands on an empty rung.

A click on a **different** tile (or the first click anywhere) leaves the existing marker-hit
precedence (BL-031: building > market_centre) completely untouched — it only additionally seeds the
cycle anchor so a follow-up repeat click knows where to advance from. This is scaffolding ahead of
units mostly existing in the live economy (BL-393) — deliberately built now per Ben's direction,
not gated on combat landing first.

### The body element's action is the survey front door (then go-to-surface)

An unsurveyed **Body**'s hero action is its **Survey section** (survey system, BL-067),
keyed on the body's survey phase (`draw_survey_section`, called from `draw_selection_action`).
See [DISCOVERY.md](DISCOVERY.md) for the model authority (the geographic fog this section reads).

- **`hidden`** — a **Dispatch Survey** button with a `cost cr · ETA days` preview (cost and ETA
  derived from size + distance). Affordability-gated against the player corporation's balance;
  disabled with an "Insufficient funds." reason when the player cannot pay.
- **`in_transit`** — `En route — ETA <days> d`.
- **`scanning`** — `Surveying <k>/<N> — ETA <days> d`.
- **`surveyed`** — the hero action switches to **"Go to surface"**, which descends via
  `focus_on_entity` — surveying is done, the move now is to look.

The Dispatch Survey button only **enqueues** `ui_state::pending_survey_dispatch`; the
mutable-world pass in `app::render` performs the upfront debit and arms the schedule
(`dispatch_survey`), exactly as construction requests are executed — the UI surfaces hold a
`const world&`. The star carries neither the survey action nor the go-to-surface action.

### The body element's fact is its commercial activity pulse (BL-089)

A selected **Body**'s (not the star's) Facts column carries a **Commercial activity**
section (`draw_activity_section`) keyed on `body_activity_visibility` (the activity fog,
BL-089) — independent of survey phase, so it can be populated on an unsurveyed body and empty
on a surveyed one. See [DISCOVERY.md](DISCOVERY.md) for the model authority (`activity_vis`,
the tier derivation).

- **`unknown`** — "Outside your trade network - no market data." No further content.
- **`known` / `visible`** — a coarse **market pulse** (`busy` / `steady` / `quiet`, derived from
  the body's aggregate market throughput); `visible` additionally notes "Live lane / your
  presence." No per-building production or stockpiles — competitor internals stay private
  (BL-068).
- **`known_stale`** — greyed: "Route gone cold - last market read is stale."

---

## Removed: the stat-block polymorphism (BL-093)

The panel's earlier form (pre-2026-07-04) rendered a **50/50 split** — an encyclopedic per-kind
stat block (`draw_summary`, dispatching to `draw_body_summary` / `draw_tile_summary` /
`draw_building_summary` / … in `entity_summary.hpp`) beside a **lens supplement**
(`draw_lens_supplement`) that re-rendered overlay-keyed market price / production rate /
population-workforce rows inline — literally duplicating the Market / Production / Population
ledgers. Both **are removed**: `draw_summary` and `draw_lens_supplement` no longer exist in
`src/ui/selection_panel.cpp`. Facts and actions carried equal weight and everything read as
uniform flat text, so the panel had no clear reason to exist — it read as "an amalgamation of
all the menu ledgers" (Ben's playtest finding, 2026-07-04). All that reference detail — orbit,
parent, composition, deposits, prices — now lives in the ledgers, one `go to` away; the header
identity line (name · type) is the only trace of it left in the panel itself.

The shared per-entity content builders in `entity_summary.hpp` (`draw_body_summary`,
`draw_tile_summary`, `draw_building_summary`, …) are unaffected by this removal — they remain
the Tile Ledger's content and the future hover card's planned content; only the Selection
element stopped calling them.

---

## Layout & chrome

**Current shape (BL-213, 2026-07-28) — the Selection band.** The element lives in a **fixed
band at the bottom of the screen**. Its left neighbour is the **comms dock** (BL-227,
2026-07-30 — the comms log moved from the right column to the bottom-left tile of this same
strip, `comms_dock_rect` in `src/ui/foldout_column.{hpp,cpp}`); its right neighbour is the
right chrome column (time panel + minimap). Dock and band share one top edge and one height,
so the screen's bottom row reads as a single solid strip. This superseded two earlier shapes in turn: the original
BL-065 full-width bottom bar, then the BL-124 shell-column sidebar, then the BL-194/195
**click-anchored "sticky card"** that froze at the click position and centred there (canvas-
confined, clamped so it stayed on-screen). Ben's 2026-07-28 call retired the click-anchoring:
*"menus that are to do with doing/building need space at the bottom of the screen, rather than
floating with the cursor."* The band is now:

- **Fixed, not click-anchored.** Always the same rect — left edge at the **comms dock's right
  edge** (the dock takes three quarters of the fold-out column's width; the band takes the
  quarter it gives back, BL-227), right edge at the right chrome column, bottom-anchored at
  `selection_band_height` tall (`app.cpp`'s band block; the frame is
  `src/ui/selection_card.{hpp,cpp}`) — regardless of where the selecting click landed. The
  player's eye never has to re-find it.
- **Not mutually exclusive with the ledgers.** Because it no longer shares the shell column,
  selecting an entity **leaves whatever fold-out ledger is open untouched** — both are visible
  at once. (This reverses the BL-124 sidebar's rule, which closed the ledger on a new selection
  because the two competed for the same column.)
- **Height is display-derived, not a constant** (2026-07-30). `selection_band_height(disp_x,
  disp_y)` = minimap height + `chrome_margin` (`src/ui/foldout_column.{hpp,cpp}`), so the
  band's top edge lands exactly on the minimap's and the bottom row reads as one band. The
  flat 340 px it replaced overhung the minimap by 80 px at 1720×1080 — "discordant", Ben's
  word — with the main canvas practically blocked.
- **Content unchanged in kind.** `draw_selection_content` (selection_panel.cpp) is still the
  shared per-kind dispatcher; only its container moved. The tile's vertical layout (hex render +
  accordion, § below) and the action|facts split (other kinds) render exactly as before, just in
  a wide-short band instead of a narrow-tall sidebar or a small floating card.
- **Header row:** a small coloured **kind icon** (`draw_selection_icon` — circle for body,
  square for building, outlined square for tile, pentagon otherwise; a first pass ahead of a
  richer per-entity icon), then the title line (name · type), then a right-aligned **`[>]`**
  ('go to') button. There is no close button (see § Always open).
- **Always open — dismissal retired (BL-266, 2026-08-09).** The band never hides:
  the `[x]` button, the Esc hide rung, and the `selection_hidden_for` state are
  all removed. Dismissal was right for the BL-194/195 floating card over an open
  canvas; with the shell filling the perimeter, a hidden band reads as a hole in
  the frame (Ben, NR-017).

  Being selection-driven with no rail slot, the element is still **not** governed
  by the universal toggle rule: that rule toggles the rail's ledgers, not this
  element.
- **Resting state — the player's own corporation.** With nothing selected
  (fresh session, or after clicking empty space) the band rests on the player
  corp, through the same `selection_kind::corporation` builder any selection
  uses. Deselecting means "stop looking at that, back to looking at yourself".

  `selected_entity` stays null while resting, so deselect remains representable;
  the band substitutes the player corp at draw time and never renders
  `selection_kind::none`. The player corp exists before the first frame, so
  there is no bootstrap gap.
- **Esc ends at the system menu.** With the hide rung gone, Esc's ladder is:
  exit-confirm → close system menu → pop a drill level → corp roll-up →
  fold overlay → open the system menu. The "pause screen" IS `show_system_menu`
  (settled 2026-08-02); nothing new was built for it.

**Click-through.** The band draws as an ordinary ImGui window over the canvas; ImGui's
`WantCaptureMouse` flag (already how every other chrome window — nav rail, header, minimap —
prevents the canvas from receiving clicks underneath it) covers the band the same way, so no
special-case hit-testing was needed.

### Lens strip relocation (BL-093)

The overlay-lens control strip (`ui::draw_overlay_controls`, `src/ui/overlay.cpp`) no longer
lives beneath the Selection element — it moved onto the **minimap**, reprising its pre-BL-013
location. A lens mode bar now occupies the bottom row of the minimap box (chrome fill drawn in
`app.cpp`'s minimap block; the interactive glyph row is `draw_overlay_controls(ui, x, top_y, w)`
positioned over it), leaving the Selection element the full height it needs. The strip itself
also trimmed from 9 lenses to a single row — **eight** since BL-226 (Continent lens) joined
the original seven: Corporation, Country, Resource, Market, Population, Opportunity,
Production, Continent. **Scarcity** and **Industry** stay off the on-screen row (joining
**Supply**, **Reach**, and **Supply-routes**) as keyboard-cycle only — the minimap bar does
not fit them all. The Resource/Market/Scarcity resource-selector, formerly a 140px inline
combo, moved again: it now lives in the **on-canvas lens legend** (BL-134, lens selector in
legend), not on the bar. Full detail: [LENSES.md](LENSES.md), [MINIMAP.md](MINIMAP.md).

---

## Lens-driven hover & selection resolution (settled 2026-06-15, [F4])

A single pointer position overlaps a **stack** of entities — a building sits *on* a tile sits *on*
a body. Both Focus (hover) and Selection (click) resolve that stack to exactly one entity. The
rule has two parts: a fixed stack order, and a lens-evaluated validity filter over it.

**The kind stack (most-specific → least).** At a pointer position the candidate entities are
ordered:

```
building → market → unit  →  tile  →  body
```

(the marker kinds first, then the tile they occupy, then the body that hosts it). "Lowest" means
**most-specific** — earliest in that order.

**Lowest *valid* entity.** Resolution walks the stack from most-specific and returns the **first
entity the active lens deems valid**. Validity is not fixed: it is **the active lens's question**
(§ Per-lens validity in LENSES.md). With **no lens** every drawn kind is valid, so resolution
returns the literal lowest entity (a building over a tile resolves to the building; bare terrain
resolves to the tile). Under a lens, kinds the lens does not care about are *skipped*, so the same
pointer position can resolve to a **different entity per lens**.

**The lens names the ledger the selection drives.** Resolving the entity and choosing its 'go to'
ledger are the *same* decision — the lens that validates the entity also routes it:

| Active lens | Resolves to | 'Go to' / routes to |
|---|---|---|
| **none** (terrain) | the tile (or the lowest drawn marker on it) | Tile Ledger |
| **Corporation** | the owning **corporation** of the hovered tile/building | Balance Ledger |
| **Resource** | the hovered tile's **deposit** | Tile Ledger (deposit detail) |
| **Market** | the body's **market** / listing | Market Ledger |
| **Country** *(was Faction)* | the owning **nation** | Nation ledger |
| **Supply** *(Layer 5)* | the **route / stockpile** under the pointer | (Supply surface, when it exists) |

This couples the Selection element's Focus state to the lens system (LENSES.md) and the ledger
family (BACKLOG § Ledger): the 'go to' dispatch seam (`focus_on_entity`) is unchanged — the lens
only changes *which entity id and which target* are handed to it. The same dispatch carries the
non-spatial 'go to' routing (nation/corporation → ledger) already specified above.

**Wiring order.** This is the design rule; it lands once its prerequisites exist — canvas
hit-testing for the marker kinds (§ Open questions below) and the per-lens ledgers (the [A4]
family). Until then the rule is dormant: with no lens and no marker hit-testing, resolution
degrades to today's tile/body behaviour, which is the `none`-lens row above.

## Open questions / deferred

- **Multi-select.** Out of scope; the model is single-selection. A future
  drag-box or shift-click would extend `selected_entity` to a set.
- **Selection persistence across navigation.** A selection survives canvas
  navigation (it is independent of Active). Whether a stale selection (entity
  destroyed) auto-clears is an implementation detail — treat a missing id as
  "no selection".
- **Hit-testing non-body entities on the canvas.** Buildings, units, and markets
  are not yet independently click-selectable on the surface canvas (they appear
  as Tile Ledger rows). Canvas hit-testing for them is its own task; until then
  those kinds are selected only from within ledgers.
