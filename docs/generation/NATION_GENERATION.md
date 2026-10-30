# Project Io — Nation Generation

> **Settles:** how a nation comes to exist — where a seed is placed, how territory expands,
> how the history's realms fold into nations, what the size floor merges away · how a resource
> profile and a political character are derived from the ground a nation holds · how a nation
> inherits its realm's name and colour · what treasury and substrate density it opens with · how
> settlements are placed alongside · how many nations, and how many majors, a world ends with.
> **Not here:** what a nation *does* once the campaign runs (../politics/NATIONS) · what
> industry its ground opens with (CORPORATION_GENERATION) · which ladder produced the history it
> inherits (../lore/HISTORY).
> **Confused with:** ../politics/NATIONS.md, CORPORATION_GENERATION.md, ../lore/HISTORY.md.

Nations are the political and territorial layer overlaid on the tile map. They define the
geopolitical backdrop at campaign start: who controls what land, what the diplomatic starting
positions are, and whose ground each building stands on — a nation owns every building on its
ground (Ben, 2026-10-23; `../politics/NATIONS.md` § What a nation owns).

This document covers **only the generation strategy** — how a nation comes to exist. What a nation
*does* afterwards is owned by **[`docs/politics/NATIONS.md`](../politics/NATIONS.md)**: the
treasury, law authorship and the nation-behaviour grant (Ben, 2026-08-18). Where the two disagree
about a field, generation wins on how it is **set** and NATIONS.md wins on what it **means**.

**The player is one of the nations this doc makes (Ben, 2026-10-10).** It plays a **major** nation
and builds its own buildings (Ben, 2026-10-23; `docs/CONCEPT.md` § Player identity). Generation makes the
player's nation exactly as it makes a rival's; nothing here is placed, sized or favoured for the
player.

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

### The nation count is a target (Ben, 2026-10-10)

**Overturning** the standing ruling that the count is an outcome, never an input (Ben, 2026-07-30),
and the "no nations slider" that went with it. The player sets two numbers on the wizard's setup round
(`docs/ui/STARTUP.md` § Rounds), before the Life round builds the world:

| Target | Means | Range |
|---|---|---|
| **Total** | nations alive on the homeworld when generation ends | at least the major target, **at most 40** |
| **Major** | nations generation raises to a full actor when generation ends (`docs/GLOSSARY.md` § Major nation) | **1 to 6** |

**Both targets are always hit (Ben, 2026-10-10).** A world that ends off either target is a defect,
not a reported outcome — the reverse of NR-911's report-and-count rule, for these two numbers only.

**A major is raised, not chartered (Ben, 2026-10-23).** A major is a nation generation raises to a
full actor, chosen by the count target from where capital accumulated: the nations whose ground
holds the most industry points. It is no longer a nation holding a champion; the champion and the
specialist are retired with every corporation (`docs/CONCEPT.md` § Player identity). Every other
nation is a minor.

**How a target is hit: a lean, then in-world reconciliation (Ben, 2026-10-10, option C).** A count
is never imposed by deleting, splitting or relabelling a nation. Two stages, in order:

1. **The lean.** Each target conditions the passes that make the number. The total leans the
   history's **conquest appetite and secession pressure** — fewer nations wanted, more appetite and
   less secession; more wanted, the reverse (Ben, 2026-10-11; seed density cannot carry it, because
   on a settled world the seeds are the history's region anchors and `land_tiles_per_seed` is never
   read). The major target leans how Industrialisation concentrates charter capital
   (`INDUSTRIALISATION.md` § 1). A lean lands near the target; it does not promise it.
2. **Reconciliation, by the history's own verbs.** Whatever gap the lean leaves is closed by events
   the sim already knows, each recorded in the history with a cause a player can read:
   - **too many nations** — conquest and absorption, the Empires and Exploration verbs;
   - **too few nations** — a province seceding (a subject is already its own nation at the fold, so
     freeing one adds none);
   - **too few majors** — the **richest minor is raised** to a major;
   - **too many majors** — the **poorest surplus major is demoted** to a minor. Its ground and every
     building on it stay its own: the 2026-10-11 fold moved a major's *specialists* into the richest
     major's champion, and with no corporations there is nothing left to move (decided 2026-10-23,
     on Ben's behalf; NR queue). The total is untouched.

**The total is reconciled in Industrialisation's closing rounds; the majors after the landscape
search (Ben, 2026-10-11).** The two counts come into being at different points of the pipeline, so
each is reconciled where it can be read:
- **The total, inside the history.** The size floor moves into the history as **closing-round
  absorptions** (§ Pass 2c): a realm below the minimum territory is absorbed by its neighbour as an
  event of the span, and the remaining gap is closed by the verbs above in the same closing rounds.
  The nations that reach the fold are already on the total.
- **The majors, after the search.** The capital a major is chosen by is the industry the landscape
  search spends, after the history closes. So the majors are reconciled once the search's winner is
  applied — a minor raised or a major demoted — and each correction is recorded as a history event
  **dated back** into Industrialisation's closing rounds, so the record reads as one history.

**The three readings (Ben, 2026-10-11):**
- **The richest minor** is the one with the most industry points standing on its ground
  (`region::industry_points`, summed) — what a charter is built from.
- **The poorest surplus major** is demoted; "poorest" reads the same industry-point measure.
- **The minimum-territory floor runs first.** No nation survives below it; reconciliation then
  closes whatever gap the floor leaves, adding nations back by secession where the floor took the count
  under the total.
- **The state charter and the champion's fallbacks are retired (Ben, 2026-10-23).** A minor that
  could not afford its champion had its treasury pay the gap as a **state charter** (2026-10-11); a
  champion that found no window could take any land tile of its nation, or be founded by
  consolidating its nation's background firms (2026-10-12). Raising a minor buys nothing and places
  nothing, so none of them has a subject.
- **The target outranks the floor (Ben, 2026-10-11).** Where no secession can leave both sides
  above the minimum territory, the closing rounds still split a province, choosing the split that
  leaves the smallest shortfall, so the total is always hit. A nation below the floor is the cost of
  a target the ground could not otherwise hold; a target is never missed to keep the floor.
- **No target, no reconciliation.** A world built without targets — a harness, an old save, any
  build that skips the setup round — keeps its natural counts, uncapped. The wizard always sets
  both. `GENERATION_STRATEGY.md` § Asymmetry is the deliverable
holds for everything else. These two counts are its one sanctioned exception: the player sets them,
and the world's shape inside them stays the world's.

The library spread shows why both stages are needed. Unforced, the curated worlds of
`seed_library.json` end with 20 to 69 nations folded from 37 to 95 living polities, and 3 to 11
nations holding a planned specialist (measured 2026-10-11, when a major was a nation holding a
champion). Reconciliation therefore removes nations on about half the library and adds them on the
rest, and trims majors where more than six would form.

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
settled, industrialised, fought over, and read by industry generation.

Per region it records: the anchor tile, the **culture it inherits** (the culture of the stream
that reached it — [COLONISATION.md](COLONISATION.md) § Culture arrives by route — so the pantheon
distribution is a map of who walked where rather than a per-region re-roll), its
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

**Moving into the history (Ben, 2026-10-11; § The nation count is a target).** On a settled world the
floor becomes closing-round absorptions in Industrialisation, recorded as events, so the total can be
reconciled after it. What follows describes the rule the absorptions keep, and the post-pass that
still runs where no history ran.

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
into the nearest nation across the water, § Pass 2e). This pass is *not* itself a source of fragmentation — it only
ever merges a nation into a neighbour it already touches, so it cannot cut one in two.

**The floor runs after the fold, so it judges realms, not seeds** (§ Pass 2d — The polity fold).
Every seed anchored in ground one polity held at the close has already collapsed into that
polity's one candidate nation before the floor looks, so what it absorbs is a small realm, never
one region of a large one. A **city state** — a polity that reached the close holding one region
with a population centre on it — is exempt: the loop steps past it, it is never absorbed, and it
never absorbs, since a city state that annexed its neighbours would stop being one. This
absorption is the **only merge** in the pipeline, and § Pass 2d owns what an absorption does to
a nation's identity.

### Pass 2d — The polity fold

**One realm becomes one nation** (Ben, 2026-09-24). The carve answers a geometric question — where
the line between two anchors falls — and the fold answers the political one the history already
settled: whose flag flies over both. Every seed anchored in a region the same polity held at the
last span's close collapses into ONE nation, so an empire that held nine regions arrives as one
realm holding nine regions' ground, not nine neighbours that happen to touch. The fold runs before
the size floor (Pass 2c), which is why the floor judges realms.
`GENERATION_STRATEGY.md` § Pass 2 is the economy pass, and it is TWO phases owns the handoff
this pass finalises — the polity map crosses, and the carve is unchanged.

**A nation is its realm, and it keeps what the realm had.** The fold's **representative** for a
polity is its founding core — the lowest-indexed region it held, because region ids ascend in
founding order — and the nation the fold makes carries the realm's identity through that
representative: its **name**, coined once at the realm's founding and never re-coined (§ Pass 5);
its **colour slot**, the one the wizard's rounds painted it in (§ Pass 5); its **treasury** at
the close (§ Pass 7); and its capital. **The realm's seat at the close is the nation's
capital tile (Ben, 2026-10-17)**, save a realm under the size floor, whose capital follows the
larger piece it keeps (§ Pass 2e, Ben 2026-10-18). Nothing about a realm is recalculated at the handoff:
the nation the player meets at Begin is the realm they watched rise, under the same name and the
same colour. A realm's territory is **one territory** (§ Pass 2e): a polity that conquered across
a neighbour does not keep the far side, which the closing rounds cede to the nation around it.

**A city state crosses as itself.** A polity that reached the close holding one region with a
population centre on it is a nation on its own account. It is judged **as the realm stood at the
close, before any cession** (Ben, 2026-10-18): a realm the strait rule cut down to its seat is not
thereby a city state, and stays open to the floor and the total like any small nation — exempt from the size floor, and never an
absorber. A world with no city state is a legitimate outcome; the exemption is a permission, never
a quota.

**The size floor's absorption is the only merge, and the absorber keeps its identity** (merge
rule A, Ben, 2026-09-24). When Pass 2c folds a small realm into a neighbour, the nation that
results is the **absorber**: it keeps its own name and its own colour slot, its chest grows by the
absorbed realm's (§ Pass 7), and the seat card (`../ui/STARTUP.md` § The seat) lists under the
nation's name the realms it absorbed. The absorbed realm contributes ground, people and a chest —
never a name, whatever index its seed carried. No other pass merges nations. The one transfer of
ground between surviving nations is the cession of an exclave (§ Pass 2e), which moves ground and
never a nation.

**Ownerless ground crosses as a nation with no realm.** A Voronoi cell around an anchor no polity
held at the close folds to itself, so it becomes a nation of its own, subject to the size floor
like any other. It has no founding realm to inherit from, so it is the one case Pass 5 **coins** a
name for — in the tongue of the culture that settled the region its seed grew from — and the one
case the colour table assigns a fresh slot to; the seat card says the ground had no realm at the
close. A body with no settlement pass is all ownerless ground, and there the fold is the identity:
every seed is its own nation, exactly as the random-placement path carves.

### Pass 2e — One territory: the strait rule

**Every nation holds one territory (Ben, 2026-10-16).** A nation's ground is one land-contiguous
group, or several groups joined by **short straits**, never ground that jumps across another
nation. Borders that leap a neighbour read as a defect at the first look at the map, whatever
history produced them.

**What joins two groups.** Two of a nation's land groups (cardinal adjacency, column-wrapped) are
**joined** when a path between them crosses **at most `strait_tiles` ocean tiles and no land
another nation holds**. Joins chain: a nation's groups form one territory when every group reaches
the **core** — the group holding its capital, which is the realm's seat at the close (§ Pass 2d); a
nation with no realm is seated on its lowest seed inside its territory — through joins. **A realm
under the size floor keeps the larger piece (Ben, 2026-10-18):** where a realm's land at the close,
before any cession, is under the floor, its core is the larger of its seat's group and its largest
group — land tiles only, and only a group holding one of its provinces (Ben, 2026-10-18) — and its capital follows the core; a realm at or over the floor keeps its seat's group. The
core is fixed before the first cession: ground a nation receives never becomes its core. `strait_tiles` is **3** (≈ 380 km at
`body_km_per_tile`), a calibration inside "a few sea tiles" (Ben, 2026-10-16).

**No capital on a speck (Ben, 2026-10-25).** A capital moved to its nearest province must not land
on a speck. Where the core chosen above stands on a land group under **10 tiles**
(`capital_speck_tiles`) while a larger group of the nation's own holds one of its provinces, the
nation is seated on its **largest** such group instead — groups measured as the strait rule
measures them, land tiles only — at the province in it nearest the old seat, ties to the lower tile
index. A capital moved to its nearest province — the seat had no ground of its own — may take any
such group. Every other seat on a speck takes the largest such group **within its own territory**,
the groups joined to it by land or straits: the capital moves, the territory does not, so no
cession changes and a realm at or over the floor still keeps its seat's group. The rule covers
every nation, realm or not, is read with the rest of the core before the first cession, again for
an absorber after the size floor, and once more on the finished map — where a province the nation
received may now be its largest group — moving the capital tile alone. A larger group with no province on it — a seedless
landmass Pass 2b hung on the seat's own cell — gives the capital nowhere to stand, so the seat stays
put.

**The capital tile is its seat's city (Ben, 2026-10-26).** The rules above choose the seat
*province*; the capital *tile* is that province's largest population centre — the tile with the
most centre scale standing on it, ties to the lower tile id (the measure of `province_anchors`, the
pick the capital garrison's post and the province holder read) — **on the capital's own land
group**: a province can hold an islet across a strait, and the move must not seat a capital on a
speck the rule above kept it off. Where the province has no centre on that group, the group's
largest centre is the seat. It is set once the province partition and its anchor foundings exist,
before roads, industry or the Logistic Point hub read the capital (`seat_capitals_on_centres`).
Only the tile moves: the territory and every cession stand. Every land province holds a centre at
generation, so a land group without a city is a defect to count, not a case to design for; such a
seat keeps the tile chosen above. The capital's tile is a city tile, and a city tile takes the
Logistic Point hub every capital holds like any other building (its stack cap is twelve).

**An exclave is ceded, not kept.** A group that does not reach the core is an **exclave**, and it is
**split among the nations that border it** (Ben, 2026-10-18): each part goes to the bordering
nation nearest to it — province by province on a settled world, where a province never splits,
and tile by tile where no history ran — so no one neighbour takes a large exclave whole. An island
with no land neighbour goes whole to the nation whose ground is fewest ocean tiles away. Ties go
to the lower nation index, then the lower tile index. The cession moves ground only. The nation
keeps its core, its name and its chest, so a cession never changes the nation count, and the
receiver's own contiguity is re-checked after each one until no exclave is left.

**A cession takes the Pass 2b islets hung on the ceded province with it (Ben, 2026-10-25).** Pass 2b
hangs a seedless landmass on the carve cell of one province, so the islet is that province's ground
wherever it lies. When the province is ceded, every islet of its cell that the cedent still holds
outside the exclave goes with it, to the same receiver; the cedent's core is never carried. Where an
exclave is split tile by tile and the province's cell falls to several neighbours, the islet goes to
the receiver of the part of the cell it hangs from: the exclave tile of that cell nearest the islet
by Pass 2b's own grid distance, ties to the lower tile index. A carried islet is ground like any
other, so the receiver's contiguity is re-checked after it: an islet within strait reach of another
nation is ceded on, and one beyond every strait is the receiver's remote island. Leaving the islet
with the cedent would let a realm cut down to a few tiles of its seat keep a far landmass as its
whole economy.

**A cession is a history event.** On a settled world it is a **closing-round step of
Industrialisation** (§ The nation count is a target), recorded with its cause — the exclave was
cut off from its realm — and it runs **first**, before the size floor and before reconciliation.
The floor then judges territories as the player will see them. Reconciliation keeps the rule: an
absorption joins a nation to a neighbour it already touches, and a secession is chosen so both
sides remain one territory. Where an absorption reaches its neighbour only through owned water —
a lake or a coast that joins beyond a strait — the closing rounds run the rule again after it and
cede what is left over (Ben, 2026-10-17).

**Every launch obeys the rule (Ben, 2026-10-17).** A launch that closes before Industrialisation —
an Empires-round launch, where no closing round runs — holds one territory too: the same rule runs
as a post-pass after Pass 2d, and each cession is recorded as an event of the last span that ran.
Where no history ran at all, it runs as the same post-pass, and again after the size floor.

**The remote island.** A landmass farther than `strait_tiles` from every nation's ground cannot
be held contiguously by anyone. Pass 2b still gives it to the nearest nation, and it is the one
group exempt from the rule, **at any size** (Ben, 2026-10-17): a whole continent beyond every
strait is held by one nation as a remote island would be. The floor's island case follows the same reading: an island with no
land neighbour is absorbed by the nearest nation across the water, never the globally largest.

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
produced by the sim, whether by a stalled front or a rival cutting it off. An exclave on a **seedless** landmass is **Pass 2b** cleanup and is not
evidence for the claim.

**What the measurement says** (seeds 0–5): 196 exclaves, **60 emergent** and 136 from
Pass 2b — 31 % of exclaves by count, but **49 % of exclave tiles** (940 of 1930). Every swept seed
produced emergent exclaves (6–17 each), and roughly half of all nations hold at least one. The
promise pays: fragmentation is real and sim-produced, but Pass 2b's long tail of tiny orphan
islands dominates the raw *count*, so the honest headline is the tile share, not the component
share. Those figures describe the carve **before** § Pass 2e. The cession turns emergent
fragmentation into history — a cut-off province changing hands — rather than into a map defect.
So the audit asserts two things on the finished world: **no exclave outside the strait rule**, save
the remote island; and the cessions counted as recorded events, so the sim still visibly
produces the fragmentation the rule resolves. **The rule must not starve the minors (Ben,
2026-10-18).** The audit also reads the nations' sizes on every library seed at the default
targets, and fails a seed whose minors' median falls below **80% of that seed's own median
measured before the strait rule** (Ben, 2026-10-18), or whose majors hold more than 80% of the land. It is a reading of the
world, never a clamp on it. A seed Ben accepts below its bar is recorded in the audit at the
reading he accepted, so a further drop still fails.

### Pass 3 — Resource profile derivation

Each nation's resource profile is computed by summing the deposit profiles of its tiles,
weighted by composition type. The result is a read-only descriptor — `iron_ore_abundance`,
`agricultural_abundance`, `petroleum_abundance`, etc. — used to characterise the nation
economically for diplomacy initialisation and industry generation.

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
| `economic_focus` | `extraction`, `processing`, `trade` | Biases the nation's build scorer through its focus weight (Ben, 2026-10-26; `../ai/AI_OPPONENT.md` § 5, Scoring) |

Sentiment between any two nations at generation time is a function of ideology compatibility,
territory adjacency, and resource overlap. High-overlap neighbours start with lower sentiment
regardless of ideology.

### Pass 5 — Naming

**The register of naming sites.** A tongue is coined in [CREEDS.md](../lore/CREEDS.md); this
section owns which passes *consume* one. The register is here rather than beside the tongue
because consumption is a generation fact, and because a claim that every name is drawn from a
tongue is **not true of every site** — the exceptions are the reason a register exists at all.

| Site | Source | Draws on a tongue |
|---|---|---|
| Star and body names | `body_names.cpp` | Yes — its own `roll_tongue` sky tongue, distinct from any culture's |
| Realm names | the founding, `history_sim.cpp` (`polity::name`) | Yes — `coin_lexicon` over the founding region's culture `speech`, at the year the realm is founded; carried by id through every later span and onto the record beside the region names |
| Nation names | § Pass 5, `nation_generation.cpp` | **Inherited**, not coined — a nation takes its founding realm's name verbatim. `make_nation_name` coins over the culture's `speech` only for ownerless ground |
| Region names | `settlement.cpp` | Yes, **both halves** — the culture half and the quarter word |
| City names | `city_names.cpp` | Yes — `coin_lexicon` over the founding culture's tongue |
| Culture and god names | `creeds.cpp` | Yes — the tongue's own word builder |
| ~~Corporation names~~ | — | **Retired** with the corporation (Ben, 2026-10-23; [CORPORATION_GENERATION.md](CORPORATION_GENERATION.md) § Pass 5 — Naming). |

**No English survival is sanctioned in generation.** The corporation type word ("Holdings") was the
one, and it retired with the corporation (Ben, 2026-10-23). Nothing in the register may acquire an
exception by resembling it.

**A tongue with no phoneme inventory cannot coin**, so `quarter_word` keeps an English fallback
table for that case alone — a region with no name at all would be worse than one out of
register. That is a degenerate-input guard, not a second naming system.


**A realm's name is length-capped** (Ben, 2026-10-13), so a nation's full name fits the lists
that print it — the Market ledger's nation list is the binding one, at 3/5 of the fold-out
column. `coin_realm_name` tries its three forms and returns the first within the cap, falling
back to the bare name, which is always within it; the cap is a constant beside the coiner. The
coiner stays a pure function of tongue and salt, so the cap changes names and nothing else —
no draw moves, no digest moves.

**A nation inherits its founding realm's name, verbatim** (Ben, 2026-09-24, superseding the
lowest-indexed-seed rule). A realm is named **once, at its founding**, in the tongue of the culture
of the region it was founded on — `polity::name`, coined by `coin_lexicon` over that culture's
`speech` at the founding year, carried by id through every later span, and written onto the
record beside the region names so the board and the ticker print it
([CIVILISATION.md](CIVILISATION.md) owns the founding). The name never follows the capital: a
realm that moves its seat keeps the name it was founded under. The nation the fold makes of that
realm (§ Pass 2d) *is* the realm, so Pass 5 does not name it — it copies the name across. The
player meets at Begin the realm they watched rise in the wizard, under the same name.

**The name follows the fold representative through a merge.** A nation the size floor grew by
absorption (Pass 2c) carries its **absorber's** realm name — the representative of the polity
that absorbed — and the absorbed realm's seed contributes nothing to it, whatever its index. A
rule that named a merged nation after whichever surviving seed carried the lowest index could
name a realm after the small neighbour it had just absorbed; the fold representative is the one
rule Pass 2d, Pass 5 and Pass 7 all read, and it cannot.

**There is no name bank, and coining is for ownerless ground only** (BL-290, native nation
names). A nation with no founding realm — a Voronoi cell no polity held at the close (§ Pass 2d)
— is named here, in the **tongue of the culture that settled the region its seed grew from** —
the same phoneme inventory the creeds pass (BL-235) coined that culture's own name and its gods
from. Naming *consumes* the phonology the generation chain already produces; it does not roll a
second one.

The plumbing:

- `world/tongue.{hpp,cpp}` owns the `tongue` (onset / vowel / coda inventory), the word builder,
  and `coin_lexicon`. `creeds.cpp` rolls the tongue and **retains** it on `culture::speech`; the
  founding in the history sim coins a realm's name from that same retained tongue.
- `hard_coded_world.cpp` carries each region's tongue across into `nation_params::seed_tongues`,
  parallel to the `seed_tiles` anchors the settlement pass supplies, and each seed's polity into
  `nation_params::seed_polities` — the fold's input and the name's.
- Pass 5 gives each nation the name of its fold representative's polity. Only a nation whose
  representative carries no polity is coined: it takes that seed's speech and builds the name with
  one of three structural forms: bare name, epithet + name, name + realm word.

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

**Colour is inherited the same way: a nation's colour is its realm's slot** (Ben, 2026-09-24).
The wizard's rounds paint every realm in one slot of the lapse polity palette — a slot the round
assigns from the realm's founding culture's hue family, offset against its neighbours so kin
realms read as kin and no two neighbours share a hue — and **pin it on the record**, so a realm
keeps its colour from the round it rises in to the close
(`../ui/STARTUP.md` § Rounds — System, Life, Culture, Empires, Exploration, Industrialisation
owns the assignment and the pin). Pass 5 writes a per-world **nation → colour-slot table**: each
nation takes the slot its founding realm holds on the record — through a merge, the absorber's —
and a nation of ownerless ground takes a slot the same rule hands it against its neighbours. The
table is derived from the saved record, at Begin and again on load, so a loaded game colours every
nation as the wizard did; `ui::palette::nation_colour` reads it and never hashes an entity id.
Every surface that colours a nation reads that one table — the national border band on the plain
canvas (`../ui/LENSES.md` § The Country lens has retired — national borders are chrome; rendered
in `../ui/PLANETARY.md` § The national border band), the carve on the loading screen (§ The carve
is watched), the History ledger's Ages tab and its seat map (`../ui/ledgers/tile_ledger.md`), and
any Ages view that replays the wizard's rounds in play — so the colour a realm was watched in is
the colour its nation is met in. The palette the slots index **must hold under a colour
deficiency**, on the same reasoning that chose the twelve-slot nation table — safe hues widened by
lightness within a hue family rather than by new hues — because the band is the plain canvas's
only political read; that requirement is checked before the slots are pinned.

### Pass 6 — Substrate density

The last pass in `generate_nations` writes the geography of settlement density. For every
nation-owned tile it finds the nearest population centre on the same body (Chebyshev distance)
and computes a **density ripple**: `max(0, 1 − dist/8) × centre.scale`, taking the strongest
centre. That value is written to `tile_component.substrate_density`.

**Nothing reads the field.** The economy is carried by real buildings each nation owns
(`CORPORATION_GENERATION.md` § Pass 6), not by a per-tile capacity aggregate, and the Industry
lens (`body_surface_canvas.cpp`) reads that plant on each tile directly. The ripple is a
settlement-density description with no consumer. The pass requires population centres to already
exist — which is why `generate_population_centres` runs before `generate_nations` in
`hard_coded_world.cpp` (see § Settlement generation below).

### Pass 7 — Starting treasury *(Ben, 2026-08-24; the source ruled BL-975, 2026-09-15)*

**Every nation opens with real credits, and none opens in debt (Ben, 2026-10-11).** The player is a
nation and sees its starting credits, so a median nation opening on a fraction of a credit is a
defect, not a skew to report. The lever is **the history banks more**: polities earn and keep more
across the spans, so chests grow everywhere in-world rather than by a larger conversion or a reserve
granted at the handoff. **No major leaves generation on a zero treasury (Ben, 2026-10-19):** a major
owns every building on its ground, its hubs among them (Ben, 2026-10-23), and a nation that
cannot fund its hubs idles every one at the first tick and starves the industry that draws their
Logistic Points. Two rules hold it there. **A state founding leaves a year of the node bill in the
chest, counted from the game's opening** (Ben, 2026-10-19; read since 2026-10-23 as the network-node
bill the treasury pays first, `../politics/NATIONS.md` § What a nation owns): the twelve settle ticks
before the opening plus four, sixteen ticks of what the funding gate would charge its network
nodes: it pays only from what lies
above that, and a shortfall is the founding's, never a mint. **No realm closes the history empty**
(Ben, 2026-10-19): the poorest realms earn more across the spans — the same earning lever as below,
lifting the floor and keeping the order — until none folds an empty chest. Three rules carry it:
**a small realm's endowment has a floor** (a fraction of the average realm's world-share income);
the standing army carries only a share of the earn, and tribute is a share of the earn rather than
the chest (`EXPLORATION.md` § Force persists, and the tribute clause). The army share and the
endowment floor are **calibrated together by sweep** against two targets: no major opens short of a
year of its node bill, and the median nation still opens inside 1,000–2,000 credits.
**Calibrated (Ben, 2026-10-19):** the army carries **600 per mille** of the earn and the endowment
floor is **500 per mille** of the average realm's. The sweep met neither target fully — it cut the
majors short of a year from 20 of 78 to 7 on natural worlds — and Ben accepted it, tracing what
still drains the rest as its own work. The 1,000–2,000 band is judged on every pair: a world built to
few nations opens its median above it, and that is accepted rather than cut. **The floor may
reorder a few realms (Ben, 2026-10-19):** it lifts the endowment alone, so a small realm that also
trades can pass a larger one that does not; that small reordering is accepted, the one exception to
"keep the order". **No capital starts dark (Ben, 2026-10-22).** Every capital holds a
Logistic Point source, a minor's as a major's (`../economy/LOGISTICS.md` § Where generation puts
the sources), so every nation holding network nodes beyond its capital's free source carries a bill
for them from the first tick. The
endowment floor stays at 500 per mille: swept from 500 to the cap, it lifted few of the short realms
and pushed more seeds' medians out of the band, and no floor reaches a nation the history never held.
The top-up that made **the firms on a nation's ground fund its opening node bill** (2026-10-22) is
retired with the firms (Ben, 2026-10-23): it was a transfer from their balances, and every building's
money is now the treasury's. **A poor nation opens with fewer state buildings (Ben, 2026-10-29, overturning "keeps what it has").**
A nation still short of sixteen ticks of its running bill once the foundings' reserve is taken does
not open with a state it cannot run: generation sizes its opening state to what its chest can run
for sixteen ticks — the garrison wage and the state bill together — standing the capital's source
first and the rest in the order the funding gate pays them, and leaving unbuilt whatever the chest
cannot carry. Nothing is minted; the nation is smaller, not richer. **Military bases are cut like any
building (Ben, 2026-10-29)**, in the gate's order; a garrison whose base is cut keeps its men and
stands with no recorded base. The capital's source costs nothing to run (`../economy/LOGISTICS.md`
§ Every capital holds a source), so even a nation with an empty chest opens with its capital lit.
**The settle builds only from surplus (Ben, 2026-10-29):** a nation's scorer in the settle may build
only from income above its running bill, so it never spends the chest the cut sized. No nation, the player's or a
rival's, takes on debt before the game starts. How much
more, and through which earnings, is settled by measurement (BL-1298) against these rulings
(Ben, 2026-10-11):
- **The target:** a typical (median) nation opens on **1,000 to 2,000 credits** — for now, to be
  revisited once the campaign shows what a state spends.
- **Lift the floor, keep the order.** Poor nations gain the most; the realms that inherited empires'
  chests stay the richest. The spread narrows from the bottom, never by cutting the top.
- **The lever is earning (Ben, 2026-10-11, on the treasury census).** A realm **taxes its seats'
  stores every round** — a standing levy on what its own ground produces, swept into the chest as
  the 1200 consolidation once swept it, but every round rather than once — and **endowment income
  is rescaled** so a small realm earns too. Not a larger conversion rate, not a reserve granted at
  the handoff. **The endowment is summed over the ground a realm holds, not averaged (Ben,
  2026-10-12)**, so a larger realm earns more and the inherited order holds: a flat per-realm
  endowment reached the band but scrambled which realms were richest. **It is read as a share of the
  world's ground (Ben, 2026-10-12)**: a realm earns by the share of its world's held ground it holds,
  so a small world's nations are not poorer for the world being small and every world's median lands
  near the band. With real chests, the treasury's conversion into industry points (`industry_points_per_1000_treasury_units`, per thousand units since the rate fell below one point a unit) is re-sized toward
  the scale term — **at the lowest rate that keeps market_viability's G1 at tick 50 at or above its
  target** (Ben, 2026-10-12; the same-order rate cost it 2.8 points). ("Keep more" was dropped: the treasury-to-industry conversion drew 4 credits across
  the library, so it is not where the money goes.)
- **Measured before the ruling** (`tools/verify/treasury_census.cpp`, 16 library seeds): 99.6% of
  every chest is the one-time 1200 fold of the seats' stores; the ongoing earn after it is about
  2,200 credits pooled over 760 years against 307,000 folded; the standing-army bill then drains the
  chests, so the median nation opens on **0.02 credits** and 49 of 77 majors on under one.

The rate's own rationale below was argued against the old median (0.41 credits), the extraction
levy and the corporate economy as they stood; the levy and the corporations are retired (Ben,
2026-10-23), so the rationale is re-read once the measured chests land, rather than kept by inertia.

`nation_component::treasury` opens on **what the history banked, not what the campaign has yet
earned**. NATIONS.md's rule — a treasury nothing has credited is zero, and a treasury that started
full would be a balance change smuggled in as a field — still holds for the ONGOING campaign; it
never settled generation, and two consumers hit the gap at once (garrison sizing and contract-offer
funding both scale off treasury, so both flattened to their floor in a freshly generated world).
Ruled 2026-08-24 that generation credits a treasury of its own; ruled 2026-09-15 that the credit is
the **treasury folded across the last simulated span's handoff** — Exploration's at 1660, or the
Industrialisation span's at 1960 where that span runs — not a levy re-run at world creation. The history
already wrote the number, and a nation that inherited an empire's chest should open richer than one
that inherited a city state's.

**The rule.** `EXPLORATION.md` § Where the treasury sits puts one treasury per polity at its
capital seat, as a fact about the ground (`region::treasury`): the flag over that ground at the
close owns the chest. Pass 2d folds a polity's anchored regions into ONE nation, and the polity's
whole treasury at the close — summed over every region flying its flag, so a water-seated capital
still counts —
lands on that nation, once. A seed the size floor (Pass 2c) absorbs hands its chest to the realm
that absorbed it, the same way it hands over its ground. A nation with **no folded polity** — a
Voronoi cell the history never held, or any body without a settlement pass — opens on the
**floor: 0 credits**, NATIONS.md's own zero for a treasury nothing has credited.

**The one stated conversion.** The sim's treasury is in its material currency; the campaign's is
credits. The conversion is a per-mille, and it is named here and nowhere else:

    treasury_credits = Σ region::treasury (at the last span's close, over the folded polities) × 0.01 ‰

that is, **0.01 per mille — one credit per 100,000 units of material**
(`nation_params::treasury_credit_per_mille`). Chosen against two scales the campaign already has,
and **kept when the credit moved to the 1960 close (Ben, 2026-09-22, NR-912): the rate stands, and
the skew below is the history's, not the rate's.** Measured on the reference seed's shipped world:

- **The first quarter's budget, as argued.** Every nation's extraction levy (retired, Ben, 2026-10-23) was its
  whole quarterly income at the open — ~7.5 credits a nation in quarter one when the rate was chosen. The
  rate was first argued on the median POLITY chest (~10 million material, ~100 credits: thirteen
  quarters of levy). But a nation opens on its OWN folded chest, and most realms fold small ones:
  on the reference seed the **median nation opened on 0.41 credits** when the rate was chosen (3.64,
summing 13,683, on the 2026-10-11 census), 22 of the 38 on under one,
  and 14 on 100 or more — thirteen quarters of levy and upward. So the reserve is held by the
  realms that inherited empires' chests, and most realms open on their levy alone. Ten times the
  rate (0.1 ‰) would lift that median to ~4 credits and multiply every hoard with it.
- **The corporate economy it faced, as argued.** At 0.01 ‰ the reference seed's 38 treasuries sum to
  **31,691 credits** and the richest realm opens on **5,420** — a state that can fund a line or two
  of its budget (NATIONS.md § 1) without becoming the economy's largest buyer on tick one. At
  0.1 ‰ the treasuries alone would sum to ~317,000, an order of magnitude past a corporate field's
  opening cash.

Garrison sizing (`seed_nation_garrisons`, MILITARY.md § Nation garrisons) reads the result and
differentiates on it: the reference seed's poorest realms — the median among them — hold the
20-head floor, its richest the 200-head ceiling. The spread is log-heavy because the chests at the
close are — a consequence of the spans' own endowment and trade income, not a distribution
authored here.

**What this pass is not.** It is not a mint inside the campaign: it runs once, at world creation,
as the fold of a stock the history already held. It is the only opening money any actor holds:
the buildings generation places draw on it (`CORPORATION_GENERATION.md` § Pass 4). It says nothing about what the treasury is *spent on* —
NATIONS.md § 1 owns the budget, and Industrialisation owns whatever the 1660 → 1960 span adds to it.

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

**Market carving** — after nations and their industry exist, `hard_coded_world.cpp` seeds the
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

The homeworld's nation count is **a player-set target**, hit by a lean and then reconciliation
(§ The nation count is a target). The size floor and seed density stay fragmentation-modulated
under the lean. The 2026-07-30 ruling's mechanism survives: let naturally different cultures emerge,
and let a war/conflict stage narrow the count. Its premise, that the generator is never tuned to a
target, is overturned (Ben, 2026-10-10). Sizes stay strongly varied. The
base knobs are `land_tiles_per_seed`, `min_seed_separation`, and `min_nation_tiles`
(`nation_params`), modulated by `nation_params_from_ladder`; the targets of § The nation count is a target condition them.

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

The carve is drawn in `ui::palette::nation_colour` - the same per-world slot table the
**national border band** draws in (§ Pass 5 — Naming;
`../ui/LENSES.md` § The Country lens has retired — national borders are chrome) - so the map a
player watches being carved is the map they meet again on the plain canvas. The nation entities
do not exist while the BFS runs, so the map is keyed by index until `nation_id_base` is published
(ids come out of one tight `create_entity` loop, so index *i* is `base + i`); the borders have
already settled by then, and the recolour reads as them becoming real nations.

---

## Relationship to corporations

**Retired** (Ben, 2026-10-23). There are no corporations: a nation owns every building on its
ground and builds only there (`docs/politics/NATIONS.md` § What a nation owns, § Own ground).

---

## Open items

**Nation system design.** What nations actually *do* — build and run their industry, tax wages, declare
war, provide infrastructure — is owned by [`docs/politics/NATIONS.md`](../politics/NATIONS.md),
including the calls it leaves open. The generation layer exists so the world has political
structure from turn one independent of how much of that system is designed.

**Era-based reform.** The hypothesis that later Eras see corporations become powers above states
retired with the corporation (Ben, 2026-10-23).

**Fragmentation and history.** Pass 2c is a *light* "in history" pass (over-seed +
merge-below-the-size-floor, giving varied sizes and irregular borders), and the settlement sim
produces emergent exclaves on top of it. Disputed zones and contested tiles — fragmentation as a
live political fact rather than a border shape — have no owner in the generation layer; BL-518
(war redraws borders) is the first thing that moves a border after generation.

**Off-homeworld politics.** As colonies establish on other bodies, some form of jurisdiction
and governance will be needed there. Whether this extends the nation model is unresolved; a corporate
governance model is not a candidate (Ben, 2026-10-23).
