# Project Io — Nation Generation

Nations are the political and territorial layer overlaid on the tile map. They define the
geopolitical backdrop at campaign start: who controls what land, what the diplomatic starting
positions are, and what legal context corporations operate within.

This document covers **only the generation strategy** — how a nation comes to exist. What a nation
*does* afterwards is owned by **[`docs/politics/NATIONS.md`](../politics/NATIONS.md)**: the
treasury, law authorship and the nation-behaviour grant (Ben, 2026-08-18). Where the two disagree
about a field, generation wins on how it is **set** and NATIONS.md wins on what it **means**.

**Nations are backdrop to the player, not the player.** The player is a **law subject** — a
mercenary company in the live arc (`docs/development/ROADMAP.md` § The two arcs) — so the
mechanics a nation holds stay a nation's and do not become the player's levers.

---

## Design principles

**Territory derives from the tile map.** Nation boundaries are placed over an already-generated
body tile map. A nation's resource profile is a consequence of the tiles it controls, not a
separately authored value. This means a nation rich in volcanic or metallic terrain will naturally
tend toward heavy industry; a nation controlling fertile grassland will lean agricultural.

**Procedural with tuned parameters.** Unlike the prototype tile map (fixed seed, authored solar
parameters), nation generation is procedural from the start. A campaign seed governs all
randomness. Tunable parameters — seed density, size variance, fragmentation — allow world
character to be shaped without re-authoring. Different seeds produce different political maps
over the same tile geography.

**The nation count is an outcome, not an input.** There is no nation-count target anywhere in the
pipeline and no campaign-setup slider for it. Seeds scale with the body's habitable land area, and
the merge pass absorbs every nation that fails to hold a minimum viable territory. How many
nations a world ends up with therefore falls out of its landmass and coastline: a large, dry
continental homeworld supports more nations than a small, ocean-heavy one. The two constants that
shape it are `land_tiles_per_seed` and `min_nation_tiles` (`nation_params`,
`src/world/nation_generation.hpp`) — both are retuning dials on *character*, not on count.

**Same pipeline, all bodies.** Nation generation runs only on bodies with sufficient habitable
area — the homeworld. The pipeline is body-agnostic; it reads tile
compositions and applies the same logic regardless of which body it operates on.

---

## Generation pipeline

### Pass 0 — The history ladder

The ladder is BL-221 (pre-national ladder). Before any seed is placed, `run_history_ladder` (`src/world/history_ladder.cpp`) runs over the
finished tile map and the body's planetology state. It scores every tile for agrarian value
(`agrarian_score` — composition, landform, habitability), picks the independent **cradles** by
greedy argmax with a separation floor, and measures the **barrier field** (ocean, mountain,
canyon, dead ground). From those it computes `fragmentation_q`, `conquest_cost_q` and
`exit_cost_q` (integer, 0–1000 — gate-path arithmetic stays integer per `PLANETOLOGY.md`
§ Determinism).

`nation_params_from_ladder` then **modulates the nation parameters** from those scalars: a
fragmented world lowers `land_tiles_per_seed` and `min_nation_tiles` (denser seeds, smaller
viable survivors — both clamped to [½, 1½] of the base so a tuning mistake cannot produce a
one-nation or thousand-nation world), and a high conquest cost raises `min_seed_separation`.
The ladder **drives** the political map rather than narrating one it was handed (Ben,
2026-07-30). A world with no cradles (below a land biosphere) leaves the caller's defaults
untouched. Full stage design: `../lore/HISTORY.md`.

### Pass 0b — Settlement & industrialisation

The settlement pass is BL-218 (nations rewrite). Between the ladder and the seeds sits `run_settlement` (`src/world/settlement.cpp`), which turns
the ladder's cradles and the creeds' pantheons into **regions** — the unit that actually gets
settled, industrialised, fought over, and read by corporation generation.

Per region it records: the anchor tile, the **culture it inherits** (its nearest cradle's, so
the pantheon distribution is a map of who walked where rather than a per-region re-roll), its
**ancient endowment** (farm / ore / energy / harbour, surveyed once over the anchor's window), a
founding year derived from how strongly the ground invited settlement, and — where the ground can
pay for it — the year its furnaces lit.

Two details are load-bearing:

- **The endowment scores are world-relative, not absolute.** A region scores 500 on a class when
  it holds the world's average of it, 1000 at twice the average. "Ore country" only means something
  next to the rest of the map, and the relative form also survives the `deposit_scalar` abundance
  tier without re-tuning — a lean world still has its own ore regions, they are simply poorer in
  absolute terms. The industrialisation gate is *above-average* fuel, so an average region does
  not industrialise.
- **The creed and the deposit are the same fact seen twice.** A culture raises a forge god only
  where its cradle window held ore (`../lore/CREEDS.md`), so a region whose people kept that god
  *and* sits on ore lights its furnaces earlier. The charter culture's oath god buys a smaller
  bonus — Stage 3's contract law reaching capital.

### Pass 1 — Seed placement

**The seeds are the region anchors.** `nation_params::seed_tiles` is filled from
`settlement_seed_tiles` and consumed verbatim; the random placement described below is the
fallback for any body with no settlement pass.

The design in one line — *seeding is settled, expansion is tuned*. Pass 1b/2's growth machinery
is tuned for size variance (BL-053, nation size variance); the settlement pass replaces its
**inputs**, which keeps the tuned behaviour while making the variance **emerge** from where
people actually settled rather than being dialled in.

A set of nation seeds are placed across the body's landmass tiles. The seed count is **derived
from the habitable landmass, as modulated by the ladder** (Pass 0): one seed per
`land_tiles_per_seed` non-ocean tiles (base 80, scaled down on a fragmented world). Placement
avoids stacking seeds in small geographic areas: a minimum separation distance (in tiles) is
enforced between seeds, which also caps how many seeds physically fit whatever the density asks
for.

A seed is only a **candidate core**, not a nation — the density is deliberately several times
denser than the surviving count, because Pass 2c absorbs most of them.

Seeds prefer habitable compositions (grassland, forest, wetland) but can land on any non-ocean
tile. This produces nations that form around productive cores rather than uniformly across
terrain.

### Pass 1b — Growth weights

Each seed is assigned a **growth weight** drawn from a skewed distribution (the cube of a
uniform draw → most seeds small, a few large). A seed's BFS step cost is divided by its weight,
so high-weight seeds expand cheaper, reach further, and claim more land. This turns the
near-uniform Voronoi cells the flat approach produces into a **strongly varied size
distribution** — a few large "great powers", several mid-sized, many small.

### Pass 2 — Territory expansion (Voronoi BFS)

Each seed expands outward via BFS. Ocean tiles are never claimed. The expansion follows a
weighted Voronoi approach: step cost decays with distance from the seed and is **divided by the
seed's growth weight** (Pass 1b); high-`H` tiles (mountains, highlands) act as soft barriers —
they can be claimed but reduce the expansion budget of the claiming nation, producing mountain
ranges that naturally fall near or at borders.

Expansion continues until all claimable land tiles are assigned.

### Pass 2b — Orphan-island assignment

The cardinal-adjacency BFS cannot cross water, so landmasses disconnected from every seed
would otherwise stay unclaimed. A deterministic post-pass closes that gap: unclaimed
non-ocean tiles are grouped into connected components (cardinal adjacency, column-wrapped),
and each whole component is assigned to the nation owning the **nearest already-claimed land
tile across the water** (by Chebyshev grid distance; ties break to the lower nation index,
then the lower tile index). After this pass every non-ocean land tile on the body belongs to
a nation — there are no unclaimed islands.

### Pass 2c — Light "in history" merges: the size floor

A deterministic post-pass gives the map a "grown in history" feel without simulating history.
The body is **over-seeded** in Pass 1, then the smallest nations are repeatedly **absorbed into
their largest cardinally-adjacent neighbour**. The stopping condition is a **minimum viable
territory, not a target count**: the pass ends when the smallest surviving nation clears
`min_nation_tiles` (80 by default — one seed's worth of land, so a nation must end up holding at
least what its own seed was budgeted), or when only one nation is left. Surviving owner indices
are then compacted.

Because the loop stops on *size* rather than *count*, the number of nations is whatever the
geography leaves standing. The result amplifies the size spread (rich-get-richer) and produces
**irregular, non-convex borders** — a nation may be the union of a core and an absorbed
neighbour. Tie-breaks are fully deterministic (smallest by tile count then lowest index;
absorbing neighbour by tile count then lowest index; an island with no land neighbour is absorbed
into the globally largest nation). This pass is *not* itself a source of fragmentation — it only
ever merges a nation into a neighbour it already touches, so it cannot cut one in two.

### Territorial fragmentation, measured

The settlement-sim path rests largely on the claim that **fragmentation falls out of it for
free** — a growth front that crosses a strait and stalls, or a nation cut off by a rival's
expansion, leaves an exclave nobody authored. The claim is measured rather than assumed (BL-284,
fragmentation audit): `tools/verify/world_audit.cpp` counts each nation's **non-contiguous
territorial components** across a six-seed sweep and attributes every exclave to one of two
producers.

The attribution is exact rather than heuristic, and it turns on Pass 2 being water-blocked:

* Pass 2 can only claim tiles on a landmass that already holds a seed, and on such a landmass it
  runs to exhaustion — so every land tile there is claimed by the BFS.
* Pass 2b therefore only ever fires on a **seedless** landmass, and hands the whole thing to one
  nation in a single act.

So an exclave on a **seeded** landmass (one holding at least one region anchor) is **emergent** —
produced by the sim, whether by a stalled front, a rival cutting it off, or a Pass 4b rupture
redrawing the border. An exclave on a **seedless** landmass is **Pass 2b** cleanup and is not
evidence for the claim.

**What the measurement says** (seeds 0–5): 196 exclaves, **60 emergent** and 136 from
Pass 2b — 31 % of exclaves by count, but **49 % of exclave tiles** (940 of 1930). Every swept seed
produced emergent exclaves (6–17 each), and roughly half of all nations hold at least one. The
promise pays: fragmentation is real and sim-produced, but Pass 2b's long tail of tiny orphan
islands dominates the raw *count*, so the honest headline is the tile share, not the component
share. The audit asserts only a wide bar — at least 6 emergent exclaves across the sweep — to be
tightened once the distribution has been watched.

### Pass 3 — Resource profile derivation

Each nation's resource profile is computed by summing the deposit profiles of its tiles,
weighted by composition type. The result is a read-only descriptor — `iron_ore_abundance`,
`agricultural_abundance`, `petroleum_abundance`, etc. — used to characterise the nation
economically for diplomacy initialisation and corporation generation.

### Pass 4 — Political character assignment

**The three axes are OUTPUTS of the settlement record, not an independent draw.**
`derive_national_character` (`src/world/settlement.cpp`) runs immediately after
`generate_nations` — it has to, because every derivation needs the political outcome — and
overwrites what the random pass drew. The random draw is the fallback for bodies with no
settlement pass.

| Axis | Derived from | Rule |
|---|---|---|
| `expansionism` | the **border-contest integral** | Shared-border share of a nation's territory, plus the settled weight pressed against it (rival regions within 12 tiles). A nation that expanded into empty land scores near zero on both; one that grew by pressing against neighbours scores high. ≥600 aggressive, ≥280 moderate, else passive. |
| `economic_focus` | the dominant class of the regions settled **during industrialisation** | Not of the tiles it merely holds — what a nation built on is what it becomes known for. Ore/farm → extraction, energy → processing, harbour → trade. An early first-mover's extraction reads as processing instead (see below). |
| `ideology` | **industrialisation timing against neighbours** | A rank, not an absolute: earliest tercile → mercantile, middle → technocratic, latest → authoritarian (late catch-up under competitive pressure trends statist/directed). A nation that never industrialised reads isolationist. |

None of the three needs a new roll — each is computable from the run's own record, which is the
standing requirement that generation produces consequences rather than dice.

### Pass 4b — The historical ruptures

`resolve_historical_ruptures` then fires a bounded set of ruptures over the most-contested
nations, reusing `planetology.hpp`'s class-agnostic `resolve_checkpoint` rather than inventing a
second branch mechanism — the second checkpoint class of BL-217 (branch checkpoints). Branch
eligibility is a **filter, never a weight** (BL-217's rule): a
nation with no land neighbour cannot go to war, a single-region nation cannot collapse. Every
attempt appends a `checkpoint_record`, failures included.

Each branch is a **transform on state**; the line it appends is a *record of* the transform, never
a substitute for it:

- **Collapse** — the two most peripheral regions pass to a bordering neighbour and their
  industrial clock resets; abundance falls 20%. Ideology unchanged.
- **War** — the contested border redraws toward the stronger (bounded at a quarter of the loser's
  territory, so the non-hegemony invariant — BL-224 — is respected rather than spent); the loser's
  posture rises to aggressive (grievance is the point of the axis); both lose abundance; **the
  victor's gods travel with the border**, and part of the loser's record is **destroyed** — see
  below.
- **Revolution** — territory untouched, the ideology axis flips, abundance takes a one-off hit.
  The cheapest transform and the one that most changes how the nation later behaves.

**The record is not safe.** Where a war takes a region, the lines naming it are erased and a
dated **lacuna** is left in their place — "what the *X* wrote of itself does not survive the *Y*
occupation", with a count of the lines lost. The hole is visible rather than silent, which is the
difference between a history that was fought over and one that was merely written. A conquered
region keeps its founders in `founding_culture` and its conquerors in `culture`, so the erasure
is of the *record*, never of the fact — which is exactly the pair a later religion or population
layer needs to describe a grievance.

Across a six-seed spread, four worlds lost part of their record to a war.

---

The random pass — the fallback path for a body with no settlement record, and the description of
the axes themselves:

Each nation carries a set of parametric political attributes; on the fallback path they are drawn
from a seeded random pass. These seed the sentiment graph and the starting tone of diplomatic
interactions (`docs/politics/RELATIONS.md` owns sentiment; BL-545, sentiment substrate).

| Attribute | Values | Effect |
|---|---|---|
| `ideology` | `authoritarian`, `technocratic`, `mercantile`, `isolationist` | Shapes starting sentiment toward other nation types |
| `expansionism` | `passive`, `moderate`, `aggressive` | Modulates early military posture |
| `economic_focus` | `extraction`, `processing`, `trade` | Biases nation investment behaviour (see § Open items) |

Sentiment between any two nations at generation time is a function of ideology compatibility,
territory adjacency, and resource overlap. High-overlap neighbours start with lower sentiment
regardless of ideology.

### Pass 5 — Naming

**There is no name bank** (BL-290, native nation names). A nation is named in the **tongue of the
culture that settled the region its seed grew from** — the same phoneme inventory the creeds pass
(BL-235) coined that culture's own name and its gods from. Naming *consumes* the phonology the
generation chain already produces; it does not roll a second one.

The plumbing:

- `world/tongue.{hpp,cpp}` owns the `tongue` (onset / vowel / coda inventory), the word builder,
  and `coin_lexicon`. `creeds.cpp` rolls the tongue and **retains** it on `culture::speech`.
- `hard_coded_world.cpp` carries each region's tongue across into `nation_params::seed_tongues`,
  parallel to the `seed_tiles` anchors the settlement pass supplies.
- Pass 5 gives each surviving nation the speech of the **lowest-indexed seed still inside it** (a
  seed absorbed by the Pass 2c merge contributes nothing — the surviving core names the realm), then
  builds the name with one of three structural forms: bare name, epithet + name, name + realm word.

**The structural words are native too.** There is no "Republic", "Commonwealth", "Free" or
"United": `coin_lexicon` coins each tongue its *own* morphemes for *realm*, *town* and *standing epithet*,
from that tongue's own sounds. The lexicon is a **pure function of the tongue** (its stream is
seeded by hashing the inventory, not by a caller's RNG), so one culture coins the same words
wherever it is consumed — nation names in one pass, city names in another — without those passes
having to share a stream. Two nations of one culture therefore read as kin in both the sounds and
the scaffolding, and no generated name carries an English or Latin morpheme.

A body with no culture layer (no settlement pass, or a caller supplying bare seed tiles) falls back
to **one** tongue rolled in Pass 5 for the whole body, not a per-nation re-roll: an unwritten history
is still a shared one.

**Region names** follow it too (BL-348, native region names). A region is `<People> <Region>`,
and both halves are native — a native people-name beside an English *Reach* or *Coast* would put
two naming systems side by side in one string and read as a bug rather than a style.
`coin_lexicon` coins each tongue **nine** region words, and `region_word` asks the region's tongue
rather than returning a literal.

Nine, because the choice of region word is **positional** and stays that way: a 5-band
north→south axis and a 4-sector dawnward→outer axis, so the name still carries a fact about the
ground rather than becoming decorative. An English table is the fallback for a culture whose
tongue cannot coin at all. Sample (default seed): *Dothkua Shethdeith*, *Rekmaik
Taikme*, *Rairuath Rairith*, *Shuahualmi Beduas*, *Kuakuam Ruatem*, *Nuathorsho Ruvor*,
*Nuathorsho Rove*, *Tatadith Hezo*, *Kuakuam Muamem* — the kinship is emergent here as well, since
*Nuathorsho Rove* / *Nuathorsho Ruvor* and *Kuakuam Ruatem* / *Kuakuam Muamem* draw their region
words from one sound system. The nine words are drawn **last** in `coin_lexicon`, so they perturb
no nation or city name drawn before them.

**City names** follow the same rule. `generate_city_name` takes a tongue and suffixes the culture's
own coined settlement morpheme; there is no `-ton` / `-ford` / `-haven` / `-burg` bank. Because
population centres are placed *before* the creeds exist, they are first named from a tongue rolled
for the body, then re-named per-region by `name_population_centres` (`world/city_names.cpp`) once
the settlement record exists, using the **nearest region's** culture — the same "whose gods" rule
the settlement pass uses.

### Pass 6 — Substrate density

The last pass in `generate_nations` writes the geography of settlement density. For every
nation-owned tile it finds the nearest population centre on the same body (Chebyshev distance)
and computes a **density ripple**: `max(0, 1 − dist/8) × centre.scale`, taking the strongest
centre. That value is written to `tile_component.substrate_density`.

**Nothing reads the field.** The background economy is carried by real background firms
(`CORPORATION_GENERATION.md` § Pass 6), not by a per-tile capacity aggregate, and the Industry
lens (`body_surface_canvas.cpp`) reads those firms' plant on each tile directly. The ripple is a
settlement-density description with no consumer. The pass requires population centres to already
exist — which is why `generate_population_centres` runs before `generate_nations` in
`hard_coded_world.cpp` (see § Settlement generation below).

---

## Settlement generation (companion pass)

Not part of `generate_nations`, but sequenced around it and documented here because the nation
passes read it.

**Population centres** — `generate_population_centres` (`src/world/population_generation.cpp`)
runs on the homeworld *before* the ladder and the nation pass. It places 20–40 centres (grid tiles /
1000, clamped) on valid tiles (`placement_rules::can_place_population_centre`), one at a time
with a 3× weighting for tiles adjacent to an existing centre, so agglomeration is progressive.
Each centre draws a **scale** 1–5 from a weighted distribution (40/30/20/8/2%) mapping to
10k–5M population, and is named from an independent seeded stream (`generate_city_name`, in a
tongue rolled for the body, replaced later by `name_population_centres` — see Pass 5) so
naming never perturbs placement.

**Market carving** — after nations and corporations exist, `hard_coded_world.cpp` seeds the
homeworld's markets from the centres. Markets anchor to population-centre tiles but are
**resource-carved** (BL-096, resource-carved markets): each nation's tradeable-raw
concentration (mean deposit per owned tile, seeded jitter ×[0.85, 1.15]) is classified against
the cross-nation mean into a population-scale gate — ≥1.30× mean → gate 2 (fracture: more of
its centres get markets), <0.70× mean → gate 4 (fold: its small centres route to a
neighbour's market via `market_for_tile`), otherwise gate 3. A centre seeds a market only if
its scale clears its nation's gate; one unanchored fallback market is seeded if none qualifies.
Endemic goods are then priced by distance from where they grow (BL-191, endemic pricing).
`GENERATION_STRATEGY.md` § The economic premise carries the corporate-contest term the carve
also reads.

---

## Scope

The homeworld is the only body with nation generation. The other prototype bodies are unclaimed
territory.

The homeworld's nation count is **not authored**. The size floor and seed density are
fragmentation-modulated, and the default seed settles at **43 nations**. A count that high is the
ladder doing its job, settled deliberately (Ben, 2026-07-30 — recorded in `../lore/HISTORY.md`
§ Implementation): let naturally different cultures emerge; a war/conflict stage narrows the
count if needed, rather than tuning the generator to a target. Sizes stay strongly varied. The
base knobs are `land_tiles_per_seed`, `min_seed_separation`, and `min_nation_tiles`
(`nation_params`), modulated by `nation_params_from_ladder`; the New World setup screen has
**no nations slider** — the count is not the player's to set.

**What planetology data nation placement does and does not consume.** The ladder consumes the
planetology state directly (`life_stage` peak, `arable_share`, `endemics`). `generate_nations`
itself reads only tiles: seed preference over habitable compositions and Pass 3's deposit-summed
resource profile are *indirectly* downstream of the biosphere, but no endowment, region, or
biography data is read at placement time.

What a nation does once it exists — its treasury, the laws it authors, the budget it allocates
and the stance it derives — is [`docs/politics/NATIONS.md`](../politics/NATIONS.md)'s, not this
document's.

---

## The carve is watched

The territory carve has a **surface** (BL-305, loading-screen carve): it is drawn live on the loading screen
(`app::draw_building_carve` in `src/core/app.cpp`, `app_screen::building`), in the seconds
between the New World wizard's "Begin" and the first frame of play. Pass 2's weighted Voronoi
BFS publishes each tile **at the moment it settles**, so the player watches nations grow
outward from their region anchors and stop where a mountain, a coast, or a rival stops them.
Pass 2b and 2c rewrite indices wholesale, so the map is restated once when they finish - the
islands join, the undersized realms are absorbed, and the final count is then reported.

Live, **not a post-hoc replay**. There is no recording and no second pass: the screen is
reading the carve as it happens.

**The mechanism, and its one hard constraint.** Generation runs on a worker thread; the loading
screen renders on the main thread. The only thing they share is `generation_progress`
(`src/world/hard_coded_world.hpp`) - **atomics only**: the worker writes, the renderer reads,
no mutex, no callback into UI code, no re-entrant rendering. `generate_nations` takes an
optional `generation_progress*` and treats null as "publish nothing". The carve is published as
a fixed-size array of per-cell relaxed atomics holding *nation index + 1*, with an epoch counter
the renderer watches so a still frame costs nothing. Per-cell rather than double-buffered on
purpose: each tile is claimed exactly once, so a half-observed publish is a valid **earlier
frame of the animation**, not a torn read. The counter-guarded arrays (the charter marks and
rows) do use release/acquire on their count, because there the reader must not read a slot
before it was filled.

**It is a pure tap.** The publish calls are write-only, consume no randomness, and change no
branch, so a world generated with the screen watching is byte-identical to one generated
headlessly - the standing determinism invariant is the binding constraint and the visualisation
is subordinate to it. Every existing headless caller (harnesses, `--serve`, `--verify`) passes
null and is unaffected.

The carve is drawn in `ui::palette::nation_colour` - the same palette the in-game **Country
lens** uses - so the map a player watches being carved is the map they meet again under the
lens. The nation entities do not exist while the BFS runs, so the map is keyed by index until
`nation_id_base` is published (ids come out of one tight `create_entity` loop, so index *i* is
`base + i`); the borders have already settled by then, and the recolour reads as them becoming
real nations.

---

## Relationship to corporations

Corporations are legally registered within a nation and operate within its territory by default.
The nature of this relationship — what obligations it creates, what it restricts, and how it
evolves across Eras — is an open design item. See `CORPORATION_GENERATION.md` § Open items and
`docs/politics/NATIONS.md`.

---

## Open items

**Nation system design.** What nations actually *do* — tax corporations, issue licences, declare
war, provide infrastructure — is owned by [`docs/politics/NATIONS.md`](../politics/NATIONS.md),
including the calls it leaves open. The generation layer exists so the world has political
structure from turn one independent of how much of that system is designed.

**Era-based reform.** There is a working hypothesis that later Eras will see corporations
become de-facto powers above states, with the nation layer diminishing in authority as the
game progresses. This has significant implications for what nations need to be able to *do*
and how that capability decays. It is noted here and unscoped.

**Fragmentation and history.** Pass 2c is a *light* "in history" pass (over-seed +
merge-below-the-size-floor, giving varied sizes and irregular borders), and the settlement sim
produces emergent exclaves on top of it. Disputed zones and contested tiles — fragmentation as a
live political fact rather than a border shape — have no owner in the generation layer; BL-518
(war redraws borders) is the first thing that moves a border after generation.

**Off-homeworld politics.** As colonies establish on other bodies, some form of jurisdiction
and governance will be needed there. Whether this extends the nation model or introduces a
new corporate governance model is unresolved.
