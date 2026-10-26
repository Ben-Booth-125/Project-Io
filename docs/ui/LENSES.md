# Project Io — Map Lenses

> **Settles:** which lenses exist and how many may be active at once · which rung
> each applies to — the whole Lens × rung table, for every rung at once — and how
> the strip changes with the rung · which key a lens draws and whether it collapses
> · what each lens overlays and at what grain it selects · how opening a ledger
> arms a lens.
> **Not here:** where the lens bar and the lens chrome region sit (MINIMAP) · glyph
> shapes (ICONS) and identity colours (`presentation.hpp`) · what a press under a
> lens resolves to (SELECTION) · what a rung draws that no lens gates, and what
> each rung defers (SOLAR, CIRCUMPLANETARY, PLANETARY) · what the unlensed ground
> looks like (PLANETARY, RENDERING).
> **Confused with:** MINIMAP.md, SELECTION.md, ICONS.md.

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

The whole `overlay_mode` family at a glance. Which of them the minimap strip carries
is read in § The strip rotates with the rung; every lens is also reachable by the
keyboard lens-cycle (`L` / `Shift+L`, `0` clears). `canvas_command.cpp` derives
`overlay_mode_count` from the enum's own `count` sentinel, so a lens added at the
end of the family is reachable
by the cycle without a literal being kept in step by hand.

| `overlay_mode` | Surface (one line) |
|---|---|
| `resource` | Planetary contiguous-deposit flat fill; good selector in the legend |
| `market` | Planetary **catchment tint** — one colour per market + city-name key; Circumplanetary price strip |
| `population` | Per-tile red→green **value mark** (workforce efficiency) + gradient key |
| `continent` | Planetary plate tint + boundary lift + plate-count key |
| `scarcity` | Per-market shortfall blocks + key |
| `industry` | Rival-plant amber tint + key |
| `supply` | Solar per-convoy lines · Circumplanetary convoy-count badge · Planetary per-tile convoy glyph |
| `reach` | Planetary key listing the active body's trade-route endpoints by recency |
| `supply_routes` | Planetary key of aggregated lanes, log-scaled thickness |
| `throughput` — the **Logistics** lens | Planetary: every link of the player nation's chain — roads, rail legs, sea legs — coloured red → yellow → green by supplied over requested where traffic crosses it, steel where it is connected but idle, umber where a road is not tied to the capital + a ring on every source + numbers on every link (supplied over requested; one figure on an idle link) + key. No ground tint; the painted routes leave the ground while it is up. |
| `culture` | Planetary province fill in the primary culture's lineage hue; a checker of the second culture where it is close + culture key |
| `trade_flow` | Planetary paths of the player's shipments along the routes they ride (roads, sea lanes, rail), with direction chevrons and a head at the destination + a marker on each short market coloured by why the player's surplus did not go there + class key |

Identity colours live in `presentation.hpp`.

Two build-flow surfaces mark the map without being `overlay_mode` values, so they are on neither
the strip nor the cycle: the **placement lens** (§ The placement lens), armed by choosing a
building in the Construction view's site finder, and the **placement-suitability surface**
(§ Placement-suitability surface), armed by placement mode.

---

## Rung applicability

The lens bar — on the **minimap** (see [MINIMAP.md](MINIMAP.md)) — is **derived per
rung**: it carries the lenses that draw something at the canvas rung in play, and
nothing else (§ The strip rotates with the rung). What follows is why a given lens
earns a strip glyph at all, which is a separate question from which rung shows it.

The **Continent** lens earns a place on the strip over the keyboard-only shelf
because it answers a question the player asks at *first sight* of a body — "why is
the land shaped like that?" — which is exactly the moment they are looking at the
strip.

**Logistics carries its own glyph** (Ben, 2026-08-25: *"the only thing now is to add
the glyph for our throughput lens"* — the lens was named Throughput until Ben renamed it
Logistics, 2026-10-24). Logistics is one of the two things the whole
design is rate-limited by, which is a poor thing to leave undiscoverable. It carries
its **own** glyph rather than the borrowed convoy chevron — a strip lens gets a
distinct mark (BL-605). The glyph is a **truck** (Ben: *"just use a truck as the
glyph"*); two abstract cuts were tried and both failed at strip size, and a
vehicle needs no decoding.

The campaign opens on **no lens** (`overlay_mode::none`, the plain canvas) — a
click only updates the Selection element and never re-skins the canvas, so the
canvas starts unskinned and the player picks a lens deliberately (Ben,
2026-06-30, reversing an earlier Corporation default). The bar is single-select
with a null state (re-selecting the active lens clears to plain terrain,
`toggle_overlay`).

**The default view does not mark the player's own buildings** (Ben, 2026-10-24). With no
lens the player's buildings are drawn as every building is — no identity wash, no outline,
no tint saying they are the player's. Ownership on the plain canvas is the national border
band's (`PLANETARY.md` § Ownership reads from the ground); a lens the player picks may still
mark the player's footprint (§ Corporation and Company lenses — retired, the outline).

The per-rung representation of every lens, on-bar or keyboard-only. "—" = no
representation intended. **This table is the only place a lens's rungs are
stated.** The rung docs ([SOLAR.md](SOLAR.md), [CIRCUMPLANETARY.md](CIRCUMPLANETARY.md),
[PLANETARY.md](PLANETARY.md)) point here rather than carrying a column of it: a
column read off one rung at a time cannot show that Market and Supply span the
ladder while the rest do not, and a copy drifts from the original silently.

| Lens | Solar | Circumplanetary | Planetary |
|---|---|---|---|
| Supply *(keyboard-cycle only)* | per-convoy route lines | per-body convoy-count badge | per-tile convoy glyph |
| **Resource** | — | — | contiguous-deposit flat fill + key |
| **Market** | — | per-body price strip | catchment tint + city-name key |
| **Population** | — | — | per-tile workforce-efficiency **heatmap** |
| **Scarcity** *(keyboard-cycle only)* | — | per-body shortfall badge | per-market shortfall blocks + key |
| **Industry** *(keyboard-cycle only)* | — | — | rival-plant amber tint + key |
| **Continent** | — | — | plate tint + boundary lift + key |
| **Nations** | — | — | each nation's whole territory in its identity colour + nation key |
| **Reach** *(keyboard-cycle only)* | connected-body glow | — | connection-list key |
| **Supply-routes** *(keyboard-cycle only)* | aggregated graph edges | — | lane-list key, log-scaled thickness |
| **Logistics** | — | — | every chain link (road, rail, sea) coloured by traffic or reach + source ring + numbers on links + key |

**Per-lens rung notes.** Resource, Population, Industry and Continent
are **Planetary-only** — their unit of meaning (a tile, a deposit, a
rival-plant reading, a plate) is sub-body and has no coherent inter-body
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

### A lens washes the rendered ground (Ben, 2026-10-09, NR-1003)

**Under a lens the Planetary canvas still draws the baked ground** — the same master image,
at the same 22.5° camera, with its structures, settlements, landform forms and rivers
([RENDERING.md](RENDERING.md) § Level of detail, § Installations). The lens is a
**translucent wash over it**, per tile, in the identical geometry, never an opaque fill that
replaces it. The wash was ruled once the ground became one image per body: before that, a lens
drew over a vector fill, and with the canvas glyphs retired every building, river and mountain
vanished the moment any lens was up — which made the owner lenses ask the player to pick owners
whose buildings they could not see.

- **One wash rule for the roster.** A lens colours a tile by blending its colour over the
  ground at a wash strength strong enough to read as the lens's answer and weak enough that
  the structure, form and river beneath still read through it; the strength is one tuned value
  per lens family (categorical, sequential), set by eye against captures. A tile the
  lens has no answer for takes **no wash** — the plain ground shows.
- **The strengths (set against `lens_modes.lua`'s `lens_wash_*` captures, 2026-10-09).**
  **Categorical** — Market, Continent, the Resource lens's deposits —
  washes at **0.46**: a hue says *which*, and hues separate at under half alpha even over
  textured ground, so the structures and forms keep reading through. **Sequential** —
  Population, Throughput, Scarcity, Industry — washes at **0.54**: a ramp says *how much*,
  and a value read has to beat the ground's own light and dark, so it takes a little more.
  Where a sequential lens's value already set its strength (Scarcity's shortfall, Industry's
  density), the value scales the family strength the same way. The Resource lens's
  no-deposit ground keeps its white wash (§ Resource lens) at **0.50** over the bake, paler
  than the fill path's 72% so the forms survive it.
  The Continent boundary keeps its lift toward white inside the plate wash. A lit
  structure's hover wash is a wash too, at full tile radius, so it never draws a grid.
- **The lens's own marks stay vector chrome** above the wash: keys, value marks, arrows,
  labels.
- **Hit-testing is unchanged** — a lens still resolves a press to its own structure grain
  (SELECTION.md § A lens collapses selection to ONE TIER).
- **Before a body's ground exists** (the far page not yet baked) a lens falls back to the
  vector fill, as the plain canvas does.

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

The region's rect, its bottom anchor and the input blocker over it are
[MINIMAP.md](MINIMAP.md) § Lens chrome.

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

The good selector shared by the Market and Scarcity lenses is one combo bound to
`ui_state.lens_resource` (`draw_lens_resource_combo`), sitting directly above the
active key in the same region — never on the minimap bar, which carries glyphs only
(BL-134, lens selector in legend). The Resource lens has its own search-and-checklist
key instead (§ Resource lens).

---

## Corporation and Company lenses — retired

**Both retire with the corporation and the company (Ben, 2026-10-23;
`docs/GLOSSARY.md` § Corporation, § Company).** The Corporation lens tinted a picked set of
corporations' tiles over the rest in owned-grey; the Company lens was its mirror over background
firms. With no firm between a nation and its buildings, a building's owner is always its
ground's nation (`../politics/NATIONS.md` § Own ground), so "who owns what" is "whose ground is
this" — the **Nations lens**'s question (§ The Nations lens, below). Neither lens's picked-owner
set, checklist key nor glyph survives.

The general **player-identity accent** — an outline round the player's tiles drawn under every
lens (`is_player_tile` in `body_surface_canvas.cpp`) — was never the Corporation lens's, and
stays under the lenses. **The default view carries none of it** (Ben, 2026-10-24): no outline
and no wash on the plain canvas (§ Rung applicability, the default view). There is no HQ star on this canvas, the player's or
anyone's (Ben, 2026-10-10: "We should remove the HQ glyph and market center glyphs"); the HQ
itself retired (Ben, 2026-10-23, `docs/GLOSSARY.md` § Headquarters).

---

## The Nations lens — the whole territory, in its nation's colour

**A Nations lens paints every nation's whole territory in its identity colour** (Ben,
2026-10-14). It answers *whose ground is this, all of it at once* — the reading the border band
gives one edge at a time. It sits on the Planetary row of the lens bar like any other lens.

- **The always-on border band stays** (§ The Country lens has retired, below): the plain map keeps
  its national strokes. The lens is the deliberate, map-wide answer; the band is the context.
  Under the lens the band is suppressed with every other lens, since the fill already says it.
- **No two bordering nations share a similar colour** — the identity colour rule below, which the
  lens is the strictest reader of.
- **Selection is unchanged** (Ben, 2026-10-14): a press on the border band selects the nation,
  on the plain map as before; under the lens a press resolves as any lens's does (§ A lens
  collapses selection to ONE TIER), to the nation.
- Unowned ground and open ocean take no fill.

This reverses the 2026-08-24 retirement (BL-601) only in part: the band it created stays, and
what returns is the fill as a lens, not as the default view — the argument below, that territory
must not cost the player every other reading, still holds, because a lens is chosen.

## The Country lens has retired — national borders are chrome

*(`overlay_mode::country` no longer exists, and since 2026-08-28 neither does its
name: `"country"` and the legacy alias `"faction"` were removed from the
verify-script name parser, where they had resolved to `overlay_mode::none`. The
alias was retired because it was true of the picture and false of the check — a
script naming the retired lens captured the plain canvas while still asserting a
shield glyph and a territory tint, and six more swept it as one leg and captured
the default view twice under two names. **The default view is the country view**;
there is nothing left to alias.)*

Ben, 2026-08-24: *"National borders should not diffuse together, instead they
should borders extending their colour inwards. With this, we can drop the nation
lens."*

**The lens dissolved rather than being deleted: its content became chrome.** A
nation's border is an inset stroke in its identity colour — **hard edges only**,
no wash inside the territory (Ben, 2026-10-10) — drawn **on the plain canvas and
suppressed while any lens is up** (Ben, 2026-08-28, reaffirmed 2026-09-07). Roads are *not* the precedent here, and the
difference is the point: a road is terrain a lens reads over, while a national
border is a second political answer competing with the one the lens was opened to
ask. The full render spec, the edge weights, and the
never-average-two-nations constraint that decides the stroke's inset are in
[PLANETARY.md](PLANETARY.md) § The national border band.

**The identity colour is the realm's, pinned per world (Ben, 2026-09-24).** A
nation's colour is the colour slot its founding realm held on the wizard's map —
the slot the Empires, Exploration and Industrialisation rounds carry by id
(`STARTUP.md`) — pinned as a per-world nation → slot table into
`palette::nation_colour` at Begin and at load, read from the saved report. The
border band, the seat map and the Ages view share that one table, so the realm
the player watched grow through the rounds is met in the same colour at Begin:
one mapping, never a hash of the nation id and never a second greedy assignment
of its own.

**Bordering nations never share a similar colour** (Ben, 2026-10-14: *"making sure we don't
have similarly coloured nations bordering each other"*). At Begin, after the slots are pinned,
one pass checks every pair of campaign nations that share a border: a pair whose colours sit
closer than the palette's minimum distinguishable distance re-slots the nation with the smaller
territory to the nearest free slot that clears every one of its neighbours. **This outranks
lineage**: kin realms read as kin by hue (`STARTUP.md` § Round 3), except where kin border
each other, where telling them apart wins. The pass is deterministic, saved in the same pinned
table, and read by every surface — the band, the lens, the seat map, the swatches — so they agree.

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

What a press under a lens resolves to, and which ledger it drives, is
[SELECTION.md](SELECTION.md) § The lens names the ledger the selection drives.

## The other direction — a ledger opens, its lens arms

That routing table — [SELECTION.md](SELECTION.md) § The lens names the
ledger the selection drives — sends a **click on a lens** to a ledger. The
reverse also holds for some slots: **opening the ledger arms the lens**, so the
list and the map answer the same question in two registers rather than the player
having to arm it themselves.
Ben's preference is the general form — *"opening a menu usually should arm a lens"* —
and 2026-08-30 settled the Market half of it by name: *"when we open the market
ledger, we should also activate the market lens."*

**A pair exists only where both directions name each other.** That is what keeps this
from becoming a lens on every slot: that routing table is the test, and most rail
slots are not in it.

| Rail slot | Ledger | Arms | The shared question |
|---|---|---|---|
| 5 | Market | `market` | Which market is this ground in, and what does it pay? |
| 6 | Convoys | `supply_routes` | What is moving, and along which lanes? |
| 9 | History | `continent` — on entering **Tectonics**, see below | Which plate made this ground? |

**There are two arming rules, and which one a slot uses follows from how many questions
its ledger answers.**

**Fixed on open** (slots 5 and 6). Every tab gets the same lens, because the ledger
answers one question on every tab. That a strip can arm only one lens is a real
constraint rather than a simplification — it is why Convoys left the Market ledger at
all (`convoys.md` § 3).

**On entering a sub-view** (slot 9, Ben 2026-08-30). History answers four questions and
only **Tectonics** has a map twin, so a fixed arm would hand the Continent lens to a
player who opened on Story to read a deep-time biography. The lens is armed on the
**edge** — the frame the view starts being drawn — which covers both routes in: pressing
the tab, and opening the ledger onto a Tectonics it was last left on.

**On the edge, never while on it.** A player who reaches Tectonics and then deliberately
picks another lens keeps it; re-arming every frame would make the lens strip unusable on
that view. This is asserted rather than assumed — `lens_ledger_pairs.lua` checks that
opening History on Story arms nothing, that entering Tectonics arms `continent`, and
that a lens chosen afterwards survives.

**The semantics:** arm on **open** (History: on entering Tectonics); **disarm on close**, for
every pair. Ben, 2026-10-13: *"When the market ledger closes, this should also deactivate the
lens"*, then for Convoys and History, *"They should match the market."* The disarm is taken on the
ledger's close edge, whatever closed it, and only while the lens is still the one the ledger arms
— a lens the player picked meanwhile, or one a newly opened ledger just armed, is left alone. This
answers NR-722's close half; whether opening a ledger should override a lens the player chose
deliberately is still open. What is undecided is
the rest — whether opening a second ledger should re-arm to that one's lens, and whether
a lens the player *deliberately* chose should be overridden by opening a ledger at all.

**Why History takes the second rule.** `Continent` routes to *"History ledger, at its
tectonic record"*, so the pair is real — but three of History's four views have nothing
to do with plates. The choice was between a lens those three do not want and a second
arming rule; Ben took the second rule (2026-08-30), stated here rather than left to be
inferred from the code, which is what NR-722 exists to prevent.

**One lens has no ledger to pair with, and that is not an omission.**
`Logistics` routes a press to the **tile** (SELECTION.md § A lens collapses selection to ONE
TIER) and has no ledger of its own: it is
the surface half of `LOGISTICS.md` § Logistic Points, and no ledger surfaces LP.
Logistics is the road and Supply is the traffic; the Convoys ledger is the traffic's, so
the lens's twin would be a **Logistics ledger that does not exist**. (The Corporation and
Company lenses, which routed to a per-firm ledger, retired with the firms — Ben, 2026-10-23.)

## The strip rotates with the rung (Ben, 2026-08-28)

The lens strip on the minimap shows **the lenses that draw something at the current canvas rung**,
not a fixed set. Owned by BL-670 (rung-keyed lens strip).

| Rung | On the strip |
|---|---|
| **Planetary** | Resource, Market, Scarcity, Industry, Population, Continent, Logistics, Trade flow |
| **Circumplanetary** | Market, Scarcity, Supply |
| **Solar** | Supply, Reach, Supply-routes |

Each row is this document's own per-rung representation table read straight off: a lens appears
where it draws something. Before this the strip was one hand-kept array of six, and the six were
chosen by **what fit the 240 px bar** — a layout accident deciding a discovery question, which left
six of the twelve built lenses reachable only by the `L` / `Shift+L` cycle that nothing on screen
mentions. The keyboard cycle still reaches every lens from every rung; the strip is the discovery
surface, not the only door.

**Every lens has a glyph on the strip (Ben, 2026-10-07).** Each built lens sits in at least one
rung row, and each carries its **own** glyph: no lens borrows another lens's mark, and no lens is
keyboard-only. Adding a lens means adding it to a rung row and giving it a glyph in
`src/ui/icons.*`, catalogued in ICONS.md.

**Three lenses draw a value field, not a region**, and are therefore read-only surfaces: Population
(per-tile habitability), Industry (per-tile substrate throughput) and Logistics (the points
supplied and requested on every tile, and the flow along every road). Their subject is a number spread across the map rather than a thing standing on it, so
there is nothing for a selection to be *of*. SELECTION.md carries the reasoning.

## Resource lens

**Intent.** Read the map as a *deposit-density surface*: where the body's mineral
and material wealth concentrates, so the player can site extraction before any
economy exists. It needs no simulation, only the tile generation.

**Data definition (settled).** Every tile carries a `resource_deposit` profile
from generation (see [TILES.md](../economy/TILES.md) and
[TILE_GENERATION.md](../generation/TILE_GENERATION.md)). The lens reads that
profile directly at draw time; **no new data is generated**.

**A toggled set — flat fill, split where goods coincide (Ben, 2026-10-04; owner
BL-1182, resource lens multi-select).** The lens draws a **set** of resources at once,
toggled on and off from its own checklist, at most **six** — one per hex edge. This
replaces BL-019's single-select; it keeps that ruling's other half, that the lens shows
deposit **shape**, never a magnitude gradient. Only **extractable** goods
(`placement_rules::k_extractable`) can be toggled: a manufactured good — computers —
is never on a body's surface, so it is not offered.

- **One toggled good on a tile** — the tile takes that good's identity colour,
  solid.
- **Two or more** — the hex splits into **equal pie wedges from the centre**, one per
  good, in toggle order. Each wedge spans 6/n of the perimeter, so two goods halve the
  hex and three or six land on its vertices. Below the coarse level of detail the hex
  is a few pixels across, and it shows the first good only.
- **None** — the tile takes a **white wash that keeps a ghost of its terrain**: it is
  desaturated 80% toward its own luma, then pulled 72% of the way to a wash colour — a
  warm off-white on land, a pale blue-grey on water so the coastline survives. The
  deposits are the only saturated thing on the map (Ben, 2026-10-04: "the white wash
  should still exist … push up the contrast").
- **Outline** — every deposit blob is ringed with a dark stroke on each side facing
  ground that carries no toggled good, so pale identities (silica, sand, rare earth)
  still read against the wash. Like the split, it is skipped below the coarse level of
  detail.

The intra-body **activity fog is lifted** under this lens: a deposit is survey
knowledge, not activity, so surveyed ground reads clear while the survey mask still
owns unsurveyed tiles ([DISCOVERY.md](DISCOVERY.md) § the two fogs). Intensity lives in
tile detail and the hover card, not the lens. A deposit is the 8-connected blob of tiles
with the good; because the fill is uniform, the per-tile threshold is visually identical
to a flood-fill grouping, so no flood-fill pass is built.

**Rung.** Planetary only — deposits are per-tile and have no inter-body surface.
The render pass is guarded behind `overlay_mode::resource` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp), matching every
other Planetary-only lens.

**Colour.** Resource identity colours from
[`presentation.hpp`](../../src/ui/presentation.hpp) (`presentation_of(res).colour`),
the same source the resource *pip* uses — so a tile's lens tint matches its pip.
Hue carries identity. No per-faction colours are involved.

**Glyph.** Three stacked horizontal strata, widening and deepening in opacity
top-to-bottom — a gradient / deposit-density motif (`icons::resource`, the
`ImU32`-colour overload). Distinct from the supply horizontal *pair* (thin,
full-width, equal), the market *vertical* bars and the country shield; and distinct from the
resource *pip* diamond (the
`resource_type` overload) it shares a name with.

**Legend.** Strip glyph highlight + tooltip ("Resource deposits"), plus a key in
the shared lens chrome region: a header with the toggled count ("Resource deposits
(n/6)"), a **search box**, and under it a **checklist** of the extractable goods present
on the active body, filtered by name as the player types. A checked row shows the good's
identity swatch; an unchecked row shows an empty one. With six on, the unchecked rows
grey out. Goods absent from the body are not listed — a box the player cannot use
earns no row.

**Hover and selection.** A split tile belongs to every deposit it carries. The pointer
resolves to the **wedge under it**, so hovering lights every tile carrying that good,
and a press selects that deposit (the Market ledger route below is unchanged). The
hover card lists each toggled good on the tile with its richness band.

**Interaction notes.** Planetary-only. The Resource lens does **not** use the shared
good combo; that stays with Market and Scarcity (`ui_state.lens_resource`). Its own set
is `ui_state.lens_resources`. No new data, no tick dependency. Verified by
`scripts/verify/resource_lens.lua`.

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

**Surface.** A **heatmap** — the tile itself takes `ryg_colour(workforce_efficiency)` composited
over its terrain hue at `0.72`, on every tile except water. Owned by BL-668 (workforce heatmap),
Ben 2026-08-28: *"rework the workforce efficiency lens to be a heatmap, akin to throughput"*.

It was a per-tile red→green **dot** drawn in place of the building glyph, and that shape was the
reason to change it: this was the only lens in the roster that answered by **adding a mark** rather
than by colouring the ground, so it read one tile at a time and never as a field — which is the
whole reason to have a lens. The dot also had to suppress the stack ring and the landform glyph to
avoid competing with them; a tint competes with nothing, so the tile reads normally under this
lens (both marks have since left the canvas for the ground bake — RENDERING.md § Installations).

**Water is left alone** rather than tinted at the ramp's floor. Workforce efficiency is *undefined*
on ocean, not zero, and painting it red would assert "bad ground" about ground that is not ground —
it also keeps the coastline legible, which a wall-to-wall wash destroys.

**Glyph.** A small figure — round head over a tapered torso (`icons::population`); reads as
"people / workforce", distinct from the other lens glyphs.

**Legend.** A low→high gradient bar (`draw_population_key`), labelled "Workforce efficiency" and
mapping the bar's ends to `0.5×`→`1.0×` workforce efficiency (not `0`→`1` habitability) — so the
key reads the same labour multiplier the marks show. Tooltip "Workforce efficiency".

**Interaction notes.** Planetary-only, single-select, no selector (the whole-body efficiency
surface needs no resource pick). Verified by `scripts/verify/population_lens.lua`.

## Culture lens

**Intent (Ben, 2026-09-15).** *Who lives here, and so what does this market want?* The lens shows
the peoples on the ground at province grain. It is the legible cause of a market's cultural demand
weight (`../economy/MARKETS.md` § Three properties the set has to hold, property 5 as amended), so a
player can read why a good is dear in one city and cheap in the next.

**Data definition.** A province's culture shares are **derived**, never stored on the province: its
region's rural shares blended with the shares of each population centre inside it, weighted by
population (`../generation/PROVINCES.md` § What a province is). Migration fills the centres, so
cities read mixed and countryside reads plain (`../generation/INDUSTRIALISATION.md` § Beat 2).

**Surface.** Two layers on the province:

- **Primary fill** — the leading culture's colour from the lineage palette (BL-919, the same
  derivation the Culture round uses), blended across province vertices like every fill lens.
- **Secondary checker** — where the second culture's share is **at least 95% of the first's**
  (Ben, 2026-09-15: the relative reading — 42% and 40% checker, 42% and 38% do not), the province
  carries a hard checker of the two colours. It is **not** blended; it is the one exception to
  § Every lens blends across provinces, as the border band is. Only the top two cultures draw.

**Why the checker is not plaid.** The plaid refusal (`STARTUP.md` § Rounds) is about many identity
colours competing everywhere. The checker appears only on genuinely contested provinces, and kin
cultures share a hue family, so a checker between siblings reads quietly and one between strangers
reads loudly — which is the information.

**Legend.** A key of the cultures present on the body, grouped by lineage family, with the checker
swatch explained once.

**Interaction notes.** Planetary-only, single-select, no selector. Hides the border band like every
lens; nation borders are not what this lens asks.

## Scarcity lens

**Glyph / access.** On the Planetary and Circumplanetary strips (§ The strip rotates with the
rung), and in the keyboard lens-cycle.

**Intent.** The inverse of the Resource lens: read the map as an *absence surface* — where a chosen
good is **scarce or absent**, so the player sees gaps rather than concentrations. Answers "where is
there *no* iron?" directly, which the density lens only shows by omission.

**Data definition (settled, BL-018, scarcity lens).** Reads **market supply shortfall**, not tile deposits:
`shortfall = max(0, market.demand[sel] − market.supply[sel])` — how much demand outran supply for
the selected good last tick, independent of price. **Single-resource only** (scarcity *of what?*);
the good is the shared `ui_state.lens_resource` (same combo as Market). Reads the
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

**Glyph / access.** On the Planetary strip (§ The strip rotates with the rung), and in the
keyboard lens-cycle.

**Intent.** Read the map as a *rival-plant surface*: where the industry the player did **not**
build already stands — distinct from where people live (the population-centre markers) and from
where labour is efficient (Population). One of a three-layer read: Settlements (discrete
markers) · Industry (this lens) · You (identity chrome).

**Data definition (re-subjected, Ben, 2026-10-23).** The lens reads the **buildings owned by
nations other than the player's** standing on each tile of the active body, taken from the
economy report's `building_report` rows. A building's owner is always its ground's nation
(`../politics/NATIONS.md` § Own ground), so these are the rivals' plants on their own ground. Per
tile the field is `Σ (0.5 + 0.5 · output_share)` over those buildings, where `output_share` is the
building's `output_quantity` normalised to the largest such output on that body. The `0.5` floor
keeps an idle or under-construction plant visible — its *presence* is the fact the lens reports —
while the output term separates a token works from a real one, and two buildings on one tile
stack. **Pure rendering**: nothing is written back. `tile.substrate_density` does not feed this
lens (it is retained on the tile only because removing it would be a save-format touch for no
gain — Ben, 2026-08-12).

The question it answers is **"where is the industry I did not build?"** — an *ownership*
question, which is why it survived the roster pruning that retired the Production lens (§ Rung
applicability): body-relative output intensity *including the player's own holdings* answered no
question the player was asking at the strip, whereas "whose plant is already standing here" does.

**Rung.** Planetary only — the field is per-tile and has no inter-body surface. Guarded
behind `overlay_mode::industry` in
[`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp).

**Colour.** A sequential dark→amber gradient: a tile carrying a rival's plant composites its
terrain hue toward industrial amber (`IM_COL32(210, 150, 70)`) at opacity `0.15 + 0.6·t`, where `t`
is the tile's field value normalised to the body's maximum. Tiles with no rival building keep
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

**Intent.** Read the map as *your commercial network*: which bodies the player's
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

**Glyph / access.** On the Solar strip with its own glyph, distinct from the convoy
chevron (§ The strip rotates with the rung).

**Key.** The shared `draw_scroll_list_key` chrome, headed "Reach (your trade
network)"; an unrouted body honestly says "no routes from this body".

## Supply-routes lens

**Intent.** The aggregated lane graph: one edge per (body pair), thickness from
traffic volume, colour from recency — Supply shows convoys in flight, this shows
the *standing lanes* they carved.

**Surface.** Built from `w.trade_routes` at render time (upserted per pair + owner,
so the player filter yields one entry per pair). On the Planetary canvas it reads
as a **lane-list key** (`draw_supply_routes_key`, `body_surface_canvas.cpp`): one
row per lane touching the active body, a **log-scaled thickness bar** from
`convoy_count` (a bare completion reads as a thin sliver; heavy repeat traffic
saturates rather than dominating linearly), recency-tier colour shared with
Reach. The Solar-canvas graph is the lens's inter-body representation (rung table).

**Glyph / access.** On the Solar strip with its own glyph, distinct from Supply's
two lines, its strip neighbour (§ The strip rotates with the rung).

## Trade-flow lens

**Intent.** Where is my surplus going, and why is it not going where it is short? Supply
shows convoys in flight and Supply-routes the lanes they carved; Trade-flow shows the
**decision** behind them — what the player's dispatcher sent this pass, to where, at what
landed price, and what it refused. Owned by BL-1222 (trade-flow lens). Ruled by Ben
(2026-10-07, the lens form): a player lens over the player's own flows; the whole-world
diagnosis stays headless (the market-viability skill's logistics row). **The player sees unmet
want (Ben, 2026-10-08, the chain-start form):** the inputs a market's processors want and cannot
get are shown to the player, on the market ledger or on this lens — which surface is open, and is
asked of the player's question first. Owner: BL-1222 (trade-flow lens).

**Visibility.** The player's own flows only (DISCOVERY.md § Competitor visibility). A market
being short of a good is a public market signal; the refusal is a fact about the player's own
surplus. No rival's shipments, shelves or refusals appear. A market's own shelf exports are
no nation's shipments and do not appear either.

**Surface (Planetary).**
- **Flows** — one path per (source market, destination market, good) the player shipped,
  width from units over a short trailing window, in the neutral logistics hue. Hover: good,
  units, landed price (the destination price the dispatcher netted against its haul).
- **A flow rides the network** (Ben, 2026-10-26: *"render the paths from one market to another
  over the actual network"*). The path is drawn along the tiles the newest shipment of that flow
  was **priced on** — the overland road path, or land → port → sea lane → port → land, or road →
  rail → road — the route the dispatcher recorded with the shipment and the one a convoy of it
  rides (`SUPPLY.md` § Logistical cost: the lane is the legs). It is never the chord between the
  two market centres. It sits a few pixels to the right of the route, so A → B and B → A read as
  two paths; dark **chevrons** down its length give the direction, the **head** sits at the
  destination, and its **width** is the volume. A flow whose route can no longer be walked (a road
  cut since it was priced) falls back to the straight arrow between the centres.
- **Refusals** — a marker on each market of the active body that is short of a good the
  player holds in surplus, coloured by the **best** class over the player's sources of
  that good. Hover lists the classes by good.

**The classes** name the nation dispatcher's rule that refused, ranked by how far a
destination got through those rules, best last:
- **no lane** — another body, and no lane off this one.
- **price gate** — the destination does not pay the margin over the source. The dispatcher
  tests this before it prices a route.
- **no route** — no road or sea leg reaches it.
- **costly** — routed, but the haul eats the margin.
- **no propellant** — a space lane, but the pool cannot fuel the launch.
- **no room** — the destination cannot absorb more at the landed price.
- **no funds** — the rule would send, but the player cannot pay for the convoy.
- **room** — the rule would send, but the one-destination-per-pass rule or the LP cap held
  it back.

The market-viability skill's logistics row classifies **market shelf** exports, a
different dispatcher with its own order; its labels are the tool's
(`body / noroute / gate / costly / noroom / room`), not these.

**Data.** The dispatcher records, for the player's nation only, each pass's
shipments — with each shipment's priced route waypoints (its origin tile, its two ports, its two
stations) — and its best class per (good, destination market) as a transient output,
tagged with the nation it was taken for. The lens draws only passes taken for the
nation the player holds now, so a seat change shows nothing of the nation
left behind. The simulation never reads the record and it is not saved, so a loaded
game shows the lens from its first pass on. A flow's tiles are read from those waypoints by the
one route reader every convoy surface uses, and held per flow until its waypoints or the network
move, so a frame re-derives no path.

**Legend.** A class key (colour → class) and a flow-width key (units per tick), in the
shared legend home.

**Glyph / access.** On the Planetary strip with its own glyph (§ The strip rotates with the
rung), and in the keyboard lens-cycle.

## Logistics lens

**Named Logistics** (Ben, 2026-10-24). The lens was called Throughput; the strip, the key,
the hover card and this section all say **Logistics** now. Its `overlay_mode` value keeps the
identifier `throughput`, and a script may arm it as `logistics` or `throughput`.

**Intent.** Read the map as a *capacity surface*: where do the player nation's Logistic
Points reach, and does each road carry what is asked of it? Owned by BL-606 (throughput
lens), the surface half of [LOGISTICS.md](../economy/LOGISTICS.md) § Logistic Points, whose
ruling is that the lens is **Reach with a magnitude** — an extension of an existing surface
rather than a new one. Its companion duty is the same section's *"surfacing is
non-optional"*: a cap nobody can see is silent interdiction again.

**Data definition (Ben, 2026-10-16).** The lens is **player-facing**: it shows the
player's own nation's network and points, and nothing else. A rival's sources, roads and
ground are not coloured, numbered or hovered — not shaded zero, but left as they are, because
a neighbour's logistics is not a value of zero, it is not the subject (`DISCOVERY.md`
§ Competitor visibility). Every figure the lens shows is in **Logistic Points per tick**,
absolute (Ben, 2026-10-24: *"absolute numbers rather than a percentage … both the amount
generated, and the amount consumed"*).

**The ground is not tinted** (Ben, 2026-10-24: remove the colour gradient from tiles).
Neither the vector fill nor the wash over the baked ground carries a Logistics colour. The
lens's read is the **road colour** and the **numbers**; the ground under them is the plain
canvas's.

**The roads carry the colour: supplied over requested, by the routed flow** (Ben,
2026-10-24). Every road tile of the player's nation is coloured by the flow routed through
it along the chains of `LOGISTICS.md` § How a tile's points get to it:

- **Requested** is the total need of every producing building whose routed request crosses
  that road tile on its way back to the nation's capital — the buildings on the tile itself
  and every building downstream whose route passes through it.
- **Supplied** is what the chain **could deliver** there: every source whose walk crosses
  the road tile — a building's walk back to its source, or a source's link back toward the
  capital — counted **once** at that tile however many requests brought it, its points made
  this tick × its reach at the tile, summed. **A source also supplies the tile it stands on,
  and stacked sources supply together** (Ben, 2026-10-26): every connected source standing on
  the tile counts there at full reach whether or not a request's nearest-first route picked
  it, and wherever one hub of a stack carries a request, every hub of that stack's network
  on its tile carries it too. Supply can therefore exceed the request, which
  is what the scale below needs; what the resolver actually **granted** those requests is
  kept beside it on the hover card.
- A request is routed as the resolver routes it: its qualifying sources, nearest first,
  topped up until covered. Each source it draws on carries a share of it — its reach there,
  the last one cut to what covers the request — and that share of the need lies on every
  tile from the building back to the source, and on along that source's chain to the
  capital, once per share. A building no source qualifies for lays its request
  on its own tile, with nothing supplied.
- **The colour is the ratio supplied / requested on a log2 scale**: **2 or more is full
  green**, **1 is yellow**, **0.5 or less is full red** (Ben, 2026-10-26: *"use red for the
  extreme"*), interpolated on log2 between the three stops — so a link short by half or more
  reads as the alarm it is, and a link exactly covered reads amber, not as a shortfall.
  The ramp is for links that **carry** a routed request; a link no request crosses is not on
  it at all (Reach is kept apart from traffic, below).
- The road keeps its **tier** — width and the 1 : 1.5 : 2 weights, and the tier's opacity
  ladder — only its colour is the lens's. Roads that are not the player nation's keep their
  ordinary colour.

**Two numbers on every link that carries traffic, stacked** (Ben, 2026-10-24). Wherever the rung
gives the text room (a drawn radius of **20 px** or more), each **link** cell of the player's chain
— a road tile, a rail leg's track, a sea leg's water — that a routed request crosses carries
**supplied** above **requested**, both LP/tick: the routed flow above — the pair the link's colour
is the ratio of. A **connected, idle** link carries **one** figure, what could reach it, in the idle
steel; a road **not tied to the capital** carries none. **A tile off the network carries no
numbers** (Ben, 2026-10-24: the network is the roads and the rail); its hover card still explains
it. Below 20 px the digits do not fit and the link colour carries the read alone.

**Every link of the chain** (Ben, 2026-10-25: *"a lens that shows each chain link"*, and *"points on
lanes and rail"*). A railway station's points run **station to station** and a port's run across
**water**, so the routed flow names only a rail or sea link's two ends: two groups of the nation a
line or a crossing joins used to read as cut apart. The lens draws every link of the chain the
resolver routes on (`LOGISTICS.md` § How a tile's points get to it):

- **Road walks** are the roads themselves, as above.
- **A rail leg** runs along the **track**, station to station (`rail_leg_path`), and every track
  tile on it carries the leg's colour and numbers.
- **A sea leg** runs down the port's own field, from the far end to the port — a port's points
  ride the **lanes** (`lp_lane_walk`, `LOGISTICS.md` § One pool), so the leg is the run of lane
  tiles between the two ports, on the sea or a lake alike — and every lane tile on it carries the
  leg's colour and numbers.
- A leg's figures are its own: **requested** and **granted** are the routed shares whose path to the
  capital runs through that link (each request routed as the resolver routes it, its shares laid up
  its sources' paths), and **supplied** is the carrying source's points × its reach across the leg.
  Where legs share a tile their shares add; a road tile a leg also crosses (a station's, a
  landing's) takes the leg's figures unless the road itself carries traffic.
- Each leg is stroked along its run over a dark casing — a Highway's weight on rail, a lane's on the
  sea. The bake paints no rail bed under the lens, so every **track** tile is also drawn, in a dark
  ballast tone at a Road's weight, with the player's legs over it.
- A rail line between two connected stations that the chain did not join by is still a leg —
  **connected, idle**.

**Reach is kept apart from traffic** (Ben, 2026-10-25: *"an unused road between them reads 0,0"*).
Each link cell of the player's chain is in one of three states, and only the first is on the ramp:

- **Carrying** — a routed request crosses it: the ramp, two numbers.
- **Connected, idle** — on the chain, nothing routed across it. For a road: a connected source of
  the nation reaches the tile (`lp_route_to`'s `reached` — the same qualifying-source test routing
  runs). For a leg: it is one of the chain's links. Drawn in a pale **steel** `(204, 214, 230)`, one
  number (what reaches it), and the hover card says *Connected, idle*.
- **Not tied to the capital** — a road of the nation no connected source reaches. A dull **umber**
  `(118, 104, 92)`, darker than the plain canvas's sand, no number; the card says why (*Not tied to
  the capital* or *No source reaches here*).

The key carries both off-ramp states as swatches under the ramp.

**The hover card** over the player's own ground shows the same pair stacked — *Supplied N
LP/tick* over *Requested M LP/tick*; on a road *Available N LP/tick* over *Requested M
LP/tick*, then *granted G* — then what the tile's buildings consumed, then the why:
the network that reaches it best, its cost to the nearest source, and full reach or fade
(`TOOLTIP.md` § Ground).

**The marks** are that nation's **sources** — each running logistics hub, railway station
and port with the points it actually makes, upkeep folded in, read off the economy step's
own resolve. **A city is not a source** (`LOGISTICS.md` § Logistic Points, "Cities make
none"), nor is one on ground no nation holds, nor one on a neighbour's.

**The key** carries the road ramp (red *0.5x* → yellow *1x* → green *2x+*, captioned "supplied /
requested"),
the source ring drawn exactly as the map draws it beside the largest source's rate, and the
nation's arithmetic, absolute: what its connected sources **generated** this tick and how
many; what its buildings **consumed** — production's grants, the marches' grants and what
shipments drew — with what is left **unused**; and what production **needs** at its run,
whatever reaches it. A nation with no connected source still shows its ramp and its
arithmetic, with *No connected source here*, so "generated 0 against a need" reads as the
shortfall it is. A seat with no nation says so instead. No tagline — the lens is named for
what it shows.

**Why the road and not the ground.** The ground tint answered *how many points reach here*,
which the numbers already say exactly; it buried the network under a wash. The question a
player acts on is **where the network is short** — which road is asked to carry more than
its chain delivers — and that is a property of the road and the requests routed along it,
not of the tile alone. A request is drawn back along the same per-source fields the resolver
routed it on, so the colour and the economy cannot disagree about which road carried it.

**What the field used to be.** The lens first shaded the *reach cost* — the weighted
distance to the nearest supply anchor — because a per-tile points figure could not vary
while every anchor made the same authored rate; BL-1309 ended that condition, the lens then
shaded the absolute points reaching each tile, and the ground tint has now retired outright
(above). `tools/verify/throughput_field_census.cpp` remains the record of the reach-cost
ramp's measurement.

**Rung.** Planetary only — sources, roads and requests are per-tile. Guarded behind
`overlay_mode::throughput` in [`body_surface_canvas.cpp`](../../src/ui/body_surface_canvas.cpp).
The drawn road network is this lens's at every rung (RENDERING.md § Roads and sea lanes).
**While the lens is up the bake paints no route surface at all** (Ben, 2026-10-25: *"currently you
can see roads twice"*): the painted thread and the drawn stroke follow different geometry, so both
showing read as two roads. Arming the lens re-bakes only the windows a route reaches, as a
route-only partial re-bake on the worker pool, and disarming it re-bakes them back — RENDERING.md
§ Roads and sea lanes, "The Logistics lens takes the routes off the ground", owns the mechanism and
its measured cost. The capital star shows under this lens (PLANETARY.md § The capital star).

**Colour.**

- **Link carrying traffic:** red `(226, 72, 58)` at `t = 0` → yellow `(236, 206, 60)` at
  `t = 0.5` → green `(84, 214, 104)` at `t = 1`, on `t = (log2(supplied / requested) + 1) / 2`,
  clamped to [0, 1]; a road keeps its tier's alpha.
- **Connected, idle:** steel `(204, 214, 230)`. **Not tied to the capital:** umber `(118, 104, 92)`.
- **Rail track** (every track tile; the lens draws it): ballast `(92, 86, 80)`.
- **Source:** a **ring** on the source tile, its thickness carrying that source's share of
  the body's largest, in a hot near-white cyan over a dark backing. A ring rather than a disc
  because every source tile already carries a structure — a filled mark is hidden by it.

**Fogs.** The road colour and the numbers follow the road and marker conventions: the
survey mask owns them; the source ring is as public as the structure beside it.

**Glyph / access.** On the Planetary strip with its own glyph (`icons::throughput`, a truck).

**Determinism.** Everything the lens shows is rebuilt from the tick's own resolve by
`update_body_throughput` (`body_surface_canvas.hpp`) — non-const, for the same reason
`update_body_vision` is: the draw holds a `const world&` and must not be what populates a
cache. The routed flow (`lp_route_flow`) is a pure read summed in ascending building id,
held across frames only until the tick, the buildings or the network move. Nothing is
persisted and the simulation reads none of it.

**Interaction notes.** Planetary-only, single-select, no selector. Verified by
`scripts/verify/logistics_lens.lua` and `scripts/verify/logistics_chain.lua` (the chain's links,
reach apart from traffic, the unpainted ground and the capital star).

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

## The placement lens *(not a strip lens)*

**Intent.** *Where on my ground could this building go, and which of those tiles have no logistics?*
The map half of the Construction view's site ledger (`ledgers/construction.md` § The site ledger;
Ben, 2026-10-25: *"we should use a lens to show which tiles are valid … and flag tiles which do not
currently have logistics operating"*). Building first: the player picks a building in the view's
site finder, and the lens marks exactly the tiles the ledger beside it lists.

**A mode of the build flow, not an `overlay_mode`.** It is armed by choosing a building, not from
the strip; it composites over whatever strip lens is up, as the suitability surface above does; and
no saved view binding ever holds it, so the enum and the save are untouched. It carries no key: the
ledger beside it is its legend, and its caption counts the flagged tiles under the same glyph.

**Trigger.** Live while the Construction ledger draws its **Construction** view with a building
chosen, on the Planetary rung, for the active body. Switching to the Buildings view or another rung
leaves the map plain the same frame; choosing the chosen building again, or **None**, clears it; and
closing the ledger clears the choice (§ The other direction: a ledger's lens disarms on its close
edge). A press on the map under it **selects** as it would without it — the lens never claims the
click, unlike armed placement mode — so a tile picked off the map reaches its Build door exactly as
a ledger row does.

**What is marked.** Each tile is one of three classes, read off the site finder's own evaluation
(`construction_sites.cpp`), never re-derived by the canvas:

- **A site** — the player's own ground, and the Build door would accept the chosen building there:
  `placement_rules::can_place_in_world` with the player's actor, the recipe's air, and the treasury
  against `construction_capex`. Composited 30 % toward `palette::site_valid` (green).
- **A site with no Logistic Points operating** — a site whose routed cover is zero
  (`lp_route_to(...).cover == 0`: no source reaches it, or one does and is not tied to the capital).
  Composited 40 % toward `palette::site_no_logistics` (amber), and, where the tile is drawn at 8 px
  radius or more, the **no-logistics flag** (`icons::no_logistics`, the struck truck) at its centre,
  so colour is never the only channel. Below that size the wash carries it alone.
- **Everything else** — not the player's ground, or the door would refuse it there (including a
  site the treasury cannot cover). Darkened 45 % toward black.

The selected tile is skipped, as the suitability surface skips it: it is already outlined.

**Cost.** The canvas reads one byte per tile from a class field the finder rebuilds only when its
key moves (the chosen building, the body, the treasury, the reach budget, and the frame stamp: the
economy tick, the building count, the ground's owners, the logistics generation). The wash rides the
existing wash channel — the fill path's lerp, or a translucent wash over the bake — so the lens adds
no draw pass of its own.

**Determinism.** A derived read for the interface. It is never serialised and the simulation reads
none of it.

**Interaction notes.** Planetary only; no selector of its own (the site finder's picker is the
selector). Verified by `scripts/verify/construction_sites.lua`.
