# Construction — design Q&A

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `rail slot 6` · Source: `src/ui/construction_panel.cpp` · Mock table(s): `buildings.csv` · Related: `BL-122`, `BL-029` (queue backend), `BL-044` (material build cost), `BL-123` (selection resize)
> Host: shell fold-out column (BL-122), ~480px @1720.

## 1. Top question — the one thing this answers at first glance
**"What can I build right now, and what will it cost me?"** — the placement affordance. Opening Construction should put the player one click from arming a building type and reading its cost (cash + materials, per `reg.economics()`). The secondary questions that justify the tabs: **"How do I configure the buildings I already have?"** (workforce %, recipe, decommission — the Manage tab) and **"How do I sell what those buildings make?"** (per-body sell orders — the Sell Orders tab). These are three genuinely different verbs (place / configure / sell), which is exactly why the 2026-07-06 redesign split the old single scrolling column into tabs.

## 2. Sub-levels — views & default

| View | Answers (one question) | Content (live in code today) |
|---|---|---|
| **Build** | "What can I place, and what's it cost?" | Three arm-buttons (Extraction Site / Processing Facility / Port); for extraction, a `placement_rules::k_extractable` target picker; live cost readout (`build_cost` cr + `resource_build_cost[]` materials); "click a tile to build". Plus a **Queue** subsection — **placeholder only** (`any_items = false`, BL-029 owed). |
| **Manage** | "How do I configure the thing I have?" | Resolves the selected building (by building-id, falling back to tile-match); shows type/target/recipe/build-cost/maintenance; **live** workforce slider (0–200%), recipe combo (when >1 recipe), Decommission button. |
| **Sell Orders** | "How do I sell what I make?" | Per-body sell-order list for the player corp (resource ×qty ≥ floor); add/remove; reads the body's `market_component`. Flagged in-code as a **candidate to move to the Market ledger** later. |

**Default view on open:** `Build` (`panel_view == 0`, the placement affordance — the reason you opened Construction).
**Cross-cutting selectors that are NOT views (toggle-exempt):** the extraction **target** resource picker (Build), the **recipe** combo and **workforce** slider (Manage), and the **body / resource** combos (Sell Orders). None of these are sub-views; they're in-view selectors.

## 3. Lens on open
**Arm `opportunity`** (per-tile best-building net margin) on the **Build** view — it is the literal "where should I build?" signal that the placement verb is asking, and Ben's stated preference is that opening a menu usually arms a lens. Have the lens **follow the sub-view**, not fixed:
- **Build → `opportunity`** (best-margin tiles light up as build candidates).
- **Manage → `production`** (per-tile output intensity — see what your existing buildings are actually yielding while you tune them). Alternative: `none`, since Manage is about one already-selected building, not the map.
- **Sell Orders → `market`** (price signal for the resources you'd be selling).

Proposal, not currently wired — the panel does not arm any lens today.

## 4. Data — live vs plumbing gaps
**Live world state today:**
- **Build arming + cost** — fully live: `recipe_registry::economics()` gives `build_cost` and `resource_build_cost[]`; `placement_rules::k_extractable` drives the target list.
- **Manage** — fully live against `world::buildings`: `workforce_target`, `active_recipe_index`/`recipe`, `decommissioned` all read and **write back** to the component.
- **Sell Orders** — live against `state.sell_orders` and the body's `market_component`.

**Maps to mock `buildings.csv`:** `type`, `output`, `active`, `exhausted`, `corp`/`body` — enough to mock a **built-inventory / Manage roster** table (filter to player corp = 30310 Genom Systems). Note the CSV rows are the *built* estate, not a queue.

**Plumbing gaps (flag explicitly):**
- **Construction QUEUE is not authored** (BL-029). `draw_queue_section` always shows "No active construction." — there is no `w.construction_queue`, no progress %, no ticks-left, no cost-remaining. Any queue table in the mock is **owed data**, not live. This is the single biggest gap.
- **`buildings.csv` has no per-building `workforce_target` / `recipe` / `decommissioned` / `maintenance` column** — the Manage tab's live fields aren't in the export, so a Power BI "manage roster" can't show them yet. Export gap.
- **No `opportunity`/`production` per-tile margin data in mockdata** — if the lens ships, it needs a per-tile net-margin/output export that doesn't exist.
- **`exhausted` flag is exported but unused in the panel** — the code shows no exhaustion state; deposit-exhaustion surfacing is owed.

## 5. Close / toggle semantics
Rail slot 6 icon toggles the whole Construction ledger open/closed. Inside, re-clicking the **currently-active tab** (`Build` while on Build, etc.) **closes the ledger entirely** (releases the column), per the universal toggle rule — not "collapse to Build". Switching to a *different* tab just changes view. The in-view selectors (target, recipe, workforce, body/resource combos) are exempt. A new entity selection also closes the ledger (mutual-exclusion with Selection in the same column, BL-122/123).

One friction point to note: **Manage depends on the current selection**, which is the same state that closing-for-Selection consumes. If the player selects a building to manage it, that selection wants to *drive* the Manage tab — but a selection also closes any open ledger. Ben needs to rule on how "select a building → manage it" survives the mutual-exclusion (see open questions).

## Open questions for Ben
- **Manage-vs-Selection collision.** Manage reads `state.selected_entity`, but a new selection closes the ledger to show the Selection element. Should selecting a building **auto-open Construction/Manage** instead of the Selection card, or should Manage's controls migrate *into* the Selection element (and Construction lose its Manage tab)? This is a real overlap between two column tenants.
- **Does Sell Orders belong here at all?** The code itself flags it as a candidate to move to the **Market** ledger. Keep it in Construction (place → sell as one workflow) or move it out so Construction is purely build+manage?
- **Queue placement.** When BL-029 lands, does the Queue stay a subsection under Build, or become its own **4th tab** ("Queue")? It currently shares the Build view but answers a distinct "what's in flight?" question.
- **Lens per sub-view or fixed `opportunity`?** Confirm the follow-the-view mapping (Build→opportunity / Manage→production / Sell→market), or pin one lens for the whole ledger.
