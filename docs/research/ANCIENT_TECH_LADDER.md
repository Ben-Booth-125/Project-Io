# Ancient Tech Ladder — Mockup (0 CE → campaign epoch)

> **Status: research scaffolding — mockup basis, not authority.** Captured 2026-08-04 on branch
> `claude/ancient-tech-tree-mockup-m3fgk3`, from Ben's ask: *mock up an ancient tech tree as a
> basis; consider the major advancements from 0 CE, and what differences are realistic by 1960.*
> Tracked as **BL-296 (ancient tech ladder)**; consumed by **BL-271 (Era −1 sim)** and
> **BL-274 (era-keyed rosters)**. Authority propagates into `docs/lore/HISTORY.md` (and the
> sim's own doc) only when the work lands. Read as a basis for the design conversation, not a spec.
>
> **Amended 2026-08-04 (same day, follow-up session):** § Geometry settles the constellation
> shape (Ben's call, overturning BL-087's Q1 binary-tree resolution), and § Density test carries
> the three-grain examples the detail-level call will be made against.

## What this is — and the one stance it must not violate

Ben's recorded position (BL-274, era-keyed rosters): *a player-facing tech tree only works in a
1900s+ start*. The ancient side is **derived, not chosen** — availability is a function of
endowment, institutions and contact, exactly as the settlement pass (BL-218) already derives
industrialisation timing. Nothing here contradicts that.

So this mockup is a **tech tree in structure, a ladder in play**: the tree (nodes + prereqs) is
the data the Era −1 sim (BL-271) evaluates each year-tick per nation; no one ever clicks a node.
"Ancient tech tree" and "era table" are the same object viewed from two sides — the tree is the
*full* dependency data; BL-274's roster table reads its military column; the sim's pacing reads
its gates. It replaces nothing that exists: `scripts/tech_tree.lua` (BL-087) covers the campaign's
1960+ arc; this ladder covers the 0 CE → 1960 run-up that arc stands on, and its last band hands
off to that file's Era 0 quests.

**Naming rule applies in full** (io-standing-rules § Terms & docs): Earth appears below as
*calibration reference* — dates, pacing, spread sizes. Node names are generic mechanism names;
no Earth proper noun is content. The sim renders these through Kepler's own generated cultures.

---

## Shape

**Six bands** spanning 0 CE → 1960, each ~2–5 centuries of Earth-equivalent time. BL-274 leaned
to four bands for the *roster*; the ladder needs six because the economic divergence that answers
"what differs by 1960" happens inside the two bands BL-274's lean merges (early-modern and
industrial split, T4/T5). Reconciliation is cheap: roster era-bands are a coarser grouping of the
same spine (T1–T2 → classical, T2–T3 → medieval, T4 → gunpowder, T5–T6 → industrial).

**Seven domains** (columns), so a nation's tech state is legible as a profile, not a scalar:

| Domain | What it carries | Reads / feeds |
|---|---|---|
| **Agriculture** | food surplus per worker | province demography (BL-273 — landed), manpower budget |
| **Materials** | metallurgy, chemistry | building/recipe availability, roster material gates |
| **Energy** | prime movers: muscle → water/wind → coal → oil | the Stage 4 furnace date (already computed) |
| **Transport** | reach and cost of movement | contact graph, trade routes, supply radii |
| **Institutions** | writing, law, credit, printing, administration | HISTORY.md Stages 1–3 made mechanical; diffusion rate |
| **Military** | weapons + doctrine regimes | **delegated to BL-274's roster table** — only the band boundaries live here |
| **Medicine** | disease, demography | BL-273 growth/drawdown rates |

**Node schema** (mirrors `scripts/tech_tree.lua`'s, plus the acquisition axes that make it
derived rather than researched):

```
node: id            "T3-EN-01" (band - domain - ordinal)
      name          generic mechanism name, never a proper noun
      band, domain
      prereqs       node ids; mostly in-domain chains, cross-domain links sparing (BL-087 shape rule)
      gate          endowment predicate over settlement_state, or "-" (none)
                    vocabulary: ore_q | fuel | arable | coastal | grassland (the BL-274
                    named-substitution set; grassland is the horse proxy)
      diffusion     artifact | practice | capacity   (see below — the load-bearing axis)
      earth_ref     calibration date/context, reader-facing only
```

### The diffusion axis — why nations end up unequal

Three classes, because things spread at three different speeds. This single axis generates
almost all of the realistic 1960 spread:

- **artifact** — the *thing* trades without its production base. Guns, radios, rifles, cloth.
  Diffuses along the contact graph in **years**; a nation can hold artifacts three bands above
  its capacity. This is how a T3-capacity nation fields T6 rifles in 1960 — which Earth's 1960
  demonstrates everywhere.
- **practice** — a technique carried by people: crop rotation, the stirrup, positional
  arithmetic, double-entry books. Diffuses by contact in **decades**; needs no endowment, only
  exposure and an institution to keep it (a nation below the band's Institutions floor absorbs
  practices slower).
- **capacity** — a production base: smelting, shipyards, coke furnaces, machine tools. Needs the
  **endowment gate satisfied locally** plus the prereq capacities; diffuses in **generations or
  never**. Capacity is what the record-burning rule can destroy and what conquest cannot simply
  carry home.

Rule of thumb the sim should reproduce: **artifacts leapfrog, practices follow contact, capacity
follows the map.**

---

## Geometry — the constellation (settled 2026-08-04, Ben)

The reference is the Path of Exile passive tree: one shared radial web, identity from where you
enter and what you can afford to reach. **This overturns BL-087's Q1 resolution** (2026-07-08,
binary tree / no re-converging mesh) — overturn recorded at `ERA1_TECH_LANDSCAPE.md` § Q1. The
motive behind Q1 survives: the "inaccessible mess" failure mode is now excluded by node-count
discipline and the fog, not by forbidding re-convergence.

- **One shared web.** Rings = bands (T1 centre → T6 rim), sectors = domains. Time is the radial
  axis; specialisation is the angular one. The § Shape "spine" survives as the ring boundaries;
  its vertices are the ring-crossing gates.
- **Entry points = endowment.** Nations enter at different sectors of the inner ring — the forge
  culture via Materials, the river culture via Agriculture — and gates price travel differently
  per nation. One web of knowledge, unequal traversal: the 1960 spread *is* pathing distance.
- **Travel is OR, meaning is AND.** Web edges are adjacency; owning any adjacent node opens a
  travel node (OR). Vertices and keystones keep BL-156's flat AND condition-set (research /
  structure / stockpile / quest-capstone / endowment gate). PoE's own grammar, kept.
- **Keystone forks — binary choices with exclusion.** Taking one branch **closes the other's
  availability window**; nothing ever un-completes, so the monotonic unlocked set survives as
  windows (BL-087's v0.3.0 concept, pulled earlier by this geometry).
- **The tech fog.** Nodes render only within reach of an owned node — the third fog, after the
  geographic and activity fogs (`docs/ui/DISCOVERY.md`). Exclusion and fog compose: a closed
  branch goes dark permanently. This is the record-burning rule applied to knowledge — by 1960 a
  nation cannot even see the tree it didn't climb, and the whole web is deliberately never
  visible at once.
- **Node budget: much smaller than the reference.** PoE's ~1,325 nodes exist because travel
  filler is its pricing mechanism; Io prices in time and diffusion, so the filler has no job.
  Target 100–200 meaning-dense nodes for the full web (see § Density test).
- **Vertices are quests** (added 2026-08-04, Ben: quests for key future technology sit at clear
  vertices). The gate = quest = tech unification (BL-087) maps onto the geometry directly: a
  ring-crossing vertex **is** a quest object — named for the capability regime beyond it, its
  capstone carrying the economic conditions (BL-087's "economic conditions reserved for
  capstones" rule, surviving the shape overturn), its completion opening the next ring region
  in that sector. Travel nodes and keystones between vertices stay plain techs.
- **Derived stays derived.** On the ancient layer nations path by seeded, scored choice at forks
  (the corp-AI precedent — deterministic, replayable). The campaign arc can use the same
  geometry with real player clicks; the two trees share one shape language.

## Density test — one slice at three grains

Ben's call (2026-08-04): the tree should be much smaller than the reference, and the right
detail level is a **fun** question to be tested against worked examples. Here is one slice —
the steam transition (T4 Energy → T5) — written at three grains, so the call can be made
against real technology rather than in the abstract.

**Coarse — ~1 node per band per domain (whole web ≈ 40–50 nodes).**

> Deep Mining → **Steam Power** → Cheap Steel → Railways

Four nodes; grand-strategy grain. Legible at map scale, but it wastes the geometry: there is no
interleaving to see, and a fork has nothing to bite on — both sides of any real historical
choice collapse into one label. Exclusion at this grain would feel arbitrary.

**Medium — 3–5 nodes per band per domain (whole web ≈ 100–150; the § Ladder's current grain).**

> Deep Mining & Drainage → **Atmospheric & Rotative Steam** → Coke Smelting → Machine Tools →
> Converter Steel → Railway / Steamship
> plus one keystone: **Fuel Doctrine** — *Coke* (scale; wants coalfields) ⊘ *Charcoal* (quality;
> wants forest) — a real historical fork: charcoal-iron nations held the quality niche while
> coke nations took the scale.

About eight nodes for the slice. The interleaving is visible (mining begs for the engine, the
engine feeds the rail), every node still carries a one-line meaning, and the one fork is
explainable from the map — endowment picks it for an AI nation, the player picks it knowingly.

**Fine — reference grain, with travel nodes (whole web ≈ 400+).**

> Drainage Pumps → Atmospheric Engine → Separate Condenser → Rotative Motion →
> keystone: *High-Pressure Steam* (mobile: rail, ships) ⊘ *Stationary Beam* (mill power) →
> Compound Engines · coke chain: Coking Ovens → Hot Blast · tools chain: Boring Machine →
> Screw Lathe → Interchangeable Parts

Twenty-plus nodes for the same slice. Pathing itself becomes the gameplay — the reference's own
pleasure — but on the *derived* ancient layer nobody paths by hand, so the detail is invisible
outside a codex. Detail only pays where someone **chooses** or **reads**.

**The principle the examples suggest:** density should follow the consumer, per region of the
web. Sim-only regions (the ancient layer) want coarse-to-medium; player-clicked regions (the
campaign arc) want medium, with fine grain reserved for the *neighbourhood of each keystone*,
where the choice deserves texture. The fun question is then not "how detailed is the tree" but
"how detailed is the neighbourhood of each choice." Recommendation: **medium as the web-wide
grain** — logged as NR-056 (density grain) for Ben's call.

### The comparison slice — Institutions at medium grain (2026-08-04)

The steam slice tests the geometry where **capacity gates** carry the drama. Institutions is the
deliberate contrast: nearly every node is **practice-class** (diffuses by contact, no endowment
gate), and the forks are ideological rather than material. Same grain, same shape budget —
eight techs, one keystone — plus the vertex quests, invented per the new rule above.

The slice runs the Institutions sector from ring T3 to the T6 boundary:

**Techs** (travel nodes and junctions):

> T3-IN-01 Credit Instruments & Double Entry → T3-IN-02 Chartered Companies →
> T4-IN-01 Empirical Method *(cross-sector link: wants T4-MA-01 Movable-Type Press from
> Materials — the interleaving, visible)* → T4-IN-02 Joint-Stock & Public Credit →
> T5-IN-02 Mass Schooling & Press → T6-IN-01 Broadcast *(cross-link back: wants T5-IN-01
> Telegraph, itself capacity-class — the one hard gate in the slice)*

**Keystone — Sovereign Doctrine** (between T4-IN-02 and the T4/T5 vertex):

> *Chartered Capital* — courts bind the sovereign; cheaper state credit, corporate autonomy,
> seizure carries a lasting credit cost
> ⊘ *Command Estate* — crown monopolies and confiscation rights; immediate revenue, chronic
> credit penalty, corps operate at the sovereign's pleasure
>
> This is HISTORY.md's Stage 3 (capital disciplines the sovereign) turned from narration into a
> choice. For AI nations the creed picks it — the sealed-oath god's provinces take Chartered
> Capital, the same fact read twice, one layer apart (the BL-218 pattern). The untaken doctrine
> goes dark; by 1960 a Command Estate nation cannot see the institutional road not taken —
> which is *why* its 1960 economy tolerates corporations differently.

**Vertex quests** (the invented gates for the key technology beyond each ring crossing —
schema per `scripts/tech_tree.lua`: thesis, capstone conditions, opens):

| vertex | quest | thesis | capstone conditions | opens |
|---|---|---|---|---|
| T3/T4 | **The Enforceable Promise** | contract law reaches capital: an entity that outlives its members can own, sue, be sued | research T3-IN-02 + market: credit-instrument volume ≥ X sustained | T4 Institutions region (press, method, joint-stock) |
| T4/T5 | **The Disciplined Sovereign** | the state that serves markets outlives the state that raids them | research T4-IN-02 + surplus: debt service sustained N years, no default | T5 Institutions region (mass schooling, mass politics) |
| T5/T6 | **The Lettered Public** | a literate population is the substrate every machine-age institution runs on | research T5-IN-02 + structure: schooling reach ≥ threshold across provinces | T6 Institutions region (broadcast, early computing's institutional half) |

**What the comparison shows.** Medium grain holds up in both sectors, but they draw their
texture from different mechanisms: in Materials/Energy the *gates* differentiate nations
(endowment decides who passes), while in Institutions the *keystones* do (doctrine decides, and
locks in). Practice-class diffusion means Institutions inequality shows up as adoption **lag**,
not absence — by 1960 everyone has schooling on paper — so without its keystone the sector
would flatten into sameness. One fork per slice is therefore not decoration; it is where this
sector's differentiation lives. The vertex quests also read naturally at this grain: each names
a capability regime a player (or the sim's codex) can understand without opening a single node.

> **Grain settled: MEDIUM (Ben, 2026-08-04, against both slices — NR-056 resolved).** The
> mixed-by-consumer upgrade (fine grain in keystone neighbourhoods) stays available if
> playtests want more texture at forks.

---

## The ring-1-to-2 neighbourhood (medium grain, worked in full)

The first complete region of the web at the settled grain: every sector, ring T1 through the
T1/T2 crossings into ring T2. The centre is the **Stage 0 hand-off** — the cradle's agrarian
surplus, which every sector's first node hangs off. Nations enter on ring T1 at their
endowment-flavoured sector.

### Nodes by sector

All techs below are the § Ladder's T1/T2 rows (plus T2-ME-01, added with this pass); the
neighbourhood adds the connective tissue — vertex quests, one keystone, and the cross-links.

| sector | ring T1 | ring T2 | cross-links in |
|---|---|---|---|
| **Materials** | Bloomery Iron · Structural Masonry & Cement | Pattern-Forged Steel · Paper | — |
| **Energy** | Water Wheel | Windmill | — |
| **Transport** | Engineered Road Network · Deep-Hull Sail | Lateen & Long-Range Rig | Masonry → Roads |
| **Institutions** | Codified Law & Census · Coinage at Scale | Positional Arithmetic · Endowed Scholarship | Paper → Arithmetic |
| **Military** | *regime: massed iron infantry* | *regime: stirrup cavalry, fortress* | roster rows, BL-274 |
| **Medicine** | Physicians' Canon | Founded Infirmaries | — |
| **Agriculture** | Iron-Shod Plough · Irrigation Works | Mouldboard Plough & Harness · Three-Field Rotation | Masonry → Irrigation |

### Vertex quests at the T1/T2 crossings

Five earned vertices; two sectors deliberately have none (the sparse-sector rule, below).

| sector | quest | thesis | capstone conditions | opens |
|---|---|---|---|---|
| Materials | **The Common Forge** | iron stops being a treasure and becomes a tool | research Bloomery Iron + market: tool volume ≥ X | T2 Materials |
| Energy | **The Tireless Wheel** | work detaches from muscle for the first time | structure: N water wheels operating | T2 Energy |
| Transport | **All Roads Home** | goods move because the way is maintained | research Roads + Sail + market: trade-route volume ≥ X *(cross-link: wants Coinage)* | T2 Transport |
| Institutions | **The Written Ledger** | the sovereign counts, therefore the sovereign can promise | research Census + Coinage + Granary Doctrine taken | T2 Institutions |
| Agriculture | **The Fed Village** | surplus is the platform every other sector stands on | surplus: sustained food surplus ≥ X | T2 Agriculture |

The **Military** crossing is not a quest: it is the BL-274 roster turnover (massed infantry →
stirrup cavalry), fired by grassland endowment plus the band boundary — availability derived,
per that item. **Medicine** earns no vertex at this ring: its edge crosses the boundary as a
plain prereq (Physicians' Canon → Founded Infirmaries).

**The sparse-sector rule** (adopted with this pass): a sector earns a vertex quest only where
the ring crossing is a genuine capability regime — "clear vertices", per Ben. Thin sectors
cross on plain edges rather than inventing ceremony. This is the medium-grain discipline
applied to quests, and it keeps quest count proportional to how much a sector matters per era.

### Keystone — Granary Doctrine (Institutions, ring T1)

> *Temple Stores* — the surplus is gathered and redistributed by command; famine-resistant,
> stable, and markets stay thin
> ⊘ *Open Granaries* — the surplus is priced and traded; markets deepen early, famine risk
> stays live
>
> The campaign's own premise — **markets, not command** (HISTORY.md claim 1) — becomes
> something nations *chose* at ring 1, not a fact of the setting. Creed-picked for AI nations;
> a world where most cradles took Open Granaries is the saturated market world the 1960
> campaign asserts, and BL-275's (history sweep) distributions can assert how often that world
> actually emerges. Note the interlock: The Written Ledger requires the doctrine to be
> *taken* — either branch satisfies it, so the fork is unavoidable, but never dictated.

### The count, extrapolated

This neighbourhood: **20 techs + 5 vertex quests + 1 keystone + 2 roster regimes ≈ 28 objects**
across two rings. Five ring-crossings × seven sectors at this density extrapolates to roughly
**130–150 objects web-wide** — inside the § Geometry budget, with room for the campaign arc's
denser keystone neighbourhoods.

---

## The ladder

Nodes are the 3–5 load-bearing advancements per band per domain — the ones that flipped a regime
(the streamlining principle from `ERA1_TECH_LANDSCAPE.md`), not the encyclopedia. Military rows
name only the regime; unit detail is BL-274's.

### Band T1 — Classical (Earth ≈ 0–500)

The state a Stage 0–2 world enters the ladder with. Iron is known everywhere connected; the
first non-muscle prime mover appears.

| id | node | prereqs | gate | diffusion | earth_ref |
|---|---|---|---|---|---|
| T1-AG-01 | Iron-Shod Plough | — | — | practice | widespread by 0 CE |
| T1-AG-02 | Irrigation Works | — | arable | capacity | canal/qanat systems |
| T1-MA-01 | Bloomery Iron | — | ore_q | capacity | mature by 0 CE |
| T1-MA-02 | Structural Masonry & Cement | — | — | practice | 1st c. concrete |
| T1-EN-01 | Water Wheel | — | arable | capacity | 1st c. — first non-muscle prime mover |
| T1-TR-01 | Engineered Road Network | T1-MA-02 | — | capacity | imperial road nets |
| T1-TR-02 | Deep-Hull Sail | — | coastal | capacity | Mediterranean/monsoon trade |
| T1-IN-01 | Codified Law & Census | — | — | practice | the Stage 1 floor |
| T1-IN-02 | Coinage at Scale | T1-IN-01 | ore_q | practice | monetised economies |
| T1-MIL | *Regime: massed iron infantry, siegecraft* | T1-MA-01 | ore_q | — | BL-274 rows |
| T1-ME-01 | Physicians' Canon | T1-IN-01 | — | practice | Galenic-style corpus |

### Band T2 — Post-Classical (Earth ≈ 500–1000)

The quiet band: agricultural and cavalry revolutions, paper. On Earth its centre of gravity
moved — the calibration lesson is that **band leadership is not sticky** (see § Pacing).

| id | node | prereqs | gate | diffusion | earth_ref |
|---|---|---|---|---|---|
| T2-AG-01 | Mouldboard Plough & Harness | T1-AG-01 | arable | practice | heavy-soil opening, 6th–9th c. |
| T2-AG-02 | Three-Field Rotation | T2-AG-01 | — | practice | ~50% arable output gain |
| T2-MA-01 | Pattern-Forged Steel | T1-MA-01 | ore_q + fuel | capacity | crucible/pattern steels |
| T2-MA-02 | Paper | — | — | capacity | 105 CE, west by 8th–13th c. — the diffusion showpiece |
| T2-EN-01 | Windmill | T1-EN-01 | — | capacity | 9th–12th c. |
| T2-TR-01 | Lateen & Long-Range Rig | T1-TR-02 | coastal | capacity | ocean-capable coasting |
| T2-IN-01 | Positional Arithmetic | T2-MA-02 | — | practice | the zero; calculation cheapens |
| T2-IN-02 | Endowed Scholarship | T1-IN-01 | — | practice | institutional memory survives dynasties |
| T2-MIL | *Regime: stirrup heavy cavalry, fortress* | — | grassland | — | BL-274 rows |
| T2-ME-01 | Founded Infirmaries | T1-ME-01 | — | practice | endowed hospitals; care institutionalises |

### Band T3 — High Medieval (Earth ≈ 1000–1450)

Energy and credit: mill networks industrialise water power; the enforceable promise (Stage 1)
reaches capital. Ends with the first gunpowder — the T3/T4 boundary is the roster's
medieval → gunpowder turnover.

| id | node | prereqs | gate | diffusion | earth_ref |
|---|---|---|---|---|---|
| T3-AG-01 | Land Reclamation | T2-AG-02 | arable | capacity | polders, terracing, drainage |
| T3-MA-01 | Blast Furnace (cast iron) | T2-MA-01 | ore_q + fuel | capacity | 12th–14th c. Europe; earlier east |
| T3-MA-02 | Mechanical Clockwork | — | ore_q | capacity | precision mechanism tradition |
| T3-EN-01 | Mill Networks | T1-EN-01, T2-EN-01 | arable | capacity | cam/crank trip-hammers, fulling |
| T3-TR-01 | Compass & Sternpost Rudder | T2-TR-01 | coastal | practice | dead-reckoning seafaring |
| T3-TR-02 | Ocean-Rated Hulls | T3-TR-01 | coastal | capacity | cog → carrack line |
| T3-IN-01 | Credit Instruments & Double Entry | T2-IN-01 | — | practice | bills of exchange; Stage 3's seed |
| T3-IN-02 | Chartered Companies | T3-IN-01, T1-IN-01 | — | practice | the Charter Age node — Stage 1's apex |
| T3-MIL | *Regime: crossbow/plate; first gunpowder* | T3-MA-01 | fuel | — | BL-274 rows; powder needs the energy proxy |
| T3-ME-01 | Quarantine Doctrine | T1-ME-01 | — | practice | plague response; drawdown mitigation |

### Band T4 — Gunpowder / Early Modern (Earth ≈ 1450–1750)

Printing, ocean range, and guns restructure everything. The **institutional divergence band**:
on Earth, who adopted printing + joint-stock + standing armies predicted the next band's winners
better than any resource did.

| id | node | prereqs | gate | diffusion | earth_ref |
|---|---|---|---|---|---|
| T4-AG-01 | Crop-Package Exchange | T3-TR-02 | — | practice | inter-continental crop transfer |
| T4-AG-02 | Convertible Husbandry | T2-AG-02 | arable | practice | fodder rotation; feeds T5 workforce release |
| T4-MA-01 | Movable-Type Press | T2-MA-02, T3-MA-02 | — | capacity | 1450s; information cost collapses |
| T4-MA-02 | Cast Ordnance | T3-MA-01 | ore_q + fuel | capacity | foundry cannon |
| T4-MA-03 | Precision Instruments | T3-MA-02 | — | capacity | lenses, gauges, screw threads — T5's toolroom |
| T4-EN-01 | Deep Mining & Drainage | T3-EN-01 | fuel | capacity | the pump problem that *asks for* steam |
| T4-TR-01 | Full-Rigged Ship | T3-TR-02 | coastal | capacity | global range |
| T4-TR-02 | Celestial Navigation | T3-TR-01, T4-MA-03 | coastal | practice | longitude closes the ocean |
| T4-IN-01 | Empirical Method | T4-MA-01 | — | practice | knowledge compounds openly |
| T4-IN-02 | Joint-Stock & Public Credit | T3-IN-02 | — | practice | Stage 3 lands: capital disciplines the sovereign |
| T4-MIL | *Regime: flintlock line, artillery fortress, broadside fleet* | T4-MA-02 | fuel | — | BL-274 rows |

### Band T5 — Industrial (Earth ≈ 1750–1900)

The Stage 4 gate. Fossil fuel breaks the organic ceiling and **divergence explodes** — before
this band, connected nations sit within ~1 band of each other; after it, compounding growth
opens gaps of 3+ bands in a century (the calibration fact the sim must reproduce; § Pacing).
The furnace date `run_settlement` already computes **is** this band's entry date.

| id | node | prereqs | gate | diffusion | earth_ref |
|---|---|---|---|---|---|
| T5-EN-01 | Atmospheric & Rotative Steam | T4-EN-01, T4-MA-03 | fuel | capacity | 1712 → 1780s; the master node |
| T5-MA-01 | Coke Smelting | T3-MA-01 | ore_q + fuel | capacity | breaks the charcoal ceiling |
| T5-MA-02 | Machine Tools | T4-MA-03, T5-EN-01 | ore_q | capacity | interchangeable parts; capacity that builds capacity |
| T5-MA-03 | Converter Steel | T5-MA-01 | ore_q + fuel | capacity | 1856; cheap steel |
| T5-AG-01 | Farm Mechanisation | T5-MA-02 | arable | capacity | reaper line; releases workforce to industry |
| T5-TR-01 | Railway | T5-EN-01, T5-MA-01 | fuel | capacity | interior distance collapses |
| T5-TR-02 | Steamship | T5-EN-01 | coastal | capacity | schedule replaces season |
| T5-IN-01 | Telegraph | T5-MA-02 | — | capacity | information detaches from transport |
| T5-IN-02 | Mass Schooling & Press | T4-MA-01 | — | practice | literate workforce — T6's floor |
| T5-MIL | *Regime: rifle, ironclad, general staff* | T5-MA-03 | ore_q + fuel | — | BL-274 rows |
| T5-ME-01 | Germ Theory & Sanitation | T4-IN-01 | — | practice | mortality falls ahead of wealth |

### Band T6 — Machine Age (Earth ≈ 1900–1960, the campaign epoch)

The band the campaign opens in. Its exit — `E0-RK-*` (Rocketry) in `scripts/tech_tree.lua` — is
where this ladder hands off to the existing 1960+ tree.

| id | node | prereqs | gate | diffusion | earth_ref |
|---|---|---|---|---|---|
| T6-EN-01 | Electrification | T5-EN-01 | fuel | capacity | grid power; the second prime-mover shift |
| T6-EN-02 | Internal Combustion & Oil | T5-MA-02 | fuel | capacity | oil displaces coal at the margin |
| T6-MA-01 | Synthetic Chemistry | T5-MA-03 | fuel | capacity | dyes → polymers → explosives |
| T6-MA-02 | Fixed-Nitrogen Synthesis | T6-MA-01 | fuel | capacity | 1913; food ceiling lifts — feeds BL-273 growth |
| T6-AG-01 | Tractor & Fertiliser Package | T6-EN-02, T6-MA-02 | arable | capacity | single-digit % farm workforce |
| T6-TR-01 | Automotive Transport | T6-EN-02 | — | capacity | road freight; artifact form diffuses instantly |
| T6-TR-02 | Flight | T6-EN-02, T5-MA-02 | — | capacity | 1903 → jets by the epoch |
| T6-IN-01 | Broadcast | T5-IN-01 | — | capacity | radio: artifact diffuses, capacity doesn't |
| T6-IN-02 | Early Computing | T5-MA-02, T6-EN-01 | — | capacity | the epoch's frontier; feeds E0-ELEC |
| T6-MIL | *Regime: armour, airpower, radar; missiles/atomics at the frontier* | T6-MA-01, T6-TR-02 | fuel | — | BL-274 rows |
| T6-ME-01 | Antibiotics & Mass Vaccination | T5-ME-01, T6-MA-01 | — | practice+artifact | demographic transition completes |

---

## What "not every nation is equal" realistically means by 1960

This is the section the sim gets tuned against. Earth's 1960 gives four calibration facts:

**1. The spread is in capacity, not knowledge.** By 1960 essentially every connected polity
*knows about* everything through T6 — awareness spread is ~0 bands. Capacity spread is **3–4
bands**: frontier nations hold full T6 (jets, atomics, computers); the median holds T5 capacity
running imported T6 artifacts; the least industrialised hold **T2–T3 domestic capacity**
(subsistence agriculture, no metallurgical base) while carrying T6 rifles and radios. A nation's
tech state must therefore be at least two numbers per domain — **capacity band** and **artifact
access** — or the sim cannot represent the most common condition on the 1960 map.

**2. Artifacts compress military spread to ~1–2 bands; capacity spread stays wide.** Surplus
weapons diffuse in years, so 1960 armies cluster: frontier fields missiles, median fields the
last war's surplus, laggard fields rifles — everyone is armed within a band or two. But only
~a dozen Earth nations could *manufacture* a 1960 aircraft. BL-274's rosters should read
**artifact access** for what a nation fields and **capacity** for what it can replace under
blockade — that distinction is a war-endurance mechanic for free.

**3. Divergence is late and fast.** From 0 to ~1750, connected nations stay within ~1 band —
leadership moves around, but nobody compounds. T5 is the divergence event: fuel-gated compounding
opens 3-band gaps in a century. The sim reproduces this if (and only if) capacity gates bind:
T5's fuel gate is the Great-Divergence mechanism, and it is *already* how `run_settlement` picks
its furnace dates — endowment, not virtue, extended across 2000 years.

**4. Nobody connected is at T1, and leadership is not sticky.** The contact graph forbids stasis:
practices reach everyone eventually, so by the epoch only genuinely isolated pockets sit below
T2. And the T2 lesson — Earth's classical leader was overtaken by its periphery twice — means
band leadership should migrate across a 2000-year run; a sim where the Stage 0 cradle always
finishes first is mis-tuned (BL-275's sweep can assert this as a distribution property).

### The 1960 handoff

At campaign start each nation's band vector becomes: available buildings/recipes (capacity bands
→ the unlocked set, per BL-156's set-membership shape), BL-274 roster rows (artifact + capacity
per fact 2), BL-273 demography parameters (T5-ME/T6-ME reached → growth rates), and Nation AI
economic character. The player's corp then plays the existing `tech_tree.lua` arc from wherever
its chartering nation's vector puts it — a corp chartered in a T5-capacity nation starting Era 0
*behind* the frontier is a legitimate, interesting start.

---

## Acquisition model (how the sim runs it)

Per year-tick, per nation, per domain — all integer, seeded, deterministic:

1. **Invention roll** at the frontier: a nation with all prereqs + gate satisfied accumulates
   progress toward the band's next capacity node; creed modifiers apply (forge god → Materials,
   sealed-oath → Institutions — the BL-218 mechanism, extended).
2. **Practice diffusion**: nodes held by contact-graph neighbours (trade or war contact — BL-274
   open Q3's lean, confirmed here) diffuse at a rate scaled by the receiver's Institutions band.
3. **Artifact flow**: tradable nodes propagate along active trade routes ~immediately; access
   decays if contact lapses (artifacts wear out; capacity doesn't).
4. **Regression**: collapse/war ruptures can destroy *capacity* (burn the record, wreck the
   plant) but never awareness — a post-collapse nation rebuilds faster than it invented,
   which is exactly Earth's post-war pattern.

## Deliberate compressions

- 3–5 nodes per band per domain, dead-end leaves allowed, no re-converging mesh (BL-087's shape
  rules, reused).
- Military unit detail delegated wholly to BL-274's table; the ladder owns only band boundaries.
- Saltpetre/sulphur, pasture, and river signals don't exist; gates use the named-substitution
  set (`fuel` proxies powder chemistry, `grassland` proxies horses) per the BL-221 convention.
- No tech *loss* short of rupture; no secrecy mechanics (guarded knowledge leaked on Earth in
  decades — modelling it buys little at year-tick granularity).

## Open questions (for the design conversation)

1. **Band count reconciliation** — six here vs BL-274's four-band lean. Proposal above: rosters
   group the six; needs Ben's confirmation before either authors data.
2. **State representation** — capacity band + artifact access per domain is 14 small ints per
   nation. Enough? Or does artifact access collapse to one nation-level int (lean: one int —
   artifacts cluster in practice)?
3. **Where the data lives** — authored C++ table beside the roster table (lean: yes — the sim
   consumes it, nothing player-facing reads it yet), or a second Lua data file like
   `tech_tree.lua`? Determinism favours C++; moddability argues Lua later.
4. **Does the player ever see it?** Partially advanced by § Geometry — the tech fog implies a
   rendered web *somewhere*, and a 1960 codex page per nation would make the
   philosophical-development payload visible — but the surface is BL-208/BL-271 territory,
   not this item's.
5. **Density grain** — § Density test's three worked grains; recommendation is medium
   (~100–150 nodes web-wide, fine grain only around keystones). Logged as NR-056; Ben decides
   against the examples.
