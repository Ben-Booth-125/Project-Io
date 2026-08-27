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

*96 entries — 69 open, 27 resolved.*

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

### NR-599 — Interception narration is unowned after Sprint 25b's deletion
*question · raised 2026-08-24 · from The Sprint 18 design form: Ben ruled 25b deleted under the purge policy; its interception-surfacing half died with the shell.*

Interdiction's core landed 2026-08-21, but every interception_record is computed and discarded — the out_cuts sink (supply_system.hpp) has no production caller, so LOGISTICS.md §7's own clause ('announced in the same change that resolves it… silence is the wrong default', NR-407) is design without an owner. The work is one wiring pass: comms message naming lane and interceptor, Convoys-ledger exit with stated cause, tile marked a few ticks — the battle_dispatches precedent applies.

**Why it matters.** An interception is the most consequential thing that can happen to a player's economy without them pressing anything; today it is silent — the exact defect the deleted sprint existed to fix.

- Add it to Sprint 18 as a ninth item — it is small, and BL-597 (LP passive convoys) touches the same dispatch surfaces
- File it fresh for the next conflict-facing sprint
- Accept silent interception until the military surface returns

> **Recommendation:** File it fresh now and let it ride whichever sprint next touches the convoy surfaces; it is an afternoon of wiring, and leaving it unfiled is how designed-but-silent work went missing the last time.

*Files: `src/world/supply_system.hpp`, `src/app/app.cpp`, `docs/economy/LOGISTICS.md`*

### NR-600 — LP rates and upkeep constants: first-cut-then-tune, taken on your behalf
*decision taken on your behalf · raised 2026-08-24 · from The Sprint 18 design form left the rates line blank; the LP consumer order was answered (active first) but not who sets the constants.*

Decision taken so the sprint can build: the first-cut constants — active-LP credit price per unit-distance, passive cap magnitudes per city scale, and BL-603's (upkeep zeros) decay/recovery/out_of_supply_reach rates — are authored in the landing items, each argued against BL-543's (value anchor) unit-cost anchor with a dated comment in scripts/economy.lua, and flagged here for your tuning pass rather than ruled ahead of code.

**Why it matters.** LOGISTICS.md's own constraint 3 makes the LP cost formula load-bearing (the allocation sort key is a function of it); a wrong first cut is cheap to retune but expensive to discover late.

> **Recommendation:** Review the constants at the sprint's first calibration pause — overturn any of them freely; the items carry the argument for each number.

*Files: `scripts/economy.lua`, `docs/economy/LOGISTICS.md`*

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

### NR-616 — ai_skill_harness's five golden bands are already red on main -- unrelated to BL-608 (Port gates the sea leg)
*observation · raised 2026-08-24 · from BL-608 (SEA_PORT_GATE) golden-churn verification pass.*

Building BL-608 (the Port-building sea-mode gate in price_convoy_leg, src/world/supply_system.cpp), I ran ai_skill_harness as a golden-churn check. All five seeds fail net-worth-final, net-worth-min and solvency (net worth reads roughly -3.1M to -4.9M against golden bands blessed 2026-08-16), plus several dial/build thrash-ceiling rows. I re-ran the identical harness against the UNMODIFIED main tree (git stash of my one changed file) and got byte-for-byte the SAME 20 failures -- so this is pre-existing golden drift, not something BL-608 caused. Flagging rather than silently re-blessing, per the golden-churn discipline; I made no attempt to fix or re-bless it, since it is out of BL-608's scope.

**Why it matters.** The 2026-08-16 bands (BL-436) are stale against whatever landed since -- possibly BL-573/BL-574 (mercenary contracts), the N-series nation grants, or BL-586's roster widening, any of which could move rival net worth. Whoever owns ai_skill_harness next should re-derive the bands from a fresh bless rather than assume BL-608 (or any other recent item) broke them.

*Files: `tools/verify/ai_skill_harness.cpp`*

### NR-617 — build_harness.js's LUA_TUS exclusion list is stale -- contract_template.cpp needs sol2/lua54 but is not excluded
*observation · raised 2026-08-24 · from BL-608 (SEA_PORT_GATE) build pass -- the headless one-line builder failed on the world-superset TU set.*

tools/verify/build_harness.js globs every src/world/*.cpp minus LUA_TUS = {recipe_registry, works_registry, tech_tree, world_gen_config} and documents that set as 'the four sol2/Lua TUs io_world_obj excludes'. contract_template.cpp #includes lua_state.hpp (which pulls sol/sol.hpp) and is NOT in that set, so the one-line builder fails to compile (no sol2 on INCLUDE) and, once sol2's include dir is added by hand, fails to LINK (no lua54.lib named at all -- the script's cl invocation carries no /link section). I routed around it for BL-608's sea_port_gate.cpp by hand-adding sol2_src/include + lua_src to INCLUDE and appending build\lua54.lib on the link line outside the tool. Did not touch build_harness.js itself -- out of BL-608's scope and I did not want to guess at the intended fix (exclude contract_template.cpp like the others, since nothing here needs to actually LOAD a script; or thread a real lua54.lib link path through for every TU that only compiles against the headers).

**Why it matters.** Every session that reaches for this builder against a harness pulling in contract_template.cpp (any world-superset harness, which is most of them) hits the same wall and has to rediscover the same workaround. It is the documented 'one-line builder' (NR-392) silently failing on a large slice of its own advertised surface.

*Files: `tools/verify/build_harness.js`*

### NR-619 — BL-596 active LP: nearest-anchor rule and refusal surfacing are reasoned interpretations, not lookups
*decision taken on your behalf · raised 2026-08-25 · from BL-596 (LP_ACTIVE_MARCH) landing — LOGISTICS.md does not name the mid-route anchor-attachment rule or a narration pathway for a refused march.*

Two calls made so the item could land: (1) a marching unit draws from the anchor with the LOWEST intra_body_path cost from its CURRENT position, recomputed fresh each tick (never cached per-unit) — reusing the exact anchor enumeration body_reach_field seeds its Dijkstra from. (2) a refusal is surfaced only via unit_march_tick::refused_no_lp, a counter, matching the marching/arrived/recomputed style — this codebase has no existing per-unit-movement narration pathway (no agency_event/world_history_entry equivalent fires from run_unit_march today), so no comms/UI surface was invented for this first cut.

**Why it matters.** LOGISTICS.md (Refusal, surfacing and determinism) requires refusal to be non-optional and legible to the player — a counter on a return struct nobody currently reads UI-side is real but not yet PLAYER-visible. BL-606 (the Throughput lens) is the named surface; until it lands, a refused march is legible only to a harness, not to Ben in play.

- Leave refused_no_lp as the sole signal until BL-606 lands the lens
- Add a lightweight comms-log line now (piggybacking the pattern BL-458 interdiction uses) ahead of BL-606

> **Recommendation:** Leave it — BL-606 is already scoped to be the throughput surface, and a second ad-hoc narration path now would likely be thrown away when it lands.

*Files: `src/world/economy_system.cpp`, `src/world/economy_system.hpp`, `src/world/logistics.hpp`, `src/world/logistics.cpp`, `docs/economy/LOGISTICS.md`*

### NR-621 — BL-606's Throughput lens shades by REACH COST, not by 'the LP serving this tile' - because the second is measurably a constant
*decision taken on your behalf · raised 2026-08-25 · from BL-606 (throughput lens), landing the surface half of LOGISTICS.md section Logistic Points.*

LOGISTICS.md says 'throughput is that field with a magnitude'. Built literally - per tile, the active LP of the anchor serving it - that field is a CONSTANT, and this was measured rather than assumed. On the home body: 57 anchors, every one generating the same authored active_lp_per_anchor_tick (20.0), and EVERY tile carrying a finite reach cost (ocean is crossable at a sea-leg cost, so nothing is unreachable). So both 'is it reached' (the Reach binary) and 'how much LP reaches it' are flat over all 31,581 tiles - a wash carrying no information. Making the second vary would need a per-tile NEAREST-ANCHOR attribution the engine does not have, and deriving one is the second distance/anchor model BL-325 ruling 3 forbids. So the lens draws the quantity that does vary: the reach FIELD's own magnitude - how far this ground is from a generator - with the LP quantity drawn as a ring on each anchor, where LOGISTICS.md constraint 2 insists its spatial locus stays. Field ramp measured and square-root compressed: cost median 20.8 against max 101.8, so a linear ramp put four fifths of the grid in its top fifth and the map read as one flat wash.

- Accept as built - reach-cost field plus a per-anchor LP ring
- Per-tile nearest-anchor attribution instead, accepting a new multi-source Dijkstra that returns the SOURCE as well as the cost (it starts to say something only once per-anchor rates stop being uniform - rate scaled by city scale is the obvious trigger)
- Anchor rings only, no field - the strictest reading of 'LP has a spatial locus'

> **Recommendation:** Accept as built for now, and revisit if per-anchor rates ever become non-uniform. The lens is written so that day needs no change: the anchor ring already ramps on each anchor's share of the body maximum, it is simply 1.0 everywhere today.

*Files: `src/ui/body_surface_canvas.cpp`, `docs/ui/LENSES.md`, `docs/economy/LOGISTICS.md`*

### NR-622 — BL-606 shipped with NO live click - the container cannot approve the OS-level computer-use dialog
*observation · raised 2026-08-25 · from BL-606 (throughput lens). Third occurrence this sprint after BL-607 and its integration.*

The project rule is that a UI requirement needs a live press, not only a headless capture. It was attempted: request_access for ProjectIo returned user_denied, because nothing in this container can approve the Windows consent dialog. So the Throughput lens is verified by three headless captures (scripts/verify/throughput_lens.lua - full grid, framed anchor, and the key over an open Selection band) and a clean build, and NOT by a human press. Two specific things a capture cannot prove here: that the keyboard lens-cycle (L / Shift+L) actually reaches the new mode in a real session (the capture path sets ui.overlay directly through verify.set_overlay), and that the key does not fight the pointer over the Selection band it now floats above. Worth noting separately: the access request resolved ProjectIo to c:/users/benbo/project-io/.claude/worktrees/elated-mclean-7dd61c/build/projectio.exe - a LEFTOVER WORKTREE, not the shared checkout. That stale worktree is the same one this sprint's briefs blame for forking agents off a stale base, and it is still on disk.

> **Recommendation:** Ben presses L through to the Throughput lens once in a live session and looks at the key over the Selection band. Separately, delete the .claude/worktrees/elated-mclean-7dd61c worktree - it is now actively misdirecting tooling, not only git.

*Files: `scripts/verify/throughput_lens.lua`, `src/ui/body_surface_canvas.cpp`*

### NR-623 — lens_strip's three goldens are stale by world drift, blessed 2026-06-17 at 44b0228 - not touched by BL-606
*observation · raised 2026-08-25 · from Ran as an adjacent-regression check while landing BL-606 (throughput lens).*

lens_strip_none, lens_strip_corporation and lens_strip_production all fail at about 24 percent differing. The diff is the WORLD, not the strip: 38 nations in the golden against 43 now, balance Cr 3.2k against Cr 2.2k, a different terrain field and a different Selection card. BL-606 cannot be the cause - under overlay_mode none/corporation/production not one line of it executes and update_body_throughput early-returns. golden_log.json dates all three to 2026-06-17 at commit 44b0228, which is months of world change ago. NOT re-blessed: the golden policy is a curated world-independent set, and these three are world-dependent, so re-blessing buys one clean run and the same failure the next time the world moves.

> **Recommendation:** Retire the three lens_strip goldens to capture-only, or re-point the script at something world-independent - the strip itself is the subject, the map behind it is not. Either way it is a golden-policy call, not a fix.

*Files: `scripts/verify/lens_strip.lua`, `scripts/verify/golden_log.json`*

### NR-624 — The LP REFUSAL still has no surface - run_unit_march counts it and the count is dropped on the floor
*observation · raised 2026-08-25 · from Noticed while landing BL-606 (throughput lens), reading run_unit_march for the lens's data source.*

LOGISTICS.md section 'Refusal, surface and determinism' carries TWO duties. BL-606 discharges the second ('throughput is a lens'). The first - 'a leg over the cap fails, refused outright, AND THE PLAYER IS TOLD WHY... surfacing is non-optional, a refusal nobody sees is silent interdiction again' - is still owed. run_unit_march returns unit_march_tick::refused_no_lp, but that struct never reaches economy_report, so no UI can read it: a march refused for want of LP is, to the player, a unit that simply did not move. The Throughput lens shows where throughput is THIN, which is the standing read; it does not and cannot say 'this order was refused this tick', which is the event read.

> **Recommendation:** A small item: carry unit_march_tick into economy_report and surface the count where the player already looks at movement - the decision feed is the natural home, a refusal being exactly the class of thing it exists for. Deliberately not folded into BL-606, which owns the lens.

*Files: `src/world/economy_system.cpp`, `src/world/economy_system.hpp`, `docs/economy/LOGISTICS.md`*

### NR-625 — Two surfaces report different grid dimensions for the same body
*question · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

On one frame, for Huhaidar: the Generation Ledger's PROFILE reads 'Grid 180x84 (15120 tiles)' -- and 180*84 does equal 15120 -- while the canvas's own caption reads 'Huhaidar Planet (261x121)'. The archived roadmap's ancient-refocus note describes the map as 3x at 312x145. Three numbers, one body; at most one of them is the grid. Worth settling before either surface is written into an authority. [Renumbered from NR-598 on the 2026-08-25 Sprint 17b merge: both sessions minted that id independently for different findings, and main's was already published and cited. Same rule 17b itself applied when it renumbered its NR-589..597 to NR-607..615.]

**Why it matters.** The tile census, the placement rules and every per-body area calculation are read off whichever of these is real.

*Files: `src/ui/generation_ledger.cpp`, `src/ui/body_surface_canvas.cpp`, `docs/economy/TILES.md`, `docs/generation/TILE_GENERATION.md`*

### NR-626 — Every decision in the AI feed reads 'overridden' with a chosen score of 0.00
*question · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

The decision feed's visible entries are all one shape: a build candidate, the reason 'best available build site', a score line reading '0.00 v 2.08' or '0.00 v 0.08', and the word 'overridden' in red. If the first number is the chosen candidate's score, the scorer is taking a zero-scored option over a positive one in every logged case; if it is not, the readout does not say what it is. Either the feed is mislabelled or the scorer is doing something worth knowing about. [Renumbered from NR-599 on the 2026-08-25 Sprint 17b merge: both sessions minted that id independently for different findings, and main's was already published and cited. Same rule 17b itself applied when it renumbered its NR-589..597 to NR-607..615.]

**Why it matters.** The feed is the only window onto rival reasoning, and 'the credible rival' is a named theme. A readout that always says the same thing cannot distinguish a working scorer from a broken one.

*Files: `src/ui/decision_feed.cpp`, `src/world/corp_ai.cpp`, `docs/ai/AI_OPPONENT.md`*

### NR-627 — Goldens deliberately NOT blessed for the shell pass, despite the exception granted
*decision taken on your behalf · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

Ben granted an exception to the curated-world-independent golden policy for this pass, on the grounds that ubiquitous UI items are essentially world-independent. Held anyway, for two reasons. (1) The captures are FULL FRAMES and the canvas fills most of every one, so a golden over them is world-dependent whatever the chrome does -- a generation change would fail all 31 for reasons with nothing to do with the UI. (2) The pass exists to decide what several of these surfaces should become; blessing now pins the look we are about to change, and the first real alteration then re-blesses everything, which is the bulk re-bless the policy exists to prevent. Recommendation: bless after the alterations, and only over frames where chrome dominates -- or give the golden harness a capture REGION so chrome can be diffed without the canvas behind it. [Renumbered from NR-600 on the 2026-08-25 Sprint 17b merge: both sessions minted that id independently for different findings, and main's was already published and cited. Same rule 17b itself applied when it renumbered its NR-589..597 to NR-607..615.]

**Why it matters.** The grant is cheap to spend once. Spending it on a state we are about to overwrite wastes it and leaves 31 stale goldens behind.

*Files: `.claude/skills/verifier-visual/SKILL.md`, `docs/development/DEVELOPMENT_PRACTICES.md`*

### NR-628 — RESEARCH.md placed under docs/economy/ with a router row - location was a delegated call
*decision taken on your behalf · raised 2026-08-25 · from The 2026-08-25 population design form: Ben chose 'new RESEARCH.md' and asked for the stub now, without naming a path.*

Placed at docs/economy/RESEARCH.md (RP is produced by a building and spent in the economy/era gate) and a row added to CLAUDE.md's Economy table. Alternatives were docs/tech/ (rejected: that folder is engineering foundations, not game systems) and docs/research/ (rejected: explicitly non-authority scaffolding). Overturn by moving the file and the router row.

*Files: `docs/economy/RESEARCH.md`, `CLAUDE.md`*

### NR-632 — BL-615 leaves owed UI wiring: build-door locks, preview gates, naming/colour rows for schooling and university
*observation · raised 2026-08-25 · from Agent P's report - enforcement is at construct_building only; UI preview call sites pass a default (empty) gate.*

Three one-line-each wirings owed to the main session at integration: the door/preview call sites (body_surface_canvas.cpp:3825, selection_panel.cpp:3729) should resolve the real gate so the new reason-coded locks render; presentation.cpp naming and icons.cpp glyph switches need rows for building_type 8/9 (currently 'None'/neutral). Tracked in REFINED.md wave 3; BL-615's R2 visual requirement (live click on the locks) closes only after this.

*Files: `src/ui/body_surface_canvas.cpp`, `src/ui/selection_panel.cpp`, `src/ui/presentation.cpp`, `src/ui/icons.cpp`*

### NR-642 — BL-623/BL-624 delegated calls: two-call partition, C2's roaded class deleted, scale-1 ruins, migration skips razed, demand skip in market_clearing
*decision taken on your behalf · raised 2026-08-25 · from Agent W3's report, commits ef20f9fe / d8f7ed3a.*

Five calls, each overturnable: (1) the homeworld partition runs early (before roads) AND the canonical whole-world rebuild still runs at end of generation - byte-identical for early bodies since the fill reads no roads and anchors are seed-skipped; P6 guards the identity (Selene/Pallas are authored after the homeworld political pipeline, forcing the two-call shape). (2) The partition harness's roaded-edge instrument class (C2c) was DELETED rather than kept as a report row - against a pre-road partition it measures settlement clustering, not a rule. (3) A razed centre keeps SCALE 1 (not 0) so the strictly-positive anchor scan still finds the ruin; re-razing a ruin is rejected_state. (4) Migration ignores razed centres in both directions - re-settlement is the growth pass's job alone. (5) BL-624's demand skip landed in market_clearing.cpp, one file beyond the item's list (inject_population_demand lives there).

*Files: `src/world/hard_coded_world.cpp`, `src/world/province.cpp`, `src/world/market_clearing.cpp`*

### NR-643 — Population-centre fields (razed, growth_accumulator, province_anchor) are outside world::state_hash - pre-existing scope, observed again
*observation · raised 2026-08-25 · from Agent W3, in passing (BL-624).*

state_hash folds the fields a tick may mutate, but the population-centre record's newer fields (BL-616's accumulator, BL-624's razed) are not in it - divergence in them would not move the hash. Today the harness determinism rows compare the fields directly, so drift is still caught; if state_hash is ever leaned on as the sole replay check for centre state, fold them in (a hash-baseline move, so do it deliberately with a re-bless).

*Files: `src/world/world.cpp`*

### NR-645 — logistics_harness T7 dispatch block (6 assertions) fails at pristine HEAD - pre-existing drift, not the flood-field change
*observation · raised 2026-08-25 · from Warm-start stall fix session (flood-field pathfinding): harness sweep before commit.*

The six T7/T8 intra-body dispatch assertions (shortfall dispatches one convoy, cost debit, cost arithmetic 0.4, the two 0.352 discounts, decommissioned-hub no-discount) fail identically when the harness is built against unmodified HEAD (5778a932) - verified in a throwaway worktree. So the drift predates the 2026-08-25 flood-field change; the likely suspect is a dispatch-gate landed since the harness's synthetic worlds were authored (e.g. BL-597's passive-LP draw refusing anchorless bodies), but that is unconfirmed. Needs a session to rebase the harness's synthetic worlds onto the current dispatch contract - not a weakening of the assertions.

*Files: `tools/verify/logistics_harness.cpp`*

### NR-646 — Disclosure is BINARY - reconciling 'private firms do not file' with 'retire the bands'
*decision taken on your behalf · raised 2026-08-26 · from Sprint 20 design session, 2026-08-26: the profitability ledger, the buyout, the spawn shortlist.*

Two of the same form's rulings pull opposite ways read literally: disclosure is gated by ownership class (public files, private/closed do not), AND BL-262's banding is retired because company information need not be invisible. Taken on Ben's behalf: disclosure is BINARY. A public firm shows exact figures, a private or closed one shows a dash, and there is no graded middle - which is precisely WHY the bands go rather than being re-scoped. A dash therefore always means 'this firm does not file', never 'you have not earned this'. Overturnable: the alternative is that private firms show a band, which keeps the banding machinery alive.

*Files: `docs/economy/FINANCE.md`, `docs/politics/RELATIONS.md`*

### NR-648 — The buyout price is a SINK, and that is why the floor is zero
*decision taken on your behalf · raised 2026-08-26 · from Sprint 20 design session, 2026-08-26: the profitability ledger, the buyout, the spawn shortlist.*

A whole-firm price needs a floor, and there is no salvage in the prototype - construction.hpp refunds nothing on demolition and calls salvage a separate design question - so flooring at book value would invent a redemption nobody can take. Taken on Ben's behalf: the acquisition price is a SINK, the same treatment construction already gives a build cost (FINANCE.md calls a levy a transfer specifically BECAUSE an ordinary spend is not), and the floor is therefore zero because a sink cannot run backwards. A public firm's sellers are a diffuse shareholder base, not a modelled actor, so there is nobody a negative price could be paid by. The alternative worth considering: the price is a TRANSFER to the target's home nation treasury, which would give nations a stake in corporate churn.

*Files: `docs/economy/FINANCE.md`*

### NR-650 — The purged syndicate arc was still asserted by five authority docs three days after the purge
*observation · raised 2026-08-26 · from Sprint 20 design session, 2026-08-26: the profitability ledger, the buyout, the spawn shortlist.*

BL-524..BL-530 (the syndicate/equity tier) were purged on 2026-08-23, but GLOSSARY.md still carried a full Syndicate entry, and CONCEPT.md, SYSTEMS.md, RELATIONS.md and ROADMAP.md each asserted the two-tier ownership model as settled design. This session removed all five, since Ben's whole-firm ruling overturns the model outright. The general lesson is the one worth Ben's eye: a PURGE moves backlog prose but does not touch the authority docs the purged items had already written into, so a purge needs a doc sweep the way a landing does. PHANTOMS.md still carries five syndicate mentions - left alone deliberately, since it is a scan record of what was designed, not an authority. SECOND CONSUMER FOUND, same session: story_check.js reports 50 failures, every one a user story tracing to a retired id (BL-428, BL-429 and the rest of the BL-001..BL-568 range). USER_STORIES.md is the second route from docs to code, and the purge cut it at the knees without anyone noticing for three days. Same lesson, now twice: a purge moves backlog prose and leaves every CONSUMER of a backlog id pointing at nothing - authority docs in one direction, user stories in the other. Pre-existing and not this wave’s; recorded so the sweep, when it happens, covers both.

*Files: `docs/GLOSSARY.md`, `docs/CONCEPT.md`, `docs/SYSTEMS.md`, `docs/politics/RELATIONS.md`, `docs/development/ROADMAP.md`, `docs/development/PHANTOMS.md`*

### NR-651 — Valuation lives in FINANCE.md, not MARKETS.md - a whole firm has no order book
*decision taken on your behalf · raised 2026-08-26 · from Sprint 20 design session, 2026-08-26: the profitability ledger, the buyout, the spawn shortlist.*

The purged BL-527 put corporation valuation in MARKETS.md on the argument that an equity price IS a price and belongs in resolve_price's existing machinery - band, EMA smoothing, order book. That argument depended on FRACTIONAL shares trading repeatedly. Ben's whole-firm ruling removes the repeated trade: a firm changes hands once, at a computed price, with no book and no counterparty to clear against. Taken on Ben's behalf: the valuation formula lives in FINANCE.md beside the money loop it reads, and MARKETS.md is untouched. Revisit only if fractional stakes ever return.

*Files: `docs/economy/FINANCE.md`, `docs/economy/MARKETS.md`*

### NR-652 — NOVEL: ownership transfer is a mechanic class no authority doc owned before this session
*novel-work · raised 2026-08-26 · from Sprint 20 design session, 2026-08-26: the profitability ledger, the buyout, the spawn shortlist.*

Flagged at the moment it arose, per the novelty rule. Every prior verb moves goods, credits, units or sentiment; the buyout moves an ACTOR - a corporation stops existing and its entire asset graph re-points. Nothing in the corpus owned that before 2026-08-26 (the nearest thing was the purged syndicate arc, and it deliberately avoided transfer by holding equity instead). It now has an owner - FINANCE.md § Whole-firm acquisition - so the flag is about SCOPE, not homelessness: dissolution touches the HQ/influence-range recompute, unit muster bases, open market orders, the save format and every surface that resolves a corp id. BL-628 is sized L for that reason and it is the item most likely to be under-scoped.

*Files: `docs/economy/FINANCE.md`, `src/world/corp_command.cpp`*

### NR-654 — ALL THREE wave-1 worktrees were cut from the session-start commit, not current HEAD
*observation · raised 2026-08-26 · from Sprint 20 wave 1: three agents launched in one message, BL-637 reported it could not read its own backlog item or requirement group.*

Three worktree agents were launched in a single message from HEAD e40e38d8. ALL THREE were cut from 638cdd65 instead - the commit HEAD sat at when the SESSION began, five commits earlier. The main session's merge-base check looked clean for two of them only because those agents had ALREADY fast-forwarded themselves before the check ran; the correction is recorded here rather than left standing. Two of the three noticed and said so - BL-637 reported it could not read its own backlog item or requirement group, BL-626 reported the same and fast-forwarded to main before starting any work. That is two independent confirmations of a systemic behaviour, not one worktree's bad luck. THE RISK IS NOT HYPOTHETICAL: both src/ agents were briefed to read authority-doc sections written EARLIER THE SAME SESSION, which do not exist at 638cdd65. An agent that did not check would have built against a design it could not see and reported success. This is the io-worktree-agents-stale-base pattern recurring. Proposed standing check, cheap and mechanical: before trusting any wave, run git merge-base <branch> main for every launched worktree, and re-brief or relaunch any agent whose base predates the design it was told to read. Better still, brief every agent to fast-forward to main first and say so - the two that survived did exactly that on their own initiative. UPDATE, same session: a third confirmation arrived - BL-631 also reported basing on 638cdd65 and merging main before starting. Three of three. The pattern is not that some worktrees go stale; it is that they ALL did, and only the agents that checked survived it.

*Files: `docs/development/DELIVERY.md`*

### NR-656 — book_value is flat build_cost, NOT what the build press charges - and the agent was right to diverge
*decision taken on your behalf · raised 2026-08-26 · from Sprint 20 wave 1, BL-626: flagged as an assumption by the agent, checked in the main session.*

FINANCE.md § The quarterly return says book_value is 'the same construction cost the build press already charges - one number, one authority.' It is not, and it should not be. construction.cpp:132 charges econ.build_cost + material_cost, and material_cost (construction.cpp:120-126) values the resource build cost AT THE CURRENT MARKET PRICE. Folding that into book value would make a firm's balance sheet mark-to-market: the same building's book value would move every quarter with commodity prices the firm does not own, and BL-628's buyout price would swing with it. The agent implemented the flat build_cost and flagged the divergence rather than silently matching the doc. Taken on Ben's behalf: the IMPLEMENTATION is right and the DOC is wrong - book value is historical cost, deliberately stable, and FINANCE.md's sentence is being corrected rather than the code. Overturnable: if you want the materials in, it is a one-line change, but the buyout price becomes commodity-sensitive.

*Files: `docs/economy/FINANCE.md`, `src/world/budget_system.cpp`*

### NR-657 — net is the balance difference, not corp_budget::net() - a ULP apart, and only one of them telescopes
*decision taken on your behalf · raised 2026-08-26 · from Sprint 20 wave 1, BL-626: the agent's one stated design call.*

FINANCE.md's field table names the source as corp_budget::net() while describing the value as 'the exact balance delta'. Those are not the same float: apply_budget accumulates its delta interleaved and its own comment forbids re-grouping, so net() can differ by a ULP. Only the difference of two consecutive balances telescopes exactly, which is what the retain property demands. The agent chose the balance difference and recorded why in the struct's doc comment. Endorsed - the property is the point of the record, and a doc sentence that names a source is worth less than a record that cannot lie. FINANCE.md's table is being corrected to say so.

*Files: `docs/economy/FINANCE.md`, `src/world/components.hpp`*

### NR-659 — The default campaign classes EVERY corporation closed - nothing files, nothing is buyable, and the sprint goal is unreachable there
*question · raised 2026-08-26 · from Sprint 20 wave 1, BL-631 (ownership class): the agent measured it; the main session traced the cause and checked the fix target.*

MEASURED across 8 seeds. At the default epoch (0 CE) every one of 64 corporations and all 1298 regions class `closed`. At epoch 1960 the spread appears (public 4, private 11, closed 49). CAUSE: settlement.cpp:441/470 does `if (antiquity) break;` for stop_year < 1700, so no furnace ever lights and median_industrial_year stays 0 - every region reads as never-industrialised, which is Pass 2b's third rung firing CORRECTLY on a degenerate input. CONSEQUENCE: on the default world the profitability ledger is empty, nothing is buyable, and Sprint 20's whole goal - save up, buy a firm, stay profitable - cannot happen. BL-627, BL-628 and BL-634 all rest on this. THE FIX LOOKS PRECISE AND SMALL. Pass 2b reads the ENERGY TRANSITION (HISTORY.md Stage 4), which antiquity skips - but the design's own justifying sentence names the ENFORCEABLE PROMISE (Stage 1): 'the institutions that make a contract enforceable are the institutions that make a share transferable.' Stage 1 runs in the ladder pass (Stages 0-2), which antiquity does NOT skip, and its output already exists as history_ladder::charter_cradle (history_ladder.hpp:92) - the Charter Act, first perpetual company registered, which is literally corporate personhood. Recommend re-pointing the derivation from Stage 4 to Stage 1. Same shape, same no-new-machinery property, and it makes the mapping era-agnostic. WORTH SAYING PLAINLY: this is the ERA-RELATIVE GATES class that Sprint 19's retro named as its lesson - an absolute or late-era signal read in an era where it does not exist, producing a legitimate-but-wrong world. It recurred one sprint later, in a design written this morning, and was caught the same way: because the agent measured the distribution and NAMED the consequence instead of shipping it. Related: NR-641 (all-Track antiquity world).

*Files: `docs/generation/CORPORATION_GENERATION.md`, `src/world/corporation_generation.cpp`, `src/world/settlement.cpp`, `docs/lore/HISTORY.md`*

### NR-660 — NOVEL: the public-class floor is UNMEETABLE by construction on the default world - a waiver was applied by analogy
*novel-work · raised 2026-08-26 · from Sprint 20 wave 1, BL-631: the agent raised the novelty flag; the main session endorses the call.*

Pass 2b added a second condition to Pass 2's world-level reject-and-reroll: at least one public specialist. On the default world (see NR-659) no region can ever yield public, so the floor is unmeetable BY CONSTRUCTION - every default world would have burned all six attempts and stood on attempt 5's region set, silently relocating every corporation in service of a condition that cannot be satisfied. No doc owns 'what happens when a floor is unmeetable by construction'; Pass 2b assumes public is achievable. The agent waived the floor when no region any corp could anchor to yields public, by analogy with the waiver the focus floor already carries for corp_count < 3, and called it out in code and in the commit rather than burying it. ENDORSED: it is the same idiom, applied to the same kind of impossibility, and the alternative was a silent reshuffle of every default world. It becomes moot if NR-659's re-pointing lands, since public becomes reachable again - but the waiver should stay, because 'the floor is unmeetable here' is a real state and standing on attempt 5 for it is not an outcome anyone chose.

*Files: `src/world/corporation_generation.cpp`, `docs/generation/CORPORATION_GENERATION.md`*

### NR-661 — spectator_determinism's golden was ALREADY stale on main before wave 1 touched anything
*observation · raised 2026-08-26 · from Sprint 20 wave 1, BL-631: the agent isolated it against main's own sources.*

spectator_determinism fails its 81ADF917369317C7 row. The agent built the harness against MAIN's unmodified sources and got the identical actual value (C91FB6ADA80B65CE) and the identical failure, so the drift predates this wave. It correctly did NOT re-bless - a golden re-blessed by whoever happens to trip over it is a golden nobody reviewed. Someone needs to find which landed change moved it and re-bless deliberately, with dated provenance, per the harness's own provenance-log idiom. Note the standing rule attached to this harness: the two properties it guarantees are that the flag DEFAULTS FALSE (an ordinary session is byte-identical) and that admitting one more corp shifts no rival's cadence slot - a state_hash constant is dated evidence, not the invariant itself.

*Files: `tools/verify/spectator_determinism.cpp`*

### NR-662 — The disclosure rule as written locks the PLAYER out of its own balance sheet
*decision taken on your behalf · raised 2026-08-26 · from Sprint 20 wave 2, BL-633: the agent implemented the rule literally, saw the consequence, and asked rather than carving out an exception on its own.*

I wrote 'capital is exact for a public corporation, absent for a private or closed one' into FINANCE.md and RELATIONS.md with NO carve-out for the observer's own firm. The agent implemented it literally and correctly - and on a default antiquity world, where every corp is closed, THE PLAYER CANNOT READ THEIR OWN CAPITAL in the Corporations panel. That is my error, not the implementation's. Taken on Ben's behalf and written into both docs: disclosure is a rule about reading ANOTHER firm. A corporation always reads its own books, whatever its class - a firm that could not would be unrunnable, and a closed firm reading its own books while publishing none is exactly what closed MEANS. The code change is one line (is_player/owner exemption on capital_disclosed) and is owed with BL-633's merge. Overturnable, but I cannot construct the argument for the other side.

*Files: `docs/economy/FINANCE.md`, `docs/politics/RELATIONS.md`, `src/world/standing.cpp`, `src/ui/corporation_panel.cpp`*

### NR-663 — expect_no_clipping reported CLEAN while a genuinely clipped button was on screen
*observation · raised 2026-08-26 · from Sprint 20 wave 2, BL-633: the agent flagged its own green row as vacuous, and separately reported the real defect.*

Two findings that are only interesting together. (1) The agent's acceptance script passed expect_no_clipping with '0 failure(s), 0 record(s) total' - the overflow ledger saw NOTHING, so the pass is vacuous, and the agent said so rather than banking it. (2) In the same session it reported a real, visible layout defect in the same panel: the Corporation name column is squeezed to a SINGLE LETTER and the Stance column's 'Declare Hostile' button clips off the right edge as 'De...'. So the clipping detector returned clean on a frame that contained actual clipping. Cause: ImGui SmallButton labels are not instrumented into the overflow ledger. THIS IS THE EXACT FAILURE CLASS THE LIVE-CLICK RULE EXISTS FOR - BL-449 shipped on a compile and a 36/36 harness and was unusable for precisely this reason - and it means the automated substitute we have for a live click cannot currently see the defect that motivated the rule. Worth instrumenting SmallButton, or the ledger's clean verdict will keep meaning less than it appears to. The layout defect itself is BL-639. THIRD INSTANCE, found 2026-08-26 while PROVING the NR-686 fix: the build wrapper returned EXIT CODE 0 on a run where ninja reported LNK1104 and stopped. So the family is now: a clipping check that passes vacuously, a build that copies nothing while printing BUILD_OK, and a wrapper that reports success on a failed link. All three are in the VERIFICATION LAYER rather than the game, and all three share one shape - a check whose green means less than it appears to. Worth treating as one problem in Sprint 21 rather than three notes.

*Files: `src/ui/`, `scripts/verify/`, `tools/verify/`*

### NR-665 — The private demote admits STATIST only, not the never-industrialised character - a departure from Pass 2b's literal text
*decision taken on your behalf · raised 2026-08-26 · from Sprint 20 wave 2, BL-638: the agent departed from the doc, said so, and gave the reason.*

Pass 2b as I wrote it said 'a statist OR ISOLATIONIST character lands here [private]'. The agent admitted `authoritarian` only and excluded `isolationist`, because nation_component::politics IS the industrialisation-timing tercile and `isolationist` is its NEVER rung - so on an antiquity world every nation is isolationist, and demoting on it would have re-created the exact degenerate distribution one rung down: every corporation `private` instead of every corporation `closed`. In its words: 'Nobody built a furnace is a statement about industry, not corporate law; treating it as one smuggles Stage 4 back in through the polity term.' ENDORSED and written into the doc. This is the SAME trap BL-638 exists to fix, reappearing in a different term of the same expression - which is worth noticing on its own: the industrial signal is threaded through more of generation than the obvious call site, so a future rule reading `politics` is reading industrialisation timing whether it means to or not. The fallback path (ownership_from_character) is untouched, where isolationist -> closed still holds.

*Files: `docs/generation/CORPORATION_GENERATION.md`, `src/world/corporation_generation.cpp`*

### NR-667 — ownership_class values change on every seed - any state_hash golden over the corp store will move
*observation · raised 2026-08-26 · from Sprint 20 wave 2, BL-638: flagged by the agent, unresolved at its report.*

BL-638 changes the VALUE of a serialised field on every corporation in every seed. charter_reach itself is generation-time only and unserialised, so the format is untouched - but any golden that hashes the corporation store moves. The agent correctly did not re-bless anything and did not run spectator_determinism or a state-hash harness. This must be checked at the merge and folded into the sprint's ONE deliberate re-bless wave with dated provenance - not re-blessed here, and not dribbled. Note NR-661: spectator_determinism's golden was ALREADY stale on main before this sprint, so a mover found there is not necessarily this change.

*Files: `tools/verify/spectator_determinism.cpp`, `src/world/corporation_generation.cpp`*

### NR-669 — The zero floor laundered a NaN into a FREE firm - caught by the harness, on the AI-facing seam
*observation · raised 2026-08-26 · from Sprint 20 wave 2, BL-628: the agent found it in its own code and fixed it before reporting.*

Worth recording as a pattern, not just a fixed bug. The price is max(0, book + k x trailing_net + balance). Applying the floor FIRST meant a NaN - reachable from a corrupt filed return - compared false against 0, so max returned 0.0f, and the seam's own isfinite guard then saw a clean number and let it through. A corrupt record bought a firm for FREE. The fix is one line (propagate non-finite, then floor) and is now pinned by two harness rows. THE GENERAL SHAPE: a sanitising operation placed BEFORE a validity check can launder the invalid value into a valid-looking one, and this is exactly the class the standing rules' AI-facing-seam paragraph exists for - validate as the value that lands, reject whole, never clamp. Here the clamp WAS the vulnerability. Written into FINANCE.md so the ordering is design, not an implementation detail someone can reorder later.

*Files: `docs/economy/FINANCE.md`, `src/world/corp_command.cpp`, `.claude/rules/io-standing-rules.md`*

### NR-671 — The ancient economy has TWO live demand sinks - mercantile demand is named as a consumer everywhere and implemented nowhere
*question · raised 2026-08-26 · from Ben's question, 2026-08-26: 'how much profit do we really expect to make anyway - do we have a list mapping resources to consumers?' Built the map from scripts/recipes.lua, economy.lua and world_gen.lua.*

There was no such list. The CONSTRAINT exists - PRODUCTION.md admission rule, chain_depth R1/R1b - but not the map. Built, and it explains BL-635s residual completely. FOUR consumer channels exist in code: recipe inputs; population_demand (food_rations .60, agricultural_produce .20, water .30, clean_water .35, consumer_goods .25, medical_supplies .15); background_demand (silicon, refined_copper, ree_alloy, machinery, alloys, electronics - ALL INDUSTRIAL, so entirely inert at 0 CE); unit upkeep (ordnance, now 0.0035/head after BL-635); and construction materials (refined_fuel, steel, stone, timber). IN THE ANCIENT BAND that leaves essentially TWO live sinks: population wants food_rations and agricultural_produce (+water); construction draws stone and timber. Everything else terminates dead. SEVEN goods are produced in-band with no sink at all: trade_goods_misc 8.00, ceramics 3.40, dressed_stone 2.90, tools 25.50, leather 7.20, rigging 14.50, plus ordnance 43.00 whose only draw is now negligible. FOUR more are extractable raws with no sink in any band: tobacco, spices, coffee, furs. THE MECHANISM GAP: chain_depths exemption table (k_actor_consumed) exempts ten of these from the orphan check by naming their consumer as MERCANTILE DEMAND. Grep says mercantile demand does not exist - market_clearing.cpp has exactly three injections (population, background, interbody) and none is it. So the guard passes because the exemption asserts a consumer that was never built. Honest in intent (BL-586 named these terminal, sold) but sold requires a buyer. CONSEQUENCE: the ancient value chain is stone->dressed_stone DEAD, timber->planks->tools DEAD, clay->ceramics DEAD, hides->leather DEAD, fibre->cloth->rigging DEAD, sand->glass->trade_goods_misc DEAD; only agri->food_rations->population and the charcoal->blooms->steel->construction line are live. Which is EXACTLY BL-635s measurement: iron_ore and agricultural_produce sites run 80/80, stone/timber/fibre idle 80/80. Spawn viability is therefore not a constant to tune - the demand side of the ancient economy was never authored. The supply side landed 2026-08-24 (BL-585/BL-586); its demand counterpart did not.

*Files: `scripts/economy.lua`, `scripts/recipes.lua`, `src/world/market_clearing.cpp`, `tools/verify/chain_depth.cpp`, `docs/economy/PRODUCTION.md`*

### NR-672 — Under spectate, the camera-anchored corp discloses its capital while nothing owns it
*observation · raised 2026-08-26 · from Sprint 20 wave 2 review barrier, suggestion 8.*

The NR-662 self-exemption reads `cs.is_player`, which derives from world::player_entity. Under spectator mode (BL-409) that field degrades to a camera/ledger anchor with NO ownership meaning - the standing rules say so explicitly. So under spectate exactly one corp discloses its capital for no reason other than where the camera is pointing, which is a small incoherence rather than a leak: BL-408's god view already makes everything readable there, so nothing is hidden that spectate does not intend to show anyway. Recorded rather than fixed because the right fix depends on a question worth asking once: should ANY disclosure gate consult player_entity under spectate, or should the whole disclosure layer be bypassed there the way the fog already is? The second is tidier and matches the precedent.

*Files: `src/world/standing.cpp`, `docs/ui/DISCOVERY.md`*

### NR-673 — NOVEL: a check that forces PRODUCTION CODE to declare what it does
*novel-work · raised 2026-08-26 · from Sprint 21 wave 0, BL-648: the agent raised the flag; the main session endorses the pattern and thinks it generalises.*

BL-648's registry substantiates an exemption by reading the DATA the running pass multiplies by - four of five probes read Lua-authored vectors, so a demand channel landing in economy.lua flips a row green with no harness edit at all. The fifth could not: a convoy launch's propellant burn was a private C++ constant, and no amount of reading Lua finds it. The agent's three options were to re-type the constant into the harness (rebuilding the exact loophole the item exists to close), leave propellant falsely red, or make the PASS DECLARE ITS DRAW - exporting launch_draw_per_convoy so the gate, the debit and the registry became one object. It took the third, which is right, and it is a PATTERN no doc in the corpus owns: a verification requirement reaching back into production code and changing its shape so that what it consumes is declared rather than inferred. Worth Ben's eye because it generalises - every future 'does anything actually do X' guard faces the same three options, and the third is the only one that cannot rot. The residual risk is named in the code: deleting the draw while leaving the vector standing. Far narrower than a prose string.

*Files: `tools/verify/chain_depth.cpp`, `src/world/supply_system.hpp`, `docs/development/DEVELOPMENT_PRACTICES.md`*

### NR-674 — haulage_measure's convoy baseline has moved - 1368/1014 against the noted 1055/802
*observation · raised 2026-08-26 · from Sprint 21 wave 0, BL-648's verification sweep.*

haulage_measure reports 0 failures at 1368 convoys dispatched, 1014 intra-body, against a previously-noted baseline of 1055/802. The agent correctly did not re-bless anything and named the gap. It matters more than an ordinary number drift because of what this harness IS: convoy traffic can collapse with every state_hash golden digit-identical, so haulage_measure against its baseline is the only real check that trade is still happening. A baseline that has silently moved by ~30% is therefore a check that has quietly stopped meaning what it meant. Sprint 19's population work (~40x centre density) and Sprint 20's road and levy changes are the obvious candidates and neither was chased. Someone should establish the current number deliberately, with provenance, rather than letting the next reader compare against a figure from a different world.

*Files: `tools/verify/haulage_measure.cpp`*

### NR-677 — AI_OPPONENT.md § 10a describes MCP as it was before the 2026-07-28 spec made it stateless
*observation · raised 2026-08-26 · from Found by the 2026-08-26 AI SOTA sweep, which correctly declined to promote it - it predates the sweep's window and is a doc-accuracy gap rather than a field development.*

The MCP spec of 2026-07-28 made the protocol STATELESS: it removed `Mcp-Session-Id` and the initialize handshake from Streamable HTTP, and deprecated Roots / Sampling / Logging on a 12-month clock. § 10a's description of MCP was written before that release. It is not WRONG in what it claims about Io's own server, and `tools/mcp/server.js` is unaffected (the 2026-08-22 roadmap keeps stdio first-class and extends the HTTP approach to local servers speaking Streamable HTTP over stdio, so no action falls out of that either). But the section describes a protocol shape that has since changed, and a deprecation on a 12-month clock is the kind of thing that is cheap to note now and expensive to discover later. Worth a read and a small amendment when someone is next in that file - not urgent, and explicitly NOT a finding of the sweep, which was right to keep its diff clean.

*Files: `docs/ai/AI_OPPONENT.md`, `tools/mcp/server.js`*

### NR-678 — I wrote 'electronics' into the industrial household basket in two docs; the data has it in background demand
*decision taken on your behalf · raised 2026-08-26 · from Sprint 20 wave 4, BL-640: the agent found the doc and the data disagreeing and correctly declined to resolve it by editing either the design or the weights.*

POPULATION.md § Population demand and MARKETS.md § Demand channels both said an industrial household wants 'consumer goods, medical supplies and ELECTRONICS'. The industrial tranche is actually clean_water / consumer_goods / medical_supplies; electronics lives in background_demand. I wrote that sentence as illustration this morning and it became a claim. Taken on Ben's behalf: both docs now name what the basket really carries, because a doc asserting a weight that does not exist is worse than a thin basket. BUT THE UNDERLYING QUESTION IS REAL AND IS BEN'S: should an industrial household consume electronics directly? It is plausible on its face - consumer electronics are a household good - and it would give electronics a demand channel that scales with population rather than with the background-industrial stopgap. Adding it is one Lua row. Not done, because which goods a household wants is a content decision about what the game is about, not a typo fix.

*Files: `docs/economy/POPULATION.md`, `docs/economy/MARKETS.md`, `scripts/economy.lua`*

### NR-680 — rigging's exemption named BL-640 as its buyer, and BL-640 does not buy it
*question · raised 2026-08-26 · from Sprint 20 wave 4, BL-640: the agent declined to invent a buyer or to widen its own requirement to cover one.*

chain_depth's exemption for `rigging` claimed 'the era-banded household basket (BL-640) owns the buyer'. It does not: POPULATION.md's ancient household is ceramics, cloth, leather and dressed stone, and rigging is ship's tackle - not a household good by any reading. The agent re-pointed the claim prose to say so and LEFT THE ROW RED rather than widening R2's four goods to make its own item look better. Correct, and it leaves rigging genuinely unowned. Candidates worth weighing: the Infrastructure channel (BL-643) if ports and hubs draw materials - a shipwright's output is closer to infrastructure than to a household; or Construction (BL-642) if a port's resource_costs name it. Three of the four remaining red goods now have this shape - rigging, tools and trade_goods_misc all need a channel to claim them, and ordnance needs BL-646.

*Files: `tools/verify/chain_depth.cpp`, `docs/economy/LOGISTICS.md`, `docs/economy/PRODUCTION.md`*

### NR-682 — The building-upkeep state_hash fold is SPARSE - a deliberate departure from BL-454
*decision taken on your behalf · raised 2026-08-26 · from Sprint 20 wave 4, BL-641: taken by the agent, documented in code, and flagged rather than buried.*

BL-641 folds building supply_factor into world::state_hash only where a building is NOT fully supplied. It is complete as a DETECTOR - any differing factor means at least one side is non-neutral and folds (id, value) - but it contributes nothing in a world where the mechanism never fires, which is why no golden moved and spectator_determinism still reads E350DF2A50BF4BAA. BL-454 took the other route for unit upkeep and accepted moving the golden. The sparse fold is cheaper and, with rates at 0.0, keeps this landing invisible to every golden - which is exactly what you want from a shape that ships inert. The cost is an inconsistency between two folds doing the same job, and the question of which is the house pattern is unanswered. Overturnable; if the rates ever go non-zero the two should probably agree.

*Files: `src/world/world.cpp`, `src/world/components.hpp`, `tools/verify/spectator_determinism.cpp`*

### NR-683 — The committed census baseline is format-stale after one day
*observation · raised 2026-08-26 · from Sprint 20 wave 4, BL-641: the agent declined to re-bless a committed baseline, correctly.*

docs/development/baseline_census_2026-08-26.txt was committed this morning as the durable before for Sprint 20/21's tuning passes. BL-641 added a column (industry) and a diagnostic line, so the file no longer matches the report's format - the ECONOMIC figures still compare cleanly, which is what matters, but a naive diff now shows format noise alongside signal. The agent did not re-bless it, which is right: a baseline someone re-blesses whenever the format moves is not a baseline. Worth deciding once how this file ages - regenerate it at each wave boundary with a dated filename and keep the series, or pin the comparison to the economic rows only and let the format drift. The second is less work and more robust.

*Files: `docs/development/baseline_census_2026-08-26.txt`, `tools/verify/demand_census.cpp`*

### NR-684 — The equal-tranche invariant was counting UNITS, not value - the ancient band was half the cost of living
*observation · raised 2026-08-26 · from Sprint 20 wave 5, BL-655: found while sizing the retune, and the fix is why the numbers are defensible.*

BL-640 sized the ancient household tranche to total 0.75, equal to the industrial tranche's 0.75, on the reasoning that the band should change WHICH value chain a household consumes and not HOW MUCH. The invariant was counting units. Priced at base_price, the ancient household cost 3.12 cr per scale-point against the industrial household's 6.15 - so the band was halving the cost of living, which is exactly what it was written not to do. BL-655 re-sized against VALUE: the ancient tranche now costs 6.145 against 6.150, and a head costs 10.80 cr in either band. Worth recording as a pattern, not just a fix: an invariant stated in the units of the data (weights) rather than the units of the meaning (credits) reads as correct and measures as wrong, and this one survived a review and a merge before anybody priced it.

*Files: `scripts/economy.lua`, `docs/economy/POPULATION.md`*

### NR-685 — Density stopped at 88 of 104 on purpose - the last two goods belong to other items
*decision taken on your behalf · raised 2026-08-26 · from Sprint 20 wave 5, BL-655.*

The retune reached 352 buildings / 88 corps against the pre-BL-640 584 / 104. It stopped there deliberately. The next two goods that would each add a tranche - `tools` and `trade_goods_misc` - are real ancient household candidates and both are in-band, but their buyers are OWNED by BL-590 (construction draw) and BL-647 (endemic luxury). Taking them would have hit 104 and quietly annexed two other items' designs, which is how a count gets reached and a roster gets confused. `peat` was the third candidate and the price instrument vetoed it - already 6.90x base, so household demand would have deepened a shortage. ENDORSED. If Ben wants 104 sooner, the honest route is to land BL-590 and BL-647 rather than to widen this basket, and the count then follows from those items rather than being borrowed against them.

*Files: `scripts/economy.lua`, `docs/development/backlog.json`*

### NR-687 — The pre-game manufactures ZOMBIES - interest has no consequence attached, so a failing firm never fails
*question · raised 2026-08-26 · from Ben, 2026-08-26, on the live build: 'still seeing a debt... mostly caused by compounding interest, but it should still not be happening.' Measured immediately after across 12 seeds.*

BEN IS RIGHT AND THE ATTRIBUTION CONFIRMS IT. Of 12 swept seeds, 3 clear - and all three carry ZERO interest, income 68-104 cr/qtr, balance +2.1k to +3.9k. The other 9 are all marked (dip): they went negative once, and interest is then 43-60% OF THEIR ENTIRE LOSS (seed 0: op.net -40.1, net -88.5, interest 48.4; seed 7: -34.3 / -85.5 / 51.3). Their OPERATING net is only -24 to -40 cr/qtr - a modest, closeable gap. The debt is not the operating gap; it is 80 quarters of compounding on top of it, 1.02^80 = 4.9x. THE REAL DEFECT IS NOT THE RATE. 2%/qtr is a reasonable charge in play. The defect is that INTEREST HAS NO CONSEQUENCE ATTACHED: components.hpp says a balance 'may go negative (no insolvency consequence)', so a firm that would have died in year three keeps trading for twenty and is then handed to the player carrying the compounded wreckage. The pre-game is a HISTORY GENERATOR, and a history in which nothing ever fails is not a history - it is an accumulator. RECOMMENDED, and it is not 'switch interest off': KEEP the interest and ADD THE CONSEQUENCE IT IS SUPPOSED TO HAVE. A firm underwater for N consecutive quarters exits - dissolved, or absorbed through BL-628's dissolution machinery, which landed today and already knows how to move an actor's assets and cancel its promises. Three things fall out at once: the pre-game stops manufacturing zombies; the surviving field at seat time is SELF-SELECTED for viability, so BL-630's shortlist floor starts being met instead of falling through to its fallback on 23/24 seeds; and interest becomes the mechanism that identifies failure rather than a number that only accumulates. TWO WEAKER ALTERNATIVES, named so the choice is visible: suppress interest during the warm start only (cheap, but it hides the failure rather than resolving it, and a corp still enters play with an operating deficit); or raise opening capital (buys quarters, changes nothing structural - seed 1 earned 0.0 income across its whole trailing window and no starting balance survives that).

*Files: `src/world/components.hpp`, `src/world/economy_system.cpp`, `src/world/corp_command.cpp`, `docs/economy/FINANCE.md`, `docs/generation/CORPORATION_GENERATION.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-629 — Wage competition written as the SETTLED labour-clearing model, superseding proportional contention
*decision taken on your behalf · raised 2026-08-25 · from Ben, 2026-08-25 form: 'wage competition is preferred'.*

'Preferred' was written into POPULATION.md section Contention as the settled design (docs are state-independent; the proportional scalar remains what the code does until BL-614 lands). If 'preferred' meant 'leaning, not settled', overturn by restoring the proportional paragraph and marking BL-614 (wage competition) as a direction to evaluate instead.

> **RESOLVED.** Ben, 2026-08-25 verdict form: SETTLED - the doc stands as written.

*Files: `docs/economy/POPULATION.md`*

### NR-630 — The centre-density recalibration is ~40x, and its downstream tunables are not re-derived
*observation · raised 2026-08-25 · from Measured while filing BL-610/BL-611: centres today 1 per ~410 land tiles, land provinces 1 per ~10.*

'A centre in every land province' implies roughly 40x today's centre count, mostly small strata. Self-adjusting downstream: Pass 6 background-firm calibration (a 0.90 production/demand target, not a count). NOT self-adjusting and needing re-derivation when BL-610 builds: population_demand's demand_scale, workforce supply per centre (labour units 1/3/10/30/100), the market carve's population anchoring, logistics node discounts (most tiles now near a centre), and road generation's edge count (~40x more MST nodes). Each is a measured retune, not a redesign.

> **RESOLVED.** Ben, 2026-08-25: keep 6.0 tiles/centre, retunes next sprint - now BL-622 (density retunes).

*Files: `src/world/population_generation.cpp`, `scripts/economy.lua`*

### NR-631 — BL-615 landed with five first-cut calls: radius 6, smithy exemption, era band any, no save-version bump, inclusive bound
*decision taken on your behalf · raised 2026-08-25 · from Agent P's report, BL-615 (stratum placement gates), commit 9ea61218 on its worktree branch.*

Calls taken so the item could land, each overturnable: (1) centre_proximity_radius = 6 for the three industrial steel recipes (a quarter of the 24-unit reach budget; wrapped grid distance, inclusive at the bound); the ancient Smithy (steel_from_blooms) is deliberately ungated - a smithy is not a mill. (2) min_centre_scale > 0 implies requires_centre; proximity counts a centre of ANY stratum. (3) schooling/university take era band 'any' with timber/stone baskets so both bands can build them. (4) max_building widened with NO save-version bump - no serialized array is sized by building_type, reasoning recorded at the constant. (5) Rivals may propose a gated placement and be refused at apply time - deterministic, slightly wasteful.

> **RESOLVED.** Ben, 2026-08-25 verdict form: confirmed.

*Files: `scripts/recipes.lua`, `scripts/economy.lua`, `src/world/placement_rules.cpp`*

### NR-633 — BL-613/BL-614 landed with six first-cut calls: seeding constants, jurisdiction rule, product composition, non-negative bids, WF.R3 re-spec, education rate
*decision taken on your behalf · raised 2026-08-25 · from Agent E's report, commits 20b86c67 (BL-613) and dd283070 (BL-614) on its worktree branch.*

Calls taken so the items could land, each overturnable: (1) qualification seeding = first_furnace tercile base (early 0.35 / mid 0.22 / late 0.12 / never 0.05) + 0.25 x industrialised-region share, clamped [0,1] - never-industrialised nations floor at 0.05, not 0. (2) A building outside every nation is UNGATED by the qualified pool (the levy-pass precedent); a nation with no centre labour on a body has an empty pool there and its deep methods idle. (3) Qualified and ordinary grants compose by PRODUCT; qualified clears first but does not deplete the ordinary pool. (4) wage_bid is non-negative - bids raise, never undercut, in this cut; serialisation refuses negatives. (5) econ_harness WF.R3 asserted the superseded proportional model and was RE-SPECIFIED to the ruled wage contract - not weakened: output conservation and the aggregate still asserted, a differential bid row added. (6) Education raising rate 0.0005/building/tick as a local constexpr, inert until the is_education_building seam opens; move to economy.lua with the roster wiring. First-cut qualified_workforce ladder: spacecraft 0.50, heavy-route 0.40, electronics 0.35, medical 0.30, machinery/alloys/ordnance 0.25, steel_bessemer 0.15; ancient roster untouched.

> **RESOLVED.** Ben, 2026-08-25 verdict form: confirmed.

*Files: `src/world/settlement.cpp`, `src/world/economy_system.cpp`, `scripts/recipes.lua`*

### NR-634 — spectator_determinism has a red golden row ON MAIN at dc7d0554 - pre-existing, same class as ai_skill_harness (NR-616)
*observation · raised 2026-08-25 · from Agent E, measured with baseline isolation: harness built from dc7d0554 in a scratch tree shows identical hashes and the same golden-row failure before and after its commits.*

played=0AF36395D78F3DAF, spectated=C890E1AF7A9A27E1 - the two match each other (the harness's two real properties hold) but differ from the stored golden 71273F6FEDE03965. World drift from some prior landing moved the constant and nobody re-blessed with dated provenance, as the standing rule expects. Not blessed by the agent (correctly). Wants a deliberate re-bless with a provenance line, or a hunt for which landing moved it.

> **RESOLVED.** Resolved 2026-08-25 by the wave-1/wave-2 re-blesses: the golden now carries a provenance log recording BOTH the pre-existing unprovenance'd drift (0AF36395D78F3DAF at dc7d0554) and the sprint's two legitimate world moves; current constant DEC269E7941134D4, confirmed by R2's two-built-worlds row.

*Files: `tools/verify/spectator_determinism.cpp`*

### NR-635 — BL-610/BL-612 carve constants are first-cut: 10% urban share, rank-size banding, footprints 1/1/2/4/7; density landed at 6.0 tiles/centre vs the ~10 target
*decision taken on your behalf · raised 2026-08-25 · from Agent G's report, commits 594d4e18 / 88174045.*

The carve model is: count = each living region's urban headcount (10% share) / 10k heads, floored at 1; scales = integer rank-size share-out banded to the nearest k_population_for_scale rung in log space; footprints 1/1/2/4/7 tiles by scale on most-livable neighbours. Measured seeds 0-2: 8.76/8.04/6.18 land tiles per centre pre-anchor, 6.04 after anchor villages; histogram s1=4928 s2=206 s3=45 s4=11 s5=1. Slightly denser than the ~10 target - k_demography_heads_per_centre is the one data knob if Ben wants it back. P5a3 over-preference share also moved 4.63% -> ~7.2% (report-only row; the 'rare' judgement is Ben's).

> **RESOLVED.** Ben, 2026-08-25: density stands at 6.0; the knob stays; retunes are BL-622 (density retunes).

*Files: `src/world/population_generation.cpp`*

### NR-636 — BL-611 grew twice, forced by its own assertions: the nation lock implements purged BL-563's core, and anchor foundings carry a flag + the v12 bump
*decision taken on your behalf · raised 2026-08-25 · from Agent G's report, commit cab05f8f - flagged by the agent as its two novelty points.*

(1) The whole land fill is nation-locked (growth, leftovers, singleton absorption), making land provinces single-nation by construction - this is the core of purged BL-563 (province respects nation), designed but never built; the brief's anchor==tile-owner assertion is unreachable without it. (2) Centre-less pockets get a scale-1 anchor FOUNDING post-partition (relaxed habitability gate on pure-ice, counted), flagged province_anchor and excluded from partition seeding so the partition stays a pure function of the pre-anchor world - hence the second save bump. Also: hinterland seeding retained on UNSETTLED bodies' land (Selene/Cinder/Pallas have no centres); P9b retired as superseded (absorption legitimately merges centres at this density); P5d redefined to the nation lock.

> **RESOLVED.** Ben, 2026-08-25 verdict form: confirmed.

*Files: `src/world/province.cpp`, `src/world/population_generation.cpp`*

### NR-637 — At the new density the Planetary canvas reads settlement-saturated at far zoom - does the marker vocabulary need a tier gate?
*question · raised 2026-08-25 · from lens_strip_population.png captured on the integrated wave-1 world (build/screenshots/).*

With ~1,450 centres the always-on civic chrome draws a skyline glyph for nearly every land tile cluster - the lived-in goal is unmistakably met, but at far zoom the surface reads as a uniform settlement grid rather than a hierarchy of cities over towns over villages. Candidate directions, Ben's call: (a) zoom-gate village markers (scale 1 renders only past a zoom threshold, the conurbation anchor carries the far read); (b) raise the conurbation clustering distance so villages fold into their town's glyph; (c) leave it - density IS the message. POPULATION.md § Centre rendering owns the vocabulary.

> **RESOLVED.** Ben, 2026-08-25 verdict form: leave it - density is the message. No marker change.

*Files: `src/ui/body_surface_canvas.cpp`, `docs/economy/POPULATION.md`*

### NR-638 — BL-616/BL-617 first-cut calls: raze precondition unit-on-body, stance gate latent (all-neutral), selectivity 1.5 makes brain drain bite, raze keeps urban ground
*decision taken on your behalf · raised 2026-08-25 · from Agent W2E's report, commits be4c9588 (BL-616) and bc1ea38b (BL-617) on its worktree branch.*

Four calls, each overturnable: (1) raze_centre's occupation precondition = the acting corp owns a unit positioned on the centre's BODY (a stricter on-centre-tile rule is a one-line change); the verb is deliberately NOT in the corp-AI candidate list - no standing-rules grant covers a rival razing. (2) The migration stance gate reads the existing stance tables by nation entity id, but nothing DECLARES nation stance yet, so live inter-nation flows all clear at the neutral throttle (250 permille) - the gate is real but latent until a nation-stance verb exists. (3) qualified_selectivity = 1.5: migrants skew qualified, so emigration lowers the origin's FRACTION, not just its headcount - at exactly 1.0 the fraction would be invariant, contradicting the doc's 'debits the fraction' reading. (4) Razing leaves the urban land-use stamp (historied ground; extraction stays blocked) - now written into POPULATION.md. Growth/decline constants: step 10 ticks, shed/gain pop/25, promotion window 50 ticks; migration rate 10 permille, wage_weight 0.02 - all data or constexpr, first-cut-then-tune.

> **RESOLVED.** Ben, 2026-08-25 verdict form: confirmed (all four calls).

*Files: `src/world/economy_system.cpp`, `src/world/corp_command.hpp`, `scripts/economy.lua`*

### NR-639 — tools/mcp/server.js VERBS array is stale by 10 verbs (stops at hold_convoy) - index-is-value, so the whole tail is missing
*observation · raised 2026-08-25 · from Agent W2E, observed in passing while appending raze_centre (verb 27).*

The MCP server's VERBS name array has 17 entries while corp_verb now counts 28 - every verb since BL-448 is unaddressable over the wire, and because index-is-value the fix is appending the full ordered tail, not one name. Compounding: tools/session/verb_coverage.js lists raze_centre unmapped alongside the pre-existing withdraw_from_battle / accept_offer / abandon_contract. An AI-facing seam drifted silently; worth a small owned fix plus a lint that diffs the array length against corp_verb_count.

> **RESOLVED.** Fixed 2026-08-25 (Ben: 'update the MCP server's verb table'): full 11-verb tail appended in enum order (28 total); schema gains province + extended order/counterparty descriptions; verb_coverage.js gains a FAILING drift guard (length AND order vs the seam) plus subsystem rows for the four unmapped verbs. Honest gap recorded in place: agent_protocol.cpp parses no units key, so a wire accept_offer cannot commit units yet - that is seam work, not schema work.

*Files: `tools/mcp/server.js`, `tools/session/verb_coverage.js`*

### NR-640 — Anchor villages are OFF the road lattice - connecting them post-partition breaks partition-recompute reproducibility; ordering question for Ben
*question · raised 2026-08-25 · from Agent W2G's report (BL-620): road_generation_harness R3 was already red at HEAD - BL-611's ensure_province_anchor_centres founds ~1,086 villages AFTER generate_roads.*

Roads -> partition -> anchors is a hard ordering: roads bind provinces, so the partition must see final roads, and anchors exist only after the partition. W2G built and then REVERTED a connect-the-anchors pass because any post-partition road_level write changes the partition's input and broke P6 (partition-recompute reproducibility, a save/load contract) plus two more rows. Current contract, asserted by new R3/R3b: anchor foundings are the ONLY off-lattice centres. The open call: (a) leave anchors off-lattice as administrative foundings (current state - a pure-ice village with no road is arguably honest); (b) a partition fixpoint (roads -> partition -> anchors -> anchor spurs -> REPARTITION, iterate) - deterministic but a real algorithm change; (c) fold anchor founding INTO the pre-road centre pass somehow (loses the 'only provinces the fill left empty' definition).

> **RESOLVED.** Ben, 2026-08-25: option (c) with the overturn - the partition runs before roads (ruling 2's binding half superseded), every province is given its settlement before the lattice is laid, and the settlement is the capture anchor. Work: BL-623 (provinces before roads) + BL-624 (razed settlement tier, closing the razed-anchor hole). v0.1.19 recuts after.

*Files: `src/world/hard_coded_world.cpp`, `src/world/road_generation.cpp`, `src/world/province.cpp`*

### NR-641 — The default campaign world is now an ALL-TRACK lattice - every nation sits at the qualification floor at epoch 0
*question · raised 2026-08-25 · from Agent W2G's report (BL-618): qualification spreads 0.05-0.60 only at epoch >= 1700; epoch_year 0 (the campaign default, an antiquity start) leaves every nation at the never-industrialised floor 0.05.*

The first-cut tier mapping (Road needs qualification >= 0.10, Highway >= 0.30, redundancy loops rationed by qualification/0.40) is era-coherent - highways are industrial - but its visible consequence is that the DEFAULT generated world promotes nothing past Track: a global logistics-cost change (Track x0.67 vs Road x0.50 vs Highway x0.40) and a different-looking map. Options: (a) accept - an antiquity world with only tracks is honest, and roads arrive as qualification rises in play (BL-616's education loop now matters); (b) rebase the thresholds on the era-relative qualification distribution rather than absolute values, so antiquity keeps its Roman-road-analogue backbone; (c) keep absolute thresholds but lower them. Differential harness rows Q1-Q4 prove the lever works either way.

> **RESOLVED.** Ben, 2026-08-25: era-relative thresholds - implemented same session as BL-621 (era-relative road gates); LOGISTICS.md updated, harness re-specified.

*Files: `src/world/road_generation.cpp`, `docs/economy/LOGISTICS.md`*

### NR-644 — Stale tech-tree goldens (1720x1080, blessed 2026-08-15) fail every verify run on size alone - delete or deliberately re-admit
*question · raised 2026-08-25 · from Tech tree restyle session (wide8 verdicts, GLOBAL_STYLE_SHEET.md) - first verify run after the restyle.*

scripts/verify/golden/tech_tree_{tabs,era1,antiquity}.png are 1720x1080 against the harness's fixed 1280x720, so every tech_tree_panel.lua run reports 3 golden FAILs regardless of content. They were blessed the same day the golden demotion ruling landed (NR-237: curated world-independent set only, currently the icon_silhouettes pair) and these frames carry shell/world content, so they sit outside the policy anyway. Recommend deleting the three; re-admitting the surface would be a deliberate copy-in after the restyle is eyeballed.

> **RESOLVED.** Ben, 2026-08-25: delete the three stale goldens - done same session; re-admitting the surface stays a deliberate copy-in per the NR-237 curated-set policy.

*Files: `scripts/verify/golden/tech_tree_tabs.png`, `scripts/verify/golden/tech_tree_era1.png`, `scripts/verify/golden/tech_tree_antiquity.png`*

### NR-647 — Does the OPERATIONAL fog go too, or only the financial banding?
*question · raised 2026-08-26 · from Sprint 20 design session, 2026-08-26: the profitability ledger, the buyout, the spawn shortlist.*

'We don't need company information to be invisible' was said while retiring BL-262 (the banded standing read). BL-068's competitor-visibility rule in DISCOVERY.md is a separate and wider thing: a rival hover card shows type and owner only, no production rates, no stockpiles, and the doc's stated principle is that intelligence is earned by reasoning over public signals. Narrow reading: only the financial banding goes, operational internals stay private. Wide reading: the whole competitor fog goes. The narrow reading is the one this session wrote to, because the disclosure gate would be pointless under the wide one. DISCOVERY.md has NOT been edited pending the answer.

> **RESOLVED.** Ben, 2026-08-26: 'Operational fog should persist.' The NARROW reading is confirmed - only the financial banding goes. DISCOVERY.md now carries both halves: filed financials are public wherever a firm files, and a new subsection states that production rates, stockpiles, recipes and workforce dials are untouched. The line written into the doc: an open book tells you what a firm earned, never how it operates.

*Files: `docs/ui/DISCOVERY.md`*

### NR-649 — The spawn shortlist does NOT solve the shallow-opening problem the corp-choice screen was built for
*observation · raised 2026-08-26 · from Sprint 20 design session, 2026-08-26: the profitability ledger, the buyout, the spawn shortlist.*

BL-435's selection screen existed on a MEASURED finding: over 24 seeds the generator handed the player a pure-extraction corp on 13 of them, so the chain-depth ladder had no rung to stand on, and player_seed_sweep records it. Solvency was explicitly NOT the discriminator - all 24 seeds ended positive. The replacement (BL-630, spawn shortlist) gates on VIABILITY, and a shallow pure-extraction corp is usually perfectly viable, so it will clear the floor every time. Retiring the screen therefore restores the exact distribution the screen was built to fix, unless the shortlist takes a second, DEPTH criterion alongside the viability one. Not filed as a blocker because BL-630 is buildable either way; the criterion is Ben's call.

> **RESOLVED.** Ben, 2026-08-26: 'The shortlist can be mostly random for now, targeted towards population centres and processing, rather than extraction.' So the depth concern IS addressed, but as a WEIGHTED DRAW rather than a second gate - a shallow corp stays drawable, just less often. Written into CORPORATION_GENERATION.md and into BL-630; player_seed_sweep reports the resulting distribution rather than asserting a target.

*Files: `docs/generation/CORPORATION_GENERATION.md`, `tools/verify/player_seed_sweep.cpp`*

### NR-653 — Sprint 20 is not open, and its inherited debts are unchanged by this design
*observation · raised 2026-08-26 · from Sprint 20 design session, 2026-08-26: the profitability ledger, the buyout, the spawn shortlist.*

Ben ruled this ledger a SIBLING of the spawn-viability pass rather than its carrier, so Sprint 20's theme is unchanged and the sprint is still uncut - the design-before-cut ordering NEXT_SESSION.md names. Carried forward untouched by this session and still owed: the two live clicks (dispatch form, Throughput lens container access - three sprints running), the v0.1.18 tag Ben left uncut, and the dispatch form's remaining UX fixes awaiting an A/backlog-or-B/build-now call. Note also that BL-630 (spawn shortlist) and the viability pass BOTH move the warm start's ordering, so whichever lands second inherits the other's golden re-bless - they should share one wave, not two.

> **RESOLVED.** Ben, 2026-08-26: Sprint 20 opened same day with the ledger + viability theme, so the sibling framing became one sprint in two phases rather than two sprints. The inherited debts now have owners: the live clicks are BL-636 (live-click debt), the shared golden re-bless is written into the sprint notes, and the v0.1.18 tag stays Ben’s call.

*Files: `docs/development/NEXT_SESSION.md`, `docs/development/sprints.json`*

### NR-655 — A filed return CANNOT see subsidies or contract payouts - and the buyout would undervalue every firm that earns them
*question · raised 2026-08-26 · from Sprint 20 wave 1, BL-626 (quarterly return): the agent found it and the main session confirmed it at app.cpp:1271/1281.*

MEASURED, not suspected. apply_budget runs at app.cpp:1271 and run_nation_step at :1281 - ten lines later. So a return files BEFORE national transfers and mercenary-contract payouts touch the balance, and corp_budget::subsidies is always 0 in a filed record. Two consequences. (1) The retain property R2 holds exactly only where nothing outside the money loop moves a balance; on a real campaign construction, hire, survey, convoy and nation payouts all do. (2) THE ONE THAT MATTERS: BL-628 prices a buyout off trailing_net, so a firm earning through mercenary contracts - which CONTRACTS.md calls THE income loop - reads as unprofitable on its own filed returns and would be systematically undervalued. THE FORK: (A) add an eighth field, 'other' = whole-tick balance delta minus money-loop net, filed after the tick's last mover, so the seven flows still explain net AND net+other telescopes exactly; the buyout prices off the sum. (B) keep it as built and document trailing_net as money-loop profit only, accepting the undervaluation. (C) file after everything and let net be the whole delta, losing the property that the seven flows explain it. RECOMMEND (A): one extra field, nothing hidden, everything telescopes, and it is the legible answer rather than the clever one. It needs a new item that BL-628 requires, and it changes what FINANCE.md says about where the return is filed - the doc's 'at the end of apply_budget' is my wording from earlier today and it is the part that is wrong.

> **RESOLVED.** Ben, 2026-08-26: option A. A return gains an eighth field, `other`, filed after the tick's last mover. Written into FINANCE.md section The eighth field; BL-653 owns the work.

*Files: `docs/economy/FINANCE.md`, `src/world/budget_system.cpp`, `src/core/app.cpp`*

### NR-658 — build_harness.js is unusable from an agent Bash session - second independent report
*observation · raised 2026-08-26 · from Sprint 20 wave 1, BL-626.*

The committed harness build route dies with "'cl' is not recognized" when invoked from the Bash tool: its cmd /c spawn is mangled by Git Bash. The agent worked around it by generating an equivalent .bat over the same glob-derived 53-TU source set and running that. This is the same failure the io-headless-build-invocation note already records, now hit again by a fresh agent with no memory of it - which is the signal worth acting on. Not a code defect in the harnesses themselves, but every agent that needs to build one pays this tax and some will conclude the harness is broken rather than the invocation. Worth a small fix to build_harness.js or a documented Bash-safe entry point.

> **RESOLVED.** FIXED, 2026-08-26, in the wave-1 integration: tools/verify/build_harness.bat is a committed Bash-safe entry point. It establishes the MSVC environment ONCE in its own shell before node runs, so the inner spawn inherits a PATH that already has cl on it instead of building one across a mangled quoting boundary. Arguments, output location and exit code are build_harness.js’s own - the wrapper adds nothing but the environment. Verified by building five harnesses through it. Three sessions hit this and each rediscovered the same workaround, which is what made it worth a committed fix rather than a fourth note.

*Files: `tools/verify/`, `docs/development/DEVELOPMENT_PRACTICES.md`*

### NR-664 — A new verify script (corp_disclosure.lua) is written but unregistered - registration needs Ben
*question · raised 2026-08-26 · from Sprint 20 wave 2, BL-633.*

The agent authored scripts/verify/corp_disclosure.lua - an acceptance script that injects a real press through ImGui's own event queue and hit-test via verify.click (BL-521), so a control rendering past the panel's edge FAILS there rather than passing on a screenshot. It correctly did not name it in verifier-visual's SKILL.md, because 'tool creation is skill creation' and registering a check needs Ben's permission. The script works (it selected Faros-YelenKalen International by clicking a row). Ben's call: register it as a named check, or leave it as an ad-hoc script. Recommend registering - an injected press is materially stronger than a capture, and it is the closest thing we have to the live click that keeps slipping.

> **RESOLVED.** Ben, 2026-08-26: REGISTER. corp_disclosure.lua is now a named check in verifier-visual's SKILL.md, with its strengths and its one honest limit recorded - it injects a real press, and it still is not a live click.

*Files: `scripts/verify/corp_disclosure.lua`, `.claude/skills/verifier-visual/SKILL.md`*

### NR-666 — The INDUSTRIAL epoch now yields FEWER public firms than antiquity - measured, and it reads backwards
*question · raised 2026-08-26 · from Sprint 20 wave 2, BL-638's R2 sweep, 8 seeds at each epoch.*

0 CE: public 20 of 64 corps (31.2%), floor met in 8 of 8 worlds. 1960: public 14 of 64 (21.9%), floor met in 5 of 8, three worlds standing floor-unmet. So the ANTIQUITY default now produces MORE publicly-held firms than the industrial arc, which is the opposite of what the history it models would suggest. The mechanism is legible and may be entirely fine: a 1960 world carries more regions (1550 vs 1298), the charter diffuses from a single cradle, so a bigger world dilutes its reach. But it means three modern worlds in eight have nothing to file and nothing to buy - the exact condition BL-638 was written to remove, now relocated to the other end of the timeline. TWO READINGS, and it is Ben's call which: (a) correct as-is - an unmet floor honestly stands, and a world where corporate personhood never spread is a legitimate world; (b) the reach radius should scale with the world's size or era rather than being fixed, so diffusion keeps pace with the ground it has to cover. Related dial: the agent measured that the CULTURAL carrier does most of the work (328 lived regions against 95 copied) because conquest_cost_q keeps the geometric radius near its floor - so if the map's shape should matter more than its peoples, that scaling is where to reach.

> **RESOLVED.** Ben, 2026-08-26: CORRECT AS-IS - an unmet floor honestly stands. A later epoch may yield fewer public firms than an earlier one; the dilution is legible and the result is a legitimate world. Written into CORPORATION_GENERATION.md with an explicit instruction NOT to add a size or era term to the radius to make the curve match intuition.

*Files: `src/world/settlement.cpp`, `docs/generation/CORPORATION_GENERATION.md`*

### NR-668 — NR-655's exposure is now MEASURED: the buyout undervalues a contract-earning firm by 640 credits
*question · raised 2026-08-26 · from Sprint 20 wave 2, BL-628: the harness was briefed to state the exposure rather than hide it, and it quantified it instead.*

NR-655 raised this as a risk; BL-628's harness turned it into a number. Two fixtures, identical except that one earns through a mercenary contract: both file a trailing_net of 10.00, because the payout arrives in run_nation_step AFTER apply_budget has filed. Loop-only firm prices at 460.00; the contract earner at 1100.00 (its balance carries the cash). Had the return SEEN the payout, trailing_net would be 90.00 and the price 1740.00. MEASURED UNDERVALUATION: 640.00 credits, exactly k(8.0) x the 80.00/quarter the record cannot see. The harness asserts that the two firms' filed trailing_nets are IDENTICAL - i.e. it pins the blindness as a known property rather than letting a future fixture hide it. This makes NR-655's fork concrete: option A (an eighth `other` field filed after the tick's last mover) closes exactly this 640. Ben's call, and BL-628 is built and correct WITHOUT it - the price is what the doc specifies; the doc's input is what is short.

> **RESOLVED.** Ben, 2026-08-26: closed by the same ruling as NR-655 - the eighth field closes exactly this 640 credits. BL-653. Its harness row pinning the blindness is to be INVERTED to pin the closure, not deleted.

*Files: `docs/economy/FINANCE.md`, `src/world/corp_command.cpp`, `src/world/budget_system.cpp`*

### NR-670 — Should a rival ever be able to buy the PLAYER out?
*question · raised 2026-08-26 · from Sprint 20 wave 2, BL-628: gated at the seam and flagged rather than assumed.*

FINANCE.md's ownership-class rules are silent on the player's own corporation. Read literally, a public player corp could be bought and ERASED, leaving world::player_entity dangling. BL-628 refuses it - gated at the command seam rather than in corp_ai.cpp, because a scorer-side guard would not bind a wire caller, which is the right place for it either way. But the underlying question is Ben's and it is not small: every widening in the standing rules concerns what may be done TO a corp a human owns, and being bought out is the furthest possible version of that. If the answer is ever yes, player_entity needs a rule of its own (does the player follow the assets? get a game-over? re-seat?). Recorded in FINANCE.md as a live question rather than left as a default fallen into. Note the spectator precedent: under corp_ai_params::spectating the prohibition has no subject, so a buy-the-player gate arguably should not apply there either.

> **RESOLVED.** Ben, 2026-08-26: NEVER. The player's corporation is not buyable. Written into FINANCE.md as settled, with the reason: every other widening makes the world act ON the player, and erasing the seat is the one move that ends the conversation instead of continuing it. player_entity needs no rule of its own, since it cannot be left dangling by a verb that cannot fire.

*Files: `docs/economy/FINANCE.md`, `.claude/rules/io-standing-rules.md`, `src/world/corp_command.cpp`*

### NR-675 — refined_copper is UNTAGGED in recipes.lua - a copper smelter runs at 0 CE while every sibling is industrial
*question · raised 2026-08-26 · from Sprint 21 wave 0, BL-649's census: measured, not inferred - 8,940 units produced at 0 CE on seed 0.*

scripts/recipes.lua's refined_copper recipe (~line 154) carries no `era =` field, so it defaults to `any` and is allowed in the ancient band. Every sibling refining recipe is tagged `industrial`. The census measures the consequence rather than guessing at it: 8,940 units of refined_copper produced at 0 CE, chain depth 1, and background_demand pays for it - which makes it one of the very few ancient chains with a live buyer, for what looks like an oversight. TWO READINGS and it is Ben's call: (a) it IS an oversight, tag it `industrial`, and the ancient band loses a chain it should not have had; (b) it is correct - copper smelting is genuinely ancient technology, unlike the rest of that group, and the tag was omitted on purpose. Reading (b) is historically defensible, which is exactly why it should not be decided by whoever notices it next. Nobody has touched the line.

> **RESOLVED.** Ben, 2026-08-26: OVERSIGHT - tagged `industrial`. MEASURED THROUGH THE NEW CENSUS, which is its first real use: total ancient market demand 2850.2 -> 2617.3 (-8.2%), and copper_ore JOINS THE DEAD EXTRACTABLE LIST (17,686 units dug with no ancient buyer) because the smelter was its only in-band consumer. That consequence is correct and is now visible rather than latent. Also observed in the diff: the construction channel read 35.0 before and 0.0 after purely because nothing happened to be mid-build at the sample tick - transient by nature, which is exactly BL-642's premise.

*Files: `scripts/recipes.lua`, `docs/economy/PRODUCTION.md`*

### NR-676 — The census invented vocabulary the tuning passes will cite - worth ratifying once
*novel-work · raised 2026-08-26 · from Sprint 21 wave 0, BL-649: the agent raised a mild novelty flag and it is the right call.*

MARKETS.md § Measuring it owns the task, so the work was not unowned - but the census had to coin terms no doc defines, and every Sprint 21 tuning pass will now quote them: the STRUCTURAL SINK vs OBSERVED DEMAND distinction (a good can have a sink authored and still have zero demand this tick), the REC/DEP producibility split, and the `px` priced-on-any-market axis. It also split economy_report::wants into construction vs processing, recovered from world state rather than stored, with the residual asserted non-negative. None of it is controversial and all of it is useful; it just should be ratified into MARKETS.md § Demand channels once rather than accreting as harness-local jargon three passes deep. Cheap to do, and the alternative is a report nobody outside this session can read.

> **RESOLVED.** Ben, 2026-08-26: RATIFY. The census's vocabulary is written into MARKETS.md section Measuring it as a table - structural sink vs observed demand, REC/DEP, px - with the distinction that does the work called out: a structural sink with ZERO observed demand is the signature of a dead chain, and invisible to any check looking at only one of them.

*Files: `tools/verify/demand_census.cpp`, `docs/economy/MARKETS.md`*

### NR-679 — Banding the baskets HALVED the generated world - 584 buildings / 104 corps -> 261 / 64
*observation · raised 2026-08-26 · from Sprint 20 wave 4, BL-640: the agent found it, named it as its own, and flagged it before merge rather than after.*

Pass 6 generates background firms until a body's real production reaches ~90% of its real demand. That stop condition reads the same baskets BL-640 just banded - through `body_demand` in corporation_generation.cpp, which was NOT in the brief's file list and which the agent repointed for correctness. Before the band, Pass 6 was chasing demand for silicon, machinery, alloys and electronics that the ancient band STRUCTURALLY CANNOT PRODUCE, so it kept adding firms against a gap that could never close. The behaviour is now correct and the world is half the size: 584 buildings / 104 corps -> 261 / 64. THIS IS A LARGE GENERATED-WORLD CHANGE AND EVERY DOWNSTREAM MEASUREMENT MOVES WITH IT - market saturation, prices, spawn viability, the haulage baseline (already flagged as drifted in NR-674), and the spectator golden. Worth Ben's eye on the FEEL as much as the numbers: a world with 64 corps rather than 104 is a visibly different economy, and it is the honest one. The old count was inflated by chasing an unclosable target.

> **RESOLVED.** Ben, 2026-08-26: KEEP THE FIX, retune density upward. BL-655 owns it, and carries the distinction that matters - raising Pass 6's target fraction reaches the old count by making the world OVERPRODUCE (gluts, floored prices), while raising the demand baskets reaches it by making the world CONSUME more (prices hold). Try the demand side first; record an oversupply as deliberate if it is taken.

*Files: `src/world/corporation_generation.cpp`, `docs/generation/CORPORATION_GENERATION.md`, `docs/economy/MARKETS.md`*

### NR-681 — NOVEL: a demand channel that consumes without pricing cannot bootstrap its own supply
*novel-work · raised 2026-08-26 · from Sprint 20 wave 4, BL-641: the agent raised the flag, and the property is now written into MARKETS.md as the register's third.*

The implementation was owned end to end by FINANCE.md and copied cleanly from run_unit_upkeep. What is novel is what turning it on MEASURED: operating firms 227 -> 19, because a pool draw for a good the band produces at 0.0 starves the drawer and never signals the market that would supply it. That is a CROSS-CHANNEL property no doc stated, and four unbuilt channels were designed the same way this morning. It is now property 3 of MARKETS.md section Demand channels, and BL-654 owns the question it raises. Worth Ben's eye on the meta-point as much as the mechanism: the item shipped INERT rather than shipping a collapse or quietly picking a smaller rate that would have hidden the same defect behind a slower decay - which is the difference between a measurement and a number that looks acceptable.

> **RESOLVED.** Ben, 2026-08-26: answered by the BL-654 ruling - a short pool BUYS, up to a reservation ceiling, and one rule covers every goods draw. The novel property stands as MARKETS.md's property 3 and now has its settled answer beneath it.

*Files: `docs/economy/MARKETS.md`, `src/world/economy_system.cpp`*

### NR-686 — A Lua-only change does NOT reach the play build - the script copy is attached to the compile target
*observation · raised 2026-08-26 · from Sprint 20 wave 5: caught while handing Ben the Release build to judge BL-655 density.*

BL-655 changed scripts/economy.lua and no C++ at all. Both trees were rebuilt and both printed BUILD_OK - and BOTH SHIPPED THE OLD SCRIPTS. The CMake copy-scripts step is a side effect of building the ProjectIo target, so when nothing recompiles it does not fire, and build/scripts and build_rel/scripts keep whatever they had. Caught only because the handover was verified by grepping the OUTPUT tree for the new weights rather than trusting BUILD_OK. WHY IT MATTERS MORE THAN IT LOOKS: this session is a tuning sprint. Most of Sprint 21 is Lua - demand baskets, upkeep rates, price-band constants - so the class of change most likely to be evaluated is exactly the class that silently does not ship. Ben would have judged the old density and we would both have believed the number. A harness reading scripts/ directly is unaffected, which is why every measurement in this session was right while the playable build was wrong - the two paths disagree and nothing says so. FIX CANDIDATES: make the copy its own always-run target rather than a side effect of compiling; or have the app resolve scripts/ from the repo root in a dev build so there is one copy and it cannot go stale. The second removes the failure mode rather than making it fire more often.

> **RESOLVED.** FIXED 2026-08-26, the simple way on Ben call. copy_scripts is now an ALWAYS-RUN custom target that ProjectIo DEPENDS ON, not a POST_BUILD side effect firing only on relink. PROVEN rather than assumed: build_rel/scripts/economy.lua was deliberately overwritten with a stale marker and the tree rebuilt with ZERO C++ changes - the marker was gone and the real basket restored. The thorough fix (a dev build resolving scripts/ from the repo root, so there is only ever one copy) goes to Sprint 21 visibility pass, Ben call.

*Files: `CMakeLists.txt`, `build_app.bat`, `src/core/app.cpp`*

