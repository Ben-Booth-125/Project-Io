# Discovery & Intelligence

The authority for how the player *learns about the world* — the two layered "fogs" that gate what
is known, and the competitor-information rules that make that knowledge a strategic resource. The
owning items are BL-067 (survey), BL-068 (competitor visibility), BL-088 (persistent trade
routes) and BL-089 (commercial fog).

The canvases that *render* this model are documented in [`SOLAR.md`](SOLAR.md),
[`PLANETARY.md`](PLANETARY.md) and [`SELECTION.md`](SELECTION.md); the glyph vocabulary is in
[`ICONS.md`](ICONS.md); the map-lens overlays are in [`LENSES.md`](LENSES.md). This document owns the
*model* — what is knowable, how it is earned, and why.

---

## The core idea

The player is **not omniscient**. They must spend to learn, and read markets to infer what rivals
are doing. Two independent fogs govern this, plus one visibility rule:

| Layer | Owns | Driver | Grain |
|---|---|---|---|
| **Geographic fog** (survey, BL-067) | a body's tile map + deposit bands | the player dispatches a paid survey | per-tile / per-region |
| **Activity fog** (commercial sphere, BL-089) | a body's Unknown/Known/Visible activity tier + coarse market pulse | the player's own trade routes + presence | per-body |
| **Competitor visibility** (BL-068) | what is readable about *rivals* | structural (buildings) vs market (signals) | per-building / per-market |

The two fogs are **independent axes**, deliberately *not* chained: a body can be **Known** (your
commerce reaches it, so you read its market) yet **unsurveyed** (you have no tile map for it), and
vice-versa. They render as two distinct badges on the Solar canvas and must stay visually tellable
apart (the survey badge sits at a body's upper-right; the activity badge at its lower-left).

---

## Geographic fog — the Survey system

Authority for the mechanic: `src/world/survey_system.{hpp,cpp}`; the badge rendering is in
[`SOLAR.md`](SOLAR.md), the region mask in [`PLANETARY.md`](PLANETARY.md).

A body's `body_component::survey` moves through four phases:

- **`hidden`** — the player knows only the body's *public* facts: type, orbital position, and grid
  size (an astronomy dot). No tiles, no deposits.
- **`in_transit`** — a dispatched probe is en route; nothing is revealed yet.
- **`scanning`** — the grid reveals region-by-region. The grid is partitioned into square
  super-cells (`survey_super_cell = 8` tiles); regions reveal in raster (row-major) order, and a tile
  is visible iff its region index `< regions_done`. Deterministic — a pure function of grid
  dimensions, no RNG.
- **`surveyed`** — the whole tile map and deposit profile are readable.

`home_body` (and the star) begin `surveyed`; every other body begins `hidden`. Dispatch debits the
survey **cost** upfront (scales with body size × distance from home) and arms a **schedule**
(`transit_ticks` lead time + `scan_ticks` reveal time); `advance_surveys` crosses whole-day
boundaries deterministically. Deposit richness is revealed at survey time as a **band** (rich /
moderate / sparse); exact amounts confirm once an extraction site is placed.

The exploration loop: spot a promising body by orbital position and type → spend to survey → decide
whether the deposit profile justifies building.

---

## Competitor visibility

The intentional intelligence channel — what one corporation can learn about another. The single
branch point is `is_player_owned(world, building)` (with `owner_corp_of` behind it,
`src/world/world.{hpp,cpp}`): everything not player-owned is treated uniformly as a rival.

- **Rival buildings are visible on-canvas** — they are physical structures, unambiguously present
  (subject to the *geographic* fog: a rival marker shows only on surveyed regions).
- **Rival internals are private** — production rates and stockpile quantities never appear. A rival
  hover card shows **type and owner only**.
- **Construction state is public** (ruled 2026-08-09, NR-090): a rival's under-construction
  buildings render with the same construction dimming as the player's own. Scaffolding is
  externally observable, so a rival's expansion frontier is readable at a glance — deliberate
  counterplay, not a leak. Tick counts / progress detail stay private (the hover card hides them).
- **Market supply/demand aggregates and prices are public** — the deliberate signal. The player
  *infers* rival state from market movement: a rising price in a good a rival is known to extract
  means their output is down, or demand is up elsewhere — not a fact read off a panel.

This rule is what makes decisions interesting rather than trivial: intelligence is *earned by
reasoning over public signals*, not handed over.

### The spectator exemption — god view

Both fogs and the competitor rule protect a **competitive relationship**; spectator mode
(BL-409, spectator mode) has no player, so there is nobody being cheated. Under spectate — and
only there — a **god view** flag (BL-408, god view; `ui_state::god_view`, toggled from the system
menu, default off) lifts:

- **This rule's internals redaction**: the rival building hover/Selection cards open their
  Production/Stockpile rows (same rows, real values), the rival building card gains the
  read-only Profitability page, and a selected corp's facts column carries the full readout
  (cash, reserve floor, Should-Have buffer, per-body pools, running production).
- **The geographic fog at draw time**: the Planetary canvas renders unsurveyed regions
  through a heavy lock-colour wash — the *tell* — rather than as ordinary ground, so where
  the corps' own blindness sits stays legible while the watcher sees through it. The
  header's `UNSURVEYED / Surveying n/m` suffix stays for the same reason.

The lift is **strictly presentational**: the flag lives in `ui_state`, is read at the draw
call only, and `world/*`, `survey_tile_visible`, `body_activity_visibility` and
`export_corp_blackboard` never see it — the AI stays exactly as visibility-honest as before
(Ben, 2026-08-06: *"No additional information for the AI."*). God view grants **sight,
never hands**: rival controls (Mothball / Dismantle / Auto, Method's recipe switch) stay
disabled under it. With the flag off, every gated surface renders byte-identical to a
played session. The comms-feed redaction (`post_nation_agency_comms`'s
one-nation-line-per-tick throttle) is outside the flag's reach by construction — a post-time
store, not a draw-time read — so lifting it is its own design, not a flag read.

---

## Trade routes — the substrate the activity fog reads

Authority: `trade_route` in `src/world/components.hpp`; `world.trade_routes`; the upsert in
`credit_arrived_convoys` (`src/world/supply_system.cpp`). Fuller economy context in
[`../economy/SUPPLY.md`](../economy/SUPPLY.md).

Convoys are **transient** (auto-dispatched to fill a shortfall, erased on arrival), so they leave no
durable lane for a fog to read. A **`trade_route`** is that durable record: a body-pair lane a
corporation's commerce has actually run.

```cpp
struct trade_route { entity_id body_a, body_b, corp; int last_tick, convoy_count; };
```

- Keyed on the **unordered body pair** + corp (a body may host several markets; all collapse to one
  lane for visibility — `body_of_market` does the resolution).
- **Upserted** when a convoy completes an inter-body lane (`last_tick = ` completion day,
  `++convoy_count`); **one completed convoy establishes** a route. Intra-body lanes record nothing.
- **Never erased.** Aging to "stale" is a *read-time* concern (below), so there is no deletion logic
  and no flicker. Routes are part of the flat-binary serialisation seam, read and written
  symmetrically.
- Recording is done for **all** corps (cheap, and what rival-route intelligence would read); the
  fog reads only the **player's** routes.

---

## Activity fog — the commercial sphere

*The player's trade network is their intelligence network: where your goods flow, the world lights
up.* The fog keeps the spotlight on the player and makes commercial **reach** felt — it is **not** a
corporate-secrecy simulator (that framing was explicitly rejected).

### The per-body tier — a pure function

`body_activity_visibility(const world&, entity_id body, int now_tick, int route_fresh_ticks)` in
`src/world/world.{hpp,cpp}` — derived on demand from routes + live convoys + ownership + tick; no
stored state, nothing serialised.

```cpp
enum class activity_vis : uint8_t { unknown, known_stale, known, visible };
```

- **`visible`** — the player owns a building on the body, **or** a live player convoy is in transit
  on a lane touching it. The current, direct feed. (`home_body` is always visible.)
- **`known`** — a player `trade_route` reaches the body **and** `now − last_tick ≤ route_fresh_ticks`
  (default `route_fresh_ticks_default = 90` days — a calibration constant).
- **`known_stale`** — a player route endpoint exists but traffic has gone cold
  (`now − last_tick > route_fresh_ticks`). Greyed; **never** falls back to `unknown` (mild decay).
- **`unknown`** — none of the above: the public astronomy dot only.

Independent of survey phase: a surveyed-but-unrouted body is still `unknown` for *activity*; an
unsurveyed-but-routed body is `known`. (The read-side needs the current day; `world.current_day_tick`
mirrors it each frame so UI surfaces can age routes without threading the tick everywhere.)

### What each tier reveals

- **Unknown** — type + orbital position only.
- **Known / Visible** — a **coarse market pulse**: the body's *public* market (prices, supply/demand
  aggregates — already public under the competitor rule) plus a busy / steady / quiet activity level
  derived from market throughput. **No** per-building production or stockpiles — those stay private
  / survey-gated.
- **Stale** — the last-seen pulse, greyed with an age.

### Illumination (Solar canvas)

Rendered in `src/ui/solar_system_canvas.cpp` (see [`SOLAR.md`](SOLAR.md) for the chrome table):

- **Activity badge** per body — a concentric "pulse" glyph (`ui::icons::activity`) at the body's
  **lower-left**, coloured by tier (`palette::activity_known` / `_stale` / `_visible`), deliberately
  offset from the survey badge (upper-right) so the two fogs read apart. Unknown and home bodies show
  no badge (home carries its presence halo, BL-085).
- **Corridors** — the player's persistent routes drawn as lit lanes between endpoint bodies (fresh
  routes glow, stale routes fade to grey), so commercial reach is felt on the map.
- **Proximity glimpse** (BL-099, proximity glimpse) — a body a completed player lane merely *passes
  near* — not an endpoint, and with no route of its own — is marked `known_stale`: a faint, decaying
  "peek", the third illumination geometry that completes the endpoint / corridor / proximity trio.
  Sampled once at the convoy's discrete completion tick (`record_proximity_glimpses`, from live
  orbital positions), never a per-frame test, so it cannot flicker with orbital drift; it feeds the
  same lower-left activity badge at the stale tier. A glimpse never rises above `known_stale` (a
  peek, not a data feed) and a body's own route always outranks it. **Sample-and-store** is the
  design because position is not a pure function of tick (`orbital_angle_rad` is mutated), so a
  glimpse could not be reconstructed at a later read: the closest-approach set is sampled at the
  completion tick — when orbits have already advanced for that frame, so the live positions *are*
  the completion-tick positions — and the glimpse tick is stored (`world.body_last_glimpse_tick`);
  the fog reads the stamp and never recomputes geometry. Calibration constants:
  `glimpse_radius_au_default` (0.25 AU) and `glimpse_fresh_ticks_default` (90 ticks).
- **Hover** — the Solar-canvas body tooltip carries a short activity read keyed on
  `body_activity_visibility` (unknown → outside network; known → market pulse; stale → gone cold;
  visible → live lane / presence), wording aligned with the Selection panel's
  `draw_activity_section`; the star is excluded. Pure read, no state.
- **Selection panel** — a body gains a "Commercial activity" section keyed on the tier (unknown →
  "outside your trade network"; known/visible → the market pulse; stale → a greyed note).

### Illumination (Planetary canvas) — the intra-body reach fog

The body-level tier above is a *coarse* per-body read on the Solar canvas. On the **Planetary
surface** the activity fog is a **per-tile** treatment (BL-151/BL-152/BL-154, intra-body reach
fog), rendered in `src/ui/body_surface_canvas.cpp`. It is a *third* visibility axis, independent
of both the survey fog and the body-level tier: it asks "where on **this** body does my commerce
actually reach?" The surface reads as **mostly unknown**, lit only where the player has presence —
so an own-territory home planet still carries fog of war.

- **Semantics (Ben's call):** commercial *reach*, not unknown terrain — the home planet is the
  player's own soil, so "fog" means *where your logistics operate*, not *where the land is unmapped*.
- **Substrate:** live-derived, **no stored state / no save-format change** (intra-body traffic records
  nothing persistent — the Solar fog's `trade_route` substrate is inter-body only).

Three vision layers, all derived VIEW state (`ui_state`, rebuilt each frame by `ui::update_body_vision`
for the active body — never serialised, no feedback into `world/*`). A tile's `vision` is `1` in the
permanent layers, else the moving beam's intensity; the fog wash scales with `1 − vision`, applied over
the lens fill so a fogged region's analytic read dims with it. Survey owns the *unrevealed* tiles.

**Roads take the same wash (BL-185, roads under fog).** The lens fill and the road spans go through
a single `fog_dim` (`body_surface_canvas.cpp`), so the fog reads as one uniform wash — a road on an
unreached tile is never the one bright thing on a dark surface. A road edge spans **two** tiles, and
it is fogged by the **max** of the pair's vision — a road is lit if *either* end is reached. Max is
chosen because it is **symmetric**, so the two tiles' halves fog to the same value and the span
stays one continuous weight (the same no-from/to-asymmetry property the road geometry has); and
because reach is a flood outward from where the player operates, so an edge touching a reached
tile is inside that reach rather than one hop past its rim. A junction's centre cap takes the
brightest edge meeting there. Presentation only — no `world/*` change.

- **Permanent building pockets:** a radius-2 flood around each of the player's own building tiles —
  your installations are always visible.
- **Permanent corp-centre → market corridors:** a **3-wide** corridor (the A* path flooded one hop
  to each side) from the **corp centre of operation** (the lowest-id player building tile on the
  body) to each **market centre** the corp operates in (the catchment markets of its buildings).
  Your trade spine is always lit.
- **Moving convoy beam:** a live player intra-body convoy lights a **radius-2** beam with a bright
  **head** and a dimming **tail**. `ui_state.convoy_beams` stores each convoy's tile path (from
  `intra_body_path`'s exposed sequence) + progress + speed; the canvas interpolates the head along
  the path by the fraction through the current econ tick (`fmod(sim_now_days, 90)/90`), so it
  **glides smoothly** between quarterly steps, and trails a tail one econ tick's travel behind
  (intensity ramping `1 → 0` head → tail). This is the "things are moving" layer; the corridors +
  pockets are the "where I operate" layer.

---

## Determinism, serialisation, scope

- Everything here is **deterministic**: survey reveal is a pure function of grid dimensions; the
  activity tier is a pure function of routes + convoys + ownership + tick + the proximity-glimpse
  stamps (each written once at a discrete completion tick — no per-frame geometry, no reconstruction
  of past positions). No RNG, no Lua, no per-tile data exposed to Lua.
- **Serialisation:** `world.trade_routes` and the proximity-glimpse stamps
  (`world.body_last_glimpse_tick`) are part of the flat-binary seam, read and written symmetrically
  (the world snapshot is BL-536, world snapshot save). Both are held in `world` maps, off
  `body_component` (the `corp_body_pools` rationale), to keep the body's flat-binary layout
  untouched. Survey state rides on `body_component`. The activity tier and the market pulse are
  derived, so they need no persistence.
- **Verification:** headless — `tools/verify/survey_harness.cpp`, `visibility_harness.cpp`,
  `trade_routes_harness.cpp`, `commercial_fog_harness.cpp` (the last covering the glimpse
  geometry, tier, decay + determinism); visual — the `survey`, `visibility`, `commercial_fog`, and
  `proximity_glimpse` golden scripts under `scripts/verify/`.

## Extensions

- **Rival routes** surfaced to the player — the substrate already records every corp's routes;
  the read is gated on the competitor-visibility rule.
- Per-resource "where is X cheap" inference tooling; a persistent commercial-sphere minimap.
