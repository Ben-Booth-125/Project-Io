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

*102 entries — 102 open, 0 resolved.*

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

### NR-688 — The generation-config omission is now STRUCTURALLY visible - three harnesses had it
*decision taken on your behalf · raised 2026-08-26 · from BL-634's R0, plus the sibling defect it found in player_seed_sweep.*

Three harnesses omitted world_gen_config in one day: spawn_solvency (which invalidated a sprint of viability numbers), material_floor's counterfactual, and player_seed_sweep. Fixing each by hand would have left the fourth to find. Taken on Ben's behalf: make the omission SAYABLE rather than silent. `world_gen_config::is_fallback` defaults true and load_from_lua clears it, so any harness can print which of the two worlds it measured; and `parsed_gen_config(lua)` in harness_params.hpp is the one-line correct path with the whole hazard documented above it. A Lua-free logic harness legitimately keeps the fallback - prices are not its subject - and can now SAY so instead of leaving a reader to guess. What is NOT done: player_seed_sweep still omits it at three call sites, and nothing yet FAILS on the omission. Making it loud rather than merely sayable is the obvious next step and was not taken without Ben.

*Files: `src/world/world_gen_config.hpp`, `tools/verify/harness_params.hpp`, `tools/verify/player_seed_sweep.cpp`*

### NR-689 — FINANCE.md describes the eighth field as if it exists; BL-653 is unbuilt
*observation · raised 2026-08-26 · from BL-634, which read the doc and then the code.*

FINANCE.md section The eighth field says trailing_net is the mean of `net + other`. quarterly_return carries seven flows plus net and no `other`; corp_trailing_net averages `net` alone. This is the SANCTIONED pattern - a settled design is written the moment it is settled, and BL-653 owns the work - so the doc is not wrong so much as ahead. Recorded because BL-634 read it as current and had to check the code to find out, which is a real cost the state-independence rule pays: a reader cannot tell 'settled and built' from 'settled and owed' without querying the backlog. Worth considering whether a doc that describes an unbuilt mechanism should say which item owes it - CORPORATION_GENERATION.md's Pass 2b does exactly that and reads no worse for it.

*Files: `docs/economy/FINANCE.md`, `docs/development/DELIVERY.md`*

### NR-692 — Two senses of "visibility" - which sprint owns the verification family
*question · raised 2026-08-27 · from Sprint 21 opened 2026-08-27; the phrase was already attached to the demand sprint on 2026-08-26.*

On 2026-08-26 Ben steered that "Sprint 21 is a VISIBILITY pass" and three things were parked against it: NR-663 family (four checks whose green means less than it appears), BL-639 (panel columns), and NR-686 thorough fix (a dev build resolving scripts/ from the repo root). At that moment 21 was the DEMAND sprint. On 2026-08-27 a UI visibility pass opened as Sprint 22 (briefly numbered 21, corrected the same day). The two senses are not the same work: (a) UI visibility - the screen does not say what the model knows; (b) verification visibility - a check that passes without checking. THE CALL: does the verification family follow the NUMBER (into UI visibility, sprint 22) or the THEME it was steered onto (demand, sprint 21), or become its own item? Parked in sprint 22 planned rather than assigned, because inferring it would be a guess. Note the two senses did just meet in practice: NR-690 is a fifth member of the verification family, found by the UI scan.

*Files: `docs/development/sprints.json`*

### NR-695 — overflow_tile_v2 hung the suite - CPU burning, memory collapsed, nine scripts starved
*question · raised 2026-08-28 · from Sprint 22 (UI visibility), the 2026-08-28 capture run.*

text_overflow_floor.lua sweeps every fold-out ledger by sub-view. Its `tile` panel is the History ledger, and view 2 is the TILES view - a table with a row per tile on the body (composition, landform, hazard, habitability, deposits, buildings, local market state), captured with expect_no_clipping instrumenting every string drawn into the overflow ledger. The run reached overflow_tile_v2 about 60 minutes in and was still on it 80 minutes later: CPU climbing steadily (5315 -> 6457, ~70% of a core sustained), working set peaking near 1.6 GB then settling ~1.0 GB, zero new captures. Nine scripts sit behind it unrun (throughput_lens, both tile_build_ledger checks, tile_texture, time_controls, ui_shell_fixture, v009_batch, visibility, zoom_ladder), which is ten more elements with nothing to review. THE CALL: is the Tiles view legitimately this expensive to instrument, or is the overflow ledger quadratic in rows? Either way the suite should not be serially blocked by one capture - parking it (the parked/ mechanism, with a debt item) or capping the Tiles table under verify are both cheaper than the status quo. Note the irony: UI-087 Tiles view is CLIP-ONLY class, so this capture's own assertion is the only thing checking it. UPDATED 03:42, ~2h45m in, and the reading has CHANGED: the working set COLLAPSED from ~1000 MB to 17 MB while CPU kept climbing (6457 -> 8404). A process burning CPU while touching almost no memory is not rendering a large table - it is spinning. The earlier 'slow but computing' read was defensible at 45 minutes on a rising memory curve; it is not defensible now. Treat this as a HANG with a compute loop, not an expensive capture, and note that neither state is distinguishable from the outside without NR-694 being fixed first.

*Files: `scripts/verify/text_overflow_floor.lua`, `src/ui/tile_inspector.cpp`, `src/ui/text_fit.cpp`, `scripts/verify/parked/README.md`*

### NR-699 — A corporation's tile group is highlighted with a WASH, not the outline Ben asked for
*decision · raised 2026-08-28 · from Sprint 23 (selection), taking Ben's tile-group ruling into BL-665.*

Ben's words were "hovering one tile displays an OUTLINE around all company buildings for that corporation/company". BL-665 is written for a per-tile WASH instead, and the call is taken rather than asked because he already ruled the same question the other way three days earlier.

On 2026-08-24, for the market catchment: "the entire market gets highlighted on mouse over". The resolution recorded in body_surface_canvas.cpp is explicit about why it became a wash - outlining a region means walking its boundary every frame to find which edges face out, a wash is one test per tile, and "all of THIS is one thing" is an area claim rather than an edge one. A corporation's holdings are the same kind of region, so the same answer applies and the two highlights stay consistent with each other.

WHERE THIS COULD BE WRONG: a corporation's holdings are SCATTERED where a catchment is contiguous, and a wash over scattered tiles may read as "these tiles are lit" rather than "these tiles are one owner's". If the group needs to look different from a catchment, say so and BL-665 gains a boundary walk.

*Files: `src/ui/body_surface_canvas.cpp`, `docs/ui/LENSES.md`*

### NR-700 — Under the one-tier rule the Corporation lens's current destination is the player's own books
*observation · raised 2026-08-28 · from Sprint 23 (selection), tracing where a corporation click lands today.*

Reading the click handler to size BL-665, the existing corporation route came out wrong in a way worth naming separately from the item that fixes it: a corporation structure hit calls close_all_panels then opens show_balance_ledger, and the Balance ledger is the PLAYER'S OWN accounts. So clicking a rival's ground under the Corporation lens today shows you your own books - a destination that is not empty, not broken, and not about the thing that was clicked, which is the failure mode hardest to notice in a capture.

BL-666 owns the fix (an aimable corporation destination and a company one that exists). Filed here because it predates BL-665 and would have been a live defect even without the one-tier rule - worth knowing it was found by reading rather than by looking.

*Files: `src/ui/body_surface_canvas.cpp`, `src/ui/ui_state.hpp`*

### NR-702 — A press on inert ground now CLOSES the Company ledger, not just the Selection band
*decision · raised 2026-08-28 · from Sprint 23, BL-666; the review barrier's finding 7.*

Ben ruled that a press on ground an active lens has no answer for clears the Selection band to resting. THE CALL TAKEN, which he did not rule on: the same press also clears selected_company / selected_corporation_dossier and closes the Company ledger.

The reasoning: the Company ledger's whole subject is one named firm, so leaving it open on firm X after the player has cleared the band is the same failure NR-697 recorded - a surface asserting a subject the player did not just click - moved from the band into the column. The tile build ledger is the precedent and it goes further than this: app.cpp actively closes it the moment the selection stops being a tile.

WHERE THIS COULD BE WRONG: a ledger normally persists until the player closes it, and this makes one ledger close as a side effect of a click somewhere else on the map. If a firm's ledger should stay up until dismissed, the three lines come out of the lens_answered_nothing branch and the ledger needs a close affordance of its own - it currently has no rail slot and no close button, so close_all_panels is its only other exit.

*Files: `src/ui/body_surface_canvas.cpp`, `src/ui/company_ledger.cpp`*

### NR-703 — A save written under the Throughput or Company lens could not be loaded back
*observation · raised 2026-08-28 · from Sprint 23; found by the review barrier (finding 3) while checking the batch's serialisation seam.*

FIXED IN THIS BATCH, filed because of what it says about the class rather than the instance. save_game.cpp's `max_overlay` range bound was hand-kept at overlay_mode::supply_routes while two lenses had since been appended after it - throughput (Sprint 18) and company (2026-08-28). Saving with either active wrote a byte the loader's range check rejected, and a failed r_enum fails the WHOLE envelope, so the campaign simply would not reopen.

Silent, total, and reachable by nothing more than picking a lens before saving. It predates this batch; what the batch changed is that the Company lens is now a destination a player has a reason to sit in.

The fix derives the bound from overlay_mode::count rather than naming a lens. WORTH A LOOK: the same file holds several hand-kept `max_*` constants (max_atmos, max_hydro, max_geo, max_bias, max_canvas). Each is a genuine domain bound rather than 'the last value', so none is wrong today - but the failure mode is identical if any of those enums grows, and it fails the entire save rather than one field.

*Files: `src/core/save_game.cpp`*

### NR-704 — The two corporation ui_state flags are named the wrong way round
*observation · raised 2026-08-28 · from Sprint 23, BL-666; the review barrier's findings 1 and 2, after the first cut shipped green on the wrong surface.*

`show_corporation_panel` drives draw_corporation_dashboard - nav slot 1, THE PLAYER'S OWN corporation at a glance. `show_corporations_table` drives draw_corporation_panel - nav slot 8, the all-corporations table. Each flag names the other one's function.

THIS IS NOT A TIDINESS COMPLAINT. BL-666's routing took the flag whose name matched the function it wanted and reproduced NR-700 one surface over: a press on a rival's ground opened the player's own dashboard. It got through because pointer_target's open_panel had no name for the table, so the check asserted 'corporation' and passed for either surface. Two independent guards failed to the same naming.

Both halves are now fixed at the sites - the routing takes the right flag with the trap recorded beside it, and open_panel distinguishes 'corporation' from 'corporations'. The rename itself is not taken: it touches nav_pane, app.cpp, view_nav and the verify seam, which is more than this batch should carry. Worth doing before the ledger batch (sprint 24) reviews both surfaces.

*Files: `src/ui/ui_state.hpp`, `src/ui/nav_pane.cpp`, `src/core/app.cpp`, `src/core/verify_api.cpp`*

### NR-705 — The corporations table's NAME column is clipped to a single character
*observation · raised 2026-08-28 · from Sprint 23, live-clicking BL-666's corporation destination in the built app.*

Opening the all-corporations table from a Corporation-lens press showed every row as a single letter followed by its figures - "C 1 bodies 1707.5 3% Neutral", "T 1 bodies -3677.3 0% Neutral". The header reads "C Reach Capital Share Stance", so the name column is present and squeezed to about one glyph wide.

The table is unusable as a comparison surface in that state: it is a list of forty firms you cannot tell apart, and the row BL-666 aims cannot be read as the firm the player just pressed. The Selection band names the firm correctly, so the routing is right and the presentation is not.

Found by looking, not by reading - the harness asserts open_panel and the aimed id, neither of which can see a column width. Sprint 24 (ledgers) owns this surface; filing it here so the batch that reviews it starts from a known defect rather than rediscovering it.

*Files: `src/ui/corporation_panel.cpp`*

### NR-707 — The convergent and divergent boundary masks OVERLAP, and the obvious reading is wrong
*observation · raised 2026-08-28 · from Sprint 23 wave 3, BL-660's data half; found by an assertion that failed.*

The brief for the divergent mask asked for an assertion that no tile is marked both convergent and divergent. IT FAILED, and the agent did not weaken it - which is the right outcome and the reason this is filed.

The cause is not a sign bug. The boundary walk visits each tile against TWO neighbours and marks per neighbour, so a tile at a junction between a closing pair and an opening pair is genuinely on both. height_bias has always accumulated the +0.12 uplift AND the -0.08 subsidence on that tile; the overlap is pre-existing and correct. Measured at 1 tile in 15120 on the fixture seed.

The assertion was re-specified to the invariant that actually holds - every both-marked tile sits on two DISTINCT classified boundaries - and the false 'exclusive' claim was corrected in the header, the .cpp and CONTINENTS.md. Both UI surfaces show the two counts SEPARATELY and never sum them, for the same reason.

Worth your eye because 'a tile is either colliding or rifting' is the intuitive model and the generator does not work that way.

*Files: `src/world/continents.cpp`, `docs/generation/CONTINENTS.md`, `tools/verify/continents_harness.cpp`*

### NR-708 — No envelope field has save round-trip coverage, and the save version just moved
*question · raised 2026-08-28 · from Sprint 23 wave 3; reported by the agent that bumped save_game_version to 2.*

The divergent mask is serialised in the ENVELOPE (w_continents / r_continents in src/core/save_game.cpp), and save_game_version went 1 -> 2. read_save_game compares the constant for STRICT EQUALITY and refuses the whole file, so every existing save is now unreadable. That follows this file's own conventions - it has no upgrade path and the agent correctly did not invent one - but it is worth knowing rather than discovering.

THE GAP THAT MATTERS MORE: the round trip is not covered by any harness, and never has been. save_game.cpp's translation unit reaches ui_state.hpp and so <imgui.h>, which the headless harness builder excludes by construction. `convergent` has had no round-trip coverage either, nor has any other envelope field - this change did not create the hole, it walked into it.

The fix is a CMake-declared envelope harness linking imgui, on the font_glyph_harness precedent. Worth an item: a serialisation seam with no read/write test is exactly where the asymmetry that corrupts a snapshot hides, and the max_overlay bug (NR-703) came out of this same file today.

*Files: `src/core/save_game.cpp`, `src/core/save_game.hpp`, `tools/verify/build_harness.js`*

### NR-709 — The fold-out column starves every stretch NAME column, and NR-705 is one of three
*observation · raised 2026-08-29 · from Sprint 24 (ledgers), the batch-3 capture pass: shell_pass.lua over the thirteen slots plus a new ledger_pass.lua over every sub-view and scroll foot.*

NR-705 filed the corporations table showing every firm as a single letter. The capture pass found the SAME defect on two more surfaces, so it is a class rather than an instance and should be fixed as one.

The mechanism, read out of corporation_panel.cpp:128-134: the column host is ~358 px (shell_column_width at 1720 wide), and the table declares Reach/Capital/Share at WidthFixed 62 each plus Stance at WidthFixed 220 - 406 px of fixed width in a 358 px box. The Corporation column is the only WidthStretch column, so it absorbs the whole overflow and collapses to one glyph. Any table in this column that puts its identity column on stretch behind fixed siblings loses the identity.

The three instances found: (1) the all-corporations table, header "C Reach Capital Share Stance", 88 rows of one letter (NR-705); (2) the Economy panel Markets view, header "R Supply Demand Price" - THE RESOURCE NAME IS GONE ENTIRELY, every row a colour swatch and three numbers; (3) the Economy panel Corps view, "CalonIntus-PaxisAthen Venture" cut mid-word by the Balance column.

The question for you is whether the fix is per-table (shrink Stance, elide names) or a column-host rule - a minimum identity width the fold-out enforces, so the next table added here cannot reintroduce it.

*Files: `src/ui/corporation_panel.cpp`, `src/ui/economy_panel.cpp`, `src/ui/foldout_column.hpp`*

### NR-710 — The History ledger Ages view produced no frame in nineteen minutes, and no check has ever opened it
*observation · raised 2026-08-29 · from Sprint 24 (ledgers), the batch-3 capture pass: shell_pass.lua over the thirteen slots plus a new ledger_pass.lua over every sub-view and scroll foot.*

MEASURED, this build, this fixture: selecting history_view = 2 (Ages) and rendering produced NO CAPTURE after nineteen minutes of solid CPU. The run was killed rather than left to finish, so the pass could reach the views after it.

Cause, read out of tile_inspector.cpp: the Ages cache is built inline on the drawing thread - run_history_sim(cached_ss, ..., start_year 0, stop_year campaign_epoch_year) over the body’s real terrain - so the first frame that shows the tab replays the whole Era -1 political history before it returns. In the built app that is a tab click that stops the application, with no progress and no way back.

WHAT IS AND IS NOT ESTABLISHED: that no frame arrived in nineteen minutes is measured. Whether the sim is very slow or does not terminate on this fixture is NOT established - CPU climbed steadily throughout, which is consistent with both. Neither reading should be repeated as fact.

WHY IT WAS NEVER SEEN: no verify script has ever selected this view. history_ledger_and_comms.lua covers Story and Chain and returns to Story. verify.ages_year was added with BL-277 and, until ledger_pass.lua, was called by nothing. A view shipped, documented in ledgers/tile_ledger.md, and never once rendered by a check.

SECOND HALF, same fix window: tile_inspector.cpp sets s.ages_year = 0 on the frame it rebuilds the cache, which is the frame a script park lands on - so even once it renders, a captured Ages frame shows year 0 whatever the script asked for.

*Files: `src/ui/tile_inspector.cpp`, `src/core/verify_api.cpp`, `scripts/verify/ledger_pass.lua`*

### NR-711 — The Economy ledger Holdings view still itemises every rival corporation stockpile
*observation · raised 2026-08-29 · from Sprint 24 (ledgers), the batch-3 capture pass: shell_pass.lua over the thirteen slots plus a new ledger_pass.lua over every sub-view and scroll foot.*

Captured live: Economy > Holdings prints one expandable block per (corp x body) with EXACT quantities per resource for corporations the player does not own - Faros-YelenKalen 126.9 Iron Ore, 8.0 Agricultural Produce, 2.5 Peat; Exoar-Exoex 26.1 / 8.0; and so on down the list.

That is the competitor-visibility rule (DISCOVERY.md) broken on the one surface that breaks it. Every other surface in the app meets the redaction standard - the Selection card, the hover card, and the corporations table, which prints a dash where a firm does not file.

The doc that owns this surface, docs/ui/ledgers/economy.md, already flags it and names BL-482 (economy panel pools leak) as the owner. THAT ITEM NO LONGER EXISTS: backlog_query returns nothing for BL-482. So the defect is live, documented, and unowned - the doc is citing a dead id as its fix.

Decision needed: refile the redaction as a sprint-24 item, or take economy.md’s own lead question (fold Economy into Corporation), which would delete the surface and the leak together.

*Files: `src/ui/economy_panel.cpp`, `docs/ui/ledgers/economy.md`, `docs/ui/DISCOVERY.md`*

### NR-712 — The Contracts ledger says ticks in two call sites, against the standing display rule
*observation · raised 2026-08-29 · from Sprint 24 (ledgers), the batch-3 capture pass: shell_pass.lua over the thirteen slots plus a new ledger_pass.lua over every sub-view and scroll foot.*

contracts_ledger.cpp:207 prints "Contract deadline in %d ticks" and :297 prints "Deadline in %d ticks". Both are on screen in the captures - the Offers view shows it three times over, once per offer.

The display word is qtr, never tick (NR-002, your ruling 2026-08-01) - a Tick is literally a calendar quarter, and every other surface honours it: the Construction ledger says "~N qtrs", the Market ledger convoy rows say "1 qtr" / "2 qtrs", the header says "/ qtr".

Two call sites, mechanical. Filed rather than fixed because it is one line of a larger contracts pass the batch is about to take, and because the same view has three other wording problems worth ruling on together: offers name their target as "Province #31808" (a raw entity id), the Accept press is greyed with an unexplained "(still filling)", and "escrow 5 / 400" appears with no definition anywhere on the surface.

*Files: `src/ui/contracts_ledger.cpp`*

### NR-713 — Three ledgers print a backlog id at the player, and seven of thirteen have no design doc
*observation · raised 2026-08-29 · from Sprint 24 (ledgers), the batch-3 capture pass: shell_pass.lua over the thirteen slots plus a new ledger_pass.lua over every sub-view and scroll foot.*

ON-SCREEN BACKLOG IDS, all three found in captures: balance_ledger.cpp:258 "Policy levers - not yet wired (BL-155)"; tech_tree_panel.cpp:599 "BL-087 design mock - read-only."; and the Tectonics view "First cut (BL-660)." A player has no way to resolve a BL- id, so the string says a thing is unfinished without saying what it will do. The honesty is right and the vocabulary is ours, not theirs.

DOC COVERAGE, counted against docs/ui/ledgers/: six of the thirteen slots have a 5-axis design doc (Balance, Construction, Corporation-as-slot-8, Market, Economy, History). SEVEN DO NOT - Contracts (slot 13), Generation (10), AI decisions (11), Strategy (12), Research (4), and the Corporation OVERVIEW DASHBOARD (slot 1), which corporation.md excludes in its own header note. Each of the seven has a question_log entry, so the surface question is recorded; none has the sub-levels / lens / data / toggle answers the other six carry.

ALSO STALE, found while reading: ledgers/tile_ledger.md documents THREE History views and names a "Tiles" view that no longer exists, while the code carries FOUR (Story/Chain/Ages/Tectonics, tile_inspector.hpp). ui_state.hpp’s own comment on history_view still reads "2=Tiles, 3=Ages" - two errors in one line. ui_elements.json carries UI-087 "Tiles view" as an element and has no entry for Tectonics.

*Files: `src/ui/balance_ledger.cpp`, `src/ui/tech_tree_panel.cpp`, `src/ui/ui_state.hpp`, `docs/ui/ledgers/tile_ledger.md`, `docs/ui/ui_elements.json`*

### NR-714 — Two capture instruments cannot see what they are named for, and the clipping ledger passes vacuously over six clipped strings
*observation · raised 2026-08-29 · from Sprint 24 (ledgers), the batch-3 capture pass: shell_pass.lua over the thirteen slots plus a new ledger_pass.lua over every sub-view and scroll foot.*

THE CHECK NAMED FOR THE RANKING CANNOT SEE IT. budget_ledger_ranked.lua captures two frames; the Balance ledger top-buildings ranking is in neither. One frame is scrolled to the head, where the ranking is below the fold; the other has the ledger CLOSED. The ranking does exist and reads well - ledger_pass scrolled foot capture is the first frame that has ever contained it.

SCROLL CANNOT REACH TWO LEDGERS. verify.scroll_panel resolves a WINDOW name, so it moves Balance, History, Economy and the corporations table (head and foot differ) but does nothing on the Market ledger, whose price list is an inner child region (head and foot are byte-identical), and has no key at all for the Generation ledger (same). So everything below the first three goods in the Market ledger has never been captured.

THE CLIPPING LEDGER IS VACUOUS ON THIS CLASS. expect_no_clipping reports "PASS: 0 failure(s), 0 RECORD(S) total" over both passes, on frames that visibly contain at least: "82% of labour demand n" (Corporation dashboard), "Cr 0.10 /" (Balance, Laws), "CalonIntus-PaxisAthen Ventu" (decision feed), "- 8 decision" (Strategy), "Huhaidar -> Kua Sua" (Market convoys), "Extraction Site [260" (Balance ranking). This is NR-663 again with a bigger sample: table cells and SmallButton labels are not instrumented into the overflow ledger, so the one free assertion the pass makes is a green that means nothing.

The assertion is left in ledger_pass.lua deliberately, with this recorded beside it, so that instrumenting the ledger turns it red without anyone re-authoring the script.

AND THE COVERAGE TOOL CANNOT SEE EITHER PASS. ui_coverage --captures attributes a PNG to an element through the element checks array, which names scripts. Neither shell_pass nor ledger_pass is named against any element, so a run that produced 36 fresh ledger frames reports FOUR of the thirty ledger elements as having something to look at - and the four are attributed to the older single-purpose scripts, not to the passes. The spine this sprint scopes off is blind to the instrument the sprint uses. (Second, smaller trap in the same tool: --captures defaults to a screenshots dir at the repo root, and the app writes to build/screenshots, so the bare flag reports zero captures rather than saying it looked in the wrong place.)

*Files: `scripts/verify/budget_ledger_ranked.lua`, `scripts/verify/ledger_pass.lua`, `src/core/verify_api.cpp`, `tools/session/ui_coverage.js`, `docs/ui/ui_elements.json`*

### NR-715 — ledger_pass.lua is new and unauthorised as a skill - a second class-wide capture instrument beside shell_pass
*novel-work · raised 2026-08-29 · from Sprint 24 (ledgers), the batch-3 capture pass: shell_pass.lua over the thirteen slots plus a new ledger_pass.lua over every sub-view and scroll foot.*

The batch needed every ledger SUB-VIEW, and shell_pass.lua only opens each slot on its default view. I authored scripts/verify/ledger_pass.lua: 36 captures over the thirteen slots - four expanded corp roll-ups, the Balance foot, three Economy views, four Research eras, three Market views, both ends of the corporations table, four History views and its three Chain rounds, the Generation ledger, both observability panels, three Contracts views.

Flagged because the standing rule says a check without a tool should become one, and "tool creation is skill creation" needs your permission before it is named in a SKILL.md. verifier-visual auto-discovers scripts/verify/*.lua so it is RUNNABLE without that; naming it as a review instrument beside shell_pass is your call.

It also deliberately does NOT capture the Ages view (NR-710) and records why in its own footer, which is the second thing worth your eye: a capture instrument that documents what it cannot reach, rather than quietly covering twelve of thirteen and reading as if it covered all of them.

*Files: `scripts/verify/ledger_pass.lua`*

### NR-716 — shell_pass walks every rail slot by NAME, so it cannot see a renumbered rail
*observation · raised 2026-08-29 · from Sprint 24, verifying BL-676 (retire economy panel) after the rail was renumbered twice in one session.*

BL-676 moved Construction to slot 3, BL-675 had inserted Acquisitions at 5, and everything between shifted. The failure mode of a renumber is a rail that compiles and silently opens the WRONG SURFACE from a slot. shell_pass.lua is the check that looks like it would catch that - it is described as walking the thirteen nav-rail ledgers - and it CANNOT.

It reaches every ledger through verify.show_panel("construction", true), which writes the ui_state flag directly. It never presses a rail slot, so nav_pane.cpp's switch is not on its path at all. Its green says every ledger DRAWS; it says nothing about which door opens which one. The capture filenames carry slot numbers, which makes it read as stronger evidence than it is - shell_22_slot03_construction.png is named by the script author, not measured.

I verified the mapping by READING the switch instead, case by case, and it is correct. That is not a check and it will not survive the next renumber.

Same shape as the four instances Sprint 21 collected: a check whose green means less than it appears. The fix is small - a check that clicks each rail slot at its known screen position and asserts pointer_target().open_panel is the expected surface, which verify.click and the existing open_panel field already support. Worth doing because the rail has now been renumbered twice in one session and will move again when Corp. Strategy is built.

*Files: `scripts/verify/shell_pass.lua`, `src/ui/nav_pane.cpp`*

### NR-717 — A design paragraph written in the present tense was read as shipped code, by me and then by an agent
*observation · raised 2026-08-29 · from Sprint 24, BL-678 (companies are open). Found by the agent measuring what the brief asserted.*

I briefed an agent that retiring closure was risky because "rivals buy too — `buy_corporation` is scored in `corp_ai.cpp`'s candidate list under the solvency gate", and told Ben the same thing in the same words. I took it from FINANCE.md § Whole-firm acquisition, which said exactly that in the present tense.

IT IS NOT BUILT. `buy_corporation` appears NOWHERE in `src/world/corp_ai.cpp`, and BL-629 (rival acquisition) is status `designed`. The agent found it by measuring rather than assuming: rival acquisitions came back 0.00 per run on all twelve seeds both before AND after its change, and it went looking for why instead of reporting a clean result. I confirmed it myself with a grep before merging.

THE DOC WAS NOT WRONG, IT WAS UNREADABLE IN ONE PARTICULAR WAY. It ended with "Design: BL-629 (rival acquisition)", so a careful reader had the signal. But the body said `buy_corporation` "JOINS the corp_command seam and IS SCORED in corp_ai.cpp's EXISTING deterministic candidate list" — present tense, naming the real file, calling the list existing. That sentence is indistinguishable from a description of shipped code. Fixed at the site by moving the paragraph to the conditional and recording why.

WHY THIS IS WORTH YOUR EYE RATHER THAN JUST A FIX. The state-independence rule (io-standing-rules.md § Terms & docs) says a doc must never record whether a thing is built. This paragraph obeyed the LETTER of that — it recorded no build status — and still misled two readers into believing something was built, because present-tense prose about a named file reads as a report. The rule may want its positive half stated: a design paragraph that names its implementation site should be written in the conditional. I have NOT made that edit to the standing rules, because widening a standing rule is yours.

THE CONSEQUENCE IS ALSO STILL LIVE. The snowball risk I raised is real but LATENT: it arrives with BL-629, and it will arrive against a field of ~82 buyable firms rather than the 1.6 that existed when BL-629 was designed. Worth knowing before that item is scheduled.

*Files: `docs/economy/FINANCE.md`, `src/world/corp_ai.cpp`, `.claude/rules/io-standing-rules.md`*

### NR-718 — BL-176's empty-room fix was deleted with the tab it lived on, and the problem came back unnoticed
*observation · raised 2026-08-29 · from Sprint 24, opening the Construction ledger review after Ben moved it to rail slot 3.*

The Construction panel draws two lines: "Estimated cost: 0.0 / quarter" and "No active construction." That is the whole surface, and it now sits at slot 3.

IT WAS DIAGNOSED AND FIXED ONCE ALREADY. `ui_state::construction::panel_view` still carries the comment: "Defaults to Buildings (BL-176): the queue is empty most of the time, so opening on it made the panel's front door an empty room, while the player always owns buildings." BL-176 found this exact problem and solved it by opening on a Buildings roster instead of the queue.

THE 2026-08-15 REWORK DELETED THE FIX ALONG WITH THE TAB. construction_panel.cpp says so plainly: the Buildings tab and its inline detail moved onto the building Selection card, "so the tab switcher and its selection-driven auto-focus are both gone with it; there is nothing left to switch between." That is a defensible move on its own terms — per-building configuration IS targeted, and the menus-are-broad-ledgers rule puts it on a Selection card. But it left the panel opening on the queue, which is the empty room BL-176 named, and nothing recorded that the fix had been undone.

IT WENT UNNOTICED BECAUSE OF WHERE THE SLOT SAT. The panel was slot 6, then 7 after the Acquisitions insert. Ben moved it to 3 on 2026-08-29, which is why it is being read now. The rework was two weeks earlier.

DEAD STATE LEFT BEHIND, and this is the part with a cheap fix. `construction.panel_view` is now WRITE-ONLY: verify_api.cpp:2241 sets it and nothing in src/ reads it. `construction.panel_focus_building` is referenced nowhere outside its own declaration. Both are fields describing a two-view panel that has one view, and the panel_view comment is a detailed description of a feature that does not exist — the kind of comment that is worse than none, because it reads as current.

WHAT I WOULD NOT DO: quietly restore the Buildings tab. The rework moved that content for a reason and the doc (docs/ui/ledgers/construction.md) still carries "does a Buildings roster earn a tab?" as an open question for Ben. The question this raises is not "put it back" but "what should the third slot on the rail show when nothing is building?" — and that is a design call, not a defect fix.

*Files: `src/ui/construction_panel.cpp`, `src/ui/ui_state.hpp`, `src/core/verify_api.cpp`, `docs/ui/ledgers/construction.md`*

### NR-719 — Two of four scrolled captures are the unscrolled frame, for two different reasons
*observation · raised 2026-08-29 · from Sprint 24, gathering Market captures for its redesign. Found by hashing two captures rather than by looking at them.*

`ledger_05_market_0_prices.png` and `ledger_05_market_0_prices_foot.png` are BYTE-IDENTICAL — same md5. The one named for the foot of the list is the head of the list. The Budget pair from the same run differ correctly, so the harness works; this surface defeats it.

THE CAUSE: `verify.scroll_panel("market", …)` requests scroll on the window named "Market Ledger" (verify_api.cpp § scroll_panel), but the price sparklines are stacked inside an INNER child scroll region under "Price over time". The outer window scrolls and the inner child does not, so the request lands on the wrong scroller and silently does nothing.

THE CONSEQUENCE, and it is the reason this is filed rather than just fixed: the Market ledger prices roughly 42 goods, of which about 3.5 fit the column. NOTHING HAS EVER LOOKED AT GOODS 4 THROUGH 42 — not a capture, not a golden, not a human. Whatever is wrong down there has never been visible. That is a live blind spot going into the surface's redesign, and it is the single most useful thing to fix first.

AND I WROTE THE CHECK THAT LIED. `ledger_pass.lua` is mine, from this session; its whole justification is that shell_pass only sees default views and misses what is below the fold. It produced a capture named `_foot` containing the head, and I did not notice until I hashed the pair — I had LOOKED at both images and read them as the same surface without registering that they were the same PIXELS. Exactly the shape Sprint 21 collected four instances of: a check whose green means less than it appears. The scroll helper needs to assert that the frame actually moved, which is one comparison and would have caught this on the run that introduced it.

The same doubt applies to every other `foot()` call in that script. The Budget pair is verified different; the corps-table, generation and history feet are not, and should be hashed before any of them is cited as evidence.

HASHED ALL FOUR PAIRS AFTER FINDING THE FIRST. Two are false and the causes differ:
  - MARKET prices: identical. scroll_panel knows the name and aims at the window "Market Ledger", but the sparklines live in an INNER child scroller, so the request lands on the outer window and moves nothing.
  - GENERATION ledger: identical, and worse in kind. scroll_panel handles only tile/history, market, balance, corporation and construction - there is NO generation_ledger case at all. Its own comment says an unknown name CLEARS the request, so the call is a silent no-op by design. A caller cannot tell "scrolled to the foot" from "this panel has no scroll hook".
  - Corps table and History story: verified genuinely different. Those two are sound.

SO THE FIX HAS TWO HALVES. scroll_panel should REJECT an unknown name loudly rather than clearing the request - a silent no-op on a typo or an unhandled panel is how a capture comes to be named for content it does not contain. And the market case needs the request to reach the child scroller, not the window.

*Files: `scripts/verify/ledger_pass.lua`, `src/core/verify_api.cpp`, `src/ui/market_ledger.cpp`*

### NR-720 — quarterly_return R2 fails on main and nothing owns it
*observation · raised 2026-08-29 · from Sprint 24, verifying BL-685 (exchange record). Reported by the agent as pre-existing; confirmed here by a different method.*

`quarterly_return` fails R2 — "every filed net == that tick's measured apply_budget delta, EXACTLY" — on current main. A search of the backlog and this file finds NO owner: no item, no prior entry, nothing.

IT IS NOT BL-685's. The agent said so having reverted `market_clearing.cpp` to HEAD and reproduced. I checked it a different way rather than re-running its experiment: the whole clearing diff is ONE lambda that constructs an `exchange_record` and pushes it, called at four sites. No arithmetic, no balance write, no mutation of supply, demand or price. A budget identity cannot be broken by an append.

WHY IT MATTERS MORE THAN ONE RED ROW. R2 is the assertion that the FILED return matches what the money loop actually did — the quarterly return is what the profitability ledger prints, what the acquisition price is read off (`corp_acquisition_price` uses trailing net), and what a player judges a buyout by. If a filed net can disagree with the real delta, then the price on the Acquisitions ledger is computed from a number that does not match the books. That is a correctness question about a figure now on two surfaces, not a stale test.

It may well be small — a rounding tolerance, an ordering, a flow the harness does not model. But "exactly" is what it asserts, and nobody has looked.

RELATED, and the reason this went unnoticed: `spectator_determinism` is also red on main, tracked as NR-661 (its golden was stale before wave 1). Two red harnesses on main train the eye to expect red, which is how a third arrives unremarked.

*Files: `tools/verify/quarterly_return.cpp`, `src/world/budget_system.cpp`, `src/world/components.hpp`*

### NR-721 — About thirty harnesses could not compile from a clean configure, and a release cut passed anyway
*observation · raised 2026-08-29 · from Sprint 24, BL-685. Found by an agent that could not run the harnesses its brief required.*

Since `b668434c` — the v0.1.21 cut — `harness_params.hpp` reaches `<sol/sol.hpp>`, but CMake's glob loop for `tools/verify/*.cpp` published only `src` as an include dir. Roughly THIRTY harnesses therefore could not compile from a clean configure: `determinism_harness`, `world_determinism`, `spectator_determinism`, `quarterly_return`, `save_roundtrip`, `demand_census` and more.

FIXED as part of BL-685 (the glob now carries the sol2 and Lua include paths; linking Lua stays opt-in), taken because it blocked that item's own verification step.

THE PART WORTH YOUR ATTENTION IS HOW IT SURVIVED. Existing build trees kept passing, because they had stale `.obj` files from before the header changed — so every developer and every CI run with a warm tree saw green. Only a clean configure exposed it, and nothing does a clean configure. A RELEASE WAS CUT over it.

This is the same family as the four instances Sprint 21 collected under "a check whose green means less than it appears" — and a worse one, because the green was not merely weak, it was measuring a tree that could no longer be reproduced from source. The memory `io-headless-build-invocation` already records the sibling trap (ctest runs stale exes without rebuilding).

The question this raises is process, not code: is a periodic clean-configure build worth adding to the loop, and where — `check.bat`, CI, or a release-cut precondition? A build that only works from a warm tree is a build that works by accident.

*Files: `CMakeLists.txt`, `tools/verify/harness_params.hpp`*

### NR-722 — Arming a lens on ledger open has no owning doc, and slot 7 is the first to do it
*novel-work · raised 2026-08-29 · from Sprint 24, BL-689 (convoys ledger). Flagged by the agent, filed here.*

The Convoys ledger arms `supply_routes` when it opens. NO LEDGER HAS EVER DONE THIS — every ledger doc in `docs/ui/ledgers/` proposes a lens-on-open and none was built, so the pairing existed only as a recurring proposal until now.

It is a good pairing and it is the reason Convoys left the Market ledger at all: a tab strip can arm only one lens, and the old third tab always got the price wash when it wanted the lane overlay. But the BEHAVIOUR is unowned. `LENSES.md` documents no menu-triggered arm; the ledger docs propose them individually with no rule behind them.

The questions a rule would have to answer, none of which any doc does: does closing the ledger DISARM the lens, or does it persist (corporation.md guessed persist, on the grounds that lens state is canvas-owned)? Does opening a second ledger re-arm to that one's lens? What happens when the player has deliberately chosen a lens and then opens a ledger — is their choice overridden?

Slot 7 answers all three by implementation rather than by design, and the next ledger to want a lens will copy whatever it did. Worth Ben settling before that happens.

*Files: `src/ui/convoys_ledger.cpp`, `docs/ui/LENSES.md`, `docs/ui/ledgers/`*

### NR-723 — The nav rail's height became a real constraint at fourteen slots, and it broke 720p
*novel-work · raised 2026-08-29 · from Sprint 24, BL-689. Caught and fixed by the agent inside its own batch.*

Adding the fourteenth slot put slot 14's centre at **y = 746 against a 720 px display**. ImGui rejects presses outside the window, so the Contracts ledger was DRAWN AND UNREACHABLE — visible, apparently fine, and inert.

IT WAS INVISIBLE AT 1080p. The same layout is correct there, so no single-resolution capture could have caught it; it needed the pair. This is the counter-case to the same day's finding that reviews should happen at 1080p (NR-719) — the review resolution and the FIT resolution are different questions, and `shell_pass` staying at 720p is what makes the second answerable.

Fixed by deriving slot size from the rail's actual height, capped at its width: 1080p is pixel-identical, 720p compresses. Saved as `scripts/verify/nav_rail_fit.lua`.

THE NOVELTY IS THAT NO DOC COVERS THE RAIL'S HEIGHT BUDGET. `MENU.md` curates which slots exist and `LAYOUT.md` places the rail, but nothing says how many slots fit the smallest supported display, or what should give when they stop fitting — smaller slots, a scroll, a second column, or a cap on the curated set. The rail has grown 13 -> 14 twice in one day. The next addition needs a rule rather than another fix.

*Files: `src/ui/nav_pane.cpp`, `docs/ui/MENU.md`, `docs/ui/LAYOUT.md`, `scripts/verify/nav_rail_fit.lua`*

### NR-724 — Deleting the Production card orphaned the growth-track readout, and two docs now assert it exists
*question · raised 2026-08-29 · from Sprint 24, BL-691. Found by the agent that deleted the card; it correctly refused to resolve it.*

BL-691 cut three of four roll-up cards from the Corporation ledger on Ben's 2026-08-29 call. One of them, Production, was the ONLY DISPLAY of the chain-depth growth track — and it was put there by Ben's own earlier ruling.

BL-591 (2026-08-23): *"the depth readout goes on the corporation dashboard"*. The reason it was built at all is that `corp_reached_depth` GATES SIX PLACES in the simulation and was displayed in none of them. So the readout existed to close exactly the gap that deleting the card has just reopened.

TWO LIVE DOCS NOW ASSERT SOMETHING UNTRUE, and neither was edited:
  - `docs/economy/PRODUCTION.md` § Chain depth (~line 732): "The readout lives on the corporation dashboard, not the building card", with its three lines described in detail.
  - `docs/development/user_stories.json:500`: a surface named "Corporation dashboard — Production card, Growth track".

THE AGENT WAS RIGHT NOT TO FIX EITHER. Choosing between "give the growth track a new home" and "retire the readout" is a design call, and editing PRODUCTION.md would have taken it silently in one direction — the exact failure mode the state-independence rule exists to prevent. Newest-dated wins, so the 2026-08-29 cut stands; what it leaves behind is Ben's.

THE OPTIONS, none costed:
  - **Fold it into the Balance card.** Cheapest, but it is a production fact on a money surface, which is the reasoning that removed Production in the first place.
  - **Move it to the Construction ledger's Buildings tab.** That tab now holds the estate and its levers, and chain depth is a fact about what the estate can make next. This is my lean.
  - **Retire the readout.** Legitimate — but it puts `corp_reached_depth` back to gating six places and displaying in none, which is the state BL-591 was filed to end.

WORTH NOTING AS A PATTERN RATHER THAN AN INCIDENT: this is the THIRD time this sprint a deletion has silently orphaned an earlier ruling — BL-176's empty-room fix went with the Buildings tab (NR-718), NR-245's controls-on-the-card went with the levers (recorded in BL-683), and now BL-591's readout goes with the Production card. Each was found by reading rather than by any check. A deletion that removes a surface should ask what was placed there by a ruling, and nothing in the method prompts that question.

*Files: `docs/economy/PRODUCTION.md`, `docs/development/user_stories.json`, `src/ui/corporation_dashboard.cpp`, `src/ui/construction_panel.cpp`*

### NR-725 — Only ONE market-bearing body ever exists — measured to 400 econ ticks
*observation · raised 2026-08-29 · from Sprint 24, BL-687. Found because the Trades gate needed a second body to test its shut half, and there is not one.*

Measured at 16, 60, 120, 240 and **400 econ ticks** — a hundred in-game years: **one market-bearing body**, every time. The number is now printed by `trades_tab.lua` on every run ("MEASURED market-bearing bodies: 1") rather than living in a report.

THE MECHANISM IS WORKING EXACTLY AS DESIGNED, WHICH IS THE PROBLEM. `MARKETS.md` § Spontaneous market emergence: generation seeds markets **only on the home body**, and an off-world market comes into existence the tick a body's first building COMPLETES there — "investment, not presence", deliberately not a survey completing. Nothing ever builds off-world in a hundred years, so the trigger never fires.

So a designed and implemented mechanism — with its own pricing rule (`price_distance_gain`), its own demand injection (`inject_interbody_demand`, the counterpart rule, the one-tick lag), and its own harness (`interbody_pull_harness`) — has never once run in a played world.

WHAT IT TOUCHES BEYOND THE TAB, and this is why it is filed rather than noted:
  - The Market ledger's **Body combo has exactly one entry**, always. Its "which body" question is not a question.
  - `body_average_price` (BL-686) averages across the markets of the only body there is.
  - The **Convoys ledger** ships as an inter-body cargo read while all traffic is intra-body, between the home body's nine carved markets.
  - Inter-body trade, the distance pricing, and the whole commercial half of the space arc are unexercised — not unbuilt, UNEXERCISED, which is harder to notice.

IT IS NOT OBVIOUSLY A DEFECT. A hundred years with no off-world investment may be the honest consequence of an economy where nothing off-world pays yet — that is a finding about the ECONOMY, not the market layer. But it means several surfaces are designed against a world shape that does not occur, and at least one harness asserts behaviour no player will meet.

The question for Ben: is off-world investment meant to happen inside a campaign, and if so what is supposed to trigger it? If not, the surfaces that assume multiple bodies should say so.

*Files: `docs/economy/MARKETS.md`, `src/world/economy_system.cpp`, `src/ui/market_ledger.cpp`, `docs/ui/ledgers/market.md`, `docs/ui/ledgers/convoys.md`*

### NR-726 — Two novel verification techniques from the Trades check, both worth a ruling before they spread
*novel-work · raised 2026-08-29 · from Sprint 24, BL-687. Flagged by the agent, filed here.*

TWO THINGS NO EXISTING CHECK DOES, both defensible, both worth Ben deciding on before they become the pattern.

**1. Demolition as a verification instrument.** The Trades gate has two branches — the player owns a building on this body, or does not — and the shut branch is unreachable by selection because only one market-bearing body exists (NR-725). Rather than weaken the assertion to something it could reach, the check DESTROYS WORLD STATE to get there: it demolishes the player's three buildings through the real `demolish` verb, then asserts the gate is shut while the world still holds all 24 orders.

It is defensible on three counts — a real verb rather than a back door, it runs LAST so nothing downstream inherits the wreckage, and the alternative was an assertion that could never fire. But no other check mutates the world to reach a predicate, and "destroy the world to test the else branch" is a technique that will be copied badly if it is not bounded. Worth a rule: last-only, real verbs only, and state what was destroyed.

**2. A verify reader that exists to FIND a state rather than assert one.** `market_bodies` and `player_operates_on` were added not to check a claim but to discover whether a claim was checkable. The brief assumed the shut state was reachable; the honest answer was to measure that it is not, and report the number.

That is a different kind of reader from every other one in `verify_api` — the rest answer "is this true", these answer "is this testable". It is the shape that turns a brief's wrong premise into a finding instead of a weakened test, which this sprint has now wanted four times.

*Files: `scripts/verify/trades_tab.lua`, `src/core/verify_api.cpp`, `docs/development/DEVELOPMENT_PRACTICES.md`*

### NR-727 — Chain depth is not the brake on the ancient economy - half the ungated roster already produces nothing
*observation · raised 2026-08-29 · from Sprint 24, measuring BL-692 before building it.*

Retiring the depth gate was expected to open the ancient roster. Measured, it opens **six recipes, of which two stay shut on their own** — and the evidence that depth was never the constraint is already on disk.

**Sawmill, Stonemason, Potter's Kiln and Weaver are required-depth 0. They are open today. They produce 0.0.** `scripts/economy.lua` (2026-08-26) says it outright: *"NOTHING IN THE SHIPPED ANCIENT WORLD PRODUCES TOOLS OR PLANKS."* Six of eight design channels are ABSENT in this band, and seven produced goods have no sink at all.

So a corp cannot reach depth 1 not because a gate refuses it, but because the inputs do not exist to produce. Removing the gate changes which door is locked; it does not put anything behind the door.

THIS REFRAMES THE PROGRESSION QUESTION. Ben's instinct — that unlocks should follow research rather than economy — may well be right on its own terms. But retiring depth will not make the ancient economy progress, because the thing stopping it is missing supply, not a lock. Expect the measurement to show little movement, and do not read that as the retirement having failed.

The adjacent facts, same measurement: market demand is **0.000** for iron_blooms, trade_goods_misc, leather, tools, rigging, ordnance and steel; starvation is **30.9%** of processing building-ticks; and the glut forecast that should penalise building into no demand is switched off (`corp_ai.cpp:372-398` returns 1.0 when demand is zero).

*Files: `scripts/economy.lua`, `src/world/corp_ai.cpp`, `docs/economy/PRODUCTION.md`, `tools/verify/tier_margin.cpp`, `tools/verify/demand_census.cpp`*

### NR-728 — The tech-gate ids do not match tech_tree.lua, so the viewer shows the wrong unlock
*observation · raised 2026-08-29 · from Sprint 24, found in passing while measuring BL-692.*

`tech_tree.cpp:95-99` resolves the simulation's gates against `tech_tree.lua` **by id**. The ids do not line up:
  - `E0-EC-01` — the simulation's **Toolmaker** unlock. In `tech_tree.lua` (line 408) that id is **"Semiconductor Fabrication"**.
  - `E0-EC-03` — the simulation's **refined_copper** unlock. In the Lua (line 414) that id is **"Semiconductor Fabrication II"**.
  - `E1-EC-01` — the simulation's **steel_bessemer** unlock. **It does not exist in the Lua file at all.**

So the F9 tech-tree viewer shows the Toolmaker gate's requirement list attached to a semiconductor node the simulation will never deliver. The viewer is not wrong about its own data; the two stores simply disagree and nothing checks that they agree.

RELATED AND PARTLY CONTRADICTED: NR-591 records the opposite belief and holds for `E1-EC-01` only; NR-687 names a non-existent `E0-EC-02`. Both should be reconciled against this.

CONTEXT THAT MAKES IT LOW-URGENCY BUT WORTH FIXING BEFORE THE TECH TREE IS BUILT: `tech_tree.lua`'s own header says **"DATA ONLY … nothing in the simulation reads this"**, and its ~150 nodes census as 83 derived, 40 sketch, 26 stub, 1 parked. The mismatch costs nothing today because the viewer is a mock. It costs a great deal the moment research becomes real and someone assumes the two stores were ever reconciled.

*Files: `src/world/tech_tree.cpp`, `scripts/tech_tree.lua`, `src/world/tech_gate.cpp`*

### NR-729 — Retiring mercenary contracts leaves "paid for outcomes" without a mechanism, and the progression chain with a gap
*question · raised 2026-08-29 · from Sprint 24, BL-693. Raised before acting rather than discovered after.*

Ben retired the mercenary contract on 2026-08-29. Two authority claims now have nothing behind them, and both are load-bearing rather than decorative.

**1. CONCEPT.md § Player identity.** *"The player is a mercenary company... it is armed and for hire... and it is PAID FOR OUTCOMES."* The mercenary contract WAS being paid for outcomes. With it retired the player earns by trade alone — which is a coherent game, but it is not the sentence the identity doc opens with. Either the clause needs a new referent, or the identity needs restating.

**2. SYSTEMS.md / CONTRACTS.md progression chain.** `markets -> CONTRACTS -> force -> territory`, with the stated argument that *"the market cannot sell you a lead time, a refusal, or a reputation"*. Contracts is the middle link — the thing that turns market income into a reason to hold force. Remove it and markets connects to force through nothing.

NEITHER IS AN ARGUMENT AGAINST THE RETIREMENT. The measured state supports Ben: the surface was inert (`can_accept = fully_escrowed && !expired`, and seven offers read escrow 0/400 because client nations never fund them), so the loop was not running anyway. Retiring a mechanism that never ran is honest.

WHAT IS OWED IS THE DOC WORK AND ONE DECISION. Ben has said the military attention-getter is still open and that *"tech will later hold these elements"*. So the question is narrow: **until tech carries it, does the player have an income loop other than trade, and should CONCEPT.md say so plainly rather than keep the mercenary framing?** Answering "trade only, for now" is a perfectly good answer — it just has to be written down, because the identity doc is the one place a reader goes to learn what the player IS.

The buy side (procurement, BL-350) is untouched and stays — it is live, and the identity depends on it separately.

*Files: `docs/CONCEPT.md`, `docs/SYSTEMS.md`, `docs/economy/CONTRACTS.md`, `docs/development/ROADMAP.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

