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
| 8 | Who the player is (design only) | **Closed 2026-08-10** — BL-094 rewritten as the militia, BL-350 filed, v0.3.0 roster reconciled |
| 9 | The militia takes the field | **Closed 2026-08-10** — v0.1.5 cut; BL-325/BL-331 landed, BL-332 designed and re-versioned |
| 10–14 | Re-sequenced 2026-08-10 (the living world inserted) | **Overtaken** — Sprints 10/11 closed 2026-08-11; Sprints 12–14 superseded 2026-08-12 by the 0 CE refocus (NR-177), never opened — their minors parked with the space arc |
| 15 | The 0 CE refocus (retro-recorded) | **Closed 2026-08-12** — the tangent that became the product; epoch 0, 3× map, Era −1 sim wired in, mercenary seam designed; see below |
| 16 | The mercenary vertical slice | **Open 2026-08-12** — BL-377 playable end-to-end + BL-315 on the critical path; cuts v0.1.15 |
| 18b | Roster invariants (retro-recorded) | **Closed 2026-08-16** — BL-432 landed; BL-435 paused 4/6; BL-436 filed |
| 19 | The economy tells the truth | **Closed 2026-08-17 — goal NOT met.** The blame moved three times and landed on supply; goldens left red and unblessed |
| 20 | Not yet chosen | **Proposed 2026-08-17** — three candidates laid out below; Ben's call |

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

# Sprint 15 — The 0 CE refocus (retro-recorded 2026-08-12)

**This entry is written after the fact, and that is its first finding.** The refocus work ran
across three sessions (2026-08-12) with no sprint entry — the exact drift this file's standing
caution names (*amend when the goal changes, not at the retro*), committed for a third time. The
goal is reconstructed from NR-177's ruling; the retro is real.

**Goal (reconstructed).** Refocus the game to 0 CE per Ben's ruling: *"our project is too grand
to be able to really feel out the gameplay."* The ancient era becomes a standalone commercial
product — the mercenary company as player, tile-and-fine-tick grain, the space work stashed for
DLC.

**Retro.**

- **Landed:** the 0 CE epoch default; the 3× map (312×145) with a physical tile scale and
  travel time; the Era −1 sim wired into campaign generation (400 pre-epoch years — 62 battles,
  16 conquests, ~1100 foundings); the stepped decision clock; three scorer fixes including the
  supply-hubs change; async generation + the loading screen with its inner bars; the
  startup-"crash" diagnosis (AppHangB1, never a crash — NR-209) and its fix package (persona
  counsel out of the warm start, the warm start sliced across frames, two logistics scaling
  fixes — warm start ~90 s → ~6.5 s); crash logging; the zoom-out cap; **BL-377** (mercenary
  contract seam, designed) and **BL-378** (minimap base render, designed).
- **Filed:** BL-398 (persona counsel tick cost), NR-177, NR-205–NR-210.
- **Slipped:** nothing — but only because nothing was planned, which is the point of recording
  this entry at all.
- **Runtime:** not tracked (fourth consecutive entry; the format's Runtime line remains
  uncollected).
- **Feedback:** the tangent produced more product-defining change in one day than the two
  planned sprints before it — which is fine exactly once. The two-arc rewrite (this entry's
  sibling changes in ROADMAP.md and backlog.json) is what makes it fine: the tangent now has a
  band, a next cut, and a parked counterpart, instead of being an ever-growing exception.

---

# Sprint 16 — The mercenary vertical slice (opened 2026-08-12)

**Goal.** The loop that makes the ancient product a game: a polity hires the company, the company
fights, the company is paid — playable end-to-end, however rough. Cuts **v0.1.15**.

**Planned.**
- **BL-377 — mercenary contract seam** (`designed`): offers, acceptance, the contracted fact,
  payment. The buy side exists (`procurement.cpp`); this is the sell side's first surface.
- **BL-315 — conflict spine** (`design-owed`, A): promoted from v0.3.0 groundwork onto the
  critical path — a mercenary company's core loop is built entirely on Conflict, and nothing in
  the campaign layer commands `resolve_battle` today. Design first, against the Era −1 machinery
  that already fights wars in generation.
- **BL-378 — minimap base render** (`designed`): the company's home, always on screen.
- Riders as room allows: **BL-398** (persona counsel tick cost — the live-play hitch).

**Risk.** BL-315 is the unknown: it is design-owed, priority A, and the item the whole sprint
leans on. If it sprawls, the fallback is BL-377 against the *existing* battle resolution with the
thinnest possible command surface — a contract you accept whose battle resolves itself is still a
loop; a designed-but-unbuilt spine is not.

**Amendment 2026-08-15.** Three of the four planned items are `complete` — **BL-377** (mercenary
contract seam), **BL-378** (minimap base render) and **BL-398** (persona counsel tick cost). The
sprint then drifted: the sessions that followed went to v0.1.16 work (spectator mode, the decision
feed, gate hygiene) rather than to the two items that make the slice *play*. **BL-315** (conflict
spine) is now `designed` rather than design-owed, so the sprint's stated risk did not materialise —
it simply was not built. The remainder is re-planned as **Sprint 18** below, and v0.1.15 stays
uncut until it lands.

---

# Sprint 17 — Economy breadth: the chain is the growth track (opened 2026-08-15)

**Goal.** Give the ancient economy enough building options, production methods and *depth* that
progress is something the player builds rather than something a system grants. Cuts a new minor,
**v0.1.17**. Ben, this date: *"make sure the economy has robust and expansive building options,
production methods, and an implied growth track that we can refine tech and politics around."*

**The four rulings that shape it** (2026-08-15, elicitation form):

1. The growth spine is **chain depth** — not building tiers, not the tech ladder, not settlement
   scale. How far down the production graph a corp can reach is what gates its next building.
2. The roster goes **past 20 building types**, as a content sprint of its own.
3. Buildings offer **alternate production methods with real trade-offs** — not tier upgrades.
4. **Economy first**, military follows.

**What the measurement found before any work.** `scripts/economy.lua` carries **7** `building_type`
entries and `scripts/recipes.lua` carries **17** recipes, one per output, no alternates — so
choosing a building *is* choosing its method today. One of the seven is the space-era Launchpad and
the refinery chain runs on petroleum, inside a product set at 0 CE. And the ancient substrate is
already in the enum and mostly idle: `stone`, `timber`, `sand`, `clay`, `peat`, `charcoal`,
`iron_blooms`, `grain`, `fodder`, `salt`, `bullion` are BL-286 values whose behaviours were never
filed. The sprint's cheapest and most honest version is to give those orphans consumers rather than
mint new resources.

**Planned.**
- **BL-428 — chain depth is the growth spine** (`designed`, A): the depth metric off the recipe
  graph, what it gates, and the acyclicity it requires. Foundation — everything else reads it.
- **BL-433 — space-era buildings in the ancient arc** (`designed`, B): *answer this first.* It
  carries an open a/b/c on how the two arcs share one data set, and BL-429 writes into exactly
  those files.
- **BL-429 — ancient building roster** (`designed`, A): 7 → 20+ named buildings on ~3
  `building_type` values, following the BL-166/BL-168 pattern (recipe + placement rule, not a new
  enum value). Every ancient orphan resource gets a consumer.
- **BL-430 — alternate production methods** (`designed`, A): several recipes per output, with the
  design bar that no method may dominate a sibling on every axis.
- **BL-431 — production-method and chain UI** (`designed`, B): the method selector, the chain
  trace, the depth readout. Without these the content is a data file the player cannot see.
- **BL-432 — economy-breadth guard harness** (`designed`, B): the four invariants mechanised —
  acyclic graph, no orphan resources either direction, every building reachable, no dominant
  method.

**Risk.** Content sprints sprawl, and this one has a real dependency chain (BL-428 → BL-429/430 →
BL-431). The order above is the mitigation: depth and the era question settle first, and the roster
is built against a metric that already exists rather than alongside one. If the roster runs long,
**BL-431's UI is the part that must not be cut** — 20 invisible buildings are worth less than 12
legible ones.

**Watch for.** `corp_ai.cpp` scores build candidates over whatever roster it is handed. A 20+
roster and a method choice are both new decision surfaces for the scorer; BL-430 names the call
explicitly rather than letting a default pin every rival to the same method.

---

# Sprint 19 — The economy tells the truth (opened 2026-08-16)

**Goal.** Make the economy's own numbers mean what the design says they mean. Ben's call this
date, choosing this over the military surface: the last two sprints built chain depth, alternate
methods and a corp-selection screen **on top of a substrate that rewards the opposite of what they
teach**, and that is worth fixing before more is stacked on it.

**The measurement that opened it** (2026-08-16, BL-435 task B — a clean natural experiment, since
only one entry in `focus_asset_pattern` moved and seeds/tiles/recipes/prices were all held):

| | before | after |
|---|---|---|
| player corp, seed 0 | 3 extraction / 0 processing — **55,179 cr** | 2 extraction / 1 processing — **28,288 cr** |
| `ai_skill_harness` final net worth | — | **−71 / −37 / −9 / −20 / −38 %** across five seeds |

A mine outearns the refinery fed by it. That is backwards for the value-add step the Trade pillar
rests on, and it quietly contradicts three shipped systems at once: **BL-428** asks the player to
climb a ladder that costs money at every rung, **BL-430**'s alternate methods become a choice
between ways of losing income, and **BL-435**'s selection screen presents a processing-heavy corp
as the better opening when it is measurably the poorer one.

**Planned.**
- **BL-436 — processing underearns extraction** (`designed`, A, d3): the keystone. Four candidate
  causes are already recorded on the item and are to be **measured before anything is tuned** —
  input cost vs output price, wages/maintenance, market depth for the inputs, throughput.
- **BL-417 — the build score is quadratic** (`designed`, A, d2): `net / payback` collapses to net²
  over capex, and the tuning was calibrated against that curve without anyone deciding it should be
  there. Reads directly on BL-436: if the scorer's curve is wrong, "the AI prefers mines" is
  evidence about the scorer rather than about the economy.
- **BL-404 — the inter-body pull is unnetted** (`designed`, A, d2) and **BL-406 — the home market
  is an arbitrary pick** (`designed`, A, d4): BL-404 is blocked on BL-406, so BL-406 leads. Both
  are demand-side truth defects in the same seam.
- **BL-422 — a held sell order still credits market inventory** (`designed`, B, d2): phantom
  supply distorts every price the above are measured against, so it is worth clearing early.

**Sequencing.** BL-436's *measurement* first, before any fix — it may well reassign the blame, and
BL-417/BL-422 are both plausible contributors rather than neighbours. BL-422 and BL-406 are
independent of it and can run alongside. **BL-404 waits on BL-406.**

**Done when** a processing facility fed by its own inputs outearns the extraction site it replaced
over a representative rollout — *or* there is a written, deliberate reason why it should not,
propagated into PRODUCTION.md. Either closes it; the current state, where the arrangement is
accidental and undocumented, does not.

**The check that says it worked.** `ai_skill_harness`'s bands were re-blessed DOWNWARD on
2026-08-16 and the file says why in as many words: *when BL-436 lands these numbers should rise,
and a bless that does not raise them means the fix did not work.* That is this sprint's success
criterion, already written down before the work starts.

**Risk.** Every item here moves economy numbers, so goldens will move repeatedly. The discipline
that made this session's blesses honest — confirm reproducible across two runs, record the cause in
the file, never bless before the cause is understood — is the thing to keep, and the thing most
likely to erode across four items in one sprint.

**Carried, not dropped.** BL-435's tasks E and F (paused 4/6, resume note in REFINED.md), and the
military engagement surface below — now deferred a **third** time. Name that at this sprint's
retro rather than letting a fourth accumulate silently.

**Progress (amended as it happens, not at the retro).**

- **BL-436 (processing underearns extraction)** — measured, part-fixed, **open on a Ben-owned
  call**. The rate asymmetry is fixed but *disabled* (`richness_reference = 0.0`) because the
  cost side was calibrated against the old inflated income; a scale sweep killed the obvious
  hypothesis (raising extraction income makes the collapse worse). Also produced **BL-437**
  (landed) and a corrected measurement instrument.
- **BL-422 (held sell order phantom inventory)** — **landed 2026-08-16**. Bigger than filed:
  `inventory` is what processors actually draw inputs from, so a held order's phantom stock was
  bought and never paid for — goods from nothing, money destroyed. Fixed as a conservation law
  (inventory gains exactly what pools lose), guarded by `order_book_harness` R7 with three rows
  proven to fail pre-fix, and it took a latent `unordered_map` float-accumulation nondeterminism
  with it. **Byte-identical in the five AI benchmark seeds** — which says the benchmark never
  reached the case, and is a warning for BL-436's calibration (NR-263).
- **Still to run:** BL-417 (build score is quadratic), BL-406 (home market is an arbitrary pick)
  → BL-404 (inter-body pull unnetted).

---

## Sprint 19 retro — the blame moved three times, and the goldens are red (closed 2026-08-17)

### The done-when was NOT met, and the success criterion inverted

The sprint's done-when: *a processing facility fed by its own inputs outearns the extraction site
it replaced, or a written deliberate reason why not.* Neither holds.

Processing reads **−10.44** per building-tick against extraction's **+7.82**. There is no
deliberate written reason, because the reason turned out to be a defect chain rather than a design
call.

The named success criterion inverted outright. `ai_skill_harness`'s bands were re-blessed downward
on 2026-08-16 with the file saying *these should RISE when the fix lands*. They **fell** — three of
five seeds insolvent, corps at **−2M to −3.4M** final net worth.

**The goldens are deliberately left red and unblessed.** That is this sprint's defining event, and
it is the criterion working rather than failing. Blessing here would have recorded bankruptcy as
the expected state.

### The blame was reassigned three times in two days

Nothing else about the sprint matters as much as this sequence. Each stage was measured, and each
measurement killed the previous story.

| Stage | The story | What measurement said |
|---|---|---|
| 1 (opened) | the scoring curve — `net/payback` is net²/capex, so the AI prefers mines | **wrong, and not even reachable.** `corp_ai.cpp` had **no** `processing_facility` build candidate at all (NR-265). A rival owned only the processors it was *generated* with, for the whole campaign. Retuning a curve that only ranks mines against mines could never have produced a processor |
| 2 | the richness rate ratio — a mine's rate is multiplied by raw deposit richness (mean ~53), a processor's by nothing | **right, and fixed.** Extraction fell 1666.69 → 14.53 revenue per building-tick, capex payback 0 ticks → 12. Two tiers finally at the same order of magnitude |
| 3 (where it landed) | with the ratio fixed, price is the binding constraint — reprice refined goods +72% | **wrong to act on.** Seven wanted recipe inputs cannot be mined **at all**. Coal binds 54% of starved processor ticks: 1,671 tiles carry it, **zero** sites target it, because `richest_extractable` gives a site only the single richest deposit on its tile (NR-272) |

Two corrections fell out of stage 1 that were worth more than the item that found them. BL-436's
own calibration narrative explained a collapse by *corps building processors with extra income* — a
mechanism with no middle link, since no corp could build one (NR-266). And BL-428's chain-depth
ladder, the growth spine Sprint 17 built, had **no AI player**: depth is climbed by operating
deeper processors (NR-267).

A retro that reads "processing underearns extraction, cause unknown" would be missing the sprint.
The cause is now named and it is **supply**, not price and not upkeep.

### What actually landed

- **BL-422 (held sell order credits phantom inventory)** — **complete**. Bigger than filed:
  `inventory` is what processors draw inputs from, so held stock was bought and never paid for.
  Refixed as a conservation law, and it took a latent `unordered_map` float-accumulation
  nondeterminism with it.
- **BL-417 (the build score is quadratic)** — step 1 landed, honestly. The "zero behavioural
  change" claim was **not** established when written; measured A/B on pinned MSVC and byte-identical
  by rounding luck, not by algebra (NR-268). Step 2 stays Ben's, and is now a smaller question than
  filed.
- **BL-439 (AI never builds processors)** — 3/4. The candidate is in, on the same score curve and
  gates as extraction. Rivals build 69 processors across the benchmark set, against 0. Task C, the
  golden re-bless, is **held** rather than skipped.
- **BL-440 (mines only target the richest)** — 2/4. `rank_extraction_sites` now emits a candidate
  per extractable deposit, weighted by unmet recipe demand. Task C is **held on a design fork**.
- **BL-437 (a site works every deposit on its tile)**, **BL-435 (no corporation is seeded that
  cannot make money)** — both landed, both filed and closed inside the sprint.
- **Instruments:** `tier_margin` was written, then found to be measuring a world with **no prices**
  — it loaded two of the three economy scripts. Corrected mid-sprint; every figure taken before
  that is suspect wherever prices are involved.
- **`richness_reference` enabled at the MEDIAN (24.9), not the mean.** The authored promise is "a
  typical deposit lands at ~1.0", and the mean sits at the 78th percentile.

### What this sprint is actually worth remembering for

**Every guard written this sprint was run against the pre-change build first, and shown to fail by
construction.** BL-439's R5 read `processors_gained = 0` on all five seeds before the change and
12/15/16/16/10 after. BL-440's R4b failed on seven resources. Three of BL-422's twelve R7 rows
failed pre-fix.

That is the direct answer to the two previous retros' findings — a check pointing at a deleted tab
and still passing, a harness reporting ALL PASS on code it never exercised. A guard that has never
been seen to fail is not coverage.

**And the same trap was still caught twice more this sprint.** `ai_skill_harness` hand-builds its
recipe registry and had fallen out of date with `economy.lua`; it reported ALL PASS on exactly the
code BL-436 changed. `tier_margin` was written *to diagnose the economy* and omitted the script
carrying the prices. A hand-built fixture drifts silently by default.

**Half of BL-436's starvation was a live-game defect, not a harness one.** The default-recipe
backfill lived inline in `app::load_economy` — a world-generation invariant depending on the UI's
startup sequence — and it ran *before* `generate_background_firms` authored more processors. Every
background firm's processor kept `no_recipe` for the whole campaign, paying maintenance and
reporting as ordinary idleness. Fixing it made the economics **worse** (−9.08 → −10.44) because the
starvation became visible. The defect was hiding the reason, not causing it.

### Where the method held, and where it did not

**Held.** Measure-then-act was applied at every fork and paid three times. The −3M net worths were
explained rather than tuned around: `k_debt_interest_per_quarter` 0.02 over 300 ticks is
1.02³⁰⁰ ≈ 380×, against per-building extraction nets of +5 to +11. Nothing was blessed, nothing was
repriced, and the reason is written down rather than assumed.

**Did not — the recurring three, all now overdue.**

1. **No sprint entry was kept live. This is the FIFTH consecutive sprint.** The previous retro said
   fourth. The Progress block above was amended once, on the sprint's second day, then not again
   across the three sessions that reassigned the blame twice. The standing caution — *amend when
   the goal changes, not at the retro* — is now the single most-repeated finding in this file.
2. **The Runtime line has been uncollected for SEVEN entries.** The previous retro said *"either
   wire `tools/session/timer.js` in or drop the line"*, and neither happened. **Recommendation:
   drop it from the format block.** Seven misses is not a tooling gap, it is a signal nobody wants;
   keeping a field that always reads "not tracked" trains readers to skip the retro's own checklist.
   If pacing is wanted later, derive it from commit timestamps, which are already collected.
3. **The military engagement surface is deferred a THIRD time.** Sprint 19's own block said to name
   that here rather than let a fourth accumulate. Named. Three deferrals all point the same way,
   and the argument for each was sound in isolation — which is exactly how a fourth happens.
   Sprint 20's choice below is where that stops or does not.

### Left for another session

- **BL-440 task C (generation sites use the same broken rule)** — held on a design fork, and it is
  the load-bearing half: `tier_margin` is byte-identical across tasks A and B, because a 3-seed
  20-tick run is dominated by *generated* assets and generation still calls `richest_extractable`.
- **NR-271's repricing (a/b/c)** — **blocked behind BL-440 (c)**. Every option was sized on a chain
  whose second input is unobtainable, so all must be re-derived.
- **BL-439 task C and BL-435 tasks E/F** — the golden re-bless and the corp-screen guard, both
  waiting on the same substrate settling.
- **BL-406 (the home market is an arbitrary pick) → BL-404 (the inter-body pull is unnetted)** —
  planned this sprint, never started.
- **NR-269's option (c)** — the debt-interest amplifier as its own item. A corp that cannot recover
  from one bad quarter makes every economy measurement noisier.

---

# Sprint 20 — the choice (drafted 2026-08-17, NOT opened — Ben picks)

Sprint 19 closed with its keystone open and its instruments finally honest. Three candidates, laid
out with cost and consequence. **This is a proposal, not a decision.**

### Candidate A — finish the economy thread (v0.1.17)

**The work.** BL-440 (mines only target the richest) task C is one design call from buildable — Ben
chooses between (i) a post-registry retarget pass and (ii) a static demand hint at world-gen time.
Then re-run `tier_margin`, re-derive NR-271's price options on a chain whose inputs exist, and
re-bless the red goldens with a cause.

**What it costs.** Two to three sessions, most of it measurement rather than code. One design call
up front, which is the actual gate.

**What it unblocks.** Everything currently parked: the repricing, BL-439's golden re-bless
(AI never builds processors), BL-435's corp-screen guard (starting-corp selection), and the honest
closure of BL-436 (processing underearns extraction). It also settles the substrate Sprints 17 and
18b built on, which was Sprint 19's whole argument for existing.

**The risk.** This is the fourth session in a row on the same substrate, and the blame has moved
three times already. It may move again.

### Candidate B — the military engagement surface (v0.1.15)

**The work.** Sprint 18 as written: BL-393 (units are write-only and inert), BL-315 (conflict
spine), BL-384 (the Era −1 sim conquers nothing), plus the unit UI half.

**What it costs.** The largest of the three. BL-315 is the item it leans on — designed, with
nothing built against it. Four to six sessions.

**What it unblocks.** The mercenary vertical slice, uncut since Sprint 16, and the only thing on the
board that makes the ancient product *play* rather than *tick*. A player who raised 25 units still
has nothing to do with any of them.

**The risk.** It starts on a substrate whose value-add step is measurably upside-down. Sprint 19's
opening argument applies unchanged.

### Candidate C — the debt amplifier alone (a short sprint)

**The work.** NR-269's option (c): make insolvency recoverable before more economy numbers are read
off it. `k_debt_interest_per_quarter` at 0.02 over 300 ticks compounds 380×, so a corp a few
thousand down ends at −3M and every rollout carries that noise.

**What it costs.** One session, possibly less. Smallest by a wide margin.

**What it unblocks.** Cleaner numbers for whichever of A or B follows, and a golden set that could
plausibly be blessed again.

**The risk.** It is not a sprint goal, it is a task. Running it alone spends a week's framing on
something that should ride inside A.

### Recommendation

**Candidate A, with Candidate C folded in as a rider — and Sprint 21 pre-committed to B.**

The reason is not that the economy matters more than the fight. It is that A is **one design call
from done** and B is **four sessions from its first playable tick**, and A's open half currently
blocks five parked items across three sprints.

Candidate C rides along because it is a day's work that makes A's own measurements readable.
Running it as its own sprint would be ceremony over a task.

**The honest counterweight, stated plainly: this defers the military surface a fourth time**, which
is exactly what three retros in a row have warned against. Naming a deferral is not the same as
fixing it.

If Ben's read is that the fight has waited long enough, **B is a defensible call and the economy
thread pauses cleanly** — BL-440's fork is written down and REFINED.md holds the resume note.

The one option worth arguing against is starting B *without* answering A's fork. It is cheap to
answer and expensive to leave open, because it silently invalidates every economy number taken
after it.

---

# Sprint 21 — The fight becomes reachable (proposed 2026-08-17, NOT opened — Ben picks)

**This is a proposal, not a decision.** Sprint 20's recommendation pre-committed Sprint 21 to
candidate B, the military engagement surface. This block is what B looks like once someone reads
the source rather than the plan — and the reading changes the shape substantially.

**Goal.** Close the gap between a battle resolver that works and a campaign that cannot start one.
Not "build combat": combat is built. Build the **reach** to it — a verb that moves a unit, a rule
that says who may be fought, a trigger that opens a battle, and a screen that shows it.

## What the audit found, because it re-frames the whole sprint

Sprint 18's block says BL-315 (armed house conflict spine) is *"designed, with nothing built against
it"*. That is now wrong in the direction that matters. `src/world/campaign_battle.{hpp,cpp}` is
**written, compiled into the shipping binary, and covered by `campaign_battle_harness`** — seeded
rounds, priced withdrawal, a measured uncertainty curve. It has **zero callers outside its own
harness**. The same is true of `resolve_battle`, whose only caller is the Era −1 sim.

So the sprint's unknown is not the resolver. It is everything that would ever call one, and none of
that exists at all:

| Capability | State | Evidence |
|---|---|---|
| Campaign battle resolver (rounds, seeded swing, withdrawal) | **BUILT** | `campaign_battle.cpp`, harness green |
| Era −1 battle resolver (`resolve_battle`) | **BUILT** | `combat.cpp`, `combat_harness` |
| Terrain defence / attrition scalars | **BUILT** | `terrain_combat.cpp` |
| Unit roster, era bands, corp-side gating | **BUILT** | `unit_roster.cpp` |
| `hire_unit` verb + AI use of it | **BUILT** | `corp_command.cpp`; observed 6–12 hires/seed |
| `military_base` building, hire gated on it | **BUILT** | BL-325 (military bases and supply) S1+S2 |
| Unit marker, Selection card, tile click-cycle | **BUILT** | `body_surface_canvas.cpp`, `selection_panel.cpp` |
| **A verb that takes a unit as its subject** | **ABSENT** | every `corp_verb` names a building, body or order |
| **Any hostility / standing model** | **ABSENT** | no corp may be "at war with" another, anywhere |
| **Any engagement trigger** | **ABSENT** | nothing detects two forces on one tile |
| **Battle state in the world** | **ABSENT** | `world` has no battle store; nothing steps a round |
| **Unit facts on the blackboard** | **ABSENT** | `export_corp_blackboard` has no unit section |
| **Out-of-supply decay** | DESIGN-ONLY | BL-325 (military bases and supply) S3, held behind BL-315 |
| **Unit verb dictionary entries** | DESIGN-OWED | BL-314 (unit verb family), waiting on a seam |
| Hire price on screen | ABSENT | BL-405 (hire has no price on screen), d1 |
| Demolish vs garrisoned units | **DEFECT** | `demolish_building` never touches `w.units` |

**The capability everyone assumes exists and does not is a HOSTILITY MODEL.** Every plan in this
file — Sprint 16, Sprint 18, Sprint 20's candidate B — talks about fighting, and no document owns
the question *who is allowed to fight whom*. BL-315 (armed house conflict spine) settled that army,
mercenary and pirate are three derived readings of one company, which is a **naming** rule and
answers nothing about targeting. Without it there is no legal predicate on an attack verb, no
trigger condition, and no way for the AI scorer to price an engagement. It is not written down as
work anywhere, and it blocks every other slice.

**Second finding, cheaper but real:** `resolve_battle` and `resolve_campaign_battle` both take
fully-resolved `army_stack_entry` values, and **nothing converts a live `unit_component` into one**.
`unit_component::strength` is written raw by all three of its writers despite a "fixed-point" doc
comment (NR-247). The adapter is small, but it is a real missing piece, and it is where that
inconsistency has to be settled rather than deferred again.

**Planned.**
- **BL-393 — units are write-only and inert** (`designed`, A, d3): the foundation, and now the item
  that must grow the `unit_command` seam rather than defer it. Its own text proposes leaving
  movement to BL-315; that split no longer helps, because BL-315's resolver is the half already
  built.
- **BL-315 — armed house conflict spine** (`designed`, A, d4): re-scoped by the audit to what is
  actually missing — hostility, movement, the engagement trigger, battle state in the world, and the
  stack adapter. The resolver itself is done.
- **BL-405 — hire has no price on screen** (`designed`, B, d1): rides in the UI slice.
- **BL-314 — unit verb family** (`design-owed`, B, d3): unblocks the moment the seam exists, and is
  transcribed from it, never authored ahead of it (BL-270's discipline).
- **BL-384 — the Era −1 sim conquers nothing** (`designed`, A, d5): 267 battles, zero provinces
  taken. Shares no file with anything above and can run from day one.

**Sequencing.** Two foundation slices run **alone and first**: the `unit_command` seam (F1) and the
hostility model (F2). F1 blocks every verb; F2 blocks every trigger and every AI score. After both
land, four slices fan out with no file overlap. BL-384 (Era −1 conquest) is independent of all of
it and starts immediately alongside F1. Full task decomposition and collision map: `REFINED.md`.

**Done when** a player can select a unit, order it onto a tile held by a hostile force, watch the
battle run round by round, choose to withdraw partway, and see the surviving strength on both sides
— with the same save replaying the same fight. Anything less leaves the resolver as unreachable as
it is today.

**The check that says it worked.** A new `tools/verify/engagement_harness.cpp` driving the whole
path headlessly: hire → move → contact → resolve, asserting (1) two runs of one seed produce
byte-identical battle outcomes, (2) a withdrawal at round 3 costs strictly more than one at round 1,
(3) no verb mutates anything on a rejection. Plus a `--verify` capture of the battle screen. The
determinism assertion is the load-bearing one: `campaign_battle` earns its seeded stream only if the
*caller* feeds it a stable identity, and the caller is exactly what this sprint writes.

**Risk.** The hostility model is a **design call, not an implementation**, and it is not filed as an
item. If Ben does not settle it early, F2 stalls and the four fan-out slices stall behind it — the
same failure mode as BL-440's open fork in Sprint 19. File it and answer it in the first session, or
the sprint's parallelism does not exist.

**Second risk.** BL-325 (military bases and supply) tail — out-of-supply decay — touches
`economy_system.cpp` and `scripts/economy.lua`, which is the seam three concurrent agents are live
in right now. It is **cut** from this sprint for that reason, not for scope. See § what this sprint
does not carry, in `REFINED.md`.

---

# Sprints 22–24 — Spectate, and the diplomacy it makes watchable (proposed 2026-08-17, NOT opened — Ben picks)

**These are proposals, not decisions.** Ben's brief this date asked for sprints aimed at spectator
mode, padding out rival diplomatic thinking, and ensuring commands are reachable by both AI and UI.
The brief offered a thesis: that those are one requirement, because spectate removes the human seat
and forces every corp through one seam. This block tests that thesis before building on it, and the
test changes the sequencing.

## The thesis, verified in one half and refuted in the other

**The half that holds.** Under `corp_ai_params::spectating` the player-corp prohibition is not
excepted but *unsubscribed* — there is no owner to protect, so `world::player_entity` evaluates on
the same staggered cadence as any rival. A spectated session therefore does drive every corp through
`apply_corp_command`, exactly as claimed. Spectate makes an unreachable command **consequential**:
a verb only the UI can issue is a capability the spectated world simply does not have.

**The half that does not.** Consequential is not observable. When a UI-only verb never fires,
nothing reports that it never fired, and the session looks fine. Spectate cannot see the UI axis at
all, by construction — there is no UI in it to check against.

**The evidence is in our own harness.** `tools/verify/spectator_determinism.cpp` buckets the fifteen
verbs into four families. `place_sell_order` alone satisfies the whole `trade` family, hiding the
four unemitted trade verbs behind it; `hire_unit` alone satisfies `other`, hiding `demolish` and
`place_road`. The harness written to prove spectate exercises the seam does so at a granularity too
coarse to notice that **seven of fifteen verbs never fire**.

So spectate is the right forcing function, and it is not self-checking. That is why Sprint 22 leads
with an instrument rather than with fixes, and it is the one structural change these proposals make
to the brief. Recorded as NR-305.

## The measured gap, which grounds all three sprints

Three artefacts already answer the reachability question and no tool joins them:
`src/world/corp_command.hpp` (the seam), `docs/ai/ACTIONS.json` (the dictionary), and each dictionary
entry's own `surface` field (the press). A fourth column comes from grepping `cmd.verb = corp_verb::`
in `corp_ai.cpp`, which returns **ten sites covering eight verbs**.

| corp_verb | Dictionary | UI press | AI candidate | |
|---|---|---|---|---|
| `build` | ✓ | ✓ Tile construction ledger | ✓ ×3 sites | |
| `set_recipe` | ✓ | ✓ Selection Method compare | ✓ | |
| `set_workforce` | ✓ | ✓ Band slider / panel tiers | ✓ | |
| `idle` | ✓ | ✓ Band bottom row | ✓ | |
| `resume` | ✓ | ✓ Band, in place of Idle | ✓ | |
| `survey` | ✓ | ✓ Band Survey section | ✓ | |
| `hire_unit` | ✓ | ✓ Band Hire block | ✓ | |
| `place_sell_order` | ✓ | ✓ Market Ledger tab | ✓ | **8 verbs complete** |
| `demolish` | ✓ | ✓ Band + confirm popup | **✗** | contradicts a standing rule |
| `place_road` | ✓ | ✓ Construction ledger tiers | **✗** | contradicts a standing rule |
| `remove_sell_order` | ✓ | ✓ Sell Orders row button | **✗** | BL-293 gave placement, not withdrawal |
| `set_workforce_auto` | ✓ | ✓ Auto checkbox / button | **✗** | judged **correctly** absent |
| `request_quote` | ✓ | **✗ SEAM-ONLY** | **✗** | BL-350 landed no surface |
| `accept_quote` | ✓ | **✗ SEAM-ONLY** | **✗** | BL-350 landed no surface |
| `cancel_contract` | ✓ | **✗ SEAM-ONLY** | **✗** | BL-350 landed no surface |

**The dictionary is the one axis with no gap** — 15 of 15, which is BL-270's discipline working.
Two findings are defects rather than gaps. `.claude/rules/io-standing-rules.md` states the
BL-202/BL-203 exception as rivals scoring *"build, demolish, survey and road decisions each tick"*,
and two of those four are not emitted — a **standing rule asserting a behaviour that does not
exist**. And BL-350 (procurement seam) landed complete world state, serialisation and a harness with
no player surface, so an external agent can contract for goods and a human cannot. Recorded as
NR-303.

---

## Sprint 22 — Every command reachable from both directions (proposed)

**Goal.** Make the seam's fifteen verbs reachable by a human *and* by the scorer, and leave behind
the instrument that says so. Not "add features": every verb below already exists, is validated, and
is serialised. Build the two ends that were never attached.

**Planned.**
- **BL-451 — spectate asserts families, not verbs** (`designed`, B, d2): a per-verb histogram beside
  the existing family assertion. Printed always, asserted never — a floor per verb would fail
  legitimately whenever a verb is correctly rare, and a harness that fails legitimately gets muted.
- **BL-444 — verb reachability coverage tool** (`designed`, A, d2): the static join across seam,
  dictionary, surface field and scorer. Fails on a **dictionary** gap only; UI and AI gaps are
  reported, since both are legitimate design states.
- **BL-445 — procurement has no UI** (`designed`, A, d3): the largest gap, and the keystone. A
  Contracts tab on the Market Ledger, beside Sell Orders. The four decline results are already
  distinguishable at the seam and are currently read by nothing.
- **BL-446 — the scorer cannot procure** (`designed`, B, d3): request/accept/cancel as ordinary
  candidates, capped at one per evaluation. Blocked on BL-445, so the surface and the scorer are
  argued against the same behaviour.
- **BL-447 — the scorer never demolishes or roads** (`designed`, B, d3): three real gaps, plus the
  standing-rules correction. `set_workforce_auto` is explicitly **out of scope and judged correct** —
  it hands a dial to an auto-solver, and a scorer that already sets the dial has no use for it.

**Sequencing.** The two instruments run **first and alone** — BL-451 and BL-444 share no file with
anything and no fix should be judged without them. BL-445 then leads BL-446. BL-447 is independent
of all of it and starts on day one; its three legs (`demolish`, `place_road`, `remove_sell_order`)
are separable and any one may be cut without stranding the others.

**Done when** the coverage tool reports every verb reachable from a human press and from a scorer
candidate, or names in writing why a verb is deliberately not — and a spectated rollout's per-verb
histogram shows the AI column actually walked, not merely wired.

**The check that says it worked.** BL-444's tool is the check, and BL-451's histogram is its
behavioural counterpart: one says the path exists, the other says it was taken. The pairing is the
point — a static join alone would pass on a candidate site that never wins a score. Per CLAUDE.md
§ Tool creation is skill creation, the tool is wrapped into a skill rather than left loose.

**Risk.** BL-445 is a UI item on the Market Ledger, which BL-293 (order book on the seam) already
touched, and UI work has been the reliably underestimated half of this project. The mitigation is
that it adds **no fourth code path**: the presses issue `corp_command` records through
`apply_corp_command`, exactly as the Sell Orders tab does today.

**Second risk.** BL-446 walks `w.corporations` to pick a supplier, and that is an `unordered_map`.
Iterating it directly is the precise nondeterminism BL-422 (held sell order) already caught once
this month. Sort the id list first; treat any float accumulation over corps as suspect.

---

## Sprint 23 — Friend, neutral, hostile (proposed)

**Goal.** Give the world a place to record that one corporation is at war with, or allied to,
another. Substrate only, and deliberately **inert** — stance gates nothing in this sprint.

**Why now, and why it is not optional.** The Sprint 21 audit above found that *"the capability
everyone assumes exists and does not is a hostility model"*, and flagged it as a design call filed
as work nowhere. Ben settled it on 2026-08-17: hostility is a declared war state corps opt into
deliberately, with a friend state alongside it. This sprint is that ruling written as work.

**A sequencing correction, stated plainly.** If Sprint 21 (military reach) opens, **this sprint must
precede it or be folded in as its F2**. Sprint 21's own risk paragraph says its four fan-out slices
stall behind the hostility model. Running 23 after 21 reproduces exactly the failure that paragraph
predicts.

**Planned.**
- **BL-448 — friend/neutral/hostile stance** (`designed`, A, d4): world state, the declare/offer/
  accept/return-to-neutral verbs, serialisation, and a determinism harness.
- **BL-449 — stance needs a surface** (`designed`, A, d3): the Corporation panel column and presses.
  Filed **separately on purpose** — BL-350 landed a complete seam with no press and nobody noticed
  for weeks, and a surface that is a task inside the model item is the first thing cut under pressure.

**Two things the audit found already built, which change the shape.**

*Do not call it "standing".* `src/world/standing.{hpp,cpp}` exists and means the BL-262 coarse public
**power** read — negligible through dominant, over reach, capital and market share. Overloaded, every
future sentence about "a corp's standing" is ambiguous between how strong it is and how it feels
about you. Proposed term: **stance**, in `src/world/stance.{hpp,cpp}`. Recorded as NR-304.

*The substrate is half-built.* `world::corp_reputation` is already a
`std::map<std::pair<entity_id, entity_id>, float>` — directed, serialised, sorted-key deterministic.
And `rejected_embargo` already declines a quote when a supplier's `condition_set` evaluates false
against the buyer. There is already a relationship-shaped gate in the economy, so stance should feed
it rather than sit beside it.

### The open question — symmetry. Ben's call, and not to be settled in implementation

The ruling admits two readings, and they differ in the **data model**, not merely the logic.

**Reading A — asymmetric hostility, mutual friendship.** War is done *to* you; friendship is agreed
*with* you. `declare_hostile` applies unilaterally and at once. `offer_friendship` creates a pending
offer that `accept_friendship` converts. Storage is a directed pair, matching `corp_reputation`
exactly. The world then holds one-sided wars, so every consumer must ask *"is A hostile to B"* and
none may ask *"are A and B at war"*.

**Reading B — symmetric throughout.** Both modes belong to the pair. Declaring hostility drags the
target into it. Storage is a canonicalised `(min id, max id)` key, one row per relationship, and the
one-sided question cannot be asked because it does not exist.

A models the fiction better and doubles the state. B is simpler at every consumer and cannot express
an ambush. **Recommendation, offered as a recommendation:** A — the ruling's own words, "declared"
and "opt into", are one-sided, and the reputation table proves the directed shape is already
affordable. It remains Ben's call. Recorded as NR-302, with a hybrid third option.

**Done when** a save round-trips a full stance table, two runs of one seed produce identical stance
tables, and the player can see and change a stance from the Corporation panel — with nothing in the
world yet behaving differently because of it.

**The check that says it worked.** A determinism harness asserting: the table survives a save/load
byte-identically; two rollouts of one seed agree; a rejected stance command mutates nothing; and
under Reading A a one-sided pair is representable and stays one-sided until acted on. That last
assertion is the one that fails loudly if the symmetry answer changes after the fact.

**Risk.** Landing the model inert is deliberate and is also the risk — an inert system is easy to
get subtly wrong, because nothing consumes it and nothing complains. The determinism harness is the
only thing standing in for a consumer, so it has to be written first, not last.

**Second risk.** A second, smaller question rides on BL-449 and should be answered in the same
session: does a declaration **announce itself**, or is it discovered on contact under the BL-068
competitor-visibility rule? It is a comms-dock question if it announces, and a discovery question if
it does not.

---

## Sprint 24 — The rival decides who to fight (proposed)

**Goal.** Let rivals choose stance for legible reasons, as ordinary candidates on the scorer that
already exists. The payoff is squarely spectator-facing: a spectated war whose cause is invisible is
a fight, not a story.

**Planned.**
- **BL-450 — rivals cannot reason about stance** (`designed`, A, d4): the scoring terms, hysteresis,
  and new `corp_decision_reason` values. Blocked on BL-448.

**What this is, in the terms the standing rules use.** A widening of the BL-202/BL-203 exception in
exactly the way BL-324 (rival hiring) was — one more legal verb family on the same deterministic
scored-utility layer. No negotiation state machine, no planner, and **no nation behaviour**: stance
is corp-to-corp, and nation actors stay deferred as they are today.

**The three scoring terms, and the one most likely to be forgotten.** Toward hostile: contested
overlap and relative strength, damped by the target's military capacity and by shared market
exposure. A corp does not open a war it is losing, nor burn a market it eats from. Toward friendly:
complementarity, where `corp_reputation` and Sprint 22's completed contracts are already the scalar
that says so. Toward **neutral**: the de-escalation term — without it, stances ratchet and every long
game ends with everyone hostile to everyone.

**Hysteresis is required, not optional.** Stance changes carry a margin the way the workforce and
recipe dials do, one stance command per corp per evaluation, and a cooldown before a pair may move
again. A stance that flips on a marginal score is worse than no stance model, because it makes the
Decision Feed unreadable and would make any later military trigger fire on noise.

**Sequencing.** One item, so sequencing is internal: the scoring terms land before the legibility
values, and the hysteresis lands with the first term rather than after all three.

**Done when** a spectated rollout produces at least one hostile and one friendly declaration, each
readable in the Decision Feed with a stated cause, and the same seed reproduces the same declarations
on the same ticks.

**The check that says it worked.** Extend the stance determinism harness with a behavioural pass over
several seeds asserting: declarations reproduce tick-for-tick; no pair oscillates within the cooldown;
and **the neutral term actually fires** — an end-state where every pair is hostile is a failure, not a
dramatic outcome. That last assertion is the one worth writing first, because it is the failure this
design is most likely to reach.

**Risk.** This is the sprint most able to talk itself into a planner. The scorer is a flat candidate
list, and "diplomacy" invites intent, memory and negotiation, none of which are deterministic scored
utility. Every term must be computable from state a rival may legitimately see under BL-068, in one
pass, with no stored plan.

**Second risk.** Stance terms read `compute_corp_standings`, whose header warns explicitly against
unifying its published bands with the AI's internal scorer — a Goodhart trap where every AI optimises
the number shown to the player. Reading it is fine; scoring *to* it is the thing to refuse.

---

## What these three sprints deliberately do not carry

**The governing-body pivot (BL-094) stays out.** A diplomacy arc pulls toward it hard — stance looks
like the first inch of law and policy reaching military outcomes, which is precisely the pivot's
thesis. It stays out because it is a **v0.3.0** identity change and these are v0.1.16 items, and
because the pivot changes who the player *is*. Landing that on top of an inert stance table, before
anything consumes stance at all, would be building the roof first.

**The political layer stays out** for the same reason, one minor further away.

**Era −1 diplomacy stays out** — BL-297 (Era −1 diplomacy seam) and BL-298 (diplomacy test battery)
are v0.1.16 and look adjacent. They are not: they are **nation**-level, inside the history sim, and
corp stance is a campaign-level corp-to-corp relation. Sharing the word "diplomacy" is the only thing
they share, and merging them would smuggle nation behaviour past a standing prohibition.

**Nation behaviour stays deferred**, unchanged, per backlog.json § BL-054.

**Military consequence stays out of Sprint 23 by design.** Stance gates nothing there. What hostility
*permits* is BL-315 (armed house conflict spine), which is Sprint 21's business, and it is better for
the stance model to be argued about against a real save than against prose.

**Not proposed: a stance model for the Era −1 sim, reputation decay, treaties, or trade embargoes as
a player verb.** The embargo path already half-exists through `condition_set` and
`rejected_embargo`, and extending it is a genuine follow-on — but it is consequence, and these three
sprints are reach, substrate and reasoning in that order.

**Also not proposed: BL-412 (live agent control seam).** It is the natural companion to a
reachability sprint and is deliberately left alone. Its own text says it does not need BL-409
(spectator mode) and carries three open design problems — clock ownership, determinism under a wall
clock, and transport. It is a research seam, not a reach gap, and folding it in would let its cost
in through the side door.

---

# Sprint 25 — Armies that cost something, convoys you can steer (proposed 2026-08-17, NOT opened — Ben picks)

**This is a proposal, not a decision.** Ben's brief this date asked what to build to pad out the
military and logistics systems. This block answers by auditing both against source, and the audit
argues for the **seam between them** rather than for more of either. Recorded as NR-309, because it
is a scoping call taken on Ben's behalf and it excludes readings he may have meant.

## Why not more of either, stated first

**The military side is already proposed, three times over.** Sprint 21 carries the reach arc
(unit verbs, engagement trigger, battle state), Sprint 23 the hostility substrate, Sprint 24 the
rival's stance reasoning. A fourth military-content sprint duplicates them.

**The logistics side already has a minor waiting.** v0.1.12 (logistics modes) holds four designed
items — freight premium (**BL-153**), rail as a distinct mode (**BL-173**), coastal ports
(**BL-188**), supply-lens flow legibility (**BL-175**) — under the theme *"distance costs
something, in more than one way, and the player can see it."* Every one makes distance cost more or
read better. **After all four land, the player still cannot dispatch a single convoy.**

So padding either side alone adds to a stack that is already deep and still untouchable. What is
missing is the coupling: armies that draw on the network, and a network the player can direct.

## The audit

| Capability | State | Evidence |
|---|---|---|
| Convoy entity, per-tick advance, pool crediting | **BUILT** | `supply_system.cpp` (~340 lines) |
| Terrain-weighted A* routing, roads, rivers, node discount | **BUILT** | `logistics.cpp`, BL-077/146–149/170/172 |
| Travel time from derived km-per-tile | **BUILT** | `body_km_per_tile`, landed 2026-08-12 |
| Auto-dispatch on shortfall, intra- and inter-body | **BUILT** | `dispatch_convoys`, launchpad-gated |
| Convoy rendering — beam + tail, Solar lines, Supply lens, route graph | **BUILT** | `body_surface_canvas.cpp`, `solar_system_canvas.cpp` |
| Persistent trade routes + activity fog | **BUILT** | BL-088 / BL-089 |
| `military_base`, hire-at-base, corp-side gating | **BUILT** | BL-325 S1+S2 |
| Two battle resolvers, terrain scalars, unit roster | **BUILT** | Sprint 21's audit; unchanged |
| **A verb that dispatches, holds or routes a convoy** | **ABSENT** | 15 `corp_verb`s; not one names a convoy |
| **A list of what is in flight** | **ABSENT** | no ledger tab, no Selection section |
| **Arrival time on any surface** | **ABSENT** | the 2026-08-12 travel model is invisible |
| **Any recurring cost of a unit** | **ABSENT** | `w.units` in **no** budget or economy flow |
| **Orphan cleanup on demolish** | **DEFECT** | `demolish_building` erases asset/building/stockpile, never units |
| **A consumer for `military_points`** | **ABSENT** | 3 sites in `src/`: decl, param load, write |
| **A consumer for `science`** | **ABSENT** | same three-site shape |
| **A military authority doc** | **ABSENT** | no `docs/military/`; BL-325 points at the AI doc |
| Out-of-supply decay | DESIGN-ONLY | BL-325 S3, held behind BL-315 |
| Sea-mode port gate | ABSENT | sea fires on any ocean-crossing path (BL-188) |
| Per-node throughput | out of scope | SUPPLY.md § Infrastructure gates |

### Three findings that re-frame the brief

**1. The convoy layer is the largest built subsystem in the game with no player verb at all.**
`corp_verb`'s fifteen values name eight buildings, three orders, two contracts, one body and one
tile. Layer 5 — the *only* coupling between two markets' prices, in SUPPLY.md's own words
(*"there is no abstract price-coupling term between bodies: the convoy **is** the coupling"*) — is
entirely automatic. This is not a new discovery so much as an unread one: SUPPLY.md's Build status
has carried the line *"Player-directed dispatch (the § Dispatch trigger 'exception' has no UI or
code path yet)"* since the layer landed, and its § Dispatch trigger describes player-direction as a
shipped exception that was never built.

**Sprint 22's reachability audit could not see this**, and the reason is structural: it joined the
seam, the dictionary, the surface field and the scorer *over the verbs that exist*, and all fifteen
are reachable from at least one direction. A join over existing verbs is blind to the absent one.
If BL-444's coverage tool ships as designed it inherits that blind spot, and a green report will
read as *everything is reachable* over a game with an untouchable logistics layer. Recorded as
NR-308, with a suggested subsystem column for BL-444.

**2. An army is bought once and is then free forever.** Verified by grep, not inferred: `w.units`
appears in `economy_system.cpp`, `budget_system.cpp` and `construction.cpp` **zero times**.
`apply_budget`'s five flows are all building-derived; `operating_cost` takes a `building_component`.
So a building pays maintenance and wages every tick and a regiment beside it pays nothing, ever.

That makes the standing rules' hiring exception weaker than it reads. BL-324's *"never on cash"*
governs availability and Ben's 2026-08-13 ruling put spending under the solvency gate — but the
only spend is the **moment of hire**, so the gate a rival passes once is the only pressure the
roster ever applies. Twelve units cost a rival nothing to keep.

**3. Two write-only accumulators, and the precedent says one of them should probably go.**
`military_points` and `science` are each credited per tick by BL-332's pass and read by **nothing**
— no gate, no surface, no scorer, not the blackboard. `military_capability_harness` verifies the
accumulation faithfully, which makes it a green check over a number nothing reads. This is NR-257's
orphan-resource shape with the arrow reversed, and that precedent resolved by *deleting* five
resource types rather than by inventing uses. Recorded as NR-307.

**And a fourth, smaller but load-bearing: there is no military authority doc.** `docs/` has
economy, generation, ui, ai, lore, tech and development — and no military. BL-325 names
`AI_OPPONENT.md`, which is the doc about the rival's decision-making, not about how force works.
The cost has already been paid once: three sprint blocks in a row described BL-315's resolver as
unbuilt while `campaign_battle.cpp` sat compiled in the shipping binary, and Sprint 21's audit
found it only by reading source.

**Planned.**
- **BL-454 — units cost nothing to keep** (`designed`, A, d3): the keystone, and the only item here
  that couples the two systems directly. Carries the demolish-orphan fix, because the unit pass is
  the only place that can see it. **One open question first** — credits, goods, or both (NR-306).
- **BL-452 — logistics has no verb** (`designed`, A, d3): `dispatch_convoy` and `hold_convoy` at
  the seam. The auto-dispatcher's body with the shortfall scan removed — **no fourth code path**,
  the same mitigation Sprint 22 named for BL-445.
- **BL-453 — convoys have no ledger** (`designed`, A, d3): the Convoys tab, blocked on BL-452 and
  **filed separately on purpose**. BL-350 landed a complete seam with no press and nobody noticed
  for weeks; a surface that is a task inside the model item is the first thing cut under pressure.
- **BL-455 — `military_points` and `science` are write-only** (`designed`, B, d2): a measurement,
  not a build. Wire it or remove it; the third state is what caused the orphan resources.
- **BL-456 — no military authority doc** (`designed`, B, d2): `docs/military/MILITARY.md`, written
  in SUPPLY.md's style with a Build status section so it cannot rot into an aspiration doc.

**Riders already on the board, if the sprint has room:** BL-153 (freight premium — parked, and it
is the revenue half of the same lever), BL-405 (hire has no price on screen), BL-188 (port gating
for sea mode, design-owed).

**Sequencing.** NR-306 is answered **first and in the session that opens the sprint** — it decides
the shape of BL-454's unit pass, not a constant, and leaving it open reproduces exactly the stall
BL-440's fork caused in Sprint 19. After that three tracks run with no file overlap: BL-454 in
`budget_system` / `economy_system`; BL-452 → BL-453 in `corp_command` / `supply_system` → the
Market Ledger; BL-455 and BL-456 independent of everything and startable on day one.

**Done when** a player can dispatch a named cargo down a lane they chose, see it listed with an
arrival tick, hold it, and watch an army they raised draw a visible cost every quarter it stands —
with the same seed replaying identically and the pre-item save byte-identical at a zero rate.

**The check that says it worked.** A `unit_upkeep` harness asserting: N units cost exactly N × rate
and nothing else moves; demolishing the base leaves no orphan; two runs of one seed agree; and the
**rate-zero run is byte-identical to the pre-item build** — the same defaults-off proof
`spectator_determinism` used for BL-409, and the one that makes the golden re-bless honest. Plus a
`--verify` capture of the Convoys tab, and a dispatch→hold→resume round-trip through
`apply_corp_command` asserting a rejection mutates nothing.

**Risk.** BL-454 touches `economy_system.cpp` and `economy.lua` — the seam Sprint 21's own risk
paragraph names as hot — and it **moves every economy golden**, because every seed with a hiring
rival now spends differently. Take the measurement of how far rival balances move *before* raising
the rate off zero, and bless with a stated cause. This is also the fifth consecutive sprint whose
goldens are downstream of an economy change, which is an argument for landing it at rate zero and
tuning in a separate pass.

**Second risk.** `dispatch_convoy` is a seam verb taking a quantity and two market ids, so it is an
untrusted input boundary (standing rules, 2026-08-14): the quantity must be range-checked against
the source pool **as the value that lands**, and a rejection must mutate nothing — not clamp, not
truncate. The auto-dispatcher never had to care because it computed its own arguments.

## What this sprint deliberately does not carry

**Interdiction and escort stay out**, and this is the exclusion most likely to be the one Ben meant.
A convoy cannot be escorted until something can threaten it, which needs BL-315 (conflict spine) for
the engagement and BL-448 (stance) to name a legal target. It is the natural sprint *after* 23, and
folding it in here would make this block depend on two unopened proposals.

**BL-325 S3 (out-of-supply decay) stays out**, unchanged from Sprint 21's reasoning — but note that
BL-454 is its economic twin and lands the substrate it needs. Upkeep and decay are the same rule
read at two distances; upkeep is the half that does not need BL-315 first.

**Per-node throughput capacity** is out of prototype scope by SUPPLY.md's own § Infrastructure
gates. **Air mode and the airfield building** stay deferred. **A second reach field** is forbidden
outright by BL-325's ruling 3, and no item here proposes one.

**Waypoint routing and standing lane orders stay out.** BL-452 gives dispatch and hold, not a
routing UI — the pathfinder already picks the route, and letting a player override it is a
different item with a real design question behind it.

---

# Sprint 18 — The military engagement surface (planned 2026-08-15; NOT opened — deferred again 2026-08-16 for Sprint 19)

**Goal.** Finish what Sprint 16 started: make the fight *playable and visible*. Closes and cuts
**v0.1.15**, the mercenary vertical slice.

**Planned.**
- **BL-393 — units are write-only and inert** (`designed`, A): the sharpest live defect. A player
  raised 25 units and found nothing to do with any of them — no verb takes a unit as a subject, no
  blackboard predicate reads one back, and demolishing the muster base silently orphans every unit
  standing on it. Foundation for everything below.
- **BL-315 — conflict spine** (`designed`, A): units as tokens on the tile map, a campaign battle
  resolver with seeded randomness and withdrawal. Nothing in the campaign layer commands
  `resolve_battle` today.
- **BL-384 — the Era −1 sim conquers nothing** (`designed`, A): 267 battles, zero provinces taken
  across a full run. The generator that feeds the ancient product cannot fight to a conclusion.
- **BL-405 — hire has no price on screen** (`designed`, B): hiring costs credits since BL-394 and
  no surface shows it; the AI reads the price from `ACTIONS.json`, the human is told by a refusal
  string.
- **BL-325 tail — unit supply decay** (`designed`, B): the last leg of military bases and supply.
- **BL-314 — unit verb family** (`design-owed`, B): unblocks the moment BL-393 gives it a
  `unit_command` seam to transcribe from. Dictionary entries are transcribed, never authored ahead
  of the seam (BL-270).

**UI padding, in scope and not a rider.** This is the half Ben named: unit markers and their glyphs
(ICONS.md), a unit view in the Selection panel, a force ledger, battle outcomes in the comms dock,
and a `question_log.json` entry per surface. A unit you cannot see is the same defect BL-393
describes, one layer up.

**Risk.** BL-315 is still the item the sprint leans on, and it is now the *only* unknown — its
design landed but nothing is built against it. Fallback is unchanged from Sprint 16: the thinnest
command surface over the existing battle resolution beats a designed-but-unbuilt spine.

---

# Sprints 8–14 — the plan (written 2026-08-10, re-sequenced the same day; Sprints 12–14 superseded 2026-08-12 by NR-177 — their minors parked with the space arc, never opened)

> **Re-sequenced 2026-08-10 (Ben).** Sprints 8 and 9 closed the same day they were planned. A new
> **Sprint 10 — the living world** was then inserted ahead of procurement, on Ben's steer that the
> world should be filled with real industry and the abstract substrate replaced outright. Everything
> after it shifts by one: procurement 10→11, v0.1.11 12, v0.1.7 13, and v0.2.0 becomes the obvious
> Sprint 14. The reasoning for the insertion is in Sprint 10's own § Why this precedes procurement.

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

**Retro (closed 2026-08-10, same session).**

- **BL-331 was already built — under the WRONG commit id.** File survey found the exact seeding
  this item specifies already landed in `corporation_generation.cpp`, committed as
  `2eb8654 "Player starts with a military base and one unit (BL-330)"` — BL-330 is a real,
  unrelated, already-`complete` item (a Selection-panel bug). Verified rather than rebuilt: no
  harness had ever asserted the seeding happened, so a PASS/FAIL check was added to
  `tools/verify/world_audit.cpp` (`base=yes unit=yes : PASS`). Flipped `complete`.
- **BL-325 S2 (hire moves onto the base) — built and correct, with one real gap found and fixed
  along the way.** `corp_command.cpp`'s `hire_unit` now requires the target tile to carry the
  corp's own completed `military_base`; `selection_panel.cpp`'s Hire section only renders on such
  a tile. Landing the precondition exposed that `corp_ai.cpp`'s build-candidate loop never
  proposed `military_base` at all — only extraction/processing — so no rival corp could ever
  satisfy the new gate. Fixed: a muster-base candidate, tech-gate-aware (BL-344's E0-ML-01), one
  per corp, competing on merit in the same nice_to_have bucket as everything else.
- **Two real bugs caught and fixed in the same candidate, not shipped blind.** First cut allowed a
  fresh candidate on a NEW tile every eval for a corp's whole in-flight base (had_base checked only
  *completed*, not *in-flight*) — collapsed `ai_skill_harness` to 100+ builds/seed and net worth
  cratered negative. Fixed by gating on any base, complete or building. Second: the candidate's
  `can_place_in_world` call omitted the corp argument, so a corp that could never earn E0-ML-01
  re-proposed a doomed build every single eval forever (982 wasted attempts traced across the
  golden set). Fixed by passing `corp` so the tech gate applies at generation time, not just apply
  time. Both caught by actually running the harness after each change, not by inspection.
- **`ai_skill_harness` golden bands re-blessed, with the reasoning recorded in the file.** Net
  worth and solvency held; `survival_fraction` moved on seeds 1 and 4 (both now finish at 1.00) and
  `build_max` needed +1 on seed 4. Documented as a hypothesis, matching the standing style the
  file already uses for seed 4's prior widening — not asserted as measured fact.
- **What did NOT get verified, and is not being hidden.** `hire_unit` was never observed firing for
  a rival corp across the harness's five seeds. Traced to a genuine, pre-existing property of
  BL-095's pay-as-you-build model: construction stalls indefinitely if the picked tile's local
  market carries no steel, and this sprint's candidate is the first thing to actually exercise
  `corp_ai`'s own build-to-completion path in this harness (the baseline ran zero build actions
  across all five seeds). Recorded as **NR-121**, not force-fixed by tuning a score to make hire
  "appear" — that would have been gaming the check rather than answering it.
- **BL-332 — designed, not built, and re-versioned.** Answered Ben's four filing questions
  (resource not rate; two buildings not one; points buy bands, deeds open keystones; rivals
  accumulate symmetrically, the nation layer does not). On working the actual shape, both new
  buildings are resource-tier plumbing — the same kind of change BL-340 already owns — so the
  BUILD moves to **v0.1.14** (Sprint 10) rather than landing twice. v0.1.5 needed none of this
  machinery for its own cut.
- **v0.1.5's done-definition written at the cut** (ROADMAP.md), including the one thing it does
  NOT claim (hire observed end-to-end for a rival corp) rather than overstating it.
- **Feedback: two design assumptions in BL-325's own filing text turned out false, and both were
  caught only by running the harness, not by reading the code.** "The scorer prices [a muster base]
  via the existing build-candidate machinery" (BL-325's design) assumed that machinery covered
  every building type; it covered exactly two. Sprint 5's retro named the same pattern for the
  worktree-agent failure mode — "the read is the cheap part" — and it held again here: each of the
  three bugs (missing candidate type, in-flight re-proposal, tech-gate blindness) was a five-minute
  fix once found, and each was invisible until a real 300-tick rollout was run against it.

## Sprint 10 — The living world (cut v0.1.13)

**Goal.** The market saturates because real firms produce and consume, not because a substrate pass
injects supply and demand. Ben, 2026-08-10: *"we should do more work to fill out a living world,
with saturated markets, and plenty of buildings … this is the only way I can see a saturated market
working, where the player is part of a larger market where most of the mundane trades happen behind
the scenes."* His call the same day: **replace the substrate entirely**, rather than keeping it and
adding buildings as texture.

**Planned.** **BL-253** (the O(corps × tiles) strategic scan — re-goaled C/v0.2.0 → A/v0.1.13 and a
hard prerequisite, not an adjacent nicety), then **BL-366** (multi-building tiles) and **BL-368**
(real population demand) as the two foundations, then **BL-365** (background industry) as the
keystone, with **BL-367** (management surface) and **BL-369** (warm-start calendar) alongside.
**BL-130** (real market inventory) and **BL-132** (market cogeneration) are absorbed from v0.1.13's
existing five — both were orphans and both are components of this rather than neighbours of it.

**Why v0.1.13 rather than a new minor.** It was hollowed out on 2026-08-10 when BL-340 left for
v0.1.14, leaving five items with no A-priority among them. This gives it a keystone and rescues two
C-items into a coherent story. Nothing needs renumbering.

**Why this precedes procurement.** BL-350's counterparty model — a supplier that quotes, delays or
refuses, with the player routing around a refusal to a competing quote — needs suppliers to choose
between. Against today's 8 lean corps and ~24 buildings on 15,120 tiles, *"another supplier may
still quote"* is often simply false, and the mechanic would ship correct but unexercised. That is
exactly Sprint 9's `hire_unit`: shipped correct, never once observed firing across five harness
seeds (NR-121). Ordering this first also **deletes** BL-340's substrate step rather than building it
twice, since BL-365 removes the function that step would have extended.

**The measurement that motivated it.** `pregame_balance_harness 80` (2026-08-10, the real generated
world): the player corp's balance grows linearly to tick 23, knees at ~24, and **plateaus from ~tick
47 at ~185k cr**. The economy has a carrying capacity and reaches it. The same run also landed
`pre_game_ticks` 12 → 80 — the old figure's justification (*"short enough not to diverge under the
prototype's un-tuned economy"*) was measured and did not hold; it converges.

**Risk.** The largest of the five sprints by some distance — BL-365 is difficulty 5 and
`design-owed`, and its dominant open question (do ~80 background firms run the full `corp_ai`
scored-utility layer, or a reduced produce/sell model?) is a perf question that BL-253 only
partly de-risks. Accepted cost, named here so the retro does not have to rediscover it: **v0.2.0 is
now deferred a second time**, both times in the same direction.

**Owed at the cut.** A written done-definition for v0.1.13, per NR-103.

**Progress (2026-08-11).** BL-253, **BL-366**, **BL-368**, **BL-263** and **BL-130** all
`complete` — the entire blocker chain BL-365 turned out to sit behind (`blocked_on` BL-130,
which `requires` BL-263) is now clear, alongside both of Sprint 10's original foundations.
**BL-365 itself is next** — the keystone, difficulty 5, with its own open corp_ai-scope question
already settled (2026-08-11: full scored-utility layer for every background firm, not a reduced
model). BL-367, BL-132 and BL-369 remain `designed`, not yet promoted. See DEVLOG's 2026-08-11
entries.

## Sprint 11 — Procurement, and the goods it is about (cut v0.1.14)

**Goal.** The refocus's actual mechanic, plus the resource tiers that make "space equipment" a
thing rather than a label.

**Planned.** **BL-350** (the procurement/contract seam) and **BL-340** (the processing roster) —
both `designed` as of 2026-08-10, in a joint design pass, because they are mechanism and content for
the same premise. Plus **BL-332** (military points + research building), which moved here from
v0.1.5 at Sprint 9's retro as resource-tier plumbing of the same kind.

**What the re-sequence changed.** BL-340's substrate-basket step is **deleted, not deferred** —
Sprint 10's BL-365 removes `inject_substrate_demand`, so background demand for the seven new goods
is BL-365's responsibility. BL-350 now lands into a saturated world, which is the whole reason the
order flipped.

**Dependency note.** BL-340 is difficulty 4. It was the single largest unknown in the original plan;
the joint design pass has reduced that, and its remaining risk is pricing calibration rather than
shape.

### Retro (2026-08-11) — all three landed, all three complete

All three items shipped in build order — BL-340 first (least architecturally risky), then BL-332,
then BL-350 (the largest, most integration-sensitive) — each independently verified and committed
before the next started, per the file collision map: all three touch `components.hpp` and
`recipe_registry.{hpp,cpp}`, so sequencing rather than parallel worktree agents kept the shared
headers single-writer.

**What actually shipped.**

- **BL-340** — seven new `resource_type` values (32 → 39) plus six newly-priced raws, closing the
  minable-but-unsellable asymmetry the item's own measurement found. New `resource_chain_harness`.
- **BL-332** — a `research_institute` building_type (7 → 8) and two corp-level accumulators
  (`military_points`, `science`), passive and symmetric across every corp. New
  `military_capability_harness`. Production side only — no spend mechanism, no `corp_ai` build
  candidate, both named as follow-on scope rather than built.
- **BL-350** — three new `corp_verbs` on the shared command seam, a fourth flat-binary stream
  (`procurement.{hpp,cpp}`, magic `IOPC`), and BL-342's `condition_set` reaching a real consumer
  (the embargo decline) for the first time. New `procurement_harness`. One named simplification:
  fixed-schedule contract pacing rather than BL-095's market-gated stretch/pause rate.

**Verification shape, held across all three.** Every item: (1) built and ran clean against the
real `ProjectIo` target; (2) reran every existing harness whose linked TUs it touched, with zero
new failures; (3) added its own dedicated new harness rather than folding assertions into an
existing one. `ai_skill_harness`'s five pre-existing golden-band failures (NR-140) were checked
bit-identical before/after each landing — none of the three moved them, confirmed rather than
assumed by stash-and-rerun where the change plausibly could have (BL-340's steel/coal recipe fix).

**What was deliberately not built**, named per Rule 0c rather than silently dropped: BL-332's
spend mechanism (BL-087's constellation is the intended sink) and its `corp_ai` build candidate;
BL-350's own richer pacing model and its `corp_ai` candidate — the seam is buyer-agnostic and
player-reachable today, an AI user is a follow-on. BL-365 (Sprint 10's keystone, still only
`designed`) still owns background demand for BL-340's roster and remains the item that would
actually exercise BL-350's "another supplier may still quote" premise at scale.

## Sprint 12 — v0.1.11 reconciled (cut v0.1.11)

**Goal.** The fattest open minor, built against the actor Sprint 8 defined rather than the one it
was written for.

**Planned.** The laws and tech surfaces (**BL-155**, **BL-156**, **BL-186**) *as reframed* —
under a militia these are constraints the player operates inside, which is a different surface
from an enactment ledger. Then the items the refocus does not touch: **BL-211** (history ledger),
**BL-212** (nation-voiced comms), **BL-309** (deed lines), **BL-264** (wizard layout after fold),
**BL-341** (the parked Windows cold-configure check). **BL-280** (negotiated tax rate,
`design-owed`, difficulty 5) is the one to watch — Sprint 8 will likely have merged its bargaining
model into the procurement seam, in which case it shrinks or dissolves.

## Sprint 13 — Generation visibility, and the owed timelapse (cut v0.1.7)

**Goal.** Close the generation arc Ben declared himself happy with, including the one piece he
named as still owed.

**Planned.** **BL-303** (Generation Ledger), **BL-304** (field-overlay lenses), **BL-305**
(political-step visibility), and **BL-098** (the UX review walking the whole band against
`user_stories.json`) — which is the right last act, since by then five sprints of surfaces will
have accumulated. Plus **BL-317** (the New World wizard's prehistory timelapse), pulled back from
v0.3.0: it is the history time-lapse Ben named at Sprint 5's close, and it is a
generation-visibility item by nature rather than an Era −1 one.

**One sequencing option worth taking.** BL-317 pairs naturally with **BL-264** (wizard layout
after per-stage folding) — both are New World wizard stages. If Sprint 12 is building BL-264
anyway, riding BL-317 alongside it gets the timelapse three sprints earlier for very little extra,
and Sprint 13 loses nothing it needs.

---

## What this plan deliberately does not carry

- **v0.2.0 (the AI opponent), 12 items.** It is the thing that makes Io a game rather than a
  simulation, and it is *not* in the sprints above. That is a real cost, accepted because the
  opponent should be built against a settled player identity, not re-fitted to one. It is the
  obvious **Sprint 14**. **Deferred twice now** — first for the militia refocus, then again for the
  living world when Sprint 10 was inserted on 2026-08-10. Both deferrals point the same way, and
  the second one has an argument in its favour (an opponent is more interesting in a market with
  real competitors than in one with eight lean corps). Name it at the v0.1.13 retro rather than
  letting a third accumulate silently.
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


---

## Sprint 18 retro — the growth gate, and four things measurement overturned (2026-08-16)

**Goal.** Finish BL-428: make chain depth actually gate something, rather than compute a number
nothing reads.

### What landed

- **BL-428 complete** — `recipe_required_depth` + `corporation_component::produced_ever` +
  refusal at both `construct_building` and `try_switch_recipe`. 6 tasks, 4 requirements, all met.
  `chain_depth` gained 11 assertions (G1–G4), including BL-432's no-stranded-building row.
- **Two review-queue entries cleared** (NR-246, NR-244's headless half) and **three filed**
  (NR-254, NR-255, NR-256).
- **BL-435 filed** — starting-corp selection, with a measurement behind it rather than a hunch.
- **`player_seed_sweep`** added and catalogued in the `verifier-headless` skill.

### What this sprint is actually worth remembering for

**Every substantive finding came from measuring something, and four times the measurement
overturned the plausible story.** The pattern is the same one Sprint 11's retro recorded, which
suggests it is the method working rather than a coincidence:

| The story | What measurement said |
|---|---|
| "Keep only *profitable* player seeds" | Profitability is **not** the discriminator — all 24 seeds end positive, none dip. 13/24 fail on having no processing facility |
| The Method page overlap is a draw-order bug | Draw order was half of it; fixing only that gave `"..."`. The row was **160 px holding 248 px** of content |
| The app crashed after ~3 s | `--autostart-windowed` is a smoke test that **exits by design** at 120 frames |
| The queue has 36 open entries | It has **18, 15 open** — the count had measured `_note`'s lines |

**A gate on one door is not a gate.** BL-428's design named placement as what depth gates, and
implementing exactly that would have shipped a one-click bypass via retooling. Worth generalising:
when a rule is added to one seam, ask what the *other* routes to the same state are before calling
it done.

**The check that had never actually run was hiding a live defect.** `building_management_shell.lua`
had been pointing at a deleted tab (NR-246) and still passing — capture-only, no golden, nothing
comparing anything. Its first honest photograph immediately showed the Method page's two
load-bearing numbers printed on top of each other. A check that runs green while looking at the
wrong thing is worse than no check, because it is counted as coverage.

**A stale tool doc costs a session's first hour.** The `verifier-headless` vcvars path had been
wrong since 2026-07-28 and failed on the first translation unit every time. Corrected, with the
symptom quoted so the next person recognises it in one read rather than diagnosing it again.

### Where the method held, and where it did not

**Held.** Rule 0b (report measurements, then ask) was used twice — once on the row width, once on
the seed filter — and both times the numbers changed the answer rather than merely supporting it.
The second was the more valuable: it stopped a filter that would have discarded half the seed space
to fix an assignment problem.

**Did not.** The sprint ran entirely in the main session with no fan-out. Defensible for one
vertical seam of ~5 files with `construction.cpp` as the hotspot, but it means the parallel-agent
machinery got no exercise, and the session length was dominated by serial build/verify cycles.

### Left for another session

- **NR-256** — `--autostart-play` terminates unattended and the cause is unsettled. The two-line
  diagnostic that would separate the candidates is written down; it just needs running from a real
  terminal.
- **NR-243** — the four genuinely dominated recipe pairs remain the one red row in
  `recipe_switch_harness`. Pre-existing, and a content decision rather than a code one.
- **NR-253** — the recipe-switch cooldown at 1 tick is still an unplayed first cut.
- **BL-432** — two roster invariants (no orphan resources, no dominant method) still owed to
  `chain_depth`.
- **The gate is ancient-only.** Chain depth gates nothing a 1960 campaign can see, by design. Whether
  the industrial roster ever opts in is an open question BL-428 deliberately did not answer.

---

## Sprint 18b retro — the roster invariants, and what they found (2026-08-16, retro-recorded)

**Recorded after the fact, and that is the fourth time.** This session ran a sprint's worth of work
with no sprint entry open — the exact drift this file's own standing caution names (*amend when the
goal changes, not at the retro*). It is numbered 18b rather than 19 because it is the direct
continuation of Sprint 18's leftovers, not a new theme.

**Goal (reconstructed).** Clear the three review-queue entries Ben named, then land BL-432's two
owed roster invariants.

### What landed

- **BL-432 complete.** `chain_depth`'s R1/R2 rows; the item's other two assertions had already
  shipped as D4 and G3.
- **NR-243 dissolved without changing a single recipe number**, and **NR-257 answered in three
  parts** — consumers for the three one-directional orphans, a `base_price` for regolith, and the
  removal of the five double-orphans.
- **Five `resource_type` values removed** (42 → 37), six dependent sites cleaned.
- **BL-435 tasks A–D** (starting-corp selection), **paused at 4/6** on Ben's call to move on.
- **BL-436 filed**, priority A, with a clean natural-experiment measurement behind it.
- **NR-256 advanced**, NR-257/NR-260 resolved, NR-258/NR-259 filed.

### What this sprint is actually worth remembering for

**Every guard written this session immediately found something, and none of it was what the guard
was written to look for.** That is three for three:

| The guard | What it was for | What it actually found |
|---|---|---|
| R1 (orphan resources) | catch the *next* orphan | the five already there, exactly as BL-432's design predicted |
| R2 (no dominant method) | police BL-430's alternates | that BL-430 has **zero** genuine alternates, and the four "dominated" pairs were a grouping artefact |
| `--roster` (BL-435) | show the screen's data | that a processor *costs* income — undercutting the premise of the item it was written for |

**The strongest single lesson: a measurement that contradicts the item that commissioned it is the
most valuable thing a session produces.** BL-435 was filed on "a corp with a processor is a better
opening". Its own first measurement says that corp is measurably *poorer*. The item survives —
depth and money are different axes — but it now knows which axis it is on, and BL-436 exists.

**Two ways a check can be worthless, both invisible from its own output.** Sprint 18's retro found
a check running green while pointed at a deleted tab. This session found the mirror: `player_seed_sweep`
had **never once passed under ctest** since it was added, because a 69 s tool sat under a 60 s
timeout, and a Timeout reads as infrastructure noise rather than as "this has never verified
anything" (NR-259). Standing habit worth adopting: when a harness joins the gate, run the *gate*,
not the exe.

**The code disagreed with its own comments twice, in the same session, in two unrelated files.**
`recipes.lua` already stated the tier-vs-alternate axis NR-243 spent a review cycle asking for;
`focus_asset_pattern` promised extraction corps "a single processor" while the ordering denied it
half the time. Both fixes were *reading* rather than designing. Worth trying first next time a
design question looks open: the answer may already be written above the code.

### Where the method held, and where it did not

**Held.** Rule 0b twice more, both times with the numbers changing the answer. Nothing was tuned to
make a check pass: NR-243 resolved by fixing the question, not the recipes, and `ai_skill_harness`
was left failing across one report rather than blessed before Ben ruled.

**Did not.** (1) No sprint entry — fourth time. (2) A golden was missed: `spectator_determinism`
broke with BL-435 task B and sat failing across a commit because only `ai_skill_harness` was
re-blessed. The full gate caught it; a narrower run would not have. (3) Second session running with
no fan-out, and the session was again dominated by serial build/verify cycles.

- **Runtime:** not summed. Sixth consecutive entry — the format's Runtime line has now been
  uncollected longer than it was ever collected. Either wire `tools/session/timer.js` in or drop
  the line from the format block; carrying it as a permanent TODO is worse than either.
