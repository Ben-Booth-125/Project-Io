# Project Io — Weekly Sprints

A lightweight weekly rhythm layered over the backlog/Delivery system: a **goal** stated at the
start of the week, a **retro** at the end comparing what landed against it. This is feedback *for
Ben* — pacing and priority signal, not a new authority (backlog.json/REFINED.md/DEVLOG stay the
source of truth for what's actually true about an item).

Entries are newest-first, one per **sprint** — a sprint is a themed span of work, not a fixed
calendar week; it closes when its goal is settled (landed or deliberately descoped), not on a
clock. A gap with no entry means no sprint goal was set — that's fine, skip it rather than
backfilling.

## Format

```
## Sprint N — theme (opened YYYY-MM-DD)

**Goal.** 1-2 sentences: the outcome this sprint is aiming for, referencing backlog item ids
and/or a version goal (v0.1.1 etc.).

**Planned.** BL-ids targeted, one line each.

**Retro** (filled in at close).
- Landed: ...
- Slipped: ... (+ why — scope grew, blocked on a dependency, deprioritized)
- Runtime: total session time this sprint (sum of DEVLOG Runtime lines) vs. items delivered —
  the pacing signal Ben asked to track (tools/session/timer.js).
- Feedback: anything worth Ben knowing about how the sprint actually went — a pattern, a
  misjudged estimate, a design call that needed more/less discussion than expected.
```

---

## Where things stand (updated 2026-08-10)

| Sprint | Theme | State |
|---|---|---|
| 1 | Procedural generation v1 | **Closed** — goal met (food cluster landed 2026-08-02, see amendment) |
| 2a | Close out the v0.1.0 cut set | **Closed** — all four planned items landed |
| 2b | BL-210 oral-history pivot (nations/corps rewrite) | **Closed** — all four rungs built (BL-217, BL-208, BL-218, BL-219) |
| 3 | Corp AI stage B + skill harness | **Closed** — BL-203, BL-204 both landed |
| 4 | Communication surface (BL-205 chat log) | **Mostly landed** — slice 1 (window, channels, agency feed) complete 2026-07-26/28; only the C-route remainder (§7 Stage C) stays open, unstaffed |
| 5 | Era −1 history sim, 0–2000 CE (BL-271–275) | **Closed 2026-08-10** — four of five landed (BL-271/272/273/275); BL-274 (era-keyed rosters) and BL-317 (prehistory timelapse) carried to v0.3.0 |
| 6 | The release sprint | **Closed** — five versions tagged (v0.1.1, v0.1.2, v0.1.8, v0.1.9, v0.1.10); every cut minor now carries a done-definition |
| 7 | The stub minors become releases | **Closed** — v0.1.3 and v0.1.4 cut; `post-v0.1.0` swept, every open item names a minor |
| 8–12 | The next five, planned 2026-08-10 | **Planned** — see § Sprints 8–12 below |

**Next up (2026-08-10).** Sprint 5 is closed on Ben's call — *"I am happy with the generation
progress we made"* — and the board went momentarily to zero goaled sprints. The plan for
**Sprints 8–12** is written below, and it is sequenced against the **2026-08-10 refocus**
(NR-120): the player is a **national private militia** that contracts private companies to build
its space equipment. That narrows BL-094 (player-identity pivot), and it changes what the v0.1.x
tail is *for* — military and space-hardware trade stop being a later layer and become the flavour
the remaining minors are supposed to carry. **NR-102 (sequencing decoupling)** is the standing
structural item this plan is the first answer to: every open item names a minor, but until now
nothing said in what order to build them.

---

# Sprints 8–12 — the plan (written 2026-08-10)

Five sprints, planned in one pass at Ben's direction, because Sprint 5 closed and nothing was
goaled. They are sequenced against one thing: **the 2026-08-10 refocus** (NR-120).

## The refocus, and the three rulings that shape this plan

Ben, 2026-08-10: *"Let's place the player as a national private militia, which uses private
companies to build equipment for space. So the flavour of trading is initially coloured directly
with military use, and space equipment."*

Three calls were put to him before planning, because each one changes the sequence:

1. **The militia REPLACES the governing body.** Not an Era 0 reading of it, not a client of it.
   BL-094 (player-identity pivot) gets **rewritten**, not amended, and the design test the v0.1.x
   band has carried since 2026-08-03 — *"does this reach military as well as economic
   outcomes?"* — is **retired with it**. That test was derived from the governing-body reason
   (law, policy and science as the player's levers). A militia does not legislate. It procures.
2. **Sprint 8 is a design sprint.** No `src/`, no tag. The refocus lands on paper before anything
   is built against it.
3. **BL-340 (processing chain roster) is pulled forward** out of v0.1.13. Ben's word was
   *"initially"* — if trade is coloured by military and space hardware from the first tick, those
   goods have to exist, or the colour is an assertion rather than a mechanic.

### What ruling 1 costs, stated plainly

**Two already-cut minors were built for the previous actor.** v0.1.3 (Laws) and v0.1.4 (Techs)
were cut on 2026-08-10 — hours before the refocus — and both were justified by the governing-body
reason. This is not a call to unpick them: `condition_set` (BL-342) is a generic
predicate object and one enacted law (BL-343) plus one earned tech (BL-344) are working machinery
that does not care who the player is. What changes is **who enacts**. Under a militia, laws are
something the player is *subject to* and lobbies against, not something it passes — which makes
the laws surface an **input** to the player's problem rather than an output of their agency.
Sprint 8 owns that re-read; Sprint 11 owns the surface consequence. **v0.1.11's BL-155 / BL-156 /
BL-186 (laws & tech surfaces) are the most exposed items on the board** and should not be built
before Sprint 8 rules on them.

---

## Sprint 8 — Who the player is (design only, no tag)

**Goal.** Turn the refocus from a steer into a specified actor, so the four sprints after it are
sequenced against something real. Nothing in `src/`.

**Planned.**
- **BL-094 — rewrite.** Retitle off "governing body". Settle the three questions NR-120 records:
  is the militia one of the ~43 generated nations or an entity attached to one; does the
  shared-treasury call survive companies becoming counterparties (it probably does not — an
  arm's-length supplier with a price and a possible refusal is not a second wallet); and what
  replaces the retired design test.
- **BL-315 — design.** Currently `design-owed`, priority A, and its title still names the
  superseded framing. The conflict spine is the militia's *whole reason to exist*, so this stops
  being a follow-on and becomes the item.
- **File the procurement seam.** The genuinely new mechanic the refocus invents: the militia
  contracts a private company to build equipment. Counterparty, price, lead time, refusal. Nothing
  in the backlog covers it — the closest is BL-280 (negotiated tax rate), which is the same
  *bargaining* shape pointed at a different object and should be read alongside it.
- **Reconcile the v0.3.0 roster.** 22 open items, every one specified against the old actor. Cut,
  re-goal or re-read each. This is the bulk of the sprint's labour and the reason it needs one.
- **Propagate the three opened docs.** CONCEPT.md, SYSTEMS.md and GLOSSARY.md were opened to the
  governing-body framing on 2026-08-04 (NR-053) ahead of the work, on Ben's instruction. They now
  state something superseded. Correct them **as dated notes**, not a rewrite.

**Done when** BL-094 reads as the militia, BL-315 is `designed`, the procurement seam is filed,
and no open item still names an actor that does not exist.

**Risk.** A design sprint with no tag, immediately after a sprint that cut seven. If it starts
sprawling, the fallback is to timebox BL-094 + BL-315 and let the v0.3.0 reconciliation run as a
background sweep.

**Retro (closed 2026-08-10, same day).**

- **BL-094 — rewritten in full.** Title, short_name (`PLAYER_MILITIA_PIVOT`) and summary replaced;
  a dated `## 2026-08-10 — REWRITE` section added to `design`, kept alongside (not deleting) the
  2026-07-04/2026-08-03–07 history so the record of *how* the pivot was reached survives. All
  three of NR-120's open questions resolved: the militia is **attached to** one of the ~43 nations,
  not itself one of them; the shared treasury is **retracted** — companies are counterparties on
  their own market, not a linked wallet; the retired design test is replaced by **"does this
  change what the militia can FIELD, or what it must ANSWER TO?"**. `status` held at `designed` —
  it already was, and the rewrite does not reopen the design-owed question, it replaces the answer.
- **BL-315 — rephrased, deliberately NOT flipped to `designed`.** Title and summary drop the
  superseded framing; the law→military-reach strand is reversed in direction (the militia is
  *subject* to conscription/embargo/basing rights, not the body that enacts them). But real design
  work — the actual force-command verbs, the procurement→field pipeline once BL-350 exists — is
  still owed, same as before the refocus. Flipping it to `designed` here would have been the
  bookkeeping error Sprint 2a's retro warned against (items reading `landed` when they weren't).
  Stays `design-owed`, priority A.
- **BL-350 filed — the procurement/contract seam.** New item, `design-owed`, priority A, requires
  BL-094, version goal **v0.1.14**. Counterparty / price / lead time / refusal, read alongside
  BL-280 (negotiated tax rate) as the same bargaining shape pointed at a different counterparty.
  Four concrete open questions recorded on the item rather than guessed at (does the treasury debit
  on order or delivery; is refusal a hard block or a penalty; does reputation persist; how this
  reads against BL-037's preferred-seller routing).
- **v0.3.0 roster reconciled — cheaper than planned.** Grepped all 21 remaining open items for
  stale "governing" language rather than reading each one cold. Only **two** needed touching:
  **BL-333** (nuclear arc, one sentence redirected to the militia's home nation) and **BL-182**
  (corporate borders, a cross-reference note to BL-350 — this item turned out to be about the *same
  companies* the militia now contracts with, so it survives the refocus and gets more relevant, not
  less). The other 19 — the whole Era −1 sandbox tail plus BL-087 and BL-314 — were already
  actor-agnostic: background-nation history, a generic tech-gate model, a generic unit-verb family.
  The map artifact's forecast ("good news for design") held.
- **The three authority docs corrected as dated notes, not rewritten.** CONCEPT.md (4 notes),
  SYSTEMS.md (7 notes, the densest of the three — it carried the sharpest governing-body
  statements, including the "budget is the converter" framing that needed no change and the
  "shared treasury" assumption that did), GLOSSARY.md (Corporation repointed; **Governing body**
  entry replaced outright by **Militia**, kept dated so the supersession is legible rather than
  silently vanished).
- **Feedback: the sprint's own risk did not land.** The fallback (timebox BL-094+BL-315, leave the
  roster as a background sweep) was never needed — grepping for the stale term before reading each
  item cold turned a feared 22-item slog into a 20-minute check. Worth remembering next time a
  "reconcile the roster" task looks large: search first, read only what the search flags.

## Sprint 9 — The militia takes the field (cut v0.1.5)

**Goal.** The first buildable consequence of the refocus, and the band's next tag.

**Planned.** **BL-325** (military bases: a muster building, hire moved onto it, unit supply read
off it) and **BL-331** (player starts with a base and one unit) — both `designed`, both buildable
today, and both literally the militia's first surface. **BL-332** (military points + a research
building) moves here from v0.1.11: it is `design-owed`, it answers *how does the militia get
better*, and it belongs with the military minor rather than the laws one.

**Why this before procurement.** The militia has to exist on the map before there is any point
modelling what it buys.

## Sprint 10 — Procurement, and the goods it is about (new minor)

**Goal.** The refocus's actual mechanic, plus the resource tiers that make "space equipment" a
thing rather than a label.

**Planned.** Sprint 8's filed procurement items, and **BL-340** (the processing half of the
buildings roster — new resource types with market, price and recipe) pulled forward per ruling 3.
Needs a minor of its own; **v0.1.14 "Procurement"** is the obvious name, to be confirmed at the
cut. v0.1.13 keeps markets and the save-format header without its keystone, which is fine — those
five items stand on their own.

**Dependency note.** BL-340 is difficulty 4 and `design-owed`. It is the single largest unknown in
this plan and the most likely thing to spill into Sprint 11.

## Sprint 11 — v0.1.11 reconciled (cut v0.1.11)

**Goal.** The fattest open minor, built against the actor Sprint 8 defined rather than the one it
was written for.

**Planned.** The laws and tech surfaces (**BL-155**, **BL-156**, **BL-186**) *as reframed* —
under a militia these are constraints the player operates inside, which is a different surface
from an enactment ledger. Then the items the refocus does not touch: **BL-211** (history ledger),
**BL-212** (nation-voiced comms), **BL-309** (deed lines), **BL-264** (wizard layout after fold),
**BL-341** (the parked Windows cold-configure check). **BL-280** (negotiated tax rate,
`design-owed`, difficulty 5) is the one to watch — Sprint 8 will likely have merged its bargaining
model into the procurement seam, in which case it shrinks or dissolves.

## Sprint 12 — Generation visibility, and the owed timelapse (cut v0.1.7)

**Goal.** Close the generation arc Ben declared himself happy with, including the one piece he
named as still owed.

**Planned.** **BL-303** (Generation Ledger), **BL-304** (field-overlay lenses), **BL-305**
(political-step visibility), and **BL-098** (the UX review walking the whole band against
`user_stories.json`) — which is the right last act, since by then five sprints of surfaces will
have accumulated. Plus **BL-317** (the New World wizard's prehistory timelapse), pulled back from
v0.3.0: it is the history time-lapse Ben named at Sprint 5's close, and it is a
generation-visibility item by nature rather than an Era −1 one.

**One sequencing option worth taking.** BL-317 pairs naturally with **BL-264** (wizard layout
after per-stage folding) — both are New World wizard stages. If Sprint 11 is building BL-264
anyway, riding BL-317 alongside it gets the timelapse three sprints earlier for very little extra,
and Sprint 12 loses nothing it needs.

---

## What this plan deliberately does not carry

- **v0.2.0 (the AI opponent), 12 items.** It is the thing that makes Io a game rather than a
  simulation, and it is *not* in the next five sprints. That is a real cost, accepted because the
  opponent should be built against a settled player identity, not re-fitted to one. It is the
  obvious Sprint 13.
- **v0.1.12 (logistics modes), 4 items** — rail, ports, convoy distance pricing, supply-lens
  flow. Unblocked and uncontroversial; a good filler if any sprint above finishes short.
- **v0.4.0, 9 items.** Politics, and the history-ladder tail. Downstream of everything here.

**A standing caution from Sprint 6, applied to this plan.** Sprint 6's retro found that
*measurement overturned the stated cause four times*, and that three of five worktree agents
branched from a base that had already moved. Both lessons bear on a five-sprint plan written in
advance: **it will be wrong somewhere, and the honest move is to amend it when the goal changes,
not at the retro** — which is the exact failure Sprint 1's retro named and this file has now
committed twice.

---

> **Numbering note (2026-08-02).** Two entries below were both opened as "Sprint 2" on
> 2026-07-31 — the v0.1.0 cut set and the BL-210 decomposition. They are kept under their
> original headings (renamed 2a / 2b) rather than renumbered, since commit messages
> ("Sprint 2 promotion: BL-217") already point at 2b.

## Sprint 2a — Close out the v0.1.0 cut set (opened 2026-07-31, closed 2026-08-02)

*(Goal set by the assistant during the 2026-07-31 doc sweep — Ben to amend.)*

**Goal.** Land the remaining v0.1.0-goal items so the prototype cut is unblocked: the build
surface finished, terrain's combat consequence in, the tooltip text rendering clean.

**Planned.**
- BL-162 — tile construction panel: the per-candidate expected-profit chart (the one owed half)
- BL-233 — terrain combat modifiers (measurement landed 2026-07-31; adoption decision + wiring)
- BL-234 — font glyph range (defect: `fonts.cpp` atlas misses U+2014/U+2265; 26 strings show "?")
- BL-226 — continent lens: finish the settled option-B follow-on (still open, `designed`)

**Retro** (closed 2026-08-02).
- **Landed: all four.** BL-162 (tile construction panel), BL-233 (terrain combat modifiers),
  BL-234 (font glyph range) and BL-226 (continent lens) are all `complete` in `backlog.json`.
  The goal — unblock the v0.1.0 cut — was met in full.
- **Landed beyond plan:** the three v0.1.0 audit instruments (frame-budget HUD BL-249,
  econ-tick scaling, data-creep BL-251), the harness build-type/ctest-timeout fix (BL-255),
  the planetary canvas cull + cache (BL-268), and the action dictionary (BL-270, v0.1.1).
- **Slipped:** nothing from the plan.
- **Runtime:** not summed — the timer gap carried from Sprint 1 is still unfixed.
- **Feedback:** a four-item sprint with every item already `designed` closed cleanly and
  absorbed six unplanned items on top. The pattern holds: sprints scoped to promote-ready
  work land; sprints scoped to a theme (Sprint 1) drift.

---

## Sprint 5 — Era −1 history sim (opened 2026-08-02, closed 2026-08-10)

**Goal.** Build the 0–2000 CE settlement/mil-sim sandbox (BL-271–275) that proves out the nation
AI and mil-sim architecture, and tunes the campaign's non-hegemony premise against measured
distributions rather than lore. Bot-only, behind a harness flag — never in the shipped campaign
path (BL-271's stated bound).

**Dependency chain.** BL-272 (combat model) and BL-273 (province demography) have no unmet
dependencies (BL-233 and BL-218 both already landed) and touch disjoint files — **foundation
wave, run in parallel**. BL-274 (era-keyed rosters) needs BL-272. BL-271 (the sim loop) needs all
three. BL-275 (the seed sweep) needs BL-271.

**Planned.**
- BL-272 — unit/doctrine combat engine (foundation wave) — **complete 2026-08-02**
- BL-273 — province demography + manpower (foundation wave) — **complete 2026-08-02**
- BL-274 — era-keyed unit rosters (wave 2, needs BL-272 — dependency now satisfied)
- BL-271 — the year-tick sim loop (wave 3, needs BL-272/273/274)
- BL-275 — the seed-spread sweep (wave 4, needs BL-271)

**A detour, recorded not repeated.** This sprint opened mid-session with a design pass toward a
different item, BL-205's C-route (in-character LLM chat) — it went as far as a full in-process
API-call design before Ben clarified that wasn't the intent (human-in-the-loop play via
computer-use, not a shipped API integration). Reverted cleanly; see NEEDS_REVIEW.json NR-039/NR-040.

**Retro** (closed 2026-08-10 on Ben's call — *"I am happy with the generation progress we made"*).

- **Landed: four of five.** BL-272 (unit/doctrine combat model), BL-273 (province demography +
  manpower), BL-271 (the year-tick sim loop) and BL-275 (the seed-spread sweep) are all
  `complete`. The stated goal — a bot-only 0–2000 CE sandbox that tunes the non-hegemony premise
  against *measured distributions rather than lore* — was met, and BL-275 is the item that met
  it: the sweep is the measurement.
- **Carried, not slipped: two.** **BL-274** (era-keyed unit rosters) and **BL-317** (the New World
  wizard's prehistory timelapse) are both still `designed`, and both now carry **v0.3.0** — they
  moved with the rest of the Era −1 arc when NR-101 swept `post-v0.1.0`. BL-317 is the "history
  time-lapse still to do" Ben named at close; it is a *presentation* of a sim that already runs,
  not a hole in the sim.
- **The closing call is a scope judgement, not a completion claim.** Sprint 5 is closed with its
  architecture proven and its presentation owed. That is the right trade — the timelapse's whole
  value is showing a player the history, and there is no player-facing wizard stage to show it in
  until the v0.3.0 arc is underway.
- **Runtime:** not summed; the timer gap carried from Sprint 1 was never fixed and is now three
  sprints stale. Either fix it or stop naming it in the format block.
- **Feedback: this sprint proved the pattern the release sprint later exploited.** The foundation
  wave (BL-272 + BL-273) was picked *because* the two items had no unmet dependencies and touched
  disjoint files, and it landed same-day in parallel. Sprint 6's worktree fan-out is the same move
  at larger scale — and its failure mode (three of five agents branching from a moved base) is
  what happens when the dependency read is skipped rather than done. The read is the cheap part.

---

## Sprint 1 — Procedural generation v1 (opened 2026-07-21)

**Goal.** Take procedural generation to a v1 worth building on — not just the immediate
rivers/food cluster, but a foundation the rest of generation (deposits, climate, market/pop
co-gen) can extend cleanly rather than fight later.

**Planned — this sprint's build (settled, in progress).**
- BL-170 — rivers as an edge feature + logistics discount
- BL-166 — Hydroponics Bay (habitability/terrain-gated food source)
- BL-168 — Fishing Wharf (coastal-adjacency food source)
- BL-190 — food demand model (settled: plain market pull, no starvation)

**Filed this sprint, not yet in scope to build.**
- BL-188 / BL-189 — coastal ports + coastal defense (design-owed, spun off BL-168)

**Design-settled this sprint (surveyed list, widened per Ben's "finalize v1" framing).**
- BL-040 — resource generation full-set deposits: **corrected**, already shipped (backlog was
  just stale bookkeeping).
- BL-051 — tile generation refinements: **settled** — the sibling-pass pipeline-shape convention
  now standard for all future generation extensions.
- BL-132 — market/population co-generation: **settled** — blocker cleared, sequencing fixed.
- BL-167 — **reframed** as Planetology (atmosphere/chemistry/evolution history,
  Shadow-Empire-inspired), un-parked, raised to priority B. New authority doc
  `docs/generation/PLANETOLOGY.md` — design-owed on its own detail (chemistry fidelity, evolution
  abstraction, presentation surface), a likely Sprint 2 candidate.

**R&D pass — BL-167 Planetology (2026-07-21, Ben's ask).**
Ben asked for a rough chemical understanding of how generation gets from (A) an input solar system
to (B) life to (C) civilisation, always human and always carbon-based. Researched across 13 domains
with an adversarial fact-check per domain, then synthesised into `PLANETOLOGY.md` § The chain: ten
gated stages, each a threshold test rather than a simulation, each emitting one dated timeline line.
The **B→C joint is now concrete** — a biosphere physically manufactures the industrial base (banded
iron, petroleum, coal, bauxite, supergene copper, soil), so "no life, no coal" is a mechanism, not a
label. The **homeworld rule** is settled in shape: constrain the *inputs*, never the gates or the
outputs. **11 open calls are recorded and are Ben's** — the two load-bearing ones being the
dead-code problem (at four hand-authored bodies the chain produces one interesting world, so most of
the ladder is unreachable until `body_count` is activated) and `deposit_scalar` ownership vs BL-114.

**BL-167 then BUILT the same day, and the dead-code call resolved.** Ben: *"One planet with life is
much better for what I'm imagining"* — the unreachable rungs are the intended shape, not a gap. The
chain shipped as a sibling pass deriving each `body_profile` (23 of 24 fields reproduce the authored
values; Kepler bit-identical), and the one-shot flow became a **ten-stage New World wizard** — one
decision per stage, charted in the tile-selection idiom — on Ben's follow-up: *"I would prefer that
the user sees each stage and chooses at each stage."* So BL-167 is **complete**, not Sprint 2 input.
Sprint 1's goal (procgen to a v1 worth building on) is now materially exceeded on the generation
axis; the rivers/food cluster (BL-166/168/170) is still the outstanding half.
- BL-169 (solar geometry feasibility) — not touched; a different axis (system layout, not planet
  generation), left for its own pass.

**Retro** (closed 2026-07-31 — *written by the assistant during the doc sweep, not by Ben;
factual from git log + backlog.json, amend freely*).
- **Slipped: the entire planned build set.** BL-166 (hydroponics), BL-168 (fishing wharf),
  BL-170 (rivers) and BL-190 (food demand) were all re-goaled to **post-v0.1.0** in the backlog
  without a closing entry here — deprioritised when the v0.1.0 cut set took over, not blocked.
- **Landed instead** (the window's actual output, 2026-07-21 → 2026-07-31): the BL-167 Planetology
  chain + New World wizard (noted above); the v0.1.0 legibility batch (BL-174/176/177/178/179);
  corp AI stage A + blackboard + persona counsel (BL-202/206/207); the Selection band move
  (BL-213); the history ledger (BL-211); comms dock (BL-227); hover glance-then-stick (BL-230,
  retiring BL-228's freeze model); continent lens (BL-226, follow-on still open); dated history
  timestamps (BL-220); pre-national ladder (BL-221); landform render + spanning markers
  (BL-231/232); terrain-combat measurement (BL-233, ladder untouched). Plus three bulk golden
  re-blesses and the BL-220..225 history-ladder filing.
- **Runtime:** not summed — the wall-clock timer gap noted below was never fixed, so the pacing
  signal for this sprint is unreliable; treat it as missing rather than compute a bad number.
- **Feedback:** the sprint goal named one theme (procgen v1) and the work delivered another
  (v0.1.0 legibility + AI + history). The generation half was genuinely exceeded (BL-167), but
  the rivers/food cluster — the stated "outstanding half" — quietly left the version goal without
  this file noticing. That is the exact drift a close-out entry exists to catch; close the sprint
  when the goal changes, not just when it lands.
- *(Carried note: session Runtime tracking hit a real gap this sprint — the wall-clock timer
  conflated an idle/interruption gap with work time; worth fixing before relying on it.)*

**Amendment (2026-08-02) — the slipped set landed after all.** The retro above was written on
2026-07-31 while the cluster sat at post-v0.1.0. It was re-promoted the next working day and
built: BL-170 (rivers & freshwater, incl. the directed-river render + chevrons), BL-166
(Hydroponics Bay), BL-168 (Fishing Wharf, plus a construction-ledger visibility fix) and
BL-190 (food demand) are all `complete`; the full grid goldens were re-blessed after river
generation shifted the world. Sprint 1's goal is therefore **met**, not slipped — the
deprioritisation was a ten-day deferral, not a drop. The feedback point still stands: this
file did not notice the version-goal change in either direction.

---

## Sprint 2b — BL-210 decomposition → the Nations/Corporations history rewrite (opened 2026-07-31)

**Status 2026-08-02: IN PROGRESS** — decomposition done, and the chain's first two rungs built.

**Goal.** Take BL-210's umbrella pivot (design-owed, difficulty 5, "the biggest remaining piece"
per DEVLOG 2026-07-28) from one unactionable mega-item to a sequenced set of promote-ready design
passes — this is also the first sprint to deliberately practice the Design depth verb
(DELIVERY.md § Depth verbs) rather than jumping straight to code.

**Why this over the v0.1.2–v0.1.5 stubs.** Ben's ask was to decompose the diplomacy/war/tech/legal
placeholders (BL-155/156/157/158). Investigation first: those four are all `design-owed` against
the *current* nation model (Voronoi BFS + a random politics draw), and BL-210 is already
mid-rewrite of exactly that model into a simulated settlement/industrialisation history. Designing
Laws/Politics against nations that are about to stop being Voronoi blobs risks rework. Ben chose
(2026-07-31) to sequence BL-210 first; the four stubs resume once nations have real history to hang
policy/military/politics off.

**Planned — this sprint's decomposition (done 2026-07-31).**
- Split BL-210's three still-open subject-doc passes into their own sequenced items, since a
  difficulty-5 item should break down (DELIVERY.md § Priority, difficulty & version goal) rather
  than stay flat:
  - **BL-217** — checkpoint/branch data model (where branch state lives; how a lean biases which
    branch fires at a checkpoint, extending BL-167's preferences mechanism). No dependency beyond
    the already-shipped BL-167. Difficulty 3.
  - **BL-218** — Nations rewrite: territory/political-character/wealth as *outputs* of a simulated
    settlement → industrialisation → 1900s history, replacing Voronoi BFS + random politics draw.
    Requires BL-217. DEVLOG's named "biggest remaining piece". Difficulty 4.
  - **BL-219** — Corporations rewrite: industrial focus as an emergent consequence of the home
    nation's settlement history, replacing the authored focus table. Requires BL-218. Difficulty 3.
  - BL-210 itself stays open as the umbrella closing condition (all three built, plus the
    batch-sweep extension and authority-doc propagation into TILE_GENERATION/NATION_GENERATION/
    CORPORATION_GENERATION).

**Not yet in scope this sprint.** Actually *settling* BL-217/218/219's open questions (each still
needs a real design conversation — see each item's `design` field) or touching any `src/`. This
sprint's goal was decomposition only (the Design depth verb's first half); running BL-217's design
conversation is the natural Sprint 3 candidate once Ben is ready to work through its open questions.

**Progress (2026-08-02).** The "not yet in scope" line above was overtaken — BL-217's design
questions were settled and the item **built**, not just designed.
- **BL-217 (checkpoint/branch model) — complete.** Append-only `checkpoint_record` + a generic
  eligibility-filter mechanism in `planetology.{hpp,cpp}`; R1–R6 met, CTest 37/37.
- **BL-208 (world history log) — complete**, resequenced ahead of BL-218/219 because both need
  somewhere to write their history. The project's first flat-binary serialiser (BL-107's seam);
  the genesis + checkpoint chapters bridge generation output into `world` for the first time.
- **BL-218 (nations settlement rewrite) — not promoted.** Still `designed`; the strict chain
  means it starts once BL-208 is in `main` (it now is).
- **BL-219 (corporations history rewrite) — not promoted.** Depends on BL-218.
- **BL-210** stays open as the umbrella closing condition.

**Progress (2026-08-02, later the same day) — the chain's last two rungs landed.** Ben's steer:
"complete 2b, and finish with the backend of history implementation… as long as there is a way to
map belief systems onto existing and warring civilisations", and explicitly "don't be afraid to
have parts of the record erased when two nations go to war."

- **BL-218 (nations settlement rewrite) — complete.** `src/world/settlement.{hpp,cpp}`: the
  province becomes the unit that carries belief, ancient endowment and industrial timing at once.
  Nation seeds are now province anchors (seeding changes, expansion does not); the three political
  axes are derived outputs; the historical ruptures are BL-217's second checkpoint class, with
  collapse/war/revolution as real transforms.
- **BL-219 (corporations history rewrite) — complete.** Focus derives from the corp's home
  province plus the movement up the value chain; the authored table is retired and diversity
  becomes a world-level reject-and-reroll.
- **The erasure landed as asked.** A won war plants the victor's pantheon on the provinces taken
  and destroys the lines naming them, leaving a dated lacuna with a count. Four of six seeds lost
  part of their record.
- Verified by a new `settlement_harness` (S1–S8); full CTest **39/39**, which needed two harness
  follow-ons (`history_ladder_harness` H4 narrowed, `ai_skill_harness` MSVC goldens re-blessed —
  every divergence upward) plus one unrelated pre-existing fix (`trade_routes_harness` had not
  linked since BL-170 landed rivers).

**Retro** (closed 2026-08-02).

- **Landed against the goal: the whole chain.** BL-217 → BL-208 → BL-218 → BL-219 are all
  `complete`, and the authority-doc propagation went with them rather than being deferred.
  BL-210's umbrella is down to its batch-sweep extension and TILE_GENERATION.md's share.
- **What the decomposition bought.** The sprint's stated goal was decomposition *only* — "not yet
  in scope: actually settling BL-217/218/219's open questions or touching any `src/`". All of that
  happened anyway, and cheaply, because the decomposition had already named the real dependency
  order. The design passes settled in one session each because they were asking answerable
  questions.
- **Feedback: the "not yet in scope" line was overtaken twice.** Once on 2026-08-02 morning
  (BL-217/BL-208 built) and again the same afternoon (BL-218/BL-219). A sprint whose scope is
  overtaken twice in a day was scoped as a plan when it was really a queue — which is the same
  observation Sprint 3's retro made about numbering being a theme label, not a schedule.
- **Feedback: a whole-world change moves goldens, and the suite says so honestly.** Two harnesses
  moved and both were verified against a stashed pre-change baseline rather than assumed
  pre-existing. Worth keeping as the habit: the cost of checking was one stash and one target
  rebuild, and it turned "probably already broken" into a known, explained divergence.

---

## Sprints 3–5 (committed 2026-07-31) — re-sequenced: skilled NL agents for stress-testing via simulated play

**Ben's steer, same session as Sprint 2's close:** systems get proven by simulating play, so the
next three sprints put agent skill ahead of the BL-217 design conversation Sprint 2 flagged as
"natural next." BL-210/BL-217–219 stay queued, not dropped — they resume after.

**Why this doesn't need new design work (token-sparing).** `AI_OPPONENT.md` already carries a
Ben-accepted architecture (§5, 2026-07-26) and a filed follow-on decomposition (§8): BL-202 (Stage
A scorer, **complete**) → BL-203 (Stage B predictive spending — the "skilled" half) and BL-204 (bot-
vs-bot skill-regression harness — the literal simulated-play stress test) → BL-205 (chat/diplomacy
surface) → BL-207 (persona counsel packs). All four are already `designed` (promote-ready) — this
is execution, not another design pass, which is the cheap path.

### Sprint 3 — Stage B + the skill harness (BL-203, BL-204)

**Goal.** Make the corp AI actually skilled (predictive spending replacing the crude solvency
floor) and give it a regression harness that *is* simulated play: seed-set bot-vs-bot goldens
(solvency, net-worth curves) plus the tick-boundary state hash (doubles as the multiplayer desync
primitive, AI_OPPONENT.md §8). Both require only BL-202 (complete) — no blocking dependency.

**Planned.** BL-203 (diff 4), BL-204 (diff 3). Promote together — BL-204's goldens are the natural
acceptance check for BL-203's behaviour change, so building them in the same batch is cheaper than
sequencing.

**Retro** (closed 2026-07-31 — landed early, ahead of Sprint 2b's chain).
- **Landed: both.** BL-203 (corp AI stage B: strategy layer, priority buckets, predictive
  spending) and BL-204 (AI skill-regression harness: seed-set bot-vs-bot goldens + tick-boundary
  state hash) are both `complete`. BL-235 (creeds) landed alongside, wired into CTest.
- **Feedback:** Sprint 3 overtook Sprint 2b rather than following it — the planned sequence and
  the actual order diverged, which is fine but means "Sprint N" here reads as a *theme label*,
  not a schedule. Treat the numbering that way.

### Sprint 4 — the communication surface (BL-205)

**Goal.** Land the § 7 chat principle: a channel-based message surface (Public + arbitrary groups)
replacing the Explorer placeholder, carrying Stage-A templated decision-log messages now and
becoming the medium personas (BL-207) and, later, a free-text LLM planner (Stage C) speak through.
`designed`, no dependency — could in principle run before Sprint 3, sequenced after only to keep
one theme per sprint.

**Planned.** BL-205 (diff 3).

### Sprint 5 — RE-THEMED (2026-08-02): the Era −1 history sim (BL-271–275)

**Ben's steer (2026-08-02 brainstorm, recorded in the items themselves):** run the "Rome as
sandbox" plan as Sprint 5. Once generation produces a spread of earth-like planets, refine the
worlds' philosophical development by getting tons of examples running 0 CE → 2000 CE — and,
overturning the abstract-war rule, simulated history fights with **real units and real tactics**,
as typed unit types the main era later inherits (BL-272 records the overturn).

**Goal.** The settlement world (BL-218) promoted from a one-shot generation pass to a year-tick
simulation, with a combat engine the 1960 era will share.

**Planned.** BL-271 (Era −1 sim, diff 4) · BL-272 (unit/doctrine combat model, diff 4) ·
BL-273 (province demography, diff 3) · BL-274 (era-keyed unit rosters, diff 3) ·
BL-275 (history sweep distributions, diff 3 — also closes BL-210's last scope item).
Dependency order: 272 → 274, 218 → 273, all → 271, 271 → 275.

**Consumers this unblocks:** BL-054's parked runtime nation-AI half, the BL-155/156
diplomacy/war stubs (sequenced behind the rewrite for exactly this), and BL-223 (averted
rupture), which gets designed against simulated near-ruptures instead of lore.

**The previous Sprint 5 theme (persona audit + Stage C naming) rides along as its original
small scope** — a diff 1–2 reconciliation plus filing one design-owed item — rather than
holding the slot:

**Old goal.** Two threads. First, reconcile a likely stale status: REFINED.md's 2026-07-27 "AI
constituents batch" already logged BL-207 R1–R3 as covered (persona packs C1, loader/runtime C2,
Counsel channel C3, `persona_counsel_harness` C4) and marked the batch **COMPLETE**, but
`backlog.json` still carries BL-207 as `designed`, not `complete` — verify against the harness and
flip the status if the earlier landing checks out, rather than re-building already-shipped work.
Second: file the still-unitemized **Stage C** — the out-of-process LLM planner that speaks
in-character in channels (AI_OPPONENT.md §2 Area C, §7 Stage C) — as a `design-owed` item. This is
the actual "skilled natural language agent" tier Ben is pointing at; A/B (BL-202/203) and the chat
medium (BL-205) are the scaffolding it needs to stand on, which is why it lands last, not first.

**Planned.** BL-207 audit (diff 1–2), file Stage-C item (diff 1, design-owed, no build).

**Not yet in scope.** Actually promoting Sprint 3 into REFINED.md task groups + `requirements.json`
— that collision-mapping step happens at the start of Sprint 3 itself (DELIVERY.md: built fresh at
promotion, not frozen ahead of time), not in this planning pass.

---

## Sprint 7 — the stub minors become releases (2026-08-10)

**A goal was stated this time**, which is the first thing worth recording, because Sprint 6's
opening finding was that the rhythm had lapsed and its direction came from a live steer rather
than a written goal. Ben's brief named the outcome, the *sequence*, and the two standing lessons
to apply: *"THE JOB THIS SESSION — v0.1.3 and v0.1.4, in that order."* BL-342 first, because both
minors consume it. The retro below compares what landed against that.

**Planned.** BL-342 (condition_set evaluator) → BL-343 (laws MVP) → BL-344 (techs MVP), then a
done-definition written **at** each cut. Optional if there was room: BL-348 (province names read
half-native) and BL-349 (a harness that over-asserts). Named as the next structural job, not as
this sprint's: the 42 items on `post-v0.1.0` (NR-101).

### What landed

**Everything planned, plus both optionals, plus the structural job.**

- **Two versions tagged** — `v0.1.3` (Laws) and `v0.1.4` (Techs), each with a done-definition
  written at the cut and each naming its exclusions rather than dropping them (BL-155, BL-186,
  BL-280, BL-156, BL-332 re-targeted to v0.1.11).
- **Five items closed** — BL-342, BL-343, BL-344, BL-348, BL-349. Open items 76 → 71.
- **The gate went 55 → 58 tests, 0 failures**, holding the green it first reached last sprint.
  Three new harnesses (94 assertions between them); one existing harness needed a two-line change.
- **NR-101 swept.** 46 assignments; every open item now names a minor.
- **Eight review entries filed as the work happened** (NR-112–119), one of which Ben ruled the
  same session (NR-119 — v0.1.12 and v0.1.13 stand as named).

### What this sprint is actually worth remembering for

**Twice, the expensive-looking job turned out to be a bookkeeping job — and the two were the same
shape at different scales.**

1. **BL-342 is about thirty lines of switch statement**, and it unblocked two minors that had been
   design-forward for weeks. BL-155 and BL-156 had *independently* settled on the same object — "a
   flat AND-list of atomic conditions" — and neither built it, because each was scoped design-only.
   **The blocker was ownership, not effort.** Nobody owned the thing they both needed.
2. **NR-101 was the same finding at document level.** Twenty of the 45 unversioned items were
   *already assigned* by ROADMAP.md **in prose** — the whole v0.4.0 politics substrate, most of the
   v0.3.0 Era −1 arc. The roadmap and the backlog disagreed, and the disagreement was invisible
   because the prose was not queryable and the query did not read prose. Most of the "work" was
   making the two documents agree.

Sprint 6's lesson was *measure before you fix*. Sprint 7's is adjacent and worth having beside it:
**before estimating a blocked thing, check whether anything is actually blocking it.** In both
cases the answer was no — the item was unowned, or already decided somewhere unqueryable.

**The design test earned its keep, concretely.** BL-094 asks *"does this system reach military as
well as economic outcomes?"* It changed two decisions this sprint at no extra cost: `condition_set`
shipped `military_units` / `military_strength` alongside the six economic subjects, and the tech
gate went on the **Military Base** rather than a smelter. A design test that changes nothing is
decoration; this one changed two things, and the second is the instance that proves the pillar the
pivot cares about.

**Two collisions between the sprint's own decisions, both caught by writing the assertion first.**
BL-342 established that an empty `condition_set` is *true* (the common case for a law). BL-344 then
needed the opposite for a tech — ~130 authored nodes with no gate would have earned themselves on
the first tick. The fix is a separate `earnable` flag: **absence has to be modelled by absence, not
by an empty predicate.** The second collision was structural — the predicate could not live in
`tech_tree.cpp`, because that TU pulls in sol2 and is excluded from the world superset a harness
links. Neither was in the design; both surfaced from writing the check before trusting the default.

### Where the method held, and where it did not

**Held — by not doing the thing.** Sprint 6's worst cost was worktree agents branching from a moved
base (36 conflicts to integrate one commit). This sprint's three items were one dependency chain
across ~2 files each, so they were built sequentially in the main session. That was stated as a
call rather than defaulted into, which is what the method asks.

**Held — the bench prediction paid off exactly.** `econ_stability` and `home_surface_bench` failed
at `-j 4` and passed re-run idle, precisely as the v0.1.9 retro said they would. They assert
absolute wall-clock, so they measure the machine. No time was lost to it because the retro had
already said what to do.

**Did not — the JSON stores got corrupted three times.** Long resolution prose is full of backticks
(`condition_set`, `apply_budget`, `earnable`), and inside a shell heredoc those are command
substitution: the shell silently deletes the word and the mangled text lands in the canonical store
looking almost right. Three repair passes. Fixed durably by writing edit scripts to a file and
running them, and saved to memory rather than re-learned next month.

**Did not — the pacing signal is not being collected.** This is the *third consecutive* entry whose
Runtime line reads "not tracked": `tools/session/timer.js` exists, the SPRINTS format block asks
for the number explicitly, and nobody starts it. The commit span (09:24–09:53) measures the
landing, not the work, so it is not a substitute. **Ben asked for this signal and it has quietly
stopped existing** — either start the timer at session open as a standing step, or drop the Runtime
line from the format so it stops reading as data that was collected.

### Left for another session

- **NR-115** is the one genuinely for Ben: world generation still places starting military bases
  through the tile-only check, so a corp can begin a campaign with a base it has not researched.
  Defensible as fiction (inherited, not researched); a one-line fix either way.
- **NR-102 — sequencing decoupling** is now the standing structural item, and it is the honest
  successor to NR-101: a minor per item is not an order to build them in.
- **v0.1.5, v0.1.6 and v0.1.7 remain uncut**, and v0.1.11 now carries ten items *(nine from
  2026-08-10, when BL-332 moved to v0.1.5)* — the largest open
  minor in the band.
- **Windows work still owed**, unchanged from last sprint: visual goldens stale wherever a body
  name renders, MSVC skill goldens stale for two separate reasons, and BL-341 parked until someone
  is at that machine.

---

## Sprint 6 — the release sprint (2026-08-09 → 08-10)

**No goal was stated at the start**, which is itself the first finding: the sprint rhythm had
lapsed, and this session's direction came from a live steer rather than from a written goal.
Ben, opening: *"cut as many versions as we can now, rather than working on the lofty, conceptual
stuff."* The retro below compares what landed against that steer.

### What the diagnosis found, before any work

119 commits since the `v0.1.0` tag and **not one version cut**, with `CHANGELOG.md`'s
`[Unreleased]` still reading *"Nothing yet"* — so the changelog was not merely un-stamped, it was
not accruing. In the same window the roadmap kept extending *forward*: v1.0.0 named, the Era −1 arc
folded into v0.3.0, stub minors re-sequenced.

The root cause (**NR-103**): the roadmap wrote done-definitions for exactly **two** versions,
v0.1.0 and v1.0.0 — and those were the only two ever cut or scheduled. **A theme with no
done-definition has no test for *finished*, so it absorbs items indefinitely.** v0.1.1 was the
proof: its three theme legs shipped on 2026-08-03, and 26 unrelated items were then retrofitted
onto the same tag over the following week.

### What landed

**Five versions tagged** — `v0.1.1` (the word interface), `v0.1.2` (buildings rework), `v0.1.8`
(build health), `v0.1.9` (shell & legibility), `v0.1.10` (generation & content) — against zero in
the preceding six days.

- **24 backlog items closed**; open items 97 → 77 despite **13 new items filed**.
- **Every cut minor now carries a done-definition**, written at its cut. That was the structural
  fix, not the tags.
- **The gate went from lying to green.** It had been reporting ten failures of which exactly one
  was a failing assertion; v0.1.8 re-tiered it, and v0.1.10 closed with **55 tests, 0 failures** —
  the first fully green gate of the arc.
- **Four stub minors stopped being design-only.** BL-342–345 turn v0.1.3/v0.1.4/v0.1.6 into
  buildable work, on the finding that BL-155 and BL-156 had *both* settled on the same
  `condition_set` object and neither built it.

### What this sprint is actually worth remembering for

**Measurement overturned the stated cause four times**, and each time the plausible story would
have produced a plausible fix:

| Item | The story | What measurement said |
|---|---|---|
| BL-338 wetland | the relief commits displaced it | refuted by rebuilding the audit at `802421c^` — identical census. Wetland is a *drainage* feature and elevation had no vote in composition |
| BL-347 econ tick | an O(n log n) sort | the sort was **3%**; the cost was `std::map` allocation in worlds with no stacks |
| BL-346 estimator | BL-079's reflex reads an inflated number | **retracted** — that function reads *realised* credit; the real site was the pre-build estimator |
| pan "lag" | needs a 200 ms input delay | panning costs **nothing**; static and panning measured identically. It was 157k vertices at whole-grid zoom |

**And a green gate can lie.** `logistics_reach_harness` failed three assertions from *byte-identical
sources*: a conflict-heavy merge left ninja holding a stale object that `touch` would not dislodge.
Standing lesson recorded: `--clean-first` before cutting after a messy merge, and if a harness fails
suspiciously, build the same commit in a throwaway worktree before believing it.

### Where the method held, and where it did not

**Held.** Worktree sub-agents did the bulk of the work and several outperformed their briefs —
one refused its own instructions because BL-216 §1–3 was superseded by a *complete* item and
implementing it would have reverted a landed decision. Another volunteered that a knife-edge test
had influenced its parameter choice (**BL-349**), which is the kind of thing that is invisible
unless someone says it.

**Did not.** Three of five agents in the v0.1.9 batch branched from a base that had already moved,
and every one produced code that would not merge cleanly — worktrees isolate *writes*, not
*history*. The v0.1.10 batch was worse: one agent's commit swallowed a whole fast-forward (328
files) and cost 36 conflicts to integrate. **Integration read every hunk rather than trusting a
clean auto-merge, and that is why the Ages view and the disclosure controls survived.**

### Left for another session

- **v0.1.11** is open with four items: the generation globe (**BL-256**, the one item that wants a
  human watching while it is built), deed history lines (**BL-309**, designed), and this sprint's
  own fallout — **BL-348** (province names read half-native since BL-290) and **BL-349**.
- **v0.1.3–v0.1.7 remain uncut**, and are now the *next* thing in sequence: v0.1.3 and v0.1.4 are
  buildable the moment `condition_set` (BL-342) lands.
- **42 items still sit at `post-v0.1.0`** — NR-101's finding, untouched. That is a third of the open
  backlog with no minor, and it is the next structural job after the stub minors.
- **Windows work is owed**: visual goldens are stale wherever a body name renders, the MSVC skill
  goldens are stale for two separate reasons, and **BL-341** (the from-cold configure check) is
  parked until someone is at that machine.
