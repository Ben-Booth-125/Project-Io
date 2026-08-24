# Changelog

All notable changes to Project Io are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/) (pre-1.0: minor/patch are advisory while
the prototype is in flux).

Each released version corresponds to an annotated git tag (`vX.Y.Z`) — the tag is the
authoritative version-history record; no local `backups/` snapshot is kept (that convenience
scheme was retired 2026-07-31). See `docs/development/DEVELOPMENT_PRACTICES.md` § Cutting a
release.

## [Unreleased]

## [0.1.15] — 2026-08-24

### Added
- **The mercenary vertical slice (Sprint 16)**: a polity hires the company, the company fights,
  the company is paid — playable end-to-end. A province holder exists and moves on a decisive
  battle (BL-569); nations field static garrisons sized off their own treasury (BL-571); a
  threatened nation's budget derives real mercenary offers against its highest-grudge neighbour's
  weakest border province, several open concurrently (BL-572); accepting an offer commits a real
  force through the Contracts ledger's force picker, and marching that force into the target
  province is itself the trigger for a real battle against the nation's garrison — no
  `declare_hostile` needed (BL-573); every contract terminal event (accepted / completed / failed
  / abandoned) posts to the Public channel, opens a contract card, and a "Contract income" line
  now appears on the Balance ledger and header runway the tick a payout lands (BL-577). The
  Contracts ledger (nav rail, new slot 13) gives Offers / Active / History views (BL-576). Unit
  markers, a march command, and the march-to-contact loop are live on the Planetary canvas
  (BL-575). `world_save_version` 3 → 8 across the batch, one bump per new persistent field.
- `scripts/verify/mercenary_slice.lua` — a six-capture scripted playthrough of the whole loop
  from a fresh world, and `tools/verify/mercenary_contract_harness.cpp` /
  `contract_dispatch_harness.cpp`, new headless harnesses proving the contract and dispatch seams.

### Known gaps, carried forward rather than silently closed
- The contract card (`selection_kind::contract`) has no live selection trigger anywhere in the
  UI yet — its rendering is built and correct but unreachable by any control. A future item owns
  wiring the trigger.
- A live human session confirmed offer → accept (force picker) → march → active contract, but a
  real nation-issued offer's deadline (tuned in econ-ticks) takes on the order of ten hours of
  real wall-clock time to reach at the game's maximum speed — no human session can realistically
  wait out a payout today. Flagged for a pacing fix.

### Changed
- **Province names are wholly native** (BL-348). `"<People> <Region>"` had a coined culture half and
  an English region half since BL-290 — *Reach*, *Coast*, *inland* — which read as a bug rather than
  a style, because the two naming systems sat side by side in one string. Each tongue now coins its
  own nine region words, sized to the existing 5-band / 4-sector positional mapping so the name
  still carries a fact about the ground. Drawn last in `coin_lexicon`, so every nation and city name
  is byte-identical to before.
- **A knife-edge test stopped voting on generation parameters** (BL-349). `settlement_harness` S7d
  asserted a corporate-diversity floor the generator explicitly declines to guarantee, and its
  verdict moved with `lowland_share` — so BL-338's tuning pick was simultaneously defensible on
  drainage grounds and the green value. S7d now asserts the property that does not depend on
  province ownership (the corporation set is not a monoculture) and *reports* the three-way split
  instead of asserting it. Verified by the sweep the item specified: 0.15 / 0.20 / 0.25 now agree.

## [0.1.14] — 2026-08-11

**Procurement, and the goods it is about.** The refocus's actual mechanic — a corporation
contracting a private company for equipment, rather than buying against an unlimited market or
producing it in-house — plus the resource tiers that make "space equipment" a thing rather than a
label.

### Added
- **The processing chain roster** (BL-340). Seven new tradeable resources — silicon, refined
  copper, REE alloy, machinery, alloys, electronics, spacecraft components — with market pricing,
  recipes and UI presentation. Closes the "minable but unsellable" gap on silica, copper ore and
  rare earth ore (deposits existed, no market ever priced them), and fixes the Smelter's steel
  recipe to actually consume coal as its reagent, matching the design docs.
- **Military points and a dedicated Research Institute** (BL-332). Every corporation now accrues
  `military_points` from its completed Military Bases and `science` from a new Research Institute
  building, passively, per tick — the production half of "how does tech get done", symmetric
  across the player and every rival/background corporation. No spend mechanism reads either
  currency yet; that is the tech tree's job, landing separately.
- **The procurement/contract seam** (BL-350). A corporation can request a quote from a named
  supplier, accept it into a running contract (a deposit up front, the rest paid across an
  agreed lead time), or cancel one in flight. A supplier can refuse — no production capacity, no
  access to its own inputs, an embargo, or a poor trading history with the asker — with the
  refusal reason always legible. The same command both a player's press and a rival corporation's
  own decision-making go through.

## [0.1.4] — 2026-08-10

**Techs — a technology that can actually be earned, and that unlocks something other than a
factory.** One item. A tech tree has existed since 2026-07-08 with a radial viewer on F9, but its
gate was stored as a descriptive *string*: a label describing what the gate would be about, unable
to resolve true or false. No tech had ever been earned, and nothing had ever been unlocked.

### Added
- **The Military Base is gated behind a technology** (BL-344) — `E0-ML-01` "Standing Garrison
  Doctrine", earned once a corporation holds two extraction sites and Cr 2,000. Military rather than
  economic on purpose: a technology that can only unlock a building is being designed for the
  corporate player this project is pivoting away from, and gating the base cost exactly what gating
  a smelter would have.
- **Earned techs are per-corporation** (`world::earned_techs`), evaluated once per economy tick.
  Monotonic — a tech is not un-earned by a later dip below its threshold.
- **A locked build row says which technology is missing**, at the same call site that would
  otherwise offer the Build button (`placement_reason::tech_locked`,
  `construction_result::tech_locked`).

### Changed
- **`tech_node::condition` is a `condition_set`, not a string.** The F9 viewer now reports EARNED,
  LOCKED with its unmet conditions itemised, or — honestly — "no gate authored", instead of showing
  a label that could never resolve. "Not yet authored" needed its own flag, because an empty
  predicate is *true* and an un-authored tech would otherwise have earned itself on the first tick.
- The predicate lives in the Lua-free `src/world/tech_gate.cpp` rather than in
  `scripts/tech_tree.lua`, so a gate that gates construction can be linked — and tested headlessly.
  The Lua file authors identity, topology and prose, and reads the predicate back.

**Gate:** 58 tests, 0 failures. New harness: `tech_gate_harness` (33 assertions).

## [0.1.3] — 2026-08-10

**Laws — one law you can turn on and see.** Two items. Not the ten-law list and not a policy
screen: one law, enacted, changing a number the player already reads. A law the player cannot see
working is indistinguishable from an unimplemented one, and that was the difference between this
minor being cuttable and being another design document.

### Added
- **The Extraction Levy** (BL-343) — a per-unit charge on raw output, switched from the Budget
  ledger's new **Laws** section and landing as its own **Levies** bar on the Finance card, beside
  income, inputs, maintenance, wages and interest. It ships **un-enacted**: enacting nothing changes
  nothing, and the balance is bit-identical to a world with no laws at all.
- **`condition_set`** (BL-342) — the shared predicate laws, techs and quests all read. A flat
  AND-list of atomic conditions, pure and deterministic, where an empty set means *always* because
  that is the common case for a law. Two of its eight subjects are **military**: BL-094's design
  test applied at the foundation rather than promised for later, since a predicate that can only
  ask economic questions is the exact failure the governing-body pivot exists to avoid.

### Changed
- **The law enforcement seam is settled**: a law is a modifier **over** the market, never an
  override **of** it — the same principle that vetoed price clamps in July. The levy applies where
  the flow is *accounted* (`apply_budget`), never where the price is *resolved* (`clear_markets`),
  so the market stays the only thing that sets prices and the player sees the tax as its own number
  rather than as an unexplained worse price.
- `apply_budget` takes an optional production argument; omitting it charges nothing, which is why
  no existing economy harness needed to change.

**Gate:** 58 tests, 0 failures. New harnesses: `condition_set_harness` (40 assertions),
`law_harness` (21).

## [0.1.10] — 2026-08-09

**Generation & content — what the world is called and what it is made of.** Eight items. Three of
them found the filed diagnosis wrong and corrected it by measurement rather than quietly patching
around it, which is the through-line of the minor.

### Changed
- **Names are coined from each culture's own phonology** (BL-290), not from a global Latin/European
  bank. The generator already produced a per-culture sound system and naming simply never consumed
  it. Now it does — and the kinship is **emergent, not authored**: *Rerekua Tekua* and
  *Kuamreiteik Tekua* share a realm word, *Duagual* / *Shualgual* / *Vegual* a settlement morpheme,
  because the word-coiner is a pure function of the tongue and two passes reach the same morphemes
  without sharing a stream.
- **Body identity is an entity id, not a display string** (BL-257) — across **twelve** sites, eight
  more than the item listed, including a cache key that was the body *name*. Shipping generated
  names without this would have been a silent correctness bug rather than a cosmetic change. All 28
  verify scripts now address bodies by role, so a generated catalogue cannot break a check again.
- **Corporations anchor in their home province** (BL-283) rather than anywhere in their nation.
  Holdings moved off dead ground onto settled ground — barren 6→3, grassland 3→7 — because
  provinces sit where people settled. The home-province window succeeds first-try 89% of the time;
  every fallback was a corp with no home province at all.
- **The econ tick is roughly halved** (BL-347): 8×256 min 2.045 ms → **0.87 ms**, better than the
  pre-regression baseline. None of the three suspected causes dominated — the sort flagged as
  O(n log n) was 3% of it, and the real cost was map-node allocation per tick in worlds containing
  no stack at all.
- **The pre-build profit estimate accounts for the stack it would join** (BL-346) — it was over by
  25% at rank 2, 56% at rank 3, and 213% where decay and taper compounded.

### Added
- **Wetland exists again** (BL-338) — 12 tiles → 159. Wetland is a *drainage* feature, and it is the
  one composition the climate table cannot express, because a marsh is defined by where water fails
  to leave. Elevation now has a say, measured as a percentile of land above the body's own sea level
  so the slice exists on every seed.
- **Territorial fragmentation is measured and attributed** (BL-284) — 60 emergent exclaves against
  136 from orphan-island cleanup, printed as both a component count and a tile count because they
  tell different stories (31% vs 49%). This settles a claim the world generator's architecture was
  bought on, and it pays.
- **Propellant is a real resource** (BL-308) with two authored recipes and per-launch consumption:
  an unfuelled launchpad is exactly as shut as no launchpad.
- **Trade routes log both endpoints** (BL-282), so a body-scoped filter finds a route from either
  side. Measured cost: 18 → 36 entries across a 1500-tick run, bounded by body-pair count rather
  than by traffic.

### Also in this cut
- **The generation globe** (BL-256) turns out to have been finished — verified against the code
  rather than taken on trust. The wizard's right two-thirds is the world itself: the system in
  round 0, the real homeworld raster after. Its third requirement, a pannable camera, is
  **deliberately cut**: the globe spins on a clock and takes no input, because an uncontrollable
  globe tells the player that generation is slightly beyond their reach — the same thing the
  preferences-not-parameters model says. A draggable camera would have contradicted the screen's
  own premise to add a control nobody needed.

### Known gaps
- Deed history lines (BL-309) are designed but unbuilt — moved to v0.1.11.
- **Visual goldens are stale wherever a body name renders.** Confirmed by eye as a pure text delta.
  Visual goldens are Windows-authoritative, so the re-bless belongs on that machine.
- `region_word` still supplies English province words (Reach, Coast), so province names now read
  *half*-native — more visible after BL-290, not less. Filed as BL-348.
- `settlement_harness` S7d asserts a diversity floor the generator explicitly calls a preference,
  and flips on unrelated parameters. It had a quiet vote in BL-338's tuning, visible only because
  the implementer volunteered it. Filed as BL-349.

## [0.1.9] — 2026-08-09

**Shell & legibility — the standing UI set, finished.** Eight items closing the follow-through that
had been accumulating since v0.1.1: the always-on canvas layers become decodable, stacking becomes
a decision rather than a dominant strategy, screen geometry gets one owner, and disclosure stops
being a single all-or-nothing control.

### Added
- **A contextual road-tier legend** (BL-184). Three tiers had been drawn by line weight and
  brightness alone with no key anywhere. Selecting or hovering a road-carrying tile now names its
  tier, with the thin→thick ladder in a tooltip. Contextual rather than a persistent chip: roads
  render always-on like terrain, not as a lens, so the per-lens legend drawer could never have
  carried them.
- **Building stacks are a real trade-off** (BL-193). Site *k* on a tile now produces `0.8^(k-1)` of
  a lone site's rate — 2 sites yield 1.8×, 5 yield 3.36×, never 5× — while every site draws real
  reserve, so a full stack drains the deposit faster than its output multiple. Depletion tapers
  against the stack's **combined** nominal, so members exhaust together instead of desynchronising.
  The construction ledger states where a site sits in its stack and what that costs it.
- **The UI justification store** (BL-260) — `docs/ui/question_log.json`, 16 surfaces each declaring
  the question it answers, why it earns its space, and the backlog item that demanded it. A two-way
  provenance index: surface → item, and item → surface.
- **The Economy panel has a door** (BL-292). It was drawn every frame while nothing in the nav rail
  could open it — reachable only by the verify harness. It now holds rail slot 3 and has an
  `ACTIONS.json` entry, so a human reading the rail and an agent reading the dictionary can both
  find it.

### Changed
- **Roads dim with the commercial-reach fog** (BL-185). A road on an unreached tile had rendered
  identically to one on a tile the player actively operates. Both the lens fill and the road spans
  now pass through one `fog_dim`, so the fog is literally a single wash. Per-edge vision takes the
  **max** of the two tiles: a span is drawn as two halves meeting at a shared midpoint, and max is
  the only cheap combiner under which both halves agree.
- **Disclosure is two controls** (BL-265) — *expand in place*, or *take the canvas*. Full screen now
  bounds itself to the canvas rather than the window, so the header, clock, comms dock, Selection
  band and minimap survive it. A full-screened accordion shows **all** of itself, scrolled — the
  whole round of chain stages, all four dashboard roll-ups. Controls are right-aligned in one
  column everywhere, and every glyph is **drawn** through the draw list rather than typed, which is
  the missing-codepoint defect BL-234 already fixed once.
- **Screen geometry has one owner** (BL-216). `shell_metrics.{hpp,cpp}` now owns the shell's
  composed rect algebra, and the five places in `app.cpp` that each re-derived the right chrome
  column's left edge by hand were migrated onto it.
- **The comms channel strip became a selector.** A wrapping tab chain spends vertical rows, and the
  docked comms panel is 260 px tall; a combo holds any number of channels in one row.

### Removed
- **The History ledger's Tiles view** (BL-281) — a current-state readout inside a ledger about the
  past. Renaming it would have fixed the label and kept the defect. Its content already had homes:
  buildings on the canvas and in the Selection element, market data in the market surfaces. Story,
  Chain and Ages remain.

### Known gaps
- **The econ tick roughly doubled** (BL-347, priority A). Measured against the parent commit on the
  same machine: the largest sweep rung's `min` went 0.958 ms → 2.045 ms, with the cost present at
  every rung including the smallest. **Prototype scale is unaffected** — 0.20 ms mean, 5× headroom —
  so this is lost growth headroom rather than a live frame-rate problem.
- `building_profit` still estimates a lone site, so a stacked building over-reports revenue and
  remaining life (BL-346). Not display-only: the loss-streak reflex and the workforce solver both
  act on that number.
- The building-selection element still does not follow the tile element's three-column format
  (BL-229) — deliberately deferred, as the item reserves that layout for Ben to design.
- 13 of the justification store's 16 entries are `drafted`, awaiting Ben's wording. Writing the
  pair *is* the design check, so they are drafted rather than shipped as settled.

## [0.1.8] — 2026-08-09

**Build health — the gate stops lying.** No player-facing content: this is the minor where the
project's own tooling stopped costing time on every session that tripped it. The headline number
is that `ctest` reported **ten failures of which exactly one was a failing assertion**, and the
other nine were the suite misreporting itself.

> **Cut order.** Numbered v0.1.8 to avoid renumbering the design-forward stubs at v0.1.3–v0.1.6,
> but cut ahead of them deliberately — number is not sequence in this band.

### Changed
- **Three test tiers instead of one flat 60-second bound** (BL-288). Four harnesses pass but
  exceed it (`earthlike_lean_trace` 121 s, `notable_worlds` 105 s, `mediterranean_sweep` 87 s,
  `earthlike_tile_census` 58 s) and were being reported as failures; the long tier rises to 240 s
  and covers them. Two open-ended research sweeps get a `sweep` label, no timeout, and are out of
  the routine gate — they answer design questions over many worlds and will outgrow any bound.
  Two harnesses that assert *absolute* wall-clock times get a `bench` label, so a failure reads as
  "re-run on an idle machine" rather than as a regression. The gate is now
  `ctest --test-dir build_linux -LE sweep --output-on-failure`.
- **`next_id.js` fails loudly instead of silently defending nothing** (BL-322). It reported zero
  refs because `execSync` runs through `dash`, which aborts on the unquoted `(` in
  `--format=%(refname)` before `git` ever runs — so the tool worked on the Windows box where it
  was written and failed silently everywhere else. It was issuing ids **25 below the true
  ceiling**, which is the mechanical account of how BL-326..BL-333 each landed twice. Now uses
  `execFileSync` with argv arrays, cross-checks with `git show-ref`, and exits non-zero when it
  cannot prove it ran. Refs scanned: 0 → 53.
- **GCC goldens re-blessed** (BL-285) — the one real defect in the ten. Stale since 2026-08-01;
  the divergence is downward and explained by the v0.1.2 reach rule (siting is now bounded) plus
  unit hiring (a new cash outflow), and that reasoning is recorded in the harness rather than
  assumed. The MSVC set is now stale for the same reason and is flagged in place.
- **Ladder lines carry a `ladder_rung` tag** (BL-285), so the history-ladder check filters
  structurally instead of pattern-matching biography prose — rewording a line can no longer
  silently change what the check asserts over.

### Added
- **Seeded FetchContent cache** (BL-302) — per-dependency `FETCHCONTENT_SOURCE_DIR_<dep>` from a
  shared `_deps_cache`, so fresh worktrees configure offline. The item's own preferred option, a
  shared `FETCHCONTENT_BASE_DIR`, was tested and **hard-fails** across build trees on a
  generator-locked subbuild cache. Every configure now states its mode, and a deliberate from-cold
  check is documented.

### Fixed
- `world_audit` was **never broken** (BL-291) — it exits non-zero on 1 assertion of 26. The
  measured landform census stands, and `TILES.md`'s last "needs re-measuring" note is re-measured:
  Kepler's 0.0% valley is a real wet-body property, not a stale artifact.

### Known gaps
- The from-cold check has **not** been run on Windows, where the original schannel TLS revocation
  failure actually occurs — it does not reproduce on Linux, so the fix is untested against its own
  symptom. Filed as BL-341.
- The gate's one remaining failure is `world_audit`'s biome-balance assertion (forest+wetland
  2.41% against a 3% target). That is a world-generation finding carried by BL-338, not an
  instrument defect — the gate reporting it is the gate working.

## [0.1.2] — 2026-08-09

**Buildings rework — remoteness stops being free.** The minor that the simulated-play arc was
waiting on. Siting a building is now a decision with a trade-off rather than a lookup of the
richest tile on the map, and construction reads as a process on the canvas instead of a silent
timer.

> **Cut order.** This tag lands before `v0.1.1`, whose word-interface theme is cut separately
> from earlier work. Pre-1.0, minor numbering is advisory (see the header note); each tag
> documents its own theme rather than a strict chronology.

### Added
- **A logistical maximum range on placement** (BL-323) — the rule that had no code at all. A site
  must sit within a bounded logistics cost of a supply anchor (city, port, or inland logistics
  hub), computed as a multi-source Dijkstra reach field over the terrain-weighted cost function
  logistics already provided, cached per body and invalidated on every mutation that can move an
  anchor. The budget is authored in `economy.lua`, not hard-coded: at the shipped value, 77.4% of
  Kepler's land is placeable, no land tile is unreachable, and all 20 generation-placed buildings
  already sit inside it — so generation and the rule agree without retuning either.
- **A first-anchor bootstrap** — a body with no anchor refuses everything *except* an anchor, so
  working a virgin moon begins by planting a logistics hub. Deliberate mechanic, not a side effect.
- **Site-dependent build time** (BL-323) — `build_duration_ticks` becomes a per-type *base*,
  scaled by landform, by distance from the nearest supply anchor, and discounted where a tile
  already carries an established stack. A remote mountain site is now a considered commitment.
- **Construction as a visible process** (BL-323, BL-327) — a dedicated under-construction glyph on
  the Planetary canvas replaces the old dimmed-marker treatment, with remaining ticks on the
  hover card.
- **Eleven more extraction targets** (BL-323) — `k_extractable` widens from 4 to 15, covering every
  resource tile generation already deposits but no building could reach, plus the Smelter's
  iron/nickel → steel recipe.
- **Pre-commitment supply warning** (BL-328) — the construction ledger says a build will starve
  *before* you commit to it, rather than after it stalls.
- **Expandable building groups** in the construction ledger, two-tier alphabetically sorted
  (BL-326).

### Changed
- **Clicking a building fills the Selection element again** (BL-330) — it had been skipping
  straight to management, contradicting the settled single-click-selects / double-click-navigates
  click model.
- Placement surfaces stop offering tiles the gate will refuse — the rule is applied wherever a
  tile is *offered*, not only where a build is committed, so an offered-then-rejected tile no
  longer reads as a broken build.

### Removed
- **The corp-building reach circle** (BL-329) — the fog and the new reach rule now carry that
  information between them.

### Verification
- `buildings_rework_harness` 12/12 PASS; `logistics_reach_harness` 26/26 PASS.

### Known gaps
- The **processing half** of the roster (Chemical Plant, Electronics Lab, Fabricator, Assembly
  Plant, most Refinery outputs) is **not** in this cut. It needs new `resource_type` values with
  market, price and serialisation wiring — a save-format change rather than Lua authoring — and is
  filed as BL-340 (processing-chain roster) so closing the rework did not silently drop it.

## [0.1.1] — 2026-08-09

**The word interface.** The route from documentation to a text-driven player, and the groundwork
the v0.2.0 AI opponent is built on: an agent can now read the world state, look up what every
control *means*, and drive the game over a socket — with no HTTP client, no API key and no cloud
dependency anywhere in the engine. Ships alongside the shell work that ran in the same band.

> **Cut order.** Tagged after `v0.1.2`, whose buildings theme was finished first. Pre-1.0 numbering
> is advisory (see the header note); entries are ordered by version, not by tag date.

### Added — the word interface (the theme)
- **Blackboard export** (BL-206) — world state an agent reads through a stable export instead of
  scraping the UI.
- **The action dictionary** (BL-270) — all 115 controls as `{press, typed args, preconditions,
  expected output, reason to select}`, across five families. The gameplay family is *transcribed*
  from `corp_command.hpp` rather than authored beside it, so the seam cannot drift from its own
  description. `ACTIONS.json` is canonical; `ACTIONS.md` and a compact `ACTIONS_INDEX.json` are
  generated.
- **The Io MCP server** (BL-278) — `ProjectIo --serve` plus `tools/mcp/`, making both of the above
  drivable by any agent runtime. Touches no simulation code.

### Added — shell, canvases & legibility
- **The sticky detail card family** (BL-194–BL-198) — card frame, the Selection element moved
  wholesale into it, recursive bounded drill-down, a titled dual-axis chart container, and the
  resource time-series store behind it. **Drill-through** becomes one shared progressive-disclosure
  idiom (BL-214), and every chart declares the question it answers (BL-247).
- **Corporation dashboard** (BL-248) as roll-up cards.
- **Commercial-activity fog** (BL-150–BL-154) — the unlit map as a dim shadow, intra-body reach on
  the Planetary canvas, and convoy vision beams with moving head/tail corridors.
- **Radial tech-tree viewer** (BL-310, F9) — the Era 0/1 quest trees as a constellation.
- **New World wizard** split to 1/3 controls + 2/3 preview, with a real-tile globe (BL-319).
- **Mediterranean rift sea** (BL-276) — a continental rift basin floods into an enclosed sea.

### Changed
- **Hover behaviour** — a card freezes until the cursor leaves its bounds, retiring the dwell
  timer (BL-228), and hover settles into glance-then-stick (BL-230).
- **Comms dock** moves bottom-left at the Selection band height (BL-227); the **minimap** wraps
  flush to the canvas edges (BL-312); the **time controls** reflow into the header's own height
  (BL-313).
- **GPU utilisation and multicore threading** for the growing generation/render load (BL-267).

### Known gaps
- **The write leg is partial.** `place_sell_order`, `remove_sell_order` and `set_workforce_auto`
  appear in the dictionary but have no `corp_verb`, so an agent can read the order book and be told
  what the press means without being able to place a standing sell order. The cause is structural
  rather than three missing verbs: sell orders live in UI state, the world holds no order book to
  mutate, and no serialisation path touches them. `ACTIONS.json`'s note states the narrowing
  explicitly. Carried by BL-293, moved to v0.2.0.
- 22 further items that had been retrofitted onto this minor after its theme completed were
  **re-homed, not cancelled** — into v0.1.8 (build health), v0.1.9 (shell & legibility) and
  v0.1.10 (generation & content).

## [0.1.0] — 2026-08-03

**The prototype cut.** The economy loop, validated and playable end-to-end — construction,
population-grounded workforce, spatial price divergence via convoys, gated discovery, scoped
competitor intelligence, a legible budget, and a green, performance/data-growth-audited build.
This is the milestone `ROADMAP.md` names as the v0.1.0 done-definition; everything past it
(laws, techs, military, politics, the AI opponent) is the v0.1.x → v0.3.0 arc, not this cut.

### Added
- **Roads & planetary logistics** — a generated per-nation road lattice (three tiers), cities as
  free logistics hubs, and the inland logistics-hub building, landed ahead of its originally
  planned v0.1.1 slot.
- **Continent lens** (first cut) and the **Continents/Drift** generation pass feeding plate-biased
  heightmaps into tile generation, with `plate_id` retained for the lens.
- **Terrain landform render and spanning markers**, plus **terrain combat modifiers** replacing the
  flat `is_barrier` field with a graded, weighted terrain cost (water weight 750, swept to hold the
  political map at 30 Kepler nations).
- **Tile construction ledger** — a world-layer cost estimator with per-recipe rows, surviving a
  build across selection changes.
- **Quality-audit instrumentation** (no new systems, three new harnesses): a **frame-budget**
  reporter (F11, off by default — build/submit/present/other breakdown, avg/max/1%-low against the
  8 ms / 16.7 ms targets); an **econ-tick scaling** sweep (0.0018 ms/tick mean at prototype scale,
  555× headroom, growth-shape asserted at size^1.28 against a size^1.5 tolerance across a six-rung
  bodies × corps sweep); and a **data-creep** harness (~30 counters + RSS over 1500 ticks of the
  real generated world, asserting a plateau rather than reporting a snapshot).
- **Convoy data-creep coverage** — convoy dispatch now runs through the audit rollout, so the
  convoy / trade-route / glimpse plateaus bind instead of passing vacuously.

### Changed
- **Font glyph range** widened — 43 sites across 7 codepoints were missing, not the 26/2 originally
  filed.
- **Headless golden bands pinned per toolchain** (`ai_skill_harness`) rather than widened to cover
  both — a shared band spanning MSVC and GCC's divergent net-worth outputs would span ~±100% and
  catch nothing.
- **Visual goldens declared Windows-authoritative**; the Linux visual-verify job is advisory
  (inspect by eye, don't golden-diff).
- **The full 229-golden visual suite re-blessed** after two days of silent staleness from the
  terrain re-price above — the diffs were world content (terrain colour, generated corp/nation
  names, balances), not a UI regression. `DEVELOPMENT_PRACTICES.md` now names the missing
  discipline: a `src/world/` change that reshapes generated output re-blesses the visual suite in
  the same commit, mirroring the existing shared-chrome rule.

### Fixed
- Windows headless-harness gate — the absolute 1 ms tick bound at the largest swept rung was a
  Debug-build artefact (R5 still passes with 19× headroom in Debug; every growth-shape assertion
  holds either way), not a regression.

### Verified
- Headless harness suite green on Linux (Release) and Windows (Debug) at the same commit for the
  first time (42/42 — `mediterranean_sweep`'s 60s CTest timeout is a harness-runtime artefact,
  confirmed passing standalone; not a v0.1.0 item).
- Visual golden suite re-established against current world content (229 goldens re-blessed).
- Windows build green (residual warnings are third-party sol2/ImGui headers, not project code).

## [0.0.9] — 2026-07-05

Budget clarity + polish — the remaining legibility rough edges cleared before the v0.1.0 quality
audit. (The budget-clarity strands that originally named this minor shipped early, folded into
v0.0.8.)

### Added
- **In-app system menu** — a corner gear (hamburger) button opens a session-control popup:
  Pause/Resume (one shared flag with the Space hotkey) and Exit Game (inline "Really quit?" confirm,
  since there is no save). Esc toggles the same popup.
- **Corp emblem system** — the geometric corporation emblem promoted to a shared `ui::icons` glyph
  family (shape + identity colour, both a pure function of the corp id), rendered for the player and
  rivals wherever a corp is identified: the profile card, the Selection header, on-canvas
  building/HQ owner tags, and the rival hover card. Card, markers, and canvas tint now share one
  colour source of truth.
- **Commercial-fog hover line** — the Solar-canvas body hover tooltip now carries a short activity
  read keyed on the commercial-sphere fog (the first of the two BL-089 deferrals).

### Changed
- **Economy-panel legibility** — the Corporation-balances table widened to show full corp names and
  full balances (no single-character cells); the Workforce, Stockpile-pools, and Markets tables
  un-cramped the same way; the redundant per-building table removed (per-building profitability
  lives in the Corp Dashboard).

### Fixed
- **Construction panel overlap** — the Construction panel no longer occludes the bottom-left
  Selection element during placement, so the build front door and the Thrives/Valid affordance
  readout stay visible while a build is armed.

### Deferred
- **Proximity-glimpse peek** — the third commercial-fog illumination geometry (a faint "peek" from
  routing past a frontier body) held back: it needs a save-seam field or orbital back-computation,
  disproportionate for a polish minor. Re-assess at the v0.1.0 boundary.

## [0.0.8] — 2026-07-04

Discovery & intelligence — the economy gains information asymmetry, and the human / industry / player
layers become legible on the map.

### Added
- **Survey system** — bodies start unsurveyed; a player-dispatched survey (credit cost + N ticks)
  reveals the tile map and deposit bands. Home begins surveyed. Surfaced on the Solar/Planetary
  canvases and the Selection panel.
- **Competitor-visibility model** — rival buildings are visible on-canvas; their production and
  stockpiles are private; market prices and supply/demand aggregates are public (the intelligence channel).
- **Population legibility** — the Population lens re-keyed to workforce efficiency; the Selection panel
  and hover read scale / local habitability / workforce cap.
- **Population-centre markers** — the generated settlements drawn as clustered conurbations with tiered
  skyline markers (City+ labelled), civic-neutral by default and host-nation-tinted under the Country lens.
- **Player presence** — always-on home-cluster ring + an HQ star on the origin building + a home halo
  on the home body in the Solar canvas.
- **Industry-density lens** — the already-live nation-owned substrate rendered as a resource-weighted
  throughput field (occupation × terrain richness), distinct from the population read.
- **Ambient opportunity read** — the Opportunity margin surface confirmed glanceable at rest with its
  on-canvas key.
- **Persistent trade routes** — durable body-pair lanes recorded from convoy traffic (the substrate the
  fog reads).
- **Commercial-sphere fog** — a body-level activity fog (unknown / known / stale / visible) lit by the
  player's own trade and independent of the survey fog: Solar activity badges + lit corridors and a
  coarse market-pulse section in the Selection panel.
- **Budget cluster** (shipped early from the v0.0.9 theme) — itemised cashflow breakdown, debt-interest
  visibility, per-building profitability, and a display-options window.

### Changed
- Verification tooling: added the `trade_routes` and `commercial_fog` headless harnesses and new visual
  golden scripts (population markers, player presence, industry lens, ambient opportunity, commercial fog).

> **Note:** v0.0.6 and v0.0.7 were developed and merged as batches but never formally cut; their tags
> were reconstructed retroactively at the theme-boundary commits (`v0.0.6` → `ab7e28f`,
> `v0.0.7` → `61d9946`). v0.0.8 is a normal cut.

## [0.0.7] — 2026-06-30

Interactive & legible — the economy becomes something the player *drives*, not just observes.

### Added
- **Player construction** — armed build mode with placement validation (terrain/deposit affinity,
  slot rules), a resource build-cost model, and the construction panel as the build front door.
- **Recipe & workforce control** — per-building recipe selection and workforce allocation, with the
  contention scalar feeding production and wages.
- **Population centres** — static MVP placement plus the dynamic habitability→workforce feedback
  loop that grounds labour supply per body.
- **Selectable entity markers** — buildings and market centres are hit-testable on-canvas with
  lens-contextual hover cards; the pinned **Selection panel** shows polymorphic per-entity detail.
- **Placement-suitability surface** — armed build mode tints affinity tiles so siting is legible.
- **Lens-driven selection** — the Selection panel content follows the active map lens.
- **Full hotkey system** — unified keyboard control with an F1 cheat-sheet overlay.
- **Trend plots** — balance/market trend graphs in the Economy panel and Market Ledger.
- **App-driven mouse** — deterministic pointer input for reproducible verify captures.

### Changed
- **Cross-platform build** — Linux is the primary target with Windows CI; source-portability fixes
  and a GCC-14 / sol2 v3.5.0 regression guard.

## [0.0.6] — 2026-06-17

Improved core-loop — the market gains depth, spatial reach, and a read surface.

### Added
- **Matched price-time order book** — `clear_markets` restructured around an order book so bids and
  asks match by price then time rather than a single clearing pass.
- **Saturated substrate** — nation-owned background supply and demand that fills the world so the
  player trades into a live economy rather than a vacuum.
- **Supply convoys** — inter-body convoy routing with logistics costs, driving spatial **price
  convergence** between bodies.
- **Ledger family** — the Market, Balance, and Construction ledgers plus the Corp Overview dashboard,
  wired into the nav pane.
- **Market-centre seeding** — market centres seeded from population centres; multi-market dashboard.
- **Warm-start surface** — pre-game economy warm-up so a campaign opens on a settled market.

### Changed
- Backlog/process refit: JSON backlog (`backlog.json`) with schema v2 (`waits_on` → `requires`),
  the `verifier-review` skill, and the native-only git-write standing rule (BL-058).

## [0.0.5] — 2026-06-16

Layer 4 foundations, the map-lens family, and golden-image visual verification.

### Added
- **Placement-rules seam** — a reusable `src/world/placement_rules.*` validity check, shared by
  world generation and (future) player construction so both honour one set of rules.
- **Economy-stability harness** — `tools/verify/econ_stability.cpp`, a multi-tick stability run
  over the SDL/Lua-free economy (insurance before UI piles onto the loop).
- **Workforce pool (step 1)** — a per-`(corp, body)` labour pool with a proportional contention
  scalar feeding both production and wages (`world::workforce_supply`,
  `economy_report::workforce_contention`, `building_report::effective_workforce`).
- **Map-lens family** — Resource, Market, Population, and Scarcity overlay modes (`overlay_mode`)
  with per-body magnitude normalisation, on-canvas keys, a shared resource selector, and the
  Circumplanetary per-body market price strip.
- **Tile-centred markets** — multiple markets per body, each centred on a tile, generalising the
  single per-body exchange so price can vary within a body.
- **Player sell orders** — standing sell-orders with floor prices (`ui_state.sell_orders`) — the
  first half of the preferential-purchasing model.
- **Layer 4 UI groundwork** — the construction-panel scaffold, the Tile Selection element as the
  build front door, the informative hover-card/tooltip design, and uniform `ledger_chrome`
  ledger-window chrome.
- **Golden-image verification** — a PNG reader and per-pixel diff with a `--bless` workflow in the
  `ProjectIo --verify` harness, upgrading visual checks from eyeball-only to automated pass/fail.

### Changed
- **World generation** — orphan-island assignment (0 unclaimed land tiles, was ~12%), lean
  focus-shaped corporation starting holdings, and a corporation-generation realism pass
  (pre-game profit modelling).
- **Resource realism** — refreshed resource set and pricing.

### Docs & process
- Renamed the TODO backlog to **OPENS** with the Brief design-state model; added the **ROADMAP**
  milestone map and near-term publish plan. Defined **Batch Publish**, brief-spanning
  requirements, naive progress markers, and Brief timestamping/precedence. Completed the v0.1.0
  design pass (supply, infrastructure, inter-body markets, lens system) and migrated the
  requirements record to `requirements.json`.

## [0.0.4] — 2026-06-15

Layer 3 economy and the visual-verification toolchain.

### Added
- **Economy systems** — per-body market clearing, price resolution from local supply/demand,
  deposit depletion, corporate balance and operating costs, and a recipe registry
  (`src/world/{market_clearing,economy_system,budget_system,recipe_registry}.*`).
- **Economy observability** — an economy ledger panel and a player-balance header
  (`src/ui/{economy_panel,header_panel}.*`), with pre-game economy ticks.
- **Visual-verification harness** — `ProjectIo --verify <script>` headless capture mode and
  the `scripts/verify/*` checks (corporation lens, economy panel, header, selection go-to).
- **Headless logic harnesses** — `tools/verify/{econ_harness,world_audit}.cpp`.
- **Tooling/skills** — `verifier-visual`, `verifier-headless`, `commit`, `scoped-commit`
  skills and the `.claude/settings.json` allow rules.
- **Canvas command layer** — `src/ui/canvas_command.*`.

### Changed
- Repo hygiene: stopped tracking `backups/` (now gitignored only); set up the GitHub remote.

## [0.0.3] — 2026-06-14

Environment — the world-generation spine.

### Added
- Nation generation (Voronoi BFS territory) and corporation generation (nation assignment,
  starting-asset placement, financial profile).
- Two-axis terrain model (composition × landform) and tile-deposit tuning.

## [0.0.2] — 2026-06-13

Layer 2 finalisation.

### Added / Changed
- Standardised body grids, infinite side-scroll, and a zoom floor across the canvases.

## [0.0.1]

Initial prototype snapshot — application shell, canvases, and the hard-coded world.

[Unreleased]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.1.15...HEAD
[0.1.15]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.1.14...v0.1.15
[0.1.14]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.1.10...v0.1.14
[0.1.10]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.1.9...v0.1.10
[0.1.9]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.1.8...v0.1.9
[0.1.8]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.1.1...v0.1.8
[0.1.2]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.1.0...v0.1.2
[0.1.1]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.1.2...v0.1.1
[0.1.0]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.9...v0.1.0
[0.0.9]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.8...v0.0.9
[0.0.8]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.7...v0.0.8
[0.0.7]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.6...v0.0.7
[0.0.6]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.5...v0.0.6
[0.0.5]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.4...v0.0.5
[0.0.4]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.3...v0.0.4
[0.0.3]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.2...v0.0.3
[0.0.2]: https://github.com/Ben-Booth-125/Project-Io/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/Ben-Booth-125/Project-Io/releases/tag/v0.0.1
