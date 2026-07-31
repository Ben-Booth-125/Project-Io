# Corporation dashboard roll-ups — exemplar notes

## What this is

A single self-contained page (`index.html`) mocking a **Corporation dashboard** for the
player's corp, "Genom Systems" — a 2x2 grid of roll-up cards: Production, Trade, Workforce,
Finance. This surface doesn't exist yet in the real game; it's a from-scratch exploration of
how the two already-settled disclosure mechanics (Depth and Subject) would look applied to a
corp-wide summary screen rather than a single dense panel. It follows the same current, binary
fold model as the sibling exemplars in `build_UI_example_1/` and `build_UI_example_2/`.

Two data scenarios are wired via the "Scenario" switch top-right: **Q2 1960 · Steady** (a
healthy quarter) and **Q3 1960 · Labour crisis** (a wage-shortage quarter with a loss-making
Finance card, a deficit Trade balance, and one idled building). Switching scenario re-keys every
headline figure, chart, and colour across all four cards live — including the folded verdicts,
which is why they're worth reading before opening anything.

## Mechanic 1 — Depth: fold ⇄ full-screen (binary, two levels only)

Every card starts folded to **one line**: its title, a colour-keyed verdict figure, an inert
go-to-map icon, and a single chevron — no chart, no prose while folded. Clicking anywhere on the
folded row (or the chevron) is a true mode switch to a **full-screen overlay covering the
viewport**, not an in-place grow. Full-screen shows everything the card has at once: the
verdict, the chart(s) with a full swatch/label/value legend, and whatever else that card
contributes (Workforce's explainer paragraph + table; Finance's derivation line). A single "Fold
up" control in the overlay header returns to the folded line. There is no third, in-between
state — Production and Trade aren't structurally different from Workforce and Finance here; all
four cards' full-screen views are complete on their own.

## Mechanic 2 — Subject (BL-196), child drill with breadcrumb + back

Charts only exist in full-screen (the folded line has none), so a drill is always reached from
inside a card's full-screen view: clicking a bar, or the income/spend line, opens a narrower
child view scoped to one subject, replacing the roll-up body in place. The breadcrumb grows a
third segment — **"Genom Systems › `<Card>` › `<Subject>`"** — with the root segment folding all
the way up, and the card segment (equivalent to the `‹` back button, since every drill in this
mock is one level deep) popping back to that card's roll-up.

Four different subjects, four different shapes of content:

- **Finance** → clicking the headline *or* the income/spend line opens "Income history": the
  same two-series chart re-rendered at 8 quarters instead of 4, plus a one-line note. Finance is
  the one card whose drill isn't per-item — it has no per-building breakdown in this mock — so
  its headline is deliberately the only clickable headline of the four.
- **Workforce** → clicking a building's staffing bar opens that building's staffing-over-time (4
  quarters) plus a status note (comfortably staffed / trending toward the floor / below the 50%
  floor and auto-idled).
- **Production** → clicking a building's output bar opens that building's output-over-time (4
  quarters) plus its recipe and a producing/idle status note.
- **Trade** → clicking a route's volume bar opens that route's volume-over-time (4 quarters)
  plus its partner and a running/suspended status note.

Each is genuinely different data shaped for that subject, not a shared generic detail panel.

## Mechanic 3 — "Why this chart" log

Every one of the eight charts (four roll-up, four drill) carries its own closed-by-default
"? Why this chart" toggle. Opening it reveals exactly two lines — "Answers: `<the question this
chart answers>`" and "Because: `<why this evidence justifies the answer>`" — and nothing more.
It never suggests what to look at next; it only documents, on request, what a chart that already
exists is for.

## Mechanic 4 — Host (`[›]`)

Every card header carries an inert `[›]` "go to on map" button, tooltipped "not wired in this
mock" — present per the convention already established in the sibling exemplars, out of scope
for this exercise.

## Judgment calls / extrapolations beyond the settled spec

- **Scenario switch closes any open drill but leaves full-screen state alone.** If a card is
  open when the scenario changes, its roll-up re-renders in place with the new quarter's
  numbers; a drill one level in is closed rather than silently re-keyed under the player (which
  could read as a jump/glitch) — the settled spec is silent on this, and closing back to the
  card root is the least surprising choice. Which card, if any, is full-screen is a display
  preference, not data-bound, so it survives the switch untouched — matching the brief.
- **Folding up also closes any open drill**, an extension beyond what the brief specifies.
  Since a drill can only be reached through a chart that itself only exists in full-screen,
  re-opening a card later always starts at the roll-up rather than resuming a stale drill — the
  more predictable default for a control that's a real mode switch.
- **Finance's headline is clickable; the other three cards' headlines are not** — a direct
  consequence of Finance's drill being a single fixed subject (its own longer history) rather
  than per-item, so the headline is a sensible second entry point alongside the chart.
  Production/Trade/Workforce all drill per-item, which only a specific chart element can address.
- **Drill-stack depth is one level everywhere**, matching the convention already set by
  `build_UI_example_1/` and `build_UI_example_2/` — a corp dashboard's natural first drill
  target (a building, a route) doesn't have an obvious further subject to recurse into without
  inventing a second fictional layer.
- **All figures, building names, and route names are invented** in the vocabulary given in the
  brief (Kepler/Selene bodies, the resource list, Genom Systems and the rival corp names), but
  the specific numbers, quarter-over-quarter deltas, and the labour-crisis narrative are this
  mock's own scenario-building, not sourced from any real balance data in the repo.

## Files

- `build_UI_example_3/index.html` — the exemplar, self-contained, no network requests.
- `build_UI_example_3/NOTES.md` — this file.
