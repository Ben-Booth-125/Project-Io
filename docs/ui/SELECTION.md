# Project Io — Selection Info Element

The **Selection info element** is a pinned **action surface** that answers one
question about the **current selection** — whatever entity the player last
single-clicked: **"what's my move here?"** It is the opposite of a ledger: a
ledger is exhaustive reference reached deliberately from the rail; this is a
decision prompt that appears when you click. One primary **action** per
selection kind, dominant; a slim line of decision-relevant **facts**; a
**'go to'** for the deep ledger where reference detail actually lives.

It is **not** reachable from the navigation rail — the only way to open it is
to select something — and it is **not** a stat-block encyclopedia (see § Not a
stat block below); full per-entity detail lives in the Tile Ledger, Market
Ledger, Balance Ledger, etc., one 'go to' away.

See also: [LAYOUT.md](LAYOUT.md) (where it sits in the shell), [CANVASES.md](CANVASES.md)
(the click model), [TOOLTIP.md](TOOLTIP.md) (the hover card), and
`src/ui/view_nav.hpp` (`focus_on_entity`, the 'go to' target).

---

## The three interaction states

Three distinct pointer states, independent of one another: an entity can be any
combination of active, focused, and selected at once.

| State | Meaning | Lifetime | Drives | Backing |
|---|---|---|---|---|
| **Active** | The navigation **anchor** — which body/tile the canvas rungs are framed around. | Persists until you navigate. | Which Circumplanetary/Planetary rung renders, and around what. | `ui_state.active_body`. |
| **Focus** | The entity **under the pointer** right now. | Transient, per-frame. | The hover card (see [`TOOLTIP.md`](TOOLTIP.md)). | `ui_state.hovered_entity` + `hover_ticks` (stable-hover detection), plus the `hover_card_*` fields (subject, glance/stuck phase, anchor, last rect). |
| **Selection** | The entity the player **single-clicked** to inspect. | Persists until you select something else (or clear). | The Selection info element's contents and its 'go to' target. | `ui_state.selected_entity`. |

Key consequence: **Selection is distinct from Active.** Selecting a body in the
Solar view fills the panel but does **not** move or re-anchor the canvas. Only
**navigation** (double-click, or the panel's 'go to' button) changes Active.

---

## Click model

Two gestures, applied uniformly across all three canvases:

- **Single-click an entity → select it.** Sets `selected_entity`, opens (or
  re-points) the Selection info element. **No view change** — same rung, same
  pan, same zoom. *On the Planetary canvas, a click that hits no marker selects
  the **tile**, and the tile carries its province — see § The province is a
  section, not a selection.*
- **Double-click an entity → navigate to it.** Routes through
  `ui::focus_on_entity`, which resolves the entity to its most informative view
  (descend a rung, focus a surface/tile, or — for non-spatial entities — open
  the relevant ledger).
- **Single-click empty space → clear the selection.** The band stays open and
  returns to its resting state — the player's own corporation (see § Always
  open below).

The 'go to' button on the panel is exactly equivalent to a double-click on the
current selection.

**Clicking is the only opener.** Hovering never opens the Selection band — one
gesture, one meaning (`hover_card.hpp`'s header comment is the code-side record).
Hover drives only the two-phase glance→stick card, TOOLTIP.md. A dwell-to-open
opener is ruled out (Ben, 2026-07-30).

A single click never descends the zoom ladder. The minimap ascend gesture stays
a single click — the minimap has no selection semantics.

**Everything above describes the canvas with NO LENS active.** Under a lens the click model
collapses to one tier and the marker precedence inverts — see § A lens collapses selection to ONE
TIER, which supersedes this section wherever the two differ.

---

## Action + Facts — content by selection type

The panel is **polymorphic by selection kind**, but not as a stat block. The
entity's kind is resolved by probing the `world` maps (the same discrimination
`focus_on_entity` does: `w.tiles`, `w.buildings`, `w.units`, `w.markets`,
`w.bodies`, nations and corporations), and the kinds without a dedicated layout
render through the same two-column split:

- **Action** (left, dominant, ~58% of the content width) — the ONE primary
  move for this kind, via `draw_selection_action` (`src/ui/selection_panel.cpp`).
- **Facts** (right, muted) — only what informs that action, via
  `draw_selection_facts`. Everything encyclopedic (orbit, composition,
  deposits, prices) lives in the ledgers.

Each kind routes its 'go to' to the right place:

| Selection kind | Action (hero) | Facts (muted) | 'Go to' target |
|---|---|---|---|
| **Tile** | Dedicated three-column band — § The tile element's layout. | | **No-op.** A tile is selected from the surface it lives on, so 'go to' has nothing to descend to; pan-to-tile is out of scope. |
| **Body** (planet/moon/asteroid/station/star) | Unsurveyed → **Dispatch Survey**. Surveyed → **Go to surface** (descends via `focus_on_entity`). The star carries neither. | **Commercial activity** pulse (activity fog) — see below. | Canvas: `focus_on_surface` — descend to the body's **Planetary tile surface**, the most informative rung. (Not `focus_on_body` / the orbital framing: landing on a sparse circumplanetary view reads as "nothing happened".) |
| **Building** (any) | Dedicated three-column band — § The building element's layout. Player-owned and rival buildings alike. | | Canvas: `focus_on_tile` (host tile). |
| **Unit** | Dedicated three-column band — § The unit element's layout. | | Canvas: entity's position. |
| **Market** | **Go to** — locate on the canvas. | (none) | Canvas: `focus_on_surface`. |
| **Nation / Corporation** | None — **"Open its ledger via [>]."** | (none) | A ledger (no canvas of its own). |

So 'go to' is itself polymorphic: spatial entities navigate a canvas;
non-spatial entities (nation, corporation) open the relevant ledger.

### The tile element's layout

A selected **Tile** takes a dedicated layout (`draw_tile_selection`,
`src/ui/selection_panel.cpp`) running **left to right** across the band's three
horizontal columns (Ben, 2026-07-28):

1. **Left quarter — zoomed hex neighbourhood.** A bordered render of the selected tile and its
   immediate ring (`draw_tile_neighbourhood`, radius 2), the selected tile picked out — the
   actual terrain render, not a placeholder image.
2. **Centre half — a SECTION TOP NAV over five sections**, in this order
   (`ui_state::card_tile_view` holds which is showing):

   > **Buildings → Deposits → Resources → Population → Terrain**

   **The order is the argument, not a shuffle.** It runs from what the player can *act on* —
   what can still be built here — through what is there to be taken (the stock, its yield, the
   workforce that would take it) to what the ground merely *is*. The reading you can do
   nothing about is last.

   **The nav is one row, and the section below it takes the whole body.** Ben, 2026-08-24:
   *"a topnav left and right chevron, with a full canvas expansion button… straddle left and
   right buttons across the entire span, excepting the expand chevron. And our open accordion
   element title should be centred."*

   - **The chevrons straddle the span** — hard left, hard right — rather than clustering
     around the title, so the two presses sit the largest possible distance apart.
   - **The title is centred on the run between them**, with an `i/N` count that says how many
     readings exist. That count is what an accordion's stack of headers was buying, at one row
     instead of five.
   - **The full-canvas control keeps the rightmost slot** and is excepted from the straddle —
     the same two-control disclosure idiom every other surface uses, and `disclosure_controls`
     owns its glyph. `in_place` is false here: a section is already the whole body, so the only
     larger state is the canvas.
   - **The nav wraps** in both directions, so five presses of either chevron return to where
     you started.

   *A vertical accordion was built first, on Ben's earlier ruling the same day, and ruled out
   on sight. The measurement is why: five stacked headers spent **169 of the band's 258 px** on
   chrome to leave the open section **89**. The nav spends one frame height.*

   - **Buildings** — a table **per province throughout** (Ben, 2026-08-22), the selected tile
     serving only to name which province is meant: the province's total building count against
     its ceiling (`-1` is UNKNOWN and is said, never rendered as room), then one row per
     workable resource with **Built** (extraction sites in the province targeting it) and
     **Max** (the province's placement capacity for it — summed `stack_capacity` over member
     tiles that would accept an extraction site, computed through the same `can_place_in_world`
     the placement seam uses so the table cannot disagree with the build button). No chart: the
     question is a comparison of small integers.
   - **Deposits** — the province's deposits **summed** across its member tiles, richest first.
     Summing is the right reduction because a deposit is a **stock**: for a player deciding
     whether a locality is worth a mine, several tiles each holding a little iron *is* one
     province holding that much iron. On unpartitioned ground (ocean) the section falls back to
     the tile's own deposits and says so, rather than reading as "nothing here".
   - **Resources** — one chart per resource **deposited on the selected tile**, chosen through
     a **dropdown** rather than paged (reading the seventh deposit must not cost six presses):
     this tile's hazard-adjusted production vs. the top-decile tile for that resource, a
     **clustered column pair** sharing a baseline — "Tile" green, "Top 10%" muted, nice
     1/2/5-rounded ceiling, dotted gridlines. `ui_state::card_resource_page` selects the
     resource. Only these charts are click-drillable into a time-series history (BL-196,
     resource drill-down). Deposits *sums* where Resources *charts*: the two ask different
     questions of the same ground, which is why both earn a section.
   - **Population** — the population centres standing in the tile's province: the province
     total in thousands, then each centre by name with its scale word (Outpost / Settlement /
     Town / City / Metropolis, POPULATION.md § Scale bonus model), headcount and habitability,
     largest first; a centre standing on the *selected tile itself* is marked. This section is
     province-grain because the **data model** is — population lives on population centres, not
     on arbitrary tiles — so a per-tile version would have to invent a number.
   - **Terrain** — the tile's own **Habitability** and **Hazard** scalars, each charted against
     this body's average ("Body avg" muted), so a barren tile still has something to read.
     Chosen through the same dropdown Resources uses; they carry no per-tile history, so their
     chart is not a click target.

   Resources and Terrain draw through one `tile_metrics` list — the full-canvas fold charts the
   same pages by index, so the split is a filter, not a second builder — and share one chart
   body, so the drill and the disclosure control cannot come to differ between them.

   Atmospheric pollution is **not modelled** and so has no section.
3. **Right quarter — a 2×3 action button grid** (2 columns × 3 rows — the quarter-width
   column is too narrow for 3-across to read as bigger than an icon strip): **Construct
   Buildings** (opens the **tile construction ledger** — `draw_construction_ledger`, see below;
   drawn with a "primed" accent ring when something is placeable and nothing is under way),
   **Manage Buildings** (disabled unless a building occupies the tile; routes to the
   management surface), **History** and **Supply** (both drawn disabled — History has no
   surface; supply routing is the Supply lens's subject, LENSES.md), plus **two reserved
   slots** so the grid's shape never changes when a fifth or sixth action arrives.

### Multi-building tiles

A tile can carry a heterogeneous set — several extraction stacks against different deposits,
several processors, plus infrastructure (only extraction carries a capacity rule). Three
consequences, settled 2026-08-11:

- **Grouped by stack, not a flat list.** `placement_rules::stack_members` groups a tile's
  buildings by `(type, target)`; the management surface (`draw_selected_section`,
  `src/ui/construction_panel.cpp`) mirrors that grouping. A tile with more than one stack (or
  one stack with more than one member) shows a list — one row per stack, e.g. *"Extraction:
  Iron Ore x3"*, *"Processing: Steel Mill x2"* — before the single-building detail. A tile with
  exactly one building routes straight to its detail, zero extra clicks for the common case.
- **Click model: the tile selects the aggregate; drilling into a row selects that stack.**
  On canvas, only ONE marker renders per built tile (the dominant-silhouette convention) — for
  exactly one building the click lands on that installation directly (Ben, 2026-07-22: the
  whole hex belongs to the installation); for more than one it falls through to the **tile**,
  whose Manage Buildings action opens the grouped list. Selecting a row there sets the same
  `selected_entity` a canvas building-marker click would, so a single-building tile and a
  drilled-into stack reach the identical detail view either way. A **"‹ this tile's buildings"**
  back link on that detail view returns to the list when the tile carries siblings.
- **On-canvas marker: dominant-stack glyph + a "+N" count badge.** The marker renders the
  tile's lowest-id (dominant) building's silhouette, and a tile with more than one building
  gains a small "+N" text badge (N = additional buildings), lower-right, staggered past the
  corp-identity tag that sits there — the same k/N text-overlay idiom the Solar-canvas survey
  badge uses. See ICONS.md.

### The tile construction ledger

**Construct Buildings** opens the **tile construction ledger** (BL-162, tile construction
panel; `draw_construction_ledger`): a fold-out-column list of every building type placeable
on the selected tile, folded by family (Extraction / Processing / Infrastructure / Military —
Ben rejected a profit-ranked flat list) — each row its type glyph (`icons::building`), full
cost (budget + materials), a reason-coded validity read (e.g. *"A port must sit on the
coast"*), an **expected-profit** bar (`estimate_prospective_profit`, one ceiling shared across
the list so the bars compare directly), and a **Build** action that enqueues the construction
on the tile (the `construction.pending_tile` seam `app::render` executes). A processing row
stands for its group, showing the best-expected-profit recipe (PRODUCTION.md § Sub-facility
groups). This is the deliberate choice that **building on one tile is a
targeted action reached through the tile Selection element**, not a reserved menu — the
nav-rail construction surface stays a broad overview (`docs/ui/MENU.md`). The equivalent
placement-mode canvas click enqueues the same request.

Placeability is gated by `placement_rules::can_place`, and a type's cost is **budget *and*
materials** (e.g. `100 cr · 20 Steel`, from the registry `build_cost` + `resource_build_cost`).
Affordability is gated on both: the player corporation's balance *and* its material pool on
that body (`corp_body_pools`) — `construct_building` returns `insufficient_materials` when the
resources are absent, so the requirement is surfaced up front rather than only on a failed
click.

**Rejection is reason-coded, not silent** — for a row the ledger actually shows. A DIFFERENT
class of refusal never becomes a row at all: `construction_result::era_locked`,
`depth_locked` and (since BL-593) `tech_locked` are filtered out of the candidate list before
it renders, on one repeated argument stated in the filter's own code comment — *"the door not
showing what the gate would refuse."* All three lock kinds resolve differently (never
available at all / earned by building / earned by research, PRODUCTION.md § Chain depth) but
share the same door-side treatment: not a reason string, an absent row. `refined_copper`
(`E0-EC-03`, BL-589) was the first recipe the tech clause actually removed — every earlier
tech gate targeted a `building_type`, never a recipe, so the branch was dead code until then.
Filtered was the ruled choice over shown-and-locked (Ben, 2026-08-24) — extending the existing
era/depth precedent rather than adding a new lock-reason string and UI affordance.

`placement_rules::can_place[_in_world]` return a `placement_result` — a `placement_reason`
enum plus human string, implicitly convertible to `bool` — so an invalid type that DOES survive
the filter shows *why* (`Cannot build on water`, `No extractable deposit here`, `A port must
sit on the coast`, …). The same reason string follows the cursor as a **"why not here"** label
under the armed placement ghost on the Planetary canvas, and the placement-suitability surface
(LENSES.md § Placement-suitability surface) reads the same `placement_rules` seam. One
vocabulary, three surfaces — for the buildings the door was willing to name at all.

**Placement-time coexistence with the Construction panel.** These readouts are read *while a
build is armed* — exactly when the Construction panel is open. The Selection element occupies
the bottom band and the Construction panel keeps its fold-out column, so both stay visible at
the placement moment; the reason string is never folded into the Construction panel, which
would rescue only the reason line.

### The building element's layout

A selected **Building** — player-owned or rival — takes the **same 3-column band shape as the
tile element** at different proportions (`draw_building_selection_body`,
`src/ui/selection_panel.cpp`), reached through the Selection element's shared header
(icon/title/kind/'go to' — unlike the tile element, the building does not draw its own
header):

1. **Left quarter — a generic placeholder image, keyed by building type** (Ben: generic per
   building type, not one flat image for everything). Draws `ui::icons::building` — the SAME
   type-keyed glyph vocabulary the construction ledger and the on-canvas markers use — enlarged
   to fill the panel. Identity (which named silhouette a `processing_facility` draws) resolves
   the same way the on-canvas marker does (`body_surface_canvas.cpp`): the active recipe's
   primary output, falling back to `target_resource` for extraction sites and infrastructure
   types the glyph ignores. Drawn for a rival building too — the type is public
   (DISCOVERY.md, the competitor-visibility rule).
2. **Centre 5/12 — a paged accordion** (‹ Name (i/N) › pager, the same `disclosure_controls`
   full-canvas hook as the tile element's, `detail_surface::building_metric`).
   `building_pages()` builds the list per building, in order:
   - **Profitability** (`building_page_kind::profitability`, wrapping `draw_building_profit`) —
     **chart-based**, no title line, laid out to **one screen** with no scrollbar: the vertical
     budget is `ImGui::GetContentRegionAvail().y` at the top of the function (the accordion
     page body's real remaining height; NR-250 records the clamp numbers). Three elements:
     - **Left third — Revenue vs a SEGMENTED Expenses bar** (`draw_revenue_expense_bars`,
       hand-drawn since `ui::charts::draw_bars` has no stacked-column notion). Revenue is one
       plain bar; Expenses stacks `input_cost` / `maintenance` / `wages` as three shaded
       segments in one column — the finest split `building_profit.hpp` tracks (NR-248).
       **Hovering a segment** shows a tooltip naming it ("Input cost: 12.4", etc.).
     - **Right two-thirds — a "Net, 6 mo." line chart** (`ImGui::PlotLines`) — no per-building
       profit HISTORY is tracked, so this is a smooth deterministic series anchored to the live
       net-profit estimate, 6 points for "6 months" — the honest-placeholder idiom (NR-249).
     - **Below the row — an "Inputs" bar chart** (`draw_input_basket_chart`): the active
       recipe's input quantities as bars, with a **hover tooltip** naming each input resource
       rather than a permanent legend. Absent for a building with no recipe
       (`extraction_site`), in which case the row above takes the full page height.
     Included only once the building is complete (`ticks_remaining <= 0`) **and**
     `estimate_building_profit` reports `has_data`; a still-building building has nothing here
     (its **Status** page covers it).
   - **Method** (`building_page_kind::method`, `draw_production_method_section`) — "Which way
     should this building make its output?" Only for `processing_facility`. A **tiled grid**
     (2 columns): each era-allowed recipe gets its own bordered tile with the recipe name and
     its expected profit (`estimate_prospective_profit`, priced at the building's real staffing)
     in a **larger font**, and, for every recipe but the active one, a **big glyph Switch
     button** (`glyph_swap`, a two-arrow swap icon) sized as the tile's dominant visual element.
     Calls `try_switch_recipe` exactly as `construction_panel.cpp`'s management dropdown does. A
     single-recipe building still gets the page ("Only one method available.").

     **No group label here, by design.** The grid lists every era-allowed recipe regardless of
     `group` — the cheap intra-group switch and the pricier cross-group retool both go through
     `try_switch_recipe`, which refuses (or prices) whichever the corp cannot afford. The
     construction ledger is where GROUP membership is the organising axis (PRODUCTION.md § Sub-
     facility groups); on an existing building the axis is "every method this building could
     run". If a player is surprised by a cross-group switch's cost, the fix is pricing the
     Switch button, not adding a group tag.
   - **Workforce** (`building_page_kind::workforce`, `draw_building_workforce_page`) — "How much
     workforce, and by whose hand?" A placeholder trend graph (no per-building history —
     NR-249) and a single **horizontal 1% slider** (`ImGui::SliderInt`, 0–100): editing it sets
     `workforce_target` directly and clears `workforce_auto` — a manual edit pins the target.
     Any player-owned building, regardless of type. The Auto control lives on the action grid.
   - **Status** (`building_page_kind::status`) — the fallback: construction rate/ETA for a
     still-building building, "Operating." otherwise. **Rival buildings get ONLY this page** —
     the public building type plus (via `draw_rival_building_summary`) owner name, tile, and
     explicit `private` rows for production/stockpile; `building_pages()` short-circuits to a
     single Status page for any non-player-owned building rather than testing each page's
     guard against data it must not show.

   `building_pages()` / `draw_building_page()` are the shared list+dispatch pair the in-band
   accordion and the full-canvas takeover (`draw_building_page_expanded`, `selection_card.cpp`)
   both read — the same precedent as the tile element's `tile_metrics` / `draw_tile_metric_chart`.
3. **Right quarter — a 2×3 action grid**, mirroring the tile element's grid. There is no
   Manage link — every control it would route to lives on this card (Ben, NR-245). Three
   building-level actions:
   - **Mothball** (`glyph_mothball`, a box with a line through it) — flips `decommissioned`
     (reversible — no output, no wages while closed) and invalidates the logistics anchor
     cache. A **toggle button** per the standing Toggle rule — its own active state
     (`decommissioned`) is what re-clicking undoes; the tooltip reads "Mothball..." /
     "Un-mothball..." by state.
   - **Dismantle** (`glyph_dismantle`, a simple X) — permanent, asks once via a confirm popup,
     then defers through `ui_state::construction.pending_demolish` for `app::render` to execute
     (`demolish_building`) — erasing mid-draw would dangle the `building_component&` the grid
     is reading.
   - **Auto** (`glyph_auto`, an open circular-arrow "refresh" primitive), in Mothball's column
     one row down: sets `b.workforce_auto = true` on press, one-way (a manual slider edit on
     the Workforce page is what clears it). Active state (`workforce_auto == true`) is shown
     with the same accent-ring idiom the tile card's "primed" Construct button uses, never
     baked into the label text: the button's id (`"##bld_auto"`) is **stable** — a label that
     carried the live percentage would change every tick while `solve_workforce_target`
     re-solves, churning the ImGui ID and corrupting hover/active/focus state. The percentage
     is read on the Workforce page, not here.
   Mothball, Dismantle and Auto are all disabled with "Competitor building - intel only" for a
   rival. **Three slots are reserved** (`glyph_reserved`).

`ui_state::selection_building_page` is the pager index, reset to 0 alongside
`card_resource_page` on every new selection (`app.cpp`).

### The unit element's layout

A selected **Unit** (`selection_kind::unit`) takes the **same 3-column band shape** as the
building/tile elements (`draw_unit_selection_body`, `src/ui/selection_panel.cpp`) — the
card's shape is built ahead of units carrying much in the live economy (Ben's direction).

1. **Left quarter — a generic humanoid placeholder glyph** (`glyph_soldier`): a filled circle
   head over a triangle body. There is no unit-type-keyed glyph vocabulary (unlike buildings'
   `ui::icons::building`), so this is an honest placeholder rather than a faked per-type icon.
2. **Centre half — a paged accordion** (`unit_pages()` / `draw_unit_page()`, the same
   list+dispatch-pair precedent as `tile_metrics` and `building_pages`), reading real
   `unit_component` fields only:
   - **Strength** — `strength` and `count`. `unit_component::strength` is documented in
     `components.hpp` as a fixed-point combat strength scalar, but every writer
     (`corp_command.cpp`'s `hire_unit`, `corporation_generation.cpp`, `hard_coded_world.cpp`)
     sets it equal to the raw manpower count with no scale factor, and the hover-card reader
     (`entity_summary.cpp::draw_unit_summary`) prints it raw — so this page prints the raw
     value too, rather than guess a divisor that would disagree with the other surface.
   - **Roster** — `type` resolved through `unit_roster_table()` (`hire_unit` sets
     `unit_component::type` from that table's own index; an out-of-range index falls back to
     `"Type %u"`) and `owner` resolved via the same corp-name lookup the building card's rival
     summary uses.
3. **Right quarter — a 2×3 action grid**: **Go to** (`focus_on_entity`) plus five reserved
   slots (`glyph_reserved`), matching the tile/building cards' pattern.

`ui_state::selection_unit_page` is the pager index, mirroring `selection_building_page`.

### The province is a section, not a selection

**The province is reached through the tile element, not beside it** (Ben, 2026-08-24):

> *"Province selection element must be bundled into the tile selection element. By this I mean,
> we just use a longer accordion."*

A province is **not** a kind the Selection element can be. Selecting a tile *is* selecting its
province: the tile element's accordion carries the province readings — **Buildings** (its
placement capacity), **Deposits** (its stock, summed) and **Population** (its centres) — resolved
from the selected tile at the draw site. The three tile-grain readings and the three
province-grain ones sit in **one** list because they are answers about **one piece of ground**,
and splitting them across two elements made the player choose which grain to ask at before
knowing what they wanted to know.

**Two things this dissolves:**

- **The province's own Buildings page is gone.** It rolled up what already stands in the
  province, which is the same question the Buildings section's **Built** column answers, at a
  grain the player does not build at. Placement is tile-grain; the roll-up was reference, and
  reference lives in the ledgers.
- **The province's member-tile list is gone.** Its job was to *give the tile back* — to undo a
  selection that had taken the tile away. With the tile selected in the first place there is
  nothing to give back, and a list of tiles a single canvas click already reaches is a list
  worth no space.

**The consequence, stated rather than discovered.** After this, **no gesture selects a province
without also selecting a tile.** The province is still the march target and the render unit, and
both are unaffected: the march destination has always travelled in its own field
(`ui_state::pending_march_dest_province`, deliberately kept distinct so that picking a
destination does not change what the Selection element shows), and the canvas resolves a
destination province from the *hovered* tile, never from the selection.

| Field | Meaning |
|---|---|
| `ui_state::selected_province` | The province the **current selection stands in**, or `0` when the selection is not a tile. A **derived mirror**, not a selection channel. |
| `ui_state::hovered_province` | The province under the cursor, driving the hover outline. |
| `ui_state::province_sync_entity` | The `selected_entity` value the canvas last wrote. |

`selected_province` was a selection channel — mutually exclusive with `selected_entity`,
whichever was set last clearing the other — and is not one any more: **both are set together, or
neither is.** What it is still *for* is the canvas. The province outline traces the cell the
selected tile belongs to, which is the affordance that says *"the Deposits, Buildings and
Population sections you are reading are about this ground"*. It has exactly two writers, both on
the Planetary canvas: the click handler, and the reconciliation arm that catches a selection some
*other* surface made (a ledger row, a just-built building) and re-derives the mirror from it —
which is what lets a tile selected from anywhere at all still get its outline, without adding a
set-the-mirror duty to every selecting surface. **No surface reads it to decide what to draw**:
the element derives the province from `selected_entity` itself, so a stale mirror can never
mis-route a card.

An ocean or otherwise unpartitioned tile has no province. It still selects as a tile, with the
province-grain sections saying so and falling back to the tile's own figures where one exists.
### The battle element

**A live battle selects ahead of everything else** (BL-469, battle card). `draw_battle_selection`
is dispatched **first** — before the province resolution, before `selection_kind_of` — because a
battle is the only selection in the game whose subject is *spending an asset the player cannot
re-buy this tick* while they watch. Everything else answers a standing question about a thing that
will still be there next tick; this answers a decision that expires.

Like a province, a battle is not an entity, so it travels in its own fields:

| Field | Meaning |
|---|---|
| `ui_state::selected_battle_province` | The province the selected fight stands in, or `0` for none. |
| `ui_state::selected_battle_attacker` / `_defender` | The two corps, which *name* the fight — a province can hold several. |
| `ui_state::pending_withdraw_province` / `_against` | The withdraw press awaiting its confirm popup. |

`has_battle_selection()` / `clear_battle_selection()` are the accessors; the canvas reconciles a
stale battle selection on the same arm it reconciles a stale province one, and the battle sits at
**rung 0** of the repeat-click cycle (see below).

**More than one battle can stand in one province.** A third corp arriving opens its **own** battles
against each existing participant rather than joining theirs, so "the battle here" is a choice.
`first_battle_in()` makes it deliberately: **the player's own fight first**, then sorted
`(province, attacker, defender)` order when none of them is the player's — the fight you are in is
the one you need the card for (NR-468).

#### What the card carries, and why each part earns its place

- **The phase word** — `battle_phase_word(read_battle_phase(…))`. Rounds are mechanically uniform;
  the phase is a *reading* of one. It is derived **once, in the world layer**, precisely so this
  card and the Field-channel dispatch stream (CHAT.md) cannot describe the same fight differently.
- **Per-unit strength bars, both sides.** "I am at 60%" does not say whether that is one broken
  formation or five even ones, which is the difference between staying and leaving.
- **The withdrawal price, with its three terms separated** — base, per-round, pursuit — quoted from
  `quote_withdrawal()`, i.e. **the resolver's own arithmetic**, never recomputed here. A second copy
  of that sum in a card is a second place for what the card *says* leaving costs to drift from what
  leaving *charges*. The harness asserts quote-against-charge for exactly this reason.
- **A confirm popup on the withdraw press.** The press is irreversible and priced; it is the one
  action on any Selection element that spends men.

**Rival-vs-rival redaction.** A fight that is not the player's shows the phase, the round and each
side's *aggregate* strength, but each side's composition reads *"Composition unknown — a rival's
forces are not yours to count"*, and the Withdraw press is **disabled with a reason rather than
hidden**, so the rule reads as a rule instead of as a missing button. Spectator god view lifts the
redaction in the UI only, on the spectator pair-test. Whether the *aggregate* should be redacted too
is open (NR-469) — and it is the opposite call to the one the dispatch stream makes, which skips
rival fights entirely (NR-470). One question, two surfaces.

### The contract element

**A mercenary contract is the battle card's sibling** (BL-577, contract card): like a battle, a
`mercenary_contract` is not an entity — it lives in `world::mercenary_contracts`, keyed by its
own stable `id` rather than an entity id — so it cannot travel in `selected_entity` and is
resolved **before** `selection_kind_of`, owning its whole layout rather than the shared
action|facts split. Unlike a battle it is **not time-critical**: nothing on the card expires
between frames the way a fight's strength does, so it resolves after the battle check but still
ahead of the kind resolution.

| Field | Meaning |
|---|---|
| `ui_state::selected_contract_id` | The selected contract's own `id`, or `0` for none. A single scalar suffices — unlike the battle triple, a contract id is already globally unique. |

`has_contract_selection()` / `clear_contract_selection()` are the accessors, mirroring the battle
pair. There is no canvas marker for a contract to select from yet (CONTRACTS.md's "a ledger and
the map" puts a contract's *province* on the map, not the contract record itself) — the setter is
the Contracts ledger's row press (BL-576), which must clear the other selection fields the same
way every other selecting surface already does.

**What the card carries, and why:**

- **Client and contractor**, named — the two counterparties a contract's whole existence is about.
- **The predicate**, via `condition_text` over the template's predicate bound to the contract's own
  province — the same bind `run_mercenary_contract_tick` performs before judging it, so the card's
  wording of the terms cannot drift from the terms actually evaluated. The same reader the Balance
  ledger's laws listing already uses, so a contract's terms and a law's conditions share one
  vocabulary.
- **The committed force** — CONTRACTS.md Q1's "the player chooses the force, the contract never
  does" is the whole shape of the mercenary company's skill; the card is where that choice reads
  back.
- **The deadline**, the econ tick the tick-evaluation pass judges the contract against.
- **The fee split** — deposit already paid versus the remainder still owed on completion, not just
  the total. "The fee is 400cr" hides the one number that matters mid-contract: how much is still
  at stake if the job goes wrong.

### Tile repeat-click selection cycle

**A no-lens gesture.** The cycle walks the stack of things standing on one piece of ground, and a
lens removes that stack (§ A lens collapses selection to ONE TIER, rule 1). Everything in this
section applies to the plain canvas only.

`body_surface_canvas.cpp`'s click handler cycles **Battle → Soldier → Building → Tile → Battle**
on a **repeat** click at the same tile the selection already sits on, skipping any stage with
nothing there (no unit on the tile skips straight to Building; no building skips to Tile).
`ui_state::selection_cycle_tile` / `selection_cycle_stage` track the anchor tile and current stage.

**Four rungs, not five.** The province rung sat between Soldier and Building and is **dissolved**
(Ben, 2026-08-24): the province is a set of sections in the tile element, so the Tile rung reaches
it and a rung of its own selected the same ground twice.

The battle rung leads because a live battle is the time-limited reading of that ground; the other
rungs are standing facts about it. It is live only when `first_battle_in()` finds a fight in the
tile's province, so on peaceful ground the cycle runs Soldier → Building → Tile.

Rungs 1–3 are entities, so a null check *is* their liveness test. Rung 0 is not — a battle has no
entity id — so the handler keeps an explicit `stage_live[]` alongside the entity table for that
one rung.

**On bare ground with no unit, no building and no battle exactly one rung is live**, so a repeat
click re-selects the same tile. That is the honest reading: there is nothing else on that ground
to cycle to.

A click on a **different** tile (or the first click anywhere) goes through the marker-hit precedence
(`resolve_marker_hit`: unit > building > market_centre, nearest-wins within a kind) and only
additionally seeds the cycle anchor so a follow-up repeat click knows where to advance from. Unit
was raised above building by BL-575 (unit marker and march UI) to match this section's own cycle
order, where Soldier already precedes Building — a unit standing on a built tile is reachable on
the first click, not only after cycling past the building.

### The body element's action is the survey front door (then go-to-surface)

An unsurveyed **Body**'s hero action is its **Survey section** (the survey system, BL-067),
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

### The body element's fact is its commercial activity pulse

A selected **Body**'s (not the star's) Facts column carries a **Commercial activity**
section (`draw_activity_section`) keyed on `body_activity_visibility` (the activity fog,
BL-089) — independent of survey phase, so it can be populated on an unsurveyed body and empty
on a surveyed one. See [DISCOVERY.md](DISCOVERY.md) for the model authority (`activity_vis`,
the tier derivation).

- **`unknown`** — "Outside your trade network - no market data." No further content.
- **`known` / `visible`** — a coarse **market pulse** (`busy` / `steady` / `quiet`, derived from
  the body's aggregate market throughput); `visible` additionally notes "Live lane / your
  presence." No per-building production or stockpiles — competitor internals stay private.
- **`known_stale`** — greyed: "Route gone cold - last market read is stale."

---

## Not a stat block

The panel is not an encyclopedic per-kind stat block beside a lens supplement that re-renders
overlay-keyed market price / production rate / population rows — that shape duplicates the
Market / Production / Population ledgers, gives facts and actions equal weight, and reads as
"an amalgamation of all the menu ledgers" (Ben's playtest finding, 2026-07-04). Reference
detail — orbit, parent, composition, deposits, prices — lives in the ledgers, one `go to`
away; the header identity line (name · type) is the only trace of it in the panel itself.

The shared per-entity content builders in `entity_summary.hpp` (`draw_body_summary`,
`draw_tile_summary`, `draw_building_summary`, …) are the Tile Ledger's content; the Selection
element does not call them.

---

## Layout & chrome

**The Selection band.** The element lives in a **fixed band at the bottom of the screen**. Its
left neighbour is the **comms dock** (`comms_dock_rect` in `src/ui/foldout_column.{hpp,cpp}`);
its right neighbour is the right chrome column (time panel + minimap). Dock and band share one
top edge and one height, so the screen's bottom row reads as a single solid strip. Ben's
ruling (2026-07-28): *"menus that are to do with doing/building need space at the bottom of
the screen, rather than floating with the cursor."* The band is:

- **Fixed, not click-anchored.** Always the same rect — left edge at the **comms dock's right
  edge** (the dock takes three quarters of the fold-out column's width; the band takes the
  quarter it gives back), right edge at the right chrome column, bottom-anchored at
  `selection_band_height` tall (`app.cpp`'s band block; the frame is
  `src/ui/selection_card.{hpp,cpp}`) — regardless of where the selecting click landed. The
  player's eye never has to re-find it.
- **Not mutually exclusive with the ledgers.** Because it does not share the shell column,
  selecting an entity **leaves whatever fold-out ledger is open untouched** — both are visible
  at once.
- **Height is display-derived, not a constant.** `selection_band_height(disp_x, disp_y)` =
  minimap height + `chrome_margin` (`src/ui/foldout_column.{hpp,cpp}`), so the band's top edge
  lands exactly on the minimap's and the bottom row reads as one band. A flat height overhangs
  the minimap — "discordant", Ben's word — with the main canvas practically blocked.
- **One dispatcher.** `draw_selection_content` (selection_panel.cpp) is the shared per-kind
  dispatcher; the container frames it.
- **Header row:** a small coloured **kind icon** (`draw_selection_icon` — circle for body,
  square for building, outlined square for tile, pentagon otherwise), then the title line
  (name · type), then a right-aligned **`[>]`** ('go to') button. There is no close button
  (see § Always open).
- **Always open.** The band never hides: there is no `[x]` button, no Esc hide rung, and no
  hidden state. With the shell filling the perimeter, a hidden band reads as a hole in the
  frame (Ben, NR-017).

  Being selection-driven with no rail slot, the element is **not** governed by the universal
  toggle rule: that rule toggles the rail's ledgers, not this element.
- **Resting state — the player's own corporation.** With nothing selected (fresh session, or
  after clicking empty space) the band rests on the player corp, through the same
  `selection_kind::corporation` builder any selection uses. Deselecting means "stop looking at
  that, back to looking at yourself".

  `selected_entity` stays null while resting, so deselect remains representable; the band
  substitutes the player corp at draw time and never renders `selection_kind::none`. The
  player corp exists before the first frame, so there is no bootstrap gap.
- **Esc ends at the system menu.** Esc's ladder is: exit-confirm → close system menu → pop a
  drill level → corp roll-up → fold overlay → open the system menu. The "pause screen" IS
  `show_system_menu` (settled 2026-08-02).

**Click-through.** The band draws as an ordinary ImGui window over the canvas; ImGui's
`WantCaptureMouse` flag (how every other chrome window — nav rail, header, minimap — prevents
the canvas from receiving clicks underneath it) covers the band the same way, so no
special-case hit-testing is needed.

**The lens bar is not here.** The overlay-lens control strip (`ui::draw_overlay_controls`,
`src/ui/overlay.cpp`) lives on the **minimap**, leaving the Selection element the full height it
needs; the Resource/Market/Scarcity good selector lives in the lens legend. Full detail:
[LENSES.md](LENSES.md), [MINIMAP.md](MINIMAP.md).

---

## Lens-driven hover & selection resolution (settled 2026-06-15, [F4])

Owned by BL-372 (lens-keyed selection). A single pointer position overlaps a **stack** of entities — a building sits *on* a tile sits *on*
a body. Both Focus (hover) and Selection (click) resolve that stack to exactly one entity. The
rule has two parts: a fixed stack order, and a lens-evaluated validity filter over it.

**The kind stack (most-specific → least).** At a pointer position the candidate entities are
ordered:

```
building → market → unit  →  tile  →  body
```

(the marker kinds first, then the tile they occupy, then the body that hosts it). "Lowest" means
**most-specific** — earliest in that order.

**The stack is the NO-LENS rule.** With no lens active every drawn kind is valid, so resolution
returns the literal lowest entity (a building over a tile resolves to the building; bare terrain
resolves to the tile, whose element carries its province), and a repeat click walks the four rungs
below (§ Tile repeat-click selection cycle). Under a lens the stack does not apply at all — see
the next section, which supersedes it.

### A lens collapses selection to ONE TIER (Ben, 2026-08-28)

Owned by BL-664 (one tier under a lens). Three rules, and they hold for every lens without
exception:

1. **The lens's structure is the only thing under the pointer.** Resolution asks the active lens
   what structure this ground belongs to and answers with that, or with nothing. It does not walk
   a stack, because under a lens there is no stack — there is the lens's subject and there is
   everything the lens is not about.
2. **Markers do not outrank lenses.** A building glyph is not a shortcut past the lens's question.
   Under the Market lens a click on a plant gives the catchment; under Resource it gives the
   deposit; under Corporation it gives the owner. This inverts the no-lens precedence deliberately:
   *outside* a lens a marker is the specific thing the player aimed at, but *inside* one the lens
   is the question they are asking and the marker is only ground that happens to be built on.
3. **A lens with no answer here is inert.** Where the lens resolves to nothing — a tile carrying
   none of the selected resource, ground in no catchment, a lens with no structure grain at all —
   **no hover card appears and a click does nothing to the canvas**. It does not fall through to
   the tile, and it does not fall through to a marker. A lens that has nothing to say about this
   ground says nothing.

**A click on inert ground clears the band to resting.** The Selection band returns to the player's
own corporation (§ Always open), exactly as a click on empty space does. Leaving the previous
selection standing is the worse of the two readings: it makes the band assert something the player
did not just click and has no way to connect to the pointer (the failure NR-697 names).

**The repeat-click cycle is a no-lens gesture.** Battle → Soldier → Building → Tile requires a
stack to walk, and rule 1 removes it. Under a lens a repeat click on the same ground re-resolves to
the same structure, which is the honest reading: there is one tier, and it does not move.

**Consequence — a lens with no structure grain is a pure read.** Population, Industry and
Throughput draw a per-tile *value field* rather than a region: their subject is a number spread
across the map, not a thing on it. Under rule 3 they are entirely non-interactive — no hover card,
no selection, anywhere on the body (Ben, 2026-08-28: "for lenses which return none, just don't
surface a hover, and clicks will do nothing"). This is a deliberate category, not a gap: a value
field is something you *read*, and giving it a fake selectable grain would promise a pivot it has
no destination for.

### The lens names the ledger the selection drives

Resolving the entity and choosing its 'go to' ledger are the *same* decision — the lens that
validates the structure also routes it. [LENSES.md](LENSES.md) § Per-lens selection validity &
routing owns the per-lens table; this is the shape it takes.

| Active lens | Resolves to | Routes to |
|---|---|---|
| **none** (terrain) | the lowest drawn entity, then the four-rung cycle | Tile Ledger |
| **Corporation** | the corporation's **tile group** — every tile it holds on this body | that corporation's ledger |
| **Company** | the background firm's **tile group**, same shape, different subject | that company's ledger |
| **Resource** | the **deposit** — every tile carrying the selected resource | Market Ledger, aimed at that resource |
| **Market** / **Scarcity** | the **market catchment** under the pointer | Market Ledger |
| **Continent** | the **plate** | History ledger, at its tectonic record |
| **Population** / **Industry** / **Throughput** | nothing — inert | — |

**A tile group is a structure like any other.** Hovering one tile of a corporation's holdings
lights **all** of them at once (Ben, 2026-08-28: "hovering one tile displays an outline around all
company buildings for that corporation/company"), and clicking any of them opens that owner's
ledger. It is the same claim the market catchment's highlight makes — *all of this is one thing* —
so it takes the same wash the catchment does rather than a walked boundary (Ben's 2026-08-24 ruling
on that question, recorded in `body_surface_canvas.cpp`: a wash is per-tile and costs one test,
and an area statement is the truer read anyway).

**Corporation and Company route to DIFFERENT ledgers**, because since the 2026-08-28 split
([GLOSSARY.md](GLOSSARY.md)) they are different words: a corporation is a named operating firm the
player competes with, a company is a background firm. One is a rival dossier, the other is a
market participant. BL-666 (owner ledger destinations) owns what each destination is.

Ground held by nobody is inert under both, by rule 3.

## Open questions

- **Multi-select.** Out of scope; the model is single-selection. A future
  drag-box or shift-click would extend `selected_entity` to a set.
- **Selection persistence across navigation.** A selection survives canvas
  navigation (it is independent of Active). Whether a stale selection (entity
  destroyed) auto-clears is an implementation detail — treat a missing id as
  "no selection".
