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

*45 entries — 38 open, 7 resolved.*

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

### NR-254 — Build door passed a BROWSE recipe index where construct_building expects an ABSOLUTE id
*observation · raised 2026-08-16 · from Found while wiring BL-428 chain-depth gate, which needed candidate.recipe to mean what it says.*

selection_panel.cpp built its processing candidates with `static_cast<uint16_t>(ri)`, where `ri` is the loop index over reg.recipe_count()/recipe_at() — the BL-433 era-MASKED browse path. That value flowed unchanged into ui.construction.pending_recipe and from there into construct_building, whose `recipe` parameter is indexed ABSOLUTELY (the get_recipe/recipe_id space). Two sibling call sites in the same file (the Method selector, ~line 847/894) already did the right thing via reg.recipe_id(ri.name); the build door did not. Fixed this pass: candidate.recipe now stores reg.recipe_id(browsed.name), and the two readers that indexed it as a browse position (the group lookup and the output-pip lookup) were moved onto get_recipe.

**Why it matters.** The two id spaces coincide exactly while the era mask is the identity, which is every any-band campaign — so this was invisible in normal play and would have stayed invisible. In an ANCIENT campaign the mask hides the industrial recipes, browse index k names a different recipe than absolute id k, and placing a building from the Build door seeds it with the wrong recipe. Ancient campaigns are precisely the content BL-429/BL-430/BL-431 have been building out. No harness caught it because no harness drives the Build door under a non-identity mask.

> **Recommendation:** Fixed. The residual question is whether the browse/absolute split wants a TYPE (a distinct browse_index struct) rather than two uint16_t spaces that convert silently — this is the second time the split has bitten (BL-433 called it out in recipe_registry.hpp and still shipped a raw uint16_t). Worth a small backlog item if a third instance appears.

*Files: `src/ui/selection_panel.cpp`, `src/world/recipe_registry.hpp`*

### NR-256 — --autostart-play still terminates unattended, cause not established
*observation · raised 2026-08-16 · from Three unattended launches while adding the flag (commit e4a087a).*

--autostart-play removes --autostart-windowed's 120-frame cap so the window stays open for a human to look at. It works interactively — Ben used it and reported on what he saw. But launched unattended from a background shell it has terminated on its own three times, at roughly 20s, 34s and ~60s: exit code 0, no crash log, no exception, output simply ending after the warm-start timings. The frame cap is definitely not the cause (it is gated on autostart_mode::smoke and the variable timing rules it out anyway). run()'s loop has only three exits: SDL_EVENT_QUIT, SDL_EVENT_WINDOW_CLOSE_REQUESTED, and m_quit_requested, so something is delivering a close.

**Why it matters.** Two candidate causes and I could not separate them from this session: (a) a genuine stray quit/window-close event in the autostart path, which would be a real defect, or (b) the background-shell environment reaping a GUI process whose launching shell is not interactive, which would make the flag fine in real use and only untestable the way I was testing it. The interactive evidence points at (b), but 'it worked when a human was watching' is not a diagnosis. Worth settling before anyone relies on this flag for an unattended soak or a long-running capture.

> **Recommendation:** Instrument the three exit paths with a one-line printf naming which one fired, then launch once from a real terminal and once from a background shell. That distinguishes (a) from (b) in a single run each. Cheap, and it is the same two-line diagnostic NR-238 records as the thing that settled a similar this-looks-like-a-regression question.

*Files: `src/core/app.cpp`, `src/main.cpp`*

### NR-257 — BL-432's orphan row finds eight real orphan resources, five of them the exact BL-286 values the item was filed over
*observation · raised 2026-08-16 · from tools/verify/chain_depth.cpp R1, first run against the real scripts/recipes.lua + the k_extractable table.*

R1 asserts every resource_type is OBTAINABLE (a recipe produces it or a deposit yields it) and WANTED (a recipe consumes it, or a named actor does). It ships RED on eight resources. Five are orphaned in BOTH directions - grain (23), fodder (24), salt (25), transport_capacity (26) and bullion (29): nothing produces them, no deposit yields them, nothing consumes them. These are exactly BL-286's 'behaviour unfiled' values, and exactly the five BL-432's own design text predicted ('eleven resource values were added with behaviour unfiled, and five of them still have no consumer today, months later'). The check reproduced that claim independently rather than being told it. Three more are obtainable but unwanted: platinum_group_metals (9) and regolith (10) are both extractable with no consumer, and machinery (35) is produced by the Fabricator and consumed by nothing.

**Why it matters.** This is the check doing its job on its first run, not a defect in the check - so it is left RED rather than exempted quiet, the same call NR-243 recorded. But a permanently-red row decays into background noise, which is the failure mode Sprint 18's retro named from the other direction (a green check looking at the wrong thing). The five double-orphans are a content decision: either they get producers and consumers, or they are removed from the enum until their behaviour is filed. The three one-directional ones are smaller - machinery in particular looks like it simply wants a consumer in the fabrication chain. Note the save-format constraint on removal: components.hpp records that resource_type is a serialised width and every per-resource array is sized off count, so deleting a value is a save-format break once BL-107 lands.

- A) File a content item to give the five BL-286 values producers + consumers (the roster-completion reading).
- B) Remove the five from resource_type until their behaviour is filed, and re-add when it is (cheap now, a save-format break after BL-107).
- C) Add them to R1's actor-consumed exemption table with a named future actor, converting a red row to a documented promise.
- D) Give machinery/platinum_group_metals/regolith consumers separately - these are a smaller, more obviously-correct fix than the five.

> **Recommendation:** D first (three small, uncontroversial consumers), then A or B on the five as one decision. NOT C - an exemption naming an actor that does not exist is how an orphan hides, which is the precise thing BL-432's design says the explicit table must prevent.

*Files: `tools/verify/chain_depth.cpp`, `scripts/recipes.lua`, `src/world/components.hpp`*

### NR-259 — player_seed_sweep had never once passed under ctest — a 60s timeout on a 69s tool, silent because a Timeout looks like nothing
*observation · raised 2026-08-16 · from The full 78-test ctest run done to verify NR-257's resource_type removal.*

player_seed_sweep was added 2026-08-15/16 and registered in CMakeLists.txt's IO_TEST_SCRIPT_ROOTED_HARNESSES (so it gets the repo root as its working directory) but NOT in IO_TEST_LONG_HARNESSES, so it inherited the 60 s default timeout. It generates one full world per seed, 24 by default, and takes ~69 s on this box. It therefore timed out on every ctest run from the day it was added, while passing perfectly standalone - which is how it was used, and why nobody noticed. Fixed by adding it to IO_TEST_LONG_HARNESSES (240 s, ~3.5x headroom); it now passes at 71.8 s.

**Why it matters.** The failure mode is the point, and it is the mirror of the one Sprint 18's retro named. That retro found a check running GREEN while pointed at a deleted tab - coverage that was not coverage. This is the same defect from the other side: a check that had never run at all, reporting as a Timeout, which reads as infrastructure noise rather than as 'this harness has never verified anything'. Worth a standing habit: when a harness is added to the gate, run the GATE once, not just the exe. Two of the three ways a check can be worthless - green-but-blind, and never-executed - are both invisible from the harness's own output.

> **Recommendation:** No further action on this instance; it is fixed and verified. Worth considering whether a new harness's first ctest run should be part of the checklist in the verifier-headless skill, since 'passes standalone' is what both the author and the skill's own Procedure section naturally check.

*Files: `CMakeLists.txt`, `.claude/skills/verifier-headless/SKILL.md`*

### NR-258 — NR-243's four dominated pairs were a grouping artefact - the no-dominance guard was asking the wrong question
*decision taken on your behalf · raised 2026-08-16 · from Ben's call on NR-243, 2026-08-16: option C first - settle the tier-vs-alternate axis before retuning any numbers.*

The axis did not need inventing: scripts/recipes.lua already states it twice in its own comments (ids 22 and 23) - distinct raws feeding a shared good is 'an ordinary multi-producer economy fact, NOT BL-430's alternate-METHOD feature (one building offering interchangeable recipes for the same output)'. Measured against that, three of NR-243's four pairs have DISJOINT input sets (steel {iron_ore,coal} vs {iron_nickel_ore}; charcoal {timber} vs {peat}; trade_goods {clay,timber} vs {sand}) and the fourth differs by a placement precondition rather than cost (propellant atmospheric vs electrolysis - the airless route is run because the cheap one is unavailable). So all four were false positives of grouping by (primary output, era), not balance defects. No recipe magnitude was changed. The guard moved to chain_depth.cpp's R2 row, regrouped: every sibling pair is bucketed as a supply route (disjoint raws), an explicitly-exempted precondition pair, or a genuine interchangeable method, and only the third is price-compared. recipe_switch_harness's duplicate R1 was deleted rather than left as a second answer to the same question - it now runs ALL PASS.

**Why it matters.** Today the roster contains ZERO genuine interchangeable methods - 4 sibling pairs, 3 supply routes, 1 precondition, 0 methods. That is a real finding about BL-430: the alternate-method FEATURE shipped, but no content yet uses it, and the four pairs that looked like alternates are multi-producer economics. The bucketing is what keeps R2 from being a vacuous green - a pair cannot escape by being unclassifiable, and the counts print on every run - but Ben should know the dominance half is currently guarding an empty set, and will only start biting when BL-430's own alternates are authored.

> **Recommendation:** No action needed on the numbers. Worth deciding separately whether BL-430 should author at least one genuine same-inputs alternate pair, so the feature it built has content and R2's dominance half has something to guard.

*Files: `tools/verify/chain_depth.cpp`, `tools/verify/recipe_switch_harness.cpp`, `scripts/recipes.lua`*

### NR-261 — BL-422: held stock stays visible to the price signal, against the item's own stated default — the alternative is a fixed point
*decision taken on your behalf · raised 2026-08-16 · from BL-422 design: "Decide whether the supply-side price signal should still see held stock (an argument exists both ways) — default to NOT visible, matching the reservation semantics BL-386 established."*

The default was NOT adopted, and the reason is mechanical rather than a preference. Whether an order holds is decided by comparing its floor to the RESOLVED price; the resolved price is computed FROM market_component::supply (resolve_price(price, base, supply, demand)). Removing held quantity from supply raises the resolved price, which can un-hold the very order that was removed, which puts its stock back into supply. Reaching a consistent answer means iterating to a fixed point inside clear_markets. So the two arrays are now deliberately asymmetric and MARKETS.md says so in as many words: supply is the OFFER (listed stock is a real offer at a price), inventory is the DELIVERY (only what actually changed hands). Only the inventory half of BL-422 was implemented.

**Why it matters.** The phantom-BUYING defect is fully fixed — no processor can draw stock a seller never released. What remains is a pricing nicety: a held order still depresses the resolved price by appearing as supply, which in a thin market can hold the price below the floor that caused the hold, so an order can hold itself down. That is a real (if second-order) feedback loop and Ben may judge it worth the iteration cost; it is not worth spending determinism risk on unmeasured.

> **Recommendation:** Leave as implemented. If it is to be revisited, the cheap first step is a measurement rather than a fix: count how often a held order is the marginal supply that keeps the price below its own floor. File as a follow-on to BL-422 if that count is non-trivial.

*Files: `src/world/market_clearing.cpp`, `docs/economy/MARKETS.md`*

### NR-262 — An explicit matched trade pays the seller and delivers the goods to the shared market shelf, not to the buyer
*observation · raised 2026-08-16 · from Found while scoping BL-422's credit sites, 2026-08-16.*

In clear_markets, a matched explicit trade debits the seller pool and charges the buyer expenditure, but never credits the BUYER stockpile. The goods land in market_component::inventory (true before BL-422 via the listing-time credit, and still true after it via the matched-trade credit). Any corp on that body can then draw them through the ordinary processor path. So a buy order is closer to "pay to put stock on the local shelf" than to "acquire these goods".

**Why it matters.** It is currently invisible because nothing writes world::buy_orders in play — MARKETS.md § Known limitations records the buy-order book as engine-only, waiting on BL-160. The moment BL-160 derive_exchange_orders becomes the first live emitter, a player who wins a matched trade will pay for goods a competitor on the same body can consume. BL-422 deliberately did NOT change this: routing matched fills to the buyer pool is a behaviour change to the buy side, outside a fix scoped to phantom supply. order_book_harness R7.12 pins the current behaviour as conservation (pool loss == inventory gain), so a future correction will fail that row loudly rather than silently.

> **Recommendation:** Fold into BL-160 (auto-exchange policy) rather than filing separately — that is the item that makes the buy side reachable, and the delivery question has to be answered there anyway.

*Files: `src/world/market_clearing.cpp`, `tools/verify/order_book_harness.cpp`*

### NR-263 — BL-422 moves nothing in the five AI benchmark seeds — the defect it fixes is unreached by the benchmark
*observation · raised 2026-08-16 · from Before/after measurement on ai_skill_harness and spectator_determinism, 2026-08-16.*

ai_skill_harness reports byte-identical net worth, solvency, survival and action counts on all five seeds before and after the fix (final = 499896.2 / -115203.9 / 183828.8 / 306437.4 / 396557.7 in both runs), and spectator_determinism is unchanged. The AI places standing sell orders in these worlds (action[place_sell_order] = 9-10 per seed), so orders exist; either none of them hold, or no processor ever drew the phantom stock they created. order_book_harness R7.2/R7.3/R7.10 DO fail against the pre-fix code, so the guard is real — it is the benchmark that does not reach the case.

**Why it matters.** Two things follow. First, the fix is safe to land: provably behaviour-neutral in the benchmark worlds while provably corrective in the case that produces the defect. Second, and more useful, the AI benchmark does not exercise a held order at all, which means corp_ai trade_floor_multiple currently prices its orders low enough to always clear. That is worth knowing before BL-436 calibration moves prices under it: a re-tune that pushes resolved prices down turns the AI standing orders into holds, and the benchmark has never measured that regime.

> **Recommendation:** No action on BL-422. Worth carrying into BL-436: when the cost/income calibration is settled, re-check whether the AI standing orders still clear, since a held order is stock the corp neither sells nor consumes.

*Files: `tools/verify/ai_skill_harness.cpp`, `src/world/corp_ai.cpp`*

### NR-264 — A remote Linux session CAN build and run the headless suite — CMake configure is what is blocked, not compilation
*observation · raised 2026-08-16 · from This session, working around the NR-240 symptom (BL-429 slice 2 authored but never compiled because of the remote network policy).*

cmake configure fails outright in the remote container: SDL3 and Lua come in by FetchContent and the download is refused by the network policy, so no target is ever generated and every harness looks unbuildable. But the 43 src/world/*.cpp sources (the io_world_obj set, minus the four registry TUs) need neither, and every harness in the CMake glob batch links io_world_obj ALONE. Compiling them directly works: build the 43 objects once with xargs -P8 (~14 s), ar them into a static lib, then link each harness against it (~5 s each). 63 of the 77 harnesses build and run this way. The exceptions are the 8 Lua-linked ones (chain_depth, tier_margin, era_roster, player_seed_sweep, recipe_switch_harness, interbody_pull_harness, pregame_balance_harness, persona_counsel_harness), font_glyph_harness (ImGui), and the long sweeps.

**Why it matters.** NR-240 and NR-241 record work that shipped uncompiled and visually unverified because a remote session believed it could not build. It can — for two thirds of the gate, including every economy, determinism and generation harness that does not read a Lua script. That is the difference between a remote session that verifies its own work and one that hands Ben unverified changes.

> **Recommendation:** Worth saving as a tool rather than a note (CLAUDE.md § Tool creation is skill creation): a tools/verify/build_linux.sh doing the lib-then-link build, named in the verifier-headless skill as the fallback when cmake configure fails. Creating or modifying a skill needs Ben permission, so it is proposed here rather than done. Vendoring Lua would close the remaining 8.

*Files: `CMakeLists.txt`, `.claude/skills/verifier-headless/SKILL.md`*

### NR-266 — BL-436's calibration_sweep explains the x4 collapse by a mechanism that cannot happen - do not re-tune on that narrative
*decision taken on your behalf · raised 2026-08-17 · from Consequence of NR-265, found the same session, 2026-08-17.*

BL-436's calibration_sweep narrative attributes the x4 collapse to corps "spending the extra income building processors that lose MORE per tick at higher scale". Given NR-265 - the AI scorer has no processing_facility build candidate - no rival can spend income on a processor at any scale. Whatever the sweep measured, the processors involved were generated or warm-start ones only, and their count does not respond to income. The stated causal chain (more income -> more processors -> bigger per-tick loss) has no link in the middle.

MEASURED THIS SESSION (2026-08-17), since tier_margin is Lua-linked and a local session can run it. Over 3 seeds x 20 ticks on the real generated world: extraction nets +1659.18 per building-tick, processing nets -7.92. A processor pays back its 200 cr capex NEVER; a mine pays back 100 cr in 0 ticks. The gap is not subtle and it is not mainly a price problem - R6 reports it directly: deposit richness (mean 53.34) multiplies a mine's base_rate of 20 to ~1067, against a processor's FLAT base_rate of 8, a ~133:1 rate ratio before any price is applied. Of 1590 processing building-ticks, 48.9% produced, 30.9% STARVED on a missing input, 9.1% had no recipe set. And R4/R5 name three recipe inputs (ids 8, 9, 10) that have deposits, are usually the richest thing on their tile, and are produced by NOTHING - a siting/reach failure, not a margin one.

**Why it matters.** The sweep's conclusion is currently the standing reason a cost-side lever was left alone. If the mechanism is wrong the conclusion may still be right, but it is unsupported, and any future calibration reasoning that leans on it inherits the error. Cost-side calibration is explicitly Ben's call, so I have measured and stopped rather than re-deriving it myself.

The measured picture makes the original narrative even less tenable: processors are not losing because corps overbuilt them at scale, they are losing because a processor's flat rate cannot compete with a richness-multiplied mine, and because a third of their ticks starve. Neither mechanism involves the AI building anything.

> **Recommendation:** Re-derive the sweep before any cost-side number moves. tier_margin now runs locally and gives the numbers directly (it is currently the one red test in an otherwise green 79-test tier, failing its R2 and R5 exactly as designed). The three candidate levers it points at are distinct and should not be conflated: (a) the ~133:1 rate ratio from richness multiplying extraction but not processing, (b) the 30.9% input-starvation rate, (c) the 3 never-produced inputs, which read as siting/reach. All three are cost-side calibration and therefore Ben's - measured here, not touched.

*Files: `docs/development/backlog.json`, `tools/verify/tier_margin.cpp`*

### NR-267 — BL-428 chain depth has no AI player - a rival that cannot build a processor can never climb the ladder
*observation · raised 2026-08-17 · from Consequence of NR-265, found the same session, 2026-08-17.*

BL-428 gates recipes on a corp's reached chain depth. Depth is climbed by operating processors at successively deeper tiers. Since the AI scorer never builds a processing_facility (NR-265 / BL-439), a rival's depth is fixed at whatever its generated assets give it, for the whole campaign. The gate is single-player by construction.

**Why it matters.** It does not bite today: the depth-gated recipes are ancient-tier, so a 1960 campaign never reaches them. But the growth gate is being built as a shared mechanism, and it currently has exactly one participant. Worth knowing before more design leans on rivals climbing it.

> **Recommendation:** No action needed now - recorded so it is not rediscovered later as a bug. It resolves as a side-effect of BL-439 (AI never builds processors); if BL-439 slips past the point where depth-gated recipes become reachable in a live era, this becomes urgent rather than latent.

*Files: `src/world/corp_ai.cpp`*

### NR-268 — BL-417 step 1 is genuinely free on MSVC - measured, not assumed; but the item's "zero behavioural change" claim was not established when written
*decision taken on your behalf · raised 2026-08-17 · from BL-417 step 1 landing, 2026-08-17.*

BL-417's design calls step 1 a "no-op refactor, zero behavioural change". In float that is not automatic: net/(capex/net) and (net*net)/capex round differently, so the rewrite could have moved every candidate score, hence world evolution, hence every blessed golden. Measured rather than assumed. Against the pinned MSVC build, before and after the rewrite: ai_skill_harness output byte-identical across all 5 seeds, and spectator_determinism byte-identical with hashes played=855E07DE529684EC / spectated=5AC90B4ACE717FCF unchanged. No golden re-bless was needed. Separately noted: the GCC baseline numbers carried into this session differ wholesale from MSVC on every seed (e.g. seed 3 final 306437.4 GCC vs 498537.6 MSVC), so the blessed bands are compiler-bound - they are MSVC bands, and a GCC run is not a valid comparator for them.

**Why it matters.** The claim happened to be true, but it was true by luck of rounding rather than by algebra, and the item asserted it as established. Worth recording because the same shape recurs: any "algebraically equivalent" rewrite inside the scorer is a golden-affecting change until measured. The compiler-bound goldens are the more useful finding - a harness result from a non-MSVC build cannot be compared against a blessed band.

> **Recommendation:** Keep the measure-then-believe habit for scorer arithmetic. Consider whether the blessed bands should state their compiler in the harness output (ai_skill_harness already stamps "(MSVC...)" in its bless line, which is what made this catchable) and whether a GCC/WSL run should refuse to compare against them rather than reporting misleading failures.

*Files: `src/world/corp_ai.cpp`, `tools/verify/ai_skill_harness.cpp`*

### NR-269 — BL-439 landed and the AI immediately bankrupted itself on processors — the bands FELL where Sprint 19 predicted they would rise
*decision taken on your behalf · raised 2026-08-17 · from Landing BL-439 (the processing_facility build candidate), 2026-08-17.*

With a processor candidate in the scorer, rivals build 10-16 processors per seed across the benchmark set (69 total, against 0 before). Three of five seeds then go insolvent: seed 0 final net worth 498k -> -295k, seed 1 -> -89k, seed 4 -> -272k, each sitting below zero for 27-29 of 30 samples. Seeds 2 and 3 stay positive. ai_skill_harness is 18 rows red.

The estimator is NOT the culprit, and that was tested rather than assumed. The candidate was first scored with an inline revenue-minus-wages sum, then switched to estimate_prospective_profit (BL-162, the model done properly — habitability-scaled wages, real input cost, BL-193 stack decay). Same recipes chosen, same build counts, net worth moved only on seed 0 (-315k -> -295k). So the scorer is not mispricing the cost side; it is being promised a full run and handed a starved one.

The new per-building diagnostic reads: processor realised -6.24 to -11.70 per tick against a PREDICTED -0.38 to +0.08, on the same buildings, with extraction realised +22.80 where it sampled at all. Read the n: estimate_building_profit has no row for an idle or starved building, so the sample is biased toward the WORKING processors and the realised figure is the optimistic end.

**Why it matters.** This is the measurement Sprint 19 was opened to get, with the AI actually exposed to the economy for the first time. Its success criterion was written down in advance: the ai_skill_harness bands re-blessed DOWNWARD on 2026-08-16 should RISE, and a bless that does not raise them means the fix did not work. They fell, hard. Under that rule these numbers are a finding, not a bless — so the goldens are deliberately LEFT RED rather than re-blessed, and BL-439 task C is not done.

The -295k figure is also bigger than processor losses can explain on their own: 12 processors x 300 ticks x ~11/tick is ~40k, not 800k. The likely amplifier is BL-073 debt interest compounding once a corp crosses zero, which would make insolvency self-deepening rather than self-correcting. Not measured, and named here so it is not assumed either way.

> **Recommendation:** Ben owns the next call, because every lever here is cost-side calibration (the same boundary NR-266 stopped at). Three distinct options, which should not be conflated: (a) accept it as the honest finding, land BL-439 with the goldens red and a written reason, and let BL-436 fix the substrate — the bands re-bless once processing pays; (b) gate the candidate harder (a cash floor, a cap per corp) so rivals build processors without dying, which BUYS a green harness by hiding the defect the harness just found; (c) treat the debt-interest amplifier as its own item first, since a corp that cannot recover from one bad quarter makes every economy measurement noisier. My call taken in the meantime: do (a) and stop — nothing is tuned, nothing is blessed.

*Files: `src/world/corp_ai.cpp`, `tools/verify/ai_skill_harness.cpp`, `docs/development/backlog.json`*

### NR-270 — The AI can now build processors, so BL-436 can finally be measured against a rival that participates in it
*observation · raised 2026-08-17 · from Consequence of BL-439 landing, 2026-08-17.*

Every BL-436 measurement to date was taken on a world where the only processors were generated or warm-start ones, because the scorer could not build any (NR-265/NR-266). The benchmark set now carries 69 AI-built processors across five seeds, chosen by the AI on its own margin estimate.

**Why it matters.** It changes what the calibration sweep is measuring. NR-266 warned that the old narrative — corps spending income on processors that lose more at scale — described a mechanism that could not happen. It can happen now, so the sweep is worth re-running rather than re-derived on paper, and the three levers NR-266 named (the ~133:1 richness rate ratio, the 30.9% starvation rate, the three never-produced inputs) can each be measured with rivals responding to them.

> **Recommendation:** Re-run BL-436 calibration_sweep and tier_margin after BL-439 lands, before any cost-side number moves. Also resolves NR-267: BL-428 chain depth now has an AI player in principle, though whether a rival actually climbs a rung is unmeasured and the depth-gated recipes remain ancient-only.

*Files: `tools/verify/tier_margin.cpp`, `docs/development/backlog.json`*

### NR-271 — richness_reference enabled at the MEDIAN, not the mean — and the cost side still cannot be rescaled into a fix, because processing gross margin is negative before any upkeep
*decision taken on your behalf · raised 2026-08-17 · from Ben's call to enable richness_reference and rescale the cost side, 2026-08-17.*

ENABLED, and corrected while enabling it. The authored intent is "a typical deposit lands at ~1.0", and the value that delivers that is the MEDIAN richness, not the mean. tier_margin now prints the distribution (new R6b): p10 10.58 / p25 15.76 / MEDIAN 24.92 / p75 45.09 / p90 98.34 / p99 360.91, mean 53.34, max 72,321. The mean sits at the 78th percentile, so referencing against it ran the median tile at 0.47 and clamped 18% of all deposits flat at the richness_min floor — halving raw supply across the whole map. Set to 24.9.

The ratio defect is FIXED. Extraction went from 1666.69 revenue / 1659.18 net per building-tick to 14.63 / 7.89, and its capex payback from 0 ticks (instant, i.e. meaningless) to 12. Processing revenue is now 12.66 against extraction 14.63 — comparable, where it was 16.60 against 1666.69.

THE COST SIDE IS NOT THE REMAINING BLOCKER, and that is measured rather than argued. Processing reads revenue 12.66, input$ 12.87, maintenance 7.73, wage 1.14, net -9.08. Gross margin is -0.21 BEFORE any upkeep, so cutting maintenance and wages to ZERO still leaves a loss-maker. No rescale of the cost side can fix a negative value-add.

Separately, the -3M net worth figures are almost entirely COMPOUNDING, not operations. k_debt_interest_per_quarter is 0.02 and the rollout is 300 ticks: 1.02^300 = 380x. Per-building extraction nets +5 to +11 across every seed, so a corp cannot lose millions by operating; it dips a few thousand negative on processor upkeep and the interest multiplies that by 380. This confirms the amplifier hypothesis raised in NR-269.

**Why it matters.** It relocates the lever, and it lands on the option Ben raised first. Before the ratio was fixed, repricing could not close a 100:1 gap and was correctly ruled out. With the ratio fixed, price IS the binding constraint — processing revenue must rise 1.72x to break even at current upkeep (12.87 input + 8.87 opex = 21.74 needed against 12.66 earned).

It also means the two levers now compose, where before neither worked alone: refined-good prices +40% AND processing upkeep halved gives 17.72 - 12.87 - 4.44 = +0.41, i.e. break-even. Matching extraction at +7.89 needs more than that.

> **Recommendation:** Ben's call, three sized options, not mutually exclusive: (a) refined-good base prices +72% alone; (b) prices +40% with processing maintenance 10 -> 5 and base_wage 12 -> 8, which is the 'rescale the cost side' half actually doing work once it has a positive margin to protect; (c) cut recipe input quantities ~70%, which is a content change to recipes.lua rather than a price one. I have enabled the conversion at the median and stopped there — no price and no upkeep number has been touched, because tuning any of them against an unexplained -3M was the exact mistake NR-266 warns about, and the -3M is now explained.

Worth noting for whichever is chosen: 30.0% of processing building-ticks STARVE and 20.3% run with NO RECIPE SET. Half of all processor ticks earn nothing while paying maintenance. That is a defect to fix before, not after, calibrating against these means — the no-recipe share in particular looks like a generation or default-recipe gap rather than an economic outcome.

*Files: `scripts/economy.lua`, `tools/verify/tier_margin.cpp`, `tools/verify/ai_skill_harness.cpp`*

### NR-272 — The no-recipe defect was real, was in the LIVE game, and fixing it uncovered the actual blocker: coal has 1,671 tiles and zero mines
*decision taken on your behalf · raised 2026-08-17 · from Ben's call to fix the no-recipe defect before calibrating, 2026-08-17.*

FIXED, and it was not a harness artefact. author_building cannot set a recipe (ids are indices into a registry that does not exist yet at world-gen time), so processors are authored with no_recipe and backfilled later. That backfill lived INLINE IN app::load_economy, which made a world-generation invariant depend on the UI startup sequence — every path building a world without app (headless harnesses, --serve, --verify) ran processors that could never produce.

Worse, and this one hit the live game: load_economy runs the pass at app.cpp:955 and generate_background_firms authors MORE processors at :965. Every background firm processor kept no_recipe for the whole campaign, paying maintenance every tick, producing nothing, and reporting as ordinary idleness. tier_margin had already measured the shape of this in its own comment (26.5% recipe-less with no pass, 11.3% with the pre-firms pass only) and left the 11.3% standing as expected.

The loop is now world/s assign_default_recipes (corporation_generation.hpp). app calls it in BOTH positions; tier_margin retired its private copy for the shared one and calls it in both positions too; ai_skill_harness never ran it at all and now does. Measured: no-recipe ticks 20.3% -> 0.0%.

IT DID NOT IMPROVE THE ECONOMICS, and that is the finding. Processing net went 9.08 -> -10.44 per building-tick, because the processors that were silently unconfigured are now visibly STARVING: starvation rose 30.0% -> 46.3%. The buildings were never going to pay; the defect was hiding the reason.

**Why it matters.** The starvation now names one culprit. Coal (resource id 1) is the binding input on 54.0% of all starved processor ticks. It has 1,671 tiles carrying deposits and ZERO extraction sites targeting it, supplying 6.1 units/tick from ambient sources alone.

The cause is siting, not price and not upkeep: an extraction site targets richest_extractable(tile), and coal is the richest extractable on only 1.1% of the tiles that carry it. It is always out-ranked, so no mine — generated or AI-built — ever targets it, while the steel chain needs it. This is the same failure mode as R5s three never-produced inputs (ids 8/9/10), which NR-266 flagged as siting/reach rather than margin.

> **Recommendation:** Do NOT reprice yet. Repricing refined goods against a chain whose second input is unobtainable would tune around the shortage rather than fix it, and would have to be undone once coal is mined. The siting rule is the next item: an extraction site should be able to target a resource that is WANTED rather than merely richest — the demand signal already exists (the recipe input demand tier_margin R4 prints), and richest_extractable is a tile-local heuristic with no knowledge of it. Worth filing as its own backlog item; it is a corp_ai and generation change, not a calibration one.

Once coal is actually mined, re-run tier_margin and re-derive the price options in NR-271 (a/b/c) from the new numbers. The 1.72x revenue figure was measured on a starved chain and will move.

*Files: `src/world/corporation_generation.cpp`, `src/world/corporation_generation.hpp`, `src/core/app.cpp`, `tools/verify/tier_margin.cpp`, `tools/verify/ai_skill_harness.cpp`*

### NR-273 — The starting-corp pool was specialists-only by TIMING alone - now stated explicitly, and asserted
*decision taken on your behalf · raised 2026-08-17 · from BL-435 task E (the guard), while making the screen reachable to --verify.*

build_corp_choices was correct only because of WHEN it runs: the stage opens in the one frame before generate_background_firms, so m_world.corporations still held exactly the 8 specialists. That is a positional guarantee, one refactor away from silently becoming false - and it also blocked any verify hook, since a re-entered screen on a started world would have listed all 25-37 corps. Decision taken: skip is_background explicitly in build_corp_choices (a no-op on the live path, load-bearing everywhere else), and assert the split per seed in player_seed_sweep --guard G2 rather than trusting the ordering.

**Why it matters.** The pool being the specialist set is Ben’s own 2026-08-16 call and requirement R2. It was being upheld by an ordering nobody would notice breaking.

*Files: `src/core/app.cpp`, `tools/verify/player_seed_sweep.cpp`*

### NR-274 — Specialist processor coverage is now 6.92/8, not the 5.83/8 task B measured - something moved it further
*observation · raised 2026-08-17 · from player_seed_sweep --guard 24, run on resuming BL-435 at task E.*

BL-435 task B recorded specialists-with-a-processor at 2.96/8 before and 5.83/8 after its focus_asset_pattern fix. The same population measured today reads 6.92/8 over the same 24 seeds (166 of 192), with 0 seeds having none. Nothing in tasks E-F touches generation, so the extra ~1.1 came from work landed between - most plausibly BL-436's option-B extraction change or the dead-start trade holdings_range fix (2->3 draws reach the trade pattern's processing slot). Not investigated further: the guard's floor is 4.00/8 and the direction is the intended one.

**Why it matters.** The number quoted in BL-435’s own progress note is now stale, and coverage drifting upward without an item claiming it means the openings are being reshaped by side effects. Worth knowing which change owns it before anyone tunes against 5.83.

*Files: `tools/verify/player_seed_sweep.cpp`, `src/world/corporation_generation.cpp`*

### NR-275 — The corp-choice screen shipped with a clipped cell - caught by its first capture, which is the argument for capturing before blessing
*observation · raised 2026-08-17 · from BL-435 task E, scripts/verify/corp_choice.lua first successful run.*

The Holdings column rendered '2 proc / 0 extr / 1 other' clipped to '/ 1 otl' behind the Choose button - the table's column weights (0.42/0.16/0.24/0.18) gave the widest cell the narrowest stretch. Rebalanced to 0.36/0.14/0.22/0.28 off the measurement and re-captured clean. Also worth recording: the FIRST capture came back as a blank clear-colour frame, because the window is AlwaysAutoResize and ImGui sizes from the previous frame; the script now takes a warm-up capture as main_menu.lua does.

**Why it matters.** The screen was built and committed on 2026-08-16 without ever being photographed, and it carried a visible defect. Both failure modes here are cheap to hit again: a surface no automated path can reach, and a first-frame capture that looks like a working check but shows nothing.

> **Recommendation:** EYEBALL OWED, and no golden until then. screenshots/corp_choice.png is committed-capture-only on purpose: blessing a golden now would pin a layout Ben has never seen. --autostart-play will NOT show the screen (it auto-picks Surprise me), so seeing it live means starting a game from the menu. Once he has looked, bless corp_choice as a golden - the check is already written and stable.

*Files: `src/core/app.cpp`, `scripts/verify/corp_choice.lua`*

### NR-276 — The shared dependency cache never seeds anything - BL-302's directory names do not match what it looks for
*observation · raised 2026-08-17 · from Hit while configuring a fresh sub-agent worktree for BL-406/BL-404. The configure tried to DOWNLOAD SDL3 and failed offline, in a repo that carries a 120 MB cache of exactly that.*

CMakeLists.txt's seeding loop tests for '${IO_DEPS_CACHE}/${dep_name}-src/${marker}' - sdl3-src, lua_src-src, sol2_src-src, imgui_src-src. The cache on disk holds 'sdl3', 'lua_src', 'sol2_src', 'imgui_src'. No name matches, so every dep is classified COLD in any build tree that is not already warm, and the 'Deps: seeded from ...' line never prints. The main repo's build/ tree hides this completely: it has been warm since before BL-302, so nobody has configured cold there.

**Why it matters.** It defeats BL-302's entire purpose, and it only bites the case that motivated it - a fresh worktree for a parallel sub-agent, which is now the standard way work is split here. Offline, or on a slow link, a sub-agent cannot configure at all. Worked around this session by passing FETCHCONTENT_SOURCE_DIR_* explicitly (see bl406_build.bat, deleted before commit), which is a one-off, not a fix.

> **Recommendation:** One-line fix either way - point the loop at '${IO_DEPS_CACHE}/${dep_name}' or rename the four cache directories. Prefer changing the loop, since the cache directories are what the checkout actually carries. Worth a backlog item rather than a drive-by, because whichever way it goes wants a cold-configure check that would have caught it.

*Files: `CMakeLists.txt`*

### NR-277 — BL-406/BL-404 landed without re-tuning pull_fraction, though the ruling cleared it to move
*decision taken on your behalf · raised 2026-08-17 · from Ben's ruling 2026-08-15 on BL-406 said 'pull_fraction is re-tuned against whatever curve (c) produces' and cleared outpost prices to move. The session brief separately fenced off scripts/economy.lua repricing as Ben's open call, blocked behind BL-440.*

pull_fraction was left at its authored 0.50 and economy.lua was not touched. The two instructions point opposite ways, so the narrower one was taken. Measured effect of the change alone on seed 0: of three pulled resources on the test outpost, two pull at 2.8x their previous size (the counterpart wants far more than the lowest-id market did) and one is silenced outright by the now-real netting. Net direction is up, not down, so BL-263's price-floor support is not at risk - which was the specific failure mode the pre-change harness warned a naive netting would cause.

**Why it matters.** The mechanism is now correct and its magnitude is untuned. Nothing is broken by that - the pull got stronger, not weaker - but the 0.50 was chosen against a source that no longer exists, so it is a number with no remaining justification rather than a number known to be right.

> **Recommendation:** Fold the pull_fraction tune into BL-440's repricing pass rather than doing it here. It is the same class of call (an authored economy constant), it wants the same before/after price evidence, and doing it separately means tuning twice.

*Files: `scripts/economy.lua`, `src/world/market_clearing.cpp`*

### NR-278 — Outpost prices move with BL-406/BL-404 - any golden or band that reads them will shift, and none was re-blessed
*observation · raised 2026-08-17 · from Landing BL-406/BL-404. Ben's ruling explicitly cleared outpost prices to move, so this is an intended consequence, recorded rather than hidden.*

Every off-home market's demand for a pulled resource changes size this tick and therefore its resolved price changes: two of three pulled resources at 2.8x, one silenced to zero, measured on seed 0. Any world hash, golden capture, or ai_skill_harness band downstream of an outpost price will read differently. Nothing was re-blessed - the benchmark is deliberately red already (NR-269/NR-271/NR-272) and re-blessing into a red benchmark would bake in an unexamined number.

**Why it matters.** A moved golden that nobody names looks identical to a regression when it is next noticed. This is the naming. It also means the first bless after BL-440's repricing must be taken against a build that carries this change, not before it.

> **Recommendation:** Leave every golden and band red. Re-bless once, after BL-440 settles prices, rather than chasing each economy change through the capture set.

*Files: `src/world/market_clearing.cpp`*

### NR-279 — Two parallel agents both allocated NR-273/274/275 - the BL-406/BL-404 set was renumbered to NR-276/277/278, and its own commit message still quotes the old ids
*decision taken on your behalf · raised 2026-08-17 · from Integrating the three sprint19-bl417 sub-agent branches. Both the BL-435 worktree and the BL-406/BL-404 worktree branched from 5527984, where the highest id was NR-272, and each independently minted 273/274/275 for entirely different entries.*

Merge order decided it. BL-435 landed first and KEEPS NR-273 (specialists-only pool by timing), NR-274 (processor coverage 6.92/8) and NR-275 (clipped corp-choice cell). The BL-406/BL-404 set was renumbered: NR-273 -> NR-276 (the dep-cache seeding bug), NR-274 -> NR-277 (pull_fraction not re-tuned), NR-275 -> NR-278 (outpost prices move, nothing re-blessed). Cross-references were rewritten in backlog.json (BL-406 design), docs/economy/MARKETS.md and src/world/recipe_registry.hpp. THE ONE THING THAT COULD NOT BE FIXED is commit cab181b’s own message, which still reads "pull_fraction is NOT re-tuned - NR-274" and "No golden or band re-blessed - NR-275". Read against today’s file those point at two BL-435 entries. Substitute 277 and 278.

**Why it matters.** A naive merge would have silently dropped three entries - the exact failure the parallel-worktree notes warn about. It did not happen, but the near-miss is the point: id allocation off a stale local file is not safe once more than one worktree is open, and next_id.js only helps if it is actually run at authoring time rather than at landing time.

> **Recommendation:** Nothing to undo. Worth considering whether NEEDS_REVIEW ids should be minted the way backlog ids are (next_id.js scanning all branches), or whether a verify check should simply assert no duplicate NR id - the latter is a five-line check and would have caught this before the merge rather than during it.

*Files: `docs/development/NEEDS_REVIEW.json`, `docs/development/backlog.json`, `docs/economy/MARKETS.md`, `src/world/recipe_registry.hpp`*

### NR-280 — BL-406/BL-404 moved outpost prices but did NOT move the spectator rollout hash - NR-278 over-predicted its own blast radius
*observation · raised 2026-08-17 · from Integration gate on sprint19-bl417 after merging all three sub-agent branches, plus a control build of spectator_determinism at the pre-merge commit 8cc2981.*

spectator_determinism fails one assertion - R2 byte-identity against the pinned golden 855E07DE529684EC. Measured on BOTH sides of the merge, the observed hash is the SAME value, 6D546B281FFA4A68, and the spectated hash is 8606FA466D259B09 on both. So the failure is entirely pre-existing (the golden was last blessed at 3b9ffd2, before BL-439/BL-436/BL-440 landed on this branch) and the market-clearing change contributed nothing to it. Every other assertion in that harness passes, including both reproducibility checks and the R2 prohibition check.

**Why it matters.** NR-278 states that any world hash downstream of an outpost price will read differently. On this rollout it does not, which is a fact worth having before anyone treats a moved hash as evidence the pull change reached something. The likely reason is that the seed-0 300-tick spectator rollout does not exercise an off-home market enough for the pull to register - if so, the harness is blind to exactly the change BL-406 made, and that is a coverage gap rather than a reassurance.

> **Recommendation:** Do not bless. When the goldens are eventually re-blessed as one deliberate pass (NR-269 owns that call), re-blessing spectator_determinism’s R2 golden is a one-line change and should ride along. Separately worth checking whether the rollout reaches an off-home market at all.

*Files: `tools/verify/spectator_determinism.cpp`, `src/world/market_clearing.cpp`*

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

### NR-246 — building_management_shell.lua verify script targets the deleted Buildings tab
*observation · raised 2026-08-15 · from Same BL-431-follow-on delivery that deleted the construction panel's Buildings tab.*

scripts/verify/building_management_shell.lua calls verify.show_panel('construction', true) then verify.construction_view(1) to land on the (now-deleted) Buildings tab and capture its production-method/profit/workforce UI shell. View 1 no longer exists - the construction panel is queue-only (view 0). The script was left un-rewritten rather than adapted, per this delivery's scope.

**Why it matters.** The script still runs (construction_view(1) just sets an int nobody reads for dispatch anymore) but its capture no longer shows what its own comment says it verifies - the workforce/profit UI shell now lives on the building Selection card's Workforce/Profitability pages instead, reached via selection + verify.fold('building_metric', page) rather than construction_view.

> **Recommendation:** Rewrite building_management_shell.lua to select a player building and use the Selection card's pager (building_pages() order, as of the 2026-08-15 playtest rework: Profitability/Method/Workforce — Chain/Depth/Lifecycle are gone, folded into Profitability or moved to the action grid) instead of construction_view(1), or fold its intent into a new selection-card-focused check and retire this one.

> **RESOLVED.** Resolved 2026-08-16 with BL-428 slice 2. building_management_shell.lua rewritten: it now selects a player PROCESSING building (falling back to any player building) and walks the Selection card's pager, capturing all three pages separately. It also needed a new verify verb — verify.building_page(n) — because fold('building_metric', k) sets the drill KEY, not the page, so the first rewrite still captured page 1 three times. Confirmed by capture: the Method page now shows 'Method (2/3)' with both alternates and the Switch control. That first honest photograph immediately surfaced a real layout defect, filed as NR-255.

*Files: `scripts/verify/building_management_shell.lua`, `src/ui/selection_panel.cpp`*

### NR-251 — cross_group_multiplier (6.0x) is a first-cut number, needs playtest
*decision taken on your behalf · raised 2026-08-15 · from BL-434 sub-facility groups: tiered recipe-switch cost.*

recipe_switch_params::cross_group_multiplier defaults to 6.0, putting a cross-group retool (e.g. Bloomery -> Loom) at 72cr against the existing 12cr intra-group switch_cost. That is well short of a fresh processing_facility all-in capex (~400cr: 200 build_cost + ~200cr steel materials at current prices), so at these numbers a cross-group retool is NOT yet the more expensive option versus building fresh - it is a real but not decisive nudge.

**Why it matters.** The design brief asked for a multiplier large enough that build-fresh plausibly beats retool in typical cases (5-8x suggested, deliberately not full parity). 6.0x sits in that suggested band but was not validated against actual playtest economy pacing.

- Leave at 6.0x and revisit once Metal Foundry / Food Processing / etc. groups are actually played against each other.
- Raise toward the point where cross-group cost approaches or exceeds build_cost, making "just build another" the clearly cheaper move in most cases.

> **Recommendation:** Leave at 6.0x for the first playtest pass; retune once real corp behaviour is observed switching between groups.

> **RESOLVED.** Superseded 2026-08-16: Ben retired cross-group switching entirely rather than tuning its price - the only way to change a building's group is now dismantle + rebuild via the tile selector. cross_group_multiplier no longer exists (removed from recipe_switch_params, scripts/economy.lua, recipe_registry.cpp's loader); try_switch_recipe refuses a cross-group target outright (recipe_switch_result::cross_group). This tuning question is moot, not answered.

*Files: `src/world/recipe_registry.hpp`, `scripts/economy.lua`*

### NR-255 — Method page: the per-method profit figure overlaps the method name
*observation · raised 2026-08-16 · from screenshots/building_management_method.png, captured by the rewritten building_management_shell.lua (NR-246 fix).*

The Method page's rows draw the method name left-aligned and the profit figure right-aligned to a profit column, but at the Selection card's current width the two collide: the capture shows 'Food Rations' with '7/tick' printed through it and 'Miller' with '4.4/tick' through it. The profit column offset (profit_col_w) is computed against a row width that the narrow Selection band does not actually provide.

**Why it matters.** This is the surface BL-430/BL-431 built to make alternate production methods choosable, and its two load-bearing numbers are the ones being overprinted — the player cannot read either the method name or its profit cleanly. It went unseen because the old verify script captured the deleted Buildings tab (NR-246) and the new one had no golden, so nothing was comparing this page to anything. Found the first time the page was actually photographed.

> **Recommendation:** Either right-align the profit into a reserved column measured off the ACTUAL child width, or drop the profit to a second line on narrow layouts. Worth a golden on all three building pages once the layout is settled, so the next regression is caught by comparison rather than by eye.

> **RESOLVED.** Fixed 2026-08-16, same session it was raised. Two causes, both real. (1) DRAW ORDER: the name drew at full length and the right-aligned profit was painted over it. The row now measures the profit FIRST (at its own 1.1x scale — a width taken at 1.0x would under-reserve) and fits the name to what is genuinely left, routed through ui::fit_text (box 9, table_cell) so an over-long name elides with the full string one hover away and is recorded in BL-215's overflow ledger rather than clipping silently. (2) WIDTH: fixing the draw order alone just turned the name into '...', because the row was `avail * 0.62` = 160 px trying to hold 248 px of content — measured: pip 27.8 + name 103 ('Food Rations') + gap 8 + profit 70 + Switch 33 + pad 6, leaving the name 16 px. The 0.62 factor was arbitrary; the 2026-08-15 'slimmer strip, not a denser fit' call was about row HEIGHT (1.5 button-heights, against the square tiles it replaced) and explicitly argues against cramming. Row is now full available width capped at 280. Verified by capture: both rows read name + profit cleanly.

*Files: `src/ui/selection_panel.cpp`, `scripts/verify/building_management_shell.lua`*

### NR-260 — A processing facility earns LESS than the extraction site it replaces — so BL-435's 'better opening' is deeper, not richer
*decision taken on your behalf · raised 2026-08-16 · from BL-435 task B (focus_asset_pattern fix), measured with player_seed_sweep and ai_skill_harness before and after.*

Ben's call was to raise how many of the 8 selectable specialists open with a processing facility. Root cause was not tuning: focus_asset_pattern's own comment promises extraction corps 'a single processor', but the processor sat at slot 3 while extraction holdings draw 3-4, so only a 4-draw ever got one. Moving it to slot 2 makes the code match its documentation and works exactly as intended - specialists with a processor went 2.96/8 (37%) to 5.83/8 (73%) across 24 seeds, and the one seed in 24 where NOT ONE specialist had a processor is gone. The player's own generator-assigned corp went 11/24 to 18/24 'playable'. THE SIDE EFFECT IS THE PROBLEM: every measured net worth fell. Player corp, seed 0: 3 extraction/0 processing = 55,179 cr, becomes 2 extraction/1 processing = 28,288 cr - HALVED, same corp, same seed, one building swapped. ai_skill_harness's five seeds all fell too: -71%, -37%, -9%, -20%, -38% on final net worth (only seed 0 fell outside its golden band).

**Why it matters.** It undercuts the premise BL-435 is built on. The item argues a pure-extraction corp is a POORER opening because the Method page (BL-430/431) and the chain-depth ladder (BL-428) have nothing to stand on - which is true about DEPTH and appears to be false about MONEY. In the current economy a mine outearns the processor that replaced it, so the corp the selection screen would present as the better pick is the financially weaker one. Two readings, and they need different responses: (a) this is correct and interesting - depth costs money, the player trades income for reach, and the selection screen should say so rather than implying one option dominates; or (b) processing is simply underpriced relative to extraction, which is an economy-balance defect that predates this item and that BL-435 has merely made visible. I have NOT re-blessed ai_skill_harness's seed-0 band, deliberately: re-blessing would bake a 35%-ish economy-wide net-worth reduction into the baseline as though it were intended, and that is Ben's call, not a bookkeeping step.

- A) Keep the generation change and treat the income drop as the honest cost of depth. The selection screen presents holdings without ranking them, and NR-260 closes.
- B) Keep the change, and file the extraction-vs-processing margin as its own economy-balance item - the drop is evidence that processing is underpriced, independent of BL-435.
- C) Narrow the generation change (e.g. only extraction corps drawing 4 holdings keep the processor, restoring most of the old economy) and accept a smaller rise in selectable processors.
- D) Revert task B entirely and let the selection screen do all the work, choosing from the 1-5 specialists that already have a processor.

> **Recommendation:** B. The change does exactly what it was asked to do and fixes a real code-vs-comment contradiction; the income finding is genuine information about the economy rather than a reason to undo it. But A vs B is Ben's - and if the answer is that processing SHOULD outearn extraction, the balance item wants filing before the selection screen teaches players otherwise.

> **RESOLVED.** Ben's call 2026-08-16: option B. The generation change STAYS - it does what it was asked to do and fixes a real code-vs-comment contradiction - and the income finding is filed as its own economy item, BL-436 (PROCESSING_UNDEREARNS_EXTRACTION, priority A, v0.1.18), carrying the full before/after measurement and four candidate causes to measure before tuning (input-cost-vs-output-price, wages/maintenance, market depth for inputs, throughput). ai_skill_harness re-blessed to the post-change numbers with the reason in the file, including the explicit note that these bands should RISE when BL-436 lands and that a bless which does not raise them means the fix did not work. BL-435 continues from task C.

*Files: `src/world/corporation_generation.cpp`, `tools/verify/ai_skill_harness.cpp`, `tools/verify/player_seed_sweep.cpp`*

### NR-265 — BL-436's design blames a scoring curve for something structural - the AI has no processing_facility build candidate at all
*decision taken on your behalf · raised 2026-08-17 · from Found while verifying BL-417 (AI build score is quadratic) before touching it, 2026-08-17.*

BL-436's design says corp_ai's build scorer "is being asked to prefer processors on a payback curve that does not reward them". Verified against the source: corp_ai.cpp emits corp_verb::build from exactly two sites - the ranked_sites loop hard-coded to building_type::extraction_site (~line 566), and one military_base candidate (~line 942). There is no processing_facility candidate anywhere in the scorer. The seam is not at fault: corp_command.cpp builds a processor, recipe and all, when a command asks for one (BL-388). The scorer simply never asks. I have filed this as BL-439 (AI never builds processors, priority A) rather than adjusting BL-436's premise silently.

**Why it matters.** It reassigns the blame BL-417 was filed to test. "The AI prefers mines" is structural - processors are not candidates - not an artefact of the net^2/capex curve. BL-417 step 1 (write the curve out honestly) is still worth doing and has landed; but BL-417 step 2, "decide whether the quadratic bias is wanted", is now a question about a curve that only ever ranks extraction sites against each other. Retuning that curve cannot make a rival build a processor.

> **Recommendation:** Treat BL-439 as the item that actually addresses "the AI prefers mines", and re-scope BL-417 step 2 to what it really is: a choice about how extraction sites are ranked relative to one another and to the flat-scored military base. Do not tune the curve expecting a processor to appear.

> **RESOLVED.** Confirmed independently 2026-08-17 and acted on. corp_verb::build was emitted from exactly two sites in corp_ai.cpp — the ranked_sites loop (extraction_site) and one military_base — with no processing_facility candidate anywhere. BL-439 adds one: sited on the corp own asset tiles, recipe chosen from the browse space and crossed to the absolute id, gated on chain depth and on real input access (pool + local market inventory against the tick own coverage threshold), priced by estimate_prospective_profit, on the same score curve and the same solvency/glut/reserve gates as extraction. Guarded by ai_skill_harness R5, which was run against the PRE-change build first and failed by construction there (processors_gained = 0 on all five seeds). What the change then revealed is NR-269.

*Files: `src/world/corp_ai.cpp`, `docs/development/backlog.json`*

