# Global Style Sheet

Status: **draft — first surface (tech tree panel) in active exploration, nothing SETTLED yet.**
Owner: Joe. Sprint: none currently — the doc's original "ST1" pointer was to a sprint that
was never opened and was deleted outright in the 2026-08-24 unstarted-plans purge
(`SPRINTS.md`). A fresh sprint id will be authored once a direction is ready to promote,
not before.

Working doc for narrowing Project Io's visual language to one specific style + UX
direction. Rules here are proposals until a render has actually been judged against
them — then mark them **SETTLED** with the render that decided it.

Composite example renders live in `docs/development/design/renders/`. The tech tree
panel work (2026-08-25) lives in its own `renders/techtree/` subfolder, informally named
(`wide1`...`wide11`, `real1`, `nohover`, `compstate`, `colour2`) rather than the
`iter-NN` convention below — fine for a fast first exploration pass, but a directory
that graduates toward SETTLED should be renamed to the `iter-01.png`-style convention
so later sessions don't have to reconstruct the sequence from filenames alone.

---

## Iterations

| # | Render | What changed | Verdict |
|---|--------|--------------|---------|
| 0 | — | starting point, no render yet | — |

### Tech tree panel sub-track (2026-08-25, `renders/techtree/`)

First real exploration pass, scoped to one surface (the F9 tech tree panel — see
`docs/ui/question_log.json`'s `tech_tree_panel` entry) rather than the whole game. Direction:
industrial-schematic crossed with a supplier's parts catalog, reflecting that the player is a
mercenary company that *procures* tech rather than researching it (`CONCEPT.md` § Modular tech
trees) — nodes as catalogued components, not a mystical RPG skill tree.

| Render | What changed | Verdict |
|---|---|---|
| wide1 | First cyan/dark schematic pass — flat panels, three node states | Technically on-brief but bland: uniform panel silhouette, generic repeated icon, no texture |
| wide2 | Varied panel silhouettes, per-category icon glyphs, one amber secondary accent, wear/barcode texture | Real improvement — "looking better", fixed the sameyness |
| real1 | Actual in-game F9 capture (`ProjectIo --verify tech_tree_panel.lua`), not a generated mock | Reality check: real density is far higher than any mock (dozens of criss-crossing lines, heavy label overlap), all-labels-on-canvas is the live pain point |
| wide3 | Rebuilt for real density: ~25-30 node force-directed graph, icon-only nodes, info moved to a hover-triggered detail card | Correct density model, but "very boring" — uniform node size/shape, no hierarchy |
| wide4 | Added size hierarchy (capstones vs minor nodes) + per-cluster background colour atmosphere | Overshot — too many hues at once ("looks like a fkn christmas tree"), broke the one-accent rule |
| wide5 | Reframed as a supplier-catalog kiosk terminal (bezel/header/footer chrome, scanline/grain texture), capstone-only glyphs, single amber accent restored | "Cooking with gas" — best so far at the time. Chrome noted as mood-reference only: the real panel is a chrome-less full-canvas takeover (`tech_tree_panel.hpp`, BL-310 round 4), so header/nav/footer bars here don't ship literally |
| nohover | Same as wide5 with nothing hovered/selected | Confirmed resting-state legibility holds up; "boring" read expected of a static image and likely solved by real hover/pulse motion, not more renders |
| colour2 | Tested EARNED/LOCKED colour pairings (amber+cyan, amber+magenta, red-orange+cyan) at a locked near-black background (RGB ~15,15,20, matching `app.cpp`'s real `SDL_SetRenderDrawColor(15,15,20,255)`) | **Amber (EARNED) + saturated cyan (LOCKED) + dim grey (NOT-YET-AUTHORED) SETTLED.** Red-orange rejected (reads as an alarm/unauthorised state); magenta rejected (tone mismatch). Dark background confirmed as more faithful to the live game than earlier navy-tinted versions |
| wide6 | Switched to a radial/ring layout (rings = tier/depth from a capstone core) + always-on short node titles | Strong direction — the real panel is already internally called a "radial constellation viewer" (`tech_tree_panel.hpp`), so this isn't just decorative; thematically resonant for a solar-system game. Always-on titles chosen deliberately over hover-only, matching the CURRENT real behaviour (`real1`) rather than redesigning the interaction model unilaterally — that redesign is a question for Ben, not a style call, and stays unflagged for now (Joe will raise it directly) |
| wide7 | Varied node silhouettes (diamond/capsule/hex/triangle, not just hexagon), thinner/more minimal corner-bracket frame | Strong candidate — fixed hexagon fatigue, frame reads more modern/HMI. Personal-taste note: fits the game, not necessarily Joe's own preferred sci-fi aesthetic — both true at once |
| wide8 | Added a terraced/stepped ring bevel (flat steps, highlight/shadow edge-lines, no gradient), hard-edged flat drop shadows on nodes + hover card, an etched "groove" line under each connector | Best depth/glow result of the session — grooves and bevel edges read well. But: background lightened away from the locked dark tone, node shadows too heavy/noisy | 
| wide9 | Attempt to fix wide8: removed node shadows (kept hover-card shadow only), pulled background back toward dark | Overcorrected — terrace bevel nearly disappeared (edges too faint), and introduced connector glitches (dangling/disconnected dots) |
| wide10 | Attempt to fix wide9's terrace + attach labels to nodes | Terrace/connector issues not actually fixed; unrequested boxes/plates rendered behind every title, disliked |
| wide11 | Rebuilt from wide8 (not wide9) as the true base: softened node shadows slightly, kept groove/bevel, titles as plain text with no box | No better than wide8 — session ended here |

**Current best candidate: wide8** (softened per wide11's brief, though that pass didn't
actually improve on it). Not marked SETTLED — Joe flagged wide8's overall aesthetic fits the
game but isn't personally his preferred sci-fi direction; that reservation stands undecided.

**Process note for next session (Joe, 2026-08-25):** this pass iterated depth-first on one
example almost the whole session. Next time, explore a wider spread of distinct styles early
(several genuinely different directions) before optimising any single one — cheaper to find
the right family first than to perfect a direction that turns out not to be it.

## Colour guide

| Role | Colour | Hex (approx) | Notes |
|---|---|---|---|
| Background | Near-black, faint cool tint | `#0F0F14` | SETTLED — matches the real game's clear colour (`app.cpp`, `SDL_SetRenderDrawColor(15,15,20,255)`), confirmed 2026-08-25 against `colour2` |
| Panel / chrome | *Unsettled* | | Kiosk-terminal chrome in wide5+ is mood-reference only per BL-310 round 4 (real panel is chrome-less) |
| Primary text | *Unsettled* | | |
| Secondary / muted text | Dim grey outline | | Used for NOT-YET-AUTHORED nodes — deliberately recessive, not a fourth competing state |
| Accent — EARNED | Amber / gold | | SETTLED 2026-08-25 (`colour2`) — must stay gold; red-orange and magenta both tested and rejected |
| Accent — LOCKED | Saturated cyan | | SETTLED 2026-08-25 (`colour2`) — a full second live colour, not a muted dot (see `nohover` verdict on why one accent alone reads flat) |
| Positive / warning / danger | *Unsettled* | | |

## Global rules

*Unsettled game-wide; the tech tree sub-track above has working answers for its own surface:*

- Corner style (flat vs rounded) — game-wide unsettled; tech tree uses varied node silhouettes deliberately (wide7).
- Button treatment (filled / outline / minimal-transparent) — unsettled.
- Type scale and base text size — unsettled; tech tree confirmed always-on short titles are readable and can drop further in size if needed.
- Iconography weight/line-style — tech tree: flat vector, no gradients, one accent colour pair, category glyphs reserved for large/capstone nodes only (small nodes too small for legible detail).
- Tile/terrain rendering style — unsettled, untouched by this pass.
- Depth/shadow treatment — tech tree: flat hard-edged offset shadows only (no blur, no gradient); ring/tier structures may use a stepped flat bevel (highlight + shadow edge-lines per step) rather than a gradient. Still being tuned (wide8-11) — not SETTLED.

## Open questions

- [ ] What should the map itself look like at a glance — abstract/schematic or textured/representational?
- [ ] Icon style: does it match the UI chrome style, or is it allowed to differ (e.g. more detailed on-canvas glyphs vs flat panel icons)?
- [ ] Does the style vary by canvas (Solar / Circumplanetary / Planetary) or stay uniform?
- [ ] Tech tree: always-on node titles vs. hover-only — designed against the CURRENT real behaviour deliberately; the interaction-model question itself is unraised with Ben as of 2026-08-25 (Joe to raise directly).
- [ ] Tech tree: does the etched connector "groove" survive real crossing density (dozens of lines at many angles), or only read cleanly on an orderly radial layout? Stress-test planned, not yet run.

## Promotion

Once a direction is settled, propagate it into the real UI authority docs
(`docs/ui/*`) and/or a backlog item — this file stays the design-in-progress
record, not a permanent authority.
