# Project Io — Icon Vocabulary

The **icon set** is the project's library of small vector glyphs drawn directly
into ImGui draw lists — the building markers on the Planetary canvas, the resource
pips in ledgers and strips, the navigation-rail glyphs, and the map-lens buttons.
They are hand-drawn vector primitives (no font, no image atlas) so they stay crisp
at any size and recolour freely.

**Source of truth:** the `ui::icons` namespace.
- Declarations + the per-glyph visual contract: [`src/ui/icons.hpp`](../../src/ui/icons.hpp)
- Vector implementations: [`src/ui/icons.cpp`](../../src/ui/icons.cpp)

This document is the *design* reference — what each glyph means, where it is used,
and how colour is decided. When the two disagree, **the header's Doxygen contract
is authoritative for the signature and the code is authoritative for the actual
shape**; this doc should be corrected to match, not the reverse. Identity *colours*
are not defined here — they live in [`presentation.hpp`](../../src/ui/presentation.hpp)
(`presentation_of`, the `palette` namespace). See also
[CANVASES.md](CANVASES.md) (where markers are drawn), [SELECTION.md](SELECTION.md)
(the summaries that reuse swatches), and [LENSES.md](LENSES.md) (the lens glyphs
below feed it).

---

## Shared conventions

Every glyph function shares the same positional prefix; the trailing parameters
are per-glyph (type, tier, and/or colour):

```cpp
void glyph(ImDrawList* dl, ImVec2 centre, float r, /* trailing type/tier/colour per glyph */);
```

- **`dl`** — the draw list to render into (background list for canvases, window
  list for panels).
- **`centre`** — glyph centre in screen pixels. Glyphs are centre-anchored, not
  top-left.
- **`r`** — **half-extent** (think circumradius): the glyph is designed to fit a
  `2r × 2r` box centred on `centre`. Callers pass the radius, not the diameter.
- **colour** — either a caller-supplied `ImU32`, or *derived* (the `resource`
  glyph pulls its colour from `presentation_of`). See the catalogue for which.
- **trailing parameters** — most glyphs take a single colour; the multi-parameter
  cases are `building` (a `building_type` + a `resource_type` identity, § 1c, +
  `fill`), `settlement` (a `tier` + `colour`), `landform` (a
  `terrain_landform` + `colour`) and `stack_ring` (an ARRAY of colours + a count —
  the one glyph whose colour parameter is plural, because it draws one mark per
  member of a set).

Two settled visual sub-conventions (2026-06-15):

- **Every canvas-placed filled marker carries the dark outline** (`IM_COL32(20, 22, 28, 255)`,
  the file-local `outline`) so it reads on any terrain colour — this covers the whole entity-marker
  family (`building`). The resource **pip** is the single documented exception: as a
  strip/swatch/deposit glyph it stays **outline-less**.
- **`colour` means fill or stroke per family, fixed:** the filled families
  (`building`, `country`, `corporation`, resource `pip`, `settlement`, `industry`) treat
  `colour` as the **fill**; the stroke families (`supply`, `convoy`, `market`, `ledger`,
  `placeholder`, `stack_ring`, resource-**lens**) treat it as the **stroke** line colour
  and have no fill;
  `hq` and `activity` span both — the one `colour` fills the core *and* strokes the ring.

---

## Catalogue

Glyphs fall into three families by role.

### 1. Entity markers — drawn on the canvases

| Glyph | Function | Shape | Colour | Drawn for / where |
|---|---|---|---|---|
| **Extraction site** | `building(…, extraction_site, identity, fill)` | Faceted ore/mineral silhouette + outline — an angular eight-sided crystal chunk (wider than tall, corner-cut facets), distinct from the regular gem-diamond pip and the port triangle. The generic fallback when `identity` names no bespoke shape (see § 1c) | Caller `fill` | Building marker, Planetary canvas |
| **Processing facility** | `building(…, processing_facility, identity, fill)` | Filled square + outline. The generic fallback when `identity` names no bespoke shape (see § 1c) | Caller `fill` | Building marker, Planetary canvas |
| **Port** | `building(…, port, identity, fill)` | Filled upward triangle + outline (`identity` ignored) | Caller `fill` | Building marker, Planetary canvas |
| **Inland logistics hub** | `building(…, inland_logistics_hub, identity, fill)` | Filled flat-top hexagon + outline with a small dark hub dot at the centre — a six-sided network-node silhouette (BL-149, inland hub), distinct from the launchpad/none circle and the port triangle | Caller `fill` | Building marker, Planetary canvas; build-front-door row |
| **Military base** | `building(…, military_base, identity, fill)` | Filled shield + outline — flat top, shoulders tapering to a bottom point (BL-325, military base); the martial building, **filled** like every building glyph, so it never reads as the port's upward triangle or the hub's hexagon | Caller `fill` | Building marker, Planetary canvas; build-front-door row |
| **Building (none/other)** | `building(…, none, identity, fill)` | Filled circle (dot) — also the fallback for `launchpad` | Caller `fill` | Fallback building marker |
| **Resource pip** | `resource(…, res)` | Filled diamond (no outline) | **Derived** — `presentation_of(res).colour` | Resource strips, deposit markers |
| **Under construction** | `under_construction(…, colour)` | Crane silhouette — a mast, an angled boom, a back-stay brace, and a short hook line, four strokes with the same shadow-then-colour pass as `convoy`; stroke-only (BL-327, construction glyph), echoing the landform family's "not yet installed" convention — a filled glyph would claim the site already IS its type, and a desaturated silhouette reads as "faded", not "being built" | Caller `colour` (the owner-tinted marker colour) | Building marker, Planetary canvas — drawn IN PLACE OF the type silhouette while `ticks_remaining > 0` |
| **Convoy** | `convoy(…, colour)` | Rightward chevron (→) — two stroke lines meeting at a right point (goods in transit); points *right*, distinct from the filled port triangle | Caller stroke | Supply-lens on-canvas convoy marker — drawn on tiles a player convoy passes through, Planetary canvas; also the Reach lens's strip glyph |
| **Unit** | `unit_marker(…, fill, committed)` | Humanoid silhouette — a filled circle head over a filled triangle body, echoing `glyph_soldier` (the unit card's own placeholder, `selection_panel.cpp`) so the canvas marker and the card read as one vocabulary. When `committed`, an additional outer ring marks a contract-committed unit — a stub today (BL-575): nothing sets it true yet, since contracts are a later wave of the same batch (BL-573) | Caller `fill` (the owning corp/nation's identity colour) | Drawn once per (province, owner) **group** at the **province anchor tile**, Planetary canvas (BL-575, unit marker + march UI) — the same province-anchor convention the battle marker uses, since BL-511 made a unit's command grain the province. Hit-tested AHEAD of building and market centre (unit outranks both, BL-575), so a unit standing on a built tile is reachable on the first click |
| **Market centre** | `market_centre(…, colour)` | Circle outline with a centred cross (+); arms reach 60 % of the radius | Caller stroke | Market-centre marker, Planetary canvas |
| **Settlement** | `settlement(…, tier, colour)` | Tiered skyline — 1–5 filled towers (count = `tier`, clamped) on a baseline + per-tower outline, the middle tallest, heights tapering to the edges; an outpost reads as a lone tower, a metropolis as a dense cluster | Caller `colour` — civic-neutral `palette::settlement` (parchment); host-nation tint (`palette::nation_colour`) only under the Country lens | Population-centre marker, Planetary canvas (BL-083 civic markers; shown per the BL-625 LOD ladder — see PLANETARY.md § Settlement markers) |
| **Settlement (razed)** | `settlement_razed(…, colour)` | Ruin — two hollow, outline-only tower shells of unequal height on a faint rubble baseline; no fill (the skyline's silhouette with the life taken out) | Caller `colour`, dimmed inside the glyph so every ruin reads identically | Razed population centre (BL-624), Planetary canvas at close zoom only (BL-625) |
| **Unknown** | `unknown(…, colour)` | Question mark — a top hook arc, a short stem, and a dot | Caller stroke (dimmed) | Survey badge for an **unsurveyed** body, Solar canvas (BL-067, survey) |
| **Survey badge** | `survey_badge(…, colour)` | Magnifying glass — a lens circle with a diagonal handle (scan motif) | Caller stroke | Survey badge for an **in-progress** survey, Solar canvas; the canvas overlays a `k∕N` region count |
| **HQ** | `hq(…, colour)` | Ringed eight-point star — a diamond overlaid with an axis-aligned square, enclosed by a ring, with a dark centre dot so it reads against a same-colour ownership fill | Caller `colour` (the player identity colour) | The player's HQ/origin building, Planetary canvas (BL-085, player presence) |
| **Corp emblem** | `corp_emblem(…, shape, fill)` | One of six geometric primitives (circle / square / triangle / diamond / hexagon / pentagon) chosen by `shape`; names *whose* an entity is, not *what* it is | Caller `fill` — the corp's identity colour (`palette::corp_identity_colour(corp, player)`); `shape` from `palette::corp_emblem_shape(corp)` | Faction-identity emblem (BL-090, shared emblem glyphs): the identity card portrait, the Selection-panel header (corporation + owned/rival building), a small identity tag beside each building marker (player **and** rival) on the Planetary canvas, and the rival hover card |
| **Activity** | `activity(…, colour)` | Concentric pulse — a filled core ringed by a signal ring (commercial-beacon motif; deliberately distinct from the survey magnifier and the unknown "?") | Caller `colour` — per activity tier (`palette::activity_known` / `activity_stale` / `activity_visible`) | Commercial-activity fog badge, Solar canvas — lower-left of the body, offset from the survey badge's upper-right so the two fogs read apart (BL-089, commercial fog; see [DISCOVERY.md](DISCOVERY.md)) |
| **Value mark** | `value_mark(…, colour)` | Single filled dot | Caller fill — the caller's red→green ramp sample (`ryg_colour`) | Per-tile magnitude mark for the Workforce (Population) and Opportunity lenses (BL-135, lens value marks): drawn on every buildable tile while either lens is active, in place of a full-tile tint and, on occupied tiles, of the building glyph. See LENSES.md § Population / § Opportunity |
| **Battle** | `battle(…, colour)` | Two crossed blades, each a stroked shaft with a short cross-guard bar near its base, meeting at the centre. Deliberately **not** a bare X: the X is already the "close this" affordance throughout the chrome, and a mark meaning *a fight is here* must never read as a button meaning *dismiss this*. Stroke-only — a battle is an event on the ground, not a thing installed on it | Caller stroke | Drawn on the **province anchor tile only**, once per live battle, Planetary canvas (BL-469, battle marker). Province-grain by ruling (BL-467 ruling 1 — the province frames the fight), so scattering it across every participating tile would say the opposite |
| **Stack ring** | `stack_ring(…, kind_colours, kinds)` | A **segmented circle** inset to 0.76 r — one arc per building KIND on the tile, separated by a gap of 20 % of each segment's slot, each arc drawn as a dark under-stroke then the kind's colour (the `convoy` / `under_construction` shadow-then-colour idiom applied to an arc). Read **clockwise from the top**: the 12 o'clock segment is the dominant kind, the one the centre glyph depicts. Draws nothing below two kinds | Caller-supplied array, dominant **first** — sourced from `palette::building_kind_colour` | A stacked tile's marker, Planetary canvas (BL-596, buildings over the hex). Drawn under the centre silhouette, the corp emblem tag and the `+N` badge, all of which sit on top of it. Its level-of-detail gate belongs to the caller (PLANETARY.md § The ring's level of detail) |
| **Stack-count badge** | inlined in `body_surface_canvas.cpp` (no shared `icons::` helper) | A dark-filled circle carrying `+N` text (N = additional buildings/units beyond the first) — the same k/N text-overlay idiom the Solar-canvas survey badge uses for region progress, not a new glyph shape | Fixed light text on a dark disc, no owner tint | On a built tile carrying more than one building (BL-367, stacked tiles), staggered lower-right past the corp-identity tag (which also sits lower-right) so the two never overlap. Only the dominant (lowest-id) building's silhouette draws. The badge answers HOW MANY; the **stack ring** above it answers WHICH KINDS, and the two compose — a tile holding three extraction sites is one kind standing three times, so it draws `+2` and no ring at all. BL-575 reuses the SAME inline idiom, independently, for the unit marker's own group count (N = additional units of that owner in the province beyond the first) |

On the Planetary canvas the **building** glyph's `fill` encodes the *owning
corporation* (player corp = corp slot 0; rivals a hashed slot), so the
silhouette reads the building **type** and the fill reads **who owns it**.

**It draws over live ground, on no backing of its own** (BL-596, buildings over the hex).
The tile keeps its terrain hue, texture, relief and lens wash under and around the glyph,
so legibility is the glyph's own job: a pale owner-tinted fill inside the filled family's
dark outline, a pair that is self-balancing across a palette running from near-white ice
to near-black forest. **When a glyph proves illegible over some terrain, the fix is in the
glyph** — its weight, or an outline/halo on the stroke itself — never a plate reinstated
behind it. See [PLANETARY.md](PLANETARY.md) § Building markers.

### 1b. Landform glyphs — terrain shape, drawn on the canvases

A family apart from the entity markers above: these say what the tile *is*, not what is
*on* it. Accordingly they are **stroke-only** and none carries the filled family's dark
`outline` — a filled silhouette would read as "something is installed here", which is the
one thing a landform is not. One entry point, `landform(…, terrain_landform, colour)`,
dispatching by landform. The channel model is BL-231 (landform channels); the bridged
runs are BL-232 (bridged runs).

| Glyph | Function | Shape | Colour | Drawn for / where |
|---|---|---|---|---|
| **Mountain** | `landform(…, mountain, colour)` | Twin peaks sharing a saddle, open at the feet — no baseline, which is what separates it from the *filled* port triangle and the production up-triangle (both sit **on** a line) | Caller stroke — `ui::contrast_ink(fill)` | Unbuilt tile, Planetary canvas + Selection neighbourhood |
| **Canyon** | `landform(…, canyon, colour)` | Two level rim shoulders split by a narrow incision cutting below them; the gorge is the **gap**, and the level rims distinguish it from the Continent lens's diagonal seam | Caller stroke | As above |
| **Crater** | `landform(…, crater, colour)` | A flattened bowl — a wide, low ellipse with a raised near rim arc inside its lower half. The squashed aspect is load-bearing: it is deliberately **not** concentric circles (the `activity` pulse) nor a circle-plus-cross (the `market_centre`) | Caller stroke | As above |
| **Rift** | `landform(…, rift, colour)` | A single jagged fissure running top to bottom — the only zigzag in the vocabulary, so it cannot be read as a chevron (which meets at one point) or as the canyon's paired rims | Caller stroke | As above |

**Contiguous runs are bridged into one marker.** A tile with a same-landform cardinal
neighbour draws `landform_span(…)` toward each such neighbour instead of its centred glyph —
this tile's half of the shared edge, from centre to edge-midpoint, so the neighbour's half
meets it exactly and a run reads as **one** feature. This is the road span/symmetry idiom
(BL-172, road rendering) reused wholesale, including its survey-fog behaviour (a masked
neighbour draws nothing) and its centre-cap role, which the lone tile's centred glyph plays.

| Span | Shape | Echoes |
|---|---|---|
| **Mountain** | One peak per half-edge, all deflecting to the canonical side — a tile-to-tile span reads as two summits, a three-tile run as four | the twin-peak glyph |
| **Rift** | A jagged crack crossing its own axis at higher frequency, thinner stroke — the only span that crosses its axis, so it never reads as a ridge | the fissure glyph |
| **Canyon** | Two straight parallel rims with the gorge between | the paired-rim glyph |

**Crater never spans** — a basin is a blob, not a line, and its bowl glyph already says so.
Two constraints the implementation encodes: the waveform's perpendicular is **canonicalised**
rather than derived from the direction of travel, or the two halves of one span would deflect to
opposite sides and meet in a kink; and roads use this exact geometry in warm tan, so the spans
must stay non-smooth and take the contrasting ink or the map gains two look-alike span families.

The mountain profile is **one peak per half-edge**, not two: at four per span the teeth are
fine enough that a cluster reads as a jagged **outline** rather than a ridge — the one thing
bridging exists to fix.

**Plains, highland and valley draw nothing.** They are the common ground — plains and
valley alone measure ~95 % of land tiles (`world_audit` § S3) — and are carried by the
**relief tint** (`ui::landform_relief`), not by a glyph. Putting an icon on nearly every
tile would be far denser than any other glyph family and would fight the building
silhouette for the hex centre. See [CANVASES.md](CANVASES.md) § Terrain channels for the
two-channel split and why the relief composites *after* the lens tints.

Because the terrain palette spans near-white ice to dark forest, and any lens may
composite over it, these glyphs take their stroke from `ui::contrast_ink(fill)` — chosen
by the finished fill's luminance — rather than a fixed colour that would vanish somewhere
in that range.

### 1c. Named-building identity glyphs

`building(…, extraction_site | processing_facility, identity, fill)` takes a fourth
parameter — `resource_type identity` — that dispatches to a bespoke shape for the
**named** ancient buildings the Build door shows (BL-429, named buildings): an
extraction site's `identity` is its target resource, a processing facility's is its
recipe's **primary output** (`primary_output_resource`, `recipe_registry.hpp`).
Anything `identity` names with no bespoke shape below falls through to the family's
generic glyph (the extraction ore-chunk / the processing square) — always safe to pass.

**Two or more named buildings that reach the same resource share one glyph, by
design.** The glyph identifies WHAT a site makes or works, not which specific
recipe — the same way two Iron Mines on different tiles share one glyph
regardless of tile. So the Charcoal Burner and the Peat Kiln (both `-> charcoal`)
read the same, as do the Potter & Weaver and the Glassworks (both
`-> trade_goods_misc`), and Smithy/Miller share their glyph with the industrial
Smelter/Food Processor — it genuinely is the same steel, the same rations.

| Named building(s) | Resource key | Shape |
|---|---|---|
| Quarry | `stone` | An irregular five-sided boulder — taller and more asymmetric than the generic ore-chunk crystal |
| Woodcutter's Camp | `timber` | Two crossed logs with round end-caps — the only X silhouette in the building family |
| Sand Pit | `sand` | A low, wide dune with three grain dots above the crest |
| Clay Pit | `clay` | A narrow-necked raw vessel — a plain rim, unlike the tied goods bundle below |
| Peat Cutting | `peat` | Three stacked, alternately-offset turf bricks |
| Iron Mine | `iron_ore` | Crossed pick handles with small wedge heads |
| Copper Mine | `copper_ore` | A pointy-top hexagon nugget — the hub glyph's hexagon rotated 30° with its centre dot removed |
| Water Extractor | `water` | A teardrop — a pointed cap over a rounded base, the only rounded-and-pointed silhouette |
| Farm, Fishing Wharf | `agricultural_produce` | Three stalks fanning from a base point, each capped with a grain-head dot. Farm and Fishing Wharf (BL-168, fishing wharf) share the glyph — they work the same resource, distinguished by placement, not by what they visibly produce |
| Charcoal Burner, Peat Kiln | `charcoal` | A squat earthen mound with a smoke vent — the only dome silhouette |
| Bloomery | `iron_blooms` | A cluster of three rough lumps |
| Smithy (+ the industrial Smelter) | `steel` | A flat trapezoid ingot bar |
| Potter & Weaver, Glassworks | `trade_goods_misc` | A cinched sack — round body, tied neck |
| Miller (+ the industrial Food Processor) | `food_rations` | A rounded, strapped ration pack — two binding lines across a loaf shape |
| *(extraction target, no named building)* | `hides` | A raw hide stretched on a frame — a squat, irregular quadrilateral with four short peg lines at its corners |
| *(extraction target, no named building)* | `fibre` | A bundle of reed/flax stalks gathered and tied at the base — converging lines with a cinch band, distinct from the Farm's fanning grain-headed stalks |
| Tannery | `leather` | The cured counterpart of the raw hide glyph — a smoother, rounder silhouette with a single fold crease, no peg lines |
| Weaver | `cloth` | Three stacked fabric folds — a zigzag ribbon read as draped cloth |
| Shipwright | `rigging` | A coiled rope — two concentric rings sharing a gap, with a free end trailing out from the outer ring |

The remaining `k_extractable` targets outside the ancient roster's names — coal,
petroleum, silica, rare-earth ore, iron-nickel ore, platinum-group metals,
regolith — fall through to the generic ore-chunk: `extraction_building_name()`
(`selection_panel.cpp`) gives them a text name without a matching glyph (NR-239).

### 2. UI-affordance glyphs — drawn in chrome

| Glyph | Function | Shape | Colour | Used by |
|---|---|---|---|---|
| **Ledger** | `ledger(…, colour)` | Ruled-table outline (box + two rules) | Caller stroke | Nav rail slot 2 (Budget) |
| **Placeholder** | `placeholder(…, colour)` | Hollow rounded square | Caller stroke | Nav rail — fallback only; **no slot draws it** (see the legibility rule below) |
| **History** | `history(…, colour)` | Hourglass — a down-taper over an up-taper meeting at a centre waist, both ends capped; the *meeting* is what distinguishes it from the scarcity and production single triangles | Caller stroke | Nav rail slot 9 (History). Its own glyph, not a second `ledger`, which would make slot 9 indistinguishable from slot 2 (Budget) |
| **Research** | `research(…, colour)` | Branching tree — a stem rising to a fork, then two diagonals out to filled terminal nodes; the only branching glyph in the vocabulary | Caller stroke | Nav rail slot 4 (Research) |
| **Strategy** | `strategy(…, colour)` | Pennant on a pole — a vertical staff with a filled right-triangle flag at its head; the flag hangs off the staff top rather than resting on a baseline, so it stays clear of the production up-triangle | Caller stroke + fill | Nav rail slot 7 (Corp. Strategy) — **reserved slot**, drawn dim; borrowed lit by slot 11 (AI decisions), whose subject is exactly the strategic decision slot 7 is reserved for |
| **Diplomacy** | `diplomacy(…, colour)` | Two overlapping circle outlines — a two-parties-meeting motif; the overlap is the point, so it never reads as the single market-centre circle or the concentric activity pulse | Caller stroke | Nav rail slot 8 (Diplomacy) |
| **Readout** | `readout(…, colour)` | A left axis stroke with three left-anchored horizontal tally bars of descending length — "counts compared". The axis anchor keeps it distinct from the supply route pair and the resource strata (both anchorless); the horizontal bars keep it clear of the market lens's vertical ones | Caller stroke + fill | Nav rail slot 12 (Strategy readout, BL-411, emergent strategy readout) — the feed's aggregate companion; its own glyph rather than a second borrow of `strategy`, since slot 11 already lights that pennant and two lit slots must not share a silhouette |
| **Contract** | `contract(…, colour)` | A page with its top-right corner cut off (a dog-ear fold) plus a short check mark near the bottom — a signed-document motif. The fold distinguishes it from `ledger` (a plain ruled box, no fold); the rectangle baseline distinguishes it from `history`'s hourglass | Caller stroke | Nav rail slot 13 (Contracts ledger, BL-576) |
| **Acquisition** | `acquisition(…, colour)` | Two OUTLINED squares of different sizes with a short arrow running from the small one INTO the large one, its head ending *inside* the acquirer rather than on its edge — "one firm absorbed whole". The PAIR is the silhouette: no other rail slot draws two boxes, which is what keeps it clear of `corporation` (one *filled* square with a centred dot), `ledger` (one ruled box) and `industry` (the factory). The arrow gives the pair a direction, because a buyout has a buyer and a target and a symmetric pair would read as a merger — which the model does not have. **The head must end inside the large square**: the first draft landed it on the outline and the stroke swallowed it at rail radius, collapsing the glyph into one blob near enough to the corporation seal to be the very collision this catalogue exists to prevent | Caller stroke | Nav rail slot 5 (Acquisitions ledger) |

The rail's other slots borrow lens and marker glyphs: slot 1 (Corporation overview) draws
`corporation`, slot 3 (Construction) `industry`, slot 6 (Market Ledger) `market`,
slot 10 (Generation Ledger) `continent` — see MENU.md.

**A Convoys ledger draws `convoy`, which already exists** (§ 1, entity markers). Convoys leaving
the Market ledger for a surface of their own (Ben, 2026-08-29) therefore needs no new rail glyph —
the marker the canvas already draws for cargo in transit is the slot's silhouette, which is the
pairing this catalogue prefers wherever a slot has a canvas twin. Its rail position is a MENU.md
curation question and not an icon one.

### 2b. The item glyph — a value-track silhouette, not one shape per good

> **PLACEHOLDERS UNTIL A LATER SPRINT** (Ben, 2026-08-29: *"For flags and glyphs, use
> placeholders. We will come to that in a different sprint later."*). The four track silhouettes
> and the nation chips below are the **settled design**; the Market rework ships against
> placeholders and does not wait on the art. Two conditions on that, both from the
> honest-placeholder idiom (NR-249): a placeholder must be *visibly* provisional rather than a
> shape a player would learn as meaning something, and the column must not be sized to the
> placeholder — reserve the width the real glyph will need, or the table gets re-laid out twice.
> The existing `resource` pip is the natural stand-in for the item glyph, since it is already
> colour-keyed and already means "a resource"; it simply does not yet discriminate.

The Market ledger's Goods table takes an **item glyph** column (Ben, 2026-08-29:
*"item_glyph; name; price; body_average_price; price_relative_to_base_price;
six_month_price_graph"*). What exists today is `resource(…, resource_type)` — a small filled
**diamond in the resource's identity colour**, one shape for every good. Drawn as a table column
across ~42 rows that is forty-two identical diamonds, discriminating by hue alone, immediately
beside the resource's own name in that same hue. It would be decoration duplicating the column
next to it.

**Author four silhouettes keyed to the VALUE TRACK, not one per good.** `RESOURCES.md`
§ Resource categories already divides the roster into **Industrial / Ambient / Habitability /
Mercantile**, and that division is a real thing a player acts on — it says what a good is *for*.
Four shapes in the resource's identity colour give a glyph that discriminates at row height and
teaches a distinction the game already has, where forty-two would be a bank of art nobody can
hold in mind and a colour-only pip would say nothing the name does not.

**Production tier is the shading, if anything.** The three tiers (raw / refined / product) are an
ordered axis, so they belong on an ordered channel — fill weight, not silhouette. Optional; the
track alone is the load-bearing half.

The existing `resource` pip is **not** replaced: it stays the deposit-marker and resource-strip
mark, where one small colour-keyed dot is exactly right and no track distinction is wanted. The
item glyph is a table-column glyph and a second entry, on the same rule that gave `readout` its
own shape rather than a second borrow of `strategy`.

**No nation flags exist, and the presence row must not imply otherwise.** A "list of nations who
operate in that market" (Ben, same day) has `nation_colour(entity_id)` behind it — a per-nation
palette colour — and nothing else. Corps carry an emblem tag; nations carry a colour. The row is
therefore **coloured chips** with the nation's initials, named on hover. Real per-nation emblem
artwork would be a generated identity system, which is a feature of its own and not a row on a
ledger; do not stub one glyph and call it a flag.

**Nav-rail legibility rule (BL-174, nav-rail legibility).** Every rail slot draws its **own**
glyph — the shape says *which system the slot is for*, and **colour alone** carries availability
(the bright stroke for a live slot, the dim stroke for a reserved one). A column of identical
hollow squares tells a new player nothing; a tooltip cannot be seen without hovering, and never
appears in a capture. A slot with no glyph *and* no tooltip is uninterpretable and does not exist
on the rail. Reserved slots stay `BeginDisabled`.

### 3. Map-lens glyphs — the overlay control strip

| Glyph | Function | Shape | Colour | Lens |
|---|---|---|---|---|
| **Supply** | `supply(…, colour)` | Two parallel horizontal lines (route shorthand) | Caller stroke | `overlay_mode::supply`; reused for `overlay_mode::supply_routes` |
| **Market** | `market(…, colour)` | Three ascending bars (price chart), outlined | Caller stroke | `overlay_mode::market` |
| **Country** | `country(…, colour)` | Downward-pointing shield silhouette + dark outline | Caller fill | `overlay_mode::country` |
| **Corporation** | `corporation(…, colour)` | Filled square + dark inner dot ("seal") | Caller fill | `overlay_mode::corporation` |
| **Resource** | `resource(…, colour)` | Three stacked horizontal strata, widening + deepening top-to-bottom (gradient / density motif) | Caller fill (per-stratum alpha) | `overlay_mode::resource` |
| **Population** | `population(…, colour)` | Small figure: round head over a tapered torso (people / habitability motif) | Caller fill | `overlay_mode::population` |
| **Opportunity** | `opportunity(…, colour)` | Open circle with an inner "+" (stroke only) — a "potential gain / margin" motif (where value could be made) | Caller stroke | `overlay_mode::opportunity` |
| **Production** | `production(…, colour)` | Filled upward-pointing triangle over a short baseline (output / throughput rising motif); distinct from the market bars and the scarcity hollow down-triangle | Caller fill | `overlay_mode::production` |
| **Scarcity** | `scarcity(…, colour)` | Hollow downward-pointing triangle (empty / depleted motif; inverse of the filled resource pip) | Caller stroke | `overlay_mode::scarcity` |
| **Industry** | `industry(…, colour)` | Factory silhouette — a filled body block with a two-tooth sawtooth roof and a left chimney rising above the roofline; distinct from the production up-triangle and the market bars (BL-084, industry lens) | Caller fill | `overlay_mode::industry` |
| **Continent** | `continent(…, colour)` | Two filled, deliberately asymmetric quads split by a diagonal **gap** — the seam is the shape that carries the meaning, and it is a gap rather than a drawn hairline so it survives at strip size. Reads "the crust is in pieces, and this is where they meet"; distinct from the Country shield (a bordered *territory*) and from any solid landmass blob, because the lens shows the *boundary*, not the area (BL-226, continent lens) | Caller fill | `overlay_mode::continent` |
| **Reach** | *(borrows `convoy`)* | The rightward chevron | Caller stroke | `overlay_mode::reach` |
| **Throughput** | `throughput(…, colour)` | A **truck** in profile facing right — long cargo box, stepped-down cab with a raked windscreen, two wheels on the axle line; filled with the family's dark outline, hubs picked out in the outline colour so the undercarriage survives at strip size. Ben, 2026-08-25: *"just use a truck as the glyph."* Two abstract cuts were tried first and both failed at ~21px — a funnel narrowing to a node read as a bowtie (an X, already the *closed* affordance here) and a ringed node with flow stubs read as a lone ring; a truck needs no decoding, which beats metaphorical fidelity on a strip. Distinct from `convoy` (a bare chevron) and `supply` (two parallels) by being a *thing* rather than a mark, which matters because it borrowed the convoy chevron while it was keyboard-only (BL-605) | Caller fill | `overlay_mode::throughput` |

> **Three rows above are stale** and are left rather than quietly deleted: **Country**,
> **Opportunity** and **Production** name lenses Sprint 17b retired. The glyph functions
> still exist in `icons.cpp`; what no longer exists is the lens each one claims. Whether
> the functions go with them is a separate call from whether the lenses did.

In the strip ([`overlay.cpp`](../../src/ui/overlay.cpp), `draw_overlay_controls`)
each lens is an invisible button with its glyph drawn over the rect; the active
lens gets a highlighted backing and the `palette::selection` glyph colour, inactive
lenses use `palette::neutral`. The lens **name** is supplied as a hover tooltip via
`overlay_mode_name`.

The curated on-screen strip order is corporation / resource / market /
population / continent / throughput; scarcity, industry, supply, reach
and supply-routes are keyboard-cycle only (a width call, MINIMAP.md § Overlay
controls). Reach and Supply-routes **borrow** the `convoy` / `supply` glyphs rather
than carrying their own — adequate off-strip, where the glyph is never seen beside
its lender. Note `resource` is **overloaded**: `resource(…, resource_type)` is the
identity-coloured *pip* (a diamond), while `resource(…, ImU32)` is the *lens* glyph
(the strata motif) — same name, disambiguated by the final argument type and by
context (strip vs. canvas pip).

---

## Adding a new glyph

1. Declare it in `icons.hpp` with a `///` Doxygen block following the existing
   contract (`@param dl/centre/r/colour`), naming the shape and where it is used.
2. Implement in `icons.cpp`, designed to fit the `2r × 2r` box and centred on
   `centre`. Reuse the file-local `diamond` / `square` / `triangle` helpers and the
   `outline` constant where it fits the family.
3. Keep the **silhouette distinct** from existing glyphs in the same family — a
   glance should disambiguate without relying on colour alone (see the settled
   collisions below).
4. Add a row to the catalogue above and, for a lens glyph, cross-reference
   LENSES.md.

---

## Settled collisions and rules

Design calls that shaped the vocabulary, kept because each one is a rule a new glyph
must still obey:

1. **No two filled diamonds (2026-06-15).** The *extraction-site* building marker is a
   **faceted ore/mineral silhouette** so it never reads as the gem-diamond *resource pip*.
   The pip keeps the diamond (gem = resource).

2. **No two triangles (2026-06-15).** The *port* is the only filled upward triangle among
   the markers; anything chevron-shaped is an **open** chevron (the convoy's rightward →),
   and the production lens's up-triangle sits *on a baseline* the port lacks.

3. **Outline rule (2026-06-15).** *Every canvas-placed filled marker carries the dark
   outline*; the resource **pip** is the documented exception (strip/swatch glyph,
   outline-less). See § Shared conventions.

4. **Fill-vs-stroke is fixed per family (2026-06-15).** Documented in § Shared
   conventions.

5. **A marker never paints its own background (2026-08-24).** Ben: *"Remove building
   background. Buildings should be drawn over the hex, not completely on top."* No
   canvas-placed glyph fills a plate behind itself; the ground it stands on keeps
   rendering. Contrast is carried by the family's outline rule (3 above) and, for stroke
   families, by the shadow-then-colour pass. A glyph that cannot hold its shape over some
   terrain is a glyph to redraw, not a glyph to back.

6. **The rim belongs to borders — a mark placed there must not read as one (2026-08-24).**
   The hex rim already carries the province edge stroke, the player-footprint outline and
   the Country lens's nation-border segments, all of which are **hexagonal, continuous and
   thin**. The `stack_ring` earns its place there by being none of those: a **circle**,
   **inset** clear of the edges and their midpoints, **broken** by a gap between every
   segment, and **thicker**. Any future rim mark owes the same four-way separation.

7. **Nation tint vs. corp emblem.** The political layer conveys nations by tile *tint*
   (`palette::nation_colour`), glyph-less. Corporations have a dedicated **`corp_emblem`**
   glyph (shape + identity colour, both a pure function of the corp id) rendered wherever a
   corp is *identified* — the identity card, the Selection header, a small tag beside each
   building marker, and the rival hover card — in addition to the building *fill* that
   encodes ownership on the silhouette itself. The colour source is unified: card, markers,
   and canvas tint all read `palette::corp_identity_colour`.
