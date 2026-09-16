# Next session — after sprint 42, before Digitisation

Written 2026-09-16 at the close of sprint 42 (generation sharpened before Digitisation).
**Read this, then `docs/development/NEEDS_REVIEW.md` for NR-877..881, and nothing else until you
know which mode you are in.**

## The one thing that blocks everything

**NR-877 — the wave-1 re-bless is not authorised.** Five items moved the world and the exploration
regression pin (`exploration_sim_harness` R3b) is deliberately red with that entry as its stated
cause. Until Ben answers, nothing re-pins and no golden is blessed. The entry carries the shape, the
five named causes and the four digests; it is written to be read without any of this session's
context.

## Where sprint 42 left the generation layer

Twenty-five items across two waves. The layer now has what it did not have on 2026-09-15:

- **Instruments that can see their subject.** Every downstream harness measures the landscape the
  search actually chose; `demand_census` reports the fraction of markets in band; both sweeps are
  checked in with per-seed tables; the terrain's economic output is asserted, not printed.
- **Handoffs that are checked.** Both `pass_one_output` and `exploration_output` are validated on the
  shipped path and carry the culture table; consumers read the structs, not the live sim.
- **A cheap tuning loop.** The harness builder compiles the world set once (22 s cold, 5 s to link);
  the tile pipeline re-runs its Life half over a cached record in 13% of a full pass.
- **A campaign that inherits its history.** Nations open with the money their 1660 polities banked,
  so garrisons differentiate; every seat's stores consolidate at 1200; the settle is phase 6's own
  validation run rather than a warm start.

## The three findings that should shape what comes next

1. **Measurement parity is broken (`BL-1007`, priority A).** The sweeps generate with
   `world_gen_config`'s struct defaults; the app loads the Lua config. Seed 0 is two different worlds
   (6,479 battles against 9,928). **Do this before tuning anything else** — every figure in the
   sprint's records is internally consistent and describes a world the player never gets.
2. **The Exploration actor has no purse term (`NR-878`).** With the saturation caps replaced by a
   real per-head bill, the median polity spends itself to a treasury of 211. Ben's call: accept the
   shape, or add a solvency rule to a step's eligibility.
3. **The first crossing almost never happens (`NR-880`).** Four of six held seeds meet one polity in
   460 years, so there is no frontier to displace onto — and alarm saturates on every near pair, so
   the constant tuned in sprint 41 is not discriminating. The implicated constants are the campaign
   threshold and the prize pricing for an unmet target.

## Sixteen worlds to design against

`docs/generation/seed_library.json`, queried with `node tools/session/seed_library.js`. Chosen off a
48-seed parity sweep on 2026-09-16, one per question the next phase has to answer: the rich world
(seed 46, a 27M median chest) and the quiet one (seed 17, fourteen polities and thirteen flows); the
crowded (11) against the thin and rich (43); two colonial worlds (13, 41) against two that subjected
nobody (37, 4); infrastructure without trade (32) against trade without roads (10). Seed 0 is in it
because everything else is compared against it. A seed is the save — generation is a pure function of
the descriptor — so the store holds the rationale and a fingerprint, not a snapshot.

## Digitisation, when it starts

`docs/generation/DIGITISATION.md` is still the placeholder and its boundary is unchanged: companies,
prices and tariffs are its subjects. Two things it now inherits that it did not before:

- The tariff derivation is **retired on the single-span arc** and the enactment seam waits for
  Digitisation to write `protection_q` from scarcity, flows and preference (`BL-976`).
- The default epoch moves to 1960 **as Digitisation's own done-when** (Ben, NR-869), never before.

Its input contract is `EXPLORATION.md` § What this phase hands digitisation, unchanged.

## Owed, in priority order

`BL-1007` (measurement parity) · `BL-1009` (the digest cannot see treasuries — region treasury moved
80% with every digest identical) · `BL-1000`'s live click (access was denied 2026-09-16; the board is
built and captured) · `BL-1010` (era_world_harness's three reds pre-date the sprint) · `BL-1008` (six
harnesses still simulate an 80-tick warm start) · `NR-879` (85% of coined cultures never hold ground)
· `NR-881` (the seat viability floor reads eight quarters against a 12-tick settle).

## Standing hazards this sprint re-learned

- An agent's **worktree** survives a stop; its **uncommitted work** does not. Make a checkpoint
  commit the first instruction in any long brief.
- Sweep artefacts and NR readings must be regenerated on the **final integrated tree**; two items
  reported the same digests because a sibling clobbered a shared scratchpad file.
- `NR-` ids must be minted against the **hot store and the archive together**; the hot file's maximum
  is not the true maximum after an archive pass.
