# Project Io — Needs Review

**Ben's review queue.** Readable mirror of [`NEEDS_REVIEW.json`](NEEDS_REVIEW.json),
which is canonical — the JSON wins on any disagreement.

> **Generated file.** Produced by `node tools/session/render_needs_review.js`.
> Edit the JSON, then re-run; hand edits here are overwritten.

Things here are waiting on **your judgement**, not on work. Three kinds:

| Kind | Meaning |
|---|---|
| **question** | An open call nobody has made. Not blocking — a blocking item is a backlog entry with `blocked_on` set. |
| **decision-taken** | A call made **on your behalf** so work could continue. Recorded so it can be *overturned* rather than quietly becoming precedent. |
| **observation** | Something noticed in passing, too small or too cross-cutting to file, that a human should still see. |

**How this differs from the neighbours.** [`review.json`](review.json) is a *blocker* list —
items blocked on a visual artifact only you can produce; work there cannot proceed at all.
[`backlog.json`](backlog.json) is *work*. Entries here are neither: they are questions and
reversible calls. If an answer creates work, file a backlog item and resolve the entry with
that item's id.

This queue is **transient**: resolved entries are pruned promptly rather than kept for
posterity — the reasoning lands in code, an authority doc, or a backlog item at the moment
the work happens, and that is the durable record. What stays here is what is still open.

*18 entries — 15 open, 3 resolved.*

---

## Open

### NR-164 — Re-stress generation and time-lapse feel when playable
*question · raised 2026-08-11 · from 2026-08-11 notes session with Ben.*

Ben wants to re-stress-test world generation and the staged-generation time-lapse once the game is playable, to judge whether it feels alive rather than just correct. RULING 2026-08-13 (Ben, elicitation form): keep open until playable - not scheduled, not folded into another item.

*Files: `docs/ui/STARTUP.md`*

### NR-237 — BL-286 added eleven resource_type values the Lua loader could not parse, and nothing noticed for eleven days
*observation · raised 2026-08-15 · from BL-429's ancient chain — the first authored recipe to reference a BL-286 good.*

recipe_registry.cpp's resource_from_name table claims in its own comment to cover 'the full enum so recipes outside the prototype subset load without a retrofit'. It did not: grain, fodder, salt, transport_capacity, charcoal, iron_blooms, bullion and trade_goods_misc were all missing, so ANY recipe naming one threw 'Unknown resource' at load. Added them while landing BL-429.

**Why it matters.** The defect was invisible because it needed an authored recipe to trigger it, and BL-286 deliberately shipped enum + serialisation + base-price wiring with the behaviours 'unfiled' — so nothing consumed the new goods and nothing exercised the parse path. The failure mode is the one this project keeps meeting from a different direction: a data layer that is only ever validated by the data currently authored against it. Note the loader DID behave correctly once reached (it threw and named the value); the gap was that nothing reached it.

> **Recommendation:** No action needed on the fix itself — it landed with BL-429. The question worth your call is whether BL-432's roster harness should assert the parse map covers resource_count, which would catch the next such gap at the moment the enum grows rather than whenever someone first authors against it. Cheap, and it is the same shape as BL-432's existing 'no orphan resources' row.

*Files: `src/world/recipe_registry.cpp`*

### NR-238 — A slow gate looked like a regression twice in one session — the two-line diagnostic that settled it
*observation · raised 2026-08-15 · from Trying to get a full-suite green before opening PR #39 (Sprint 17).*

Three generation sweeps (earthlike_lean_trace, notable_worlds, mediterranean_sweep) ran past 15 minutes without finishing and looked like a performance regression from BL-428/BL-429. They were not. Two cheap checks settled it: (1) build/Testing/Temporary/CTestCostData.txt records ctest's per-test durations from previous runs — the missing baseline, showing these three at 16.5s / 22.2s / 20.1s; (2) world_audit.exe, a STALE binary dated 16:55 that predates the work entirely and could not contain the change, took 14s against its own 0.92s baseline. A ~15x slowdown on an untouched binary is environmental, not a regression.

**Why it matters.** The same wrong conclusion nearly got drawn twice in one session, each time for a different reason - first because two ctest instances were left contending (the exact failure Sprint 6's retro already recorded), then because the box itself was slow. Both times the tempting response was to go hunting in the diff. The general lesson is cheaper than any of that: BEFORE attributing a slowdown to a change, time something the change cannot possibly have touched. If that is slow too, stop looking at the diff. CTestCostData.txt is worth knowing about independently - the gate had no trusted baseline time until it turned up, which is why a slow run and a hung run were indistinguishable.

> **Recommendation:** Worth a short note in the verifier-headless skill under a 'diagnosing a slow gate' heading, since that is where someone will be standing when they hit it. Skill edits need your say-so, so it is not made. The underlying cause of THIS session's slowness (AV scanning fresh unsigned binaries is the likeliest candidate, given build_gen/ exists precisely to give the scanner one stable exclusion path) was not chased down.

*Files: `build/Testing/Temporary/CTestCostData.txt`, `.claude/skills/verifier-headless/SKILL.md`*

### NR-240 — BL-429 slice 2's C++ changes were authored and reviewed but never compiled — remote session network policy
*observation · raised 2026-08-15 · from BL-429 slice 2, run in a Claude Code remote/cloud session rather than the usual Windows dev box.*

cmake -B build's SDL3/sol2/ImGui FetchContent steps all pull from codeload.github.com, which this session's outbound network policy returns a 403 for (confirmed an organization policy denial via /root/.ccr/README.md, not a transient TLS fault — retrying or routing around it is explicitly the wrong move). Lua changes (recipes.lua) were syntax-checked with luac5.4 -p and pass; the C++ changes (recipe_registry.hpp/.cpp, placement_rules.hpp, selection_panel.cpp, construction_panel.cpp) were manually re-read against every call site of the touched fields (recipe::name vs the new recipe::display_name; k_extractable's size assertions) but no compiler ever saw them.

**Why it matters.** This is the first time a Project Io session has landed C++ source changes with zero compiler verification. The manual review was as thorough as a human diff review gets, but it cannot catch what a compiler catches — a typo'd member name, a missing include, an ambiguous overload. The next session with real toolchain access should treat this diff as unverified until `cmake --build` and the relevant headless harnesses (chain_depth, buildings_rework_harness, resource_chain_harness) run clean against it.

> **Recommendation:** At the next native/Windows session: build ProjectIo, run buildings_rework_harness and chain_depth at minimum (both exercise k_extractable / recipe additions directly), and open the Build door on an ancient-band tile to confirm the new names render. Fix on sight if anything fails — this is expected verification work, not a surprise.

*Files: `src/world/recipe_registry.hpp`, `src/world/recipe_registry.cpp`, `src/world/placement_rules.hpp`, `src/ui/selection_panel.cpp`, `src/ui/construction_panel.cpp`*

### NR-241 — BL-429's 14 new glyphs are geometrically reasoned but visually unverified — same root cause as NR-240
*observation · raised 2026-08-15 · from Closing BL-429's glyph gap (NR-239) in the same remote session as slice 2, same network limitation.*

14 new icons::building() shapes (quarry_stone, woodcutter_timber, sand_pit, clay_pit, peat_cutting, iron_mine, copper_mine, water_extractor, farm, charcoal_kiln, bloomery, smithy_ingot, goods_bundle, ration_pack) were authored against ImGui's AddConvexPolyFilled/AddLine/AddCircleFilled primitives, with vertex lists hand-checked for angular ordering (a simple, non-self-intersecting perimeter) but never rendered. Silhouette-distinctness from each other and from the existing vocabulary (ore_chunk, square, triangle, hub_node, shield, research) was reasoned by shape family (boulder vs crystal vs dune vs sack vs ...), not by looking at them side by side, which is the actual bar ICONS.md's own 'Adding a new glyph' process sets ('a glance should disambiguate').

**Why it matters.** Vector icon code is exactly the kind of change where 'compiles' and 'looks right' are different questions — a convex-poly vertex list can be syntactically fine and still render as a squashed or overlapping mess at real icon size (r as small as 9px in the Build door). Geometric review from source alone cannot catch that; only rendering can.

> **Recommendation:** At the next session with a real GUI build: open the Build door on an ancient-band tile (all 14 shapes appear across extraction/processing rows), the Buildings tab identity plate, and the Planetary canvas marker for a built ancient building. Check each shape reads at its actual on-screen size, not just at a zoomed mental model of the coordinates. Fix proportions on sight — nothing here is expected to be exactly right first try.

*Files: `src/ui/icons.cpp`, `src/ui/icons.hpp`*

### NR-242 — BL-430: AI recipe-switching pays no attention to the new switch cost/cooldown at scoring time
*decision taken on your behalf · raised 2026-08-15 · from BL-430 (alternate production methods) implementation session.*

corp_ai.cpp's dial_recipe margin-chase (run_corp_strategic_step) scores a recipe switch purely on projected margin gain; it does not subtract economy.recipe_switch's switch_cost, and does not check the target building's recipe_switch_cooldown before proposing a candidate. The decision taken: do NOT build cost/cooldown-aware AI scoring in this pass. The seam itself (corp_command's set_recipe verb, through try_switch_recipe in economy_system.cpp) still enforces both at apply time -- a candidate that would be rejected on cooldown or insufficient funds is simply refused, mutating nothing, exactly as the seam already handles every other reason a corp_ai candidate can fail. So the AI is not exploitable and nothing is unsafe; it just occasionally proposes (and loses a decision slot to) a switch that will bounce.

**Why it matters.** Scoped explicitly per BL-430 design ruling �-- widening corp_ai.cpp scoring to price in the switch cost/cooldown is real new planner scope (a projected-gain-minus-cost comparison, plus a cooldown pre-filter), not a small follow-on to this item. Left as a stated call rather than a silent gap.

- A) Leave as-is: the seam-level gate is the safety property that matters; a slightly wasteful candidate is cheap.
- B) File a small follow-on backlog item scoped to pricing dial_recipe candidates against switch_cost and pre-filtering on recipe_switch_cooldown > 0.

> **Recommendation:** B, next time corp_ai.cpp is touched for another dial -- cheap once someone is already in that function, not worth its own session.

*Files: `src/world/corp_ai.cpp`, `src/world/economy_system.cpp`*

### NR-243 — BL-430 no-dominance guard finds four REAL dominated recipe pairs in the shipped roster
*observation · raised 2026-08-15 · from tools/verify/recipe_switch_harness.cpp R1, run against the real scripts/recipes.lua while landing BL-430.*

The guard groups recipes by (primary output, era band) -- the set genuinely interchangeable as 'methods' on the same processing_facility per recipe_registry.hpp's browse path -- and checks that no pair beats another on BOTH input-basket cost (reference world_gen.lua prices) AND chain-depth of its deepest input. Four pairs fail cleanly: steel_from_iron_nickel dominates steel (cost 2.0 vs 3.0, same depth 0); propellant_atmospheric dominates propellant_electrolysis (cost 2.0 vs 4.0, same depth 1); peat_charcoal dominates charcoal/Charcoal Burner (cost 2.4 vs 4.5, same depth 0); glass/Glassworks dominates trade_goods/Potter&Weaver (cost 2.0 vs 3.9, same depth 0). All four pairs predate BL-430 -- they came from BL-340/BL-429/BL-433 as 'more than one raw reaches this good', and BL-429's own recipes.lua comments on the charcoal and trade_goods pairs explicitly say 'not a tuned alternate method'. BL-430 did not author or retune any of them.

**Why it matters.** Authorial intent aside, a rational player on the SAME processing_facility building always prefers the cheaper, equal-depth route -- so today Charcoal Burner, Potter&Weaver, steel's coal-fired route (once iron-nickel is available) and propellant_electrolysis are dead content once their dominant sibling is reachable. That is exactly the shape BL-430's ruling asked the guard to catch, even though the guard was written for BL-430's OWN future alternates, not as a retroactive audit of the existing roster. Left failing rather than tuned silently -- these are real balance numbers, not a defect in the check.

- A) Leave as-is: log the finding, do not touch recipes.lua magnitudes on this pass -- BL-430 scope is the mechanism, not a balance retune.
- B) Retune one input quantity per pair (e.g. raise peat_charcoal input from 2.0 to something depth-neutral but costlier) in a small follow-on, or add a second differentiating axis (e.g. workforce/throughput) that the current recipe-level model does not carry.
- C) Reclassify: some of these ARE meant as strict tier upgrades (steel_from_iron_nickel over steel, once nickel deposits are scarcer/rarer than iron) rather than side-grade alternates, in which case the guard should exclude cross-tier pairs -- needs Ben's call on which axis (terrain/resource rarity) distinguishes a tier upgrade from a real alternate.

> **Recommendation:** C first (a five-minute design call unblocks whether this is even the right guard shape), then B if genuine alternates remain dominated.

*Files: `tools/verify/recipe_switch_harness.cpp`, `scripts/recipes.lua`*

### NR-244 — BL-431 production method/chain/depth UI has no executable check yet — visual read owed live
*decision taken on your behalf · raised 2026-08-15 · from BL-431 delivery session — sub-agent had no display access, so the C++ was built and verified by compile only.*

Three new Selection-panel surfaces landed (src/ui/selection_panel.cpp: draw_production_method_section, draw_chain_trace_section/draw_chain_trace, draw_depth_readout, plus the ui_state toggles selection_method_open/selection_chain_open/selection_chain_target/selection_depth_open). The GUI target (ProjectIo.exe) built clean with no new warnings from selection_panel.cpp. A requirement group was filed (req/requirements.json, brief production-method-chain-ui, 3 rows, status active) but no scripts/verify/*.lua or tools/verify/*.cpp was authored to back it — the sub-agent had no way to run either kind of check in its context.

**Why it matters.** The three rows are exactly the kind of thing Rule 0b and the standing verifier skills exist for: R1 (method selector layout doesn't push the profitability/workforce controls off screen) needs verifier-visual against a live capture; R2 (chain trace never disagrees with depth_of) and R3 (corp_reached_depth <= max_depth()) are both cheap headless assertions over recipe_registry, straightforward for verifier-headless. Landing the UI without landing its own check leaves BL-431 the one item in the recent BL-428/429/430/431 cluster without an automated guard.

- A) Open the live app (per the standing 'open the app after visual questions' practice), eyeball the Method/Chain/Depth sections on a stacked ancient-band tile, then author scripts/verify/selection_method_chain.lua + a small tools/verify/chain_trace_agreement.cpp and flip both requirement rows to complete.
- B) Accept the compile-only verification for this session and file the two harnesses as their own small backlog items instead of blocking on them now.

> **Recommendation:** A — the surfaces are dense (three toggles stacked in an already-tight Facts column) and are exactly the case Rule 0b calls out as needing real numbers, not a guess.

*Files: `src/ui/selection_panel.cpp`, `docs/development/req/requirements.json`, `docs/ui/SELECTION.md`*

### NR-246 — building_management_shell.lua verify script targets the deleted Buildings tab
*observation · raised 2026-08-15 · from Same BL-431-follow-on delivery that deleted the construction panel's Buildings tab.*

scripts/verify/building_management_shell.lua calls verify.show_panel('construction', true) then verify.construction_view(1) to land on the (now-deleted) Buildings tab and capture its production-method/profit/workforce UI shell. View 1 no longer exists - the construction panel is queue-only (view 0). The script was left un-rewritten rather than adapted, per this delivery's scope.

**Why it matters.** The script still runs (construction_view(1) just sets an int nobody reads for dispatch anymore) but its capture no longer shows what its own comment says it verifies - the workforce/profit UI shell now lives on the building Selection card's Workforce/Profitability pages instead, reached via selection + verify.fold('building_metric', page) rather than construction_view.

> **Recommendation:** Rewrite building_management_shell.lua to select a player building and use the Selection card's pager (building_pages() order, as of the 2026-08-15 playtest rework: Profitability/Method/Workforce — Chain/Depth/Lifecycle are gone, folded into Profitability or moved to the action grid) instead of construction_view(1), or fold its intent into a new selection-card-focused check and retire this one.

*Files: `scripts/verify/building_management_shell.lua`, `src/ui/selection_panel.cpp`*

### NR-247 — Unit Strength page prints unit_component::strength raw, despite its "fixed-point" doc comment
*decision taken on your behalf · raised 2026-08-15 · from New Soldier (unit) Selection card, Strength page (src/ui/selection_panel.cpp draw_unit_strength_page).*

components.hpp documents unit_component::strength as a "Fixed-point combat strength scalar (BL-157)", implying a display divisor is needed to show a real-world number. But every current writer (corp_command.cpp hire_unit, corporation_generation.cpp, hard_coded_world.cpp) sets it equal to the raw manpower count (e.g. 50) with no scale applied, and the existing hover-card reader (entity_summary.cpp draw_unit_summary) already prints it raw with ImGui::Text("Strength: %d", u.strength). The new card prints the raw value too, matching that existing reader, rather than inventing a divisor with no basis in the actual writers.

**Why it matters.** If a future combat pass (BL-157/BL-272 follow-on) starts writing a genuinely fixed-point strength, both this new page and the older hover-card reader will silently disagree with the doc comment until someone reconciles them - worth a look whenever combat resolution actually starts consuming strength values.

> **Recommendation:** When BL-157/combat lands real fixed-point strength writes, update both entity_summary.cpp::draw_unit_summary and selection_panel.cpp::draw_unit_strength_page together (and fix or drop the stale doc comment).

*Files: `src/ui/selection_panel.cpp`, `src/ui/entity_summary.cpp`, `src/world/components.hpp`*

### NR-248 — Profitability chart's Revenue/Expenses split is as fine as building_profit.hpp gets — no revenue sub-breakdown exists
*observation · raised 2026-08-15 · from Playtest-driven building-card rework (2026-08-15): Ben asked for a Revenue-vs-Expenses bar chart on the Profitability page.*

building_profit (src/world/building_profit.hpp) tracks exactly four numbers: revenue (one pooled valuation of this tick's output at market price), input_cost, maintenance, wages. There is no per-resource or per-line revenue breakdown to chart — the pooled market resists exact per-building attribution even for the ONE revenue figure that exists, per the struct's own doc comment. draw_building_profit's new chart therefore plots Revenue against Expenses := input_cost + maintenance + wages, the finest real split the data supports, rather than inventing sub-categories.

**Why it matters.** Flagged per Rule 0b/the standing 'measure before reshaping' practice — this is a real data-availability ceiling, not an implementation shortcut. A finer revenue/expense breakdown (e.g. per-resource revenue, wages vs maintenance as separate bars) would need building_profit itself to track more, not just a UI change.

- A) Leave as-is: Revenue vs Expenses is honest and matches the finest split building_profit tracks.
- B) Extend building_profit to keep wages/maintenance/input_cost as a genuinely separate 3-bar expense breakdown alongside Revenue, if a future pass wants it (input_cost and maintenance+wages are already separate fields, so this is a small UI change, not a data change — only a true revenue sub-breakdown needs new tracking).

> **Recommendation:** A for now. If the Profitability page's 2-bar chart reads as too coarse once played more, B's expense 3-way split is cheap (the fields already exist); a revenue sub-breakdown is not.

*Files: `src/world/building_profit.hpp`, `src/ui/selection_panel.cpp`*

### NR-249 — No per-building profit history exists — the Profitability page's 6-month net-profit line is a placeholder series, same constraint as the old Workforce trend graph
*observation · raised 2026-08-15 · from Playtest-driven building-card rework (2026-08-15): Ben asked for a line chart of net profit over the last 6 months.*

No time-series profit history is recorded per building anywhere in the simulation (the same gap draw_building_workforce_page's pre-existing placeholder trend graph already lived with). draw_building_profit's new 'Net, 6 mo.' PlotLines chart reuses that same honest-placeholder idiom: a smooth deterministic series anchored to the live net-profit estimate, 6 points rather than the workforce graph's 9, with no claim to be real history.

**Why it matters.** Two placeholder trend graphs now exist on the same card (Profitability's Net line, Workforce's target trend) for the identical reason: nothing records per-building history over time. If BL-something eventually adds a real per-building time series (the way body/corp-level history is tracked elsewhere), both should switch to it together rather than one getting fixed and the other staying a placeholder.

> **Recommendation:** No action needed now — noted so a future per-building-history item knows to sweep both graphs, not just the one it was written against.

*Files: `src/ui/selection_panel.cpp`*

### NR-250 — Profitability page vertical budget for the Inputs chart is a fixed judgment-call clamp, not a measured fit
*decision taken on your behalf · raised 2026-08-15 · from Playtest reflow (2026-08-15): Ben asked the Profitability page to fit one screen — bars+line row left 1/3 / right 2/3, Inputs chart below.*

draw_building_profit budgets vertical space as: inputs_h = clamp(total_h*0.24, 46, 70) when a recipe applies, top_h = max(70, total_h - inputs_h - label_h - spacing). These numbers were picked to look right at typical accordion heights, not measured against the worst case (a recipe with many inputs widens draw_input_basket_chart's columns down to a floor, but the STRIP HEIGHT stays fixed regardless of item count).

**Why it matters.** A very short accordion height (small window) or a recipe with an unusually large input basket could still make the Inputs strip cramped even though nothing overflows/scrolls. No real building in the current recipe set stresses this, so it was not visually confirmed against a worst case.

> **Recommendation:** If a future recipe expansion adds a build with many inputs, or the accordion height shrinks further, re-check the Inputs strip legibility live and retune the clamp bounds rather than assuming these numbers still hold.

*Files: `src/ui/selection_panel.cpp`*

### NR-252 — Group-taxonomy judgment calls in the BL-434 recipe grouping
*decision taken on your behalf · raised 2026-08-15 · from BL-434 sub-facility groups: tagging every recipes.lua entry with a group.*

Two calls were genuinely ambiguous when tagging recipes.lua: (1) hydroponics_bay was grouped under Food Processing alongside food_rations/food_rations_milled, even though it PRODUCES agricultural_produce (an agriculture step) rather than processing it into rations (a food-processing step) - it could instead be a standalone Agriculture group. (2) machinery/alloys/spacecraft_components were grouped together as one Advanced Fabrication group (Fabricator + Assembly Plant), rather than split into a Fabricator group and a separate Assembly group, since the task brief gave no explicit steer either way for these three.

**Why it matters.** Both calls affect which Build-door candidates get bundled together and which recipe switches are cheap (intra-group) vs expensive (cross-group) - a different split changes actual playtest costs, not just labels.

- Leave as authored (Food Processing includes Hydroponics Bay; Advanced Fabrication covers all three).
- Split Hydroponics Bay into a standalone Agriculture group.
- Split Advanced Fabrication into Fabricator (machinery, alloys) and Assembly (spacecraft_components) once each has more recipes.

> **Recommendation:** Leave as authored; revisit if either group grows enough recipes that the merge starts feeling wrong at the Build door.

*Files: `scripts/recipes.lua`, `docs/economy/PRODUCTION.md`*

### NR-253 — recipe_switch.cooldown_ticks dropped 6 -> 1, needs playtest
*decision taken on your behalf · raised 2026-08-16 · from Playtest, 2026-08-16: Ben asked how long a method switch takes, then called the answer (6 ticks = 90 days/tick x 6 = ~1.5 in-game years) too long to even register as a mechanic - the disabled Switch glyph just read as "not possible".*

economy.recipe_switch.cooldown_ticks changed from 6 to 1 (one econ tick, ~90 days) on Ben's direct instruction. switch_cost (12cr) is unchanged. A visible progress bar was also added to the Method page ("Retooling - N ticks left") so a running cooldown is legible without hovering the disabled glyph.

**Why it matters.** 1 tick is a first-cut replacement, not a measured value - it removes the near-invisibility problem but has not been played against real economy pacing. Too short and the cooldown stops meaning anything as a commitment device (BL-430's whole point was that switching should NOT be a per-tick optimisation).

> **Recommendation:** Leave at 1 tick for the next playtest pass; retune (likely upward, 2-3 ticks) if switching starts reading as free.

*Files: `scripts/economy.lua`, `src/ui/selection_panel.cpp`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-239 — BL-429 slice 2: named buildings shipped without per-building glyphs — decision taken, not asked
*decision taken on your behalf · raised 2026-08-15 · from BL-429 slice 2 (ancient building roster, Sprint 17) — continuing the item after slice 1 landed the production chains.*

R5 asks for 20+ named buildings "each with its placement rule and glyph". This slice built the names (recipe::display_name), confirmed placement rules stay generic per BL-325's minting test, and got an ancient-band Build door past 20 named rows — but did NOT give any of the 16 new ancient buildings its own icon. Every extraction building still renders icons::building's shared ore_chunk glyph; every processing building the shared square. Farm, Smelter and Hydroponics Bay already share glyphs this way today, so nothing regressed — but R5's glyph clause is still open.

**Why it matters.** Hand-authoring 16 silhouette-distinct vector icons (ICONS.md's own per-glyph process: declare, implement, keep the silhouette distinct from its family, catalogue it) is real asset-design work, not a code change riding along with the recipe roster. Folding it into this slice would have meant either rushing 16 icons past the "distinct silhouette" bar ICONS.md itself sets, or blocking the roster's actual game-economy value on icon authoring. The call taken: ship the content now, file the glyphs as their own follow-on rather than let R5 read as fully met when it is not.

> **Recommendation:** File a follow-on backlog item (e.g. under BL-431's UI umbrella, or standalone) scoped explicitly to per-building glyphs for the ancient roster, and leave requirements.json's R5 at 'pending' until it lands — already done this pass.

> **RESOLVED.** Closed same session (2026-08-15), not deferred: icons::building() gained a resource_type identity parameter and 14 new hand-drawn glyphs cover the 9 extraction + 5 processing resource keys the ancient roster reaches. R5 in requirements.json's ancient-chain-roster group is now complete. Still open: NR-240's compile/visual-verification gap applies to this work too — nobody has actually SEEN these glyphs render yet.

*Files: `src/ui/icons.cpp`, `src/ui/icons.hpp`, `docs/ui/ICONS.md`, `docs/development/req/requirements.json`*

### NR-245 — Manage button now opens the Construction queue, not building detail (Buildings tab deleted)
*decision taken on your behalf · raised 2026-08-15 · from BL-431-follow-on delivery: Workforce and Lifecycle moved from the construction panel's Buildings tab onto the building Selection card as two more accordion pages; the Buildings tab and its tab switcher were deleted (construction_panel.cpp is Construction-queue-only now, foldout title renamed 'Building' -> 'Construction').*

The building Selection card's Manage action (src/ui/selection_panel.cpp, draw_building_selection_body's action grid) used to set ui.construction.panel_view = 1 to land on the Buildings tab's inline detail. That tab is gone - all of its detail (recipe/Method, Chain, Depth, Workforce, Lifecycle) now lives on the Selection card itself. Manage was changed to just set ui.show_construction_panel = true, opening the one view left (the queue).

**Why it matters.** Manage's meaning shifted from 'show me this building's detail' (now redundant - the card IS the detail) to 'show me what's under construction'. A player pressing Manage on a completed, non-building-anything building now sees an empty queue rather than a jump to controls, which could read as a dead button if the queue is empty and nothing else changes on screen.

- A) Leave as-is - the card already shows every control Manage used to route to, so Manage's new job (open the queue) is a legitimate, smaller one, not a broken one.
- B) Repurpose or remove the Manage action entirely now that its old destination is redundant with the card it lives on, freeing the action-grid slot for something else.

> **Recommendation:** A for now - removing/repurposing Manage is a UI-vocabulary change that deserves its own look rather than a side effect of this delivery; flag for the next Selection-card pass.

> **RESOLVED.** B, taken 2026-08-15 in the playtest rework: Ben played the card and asked for Manage gone outright (its destination was already redundant with the card, per option B above). The action-grid slot it freed, plus one former reserved slot, now hold Mothball and Dismantle (the former Lifecycle page's two controls, moved onto the action grid — see NR-246 for the pager-order fallout).

### NR-251 — cross_group_multiplier (6.0x) is a first-cut number, needs playtest
*decision taken on your behalf · raised 2026-08-15 · from BL-434 sub-facility groups: tiered recipe-switch cost.*

recipe_switch_params::cross_group_multiplier defaults to 6.0, putting a cross-group retool (e.g. Bloomery -> Loom) at 72cr against the existing 12cr intra-group switch_cost. That is well short of a fresh processing_facility all-in capex (~400cr: 200 build_cost + ~200cr steel materials at current prices), so at these numbers a cross-group retool is NOT yet the more expensive option versus building fresh - it is a real but not decisive nudge.

**Why it matters.** The design brief asked for a multiplier large enough that build-fresh plausibly beats retool in typical cases (5-8x suggested, deliberately not full parity). 6.0x sits in that suggested band but was not validated against actual playtest economy pacing.

- Leave at 6.0x and revisit once Metal Foundry / Food Processing / etc. groups are actually played against each other.
- Raise toward the point where cross-group cost approaches or exceeds build_cost, making "just build another" the clearly cheaper move in most cases.

> **Recommendation:** Leave at 6.0x for the first playtest pass; retune once real corp behaviour is observed switching between groups.

> **RESOLVED.** Superseded 2026-08-16: Ben retired cross-group switching entirely rather than tuning its price - the only way to change a building's group is now dismantle + rebuild via the tile selector. cross_group_multiplier no longer exists (removed from recipe_switch_params, scripts/economy.lua, recipe_registry.cpp's loader); try_switch_recipe refuses a cross-group target outright (recipe_switch_result::cross_group). This tuning question is moot, not answered.

*Files: `src/world/recipe_registry.hpp`, `scripts/economy.lua`*

