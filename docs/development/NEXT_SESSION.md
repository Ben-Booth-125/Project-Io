# Next session — re-planning from a clean sheet

**The whole board was cleared on 2026-09-08.** Ben's call: too many items hung off sprint plans
that should have been better planned. The live backlog (**31 items**), the review queue (**6
entries**) and the sprint list (**32c, 33, 34**) were all archived. **Nothing below is an open
commitment.** It is kept as the input to the next planning pass — the findings are real, the item
numbers are not live work. Cancelled rows are whole and cold in `archive/backlog-design-2026-Q3.json`
(`backlog_query.js --status cancelled --full`, or `archive_landed.js --restore`); the queue entries
are in `archive/needs-review-2026-Q3.json`; the sprints in `archive/sprints-2026-Q3.json`.

Sprint 32b closed 2026-09-06 with **eleven items delivered**; 32c had carried the remaining 28.
`docs/development/SPRINTS.md` § Sprint 32c was the plan.

## Start here — two independent chains

**1. The water model reaches its judgement point.** BL-776 and BL-777 landed: coastal water and
lakes are owned, open ocean is not, and no region anchors on open ocean. Remaining:

- **BL-778** (unit traversal domains) — a roster row declares which domains it crosses; land units
  may cross **owned** coastal water, the deliberate middle case. **Gate on `region::domain`, never
  on `port_q`** — that is a decayed wetness fraction that counts lakes and is inherited at 0.7×
  without re-surveying. `HISTORY.md` now says so explicitly.
- **BL-779** (naval rows become real) — `unit_class::naval` returns base power 0 and `sum_stack`
  skips the class, so three authored port-gated rows are worth nothing.
- **BL-780** (the ONE re-bless) — read the warning below before touching it.

**2. Phase 6 gets its search.** The objective can finally see a roster (BL-770 slices 1–2). What is
missing is candidate generation, parallel evaluation and a deterministic argmax —
`landscape_score.cpp` still has **no caller outside its own harness**. Building it unblocks BL-772
(retire the warm start) and BL-773 (the 3–6 minute budget).

## BL-780 carries two problems it did not create

**FOUR CAUSES, NOT ONE.** BL-780 was designed as the single point where the *water model* moves the
world once and a human asks whether the new world is better. Wave 1 ran three reorder items
alongside it, so the movement is now the water carve **plus** the population map **plus** the empire
forces **plus** the paleo deposits. Every item measured its own before/after in isolation, so the
causes stay attributable — but the question is no longer simple, and the description Ben authorises
against must name all four.

**AND THE CENTRAL CLAIM CANNOT BE SEEN — NR-791.** The hover card over water reports terrain and
habitability and says nothing about an owner; clicking water does not update the Selection panel; no
lens colours territory by owner. BL-780 asks for a judgement *by looking* at a change that is
currently invisible on every surface the game has. **Close that before the re-bless, not after** —
the recommendation on the entry is to put ownership on the water hover card, since the card already
reads the tile.

## What Ben spotted that no harness could — BL-784

**One nation comes out with a complete road lattice** while its neighbours carry the sparse trunk
shape BL-768 intended. The aggregate was right (+320 roaded tiles era-ON) and the *per-nation
distribution* was wrong — and nothing reports per-nation road density.

Likely mechanism, to confirm rather than assume: BL-768 records a corridor at the **settle** path
too, `(parent, daughter)` for every founding, and the run is settle-dominated by design (833
foundings against 270 battles). A polity that expanded by settling has a corridor from every parent
to every daughter, which over a contiguous holding **is** a spanning lattice. The corridor histogram
agrees: 3,119 of 3,185 walked exactly once — a founding tree, not a trade network.

The design question underneath is not a tuning one: **is a founding line a trade corridor at all?**
A parent settling a daughter walked that ground once; a supply line walked forty times is a road.
Weight or exclude the settle corridors rather than raising the threshold — but that is a call.

## Calls that were waiting on Ben — all archived unanswered 2026-09-07

None of these is a live queue entry any more. They are here as the questions the re-plan inherits.

| | |
|---|---|
| **NR-791** | Coastal ownership is invisible — blocks BL-780's own done-when |
| **NR-785** | Three surviving sea-leg calls; hold BL-749 and through it BL-752 |
| **NR-783** | Span boundary: authored at epoch − 400, or derived from the first furnace |
| **NR-784** | Cap the ancient arc at medieval? BL-760's counters can now answer it — nothing above medieval is ever fielded |
| **NR-787** | Stagnant lid immobile in the paleo frame — a modelling call that turned a red row green |
| **NR-790** | The fossil epoch derivation — authored, and every later paleo consumer copies its shape |
| **NR-788** | **Six** harnesses now ad hoc, awaiting skill names: `continent_drift`, `sim_water_census`, the saturation measure, `deposit_origin`, `landscape_score_harness`, `centre_region_bind` |
| **BL-758** | Era-seeded demography at 1960; `era_world_harness` R2 is deliberately **red** |

Two more from 32b, both about the furnace: is **1–4 crossers of 12** the intended outcome (BL-748's
own done-when asked for a *wide* distribution), and is **within-world tariff flatness** enough to
open BL-488's verb form?

## Standing hazards, learned the hard way this sprint

- **Exactly one item per wave may bump `save_game_version`.** Two agents bumped 4→5 independently
  and produced two layouts under one version number. It is at **8**.
- **A worktree agent cannot build `save_envelope_roundtrip`** (imgui). Every save-format change this
  sprint arrived unasserted and the integrating session had to write the check. Budget for it.
- **Any tooling fix for worktree agents must be tested FROM a worktree.** Three builder fixes landed
  and the first two verifications were run in the main checkout, where the bug could not appear.
- **`build/` is Debug.** Its generation and warm-start timings are not comparable to BL-761's
  Release figures. The Debug warm start is ~11 minutes, which makes the Debug play loop barely
  usable — a sharper argument for BL-772 than the Release number.
- **Agents' worktree bases are stale by default.** Every one this session was; all had to merge main
  before starting.

## The sprint list went too — all three, 2026-09-08

**Sprint 33 was NOT the sprint Ben named.** The market-viability sprint opened 2026-09-02 as 33 was
**renumbered to 34** on 2026-09-07; sprint 33 became the context-economy housekeeping sprint, which
then ran to **16 items delivered** across three blocks. So:

| Sprint | Closed as | Why |
|---|---|---|
| **33** context economy | `retro-recorded` | It ran. Its retro is real history and stands unchanged. |
| **34** market viability | `superseded` | Never worked. The sprint Ben meant by "remove sprint 33". |
| **32c** gamified generation | `superseded` | Its water-model chain largely landed; the remainder was cancelled, not re-promoted. |

Two records are worth keeping and are cold, not gone. Sprint **34**'s `notes` carry the measured
2026-09-02 field baseline — corps 86→61 / 71→55, valued production 20,761→4,364 / 5,775→3,531, mean
supply factor ~0.57 — which costs a full lapse run to re-measure. Sprint **33**'s retro says what
the context-economy work actually delivered.

The next new sprint is **35**, planned fresh.
