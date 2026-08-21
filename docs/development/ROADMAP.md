# Project Io — Roadmap

**This roadmap carries TWO ARCS since 2026-08-12 (NR-177, the 0 CE refocus).** The **ancient
arc** is live: a standalone commercial product set at 0 CE, the mercenary company as the player,
built on the campaign engine as it stands. The **space arc** — everything from v0.2.0 through
v1.0.0 below — is **parked for DLC**: kept intact as a destination, not deleted, per Ben's
ruling (*stash the space work, build ancient as its own product, merge back for DLC*). § The two
arcs, below, is the live map; the space-arc sections carry dated PARKED banners and are
otherwise unrewritten history.

The document remains **forward-facing and lean**: it names the version sequence, the *theme* of
each minor, and the done-definitions for the cuts. It deliberately does **not** enumerate
individual items — that lives in the backlog
([`backlog.json`](backlog.json), metadata + design prose; [`BACKLOG.md`](BACKLOG.md) is a legacy
drain) and the active worklist [`REFINED.md`](REFINED.md). The roadmap sits *above* both: it says which theme each minor
carries; the backlog and worklist say what work realises it.

---

## The two arcs (2026-08-12)

**Why.** Ben, 2026-08-12 (NR-177): *"our project is too grand to be able to really feel out the
gameplay"* — and, on the method: *"when making your ideal game, you should first focus on a
smaller project with similar design."* The ancient product is deliberate practice for the space
game, not an abandonment of it. Four rulings shape everything below: the space work is
**stashed, not re-anchored**; the player is a **mercenary company** — BL-094's militia one era
earlier, the same shape (procure force, field it, be paid), so the 2026-08-10 identity is being
*tested*, not replaced; the grain is **tile and fine tick** — the campaign engine as-is, with
`history_sim` staying the *generator* (stop year moved 1960 → 0 CE); and the release bar is
**commercial** — Steam or itch, paid, polish and content depth required.

### The ancient arc — live

The v0.1.x band continues as the ancient product's band. Already landed under the refocus
(2026-08-12, ahead of this rewrite): the 0 CE epoch default, the 3× map (312×145) with a physical
tile scale and travel time, the Era −1 sim wired into generation (400 pre-epoch years — 62
battles, 16 conquests, ~1100 foundings on the default seed), the stepped decision clock, the
async loading screen, and the startup-hang fix. SPRINTS.md § Sprint 15 is the retro-record.

- **v0.1.15 — The mercenary vertical slice.** *Theme: someone hires you, you fight, you are
  paid.* The loop that makes it a game rather than an economy with a sword icon. Carries
  **BL-377** (mercenary contract seam — a polity pays the company to make a fact about the world
  true), **BL-315** (conflict spine — pulled from v0.3.0 groundwork onto the critical path: a
  mercenary company's core loop is built entirely on Conflict), **BL-378** (the minimap becomes a
  fixed max-zoom render of the player's base), and **BL-398** (persona counsel's ~1 s/tick cost).
  Cut by **Sprint 16**; done-definition owed at the cut, per NR-103.
- **v0.1.16 — The watch.** *(Re-themed 2026-08-19 — the split Sprint 26 owed, executed in the
  version-alignment session. Ben's stated aim: developing meta, and observing AI playing the
  game, a downloaded local model included.)* *Theme: an agent plays, a human watches.* The live
  agent control seam (**BL-412** — `--serve` stops being an exclusive branch; an MCP-attached
  agent plays a rendered session), the spectator god view (**BL-408**), the emergent-strategy
  readout (**BL-411**), disable-presses-under-spectate (**BL-413**), the attention director
  (**BL-410**), the spectate viewpoint default (**BL-418**), spectate verb-family assertions
  (**BL-451**), plus the un-parked client loop (**BL-306**) and token-cost measurement
  (**BL-335**). Cut by **Sprint W1**. The old "Ancient conflict & seams" roster moved to
  v0.1.19–v0.1.22 below, with the ancient tech ladder (BL-296) and the debt floor (BL-443) to
  v0.1.11; Era −1 logistics (BL-316), the shared-currency scorer (BL-318) and the works roster
  (BL-321) had already landed.
- **v0.1.17 — Economy breadth.** *Theme: the chain is the growth track.* Named 2026-08-15 on Ben's
  steer that the economy needs "robust and expansive building options, production methods, and an
  implied growth track that we can refine tech and politics around". Four rulings settle its shape:
  the growth spine is **chain depth** (how far down the production graph a corp can reach gates its
  next building — not a tier ladder, not a tech gate); the roster goes past **20 building types**;
  buildings offer **alternate production methods with real trade-offs**, not upgrades; and it comes
  **before** the military sprint. Carries **BL-428** (the depth spine), **BL-429** (ancient
  roster), **BL-430** (alternate methods), **BL-431** (method/chain UI), **BL-432** (guard harness)
  and **BL-433** (the Launchpad and petroleum chain still shipping inside a 0 CE product). The
  ancient tech ladder (**BL-296**) and the politics stub then *refine* this spine rather than
  replace it — which is the whole reason a second unlock system was rejected. Cut by **Sprint 17**;
  done-definition owed at the cut, per NR-103.
- **v0.1.18 — The economy tells the truth.** *(Defined 2026-08-19 — Ben chose define over
  dissolve; the version had existed in the backlog since the Sprint 25a era while the Sprint-26
  split that owed its ROADMAP entry was dropped, NR-377.)* *Theme: the measured economy
  pathologies, fixed.* Sprint 19 closed goal-not-met; these four are its remainder as diagnoses:
  processing underearns extraction (**BL-436**), the economy mines one resource (**BL-437**),
  co-extraction is invisible (**BL-438**), and intra-catchment distance is free (**BL-465**).
  The benchmark is `ai_skill_harness`'s final net worth, per the Fall arc's amendment 5.
- **v0.1.19 — Ancient conflict & seams.** *(The old v0.1.16 theme, re-homed whole in the
  2026-08-19 split.)* *Theme: the Era −1 machinery graduates from generator to gameplay.*
  Era-keyed rosters (**BL-274**), the Era −1 strategic layer (**BL-277**), the diplomacy seam
  and its battery (**BL-297**, **BL-298**), the great-power seed (**BL-299**), myth & theology
  (**BL-300**), sim perf (**BL-320**) and voice/unit calibration (**BL-337**). Lane A of the
  Fall arc feeds it.
- **v0.1.20 — Stance & force.** *Theme: who may fight whom, and the verbs to do it.* The unit
  verb family (**BL-314**), company answerability (**BL-399**), the stance surface (**BL-449**),
  rivals reasoning about stance (**BL-450**), Logistic Points (**BL-464**), unit formations
  (**BL-472**, moved from v0.1.18 for theme coherence), and the parked diplomacy pair
  (**BL-474**, **BL-475**). Lane C's home; C2/C3 cut toward it.
- **v0.1.21 — The credible rival.** *Theme: the scorer behaves like a competitor, and every verb
  is reachable.* The AI build-score quadratic (**BL-417**), decision-feed rebuild cost
  (**BL-419**), the AI never building processors (**BL-439**), mines targeting only the richest
  tile (**BL-440**), the procurement UI (**BL-445**), and the scorer procuring (**BL-446**) and
  demolishing/roading (**BL-447**). Sprint 22's reachability set lands here.
- **v0.1.22 — Harness truth.** *Theme: the instruments measure the sim that ships.* The Ages
  lazy-sim cost (**BL-425**), the stale-exe gate guard (**BL-426**), the verify world-snapshot
  cache (**BL-427**), harnesses measuring a different sim than ships (**BL-462**), and
  settlement count being seed-invariant (**BL-463**).
- **Still live from the old band, arc-agnostic:** v0.1.5's remainder (BL-325 unit supply decay),
  v0.1.6 (politics stub), v0.1.7 (generation visibility — generation is shared by both arcs),
  v0.1.13's infra tail (BL-107 save format and friends), and the v0.1.2 UI leftovers.
- **v0.1.11 — Policy, tech meta and the nation as an actor. UN-PARKED into the ancient arc
  2026-08-18** (Ben, ruling 5 of NR-331). *Theme: the player is subject to a law, and someone
  enacts it.* The minor was parked with the space arc on 2026-08-12 and never opened; it comes
  back whole — **BL-155** (law/policy surface), **BL-156** (tech system early design), **BL-186**
  (laws ledger), **BL-280** (negotiated tax rate), plus **BL-211** (player-facing history ledger),
  **BL-212** (nation-voiced comms) and **BL-309** (deed history lines). What changes on the way
  back is *who enacts*: under the mercenary player, law is an **input** to the player's problem,
  not an output of their agency. It is Lane D of § Sprints 26–33 in
  [`SPRINTS.md`](SPRINTS.md), and it is legal only because the same ruling set granted
  nation-grain behaviour — see `.claude/rules/io-standing-rules.md`, 2026-08-18.

  **Lane D opened 2026-08-19** (Ben's version-alignment verdicts): Sprints **D1** and **D3** run
  as one batch, carried by two new build items — **BL-479** (tech effect union + the shared
  modifier vocabulary) and **BL-480** (law author + nation treasury; the levy becomes a conserved
  transfer, enactment off the player's ledger). The roster also grew in the same-day split:
  **BL-296** (ancient tech ladder — D2's substrate), **BL-443** (debt floor, the Fall arc's
  cheapest economy-truth unblock), **BL-392** (procurement destroys value — D4's guard),
  **BL-477** (era collapse state defines meta — Ben's paradigm, design-owed) and **BL-478**
  (ancient research spend, extracted from parked BL-087 on Ben's ruling). BL-264 (wizard layout)
  moved out to v0.1.7 — UI alignment, not *who enacts*.
- **Owed, recorded in NR-177 and not yet items:** the mercenary *sell* side beyond BL-377's
  seam (win/lose consequences, reputation), the product's **name** (Io is a moon of Jupiter),
  and the commercial cut's own done-definition.

### The space arc — parked for DLC

**v0.2.0 (AI opponent), v0.3.0 (the militia refocus, minus the Era −1 cluster pulled above),
v0.4.0 (politics + filter), v1.0.0 (the playable space game)** are parked whole: ~41 open items
now carry `parked: true` in the backlog with their version goals intact, so the DLC resume is a
flag-flip plus a re-triage, not an archaeology dig. Their sections below are **unrewritten** —
they are the record of a settled design, and the refocus's own reasoning is that this design is
practised for, not discarded. Do not build against them while parked; do not delete them either.

**v0.1.0 validates the economy loop only** — its scope and exclusions (Conflict, Research, Policy,
Diplomacy beyond a data-model stub) are owned by
[`../tech/TECH_FOUNDATIONS.md`](../tech/TECH_FOUNDATIONS.md). Everything **past v0.1.0** — the
expanded-prototype milestones below — sits *beyond* that scope by design, and is named here as
**direction, not committed scope**: each theme earns its item-level detail (and its own
done-definition) when it reaches promotion. The development constraints and tone that govern *how*
the work is done live in [`DEVELOPMENT_PRACTICES.md`](DEVELOPMENT_PRACTICES.md); this document owns
only the milestones.

---

## Versioning grain

One **minor version** (`0.0.x`) carries **one coherent theme** — a layer, or a cluster of
enablers, taken end-to-end and cut as a release (see `DEVELOPMENT_PRACTICES.md` § Cutting a
release). This is the grain established by v0.0.3 (the whole world-generation spine in one
bump). A minor is not a fixed quantum of work; it is a theme that reads as one thing.

A minor may prove too large and **split in practice** at promotion into the worklist — that is expected,
not a roadmap failure. The plan below is the *intended* shape; the split point is called out
where one is likely.

---

## Where we are — eight minors cut; the player is being redefined

> **Overtaken 2026-08-12 (NR-177):** the section below is the 2026-08-10 status, kept as
> record. The live status is § The two arcs above — the ancient product, Sprint 16, v0.1.15.
> One cut happened between the status date and this banner: **v0.1.14** was tagged 2026-08-11,
> so the latest tag is `v0.1.14`, not the `v0.1.4` the paragraph below names.

*Status 2026-08-10. Latest tag `v0.1.4`. **Eight minors are cut**: v0.1.0 (2026-08-03), then
v0.1.1, v0.1.2, v0.1.8, v0.1.9 (all 2026-08-09) and v0.1.10, v0.1.3, v0.1.4 (all 2026-08-10) —
seven of them in the two days of the release sprint, against zero in the six days before it. Six
minors remain uncut in the band: **v0.1.5, v0.1.6, v0.1.7, v0.1.11, v0.1.12, v0.1.13**, plus the
**v0.1.14** procurement minor named below. **71 open items of 311**, and since NR-101's sweep
**none of them is unversioned**.*

**Two structural facts about this map, both established 2026-08-09/10.** First, **every cut minor
now carries a done-definition written at the cut** — before the release sprint only v0.1.0 and
v1.0.0 had one, and those were the only two ever cut or scheduled, because *a theme with no
done-definition has no test for finished and absorbs items indefinitely* (NR-103). Second, the
numbering is **not the cut order**: v0.1.8/9/10 were cut before v0.1.5/6/7 because build health and
legibility were ready and the stub minors were not. Read the band as a set of named themes, not a
queue.

**The player is being redefined (NR-120, 2026-08-10).** Ben: *"Let's place the player as a
**national private militia**, which uses private companies to build equipment for space. So the
flavour of trading is initially coloured directly with military use, and space equipment."* This
**replaces** the governing-body framing that has themed the band since 2026-08-03 — it is not an
early reading of it. The consequences are worked through in § v0.1.x and § v0.3.0 below, and the
five-sprint sequence that acts on them is [`SPRINTS.md`](SPRINTS.md) § Sprints 8–12.

The interactive economy loop is mechanically complete, legible, and discovery-gated:

- **Layer 0–2** — SDL3 + fixed-timestep simulation + economy tick + sol2; the core data
  model; the Solar / Circumplanetary / Planetary canvases with the minimap zoom ladder.
- **World generation (v0.0.3)** — procedural tiles (two-axis terrain), nations (Voronoi
  BFS countries with weighted sizes and history-pass merges — 14 at v0.0.3, **43** on the
  default seed since BL-221 (pre-national ladder) landed 2026-07-30), and corporations
  (asset placement, financial profile).
- **Layer 3 economy (v0.0.4)** — extraction → processing → per-`(corp, body)` stockpile →
  per-body market with supply/demand **price resolution** → budget; finite **deposit
  depletion**; warm-start; player balance in the header.
- **Layer 4 foundations (v0.0.5)** — placement-rules seam, stability harness, workforce pool
  groundwork, ledger-window chrome; the golden-image visual-verification harness; the full
  **map lens strip** (Corp → Country → Resource → Market → Population → Opportunity →
  Production → Scarcity); multiple market centres per body (tile-centred catchment); world-gen
  fixes (orphan-island claim, lean corp holdings).
- **Improved core-loop (v0.0.6)** — matched price-time **order book**; **saturated substrate**
  (nation-owned background supply/demand filling the world); supply **convoy** routing with
  logistics costs and inter-body price convergence; the Market/Balance/Construction **ledger
  family** + Corp Dashboard; market centre seeding from population centres; warm-start surface.
- **Interactive & legible (v0.0.7)** — player **construction** (placement + resource build
  cost + terrain/deposit validation + slot rules); **recipe and workforce control** per
  building; **population centres** (static MVP + dynamic habitability→workforce feedback);
  **selectable entity markers** (buildings, market centres) with lens-contextual hover cards;
  canvas hit-testing and the Selection panel; the **placement-suitability surface** (armed
  build mode tints affinity tiles); the **lens-driven selection** panel; full **hotkey system**
  with F1 cheat-sheet overlay; balance/market **trend plots** in the Economy panel and Market
  Ledger; app-driven mouse for deterministic verify captures; cross-platform build (Linux
  primary, Windows CI).

The economy is now *interactive and legible*: discovery is gated, competitor intelligence is
appropriately scoped, and the budget is itemised. What remains before the cut is the v0.1.0
terrain strand and the quality audit — see § v0.1.0 below.

**v0.0.8 — Discovery & intelligence (cut 2026-07-04).** Gave the economy its missing strategic
dimension — information asymmetry — across three focused strands, all landed: a **survey
system** (BL-067) gating the tile map and deposit profile behind a player-dispatched action; a
**visibility model** (BL-068) making competitor buildings visible but their production/stockpiles
private, with markets as the public intelligence channel; and **population legibility** (BL-069)
surfacing the already-simulated habitability→workforce feedback so siting decisions are
reasoned, not guessed. The minor also absorbed the **legibility cluster** (BL-083–086 — settlement
markers, the industry-density substrate lens, always-on player presence, ambient opportunity
read), the **discovery-fog completion** (BL-088 persistent trade routes + BL-089 the
route-driven commercial-sphere activity fog, layered independently over the geographic survey
fog), **full budget/debt/per-building profitability legibility** (BL-072/073/074, pulled forward
from v0.0.9), **display options** (BL-076), and the QOL main-menu/campaign-start framing. The
discovery model — both fogs, the visibility rule, and their surfacing — is now landed design;
its authority is [`../ui/DISCOVERY.md`](../ui/DISCOVERY.md), not this roadmap or the backlog.
Per-item detail is in `DEVLOG.md`.

**v0.0.9 — Budget clarity + polish (cut 2026-07-05).** The remaining legibility rough edges
cleared ahead of the v0.1.0 audit: the in-app system menu (BL-070), the economy-ledger and
construction-panel legibility bugs (BL-081/082), the corp-emblem glyph family (BL-090), and the
hover-card activity line (one of the two BL-089 activity-fog deferrals). Per-item record in
`DEVLOG.md`.

**v0.1.0 — the road to the cut (historical, status 2026-07-31; the tag exists as of 2026-08-03).**
Landed on `main` since the tag: **roads &
planetary logistics** (BL-146–149, complete 2026-07-10 — the generated per-nation lattice with
three road tiers, cities as free logistics hubs, the inland logistics-hub building; built ahead
of the cut rather than waiting for the v0.1.1 slot first planned for it); the **2026-07-08
lens/UI review batch** (BL-133–145 + BL-150, all complete); the **legibility cut-blockers**
(BL-174, BL-176–179, merged 2026-07-30); the **landform render** (BL-231) and its **spanning
markers** (BL-232), both landed; a first cut of the **continent lens** (BL-226, landed
2026-07-30 — the item itself stays open); and the opening of the generation/history arc
(BL-167 planetology chain complete; BL-220/221 history-ladder foundations — post-v0.1.0-themed
work that landed early). What remains toward the cut is named under § v0.1.0 below.

---

## The arc from here

The map no longer ends at the prototype cut. Ben's **2026-07-09 refocus** extends it: **v0.1.0**
still cuts the economy-loop prototype; then the **v0.1.x** band lays groundwork for an *expanded
prototype*, **v0.2.0** carries the AI opponent, **v0.3.0** the player-identity refocus (versioned
as its own minor — Ben, 2026-08-04, resolving NR-045), and **v0.4.0** brings the political layer
and the filter system online — then **v1.0.0** (Ben, 2026-08-08) names the point where all four
cohere into one playable game. The v0.0.x themes have all shipped; the roll-up is § Where we are,
the per-item record `DEVLOG.md`.

**What v0.3.0 carries changed on 2026-08-10.** It was "the governing body"; it is now **the
national private militia** (NR-120). The *sequence* is unaffected — the identity minor still sits
after the AI opponent and before politics, for the reasons NR-045 settled — but what lands in it,
and what the v0.1.x band is stubbing *for*, both change. See § v0.3.0.

### The near sequence — which sprint cuts which minor

Planned in one pass on 2026-08-10 and owned by [`SPRINTS.md`](SPRINTS.md) § Sprints 8–14; named
here only so the roadmap's themes and the live sequence do not drift apart again, which is the
failure NR-102 (sequencing decoupling) records. **This table is the plan, not a commitment** —
amend it when a goal changes, not at the retro. *Amended the same day it was written: Sprints 8 and
9 both closed, and a new Sprint 10 (the living world) was inserted ahead of procurement, shifting
everything after it by one.*

| Sprint | Cuts | Theme |
|---|---|---|
| 8 | *(no tag)* | **Who the player is.** ✅ Closed 2026-08-10. BL-094 rewritten as the militia, BL-350 filed, v0.3.0 roster reconciled. |
| 9 | **v0.1.5** | **The militia takes the field.** ✅ Closed 2026-08-10. Military bases & supply, a starting base and unit; BL-332 designed and re-versioned. |
| 10 | **v0.1.13** | **The living world** — real background industry replaces the abstract substrate, multi-building tiles, real population demand, and the O(corps × tiles) scan fixed ahead of it. |
| 11 | **v0.1.14** | **Procurement & its goods** — the contract seam plus BL-340's resource tiers, landing into a market that now has real suppliers to choose between. |
| ~~12~~ | ~~v0.1.11~~ | **Superseded 2026-08-12** — the policy-surfaces minor parked with the space arc (NR-177). Never opened. |
| ~~13~~ | ~~v0.1.7~~ | **Superseded 2026-08-12** — v0.1.7 stays live (generation is shared) but is no longer the next cut. Never opened. |
| ~~14~~ | ~~v0.2.0~~ | **Superseded 2026-08-12** — the AI opponent parked with the space arc after its third deferral. Never opened. |
| 15 | *(no tag)* | **The 0 CE refocus.** ✅ Retro-recorded 2026-08-12 — the tangent that became the product; see SPRINTS.md § Sprint 15. |
| 16 | **v0.1.15** | **The mercenary vertical slice** — BL-377 (contract seam) playable end-to-end, BL-315 (conflict spine) on the critical path. |
| 17 | **v0.1.17** | **Economy breadth** — named in the minor's own bullet; row added 2026-08-19 so the table covers the live arc. |
| W1 | **v0.1.16** | **The watch** — BL-412 (agent seam), BL-408 (god view), BL-411 (strategy readout). Opened 2026-08-19. |
| D1+D3 | **v0.1.11** | **Meta opens** — BL-479 (tech effect union), BL-480 (law author + treasury), one batch. Opened 2026-08-19. |

**Why the living world was inserted (2026-08-10, Ben's call).** *"Replace the substrate entirely."*
The saturation the economic premise assumes is currently an abstraction injected once per tick —
`inject_substrate_demand` — with no building behind it: **~16–32 buildings sit on Kepler's 15,120
tiles**. Two consequences drove the ordering. Procurement's counterparty model needs suppliers to
choose between, and against eight lean corps *"another supplier may still quote"* is often false —
it would ship correct and unexercised, the shape of Sprint 9's `hire_unit` (NR-121). And BL-340's
substrate-basket step would otherwise be built twice, since BL-365 removes the function it extends.
**v0.1.13 was the natural home** rather than a new minor: it had been hollowed out when BL-340 left
for v0.1.14, and BL-130 / BL-132 were already orphans that belong to this work.

**Deliberately not in the six: v0.2.0 (the AI opponent).** It is the thing that makes Io a game
rather than a simulation, and holding it is a real cost — accepted so the opponent is built
against a settled player identity instead of re-fitted to one. It is the obvious **Sprint 14**, and
it has now been **deferred twice in the same direction** (the refocus, then the living world); the
second deferral at least argues for itself, since an opponent is more interesting in a market with
real competitors. **v0.1.12** (logistics modes) is unblocked and uncontroversial — a good filler if
a sprint above finishes short.

### v0.1.0 — Quality audit + legibility polish + cut

*Theme: make it read cleanly, prove it holds up, then ship.* The legibility strands have landed —
the 2026-07-08 lens/UI review batch, the legibility cut-blockers, and roads with them (§ Where we
are). **The terrain/landform strand is closed (2026-07-31).** BL-231/232 (landform render +
spanning markers), BL-226 (continent lens), BL-233 (terrain combat modifiers), BL-234 (font glyph
range) and BL-162 (tile construction panel) are all complete; BL-230 (hover glance-then-stick) was
closed out with them. Three of those had already landed and were simply never flipped off the
non-terminal `landed` status, so they had been reading as open work.

Two of the five were more than bookkeeping. **BL-233** deleted `is_barrier` and re-priced conquest
from the graded terrain field, with water as its own weighted term (weight 750, chosen by sweep to
hold the political map at 30 Kepler nations while fixing the grading defect — Pallas's barrier
field was a flat *zero*). **BL-234** turned out to be wider than filed: 43 sites across 7
codepoints, not 26 across 2.

Then the **quality audit** — three instruments, now built — and the cut.

**Audit instruments** — no new systems. **All three built 2026-07-31** (BL-249/250/251); they were
named here from the start but never itemised, so nothing owned them. Results:

- **Frame budget** (BL-249, `src/ui/frame_stats.{hpp,cpp}`, F11, off by default). Reports last /
  avg / max / 1% low against the targets — **avg < 8 ms, max < 16.7 ms** panning the full Kepler
  grid (15,120 tiles) — and breaks a spike into build / submit / present / other with ImGui's
  vertex and draw-command counts. Allocation churn is **declared not instrumented** rather than
  invented; it is not obtainable from inside the frame loop. *The targets themselves remain a
  human-in-the-loop measurement against the real app — headless capture has no vsync and no real
  present, so its numbers say nothing about them.*
- **Econ-tick scaling** (BL-250, extends `econ_stability`). Prototype scale **0.0018 ms/tick mean**
  — 555× headroom against the 1 ms target. A six-rung bodies × corps sweep gives **32× size →
  83.1× time, i.e. size^1.28**, inside a size^1.5 tolerance; the largest swept world (128×
  prototype) still ticks under 1 ms. It **named the bend**: `run_corp_strategic_step` rescans every
  tile per due corp, an O(corps × tiles) term — filed as **BL-253**, not a cut blocker.
- **Data creep** (BL-251, `tools/verify/data_creep_harness.cpp`). ~30 counters + RSS over **1500
  ticks of the real generated world**. Every exercised counter is flat from tick 500 to 1500 and
  RSS plateaus at 13,980 KiB — **no unbounded structure found**. It also reported its own blind
  spot: no convoy is dispatched in the rollout, so the convoy / trade-route / glimpse plateaus pass
  **vacuously**, and `trade_route` is never-erased by construction — filed as **BL-254**, which is
  a v0.1.0 item because it closes a hole in a cut gate.

**The four remaining v0.1.0 items all landed 2026-08-01**, which leaves the cut gate itself in
a state it has not been in before: *the suite is green on both platforms at the same commit.*

- **BL-254** (convoy data-creep scenario) — the three vacuous plateaus now bind. Convoys drain
  (1500 seeded, 1496 credited and retired), trade routes saturate at their structural bound of 18
  by tick 36 and hold flat for the remaining 1464, glimpse stamps are overwritten in place. It
  surfaced a second cause of its own blind spot, independent of the launchpad gate: the generated
  world seeds every market on the single tiled body, so it holds no inter-body pair and cannot
  record a route at all. **Whether non-home bodies should have markets at campaign start is an open
  design question** this item deliberately did not settle.
- **BL-252** (goldens) — both candidate causes turned out to be real. The bands were stale (blessed
  in the commit that added the harness, before BL-203 rewrote the Corp AI) *and* cross-platform
  divergence is genuine (seed 4 finals at 392,148 under MSVC, 182,746 under GCC). Widening was
  measured and rejected — one band holding both spans ~±100%. Headless bands are now **pinned per
  toolchain**; visual goldens are **Windows-authoritative** (DEVELOPMENT_PRACTICES.md §
  Cross-platform goldens). Optimisation level was excluded as a cause: MSVC /O2 reproduces MSVC
  Debug value for value.
- **BL-255** (build type + timeouts) and **BL-162** (tile construction ledger, reopened by review
  and now complete: world-layer estimator, per-recipe rows, ledger survives a build).

**"Headless harnesses green" now has a single truth value.** Linux/Release **35/35**;
Windows **35/36**, the one failure being `econ_stability`'s absolute 1 ms tick bound at the largest
swept rung — filed as **BL-258**, and a build-configuration artefact rather than a regression: the
Windows tree is deliberately Debug, R5 still passes with 19× headroom, and every growth-shape
assertion holds.

**The Cut happened on 2026-08-03** (`49c7fbf`, tag `v0.1.0`), preceded by a full visual re-bless
(BL-259). What had been listed as owed before it resolved as follows:

- **BL-258** (gate the absolute timing bound on an optimised build) — the live successor is
  **BL-288** (Release-only test failures, `design-owed`, A, v0.1.3). Four harnesses fail in Release
  and nothing caught it, because the default `build/` tree is Debug. Same root, wider scope.
- **Frame-budget targets measured against the real app** — still owed, and still needs a human at
  the keyboard: headless capture has no vsync and no real present, so its numbers cannot speak to
  them. Tracked as **NR-026**, open. The HUD itself shipped (BL-249).

### v0.1.x — Expanded-prototype groundwork

*Theme: ponder and stub what the expanded prototype will need — design-forward, data-model-first,
no committed systems yet.* Past the cut, the v0.1.x band is where the game's next dimensions get
their first shape: enough design and stubbing that the v0.3.0 refocus and the v0.4.0 political
layer land on positioned ground rather than a greenfield. v0.1.1 is a concrete build minor; the
stub minors (v0.1.3–v0.1.6) were deliberately design-forward — each carried a placeholder
`design-owed` item that firmed into real design as it was reached. **That framing has been
overtaken (2026-08-09/10):** BL-342–345 turned the stubs into buildable work, v0.1.3 and v0.1.4
were cut on 2026-08-10, and the band now runs to **v0.1.13** — the design-forward minors became
concrete, and the tail was named when `post-v0.1.0` was swept (NR-101, below).

> **What this band is FOR — REWRITTEN 2026-08-10 (NR-120).** The band's answer used to be: the
> player is a **governing body**, so laws (v0.1.3), techs (v0.1.4), military (v0.1.5) and politics
> (v0.1.6) are *its levers*, tested by **"does this system reach military as well as economic
> outcomes?"**
>
> **That test is retired**, along with the framing that produced it. The player is a **national
> private militia** that contracts private companies to build its space equipment. A militia does
> not legislate — **it procures**. Law, policy and science are therefore not the player's
> instruments; they are the **conditions it operates inside**, and its agency runs through what it
> buys, from whom, and what it can field.
>
> The replacement test is Sprint 8's to author (NR-120 § still_open). Until it exists, the
> question to hold each stub against is the blunt one: **whose agency does this surface express?**
> If the answer is "the player enacts it", check that against an actor who has no legislature.
>
> **What this costs, stated plainly.** v0.1.3 (Laws) and v0.1.4 (Techs) were cut on 2026-08-10,
> *hours* before the refocus, and both were justified by the retired reason. This is **not** a call
> to unpick them: `condition_set` (BL-342) is a generic predicate object, and one enacted law
> (BL-343) plus one earned tech (BL-344) are working machinery that does not care who the player
> is. What changes is **who enacts** — which makes the laws surface an *input* to the player's
> problem rather than an output of their agency. **BL-155, BL-156 and BL-186** (v0.1.11's laws and
> tech surfaces) are the most exposed open items on the board and must not be built before Sprint 8
> rules on them.

- **v0.1.1 — The word interface — CUT 2026-08-09.** *(Re-themed
  2026-07-31 for roads; **re-themed again 2026-08-03**, NR-034 — Ben named the word interface as
  this minor's theme.)* Two threads, and it stays the concrete build minor.

  **The word interface (new theme).** The route from docs to a text-driven player: the action
  dictionary (**BL-270**, complete) supplies meaning, the blackboard export (**BL-206**,
  complete) supplies state, and the **Io MCP server** (**BL-278**, moved here from v0.2.0 by
  NR-044, **complete**) is the socket that makes them drivable by any agent runtime — shipped as
  `ProjectIo --serve` plus `tools/mcp/` (`d62a4f0`). All three legs of the theme have now landed.
  BL-278 touched no simulation code, and landing it early is what lets a first real text-driven
  play attempt happen before more gets designed on top of it. Word-driven generation and the difficulty work
  land later in the arc — deliberately not pre-committed here; the arc gets named once the
  server exists and the first cloud session has produced evidence. See `docs/ai/AI_OPPONENT.md`
  § 10.

  **Shell & legibility (the standing set).** The road tier legend (BL-184) and road fog dimming
  (BL-185), building stack capacity (BL-193), the drill-through disclosure idiom (BL-214), the
  text-wrap render audit (BL-215), chat pinning (BL-216), and the building-selection tile format
  (BL-229 — Q1–Q4 answered 2026-08-03, no longer design-owed); the hover glance-then-stick
  (BL-230) is already landed.

  **Added 2026-08-03 from the review queue.** Retire the History ledger's Tiles view (BL-281),
  dual-endpoint trade-route log entries (BL-282), corp placement constrained to the home
  province (BL-283), exclave measurement reopened from BL-054 (BL-284), and harness golden
  coherence (BL-285).

  **Retrofitted 2026-08-08 — still open, surfaced 2026-08-01 → 08-04.** The minor never fully
  closed; three more waves of work now target it. **The documentation-audit findings
  (2026-08-04)**, three at priority A: naming banks read Earth-European despite the no-real-names
  standing rule (BL-290); the `world_audit` harness fails, so `TILES.md`'s tile census can't be
  re-measured (BL-291); three order-book presses have no `corp_command` verb, so no text-driven
  player can trade (BL-293 — independently re-found 2026-08-06 by Project-Rival's Battery A run
  at a measured 20% gap in a basic instruction set, NR-073). Plus the orphaned Economy panel
  (BL-292) and two dead-symbol cleanups (BL-294, BL-295). **A standing/scoring system** (BL-262,
  priority A, opened by Ben 2026-08-01): nothing today tells the player how they compare to
  rivals, on what axis — CONCEPT.md forbids an end-game verdict, so the answer is a standing, not
  a score. **Settled UI revisions, still unbuilt:** generated body names (BL-257), the
  UI-justification store pairing every readout with the question it answers (BL-260), the
  disclosure-controls revision — expand vs. full-screen as two separate controls (BL-265),
  Selection-always-open (BL-266), and the generation-globe preview (BL-256). Plus propellant as a
  real resource (BL-308) and deed history-log lines, buildable now ahead of the Era 1 tree's
  deed-gated keystones (BL-309, see v0.3.0 below). **Build health:** a fresh CMake configure
  can't fetch SDL3 on at least one machine — a TLS revocation failure a populated `_deps` cache
  hides (BL-302).

  **The retrofit was the mistake, and it is undone (2026-08-09, at the cut).** Everything in the
  paragraph above was assigned to a minor whose own theme had *already shipped* — the three legs
  landed by 2026-08-03, and three later waves were then hung on the same tag. That is what made
  v0.1.1 uncuttable for six days, and it is the concrete instance of the root cause NR-103 names:
  a theme with no done-definition has no test for *finished*, so it absorbs work indefinitely. The
  24 items still open at the cut were **re-homed into three coherent minors** — v0.1.8 (build
  health), v0.1.9 (shell & legibility), v0.1.10 (generation & content) — with BL-293 (order-book
  verbs) and BL-262 (standing) moved to **v0.2.0**, where a text-driven player and legible rivals
  are what they actually serve. None was cancelled; all kept their priority.

  **Done-definition — v0.1.1.** *Written at the cut, 2026-08-09.* v0.1.1 is cut when:

  - **An agent can read the world state** through a stable export rather than by scraping the UI.
    *(BL-206, blackboard export.)*
  - **Every control has a stated meaning** — typed arguments, preconditions, expected output —
    in a machine-consumable dictionary transcribed from the command seam, not authored beside it.
    *(BL-270, action dictionary.)*
  - **Both are reachable by any agent runtime** over a socket the engine ships without an HTTP
    client, an API key, or a cloud dependency. *(BL-278, `ProjectIo --serve` + `tools/mcp/`.)*
  - The build is **green** and the loop still verifies.
  - **Narrowed, honestly:** the write leg is partial. Three order-book presses have no
    `corp_command` verb, so an agent can read the book and be told what the press means but cannot
    place a standing sell order — the world has no order book to mutate, and no serialisation path
    for one. `ACTIONS.md`'s "full word interface" claim is corrected to say so. Carried by
    **BL-293** in v0.2.0 (NR-099).

  **Cut 2026-08-09.** 28 items terminal. Beyond the three theme legs, the minor also carried the
  sticky-card family (BL-194–BL-198, BL-214, BL-247), the corporation dashboard (BL-248), the
  comms-dock re-plan (BL-227), hover freeze / glance-then-stick (BL-228, BL-230), the
  commercial-activity fog work (BL-150–BL-154), the radial tech-tree viewer (BL-310), the
  minimap and header reflow (BL-312, BL-313), the New World wizard's real-tile preview (BL-319),
  the Mediterranean rift sea (BL-276), and the GPU/multicore performance pass (BL-267).
- **v0.1.2 — Buildings rework — CUT 2026-08-09** (**BL-323**, complete). *(Added by Ben, 2026-08-07: "we need to pad out the
  number of available buildings, and start enforcing placement rules. Especially for logistical
  max building range… We need to put a lot of work into this before any simulated games can
  occur.")* The band's second concrete build minor, and the one the simulated-play arc waits on.
  Four strands: **pad the roster** out toward `PRODUCTION.md`'s designed ~26 (against 5 generic
  kinds and 4 recipes today), mostly as Lua authoring rather than enum churn; **enforce a
  logistical max range** on placement — the rule with no code at all today, built as a reach query
  over the terrain-weighted cost function `logistics.cpp` already provides; **make build time
  depend on the site** (landform, logistics distance, established stack) rather than staying a
  flat per-type constant; and **surface construction on the canvas**.

  **Why it gates simulated play.** With remoteness free, the optimal siting strategy is "the
  richest tile anywhere" — a lookup, not a decision. An AI player driving the corp-command seam
  finds that immediately and plays it forever, so neither the v0.2.0 opponent nor a text-driven
  session tests anything until siting carries a trade-off. It is the placement-side counterpart to
  the constraint **BL-316** is landing in the Era −1 sim: breadth must cost something.

  **Sequenced ahead of the stub minors (Ben, 2026-08-07, resolving NR-084).** Originally slotted
  last to avoid renumbering; moved ahead of the design-forward stub minors (Laws, Techs, Military,
  Politics), which follow it here. None of them depends on this work and it depends on none of
  them, so the old order delayed the concrete blocker behind four design passes for no technical
  reason.

  **Done-definition — v0.1.2.** *Written at the cut, 2026-08-09, as the first of the per-minor
  done-definitions NR-103 asks for; v0.1.0's list above is the model.* v0.1.2 is cut when:

  - **Siting carries a trade-off.** A building cannot be placed at unbounded distance from a
    supply anchor, and the budget is authored data rather than a constant in code. *(BL-323 S2.)*
  - **The rule teaches rather than merely refuses** — build surfaces stop offering tiles the gate
    will reject, and a body with no anchor still has a legal first move. *(S2b + bootstrap.)*
  - **Build time depends on where you build**, not only on what you build. *(S3.)*
  - **Construction reads as a process** on the canvas, with its remaining time legible. *(S4,
    BL-327.)*
  - The build is **green** and the rework's harnesses pass. *(12/12 and 26/26.)*
  - Excluded by scope, and filed rather than dropped: the processing-chain roster, which needs new
    resource types and a save-format change (**BL-340**).

  **Cut 2026-08-09.** All six items terminal — the rework itself (BL-323) plus construction-ledger
  grouping (BL-326), the under-construction glyph (BL-327), the pre-commitment supply warning
  (BL-328), the reach-circle retirement (BL-329) and the building-selection click-model fix
  (BL-330). Tagged ahead of `v0.1.1`, whose theme is cut separately; pre-1.0 numbering is
  advisory, and each tag documents its own theme.
- **v0.1.3 — Laws** (**BL-155** law/policy surface design, **BL-186** laws ledger UI, **BL-280**
  negotiated tax rate). First pass at the law / policy surface — what a law *is* as a data
  object, how it gates or modifies economic (and later political) behaviour, and its ledger
  surface. **BL-280** (settled 2026-08-03, NR-024) gives Tax its first concrete lever: a
  chartered corp bargains its rate with its home nation rather than reading a fixed number.
  Design + stub.

  **No longer design-only (2026-08-09).** Two prototype-enablers were filed so this minor can be
  **cut** rather than merely designed: **BL-342** (the shared `condition_set` evaluator) and
  **BL-343** (laws MVP — one enacted extraction levy, appearing as its own line in the budget
  ledger). The unlock was structural: BL-155 and BL-156 had *both* settled on the same predicate
  object — "a flat AND-list of atomic conditions" — and neither built it, because each was scoped
  design-only. One small pure evaluator was all that stood between two design-forward minors and
  two shippable ones. BL-342 lands here because Laws is its first consumer.

  **Done-definition — v0.1.3.** v0.1.3 is cut when:

  - **A predicate exists that a law can be read through.** Not a label naming what a gate would be
    about — an object that resolves true or false against the world, purely and deterministically,
    with an empty set meaning *always* because that is the common case for a law.
  - **The predicate can ask a military question.** BL-094's test applied at the foundation rather
    than promised for later: a subject enum that enumerates only economic quantities is the exact
    failure the identity pivot exists to avoid, and it is far cheaper to avoid now than to
    unpick. *(2026-08-10: this survives the refocus intact and is the reason v0.1.3 does not need
    unpicking — a predicate that can ask a military question is exactly as useful when the player
    is the one **subject** to the law as when it passes it.)*
  - **One law is enacted, enforced and visible.** Not the ten-law list; one law the player can
    switch on, whose effect lands on a number they already read. A law the player cannot see
    working is indistinguishable from an unimplemented one.
  - **The enforcement seam is settled on that one law.** A law is a modifier *over* the market,
    never an override *of* it — so the levy applies where the flow is accounted, not where the
    price is resolved, and the market stays the only thing that sets prices.
  - **Enacting nothing changes nothing.** The shipped default is inert and bit-identical to a world
    with no laws at all, and repeal returns to that baseline exactly.
  - Excluded by scope, filed rather than dropped: the other nine laws and the other three effect
    families (**BL-155**), the laws ledger and enactment politics (**BL-186**), and the negotiated
    tax rate (**BL-280**) — all re-targeted to **v0.1.11**, since what remains of them is a *policy
    surface*, and the seam they would surface now exists.

  **Cut 2026-08-10.** Both enablers terminal — **BL-342** (condition_set evaluator, 40 assertions)
  and **BL-343** (laws MVP, 21 assertions). Gate: 58 tests, 0 failures. Three new harnesses;
  no existing economy harness changed, because `apply_budget`'s new `production` argument defaults
  to null and therefore charges nothing for every caller that does not opt in.
- **v0.1.4 — Techs** (**BL-156**). Early design toward the tech / quest system — the condition-set
  gate model (gate = quest = tech) that BL-087 reframed and the v0.4.0 filter system formalises.
  Design only; precursor to BL-087. **Overtaken in practice, 2026-08-04/05:** BL-087 itself —
  versioned v0.3.0, not this stub, see below — is now under active design as a constellation tech
  web at the same grain as the Era −1 sandbox's ancient tech ladder; BL-156's early pass and
  BL-087's real one are converging rather than cleanly sequencing one after the other.

  **No longer design-only (2026-08-09).** **BL-344** (techs MVP) makes it cuttable. More of this
  minor exists than its status suggested — a tech tree has been in `world/tech_tree.{hpp,cpp}`
  since 2026-07-08 and BL-310 shipped its radial viewer — but `tech_tree.hpp:49` stores
  `condition` as a descriptive **string**, so no tech can be earned and nothing can be unlocked.
  BL-344 promotes that field to BL-342's `condition_set` and closes the loop once, end to end.
  Its unlock target is deliberately the **military base** (landed with BL-325) rather than another
  building: BL-094's design test says a technology that can only unlock a building is being
  designed for the corporate player we are pivoting away from, and gating the base costs the same
  as gating a smelter. Also a cheap window — no save format exists yet (BL-107 is open), so
  changing a stored field is a compile-time change today and a migration later.

  **Done-definition — v0.1.4.** v0.1.4 is cut when:

  - **A tech can be earned.** The gate is a real predicate rather than a descriptive string, and
    the tree stops being a picture of a system and becomes the system.
  - **Earning it is per-corporation.** Research is not a world fact; a rival's discovery does not
    open the player's door.
  - **The unlock reaches a military outcome.** A technology that can only unlock a building is
    being designed for the corporate player the pivot is moving away from — so the first thing
    ever gated is the Military Base, not a smelter.
  - **The refusal teaches.** A locked type says which technology is missing, at the same call site
    that would otherwise offer the build — never a generic "can't build there".
  - **"Not yet authored" is distinguishable from "unconditional".** An empty predicate is *true*;
    a tech with no gate must therefore say so rather than read as unlocked.
  - Excluded by scope, filed rather than dropped: research points and their economy (**BL-332**),
    the rest of BL-156's early design pass (**BL-156**) — both re-targeted to **v0.1.11** — and the
    constellation grain and deeds (**BL-087**, v0.3.0).

  **Cut 2026-08-10.** **BL-344** terminal (33 assertions). Gate: 58 tests, 0 failures. One
  knock-on, taken deliberately: `buildings_rework_harness` now grants the tech in its BL-325 setup
  block, because that block tests placement and staffing rules rather than the gate.
- **v0.1.5 — Military systems** (**BL-157**). *Cut by **Sprint 9**, the first buildable consequence
  of the refocus — the militia has to exist on the map before there is any point modelling what it
  buys.* The live set is **BL-325** (military bases: a muster building, hire moved onto it, unit
  supply read off it), **BL-331** (the player starts with a base and one unit) and **BL-332**
  (military points + a dedicated research building), the last moved here from v0.1.11 on
  2026-08-10: it answers *how does the militia get better*, which belongs with the military minor
  rather than the laws one. The Conflict dimension's first data-model footing —
  units, forces, and the seams they need in the world model. Stub, not mechanics (Conflict proper
  stays post-cut scope). **Firmed up substantially by the 2026-08-07 military design session**
  (NR-077, six rulings filed in total — the rest fall under BL-315 below): unit-grain verbs
  rather than stacks, tile position as the canonical location, era-keyed (not player-tech-gated)
  rosters, doctrine-preference as the voice-bias shape — plus a filed follow-on, **BL-314** (unit
  verb family, design-owed), for the
  actual command verbs once this stub has a seam to transcribe from. Still a stub for *campaign*
  play; the Era −1 sandbox (v0.3.0, below) is where the same architecture gets proven under real
  simulated wars first.
- **v0.1.6 — Politics (stub)** (**BL-158**). A data-model stub only — enough political layer for
  the v0.3.0 player actor to have something to stand in, deferring the working system to v0.4.0.

  **No longer design-only (2026-08-09).** **BL-345** (politics MVP) makes it cuttable, and it is
  smaller than the minor looks: BL-158's own 2026-08-02 settlement records that most of the stub
  had already dissolved into other items — political character into nation generation, the lever
  slots into BL-155 and BL-094 — leaving exactly one unowned axis, **inter-nation relationships**.
  BL-345 builds that axis *with a consumer*, on a deliberate rule: **a stub nothing reads is
  indistinguishable from no stub**, and shipping records no code path consults is how you discover
  at v0.3.0 that the shape was wrong. The consumer is convoy cost across a cold border — a
  modifier on a cost function logistics already computes, the same cheap shape BL-323's reach rule
  took — so the political map starts mattering to the economic one.
- **v0.1.7 — Generation visibility + UI alignment.** *(Added by Ben, 2026-08-04: "a pass on
  generation visibility... visualising the world at each step — we haven't yet done that.")* The
  band's last minor before the refocus, and its third concrete build minor. Two themes:

  **Generation visibility.** Every generation step earns a surface the player (and Ben) can
  watch it through: the Generation Ledger build (**BL-303** — the window `GENERATION_LEDGER.md`
  designed but no item ever carried), the field-overlay lenses for the generation intermediates
  (**BL-304** — heightmap / moisture / band / plate on the Planetary canvas), and the political
  steps' visibility (**BL-305**, design-owed — the nation carve and corp seeding are today the
  only generation steps with no visibility designed anywhere). Siblings landing earlier keep
  their own goals: **BL-256** (generation globe, v0.1.1) and **BL-211** (player-facing history
  ledger).

  **UI alignment.** *"It's important to keep our UI in line with new development"* — the
  end-of-band review that walks the UI against everything v0.1.x added. **BL-098** (UX
  user-story review) is the vehicle, retargeted here from v0.1.1; it consumes
  `user_stories.json` and closes the loop between the band's new systems and the surfaces that
  serve them.

> **v0.1.8–v0.1.10 are concrete, and numbered last only to avoid churn (2026-08-09).** They carry
> the 22 items re-homed out of v0.1.1 at its cut. Their **numbers are not their sequence**: all
> three are buildable now, while v0.1.3–v0.1.6 are design-forward stubs, and Ben's standing steer
> is to cut concrete minors ahead of conceptual ones ("cut as many versions as we can now, rather
> than working on the lofty, conceptual stuff", 2026-08-09; the same call NR-084 made when
> buildings jumped the stub queue). They were appended rather than inserted at v0.1.3 purely so no
> existing minor had to be renumbered — pre-1.0 numbering is advisory, and v0.1.2 was already cut
> before v0.1.1. Each earns its done-definition at promotion, per NR-103.

- **v0.1.8 — Build health — CUT 2026-08-09.** *Theme: the project's own tooling stops lying.*
  Five items (BL-288 moved here from v0.1.3, where a priority-A build item was sitting behind the
  Laws design stub). None ships player-facing content; all five cost time on every session that
  tripped them.

  **What it actually found.** The premise of every item in this minor was that things were
  *broken*. Measurement said the tooling was mostly **misreporting**, which is worse, because a
  red suite that is mostly noise trains you to ignore it:

  - **The suite reported ten failures; exactly one was a failing assertion** (**BL-288**). Four
    harnesses pass but take longer than the flat 60 s bound (`earthlike_lean_trace` 121 s,
    `notable_worlds` 105 s, `mediterranean_sweep` 87 s, `earthlike_tile_census` 58 s — passing by
    luck). Two are open-ended research sweeps that never finish on any bound. Two assert absolute
    wall-clock times and failed only because a concurrent build was loading the machine. Now three
    tiers plus a `sweep` label excluded from the gate and a `bench` label that says "re-run idle".
  - **`world_audit` was never broken** (**BL-291**) — it exits non-zero on 1 assertion of 26, the
    S2 biome balance, which is a world-generation finding (carried by **BL-338**) rather than a
    broken instrument. The item's own three-day "stale and blocked" banner on `TILES.md` was the
    cost of reading an exit code as a verdict.
  - **`next_id.js` failed for a reason nobody would have guessed** (**BL-322**): `execSync` runs
    through `dash`, which aborts on the unquoted `(` in `--format=%(refname)` before `git` runs —
    so it worked on the Windows box where it was written and failed silently everywhere else. It
    was handing out ids **25 below the true ceiling**, the direct mechanical account of how
    BL-326..BL-333 each landed twice. Refs scanned went 0 → 53.
  - **The dependency posture's preferred option was disproved by measuring it** (**BL-302**): a
    shared `FETCHCONTENT_BASE_DIR` hard-fails across build trees on a generator-locked subbuild
    cache. Per-dependency `FETCHCONTENT_SOURCE_DIR_<dep>` works and is what landed.
  - **One real defect** (**BL-285**): `ai_skill_harness`'s GCC goldens, stale since 2026-08-01 —
    which had sat unnoticed among nine false positives.

  **Done-definition — v0.1.8.** v0.1.8 is cut when:

  - **A red test is a real failure.** No harness is reported failed for exceeding a bound it was
    never sized against, and research sweeps are not gate tests.
  - **A load-sensitive failure is legible as one** — absolute-time assertions are labelled, so
    they are re-run rather than mistaken for a regression.
  - **The id allocator defends what it claims to defend**, and fails loudly rather than open.
  - **A from-cold configure is visible** rather than hidden by a populated cache.
  - **Goldens describe the platform they run on**, and a stale set says so in place.
  - Excluded by scope, filed rather than dropped: verifying the from-cold check on the Windows
    machine, where the original TLS failure actually occurs (**BL-341**, v0.1.9) — it does not
    reproduce on Linux, so the fix is untested against its own symptom.

  **Cut 2026-08-09.** Gate: 53 tests, **1 failure**, down from 10 — and that one is `world_audit`'s
  biome balance, a genuine world-generation finding rather than an instrument defect.
- **v0.1.9 — Shell & legibility follow-through — CUT 2026-08-09.** *Theme: the standing UI set,
  finished.* Eight items. Ben ruled four open design questions at the batch's start rather than
  letting them stall it: the road-tier legend goes **contextual** (Selection/hover, not a
  persistent chip — the lens drawer was never available, being keyed to `overlay_mode` while roads
  render always-on like terrain); roads **do** dim with the commercial-reach fog; the Economy panel
  **gets a door** rather than being retired; and **BL-229** (building-selection tile format) moved
  to v0.1.10 because the item reserves that layout for Ben to design.

  Landed: the road tier legend (**BL-184**) and road/fog dimming (**BL-185**), building stack
  capacity (**BL-193**), the shell rect-algebra foundation (**BL-216**, partial — see below), the
  UI-justification store (**BL-260**), the disclosure-controls revision (**BL-265**), retiring the
  History ledger's Tiles view (**BL-281**), and the Economy panel's door (**BL-292**).

  **Three things the batch found that no item had predicted.**

  - **BL-216 is superseded in part by a *later, complete* item.** Its sections 1–3 specify a comms
    geometry that **BL-227** already replaced on Ben's own 2026-07-30 call — dock at
    `nav_pane_width` rather than `shell_column_width` (measured: at 1280×720 a shortened nav rail
    clips two of its nine slots), the fold-out column shortening to clear it, and
    `selection_band_height` derived rather than fixed at 340. Under *newest-dated wins* those
    sections were left unimplemented rather than reverting a landed item. What did land is the half
    that mattered: `shell_metrics.{hpp,cpp}` and the migration of all **five** `app.cpp` sites that
    each re-derived `disp.x - margin - mm_w` by hand. It also surfaced a live **8 px drift** —
    BL-312 flushed the minimap to the screen edge and the other four sites did not follow — now
    expressed once instead of invisibly five times. *Ben's call whether to close it.*
  - **BL-193 roughly doubled the econ tick** (**BL-347**, priority A, filed). Measured, not
    inferred: the harness was rebuilt at the parent commit and both run on the same machine.
    Largest sweep rung `min` 0.958 ms → 2.045 ms, with the cost present at *every* rung including
    the 1×8 baseline (1.71×) — the signature of fixed per-tick work, not a scaling term. **The
    prototype is unaffected** (0.20 ms mean, 5× headroom); what is lost is growth headroom, the
    same category as BL-253.
  - **BL-260's codegen has nothing to feed.** BL-247's in-UI question log and the `why_note` seam
    the item planned to generate into were removed 2026-08-02 (NR-018). The store is therefore a
    documentation artifact, which is the stronger reading of Ben's *"the docs are the audit"* — and
    13 of its 16 entries are marked `drafted`, because writing the pair **is** the design check and
    an implementer may draft it but must not ship it as settled.

  **Done-definition — v0.1.9.** v0.1.9 is cut when:

  - **The always-on canvas layers are legible** — a visual code the player cannot decode has a key
    at the moment of interest, and the fog reads as **one** wash rather than several treatments
    that resemble each other.
  - **Stacking is a decision, not a dominant strategy** — later sites pay less, every site draws
    real reserve, and the player can see both.
  - **Screen geometry has one owner.** No region re-derives another's edge by hand.
  - **Disclosure is two controls, not one** — expand in place, or take the canvas — placed in one
    column, drawn rather than typed, and a full-screened accordion shows all of itself.
  - **Every surface has a door and states its question.**
  - Excluded by scope, filed rather than dropped: the building-selection layout (**BL-229**,
    Ben's to design), the stack-aware profit estimator (**BL-346**), the econ-tick regression
    (**BL-347**), and the Windows from-cold check (**BL-341**) — all v0.1.10.

  **Cut 2026-08-09.** Gate: 53 tests, 2 failures, both known, filed and named above —
  `world_audit`'s biome balance (BL-338) and `econ_stability`'s absolute bound (BL-347).
- **v0.1.10 — Generation & content — CUT 2026-08-09.** *Theme: what the world is called and what
  it is made of.* Eight items. Notable for how often the **item's own diagnosis was wrong and the
  measurement corrected it** — three times, each recorded rather than quietly fixed:

  - **BL-338 (wetland)** blamed the relief commits. Refuted *empirically*: rebuilding `world_audit`
    at `802421c^` gave a byte-identical census. The real cause is conceptual — wetland is the one
    composition the `(band, moisture)` table cannot express, because a marsh is defined by where
    water *fails to leave*, which is elevation, and elevation had no say in composition at all.
    12 tiles → 159, and the gate's last failing assertion passes.
  - **BL-347 (econ tick)** named three suspects. **None dominated.** The sort flagged as
    O(n log n) was 3% of the added cost; the real cost was `std::map` node allocation the
    restructure introduced incidentally, per tick, in worlds containing no stack at all.
    8×256 min 2.045 ms → **0.87 ms**, better than the pre-BL-193 baseline.
  - **BL-346 (profit estimator)** — the claim that BL-079's loss-streak reflex acted on an inflated
    number was **retracted**: `estimate_building_profit` reads *realised* credit. The real site was
    `estimate_prospective_profit` (+213% at mid-band reserve), and BL-181 was inflated via its own
    inline model instead.

  **What landed.** Names are coined from each culture's own phonology (**BL-290**) with the kinship
  emergent rather than authored; body identity stops being a display string across **twelve** sites,
  eight more than the item listed, and the catalogue is generated (**BL-257**); corps anchor in
  their home province (**BL-283**); fragmentation is measured and attributed (**BL-284**); wetland
  is generated again (**BL-338**); the econ tick and the prospective estimator are fixed
  (**BL-347**, **BL-346**); propellant is a real resource with per-launch consumption (**BL-308**);
  and trade routes log both endpoints (**BL-282**).

  **BL-284 answered the question it was filed to ask.** BL-218 bought the expensive settlement-sim
  path on the argument that fragmentation would fall out for free. **It pays** — 60 emergent
  exclaves against 136 from orphan-island cleanup: 31% by component count but **49% by tile count**.
  The sim's exclaves are the large ones. The audit prints both, because quoting the raw count would
  overstate the sim's contribution twofold.

  **Done-definition — v0.1.10.** v0.1.10 is cut when:

  - **No generated proper noun is Earth-derived**, and names within a culture share a sound system
    *as a consequence of the chain* rather than by coincidence.
  - **Identity is an entity id**, never a display string — the precondition for generated names
    being a cosmetic change rather than a silent correctness bug.
  - **Where a corporation is says something** about where it came from.
  - **A claim the architecture was bought on is measured**, not assumed (**BL-284**).
  - **The tick budget holds**, and the estimators agree with what the tick actually credits.
  - Excluded by scope, filed rather than dropped: deed history lines (**BL-309**, designed but
    unbuilt) — v0.1.11.

  **BL-256 (generation globe) closed inside this cut, 2026-08-10**, after Ben asked it be verified
  rather than assumed. Two of its three requirements were already built; the third — a pannable
  camera — is **cut on a design argument**: the globe spins and takes no input, because an
  uncontrollable globe says what the preferences-not-parameters model already says, that the player
  sets conditions rather than steering. Reasoning propagated to `docs/ui/STARTUP.md`.

  **Cut 2026-08-10.** Gate: **55 tests, 0 failures** — the first fully green gate of the arc,
  reached only after a clean rebuild exposed a stale object that had been failing a harness from
  correct sources.
- **v0.1.10 — the theme, as originally filed.**
  Led by **BL-290** (priority A): the nation and city name banks read Latin/European, in direct
  breach of the standing no-real-names rule — every generated name must be sci-fi/fantasy from the
  seeded banks. With it: generated body names (**BL-257**) and the generation-globe preview
  (**BL-256**), corp placement constrained to the home province (**BL-283**), exclave measurement
  (**BL-284**), dual-endpoint trade-route log entries (**BL-282**), propellant as a real resource
  (**BL-308**), deed history-log lines (**BL-309**), and the Kepler wetland re-base (**BL-338**).

> **The band's tail was named 2026-08-10, sweeping `post-v0.1.0` (NR-101).** 45 of 97 open items
> carried no minor, and `post-v0.1.0` had become a synonym for *someday* — while this document was
> already assigning twenty of them to v0.3.0 and v0.4.0 **in prose**, where no query could see it.
> The roadmap and the backlog disagreed, and the disagreement was invisible unless you read both.
> Every open item now names a minor. The reconciliation was mechanical wherever the prose already
> said something; the three minors below are the residue — a cluster of real prototype work that
> had no theme to belong to, so it was given one. **Confirmed by Ben, 2026-08-10** — *"keep v0.1.12
> and v0.1.13 as named"* (NR-119) — so the two are settled minors rather than a working label:
> neither merges into the other, and neither is renumbered against the still-uncut v0.1.5–v0.1.7.
> **Each earns its done-definition at promotion**, per the rule this band already follows: NR-103's
> finding was that a theme with no done-definition absorbs items indefinitely, and writing one for
> work nobody has started would be inventing a test for *finished* before knowing what finishing
> looks like.

- **v0.1.11 — Policy surfaces, and the band's own leftovers.** **[Superseded 2026-08-18/19 —
  the live v0.1.11 block is in § The two arcs above (un-parked by ruling 5 of NR-331, re-themed
  *who enacts*, Lane D opened). Of this roster: BL-332 and BL-341 are complete, BL-264 moved to
  v0.1.7. Kept as record.]** *Theme: the levers stubbed across
  v0.1.3–v0.1.6 get the surfaces they were promised.* v0.1.3 and v0.1.4 shipped one working law
  and one earnable tech and deliberately excluded the surfaces around them, so this is where those
  land: the laws ledger and enactment politics (**BL-186**), the negotiated tax rate (**BL-280**),
  what remains of the law and tech design passes (**BL-155**, **BL-156**), and military points and
  research (**BL-332**). With them, four items the earlier cuts filed rather than dropped — deed
  history lines (**BL-309**), the player-facing history ledger (**BL-211**), the New World wizard's
  post-fold layout (**BL-264**), and nation-voiced public comms (**BL-212**, settled and explicitly
  *not* gated on BL-218). Plus **BL-341**, which is parked on a machine rather than on a theme: the
  from-cold configure check has to run on Windows, where the TLS failure actually happens.
- **v0.1.12 — Logistics modes.** *Theme: distance costs something, in more than one way, and the
  player can see it.* Four items that are all the same subject and were scattered across three
  categories: convoy payout weighted by haul distance rather than destination price alone
  (**BL-153**), rail as a mode distinct from the road tiers rather than a cheaper road
  (**BL-173**), coastal ports and sea trade as a mode distinct from land (**BL-188**), and the
  Planetary Supply lens reading as *flow* rather than a uniform arrow field on every hex
  (**BL-175**). The first three add ways for distance to matter; the fourth is what stops that
  being invisible, which is why it belongs here rather than in a UI minor.
- **v0.1.13 — Markets & materials.** *Theme: the market stops being fixed at world-gen, the goods
  it trades get deeper, and the save format learns to say no.* Markets emerging at runtime when
  colonisation or exploration reaches a body (**BL-263**) — today every market seeds at world-gen,
  which is why a generated world can never record an inter-body trade route. The processing half of
  the buildings roster (**BL-340**), which v0.1.2 excluded in as many words because it needs new
  resource types. Real market inventory versus derived-from-supply stock (**BL-130**) and full
  market/population co-generation (**BL-132**), both waiting on exactly this. And the two that
  follow from the rest: the save-format magic and version header (**BL-107**), because adding
  resource types is the struct-layout change it exists to reject, and the opening-economy question
  (**BL-192**) — whether a fresh campaign can afford and site a processor early, which its own
  settlement reframed from a generation guarantee into a measurement.

  > **BL-340 has left this minor (2026-08-10, NR-120 ruling 3).** The processing roster moves to
  > **v0.1.14** below. The refocus says trade is coloured by military use and space hardware
  > *"initially"* — and if those goods do not exist, the colour is an assertion rather than a
  > mechanic. The five items left here stand on their own; what they lose is a keystone, not their
  > coherence.

- **v0.1.14 — Procurement.** *(Named 2026-08-10, NR-120.)* *Theme: the militia does not build its
  equipment — it buys it, from someone who can refuse.* The minor that carries the mechanic the
  refocus invents, and the first place the new player identity is playable rather than specified.
  Two halves. **The contract seam** — a counterparty, a price, a lead time, and a refusal — filed
  in Sprint 8 because nothing in the backlog covers it today; the closest existing item is
  **BL-280** (negotiated tax rate), which is the same *bargaining* shape pointed at a different
  object and should be designed alongside it rather than after it. And **the goods worth
  contracting for** — **BL-340**'s processing roster, pulled forward from v0.1.13, supplying the
  military and space-hardware resource types that make "space equipment" a thing on the market
  rather than a label on the premise.

  **The dependency that decides this minor's size.** BL-340 is difficulty 4 and still
  `design-owed`. It is the largest unknown in the near sequence, and the most likely thing to spill
  a sprint. **Where this minor sits in the numbering is a convenience, not a claim** — it is
  sequenced *ninth to tenth in build order* (Sprint 11, after the living-world insertion shifted
  the table), well ahead of v0.1.11–v0.1.13, and follows
  the band's established habit of numbering by naming date rather than cut order.

### v0.2.0 — The AI opponent

> **PARKED 2026-08-12 (NR-177) — space arc.** Deferred twice, now parked whole with the arc.
> Its items carry `parked: true`; nothing here is committed scope until the DLC resume. The
> word-interface and MCP machinery it rests on is landed and era-agnostic, so an *ancient*
> opponent, if one is wanted, would be a new theme in the live arc, not this minor un-parked.

*Versioned theme: the AI opponent.* The backlog's live v0.2.0 set is the corp-AI arc: stage A
(**BL-202**) and stage B — predictive spending (**BL-203**) — are both **complete**
(`src/world/corp_ai.{hpp,cpp}`), together with the skill-regression harness (**BL-204**,
complete — seed-set bot-vs-bot goldens + the tick-boundary state hash). **BL-205** (corp chat
log) was **cut 2026-08-07** (NR-075) — self-declared inactive; its successor is the BL-278/BL-279
MCP arc below, not a standalone chat window. Still open: **BL-207** (persona counsel packs) and
the trade-policy pair **BL-160/161** (auto-exchange policy, counterparty allow/deny). **Three
pending scope calls sit on these, unresolved** (NR-076, 2026-08-07): cut BL-160 and let BL-161
stand alone; cut or park BL-207 behind BL-279's local-model evidence. Ben's to rule on; this
roadmap pass does not pre-empt them. **BL-261** (player alerts) stays deliberately parked here,
pending real AI-opponent play data to judge alerts against.

*Added 2026-08-03 — the word-interface route.* Ben's direction after the public
LLM-grand-strategy research sweep (`docs/ai/AI_OPPONENT.md` § 10): the C-route planner gets an
**MCP** interface and a **small, local** runtime model, with cloud inference used only to
generate the training corpus. **BL-279** (AI trace corpus + the fine-tuning pipeline) carries
that here; its prerequisite **BL-278** (Io MCP server) **landed 2026-08-03** in **v0.1.1**, where
NR-044 placed it. Both sit *above* the deterministic utility core, not in place of it — the small
local model is a macro layer over BL-202/203, never the whole opponent.

*Roster reconciled 2026-08-10 (NR-101).* Five items that were sitting on `post-v0.1.0` are this
minor's work and now say so: **BL-334** (Stage C — the conditioned dialogue layer over the
`corp_decision` intent stream, the shape Ben ruled in NR-094), **BL-335** (measure the real
per-decision token cost through the landed MCP server, replacing a ~300-token *assumption*),
**BL-336** (a goal layer for step-wise myopia — filed **parked**, explicitly contingent on
observing the failure mode first), **BL-306** (the text-only Rival harness, driving 0 A.D. through
its official RL seam instead of computer-use), and **BL-253** — `run_corp_strategic_step`'s
O(corps × tiles) rescan, which belongs here because it is the *opponent's* scaling term, not a
general performance item.

### v0.3.0 — The national private militia (the refocus)

> **PARKED 2026-08-12 (NR-177) — space arc, minus its Era −1 heart.** The militia identity is
> being *tested* one era earlier as the ancient product's mercenary company, not replaced. The
> Era −1 cluster this section calls "groundwork folded in" moved to **v0.1.16** in the live arc
> on Ben's ruling — the sandbox is now the product. What stays parked here is the 1960-era
> identity work itself (BL-094, BL-087, BL-333, BL-182) and the generation-flavour tail.

*Theme: change who the player is.* **Versioned 2026-08-04** — Ben's answer to the sequencing
question open since 2026-07-31 (NR-045): the pivot takes **its own minor**, not a share of
v0.2.0. Sequence rationale: v0.2.0's AI opponent proves the game plays before the pivot changes
who plays it, and the political layer (v0.4.0) needs the new actor to exist first. **That
sequencing survives the 2026-08-10 refocus unchanged; the actor does not.**

> **REFRAMED 2026-08-10 (NR-120).** This minor was "the governing body" from 2026-08-03 to
> 2026-08-10. Ben: *"Let's place the player as a **national private militia**, which uses private
> companies to build equipment for space."* The militia **replaces** the governing body — Ben's
> explicit ruling, taken against the alternatives of *an early reading of it* and *a contractor
> to it*. Everything below is being re-specified against the militia in **Sprint 8**, and the
> prose in this section still carries governing-body assumptions wherever it has not been marked.

**What the militia is, and what it is not.** A national private militia is armed and national in
allegiance, but it is **not the state**. It has no legislature and no ministry of science. Its
agency is **procurement and force**: what it can buy, from whom, at what price — and what it can
field with what it bought. The private companies are **counterparties**, not the player: an
arm's-length supplier with a price and a possible refusal. That is a different object from
BL-094's settled *shared treasury / prototype-of-1* call, in which the chartered corp was the
player's own economic arm — **that call does not survive the refocus unamended**, and Sprint 8
owns replacing it.

**What this changes about the hinge.** The old framing made this the hinge from *economy sandbox*
to *grand strategy*, on the argument that law, policy and science become instruments the player
wields. The militia does not wield them. The hinge is narrower and more concrete: from *economy
sandbox* to **an actor with a mission it must equip itself for**, where the economy is the supply
market it negotiates in and Conflict is what the equipment is for. **Conflict is still the pillar
this minor makes load-bearing** — arguably more directly than before, since force is now the
player's *purpose* rather than one of several levers it acquires.

**Three questions Sprint 8 must answer** (NR-120 § still_open), each of which changes what lands
here: is the militia **one of the ~43 generated nations** or an entity attached to one (nation
generation has no militia concept today); **what replaces the shared treasury** once companies are
counterparties, since the player needs money the companies do not have; and **what replaces the
retired design test** as the band's litmus.

**The conflict spine (BL-315, filed 2026-08-07).** BL-094's own promotion cost named a "new
conflict/military design item" as owed; the 2026-08-07 military design session filed it —
design-owed, priority A. **Its title still names the superseded actor**, and it is promoted by the
refocus from a follow-on to *the* item: the conflict spine is the militia's whole reason to exist,
not a subsystem it acquires. It is what the militia *commands*: force raised and paid for,
materiel **bought rather than internally drawn**, supply lines as live targets (CONCEPT.md §
Combat) — the item that turns `resolve_battle` (BL-272, already shipped) from an Era −1 research
tool into a campaign mechanic. Sprint 8 designs it.

**Basic AI rivals graduate from corp-level to nation-level here.** *(Written against the
governing-body framing; the symmetry argument holds under the militia, but "the same
governing-body layer" needs re-reading as "the same layer the player runs" — Sprint 8.)*
v0.2.0's scored-utility AI
(BL-202/203) contests Trade alone, one corp at a time. The old container for this, **BL-054**
(nation behaviour), was closed 2026-08-07 (NR-075 cut audit) and its parts redistributed — tax/
licences to BL-155, sentiment to BL-158, territorial fragmentation to BL-218/BL-284 — leaving one
residual: *the runtime nation actor*, which BL-054's own closure note says **BL-094 will
re-specify from the new player identity**, not inherit as-is. Symmetry still holds — every AI
nation needs to run the same governing-body layer the player does — so v0.3.0 is where a rival
first contests **Conflict**, not only Trade; it is just specified fresh here rather than promoted
from the old stub. That graduation is the "basic AI rivals" bar this roadmap pass is aimed at.

**Groundwork folded in here: the Era −1 sandbox** (named 2026-08-08, closing the gap NR-076
flagged 2026-08-07 — roughly 15 items, most landed or landing since 2026-08-02, bot-only and
never in the shipped campaign path). A year-tick history sim from 0 CE to the campaign epoch
(**BL-271**, first slice landed 2026-08-04) proves out, ahead of building any of it against real
campaign play:
- **the combat engine** — `resolve_battle` (**BL-272**, complete) over a class-matchup ×
  formation-doctrine × terrain × supply × season model, with era-keyed unit rosters (**BL-274**)
  and a filed strategic layer above it (**BL-277** — where armies go, when a nation sues for
  peace);
- **a diplomacy seam** (**BL-297**) — nation-level word-interface verbs entering through the same
  command-queue-at-tick-boundary pattern as `corp_command`, kept out-of-process per
  `AI_OPPONENT.md` § 10, with its own test battery (**BL-298**) required before any LLM touches
  it; Project-Rival is independently exercising this seam with a phrasing/voice-dictionary layer
  (`Project-Rival/docs/ai/PHRASINGS.json`, `VOICES.json`);
- **the ancient tech ladder** (**BL-296**) — a 0 CE → 1960 endowment-gated tech spine, distinct
  from the player-facing tree, whose *grain* — a constellation, not a binary tree — the
  concurrent BL-087 work below has already adopted.

**BL-271's own contract governs what transfers:** only *architecture* graduates into the 1960
era — the combat engine, the diplomacy seam's shape, the tech-web grain — never the
Rome-calibrated tuning constants; the sim "dies into (or is absorbed by) the 1960 era when those
systems graduate." Its output feeds BL-315 directly — the doctrine presets and four-era-band
unit tables authored 2026-08-07 (`Project-Rival/docs/ai/UNITS.json`) are numbers BL-315 inherits,
not invents from nothing.

**BL-087 (Era 1 tech/quest system) sits here too, not v0.1.4.** Reframed 2026-08-04 as a
constellation (rings = bands, sectors = domains, keystones opened by one-time **deeds**),
overturning the earlier binary-tree call; two regions — the Era 1 tree itself and "the
industrial neighbourhood" — are worked at that grain as of 2026-08-05. v0.1.4's BL-156 stays the
early stub pass; BL-087 is the real system, and the two are converging rather than strictly
sequencing.

*Roster reconciled 2026-08-10 (NR-101).* The Era −1 arc was described here in prose while its
items sat on `post-v0.1.0`, so a query of the backlog could not see the minor this section had
already assigned them to. Now explicit — the six named above (**BL-274**, **BL-277**, **BL-296**,
**BL-297**, **BL-298**, **BL-300**) plus the rest of the sandbox: logistics with real terrain and
a supply burden that grows with the empire (**BL-316**), the shared-currency scorer that unstalled
the incommensurable-verb problem (**BL-318**), the works roster — the pre-history building noun
that was simply missing (**BL-321**), the sim's own runtime budget (**BL-320**), the
two-great-powers sandbox seed (**BL-299**), and the calibration of the judgement-set voice and unit
numbers against sweep evidence (**BL-337**). Three more join them on their content rather than on
this section's prose: **BL-317** (the wizard's prehistory timelapse, which *consumes* BL-271's sim
rather than duplicating it), **BL-314** (the unit verb family, which belongs with BL-315's conflict
spine — it was filed against v0.1.5's stub and waits on a seam that exists only here), and
**BL-182** (corporate borders, whose real content is an *operate-gate*: a permission over where a
corporation may act, which is a thing **granted** rather than a thing a corporation has — under
the refocus the grantor is the nation the militia serves, and an operate-gate the player must
obtain is a sharper object than one it issues). The generation-flavour tail this section already flags as cut candidates — **BL-209**,
**BL-289**, **BL-300**, **BL-301** — is versioned here too, so that Ben's ruling cuts items with a
home rather than items with none.

**Three scope calls stay open** (NR-076, still unresolved as of this pass): cutting BL-160 and
letting BL-161 (counterparty allow/deny) stand alone; cutting or parking BL-207 (persona counsel
packs) behind BL-279's local-model evidence; cutting the generation-flavour tail (BL-209, BL-289)
while keeping BL-300/BL-301 as notes only. All three are Ben's to rule on.

### v0.4.0 — Politics + the filter system

> **PARKED 2026-08-12 (NR-177) — space arc.** Items carry `parked: true`, version goals intact.

*Theme: the political layer for real, and Era → Filter.* Two coupled deliverables:

- **Politics.** Promote the v0.1.x political stub into a working layer — the nation's political
  character, its relationships, and the levers the player-as-nation actually pulls. **Its
  generation-time substrate** is a ~7-item, still-unsequenced cluster (`post-v0.1.0` in the
  backlog, named here 2026-08-08): culture regions replacing Voronoi BFS as the ground nations
  grow over (**BL-239**), the industrial-disciplines-the-sovereign history-ladder stages
  (**BL-222**), the averted-rupture diplomacy origin (**BL-223**), the non-hegemony invariant as
  a measured harness assertion rather than lore (**BL-224**), the evolution-to-sapience gate
  chain that sets industrialisation timing (**BL-238**), the cross-pipeline sweep tuning
  (**BL-240**), and a State-Arsenal charter gate touching BL-094 directly (**BL-311**,
  design-owed). None of these ship campaign content on their own; they generate the political
  character this layer promotes into something real. *(Sequenced 2026-08-10, NR-101: all seven now
  carry `version_goal: v0.4.0` in the backlog, so this paragraph and a query finally agree. Two
  more join them — **BL-225**, asset seizure costing the seizing nation in sentiment and credit
  access, which is the one runtime mechanic `HISTORY.md` asks for and lands squarely on this
  layer's relationships; and **BL-210**, the oral-history generation umbrella that BL-238 and
  BL-240 are already working inside.)*
- **The filter system (Era → Filter).** Rename and reframe **Era** as **Filter**: the world-state
  gate governing what content is available when (**BL-087**'s catastrophic-event / quest-tree model
  re-read as a *filter* over the world). A terminology change with reach — `ERAS.md`,
  `GLOSSARY.md`, the era enums, and any `era_*` symbols — folded into the work when it lands, not
  ahead of it (authority time-slice). *(Naming watch: "filter" sits near the map-lens vocabulary in
  `LENSES.md`; confirm the two read as distinct before the rename lands.)*

### v1.0.0 — The playable game

> **PARKED 2026-08-12 (NR-177) — space arc.** This remains the space game's bar, held for the
> DLC resume. The **ancient product's** commercial bar (Steam/itch, paid — NR-177 ruling 4) is a
> different cut whose done-definition is owed in the live arc, not a reading of this one.

*Theme: everything above coheres into one game.* Named 2026-08-08, at Ben's direction, as the
terminal cut past which the four minors above stop being separate arcs and become one playable
whole. **Not a fifth pile of new systems** — Ben's framing on naming it: reachable "by following
current steps," i.e. it is what v0.1.x through v0.4.0 *add up to* once each lands, not a new arc
invented on top. Its own done-definition is below, mirroring v0.1.0's.

The bar, in one line *(restated 2026-08-10, NR-120)*: a **national private militia**, contesting
**both Trade and Conflict**, against **AI rivals doing the same** — corp-level (v0.2.0) and
nation-level (v0.3.0) — where the player's **procurement of force from private suppliers** is the
mechanism that couples the two pillars, rather than trade and war running as separate games.

---

## Done-definition — v0.1.5 (military systems, cut by Sprint 9)

*Written at the cut, per NR-103's fix — a theme with no done-definition has no test for finished.*

v0.1.5 is **the militia's first buildable surface**, not campaign-scale Conflict (BL-315 stays
post-cut scope). Cut when all of the following hold:

- **A muster building exists and gates hiring.** `military_base` (BL-325 S1) places like any
  building; hiring a unit (BL-324) requires a target tile carrying the corp's own COMPLETED base —
  hire-anywhere is retired (BL-325 S2).
- **The player starts equipped.** Generation seeds the player corp with one base and one unit on a
  home-province tile, deterministically (BL-331 — found already landed under a mislabeled commit,
  `2eb8654`, and verified rather than rebuilt this sprint; see `tools/verify/world_audit.cpp`'s
  new BL-331 PASS/FAIL check).
- **Rival corps can reach the same surface.** `corp_ai` proposes a `military_base` build the
  moment a rival corp holds none — gated on BL-344's tech (E0-ML-01), competing on merit in the
  same nice_to_have bucket as extraction/processing — so a background corp is not permanently
  locked out of hiring the way it was before this sprint (a real gap the S2 precondition exposed:
  the existing build-candidate loop never proposed `military_base` at all).
- **Out-of-supply decay is designed but not required for the cut.** BL-325 S3 (unit strength
  decay beyond the logistics reach field) stays open — the muster/hire surface is complete without
  it, and S3 is independent of S1/S2 by the item's own sequencing.
- **Military points and the research building are designed, not built.** BL-332 answers the
  resource-vs-rate, points-vs-deeds and who-else-accumulates questions Ben raised at filing, but
  its actual build re-versions to **v0.1.14** (Sprint 10) alongside BL-340 — both are new
  stockpiled-quantity plumbing, cheaper landed together than duplicated.

**What this cut does NOT claim.** `hire_unit` was not positively observed firing end-to-end for a
rival corp within `ai_skill_harness`'s five-seed, 300-tick window — construction on the tiles the
new candidate picks can stall indefinitely if the local market carries no steel (BL-095's
pay-as-you-build model, a pre-existing property this sprint is the first thing to actually
exercise via `corp_ai`'s own build path in this harness). Recorded as an open finding rather than
forced to pass by tuning a score; see `ai_skill_harness`'s 2026-08-10 golden-band comment and
NEEDS_REVIEW.json.

---

## Done-definition — v0.1.0 (the prototype cut)

v0.1.0 is the **economy loop, validated and playable end-to-end** — not the full game. It remains
the **prototype cut**; the expanded-prototype milestones above (v0.1.x → v0.4.0) stay theme-level
and earn their own done-definitions as they firm up. **v1.0.0, below, is the one exception** —
named and given a done-definition 2026-08-08, since it is the "full game" this cut deliberately
excludes. v0.1.0 is cut when all of the following hold:

- The player can **construct and manage** buildings — placement with cost and validation,
  recipe and workforce control — not merely observe authored assets. *(Shipped v0.0.7.)*
- **Population centres** ground workforce supply via habitability feedback, and the player can
  read why workforce is constrained and make siting decisions accordingly. *(Mechanics shipped
  v0.0.7; legibility surface shipped v0.0.8 — BL-069.)*
- Goods **move between bodies** via supply convoys, so **price diverges spatially** and
  logistics affects margin. *(Shipped v0.0.7.)*
- The player must **survey** a body to reveal its tile map and deposit profile — discovery is
  gated, not omniscient. *(Shipped v0.0.8 — BL-067.)*
- **Corporate intelligence is appropriately scoped**: competitor buildings are visible on-canvas;
  their production rates and stockpiles are private; market prices and supply/demand aggregates
  are public — the player reasons from market signals rather than reading off a ledger. *(Shipped
  v0.0.8 — BL-068.)*
- The **full budget** is legible — income vs. expenditure itemised, debt interest visible, runway
  readable — so the player can act before bankruptcy rather than discover it. *(Shipped v0.0.8,
  pulled forward from v0.0.9 — BL-072/073/074.)*
- The **read surfaces** — the ledger family, hover cards, trend plots, and the lens strip —
  make stockpiles, markets, balances, workforce, and construction legible at a glance. *(Mostly
  shipped v0.0.7–8; the remaining rough edges cleared in v0.0.9.)*
- The build is **green** and the loop is **verified** (headless economy/generation harnesses
  and visual capture checks).
- **Performance and data growth hold** (the v0.1.0 audit): frame and econ-tick budgets met,
  and counts/memory plateau over a long idle run rather than creeping.
- Excluded throughout, by scope: Conflict, Research, Policy, and Diplomacy beyond the
  data-model stub.

When these hold, the prototype has validated what it set out to validate, and v0.1.0 is the
milestone that says so.

---

## Done-definition — v1.0.0 (the playable-game cut)

v1.0.0 is **the whole game, not the economy loop alone** — the synthesis of v0.1.x through
v0.4.0, reached by the arc already mapped above rather than by inventing new scope. Forward-facing
like the rest of the arc past v0.1.0 (direction, not committed line-items) but concrete enough to
test against, the way the v0.1.0 list above is. v1.0.0 is cut when all of the following hold:

- The player plays as a **national private militia** — armed, national in allegiance, and *not*
  the state — whose agency is **procurement and force**: what it can buy, from whom, at what
  price, and what it can field with it. Private companies are **counterparties with a price and a
  refusal**, not the player's own economic arm. *(BL-094 as rewritten in Sprint 8, v0.3.0.)*
  **Amended 2026-08-10 (NR-120)** — the previous bar read "a governing body … with a chartered
  corporation as its economic arm", which the refocus replaces.
- **Procurement is a real negotiation, and space is what it is for.** The militia contracts for
  military materiel and space hardware, on a market where a supplier can decline; the Era 0 → Era 1
  gate (rocketry + launchpad + propellant) is the thing it is equipping toward from the opening
  tick, not a distant unlock. *(v0.1.14; BL-340's resource tiers; ERAS.md.)*
- **AI rivals contest both pillars.** The corp-level scored-utility AI (v0.2.0) still runs Trade;
  nation-level AI — the runtime-actor behaviour BL-094 specifies fresh, the residual of the
  now-closed BL-054 — now runs Conflict too, through the same conflict spine the player commands
  (BL-315) and the combat engine proven in the Era −1 sandbox (BL-272).
  Rivals are **beatable and legible** — the standing `AI_OPPONENT.md` goal — not merely present.
- **Law, policy and science bear on the militia as conditions it operates inside** — an embargo it
  must route around, a licence it must hold, a technology it can buy the output of — rather than as
  levers it pulls. *(Amended 2026-08-10: the previous clause read "law, policy and science reach
  military outcomes", BL-094's design test, which retired with the governing-body framing.
  Sprint 8 authors the replacement test; until it exists this clause states the direction, not the
  test.)* Satisfied once the v0.1.3–v0.1.6 stubs (laws BL-155/186, tech BL-156/BL-087, military
  BL-157/BL-314/BL-315, politics BL-158) have graduated into real systems rather than stayed
  placeholders.
- **The political layer is real** (v0.4.0) — nations carry working political character and
  relationships generated from actual settlement/culture history, not a random draw, and the
  player has standing within it — for a militia, the relationships that decide who will sell to it,
  who will contract it, and who will move against it.
- **The filter system is live** (v0.4.0, Era → Filter) — content gating reads as one coherent
  model end to end, not a leftover Era enum beside a newer quest-gate concept.
- **The player can tell whether they are winning** — a standing, not a verdict (CONCEPT.md
  forbids an end-game score), legible against rivals whose internals stay appropriately private
  under the existing visibility rule. *(BL-262.)*
- **The word interface covers every pillar**, not only the economic one — the diplomacy verbs
  (BL-297) and military verbs (BL-314) a text-driven player or the local-model opponent
  (`AI_OPPONENT.md` § 10) would need exist alongside the economic verbs BL-270/206/278 already
  shipped, so the AI-opponent architecture doesn't bit-rot as new pillars land.
- The build stays **green and deterministic** throughout — nation-tier commands extend the
  `corp_command` seam pattern rather than inventing a parallel one, preserving the
  multiplayer-lockstep property the seam was built to hold (`AI_OPPONENT.md` § 6).
- Excluded, by scope, unless a later Ben call moves it in: the small-local-model runtime opponent
  running unattended by default (§ 10 stays corpus-generation-and-design at this bar — a working
  local model is a stretch goal, not a gate); live multiplayer netcode (the seam exists, the
  network layer does not); and economic/political depth beyond the "basic" bar this roadmap names
  (`POPULATION.md`'s deferred scale mechanics and similar).

This list is written **forward**, the way v0.1.x → v0.4.0 already are — it will firm up, and
probably split by minor, as each one is actually reached. Read it as the target the map above is
aimed at, not a frozen spec.

---

## Sequencing — live in SPRINTS.md and REFINED.md

*Superseded 2026-07-31.* The ~100-line publish plan that lived here (last updated 2026-07-04)
is retired: everything it sequenced — the v0.0.6–v0.0.8 sessions and the v0.0.9 polish batch —
was delivered in full (per-item record in `DEVLOG.md`; roll-up in § Where we are). Its two
boundary-queued holdouts resolved too: planetary logistics (BL-077) and the economy-dynamism
pair (BL-078/079) shipped, and BL-011 (reach/logistics lens) — listed there as parked — is
complete.

Live sequencing no longer belongs in the roadmap. The weekly goal/retro rhythm is
[`SPRINTS.md`](SPRINTS.md), the active worklist is [`REFINED.md`](REFINED.md), and the method
(Batch Delivery, collision maps, worktrees) is [`DELIVERY.md`](DELIVERY.md). This document
stays theme-level.

**Amended 2026-08-10 — one table, and why it is here.** § The arc from here now carries a
sprint → minor table. That is a partial walk-back of the rule above, taken deliberately and
bounded: **NR-102 (sequencing decoupling)** records the gap NR-101's sweep left behind — every open
item names a minor, but nothing said in what order to build them — and a roadmap that names themes
while the sprint log names order lets the two drift until a retro notices. Sprint 1's retro caught
exactly that drift, and this file has now committed it twice. The bound is that the table names
**which sprint cuts which minor and nothing finer**; items, tasks and collision maps stay in
SPRINTS.md and REFINED.md, and the table is amended when a goal changes rather than at the retro.
