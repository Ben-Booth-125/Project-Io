# Project Io — REFINED (active worklist)

## Sprint 17b — the shell stops fighting the map, Batch Delivery

Seven items promoted 2026-08-24 from Ben's UI list (BL-596–BL-604; BL-599 and BL-600 landed the
same day and are not promoted here). All four gating design calls were answered the day they were
raised — the rulings live in each item's `design` prose, not here.

**Why a batch.** Six of the seven touch `body_surface_canvas.cpp`, `overlay.cpp` or both, and five
of the nine filed items are one design: the lens system and the selection grain rebuilt around
**structures** rather than tiles. Run serially they would re-open the same two files, re-bless the
same captures, and settle the same questions differently.

---

### Slices (Wave 1 — one barrier across all four)

The collision map is a **splitting heuristic**, not a gate — worktree isolation absorbs the shared
files. What it was used for is carving four slices that are each a coherent *subject*.

```
A1 the ground      BL-597 blend strength ─┐
                   BL-601 border band     │
A2 the markers     BL-596 over-hex + ring ─┤   all four in parallel,
B  lens roster     BL-604 retire two      ─┤   one step-4 barrier
                   BL-602 chrome home      │
C  the selection   BL-598 one accordion   ─┘
                              │
                    Wave 2 (main session)
                    BL-603 pivot to structure
```

---

### Slice A1 — the ground

**Task A1.1 — dial the province blend back (BL-597).**
`provides:` `k_province_blend_strength` (named constant, `body_surface_canvas.cpp`).
`consumes:` —
The corner blend takes the flat mean of a tile and its same-province corner-sharing neighbours.
Lerp that result back toward the tile's **own** fill by the new strength: `1.0` reproduces today
exactly, `0.0` is no blend. Ship near `0.35`; the final value is Ben's eye in the live app.
*Guard: `1.0` must be byte-identical to the current render, or the lerp is wrong.*

**Task A1.2 — the border band (BL-601).**
`provides:` the inward-falloff nation border pass; the border hit-test band.
`consumes:` —
A nation reads from a band at its boundary falling off inwards, never a territory tint. Two
neighbours meeting must not blend into a third colour. The band carries a real hit-test width —
the drawn stroke is a line, and a line is not clickable at play zoom.

**Task A1.3 — retire the nation lens, route the border (BL-601).**
`provides:` border→nation selection route.
`consumes:` A1.2's hit-test band.
`overlay_mode::country` retires; its content is chrome now. Clicking the band selects the nation
and opens its ledger — the route the lens used to own. **Build it as the general structure-grain
case**, not a nation special case: BL-603 generalises exactly this.

### Slice A2 — the markers

**Task A2.1 — buildings draw over the hex (BL-596).**
`provides:` plateless building marker draw.
`consumes:` —
Remove the marker's own background fill. Terrain, texture and the live lens wash keep showing
under the glyph; legibility rests on the glyph's stroke against a live background.

**Task A2.2 — the segmented ring (BL-596).**
`provides:` `draw_stack_ring` (`icons.cpp`).
`consumes:` A2.1's marker draw.
One arc per building kind around the hex rim, dominant kind's glyph in the centre. Two constraints,
neither optional: it shares the rim with the province edge stroke and the corp-border pass and must
not be confusable with either; and below the coarse-fill threshold it **degrades to the dominant
glyph alone**, never to an empty hex.

### Slice B — the lens roster and its chrome

**Task B.1 — retire two lenses (BL-604).**
`provides:` `overlay_mode` without `opportunity` and `production`.
`consumes:` —
Remove both values, their keys, their rows in LENSES.md's three tables, and their `ACTIONS.json`
entries. The cycle modulus derives from the enum sentinel, so the bar re-numbers itself.
*Decide rather than assume: Production's Circumplanetary per-body badge is a different read — does
it retire with the lens or survive as body-level chrome? And confirm BL-086's ambient opportunity
cue is untouched.*

**Task B.2 — one chrome home (BL-602).**
`provides:` the minimap-header lens region (selector + key).
`consumes:` B.1's reduced roster; `time_panel_rect` / `minimap_rect` (landed, BL-599/600).
Today there are two legend chromes and one is invisible: six of seven gradient-bar keys are drawn
**flush-left of the minimap**, inside the rect the always-open Selection band occupies, and render
as unreadable ghosts (NR-601). Only the Continent key escapes, via a foreground draw. Collapse both
chromes into one region in the minimap header, top right. **This is how six lenses get a readable
key at all** — it is not a tidy.

### Slice C — the selection element

**Task C.1 — one accordion (BL-598).**
`provides:` the merged tile/province Selection element.
`consumes:` —
Replace the province card and the tile card with one element, and the paged centre pane with an
accordion ordered **Buildings → Deposits → Resources → Population → Terrain**. Keep the tile's
available-buildings tab; drop the province buildings tab.

**Task C.2 — dissolve the province rung (BL-598).**
`provides:` repeat-click cycle without a province stop.
`consumes:` C.1's element.
The cycle becomes battle → unit → building → tile. **Consequence to carry, not discover:** no
gesture then selects a province without also selecting a tile, so any code assuming the
province-only tuple (`selected_province` set, `selected_entity` null) must keep working with both
set — check the march-order path first. `verify.select_province` writes exactly that tuple and will
diverge from the gesture; either the hook follows, or the scripts using it test an unreachable
state.

---

### Cross-slice contract (the `consumes` that must match)

| Consumer | Needs | Provider |
|---|---|---|
| A1.3 border route | hit-test band | A1.2 |
| A2.2 ring | plateless marker draw | A2.1 |
| B.2 chrome | reduced `overlay_mode` roster | B.1 |
| B.2 chrome | `time_panel_rect`, `minimap_rect` | landed (BL-599/600) |
| C.2 cycle | merged element | C.1 |
| Wave 2 BL-603 | roster, element, region walks, border-route pattern | B, C, A1 |

Every `consumes` above names a `provides` in the same batch or an already-landed symbol. No
unmatched entry — checked at promotion, re-checked by the review barrier against the merged diff.

### Doc coverage (fanned out across disjoint docs)

| Slice | Docs |
|---|---|
| A1 / A2 | `ui/PLANETARY.md`, `ui/ICONS.md` |
| B | `ui/LENSES.md`, `ui/MINIMAP.md`, `ai/ACTIONS.json` |
| C | `ui/SELECTION.md`, `ui/ledgers/selection.md`, `ui/question_log.json` |
| landed | `ui/LAYOUT.md`, `ui/TIME_CONTROLS.md` (BL-599/600) |

Each changed doc carries a transient `> ⟳` note and earns a standing S-tier review item. The batch
closes with a design-direction Q&A in the DEVLOG — it makes non-trivial calls.

### Close

Step 4 is the barrier: **all** tasks across **all** four slices reach a terminal state before any
item is committed. Then one `verifier-review` pass over the whole integrated set, then commits
one-per-item, then Wave 2, then **one re-bless** of the visual set — held deliberately during the
shell pass (NR-600) so it could happen after these alterations rather than before them.
