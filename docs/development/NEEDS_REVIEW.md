# Project Io — Needs Review

**Ben's review queue.** Readable mirror of [`NEEDS_REVIEW.json`](NEEDS_REVIEW.json),
which is canonical — the JSON wins on any disagreement.

> **Generated file.** Produced by `node tools/session/render_needs_review.js`.
> Edit the JSON, then re-run; hand edits here are overwritten.

Things here are waiting on **your judgement**, not on work. Three kinds:

| Kind | Meaning |
|---|---|
| **question** | An open call nobody has made. Not blocking — a blocking item is a backlog entry with `blocked_on` set. |
| **decision-taken** | A call made **on your behalf** so work could continue. Recorded so it can be *overturned* rather than quietly becoming precedent. |
| **observation** | Something noticed in passing, too small or too cross-cutting to file, that a human should still see. |

**How this differs from the neighbours.** [`review.json`](review.json) is a *blocker* list —
items blocked on a visual artifact only you can produce; work there cannot proceed at all.
[`backlog.json`](backlog.json) is *work*. Entries here are neither: they are questions and
reversible calls. If an answer creates work, file a backlog item and resolve the entry with
that item's id.

This queue is **transient**: resolved entries are pruned promptly rather than kept for
posterity — the reasoning lands in code, an authority doc, or a backlog item at the moment
the work happens, and that is the durable record. What stays here is what is still open.

*8 entries — 0 open, 8 resolved.*

---

## Open

*Nothing open.*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-896 — CALL: forest and coal both read the BEST held region, which grows with realm size — and Charcoal now outnumbers Coke
*question · raised 2026-09-18 · from BL-1041 (industry points) build lane and cold review, after the wave 1 ruling "score forest like fuel" (NR-895). Figures are the lane's 16-seed run; the main session re-runs them before quoting them as evidence.*

With forest scored like fuel (the best held region against the world mean, capped 1000) the Fuel Doctrine split moved from 391 coke / 132 charcoal / 333 neither to 200 / 319 / 337. 261 of the 319 Charcoal polities had passed the seam gate — Coke was open and they took Charcoal. Mean held forest score by side: coke 739, charcoal 859, neither 306. TREES.md § The scorer: 'Never a rank, never a term inside the scorer that grows with the polity's own size.' A best-of-held maximum can only rise as a realm grows (to its 1000 cap), so both ground_fuel (already a best seam) and ground_forest now breach the letter of that rule, weakly; INDUSTRY_TREE.md rules exactly this shape. The docs disagree, and wide realms read near the cap on both terms.

**Why it matters.** The Fuel Doctrine is the tree's central fork: coke to scale and the Blast Works, charcoal to quality and a ceiling. If breadth, not ground, decides it, the 1960 spread it manufactures is about realm size.

- Both terms read the SHARE of held regions whose score clears the world mean — size-neutral, like with like; the fuel GATE still passes on any held seam.
- Keep best-of-held for both, and write it into TREES.md as a stated exception to the size rule.
- Both terms read the held MEAN of the scores — size-neutral, but a large realm with one coalfield reads low.

> **Recommendation:** The share of held regions over the world mean, for both terms. It keeps "any seam opens the gate" and stops breadth from deciding the pull.

> **RESOLVED.** RULED (Ben, 2026-09-19): option A. Both ground_forest and ground_fuel read the share of held regions whose score clears the world mean; the fuel gate still passes on any held seam. Written into INDUSTRY_TREE.md § The scorer; the work is BL-1056 (points size-neutral).

*Files: `src/world/history_sim.cpp`, `docs/generation/trees/INDUSTRY_TREE.md`, `docs/generation/trees/TREES.md`*

### NR-897 — DECISIONS TAKEN (and a call): the treasury-to-points constants, and treasury points landing only on the capital
*decision taken on your behalf · raised 2026-09-18 · from BL-1041 (industry points) build lane and cold review. Figures are the lane's 16-seed run, to be re-run by the main session.*

The lane chose, on your behalf: 1000 industry points per million urban heads per year (defines the point); a fuel factor of 250 + 750 x reading/1000 (floor 250, so fuel is at most a x4 lever); a treasury share of 250 per mille of the round's surplus, where surplus is the treasury's net rise across the round's earn and army/navy bills, floored at 0 and DEBITED (a conversion, not a copy), and tribute is not counted; and an exchange rate of 1000 points per treasury unit. At 1:1 the treasury could not move any region's rank; at 1000:1 the treasury supplies a median 69% of all points (0.16-0.82 by seed), and because treasury points land on the capital's own region (PROPOSED 2026-09-18, not overturned), one region holds up to 65% of a world's points (median top-region share 0.28). The charter budget follows the points, so charters would concentrate at capitals.

**Why it matters.** "Density follows cities" becomes "density follows capitals" if the treasury term dominates and lands in one place; the real-stockpile sweep (BL-1043) and the re-bless read this.

- Spread treasury points over the polity's centres by their urban scale, not the capital alone; keep the constants.
- Keep the capital landing; lower the exchange rate so treasury is a minority input (e.g. a third of points).
- Accept as built and judge on the real-stockpile sweep.

> **Recommendation:** Spread treasury points by scale over the polity's centres. A treasury builds its realm's works where its people are, and the capital still leads because it is usually the largest centre.

> **RESOLVED.** RULED (Ben, 2026-09-19): option A. Treasury points spread over the polity's centres by urban scale; the lane's constants are kept. Written into DIGITISATION.md § Beat 1; the work is BL-1056 (points size-neutral).

*Files: `src/world/history_sim.cpp`, `src/world/history_sim.hpp`, `docs/generation/DIGITISATION.md`*

### NR-899 — CALL: under the ruled share, Charcoal leads Coke 373 to 142 — and span-founded ground is left out of both Fuel Doctrine pulls
*question · raised 2026-09-19 · from BL-1056 (points size-neutral) build, cold review and fix round; figures from the main session's chain 9 on 2867cdaa (digitisation_sim_harness --through 1960, 16 library seeds).*

NR-896 ruled both pulls read the share of held regions scoring over the world mean, to stop breadth deciding the fork. It did that: Charcoal polities are now the wooded ones (mean forest share 666 against Coke's 271; Charcoal wooder than Coke on 14 of 16 seeds). But the split moved further toward Charcoal, not back: coke 142 / charcoal 373 / neither 341 (best-of-held was 200 / 319 / 337; before forest was scored, 391 / 132 / 333). Coal scores sit on few regions, so few realms have a large SHARE of held ground over the mean on fuel, while forest is widespread. Also taken on your behalf: regions the span founds inherit fuel at x0.7 (Default A) but no forest reading, so to keep one region set both pulls exclude them (they are 0.3% of points).

**Why it matters.** The Fuel Doctrine is the tree's central fork: coke to scale and the Blast Works, charcoal to quality and a ceiling. BL-1042 and BL-1043 measure the 1960 world it manufactures; a world where Charcoal outnumbers Coke 2.6 to 1 builds fewer Blast Works.

- Accept: ground decides the fork now, and a coal-poor world industrialising on charcoal is a legitimate outcome; judge on BL-1043's sweep.
- Keep the share for forest, but let the fuel pull read the share of held ground within reach of a seam (a coalfield serves more than its own region), so coal's pull is not starved by its rarity.
- Compare each pull against its own spread, not the mean: a region counts when it is in the top third of the world for that resource, so fuel and forest clear at the same rate world-wide.

> **Recommendation:** The third option. It keeps the share and its size-neutrality, and makes 'like with like' literal: each pull counts regions in the same top fraction of the world for its own resource, so coal's rarity stops being a handicap. If you prefer to see the world before touching it, the first.

> **RESOLVED.** RULED (Ben, 2026-09-19): option C, and founded ground stays excluded from both pulls. Each pull counts held regions in the top third of the world for its own resource, a bar fixed at the span open. Written into INDUSTRY_TREE.md § The scorer; the work is BL-1059 (top-third bar).

*Files: `src/world/history_sim.cpp`, `docs/generation/trees/INDUSTRY_TREE.md`*

### NR-900 — CALL: the top-third bar — rank every region or only those carrying the resource; and a cliff at the survey's 1000 cap
*question · raised 2026-09-19 · from BL-1059 (top-third bar) lane (commit 15713143) and its cold review, 2026-09-19. Raised first as a decision taken with a recommendation to keep it; the review showed it departs from the ruled wording, so it is re-raised as a call. Per-seed carrier counts owed from the next main-session run.*

NR-899 ruled 'the top third of every region's score ... fuel and forest clear at the same rate world-wide'. The lane ranked only NONZERO scores (k = floor(m/3) of m carriers; a tie band straddling the cut falls out whole). Two problems. (1) RARITY TILT RETURNS, milder: with fuel in 150 of 300 regions and forest in 285, nonzero-only clears 50 fuel regions (17% of the world) against 95 forest (32%); ranking every region clears about 100 of each, which is the ruling's 'same rate world-wide'. The built split is coke 239 / charcoal 277 / neither 340 (from 142 / 373 / 341). (2) A CLIFF AT THE CAP: survey scores clamp at 1000 (twice the world mean, settlement.cpp:377); if more than a third of carriers sit at 1000 the tie rule pushes them all out, the bar becomes 1001 and fuel clears NOWHERE on that world (30 capped: 30 clear; 31: none). The 16 library seeds avoid it (each bar clears 31.8-33.3% of carriers), but nothing fails if a seed hits it.

**Why it matters.** It decides which ground counts as a coalfield or a forest, so the 1960 Fuel Doctrine split BL-1043 measures and BL-1044 re-blesses.

- Rank every region's score (the ruling's words), and rank the unclamped survey share so a pile at the cap cannot empty the bar.
- Keep nonzero-only (clears a third of CARRIERS, not of the world), and rank the unclamped share.
- Keep as built, and make the harness fail when a bar reaches 1001 with three or more carriers.

> **Recommendation:** The first: it is what NR-899 says, it makes fuel and forest clear at one rate world-wide, and ranking the unclamped share removes the cliff rather than alarming on it. The per-seed carrier counts (fuel vs forest nonzero) will show how far apart the options really are; they come from the next main-session run.

> **RESOLVED.** RULED (Ben, 2026-09-19): option A. The top third is taken over every region, ranking the unclamped survey share; a region with none never clears. Written into INDUSTRY_TREE.md § The scorer; BL-1059 (top-third bar) takes a fix round before it closes.

*Files: `src/world/history_sim.cpp`, `src/world/settlement.cpp`, `tools/verify/digitisation_sim_harness.cpp`, `docs/generation/trees/INDUSTRY_TREE.md`*

### NR-901 — CALL: industry points on ground with no town reach no campaign centre — on seed 0, 44% of the stock
*question · raised 2026-09-19 · from BL-1042 (stockpile to budget) lane and its cold review, 2026-09-19 (static). The lane's figures: seed 0 stock 56.2M, 24.97M of it 'no_carved_centre' in 13 regions; seeds 28 and 46 lose 2.2M each.*

The carve makes campaign centres only on regions with people AND centres (population_generation.cpp:174), so points sitting on a region with no centres at 1960 reach nobody; BL-1042 counts them unspent. The review traced two ways points land there, and ruled out a sim/carve mismatch. (a) A region earned points, then lost its towns: scale points need centres only at the moment of credit (history_sim.cpp:8666), and a sack can round centres to 0 while people remain (settlement.cpp:2460-2465). (b) The treasury fallback: when a polity holds no region with centres, BL-1056 lands the whole round's treasury credit on its capital (history_sim.cpp:1069) — which, by the same condition, has no towns. The treasury is really debited (:1083) and the handoff then discards the points. Concentration in 13 regions points to (b); the per-region treasury tally will separate them on the next run.

**Why it matters.** BL-1044 sets the charter prices against this budget; on seed 0 it is missing nearly half its stock, and (b) debits a treasury for works that never exist.

- (b) converts nothing when the polity holds no town — the treasury keeps its money, since there is nowhere to build; (a) stays unspent under a 'razed' reason, because war destroyed those works.
- Both reach the nearest carved centre of the same nation at the handoff.
- Keep as built: both unspent under one reason; judge on BL-1043.

> **Recommendation:** The first. It stops a treasury paying for works with nowhere to stand (the NR-897 principle: a treasury builds where its people are), and it gives the razed case a visible in-world cause rather than a silent loss.

> **RESOLVED.** RULED (Ben, 2026-09-19): option A. A polity holding no town converts no treasury to points (the treasury keeps it); points on a region whose towns were razed are counted unspent as razed. Written into DIGITISATION.md § Beat 1; built in BL-1042 (stockpile to budget)'s fix round.

*Files: `src/world/history_sim.cpp`, `src/world/stockpile_budget.cpp`, `src/world/stockpile_budget.hpp`, `docs/generation/DIGITISATION.md`*

### NR-902 — CALL: the density ceiling — 120 or 160 firms per body — read off BL-1039's 21 timed rows
*question · raised 2026-09-19 · from BL-1039 (charter spend rules) R6. player_seed_sweep --charter-cost on seeds 0, 28, 46 at 1x/2x/4x under the square-root rule (c = 8, B on firms, goods in turn), ceilings 120 and 160; quiet machine, main at ab9c38f8, Release /O2 MSVC 14.44.35207, serial, 7126 s; folded into charter_cost_sweep.json as runs.bl1039_sqrt. Every row: spend rules PASS at land, points balance PASS, nothing threw; the none rows match the pins. The reference budget's definition was already ruled (NR-895: B_ref = 8 x goods x firm price).*

At 1x and 2x the two ceilings give identical worlds: neither binds. At 4x: ceiling 120 lays 120 firms on every seed (12 per good, trimmed evenly, 167-190 points unspent to the ceiling); ceiling 160 lays 141/141/151 firms — it never binds, because every good reaches its own square-root cap of 15 first, so 160 is in effect no ceiling up to 4x. Live economy tick (median ms, a lower bound) at 4x, 120 vs 160: seed 0 11,582 vs 12,934 (+12%); seed 28 26,634 vs 34,614 (+30%); seed 46 13,756 vs 18,004 (+31%). The seated corporation is solvent on every row; budget specialists solvent 10 or 11 of 11 at 4x either way. These are SYNTHETIC budgets (seeded weights, not population); BL-1043's real-stockpile sweep re-reads the ceiling on real budgets. BL-1042 built with a provisional ceiling of 150; this call replaces it.

**Why it matters.** The ceiling is the only brake on firm density when budgets run large; it sets the per-tick cost of a dense 1960 world and how many firms each good carries.

- 120: it binds at 4x and trims every good evenly to 12 firms; the tick is 12-31% cheaper there than at 160.
- 160: effectively no ceiling up to 4x — each good's own cap decides; more firms, a dearer tick.
- Defer: keep BL-1042's provisional 150 and decide on BL-1043's real-stockpile rows.

> **Recommendation:** 120. A ceiling that never binds is not a brake; at 120 every good still holds firms and the dense rows cost less. BL-1043 confirms it on real budgets.

> **RESOLVED.** RULED (Ben, 2026-09-19): 120 firms per body. Written into DIGITISATION.md (the square-root cap paragraph); BL-1060 (charter spend hardening) replaces BL-1042's provisional 150 with it. BL-1043's real-stockpile sweep reads it on real budgets.

*Files: `src/world/stockpile_budget.hpp`, `src/world/charter_budget.hpp`, `charter_cost_sweep.json`*

### NR-903 — CALL: under the goods-in-turn fill, one good that cannot be placed forfeits the centre's whole remainder
*question · raised 2026-09-19 · from BL-1039 round 2 cold review (static), 2026-09-19.*

The turn's cursor moves only on a charter, and a failed placement ends the centre (corporation_generation.cpp:3287-3289, 3343-3351, 3396-3397). A raw good with no recipe gets an EXTRACTION firm, which needs an unoccupied non-urban deposit tile in the window (placement_rules.cpp:112-122); a processing firm does not. So a city centre whose radius-4 window has land but no free deposit tile stops at agricultural_produce (a low index, reached every pass) and books the rest window_exhausted, though processing goods later in the turn would have placed; the next centre on the body starts on the same good. Legacy has the same break, but the turn brings every extraction good up every pass. On seeds 0/28/46 at 4x it costs 15-17 points (legacy 12). It follows the ruling's letter; 'forfeit vs skip' was settled in a code comment, not asked.

**Why it matters.** Once the span defaults on (BL-1044) the app runs this turn on real stockpile budgets, where dense city windows are the common case; R9's 'every good with demand holds firms' can fail there.

- Skip: a good that cannot be placed in this centre's window is passed over for this centre and the turn moves on; the centre ends only when no good in the turn can place.
- Forfeit (as built): the centre ends at the first failed placement.

> **Recommendation:** Skip. A dense city with no free deposit tile should still charter the mills and works it has room for; forfeiting them books a city's budget as unspent for want of a quarry.

> **RESOLVED.** RULED (Ben, 2026-09-19): option A, skip. A good with no free site in a centre's window is passed over for that centre; the centre stops only when no good in the turn can place. Written into DIGITISATION.md; built with BL-1060 (charter spend hardening).

*Files: `src/world/corporation_generation.cpp`*

### NR-904 — CALL: a tie band at the top-third cut clears whole or falls out whole
*question · raised 2026-09-19 · from Cold check of BL-1059 (top-third bar) fix round 4a7bb291, 2026-09-19.*

The forest share is a proportion of tiles, capped at 1000 per mille by definition (settlement.cpp:396). The built tie rule pushes a band straddling the cut out whole (at most a third clears), so if more than a third of regions are fully wooded the forest bar becomes 1001 and forest clears nowhere: the NR-900 cliff surviving for forest. No library seed hit it; the harness gate would catch it, the shipped sim would not.

**Why it matters.** Which ground counts as forest for the Fuel Doctrine on a heavily wooded world.

- The tie clears whole: a bar can never empty; more than a third may clear on a wooded world.
- The tie falls out whole, as built: never more than a third; a bar can empty.

> **Recommendation:** The first.

> **RESOLVED.** RULED (Ben, 2026-09-19): option A. Regions tied at the cut all clear. Written into INDUSTRY_TREE.md § The scorer; BL-1059 (top-third bar) carries it.

*Files: `src/world/history_sim.cpp`, `docs/generation/trees/INDUSTRY_TREE.md`*

