# History Ledger exemplar — notes

> **Revision 2 (post-review).** Same change as exemplar 1: the three-segment Glance/Read/Study
> stepper described below is gone. Every stage card now folds to one verdict line by default;
> its chevron jumps straight to a true full-screen overlay for that stage, combining both
> charts, their legends, and the prose into one view — no inline growth stage in between.
> Fold-up returns to the ledger list. Each chart in the full-screen view also now carries a
> closed-by-default "Why this chart" note — the question it answers and why that evidence
> justifies the answer — per Ben's correction that this should never be a leading tutorial
> ("here's what to ask"), only a standing self-declaration attached to each visual. The
> mechanic-mapping section below describes the superseded three-level design; kept for the
> record rather than deleted.

## What this is

A single-file mock at `index.html` of the History panel Ben's original "wall of text" complaint
was about — a narrow, docked ledger column titled "History — Kepler," an accordion of five stage
cards (Stage I Agrarian Surplus through Stage V Energy Transition), each carrying its own
independent Glance/Read/Study stepper. No build step; open the file directly.

## Mechanic mapping

- **Depth stepper (BL-214)** — the three-segment control in each stage card's title row. Default
  state on load is Glance for every stage (per the brief: the honest default when five stages
  can't all fit at Read in one screenful). Read adds that stage's charts + legends
  (swatch/label/value, never hidden). Study adds the two-paragraph prose and, on Stage III's
  fragmentation chart only, a dashed threshold line + axis caption ("Runaway consolidation ≥
  0.75") and a threshold legend entry. Reveal uses a CSS grid-template-rows 0fr→1fr transition so
  content grows into place rather than popping or reflowing what's already shown. Setting Stage
  II to Study and Stage IV to Glance at the same time (tested in-session) confirms the cards don't
  interfere with each other.
- **Stage V is deliberately capped at `[Glance, Read]`** (no Study segment) to demonstrate the
  greyed-out/disabled segment state the spec calls for — its Study button is inert with a
  "Not available for this stage" tooltip, and its card shows an italic "hasn't resolved to Study
  depth yet" line instead of prose, per the "withheld wholesale, never truncated" rule.
- **Subject drill (BL-196)** — clicking any point on Stage III's fragmentation-index line chart
  opens a child card. It appears as an overlay panel anchored near that chart (not a modal, not a
  new page), with breadcrumb "History › Stage III › Fragmentation detail," a working "‹ Back," and
  content that's genuinely narrower than the parent: a recent-ticks line chart plus the 1-2 events
  that moved fragmentation on the specific tick clicked. Clicking another point inside the child
  chart re-renders the event list in place without re-opening the panel. Clicking "History" or
  "Stage III" in the breadcrumb closes the drill back to the ledger, which keeps its own scroll
  position and every stepper state exactly as it was.
- **Host axis (BL-something background)** — the "[›]" button next to the "History — Kepler" title
  is present but inert (disabled styling, explanatory title text), as instructed.
- **Stage tabs (I–V)** at the top of the ledger are quick-jump navigation only, not part of the
  three named mechanics — they scroll the matching card into view and flash its outline.

## Judgment calls / extrapolations beyond the pinned spec

1. **Single-level drill, not a nested I→II chain.** The spec allows (but doesn't require) the
   child stack to go arbitrarily deep. I implemented one drill level (line chart + event list
   together) rather than a second "tick → event ledger" hop, since the single level already
   answers "why did this number move on this tick" fully. The breadcrumb still carries three
   segments (History / Stage III / Fragmentation detail) so the popping behavior is visible even
   though only the last is a real navigable state in this mock.
2. **Only Stage III's chart is wired as drillable.** The other eight charts across the five stages
   are static at Read/Study to keep the file focused, per the brief's "one scenario done well"
   instruction — they're realistic bar/line charts with real legends, just not click-through.
3. **Per-stage depth ranges beyond Stage V's cap are all `[Glance, Study]`.** The brief only
   specifies Stage V's data as "still resolving" implicitly through its verdict text; I used that
   line as the justification for actually capping its range, since the spec's disabled-segment
   styling needed a real example to be checked rather than just described.
4. **Threshold caption placement.** The spec says "a threshold caption on one of the charts' axes" —
   I put it only on Stage III's line chart (the one with an actual named threshold, 0.75, from the
   Charter Age prose) rather than inventing thresholds for the other four stages' charts.
5. **Drill panel positioning.** "Appears in place (expand/overlay near where the parent lives)" is
   implemented by measuring the clicked chart block's on-screen position and placing the overlay's
   top edge to roughly match it inside the canvas-placeholder area, clamped to stay on-screen. In
   the real app this would anchor against the actual canvas/map surface instead of a placeholder.
6. **Legend numeric formatting.** Values are rounded to 2 decimals via a small `fmt()` helper;
   real game data would presumably come pre-formatted (e.g. "Cr 2.2k" style) — this mock uses raw
   index/count numbers since the History ledger's own units (fragmentation index, adherence score,
   basin counts) aren't currency.

## Files

- `build_UI_example_2/index.html` — the interactive mock (self-contained, no external requests).
- `build_UI_example_2/NOTES.md` — this file.
