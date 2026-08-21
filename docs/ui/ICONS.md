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
(the summaries that reuse swatches), and the **lens-design** item in
`docs/development/BACKLOG.md` (the lens glyphs below feed it).

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
- **trailing parameters** — most glyphs take a single colour; the two
  multi-parameter cases are `building` (a `building_type` + a `resource_type`
  identity, BL-429 § 1c + `fill`) and `settlement` (a `tier` + `colour`).

Two settled visual sub-conventions (the resolution of former Open clarifications 3–4):

- **Every canvas-placed filled marker carries the dark outline** (`IM_COL32(20, 22, 28, 255)`,
  the file-local `outline`) so it reads on any terrain colour — this covers the whole entity-marker
  family (`building`). The resource **pip** is the single documented exception: as a
  strip/swatch/deposit glyph it stays **outline-less**.
- **`colour` means fill or stroke per family, fixed:** the filled families
  (`building`, `country`, `corporation`, resource `pip`, `settlement`, `industry`) treat
  `colour` as the **fill**; the stroke families (`supply`, `convoy`, `market`, `ledger`,
  `placeholder`, resource-**lens**) treat it as the **stroke** line colour and have no fill;
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
| **Inland logistics hub** | `building(…, inland_logistics_hub, identity, fill)` | Filled flat-top hexagon + outline with a small dark hub dot at the centre — a six-sided network-node silhouette (BL-149), distinct from the launchpad/none circle and the port triangle | Caller `fill` | Building marker, Planetary canvas; build-front-door row |
| **Military base** | `building(…, military_base, identity, fill)` | Filled shield + outline — flat top, shoulders tapering to a bottom point (BL-325 S1); the martial building, **filled** like every building glyph, so it never reads as the port's upward triangle or the hub's hexagon | Caller `fill` | Building marker, Planetary canvas; build-front-door row |
| **Building (none/other)** | `building(…, none, identity, fill)` | Filled circle (dot) — also the fallback for `launchpad` | Caller `fill` | Fallback building marker |
| **Resource pip** | `resource(…, res)` | Filled diamond (no outline) | **Derived** — `presentation_of(res).colour` | Resource strips, deposit markers |
| **Under construction** | `under_construction(…, colour)` | Crane silhouette — a mast, an angled boom, a back-stay brace, and a short hook line, four strokes with the same shadow-then-colour pass as `convoy`; stroke-only (BL-327), echoing the landform family's "not yet installed" convention — a filled glyph would claim the site already IS its type | Caller `colour` (the owner-tinted marker colour) | Building marker, Planetary canvas — drawn IN PLACE OF the type silhouette while `ticks_remaining > 0`; replaced the BL-323 S4 desaturation treatment same-day (dimming read as "faded", not "being built") |
| **Convoy** | `convoy(…, colour)` | Rightward chevron (→) — two stroke lines meeting at a right point (goods in transit); points *right*, distinct from the filled port triangle (the old upward *unit* chevron was deleted uncalled, BL-294; a unit marker returns with BL-157) | Caller stroke | Supply-lens on-canvas convoy marker — drawn on tiles a player convoy passes through, Planetary canvas |
| **Market centre** | `market_centre(…, colour)` | Circle outline with a centred cross (+); arms reach 60 % of the radius | Caller stroke | Market-centre marker, Planetary canvas |
| **Settlement** | `settlement(…, tier, colour)` | Tiered skyline — 1–5 filled towers (count = `tier`, clamped) on a baseline + per-tower outline, the middle tallest, heights tapering to the edges; an outpost reads as a lone tower, a metropolis as a dense cluster | Caller `colour` — civic-neutral `palette::settlement` (parchment); host-nation tint (`palette::nation_colour`) only under the Country lens | Population-centre conurbation marker, Planetary canvas (BL-083) |
| **Unknown** | `unknown(…, colour)` | Question mark — a top hook arc, a short stem, and a dot | Caller stroke (dimmed) | Survey badge for an **unsurveyed** body, Solar canvas (BL-067) |
| **Survey badge** | `survey_badge(…, colour)` | Magnifying glass — a lens circle with a diagonal handle (scan motif) | Caller stroke | Survey badge for an **in-progress** survey, Solar canvas; the canvas overlays a `k∕N` region count (BL-067) |
| **HQ** | `hq(…, colour)` | Ringed eight-point star — a diamond overlaid with an axis-aligned square, enclosed by a ring, with a dark centre dot so it reads against a same-colour ownership fill | Caller `colour` (the player identity colour) | The player's HQ/origin building, Planetary canvas (BL-085, folding BL-092) |
| **Corp emblem** | `corp_emblem(…, shape, fill)` | One of six geometric primitives (circle / square / triangle / diamond / hexagon / pentagon) chosen by `shape`; names *whose* an entity is, not *what* it is | Caller `fill` — the corp's identity colour (`palette::corp_identity_colour(corp, player)`); `shape` from `palette::corp_emblem_shape(corp)` | Faction-identity emblem (BL-090): the identity card portrait, the Selection-panel header (corporation + owned/rival building), a small identity tag beside each building marker (player **and** rival) on the Planetary canvas, and the rival hover card. The shared promotion of the former profile-card-only `draw_corp_emblem` |
| **Activity** | `activity(…, colour)` | Concentric pulse — a filled core ringed by a signal ring (commercial-beacon motif; deliberately distinct from the survey magnifier and the unknown "?") | Caller `colour` — per activity tier (`palette::activity_known` / `activity_stale` / `activity_visible`) | Commercial-activity fog badge, Solar canvas — lower-left of the body, offset from the survey badge's upper-right so the two fogs read apart (BL-089; see [DISCOVERY.md](DISCOVERY.md)) |
| **Value mark** | `value_mark(…, colour)` | Single filled dot | Caller fill — the caller's red→green ramp sample (`ryg_colour`) | Per-tile magnitude mark for the Workforce (Population) and Opportunity lenses (BL-135, landed 2026-07-09): drawn on every buildable tile while either lens is active, replacing both the old full-tile tint and, on occupied tiles, the building glyph. See LENSES.md § Population / § Opportunity |
| **Battle** | `battle(…, colour)` | Two crossed blades, each a stroked shaft with a short cross-guard bar near its base, meeting at the centre. Deliberately **not** a bare X: the X is already the "close this" affordance throughout the chrome, and a mark meaning *a fight is here* must never read as a button meaning *dismiss this*. Stroke-only — a battle is an event on the ground, not a thing installed on it | Caller stroke | BL-469: drawn on the **province anchor tile only**, once per live battle, Planetary canvas. Province-grain by ruling (BL-467 ruling 1 — the province frames the fight), so scattering it across every participating tile would say the opposite |
| **Stack-count badge** | inlined in `body_surface_canvas.cpp` (no shared `icons::` helper yet) | A dark-filled circle carrying `+N` text (N = additional buildings beyond the dominant one) — the same k/N text-overlay idiom the Solar-canvas survey badge uses for region progress, not a new glyph shape | Fixed light text on a dark disc, no owner tint | BL-367: on a built tile carrying more than one building, staggered lower-right past the BL-090 corp-identity tag (which also sits lower-right) so the two never overlap. Only the dominant (lowest-id) building's silhouette still draws — this badge is what tells a stacked tile apart from a single-building one, since no per-stack marker exists |

On the Planetary canvas the **building** glyph's `fill` now encodes the *owning
corporation* (player corp = corp slot 0; rivals a hashed slot), so the
silhouette reads the building **type** and the fill reads **who owns it**.

### 1b. Landform glyphs — terrain shape, drawn on the canvases

A family apart from the entity markers above: these say what the tile *is*, not what is
*on* it. Accordingly they are **stroke-only** and none carries the filled family's dark
`outline` — a filled silhouette would read as "something is installed here", which is the
one thing a landform is not. One entry point, `landform(…, terrain_landform, colour)`,
dispatching by landform (the third multi-parameter case, alongside `building` and
`settlement`).

| Glyph | Function | Shape | Colour | Drawn for / where |
|---|---|---|---|---|
| **Mountain** | `landform(…, mountain, colour)` | Twin peaks sharing a saddle, open at the feet — no baseline, which is what separates it from the *filled* port triangle and the production up-triangle (both sit **on** a line) | Caller stroke — `ui::contrast_ink(fill)` | Unbuilt tile, Planetary canvas + Selection neighbourhood |
| **Canyon** | `landform(…, canyon, colour)` | Two level rim shoulders split by a narrow incision cutting below them; the gorge is the **gap**, and the level rims distinguish it from the Continent lens's diagonal seam | Caller stroke | As above |
| **Crater** | `landform(…, crater, colour)` | A flattened bowl — a wide, low ellipse with a raised near rim arc inside its lower half. The squashed aspect is load-bearing: it is deliberately **not** concentric circles (the `activity` pulse) nor a circle-plus-cross (the `market_centre`) | Caller stroke | As above |
| **Rift** | `landform(…, rift, colour)` | A single jagged fissure running top to bottom — the only zigzag in the vocabulary, so it cannot be read as a chevron (which meets at one point) or as the canyon's paired rims | Caller stroke | As above |

**Contiguous runs are bridged into one marker (BL-232).** A tile with a same-landform
cardinal neighbour draws `landform_span(…)` toward each such neighbour instead of its centred
glyph — this tile's half of the shared edge, from centre to edge-midpoint, so the neighbour's
half meets it exactly and a run reads as **one** feature. This is BL-172's road span/symmetry
idiom reused wholesale, including its survey-fog behaviour (a masked neighbour draws nothing)
and its centre-cap role, which the lone tile's centred glyph now plays.

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

An earlier mountain profile put *two* spikes in each half. At four per span the teeth were fine
enough that a cluster read as a jagged **outline** rather than a ridge — the one thing bridging
exists to fix — so the profile was cut to a single peak per half-edge.

**Plains, highland and valley draw nothing.** They are the common ground — plains and
valley alone measure ~95 % of land tiles (`world_audit` § S3) — and are carried by the
**relief tint** (`ui::landform_relief`), not by a glyph. Putting an icon on nearly every
tile would be far denser than any other glyph family and would fight the building
silhouette for the hex centre. See [CANVASES.md](CANVASES.md) § Terrain channels for the
two-channel split and why the relief composites *after* the lens tints (BL-231).

Because the terrain palette spans near-white ice to dark forest, and any lens may
composite over it, these glyphs take their stroke from `ui::contrast_ink(fill)` — chosen
by the finished fill's luminance — rather than a fixed colour that would vanish somewhere
in that range.

### 1c. Named-building identity glyphs (BL-429 slice 2)

`building(…, extraction_site | processing_facility, identity, fill)` takes a fourth
parameter — `resource_type identity` — that dispatches to a bespoke shape for the
**named** ancient buildings the Build door now shows (BL-429): an extraction site's
`identity` is its target resource, a processing facility's is its recipe's
**primary output** (`primary_output_resource`, `recipe_registry.hpp`). Anything
`identity` names with no bespoke shape below falls through to the family's generic
glyph (the extraction ore-chunk / the processing square) — always safe to pass.

**Two or more named buildings that reach the same resource share one glyph, by
design.** The glyph identifies WHAT a site makes or works, not which specific
recipe — the same way two Iron Mines on different tiles already share one glyph
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
| Farm, Fishing Wharf | `agricultural_produce` | Three stalks fanning from a base point, each capped with a grain-head dot. Farm and Fishing Wharf (BL-168) share the glyph — they work the same resource, distinguished by placement, not by what they visibly produce |
| Charcoal Burner, Peat Kiln | `charcoal` | A squat earthen mound with a smoke vent — the only dome silhouette |
| Bloomery | `iron_blooms` | A cluster of three rough lumps |
| Smithy (+ the industrial Smelter) | `steel` | A flat trapezoid ingot bar |
| Potter & Weaver, Glassworks | `trade_goods_misc` | A cinched sack — round body, tied neck |
| Miller (+ the industrial Food Processor) | `food_rations` | A rounded, strapped ration pack — two binding lines across a loaf shape |

**Not yet covered by a bespoke shape** (falls through to the generic ore-chunk):
the remaining `k_extractable` targets outside the ancient roster's names — coal,
petroleum, silica, rare-earth ore, iron-nickel ore, platinum-group metals,
regolith — since `extraction_building_name()` (`selection_panel.cpp`) gives them a
text name without a matching glyph. Extending the shape table to match is a
natural follow-on, not attempted this pass (NR-239).

### 2. UI-affordance glyphs — drawn in chrome

| Glyph | Function | Shape | Colour | Used by |
|---|---|---|---|---|
| **Ledger** | `ledger(…, colour)` | Ruled-table outline (box + two rules) | Caller stroke | Nav rail — slots that open a ledger window |
| **Placeholder** | `placeholder(…, colour)` | Hollow rounded square | Caller stroke | Nav rail — fallback only; **no slot draws it since BL-174** (see below) |
| **History** | `history(…, colour)` | Hourglass — a down-taper over an up-taper meeting at a centre waist, both ends capped; the *meeting* is what distinguishes it from the scarcity and production single triangles | Caller stroke | Nav rail slot 9 (History). Replaced a second `ledger` glyph, which made slot 9 indistinguishable from slot 2 (Budget) |
| **Research** | `research(…, colour)` | Branching tree — a stem rising to a fork, then two diagonals out to filled terminal nodes; the only branching glyph in the vocabulary | Caller stroke | Nav rail slot 4 (Research) — **reserved slot**, drawn dim |
| **Strategy** | `strategy(…, colour)` | Pennant on a pole — a vertical staff with a filled right-triangle flag at its head; the flag hangs off the staff top rather than resting on a baseline, so it stays clear of the production up-triangle | Caller stroke + fill | Nav rail slot 7 (Corp. Strategy) — **reserved slot**, drawn dim |
| **Diplomacy** | `diplomacy(…, colour)` | Two overlapping circle outlines — a two-parties-meeting motif; the overlap is the point, so it never reads as the single market-centre circle or the concentric activity pulse | Caller stroke | Nav rail slot 8 (Diplomacy) — **reserved slot**, drawn dim |
| **Readout** | `readout(…, colour)` | A left axis stroke with three left-anchored horizontal tally bars of descending length — "counts compared". The axis anchor keeps it distinct from the supply route pair and the resource strata (both anchorless); the horizontal bars keep it clear of the market lens's vertical ones | Caller stroke + fill | Nav rail slot 12 (Strategy readout, BL-411) — the feed's aggregate companion; its own glyph rather than a third borrow of `strategy`, since slot 11 already lights that pennant and two lit slots must not share a silhouette (BL-174) |

**Nav-rail legibility rule (BL-174).** Every rail slot draws its **own** glyph — the shape says
*which system the slot is for*, and **colour alone** carries availability (the bright stroke for a
live slot, the dim stroke for a reserved one). Before BL-174 the four reserved slots (Workforce,
Research, Corp. Strategy, Diplomacy) all drew the same hollow `placeholder` square, so a new player
saw a column of identical blanks and could not tell what any of them was for; the tooltips already
named them ("Research (coming)", …), but a tooltip cannot be seen without hovering, and never
appears in a capture. Workforce reuses the existing `population` figure; the other three got the
glyphs above. The former slot 10 — a disabled square with no glyph *and* no tooltip — was
**removed**, since nothing about it was interpretable. Reserved slots stay `BeginDisabled`.

### 3. Map-lens glyphs — the overlay control strip

| Glyph | Function | Shape | Colour | Lens |
|---|---|---|---|---|
| **Supply** | `supply(…, colour)` | Two parallel horizontal lines (route shorthand) | Caller stroke | `overlay_mode::supply` |
| **Market** | `market(…, colour)` | Three ascending bars (price chart), outlined | Caller stroke | `overlay_mode::market` |
| **Country** | `country(…, colour)` | Downward-pointing shield silhouette + dark outline | Caller fill | `overlay_mode::country` |
| **Corporation** | `corporation(…, colour)` | Filled square + dark inner dot ("seal") | Caller fill | `overlay_mode::corporation` |
| **Resource** | `resource(…, colour)` | Three stacked horizontal strata, widening + deepening top-to-bottom (gradient / density motif) | Caller fill (per-stratum alpha) | `overlay_mode::resource` |
| **Population** | `population(…, colour)` | Small figure: round head over a tapered torso (people / habitability motif) | Caller fill | `overlay_mode::population` |
| **Opportunity** | `opportunity(…, colour)` | Open circle with an inner "+" (stroke only) — a "potential gain / margin" motif (where value could be made) | Caller stroke | `overlay_mode::opportunity` |
| **Production** | `production(…, colour)` | Filled upward-pointing triangle over a short baseline (output / throughput rising motif); distinct from the market bars and the scarcity hollow down-triangle | Caller fill | `overlay_mode::production` |
| **Scarcity** | `scarcity(…, colour)` | Hollow downward-pointing triangle (empty / depleted motif; inverse of the filled resource pip) | Caller stroke | `overlay_mode::scarcity` |
| **Industry** | `industry(…, colour)` | Factory silhouette — a filled body block with a two-tooth sawtooth roof and a left chimney rising above the roofline; distinct from the production up-triangle and the market bars (BL-084) | Caller fill | `overlay_mode::industry` |
| **Continent** | `continent(…, colour)` | Two filled, deliberately asymmetric quads split by a diagonal **gap** — the seam is the shape that carries the meaning, and it is a gap rather than a drawn hairline so it survives at strip size. Reads "the crust is in pieces, and this is where they meet"; distinct from the Country shield (a bordered *territory*) and from any solid landmass blob, because the lens shows the *boundary*, not the area (BL-226) | Caller fill | `overlay_mode::continent` |

In the strip ([`overlay.cpp`](../../src/ui/overlay.cpp), `draw_overlay_controls`)
each lens is an invisible button with its glyph drawn over the rect; the active
lens gets a highlighted backing and the `palette::selection` glyph colour, inactive
lenses use `palette::neutral`. The lens **name** is supplied as a hover tooltip via
`overlay_mode_name`.

---

## Adding a new glyph

1. Declare it in `icons.hpp` with a `///` Doxygen block following the existing
   contract (`@param dl/centre/r/colour`), naming the shape and where it is used.
2. Implement in `icons.cpp`, designed to fit the `2r × 2r` box and centred on
   `centre`. Reuse the file-local `diamond` / `square` / `triangle` helpers and the
   `outline` constant where it fits the family.
3. Keep the **silhouette distinct** from existing glyphs in the same family — a
   glance should disambiguate without relying on colour alone (see the diamond
   collision under Open clarifications).
4. Add a row to the catalogue above and, for a lens glyph, cross-reference the
   lens-design doc.

---

## Open clarifications

Things the current set leaves ambiguous — resolve these as the icon vocabulary is
firmed up (several feed the **lens-design** Brief):

1. **Diamond overload — RESOLVED (2026-06-15): redraw extraction-site.** The *extraction-site*
   building marker gets a distinct **faceted ore/mineral silhouette** so it never reads as the
   gem-diamond *resource pip*. The pip keeps the diamond (gem = resource). Glyph redraw in
   `icons.cpp` lands with the [C2] icon-collision Brief.

2. **Triangle overload + contract mislabel — RESOLVED (2026-06-15): unit → true chevron.** The
   *unit/convoy* marker is redrawn as a true **open upward chevron (V)**, separating it from the
   filled *port* triangle and matching the header's existing "chevron" wording (so the contract
   and implementation agree). Redraw lands with the [C2] icon-collision Brief.

3. **Outline rule — RESOLVED (2026-06-15).** *Every canvas-placed filled marker carries the dark
   outline* (so `unit` gains it); the resource **pip** is the documented exception (strip/swatch
   glyph, outline-less). See § Shared conventions.

4. **Fill-vs-stroke — RESOLVED (2026-06-15): fixed per family.** Documented in § Shared
   conventions: `building`/`country`/`corporation`/`unit`/`pip` → `colour` is fill;
   `supply`/`market`/`ledger`/`placeholder`/resource-lens → stroke.

5. **The lens set is now complete.** The curated on-screen strip order is
   corporation / country / resource / market / population / **opportunity** /
   **production** / **continent** (**Continent** joined with BL-226, 2026-07-30,
   as the eighth on-screen glyph); scarcity and **industry** are keyboard-cycle
   only (trimmed off the strip by BL-093 — Industry the same day BL-084 shipped
   it). The set is ratified in [LENSES.md](LENSES.md) and all are catalogued
   above (`supply` exists but is off the strip; Reach and Supply-routes reuse
   the `convoy`/`supply` glyphs — dedicated glyphs are an open TODO,
   `overlay.cpp` note, BL-011/BL-014). Note `resource` is **overloaded**:
   `resource(…, resource_type)` is the identity-coloured *pip* (a diamond), while
   `resource(…, ImU32)` is the *lens* glyph (the strata motif) — same name,
   disambiguated by the final argument type and by context (strip vs. canvas pip).

6. **Nation tint vs. corp emblem — UPDATED (2026-07-05, BL-090).** The political
   layer conveys nations by tile *tint* (`palette::nation_colour`), still glyph-less.
   Corporations now have a dedicated **`corp_emblem`** glyph (shape + identity colour,
   both a pure function of the corp id) rendered wherever a corp is *identified* — the
   identity card, the Selection header, a small tag beside each building marker, and
   the rival hover card — in addition to the building *fill* (`palette::corp_colour`)
   that still encodes ownership on the silhouette itself. The colour source is
   unified: card, markers, and canvas tint all read `palette::corp_identity_colour`.
