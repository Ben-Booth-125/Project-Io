# Ledger design Q&A — mockup pass

Working design docs for the **ledger-mockup pass**: one 5-axis Q&A per fold-out surface, drafted as a
**strawman for Ben to revise** before mocking each ledger in Power BI (data in `docs/ui/mockdata/`).
Each doc answers the same five questions — **top question · sub-levels + default · lens on open · data
gaps · toggle/close semantics** — and closes with an **"Open questions for Ben"** list.

**Lifecycle:** Ben revises these → the settled answers seed a **backlog item per menu** (`design` field
in `backlog.json`) → implemented to Ben's Power BI mockups. These docs are the intermediate, not the
authority; `backlog.json` becomes authority once items are minted.

## Settled decisions these docs honour (this session)
- **Column host** — every ledger draws into the shell fold-out column (BL-122), widened to ~480px @1720;
  splits into one-question-per-view tabs (`ui::nav_button`).
- **Economy = aggregate-only** — Corporation and Market are the drill-downs; no two surfaces answer the
  same question.
- **Toggle rule** — the rail icon toggles the ledger; re-clicking the **active tab closes the ledger**;
  cross-cutting selectors are exempt.
- **Selection** — column-hosted, mutually exclusive with the ledgers, no rail slot; content relayout is
  **BL-123** (Ben to mock).

## The docs

| Doc | Default view | Sub-levels | Lens on open |
|---|---|---|---|
| [corporation.md](corporation.md) | Standings | Standings / Profiles (leaning one table + Selection drill) | `corporation`, fixed |
| [balance.md](balance.md) | Treasury | Treasury / Cashflow / Assets | **none** (money has no map field) |
| [market.md](market.md) | Prices | Prices / Markets / Trends | `market`, Markets→`scarcity` |
| [construction.md](construction.md) | Build | Build / Manage / Sell Orders | `opportunity`, follows view |
| [economy.md](economy.md) | Sector | Sector / Workforce (Cashflow & Standing cut as dups) | **none** — *not* Industry |
| [selection.md](selection.md) | — (polymorphic by kind) | tile / body / building / corp / market | contextual (currently none) |
| [tile_ledger.md](tile_ledger.md) *(deferred, BL-119)* | Tiles | Tiles / Buildings / (Market?) | `resource`, follows view |

## Cross-doc findings (see [_critic_notes.md](_critic_notes.md))
- **Economy substantially duplicates Balance + Corporation.** Its Cashflow view *is* Balance's Cashflow
  tab; its Standing rank *is* Corporation's Standings. The genuinely aggregate-only residue (Sector +
  Workforce) is thin — so the lead question is whether Economy earns its own rail slot or folds into
  Corporation. See economy.md.
- **The Economy → Industry lens pairing was wrong** and has been corrected: `overlay_mode::industry`
  paints the AI-owned nation substrate, not the player's sector mix.
- Clean, well-surfaced overlaps: Tile Ledger's Market view duplicates the Market ledger (drop it);
  Construction's Sell Orders is a candidate to move to Market.
