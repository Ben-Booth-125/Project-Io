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
| **Buildings** *(default)* | "What do I own, and how much is each kind of it making me?" | One row per building **type** the player owns at least one of — the type's display name, its **count**, and the **summed per-quarter net** of its members, sign-coloured. The list takes the **whole height of the tab**, less exactly what the levers below it need, scrolling only past that (Ben, 2026-10-26). **Expanding a row** lists that type's own buildings, one line each carrying the tile and that building's own net, visibly summing to the header's total; **pressing one selects it**, and its **levers** draw below the roster. One group open at a time (an accordion). |
| **Construction** | "What's in flight, what is it costing, where could this building go, and what could I put on this tile?" | A **collapsible** queue header carrying **N cr / qtr** (`estimated_quarterly_construction_cost` — `compute_building_opex` maintenance + wages summed over every player building with `ticks_remaining > 0`, the same formula the budget loop uses), then, when expanded, one row per in-progress build with a progress cell colour-coded dim (<25 %) / mid (25–75 %) / high (>75 %). Below it the **site finder** — a building picker and, with a building chosen, the **site ledger** (§ The site ledger, `construction_sites.cpp`) — and below that the **tile build bar** (`draw_construction_ledger_body`, `selection_panel.cpp`). |

**Why Buildings is the default.** The queue is empty most of the time and the player always owns
buildings, so opening on the queue makes the ledger's front door an empty room.

**Why the queue collapses.** Same fact, applied to the queue's own space: closed it costs one line
and the build bar below it is what a player meets; open it says when things land. A
`CollapsingHeader` is a toggle by construction, so the standing Toggle rule needs no second control.

### The two empty states differ, deliberately

- **Construction, no tile selected** → the build bar says **"Select a tile"**, and nothing else.
  The tiles the player *could* build on are the **site ledger's**, directly above it: building
  first, then the tile (Ben, 2026-10-25). The build bar stays a targeted surface — it prices one
  tile — and the site ledger is how a tile is found.
- **Buildings, a type row expanded** → the type's **own buildings**, each pressable. It lists what
  the player already owns, in a group whose count has already asserted they exist — navigation. It
  also removes a dead end: the buildings behind a count are the set the ledger has in hand.

### The site ledger — building first (Ben, 2026-10-25)

*"Currently, choosing a building to construct requires looking at the land, choosing a tile, then
placing a building. It's not clear which places would be most profitable. When the construction
tab is open, we should use a lens to show which tiles are valid, and open a ledger to compare
tiles, sortable by profit. This should take into account logistics points also, and flag tiles
which do not currently have logistics operating."*

This overturns the 2026-08-29 line that any ordering of the buildable tiles is a recommendation
(`../../CONCEPT.md` § Player identity now holds the rule that stands: the interface may rank and
compare; it never acts for the player). The ledger orders; the player still picks a row and
presses Build.

**1. Top question.** *"Where could this building go on my ground, and which of those tiles would
earn most?"*

**2. Sub-levels.** None — one table under one selector. The **building picker** (a combo, "Find a
site") offers every building the Build door offers, in the door's vocabulary and fold order: one
extraction option per extractable good ("Farm" covers the coastal Fishing Wharf, the water option
covers the Well), one **per processing recipe** (the profit turns on the recipe, so the finder
prices a recipe, where the door folds recipes under their group and picks the best per tile), then
the infrastructure and military types. The door's era, tech and recipe-unlock filters apply; the
body's-air filter is applied per tile.

**The table.** One row per tile of the player's **own ground** on the active body that the **Build
door accepts** the chosen building on — `placement_rules::can_place_in_world` with the player's
actor named (own ground, tech, the road a building needs, stacks, the province ceiling), the
recipe's air, and the treasury against `construction_capex`, exactly the door's enabled-press
test. Tiles that fit but cost more than the treasury are counted under the caption, not listed.
Six columns, **every one sortable** by its header, and **every header legible at every supported width** (Ben, 2026-10-26: no "Yi…" elision) — each fixed column is sized to the wider of its widest figure and its header, the two named columns share the rest in proportion to their headers, and the sort arrow's width is reserved only on the column the rows are sorted by — the one place the arrow draws — so a header never loses its tail to it:

| Column | Figure |
|---|---|
| **Place** | The row's identity: the named **population centre whose province holds the tile**, or, where no centre's province covers it, the nearest named centre on the body, read **"near X"**. There is no grid-position column (Ben, 2026-10-26): an x,y is an address only the map can read, and a place name is one the player already knows. A province has no name of its own (`../../GLOSSARY.md` § Province) but it is its centre's ground, so the centre names it; where a province holds two centres, the lower id names it, so the name is one answer. The row's press: it **selects the tile and frames it** on the map; the tile's Build door below places the building. The press is the whole row. Sorted by name, ties by tile id. |
| **Net** | Est. net / qtr — `estimate_prospective_profit`, the door's own figure with the same assumptions (today's local prices, 50 % staffing, no labour shortage), **with the tile's Logistic Point cover applied** (below). Sign-coloured. The default sort, **descending**. |
| **Cost** | `construction_capex` — the flat cost times the site multiplier, paid once at placement, plus the materials at the local market's posted price, bought in arrears as each tick draws them (Ben, 2026-10-26: costs depend on market prices; PRODUCTION.md § Construction is measured in days). The hover splits the two halves and names the multiplier. |
| **Market** | The market the tile clears in, by its city's name (`market_city_name`). |
| **LP** | The tile's Logistic Point cover, 0–100 % (`lp_route_to(...).cover`). **Zero is the no-logistics flag** (`icons::no_logistics`, a struck truck, in amber), drawn in place of the figure — the flag is the zero, and the width a "0%" beside it took was the width the headers needed (Ben, 2026-10-26): no source both reaches the tile and is tied to the capital, so a building there would stand and not run. The hover says which — no source reaches it, or one does and has no chain to the capital. |
| **Yield** | Extraction: units of the good per quarter, the estimate's own revenue at the price it valued it at; the hover gives the deposit's richness and what is left in the ground, or names a Well or Wharf as depositless. Empty for a building whose output does not turn on the ground. |

**Every tie is broken by tile id, ascending**, in every column and either direction, so the order is
total and reproducible; an unpriced row (no market resolves) sorts last on Net. The rows are one
line each — this is a sortable table with headers, not the build list's two-line glyph grid — and
each column explains itself on the shared hover card, one or two sentences, the § "Each element
explains itself" rule.

**Logistic Points are in the profit** (Ben, 2026-10-25: *"This should take into account logistics
points also"*). A producing building is granted only its cover's share of its run, and the
economy step folds that share into its labour, so output and the inputs that output takes fall with
it while maintenance and wages do not. `estimate_prospective_profit` takes the cover as an argument
and applies exactly that; the site ledger and the Build door both pass the tile's real cover, so a
row and the door quote one figure. The estimate assumes the nation's pool itself is not short —
that is the one part of the grant that is not a property of the tile. **Labour is not in the
estimate** (it assumes no labour shortage, as the door always has), so the ledger carries no labour
column.

**3. Lens.** Choosing a building arms the **placement lens** (`../LENSES.md` § The placement lens):
the same tiles the table lists are marked on the map — green, amber with the struck truck where no
Logistic Points operate — and every other tile is dimmed. It is a mode of the build flow, not an
`overlay_mode`: it composites over whatever lens is up and no saved view binding holds it.

**The selected site keeps its wash** (Ben, 2026-10-26). Pressing a row selects its tile, and the
placement lens keeps that tile green (or amber) with the selection mark drawn over it — dropping the
colour from the one tile the player just chose read as the site being withdrawn (`../LENSES.md`
§ The placement lens).

**4. Data sources.** `site_options` and the evaluation in `construction_sites.cpp`, over
`body_tile_grid` filtered to `tile_to_nation == player_actor`. Rebuilt only when its key moves — the
chosen building, the active body, the treasury, the reach budget, and the frame stamp (the economy tick,
the building count, the ground's owners, the logistics generation) — so a still frame pays a
comparison. The Logistic Point cover is cached on the same stamp with one routing memo
(`site_lp_cover`), which the Build door reads too.

**5. Close / toggle.** The picker is a **selector**: choosing a building aims the finder at it, and
choosing the **chosen** building again — or **None** — clears it, so its visible state is a toggle.
The lens is live only while the Construction view draws (switching to Buildings leaves the map
plain), and **closing the ledger clears the choice**, the disarm-on-close every ledger-lens pair
takes. A row press is a door, not a toggle: it selects and frames.

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

**Construction is durative and material-gated.** A building under construction advances each economy tick by a rate in [0,1] set by how much of its per-tick material need (`resource_build_cost[r] / build_duration_ticks`) the local market (`market_for_tile`) can actually give it; below `1 / max_stretch` the build is **paused**. **What "can give it" means is the whole defect this surface once had (Ben, 2026-10-16 playtest):** the sim draws the market's **persistent `inventory`**, adds the `capacity_per_build_tick` row, and counts a good priced above `reservation_mult × base` as absent (`shelf_admits`). The panel's own mirror read `supply` — the per-tick flow, reset every tick — carried no capacity row and ignored the price ceiling, so it reported "ETA ~2 qtrs" while the sim held the same build at the `1 / max_stretch` floor for twenty. **One shared read now serves both**, and the mirror matching `run_construction`'s `site_rate` is the contract its declaration cites. A stall also **names the good** that holds it, in the status line, the stack reading, the queue row and the build door. Construction is measured in **days** (PRODUCTION.md § Construction is measured in days, Ben 2026-10-24): the site carries the days of work it has left, a quarter's supply rate sets how many of them it makes over the quarter, and it completes on a day. Every construction readout — the queue's **Days** column, the status line, the stack button and the site card — counts days left at the current rate, never quarters or ticks. **Payment timing (Ben, 2026-10-25):** the whole flat build cost is paid when the site is placed, and its materials are drawn in arrears at each economy tick (PRODUCTION.md § Construction is measured in days). **Progress is days, not credits (Ben, 2026-10-26).** Every progress reading — the queue's Done bar, the site card's bar and the map's progress circle — is the days of work built over the days the whole build takes (`construction_progress_fraction`). A credit fraction opened at the flat cost's share the day a site was placed and re-priced with every price move, so it never said how much of the work was done.

### The progress circle and the site card (Ben, 2026-10-26)

*"Over every building under construction's tile, draw a circle showing build progress (days done /
total). Hovering that circle, or the site's row in the Construction queue, shows the materials
needed to build."*

**The circle.** On the Planetary canvas, over each of the player's own sites under construction, a
ring fills clockwise from twelve o'clock with the **days built over the days the build takes**
(`construction_build_days` at the tile's live site multiplier, less `construction_days_left`). Its
ink is the quarter's rate — green on schedule, amber stretched, red paused
(`construction_status_colour`) — so a stalled site reads without a hover. It draws under any lens,
because it is the site's own mark rather than a lens, wherever a tile is drawn at 5 px or more.
Rivals' sites carry none: what a rival's build still needs is its own affair.

**The site card.** Pointing at the circle — at once, with no dwell — or at a queue row's name or Done
bar opens one card, the same function on both surfaces (`construction_site_card`), so the map and
the ledger cannot tell one site two stories:

- the building's name and **Day D of T built**;
- the status line (`construction_status`), which names the good holding a stall;
- **Still to draw:** every material the site has yet to take off the shelf, each with the site
  market's **posted price now** — what the draw pays for it, in arrears at the tick (Ben,
  2026-10-26: costs depend on market prices; the flat credits were paid once at placement). The
  amount is `run_construction`'s own row (`resource_build_cost_for` over the duration in quarters)
  for the days left **plus** the days built and not
  yet drawn (`build_arrears_milli`);
- an **"!"** beside any good the market is short of, by **the construction draw's own test**: the
  shelf holds less than this quarter's draw needs (`construction_read::short_of`, set where
  `site_rate`'s coverage of that good is below 1). No second shortage test exists.

The entity glance card does not open over the ring; the site card owns that moment. The other queue
columns keep their own one-fact explainers.

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
**None on open; the placement lens while a building is chosen** (Ben, 2026-10-25). The Construction
view arms the placement lens the moment its site finder has a building chosen, and it disarms when
the choice clears, the view changes or the ledger closes (§ The site ledger). The Buildings view arms
nothing. The 2026-08 proposal to arm the retired `opportunity` / `production` lenses is withdrawn:
both lenses retired (BL-604), and the placement lens is keyed to a building the player chose rather
than a best-of-everything field.

## 4. Data sources
- **Queue** — `world::buildings` filtered to `is_player_owned` and `ticks_remaining > 0`, nearest done first; rate from the shared `read_construction` (the market's **inventory**, the capacity row and the fair-price ceiling, against `recipe_registry::economics(type).resource_build_cost` and `build_duration_ticks`, `reg.construction().max_stretch`).
- **Estimated cost** — `compute_building_opex` (`budget_system.hpp`) over the same set, with `body_mean_habitability` feeding the workforce term.
- **Build cost, arming, placement** — `recipe_registry::economics()` (`build_cost`, `resource_build_cost[]`) and `placement_rules::k_extractable`, read by the tile Selection card rather than here.
- **Roster** — `player_building_groups` (`construction_panel.cpp`): every `is_player_owned` building, keyed by `building_group_name` (the Build door's own vocabulary — `extraction_building_name` for an extraction site, the active recipe's `group` for a processing facility, `building_type_name` otherwise). The per-building net is `estimate_building_profit(...).net()`, the SAME figure `rank_player_buildings_by_profit` ranks the Balance ledger on, so the two surfaces cannot report different profits for one building. A building the report cannot yet price contributes **zero** to its group's total, never a guess. Groups are walked in name order and members by net descending then entity id, so an unordered `world::buildings` cannot vary what a player or a check sees.
- **Site ledger** — `construction_sites.cpp` (§ The site ledger): `can_place_in_world`, `construction_capex`, `construction_site_multiplier`, `estimate_prospective_profit` with the tile's `lp_route_to` cover, `market_for_tile` / `market_city_name`, the tile's deposit and reserve, and the row's place from `population_centre_tile` / `population_centre_name` through `provinces.province_of`.
- **Progress circle and site card** — `read_construction_site` (`construction_panel.cpp`): `construction_build_days`, `construction_days_left`, `build_arrears_milli`, `resource_build_cost_for`, and `read_construction`'s per-good `short_of`.
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
- ~~Sort order of the type rows.~~ **Answered** (Ben, 2026-08-29): *"I'm fine with alphabetical, we will probably reconsider it for a later version anyhow."* Alphabetical stands — a neutral, deterministic order that asserts nothing. The case for ranking by total profit is recorded rather than lost: since Ben's 2026-10-25 ruling (`CONCEPT.md` § Player identity) a ranking is information, and the site ledger now ranks build sites by profit, so nothing stands in its way. Revisit with the version that reconsiders it.
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
  table are gone. The 25 / 75 % colour bands were kept. Progress is the days of work done over the
  site's whole days (§ Construction is durative); the total is recomputed live, so the bar can move
  as the site multiplier does; the drift-free alternative is storing the original total on the
  building, which is a save-format change and is not taken.
- ~~**Lens: follow-the-view or fixed?**~~ **Answered** (Ben, 2026-10-25): the Construction view arms the
  placement lens while a building is chosen; the Buildings view arms none (§ 3).
