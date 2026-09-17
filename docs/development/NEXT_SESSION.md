# Next session — sprint 43, the 1960 baseline

Written 2026-09-17, replacing the post-NR-885 note. **Read this, then `node
tools/session/backlog_query.js --status designed --full`, then `docs/generation/DIGITISATION.md`.**

## Where things stand

- **The Digitisation build plan is cut, and it is small on purpose.** Sprint 43 (the 1960
  baseline) is open with four items; sprints 44 (the corporate web's plumbing) and 45
  (industrialisation makes the web real) are proposed goal rows in `sprints.json`, their items cut at
  the gates. The plan obeys the lessons of the archived one, recorded in sprint 43's notes.
- **Ben ruled five calls on 2026-09-17**, written where they belong:
  - file sprint 43 in full, 44-45 as goal rows;
  - re-bless the seed library, and bring replacements for seeds 4, 6, 12 and 13;
  - the charter budget stays off until Beat 1's industry points exist — no stand-in
    (`DIGITISATION.md` § 1);
  - the budget charters the whole web, specialists included (`DIGITISATION.md` § 1,
    `CORPORATION_GENERATION.md` Pass 1 and Pass 6, `GLOSSARY.md`);
  - both inherited weaknesses are measured in sprint 43 and ruled at its gate.
- **Committed alongside:** the Industry tree store opened ring 3 and ring 4 from its ring-1 and
  ring-2 milestones; fixed, and `tree_lint.js` now fails any milestone that does not open ring + 1.

## Sprint 43, in order, one lane

1. BL-1026 (seed library re-read) — exploration_sweep gains `--seeds` and `--out`.
2. BL-1027 (span cost to 1960) — `--through`, per-seed cost of each span half.
3. BL-1028 (weakness counters to 1960) — the only item that may touch `src/world`, as pure counters.
4. BL-1029 (Digitisation readings at 1960) — the thirteen readings beside a 1660 control.

## Facts the old handoff had wrong

- **"Run 1660 → 1960 on the existing engine" is `exploration_stop_year = 1960` at epoch 0.** No
  source change, and it keeps Exploration's `start_year`, so nothing re-anchors. It runs
  Exploration's forces only.
- **Never `epoch_year = 1960` for a baseline.** It selects the superseded 1160 → 1560 → 1960
  two-span arc with Exploration off (`era_minus_one.cpp:25-35, 365-370`). BL-1006 (far-trade
  reading) runs on that arc today.
- **Running to 1960 does not un-zero "advanced chains".** The recipe band follows `epoch_year`, so it
  stays a structural zero until the epoch flip decouples arc from band. Region industrialisation is
  also zero: Stage 4's lag is computed only for a settlement stop ≥ 1700.
- **"Polities rarely meet" rests on unmerged commits** (`eaf15a37`, `6d7bb462` on
  `worktree-agent-a0956d0cb7960ba28`). Main's sweep chose 412 first-crossing campaigns over 16
  seeds, and 0 of 265 in-span pairs hold a treaty. BL-1028 measures it on main.

## Still open

- **NR-886** (seven calls on wave A's work). Item 7 is answered by BL-1029; item 3 (solvency no
  longer gates the seat) sits on sprint 44's path, now that the budget charters specialists.
- **Mercenary tails** — `NATIONS.md` § Trust, `SELECTION.md`'s contract card, and
  `mercenary_contract` in the save headers. Not on this path; a save-format seam change.

## Standing hazards

- Regenerate sweep artefacts and NR readings on the **final integrated tree**; never overwrite a
  tracked sweep JSON from a measuring run — use `--out`.
- Timings in **Release**, serial, on a quiet machine, with the build tree quoted.
- Mint `NR-` ids against the **hot store and the archive together**.
- An agent's **worktree** survives a stop; its uncommitted work does not.
