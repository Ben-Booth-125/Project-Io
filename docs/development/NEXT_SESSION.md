# Next session — sprint 35, the startup budget

Sprints 32, 33 and 34 all closed. They were **one sprint carrying six independent bodies of work**,
split twice in flight as 32a/32b/32c; Ben renumbered them to plain 32, 33 and 34 on 2026-09-08 and
opened **sprint 35** on the first strand alone. `docs/development/SPRINTS.md` § Sprint 35 is the
plan; this note is the handoff.

> **A bare "sprint 33" written before 2026-09-08 means the DELETED market-viability sprint**, not
> the water-model sprint that now holds the number. That sprint never executed — all five of its
> items were cancelled unstarted — and it was deleted under the unstarted-plans rule.

## Start here — one number

The wait between pressing *new game* and playing. Measured 2026-09-03 in the **release** play build:

| | warm start | convoys | `run_economy_step` |
|---|---|---|---|
| epoch 1960 (two-span) | 73,167 ms | 14,763.9 | 57,023.9 |
| epoch 0 (ancient) | 72,088 ms | 28,321.1 | 42,361.6 |

Generation end to end is ~8 s. The Era −1 sim this whole arc has been optimising is 197–323 ms —
**under half a percent** of what the player waits through.

- **BL-761** (warm start is 72 seconds) — first, because it is the profile every later decision is
  taken on. 530–710 ms **per tick**, which is also what the player pays per tick at speed once
  playing. This is a play-speed problem wearing a loading-screen costume.
- **BL-772** (retire the warm start) — the headline, and **its blocker is gone**: `landscape_search`
  now has a caller in `app.cpp` (landed in sprint 34). What remains is the restructure the sprint 34
  retro named — `generate_corporations` appends and runs **before** the registry loads, so phase 6
  cannot vary the specialist roster at the live seam.
- **BL-773** (3–6 minute budget) — keeps the arithmetic true as each piece lands. Phase 6 already
  costs **+20 s** of measured startup: a regression until BL-772 removes the 72 s beside it.
- **BL-754** (generation budget) — the **on-screen** half, still owed. The console half was proved
  2026-09-06; R1 is deliberately not complete because nobody has opened the app and looked at the
  generating screen.

## The one risk worth reading before starting

**Phase 6's objective is partly blind — NR-793, open, confirmed on a live world.** Road tier is
invisible to it, because the resource-coverage boolean is saturated. BL-772 hands that same
objective the warm start's whole burden. Sprint 34 already refused this trade once and was right to;
if the restructure does not land clean, taking the block again is the correct outcome.

That is why **roads is the strongest candidate to run next**, and it may turn out to be a
precondition rather than a neighbour. Decide it on a measurement, not in advance.

## The other four strands — backlog items, not sprints

Deliberately not authored as sprints: authoring five at once re-creates the sprawl that split sprint
32 three ways, and an unstarted plan is a stale reference.

| Strand | Items |
|---|---|
| **Roads mean traffic** | BL-784 (one nation roaded solid) + NR-793 |
| **Instruments** | BL-753 (generation scoreboard), BL-758 (1960 demography), BL-781 (query ignores unknown flags) |
| **Maritime close-out** | BL-786 (port on owned coastal water), BL-749 (sea-leg campaign), BL-752 (colonial ties) |
| **Deep time** | BL-764 (the Lagrangian frame) — difficulty 5, splits at promotion |

BL-786 is buildable now. BL-749 is held on three NR-785 calls, and BL-752 sits behind it.
BL-753 was repointed off cancelled BL-751 on 2026-09-08 and is **unblocked**.

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

## Open calls waiting on Ben

| | |
|---|---|
| **NR-793** | Road tier is invisible to the phase 6 objective — bears directly on sprint 35 |
| **NR-785** | Three surviving sea-leg calls; hold BL-749 and through it BL-752 |
| **NR-783** | Span boundary: authored at epoch − 400, or derived from the first furnace |
| **NR-784** | Cap the ancient arc at medieval? BL-760's counters can now answer it — nothing above medieval is ever fielded |
| **NR-787** | Stagnant lid immobile in the paleo frame — a modelling call that turned a red row green |
| **NR-790** | The fossil epoch derivation — authored, and every later paleo consumer copies its shape |
| **BL-758** | Era-seeded demography at 1960; `era_world_harness` R2 is deliberately **red** since 2026-09-03 |

## Standing hazards, learned the hard way

- **`build/` is Debug.** Its timings are not comparable to BL-761's Release figures. The Debug warm
  start is **~11 minutes**, which makes the Debug play loop barely usable — a sharper argument for
  BL-772 than the Release number is.
- **Exactly one item per wave may bump `save_game_version`.** Two agents bumped 4→5 independently
  and produced two layouts under one version number. It is at **8**.
- **A worktree agent cannot build `save_envelope_roundtrip`** (imgui). Budget for the integrating
  session writing the check.
- **Any tooling fix for worktree agents must be tested FROM a worktree.** Three builder fixes landed
  and the first two verifications ran in the main checkout, where the bug could not appear.
- **Agents' worktree bases are stale by default.** Every one last sprint was; all had to merge main
  before starting.
- **A green check is not evidence that it looked.** Sprint 32 found four instruments measuring
  something other than their subject, and one let a real regression through a wave already called
  verified.
