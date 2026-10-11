# Handoff — sprint 52 (the Digitisation tech tree) → sprint 53 (2026-10-10)

Sprint 52 closed as a success on building the tech tree (Ben). Sprint 53 continues with generation
first, then the tech tail. Integration branch: `claude/tech-trees-game-eras-8036ed`. Mode: **Delivery —
Full**. The sprint row (archived) carries the retro; `REFINED.md` § Sprint 52 has the task list.

## IDS RENUMBERED AT THE MERGE ONTO MAIN (2026-10-10)

Main had already minted (and pushed) these ids while sprint 52 ran, so this branch's copies moved:
**BL-1278 -> BL-1285** (quest deed state), **NR-1025 -> NR-1031**, **NR-1026 -> NR-1032**,
**NR-1027 -> NR-1033**. The lane branches below were cut before the rename: apply the same map to
any of their commits before merging them (on main, BL-1278 is nation count targets and NR-1025..1028
are the sprint 53 identity calls).

## What is on the integration branch

- **Design** — `docs/economy/RESEARCH.md` (the system), `docs/economy/tech/DIGITISATION_TREE.md` +
  `digitisation_tree.json` (160 nodes; `node tools/session/tech_tree_store.js --check`),
  `docs/generation/trees/TREES.md` (generation trees retired; the gate table with Ben's rulings).
- **BL-1273 (nation tech state)** — merged 14e8cace: `src/world/nation_tech.*`, save v47, pacing base
  120 + 20 × √(line spend) at 200 RP a cost unit; `nation_tech_harness` 16 seeds ×2 PASS.
- **Not on the branch:** generation without research, effects, the panel. Nothing in the live game
  queues research yet (that is BL-1275).

## Carried, by branch (each lane's worktree survives at `.claude/worktrees/agent-*`)

| Item | Branch | State | Next step |
|---|---|---|---|
| BL-1272 (generation without research) | `worktree-agent-ab01dac630e84f644` | dc74f929 trees removed (reviewed); a777f4c7 sea legs + coastal ramp (naval points match baseline); **6071ed66 WIP** — fleet-afloat threshold mid-edit, unbuilt | Finish the threshold: measure the curve (threshold → Industrialisation subjections; target near the baseline 21, now 440), set a round default, G1h row; cold-review a777f4c7..tip; merge |
| BL-1274 (tech effects wired) | `worktree-agent-a4b4b115f9c2b3fe1` | b6a612a5 reviewed (61 wired / 127 unread); **69507722 WIP** fix round, unbuilt | Build + run nation_tech_harness; finish T1/T2 edits; confirm Rocketry gates launch, propellant is a +20% yield bonus, institutes staffed, prose-direction audit; merge |
| BL-1276 (tech tree panel) | `worktree-agent-aa32f1452e155d7d3` | 215177de reviewed (seam clean); **693a676c WIP** shift-click round, unbuilt | Build, run `scripts/verify/tech_tree_panel.lua`, look at the PNGs; **Ben's live click** (R2) |
| BL-1285 (quest deed state) | — | not started | after BL-1274 merges |
| BL-1275 (nation research AI) | — | not started | after BL-1274 merges; grant in AI_OPPONENT.md § 11 |
| BL-1277 (tech wave re-bless) | — | not started | last; per-cause shape description; Ben authorises |

## Rulings this sprint (all written in their owning docs)

Research leaves generation; every nation starts equal and holds nothing; the nation holds technology;
all nations playable on the industrial band (overturns NR-929). The 160-node grammar (four tiers,
AND requirement groups, clusters, pairs, quests per branch keying the next age, placeholders for
espionage and politics, the queue; AI nations fill theirs by scorer). Works lean from charter reach
(NR-1032). Pacing: base + √spend, 200 RP a unit, an age ≈ eighty years. Generation gates: sea legs =
date + naval capacity + a fleet afloat; coastal naval ramp re-weighted; dates = acquisition median.
Rocketry gates launch; propellant stays open. Institutes staffed at construction. Deed state in scope.
Panel: shift + left click queues; a queued node toggles off.

## Open for Ben

- **NR-1033** — the seated corporation edits its nation's research queue (bridge until the player is
  a nation).
- **Sprint 53's theme** — it was "the player is a nation" (BL-1245); it now also carries this tail.
  Whether the pivot stays in 53 or moves on is his call at the cut.

## Traps met this sprint

- A fresh worktree has no `build/`: configure it against `C:/Users/benbo/Project-Io/_deps_cache` with
  `FETCHCONTENT_FULLY_DISCONNECTED=ON` (the command is in the lane briefs) — the network fetch is
  blocked.
- Lane reports misnamed their own branch once; check `git branch --contains <hash>` before believing a
  commit landed on the integration branch.
- Every lane needed a fix round after cold review; budget one per item.
