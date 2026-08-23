# Sprint N1 — quarantined slices, 2026-08-22

**Nothing in this directory is a build input.** The world library globs
`src/world/*.cpp` and the harness batch globs `tools/verify/*.cpp`; this path is in neither, so
none of it compiles into anything. That is deliberate: **two of the three slices are `NOT_SOUND`**
and must not reach a build glob until they are fixed.

The three implementing agents **did not commit** — their work was untracked files in the workflow
worktrees, which are reclaimed. This directory is the salvage. It is the whole of the run's output,
~2,970 lines.

## Verdicts, from the adversarial pass

| Slice | Item | Verdict | Harness as written |
|---|---|---|---|
| **A-anchor** | BL-543 | **NOT_SOUND** | 17/17 green — and green means nothing |
| **B-sentiment** | BL-545 | **SOUND_WITH_FINDINGS** | 39/39 green; production code sound, one row false |
| **C-budget** | BL-537 | **NOT_SOUND** | 25/25 green — over an engineered fixture |

**Every one of them was green.** That is the finding worth carrying out of this run: three harnesses
written by the agents that wrote the code, all passing, and two of the three subjects defective. The
reviewers found it by **mutating real content and running ASan**, never by reading.

## A-anchor (BL-543) — NOT_SOUND

**F1 — the harness fails on correct content.** R3a/R3b/R3c and R4b hard-assert that
`scripts/economy.lua`'s upkeep rates are still bit-exactly zero. The reviewer authored the
harness's *own solved fixture* into a copy of economy.lua and got **exit 1, 13 passed / 4 failed —
with R1a green and all 19 rows in band.** The anchor is satisfied and the check is red. It would
report FAIL on the one day this harness exists for. R3 has to be a conditional branch, not an
assertion.

**F2 — the content claim is a false green.** `calibrate()` solves the wage *from* `base_price`, so
the fixture self-normalises to ratio 1.000 for any price table. The reviewer **multiplied all 33
authored base prices by 10× and the harness stayed 17/17 green.** So at shipped content the
200-line Lua parser binds exactly two facts: the rates are zero, and two goods are named. Nothing
constrains `base_price` at all.

## B-sentiment (BL-545) — SOUND_WITH_FINDINGS, the one worth keeping

The production code is genuinely deterministic, genuinely inert at authored zero, and
**structurally cannot touch a stance table** — `sentiment.cpp` includes no world header and every
signature takes only `sentiment_table&`. That is the stance invariant enforced by construction
rather than by a test, which is stronger than what was asked for.

**One false green:** R6c/R6d claim the fold is order-independent, and test no such thing. Their
input emits exactly one event per ordered pair, so nothing ever accumulates. The reviewer **deleted
`std::stable_sort` from `sentiment.cpp:159` and all 39 rows still passed.** The mechanism works; the
row guarding it would not notice its removal.

## C-budget (BL-537) — NOT_SOUND, two confirmed defects

**Defect 1 — heap-buffer-overflow, `nation_budget.cpp:62`.** The gather loop validates corp, nation
and amount, and **never `c.line`**. `budget_priority` has a `uint8_t` underlying type, so 0..255 are
valid enum values, and the code indexes a 9-element `std::array`. **AddressSanitizer confirms an
out-of-bounds write.** The standing rule that an AI-facing seam is an untrusted input boundary makes
this a real hole the moment a claim arrives from the MCP seam. One line fixes it.

**Defect 2 — the treasury can be overdrawn**, which R4 claims cannot happen.

**And the false green that matters most: conservation is not bit-exact.** The fixture is engineered
so every intermediate float is a dyadic rational — treasury 1000, reserve ¼, weights ½-¼-¼,
pro-rata scale exactly 0.75. **At ordinary weights, conservation fails: 1.53e-05 destroyed in one
tick.** That is the BL-392 class of silent value destruction, at nation grain, under the row written
to prevent it.

## What to do with this

Do not merge any of it as-is. B's production code is the closest to landing and needs one harness
row rewritten. A needs R3 inverted and its content claim either dropped or earned. C needs the
bounds check, the overdraw fix, and a design answer to the conservation question — which is
recorded as NR-540, because **bit-exact conservation may not be achievable with float weights at
all**, and that reaches back into the item's design rather than its code.
