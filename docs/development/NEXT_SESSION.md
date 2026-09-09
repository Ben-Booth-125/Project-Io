# Next session — decompose the Empires design into delivery

Written 2026-09-09 at the close of the Empires design pass. **The design is concluded and written
into its authority docs. Your job is to decompose it into delivery items and build them.**

**Read `docs/generation/CIVILISATION.md` first and in full.** It owns the Empires phase and it is
the only place the design lives. This note does not restate it — it tells you what is settled, what
order the work falls in, and what will bite you.

---

## The one thing that still blocks everything

**The roads pass crashes the full generation pipeline (Ben, 2026-09-09).** `--autostart-windowed`
walks the wizard, presses Begin, and never reaches in-game — the frames-rendered line is absent.
Until that is fixed there is no playable world at the end of the wizard, however good the rounds
look. **This is unchanged by the design pass and is still first.**

**Do not read `exit=0` as success on that command.** It exits cleanly via a window close; the proof
of success is the line `[autostart-windowed] OK  N in-game frames rendered`. Grep for it.

---

## What was settled, and where it lives

Six calls by elicitation on 2026-09-09, plus three taken on Ben's behalf. **Every one is written
into an authority doc; none of it is in this file.**

| Settled | Owner |
|---|---|
| The span, and the whole calendar arithmetic | `generation/CIVILISATION.md` § The span is 400 BCE to 1200 CE |
| A settlement is a seat flag on a region | § The unit is the city state |
| Stores sit at the seat and fall with it | § Materials are spent when something happens |
| Reach **gates** a campaign; centres come from supply and governance | § The road is the empire's skeleton |
| Opposition from pantheon temperament + origin farm class | § Culture relations |
| A civilisation is a named record with an ethic | § A civilisation is what mixing makes |
| What the phase hands pass 2 | § What this phase hands the industrial era |

**The calendar, because four documents take their figures from it:**

| | from | to | years |
|---|---|---|---|
| Culture — the migration | 2400 BCE | 400 BCE | 2,000 |
| Empires — this phase | 400 BCE | 1200 CE | 1,600 |
| Pass 1, total | 2400 BCE | 1200 CE | **3,600** |

1200 CE is unmoved, so the coast to 1560, pass 2 and the 1960 epoch are untouched.

---

## The items, in the order they unblock each other

All are `designed`, priority A, sprint 38. **This ordering is the main thing this handover exists
to give you** — it is not in the backlog, because `requires` records only hard dependencies.

**First, and cheap.**
- **BL-873 (culture coining year)** — difficulty 2, no dependencies, blocks BL-870. One integer on
  `culture`, in exactly the shape BL-865 used for `parent`. Do it first because it is small and
  because the kinship measure cannot be written without it.
- **BL-871 (empire span 400 BCE to 1200)** — the span split. Foundational: every measurement below
  is taken against a span, so doing this late invalidates readings taken before it.

**Then the two everything else hangs off.**
- **BL-866 (settlements are sparse)** — the seat. Blocks BL-867 and BL-872.
- **BL-837 (ancient logistics and roads)** — reach as a **gate**. The phase's spine, not a
  modifier. Blocks BL-872 and reshapes BL-823.

**Then.**
- **BL-867 (materials spent on action)** — needs the seat to exist.
- **BL-872 (centres from supply and governance)** — needs *both* BL-866 and BL-837.
- **BL-870 (culture relations)** — needs BL-873.

**Then.**
- **BL-869 (civilisations from mixing)** — needs BL-870, because the opposition bar gates whether a
  civilisation forms at all.
- **BL-868 (creeds raise armies)** — largely independent; its input already exists.

**Last.**
- **BL-823 (anti-hegemon levers)** — re-read after BL-837 lands. Reach-gating is now the primary
  lever, so several of the original six may be redundant. Drop what only duplicates it.
- **BL-861 (no conquest in the span)** — a *measurement*, and it needs the span split first. Its own
  text is emphatic: measure the cause, do not reason it. Settle it together with BL-854.

---

## Findings worth carrying, all of them earned the hard way

**A justification can rot faster than the ruling it justifies.** The first cut of the span section
argued the boundary was cheap because 0 CE is where the sim's ancient arc already ends. True when
written. Ben moved the boundary to 400 BCE hours later, and the sentence became false while the
ruling it supported stayed right. **The split is real work: nothing in `hard_coded_world.cpp` or
`history_sim.cpp` stops at 400 BCE.**

**A same-day ruling orphans its siblings, and it happened twice in this pass.**
`GENERATION_STRATEGY.md` line 34 restated "4000 BCE → 0 CE" *while deferring to the section that
said otherwise*, and `MANUAL.md` said it in two more places. All are fixed. **Grep the OLD wording
after any ruling, not the new one** — the stale copies are exactly the ones your new-wording search
will not find.

**`--touches <doc>` cannot answer "is this built?".** `backlog_query.js:113` filters on `files`
only, and all 34 open items put their doc in `authority_doc` with no `docs/` path in `files` — so it
returns "nothing matched" for a doc that eight items name. A fix was spawned as a background task on
2026-09-09; **check whether it landed before trusting a negative result.** Until then, grep
`authority_doc` in `backlog.json` directly.

**Six traps from sprint 37, all the same shape — a check that looks like coverage and is not.**
- **Never pipe a harness into `grep`.** A pipeline returns the *filter's* status, and a segfault
  read as `exit 0`. Run to a file, then read `$?`.
- **`--verify` adopts the harness's own world** on the lapse rounds, so the wizard's *stopped*
  generation path has no coverage at all. That shipped an empty map for 4,000 years while the
  header reported 611 foundings.
- **`--debug` on `build_harness.js` does not set `_ITERATOR_DEBUG_LEVEL`.** An out-of-bounds write
  passed both harness builds and aborted only in the app's real CMake Debug build, exit 3, no
  message.
- **A `static_assert` can be far too loose to fire** — `sizeof(app)`'s bar is ~6× the real limit
  while the process died at startup.
- **A synthetic case can assert a tie.** C3a's two cradles were equidistant under the wrapped metric.
- **A classifier can measure the wrong thing entirely** — "how much ground" measured crowding, not
  encirclement.

**What found the real defects was driving the app.** Three separate times, behind green harnesses.
**And a comment can hold an invariant nothing checks** — *"sized to the polity table, which the sim
never grows"* was true when written and false the moment a polity could be born mid-run.

---

## Numbers to quote rather than re-derive

All `build_gen` (/O2). **Debug timings are not comparable; quote the tree.**

| | start of sprint 37 | end |
|---|---|---|
| Regions at the epoch | 609 | **1,906** |
| Ownership changes | 533 | **4,432** |
| Distinct peoples holding ground | 12 | **38 / 56 / 33** |
| Route divergence from a Voronoi | — | **23–33%** |
| Conquests in the span | 0 | ~472 transfers |
| Culture descent depth | — | **9 and 10**, over 676 and 929 cultures |

**Regions are ~3× where sprint 37 started, and that is the live cost risk.** NR-809 measured reach
as roughly quadratic in region count; **BL-844 is already spent**, so no optimisation is waiting —
the cap is the **adjacency model** (BL-855), a sprint of its own. Pass 1 is now 400 years shorter,
which is a small relief and **not** a solution.

---

## Open calls, none of them work

| Entry | Question |
|---|---|
| **NR-807** | Calendar half settled; the **identity** half is open. A mercenary company arriving at an industrial 1960 epoch is the tension. A product call, not a date. |
| **NR-809** | Region count vs the adjacency model. Sharper now that regions tripled. |
| **NR-811** | Round pays a world build and Begin pays another. |
| **NR-812** | Does the wizard's `ACTIONS.json` exemption reach a Run button? Run is retired, so likely moot. |
| **NR-813** | A scrubber for the time-lapse. Defer until the spans are actually split (BL-871). |
| **NR-815 / 816 / 817** | Three culture-relations calls **taken on Ben's behalf** — symmetric opposition, kinship in years, the civilisation formation bar. Live until he overturns them; build against them. |

**Resolved this pass:** NR-814 (the four culture-relations calls), NR-818 (the coast, confirmed).

**Still unanswered inside the design**, and listed in `CIVILISATION.md` § Open questions: what an
ethic is as data; whether governance reach and supply reach are one quantity or two; where the
opposition bar sits (**a measurement, not a judgement**); whether a seat can be founded mid-span;
and how the 1,600-year span interacts with a capacity ladder calibrated over 4,000.

**BL-859 (most land habitable)** is worth pairing with those: its premise was overturned by the
unfarmed-class census — the shortfall is polar ice and mountains, so the lever is planetology or a
cold-affinity package, **not** the affinity floor.

---

## Reproduce

```
node tools/verify/build_harness.js colonisation_harness && build_gen/verify/colonisation_harness.exe 3 4000
node tools/verify/build_harness.js world_determinism    && build_gen/verify/world_determinism.exe
build/ProjectIo.exe --verify scripts/verify/history_lapse_press.lua
build/ProjectIo.exe --autostart-windowed     # grep for "in-game frames rendered"
```

**Baselines:** `colonisation_harness` 35/0. `world_determinism` 0 failures.
`history_lapse_press` **has 4 failures** — the round renumbering moved the pass-round footer, and
the coordinates must be **read off a capture, never guessed**. It fails loudly rather than silently
only because `verify.wizard_round()` reads the round back.

**The `4000` argument above is the OLD span.** Once BL-871 lands, the colonisation harness's span
argument and its 35/0 baseline both need re-establishing against 2,000 years — and a changed
baseline is stated deliberately, never absorbed.
