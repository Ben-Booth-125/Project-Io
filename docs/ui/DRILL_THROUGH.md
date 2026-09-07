# Project Io — Drill-through

> **Settles:** which three states a dense surface discloses through and which two
> controls reach them · what a full-canvas takeover occupies and what survives it ·
> which axis a gesture belongs to when depth, subject and host are confused · what
> each ladder surface shows at each state · what two edits add a surface.
> **Not here:** where the regions themselves sit (LAYOUT) · which container kinds
> text is fitted to (LAYOUT) · which ledgers the rail holds (MENU) · what a click
> selects (SELECTION).
> **Confused with:** LAYOUT.md, MENU.md, SELECTION.md.

**One disclosure idiom, obeyed by every dense surface** (BL-214, drill-through; BL-265, the
in-place rung). Without it each surface invents its own way of showing more — a collapsing
header in one ledger, a pager in another, a permanent horizontal scroll in a table, nothing
at all in the wizard. Drill-through is the single idiom, and it has **three** states reached
by **two** controls:

| State | What it is | Control |
|---|---|---|
| **Folded** | A verdict line — a figure, a label, a glyph. No sentences. | — (the resting state) |
| **Expanded in place** | The row grows where it sits: one graph, or an accordion of graphs for a more complex menu. The rest of the screen keeps working. | `⌄` (becomes `⌃`, and collapses on click) |
| **Full canvas** | A takeover of the **canvas region** showing everything the surface has at once — every item of an accordion, open, top to bottom, scrolled. | `›` (leave with `‹`, or Esc) |

A two-state model — folded, or a full-screen overlay, nothing between — is too big a step
for *"I want to see this one chart properly"* (Ben, 2026-08-02, after using it); a
three-level Glance / Read / Study stepper with one cycling control is ruled out too (Ben,
2026-07-31). These are two distinct affordances with two distinct meanings, which is what
makes them legible where one cycling control is not.

## Three axes, never conflated

The competing idioms were never several answers to one question — they were three
different questions wearing one costume. Naming the three is most of the fix:

| Axis | Question | Gesture | State |
|---|---|---|---|
| **Depth** | *How much of this subject do I want?* | `⌄` in place, `›` full canvas | `ui_state::expanded` (the takeover target **and** the in-place set) |
| **Subject** (BL-196, resource drill-down) | *What am I looking at?* | click the element; breadcrumb + back | `ui_state::card_stack`, `corp_rollup_drill` |
| **Host** (SELECTION.md) | *Where does this properly live?* | `[>]` go-to | `ui::focus_on_entity` |

Subject is therefore the **sibling axis**, not a competitor and not folded in.

## The geometry: a takeover takes the CANVAS, not the window

`fold_overlay_begin` is sized to **`ui::canvas_rect()`** ([LAYOUT.md](LAYOUT.md) § Canvas area
— centre), not to the display (Ben: *"full
screen should actually just take the place of a canvas, rather than the entire view. I would
prefer to see consistency in the UI really."*). The nav rail, identity tile, header, clock,
comms dock, Selection band and minimap all survive a takeover, and the player never loses
their selection to open a chart.

The argument is consistency and it is the good one: the canvas region is already the app's
main stage, and the zoom ladder already swaps what occupies it. A ledger going full canvas
is therefore the **same kind of event as descending the ladder**, using the same rectangle,
rather than a second full-window mode with its own rules.

**One exception, and it is real rather than a special case:** the New World wizard's chain
stages (`generation_stage`). The wizard runs on its own screen *before* the shell exists, so
there is no canvas to take and no chrome to preserve; its takeover stays window-wide. That
is what "take the main stage" means on that screen. `takeover_rect` in
`src/ui/detail_level.cpp` is the single place this is decided.

## The invariants

1. **One TAKEOVER at a time — and only the takeover is single-target.** Two rules, not one:
   - The **takeover** is a single `(surface, key)` target (`fold_state::surface`/`key`): it
     owns one rectangle, so a second has nowhere to go. Opening a second replaces the first.
   - **In-place** expansion is **not** single-target — it is a **set**
     (`fold_state::in_place`). An accordion of graphs is by definition several rows open at
     once; a single-target in-place expander would close the first graph when you opened the
     second, which is the opposite of what an accordion is.
2. **Inside a takeover the in-place set is ignored.** Every item renders open, top to
   bottom, in a scroll region — *"anything full screen deserves its full space"* only holds
   if the takeover does not depend on which rows the player happened to unfold first.
   Nothing writes the set on the takeover's behalf, so returning via `‹` restores the folded
   view **exactly**: the takeover is non-destructive.
3. **A takeover and the fold-out ledger column coexist by design, not by tolerance.**
   `foldout_column_rect()` is entirely left of `canvas_rect()`, so the row that opened a
   takeover **stays visible** in the left column while the canvas changes. That reads
   correctly for Ben's own stated reason — it is the same event as clicking a body on the
   canvas: the source stays put, the stage changes. Do not "fix" it by dimming the ledger.
4. **The level is not remembered.** A takeover is a transient mode and the in-place set is a
   reading preference — both are view state, never serialised.
5. **A fixed-rect container does not fold to one line, and has no `⌄`.** The Selection band's
   rect is a *derived* 260 px (`selection_band_height`) that cannot shrink, so folding its
   metric card would spend ~220 px on emptiness. Fixed-rect surfaces therefore rest
   **expanded in place already** (Ben, 2026-08-01, asked with the measurements) — there is no
   in-place state left to reach, so they take the `›` control alone. The same holds for a
   single-block view that already shows its content in the column (History Story).
   Folded-by-default governs **scrolling** containers, where a fold buys real room back.
6. **Drill-through adds no eleventh container.** The ten kinds in [LAYOUT.md](LAYOUT.md)
   § Container vocabulary say *how text fits*; this says *how much of it there is*.
   The takeover is that vocabulary's container **1**'s policy (wrap, vertical scroll) at
   canvas size.

## The controls, and where they sit

**The rule, in one line: controls that operate on an item in a list are right-gutter-aligned;
the control that leaves a view is top-left.** Any disclosure surface is checked against that
sentence.

A **foldable row**:

```
Expandable item          verdict                       ⌄  ›
```

Both controls sit in a **fixed right gutter** (`ui::disclosure_controls`,
`ui::disclosure_gutter_width`) — not immediately after the label, because a gutter is what
makes them line up in one vertical column across every row. **A title's length must never
move its controls** (Ben: *"it is disorienting when a button is not in the same column, but
does the same job."*). `generation_charts.cpp`, `corporation_dashboard.cpp`,
`tile_inspector.cpp` and `selection_panel.cpp` all use the same column. A row that offers
only `›` leaves the gutter's **left slot empty** rather than letting `›` slide sideways, so
the full-canvas column never moves. A row's verdict is drawn through `ui::gutter_text`, which
clips it short of the gutter so a long verdict cannot run underneath the controls.

`⌄` **is** a toggle — re-clicking while open collapses — so the standing toggle rule applies
with no exemption owed.

The **full-canvas view**:

```
‹ Example title
  <everything, scrolled>
```

One control, **top left**, immediately before the title: `‹`, which returns. Nothing sits in
a right gutter here — there is no list of rows to align with, and a return affordance
belongs where a reader starts, not where they finish. The two placements answer two
different questions: in a list the controls act *on a row*; in a takeover the control acts
on the *whole view* and means "back", and back is top-left everywhere in software, including
this app's own zoom ladder.

**All four glyphs are DRAWN, never typed.** `⌄ ⌃ ‹ ›` are notation for the design; none of
those codepoints are in the font atlas, and a string literal renders `?`. `draw_caret` and
`draw_open_arrow` in `src/ui/detail_level.cpp` build them through `ImDrawList`. Do not
introduce them as string literals.

**Esc** closes the takeover, one rung **below** the subject drills: exit-confirm → system
menu → pop `card_stack` → pop `corp_rollup_drill` → **close the takeover** → open menu. A
single press never both unwinds a drill and closes the view hosting it. Esc **does not**
collapse in-place expansions: that is a reading preference, the kind of state Esc has never
collapsed, and putting it on the ladder would make Esc unpredictable (sometimes leaving a
view, sometimes re-folding four graphs you deliberately opened). A takeover with no keyboard
exit is a defect, not a principle.

The takeover's **entry/exit transition** is deliberately unsettled and instant — a feel
question that wants the live app, not a paragraph.

## No chart question log

There is no closed-by-default "Why this chart" toggle (`ui::why_note`) revealing an
*Answers:* / *Because:* pair on a chart, and none is to be built. Ben removed it under
**NR-018**; `src/ui/detail_level.cpp` carries the standing note — *"Do not reinstate a draw
path here without reopening NR-018."* There are no `why_note` symbols in `src/`.

The **derivation caption** — which answers how a number was computed — is a different thing
and stands.

## The surfaces, and the extension recipe

On the ladder, with what each one's takeover shows:

| Surface | `⌄` in place | `›` full canvas |
|---|---|---|
| Selection band's metric card | — (rests expanded, invariant 5) | that metric's chart, on the canvas |
| History ledger — Story | — (already shown in the column) | the whole biography, on the canvas |
| History ledger — Chain (per stage) | that stage's title, explainer and charts | the **whole round** — every stage open, scrolled |
| New World wizard — chain stages | that stage's title, explainer and charts | the **whole round**, scrolled (window-wide; no shell yet) |
| Corporation dashboard — 4 roll-ups | that card's chart or rows | **all four** roll-ups, headed and scrolled |

The three accordions — History Chain, wizard stages, corp roll-ups — show everything in a
takeover: a surface that has taken the largest rectangle available has no excuse for showing
a subset. The History ledger's **Ages** tab takes no disclosure control at all — its map sizes
itself to whatever column it is given.

Adding surface #N is **two edits**: an enumerator in `detail_surface`, and a
`disclosure_controls` + `fold_overlay_begin` pair at the call site. Nothing else — no new
control, no new state, no new container kind. A surface with many foldable blocks needs
**one** enumerator, not one per block: the `key` disambiguates instances (a chain stage, a
roll-up card). Removing a surface is the same two edits in reverse.
