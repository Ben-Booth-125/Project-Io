# Next session — space: sprints 65 (near space) and 66 (far space)

Handoff written **2026-11-10** by the session that designed and built the space work. Everything
is on branch **`claude/space-mechanics-design`** (worktree `.claude/worktrees/space-mechanics-design`),
**not merged into main and not pushed**. Merging is Ben's (NR-1162).

---

## 1. Where it stands

- **Sprint 65, near space (v0.4.0):** every item complete except BL-1593 (space spectator check),
  closed by this note, and BL-1630 (launch goods supply), blocked on NR-1203.
  - orbital graph and L-points, slots, the launch gate, stations on a 5×3 fitted grid, astronauts,
    satellite roles, signal delay, the space-verb seam, rival near-space AI, the Circumplanetary
    draw, the Space ledger and designer.
- **Sprint 66, far space (v0.5.0):** every item complete except BL-1611 (far-space spectator
  check), blocked on NR-1203. BL-1632 (stale tail harnesses) and BL-1634 (garrison chase loop) are
  done; BL-1635 (ownerless harness failures) is open.
  - space unit designs and movement, Haulers, battles, debris, blockade and bombardment, mapping
    then prospecting, off-world bases, war across bodies, front pins, the propellant loop, solar
    storms, space firsts, the Early Space tree (researchable, with its surfaces), the transit
    segment, rival far-space AI, the fleet UI, the base and prospect UI, and the front menu by body.
- **Save version 92** (v81 stations … v92 transit origins). Space verbs 0–20.
- **Gates on the branch:** the space harness suite passes (17 harnesses), plus
  world_determinism, save_roundtrip and nation_tech; the eight space visual
  scripts and base_prospect_ui pass at 0 failures. No golden or digest was re-blessed.

## 2. The spectator finding — the first hurdle (BL-1593)

**Rivals reach for space and are stopped at the shelf.** Seed 0, Release, `--spectate-headless`:

| Year | What the rivals do |
|---|---|
| 1976 | First rival holds Rocketry. |
| 1977 | First Launchpad laid. Pads take 5–9 years, starved of steel and refined fuel. |
| 1995 | 4 staffed pads and centres; **0 launches**. |
| 2015 | With the launch bid visible, plants exist, but goods pile on the wrong markets of the one body; **still 0 launches**. |
| 2030 | Far space: 9 designs held by three rivals; **no unit raised**, 1 launch all game. |

**Cause, measured:** auto trade ranks routes by margin per trade point. Launch goods move few
units per point, so their routes rank #68–#141 of about 250 and the points run out first.

**Two experiments, both unmerged:**

- **A**, rank silenced-want routes first (`worktree-agent-a9f2b8a287ef3a8e6`, 8aa5f49b):
  launches from 1992, 4 by 2015. It fails market_viability G1 at tick 50 (86.0 → 80.8).
- **D**, a launch draws from any market on its home body where the nation owns ground
  (`worktree-agent-a83d4ca4b411f9d7d`, bbcb17b5, behind a switch that is off and byte-identical):
  launches from 1993, 4 by 2000, then a stall. Every other good is unmoved.

**Recommendation:** D first, then B (dense goods' trade capacity) for the stall. Ruling NR-1203
unblocks BL-1630, BL-1611 and the v0.5.0 spectate.

**What the AI does well:** it researches toward space, lays pads, staffs centres, designs hulls and
bids for its launch goods by the right rules. The far-space AI passes its harness (60/0).
It is the economy that refuses the goods, not the agent that fails to ask.

## 3. Waiting on Ben

- **NR-1203** (the trade wall). Everything in § 2 waits on it.
- **Verify-read grants:** NR-1208 (seven fleet reads) and NR-1211 (four base and prospect reads).
  Both would become scoped exceptions in `.claude/rules/io-standing-rules.md`.
- **Calls taken on your behalf:** NR-1195 through NR-1213; `docs/development/NEEDS_REVIEW.md`.
- **Digest re-pin (NR-1167):** on your go only.
- **Ids that collide with main (NR-1198):** main holds BL up to 1618, NR-1181..1194 and save v88
  (naval). This branch uses BL-1580..1635, NR-1195..1213 and saves v81..v92. A renumber pass is due
  at the merge.

## 4. Live clicks owed

- **BL-1609 (fleet UI):** fleet mark; Move with preview and confirm; re-press disarms; Dock and
  Undock; Refuel; Bombard and lift; Military Design Space save; Solar fleet press.
- **BL-1613 (Early Space research surfaces):** the Early Space tab, and shift-click to queue.
- **BL-1610 (base and prospect UI):** found a base from a landed Lander (toggle, pick, un-pick,
  confirm); embark on a seed with a staffed Launchpad; the slate wash, title counts and
  "Not yet prospected" hover on a part-prospected moon.
- **BL-1633 (front menu by body):** Assign to front lists only same-body fronts.

## 5. Traps this work paid for

- **Every lane appends one save section and bumps the version.** Renumber at merge; the
  tail-offset harnesses read world_save_tail_bytes_after; a static_assert breaks the build until
  the new section joins its table.
- **Two hunks ending before a shared closing tail lose a brace** when both sides are kept.
- **The worktree guard refuses complex inline shell** (variables, loops, awk). Write a script.
- **market_viability quick ends about 1972**, before Rocketry. It cannot see a space change.
- Older traps still stand: shell heredocs eat `\n`; `sed -i` rewrites CRLF as LF; never `taskkill`
  cl.exe; NR ids mint across hot and cold; background build exit codes are the echo's.
