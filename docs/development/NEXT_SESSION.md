# Next session — Sprint 20: the loop map is judged; build the buyout phase-one loop

**READ THIS SECTION FIRST — it supersedes the older material below.** The 2026-08-26 session
continued past the sweep into a gameplay-loops session Ben called pivotal. Outcomes:

## The loop map (Ben's judged scores are final)

Thirteen loops mapped on x = ease of access, y = importance, published as the "Io Loop Map"
artifact (https://claude.ai/code/artifact/326932b3-97f0-4a8b-b5f3-86d3650e6d9f — Ben owns it;
the full catalogue with stories, scores and build states is on the page). Ben's re-scores over
Claude's first pass: **mercenary contracts down to (access 5, importance 6)** — strategic
operations carry an immense cost, an enemy once made is hard to stay ahead of, and wars are
not day-1 content; **space up to importance 6.5, level with research** — Era 1 is the live
build but this is ultimately a space-themed 4X. Ben's reading rule for the importance axis:
*if something is important, the player can see it immediately.*

## The phase-one thesis (Ben, 2026-08-26 — strongly held, not yet doc-settled)

The first phase of gameplay is **corporate buyout**: a ledger ranking background corps on
profit, and early buyouts, so the player grows the industry their strategy needs — "scrambling
to make enough money that major industry becomes more and more public, moving away from the
decentralised and private markets for important goods." Story: *rank the private firms by
profit and buy the ones my strategy needs, so the industry that matters becomes mine and the
economy consolidates around the winners.* It sits at (7, 9) on the map — core attention,
beside production.

Why it fits (the session's assessment): the sweep measured background firms AS the working
economy (96 of 104 corps, 67% solvent) while named specialists are broke — buyout turns that
into content; the syndicate/equity tier (BL-524, majority = control) already carries the
ownership model. Missing: the profit ledger, a deterministic valuation rule, the buyout verb
on the corp-command seam, a willingness/premium rule, and rival scoring of the same verb.

Three tensions named for the settle: (1) **privacy** — rival books are private; resolve via
"public signals only for private firms, acquisition/scale opens the books" (opacity = early
risk texture); (2) **the BL-094 identity test** — frame buyouts as owning your supply base
(passes FIELD), not portfolio play; decide which framing CONCEPT.md gets; (3) **symmetry** —
rivals must score buyouts deterministically, which is also what produces the consolidation
arc and the mid-game scarcity of acquirable industry.

**Viability definition that fell out: phase-one viability = time to first buyout** — a
measurable spawn_viability column once the verb exists.

## Work queued for off-cloud (Ben: "we will need to work on the rest off cloud")

1. Write the phase-one arc into CONCEPT.md and the buyout design into its owning doc, on
   Ben's confirm; mint the items (ledger, valuation, buyout verb, rival scoring).
2. BL-628 (retire starting unit) — ruled, refined, unbuilt; goldens re-bless in the ONE
   end-of-pass wave.
3. NR-647 (viability verdict rule) — likely superseded by time-to-first-buyout; confirm.
4. The older queue below (sweep mechanics, carried debts) still stands.

---

# Older handoff — Sprint 20: read the sweep, rule on the army, settle the forms

The 2026-08-26 session built and ran the spawn-viability sweep (BL-626, spawn viability
sweep). The data is in and the headline is settled: **every named corp is insolvent by tick 3
in every campaign-real seed**, and the cause is identified — the BL-476 starting army under
BL-454 upkeep, compounded by BL-073 debt interest over the 80-tick warm start. Full data,
arithmetic and options: NR-648. The devlog entry has the narrative.

## What waits on Ben

1. **The army ruling (NR-648).** Four options logged, none chosen: retainer upkeep for seeded
   units, warm-start interest grace, a funded spawn form, or hire-when-affordable instead of
   seeding. (a)/(b)/(d) are mechanical once ruled; (c) is the forms session.
2. **The viability verdict rule (NR-647).** Which combination of solvent / earning / active
   is "viable" — wanted before the forms session so the data is read against one definition.
   Recommendation on file: runway (balance + 4× trailing net > 0) as headline, strict
   solvent∧earning alongside.
3. **The sweep defaults review (NR-646).** Spectate default, seeds 0..N-1, trailing-year
   net, the active definition — delegated calls, reversible by flag.
4. **The forms session (BL-627, corp spawn forms).** Candidate forms and constraints are in
   the item; settle against the sweep data once 1–2 are ruled.

## Mechanics for whoever runs next

- Sweep: `cmake --build build --target spawn_viability`, then
  `spawn_viability [seeds] [ticks] [--played] [--lean] [--csv <path>]` from the repo root.
  Ctest label `sweep` behind `IO_RUN_SWEEPS`. Data lands in `build_gen/verify/*.csv`
  (gitignored — regenerate, don't hunt for it).
- The 2026-08-26 session ran it on Linux by extending the NR-264 recipe with Lua 5.4.7 +
  sol2 v3.5.0 (GitHub mirrors); folding that into `build_harness.js` would close part of the
  NR-558 live-Lua gap — unfiled, mention-only.

## Carried debts (unchanged from the 2026-08-25 close-out)

- Two live clicks owed: dispatch form, Throughput lens (container access, three sprints
  running) — in Sprint 20's definition of done.
- Dispatch form UX fixes (pool-stock pre-check, priced-leg preview) await Ben's
  A/backlog-or-B/build-now call.
- One deliberate baseline re-bless wave at the END of the viability pass, with provenance
  (NR-596 precedent) — not a dribble.
- `--serve`'s 12-tick default vs the app's 80 (stale `main.cpp` comment) — align before
  trusting wire-test numbers against sweep numbers.
