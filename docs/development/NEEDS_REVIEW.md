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

*18 entries — 1 open, 17 resolved.*

---

## Open

### NR-913 — CALL: density does not follow cities on the shipped world — at 580:2 most centres cannot buy one firm, and ~79% of every stock goes unspent
*question · raised 2026-09-22 · from BL-1044 Step 2 (digitisation_sim_harness --through 1960, reading 1, 16 library seeds; player_seed_sweep --digest on the shipped arc, the charter accounts), main session, 2026-09-22 — BL-1044's done-when reading.*

BL-1044's done-when reading is 'density follows cities': firm count per market against the catchment's urban population, above its good count (BL-1029's baseline on the legacy web: rho 0.431 against 0.460). On the shipped world at the pins it does not: rho(firms, urban) median 0.138 (p25 0.095, p75 0.177), rho(firms, goods) median 0.167; urban beats goods on 5 of 16 worlds. The cause is in the charter accounts. A firm costs 1/580 of the WHOLE stock, but the stock is split over 5,579 to 15,240 carved centres, so most centres hold less than one firm's price: the median world spends 16.6% of its stock and leaves 78.8% unspent as `remainder` (seed 46: 97.4%, seed 25: 94.7%, seed 32: 56.4%). The web is bought by the few dozen richest centres — 7 to 92 firms a world, median 65, against the legacy web's 81 on every seed — so it cannot track cities across 148 to 538 markets. The cost mode's per-centre reading says the same (seed 46: rho -0.005 over 15,240 centres).

**Why it matters.** It is the phase's own claim (DIGITISATION.md § 1: the budget is the stockpile, not headcount, so density following cities is MEANINGFUL) and BL-1044's done-when. The re-bless would pin a world whose charter web sits in its great cities and nowhere else. Tick cost and the seat menu are separate knobs (NR-908); this is where the points go.

- A: accept — capital concentrates, and firms stand where it did; restate the done-when as 'density follows the richest cities' and re-bless.
- B: pool what a centre cannot spend: a centre's remainder rolls up (to its region, or its nation's capital) and buys firms there, so the stock is spent rather than stranded; the seat menu is untouched (it reads a centre's own points).
- C: price a firm off the centres, not the whole stock (e.g. the median centre's budget over a divisor), so a typical city can buy one; the specialist stays m x the whole-stock price. Density rises, and so does the tick — measured against NR-908's live-play band.
- D: hold the re-bless and re-measure the done-when on a small sweep of B or C first.

> **Recommendation:** D, measuring B first: it spends the stranded points without touching the price rulings (NR-907, NR-908, NR-910) or the seat menu, and its cost is one reading. A is legitimate if Ben reads 'cities' as 'the capitals', but it is a different phase claim than § 1 makes.

> **RESOLVED.** INTERIM RULING (Ben, 2026-09-22, the Gate 2 form): D — hold the re-bless and measure pooling (option B) first. The adoption call follows the reading. MEASURED (2026-09-22, digitisation_sim_harness --through 1960 at 650:2, 16 library seeds, pooling off by default): rho(firms, urban) / rho(firms, goods) medians, and worlds where urban beats goods — none 0.139 / 0.167, 5 of 16 (65.5 firms a world, 15.2% of the stock spent); region 0.186 / 0.195, 10 of 16 (109 firms, 19.8%); market 0.211 / 0.204, 7 of 16 (120 firms on every seed, 21.1%); nation 0.156 / 0.213, 3 of 16 (120 firms, 21.1%). The density ceiling (120 a body) binds under every pool, so ~79% of each stock stays unspent — now under the ceiling, not the price. None approaches BL-1029's legacy web (0.431 / 0.460): at most 120 firms over 148-538 markets leaves most markets empty. AND the seat menu is NOT untouched: affording is, but pooled firms at a group's richest centre take ground before poorer centres place their specialists (seed 41: 65 seated unpooled, 44 region, 29 market). The adoption call is Ben's.

*Files: `docs/generation/DIGITISATION.md`, `src/world/corporation_generation.cpp`, `src/world/stockpile_budget.hpp`, `tools/verify/digitisation_sim_harness.cpp`*

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

### NR-905 — CALL: skip (NR-903) plus a binding ceiling (NR-902) can leave a good with no firms
*question · raised 2026-09-19 · from BL-1060 (charter spend hardening) cold review, 2026-09-19 (static).*

If the first centre walked has no free deposit tile and its budget alone reaches the 120 ceiling, it spends everything on processing goods; later centres with quarries stop at density_ceiling on their first firm, and every extraction good ends at 0 firms — against DIGITISATION.md's 'the ceiling trims every good evenly' and NR-903's own reason.

**Why it matters.** Whether every good with demand holds firms on a dense body once BL-1044 turns the spend on.

- When the ceiling binds each good holds at most its even share of it; a share no centre can place is unspent under its own reason.
- The next centre serves a skipped good first (still starves if the first centre fills the ceiling).
- Accept as built; judge on BL-1043.

> **Recommendation:** The first.

> **RESOLVED.** RULED (Ben, 2026-09-19): option A. Written into DIGITISATION.md (the goods-in-turn paragraph); built in BL-1060 round 2.

*Files: `src/world/corporation_generation.cpp`, `src/world/charter_budget.hpp`, `docs/generation/DIGITISATION.md`*

### NR-906 — DECISION TAKEN: an even share of the ceiling is a reservation, not a cap; the yards come off the ceiling first
*decision taken on your behalf · raised 2026-09-19 · from Cold check of BL-1060 round 2 (2886d20f), 2026-09-19.*

NR-905 was written into DIGITISATION.md as 'a good may take AT MOST its equal part' and built that way. The check found the cap holds room back: if one good stops being short at 6 firms, 9x12 + 6 + 1 = 115, the other nine stall at 12 while still short, the ceiling never fills and later centres book their points no_gap (a false label); near the threshold a single unplaceable good drops a body that would never have hit the ceiling from 118 to 109 firms. And the yard takes a place under the ceiling but outside the shares, so its place comes out of the skipped goods. Taken on your behalf, reading your option 'each good keeps an even share': the share is a RESERVATION — a good may go beyond its share while the room left still covers every other short good's unfilled share — and each yard's place comes off the ceiling before it is shared. Also: a ceiling smaller than the turn is refused rather than silently switching the shares off.

**Why it matters.** Whether a binding ceiling still fills, and whether the unspent reasons BL-1043 reads are true.

> **Recommendation:** Keep it: it is what 'keeps an even share' means, and it makes the ceiling fill.

> **RESOLVED.** CONFIRMED (Ben, 2026-09-19): the reservation reading is right, keep it.

*Files: `src/world/corporation_generation.cpp`, `src/world/charter_budget.hpp`, `docs/generation/DIGITISATION.md`*

### NR-907 — CALL: one specialist price cannot hold the seat menu, because the library's stockpiles vary five-fold and seats track them
*question · raised 2026-09-21 · from BL-1043 (real-stockpile charter sweep) stage 1: 16 library seeds x firm price 10000/20000/40000 x specialist 4 firm charters, three parallel shards, folded into charter_cost_sweep.json as runs.real_stockpile_bl1043_a/_b/_c (main session, 2026-09-20/21).*

The seat-menu anchor (NR-886 and the sprint 45 elicitation) is that the MEDIAN library world offers about as many seats as a world with no budget. Measured over all 16 seeds, the legacy anchor is a median of 9 specialists (range 6-17) and does NOT track the stockpile: seed 31 holds 95.3M points and offers 6, seed 9 holds 27.5M and offers 16. The budget's seats DO track it almost proportionally. Medians: firm price 10000 -> 107.5 seats (range 20-412, 0 of 16 seeds at or under the anchor), 20000 -> 50.5 (8-167, 1 of 16), 40000 -> 23.5 (1-91, 7 of 16). Stockpiles run 24.7M (seed 28) to 133.6M (seed 11), and at 40000 the poorest world offers 1 seat while the richest offers 91. Pricing for the median (about 100000, extrapolating the halving per doubling) starves the poorer half below the legacy roster; pricing for the poorest floods the richest. The firm count is at the 120 ceiling on 16 of 16 seeds at 10000 and 11 of 16 at 40000, so the ceiling binds throughout. NO seed produced a no-specialist world at any tested price.

**Why it matters.** BL-1044 pins these prices at the re-bless; the seat menu is what the player chooses from on the select-corporation screen (sprint 46).

- Price a specialist as a SHARE of the world's own stockpile, so the menu is about the same size library-wide (a build, not a constant: the price stops being a fixed number and becomes a derived one, deterministic and seeded).
- Keep a fixed price and accept the spread: a world that industrialised harder offers more seats, which is the world talking rather than a knob. Then the price is chosen against the RICHEST world's menu being usable, and the anchor's 'median' wording is what changes.
- Keep a fixed low price and cap the MENU instead: the shortlist shows at most N seats however many the budget opens.

> **Recommendation:** The first if the menu's size is a UI contract; the second if the menu is meant to read the world. I lean to the first, because the anchor was written to keep the menu comparable across worlds, and a derived price keeps that without a clamp — but it changes what 'the specialist price' means, so it is yours. Either way stage 2's prices depend on it, so stage 2 is held.

> **RESOLVED.** RULED (Ben, 2026-09-21): option A — a charter's price is a SHARE of the world's own industry stockpile, fixed once when the budget is built, so the seat menu is about the same size on every world. Written into DIGITISATION.md; built by BL-1064 (derived charter price), whose sweep reads the constant. Ben also picked 80k/160k/320k for stage 2, but that answer was the FIXED-price fallback; under option A stage 2 sweeps the divisor instead.

*Files: `src/world/stockpile_budget.hpp`, `tools/verify/player_seed_sweep.cpp`, `docs/generation/DIGITISATION.md`, `docs/generation/CORPORATION_GENERATION.md`*

### NR-908 — CALL: under a derived price, which knob answers the seat menu — the divisor or the specialist price in firm charters?
*question · raised 2026-09-21 · from BL-1064 (derived charter price), reading the generation docs before stage 2 (main session, 2026-09-21). Stage 1's table (runs.real_stockpile_bl1043_a/_b/_c).*

NR-907 made a firm charter's price the world's stock over a divisor d, with a specialist still costing m firm charters. A centre then affords a specialist when its points cover m/d of the world's stock, so the seat menu turns on d/m ALONE: stage 1 gives seats ~ 0.056 x d / m on the median world (the median of seats x firm price / stock over 48 rows is 0.01395 at m = 4), so the anchor's 9 seats is d/m ~ 160. Firm density — how many charters a world's stock buys — turns on d alone. BL-1064 as filed pinned d to the seat menu with m frozen at 4, which leaves density with no knob; DIGITISATION.md § 1 and its hard-list item 7 set the FIRM price against live-play cost and the 2026-09-18 ruling anchors the SPECIALIST price (in firm charters) to the seat menu. The handoff's 'divisor near 150' was d/m, and its seat direction was inverted (a larger d/m is a cheaper seat and MORE seats).

**Why it matters.** It decides what stage 2 of BL-1043 measures and what BL-1044 pins: one knob or two.

- A: the divisor answers live-play cost and m answers the seat menu (the doc's split); stage 2 holds d/m near 160 while d moves.
- B: the divisor answers the seat menu with m frozen at 4 (BL-1064 as filed); density is whatever that divisor gives.

> **Recommendation:** A: it is what the doc already rules, and it gives each knob one job.

> **RESOLVED.** RULED (Ben, 2026-09-21, elicitation): A — the divisor answers live-play cost, m answers the seat menu. Written into DIGITISATION.md § 1. Stage 2 is FIVE rows per seed (plus the legacy row): (divisor, m) = (450, 4), (650, 4), (900, 4) for the seat menu along d/m = 112 / 162 / 225, and (325, 2), (1300, 8) for density at the ratio 162. The sweep takes them as --price-pairs.

*Files: `docs/generation/DIGITISATION.md`, `src/world/stockpile_budget.hpp`, `tools/verify/player_seed_sweep.cpp`*

### NR-909 — CALL: once the span is on by default, what do --verify, --serve and the headless run build?
*question · raised 2026-09-21 · from Planning BL-1044 (Beat 1 ships), main session, 2026-09-21, reading the three search-less start paths.*

Three start paths never search: app::run_verify (verify_api.cpp:529), run_serve (main.cpp:148) and the headless run (main.cpp:258). Each calls generate_background_firms directly and only WARNS when the stockpile budget is non-empty; today that never fires, because they build from default world_params with the Digitisation span off. BL-1044 flips the span on by default, so from then every --verify capture, every MCP/--serve session and every headless run builds a span world and lays the legacy web over it, ignoring its budget — a world the app never builds. None of these paths searches today either (they lay the legacy web with a fixed seed, not the search winner), so none of them was ever the exact shipped world.

**Why it matters.** --verify is how a UI change is checked and --serve is the AI seam; both would silently show a hybrid world, and their guards would print on every run.

- A: spend the budget on the SEED CANDIDATE (the harness's unsearched apply, apply_shipped_landscape with search = false): a non-empty budget charters as the app does, minus the search; an empty budget keeps today's call byte for byte.
- B: pin the span OFF on the three paths: they keep today's world exactly, and check a world the player no longer gets.
- C: run the full search there too: the exact shipped world, at 20-60 s more per verify run.

> **Recommendation:** A. It shows the budget world at no search cost, matches how these paths already skip the search, and leaves a span-off world untouched. B keeps verify on a world nobody plays; C taxes every UI check for the search's choice of roster, which verify has never checked.

> **RESOLVED.** RULED (Ben, 2026-09-21, the charter pin form): A — where the world carries a charter budget, --verify, --serve and the headless run spend it on the search's seed candidate (the harness's unsearched apply); an empty budget lays exactly today's web. Written into DEVELOPMENT_PRACTICES.md § A harness must build the world the application builds; built by BL-1044 (T5).

*Files: `src/core/verify_api.cpp`, `src/main.cpp`, `tools/verify/harness_params.hpp`*

### NR-910 — CALLS: the charter pins for BL-1044 — the price pair, the seat spread, the no-specialist world, the province cap, the sqrt base
*question · raised 2026-09-21 · from BL-1043 stage 2 (runs.real_stockpile_bl1043_stage2, 16 seeds x NR-908's five pairs, serial) and the seat curve (stockpile_budget_check --seat-curve, 16 seeds, d/m 162-1300), main session, 2026-09-21.*

Seats turn on d/m alone and follow the curve median 3 / 4 / 13.5 / 28 / 54 at d/m 162 / 225 / 325 / 450 / 650, so the anchor's 9 sits near d/m 290. Live tick against the legacy world: x0.43 / x0.91 / x1.70 at 325:2 / 650:4 / 1300:8, so the divisor near 650 runs the legacy cost. Whole charters are too coarse: at d = 650, m = 3 opens ~4 and m = 2 ~13.5. The spread at the anchor is wide (3-98 at d/m 325) because worlds with many near-equal cities cross the price together — the derived price removes the stock's size, not the world's shape. No-specialist worlds occur below d/m ~325 (seeds 46, 37, 25) and none above. The province cap binds on 7 of 16 worlds at 650:4, up to 24.6% of one budget.

**Why it matters.** BL-1044 pins these and re-blesses the shipped world on them.

- Price pair: A m 2 with the divisor tuned near 580 so the median lands at 9 / B 650:2 (median 13.5) / C m 3 near 870.
- Seat spread: accept (the anchor is a median) / cap the menu at N.
- No-specialist world: fall back to the no-budget world / unseatable, re-roll / seat on a background firm.
- Province cap: keep 2 / raise to 3 / lift. Sqrt base: pin 8 / measure another.

> **Recommendation:** A; accept; fall back; keep 2; pin 8.

> **RESOLVED.** RULED (Ben, 2026-09-21, the charter pin form), all as recommended: a specialist costs TWO firm charters and the divisor is the one at which the median library world opens nine seats at two (near 580, read off the seat curve); the seat spread is accepted — the anchor is a median; a world whose budget opens no specialist falls back to the no-budget world, as a refused spend does; the per-province cap stays at 2; the sqrt base is 8. Written into DIGITISATION.md § 1 and CORPORATION_GENERATION.md § Pass 1; BL-1044 pins them. PINNED (seat curve at m = 2, 16 seeds, 2026-09-21): divisor 580 — median 6.5 / 7 / 8 / 9.5 / 12.5 seats at d = 520 / 540 / 560 / 580 / 600, and 580 is the smallest divisor measured at which no library world falls back (seed 37: 0 at 560, 2 at 580); spread 2 to 73 at 580:2.

*Files: `docs/generation/DIGITISATION.md`, `docs/generation/CORPORATION_GENERATION.md`, `src/world/stockpile_budget.hpp`*

### NR-911 — CALL: the no-specialist residual — a budget that affords a specialist but places none leaves no player
*question · raised 2026-09-22 · from Building BL-1044 (Beat 1 ships), T4, main session, 2026-09-22, reading charter_web_from_budget against NR-910's fallback.*

NR-910 rules the no-specialist world falls back to the no-budget world, 'decided from the budget before anything is chartered'. BL-1044 builds exactly that: the search and the apply ask charter_budget_affords_specialist (a centre on a nation's tile whose points cover the specialist) and lay the no-budget world when none does. But affording is not placing. A centre that affords a specialist and finds no ground in either window charters none (corporation_generation.cpp, the walk's 'no ground in either window' branch). If every affording centre does so, the walk ends with no specialist, nobody is picked, spawn_seat finds nobody (spawn_seat.cpp:118), and the world has no player. That cannot be decided before the walk: a poorer centre's specialist competes for ground with richer centres' firms. It is not hypothetical in kind: the seat curve (centres affording, off the budget) matched the charted seats on 74 of 80 stage 2 rows, so on 6 rows placement differed from affording. Whether it ever reaches zero on the library at 580:2 (minimum 2 affording) is what BL-1044's measurement counts.

**Why it matters.** The app would start a campaign with no player corporation. BL-1044 states the invariant where it binds (world.hpp), prints the count on the app's seat line and fails a player_seed_sweep row (and guard S7) that seats other than one, so the case is loud, never silent. What the world should do then is a design call.

- A: extend the fallback past the walk — lay the budget world on a copy, and if it charters no specialist lay the no-budget world instead. Exact, but it is decided AFTER chartering, which the ruling's wording excludes, and costs a world copy per apply.
- B: seat the richest affording centre's specialist on a relaxed rung (a third anchor rung, nation-wide) when the walk would otherwise end with none. Keeps the budget world; adds the nation-wide scatter § 1 forbids for every other charter.
- C: leave it reported and counted, and act only if BL-1044's measurement (or any later library run) ever shows it: the invariant is loud today.

> **Recommendation:** C now, with A as the fix if it is ever measured. A is the only option that keeps both the ruling's outcome (the no-budget world) and § 1's placement rule; its cost matters only if the case occurs.

> **RESOLVED.** RULED (Ben, 2026-09-22, the Gate 2 form): C — reported and counted, fixed only if measured. No library world hit it in BL-1044 Step 2 (16 of 16 seat one player). Written into DIGITISATION.md § 1 and CORPORATION_GENERATION.md § Pass 1.

*Files: `src/world/corporation_generation.cpp`, `src/world/landscape_search.cpp`, `src/world/world.hpp`, `docs/generation/DIGITISATION.md`*

### NR-912 — CALL: the treasury credit rate on the 1960 close — the median nation opens near zero, and Pass 7's two scales are stale
*question · raised 2026-09-22 · from BL-1044 Step 2 (era_world_harness R7 on seed 0; digitisation_sim_harness's setup-diff line on 16 library seeds), main session, 2026-09-22 — the reading BL-1053 (1) left owed.*

With the span on, a nation's opening treasury is its folded polities' chest at the 1960 close x 0.01 per mille (NATION_GENERATION.md § Pass 7). The 1960 chests total 0.62-0.91x the 1660 ones across the library (median ~0.84; seed 0: 2.74B against 3.27B material), as BL-1053 forecast. But the rate's own justification no longer holds on the reference seed, and did not hold at 1660 either: § Pass 7 says 0.01 per mille opens the median nation on ~100 credits (thirteen quarters of levy) and sums the 38 treasuries to ~15,300 with the richest on ~3,800. Measured on seed 0 today: the 38 sum to 31,691 credits, the richest opens on 5,420, and the MEDIAN nation opens on 0.41 — 22 of 38 nations hold under one credit, 14 hold 100 or more. The '~100 for the median' figure read the median POLITY chest (~10M), not the median nation's folded chest (~41k material).

**Why it matters.** The rate is a stated conversion chosen against two scales; the scales are the reason for the number. As measured, most nations open broke and a handful hold a corporate field's cash between them, which is the opposite of 'a reserve, not a hoard' for the median realm. It is also a doc claim that is false today.

- A: keep 0.01 per mille and restate § Pass 7's scales from the shipped world (the sum, the top realm, the median near zero): the skew is history's, and a broke majority is a legible opening.
- B: keep the rate and give every folded nation a floor (e.g. one quarter's levy), so the median nation is not broke; the top is unchanged.
- C: re-derive the rate against the median NATION on the shipped world (a larger rate lifts the median and multiplies the top with it).

> **Recommendation:** A, with the restated numbers written into § Pass 7 by BL-1044 — the skew is the world talking, as the seat spread was (NR-910). B is the lever if a broke majority reads badly in play; C scales the hoard with the median and breaks the second scale.

> **RESOLVED.** RULED (Ben, 2026-09-22, the Gate 2 form): A — keep 0.01 per mille and restate Pass 7's scales from the shipped world. Written into NATION_GENERATION.md § Pass 7 (median nation 0.41 credits, 22 of 38 under one, 14 at 100+, sum 31,691, richest 5,420 on the reference seed).

*Files: `docs/generation/NATION_GENERATION.md`, `src/world/nation_generation.hpp`, `tools/verify/era_world_harness.cpp`*

### NR-914 — CALL: the pinned divisor opens a median 6.5 seats on the shipped world, not the anchor's 9 — the seat curve moved when the tier turned on
*question · raised 2026-09-22 · from BL-1044 Step 2 (player_seed_sweep --digest on the shipped arc; stockpile_budget_check --seat-curve on the shipped world, 16 library seeds), main session, 2026-09-22.*

NR-910 ruled the RULE — the divisor at which the median library world opens the anchor's nine seats at two firm charters — and read 580 off a seat curve taken with BL-1037's corridor tier OFF. BL-1044 turns the tier on, and the tier moves every stockpile (it changes Exploration's resume, and so the 1660 handoff the span opens on). On the shipped world at 580:2 the median library world affords 6.5 specialists and seats 6.5 (min 3, max 48; no world falls back, every world seats one player). The shipped curve at m = 2 (median centres affording): 450 4.0 | 500 5.0 | 540 5.5 | 580 6.5 | 620 8.0 | 660 13.5 | 700 14.5 | 800 17.5, none ever zero. So the anchor now sits between 620 and 660; a finer curve (600-660 by 10) is queued to name the number. The step is steep because worlds with many near-equal cities cross together (seed 12: 4 -> 52 between 620 and 660; seed 41: 7 -> 73 between 580 and 620). SEPARATELY, affording is not seating on the rich-city worlds: seeds 31, 40, 32, 38 and 9 afford 37 / 50 / 66 / 61 / 19 and seat 26 / 37 / 48 / 41 / 15 — a poorer centre's specialist finds its window taken by richer centres' firms (window_exhausted, up to 79 centres a world). The seat curve counts affording, so it overstates the menu where it is widest.

**Why it matters.** The re-bless pins the shipped world's digests on whatever divisor ships. Re-pinning after would cost a second re-bless.

- A: apply the ruled rule to the shipped world — re-pin the divisor to the value the fine curve gives for a median of nine at m = 2 (~630-650), measure its tick, then re-bless.
- B: keep 580 — the seat menu is a median, 6.5 is near the anchor, and the tick is lower.
- C: re-pin on SEATED rather than affording specialists (the digest run's count), which reads slightly higher divisors on the rich-city worlds.

> **Recommendation:** A: the ruling is the anchor, and 580 was its reading on a world that no longer ships. The tick at ~640 sits between stage 2's x0.43 (325) and x0.91 (650), inside the band. Take the number off the fine curve (affording, as NR-910 did), and note the placement loss beside it rather than chasing it with the divisor.

> **RESOLVED.** RULED (Ben, 2026-09-22, the Gate 2 form): A — apply the ruled rule to the shipped world: re-pin the divisor to the value the fine seat curve (affording, m = 2, 16 library seeds) gives for a median of nine. The number is pinned by BL-1044 and written into DIGITISATION.md § 1. PINNED 650 (2026-09-22): the fine shipped seat curve at m = 2 reads median 7.5 / 7.5 / 8 / 8.5 / 8.5 / 9 / 13.5 at d = 600-660 by 10; 650 is the first divisor at nine, none opens zero, spread 4 to 98; its tick near x0.91 legacy (stage 2).

*Files: `src/world/stockpile_budget.hpp`, `docs/generation/DIGITISATION.md`, `tools/verify/stockpile_budget_check.cpp`*

