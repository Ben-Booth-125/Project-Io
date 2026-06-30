# Project Io — Development Log

Entries are newest-first. Each entry covers one development session and records what was built, what in-session decisions were made, and what was left open. Decisions that affect the whole project permanently belong in TECH_FOUNDATIONS or a dedicated ADR; this log is for session-scoped choices and progress notes.

Entries that correspond to a tagged snapshot in `backups/` carry an explicit **version** marker in their heading (e.g. *version 0.0.2*) and a **Backup** line naming the snapshot path. These are the rollback points: to revert, restore the named `backups/vX.Y.Z/` tree over `src/`.

---

## Session — BL-057 first native Linux GUI build (2026-06-29)

**Context.** Bringing the full GUI/CMake app up on a fresh Ubuntu 24.04 laptop — the piece
BL-057 left owed because the egress-restricted sandbox can't run the SDL3/Lua/sol2/ImGui
`FetchContent` clones. Two real, previously-unhit build blockers surfaced and were fixed; the
app now configures, builds, and runs natively on Linux.

**What landed.**
- **C declared as a project language.** `project(ProjectIo LANGUAGES CXX)` →
  `LANGUAGES C CXX`. Lua 5.4 is built from `.c` files, but C was only ever enabled as a side
  effect of SDL3's own `FetchContent` `project(LANGUAGES C)`. When that didn't hold, configure
  failed with `CMAKE_C_COMPILE_OBJECT / CMAKE_C_ARCHIVE_* not set`. Now explicit.
- **sol2 v3.3.0 → v3.5.0.** GCC 14+/Clang 19+ reject v3.3.0's `optional<T&>::emplace`
  (`has no member named construct [-Wtemplate-body]`). Fixed upstream in sol2 PR #1606
  (merged Jul 2024), released in v3.5.0. Our sol2 surface is the stable core
  (`sol::state/table/optional`, `safe_script_file`) — unchanged across the bump, so risk is low.
  Until this landed, the workaround was building with GCC 13.

**Verification status.** Both fixes **confirmed on the GCC-14 laptop**: a clean build with the
*default* compiler (no gcc-13 override) configures, builds, and runs — so the cross-platform
break is fixed for a fresh modern-toolchain clone. Docs (TECH_FOUNDATIONS § Building) updated.

**CI regression guard.** `build.yml`'s `linux` job is now a **GCC 13 + GCC 14 matrix**
(`fail-fast: false`, compiler-keyed dep cache, `$CXX` threaded through the headless-harness step)
so the exact break we fixed can't silently regress — the default runner compiler is GCC 13, which
wouldn't have caught it.

**CI first-run — green.** First-ever Actions run of `build.yml` on `main` passed all three jobs:
Linux **g++-14** (full app incl. sol2 v3.5.0 + headless harnesses — confirms the GCC-14 fix in CI,
not just on the laptop), Linux **g++-13**, and the first-ever **Windows** build (confirms the
C-language + sol2 changes didn't break MSVC). Cross-platform build is verified on both OSes.

**Headless visual-verify (BL-057 step 4 — the offscreen spike).** Confirmed the visual tier needs
no monitor: `SDL_RenderReadPixels` (the capture path) is renderer-agnostic, and `xvfb-run` gives a
virtual display, so `xvfb-run ./ProjectIo --verify <script>` works unchanged. Added an **advisory**
`visual-verify` CI job (`continue-on-error`, `SDL_RENDER_DRIVER=software` for deterministic
GPU-independent output) that builds, runs all `scripts/verify/*.lua` under Xvfb, and uploads the
captures + `screenshots/diff/` images as artifacts — *not* a gate yet. The real open question is
golden portability: the committed goldens were blessed on a GPU renderer and may diff against the
software rasteriser beyond the 0.5 % tolerance; the advisory artifacts let us see the output before
re-blessing in CI and promoting to a hard gate.

**Spike result (advisory run on `main`).** The pipeline works end-to-end headless: all 24 scripts
produced captures under Xvfb + software renderer, no crash, 105 artifacts uploaded. Golden compare:
**4 clean, 20 diffed** against the GPU-blessed goldens — and the diffs are *systematic* (a consistent
~7.7 % on zoom captures, ~26–45 % on full-canvas), i.e. a deterministic GPU-vs-software rasteriser
delta, not flakiness. Confirms the fix is to make the **software renderer the reference of record**.

**Gate prep (this session).** Updated the `verifier-visual` skill to mandate `SDL_RENDER_DRIVER=software`
(+ the Xvfb invocation for headless), and added `scripts/verify/bless_all.sh` (forces software + Xvfb,
blesses every script). **Pending Ben:** re-bless the goldens locally via `bless_all.sh` and commit
them; then the CI job's `continue-on-error` is dropped to make visual-verify a hard gate — the last
BL-057 item. Held the gate flip until the re-blessed goldens land (else CI would gate on stale ones).

---

## Session — Deliver BL-053 country generation (2026-06-29)

**Goal.** Promote + deliver one of the five just-designed items. Of the five, four are GUI-side
(app.cpp / ui_state.hpp / canvas+ledger surfaces) and cannot be compiled in this sandbox — the
SDL3/ImGui/sol2 FetchContent clones are GitHub-egress-blocked here (same wall as BL-057's GUI
half). **BL-053 is the exception** — it lives entirely in the SDL/Lua-free `src/world/` headless
tier, so it can be built *and* verified here. Delivered it; the GUI four stay promotable for the
Linux box / CI.

**What landed (`nation_generation.cpp` + header + hard_coded_world + world_audit):**
- **Pass 1b — growth weights.** Each seed gets a skewed weight (cube of a uniform draw → most
  small, few large); the BFS step cost is divided by the owner's weight, so high-weight seeds
  claim more. Turns near-uniform Voronoi cells into strongly varied sizes.
- **Pass 2c — light "in history" merges.** Over-seed, then absorb the smallest nations into their
  largest cardinally-adjacent neighbour until `merge_to` remain; compact indices. Deterministic
  (no RNG), giving irregular grown borders. New `merge_to` field on `nation_params`.
- **Kepler config:** 18 seeds, min_sep 5, merge_to 14.
- **Acceptance:** world_audit BL-053 R1 (count in [12,16]) + R2 (max ≥ 3× min). Observed: **14
  nations, sizes 24..2150 tiles** (~90× spread — a few great powers, many small states).

**Isolation / determinism.** The econ harnesses build their own small worlds, and substrate is
injected as a per-body sum over nations (invariant under re-partition), so only world_audit is
affected. Full headless suite 7/7 green — no regression. Design propagated to NATION_GENERATION.md;
BL-053 marked complete; REFINED group cleared.

---

## Session — Design pass: five design-owed items (2026-06-29)

**Goal.** Design depth only (DELIVERY § depth verbs): settle the open questions for the
five active `design-owed` items via a Q&A with the developer, write the settled prose into
`backlog.json`, flip each `design-owed → designed`. No tasks, no code, no authority-doc
edits (those land with the work).

**Decisions (developer's calls, via two Q&A rounds):**
- **BL-061 app-driven mouse** — minimal deterministic capture. A `{x,y,active}` cursor-source
  struct on `ui_state` that canvases read instead of the raw IO mouse; live app feeds the real
  mouse, `--verify` leaves it inactive (hover off) unless a script sets `verify.mouse` /
  `verify.hover_tile`. No scripted click/drag this pass.
- **BL-062 hotkeys** — dev-fixed central action→binding keymap, `canvas_command` subsumed into
  it, F1 cheat-sheet overlay generated from the keymap. No player rebinding UI yet.
- **BL-020 tooltips** — lens-contextual "why" on the BL-060 hover-card: name + ≤2 lens-relevant
  stats + a why-line; data stays in ledgers. Deliverable is the content contract + 2-3 exemplars;
  the exhaustive sweep is execution.
- **BL-053 country generation** — clustered seeds, strongly varied sizes (~12-16 on Kepler:
  a few great powers, several mid, many small), plus a light merge/fragment post-pass for grown
  borders. Not a historical sim. Cleared the dangling `requires: BL-052` (no such item) and
  superseded the old ~45-country direction (newest-dated wins).
- **BL-063 tester graphs** — targeted to the key economic surfaces (Market Ledger price +
  supply/demand trends; economy-panel running-balance / income-vs-expenditure) via one reusable
  ImGui plot helper + a colourblind-safe palette; bounded ring-buffer history (coordinate with the
  v0.0.9 data-creep audit).

**Result.** All five flipped to `designed` (promote-ready); BL-020/BL-053 legacy BACKLOG.md
bodies tombstoned. The active backlog is now design-complete bar finishing BL-057 off-sandbox.

---

## Session — BL-057 + BL-040 Batch Delivery (2026-06-28)

**Goal.** Deliver the top two implementation-ready backlog items: BL-057 (cross-platform
build) and BL-040 (full-set resource generation). Run from a native Linux remote
environment with cmake 3.28 + g++ 13 — the first time the project has been compiled on
Linux, which the work depended on.

**Status: code complete, headless suite green (7/7). BL-040 complete; BL-057 partial
(GUI/CI sign-off owed off-sandbox).**

**BL-040 — full raw-set deposit authoring (complete).**
- `build_rarity_profile(seed)` in `tile_generation.cpp` builds a per-body, seeded
  per-resource rarity scalar [0,1], raw-tier only. The v0.0.4 seven-resource subset is
  pinned at 1.0 so its hand-calibrated authoring is left untouched; the six additions
  (silica, coal, iron-nickel ore, copper ore, rare-earth ore, PGM) carry fixed
  base rarities ordered by base price + small seeded jitter.
- A `put_rare` block authors the additions per terrain affinity (RESOURCES.md Tier 1),
  the scalar gating presence (frequency) and scaling magnitude.
- **Determinism decision.** The additions draw from an *independent* per-tile rng stream
  (`rare_rng`), never the shared `tile_rng`. Adding draws to `tile_rng` would shift every
  downstream draw and `derive_environment`, silently changing the calibrated economy. With
  the separate stream, `econ_harness`/`econ_stability` are bit-identical.
- `world_audit` gained the BL-040 distribution audit (R1: all six additions authored
  somewhere; R2: PGM strictly rarer than copper). Observed: silica 7431, copper 5979, coal
  3776, rare-earth 3767, iron-nickel 116, PGM 57 tiles — metallic pair scarce because
  metallic terrain only exists on the Pallas asteroid (correct for Era 0).
- Design propagated to RESOURCES.md and TILE_GENERATION.md; legacy BACKLOG.md body
  tombstoned. Brought forward from its v0.2 schedule at user request.

**BL-057 — cross-platform build (partial; the real blocker fixed).**
- **The genuinely new finding:** the prior design's claim that "the source is already
  Linux-clean" was wrong. Three `nation_component` fields were named identically to their
  enum types (`ideology ideology`, `expansionism expansionism`, `economic_focus
  economic_focus`) — ill-formed per `[basic.scope.class]`, rejected by GCC
  (`-Wchanges-meaning`) though MSVC accepts it. Never caught because the code had never
  built on Linux. Renamed to `politics`/`posture`/`focus` (matching the tile_component
  convention) across the 8 reference sites; serialization layout unaffected (no reorder/retype).
- Fixed a stale headless harness: `construction_harness` R4 asserted `insufficient_funds`
  but BL-043's Port-coastal rule makes a non-coastal Port return `invalid_tile` first. R4
  now uses a fresh valid tile + processing_facility so it tests the funds path it intends.
- **Result:** all seven `tools/verify/*.cpp` harnesses build and pass under g++ — the CI
  guard's headless tier is verified locally for the first time.
- Refreshed `build.yml`'s stale "unverified" note; documented the Linux/headless build
  recipe + the enum-naming gotcha in TECH_FOUNDATIONS.md (font fix was already present).
- **Owed (cannot be done from this sandbox — GitHub clones for FetchContent are
  403-blocked by egress policy):** the full GUI/CMake app build on Linux, visual-verify of
  the bundled font on a real window, and the first GitHub Actions run of `build.yml`. These
  land on the Linux dev box / in CI. BL-057 left `designed`, progress recorded in its item.

**Env note.** This is a native Linux git clone, not the Cowork Windows-bridge shell, so the
BL-058 git-write restriction does not apply; `.gitattributes` keeps line endings clean.

---

## Session — v0.0.6 Improved Core-Loop Batch Delivery (2026-06-17)

**Goal.** Deliver all six v0.0.6 backlog items as a Batch Delivery: BL-050 (saturated
substrate), BL-037 (order book), BL-056 (bankruptcy harness), BL-036 (multiple market
centres), BL-025 (multi-market ledger dashboard), BL-035 (warm-start surface). BL-057
(cross-platform build) deferred — Linux dev box not yet set up.

**Status: Complete — 27/27 econ_harness tests PASS, 24/24 visual goldens PASS.**

**Commits (3):**
- `6691c7a` — BL-050 integration + BL-036: substrate injection wired into `clear_markets`;
  population-centre ordering fixed (must precede `generate_nations` for Pass 6 to reference
  centres); multiple markets seeded from scale≥3 population centres.
- `a980079` — BL-037: `clear_markets` fully restructured — auto path (pool surplus /
  auto-buys) bypasses the order book and clears at `resolve_price` directly; explicit sell /
  buy orders are matched against each other; unmatched player sells clear at
  `max(ref_price, floor_price)` (market as buyer of last resort).
- `8cf72a7` — BL-025 + BL-035: `market_ledger.cpp` rewritten — body selector, dashboard
  table (all markets: supply/demand/turnover), click-to-select → resource detail; 11 golden
  images blessed.
- `b4e2b45` — BL-056: `econ_bankruptcy.cpp` harness (Wave 1 sub-agent merge).

**Key in-session decisions:**
- **Auto-surplus VWAP bypass.** Auto-surplus entries (floor_price=0) entering the order book
  caused VWAP to collapse to 0 when supply dwarfed demand, dragging EMA-smoothed prices to
  zero each tick. Fix: separate the auto path entirely — auto entries clear at `ref_price =
  resolve_price(...)` which already embeds the EMA. Using them as VWAP input would apply EMA
  twice (double-smoothing). The order book now sees only explicit player orders.
- **Substrate density static for prototype.** Growth model deferred; density is generated
  once at world creation and does not change. Substrate background supply/demand arrays are
  injected into markets each tick via `inject_substrate_demand(w)` called inside
  `clear_markets` after the per-tick zero-reset, before the order-book pass.
- **Market-centre anchor via `centre_tile`.** Each market carries a `centre_tile` field
  pointing to the population-centre tile it was seeded from. `market_for_tile` / `nearest_market`
  use this for catchment routing. The fallback (no scale≥3 centre) seeds one unanchored market.

**Open items returned to backlog:** none — all tasks completed or were not promoted.

---

## Session — Lens & Legibility Batch Delivery (2026-06-17)

**Goal.** Deliver the full lens strip (bar the Market slot, gated on multi-market
seeding) + the legibility lenses, as a Batch Delivery: BL-013, BL-052, BL-019,
BL-017, BL-009, BL-018, plus BL-012 as a design-only closer.

**Status: Complete — 22/22 requirements met across 7 groups** (lens-strip R1–R3,
faction-to-country R1–R3, resource-density R1–R3, population-opportunity R1–R3,
production-output R1–R3, scarcity-market R1–R3, meta-per-lens-upper-rungs R1). All
`visual` rows verified against blessed goldens, deterministic across 3 runs.

**Changes (all main-session, sequential — single-file-concentrated render refactor):**
- **BL-013 strip:** curated single-select `modes[8]` in order Corp, Country,
  Resource, Market, Population, Opportunity, Production, Scarcity (Supply off-strip);
  default lens → Corporation (`ui_state.hpp` + `app.cpp`); re-click clears to none.
- **BL-052 Faction→Country:** `overlay_mode::faction`→`country`, `icons::faction`→
  `country`, labels "Countries"/"Country" ("faction" kept as a verify alias);
  disentangled `palette::faction_colour`→`corp_colour` (+ `faction_slot_count`→
  `corp_slot_count`); `nation_colour` untouched.
- **BL-019 Resource:** reworked to a flat uniform fill over the contiguous deposit
  of the selected good; always single-resource (removed highest-value mode +
  `resource_lens_single`).
- **BL-017 Opportunity:** new `overlay_mode::opportunity` — per-tile best-valid-
  building net margin (diverging red→green, per-body normalised); Population
  habitability tint unchanged.
- **BL-009 Production:** new `overlay_mode::production` — per producing tile,
  Σ(output qty × resolved price) from the `economy_report` + market prices, log-scaled
  vs the body producing-tile mean; idle/exhausted read cold. Canvas signature gained
  `const recipe_registry&` + `const economy_report&`.
- **BL-018 Scarcity:** reworked from deposit-based per-tile to per-market shortfall
  (`max(0, demand−supply)`) blocks via `market_for_tile`, normalised across the body's
  markets.
- **BL-012 (design closer):** LENSES.md rung-applicability table + per-lens
  Solar/Circumplanetary notes for all eight strip lenses.
- **Render determinism fix (enabling):** the Planetary draw loop now iterates tiles
  in sorted-id order. `w.tiles` is a `std::unordered_map` whose per-process iteration
  order made full-body golden captures flake ~1–2% on antialiased hex edges; sorting
  makes captures reproducible (verified 0 fails across 3 independent suite runs).
- **Docs:** LENSES.md (all six lenses + rung table + selection-routing table),
  ICONS.md (country/opportunity/production glyphs, corp_colour), GLOSSARY.md (Country
  term). ICONS/GLOSSARY propagated by parallel sub-agents.

**In-session decisions (design-direction Q&A):**
- **No code fan-out.** Every item converged on `body_surface_canvas.cpp` + the
  shared strip/enum/colour files; per DELIVERY.md ("passes in one file are
  sequential, hotspots stay in the main session") the worktree-merge cost exceeded
  the win. Fan-out was used only for the disjoint doc-propagation wave.
- **Resource flat fill:** the settled "8-connected flood fill" is visually identical
  to a per-tile `deposit>0` threshold under a uniform fill, so no flood-fill pass was
  built (recorded as a deliberate simplification).
- **Opportunity margin** is a first-cut estimate (single workforce, no contention, no
  build-cost amortisation) — refine when those models land.
- **Production intensity** uses a geometric-mean-relative diverging scale; a body of
  similar producers reads near-neutral (honest — little spread to show).
- **Scarcity** with one market per body reads as a single body-wide block (honest to
  the catchment-as-unit structure); spatial variation arrives with BL-036.
- **GLOSSARY Faction vs Country:** the broad "Faction" actor/sentiment concept was
  kept; only the *lens* (which showed nations) became Country. Flagged as the one
  non-mechanical naming call.
- **Golden ripple:** the default-lens change (Corporation) and the icon/strip change
  restaled most canvas-bearing goldens; the whole suite was re-blessed against the
  now-deterministic frames.

**Left open:** Market-boundary lens (BL-015) + multi-market ledger (BL-025) still
gated on BL-036 (market-centre seeding → population layer). Per-body Circumplanetary
badges for Production/Scarcity noted in the LENSES.md rung table as owed.

---

## Session — Supply layer R8+R9 closure (2026-06-17)

**Goal.** Close the two remaining active requirements on the supply-layer group: R8 (headless multi-tick price convergence) and R9 (visual golden).

**Changes:**
- **R8 (headless):** Extended `tools/verify/supply_advance.cpp` with a two-body price-convergence scenario. Body A holds a large iron surplus → price stays near floor. Body B has standing demand but no supply → opens at 3.625× base. Eight simulated convoy deliveries via `credit_arrived_convoys` + `clear_markets` bring it to 0.923×. Added `market_clearing.cpp` to the harness build. All 23 assertions PASS.
- **R9 (visual):** Added `verify.seed_convoy(src, dst, resource, qty[, progress])` to the verify API in `app.cpp`; auto-creates a stub market on any body that lacks one. Authored `scripts/verify/supply_lens.lua` seeding a Kepler→Pallas in-flight convoy; blessed 3 goldens — `supply_lens_solar_route`, `supply_lens_circum_badges`, `supply_lens_planetary_glyphs` — all at 0.00% diff.

**Outcome.** Supply-layer group (BL-039/038/045) fully complete, R1–R9. REFINED.md cleared.

**In-session decisions:**
- `seed_convoy` routes Kepler → Pallas rather than Kepler → Selene. Selene is Kepler's moon and its Solar-canvas position overlaps Kepler, making the route line invisible. Pallas is a distant asteroid with a clearly separated position.
- Pallas has no authored market; `seed_convoy` creates a minimal stub in place rather than polluting world gen with a verify-only market.

---

## Session — Backlog dependency schema v2 (2026-06-17)

**Goal.** Introduce a first-class, ID-based dependency field to `backlog.json`.

**Changes:**
- `waits_on` (short_name list, v1) → `requires` (BL-XXX id list, v2). All 33 existing items converted.
- `blocked_on` retained as a field for truly-external prerequisites; all existing string refs resolved to BL-XXX ids and moved to `requires`, leaving `blocked_on: []` on all current items.
- **New stub items** created for concepts that were referenced as blockers but had no backlog entry: BL-059 `SELECTABLE_MARKERS` (gates BL-031 canvas hit-testing) and BL-060 `HOVER_CARD_PRIMITIVE` (gates BL-020 tooltip simplification). IDs renumbered from original BL-056/057 to avoid collision with items created in the same session (BL-056 ECONOMY_BANKRUPTCY_TEST, BL-057 CROSS_PLATFORM_BUILD, BL-058 GIT_BRIDGE_HYGIENE). Other `blocked_on` strings resolved to existing items: `SUPPLY_LAYER` → BL-039, `ORDER_BOOK` → BL-037, `POPULATION_LAYER` → BL-046, `CANVAS_HIT_TESTING` → BL-031.
- **Implied dependencies added:** BL-010 → BL-043, BL-012 → BL-013, BL-016 → BL-013, BL-044 → BL-043, BL-050 → BL-053, BL-053 → BL-052.
- Schema version bumped to `backlog/io-v2`. `DELIVERY.md` updated to reference both `requires` and `blocked_on`.

**In-session decisions:**
- `SUPPLY_LAYER` (used as blocker in 4 items) resolved to BL-039 SUPPLY_CONVOYS — the supply convoys build is the gating deliverable for Layer 5, not a separate stub.
- `ORDER_BOOK` (BL-014) resolved to BL-037 PREFERENTIAL_PURCHASING, which encompasses the order book mechanism.
- `POPULATION_LAYER` (BL-036) resolved to BL-046 LAYER4_INDEX — the population umbrella.
- `CANVAS_HIT_TESTING` in BL-032's blocked_on resolved to BL-031 (already in backlog, was an inconsistency).

---

## Session — Supply Layer + BL-055 nav slot sync (2026-06-16)

**Goal.** Deliver the supply routing layer (BL-039, folds BL-038 + BL-045) and the nav-slot open/close colour sync (BL-055).

**Changes:**

BL-055 (Light): `nav_pane.cpp` — `close_all_panels()` enforces exclusive-open slot behaviour; each live slot saves `was_open`, closes all, then toggles to `!was_open`. R1 complete.

BL-039 / BL-038 / BL-045:
- `convoy_component` + `convoy_mode` enum (`land/sea/air/space`) in `components.hpp`; `building_type::launchpad = 4` added.
- `world.convoys` as `std::vector<convoy_component>`.
- `supply_system.{hpp,cpp}`: `advance_convoys`, `credit_arrived_convoys`, `dispatch_convoys` (space-mode gated by launchpad on source body; logistics cost debited at dispatch; iterates markets for shortfalls).
- `recipe_registry`: `logistics_cost(convoy_mode)` accessor + `set_logistics_cost()` setter; `m_building_econ` expanded to 5 slots for the launchpad; logistics table loaded from `economy.lua`.
- `economy.lua`: `logistics.base_cost_per_unit_distance` table (land=0.02, sea=0.05, air=0.15, space=1.00).
- `app.cpp`: per-tick pipeline wired — dispatch → advance → production → markets → budget → credit.
- Supply lens canvas passes: Solar inter-body route lines, Circumplanetary throughput count badges, Planetary per-tile convoy glyphs; `icons::convoy` (open-chevron glyph).
- `tools/verify/supply_advance.cpp`: 21/21 PASS covering R1–R7.

**Outcome.** R1–R7 complete; R8 (multi-tick price convergence) and R9 (visual golden) deferred — both required an active-convoy world state that didn't exist until the following session.

**In-session decisions:**
- Auto-dispatch iterates markets for shortfalls rather than scanning corp pools directly — avoids double-pass.
- Space-mode requires a launchpad on the source body; land-mode is ungated. No launchpads exist in world gen yet, so auto-dispatch does not fire in the cold world.

---

## Session — Design Q&A: owed items sweep (2026-06-16)

**Goal.** Work through all design-owed (~) backlog items that could be settled via Q&A, without writing code. 15 items settled and flipped to `designed` (✓); 1 item (BL-053) updated with partial direction but kept owed; BL-050 open notes partly settled.

**Items settled this session (flipped to `designed`):**

- **BL-009 Production/Output lens** — intensity: log scale relative to market average (tiles above average read hot, below cool); idle/exhausted buildings: cold (zero, same as unbuilt terrain).
- **BL-010 Placement-suitability surface** — trigger: tile selection only (not armed-build); colour: affine tiles coloured, invalid tiles dark overlay, valid-but-not-affine uncoloured; canvas state tied to `selected_tile`, not `overlay_mode`.
- **BL-013 Lens strip ordering & rename** — strip order: Corp → Country → Resource → Market → Population → Opportunity → Production → Scarcity; single-select with null state; defaults to Corp at campaign start; price readout is menu-only (Market Ledger).
- **BL-015 Market lens → boundary UI** — render: filled tint per catchment (distinct colour per market, like Corporation lens); price readout: Market Ledger only (open ledger, pick resource, see per-tile price overlay).
- **BL-017 Replace Habitability with Population / Opportunity** — both as separate strip lenses; Opportunity metric: estimated net margin (best valid building's net output minus input costs, without regard to current build state or logistics).
- **BL-018 Scarcity lens blur** — render: chunky per-market blocks (solid tint per catchment, not per-tile gradient); signal: supply shortfall (demand minus supply last tick), not price.
- **BL-019 Resource-density lens** — render: flat fill over 8-connected contiguous deposit shape; no per-tile level gradient.
- **BL-025 Multiple markets in ledger** — default: dashboard view (all markets on the body); selection-driven detail per market. Flagged as vital — must ship with multi-market seeding.
- **BL-035 Economy warm-start readout** — surfaces in the Market Ledger dashboard as opening supply/demand health per resource; settle-tick count owed at promotion.
- **BL-036 Seed multiple market centres** — seeded from population centres above a threshold density; implementation gated on population generation pass.
- **BL-041 Habitability → workforce curve** — linear 0→0.6 = proportionally reduced fraction; at/above 0.6 = 100% max workforce; over-100% (tech-driven) deferred.
- **BL-043 Building rules** — four constraint types active (terrain, body cap, per-tile slot cap, adjacency); full Era 0 placement table approved; all buildings uncapped except Launchpad (1 per body); Port coast-adjacency restriction settled; open note to revise after playtesting.
- **BL-044 Construction pricing** — two-part cost (resource + budget); full Era 0 cost table approved (high tier across the board); open note to revise all costs via playtesting.
- **BL-052 Rename Faction → Country** — full disentanglement: Country for all nation/territory usages; `faction_colour()` → `corp_colour()` (not a blanket replace).
- **BL-056 Economy bankruptcy test** — bankruptcy = unable to cover maintenance (interest) at balance ≤ -5 × start_money; fixed starting conditions; ceiling tick configurable; open note for a debt-interest system when balance goes negative.

**Items partially settled / updated but kept design-owed:**

- **BL-050 Saturated substrate** — generation home settled (population sub-pass); displacement seam settled (vastly higher workforce cost to outbid substrate); dynamic growth model and slot/capacity model still open; UI clarity note opened (must visually distinguish substrate-occupied tiles).
- **BL-053 Country generation** — direction: ~45 countries (Earth-like density); size distribution open (tune visually after generation); "generated in history" model still owed.

**Items not covered (still design-owed):**

- BL-012 (meta per-lens Solar/Circumplanetary sweep), BL-020 (tooltip simplification sweep) — no Q&A this session.
- BL-011, BL-014, BL-016, BL-051, BL-054 — F priority, deferred.

**In-session decisions:**
- All buildings uncapped at body level except Launchpad (max 1) — cost is the primary constraint; arbitrary count limits rejected.
- All building costs calibrated to "high" tier deliberately; playtest note to revise. This makes the prototype harder than easy by design.
- BL-017: both Population and Opportunity are separate lenses (not a single hybrid slot).
- BL-025: dashboard-first with selection-driven detail (not tabs or dropdown selector).
- BL-018: per-market solid block render preferred over smooth gradients — honest to market structure.
- Scarcity signal is supply shortfall (volume), not price ratio — price-independent scarcity read.

---

## Session — v0.0.6 Batch Delivery (2026-06-16)

**Goal.** Batch-deliver all designed (✓), unblocked backlog items for v0.0.6 using parallel sub-agents in worktrees. 20 items promoted and delivered across 6 waves; build green at every integration point.

**Waves and outcomes:**

- **Wave A (main session, doc-only).** BL-033 inline light-mode review — confirmed clean, removed. BL-034 propagated the v0.1.0 design-pass into authority docs: `LAYOUT.md` ledger-family conventions block, `SYSTEMS.md` Supply section settled, new `docs/economy/SUPPLY.md` Layer-5 authority doc created.

- **Wave B (4 parallel agents, 1 serial follow-on).** B1: redrawn extraction-site (faceted ore-chunk polygon) and unit/convoy (open V chevron), outline convention applied to all filled markers (BL-002+003). B2: corp lens player-tile border → `palette::selection`, hover-card `draw_hover_card` dispatcher, rung-relative distance reference fixed to canvas rung, tile ledger defaults to `active_body` (BL-001/006/005/024). B3: non-linear speed curve (¼×/½×/1×/4×/16×), progress bar text suppressed (BL-007). B4 (after B1 merge): icon usage audit — all 13 call sites fully conformant, null commit (BL-004). All merged clean.

- **Wave C (3 parallel agents).** C1: economy panel audit — already conformant, null delta (BL-026). C2: population static MVP — `land_use_component` + `population_centre_component`, `population_generation.{hpp,cpp}` with seeded clustering, `agricultural_produce` demand stub in `economy_system.cpp` (BL-047). C3: building management — `workforce_target` + `decommissioned` + `active_recipe_index` on `building_component`, workforce scalar and labour/material cost split wired in economy + budget system, recipe control API, live management controls in construction panel (BL-049). C2+C3 shared-file conflict (`components.hpp`, `economy_system.cpp`) resolved by merge order. BL-047 wired into `hard_coded_world.cpp` (main session).

- **Wave D (4 parallel agents).** D1: Market Ledger — supply/demand/price/net table per resource, body selector (BL-027). D2: Balance Ledger — treasury, starting capital, net, assets (BL-028). D3: Construction Ledger refit — queue overview table prepended, management controls from C3 retained (BL-029). D4: Corp Overview Dashboard — per-corp table, player row tinted, row-click sets selection (BL-022). D3 had a merge conflict with C3 on `construction_panel.cpp` — resolved by keeping C3's live management controls, D3's queue section already present in file. All four new panels wired into `ui_state.hpp` + `nav_pane.cpp` + `app.cpp` (main session).

- **Wave E (main session).** BL-042: workforce supply now derived from population centres (scale → labour-force table, apportioned by building-count ratio); wage scaling by body mean habitability added to `budget_system.cpp`. BL-021: nav-pane rewired to the curated 9-slot order from `MENU.md` (Corp/Budget/Workforce/Research/Market/Construction/Strategy/Diplomacy/History); four live slots, five placeholder slots with tooltips. BL-030: `focus_on_entity` extended — corporation entity → open corp panel; nation → no-op stub.

- **Wave F (main session).** BL-048: body habitability aggregate computed from population-centre tile weights and stored in `economy_report.body_habitability`; habitability efficiency multiplier applied to `workforce_contention` (>0.6 → 1×, linear to 0.5× at 0). Population growth step: per-tick accumulator incremented when habitability ≥ 0.5 and food supply ≥ 50% met; levels up at tier thresholds (200/500/1500/5000 ticks). `growth_accumulator` field added to `population_centre_component`.

**In-session decisions:**
- BL-026 (economy panel refit): already conformant — null delta, no code change required.
- BL-023 (nav-rail ordering rule): confirmed stale (design already in MENU.md), removed from backlog.
- BL-033 (lens doc review): cleared inline (light mode) — LENSES.md, ICONS.md, SYSTEMS.md all consistent.
- Construction panel merge conflict: kept C3 (BL-049) live management controls; D3's queue section was already present in the merged file.
- Workforce supply derivation (BL-042): implemented in-engine with a hardcoded scale→labour table rather than reading from Lua, matching the headless-safe constraint (economy_system.cpp is harness-buildable).
- Population growth ticks added to BL-048 despite the `growth_accumulator` field not being in the original component design — added inline rather than creating a separate component.

**Open after this session:** remaining backlog items (32 items); BL-048 growth needs a proper Lua-driven rate table (currently hardcoded thresholds); the four placeholder nav slots (Workforce/Research/Corp Strategy/Diplomacy/History) need their ledger implementations.

---

> ## Handoff — Session 3: the UI-polish batch
>
> **Goal.** Clear the cheap, unblocked UI polish — per ROADMAP § Near-term publish plan
> → **Session 3 (UI polish, serial, main session)**. A Batch Publish, **strictly serial**:
> every Brief collides on `icons.{hpp,cpp}` and/or the shared UI files, so there is **no
> fan-out**. Mostly already-designed (`✓`) Briefs in OPENS § Canvas.
>
> **Briefs, in order (OPENS § Canvas / § Selection unless noted):**
> 1. **[C2 ✓] Icon silhouette collisions & contract mismatch** + **[C2 ✓] Icon outline &
>    colour conventions** — *land together*, same files (`icons.{hpp,cpp}`, `docs/ui/ICONS.md`):
>    redraw extraction-site (faceted ore, off the gem-diamond pip) and unit/convoy (open chevron);
>    bring `icons.cpp` into line with the shared outline/colour-source conventions.
> 2. **[C2 ✓] Verify icon usage is consistent** — audit every `ui::icons::*` call site against
>    ICONS.md; fix cheap drifts, promote larger ones. Runs *after* (1) so it audits the redrawn set.
> 3. **[C2 ✓] Reference distances are rung-relative** — `entity_summary.cpp`: read the distance
>    reference from the current rung (star at Solar, parent at Circumplanetary) rather than hard-coding the star.
> 4. **Time-speed curve / clarify time-control** (ROADMAP names these — confirm their exact Brief
>    text/location in OPENS before promoting; may be a § Time / § Canvas pair).
> 5. **[C2 ✓] Tile-ledger body-selector default** — `tile_inspector.cpp`: default the selector to
>    the in-view body (`ui_state.active_body` / `circumplanetary_anchor`), not the lowest id.
> 6. **[C1 ✓] Corporation-lens player border recolour** — `body_surface_canvas.cpp` (hotspot): the
>    player border is `faction_colour(0)` over a `faction_colour(0)` fill (invisible); recolour for
>    contrast (e.g. `palette::selection`). Update LENSES.md § Corporation lens. **Re-bless the
>    `corporation_lens` golden** after.
>
> **Hotspot / why serial.** `icons.{hpp,cpp}` (every icon Brief) and `body_surface_canvas.cpp`
> (border recolour) are single-writer in the main session. No disjoint scopes → no sub-agents.
>
> **Verification.** Each `src/`-changing Brief: brief-spanning **visual** requirement first, then
> author/extend a `scripts/verify/*.lua` and bless a golden (F3 diffing is live). The corp /
> resource / market goldens are existing references; (6) changes the corp golden, so re-bless it.
>
> **New process this session (just adopted — apply them):**
> - **Progress markers.** Emit a coarse `%` line (`0 … 100`, steps of 5) in your text output at
>   each checkpoint — estimate once after the collision map, weight verification heavily. See
>   CLAUDE.md § Publication pipeline → Progress reporting; OPENS § Publish.
> - **Brief timestamping.** Timestamp every *new* Brief; newest-dated statement is canon on
>   conflict; no retroactive refactor. See OPENS § Design state → Brief timestamping & precedence.
>
> **Before promoting:** ROADMAP flags two entries (C1 nav-rail ordering, A3 design-pass
> propagation) as possibly **stale** (already settled into their authority docs) — check and
> *remove* rather than work them if so.
>
> **Not in scope (gated):** v0.0.6 ledger family / population / building management; the selection
> trio; Supply / Layer 5; the design-owed `~` Briefs — **[B4 ~] substrate generation** and the new
> **[B3 ~] Multiple markets per body (tile-centred)** (§ Trade) — which need a *design* pass, not a
> publish. After Session 3, ROADMAP § Session 4 is the A3 economy-panel refit (alone).
>
> **State at handoff:** branch `v0.0.5`; tree clean; TASKS empty; no pending `⟳` notes. Last
> commits: lens batch (`030934c`, `24d8013`) + close-out + this process pass.

---

## 2026-06-16 — Process refit: JSON backlog + vocabulary alignment (branch v0.0.6)

- **Mode:** Full (doc/process only — no `src/`, CMake, or Lua touched; build unaffected).
- **What changed:**
  - **Backlog → JSON.** `OPENS.md` split into `backlog.json` (canonical metadata index — 54 items, with `status`/`priority`/`difficulty`/`waits_on`/`files`/`design`) plus `BACKLOG.md` (design prose, keyed by item). `TASKS.md` → `REFINED.md`. New `DELIVERY.md` (method authority) and `.claude/rules/io-standing-rules.md` (always-on invariants). New `REVIEW_LOG.md` (code-review gate).
  - **Vocabulary aligned** (full; glyphs kept): Brief → item, Publish → Deliver, Batch Publish → Batch Delivery, OPENS → backlog, TASKS → REFINED. Glyphs `✓`/`~` retained but bound 1:1 to the JSON `status` (`designed`/`design-owed`), JSON authoritative. All live cross-references swept; DEVLOG history and the archived REFINED publish-sets left period-accurate.
  - **Sub-agent model:** worktrees are now the primary isolation mechanism; the collision map is a *splitting heuristic*, not a gate (`DELIVERY.md` § Sub-agents & worktrees).
  - **settings.json** slimmed to broad prefix allows + a `deny` net (the split is confirmed).
- **Decisions:**
  - **markdown/JSON policy:** new items are JSON-native (prose in the `design` field); legacy items keep `BACKLOG.md` bodies (sentinel `@BACKLOG.md`), deleted on promotion — so `BACKLOG.md` only drains and is eventually removed.
  - **No review-mode approval Q&A.** Considered (Fulcrum has one) and rejected: Fulcrum needs higher-up sign-off, whereas Io is solo and authoritative — the **backlog is the review surface**, sourced from the roadmap, and we don't leave ambiguities. Rule 0a remains the only sanctioned Q&A (for *unscoped ideas*, not for reviewing settled work).
  - `requirements.json` `brief` field key intentionally **not** renamed (data migration deferred); bridged in prose.
- **Note:** the Session-3 handoff block at the top of this log predates the rename — read its "Brief / OPENS / Publish" as "item / backlog / Deliver".
- **Why:** adopt the queryable-JSON backlog and lighter vocabulary observed in Project-Fulcrum's process, kept tighter for Io's solo model.

---

## 2026-06-16 — Interim: lens-ideas Q&A + multiple market centres (branch v0.0.5)

An interim design + publish session between the lens batch and Session 3.

**Lens-ideas Q&A.** Brainstormed *what else is informative as a map lens*; six new Briefs
landed in OPENS § Canvas (commit `5204fac`): Production/Output (intensity = sell value of all
outputs), Habitability/Population, Placement-suitability (a surface on *tile selection*, not a
strip lens), Ambient/Scarcity (single-resource body-wide heatmap, access left open),
Reach/Logistics (`F4`, needs scoping), and a meta sweep on per-lens Solar/Circumplanetary
representation. All `~`.

**Multiple market centres (published — guess design, revise after population).** Promoted the
`[B3 ~]` multiple-markets-per-body Brief at the user's request, guessing the implementation:
- `market_component` gains `centre_tile`; a body may carry **several** markets.
- **Catchment = nearest centre** — `market_for_tile` (`market_clearing.{hpp,cpp}`) routes a tile
  to the market whose centre is nearest by grid distance; a body with one market routes there
  unconditionally. Clearing routes each corp's body-aggregate supply/demand to the market nearest
  its representative holding (`market_for_corp_on_body`).
- **Behaviour-preserving:** the live world still seeds **one** market per body (`centre_tile`
  null), so all existing assertions hold; the multi-market path is exercised + verified by four
  new `econ_harness` cases (MM.1–MM.4, all pass; full harness green).
- Propagated to `docs/SYSTEMS.md` § Trade and `docs/ui/LENSES.md` § Market lens; the big Brief is
  replaced by a residual `[B3 ~]` "seed multiple market centres from capitals/population" (deferred
  to the population layer) plus noted follow-ups (finer per-building split; inter-market convoys).

**Population + Scarcity lens batch (published).** Batch-published two of the new lens Briefs as
Planetary render passes — strictly serial (both collide on `ui_state.hpp` enum, `overlay.cpp`,
`body_surface_canvas.cpp`, `icons.{hpp,cpp}`, so no fan-out):
- **Population lens** (`overlay_mode::population`) — per-tile **habitability** tint (dark→liveable
  green, `0.15 + 0.7·h`); figure glyph; low→high key. Reads `tile.habitability` directly (population
  *density* deferred with the population layer).
- **Scarcity lens** (`overlay_mode::scarcity`) — single-resource **translucent** heatmap, scarcity
  `= 1 − deposit/body-max` composited hot at `0.5·scarcity`; hollow-triangle glyph; abundant→scarce
  key + resource swatch; shares the `lens_resource` selector.
- Verified: 5 blessed goldens PASS ≤0.0073%, exit 0 (`population_lens.lua`, `scarcity_lens.lua`).
  Propagated to `docs/ui/LENSES.md` (two new sections + rung table) and `docs/ui/ICONS.md`
  (two glyphs). Both Briefs removed from OPENS § Canvas; REQUIREMENTS archived.
- **Status: Complete — 9/9 requirements met** (population R1–R4, scarcity R1–R5).

### In-session decisions

**Routing keyed by corp representative tile, not per building.** Supply/demand are `(corp,body)`
aggregates, not per-tile, so a corp's whole body output routes to the single market nearest its
lowest-id building. Finer per-building splitting across catchments is a noted follow-up — adequate
for the degenerate one-market-per-body world and revisable once population seeds real centres.

### Open items

- `tools/verify/econ_stability.cpp` is **pre-existingly broken** — it calls `apply_budget` with the
  old 3-arg signature (the workforce-contention param was added later). Not touched this session;
  worth a fix so the 100-tick stability check runs again.

## 2026-06-16 — v0.1.0 Session 2: the lens batch (Resource + Market) (branch v0.0.5)

The Session-2 goal: publish the two unblocked overlay modes, golden-verified against the F3
harness. A **Batch Publish** of two Briefs (OPENS § Canvas), **strictly serial** — both write
the same hotspot files (`ui_state.hpp`, `overlay.cpp`, `body_surface_canvas.cpp`; Market also
`circumplanetary_canvas.cpp`), so no fan-out was possible. **Status: Complete — 12/12 requirements
met** (resource 6/6, market 6/6); two commits, one per Brief.

### Briefs published

- **[B3] Resource lens render pass** *(first — built the shared selector + key infrastructure)*.
  `overlay_mode::resource`: **highest-value mode** tints each tile by its richest deposit's identity
  hue at a **per-body magnitude-normalised** opacity (composited over terrain via a new `lerp_colour`,
  so density reads); **single-resource mode** is a heatmap of one selected good. A lens-local **"Single"
  checkbox + a shared resource combo** (bound to a new `ui_state.lens_resource`) drive it. First lens
  with an **on-canvas key** (gradient bar + swatch/name). `verify.set_lens_resource` /
  `set_resource_mode` hooks; 4 blessed goldens PASS ≤0.0089%.
- **[B3] Market lens render pass**. `overlay_mode::market`: a body-wide **diverging warm↔cool wash**
  keyed to `price/base_price` (`diverging_colour`), plus a **Circumplanetary per-body price strip**
  (7 goods, selected highlighted). Reuses the Resource lens's selector + key. A new
  `verify.show_panel` hook clears the economy panel that `econ_step(12)` opens before capture; 3
  blessed goldens PASS ≤0.0082%.

### Execution / design calls

- **Markets are per-body, not per-tile.** `market_component` is one exchange per body, so the spec's
  "per-tile price tint" became an honest **body-wide wash**. Confirmed in the closing Q&A; raised a new
  timestamped Brief **[B3 ~] Multiple markets per body (tile-centred)** (OPENS § Trade) for the future
  spatial model.
- **Diverging keyed to `price/base_price`, not a basket "body mean"** — a mean across goods with very
  different base prices (steel 8 vs stone ~0.5) isn't meaningful. Confirmed; LENSES.md refined.
- **Resource "value" ranks by richness alone** — `resource_presentation` has no weight field; the
  spec's richness × weight is deferred. Confirmed.
- **Circumplanetary strip in `circumplanetary_canvas.cpp`**, not `solar_system_canvas.cpp` as the
  handoff's file list said (Solar has no market surface — LENSES.md rung table).
- **On-canvas keys/strip inset past the nav rail** (`nav_pane_width`): the full-window canvases render
  *behind* the 56px nav rail (known DEVLOG note), which clipped the first key placement; re-blessed the
  resource goldens for the cleaner position.

### Design-direction Q&A (closing)

All three calls above **confirmed** by the user. Q2 surfaced the markets-per-body direction (markets
will be **multiple per body, tile-centred on the capital**) — recorded as the new [B3 ~] Trade Brief
and a "toward per-tile variation" note in LENSES.md § Market lens. User also set a **workflow rule**:
*timestamp Briefs when written; treat the newest Brief as canon on overlap; resolve Briefs at
batch-publish* (saved to memory). The formal Q&A served as the review, so the two LENSES.md
implementation notes were written directly without lingering `⟳` markers.

### Open / next

Session-2 lens batch complete. Remaining v0.1.0 arc per ROADMAP: the v0.0.6 ledger family /
population, the selection trio, and Supply / Layer 5 (gated) — plus the design-owed substrate Brief
and the new tile-centred-markets Brief.

---

## 2026-06-16 — v0.1.0 Session 2 open: S1 doc-review + substrate design Q&A (branch v0.0.5)

Opening of **Session 2 (the lens batch)**. Started with the carried-over housekeeping: the three
Session-1 doc-review `S1` reminders parked in OPENS § Documentation, each carrying a transient
`> ⟳` "pending review" note. Ran a review Q&A on all three.

### S1 doc-review outcomes

- **NATION_GENERATION § Pass 2b (orphan-island post-pass)** — **accepted as written**; whole-component
  assignment to the nearest claimed land across water (Chebyshev; tie → lower nation index, then lower
  tile index) confirmed sound. `⟳` note removed.
- **CORPORATION_GENERATION § Pass 3 (lean holdings ranges)** — **accepted as written**; the per-focus
  ranges (extraction 3–4 / processing 2–3 / trade 1–2) and retained anchor + nearest-tile clustering
  confirmed. `⟳` note removed.
- **GENERATION_STRATEGY substrate forward pointer** — **accepted**, but the user chose to **open a
  design Q&A to settle the [B4 ~] substrate-generation Brief** rather than just rubber-stamp the
  pointer. `⟳` note removed after that Q&A (a formal Q&A is itself the review, per
  DEVELOPMENT_PRACTICES § Design-direction Q&A); the pointer text updated to the settled direction.

### Design-direction Q&A — [B4 ~] saturated nation-owned substrate

A scope addition beyond the Session-2 handoff (which had parked B4 for a later design pass), taken at
the user's request. **Documentation only — no code this session.** Settled to a **best-guess primary
direction**, with the speculative parts kept as open notes in the Brief (OPENS § Environment →
§ Cross-cutting):

- **Form:** per-tile **industry/productivity field** → **per-(nation, body) market aggregate**. No
  background-building entities (sidesteps the inter-body data-creep worry).
- **Generation:** field **consumes shared tile building-slots + resources** (saturation is a real
  shared budget, not a cosmetic tint). **Leading approach (open):** the user's population-seeded
  ripple — manufacturing dense at population centres, weakening outward.
- **Market coupling:** **both supply and demand** into the per-body markets (liquidity both ways).
- **Dynamic, not static:** grows into **unsaturated, resource-available** tiles over Ticks, gated by
  **resource discovery & research**; avoids already-saturated tiles.
- **Player interaction:** **competitive** — the player can displace / buy out substrate-occupied
  slots, converting background capacity into managed holdings (kept as *reclaiming slots*, not new
  managed detail).
- **Visibility:** **map-lens overlay** (industry density); final visual treatment **the user will
  personally flag for v0.2.0**.
- **Open notes recorded:** generation home (population sub-pass vs. standalone), dynamic growth model,
  slot/capacity budget split (the displacement seam), lens treatment, and a suggestion to seed the
  field from **population × deposit profile** as a single source the lens/markets/displacement share.

### Open / next

Housekeeping complete; tree carries doc-only edits (NATION_GENERATION, CORPORATION_GENERATION,
GENERATION_STRATEGY, OPENS, DEVLOG). Next per the plan: the **lens batch** — [B3] Resource lens then
[B3] Market lens, strictly serial on the shared `ui_state` / `overlay` / `body_surface_canvas` files,
golden-verifiable against the Session-1 references.

---

## 2026-06-15 — v0.1.0 publish plan Session 1: verification + world-gen foundation (branch v0.0.5)

First execution session of the ROADMAP near-term publish plan (§ Session 1). A **Batch Publish**
of three Briefs: lay the visual-verification safety net, then take the one clean world-gen fan-out.
Three commits, one per Brief. Build green; `world_audit` all-PASS.

### Briefs published

- **[F3] Visual-verification harness — golden-image diffing** *(first, alone)*. Added a PNG
  **reader** (`read_png_rgba` — inflates the writer's own stored-block zlib) and a **diff**
  (`diff_rgba` — per-pixel max-channel threshold `T`, caller-owned fail fraction `F`, magenta diff
  image) to `png_writer`; a compare/bless step in `run_verify` (golden dir derived from the script
  path's parent, so running against the **source** path reads/writes the committed tree); and a
  `--bless` arg in `main.cpp`. **End-to-end gate proven:** a clean re-run PASSed at **0.0056%**
  differing, a deliberately wrong golden FAILed at **56.44%** with a diff image and non-zero exit.
  `verifier-visual` SKILL.md documents the bless flow + tolerance knobs (user-approved skill edit).
  *Ignore-region mask deferred* (Q&A) — the fraction tolerance already absorbs the volatile counter.
- **[C2] Orphan-island assignment** *(sub-agent A)*. A deterministic post-pass
  (`assign_orphan_islands`) groups unclaimed non-ocean land into cardinal-adjacency components and
  assigns each whole component to the nearest claimed tile's nation across water. `world_audit` now
  reports **6048/6048 Kepler land tiles owned, 0 unclaimed** (was ~12% unclaimed).
- **[B4] Revise the corporation starting-holdings shape** *(sub-agent B)*. Retired the flat
  `k_min_holdings`/`k_max_holdings` (3–6) for a focus-shaped `holdings_range` (**extraction 3–4,
  processing 2–3, trade 1–2**); anchor + nearest-tile clustering retained. `world_audit` confirms
  all 8 corps within their focus ceilings (counts now 1–3) and S1 `can_place` stays PASS.

### Execution notes

- **Fan-out earned its cost here.** F3 ran serially in the main session (shared `png_writer`/`app`
  seam). The two world-gen fixes were genuinely disjoint files, so **C2-A ∥ B4-A** went to two
  concurrent sub-agents; the integrator (main session) owned the shared `world_audit.cpp` (both new
  audits), the doc propagation, the build, and verification. Both agents' edits were verified
  retroactively (diffs read) — clean.
- **New `world_audit` checks:** C2 R1 (zero unclaimed land) and B4 R1 (per-corp counts within focus
  ceiling). Folded into the harness exit code.
- **Doc propagation:** NATION_GENERATION § Pass 2b (orphan post-pass) and CORPORATION_GENERATION
  § Pass 3 (concrete counts) updated with transient `> ⟳` notes; three `S1` review reminders raised
  under OPENS § Documentation.

### Design-direction Q&A (closing)

- **B4 holding counts.** User noted they'd expected *higher* counts "based on a highly saturated
  generation method", but deferred the call. Verified against `GENERATION_STRATEGY.md` § The
  economic premise: the saturation is the **Nation AI's background substrate** ("not the player's
  playing field… not surfaced as manageable detail"), and corporations are **lean specialists** *by
  design* — inflating them would contradict the loop-simplifying premise. **Resolution: keep the
  lean counts**; the world's missing saturation is a *substrate-generation gap*, not a corp-holdings
  gap. Raised a new **[B4 ~] Generate the saturated nation-owned background substrate** Brief
  (OPENS § Environment → Cross-cutting; forward pointer added to GENERATION_STRATEGY § Open cross-doc
  items) — design-owed: how the substrate is represented (productivity field / background buildings /
  economic aggregate).
- **F3 ignore-region mask.** Kept deferred (captures pass comfortably without it).

### Open / next

Session 1 complete. Next per the plan: **Session 2 — the lens batch** (B3 Resource lens → Market
lens, serial on the shared `ui_state`/`overlay`/`body_surface_canvas` files), now golden-verifiable
against Session-1 references.

---

## 2026-06-15 — Finalise remaining `~` Briefs + clear S1 review notes (branch v0.0.5)

Design-only session completing the long-tail `~` Briefs in the up-to-v0.0.9 window, clearing the
retroactive doc-coverage review notes, and a closing design-direction Q&A. No `src/` change.

### Briefs settled (`~ → ✓`)

- **[B4] Logistics network & infrastructure model** — the item with active downstream pull (the
  convoy-mode model). Settled at **feasibility-probe depth**: the unifying **gate + cost** rule
  (a mode is available iff its endpoint infrastructure exists at both ends; per-mode
  `base_logistics_cost` ordered land < sea < air < space, feeding [S5]). The four modes:
  **land** (road = `road_level` *tile attribute*, mode ungated, road is a cost-reducer), **sea**
  (Port-gated, implicit water path), **air** (Airfield-gated — a deferred building), **space**
  (Launchpad@origin + Orbital Port@dest, Era-1 gated, player-directed). **Capacity deferred.** Only
  the Era-1 space gate actually gates in the prototype.
- **[F3] Clarify the time control view** — design was already settled in `TIME_CONTROLS.md`
  § Production clock view; flipped the stale Brief to ✓ (implementation-only remaining).
- **[F3] Golden-image diffing** — settled golden storage (`scripts/verify/golden/`), the two-knob
  tolerance model (per-pixel `T`, failing-fraction `F`, ignore-region mask), the `--bless`
  workflow, and **no CI gate in the prototype** (advisory local PASS/FAIL via `verifier-visual`).

Deferred to **v0.2.0**: [F4] tile-gen deep passes (orbital derivation, tectonic plates) and [F5]
nation behaviour — both F-priority, beyond prototype scope. v0.0.9 code-quality Briefs left
unauthored per user scope ("leave code quality for after").

### Doc-coverage notes cleared

All eight `S1` retroactive-doc-coverage review reminders were **reviewed with the user and
cleared**, their transient `> ⟳` notes removed from the eight docs (CORPORATION_GENERATION,
GENERATION_STRATEGY, SYSTEMS § Cross-cutting, POPULATION, ICONS, CIRCUMPLANETARY, TIME_CONTROLS,
SELECTION). OPENS § Documentation now holds only the [A3] propagation tracker.

### Design-direction Q&A (closing)

Three forks on the [B4] logistics calls made on the user's behalf:

- **Road = tile attribute — confirmed**, *with* an open direction: logistics will also carry
  **unit supply** and **population supply**, pointing toward an **emanation / cross-section "fuel"
  model** (supply radiates from sources, attenuates across distance/terrain) for land/sea/air —
  space a separate, larger consideration. Target feel: **Shadow Empire**'s logistics. Recorded as
  a durable design-reference note in `SYSTEMS.md` § Supply and an open consideration in [B4]. Not a
  Brief yet; the prototype keeps the simple per-mode-cost convoy model and grows toward this.
- **Air mode — designed-but-deferred (Airfield building) confirmed.**
- **Per-node throughput capacity — deferred confirmed.**

---

## 2026-06-15 — Design-direction Q&A: v0.1.0 design pass (branch v0.0.5)

Closing Q&A for the v0.1.0 design-completion pass below (the Batch Publish § design-direction
discipline — `DEVELOPMENT_PRACTICES.md` § Design-direction Q&A). Four genuine forks put to the
user; the answers confirm direction and open several new notes. No `src/` change — outcomes folded
into the affected OPENS Briefs (§ Trade, § Supply, § Infrastructure) and recorded here.

### Outcomes

- **Market model — full order book confirmed, with refinements.** The matched price-time order book
  is the right prototype scope (the original sell-order Brief was ambiguous; now settled). Every
  order carries a **price min/max** *and* the counterparty preference. New **v0.2.0 roadmap** notes
  opened (not prototype): **corporate contracts** (standing bilateral supply agreements) and
  **international tariffs** (nation-imposed cross-border trade cost). → [B4] Preferential purchasing.
- **Price coupling — convoy-only confirmed, reframed to inter-*market*.** Divergence arises only
  from logistics, no abstract term — but the coupling is **market-to-market**, not body-to-body. A
  convoy gains a **mode** (land / sea / air / space), each **dependent on infrastructure**. Space
  distance is **Euclidean, body-centre to body-centre** (the market's parent body). → [A4] Inter-body
  markets, [S5] Supply routing.
- **Supply control — auto is the rule; player-direction is the exception.** Standing player-direction
  of *every* convoy is **deprecated** as the default — the auto path (fill a shortfall from the
  cheapest reachable source) runs the loop. **Exception (open note):** Era 0 (perhaps Era 1) **space
  launches / missions MUST be player-directed** — leaving the gravity well is an explicit decision,
  never auto-dispatched. → [S5] Supply routing.
- **Decommission — labour + material cost model.** Build cost splits into **labour + material**.
  Decommission **refunds material** (minus a small pure-loss fraction) and **charges labour** for the
  teardown. Ripples to the build-cost representation (today a single Lua constant per type). → [A4]
  Building management.

These fold into the pending [A3] propagation (§ Documentation): § Trade, § Supply + `SUPPLY.md`,
`PRODUCTION.md` (build-cost split) now also carry the Q&A refinements.

### Follow-up (same session) — infrastructure gap + flag-2 scoping

- **Logistics network is undesigned (new `~` Brief).** The convoy-mode model surfaced a real gap:
  modes (land/sea/air/space) depend on infrastructure that has no design — roads, sea routes,
  airfields, the launchpad/spaceport. Opened **`[B4 ~]` Logistics network & infrastructure model**
  in OPENS § Infrastructure, with open notes added to `SYSTEMS.md` § Supply / § Infrastructure.
- **Flag #2 scoping — no space gameplay; inter-body market is a feasibility probe.** Per the user:
  we are **not scoping space gameplay** (no corp, no nation on Selene or any off-Earth body). The
  inter-body work is a **minimal build of the market-to-market logic alone**, to test feasibility
  vs. **data-creep** (markets/pools/convoys multiplying per body). The convoy-mode + infrastructure
  richness is deliberately deferred ([B4 ~] above). Recorded as a **Prototype scope** note on the
  [A4] inter-body markets Brief.

## 2026-06-15 — v0.1.0 design-completion pass (OPENS only, branch v0.0.5)

A design-only session (no `src/` change). Goal: complete the **design** of v0.1.0 by settling the
design-owed (`~`) Briefs against the current docs/code state. Scoped to **`OPENS.md` only** — a
second agent was reading the file concurrently, so the later additions were pure insertions and no
authority doc was touched this session.

### What changed (`OPENS.md` only)

- **Settled ~13 design-owed Briefs `~`→`✓`**, capturing each design *inline in the Brief*:
  - **Menu** — Corporation overview dashboard (4-block roll-up: money/holdings/production/alerts,
    launcher links, floating window); nav-rail ordering rule (gameplay-loop grouping ruled
    canonical, SYSTEMS tier as tie-break — open question closed).
  - **Ledger** — decomposed the single *Market-lens-&-ledger family* Brief into **five discrete
    Briefs** (economy-panel refit foundation, Market / Balance / Construction ledgers, Market lens
    render pass); the former [F4] buildings-overview Brief was **absorbed** into the Construction
    ledger. Settled the **lens-driven selection resolution** rule (specificity stack
    *building→listing→tile→body* + a per-lens validity/routing table).
  - **Trade** — preferential purchasing (matched price-time **order-book** model); inter-body
    markets (divergence **via convoys**, no abstract coupling term).
  - **Resources** — full-set deposit **scarcity** model (four bands keyed to rarity↔base-price,
    affinity-gated, rare goods presence-gated).
  - **Known Bug** — frame-stutter **measurement instrument** (a live frame-time HUD — the blocker
    was the instrument, now designed); body-label stepping **fix** (accept + dot/label co-snap).
- **Authored two net-new Briefs** for genuine done-definition gaps that had no Brief:
  - **[S5 ✓] Supply routing — convoys (Layer 5)** under a new **§ Supply** — the layer had no
    Brief, only the gated Supply-lens spec. Settled at prototype depth (convoy entity, per-unit
    logistical cost, auto+player dispatch, 5-Brief decomposition). The largest remaining build (a
    `5`; v0.0.7's whole theme).
  - **[A4 ✓] Building management — functional recipe & workforce control** — the done-definition's
    "recipe and workforce control" interaction half, previously only referenced by the [S5] index
    and the disabled v0.0.5 scaffold stubs. Settled to live in the **tile Selection element**
    (targeted action), with the broad Construction ledger linking to it.
- **Added [A3 ✓] propagation Brief** (§ Documentation) with a doc map: because the session wrote
  OPENS only, the normal `~`→`✓` *settle-into-the-authority-doc* step is **owed** as a follow-up.
  Every settled Brief carries an inline "propagation tracked under § Documentation" pointer.
- **Re-rated** along the way: inter-body markets and lens-driven selection lifted from `F` (they
  serve the done-definition / ledger routing); tile-gen refinements pushed to `[F4]` with its deep
  models (orbital derivation, tectonic plates) flagged **beyond the prototype**; the [S5] Layer-4
  umbrella marked a fully-decomposed **index**.

### Decisions

- **Design captured inline in OPENS, not the authority docs** — forced by the OPENS-only +
  concurrent-reader constraint, but also a deliberate review gate: the design direction is
  reviewable in one place before it propagates. Tracked by the [A3] propagation Brief rather than
  left implicit.
- **Inter-body price coupling *is* the convoy** — no separate price-linkage term; a body's market
  stays locally resolved and divergence/convergence is purely what logistics carry, net of cost.
  Keeps the spatial-arbitrage signal honest and avoids a second, redundant coupling mechanism.
- **Per-building control is a targeted action → Selection element, not a nav slot** — consistent
  with menus-are-broad-ledgers; the Construction ledger stays a *read/overview* surface and links
  out to the control.
- **Stopped short of over-creating.** After a full done-definition sweep, only one hard gap existed
  (recipe/workforce control). Era 1 access and save/load were surfaced as **scope questions**, not
  pre-emptively authored; the user then ruled both **out of scope** (early playtest, no saves).

### Open

- **[A3] propagation** is the next documentation session — settle this pass's inline designs into
  their authority docs (`MENU`, `LENSES`, `LAYOUT`, `SELECTION`, `SYSTEMS` §Trade/§Supply, a new
  `SUPPLY.md`, `RESOURCES`, `TILE_GENERATION`, `SOLAR`) before any doc-changing publish.
- Remaining `~` Briefs are the four `F`-priority, out-of-prototype items (golden-image diffing,
  time-control rework, tile-gen deep models, nation behaviour) — no design owed for v0.1.0.

## 2026-06-15 — TODO → OPENS rename + Brief design-state model (branch v0.0.5)

A backlog-structure session (no `src/` change). Renamed the backlog and gave every Brief an
explicit **design state**, in preparation for a run of design rounds to finish the roadmap's
documentation/design before further code.

### What changed (docs only)

- **`TODO.md` → `OPENS.md` (git mv, history preserved).** The file was never a checklist of
  small actions — it is a backlog of *described intent*. "Opens" (the open items) reads as a
  noun (a list), where "Open" read as a verb. Reframed the intro as **design-focused**: a Brief
  is the high-level framework from which tasks are later cut.
- **Two open states, per-Brief glyph.** Every Brief is *not yet implemented*; what varies is
  whether its design is settled. Added a **`✓` designed / promote-ready** vs **`~` design owed**
  state, carried as a glyph in the marker — `[<priority><difficulty> <state>]` (e.g. `[B3 ✓]`,
  `[F4 ~]`). Orthogonal to priority/difficulty. **Only `✓` Briefs are promotable;** a `~` Brief
  is *designed* first (a settle-into-the-doc pass flips it to `✓`). Added a matching **Design**
  depth verb above Promote.
- **Classified every active Brief.** Walked the whole backlog: the many "Design settled
  (2026-06-15)" Briefs → `✓`; the design-revision / "design X before promoting" Briefs (Market
  lens & ledger family, Buildings overview, Preferential purchasing, Lens-driven selection,
  Corporation overview dashboard, tile-gen refinements, full-set resource scarcity, both Known
  Bugs, the time-control rework, golden-image diffing) → `~`. Blocked-on-dependency Briefs
  (non-spatial go-to, canvas hit-testing) stay `✓` — sequencing, not a design gap.
- **Glossary.** Reworked the **Brief** entry to OPENS and added a **Design state (Brief)** entry.
- **Cross-references.** Updated all *live* forward pointers (`CLAUDE.md` doc map + Publication
  pipeline, `TASKS.md` / `REQUIREMENTS.md` policy prose, `ROADMAP.md`, `GLOSSARY.md`, the
  `verifier-visual` skill, and the `See TODO §…` pointers in the design docs) to OPENS. **Frozen
  historical records left as-is**: prior DEVLOG entries, the archived TASKS.md `<details>` group
  breakdowns, and the REQUIREMENTS.md resolved-archive lines all correctly name the file as it
  was at the time. Literal `TODO:` code comments in `src/` were untouched.

### Decisions

- **Filename `OPENS.md`** (user call) over `OPEN.md` — "open" reads as a verb; "opens" as a list
  of open items.
- **Glyph in the marker** (not a word tag or a per-state section split) — one character, no new
  sections, scannable, and reuses the existing marker grammar rather than adding a parallel
  system. Keeps the change low-overhead rather than a new ceremony.
- **OPENS holds both states.** The file name means "open/unrealised", not "undesigned"; the
  glyph carries the designed-vs-undesigned nuance, so promote-ready Briefs are not mislabelled.

### Open

- The `~` Briefs are the queue for the upcoming **design rounds** — each is a *design* pass
  (settle into its authority doc, flip to `✓`) before any promotion.

## 2026-06-15 — Batch Publish process + retroactive doc-coverage reconcile (branch v0.0.5)

A process/documentation session (no `src/` change) following the >C Brief pass. Defined the
**Batch Publish** discipline, corrected the **Publish** lifecycle, and retroactively reconciled
the design docs the >C pass left stale.

### What was built (docs only)

- **Batch Publish defined** (`GLOSSARY.md`, `TODO.md` § Publish, `CLAUDE.md` § Publication
  pipeline): a multi-Brief publish carries a documentation-coverage discipline — an up-front
  **doc-coverage determination** (do the docs already record the implementation, or is it a
  direct consequence of documented behaviour?), a **per-Brief doc collision map** with
  **sub-agent fan-out** across disjoint docs, a **transient `> ⟳` blockquote note** in each
  changed doc (removed once reviewed), an **`S`-tier review Brief per changed doc**, and a
  **proportional design-direction Q&A**.
- **Publish corrected** (`TODO.md`, `CLAUDE.md`): added the **brief-spanning requirement gate**
  — before a `src/`-changing Brief is decomposed into tasks, a Brief-wide requirement (usually
  `visual` verification) is written first, shaping the decomposition and acting as the
  end-to-end acceptance gate.
- **Design-direction Q&A practice** (`DEVELOPMENT_PRACTICES.md` § Design-direction Q&A): short
  rationale, recorded in DEVLOG (no dedicated log), kept proportional.
- **Retroactive reconcile** (transient notes added to each): `CORPORATION_GENERATION.md`
  Pass 3 → clustered 3–6 focus-shaped holdings + Pass 4 pre-game operating-history;
  `SELECTION.md` → the tile "Build here" front door; `SYSTEMS.md` § Trade → standing
  sell-orders / floor price. Three `S1` review Briefs logged under TODO § Documentation.

### Design-direction Q&A (outcomes)

- **Transient-note form:** a **visible `> ⟳` blockquote** everywhere (standardised SYSTEMS.md
  off its HTML comment).
- **Corporation holdings shape:** **flagged wrong** — the landed clustered 3–6 holdings is to
  be **revised** (target shape still to settle). Logged as `[B4]` under § Environment →
  Corporation generation; the doc stays accurate to current code with a *pending-rework*
  transient note.
- **Q&A recording:** a **dev practice with short rationale** in DEVELOPMENT_PRACTICES, recorded
  in DEVLOG — no dedicated Q&A log (judged overkill); the Q&A step is **proportional**, not
  mechanical.

### Status

Complete — docs only, no build impact. Three S-tier doc reviews + one corp-gen revision Brief
left open in TODO.

---

## 2026-06-15 — >C Brief pass, Wave 1.4: Corporation generation revision (branch v0.0.5)

Two coordinated Briefs on the corp-generation passes — **[B4] larger holdings + realism** and
**[C3] pre-game profit**. The holdings rewrite was drafted by a background sub-agent (disjoint
file `corporation_generation.cpp`) and revised in the main session.

### What was built

- **Clustered, focus-shaped holdings** (`corporation_generation.cpp`): `place_starting_asset`
  (one building) replaced by `place_starting_assets` — a focus-weighted **anchor** tile, then the
  remaining slots filled from the nation's tiles **nearest the anchor** (squared grid distance,
  id tie-break) so a corp's holdings cluster. Count 3–6 (`k_min/max_holdings`); the asset **mix
  follows `industrial_focus`** (`focus_asset_pattern`). Every placement gated by
  `placement_rules::can_place` — `world_audit` reports 0 invalid placements across 15 extraction
  assets.
- **Pre-game profit** (`app.cpp`): the existing startup warm-start extended 2 → 12 ticks, so every
  corp opens onto a multi-tick operating history (moved balances, non-empty pools).

### Decisions

- **Rejected the sub-agent's in-generation warm-start** — it hand-built a *duplicate* economy
  registry inside `corporation_generation.cpp` (a second copy of the Lua constants) and authored
  recipe ids at generation. The agent flagged both. Excised: `[C3]` is implemented at app startup
  (after `load_economy`) where the **real loaded registry** already exists — no duplication, no
  generation-time recipe authoring (`load_economy` assigns default recipes as before). `run_verify`
  stays deterministically cold.

### Status

Complete — 5/5 requirements met (REQUIREMENTS § corporation-generation-revision). Verified via
`tools/verify/world_audit` (0 invalid placements; biome + reserves still green) and a clean
`ProjectIo` Debug build.

---

## 2026-06-15 — >C Brief pass, Wave 1.3: Player sell orders (branch v0.0.5)

Layer 4 core code Brief — **[A3] Player-driven sell orders & preferential purchasing**, the
sell-orders half. The `sell_order` clearing hook already existed (floor-price honoured); this
Brief made it usable.

### What was built

- **`sell_order` moved to `components.hpp`** so both `ui_state` and `clear_markets` can name it
  without an include cycle; the Layer 3 "framework hook" comments removed.
- **`ui_state.sell_orders`** — standing player orders as game-intent, passed to `clear_markets`
  by `app::step_economy`.
- **Auto path yields to player control** (`market_clearing.cpp`): a (corp, body, resource) with a
  standing order is skipped by the greedy auto-surplus sell, so the player's order (and its floor)
  governs that resource — otherwise the auto path would dump the stock at market price first.
- **Authoring UI** (`construction_panel.cpp` § Sell orders): lists the player's orders on the
  in-view body with a remove, and a form (resource combo over traded goods + quantity + floor +
  add). Replaced the old disabled "Create sell order" stub.
- **Harness** (`econ_harness.cpp`): SO.1–3 — price floors+eases to 5.0; a qty-10 floor-6 order
  sells all 10 at max(5,6)=6 (income 60); pool debited.

### Decisions

- **Preferential purchasing split out + deferred** — true counterparty choice needs a *matched
  order book*; the prototype clearing is an *anonymous pooled* exchange (aggregate supply/demand,
  one resolved price, no per-seller matching). Carved into its own `[B4]` Brief (TODO § Trade) with
  the architectural blocker recorded, rather than forced into the pooled model.

### Status

Complete — 4/4 requirements met (REQUIREMENTS § player-sell-orders). Verified via
`tools/verify/econ_harness` (SO.1–3 + all prior assertions) and a clean `ProjectIo` Debug build.

---

## 2026-06-15 — >C Brief pass, Wave 1.2 + design wave (branch v0.0.5)

Continued the priority `> C` pass. One Layer 4 core code Brief in the main session, three
design/doc Briefs fanned out to concurrent background sub-agents (disjoint file scopes:
resource docs / a new TOOLTIP.md / the lens+icons files). Four Briefs, committed one each plus
a tracking close-out.

### [A3] Tile Selection element as the build front door (code)

Made the v0.0.5 construction scaffold **functional**. New `src/world/construction.{hpp,cpp}` —
`construct_building(world&, reg, corp, tile, type, target, out)` validates via
`placement_rules::can_place`, checks the corp can afford the registry build cost, then creates
the building (+ stockpile), authors it (staffed 0.5; extraction target; a processing facility
seeded with the default "steel" recipe), appends it to the corp's assets, and debits the cost —
mirroring generation Pass 3 but player-driven. Both entry points enqueue a pending request on
`ui_state.construction` that `app::render` executes against the mutable world (the const-world UI
surfaces only enqueue): the **tile Selection element** gained a "Build here" affordance (buildable
types + cost, affordability-gated) in `selection_panel.cpp`, and the placement-mode **canvas
click** now enqueues instead of being a no-op. `recipe_registry::recipe_id` was inlined into the
header so construction logic stays Lua-free (headless-buildable). New harness
`tools/verify/construction_harness.cpp` (11/11 PASS). Stale "v0.0.5 preview / non-mutating"
comments updated across the canvas / panel / ui_state.

### [B3] Lens system design + Resource glyph (sub-agent)

`docs/ui/LENSES.md`: the four stub lenses (Supply / Market / Faction / Resource) expanded to the
Corporation section's depth — per-lens spec, rung-applicability table, legends, interaction notes.
The **Resource lens** settled as the next to build (no data dependency): highest-value tint +
single-resource heatmap with a gradient key. New `ui::icons::resource` glyph (three stacked
density strata) added + catalogued in ICONS.md. The functional render pass is a new follow-on
Brief in TODO.

### [B4] Hover-card system design (sub-agent)

New `docs/ui/TOOLTIP.md`: the card is SELECTION.md's Focus state; one `draw_hover_card` dispatcher
**reusing the existing `entity_summary` builders** (share, don't duplicate); lightweight instant /
rich "why"-annotated on dwell. The implementation is a new follow-on Brief in TODO.

### [B2] Resource realism pass (sub-agent)

`docs/economy/{RESOURCES,PRODUCTION}.md` realism fixes: liquid-oxygen Era-0 sourcing via cryogenic
air separation (vs. the previous "stockpiled by other means" hand-wave); Mine (terrestrial, Era 0)
vs. Surface Extractor (off-world metallic, Era 1) era/terrain split made coherent. Flagged: an
ERAS.md gap (now a `[C1]` Brief), and that recipe *ratios* remain Lua-authored (no numbers invented).

### Status

Complete — build-front-door 6/6, lens-system-design 3/3 (REQUIREMENTS); hover-card + resource
realism are doc-class (verified by inspection + clean build of the lens glyph). Verified via
`tools/verify/construction_harness` (11/11 PASS) and a clean `ProjectIo` Debug build (which also
compiles the new glyph and the inlined `recipe_id`). Three sub-agents fanned out on disjoint scopes.

---

## 2026-06-15 — >C Brief pass, Wave 1.1: Workforce pool — step 1 (branch v0.0.5)

First Brief of the priority `> C` pass (Layer 4 core first). Published **[A4] Workforce pool
& population coupling, step 1** — the labour-pool half of the settled POPULATION.md workforce
model, *without* population (authored supply). Full Publish lifecycle, single sequential group
(every file sits on the shared economy seam, so no fan-out).

### What was built

- **Labour pool on `world`** (`world.hpp`): `default_workforce_supply` (3.0) +
  `workforce_supply_overrides` map + a `workforce_supply(corp, body)` accessor, held off the
  component structs (the `corp_body_pools` rationale) so the economy stays on disjoint files.
- **Contention in the economy step** (`economy_system.{hpp,cpp}`): per corp, demand per body =
  Σ `workforce_assigned`; contention scalar = `min(1, supply/demand)`; reported on
  `economy_report::workforce_contention`. Effective workforce (`workforce_assigned × contention`)
  now scales **both** extraction and processing output and is reported per building
  (`building_report::effective_workforce`).
- **Wages on effective workforce** (`budget_system.{hpp,cpp}`): `apply_budget` takes the
  contention map and bills wages on allocated, not requested, labour. Call sites updated
  (`app.cpp` `step_economy`, the harness).
- **Economy panel** (`economy_panel.cpp`): a "Workforce (corp × body)" section listing throttled
  pools (scalar < 1.0) in the warning colour, else "all fully staffed".
- **Harness** (`econ_harness.cpp`): WF.R2–R5 — uncontended single-building corp (scalar 1.0,
  all prior L3 assertions unchanged), and an over-built corp (4 sites, demand 4 > supply 3 →
  contention 0.75, output 15, wages on effective workforce).

### Decisions

- **Step 1 / step 2 split kept** — population-derived supply is *not* in this Brief; the TODO
  Workforce Brief was rewritten to **step 2 only** (population coupling), to be taken with/after
  **[S4] Population centres**. Authored supply (default 3.0, overridable) is the step-1 seam.
- **Default supply 3.0** chosen so existing single-building harness corps stay uncontended
  (assertions unchanged) while a realistically over-built body throttles — a tunable constant,
  not a balance commitment.

### Status

Complete — 6/6 requirements met (REQUIREMENTS § workforce-pool). Verified via
`tools/verify/econ_harness` (20/20 PASS) and a clean `ProjectIo` Debug build.

---

## 2026-06-15 — v0.0.5 Layer 4 UI groundwork (single Brief, scaffold scope, branch v0.0.5)

Second v0.0.5 block: published the **fifth enabler** that the foundations set had deliberately
held — A4 Layer 4 UI groundwork — scoped explicitly as a **non-mutating scaffold** (the
functional construction loop stays in v0.0.6). Single Brief, full Publish lifecycle.

### What was built

- **Construction interaction state** on `ui_state` (`src/ui/ui_state.hpp`): a nested
  `construction_state` (`active` / `building_type` / `resource_type target`) and a
  `show_construction_panel` flag (defaults false — ledgers start closed).
- **Ghost placement marker + non-mutating click seam** on the Planetary canvas
  (`body_surface_canvas.cpp`): when placement mode is active a ghost `icons::building` glyph of
  the chosen type follows the hovered tile, tinted `palette::positive`/`negative` by
  `placement_rules::can_place`. The select-on-click is guarded behind `!construction.active`; in
  placement mode the click is a documented no-op seam (v0.0.6 will construct there).
- **Construction / building-management panel shell** (new `src/ui/construction_panel.{hpp,cpp}`):
  shared `ledger_chrome` window; a **Build** section whose buttons arm placement mode (extraction
  offers a target from `placement_rules::k_extractable`) with a Cancel; a **Selected building**
  section showing the building on the selected tile read-only (type / target / recipe / workforce
  / cost) with **disabled** stub controls (recipe combo, workforce slider, sell-order button).
- **Shell wiring + verify hooks**: nav-rail slot 6 (building glyph) toggles the panel; `app::render`
  draws it; `app::run_verify` gains `show_construction` / `place_mode` so the panel and placement
  mode are drivable headlessly.

### Decisions

- **Scaffold, not functional** (user call) — no build-cost spend, no world mutation, no
  recipe/workforce/sell-order writes. Keeps v0.0.5 true to "foundations"; the management controls
  are present but disabled, wired to v0.0.6 logic later.
- **B ∥ C fanned out to two concurrent sub-agents** (disjoint: `body_surface_canvas.cpp` vs. the
  new panel files) after the `ui_state` foundation landed; the `ui_state` header and the
  nav/`app.cpp` integration stayed in the main session.
- **R3/R7 visual limits recorded, not faked** — the ghost is hover-driven and there is no
  tile-selection verify hook, so those captures are not headlessly drivable; both verified by
  code grep with the limitation noted (the same harness boundary already logged for the
  body-label and frame-stutter checks).

### Status

Complete — 8/8 requirements met (see REQUIREMENTS.md § layer4-ui-groundwork archive). Verified via
the ProjectIo Debug build, code grep, and `scripts/verify/construction_panel.lua`.

---

## 2026-06-15 — v0.0.5 Layer 4 foundations (publish set, branch v0.0.5)

First v0.0.5 work block. Branched `v0.0.5` off `main` and published four of the five
enablers the roadmap names for the *make-the-economy-buildable-on* theme, as a barrier set
(all groups clear each Publish step before any advances). The fifth enabler — A4 Layer 4 UI
groundwork — was deliberately **held** for its own pass: it is heavier and bleeds into v0.0.6
construction UI, so keeping it out preserved the batch's "low-risk, largely disjoint" shape.

### What was built

- **Reusable placement-rules seam** (TODO § Infrastructure, `[SSS2]`). Pulled the
  terrain/deposit placement logic out of `corporation_generation.cpp` Pass 3 into a
  screen-independent `src/world/placement_rules.{hpp,cpp}`: the prototype-extractable set,
  `is_ocean_tile` / `is_extractable` / `extractable_deposit` / `richest_extractable`, and the
  load-bearing `can_place(tile, building_type, target) → bool`. Pass 3 re-pointed at the seam
  with **no behaviour change** (world_audit: still 3 extraction assets, 0 invalid). The single
  most useful Layer 4 prep — player construction now shares one validity check with generation.
- **Multi-tick economy-stability harness** (TODO § Resources, `[S2]`). New
  `tools/verify/econ_stability.cpp` runs production → market clearing → budget over 100 ticks
  on a small fixed world and asserts: prices stay in the `[0.25×, 4×]` band, no NaN/Inf,
  deposit reserves decrease monotonically (1200 → 7.42), balances stay bounded. Named in the
  `verifier-headless` skill; settings.json allow rule added (user-approved).
- **Workforce model design** (TODO § Workforce, `[S3]`). Settled `POPULATION.md` § Workforce
  model (prototype → Layer 4): per-`(corp, body)` labour pool, proportional contention scalar
  (`supply/demand`), population-derived supply/wages, the player-sets-target vs.
  system-allocates split, and a 3-step additive upgrade path from the L3 authored
  `workforce_assigned` constant. Design only; implementation stays the `[A4]` pool-coupling Brief.
- **Uniform ledger-window chrome** (TODO § Ledger, `[B2]`). New `src/ui/ledger_chrome.hpp`
  holds `ledger_window_size` / `ledger_window_spawn`; the Tile Ledger and Economy panel both
  drive their window size/pos from it (resolving the prior 820×560 vs. 760×620 divergence). The
  future Market / Balance / Construction family inherits the two constants.

### Decisions

- **Four enablers, not five** — A4 UI groundwork held for a dedicated pass (user call), to keep
  the batch disjoint and low-risk.
- **Pool granularity per-`(corp, body)`**, not corporation-wide — labour does not cross bodies
  without transport, and contention is local; a corp-wide pool was considered and rejected.
- **`can_place` is the strict L4 check**; Pass 3 keeps its weighted scoring and reuses the
  seam's helpers, so generation behaviour is byte-for-byte unchanged while the seam is ready
  for player construction.

Status: Complete — 16/16 requirements met across the four groups (see REQUIREMENTS.md archive
§ v0.0.5 Layer 4 foundations publish set). Verified via the ProjectIo Debug build,
`tools/verify/econ_stability`, and `tools/verify/world_audit`. One commit per Brief plus a
tracking close-out.

---

## 2026-06-15 — Roadmap to v0.1.0; INITIAL_INSTRUCTIONS retired

Documentation-only. Replaced the layer-list build sequence with a proper milestone map and
split the retired file's content to its right homes.

### What changed

- **New `docs/development/ROADMAP.md`** (indexed in `CLAUDE.md`) — a lean, forward-facing
  milestone map: the versioning grain (one coherent theme per minor), the current position
  (v0.0.4, Layer 3 economy complete), the four forward minors, and the v0.1.0 done-definition.
  Sits above TODO/TASKS — names each minor's *theme*, not its Briefs (the lean choice, to
  avoid Brief duplication that drifts).
- **`docs/development/INITIAL_INSTRUCTIONS.md` removed** (`git rm`). Its build sequence is
  superseded by ROADMAP.md; its scope/exclusions already lived in TECH_FOUNDATIONS; its
  development rules migrated (below).
- **`DEVELOPMENT_PRACTICES.md`** gained three migrated sections — the per-milestone **ImGui
  panel** rule, the standing **development constraints** ("do not" list), and the
  **tone/approach** guidance — plus the layer reference in § Testing now points at ROADMAP.md.
- **Reference fixes**: `CLAUDE.md` doc index (new ROADMAP entry, expanded PRACTICES entry),
  the § Infrastructure L4 Brief in `TODO.md` (now points at ROADMAP v0.0.6), and the
  `economy_panel.hpp` header comment. The historical DEVLOG reference (2026-06-14 entry) was
  left verbatim as a permanent record.

### Decisions (Q&A before drafting)

- **Four minors, compressed.** v0.0.5 (L4 foundations) → v0.0.6 (building management **+**
  population, folded) → v0.0.7 (supply, L5) → v0.0.8 (budget + hardening, L6 + polish) → cut
  **v0.1.0**. v0.0.6 is the acknowledged likely split point.
- **v0.1.0 = full economy loop** — construction, population, inter-body supply, full budget,
  legible read surfaces; Conflict/Research/Policy/Diplomacy excluded by scope.
- **Constraints migrate to practices**, scope prose disregarded (already owned by
  TECH_FOUNDATIONS), roadmap kept **lean** (no Brief enumeration).



Backlog-only change (no code). Reworked the TODO Brief marker from a single 1–6
difficulty into a **`[<priority><difficulty>]`** pair:

- **Priority** (importance, ascending): `F · C · B · A · S · SSS`. `F` is *deferred*
  (replaces the old difficulty-6 status); `SSS` is *do immediately*. Re-rated every Brief
  against the current goal — **getting Layer 4 working** — so enablers rank high and
  fixes/future-note tweaks rank low.
- **Difficulty** (1–5) is now an approximate *time-to-do* on a **non-linear** scale
  (~5 min / ~20 min / ~1 h / ~3 h / ~12 h+, each step ≈ 3–4× the last); a `5` is a flag to
  break the Brief down. Difficulty 6 removed.

**Layer 4 rescoped** from "production UI overhaul" to **population centres + building
management** (the deferred POPULATION.md model coupled with construction / recipe-workforce
control / sell-order UI).

**Six new pre-Layer-4 Briefs** added (important, but not cleanly Layer 3 or 4):
placement-rules seam (`SSS2`, Infrastructure), automated economy-tick stability harness
(`S2`, Resources), workforce-model design (`S3`, Workforce), resource generation (`B3`) and
resource realism pass (`B2`, Resources), and Layer 4 UI groundwork (`A4`, Canvas). The
existing workforce-pool and ledger-family Briefs were re-rated up (`A4`) as L4 substrate.
TASKS.md note updated (tasks carry difficulty only; priority is TODO-level triage).

---

## 2026-06-15 — Layer 3 finalisation published (5 Briefs)

**Status:** Complete — 13/13 requirements met across three requirement groups (C and E are
difficulty 2, inline verification). Published as a barrier set: tasks + requirements + the
collision map written for the whole set first, all code completed and verified together,
then committed.

Finalising the production economy: the market now reprices from supply/demand, deposits are
finite, the world opens warm, and the player sees their money in the header. Five Briefs
pulled (including deferred items) and taken through the five Publish steps breadth-first.

### What was built

- **Price resolution (A)** — `clear_markets` now accumulates supply/demand, then resolves
  each `market_component.price[r]` toward `base_price × sqrt(demand/supply)`, clamped to
  `[0.25×, 4×]` and EMA-eased (0.5) from the prior price. Every sale/purchase is valued at
  the resolved price, so the budget loop follows automatically. `market_clearing.{hpp,cpp}`.
- **Deposit depletion (B)** — `tile_generation` Pass 6 seeds `resource_remaining = richness
  × 400`; `run_extraction` draws it down, tapers output over the last ~8 ticks of nominal
  yield, and reports the building **`exhausted`** ("out of resources", distinct from idle)
  below 5% of nominal. Finite — no refill. Surfaced in the economy panel's State column.
- **Pre-game economy ticks (C)** — `app::run` primes two `step_economy()` ticks (and seeds
  the balance history with opening capital) before the first frame, so the player opens onto
  warm pools / moved balances / live market figures. Not run in `run_verify` (stays cold).
- **Player balance header (D)** — `draw_header_panel` re-signatured to take the world + a
  capped balance history; renders **BALANCE** (negatives red), **STOCKPILE** valuation
  (player pools at market price), and **NET** (coloured ±/qtr) plus a sparkline. History is
  maintained in `app` and pushed each `step_economy()`.
- **Uniform ledger-window principle (E)** — the Market/Balance/Construction ledger family
  stays deferred to Layer 4, but the single chrome rule it inherits (one size constant + one
  spawn anchor) is settled in `LAYOUT.md`, with a `[2]` standing Brief under TODO § Ledger.

### In-session decisions

- **Q&A before publish.** Two rounds settled the ambiguous Briefs: price curve = damped
  `sqrt(D/S)` with EMA smoothing and a `[0.25×, 4×]` clamp; depletion = taper-to-zero then
  idle, reported as "out of resources", finite only; header = balance + stockpile valuation
  + last-tick net with a sparkline; ledger family deferred but the chrome principle settled;
  pre-game = warm-start ticks only (the heavier pre-game-profit sim stays deferred).
- **Repricing seam.** `clear_markets` was restructured to a two-phase shape — move
  quantities (debit pools, record sales/buys) first, resolve prices once supply/demand are
  known, then value every movement — so the displayed price and the cash flow agree. The
  budget step was left untouched (it reads the flows).
- **Reserve sizing & taper are hard-coded estimates.** `deposit_reserve_factor = 400`,
  `deposit_taper_ticks = 8`, `deposit_min_taper = 0.05` — legible, playtest-tunable; richness
  is unchanged (still the rate multiplier), the reserve is what depletes.
- **Stockpile valuation reads ~0 for pure extractors.** Confirmed expected: surplus is sold
  each tick, so an extractor retains little stock; retained stock (processor reservations,
  Layer-4 player holds) values non-zero. The figure renders correctly.
- **Commits.** Four functional commits — A, B, (C+D merged: both edit `app.cpp` and are
  build-coupled), E — plus a tracking close-out. Verified via `tools/verify/econ_harness`
  (price + depletion), `tools/verify/world_audit` (reserve seeding), and the new
  `scripts/verify/header.lua` (header capture). Full `cmake` build links clean.

### Open / deferred (still Briefs in TODO)

- **Player-driven sell orders & preferential purchasing**, the **Market/Balance/Construction
  ledger family** (now carrying the uniform-chrome principle + a standing chrome Brief), the
  **workforce pool** (population-gated), **inter-body markets** (Layer 5), and **model
  pre-game profit** (the longer operating-history sim) all remain deferred.

---

## 2026-06-14 — Layer 3 economy published (8 Briefs)

**Status:** Complete — 27/27 requirements met across seven requirement groups (S2 is
difficulty 2, inline verification). Published as a barrier set: doc-refactor commit first,
then one commit per Brief.

The Layer 3 extraction → processing → market → money economy, plus two independent audits.
Eight Briefs taken through the five Publish steps breadth-first.

### What was built

- **Data-model foundation** — `building_component` gains `target_resource` + a `recipe` id
  (`no_recipe` sentinel); `tile_component` gains the reserved `resource_remaining` array
  (unused in L3); `corporation_component` gains `balance`; `world` gains the
  `(corp, body) → stockpile_component` pool map + `pool_for()`.
- **Recipe & economy registry** — `scripts/recipes.lua` (steel / refined fuel / food
  rations) and `scripts/economy.lua` (per-type `base_rate`/`maintenance`/`base_wage`/
  `build_cost`, `t_full`/`t_idle`), loaded by `recipe_registry` via sol2. The registry
  **header is pure data** (no sol2) so `world/*` economy logic stays headlessly buildable.
- **Production / market / budget** — `run_economy_step` → `clear_markets` → `apply_budget`,
  driven on each econ-tick boundary in `app::run` (and by `verify.econ_step`).
- **Economy panel** — read-only observability (balances, pools, building states, market
  supply/demand); nav-pane slot 7.
- **S1 placement audit** + **S2 Kepler biome balance**.

### In-session decisions

- **Processing run model (reconciled the two stated behaviours).** The Brief specified both
  a two-threshold partial run *and* auto-buying input shortfalls — which conflict for a
  pure-processing corp with an empty pool (pool coverage 0 < `t_idle` → it would never
  bootstrap). Settled: **with a market on the body the processor runs a full batch, drawing
  pool-first and auto-buying the shortfall** (the L3 path); **without a market it falls back
  to the two-threshold partial run from its own pool** (full ≥ `t_full`, scaled to coverage
  between, idle below `t_idle`). Both thresholds remain load-bearing for the marketless case;
  the constants stay tunable. Caught because the first panel capture showed every processor
  idle (`out=0.0`).
- **Field authoring at placement.** `corporation_generation` Pass 3 now staffs producing
  assets (`workforce 0.5`), authors `target_resource` from the tile's richest *extractable*
  deposit, and opens `balance` at `starting_capital`; processing recipes are assigned in
  `app::load_economy` from the registry (the recipe id is a registry index, unknown at
  generation). Defaults are legible round numbers, to be tuned by playtest.
- **S1 finding.** The old Pass 3 extraction guard scored by *total* deposit (incl. ambient
  stone/sand) and only excluded ocean, so an extraction site could land on a tile with no
  prototype-extractable deposit → zero output. Fixed by scoring on extractable deposit and
  authoring the target from it. `world_audit`: 0 invalid placements.
- **S2.** forest+wetland 1.5% → **3.96%** of Kepler tiles (high-moisture cutoff 0.65→0.55,
  ocean `bias_amp` 0.07→0.05); ocean fraction held by the percentile threshold.
- **Verification.** Two durable headless harnesses under `tools/verify/` (`econ_harness`
  for the economy arithmetic, `world_audit` for S1/S2) plus `scripts/verify/economy_panel.lua`
  with the new `verify.econ_step` / `verify.dump_economy` hooks. Full `cmake` build links clean.

### Open / deferred (still Briefs in TODO)

Price resolution from supply/demand; player-driven sell orders & preferential purchasing
(the framework hook is stubbed); inter-body markets; deposit depletion (the reserved
`remaining` field is in place); the workforce pool; and the Layer 4 construction UI.

---

## 2026-06-14 — "Brief" terminology + Layer 3 design Q&A and brief authoring

**Status:** Complete (documentation only). No code changes. Working-tree doc edits;
not yet committed.

### Terminology — "Brief"

Coined **Brief** as the glossary term for the unit of *described intent* in TODO.md
(formerly the bulky "TODO item"). A Brief is the design-level view of one piece of work;
promoting it into TASKS.md decomposes it into a **task group** (one Brief ↔ one group of
tasks), distinct from a single **task**. Chosen from a four-option shortlist (Brief vs.
Blueprint / Initiative / Epic). Refactored the term across the live process docs —
`GLOSSARY.md` (new entry), `CLAUDE.md`, `TODO.md`, `TASKS.md`, `req/REQUIREMENTS.md`,
plus two live cross-refs in `LENSES.md` / `ICONS.md`. Historical DEVLOG entries and the
REQUIREMENTS archive were left verbatim as permanent records.

### Layer 3 design Q&A — decisions

A long question/answer pass settled the direction for the remaining Layer 3 core
directives. The prerequisites (resources, tile generation, nations, corporations) are in
place; the data model is generic (`building_type` is `extraction_site` / `processing_facility`
/ `port`, with no per-building target/recipe), which shaped several answers.

- **Extraction.** Explicit `target_resource` field on `building_component`; output
  `= base_rate × deposit_richness × workforce × (1 − hazard)` (linear). Accrues **at the
  economy tick**, not per simulation step. Deposits infinite in the prototype but a
  **reserved `remaining`** field is added now (two-value deposit model: richness vs reserve).
- **Processing.** Recipes authored in **Lua → C++ registry**; explicit `recipe` id field,
  fixed at construction. Inputs drawn from a **shared (corp, body) stockpile pool**.
  **Two-threshold partial-run** (full ≥ `T_full`; proportional between; idle < `T_idle`;
  thresholds tunable/open). Recipe schema is multi-input / multi-output with reagents.
- **Workforce.** Authored constant 0–1, read-only, linear scalar. The real **pool +
  contention** model is deferred and **gated on population centres**.
- **Stockpile.** One pool per **(corporation, body)**, stored as a **world-level map**
  (the `tile_to_nation` pattern), off `building_component`. Panel shows pool totals +
  per-building rates + market + balance.
- **Market (re-scope).** Market resolution **collapses into Layer 3**: supply = surplus a
  corp **lists for sale** (above own needs); demand = processor **shortfalls auto-bought**;
  transactions clear at **`base_price`**. **Price resolution and inter-body markets stay
  open** (Trade briefs). Markets are distinct from corp pools. A **player sell-order
  framework hook** is included now.
- **Budget.** Per-corp running **`balance`** opening at `starting_capital`; income from
  sales, expenditure = input purchases + **maintenance** + **wages** (`workforce × base_wage`,
  tunable); negative allowed and flagged. **Layer 4 is redefined** as the production UI
  overhaul (construction, building management, market ledgers).

### Briefs authored

Filed the above into TODO.md as Briefs under their system categories — **Resources**
(data-model foundation, Lua recipe/constants registry, production simulation), **Trade**
(market clearing; deferred: price resolution, sell-orders/preferential purchasing,
inter-body markets), **Budget** (the money loop), **Workforce** (deferred pool/population),
**Ledger** (observability panel), **Infrastructure** (the new-Layer-4 construction/management
UI), **Environment** (deposit depletion; corporation pre-game-profit modelling), and
**Documentation** (rewrite the build sequence for the re-scope). Not yet promoted to TASKS.

### Open items

- Promote the active Layer 3 briefs (foundation → registry → production → market → budget →
  panel) into TASKS.md and requirements when ready to build.
- Threshold values (`T_full` / `T_idle`), base rates, `base_wage`, and the economy-tick
  period (one quarter) are tunables to settle during implementation/playtest.
- Full resource-enum expansion beyond the prototype subset remains a later pass.

---

## 2026-06-14 — Publish block: Selection info element + Known Bug

**Status:** Complete — 2/6 groups shipped (9/9 reqs met), 4/6 cancelled back to TODO.
GT: 5/5 (R1–R5). GL: 4/4 (R1–R4). Frame stutter: 0/2 (R1/R2 failed). Body labels: 1/2
(R1 complete, R2 failed). See REQUIREMENTS.md archive for the per-row outcomes. Full
app builds clean (Debug, exit 0); `selection_go_to.lua` captures regenerate.

### Process — first multi-item publish under barrier semantics

Clarified the Publish lifecycle in TODO.md: when several items publish together, the
five steps run as **barriers across the whole set** (breadth-first, not depth-first) —
every item clears step *N* before any starts *N+1*, and step 4 (complete) closes only
on *terminal* states (complete **or** cancelled). Then exercised it on the two
sub-sections (six groups). Combined collision map: only GT (`view_nav.*`, `app.cpp`
run_verify region, `selection_go_to.lua`) and GL (docs) landed code/docs, and their
write-sets are disjoint, so the set was collision-free in execution.

### What was built (shipped groups)

- **Go-to planetary landing + Kepler-only reliability** — merged three cross-filed
  items (Selection 'go to' → planetary, "only works for Kepler", and the duplicate
  Known Bug row). `focus_on_entity` now routes a **body** through `focus_on_surface`
  (Planetary tile rung) instead of `focus_on_body`, and a **tile** selection is a
  no-op. Confirmed the Kepler-only symptom was an unhelpful landing rung, not an
  id/lookup failure: added `verify.go_to` (drives the real `focus_on_entity` path) and
  `scripts/verify/selection_go_to.lua`; Kepler / Cinder / Selene all land on their tile
  grids with the minimap re-anchoring to each. `view_nav.{cpp,hpp}`, `app.cpp`,
  `SELECTION.md`.
- **Generation Ledger design** — authored `docs/generation/GENERATION_LEDGER.md`
  (indexed from `CLAUDE.md`): per-tile derivation breadcrumb, per-body histograms,
  regenerate-on-demand (don't persist) data lifetime, and surfacing as a Ledger window
  plus a Planetary field-overlay lens, sharing the tile-derivation content builder with
  the hover card / Selection element.

### Cancelled back to TODO (terminal, no code landed)

- **Non-spatial 'go to' routing** — blocked: no `nation_ledger` / `corporation_ledger`
  target exists.
- **Canvas hit-testing** — blocked: those entities are not yet drawn as selectable
  canvas markers.
- **Frame stutter measurement** — verification needs frame-time instrumentation over a
  *live* present loop (no headless tool); baseline recorded (vsync on, no cap, no
  readout). The live instrument is the deferred design work.
- **Body labels stepping** — root cause confirmed (`AddText` glyph-grid quantisation vs.
  sub-pixel dot; `solar_system_canvas.cpp:218–224`); the fix and its temporal
  verification are deferred (no headless tool observes motion over time).

---

## 2026-06-14 — Visual-verification harness (Phase 2)

**Status:** Complete — V7–V12 met. Full app builds clean (Debug, exit 0); `--verify`
runs headless and regenerates the corporation-lens captures via the new library.

### What was built

Phase 2 of the visual-verification harness — making a visual check no longer require
hand-writing a bespoke `.lua` each time, across the three TODO strands (settled: shared
command layer; one general `verifier-visual` skill).

- **Shared canvas command vocabulary** (`src/ui/canvas_command.{hpp,cpp}`): an
  `enum class canvas_command` (descend/ascend, body next/prev, pan ×4, zoom ×2, lens
  next/prev/clear), `apply_canvas_command` (pure `ui_state` mutation), and
  `canvas_command_from_name`. The single dispatch behind both the keys and the verify API.
- **Keyboard navigation** (`src/core/app.cpp`, `handle_key_down`): the keybinding table
  (CANVASES.md § Keyboard) mapped onto the command layer, guarded by
  `ImGui::GetIO().WantCaptureKeyboard`; F12 capture unchanged.
- **`verify.center_tile(col,row[,zoom])`** — folds the pan-centring math out of Lua.
  Implemented as a pending-centre request on `ui_state`, consumed inside
  `body_surface_canvas` where the exact grid transform is known (so the math lives in
  one place). Also added `verify.command(name)` (shared dispatch) and `verify.buildings()`
  (building positions as a Lua table, for `tour_buildings`).
- **Reusable library** (`scripts/verify/lib.lua`): `sweep_overlays(prefix)`,
  `tour_buildings(zoom)`, `frame_tile(col,row,zoom)`. Auto-loaded by the harness from
  the script's directory before the script runs (no `require` — `package` is not opened).
- **`corporation_lens.lua` refactored** onto the library — no hand-computed `set_pan`
  literals; reproduces the Phase 1 R2–R6 captures.
- **`verifier-visual` skill** (`.claude/skills/verifier-visual/SKILL.md`): wraps
  `ProjectIo --verify <script>`; authorising a check = adding a `scripts/verify/*.lua`.
- **Docs**: CANVASES.md § Keyboard, DEVELOPMENT_PRACTICES.md § Visual verification.

### In-session decisions

- **Shared command layer (owner's call).** Keyboard and the verify API dispatch through
  one `canvas_command` enum, so a script reads as the player's key sequence.
- **One general `verifier-visual` skill** rather than per-feature skills; a check is
  authorised by adding its script.
- **center_tile via a pending request**, not a duplicated transform: the canvas already
  computes the exact (font/grid-dependent) metrics at draw time, so the request is
  consumed there — no second copy of the pan math, no empirical pan constants.

### Verification

Ran `ProjectIo --verify scripts/verify/corporation_lens.lua`; inspected the PNGs. The
player building tile (22,82) and a rival (42,63) each centre exactly under the
corporation lens, with the player blue / rival coral tints and markers — matching the
Phase 1 evidence, now produced by `frame_tile`/`sweep_overlays` with no pan math.

### Open items

- Keyboard injection itself is not exercised headlessly (no synthetic SDL events); the
  shared dispatch it routes through is, via `verify.command`. A later strand could inject
  events if end-to-end key coverage is wanted.
- The verify capture still shows a stray hover tooltip (mouse rests at a default
  position with input enabled) — cosmetic, pre-existing, not Phase 2 scope.

---

## 2026-06-14 — Visual-verification harness (Phase 1) + Corporation lens closed

**Status:** Complete — visual-harness V1–V6 met; corporation-lens R2–R6 re-verified
and the cancelled group closed (all 9 rows met). Full app builds clean (Debug, exit
0); `--verify` runs headless and exits 0.

### What was built

Phase 1 of the automated visual-verification harness — the tool that makes the
`visual` requirement class runnable without a human at the screen (it had been the
blocker that cancelled the corporation-lens group earlier this session).

- **PNG writer** (`src/core/png_writer.{hpp,cpp}`): dependency-free
  `write_png_rgba()` — stored-DEFLATE zlib + CRC32/adler32. Chosen over vendoring
  stb_image_write (no fetch) and over keeping BMP (the Read tool reads PNG, not BMP,
  so Claude can inspect captures directly).
- **Capture → PNG** (`src/core/app.cpp`): `save_screenshot()` now converts the
  `SDL_RenderReadPixels` surface to RGBA32 and writes PNG; supports a named capture
  (F12 keeps the timestamped path).
- **`--verify <script>` mode** (`src/core/app.{hpp,cpp}`, `src/main.cpp`):
  `run_verify()` sets up a deterministic session (fixed window, seeded
  `make_hard_coded_world`, sim paused), binds a `verify` Lua table
  (`goto_surface`, `set_overlay`, `set_zoom`, `set_pan`, `add_pan`, `capture`,
  `log_buildings`) wired straight to `ui_state` (direct-state driver), runs the
  script, exits. `setup_world()` extracted from `run()` so both share one start state.
- **Corporation-lens verify script** (`scripts/verify/corporation_lens.lua`):
  captures the home surface under none/faction/corporation and zooms onto the player
  building (22,82) and a rival (42,63).
- **Docs** (`DEVELOPMENT_PRACTICES.md` § Visual verification, `req/REQUIREMENTS.md`
  policy): the harness is now the standard `visual` verification method.

### Verification result (corporation lens R2–R6)

Ran `ProjectIo --verify`; inspected the PNGs via the Read tool. Confirmed: the
corporation lens button (square+dot glyph) is present and active in the strip
(R2/R3); the player tile tints `faction_colour(0)` blue and a rival tile tints a
distinct hashed colour (coral) — R4 and R5; surrounding non-corporate tiles stay
terrain-coloured (R6). The cancelled corporation-lens group is now closed.

### In-session decisions

**Driver = direct state manipulation (owner's call).** The verify API writes
`ui_state` directly rather than injecting keys, so captures are reproducible. Full
keyboard navigation is deferred to Phase 2 (player-facing; TODO § Canvas).

**Pan aiming is empirical.** `set_pan` takes screen pixels; the pan to centre a tile
is `(grid_centre − tile_local) · zoom`, derived/confirmed against a capture rather
than replicating the canvas's title-bar/font-dependent transform in two places.

### Open items / findings

- **Player-tile border is redundant** under the corporation lens — fill and border
  are both `faction_colour(0)`, so the border is invisible. R5 holds via the fill
  colour; logged as a `[1]` TODO § Canvas to recolour the border for contrast.
- A script edited after a build is stale in `build/Debug/scripts` until the next
  build; run `--verify` against the source path (`../../scripts/...`) when iterating.

---

## 2026-06-14 — Corporation lens

**Status:** Code-complete, then **CANCELLED** — 4/9 requirements met (R1, R7, R8, R9);
the 5 visual rows (R2-visual, R3–R6) are `failed` (no `visual` tool; computer-use
declined). Full app builds clean (Debug, exit 0); all changed translation units
compile and link into `ProjectIo.exe`. The code remains in the tree. Per the new
TASKS.md § "Cancelling a task group", the group was cancelled rather than left
half-tracked: requirements marked `failed`, intent merged back into TODO § Canvas
"Corporation lens — verify the landed code", task stubs removed from TASKS.md. The
reusable verification method is TODO § Canvas "Automated visual-verification harness".

### What was built

Promoted and executed the **Corporation lens** group from TASKS.md (TODO § Canvas
[4]). A → {B, C} → D; all run in the main session (the canvas integrator owns the
hotspot file, and B/C are trivial enough not to warrant fan-out).

- **A — `docs/ui/LENSES.md` created.** New design authority for the lens system.
  The Corporation section is fully settled (ownership = a tile carrying a corporate
  building via `w.corporations[].assets` → `building_component.tile`, **no influence
  radius**; Planetary-only; player corp = `faction_colour(0)` + border, rivals =
  per-corp hash; unowned = terrain colour, no nation underlay). The other four lens
  sections are stubs recording current behaviour.
- **B — `icons::corporation` added** (`icons.{hpp,cpp}`): a filled square with a
  centred dark inner dot — a "seal" silhouette distinct from the processing-facility
  plain square, the extraction diamond, and the port/unit triangle.
- **C — `overlay_mode::corporation` added** (`ui_state.hpp`) and wired into the
  overlay strip (`overlay.cpp`): glyph dispatch, `overlay_mode_name`
  ("Corporation ownership"), `overlay_mode_short_name` ("Corp"), and the strip
  `modes` array (now four lenses).
- **D — render pass in `body_surface_canvas.cpp`.** Under `overlay_mode::corporation`:
  owned tiles tint to their corp colour; player-corp tiles additionally get a thin
  `faction_colour(0)` hex outline; unowned tiles stay terrain-coloured. Guarded
  entirely behind the corporation branch — no change on Solar/Circumplanetary.

### In-session decisions

**Extracted a shared `corp_colour` lambda.** The building-marker pass already
inlined the player-vs-rival colour logic (faction slot 0 for the player, a
multiplicative hash kept off slot 0 for rivals). The lens tint must agree with the
markers, so the logic was lifted into one lambda used by both — a single source of
truth rather than a second copy.

### Open items

- Visual confirmation of R2/R3–R6 (glyph reads correctly in the strip; tints,
  player border, and unowned-terrain fallback render as intended) still pending —
  needs an in-GUI run.

---

## 2026-06-14 — Layer 3 foundations: political-layer render, lens icons, Circumplanetary zoom cap

**Status:** Complete (code). Full app builds clean (Debug, exit 0); all six changed
translation units compile and link into `ProjectIo.exe`. Not yet visually run in
the GUI.

### What was built

Promoted the four sub-difficulty-6 Canvas items from TODO.md into TASKS.md and
executed them in four waves, using **parallel sub-agents on disjoint file scopes**
for the substantial branch and the main session for foundations, hotspots, and
integration.

- **Wave 1 (foundations, main session):** Circumplanetary `zoom_max` derived from
  `max_moon_au / 0.15` so the deepest zoom frames ~0.3 AU
  (`circumplanetary_canvas.cpp`); three lens glyphs `supply`/`market`/`faction`
  added to `icons.{hpp,cpp}`; `palette::nation_colour(entity_id)` — a 12-slot hue
  wheel keyed by a Knuth multiplicative hash — added to `presentation.{hpp,cpp}`.
- **Wave 2 (concurrent sub-agents on disjoint files):** nation/corporation stat-block
  builders `draw_nation_summary`/`draw_corporation_summary` (`entity_summary.{hpp,cpp}`)
  ∥ the Faction-lens render in `body_surface_canvas.cpp` (nation tile tint + odd-r
  neighbour borders via midpoint-perpendicular edges + per-corporation building
  marker colours). Map-lens icon buttons wired into `overlay.cpp` inline meanwhile.
- **Wave 3 (main session, hotspot):** `nation`/`corporation` added to
  `selection_kind`; `selection_kind_of`, `selection_kind_name`, and the
  `selection_panel.cpp` title/summary dispatch extended.
- **Wave 4 (main session):** open on the Faction lens by default (`app.cpp`).

This closes Canvas items **[1] Circumplanetary max zoom**, **[2] Map lens icons**,
**[4] Render the political layer**, and **[2] Default view should surface the
generated world** (home-surface open + Faction lens default).

### In-session decisions

- **Nation colours: 12-slot fixed hue wheel + Knuth hash**, distinct from the
  6-slot faction palette — nations tint territory, factions mark ownership.
- **Faction-lens tint is a direct replacement** of the terrain colour (no blend);
  unclaimed tiles (absent from `tile_to_nation`, e.g. ocean) keep terrain hue.
- **Borders draw only under the Faction lens**, via the robust midpoint-perpendicular
  edge method (avoids per-vertex offset-row mapping). Claimed/unclaimed counts as a
  border.
- **Building markers are coloured by owning corporation always-on** (player corp =
  faction slot 0; others a hashed non-zero slot), independent of the active lens.
- **Selection summaries for nation/corp ship now**; canvas hit-testing for them
  remains a separate Ledger follow-up (they are not yet click-selectable).

### Left open

- New TODO item **[3] Design the lens system** (`docs/ui/LENSES.md`): spec the five
  lenses incl. proposed Corporation + Resource lenses, rung applicability, icon
  vocabulary, legend format.
- No legend/colour key for the Faction lens yet (deferred with the lens-design doc).
- Canvas hit-testing for nations/corps (Ledger follow-up); the political-layer
  hover read-out still waits on the deferred hover-card system.

---

## 2026-06-14 — version 0.0.3 — Environment: nation + corporation generation, tile tuning

**Status:** Complete (code). Full app builds clean (Debug, exit 0); the two new
`src/world/*.cpp` translation units compile and link into `ProjectIo.exe`. Logic
verified via a throwaway headless harness (compiled with the `world/*` TUs only,
per `reference_headless_build`); not yet visually run in the GUI.

**Backup:** `backups/v0.0.3/src/` — restore this tree over `src/` to roll back.

### What was built

Closed out the **Environment** category for v0.0.3 — the world-generation spine.
TODO.md's Environment section was broadened from terrain-only to the whole
generation layer (Tile / Nation / Corporation) and the two unitemised generation
docs were itemised, scoped, and promoted to TASKS.md as three groups.

Execution used **parallel sub-agents on disjoint file scopes**, gated by the
real collision map (the passes inside each generator share one `.cpp`, so
within-generator parallelism was rejected; concurrency is cross-group):

- **Wave 1 (concurrent):** Tile tuning (`tile_generation.cpp` only) ∥ Nation
  pipeline (`components.hpp`, `world.{hpp,cpp}`, new `nation_generation.{hpp,cpp}`).
- **Wave 2:** Corporation pipeline (new `corporation_generation.{hpp,cpp}` +
  `components.hpp`/`world.hpp`), gated on the nation component existing.
- **Integration (main session):** all `hard_coded_world.cpp` hooks, both builds,
  and verification. Sub-agents did not build or commit.

**Nation generation** (`generate_nations`): five passes — habitable-preferring
seed placement with min-separation, weighted Voronoi BFS (ocean never claimed,
mountains/highlands as soft cost barriers), resource-profile sum, seeded political
character (`ideology`/`expansionism`/`economic_focus`), procedural phoneme naming.
Stores: `world.nations`, `world.tile_to_nation`. Verified on Kepler: 10 nations,
0 ocean claimed, 0 empty, territory 108–1843 tiles.

**Corporation generation** (`generate_corporations`): five passes — nation
assignment weighted by `economic_focus` + balancing, focus draw biased by home
nation, collision-checked starting-asset placement (focus→building_type, seeded
from existing `w.buildings`), seeded capital with processing/trade premium,
corporate naming. Sets `world.player_entity` to the flagged player corp. Verified:
8 corps, exactly 1 player, all homed, all placed a collision-free asset.

**Tile tuning** (`tile_generation.cpp`): Selene icy 52%→33% (cold band outer
50%→30% of rows); landform prominence (mountain/rift rings 2→3, crater 1→2,
`scale_to_area` ref 1800→1200); Kepler Pass 2 `bias_amp` 0.15→0.07.

### In-session decisions

- **Filed nation/corp generation under Environment** (not Diplomacy), per the
  user — keeps the v0.0.3 world-generation theme coherent; the items note they
  *seed* the deferred Diplomacy/Budget layers.
- **Tile→nation ownership lives in a `world.tile_to_nation` map, not a
  `tile_component` field** — keeps the nation group's file scope disjoint from the
  tile-tuning group so they could run concurrently.
- **Fixed a latent compile trap:** `nation_component`/`corporation_component`
  give members the same name as their enum type; default initialisers must use
  global-scope qualification (`::ideology::mercantile`) or the member shadows the
  type.

### Left open

- **Orphan-island assignment** — the cardinal-adjacency BFS can't cross water, so
  ~708/6048 (~12%) of Kepler land (disconnected islands) stays unclaimed.
  Defensible; a nearest-nation post-pass would close it if full coverage is wanted.
  Logged in TODO.md § Environment → Nation generation.
- **Kepler forest/wetland** still modest (~0.9% / ~0.5%) after the bias cut —
  improved but a candidate for one more eyeball-tuning nudge.

---

## 2026-06-14 — Selection info element + single/double-click model

**Status:** Complete (code). Builds clean (Debug, no warnings). Not yet visually run — the panel is hidden until a selection is made, so a no-input screenshot would not show it.

### What was built

Design first: new **`docs/ui/SELECTION.md`** (the element, the three interaction
states, the click-model change, the polymorphic content/'go-to' table, the
shared-builders abstraction); **glossary** entries for the **Active / Focus /
Selection** states; a **`LAYOUT.md`** region; a **`CANVASES.md`** click-model
call-out; and the doc indexed in `CLAUDE.md`. The original single TODO item was
split into six scoped sub-items (A–F) with a dependency/parallelisation note.

Implemented A–D against that design:

- **A — selection state (`src/ui/selection.{hpp,cpp}`, `ui_state.hpp`).** New
  `ui_state::selected_entity` (and `selection_hidden_for` for the close-button
  hide), kept distinct from the `active_*` navigation anchors. `selection_kind`
  enum + `selection_kind_of(w, id)` resolver probing the world maps in the same
  order as `focus_on_entity`.
- **B — click model (all three canvases).** Single left-click **selects**
  (`selected_entity`, null clears on empty space; no view change); **double**-click
  **navigates** (the former descend/focus). Minimap ascend stays single-click.
  The on-canvas highlight ring now follows `selected_entity` (was `active_body` /
  `active_tile`) so a single click gives immediate feedback.
- **C — shared content builders (`src/ui/entity_summary.{hpp,cpp}`).** Per-kind
  stat blocks (`draw_{body,tile,building,market,unit}_summary`), content-only and
  stale-id tolerant, built on the presentation layer. `body_type_name` /
  `building_type_name` centralised into `presentation.{hpp,cpp}` (de-duplicating
  the copies in the Tile Ledger and — during integration — all three canvases).
  The Tile Ledger reuses the shared names; its multi-tile table stays as-is.
- **D — the panel (`src/ui/selection_panel.{hpp,cpp}`).** Pinned bottom-left
  above the overlay strip, hidden until a valid selection exists. Header: title +
  kind, a **'go to'** button (`focus_on_entity`) and a **close** button (hides
  until the next selection). Dispatches on `selection_kind` to the C builders.
  Wired in `app.cpp`.

### In-session decisions

**Built in dependency order A → {B, C} → D**, with **C run as a parallel
background sub-agent** while A/B were done in the foreground (the two independent
roots: C's builders don't depend on A's state field; only D needs both). The one
predicted collision — C adding `ui::body_type_name` while the canvases (B's
files) kept their own local copies — surfaced as an ambiguous-call build error
and was resolved at integration by deleting the three canvas-local copies and
adding the `presentation.hpp` include. This is exactly the seam the TODO note
flagged; keeping B and C to disjoint files made it a clean, single-point fix.

**Selection ring vs. active anchor.** The canvas highlight was repointed from the
navigation anchor to `selected_entity`. Selecting a moon now rings the moon
rather than the always-anchored planet — the intended selection feedback.

### Open items

- **E — non-spatial 'go to' routing** (nation/corporation → ledger) and **F —
  canvas hit-testing for buildings/units/markets**: left as TODO sub-items.
  E is not actionable until those entity kinds exist; F until those entities are
  drawn as selectable canvas markers. 'Go to' currently routes everything through
  `focus_on_entity` (spatial only).
- **Per-kind title icon** deferred to the hover-card work (shares the builders).
- **Overlay-strip stacking** uses a fixed 40 px offset to sit the panel above the
  lens strip — prototype-grade, like the rest of the shell chrome.

## 2026-06-14 — Two-axis terrain model + six-pass procedural generation

**Status:** Complete (code). Builds clean; validated with a throwaway headless stats harness (since removed).

### What was built

**`docs/generation/TILE_GENERATION.md`** (relocated)
Moved out of `docs/development/` into a new `docs/generation/` area. References updated in `CLAUDE.md` and `docs/economy/TILES.md`. Implementation notes refreshed to point at the new code module and record the deviations below.

**`src/world/components.hpp`** (data model)
`resource_type` expanded from 4 to 19 values: the full Tier-1 raw set (Earth-sourced + space-sourced + ambient) plus the prototype Tier-2 refined goods, ordered by tier per `RESOURCES.md`. `terrain_type` (5-value flat enum) replaced by the two-axis model: `terrain_composition` (11 values) and `terrain_landform` (7 values), both carried on `tile_component` (the old `terrain` field is gone).

**`src/world/tile_generation.{hpp,cpp}`** (new)
The deterministic six-pass pipeline: (1) cylinder-sampled simplex heightmap, X-wrap seamless; (2) latitude-biased ocean threshold; (3) latitude bands + independent moisture map; (4) composition by (band, moisture) for atmospheric bodies, dedicated airless/metallic tables otherwise, with a geology-driven volcanic overlay; (5) BFS landform clusters (mountain/rift/crater) plus a low-ground valley fill; (6) per-tile deposits keyed on (composition, landform) with the ambient every-tile guarantee. Bodies are described by a `body_profile` (temperature/atmosphere/hydrology/geology/water_fraction/bias) — the passes branch only on these, never on body identity. A `body_profile`-driven `generation_record` out-param optionally captures per-pass intermediates (heightmap, moisture, bands, ocean threshold) as the seam for a future generation Ledger.

**`src/world/hard_coded_world.cpp`** (rewired)
Old inline weight-table generator removed. The four prototype bodies now carry authored solar profiles: Cinder (scorching/airless/high-geology), Kepler (temperate/thick/liquid 0.60/moderate), Selene (cold/airless/polar-frozen), Pallas (cold/airless/metallic-bias). Kepler market re-authored against the new resource indices via a small `resource_array` helper.

**UI** — `terrain_colour` (now keyed on composition, 11 cases) updated in `body_surface_canvas.cpp`; `composition_name`/`landform_name` centralised in `presentation.{hpp,cpp}` and used by the canvas tooltip and Tile Ledger (the inspector gained a Landform column); resource presentation table expanded to 19 entries.

### In-session decisions

**Expanded the resource enum now (not deferred).** Faithful Pass 6 needs petroleum, water, agricultural produce, and the ambient resources, which the old 4-value enum lacked. Chosen over an interim mapping to avoid throwaway deposit work; the deposit arrays already span the full enum width so future resource authoring needs no generation change.

**Seed counts scale with grid area (deviation from the doc tables).** The doc's absolute landform seed counts collapse to ~0% coverage on the prototype's 180×84 grids. `scale_to_area()` scales counts up with grid area (never below the authored count, so small bodies are unaffected) to keep feature *density* consistent. Absolute feature prominence/size remains a tuning knob — landform clusters are still intentionally tight per the doc's ring transitions.

**Hazard/habitability derived, not authored.** The design tables don't specify these, but `tile_component` carries them and the inspector shows them, so they are derived from a composition base ceiling modified by landform (mountain/rift raise hazard and cut habitability; valley raises habitability).

### Open items / flagged for tuning

- **Generation Ledger** — filed in TODO.md. The `generation_record` hook exists; the Ledger's persistence model (capture vs. regenerate-on-demand) and UI are unstarted by request.
- **Landform prominence** — features are small/sparse by the doc's cluster rules even after area-scaling. Dial cluster radius / seed density up if more prominent ranges are wanted.
- **Kepler biome balance** — the equatorial ocean bias (`bias_amp = 0.15`) drowns most tropical/subtropical land, so forest/wetland are sparse (~1% / ~0%). Lower the bias to widen the habitable belt.
- **Selene ice fraction** — ~52% icy, because the `cold` polar band spans the outer 50% of rows (doc-consistent) and all polar rows ice over on a polar-frozen body. Narrow the cold polar band or use a tighter polar-cap override if a smaller ice cap is wanted.

---

## 2026-06-14 — Economy design expansion: tiles, population, ambient resources, logistics note

**Status:** Complete (documentation only). No code changes this session.

### What was built

**`docs/economy/TILES.md`** (new)
Two-axis terrain model: `terrain_composition` (barren, rocky, volcanic, icy, tundra, grassland, forest, wetland, ocean, regolith, metallic) × `terrain_landform` (plains, highland, mountain, canyon, valley, crater, rift). Each combination has documented deposit profiles, build cost modifiers, and amenity potential. The current single `terrain_type` enum is redesigned as two separate fields on `tile_component`; existing hard-coded data does not need immediate retrofit but a generation update is filed in `TODO.md`.

**`docs/economy/POPULATION.md`** (new)
Population centres as a formal concept. Scale/agglomeration bonus model (Outpost → Metropolis: +5% to +50% processing throughput, +0% to +20% extraction yield). Land-use state system (undeveloped, extraction, urban, amenity, infrastructure): placing urban or amenity development on a tile permanently sacrifices its extraction potential — the mechanism preventing simultaneous maximisation of raw extraction and finished goods. Population demand basket (food rations, clean water, consumer goods, habitability goods). All deferred from prototype; existing fields (`habitability`, `workforce_assigned`, `market_component.demand`) are already positioned correctly.

**`docs/economy/RESOURCES.md`** (updated)
Added "value tracks" framing alongside tiers (industrial / ambient / habitability). Added Tier 1 ambient resources section: stone, timber, sand, clay, peat — every eligible tile generates a low baseline deposit of at least one ambient resource. Added habitability goods section: clean water, building materials, consumer goods, medical supplies, utilities. Prototype resource count stays at seven; full enum target revised to approximately 35–40 entries.

**`docs/economy/PRODUCTION.md`** (updated)
Extraction building table expanded: Quarry (stone/sand/clay), Lumber Camp (timber), Surface Extractor (regolith/PGMs, Era 1). Added amenity buildings section (Park, Recreation Facility, Cultural Centre) and habitability production buildings (Water Treatment Plant, Construction Yard, Consumer Goods Factory, Pharmaceutical Lab, Power Plant). Added logistics open design note: transport capacity caps supply throughput, not price — oversupplied markets that cannot ship simply stop accumulating rather than crashing in price. Storage Depot added as an infrastructure building placeholder.

**`docs/development/TODO.md`** (updated)
New Environment category with a [4] item covering the full tile generation update: enum rename/expansion, terrain_landform field, revised BFS water logic (variable-width band seeding, polar ice/tundra at poles), per-composition deposit profiles, ambient resource baseline, and landmark landform pass.

### In-session decisions

**Every tile must have at least one deposit.** Ambient resources (stone, timber, sand, clay, peat) fill this role. They have low base prices but ensure no tile is economically inert. Quarry and Lumber Camp extract them.

**Transport capacity constrains throughput, not price.** If a body cannot export its output, production stalls before the price crashes. This is a key design nuance that affects how the market model is implemented at Layer 4 and how ports/warehouses matter at Layer 5. Marked open; to be decided when Layer 5 is designed.

**Population centres are a formal system, not a modifier.** Population has a scale model with named tiers and explicit land-use trade-offs. The "can't fully develop raw extraction and finished goods" constraint is structural — urban tiles are not also extraction tiles.

**Habitability is a separate value track.** Habitability goods and amenity buildings affect workforce efficiency and population growth indirectly, not profit directly. They are worth producing for productive system health, not for market margins.

### Open items

- Tile generation update (filed in TODO.md [4]).
- Full enum expansion for terrain_composition, terrain_landform, and the extended resource list — to be done at the start of Layer 3 implementation.
- Logistics throughput model — open design decision, deferred to Layer 5.
- Population centre implementation — deferred post-prototype.

---

## 2026-06-14 — Layer 3 economy design: resources, production, eras

**Status:** Complete (documentation only). No code changes this session.

### What was built

Three new documents under `docs/economy/` establishing the production system prior to Layer 3 implementation.

**`docs/economy/RESOURCES.md`**
Full resource list: 23 resources across three tiers. Tier 1 — raw materials (11): seven Earth-sourced (iron ore, coal, petroleum, silica, copper ore, rare earth ore, agricultural produce) and four space-sourced (water, iron-nickel ore, platinum group metals, regolith). Tier 2 — refined goods (7): steel, refined fuel, silicon, refined copper, REE alloy, liquid oxygen, food rations. Tier 3 — products (5): machinery, electronics, propellant, alloys, spacecraft components. Regolith is a special non-traded local-use resource. Prototype subset is seven resources (four raw, three refined); all 23 enum values are defined from the start.

**`docs/economy/PRODUCTION.md`**
All building types and recipes. Extraction: Mine (hard minerals), Oil Platform (petroleum), Farm (agricultural produce), Ice Extractor (water from ice — Era 1). Output rate is `deposit × workforce_assigned × (1 − hazard_level)`. Processing: Smelter, Refinery, Chemical Plant, Electronics Lab, Fabricator, Food Processor, Assembly Plant — each supports one or more named recipes. Infrastructure (designed, not yet implemented): Port, Launchpad, Orbital Port, Warehouse. Workforce model documented: `workforce_assigned` is a constant in the prototype; the full policy allocation system is deferred. Layer 3 scope: seven prototype resources, three processing buildings (Smelter/Refinery/Food Processor).

**`docs/economy/ERAS.md`**
Formal era system. Era 0 (Terrestrial) starts at campaign epoch 1 January 1960: heavy industry, no space access. Era 0→1 gate requires Rocketry research + a staffed Launchpad + minimum propellant reserve. Era 1 (Early Space) unlocks all solar system bodies, the Ice Extractor, Assembly Plant, and Orbital Port; the dominant challenge is closing the propellant loop via ISRU. Era 2+ is stubbed.

**Existing documents updated:**
- `CLAUDE.md` — three new economy doc entries added to the reading list.
- `docs/GLOSSARY.md` — Building, Era, ISRU, Recipe, Resource, Stockpile defined.
- `docs/development/INITIAL_INSTRUCTIONS.md` — Layer 3 description expanded to reference `RESOURCES.md` / `PRODUCTION.md` and the seven-resource prototype subset.

### In-session decisions

**Start date 1960 confirmed.** The "early industrial, space locked" era choice validates the existing campaign epoch. 1960 signals post-WWII heavy industry and a nascent-but-real electronics sector; the specific year is fictional in the game's alternate timeline.

**Specialised extraction buildings.** Each extraction type targets a specific resource class (Mine → hard minerals, Oil Platform → petroleum, Farm → agricultural produce, Ice Extractor → water ice). The Mine is the broadest type: its output is determined by the tile's deposits, so the same building type works on both a volcanic Kepler tile (rare earth ore) and a metallic asteroid tile (iron-nickel ore).

**Generic processing via recipes.** Processing buildings are differentiated by their active recipe, not by enum value. A Refinery can produce refined fuel, silicon, refined copper, or REE alloy depending on configuration. This keeps the building type list manageable while supporting the full recipe table.

**Three-tier chain, Tier 3 deferred in prototype.** The full three-tier chain (raw → refined → product) is documented but only Tiers 1 and 2 are implemented in the prototype. Tier-3 product recipes exist in the design but have no authored tile deposits or live buildings until a later pass.

**Eras are a formal system.** "Era" is a first-class game concept with defined gates, not an informal phase description. This affects future tech-tree design, tutorial pacing, and progression gating.

**Food is a resource.** Agricultural produce (Era 0) is extracted by Farm and processed into food rations by the Food Processor. Off-world workforce consumes food rations; a shortage reduces effective workforce at the destination. Food production on Earth is easy; it becomes a critical logistics challenge once off-world colonies exist.

**Propellant is the Era 0→1 gate good.** The Launchpad requires a propellant reserve to operate. Propellant is produced from refined fuel + liquid oxygen. Liquid oxygen requires water (Era 1 input), making the first propellant stockpile the tightest resource bottleneck in early play.

**Prototype enum scope.** All 23 resource types and all building type enum values are defined in code from the start of Layer 3. Unimplemented resources have zero deposits; unimplemented buildings have no authored placements. No retrofit needed when expanding to the full set.

### Open items

- Prototype implementation of Layer 3 (extraction logic, processing logic, ImGui panel showing stockpile changes).
- Lua recipe file structure: decide whether each recipe is a top-level table in `scripts/` or inline in a resource/building definition file.
- Workforce policy allocation (how the player redistributes `workforce_assigned` in real time) — deferred, but the field is in place.
- Deposit depletion model — deferred; the prototype treats deposits as infinite.
- Rocketry tech unlock — designed (ERAS.md), not implemented; requires a skeleton tech system.

---

## 2026-06-14 — Calendar polish + two-column time panel + TODO recategorisation

**Status:** Complete. Builds (Debug, MSVC) and links; default view captured (and a
zoomed crop of the time panel) to confirm the two-column layout renders.

### What was built

**Compact calendar formatters.** Dropped yesterday's `short_date` (`dd/mmm/yyyy`).
The readout is now built from two pieces: `1960 Q1` (year + quarter, formatted
inline from `calendar_date`) and `Jan 01` (`ui::fmt::month_abbrev` + zero-padded
day). The epoch is unchanged (day 0 = Jan 01 1960). A brief intermediate
`long_date` (`January 1st 1960`, with full month names + ordinal) was tried and
then removed when the layout settled on the compact two-line block; only
`month_abbrev` remains.

**Quarter progress bar.** Replaced the `Q1 - Day N` text with an ImGui
`ProgressBar` driven by `ui::fmt::quarter_progress(day)` (0..1 through the 90-day
in-year quarter), labelled with its percentage. Since the economy resolves on the
quarter boundary, the bar doubles as a countdown to the next economy tick.

**Two-column time panel.** The two stacked top-right panels (system tick readout +
speed controls) are merged into one `##time_panel` window, split 25% / 75% via a
two-column stretch table: a left calendar block (`1960 Q1` / `Jan 01` / progress
bar in three rows) and the compressed speed controls on the right (`Sim` counter +
the pause/1–5 buttons). The panel now takes input (it was previously NoInputs);
the explorer's top is keyed off the single panel height (`mm_h * 0.5`).

**TODO recategorisation.** `docs/development/TODO.md` now declares an explicit
category set — UI categories (Canvas, Menu, Ledger, Documentation, Known Bug) plus
game-system categories mirroring `SYSTEMS.md` — and every item sits under exactly
one. Existing items were re-homed (selection-info ledger → Ledger, hover-card →
Canvas, label-stepping → Known Bug, menu-definition → Menu). The per-session
changelog paragraph was dropped (the items stand alone; session history lives
here). Three new items were filed: Circumplanetary 0.3 AU max zoom (Canvas, [1]),
map-lens icons (Canvas, [2]), Tile Ledger default body from the active view
(Ledger, [2]).

### Open items

- The three new TODO items are recorded, not implemented.
- Combined-panel verified on the default (Planetary) view; an interactive pass over
  the quarter bar advancing across an economy tick is still worth a look.

---

## 2026-06-13 — TODO follow-ups: zoom/scale, calendar, highlight ties, overlay controls, icon nav

**Status:** Complete. Builds (Debug, MSVC) and links. Worked through every
`TODO.md` item at difficulty 3 and below — the follow-up revisions raised against
the building-blocks session — leaving the difficulty 4+ items (selection-info
ledger, stepped-label bug) and the deferred (difficulty 6) work.

### What was built

**[3] Zoom slider direction + shared Circumplanetary scale/zoom.** The Solar zoom
slider was reversed (dragging right zoomed *out*). Factored the scale-bar + zoom-
slider block out of `solar_system_canvas.cpp` into a shared
`ui::draw_scale_zoom_overlay` (`src/ui/canvas_scale.{hpp,cpp}`); both the Solar
and Circumplanetary canvases now call it. The slider now drives the zoom factor
directly on a logarithmic track, so **right = zoomed in, left = zoomed out**, and
shares its `[zoom_min, zoom_max]` bounds with the scroll-wheel handler on each
canvas. The Circumplanetary canvas gained the scale bar + zoom slider it lacked.

**[2] Date format & epoch.** `ui::fmt::short_date` switched from `Y1 M05 D12` to a
`dd/mmm/yyyy` form with month abbreviations (`01/Jan/1960`); added a month-abbrev
table + `month_abbrev`, and a `campaign_epoch_year = 1960` so day 0 is `01/Jan/1960`
(`calendar_date::year` is now the calendar year, not a 1-based campaign year). The
in-year quarter readout is unchanged.

**[2] Highlight resolution on ties.** Overlapping markers each drew their own hover
ring (the per-entity hit-test set `this_hovered` independently). Each canvas now
runs a hit-test pass that resolves a **single** hovered entity — nearest centre to
the cursor, entity id breaking exact ties (arbitrary but stable) — before drawing,
so a tie highlights one entity. Solar and Circumplanetary resolve the hovered body
up front; the Planetary canvas defers the hover outline to the single nearest hex
copy (selection still draws on every visible wrap copy). Documented the convention
on `resolve_highlight` (tie resolution is the caller's responsibility).

**[3] Relocate overlay controls + default lens.** Removed the minimap **mode bar**
(the three overlay-mode dots) — the inset now uses the full height under the title.
Added `ui::draw_overlay_controls`, a bottom-left strip of labelled mode buttons
(Supply / Market / Faction) running from the nav-rail edge inward, clear of the
centred scale/zoom control. `ui_state::overlay` now defaults to `supply` rather
than `none`. The on-canvas legend chip was dropped (the strip names the active
lens); `draw_canvas_overlay` is now a no-op extension point for real lens geometry.

**[3] Narrower, icon-based nav rail.** `nav_pane_width` 200 → 56; the pane is now an
icon rail of ten square slots, each a glyph (`src/ui/icons.hpp`) with the menu name
in a hover tooltip instead of a worded label. Added `icons::ledger` (ruled-table
glyph, for the wired Tile Ledger slot) and `icons::placeholder` (hollow square, for
reserved slots). Decoupled the profile from the rail width — added
`profile_panel_width` (200) so the profile and the header stay wide while the rail
is narrow; the header now starts at the profile's right edge, and the Tile Ledger
window spawns clear of the profile/header.

### In-session decisions

**Slider drives zoom directly, not visible-AU.** The old slider edited a derived
"visible AU" value (0.5–50) inverted relative to zoom, which is why it read
backwards. Driving `zoom` directly on a log track makes the direction obvious and
lets the slider and wheel share one `[zoom_min, zoom_max]` range per canvas.

**Profile width decoupled from the nav rail.** The profile previously aligned to
`nav_pane_width`; narrowing the rail to 56 px would have crushed the portrait +
name. Gave the profile its own `profile_panel_width` so the rail can be an icon
column without distorting the identity panel above it.

### Open items

- Per-menu nav icons are placeholders (one ledger glyph + a generic reserved
  glyph) until the menu set is defined (TODO `[6]`).
- Changes verified by build + code review; an interactive click-through / screenshot
  pass of the new slider direction, overlay strip, and icon rail is still worth doing.

---

## 2026-06-13 — Pre-Layer-3 UI building blocks + asteroid belt (label-shimmer fixed)

**Status:** Complete. Builds and runs (verified after each item; solar view
captured to confirm the belt and the new font render). Worked through every
`TODO.md` item below difficulty 6 ahead of Layer 3, leaving only the two
deferred (difficulty 6) items — the hover-card system and the menu definition.

Eight items, each a small focused module under `src/ui/`, built and
build-verified one at a time. Foundational/shared primitives first so later
items reuse them; the meatiest (asteroid belt) last.

### What was built

**[2] Value & date formatting helpers — `src/ui/format.hpp` / `.cpp`.**
`ui::fmt` with `abbreviate` (1.2k / 3.4M / 5.0B), `credits` ("Cr 1.2k"),
`signed_delta` (+/−/"±0"), `rate` ("+1.2k / qtr"), `percent`, `sign_of`, and the
deferred **calendar** (`date_from_day` / `short_date`). The calendar completes
sim_loop's tentative constants (30-day months, 3-month quarters) with a defined
4-quarter / 360-day year. The system-tick readout now shows `Y1 M05 D12` and
`Q2  -  Day N` instead of raw counts, and the header budget/stockpile
placeholders route through the formatters so live numbers drop straight in.

**[3] Presentation metadata — `src/ui/presentation.hpp` / `.cpp`.**
Single source of truth for resource identity (`resource_presentation`: name,
abbreviation, identity colour) replacing the duplicated `resource_labels[]` in
`tile_inspector.cpp` and `body_surface_canvas.cpp`. Plus a **semantic palette**:
positive/negative/neutral (deltas), selection/hover/pinned (interaction), and
six reserved faction colour slots (the data model already permits multi-faction).
`value_colour(...)` maps a signed value to its palette colour. Demonstrated in
the Tile Ledger market table (resource colour swatch; price coloured by its move
vs. base_price).

**[2] Shared selection / hover / pinned highlight convention — `src/ui/highlight.hpp` / `.cpp`.**
`resolve_highlight(selected, hovered, pinned)` (precedence selected > pinned >
hovered) plus `draw_body_highlight` / `draw_hex_highlight` using palette colours.
All three canvases refactored onto it; this **adds a hover ring** (light blue) on
bodies and tiles, previously only a tooltip. Pinning is reserved in the
vocabulary (amber) but not yet driven.

**[2] Focus-on-entity helper — `src/ui/view_nav.hpp` / `.cpp`.**
`focus_on_body` / `focus_on_surface` / `focus_on_tile` / `focus_on_entity` — one
call that selects an entity, chooses the rung that frames it, and centres that
rung. Rung rule: a *body* frames in its orbital/local view (star → Solar,
planet/asteroid/station → its own Circumplanetary, moon → parent's with the moon
selected); something *on* a surface → that body's Planetary surface. The opening
view now goes through `focus_on_surface` rather than poking `ui_state`.

**[3] Render-time interpolation building block — `src/ui/interp.hpp` / `.cpp`.**
The decided read path for fractional-progress entities before convoys exist:
`lerp` (float/ImVec2), an `interpolated<T>` (previous/current with `advance` and
`at(alpha)`), and `econ_tick_alpha` / `day_alpha` from the continuous
`elapsed_days` clock. Layer 5 convoys will hold `interpolated<float> progress`
and read `lerp(route_start, route_end, progress.at(econ_tick_alpha(...)))` to
glide across the quarter instead of jumping at the boundary. No consumer yet
(documented building block).

**[4] Canvas overlay layer + mode switching — `src/ui/overlay.hpp` / `.cpp` + `ui_state`.**
`overlay_mode { none, supply, market, faction }` in `ui_state`; an overlay draw
pass (`draw_canvas_overlay`) over the primary canvas, below the chrome; and the
minimap **mode bar made interactive** — three dots toggle the three lenses
(click the active dot to clear). No overlay data yet, so an active lens draws
only a bottom-left legend chip naming itself — the single extension point where
Layer 5 supply routes etc. will hang their geometry. Mode bar height bumped 10→14
px so the dots are clickable.

**[3] Icon/glyph + font-atlas strategy — `src/ui/fonts.*`, `src/ui/icons.*`.**
Decided with the developer: **vector glyphs via the draw list** (no font asset)
and a **system TTF loaded with oversampling** (`OversampleH=3`, `PixelSnapH=false`,
candidates under `C:/Windows/Fonts`, falling back to an oversampled built-in
font). The oversampled atlas improves glyph crispness. **Note:** this did *not*
resolve the label-motion artefact — the developer reports body labels still
advance in steps every few ticks rather than gliding, so the bug is re-logged in
`TODO.md` (Known bugs) with that sharper symptom; the cause appears temporal, not
purely sub-pixel rasterisation. `ui::icons` draws building
(diamond/square/triangle), resource pip, and unit chevron glyphs; the Planetary
built-tile marker now uses the building-type glyph instead of a uniform white
square.

**[4] Asteroid belt as a textured ring — `world.belt`, `hard_coded_world.cpp`, `solar_system_canvas.cpp`.**
A `world::belt` (`asteroid_belt` { inner/outer radius }) — system-level data,
**not a body**. The Solar canvas renders it as a translucent annulus (thick ring
stroke) with a deterministic fixed-seed scatter of dusty specks (positions in AU
space, so it pans/zooms with the view and holds still between frames). One
**notable asteroid** — Pallas (the prototype keeps a single belt body per the
developer's call) — is an ordinary `body_type::asteroid` entity at a radius
inside the band, drawn *over* it in the normal body pass, so it stays
hoverable/labelled/selectable and carries a small 30×14 tile grid (no water) so
its surface is explorable. The Solar auto-fit extent now includes the belt's
outer radius.

### In-session decisions

- **Font: system TTF + oversampling (developer choice).** Crisp, fluid labels
  with no committed binary asset; Windows font path for now (prototype is
  Windows-only). A bundled `assets/fonts/*.ttf` can be prepended to the candidate
  list later for portability without touching call sites. Default size 16 px.
- **Icons: vector glyphs via the draw list (developer choice).** Crisp at any
  zoom, no atlas/licensing, matches how the canvases already draw.
- **Notable asteroids are separate bodies drawn over the band**, not markers
  embedded in the ring — resolving the open question in the belt TODO. Keeps them
  uniform with every other body (selectable, labelled, own surface).
- **Overlay building block lands now, data later.** The mode bar drives a real
  `ui_state.overlay` + draw pass; with no economic data yet it shows only a
  legend chip. This reserves the mechanism so Layer 5 adds geometry, not plumbing.
- **Highlight precedence selected > pinned > hovered**, single ring per entity, so
  a pinned entity keeps its identity colour under the cursor and selection always
  wins.

### Open items

- **Overlay lenses have no data.** The pass and toggle exist; supply-route
  geometry (Layer 5) is the first real consumer.
- **Pinning is unwired.** The highlight convention and `focus_on_entity` are
  ready, but nothing sets a "pinned" state yet (the Explorer is still a
  placeholder).
- **Interpolation has no consumer** until convoys exist (Layer 5).
- **Font candidate list is Windows-only** — fine for the prototype; revisit for
  cross-platform with a bundled font.
- **Asteroid surfaces** use the generic tile generator at a small grid; proper
  asteroid terrain/deposit authoring is for the extraction layer.

---

## 2026-06-13 — version 0.0.2 — Layer 2 finalisation (standardised body grids, infinite side-scroll, zoom floor)

**Status:** Complete. Builds and runs. **Tagged snapshot.**
**Backup:** `backups/v0.0.2/` (copy of `src/` at this version — the rollback point for v0.0.2; previous snapshot is `backups/v0.0.1/`).

The hard-coded world is pared to three surface bodies on standardised grids, the Planetary canvas scrolls horizontally without bound, and its zoom floor is derived correctly. Layer 2 is considered finalised at this version.

### What was built

**Standardised body grids (~9:5 width:height).**
`make_hard_coded_world` now sizes the two planets at **180 × 84** (columns × rows) and Selene (Kepler's moon) at **90 × 42** — the same ratio at half scale. The height is a little under half the width by design: the width spans the full circumference (both hemispheres) and the height is pole-to-pole with the non-traversable polar caps truncated. The stale generation TODO (which described the rule as unenforced) was replaced with a comment recording the settled ratio.

**Backdrop bodies removed.**
With the canvas perspectives settled, every body except Helios, Cinder, Kepler, and Selene was deleted from the world: Veld, Ochre, Vesta, Ceres, Pallas, Bastion, Forge, Cyra, Halo, Mote. Vesta's hand-authored tiles, extraction site, and market went with it. The now-unused `create_simple_body`, `tile_spec`, and `create_tile` helpers were removed; `hard_coded_world.hpp`'s doc comment was rewritten to list the three surviving surface bodies.

**Infinite horizontal scroll on the Planetary canvas.**
`draw_body_surface_canvas` now draws each tile at every integer wrap offset `k` whose copy falls within the canvas, where the grid repeats every `period_px = gw * col_step * zoom`. The `k`-range is derived per tile from the visible x-extent, so panning past either edge continues seamlessly from the far side with no seam and no special-casing of "three offsets". Hit-testing runs inside the same copy loop, so the hovered/clicked column is always correct regardless of wrap. Horizontal pan is wrapped with `fmod(pan_x, period_px)` each frame to stop `pan_x` drifting without bound — visually identical because the grid is periodic.

**Planetary zoom floor derived from the height-normalised zoom.**
The minimum zoom was a guessed constant (`0.2f`) unrelated to the zoom definition, so it let the grid shrink to ~19% of the canvas height (viewport showing ~525% of the grid). Since zoom is normalised so the grid fills `kFitMargin` (0.95) of the canvas height at zoom 1, the floor is now derived: `kMinZoom = 1 / (kMinZoomHeadroom * kFitMargin) ≈ 0.877`, where `kMinZoomHeadroom = 1.2` means the viewport spans ~120% of the grid height at minimum zoom (full grid + ~20% headroom). Max zoom (`kMaxZoom = 20`) is unchanged; the stored zoom is clamped to `[kMinZoom, kMaxZoom]` each frame as well as in the wheel handler.

### Docs

- `PLANETARY.md` updated: new "Target size and aspect ratio" wording (180×84 / 90×42, the 9:5 rationale, three-body world), the horizontal-wrap and interaction sections describe the seamless side-scroll, and the deferred table now lists only seam *visualisation* (an explicit wrap marker) as post-prototype.
- `TODO.md` gained a **"UI building blocks (decide before Layer 3)"** section capturing the rendering primitives Layers 3–6 will need but Layer 2 simplified: a tooltip/hover-card system (recorded then **deferred to difficulty 6** at the developer's call), centralised resource/palette presentation metadata, shared value/date formatting, a canvas overlay layer + mode-bar wiring, an icon/font-atlas strategy (folds in the label-shimmer fix), a shared selection/hover/pinned highlight convention, a "focus on entity" view-navigation helper, and render-time interpolation for fractional-progress entities (convoys). The stale Vesta/Ceres/Pallas reference in the asteroid-belt item was corrected to note the backdrop bodies are now removed.

### Versioning / backups

- This session is tagged **version 0.0.2**; `src/` is snapshotted to `backups/v0.0.2/` as the rollback point. DEVLOG headings now carry an explicit version marker + Backup line for any tagged snapshot (see the note at the top of this file). Decision on whether to split DEVLOG into per-version files: **kept as a single newest-first file** — version markers make rollback points easy to find by search, and one file preserves chronological review and grep across the whole history. Revisit only if the file becomes unwieldy.

### Open items

- Procedural generation still seeds water from the centre row only; with the taller 84-row grids the polar caps are land by default. Whether the caps should read as ice/barren rather than ordinary land terrain is unaddressed.

---

## 2026-06-13 — Canvas zoom ladder + Circumplanetary canvas

**Status:** Complete. The two-canvas binary swap is replaced by a three-rung zoom ladder (Solar → Circumplanetary → Planetary) with click-to-descend / minimap-to-ascend navigation. Builds and runs; the game opens on the home planet's surface.

### What was built

**Circumplanetary canvas (new middle rung).**
`src/ui/circumplanetary_canvas.{hpp,cpp}` — a top-down view of a single planet (the *anchor*) and its moons: the anchor at centre (enlarged), an orbital ring and dot per moon, selection outline on `active_body`, hover tooltips, and primary-only pan/zoom (`circum_zoom`, `circum_pan_x/y` in `ui_state`). The free function `circumplanetary_anchor(world, active_body)` resolves the anchor — the body itself if it orbits the star, or its parent planet if it is a moon — and is shared with `app::render()` for the minimap title.

**The zoom ladder replaces the binary swap.**
`ui_state::surface_is_primary` (bool) became `ui_state::primary_level` (`enum class canvas_level { solar, circumplanetary, planetary }`). The minimap is now pure **context**: it shows the rung one step *out* from the primary. Navigation:
- **Descend** by clicking a body in the primary canvas (Solar→Circumplanetary, Circumplanetary→Planetary). Clicking a moon on the Solar canvas opens its parent's circumplanetary view with the moon selected.
- **Ascend** by clicking the minimap.

The solar and circumplanetary canvases gained an explicit `bool is_minimap` parameter (their click handling differs between primary and minimap). The body-surface canvas is now only ever primary — its minimap branch and `surface_is_primary` writes were removed.

**Star as a body entity.**
`body_type::star` added to `components.hpp`. `make_hard_coded_world` creates **Helios** at the system centre (radius 0, stationary) and stores it in `world.star_body`. The solar canvas now draws the star through the normal body pass (new star style: 18 px, yellow) instead of a hard-coded circle, labels it, and excludes it from descend clicks (it has no circumplanetary view). Zero-radius bodies are skipped in the orbital-ring pass.

**Home planet start.**
`world.home_body` added and set to **Kepler**. `app::run()` opens with `active_body = home_body` and `primary_level = planetary` — the game starts on the home planet's surface, with Kepler's circumplanetary view in the minimap.

**Minimap chrome.**
`app::render()` now draws a title bar above the inset and a placeholder mode bar (three dim dots) below it, with the inset canvas between. The title names what the minimap shows: the **star name** (primary Circumplanetary), the **planet name** (primary Planetary), or the **game name `Project Io`** at the top rung, where the inset is a dark branding fill (no canvas, non-interactive).

**Docs reconciled to the ladder.**
`CANVASES.md` rewritten (binary swap → three-rung ladder, context minimap, new `ui_state`/signatures). New `CIRCUMPLANETARY.md`. `SOLAR.md` (body click descends; star is an entity), `PLANETARY.md` (surface is always primary), `LAYOUT.md` (canvas area / minimap / companion list), `MINIMAP.md` (top rung = game name), and `CLAUDE.md` (CANVASES entry) updated.

### In-session decisions

**Minimap is context-only; descend via the primary, not the minimap.**
Chosen over an up/down tabbed minimap. The minimap always shows the zoom-out neighbour; the player descends by clicking a body in the primary canvas. This keeps the bottom mode bar free for future overlay modes rather than spending it on level navigation.

**Star as an entity, not a `world.star_name` string.**
Keeps the star uniform with every other body (name, position, style, future selectability) at the cost of a new enum value and a skip in the ring pass. Name "Helios" is a placeholder, consistent with the original body names.

**Open:** the mode bar has no function yet (placeholder); the ladder navigation was verified by screenshot (opens on Kepler surface, minimap shows Kepler + Selene), not yet by interactive click-through of every rung.

---

## 2026-06-13 — TODO triage and UI shell polish

**Status:** Complete. Cleared the difficulty-1/2 TODO items; the asteroid-belt ring (difficulty 4) and the two deferred items (label shimmer, menu definition) remain.

### What was built

**TODO difficulty ratings.**
`docs/development/TODO.md` was annotated with a 1–6 difficulty scale (1 trivial → 5 very hard, 6 deferred). The four easiest items were implemented this session and removed from the list.

**Pause as a toggle.**
The time-controls pause button (`App.cpp`) now toggles. `app` gained `m_prev_speed`: pressing pause stores the current speed and sets speed 0; pressing it again restores the stored speed. Speed buttons 1–5 also update `m_prev_speed` so a later pause/unpause round-trips correctly. The button label flips to `>` while paused and `II` while running, so it reflects its toggle state.

**Quarter label.**
The system-tick readout was relabelled: economy ticks now read `Quarter N` (previously `Econ`, briefly `Q`), aligning with the quarterly econ-tick model in `sim_loop.hpp`.

**Header resource strip simplified.**
`header_panel.cpp` dropped the four named placeholder resources (Ore/Metal/Fuel/Goods) for a single `STOCKPILE 0` aggregate placeholder, per the prototype's "deliberately scarce" header intent in HEADER.md.

**Default solar view ≈ 5 AU + scale bar and zoom slider.**
`app::run()` sets the initial `solar_zoom` so the opening view spans roughly 5 AU (computed from the outermost orbit). The scroll-wheel zoom-out is capped at 50 AU (min zoom derived from `max_radius_au / 50`).

A bottom-centre overlay on the primary solar canvas (`solar_system_canvas.cpp`) replaces the earlier `[-] X.X AU [+]` text row:
- A **fixed-width scale bar** (8% of canvas width) with end ticks. Its label shows the spanned distance dynamically to two decimals (`%.2f AU`) at the current zoom.
- A **logarithmic zoom slider** offset to the right of the bar, ranging 0.5 AU (zoomed in) to 50 AU (zoomed out), with no value text — the bar already reports distance.

The overlay is a borderless, fill-free, padding-free ImGui window anchored so the scale bar is screen-centred and the slider sits to its right.

### In-session decisions

**Scale overlay drawn before the `input_enabled` early-out.**
The scale/slider block sits before the canvas's `if (!input_enabled) return;`. Drawing it after would cause a one-frame flicker loop: hovering the slider sets `WantCaptureMouse`, which disables canvas input the next frame, which would skip the draw. Placing it earlier and building it as a real ImGui window keeps it persistent while still handling its own input.

**Fixed-width bar with dynamic distance, not a round-number bar.**
An earlier pass picked the largest "nice" AU span (0.1/0.2/0.5/1/…) that fit within 8% of the width. Changed to a fixed 8%-width bar whose AU value floats to two decimals — simpler and reads as a steady on-screen ruler whose label changes with zoom.

---

## 2026-06-13 — Layer 2: planetary view, hex tiles, procedural terrain

**Status:** Complete. Horizontal wrap rendering, pan/zoom min/max enforcement, and grid size expansion via procedural generation are the main open items.

### What was built

**Doc restructure — CANVASES.md → SOLAR.md + PLANETARY.md.**
`docs/ui/CANVASES.md` was refactored from a monolithic canvas spec into a thin overview document. Per-canvas detail moved to two new files: `docs/ui/SOLAR.md` (Solar System Canvas) and `docs/ui/PLANETARY.md` (Body Surface / Planetary Canvas). CANVASES.md retains shared concerns: primary/minimap layout, region sizing, selection state struct, and the `input_enabled` dispatch model.

**Hex tiles on the planetary canvas.**
`body_surface_canvas.cpp` was rewritten from a rectangular grid to a **pointy-top hexagonal grid** using odd-r offset coordinates. Tile centres are computed via `hex_local_centre(col, row, hex_size)`. Drawing uses `ImDrawList::AddConvexPolyFilled` for filled hexes and `AddPolyline` for the selection outline. Hit-testing uses distance-to-centre (< circumradius), which is approximate but sufficient for usability. A clip rect prevents hexes bleeding over the title bar or into the solar canvas.

**Water terrain type.**
`terrain_type::water = 4` added to `components.hpp`. Colour: `(40, 80, 160)` deep blue. `terrain_name()` and `terrain_colour()` updated. No deposits, high habitability modifier. Tile data for existing hard-coded bodies is unchanged for now — water placement is deferred until grids expand.

**Pan/zoom on the planetary canvas.**
`ui_state` gained `planetary_zoom`, `planetary_pan_x`, `planetary_pan_y`. Controls match the solar canvas: middle mouse button pans, scroll wheel zooms anchored at the cursor. Both primary view and minimap use the same `planetary_zoom` value so they stay in sync; only the primary applies pan offset.

**Zoom reference frame: fit-by-height.**
The planetary `hex_size` is computed from `fit_by_y` only (canvas height / grid height) rather than `min(fit_by_x, fit_by_y)`. This means zoom=1 is defined as "full grid height fills the canvas," and zoom=4/3 is exactly "3/4 of the grid height visible" — a ratio that holds for any canvas size, including the minimap. Default `planetary_zoom = 4.0f/3.0f`.

**Solar body click no longer switches canvas.**
Previously clicking a body in the solar view set `surface_is_primary = true`. That was changed: a body click now only sets `active_body`. The planetary minimap updates immediately to show the new body; the player navigates to the planetary primary view by clicking the minimap. SOLAR.md updated to document this.

**Procedural tile generation — Kepler, Cinder, Selene.**
`hard_coded_world.cpp` gained a `generate_body_tiles()` function that:
1. Seeds the BFS queue with the **entire centre row** so the ocean grows as a horizontal equatorial band (poles are land).
2. BFS expands with shuffled neighbour order for an irregular coastline, stopping at 60% water coverage. Horizontal wrap is handled in `hex_neighbors()` via column modulo.
3. Land tiles draw terrain from a per-body weighted table (barren/rocky/icy/volcanic). Hazard, habitability, and resource deposits are set by terrain type with mild random jitter.

Three bodies received generated tile grids (replacing the earlier small placeholder grids):
- **Kepler** (Earth analogue): 42 × 174. Barren/rocky dominant, some icy and volcanic. Replaced the hand-authored 4×4 tile table; buildings now attach to the first two land tiles found in raster order.
- **Cinder** (Mercury analogue): 36 × 186. Volcanic dominant (45%), then barren and rocky.
- **Selene** (Kepler's moon): 18 × 92. Barren dominant; smaller grid appropriate for a moon.
- **Vesta** retains its 3×3 hand-authored tiles — small enough to curate manually.

`tile_spec` / `create_tile` helpers are retained for Vesta only. The `generate_body_tiles()` function returns a flat `vector<entity_id>` in raster order (`row*gw+col`) for building placement lookup.

**Two implementation TODOs filed in source.**
- `body_surface_canvas.cpp`: horizontal wrap / infinite scroll — describes the triple-draw approach needed for seamless east/west panning.
- `hard_coded_world.cpp`: generation rules — body ~2× wide as tall (both hemispheres), polar row truncation, zoom min (~12 tiles wide) and zoom max (~12 tiles beyond total grid height).

### In-session decisions

**Pointy-top hexes, odd-r offset.**
Chosen over flat-top because rows read as latitude bands, which aligns naturally with the horizontal ocean / polar land model. Odd rows are shifted right. Column wraps for horizontal continuity; rows do not (poles are boundaries).

**Water is an equatorial band, not a blob.**
Original BFS used a single random interior seed point, producing a roughly radial blob. Changed to seeding the full centre row simultaneously; BFS then expands symmetrically up and down, producing a horizontal ocean with irregular north/south coastlines. This is consistent with the design intent that poles are at the top and bottom of the grid.

**Grid sizes are slightly varied from the 40×180 target.**
The 40×180 ratio is a design guideline, not a precise spec. Sizes were deliberately varied (42×174, 36×186, 18×92) to reflect that real bodies won't be uniform. The aspect ratio (~4:1 for planets, ~5:1 for Selene) is the constraint, not the exact count.

**Zoom reference frame changed from min-fit to height-fit.**
The earlier 4/3 multiplier on the auto-fit zoom had no meaningful effect because the wide planetary grid is always width-constrained. Redefining zoom=1 as "full grid height fills canvas" makes zoom=4/3 a geometrically correct "3/4 height visible" that works at both primary and minimap scale without per-canvas adjustment.

**Both primary and minimap use the same `planetary_zoom`.**
The solar canvas minimap always shows the default (full-system) framing regardless of solar zoom state. For the planetary canvas, the user preference is that primary and minimap are tied — both show the same zoom level. Only pan is suppressed on the minimap (it centres on the grid midpoint).

**Kepler buildings re-attached by raster scan after generation.**
The 4×4 hand-authored Kepler tiles referenced specific entity IDs. After switching to procedural generation the IDs are no longer predictable. Rather than authoring specific target coordinates (which could land on water), buildings are attached to the first two non-water tiles found scanning left-to-right, top-to-bottom. This is an acceptable heuristic for a prototype where building placement logic is deferred.

### Open items

- **Horizontal wrap rendering** — east/west pan currently shows blank space beyond the grid edge. The canvas TODO describes the triple-draw approach.
- **Zoom min/max not enforced** — the generation TODO documents the intended limits (12 tiles wide min, 12 tiles beyond total height max). Currently unclamped beyond a 0.1 floor.
- **Grid sizes for other bodies** — Bastion, Halo, Ochre, Veld, Ceres, Pallas, Forge, Cyra, Mote, Mote retain small placeholder grids (2×2 to 4×4). Expansion deferred until procedural generation is introduced.
- **Water placement in existing small grids** — Vesta and all backdrop bodies have no water tiles. The `water` terrain type exists but is not used in any current tile data.
- **Building placement** — raster-scan heuristic for Kepler is a placeholder. Proper authored placement should be revisited when the extraction layer (Layer 3) designs building site selection.

---

## 2026-06-13 — UI shell placeholders, orbital motion, and canvas pan/zoom

**Status:** Complete. Canvas refinement continues; asteroid belt (as a ring) and the parked UI items remain open — see `docs/development/TODO.md`.

### What was built

- **UI shell docs** — expanded `docs/ui/LAYOUT.md` with profile, header, explorer, minimap, and a UI-popup note, each linking its own spec. New stub specs: `PROFILE.md`, `HEADER.md`, `EXPLORER.md`, `MENU.md`, `MINIMAP.md`, `TIME_CONTROLS.md`. Gated behind LAYOUT.md (not added to the CLAUDE.md authoritative set, by request).
- **Placeholder panels** — `src/ui/profile_panel`, `header_panel`, `explorer_panel`: fixed ImGui panels matching the `nav_pane` style. Profile (top-left, portrait + name placeholder), header (budget + scarce resource strip, zeroed), explorer (empty pin list). `nav_pane` gained a `top_offset` so it sits below the profile. Wired into `app::render()`.
- **Orbital motion** — `body_component` gained `parent`, `orbital_angular_velocity_rad_per_day`. New `src/world/orbital_system`: `advance_orbits` (advances angles by elapsed days, freezes when paused) and `kepler_angular_velocity` (speed from radius via Kepler's third law). `sim_loop` exposes continuous `elapsed_days()`; the app loop advances orbits per-frame.
- **Sol-approximation world** — `hard_coded_world.cpp` rebuilt to ~6 planets (Cinder/Veld/**Kepler**/Ochre/Bastion/Halo, real-AU spacing), 4 parented moons, and 3 belt asteroids (**Vesta** repurposed from moon → asteroid, keeps its tiles/market). Only Kepler and Vesta carry tiles; the rest are backdrop bodies. Default surface selection now prefers a tiled body.
- **Canvas pan/zoom + labelling** — Solar System Canvas gained cursor-anchored scroll zoom and middle-drag pan (primary view only; the minimap stays at default framing). Positions/rings scale with zoom; element sizes do not. Star bumped to 1.5x. Planets/asteroids labelled permanently, moons on hover. View state (`solar_zoom`, `solar_pan_x/y`) lives in `ui_state`. Labels track the live body position; a residual shimmer (bitmap-font sub-pixel artifact) is left unfixed and logged in `TODO.md`.

### In-session decisions

**Moons are parented, not flat orbits.** When asked, chose to add a `parent` field so a moon orbits its planet (composed at draw time) and tracks it as it moves, rather than giving moons their own star orbit (which would drift apart under animation). Moon orbital radii are a small *visible* offset, **not** true scale — real moon distances render on top of the planet.

**Label shimmer is a font-rasterization artifact, left as a known bug.** Bodies move smoothly (continuous `elapsed_days`), but text labels shimmer. Root cause: the default ImGui font is a bitmap atlas with no sub-pixel positioning, so glyphs are crisp only at integer coordinates — an anti-aliased body dot reads smooth at any fraction, but text at the same fractional coordinate shimmers as the fraction changes each frame. Two attempts were explored and reverted: (1) sampling the label position once per sim tick — wrong, it made the label hold still then hop to catch the still-moving body, a positional jump amplified by zoom (the `sim_tick` canvas parameter added for this was removed); (2) rounding the label's screen coordinate to whole pixels — crisp but it made the label step 1px at a time. Final state: the label draws at its live fractional position (fluid motion, residual shimmer). The durable fix is font oversampling / sub-pixel rendering; deferred and logged under *Known bugs* in `TODO.md`.

**Pan/zoom on the solar canvas only.** The Body Surface Canvas keeps "no pan/zoom" (deferred until large procedural bodies exist). Zoom keeps element sizes constant per the request — only framing changes.

**Ledgers start closed (policy).** Codified in `MENU.md` and `LAYOUT.md`: every ledger defaults closed on a fresh session; `show_tile_ledger` now defaults `false`. New ledgers must follow.

**Asteroid belt deferred.** Intended as a single thick, translucent textured *ring* (not orbiting body dots) with ~3 notable asteroids that remain selectable bodies; the belt itself is not a body. Recorded in `TODO.md`; current three asteroids are placeholders.

**Header currency placeholder.** Used `Cr` (credits) rather than a currency glyph — ImGui's default font has no `₡`/symbol coverage beyond ASCII.

---

## 2026-06-13 — Layer 2: Primary canvases

**Status:** Complete. Canvas visual refinement and Layer 3 (extraction and production) are next.

### What was built

- `src/ui/ui_state.hpp` — `ui_state` struct shared by both canvases: `active_body`, `active_tile`, `surface_is_primary`, plus `show_tile_ledger` (owned by the nav pane).
- `src/ui/solar_system_canvas.hpp` / `.cpp` — Top-down system view: star, per-body orbital rings, type-coloured body dots, labels, selection outline, hover tooltip. Draws to the ImGui background draw list. Coordinate mapping per CANVASES.md (y negated, `scale = min_dim·0.45 / max_radius_au`).
- `src/ui/body_surface_canvas.hpp` / `.cpp` — Tile grid for `active_body`: terrain-coloured cells with 1 px gaps, building markers, selection outline, title bar, and a hover tooltip (suppresses zero deposits).
- `src/ui/nav_pane.hpp` / `.cpp` — Left navigation pane: fixed full-height column, ten numbered tab slots, only the **Tile Ledger** wired (parked at slot 8). Exposes `nav_pane_width`.
- `src/world/components.hpp` — `orbital_angle_rad` added to `body_component`; Kepler `1.05`, Vesta `3.93` authored in `hard_coded_world.cpp`.
- `src/ui/tile_inspector` — renamed window to **Tile Ledger**; now takes `bool* p_open` so it fully closes (X button) rather than collapsing. Toggled by the nav tab.
- `src/core/sim_loop` — rebuilt as a **three-layer clock**: sim tick → day tick → econ tick, with a runtime speed multiplier (pause + 1x–5x).
- `src/core/app` — fixed top-right **system tick** readout (Day/Econ) and a **speed-control** panel below it; nav pane and Tile Ledger wired into `render()`; F12 screenshot capture (`save_screenshot` via `SDL_RenderReadPixels` + `SDL_SaveBMP`).
- `tools/capture.ps1` — build → launch → F12 → BMP→PNG dev-loop wrapper.
- `.claude/settings.local.json` — `acceptEdits` default plus an allowlist for the build/screenshot loop (gitignored).

### In-session decisions

**Body click always brings the surface forward.**
CANVASES.md contradicted itself: the layout section states the intent ("click a body, arrive at its surface — a single action") while the interaction bullet made the swap conditional on the Solar System Canvas already being the minimap. Implemented the *intent*: clicking a body sets `active_body` and `surface_is_primary = true` unconditionally. CANVASES.md updated to match.

**`input_enabled` added as a 5th canvas parameter.**
The primary canvas fills the whole window *behind* the bottom-right minimap, so a click in the overlap would otherwise be processed by both canvases. `app::render()` routes input to exactly one canvas (mouse-in-minimap → minimap, else primary), gated by `WantCaptureMouse` so ImGui panels take precedence. Deviates from the 4-arg signature in the spec; documented.

**Canvases drawn to the ImGui background draw list.**
Keeps the debug/overlay windows (nav pane, tick, ledger) on top with no z-order management, and lets manual hit-testing coexist with ImGui. No separate minimap draw path — element sizes scale by `min_dim/720` with floors, and labels/titles are suppressed below ~320 px so the minimap stays readable.

**Minimap / right-column sizing.** `mm_w = max(240, 0.20·min(window w,h))`, `mm_h = mm_w·0.75` (the 240×180 4:3 ratio). The system-tick and speed panels reuse `mm_w` so the right column stays aligned; each is ~⅓ of `mm_h` tall.

**Three-layer tick model with derived pacing.**
`sim_ticks_per_day = 12`, `econ_tick_days = 90` (three 30-day months → quarterly economy resolution). Real-time pacing comes from one constant, `seconds_per_day_1x = 6.0`, so 1x = 6 s/day and **3x ≈ 2 s/day** as requested; 12 sim ticks/day gives 6 steps/sec at 3x — fine-grained enough to interpolate fluid motion later. Speed 0 = paused (drops the accumulator so unpausing doesn't fast-forward). All calendar/pacing values are `static constexpr` tunables — explicitly tentative.

**`init.lua` config repurposed.** The unused `sim_hz` / `econ_per_sec` were retired in favour of `default_speed` (1x–5x), which `run()` reads via `set_speed`. Closes the Layer 0/1 open item about wiring `config` to the loop. The calendar itself now lives in C++.

**Nav pane is a launcher, ledger stays a window.** Tabs toggle panels rather than docking content; the Tile Ledger remains a floating, movable window (kept "as-is") but closable. Slot numbering and the slot-8 placement are temporary — menu layout is deliberately out of scope while canvas work takes priority.

**Screenshot tooling: in-app capture over external screengrab.** F12 dumps the exact composited backbuffer to `build/Debug/screenshots/`. BMP (not PNG) to avoid adding an `SDL_image` dependency; the wrapper converts to PNG via `System.Drawing`. Permissions use a wrapper-script allowlist because shell permission rules are prefix-matched and can't scope by directory.

### Corrections made during session

`tools/capture.ps1` used an em dash in a string literal; Windows PowerShell 5.1 reads BOM-less files as ANSI and the multibyte character broke parsing. Replaced with ASCII.

Nav pane labels were clipped to a single glyph. Cause: `-1.0f` was passed as the `Selectable` width — unlike `Button`, `Selectable` treats a nonzero `size.x` as a *literal* width, producing a near-zero-width box. Fixed by deriving the width from `GetContentRegionAvail().x`. (Widening the pane had no effect until this was found.)

### Open items

- **Canvases render full-window behind the nav pane and top-right panels.** The leftmost sliver of the solar view and the top-right corner are occluded. Clean follow-up: inset the primary canvas to start at `nav_pane_width` and below the tick/speed column. `nav_pane_width` is already exposed for this.
- **Nav slot layout is temporary** — Tile Ledger at slot 8, others empty placeholders. Revisit when the menu set is designed.
- **Calendar values tentative** — no year/month/day date display yet; the tick widget shows raw Day/Econ counts.
- **Lua "alive" indicator dropped** from the fixed tick widget to fit the ~⅓-minimap height; restore with a slightly taller widget if wanted.
- `m_` member prefix still unaddressed in DEVELOPMENT_PRACTICES (carried from Layer 0/1).

---

## 2026-06-13 — Layer 1: ECS data model

**Status:** Complete. Layer 2 (extraction and production) is next.

### What was built

- `src/world/entity.hpp` — `entity_id` typedef (`uint32_t`); `null_entity = 0` sentinel.
- `src/world/components.hpp` — Shared enums (`resource_type`, `terrain_type`, `body_type`, `building_type`) and all six Layer 1 component structs: `tile_component`, `body_component`, `building_component`, `stockpile_component`, `market_component`, `unit_component`. Resource deposits and market arrays are `std::array<float, resource_count>` indexed by `resource_type`.
- `src/world/world.hpp` / `world.cpp` — ECS registry: one `std::unordered_map<entity_id, Component>` per component type, `create_entity()` allocating monotonically increasing IDs.
- `src/world/hard_coded_world.hpp` / `hard_coded_world.cpp` — `make_hard_coded_world()` populating two authored bodies: Kepler (4×4 planet, 1.0 AU, iron/silicate deposits, two buildings) and Vesta (3×3 moon, 5.2 AU, ice/rare-metal deposits, one building). ~200 authored float values across 25 tiles, 2 markets, 3 buildings, 1 unit stub.
- `src/ui/tile_inspector.cpp` — Layer 1 ImGui panel: body selector combo, scrollable tile table (terrain, hazard, habitability, per-resource deposits), buildings list, market supply/demand/price table. Serves as the functional specification for the production tile canvas and market ledger.
- `src/core/app` updated — `world m_world` member added; `make_hard_coded_world()` called at startup; `ui::draw_tile_inspector(m_world)` called each frame.

### In-session decisions

**ECS over OOP for the data model.**
The developer chose ECS explicitly. Entities are plain `uint32_t` IDs; all data lives in per-component maps on the `world` registry. No base classes, no virtual dispatch. Layer 1 has no systems yet — only data.

**`std::unordered_map` for component storage.**
Dense arrays would require a stable maximum entity count upfront. Sparse maps are correct for the prototype's authored, bounded world and keep entity creation trivial. Revisit if component iteration becomes a hot path in later layers.

**Four resource types for prototype scope.**
`iron_ore`, `ice`, `silicates`, `rare_metals` — enough to produce meaningful supply/demand divergence between bodies without expanding the market or extraction logic prematurely.

**`resource_count` constant from enum sentinel.**
`resource_type::count` used as array size via `static_cast<std::size_t>`. Avoids a separate manifest constant; adding a new resource type automatically sizes all arrays correctly.

**`tile_spec` local struct in `hard_coded_world.cpp`.**
A private helper struct used only during world construction — not part of the runtime data model. Keeps the authored values readable as a flat table without polluting `components.hpp`.

**Market prices seeded to `base_price` at init.**
Prices are set equal to `base_price` at construction so the market is in a neutral state before the first economy tick runs price resolution (Layer 3). No placeholder zeroes that would require special-casing.

### Corrections made during session

`SDL3::SDL3main` removed from `target_link_libraries` and `#include <SDL3/SDL_main.h>` removed from `main.cpp`. The SDL_main entry-point shim is only needed for Windows GUI subsystem builds; CMake defaults to the console subsystem, making it redundant. This also resolved the `SDL3::SDL3main` target-not-found error produced by the Visual Studio generator when building against FetchContent SDL3.

`onelua.c` added to the Lua exclusion list in `CMakeLists.txt`. The Lua repository includes this single-file amalgamation which re-includes `lua.c`, causing a duplicate `main` symbol at link time. Excluding it alongside `lua.c` and `luac.c` resolves the error.

### Open items

- `m_` member prefix convention: carried forward from Layer 0, still unaddressed in DEVELOPMENT_PRACTICES. Confirm before Layer 2 adds more types.
- `scripts/init.lua` `config` table not yet wired to `sim_loop` constructor. Still uses hardcoded defaults.
- `unit_component.owner` is `null_entity` — the player corporation entity is not yet defined. Needs a home before Layer 5 (budget) assigns revenue to a faction.

---

## 2026-06-13 — Layer 0: Engine scaffolding

**Status:** Complete. Layer 1 data model begun by end of session.

### What was built

- `CMakeLists.txt` — FetchContent build for SDL3 (`release-3.2.0`), Lua 5.4 (`v5.4.7`), sol2 (`v3.3.0`, header-only), Dear ImGui (`v1.91.6` with SDL3 + SDLRenderer3 backends).
- `src/core/sim_loop` — Fixed-timestep loop at 20 Hz using an SDL `GetTicks` accumulator. Economy tick fires every N sim steps (default: 20, i.e. 1 Hz). Spiral-of-death clamp at 8 steps.
- `src/core/app` — SDL3 window and renderer, ImGui initialised, render loop calling `sim_loop::tick()` each frame.
- `src/scripting/lua_state` — sol2 wrapper; `safe_script_file` used for all file loads per TECH_FOUNDATIONS constraint on unprotected sol2 calls.
- `scripts/init.lua` — Loaded at startup; prints confirmation and defines a `config` table for future use.
- `.gitignore` — Covers build output, CMake artifacts, IDE files, compiled binaries.
- Engine Status ImGui panel — displays live sim tick and econ tick counters to confirm both loops are running.

### In-session decisions

**sol2 integrated as header-only, bypassing its CMake.**
sol2's own `CMakeLists.txt` runs `find_package(Lua)` which conflicts with our FetchContent-built Lua. Using `FetchContent_Populate` and manually adding `${sol2_SOURCE_DIR}/include` to the game target's include dirs avoids the conflict with no functional loss — sol2 is header-only regardless.

**SDL3 linked as shared; DLLs copied post-build on Windows.**
Static SDL3 introduces platform library dependencies (user32, gdi32, etc.) that SDL's CMake handles correctly but which complicate the link on MSVC. Shared + post-build DLL copy via `$<TARGET_RUNTIME_DLLS:ProjectIo>` is the simpler default. Revisit if distribution packaging becomes a concern.

**`SDL3::SDL3main` removed from link.**
The SDL_main redirection was unnecessary for this configuration; removing it resolved a linker issue without changing behaviour.

**`onelua.c` excluded from Lua build.**
The Lua repo includes `onelua.c`, a single-file amalgamation that re-includes `lua.c`. Excluding it alongside `lua.c` and `luac.c` prevents duplicate symbol errors.

**`max_catchup_steps = 8` for accumulator clamp.**
Chosen to allow the sim to catch up after a ~400 ms hitch at 20 Hz without stalling. No empirical basis yet — revisit if the sim loop becomes expensive enough to make 8 steps a meaningful cost.

**`window_w` / `window_h` as compile-time constants in `app.cpp`.**
Not exposed to Lua or config yet. Sufficient for the prototype; move to a config table in `init.lua` if window size needs to vary.

### Corrections made during session

Naming convention violations caught in review: all type names, function names, member variables, and filenames were PascalCase or camelCase on first write. Corrected to `snake_case` throughout per DEVELOPMENT_PRACTICES. Files renamed on disk (two-step rename required for `App` → `app` on Windows NTFS).

Documentation style: public interfaces initially used `//` comments. Corrected to `///` Doxygen with `@param` / `@return` throughout.

### Open items

- Member variable prefix (`m_`) is used throughout but not addressed in DEVELOPMENT_PRACTICES. Confirm whether to keep it or drop it before Layer 1 adds more types.
- `scripts/init.lua` defines a `config` table with `sim_hz` and `econ_per_second`. These are not yet read back by `sim_loop` — the constructor uses hardcoded defaults. Wire this up when the Lua/C++ boundary is exercised further.
