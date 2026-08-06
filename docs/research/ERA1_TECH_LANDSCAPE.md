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

---

# The Era 1 tree — first draft (2026-08-05)

> **DRAFT FOR REVIEW — the shape is the argument; the node list is a starting point, not a
> proposal.** Ben, 2026-08-05: *let's consider the shape of the Era 1 tree. It will be the first
> tech tree to gate keystones via quests, i.e. tangible actions done in game. I want to have a heavy
> hand in what actual nodes there are, however you are certainly capable of drafting the initial
> tree, which we can leave for a later review.* Read § Shape and § The deed primitive as design;
> read § The node draft as a first cut to be overwritten. Nothing here is transcribed into a data
> store yet — deliberately, so the node review isn't reviewing something that already looks settled.
> Tracked under **BL-087 (filter system / Era 1 content)**; the effect vocabulary is
> `docs/research/TECH_EFFECTS.md`.

## What makes this tree different from the pre-game one

Three differences, and all three follow from one fact: **someone is actually playing this one.**

| | Pre-game ladder (BL-296) | Era 1 tree |
|---|---|---|
| Who paths it | the year-tick sim, by endowment and contact | the player, by clicking |
| What a keystone costs | a creed or an endowment the nation already has | **a deed — something done, once, in game** |
| What effects do | describe a capacity band, read at the 1960 handoff | fire live, at the moment of unlock |
| Rings mean | historical bands (centuries) | depth within one era |
| Exclusion | the road not taken goes dark, invisibly | the player *chooses* and watches the other branch close |

The pre-game tree earns its inequality from the map. This one has to earn its progression from the
player's own economy — which is what the threading insight already said (*you don't research your
way into Era 2, you build an economy that earns it*), now made structural.

## The deed primitive — the missing condition type

The existing condition vocabulary (§ The condition vocabulary) is entirely **state**: `research`,
`structure`, `stockpile`, `market`, `surplus`, `era` are all predicates sampled at a tick. Every one
of them can be true today and false tomorrow, and none of them can express *"you did this"*.

A tangible action is not a state. Landing on a body you had never touched is an **event**: it
happens once, at a tick, at a place, and it stays true forever afterwards. That is a seventh
primitive, and it is the one Ben's framing needs:

```
deed:  subject   the act — first_landing | first_return | first_assembly | first_export | …
       scope     where it counts: any body / a named body class / off-world / this corporation
       count     1 for a first, N for a cumulative deed ("ten launches")
       recorded  the tick it fired, stored — a deed never un-fires
```

Three properties make it worth adding rather than faking with `structure` + `stockpile`:

1. **It is monotonic and cheap to serialise** — a flag plus a tick, which is exactly what BL-156's
   unlocked set already round-trips. No new save hazard.
2. **It is deterministic** — the deed fires inside the sim, from the same tick data everything else
   reads. Seed + command log still replays.
3. **It says something the state vocabulary can't.** "Own a launchpad and 500 propellant" is a
   shopping list. "Land on a body you have never operated on" is a story beat, and the player
   remembers doing it.

Deeds are also the honest home for **firsts** — the moments an Era is remembered by. A campaign log
line ("First footing on Ganymede analogue, day 4,412") comes free with the record.

## Shape

**Five sectors, three rings, one substrate line.** Sectors are the threads (§ The threads); rings
are depth within the era, not centuries.

| ring | name | the question it answers |
|---|---|---|
| **R1** | **Reach** | can you get there at all, and see what's there? |
| **R2** | **Foothold** | can you stay, and make something locally? |
| **R3** | **Industry** | does it pay for itself, and can it supply someone else? |

| sector | thread | what it owns |
|---|---|---|
| **Launch** | 1 | cadence, reuse, lift cost per kg |
| **Volatiles** | 2 | ISRU: water, electrolysis, the propellant loop |
| **Mobility** | 3 | transfer cost, the propellant tax, engine classes |
| **Yards** | 4 | orbital construction, assembly, the port as trade participant |
| **Extraction** | 5 | off-world deposits: regolith, iron-nickel, PGM |

**Power / Automation (thread 6) is not a sector** — it is a standing line (`L-AUTO`) that crosses
every sector, exactly as this doc's § Two kinds of quest requires: standing lines deepen, they never
gate an Era. `L-LOG` (Logistics) and `L-MIL` (Military, reserved) sit the same way.

**Vertices stay quests.** The rule from the pre-game geometry carries over unchanged — a ring
crossing in a sector *is* a quest object, named for the capability regime beyond it. What changes is
what the capstone asks for: on the pre-game side it was research plus an economic threshold; here
**every ring-crossing capstone carries a deed**.

## The four keystones — each opened by a deed

This is the part to review first, because it is the structural claim. Each keystone is a binary fork
with permanent exclusion, and **none of them are visible until their deed fires**. You do not choose
your propellant chemistry from a menu; you make propellant off-world once, and *then* the fork appears.

| keystone | opened by the deed | branch A | ⊘ | branch B |
|---|---|---|---|---|
| **Lift Doctrine** *(Launch, R1/R2)* | **Ten Flights** — cumulative launches ≥ N from one body | *Reusable Chemical* — low capital, cost falls with cadence, scales anywhere | ⊘ | *Fixed Infrastructure* — mass driver / tether: enormous capital and a site requirement, then near-zero marginal lift |
| **Propellant Doctrine** *(Volatiles, R2)* | **The First Tank** — produce any propellant off-world, once | *Methalox* — Sabatier route; wants a carbon source; the methane tell | ⊘ | *Hydrolox* — electrolysis route; wants water ice; higher performance, harder storage |
| **Yard Doctrine** *(Yards, R2/R3)* | **The First Truss** — complete one structure assembled off Earth | *Orbital Assembly* — build at the depot; no gravity well, total dependence on lift | ⊘ | *Surface Assembly* — build on the body; local materials, pays the well every launch |
| **Autonomy Doctrine** *(crossing Extraction × L-AUTO, R3)* | **The Empty Shift** — run an off-world building at zero workforce for N days | *Crewed Operations* — higher output ceiling, habitability and life-support burden | ⊘ | *Autonomous Fleets* — workforce scalar collapses, electronics dependency becomes strategic |

Four forks for one era matches the rule the industrial pass adopted — **fork count scales with the
band's divergence** — and Era 1 is the divergence era of the campaign proper. Each fork also keys
off a *different* axis: cadence, chemistry, geometry, labour. If two of them turn out to answer the
same question, one should go.

## The node draft — ~45 objects

> **This table is the part Ben wants to own.** It is drafted so the shape can be judged against
> something concrete: names, prereq direction, and the effect each node carries (using the eleven
> kinds from `TECH_EFFECTS.md`). Expect most names to change.

### Launch

| ring | node | effects |
|---|---|---|
| R1 | Expendable Lift Cadence | `modifier` → launch cost per kg · `unlock` → repeat dispatch without refit |
| R1 | Recovery & Refurbishment | `modifier` → launch cost with cadence |
| R2 | Reusable Booster | `upgrade` → Launchpad throughput · `modifier` → lift cost |
| R2 | Heavy Lift Stack | `unlock` → oversize payload class (assembly modules) |
| R3 | Launch Site Network | `reach` → dispatch from a second body · `retire` → single-site dispatch limit |
| R3 | *Fixed Infrastructure branch:* Mass Driver | `unlock` → non-rocket lift [Lift Doctrine B only] |

### Volatiles

| ring | node | effects |
|---|---|---|
| R1 | Volatile Prospecting | `intel` → ice/water richness bands appear in survey |
| R1 | Ice Extraction | `unlock` → Ice Extractor [designed building] |
| R2 | Water Purification | `unlock` → water recipe chain |
| R2 | Electrolysis | `unlock` → water → LH₂ + LOX recipe |
| R2 | Sabatier Reactor | `unlock` → CO₂ + H₂ → methane recipe [Propellant Doctrine A] |
| R2 | Cryogenic Storage | `unlock` → depot tank · `modifier` → boil-off |
| R3 | In-Situ Tank Fabrication | `access` → depot buildable without lifted parts |
| R3 | Propellant Export | `resource` → propellant enters the space market as a traded good |

### Mobility

| ring | node | effects |
|---|---|---|
| R1 | Transfer Planning | `modifier` → propellant per leg |
| R2 | High-Impulse Engines | `modifier` → propellant tax · `reach` → longer legs |
| R2 | Electric Propulsion | `modifier` → propellant tax for slow cargo · `unlock` → bulk-freight lane class |
| R3 | Nuclear Thermal | `reach` → the outer-body lanes · `modifier` → transit time |
| R3 | Depot Routing | `reach` → refuel mid-route · `intel` → lane freshness |

### Yards

| ring | node | effects |
|---|---|---|
| R1 | Orbital Rendezvous | `unlock` → two-vehicle operations |
| R2 | Modular Assembly | `unlock` → Assembly Plant [designed] |
| R2 | Orbital Port | `unlock` → Orbital Port [designed] · `reach` → the station joins the trade graph |
| R3 | Station Fabrication | `unlock` → habitat/station class · `access` → orbital build sites |
| R3 | On-Orbit Servicing | `modifier` → asset maintenance · `retire` → single-use orbital assets |

### Extraction

| ring | node | effects |
|---|---|---|
| R1 | Regolith Excavation | `unlock` → Surface Extractor [designed] |
| R2 | Metallic Body Working | `unlock` → iron-nickel ore chain · `access` → metallic bodies |
| R2 | Platinum-Group Separation | `unlock` → PGM recipe · `resource` → PGM as a traded good |
| R3 | Deep Regolith Processing | `upgrade` → extractor throughput |
| R3 | Autonomous Mining Rig | `modifier` → workforce scalar [Autonomy Doctrine B] |

### Standing lines (cross every sector, never gate the era)

**L-LOG** deepens the convoy/propellant tax. **L-AUTO** carries the workforce scalar and the power
substrate — and owns the Autonomy Doctrine's second branch. **L-MIL** stays **reserved**: this doc's
own rule is not to enumerate it until the combat system is mapped, and BL-157 (units) is still a stub.

## What this draft deliberately does not do

- **No node count inflation.** ~45 objects, against the worked propellant loop's 25 techs *for one
  quest*. The medium-grain call from the pre-game side applies here too: detail pays where someone
  chooses, and the choosing happens at the four keystones, not along the chains.
- **No new resources.** Every target above is an existing or already-designed resource; propellant is
  the one that still has no `resource_type` value, which is a real blocker for the Volatiles sector.
- **No Era 2 hook.** The Era event mechanics (§ Resolutions, 2026-07-08) own that, and an Era is a
  *rupture* the player manages, not a door this tree opens.

## Open questions for the review

1. **Are four keystones right, or is one of them a chain?** Autonomy is the weakest — it may be
   an L-AUTO tier rather than a fork.
2. **Does a deed belong to the corporation or the world?** "First footing" reads as a world first
   (only one corporation can have it) or a personal first (each corporation gets its own). World
   firsts create a race; personal firsts create a checklist. Lean: **world** for the four keystone
   deeds, personal for anything smaller.
3. **Do rival corporations see your deeds?** The activity fog says buildings are visible and
   internals are private (BL-068). A landing is arguably visible; a propellant surplus is arguably
   not. This is the first place the tech system touches the discovery model.
4. **Does an unfired deed hide its keystone entirely, or show it locked?** The tech fog says
   unreachable nodes are not rendered at all. A hidden fork is a better surprise; a visible locked
   fork is a better goal.

---

# Red herrings and the rupture — the 1960s tree's danger model (2026-08-05)

> **DRAFT FOR REVIEW.** Ben, 2026-08-05: *can we put in little red herrings that make Era 1 failure
> (WW3) more likely? One obstacle for the player to navigate is the idea that more advanced does not
> mean better. The player must be skilled at avoiding danger, in each dimension of play.*
>
> Sits on **BL-223**'s settled two-rupture model (2026-08-02): a **past averted** near-miss that
> drives starting conditions, and a **future seeded catastrophic event** that ends Era 0 during
> play — *the rupture averted then is not averted this time*. This section designs what the player
> can do to bring that second one on, and how the tech tree tempts them into it.

## The load-bearing half is not the herrings

A red herring with nothing to trigger is flavour. For "more advanced does not mean better" to be a
**skill**, three things must be true, and only the first is about the tech tree:

1. Some attractive techs must carry a cost that is **not on their own stat line**.
2. That cost must accumulate into something the player can **watch** and **act on**.
3. The catastrophe must resolve **deterministically** from it — the standing rule forbids random
   ruptures, and a dice-roll war would make the whole exercise unlearnable anyway.

So the herrings need a quantity. BL-223 already fixed its shape: the deterrence ceiling is *"a
per-nation scalar, NOT a nuclear-equivalent object"*. The same discipline applies here — **two
per-nation scalars with named consumers**, no new world object:

| scalar | meaning | moved by |
|---|---|---|
| **Ceiling** (BL-223's, unchanged) | how much restraint this nation carries — the memory of the rupture that *was* averted | history-ladder outcome at 1960; decays slowly as the memory ages |
| **Alarm** | how threatened this nation feels *by others* | others' **visible** capability, severed trade ties, posture, domestic instability |

**The rupture check.** The event arrives on its seeded date (the § Resolutions model, unchanged, with
the visible countdown). What the date decides is *when it is tested*, not the outcome: if aggregate
Alarm has risen above aggregate Ceiling, the rupture goes hot and **Era 1 fails** — the Era event's
selective destruction lands on exactly the orbital and heavy-industrial assets the space programme
needed. If it hasn't, it is averted a second time and the Era 1 quest trees open.

Two consequences worth stating plainly. **The player is not the only source of Alarm** — rival corps
and nations raise it too, so a careful player can still lose a world someone else wrecked, and
managing *others'* alarm becomes a legitimate goal. And **Alarm is relievable**: it decays, and
several nodes cut it directly. A pressure that only ever rises is not a skill test, it is a timer.

## The seven kinds of red herring

| kind | the trap | why it tempts | the tell |
|---|---|---|---|
| **Escalator** | genuinely better output, raises Alarm because the capability is dual-use | the numbers are simply good | the node's own dual-use description; rival alarm ticks on completion |
| **Legibility trap** | the capability is fine; being *seen* to hold it is the cost | no downside on the stat line at all | it is a large, visible, single-site facility — rivals' fog tier reads it |
| **Interdependence severer** | autarky and substitution: looks like resilience, cuts the trade ties that hold Alarm down | removes a supply risk you can see, for one you cannot | your own export routes go quiet |
| **Brittle optimisation** | better numbers, catastrophically worse under blockade | strictly dominant in peacetime | the input it optimises is one you import |
| **Contextual dud** | advanced, and wrong *here* — the Martian water mill inverted | it is the newest thing available | its prerequisite substrate is absent on this body |
| **Tempo trap** | a fine tech that costs you the clock | it is safe, useful, and cheap | the countdown is visible and it does not move |
| **Domestic destabiliser** | output up, unrest up — and unrest is what makes a nation reach for a foreign enemy | the productivity gain is immediate and large | the unrest surface moves the same tick |

**Every trap needs a tell, and the tell must precede commitment.** That is the project's own tone
rule — legible in hindsight, not locally clever — and the difference between a skill test and a
gotcha. A player who reads the tells and takes the tech anyway has made a *decision*; a player who
could not have known has been cheated.

**And one inverse, deliberately.** *Hardened Dispersed Basing* looks aggressive and is **stabilising**
— a survivable second strike removes the use-it-or-lose-it panic that drives first strikes. If every
menacing-looking node were a trap, "menacing" would just become the tell and the lesson would
collapse into a colour-coding exercise. One node has to punish that heuristic.

## A danger in each dimension of play

| dimension | the herring | the skilled play |
|---|---|---|
| **Economy** | Autarkic Substitution — replace imports with synthetics | keep cross-border routes live; trade interdependence is the cheapest Alarm suppressant in the game |
| **Logistics** | Single-Corridor Efficiency — route everything through the cheapest chokepoint | pay for redundancy; a chokepoint is also a casus belli |
| **Military** | Fissile Enrichment, Ballistic Lift | hold **capacity** without **posture** — the ladder's own artifact-vs-capacity split, now a diplomatic instrument |
| **Information** | Integrated Air Defence — defensive, reads as war preparation | disclosure and inspection: voluntarily legible capability alarms less than concealed capability |
| **Institutional** | Total Automation — workforce collapse into unrest | pace it against the Labour Doctrine you took; unrest is a foreign-policy input, not just a domestic cost |
| **Space (Era 1 prep)** | **Heavy Ballistic Lift** — the same stack that reaches orbit is a missile | the Lift Doctrine fork, plus Open Launch Inspection: choose *how* you reach orbit, and let others watch |

The space row is the one that makes the whole design work, because it is **unavoidable**. You cannot
reach Era 1 without lift, and lift is what frightens everyone. The player's job is not to dodge the
dangerous tech — it is to buy the reassurance that lets them hold it.

## Draft nodes for the 1960s tree

> Node names and placement are Ben's call, per the Era 1 draft above. Effects use the
> `TECH_EFFECTS.md` vocabulary; `alarm` and `ceiling` are shown as `modifier` targets.

### E0-HEAVY — Heavy Industry

| node | kind | effects |
|---|---|---|
| **Fissile Enrichment** | escalator | `unlock` → fission power recipe · `modifier` → own alarm contribution **↑↑** |
| **Autarkic Substitution** | severer | `unlock` → synthetic substitutes for imported inputs · `modifier` → import exposure ↓, rivals' alarm ↑ |
| **Continuous Casting** | *clean* | `upgrade` → steel throughput. No hidden cost — the tree must contain plain goods, or the herrings are just a tax |

### E0-ELEC — Electronics & Computing

| node | kind | effects |
|---|---|---|
| **Integrated Air Defence** | legibility trap | `unlock` → defensive coverage · `modifier` → rivals' alarm ↑ (defence is unreadable from outside) |
| **Automated Launch Authority** | escalator | `modifier` → response time ↓↓, rivals' alarm ↑↑, **rupture severity ↑** if it fires — the archetype of advanced-and-worse |
| **Civil Telemetry Network** | *mitigation* | `intel` → shared observation · `modifier` → **all** alarm ↓. The node that buys the era |

### E0-ROCKET — Rocketry

| node | kind | effects |
|---|---|---|
| **Heavy Ballistic Lift** | escalator, unavoidable | `unlock` → the Era 1 lift path · `modifier` → rivals' alarm ↑↑ |
| **Open Launch Inspection** | *mitigation* | `intel` → your launch schedule is public · `modifier` → rivals' alarm ↓↓ · `modifier` → your tempo ↓ |
| **Hardened Dispersed Basing** | **inverse herring** | looks aggressive; `modifier` → rivals' alarm **↓** (a survivable force is not a panicked one) |

### Standing lines

| node | kind | effects |
|---|---|---|
| **Total Automation** (L-AUTO) | destabiliser | `modifier` → workforce scalar ↓↓, unrest ↑, own alarm ↑ |
| **Single-Corridor Efficiency** (L-LOG) | brittle | `modifier` → haul cost ↓↓ · fails hard under blockade; the corridor becomes a claim |
| L-MIL | **reserved** | not enumerated — BL-157 is still a stub and this doc's own rule holds |

## What this must not become

- **Not a morality meter.** Alarm is other nations' *reading* of you, not a judgement. A nation that
  disarms and is read as weak has not become safe.
- **Not unwinnable.** Alarm decays, three named nodes cut it, and trade suppresses it continuously.
  If a playtest finds the rupture unavoidable, the mitigations are underpowered — that is a tuning
  fault, not a lesson.
- **Not random.** Seeded date, deterministic threshold, visible countdown. The player loses to a
  choice, and can see which choice it was afterwards.
- **Not one-dimensional.** If Alarm ends up driven 90% by military nodes, the "each dimension"
  requirement has failed and the economic and information rows need re-weighting.

## Open questions

1. **Is Alarm per-nation-pair or per-nation?** Pairwise is truer (you can frighten one neighbour and
   reassure another) and costs N² state. Lean: **per-nation**, with the pairwise version deferred —
   BL-223's precedent is a plain per-nation scalar.
2. **Does the player see rivals' Alarm, or only their own contribution?** The discovery model would
   say: your own contribution always, rivals' level only where your intelligence reaches.
3. **Can the rupture be a partial failure?** A limited war that wrecks one region's industry is more
   interesting than a binary, and matches the Era event's "selectively destroys" clause.
4. **Does Era 1 failure end the campaign or delay it?** Lean: delay, expensively. Losing the space
   programme to a war you helped cause is a better story than a game-over screen.
