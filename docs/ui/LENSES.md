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

## Current roster (2026-07-31)

The whole `overlay_mode` family at a glance, derived from `overlay.cpp` /
`ui_state.hpp`. The per-lens sections below are the design history; this table is
where to look first. Bar slots 1–8 are the minimap strip order (BL-226 made
Continent the eighth); "off the bar" lenses are reached by the keyboard lens-cycle
(`L` / `Shift+L`, `0` clears).

| `overlay_mode` | Bar | Surface (one line) | Status |
|---|---|---|---|
| `corporation` | 1 | Planetary tile tint per owning corp, player border, rival HQ reach rings (BL-182) | built |
| `country` | 2 | Planetary nation tint + owner borders + per-nation key (BL-133, landed 2026-07-09) | built |
| `resource` | 3 | Planetary contiguous-deposit flat fill; good selector in the legend (BL-134) | built |
| `market` | 4 | Planetary **catchment tint** — one colour per market (BL-015) + city-name key; Circumplanetary price strip | built |
| `population` | 5 | Per-tile red→green **value mark** (workforce efficiency; BL-135, landed 2026-07-09) + gradient key | built |
| `opportunity` | 6 | Per-tile red→green **value mark** (catchment demand-gap rank; BL-135/BL-136) + key | built |
| `production` | 7 | Planetary intensity tint, red→yellow→green vs body mean (BL-137) + key | built |
| `continent` | 8 | Planetary plate tint + boundary lift + plate-count key (BL-226, landed 2026-07-30) | built |
| `scarcity` | off the bar | Per-market shortfall blocks + key | built |
| `industry` | off the bar | Substrate-throughput amber tint + key (BL-084) | built |
| `supply` | off the bar | Solar per-convoy lines · Circumplanetary convoy-count badge · Planetary per-tile convoy glyph | built — Layer 5 convoys are live |
| `reach` | off the bar | Planetary key listing the active body's trade-route endpoints by recency (BL-011, landed 2026-07-08) | built |
| `supply_routes` | off the bar | Planetary key of aggregated lanes, log-scaled thickness (BL-014, landed 2026-07-08) | built |

> **Access note (2026-07-31) — RESOLVED (2026-08-02).** The stale
> `overlay_mode_count` this note reported is fixed in code:
> `canvas_command.cpp` anchors the count to the last enumerator
> (`supply_routes` + 1 = 14) with a `static_assert` naming the re-anchor rule,
> so the keyboard cycle reaches every lens including Supply-routes. Caught
> stale during the BL-270 action-dictionary sweep, which transcribes access
> paths from code rather than from this table.

> **Status.** All lenses are built except where a section names a gating
> dependency. **BL-226** (2026-07-30) added the **Continent** lens — the
> tectonic plates from BL-210's Continents/Drift pass, retained on the generation
> report so the boundaries that shaped the terrain can be drawn back onto it. It
> is the **eighth** glyph on the BL-093 strip, the first addition to that row of
> seven. The **Lens & Legibility** batch (BL-013/052/019/017/009/018) settled
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
> the bar). The resource/good selector (shared by Resource/Market/Scarcity) was
> briefly a popup on the minimap bar; **BL-134** (2026-07-09) moved it into the
> **on-canvas lens legend** (`draw_lens_resource_combo`,
> `body_surface_canvas.cpp`), where it now lives. The **Market** lens's
> per-catchment surface **landed** — BL-036 (market seeding) shipped multiple
> centres and BL-015 reworked the wash to a catchment-boundary tint. **Supply**
> no longer waits: Layer 5 shipped (`supply_system.cpp` populates `w.convoys`),
> so its route geometry renders on all three rungs, and the aggregated
> trade-lane read is the **Supply-routes** lens (BL-014). **BL-011** (reach lens)
> and **BL-014** (supply-routes lens) landed 2026-07-08 — see their sections.
> Identity colours live in `presentation.hpp`; the corporation-identity helper is
> `palette::corp_colour` (renamed from `faction_colour`, BL-052).

---

## Rung applicability

Which lenses are meaningful on each rung of the ladder. "—" = no representation
intended; "✓" = built and active; "(later)" = a representation is specified here
but its build is gated on a dependency (named in the lens section).

The lens bar — now on the **minimap** (BL-093, relocated from the bottom-left
strip; see [MINIMAP.md](MINIMAP.md)) — presents a **curated subset** in this order
(BL-013, trimmed BL-093, extended BL-226): **Corporation → Country → Resource →
Market → Population → Opportunity → Production → Continent**. **Scarcity** and
**Industry** are off the bar, reached by **keyboard lens-cycle only** — joining
**Supply**, **Reach** and **Supply-routes**, which do not fit the strip. The campaign
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
| Supply *(keyboard-cycle only)* | **✓ per-convoy route lines** | **✓ per-body convoy-count badge** | **✓ per-tile convoy glyph** (regenerated table, 2026-07-31 — Layer 5 convoys live) |
| **Corporation** | — | — | **✓ tile tint + player border + rival reach rings (BL-182)** |
| **Country** | — | — | **✓ tile tint + owner borders + nation key (BL-133)** |
| **Resource** | — | — | **✓ contiguous-deposit flat fill + key** |
| **Market** | — | ✓ per-body price strip | **✓ catchment tint + city-name key (BL-015)** |
| **Population** | — | — | **✓ per-tile value marks, workforce efficiency (BL-069 re-key, BL-135 marks)** |
| **Opportunity** | — | — | **✓ per-tile value marks, demand-gap rank (BL-136)** |
| **Production** | — | (later) per-body output-throughput badge | **✓ production-intensity tint + key (BL-137 ramp)** |
| **Scarcity** *(keyboard-cycle only)* | — | (later) per-body shortfall badge | **✓ per-market shortfall blocks + key** |
| **Industry** *(keyboard-cycle only)* | — | — | **✓ substrate-throughput amber tint + key** |
| **Continent** | — | — | **✓ plate tint + boundary lift + key (BL-226)** |
| **Reach** *(keyboard-cycle only)* | (owed) connected-body glow | — | **✓ connection-list key (BL-011)** |
| **Supply-routes** *(off the bar; cycle misses it — roster note)* | (owed) aggregated graph edges | — | **✓ lane-list key, log-scaled thickness (BL-014)** |

**BL-012 per-lens rung notes.** Corporation, Country, Resource, Population,
Opportunity, Industry, and Continent are **Planetary-only** — their unit of meaning
(a tile, a building, a deposit, a margin, a substrate-throughput reading, a plate)
is sub-body and has no coherent inter-body surface, and nations are sub-body
political units. Market and Supply are the genuinely multi-rung lenses (prices per
body-market; logistics span the ladder); Reach and Supply-routes are body-level
reads whose natural home is the Solar rung — their Solar surfaces (connected-body
glow; the aggregated graph) are **owed**, with the Planetary keys standing in
(BL-011/BL-014 notes in `body_surface_canvas.cpp`). Production and Scarcity are
Planetary today but each has a natural **Circumplanetary per-body badge** owed
(total output / aggregate shortfall for the anchor body) — additive passes guarded
behind the same `overlay_mode`, not changing the Planetary behaviour.

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
(`overlay_mode_name` → "Countries"), plus — landed 2026-07-09, BL-133 (country
lens legend) — an on-canvas **per-nation key** (`draw_country_key`,
`body_surface_canvas.cpp`): one `palette::nation_colour` swatch + name per nation
present on the active body, sorted by id, box auto-sized to the widest name. The
Corporation-lens counterpart (a per-corp swatch list) remains a follow-up.

**Interaction notes.** Planetary-only, single-select. Borders are recomputed at
draw time from the neighbour ownership comparison; no border data is persisted.

## Supply lens *(landed with Layer 5 — gate dissolved 2026-07-31 note)*

> **Landed (2026-07-31 note).** Layer 5 shipped: `supply_system.cpp` dispatches
> real convoys into `w.convoys`, so the gate below is history. What renders today:
> **Solar** — a line per player convoy in transit (`solar_system_canvas.cpp`);
> **Circumplanetary** — a convoy-count badge beside each body's label;
> **Planetary** — a convoy glyph on the active body's tiles while a player convoy
> touches it (`supply_active` in `body_surface_canvas.cpp`). The *aggregated*
> lane graph the spec below gestures at is its own lens now — see § Supply-routes
> lens (BL-014). The throughput scale-key remains owed. Some code comments still
> say "w.convoys is empty until dispatch lands" — stale, ignore them.

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
Planetary-first default). ~~**Gated on Layer 5**: no convoy/route geometry is
generated yet, so nothing renders today.~~ *(Superseded — see the landed note at
the top of this section; the data arrived and the lens draws it.)*

## Market lens *(built 2026-06-16 — price resolution landed in v0.0.4)*

> **Surface superseded (2026-07-31 note).** BL-015 (market boundary lens) reworked
> the Planetary surface from the price wash described below to a **catchment
> tint** — one colour per market, so market boundaries read as colour boundaries —
> with a city-name swatch key (`draw_market_key`). The Circumplanetary price strip
> stands. The wash prose below is history.

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
The good-selector is the shared combo from the Resource lens (bound to
`ui_state.lens_resource`; since BL-134, 2026-07-09, it lives **in the on-canvas lens legend** —
`draw_lens_resource_combo` in `body_surface_canvas.cpp` — not on the minimap bar). On-canvas keys/strip are anchored **flush-left of the minimap** (their right edge at the
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

## Per-lens selection validity & routing — **designed, not built**

> **Status (2026-08-04).** This section describes a resolver that **does not exist**. No selection
> code reads `state.overlay`: `body_surface_canvas.cpp` hit-tests markers (building outranks market
> centre) and otherwise takes the tile under the pointer, with a built tile resolving to its
> building — identically under every lens. The overlay is read further up the same file only to
> draw the lens *key*. The claim had also propagated into eight `lens.*` entries in
> `docs/ai/ACTIONS.json`; those were corrected in the same pass.
>
> **What is true today:** the lens changes what is *drawn*, never what a click *selects*.

The design below stands as a design (settled 2026-06-15, [F4]) — it is a good idea and worth
building; it is simply not the behaviour. Its premise: the active lens should not only re-skin the
canvas but **define what the pointer resolves to**, each lens answering "what is the meaningful
target under this pointer?" in its own terms.

| Lens | Valid target under the pointer | Would route selection to |
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

Under the design a lens skips kinds it does not validate: beneath the Corporation lens a hovered
*building* would resolve *through* to its owning corporation, because the corporation is that
lens's unit of meaning. Note the two rows that already match reality — **none** and **Industry**
both describe the lens-agnostic fall-through, which is why they read as correct today.

If this is ever built, it needs a backlog item; it does not have one.

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
`ui_state.lens_resource`) — since BL-134 (2026-07-09) it sits **in the on-canvas
lens legend**, not on the minimap bar. No new data, no tick dependency.

**Implemented 2026-06-17** (Lens & Legibility batch, BL-019). `overlay_mode::resource`
Planetary pass in `body_surface_canvas.cpp`: a tile with `resource_deposit[sel] > 0` is
composited toward `presentation_of(sel).colour` at a fixed 0.8 (uniform flat fill); other tiles
keep terrain. The highest-value mode and the `resource_lens_single` toggle were removed (the
lens is always single). The on-canvas key sits **flush-left of the minimap**, vertically centred on
it (a drawer off the minimap's left side; `lens_key_anchor` from `app.cpp`) — relocated 2026-07-06
from the former left-edge / past-the-nav-rail placement. Verified by `scripts/verify/resource_lens.lua` against blessed goldens
(deterministic after the draw-order fix).

## Population lens *(built 2026-06-16, re-keyed 2026-06-30 — BL-069)*

> **Surface superseded (2026-07-31 note).** BL-135 (value-lens tile marks,
> 2026-07-09) replaced the full-tile tint below with a per-tile red→green
> **value mark** (`icons::value_mark`) on every buildable tile, reading the same
> `workforce_efficiency` quantity; tiles keep their terrain hue. The gradient key
> stands. Shared change with the Opportunity lens.

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

> **Surface superseded (2026-07-31 note).** Two changes since the prose below:
> BL-136 (opportunity demand signal, 2026-07-09) re-keyed the metric to a
> body-relative, volume-weighted **demand-gap rank** per catchment, and BL-135
> swapped the tile tint for the per-tile red→green **value mark**
> (`icons::value_mark`), shared with the Population lens. Key is the red→green
> rank bar.

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
same placement as the others. Tooltip "Resource scarcity". The resource selector appears in the
on-canvas lens legend (BL-134, shared with Resource/Market) when the lens is active via
keyboard-cycle.

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

## Continent lens *(built 2026-07-30 — BL-226)*

**On the strip as the eighth glyph.** The first addition to the BL-093 row of seven. It earns the
slot rather than the keyboard-only shelf because it answers a question the player asks at *first
sight* of a body — "why is the land shaped like that?" — which is exactly the moment they are
looking at the strip.

**Intent.** Show the **tectonic plates** the Continents/Drift pass (BL-210) drifted into place, and
above all show **where they meet**. A plate interior is just a region; a plate *boundary* is where
the mountain range, the rift and the porphyry copper came from. This is the lens that makes the
generated history visible on the map rather than only readable in the biography.

**Data definition (settled).** `run_continents` assigns every tile to a plate by wrapped Voronoi,
then folds a per-tile height bias into Pass 1's heightmap — after which the plate that raised a tile
is **unreadable from the finished terrain**. So the pass now returns its per-tile
`continent_state::plate_id`, and `make_hard_coded_world` retains the whole `continent_state` on
`generation_report::body_entry`. This is **presentation data**: the report never enters `world`, so
the field stays off the serialisation seam (the same reasoning that keeps `world_params` in the app,
BL-114). The canvas matches the active body to its report entry **by name**, the stable key the Tile
Ledger's biography already uses. The pass's `history` lines are *moved* into the body biography and
cleared, so those lines keep a single owner.

Deriving the field instead by flood-filling contiguous land at render time was considered and
rejected: it yields **landmasses, not plates**, so it can colour the continents but cannot explain
them.

**Rung.** Planetary only — plates are a per-tile surface field. Guarded behind
`overlay_mode::continent` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp).

**Colour.** **Categorical**, not sequential: each plate takes a slot from a dedicated ten-colour
table (`plate_colour`), composited over the terrain at opacity `0.80` — the lens is about the plate
field, not the terrain beneath it. Boundary tiles (any of the four neighbours belongs to another
plate; columns wrap, rows do not) then take a **separate white lift** at `0.45`.

Two constraints the first draft got wrong and the final version encodes:

- The boundary must read on a **different channel** from the plate colour. "The same colour, blended
  harder" is not a visible difference — those boundaries vanished entirely on capture.
- The palette must be genuinely **categorical**. Muted mineral tones chosen to avoid resembling the
  nation wheel all landed at luminance ~100–130 with almost no hue spread, and collapsed into one
  grey wash. The final table keeps the earthy cast that separates it from
  `palette::nation_colour` — plates are *substrate*, not identity, and this must not read as a
  second Country lens — but alternates light/dark so adjacent slots differ even in greyscale.

**Glyph.** Two interlocking plates split by a diagonal seam (`icons::continent`; see
[ICONS.md](ICONS.md)). The **seam** is the load-bearing shape: it distinguishes the glyph from the
Country glyph's bordered territory and from any solid landmass blob, because what the lens shows is
the boundary, not the area.

**Legend.** Strip glyph highlight + tooltip (`overlay_mode_name` → "Continents (tectonic plates)"),
plus an on-canvas key (`draw_continent_key`) at the usual flush-left-of-the-minimap anchor. Unlike
the gradient keys it has no scale to explain — the tint is categorical — so it explains the one
thing that is not self-evident: that the **pale** tiles are boundaries. It also reports the plate
count, and degrades honestly: a body with no plate record says so, and a **stagnant-lid** body
(`plate_count == 1`) says "one immobile plate" rather than drawing a meaningless single tint.

**Interaction notes.** Planetary-only, single-select. Verified by
`scripts/verify/continents_terrain.lua`, which captures the lens on **Kepler** and on **Selene** (the
small-grid body — a different plate count and a tighter key layout). The check that matters is
**correspondence**: the boundaries in the lens capture should line up with the ridges and coastlines
in the plain-terrain capture from the same script, since that is what confirms the lens is showing
the field the terrain was actually derived from.

## Reach lens *(built 2026-07-08 — BL-011; off the bar)*

**Intent.** Read the map as *your commercial network*: which bodies the corp's
persistent trade routes (`w.trade_routes`, BL-088) actually connect, tiered by
recency. Player's own routes only, per the competitor-visibility rule
([DISCOVERY.md](DISCOVERY.md)) — rival lanes stay private.

**Surface.** `trade_route` is body-level, and the Planetary canvas only ever shows
the *active* body's grid — so the "highlight the connected bodies" read lands here
as a **connection-list key**: one row per endpoint the active body is routed to,
name + recency dot (`draw_reach_key`, `body_surface_canvas.cpp`). Fresh routes
read `palette::activity_known` green; gone-cold routes grey
(`activity_stale`) — the activity-fog convention (BL-089). No tile re-skin. The
body-marker glow the design calls for belongs on the **Solar** canvas and is owed.

**Glyph / access.** Reuses `icons::convoy` (a dedicated glyph is an open TODO in
`ui::icons`); not on the strip — keyboard lens-cycle only.

**Key.** The shared `draw_scroll_list_key` chrome, headed "Reach (your trade
network)"; an unrouted body honestly says "no routes from this body".

## Supply-routes lens *(built 2026-07-08 — BL-014; off the bar)*

**Intent.** The aggregated lane graph: one edge per (body pair), thickness from
traffic volume, colour from recency — Supply shows convoys in flight, this shows
the *standing lanes* they carved.

**Surface.** Built from `w.trade_routes` at render time (already upserted per
pair + corp, so the player filter yields one entry per pair). On the Planetary
canvas it reads as a **lane-list key** (`draw_supply_routes_key`,
`body_surface_canvas.cpp`): one row per lane touching the active body, a
**log-scaled thickness bar** from `convoy_count` (a bare completion reads as a
thin sliver; heavy repeat traffic saturates rather than dominating linearly),
recency-tier colour shared with Reach. The Solar-canvas graph rendering the
design specifies is owed.

**Glyph / access.** Reuses `icons::supply`; off the strip, reached by the
keyboard lens-cycle (the 2026-07-31 unreachability note is resolved — see the
roster's access note).

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
