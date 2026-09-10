# Next session — close BL-868, then close sprint 38

Written 2026-09-10 at the close of a batch-delivery session that landed 8 of sprint 38's 9
items. **This note replaces the 2026-09-09 handover — that one's decomposition work is done.**

## Where things stand

All of CIVILISATION.md's Empires design is built and verified on main except one item:

| Item | Status |
|---|---|
| BL-873 (culture coining year) | complete |
| BL-871 (empire span 400 BCE–1200 CE) | complete |
| BL-866 (settlements are sparse) | complete |
| BL-837 (ancient roads / reach gate) | complete |
| BL-867 (materials spent on action) | complete |
| BL-870 (culture relations) | complete |
| BL-869 (civilisations from mixing) | complete |
| BL-872 (centres from supply/governance) | complete |
| **BL-868 (creeds raise armies)** | **open — seven attempts, unresolved** |

Read `docs/generation/CIVILISATION.md` for the design; it hasn't changed. This note is only
about the one holdout.

## BL-868 — what's actually blocking it

**This is not a wiring problem.** `w_aggr_q` (in `history_sim_params`) leans the Campaign score
by the acting polity's `aggression_q` (carried from its founding culture), proportionally and
symmetric around a neutral 500 — the same idiom as `w_cult`/`w_dist`. That part has looked
correct since the first attempt.

**It's a test-fixture problem, and now a coupled one.** Seven attempts, roughly:
1–3. Vacuous or confounded metrics (`trace_battles` off; `campaign_chosen` counts that fall
   when a symmetric arms race ends a war early rather than rise).
4–5. Separation/calibration issues — polities placed outside `neighbour_radius`'s default reach.
6. A genuinely sound design: hold the *defender's* culture at neutral aggression in both runs,
   sweep 6 seeds, measure **time to first conquest** rather than an event count. This isolates
   the real confound.
7. Rebased pass 6 onto current main (post BL-837/BL-872) and reran it: **zero conquests in
   either run**, 0/6 seeds, in 1000 years. `two_polity_world`'s fixture no longer fights at all
   under the reach gate (BL-837) and supply floor (BL-872) — both landed *after* BL-868 was
   first designed, and both are tuned against different fixtures.

**BL-868, BL-837 and BL-872 are coupled.** The aggression lean can't be demonstrated until the
fixture can actually produce a conquest under current reach/supply mechanics. Two ways in:
- Rebuild the fixture for the post-BL-837/872 world — shorter reach requirement between the two
  polities, or explicit road/supply seeding so a campaign is reachable at all.
- Or give it a much longer `stop_year` so reach and supply have time to build up before judging
  whether the lean ever gets to matter.

Either way, **check first that a conquest happens at all in the new fixture with `w_aggr_q=0`**
before reasoning about the lean — that was the mistake baked into pass 7.

The pass-6/7 code sits uncommitted in `.claude/worktrees/agent-aec88315766979641`
(commit `b6d04c6c`, rebased cleanly onto main at `ed229efb`). The test design (hold defender
neutral, measure time-to-first-conquest) is worth keeping; only the fixture needs redoing.

**Read this alongside BL-861 (sprint 37, still open, kept open 2026-09-10).** BL-861 found the
same symptom a sprint earlier — seed 0's full 4000-year span fights zero battles — and was never
resolved; its own notes name culture-by-route's contiguous kin blocks and large unclaimed
buffers as the likely causes. Worth diagnosing BL-868's silent fixture and BL-861's silent world
together rather than as two separate no-conquest mysteries.

## Also worth knowing

- **BL-887 (reach-as-centre-chains)** was filed out of this sprint, priority B, no sprint —
  Ben's deliberate call to defer a reach-model rework (chains of population centres, Logistic
  Points) until tech progression is wired into generation. Too few small polities survive
  Round 4 as it stands; this is the eventual fix, not now.
- **BL-861** (no-conquest measurement, sprint 37) and **BL-823** (anti-hegemon levers, no
  sprint) both need a re-scoping pass before implementation — their prose predates how much
  this sprint changed the underlying mechanics. Neither is sprint 38's.
- **BL-867's backlog record was found unmarked** during this session's housekeeping, despite
  its code having landed on main days earlier (`b25db602`) — the delivery commit happened, the
  bookkeeping commit didn't. Fixed same session; worth a beat of caution that a batch delivery's
  last step (mark it in `backlog.json`) is as easy to drop as any other.
