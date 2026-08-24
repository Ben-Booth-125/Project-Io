# Selection element — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `selection-driven / not in rail` · Source: `src/ui/selection_panel.cpp` (`selection_card.cpp` for the full-canvas takeover) · Mock table(s): `none (all live)` · Authority: `SELECTION.md` (the element's own doc — this page is the 5-axis summary) · Owning items: `BL-598` (selection one accordion), `BL-534` (selection accordion and buildings), `BL-564` (selection one element), `BL-372` (lens-keyed selection), `BL-475` (corp ledger stance detail)
> Host: the **Selection band** — a pinned band of its own (`LAYOUT.md`), always open, not a column tenant.

## 1. Top question — the one thing this answers at first glance
**"What's my move on the thing I just clicked?"** This is the opposite of a ledger: not exhaustive reference reached from the rail, but a decision prompt that appears *because* the player selected something. It is polymorphic by selection **kind** (`selection_kind_of`: tile / body / building / market / unit / nation / corporation, plus an **active battle** and a **mercenary contract**, which are not entities but borrow the band). A **province** is not among them: it folded into the tile element as a set of accordion sections (`BL-598`). Each kind leads with its **action**, backed by decision-relevant **facts**, plus a `[>]` 'go to' (`focus_on_entity`) into the deep ledger where the encyclopedic detail lives. Secondary question per kind: *"what do I need to know to make that move?"* — the tile's suitability, the building's profitability, the body's commercial pulse, the unit's strength.

## 2. Sub-levels — views & default
Not tabbed views — **polymorphic by selection kind**. Two layout families share the band: the **three-column card** (left quarter: a picture; centre: the views; right quarter: a 2×3 action-glyph grid), and the older **action | facts split** (`draw_selection_action` at a 58 % width, `draw_selection_facts` to its right) for the kinds that have not had their own mockup.

The centre column comes in two idioms. The **tile** takes a true **accordion** — five stacked section headers, one open at a time, none open reachable (`BL-598`); every other card still takes a **pager** (‹ Name (i/N) › arrows plus the `disclosure_controls` full-canvas hook). The tile earned the accordion because it has five sections and the pager hid four of them behind an arrow press.

| Kind | Layout | Action | Facts / views | 'Go to' target |
|---|---|---|---|---|
| **Tile** (and, through it, its **province**) | three-column card, own header (`draw_tile_selection`) | **Construct Buildings** (opens the tile construction ledger — extractable-target picker, cost-annotated build buttons, affordability-gated), plus the rest of the glyph grid | A five-section **accordion**, ordered from what can be acted on to what the ground merely is: **Buildings** (per province: count vs ceiling, Built / Max per workable resource) · **Deposits** (the province's stock, summed) · **Resources** (this tile's per-deposit production vs top-decile, dropdown-selected, click-drillable to history) · **Population** (the province's centres, by scale and headcount) · **Terrain** (habitability / hazard vs body average) | — (already on the surface) |
| **Body** (planet/moon/asteroid/station) | action \| facts | Unsurveyed → **Dispatch Survey** (cost · ETA, funds-gated); surveyed → **Go to surface** | **Commercial activity** pulse — busy / steady / quiet or "outside your network" (`body_activity_visibility`) | Planetary surface |
| **Body** (star) | action \| facts | none | none | — |
| **Building** (player) | three-column card (`draw_building_selection_body`) | glyph grid: Manage, Mothball / Reopen, Auto workforce, Dismantle, Go to | `building_pages()`: Profitability (revenue vs segmented expenses, Net 6 mo., input basket) · Status · Workforce · Production method (every era-allowed recipe side by side, switched through `try_switch_recipe`) | host tile |
| **Building** (rival) | three-column card | none — intel only | type-keyed glyph (type is public); owner; `Production: private` / `Stockpile: private`, lifted only under spectator god view | host tile |
| **Unit** | three-column card (`draw_unit_selection_body`) | **Go to** plus reserved slots | `unit_pages()`: Strength (`strength`, `count`) · Roster (type via `unit_roster_table()`, owner) | unit position |
| **Battle** | own card (`draw_battle_selection`) | — | phase word, `round i / max_rounds`, each side's stack (`draw_battle_side`), redacted for the side the player cannot see | province |
| **Market** | action \| facts | **Go to** — locate on canvas | (empty) | market centre |
| **Nation / Corporation** | action \| facts | none — "Open its ledger via [>]." | corporation: empty, except the internals readout under spectator god view | its ledger |

**Default on open:** whichever kind was selected. With no valid selection the band frame substitutes the **player corporation**, so the band is never empty.
**Cross-cutting selectors (NOT views, toggle-exempt):** the Resources and Terrain sections' **dropdowns** (`ui_state::card_resource_page`) and the tile construction ledger's **extractable-target** picker — each picks *what*, not a view. The tile accordion's open section (`card_tile_view`) and the building/unit pagers (`selection_building_page`, `selection_unit_page`) are view selectors inside the card, not ledger tabs — but the tile's section headers ARE toggles, because each shows its own open state: pressing the open one closes it, and `card_tile_view = -1` (none open) is a reachable state.

## 3. Lens on open
**Contextual, per selection kind — a genuine open design call; the answer is `none`.** Selecting arms no lens. The settled "opening a menu usually arms a lens" preference maps cleanly onto the § *Lens-driven hover & selection resolution* rule in `SELECTION.md`, which couples *which lens is active* to *which entity resolves and which ledger it routes to* (`BL-372`, lens-keyed selection). A strawman that follows the existing lens/ledger coupling:

- **Tile** → **resource** lens (the tile's deposits are what the Build action reads) — *or* leave alone, since the Resources view already answers "what's here" without an overlay.
- **Body** → **market** lens (the commercial-activity fact is a market pulse) — or **none** while unsurveyed (nothing to overlay yet).
- **Building (player)** → **production** lens; **(rival)** → **corporation** lens (ownership is the only public fact).
- **Corporation** → **corporation** lens; **Nation** → **country** lens.

I'd fix the lens **on selection** (not follow a sub-view), and only when the kind has a natural overlay — leave it alone for star/unit. **Weakest guess here** — Ben should confirm whether selecting arms a lens at all, given the resolution rule says the lens *drives* selection, so auto-arming risks a feedback loop (a lens change re-resolving the stack under the pointer).

## 4. Data sources
**All live** — the Selection element reads the `world` directly through the `entity_summary` / placement / survey / profit / activity seams; it uses **none** of the `docs/ui/mockdata/` CSVs. Concretely:
- Build front door: `placement_rules::can_place[_in_world]`, `recipe_registry::economics` (cost), `market_for_tile` (material pricing), `w.corporations[player].balance` (afford gate).
- Terrain and Resources sections: `tile_component` habitability / hazard / `resource_deposit`, `placement_rules::k_extractable`, the body-wide top-decile reference. Buildings: `stack_capacity` summed over province member tiles through `can_place_in_world`, over `province_buildings_standing` / `province_building_ceiling`. Deposits: `resource_deposit` summed over the province's member tiles. Population: `world::population_centres` / `population_centre_tile` / `population_centre_name`, filtered to the tile's province.
- Survey: `survey.phase`, `survey_cost` / `survey_eta_days`.
- Profitability: `estimate_building_profit(w, reg, report, id)` (needs one economy tick to have run, else "Run an economy tick to estimate."); Production method: `recipe_registry` era-allowed recipes, `try_switch_recipe`.
- Commercial activity: `body_activity_visibility` + per-market throughput.
- Unit: `unit_component::strength`, `count`, `type`, `owner`; Battle: `active_battle` state and `params.max_rounds`.

**Redaction** is a UI rule, not a world one: rival production, stockpile and corporation internals are withheld per the competitor-visibility rule (`DISCOVERY.md`), and the spectator god view lifts it at the read-site only.

## 5. Close / toggle semantics
**Different from the ledgers — selection-driven, not a rail toggle.** The universal toggle rule (re-click active tab closes the ledger) does **not** apply, because the Selection has no nav-rail slot and no ledger tabs. Its semantics:
- **Re-populates** on a valid single-click selection (`selected_entity` set, `selection_kind != none`); a battle press sets the `selected_battle_*` triple, a contract press `selected_contract_id`. A press on bare ground selects the **tile** and mirrors its province into `selected_province` for the canvas outline — no gesture selects a province on its own.
- **No close button** — the band is always open; with nothing selected it shows the player corporation.
- **Independent of the fold-out column**: opening or closing a ledger never touches the selection, and selecting never closes a ledger.
- **Single-click empty space** clears the selection → the band falls back to the player corporation.
- The `[>]` button is a navigate, not a toggle — equivalent to a double-click on the current selection (`focus_on_entity`).
- Tile repeat-click cycles through a multi-building tile's occupants (`SELECTION.md` § Tile repeat-click selection cycle).

## Open questions for Ben
- **Does selecting arm a lens at all?** The `SELECTION.md` resolution rule has the lens *driving* which entity resolves, so auto-arming a lens on selection risks re-resolving the pointer stack under the player — a feedback loop. Fork: (A) selection never touches the lens (the band's behaviour), or (B) selection arms a fixed contextual lens per kind (my strawman in §3). I lean (A) given the coupling.
- **Do the action | facts kinds (body, market, nation, corporation) migrate to the three-column card?** Tile, building, unit, battle and contract have their own mockups; the rest keep the 58/42 split until they do. Confirm the card is the target shape for all kinds — and whether the tile's **accordion** or the others' **pager** is the target centre idiom, now that the band carries both.
- **Stubbed facts (market / nation / corporation):** design real facts for these (e.g. market → top movers; corporation → the stance detail `BL-475` proposes) so the band isn't half-empty, or keep them thin and let `[>]` carry it? The settled taxonomy says corp/market drill-downs are the standalone ledgers, which argues for thin.
- **Canvas hit-testing for the marker kinds.** Building, unit and market markers are selectable from ledger rows and from the canvas where the lens-keyed rule resolves them (`BL-372`); confirm the per-lens resolution order rather than a kind-priority stack.
