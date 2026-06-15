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
`docs/development/TODO.md` (the lens glyphs below feed it).

---

## Shared conventions

Every glyph function takes the same positional contract:

```cpp
void glyph(ImDrawList* dl, ImVec2 centre, float r, /* colour or type */);
```

- **`dl`** — the draw list to render into (background list for canvases, window
  list for panels).
- **`centre`** — glyph centre in screen pixels. Glyphs are centre-anchored, not
  top-left.
- **`r`** — **half-extent** (think circumradius): the glyph is designed to fit a
  `2r × 2r` box centred on `centre`. Callers pass the radius, not the diameter.
- **colour** — either a caller-supplied `ImU32`, or *derived* (the `resource`
  glyph pulls its colour from `presentation_of`). See the catalogue for which.

Two visual sub-conventions, applied inconsistently across the set today (see
**Open clarifications**):

- **Filled glyphs carry a dark outline** (`IM_COL32(20, 22, 28, 255)`, the
  file-local `outline`) so they read on any terrain colour.
- **Stroke-only glyphs** use the supplied `colour` as the line colour and have no
  fill.

---

## Catalogue

Glyphs fall into three families by role.

### 1. Entity markers — drawn on the canvases

| Glyph | Function | Shape | Colour | Drawn for / where |
|---|---|---|---|---|
| **Extraction site** | `building(…, extraction_site, fill)` | Filled diamond + outline | Caller `fill` | Building marker, Planetary canvas |
| **Processing facility** | `building(…, processing_facility, fill)` | Filled square + outline | Caller `fill` | Building marker, Planetary canvas |
| **Port** | `building(…, port, fill)` | Filled upward triangle + outline | Caller `fill` | Building marker, Planetary canvas |
| **Building (none/other)** | `building(…, none, fill)` | Filled circle (dot) | Caller `fill` | Fallback building marker |
| **Resource pip** | `resource(…, res)` | Filled diamond (no outline) | **Derived** — `presentation_of(res).colour` | Resource strips, deposit markers |
| **Unit / convoy** | `unit(…, colour)` | Filled upward triangle (no outline) | Caller `colour` (e.g. a faction colour) | Unit stacks, Layer 5 convoy heads |

On the Planetary canvas the **building** glyph's `fill` now encodes the *owning
corporation* (player corp = faction slot 0; rivals a hashed slot), so the
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
| **Faction** | `faction(…, colour)` | Downward shield silhouette + outline | Caller fill | `overlay_mode::faction` |
| **Corporation** | `corporation(…, colour)` | Filled square + dark inner dot ("seal") | Caller fill | `overlay_mode::corporation` |
| **Resource** | `resource(…, colour)` | Three stacked horizontal strata, widening + deepening top-to-bottom (gradient / density motif) | Caller fill (per-stratum alpha) | `overlay_mode::resource` |

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

1. **Diamond is overloaded.** Both the *extraction-site* building marker and the
   *resource pip* render as a filled diamond, distinguished only by colour
   (caller fill vs. resource identity colour) and outline (building has one, the
   pip does not). They appear in different contexts today, but if a resource pip
   and an extraction marker ever sit near each other the silhouette is identical.
   Decide whether extraction should get a more mineral-specific glyph.

2. **Upward triangle is overloaded — and the contract mislabels it.** The *port*
   building marker and the *unit/convoy* marker are both filled upward triangles,
   differing only by the port's outline. The header describes `unit` as "an upward
   **chevron**", but the implementation draws a solid triangle. Either redraw
   `unit` as a true chevron (open V) to separate it from port, or correct the
   header text.

3. **Outline is applied inconsistently.** `building` and `faction` carry the dark
   outline "for contrast on any terrain", but `unit` (also canvas-drawn) does not.
   Decide whether every canvas-placed filled glyph should outline.

4. **`colour` means fill for some glyphs and stroke for others.** Supply/market/
   ledger/placeholder treat it as a stroke colour; building/faction/unit treat it
   as a fill. This is fine but undocumented per-call; the catalogue's Colour column
   is the current truth.

5. **The lens set is now complete.** Five lens glyphs exist
   (supply/market/faction/corporation/**resource**); all five are ratified in
   [LENSES.md](LENSES.md) and catalogued above. Note `resource` is **overloaded**:
   `resource(…, resource_type)` is the identity-coloured *pip* (a diamond), while
   `resource(…, ImU32)` is the *lens* glyph (the strata motif) — same name,
   disambiguated by the final argument type and by context (strip vs. canvas pip).

6. **No dedicated nation / corporation entity glyph.** The political layer conveys
   nations by tile *tint* (`palette::nation_colour`) and corporations by building
   *fill* (`palette::faction_colour`), not by a glyph. If nations/corps become
   directly canvas-selectable (the Ledger hit-testing follow-up), decide whether
   they need their own markers.
