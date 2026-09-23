# Sprint 46 handoff — the generation reaches the game

Written 2026-09-23, at sprint 45's close. **Read the memory `io-sprint-45-open` first, then this.**
Sprint 45 is closed and archived: seventeen items delivered, one re-bless spent, the retro in
`sprints.json`. The hot backlog now holds **seven open items and nothing else**, every one sprint 46.

## The goal, unchanged from Ben's sprint 46 form (NR-898)

The wizard shows the whole arc and **the game begins on the world it built**: round 6 plays the
Digitisation span, Begin adopts the wizard's world, every row of `DIGITISATION.md` § What crosses
into play carries into the campaign or is stated deferred, the superseded 1560 → 1960 arc and its
narrative passes are retired, the default epoch flips to 1960, and the select-corporation screen
closes the sprint. Sprint 45 built the world; sprint 46 wires it in.

## Start here, in this order

### 1. BL-1066 (the player cannot build) — priority S

Do this before the cut. On the shipped world the player places a military base and **it never
completes**: `tech_locked` at tick 0, placeable after 12 econ ticks, still unfinished 100 ticks
later while the corp's balance climbs past 70,000 credits. Not cash, not the gate, not the harness's
40-tick bound — construction itself does not proceed. The app lays the same web as `--verify` does
(NR-909), so this is the world a player is handed: **the core loop's first move may not land**, and
a sprint about wiring the generation into the game cannot open on top of it. It also blocks eleven
of the twelve failing verify scripts (`scripts/verify/lib.lua`'s shared fixture). The item names the
two candidates — construction capacity on the body, or the materials the build draws — and the
probes are in the 2026-09-23 session scratchpad. Run them with
`build_rel/ProjectIo.exe --verify <probe>.lua`: the RELEASE app runs the visual suite about four
times faster than Debug.

### 2. NR-915 — the only open queue entry, and Ben's call

The live tick at the pinned 650:2 is a median **x0.78** of the same seed's legacy world, inside
NR-908's band, but the tail is heavy: seeds 41, 31 and 28 at **x5.79, x4.12, x3.13** (30.9, 25.5 and
12.8 s a live tick). The cost follows the SPECIALISTS a world seats, not its firms. The entry
recommends reading the phase split (the cost run prints it) and then bounding the seat side, rather
than moving the divisor the re-bless just pinned. It bears on 46 directly: a 31-second tick is the
player's experience of the world being wired in.

### 3. The cut

`docs/development/drafts/sprint-46-items.json` — 21 rows, including `DENSITY_FOLLOWS_CITIES`
(NR-913). Its own `_note` is the procedure: mint ids (`next_id.js`), file each row with sprint "46",
write a requirement group per item, amend BL-1047, raise the campaign-tech scope flag as a
novel-work NR, then DELETE the draft. **Every file:line in it predates BL-1042, BL-1044 and BL-1050
and must be re-checked as each item is filed.** The seven already-filed items below are part of the
sprint and should not be re-filed.

## The seven open items

| Item | Pri | What it is |
|---|---|---|
| BL-1066 PLAYER_CANNOT_BUILD | S | the player's construction does not complete on the shipped world |
| BL-1047 EPOCH_FLIP_GATES | A | decouple the four >= 1700 gates and the settlement stop year, so the epoch can flip |
| BL-1065 CTEST_TIER_RELINKED | A | nine targets do not link their Lua TUs; three no longer compile; 48 rows time out |
| BL-1046 CAPTURED_TREASURY_STRANDED | B | a captured capital's chest is stranded — and treasury is what seeds the campaign |
| BL-1049 CIVILISATION_INDEX_REUSE_AT_1200 | B | the 1200 resume restarts the civilisation and creed tables; a digest mover, so it rides 46's re-bless |
| BL-1062 ORDER_DEPENDENCE_LINT | C | the lint that guards the byte-identical-replay rule BL-1050 fixed by hand |
| BL-1063 WORKER_THROW_MESSAGE_LOST | C | the replay tripwire's message is lost when it throws inside a search worker |

Sprint 45's other nine strays were **cancelled** on 2026-09-23 as generation-internal polish
(BL-1035, BL-1045, BL-1048, BL-1052, BL-1054, BL-1055, BL-1057, BL-1058, BL-1061). Each carries its
reason and all are restorable: `archive_landed.js --restore`.

## What sprint 45 leaves standing, and what it does not

**Standing.** The span and the corridor tier run by default; the charter pins are 650:2 (province
cap 2, sqrt base 8); the no-specialist world falls back before anything is chartered; the one-player
invariant is printed and counted; `--verify`, `--serve` and headless spend the budget on the seed
candidate; `player_seed_sweep --arc shipped|legacy` keeps both worlds checkable, with the shipped
pins re-checked 16/16 and the legacy arc's re-pin 16/16.

**Not standing.** *Density follows cities* is NOT met — rho(firms, urban) 0.139 against
rho(firms, goods) 0.167, where the legacy web read 0.431 / 0.460 — and ships that way by ruling
(NR-913), with the reading restated as taken-and-reported and the question promoted to its own
sprint 46 item. The 8 golden diffs from the visual suite (click_injection, corp_dashboard,
icon_silhouettes, sell_order, trades_tab) are the world moving under demoted goldens and were
deliberately NOT blessed.

## Hazards worth carrying into 46

- **Check for orphans before starting a suite**: three `--verify-all` processes from stopped chains
  ran together for ten hours on 2026-09-22 and made both the ctest and visual readings worthless.
  `tasklist | grep -i ProjectIo` first.
- **The machine changed**: 31.2 GB (was 15.5) and 16 cores, so "one sweep at a time" no longer binds
  — but a TIMING row still needs the machine to itself.
- **A number and the world it was read on are one fact.** 580 was the nine-seat divisor on a
  tier-off world; the tier moved every stockpile and the same rule read 650 on the shipped one.
  Sprint 46 flips the epoch, which moves worlds again: re-read every calibration it inherits.
- `keepawake.ps1 -WhileFile <lock>` holds across a chain; the process form releases in the first gap.
- `build_app.bat` eats stdin — a `while read` loop over target names builds only the first.
- This checkout is shared with other sessions (one cut v0.1.24 mid-flight on 2026-09-22): check
  `git status` before committing, and commit in increments.
