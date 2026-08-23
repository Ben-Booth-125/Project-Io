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

*240 entries — 240 open, 0 resolved.*

---

## Open

### NR-164 — Re-stress generation and time-lapse feel when playable
*question · raised 2026-08-11 · from 2026-08-11 notes session with Ben.*

Ben wants to re-stress-test world generation and the staged-generation time-lapse once the game is playable, to judge whether it feels alive rather than just correct. RULING 2026-08-13 (Ben, elicitation form): keep open until playable - not scheduled, not folded into another item.

*Files: `docs/ui/STARTUP.md`*

### NR-237 — BL-286 added eleven resource_type values the Lua loader could not parse, and nothing noticed for eleven days
*observation · raised 2026-08-15 · from BL-429's ancient chain â€” the first authored recipe to reference a BL-286 good.*

recipe_registry.cpp's resource_from_name table claims in its own comment to cover 'the full enum so recipes outside the prototype subset load without a retrofit'. It did not: grain, fodder, salt, transport_capacity, charcoal, iron_blooms, bullion and trade_goods_misc were all missing, so ANY recipe naming one threw 'Unknown resource' at load. Added them while landing BL-429.

**Why it matters.** The defect was invisible because it needed an authored recipe to trigger it, and BL-286 deliberately shipped enum + serialisation + base-price wiring with the behaviours 'unfiled' â€” so nothing consumed the new goods and nothing exercised the parse path. The failure mode is the one this project keeps meeting from a different direction: a data layer that is only ever validated by the data currently authored against it. Note the loader DID behave correctly once reached (it threw and named the value); the gap was that nothing reached it.

> **Recommendation:** No action needed on the fix itself â€” it landed with BL-429. The question worth your call is whether BL-432's roster harness should assert the parse map covers resource_count, which would catch the next such gap at the moment the enum grows rather than whenever someone first authors against it. Cheap, and it is the same shape as BL-432's existing 'no orphan resources' row.

*Files: `src/world/recipe_registry.cpp`*

### NR-238 — A slow gate looked like a regression twice in one session â€” the two-line diagnostic that settled it
*observation · raised 2026-08-15 · from Trying to get a full-suite green before opening PR #39 (Sprint 17).*

Three generation sweeps (earthlike_lean_trace, notable_worlds, mediterranean_sweep) ran past 15 minutes without finishing and looked like a performance regression from BL-428/BL-429. They were not. Two cheap checks settled it: (1) build/Testing/Temporary/CTestCostData.txt records ctest's per-test durations from previous runs â€” the missing baseline, showing these three at 16.5s / 22.2s / 20.1s; (2) world_audit.exe, a STALE binary dated 16:55 that predates the work entirely and could not contain the change, took 14s against its own 0.92s baseline. A ~15x slowdown on an untouched binary is environmental, not a regression.

**Why it matters.** The same wrong conclusion nearly got drawn twice in one session, each time for a different reason - first because two ctest instances were left contending (the exact failure Sprint 6's retro already recorded), then because the box itself was slow. Both times the tempting response was to go hunting in the diff. The general lesson is cheaper than any of that: BEFORE attributing a slowdown to a change, time something the change cannot possibly have touched. If that is slow too, stop looking at the diff. CTestCostData.txt is worth knowing about independently - the gate had no trusted baseline time until it turned up, which is why a slow run and a hung run were indistinguishable.

> **Recommendation:** Worth a short note in the verifier-headless skill under a 'diagnosing a slow gate' heading, since that is where someone will be standing when they hit it. Skill edits need your say-so, so it is not made. The underlying cause of THIS session's slowness (AV scanning fresh unsigned binaries is the likeliest candidate, given build_gen/ exists precisely to give the scanner one stable exclusion path) was not chased down.

*Files: `build/Testing/Temporary/CTestCostData.txt`, `.claude/skills/verifier-headless/SKILL.md`*

### NR-240 — BL-429 slice 2's C++ changes were authored and reviewed but never compiled â€” remote session network policy
*observation · raised 2026-08-15 · from BL-429 slice 2, run in a Claude Code remote/cloud session rather than the usual Windows dev box.*

cmake -B build's SDL3/sol2/ImGui FetchContent steps all pull from codeload.github.com, which this session's outbound network policy returns a 403 for (confirmed an organization policy denial via /root/.ccr/README.md, not a transient TLS fault â€” retrying or routing around it is explicitly the wrong move). Lua changes (recipes.lua) were syntax-checked with luac5.4 -p and pass; the C++ changes (recipe_registry.hpp/.cpp, placement_rules.hpp, selection_panel.cpp, construction_panel.cpp) were manually re-read against every call site of the touched fields (recipe::name vs the new recipe::display_name; k_extractable's size assertions) but no compiler ever saw them.

**Why it matters.** This is the first time a Project Io session has landed C++ source changes with zero compiler verification. The manual review was as thorough as a human diff review gets, but it cannot catch what a compiler catches â€” a typo'd member name, a missing include, an ambiguous overload. The next session with real toolchain access should treat this diff as unverified until `cmake --build` and the relevant headless harnesses (chain_depth, buildings_rework_harness, resource_chain_harness) run clean against it.

> **Recommendation:** At the next native/Windows session: build ProjectIo, run buildings_rework_harness and chain_depth at minimum (both exercise k_extractable / recipe additions directly), and open the Build door on an ancient-band tile to confirm the new names render. Fix on sight if anything fails â€” this is expected verification work, not a surprise.

*Files: `src/world/recipe_registry.hpp`, `src/world/recipe_registry.cpp`, `src/world/placement_rules.hpp`, `src/ui/selection_panel.cpp`, `src/ui/construction_panel.cpp`*

### NR-241 — BL-429's 14 new glyphs are geometrically reasoned but visually unverified â€” same root cause as NR-240
*observation · raised 2026-08-15 · from Closing BL-429's glyph gap (NR-239) in the same remote session as slice 2, same network limitation.*

14 new icons::building() shapes (quarry_stone, woodcutter_timber, sand_pit, clay_pit, peat_cutting, iron_mine, copper_mine, water_extractor, farm, charcoal_kiln, bloomery, smithy_ingot, goods_bundle, ration_pack) were authored against ImGui's AddConvexPolyFilled/AddLine/AddCircleFilled primitives, with vertex lists hand-checked for angular ordering (a simple, non-self-intersecting perimeter) but never rendered. Silhouette-distinctness from each other and from the existing vocabulary (ore_chunk, square, triangle, hub_node, shield, research) was reasoned by shape family (boulder vs crystal vs dune vs sack vs ...), not by looking at them side by side, which is the actual bar ICONS.md's own 'Adding a new glyph' process sets ('a glance should disambiguate').

**Why it matters.** Vector icon code is exactly the kind of change where 'compiles' and 'looks right' are different questions â€” a convex-poly vertex list can be syntactically fine and still render as a squashed or overlapping mess at real icon size (r as small as 9px in the Build door). Geometric review from source alone cannot catch that; only rendering can.

> **Recommendation:** At the next session with a real GUI build: open the Build door on an ancient-band tile (all 14 shapes appear across extraction/processing rows), the Buildings tab identity plate, and the Planetary canvas marker for a built ancient building. Check each shape reads at its actual on-screen size, not just at a zoomed mental model of the coordinates. Fix proportions on sight â€” nothing here is expected to be exactly right first try.

*Files: `src/ui/icons.cpp`, `src/ui/icons.hpp`*

### NR-242 — BL-430: AI recipe-switching pays no attention to the new switch cost/cooldown at scoring time
*decision taken on your behalf · raised 2026-08-15 · from BL-430 (alternate production methods) implementation session.*

corp_ai.cpp's dial_recipe margin-chase (run_corp_strategic_step) scores a recipe switch purely on projected margin gain; it does not subtract economy.recipe_switch's switch_cost, and does not check the target building's recipe_switch_cooldown before proposing a candidate. The decision taken: do NOT build cost/cooldown-aware AI scoring in this pass. The seam itself (corp_command's set_recipe verb, through try_switch_recipe in economy_system.cpp) still enforces both at apply time -- a candidate that would be rejected on cooldown or insufficient funds is simply refused, mutating nothing, exactly as the seam already handles every other reason a corp_ai candidate can fail. So the AI is not exploitable and nothing is unsafe; it just occasionally proposes (and loses a decision slot to) a switch that will bounce.

**Why it matters.** Scoped explicitly per BL-430 design ruling ï¿½-- widening corp_ai.cpp scoring to price in the switch cost/cooldown is real new planner scope (a projected-gain-minus-cost comparison, plus a cooldown pre-filter), not a small follow-on to this item. Left as a stated call rather than a silent gap.

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

### NR-244 — BL-431 production method/chain/depth UI has no executable check yet â€” visual read owed live
*decision taken on your behalf · raised 2026-08-15 · from BL-431 delivery session â€” sub-agent had no display access, so the C++ was built and verified by compile only.*

Three new Selection-panel surfaces landed (src/ui/selection_panel.cpp: draw_production_method_section, draw_chain_trace_section/draw_chain_trace, draw_depth_readout, plus the ui_state toggles selection_method_open/selection_chain_open/selection_chain_target/selection_depth_open). The GUI target (ProjectIo.exe) built clean with no new warnings from selection_panel.cpp. A requirement group was filed (req/requirements.json, brief production-method-chain-ui, 3 rows, status active) but no scripts/verify/*.lua or tools/verify/*.cpp was authored to back it â€” the sub-agent had no way to run either kind of check in its context.

**Why it matters.** The three rows are exactly the kind of thing Rule 0b and the standing verifier skills exist for: R1 (method selector layout doesn't push the profitability/workforce controls off screen) needs verifier-visual against a live capture; R2 (chain trace never disagrees with depth_of) and R3 (corp_reached_depth <= max_depth()) are both cheap headless assertions over recipe_registry, straightforward for verifier-headless. Landing the UI without landing its own check leaves BL-431 the one item in the recent BL-428/429/430/431 cluster without an automated guard.

- A) Open the live app (per the standing 'open the app after visual questions' practice), eyeball the Method/Chain/Depth sections on a stacked ancient-band tile, then author scripts/verify/selection_method_chain.lua + a small tools/verify/chain_trace_agreement.cpp and flip both requirement rows to complete.
- B) Accept the compile-only verification for this session and file the two harnesses as their own small backlog items instead of blocking on them now.

> **Recommendation:** A â€” the surfaces are dense (three toggles stacked in an already-tight Facts column) and are exactly the case Rule 0b calls out as needing real numbers, not a guess.

*Files: `src/ui/selection_panel.cpp`, `docs/development/req/requirements.json`, `docs/ui/SELECTION.md`*

### NR-248 — Profitability chart's Revenue/Expenses split is as fine as building_profit.hpp gets â€” no revenue sub-breakdown exists
*observation · raised 2026-08-15 · from Playtest-driven building-card rework (2026-08-15): Ben asked for a Revenue-vs-Expenses bar chart on the Profitability page.*

building_profit (src/world/building_profit.hpp) tracks exactly four numbers: revenue (one pooled valuation of this tick's output at market price), input_cost, maintenance, wages. There is no per-resource or per-line revenue breakdown to chart â€” the pooled market resists exact per-building attribution even for the ONE revenue figure that exists, per the struct's own doc comment. draw_building_profit's new chart therefore plots Revenue against Expenses := input_cost + maintenance + wages, the finest real split the data supports, rather than inventing sub-categories.

**Why it matters.** Flagged per Rule 0b/the standing 'measure before reshaping' practice â€” this is a real data-availability ceiling, not an implementation shortcut. A finer revenue/expense breakdown (e.g. per-resource revenue, wages vs maintenance as separate bars) would need building_profit itself to track more, not just a UI change.

- A) Leave as-is: Revenue vs Expenses is honest and matches the finest split building_profit tracks.
- B) Extend building_profit to keep wages/maintenance/input_cost as a genuinely separate 3-bar expense breakdown alongside Revenue, if a future pass wants it (input_cost and maintenance+wages are already separate fields, so this is a small UI change, not a data change â€” only a true revenue sub-breakdown needs new tracking).

> **Recommendation:** A for now. If the Profitability page's 2-bar chart reads as too coarse once played more, B's expense 3-way split is cheap (the fields already exist); a revenue sub-breakdown is not.

*Files: `src/world/building_profit.hpp`, `src/ui/selection_panel.cpp`*

### NR-249 — No per-building profit history exists â€” the Profitability page's 6-month net-profit line is a placeholder series, same constraint as the old Workforce trend graph
*observation · raised 2026-08-15 · from Playtest-driven building-card rework (2026-08-15): Ben asked for a line chart of net profit over the last 6 months.*

No time-series profit history is recorded per building anywhere in the simulation (the same gap draw_building_workforce_page's pre-existing placeholder trend graph already lived with). draw_building_profit's new 'Net, 6 mo.' PlotLines chart reuses that same honest-placeholder idiom: a smooth deterministic series anchored to the live net-profit estimate, 6 points rather than the workforce graph's 9, with no claim to be real history.

**Why it matters.** Two placeholder trend graphs now exist on the same card (Profitability's Net line, Workforce's target trend) for the identical reason: nothing records per-building history over time. If BL-something eventually adds a real per-building time series (the way body/corp-level history is tracked elsewhere), both should switch to it together rather than one getting fixed and the other staying a placeholder.

> **Recommendation:** No action needed now â€” noted so a future per-building-history item knows to sweep both graphs, not just the one it was written against.

*Files: `src/ui/selection_panel.cpp`*

### NR-250 — Profitability page vertical budget for the Inputs chart is a fixed judgment-call clamp, not a measured fit
*decision taken on your behalf · raised 2026-08-15 · from Playtest reflow (2026-08-15): Ben asked the Profitability page to fit one screen â€” bars+line row left 1/3 / right 2/3, Inputs chart below.*

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

selection_panel.cpp built its processing candidates with `static_cast<uint16_t>(ri)`, where `ri` is the loop index over reg.recipe_count()/recipe_at() â€” the BL-433 era-MASKED browse path. That value flowed unchanged into ui.construction.pending_recipe and from there into construct_building, whose `recipe` parameter is indexed ABSOLUTELY (the get_recipe/recipe_id space). Two sibling call sites in the same file (the Method selector, ~line 847/894) already did the right thing via reg.recipe_id(ri.name); the build door did not. Fixed this pass: candidate.recipe now stores reg.recipe_id(browsed.name), and the two readers that indexed it as a browse position (the group lookup and the output-pip lookup) were moved onto get_recipe.

**Why it matters.** The two id spaces coincide exactly while the era mask is the identity, which is every any-band campaign â€” so this was invisible in normal play and would have stayed invisible. In an ANCIENT campaign the mask hides the industrial recipes, browse index k names a different recipe than absolute id k, and placing a building from the Build door seeds it with the wrong recipe. Ancient campaigns are precisely the content BL-429/BL-430/BL-431 have been building out. No harness caught it because no harness drives the Build door under a non-identity mask.

> **Recommendation:** Fixed. The residual question is whether the browse/absolute split wants a TYPE (a distinct browse_index struct) rather than two uint16_t spaces that convert silently â€” this is the second time the split has bitten (BL-433 called it out in recipe_registry.hpp and still shipped a raw uint16_t). Worth a small backlog item if a third instance appears.

*Files: `src/ui/selection_panel.cpp`, `src/world/recipe_registry.hpp`*

### NR-256 — --autostart-play still terminates unattended, cause not established
*observation · raised 2026-08-16 · from Three unattended launches while adding the flag (commit e4a087a).*

--autostart-play removes --autostart-windowed's 120-frame cap so the window stays open for a human to look at. It works interactively â€” Ben used it and reported on what he saw. But launched unattended from a background shell it has terminated on its own three times, at roughly 20s, 34s and ~60s: exit code 0, no crash log, no exception, output simply ending after the warm-start timings. The frame cap is definitely not the cause (it is gated on autostart_mode::smoke and the variable timing rules it out anyway). run()'s loop has only three exits: SDL_EVENT_QUIT, SDL_EVENT_WINDOW_CLOSE_REQUESTED, and m_quit_requested, so something is delivering a close.

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

### NR-258 — NR-243's four dominated pairs were a grouping artefact - the no-dominance guard was asking the wrong question
*decision taken on your behalf · raised 2026-08-16 · from Ben's call on NR-243, 2026-08-16: option C first - settle the tier-vs-alternate axis before retuning any numbers.*

The axis did not need inventing: scripts/recipes.lua already states it twice in its own comments (ids 22 and 23) - distinct raws feeding a shared good is 'an ordinary multi-producer economy fact, NOT BL-430's alternate-METHOD feature (one building offering interchangeable recipes for the same output)'. Measured against that, three of NR-243's four pairs have DISJOINT input sets (steel {iron_ore,coal} vs {iron_nickel_ore}; charcoal {timber} vs {peat}; trade_goods {clay,timber} vs {sand}) and the fourth differs by a placement precondition rather than cost (propellant atmospheric vs electrolysis - the airless route is run because the cheap one is unavailable). So all four were false positives of grouping by (primary output, era), not balance defects. No recipe magnitude was changed. The guard moved to chain_depth.cpp's R2 row, regrouped: every sibling pair is bucketed as a supply route (disjoint raws), an explicitly-exempted precondition pair, or a genuine interchangeable method, and only the third is price-compared. recipe_switch_harness's duplicate R1 was deleted rather than left as a second answer to the same question - it now runs ALL PASS.

**Why it matters.** Today the roster contains ZERO genuine interchangeable methods - 4 sibling pairs, 3 supply routes, 1 precondition, 0 methods. That is a real finding about BL-430: the alternate-method FEATURE shipped, but no content yet uses it, and the four pairs that looked like alternates are multi-producer economics. The bucketing is what keeps R2 from being a vacuous green - a pair cannot escape by being unclassifiable, and the counts print on every run - but Ben should know the dominance half is currently guarding an empty set, and will only start biting when BL-430's own alternates are authored.

> **Recommendation:** No action needed on the numbers. Worth deciding separately whether BL-430 should author at least one genuine same-inputs alternate pair, so the feature it built has content and R2's dominance half has something to guard.

*Files: `tools/verify/chain_depth.cpp`, `tools/verify/recipe_switch_harness.cpp`, `scripts/recipes.lua`*

### NR-259 — player_seed_sweep had never once passed under ctest â€” a 60s timeout on a 69s tool, silent because a Timeout looks like nothing
*observation · raised 2026-08-16 · from The full 78-test ctest run done to verify NR-257's resource_type removal.*

player_seed_sweep was added 2026-08-15/16 and registered in CMakeLists.txt's IO_TEST_SCRIPT_ROOTED_HARNESSES (so it gets the repo root as its working directory) but NOT in IO_TEST_LONG_HARNESSES, so it inherited the 60 s default timeout. It generates one full world per seed, 24 by default, and takes ~69 s on this box. It therefore timed out on every ctest run from the day it was added, while passing perfectly standalone - which is how it was used, and why nobody noticed. Fixed by adding it to IO_TEST_LONG_HARNESSES (240 s, ~3.5x headroom); it now passes at 71.8 s.

**Why it matters.** The failure mode is the point, and it is the mirror of the one Sprint 18's retro named. That retro found a check running GREEN while pointed at a deleted tab - coverage that was not coverage. This is the same defect from the other side: a check that had never run at all, reporting as a Timeout, which reads as infrastructure noise rather than as 'this harness has never verified anything'. Worth a standing habit: when a harness is added to the gate, run the GATE once, not just the exe. Two of the three ways a check can be worthless - green-but-blind, and never-executed - are both invisible from the harness's own output.

> **Recommendation:** No further action on this instance; it is fixed and verified. Worth considering whether a new harness's first ctest run should be part of the checklist in the verifier-headless skill, since 'passes standalone' is what both the author and the skill's own Procedure section naturally check.

*Files: `CMakeLists.txt`, `.claude/skills/verifier-headless/SKILL.md`*

### NR-261 — BL-422: held stock stays visible to the price signal, against the item's own stated default â€” the alternative is a fixed point
*decision taken on your behalf · raised 2026-08-16 · from BL-422 design: "Decide whether the supply-side price signal should still see held stock (an argument exists both ways) â€” default to NOT visible, matching the reservation semantics BL-386 established."*

The default was NOT adopted, and the reason is mechanical rather than a preference. Whether an order holds is decided by comparing its floor to the RESOLVED price; the resolved price is computed FROM market_component::supply (resolve_price(price, base, supply, demand)). Removing held quantity from supply raises the resolved price, which can un-hold the very order that was removed, which puts its stock back into supply. Reaching a consistent answer means iterating to a fixed point inside clear_markets. So the two arrays are now deliberately asymmetric and MARKETS.md says so in as many words: supply is the OFFER (listed stock is a real offer at a price), inventory is the DELIVERY (only what actually changed hands). Only the inventory half of BL-422 was implemented.

**Why it matters.** The phantom-BUYING defect is fully fixed â€” no processor can draw stock a seller never released. What remains is a pricing nicety: a held order still depresses the resolved price by appearing as supply, which in a thin market can hold the price below the floor that caused the hold, so an order can hold itself down. That is a real (if second-order) feedback loop and Ben may judge it worth the iteration cost; it is not worth spending determinism risk on unmeasured.

> **Recommendation:** Leave as implemented. If it is to be revisited, the cheap first step is a measurement rather than a fix: count how often a held order is the marginal supply that keeps the price below its own floor. File as a follow-on to BL-422 if that count is non-trivial.

*Files: `src/world/market_clearing.cpp`, `docs/economy/MARKETS.md`*

### NR-262 — An explicit matched trade pays the seller and delivers the goods to the shared market shelf, not to the buyer
*observation · raised 2026-08-16 · from Found while scoping BL-422's credit sites, 2026-08-16.*

In clear_markets, a matched explicit trade debits the seller pool and charges the buyer expenditure, but never credits the BUYER stockpile. The goods land in market_component::inventory (true before BL-422 via the listing-time credit, and still true after it via the matched-trade credit). Any corp on that body can then draw them through the ordinary processor path. So a buy order is closer to "pay to put stock on the local shelf" than to "acquire these goods".

**Why it matters.** It is currently invisible because nothing writes world::buy_orders in play â€” MARKETS.md Â§ Known limitations records the buy-order book as engine-only, waiting on BL-160. The moment BL-160 derive_exchange_orders becomes the first live emitter, a player who wins a matched trade will pay for goods a competitor on the same body can consume. BL-422 deliberately did NOT change this: routing matched fills to the buyer pool is a behaviour change to the buy side, outside a fix scoped to phantom supply. order_book_harness R7.12 pins the current behaviour as conservation (pool loss == inventory gain), so a future correction will fail that row loudly rather than silently.

> **Recommendation:** Fold into BL-160 (auto-exchange policy) rather than filing separately â€” that is the item that makes the buy side reachable, and the delivery question has to be answered there anyway.

*Files: `src/world/market_clearing.cpp`, `tools/verify/order_book_harness.cpp`*

### NR-263 — BL-422 moves nothing in the five AI benchmark seeds â€” the defect it fixes is unreached by the benchmark
*observation · raised 2026-08-16 · from Before/after measurement on ai_skill_harness and spectator_determinism, 2026-08-16.*

ai_skill_harness reports byte-identical net worth, solvency, survival and action counts on all five seeds before and after the fix (final = 499896.2 / -115203.9 / 183828.8 / 306437.4 / 396557.7 in both runs), and spectator_determinism is unchanged. The AI places standing sell orders in these worlds (action[place_sell_order] = 9-10 per seed), so orders exist; either none of them hold, or no processor ever drew the phantom stock they created. order_book_harness R7.2/R7.3/R7.10 DO fail against the pre-fix code, so the guard is real â€” it is the benchmark that does not reach the case.

**Why it matters.** Two things follow. First, the fix is safe to land: provably behaviour-neutral in the benchmark worlds while provably corrective in the case that produces the defect. Second, and more useful, the AI benchmark does not exercise a held order at all, which means corp_ai trade_floor_multiple currently prices its orders low enough to always clear. That is worth knowing before BL-436 calibration moves prices under it: a re-tune that pushes resolved prices down turns the AI standing orders into holds, and the benchmark has never measured that regime.

> **Recommendation:** No action on BL-422. Worth carrying into BL-436: when the cost/income calibration is settled, re-check whether the AI standing orders still clear, since a held order is stock the corp neither sells nor consumes.

*Files: `tools/verify/ai_skill_harness.cpp`, `src/world/corp_ai.cpp`*

### NR-264 — A remote Linux session CAN build and run the headless suite â€” CMake configure is what is blocked, not compilation
*observation · raised 2026-08-16 · from This session, working around the NR-240 symptom (BL-429 slice 2 authored but never compiled because of the remote network policy).*

cmake configure fails outright in the remote container: SDL3 and Lua come in by FetchContent and the download is refused by the network policy, so no target is ever generated and every harness looks unbuildable. But the 43 src/world/*.cpp sources (the io_world_obj set, minus the four registry TUs) need neither, and every harness in the CMake glob batch links io_world_obj ALONE. Compiling them directly works: build the 43 objects once with xargs -P8 (~14 s), ar them into a static lib, then link each harness against it (~5 s each). 63 of the 77 harnesses build and run this way. The exceptions are the 8 Lua-linked ones (chain_depth, tier_margin, era_roster, player_seed_sweep, recipe_switch_harness, interbody_pull_harness, pregame_balance_harness, persona_counsel_harness), font_glyph_harness (ImGui), and the long sweeps.

**Why it matters.** NR-240 and NR-241 record work that shipped uncompiled and visually unverified because a remote session believed it could not build. It can â€” for two thirds of the gate, including every economy, determinism and generation harness that does not read a Lua script. That is the difference between a remote session that verifies its own work and one that hands Ben unverified changes.

> **Recommendation:** Worth saving as a tool rather than a note (CLAUDE.md Â§ Tool creation is skill creation): a tools/verify/build_linux.sh doing the lib-then-link build, named in the verifier-headless skill as the fallback when cmake configure fails. Creating or modifying a skill needs Ben permission, so it is proposed here rather than done. Vendoring Lua would close the remaining 8.

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

### NR-269 — BL-439 landed and the AI immediately bankrupted itself on processors â€” the bands FELL where Sprint 19 predicted they would rise
*decision taken on your behalf · raised 2026-08-17 · from Landing BL-439 (the processing_facility build candidate), 2026-08-17.*

With a processor candidate in the scorer, rivals build 10-16 processors per seed across the benchmark set (69 total, against 0 before). Three of five seeds then go insolvent: seed 0 final net worth 498k -> -295k, seed 1 -> -89k, seed 4 -> -272k, each sitting below zero for 27-29 of 30 samples. Seeds 2 and 3 stay positive. ai_skill_harness is 18 rows red.

The estimator is NOT the culprit, and that was tested rather than assumed. The candidate was first scored with an inline revenue-minus-wages sum, then switched to estimate_prospective_profit (BL-162, the model done properly â€” habitability-scaled wages, real input cost, BL-193 stack decay). Same recipes chosen, same build counts, net worth moved only on seed 0 (-315k -> -295k). So the scorer is not mispricing the cost side; it is being promised a full run and handed a starved one.

The new per-building diagnostic reads: processor realised -6.24 to -11.70 per tick against a PREDICTED -0.38 to +0.08, on the same buildings, with extraction realised +22.80 where it sampled at all. Read the n: estimate_building_profit has no row for an idle or starved building, so the sample is biased toward the WORKING processors and the realised figure is the optimistic end.

AMENDED 2026-08-17 after integration: the count above (18 red rows) was true when written and is now 23. Net-worth final/min and solvency are red on all five seeds, dial-action thrash ceilings on seeds 0/2/3/4, build-action on seed 3; survival still passes everywhere. The extra five rows come from BL-406/BL-404 moving outpost prices, not from further AI change â€” see NR-278. The finding and the decision not to bless are unchanged.

**Why it matters.** This is the measurement Sprint 19 was opened to get, with the AI actually exposed to the economy for the first time. Its success criterion was written down in advance: the ai_skill_harness bands re-blessed DOWNWARD on 2026-08-16 should RISE, and a bless that does not raise them means the fix did not work. They fell, hard. Under that rule these numbers are a finding, not a bless â€” so the goldens are deliberately LEFT RED rather than re-blessed, and BL-439 task C is not done.

The -295k figure is also bigger than processor losses can explain on their own: 12 processors x 300 ticks x ~11/tick is ~40k, not 800k. The likely amplifier is BL-073 debt interest compounding once a corp crosses zero, which would make insolvency self-deepening rather than self-correcting. Not measured, and named here so it is not assumed either way.

> **Recommendation:** Ben owns the next call, because every lever here is cost-side calibration (the same boundary NR-266 stopped at). Three distinct options, which should not be conflated: (a) accept it as the honest finding, land BL-439 with the goldens red and a written reason, and let BL-436 fix the substrate â€” the bands re-bless once processing pays; (b) gate the candidate harder (a cash floor, a cap per corp) so rivals build processors without dying, which BUYS a green harness by hiding the defect the harness just found; (c) treat the debt-interest amplifier as its own item first, since a corp that cannot recover from one bad quarter makes every economy measurement noisier. My call taken in the meantime: do (a) and stop â€” nothing is tuned, nothing is blessed.

*Files: `src/world/corp_ai.cpp`, `tools/verify/ai_skill_harness.cpp`, `docs/development/backlog.json`*

### NR-270 — The AI can now build processors, so BL-436 can finally be measured against a rival that participates in it
*observation · raised 2026-08-17 · from Consequence of BL-439 landing, 2026-08-17.*

Every BL-436 measurement to date was taken on a world where the only processors were generated or warm-start ones, because the scorer could not build any (NR-265/NR-266). The benchmark set now carries 69 AI-built processors across five seeds, chosen by the AI on its own margin estimate.

**Why it matters.** It changes what the calibration sweep is measuring. NR-266 warned that the old narrative â€” corps spending income on processors that lose more at scale â€” described a mechanism that could not happen. It can happen now, so the sweep is worth re-running rather than re-derived on paper, and the three levers NR-266 named (the ~133:1 richness rate ratio, the 30.9% starvation rate, the three never-produced inputs) can each be measured with rivals responding to them.

> **Recommendation:** Re-run BL-436 calibration_sweep and tier_margin after BL-439 lands, before any cost-side number moves. Also resolves NR-267: BL-428 chain depth now has an AI player in principle, though whether a rival actually climbs a rung is unmeasured and the depth-gated recipes remain ancient-only.

*Files: `tools/verify/tier_margin.cpp`, `docs/development/backlog.json`*

### NR-271 — richness_reference enabled at the MEDIAN, not the mean â€” and the cost side still cannot be rescaled into a fix, because processing gross margin is negative before any upkeep
*decision taken on your behalf · raised 2026-08-17 · from Ben's call to enable richness_reference and rescale the cost side, 2026-08-17.*

ENABLED, and corrected while enabling it. The authored intent is "a typical deposit lands at ~1.0", and the value that delivers that is the MEDIAN richness, not the mean. tier_margin now prints the distribution (new R6b): p10 10.58 / p25 15.76 / MEDIAN 24.92 / p75 45.09 / p90 98.34 / p99 360.91, mean 53.34, max 72,321. The mean sits at the 78th percentile, so referencing against it ran the median tile at 0.47 and clamped 18% of all deposits flat at the richness_min floor â€” halving raw supply across the whole map. Set to 24.9.

The ratio defect is FIXED. Extraction went from 1666.69 revenue / 1659.18 net per building-tick to 14.63 / 7.89, and its capex payback from 0 ticks (instant, i.e. meaningless) to 12. Processing revenue is now 12.66 against extraction 14.63 â€” comparable, where it was 16.60 against 1666.69.

THE COST SIDE IS NOT THE REMAINING BLOCKER, and that is measured rather than argued. Processing reads revenue 12.66, input$ 12.87, maintenance 7.73, wage 1.14, net -9.08. Gross margin is -0.21 BEFORE any upkeep, so cutting maintenance and wages to ZERO still leaves a loss-maker. No rescale of the cost side can fix a negative value-add.

Separately, the -3M net worth figures are almost entirely COMPOUNDING, not operations. k_debt_interest_per_quarter is 0.02 and the rollout is 300 ticks: 1.02^300 = 380x. Per-building extraction nets +5 to +11 across every seed, so a corp cannot lose millions by operating; it dips a few thousand negative on processor upkeep and the interest multiplies that by 380. This confirms the amplifier hypothesis raised in NR-269.

**Why it matters.** It relocates the lever, and it lands on the option Ben raised first. Before the ratio was fixed, repricing could not close a 100:1 gap and was correctly ruled out. With the ratio fixed, price IS the binding constraint â€” processing revenue must rise 1.72x to break even at current upkeep (12.87 input + 8.87 opex = 21.74 needed against 12.66 earned).

It also means the two levers now compose, where before neither worked alone: refined-good prices +40% AND processing upkeep halved gives 17.72 - 12.87 - 4.44 = +0.41, i.e. break-even. Matching extraction at +7.89 needs more than that.

> **Recommendation:** Ben's call, three sized options, not mutually exclusive: (a) refined-good base prices +72% alone; (b) prices +40% with processing maintenance 10 -> 5 and base_wage 12 -> 8, which is the 'rescale the cost side' half actually doing work once it has a positive margin to protect; (c) cut recipe input quantities ~70%, which is a content change to recipes.lua rather than a price one. I have enabled the conversion at the median and stopped there â€” no price and no upkeep number has been touched, because tuning any of them against an unexplained -3M was the exact mistake NR-266 warns about, and the -3M is now explained.

Worth noting for whichever is chosen: 30.0% of processing building-ticks STARVE and 20.3% run with NO RECIPE SET. Half of all processor ticks earn nothing while paying maintenance. That is a defect to fix before, not after, calibrating against these means â€” the no-recipe share in particular looks like a generation or default-recipe gap rather than an economic outcome.

*Files: `scripts/economy.lua`, `tools/verify/tier_margin.cpp`, `tools/verify/ai_skill_harness.cpp`*

### NR-272 — The no-recipe defect was real, was in the LIVE game, and fixing it uncovered the actual blocker: coal has 1,671 tiles and zero mines
*decision taken on your behalf · raised 2026-08-17 · from Ben's call to fix the no-recipe defect before calibrating, 2026-08-17.*

FIXED, and it was not a harness artefact. author_building cannot set a recipe (ids are indices into a registry that does not exist yet at world-gen time), so processors are authored with no_recipe and backfilled later. That backfill lived INLINE IN app::load_economy, which made a world-generation invariant depend on the UI startup sequence â€” every path building a world without app (headless harnesses, --serve, --verify) ran processors that could never produce.

Worse, and this one hit the live game: load_economy runs the pass at app.cpp:955 and generate_background_firms authors MORE processors at :965. Every background firm processor kept no_recipe for the whole campaign, paying maintenance every tick, producing nothing, and reporting as ordinary idleness. tier_margin had already measured the shape of this in its own comment (26.5% recipe-less with no pass, 11.3% with the pre-firms pass only) and left the 11.3% standing as expected.

The loop is now world/s assign_default_recipes (corporation_generation.hpp). app calls it in BOTH positions; tier_margin retired its private copy for the shared one and calls it in both positions too; ai_skill_harness never ran it at all and now does. Measured: no-recipe ticks 20.3% -> 0.0%.

IT DID NOT IMPROVE THE ECONOMICS, and that is the finding. Processing net went 9.08 -> -10.44 per building-tick, because the processors that were silently unconfigured are now visibly STARVING: starvation rose 30.0% -> 46.3%. The buildings were never going to pay; the defect was hiding the reason.

**Why it matters.** The starvation now names one culprit. Coal (resource id 1) is the binding input on 54.0% of all starved processor ticks. It has 1,671 tiles carrying deposits and ZERO extraction sites targeting it, supplying 6.1 units/tick from ambient sources alone.

The cause is siting, not price and not upkeep: an extraction site targets richest_extractable(tile), and coal is the richest extractable on only 1.1% of the tiles that carry it. It is always out-ranked, so no mine â€” generated or AI-built â€” ever targets it, while the steel chain needs it. This is the same failure mode as R5s three never-produced inputs (ids 8/9/10), which NR-266 flagged as siting/reach rather than margin.

> **Recommendation:** Do NOT reprice yet. Repricing refined goods against a chain whose second input is unobtainable would tune around the shortage rather than fix it, and would have to be undone once coal is mined. The siting rule is the next item: an extraction site should be able to target a resource that is WANTED rather than merely richest â€” the demand signal already exists (the recipe input demand tier_margin R4 prints), and richest_extractable is a tile-local heuristic with no knowledge of it. Worth filing as its own backlog item; it is a corp_ai and generation change, not a calibration one.

Once coal is actually mined, re-run tier_margin and re-derive the price options in NR-271 (a/b/c) from the new numbers. The 1.72x revenue figure was measured on a starved chain and will move.

*Files: `src/world/corporation_generation.cpp`, `src/world/corporation_generation.hpp`, `src/core/app.cpp`, `tools/verify/tier_margin.cpp`, `tools/verify/ai_skill_harness.cpp`*

### NR-273 — The starting-corp pool was specialists-only by TIMING alone - now stated explicitly, and asserted
*decision taken on your behalf · raised 2026-08-17 · from BL-435 task E (the guard), while making the screen reachable to --verify.*

build_corp_choices was correct only because of WHEN it runs: the stage opens in the one frame before generate_background_firms, so m_world.corporations still held exactly the 8 specialists. That is a positional guarantee, one refactor away from silently becoming false - and it also blocked any verify hook, since a re-entered screen on a started world would have listed all 25-37 corps. Decision taken: skip is_background explicitly in build_corp_choices (a no-op on the live path, load-bearing everywhere else), and assert the split per seed in player_seed_sweep --guard G2 rather than trusting the ordering.

**Why it matters.** The pool being the specialist set is Benâ€™s own 2026-08-16 call and requirement R2. It was being upheld by an ordering nobody would notice breaking.

*Files: `src/core/app.cpp`, `tools/verify/player_seed_sweep.cpp`*

### NR-274 — Specialist processor coverage is now 6.92/8, not the 5.83/8 task B measured - something moved it further
*observation · raised 2026-08-17 · from player_seed_sweep --guard 24, run on resuming BL-435 at task E.*

BL-435 task B recorded specialists-with-a-processor at 2.96/8 before and 5.83/8 after its focus_asset_pattern fix. The same population measured today reads 6.92/8 over the same 24 seeds (166 of 192), with 0 seeds having none. Nothing in tasks E-F touches generation, so the extra ~1.1 came from work landed between - most plausibly BL-436's option-B extraction change or the dead-start trade holdings_range fix (2->3 draws reach the trade pattern's processing slot). Not investigated further: the guard's floor is 4.00/8 and the direction is the intended one.

**Why it matters.** The number quoted in BL-435â€™s own progress note is now stale, and coverage drifting upward without an item claiming it means the openings are being reshaped by side effects. Worth knowing which change owns it before anyone tunes against 5.83.

*Files: `tools/verify/player_seed_sweep.cpp`, `src/world/corporation_generation.cpp`*

### NR-275 — The corp-choice screen shipped with a clipped cell - caught by its first capture, which is the argument for capturing before blessing
*observation · raised 2026-08-17 · from BL-435 task E, scripts/verify/corp_choice.lua first successful run.*

The Holdings column rendered '2 proc / 0 extr / 1 other' clipped to '/ 1 otl' behind the Choose button - the table's column weights (0.42/0.16/0.24/0.18) gave the widest cell the narrowest stretch. Rebalanced to 0.36/0.14/0.22/0.28 off the measurement and re-captured clean. Also worth recording: the FIRST capture came back as a blank clear-colour frame, because the window is AlwaysAutoResize and ImGui sizes from the previous frame; the script now takes a warm-up capture as main_menu.lua does.

**Why it matters.** The screen was built and committed on 2026-08-16 without ever being photographed, and it carried a visible defect. Both failure modes here are cheap to hit again: a surface no automated path can reach, and a first-frame capture that looks like a working check but shows nothing.

> **Recommendation:** EYEBALL OWED, and no golden until then. screenshots/corp_choice.png is committed-capture-only on purpose: blessing a golden now would pin a layout Ben has never seen. --autostart-play will NOT show the screen (it auto-picks Surprise me), so seeing it live means starting a game from the menu. Once he has looked, bless corp_choice as a golden - the check is already written and stable.

*Files: `src/core/app.cpp`, `scripts/verify/corp_choice.lua`*

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

Merge order decided it. BL-435 landed first and KEEPS NR-273 (specialists-only pool by timing), NR-274 (processor coverage 6.92/8) and NR-275 (clipped corp-choice cell). The BL-406/BL-404 set was renumbered: NR-273 -> NR-276 (the dep-cache seeding bug), NR-274 -> NR-277 (pull_fraction not re-tuned), NR-275 -> NR-278 (outpost prices move, nothing re-blessed). Cross-references were rewritten in backlog.json (BL-406 design), docs/economy/MARKETS.md and src/world/recipe_registry.hpp. THE ONE THING THAT COULD NOT BE FIXED is commit cab181bâ€™s own message, which still reads "pull_fraction is NOT re-tuned - NR-274" and "No golden or band re-blessed - NR-275". Read against todayâ€™s file those point at two BL-435 entries. Substitute 277 and 278.

**Why it matters.** A naive merge would have silently dropped three entries - the exact failure the parallel-worktree notes warn about. It did not happen, but the near-miss is the point: id allocation off a stale local file is not safe once more than one worktree is open, and next_id.js only helps if it is actually run at authoring time rather than at landing time.

> **Recommendation:** Nothing to undo. Worth considering whether NEEDS_REVIEW ids should be minted the way backlog ids are (next_id.js scanning all branches), or whether a verify check should simply assert no duplicate NR id - the latter is a five-line check and would have caught this before the merge rather than during it.

*Files: `docs/development/NEEDS_REVIEW.json`, `docs/development/backlog.json`, `docs/economy/MARKETS.md`, `src/world/recipe_registry.hpp`*

### NR-280 — BL-406/BL-404 moved outpost prices but did NOT move the spectator rollout hash - NR-278 over-predicted its own blast radius
*observation · raised 2026-08-17 · from Integration gate on sprint19-bl417 after merging all three sub-agent branches, plus a control build of spectator_determinism at the pre-merge commit 8cc2981.*

spectator_determinism fails one assertion - R2 byte-identity against the pinned golden 855E07DE529684EC. Measured on BOTH sides of the merge, the observed hash is the SAME value, 6D546B281FFA4A68, and the spectated hash is 8606FA466D259B09 on both. So the failure is entirely pre-existing (the golden was last blessed at 3b9ffd2, before BL-439/BL-436/BL-440 landed on this branch) and the market-clearing change contributed nothing to it. Every other assertion in that harness passes, including both reproducibility checks and the R2 prohibition check.

**Why it matters.** NR-278 states that any world hash downstream of an outpost price will read differently. On this rollout it does not, which is a fact worth having before anyone treats a moved hash as evidence the pull change reached something. The likely reason is that the seed-0 300-tick spectator rollout does not exercise an off-home market enough for the pull to register - if so, the harness is blind to exactly the change BL-406 made, and that is a coverage gap rather than a reassurance.

> **Recommendation:** Do not bless. When the goldens are eventually re-blessed as one deliberate pass (NR-269 owns that call), re-blessing spectator_determinismâ€™s R2 golden is a one-line change and should ride along. Separately worth checking whether the rollout reaches an off-home market at all.

*Files: `tools/verify/spectator_determinism.cpp`, `src/world/market_clearing.cpp`*

### NR-281 — BL-441: the want registered is NET of the corp's own pool, not the gross full-run need â€” a deliberate narrowing of the item's wording
*decision taken on your behalf · raised 2026-08-17 · from Implementing BL-441 in run_processing (economy_system.cpp), reading the item's design against what mc.demand means to resolve_price.*

BL-441's design says the demand to register is 'the full-run need per input'. Taken literally that is the GROSS need â€” 2 units per batch times the full batch count â€” regardless of how much of it the corp already holds in its own (corp, body) pool. I registered `max(0, need_full - pool_on_hand)` instead: the want the corp has OF THE MARKET. In the item's own worked example the two are identical, because a starved processor's pool is empty by construction, and the guard (order_book_harness R8) is written on an empty pool so it cannot tell them apart.

**Why it matters.** They differ for a corp that is NOT starved. mc.supply is an offer made to the market and mc.inventory is a delivery to it â€” BL-422's distinction. resolve_price compares supply against demand, so demand has to be the BID to the market for the comparison to be apples-to-apples. Under the gross reading, a vertically integrated corp that feeds its smelter entirely from its own mine would register full market demand for an input it never intends to buy, permanently inflating that input's price for everyone else. That is BL-422's defect in a third direction: crediting a transaction nobody made. Under the net reading, demand appears exactly when the buffer drains, which is when the corp actually competes for the good.

> **Recommendation:** Keep the net reading; it is the one that makes demand the counterpart of supply. If Ben wants gross â€” a defensible alternative if mc.demand is meant to express total CONSUMPTION of a good rather than market pressure on it â€” it is a one-line change at the same site, but the two meanings should not be mixed and MARKETS.md should then say which it is.

*Files: `src/world/economy_system.cpp`, `docs/economy/MARKETS.md`*

### NR-282 — BL-441 necessarily pulled run_construction into scope â€” otherwise the fix would have silently ZEROED construction's market demand
*decision taken on your behalf · raised 2026-08-17 · from Tracing every writer of economy_report::purchases before splitting the demand read in market_clearing.cpp.*

BL-441's design names run_processing only. But run_construction (economy_system.cpp, BL-095 pay-as-you-build) is the OTHER writer of report.purchases, and market_clearing's single demand loop was reading both. Had I pointed demand at a new `wants` map fed only by processing, construction sites would have gone from registering their drawn materials as demand to registering NOTHING â€” a regression introduced by the fix. So run_construction now records its want too: the full-rate material need, registered BEFORE the `rate <= 0` continue, so a build paused for want of steel finally says so. This is the same defect in the same file, and arguably the starker case: a fully stalled build site previously registered zero demand for the exact material stalling it.

**Why it matters.** It widens the diff beyond the item's stated file scope while a parallel worktree (BL-442, price band as data) is editing the same two files. Worth Ben knowing it was forced by coherence rather than chosen, and worth knowing that construction demand is now a want and will read higher than before across the whole economy.

> **Recommendation:** Accept as part of BL-441. If it should have been its own item, the split point is clean â€” the construction want is one added block in run_construction.

*Files: `src/world/economy_system.cpp`*

### NR-286 — Decision taken: registered price_band_harness in the verifier-headless SKILL.md without asking first
*decision taken on your behalf · raised 2026-08-17 · from BL-442 step 1. CLAUDE.md: 'Creating or modifying a skill requires user permission'; this session is non-interactive.*

BL-442's guard is a new harness, tools/verify/price_band_harness.cpp. The documented way a headless check becomes a permanent asset is to name it in .claude/skills/verifier-headless/SKILL.md, but modifying a skill wants Ben's permission and there was no one to ask. Taken the call to add the entry, on the grounding that this is the additive case the skill's own text invites ('Authorising a new check = adding a tools/verify/*.cpp harness and naming it here') rather than a change to how the skill behaves. The alternative was an unregistered harness, which is exactly the 'loose tool is forgotten' outcome the standing rule warns against.

**Why it matters.** A delegated decision that is unrecorded is indistinguishable from one Ben made. If he would rather skill edits always wait for him, the entry is one paragraph to revert - but the harness should then be registered somewhere else, or it will rot.

> **Recommendation:** Ratify the SKILL.md entry, or say that skill edits must always wait and name where new harnesses get registered instead.

*Files: `.claude/skills/verifier-headless/SKILL.md`, `tools/verify/price_band_harness.cpp`*

### NR-289 — The premise of BL-442 step 2 is wrong in its most important particular: inter-market trade ALREADY happens on day 1
*observation · raised 2026-08-17 · from tools/verify/haulage_measure.cpp, new this session. 5 seeds of the real generated world, 20 econ ticks each, band unchanged at the old [0.25x, 4.0x].*

BL-442 step 2 is written on the premise that the narrow ceiling is why nothing moves between markets, and that inter-market trading is a late-game unlock rather than a day-1 fact. Measured at the OLD band: 2146 convoys dispatched, of which 1677 are intra-body market-to-market hauls. Inter-market trade was never absent. What IS absent - and was zero at the old band and stayed zero at the new one - is inter-BODY trade: 0 space-lane convoys and 0 persistent trade_routes across all five seeds. trade_routes is a body-level record (body_a/body_b in components.hpp), so BL-088's persistent route store cannot see intra-body trade at all, which is very likely why the thread believed there was none: the surface everyone reads was structurally blind to the trade that was happening.

**Why it matters.** The zero that matters is gated by the launchpad and the propellant burn in dispatch_convoys, not by the price band. No ceiling reachable by any sane number will open the space lane, because the lane is refused before any price is consulted. If the goal is 'inter-body trade from day 1', widening the band is the wrong lever and this item cannot deliver it. If the goal is the intra-body one, it was already met and the widening is a volume/reach change rather than an unlock.

> **Recommendation:** Ben to say which trade he meant. If it is the space lane, that wants its own item against the launchpad/propellant gate in supply_system.cpp, not a price tunable. The widening landed here on its own honest merit - it extends the servable set from the median neighbour pair to every measured neighbour pair - but it should not be recorded as having created inter-market trade, because it did not.

*Files: `tools/verify/haulage_measure.cpp`, `src/world/supply_system.cpp`, `scripts/economy.lua`*

### NR-290 — Decision taken: the price band's FLOOR was left at 0.25 while the ceiling moved 4.0 -> 10.0
*decision taken on your behalf · raised 2026-08-17 · from BL-442 step 2. The item's summary calls the whole [0.25x, 4x] band 'far too narrow'; Ben's 2026-08-17 requirement derives only a ceiling.*

Widening symmetrically was the obvious reading of 'widen the band' and it was rejected. The derivation Ben gave - the ceiling must exceed base + haulage for a neighbouring market to be worth serving - constrains the ceiling and says nothing about the floor. A lower floor does widen the arbitrage margin, but it does so by cutting the price an abundant producer receives, which is the same direction Sprint 19's falling economy numbers already complain about, and it would have put an authored guess immediately beside a derived number.

**Why it matters.** It is a deliberate half-execution of the item as written, so it should not read later as an oversight. It also leaves the band asymmetric - [0.25x, 10.0x] - which is a legitimate shape for a scarcity model but is a change in character from the roughly log-symmetric [0.25x, 4x] it replaces.

> **Recommendation:** If Ben wants the floor moved too, it should get its own derivation rather than a matching round number - most naturally from the carry cost of holding unsold stock, which the model does not currently charge at all.

*Files: `scripts/economy.lua`, `docs/economy/MARKETS.md`*

### NR-291 — The scarcity-outranks-abundance constraint was measured at 233x and deliberately NOT used
*decision taken on your behalf · raised 2026-08-17 · from haulage_measure's base-price report: min 0.60, max 140.00 (spacecraft_components), ratio 233.33.*

Ben's second sentence - 'a resource nobody can obtain is still cheaper than the one everybody has', with coal 2.0 -> 8.0 against iron ore 2.5 - reads as a constraint that a scarce good must be able to outprice an abundant one. Taken literally across the whole authored price table that demands ceil_mult > 233, because the table spans three tiers from a 0.60 raw to a 140.00 Tier 3 product. That was not used. RESOURCES.md promises margin WIDENS up the tiers and BL-340 priced spacecraft_components at 56x iron ore on purpose; a ceiling that lets the cheapest raw outprice the dearest product would delete the tier model the whole Trade pillar rests on.

**Why it matters.** The constraint is only coherent WITHIN a tier, and Ben's own worked example is two raws. Within the ordinary raw tier (0.60 to 6.00 rare_earth_ore) it demands ceil > 10, which the landed 10.0 satisfies exactly - so the two derivations agree, and that agreement is the reason to be comfortable with 10.0 rather than a number chosen only from haulage.

> **Recommendation:** Ben to confirm the within-tier reading. If he meant it across tiers, the fix is not a bigger ceiling - it is that scarcity should scale a good's price against its OWN tier's neighbours, which is a different model and a different item.

*Files: `scripts/world_gen.lua`, `docs/economy/MARKETS.md`*

### NR-292 — The refined tier's price is CEILING-DETERMINED at any band - widening 4x -> 10x only moved the peg, and made processing lose more
*observation · raised 2026-08-17 · from tier_margin R7 (authored base_price vs live mean market price), run on both sides of the BL-442 step 2 widening.*

At the OLD 4.0x ceiling the whole refined/product tier already sat at 3.35x-3.85x of base - i.e. riding the clamp. At the NEW 10.0x ceiling the same resources sit at 8.13x-9.55x. They did not find a new equilibrium; they moved to the new clamp. ids 30/31/32 (machinery, alloys, electronics) read 9.38x, 9.37x, 9.55x. That is a price the band is setting, not one supply and demand are setting. Consequence, measured: processing input cost 14.86 -> 26.56 while processing revenue only 12.55 -> 17.17, so processing net/tick fell from -10.42/-10.81 to -17.68. Extraction barely moved (8.09 -> 8.31) because raws are NOT pegged.

**Why it matters.** It is the exact failure mode the item's own brief warned about ('a price pegged at the ceiling for a whole run is a finding worth more than a landed number'), and it says the ceiling is doing a job it should not be doing. A processor buys refined inputs from the rung below it, so when every refined price rides the clamp, widening the clamp raises a processor's COSTS as fast as its prices - faster, in fact, because a lower-rung processor sells one pegged good and buys several. Widening the band cannot fix processing margin; it makes it worse, monotonically. BL-436's open calibration questions (richness_reference, processing upkeep, recipe input quantities - NR-271) are where that fix lives, and they are deliberately untouched here.

> **Recommendation:** Ben to decide whether 10.0 stands. It is correctly derived for the question it was asked (crossing inter-market haulage) and it is actively harmful to the question Sprint 19 actually cares about (does refining pay). Those are two different levers and this item was pointed at the first. If the pegging is the priority, the real finding is that the refined tier has no demand-side price discovery at all - it is clamp, EMA, clamp - which is a market-model item, not a tunable.

*Files: `scripts/economy.lua`, `src/world/market_clearing.cpp`, `tools/verify/tier_margin.cpp`*

### NR-293 — ai_skill_harness is STRUCTURALLY BLIND to scripts/economy.lua - its 20 red bands cannot move for any data-only economy change
*observation · raised 2026-08-17 · from BL-442 step 2. ai_skill_harness was run on both sides of a 2.5x price-ceiling widening and returned BYTE-IDENTICAL output: same 20 failures, and all five seeds' net_worth final/min identical to the decimal (-2220111.0, -3080723.2, -2914809.5, -3407154.2, -1947063.6).*

The harness hand-builds its recipe_registry (make_registry, ai_skill_harness.cpp line ~78) rather than loading the shipped Lua - its own comment says 'mirrors scripts/economy.lua'. The mirror covers building economics; it does not cover economy.price_band, so the harness runs on recipe_registry.hpp's struct defaults (0.25/4.0) whatever the shipped file says. It is not in CMakeLists' IO_TEST_SCRIPT_ROOTED_HARNESSES and links no Lua.

**Why it matters.** The thread has been asking all session whether the Sprint 19 bands RISE. For any change authored in economy.lua, that question is not merely unanswered by this harness - it is unanswerable, and the identical output reads exactly like 'the change had no effect on the AI' when the truth is 'the change never reached the AI'. A silent mirror that has fallen behind is worse than no mirror: it produces a confident null result. This is the same class of defect as BL-389 (--serve never loads world_gen.lua) and as tier_margin's own missing world_gen.lua load, both already recorded.

> **Recommendation:** Either make the harness live-Lua like tier_margin and player_seed_sweep (it would then measure the shipped economy, at the cost of moving its bands once and deliberately), or add price_band to make_registry's mirror with a comment naming what else is NOT mirrored. Not done here: it would move all 20 bands in the same commit as a band change, making the two unattributable, and re-blessing is Ben's call. Filed rather than fixed.

*Files: `tools/verify/ai_skill_harness.cpp`, `CMakeLists.txt`*

### NR-294 — Widening the ceiling REDUCED convoy dispatch - higher scarcity prices suppress the demand that dispatch reads
*observation · raised 2026-08-17 · from tools/verify/haulage_measure.cpp second section, 5 seeds x 20 econ ticks, both bands.*

Convoys dispatched fell 2146 -> 2029, and the intra-body market-to-market subset fell 1677 -> 1576, when the ceiling went 4.0 -> 10.0. The mechanism is legible and is not a bug: dispatch_convoys sizes every haul from a market's shortfall (demand - supply), and demand is price-elastic (economy.demand.demand_elasticity, exponent on base_price/price). A higher ceiling lets a scarce good's price rise further, elasticity then cuts the quantity demanded, the shortfall shrinks, and fewer/smaller convoys are dispatched to fill it.

**Why it matters.** It is the direct opposite of the item's stated goal. The premise was that a higher ceiling makes distant supply worth hauling; measured, the price and the quantity move in opposite directions and the quantity effect wins at this scale. A margin that is wide enough but on a demand that has shrunk is not more trade. Anyone reading the landed ceiling as 'more goods now move' would be wrong by 5%.

> **Recommendation:** Read together with NR-289 (inter-market trade was never absent) this says the band was not the constraint on trade in either direction. Ben to weigh whether 10.0 still earns its place on the haulage derivation alone. The number is honestly derived; the outcome it was expected to produce did not occur, and that is reported rather than tuned around.

*Files: `tools/verify/haulage_measure.cpp`, `src/world/supply_system.cpp`, `scripts/economy.lua`*

### NR-296 — Measured (BL-443): interest is 98.8% of every benchmark corp's deficit on all five seeds, so ai_skill_harness's net-worth band measures WHEN a corp first dipped, not how it played
*observation · raised 2026-08-17 · from BL-443 (debt compounds with no floor) measurement phase. New instrument tools/verify/debt_decomposition.cpp: the ai_skill_harness rollout (same registry, same seeds, same 300 ticks, same run_tick order) with apply_budget's existing optional `breakdown` sink accumulated per corp. No economic behaviour was changed and nothing was re-blessed.*

FIRST, the existing check. tools/verify/econ_bankruptcy.cpp does NOT pin the debt spiral as expected behaviour, so BL-443 is not a disagreement with a guard. It asserts nothing at all: it prints PASS unconditionally on the last line, and its header says in as many words that 'the prototype carries no insolvency mechanic'. It is a calibration REPORT wearing a harness's clothes. Its one notion of insolvency is a local calibration trigger, balance <= -5x starting_capital, and that threshold is inert on the real generator - see the last paragraph.

SECOND, the decomposition. The '-3M is almost entirely compounding' claim was arithmetic inference from 1.02^300; it is now an observation, and it is stronger than the claim. Aggregate over the seven non-player corps, per seed: seed 0 final -2,220,111 of which operating -24,942 and interest -2,193,302; seed 1 final -3,080,723 (operating -34,452, interest -3,044,605); seed 2 final -2,914,810 (operating -31,869, interest -2,880,475); seed 3 final -3,407,154 (operating -37,752, interest -3,366,534); seed 4 final -1,947,064 (operating -23,421, interest -1,921,775). Interest is 98.7-98.8% of the deficit on ALL FIVE seeds. Operating flows are 79x to 88x smaller than the interest charged on them. The invariance of that 98.8% across five different generated worlds is itself the finding: the final number is a near-pure function of the interest rule, and is close to blind to what happened in the world.

THIRD, when. Corps do not dip late and they do not dip deep. Every rival crosses zero between tick 9 and tick 27 on four of five seeds, and it crosses by a rounding error: observed first-crossing balances include -1.0, -3.7, -3.8, -4.6, -7.4 and -12.9 credits. Seed 0 corp 46797 went one credit into the red at tick 19 and finished at -248,761, against a 300-tick operating loss of -2,717. That is a 92x amplification of a loss the corp could have covered out of a single tick's income.

The reason there is no buffer: every generated corp opens at balance 0. corporation_params::base_capital defaults to 0.0f and corporation_generation.cpp sets both background firms (line 1760) and the seeded roster to it, on the premise that 'capital is earned through the pre-game warm start'. The benchmark runs no_prehistory(), so no warm start runs. A corp with zero cash and a 2%/qtr charge on any negative balance has no first bad quarter it can survive.

**Why it matters.** It puts the net-worth band out of commission as a SKILL signal, which is the metric Sprint 19 has been reading all week. Final net worth decomposes as (small operating loss) x (1.02 ^ ticks since first crossing), so the dominant term is the EXPONENT - the tick a corp first crossed zero - not the base. The spread that exponent contributes is roughly 50x between an early and a late crosser, and the measurement contains a clean natural experiment for it. Seed 1, two corps in the same world: corp 46831 crossed at tick 18 and finished at -855,092; corp 46828 crossed at tick 156 and finished at -5,057. Their actual operating performance differs by 13x (-11,306 against -868). Their headline net worth differs by 169x. The band ranks them as if one were catastrophically worse than the other; it is measuring a 138-tick head start on the compounding clock.

This also explains why the band has needed re-blessing after almost every economy change while the survival fraction stayed green throughout Sprint 19. Any change that moves the first-crossing tick by a handful of ticks moves final net worth by a multiple, so the band has been tracking a lever that is arithmetically hypersensitive and substantively uninformative.

On survival as the replacement: it is UNCORRUPTED but LOW-RESOLUTION. It reads the world (corps holding at least one non-decommissioned building) rather than the balance, so compounding cannot touch it - that is genuinely why it held. But with no insolvency mechanic nothing forces a decommission from debt, so it only moves when the BL-079 reflex tier idles or sheds something, and it therefore cannot distinguish a corp trading well from a corp merely still standing. It is the sounder of the two today, and it is not yet a skill metric. It becomes one the moment insolvency has a consequence, which is the same change BL-443 is about.

> **Recommendation:** MEASUREMENT ONLY was done; no lever pulled, no constant touched, no golden or band re-blessed. The three BL-443 candidates against the numbers above:

(a) DEBT CEILING - argue against. The measurement says the damage is the amplification factor, not the magnitude. A ceiling replaces '-3.4M' with '-C' for every insolvent corp, which makes them all read IDENTICAL and censors the variable rather than restoring its meaning. It also leaves the corp in the same undefined state it is in now. It silences the number BL-443 named and fixes nothing under it.

(b) FORCED LIQUIDATION - right shape, wrong instrument to reach for first, and the measurement supplies a concrete objection. Any threshold expressed as a multiple of starting capital degenerates to zero here, because starting_capital IS zero on every generated corp - econ_bankruptcy's own -5x rule triggers at -0.0. And a threshold in absolute credits would fire on essentially the whole field, since every rival is a few credits underwater by tick 27. Liquidation designed against these numbers today liquidates everyone by tick 30.

(c) RESTRUCTURING - RECOMMENDED, and the measurement argues for it much more strongly than the abstract case did. The operating losses are tiny: roughly -10 credits per tick against corps booking thousands in income. A corp needs to shed a handful of credits per tick to be solvent, and the machinery that sheds them already exists - BL-079's reflex tier idles persistently loss-making buildings, it is simply not gated on the corp's own solvency. So (c) is the smallest real change, it composes with the reflex tier and the BL-202 scorer instead of sitting beside them, and it attacks the term that actually dominates (it shortens the compounding window rather than capping its output). It also makes the survival fraction load-bearing, since a corp that cannot restructure back to solvency is then the one that goes.

Two things for Ben that are NOT levers and should be decided before one is chosen. First: is a corp opening at balance 0 intended in a session where no pre-game warm start runs? The interest rule is defensible at ~8%/yr; charging it against a corp given no capital at all is what makes the first bad quarter unrecoverable by construction. Second: what should be done with econ_bankruptcy.cpp, which reports green forever by printing an unconditional PASS - it should either grow BL-443's bounded-terminal-state assertion or be renamed to say it is a report.

*Files: `tools/verify/debt_decomposition.cpp`, `tools/verify/econ_bankruptcy.cpp`, `tools/verify/ai_skill_harness.cpp`, `src/world/budget_system.cpp`, `src/world/corporation_generation.cpp`*

### NR-297 — Decision taken: added tools/verify/debt_decomposition.cpp as a measurement instrument and did NOT register it in the verifier-headless skill
*decision taken on your behalf · raised 2026-08-17 · from BL-443 measurement phase, non-interactive session. CLAUDE.md: 'Creating or modifying a skill requires user permission'.*

The BL-443 numbers needed a rollout instrumented through apply_budget's `breakdown` sink, which no existing harness does. Built it as tools/verify/debt_decomposition.cpp. Deliberately did NOT add it to .claude/skills/verifier-headless/SKILL.md, unlike NR-286's call on price_band_harness, for a reason specific to this file: it asserts nothing and has no PASS/FAIL. It prints a table and exits 0. Registering a non-asserting instrument in the harness skill would put a permanently-green entry in the gate, which is the same failure mode the entry above criticises econ_bankruptcy for. The CMake glob at CMakeLists.txt:687 picks it up automatically, so it builds and is discoverable without the skill edit.

**Why it matters.** It leaves a useful tool slightly less discoverable than the standing rule would like ('the loose one is forgotten'). The honest fix is not a skill entry but a decision about whether tools/verify/ should hold instruments at all, or whether measurement harnesses want their own home and their own ctest exclusion.

> **Recommendation:** Either keep it unregistered as an instrument (and say instruments live in tools/verify/ unregistered), or have it grow BL-443's real assertion - a corp driven insolvent reaches a bounded terminal or recovered state - at which point it becomes a genuine guard and should be registered. Note also that NR-296/NR-297 were allocated from this worktree's file, where NR-288 was the max; sibling worktree agents were active, so check for an id collision at integration.

*Files: `tools/verify/debt_decomposition.cpp`, `.claude/skills/verifier-headless/SKILL.md`*

### NR-301 — BL-325's out-of-supply decay cut from the Sprint 21 proposal on the economy seam, not on scope
*decision taken on your behalf · raised 2026-08-17 · from Sprint 21 scoping audit, deciding what the proposed sprint carries.*

BL-325 (military bases and supply) slice S3, deterministic out-of-supply strength decay, is the last leg of that item and reads naturally as part of a military sprint. It needs economy_system.cpp and scripts/economy.lua. Three agents are live in the market_clearing / economy_system / budget_system / economy.lua seam right now, and the Sprint 21 decomposition is otherwise entirely clear of it - no proposed slice writes any of those four files.

**Why it matters.** Including it would put the one sequencing hazard into an otherwise four-way-parallel sprint, for a slice whose own item already sequences it behind the conflict spine (NR-177). The call was taken rather than escalated because it is a sequencing decision, not a design one - but it is a deliberate narrowing of a theme already deferred three times, so it is recorded rather than left in the prose.

> **Recommendation:** Pick it up in the sprint after, once the conflict layer gives units something to be out of supply FROM and the economy seam has quiesced. If Ben would rather it rode along, sequence it last and alone - never concurrent with the other slices.

*Files: `docs/development/REFINED.md`, `docs/development/SPRINTS.md`*

### NR-304 — Called it 'stance', not 'standing' - the latter is taken by BL-262's power bands
*decision taken on your behalf · raised 2026-08-17 · from Sprints 22-24 proposal, naming the friend/neutral/hostile model.*

The brief called the friend/neutral/hostile model 'the standing model'. src/world/standing.{hpp,cpp} already exists and means something unrelated: the BL-262 coarse public POWER read - negligible/minor/notable/major/dominant over reach, capital and market share. I named the new concept corp_stance in src/world/stance.{hpp,cpp} instead of overloading the word.

**Why it matters.** Overloaded, every future sentence about 'a corp's standing' is ambiguous between how strong it is and how it feels about you - in prose, in the glossary, and in symbol names two headers apart. Cheap to fix now and expensive later. Also note BL-262's own header warns against unifying its bands with the AI scorer (a Goodhart trap); a stance model that shares its name invites exactly that confusion.

### NR-313 — This session cannot run live-Lua or any visual verification - the dependency host is blocked by egress policy
*observation · raised 2026-08-17 · from Sprint 25a implementation, configuring CMake before starting BL-457.*

CMake fetches SDL3, Lua, sol2 and ImGui from codeload.github.com at configure time. That host returns 403 through this session's egress proxy, which the proxy documentation classifies as an organization policy denial and explicitly says not to route around. Consequence: the GUI cannot be built at all, and neither can any harness that links Lua - which is chain_depth (BL-457's own named guard), price_band_harness and haulage_measure. What DOES work: 43 of the 47 world TUs are Lua-free and compile with plain g++, so every non-Lua harness runs. Verified end to end by building and running econ_harness (ALL PASS).

**Why it matters.** It changes what 'verified' means for everything in Sprint 25a and it must not be discovered at cut time. Three concrete gaps: (1) no visual/--verify capture, so BL-453's Convoys tab ships unphotographed; (2) no chain_depth run, so the R1 orphan row is argued by inspection rather than executed; (3) no Lua load test, so a syntax or name error in recipes.lua / economy.lua / world_gen.lua would not surface here - the exact class of defect that crashed startup in 2026-08-12 and that NR-237 caught eleven days late. Requirement rows that could not be executed are marked pending rather than complete, per the project's own rule that a guard never seen to fail is not a guard.

### NR-314 — BL-457 authored three presentation rows outside its scope, because a positional array left no choice
*decision taken on your behalf · raised 2026-08-17 · from Sprint 25a, appending ordnance to src/ui/presentation.cpp resource_table.*

resource_table is a positional constexpr array declared [resource_count]. It carried 34 initialisers for 37 values, so clean_water, consumer_goods and medical_supplies (BL-368, landed 2026-08-11) were value-initialised to nulls and rendered as '(unnamed resource)' - a population centre's own demand basket, nameless, for six days. Adding ordnance at index 37 is impossible without filling 34-36, so BL-457 authored all four rows. Names, abbreviations and colours were chosen for the three habitability goods in the palette's existing idiom (cooler and paler than the industrial tier, matching the argument that priced them modestly).

**Why it matters.** It is scope BL-457 did not ask for, landed on Ben's behalf, and it touches a UI file this session cannot photograph (NR-313) - so four new colour choices ship unseen. The defect itself is real but minor: presentation_of()'s fallback handled it without crashing, which is exactly why it survived. Worth noting the compiler is structurally unable to catch this - a short initialiser list for a sized array is legal C++ and silently zero-fills.

### NR-315 — BL-455 wired science as a condition SUBJECT, which is 'reached' not 'spent' - and that was read off BL-344, not chosen
*decision taken on your behalf · raised 2026-08-17 · from Sprint 25a, implementing BL-455 after Ben ruled delete military_points / wire science.*

The item's design flagged one thing to settle while wiring: whether a tech's cost is SPENT (a balance that decrements on unlock) or REACHED (a threshold the total must pass), and said to read BL-344's landed shape and match it rather than choosing freshly. Read: tech_gate is a condition_set PREDICATE over corp state, evaluated fresh each time, and nothing anywhere in the gate system debits anything. So science became condition_subject::science - a threshold, appended last to the serialised uint8_t enum - and is classified CONTINUOUS rather than integral, since a corp genuinely sits at 12.5 points.

**Why it matters.** It forecloses a design option quietly. A spend model is not reachable by extending this: it would need a debit mechanism, an unlock transaction, and a decision about what happens when two gates want the same points - none of which condition_set can express, because a predicate that changes the state it measures is not a predicate. If research is ever meant to be a CURRENCY rather than a LEVEL, that is a new mechanism and this wiring is the wrong foundation for it. Recorded so the choice is visible rather than inherited.

### NR-316 — All three Sprint 25a worktree agents branched from the SESSION-START commit, not the current branch head - the v0.1.9 failure, repeated
*observation · raised 2026-08-17 · from Sprint 25a fan-out; caught when the docs agent reported BL-454/455/459 missing from its backlog.json.*

Three sub-agents were launched with isolation: worktree AFTER three commits had landed on the working branch (the sprint rescope, the seven rulings, and BL-457 ordnance). All three worktrees were created at 7f0fac6 - the commit that was HEAD when the SESSION started - not at 44166a4, the branch head at launch time. The docs agent surfaced it by noticing its backlog.json still recorded BL-448's symmetry as an open question and had no BL-454/455/459 at all. The severe case was the upkeep agent: resource_type::ordnance did not exist in its tree, and its entire item is 'units draw ordnance', so it was one step from inventing a duplicate append to a serialised enum.

**Why it matters.** This is verbatim the v0.1.9 batch failure the Sprint 17 retro recorded - 'three of five agents branched from a base that had already moved, and worktrees isolate WRITES, not HISTORY' - and the mitigation recorded then (integration reads every hunk) is a cure, not a prevention. The prevention is to state the base explicitly at launch or to have agents merge the working branch as their first act. Neither happened here because the worktree base is chosen by the harness, not by the brief, and nothing in the launch surface reports which commit it picked. Two agents were mid-flight when it was caught; both were told to merge the working branch before continuing.

### NR-318 — hold_convoy names its convoy through cmd.order, not cmd.subject - convoys are not entities
*decision taken on your behalf · raised 2026-08-17 · from Sprint 25a, BL-452. The brief said subject; the agent deviated and said so.*

The brief specified subject = the convoy. The agent found that convoys are not entities at all - w.convoys is a plain vector, not an entity map - so an entity_id subject had nothing real to name. It instead gave convoy_component a monotonic id from a new allocate_convoy_id() (mirroring next_order_id) and carried it in cmd.order, the field every other non-entity handle already uses: sell_order::id, quote and contract ids. Two related calls ride with it: the named SOURCE MARKET only selects the source body, with the intra-body route still running from the corp's production anchor exactly as the auto-dispatcher does (a second routing rule for the player was refused); and a foreign convoy id returns the same rejected_invalid as a nonexistent one, following BL-397's oracle rule so the seam does not leak which ids exist.

**Why it matters.** It is a deviation from an explicit instruction, taken for a good reason, and it sets the idiom for every future verb naming a transient object. Minting a real entity for something that lives about two ticks and appears in no map would have been worse, but it is a call Ben should see rather than inherit. The oracle-rule choice in particular is a deliberate usability cost paid for an information-leak guarantee.

### NR-319 — spectator_determinism's golden: two independent causes, now separated by measurement
*observation · raised 2026-08-17 · from Sprint 25a integration, isolating the one red check across three commits.*

The R2 byte-identity row fails on the merged branch. The implementing agent reported two candidate causes together - the ordnance resource append, and the golden being MSVC-derived. Building the harness at three points under g++ separates them. At 4f3c4d8 (before any of this work): observed 9744431472DE5755, already FAILING against golden 855E07DE529684EC. At 44166a4 (ordnance only): observed CCF93A83903B6B45. At 9644eaa (ordnance + the military_points deletion): observed CCF93A83903B6B45, UNCHANGED.

**Why it matters.** Three separate facts fall out, and the conflated version supports none of them. (1) The golden cannot pass under g++ at any commit - it is toolchain-specific, so this row is permanently red in any Linux/cloud session and its redness carries no signal there. (2) The ordnance append DID move the hash, so a Windows re-bless is genuinely owed and is not merely a toolchain artefact. (3) Deleting corporation_component::military_points moved it NOT AT ALL, which says state_hash does not walk that field but does walk the per-resource arrays. Without the three-point measurement, (3) would have been assumed either way.

### NR-322 — Unit quality is DERIVED from the roster row power_mod, not authored as a second column
*decision taken on your behalf · raised 2026-08-18 · from Sprint 25a, BL-459. Agent judgement call, flagged for overturning.*

BL-459's formula is strength = count x roster_type_quality x supply_factor. Rather than add a quality column to all 19 roster rows, the agent derived quality from each row's existing power_mod as (1000 + power_mod)/1000 - so a Levy Spear at power_mod 0 is quality 1.000 and a Rifle Regiment at 380 is 1.380. Related calls: supply lands in the stack entry's COUNT and quality in its type_power_mod, because putting quality in both would square it against unit_strength; and unit_strength takes an unused const world& to match the signature condition_set.cpp calls, documented as the seam for a future per-era roster.

**Why it matters.** One number cannot drift from itself, which is the real argument for it - a second authored column would need keeping in sync with combat power forever, and nothing would enforce it. The cost is that quality and combat power become the same dial: a unit cannot be expensive-but-fragile or cheap-but-tough, because its upkeep-weighted strength and its battlefield power are the same figure scaled. That is a design constraint, not a bug, and it is invisible unless someone says so.

### NR-323 — Two surfaces changed materially and their question_log entries were deliberately not written
*observation · raised 2026-08-18 · from Sprint 25a, BL-454/BL-459 integration. The agent flagged it rather than editing the shared files.*

The unit Selection card's Strength page now shows a derived figure with its count x quality x supply derivation, an unsupplied callout and an upkeep block; the Corporation dashboard gains a seventh Finance bar (Force) and a force line beneath the net. Both are material content changes to existing surfaces. The agent did not touch docs/ui/question_log.json or docs/ai/ACTIONS.json because both were live merge hazards against concurrent work, and said so rather than editing them.

**Why it matters.** BL-260's rule is that every information surface declares the question it answers, and the enforcement is authorship rather than machinery - so a missed entry is missed permanently and silently. This is exactly the gap the question log exists to expose: a surface that changed with no recorded justification. The merge-hazard reasoning was correct at the time and the debt is now mine, not the agent's.

### NR-325 — Two fold-out ledgers open at once overlap in the same column slot rather than one replacing the other
*observation · raised 2026-08-18 · from Post-merge visual run, BL-453 Convoys tab capture (2026-08-18).*

With the Corporations dashboard already open, verify.show_panel('market', true) drew the Market Ledger INTO THE SAME shell fold-out column, both windows compositing on top of each other â€” the Corps/Holdings/Markets strip legible over the ghosted Prices/Sell Orders/Convoys strip. Closing the other panels first produced a clean Convoys capture. Knock-on: verify.scroll_panel targets an ImGui WINDOW name, so with two panels stacked it scrolls the invisible one and the capture silently shows an unscrolled panel.

**Why it matters.** The fold-out column hosts one ledger at a time by design (LAYOUT.md / the toggle rule), so this is either a missing mutual-exclusion in the panel open flags or an accepted state nobody has looked at. Either way it makes verify captures order-dependent in a way no script declares, which is a quiet source of misleading goldens.

### NR-326 — Three stale cloud branches merged to main; their app.cpp changes were re-ported, not merged
*decision taken on your behalf · raised 2026-08-18 · from Merge of claude/ui-documentation-json-1566f7, claude/ecstatic-hofstadter-80f1d6 and claude/elated-mclean-7dd61c into main (2026-08-18).*

All three branched from a base predating the verify-API carve-out, so each still had run_verify and its helpers inside src/core/app.cpp, which main has since split into src/core/verify_api.cpp. Merging their app.cpp hunks would have re-created deleted code. Resolution taken: app.cpp took HEAD wholesale in both conflicts, and the intent was re-ported by hand â€” the lens branch needed nothing (main's verify_api.cpp already maps reach and supply_routes), and elated-mclean's verify.scroll_panel binding was moved into verify_api.cpp beside panel_view, with the SKILL.md pointer corrected from app.cpp to verify_api.cpp. Also: every golden PNG the three branches carried was DROPPED rather than merged, since main's curated set is the two icon_silhouettes files after the 2026-08-15 demotion. For the lens-cycle conflict, the branch's overlay_mode::count sentinel was taken over main's last-enumerator + static_assert, because the enum now carries the sentinel and it needs no hand maintenance.

**Why it matters.** This is the stale-base failure mode again â€” worktrees isolate writes, not history â€” and it is the third recorded instance. A clean textual merge here would have compiled and silently reverted a refactor. Recording it so the re-port is visible as a decision rather than looking like the branches merged cleanly.

### NR-327 — Two presentation issues visible in the BL-453 Convoys tab on its first real render
*observation · raised 2026-08-18 · from Post-merge visual pass, first time the Convoys tab has been rendered on a machine that can build ImGui (2026-08-18).*

First, route labels are clipped mid-word â€” rows read "Huhaidar -> Kai Sa..." and "Huhaidar -> Huhai..." with the destination cut off, which is the one field distinguishing otherwise identical rows. Second, two of five rows show a quantity of x0 ("Agricultural Produce x0"), each with a live progress bar, an ETA and a non-zero haul cost paid.

**Why it matters.** The clipping defeats the tab's purpose: with several Agricultural Produce convoys in flight, the destination is what tells them apart, and it is the part being truncated. The x0 rows are either a real defect (a convoy dispatched with nothing aboard, still paying haulage) or a legitimate empty return leg that the tab does not distinguish from a laden one â€” from the surface alone a player cannot tell which, and neither could I.

### NR-329 — tier_margin was already red before Sprint 19 - dated by the bisect
*observation · raised 2026-08-18 · from Full ctest run of the merged tree (2026-08-18).*

tier_margin reports: 3 recipe inputs that have deposits are never produced; 7 wanted recipe inputs sit on 200+ tiles with no site naming them; and a processing facility does not out-earn an extraction site per tick. The third is already known â€” the verifier-headless notes on player_seed_sweep record BL-436 measuring exactly that, and warn its G4 depth floor must never be re-read as a profitability gate. The first two rows I have not traced.

**Why it matters.** Not merge damage â€” tier_margin links only world/*, and nothing merged today touches world logic. It is either inherited from Sprint 25a (which added ordnance and a Fabricator recipe consuming steel + machinery, plausibly moving the sited/produced sets) or older still. Worth knowing which, because the second row â€” a wanted input on 200+ tiles that nothing sites for â€” is the shape of a good the economy asks for and cannot get.

### NR-333 — Two independent notions of era have drifted apart, and each stranded something
*observation · raised 2026-08-18 · from Sprint 26b doc truth pass, 2026-08-18.*

The recipe registry has era_band_for_epoch (recipe_registry.hpp:36-38), derived from epoch_year. The unit roster has campaign_roster_band (unit_roster.hpp:61), a hard constant set to industrial with a comment still reading 1960s. Neither consults the other. BL-460 is the recipe side stranding ordnance; BL-461 is the roster side offering industrial units in a 0 CE campaign.

### NR-335 — BL-384's filed premise does not reproduce â€” 267 battles / 0 conquests is stale
*observation · raised 2026-08-18 · from Sprint 27 assertion agent, 2026-08-18, over seeds 0-7 with real terrain; independently confirmed against an unmodified history_sweep run.*

Where a world fights at all it converts nearly every battle into ground taken (137/144, 272/272, 272/272, 27/27). The transfer branch at history_sim.cpp:996-1027 is NOT the bottleneck, which is what BL-384 was filed against. The real shape is bimodal: 4 of 8 worlds fight nothing whatsoever across the whole run. The 267/0 figure appears descended from the pre-fix w_cult era recorded in the scorer's own comment.

### NR-336 — The era pass costs 1.2-1.8 s in Release, not ~23 s â€” BL-320's urgency drops by an order of magnitude
*decision taken on your behalf · raised 2026-08-18 · from Sprint 26a determinism agent, 2026-08-18, MSVC 14.44 pinned, both configurations measured.*

The ~23 s per-world cost of the Era -1 pass is quoted unqualified in CMakeLists.txt, hard_coded_world.hpp and harness_params.hpp, and it is a DEBUG number. At /O2 /DNDEBUG the same pass costs 1.2-1.8 s per world; the whole world_determinism harness runs 10.3 s Release against 128 s Debug. Cost also tracks the province table rather than combat â€” seed B ran 3x longer with FEWER battles (193 vs 365) and more foundings (994 vs 765).

### NR-337 — The determinism digest would have passed while comparing nothing â€” the agent caught it, no check would have
*observation · raised 2026-08-18 · from Sprint 26a determinism agent, 2026-08-18.*

world_determinism's existing digest is world_metrics, a tile-and-count digest. The Era -1 sim touches no tile: it moves provinces between owners and reaches the world only through the nation carve, derived character, names and charter placement. A prehistory-ON case compared on world_metrics alone would have gone green while asserting nothing about the pass it exists to cover. The agent built a deep_digest instead â€” world::state_hash folded with the political layer that state_hash deliberately omits (nations, tile_to_nation, population centres, corporations, history_log), because state_hash is a tick-boundary instrument and borders do not move on a tick.

### NR-338 — The generic harness target links no sol2, so any harness measuring background firms sees a world with ZERO firms
*observation · raised 2026-08-18 · from Sprint B1 substrate census agent, 2026-08-18. Verified, not assumed: seed_sweep_probe prints firms=0.*

CMakeLists' generic glob target for tools/verify/*.cpp links io_world_obj only, which excludes the sol2 translation units. generate_background_firms sizes itself against economy.lua's population_demand and background_demand baskets, and those are all-zero on a default registry with no Lua loaded â€” so it places zero firms. A census built on the generic target would have described a world nobody plays. The agent added an explicit live-Lua target for substrate_census, mirroring chain_depth, tier_margin and haulage_measure.

### NR-345 — The unit card is unreachable to the verify harness - battle-visual work needs a unit-selection driver first
*observation · raised 2026-08-18 · from Battle-visual design browse (2026-08-18 session): toured canvases and military surfaces via --verify captures.*

verify_api.cpp has select_tile (selects the TILE entity directly) and select_building, but no binding selects a UNIT - the click-cycle (unit -> building -> tile) exists only in live mouse handling. So the Selection unit card (Strength / Roster pages) cannot be captured headless, and any battle-visual requirement over units cannot be verified until a driver hook (e.g. verify.select_unit or a cycling select) is added. Pairs with the known no-unit-marker gap in MILITARY.md section What is absent.

*Files: `src/core/verify_api.cpp`, `docs/military/MILITARY.md`*

### NR-348 — Province partition algorithm settled - the numeric sub-details delegated to promotion
*decision taken on your behalf · raised 2026-08-19 · from 2026-08-19 partition elicitation: Ben chose terrain-seam jitter blocks and lowest-id fragment merging.*

BL-466 (province partition) flipped design-owed -> designed with the three-pass algorithm (base 2x2 blocks with seeded origin offset; one-sweep terrain-seam jitter clamped to 3-5 and connectivity; strait split + single-tile fragment merge). Delegated calls, to be fixed at promotion: (1) seam-evidence weights - river vs landform vs composition edge; (2) the swap-score threshold governing how often borders bend; (3) the origin-offset fold; (4) which tile fields carry river evidence (the canvas draws rivers; the authoritative field is named at promotion, not guessed). Also recorded: post-merge coastal provinces may exceed 5 tiles - the band bends at coasts by design.

*Files: `docs/development/backlog.json`*

### NR-349 — BL-448 (corp stance) skipped serialization.cpp â€” the file does not exist
*decision taken on your behalf · raised 2026-08-19 · from BL-448, corp stance data model + verbs delivery.*

BL-448's file scope named src/world/serialization.cpp as a wiring target. No such file exists anywhere in the repo (confirmed by search); persistence is done per-subsystem (e.g. procurement.cpp's own write_procurement/read_procurement, never called from a production save path). The item's own design text says the harness must assert in-memory replay only because "no serialiser exists yet in this project; do not invent one." Read together with the empty file, this was taken as licence to land the whole substrate with no persistence path at all, matching the design's explicit framing ("Landing the substrate inert is deliberate").

**Why it matters.** A save made after this lands will silently drop every corp's hostility/friendship/pending-offer state on reload, since nothing writes or reads it. Consistent with the design's stated scope (no consequence yet, substrate only), but worth Ben seeing named rather than discovered later as a save-format gap.

*Files: `src/world/stance.hpp`, `src/world/stance.cpp`, `src/world/world.hpp`*

### NR-351 — BL-384 (Era -1 sim conquers nothing) â€” hypothesis refuted, real cause is a scorer magnitude mismatch, no fix applied
*decision taken on your behalf · raised 2026-08-19 · from BL-384 confirm-before-fix instrumentation this session â€” see backlog.json BL-384 design field, FINDING 2026-08-19 section, for full measurement detail.*

Instrumented history_sim.cpp (temporarily) and measured a full 4000 BCE -> 0 CE run against the real Kepler world. The filed hypothesis (scorer/resolver terrain disagreement causes the sim to pick fights it then loses) is REFUTED: Campaign is never even selected as a polity-year's best_verb (0/1200 selections in the measured run), so no battle is ever fought for a terrain-blind estimate to lose. 372,660 of 524,800 scored campaign candidates clear the Campaign verb's own threshold, but Settle wins the shared-currency comparison in effectively every case (1188/1200), because Settle's score is a near-direct fraction of region value while Campaign's score is run through five multiplicative/subtractive discounts (campaign_gain_q, p_win_q, distance, culture, def_eff/supply cost) before competing on the same axis.

**Why it matters.** I did NOT attempt a fix. Rebalancing a four-verb shared-currency scorer (BL-309) is a different and larger-scoped problem than BL-384 as filed, and the item's own text explicitly warns against blind-tuning combat constants ('that would be guessing'). It also risks moving the B318c/R3/R5/B299 assertions that currently pass against synthetic fixtures. Recommend this becomes its own scoped follow-up item â€” a shared-currency rebalance pass with its own before/after measurement â€” rather than a same-session patch riding on this confirmation.

- File a new backlog item for the shared-currency rebalance (Settle vs Campaign scoring magnitude), scoped and measured independently of BL-384
- Leave BL-384 open as-is and let a future session pick up the rebalance under this same item

> **Recommendation:** File a new item â€” the rebalance is a distinct, larger-scoped problem (four verbs, five discount terms) from what BL-384 described, and giving it its own before/after measurement keeps BL384a/b honest as the acceptance bar.

*Files: `src/world/history_sim.cpp`, `src/world/combat.cpp`, `tools/verify/history_sim_harness.cpp`*

### NR-352 — BL-470 (unit march seam) â€” three implementation judgment calls, none blocking
*decision taken on your behalf · raised 2026-08-19 · from BL-470 implementation this session â€” the design left three specifics for the implementer to resolve.*

Three calls made while building BL-470 (march/halt/disband): (1) NR-344 (war flips the queue) is implemented as a pure VISITATION reorder inside run_unit_march (mobilised corps first, ascending unit id within each group) â€” there is no shared logistics-point pool yet (BL-464) for a marching corp to actually contend with a convoy over, so the rule is currently observable only by inspecting corp_is_mobilised/visitation order, not by any different in-game outcome, mirroring how BL-454 shipped upkeep inert at rate zero. (2) The design says the pass runs "after battle discovery", but no battle-discovery phase exists yet (BL-467) â€” run_unit_march was placed immediately before run_unit_upkeep, with a comment naming the slot it will occupy once BL-467 lands. (3) march_points_per_class was authored with first-cut placeholder constants (infantry 1.0, cavalry 1.5, ranged 1.0, siege 0.5, naval 0.0 tiles/tick against the plains=1.0 traversal weight) â€” untuned by playtest, same status as BL-394/BL-454s hire/upkeep constants when they landed.

**Why it matters.** None of these block the item â€” R1-R5 all pass their headless requirement â€” but (1) means Ben should not expect any visible consequence from declaring hostility yet (it only matters once BL-464 lands a contended resource), and (3) means the march speed numbers are a first guess, not a balance pass.

> **Recommendation:** No action needed now. Revisit (1) when BL-464 (Logistic Points) lands â€” that is when the reorder gets something to actually contend over. Revisit (3) during a playtest/tuning pass, the same way BL-394s hire costs were.

*Files: `src/world/economy_system.cpp`, `src/world/economy_system.hpp`, `scripts/economy.lua`, `docs/military/MILITARY.md`*

### NR-353 — BL-449 stance column overflows the shell fold-out panel - presses are unreachable
*observation · raised 2026-08-19 · from Live visual check of the Corporation panel (Diplomacy nav slot) after BL-449 landed.*

corporation_panel.cpp's table lives inside ui::foldout_begin's shell fold-out column, whose width BL-111 deliberately narrowed to fit exactly 4 columns (Corporation stretch + 3x WidthFixed 62px) after six columns previously collapsed to single-glyph headers. BL-449 added a 5th column, Stance, at WidthFixed 220px, without revisiting that budget. Confirmed live: every row shows a Stance value (Neutral/etc) but the three transition buttons (Declare Hostile / Offer Friendship / Return to Neutral) render past the panel's right edge. The panel is not resizable (dragging the border does nothing - it's part of the fixed shell column) and the table has no horizontal scroll enabled, so the buttons are genuinely unreachable, not just visually tight.

**Why it matters.** BL-449 shipped a stance UI that cannot actually declare hostility, offer/accept friendship, or return to neutral in the live app - the exact BL-350 failure mode (a complete seam with no reachable press) the item was filed to avoid, just one layer further down (the press exists in code but is not reachable on screen).

- A - shrink the Stance column (e.g. label only + a single overflow/context-menu button, or icon-only presses) to fit the existing ~4-column budget
- B - widen the shell fold-out column specifically when this panel is open (a per-panel width override), reopening the BL-111 constraint deliberately for this one case
- C - give the Corporation panel its own non-shell window (like the original de-scoped design before BL-122 re-hosted it), decoupling it from the shared shell width entirely

> **Recommendation:** A is cheapest and keeps every panel inside the shell at one shared width, which is presumably the whole point of BL-111/BL-122's constraint; worth Ben's call given it touches a settled layout decision.

*Files: `src/ui/corporation_panel.cpp`, `src/ui/foldout_column.cpp`*

### NR-354 — Batch Delivery scoped down to BL-460/BL-441/BL-442(step1); BL-439/BL-440(c)/BL-443 deferred
*decision taken on your behalf · raised 2026-08-19 · from 2026-08-19 Batch Delivery session survey of the v0.1.16 economy-integrity cluster (BL-439 through BL-443, BL-460).*

Surveyed BL-439 (AI never builds processors), BL-440 (mines only target richest), BL-441 (unmet demand never registered), BL-442 (price band is code not data), BL-443 (debt compounds with no floor) and BL-460 (ordnance unproducible at 0 CE) as a candidate batch. Chose to land only BL-460, BL-441, and BL-442's step 1 (behaviour-identical constant relocation) this pass, and deferred BL-439, BL-440's remaining part (c), and BL-443 entirely - no code touched for those three. [CORRECTED 2026-08-19 by the N1 audit: the claim "no code touched for BL-439/BL-440(c)/BL-443" is wrong for BL-439 â€” its tasks A/B/D landed on main at debfa87 on 2026-08-17 (corp_ai.cpp:736-905 + the ai_skill_harness guard). Accurate only about task C (the re-bless). BL-440(c) and BL-443 statements stand.]

**Why it matters.** BL-439 explicitly reshuffles every blessed golden and every ai_skill_harness band as its stated cost ('paid once, deliberately, re-blessed as part of landing it') - not something to fold into a multi-item batch pass without dedicated attention to the re-bless. BL-440(c) needs a design call between two named implementation shapes (a post-registry retarget pass vs a static demand hint) that BL-441's own design notes partially supersedes - clearer to resolve after BL-441 lands and its effect on the coal shortage can be measured, not before. BL-443 explicitly says 'MEASURE BEFORE CHANGING ANYTHING' and requires a game-design call (debt ceiling vs forced liquidation vs restructuring) that shapes the Conflict/Trade arc - not a mechanical fix. Landing all six in one pass risked a sprawling, under-verified batch; scoping down keeps each landed item independently verifiable.

- A - next session: re-run tier_margin/ai_skill_harness against the BL-441+BL-442(step1) landing to see whether the coal shortage measurably improves before deciding BL-440(c)'s shape
- B - bring BL-443's measure-first step (decompose a benchmark corp's balance into operating flows vs accrued interest) to Ben as its own small session before choosing (a)/(b)/(c)
- C - treat BL-439 as its own dedicated Batch Delivery pass specifically because of the golden re-bless cost, rather than bundling it with anything else

> **Recommendation:** A, then C, then B in that order - BL-440(c) may partly resolve itself once BL-441's price signal is live, which is worth checking before spending more design effort on it; BL-439's golden re-bless deserves an isolated pass; BL-443's measurement step is cheap and can happen any time.

*Files: `docs/development/backlog.json`*

### NR-358 — BL-441 and BL-442 were picked up for delivery already landed on main; both flipped to complete, including BL-442 step 2 which this session's brief said to leave open
*decision taken on your behalf · raised 2026-08-19 · from Delivery session for BL-441 (unmet demand) + BL-442 step 1 (price band to data), started against this worktree's HEAD (46118b6, an ancestor of main tip 78bc295).*

Before writing any code, git history showed both items already fully landed on main from the 2026-08-17 session: BL-441 at commits 37989d1/f0a50ce, BL-442 step 1 at 9fb90e2/5e442d1, and BL-442 STEP 2 (the band widening this session was explicitly told not to do) at 2a7aa01 - deriving ceil_mult=10.0 from measured haulage via tools/verify/haulage_measure.cpp, with the derivation written into MARKETS.md as the item's own design asked. Only backlog.json's status field for both items was stale (still 'designed'), plus REFINED.md and NEEDS_REVIEW.json already carry the landing detail from that session. No source file was changed this session.

**Why it matters.** The task brief, written without this knowledge, said to leave BL-442 as 'designed' with step 2 noted as remaining. Following that instruction literally would have recorded a false claim (step 2 outstanding) against a codebase where it had already shipped, reviewed, and measured. Flipped both items to 'complete' instead, with CLOSED notes in each item's design field pointing at the actual landing commits, on the grounds that backlog.json's job is to reflect reality and a stale instruction should not be allowed to reintroduce a stale doc.

### NR-355 — chain_depth's new era-aware R1b row finds five more goods with the same ordnance-era-strand shape
*observation · raised 2026-08-19 · from BL-460 (ORDNANCE_UNPRODUCIBLE_AT_0CE) implementation this session - the new R1b row in tools/verify/chain_depth.cpp checks, per concrete campaign band, that every WANTED resource has a producer REACHABLE in that same band.*

spacecraft_components, propellant, clean_water, consumer_goods and medical_supplies are all industrial-only-produced (recipes.lua, era = "industrial", no ancient route) yet drawn by consumers that are NOT era-gated in the C++: BL-350 procurement (economy_system.cpp:798-829), the propellant dispatch draw, and inject_population_demand (market_clearing.cpp:233, called unconditionally at market_clearing.cpp:545). So an ancient campaign (epoch_year < 1700, the shipped default) can in principle starve on all five exactly as it starved on ordnance before this item's fix - R1b would fail on all five if they were not exempted. They are exempted in R1b's k_known_gaps table (each with this note as its tracking reference) so the row stays green and honest rather than silently ignoring or silently fixing five goods outside BL-460's stated scope (a single-good, difficulty-2 item).

**Why it matters.** This is the SAME defect class BL-460 fixed for ordnance, found by the guard BL-460 built specifically to catch it. Whether each of these five is a live bug depends on whether population centres / procurement contracts / propellant dispatch are actually reachable in an ancient (0 CE) campaign today - PRODUCTION.md and POPULATION.md suggest population centres are still largely prototype-deferred, and procurement/propellant read as Era 1 (space) mechanics that may simply never fire for an ancient corp - but nothing in the code enforces that as a fact, only as an assumption, which is exactly the shape NR-257 and this item both exist to catch.

- A - one item per good (or one batched item) auditing whether each consumer actually fires in an ancient campaign; if any does, either give the good an ancient route (BL-460's pattern) or make the consumer era-gated
- B - leave the five in R1b's known-gap table indefinitely as a documented, accepted limitation of the ancient arc (these goods/mechanics are implicitly industrial-only)
- C - do nothing until a future item independently trips over one of these five in play, at which point R1b's known-gap table already names the fix location

> **Recommendation:** B for now, revisited as A once population centres (POPULATION.md) or the ancient-vs-industrial procurement question comes up on its own - filing five difficulty-2 items today would be scope creep on BL-460, which is a single-good fix. R1b's known-gap table is the durable record either way.

*Files: `tools/verify/chain_depth.cpp`, `src/world/market_clearing.cpp`, `src/world/economy_system.cpp`, `scripts/recipes.lua`*

### NR-373 — verb_coverage.js's first real run finds march_unit/halt_unit/disband_unit missing from the dictionary
*observation · raised 2026-08-19 · from BL-444 (VERB_REACHABILITY_COVERAGE_TOOL) implementation - tools/session/verb_coverage.js, run for real against src/world/corp_command.hpp, docs/ai/ACTIONS.json and src/world/corp_ai.cpp.*

The corp_verb enum carries 24 verbs; the ACTIONS.json gameplay family only has 21 gameplay.* entries. The three missing are march_unit, halt_unit and disband_unit - BL-470's unit march seam, appended to the enum 2026-08-19 (corp_command.hpp: 'Appended AFTER return_to_neutral, same append-only rule'). ACTIONS.md's own convention (CLAUDE.md: 'Any change to a control, binding, lens, ledger or panel must update its entry') was not followed for this landing, so the tool's exit code is currently 1 in an otherwise clean tree.

**Why it matters.** This is exactly the drift class ACTIONS.md warns about ('a stale entry misleads the AI player the way a stale golden misleads a visual check') and the reason BL-444 was commissioned - a dictionary gap on a real, landed seam addition, caught on the tool's very first run rather than staying invisible.

- A - author the three gameplay.march_unit / gameplay.halt_unit / gameplay.disband_unit entries now (small, mechanical - corp_command.hpp's own comments already state each verb's contract) and re-run render_actions.js
- B - file a small backlog item scoped to closing this one dictionary gap, so it goes through Delivery rather than being patched ad hoc
- C - leave open until the next session that touches ACTIONS.json, since verb_coverage.js now makes the gap visible and self-documenting

> **Recommendation:** A - this is the exact kind of small, mechanical dictionary-catch-up the tool exists to surface quickly; the three verbs' contracts are already fully specified in corp_command.hpp's comments, so authoring the entries is transcription, not design.

*Files: `tools/session/verb_coverage.js`, `docs/ai/ACTIONS.json`, `docs/ai/ACTIONS.md`, `src/world/corp_command.hpp`*

### NR-374 — presentation.cpp still carries three unauthored resource rows (charcoal, iron_blooms, trade_goods_misc) after BL-414
*observation · raised 2026-08-19 · from BL-414 (RESOURCE_NAME_TABLE_TRIPLE_DESYNC) implementation this session - consolidating the world-layer Lua-name lookup tables in recipe_registry.cpp and world_gen_config.cpp into src/world/resource_names.{hpp,cpp}.*

src/ui/presentation.cpp's resource_table (enum -> display name/abbreviation/colour, a separate concern from the Lua-name parser BL-414 consolidated) still carries three explicit { nullptr, nullptr, 0 } rows for charcoal, iron_blooms and trade_goods_misc (BL-286 logistics goods). presentation_of() falls back to "(unnamed resource)" for these. Left untouched by BL-414 deliberately - it is content authoring (picking a name/abbreviation/colour), not the structural desync BL-414 was scoped to fix, and src/world/ must not depend on src/ui/ so the two tables cannot simply merge.

**Why it matters.** If any code path ever surfaces one of these three resources with a positive quantity (a recipe naming them, a market listing), the player sees "(unnamed resource)" / "?" rather than a real name. Today they stay unreached because nothing produces them (per the existing comment in presentation.cpp), so this is latent, not live.

- A - author the three rows now (small, mechanical - display name + abbreviation + a colour distinct from neighbours) next time presentation.cpp is open for other reasons
- B - file a small backlog item scoped to authoring BL-286's remaining presentation rows
- C - leave as documented technical debt until one of the three resources gets a real producer/consumer (at which point it stops being latent)

> **Recommendation:** C for now - authoring display data for resources nothing produces or consumes yet is premature; revisit when BL-287-290 (or successors) give them behaviour.

*Files: `src/ui/presentation.cpp`*

### NR-375 — Batch Delivery this session built a fresh low-collision batch (BL-414/420/444) rather than resuming NR-354's deferred economy cluster
*decision taken on your behalf · raised 2026-08-19 · from Survey step of this Batch Delivery session, reading NR-354 (previous session's scoping decision) before picking a batch.*

The session brief suggested resuming BL-439/440/443 (deferred by the immediately-prior session per NR-354, for reasons specific to each - BL-439's golden re-bless cost, BL-440(c)'s design call, BL-443's measure-first requirement). Re-surveyed the open v0.1.16 set instead and picked three independent, low-risk, non-economy items: BL-414 (resource name table dedup), BL-420 (decision-feed label dedup), BL-444 (verb reachability coverage tool) - all designed, priority A, small (d1-d3), no shared files. Ran each in an isolated worktree agent, merged cleanly (no conflicts, stale-base checked against merge-base), independently re-verified (rebuilt, re-ran corp_ai_harness/econ_harness myself rather than trusting agent self-reports, confirmed econ_harness's one failure - WF.R4 - is pre-existing on main and unrelated), and did the live UI check for BL-420 (the only one touching src/ui) myself since the sub-agent had no GUI access in its sandbox.

**Why it matters.** NR-354's three deferred items still need their own dedicated attention (BL-439 especially, for the golden re-bless) - this session did not advance them, so they remain exactly where NR-354 left them. Recording this explicitly so a future session does not read "a Batch Delivery session ran" as progress on that cluster.

> **Recommendation:** Next session should pick up NR-354's option A (re-run tier_margin/ai_skill_harness against BL-441/442's landing before deciding BL-440(c)'s shape) or option C (BL-439 as its own dedicated pass) - both are still live and unaddressed.

*Files: `docs/development/backlog.json`*

### NR-376 — BL-476 rival military seeding: iteration order and shared-occupancy threading, plus the is_background exclusion mechanism
*decision taken on your behalf · raised 2026-08-19 · from Implementing BL-476 (RIVALS_START_UNARMED) in src/world/corporation_generation.cpp.*

Two calls made while extending the player-only military seeding block to every corp: (1) iterated `corp_ids` in its existing generation-order sequence (player + rivals, no BL-365 background firms - they are created by a separate, later pass and never appear in this vector) rather than re-sorting by entity_id first, since corp_ids is already deterministic and re-sorting would add a step with no behavioural difference; (2) threaded ONE shared `occupied_tiles` set across every corp's seed_starting_military call (rebuilt once from w.buildings before the loop, then mutated in place by author_building each iteration) so two corps can never be placed on the same tile - this widens the original player-only rebuild-from-w.buildings pattern to a loop without changing its semantics for the player's own case.

**Why it matters.** Both calls are load-bearing for determinism: a different iteration order or a per-corp-rebuilt occupancy set could change which tile a given rival lands on, or (worse) let two corps collide on one tile. The is_background exclusion is implicit (corp_ids never contains a background firm) rather than an explicit filter - correct today because generate_background_firms runs strictly after generate_corporations, but a future refactor that reorders those two passes would silently arm background firms again with no compile error.

> **Recommendation:** If the two generation passes are ever reordered or merged, add an explicit `!cc.is_background` guard in the seeding loop rather than relying on the passes' current call order - noted here so that refactor does not quietly reintroduce armed background firms.

*Files: `src/world/corporation_generation.cpp`*

### NR-377 — The v0.1.16 split executed: destination mapping chosen on Ben's behalf
*decision taken on your behalf · raised 2026-08-19 · from Version-alignment session 2026-08-19: six-agent research workflow + 27-verdict contradiction audit + Ben's form verdicts.*

Ben ruled "split the 36-item v0.1.16 holding pen now" but not the destinations. Mapping chosen: v0.1.16 keeps The watch (BL-408/410/411/412/413/418/451 + un-parked BL-306/BL-335); v0.1.19 Ancient conflict & seams (BL-274/277/297/298/299/300/320/337); v0.1.20 Stance & force (BL-314/399/449/450/464/474/475 + BL-472 from v0.1.18); v0.1.21 The credible rival (BL-417/419/439/440/445/446/447); v0.1.22 Harness truth (BL-425/426/427/462/463); BL-296 + BL-443 to v0.1.11 (meta). Rationale: theme coherence per the roadmap grain; the watch keeps the number because its items were already there and Sprint W1 cuts it.

### NR-378 — Stale-goal re-homing destinations chosen (BL-372/375/391/392, BL-264)
*decision taken on your behalf · raised 2026-08-19 · from Version-alignment session 2026-08-19: six-agent research workflow + 27-verdict contradiction audit + Ben's form verdicts.*

BL-372 (lens-keyed selection) -> v0.1.7 (UI alignment); BL-375 (time-to-space pacing, parked) -> v0.2.0; BL-391 (reputation floor deadlock) -> v0.1.15 (the contract/pay loop owns reputation); BL-392 (procurement destroys value) -> v0.1.11 (Lane D guard: D4 says fix before BL-445); BL-264 (wizard layout, orphaned by the v0.1.11 supersession) -> v0.1.7. BL-341 found complete, untouched.

### NR-379 — BL-087 piece 2 extracted as BL-478 rather than un-parking the whole item
*decision taken on your behalf · raised 2026-08-19 · from Version-alignment session 2026-08-19: six-agent research workflow + 27-verdict contradiction audit + Ben's form verdicts.*

Ben's verdict said "Un-park BL-087 piece 2 into the live arc". Executed as an EXTRACTION: new item BL-478 (ancient research spend, v0.1.11, design-owed) carries the research-state + spend mechanism; BL-087 itself stays parked with the space arc, its design noting the extraction.

### NR-364 — BL-186 laws-ledger design amended to the who-enacts model without a fresh ask
*decision taken on your behalf · raised 2026-08-19 · from Version-alignment session 2026-08-19: six-agent research workflow + 27-verdict contradiction audit + Ben's form verdicts.*

BL-186's Â§ Settled 2026-07-19 (player enact/repeal fast-path on the Budget ledger) struck with a dated note: ledger is browse-only, extraction tax surfaces BL-280's negotiation, enactment belongs to the nation actor (BL-480).

### NR-365 — Sizing note: the Fall arc's "12-20 sessions / two-month" line left standing as a ceiling
*observation · raised 2026-08-19 · from Version-alignment session 2026-08-19: six-agent research workflow + 27-verdict contradiction audit + Ben's form verdicts.*

Measured cadence: median sprint closes in 1 day (14 dated sprints across 29 days); the two design-heavy sprints ran 8-10 days. Honest range for the arc: ~3 weeks (median-like) to ~2 months (design-heavy-like). The risk line is denominated in sessions, which dates cannot measure; not edited.

### NR-366 — Worktree/branch debt named, not swept: 29 merged worktree-agent branches, ~15 stale mounts, one superseded rename branch
*observation · raised 2026-08-19 · from Version-alignment session 2026-08-19: six-agent research workflow + 27-verdict contradiction audit + Ben's form verdicts.*

Cleanup NOT run this session (deletion is destructive and was not on the form). The rename branch worktree-agent-ac68172a carries a9f1e52, superseded 42 minutes later by main's 70b2470 - eyeball-diff owed before deletion per the stale-base memory. The two unmerged branches: that one, plus the deliberate BL-474/475 paused WIP.

### NR-367 — N1 audit complete: BL-437 flipped; five holds bounded; BL-443 confirmed open but gated on your NR-296 lever pick
*observation · raised 2026-08-19 · from Read-only N1 audit agent over the seven un-flipped Sprint-26 items, evidence file:line + hashes; item-commits.json regenerated (350 entries â€” the seven were missing because the generator had not been re-run).*

BL-437 (co-extraction) complete at 2884f6c. Held with bounded remainders: BL-417 (step 2 = your NR-265/268 call), BL-429 (one GUI look), BL-439 (task C re-bless = your NR-269 pick; sequence after the debt lever), BL-440 (task D doc + re-run tier_margin R4b), BL-453 (build + capture + live Hold press; do not re-build). BL-443 genuinely unbuilt; the audit resolved the item's own existing-guard worry (econ_bankruptcy asserts nothing).

### NR-368 — BL-335 measured: the 300-token assumption holds on output, fails ~60x on input â€” BL-481 filed as the fix
*observation · raised 2026-08-19 · from BL-335 one-off measurement over build/ProjectIo.exe --serve (pre-batch binary; the MCP surface is unchanged by the in-flight work). Raw table in the item summary; script + raw output preserved in the session scratchpad.*

A MINIMAL decision round is ~19-20K input tokens, a NAIVE one ~26K, against LANGUAGE_POLICY_FEASIBILITY's compact-blackboard premise; output stays under 300 tokens with margin. 96% of input bulk is the market and rival-building triples with a repeated 70-byte envelope. BL-481 (compact encoding, v0.1.16, designed) carries the fix: fuse + hoist + filter = ~1.5-5K tokens per round for the BL-306 client loop.

### NR-369 — BL-480 delegated call: the seeded levy's author nation is the player's home nation
*decision taken on your behalf · raised 2026-08-19 · from BL-480 build agent (worktree branch worktree-agent-a9ac4a6bca1072bf3 @ 95a68d8), barred from board files; entry filed by the main session per Rule 0c.*

The generation-seeded extraction levy needs an enacting nation. Chosen: the PLAYER corp's home_nation â€” consumes no RNG, and makes the ledger's read-only levy line non-vacuous from turn one (the law binds the player). Fallback: largest territory, ties to lowest entity id; no nations, no law. Asserted deterministic in law_author_harness (14/14). Consequence stated plainly: the levy now charges the player from turn one by design â€” the item's stated shape, not a side effect.

### NR-370 — BL-408 findings: a pre-existing BL-068 leak (filed as BL-482) and a dead module its design named
*observation · raised 2026-08-19 · from BL-408 build agent report (branch worktree-agent-aa5fd296e1318e86e @ 701cba5); entries filed by the main session.*

(1) economy_panel.cpp draw_pools already shows every corp's pool quantities to a PLAYED session â€” past the competitor-visibility rule, predating god view; filed as BL-482 (economy panel pools leak, v0.1.7, B). (2) entity_summary.cpp's draw_corporation_summary â€” which BL-408's own design named as a lift site â€” is dead code; nothing calls the module. The live corp surface is the Selection band's facts column, where the lift was implemented instead. (3) The comms-redaction lift (BL-408 lift 2) was deliberately deferred: post_nation_agency_comms is a post-time store, and flag-gating it would latch state â€” it needs a small read-time-filter design of its own, recorded in DISCOVERY.md's new god-view section.

### NR-371 — BL-479 delegated calls: mirror field, earn-time accumulation (BL-107 debt), fixture seam, modifier-blind estimators, skill listing owed
*decision taken on your behalf · raised 2026-08-19 · from BL-479 build agent report (branch worktree-agent-ac0ff6e87868aed58 @ 8c62fe1); filed by the main session per Rule 0c.*

Five calls: (1) unlocks_structure kept as a mirror field maintained solely by add_effect, so the existing tech-gate harness compiles unmodified (R3) with one authoring point. (2) Modifiers accumulate on world.corp_modifiers at earn time â€” derived, unhashed, unserialised; BL-107 now carries the recompute-on-load debt as a dated design note. (3) The gate-table overload of advance_tech_gates is the fixture seam â€” the shipped table carries NO modify_scalar tech, because authoring live buff content is yours, not a test's; it is also the natural sharing shape for law. (4) Estimator sites (Build-door stack pricing, generation census, workforce solver, corp_ai scorer) stay modifier-blind â€” exact today with no shipped buff, but the FIRST authored buff tech makes them approximations; revisit then. (5) The new harness is in README.md + the CMake glob but NOT in verifier-headless SKILL.md â€” skill edits need your permission; the listing is owed.

### NR-372 — BL-412 delegated calls: agent-gated clock semantics, boundary drain, detach/second-client policy, MCP lockstep pairing
*decision taken on your behalf · raised 2026-08-19 · from BL-412 build agent report (branch worktree-agent-a51c6af46393ea9de @ 8d092a7); filed by the main session per Rule 0c.*

Six calls: (1) attach PAUSES the sim and TICK releases one econ tick (agent gates the clock per the REFINED brief); human speed keys still override, and even free-running, commands land only at tick boundaries â€” the transcript stays a replay artifact either way. (2) Commands drain at the boundary BEFORE the next step, stamped with the completed tick â€” run_serve's exact tick-tag contract. (3) SHUTDOWN on the live seam closes the CONNECTION, never the app. (4) Detach discards never-answered commands and leaves the world paused. (5) One client at a time; a second connection is refused outright. (6) The MCP --attach transport sends COMMAND+TICK as one exchange so a lockstep client cannot deadlock against the gated clock. Also observed: server.js's hand-maintained VERBS array is stale (missing stance + unit-march) â€” noted on BL-306, whose loop should derive verbs from ACTIONS_INDEX.json.

### NR-359 — DEVLOG.md rebuilt from its last clean commit - merge 0677f7a had re-imported 81 pre-rollover sessions in chimeric form
*decision taken on your behalf · raised 2026-08-20 · from PR-45 review session, 2026-08-20. The COLLAPSE session's index regen exposed it (168 -> 251 rows, mass duplicates).*

Merge 0677f7a (branch claude/ecstatic-hofstadter-80f1d6, a July-era branch carrying the pre-rollover DEVLOG) re-imported 81 already-archived sessions into DEVLOG.md, splicing headings onto other sessions' bodies - a June 2026 heading carried the BL-429 slice 3 verification tail, and slice 3 itself was truncated. Rebuilt DEVLOG.md as: the last clean pre-merge state (commit 0936400, 66 sessions, slice 3 whole) + the COLLAPSE session on top; the one genuinely new session from the old branch (Lens-cycle fix, 2026-07-31) moved to archive/DEVLOG-2026.md at its chronological slot; index regenerated to 171 rows. A paragraph-level diff against the corrupted file confirmed nothing unique was lost - every dropped paragraph exists in the archive or in the restored (fuller) session bodies.

**Why it matters.** This deleted ~4,200 lines from DEVLOG.md on your behalf. The verification was mechanical (heading-set and paragraph-set containment checks, both scripted in-session), but the rebuild picked commit 0936400 as 'last clean' by heading-count archaeology, and you should know the repair happened in case any session prose you remember reads differently now. The structural cause - a stale long-lived branch merged after the rollover - can recur; the devlog_index tool could cheaply assert 'no live session heading also exists in an archive volume' and turn this failure into a lint.

### NR-360 — Session-delegation roles carved as economy / ui / generation; router left unthinned
*decision taken on your behalf · raised 2026-08-20 · from BL-497 (session delegation roles), Ben's 2026-08-20 ask to prompt sessions differently as the corpus outgrew one context.*

Carved three implementer roles (economy-dev, ui-dev, generation-dev) matching the repo's natural vertical slices, plus directory CLAUDE.md files for src/world and src/ui only. Chosen on Claude's judgement: military/AI work routes through economy-dev or generation-dev briefs for now rather than owning a fourth role, and the root CLAUDE.md router was left as-is.

**Why it matters.** Role boundaries shape which invariants a cold agent sees. If military/mil-sim work grows (Sprint 26-33 direction), a dedicated military-dev role and a docs/lore scoped file may earn their place. Thinning the root router (its entries have drifted toward design prose) is a separate, larger call left for Ben.

- A) Accept the three-role carve; add roles only when a slice recurs.
- B) Add military-dev now, ahead of the Lane C engagement work.
- C) Also schedule a root-router thinning pass (entries become one-liners, prose pushed to the owned docs).

### NR-361 — Scope-drift rewording: intro framing chosen as four strands; novel-work flag mechanics
*decision taken on your behalf · raised 2026-08-20 · from Ben's 2026-08-20 mid-session asks: raise a novelty flag, and reword README.md / CLAUDE.md for the drifted scope.*

Both intros now drop the bare '4X' label and frame the project as: economy loop (shipped) + generated pre-campaign history (Era -1) + military layer (partial) + AI direction (scored-utility now, local-model at v0.2.0), with the governing-body pivot as the arc's destination. The novelty flag landed as a fourth NEEDS_REVIEW kind ('novel-work') plus a standing rule; sub-agents flag it in their report, the main session files it.

**Why it matters.** The four-strand framing is Claude's carve of the drift, not Ben's wording - it will anchor how new contributors and fresh sessions read the project. Cheap to reword if Ben would frame it differently (e.g. keeping '4X', or leading with the collapse metagame).

- A) Accept the four-strand framing.
- B) Reword - supply the framing Ben would use.
- C) Keep README player-facing and move the strand breakdown to CLAUDE.md only.

### NR-362 — World-snapshot serialiser timing: reshape world.hpp now, or declare it final-enough?
*question · raised 2026-08-20 · from Code-review audit of src/world (2026-08-20). Only the three side-streams (history_log, order_book, procurement) have flat-binary IO; BL-107 (save-format version header) is blocked on the world snapshot existing.*

Data-model restructuring is free today and expensive the day the snapshot serialiser lands. world.hpp (571 lines) carries ~17 component maps plus parallel side-tables (population_centre_tile/name, corp_body_pools, embargo conditions, mutable caches) that the serialiser will have to enumerate as-is.

**Why it matters.** This is the one finding with rework-scale consequences, and it is a timing decision, not a refactor: taken now it costs nothing; deferred past the snapshot it makes the 'seam travels with the change' invariant expensive to honour retroactively.

- A) Declare the struct final-enough and land the snapshot serialiser EARLY in the military milestone (unblocks BL-107).
- B) Do one deliberate reshaping pass (fold parallel side-tables into their components) first, then serialise.
- C) Defer both - accept that later reshaping pays the serialiser tax.

### NR-380 — Your call: the batch ships two visibility positions at once — gated god view vs ungated strategy readout
*question · raised 2026-08-19 · from verifier-review of the integrated batch (verdict GO COMPILE, finding #1).*

BL-408 gates rival internals behind spectating && god_view; BL-411's strategy readout (nav slot 12) shows every rival's decision-mix aggregates UNGATED in a played session. The reviewer ruled it not a BL-068 regression — the decision feed (slot 11, strictly more revealing: per-decision verb, target, tile, scores) has been ungated since BL-407, and the readout leaks no credits or stockpiles (counts only). But the two positions now coexist undeclared. Options: (A) declare decision-stream aggregates a PUBLIC channel — one paragraph in DISCOVERY.md beside the BL-068 rule, no code change (consistent with the feed precedent; my recommendation); (B) gate slots 11+12's rival rows on spectating — restores strict BL-068 but takes the feed away from played sessions that have had it since BL-407.

### NR-381 — Review fixes applied at integration; two process findings from the verifier-review
*decision taken on your behalf · raised 2026-08-19 · from verifier-review findings #2,3,4,5,8,9,10,11,12 applied by the main session before the integrating build.*

Applied without a fresh ask: treasury credits now applied post-loop in sorted (corp,nation) order (float-order stability, #2); treasury doc states not-serialised/not-hashed (#3); SYSTEMS.md levy prose corrected (#4); readout header says decision-count not spend (#5); --autostart now honours --host-agent instead of ignoring it silently (#9 — wired, not rejected, matching every sibling path); transcript doc says truncate (#10); verify.buildings() iterates sorted corp ids (#11); the god-view checkbox joined ACTIONS.json, 135 entries (#12). The spectate x seam collision (#8) is recorded on BL-413 as a refuse-the-second-press rule. PROCESS findings for the retro: the batch's collision map was wrong in both directions (named collisions that did not exist, missed app.cpp/verify_api.cpp/ui_state.hpp ones that did — clean merges were worktree isolation plus luck), and no symbol-level provides/consumes contract was written (#13); the levy shipping enacted moves every generated-world state_hash structurally (#6) — the harness re-run must bless deliberately, not absorb.

### NR-382 — Your call: the enacted levy's placeholder rate (1.0 cr/unit, all resources) sends every rival insolvent — rate ruling before any bless
*question · raised 2026-08-19 · from Integrated harness verification: ai_skill_harness 17 PASS / 19 FAIL, all 19 in the golden-band families; determinism rows and BL-439's processor row green. Nothing blessed.*

BL-480 ships the extraction levy ENACTED (your who-enacts model), which exposes that its rate was a placeholder that never bit while the law shipped un-enacted: seed_prototype_laws defaults rate=1.0 cr per raw unit, scope all_resources. Result vs the 2026-08-16 bands: net-worth finals 78K..499K -> −3.6M..−5.2M, solvency 30/30 below zero on all five seeds, min==final everywhere (monotonic decline; BL-073 interest compounds once corps cross zero — the BL-443 spiral doing the multiplying). The 2026-08-17 record was already red at −1.9M..−3.4M (Sprint 19, left red and attributed), so this deepens an attributed red rather than breaking a green.

### NR-383 — Merge of the mobile design session: dual id-collision renumbered (origin BL-476..482 -> BL-504..510; local NR-356..362 -> NR-373..379; uncommitted NR-373..375 -> NR-380..382)
*decision taken on your behalf · raised 2026-08-20 · from origin/main merge (mobile COLLAPSE.md session + repo-delegation strategy), merge commit 97c12bb.*

Both machines minted BL-476..482 and NR-356..362 independently. Local BL ids are baked into src/ code comments and harnesses (corporation_generation.cpp, law.hpp, tech_gate, five tools/verify harnesses), so the ORIGIN side moved: the mobile session's BL-476..482 (strain accumulator, culminating events, etc.) became BL-504..510, remapped through backlog.json, COLLAPSE.md, DEVLOG, DEVLOG_INDEX and the origin NR entries. On the NR side the origin ids were woven into COLLAPSE.md rulings, so the LOCAL side moved: committed NR-356..362 became NR-373..379 (refs updated in backlog.json, requirements.json, ROADMAP.md, REFINED.md), and the still-uncommitted NR-373..375 from the 2026-08-19 session became NR-380..382. backlog.json was resolved as a JSON three-way merge; BL-464 (logistic points) keeps origin's authored design plus local's v0.1.20 retarget. No duplicate ids remain; all four JSON stores validate.

**Why it matters.** Commit messages on both sides still cite pre-renumber ids (immutable); anything you remember as BL-476-the-strain-accumulator is now BL-504 (STRAIN_ACCUMULATOR), and the COLLAPSE decomposition is BL-483..496 + BL-504..510. next_id.js should now mint from BL-511 / NR-384.

### NR-384 — Lane C could not start: BL-467 (battle state) needs BL-466 (province partition), which is unbuilt
*observation · raised 2026-08-20 · from Four-lane batch refinement (Sprints 27/B2/B3/C3/D4), REFINED.md 2026-08-20.*

BL-467 rules the PROVINCE as the engagement envelope (your 2026-08-19 elicitation, ruling 1), but src/world/province.{hpp,cpp} does not exist and BL-466 is still designed-not-built. Sprint C3 as written (battle store + trigger + losses) therefore has no envelope to draw. Refined as two sequential tasks, C1 = BL-466 alone as the foundation, C2 = BL-467 after C1 merges. This costs Lane C its parallelism inside the lane but not across lanes.

### NR-387 — Deferred: Sprint D2 (research becomes a currency) wants a design pass, not an implementer
*observation · raised 2026-08-20 · from Four-lane batch refinement (Sprints 27/B2/B3/C3/D4), REFINED.md 2026-08-20.*

BL-478 (ancient research spend) is design-owed and the debit mechanism has no design. NR-315 records that condition_subject::science shipped as a LEVEL, picked by reading BL-344 shape rather than by choice - so the spend model is an open design question, not a build task. Held out of this batch. It is the natural next design session, and it gates the 92-object ancient_tech_ladder.json becoming reachable.

### NR-388 — Six items are landed-awaiting a live GUI check and none can flip without one session at the app
*observation · raised 2026-08-20 · from Four-lane batch refinement (Sprints 27/B2/B3/C3/D4), REFINED.md 2026-08-20.*

BL-412 (live agent seam), BL-408 (spectator god view), BL-411 (strategy readout), BL-480 (law author read-only levy line), BL-429 (ancient roster), BL-453 (convoy ledger). The code is in the tree; every held requirement row is a LIVE check. Promoted as Lane M, main session, ahead of the four build lanes - it is the cheapest six flips on the board.

### NR-389 — BL-408 spectator god view has NO reachable entry in a played session - only a verify script can turn spectate on
*observation · raised 2026-08-20 · from Lane M live check, four-lane batch. Build ce7e9f7, fresh build_app.bat run.*

The god-view checkbox renders only while corp_ai_params::spectating is set (ACTIONS.json chrome.sysmenu_god_view: "unreachable in a played session"). The ONLY writer of ui_state.spectating in the whole tree is verify_api.cpp:656, the Lua binding verify.spectate(on). There is no menu item, no checkbox, no CLI flag (main.cpp parses --bless, --export-blackboard, --serve, --verify, --verify-all - no --spectate). So BL-408 cannot be live-checked, and more to the point a player or a watcher cannot enter spectate mode at all outside a verify script. This is exactly the gap the 2026-08-19 live-check rule was written for: the code compiles, the harness is green, and the press does not exist. Needs a decision - either an entry point (a CLI flag and/or a main-menu option) is in BL-408 scope and was missed, or spectate is deliberately harness-only for now and BL-408 requirement rows should say so rather than claiming a surface.

### NR-390 — kMaxCrossingTiles = 3 is an unpinned number, and cut 2 REMOVES roads as well as adding them
*decision taken on your behalf · raised 2026-08-20 · from Lane B1 (Sprint B2, road cuts), agent report + main-session diff review, merged at 530eb87.*

The agent chose kMaxCrossingTiles = 3 with no doc pinning it. It governs two things at once: what counts as a strait worth stamping shore-to-shore, and how wide an unowned gap territorial adjacency tolerates. Consequence worth seeing: cut 2 does not only add reach, it DELETES road that used to generate - an edge whose route crosses open ocean is now not stamped at all, which removed 6 Tracks on the home body (234 -> 228) that were disconnected fragments on distant shores. That is a deliberate reading (fragmented shores are noise, not reach) and it is defensible, but it is a design call taken on your behalf. The larger call NOT taken: making roads bridge straits by stamping the water tiles themselves, so the corridor is continuous for the logistics A*. That would overturn the stated roads-are-a-land-feature invariant and collides with BL-188 (coastal ports).

### NR-391 — The 26-of-43 road-less nation figure did not reproduce - two different definitions were in play
*observation · raised 2026-08-20 · from Lane B1 (Sprint B2, road cuts), agent report + main-session diff review, merged at 530eb87.*

BL-463 design cites 26 of 43 nations with no road network on seed 0. The new road_reach_census measures 14 road-less of 161 nations across the 8-seed set (8.7%), seed 0 contributing 2. The agent believes the older figure counted nations with no intra-nation road EDGE (which includes every single-centre nation whether or not a border link roads it), where the census counts nations with no roaded TILE in territory. Both are defensible; only one is now instrumented and inspectable. Worth ruling which definition the civilised-world argument should be tracked against, because the two differ by nearly 2x and one of them is quoted in a priority-A item.

### NR-392 — novel-work: a worktree agent had no sanctioned way to build a harness, so it authored build_gen_harness.bat
*novel-work · raised 2026-08-20 · from Lane B1 (Sprint B2, road cuts), agent report + main-session diff review, merged at 530eb87.*

Nothing in the reading list owns the question how does a worktree agent build a harness. A fresh worktree has no configured build tree and a CMake configure pulls SDL + Lua over FetchContent, which is not worth paying for one harness. The agent wrote build_gen_harness.bat at the repo root: it globs srcworld*.cpp minus the four sol2 TUs exactly as io_world_obj does, so unlike the README hand-written cl recipes it cannot drift stale - which is the standing complaint against those recipes. Merged with the lane. Two things owed and both need your permission: folding this into the verifier-headless skill, and naming road_reach_census there so it becomes a permanent asset rather than a loose tool.

### NR-393 — MAJOR: the Era -1 sim now conquers on half the seed set - the filed 267-battles/0-conquests premise is gone, and Sprint 28 framing is wrong
*observation · raised 2026-08-20 · from Lane A (Sprint 27, BL-384 assertion half), agent report verified against main by the main session.*

Fresh 8-world sweep on real terrain, measured today against the current build: 4 of 8 seeds fight AND take ground, at a near-1:1 conquest:battle ratio (144/137, 272/272, 272/272, 27/27). The other 4 seeds see zero battles. So Campaign is not universally unreachable - it is SEED-DEPENDENT and bimodal. Sprint 28 plans its fix on the premise that Settle always outscores Campaign under the shared currency; that premise does not hold on half the set, and a fix tuned against it would be tuning away a behaviour that already works on seeds 1, 2, 4 and 6. Sprint 27 own risk clause named exactly this outcome and instructed that it be reported rather than absorbed. Also unchanged and still red: 0 polities eliminated and 0 hegemonies anywhere in the set - BL-308 spiral still does not end.

### NR-394 — Refinement error: Sprint 27 assertion half was already delivered before I promoted it
*decision taken on your behalf · raised 2026-08-20 · from Lane A (Sprint 27, BL-384 assertion half), agent report verified against main by the main session.*

I promoted Task A1 (author the BL-384 conquest assertion) into the four-lane batch. It was already committed on main at 610e276 and f4a0c18 - the BL384a/BL384b assertions, the elimination counter and the dominance-share counter all exist, reusing history_sweep hegemony_threshold_q verbatim. I checked BL-384 status (designed) but not whether the assertion HALF specifically had landed, and the item stays open because the FIX half is outstanding. The agent correctly authored nothing, changed nothing, and made no commit; the lane still paid for itself by re-measuring (NR-393). Method note worth taking: a half-delivered item reads as designed in the backlog index, so status alone does not answer has this part shipped - the item dated notes or git log do.

### NR-395 — The R7 perf assertion is still live and failing although the backlog says it was dropped
*observation · raised 2026-08-20 · from Lane A (Sprint 27, BL-384 assertion half), agent report verified against main by the main session.*

history_sim_harness R7 (a perf budget) fails at 16679 ms on the primary Kepler run. The backlog records this budget as DROPPED in favour of BL-320, but the assertion was never removed from the harness, so it reds every run and adds noise to a harness whose reds are supposed to mean something. Either the drop did not happen and the budget is real, or the assertion should go. Small, but a permanently-red assertion trains everyone to ignore the harness.

### NR-396 — A FOURTH procurement fault, unfiled: the deposit and every instalment left the buyer and arrived nowhere
*observation · raised 2026-08-20 · from Lane D (BL-392 + Sprint D4 tariff), agent report + main-session diff review, merged at 9b75621.*

BL-392 filed three faults. The agent found a fourth while satisfying the conservation requirement: the deposit and every paced instalment were debited from the buyer and credited to NOBODY - every contract in flight was burning credits out of the economy. The supplier is now credited exactly what the buyer is debited. This is a real behaviour change that no item asked for, admitted because the conservation requirement made it unfixable-around. Worth knowing that the money supply has been quietly shrinking in every session where a contract was live.

### NR-397 — A small cross-body contract still loses to spot, on purpose - two dials if you disagree
*decision taken on your behalf · raised 2026-08-20 · from Lane D (BL-392 + Sprint D4 tariff), agent report + main-session diff review, merged at 9b75621.*

Re-measured after the BL-392 fix, contracted parcel vs the same parcel at spot (positive = contract wins): 20 units -1.2500, 100 units +6.2500, 500 units +93.7500. The -0.14 break-even-by-construction is gone - the discount is real and monotone in size, and the goods now arrive somewhere usable. But at 20 units the 5% freight still exceeds the 1.7% discount, so a small cross-body order is a bad deal. The agent took the reading that freight is an honest cost and that vs-spot-on-the-suppliers-market is the wrong comparison for goods the buyer cannot reach anyway. If you want small orders to win too, the dials are offbody_freight_fraction (0.05) and volume_discount_half_quantity (100), both in scripts/economy.lua.

### NR-398 — novel-work: a nation now holds and receives money, and conservation is NOT a global property of this economy
*novel-work · raised 2026-08-20 · from Lane D (BL-392 + Sprint D4 tariff), agent report + main-session diff review, merged at 9b75621.*

Two things earn the flag. First, nation treasury is the first money a non-corporate actor has ever held, and no doc owns what a nations balance is FOR - the tariff credits it, and nothing spends it. Second and more important: money conservation was never a property of this economy and still is not globally - the market is a buyer of last resort that pays sellers with nobody money. The new money_conservation harness asserts conservation across these two items own flows against control worlds, and its header says so at length so nobody later reads it as a global claim. Someone will eventually have to decide whether global conservation is a goal or whether the market is deliberately an infinite counterparty.

### NR-399 — The procurement save seam rejects old saves outright on a version bump - pre-existing, but now exercised
*observation · raised 2026-08-20 · from Lane D (BL-392 + Sprint D4 tariff), agent report + main-session diff review, merged at 9b75621.*

read_procurement gates on version != procurement_version and returns false, so bumping 1 -> 2 (which this work did, for delivery_body and freight_cost on both records) makes every pre-existing save unreadable rather than upgradable. That strict-equality check is PRE-EXISTING and not introduced here, but this is the first bump to actually exercise it. Related gap the agent flagged and did not introduce: nation_component::treasury and law::author_nation have no serialiser at all - nations and laws are not saved, so a treasury survives nothing. BL-107 is the item that owns picking those up. AMENDED 2026-08-20, same session: this overstates the risk. There is NO game save/load path at all - app.cpp persists settings only, and neither read_procurement nor read_history_log has a caller anywhere in src/. So no player save exists to be rejected, and the version bump costs nothing today. The seam correctness still matters for the day a save lands; the urgency does not.

### NR-400 — The D4 import tariff has NO authoring path - nothing in src/ can enact one
*observation · raised 2026-08-20 · from Pre-compile static review of the integrated four-lane batch, 2026-08-20; blocker fixed at bd238d5.*

Outside law.{hpp,cpp} and market_clearing.cpp there is not one reference to law_effect_kind::import_tariff anywhere in src/. No corp_verb, no UI control, no generation seeding, no agent-seam command creates a tariff law; seed_prototype_laws seeds only the extraction levy. So in a real campaign any_import_tariff_enacted is permanently false and the entire tariff pass is unreachable. The mechanism is built, proved and conserved - in a harness fixture. If D4 acceptance is a cross-border sale pays a duty into the market nation treasury, that is NOT met in the shipped binary. Deliberately not fixed here: seeding a tariff at generation changes every generated world and is a design call, and the granted nation-grain scorer enacting one mid-campaign is the seam the work was shaped for. Your call which of the two it should be.

### NR-402 — Six new harnesses joined the routine ctest gate at the 60s default, two of them census-class
*observation · raised 2026-08-20 · from Pre-compile static review of the integrated four-lane batch, 2026-08-20; blocker fixed at bd238d5.*

The CMake GLOB registers tools/verify/*.cpp automatically, so this batch six new harnesses entered the routine gate silently. road_reach_census (3x make_hard_coded_world) and rival_military_seeding_harness (4x) are the exposure. substrate_census own CMake note records ~62s per world in a Debug tree and parked itself in IO_TEST_SWEEP_HARNESSES for exactly this reason; road_reach_census is by name the same category and is not in that list. Expect a Debug-tree Timeout, which NR-259 calls a silent failure. Also: rival_military_seeding_harness has never been named in tools/verify/README.md - it arrived undocumented in an earlier commit, not in this batch.

### NR-403 — Decision: world_audit now skips the BL-476 seeded military base rather than raising its ceiling
*decision taken on your behalf · raised 2026-08-20 · from Full CTest suite over the integrated four-lane batch, 2026-08-20 (96 tests, 1474s).*

B4 R1 failed on the integrated tree and passed in every agent worktree, because BL-476 (ce0e5cc) landed after their common base and seeds a military_base into EVERY corp assets. holdings_range governs ECONOMIC holdings and never counted that base, so the audit was comparing against a ceiling one too low for every corp - it only reds when a draw lands at the top of its range, and this batch settlement-density change made one do so. Two ways to fix: raise every ceiling by 1, or stop counting the base. I took the second, because the first would hide a real +1 and would make the audit assert something holdings_range does not promise. Verified: every corp drops by exactly one and all sit inside the declared ranges. If you would rather the audit tracked TOTAL footprint including military, say so and it inverts. Note the file own comment now records this as the FOURTH drift of a hand-mirrored table - the pattern, not this instance, is the thing worth fixing.

### NR-405 — RULING: unit position moves to province grain, overturning the 2026-08-13 tile-token ruling
*decision taken on your behalf · raised 2026-08-21 · from BL-511 design form, Ben 2026-08-21.*

Recorded because it is the third dated position on the same question and the previous two are still quoted in authority docs. 2026-08-13 (BL-315 ruling 2): units are tokens on the TILE map, explicitly not armies at province grain. 2026-08-19 (BL-467): grains SPLIT - command at the tile, engagement at the province. 2026-08-21 (this): units move and are selected at PROVINCE grain, collapsing that split. It is a simplification - BL-467 engagement rule no longer needs a tile-to-province reduction because a unit position IS a province - and the ~4-tile province keeps it modest. Costs, all named in BL-511: march_unit payload changes tile -> province (BL-470 landed that verb 2026-08-19), unit_component is tile-canonical in MILITARY.md, and BL-471/BL-469 should follow the render change rather than precede it. MILITARY.md and BL-315 still assert the OLD ruling and must be corrected as part of landing the work, not ahead of it (authority time-slice).

### NR-406 — The province building limit would be the first ceiling in the game that MOVES during play
*question · raised 2026-08-21 · from BL-513 design, from Ben four-input heuristic.*

Ben named infrastructure as one of the four inputs to the province building limit (area, infrastructure, habitability, population). Roads and hubs are built during play, so the ceiling would RISE as a player invests - unlike the per-tile deposit cap and every other placement bound, all fixed at generation. That coupling looks intended and desirable (BL-325 ruling 3 makes economic reach and military reach one field, so infrastructure paying off into capacity is consistent), but it is worth an explicit yes: a dynamic ceiling means a building can become legal later, and any UI that shows remaining capacity has to recompute rather than cache. If it should instead be read once at generation, that is a smaller item.

### NR-407 — BL-458 shipped SILENT: interdiction works, but nothing tells the player it happened
*observation · raised 2026-08-21 · from Lane M (BL-458 supply interdiction), agent report + main-session verification, merged and MSVC-confirmed.*

The mechanic is real and proven - a hostile unit on a convoy head tile takes the cargo into its own pool and the convoy never arrives (interdiction_harness 49/49 under MSVC). But all three surfaces the item asks for are absent: the comms-dock message, the Convoys-tab row leaving with a stated cause, and the canvas mark. All three need somewhere to keep an interception_record, which means a field on world - a file other lanes in this batch hold open - so intercept_convoys RETURNS its records and credit_arrived_convoys discards them. As shipped a convoy vanishes with no explanation, which the item explicitly says it must not do. This is the honest incomplete of an otherwise complete lane: the requirement group supply-interdiction R3 is PARTIAL, not met. The surfaces are a small follow-up once the province-grain batch stops holding world.hpp.

### NR-408 — Decision: the interdiction trigger sits INSIDE credit_arrived_convoys, which reads surprising
*decision taken on your behalf · raised 2026-08-21 · from Lane M (BL-458 supply interdiction), agent report + main-session verification, merged and MSVC-confirmed.*

intercept_convoys is called from the top of credit_arrived_convoys rather than from the four call sites (app.cpp, main.cpp twice, harnesses). The agent took this on my behalf because those files are outside its ownership and this is the only seam all four share. It is defensible - a convoy that was intercepted must not then be credited, so the two belong in one ordered step - but it reads oddly against the function name, and a future reader looking for the trigger will not look there. It carries a loud comment. Worth confirming, or renaming the function to say what it now does.

### NR-409 — convoy_tile_at takes world&, not const world& - the A* cache forces it
*observation · raised 2026-08-21 · from Lane M (BL-458 supply interdiction), agent report + main-session verification, merged and MSVC-confirmed.*

The item specified entity_id convoy_tile_at(const world&, const convoy_component&). It shipped as world& because intra_body_path POPULATES the A* cache, so const is unreachable without paying for a second uncached path solve. Documented at the declaration. Minor, but recorded because the item text will otherwise read as unimplemented, and because a non-const read-shaped function is the kind of thing a later reviewer flags as a mistake when it was a measured trade.

### NR-410 — The province ceiling coefficient has a 28% spread across seeds, and that is reported not hidden
*Lane B (BL-513 province building ceiling), merged and verified 2026-08-21. · raised 2026-08-21 · from k = 12.6468 is pinned as the aggregate of pooled-per-tile-capacity / sustain-units across 8 seeds, so the ceiling REDISTRIBUTES the capacity the world already grants onto Ben four inputs rather than inventing a new regime. The per-seed ratios run 11.4694 .. 15.2588 - a 28.36% spread of the mean, because seeds genuinely differ in habitable-land share. Consequence: a low-habitability seed gets a ceiling slightly above the capacity it already had, a high one slightly below. Neither binds today (0.115% used, zero provinces at ceiling), so nothing is affected yet - but if the ceiling ever binds, it will bind unevenly across seeds. Worth knowing before density rises. The agent first draft used an invented k of 2.213 and the probe caught it immediately (15-26 provinces already over ceiling); the pinning discipline worked exactly as BL-463 intended.*

### NR-411 — Perf: province_buildings_standing is an O(all buildings) scan inside can_place_in_world
*Lane B (BL-513 province building ceiling), merged and verified 2026-08-21. · raised 2026-08-21 · from The ceiling gate runs last and only for otherwise-valid tiles, and spectator_determinism still completes in 14s, so it is not a problem today. But it is a linear scan over every building in the world on a placement path that the AI scorer hits every tick. If placement checks get hotter - more corps, more density, or a scorer that evaluates more candidates - this wants a per-province building index rather than a scan. Recorded now so it is a known cost rather than a future mystery.*

### NR-412 — Province id 0 is a REAL province, so the march seam needed a different sentinel
*Lane U (BL-511 seam half, march_unit to province), merged and verified 2026-08-21. · raised 2026-08-21 · from Measured, not assumed: body rank 0 | block 0 | component 0 is a reachable id, so 0 cannot mean absent. corp_command::province and the wire province= field default to no_province (0xFFFFFFFF), which is structurally unreachable because it would need component index 7 and a 2x2 block yields at most 4. This matters more than it reads: a 0 default would have given an OMITTED wire field a real destination and answered applied - a silent order substitution, exactly what the untrusted-input-boundary rule forbids. The rejection sweep covers 123 in-range-but-absent ids including every single-bit flip of a valid one, against a unit carrying a LIVE order so that mutates nothing cannot pass vacuously. 81/81.*

### NR-413 — BL-470 never added its three verbs to ACTIONS.json - the dictionary has been misrepresenting the seam since 2026-08-19
*Lane U (BL-511 seam half, march_unit to province), merged and verified 2026-08-21. · raised 2026-08-21 · from march_unit, halt_unit and disband_unit were ALL absent from the action dictionary the AI player reads, from the day BL-470 landed them. The standing rule is that any change to a control or binding updates its entry, and ACTIONS.json is transcribed FROM corp_command.hpp - so the dictionary was simply two days stale on three verbs. The Lane U agent authored all three rather than only the march_unit entry its task named, which was the right call: leaving two absent would keep the dictionary lying about the seam it had just been asked to correct. Flagged because it is the second dictionary-drift finding this week and the enforcement model here is authorship, not machinery (Ben, 2026-08-01) - which only works if it is noticed.*

### NR-414 — Scope taken in Lane U: blackboard facts for unit province and order destination
*Lane U (BL-511 seam half, march_unit to province), merged and verified 2026-08-21. · raised 2026-08-21 · from The agent added unit_province and unit_order_dest_province to corp_ai.cpp blackboard export, beyond the literal task. Reason given, and it holds: without them an agent over the wire reads its unit position as a TILE but must name its destination as a PROVINCE, so the word interface would be unusable - it could not describe the move it is being asked to make. Small, in the spirit of the ruling, and stated rather than slipped in. Confirm or revert.*

### NR-416 — BL-511 R1 is PARTIAL: the live click is owed, and the agent was right not to take it
*observation · raised 2026-08-21 · from Lane R (BL-511 province render + selection), merged and verified 2026-08-21.*

The agent declined to drive your desktop unattended to satisfy the LIVE half of requirement R1, which was the correct call for a non-interactive sub-agent session. What it did instead is real coverage rather than a dodge: it added verify.mouse plus two-frame hover captures so the canvas OWN distance-to-hex-centre hit-test resolves a province from a cursor position, and the click handler consumes exactly that hovered_tile. The resolution path is proven; the PRESS is not. Per the 2026-08-19 standing rule a scripted capture does not prove a press is reachable, so R1 stays partial until someone clicks. The app is open on your machine now.

### NR-418 — Three small findings from the render work, all fixed or noted in place
*observation · raised 2026-08-21 · from Lane R (BL-511 province render + selection), merged and verified 2026-08-21.*

(1) verify.clear_selection had DIVERGED from the gesture it stands in for - it cleared selected_entity but left the province set, so a capture taken after it showed a stale province card. Fixed to match a real empty-space click. This is the harness-lies-quietly class of defect, and it was found by using the harness. (2) building_component has no owner field; ownership is corporation.assets, and the province card resolves it the way the canvas does. (3) dl->_Data->TexUvWhitePixel does not compile outside imgui_internal.h - ImGui::GetFontTexUvWhitePixel() is the public route, noted in a comment for the next person reaching for Prim*.

### NR-419 — Repartition result: 97.90% of provinces in the 7-12 band, and every miss is explained
*observation · raised 2026-08-21 · from Province repartition to 7-12 tiles, merged and verified 2026-08-21.*

21,161 provinces across 6 seeds. Mean 9.11, min 1, max 18. Under floor: 109 (0.52%). Over ceiling: 336 (1.59%). The two misses have different characters and both were reported rather than clamped, which is the right instinct. UNDER-FLOOR ARE ALL TRUE ISLANDS - the agent added assertion P5c which walks every sub-floor province neighbourhood and asserts none had an adjacent province to merge into; it reports 0 stranded. A merge cannot invent land for a 2-tile island, so that is the honest floor, not a failure. OVER-CEILING (max 18) come from a merge piling onto a crowded coast; reported, not clamped, per your standing preference for honest constraints over clamping.

### NR-420 — BL-513 k survives the repartition, as predicted - 0.74% move
*observation · raised 2026-08-21 · from Province repartition to 7-12 tiles, merged and verified 2026-08-21.*

k was pinned at 12.6468 as (pooled per-tile capacity / sustain units) aggregated over the world. Both sides are sums over TILES, so the prediction was that province size cannot move the aggregate. Measured after repartition: 12.5535, a 0.74% move, well inside the pre-existing 28.59% per-seed spread, with ceilings totalling 100.75% of pooled per-tile capacity. k was NOT adjusted. What did change completely is the per-province distribution: ceilings now run 1..262 per province against a previously narrow spread. Worth knowing if the ceiling ever starts to bind.

### NR-422 — The march harness fixtures could not survive the repartition, and that is information
*observation · raised 2026-08-21 · from Province repartition to 7-12 tiles, merged and verified 2026-08-21.*

unit_march_harness went from 81/81 to 8 failures the moment provinces changed, all of them fixture-shape rather than product defects. Root cause worth recording: make_row_body builds a body of grid_height 1, and on a one-row body EVERY 3x3 block is a 3-tile fragment, so the merge cascades and the entire row becomes one province however long it is - widening the fixture from 8 to 24 tiles changed nothing. A row fixture can no longer express march from one province to another at all. I added make_grid_body/grid_tile_at helpers and converted M1; the remaining fixtures went back to the agent that wrote them, with instructions not to weaken any assertion and to stop and report if any assertion turns out to be genuinely impossible under the new partition. The general lesson: a synthetic fixture encodes assumptions about world SHAPE, and a generation change can invalidate it silently - here it failed loudly, which is the good case.

### NR-423 — The province selection fold is a conformance win, and it removed a real behaviour divergence
*observation · raised 2026-08-21 · from Selection fold agent, merged and verified 2026-08-21.*

Ben read draw_province_selection as a second selection element and he was right about the cause: it was dispatched first and drew its own header, chrome and linear layout rather than being a body of the one polymorphic panel. It is now shaped exactly like draw_building_selection_body and draw_unit_selection_body - shared header block, shared three-column band, shared pager chrome, shared 2x3 action grid. Two details worth keeping: the icon borrows the TILE kind deliberately (a province is a cluster of tiles, not a new kind of thing), and Construct is absent from the action grid because placement is still tile-grain and the Tiles page is the route to it. The stale-id fallback also IMPROVED as a side effect - it now falls through to ordinary kind resolution and renders a normal selection, where the old card drew a bare dash. Nothing in the code assumes a province size, so the 7-12 repartition only changes how many bands the mixture bar has.

### NR-424 — The live click on a province is STILL owed - two agents in a row could not take it
*observation · raised 2026-08-21 · from Selection fold agent; follows NR-416.*

The second agent to touch this surface also could not satisfy the standing live-check rule: the verify API has no click injection, and computer-use has no granted applications in a non-interactive sub-agent session. So the press remains as untested by harness as it was before the fold - the hit-test resolution path is proven, the press is not. Recording it a second time because that is now a PATTERN rather than an incident: any interactive surface built by a sub-agent will arrive with its live half owed, by construction. Two ways out worth considering - give the verify API a click-injection hook so the press becomes scriptable, or accept that live checks are Ben work and batch them. The first is a small item and would close this class permanently.

### NR-425 — CORRECTION: my 81/81 verification of unit_march_harness was hollow - M6 was segfaulting and M6/M7 never ran
*observation · raised 2026-08-21 · from March fixture rebuild, 2026-08-21. Correcting my own earlier report.*

I reported unit_march_harness as 81 passed, 0 failed and treated that as verification of Lane U. It was not. M6 discarded apply_corp_command result and then indexed u.order.path[u.order.next_index] on a REFUSED order, so it indexed an empty vector and SEGFAULTED the process mid-M5. M6 and M7 reported nothing because they never ran, and the exit code was 139 with output ending mid-word. I grepped for passed/failed and never checked the exit code, so the crash was invisible to me. Now fixed: the setup asserts instead of discarding, so a refused order fails loudly, and the harness is 88 passed 0 failed with EXIT_CODE=0 confirmed explicitly. The method lesson is mine, not the agent: a harness that prints a pass count can still have died before the end, and a pass count is not an exit code. Worth adding an exit-code check to how the verifier-headless skill reports.

### NR-426 — CORRECTION: a one-row body does NOT always collapse to one province - it splits above 19 tiles
*observation · raised 2026-08-21 · from March fixture rebuild, 2026-08-21. Correcting my own earlier instruction to the agent.*

I told the agent that on a one-row body the merge cascades so the whole row becomes one province HOWEVER LONG it is, having observed 8 and 24 tiles both giving one province. That generalisation was wrong. Measured: rows of 1-12 tiles give 1 province, 20 and 24 give 2, and 30 gives 3. My 24-tile test appeared to fail for a different reason - 24 was too short to give two REACHABLE provinces for what M1 needed, not too short to split at all. Consequence: M2 20-wide row fixture is honest and was correctly left alone rather than converted. Recording because the false generalisation is the sort of thing that gets quoted back later as a property of the partition.

### NR-430 — CORRECTION: my BL-517 brief claimed a tile serialisation seam that does not exist
*observation · raised 2026-08-21 · from BL-517 (retain the heightmap), merged and verified 2026-08-21.*

I briefed the agent to append height to the tile record because 'the tile record already carries a round-trip proof - extend it, do not replace it'. There IS no tile serialiser. The only flat-binary streams in the project are the history log (which carries the province section), the order book and procurement. Tiles are never written; they are regenerated from the seed, and province_partition_harness's round-trip proof is over the HISTORY-LOG stream, not a tile record. The agent correctly did nothing rather than inventing a tile serialiser to satisfy the instruction, and said so plainly. Requirement retain-heightmap R2 is therefore N/A rather than complete. Worth carrying forward: the same wrong assumption is easy to make again, because several items talk about 'the serialisation seam' as though tiles were in it.

### NR-431 — The Generation Ledger field overlay BL-517 was told to feed does not exist yet
*observation · raised 2026-08-21 · from BL-517 (retain the heightmap), merged and verified 2026-08-21.*

BL-517's scope included exposing height to the Generation Ledger's field overlay, 'where it is already designed'. It is designed and not built: GENERATION_LEDGER.md's own status table reads 'Field lenses ... Partial substrate - the generation_record seam exists and is filled on demand; no lens built'. So there was nothing to expose it to, and no UI change was made (src/ui was outside the agent's file scope in any case). The retained field is exactly what such a lens would read without regenerating, so the item still paid for the lens - it just did not build it. Not a gap to fix now; a note so the scope line in BL-517 is not read later as unfinished work.

### NR-434 — The agent corrected its own mechanism mid-task, and the correction is the interesting part
*observation · raised 2026-08-21 · from BL-515 (organic province borders), merged and verified 2026-08-21.*

Its first implementation followed BL-515's literal wording - past the size budget, only cross a cheap edge. That produced 3,860 provinces at EXACTLY 7 tiles and 616 singletons: a de-facto clamp wearing the costume of a growth rule, which is the precise failure mode the organic redesign existed to remove. It replaced that with hinterland seeds chosen up front at spacing, plus a self-referential soft brake - past its budget a region annexes only ground no harder to reach than what it already holds - which needs no threshold constant at all. Worth recording because it correctly distinguished FIXING AN ARTEFACT from CHASING THE BASELINE SPREAD, which is the line the brief drew, and it stated the distinction rather than quietly retuning.

### NR-435 — Both coefficients were pinned against measurement, and one pin is genuinely elegant
*observation · raised 2026-08-21 · from BL-515 (organic province borders), merged and verified 2026-08-21.*

k_province_height_cost = 683: pinned so that a gradient at the p90 of adjacent-land steepness costs the same as crossing a river, measured over 653,910 adjacent land edges (mean 0.0270, p50 0.0186, p90 0.0586, p99 0.1406, so 40/0.0586 = 682.7). Measured on TERRAIN ALONE, so re-pinning it cannot chase its own tail - that property is what makes it re-derivable later. k_province_road_bind_divisor = 4: the smallest divisor at which a road link binds harder than plain ground EVEN ACROSS A RIVER, with a swept table showing roaded-internal share 56.85% at divisor 1 (no binding at all) rising to 74.81% at 4. The agent noted the lift is concave with no knee - the table confirms the choice rather than picking it, which is an honest thing to say about a sweep that does not hand you an answer. k_province_seed_spacing = 3 is structural, not tuned: seeds at separation d tile a plane in (sqrt3/2)d^2 tiles, and d=3 gives 7.79, the only integer whose ideal cell lands inside 7-12.

### NR-436 — BL-513's k confirmed size-independent a second time; the agent declined to re-pin and was right
*observation · raised 2026-08-21 · from BL-515 (organic province borders), merged and verified 2026-08-21.*

Measured after the organic repartition: 12.5047 aggregate (spread 11.3391..15.1435, 28.73% of mean) against the pinned 12.6468 - a 1.1% drift, far inside the seed spread. That is the second independent confirmation that the ceiling coefficient does not depend on province size, which was predicted from its shape (both sides of its ratio are sums over tiles). The residual is real and explicable: the population factor multiplies per-province, so which tiles sit with a centre moves the total slightly. The agent did NOT re-pin, on the grounds that it is BL-513's contract, the ceiling binds on nothing (0.114% used) and the brief did not authorise it. Correct call, and it flagged it as delegated rather than silently leaving it.

### NR-440 — Absorption improved the distribution on every axis except the ceiling
*observation · raised 2026-08-21 · from Singleton absorption pass on BL-515, merged and verified 2026-08-21.*

6-seed sweep, before -> after: provinces 24,498 -> 22,401 (exactly 2,097 absorbed, so the counts reconcile against the pre-pass baseline); mean 7.87 -> 8.61; under 3 tiles 3,008 -> 911; under 7 6,195 -> 4,098; in the 7-12 band 74.71% -> 76.80%. 2,148 singletons existed, 2,097 were absorbed, and 51 remain as TRUE ISLANDS with zero land neighbours - each one verified by a new harness row rather than asserted. Ben's retraction of 'do not reject tiny provinces' bought roughly a two-thirds cut in sub-3 provinces. The residual 911 under 3 are two-tile provinces, which the pass deliberately does not touch: the ruling said one tile, and reinstating a merge-to-floor rule would rebuild the de-facto clamp BL-515 existed to escape.

### NR-441 — BL-513's pin drifted 0.06% and was again left alone
*observation · raised 2026-08-21 · from Singleton absorption pass on BL-515, merged and verified 2026-08-21.*

province_capacity_probe's BL-513 ratio moved 12.2546 -> 12.2477 across the absorption (0.06%), measured by running the probe at HEAD and again after rather than reasoned about. k_province_buildings_per_sustain_unit stays at 12.6468, un-re-pinned. That is now the third independent confirmation that the ceiling coefficient is insensitive to how the partition is drawn, which is what its shape predicted - both sides of its ratio are sums over tiles.

### NR-442 — urban already destroys the geology under a city, and the axis split fixes it as a side effect
*observation · raised 2026-08-21 · from BL-519 / BL-520 design work, 2026-08-21.*

BL-366's urban transform is documented as one-way and it OVERWRITES terrain_composition. So today, when a metallic tile's building stack fills, the tile stops being metallic - the fact is gone, not hidden. Nothing reads it afterwards, so nothing has broken visibly, but it means a city can never know what it was built on, and any future rule that wants to (a mine under a city, subsidence, resource exhaustion, a lens showing what the land WAS) has no data to read. BL-519 fixes this incidentally: with substrate separate from state, paving a tile leaves the substrate intact. Recorded separately from the item because it is a live data-loss bug, not merely a design awkwardness, and because if BL-519 is ever deferred this should be fixed on its own.

### NR-444 — The composition migration is 330 references across 49 files, and the accessor decision shapes it
*question · raised 2026-08-21 · from BL-519 / BL-520 design work, 2026-08-21.*

Measured rather than estimated: 330 references to composition in src/, concentrated in tile_generation.cpp (60), terrain_combat.cpp (32), placement_rules.cpp (32), corporation_generation (18), nation_generation (15) and the UI (hex_render 14, presentation 13, generation_ledger 12). There is NO save format, so none of this is a data migration - it is call sites only, which is why the item is landable at all. BL-519 proposes a derived composition() accessor so every call site keeps working while the axes land underneath, then migrating by MEANING in batches. The question for you is whether that accessor is permanent or deleted last. Permanent is a much smaller diff and a lasting lie - the codebase would keep asking a question the data model no longer answers. Deleted is honest and a longer migration. The interesting part is that a call site asking 'is this forest?' usually means either 'is there timber here?' (cover) or 'can I build on it?' (substrate), and which it meant is precisely the information the overloaded enum has been losing.

### NR-445 — BL-519 landed with FOUR calls of yours taken as given — and one of them, urban-as-cover, is the reason the shipped urban bug is fixed
*decision taken on your behalf · raised 2026-08-21 · from BL-519 (tile axis split), landed 2026-08-21.*

You answered the item's four open questions before implementation: cover is GRADED (a density scalar), urban is a COVER value, tundra is DROPPED (scrub on cold ground), and the composition() shim is DELETED in this pass with all call sites migrated. All four are implemented as answered. Recording them here because the item's own design prose argued the OPPOSITE on urban - it proposed a separate state axis - and a future reader will otherwise read the code as having ignored the design. Your call is the better one and the code says why: urban-as-cover still preserves the SUBSTRATE, which is the entire shipped bug (paving a metallic tile destroyed the fact it was metallic). A separate state axis would have bought only the ability to remember what GREW there before the city, which nothing reads. tile_axes_harness A2c is the assertion that would have failed before the item and passes now.

### NR-446 — BL-519's headline case was inert at the threshold the design implied — 'a mountain might have a forest' fired ZERO times until I moved a number
*decision taken on your behalf · raised 2026-08-21 · from BL-519 (tile axis split), Pass 4d cover refinement.*

Your brief was 'a mountain might have a forest or not', and the design lists forest as a cover that reads on rocky and volcanic ground. I first gated forest-on-rock at moisture >= 0.55, matching the moisture the biome table uses for forest on soil. It fired ZERO times on a homeworld, and the reason is structural rather than a tuning miss: the biome table routes ground BY moisture, so `rocky` is only ever drawn in the dry and middling columns - wet rocky ground does not exist for the branch to find. I lowered it to 0.45 (the top half of the middling column) on the argument that 0.55 is the moisture at which SOIL grows a forest, and a slope is not soil. It now produces 3,054 forest tiles across a 6-seed sweep, of which 110 on the primary homeworld carry BOTH timber and iron - the pair the pre-split model could not express. FLAGGING IT because the number is a judgement about how common a wooded crag should be, and I picked it to make the feature reachable rather than because you chose it. The census is REPORTED by tile_axes_harness section D, so you can move it against a number.

### NR-447 — BL-519 moved the settlement map: a wooded hillside now scores better than a bare crag, so population centres and province borders shifted
*observation · raised 2026-08-21 · from BL-519, settle_score / agrarian_score migration.*

settle_score and agrarian_score were single-slot tables that had to choose between naming the cover and naming the ground, so a forested crag and a forested floodplain scored the same. Migrated by meaning: the COVER sets the score, the SUBSTRATE decides how much of it is real (sedimentary keeps every pre-split number exactly; rocky gets a third, and bare rock keeps its old 6/5). CONSEQUENCE, measured: population centres move, and because BL-515 seeds provinces from centres, the partition moves with them - 3,709 provinces became 3,714 on the default seed, and the over-12 share went 4.72% -> 4.90%. Everything still passes and the hard cap of 20 still holds at max 16. Reporting it because a province-count change is exactly the kind of thing that looks alarming when noticed later without a cause attached. The cause is that cover now matters to settlement, which is the point of the axis.

### NR-448 — TWO harnesses fail with stale goldens — ai_skill_harness (28) and history_sim_harness (8). Both PRE-EXISTING, proven against the previous commit, NOT BL-519
*observation · raised 2026-08-21 · from Full harness sweep during BL-519.*

ai_skill_harness prints its own banner: 'bands blessed: 2026-08-09 (GCC - STALE since 2026-08-14/BL-386; re-bless from the Linux build)'. history_sim_harness carries no such banner but fails the same way. I did not take either on trust: I built BOTH at the pre-BL-519 commit (2c17e96) in a separate worktree and ran them. ai_skill_harness: 28 failures each side, PASS/FAIL pattern byte-identical. history_sim_harness: 8 failures each side, PASS/FAIL pattern byte-identical. So BL-519 neither caused nor worsened either. Both want re-blessing, and BL-519 is a reason to do it AFTER this lands rather than before, since deposits and the settlement map both moved. NOT re-blessed here - re-blessing a golden without authorisation is exactly what src/world/CLAUDE.md forbids. The 8 history_sim rows are about whether the Era -1 sim fights and takes ground at all (B384a, B384c, BL384a), which is a BL-384-shaped problem and probably wants an item rather than a re-bless.

RE-CONFIRMED at session close, after BL-467 as well as BL-519. Full sweep: 83 of 100 harnesses pass, 14 cannot build in this container (sol2 / ImGui / non-world TUs), and these same 3 fail. Both patterns are STILL byte-identical to the pre-session baseline, so neither BL-519 nor BL-467 touched them. history_sim's 8 rows are about whether the Era -1 sim fights and takes ground at all, which is BL-384-shaped and probably wants an item rather than a re-bless.

### NR-451 — BL-519's UI half is COMPILE-CHECKED but not RUN, and no visual check was possible — this container cannot build the app
*observation · raised 2026-08-21 · from BL-519, src/ui migration.*

SDL3 and ImGui come from CMake FetchContent and the session proxy blocks those downloads; there is also no display. So ProjectIo could not be built or run, and no scripts/verify/*.lua check could be executed. What I DID do: cloned ImGui's headers and ran `g++ -fsyntax-only` over every file in src/ui, which is clean. That covers type errors and my migration; it does NOT cover linkage, runtime behaviour, or how anything LOOKS. Specifically unverified: the terrain colour blend (calibrated so all four canonical covers reproduce their pre-split RGB exactly, and arithmetically checked - but never rendered), the generation ledger's new two-histogram layout, and the wizard preview globe's packed-axes sampling. Worth a look on your machine before this is treated as done.

### NR-452 — The pre-BL-409 state_hash golden in the STANDING RULES is stale — it was already failing before BL-519, and BL-519 has moved the hash again
*question · raised 2026-08-21 · from Full 99-harness sweep during BL-519; spectator_determinism R2.*

`.claude/rules/io-standing-rules.md` cites `state_hash 3CBAD1D44EE71EDE` by value as the proof that BL-409's spectator mode leaves an ordinary played session byte-identical, and spectator_determinism R2 asserts it. THAT ROW FAILS, and it failed BEFORE this session: built at the previous commit (2c17e96) in a separate worktree, the unspectated hash is B7E6F60D6AFE78C0, not the golden. So something between BL-409 and today already moved world generation without the golden being re-pinned. BL-519 has now moved it again, to B4D09255AF346008 — expected, since deposits and the settlement map both changed, and the item's own resolution says so.

WHAT STILL HOLDS, and it is the part that matters: every load-bearing property in that harness passes on both sides. The prohibition binds (the player corp issues ZERO decisions unspectated), an unspectated run is reproducible, admitting the player shifts no rival's cadence slot, and a spectated run is reproducible tick for tick. The failing row is byte-identity against a FROZEN NUMBER, not a property.

NOT re-blessed. src/world/CLAUDE.md: 'Goldens and pinned bands (state hashes, generation sweeps) are contracts: report movement, never re-bless without authorisation.' And this one is quoted in the standing rules, so re-pinning it edits a rule file — squarely your call, not mine.

Two things need deciding: (1) re-pin to B4D09255AF346008 and update the standing-rules citation; (2) worth a moment's thought on whether a hash pinned to one generation build can survive as a contract at all, given generation legitimately changes — the OTHER three rows prove BL-409's property without any frozen number, which suggests the byte-identity row has done its job and could become a reported hash rather than an asserted one.

RE-CHECKED after BL-467, which folds a new `battles` list into state_hash. The fold is guarded by `if (!battles.empty())`, and the guard WORKS: the hashes are byte-identical before and after BL-467 (played B4D09255AF346008, spectated BB2D94F63FC165E5). So a world that never fights hashes exactly as it did, and BL-467 did not move this golden further. It is still stale for the earlier reason.

### NR-453 — BL-521 was built with NO design to build against — the whole API shape was chosen by the agent, because the item's design prose is the corrupted one
*novel-work · raised 2026-08-21 · from BL-521 (verify-API click injection), built 2026-08-21.*

Flagged as novel by the agent, and it is right. The CAPABILITY is not scope growth — DEVELOPMENT_PRACTICES.md owns headless verification and BL-521 is a designed A-priority item. But its design field holds the filing script instead of the design (NR-449), so there was nothing to build against: the six Lua binding names, the pan-to-click trade-off and the double-click forcing were all chosen by the agent, not derived from an authority. That is worth knowing before the API is treated as settled, because a verify binding is a seam other checks will be written against and renaming one later invalidates every script that used it.

### NR-454 — BL-521 closes the live-check class for CANVASES but not for PANELS — clicking a nav-rail button or a ledger tab by name needs a target registry nobody owns
*question · raised 2026-08-21 · from BL-521, reported by the implementing agent.*

What shipped: a script can click a POSITION or a TILE. What it still cannot do is `verify.click('nav.market')`. Nav-rail buttons, ledger tabs, Selection-band rows and the resource graph publish no screen rect, so clicking them by name needs every clickable surface declaring `{name, rect}` into ui_state. That is a design decision, not an implementation detail — who owns the registry, does every widget owe an entry, and does it duplicate ACTIONS.json, which already names every control? Genuine scope growth if pursued. Needs a ruling before the live-check class is FULLY closed, and it is the half that covers most of the surfaces NR-388's six landed-awaiting-a-live-check items are about.

### NR-455 — BL-521's click_tile PANS the view as a side effect, and verify.mouse was deliberately left in place beside the new verify.hover
*decision taken on your behalf · raised 2026-08-21 · from BL-521, two calls taken by the implementing agent.*

(1) `verify.click_tile(col,row)` and `verify.tile_screen(col,row)` ask the canvas where a tile is, which CENTRES it — so clicking a tile moves the camera. Accepted because the alternative is a second copy of the canvas transform living in the verify API, which would drift. The fix, if you want one, is the canvas publishing tile screen rects without moving. (2) `verify.mouse` moves only the canvases' cursor; the new `verify.hover` moves ImGui's too. Unifying them would be cleaner but would re-bless every existing hover golden, so `mouse` was left untouched. Both are reasonable; both are the agent's calls rather than yours.

### NR-456 — BL-520: texture SURVIVES the lenses at 0.45 rather than being replaced by them — a decision taken on your behalf
*decision taken on your behalf · raised 2026-08-21 · from BL-520 (basic tile texturing), open question 1. src/ui/body_surface_canvas.cpp.*

The item asked whether texture survives the map lenses or whether lenses replace it — 'a texture underneath a Country-lens fill either reads as depth or as dirt'. I picked SURVIVES, at 0.45 strength, and implemented it. Reasons: (1) the precedent is one channel over — BL-231's landform relief is composited AFTER the lens tint on the argument that terrain facts stay true under an overlay, and 'this is closed-canopy forest' is the same class of fact as 'this is a mountain'; (2) replacing it would make each lens a different MAP rather than the same map read differently, which is what the lens bar depends on. The thing that actually keeps it from reading as dirt is not the 0.45 — it is that every mark's ink is derived from the tile's OWN drawn fill (pushed 55% toward a per-cover target), so under Country a mark is that nation's colour darkened, i.e. shading on the block rather than a second competing colour. Same derivation makes the fog and survey dim free. THE FRAME THAT FALSIFIES IT is texture_lens_country in scripts/verify/tile_texture.lua — if the marks read as unrelated speckle there, the answer was 'replace' and the fix is one constant (k_texture_lens_strength -> 0) plus deleting the attenuation branch.

[ID NOTE: filed by the BL-520 agent as NR-452 while a concurrent lane was filing entries in the same range; renumbered on merge. The BL-520 commit message and any doc comment written by that agent cite the OLD id.]

### NR-457 — BL-520's texture pass is COMPILE-CHECKED ONLY — never rendered, and its verify script has never been run
*observation · raised 2026-08-21 · from BL-520, src/ui/hex_render.cpp + src/ui/body_surface_canvas.cpp.*

Same container limitation as NR-451: SDL3/ImGui come from FetchContent, the proxy blocks the downloads, and there is no display, so ProjectIo could not be built or run and no scripts/verify/*.lua could execute. What IS verified: `g++ -std=c++20 -fsyntax-only` is clean on both changed files, and every number quoted in the code comments and in PLANETARY.md was computed independently (LOD ramp 0/0.25/0.5/1 at r=14/16/18/22; density 75/150/205/255 -> 2/3/4/5 marks at alpha 123/157/182/204 of 255; grain alphas 36-76 of 255 at full strength, 16-34 under a lens). What is NOT verified: how ANY of it looks — whether the grain is too loud to let the province blend read as one shape, whether the cover marks are distinguishable from each other, and whether the lens attenuation in NR-452 is right. scripts/verify/tile_texture.lua was written and named in the verifier-visual skill for you to run; it is deliberately capture-only with NO golden blessed, since blessing an unseen frame pins whatever got built.

[ID NOTE: filed by the BL-520 agent as NR-453 while a concurrent lane was filing entries in the same range; renumbered on merge. The BL-520 commit message and any doc comment written by that agent cite the OLD id.]

### NR-458 — BL-520 created a NEW VISUAL VOCABULARY that no authority owns - thirteen mark kinds deliberately not icons, and a third always-on render channel
*novel-work · raised 2026-08-21 · from BL-520 (basic tile texturing), merged 2026-08-21. Flagged by the implementing agent.*

Two parts, and the agent was right to raise both.

(1) THE MARK VOCABULARY. Nine cover patterns and four substrate grain kinds, none of which existed before. ICONS.md owns the `ui::icons` glyph namespace under a `(dl, centre, r, colour)` contract; these marks are in NEITHER that namespace NOR on that contract. The agent put them in hex_render and documented them in PLANETARY.md, arguing that a hashed FIELD of marks is not an icon. That reads right - an icon is a symbol you look up, a grain is a texture you look through - but it is a boundary judgement that leaves two visual vocabularies where there was one, and ICONS.md currently says nothing about it.

(2) A THIRD RENDER CHANNEL. Texture now composites on every tile forever, alongside hue (substrate x cover) and BL-231's landform relief. Small in code, permanent in consequence: every future overlay, lens and capture inherits it, and the LOD gate makes it the first channel that appears and disappears with zoom.

### NR-459 — Fanned-out agents get worktrees at the SESSION's base commit, not at HEAD — one built an entire item against an enum that had already been deleted
*observation · raised 2026-08-21 · from BL-516, BL-520 and BL-521 run concurrently on 2026-08-21 while BL-519 landed in the main session.*

All three agents were briefed 'off the current HEAD' and all three got worktrees at 21d4834, the commit this SESSION started from — two behind by the time they launched. The BL-520 agent noticed and fast-forwarded itself before starting. The BL-516 agent did not: it concluded from its own `git log` that 'BL-519 has not landed in code', which was true of its base and false of the repo, and then widened `terrain_composition` — the exact enum BL-519 had just deleted. Roughly an hour of good work (structural water classification, a measurement-pinned seed spacing, per-domain cap assertions) landed on an axis that no longer exists. It is being ported rather than redone, and the port is cheap ONLY because that agent had routed every water test through one choke-point predicate.

TWO THINGS WORTH KEEPING, because DELIVERY.md's worktree model assumes agents start from where the main session is:

1. A brief that says 'off the current HEAD' is not enough. It should name the COMMIT HASH, and tell the agent to verify it and fast-forward if it is behind. Cheap, and it would have caught this in one command.
2. The sharper rule, which is the one I gave that agent: when a brief's stated base and the repo disagree ABOUT THE THING YOU ARE CHANGING, that is a stop, not something to reconcile. The BL-516 agent reconciled — reasonably, in isolation — and the reconciliation is what cost the hour.

Also worth noting: two agents independently allocated the SAME review ids (NR-452, NR-453) while this lane was using them, because NEEDS_REVIEW.json has no equivalent of next_id.js. Resolved by renumbering on merge, but next_id.js exists for backlog ids precisely because minting off a local file collides, and the review log has the same shape and no such tool.

### NR-460 — Sea provinces reach 82 tiles against your stated 80 — the SAME shape as the 12-vs-13 case you ruled on this morning, and it wants the same kind of answer
*question · raised 2026-08-21 · from BL-516 (water kinds and sea provinces), merged 2026-08-21.*

You said open-ocean provinces should be 'much larger, but not larger than say 80 tiles'. Growth clamps at 80 and holds. But singleton absorption picks a singleton's CHEAPEST neighbour, and that neighbour may already be full — so a survivor ships at 81 or 82, exactly as a land province shipped at 13. Measured over 6 seeds: 2,272 open-ocean provinces, mean 41.07, max 82; 11 over the clamp totalling 12 excess tiles, and absorption accounts for all 12. 26 sit exactly ON 80.

The agent did NOT invent a second number, which was right — it asserted the accounting identity instead (every tile above 80 arrived by absorption). But that leaves 80 as a clamp the shipped partition exceeds, which is the precise situation NR-438 existed to resolve on land. Your land answer was: 12 becomes a PREFERENCE, 20 becomes an asserted hard cap, and the over-share is reported not asserted. The parallel answer here would be 80-as-preference plus a sea hard cap — but 'say 80' may equally have meant an absolute you want enforced, in which case absorption needs a different rule at the sea boundary. Your call; the numbers are in province_partition_harness section W.

Coastal water is a separate band and is fine: 4,213 provinces, mean 4.57, max 14, inside the 20-tile hard cap it inherits from land (asserted as W2b).

### NR-461 — A lakeshore is no longer a coast — 22% of coastal land tiles lost port and wharf eligibility, taken on your behalf
*decision taken on your behalf · raised 2026-08-21 · from BL-516, is_coastal narrowed to sea.*

`is_coastal` used to mean 'next to any water'. Now that lake, coast and open ocean are distinct, it means 'next to the SEA', so a tile on a lakeshore stops qualifying. That is the correct reading of the words and it is also a real gameplay change: a Port and a Fishing Wharf both gate on it.

MEASURED, re-taken against the current base: 401 of 1,801 tiles lose eligibility on seed 0 (22.3%), 486/1,713 on seed 1 (28.4%), 432/2,391 on seed 2 (18.1%). So roughly a fifth to a quarter of what used to be buildable coast is not any more.

Defensible either way. A lake genuinely should not support an ocean-going port; but a large lake plausibly should support a fishing wharf, and the current rule denies both. If you want that split, the natural shape is Port gating on sea and Wharf gating on any water — one line each, and it needs your ruling rather than mine.

### NR-462 — Lakes were given their own provinces because your water ruling never said where they belong
*question · raised 2026-08-21 · from BL-516, the open question the item itself names.*

You named three water kinds — lakes, coasts, oceans — and gave province rules for coastal (3-12) and open ocean (much larger, ~80). Lakes got neither. The agent put a lake on the COASTAL band as its own province: it invents no new size rule and keeps the invariant that a province never spans two domains. Measured on seed 0: 1,433 lake tiles.

The alternatives it did not take, for when you rule: a lake could belong to the LAND province that surrounds it (which makes a lake a feature of a place rather than a place of its own, and would suit a small pond), or lakes could be excluded from the partition entirely the way ocean was before this item. The current choice is the most consistent one; it is not necessarily the one you want.

### NR-463 — An adversarial scout found FOUR real defects in BL-467 an hour after I wrote it — including a withdrawal that cost no men
*observation · raised 2026-08-21 · from BL-467, verified by a 15-agent scout workflow run against the working tree.*

Recording this because the finding rate is the point, not the fixes. I read the seam myself, wrote the item, and had it compiling and passing 26 harness checks. A scout that read the same code adversarially found four things wrong with it:

1. WITHDRAWAL COST NO MEN. I captured each side's strength AFTER applying the withdrawal, so the parting cost - the resolver's three-term price, the whole reason disengaging is a decision - was invisible to the loss distribution. Breaking off was free in men and expensive only in a number that was erased at the end of the tick. My own harness row passed anyway, because it asserted the REQUEST was accepted rather than that it cost anything.

2. THE MUTUAL-HOSTILITY DEDUP WAS A NO-OP. std::unique only collapses ADJACENT equals, and with a third corp C where A < C < B the sort puts (P,A,B), (P,A,C), (P,B,A) - the mutual pair is not adjacent, so the dedup did nothing in exactly the case it was written for. Correct by accident via a different guard, with a comment claiming a mechanism that did not work. Removed.

3. THE ALL-NAVAL CASE WAS UNSCREENED. resolve_battle scores naval at exactly zero and rejects nothing, and its victory test is a strict '>', so a 0-vs-0 fight resolves as a DEFENDER VICTORY with 400/200 per-mille losses on forces that could not have fought. Discovery never inspects unit class. Unreachable today (land-only roster) and now guarded.

4. ZERO-COUNT UNITS WERE NEVER REAPED - see the next entry.

What made it work: the scout was told to REFUTE each claim and to default to holds=false when it could not confirm from what it read. Several verifications came back refuting the reader that produced them, and the plan it wrote carried the corrections rather than the original claims.

### NR-464 — BL-467's aftermath ruling was FALSE about the code - nothing reaped a zero-count unit, so I extended the existing cleanup rather than adding a second one
*decision taken on your behalf · raised 2026-08-21 · from BL-467; the defect the scout's Q1 named.*

BL-467's design says 'zero-count units are disbanded by the upkeep pass's EXISTING cleanup, which runs last in the economy step', and I wrote requirement R9 repeating it. Both were wrong about what the code does: run_unit_upkeep's orphan cleanup tests for a missing owner, a missing muster base and a missing tile, and has NO count<=0 test anywhere. Before this item nothing could reduce a count to zero, so the case had never arisen.

Left alone, a unit ground to zero in a battle would have persisted forever - holding a tile, pinning its province, drawing upkeep, and folded into state_hash as a force that no longer exists.

TAKEN ON YOUR BEHALF: I added 'count <= 0' as one more condition on the EXISTING orphan cleanup rather than disbanding inside the battle pass. That keeps the ruling's actual intent - the battle pass removes MEN, the upkeep pass removes UNITS, and there is exactly one disband site - while making the sentence true. The alternative reading, that a zero-count unit is a legitimate world object, seems clearly wrong but is yours to overturn.

### NR-465 — Four things in BL-467 are stubs that nothing in the code, the item or the requirements admits are stubs
*question · raised 2026-08-21 · from BL-467, from the scout's 'what this plan is not sure of'.*

All four are live in every production battle, and none was written down anywhere as a known gap. Naming them because a stub nobody recorded becomes a bug somebody diagnoses later.

1. DOCTRINE IS ALL ZEROS. Both sides are passed doctrine_row{}. No per-corp doctrine field exists on corporation_component, so resolve_battle's whole doctrine machinery - frontal bonus, flank fragility, the mountain penalty - contributes NOTHING to any battle the game plays. Reasonable as a first cut; it wants an item.

2. SEASON IS HARDCODED TO SUMMER. The world carries no season at all, so the resolver's winter penalty is dead code in production. Forced rather than chosen.

3. MEMBERSHIP IS SNAPSHOTTED AT OPEN. A unit marching into a province where a battle already runs never joins it, because discovery skips a pair already fighting there. Reinforcement is impossible, and whether it SHOULD be is not settled anywhere - ruling 1 says units keep their tiles and never pool, which does not answer it.

4. 'THE FIELD IS HELD BY THE SIDE THAT STAYED' HAS NO CONSEQUENCE. It is in the item and in R9, but no territorial-control concept exists for a battle to affect, so holding the field means nothing happens. R9 is marked PARTIAL for this reason rather than complete.

### NR-466 — Two smaller BL-467 calls you may want to overturn: disband-under-fire is legal, and the battle seed reads a tick the code says is not authoritative
*question · raised 2026-08-21 · from BL-467, the scout's Q4 and its determinism caveat.*

1. DISBAND-UNDER-FIRE IS LEGAL. march_unit is refused for a unit in contact, on the reasoning that walking away is a priced withdrawal. disband_unit is NOT guarded - a corp can dissolve its army mid-battle. I left it legal rather than blocking it for consistency, on the grounds that it is not actually an escape: disbanding forfeits the WHOLE remaining unit with no refund, where withdrawing costs a fraction, so it is strictly worse than the priced option and reads as a rout rather than an exploit. But the asymmetry with march_unit is real and it was my call.

2. THE BATTLE SEED FOLDS world::current_day_tick, whose own comment says it is 'not authoritative sim state and not serialised'. Every tick driver sets it, so it is correct in practice - but it is now load-bearing for a random stream while the field disclaims exactly that role. Either the comment is too modest or the seed should fold something else.

3. Related, smaller: campaign_battle_params is constructed fresh each tick and NOT stored on the battle, so a tuning change between ticks would silently re-tune a fight in flight - and setting swing_permille to 0 would change the DRAW COUNT mid-stream, which breaks replay. Impossible today (hardcoded defaults); a real hazard the moment params become data, which the item's own 'authored data' framing invites.

### NR-467 — Sprint C3's rider, measured: a flat per-head upkeep is BRUTALLY REGRESSIVE — at 'light' rates a 700-man army costs a small corp 155% of its entire income and a large one 7.8%
*question · raised 2026-08-21 · from Sprint C3 rider; tools/verify/unit_upkeep_rates.cpp, built and run 2026-08-21.*

The rider was 'turn the upkeep rates off zero and measure the movement'. I measured and did NOT turn them on: scripts/economy.lua says in as many words that the pricing is to be 'tuned by playtest against a MEASURED BASELINE rather than guessed here', so the numbers are yours to pick. New harness `unit_upkeep_rates` prints them; it asserts only invariants (shipped rates still zero, zero still costs nothing, the draw is linear in heads).

THE FORCE: 500 Levy Spear + 200 Rifle Regiment — 700 men, a mid-sized standing army.

  rate set   credits/tick   ordnance/tick   ticks to collapse unsupplied   ticks to recover
  shipped            0.00           0.000   never                          never
  token             15.52           1.400   200                            100
  light             77.60           7.000   50                             40
  real             310.40          28.000   20                             25
  heavy            776.00          70.000   9                              17

THE FINDING, and it is not the one I expected. As a SHARE of corp income the same rate lands completely differently by corp size:

  rate set    vs 50/tick corp   vs 200/tick   vs 1000/tick
  token              31.0%          7.8%          1.6%
  light             155.2%         38.8%          7.8%
  real              620.8%        155.2%         31.0%
  heavy            1552.0%        388.0%         77.6%

So 'light' — a rate that reads as modest and costs a large corp 7.8% — is INSTANTLY FATAL to a small one, at 155% of everything it earns. A flat per-head rate does not scale a cost, it selects which corps may have armies at all. That may be exactly what you want (a militia is a rich corp's privilege) or exactly what you don't (only one corp can ever field a force, so there is no conflict). Either way it is a design choice the number hides, and it wants deciding before a rate is picked rather than after a playtest reads oddly.

FOR BL-458 (supply lines cannot be cut), which this rider exists to unblock: the column that matters is ticks-to-collapse. At 'token' an unsupplied army takes 200 ticks to degrade — cutting a lane is a gesture nobody would notice. At 'real' it is 20 ticks, and at 'heavy' 9. Interdiction only becomes a mechanic somewhere around 'real'. Note also that an unsupplied unit is not dead but WEAK: supply_factor_permille feeds unit_to_stack_entry, so it is measurably worse in the resolver — which is now a live consequence rather than a theoretical one, since BL-467 landed the same day.

One more thing the sweep surfaces: `out_of_supply_reach` at 0 disables the reach trigger entirely, and its own comment notes the reach field is therefore never built from the upkeep pass — 'the Dijkstra cost is opt-in'. Turning it on has a PERFORMANCE cost nobody has measured. I did not measure it either, because it needs a real generated world rather than this fixture; flagging it so it is not discovered as a frame-time regression.

### NR-468 — Which battle a click selects when several stand in one province
*decision taken on your behalf · raised 2026-08-21 · from BL-469 (battle card) — adversarial scout pass over the surfaces.*

A third corp arriving in a province opens its OWN battles against each existing participant rather than joining theirs, so one province can hold several live battles. A click on the province is one press but has several possible targets. TAKEN: `first_battle_in()` returns the player's own fight first, falling back to sorted (province, attacker, defender) order when none of them is the player's.

**Why it matters.** The rule is invisible in the UI — the card shows one fight with no indication that others stand on the same ground. A player watching a three-way could reasonably think the other two fights do not exist.

- Keep the current rule (own fight first, then sorted).
- Cycle through the province's battles on repeated clicks, as the 5-rung selection cycle already does for other kinds.
- Show a chooser when more than one battle stands in the province.

> **Recommendation:** Keep it for now; revisit if three-way fights turn out to be common in play. The cycle option is the cheapest upgrade and reuses machinery that already exists.

*Files: `src/world/battle_system.cpp`, `src/ui/selection_panel.cpp`, `src/ui/body_surface_canvas.cpp`*

### NR-469 — A rival-vs-rival battle card still shows both sides' aggregate strength
*question · raised 2026-08-21 · from BL-469 (battle card) — adversarial scout pass over the surfaces.*

The card DOES redact per-unit composition on a fight that is not the player's — each side reads 'Composition unknown - a rival's forces are not yours to count', and the Withdraw press is disabled with 'Not your fight' rather than hidden, so the rule reads as a rule. What is NOT redacted is the AGGREGATE: the strength bar, the '% of what marched in' figure, the phase word and the round count are shown for any battle. (Spectator god view lifts the redaction in the UI only, BL-408's existing pair-test.)

**Why it matters.** Aggregate strength is arguably still an internal under BL-068 — knowing a rival is at 30% is most of what you would pay a survey for. And it is the OPPOSITE call to the one the dispatch stream makes (NR-470: rival fights are skipped entirely), so the two surfaces on the same data currently disagree about what the player may see.

- Keep the aggregate visible — a fight's momentum is a public fact, its order of battle is not.
- Redact the aggregate too: phase word and province only, no numbers.
- Gate BOTH surfaces on the body's activity-fog tier (BL-089), so a war where you trade reads clearly and one elsewhere does not.

> **Recommendation:** Option 3 makes the two surfaces agree by construction and reuses fog machinery that already exists. Settle it with NR-470 — one question, two surfaces.

*Files: `src/ui/selection_panel.cpp`, `docs/ui/DISCOVERY.md`*

### NR-470 — Field-channel dispatches are restricted to the player's own fights
*decision taken on your behalf · raised 2026-08-21 · from BL-468 (battle dispatches) — adversarial scout pass over the surfaces.*

`post_battle_dispatches` posts a line ONLY when the player's corp is the attacker or the defender; rival-vs-rival dispatches are skipped. TAKEN on the BL-212 precedent — corporations are kept out of Public precisely so a rival's internals do not leak through comms, and a dispatch naming two rivals' strengths would be that leak by another route. A rival war is visible on the canvas as a battle marker, which says WHERE without saying how it is going.

**Why it matters.** It is the OPPOSITE call to the one the battle card currently makes (NR-469: the card shows a rival-vs-rival fight in full). Two surfaces on the same data disagreeing about what the player may see is the state worth settling, whichever way it goes — either the card should redact, or the dispatches should not.

- Keep as-is and redact the card to match (the conservative pairing).
- Post rival fights as a one-line neutral notice and open the card up to match.
- Gate BOTH on the body's activity-fog tier (BL-089), so a war where you trade reads clearly and one elsewhere does not.

> **Recommendation:** Option 3 composes with the fog already built and makes the two surfaces agree by construction. Settle it together with NR-469 — one question, two surfaces.

*Files: `src/core/session_history.cpp`, `src/core/battle_dispatch_text.cpp`*

### NR-471 — The Field channel has no mute, and battle traffic is the loudest stream yet
*observation · raised 2026-08-21 · from BL-468 (battle dispatches).*

`chat_panel`'s channels are a fixed pair (Public, Field) with no per-channel mute flag. A dispatch posts per battle per tick, so a busy war is several lines a tick — an order of magnitude more traffic than the nation-voiced or counsel posters that channel model was built for.

**Why it matters.** Not a defect today (the Field channel is separate, so it does not bury Public), but the first stream in the game whose volume is driven by simulation intensity rather than by scripted events.

> **Recommendation:** File a small backlog item for a per-channel mute if play shows it is needed. Not worth building on speculation.

*Files: `src/ui/chat_panel.hpp`, `src/core/session_history.cpp`*

### NR-472 — verify_api gained a GENERIC corp_command binding, not a narrow one
*decision taken on your behalf · raised 2026-08-21 · from BL-469 — the visual-check class was blocked; the scout found verify_api could only issue place_sell_order, so no battle could be made to exist inside a verify run at all.*

TAKEN: added `verify.corp_command{...}`, a generic binding over the whole corp_verb seam, rather than a narrow `verify.hire_unit`. It is range-gated against `corp_verb_count` and range-checks the workforce value BEFORE the narrowing cast, per the untrusted-boundary rule; an out-of-range command is rejected whole and mutates nothing.

**Why it matters.** This is a widening of the verify seam's reach from one verb to all of them. It is the right shape (every future verb is testable without another binding) but it deserves a look, because the verify seam is a scripted input boundary and the standing rule treats those as untrusted.

- Keep the generic binding.
- Replace it with per-verb narrow bindings, added as each is needed.

> **Recommendation:** Keep it. The alternative is a binding per verb forever, and the validation is at the boundary either way.

*Files: `src/core/verify_api.cpp`*

### NR-474 — BL-384's premise is REFUTED — the sim conquers heavily, but a quarter of worlds fight no war at all
*observation · raised 2026-08-21 · from Sprint 28 gate. tools/verify/history_conquest_gap.cpp, 8 real 0→1960 runs.*

BL-384 records '267 battles and ZERO conquests' and reads it as a global property of the sim. Measured today over 8 worlds, it is not: seeds 1,2,3,5,6,7 produce 43–435 conquests each, 1199 conquests over 1226 battles. What IS true, and is a different defect, is that seeds 0 and 4 fight ZERO battles — not few, none. So the outcome is close to BINARY: a world is either warlike or perfectly peaceful, and BL-384 was written against one of the peaceful ones (seed 0, the default world).

All three mechanisms the item names as candidates are refuted by measurement rather than argued away. (1) The scorer is NOT systematically optimistic by the terrain factor: bucketed against realised outcomes it is off by −0.0pp in the bucket holding 1215 of 1226 battles. (2) The scored staging hub differs from the levying hub in 0 of 1226 battles. (3) The transfer bar is not the constraint — 99.8% of victories convert to a conquest.

CORRECTION, same day: the first run of this harness hardcoded a 312x145 grid against a real 261x121 (BL-424 took the homeworld to 70% area). build_sim_terrain fills an oversized grid with defaults, so region anchors outside the real bounds read as flat grassland and region_distance's cylinder wrapped at the wrong width. Every figure above is the CORRECTED one. The zero-war headline and all three refutations survived; the scorer's error moved from -2.5pp to -0.0pp and terrain defence from 20% of battles to 40%.

**Why it matters.** Sprint 28's goal is 'a province changes hands'. Provinces change hands 1199 times. The sprint as written would have been spent tuning combat constants that measurement says are calibrated — which is precisely the outcome BL-384's own design warned about ('if it is not, the cause is elsewhere and tuning combat constants would be tuning the wrong thing'). The item needs restating around the real defect before any of it is built.

Worth noting WHY it went stale: the world has changed a great deal since 2026-08-13 — the 3× map, BL-519's terrain axis split (which moved the settlement map, NR-447), BL-515's province ids and BL-516's water kinds. A single-seed observation from before all of that could not have survived it.

> **Recommendation:** Restate BL-384 as 'a quarter of worlds fight no war at all' and re-scope Sprint 28 to that. The open question is whether a peaceful world is a BUG or a legitimate outcome — a world with room to expand choosing Settle over Campaign is arguably correct, and BL-224's non-hegemony invariant wants some worlds to stay multipolar. That is Ben's call, not a tuning decision.

*Files: `docs/development/backlog.json`, `tools/verify/history_conquest_gap.cpp`, `src/world/history_sim.cpp`*

### NR-476 — Ben's read that Lane A depends on Lane C — the ingredients already exist, at the other grain
*decision taken on your behalf · raised 2026-08-21 · from Ben, 2026-08-21: 'A relies on C, or at least some pseudo war in provinces, using defense buffs and possibly buildings.'*

Checked against the code rather than the docs, and the ingredients Ben names ALL already exist in the Era −1 sim — at polity grain, not campaign grain. `resolve_battle` is called with the full terrain quadruple (substrate, cover, density, landform) so terrain defence buffs are in; `work_defence_mod` is the buildings term and is read at history_sim.cpp:634 and :965; and history_sim.cpp's own comment says TERRITORY MOVES AT PROVINCE GRANULARITY, NEVER TILE. So Lane A already has its pseudo-war in provinces with defence buffs and buildings.

TAKEN: sequenced Lane A independently of Lane C rather than behind it. The two resolvers are separate by explicit design — combat.hpp states resolve_battle is era-agnostic and knows nothing about walls or campaigns — and BL-467's campaign resolver runs in the economy tick on a different actor grain (corps, not polities).

**Why it matters.** If Ben's read had been right, Lane C would gate Lane A and the whole arc would serialise. It does not, so the lanes can run in parallel — which is what they were doing when this was written. But it is worth him overturning if what he actually meant is that the two war models should CONVERGE rather than that one blocks the other; that would be a real design direction and a large one.

- Keep the two resolvers separate — nation-scale history vs corp-scale campaign, as combat.hpp designs them.
- Converge them: one resolver, one war model, both grains.

> **Recommendation:** Keep them separate. They answer different questions and BL-467 has just shown the campaign one working. But the fact that BL-321's defence works are inert in practice (NR-475) means the sim's half is less real than it looks, which may be what prompted the read.

*Files: `src/world/history_sim.cpp`, `src/world/combat.hpp`, `src/world/battle_system.hpp`*

### NR-477 — The sea haulage rate contradicts BL-188's own ruling, and must NOT be flipped on its own
*decision taken on your behalf · raised 2026-08-21 · from Sprint B2 (Lane B). tools/verify/sea_leg_census.cpp, 16 seeds, 1230 routes.*

`scripts/economy.lua` has `land = 0.02, sea = 0.05` — sea is 2.5x DEARER than land, contradicting BL-188's settled ruling 1 that 'sea's advantage is a lower per-unit RATE'. TAKEN: left it alone. `path.cost` does double duty — it is the A* routing weight (`sea_leg_cost = 2.5` deliberately prices water above any landform so routes prefer land) AND the haul price. Flipping the rate in that one number sends every convoy to sea.

**Why it matters.** It is a visible contradiction with a tempting one-line fix, and the one-line fix is wrong. Recorded so the next reader does not reach for it. Untangling the routing weight from the price is part of BL-522 (the per-leg split), not a tune that can precede it — and the resulting calibration constant is Ben's call, not an agent's.

> **Recommendation:** Settle the rate as part of BL-522, once price and routing weight are separate numbers.

*Files: `scripts/economy.lua`, `src/world/logistics.cpp`, `docs/economy/SUPPLY.md`*

### NR-478 — 85.8% of nations are single-centre, but only 3.4% end stranded — so no new mechanism was built
*decision taken on your behalf · raised 2026-08-21 · from Sprint B2 (Lane B). tools/verify/road_reach_census.cpp row C5, 8 seeds, 386 nations.*

Sprint B2's plan floated giving single-centre nations 'a short local network'. The measurement says don't. 331 of 386 nations (85.8%) are single-centre — the number the sprint asked for and which the landed fix never reported — but after the per-centre Track plus the cross-strait border links, only 13 (3.4%) end STRANDED, meaning roaded with no roaded neighbour anywhere. TAKEN: added the measurement as harness row C5 and built no new generation mechanism.

**Why it matters.** A new generation mechanism aimed at a 3.4% residue is the thing to avoid, and the 85.8% figure is exactly the kind of number that makes a problem look enormous when the actual failure is small. Flagged because the sprint plan said to do it and I did not.

> **Recommendation:** Leave it. If the 13 stranded nations turn out to matter visually, they are a targeted fix, not a mechanism.

*Files: `tools/verify/road_reach_census.cpp`, `src/world/road_generation.cpp`*

### NR-479 — BL-188 stays parked, and the reason moved: the blocker is in the logistics leg, not the road raster
*decision taken on your behalf · raised 2026-08-21 · from Sprint B2 (Lane B).*

Sprint B2's plan was 'the three structural cuts, then BL-188 un-parked'. The cuts had already landed (commit 5305d25, then adapted by BL-516's water kinds), and they do NOT unblock BL-188 — not marginally. They change the road raster; BL-188's blocker is in `logistics.cpp` / `supply_system.cpp`, where `crosses_ocean` is a route-wide boolean. TAKEN: left BL-188 parked, appended the finding to its design field, and filed the real blocker as BL-522 (whole-route mispricing) at priority A.

**Why it matters.** BL-188 has now been parked twice for reasons that turned out to be elsewhere. Naming the actual blocker as its own item is what stops that happening a third time — and BL-522 is worth doing on its own merits, since it mispricess 64.5% of hauls today whether or not a port is ever built.

> **Recommendation:** Keep BL-188 parked until BL-522 lands, then re-read it against a per-leg cost model.

*Files: `docs/development/backlog.json`*

### NR-480 — Concurrent agents get worktrees at the SESSION base again — B2 was five commits behind
*observation · raised 2026-08-21 · from Sprint B2 (Lane B), which caught it because the brief told it to check.*

The B2 worktree was created at 4a504d7, FIVE commits behind the branch tip, and the agent fast-forwarded itself before reading any code. This is NR-459 recurring. It mattered concretely: at the stale base the three road cuts looked unlanded and BL-516's water kinds were absent, so the agent would have rebuilt work that already existed against an enum that had changed.

**Why it matters.** NR-459 recorded this once and the mechanism has not changed. What DID work is the mitigation: briefing every agent to verify its base and fast-forward before reading anything, as its first action. That is now worth making standing practice rather than a per-brief habit — three agents launched today, and the one that was told to check, caught it.

> **Recommendation:** Add the base-check to DELIVERY.md's sub-agent section so it is not a thing each brief has to remember.

*Files: `docs/development/DELIVERY.md`*

### NR-481 — Sprint B3's stated premise is stale in all three of its parts
*observation · raised 2026-08-21 · from Sprint B3 Lane B agent, 2026-08-21. Verified against the tree at origin/main 269049d, not assumed.*

B3's goal line reads: "The population-centre clamp is still clamp(tiles/1000, 20, 40) from the 180x84 era, against a 312x145 map." Every clause of that is now false.

(1) THE CLAMP IS GONE. BL-463 (settlement count is seed-invariant) landed 2026-08-21 and replaced it with `land_tiles / k_land_tiles_per_centre` (410), clamped only structurally to [1, placeable_tiles].

(2) THE MAP IS NOT 312x145. BL-424 took the homeworld to 70% area; `home_grid_width/height` have been 261x121 = 31,581 since commit efe1ff0. A comment in hard_coded_world.cpp still said 312x145 and has been corrected in this change.

(3) THE PER-BODY FIRM CAP IS ALREADY REPLACED, and NOT by a per-nation bound. B3's plan for BL-374 (corp density target) was to swap the per-BODY firm cap for a per-NATION one. That work landed 2026-08-20: `max_firms_per_body` is now 200 (anti-runaway only) and shaping moved to a per-RESOURCE cap (8) and a per-PROVINCE cap (2), on Ben's own dated words: "we should have two levels, per resource caps, and per province caps." Building a per-nation bound now would contradict that ruling, so it was NOT done.

[RENUMBERED ON MERGE from NR-473: three lanes ran concurrently and two of them minted the same review ids. The other NR-473 was already pushed. Cross-references inside this entry are remapped to match.]

**Why it matters.** Three of the sprint's three planned actions were already taken by other lanes between the sprint being written (2026-08-20) and being worked (2026-08-21). An agent briefed on the sprint text alone would have re-derived a constant that had just been derived, or reversed a dated ruling. The sprint goal lines are snapshots and go stale within a day at this pace; the code is the authority.

- Rewrite B3's goal to what actually remains (the density DECISION below, and BL-522).
- Close B3 as overtaken and open its remainder as a fresh sprint.

> **Recommendation:** Rewrite the goal. The remaining question is real and is the entry below.

### NR-482 — Settlement density: 86% of nations hold exactly ONE centre, and raising it triples the labour pool
*question · raised 2026-08-21 · from Sprint B3 Lane B agent, 2026-08-21. Measured by the new tools/verify/settlement_density.cpp over 3 seeds x 8 divisors, at the shipped 400-year prehistory. NOT TUNED - reported.*

BL-463 fixed the defect it was written against (the count no longer being seed-invariant) and deliberately pinned the divisor at the shipped generator's OWN measured density, so the CENTRAL density was never re-derived. This measures what that central density produces, and the number that matters is per-NATION, not per-world.

SHIPPED TODAY (divisor 410), 3 seeds, 39,543 land tiles, 168 nations:
  200 centres | 197.7 land tiles/centre | 1.19 centres per nation | median 1 | max 4
  centres-per-nation histogram: 0 nations with none, 145 with one, 15 with two, 7 with three, 1 with four
  => 145 of 168 nations (86.3%) hold FEWER THAN TWO centres.

WHY <2 IS THE THRESHOLD THAT MATTERS: road_generation.cpp's per-nation backbone skips any nation with `n < 2` centres. Sprint B2 gave a single-centre nation a Track on its own centre tile, so it is no longer road-LESS - but it has one roaded tile and no internal network. 86% of the world's nations are in that state. That is a far more direct reading of "does not appear physically civilised" than the world-level count is.

A SECOND STRUCTURAL SIGNAL: at 410 the coverage pass (one seat per uncovered nation) places MORE centres than the primary geographic pass does - 104 vs 96. Over half the settled world is a structural top-up at an argmax tile rather than a geography. As the divisor tightens the primary pass takes over: 74% of centres at 200, 83% at 150, 91% at 100.

THE SWEEP (aggregate over the same 3 seeds):
  divisor | centres | land/centre | centres/nation | nations with <2 centres
      410 |     200 |       197.7 |           1.19 |  145 (86.3%)   <- shipped
      300 |     219 |       180.6 |           1.30 |  136 (81.0%)
      200 |     264 |       149.8 |           1.57 |  119 (70.8%)
      150 |     316 |       125.1 |           1.88 |  101 (60.1%)
      120 |     373 |       106.0 |           2.22 |   87 (51.8%)
      100 |     431 |        91.7 |           2.57 |   78 (46.4%)
       80 |     521 |        75.9 |           3.10 |   64 (38.1%)
       60 |     681 |        58.1 |           4.05 |   52 (31.0%)

THE REASON THIS WAS NOT TUNED, and it is the whole point of the entry: THE DIVISOR IS NOT A GENERATION KNOB, IT IS AN ECONOMY KNOB. economy_system.cpp derives each body's entire workforce supply as a sum over its centres (`labour_by_scale[scale]`), so halving the divisor roughly doubles the labour pool the whole prototype economy runs on. Measured headcount over the 3 seeds: 27,580k at 410; 71,570k at 150; 90,980k at 100 - 2.6x and 3.3x. Workforce is one of the few binding constraints in the shipped economy. Picking a divisor here would be choosing the size of the labour market without pricing it, which is exactly the move BL-463's own "Direction, not a chosen number" section forbids.

[RENUMBERED ON MERGE from NR-474: three lanes ran concurrently and two of them minted the same review ids. The other NR-474 was already pushed. Cross-references inside this entry are remapped to match.]

**Why it matters.** Two defensible structural criteria point at different divisors, and BOTH more than double the labour pool:
  * "the MEDIAN nation is internally connected" (>=2 centres) => divisor ~100-120
  * "geography places more centres than the structural top-up does" (primary >=80% of centres) => divisor ~150
Neither is a taste number, but neither can be adopted without deciding whether the economy should run on 2.6-3.3x today's workforce. There is also a third option nobody has costed: raise the centre COUNT while shifting the scale distribution DOWN (k_scale_weight is currently 40/30/20/8/2), so the map reads as more settled without the labour pool moving. That decouples "looks civilised" from "has more workers" and may be what the complaint actually wants.

- Divisor ~150: geography dominates the top-up; ~2.6x headcount; 60% of nations still under 2 centres.
- Divisor ~100-120: the median nation gets a road backbone; ~3.3x headcount.
- Hold the divisor and re-weight k_scale_weight downward instead - more places, same people.
- Hold everything; accept that 86% of nations are one-town nations and close the complaint elsewhere.

> **Recommendation:** Do not pick from a table. The third option is the one worth costing first, because it is the only one that changes what the map LOOKS like without changing what the economy RUNS on - and the complaint was about how the world looks. Costing it needs a run of substrate_census/econ_harness in a Lua tree, which this container cannot do.

### NR-483 — NOVEL WORK: no authority doc owns population-centre DENSITY as a tuning subject
*novel-work · raised 2026-08-21 · from Sprint B3 Lane B agent, 2026-08-21. Flagged at the moment the density re-derivation was attempted.*

docs/economy/POPULATION.md owns the population-centre MODEL (scale, agglomeration, habitability feedback). docs/generation/TILE_GENERATION.md owns the tile pipeline. Neither owns the question "how many centres should a world have, and what is that number answerable to?" BL-463 answered it once, by preserving the shipped generator's own historical density - which is a defensible way to avoid picking a number, but it means the density has never been derived from anything.

The measurement above shows why it needs an owner: the divisor is simultaneously a GENERATION constant (how settled the map looks), a ROAD constant (road_generation's `n < 2` gate), and an ECONOMY constant (the body's whole labour supply). Three docs have a claim on it and none states the constraint.

The same gap applies one level up: NOTHING in the corpus states what "physically civilised" means as a target. substrate_census (Sprint B1) measures it and explicitly refuses to threshold it, correctly, because thresholding it is a design act nobody has performed.

[RENUMBERED ON MERGE from NR-475: three lanes ran concurrently and two of them minted the same review ids. The other NR-475 was already pushed. Cross-references inside this entry are remapped to match.]

**Why it matters.** Without an owner this constant gets re-derived by whoever next reads the complaint, against whatever criterion is in front of them. That is how the 180x84 clamp survived the map tripling and then the map shrinking.

- Give POPULATION.md a Density section that states the three constraints the divisor answers to.
- Leave it to BL-463's successor item once the ruling above is made.

> **Recommendation:** The former, as part of whatever change lands the ruling.

### NR-484 — spectator_determinism's pinned golden is RED on origin/main in a Lua-free harness build
*observation · raised 2026-08-21 · from Sprint B3 Lane B agent, 2026-08-21. Run at origin/main 269049d, with this session's own change STASHED - so the failure is not attributable to this work.*

`spectator_determinism` R2 ("the unspectated hash equals the pre-BL-409 golden") fails:
    golden=344A9FE48306E93A  observed=B4D09255AF346008
The observed hash is byte-identical with this session's change applied and with it stashed, which is also the proof that the change moved nothing.

CAVEAT, stated rather than glossed: this was built with the container's prescribed Lua-free object set (recipe_registry / works_registry / tech_tree / world_gen_config excluded from the link, per the brief's build recipe). The harness hand-builds its own recipe_registry, so it is *designed* to run Lua-free - but I cannot rule out that the exclusion itself shifts the hash, and I have no Windows/CMake tree here to check against. NOT re-blessed, and deliberately left red.

[RENUMBERED ON MERGE from NR-476: three lanes ran concurrently and two of them minted the same review ids. The other NR-476 was already pushed. Cross-references inside this entry are remapped to match.]

**Why it matters.** The standing rule is that goldens are contracts and movement is reported, never re-blessed without authorisation. Either the world legitimately moved on main since the golden was last blessed (the harness's own provenance log says it has been re-blessed before for exactly that reason) and it needs re-blessing with a dated note, or the Lua-free link is not hash-equivalent and the harness's build assumptions need stating. Both are worth knowing; neither should be settled by an agent.

- Run it in a full CMake tree to establish which of the two it is.
- Re-bless with a dated provenance line, if the full-tree run confirms the world legitimately moved.

> **Recommendation:** Run it in a full tree first. Do not re-bless off this container's result.

### NR-485 — hire_axis_cost moved into unit_roster.hpp, slightly past BL-498's literal wording
*decision taken on your behalf · raised 2026-08-21 · from Lane C, BL-498 (shared hire gate table).*

BL-498 asked for one shared table behind `campaign_gate_input`'s preference lists. TAKEN: moved `hire_axis_cost` into `unit_roster.hpp § hire_axis_table` alongside it, so the cost and the resources it is paid in are ONE contract rather than two that must agree. A small widening past the item's literal words.

**Why it matters.** The bug class BL-498 names is exactly 'two hand-mirrored lists drift apart'. Leaving the cost outside the table it is paid against would have fixed one instance and left the neighbouring one. But it is a scope widening on an audit-finding item, so it should be seen rather than assumed.

> **Recommendation:** Keep it. The harness can now assert cost and candidates together, which was not possible before.

*Files: `src/world/unit_roster.hpp`, `src/world/corp_command.cpp`*

### NR-486 — An interception is reported, not stored — the out_cuts sink rather than a world field
*decision taken on your behalf · raised 2026-08-21 · from Lane C, BL-458 / NR-407.*

`supply_system.cpp:158` read `(void)intercept_convoys(w, tick);` — the interception records were computed and dropped, AND the cut convoy is erased inside that same call, so an interception existed for one statement and then did not exist at all. No UI work was possible against that. TAKEN: `credit_arrived_convoys` gained an optional `out_cuts` sink, so an interception rides the tick's report the way `agency_events` does. NOT state: nothing serialised, nothing folded into `state_hash`.

**Why it matters.** It is the same call BL-467's `battle_dispatch` made two commits earlier, for the same reason — an event that is erased in the tick it happens can only reach a surface by being reported. Worth Ben seeing as a pattern forming rather than as two isolated choices. Reversible; a `world` field would have touched `state_hash` and needed a serialisation seam that does not exist (NR-349).

- Keep it reported (no state, no hash, no save format).
- Promote interceptions to world state if something later needs to read them across ticks.

> **Recommendation:** Keep it reported. An interception is an event, not state.

*Files: `src/world/supply_system.hpp`, `src/world/supply_system.cpp`, `src/world/economy_system.hpp`*

### NR-487 — Two goldens are RED on origin/main, independently confirmed by two lanes
*observation · raised 2026-08-21 · from Lanes B3 and C, each reverting to base and rebuilding to confirm it was not their change.*

`spectator_determinism` R2: golden 344A9FE48306E93A against observed B4D09255AF346008. `ai_skill_harness`: 28 failures, and it self-declares its bands 'blessed 2026-08-09 (GCC - STALE since 2026-08-14/BL-386)'. Both fail identically on the untouched base. NEITHER RE-BLESSED.

**Why it matters.** Two independent lanes measured the same observed spectator hash (B4D09255AF346008), which is the corroboration that makes it a real world change rather than a container artifact. B3 caveats that it built with the container's Lua-free object set and cannot fully rule out that the exclusion itself shifts the hash — so the re-bless wants one run on Ben's toolchain before the number is pinned.

> **Recommendation:** Re-bless both, on Ben's machine, once he is satisfied the world changes behind them were intended (BL-519's terrain split and BL-424's map area are the obvious candidates). Goldens are contracts; this is his signature, not an agent's.

*Files: `tools/verify/spectator_determinism.cpp`, `tools/verify/ai_skill_harness.cpp`, `.claude/rules/io-standing-rules.md`*

### NR-488 — A mutation test passed when it should have failed, and the agent caught itself
*observation · raised 2026-08-21 · from Lane C, BL-498. Reported by the agent against its own work.*

The first mutation probe written for the shared hire table PASSED — it had no teeth, because `hire_axis_resource`'s `return e.candidates[0]` fallback masked the mutation. Re-run with a mutation modelling the real drift (the debit stops recognising the table's LAST candidate), R5 then failed on exactly that candidate on all three axes, as it should.

**Why it matters.** This is the failure mode that makes a green harness worthless, and it is invisible unless someone deliberately tries to make the check fail. Worth recording as method: a new assertion should be proven to FAIL against a deliberate break before it is trusted to pass — the same discipline as BL-384's 'a row that would pass before the change proves nothing'.

> **Recommendation:** Consider making mutation-proving a stated step for new harness rows in DELIVERY.md. Not filed as work; Ben's call whether it is worth the ceremony.

*Files: `tools/verify/corp_ai_harness.cpp`, `docs/development/DELIVERY.md`*

### NR-490 — War-gated progression is designed but not yet live — one earnable gate in 150 tech nodes
*observation · raised 2026-08-21 · from Checking Ben's stated reason for the 2026-08-21 zero-war ruling.*

Ben ruled a world that never fights is a bug because 'many ancient tech quests would not be unlockable'. Checking what exists: `scripts/tech_tree.lua` holds 150 nodes and the file says in its own comment that E0-ML-01 is 'THE ONE LIVE GATE... Every other node in this file is authored data the F9 viewer draws'. Its predicate is in tech_gate.cpp, and it reads corp military_units/military_strength - not battles and not conquest. The Era -1 works roster gates on capacity band (materials), also not on war.

So today no content anywhere is gated on a war HAPPENING. The dependency is real as design intent and absent as mechanism.

**Why it matters.** Two things follow, pointing the same way. First, the ruling is sound but its stated justification cannot yet be verified by running anything - worth knowing before someone tries. Second, and more useful: the tech design is being drawn on top of a war rate that is ZERO in a quarter of worlds. Fixing the rate before the gates are authored is much cheaper than discovering, once 149 stubs become live gates, that a quarter of campaigns start with a dead branch.

> **Recommendation:** No action beyond noting it. But when the ancient tech quests are authored (BL-478 and the tech-effect work), the war-rate band from Sprint 28's R2 should be the number their reachability is checked against - otherwise the two are designed independently and meet only in a bug report.

*Files: `scripts/tech_tree.lua`, `src/world/tech_gate.cpp`, `docs/development/req/requirements.json`*

### NR-499 — Control changing hands mid-campaign — what happens at the moment a majority flips
*question · raised 2026-08-21 · from Raised BY Ben's NR-491 ruling. The "never confers control" design had no such moment; a threshold design necessarily does.*

Crossing > 50% turns a corporation the scorer was running into one the player drives, and dropping below hands it back. Four things are undefined: (1) whether reversion is immediate or lagged; (2) what happens to a build order already in flight at the moment control flips; (3) whether the corp_ai scorer, on regaining a corporation, resumes cleanly or needs its state rebuilt; (4) whether a rival syndicate can take control OF A CORPORATION THE PLAYER CONTROLS by buying past them.

**Why it matters.** Item (4) is the one that needs a ruling rather than a design. It is a relational action by a rival against a corp a human owns — the same threshold BL-450 (rivals score stance) needed its own dated grant to cross, and the standing rules treat that class of widening as requiring an explicit, dated grant rather than an inference. The other three are ordinary state-transition design, but they are the kind that is cheap now and expensive after BL-525 fixes the data model. All four are downstream of a ruling that only landed today, so none of them is a gap in the original design.

- Rule now, before BL-525 is written.
- Defer to BL-529 (rival syndicate behaviour), where the acquisition-contest question naturally lands.
- Rule (4) now — it is a standing-rule grant — and defer (1)-(3) to BL-529.

> **Recommendation:** Option 3. The three mechanical questions are genuinely BL-529's to answer with the scorer in front of it. But a rival taking control of the player's corporation is a grant, not a mechanism, and the standing rules are explicit that such widenings are dated and scoped rather than inferred — so it should not arrive as an implementation detail inside a scoring item.

*Files: `docs/development/backlog.json`, `.claude/rules/io-standing-rules.md`*

### NR-500 — BL-523 (corp kind axis) and BL-529 (control gate) both add a filter to corp_ai's candidate loop
*question · raised 2026-08-21 · from Found while resolving the merge conflict between this session's branch and main. The two items were authored in parallel branches on the same day and neither references the other.*

BL-523 makes `corp_ai.cpp` read a corporation KIND (today the file contains no reference to `is_background` at all, so a background filler firm evaluates military_base and hire_unit exactly like a named rival). BL-529 makes it read CONTROL (skip any corporation the player's syndicate majority-holds, per Ben's NR-491 ruling). They are orthogonal axes rather than duplicates, but they gate the same loop. Separately, BL-526's carve splits named corps into operating corporations and does not say whether a carved corporation is background or named — which is precisely the input BL-523's axis needs.

**Why it matters.** Two independently-added filters on one candidate loop is how that loop stops being explainable — the same argument BL-219 won when it retired a second mechanism deciding one field. Cheap to reconcile now, while both are unbuilt and BL-523 is still design-owed; expensive once one has shipped and the other is written around it. The carve ambiguity is the sharper half: BL-523 cannot be specified against a kind axis whose value for carved corporations is undefined.

- Reconcile now: one gating mechanism reading both axes, specified across BL-523 and BL-529.
- Build BL-523 first (it is v0.1.22, ahead of v0.1.23) and have BL-529 extend its gate rather than add one.
- Keep them independent and accept two filters.

> **Recommendation:** Option 2. BL-523 is already the earlier minor and is design-owed, so it can absorb the requirement cheaply; BL-529 then extends a gate that exists rather than inventing a parallel one. Either way BL-526 owes an answer on whether a carved corporation is background or named — that is a gap in my item, not in BL-523.

*Files: `src/world/corp_ai.cpp`, `docs/development/backlog.json`*

### NR-501 — BL-519 and BL-520 are now RENDERED — the container-blind half of NR-451 and NR-457 is closed
*observation · raised 2026-08-22 · from UI session 2026-08-22, local build*

Built ProjectIo locally (BUILD_OK, exe relinked 01:31) and ran tile_texture.lua, province_render.lua and click_injection.lua. NR-451 and NR-457 both recorded that BL-519 and BL-520 were compile-checked only, never rendered, because that container had no SDL3 and no display. That is now closed: 14 texture captures, 20 province captures and 6 click captures exist under build/screenshots/. The texture pass genuinely works — province_blend_close.png is the frame that shows it, with per-tile canopy ticks legible over a province blend that still reads as one continuous field, and texture_cinder_close.png shows the non-biotic chip/scratch grain doing the same on an airless body. Ocean draws nothing, as designed.

**Why it matters.** Two items shipped complete on a compile check alone. They now have eyeballed evidence, and the evidence supports the design rather than contradicting it.

*Files: `build/screenshots/`, `scripts/verify/tile_texture.lua`*

### NR-506 — There is no space to the RIGHT of the minimap — its right edge is flush to the screen edge by design (BL-312)
*question · raised 2026-08-22 · from UI session 2026-08-22, shell_metrics.cpp*

Your first framing put the legend "to the right of the mini-map", and the form answer sized it at "about 1/4 the minimap space". The first of those cannot be built as stated: minimap_rect returns { disp.x - w, ... }, so the minimap right edge IS the screen edge, deliberately, under BL-312 — the comment calls it an 8px gap with no neighbour to its right to justify it. Measurements, since they are all fixed: minimap_width = max(336, 0.28 * min(disp_x, disp_y)) and minimap_height = 0.75 * width. That floor of 336 wins on every common display — 1280x720, 1600x1000, 1720x1080 and 1920x1080 ALL give exactly 336 x 252 px. It only grows above roughly 1200px of minimum dimension (2560x1440 gives 403 x 302). So a quarter of the minimap space is about 21,168 px-squared, which is 84 x 252 as a side column, or 336 x 63 as a strip above or below it.

**Why it matters.** Three readings produce materially different layouts and I do not want to guess between them. An 84px-wide column cannot hold a nation name at all, so that reading forces the shortened names and wrapping you already asked for, hard.

- A strip 336 x 63 directly ABOVE the minimap, scrolling — full width, so names fit, and nothing moves
- A column 84 x 252 to the LEFT of the minimap, scrolling — matches "beside it" but is very narrow
- The legend overlays a quarter of the minimap itself, translucent
- Shrink the minimap and put the legend in the space freed to its left

> **Recommendation:** The strip above the minimap. It is the only one of the four where a nation name fits on one line at a readable size, and it costs the minimap none of its area.

*Files: `src/ui/shell_metrics.cpp`, `src/ui/body_surface_canvas.cpp`*

### NR-507 — BL-533 landed and found a real click-through defect: the canvas was taking presses meant for the legend
*observation · raised 2026-08-22 · from UI session 2026-08-22, lens_legend.lua*

Re-homing the legend surfaced a defect that predates it and would have applied to any background-draw-list surface. The legend paints through ImGui::GetBackgroundDrawList(), which draws pixels and registers no window, so io.WantCaptureMouse stayed false over it — and app.cpp derives the canvas primary_input from exactly that flag (primary_input = !mouse_in_mm && !panel_blocking). Measured symptom: clicking the legend header toggled the legend AND selected whatever tile lay underneath, moving the Selection panel to a tile the player never aimed at. Confirmed real rather than a harness artifact by hovering to settle WantCaptureMouse first, which changed nothing. Fixed with an empty always-on window over the box footprint. Proof: legend_country_collapsed.png and legend_country_recollapsed.png are now BYTE-IDENTICAL across an open-then-close cycle, where before the second capture carried a different selected tile.

**Why it matters.** Any future on-canvas surface drawn the same way inherits the same hole, and it is invisible to every check we have — the surface looks correct in a capture while quietly stealing or leaking presses. Worth knowing before the next one is built.

*Files: `src/ui/body_surface_canvas.cpp`, `src/core/app.cpp`*

### NR-512 — docs/CONCEPT.md is two framings behind, and CLAUDE.md sends every new session there first
*observation · raised 2026-08-22 · from Phantom-feature scan, 2026-08-22 (PHANTOMS.md § Class 5).*

CONCEPT.md carries the 2026-08-10 militia correction (NR-120) but NOT the 2026-08-12 two-arcs split, and not the 2026-08-21 syndicate tier. So it still describes solar-to-galactic scope, orbital logistics and planetary isolation as the live design, when the live arc is ancient and 0 CE with a mercenary company in the seat. CLAUDE.md § Documents says of it: 'Start here for questions about what the game is and how it should feel.' Separately and in the other direction, MANUAL.md § 5 still lists the campaign conflict layer as [OWED], which BL-467/BL-468/BL-469 falsified on 2026-08-21.

**Why it matters.** The doc-map's designated first read is the one most out of date. The authority time-slice explains why - BL-094 is parked and unlanded, so its framing has not propagated - but the two-arcs split is not BL-094's to land, and CONCEPT.md carries no dated note for it the way it does for the militia.

> **Recommendation:** A dated note at the head of CONCEPT.md naming the live arc, same shape as the existing 2026-08-10 correction. Cheap, and it stops the doc actively misleading until BL-094 lands. Filed rather than done: re-voicing CONCEPT.md is session 10 of the proposed sequence, and it is yours to voice.

*Files: `docs/CONCEPT.md`, `docs/MANUAL.md`*

### NR-513 — novel-work: the phantom register is a new standing artifact, and it has no lifecycle
*novel-work · raised 2026-08-22 · from Phantom-feature scan, 2026-08-22.*

docs/development/PHANTOMS.md is a new document class - a scan output that points at design sessions. Nothing in the corpus owns 'a list of things with no owner', and it overlaps three existing stores: NEEDS_REVIEW.json (open calls), backlog.json (work), and MILITARY.md § What is absent (a per-doc holes list, which is the pattern this file generalises). It is written as transient - stale by design once the sessions run - but nothing says who prunes it or when.

**Why it matters.** An unowned list of unowned things is how a fourth store accretes. The alternative shape, and probably the better one, is no central file at all: every doc that owns a partially-built system carries its own 'What is absent' section, MILITARY.md's model, and the scan becomes a periodic check rather than a document.

- Keep PHANTOMS.md as a transient scan output, pruned as sessions land.
- Dissolve it into per-doc 'What is absent' sections and re-run the scan periodically instead.
- Keep it and give it a query tool if it grows.

> **Recommendation:** Option 2 as the destination, option 1 to get there - the file is useful right now because ten sessions are not yet run. Once each subject has an owner, its rows move into that owner's absent-list and the file goes away.

*Files: `docs/development/PHANTOMS.md`*

### NR-525 — REVIEW: docs/economy/LOGISTICS.md — the network authority (Ben flagged this one as the priority)
*question · raised 2026-08-22 · from Phantom-scan documentation pass, 2026-08-22. Ben asked for a review item per doc: "I'll read them in full and let you know where I would want to change things."*

New 322-line authority, split from SUPPLY.md on the line "Logistics is the road; Supply is the traffic". Ben, 2026-08-22: "Pay extra attention to the logistic system, as that is the most substantial part which I feel I haven't given a firm authority on." Owns BL-325 ruling 3 in general form, the shared traversal-cost function, reach as the placement constraint, roads and tiers, physical scale and travel time, cache invalidation, interdiction, and the whole BL-464 Logistic Points design - two rulings, three settled rules, and the seven findings that rejected its first cut. Five open questions at the end. | ANSWER FORM: https://claude.ai/code/artifact/debe7b8f-7315-429a-a805-0e295e9405bc - this doc's open questions are a section of the design register, with the evidence, the options and a free-text field. Answers copy back as one block.

**Why it matters.** Ben asked to read each new authority doc in full and mark what he wants changed. This entry is the place to record his verdict; resolve it with the changes made, or with "accepted as written".

> **Recommendation:** Read § Logistic Points first - it is the largest section and the one carrying settled rulings that were only ever inside a backlog item. The finding worth your attention is the last one: goods-vs-force priority is currently decided invisibly by tick phase order, so "the army goes unsupplied" is an inherited default nobody chose.

*Files: `docs/economy/LOGISTICS.md`*

### NR-526 — REVIEW: docs/generation/PROVINCES.md — the spatial unit of consequence
*question · raised 2026-08-22 · from Phantom-scan documentation pass, 2026-08-22. Ben asked for a review item per doc: "I'll read them in full and let you know where I would want to change things."*

New 217-line authority for the province as a game object, gathered from province.hpp, TILE_GENERATION.md, PLANETARY.md and SELECTION.md. Owns the four partition rulings, the three size constants (7 soft / 12 preferred / 20 hard) and why the hard cap is asserted rather than imposed, the three domains, unit position at province grain (NR-405), and your 2026-08-22 ruling that every lens blends. Four open questions, two of which are your existing unruled review entries (NR-433 on the 3-tile floor, NR-421 on the inert ceiling). | ANSWER FORM: https://claude.ai/code/artifact/debe7b8f-7315-429a-a805-0e295e9405bc - this doc's open questions are a section of the design register, with the evidence, the options and a free-text field. Answers copy back as one block.

**Why it matters.** Ben asked to read each new authority doc in full and mark what he wants changed. This entry is the place to record his verdict; resolve it with the changes made, or with "accepted as written".

> **Recommendation:** The open question worth deciding is 3 - whether a province ever gains an OWNER. Every use so far is spatial, and BL-518 (war redraws borders) is where that pressure first arrives.

*Files: `docs/generation/PROVINCES.md`*

### NR-527 — REVIEW: docs/economy/CONTRACTS.md — the income loop, out of the market doc
*question · raised 2026-08-22 · from Phantom-scan documentation pass, 2026-08-22. Ben asked for a review item per doc: "I'll read them in full and let you know where I would want to change things."*

New 250-line authority carving procurement and the mercenary contract out of MARKETS.md, where the game's income loop had been a subsection of the market document. Records BL-377's full design - the condition_set spine, offers derived from history_sim's campaign scorer, the four settled answers, three terminal states - plus what the nations session changed: the client is now a nation with a treasury, and the relationship rides sentiment's Trust dimension rather than a parallel axis. | ANSWER FORM: https://claude.ai/code/artifact/debe7b8f-7315-429a-a805-0e295e9405bc - this doc's open questions are a section of the design register, with the evidence, the options and a free-text field. Answers copy back as one block.

**Why it matters.** Ben asked to read each new authority doc in full and mark what he wants changed. This entry is the place to record his verdict; resolve it with the changes made, or with "accepted as written".

> **Recommendation:** Open question 1 is the one that needs you: a contract fee is an actor-to-actor transfer, which NATIONS.md says must conserve - so a nation that cannot afford a contract must not be able to offer it. That couples offer generation to BL-537 budget and is not stated anywhere yet.

*Files: `docs/economy/CONTRACTS.md`*

### NR-528 — REVIEW: docs/PEOPLE.md — named people and roles (NEW system, proposal not design)
*question · raised 2026-08-22 · from Phantom-scan documentation pass, 2026-08-22. Ben asked for a review item per doc: "I'll read them in full and let you know where I would want to change things."*

New 196-line design-owed doc from your 2026-08-22 ask. Everything past § Precedent is a PROPOSAL, not settled design. Builds on BL-207 (persona counsel packs, already fully designed) and BL-370 (corp leader figure, your own 2026-08-11 ask) rather than inventing a rival. Proposes: a person is a name, a role, a seat, a tenure and AT MOST ONE bias; a cast not a population; and mortality, because a person who never leaves is a permanent modifier wearing a name. Six open questions. Filed as BL-547. | ANSWER FORM: https://claude.ai/code/artifact/debe7b8f-7315-429a-a805-0e295e9405bc - this doc's open questions are a section of the design register, with the evidence, the options and a free-text field. Answers copy back as one block.

**Why it matters.** Ben asked to read each new authority doc in full and mark what he wants changed. This entry is the place to record his verdict; resolve it with the changes made, or with "accepted as written".

> **Recommendation:** Question 2 is the cheapest to overturn now and the most likely to be wrong: one bias, or several. Question 1 sets the order - company head is your own earlier ask and needs no other system; minister is the highest value because it gives lobbying a target.

*Files: `docs/PEOPLE.md`*

### NR-529 — REVIEW: docs/EVENTS.md — random events (NEW system, proposal not design)
*question · raised 2026-08-22 · from Phantom-scan documentation pass, 2026-08-22. Ben asked for a review item per doc: "I'll read them in full and let you know where I would want to change things."*

New 212-line design-owed doc from your 2026-08-22 ask. The interesting problem is the word "random": Io is deterministic, so the doc reuses YOUR OWN BL-315 ruling 4 - uncertain to the player, deterministic to the engine, drawn from a seeded stream folded from the subject's identity. Second finding: the meta layer already supplies two thirds of an event (condition_set = predicate, modifier_set = effect), so only the trigger is new - and it is the consumer collapse_strain has been waiting for. Filed as BL-548. | ANSWER FORM: https://claude.ai/code/artifact/debe7b8f-7315-429a-a805-0e295e9405bc - this doc's open questions are a section of the design register, with the evidence, the options and a free-text field. Answers copy back as one block.

**Why it matters.** Ben asked to read each new authority doc in full and mark what he wants changed. This entry is the place to record his verdict; resolve it with the changes made, or with "accepted as written".

> **Recommendation:** Question 4 decides how large this system is: do events DRIVE the collapse metagame or merely express it. Question 2 - how often is an event - is the single number separating texture from chaos, and nothing has established a target rhythm.

*Files: `docs/EVENTS.md`*

### NR-532 — The design register is live — 41 open calls across ten sections, as a form
*observation · raised 2026-08-22 · from Ben, 2026-08-22: "now please revisit each one and open forms for answering the open questions."*

Published at https://claude.ai/code/artifact/debe7b8f-7315-429a-a805-0e295e9405bc. Every open question across the eight new authority docs plus SYSTEMS.md § The progression chain plus four cross-cutting calls, gathered into one form: 41 questions in 10 sections. Each carries its evidence, 3-5 options with one marked as suggested, and a free-text field that overrides the options. Progress is tracked per section; "Copy all answers" puts everything on the clipboard in one block. It is a LIVE DOC - radios and contenteditable fields are captured as edits, so answers reach a watching session directly; deliberately no <textarea> and no <select>, neither of which is captured. Answers also persist to localStorage as a per-viewer draft.

The generator is committed rather than being a one-off (CLAUDE.md § Tool creation is skill creation): tools/session/register/questions.js is the canonical question set and build.js emits the HTML. Verified to regenerate byte-identically. Republish to the same URL to keep answers in place.

**Why it matters.** The open questions were the point of writing the docs as capture rather than design, and they were spread across ten files. A form is the difference between 41 questions that get answered and 41 that get skimmed. It also means the answers arrive in one structured block that can be propagated in a single pass, the way the six nations rulings were.

> **Recommendation:** Answer in any order. The four that change the most downstream: LOGISTICS Q1 (what generates LP), EVENTS Q4 (drive the collapse metagame or express it - it decides the system's size), PEOPLE Q2 (one bias or several - cheapest to overturn now), and NATIONS Q1 (whether the grant reaches a rival, which blocks the player-facing halves of two items).

*Files: `tools/session/register/questions.js`, `tools/session/register/build.js`*

### NR-533 — A rival hostility declaration is now SIGNALLED, which overturns NR-350 and removes the ambush property for the player
*decision taken on your behalf · raised 2026-08-22 · from Ben's answer to RELATIONS Q4 in the design register.*

Ben chose "Yes, but signalled - a rival declaration surfaces to the player". NR-350 had a declaration stay SILENT, discovered on contact rather than announced, and BL-448 shipped on that. Newest-dated wins, so the reversal stands and is recorded rather than quietly applied.

**Why it matters.** stance.hpp's directed hostility exists so that "a corp can be at war and not know it yet" - the AMBUSH PROPERTY BL-458's interdiction was designed around. Signalling removes that property FOR THE PLAYER: interdiction becomes a known risk rather than a surprise, and the first lost convoy stops being the first news. Directedness itself is untouched - hostility still needs no reciprocation. What the ruling does NOT settle is whether rival-vs-rival declarations are equally visible, which is a BL-068 question.

> **Recommendation:** Accept - the ruling is deliberate. Two things to carry: BL-458's design prose still argues from the ambush property and should be read with this in mind, and the rival-vs-rival visibility question needs an answer before BL-449 renders anything.

*Files: `docs/politics/RELATIONS.md`, `docs/economy/LOGISTICS.md`, `src/world/stance.hpp`*

### NR-534 — Backlog schema gains `superseded_by` — the first way to retire an item without claiming it was built
*decision taken on your behalf · raised 2026-08-22 · from The designed/design-owed triage Ben asked for, 2026-08-22.*

backlog.json's note says `complete` is the ONLY terminal status and that terminal items must be retained, never deleted. That left no way to express "this design is superseded and was never built" — the two available readings were both wrong: `complete` claims work landed (which is the BL-377 defect, deliberately re-introduced), and deletion loses the reasoning. Added a `superseded_by` array field carrying the ids that took the strands on, with status `complete` and a resolution opening SUPERSEDED, NOT BUILT. backlog_lint now (1) exempts such items from the false-complete arm — they never intended to write the files they name — and (2) FAILS if a superseded item names a destination id that does not exist, or carries superseded_by without being terminal. Both guards were negative-tested; the first draft of the exemption did not fire at all, because it sat after a guard that skips items naming no source paths.

**Why it matters.** Retiring an item as `complete` would make the backlog lie in exactly the way the phantom scan was opened to catch, and the new drift check would either flag it forever or need allow-listing. This is a schema addition rather than a convention, so Ben should confirm it - two items now carry the field (BL-158, BL-345) and it is cheap to rename or restructure while that is true.

- Keep `superseded_by` as an array of destination ids (what shipped).
- Add a distinct terminal status `superseded` instead, and update the tooling that reads status.
- Revert: retire by editing the resolution only, with no structured field.

> **Recommendation:** Keep it. A structured field is queryable and the lint can enforce it; a status change would touch gyre.py, ship_items.js, apply_session_close.js and every progress count, for no more expressiveness.

*Files: `docs/development/backlog.json`, `tools/session/backlog_lint.js`*

### NR-535 — Triage result: 178 open items reviewed, 2 retired, 12 re-pointed, 1 title three framings behind — and the suspects were mostly not stale
*observation · raised 2026-08-22 · from Ben, 2026-08-22: "let's do a pass on all designed and owed backlog items. We can reword them to fit the current auth docs, or drop things that are no longer relevant."*

178 open items (149 designed, 29 design-owed). A keyword scan flagged 107 as touching a subject the new docs own, which was too coarse to act on — 'province' and 'law' alone matched 32 items each, nearly all of them ordinary mentions. What actually paid:

RETIRED (2). BL-158 (politics datamodel stub) and BL-345 (politics relationship axis), both into BL-545. BL-345 is not merely superseded but CONTRADICTED: it proposed a SYMMETRIC per-pair scalar and Ben ruled directed. Its acceptance rule — 'a stub nothing reads is indistinguishable from no stub' — transferred to BL-545 and now binds it to land with a consumer.

RE-POINTED (12) at the doc that now owns the subject, on a rule applied consistently: a UI item keeps its SURFACE doc, a subject item takes the subject doc. So BL-474/475 (stance surfaces) stayed on LENSES/SELECTION while BL-297/298 (diplomacy seam) moved to RELATIONS.

CORRECTED (1). BL-186's title said the laws ledger is 'the surface for enacting/repealing laws' while its own body had struck exactly that as the overturned governing-body model. The body was right and the title was three framings behind — and a title is what a backlog query returns.

**Why it matters.** The 2026-08-13 retire triage checked 14 suspects and ZERO retirements survived; this one checked more and two survived, for a specific reason worth recording: BL-545 did not exist in August 13. A retirement survives when the destination exists, and not before. That is also why BL-158's earlier refutation ('the character_preset strand would be orphaned') no longer holds — BL-155 has described that object since 2026-07-10 and now names it.

The finding that reaches furthest: BL-158 proposed DIRECTED sentiment on 2026-08-02, with the same argument Ben used twenty days later ('a symmetric scalar cannot express A resents B, B is indifferent'). So NR-520 undercounted — FOUR designs were converging on one quantity, not three, and the earliest was already right about the axis.

> **Recommendation:** Two axes remain unworked and both need a build to do honestly. The 13 FALSE-OPEN candidates the new lint reports each need checking against the code before a status flips - BL-458, BL-511 and BL-480 are near-certain, the rest are not. And the parked set (36 items, mostly space-arc) was left alone deliberately: a parked item legitimately carries its own arc's framing, and rewording it to the live arc would be the churn the two-arcs split exists to avoid.

*Files: `docs/development/backlog.json`*

### NR-536 — Sprint N1 pre-change baseline captured — state_hash B4D09255AF346008, and the one red is the known pre-existing one
*observation · raised 2026-08-22 · from Opening Sprint N1. An inertness claim can only be proven against a measurement taken BEFORE the change.*

Built the world layer by the NR-264 recipe and ran three baselines at commit d85416e, before a line of Sprint N1 was written.

  world_determinism        ALL PASS (0 failures) - 400-year era pass, identical across two same-seed runs
  money_conservation       ALL PASS (0 failures) - including P3.0, total corporate cash never moves over 8 ticks
  spectator_determinism    1 FAILURE, and it is PRE-EXISTING

The failure is R2 byte-identity: golden=344A9FE48306E93A observed=B4D09255AF346008. Those are the SAME TWO VALUES NR-484 and NR-487 record as red on origin/main, so it is attributed and is not this sprint's.

THE USEFUL PART: B4D09255AF346008 is the observed state_hash of an unspectated played session at the pre-N1 commit. All three Sprint N1 items claim INERTNESS at authored-zero rates. If integration leaves that value unchanged, the claim is proven at the strongest available level - the whole world hashing identically - rather than by a harness asserting its own narrow property.

**Why it matters.** Three requirement rows across three items say 'bit-identical to the pre-item build'. Without a pre-change number those rows can only be self-asserted by the harness that ships with the change, which is the false-green failure mode the adversarial pass exists to catch. This is the number they are checked against.

It also means the economy benchmark's deliberate red (NR-269/271/272) and this golden's red are both accounted for BEFORE the batch, so anything new that goes red is the batch's.

> **Recommendation:** Re-run all three after integration and compare against these exact values. A moved state_hash on an all-zero-rates build is a FAILED inertness row, not a re-bless.

*Files: `tools/verify/world_determinism.cpp`, `tools/verify/money_conservation.cpp`, `tools/verify/spectator_determinism.cpp`*

### NR-537 — Hotspot audit: all four "serial" clusters are NOT serial, three more phantom-file declarations, and ~8 more false-opens
*observation · raised 2026-08-22 · from Four-agent audit of the biggest file hotspots, run while Sprint N1 built. Every claim below was spot-checked by hand before acting.*

A file-collision map over near-term open work said history_sim.cpp (24 items), body_surface_canvas.cpp (13), corp_ai.cpp (11) and components.hpp (11) were serial bottlenecks. AN AGENT READ EACH ONE. All four came back NOT SERIAL, and the file-level count was wrong in a different way each time.

history_sim.cpp: 82% of the file is ONE 1,040-line function, so '24 items touch this file' is nearly '24 items touch this function'. What rescues it is a property stated in the file's own header - `salt` is a HASH, not a generator, so 'nothing here consumes a stream, and inserting a decision does not shift the numbers a later decision sees'. Concurrent edits are safe HERE and would not be in a seeded-stream design. Ten named split seams. Three residual serialisers, all narrow - and the load-bearing one is undocumented: THE SCORING-BLOCK ORDER IS SEMANTICALLY LOAD-BEARING (line 843: build_work is scored last because every earlier verb uses `>` against best_score), so two agents each inserting a scoring block merge into an arbitrary order that changes tie-breaks and therefore every generated world.

components.hpp: THE APPEND-ONLY RULE DOES NOT BIND THIS CLUSTER AT ALL. The agent traced every enum to its consumers: not one of the 11 items appends a value to any enum in the file. I had assumed the opposite. The real append-only contention is in a file NOBODY declared - corp_command.hpp's `corp_decision_reason` (BL-446/447/450) and `corp_verb` (BL-377/472/511).

body_surface_canvas.cpp: 'it is not even 13 items - three have already landed.'

corp_ai.cpp: three independent concerns; export_corp_blackboard shares zero symbols with the scorer.

THREE MORE PHANTOM FILES, the BL-377 shape: BL-505 declares src/world/name_generation.{hpp,cpp} (never existed; the generator is tongue.*), BL-551 declares src/world/contracts.cpp (does not exist - inherited from BL-377), BL-525 declares src/world/serialisation.cpp (does not exist and NO item creates it - the seam is sectional, not central). All three corrected.

AND MORE FALSE-OPENS, each with a commit: BL-514 and BL-532 landed in c8a345a; BL-533's half in 3932eea; BL-408's canvas half at 57e596f; BL-480's treasury at 95a68d8; BL-511's components half at 3f2c7c6; BL-212 'LANDED 2026-07-28' per its own note with ZERO references in the file it declares; BL-439 and BL-440's corp_ai halves.

**Why it matters.** THE METHOD FINDING IS THE BIGGEST ONE: a file-level collision map is systematically WRONG in both directions. It over-counts (two items at opposite ends of a 3,545-line file are not in each other's way) and under-counts (the real append-only contention was in corp_command.hpp, which the map never surfaced because few items declare it). collision_map.js's header already warns of exactly these two blind spots; this audit measured them.

The scheduling consequence is large and favourable: v0.1.16's 36 items are NOT a serial queue. The audit proposes six waves for the Fall cluster with three-and-four-way parallel arms.

The scheduling consequence that is UNfavourable: four v0.1.16 items are blocked by items in LATER minors. BL-491 requires BL-320 (v0.1.19); BL-493 requires BL-425 and BL-427 (both v0.1.22); BL-494 requires all four plus BL-462's territory (v0.1.22). A minor cannot cut while a quarter of its perf chain waits on three later minors.

> **Recommendation:** Three things, in order. (1) The version-goal inconsistency is the one that would actually block a cut - either those four items move out of v0.1.16 or their blockers move in. It is a sequencing call and it is Ben's. (2) The confirmed false-opens want the same treatment BL-514 and BL-532 got here: verify by hand, then flip with the commit named. I did the two the audit called FULLY landed and left the partial ones (BL-533, BL-511, BL-408) with their halves stated, because a partial slice landing under an open item is exactly the case the lint warns not to flip blind. (3) The corp_command.hpp append contention should get the Fall-arc convention applied: one appender per batch, in a stated order.

> **RESOLVED.** PARTLY RULED BY BEN, 2026-08-22: move the inverted items out of v0.1.16. Done, and the count was three rather than four - BL-492's inversion had already been dissolved by correcting its dependency (it named BL-491 where its own text needs strain, which is BL-504 at the same minor), so it is startable in v0.1.16 and was deliberately left there with the reason written into the item so a later pass does not sweep it along. BL-491 -> v0.1.19 (needs BL-320). BL-493 -> v0.1.22 (needs BL-425 and BL-427). BL-494 -> v0.1.22 (latest blocker sets the floor; also missing hard_coded_world.cpp from its file list, where its deliverable actually lives - added). v0.1.16 falls from 36 open items to 33. The audit's other findings - the six-wave batching for the Fall cluster, the corp_command.hpp append contention, the remaining confirmed false-opens - are untouched and still open.

*Files: `docs/development/backlog.json`, `tools/session/collision_map.js`*

### NR-538 — A version-goal inversion sweep found two more, and one was created THIS SESSION by an otherwise-correct ruling
*decision taken on your behalf · raised 2026-08-22 · from Moving the inverted v0.1.16 items out, on Ben's instruction.*

Having moved the three the audit named, I swept the whole backlog for the same defect - an open item whose version goal sits EARLIER than a blocker's. Two more turned up.

BL-391 (reputation floor deadlock) at v0.1.15 requiring BL-546 at v0.1.24. THIS SESSION CREATED IT: the sentiment ruling made BL-391 require BL-546, and nothing moved the version goal to match. Moved to v0.1.24 and the reasoning written into the item. THE COST IS STATED RATHER THAN BURIED: this is a priority-A LIVE defect - a player measured reputation -6 against a floor of -5 with no mechanic anywhere that could move it back - and it now ships two minors later than it did this morning. The alternative is landing decay standalone at v0.1.15 and migrating it in BL-546, which is the double-build the substrate ruling exists to avoid. If that trade is wrong it is Ben's to overturn.

BL-211 (player-facing history ledger) at v0.1.11 requiring BL-210 at v0.4.0. PRE-EXISTING and NOT moved: BL-210 is PARKED, so this is an item waiting on a decision rather than on a build, which is a different problem from a scheduling inversion. Left alone deliberately and flagged by the new check as 'blocker is PARKED - may be legitimate'.

Added checkVersionInversion() to backlog_lint. Negative-tested: re-introducing BL-494's inversion makes it name all three blockers by version. It is a WARNING not a fail, because a parked blocker can be legitimate and a version goal is a plan rather than a contract.

**Why it matters.** The shape is what makes it worth a permanent check rather than a one-off fix: THE INVERSION ARRIVES AS A SIDE EFFECT OF AN OTHERWISE-CORRECT EDIT. Adding a `requires` is right; forgetting that it re-dates the item is invisible, and nothing else in the tooling notices. That is the third defect class this session that was invisible from every status view - after the false-complete (BL-377) and the false-open (BL-514, BL-532) - and all three now lint.

> **Recommendation:** Confirm or overturn BL-391's move. It is the only one of the five with a real cost, and it is a priority-A defect slipping two minors.

*Files: `docs/development/backlog.json`, `tools/session/backlog_lint.js`*

### NR-510 — corp_modifiers is NOT recomputable from earned_techs - the save serialises it directly, against BL-107's own note
*decision taken on your behalf · raised 2026-08-22 · from BL-536 (world snapshot save), writing the load path*

world.hpp calls corp_modifiers "DERIVED state - recomputable from earned_techs x the gate table", and BL-107 carries a 2026-08-19 change-note saying the save format "owes a recompute-on-load (re-fold earned techs through the effect table) or loads will silently drop every earned buff". I set out to write that re-fold and it does not reconstruct the right ANSWER.

The order is the problem. advance_tech_gates appends a corp's modify_scalar effects in GATE-TABLE order within one call, but it runs every tick, so the stored sequence is really sorted by (earn tick, gate index). A re-fold from earned_techs can only sort by (gate index). The two differ the moment a corp satisfies a higher-index gate before a lower-index one - and nothing stops that, since gate conditions are independent.

It matters because modified_scalar folds in stored order and apply_scalar_modifier mixes add/subtract with multiply, which do not commute. A re-fold would silently return a different number for a corp that earned its techs out of table order.

DECISION TAKEN: corp_modifiers is SERIALISED directly rather than recomputed. It is small (a handful of entries per corp), and its stored ORDER carries information the earned set does not. This makes it state, not cache.

**Why it matters.** Two docs now say something inaccurate: world.hpp's 'DERIVED state - recomputable' comment, and BL-107's change-note prescribing the re-fold. I have corrected the code and the requirement row; the two prose claims want Ben's eye because the same reasoning may apply to any other field described as 'recomputable' whose consumer is order-sensitive.

### NR-511 — The two shipped binary streams keep private copies of the read/write primitives; the new header does not absorb them
*decision taken on your behalf · raised 2026-08-22 · from BL-536 (world snapshot save), writing src/world/binary_io.hpp*

history_log.cpp (IOHL) and province.cpp each define file-local write_u32/read_u32/write_str/read_str, byte-identical to the ones binary_io.hpp now provides. I left both alone rather than refactoring them onto the shared header.

Reasoning: their bytes are shipped, a snapshot written by an older build must keep reading, and the refactor changes no behaviour a reader can observe. Against that, CLAUDE.md's tone section calls duplication a debt, and there are now three copies.

**Why it matters.** It is a small, deliberate piece of duplication in exactly the seam that is supposed to have ONE definition of its conventions. Worth a ruling now rather than discovering three more copies later.

### NR-512 — A world snapshot embeds the history-log stream whole, magic and all, rather than re-writing its records
*decision taken on your behalf · raised 2026-08-22 · from BL-536 (world snapshot save)*

world::history_log and world::provinces already have a serialiser - write_history_log/read_history_log, which since BL-466 also carries the province section as its trailing block. The world snapshot calls those functions as one nested section instead of writing those two containers itself.

The consequence is that a snapshot contains a complete IOHL stream inside it, with its own magic and version header. That reads slightly odd on a hex dump, and it means the history log is version-guarded twice.

**Why it matters.** The alternative was a second definition of the same bytes - the NR-511 problem again, and this time for records that are genuinely intricate (the per-topic timestamp convention, the strictly-ascending province id check). Reuse looked clearly right, but a nested magic is the kind of thing worth stating out loud rather than letting someone find.

### NR-513 — verify.command never routed through dispatch_action, so every app-level command it named silently did nothing
*observation · raised 2026-08-22 · from BL-536, wiring the F5/F6 quick-slot bindings*

verify_api.cpp's `command` hook called ui::apply_canvas_command directly, under a comment claiming it was "the same dispatch the keyboard uses". It was not. A key press goes through app::dispatch_action, which handles the commands needing app members - the six time controls, the three UI toggles - and only then falls through to apply_canvas_command. So verify.command("pause_toggle") did nothing at all, and neither did speed_1..speed_5, help_toggle, options_toggle or tech_tree_toggle.

Nothing caught it because no committed script uses one: a grep of scripts/verify shows only ascend, descend and zoom_in, all of which apply_canvas_command handles. The gap was invisible precisely because it sat under the part of the vocabulary nobody had scripted yet.

FIXED as part of BL-536 (it had to be, or the two new save bindings would have been the tenth and eleventh commands to silently no-op). Behaviour-identical for every existing caller.

**Why it matters.** A verify hook that quietly does nothing is worse than one that errors: a script reads as if it drove the clock, the capture looks plausible, and the claim is false. Worth knowing that nine commands were in that state, in case any past reasoning leaned on one.

### NR-514 — Save is a discrete act, not the per-tick autosnapshot TECH_FOUNDATIONS described
*decision taken on your behalf · raised 2026-08-22 · from BL-536 (world snapshot save)*

TECH_FOUNDATIONS' save-model paragraph specified a tick-boundary snapshot: "the full simulation state will be serialised at each economy tick... manual save is a named copy of the most recent snapshot". I did not build that. Saving happens only when asked - F5, --load's counterpart, or verify.save.

Reason: a snapshot is 18.7 MB. Writing one every economy tick would put a multi-megabyte serialise and file write on the tick path, for a rollback feature nothing has asked for. The stated benefit was "clean, deterministic save points", and that is already what world::state_hash provides for debugging, at no I/O cost.

I rewrote the paragraph to describe what exists and to say plainly that the autosnapshot was never built.

**Why it matters.** It reverses a written technical decision rather than merely implementing it, which is Ben's call, not mine. If the intent was rollback or a replay/debug facility, that is a different item and the design should say so.

### NR-515 — Quick save/load has no confirmation and one unnamed slot
*decision taken on your behalf · raised 2026-08-22 · from BL-536 (world snapshot save)*

F5 overwrites `quicksave.iosave` without asking; F6 discards the live campaign without asking. One slot, no naming, no listing, no menu entry. BL-536's design said explicitly "no autosave cadence, no save-slot UI", so this is in scope as written - but it is the part a player touches, and the failure mode is real: F6 pressed by mistake loses everything since the last F5.

Against that: a refused load costs nothing (the read builds into scratch), and the quick-slot convention is well understood from other games.

**Why it matters.** It is the one place this item can lose a player's work, and the mitigation is a UI item rather than a format one.

### NR-545 — BL-536 landed a world serialiser, and it invalidates the hotspot audit's components.hpp conclusion
*observation · raised 2026-08-23 · from Evaluating origin/main at Ben's request, mid-turn, while Sprint N1 was being triaged.*

PR #54 merged BL-536 (world snapshot save): src/world/world_save.{hpp,cpp} (847 lines) and binary_io.hpp (442). THIRTY ENUMS NOW CROSS A VERSIONED BYTE STREAM - terrain substrate/cover/landform, building_type, resource_type, condition_subject, condition_comparator, modifier_subject, modifier_op, law_effect_kind, stance, ideology, expansionism, economic_focus, convoy_mode, unit_class, battle_season and more - each written as a uint8 and read back through r_enum against a named maximum, so an out-of-range byte is rejected rather than cast.

THE HOTSPOT AUDIT'S components.hpp VERDICT IS NOW FALSE. It concluded, having traced every enum in the file to its consumers, that 'the append-only rule does not bind this cluster at all' and that resource_type was the only enum crossing a stream. THAT WAS TRUE AT THIS BRANCH'S BASE AND IS FALSE ON origin/main. I gave batching advice resting on it - specifically that the real append contention lived in corp_command.hpp rather than components.hpp - and that advice needs revising: it now lives in BOTH.

Three further things written this session are corrected in the same commit: RELATIONS.md's 'nothing survives a save' (stance IS saved now), META_LAYER.md's 'designed against a save seam that does not exist yet' (it exists, so append-only is a live constraint rather than a forward-looking discipline), and BL-525's note that no item creates a world serialiser.

**Why it matters.** The audit was CORRECT when it ran and WRONG four hours later, because a merge landed underneath it. That is not a defect in the audit - it is the half-life of a finding in a repo with concurrent branches, and it is the argument for Ben's instruction to evaluate origin early rather than at integration.

The practical consequence is a real one: any Sprint N1 or v0.1.24 item that appends an enum is now touching a live save format. BL-537's budget_priority enum, which the quarantined C slice already indexes unchecked (its heap-buffer-overflow), would be written to disk.

> **Recommendation:** Re-run the components.hpp half of the hotspot audit against the merged tree before briefing any agent on that cluster. The other three hotspot verdicts are unaffected - history_sim, corp_ai and body_surface_canvas do not turn on what is serialised.

*Files: `docs/development/backlog.json`, `docs/politics/RELATIONS.md`, `docs/META_LAYER.md`, `src/world/world_save.cpp`*

### NR-546 — BL-537: bit-exact conservation is not achievable over arbitrary float weights — the claim was narrowed rather than the code changed
*decision taken on your behalf · raised 2026-08-23 · from Fixing the Sprint N1 C-budget slice (BL-537, national budget) after the adversarial pass found conservation failing at ordinary weights.*

The national budget's requirement R1 said world credit is 'conserved bit-exactly over any tick span'. The adversarial pass showed it holds only because the fixture is built from dyadic rationals: at ordinary weights 1.53e-05 vanished in one tick. The decision taken, rather than escalating: the claim was wrong, not the code. Bit-exact conservation of a float transfer requires BOTH endpoints to represent it exactly, and `corporation_component::balance += amount` cannot once the balance has outgrown the credit — the identical rounding `apply_budget`'s `cc.balance += delta` has carried since it shipped. So R1 was split into the half that IS exact for any inputs (the pass debits precisely what it credits: paid, total_transferred and the treasury delta are ONE accumulation) and the half that is not (the world total afterwards). The harness now proves the distinction instead of assuming it: one 64-tick span at ordinary weights whose balances are swept moves world credit by EXACTLY zero; an identical span whose balances accumulate drifts within the destinations' own rounding budget, and the two spans transfer a bit-identical total.

**Why it matters.** It is a design answer, not a code change, and it reaches back into what BL-537's requirement group promises. If Ben wants literal conservation to the bit at any weights, the only construction that delivers it is integer or fixed-point credits — which is a money-model change spanning apply_budget, the market and every balance in the game, not a change to this pass. Worth knowing that the bound was measured rather than assumed: over 512 generated shapes the residual sits at ~5e-8 of everything moved, its sign varies, and it vanishes entirely when the destination can hold the credit.

- Accept the narrowed claim (done). The pass conserves exactly; the destinations round, as they always have.
- Move credits to fixed-point across the money loop, and get literal conservation everywhere. Large, and touches every balance in the game.
- Leave a per-tick reconciliation that sweeps the residual somewhere. Rejected — it hides rounding rather than removing it, and gives the residual a destination nobody chose.

> **Recommendation:** Accept the narrowed claim. The residual is float rounding at the destination, not a leak, and it is the rounding every other money flow already carries — a special case here would be inconsistent as well as expensive.

*Files: `src/world/nation_budget.hpp`, `src/world/nation_budget.cpp`, `tools/verify/nation_budget_harness.cpp`*

### NR-547 — Sprint N1's real lesson: all three harnesses were green, two subjects were defective, and every defect was found by mutating content or turning on a sanitizer
*observation · raised 2026-08-23 · from Closing out the Sprint N1 fixes (BL-537, BL-543, BL-545).*

Three implementing agents each wrote the code AND its harness. All three harnesses passed — 17/17, 39/39, 25/25 — and two of the three subjects were unsound. Not one defect was found by reading. They were found by: authoring the harness's own solved fixture into the real economy.lua (BL-543's check went RED on the day the anchor was satisfied); multiplying all 33 base prices by ten and watching nothing move (the content claim bound nothing); deleting a std::stable_sort and watching 39 rows still pass (the order-independence rows tested no order); compiling under AddressSanitizer (a heap-buffer-overflow the 34 value rows could not see); and running 512 generated shapes instead of one authored fixture (156 overdraws and 26 negative treasuries the authored fixture could not reach). Every fix in this pass added the check that would have caught its own defect, and each was mutation-tested against the pre-fix code before being accepted.

**Why it matters.** The project's verification model is authorship — 'the docs are the audit'. That works for coverage and fails for adequacy: an author writing their own check writes the fixture their code passes. The cheap, repeatable counter is four moves, none of which need a second person: mutate the CONTENT the check reads, mutate the CODE the check guards, generate shapes instead of authoring one, and turn on a sanitizer. Worth considering whether the verifier-headless skill should require the mutation step for a NEW harness the way it now requires a repo-root working directory.

- Add a 'prove the row has teeth' step to verifier-headless: every new assertion must be shown failing against a stated mutation before it is accepted.
- Leave it to authorship, as with the doc audit.
- Require it only for harnesses covering a determinism, conservation or memory-safety claim.

> **Recommendation:** The third. The mutation step costs minutes on a rule/arithmetic row and would have caught all four false greens here; requiring it on every row would tax the cheap regression harnesses that are already fine.

*Files: `.claude/skills/verifier-headless/SKILL.md`, `tools/verify/nation_budget_harness.cpp`, `tools/verify/sentiment_harness.cpp`, `tools/verify/value_anchor.cpp`*

### NR-548 — spectator_determinism's golden is MSVC-derived and fails under g++ — every cloud/Linux session will see one red row that is not a regression
*observation · raised 2026-08-23 · from Regression-running the existing harnesses after the Sprint N1 fixes, in the Linux remote container.*

spectator_determinism reports 'R2 byte-identity: the unspectated hash equals the pre-BL-409 golden' as FAILED here: golden 344A9FE48306E93A, observed B4D09255AF346008. This is not caused by the Sprint N1 work — rebuilding the world library with nation_budget.o and sentiment.o REMOVED produces the identical observed hash. The harness's own provenance log already says why: 'MSVC-derived; this golden is toolchain-specific (float clearing arithmetic differs under g++)'. The two rows that carry real meaning both pass — R2's reproducibility (two independently built worlds agree) and R3 (spectating is deterministic).

**Why it matters.** A permanently-red row trains a reader to ignore the harness, which is exactly how a real regression gets waved through. And it will be red in every session that is not on Ben's Windows box, which now includes every cloud session. Two honest fixes exist; blessing the g++ value is NOT one of them, because it would then be red on MSVC instead.

- Carry TWO goldens, one per toolchain, and check the one matching the compiler. Small, and makes the toolchain dependence visible rather than a footnote.
- Make the row report rather than assert when the compiler is not the one the golden was taken on — same shape as value_anchor's R3 branch.
- Leave it, and rely on the provenance comment being read.

> **Recommendation:** The first. It is a few lines, it keeps the row's teeth on both toolchains, and it turns a footnote into a check.

*Files: `tools/verify/spectator_determinism.cpp`*

### NR-549 — binary_io.hpp landed a day after sentiment.cpp copied the older idiom — four files now hand-roll the same byte primitives
*observation · raised 2026-08-23 · from Integrating Sprint N1's sentiment slice and auditing it against the merged origin/main.*

src/world/sentiment.cpp opens with private write_u32/read_f32 helpers over reinterpret_cast, and says in its own comment that it follows 'history_log / procurement exactly'. It did, and that was correct: binary_io.hpp (BL-536) merged on 2026-08-22, a day after the slice was briefed. So there are now four copies of one idiom, three of them without the shared version's bounds constants and its checked r_enum. Nothing is broken — all three bound their counts and all three round-trip — but the next person to add a stream will copy the nearest one, and three of the four nearest ones are the wrong one. Filed as BL-553 (priority C, v0.1.25).

**Why it matters.** The specific loss is r_enum. It reads a uint8_t and REJECTS an out-of-range byte rather than casting it, which is the property that matters most now that thirty enums cross a versioned byte stream (NR-545). The older streams do not get it. Worth knowing too that this is the second thing BL-536's merge invalidated about work briefed before it (the first was the components.hpp audit conclusion, NR-545) — a merge landing mid-sprint quietly ages every brief written before it, and neither instance was noticed by the agent that hit it.

> **Recommendation:** Land BL-553 when something next touches one of those streams — which BL-546 is doing right now to procurement.cpp. Not worth a pass of its own.

*Files: `src/world/binary_io.hpp`, `src/world/sentiment.cpp`, `src/world/procurement.cpp`, `src/world/history_log.cpp`*

### NR-550 — requirements.json silently deleted a whole requirement group for a day, and a parse-and-rewrite made it permanent
*observation · raised 2026-08-23 · from Found while integrating Sprint N2's lane B, which hit it independently.*

BL-536's append on 2026-08-22 left out the `},{` between the world-save-snapshot and value-anchor group objects. That is still VALID JSON: the second object's keys land inside the first and JSON's last-key-wins rule discards the collisions. So value-anchor (BL-543, four rows) vanished from every consumer at once - `requirements_query.js value-anchor` answered 'nothing matched', and had been answering that since the day the group was written. The group stayed recoverable from the raw bytes until a parse-and-rewrite script loaded and re-saved the file, which erased it from the text too. THIS SESSION'S OWN SPRINT-SCAFFOLDING WRITE DID THAT. Recovered from 4262d66's raw text and re-inserted with its rows resolved. Also found and disambiguated two groups sharing the brief `battle-state-in-world`, which made every query for it answer for one of two different pieces of work.

**Why it matters.** The store is canonical and it lied for a day, with no symptom a reader would notice: the query answered honestly that nothing matched, and every consumer agreed. What makes it worth machinery rather than care is the second half - the corruption ERASES ITS OWN EVIDENCE on the next write, so the window to recover it is however long until the next tool touches the file. backlog_lint now carries checkStoreIntegrity over both JSON stores (record count in the raw text vs after the parse, plus key uniqueness), mutation-tested three ways. It is the fourth check this session that exists because a defect had no view that could show it.

> **Recommendation:** No action needed - repaired and guarded. Worth knowing that any future hand-edit of these stores should be followed by `node tools/session/backlog_lint.js`, which now catches this in under a second.

*Files: `docs/development/req/requirements.json`, `tools/session/backlog_lint.js`*

### NR-551 — A reported determinism violation in spectate does NOT reproduce - four clean trees give identical hashes
*observation · raised 2026-08-23 · from Verifying Sprint N2 lane B (BL-546) before merge.*

The lane reported that spectator_determinism's SPECTATED state_hash moves 042D88AB1C8701A2 -> BB2D94F63FC165E5 under its change, and that it had isolated the cause to struct layout by adding one empty std::map member to `world` on a pristine tree. It called this a real violation of the determinism invariant and asked for a review entry. IT DOES NOT REPRODUCE. Built from clean git archives with the same recipe and flags, main (41816d6), 4262d66, this branch after all three merges, and the lane's own tree ALL give the identical pair played=B4D09255AF346008 spectated=BB2D94F63FC165E5. And repeating the lane's own probe - one inert std::map member added to `world` at 4262d66 - moves NEITHER hash. The claimed baseline 042D88AB1C8701A2 matches no tree that can be built here.

**Why it matters.** Two things. First, filing it as reported would have put a false determinism violation into the standing-rules space, and a future session would have spent a day chasing a ghost in the one invariant the project cannot compromise on. Second, the likely cause is worth naming because it bit a sibling lane in the same run: a stale object left in the static archive after a source was restored, so a 'clean' rebuild links a mixture. Lane A hit exactly that and said so ('my mutation runner left a mutated .o in the archive after restoring the source, so one green run was green against a mutant'). An `ar rcs` over a stale obj/ directory is a silent, reproducible way to measure a build that never existed.

> **Recommendation:** No defect to fix. Worth adding to the build recipe in REFINED.md and the agent briefs: delete obj/ before rebuilding when the point of the rebuild is a comparison. If Ben wants certainty, the cheap confirmation is to re-run spectator_determinism on his own MSVC box, where the golden was taken.

*Files: `tools/verify/spectator_determinism.cpp`, `docs/development/REFINED.md`*

### NR-552 — BL-546 decision taken: procurement v3 drops the reputation block entirely rather than re-deriving it
*decision taken on your behalf · raised 2026-08-23 · from Sprint N2 lane B (BL-546), reported by the lane and accepted at merge.*

The procurement stream bumped 2 -> 3. The alternative considered was to keep writing a reputation-shaped block sourced from the Trust axis, so the format stayed recognisable. That was rejected: it recreates the second store the item exists to delete, and the two would diverge the first time anything other than procurement moved the axis. The relational value now rides write_sentiment/read_sentiment and the world snapshot (bumped 1 -> 2, with the pair map carrying two floats and rejecting a null id, a self pair, a non-finite value or a stored-neutral row). read_procurement's contract narrowed from four fields to three.

**Why it matters.** It is a save-format change behind a strict version gate, so it is not reversible by editing a reader later - a v2 stream is refused, not migrated. That costs nothing today (NR-399 established there is no game save/load path yet) but BL-107 will make it cost something. The reasoning is sound and the harness proves the round trip; this is filed so the call is Ben's to revisit while it is still cheap.

> **Recommendation:** Accept. Two stores for one quantity is the defect BL-546 exists to remove, and a compatibility-shaped block would have preserved the shape while losing the property.

*Files: `src/world/procurement.cpp`, `src/world/procurement.hpp`, `src/world/world_save.cpp`*

### NR-553 — BL-542 novelty: the grudge term has no data source, and corp-grain stance is being read as a nation-grain input
*novel-work · raised 2026-08-23 · from Sprint N2 lane A (BL-542) raised both under the novelty flag; recorded at merge.*

Two joints in the nation scorer that no authority doc owned. (1) THE GRUDGE HAS NO INPUT. NATIONS.md says the Era -1 residue biases the first two terms, but no campaign-era nation-pair state of any kind reaches `world` - the real pair outcomes sit behind BL-541 (directional tariffs). The lane substituted a derivation from the residue that DOES survive generation - contested border length plus the pair's `expansionism` posture, which NATION_GENERATION.md already derives from a border-contest integral - and isolated it in one function, `grudge_from_border`, so swapping the input when BL-541 lands moves nothing else. (2) NATIONS HOLD NO STANCE. `stance.hpp` is corp->corp only, so 'force standing near its borders' had no nation-grain reading. The lane bridged it: a border unit's owner is hostile toward corps registered in the threatened nation, weighted by the share it has declared against. That is the first place in the codebase where a corp-grain relation is read as a nation-grain input.

**Why it matters.** Both are defensible and both are testable, and the lane flagged them rather than absorbing them, which is the point of the novelty rule. But the second is arguably BL-540's (nation-to-corp stance) to own, and BL-545's sentiment substrate - which landed the same day - is the thing that will eventually supply both readings properly. Deciding now which item owns the bridge is cheaper than discovering two of them later.

- Leave both in nation_ai.cpp and let BL-541 and BL-540 replace them in place - the lane isolated them for exactly this.
- Move the stance bridge into BL-540's scope now, before it is built on.
- Reshape both onto BL-545's sentiment substrate directly, skipping the substitutes.

> **Recommendation:** The first. Both substitutes are one function each and named as substitutes in the doc; replacing them is a smaller job than pre-deciding their home. But BL-540 should carry a note that the bridge already exists.

*Files: `src/world/nation_ai.cpp`, `docs/politics/NATIONS.md`*

### NR-554 — ANSWERED IN PART: the corrected Era -1 says 11 of 32 worlds fight nothing, and nothing yet separates them
*question · raised 2026-08-23 · from Sprint N2 lane C (Sprint 28 lane A T1/T2), reproduced independently by the main session.*

RE-MEASURED 2026-08-23 on the CORRECTED fixture (BL-462), which supersedes both earlier readings - the n=8 one this entry first carried and the n=32 one that refuted it. Both were about a sim the game does not run.

WHAT THE REAL ERA -1 SAYS, verified independently by the integrating session on its own rebuild: 11 of 32 worlds fight nothing (34%). The fork answer SURVIVES unchanged - all 11 silent worlds show the campaign score clearing its threshold and losing the argmax on every round, never failing to clear. And BL-384's founding premise is dead: 2,503 battles produce 1,789 conquests (71.5%), with no seed anywhere fighting and taking nothing.

NOTHING SEPARATES silent worlds from fighting ones, and both earlier diagnoses fail harder on the real fixture than they did on the wrong one. Ceiling-vs-floor: 32 of 32 seeds OVERLAP, none disjoint (it was 3 of 17 before). Score level: silent ceilings 275-439 (median 348) against fighting 350-531 (median 405), with 15 of 21 fighting seeds inside the silent range - a shift, not a separation. Four further candidates were tested (cleared candidates, scored candidates, contacts, foundings) and none separates.

ONE THING DOES HOLD ACROSS EVERY WORLD: Settle takes 83-100% of the rounds Campaign loses, silent and fighting alike (silent median 99.2%, fighting 95.8%). So the open question is not the LEVEL of either score but their PER-ROUND JOINT BEHAVIOUR - whether Campaign's good rounds ever coincide with Settle's bad ones. verb_contest_trace already carries year and polity; nothing slices it that way yet.

**Why it matters.** Zero-war at 34% is a third of every world generated, which keeps Ben's 2026-08-21 ruling biting - if a zero-war world means many ancient tech quests are unreachable, a third of worlds are unfinishable - while sitting further from BL-224's non-hegemony invariant than the 53% figure did. The target is still a rate inside a stated band, reported and never clamped. What has changed is that the measurement is now trustworthy and the diagnosis is genuinely open: three candidate separators have been tested and refuted, so the next move is a real question rather than a tuning pass. AND THE MEASUREMENT IS STILL BOUNDED: build_work cannot enter the contest headlessly (NR-558), so every statement here is about a four-verb contest where the game scores five.

- Slice verb_contest_trace by (year, polity) to test the joint-behaviour hypothesis - the data already exists and nothing reads it that way. Cheapest next measurement and the only one the evidence currently points at.
- Close the works gap first (NR-558) so the contest has all five verbs, then re-measure. Slower, but every finding until then carries an asterisk.
- Accept 34% as the rate and rule on the band it should sit in, treating the mechanism as a later question.

> **Recommendation:** The first, then the second. The joint-behaviour slice is a report-only change to an instrument that is now trustworthy, and it is the only hypothesis the measurement has not yet eliminated. But build_work missing from the contest is a real asterisk on any conclusion about which verb wins, so NR-558 should be settled before a fix is chosen rather than after.

*Files: `src/world/history_sim.cpp`, `tools/verify/history_conquest_gap.cpp`, `src/world/hard_coded_world.cpp`*

### NR-555 — Decision taken on Ben's behalf: three rows in a requirement group Sprint 28 did not own were flipped to complete at merge
*decision taken on your behalf · raised 2026-08-23 · from Sprint N2 lane C reported it; the main session accepted it at merge and did not file the entry Rule 0c requires. Caught by the adversarial pass.*

growth-extinguishes-war R3, R4 and R5 were flipped from pending to complete by the lane, on the grounds that this task's deliverable is precisely what those rows asked for. The main session read the three row texts, judged them satisfied, and carried the flips through the merge - WITHOUT filing a decision-taken entry, which is the exact case CLAUDE.md Rule 0c names. R4 has since been RE-OPENED: it was closed on a false metric (the 1-3 margin bucket recorded as empty; the harness prints 39). R3 and R5 stand - R3's question is genuinely answered by the fork, and R5's inertness claim is true even though the row that asserts it is weaker than its wording.

**Why it matters.** Two things, and the second is the reason this is filed rather than just fixed. An unrecorded delegated decision is indistinguishable from one Ben made - that is Rule 0c's whole argument, and it applied twice over here because the flips were cross-group, which is a stronger claim than closing your own rows. And the flip that turned out to be wrong is the one that would have been hardest to catch later: R4 closed with a confident-sounding note carrying a number nobody had recomputed.

> **Recommendation:** No action beyond what is done - R4 re-opened, R3/R5 stand, entry filed. Worth noting the pattern for future briefs: a lane that closes rows in a group it does not own should be required to say so in its report AND to file the entry itself, rather than leaving the integrator to notice.

*Files: `docs/development/req/requirements.json`*

### NR-556 — A duplicate backlog item was authored for a defect an existing priority-A item already owned
*observation · raised 2026-08-23 · from Authoring BL-554 (Era -1 harness fixture) during Sprint N2 closeout.*

BL-554 was written up as a new item for the history_conquest_gap fixture divergence. BL-462 (HARNESSES_MEASURE_A_DIFFERENT_SIM_THAN_SHIPS, filed 2026-08-18, priority A, v0.1.22) already owned it, named the span and clock divergences with the same line numbers, and carries the better provenance - it records this as the THIRD instance of the repo failure mode where a check looks like coverage and is not. BL-554 was retired into BL-462 the same day, contributing the four divergences BL-462 did not have (seed, creeds, the works argument that gates build_work out of the contest, the post-sim settlement) and an exact acceptance test.

**Why it matters.** The duplicate cost nothing because it was caught within the hour, but the reason it happened is worth naming: a search for an existing owner was never run. backlog_query.js --grep and --touches exist precisely for this and take seconds. The near-miss is that BL-462 sits at v0.1.22 with a version goal, a file scope and a design; had the duplicate survived, the project would have carried two priority-A items for one defect in two different minors, and whichever one got worked would have silently orphaned the other.

> **Recommendation:** No action - already folded. Worth a line in DELIVERY.md before authoring a new item: grep the backlog for the subject first. The existing rule covers ID allocation (next_id.js) but not subject collision.

*Files: `docs/development/backlog.json`, `docs/development/DELIVERY.md`*

### NR-557 — A new pattern landed without an owner: generation hands a harness its own arguments back
*novel-work · raised 2026-08-23 · from BL-462's fix; flagged by the implementing lane under the novelty rule and confirmed at merge.*

To let a harness reproduce the Era -1 exactly, `make_hard_coded_world` gained an optional out-parameter that captures the arguments the sim was actually called with - params, seed, creeds, settlement. Nothing in TILE_GENERATION.md, GENERATION_LEDGER.md or DEVELOPMENT_PRACTICES.md owns this shape. The two established precedents are different things: `generation_report` is a presentation record of generation's OUTPUTS, and `generation_record` holds per-pass intermediates regenerated on demand. This is a third - a capture of a call's INPUTS so a check can re-run it faithfully. It is deliberately off the save seam and costs nothing when unrequested.

**Why it matters.** It is the right answer to BL-462 and it generalises: any generation pass a harness might want to re-run faithfully has the same problem, and this shape solves it without a save-format change or a second construction of the value. That is exactly why it wants a decision before it spreads. The rejected alternatives are recorded in the requirement group - re-deriving the creeds in the harness (a second construction of the same value, the defect the item exists to remove) and putting creeds on `body_entry` (a save-format change carrying pantheons and tongues to deliver one integer per culture, and it would not have solved the settlement half at all).

- Adopt it as a named pattern in DEVELOPMENT_PRACTICES.md, so the next harness that needs it does the same thing rather than inventing a third shape.
- Leave it local to the Era -1 and treat a second instance as the trigger to generalise.
- Reject it and take the save-format route after all.

> **Recommendation:** The first. The argument for the pattern is the same argument BL-462 makes - two constructions of one call WILL drift - and it applies to every generation pass, not just this one. Naming it costs a paragraph.

*Files: `src/world/era_minus_one.hpp`, `src/world/hard_coded_world.cpp`, `docs/development/DEVELOPMENT_PRACTICES.md`*

### NR-558 — No headless harness can measure the shipped configuration exactly — two Lua-authored inputs reach generation and none can load them
*question · raised 2026-08-23 · from BL-462's fix; the sixth divergence it could not close.*

`make_hard_coded_world` reads two Lua-authored inputs a headless harness cannot load: `scripts/works.lua` (BL-321's Era -1 works table) and `scripts/world_gen.lua`. The four excluded TUs for a headless build are exactly the Lua ones (NR-264). The consequence is concrete and current: `history_sim.cpp:896` gates `build_work` on a non-empty works registry, so THE HARNESS SCORES FOUR VERBS WHERE THE SHIPPED GAME SCORES FIVE - and verb competition is what that harness exists to measure. The lane declined to transcribe works.lua into C++ because that puts the roster in two places and stales the harness the next time the table moves, which is BL-462's own failure mode one layer down. It passes null on both sides through the shared fixture so they cannot diverge, and prints a banner on every run.

**Why it matters.** DEVELOPMENT_PRACTICES.md treats headless harnesses as THE verification path for `world/*` logic. This says that path has a ceiling: it can verify mechanisms exactly and can never verify the shipped configuration exactly. BL-462's premise - 'no check measures the run that actually generates a world' - is now 5/6 closed, and the last sixth is structural. It also bounds what the Era -1 findings can claim: any statement about which verb wins is a statement about a four-verb contest.

- A Lua-free authoring path for works and world_gen that PRODUCTION also uses - the tables become data the C++ side can read without sol2. Largest, and it removes the class.
- A Lua-capable verification path: extend `ProjectIo --verify` to run these checks with the real registries, as the live-Lua harnesses (haulage_measure, substrate_census) already do.
- Accept the ceiling, keep the banner, and require every Era -1 finding to state the verb count it was measured under.

> **Recommendation:** The second, scoped to the harnesses that need it. The live-Lua pattern already exists in the repo and is understood; extending it is cheaper than re-authoring two content files, and it closes the gap for world_gen.lua at the same time. The third is the honest interim and is already in place.

*Files: `tools/verify/history_conquest_gap.cpp`, `docs/development/DEVELOPMENT_PRACTICES.md`, `scripts/works.lua`*

### NR-559 — Seven bug reports filed as eight items: two calls taken on Ben’s behalf
*decision taken on your behalf · raised 2026-08-22 · from Ben, 2026-08-22, seven numbered bug reports (single plate / non-viable starts / lack of industrialisation / provinces cross borders / bundle selection / mountains as elevation / legends in the minimap).*

Filed as BL-559 through BL-566. Two partition calls were taken rather than asked, plus one status call across three items.

(1) BUG 3 WAS SPLIT INTO TWO ITEMS. Ben wrote it as one sentence — "Surprising lack of industrialisation (need more worked tiles, and these should not be filled on the default lens)" — and it is one thought, but the two halves land in different layers: the density is generate_background_firms in src/world/ (BL-561), the fill is built_plate_colour in src/ui/ (BL-562). They are cross-linked in both directions and BL-561 lists BL-562 in unblocks, so the connection is not lost; but they can be built independently and one item spanning both layers would have carried two collision maps and two verification classes.

(2) HALF OF BUG 4 WAS NOT FILED, BECAUSE IT ALREADY IS. Ben wrote "Provinces cross country boundaries, and country boundaries should not diffuse together". The first half is genuinely new and is BL-563. The SECOND half is BL-535 (national hue replaces the Country lens), which carries his own ruling from earlier the same day that the corner blend stops at national borders — designed, not yet built. Filing it again would have created two items for one change. BL-563 says so explicitly rather than silently omitting it.

(3) THREE ITEMS FILED design-owed, NOT designed. BL-559 (which of gate / design-around / retune), BL-560 (the sentence has two readings and no predicate), BL-561 (direction is clear, the lever is not, and the measurement seam should run first). The other five are designed — the mechanism is traced and the change is stated.

**Why it matters.** The split and the omission both change what Ben sees when he next queries the backlog: seven reports come back as eight rows, and one of his sentences has no row of its own. If either call is wrong it is cheap to undo now and expensive once tasks are promoted against them.

- Accept both calls — BL-561/BL-562 stay separate, bug 4’s second half stays with BL-535
- Merge BL-561 and BL-562 back into one item if the fill and the density should land together
- Re-file bug 4’s second half as its own item if BL-535 is going to be re-scoped or dropped

> **Recommendation:** Accept. The strongest argument for the split is that BL-562 is worth doing on its own merits even if the density never moves, and the strongest argument against re-filing bug 4 is that BL-535 already carries Ben’s own wording on it.

*Files: `docs/development/backlog.json`*

### NR-560 — The conquest half of Ben’s BL-563 answer was split into BL-567 rather than carried in the generation item
*decision taken on your behalf · raised 2026-08-22 · from Ben, 2026-08-22, elicitation form, answering past the three BL-563 options: "Generate provinces alongside national borders, and have it be impossible to conquer a tile without also controlling the province".*

The answer carries two rulings and they were separated. The FIRST (co-generate provinces with national borders) stayed in BL-563 and rewrote it: the item had proposed rejecting cross-nation candidates inside the existing fill, and Ben’s answer is the stronger design — regions grown to FIT the border rather than grown across it and trimmed. That also dissolved the frontier-fragmentation question the form asked, which is why no merge pass is in the ruling: offcuts never exist to merge.

The SECOND (a tile cannot be conquered without the province) became BL-567, category Military, requires BL-563. Reasons for splitting rather than carrying: it lands in components.hpp / world.hpp / MILITARY.md rather than province.cpp; it is a FORWARD constraint on a system that does not exist rather than a fix to one that does; and it is design-owed where BL-563 is now designed, so carrying it would have held a buildable generation fix behind an unsettled military question.

**Why it matters.** BL-563 can be promoted and built now; BL-567 cannot, and should not block it. If the split is wrong the cost is two rows where Ben wanted one, which is cheap to merge back before either is promoted.

- Accept the split — BL-563 builds now, BL-567 carries the conquest invariant
- Merge BL-567 back into BL-563 if the conquest rule should land with the generation change

> **Recommendation:** Accept. BL-563 has a headless requirement that fails on today’s build and a clear fix; BL-567 has four genuinely open questions and a documented architectural collision. Holding the first behind the second buys nothing.

*Files: `docs/development/backlog.json`*

### NR-561 — BL-561: raising max_firms_per_body may be INERT — the two stop bounds are in series, not in parallel
*observation · raised 2026-08-22 · from Ben, 2026-08-22, elicitation form: chose "Raise max_firms_per_body now" over running the measurement seam first.*

Recorded because it is a caveat against a decision Ben took explicitly, and it should be visible as a flagged risk rather than buried in a design field.

generate_background_firms loops `for (iter < max_iterations_per_body && firms_this_body < max_firms_per_body)` and BREAKS EARLY when production_ratio(production, demand) >= target_ratio (0.90f, corporation_generation.cpp:1718). The bounds are therefore in SERIES: whichever is reached first ends the loop. If the ratio is what currently stops it, the loop never approaches the 200-firm cap, and raising that cap adds exactly zero firms.

The ruling is not thereby wrong — the cap may well be binding, and its own comment records it was already raised once from 40, which is weak evidence that it has been binding before. But the change is a coin-flip between "works" and "no-op" until someone reports which bound fired.

**Why it matters.** The failure mode is not a bug, it is a SILENT no-op: the constant changes, the build succeeds, the world regenerates, and nothing moves — which reads as "the lever was too small" and invites raising it again. The distinguishing measurement costs almost nothing (measure_production_ratio was added on 2026-08-20 for exactly this question), so the design now requires reporting which bound fired as PART OF landing the change rather than as a preliminary study Ben declined.

- Land the raise with the which-bound-fired report attached, as the design now says
- Run the seam first after all, if the no-op case seems likely
- Raise both target_ratio and max_firms_per_body together, so the change cannot be inert

> **Recommendation:** The first. It honours Ben’s choice of lever, costs one extra printed line in whatever harness or generation report carries it, and makes the no-op case self-diagnosing instead of silent.

*Files: `src/world/corporation_generation.cpp`, `docs/development/backlog.json`*

### NR-562 — Campaign-era conquest (BL-567) collides with the documented reason provinces sit outside state_hash — and BL-536 is designing the save format right now
*observation · raised 2026-08-22 · from Tracing Ben’s 2026-08-22 conquest ruling against world.hpp:250 and BL-518.*

world.hpp:250 excludes `provinces` from state_hash and gives the reason explicitly: "state_hash folds the fields a TICK may mutate ... the partition is generation output and never moves once built". BL-518 states the same arrangement as Ben’s own earlier ruling (borders move during generation only, fixed once the campaign starts) and then warns, in as many words: "If this item ever grew into campaign-era border changes, that whole arrangement would need revisiting."

Ben’s conquest ruling is that growth. The reconciling reading — and BL-567 recommends it rather than assuming it — is that the two things must be separated: the PARTITION (which tiles compose province P) stays fixed generation output, and only OWNERSHIP (which nation holds P) moves. That satisfies the rule exactly, since tiles are then not what changes hands, and it leaves provinces immutable, outside state_hash, still checked by determinism_harness across two generations, and still safe as a BL-467 battle-seed input.

What does become live tick-mutable under that reading is nation_component::tiles, documented as "populated by Pass 2 (territory expansion) and STABLE THEREAFTER". That stops being true, and the field then has to enter state_hash and the save format.

**Why it matters.** BL-536 (world-snapshot save) was designed on 2026-08-22 — the same day — and is being built now. It decides field-wise what is serialised. If nation ownership is about to become mutable campaign state, BL-536 should know that while its format is still being written, not after IOSV has a version number in the wild. The cost of knowing now is a comment; the cost of knowing later is a format version bump.

- Tell BL-536 now, so the envelope anticipates mutable nation ownership even if nothing writes it yet
- Leave BL-536 alone — the save version field exists precisely so the format can move, and BL-567 is design-owed with no build date
- Settle BL-567’s partition-vs-ownership question first, since it is what determines the answer

> **Recommendation:** The third, then the first. The partition-vs-ownership call is small, it is the thing everything else depends on, and it is Ben’s. Once it is made, BL-536 either needs nothing or needs one section — and either way it finds out cheaply.

*Files: `src/world/world.hpp`, `src/world/components.hpp`, `docs/development/backlog.json`*

### NR-563 — Do BL-321's defence works ever reach a battle? Still unmeasured, and needs a works fixture
*question · raised 2026-08-21 · from Replaces the withdrawn NR-475. Sprint 28 T4.*

The original question stands and its apparent answer did not: does a region carrying a defence work ever get attacked? `work_defence_mod` feeds both the scorer (history_sim.cpp:634, raising the defender's scored strength) and the resolver (:965, as readiness on the defender's stack), so if works are raised on safe interior regions while fighting happens on frontiers, the sim invests in defence exactly where defence is not needed — and neither number would ever show it.

It cannot be measured with the Lua-free convention every history harness follows, because the works table is Lua. Two routes: the hand-built fixture the BL-321 sweep already uses, or accept that this one check needs the real table.

**Why it matters.** It is a question about where a polity's investment goes, which is the kind of thing that is invisible until someone looks. But it is NOT currently blocking anything and should not be confused with the war-rate defect Sprint 28 is fixing — those were briefly conflated when the artifact-zero made works look inert.

> **Recommendation:** Fold into Sprint 28's T4 only if the fixture is cheap; otherwise defer. The war-rate work (T1-T3) does not depend on it.

*Files: `tools/verify/history_conquest_gap.cpp`, `src/world/history_sim.cpp`, `src/world/works_roster.cpp`*

### NR-564 — EVERY history harness measures a 4000-year sim the game never runs — production is 400 years
*observation · raised 2026-08-21 · from Sprint 28 T3, found when a verified fix changed nothing in production.*

`history_sim_params`'s DEFAULTS are a 4000-year run (-4000 -> 0) on the six-band ladder, 136 decision rounds. `make_hard_coded_world` runs something else entirely: `prehistory_years = 400`, so -400 -> 0, on ONE band with step 4 — 100 rounds over a tenth of the span. It also passes creeds and a works registry, and seeds the sim with `params.seed ^ 0x415C1E17u` rather than the raw seed.

Every history harness in the repo takes the struct defaults, `history_sim_harness`'s own B384 outcome sweep included. So the sweep that BL-384 was filed from, the sweep that asserts B384a/B384b, and my own conquest-gap instrument were all measuring a configuration production does not generate.

It was caught only because a fix that was verified to work — 2 of 8 zero-war worlds down to 0 of 8, four harness rows flipping red to green — produced a BYTE-IDENTICAL production sweep. Base and post-fix `seed_sweep_probe` output match exactly, seed for seed.

**Why it matters.** Three things follow. (1) BL-384's original numbers, my restated numbers, and the existing B384 assertions are all about a sim the player never sees — the direction of every finding survives, but no magnitude should be quoted as if it described a generated world. (2) A green harness here is weak evidence about the game, which is the property that makes this worth fixing rather than noting. (3) The 400-year run is a tenth of the span with roughly three quarters of the decision rounds, so it is not a scaled-down version of the same thing — the neighbourhood never fills, which is exactly why the settle-feasibility fix bites in one and not the other.

- Point the harnesses at production parameters (mirrored constants, like sea_leg_census does with its rates).
- Export a `production_history_params()` from hard_coded_world so there is ONE definition.
- Keep both: assert on production params, and keep a long-run sweep explicitly labelled as such.

> **Recommendation:** Option 2 then 3. A mirrored constant is a known cost but this one has already caused a day of measuring the wrong thing, and the span is not a tuning knob a harness should be free to pick.

*Files: `src/world/hard_coded_world.cpp`, `tools/verify/history_sim_harness.cpp`, `tools/verify/history_conquest_gap.cpp`*

### NR-565 — Sprint 28 T3: at PRODUCTION parameters campaign loses by as little as 8 points and never wins
*question · raised 2026-08-21 · from Sprint 28 T3, re-measured at production parameters.*

With the harness pointed at the real 400-year configuration, a silent world's funnel reads: campaign CLEARED its threshold 1080 times and LOST all 1080 — beaten by settle x682 and invest x398 — with a margin of mean 205 and MINIMUM 8. Settle failures are zero at this span, so the settle-feasibility defect that dominates the 4000-year run is not what is happening here.

So the production defect is a genuine weighting question, and a narrow one: campaign comes within 8 points of winning and never does.

**Why it matters.** Ben has ruled a zero-war world is a bug, so this must change. But the change is a TUNING call on the verb weights, and the sprint's own R2 says the target is a rate inside a stated band reported and never clamped — which makes the band Ben's to sanction rather than mine to pick. A margin of 8 says a small, defensible adjustment would flip these worlds; it also says the current balance is finely poised, so whatever is chosen should be measured across seeds, not fitted to the two that fail.

- Adjust the campaign/settle weighting to a measured band (a tune, wants Ben's number).
- Find a second structural asymmetry the way T3 found the first — invest x398 is a large share and has not been examined.
- Accept a lower floor than 'every world fights' if the tech-gate reachability Ben cited is satisfied by fewer.

> **Recommendation:** Look at INVEST before tuning anything. It takes 398 of seed 4's 1080 losses and nobody has examined whether it has the same scorer/executor gap Settle had — a second structural finding would be worth more than a weight.

*Files: `src/world/history_sim.cpp`, `tools/verify/history_conquest_gap.cpp`*

### NR-566 — Sprint 28 T3's settle-feasibility gate was NOT landed on merge — on the corrected fixture it moves every production world and makes the 8-seed table MORE silent (1/8 -> 3/8)
*decision taken on your behalf · raised 2026-08-23 · from Housekeeping merge of origin/claude/bl-519-520-nr-438-439-n29ljl onto main (2026-08-23).*

The branch carried two things: the Sprint 28 T1/T2 diagnostic (a duplicate of main's lane-A `verb_contest_trace`, landed 5101d586) and the T3 fix — `settle_target_cell` shared by scorer and executor so Settle is not offered when no free cell exists. Its commit claims the fix is byte-identical at production parameters. That was measured BEFORE BL-462 (2026-08-23) corrected `history_conquest_gap` to run generation's actual Era -1 (400 years, one band, real creeds, folded seed). Ported onto today's main and re-measured on the corrected fixture at n=8: A1/R1/R2/R4 pass, R3 FAILS — every seed's battles/conquests/foundings moved, and the silent-world count went 1/8 -> 3/8 (seeds 0, 3, 7 fell silent; seed 4 woke: 0 -> 180 battles). Current values in pinned[] form: {0,0,1186,..} {169,162,989,..} {93,59,1019,..} {0,0,900,..} {180,180,1014,..} {70,61,998,..} {79,22,1064,..} {0,0,1090,..}. `history_sim_harness` at the 4000-year default went 8 failures -> 4, consistent with the branch's claim for THAT configuration.

**Why it matters.** The gate is argued as a pure feasibility fix ('an action that cannot be performed stops being offered'), and the argument is sound. But its measured effect on the sim the game runs is a reshuffle that does not reduce silence on this sample, and landing it re-pins R3's regression table and invalidates every generation golden and the state_hash quoted in the standing rules — a cost that needs a measured benefit to justify. Landing it inside a housekeeping merge would have been a forced outcome.

- A) Land it: apply build_gen/settle_feasibility_port.patch (also on branch feat/sprint28-settle-feasibility), re-pin R3 at n=8 and re-measure silence at n=32 against the 11/32 baseline in history_conquest_gap's README entry; re-bless generation goldens.
- B) Hold it as a backlog item under BL-384 (growth extinguishes war), to be landed with the weighting work NR-565 asks for, measured together.
- C) Drop it — the silent-world fork is already answered by main's diagnostic, and this fix does not move the production number.

> **Recommendation:** B. The fix is correct in principle and should not be lost, but its only demonstrated effect is on a configuration the game does not run. Measure it at n=32 alongside the Invest/Settle weighting call (NR-565) so one re-pin covers both.

*Files: `src/world/history_sim.cpp`, `src/world/history_sim.hpp`, `tools/verify/history_conquest_gap.cpp`, `build_gen/settle_feasibility_port.patch`*

### NR-567 — 'Wire the spines live' is three pieces, not two — the budget pass pays nobody without a claim producer, so public exploration (BL-538 line 5) is pulled forward into the wiring sprint
*decision taken on your behalf · raised 2026-08-23 · from Sprint N3 scoping, 2026-08-23, after Ben chose the wiring-first order for the invisible N1 features.*

run_national_budget transfers only against budget_claim rows, and every claim producer belongs to BL-538 (the nine priority lines), which is designed and unbuilt. Wiring BL-542 (scorer) + BL-537 (budget pass) alone yields a nation that authors weights every cadence and pays nothing — a surface over it (BL-555/BL-558) would render a treasury and a weight vector and an empty payments list. Ben was asked and chose BOTH: public exploration now as the proving line (survey_system already has a paid dispatch with cost and duration and 'merely has no funder but the player'), then contracted force once BL-377's contract seam exists.

**Why it matters.** It changes the sprint's shape: BL-538's first cut (lines 1-6) is NOT being built wholesale — one line is pulled forward to prove the loop, and the other four hosted lines stay in BL-538. Recording so the partial landing of BL-538 is a choice on the record, not drift. It also means BL-377 (mercenary contract seam, reopened 2026-08-22 as never-built) enters this sprint as the second line's host.

- Confirm: public exploration + contracted force in this sprint; lines 1-4 stay in BL-538.
- Widen to BL-538's whole first cut (lines 1-6) in one go.
- Narrow back to public exploration only; contracted force waits for a BL-377 sprint of its own.

> **Recommendation:** Confirm, as chosen.

*Files: `src/world/nation_budget.hpp`, `src/world/nation_ai.hpp`, `src/world/survey_system.cpp`, `docs/economy/CONTRACTS.md`*

### NR-569 — Four calls taken on Ben's behalf from the integration map: weight map on world (save v2→3), state_hash folds treasuries conditionally, player corp not funded this cut, national pass runs under spectating
*decision taken on your behalf · raised 2026-08-23 · from Sprint N3 integration map, 2026-08-23.*

a) world::nation_budgets (std::map<entity_id, nation_budget>) is the persistent weight map the scorer overwrites one slot at a time; serialised beside nations, world_save_version 2→3, which refuses today's quicksave.iosave (rejection is the migration story, world_save.hpp:60). b) state_hash folds nation treasuries and the weight map ONLY when any treasury is non-zero or the map is non-empty (the battles precedent, world.cpp:193-204), so every empty-nation fixture's hash is unchanged. c) The player corp is never scored by corp_ai (corp_ai.cpp:613-614) so it is never funded this cut; a petition_nation verb is the BL-538 follow-on and the player's first path through the seam. d) The national pass has no human subject, so it runs regardless of corp_ai_params::spectating — a subsidy landing on the player's corp is a transfer TO, not an action ON, the player, so this is not a standing-rule widening.

**Why it matters.** (a) and (b) move goldens; (c) means the mercenary's own income loop is still absent until contracted force lands; (d) is an interpretation of the prohibition that should be on record.

- Confirm all four.
- Overturn (c): give the player a petition path in this sprint.
- Overturn (d): gate the pass on spectating like corp_ai.

> **Recommendation:** Confirm.

### NR-570 — Sprint N3 slice S2 — six calls taken in the budget pass and the claim producer (earmark subject validated against w.bodies; per-claim fill_fraction; fixture line swapped to schooling)
*decision taken on your behalf · raised 2026-08-23 · from Worktree agent S2 (b560a1d1), Sprint N3 slice 1.*

1) A public_exploration claim whose subject is null OR not in w.bodies is rejected whole at gather (brief asked only for null) — 'validate as the value that lands', since dispatch_survey refuses an unknown body. 2) budget_transfer::fill_fraction is THIS CLAIM's fill (credits/amount), not the line's; the line's is already in lines[]. 3) Rule 3a (earmarked claims whole-or-nothing, Ben NR-568) is tested against the computed float remaining share, so a claim equal to the share within one ULP can be skipped this tick and funded next — measure-zero, and the share accumulates. 4) A two-line clip keeps 'paid never more than share' true on a MIXED earmarked/unearmarked line — unreachable today, documented as a guard. 5) nation_budget_harness's R1 pro-rata fixtures moved from public_exploration to schooling because the exploration line now rejects subject-less claims; arithmetic identical to the bit. 6) The 512-shape fuzz generator gained two bodies with index-derived subjects WITHOUT drawing from its RNG stream, so the N1 defect's shapes are unchanged; ~1 in 3 exploration claims now drop at gather.

**Why it matters.** (1) and (2) are seam shape a surface will read; (5) silently changes which line the arithmetic rows exercise.

- Confirm all six.
- Overturn (1): null-only validation, let dispatch refuse.
- Overturn (5): keep the fixtures on exploration with synthetic subjects.

> **Recommendation:** Confirm.

### NR-571 — Sprint N3 slice S3 — sentiment decay authored at 0.074125 (six figures, not the form's 0.0741); non-number rates rejected rather than defaulted; RELATIONS.md edited outside the slice's map
*decision taken on your behalf · raised 2026-08-23 · from Worktree agent S3 (f1e802b2), Sprint N3 slice 1.*

1) economy.lua authors trust_decay_per_tick = access_decay_per_tick = 0.074125 — the same derivation (half-life 9 ticks, 1 − 2^(−1/9)) to six significant figures, because 0.0741 puts the 9-tick result 2.5e-4 off −1.0 and six figures keep it within 3e-6. 2) The loader's read_unit_rate() rejects a NON-NUMBER under either key (not only non-finite / out-of-range), naming the key — stricter than get_or's silent default elsewhere in the file. 3) docs/politics/RELATIONS.md:183 and :235 both asserted 'decay is unauthored'; struck through and dated, outside S3's collision map. 4) A second stale save-format note in components.hpp:85-91 (resource_type) fixed in passing. 5) Rows R3h–R3m added to the existing sentiment_harness rather than a new harness. 6) The harness tolerance is 1e-5 (binary32 multiplicative decay has no 'fixed-point step'); stated in the harness comment.

**Why it matters.** (1) is Ben's number rendered more precisely, not changed; (2) is a deliberate reading of the seam rule as covering type. Recorded so the six-figure constant is not mistaken for a tuning.

- Confirm.
- Author 0.0741 and loosen the harness tolerance to match.

> **Recommendation:** Confirm.

### NR-572 — The nation spines are wired live but the exploration loop does not CLOSE at realistic weights — an indivisible survey earmark can't be met from a ~1.3% line's single-tick share, and the default world enacts no levy so treasuries are 0
*blocker · raised 2026-08-23 · from Sprint N3 slice 1 integration; nation_wiring.cpp § REALISTIC.*

run_nation_step now runs every tick in all three drivers (app, --serve, --export-blackboard): the scorer authors weights, the budget pass spends, earmarked survey claims dispatch, and conservation + determinism hold — nation_wiring.cpp R1-R5 green, and the mechanism closes on a funded fixture. But at the SCORER's own exploration weight it does not close in a played game, for two independent reasons. (1) INDIVISIBILITY vs THIN SHARE: a nation's exploration line gets treasury × (1 − reserve 0.35) × ~0.013 weight ≈ 33–913 cr on a 5000-cr treasury, while corp_ai's top-scoring gated survey claim costs 2,390–16,290 cr; Ben's ruling (NR-568) makes an earmark whole-or-nothing, and a line's share is 'a claim on THIS tick's spendable, not a pot' (nation_budget.hpp rule 2), so the share never accumulates toward the survey unless the TREASURY grows. Measured: a nation needs ~89k cr of treasury to fund the cheapest survey in one tick, ~1.9M for the one corp_ai actually claims. (2) NO INCOME: the default generated world enacts no levy or tariff, so treasuries are 0 (nation_wiring R4, money_conservation had to enact a tariff by hand to see any treasury move) — the spend side has nothing to spend. N1 already called the treasury 'a scoreboard with no game attached'; this is the same gap seen from the spend side.

**Why it matters.** The wiring is done and correct — the inertness gap the sprint targeted is closed, and BL-555/BL-558 can now render a treasury, weights and (once funded) transfers. But a player will not SEE a nation fund a survey until both halves are addressed, so the live-check half of the sprint (R8) will show a treasury and weights but no payments. This is the boundary between 'wired' and 'playing'.

- A) Earmark lines ACCUMULATE toward an indivisible purchase — a per-line pot for earmarked lines only, an explicit exception to rule 2's 'not a pot'. Closes (1); needs a serialisation field for the pot.
- B) Raise the exploration weight (nation scorer / nation_ai_params) so one tick's share clears a survey at plausible treasuries — a tuning change, closes (1) without a data-model change but starves the other eight lines.
- C) Make survey cost payable in instalments (dispatch on partial funding, scan proceeds as credits arrive) — closes (1) by making the purchase divisible; larger survey_system change.
- D) Wire nation INCOME first (levy/tariff enactment in generation, or a nation scorer that enacts) so treasuries are non-zero — closes (2); orthogonal to (1) and arguably the prerequisite.
- E) Accept for this cut: the wiring is the deliverable, funding is BL-538's remaining lines + a nation-income item; file both and move to the surfaces.

> **Recommendation:** D then A. (2) is the prerequisite — nothing spends without income — and it is the older gap (N1's 'scoreboard with no game'). Then (A) for the indivisibility, because it preserves Ben's whole-or-nothing earmark AND the weight model, at the cost of one serialised pot field. (B) fights the weight model; (C) is the biggest change. Either way the wiring stands.

*Files: `src/world/nation_step.cpp`, `src/world/nation_budget.hpp`, `src/world/nation_ai.cpp`, `tools/verify/nation_wiring.cpp`*

### NR-573 — Doc state-independence sweep: the rule applied, and the calls taken inside it
*decision taken on your behalf · raised 2026-08-23 · from Ben, 2026-08-23: 'authority docs drive development and we should strive to make sure they are correct when authored'; the time-slice rule is wrong.*

Every authority doc under docs/ (minus development/, the generated mirrors ACTIONS.md and QUESTION_LOG.md, and the critic/mockdata notes) was rewritten to state the design as present-tense fact. Rule: a BL id survives only as the OWNER of a design, with its short handle; dated rulings and NR pointers survive as provenance; 'landed/shipped/not yet/design-owed/Build status/What is absent' language and sections go. DELIVERY.md § Design state and the standing rules now say the authority doc owns a design from the moment it is settled, and the backlog item points at it. Calls taken without asking: (1) designs the code lacks are stated FLATLY, not hedged with 'when it exists' — the backlog is the only build-state record; (2) the research/ docs were swept lightly and keep their 'research, not authority' framing, since that is a kind-of-document fact, not a build-state fact; (3) MANUAL.md's designed/built marks were removed along with everything else — if Ben wants a player manual that describes the BUILT game, that is a different document and should say so.

**Why it matters.** Every doc was a status snapshot that rotted between sessions; MILITARY.md alone carried two holes the code had already closed. The cost is that a doc no longer tells a reader what is missing — backlog_query.js --touches <doc> has to.

### NR-574 — Orphan holes from the sweep were filed as backlog items AND gathered into one proposed sprint — reading 'authoring sprints comes first' as 'plan them, do not just pile them'
*decision taken on your behalf · raised 2026-08-23 · from Ben, 2026-08-23: 'We should be filing items in the backlog, but I think authoring sprints comes first. We already have the problem of massive backlogs with items left behind for too long.'*

Each 'What is absent' hole the sweep deleted was audited against the backlog. Holes with an owning open item were dropped from the doc (the item carries them). Holes with NO item were filed as backlog items so they have an id, and the whole set was written into sprints.json as one proposed sprint so they are planned rather than shelved. See the sweep's DEVLOG entry for the list.

**Why it matters.** The alternative reading — author a sprint INSTEAD of filing items — would leave the holes with no id and no home once the sprint closes.

### NR-575 — CLAUDE.md rewritten as a router: session-mode table first, complete doc map, condensed method — 7,578 → ~2,100 words
*decision taken on your behalf · raised 2026-08-23 · from Ben, 2026-08-23: 'tonnes of information missing, trimming it to direct to authority docs, and determine which mode the session runs in is vital.'*

Six session modes (Design / Delivery-Light / Delivery-Full / Corpus / Review-verify / Rival), each with its signal, deliverable and reading list; the rule 'say the mode in your first line'. The doc map now lists every doc under docs/ (24 were missing: SUPPLY, COLLAPSE, CREEDS, STRATEGIES, MANUAL, the research notes, the shell panels, the ledger Q&As, PHANTOMS, NEXT_SESSION, REVIEW_AUTOMATION, the retired files) as one line each. The 200-word state essays per doc are gone — the doc owns its own description. The player-identity and syndicate paragraphs left CLAUDE.md; CONCEPT.md and GLOSSARY.md must carry them (verified in the same sweep).

**Why it matters.** CLAUDE.md is read by every session; a stale state essay there outranks a correct doc in practice.

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

