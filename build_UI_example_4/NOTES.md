# "What do you want to know?" exemplar — notes

> **Rebuilt 2026-07-31.** This directory was wiped by a concurrent process mid-session; this is
> a faithful reconstruction from the original brief, not a fresh design pass. No original file
> content survived to check against — everything below reflects the reconstruction's own
> judgment calls, made to match the brief as closely as possible.

> **Historical note — this is the discarded exploration, kept for comparison.** Ben later
> corrected the underlying idea this exemplar prototypes: a tutorial or help surface must never
> pre-write questions and answer them for the player, only help them navigate information that's
> already there. What he actually wants elsewhere in the UI is the much narrower **BL-247
> (chart question log, short_name `CHART_QUESTION_LOG`)** — a closed-by-default "Why this
> chart" note beside each chart, stating the question *that chart* answers and why, revealed
> only on request. `build_UI_example_1/index.html` and `build_UI_example_2/index.html` carry a
> working preview of that pattern. This exemplar is the earlier, discarded "pre-written Q&A"
> idea BL-247's design note names directly as its own counter-example — it is being restored
> **as-is**, as the historical record of what was tried and moved away from, not as a
> recommended direction. Nothing about its interaction model has been changed to match the
> correction.

`index.html` is a standalone, throwaway HTML/CSS/JS mockup of a "What do you want to know?"
panel: a short list of first-person player questions that, when clicked, the game answers
directly. It touches nothing under `src/` or `docs/` and has zero network dependencies — open
by double-click.

## What's on screen

A single centred panel titled "What do you want to know?" holding six questions, grouped by the
player-intent categories named in the brief (Operating the economy, Reading trade, Finding
opportunity):

1. Why is my Iron Ore extractor at Kepler losing money?
2. Where should I sell Steel for the best price right now?
3. Which market has the most unmet demand for Silica?
4. How is my corporation's cash position trending?
5. What is driving up my Refined Fuel input costs?
6. Which of my buildings are least efficient right now?

Clicking a question selects it (highlighted row) and folds open a one-line direct-verdict
answer card underneath the list — e.g. "Losing Cr 38.2/tick" in red, or "Best at Meridian
Exchange — Cr 64.20/unit" in green. That folded line is the whole resting state: no chart, no
prose, colour-coded by sign only.

## Mechanic mapping

- **Depth (binary fold).** Every answer defaults folded — headline verdict only. Its one
  chevron jumps straight to a true full-screen overlay (a mode-switch, not an in-place grow)
  holding the full purpose-built breakdown for that specific question: a bar breakdown for the
  Iron Ore question (Revenue / Inputs / Wages / Maintenance / Net, matching the real
  per-building profit readout shape), a market-comparison bar chart for Steel price and Silica
  demand, a 12-tick line chart for the cash trend, a cost-driver bar chart for Refined Fuel, and
  a ranked efficiency list for the buildings question. "Fold up" always returns straight to the
  one-line folded card, regardless of how deep a drill is open — folding is a single jump back
  to the binary "off" state, not a level-by-level unwind.
- **Subject drill (BL-196).** The Iron Ore answer's Wages bar is clickable inside the
  full-screen view; clicking it opens a second-level child view ("Wages detail") showing worker
  count (18/24, 75% staffed) and pay rate (Cr 7.90/tick), consistent with the Wages figure in
  the parent breakdown (18 × 7.90 = Cr 142.2/tick). The breadcrumb reads `Questions › Why is my
  Iron Ore extractor losing money? › Wages detail`; a `‹` back button pops one hop (drill →
  answer), and clicking the `Questions` root segment jumps straight past both hops to the bare
  list, closing the overlay and clearing the selection in one step. Verified interactively
  (headless DOM clicks) that both paths land in the correct state. The other five questions are
  single-level (verdict → one full-screen chart, no further drill) — the brief only requires at
  least one second-level drill, and Iron Ore → Wages is the one it names explicitly.
- **Question list selection.** Re-clicking the currently-selected question closes its answer
  and clears the highlight. Per the standing UI rule ("any control whose active state is
  visible is a toggle"), a highlighted list item controlling a visible panel is a toggle rather
  than a plain switch-target selector — this list isn't a cross-cutting body/market/resource
  combo (the standing rule's stated exemption), it's closer to the nav-rail case the rule names
  directly.
- **Deliberately absent: the "why this chart" log (BL-247).** This exemplar predates that
  correction and is kept as it was — the questions themselves ARE the pre-written Q&A pattern
  BL-247 was raised against, so adding a "why this chart" toggle on top would blur the very
  distinction the two exemplars exist to show side by side.

## Judgment calls

- **No original file to recover.** Nothing survived the wipe; this reconstruction works from
  the brief alone (plus the sibling exemplars 1–3 and BL-247/BL-214's backlog design notes for
  visual-language and mechanic-shape consistency). Any numeric or copy choice below is this
  reconstruction's own, not a recovered original.
- **Answer-card placement.** The brief doesn't fix where the answer surfaces relative to the
  question list. I put it inline, directly below the list, inside the same panel, rather than as
  a separate floating card — keeps the whole "ask → fold-open answer" loop legible as one
  continuous read, and matches "narrow by default" better than a second panel would.
- **Six questions, one two-level example.** The brief's own examples were used near-verbatim for
  wording; data (prices, percentages, worker counts) is hand-authored for internal consistency
  (e.g. Wages detail's 18 workers × Cr 7.90/tick reproduces the parent breakdown's Cr 142.2
  Wages figure exactly) rather than seeded-random, since a discarded exemplar with a small fixed
  question set didn't need the multi-scenario generator pattern used in examples 1 and 3.
- **Steel-price "clears above lowest" line.** Computed properly against the actual minimum
  price in `STEEL_MARKETS` (Cr 8.80 = 64.20 − 55.40) rather than a hardcoded figure, so it can't
  drift out of sync with the chart data above it.
- **Verdict colour convention.** Loss/negative figures (Iron Ore, Refined Fuel driver, least-
  efficient building) are red; the cash trend is green because the mock scenario is drawn
  trending up; Silica unmet demand uses amber (attention/opportunity, not a P&L sign) since an
  unfilled order isn't itself a gain or a loss.

## Verification

No live browser tooling could screenshot this session's environment, so correctness was checked
two ways: `node --check` against the extracted `<script>` block (passes — no syntax errors), and
a full pass of headless DOM interaction in the preview pane (`javascript_tool`) exercising every
question's fold → full-screen transition, the Iron Ore → Wages two-level drill via both the back
button and the breadcrumb root segment, fold-up from a nested drill, and the question-list
toggle-close. All state transitions matched expectations; one real bug (a broken `reduce` in the
Steel-price derivation line that always evaluated to a constant instead of the competitor
markets' minimum price) was found and fixed during this pass.

## Files

- `build_UI_example_4/index.html` — the exemplar, self-contained, no network requests.
- `build_UI_example_4/NOTES.md` — this file.
