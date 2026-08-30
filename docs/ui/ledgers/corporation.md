# Corporation — design Q&A

> **Two surfaces share the word, and this doc is about the second.**
>
> - **Slot 1** is the **Corporation ledger** — `src/ui/corporation_dashboard.cpp`. It asks
>   **"How well am I doing?"** and holds **one card, Balance**: a verdict line ("±X/qtr,
>   balance Y") over a two-column chart — the quarter's earnings against its expenses stacked
>   by flow — with the quarter's net beneath it. It is about the **player's own corporation**,
>   not the rival field. Owned by `BL-248` (corporation dashboard roll-ups) and
>   `BL-691` (corp: how am I doing). See § Slot 1 below.
> - **Slot 8** (Diplomacy) hosts the **corporation ledger** this doc describes —
>   `src/ui/corporation_panel.cpp`, `foldout_begin("Corporations")`.
>
> The **numbered sections** below describe the slot-8 ledger; § Slot 1 describes slot 1.
> Its stance half is `BL-449` (stance needs a
> surface) / `BL-475` (corp ledger stance detail); its grouped shape and its column budget are
> `BL-639` (corp panel column budget). **The banded "how do I stand against whom" read is
> retired** (Ben, 2026-08-26) — figures are exact where a corporation files and absent where it
> does not; see `docs/politics/RELATIONS.md` § Standing and `docs/economy/FINANCE.md`
> § Disclosure. The profitability table that owns this surface's former financial half is
> BL-627 (profitability ledger).

> **Working design doc** for the ledger-mockup pass (Power BI). Strawman answers — Ben revises.
> Menu slot: `nav rail slot 8` · Source: `src/ui/corporation_panel.cpp` · Mock table(s): `corporations.csv`, `cashflow.csv`
> Host: shell fold-out column, ~380px @1720.

## Slot 1 — the Corporation ledger

> Source: `src/ui/corporation_dashboard.cpp` · Host: shell fold-out column.

**Top question: "How well am I doing?"** — and that question is why the surface holds **one
card**. It carried four roll-ups, and three of them were answered better somewhere else:
Production and Workforce by the Construction ledger's Buildings tab, which holds the estate
*and* the per-building levers; Trade by the Market and Convoys ledgers. What is left that only
this surface can answer is how well the corporation is doing, and the honest answer to that
today is **money** (Ben, 2026-08-29).

**The card is Balance**, and it is a **sub-header inside the Corporation ledger**, not a
competing surface name — the ledger is called Corporation (Ben, 2026-08-29). It shows its
content **at rest**: a verdict line (`±X/qtr, balance Y`) carrying the full-canvas control
alone — there is nothing to expand in place when the card already draws its chart — then the
chart, then the quarter's net, and a force line when the corp fields units.

**The chart is two columns sharing a baseline.** Earnings on the left; every outflow
`corp_budget::net()` subtracts stacked on the right — **Inputs, Maintenance, Wages, Interest,
Levies, Force** — each segment its own shade, naming itself and its figure on hover. Levies and
Force keep their own segments rather than folding into Maintenance or Wages for the reason they
were given their own bars in the first place: a tax the player cannot see working is
indistinguishable from an unimplemented one, and a force cost buried inside Wages is a term
nobody can tune against. Subsidies join the earnings column only when a nation paid any.

**A zero flow is absent from the stack, not drawn flat**, so the **segment count itself reads
the solvency boundary**: interest is charged only while the balance is negative, so a solvent
corp's stack is one segment shorter than an indebted corp's
(`docs/ui/ledgers/balance.md` § Data sources). That variable segment count is the one real
difference between this grain and the building grain, and it lives in the **shared drawer**
(`ui::charts::draw_stacked_columns`) rather than in a fork of it — the building card's
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
general licence to duplicate, and not an oversight for a later sweep to clean up.

**Deferred by name, not forgotten:** research points, score, ranking, market cap and other
heuristics — *"we will revisit this once core gameplay is established"* (Ben, 2026-08-29).
Several have no store behind them at all: there is no score, no ranking and no market cap in
the model, and research points are a stub (`docs/economy/RESEARCH.md`).

**Toggle semantics.** The rail's slot-1 icon toggles the ledger. The card's one control is the
full-canvas `›`; the takeover's return control folds it back, as does Esc. There are no
sub-view tabs, so the toggle rule's tab clause has no subject here.

## 1. Top question — the one thing this answers at first glance
**"Who am I standing with, and against?"** This is a **diplomacy** read, not a ranking. The surface answers it by *arranging* the field rather than by adding a column to it: the rows are filed under three collapsing groups — **Friends** (`are_friends`), **Hostile** (`is_hostile` in either direction), **Neutral** (the remainder) — each carrying its count in the header. The player's own row is tinted.

**The population is the NAMED field.** Corporations with `is_background == false`, plus the player's own row. A background firm (BL-365, background firms) exists to fill a body's production gap; it is not a party anyone can stand toward, and it is not listed. On a default world that is the difference between eighty-odd rows and a handful, and it is what makes the surface legible at all — the footer names the firms it is hiding so the filter is stated rather than silent.

**Each row carries the firm's NAME and its Capital.** The name is the field a diplomacy surface cannot do without, so it takes every pixel the other does not need. **Capital** follows `FINANCE.md` § Disclosure: exact for a `public` corporation, a dash for a `private` or `closed` one, and **exact always for the player's own row** — a corporation always reads its own books. A dash means *this firm does not file*, never *you have not earned this*, and says so on hover. The bands are retired (Ben, 2026-08-26).

**Reach and Share are NOT on this surface.** Both are `corp_standing` figures and both are public, but neither is a stance fact, and at this column width each of them costs the firm's name. The per-quarter financial read belongs to BL-627 (profitability ledger); the *shape* of a rival belongs to the Selection element. One question, one home — and this one's question is stance.

**An empty group states its emptiness.** Friends is normally empty at campaign start, and a section that simply vanished would read as a missing feature rather than as an answered question. Every group therefore prints either its rows or a sentence saying it has none.

**Stance, per row.** A hostile row says **which direction the hostility runs** — *you declared hostility*, *hostility declared on you*, or *hostile both ways*. Hostility is directed (`RELATIONS.md` § 1 Stance: *a corp can be at war and not know it yet*), so a row that only said "hostile" would have thrown away the half of the fact the player acts on. `is_hostile` is asked once per direction and is never collapsed with the symmetric `are_friends` behind one accessor — that section's third invariant.

**There is no discovery gate.** `RELATIONS.md` § 1 Stance overturns NR-350: *"A declaration against the player is SIGNALLED"* (Ben, 2026-08-22). Every named corporation is listed, grouped and actionable; a declaration against the player groups immediately rather than waiting on contact.

**The transition presses live in the row's action strip.** A disclosure arrow at the left of a row opens a strip beneath it, and the strip draws its presses at the **column's full width** — which is the arrangement that lets them exist at all in ~324 px of content. **Declare Hostile** raises an inline confirm in the same strip (*"Dissolves any friendship. Not unilaterally reversible."* → Declare / Cancel); **Offer Friendship** sends an offer (the strip then reads *Offer sent*) and **Accept Friendship** answers one; **Return to Neutral** withdraws. Hostility is directed and declared, friendship symmetric and mutually chosen (`RELATIONS.md`, Ben 2026-08-17). The player's own row carries no presses — a corp cannot stance itself. **A press that changes stance moves the row between groups**, which is how the surface confirms the press took effect.

**Column budget.** Every extent on this surface is taken from the host's own available width, never from a literal. The host is the shell fold-out column (`shell_column_width` less the icon rail — ~324 px of content at 1720 wide, with a 380 px column floor at 1280), and a fixed-pixel budget against it is what collapsed the firm's name to a single letter and clipped `Declare Hostile` to `De...`.

**Ownership boundary (Corporation vs Economy).** This doc **owns the corp-grain stance read** and the named field it is drawn over. Economy's Corps view claims a similarly-named balance list, but the two must not both answer it: **Economy confines its first glance to whole-economy aggregate** (player totals, sector balance, income/expenditure trend), and the per-firm financial comparison — the read the retired Reach/Share columns were reaching for — belongs to BL-627 (profitability ledger). One question, one home.

## 2. Sub-levels — views & default

**No sub-view tabs.** The ledger is one list, so it has no button-strip nav and nothing to switch between — the rail icon alone opens and closes it, and the toggle rule's tab clause has no subject here.

| Level | Answers (one question) | Content |
|---|---|---|
| **The grouped list** (the whole surface) | Who am I standing with, and against? | Three collapsing groups — Friends / Hostile / Neutral — each with its count and an explicit empty state. Rows carry the firm's name and its Capital; a hostile row also carries its direction line. Footer names how many firms are listed and how many background firms are not. |
| **The row action strip** (per row) | What can I do about this firm? | Opened by the row's disclosure arrow: Declare Hostile (with its inline confirm), Offer / Accept Friendship, Return to Neutral, drawn at the column's full width. One strip open at a time. |
| **The Selection element** (per row, shared) | What is this firm's shape? | A row click routes the corp to the Selection element (`s.selected_entity`, and the dossier field with it), which is where home nation, focus and building count are read. Not duplicated back into this column. |

**A group header is disclosure, not a tab.** Collapsing a group does not close the ledger. It is still a toggle in its own right — the header shows its open state and clicking it again reverses it, which is what the toggle rule asks of any control whose active state is visible.

**A row expander is disclosure too**, and the same reading applies: pressing an open row's arrow closes it.

Cross-cutting selectors (NOT views, exempt from the toggle rule): **none needed** — the named corp set is small and fully enumerable. A future *sort selector* would be a selector, not a view.

**Asset-count ownership (three-way).** The per-corp building count (`assets.size()`) is read through the **Selection element**, and Corporation's row-click is the route to it. That is distinct from Balance's **Assets** figure (the player corp's own count, income and cargo value) and from Economy's **Sector** composition (the *aggregate* building composition across the whole field). Per-corp on selection, player-self in Balance, whole-field composition in Economy — no three-way overlap.

## 3. Lens on open
Arms **`overlay_mode::corporation`** (per-corp tile tint, player-corp border) — the candidate lens for this slot, and consistent with Ben's "opening a menu usually should arm a lens." Rationale: the Corporation ledger is *about the field of players*, and the corporation lens paints exactly that field onto the canvas (whose buildings are whose), so the ledger and the map answer the same question in two registers. **Fixed on open**, not sub-view-following — the ledger has no sub-views to follow. `ui_state.overlay` defaults to `none` at campaign start, so arming here would be an explicit set. This is a proposal; the panel arms no lens.

## 4. Data sources
Every field the surface needs exists on `corporation_component` (`src/world/components.hpp`), `corp_standing`, or the stance tables:
- **name / is_player / is_background** — `corporation_component`. `is_background` is the population filter; `is_player` and `is_background` are never both true, so the player's row falls out of the same predicate.
- **capital_balance / capital_disclosed** — `corp_standing`, computed by `standing.hpp`; `capital_disclosed` already carries both halves of the rule (the firm files, **or** it is the observer's own corp).
- **stance** — `is_hostile` (directed, asked once per direction) and `are_friends` (symmetric) over the stance tables (`stance.hpp`); the presses issue the four stance verbs through `corp_command`.
- **home_nation / focus / assets** — read through the **Selection element**, not here.
- **reach_bodies / market_share** — still computed on `corp_standing` and still available; this surface simply does not print them.

Mock-data notes:
- **`starting_capital` is a placeholder** (0 in mock `corporations.csv`, and `since_start` is just a dup of `balance`). Any "growth since start" framing needs the exporter to emit a true opening balance.
- **No time-series per rival.** `player_timeseries.csv` covers only the player corp; there is no per-corp balance history, so a "who is climbing over time" sparkline has no feed.

**Row order.** Within a group, rows walk `corp_standing`'s sorted `entity_id` order. The corp map is unordered, so the sorted walk is the only order that is the same on every run.

## 5. Close / toggle semantics
The nav-rail slot-8 icon toggles the ledger open/closed, and it is the only control that closes it: there are no sub-view tabs, so the toggle rule's tab clause has no subject here. A row click selects a corp and routes it to the Selection element. Two controls on this surface are **disclosure toggles** — the group header and the row's expander arrow — and each reverses on a second press without closing the ledger. The stance presses themselves are actions, never toggles. Opening the ledger arms the corporation lens (proposed); closing it does **not** auto-disarm the lens (lens state is canvas-owned and persists).

## Open questions for Ben
- **The group words.** The groups read **Friends / Hostile / Neutral**, taken from `GLOSSARY.md` (*stance is hostility and friendship*) and `RELATIONS.md` § 1. "Allies" and "Rival" were considered and rejected — alliance is not a modelled relation, and *rival* already means *any non-player corporation* in this codebase, most of which are neutral. Confirm, or name them yourself.
- **Should a group be sorted by anything but entity id?** Capital descending would make the Neutral group a ranking again, which is the read this surface deliberately gave up. Fixed id order is the current answer.
- **Does an incoming friendship offer deserve its own group**, or is *Offer sent* / *Accept Friendship* inside the row's strip enough? An offer is explicitly **not** a stance (`stance.hpp` invariant 1), so it has no group of its own today.
- **Lens: fixed or follow?** I set it **fixed = corporation** on open. Confirm you don't want it disarmed when the ledger closes (I left the lens persisting).
- **Nation-grain diplomacy.** Slot 8 is called Diplomacy and this surface answers only the corp grain. When nation stance, treaties and lobbying need a surface, do they join this ledger as a second group of sections, or take a slot of their own?
- **Does this table survive BL-627 (profitability ledger), or does the profitability table absorb it?** With the bands retired, the financial half of Standings is a thinner version of what the profitability ledger prints per quarter. The stance column is the part with no other home short of `BL-475` (corp ledger stance detail).
