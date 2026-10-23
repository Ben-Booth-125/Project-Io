# Nation overview (slot 1; formerly Corporation) — design Q&A

> **Two surfaces shared the word "Corporation"; one survives, re-subjected to the nation.**
>
> - **Slot 1** is the ledger `src/ui/corporation_dashboard.cpp` draws. It asks **"How well am
>   I doing?"** and holds **one card, Balance**: a verdict line ("±X/qtr, treasury Y") over a
>   two-column chart — the quarter's earnings against its expenses stacked by flow — with the
>   quarter's net beneath it. It is about the **player's own nation's** money, not the rival
>   field (re-subjected, Ben, 2026-10-23: the corporation is retired and a building's money is
>   its nation's treasury's — `docs/politics/NATIONS.md` § What a nation owns). Owned by
>   `BL-248` (corporation dashboard roll-ups) and `BL-691` (corp: how am I doing). See § Slot 1.
> - **Slot 8** (Diplomacy) hosted a **corporation stance ledger** (`src/ui/corporation_panel.cpp`).
>   It **retires** with the corporation (Ben, 2026-10-23) — see § Slot 8, retired.

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `nav rail slot 1` · Source: `src/ui/corporation_dashboard.cpp` · Mock table(s): `cashflow.csv`
> Host: shell fold-out column, ~380px @1720.

## Slot 1 — the Nation overview

> Source: `src/ui/corporation_dashboard.cpp` · Host: shell fold-out column.

**Top question: "How well am I doing?"** — and that question is why the surface holds **one
card**. It carried four roll-ups, and three of them were answered better somewhere else:
Production and Workforce by the Construction ledger's Buildings tab, which holds the estate
*and* the per-building levers; Trade by the Market and Convoys ledgers. What is left that only
this surface can answer is how well the player's nation is doing, and the honest answer to that
is **money** (Ben, 2026-08-29).

**The card is Balance**, and it is a **sub-header inside the ledger**, not a competing surface
name (Ben, 2026-08-29). It shows its content **at rest**: a verdict line (`±X/qtr, treasury Y`)
carrying the full-canvas control alone — there is nothing to expand in place when the card
already draws its chart — then the chart, then the quarter's net, and a force line when the
nation fields units.

**The chart is two columns sharing a baseline.** Earnings on the left — what the nation's
buildings sold, plus the tariff and the tax that return to the treasury (`NATIONS.md` § 2); every
outflow of the nation's quarterly return stacked on the right — **Inputs, Maintenance, Wages,
Force** — each segment its own shade, naming itself and its figure on hover. Force keeps its own
segment rather than folding into Wages for the reason it was given its own bar in the first
place: a force cost buried inside Wages is a term nobody can tune against. The former
**Interest**, **Levies** and **Subsidies** segments have no subject (Ben, 2026-10-23): a nation
never runs into debt, the extraction levy retired, and no nation pays another's industry.

**A zero flow is absent from the stack, not drawn flat.** That rule lives in the **shared
drawer** (`ui::charts::draw_stacked_columns`) rather than in a fork of it — the building card's
Revenue / Expenses graph is the same function over `building_profit`. One chart, one
arithmetic, two surfaces that cannot come to disagree.

**Vertical budget.** The chart takes the host's own remaining height, less the lines that
follow it. That is the point of the card: the resting column used to be four verdict lines over
roughly 700 px of empty column at 1920×1080, and the space is now the answer rather than the
complaint. The column is ~380 px at 1280 and 384 px at 1920 (`shell_column_width`) — the
difference between the two resolutions is all vertical, so no extent here is a literal.

**The overlap with the Budget ledger is explicitly ACCEPTED** (Ben, 2026-08-29): *"This is
called the 'Corporation' ledger, it just has that 'Balance' sub-header. If the data is in both
places, that's fine. We can revisit the 'Budget' ledger later."* That is a deliberate relaxation
of this doc set's one-question-one-home line, taken for these two surfaces and dated — not a
general licence to duplicate, and not an oversight for a later sweep to clean up. With both
surfaces now reading the one treasury, the overlap is wider than when it was accepted.

**Deferred by name, not forgotten:** research points, score, ranking and other heuristics —
*"we will revisit this once core gameplay is established"* (Ben, 2026-08-29). Several have no
store behind them at all: there is no score and no ranking in the model, and research points are
the nation's (`docs/economy/RESEARCH.md` § Who holds technology).

**Toggle semantics.** The rail's slot-1 icon toggles the ledger. The card's one control is the
full-canvas `›`; the takeover's return control folds it back, as does Esc. There are no
sub-view tabs, so the toggle rule's tab clause has no subject here.

## Slot 8 — the corporation stance ledger, retired

**Retired (Ben, 2026-10-23).** It asked *"who am I standing with, and against?"* over the named
field of corporations: Friends / Hostile / Neutral groups, each row a firm's name and disclosed
capital, with an action strip of corp-to-corp stance presses, and it proposed arming the
Corporation lens. With no corporations there is no field to stand toward, no capital to disclose
and no lens to arm (`docs/GLOSSARY.md` § Corporation; `../LENSES.md` § Corporation and Company
lenses — retired).

**Whether slot 8 hosts a nation-grain diplomacy surface is open**, as it was before the
retirement; this doc does not design one. `docs/politics/RELATIONS.md` owns which relations
exist between nations.
