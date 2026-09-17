# Next session — after NR-885, before Digitisation is built

Written 2026-09-17. **Read this, then `docs/generation/DIGITISATION.md`, and nothing else until you
know which mode you are in.** The natural next step is Digitisation. It is designed and it is not
yet ready to build; this note says what stands between the two.

## Where things stand

- **Backlog empty, `REFINED.md` drained.** Ben stopped at wave A on 2026-09-16 and archived waves
  B–D unbuilt: *"just leave it at wave A and we can reinvent anything important later."*
- **`main` is pushed and level with `origin`** (09a1b567). Pushing is allowed; force-pushing is not.
- **One review entry is open: NR-886** (seven calls left by wave A). None blocks anything.
- **The player identity is ruled (NR-885, 2026-09-17).** The player is a **corporation that holds
  a seat**; the mercenary company is retired; the FIELD / ANSWER-TO design test is kept as written.
  `docs/CONCEPT.md` § Player identity holds it, with Ben's statement of the role. The idea of the
  player as the corporation's individual leader, recruiting talent, is an open direction only.

## Digitisation: designed, not ready

**The design is complete.** `DIGITISATION.md` carries the seven properties of the opening map, the
three beats, the region-to-tile downscale, and the readings the phase is judged on. Its input is the
checked `exploration_output` struct (`EXPLORATION.md` § What this phase hands digitisation). Ben on
its aim (2026-09-17): *"it's less important what the player should be doing, and more important what
the world allows."*

**The build plan is gone.** The Digitisation span and its dependants were cancelled unbuilt with
waves B–D on 2026-09-16 and sit cold in `archive/backlog-design-2026-Q3.json`. Starting means cutting a
new, tighter plan — Ben called the last one "highly unstructured". Do not resurrect it wholesale.

**Two inherited weaknesses want a call before Beat 3 is built.**

1. **Displacement held only against a saturated alarm.** With alarm set from measured capability,
   pooled displacement fell 2.73 → 0.50 and no deterrence weight restored it
   (`EXPLORATION.md` § The arms race). Beat 3's proxy war assumes the arms race displaces great-power
   war onto clients — which is also one of the doc's open questions.
2. **Polities rarely meet.** The exploration scorer never reads contact, and stay-home verbs win
   78.6% of the rounds where an unmet target clears the threshold. World war, decolonisation and far
   trade all need a world that has met itself. Either Digitisation inherits that and must produce
   contact itself, or Exploration is revisited first. Ben's call.

**Two build hazards are already known.**

- **Property 7 (far trade) is campaign economy work:** per-market pools change the save format.
  That is Full mode, with a cold review.
- **Property 2 (advanced production) needs a buyer before a seller.** Its demand ladder
  (BL-996, demand ladder) was archived after failing cold review on six counts.

## The suggested first slice

**Measure before mechanism.** Run the span 1660 → 1960 on the existing engine with no new
mechanism, and take the Digitisation readings at 1960 over the seed library.

- It answers the doc's first open question: whether 300 more years on the shared engine is
  affordable at Exploration's region counts.
- It gives every reading a real baseline. Today `digitisation_sim_harness` generates at epoch 0, so
  "advanced chains" reads zero by construction (NR-886, item 7).
- It is small, and it does not flip the default epoch — that stays Digitisation's own done-when
  (NR-869).

After that, admit mechanisms **one property at a time**, starting with property 1 (the corporate
web), because the seat shortlist and every downstream reading consume it.

## Calls in NR-886 that sit on this path

- **Item 3 — solvency no longer gates the seat** (74 of 84 shortlisted seats have negative trailing
  net; the live click never ran). Now that the seat is an identity, this is the next seat question.
- **Item 7 — readings generated at epoch 0**, above.
- Items 1, 2 and 4–6 wait for the Exploration or harness work that next reads them.

## Noticed, not filed

The backlog is deliberately empty, so these are recorded here rather than filed.

- The retired **mercenary contract** still has doc tails: `NATIONS.md` § Trust names it as the
  dimension's rider, and `SELECTION.md` still describes its contract card.
- `mercenary_contract` still appears in `src/world/world_save.hpp`, `binary_io.hpp`,
  `corp_command.hpp` and two UI headers — a save-format remnant, so removing it is a seam change.

## Sixteen worlds to design against

`docs/generation/seed_library.json`, queried with `node tools/session/seed_library.js`. One seed per
question the phase has to answer; a seed is the save. Read readings per seed, never the median.

## Standing hazards

- An agent's **worktree** survives a stop; its **uncommitted work** does not. Make a checkpoint
  commit the first instruction in any long brief.
- Regenerate sweep artefacts and NR readings on the **final integrated tree**, after the last world
  mover merges.
- Mint `NR-` ids against the **hot store and the archive together**.
- Cold review catches what self-report cannot: budget a fix round on any lane that touches the
  economy or the save format.
