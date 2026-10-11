# Sprint 53 handoff — the player is a nation, and the history is written on purpose (2026-10-11)

Read this first in the session that opens sprint 53. Main is `adf266fa`, **v0.1.30** (sprints 50 and 52
cut together; tag pushed). Sprints 50, 51 and 52 are all closed and on main. Nothing is in flight in the
main checkout. The previous handoff (sprints 50/51) is in git history at `cd525884`.

**Mode to open with: Design**, for the open calls below, then **Delivery — Full** (save format, economy,
generation and the AI grants are all touched). Ben's order: **generation first.**

## What is ruled — read the docs, not this note

All of this was settled 2026-10-10/11 and is written into its owning doc. Read the doc before building.

| Subject | Owning doc § | Backlog |
|---|---|---|
| The player is a **major nation** operating its **champion** corporation; design test is the nation's (field / answer to other nations, its population, its corporations); eliminated at no territory; the nation musters and buys equipment on the order book | `CONCEPT.md` § Player identity; `NATIONS.md` § Major and minor nations; `MILITARY.md` § The muster interface | BL-1245 (player is a nation, designed), BL-1284 (player nation verbs) |
| Nation counts are **targets, always hit**: total <= 40, majors 1..6, by a lean then in-world reconciliation (option C); majors and seats one-to-one | `NATION_GENERATION.md` § The nation count is a target; `GENERATION_STRATEGY.md` § Asymmetry is the deliverable | BL-1278 (nation count targets) |
| One specialist per major — the champion; none in a minor; minors keep background firms (NR-1027) | `CORPORATION_GENERATION.md` § Pass 1 (AMENDED FORWARD 2026-10-10) | BL-1283 (one champion per major) |
| Minor nations run shallow | `NATIONS.md` § Major and minor nations | BL-1282 (minor nations shallow) |
| Globe retired; the Life round draws the 2D map; a setup round precedes Life | `STARTUP.md` § The Life map, § Rounds | BL-1280 (Life round 2D map), BL-1279 (wizard setup round) |
| The seat is a **nation selection screen**, majors only, a seven-part card; shaped for six humans, seats one (NR-1025) | `STARTUP.md` § The seat | BL-1281 (nation selection screen), BL-1295 (nation card history) |
| **History patterns**: rise and fall, late culture and world war always hit; civil war and revolution leans; fixed by design; tracked by detectors to pattern tags on the card | `GENERATION_STRATEGY.md` § History patterns; `CIVILISATION.md`, `EXPLORATION.md`, `INDUSTRIALISATION.md`, `MILITARY_HISTORY.md` | BL-1286 (history pattern detectors), BL-1287 (rise and fall), BL-1288 (late culture), BL-1289 (world war), BL-1290 (civil war lean), BL-1294 (revolution lean) |
| AI grants: the player's nation is never scored (a narrowing); rivals act toward a human nation with the same verbs (NR-1026); AI nations fill their research queues | `AI_OPPONENT.md` § 11 | BL-1275 (nation research AI) |

## Suggested order

1. **Measure first — BL-1286 (history pattern detectors).** Build the five detectors and read the 16
   library seeds unforced: how often each pattern already happens. In the same pass, read each seed's
   natural nation count and how many nations hold a specialist — BL-1278's measure-first. Living
   polities run ~40-115 across the library, so the 40 cap reconciles on almost every seed.
2. **Design session** on what the measurement shows: BL-1278's reconciliation mechanics (which span,
   the richest-minor reading, buy-out pricing, the merge pass's min-territory stop); BL-1279's setup
   round contents (Ben: "along with the existing choices at each phase"); BL-1284's verb set; the
   development-lag mechanism BL-1288 needs (none exists); world-war razing and war debt (BL-1289).
3. **Generation build**: BL-1278, BL-1283, BL-1282, then the three always-hit patterns, then the leans.
4. **UI, can run in parallel from the start**: BL-1280 (Life 2D map) is designed and independent.
   BL-1281 waits on BL-1283; BL-1295 waits on BL-1281 and BL-1286. Every UI item needs Ben's live click.
5. **One re-bless per wave, with Ben's go.** World-movers this sprint: BL-1272, BL-1274 (tech tail),
   BL-1283, BL-1278, BL-1289. Each measures its own before/after so the re-bless can name causes.

## The tech tail (sprint 52, carried) — read `HANDOFF_TECH_TREE.md`

Three lanes sit **unmerged** on their worktree branches, each with WIP unbuilt:
`worktree-agent-ab01dac630e84f644` (BL-1272 generation without research, fleet threshold mid-edit),
`worktree-agent-a4b4b115f9c2b3fe1` (BL-1274 tech effects wired, fix round unbuilt),
`worktree-agent-aa32f1452e155d7d3` (BL-1276 tech tree panel, shift-click round unbuilt; live click owed).
Not started: BL-1275 (nation research AI), BL-1285 (quest deed state), BL-1277 (tech wave re-bless).
**Those lanes were cut before the id renumbering**: in their commits, BL-1278 means BL-1285 and
NR-1025/1026/1027 mean NR-1031/1032/1033. Apply that map before merging any of them — on main,
BL-1278 is nation count targets and NR-1025..1028 are this sprint's identity calls.

Also carried from sprint 51: BL-1247 (ground benchmark), BL-1293 (anchor rings clip), BL-1255 (rail rung).

## Open calls for Ben

- **NR-1034** — the rise-and-fall realm stays under the non-hegemony cap; the late culture is the most
  isolated by reach (decisions taken on his behalf).
- **NR-1035** — the sprint 50 merge moved G3 99.8 -> 95.8.
- The rest of the review queue (25 open) — `node tools/session/render_needs_review.js`.

## The gate baseline on main (adf266fa)

`market_viability`, 16 seeds, 400 ticks: **G1 92.9, G1 t50 85.5, G1b 6.0 (over 5), G2 111.7, G3 94.7.**
`world_determinism` PASS; `player_seed_sweep --guard 12` ALL PASS; `history_sim_harness` at its
R3a2/R3a3 two-failure baseline; `nation_tech_harness` PASS. BL-1277 holds the tech merge's movement.

## Traps met getting here

- **Ids collide across parallel sprints, and `next_id.js` misses them.** In one day it missed ids that
  lived only in another branch's cold archive or prose (BL-1291..1293, BL-1278, NR-1011/1020/1025..1027).
  Before minting, grep every mention on main and each open branch:
  `git grep -ohE 'BL-1[0-9]{3}' <ref> -- docs tools src | sort -n | tail`, and the same for `NR-`.
  Main's maximum now: BL-1295, NR-1035.
- **The Debug CTest tier is not a gate**: most world harnesses hit its 60 s timeout. Gate on the Release
  path: `bash tools/verify/build_lua_harness.sh <name>` then `./build_gen/verify/<name>.exe` —
  `world_determinism`, `player_seed_sweep --guard 12`, `history_sim_harness`, `market_viability`.
- `cmd //c "build_app.bat all"` fails to find the script; call `./build_app.bat all` directly.
  `build_app.bat all` stops at the first failed target; nmake `/K` keeps going.
- A fresh worktree has no `build/`: configure with `IO_DEPS_CACHE=C:/Users/benbo/Project-Io/_deps_cache`
  and `-DFETCHCONTENT_FULLY_DISCONNECTED=ON`.
- **The PC slept mid-gate** overnight even under a keep-awake hold (the seed sweep took 17:21 -> 10:21).
  Split long gate runs or check the clock between them.
- Pre-existing broken harnesses, not regressions: `chain_depth` (pools), `nation_scorer_harness` and
  `battle_engagement_harness` (the retired mercenary design) — BL-1231 (broken harnesses).

## Cleanup for Ben

Stale worktrees under `.claude/worktrees/` (about 30, many locked, most from merged sprint 50/51/52
lanes). Keep the three tech-tail lanes above until they merge. `rm -rf` is denied to sessions.
