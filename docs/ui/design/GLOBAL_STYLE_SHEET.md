# Global Style Sheet

Status: **draft — first surface (tech tree panel) in active exploration, nothing SETTLED yet.**
Owner: Joe. Sprint: none currently — the doc's original "ST1" pointer was to a sprint that
was never opened and was deleted outright in the 2026-08-24 unstarted-plans purge
(`SPRINTS.md`). A fresh sprint id will be authored once a direction is ready to promote,
not before.

Working doc for narrowing Project Io's visual language to one specific style + UX
direction. Rules here are proposals until a render has actually been judged against
them — then mark them **SETTLED** with the render that decided it.

Composite example renders live in `docs/ui/design/renders/`. The tech tree
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

**Genre working definition (2026-08-26, come back to this — a good description):**

> "Industrial schematic HMI" — flat-shaded, monochrome-plus-one-accent, hard-edged,
> diegetic machine terminal. Minimal by subtraction (no glow, no gradient, no chrome)
> rather than by whitespace.

Sits in the flat + angular corner of a two-axis space (glow↔flat edge-lighting,
organic/holographic↔angular/printed). Reference families being gathered under
`renders/refs/` (on-style / off-style). Not the glossy Apple-in-space look and
not holographic-glow FUI — both are the deliberate off-style contrast.

**Process note for next session (Joe, 2026-08-25):** this pass iterated depth-first on one
example almost the whole session. Next time, explore a wider spread of distinct styles early
(several genuinely different directions) before optimising any single one — cheaper to find
the right family first than to perfect a direction that turns out not to be it.

### Menu / shell sub-track (2026-08-30, `renders/main-menu/` + `renders/menu-shell/`)

Breadth-first pass per the process note: four distinct style families, each rendered as a
main-menu and an in-game shell frame. Menu track lives in `renders/main-menu/` (prompts +
screenshot pairing in `PROMPTS.txt`, then `PROMPTS_round2.txt` / `_round3.txt` / `_final.txt`);
shell track in `renders/menu-shell/` (`PROMPTS_round2.txt`). Shared style refs moved to
`renders/refs/ON-STYLE` and `OFF-STYLE`.

| Family | Menu verdict | Shell verdict |
|---|---|---|
| A — Industrial schematic / HMI | Rejected — "too boring and flat". Confirms flat-vector alone reads as empty without a focal element | — |
| B — Nuclear-era control room / atompunk | Rejected as a menu — mood liked, but wrong fit for the game and too busy: the settings were hard to find and select | — |
| C — Deep-space minimalism / observatory | **Works** — "even though it is more minimal". The round-2 base | — |
| D — Orbital-industrial brutalism | Rejected — "completely the wrong style" for the menu | **Strong** — shell-D rendered well enough to reconsider brutalist weight for the shell |

Two carry-forwards:

- **Menu round 2 (`PROMPTS_round2.txt`)** — all four variants (M1–M4) built on C, each adding
  one focal / hero element (A's failure) and keeping the settings cluster simple (B's failure).
  M2 tests borrowing B's warmth, M3 tests borrowing D's weight. **Verdict:** M1 ("Observatory,
  refined") strongest of the four, but not clearly ahead of the round-1 C menu — head-to-head
  still open.
- **Menu round 3 (`PROMPTS_round3.txt`)** — colour iteration on the C/M1 base (rounds 1–2 were
  pale-gold monochrome only): five accent treatments (CV1 gold / CV2 cyan / CV3 amber / CV4
  radium-teal / CV5 bone+ember). Hero element becomes a **tilted-hexagon cluster in the
  bottom-right corner**, drawn in oblique projection at the close-up game camera angle (not
  top-down) — Joe liked the round-2 hex motif but wants it angled. Doubles as the first concrete
  test of the close-up map treatment. **Verdict:** CV5 (bone white + one hot accent) the clear
  favourite; dark background reaffirmed as essential; the round-3 layout reads as "an item
  showcase" not a menu — the negative space needs a backdrop element.
- **Menu final synthesis (`PROMPTS_final.txt`)** — one render: CV5 palette with the ember-red
  swapped for gold/amber `#E8A33D` (CV5's red conflicts the amber+blue house direction); a
  line-art planet + concentric orbital rings/leader-lines backdrop at ~30–40% opacity behind the
  controls (merges mainC's "big circles and lines" with M1's "nice world" — which reads stronger
  alone is still undecided, so the render combines them); the tilted-hex corner retained with one
  gold-lit tile. After this, the menu track pauses to review the round 1–3 spread with Ben.
- **Shell round 2 (`menu-shell/PROMPTS_round2.txt`)** — shell-D's strength may be the **realistic
  map** it happened to show rather than the brutalist chrome, so all three variants shared one
  controlled map (semi-representational, oblique ~30–40°, hexes only on hover/select). S1 hairline
  observatory / S2 observatory + slab weight / S3 brutalist. **Verdict:** S2 the clear winner
  (hairline shell chrome + solid dark slabs on Selection band / comms / minimap + stencil section
  labels + **filled** active nav slot — the filled-on-select glyph is explicitly liked and kept).
- **Shell round 3 (`menu-shell/PROMPTS_round3.txt`)** — S2 base, introducing a **second colour**
  (what broke S3 up nicely). Second colour is cyan `#3FC9E8`, consistent with the SETTLED
  amber+cyan tech-tree pair. Three variants differ only in the second colour's *job*: T1 cyan =
  navigation/structure scaffold; T2 cyan = the open fold-out ledger layer only (an open menu
  reads as a distinct layer, its lit nav slot goes cyan); T3 cyan = the numeric data channel
  (figures, sparklines, gauges). **Verdict:** all three missed — Joe wanted **material
  tonality**, not a highlight accent. What worked in S3 was its two greys (a lighter "concrete"
  frame + darker panels), which gave the shell a body.
- **Shell round 4 (`menu-shell/PROMPTS_round4.txt`)** — S2 base + a two/three-tone matte surface
  system (flat planes, hard 1px edges, no grain). The map canvas stays `#0F0F14` (dark background
  still essential); the shell *furniture* may go lighter. U1 light frame / dark content wells; U2
  dark frame / light raised content panels; U3 three tones (canvas < furniture < open-ledger) so
  an open menu is its own layer. **Verdict:** U2 the winner (U1 close behind).
- **Shell final synthesis (`menu-shell/PROMPTS_final.txt`, v2)** — U2 with exactly one change:
  the map is **no longer boxed** — no gutter, bezel or frame; it fills the whole screen and runs
  *under* the shell bars to all four edges. The bars keep their U2 shapes and stay **anchored
  flush to the screen edges** (not floating inset cards — v1 got this wrong, along with drifting
  the palette). Panels opaque, U2 palette pinned exactly. Whether the map is boxed or full-bleed
  under the bars is `LAYOUT.md`'s to own once a direction is picked.

### Glyph / icon sub-track (2026-08-30, `renders/glyphs/`)

Round 1 (`renders/glyphs/1`): a 24-glyph sheet, dark bg + gold, corner-bracket frames.
**Rejected** — flattened the whole vocabulary to outline (losing `ICONS.md`'s load-bearing
filled-vs-stroke split), invented glyphs that don't exist (gas field, energy node, farmland,
forest), and drifted several shapes off spec.

- **Glyphs round 2** — four directions (G1–G4) that all held the `ICONS.md` placeholder *shapes*
  constant and varied only stroke weight. **Rejected as misframed** — `ICONS.md` documents the
  current `icons.cpp` placeholder primitives, not a target; the renders came out identical.
- **Glyphs round 3 (`renders/glyphs/PROMPTS_round3.txt`)** — from-scratch redesign: the list is
  *meanings*, not shapes to copy. Four design languages (L1 engraved instrument / L2 bold signage /
  L3 faceted constructivist / L4 rounded-geometric modern). **Scope then narrowed** — see round 4.
- **Glyphs round 4 (`renders/glyphs/PROMPTS_round4.txt`)** — scope cut to the **chrome buttons
  only** (~28 marks): the 13 left nav-rail buttons, the 6 map lens buttons, the time-panel
  pause/play mark, and the Selection-band 2×3 action grid (Construct / Manage / Mothball /
  Dismantle / Auto / Swap / Go-to / Reserved — `SELECTION.md`). Everything "installed on a tile"
  (buildings, HQ, units, under-construction)
  becomes **real rendered geometry on the oblique map**, not a glyph; landform marks are terrain
  rendering; corp emblems are a separate brainstorm. Each nav glyph must read both as gold-on-dark
  and as a dark knockout on the settled filled-gold active slot. Same four languages, full
  button set per sheet. **Verdict:** L1–L4 rendered too similar to separate; **L3 (faceted
  constructivist) picked** on one criterion — *minimise curves* (straight lines / hard angles,
  a curve only where unavoidable). **Flag for Ben:** a stylish set may outgrow `ICONS.md`'s
  "hand-drawn ImGui primitives, no atlas" rule and want an SVG path set or SDF atlas — a
  `TECH_FOUNDATIONS`-adjacent call before the `icons.cpp` rebuild.
- **Glyphs round 5 (`renders/glyphs/ROUND5_NOTES.txt`, next session)** — commit to L3, no more
  comparison sheets. One L3 full-set sheet, then per-glyph solo iteration on the ambiguous ones
  (Corporation/Contracts/Budget; Market Ledger vs Market lens; Workforce vs Population lens; the
  three dev-tail marks). Then the track moves to the **minimap** (`renders/minimap/`, empty;
  `MINIMAP.md` has role/chrome but no spec for the visual-interest / traffic Joe wants).

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
