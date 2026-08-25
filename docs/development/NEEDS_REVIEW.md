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

*53 entries — 40 open, 13 resolved.*

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

