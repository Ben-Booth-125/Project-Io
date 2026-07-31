# Selection band exemplar — notes

> **Revision 2 (post-review).** Ben reviewed the original three-level Glance/Read/Study
> stepper below and asked for something simpler: fold up by default, one line only; a single
> up/down chevron; the second (and only other) state is a true full-screen overlay showing
> everything at once — no inline growth in between, Read and Study merged into one combined
> view. That's what's live in `index.html` now — the three-segment `.stepper` control
> described below is gone. A new "Why this chart" affordance was also added: a closed-by-default
> note next to each chart stating the question it answers and why that evidence justifies the
> answer — explicitly NOT a tutorial that tells the player what to ask, only a standing
> self-declaration each chart carries. The mechanic-mapping section below describes the
> superseded three-level design; kept for the record rather than deleted.

`index.html` is a standalone, throwaway HTML/CSS/JS mockup of the tile Selection band with the
BL-214 depth stepper wired up (designed, not yet built in the real game) and the BL-196
recursive drill-down wired up (already shipped mechanic, prototyped here in the band's shape).
It touches nothing under `src/` or `docs/` and has zero network dependencies — open by
double-click.

## What's on screen

A fixed bottom dock (300px tall) over a dark diamond-tiled background that stands in for the
Planetary canvas, so the band reads as an overlay rather than a page. Three columns:

- **Left — hex grid.** An SVG cluster of the selected tile plus its six axial neighbours
  (pointy-top hexes, standard axial offsets). Clicking a neighbour re-selects it as the active
  tile — this is the mechanism that gives the mock its second (and third… seventh) data
  scenario: every hex has its own deterministically-generated resource profile, so re-selecting
  changes every number downstream (glance line, chart bars, threshold caption, drill history).
- **Middle — the metric card.** A pager (`< Iron Ore (1/5) >`) cycling five pages per tile
  (Iron Ore, Copper Ore, Coal fully wired with chart + drill; Habitability and Hazard are
  simpler stat-only pages, per the brief's "one working page is enough" allowance). The
  three-segment stepper sits top-right of this card's own title row, exactly per spec.
- **Right — action grid.** Static 2×3 grid: Construct / Manage / History / Supply plus two
  inert placeholder slots. Hand-drawn-style SVG glyphs, not emoji, to match the game's icon
  vocabulary in spirit.

## Mechanic mapping

- **BL-214 (Depth: Glance/Read/Study)** — the stepper in the metric card's header. Glance shows
  only the pip + verdict line. Read reveals the clustered-column chart (This tile vs. Top 10%,
  full swatch/label/value legend, dotted gridlines). Study reveals the hazard-derivation caption
  plus a dashed threshold tick on the chart. Implemented as CSS Grid's `grid-template-rows:
  0fr → 1fr` trick on wrapper blocks that are *already in the DOM* at Glance — so raising the
  level only grows new content in below what's already showing; nothing already visible moves,
  satisfying the "no reflow, additive only" invariant. Lowering the level animates the same
  blocks shut and going back up re-reveals identical content (no re-fetch/no randomization,
  since it's the same DOM node, not a re-render). Clicking the already-active segment is a
  genuine no-op (state write is skipped before any DOM touch).
- **BL-196 (Subject drill-down)** — clicking either bar in the Read-level chart opens a child
  view in place: the pager header is replaced by a breadcrumb (`Tile [97, 10] › Iron Ore
  history`) with a `‹` back control and a clickable root segment, and the card body swaps to a
  12-point SVG line chart of that tile's history for that resource. The swap plus a small
  slide-in animation reads as "drilled into," not a popup. Back returns to the parent
  Glance/Read/Study view at whatever depth was last set (depth state persists independently of
  drill state, since they're orthogonal axes per the brief).
- **Host axis (BL-… go-to)** — a small `›` icon sits beside the stepper (and again in the
  breadcrumb header when drilled). It's inert by design, exactly as scoped ("doesn't need to do
  anything functional in the mock").

## Judgment calls / extrapolations beyond the pinned spec

- **Stepper default level.** Not specified which level a surface opens at by default; I chose
  **Read** (matches "the compact composed view" being the normal resting state described
  elsewhere in the brief, e.g. the shipped accordion chart). Glance-only on load felt too bare
  for a first impression; Study-by-default would front-load prose the brief says should be
  reserved for deliberate asks.
- **Allowed range [lo, hi].** This surface uses the full `[glance, study]` range, so no segment
  is ever greyed out. I still implemented the disabled/out-of-range styling described in the
  spec (`.stepper button.disabled`) even though nothing currently exercises it, so the control
  is faithful to the general mechanic, not just this one surface.
- **Pager wiring depth.** Per the brief's explicit allowance, only 3 of 5 pages (Iron Ore,
  Copper Ore, Coal) carry full chart + drill data; Habitability and Hazard are simpler
  stat-only pages that still respect the stepper (their Read/Study blocks are just thinner)
  but have no bar chart to drill into, since a single scalar has no natural "this vs. top
  decile" comparison.
- **Drill entry point.** The brief says clicking "either bar" opens the child card; I record
  *which* bar was clicked (`this` vs `top`) and reflect it in a one-line sub-caption under the
  breadcrumb ("Output over the last 12 ticks, from the 'This tile' bar") purely as a nicety —
  the underlying history series is the same regardless of entry bar, since both bars describe
  the same resource/tile pair.
- **Second/third data scenario.** The brief asks for "at least two different mock data
  scenarios, not just one hardcoded screen." Rather than hand-author two fixed screens, I used a
  small seeded-RNG generator (mulberry32 + FNV hash) keyed by tile coordinate + resource name,
  so every one of the 7 hexes × 5 pages is a distinct, stable dataset. This felt more honest to
  Project Io's own "deterministic generation" ethos than literally two static JSON blobs, and
  it's what makes clicking around the hex grid genuinely informative rather than decorative.
- **Background canvas.** Built as a tiled CSS diamond/argyle gradient (the brief explicitly
  allows "a simple static CSS hex or diamond tile pattern"), not a real hex-grid SVG tile,
  to keep it cheap and clearly secondary to the dock in front of it.
