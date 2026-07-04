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
  multi-parameter cases are `building` (a `building_type` + `fill`) and
  `settlement` (a `tier` + `colour`).

Two settled visual sub-conventions (the resolution of former Open clarifications 3–4):

- **Every canvas-placed filled marker carries the dark outline** (`IM_COL32(20, 22, 28, 255)`,
  the file-local `outline`) so it reads on any terrain colour — this covers the whole entity-marker
  family (`building`, `unit`). The resource **pip** is the single documented exception: as a
  strip/swatch/deposit glyph it stays **outline-less**.
- **`colour` means fill or stroke per family, fixed:** the filled families
  (`building`, `country`, `corporation`, `unit`, resource `pip`, `settlement`, `industry`) treat
  `colour` as the **fill**; the stroke families (`supply`, `convoy`, `market`, `ledger`,
  `placeholder`, resource-**lens**) treat it as the **stroke** line colour and have no fill;
  `hq` and `activity` span both — the one `colour` fills the core *and* strokes the ring.

---

## Catalogue

Glyphs fall into three families by role.

### 1. Entity markers — drawn on the canvases

| Glyph | Function | Shape | Colour | Drawn for / where |
|---|---|---|---|---|
| **Extraction site** | `building(…, extraction_site, fill)` | Faceted ore/mineral silhouette + outline — an angular eight-sided crystal chunk (wider than tall, corner-cut facets), distinct from the regular gem-diamond pip and the port/unit glyphs | Caller `fill` | Building marker, Planetary canvas |
| **Processing facility** | `building(…, processing_facility, fill)` | Filled square + outline | Caller `fill` | Building marker, Planetary canvas |
| **Port** | `building(…, port, fill)` | Filled upward triangle + outline | Caller `fill` | Building marker, Planetary canvas |
| **Building (none/other)** | `building(…, none, fill)` | Filled circle (dot) | Caller `fill` | Fallback building marker |
| **Resource pip** | `resource(…, res)` | Filled diamond (no outline) | **Derived** — `presentation_of(res).colour` | Resource strips, deposit markers |
| **Unit / convoy** | `unit(…, colour)` | Open upward chevron (V) — two stroke lines meeting at a bottom point, open at the top; drawn with a dark 2 px shadow pass then a 1.5 px colour pass; stroke-only so it never reads as the filled port triangle | Caller `colour` (e.g. a corp colour) | Unit stacks, Layer 5 convoy heads |
| **Convoy** | `convoy(…, colour)` | Rightward chevron (→) — two stroke lines meeting at a right point (goods in transit); points *right*, never confusable with the *unit* marker's upward chevron | Caller stroke | Supply-lens on-canvas convoy marker — drawn on tiles a player convoy passes through, Planetary canvas |
| **Market centre** | `market_centre(…, colour)` | Circle outline with a centred cross (+); arms reach 60 % of the radius | Caller stroke | Market-centre marker, Planetary canvas |
| **Settlement** | `settlement(…, tier, colour)` | Tiered skyline — 1–5 filled towers (count = `tier`, clamped) on a baseline + per-tower outline, the middle tallest, heights tapering to the edges; an outpost reads as a lone tower, a metropolis as a dense cluster | Caller `colour` — civic-neutral `palette::settlement` (parchment); host-nation tint (`palette::nation_colour`) only under the Country lens | Population-centre conurbation marker, Planetary canvas (BL-083) |
| **Unknown** | `unknown(…, colour)` | Question mark — a top hook arc, a short stem, and a dot | Caller stroke (dimmed) | Survey badge for an **unsurveyed** body, Solar canvas (BL-067) |
| **Survey badge** | `survey_badge(…, colour)` | Magnifying glass — a lens circle with a diagonal handle (scan motif) | Caller stroke | Survey badge for an **in-progress** survey, Solar canvas; the canvas overlays a `k∕N` region count (BL-067) |
| **HQ** | `hq(…, colour)` | Ringed eight-point star — a diamond overlaid with an axis-aligned square, enclosed by a ring, with a dark centre dot so it reads against a same-colour ownership fill | Caller `colour` (the player identity colour) | The player's HQ/origin building, Planetary canvas (BL-085, folding BL-092) |
| **Activity** | `activity(…, colour)` | Concentric pulse — a filled core ringed by a signal ring (commercial-beacon motif; deliberately distinct from the survey magnifier and the unknown "?") | Caller `colour` — per activity tier (`palette::activity_known` / `activity_stale` / `activity_visible`) | Commercial-activity fog badge, Solar canvas — lower-left of the body, offset from the survey badge's upper-right so the two fogs read apart (BL-089; see [DISCOVERY.md](DISCOVERY.md)) |

On the Planetary canvas the **building** glyph's `fill` now encodes the *owning
corporation* (player corp = corp slot 0; rivals a hashed slot), so the
silhouette reads the building **type** and the fill reads **who owns it**.

### 2. UI-affordance glyphs — drawn in chrome

| Glyph | Function | Shape | Colour | Used by |
|---|---|---|---|---|
| **Ledger** | `ledger(…, colour)` | Ruled-table outline (box + two rules) | Caller stroke | Nav rail — slots that open a ledger window |
| **Placeholder** | `placeholder(…, colour)` | Hollow rounded square | Caller stroke | Nav rail — reserved/unassigned slots |

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

5. **The lens set is now complete.** The curated strip order is
   corporation / country / resource / market / population / **opportunity** /
   **production** / scarcity / **industry** (Industry joined the strip with BL-084;
   catalogued here 2026-07-04, its LENSES.md section still owed); the rest are
   ratified in [LENSES.md](LENSES.md) and all are catalogued above (`supply`
   exists but is off the strip). Note `resource` is **overloaded**:
   `resource(…, resource_type)` is the identity-coloured *pip* (a diamond), while
   `resource(…, ImU32)` is the *lens* glyph (the strata motif) — same name,
   disambiguated by the final argument type and by context (strip vs. canvas pip).

6. **No dedicated nation / corporation entity glyph.** The political layer conveys
   nations by tile *tint* (`palette::nation_colour`) and corporations by building
   *fill* (`palette::corp_colour`), not by a glyph. If nations/corps become
   directly canvas-selectable (the Ledger hit-testing follow-up), decide whether
   they need their own markers.
