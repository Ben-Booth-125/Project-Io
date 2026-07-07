# Selection element — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `selection-driven / not in rail` · Source: `src/ui/selection_panel.cpp` · Mock table(s): `none (all live)` · Related: `BL-093, BL-071, BL-074, BL-067, BL-089, BL-068, BL-122, BL-123`
> Host: shell fold-out column (BL-122), ~480px @1720.

## 1. Top question — the one thing this answers at first glance
**"What's my move on the thing I just clicked?"** This is the opposite of a ledger: not exhaustive reference reached from the rail, but a decision prompt that appears *because* the player selected something. It is polymorphic by selection **kind** (tile / body / building / market / unit / nation / corporation), and each kind leads with ONE hero **action** (`draw_selection_action`), backed by a slim line of decision-relevant **facts** (`draw_selection_facts`), plus a `[>]` 'go to' into the deep ledger where the encyclopedic detail lives. Secondary question per kind: *"what do I need to know to make that move?"* — the tile's suitability, the building's profitability, the body's commercial pulse.

## 2. Sub-levels — views & default
Not tabbed views — **polymorphic by selection kind**. The "sub-levels" are the per-kind action/facts pair, selected by *what you clicked*, not by a nav tab:

| Kind | Hero action (`draw_selection_action`) | Facts (`draw_selection_facts`) | 'Go to' target |
|---|---|---|---|
| **Tile** | **Build here** — build front door: extractable-target radios + Build extraction site / processing facility / port, each cost-annotated (`100 cr · 20 Steel`) and affordability-gated | **Suited for** — territory owner + Thrives/Valid/Invalid affordance readout (BL-071) | No-op (already on the surface) |
| **Body** (planet/moon/asteroid/station) | Unsurveyed → **Dispatch Survey** (cost·ETA, funds-gated); surveyed → **Go to surface** | **Commercial activity** pulse — busy/steady/quiet or "outside your network" (BL-089) | `focus_on_entity` → Planetary surface |
| **Body** (star) | none | none | — |
| **Building** (player) | **Manage building** → opens construction panel | **Profitability (est./tick)** — Revenue/Inputs/Wages/Maint + Net (BL-074) | host tile |
| **Building** (rival) | none — "Competitor building - intel only." | Owner + `Production: private` / `Stockpile: private` (BL-068) | host tile |
| **Market / Unit** | **Go to** — locate on canvas | (stubbed) | entity position |
| **Nation / Corporation** | none — "Open its ledger via [>]." | (stubbed) | a ledger |

**Default on open:** whichever kind was selected — there is no neutral default; the element only exists while a valid selection stands.
**Cross-cutting selectors (NOT views, toggle-exempt):** the tile build front door's **extractable-target radio row** (`ui.construction.target`) — it picks *what* to extract, not a view. It's the only in-panel selector.

## 3. Lens on open
**Contextual, per selection kind — this is a genuine open design call, currently `none`.** The code today arms no lens on selection. But the settled "opening a menu usually arms a lens" preference maps cleanly onto the § *Lens-driven hover & selection resolution* rule already in SELECTION.md, which couples *which lens is active* to *which entity resolves and which ledger it routes to*. A reasonable strawman that follows the existing lens/ledger coupling:

- **Tile** → **resource** lens (the tile's deposits are what the Build action reads) — *or* leave alone, since the affordance readout already answers "what's here" without an overlay.
- **Body** → **market** lens (the commercial-activity fact is a market pulse) — or **none** while unsurveyed (nothing to overlay yet).
- **Building (player)** → **production** lens; **(rival)** → **corporation** lens (ownership is the only public fact).
- **Corporation** → **corporation** lens; **Nation** → **country** lens.

I'd fix the lens **on selection** (not follow a sub-view, since there are none), and only when the kind has a natural overlay — leave it alone for star/unit. **Weakest guess here** — Ben should confirm whether selecting arms a lens at all, given the resolution rule says the lens *drives* selection, so auto-arming risks a feedback loop (a lens change re-resolving the stack under the pointer).

## 4. Data — live vs plumbing gaps
**All live** — the Selection element reads the `world` directly through the `entity_summary`/placement/survey/profit/activity seams; it uses **none** of the `docs/ui/mockdata/` CSVs. Concretely:
- Build front door: `placement_rules::can_place[_in_world]`, `recipe_registry::economics` (cost), `market_for_tile` (material pricing), `w.corporations[player].balance` (afford gate). **Live.**
- Affordance readout: `tc.resource_deposit`, `placement_rules::k_extractable`, nation territory scan. **Live.**
- Survey: `bit->second.survey` phase, `survey_cost`/`survey_eta_days`. **Live.**
- Profitability: `estimate_building_profit(w, reg, report, id)`. **Live** (needs one economy tick to have run, else "Run an economy tick to estimate.").
- Commercial activity: `body_activity_visibility` + per-market throughput. **Live.**

**Plumbing gaps (not new data — layout & wiring):**
- **BL-123 re-lay-out (the real gap).** The content still uses the wide-bottom-bar **action|facts horizontal split** (58% / rest, `BeginChild` side-by-side, header stat offsets `v1=68, l2=150, v2=210` in `draw_building_profit`). Those fixed pixel offsets and the horizontal split were tuned for a wide short bar; in the ~480px tall column they'll cramp. **Ben to mock (BL-123) — not designed here.**
- **Market / Unit / Nation / Corporation facts** are stubbed (empty). Nation & corporation kinds aren't independently click-selectable on canvas yet (SELECTION.md § Open questions).
- **Building/unit/market canvas hit-testing** not wired — those kinds are reachable only from ledger rows today.

## 5. Close / toggle semantics
**Different from the ledgers — selection-driven, not a rail toggle.** The universal toggle rule (re-click active tab closes the ledger) does **not** apply, because the Selection has no nav-rail slot and no tabs. Its semantics:
- **Opens** on a valid single-click selection (`selected_entity` set, `selection_kind != none`).
- **`[x]` closes** by setting `ui.selection_hidden_for = selected_entity` — hides *this exact selection* until the next (different) selection re-shows it. Close hides, it does not destroy; state persists.
- **Mutually exclusive with ledgers** (settled): a new selection closes any open ledger to take the column; while a ledger owns the column this draw is gated off; the selection reappears when the ledger closes.
- **Single-click empty space** clears selection → element hidden (empty state).
- The `[>]` button is a navigate, not a toggle — equivalent to a double-click on the current selection (`focus_on_entity`).

## Open questions for Ben
- **Does selecting arm a lens at all?** The SELECTION.md resolution rule has the lens *driving* which entity resolves, so auto-arming a lens on selection risks re-resolving the pointer stack under the player — a feedback loop. Fork: (A) selection never touches the lens (current behaviour), or (B) selection arms a fixed contextual lens per kind (my strawman in §3). I lean (A) given the coupling.
- **Does the action|facts split survive the narrow column, or does it stack vertically?** In ~480px I'd expect **action on top, facts below** (vertical) rather than the current 58/42 horizontal `BeginChild` split — but that's BL-123's call. Confirm the intended stacking so the mock reflects it.
- **Tile build front door: keep all three build buttons + the radio row inline, or fold to a compact list/menu?** Three buttons each with a cost annotation plus a per-resource radio row is the panel's densest content (`content_rows` = 9, the tallest kind) — likely the tightest fit in the narrow column.
- **Stubbed kinds (market/unit/nation/corporation facts):** do we design real facts for these now (e.g. market → top movers, corporation → balance sparkline) so the mock isn't half-empty, or keep them stubbed and let the `[>]` ledger carry it? The settled taxonomy says corp/market drill-downs are the standalone ledgers, which argues for keeping these facts thin.
