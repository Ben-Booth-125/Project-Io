# Ledger design Q&A

> **Settles:** which five questions every ledger Q&A must answer · what these
> working docs are for, and what standing they have against the authority docs they
> feed · which settled decisions each Q&A must honour — the column host, the
> no-two-surfaces-answer-the-same-question rule, the toggle rule, the Selection
> band's exemption · which surfaces have a Q&A of their own.
> **Not here:** any single ledger's answers (the sibling files here) · the rail and
> the ledger set (MENU) · the column's geometry (LAYOUT).
> **Confused with:** MENU.md, LAYOUT.md, SELECTION.md.

Working design docs for the **ledger-mockup pass**: one 5-axis Q&A per fold-out surface, drafted as a
**strawman for Ben to revise** before mocking each ledger in Power BI (data in `docs/ui/mockdata/`).
Each doc answers the same five questions — **top question · sub-levels + default · lens on open · data
sources · toggle/close semantics** — and closes with an **"Open questions for Ben"** list.

**Lifecycle:** Ben revises these → the settled answers seed a backlog item per menu (`design` field
in `backlog.json`) → implemented to Ben's Power BI mockups. These docs are the intermediate, not the
authority; `backlog.json` is authority once items are minted, and the subject's authority doc
(`MENU.md`, `SELECTION.md`, `LAYOUT.md`) once the work is in the game.

## Settled decisions these docs honour
- **Column host** — every ledger draws into the shell fold-out column (`foldout_begin`,
  `src/ui/foldout_column.hpp`), whose width is derived by `shell_column_width(disp.x)` — 380–460 px by
  resolution, ~380 px at 1720 wide; sub-views are one-question-per-view tabs (`ui::nav_button`).
- **No two surfaces answer the same question** — an aggregate read belongs on exactly one ledger, and
  the Nation overview and Market are the drill-downs beneath it.
- **Toggle rule** — the rail icon toggles the ledger; re-clicking the **active tab closes the ledger**;
  cross-cutting selectors are exempt.
- **Selection** — the Selection element is a band of its own, always open, with no rail slot
  (`SELECTION.md`); it is not a column tenant.

## The docs

| Doc | Default view | Sub-levels | Lens on open |
|---|---|---|---|
| [corporation.md](corporation.md) | Balance (the Nation overview, slot 1) | one card; the slot-8 stance ledger **retired** (Ben, 2026-10-23) | none |
| [balance.md](balance.md) | Treasury | Treasury / Cashflow / Assets (proposed) | **none** (money has no map field) |
| [acquisitions.md](acquisitions.md) | **Retired** with the buyout (Ben, 2026-10-23) — the doc is a record | — | — |
| [market.md](market.md) | Goods | Goods (the flattened price table) / Trades | `market`, fixed |
| [convoys.md](convoys.md) | Convoys in flight | one flat view (Routes and History proposed, neither backed by a store) | `supply_routes` |
| [construction.md](construction.md) | Buildings | Buildings (estate by type) / Construction (queue + site ledger + build bar) | **the placement lens** while a building is chosen on the Construction view — a mode of the build flow, not an `overlay_mode` |
| [selection.md](selection.md) | — (polymorphic by kind) | tile / province / body / building / unit / battle / market / nation | contextual (none) |
| [tile_ledger.md](tile_ledger.md) | last-left view | Story / Chain / Ages / Tectonics | none |

## Cross-doc findings (see [_critic_notes.md](_critic_notes.md))
- **A whole-enterprise aggregate ledger does not survive the duplication test.** Its cashflow view is
  Balance's cashflow tab and its standing rank was the retired corporation Standings, both to the same figures; the
  residue that is genuinely aggregate-only is thin, and the parts of it that name rivals disclose more
  than competitor visibility allows (`DISCOVERY.md`). Aggregation is a view on an owning ledger, not a
  ledger of its own.
- **`overlay_mode::industry` is not a sector lens**: it paints the AI-owned nation substrate, not the
  player's sector mix, so it is the wrong pairing for any surface asking "what am I in".
- **Build sites rank by estimated profit** (Ben, 2026-10-25, overturning the 2026-08-29 refusal of a
  best-margin build-opportunity surface). The Construction view's site ledger lists every tile that takes
  a chosen building, sorted by profit, and the placement lens marks them on the map. `CONCEPT.md`
  § Player identity holds the rule that stands: the interface may rank and compare, and never acts for
  the player. The retired `overlay_mode::opportunity` stays retired — the placement lens is keyed to a
  chosen building, not a per-tile best-of-everything field.
- Clean, well-surfaced overlaps: standing orders belong to the Market ledger (where they live), not
  Construction; the History ledger carries no market section. **Convoys belonged to neither** — it
  is cargo in transit, `SUPPLY.md`'s subject rather than `MARKETS.md`'s, and it is a rail ledger of
  its own ([convoys.md](convoys.md), rail slot 6).
