# REFINED — active worklist

Sprint 32 (gamified generation), **wave 2 — the eight-phase reorder**. Promoted 2026-09-03.

## The batch, and why it is four items rather than twelve

Ben asked for the twelve reorder items as a batch delivery. Carving the collision map turned it
into **four waves**, because the dependency graph is deep rather than wide — BL-765 needs BL-763
and BL-764; BL-768 and BL-769 need BL-766 and BL-767; BL-770 needs BL-771; BL-772 needs BL-770;
BL-773 needs BL-772. A twelve-item "batch" over that graph is a sequence wearing a batch's name.

**This wave delivers four**, chosen as the deepest independent roots — two phase foundations, one
prerequisite that unblocks the largest win, and one instrument that another item is already
blocked on:

- [ ] **BL-762 (resource origin split)** — phase 1. A `resource_origin` classification, and the
      Body phase places only geological deposits. HAZARD: `tile_rng` is shared with the endemic
      draw and with hazard/habitability jitter, so removing a draw moves them.
- [ ] **BL-763 (continent time axis)** — phase 2 foundation. `run_continents` returns an ordered
      sequence of plate snapshots with a defined epoch length; the endpoint is unchanged.
- [ ] **BL-771 (tick length is a constant)** — phase 6 prerequisite. Make tick length a parameter
      and classify every rate by its true period. Unblocks BL-770, which unblocks BL-772, which is
      the 72-second win.
- [ ] **BL-757 (sweep measures another era)** — the instrument BL-767 is blocked on. Point the
      sweep at the run generation actually performs.

## Held back, with reasons

- **BL-764 (Lagrangian tiles)** — difficulty 5, and its own item says it should split again at
  promotion. It is a representation change to the oldest layer in the generator and every pass
  downstream reads its output. It gets its own delivery, not a slot in a batch.
- **BL-765 (paleo deposits)** — needs BL-763 and BL-764.
- **BL-766 (population map early)** — difficulty 5, touches the sim, the ECS and two save
  versions, and it collides with **BL-758**, which is an open ruling for Ben. It probably
  *dissolves* BL-758 (if centres exist before the sim, the sim's seed-any-zero-population rule
  becomes moot), which is a reason to sequence it deliberately rather than race it.
- **BL-767 (empires reliably form)** — gated on BL-757 landing in this wave. Next wave.
- **BL-768, BL-769** — need BL-766 and BL-767.
- **BL-770 (Era 0 candidate search)** — needs BL-771 from this wave, and BL-761.
- **BL-772, BL-773** — need BL-770.

## Barriers (Batch Delivery semantics)

Every task across all four items reaches a terminal state before **any** item is committed. Then
**one** `verifier-review` pass over the whole integrated set — the failure it hunts is
cross-slice. Commits are one per item, back to back, after the review closes clean.

## The invariant every slice inherits

The 0 CE world stays byte-identical unless an item states otherwise and blesses it deliberately.
The acceptance test is `world_determinism`'s digests, measured 2026-09-03:
`039EE9880739CDF6` (seed A on), `B0EBBA249B3DDABB` (seed B on), `DE55600457797638` (off).
BL-762 is the one item that may legitimately move them; if it does, the movement is stated and
blessed on purpose, never absorbed.

**Requirement groups.** `resource-origin-split`, `continent-time-axis`, `tick-length-parameter`,
`sweep-measures-generation`.
