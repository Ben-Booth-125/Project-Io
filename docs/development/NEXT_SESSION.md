# Next session — Sprint 20 design: corporation spawn forms & the viability pass

Ben's call (2026-08-25, closing the flood-field/BL-625 session): Sprint 20 is a **spawn
viability pass** — gather data on corporation starts *before* the UI-truth work — and the
**spawn forms need a design session first**.

## What the session settles

- The forms a corporation start can take (Ben: "there are a few forms for corporation starts
  to take"). Owner doc on settling: `docs/generation/CORPORATION_GENERATION.md`.
- The viability metric and its harness: measured over the real 80-tick warm start, relative
  across forms on a fixed seed + rival field (absolute numbers are confounded by the open
  economy pathologies — see the "economy tells the truth" theme).

## Evidence in hand (this session's dispatch triage)

- **The default start is insolvent**: Genom Systems opens at Cr −1, net −627/qtr, and
  `dispatch_convoy` refuses a 2-unit haul on funds (wire test, 2026-08-25). Spawn viability
  is not hypothetical.
- Warm-start sweeps are newly affordable: gen + 12 ticks ≈ 20 s Release after the
  flood-field fix. A 100-seed sweep is ~an hour; the world-snapshot cache (harness-truth
  theme) would cut it further.

## Cautions carried from the close-out discussion

1. One deliberate baseline re-bless wave at the end of the pass, with provenance — not a
   dribble (NR-596 precedent).
2. Rivals must be able to bootstrap every spawn form ("never on cash" availability ruling:
   a corp with no stock and no market access may have no legal candidates) — track rival
   trajectories per form, or dead corps poison the seed's data.
3. `--serve`'s 12-tick default understates the app's 80-tick warm start (`main.cpp` comment
   is stale) — align before trusting sweep numbers.
4. Design before cut (the NR-102 drift rule): settle the forms, then open the sprint.
5. Owed regardless of theme: the live-click debt (dispatch form, Throughput lens container
   access — three sprints running) goes in Sprint 20's definition of done. The dispatch
   form's remaining UX fixes (pool-stock pre-check, priced-leg preview in the form) await
   Ben's A/backlog-or-B/build-now call.
