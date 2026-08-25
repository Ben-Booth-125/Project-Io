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

## Roster

The whole `overlay_mode` family at a glance. Bar slots 1–6 are the minimap strip
order; "off the bar" lenses are reached by the keyboard lens-cycle (`L` /
`Shift+L`, `0` clears). `canvas_command.cpp` derives `overlay_mode_count` from the
enum's own `count` sentinel, so a lens added at the end of the family is reachable
by the cycle without a literal being kept in step by hand.

| `overlay_mode` | Bar | Surface (one line) |
|---|---|---|
| `corporation` | 1 | Planetary tile tint per owning corp, player/rival HQ markers |
| `resource` | 3 | Planetary contiguous-deposit flat fill; good selector in the legend |
| `market` | 4 | Planetary **catchment tint** — one colour per market + city-name key; Circumplanetary price strip |
| `population` | 5 | Per-tile red→green **value mark** (workforce efficiency) + gradient key |
| `continent` | 6 | Planetary plate tint + boundary lift + plate-count key |
| `scarcity` | off the bar | Per-market shortfall blocks + key |
| `industry` | off the bar | Background-firm plant amber tint + key |
| `supply` | off the bar | Solar per-convoy lines · Circumplanetary convoy-count badge · Planetary per-tile convoy glyph |
| `reach` | off the bar | Planetary key listing the active body's trade-route endpoints by recency |
| `supply_routes` | off the bar | Planetary key of aggregated lanes, log-scaled thickness |
| `throughput` | off the bar | Planetary reach-cost field (far → at an anchor) + an active-LP ring on every supply anchor + gradient key |

Identity colours live in `presentation.hpp`; the corporation-identity helper is
`palette::corp_colour`.

---

## Rung applicability

The lens bar — on the **minimap** (see [MINIMAP.md](MINIMAP.md)) — presents a
**curated subset** in this order: **Corporation → Resource → Market →
Population → Continent**. **Scarcity** and **Industry** are off the bar, reached
by **keyboard lens-cycle only** — joining **Supply**, **Reach**,
**Supply-routes** and **Throughput**, which do not fit the strip. The Continent
lens earns its bar
slot over the keyboard-only shelf because it answers a question the player asks at
*first sight* of a body — "why is the land shaped like that?" — which is exactly
the moment they are looking at the strip.

> **Throughput's absence from the strip is questioned, not settled** (Ben,
> 2026-08-25, looking at the live lens): *"I can't see a logistics glyph below the
> minimap."* BL-605 owns it, and it is entangled with the legend region below —
> the strip currently lives on the minimap bar, which is the rect a legend now
> claims.

The campaign opens on **no lens** (`overlay_mode::none`, the plain canvas) — a
click only updates the Selection element and never re-skins the canvas, so the
canvas starts unskinned and the player picks a lens deliberately (Ben,
2026-06-30, reversing an earlier Corporation default). The bar is single-select
with a null state (re-selecting the active lens clears to plain terrain,
`toggle_overlay`).

The per-rung representation of every lens, on-bar or keyboard-only. "—" = no
representation intended.

| Lens | Solar | Circumplanetary | Planetary |
|---|---|---|---|
| Supply *(keyboard-cycle only)* | per-convoy route lines | per-body convoy-count badge | per-tile convoy glyph |
| **Corporation** | — | — | tile tint + player/rival HQ markers |
| **Resource** | — | — | contiguous-deposit flat fill + key |
| **Market** | — | per-body price strip | catchment tint + city-name key |
| **Population** | — | — | per-tile value marks, workforce efficiency |
| **Scarcity** *(keyboard-cycle only)* | — | per-body shortfall badge | per-market shortfall blocks + key |
| **Industry** *(keyboard-cycle only)* | — | — | background-firm plant amber tint + key |
| **Continent** | — | — | plate tint + boundary lift + key |
| **Reach** *(keyboard-cycle only)* | connected-body glow | — | connection-list key |
| **Supply-routes** *(keyboard-cycle only)* | aggregated graph edges | — | lane-list key, log-scaled thickness |
| **Throughput** *(keyboard-cycle only)* | — | — | reach-cost field + per-anchor active-LP ring + key |

**Per-lens rung notes.** Corporation, Resource, Population, Industry and Continent
are **Planetary-only** — their unit of meaning (a tile, a building, a deposit, a
background-plant reading, a plate) is sub-body and has no coherent inter-body
surface. Market and Supply are the genuinely multi-rung lenses (prices per
body-market; logistics span the ladder). Reach and Supply-routes are body-level
reads whose natural home is the Solar rung — the connected-body glow and the
aggregated graph — with the Planetary keys as their per-body read. Scarcity is
documented as carrying a **Circumplanetary per-body badge** (the anchor body's
aggregate shortfall) — an additive pass guarded behind the same
`overlay_mode`, not changing the Planetary behaviour.

A retired lens takes its coarser rungs with it. Production's Circumplanetary
per-body output badge was a *different read* from its Planetary intensity tint — a
body-level throughput total rather than a per-tile comparison — but it was reached
only by selecting the lens, so with the lens gone there is no control that shows it.
A body-level output badge that stood on its own would be **new always-on chrome**
with its own question to answer, not a survivor of the Production lens.

Interaction notes shared by all lenses: lenses are **Planetary-first** in this
prototype, single-select (one `overlay_mode` at a time). The mode bar lives on the
**minimap**; the lens re-skins whichever Planetary canvas is open. Coarser
Solar/Circumplanetary representations, where specified, are additive render
passes guarded behind the same `overlay_mode` — they do not change the Planetary
behaviour.

### Legend placement — one chrome home

**Everything a lens puts on screen beside the canvas lives in one region: the
minimap's header, top right.** Ben, 2026-08-24 (BL-602, one chrome home): *"This
selection element for lenses should always fit in the header for the minimap, at
the top right corner."* The selector and the key share that home, because a lens
draws **at most one** key — so one region serves the whole roster and there is
nothing for a new lens to choose between.

This supersedes two earlier rulings on the same region. BL-533 (legend in minimap
quadrant) moved legends off the canvas into the right chrome column; BL-566
(legend inside minimap) revised that to put them inside the minimap box. The
current placement is the minimap **header**.

**The rect.** Same x and width as the minimap (`ui::minimap_rect`), so the two read
as one stack of chrome flush to the right screen edge; **bottom edge on the
minimap's top edge**, growing **upward** into the column's otherwise-unused space
and ceilinged one margin below the time panel's foot. `ui::lens_chrome_rect`
(`shell_metrics.hpp`) owns that algebra and every legend asks it — no key takes a
position argument, so there is no second derivation to drift.

**Bottom-anchored, and that is load-bearing.** A box that grows upward from a
fixed top takes its own header, and therefore its toggle, with it: opening the
list moved the control a third of the screen and the second press landed on the
canvas instead of closing it, so the toggle worked exactly once. Anchoring the
**bottom** keeps the header on the minimap's edge open or shut, and the list reads
as a drawer sliding up out of the minimap.

Every legend is keyed off the active `overlay_mode` and drawn in
`body_surface_canvas.cpp`, before the input early-out so it shows in headless
captures too. Two shapes share the one region:

- **Count-driven keys** — Market, Reach, Supply-routes, whose row list
  grows with the world (markets / lanes present) — share
  `draw_scroll_list_key`. They are a **dropdown, collapsed by default**
  (`ui_state::lens_key_open`; one flag serves every legend, since a lens draws at
  most one), so a lens switch never throws a forty-row list across the column
  (Ben, 2026-08-22, NR-503). The header bar sits at the box's **foot**, on the
  minimap's edge, carrying a caret, the title and the row count — the count so a
  collapsed legend still says how much it is hiding. Rows live in a bounded,
  wheel/drag-scrolling ImGui child; long labels **wrap** rather than widening the
  box (Ben: "keep names shorter, and use text wrapping").
- **Fixed-height gradient-bar keys** — Resource, Scarcity, Population, Industry, Throughput,
  Continent — use `begin_lens_key` and draw **open**. The collapse affordance
  belongs only to the lists, because a list that grows with the world is the only
  thing it was ever for; a key with nothing to overflow gains nothing from a press
  that hides it.

**Z-order is a placement consequence, not a patch.** The gradient keys used to
anchor flush-**left** of the minimap, vertically centred — inside the rect the
always-open Selection band occupies — so they drew as ghosts through the band at
roughly a tenth of their contrast, and only the Continent key escaped by taking
ImGui's foreground draw list with an opaque fill (BL-376, continent key z-order).
One of seven was fixed and the collision was never generalised. In the region the
keys no longer overlap any window, so every key draws on the shared background
list and the foreground special case is gone. An **input blocker** over the whole
region is what remains necessary: a draw list paints pixels and registers no
window, so without it a press on the legend would also select the tile underneath.

> **STILL OPEN — Ben, 2026-08-25: "when a lens needs a legend, it takes the minimap
> space."** Ruled while looking at the live build, which did NOT yet carry the
> region described above — the key was still on the old flush-left anchor, and the
> complaint was that it "looks sloppy popped out over the selection element". The
> region above already answers that complaint, so this ruling may already be
> satisfied; what it asks for and what shipped are not obviously the same rect.
> The header sits ABOVE the minimap and grows upward into the column; the ruling
> says the legend takes the minimap's OWN space and the minimap yields.
>
> **Do not resolve this by reading — look at it.** BL-604 owns the question, and
> is to be settled against the merged build rather than either description.

The resource/good selector shared by the Resource, Market and Scarcity lenses is
one combo bound to `ui_state.lens_resource` (`draw_lens_resource_combo`), sitting
directly above the active key in the same region — never on the minimap bar, which
carries glyphs only (BL-134, lens selector in legend).

---

## Corporation lens

**Intent.** Read the map as a *corporate landscape*: where corporations operate,
who owns what, and how the player's footprint sits against rivals. National
territory is the border band's job (always-on chrome, [PLANETARY.md](PLANETARY.md)
§ The national border band) — this lens is about **corporate-owned tiles**
first; nation context is deliberately absent from its fill.

**Ownership definition (settled).** A *corporate-owned tile* is any tile on which
a corporation holds a building. The mapping is derived at draw time from
`w.corporations[].assets` → `building_component.tile` (a building id resolves to
its tile). **There is no influence radius and no nation-domination heuristic** —
only the literal building tile counts as owned. A tile with no corporate building
is *unowned*.

**Rung.** Planetary only. No Solar or Circumplanetary representation — the render
pass is guarded entirely behind `overlay_mode::corporation` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp) and changes
nothing on the other two canvases.

**Colour.**
- **Owned tiles** are tinted their owning corporation's identity colour — a direct
  replacement of the terrain hue, not a blend.
- The **player corporation** (`w.player_entity`) uses
  `presentation::faction_colour(0)` for its tile fill and additionally gets a thin
  border in `palette::selection` (white) so the player's holdings contrast against
  any rival fill colour at a glance.
- **Rival corporations** use the per-corp hashed slot already used for the
  building markers (a multiplicative hash kept off slot 0 so a rival never
  collides with the player's colour).
- **Unowned tiles** render in their plain terrain colour with **no tint** — there
  is no nation underlay in this lens.

**Glyph.** A filled square with a centred inner dot — a "seal" silhouette
(`icons::corporation`; see [ICONS.md](ICONS.md)). Distinct from the
extraction-site filled diamond, the processing-facility plain filled square, and
the port/unit filled triangle.

**Legend.** The active lens is named by the strip glyph highlight and its hover
tooltip (`overlay_mode_name` → "Corporation ownership"). A per-corp colour key is the
lens's legend, sharing the `draw_scroll_list_key` chrome with the other
count-driven keys (§ Legend placement).

**Player identity chrome.** The player's white outline above is drawn as part of the
Corporation lens's own fill/border pass, but the *general* player-identity accent — a subtle wash
on the player's tiles at the plain default, plus an outline drawn under **every** lens — is
separate, always-on chrome (`is_player_tile`, `corp_identity(w.player_entity)` in
`body_surface_canvas.cpp`) drawn once regardless of which lens (if any) is active. The
Corporation lens does not add a second player outline — it *extends the same identity language
to rivals*, giving every corporation (not just the player) a readable tile tint. The home ring /
HQ star, drawn only on the player's home body, is a further, distinct layer of the same identity
chrome.

**Corporate HQ marker** (BL-182, corporate reach). Beyond tinting *held tiles*, the lens draws
each **rival** corporation's `hq` star, in the corp's identity colour, reading the corp's
**persisted** seat (`corporation_component::hq_building`, designated at generation —
CORPORATION_GENERATION.md § Pass 3b). There is **no reach ring** around that seat: a fixed-radius
ring never grows as a corp builds outward and, with the reach fog showing supply reach, "doesn't
show anything informative" (Ben's live critique). `influence_range` is computed and stored — a
future operate-gate may want it — but not drawn. The player's marker and every rival's are drawn
through **one shared `draw_corp_hq` path**, each on that corp's **home body** (the single-home
model; branch offices on other bodies are out of scope). The player's own marker stays always-on;
the rival layer shows under this lens (no double-draw of the player). This layer is
**render-only chrome** — it gates nothing. The *gameplay* mechanic (range that gates operations,
the national origin gate, multi-HQ building via advancement, the tall/wide axis, law/tech
levers) is BL-182's (corporate borders). See `scripts/verify/corporate_reach.lua`.

---

## The Country lens has retired — national borders are chrome

*(`overlay_mode::country` no longer exists. `"country"` and its legacy alias
`"faction"` still parse in the verify-script name parser, resolving to
`overlay_mode::none` — the plain canvas, which is where national borders now
live.)*

Ben, 2026-08-24: *"National borders should not diffuse together, instead they
should borders extending their colour inwards. With this, we can drop the nation
lens."*

**The lens dissolved rather than being deleted: its content became chrome.** A
nation's identity colour sits at its frontier and falls off inwards over three
tiles, drawn always — under every lens and on the plain canvas — exactly as
roads are. The full render spec, the falloff table, and the
never-average-two-nations constraint that decides both halves of the pass are in
[PLANETARY.md](PLANETARY.md) § The national border band.

**Why a lens was the wrong home for it, in this system's own terms.** A lens is a
*mode*: one is active at a time, and it re-skins the map to answer one question at
the cost of every other. Territory is not that kind of question — it is context
the player wants while reading prices, deposits or output, not instead of them. A
territory-wide tint forced the trade; a band at the boundary does not, because it
leaves the middle of a territory free.

**And the tint had a defect a band does not.** Under BL-532's ruling the Country
lens vertex-blended like everything else, so a province straddling a frontier could
tint toward a nation it did not belong to — the mean of two nation colours is a
third nation's colour. The band is composited per tile after the blend, and its
boundary stroke is inset inside each tile rather than laid on the shared edge, so
no pixel can belong to a colour that is neither neighbour's.

**Its per-nation key retired with it.** A scroll-list of every nation on the body
was the legend for a wash that no longer exists; the band answers the same question
at the thing itself, by hovering the border.

## Structure-grain selection & routing

**The lens defines what the pointer resolves to — at the grain the lens DRAWS.** That principle
is not new; the routing table below has carried it since 2026-06-15. What BL-603 adds is that the
rule now works on whole **structures**, and says so *before* the click. Ben, 2026-08-24: *"When a
lens reveals any large structure, the selection should pivot to the entire structure, and no longer
provinces. So for the market lens, the entire market gets highlighted on mouse over, and clicking
opens up our market ledger for that market."*

### Two resolvers, and which wins

| Resolver | Answers | Used by |
|---|---|---|
| **Boundary** (`resolve_structure_hit`) | "am I on this structure's edge?" | The national border band — always-on chrome, not a lens |
| **Area** (`lens_structure_of_tile`) | "which structure is this ground part of?" | The active lens |

**A marker outranks both, and a boundary outranks an area.** The order is not arbitrary: a marker
and a border are things the player *aimed at*, while a catchment is ground they happen to be over.
That ordering is also what keeps a border clickable while a lens is active.

### Which lenses carry a structure

| Lens | Structure | Resolves at | Opens |
|---|---|---|---|
| **Market**, **Scarcity** | The market's whole **catchment** | Area — any ground inside it | Market Ledger, aimed at that market |
| **Corporation** | The corp's **holdings** on this body | The **marker** — see below | Budget ledger |
| Population, Industry, Resource, Continent | *(none — tile grain)* | — | Tile Ledger |

**Why Corporation resolves at the marker and not the ground.** Its structure is exactly the set of
tiles carrying that corp's buildings — so every tile in it *also* carries a marker, and a marker
outranks a structure. An area pivot there could never fire. It resolves *through* the building
instead, which is what this document has described since 2026-06-15 and what nothing implemented
until BL-603.

**Why Resource and Continent carry none.** Their regions — a contiguous deposit, a tectonic plate —
have no **entity id**, and a selection is an entity. Lighting a region the player then cannot select
would promise a pivot that never arrives, so they keep tile grain rather than being half-supported.

### The hover half

The structure lights **whole** on hover, as a wash rather than an outline: outlining a catchment
means walking its boundary every frame to find the outward-facing edges, while a wash costs one test
per drawn tile — and it is the truer read, because the claim is *"all of this is one thing"*, which
is an area statement rather than an edge one. It lands one frame behind the pointer, exactly as the
hovered-province outline already does and for the same reason: the tile loop must know the answer
before it has drawn the tile that produces it.

**Check:** `scripts/verify/lens_structure_pivot.lua`.


**A structure is selected by its boundary, not by anything under a single tile.**
This is a selection grain of its own, sitting between the per-lens routing table
below (which resolves what a *tile* means under the active lens) and the markers.
It is **lens-agnostic**: a structure boundary is chrome, so it resolves the same
way whatever lens is active, and it is not a row in the per-lens table.

| Structure | Target on canvas | Routes selection to |
|---|---|---|
| **Nation** | the **national border band** (§ above) | the nation — the Nation ledger |

Ben's ruling of 2026-08-24, on where the nation ledger is reached now that the
lens that owned the route has gone: **"click the border itself."** The border is
what carries a nation on screen, so the border is what opens it — a rail slot
would have put a nation behind a menu while its territory sat under the pointer,
and routing through the province card would have made it reachable only via a
grain the player may not care about.

Two things the pattern requires, neither optional:

- **A real hit width.** The drawn stroke is a line, and a line is not clickable at
  play zoom. Each drawn boundary segment registers a hit **corridor** whose width is
  independent of the stroke's thickness (`structure_hit_zone`, `ui_state.hpp`).
- **A hover read that names the structure before the click commits.** An immediate
  label at the cursor — not the glance-then-stick hover card
  ([TOOLTIP.md](TOOLTIP.md)), which waits out an appear delay by design.

**Priority:** markers (building > market centre > unit) outrank a structure
boundary, and a boundary outranks the tile/province under it. A marker is a
specific thing the player aimed at; inside the corridor, the boundary *is* what the
pointer is on.

**This is the general case on purpose.** A `structure_hit_zone` carries its own
kind and its own corridor width, and the resolver is a nearest-segment walk that
knows nothing about nations. A plate rim, a market-catchment edge or a corp
territory boundary joins the table by producing zones; the resolver and the click
path do not change.

## Supply lens

**Intent.** Read the map as a *logistics network*: where goods move, along which
routes, and where throughput concentrates. The economic counterpart to the border
band's political read.

**Rung applicability.** Supply is the one lens with a meaningful representation on
*every* rung, because logistics span the zoom ladder. `supply_system.cpp`
dispatches convoys into `w.convoys` each tick, and the lens draws them:
- **Solar** — a line per player convoy in transit (`solar_system_canvas.cpp`).
- **Circumplanetary** — a convoy-count badge beside each body's label.
- **Planetary** — a convoy glyph on the active body's tiles while a player convoy
  touches it (`supply_active` in `body_surface_canvas.cpp`).

The *aggregated* lane graph — the standing lanes convoys carve — is its own lens,
§ Supply-routes lens.

**Colour.** Route lines and badges use a single neutral logistics hue
(`palette` entry, not per-faction) modulated by throughput; the lens is about flow
volume, not ownership. Tiles are not tinted — supply annotates, it does not
re-skin terrain.

**Glyph.** Two parallel horizontal lines — a route/convoy shorthand
(`icons::supply`). Distinct from the market vertical bars.

**Legend.** Strip glyph highlight + tooltip ("Supply routes"), and a throughput
scale-key (line weight → goods/tick).

**Interaction notes.** Propagates across all three rungs (the exception to the
Planetary-first default).

## Market lens

**Intent.** Read the map as a *market surface*: which market each tile clears
against, and where the boundaries between markets fall. The complement to
Supply — Supply shows flow, Market shows the markets that drive it.

**Rung applicability.**
- **Solar** — none; prices are per-body-market and have no inter-body surface.
- **Circumplanetary** — a **per-body price strip** for the anchor body's market (a
  compact good→price list, the selected good highlighted), drawn in
  `circumplanetary_canvas.cpp`.
- **Planetary** — a **catchment tint** (BL-015, market boundary lens): one colour
  per market, composited over terrain, so market boundaries read as colour
  boundaries. Markets are tile-centred (`market_component::centre_tile`) and a
  tile clears against the nearest centre's market — its *catchment*,
  `market_for_tile` in `src/world/market_clearing.cpp` — so the tint is uniform
  within a catchment and changes at its edge.

**Colour.** One identity colour per market (`market_catchment_colour`), not a
price ramp: the Planetary surface is about *which* market, and the strip carries
the prices. The player picks which good the strip highlights through the shared
selector (`lens_resource`).

**Glyph.** Three ascending bars — a price-chart silhouette (`icons::market`).
Distinct from the supply horizontal pair and the resource horizontal strata
(market bars are vertical and outlined).

**Legend.** Strip glyph highlight + tooltip ("Market catchment boundaries") plus a
city-name swatch key (`draw_market_key`) in the shared lens chrome region, the
good-selector combo directly above its header bar (§ Legend placement).

**Interaction notes.** Single-select. The Circumplanetary strip and the Planetary
surface share the resolved `market_component.price`. Verified by
`scripts/verify/market_lens.lua`, which runs `verify.econ_step(12)` to diverge
prices from base before capture (and `verify.show_panel("economy", false)` to
clear the panel `econ_step` opens).

## Per-lens selection validity & routing (settled 2026-06-15, [F4])

Owned by BL-372 (lens-keyed selection). The active lens does not only re-skin the canvas: it **defines what the pointer
resolves to**, each lens answering "what is the meaningful target under this
pointer?" in its own terms. SELECTION.md § Lens-driven hover & selection
resolution owns the stack-walk; this is the per-lens table it reads.

| Lens | Valid target under the pointer | Routes selection to |
|---|---|---|
| **none** | the lowest drawn entity (marker, else tile/province) | Tile Ledger |
| **Corporation** | the **owning corporation** of the tile/building | Balance Ledger |
| **Resource** | the tile's **deposit** profile | Tile Ledger (deposit detail) |
| **Market** | the body's **market** / the listing under the pointer | Market Ledger |
| **Scarcity** | the tile's **market** (the catchment under the pointer) | Market Ledger |
| **Industry** | the tile (no dedicated ledger route; falls through to the tile) | Tile Ledger |
| **Supply** | the **route segment / stockpile** under the pointer | Supply surface |

**Country has no row because it is not a lens.** A nation is reached by its border
band under *every* lens — see § Structure-grain selection & routing above.

A lens skips kinds it does not validate: beneath the Corporation lens a hovered
*building* resolves *through* to its owning corporation, because the corporation
is that lens's unit of meaning. The **none** and **Industry** rows describe the
lens-agnostic fall-through: `body_surface_canvas.cpp` hit-tests markers (building
outranks market centre) and otherwise takes the tile — or province — under the
pointer, with a built tile resolving to its building.

## Resource lens

**Intent.** Read the map as a *deposit-density surface*: where the body's mineral
and material wealth concentrates, so the player can site extraction before any
economy exists. It needs no simulation, only the tile generation.

**Data definition (settled).** Every tile carries a `resource_deposit` profile
from generation (see [TILES.md](../economy/TILES.md) and
[TILE_GENERATION.md](../generation/TILE_GENERATION.md)). The lens reads that
profile directly at draw time; **no new data is generated**.

**Single mode — flat contiguous fill (settled, BL-019, resource lens single mode).** The lens is **always
single-resource** (no highest-value mode, no Single toggle): the player picks a
good from the shared selector and the lens fills the **whole contiguous deposit**
of that good as a **flat, uniform colour** — the *shape* of the deposit, not a
magnitude gradient. Every tile carrying any of the resource (deposit > 0) takes the
resource's identity colour at a fixed 0.8 opacity (composited over terrain); a tile
without it keeps its terrain hue. Intensity lives in tile detail, not the lens. A
deposit is the 8-connected (diagonals included) blob of tiles with the good;
because the fill is uniform, the per-tile threshold is visually identical to a
flood-fill grouping, so no flood-fill pass is built.

**Rung.** Planetary only — deposits are per-tile and have no inter-body surface.
The render pass is guarded behind `overlay_mode::resource` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp), matching the
Corporation pattern.

**Colour.** Resource identity colours from
[`presentation.hpp`](../../src/ui/presentation.hpp) (`presentation_of(res).colour`),
the same source the resource *pip* uses — so a tile's lens tint matches its pip.
Hue carries identity. No per-faction colours are involved.

**Glyph.** Three stacked horizontal strata, widening and deepening in opacity
top-to-bottom — a gradient / deposit-density motif (`icons::resource`, the
`ImU32`-colour overload). Distinct from the supply horizontal *pair* (thin,
full-width, equal), the market *vertical* bars, the country shield, and the
corporation seal-square; and distinct from the resource *pip* diamond (the
`resource_type` overload) it shares a name with.

**Legend.** Strip glyph highlight + tooltip ("Resource deposits"), plus a key in
the shared lens chrome region: the selected resource's identity swatch + name and the note
"filled = deposit present". Flat, not a gradient — the lens shows deposit *shape*.

**Interaction notes.** Planetary-only, single-select. The resource selector is the
shared combo (bound to `ui_state.lens_resource`) in the lens legend. No new data,
no tick dependency. Verified by `scripts/verify/resource_lens.lua`.

## Population lens

**Intent.** Read the map as a *liveability surface*: where land is hospitable, so the player can
weigh siting and (later) population pressure. The complement to Resource's material read.

**Data definition (BL-069, population lens re-key).** Every tile carries a `habitability` value
in `[0, 1]` from generation (`tile_component.habitability`), but the lens does not show that raw
value. It shows `workforce_efficiency(tile.habitability)` — the same curve `economy_system.cpp`
applies to scale labour — from [`workforce.hpp`](../../src/world/workforce.hpp): full efficiency
(`1.0`) at/above habitability `0.6`, ramping linearly down to `0.5` at habitability `0`. So the
lens shows the **labour consequence**, including the `0.6` full-labour cliff, not the raw terrain
habitability — "build where habitability ≥ 0.6 for full workforce" reads directly. The Population
lens, the Selection panel and the hover card all surface the *same* habitability→labour feedback
the economy applies. No new data; population *density* (people per tile) is separately carried by
the population-centre markers, not this lens.

**Rung.** Planetary only — workforce efficiency is per-tile and has no inter-body surface. Guarded
behind `overlay_mode::population` in [`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp).

**Surface.** A per-tile red→green **value mark** (`icons::value_mark`, BL-135, value-lens tile
marks) on every **buildable** tile (valid terrain for activity — ocean excluded), coloured by
`ryg_colour(workforce_efficiency)`; tiles keep their terrain hue so terrain still reads. Drawn
instead of, not blended with, the building glyph on occupied tiles. It is the **only** lens
drawing the value mark: Opportunity shared the idiom until BL-604 retired it.

**Glyph.** A small figure — round head over a tapered torso (`icons::population`); reads as
"people / workforce", distinct from the other lens glyphs.

**Legend.** A low→high gradient bar (`draw_population_key`), labelled "Workforce efficiency" and
mapping the bar's ends to `0.5×`→`1.0×` workforce efficiency (not `0`→`1` habitability) — so the
key reads the same labour multiplier the marks show. Tooltip "Workforce efficiency".

**Interaction notes.** Planetary-only, single-select, no selector (the whole-body efficiency
surface needs no resource pick). Verified by `scripts/verify/population_lens.lua`.

## Scarcity lens

**Off the on-screen bar.** The minimap bar holds eight glyphs; Scarcity is reached by
**keyboard lens-cycle only**. The `overlay_mode::scarcity` render pass fires when selected by
keyboard exactly as a bar lens would.

**Intent.** The inverse of the Resource lens: read the map as an *absence surface* — where a chosen
good is **scarce or absent**, so the player sees gaps rather than concentrations. Answers "where is
there *no* iron?" directly, which the density lens only shows by omission.

**Data definition (settled, BL-018, scarcity lens).** Reads **market supply shortfall**, not tile deposits:
`shortfall = max(0, market.demand[sel] − market.supply[sel])` — how much demand outran supply for
the selected good last tick, independent of price. **Single-resource only** (scarcity *of what?*);
the good is the shared `ui_state.lens_resource` (same combo as Resource and Market). Reads the
existing `market_component` arrays — no new data; needs the economy to have ticked so supply/demand
are populated.

**Rung.** Planetary, plus a Circumplanetary per-body shortfall badge (rung table). Guarded behind
`overlay_mode::scarcity` in `body_surface_canvas.cpp`.

**Colour — chunky per-market blocks.** The lens is a **market-level field**, not a per-tile one:
every tile in a market's catchment (via `market_for_tile`) reads as **one solid block** tinted by
that market's shortfall, normalised across the body's markets (the body-max shortfall in a pre-pass).
A tile is composited toward a hot hue (`IM_COL32(220, 70, 55)`) at opacity `0.6 · scarcity`, so a
met market keeps terrain and a short one reads hot. The catchment is the unit, so the surface has
exactly as much spatial variation as the body has markets.

**Glyph.** A hollow downward-pointing triangle (`icons::scarcity`) — an "empty / depleted" motif,
the inverse of the filled resource pip.

**Legend.** An abundant→scarce gradient bar ("Market scarcity", met → scarce) plus the selected
resource's name and identity swatch, in the shared lens chrome region. Tooltip "Market
scarcity". The resource selector appears in the lens legend (shared with Resource/Market).

**Interaction notes.** Planetary-only, single-select. The verify script runs `verify.econ_step(12)`
so market supply/demand populate before capture. Verified by `scripts/verify/scarcity_lens.lua`
(iron and steel variants prove the selector re-skins the surface).

## Industry lens

**Off the on-screen bar.** Like Scarcity, Industry is reached by **keyboard lens-cycle only**;
the `overlay_mode::industry` render pass is unaffected by its absence from the bar.

**Intent.** Read the map as a *rival-plant surface*: where the industry the player did **not**
build already stands — distinct from where people live (the population-centre markers) and from
where labour is efficient (Population). One of a three-layer read: Settlements (discrete
markers) · Industry (this lens) · You (identity chrome).

**Data definition (BL-373, industry lens re-point).** The lens reads the **buildings owned by
background corporations** (`corporation_component.is_background`) standing on each tile of the
active body, taken from the economy report's `building_report` rows. Per tile the field is
`Σ (0.5 + 0.5 · output_share)` over those buildings, where `output_share` is the building's
`output_quantity` normalised to the largest background output on that body. The `0.5` floor keeps
an idle or under-construction background plant visible — its *presence* is the fact the lens
reports — while the output term separates a token works from a real one, and two buildings on one
tile stack. **Pure rendering**: nothing is written back. `tile.substrate_density` does not feed
this lens (it is retained on the tile only because removing it would be a save-format touch for
no gain — Ben, 2026-08-12).

The question it answers is **"where is the industry I did not build?"** — an *ownership*
question, which is why it survived the roster pruning that retired the Production lens (§ Rung
applicability): body-relative output intensity *including the player's own holdings* answered no
question the player was asking at the strip, whereas "whose plant is already standing here" does.

**Rung.** Planetary only — the field is per-tile and has no inter-body surface. Guarded
behind `overlay_mode::industry` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp).

**Colour.** A sequential dark→amber gradient: a tile carrying background plant composites its
terrain hue toward industrial amber (`IM_COL32(210, 150, 70)`) at opacity `0.15 + 0.6·t`, where `t`
is the tile's field value normalised to the body's maximum. Tiles with no background building keep
their terrain hue untinted. Sequential (not diverging) — density has a single good direction. No
per-faction colours.

**Glyph.** A factory silhouette — a sawtooth-roofed block with a chimney (`icons::industry`; see
[ICONS.md](ICONS.md)) — distinct from every other lens glyph.

**Legend.** Strip glyph highlight + tooltip (`overlay_mode_name` → "Industry density"), plus an
on-canvas **low→high amber gradient key** (`draw_industry_key` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp)) — a bar running the terrain-hue
base to full industrial amber, in the shared lens chrome region (§ Legend placement).

**Interaction notes.** Planetary-only, single-select; the verify script's `verify.econ_step(4)` is
load-bearing — the field is read from the economy report, so it needs a tick to exist at all.
Verified by `scripts/verify/industry_lens.lua` (`industry_lens_full`, `industry_lens_zoom`).

## Continent lens

**Intent.** Show the **tectonic plates** the Continents/Drift pass drifted into place
(`docs/generation/CONTINENTS.md`), and above all show **where they meet**. A plate interior is
just a region; a plate *boundary* is where the mountain range, the rift and the porphyry copper
came from. This is the lens that makes the generated history visible on the map rather than only
readable in the biography.

**Data definition (settled).** `run_continents` assigns every tile to a plate by wrapped Voronoi,
then folds a per-tile height bias into Pass 1's heightmap — after which the plate that raised a tile
is **unreadable from the finished terrain**. So the pass returns its per-tile
`continent_state::plate_id`, and `make_hard_coded_world` retains the whole `continent_state` on
`generation_report::body_entry`. This is **presentation data**: the report never enters `world`, so
the field stays off the serialisation seam (the same reasoning that keeps `world_params` in the
app). The canvas matches the active body to its report entry **by name**, the stable key the Tile
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

Two constraints the palette encodes:

- The boundary must read on a **different channel** from the plate colour. "The same colour, blended
  harder" is not a visible difference — such boundaries vanish entirely on capture.
- The palette must be genuinely **categorical**. Muted mineral tones at luminance ~100–130 with
  almost no hue spread collapse into one grey wash. The table keeps the earthy cast that separates
  it from `palette::nation_colour` — plates are *substrate*, not identity, and this must not read as
  a national territory — but alternates light/dark so adjacent slots differ even in greyscale.

**Glyph.** Two interlocking plates split by a diagonal seam (`icons::continent`; see
[ICONS.md](ICONS.md)). The **seam** is the load-bearing shape: it distinguishes the glyph from the
Country glyph's bordered territory and from any solid landmass blob, because what the lens shows is
the boundary, not the area.

**Legend.** Strip glyph highlight + tooltip (`overlay_mode_name` → "Continents (tectonic plates)"),
plus a key (`draw_continent_key`) in the shared lens chrome region (§ Legend placement). Unlike the
gradient keys it has no scale to explain — the tint is categorical — so it explains the one thing
that is not self-evident: that the **pale** tiles are boundaries. It also reports the plate count,
and degrades honestly: a body with no plate record says so, and a **stagnant-lid** body
(`plate_count == 1`) says "one immobile plate" rather than drawing a meaningless single tint.

**Z-order.** This key was for a while the only one that read at all, because it alone drew on
ImGui's **foreground** list with an opaque fill (BL-376, continent key z-order) while the other
gradient keys were buried under the always-open Selection band at the old flush-left anchor. That
was a z-order patch standing in for a placement fix. In the shared region no key overlaps a window,
so this one draws on the same background list as the rest — the plate swatches, which are the one
thing it exists to show, read on their own without a special case.

**Interaction notes.** Planetary-only, single-select. Verified by
`scripts/verify/continents_terrain.lua`, which captures the lens on **Kepler** and on **Selene** (the
small-grid body — a different plate count and a tighter key layout). The check that matters is
**correspondence**: the boundaries in the lens capture should line up with the ridges and coastlines
in the plain-terrain capture from the same script, since that is what confirms the lens is showing
the field the terrain was actually derived from.

## Reach lens

**Intent.** Read the map as *your commercial network*: which bodies the corp's
persistent trade routes (`w.trade_routes`, DISCOVERY.md) actually connect, tiered by
recency. Player's own routes only, per the competitor-visibility rule
([DISCOVERY.md](DISCOVERY.md)) — rival lanes stay private.

**Surface.** `trade_route` is body-level, and the Planetary canvas only ever shows
the *active* body's grid — so the "highlight the connected bodies" read lands here
as a **connection-list key**: one row per endpoint the active body is routed to,
name + recency dot (`draw_reach_key`, `body_surface_canvas.cpp`). Fresh routes
read `palette::activity_known` green; gone-cold routes grey
(`activity_stale`) — the activity-fog convention. No tile re-skin. The
body-marker glow belongs on the **Solar** canvas (rung table).

**Glyph / access.** Reuses `icons::convoy` (a dedicated glyph is an open TODO in
`ui::icons`); not on the strip — keyboard lens-cycle only.

**Key.** The shared `draw_scroll_list_key` chrome, headed "Reach (your trade
network)"; an unrouted body honestly says "no routes from this body".

## Supply-routes lens

**Intent.** The aggregated lane graph: one edge per (body pair), thickness from
traffic volume, colour from recency — Supply shows convoys in flight, this shows
the *standing lanes* they carved.

**Surface.** Built from `w.trade_routes` at render time (upserted per pair + corp,
so the player filter yields one entry per pair). On the Planetary canvas it reads
as a **lane-list key** (`draw_supply_routes_key`, `body_surface_canvas.cpp`): one
row per lane touching the active body, a **log-scaled thickness bar** from
`convoy_count` (a bare completion reads as a thin sliver; heavy repeat traffic
saturates rather than dominating linearly), recency-tier colour shared with
Reach. The Solar-canvas graph is the lens's inter-body representation (rung table).

**Glyph / access.** Reuses `icons::supply`; off the strip, reached by the
keyboard lens-cycle.

## Throughput lens

**Intent.** Read the map as a *capacity surface*: how much can move through here,
and how far is this ground from the capacity that would move it? Owned by BL-598
(throughput lens), the surface half of
[LOGISTICS.md](../economy/LOGISTICS.md) § Logistic Points, whose ruling is that
**throughput is Reach with a magnitude** — an extension of an existing surface
rather than a new one. Its companion duty is the same section's *"surfacing is
non-optional"*: a cap nobody can see is silent interdiction again.

**Data definition.** Two reads, both of things that already exist.

- The **field** is `body_reach_field`, read per tile through the const
  `tile_reach_cost` — the weighted cost from that tile to its nearest supply
  anchor. The Reach rule spends this field as a *binary placement predicate*
  (reachable / not); the lens spends the quantity the predicate throws away.
- The **magnitude** is `active_lp_anchor_pools` — this tick's active Logistic
  Points at every supply anchor (a city, or a built and active port or inland
  hub), at the authored `active_lp_per_anchor_tick`.

**Why the field is the reach cost and not "the LP serving this tile".** Because
the second is a constant, measured rather than assumed: on the home body every
anchor generates the same authored rate and *every* tile has a finite reach cost
(ocean is crossable at a sea-leg cost), so both "is it reached" and "how much LP
reaches it" are flat over the whole grid. A per-tile *nearest-anchor attribution*
would be needed to make the second vary, and deriving one is the second
distance model [LOGISTICS.md](../economy/LOGISTICS.md) rule 2 refuses. The lens
therefore draws the quantity that does vary and is the player's actual question:
how far this ground is from capacity. **It does not assert that LP attenuates with
distance** — LP is not distance-priced, and a lens implying it were would be
asserting the model constraint 3 forbids.

**Rung.** Planetary only — anchors and the reach field are per-tile. Guarded
behind `overlay_mode::throughput` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp).

**Colour.** Two deliberately different sequential ramps, because the lens shows
two things and one ramp cannot separate them.

- **Field:** deep navy (furthest) → the logistics cyan (at an anchor), composited
  at `0.72`. The cost ratio is **square-root compressed** before the ramp: the
  distribution is heavily left-skewed (measured on the home body: median `20.8`
  against a maximum of `101.8`, over 57 anchors), so a linear ramp puts four
  fifths of the grid in its top fifth and the map reads as one flat wash.
  Unreachable ground takes the cold end.
- **Anchor:** a **ring** on the anchor tile, its thickness carrying that anchor's
  share of the body's largest pool, in a hotter near-white cyan over a dark
  backing. A ring rather than a disc because every anchor tile already carries a
  settlement or building marker drawn after this pass — a filled mark is simply
  hidden by it, which is what the first cut did at all 57 anchors.

**Fogs.** The field is part of the tile fill, so the survey mask and the
intra-body vision wash govern it exactly as they govern every other lens fill. The
anchor ring follows the **marker** convention rather than the road-span one: the
survey mask owns it, the vision fog does not dim it — an anchor is a city or a
completed port, as public as the building glyph beside it.

**Glyph / access.** Off the strip, keyboard lens-cycle only; reuses
`icons::convoy` on the same terms Reach does, the lens it extends.

**Legend.** A fixed-height gradient key (§ Legend placement) on the **foreground**
draw list with an opaque fill, so it is readable over the Selection band. It
carries the field ramp (`far` → `at anchor`), the anchor ring drawn exactly as the
map draws it beside the per-anchor rate, and the body's anchor count and total
LP/tick. A body with no anchor, or an authored rate of zero, says so rather than
drawing a scale over nothing.

**Determinism.** The pools are rebuilt **every frame** by `update_body_throughput`
(`body_surface_canvas.hpp`) — non-const, for the same reason `update_body_vision`
is: the draw holds a `const world&` and must not be what populates a cache.
Nothing is persisted: LP is a per-tick rate, so the lens holds a photograph of
this tick's rate, never a stock carried across one. The anchor list is **sorted by
tile id before it is reduced**, so neither the legend's total nor the per-tile
lookup depends on hash-map iteration order.

**Interaction notes.** Planetary-only, single-select, no selector. Verified by
`scripts/verify/throughput_lens.lua`.

## Placement-suitability surface *(not a strip lens)*

**Intent.** A siting aid: while a build is **armed**, every other tile is tinted by how well the
**armed building** would do there — *where can this go, and where is it best?* Its **inverse** is
the tile construction ledger's reason-coded validity read (`SELECTION.md` § The tile construction
ledger): *given this tile, which buildings?* The two share the `placement_rules` seam; this surface
is the map-wide "which tiles for a building", the ledger is the per-tile "which buildings for a
tile". It is **not** an `overlay_mode` and never appears in the strip; it is an additive surface in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp) that composites over whatever the
active strip lens already drew.

**Trigger.** Gated on **construction mode** (`construction.active`), keyed to the armed
`construction.type` / `construction.target`. It does not fire on bare tile selection: re-tinting
the whole map on every inspection click reads as a spurious lens change and fights the active
lens (2026-06-30). A plain selection never re-skins the map.

**Colour.** Each non-selected tile: **invalid** (`can_place` false) → darkened 35%; **affine** →
composited 24% toward green. Affinity applies to **extraction only** — a tile whose own richest
extractable resource is the armed target reads as optimal; other building types carry no
terrain-affinity signal, so a valid tile stays uncoloured. The armed-from tile is skipped (it is
already outlined as the selection).
