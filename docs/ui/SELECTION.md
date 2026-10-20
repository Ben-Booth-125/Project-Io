# Project Io — Selection Info Element

> **Settles:** which three pointer states exist and what each one drives · which
> press changes which of them · what this element offers for each kind of thing
> selected, and where its deeper reference lives · what repeat-clicking the same
> ground does · how a stack of things under one pointer resolves to a single one ·
> how an active lens narrows what a press may resolve to and which ledger it
> drives · what this surface deliberately refuses to be.
> **Not here:** the hover card itself (TOOLTIP) · what a lens shows (LENSES) ·
> where the band sits in the shell (LAYOUT) · the ledgers it points at (MENU).
> **Confused with:** TOOLTIP.md, LENSES.md, LAYOUT.md.

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

### The centre presents; it does not operate

**The Selection element's centre presents DATA and never holds levers** (Ben, 2026-08-29). A
dial that changes the world belongs to a ledger; the centre says what the selected thing *is*
and what it is *doing*. The element is a **reading** surface with one designated place to act.

**This REVERSES NR-245** — *"There is no Manage link — every control it would route to lives on
this card"* — for the centre specifically, and the reversal is recorded rather than quietly
applied, newest-dated winning. What NR-245 was avoiding remains avoided: there is still no
Manage link and no round trip through a second surface, because the ledger that now holds the
levers is reachable from the card's own action grid.

The boundary runs **around the centre, not around the element**. The right-hand **action grid**
is the designated action area and keeps its buttons; the centre's paged accordion keeps only
pages that report. Where a page's whole content is a control — a recipe switcher, a workforce
slider — that page moves to the ledger for its kind.


The rule composes with the **menus-are-broad-ledgers** test in `MENU.md` rather than fighting it.
That test asks whether a *surface* earns a rail slot; this one asks where a *control* lives once
the surface exists. A ledger may be entered by a targeted press without becoming a targeted
surface: the Buildings ledger is a broad roster of the whole estate, and selecting one building
is how you aim it.

Each kind routes its 'go to' to the right place:

| Selection kind | Action (hero) | Facts (muted) | 'Go to' target |
|---|---|---|---|
| **Tile** | Dedicated four-column band, no header row — § The tile element's layout. | | **No-op.** A tile is selected from the surface it lives on, so 'go to' has nothing to descend to; pan-to-tile is out of scope. |
| **Body** (planet/moon/asteroid/station/star) | Unsurveyed → **Dispatch Survey**. Surveyed → **Go to surface** (descends via `focus_on_entity`). The star carries neither. | **Commercial activity** pulse (activity fog) — see below. | Canvas: `focus_on_surface` — descend to the body's **Planetary tile surface**, the most informative rung. (Not `focus_on_body` / the orbital framing: landing on a sparse circumplanetary view reads as "nothing happened".) |
| **Building** (any) | The host tile's card with this stack expanded in place, no header row — § The building element's layout. Player-owned and rival buildings alike. | | None: the tile element draws no go-to. |
| **Unit** | Dedicated three-column band — § The unit element's layout. | | Canvas: entity's position. |
| **Market** | **Go to** — locate on the canvas. | (none) | Canvas: `focus_on_surface`. |
| **Nation / Corporation** | None — **"Open its ledger via [>]."** | (none) | A ledger (no canvas of its own). |

So 'go to' is itself polymorphic: spatial entities navigate a canvas;
non-spatial entities (nation, corporation) open the relevant ledger.

### The tile element's layout

A selected **Tile** takes a dedicated layout (`draw_tile_selection`,
`src/ui/selection_panel.cpp`) running **left to right** across the band in four columns
(Ben, 2026-10-13, the sprint 54 tile pass; it supersedes the 2026-07-28 three-column layout
and the 2026-08-24 six-section nav):

> **Preview → Province → Tile → Actions**

**The tile element draws no header row.** The shared `[icon] Tile [x, y] · type  [>]` row
spent a line of the band on a name the canvas already shows and a 'go to' that is a no-op for
a tile (§ Action + Facts). The whole band height goes to the four columns. Kinds that are not
a tile keep the shared header (§ Layout & chrome).

1. **Preview — the zoomed hex neighbourhood.** A bordered render of the selected tile and its
   immediate ring (`draw_tile_neighbourhood`, radius 2), the selected tile picked out, drawn
   from the **baked neighbourhood page** — the same ground, structures and grade the canvas
   draws. **Until that page is ready for this tile, the box shows a loading bar**, not flat
   per-hex colour fills. The fills read as a second, strategic-looking map of the same ground,
   which is a claim about what the tile looks like that the bake then contradicts a moment
   later. A bar says the true thing: the picture is coming.
2. **Province — one titled card**, province grain throughout, the selected tile serving only
   to name which province is meant. No pager: everything the province answers fits one card,
   and **the card itself never scrolls** — only the resource table does, inside its own rows
   (Ben, 2026-10-13, round 2). Top to bottom:
   - **The header row** — the title, then the **capacity bar, narrowed** to share the row, then
     the **population count right-aligned** at the row's end.
   - **Capacity** — the province's building count against its **province building ceiling**
     (`../economy/PRODUCTION.md` § The province building ceiling), drawn as a **bar**. Hovering
     the bar opens a tooltip naming **where the capacity is granted** — the ceiling's four
     sustain inputs (area, infrastructure, habitability, population), each with its share, the
     same terms `province_building_ceiling` computes from. A ceiling of `-1` is UNKNOWN and the
     bar says so rather than drawing room.
   - **The resource table** — one row per workable resource in the province, three columns:
     **Built** (extraction sites in the province targeting it), **Max** (the province's placement
     capacity for it — summed `stack_capacity` over member tiles that would accept an extraction
     site, through the same `can_place_in_world` the placement seam uses, so the table cannot
     disagree with the build button) and **Resource** (the province's deposit of it, **summed**
     across member tiles — a deposit is a stock, so several tiles each holding a little iron *is*
     one province holding that much). Richest deposit first. Agricultural Produce
     is labelled **Farm** in this table (Ben, 2026-10-14) — the name of what is built on it, short
     enough for the card. This one table replaces the former
     separate Buildings and Deposits sections: they were two readings of the same rows.
     It takes the rest of the card's height and **scrolls inside itself**.
   - **Population** — the province total in thousands, **top right of the header row**.
     Hovering it lists each population centre by name with its scale word (Outpost /
     Settlement / Town / City / Metropolis, POPULATION.md § Scale bonus model), headcount and
     habitability, largest first; a centre standing on the *selected tile itself* is marked.
     The list moved into the hover so the table can have the card's height. Province grain
     because the data model is.

   On unpartitioned ground (ocean) the card says there is no province and falls back to the
   tile's own deposits.
3. **Tile — one titled card: the buildings on this tile.** Tile grain, because a building
   stands on a tile. **Buildings only** — no market or price rows; what the tile's goods fetch
   is the Market ledger's question.
   - **A grid of building buttons, three across**, one button per **stack** (grouped by type,
     what it makes, and owner — `placement_rules::stack_members`; a rival's stacks group by
     type and owner only, since splitting them by recipe would itself reveal the recipes). Each
     button carries the **type glyph** (`icons::building`, with the stack's count when above
     one), the **building name**, and its **net profit per quarter** (`estimate_building_profit`,
     the figure the Profitability page charts), coloured by sign. A **rival's** button shows
     glyph and name, with profit reading *private* (DISCOVERY.md, the competitor-visibility
     rule). A stack still under construction shows its ETA in place of profit.
   - Running state (Understaffed, Idle and its reason) is **not** on the button. It is read on
     the building's own Status page, one press away, and in the building hover card.
   - **An unbuilt tile states *Nothing built here* positively.**
   - **Pressing a button expands that building in place** — § The building element's layout.
     The province card and the preview stay where they are; only the tile card and the action
     grid change.
4. **Actions — a 2×3 button grid** (2 columns × 3 rows). For the tile: **Construct Buildings**
   (opens the **tile construction ledger** — `draw_construction_ledger`, see below; drawn with
   a "primed" accent ring when something is placeable and nothing is under way), **History**
   and **Supply** (both drawn disabled — History has no surface; supply routing is the Supply
   lens's subject, LENSES.md), and reserved slots so the grid's shape never changes. There is
   no **Manage Buildings** button: the building grid *is* the tile's buildings. **While a
   building is expanded, the grid retargets to that building's actions** (§ The building
   element's layout).

**What retired, and why** (Ben, 2026-10-13):

- **The Resources section** (a per-deposit chart of this tile's yield against the top decile)
  and **the Terrain section** (habitability and hazard against the body average). Neither
  changed what the player does next, and both are reachable elsewhere — the hover card reads
  terrain and habitability, and the lenses read the field. The Resources charts were the
  only door to the resource history drill (BL-196), which retired with them (Ben, 2026-10-14).
- **"What it fetches here"** — the market and per-good price rows under Production. **This
  REVERSES the 2026-10-08 ruling** (the sprint 51 visibility pass) that added them as the
  initial-market-conditions read; newest-dated wins. The reading itself survives in the Market
  ledger.
- **The six-section nav** (Production → Buildings → Deposits → Resources → Population →
  Terrain). With two sections retired and Buildings, Deposits and Population merged into one
  province card, each side holds one page, and a pager over one page is chrome with nothing to
  page.

### A water tile is selectable, and its selection answers ownership (Ben, 2026-09-06)

**Water tiles are selectable.** Clicking water resolved to nothing at all until 2026-09-06 — the
Selection panel did not update, which was defensible while water was a wall and indefensible once
it became territory. A water tile selects exactly as a land tile does, through the same
`focus_on_entity` path and into the same three-column band.

**The reason it must be selectable is that it is the ONLY surface that can answer the water
model's central claim.** Coastal water and lakes carry an owner, derived from the shore that
claims them; open ocean structurally does not (`docs/generation/PROVINCES.md` § Who owns water).
That is the load-bearing shape of the model, and it was invisible on every surface the game
had — the hover card reported terrain and habitability and said nothing about title, and no lens
colours ground by owning nation. A claim nobody can look at is one that can only be trusted, which
is not the standard this project holds a generated world to.

**What the band presents for water**, against the land layout above:

| Column | Water tile |
|---|---|
| Left — hex neighbourhood | Unchanged. The ring reads a shoreline usefully; it is where the coast/ocean boundary is legible at all |
| Centre — facts | **Owner** and **domain** are the headline pair — the nation holding it, or *unowned* stated **positively** for open ocean rather than left blank. Then the province, and habitability |
| Right — actions | **Construct** is live only for a **port**, on **owned** coastal water; everything else is disabled, as on ground that refuses the type |

**Unowned must READ as a fact, not as missing data.** An empty owner row and an owner row saying
"unowned" are the same pixels' worth of effort and opposite in meaning: the first looks like the
panel failed, the second is the design. Open ocean is unowned *structurally* — it is the assertion,
not the absence of one.

**What is deliberately not here.** The centre column does not gain a deposit, workforce or
resource page for water — those read the ground, and there is none. And selecting water does not
substitute for a territory lens: the panel answers *who holds this tile*, one tile at a time,
where a lens would answer *who holds this region* at a glance. The lens stays unbuilt and
unpromised.

### Multi-building tiles

A tile can carry a heterogeneous set — several extraction stacks against different deposits,
several processors, plus infrastructure (only extraction carries a capacity rule). Three
consequences (settled 2026-08-11; the drill and click model re-ruled Ben, 2026-10-13):

- **Grouped by stack, not a flat list.** `placement_rules::stack_members` groups a tile's
  buildings by `(type, target)`; the tile card's building grid and the management surface
  (`draw_selected_section`, `src/ui/construction_panel.cpp`) both read that grouping, one
  button or row per stack.
- **A canvas press selects the tile, and the tile opens on its building grid** (Ben,
  2026-10-13, round 2: *"default tile selection to the list of buildings rather than any
  individual building"*). This REVERSES the 2026-07-22 rule that a press on a single-building
  hex lands on the installation: the grid is one press from any building on it, and a hex
  that opened on a building hid that it was a tile with a province at all.
- **Selecting a building IS selecting its tile, with that building expanded.** There is no
  separate building element. The routes that select a building — a press on its button in the
  grid, the repeat-click cycle's Building rung, a ledger row — set `selected_entity` to the
  building, and the band draws the **host tile's** card with that building's stack expanded in
  the tile column. An unlabelled **‹** back control on the expanded card selects the tile again
  and returns to the grid.
- **On the canvas: a cluster, no marker.** A built tile draws no glyph and no count badge; it
  stands a cluster of up to three structures, one per stack, baked into the ground
  (RENDERING.md § Installations). The count is read on the stack's button in the tile card.

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
class of refusal never becomes a row at all: `construction_result::era_locked` and (since
BL-593) `tech_locked` are filtered out of the candidate list before it renders, on one repeated
argument stated in the filter's own code comment — *"the door not showing what the gate would
refuse."* The two lock kinds resolve differently (never available in this campaign at all /
held by the corp's nation through research, PRODUCTION.md § Chain depth) but share the same
door-side treatment: not a reason string, an absent row. The tech clause removes a recipe only
where a node unlocks it and the corp's nation does not hold that node. Filtered was the ruled choice over

shown-and-locked (Ben, 2026-08-24) — extending the existing era precedent rather than adding a
new lock-reason string and UI affordance.

**Chain depth locks nothing** (Ben, 2026-08-29). The door once carried a third clause erasing a
recipe deeper than the corp had reached, and the Method Switch row once greyed itself with *"Your
industry cannot make what this needs yet."* Neither exists: tech is the only method lock, so the
Switch control is disabled by its cooldown alone.

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

A building is **not a layout of its own** (Ben, 2026-10-13): it is the tile card with one
stack **expanded in place** (§ Multi-building tiles). Player-owned and rival buildings alike.
The preview and the province card do not move; two columns change.

1. **The tile column becomes the building's card** — one card, no pager, laid out to the
   card's height with no scrollbar (Ben, 2026-10-13, round 2). Top to bottom:
   - **The header row** — an unlabelled **‹** back control, the building name, and the
     **number of buildings** in the stack (*Steel Mill ×3*).
   - **The production method** — the stack's active recipe by name (an extraction site names
     the good it extracts), then the status line: the **running state with its reason**
     (*Running*; *Idle — input short: Steel, over the fair-price ceiling*), or the construction
     rate and ETA while it is still building — never a bare "Operating.".
   - **The profit row** — **left**, a short **net-over-time line chart** ("Net, 6 mo.", the
     honest placeholder series anchored to the live estimate, since no per-building profit
     history is tracked — NR-249); **right**, the **net profit per quarter** as one figure,
     coloured by sign, with **expenses per quarter** below it as one figure. Hovering expenses
     splits it into `input_cost` / `maintenance` / `wages` (NR-248). The column chart of
     revenue against stacked expenses and the Inputs bar chart are retired: one number says
     what the column pair said, in a fraction of the height.
   - **The workforce row** — the building's workforce target as a **read-only bar** with the
     percentage of full employment to its right (0–100 %, the internal target ÷ 2). The game sets it, one 25 % step a quarter toward the
     most profitable level (`../economy/PRODUCTION.md` § Workforce target and the auto-solver);
     the slider and the Auto control are retired (Ben, 2026-10-13).

   **Every card shows its method and status line, always** — before the first quarter, and for
   a building with no recipe. A building with no recipe names what it does instead (a Port: the
   trade points it makes; a hub: the haul it discounts). The profit and workforce rows are drawn
   for the player's own completed building; **a fresh campaign opens with them filled**, from the
   settle's quarters (`STARTUP.md` § Handoff, Ben 2026-10-13: *"the first few ticks in
   generation … pass their data to the live game"*). A building with no output still has costs:
   its profit row reads its net (negative) and expenses. A building still under construction
   shows the header, method and status. A **rival's** card
   is the header, owner name and an explicit `private` row for production (DISCOVERY.md, the
   competitor-visibility rule) — it short-circuits rather than testing each guard against data
   it must not show.

   The card holds no lever: the workforce slider it briefly carried (2026-10-13, round 2) is
   retired with player workforce control altogether, so § The centre presents holds without
   exception. The method switch stays in the Construction ledger's Buildings view; the card names
   the method and does not switch it.

2. **The action grid retargets to the building** — the same 2×3 cell layout, now holding
   three building-level actions. This is the element's designated place to ACT; the cards are
   where it presents. The back control (or any selection elsewhere) returns the grid to the
   tile's actions.
   - **Mothball** (`glyph_mothball`, a box with a line through it) — flips `decommissioned`
     (reversible — no output, no wages while closed) and invalidates the logistics anchor
     cache. A **toggle button** per the standing Toggle rule — its own active state
     (`decommissioned`) is what re-clicking undoes; the tooltip reads "Mothball..." /
     "Un-mothball..." by state.
   - **Dismantle** (`glyph_dismantle`, a simple X) — permanent, asks once via a confirm popup,
     then defers through `ui_state::construction.pending_demolish` for `app::render` to execute
     (`demolish_building`) — erasing mid-draw would dangle the `building_component&` the grid
     is reading.
   - **Reserved**, in Mothball's column one row down — the cell the Auto control held until
     player workforce control retired (Ben, 2026-10-13; `../economy/PRODUCTION.md` § Workforce
     target and the auto-solver). The game sets the target; the card reads it.
   - **Open in ledger** (`glyph_roster`, a panel outline with one row picked out), beside that reserved cell —
     opens the Construction ledger's **Buildings** view **aimed** at this building: its type
     group expanded and the building selected, so the levers that left the centre are on screen
     in one press. It exists because the lever move would otherwise leave a building selected on
     the MAP with no route to its own controls at all. It is a **door, not a toggle** — its
     active state is not visible on the card, so a second press must not close what it opened —
     and it takes its own glyph rather than sharing a silhouette with the lit controls inches
     away (BL-174).
   Mothball and Dismantle are both disabled with "Competitor building - intel only" for a
   rival. **Open in ledger is ABSENT on a rival's card**, not merely disabled: the Buildings view
   is the player's estate, so a rival building has no row for the button to aim at. Its cell
   falls back to `glyph_reserved`, so the grid keeps its shape and nothing reflows. **Three slots
   are reserved** (`glyph_reserved`).

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
province: the tile element's **Province card** carries the province readings — capacity, the
Built / Max / Resource table and population — resolved from the selected tile at the draw site.
The province card and the tile card sit side by side in **one** element because they are
answers about **one piece of ground** (the side-by-side split is Ben, 2026-10-13),
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
(`resolve_marker_hit`: unit > building, nearest-wins within a kind) and only
additionally seeds the cycle anchor so a follow-up repeat click knows where to advance from. A
building hit lands on its **tile**, never the building (§ Multi-building tiles, round 2), so the
first click seeds the Tile rung and the Building rung is reached by the repeat click. Unit
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
- **Header row** (every kind but the tile, which draws none — § The tile element's layout):
  a small coloured **kind icon** (`draw_selection_icon` — circle for body,
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
unit → building  →  tile  →  body
```

(the marker kinds first, then the tile they occupy, then the body that hosts it). **A market is
not in the stack** (Ben, 2026-10-10: "We should remove the HQ glyph and market center glyphs"): its centre glyph and hit zone retired from the
Planetary canvas, so a market is selected from the map through the **Market lens** — a press on its
catchment, by the rule in the next section — or from the Market ledger. With no lens, a press on a
market's centre tile resolves as any other tile or building press. "Lowest" means
**most-specific** — earliest in that order.

**The stack is the NO-LENS rule.** With no lens active every drawn kind is valid, so resolution
returns the literal lowest entity (a building over a tile resolves to the building; bare terrain
resolves to the tile, whose element carries its province), and a repeat click walks the four rungs
below (§ Tile repeat-click selection cycle). Under a lens the stack does not apply at all — see
the next section, which supersedes it.

**Two resolvers, and which wins.**

| Resolver | Answers | Used by |
|---|---|---|
| **Boundary** (`resolve_structure_hit`) | "am I on this structure's edge?" | The national border band — plain-canvas chrome, not a lens, and suppressed with its hit corridor while any lens is up |
| **Area** (`lens_structure_of_tile`) | "which structure is this ground part of?" | The active lens |

**With no lens active, a marker outranks both, and a boundary outranks an area.** The order is not
arbitrary: a marker and a border are things the player *aimed at*, while a catchment is ground they
happen to be over.

### A lens collapses selection to ONE TIER (Ben, 2026-08-28)

Owned by BL-664 (one tier under a lens). The pivot it sharpens is Ben's earlier ruling,
2026-08-24: *"When a lens reveals any large structure, the selection should pivot to the entire
structure, and no longer provinces. So for the market lens, the entire market gets highlighted on
mouse over, and clicking opens up our market ledger for that market."* Three rules, and they hold
for every lens without exception:

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

**One hover mark, not three (Ben, 2026-08-28: "we want to stop dual hover").** A pointer position
could light three things at once — the tile's own ring, its province edge, and the lens structure's
wash — so hovering a market lit the catchment *and* the tile inside it. Under a lens the structure
is the only thing that lights, because it is the only thing the pointer resolves to; the tile ring
and the province edge are **no-lens marks**. Selection is a different question and keeps its
outlines under every lens.

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

**Throughput takes a hover read, and still no selection (Ben, 2026-10-20, choosing option A
for BL-1353, the throughput lens rework).** Its field is the player nation's own per-tile
coverage, a figure that differs tile to tile and has a cause worth naming — so the pointer
resolves to **the ground**, `structure_kind::ground`, and the hover card reads it. The card is
the *why* behind the drawn number: the tile's coverage, the network whose reach is best there, its
traversal cost to that network's nearest source, and whether that is inside full reach or on the
fade. It names a network, not a building: coverage is read off one distance field per network
seeded by all of them, so no single source is "the" one.
It is also the only read at a rung too far out for the per-tile numbers to draw. **A click
stays inert** and clears the band to resting, exactly as above: a hover read promises no pivot,
so the "no destination" objection does not arise. Ground the lens leaves bare — another
nation's — resolves to nothing and shows no card. Population and Industry are unchanged: still
pure reads, no hover, no selection (Ben, same ruling: "Throughput only").

### The lens names the ledger the selection drives

Resolving the entity and choosing its 'go to' ledger are the *same* decision — the lens that
validates the structure also routes it.

| Active lens | Resolves to | Routes to |
|---|---|---|
| **none** (terrain) | the lowest drawn entity, then the four-rung cycle | Tile Ledger |
| **Corporation** | the corporation's **tile group** — every tile it holds on this body | that corporation's ledger |
| **Company** | the background firm's **tile group**, same shape, different subject | that company's ledger |
| **Resource** | the **deposit** — every tile carrying the selected resource | Market Ledger, aimed at that resource |
| **Market** / **Scarcity** | the **market catchment** under the pointer | Market Ledger |
| **Continent** | the **plate** | History ledger, at its tectonic record |
| **Nations** | the **nation** whose territory is under the pointer | wherever a press on its border band goes — the same `structure_kind::nation` selection |
| **Population** / **Industry** | nothing — inert | — |
| **Throughput** | **the ground**, for the hover card only — a click is inert | — |
| **Supply** / **Supply-routes** / **Reach** | body-to-body; no Planetary structure | — |

**A tile group is a structure like any other.** Hovering one tile of a corporation's holdings
lights **all** of them at once (Ben, 2026-08-28: "hovering one tile displays an outline around all
company buildings for that corporation/company"), and clicking any of them opens that owner's
ledger. It is the same claim the market catchment's highlight makes — *all of this is one thing* —
so it takes the same wash the catchment does rather than a walked boundary (Ben's 2026-08-24 ruling
on that question, recorded in `body_surface_canvas.cpp`: a wash is per-tile and costs one test,
and an area statement is the truer read anyway). It lands one frame behind the pointer,
exactly as the hovered-province outline already does and for the same reason: the tile loop
must know the answer before it has drawn the tile that produces it.

**Check:** `scripts/verify/lens_structure_pivot.lua`.

### A deposit and a plate are cards, not just channels

The two non-entity structures get Selection-band content of their own, dispatched ahead of the
kind resolution exactly as the battle and the contract are — none of the four is an entity, so
`selection_kind_of` cannot see any of them. Owned by BL-671 (the two region cards).

- **Deposit** — the good's showing across the **active body**: how many tiles carry it, the richest,
  the total. Not one tile's richness: the lens draws every tile carrying the good, and the selection
  grain follows the drawing. It has no actions — you cannot dig a region, you site a building on one
  tile of it — so the card hands the player to the Market ledger, already open on Prices.
- **Plate** — its kind (oceanic or continental), its tile count, its drift, and its boundary tiles
  counted **separately as collisions and rifts**. Never summed: a tile at a junction between a
  closing pair and an opening pair sits on both, and the two say different things — a collision is
  where the mountains came from, a rift is where the crust thinned.

**Why the band showed the wrong subject before this.** The band substitutes the player's own
corporation whenever `selected_entity` is null (§ Always open), and both region presses deliberately
null it. So a plate press did not leave the band blank — it confidently showed a *different*
subject, which is the worse failure and the harder one to notice.

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
