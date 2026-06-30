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
