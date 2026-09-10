# Next session — sprint 38, reframed onto the arc

Written 2026-09-10, replacing the earlier note of the same day. **That note's central diagnosis
was wrong and this one says why**, because the mistake is worth not repeating.

## What changed

The previous handover said sprint 38 had one holdout, BL-868 (creeds raise armies), blocked by a
`two_polity_world` fixture that BL-837 and BL-872 had broken. Ben reframed the close around what
he actually wants out of the phase — *volatility, conflict, and asymmetric polities* — and the
sweep was run to see where each of those stands.

**`history_sweep`, 16 seeds, main at `c83a0d74`:**

| | Median | Range | |
|---|---|---|---|
| Battles per world | 412 | 4 – 1000 | healthy |
| Conquests per world | 386 | 4 – 740 | healthy |
| Worlds with zero conquest | 0 / 16 | | healthy |
| Largest polity's share | **3.3%** | 2.1% – 5.4% | flat |
| Polities eliminated | **0** | 0 in every world | flat |
| Rise / peak / fall worlds | **0 / 16** | | flat |

With 31–61 powers per world an even split is 1.6–3.2%. **Conflict is fixed; asymmetry and
volatility are not.** 386 conquests per world and the political map ends the shape it started.

**BL-868's fixture was never broken.** It was reporting the truth — conquest is capped
*everywhere*, not just there — and seven passes read a correct null result as a fixture bug. The
lesson: check the world-level instrument before rebuilding a synthetic fixture around a null.

## Ben's rulings, 2026-09-10

- **The arc.** *"Really I want to see polities be eliminated and empires to form, before
  collapsing back into those smaller polities - with some surviving as larger kingdoms."* Written
  into `GENERATION_STRATEGY.md` § The asymmetry is POLITICAL as well as economic and
  `CIVILISATION.md` § The arc the phase must produce.
- **A peaceable world is legitimate.** The claim is distributional; a seed that refuses war is not
  a failure case. This settles the BL-861 / BL-854 standoff — `B384a` passes and did not need
  retiring.
- **The wall moves when you win** (NR-823). Reach still GATES rather than prices, so geography
  cannot be *bought* past — but the gate is not fixed, so winning extends reach outward and
  geography must be *built* past. Softening the gate to a price was declined; centre chains
  (BL-887) stay deferred.
- **Random worlds, wizard only.** The wizard opens on a rolled seed. Entropy stops at the UI, so
  `world/*` stays pure and the determinism rule is untouched — no grant needed.
- **BL-861 cancelled** as superseded (NR-824).

## The work now

| Item | |
|---|---|
| `BL-889` (conquest must compound) | The sprint's real subject. **May NOT be delivered by lowering `sustainable_campaign_floor_q`** — that is the softening Ben declined, and floors of 80 and 20 already measured identical. |
| `BL-892` (reach_mod inert on supply) | Priority A. `W7b` fails: a pre-built reach work does not change the supply path. This *is* the widening mechanism the ruling rests on. Start here. |
| `BL-890` (wizard rolls seed) | Difficulty 1. `startup_screens.cpp:333` already has Roll; only the default is missing. |
| `BL-891` (round 4 structure readout) | Check BL-817 / BL-830 coverage first — may be a read on those, not a new panel. |
| `BL-868` (creeds raise armies) | Blocked on BL-889. Wiring is sound and sits rebased in `.claude/worktrees/agent-aec88315766979641` (`ea2b1111`). Verify distributionally on the sweep, never on a two-polity fixture. |

## Evidence to start from

- `history_sim_harness` fails `R3a2` / `R3a3` on main: `near: 6 battles / 1 conquests | far: 0
  battles / 0 conquests` — in a case that *deliberately* sets `neighbour_radius=40` and
  `w_dist=0` so only supply decay can stop the far target. Zero far campaigns. That is the gate
  vetoing distance before supply is priced.
- `history_sim.hpp` § `sustainable_campaign_floor_q` carries the prior investigation: floors of 80
  and 20 gave identical elimination counts, ruling out a calibration fix.
- **A gate-off sweep was started this session and had not finished when it closed.** Re-run
  `./build_gen/verify/history_sweep.exe 16 --set sustainable_campaign_floor_q=0` and compare
  against the gate-on rows (saved in the BL-889 record). It answers whether the gate is the
  *cause* of the flatness or only the leading suspect.
- `history_sweep`'s `--set` table now carries the reach dials
  (`sustainable_campaign_floor_q`, `sustainable_garrison_floor_q`, `terrain_reach_cost_q`,
  `road_tier1_uses`, `road_tier2_uses`) — added this session, because the field's own comment
  says to tune it with the sweep and the sweep could not reach it.
