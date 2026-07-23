# Project Io — Map Lenses

The **lens system** is the set of data overlays the player toggles over the
canvases from the lens mode bar on the **minimap**
([`overlay.cpp`](../../src/ui/overlay.cpp), `draw_overlay_controls`; see
[MINIMAP.md](MINIMAP.md)). One lens is active at a time — the active mode is a
single `overlay_mode` enum value ([`ui_state.hpp`](../../src/ui/ui_state.hpp));
`overlay_mode::none` is the plain canvas. Each on-screen lens has one distinct
vector glyph in the bar (see [ICONS.md](ICONS.md) § Map-lens glyphs) and re-skins
or annotates the canvas when active.

This document is the *design* authority for what each lens shows, which rung of
the [canvas zoom ladder](CANVASES.md) it applies to, and how its legend reads.
Glyph shapes live in [ICONS.md](ICONS.md); identity colours live in
[`presentation.hpp`](../../src/ui/presentation.hpp) (the `palette` namespace).

> **Status.** All lenses are built except where a section names a gating
> dependency. The **Lens & Legibility** batch (BL-013/052/019/017/009/018) settled
> the curated single-select strip, renamed Faction → **Country**, reworked the
> **Resource** lens to a flat contiguous-deposit fill, reworked **Scarcity** to a
> per-market shortfall field, and added the **Opportunity** (net-margin) and
> **Production** (intensity) lenses. The 2026-07-01 visibility-pass cluster added
> the **Industry** lens (BL-084, per-tile nation-substrate throughput field), and
> **re-keyed Population** (BL-069) from raw habitability to workforce efficiency.
> **BL-093** (2026-07-04) relocated the lens bar from the bottom-left strip onto
> the **minimap** (a single row of 7 glyphs fits its width) and trimmed the
> on-screen set to **Corp, Country, Resource, Market, Population, Opportunity,
> Production** — **Scarcity** and **Industry** dropped off the visible bar to
> **keyboard-cycle only**, joining Supply (all three `overlay_mode` values still
> exist and still render when selected by keyboard; they are just not glyphed on
> the bar). The resource/good selector (shared by Resource/Market/Scarcity) is now
> a **popup** opened from the minimap bar — the old 140px inline combo does not
> fit there. The **Market** lens's per-catchment surface still waits on
> multi-market seeding (BL-036); **Supply** waits on Layer-5 route geometry.
> Identity colours live in `presentation.hpp`; the corporation-identity helper is
> `palette::corp_colour` (renamed from `faction_colour`, BL-052).

---

## Rung applicability

Which lenses are meaningful on each rung of the ladder. "—" = no representation
intended; "✓" = built and active; "(later)" = a representation is specified here
but its build is gated on a dependency (named in the lens section).

The lens bar — now on the **minimap** (BL-093, relocated from the bottom-left
strip; see [MINIMAP.md](MINIMAP.md)) — presents a **curated subset** in this order
(BL-013, extended BL-084, trimmed BL-093): **Corporation → Country → Resource →
Market → Population → Opportunity → Production**. **Scarcity** and **Industry**
are off the bar, reached by **keyboard lens-cycle only** — joining **Supply**,
which remains off-bar pending Layer-5 route geometry. The campaign
opens on **no lens** (`overlay_mode::none`, the plain canvas) — a click only updates
the Selection element and never re-skins the canvas, so the canvas starts unskinned
and the player picks a lens deliberately (reverses BL-013's Corporation-default,
2026-06-30). The bar is single-select with a null state (re-selecting the active
lens clears to plain terrain).

The per-rung representation of every lens, on-bar or keyboard-only (the BL-012 sweep). "—" = no
representation intended; "✓" = built and active; "(later)" = specified but gated on
a dependency (named in the lens section).

| Lens | Solar | Circumplanetary | Planetary |
|---|---|---|---|
| Supply *(keyboard-cycle only)* | (later) inter-body route lines | (later) per-body throughput badge | (later) per-tile route + stockpile |
| **Corporation** | — | — | **✓ tile tint + player border** |
| **Country** | — | — | **✓ tile tint + owner borders** |
| **Resource** | — | — | **✓ contiguous-deposit flat fill + key** |
| **Market** | — | ✓ per-body price strip | ✓ per-body price wash |
| **Population** | — | — | **✓ workforce-efficiency tint + gradient key (BL-069)** |
| **Opportunity** | — | — | **✓ per-catchment unmet-demand tint + key (BL-112)** |
| **Production** | — | (later) per-body output-throughput badge | **✓ production-intensity tint + key** |
| **Scarcity** *(keyboard-cycle only)* | — | (later) per-body shortfall badge | **✓ per-market shortfall blocks + key** |
| **Industry** *(keyboard-cycle only)* | — | — | **✓ substrate-throughput amber tint + key** |

**BL-012 per-lens rung notes.** Corporation, Country, Resource, Population,
Opportunity, and Industry are **Planetary-only** — their unit of meaning (a tile, a
building, a deposit, a margin, a substrate-throughput reading) is sub-body and has
no coherent inter-body surface, and nations are sub-body political units. Market
and Supply are the genuinely multi-rung lenses (prices per body-market; logistics
span the ladder). Production and Scarcity are Planetary today but each has a
natural **Circumplanetary per-body badge** owed (total output / aggregate shortfall
for the anchor body) — additive passes guarded behind the same `overlay_mode`, not
changing the Planetary behaviour. None propagate to the Solar rung in the
prototype.

**Resource** is **built** (2026-06-17): unlike Supply and Market it had **no data
dependency** (tile `resource_deposit` is already generated), so it landed directly as a
Planetary render pass. See its section for the full spec.

Interaction notes shared by all lenses: lenses are **Planetary-first** in this
prototype, single-select (one `overlay_mode` at a time). Since BL-093 (2026-07-04)
the mode bar itself lives on the **minimap** rather than a canvas-bottom strip (see
[MINIMAP.md](MINIMAP.md)) — the lens still re-skins whichever Planetary canvas is
open; only the *control's* location moved. Selecting the already-active lens
clears back to `overlay_mode::none` (`toggle_overlay`). Coarser
Solar/Circumplanetary representations, where specified, are additive render
passes guarded behind the same `overlay_mode` — they do not change the Planetary
behaviour.

---

## Corporation lens

**Intent.** Read the map as a *corporate landscape*: where corporations operate,
who owns what, and how the player's footprint sits against rivals. National
territory is the Faction lens's job — this lens is about **corporate-owned tiles**
first; nation context is deliberately absent.

**Ownership definition (settled).** A *corporate-owned tile* is any tile on which
a corporation holds a building. The mapping is derived at draw time from
`w.corporations[].assets` → `building_component.tile` (a building id resolves to
its tile). **There is no influence radius and no nation-domination heuristic in
this iteration** — only the literal building tile counts as owned. A tile with no
corporate building is *unowned*.

**Rung.** Planetary only. No Solar or Circumplanetary representation — the render
pass is guarded entirely behind `overlay_mode::corporation` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp) and changes
nothing on the other two canvases.

**Colour.**
- **Owned tiles** are tinted their owning corporation's identity colour (a direct
  replacement of the terrain hue, matching the Faction lens's tint convention).
- The **player corporation** (`w.player_entity`) uses
  `presentation::faction_colour(0)` for its tile fill and additionally gets a thin
  border in `palette::selection` (white) so the player's holdings contrast against
  any rival fill colour at a glance.
- **Rival corporations** use the per-corp hashed faction slot already used for the
  building markers (a multiplicative hash kept off slot 0 so a rival never
  collides with the player's colour).
- **Unowned tiles** render in their plain terrain colour with **no tint** — there
  is no nation underlay in this lens.

**Glyph.** A filled square with a centred inner dot — a "seal" silhouette
(`icons::corporation`; see [ICONS.md](ICONS.md)). Distinct from the
extraction-site filled diamond, the processing-facility plain filled square, and
the port/unit filled triangle.

**Legend.** As with the other lenses today, the active lens is named only by the
strip glyph highlight and its hover tooltip (`overlay_mode_name` →
"Corporation ownership"); no on-canvas colour key yet. A per-corp colour key is a
follow-up shared with the Faction lens.

**Cross-note (BL-085).** The player's white outline described above is drawn as part of the
Corporation lens's own fill/border pass, but the *general* player-identity accent — a subtle wash
on the player's tiles at the plain default, plus an outline drawn under **every** lens — is
separate, always-on chrome (`is_player_tile`, `corp_identity(w.player_entity)` in
`body_surface_canvas.cpp`) drawn once regardless of which lens (if any) is active; it is not a
second player outline layered on top of the Corporation lens's own. The Corporation lens does not
add a second player outline — it *extends the same identity language to rivals*, giving every
corporation (not just the player) a readable tile tint. See also the BL-085 home ring / HQ star,
drawn only on the player's home body, which is a further, distinct layer of the same identity
chrome.

**Corporate reach (BL-182, visual slice — shipped; foundation BL-201).** Beyond tinting *held
tiles*, the lens draws each **rival** corporation's **HQ-projected border**: a **reach ring**
centred on that corp's HQ plus an `hq` star, in the corp's identity colour. As of BL-201 the ring
reads the corp's **persisted** seat + range (`corporation_component::hq_building` /
`influence_range`, designated at generation — CORPORATION_GENERATION.md § Pass 3b), not a
render-time recompute; the ring radius is `influence_range · hex_size · zoom`. The player's border
and every rival's are now drawn through **one shared `draw_corp_border` path**, each on that corp's
**home body** (the single-home model; branch offices on other bodies are deferred). This extends
the identity language of the always-on player-only home ring/HQ star (BL-085) to rivals, so
corporations read as having **borders too** — the corporation-side counterpart to the Country
lens's national borders. The player's own border stays always-on; the rival layer shows under this
lens (no double-draw of the player). This layer is **render-only chrome** — it gates nothing. The
full *gameplay* mechanic (range that gates operations, the national origin gate, multi-HQ building
via advancement, the tall/wide axis, law/tech levers) stays deferred in
`docs/development/backlog.json` (BL-182). See `scripts/verify/corporate_reach.lua`.

---

## Country lens

*(Renamed from "Faction" — BL-052. The lens shows **national** territory, so its
name is Country; `overlay_mode::country`, glyph `icons::country`. `"faction"`
remains a legacy alias in the verify-script name parser.)*

**Intent.** Read the map as a *political landscape*: which nation holds which
tile, and where the borders between them fall. This is the national counterpart
to the Corporation lens — territory, not corporate holdings.

**Ownership definition.** A tile's owner is its nation entity id, derived from the
`world` tile→nation ownership map (stored off-tile so the nation and tile-tuning
groups stay disjoint; see [NATION_GENERATION.md](../generation/NATION_GENERATION.md)).
Unclaimed tiles have no owner.

**Rung.** Planetary only. No Solar or Circumplanetary representation is intended:
nations are sub-body political units and have no coherent expression on the
inter-body rungs. The render pass is guarded behind `overlay_mode::country` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp).

**Colour.**
- **Claimed tiles** are tinted their owning nation's identity colour
  (`palette::nation_colour`, keyed by nation entity id), a direct replacement of
  the terrain hue.
- **Borders:** a dark stroke is drawn on every hex edge shared with a neighbour of
  a *different* owner — including the claimed/unclaimed boundary — so contiguous
  territory reads as a filled region with a hard outline.
- **Unclaimed tiles** keep their plain terrain hue with no tint.

**Glyph.** A downward-pointing shield silhouette with a dark outline
(`icons::country`). Distinct from the corporation seal-square and the resource
strata.

**Legend.** Named by the strip glyph highlight and its hover tooltip
(`overlay_mode_name` → "Countries"); no on-canvas colour key yet. A
per-nation colour key is a follow-up shared with the Corporation lens (both want
the same identity-swatch list).

**Interaction notes.** Planetary-only, single-select. Borders are recomputed at
draw time from the neighbour ownership comparison; no border data is persisted.

## Supply lens *(target spec — gated on Layer 5 route geometry)*

**Intent.** Read the map as a *logistics network*: where goods move, along which
routes, and where throughput concentrates. The economic counterpart to Faction's
political read.

**Rung applicability.** Supply is the one lens with a meaningful representation on
*every* rung, because logistics span the zoom ladder:
- **Solar** — **inter-body route lines** between bodies that trade, weighted by
  throughput (line thickness/opacity).
- **Circumplanetary** — a **per-body throughput badge** summarising goods in/out
  for the focused body.
- **Planetary** — **per-tile** route segments and the producing/consuming tile's
  stockpile state.

**Colour.** Route lines and badges use a single neutral logistics hue
(`palette` entry, not per-faction) modulated by throughput; the lens is about flow
volume, not ownership. Tiles are not tinted — supply annotates, it does not
re-skin terrain.

**Glyph.** Two parallel horizontal lines — a route/convoy shorthand
(`icons::supply`). Distinct from the market vertical bars.

**Legend.** Strip glyph highlight + tooltip ("Supply routes"). A throughput
scale-key (line weight → goods/tick) is part of the build when routes render.

**Interaction notes.** Propagates across all three rungs (the exception to the
Planetary-first default). **Gated on Layer 5**: no convoy/route geometry is
generated yet, so nothing renders today. No interim Planetary-only stub is built —
the lens waits for its data.

## Market lens *(built 2026-06-16 — price resolution landed in v0.0.4)*

**Intent.** Read the map as a *price surface*: where a good is dear or cheap, and
how scarcity varies across markets. The complement to Supply — Supply shows flow,
Market shows the prices that drive it.

**Rung applicability.**
- **Solar** — none; prices are per-body-market and have no inter-body surface.
- **Circumplanetary** — a **per-body price strip** for the anchor body's market (a
  compact good→price list, the selected good highlighted).
- **Planetary** — a **body-wide price wash** for the player-selected good (dear =
  warm, cheap = cool). Markets are per-**body** (`market_component`), not per-tile, so
  the tint is uniform across the body — a property of the body's market, not of each
  tile.

**Colour.** A diverging warm↔cool gradient keyed to **price relative to the good's
own base (floor) price** (`price / base_price`); neutral mid-tone at the floor ratio
1.0, trending cool below and warm above, across the market's `[0.25×, 4×]` clamp band.
*(Refinement from the original "relative to the body mean": a basket mean across
resources of very different base prices is not meaningful — the per-good floor ratio is
the scarcity signal.)* As with Resource, the player picks which good the surface shows
(a good selector shared with the Resource lens's resource picker — one `lens_resource`).

**Glyph.** Three ascending bars — a price-chart silhouette (`icons::market`).
Distinct from the supply horizontal pair and the resource horizontal strata
(market bars are vertical and outlined).

**Legend.** Strip glyph highlight + tooltip ("Market prices") plus a diverging
gradient key (cheap↔dear) and the selected good's name when the per-tile surface
is active.

**Interaction notes.** Single-select. Price resolution landed in v0.0.4, so the lens
is built: the Circumplanetary strip and the Planetary wash share the resolved
`market_component.price`.

**Implemented 2026-06-16** (Market lens render Brief; design confirmed in the batch-close Q&A).
Planetary wash in `body_surface_canvas.cpp` (`diverging_colour`, composited over terrain at ~0.55
alpha); the per-body strip in **`circumplanetary_canvas.cpp`** — the Circumplanetary rung, *not*
`solar_system_canvas.cpp` as the Session-2 handoff's file list said (Solar has no market surface).
The good-selector is the shared combo from the Resource lens (`overlay.cpp`, bound to
`ui_state.lens_resource`; since BL-093 a popup opened from the minimap bar rather than an inline
combo). On-canvas keys/strip are anchored **flush-left of the minimap** (their right edge at the
minimap's left edge, vertically centred on the minimap — a `lens_key_anchor` passed from `app.cpp`),
reading as a drawer folding out from the minimap's left side and clearing the widened
Selection/ledger column (relocated 2026-07-06 from the former canvas-left-edge placement past the nav
rail). The **count-driven keys** — Country, Market, Reach, Supply, whose row list grows with the
world (nations/markets/lanes present) — share `draw_scroll_list_key` (`body_surface_canvas.cpp`,
BL-163/164, 2026-07-10): the box height is **capped to the canvas vertical span** and its rows live
in a **bounded, wheel/drag-scrolling ImGui child** with a clean scrollbar, so a long list scrolls
inside the box rather than overrunning the canvas edges (the pre-fix failure — e.g. a ~20-nation
Country legend spilling off the bottom). The header stays fixed above the scroll region, and the
Market lens's good-selector combo sits above the header. The fixed-height gradient-bar keys
(Production, Scarcity, Population, Industry, Opportunity) keep their simple `begin_lens_key` chrome. Verified by `scripts/verify/market_lens.lua`, which runs `verify.econ_step(12)`
to diverge prices from base before capture (and a new `verify.show_panel("economy", false)` hook to
clear the panel `econ_step` opens), against blessed goldens.

**Toward per-tile variation.** The wash is body-wide because the present world seeds **one market
per body**. The data model now supports **multiple tile-centred markets per body** — each market
carries a `centre_tile`, and a tile clears against the nearest centre's market (its *catchment*,
`market_for_tile` in `src/world/market_clearing.cpp`); landed 2026-06-16. When generation seeds
genuinely multiple centres (from capitals / population centres — owed, follows the population
layer, OPENS § Trade), this lens should tint **each market's catchment** rather than the whole
body, regaining a spatial per-market surface.

## Per-lens selection validity & routing (settled 2026-06-15, [F4])

The active lens does not only re-skin the canvas — it also **defines what the pointer resolves
to**. Each lens answers "what is the meaningful target under this pointer?" differently, so the
same position resolves to a different entity (and routes to a different ledger) per lens. The
Selection element walks the kind stack (SELECTION.md) and returns the first entity this lens calls
valid:

| Lens | Valid target under the pointer | Routes selection to |
|---|---|---|
| **none** | the lowest drawn entity (marker, else tile) | Tile Ledger |
| **Corporation** | the **owning corporation** of the tile/building | Balance Ledger |
| **Country** | the **owning nation** of the tile | Nation ledger |
| **Resource** | the tile's **deposit** profile | Tile Ledger (deposit detail) |
| **Market** | the body's **market** / the listing under the pointer | Market Ledger |
| **Opportunity** | the tile (and its best-building margin breakdown) | Tile Ledger |
| **Production** | the producing **building** under the pointer | Balance Ledger |
| **Scarcity** | the tile's **market** (the catchment under the pointer) | Market Ledger |
| **Industry** | the tile (no dedicated ledger route; falls through to the tile) | Tile Ledger |
| **Supply** *(Layer 5)* | the **route segment / stockpile** under the pointer | (Supply surface) |

A lens skips kinds it does not validate: under the Corporation lens a hovered *building* resolves
*through* to its owning corporation, not to the building, because the corporation is the lens's
unit of meaning. Under no lens that same building resolves to itself. This is the per-lens
*validity* function the resolution rule consumes; the gating on data/geometry per lens (Market,
Supply) applies here too — a lens whose data does not yet exist contributes no valid target.

## Resource lens *(built 2026-06-17 — no data dependency)*

**Intent.** Read the map as a *deposit-density surface*: where the body's mineral
and material wealth concentrates, so the player can site extraction before any
economy exists. This is the lens that pays off *now* — it needs no simulation,
only the tile generation already in place.

**Data definition (settled).** Every tile carries a `resource_deposit` profile
from generation (see [TILES.md](../economy/TILES.md) and
[TILE_GENERATION.md](../generation/TILE_GENERATION.md)). The lens reads that
profile directly at draw time; **no new data is generated**.

**Single mode — flat contiguous fill (settled, BL-019).** The lens is **always
single-resource** (no highest-value mode, no Single toggle): the player picks a
good from the shared selector and the lens fills the **whole contiguous deposit**
of that good as a **flat, uniform colour** — the *shape* of the deposit, not a
magnitude gradient. Every tile carrying any of the resource (deposit > 0) takes the
resource's identity colour at a fixed opacity (composited over terrain); a tile
without it keeps its terrain hue. Intensity lives in tile detail, not the lens. A
deposit is the 8-connected (diagonals included) blob of tiles with the good;
because the fill is uniform, the per-tile threshold is visually identical to a
flood-fill grouping, so no flood-fill pass is built.

**Rung.** Planetary only — deposits are per-tile and have no inter-body surface.
The render pass is guarded behind `overlay_mode::resource` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp), matching the
Corporation/Faction pattern.

**Colour.** Resource identity colours from
[`presentation.hpp`](../../src/ui/presentation.hpp) (`presentation_of(res).colour`),
the same source the resource *pip* uses — so a tile's lens tint matches its pip.
Opacity carries magnitude; hue carries identity. No per-faction colours are
involved.

**Glyph.** Three stacked horizontal strata, widening and deepening in opacity
top-to-bottom — a gradient / deposit-density motif (`icons::resource`, the
`ImU32`-colour overload). Distinct from the supply horizontal *pair* (thin,
full-width, equal), the market *vertical* bars, the faction shield, and the
corporation seal-square; and distinct from the resource *pip* diamond (the
`resource_type` overload) it shares a name with.

**Legend.** Strip glyph highlight + tooltip ("Resource deposits"), plus an
on-canvas key: the selected resource's identity swatch + name and the note
"filled = deposit present". Flat, not a gradient — the lens shows deposit *shape*.

**Interaction notes.** Planetary-only, single-select. The resource selector is the
shared combo (form shared with the Market and Scarcity selectors, bound to
`ui_state.lens_resource`) — since BL-093 a popup opened from the minimap lens bar
rather than an inline strip combo. No new data, no tick dependency.

**Implemented 2026-06-17** (Lens & Legibility batch, BL-019). `overlay_mode::resource`
Planetary pass in `body_surface_canvas.cpp`: a tile with `resource_deposit[sel] > 0` is
composited toward `presentation_of(sel).colour` at a fixed 0.8 (uniform flat fill); other tiles
keep terrain. The highest-value mode and the `resource_lens_single` toggle were removed (the
lens is always single). The on-canvas key sits **flush-left of the minimap**, vertically centred on
it (a drawer off the minimap's left side; `lens_key_anchor` from `app.cpp`) — relocated 2026-07-06
from the former left-edge / past-the-nav-rail placement. Verified by `scripts/verify/resource_lens.lua` against blessed goldens
(deterministic after the draw-order fix).

## Population lens *(built 2026-06-16, re-keyed 2026-06-30 — BL-069)*

**Intent.** Read the map as a *liveability surface*: where land is hospitable, so the player can
weigh siting and (later) population pressure. The complement to Resource's material read.

**Data definition (re-keyed, BL-069).** Every tile carries a `habitability` value in `[0, 1]` from
generation (`tile_component.habitability`), but the lens no longer tints by that raw value. It tints
by `workforce_efficiency(tile.habitability)` — the same curve `economy_system.cpp` applies to scale
labour — from [`workforce.hpp`](../../src/world/workforce.hpp): full efficiency (`1.0`) at/above
habitability `0.6`, ramping linearly down to `0.5` at habitability `0`. So the lens shows the
**labour consequence**, including the `0.6` full-labour cliff, not the raw terrain habitability —
"build where habitability ≥ 0.6 for full workforce" reads directly as a flat brightest-green band.
No new data; population *density* (people per tile) is separately carried by the BL-083 population-
centre markers, not this lens.

**Rung.** Planetary only — workforce efficiency is per-tile and has no inter-body surface. Guarded
behind `overlay_mode::population` in [`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp).

**Colour.** A sequential dark→liveable-green gradient: a tile's terrain hue is composited toward a
"liveable" green (`IM_COL32(80, 200, 110)`) at opacity `0.15 + 0.7·workforce_efficiency(habitability)`
(`lerp_colour`), so full-labour land reads brightest and the lowest-efficiency land barely tints.
Zero-habitability tiles (ocean, barren) stay untinted so terrain still reads. Sequential (not
diverging) — efficiency has a single good direction. No per-faction colours.

**Glyph.** A small figure — round head over a tapered torso (`icons::population`); reads as
"people / workforce", distinct from the other lens glyphs.

**Legend.** A low→high gradient bar (`draw_population_key`), anchored **flush-left of the minimap**,
vertically centred on it (the shared Resource/Market key placement, relocated there 2026-07-06),
labelled "Workforce efficiency" and mapping
the bar's ends to `0.5×`→`1.0×` workforce efficiency (not `0`→`1` habitability) — so the key reads
the same labour multiplier the tint shows. Tooltip "Workforce efficiency".

**Interaction notes.** Planetary-only, single-select, no selector (the whole-body efficiency
surface needs no resource pick). Verified by `scripts/verify/population_lens.lua` against blessed
goldens.

**Re-keyed 2026-06-30** (BL-069, part of the BUILD_LEGIBILITY discovery strand). The lens was
raw-habitability tint at ship (2026-06-16); the re-key swaps in `workforce_efficiency` so the
Population lens, the Selection panel, and the hover card all surface the *same* habitability→labour
feedback the economy applies, rather than the lens showing a different quantity than the sim
consumes.

## Opportunity lens *(built 2026-06-17 — BL-017; rekeyed to unmet demand 2026-07-07 — BL-112)*

**Intent.** Read the map as an *opportunity surface*: where is demand going unmet, so the
market will pay a premium to whoever supplies it? Under the BL-078 elastic economy the fillable
gap is a first-class, legible thing — a market bidding above base price — and this lens surfaces
it directly (superseding the earlier per-tile "best-building net-margin" siting estimate).
Paired with Population on the strip.

**Data definition (settled, BL-112).** Per body market, the opportunity intensity is the
**biggest tradeable price/base ratio** on that market — `max over tradeable r of price[r] /
base_price[r]` (resources with `base_price == 0` skipped). A market with a wide unmet-demand gap
prices its scarce good far above base and reads hot; a saturated market sits near neutral. Each
tile takes its **catchment market's** ratio via `market_for_tile`, so the surface reads as
uniform blocks per catchment (the same idiom as the Scarcity lens). No new data — it reads the
live `market_component` demand-discovered prices.

**Rung.** Planetary only. Guarded behind `overlay_mode::opportunity`.

**Colour.** A **diverging** surface: a market bid above base (unmet demand) composites toward
green (`IM_COL32(110,200,120)`), an oversupplied one toward red (`IM_COL32(216,100,96)`), keyed
by `d = log(clamp(ratio, 0.25, 4)) / log(4)` at `0.75 · |d|` alpha. Tiles with no catchment
market keep terrain.

**Glyph.** An open circle with an inner "+" (`icons::opportunity`) — a potential-gain motif.

**Legend.** Strip glyph + tooltip ("Opportunity (unmet demand)") + an on-canvas
supplied→unmet diverging key.

**Interaction notes.** Planetary-only, single-select; the script ticks the economy
first so prices have resolved. Verified by `scripts/verify/opportunity_lens.lua`.
Owed follow-on: the margin formula is a first-cut estimate (single workforce, no
contention); refine when the build-cost amortisation model lands.

## Production lens *(built 2026-06-17 — BL-009)*

**Intent.** Read the map as a *production-intensity surface*: where value is actually
being made right now. Complements Opportunity (potential) with the realised output.

**Data definition (settled).** Per producing tile, intensity = **Σ(output qty ×
resolved price)** across the building's outputs this tick, read from the
`economy_report` (`output_quantity`) and the tile's market prices; a processor's
total output is split across its recipe's products by their batch proportions.
Idle / exhausted / unbuilt tiles produce nothing → no entry → cold.

**Rung.** Planetary today; a Circumplanetary per-body output badge is owed (rung
table). Guarded behind `overlay_mode::production`.

**Colour.** Each producing tile's value is taken **relative to the body's
producing-tile geometric mean** and run through the diverging warm↔cool ramp
(`diverging_colour`, the same band the Market wash uses): above the mean reads warm,
below cool, composited at 0.6 over terrain. So contrast is meaningful across bodies
of very different absolute output; a body of similar producers reads near-neutral
(honest — there is little intensity spread to show).

**Glyph.** A filled upward triangle over a baseline (`icons::production`) — output
rising.

**Legend.** Strip glyph + tooltip ("Production intensity") + an on-canvas low→high
diverging key.

**Interaction notes.** Planetary-only, single-select; the script ticks the economy
so buildings produce and the report populates before capture. Verified by
`scripts/verify/production_lens.lua`.

## Scarcity lens *(built 2026-06-16 — no data dependency; keyboard-cycle only since BL-093, 2026-07-04)*

**Off the on-screen bar (BL-093).** Scarcity dropped off the visible minimap lens bar in the
BL-093 trim (a single row of 7 fits; Scarcity and Industry were cut to make room) and is now
reached by **keyboard lens-cycle only**, joining Supply. The `overlay_mode::scarcity` render pass
below is unchanged and still fires when selected by keyboard — only the glyph's presence on the
bar was removed.

**Intent.** The inverse of the Resource lens: read the map as an *absence surface* — where a chosen
good is **scarce or absent**, so the player sees gaps rather than concentrations. Answers "where is
there *no* iron?" directly, which the density lens only shows by omission.

**Data definition (settled, BL-018).** Reads **market supply shortfall**, not tile deposits:
`shortfall = max(0, market.demand[sel] − market.supply[sel])` — how much demand outran supply for
the selected good last tick, independent of price. **Single-resource only** (scarcity *of what?*);
the good is the shared `ui_state.lens_resource` (same combo as Resource and Market). Reads the
existing `market_component` arrays — no new data; needs the economy to have ticked so supply/demand
are populated.

**Rung.** Planetary only. Guarded behind `overlay_mode::scarcity` in `body_surface_canvas.cpp`.

**Colour — chunky per-market blocks.** The lens is a **market-level field**, not a per-tile one:
every tile in a market's catchment (via `market_for_tile`) reads as **one solid block** tinted by
that market's shortfall, normalised across the body's markets (the body-max shortfall in a pre-pass).
A tile is composited toward a hot hue (`IM_COL32(220, 70, 55)`) at opacity `0.6 · scarcity`, so a
met market keeps terrain and a short one reads hot. With one market per body (the present generation)
the whole body reads as a single block — honest to the market structure (the catchment is the unit);
it gains spatial variation once multiple centres are seeded (BL-036).

**Glyph.** A hollow downward-pointing triangle (`icons::scarcity`) — an "empty / depleted" motif,
the inverse of the filled resource pip.

**Legend.** An abundant→scarce gradient bar plus the selected resource's name and identity swatch,
same placement as the others. Tooltip "Resource scarcity". The resource selector appears as the
shared popup (form shared with Resource/Market) when the lens is active via keyboard-cycle.

**Interaction notes.** Planetary-only, single-select. The script runs `verify.econ_step(12)` so
market supply/demand populate before capture. Verified by `scripts/verify/scarcity_lens.lua`
against blessed goldens (iron and steel variants prove the selector re-skins the surface).

**Implemented 2026-06-17** (Lens & Legibility batch, BL-018). Reworked from the prior deposit-based
per-tile heatmap to per-market shortfall blocks: a pre-pass collects each body market's
`max(0, demand−supply)` of the selected good and the body-max; the fill pass maps each tile to its
market (`market_for_tile`) and composites the hot hue by the normalised shortfall. The on-canvas key
reads "Market scarcity" (met → scarce).

## Industry lens *(built 2026-07-04 — BL-084; keyboard-cycle only since BL-093, 2026-07-04)*

**Off the on-screen bar (BL-093).** Industry shipped the same day it was trimmed off the visible
minimap lens bar — the BL-093 redesign keeps the on-screen row to 7 glyphs, so Industry (like
Scarcity) is reached by **keyboard lens-cycle only**, joining Supply. The `overlay_mode::industry`
render pass below is unaffected; only the bar presence changed.

**Intent.** Read the map as an *economic-throughput surface*: where the existing, nation-owned
industry already is — distinct from where people live (BL-083's markers) or where labour is
efficient (Population). Part of the 2026-07-01 visibility-pass cluster's three-layer read:
Settlements (BL-083, discrete markers) · Industry (this lens) · You (BL-085, identity chrome).

**Data definition (settled).** The substrate is mechanically live from BL-050: every tile carries
`substrate_density` in `[0, 1]`, the nation-owned background occupation injected each economy tick
as `background_supply[]`/`background_demand[]` into the body's market clearing. Rendered raw,
`substrate_density` ripples outward from population centres and reads collinear with the Population
lens, so the lens instead renders a **throughput field**: each tile's `substrate_density` weighted
by its **terrain deposit richness** (the sum of `resource_deposit` across all resources, normalised
to the body's richest tile) — brightest where dense occupation sits on rich terrain, decoupling the
field from the population ripple. **Pure rendering**: no change to `substrate_density` or the market
arithmetic.

**Rung.** Planetary only — the substrate field is per-tile and has no inter-body surface. Guarded
behind `overlay_mode::industry` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp).

**Colour.** A sequential dark→amber gradient: a tile with any substrate composites its terrain hue
toward industrial amber (`IM_COL32(210, 150, 70)`) at opacity `0.15 + 0.6·t`, where `t` is the
tile's throughput normalised to the body's maximum. Tiles with no substrate (`substrate_density`
`0`) keep their terrain hue untinted. Sequential (not diverging) — throughput has a single good
direction. No per-faction colours.

**Glyph.** A factory silhouette — a sawtooth-roofed block with a chimney (`icons::industry`; see
[ICONS.md](ICONS.md)) — distinct from the Production lens's up-triangle and every other lens glyph.

**Legend.** Strip glyph highlight + tooltip (`overlay_mode_name` → "Industry density"), plus an
on-canvas **low→high amber gradient key** (`draw_industry_key` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp)) — a bar running the terrain-hue
base to full industrial amber, matching the placement convention of the other built lenses
(**flush-left of the minimap**, vertically centred on it since the 2026-07-06 relocation from the
former nav-rail-inset left edge). The key landed in the 2026-07-04 reconciliation
(the lens's original delivery shipped the tint but not the key).

**Interaction notes.** Planetary-only, single-select; the script runs `verify.econ_step(4)` so the
substrate injection has settled before capture. Verified by `scripts/verify/industry_lens.lua`
against blessed goldens (`industry_lens_full`, `industry_lens_zoom`).

## Placement-suitability surface *(BL-010 — not a strip lens)*

**Intent.** A siting aid: while a build is **armed**, every other tile is tinted by how well the
**armed building** would do there — *where can this go, and where is it best?* Its **inverse** is
the selected-tile affordance readout (BL-071, `SELECTION.md`): *given this tile, which buildings?*
— always-on, no arming required. The two share the `placement_rules` seam; this surface is the
map-wide "which tiles for a building", the readout is the per-tile "which buildings for a tile".
It is **not** an `overlay_mode` and never appears in the strip; it is an additive surface in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp) that composites over whatever the
active strip lens already drew.

**Trigger (revised 2026-06-30).** Gated on **construction mode** (`construction.active`), keyed to
the armed `construction.type` / `construction.target`. The original BL-010 build (2026-06-16) fired
on **bare tile selection**, but re-tinting the whole map on every inspection click read as a
spurious lens change and fought the active lens — so the trigger now requires an armed build. A
plain selection never re-skins the map.

**Colour.** Each non-selected tile: **invalid** (`can_place` false) → darkened 35%; **affine** →
composited 24% toward green. Affinity applies to **extraction only** — a tile whose own richest
extractable resource is the armed target reads as optimal; other building types carry no
terrain-affinity signal, so a valid tile stays uncoloured. The armed-from tile is skipped (it is
already outlined as the selection).
