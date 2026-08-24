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

*29 entries — 27 open, 2 resolved.*

---

## Open

### NR-164 — Re-stress generation and time-lapse feel when playable
*question · raised 2026-08-11 · from 2026-08-11 notes session with Ben.*

Ben wants to re-stress-test world generation and the staged-generation time-lapse once the game is playable, to judge whether it feels alive rather than just correct. RULING 2026-08-13 (Ben, elicitation form): keep open until playable - not scheduled, not folded into another item.

*Files: `docs/ui/STARTUP.md`*

### NR-532 — The design register is live — 41 open calls across ten sections, as a form
*observation · raised 2026-08-22 · from Ben, 2026-08-22: "now please revisit each one and open forms for answering the open questions."*

Published at https://claude.ai/code/artifact/debe7b8f-7315-429a-a805-0e295e9405bc. Every open question across the eight new authority docs plus SYSTEMS.md § The progression chain plus four cross-cutting calls, gathered into one form: 41 questions in 10 sections. Each carries its evidence, 3-5 options with one marked as suggested, and a free-text field that overrides the options. Progress is tracked per section; "Copy all answers" puts everything on the clipboard in one block. It is a LIVE DOC - radios and contenteditable fields are captured as edits, so answers reach a watching session directly; deliberately no <textarea> and no <select>, neither of which is captured. Answers also persist to localStorage as a per-viewer draft.

The generator is committed rather than being a one-off (CLAUDE.md § Tool creation is skill creation): tools/session/register/questions.js is the canonical question set and build.js emits the HTML. Verified to regenerate byte-identically. Republish to the same URL to keep answers in place.

**Why it matters.** The open questions were the point of writing the docs as capture rather than design, and they were spread across ten files. A form is the difference between 41 questions that get answered and 41 that get skimmed. It also means the answers arrive in one structured block that can be propagated in a single pass, the way the six nations rulings were.

> **Recommendation:** Answer in any order. The four that change the most downstream: LOGISTICS Q1 (what generates LP), EVENTS Q4 (drive the collapse metagame or express it - it decides the system's size), PEOPLE Q2 (one bias or several - cheapest to overturn now), and NATIONS Q1 (whether the grant reaches a rival, which blocks the player-facing halves of two items).

*Files: `tools/session/register/questions.js`, `tools/session/register/build.js`*

### NR-589 — A ruling was taken on a stale doc paragraph — the four "dominated" recipe pairs are false positives, and the real finding is the opposite
*decision taken on your behalf · raised 2026-08-23 · from Sprint 17 authoring session. Ben ruled "delete the dominated sibling" on evidence this session supplied from docs/economy/PRODUCTION.md § Alternate production methods.*

The doc paragraph names four sibling pairs as dominated on both axes and calls it Ben's open call (NR-243). It is stale. recipe_switch_harness.cpp's header records that its R1 grouping was RETRACTED on 2026-08-16 and moved to chain_depth.cpp's R2 row, which buckets every same-output sibling pair as (a) a supply route with disjoint raws, (b) a named precondition pair, or (c) a genuine interchangeable method — and only compares (c) on price. All four pairs fall in (a) or (b). Acting on the ruling would delete the coal Smelter, the Charcoal Burner, the Potter & Weaver and the airless propellant route, and would strand sand, peat and iron_nickel_ore as orphans, failing chain_depth's own R1 in the same pass. Not acted on. What the replacement guard actually shows is sharper and is now BL-587 (interchangeable methods exist): bucket (c) is EMPTY, so the 2026-08-15 alternate-methods ruling is mechanism with no content behind it.

**Why it matters.** Two things. The ruling stands unfulfilled unless Ben re-rules, and he ruled on a premise this session gave him — so the correction is owed to him directly, not buried in an item. And PRODUCTION.md is stating as an open question something settled seven days earlier, which is exactly the doc-truth failure the 2026-08-23 state-independence sweep exists to catch; the sweep's own holes audit even lists this line as a live hole pointing at an open item.

> **Recommendation:** Take BL-587 (author the missing methods) instead of a deletion, and let it correct PRODUCTION.md § Alternate production methods in the same pass. Re-rule the deletion only if a specific pair is wanted gone for a reason the guard does not model.

*Files: `docs/economy/PRODUCTION.md`, `tools/verify/chain_depth.cpp`, `tools/verify/recipe_switch_harness.cpp`*

### NR-590 — Sprint 17 authored: ten items, six rulings, and four calls taken inside them
*decision taken on your behalf · raised 2026-08-23 · from Ben, 2026-08-23: "Let's get sprint 17 started too ... author items in backlog.js" plus the six-question elicitation form answered the same day.*

BL-585..BL-594 filed for Sprint 17 (v0.1.17), authored against the code rather than against the archived designs. Four calls taken without asking: (1) the sprint keeps the economy-breadth THEME but none of its original items — three landed, three were absorbed by BL-434/BL-460/BL-436 and the recipe-switch work, so re-filing them would have re-litigated finished work; (2) the "new goods" ruling is split into its own first item (BL-585) so the enum append and save bump happen once and every later item stays in Lua; (3) Ben's progression steer is implemented as a THIRD ARM on tech_effect (unlock_recipe) rather than as a new lock kind — the union already has two arms and one authored gate, and a recipe lock is the arm it is missing, not a new system; (4) the guard rows go into chain_depth rather than a new harness, because R1 was moved OUT of recipe_switch_harness in 2026-08-16 precisely to stop two harnesses answering one question with two reference-price tables.

**Why it matters.** The sprint spends a save-format bump and opens the tech-effect union, both of which are cheap now and expensive later. If any of the four calls is wrong, it is cheapest to overturn before BL-585 lands.

> **Recommendation:** No action needed unless one of the four reads wrong. BL-585 is the item to hold if the enum append should wait.

*Files: `docs/development/backlog.json`, `docs/development/sprints.json`*

### NR-591 — The tech tree is ~150 nodes of inert data with exactly one gate that resolves, and Sprint 17 is the first work to lean on it
*observation · raised 2026-08-23 · from Measured while authoring BL-588 (unlock_recipe tech arm).*

scripts/tech_tree.lua opens with "DATA ONLY — the tech system is post-prototype; nothing in the simulation reads this", and prototype_tech_gates() in tech_gate.cpp returns a single gate, E0-ML-01, which unlocks the military base. The tech_effect union has two arms: unlock a building_type, or move a scalar. So the only lock the economy earns today is depth_locked, and it applies only to recipes tagged era = "ancient". NR-490 already observed the same shape from the progression side ("one earnable gate in 150 tech nodes"). RESOLVED IN BL-588 (2026-08-24): the gate table names its own fresh ids — E0-EC-01 (Toolmaker) and E1-EC-01 (Bessemer Converter) — neither transcribed from nor reconciled against tech_tree.lua's existing sketch/derived nodes. The second option this entry named, taken rather than the first.

**Why it matters.** BL-588 authors gates for the first time as content rather than as a proof. That is the moment the tree stops being a picture of a system, and it is worth knowing that the node list in tech_tree.lua is still marked status="sketch" for Era 1 and status="derived" for Era -1 — neither is a ratified content set. Authoring gates against ids in that file binds real behaviour to nodes Ben has explicitly not reviewed.

> **Recommendation:** Either author BL-588's first gates against a small, deliberately-chosen set of Era 0 node ids, or let the gate table name its own ids and reconcile with tech_tree.lua later. State which at promotion; do not let the Lua node list become authoritative by accident.

*Files: `scripts/tech_tree.lua`, `src/world/tech_gate.cpp`, `src/world/tech_gate.hpp`*

### NR-592 — corp_ai.cpp never prices resource_build_cost when scoring a build candidate — pre-existing, not caused by BL-590
*observation · raised 2026-08-24 · from Measured while authoring BL-590 (per-building materials): the build-candidate loops price only building_economics::build_cost (ex.build_cost / pe.build_cost / mex.build_cost), never resource_build_cost.*

BL-590 gave named buildings materially different resource_build_cost baskets (ancient buildings now cost timber/stone, not steel). The AI scorer's capex estimate never priced that array before this item and still does not after it — a candidate scores on cash alone, so a rival can propose a build whose MATERIALS it cannot actually reach even with sufficient cash. construct_building's own affordability gate refuses it cleanly at apply time (no mutation), so this is a missed-opportunity gap for the scorer, not a correctness bug: the seam already enforces the real cost, same shape as the recipe-switch scorer's own documented gap (NR-242, PRODUCTION.md § Alternate production methods).

**Why it matters.** Was already true before BL-590 (every type shared one steel basket, so the gap was invisible — steel is cheap and plentiful for most rivals). BL-590 makes it visible for the first time: an ancient rival with no timber stockpile could now score a Sawmill it cannot actually place. Not urgent (the gate protects correctness), but worth knowing before BL-589's start-gate audit or BL-594's playthrough, in case a rival's build thrash traces back to this rather than to something either of those items would otherwise suspect.

> **Recommendation:** Leave as documented debt unless BL-589/BL-594 measure it costing something real (excess refused-build churn, a rival stalling on a candidate it can never place). If so, pricing resource_build_cost_for into the scorer's capex estimate is a small, contained addition — the same shape corp_ai.cpp already applies to build_cost.

*Files: `src/world/corp_ai.cpp`, `docs/economy/PRODUCTION.md`*

### NR-593 — BL-591's growth-track readout is render-confirmed, not click-confirmed — no computer-use access this session
*decision taken on your behalf · raised 2026-08-24 · from Ben, 2026-08-24, asked directly: how to close BL-591 given no computer-use access this session. Answered: accept the render proof, file this entry.*

The standing rule (io-standing-rules.md, "A UI requirement needs a live check") asks for an actual mouse click before marking a visual requirement complete, not just a scripted capture. This session's computer-use MCP disconnected mid-session (unrelated to this item), so no literal click was possible. What WAS confirmed: scripts/verify/corp_dashboard.lua's corp_rollup_production capture, driven by verify.fold("corp_rollup", 0), renders the three growth-track lines correctly with real content (a fresh corp: "Reached depth 0, via Iron Ore / Next: Bloomery / Needs: Charcoal"). This drives the identical rollup_body() code path a real click reaches -- same function, same data -- and the fold/expand CONTROL itself is not new (BL-248, already live-verified in earlier work); BL-591 only added content inside an already-reachable surface. Ben's call: this is close enough to accept, following BL-575's own precedent (shipped complete with a named residual verification gap, not blocked).

**Why it matters.** If a future session finds the growth-track lines are NOT actually visible when a human opens the Production card (e.g. a layout overflow, a z-order issue, a fold-state bug the script's scripted path does not exercise the same way a click does), this is the record explaining why that was not caught now.

> **Recommendation:** A quick live-click spot check the next time computer-use (or equivalent interactive access) is available in a Project Io session -- open the Corporation dashboard, expand Production, confirm the three lines read as shown in the capture.

*Files: `src/ui/corporation_dashboard.cpp`, `scripts/verify/corp_dashboard.lua`*

### NR-594 — tech_gate_harness only exercises E0-ML-01 — the three economy gates (E0-EC-01/02/03) are proven live but not covered by a dedicated T-row
*observation · raised 2026-08-24 · from Measured while authoring BL-589 (start-gate audit): tech_gate_harness.cpp's full T1-T7 suite only names "E0-ML-01" (the original garrison gate).*

BL-588 authored E0-EC-01 (Toolmaker) and E1-EC-01 (Bessemer Converter); BL-589 added E0-EC-03 (refined_copper). None of the three has its own T1-style existence/predicate-isolation/determinism coverage the way E0-ML-01 does — they are proven correct only indirectly: tech_effect_union_harness exercises the unlock_recipe ARM generically, chain_depth's G5 row proves E0-EC-03 specifically resolves under its own predicate, and the full-suite pass (construction_gate_harness, corp_ai_harness, etc.) never regressed. That is real coverage, but it is scattered across three files rather than living in the one harness whose whole job is this table.

**Why it matters.** If a future edit to any of the three gates' predicates breaks its intended isolation (e.g. E0-EC-01's structure condition accidentally also satisfies E1-EC-01), nothing named after this file catches it directly — same failure class E1-EC-01's own surplus-only first draft was caught by (T3's incidental collision, not a dedicated E1-EC-01 test).

> **Recommendation:** A follow-on pass extending tech_gate_harness.cpp with T-rows per economy gate (T8-ish for each), or folding them into chain_depth's roster-breadth guard (BL-592) if that is a better home. Not urgent — no gap has actually bitten yet — but worth doing before a fourth gate makes the collision surface bigger.

*Files: `tools/verify/tech_gate_harness.cpp`, `src/world/tech_gate.cpp`*

### NR-595 — BL-593's tech-lock filter is render-confirmed only partially — the tile build ledger cannot be scrolled into view via existing verify bindings, and computer-use still cannot reach the custom ProjectIo.exe
*decision taken on your behalf · raised 2026-08-24 · from Continuing the same live-click constraint NR-593 recorded for BL-591, now compounded by a second, distinct gap found while trying to verify BL-593.*

Two separate verification limits, not one: (1) computer-use reconnected mid-session but request_access does not resolve a custom .exe outside the Windows Start-Menu app registry ("ProjectIo" returns notInstalled) -- the same gap NR-593 hit. (2) NEW: scripts/verify/build_door_wide_roster.lua (authored this item) selects a tile and opens the Build door, but the ledger window's title is dynamic ("Construct [x, y]"), which does not match any of scroll_panel's five fixed window-name strings (Tile Ledger/Market Ledger/Economy/Balance Ledger/Corporations/Building) -- so the column cannot be scrolled to bring Metal Foundry (where the fix actually applies) into frame. Both captures (build_door_select, build_door_wide_roster) show only Extraction and Infrastructure, cut off before Processing.

**Why it matters.** The code fix itself is low-risk -- it extends an existing, already-verified removal predicate (era_locked/depth_locked) with one more clause (tech_locked), same shape, same call site -- but it has not been SEEN rendering correctly, only reasoned about from source and confirmed not to crash. If a future change to the ledger's layout or the recipe_unlocked call silently breaks this, no visual check catches it until scroll_panel gains a case for the Build ledger's window (or that window is given a name scroll_panel already knows).

> **Recommendation:** Add a scroll_panel case for the tile build ledger's window (likely needs the dynamic "Construct [x, y]" title matched by prefix, or the window renamed to something fixed scroll_panel can target) the next time this surface needs a full-content capture. Separately, a live-click pass the next time computer-use can reach this project's built exe (a Start-Menu shortcut, or an update to how request_access resolves custom binaries) would close both this and NR-593 at once.

*Files: `scripts/verify/build_door_wide_roster.lua`, `src/core/verify_api.cpp`, `src/ui/selection_panel.cpp`*

### NR-597 — The corp-selection screen’s per-row "Choose" buttons do not respond to clicks
*novel-work · raised 2026-08-24 · from BL-594’s live playthrough (two separate game sessions, two different generated worlds) — every individual "Choose" button on the "Choose your corporation" screen was clicked and did nothing, reproduced 2-for-2; only "Surprise me" (the seed-default pick) responded.*

A real, reproducible UI defect on scripts/verify/corp_choice.lua’s live surface (BL-435). Not investigated further — this item’s scope is the ancient-roster playthrough, not this pre-existing screen. Ben’s ruling: not urgent, keep it out of the backlog for now (KNOWN_BUGS.md is retired per CLAUDE.md, so this is the durable record instead of a filed item).

**Why it matters.** A player who wants a SPECIFIC starting corporation (not the seed default) currently cannot choose one — the screen offers 8 rows and only the escape hatch works.

> **Recommendation:** File a proper BL- item and fix when it becomes a priority — likely a click-handler wired to the row highlight but not the button itself, given corp_choice.lua’s own note that the stage is reached via a re-entry path (verify.show_corp_choice) rather than the normal flow.

*Files: `src/ui/*.cpp (corp choice screen, not yet located)`, `scripts/verify/corp_choice.lua`*

### NR-598 — Two surfaces report different grid dimensions for the same body
*question · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

On one frame, for Huhaidar: the Generation Ledger's PROFILE reads 'Grid 180x84 (15120 tiles)' -- and 180*84 does equal 15120 -- while the canvas's own caption reads 'Huhaidar Planet (261x121)'. The archived roadmap's ancient-refocus note describes the map as 3x at 312x145. Three numbers, one body; at most one of them is the grid. Worth settling before either surface is written into an authority.

**Why it matters.** The tile census, the placement rules and every per-body area calculation are read off whichever of these is real.

*Files: `src/ui/generation_ledger.cpp`, `src/ui/body_surface_canvas.cpp`, `docs/economy/TILES.md`, `docs/generation/TILE_GENERATION.md`*

### NR-599 — Every decision in the AI feed reads 'overridden' with a chosen score of 0.00
*question · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

The decision feed's visible entries are all one shape: a build candidate, the reason 'best available build site', a score line reading '0.00 v 2.08' or '0.00 v 0.08', and the word 'overridden' in red. If the first number is the chosen candidate's score, the scorer is taking a zero-scored option over a positive one in every logged case; if it is not, the readout does not say what it is. Either the feed is mislabelled or the scorer is doing something worth knowing about.

**Why it matters.** The feed is the only window onto rival reasoning, and 'the credible rival' is a named theme. A readout that always says the same thing cannot distinguish a working scorer from a broken one.

*Files: `src/ui/decision_feed.cpp`, `src/world/corp_ai.cpp`, `docs/ai/AI_OPPONENT.md`*

### NR-600 — Goldens deliberately NOT blessed for the shell pass, despite the exception granted
*decision taken on your behalf · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

Ben granted an exception to the curated-world-independent golden policy for this pass, on the grounds that ubiquitous UI items are essentially world-independent. Held anyway, for two reasons. (1) The captures are FULL FRAMES and the canvas fills most of every one, so a golden over them is world-dependent whatever the chrome does -- a generation change would fail all 31 for reasons with nothing to do with the UI. (2) The pass exists to decide what several of these surfaces should become; blessing now pins the look we are about to change, and the first real alteration then re-blesses everything, which is the bulk re-bless the policy exists to prevent. Recommendation: bless after the alterations, and only over frames where chrome dominates -- or give the golden harness a capture REGION so chrome can be diffed without the canvas behind it.

**Why it matters.** The grant is cheap to spend once. Spending it on a state we are about to overwrite wastes it and leaves 31 stale goldens behind.

*Files: `.claude/skills/verifier-visual/SKILL.md`, `docs/development/DEVELOPMENT_PRACTICES.md`*

### NR-601 — Six of the seven gradient-bar lens keys are drawn underneath the Selection band and cannot be read
*question · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

Corrected twice, and the second correction is the finding. First pass: I cropped the lens captures to the minimap, saw no legend, and reported it missing. Second: the full-column crop showed the Market lens keying itself ABOVE the minimap and the Population lens keying nothing, so I reported that. Both wrong. LENSES.md documents TWO legend chromes -- count-driven keys (Country, Market, Reach, Supply-routes) in the right chrome column above the minimap, and fixed-height GRADIENT-BAR keys (Resource, Production, Scarcity, Population, Industry, Opportunity, Continent) anchored flush-LEFT of the minimap and vertically centred on it. That anchor puts them inside the rect the always-open Selection band occupies. Measured: under the Population lens the key IS drawn -- 'Workforce efficiency / low ... high' -- and it shows through the band as a ghost at roughly a tenth of its intended contrast, unreadable. The doc already half-knows this: the Continent key alone is drawn on ImGui's FOREGROUND list with an opaque fill 'so it floats over the always-open Selection band rather than being buried by it' (BL-376). One of the seven was fixed; the other six were not, and nothing had ever captured them to notice.

**Why it matters.** A colour code with no readable key at the moment of interest is the exact failure the v0.1.9 done-definition named and claimed to have closed. It also means six lenses have been shipping with an invisible legend since the Selection band became always-open (BL-266), which is a collision between two landed items rather than a defect in either. Ben's 2026-08-24 UI list independently proposes the fix from the other end -- every lens selector and key lives in the minimap header -- which would retire the flush-left anchor entirely.

*Files: `docs/ui/LENSES.md`, `docs/ui/SELECTION.md`, `src/ui/body_surface_canvas.cpp`, `src/ui/overlay.cpp`*

### NR-602 — Five always-on surfaces have no entry in the UI justification store
*observation · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

question_log.json holds 39 surfaces and the rule is that every information surface declares the question it answers and why it earns its space. Five of the surfaces this pass walked have no entry at all: the TIME PANEL, the MINIMAP, the LENS LEGEND region, the in-session SYSTEM MENU, and the LAUNCH SCREEN. Four of the five are on screen in every session. One near-miss worth naming while here: the Header entry's stated question is 'Am I solvent, and what is the date?' -- and the header does not show the date, the time panel does. So the store already attributes one surface's job to another. NOT drafted here on purpose: writing the pair IS the design check, and four of these five are surfaces the pass has just proposed changing (NR-610, NR-615, NR-598, NR-601), so a question authored now would be answering for a surface whose shape is not settled. Draft them as each ruling lands.

**Why it matters.** Enforcement of this store is authorship, not machinery (Ben, 2026-08-01: no check may be built against it), which means a missing entry is only ever found by someone reading it against the screen. This pass is the first time anyone has read all of it against all of the shell.

*Files: `docs/ui/question_log.json`, `docs/ui/TIME_CONTROLS.md`, `docs/ui/MINIMAP.md`, `docs/ui/LENSES.md`, `docs/ui/MENU.md`, `docs/ui/STARTUP.md`*

### NR-603 — Retiring the Opportunity lens also retires BL-086's ambient growth read - there was never a second surface
*decision taken on your behalf · raised 2026-08-24 · from Sprint 17b, Slice B (BL-604 / BL-602), reported 2026-08-24.*

Ben's instruction was 'retire the opportunity lens and the production intensity lens', and the item was filed with a rider saying BL-086's ambient opportunity read is a SEPARATE always-on cue and should be confirmed untouched. That premise is wrong, and the slice checked rather than assumed: archive/DEVLOG-2026.md records BL-086 closed with NO NEW CODE - 'the shipped Opportunity lens already reads at rest with its key and isn't auto-activated; pinned with a golden'. scripts/verify/opportunity_ambient.lua pinned exactly that, and was deleted with the lens. So there is no second surface: the lens WAS BL-086's outcome, and retiring it retires the ambient read too.

**Why it matters.** Ben asked for the lens to go, not for the glanceable growth read to go, and until now the two looked separable. They are not. If he still wants an at-rest read of where demand is unmet, it is NEW work with its own question to answer - not a surface that can be rediscovered. Raised so the loss is a decision rather than an accident found later.

*Files: `docs/ui/LENSES.md`, `docs/development/archive/DEVLOG-2026.md`*

### NR-604 — LENSES.md's rung table describes three surfaces that do not exist, and its routing table described a fourth
*observation · raised 2026-08-24 · from Sprint 17b, Slice B (BL-604 / BL-602), reported 2026-08-24.*

Retiring the Production lens raised the question of whether its Circumplanetary rung - a per-body output-throughput badge - should survive as body-level chrome. It was retired, and the decisive reason was that it WAS NEVER BUILT: circumplanetary_canvas.cpp carries no overlay_mode::production pass at all, only the Supply convoy badge. LENSES.md's rung table described an intent as a surface. The slice then noticed the same hole one row down: SCARCITY's documented Circumplanetary per-body shortfall badge is equally unbuilt. It was outside the item and left alone. CENSUS COMPLETED 2026-08-24 during Wave 2, mechanically rather than by reading: every overlay_mode grepped against all three canvases. Result — EVERY lens is Planetary-only except Market (which adds Circumplanetary) and Supply (Circumplanetary and Solar). So the rung table's claims for SCARCITY's Circumplanetary per-body badge, REACH's Solar connected-body glow, and SUPPLY-ROUTES' Solar aggregated graph edges are all unbuilt, alongside Production's, which retired with its lens. Three live fictions, not one.
A FOURTH was found in the ROUTING table and has now been fixed rather than filed: 'beneath the Corporation lens a hovered building resolves through to its owning corporation' had been stated since 2026-06-15 and never implemented — a click gave the building. BL-603 implements it, which is how it was discovered: the check asserted the documented behaviour and it failed.
NOT REWRITTEN TO MATCH THE CODE, deliberately. The standing rule is that authority docs are state-independent: they say what is TRUE OF THE DESIGN, not what is built, and 'if a doc and the code disagree, one of them is wrong and the fix is work, not a footnote'. So the three remaining rungs are WORK, not doc errors — either they get built or Ben rules them out of the design. What they are not is a table anyone can read as an inventory of what exists.

**Why it matters.** BL-603's per-lens structure walk had to be written against what the code does rather than what the table says, and the fourth fiction was caught only because a check asserted the documented behaviour instead of the observed one. The general lesson is the sharper half: a table of per-surface claims that nobody can query is a table that drifts silently. Every other store in the project has a tool (backlog_query, actions_query, story_check); this one has prose.

*Files: `docs/ui/LENSES.md`, `src/ui/circumplanetary_canvas.cpp`, `src/ui/solar_system_canvas.cpp`, `src/ui/body_surface_canvas.cpp`*

### NR-606 — Four verify scripts fail for reasons that predate this sprint, and one of them has never run at all
*observation · raised 2026-08-24 · from Sprint 17b integration sweep, 2026-08-24 - all 92 verify scripts run against the merged tree.*

The integration sweep ran every scripts/verify/*.lua against the merged tree: 92 scripts, 6 failures. Two are ours and accounted for (icon_silhouettes' two goldens moved because the chrome moved - see below). The other four fail against a PRE-SPRINT build too, checked directly rather than assumed:
  * sticky_card.lua and building_element.lua - both die on 'no valid iron tile near the rival cluster'. A fixture assumption that generation drift has invalidated; the same class of rot as the hard-coded tile that had built_tile_render.lua framing open ocean and pop_markers.lua framing empty terrain.
  * recipe_workforce.lua - asserts 'at 100% workforce the building produces output' and measures out=0.0. That is an ECONOMY finding wearing a UI check's clothes, and it is the more interesting of the four.
  * battle_card.lua - a Lua error on line 17, `verify.goto_surface()` called with no argument. It has never run: verifier-visual's own SKILL.md says it was authored 2026-08-21 in a container with no SDL3 and awaits a run. This is that run, and it fails on its first line of work.
GOLDENS LEFT RED AND ATTRIBUTED rather than blessed: icon_silhouettes' pair diff at 10.39%, and the diff image shows exactly this sprint's chrome - the time panel flush to the top-right, the taller minimap, the accordion in the Selection band. Blessing now then re-blessing after Wave 2 (BL-603) is the bulk re-bless the golden policy exists to prevent, so it holds until the sprint closes. Precedent: Sprints 19 and 25a closed the same way.
Worth noting about that pair: icon_silhouettes is the whole curated golden set, and it is not an icon catalogue - it is two FULL-FRAME captures of the shell. They are world-independent only because the canvas happens to be dark in them.

**Why it matters.** Four rotten checks is four surfaces with no working guard, and nobody knew, because nothing had run the whole set in one pass. recipe_workforce is the one to look at first - a building at full workforce producing nothing is either a real economy defect or a stale fixture, and both are worth knowing before Sprint 17's economy work leans on it.

*Files: `scripts/verify/sticky_card.lua`, `scripts/verify/building_element.lua`, `scripts/verify/recipe_workforce.lua`, `scripts/verify/battle_card.lua`, `.claude/skills/verifier-visual/SKILL.md`*

### NR-607 — Do we still want versioned releases? Item-level versioning is already gone
*question · raised 2026-08-24 · from Ben, 2026-08-24, opening the ROADMAP trim: 'I don't think we need versioned releases right now.'*

Raised rather than acted on, because dropping the version ladder is a bigger change than the trim he asked for. The observation stands on its own: item-level versioning ALREADY ended on 2026-08-23, when the backlog purge left the hot file holding one sprint of items with no version field at all - so nothing queryable maps a minor to its work any more, and the roadmap was the last place the mapping lived (in prose). Tags v0.0.4..v0.1.15 are real and stay; the themes are live and stay. What is in doubt is the NUMBERING and the per-minor cut ceremony around it. Three readings, all defensible: (a) keep the ladder, cut tags as before; (b) keep themes, drop numbers, and let a sprint close be the only unit of completion - which is what practice already does, since the nation/province/watch lanes landed real work under no tag; (c) keep numbering only for the eventual commercial cut of the ancient product. The trimmed ROADMAP.md holds the question in a banner and behaves as (b) in the meantime - themes named, sequencing delegated to SPRINTS.md.

**Why it matters.** The done-definition-at-the-cut rule (NR-103) is attached to the version ladder. If minors stop being cut, that rule needs a new anchor or the failure it prevents - a theme with no test for finished absorbing items indefinitely - comes back. NR-102 (sequencing decoupling) is the same hazard from the other side.

*Files: `docs/development/ROADMAP.md`, `docs/development/SPRINTS.md`, `docs/development/DEVELOPMENT_PRACTICES.md`*

### NR-608 — A loaded save does not RENDER as the world it was saved from -- the day-tick mirror is rewound and the canvas comes back dimmed
*decision taken on your behalf · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

Measured in both directions. The WORLD half of the save is whole: player_balance comes back bit-identical (1182.7651367188 -> 1182.7651367188). But world::state_hash does NOT round-trip once any econ tick has run (7161C70488A767CA -> 62CAB4412DEDC7E0), while a fresh world with zero ticks DOES round-trip exactly -- which localises it. Cause: verify.econ_step increments m_world.current_day_tick directly and never advances m_sim_loop, so app::save_game_to writes env.day_tick = m_sim_loop.day_tick() (still 0 under --verify) and app::load_game_from then sets m_world.current_day_tick = env.day_tick. The hash is keyed on that tick, hence the divergence. The VISIBLE consequence is the one that matters: the activity fog reads every glimpse as stale, so the loaded canvas is dimmed. ui_fixture_live.png beside ui_fixture_loaded.png is the pair. DECISION TAKEN: shell_pass.lua stages its own world (stage_ui_fixture in lib.lua) rather than loading the snapshot, because reviewing a layout against a degraded picture of it is worse than paying the generate cost. The fix is Ben's call because it is not local: making econ_step drive sim_loop::advance_days would start advancing dates, orbits and surveys in EVERY existing verify capture.

**Why it matters.** Ben's stated sequence for the UI minor was save -> load -> bless goldens. Goldens blessed from a load would pin a fog state the live campaign never shows. It also means save_load.lua's 'state_hash unchanged across the round trip' assertion passes only because that script never ticks the economy.

*Files: `src/core/verify_api.cpp`, `src/core/app.cpp`, `scripts/verify/ui_shell_fixture.lua`, `scripts/verify/save_load.lua`*

### NR-609 — The comms log is not in the save envelope, so the dock comes back empty after any load
*question · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

save_game.hpp's exclusion list justifies dropping 'hover and chat state' as transient view state. That is true of hover and is not true of the LOG: the comms dock is one of the always-on surfaces, and its content is the record of what happened in the campaign, not a view preference. After a load the dock holds only the lines the load itself posts. Two options: put the message vector in the envelope, or derive the dock from history_log on load -- the cheaper answer if the log already carries the same events.

**Why it matters.** A player who saves and reloads loses the campaign's whole narrative surface. It is also the second reason a loaded world does not render as the saved one (NR-608).

*Files: `src/core/save_game.hpp`, `src/core/save_game.cpp`, `docs/ui/CHAT.md`*

### NR-610 — Every date in the UI reads 1960, and three readouts of the same clock disagree
*observation · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

Three things, one subject. (1) campaign_epoch_year is a hard-coded 1960 in BOTH src/ui/format.hpp:82 and src/world/planetology.hpp:275, tied together by a static_assert, while world_params::epoch_year defaults to 0 -- and no file under src/ui reads epoch_year at all. So a 0 CE campaign renders '1960 Jan 1st [Q1]' in the time panel and '1960 Jan 13th' in the AI decision feed. (2) On one frame the header reads '19q' elapsed, the time panel reads Jan 1st Q1, and the decision feed reads Jan 13th. Part of that spread is the --verify artifact in NR-608, but the epoch itself is live-app behaviour. (3) The launch screen's tagline still reads 'Near-future corporate...', which is the space arc's framing rather than the 0 CE product's. OVERLAP, checked in the archived backlog: BL-552 (two clocks, one word) already owns the tick-vocabulary half of this -- a day tick and an economy tick both called 'tick' -- and is designed, not built. This entry is the EPOCH half, which BL-552 does not touch: the year the calendar counts from is a hard-coded constant that no UI site reads from world_params. Keep them separate; fixing the vocabulary would not move a single date. OVERLAP, checked in the archived backlog: BL-552 (two clocks, one word) already owns the tick-vocabulary half of this -- a day tick and an economy tick both called 'tick' -- and is designed, not built. This entry is the EPOCH half, which BL-552 does not touch: the year the calendar counts from is a hard-coded constant that no UI site reads from world_params. Keep them separate; fixing the vocabulary would not move a single date.

**Why it matters.** The date strip is the most-read piece of chrome in the game and it currently contradicts the product the roadmap says we are building. It is also the cheapest tell that the 0 CE refocus is unfinished.

*Files: `src/ui/format.hpp`, `src/world/planetology.hpp`, `src/ui/time_panel.cpp`, `src/ui/startup_screens.cpp`, `docs/ui/HEADER.md`, `docs/ui/TIME_CONTROLS.md`*

### NR-611 — The Selection band's resting state is ~90% empty, and its action grid is six near-identical circles in every kind
*question · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

Four observations on one surface, all visible in _band_strip.png. (a) The RESTING state -- no selection, so the band rests on the player's own corporation (BL-266) -- is a title row plus 'Open its ledger via [>].' over ~200px of black, and that is the state the band is in most of the time. (b) The right-hand action grid is a 2x3 of six slots, and across tile / province / own building / rival building most slots draw as an empty circle; an empty circle is indistinguishable from a disabled action and from an unimplemented one. (c) The left cell has no consistent subject: a hex neighbourhood for tile and province, a large flat grey silhouette for the Military Base, a bare X for the Extraction Site -- the last two read as missing artwork rather than as designed glyphs. (d) A click on a tile drives the BAND; no click-opened sticky card appeared, so whether BL-194/BL-195's card still exists after BL-266 made Selection always-open is a question the authority does not answer. UPDATE 2026-08-24, from Slice C: the dead-space half of this is now CHEAP to fix and is waiting on Ben rather than on work. BL-598 put a reusable section-toggle shape in selection_panel.cpp (about ten lines), so the resting band could take the same accordion the tile now takes. What it needs is not code - it is Ben NAMING THE SECTIONS, because naming them is the design. The slice was instructed not to take the free improvement and did not.

**Why it matters.** The band is permanent chrome and the second-largest region on screen. Its resting state is the shell's biggest single piece of dead space.

*Files: `docs/ui/SELECTION.md`, `docs/ui/ledgers/selection.md`, `src/ui/selection_panel.cpp`, `src/ui/selection_card.cpp`*

### NR-612 — Internal identifiers are leaking into player-facing UI
*observation · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

Four sites found in one pass. The Budget ledger prints 'Policy levers - not yet wired (BL-155)'. The Research panel opens with 'BL-087 design mock - read-only.' The tech ladder offers 'Era 2 (unauthored)' -- an authoring state, offered as a choice. The Contracts ledger names its subject 'Province #20886' and repeats it as 'holds province #20886'; provinces are the grain the player clicks and they carry no player-facing name here at all. A backlog id in a shipped surface is a different category from a placeholder: it is the development process addressing the player.

**Why it matters.** These are invisible to us and the first thing an outside player sees. The province one is more than cosmetic -- it means the contract surface cannot say WHERE the job is.

*Files: `src/ui/balance_ledger.cpp`, `src/ui/tech_tree_panel.cpp`, `src/ui/contracts_ledger.cpp`, `docs/ui/ledgers/balance.md`, `docs/economy/CONTRACTS.md`*

### NR-613 — Nav slot 8's corporations table draws its name column one character wide
*observation · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

The table's columns are 'C | Reach | Capital | Share | Stance'. The first is the corporation NAME, allotted roughly ten pixels, so every row reads as a single letter (F, E, C, G, T, N, Y, Q, H, Z, B, S) and the header 'C' is itself the truncated word. Beside that, sixteen of seventeen rows read 'Minor | Minor | Negligible | Negligible | Neutral', so the table's information content is close to zero, while one row has a different shape entirely ('G | 1 bodies | 957.8 | 2% | -'). This is the slot that had NO verify hook until this pass, which is why nothing had ever captured it.

**Why it matters.** It is the provisional host for Diplomacy and the only side-by-side rival comparison in the game. It is also direct evidence for the rule that a surface with no scripted path is a surface nobody has looked at.

*Files: `src/ui/corporation_panel.cpp`, `docs/ui/MENU.md`*

### NR-614 — expect_no_clipping's scope is draw-list text only, so '0 records' over the whole shell is not evidence of no clipping
*observation · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

The 31-capture pass ends with 'expect_no_clipping PASS: 0 failure(s), 0 record(s) total' -- zero RECORDS, not zero failures -- while the Corporation dashboard visibly truncates 'Workforce 76% of labour demand n' mid-word, the Economy panel clips 'CalonIntus-PaxisAthen Venture:', and a table row is cut in half at the fold. The reason: ui::text_fit records only what routes through it, which is draw-list text (the skill's own coverage grep finds just 2 unexempted AddText sites), whereas ~524 ImGui::Text* calls in src/ui draw inside ImGui windows and are clipped by ImGui itself, invisible to the recorder. The check is not broken; its scope is far narrower than 'the shell does not clip', and the verifier-visual skill reads as the latter.

**Why it matters.** A green clipping verdict over a frame that visibly clips is worse than no verdict. Either the recorder grows to cover widget text, or the skill's wording narrows to what it actually proves.

*Files: `src/ui/text_fit.cpp`, `.claude/skills/verifier-visual/SKILL.md`, `docs/development/DEVELOPMENT_PRACTICES.md`*

### NR-615 — The system menu offers only Resume and Exit Game -- there is no way to save from the UI
*observation · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

The header-corner popup holds exactly two buttons. Quick save and quick load shipped with BL-536 bound to F5/F6, and display options exist (BL-076), but none of the three is reachable from the menu -- so a player who does not know the function keys cannot save, and cannot discover that saving exists. The popup also draws over the header's NET and elapsed-quarter readouts. NOT a regression, checked in the archived backlog: BL-070 (in-app system menu) is complete and its stated scope was 'Exit Game / pause without keyboard' -- so the two buttons ARE the item. Save and load arrived later with BL-536 and nothing went back to the menu, and display options (BL-076) were never added either. The gap is an unclaimed seam between two landed items, not a defect in either. NOT a regression, checked in the archived backlog: BL-070 (in-app system menu) is complete and its stated scope was 'Exit Game / pause without keyboard' -- so the two buttons ARE the item. Save and load arrived later with BL-536 and nothing went back to the menu, and display options (BL-076) were never added either. The gap is an unclaimed seam between two landed items, not a defect in either.

**Why it matters.** Ben opened this session with 'first we should make a save game'. From inside the game, that is a keyboard secret.

*Files: `src/ui/time_panel.cpp`, `docs/ui/MENU.md`, `docs/ui/HEADER.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-596 — spectator_determinism’s R1 A/B family-coverage check retired — bit-identical RNG-stream determinism is not a property this harness needs to hold
*decision taken on your behalf · raised 2026-08-24 · from BL-586 slice 2 (Tannery/Weaver/Shipwright): widening resource_count 42->47 shifted an RNG stream and broke the "seated+spectated reaches every family it reached as a rival" assertion, unrelated to the new content itself (differentially confirmed: identical failure with recipes.lua/economy.lua reverted to pre-slice-2, enum width unchanged).*

The check compared as_seated (family coverage in a seated+spectated 300-tick rollout) against as_rival (the same corp’s coverage as an ordinary rival) and required as_seated >= as_rival. Put to Ben rather than silently patched (per the standing rule against weakening a failing test) or silently left failing. Ruled: bit-identical RNG-stream determinism is not the property this harness needs to prove beyond what R2/R3 already assert — saves carry the actual state, not a replay-from-seed, and occasional randomness is a deliberate strategy lever, not a defect. The check is retired (commented out with the ruling and full provenance trail, not deleted) rather than weakened to pass; the two properties io-standing-rules.md’s BL-409 section actually cites by name (defaults false, no rival’s cadence slot shifts) are untouched and still pass.

**Why it matters.** io-standing-rules.md’s Determinism & data model section cites spectator_determinism.cpp by name as the harness proving the BL-409 spectator grant’s two load-bearing properties. This ruling narrows what that citation is understood to guarantee — worth a standing-rules note so a future reader does not assume EVERY assertion in that file is load-bearing.

> **Recommendation:** Add a one-line clarifying note to io-standing-rules.md’s BL-409 paragraph: the harness proves the two named properties (default-false, no cadence-slot shift), not RNG-stream-identical behavioural outcomes across a content change — that expectation was explicitly ruled out 2026-08-24.

> **RESOLVED.** Note added to io-standing-rules.md’s BL-409 paragraph (2026-08-24): the harness guarantees the two named properties (defaults false, no cadence-slot shift), not RNG-stream-identical behaviour across a content change. The retired check itself stays retired in spectator_determinism.cpp with its own provenance comment.

*Files: `tools/verify/spectator_determinism.cpp`, `.claude/rules/io-standing-rules.md`*

### NR-605 — The Selection band now carries two centre-column idioms - an accordion for the tile, pagers for everything else
*question · raised 2026-08-24 · from Sprint 17b, Slice C (BL-598), reported 2026-08-24.*

BL-598 replaced the tile element's three-view PAGER with a five-section ACCORDION, on Ben's ruling. The building card and the unit card still use pagers. That is defensible on its face - the tile has five sections and they have two or three, and an accordion over two sections is ceremony - but it is a fork in the shell's vocabulary, and the slice filed it as an open question in ledgers/selection.md rather than unifying on its own judgement. Which is right: a reader now learns two disclosure idioms for the same region, and 'how many sections' is not a rule a player can see.

**Why it matters.** The band is permanent chrome and the second-largest region on screen. Two idioms in one rect is the kind of thing that reads as an accident rather than a decision, and it gets more expensive to unify with every card added. Three answers: unify on the accordion, keep the split and write the rule down (pager under N sections), or let each card choose and stop treating it as one region.

> **RESOLVED.** RESOLVED 2026-08-24, by the fork closing rather than by a ruling on it. Ben replaced the tile card's accordion with a section TOP NAV the same day he ruled the accordion in - so the tile card uses a pager-shaped control again, as the building and unit cards always did, and the shell carries one disclosure idiom in that region rather than two. The question this entry asked - unify, or write down the rule for the split - is answered by there being no split.

*Files: `docs/ui/SELECTION.md`, `docs/ui/ledgers/selection.md`, `src/ui/selection_panel.cpp`*

