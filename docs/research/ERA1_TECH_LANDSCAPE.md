# Era 1 Tech — Research Landscape (2000–2040)

> **Status: research scaffolding — not authority.** Captured 2026-06-30 on branch
> `claude/era1-tech-research` during Era 1 tech-system ideation. This document points
> *toward* a design; it settles nothing. The eventual design lands as a backlog item, and
> authority propagates into `docs/economy/ERAS.md` (and any new tech doc) only when the work
> lands. Read it as a map for further reading, not a spec.

## What this is for

We are ideating the Era 1 technology/research system. Before building anything, we need three
questions answered *above* the level the current code can reach:

1. **What is the research *for*** — its job in the game, not its contents.
2. **What the system looks like at altitude** — how many threads, what anchors them, how compressed.
3. **What the user experience is** — how the player sees, chooses, and feels progress.

This note gathers the external reference (real 2000–2040 milestones + plausible sci-fi
alternatives) that those three questions need as input.

### Timeline reframe

The campaign epoch is being moved from 1960 → **~2000**, with **Era 1 ≈ 2000–2040**. This anchors
Era 1 to the richest, best-documented near-future space window we have (reusable launch, ISRU,
commercial stations, asteroid sample return, nuclear propulsion revival) plus a deep sci-fi bench
for the *plausible alternatives*. This is a fiction retcon to CONCEPT/ERAS — captured here, not yet
applied to the authority docs.

### The streamlining principle

Earth's real tech development is messy, redundant, and path-dependent. A game wants a *compressed,
legible* pipeline. So the job is to name the **3–5 load-bearing milestones per thread** — the ones
that actually flipped a capability regime — not the encyclopedia, and to note where **sci-fi offers
a cleaner branch than the messy real path.** That compression is what "more streamlined than actual
Earth" means.

---

## Three cross-cutting insights (these answer "what is research for")

**1. The demand-loop lesson (the big one).** The asteroid-mining startups — Planetary Resources
(founded 2012) and Deep Space Industries (2013) — raised real money and both folded/were absorbed by
~2018–19. Not because mining is impossible, but because there was **no in-space demand**, and you
*cannot* return platinum-group metals to Earth without crashing their own price (the "oversupply
paradox"). **Lesson for Io:** the resource frontier is worthless without a *space-side* demand sink,
and the propellant loop **is** that sink. This validates Era 1's entire strategic question, and says
the economy must create demand *before* supply pays off. It is why ISRU/propellant is the keystone
tech, not asteroid metals.

**2. Reusability is *the* regime change.** Cost-to-orbit is the master variable — stations, ISRU
industry, and mining only pencil out below a cost threshold. Reusability is the single milestone that
flipped space from *project* to *industry*. That is literally the Era 0→1 valve: "Rocketry" =
"reusable launch."

**3. The TRL gap is the streamlining license.** Almost everything below sits at TRL 4–6
(demonstrated, not industrialised) as of the mid-2020s. Asserting *industrial-scale* ISRU and orbital
manufacturing by 2040 is optimistic-but-not-fantasy — that is the sci-fi-plausible license. The game
compresses the expensive, decade-long TRL 6→9 climb into a single research milestone.

---

## The threads (the spine of the tech system)

Organised as disjoint threads, because that is the shape the tech system wants. A *lean* Era 1 set
compresses these to ~3 (Threads 1, 2, 4); Threads 3, 5, 6 fold in as economic interdependencies or
later capstones.

### Thread 1 — Launch & Access → *"Rocketry" / the Era gate*

- **Real milestones:** Falcon 9 first booster landing (Dec 2015) → first reflight (SES-10, Mar 2017)
  → Falcon Heavy (2018) → Starship full-stack development (2020s). Cost-per-kg to LEO falls ~10×
  over the window.
- **Sci-fi branch:** space elevator (Clarke, *The Fountains of Paradise*; KSR), skyhook / rotovating
  tether, mass driver / electromagnetic launch, laser / beamed-energy launch.
- **Io insight:** the master valve — the cleanest single thing to make the Era 0→1 gate. Everything
  else is downstream of cheap access.
- **Search:** *reusable launch vehicle economics*, *cost per kg to orbit trend*, *space elevator
  feasibility tether material*.

### Thread 2 — In-Situ Resource Utilisation (ISRU) → *Ice Extractor, Surface Extractor, the propellant loop*

- **Real milestones:** LCROSS confirms lunar water ice (2009) → orbital water-ice mapping → **MOXIE
  makes O₂ from Mars CO₂** (Perseverance, 2021–23) → lunar PROSPECT / propellant-from-regolith
  programs. *The* central near-future space-economics challenge.
- **Sci-fi branch:** *The Expanse* (Belters mining ice; water as the Belt's lifeblood and currency),
  *The Martian* (in-situ water and fuel), KSR Mars regolith processing.
- **Io insight:** the keystone. "Close the propellant loop" is a real, named engineering goal, not
  flavour. Note the **methane tell** — SpaceX chose methane (Raptor) partly *because* it is ISRU-able
  (Sabatier: CO₂ + H₂ → methane). Propellant *type* and ISRU are coupled in reality; couple them in
  Io too.
- **Search:** *in-situ resource utilisation propellant*, *lunar water ice extraction*, *Sabatier
  reaction Mars fuel*, *propellant depot architecture*.

### Thread 3 — Propulsion & Mobility → *the convoy propellant tax*

- **Real milestones:** chemical hydrolox/methalox baseline → ion/Hall thrusters at scale (Dawn;
  Starlink) → solar-electric propulsion → **nuclear thermal/electric revival** (DARPA/NASA DRACO,
  announced 2023, demo targeted ~2025–27).
- **Sci-fi branch:** the *Expanse* **Epstein drive** is the canonical "what if propellant were nearly
  free" branch (fusion torch) — the most influential near-future propulsion fiction. Also Orion
  nuclear-pulse, VASIMR, solar sails.
- **Io insight:** a research thread here directly lowers the propellant-per-leg tax — a clean,
  legible payoff. The Epstein drive is the "endgame" anchor if a propulsion capstone is ever wanted.
- **Search:** *nuclear thermal propulsion DRACO*, *specific impulse comparison propulsion*, *Epstein
  drive plausibility*.

### Thread 4 — Orbital Construction & Manufacturing → *Assembly Plant, Orbital Port*

- **Real milestones:** ISS modular assembly (proof orbital construction works) → in-space
  manufacturing (Made In Space 3D printer 2014; ZBLAN fibre) → **Varda returns in-space-manufactured
  pharma crystals** (W-1 capsule, 2024) → commercial stations (Axiom, Orbital Reef, Starlab) →
  on-orbit servicing/assembly (OSAM, Redwire).
- **Sci-fi branch:** O'Neill cylinders (the foundational space-manufacturing-habitat vision), the
  *Expanse*'s Tycho Station / the *Nauvoo* mega-build.
- **Io insight:** real in-space manufacturing today is *tiny high-value goods* (fibre, crystals,
  pharma) — the game scales it to industrial. Orbital Port = the logistics node that makes a station
  a trade participant, not an island.
- **Search:** *in-space manufacturing ZBLAN*, *on-orbit assembly OSAM*, *commercial space station LEO
  economy*, *O'Neill cylinder*.

### Thread 5 — Body Resource Extraction → *water, iron-nickel ore, PGM, regolith*

- **Real milestones:** Planetary Resources / DSI rise-and-fall (2012–2019, the cautionary tale) →
  **Hayabusa2 returns Ryugu sample** (2020) → **OSIRIS-REx returns Bennu sample** (2023) → Chang'e-5/6
  lunar samples (2020/2024) → **Psyche** launches to a metallic asteroid (2023, arrival ~2029).
- **Sci-fi branch:** the *Expanse* Belt as an industrial periphery; near-universal in asteroid-mining
  fiction.
- **Io insight:** see insight #1. Extraction is the *supply* side — economically dead without the
  demand the other threads create. The strongest argument that the tech threads must be
  *interdependent at the economic level* even while *disjoint at the unlock level*.
- **Search:** *asteroid mining economics platinum oversupply*, *Psyche mission metallic asteroid*,
  *Bennu sample return*.

### Thread 6 — Power, Automation & Materials → *the economic substrate / research inputs*

- **Real milestones:** terrestrial solar PV cost collapse → **space fission power** (Kilopower/KRUSTY
  test, 2018) → space-based solar power demos (Caltech SSPD-1, 2023) → robotics/autonomy and advanced
  materials throughout.
- **Sci-fi branch:** fusion (the perennial), space-based solar power (Asimov's *"Reason"*; real SBSP
  studies), and **von Neumann / self-replicating machines** as the extreme-automation branch.
- **Io insight:** more Era-0 *substrate* than an Era-1 unlock — the natural home for whatever the R&D
  Lab *consumes* (the Electronics question) and for "raises the ceiling" bonuses. Probably not its own
  Era-1 thread, but the resource backbone behind all of them.
- **Search:** *space nuclear power Kilopower*, *space based solar power feasibility*, *space
  manufacturing automation*.

---

## The sci-fi shelf (the "plausible alternatives" half)

Ranked by usefulness to Io's economics:

1. ***The Expanse* (Corey)** — the definitive near-future *space-economy* fiction: Belt ISRU,
   propellant-as-lifeblood, the Epstein drive, mega-construction.
2. **KSR *Mars trilogy*** — ISRU and terraforming at industrial scale.
3. ***The Martian* (Weir)** — small-scale ISRU made legible.
4. **Asimov / Clarke** — for the launch-and-power alternatives (space elevator, SBSP).

## Two live debates worth knowing

- **Moon-first vs asteroid-first.** Lunar water is closer and gravity-well-cheap to a depot;
  asteroids are metal-rich but distant. Maps onto Io's Selene-vs-belt body choices.
- **Return-to-Earth vs space-only economy.** The consensus that asteroid wealth only works if it
  **stays in space** — the design backbone of Io's whole premise (and insight #1).

---

## How this feeds the next step

- **Insight #1** reframes *what research is for*: it builds the **demand loop**, not just a tech list.
- **The threads** give the *system altitude*: how many threads, what anchors each, what to compress.
- **The debates** seed the *UX* choices (Moon-vs-asteroid, stays-in-space).

## Design state so far (parked — from the ideation that produced this note)

Decisions reached before pausing, to resume from:

- **Ambition:** *lean* Era 1 tech set — ~3 techs: **Rocketry** (gates Era 1 entry), **In-Situ Resource
  Utilisation** (Ice + Surface Extractor), **Orbital Construction** (Assembly Plant + Orbital Port).
  Each unlocked separately (disjoint); only Rocketry is wired into the Era gate.
- **Mechanism:** *build + accrue* — an **R&D Lab** building (Era-0 constructible, needs workforce),
  targets one tech at a time, consumes an input each tick and accrues `research_rate` (× workforce)
  toward a `research_cost`. Scale via building more Labs. Resolves the ERAS "purchased" vs CONCEPT
  "build it" wording conflict in favour of build-it.

**Open sub-decisions (not yet locked):**

1. **Input resource** — Electronics (recommended; ties research into the Era-0 economy) / money-only /
   abstract research budget.
2. **Era scope** — per-corporation (recommended) / global.
3. **Tech ordering** — fully disjoint / Rocketry-as-prereq for the other two.
4. **Scaling model** — confirm multi-Lab = parallel/faster research.
5. **ISRU/Orbital timing** — researchable during Era 0 (pre-bankable, enables a rush-vs-bank choice) /
   only after entering Era 1.

**Scope flag:** this is a real departure from ROADMAP's "Research excluded from v0.1.0." Proposed
resolution: the *lean gate-tech* is in-scope (it makes the prototype's premise testable) while the
*full quest-based tree* stays deferred; reconcile ERAS ↔ ROADMAP **when the work lands**, not before.

**Next dig (suggested):** go a level deeper on Thread 2 (ISRU / propellant), the keystone.

---

# Tech-tree structure — first sketch (2026-07-01)

> Still **scaffolding, not authority** (same header caveat as above). This section is the design
> sketch produced from the ideation that followed the research pass — quest structure, the
> itemisation schema, and one worked quest. It **supersedes the "Design state so far (parked)"
> block above** where the two differ (newest-dated wins). It settles the *shape*; the numbered
> open questions at the end are still owed. Tracked as **BL-087**.

## The threading insight — *gate, quest, and tech are the same object*

The Era 0→1 gate in `ERAS.md` is already a **heterogeneous condition set** met simultaneously:
Rocketry researched **+** Launchpad built & staffed **+** propellant stockpile ≥ threshold. That is
not a research bar — it mixes research, a structure, and an *economic state*.

Generalise it. **An Era gate is a big quest; a quest is a chain of techs; a tech is a small gate** —
all three are "a set of conditions that must hold to unlock something." The model is **self-similar**,
and that self-similarity is what threads the research landscape, the schema, and the existing ERAS
gate into one mechanism.

The payoff: a tech's unlock condition need **not** be research-accrual alone. It can be an *economic
state*. This captures Ben's phase-1 idea directly — "launch Corp rockets to the Moon" gated behind a
corporation able to **supply a certain excess**, or a **market where rockets are sufficiently cheap**.
Research and economy become the *same gating vocabulary*, which is exactly how you make the game
**demand use of its systems** rather than let a player drift past them (the Terra Invicta lesson,
below): some techs are *earned through the economy*, not merely ticked toward.

### The condition vocabulary (what a gate/quest/tech unlock is built from)

An unlock is an **AND/OR expression** over these primitives (keep it mostly-AND for legibility):

| Condition | Holds when… | Example use |
|---|---|---|
| `research`  | accrued research points ≥ `cost` (the R&D Lab build+accrue mechanism) | most techs |
| `structure` | a named building exists / is staffed on a body | Launchpad staffed |
| `stockpile` | a resource on a body ≥ threshold | propellant reserve for the Era gate |
| `market`    | a good's cleared price ≤/≥ threshold | "rockets cheap enough" to unlock Moon launch |
| `surplus`   | a corp's sustained *excess* output of a good ≥ threshold for N ticks | "supply a certain excess" |
| `era`       | a given Era is active | quest availability |

The same vocabulary expresses a single tech, a quest capstone, and an Era gate — only the scale
changes.

## Two kinds of quest

| | **Gate quests** | **Standing lines** |
|---|---|---|
| Purpose | define an Era's identity; capstone opens the next Era | persistent depth across *every* Era |
| Count | 2–3 live per Era | 2–3 carried through all Eras |
| Gates an Era? | yes (exactly one capstone tech per gate-quest) | never |
| Examples | Rocketry, The Propellant Loop, Orbital Industry | **Logistics**, **Military**, Economy/Automation |

The **standing lines** are where the ~100-hour depth lives and where **logistics and military** — flagged
as large — belong: they never gate an Era, they only deepen, so "what opens the next Era" stays
unambiguous.

## Itemisation schema (mirrors `backlog.json`: a queryable index + a prose reasoning field)

**Quest record:**

```json
{
  "id": "E1-PROPLOOP", "name": "The Propellant Loop", "era": 1,
  "type": "gate",                 // gate | standing
  "thread": "ISRU",               // ties to the research threads above
  "thesis": "Close the propellant loop off-world.",
  "capstone": "E1-PL-23",         // the tech whose completion IS the quest goal
  "gates": null,                  // which Era this capstone opens, or null for standing
  "target_tech_count": 25, "status": "sketch"
}
```

**Tech record:**

```json
{
  "id": "E1-PL-09", "name": "Water Electrolysis", "quest": "E1-PROPLOOP",
  "kind": "invention",            // invention (spine) | tier (upgrade)
  "tier": null,                   // 1/2/3 for tier techs
  "prereqs": ["E1-PL-08"],        // tech ids; MAY cross quests
  "cost": "M",                    // cost model B — see below
  "payoff": "recipe",             // taxonomy below
  "unlocks": "Recipe: Water → LH₂ + LOX",
  "condition": "research",        // condition-vocabulary primitive(s)
  "value": "The keystone step — turns mined ice into propellant; without it the loop is inert.",
  "grounding": "Real: water electrolysis yields the highest-Isp cryo propellants.",
  "status": "sketch"
}
```

**`payoff` taxonomy** (the queryable proxy for "what is actually valuable"), descending desire:
`gate` (opens the next Era — one per gate-quest) · `resource` (new resource access) · `building`
(new structure) · `recipe` (new recipe on an existing building) · `efficiency` (a tier upgrade:
yield ↑, workforce scalar ↓, boil-off ↓, propellant-per-leg ↓) · `enabling` (pure prerequisite —
use sparingly; these are the "boring" nodes). A healthy 25-tech quest audits to roughly
1 `gate` + 2 `resource` + 3 `building` + 5 `recipe` + 13 `efficiency` + ~1 `enabling`.

**Cost model — B (small vocabulary, default `M`).** `S / M / L / XL`, mirroring backlog difficulty.
Inventions = `M`, tier chains escalate `S→M→L`, capstones = `XL`. At sketch stage **default every
tech to `M`** and tag only the outliers — giving "all the same" simplicity now with a place to
differentiate when tuning matters, no rework. (`A` uniform and `C` bespoke-numeric were the rejected
alternatives.)

**Every field is also a UI element** (why itemising is progress toward picturing the tree):
`name`+`payoff` → node label/icon · `prereqs` → edges · `cost` → progress ring · `value`+`grounding`
→ tooltip (where "fiction supplies the theory" lives) · `kind` → node size/shape.

## Worked gate-quest — **The Propellant Loop** (`E1-PROPLOOP`, keystone, ~25 techs)

Distinct spine **inventions** in bold; the rest are tier/efficiency fill. Note the capstone and two
late nodes gate on **economic** conditions (`surplus` / `market` / `stockpile`), not research — the
threading insight in practice.

| id | tech | kind | pre | cost | payoff | condition | unlocks |
|---|---|---|---|---|---|---|---|
| E1-PL-01 | **Volatile Prospecting** | inv | — | M | resource | research | ice/water richness bands appear in survey |
| E1-PL-02 | Volatile Prospecting II | tier2 | 01 | S | efficiency | research | survey range/accuracy ↑ |
| E1-PL-03 | **Regolith Excavation** | inv | 01 | M | building | research | Surface/Ice extraction rig |
| E1-PL-04 | Regolith Excavation II | tier2 | 03 | S | efficiency | research | throughput ↑ |
| E1-PL-05 | Regolith Excavation III | tier3 | 04 | M | efficiency | research | throughput ↑ |
| E1-PL-06 | **Water Extraction (thermal)** | inv | 03 | M | recipe | research | regolith/ice → raw water |
| E1-PL-07 | Water Extraction II | tier2 | 06 | S | efficiency | research | yield ↑ |
| E1-PL-08 | **Water Purification** | inv | 06 | M | recipe | research | raw water → clean water |
| E1-PL-09 | **Water Electrolysis** | inv | 08 | M | recipe | research | water → LH₂ + LOX (keystone) |
| E1-PL-10 | Water Electrolysis II | tier2 | 09 | S | efficiency | research | energy per kg ↓ |
| E1-PL-11 | **Cryogenic Liquefaction** | inv | 09 | M | recipe | research | gas → liquid H₂/O₂ |
| E1-PL-12 | **Cryogenic Storage** | inv | 11 | M | building | research | depot tank; boil-off baseline |
| E1-PL-13 | Cryogenic Storage II | tier2 | 12 | S | efficiency | research | boil-off ↓ |
| E1-PL-14 | Cryogenic Storage III | tier3 | 13 | M | efficiency | research | boil-off ↓ |
| E1-PL-15 | **Sabatier Reactor** | inv | 09 | M | recipe | research | CO₂+H₂ → methane (the "methane tell") |
| E1-PL-16 | Sabatier II | tier2 | 15 | S | efficiency | research | yield ↑ |
| E1-PL-17 | **Methane Liquefaction** | inv | 15 | M | recipe | research | methalox propellant path |
| E1-PL-18 | **Autonomous Mining Rig** | inv | 05 | M | efficiency | research | workforce scalar ↓ *(cross-links Automation line)* |
| E1-PL-19 | Autonomous Rig II | tier2 | 18 | S | efficiency | research | workforce scalar ↓ |
| E1-PL-20 | **In-Situ Tank Fabrication** | inv | 12 | M | building | `stockpile` | local depot without Earth lift (needs regolith/metal on body) |
| E1-PL-21 | **Orbital Propellant Depot** | inv | 20 | L | building | `structure` | the demand-sink node *(cross-quest dep: Orbital Port)* |
| E1-PL-22 | Depot Network II | tier2 | 21 | M | efficiency | research | transfer loss ↓ |
| E1-PL-24 | **Boil-off Recovery** | inv | 14 | M | efficiency | research | reclaim vented gas |
| E1-PL-25 | Propellant Market Hook | inv | 21 | S | — | `market` | depot *sells* propellant into the space market (creates the price signal) |
| **E1-PL-23** | **Closed-Loop Certification** | **capstone** | 21,17 | **XL** | **gate** | **`surplus`** | **quest goal: propellant loop closed on one off-world body** (body produces propellant excess ≥ threshold for N ticks) |

Reads as a story — *find it → dig it → split it → store it → sell it → close the loop* — and the
capstone **is** Era 1's strategic thesis, so completing the quest and proving the Era's premise are
the same act. The capstone gates on a *sustained economic surplus*, not a research total: you don't
research your way into Era 2, you **build an economy that earns it**.

## Quest map — Era 0 & Era 1 (stubs, to expand)

| id | quest | era | type | thread | thesis / capstone |
|---|---|---|---|---|---|
| E0-HEAVY   | Heavy Industry            | 0 | gate | — | steel/chem/refining base |
| E0-ELEC    | Electronics & Computing   | 0 | gate | — | Info-Age footing; feeds the R&D Lab input |
| E0-ROCKET  | Rocketry                  | 0 | gate | Launch | **capstone gates Era 1** (operable Launchpad) |
| E1-LAUNCH  | Launch & Access           | 1 | gate | Launch | cheap reusable lift *(may fold into E0-ROCKET's tail)* |
| E1-PROPLOOP| The Propellant Loop       | 1 | gate | ISRU | **worked above** — close the loop off-world |
| E1-ORBITAL | Orbital Industry          | 1 | gate | Orbital Constr. | Assembly Plant + Orbital Port |
| L-LOG      | Logistics *(standing)*    | 0→ | standing | Propulsion | deepens into the convoy/propellant tax (chemical → ion → nuclear-thermal → *parked* Epstein) |
| L-MIL      | Military *(standing)*     | 0→ | standing | — | **reserved** — do not enumerate until the combat *system* is mapped |
| L-AUTO     | Economy / Automation *(standing)* | 0→ | standing | Power/Automation | workforce-scalar & power substrate behind every quest |

## Comparable — Terra Invicta (what to take, what to avoid)

The one near-future game that does space-building at depth; a different core loop, but instructive.
**Take:** it forces players spaceward with *three stacked pressures* — an external clock (alien
threat), a soft economic pull (Earth resources cost lift; space resources pool free), and a
**shifting bottleneck** (early game gated by *Boost/lift*, then the binding constraint *switches* to
*Mission Control*). The shifting bottleneck is the model to steal — the thing that limits you changes
as you grow, so the optimal action changes and you keep re-engaging different systems. Io's analogue:
the concept's **"WW3-scale rupture"** as a *visible countdown* is the external clock that stops the
Earth-conquest player from stalling indefinitely; the launch-vs-ISRU cost gradient is the soft pull.
**Avoid:** TI's UI is widely reviled ("dense, obtuse, tiny icons"); the **tech tree specifically** is
"an inaccessible mess" (no hover summaries, must exit the tree to act); and the **mid-game drags**
because the propulsion tiers offer no meaningful choices. Direct cautions for our propulsion line
(give *middle* tiers real range/thrust/fuel trade-offs) and for the tech-tree UI (hover = the `value`
tooltip; act without leaving the tree). Sources:
[MIT Tech Review — asteroid-mining bubble](https://www.technologyreview.com/2019/06/26/134510/asteroid-mining-bubble-burst-history/) ·
[TI beginner's guide (wiki)](https://wiki.hoodedhorse.com/Terra_Invicta/Help:Gameplay_Guides/Beginner's_Guide) ·
[TI "UI absolutely repulsive?" thread](https://steamcommunity.com/app/1176470/discussions/0/3361398331724778980/) ·
[TI Mission Control (wiki)](https://wiki.hoodedhorse.com/Terra_Invicta/Mission_Control_Priority).

## Corrections to the research above (2026-07-01 web pass)

Three facts from the web pass that update the threads above:

1. **DRACO (nuclear-thermal) was cancelled** in the FY2026 budget (May 2025) — no NTP/NEP funding.
   Thread 3's "demo ~2025–27" is stale. This *helps* the design: nuclear-thermal becomes a clean
   *parked/plausible* propulsion capstone (with the Epstein drive beyond it), not an active-real node.
   [Aerospace America](https://aerospaceamerica.aiaa.org/nasa-and-darpa-are-cautioned-against-overselling-the-performance-of-their-nuclear-rocket-tech/)
2. **Launch (Thread 1) and ISRU (Thread 2) are partly *rivals*, not pure complements.** Cheap
   reusable lift *undercuts* the off-world-propellant business case below a distance/scale threshold
   (breakeven ≈ $40k/kg to cislunar). That rivalry *is* the Era 1 strategic tension — mine-in-place
   only pays past a distance/scale line, matching the Selene-vs-belt choice.
   [ISRU economics (arXiv)](https://arxiv.org/pdf/2303.09011) ·
   [Water on the Moon (New Space Tracker)](https://newspacetracker.com/articles/water-ice-on-the-moon/)
3. **The asteroid-mining bust was as much a *capital/runway* failure as a demand failure** — the
   decades-long gap between investment and revenue outran VC patience. Argues extraction techs should
   carry **long payback curves** (a bank-vs-rush tension), which maps onto open sub-decision #5.
   [MIT Tech Review](https://www.technologyreview.com/2019/06/26/134510/asteroid-mining-bubble-burst-history/)

## Open questions (design-owed — carried into BL-087)

1. **Within a quest: linear spine or mesh?** Legibility favours a mostly-linear spine + a few optional
   branches, not a web.
2. **Do standing lines gate anything, ever?** Lean: no — pure depth keeps "what opens the next Era"
   unambiguous.
3. **Is a "tech" a passive unlock or a required *build*?** concept.md says build-it; at 25/quest, most
   techs likely unlock a *capability/upgrade* on existing buildings, and only spine techs add
   structures.
4. **Cross-quest dependencies** (E1-PL-21 needs the Orbital Port; E1-PL-18 the Automation line) —
   allowed, or kept disjoint? Real interdependency is richer but costs "disjoint tree" legibility.
5. **How many economic-gated techs?** Powerful for "demand use of systems," but over-used they stall a
   player who is behind economically. Likely reserve for *capstones* and a few marquee unlocks.
6. **Scope reconciliation.** The *lean gate-tech* (Rocketry/ISRU/Orbital) is prototype-relevant; the
   *full quest tree* is post-prototype. ROADMAP currently excludes Research from v0.1.0 — reconcile
   ERAS ↔ ROADMAP **only when the work lands** (authority time-slicing), not from this sketch.

# Resolutions — design session (2026-07-08)

> Resolves the six open questions above (mobile design session with Ben). Newest-dated wins: where
> this section contradicts the first sketch, this section supersedes it. Two answers go beyond the
> question asked and **reframe the sketch** — the Era model and what capstones open — recorded
> first because the per-question answers read differently in their light.

## The Era reframe — *Eras are catastrophic events; capstones open quest trees*

- **Eras are based on time, not tech.** An Era transition is a **catastrophic world event** — e.g.
  a war, a satellite/Kessler cascade — arriving on the world clock. This *is* the external clock
  from the Terra Invicta comparable: the world moves on whether or not the player is ready. Tech
  can move *faster* than the Era clock, but tech never opens an Era.
- **Capstones open quest trees.** The `ERAS.md` Era 0→1 condition set (Rocketry researched +
  Launchpad staffed + propellant stockpile) should be re-read as gating a new **quest tree**, not
  the Era itself ("these should have been recorded as gating new quest trees" — Ben). Generalised:
  a gate-quest capstone opens the next quest tree(s). The threading insight survives intact —
  gate, quest, and tech remain one condition-set object — only *what the largest gates open*
  changes: further quests, not Eras.
- **Determinism.** Catastrophic Era events must be deterministic — a seeded schedule or a seeded
  world-state trigger — per the standing determinism rule. No random ruptures.
- **Authority untouched.** `ERAS.md` is *not* edited from this reframe — authority time-slices;
  the reframe lives here (and in BL-087's design field) until implementing work lands.

**New open questions spawned (the next owed set):**

- **A.** Are Era-event timings fixed seeded dates, or influenced by world state? And is the event
  *foreseeable* — the TI visible-countdown lesson argues the player should see it coming?
- **B.** What mechanically changes at an Era boundary — demand shifts, destruction, market
  disruption, which quest trees become era-appropriate?
- **C.** Terminology: a "gate quest" now opens quest trees, not Eras — rename (keystone quest?)
  when itemisation resumes, and revisit the `gates` field of the quest record.

## Q1 — quest shape: mostly a binary tree, some dead-end leaves

> **OVERTURNED (2026-08-04, Ben).** The binary-tree / no-re-converging-mesh shape is superseded
> by the **constellation geometry** settled in `ANCIENT_TECH_LADDER.md` § Geometry: a shared
> radial web (rings = bands, sectors = domains), travel by adjacency (OR), meaning nodes
> carrying AND condition-sets, keystone forks with exclusion via availability windows, and a
> tech fog so the whole web is not visible at once. The *motive* below survives — the TI
> "inaccessible mess" failure mode stays excluded, now by node-count discipline and the fog
> rather than by forbidding re-convergence. Kept for the record.

Branching factor ≤ 2 out of any node; no re-converging mesh. Dead-end leaves — tier chains and
optional techs that lead nowhere further — carry the depth without onward requirement. Sits
between the strictly-linear and mesh options: route choice exists, but the TI "inaccessible mess"
failure mode stays excluded.

## Q2 — standing lines: never gate Eras; **can gate quest lines**

(Corrected in-session: an initial "can gate Eras" selection was a mis-pick; the intent was quest
*lines*.) A quest tree's availability condition may include standing-line depth (e.g. Logistics ≥
tier N) — adding a line-depth primitive to the condition vocabulary alongside `era`. Under the Era
reframe, *nothing* tech-gates an Era, so the original worry (ambiguity about what opens the next
Era) dissolves: Eras arrive; quests are what gets opened.

## Q3 — the build coupling lives on research *generation*; payoffs are mostly tangible

- **Input side:** research capacity is improved by **dedicated buildings** (the R&D Lab) or
  **scales with industry** — the exact mix is deliberately unresolved, a playtest-tuning item.
- **Output side:** a completed tech's payoff should be **mostly tangible effects** — a new
  building, a more efficient production method (a recipe), a new gathering technique — and only
  sometimes a permanent passive buff. This rebalances the sketch's healthy-quest audit (which had
  ~13/25 pure `efficiency` nodes): prefer expressing an improvement as a new *method* the player
  adopts over a bare percentage buff.

## Q4 — cross-quest prerequisites: allowed, sparingly, visually marked

A handful of deliberate cross-links per Era (the Depot→Orbital Port and Rig→Automation cases
already tabled), visually marked in the tree UI. As recommended in the sketch.

## Q5 — economic gates: capstones + a few marquee nodes

Every gate-quest capstone gates economically (as the Propellant Loop's does), plus 1–2 marquee
mid-quest nodes per Era. Not liberal — over-use stalls a player who is behind economically (the
asteroid-mining capital/runway lesson, correction #3 above).

## Q6 — scope: **all post-prototype**

Even the lean gate-tech implementation waits until after v0.1.0; the prototype keeps today's
hard-coded Era arrangement. (Supersedes the sketch's lean that the lean gate-tech was
prototype-relevant.) ROADMAP already excludes Research from v0.1.0, so nothing needs reconciling
now; ERAS ↔ ROADMAP reconcile only when post-prototype work lands.

## Resolutions — Era-event mechanics (2026-07-08, v0.2.0-scope)

> Resolves questions A–C spawned by the Era reframe above. Brief — this is v0.2.0-horizon design,
> not near-term.

**A — timing:** a **seeded date** per campaign (deterministic), with a **visible in-UI countdown**
once conditions near it — the Terra Invicta external-clock lesson made literal: the player sees
the rupture coming and can race it, rather than being blindsided.

**B — boundary effects:** an Era event is not purely additive. It (1) **shocks demand/markets** —
shortages and price spikes ripple through the nation substrate; (2) **selectively destroys** —
some buildings/infrastructure are damaged (a satellite cascade wrecks orbital assets; a war damages
surface facilities), giving the moment real stakes and a recovery arc; and (3) **unlocks the new
Era's quest trees**. All three fire together — the event is a shock the player manages, not just a
door that opens.

**C — terminology:** rename **gate quest → keystone quest**, matching the "keystone" language
already used for the Propellant Loop. Apply on the next pass through the itemisation schema
(quest-record `type` field); not renamed retroactively in this doc's earlier sections.
