# Project Io — Needs Review

**Ben's review queue.** Readable mirror of [`NEEDS_REVIEW.json`](NEEDS_REVIEW.json), which is
canonical — the JSON wins on any disagreement.

Things here are waiting on **your judgement**, not on work. Three kinds:

| Kind | Meaning |
|---|---|
| **question** | An open call nobody has made. Not blocking — a blocking item is a backlog entry with `blocked_on` set. |
| **decision-taken** | A call made **on your behalf** so work could continue. Recorded so it can be *overturned* rather than quietly becoming precedent. |
| **observation** | Something noticed in passing, too small or too cross-cutting to file, that a human should still see. |

**How this differs from the neighbours.** [`review.json`](review.json) is a *blocker* list — items
blocked on a visual artifact only you can produce (a mockup, an annotated screenshot); work there
cannot proceed at all. [`backlog.json`](backlog.json) is *work*. Entries here are neither: they are
questions and reversible calls. If an answer creates work, file a backlog item and resolve the entry
with that item's id. If an entry turns out to need a mockup, move it to `review.json`.

Entries are **never silently deleted** — set `status: resolved` and write the resolution, so the
reasoning survives the answer.

*Seeded 2026-08-01 at Ben's request, populated from that session.*

---

## Open

### NR-001 — the ledger says "50% staffing", but your own corp auto-solves the dial on tick one
*question · from BL-162 (tile construction ledger)*

The caption reads *"Est. net / tick at today's local prices - 50% staffing, no labour shortage."*
The estimate uses `workforce_assigned = 0.5`, mirroring `construction.cpp` exactly — so it correctly
describes the building **at the instant it is created**. But BL-181 defaults player buildings to
`workforce_auto`, and the dial is auto-solved for maximum profit on the first tick.
`prospective_profit_harness` measured realised extraction at **2× the estimate** for precisely this
reason, and had to pin `workforce_auto = false` to make its cross-check mean anything.

The figure is honest for the moment it is shown, and misleading as a prediction — which is how a
player reads a profit bar. Every candidate is understated by the same factor, so the **ranking** is
unaffected; the **magnitude** is not.

- Leave it — defensible, and ranking is what the chart is for.
- **Say it is a floor** — *"at least +1243 / tick"*, or append *"before auto-staffing"*. Two words.
- Estimate at the auto-solved workforce — most accurate for your corp, but it stops mirroring
  `construction.cpp` and diverges from what a background corp gets.

> **Recommendation:** the middle one. The chart ranks correctly already; two words stop the
> magnitude reading as a ceiling when it is a floor. The third option buys accuracy at the cost of
> the estimate no longer describing the building actually created — the property that makes it
> verifiable against `economy_system`.

### NR-002 — the header reads `/ qtr` while the ledger beside it reads `/ tick`
*observation · noticed while inspecting the BL-162 goldens*

The header prints `NET +3.2k / qtr`. The construction ledger, the Selection band and
`draw_building_profit` all print `/ tick`. `GLOSSARY.md` defines **Tick**; it does not define "qtr".

The standing rules say a defined term must not be substituted. This is a live violation inches from
figures that get it right, on the first screen a new player reads. It is also pre-existing and
outside BL-162's scope, which is why it was flagged rather than fixed — changing a header figure's
units is a decision about what the header is *for*, not a typo.

- Change the header to `/ tick`, matching every other surface.
- Keep `/ qtr` and define **Quarter** in GLOSSARY, if the header really does report a longer period.
- Leave it.

> **Recommendation:** needs your intent first. If the header genuinely aggregates over a quarter it
> is a different quantity and wants a glossary entry; if it is an old label for the same per-tick
> figure, change it. Worth checking which it actually computes — I checked the labels, not the maths.

### NR-003 — a sub-tick payback prints "payback ~0 ticks"
*observation · from BL-162, visible in the blessed golden*

*Extraction: Agricultural Produce* shows `686 cr - payback ~0 ticks` (capex 686, net +1243/tick, so
true payback ≈ 0.55 ticks). Cosmetic, but `~0 ticks` is the one value in the row readable as an error
or as "costs nothing". The intended meaning — it pays for itself inside the first tick — is stronger
than what it prints.

Floor the display at 1, or special-case the wording (*"pays back within 1 tick"*).

> **Recommendation:** the wording if it fits the row width, else floor at 1. It is here rather than
> in the backlog because it is too small to file and too easy to lose.

### NR-004 — should non-home bodies have markets at campaign start?
*question · from BL-254, reported by the harness itself*

The generated world seeds **all six markets on Kepler**. A `trade_route` is body-level, so intra-body
lanes record nothing — meaning the generated world **cannot record a trade route however long it
runs**, regardless of the launchpad gate. The data-creep harness had to author three stub markets
pre-run just to give its convoy lanes endpoints. This is a second cause of BL-254's blind spot,
independent of the launchpad gate the filed item named.

This is a world-premise question, not a harness problem. If no other body has a market at start then
inter-body trade — one of the two pillars every system is meant to feed — has no destination on turn
one, and the activity fog (BL-089), persistent routes (BL-088) and the supply lens all describe
machinery with nothing yet to act on.

- **Intended** — Era 0 is terrestrial by design; record it in `ERAS.md` / `MARKETS.md` so it stops
  looking like an omission.
- Seed a minimal market on one other body at start.
- Revisit when the Era 1 transition is actually built.

> **Recommendation:** the first or third — this looks intended, given Era 0 is explicitly
> terrestrial. But write it down either way: an instrument had to work around it, which is the signal
> that the premise is undocumented rather than merely unimplemented.

### NR-005 — BL-256 (generation globe): which fidelity tier, and can the preview afford the continents pass?
*question · from BL-256*

The wizard preview runs the **planetology chain only**, so at wizard time there is no height field,
no ocean mask and no terrain for a map-like globe to sample. The item is written with two tiers:
Tier 1 a characterisation globe from the planetology scalars, Tier 2 a real-landmass globe that also
runs the deterministic continents pass plus enough of tile Pass 1 to get a height/ocean field.

Tier 2 is the one you actually asked for. But the preview re-runs on **every control move** and the
continents pass is the expensive half, so it may not be affordable per-move. The cost is unmeasured;
the item says measure before adopting.

> **Recommendation:** build both as filed. Keep Tier 1 even if Tier 2 proves cheap — the early rounds
> genuinely have not decided the landmass, and showing invented continents there would be the
> generation equivalent of a lying figure. The item requires the tier be captioned either way.

### NR-006 — BL-257 (generated body names): which naming register?
*question · from BL-257, the item's one genuinely open design call*

The current five mix three registers: mythological (Helios, Selene), descriptive (Cinder) and
scientist-eponym (Kepler). A generated pool must pick deliberately — drawn at random from all three
it reads as noise rather than as a convention.

- Mythological · Scientist-eponym · Descriptive
- **A deliberate mix with a rule** — e.g. planets mythological, moons descriptive — so the mixing is
  legible rather than random.

> **Recommendation:** the rule-based mix is the deepest and probably the most characterful: a rule
> the player can half-perceive reads as a world with a history of being named, rather than a bag
> drawn from. Squarely your call — the item is written so the pool swaps without touching the
> identity work.

### NR-007 — decisions taken on your behalf, 2026-08-01
*decision-taken · all reversible*

1. **Windows `ai_skill_harness` bands re-blessed.** BL-252 said not to re-bless before knowing which
   cause was at work; the diagnostic established staleness, so this was sanctioned — but it is still
   a bless nobody reviewed.
2. **Four `tile_build_ledger` goldens re-blessed on Windows**, after inspecting the captures.
3. **BL-162 R7's `visual` verification leg removed** — this item staled that golden by construction,
   and a row must not read complete on a red leg. The code and headless legs stand alone.
4. **ctest timeout tiers set** at 60 s default / 120 s for three named long-runners, against measured
   Debug runtimes.

> **No action needed unless you disagree.** One value deserves your eye: `data_creep_harness` now
> runs **42.95 s in a Debug build against its 120 s timeout** — 2.8× headroom, below the ≥3× the tier
> was sized for, because BL-254 added 1500 seeded convoys *after* that sizing. It passes comfortably
> and is nowhere near hang territory, so nothing was changed; it is the value to revisit if that
> rollout grows again.

### NR-008 — `backlog_lint`'s two standing warnings are unowned
*observation*

Every clean run prints the same two: BL-114's requirement group is complete while row R3 is still
pending, and BL-190's group is complete while the item is still `designed` (it should be terminal).
Both predate this session; neither is a fail.

A lint that always prints the same two warnings trains everyone to skim the warning section — which
is exactly where a real new warning will appear. Same failure mode as a permanently-red test.

> **Recommendation:** resolve both as a five-minute cleanup. They were left alone this session
> because both belong to other people's items, and flipping someone else's status silently is worse
> than the warning.

### NR-027 — BL-217 checkpoint retrofit: S8/Legacy has no branch point, so no checkpoint was added there
*decision-taken · from BL-217 (checkpoint/branch/lean foundation)*

The task brief named S8/Legacy as one of four biological die-off points to wrap in a
`checkpoint_record`, alongside Spark, Breath and Green. Reading the code, S8 Legacy has no
`die()`/branch decision at all — it is a deterministic resource-endowment calculation over
whatever `life_stage` the chain already reached. No checkpoint was added there.

BL-217's own admission rule says a checkpoint is a point where the outcome distribution genuinely
branches, not every point where something interesting happens; S8 fails that test. Five checkpoints
were recorded instead — Spark (1), Breath (GOE + NOE), Green (land colonisation + fire threshold) —
matching every genuine branch S5–S8 actually contains.

> **No action needed** unless a future S8 mechanic introduces a real fork, at which point it would
> earn its own checkpoint under the same rule.

### NR-028 — BL-217 verification: fresh worktree could not configure CMake, fell back to hand-compiled `cl`
*observation · from BL-217 (checkpoint/branch/lean foundation)*

`cmake -S . -B build` in this worktree failed at the SDL3 FetchContent step — `codeload.github.com`'s
TLS handshake fails with `CRYPT_E_NO_REVOCATION_CHECK` (confirmed independently with a direct `curl`
to the same URL). Per the task's documented fallback, `planetology_harness` and `planetology_sweep`
were hand-compiled with `cl` instead (mirroring `creeds_harness`'s world-superset TU list) and both
ran clean: 121/121 PASS (19 of them the new R13 checkpoint assertions), and the sweep's metrics
reproduced the doc's committed numbers exactly. The full `ProjectIo` target and the whole-suite
`ctest` (~37/37 expected) could not be run this session for the same reason.

> **Recommendation:** next session with network access (or the main non-worktree checkout), run
> `build_app.bat` then `ctest --test-dir build --output-on-failure` once to close this out.

---

## Resolved

*(none yet)*
