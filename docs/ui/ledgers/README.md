# Ledger design Q&A

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
  Corporation and Market are the drill-downs beneath it.
- **Toggle rule** — the rail icon toggles the ledger; re-clicking the **active tab closes the ledger**;
  cross-cutting selectors are exempt.
- **Selection** — the Selection element is a band of its own, always open, with no rail slot
  (`SELECTION.md`); it is not a column tenant.

## The docs

| Doc | Default view | Sub-levels | Lens on open |
|---|---|---|---|
| [corporation.md](corporation.md) | Standings | one table + Selection drill (Profiles proposed) | `corporation`, fixed (proposed) |
| [balance.md](balance.md) | Treasury | Treasury / Cashflow / Assets (proposed) | **none** (money has no map field) |
| [market.md](market.md) | Goods | Goods (the flattened price table) / Trades | `market`, fixed |
| [construction.md](construction.md) | Buildings | Buildings (estate by type) / Construction (queue + build bar) | **none** — `opportunity` is refused, see below |
| [selection.md](selection.md) | — (polymorphic by kind) | tile / province / body / building / unit / battle / market / corp / nation | contextual (none) |
| [tile_ledger.md](tile_ledger.md) | last-left view | Story / Chain / Ages | none |

## Cross-doc findings (see [_critic_notes.md](_critic_notes.md))
- **A whole-enterprise aggregate ledger does not survive the duplication test.** Its cashflow view is
  Balance's cashflow tab and its standing rank is Corporation's Standings, both to the same figures; the
  residue that is genuinely aggregate-only is thin, and the parts of it that name rivals disclose more
  than competitor visibility allows (`DISCOVERY.md`). Aggregation is a view on an owning ledger, not a
  ledger of its own.
- **`overlay_mode::industry` is not a sector lens**: it paints the AI-owned nation substrate, not the
  player's sector mix, so it is the wrong pairing for any surface asking "what am I in".
- **`overlay_mode::opportunity` is refused as a ledger pairing** (Ben, 2026-08-29). A per-tile
  best-margin field answers "where should I build next" outright, and a player who follows it has
  stopped choosing. `CONCEPT.md` § Player identity holds the rule and the qualification that
  followed it the same day: rank where the top row is one input among several, never where the top
  row simply IS the move. Market prices rank; build sites do not.
- Clean, well-surfaced overlaps: standing orders belong to the Market ledger (where they live), not
  Construction; the History ledger carries no market section. **Convoys belonged to neither** — it
  is cargo in transit, `SUPPLY.md`'s subject rather than `MARKETS.md`'s, and now has its own doc.
