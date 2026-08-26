# Next session — Sprint 20 batch delivery: the profitability ledger and a viable spawn

Sprint 20 opened 2026-08-26. The design is settled and written into the authority docs; the
items are minted and seated. What remains is **build**, and Ben's instruction closing the
design session was to move to **batch delivery**.

## The sprint in one line

Land the quarterly return and its surfaces, then prove the start on Ben's own criterion
(2026-08-26): *a corporation can save up over a few economy ticks to buy another company, and
still be making a profit afterwards.* BL-634 (acquisition viability) is the instrument that
says whether it holds.

## Where the design lives

Read the owning doc, not the backlog prose — the items carry the work, the docs carry the design.

| Subject | Doc |
|---|---|
| The record, disclosure, the buyout, its price | `docs/economy/FINANCE.md` §§ The quarterly return · Disclosure · Whole-firm acquisition |
| Ownership class, the spawn shortlist and the seat | `docs/generation/CORPORATION_GENERATION.md` §§ Pass 2b · Player corporation |
| The screen consequences and the reorder | `docs/ui/STARTUP.md` §§ The screen state machine · Handoff · The seat |
| Standing without bands | `docs/politics/RELATIONS.md` § Standing |

## Wave order, and why

1. **BL-637 (save-version reservation) first if anything runs in parallel.** BL-626 and BL-631
   both version the save format and both touch `world_save.cpp` + `components.hpp`. Either one
   agent takes both, or versions are claimed before the agents split — Sprint 19's retro asked
   for this by name after two agents claimed the same version.
2. **Wave 1** — BL-631 (ownership class), BL-626 (quarterly return), and BL-635 (spawn
   solvency) *diagnosis*. BL-635's diagnosis reads BL-626's own returns, so it starts after the
   record exists and its fix lands in wave 2.
3. **Wave 2** — BL-628 (whole-firm buyout), BL-633 (retire standing bands), BL-630 (spawn
   shortlist).
4. **Wave 3** — BL-629 (rival acquisition), BL-627 (profitability ledger), BL-634 (acquisition
   viability).

Integration, build and verification stay in the main session, as always. Agents that write code
run in separate worktrees.

## Cautions carried

1. **BL-635 gates the sprint's point.** The default spawn is measurably insolvent — Genom
   Systems at Cr −1, net −627/qtr (wire test, 2026-08-25). A corp that bleeds every quarter never
   saves up for anything, so BL-634's loop cannot start until this is fixed at its cause. Do not
   clamp the balance and do not hand out a subsidy.
2. **One golden re-bless wave, at the end, with provenance.** BL-630 moves the warm start ahead
   of seating, so every seed's opening changes. Never a dribble (the NR-596 precedent).
3. **BL-627 is design-owed and stays that way** until Ben writes its `question_log.json` pair —
   required on every surface, and the wording is his.
4. **Two open questions can move the sprint.** NR-647: does the *operational* fog go too, or only
   the financial banding — `DISCOVERY.md` is deliberately unedited pending it. NR-649: should the
   spawn shortlist carry a **depth** criterion as well as viability, since a shallow
   pure-extraction corp clears a profitability floor every time.
5. **Owed regardless.** BL-636 (live-click debt) — the dispatch form and Throughput lens, three
   sprints running, blocked on NR-622's environment problem rather than on design. The v0.1.18
   tag is still uncut; a release is Ben's to call.
