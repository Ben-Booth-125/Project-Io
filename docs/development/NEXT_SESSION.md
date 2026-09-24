# Sprint 47 handoff — one history, told through the rounds

Written 2026-09-24, at sprint 46's early close and the **v0.1.25** cut. Read the memory
`io-sprint-46-core-is-round-six`, then this, then `drafts/sprint-47-narrative-survey.md`.

## The goal (Ben, 2026-09-24)

> Focus on having a **connected narrative through each generation round**.

The wizard is six rounds — System, Life, Culture, Empires, Exploration, Industrialisation — then
Begin and the select-company canvas. Each round now works. What it does not do is read as **one
history unfolding**. A people the player watches spread in Culture is gone 160 years into Empires.
A realm changes colour and name at every seam and is re-announced as "rising" when it is centuries
old. Begin re-coins every nation and city, so the player cannot tell which realm became their
nation. The corporation they choose has no past.

**Proposed done-when for the sprint** (confirm at the cut): a player can follow one people, one
realm and one place from the Culture round to the seat card without a name, a colour or a thread
breaking, and every seam between rounds shows the ground the last round left.

## What sprint 46 leaves you

**Landed and verified** (v0.1.25, pushed): pools per market; trade that reaches for price; the
epoch flip (generation reads no epoch — epoch 0 and 1960 build one world); the superseded arc and
its narrative passes gone; the Industrialisation rename; the live tick 5–9× faster with identical
results.

**Built and merged, owing Ben's LIVE CLICK before they close** (sprint 47 rows, `backlog_query.js
--grep` each):
- BL-1068 (round 6 plays the span) and BL-1080 (round 6 shows industry — heat and Ind column)
- BL-1072 (the wait says what it is doing) and BL-1073 (Begin adopts the wizard's world)
- BL-1076 (select company)

**Rides the re-bless:** BL-1082 (the state hash folds the seat), and BL-1049 (civilisation index
reuse). The re-bless is **authorised in principle** (Ben, 2026-09-24) and is taken ONCE after the
sprint's world-movers land. Digests have moved all of sprint 46 and are NOT re-pinned: current
`world_determinism` is seedA/on `5BA2EE1EE993C201`, seedB/on `5ECE9A097D14DACE`, seedA/off
`4834366D19271E5F`, seedA/on/epoch0 = seedA/on.

**Unscheduled carry** (sprint null, awaiting the cut): BL-1066 (the player cannot build — its
reading is owed on the flipped world; capacity unresolved), BL-1071 (market exports — weak, see
the calls below), BL-1069 (housing), BL-1070 (epoch-0 anachronisms), BL-1077 (village roads cost),
BL-1078 (Begin blocks the window), BL-1046, BL-1062, BL-1063, BL-1065 (the CTest tier). And
`drafts/sprint-46-items.json`: 14 unfiled rows, most of them the play-handoff carry (price field,
tariff posture, sea lanes, depletion, campaign tech, culture lens, nation summary, search seed…).

## The evidence for sprint 47

`docs/development/drafts/sprint-47-narrative-survey.md` — a 91-agent survey of every round: what
it shows, what it inherits and hands on, and **80 adversarially-confirmed breaks** (28 high)
(`drafts/sprint-47-breaks.json`). Its section B (the critic) **overrides** section A where they
conflict — read both. The survey's twelve candidate items and twelve questions are the raw
material for the cut; the ranked breaks, in brief:

1. **A reroll silently forks the history** — one `era_seed` feeds Empires, Exploration and
   Industrialisation, so rerolling round 5 changes the history round 4 still shows; a Culture
   reroll changes nothing visible but forks every age after it.
2. **Realms lose their identity at every seam** — the palette carry is dead code, names reset to
   the lowest-index region, and every survivor is re-announced "A realm rises".
3. **Begin re-mints the world** — nation names, colours and city names are all new.
4. **The culture thread dies ~160 years into Empires**; the culture base STARTUP.md promises under
   the polity fill does not exist.
5. **Round 6 does not close on the map you play** — firms, markets and prices are made after Begin.
6. **Every seam is a blank cut**, and round 6's captions narrate the whole rebuild.
7. **No bridge from life to people** — the globe cuts to an unnamed 2400 BCE map.

## How to run it (fable, ultracode)

1. **Session start.** `git fetch origin`; main should equal origin (v0.1.25). Check `git status`
   (shared checkout). `tasklist | grep -i ProjectIo` for orphans.
2. **First, a design pass with Ben — a FORM, not code** (memory `ben-prefers-widget-qa-popup`).
   Ask the survey's § A.5 questions, amended by § B: realm naming (Q1) and nation naming (Q2)
   together, since nations are already coined in the culture's tongue; what a reroll re-rolls
   (Q3); how peoples show after round 3 (Q4); the wait between rounds (Q5); the Life→people
   bridge (Q6 — § B: this revives part of the retired Inheritance round, a design call); per-age
   closes (Q7); the landscape search inside round 6 (Q8 — re-measure the Begin freeze first,
   the ~20 s figure is stale); the seat card's origin line (Q9); the Drawdown lean (Q10); the
   Ages view in play (Q11); **presentation-only or sim work too** (Q12 — furnace crossings never
   fire on a generated world because settlement always takes the antiquity branch; that is sim
   work). Add the critic's missing thread: **the campaign's first frame** (body name, 1960, a line
   looking back, a way into the Ages view). Write each answer into its owning doc as it is given
   (STARTUP.md, CIVILISATION.md, NATION_GENERATION.md, COLONISATION.md, PLANETOLOGY.md).
3. **Also put to Ben** (carried calls from sprint 46, not narrative): should the Logistic Point
   cap brake market exports this hard (BL-1071 moves ~46 of ~3,300 stranded steel in 20 ticks);
   a market export moves no money (the haul is the margin given up) — confirm; the epoch still
   picks the recipe band (BL-1047 kept it, as `industrial_band_from_year`) — confirm; tariffs from
   history now read zero (their only formula went with the old arc) — who writes them now.
4. **Cut the sprint.** Mint ids with `node tools/session/next_id.js`; file each item with its
   owning doc, requirement group and a done-when **a live click can check**; raise `novel-work`
   entries for new event kinds (a `cradle` event) and new leans (§ B asks for this). Suggested
   order from the survey: **identity first** (reroll, colour, name, roads survive the seam), then
   **the culture thread**, then **each age's close**, then **the hand-over into play** (no blank
   seams, realm becomes nation, the map you will play). Every later item assumes a realm keeps
   its id, name and colour across a seam.
5. **Build with workflows, one per item or per lane**: implement in a worktree → cold
   `code-reviewer` → main-session merge, Release build, `world_determinism` twice, save round
   trips, `history_lapse_press.lua` → Ben's live click. Keep lanes disjoint by file:
   `src/ui/startup_screens.cpp` + `history_lapse.cpp` (UI) vs `history_sim.cpp` / `settlement.cpp`
   / `nation_generation.cpp` (generation).
6. **Close the owed live clicks early** (step 2's form can carry them), so sprint 46's built UI
   closes before sprint 47 changes the same surfaces.
7. **The re-bless** once the sprint's world-movers have landed (naming at Begin, per-span seeds
   and any sim work move digests; the rest is presentation). BL-1082 lands in it.

## Hazards learned in sprint 46

- **Worktree agents edit the main session's scratchpad** (twice rewrote `build_rel.bat` to their
  own worktree). Keep main-session scripts as `main_session_*.bat` with a do-not-edit header and
  check the `cd` line; confirm a new source file appears in `build_rel/build.ninja` after a merge.
  (memory `io-agents-share-the-session-scratchpad`)
- **`cmd.exe` is blocked inside some agent worktrees**; agents fell back to bash ports of the build
  scripts. Brief them that this is expected.
- **The CTest tier is not a working gate** (BL-1065). Gate on the Release build, `world_determinism`
  (twice, bit-identical), `save_roundtrip`, `save_envelope_roundtrip` (CMake target in `build_rel`),
  and each item's harnesses.
- **Save versions**: `node tools/session/next_save_version.js` now reports both the world version
  (25) and the envelope version (21); claim with `--kind world|envelope --claim "<item>"`.
- **Cold review finds what harnesses cannot** — every economy lane in sprint 46 had real defects a
  green harness passed. Budget a fix round per lane.
- **`--verify` reuses the harness's world**, so the wizard's full-build path, the loading wait and
  Begin's adoption are only exercised by the real app — a live click, or `begin_adopts_check.js` /
  `seat_pick_check.js`.
- A number and the world it was read on are one fact: every sprint 46 economy reading was on the
  epoch-0 verify world, which after the flip is the same world as 1960 — but BL-1066's done-when
  is still owed on it.
