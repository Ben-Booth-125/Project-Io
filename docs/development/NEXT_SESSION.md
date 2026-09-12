# Next session — build sprint 40, Exploration

Written 2026-09-11 at the close of the Exploration design session, for the session that builds it.
**Mode: Delivery — Full** (the economy seam, the save format, >2 logic files, determinism risk).
Read `docs/development/DELIVERY.md` before starting.

Design is **settled and written down**. Do not redesign it. If something here looks wrong, say so
and ask Ben — do not quietly pick a different answer.

## Read these, in this order, and nothing else

1. `docs/generation/EXPLORATION.md` — the phase. The whole authority. ~475 lines.
2. `docs/generation/trees/EXPLORATION_TREE.md` — its technology tree, already authored.
3. `docs/development/sprints.json` sprint 40 — goal, risk, done-when, sequence.
4. `node tools/session/backlog_query.js --grep BL-9xx --full` — one item at a time, as you take it.

`CIVILISATION.md` § The closure of the Empire era is the input contract — read **only** that
section when you need to know what crosses at 1200 CE. The corpus is ~650K tokens; do not sweep it.

## What this phase is, in five lines

Exploration runs **1200 → 1660 CE**, on the shared `history_sim` engine, taking `pass_one_output`
as its only input. Where Empires asks *who holds this ground*, it asks *who wants what someone else
holds, and what they will do about it*. Its one structural claim is that **conflict MOVES** —
neighbour-war rate falls *relative to* frontier-skirmish rate. It is the phase where material
becomes capital. It hands digitisation the treasuries, the scarcity signals and the preferences
that a saturated corporate web will form around.

## The sequence, and why it is this order

**BL-937 (the ten readings) LANDS FIRST.** Not last. The sprint's named risk is the *frozen map*:
every mechanism here damps conflict near home, and a phase that damps it everywhere looks identical
to one that displaces it until somebody measures the ratio. BL-905/BL-906 already proved on the
Empire span that instrumentation is what turns a guessed constant into a measured one.

Then, in order:

| Wave | Items | Why together |
|---|---|---|
| 0 | **BL-937** | Nothing else is judgeable without it |
| 1 | **BL-931** (the span runs), **BL-930** (wire the tree) | The engine and the actor's technology |
| 2 | **BL-932** (treasury), **BL-939** (scarcity signal), **BL-940** (corridor throughput) | The three quantities everything else reads |
| 3 | **BL-933** (treaties), **BL-934** (subjects), **BL-935** (ports/navies/armies) | The mechanisms that spend and bind |
| 4 | **BL-941** (deterrence), **BL-942** (two strategies), **BL-936** (preference) | The phase's claims, which need wave 3 to exist |
| 5 | **BL-943** (fleet/caravan exemplars), **BL-938** (Industry ring-1 migration) | Presentation, and the tree cleanup |

`BL-944` (the schism verb) is **gated on a Ben decision** — see below. `BL-945` (depletion) is
parked for digitisation and is not sprint 40 work.

## Four rulings you must not re-litigate

1. **The treasury sits at the CAPITAL SEAT and is capturable** — one per polity, consolidated from
   the seats once at 1200 as the phase's visible opening act. Ben moved it there deliberately so
   that sacking a capital takes something real. It is *not* a free-floating per-polity scalar.
2. **There is a SCARCITY SIGNAL, never a price.** One integer per (good, market), no clearing, no
   order book. Digitisation resolves prices. A price here builds the economy layer two phases early.
3. **Goods move as ONE NUMBER PER CORRIDOR. No cargo object ever exists.** Per-good routing is cut
   because the Empire pass already pays a full Dijkstra per polity per round against a region count
   that grows inside the run. `region::network_supply_q` is already throughput in all but name.
4. **Both consolidation and expansion must pay**, and which a realm reaches for is *derived* from
   its creed (`zeal`, `dominion`, `sea_legs_q`) — never a flag, never a term inside the scorer.

## Three traps this session already fell into — do not repeat them

**The Industry tree's obvious shrink is wrong.** Its ring 4 is the twentieth century
(electrification, oil, flight, broadcast, antibiotics) and the campaign epoch is 1960. Its **ring 1
is the exploration age** and duplicates the newly-authored Exploration tree. `BL-938` is a
*migration*, not a trim. Read the item before touching either tree.

**`polity::parent` is NOT the overlord link.** `parent` records lineage — who broke away from whom.
Subjection is a live relation that begins, ends and transfers. `BL-934` adds its own field.

**There is NO unclaimed ground.** The migration fills every habitable landmass, so a far continent
at 1200 CE is **settled and unmet**. What is discovered is *people*. Sprint 40's original premise
said otherwise and has been corrected; if you find that premise anywhere else, it is stale.

## One decision owed from Ben before BL-944

The universalising creed landed 2026-09-11 with the residue a schism cuts along. But **reassertion
fired zero times in 16 worlds** — held ground under an adopting realm sits above the binding floor,
because BL-896's secession floor had already removed the badly-reached ground. A schism verb built
on that substrate would also fire zero times. `universal_creed_convert_supply_q` and
`universal_creed_alien_penalty_q` are the dials; the honest reading is that it needs a higher floor
or a second binding term. **Ask Ben; do not pick one.**

## Working rules for this sprint

- **Sub-agents run in separate worktrees** when they write code. Brief each on a tight block with
  the one or two docs it needs. Tell it explicitly: `cmd //c` is refused inside a worktree, use
  `bash tools/verify/build_lua_harness.sh`; block on your own long waits, do not yield mid-wait;
  stop once you have a decision. The main session merges, builds and verifies — **assume nothing
  from a self-report** (four of five capable lanes failed independent review last time).
- **`git fetch origin` and integrate before minting any id.** `next_id.js` can hand out a taken id
  on a stale branch; its SCAN INCOMPLETE line is the tell.
- **Determinism is non-negotiable.** Every new quantity is a deterministic consequence of upstream
  scalars. No dice, no roll, no wall-clock.
- **Any change to a tree store re-runs `node tools/session/tree_lint.js`.** All four pass today.
- **`header_graph.js` FAILs on main** at 80 dangling — compare the *count*, never the verdict.
- **A UI requirement needs a live click** (BL-943), not just a capture.
- Log anything wanting Ben's judgement into `NEEDS_REVIEW.json` *as it arises*, not at the close.
  A CALL only Ben can make → the queue. WORK somebody must do → `backlog.json`.

## State

Branch `claude/exploration-era-design-e0f6d7`, two commits: `2fff2443` (the phase gets a doc) and
`b24dc243` (rename, deepening, the tree). Working tree otherwise clean. **Nothing in `src/` has
changed** — this sprint starts from a green build.

All four tree lints pass. `backlog_lint` is 0 fails, 1 pre-existing warning (BL-746 absent).
