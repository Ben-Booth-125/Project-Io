# Construction — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `rail slot 3 "Construction"` · Source: `src/ui/construction_panel.cpp` · Mock table(s): `buildings.csv`
> Host: shell fold-out column, ~380px @1720 (derived — `shell_column_width(disp.x)`, 380–460 by resolution).

## 1. Top question — the one thing this answers at first glance
**"What do I own, what is each kind of it earning me, and what is on the way?"** — the estate read
first, the in-flight read second. Both are broad, which is what earns the rail slot under the
menus-are-broad-ledgers rule in `MENU.md`.

**The targeted halves live here too, but they are reached rather than listed.** Building is a
*targeted* act, so the build bar prices the **selected tile** — and it is a section of this
ledger's Construction view, not a surface of its own. Per-building configuration is targeted the
same way: the **levers** (production method, workforce) draw for the **selected building**, below
the Buildings view's roster. Neither breaks the rule, because a ledger may be *entered* by a
targeted press without becoming a targeted surface (`SELECTION.md` § The centre presents; it does
not operate): the roster is the whole estate, and selecting one building is how you aim it.

**One construction element, two doors.** The nav rail's slot 3 and the tile Selection element's
**Construct** button open the *same* Construction view. There is no second build bar and no
second code path — before this they were two different surfaces, so which one a player met
depended on which control they happened to press.

## 2. Sub-levels — views & default

Two views behind a `nav_button` strip (`construction.panel_view`), inside `foldout_begin("Construction")`.

| View | Answers (one question) | Content |
|---|---|---|
| **Buildings** *(default)* | "What do I own, and how much is each kind of it making me?" | One row per building **type** the player owns at least one of — the type's display name, its **count**, and the **summed per-quarter net** of its members, sign-coloured. About **nine rows** visible with a vertical scroll. **Expanding a row** lists that type's own buildings, one line each carrying the tile and that building's own net, visibly summing to the header's total; **pressing one selects it**, and its **levers** draw below the roster. One group open at a time (an accordion). |
| **Construction** | "What's in flight, what is it costing, and what could I put on this tile?" | A **collapsible** queue header carrying **N cr / qtr** (`estimated_quarterly_construction_cost` — `compute_building_opex` maintenance + wages summed over every player building with `ticks_remaining > 0`, the same formula the budget loop uses), then, when expanded, one row per in-progress build with a progress cell colour-coded dim (<25 %) / mid (25–75 %) / high (>75 %). Below it, the **tile build bar** (`draw_construction_ledger_body`, `selection_panel.cpp`). |

**Why Buildings is the default.** The queue is empty most of the time and the player always owns
buildings, so opening on the queue makes the ledger's front door an empty room.

**Why the queue collapses.** Same fact, applied to the queue's own space: closed it costs one line
and the build bar below it is what a player meets; open it says when things land. A
`CollapsingHeader` is a toggle by construction, so the standing Toggle rule needs no second control.

### The two empty states differ, deliberately

- **Construction, no tile selected** → the words **"Select a tile"**, and nothing else. This is
  the answer, not a placeholder. A richer empty state would have to list the tiles the player
  *could* build on, and **any ordering of that list is a recommendation** — the surface Ben
  declined ("it enters the territory of telling the player what to do"; `CONCEPT.md`). There is no
  way to fill this space that is not the thing that was just declined.
- **Buildings, a type row expanded** → the type's **own buildings**, each pressable. This one
  *can* be richer for exactly the reason the other cannot: it ranks nothing and recommends
  nothing. It lists what the player already owns, in a group whose count has already asserted
  they exist. That is navigation. It also removes a dead end — "Select a tile" is an instruction
  with no affordance attached, and the buildings behind a count are the set the ledger has in hand.

### The levers

**Method** (`draw_production_method_section`) and **Workforce** (`draw_building_workforce_page`)
draw below the roster for `ui_state::selected_entity`, when that is a player-owned building.
They live here because the Selection element's centre presents data and never holds levers (Ben,
2026-08-29; `SELECTION.md` § The centre presents, which owns that rule). **The same function
bodies** the card's accordion used to host are called from here — never a second set of controls
writing `workforce_target` and `try_switch_recipe`, because two sets of controls on one field is
how two surfaces come to disagree. A building still under construction shows "Under construction."
instead: there is no method to switch and no workforce to set yet.

**The two selectors compose.** Selecting a building on the map and pressing its row here both set
the same `selected_entity`, so both reach the same levers. The building Selection card's action
grid carries a fourth button that opens this view **aimed** at the selected building — its type
group expanded, the building selected — which is how a map selection reaches its levers in one
press.

**Construction is durative and material-gated.** A building under construction advances each economy tick by a rate in [0,1] set by how much of its per-tick material need (`resource_build_cost[r] / build_duration_ticks`) the local market (`market_for_tile`) can actually give it; below `1 / max_stretch` the build is **paused**. **What "can give it" means is the whole defect this surface once had (Ben, 2026-10-16 playtest):** the sim draws the market's **persistent `inventory`**, adds the `capacity_per_build_tick` row, and counts a good priced above `reservation_mult × base` as absent (`shelf_admits`). The panel's own mirror read `supply` — the per-tick flow, reset every tick — carried no capacity row and ignored the price ceiling, so it reported "ETA ~2 qtrs" while the sim held the same build at the `1 / max_stretch` floor for twenty. **One shared read now serves both**, and the mirror matching `run_construction`'s `site_rate` is the contract its declaration cites. A stall also **names the good** that holds it, in the status line, the stack reading, the queue row and the build door. Construction is measured in **days** (PRODUCTION.md § Construction is measured in days, Ben 2026-10-24): the site carries the days of work it has left, a quarter's supply rate sets how many of them it makes over the quarter, and it completes on a day. Every construction readout — the queue's **Days** column, the status line, the stack button and the site card — counts days left at the current rate, never quarters or ticks. **Payment timing (Ben, 2026-10-25):** the whole flat build cost is paid when the site is placed, and its materials are drawn in arrears at each economy tick (PRODUCTION.md § Construction is measured in days). So a site's progress bar — credits paid in against the capex — opens at the flat cost's share the day it is placed, and climbs as each tick bills the materials built.

**A row is two lines tall, and text wraps rather than truncating (Ben, 2026-10-17).** The grid's
columns are right and its one-line row was too tight: *"we are going to have to use two row spaces
for each building, this is so that text can wrap to the second row. Many rows truncate the text,
and it will look better with text wrap taking up more space."* So each row reserves **two line
heights**, and a column whose text does not fit **wraps into the second** instead of eliding. The
columns, their order and their widths are unchanged — this buys back the words a 70 px name column
was cutting, and a row that needs only one line still occupies two so the grid stays a grid.

**A road is built from a road to a tile, in two presses (Ben, 2026-10-17).** Selecting a tile and
pressing Build asked the wrong question — it tried to road the tile you had already selected, which
is why a roaded tile answered *"this tile already has an equal or better road."* A road is a
crossing between two tiles (`../../economy/LOGISTICS.md` § 4), so the press must name **which
neighbour**: select a tile that carries a road, press Build on the road row, and the tiles it could
legally cross to **highlight** — exactly the sides a crossing may legally carry, so never a side no
walk can cross, never a water-to-water span, and never one already holding an equal or better
road. Pressing a highlighted tile lays that crossing and charges it.

**A tile with nowhere to go mutes the press (Ben, 2026-10-18).** Where no neighbour admits a
crossing the Build press is **muted and unpressable**, and its explainer says why. The first cut
left it live, on the reasoning that arming and lighting nothing says the refusal more quietly than a
sentence does; Ben overruled that, and he is right that a press which arms and then does nothing
teaches the player nothing about *why*. A dead control that explains itself is the honest one. The
explainer is the only place the reason lives — it does not come back as a row of red text.

**One line a row, and the words live in the hover (Ben, 2026-10-17; the row is two lines tall since
the same day — see above).** *"We should move a lot of the
text on each construction panel to tooltips, and favour using icons where possible. We really need
to compact this."* Every row on this ledger — the build list, the queue, the tile card's stack
buttons, the expanded site card — is **one line**, and it reads in glyphs: the building glyph
(`../ICONS.md`), the cost, the materials, a **resource pip** for what it makes, the stack as `2/4`,
a state mark, the progress bar, the press. A word appears only where no glyph carries the meaning.

**The line is a grid, not a sentence (Ben, 2026-10-17).** Each thing it carries sits in its **own
column**, the same column in every row, so a column reads down the list as a column: every cost
under every cost, every stack under every stack. A row of chips flowing left to right at whatever
width its text happens to need would compact the surface and still refuse to be scanned — the
alignment *is* the compaction. A row whose figure is absent leaves its column empty rather than
closing the gap.

**Each element explains itself, and only itself (Ben, 2026-10-17).** The hover is **per column**, not
per row: hovering the cost explains the cost, the pip says what the building makes, the stack figure
says which cap it is counting against, the state mark names the good that stalls it. A card that
answered every column at once was the first cut, and it was long enough that reading it cost more
than the seven lines it replaced — *"I would expect the tooltip to be shorter, and give each element
in the row a different explainer."* A column's explainer is **one or two short sentences**; if it
needs more, the column is carrying two facts and should be two columns or one fact.

**What the line drops belongs to those explainers** (`../TOOLTIP.md`), not to a second line and
not to nothing: the sentence a chip replaced, the stall's named good, what the building makes in
words, and what filled a tile when it is full. The rule this enforces is that **a scan and a read
are different acts** — the row serves the scan, the card serves the read — and the surface failed it
when one row grew to seven lines and four rows filled the panel.

**Default view on open:** Buildings.
**Cross-cutting selectors that are NOT views (toggle-exempt):** none. The Buildings view's method
grid and workforce slider are levers on the selected building, not sub-views; the type-group
headers are an accordion, and a `CollapsingHeader` is already a toggle.

## 3. Lens on open
**Proposal: follow the sub-view.**
- **Construction → `opportunity`** (per-tile best-building net margin — the literal "where should I build?" signal, which is the question the build bar is on screen to answer).
- **Buildings → `production`** (per-tile output intensity — see what the estate is yielding while you tune it). Alternative: `none`, since a lever is about one already-selected building, not the map.

The ledger arms no lens on open; the mapping above is the proposal.

## 4. Data sources
- **Queue** — `world::buildings` filtered to `is_player_owned` and `ticks_remaining > 0`, nearest done first; rate from the shared `read_construction` (the market's **inventory**, the capacity row and the fair-price ceiling, against `recipe_registry::economics(type).resource_build_cost` and `build_duration_ticks`, `reg.construction().max_stretch`).
- **Estimated cost** — `compute_building_opex` (`budget_system.hpp`) over the same set, with `body_mean_habitability` feeding the workforce term.
- **Build cost, arming, placement** — `recipe_registry::economics()` (`build_cost`, `resource_build_cost[]`) and `placement_rules::k_extractable`, read by the tile Selection card rather than here.
- **Roster** — `player_building_groups` (`construction_panel.cpp`): every `is_player_owned` building, keyed by `building_group_name` (the Build door's own vocabulary — `extraction_building_name` for an extraction site, the active recipe's `group` for a processing facility, `building_type_name` otherwise). The per-building net is `estimate_building_profit(...).net()`, the SAME figure `rank_player_buildings_by_profit` ranks the Balance ledger on, so the two surfaces cannot report different profits for one building. A building the report cannot yet price contributes **zero** to its group's total, never a guess. Groups are walked in name order and members by net descending then entity id, so an unordered `world::buildings` cannot vary what a player or a check sees.
- **Per-building configuration** — `building_component::workforce_target`, `active_recipe_index` / `recipe`, written through `try_switch_recipe` and the workforce verbs from this ledger's levers. `decommissioned` is written from the building Selection card's action grid.

**Maps to mock `buildings.csv`:** `type`, `output`, `active`, `exhausted`, `corp` / `body` — enough to mock a built-inventory roster (filter to the player's owner id, 30310 in the pre-2026-10-23 mock). The CSV rows are the *built* estate, not a queue; it carries no `workforce_target` / `recipe` / `decommissioned` / `maintenance` column and no progress or ticks-left, so neither the live queue nor a Buildings roster can be reproduced from it without extending the exporter. There is no per-tile margin or output export for an `opportunity` / `production` mock either.

## 5. Close / toggle semantics
Rail slot 3 icon toggles the whole Construction ledger open/closed. Re-clicking the
**currently-active tab closes the ledger entirely** (releases the column), per the universal
toggle rule — not "collapse to Buildings". Switching to the *other* tab just changes view.

The queue header and the type-group headers are `CollapsingHeader`s, so each is already a toggle
in its own right; the levers (method grid, workforce slider) are exempt, being controls on a
selected building rather than expressions of an active state.

The tile Selection element's **Construct** button and the building card's **Open in ledger**
button are **doors, not toggles**: neither carries a visible active state, so a second press must
not close what it opened. Both route through `close_all_panels` first, so opening the Construction
ledger releases whichever other ledger held the column, exactly as a rail press does. The Selection
band is independent of the column and stays where it is.

## Open questions for Ben
- ~~Sort order of the type rows.~~ **Answered** (Ben, 2026-08-29): *"I'm fine with alphabetical, we will probably reconsider it for a later version anyhow."* Alphabetical stands — a neutral, deterministic order that asserts nothing. The case for ranking by total profit is recorded rather than lost: it is defensible under `CONCEPT.md`'s line, since these are buildings the player already owns and ordering them is navigation rather than the declined build-opportunity surface. Revisit with the version that reconsiders it.
- **Should the roster narrow to the SELECTION?** The design says "group all selected buildings, or
  all of them if none". Selection is single-entity today, so grouping over it collapses the roster
  to one row — and collapses it *under the press that selected the building*, destroying the
  navigation the view exists for. Built as: always the whole estate, with the selection driving
  which group is expanded and which row is highlighted. Confirm, or say what a multi-building
  selection would be.
- ~~**Queue table shape.**~~ **Answered in build (2026-10-17, superseding the previous day's
  four):** six columns — glyph, building (selects it), done (a bar), quarters left, cost left, and
  the state mark — the shape the grid ruling gives every construction row, so the queue, the build
  list, the tile card's stacks and the expanded site card all read in the same places. The build
  list and the rail rows take **eight** (glyph, name, cost, output pip, stack, state, bar, press);
  the queue, the tile card's stacks and the expanded site card take **six**, the site card
  borrowing the queue's own set so one site reads identically on both. Body, tile
  and every stall sentence moved onto the hover card, and the stall lines that printed under the
  table are gone. The 25 / 75 % colour bands were kept. Progress is
  `construction_paid / construction_capex`, which re-prices live, so the bar can wobble as prices
  and the site multiplier move; the drift-free alternative is storing the original total on the
  building, which is a save-format change and is not taken.
- **Lens: follow-the-view or fixed?** Confirm the mapping (Construction→opportunity /
  Buildings→production), or pin one lens for the whole ledger.
