# Io Earth-Like World Tests — Findings & Economical Test Plan

**Date:** 2026-08-03. **Status:** Stage 1 built and run under dispensation (uncommitted);
stages 2–3 designed.
**Provenance:** authored in a Project-Rival session from a six-reader recon of the Io repo.
Rival never writes Io source, so the harnesses named here are built by an Io session (or
this one under explicit dispensation). Adopt this doc into Io (`docs/generation/` or the
backlog item that executes it) when the work is picked up.

**Purpose:** record what exists for (a) an AI playing Io via text and (b) measuring which
generation parameters lead to earth-like worlds — then define the smallest test battery
that answers (b). Economy is the design constraint: cheapest test first, each stage's
output gates the next, no infrastructure until a run demands it.

---

## 1. Findings — AI play via text (context for the serve seam)

BL-278 (MCP server) landed 2026-08-03: five tools over `ProjectIo --serve` — blackboard
read (BL-206 schema, byte-identical), the eight `corp_command` verbs through player-grade
validation, single-tick advance, action-dictionary lookup (114 entries). An AI can play
today, but only the one canned default world.

Ranked gaps for a real text player:

1. **No world setup.** `--serve` calls `make_hard_coded_world()` with default params;
   the signature already accepts `world_params` (seed + eight leans). One flag of plumbing.
2. **No batch tick, no reset.** One tick per round-trip; restart = kill the process.
3. **No corp discovery.** The agent must be told its corp id out of band.
4. **Write channel narrower than the dictionary.** `place_sell_order`,
   `remove_sell_order`, `set_workforce_auto` are dictionary entries with no `corp_verb` —
   a text player cannot use the order book.
5. **BL-279 (trace corpus) has no substrate** — MCP decisions are not captured.

**Convergence:** seed + preferences plumbed into `--serve`, plus a RESET opcode, serves
both agendas at once — AI play over varied worlds, and batch play-testing. Same wrapper.

Small defects spotted in passing (candidate review-queue entries, not filed):
`resources/list` unimplemented despite server.js's header comment; `get_blackboard`'s
`ticks` arg only relabels a current-state read; building-type range 0–5 (server.js) vs
0–4 (main.cpp comment); `.claude/settings.json` still carries the four per-binary allows
CLAUDE.md says were removed, and lacks the `build_gen`-anchored rule it describes.

---

## 2. Findings — what "earth-like" already means in Io

Io has a codified Earth: `homeworld_viability` (src/world/planetology.cpp:249) — a
strict floor of **ten criteria expressed as fourteen clauses** (each two-sided range
rejects from either end, and which end fires turns out to matter). Cradle archetype;
civilised; O₂ 16–30%; liquid hydrology; ocean 40–75%; 275–305 K; arable ≥ 8%; escape
velocity 9.5–13 km/s; fuel endowment ≥ 0.35; iron ≥ 0.80. Enforced by reject-and-reroll
(cap 512), never clamped — the settled discipline (Ben: "generated Earths read as
forced" when clamped).

Two measurement harnesses exist, both stdout-only, in-process aggregation:

- **`tools/verify/planetology_sweep.cpp`** — sweeps the 8 wizard leans over the homeworld
  slot only. Measures acceptance (77.4%, 1.29 mean draws) and p05/median/p95 spreads of
  nine metrics (iron, coal, oil, copper, O₂, arable, ocean, temp, escape velocity).
- **`tools/verify/mediterranean_sweep.cpp`** — sweeps campaign seeds through Kepler's
  exact shipped wiring; hex connected-component labeling over the ocean mask; enclosed-sea
  / strait-sea rates (BL-276: floor 100%, arena 89.6% at 500 seeds).

### The four measurement gaps

1. **`viability::reason` is dead data.** Documented "for the sweep's histogram"
   (planetology.hpp:605); no consumer exists. Nobody knows which of the 13 clauses
   actually does the rejecting.
2. **No categorical outcome map.** The sweep records no `body_archetype`, no `died_at`,
   no `life_stage`/`peak` — what the rejected ~23% *become* is unmeasured, and the ten
   raw `planetology_params` are never swept directly (only leans).
3. **Ocean is an echo, not an outcome.** Homeworld `water_fraction` =
   `clampf(p.home_ocean, 0.20, 0.85)`; every other body is hard-coded 0.55. The sweep's
   "ocean spread" measures its own sampling band.
4. **No tile-level earth-likeness metrics.** Only the ocean mask is labeled. Continent
   count, largest-landmass share, coastline shape, composition-within-band (forest in
   temperate, icecap, desert), and river behaviour on real Kepler are all uncomputed.
   `mediterranean_sweep`'s `label()` is directly reusable for a land mask.

### Couplings a sweep must respect

- **Leans cannot isolate a variable.** Changing any preference (or seed) changes the
  reject-and-reroll attempt count, which re-draws every resolved parameter. One-at-a-time
  isolation is only real *below* the entry point, at `run_planetology(body, params, seed)`
  — a pure function over arbitrary raw params.
- **Orbit is derived from star mass** (habitable-band placement), and the interior lean
  sets age + radiogenic jointly. A star-mass corridor must reproduce or explicitly sweep
  the orbit derivation.
- **Kepler's tile seed is itself reject-and-reroll probed** (golden-ratio stride), so a
  campaign-seed sweep has a hidden non-linearity: adjacent seeds can converge.
- **`abiogenesis_ease` is pinned to 1.0** by `resolve_preferences`; only a raw-param
  sweep can vary it. `abundance` is the one cleanly isolated knob (pure post-multiply).

### Prior art this plan extends (not replaces)

**BL-240 (whole-pipeline generation sweep)** — designed, unbuilt, post-v0.1.0 — is the
designated instrument; Ben's update explicitly wants failure cases as output. **BL-275
(history sweep distributions)** sets the reporting discipline: deterministic per seed;
**report, don't gate** — assertions only after Ben has seen the raw spread. This battery
is BL-240's planetology + tile floor, delivered early and small.

---

## 3. The economical test plan

Three stages, strictly ordered; each stage's output gates the next. Total new code:
~30 lines added to one existing file, plus two new harnesses. Total compute: minutes.
No sharding args, no JSONL merge infrastructure — every run here is a single process;
"parallel" means the independent stages/jobs run side by side in WSL. Build
infrastructure only when a single run exceeds ~2 minutes, not before.

### Stage 1 — Rejection census — **BUILT AND RUN, 2026-08-03**

**Question:** which viability clauses actually kill earth-likeness, and what do the dead
worlds become?

**Built as:** a `C1` section added to `tools/verify/planetology_sweep.cpp` (~150 lines,
no new file, no change to `src/`). Draws one unshaped attempt per seed, runs
`run_planetology` + `homeworld_viability`, and histograms `reason` — the first consumer
of a field the header has documented "for the sweep's histogram" since BL-167. Also
records `archetype` and `died_at` for every reject, closing gap 2 in the same pass.

**The mirror, and why it is safe.** `resolve_preferences` returns only the draw that
PASSED, so rejections are invisible from outside it. They are recoverable because a draw
is a pure function of (preferences, seed, attempt), replayed via the public
`checkpoint_rng` (bit-identical to planetology.cpp's internal `rng`). That replay
duplicates the sampling band table, and duplicated tables drift — so **every censused
draw is cross-checked against the live function**: a viable replay must mean
`attempts == 1` with bit-identical params, a rejected replay must mean `attempts >= 2`.
A band edited in planetology.cpp fails the harness instead of silently re-pointing the
histogram. 20,000/20,000 draws agreed.

**Cost measured:** 20,000 draws in **0.166 s** (WSL2, g++ -O2). Results stable between
the 2k and 20k runs. Comfortably inside the 60 s CTest cap.

**Built and run on both toolchains.** g++ 15.2.0 (WSL2) and MSVC 14.44.35207 (the
version `build/` is pinned to) — compiles clean on both, and the CMake glob picks it up
as a CTest target with no wiring needed.

**Unplanned bonus result: cross-compiler determinism is now empirically verified.** The
two runs agree on **every single count** — 15692/4308 accepted/rejected, 2056/1128/1014/110
by clause, 2252/26/2030 by archetype. That is 20,000 worlds through the full ten-stage
chain producing bit-identical outcomes across two compilers on two operating systems.
PLANETOLOGY.md's determinism discipline (no `std::` distributions, no `exp`/`log`/`pow`
in any gate path, splitmix64 with an explicit float mapping) is doing exactly what it
claims, and the `checkpoint_rng` replay inherits it. Nothing in the repo was testing this
across compilers before; it fell out of running the census twice.

#### Results (20,000 unshaped draws)

Acceptance **78.5%** on the first draw — an independent cross-check of the documented
77.4% (1.29 mean draws ⇒ 77.5%), from a separate seed stream.

| Clause that rejected | % of rejects |
|---|---|
| not a Cradle | 47.7% |
| not enough arable land | 26.2% |
| O₂ too high (fire-suppressed) | 23.5% |
| O₂ below breathable/combustion floor | 2.6% |
| **the other ten clauses** | **0.0%** |

Rejects became: Cradle-that-missed-a-number 52.3%, Boring Billion 47.1%, Mat World 0.6%.
They died at: the **Breath** gate (S6, the second oxygenation) 47.7%, or survived every
gate and failed a number 52.3%.

#### What the census establishes

1. **Ten of fourteen clauses never fire.** Never: no civilisation, no liquid surface
   water, too dry, too wet, too cold, too hot, gravity too low, gravity too high, no
   primary fuel, iron too poor. This is not a defect — it is reject-and-reroll working
   as designed. The sampling bands were calibrated against this very sweep, and now sit
   strictly *inside* the floor: `home_ocean` samples 0.42–0.72 against a 0.40–0.75
   window, so "too dry"/"too wet" are unreachable **by construction**; the carbonate
   thermostat pins temperature to ~277–288 K against a 275–305 window; `home_mass`
   0.72–1.32 lands inside the 9.5–13 km/s gravity window. The floor's live surface is
   **oxygen and arable land, and nothing else.**

2. **Oxygen is the master variable.** Boring Billion (died at Breath) 47.7% + O₂ too
   high 23.5% + O₂ too low 2.6% ≈ **74% of all rejections are the oxygen story**. And
   the second numeric clause is not independent of it: `arable_share = land_frac ×
   (0.28 − (o2 − 0.21) × 0.45) × mobile_lid_factor`, so high O₂ depresses arable too.
   The two live clauses share a driver.

3. **Only three archetypes are reachable** from the homeworld bands, consistent with the
   doc's known dormancy (six of thirteen archetypes have no body to fire on in the
   shipped set).

#### Why `interior=low` is the expensive lean — **resolved by reading the chain**

The census raised it (2.52 draws vs a ~1.24 baseline) but could not explain it. Tracing
S6 settles it, and corrects the guess I first offered ("it starves the timing budget"):

The Breath gate's second sub-gate is `noe_fires = (noe_at > 0.10) && st.mobile_lid`, and
`mobile_lid` requires `theta` in **[0.55, 2.2]**. Meanwhile
`theta = radiogenic_decay_curve(age) × clamp(p.radiogenic, 0.3, 2.2) × (mass/radius)`.

So **`system_age_gyr` enters the gate twice, with opposing signs**:

- through `budget = age − 1.0` → older gives a *larger* timing budget, which **helps**
  the NOE fire;
- through the radiogenic decay curve → older gives a *colder* interior, lowering `theta`
  toward the 0.55 mobile-lid floor, which **hurts**.

`interior=low` ("cold and old") pushes both halves of its folded preference the same way
on the second path — age 6.0–8.0 Gyr *and* radiogenic 0.60–0.95 — so `theta` falls under
0.55, the lid goes stagnant, the tectonic nutrient shock never arrives, the Boring
Billion lock never breaks, and the world dies at Breath as "not a Cradle". The heat path
dominates the budget path.

**The real driver is `radiogenic`, not age** — and it feeds *both* live clauses, because
`mobile_lid` also multiplies `arable_share` by 0.72 when stagnant.

### Stage 2 — Knob corridors — **now 2–3 jobs, not 3–5** (gated by Stage 1)

**Question:** per implicated raw knob — where are the earth-like edges, and what does the
world die into beyond each edge?

**Stage 1 narrowed this, which was the point of running it first.** The census says
~74% of rejection pressure is the oxygen story and the rest is arable land, so the
corridor sweep targets:

1. **`oxygenation`** — the dominant knob. Drives the Breath gate, both O₂ clauses, and
   (through the arable formula) part of the second numeric clause.
2. **`radiogenic`** — promoted from conditional to firm by the S6 trace above. It sets
   `theta`, which gates `mobile_lid`, which is a hard AND in the NOE sub-gate *and* a
   0.72× multiplier on `arable_share`. Like `oxygenation`, it feeds both live clauses.
3. **`home_ocean`** — the other input to `arable_share`, via `land_frac`.
4. **`system_age_gyr`** — worth one corridor despite entering only indirectly, precisely
   because it enters **twice with opposing signs** (timing budget up, interior heat
   down). That is a non-monotonic corridor, and a stepped sweep is the cheapest way to
   see where the two effects cross.

**Dropped by evidence:** `star_mass`, `home_mass`, `metallicity`, `coal_climate`,
`drawdown`. Their clauses never fire inside the sampled bands, so a corridor over them
would map a region the generator never visits.

**Both live clauses now have named mechanisms**, which is the useful state to stop
measuring and start deciding from: oxygen enters through the `dial` in the GOE/NOE timing
and the combustion floor; tectonic heat enters through `theta → mobile_lid`. Everything
else in the floor is guaranteed by the sampling bands.

**Method:** new harness (`tools/verify/earthlike_corridor.cpp`), below the entry point:
hold Sol defaults, step ONE `planetology_params` float across its internal clamp range
(64 steps × 128 seeds), record archetype / `died_at` / viability + reason / temp / O₂ /
water state. At Stage 1's measured speed (~8 µs per chain run) a full corridor is well
under a second — so this is one process, not a parallel fan-out.

**Known-trivial axis, stated up front:** the ocean corridor will just recover the input
clamp — ocean is not derived (gap 3). Its corridor answers "what window survives," never
"what produces it." Do not spend seeds pretending otherwise.

**Cheaper first move, now that C1 exists:** add a per-lean breakdown to the census (walk
the 8 axes × 3 leans through the same histogram). That reuses built code, costs ~1 s,
and may answer the interior question without writing the corridor harness at all.

**Contingent (NOT built now):** a pair-interaction atlas (star×age, mass×ocean,
oxygenation×drawdown). Trigger: a Stage 2 corridor edge that visibly shifts when a second
knob moves, or a census clause whose rejections no single knob explains. Otherwise skip.

### Stage 3 — Tile census (one run at 500 seeds)

**Question:** do accepted (viability-passing) worlds *look* like Earth at tile level?

**Method:** new harness (`tools/verify/earthlike_tile_census.cpp`) cloning
`mediterranean_sweep`'s exact Kepler wiring and reusing its `label()`. Per seed:
land-component count and largest-landmass share of land; coastline ratio (land tiles
adjacent to ocean / land tiles); forest-share-within-temperate-band, icecap and desert
shares; mountain fraction; river source count + fraction reaching ocean; enclosed-sea
presence (reuse). 500 seeds — same scale as the BL-276 run — one process.

**Advisory Earth bands (report, don't gate):** land ~25–35% (Kepler targets 40% land);
largest landmass ~45–70% of total land; 3–8 land components ≥ 100 tiles; the rest are
reported as raw spreads for Ben to set bands against. Assertions come only after the
first run is eyeballed — BL-275's discipline, verbatim.

**Deferred entirely:** the lean→outcome trace (8 axes × 3 leans × tile metrics — 24 jobs).
Highest cost, lowest immediate information; run only when a wizard-facing question
("does ocean=high give archipelagos?") actually arises.

### Cost summary

| Stage | New code | Jobs | Compute | Gates |
|---|---|---|---|---|
| 1 census ✅ | ~150 lines in existing sweep | 1 | **0.166 s measured** | picked Stage 2's knobs: 5 of 10 dropped |
| 2 corridors | 1 new harness | 2–3 | sub-second each | may trigger pair atlas |
| 3 tile census | 1 new harness | 1 | ~1 min class | Ben sets bands after |

The planetology chain is far cheaper than assumed (~8 µs per world), so **no sharding,
no JSONL, no merge step is warranted at any stage** — the infrastructure the first draft
of this plan reserved for later is now positively ruled out for stages 1–2. Stage 3 is
the only stage whose cost is not yet measured, because it generates tiles rather than
scalars.

Execution route: WSL2 clone (`~/Project-Io`, Ninja Release, ctest or direct) — the
standing answer to Smart App Control blocking fresh Windows exes. CMake's
`tools/verify/*.cpp` glob auto-registers both new harnesses; keep their default arg
sizes under the 60 s CTest cap.

---

## 4. Open calls for Ben

1. **Commit Stage 1?** The C1 census is written and passing in
   `tools/verify/planetology_sweep.cpp` but **uncommitted** — dispensation covered
   building and smoke-testing, not committing. It is an Io-source change made from a
   Rival session, so it wants a deliberate call, not a default.
2. **Is a floor with ten inert clauses the floor you want?** The census says the bands
   already guarantee ocean, temperature, gravity, fuel and iron, so those clauses are
   unreachable. Three readings, all defensible: (a) leave them — they are a free
   backstop if a band is ever widened; (b) widen the bands so the floor does the work
   and variety rises; (c) narrow the floor to the two live clauses and admit the bands
   are the real specification. This is a design call, not a defect.
3. **File the passing defects?** §1's MCP/server chips and the settings.json drift are
   one review-queue append away — say the word.
4. **Stage 3 bands:** the advisory Earth bands above are placeholders for your judgement;
   the first 500-seed spread is the evidence to set them against.

## 5. Session log — decisions taken on Ben's behalf

Per Rule 0c, recorded as taken rather than saved for a summary.

- **Mirrored the band table rather than changing `src/world/`.** Censusing rejections
  needs draws `resolve_preferences` discards. The alternative was a small out-param on
  `resolve_preferences` to record reasons — cleaner data, but a public-API change to the
  core generation file from a Rival session. Chose the mirror plus a per-draw
  cross-check, so the duplication cannot drift silently. Reversible: if the census
  becomes permanent, the out-param is the better long-term shape.
- **Asserted only the mirror's integrity, never the distribution.** Which clause
  dominates is reported, not gated — BL-275's discipline, applied before Ben has seen a
  spread.
- **Built in WSL via direct g++, not CMake.** The WSL clone at `~/Project-Io` is far
  behind local main (no `checkpoint_rng` at all), and a CMake configure would fetch
  SDL3/Lua/sol2 for a harness that needs none. Compiled the SDL/Lua-free world superset
  straight from the live Windows tree into `~/io-sweep`. The stale WSL clone was left
  untouched — but it is stale enough to mislead a future session that trusts it.
- **Then verified the MSVC build too, because WSL is not the route Ben uses.** A harness
  that only compiles under g++ would break the whole CTest tier on Windows. Built the
  single target against the BuildTools 14.44 toolchain the existing `build/` cache is
  configured with — deliberately NOT the Community 18 vcvars the verifier-headless skill
  names, since mixing toolchain versions against a configured cache is the documented
  `<variant>` explosion.
- **Corrected this doc's own error:** the floor was described as 13 clauses; it is ten
  criteria expressed as fourteen clauses. The census made the miscount self-evident.
- **Tightened one inconsistency found reviewing my own C1 code.** The clause table fails
  loudly on an unknown reason, but the archetype histogram silently dropped out-of-range
  values. `body_archetype` has no `count` sentinel (append-only save-format ids), so its
  table size is hand-held — and BL-209 has already appended three. A fourth would have
  vanished from the histogram with the percentages quietly failing to sum. Now counted
  and asserted, matching the clause discipline. C1 carries three checks, not two;
  measurements unchanged.
- **Answered the interior-lean question by reading the chain, not by extending the
  harness.** The proposed per-lean census rerun would have added more uncommitted Io
  source while the "commit Stage 1?" call is still open. Tracing S6 cost nothing, has no
  blast radius, and gave a sharper answer than a histogram would have — it named the
  mechanism (`theta → mobile_lid`) rather than just the correlation, and refuted my own
  first guess about the cause.
