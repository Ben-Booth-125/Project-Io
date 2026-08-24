# Project Io — REFINED (active worklist)

## Sprint 16 — the mercenary vertical slice (v0.1.15), Batch Delivery

Ten items, promoted 2026-08-23. Goal: a polity hires the company, the company fights, the
company is paid — playable end-to-end. Design is settled for all ten (`node
tools/session/backlog_query.js --status designed --sprint 16 --table`); each has an
item-spanning requirement in `req/requirements.json` (`--batch
2026-08-23-sprint-16-mercenary-slice`).

**Dependency waves** (an item's `requires` in backlog.json; a wave's items build in parallel,
waves run in order — this is the collision map's splitting heuristic, not a file-disjointness
gate, since worktree isolation absorbs any overlap):

```
Wave 1  BL-569 province holder        BL-575 unit marker + march UI
           |                                    |
Wave 2  BL-570 condition subject   BL-571 nation garrisons
           |         \                 /   |
Wave 3      \          BL-572 contract offers
             \                   |
Wave 4        BL-573 contract record & verbs
                 /        |          \
Wave 5  BL-574 harness  BL-577 messages+income  BL-576 ledger (+ needs BL-575)
                 \        |          /
Wave 6         BL-578 slice playthrough (needs all nine)
```

Wave 1 has no shared files (battle/province vs. UI canvas) — genuinely disjoint, no worktree
needed for the pair, though each still gets its own worktree per the standing sub-agent model.
Every later wave is a strict sequential dependency on the wave above it; there is no same-wave
symbol sharing beyond wave 1 and the three-way wave 5 fan-out (574/576/577 all read BL-573's
`mercenary_contract` type but do not write each other's files).

---

### Wave 1 — DONE (2026-08-23)

**BL-569 (province holder)** and **BL-575 (unit marker + march UI)** landed and merged to
`main` (worktree-agent-a85dd40a41493c103, worktree-agent-a0220dee51ed2793d); both `complete`
in backlog.json, design prose archived. Full-suite re-verification after the merge fixed one
save-version tripwire and re-blessed one golden — see commit `e9c2c5ac`. BL-575's live-click
pass confirmed March is reachable and dispatches; it did not confirm the unit visibly moving
(NR-577, not a blocker — see that entry).

---

### Wave 2 — DONE (2026-08-23)

**BL-570 (condition province subject)** and **BL-571 (nation garrisons)** landed and merged to
`main` (worktree-agent-aae464e367f3fcb1d, worktree-agent-a477567f226eaa377); both `complete` in
backlog.json, design prose archived. Both branches had merged an older `main` into themselves
before this session's origin/main reconciliation landed, so integrating them meant resolving
stale-base doc conflicts by hand each time (see commits `0a8f6de2`, `b409fc6f`) — and a real
collision: both items independently bumped `world_save_version` 4→5 for different new fields;
combined into one coherent bump to 6. `spectator_determinism`'s golden re-blessed again
(garrisons are new units, folded into `state_hash` unconditionally).

Two things flagged for Ben, not blockers: `NR-579` (BL-570's `fee_mult`/`deadline_ticks` are
legible placeholders) and `NR-580` (every nation's treasury is 0 at generation, so BL-571's
garrisons all land on the sizing floor — the same gap BL-572, next, is about to hit from the
funding side).

---

### Wave 3 — DONE (2026-08-23)

**BL-572 (contract offers)** landed and merged to `main` (worktree-agent-a1a24b0f421a2fbf6, a
clean fast-forward — no sibling wave-3 agent, no doc-conflict archaeology this time). `complete`
in backlog.json, design prose archived. `nation_scorer_harness` gained R8 (34 checks, all
pass); `save_roundtrip` clean (`world_save_version` 6→7). `spectator_determinism`'s golden did
NOT move this time (a default world's `mercenary_offers` stays empty while treasury is 0, so no
new state entered the hash).

One thing carried forward into Wave 4, not a blocker: `NR-581` — offer fee/deadline are a
placeholder `contract_offer_params`, not a live `contract_template_registry` lookup (that
registry needs sol2/Lua, unreachable from `world/*`'s Lua-free superset `derive_contract_offers`
lives in). BL-573 below is where a real lookup belongs, since accepting an offer needs the
template's actual predicate anyway — thread `contract_template_registry` through the same way
`recipe_registry` already reaches `world/*` (a plain parameter, loaded once at the app-layer
boundary, never loaded by `world/*` itself).

---

### Wave 4 — DONE (2026-08-23)

**BL-573 (contract record and verbs)** landed and merged to `main`
(worktree-agent-a426dc003723cf20f, a clean fast-forward). `complete` in backlog.json, design
prose archived. New harness `tools/verify/mercenary_contract_harness.cpp` (51 checks) proves
accept/evaluate/pay-or-fail works at all — **BL-574 below extends this existing file with the
M1–M7 terminal-state cases, it does not create a new one.** `save_roundtrip` clean
(`world_save_version` 7→8). `contract_template_registry` now threaded from the app-layer
boundary the way `recipe_registry` is (closes NR-581's deferred half); BL-571's
`active_mercenary_contract_for` stub now resolves for real, so the corp-vs-nation garrison
battle trigger is live. `spectator_determinism`'s golden held.

One deliberate deviation from the item's own original brief, correctly caught in-flight: payout
is a direct transfer from the offer's already-fully-funded escrow, not a second `budget_claim`
draw — the treasury was already debited in full while BL-572's escrow accumulated, so a fresh
claim would double-debit the same contract. One flagged number: `NR-582`, `contract_failed`'s
sentiment magnitude (-4.0 Trust, double `contract_cancelled`'s -2.0) is a ratio, not measured —
same discipline as NR-579/580's placeholders, revisit at playtest.

---

### Wave 5 — DONE (2026-08-24)

**BL-574 (contract harness), BL-576 (Contracts ledger), BL-577 (messages + income)** all landed
and merged to `main` (worktree-agent-afa7ac59bf567cd92, worktree-agent-a5991264ff4655709,
worktree-agent-a868b5c9d8eb5bcb9 — commits `e1a1a44a`, `9e9f581d`, `9bc93591`). All three
`complete` in backlog.json, design prose archived. Two genuine mid-batch collisions, both from a
concurrent session sharing this checkout (a review-queue purge and a Sprint 32 branch merge
landing on `main` between waves): an `NR-583` id collision (renamed to `NR-584`) and a
`question_log.json` conflict between BL-576's and BL-577's own additions (resolved as a union,
both kept). `mercenary_contract_harness` reached 66 checks; new `contract_dispatch_harness`
(20 checks); all independently re-verified on the merged tree, `spectator_determinism`'s golden
held.

**Live-click pass (main session, computer-use, 2026-08-24)** against a real generated world
with real live offers (a funded nation, contrary to NR-580's worst case): the Contracts
ledger's rail slot, toggle rule, all three views, the Accept→force-picker→Confirm flow (balance
visibly credited), and the Abandon confirm-with-reputation-cost popup all confirmed reachable
and correct by mouse click. **BL-577's contract card was NOT reachable** — no UI control
anywhere calls a selection trigger for a `mercenary_contract` entity, confirmed directly by
clicking an Active-view contract row (it selects nothing). The card's rendering code exists and
reads correct, but is unwired; a future item needs to add that trigger before it can be
live-verified. Two design calls flagged, not blockers: `NR-585` (`abandoned_event_posted` is
deliberately unserialized) and `NR-586` (the ledger took a new nav-rail slot 13 — the curated
nine plus the developer tail were both full).

---

### Wave 6 (after all nine)

#### BL-578 — MERCENARY_SLICE_PLAYTHROUGH
Requirements: § mercenary-slice-playthrough (R1–R4). Files: `scripts/verify/mercenary_slice.lua`,
`ROADMAP.md`, `sprints.json`, `MANUAL.md`. One task: the six-capture `--verify` script, a live
playthrough, the flag-off determinism check, then the v0.1.15 cut.

**Known gap to route around, not silently paper over:** the design's "a completed contract"
capture likely meant the contract CARD (BL-577), which has no live trigger yet (Wave 5's own
note, above) — the ledger's History view is the reachable substitute for "a completed contract"
on screen. Either capture History instead, or treat wiring a selection trigger for
`mercenary_contract` as this item's own scope addition if the card specifically is wanted.

---

Promote/build order: DELIVERY.md § The Delivery lifecycle. Step 4a (`verifier-review`) runs
once per wave that merges more than one item, not once for the whole batch, since waves 2–5
land on top of code the previous wave actually wrote.

---

## Sprint 17 — the ancient roster becomes a ladder (v0.1.17)

**Sprint 16 merged to `main` 2026-08-24** (all ten items, v0.1.15 cut) and this branch merged
`origin/main` the same day — the sequencing gate is open. `BL-587` had already landed standalone
ahead of the merge (see below); `BL-585` and `BL-586` (wave 1) landed together immediately after,
since `BL-585`'s own design named the exact new-goods list as settled WITH `BL-586`'s chains, not
before. Remaining: `BL-588` through `BL-594`.

#### BL-587 — INTERCHANGEABLE_METHODS_EXIST — COMPLETE 2026-08-23

Requirements: `req/requirements.json` § interchangeable-methods-exist. Files: `scripts/recipes.lua`,
`tools/verify/chain_depth.cpp`, `docs/economy/PRODUCTION.md`.

1. Two genuine interchangeable methods authored on the **existing** resource roster (no new
   `resource_type` values — that is BL-585's item, gated on Sprint 16's save-format bump):
   `charcoal_from_kiln` "Coking Kiln" (ancient, shares `timber` with the Charcoal Burner, reagent
   `iron_blooms` at required depth 2) and `steel_bessemer` "Bessemer Converter" (industrial, shares
   `iron_ore`+`coal` with the Smelter, reagent `machinery` at required depth 2). Both landed as
   recipe ids 29/30 — appended, never inserted.
2. `chain_depth.cpp`'s R2 row re-run against the shipped roster: **13 sibling pairs, 10 supply
   routes, 1 precondition, 2 interchangeable methods, 0 dominated** — the bucket that was empty
   before this item now holds real content and the guard still passes.
3. `recipe_switch_harness` and `price_band_harness` re-run clean — both link the live
   `recipes.lua`/`economy.lua` and neither is Sprint-16-owned, so both are cheap confirmation that
   the shared file was not broken for anyone else building against it.
4. `PRODUCTION.md` § Alternate production methods corrected: the four pairs once reported
   "dominated" (NR-243) are supply routes/a precondition, not methods — closed rather than
   retuned (NR-589) — and the new pairs are recorded in the Metal Foundry / Fuel Production group
   tables and the ancient-chain table.

---

#### BL-585 — ANCIENT_GOODS_APPEND — COMPLETE 2026-08-24

Requirements: `req/requirements.json` § ancient-goods-append-and-slice-1 (R1). Files:
`src/world/components.hpp`, `world_save.{hpp,cpp}`, `scripts/world_gen.lua`,
`tools/verify/chain_depth.cpp`, `docs/economy/RESOURCES.md`, plus two guarded exhaustiveness
checks found while building and fixed in the same pass: `src/core/verify_api.cpp`
(`k_resource_slugs`, the GUI build's own static_assert) and `src/world/resource_names.cpp` (the
one mapping both `recipes.lua` and `world_gen.lua` load Lua resource names through — the exact
NR-237 defect class, so every new value goes here THE SAME CHANGE, not after).

1. Four `resource_type` values appended (`ceramics`, `dressed_stone`, `planks`, `tools`),
   `resource_count` 38 → 42. `world_save_version` 8 → 9 — structural, not additive: every
   per-resource array in the stream widens, so a v8 stream is refused whole, same contract as
   every prior bump. `save_roundtrip` gained **P13** (v8 refusal).
2. `world_gen.lua` `base_price` for all four, derived at the roster's observed ~1.433x markup.
3. `chain_depth` R1/R1b re-run clean; `tools`/`ceramics`/`dressed_stone` added to R1's
   actor-consumed table (terminal, market-sold) and deliberately **NOT** added to R1b's narrower
   table, matching that row's own documented rule for `trade_goods_misc`.
4. Full tree (`cmake --build build`, every target) and the whole ctest suite verified clean.
   `spectator_determinism`'s golden re-blessed (`2FB3C201D7C4B1FA`) — the structural array-width
   move this append causes, confirmed reproducible across two independently built worlds before
   blessing, per that file's own standing policy.

#### BL-586 — ANCIENT_ROSTER_WIDE — SLICE 1 LANDED 2026-08-24 (still `designed`)

Requirements: `req/requirements.json` § ancient-goods-append-and-slice-1 (R2). Files:
`scripts/recipes.lua`, `docs/economy/PRODUCTION.md`.

1. Four named buildings on **existing** raws — no new deposit, no new extraction target, so
   `placement_rules.cpp` and `icons.cpp` are untouched this slice (icon fallback renders them
   without a crash, same precedent as BL-429's own slice 1): Potter's Kiln (clay → ceramics),
   Stonemason (stone → dressed_stone), Sawmill (timber → planks), Toolmaker (blooms + planks →
   tools, required depth 2, `depth(tools) = 3` — tied with the ancient ceiling, not past it).
2. New sub-facility group `Construction Materials` (Kiln/Stonemason/Sawmill); Toolmaker joined
   `Metal Foundry`.
3. **Deliberately not this slice**: Tannery/Weaver/Shipwright — all three need `hides`, a new
   extractable raw with real tile-generation deposits, a materially bigger change than a recipe.
   BL-586 stays `designed` rather than flipping to `complete` for exactly this reason.

---

#### BL-588 — UNLOCK_RECIPE_TECH_ARM — COMPLETE 2026-08-24

Requirements: `req/requirements.json` § unlock-recipe-tech-arm. Files: `src/world/tech_gate.{hpp,cpp}`,
`construction.cpp`, `economy_system.{hpp,cpp}`, `corp_ai.cpp`, `corp_command.cpp`,
`docs/META_LAYER.md`, `docs/economy/PRODUCTION.md`.

1. `tech_effect_kind` gained a third arm, `unlock_recipe`, storing a recipe **name** (never an id —
   ids are positional). `tech_gate::unlocks_recipe` mirrors it, maintained only through `add_effect`,
   exactly as `unlocks_structure` already is. `recipe_unlocked(w, reg, corp, recipe_id)` resolves id →
   name and checks `world::has_tech`; `gating_tech_for_recipe` is the reverse lookup.
2. Checked at **both** doors — `construct_building` (reuses `construction_result::tech_locked`) and
   `try_switch_recipe` (new `recipe_switch_result::tech_locked`, mapped to the existing
   `corp_command_result::rejected_tech_locked`) — closing the same retool bypass the depth gate's
   own comment names. `corp_ai.cpp`'s build-candidate loop filters on it, mirroring the existing
   depth filter.
3. Two first-cut gates authored **fresh**, not transcribed from `tech_tree.lua`'s unreviewed
   sketch/derived node list (the call NR-591 raised and this item took): `E0-EC-01` unlocks the
   Toolmaker (BL-586) on a processing facility + Cr 500; `E1-EC-01` unlocks the Bessemer Converter
   (BL-587) on holding `machinery` in stockpile.
4. **A real predicate defect found and fixed before landing.** E1-EC-01's first draft (surplus ≥
   1,500 only) was caught by `tech_gate_harness`'s own T3 fixture — satisfiable by any solvent corp
   regardless of what it had actually built, the exact failure mode a gate exists to prevent, not
   merely a test collision. Corrected to require the Converter's own reagent in stock instead.
5. Full tree build and 13 relevant harnesses (`tech_effect_union_harness`, `tech_gate_harness`,
   `construction_gate_harness`, `construction_harness`, `corp_ai_harness`, `chain_depth`,
   `recipe_switch_harness`, `price_band_harness`, `save_roundtrip`, `determinism_harness`,
   `spectator_determinism`, `corp_ai_predictive_harness`, `corp_agency_harness`) all clean.
   `spectator_determinism`'s golden held — no re-bless needed.

---

#### BL-590 — PER_BUILDING_MATERIALS — COMPLETE 2026-08-24

Requirements: `req/requirements.json` § per-building-materials. Files: `scripts/economy.lua`,
`src/world/recipe_registry.{hpp,cpp}`, `construction.cpp`, `economy_system.cpp`,
`src/ui/selection_panel.cpp`, `construction_panel.cpp`, `tools/verify/construction_harness.cpp`,
`docs/economy/PRODUCTION.md`.

1. `recipe_registry::resource_build_cost_for(type, target, recipe)` — the single lookup every
   material-cost call site now goes through: `extraction_site` keyed by `target_resource`,
   `processing_facility` by recipe id, falling back to the type's own `resource_build_cost` when
   unauthored. Two `std::map` overrides, populated by `load_from_lua` from an optional
   `material_overrides` table nested beside each type's `resource_costs` in `economy.lua`.
2. **13 raw occurrences across 4 files migrated** — `construct_building`, the three
   `run_construction` material-draw sites (`economy_system.cpp`), both copies of
   `construction_rate` (`selection_panel.cpp`'s live one and `construction_panel.cpp`'s unused
   mirror — updated for consistency though nothing calls it), and the Build door's capex loop.
   The two road-specific sites (`place_road`, the road capex preview) are unchanged — roads carry
   no building_type/recipe identity to key an override on.
3. First-cut coverage: 5 ancient extraction targets and all 13 ancient processing recipes get a
   timber/stone basket; every industrial and space-sourced entry is untouched.
4. **A real pre-existing gap found and recorded, not silently fixed** (NR-592):
   `corp_ai.cpp`'s build-candidate scoring prices only `build_cost`, never `resource_build_cost` —
   true before this item too, invisible while every type shared one steel basket. `construct_building`'s
   affordability gate still refuses cleanly (no mutation) if a rival's cash covers a candidate its
   materials cannot, so this is a missed-opportunity gap for the scorer, not a correctness bug.
5. Verified three ways: `construction_harness`'s new R9 (a hand-built registry, 5/5 — override hit,
   type-default fallback on both extraction and processing, and a non-extraction/processing type
   ignoring both maps regardless of what's passed); a full tree build with 10 harnesses clean; and a
   temporary live-Lua probe confirming the authored `economy.lua` tables actually resolve through
   `load_from_lua` (built, run, deleted — not a committed harness). `spectator_determinism`'s golden
   held — material overrides change nothing `state_hash` folds.

---

#### BL-591 — DEPTH_READOUT_ON_DASHBOARD — COMPLETE 2026-08-24

Requirements: `req/requirements.json` § depth-readout-on-dashboard. Files: `src/ui/corporation_dashboard.{hpp,cpp}`,
`src/ui/presentation.cpp`, `docs/ui/question_log.json`, `docs/development/user_stories.json`,
`docs/economy/PRODUCTION.md`. **Authority doc corrected** from the item's original `SELECTION.md`
(which does not cover the Corporation dashboard — that surface lives in `LAYOUT.md`'s disclosure
table) to `PRODUCTION.md` (which owns the depth mechanism itself).

1. `corp_rollups` gains four fields (`reached_depth`, `reached_good`, `next_rung`,
   `missing_inputs`), computed once in `derive_corp_rollups` (now takes `recipe_registry`) and
   rendered at the top of the Production card by the shared `rollup_body()` — both dashboard hosts
   (in-place accordion, full-canvas takeover) draw the identical three lines, per BL-265's own
   shared-body pattern.
2. **A real pre-existing UI defect found and fixed**, not merely worked around: `ui::resource_name`
   carried explicit nulls for `charcoal`, `iron_blooms` and `trade_goods_misc` — a "still
   unauthored" comment left over from BL-286 that had been stale for over a week, since BL-429
   (2026-08-15) gave all three real producers and consumers. Exactly the kind of good this readout
   needs to name. Backfilled, plus entries for the four BL-585 goods (`ceramics`, `dressed_stone`,
   `planks`, `tools`), which would otherwise have rendered `(unnamed resource)` the moment
   anything named them.
3. **Render-confirmed, not click-confirmed** (NR-593). `scripts/verify/corp_dashboard.lua`'s
   `corp_rollup_production` capture — driven by `verify.fold("corp_rollup", 0)`, the identical
   `rollup_body()` code path a real click reaches — shows the three lines with real content: a
   fresh corp reads "Reached depth 0, via Iron Ore / Next: Bloomery / Needs: Charcoal". No
   `computer-use` access this session for a literal mouse-click pass; the fold/expand control
   itself is not new to this item (BL-248, already live-verified in earlier work), so BL-591 only
   added content inside an already-reachable surface. Accepted as sufficient per Ben's ruling
   (2026-08-24), following BL-575's own precedent for shipping with a named residual verification
   gap rather than blocking.
4. `question_log.json`'s `corporation_dashboard` entry updated in place (the growth track answers
   the same "how is my corporation doing" question, not a new one). `user_stories.json`'s US-016
   — already exactly this intent, but pointed at the Building element's own Depth readout (BL-431),
   cut outright in the 2026-08-15 playtest rework and never actually built there — corrected to
   the surface this item actually shipped, rather than left claiming coverage for a page that
   does not exist.

---

## Sprint 17 — wave 2 (after wave 1)

#### BL-589 — START_GATE_AUDIT — COMPLETE 2026-08-24

Requirements: `req/requirements.json` § start-gate-audit. Files: `scripts/recipes.lua`,
`scripts/economy.lua`, `src/world/tech_gate.cpp`, `tools/verify/chain_depth.cpp`,
`docs/economy/PRODUCTION.md`.

1. **Measured before ruling anything**: a fresh ancient corp (reached depth 0, no tech earned)
   saw FIVE Build-door processing groups open — Metal Foundry, Fuel Production, Food Processing,
   Artisan Goods, Construction Materials — not the item's own "extraction, one food route, one
   fuel route" first-cut guess. Metal Foundry was open only through `refined_copper`, an
   `any`-band recipe with no ancient identity and no depth gate — the roster's widest anachronism.
2. **Put five concrete calls to Ben** (elicitation form, per the item's own "state the call, do
   not let it stand as a default" instruction) rather than picking silently. The ruling: gate
   `refined_copper` by tech (`E0-EC-03`, owning a processing facility + Cr 400 surplus); leave
   every other group exactly as measured — Fuel Production keeps both Charcoal Burner and Peat
   Kiln (a genuine supply-route pair, R2's own classification), Food Processing keeps both Food
   Rations and Miller, Artisan Goods and Construction Materials stay fully open, and the any-band
   depth exemption itself is **not** narrowed.
3. `chain_depth.cpp`'s new **G5** row asserts the ruled opening exactly: every recipe outside the
   one deliberate lock matches its ruled open/closed state (4/4), `refined_copper` reads
   `tech_locked` at tick 0, and `E0-EC-03` is confirmed NOT a permanent orphan — it genuinely
   resolves once a corp meets its own authored predicate.
4. Full tree build and 11 relevant harnesses all clean (`tech_gate_harness`,
   `tech_effect_union_harness`, `construction_gate_harness`, `construction_harness`,
   `corp_ai_harness`, `recipe_switch_harness`, `price_band_harness`, `save_roundtrip`,
   `determinism_harness`, `spectator_determinism` — golden held, no re-bless needed — and
   `chain_depth` itself).
5. **A real coverage gap found and recorded, not fixed**: `tech_gate_harness.cpp` only exercises
   the original garrison gate (`E0-ML-01`) with dedicated T-rows; the three economy gates
   (`E0-EC-01`/`E1-EC-01` from BL-588, `E0-EC-03` from this item) are proven correct only
   indirectly, scattered across `tech_effect_union_harness`, `chain_depth`'s G5, and the
   full-suite pass. Filed as `NR-594` — real coverage, just not consolidated, and not urgent
   enough to expand this item's scope over.
