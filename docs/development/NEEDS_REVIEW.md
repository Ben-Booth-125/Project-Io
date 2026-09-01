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

*125 entries — 117 open, 8 resolved.*

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

### NR-704 — The two corporation ui_state flags are named the wrong way round
*observation · raised 2026-08-28 · from Sprint 23, BL-666; the review barrier's findings 1 and 2, after the first cut shipped green on the wrong surface.*

`show_corporation_panel` drives draw_corporation_dashboard - nav slot 1, THE PLAYER'S OWN corporation at a glance. `show_corporations_table` drives draw_corporation_panel - nav slot 8, the all-corporations table. Each flag names the other one's function.

THIS IS NOT A TIDINESS COMPLAINT. BL-666's routing took the flag whose name matched the function it wanted and reproduced NR-700 one surface over: a press on a rival's ground opened the player's own dashboard. It got through because pointer_target's open_panel had no name for the table, so the check asserted 'corporation' and passed for either surface. Two independent guards failed to the same naming.

Both halves are now fixed at the sites - the routing takes the right flag with the trap recorded beside it, and open_panel distinguishes 'corporation' from 'corporations'. The rename itself is not taken: it touches nav_pane, app.cpp, view_nav and the verify seam, which is more than this batch should carry. Worth doing before the ledger batch (sprint 24) reviews both surfaces.

*Files: `src/ui/ui_state.hpp`, `src/ui/nav_pane.cpp`, `src/core/app.cpp`, `src/core/verify_api.cpp`*

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

### NR-717 — A design paragraph written in the present tense was read as shipped code, by me and then by an agent
*observation · raised 2026-08-29 · from Sprint 24, BL-678 (companies are open). Found by the agent measuring what the brief asserted.*

I briefed an agent that retiring closure was risky because "rivals buy too — `buy_corporation` is scored in `corp_ai.cpp`'s candidate list under the solvency gate", and told Ben the same thing in the same words. I took it from FINANCE.md § Whole-firm acquisition, which said exactly that in the present tense.

IT IS NOT BUILT. `buy_corporation` appears NOWHERE in `src/world/corp_ai.cpp`, and BL-629 (rival acquisition) is status `designed`. The agent found it by measuring rather than assuming: rival acquisitions came back 0.00 per run on all twelve seeds both before AND after its change, and it went looking for why instead of reporting a clean result. I confirmed it myself with a grep before merging.

THE DOC WAS NOT WRONG, IT WAS UNREADABLE IN ONE PARTICULAR WAY. It ended with "Design: BL-629 (rival acquisition)", so a careful reader had the signal. But the body said `buy_corporation` "JOINS the corp_command seam and IS SCORED in corp_ai.cpp's EXISTING deterministic candidate list" — present tense, naming the real file, calling the list existing. That sentence is indistinguishable from a description of shipped code. Fixed at the site by moving the paragraph to the conditional and recording why.

WHY THIS IS WORTH YOUR EYE RATHER THAN JUST A FIX. The state-independence rule (io-standing-rules.md § Terms & docs) says a doc must never record whether a thing is built. This paragraph obeyed the LETTER of that — it recorded no build status — and still misled two readers into believing something was built, because present-tense prose about a named file reads as a report. The rule may want its positive half stated: a design paragraph that names its implementation site should be written in the conditional. I have NOT made that edit to the standing rules, because widening a standing rule is yours.

THE CONSEQUENCE IS ALSO STILL LIVE. The snowball risk I raised is real but LATENT: it arrives with BL-629, and it will arrive against a field of ~82 buyable firms rather than the 1.6 that existed when BL-629 was designed. Worth knowing before that item is scheduled.

*Files: `docs/economy/FINANCE.md`, `src/world/corp_ai.cpp`, `.claude/rules/io-standing-rules.md`*

### NR-721 — About thirty harnesses could not compile from a clean configure, and a release cut passed anyway
*observation · raised 2026-08-29 · from Sprint 24, BL-685. Found by an agent that could not run the harnesses its brief required.*

Since `b668434c` — the v0.1.21 cut — `harness_params.hpp` reaches `<sol/sol.hpp>`, but CMake's glob loop for `tools/verify/*.cpp` published only `src` as an include dir. Roughly THIRTY harnesses therefore could not compile from a clean configure: `determinism_harness`, `world_determinism`, `spectator_determinism`, `quarterly_return`, `save_roundtrip`, `demand_census` and more.

FIXED as part of BL-685 (the glob now carries the sol2 and Lua include paths; linking Lua stays opt-in), taken because it blocked that item's own verification step.

THE PART WORTH YOUR ATTENTION IS HOW IT SURVIVED. Existing build trees kept passing, because they had stale `.obj` files from before the header changed — so every developer and every CI run with a warm tree saw green. Only a clean configure exposed it, and nothing does a clean configure. A RELEASE WAS CUT over it.

This is the same family as the four instances Sprint 21 collected under "a check whose green means less than it appears" — and a worse one, because the green was not merely weak, it was measuring a tree that could no longer be reproduced from source. The memory `io-headless-build-invocation` already records the sibling trap (ctest runs stale exes without rebuilding).

The question this raises is process, not code: is a periodic clean-configure build worth adding to the loop, and where — `check.bat`, CI, or a release-cut precondition? A build that only works from a warm tree is a build that works by accident.

*Files: `CMakeLists.txt`, `tools/verify/harness_params.hpp`*

### NR-723 — The nav rail's height became a real constraint at fourteen slots, and it broke 720p
*novel-work · raised 2026-08-29 · from Sprint 24, BL-689. Caught and fixed by the agent inside its own batch.*

Adding the fourteenth slot put slot 14's centre at **y = 746 against a 720 px display**. ImGui rejects presses outside the window, so the Contracts ledger was DRAWN AND UNREACHABLE — visible, apparently fine, and inert.

IT WAS INVISIBLE AT 1080p. The same layout is correct there, so no single-resolution capture could have caught it; it needed the pair. This is the counter-case to the same day's finding that reviews should happen at 1080p (NR-719) — the review resolution and the FIT resolution are different questions, and `shell_pass` staying at 720p is what makes the second answerable.

Fixed by deriving slot size from the rail's actual height, capped at its width: 1080p is pixel-identical, 720p compresses. Saved as `scripts/verify/nav_rail_fit.lua`.

THE NOVELTY IS THAT NO DOC COVERS THE RAIL'S HEIGHT BUDGET. `MENU.md` curates which slots exist and `LAYOUT.md` places the rail, but nothing says how many slots fit the smallest supported display, or what should give when they stop fitting — smaller slots, a scroll, a second column, or a cap on the curated set. The rail has grown 13 -> 14 twice in one day. The next addition needs a rule rather than another fix.

*Files: `src/ui/nav_pane.cpp`, `docs/ui/MENU.md`, `docs/ui/LAYOUT.md`, `scripts/verify/nav_rail_fit.lua`*

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

### NR-730 — No harness compiles src/ui or src/core, so a worktree agent cannot build what it changes
*observation · raised 2026-08-29 · from Sprint 24a, BL-692. The agent hit it and wrote a stopgap rather than skipping verification.*

A fresh git worktree has no configured `build/`, and `build_app.bat` requires one. Configuring from scratch risks a ~120 MB FetchContent download. So an agent working in a worktree **cannot build the GUI at all** — and no `tools/verify/*.cpp` harness compiles `src/ui/` or `src/core/` either, because the harness builder excludes anything reaching `<imgui.h>` by construction.

The consequence: **every UI-touching worktree agent this sprint reported "green" on harness runs that never compiled the code it changed.** They were not wrong to — the checks they ran are real — but the compile itself only happened when the main session merged and built. That has worked because the main session always builds; it is not a property anyone designed, and it fails silently the day someone trusts an agent's green.

The stopgap: `tools/verify/syntax_check_gui.bat` (committed with BL-692) runs `cl /Zs` over the GUI translation units against the shared `_deps_cache`. It proves COMPILE, not LINK. Better than nothing and explicitly not a build.

The question for Ben is which of three this wants to become: a committed helper that configures a worktree build against `_deps_cache` (the previous agent wrote `cfg_worktree.bat` for the same reason and deleted it before committing — that is twice now); a fallback folded into `build_app.bat` itself; or a rule that worktree agents do not verify GUI changes and the integrating session owns that step entirely. The third is the current de-facto answer and nothing says so.

*Files: `tools/verify/syntax_check_gui.bat`, `build_app.bat`, `tools/verify/build_harness.js`*

### NR-735 — History's catalogue entries were reconciled against the code, retiring UI-087 and minting two
*decision · raised 2026-08-30 · from Sprint 24b, the Ages params fix. The doc half of NR-713, taken rather than re-filed.*

NR-713 recorded that `ui_elements.json` carried UI-087 "Tiles view" for a view that no longer exists and had no entry for Tectonics. Reading it to act on it turned up a third gap nobody had named: there was no entry for the AGES view either, so two of History's four views were absent from the inventory the UI sprints scope off.

TAKEN ON YOUR BEHALF, all reversible and all doc-only:
  * UI-087 ("Tiles view") REMOVED, its number left vacant per the file's own stable-not-dense id rule (the vacant list now reads 031, 035, 036, 087).
  * UI-115 "Tectonics view" minted, carrying the `ledger_pass` check.
  * UI-116 "Ages view" minted, carrying `ages_replay` - which also clears it from the orphan list.
  * UI-084 renamed from "History Ledger (Story/Chain/Tiles)" to name the four views it actually has.
  * `ui_state.hpp`'s `history_view` comment, which read "2=Tiles, 3=Ages" against a Story/Chain/Ages/Tectonics enum - two errors in one line - now mirrors `history_view_id` and says it is the mirror rather than the source.
  * `tile_ledger.md` now describes four views, and documents Ages as replaying GENERATION's era.
    `ledgers/README.md`'s view row was ALREADY corrected by uncommitted work in the tree when this
    session opened - not mine, carried in this commit because leaving it out would have left two
    ledger docs disagreeing about how many views History has.

WHAT I DID NOT TAKE: `ui_coverage --orphans` still reports three unclaimed checks - `acquisitions_ledger`, `export_mockdata`, `pan_perf`. The first is Sprint 24a's and the other two are not ledger checks at all. Left alone rather than swept in under this item.

*Files: `docs/ui/ui_elements.json`, `docs/ui/ledgers/tile_ledger.md`, `docs/ui/ledgers/README.md`, `src/ui/ui_state.hpp`*

### NR-736 — History answers a generation question, and you want it answering a strategy one
*question · raised 2026-08-30 · from Sprint 24b. Your direction, 2026-08-30, given while the Ages fix was in hand.*

Recorded here as the pointer; the questions themselves are written into `docs/ui/ledgers/tile_ledger.md` - The direction this surface is pointed - which is where they can be designed against.

YOUR WORDS: "History at present tells the story of generation. Rather it should in fact answer questions about strategy that goes deeper into the meta-game" - which provinces are claimed by other nations, who your nation expects to be fighting soon, which resources are becoming depleted.

THE SHAPE WORTH NAMING, because it is what makes these one surface rather than three: all four existing views show STATE (a finished biography, a finished chain, a finished era, a finished plate map), and all three of your examples are a TREND or an EXPECTATION. That is why none of them can be read off what exists, and it is a sharper statement of the gap than "the infrastructure isn't there".

SIX QUESTIONS ARE OPEN IN THE DOC, of which two look load-bearing: whether the unit is the BODY or the NATION (every view today sits behind a body combo; all three of your examples are per-nation, so the seam runs through the middle of the ledger), and whether a claim - ground a nation asserts and does not hold - exists anywhere in the model. It does not.

NOTHING WAS BUILT AGAINST THIS, per your instruction to keep the ledger similar.

*Files: `docs/ui/ledgers/tile_ledger.md`*

### NR-741 — The lens/ledger scan: three pairs exist, not more, and LENSES.md's routing table is the test
*observation · raised 2026-08-30 · from Sprint 24b. Ben: "connect lenses with ledgers ... Scan to see if other pairs exist."*

SCANNED all twelve lenses against all thirteen rail slots. The scan needed no judgement in the end, because LENSES.md already carries the answer in the OTHER direction: its per-lens routing table names, for every lens, the ledger a click on it opens. A pair is real where both directions name each other.

THE THREE PAIRS:
  * Slot 6 Market <-> `market`. Ben's, 2026-08-30, and the routing table already sent Market, Scarcity AND Resource clicks to this ledger.
  * Slot 7 Convoys <-> `supply_routes`. Already built (BL-689).
  * Slot 10 History <-> `continent`. Real - `Continent` routes to "History ledger, at its tectonic record" - but NOT built, and deliberately. See NR-742.

WHAT IS NOT A PAIR, and why each is a finding rather than a gap:
  * `Corporation` and `Company` route to "that corporation's / that company's ledger" - per-entity surfaces reached through Selection, not rail slots. There is no slot to arm from. Note this contradicts corporation.md, which proposes arming `corporation` from rail slot 1 - written when slot 1 WAS a rivals table, which BL-691 turned into the player's own dashboard. The proposal outlived its surface.
  * `Population`, `Industry` and `Throughput` are all marked INERT in the routing table - they route nowhere at all.
  * Balance/Budget: balance.md argues `none` at length and I agree. Money has no map field. But LENSES.md § Which lenses carry a structure says the Corporation lens opens the BUDGET ledger, while the routing table forty lines later says "that corporation's ledger". Two tables in one doc disagreeing about one lens's destination - worth a look independently of arming.
  * Construction: its proposals were `opportunity` and `production`, BOTH RETIRED (BL-604), and `opportunity` is separately refused as a ledger pairing (your ruling, 2026-08-29). Nothing left to propose.
  * Research, Acquisitions, Diplomacy, Generation, AI decisions, Strategy: no lens routes to any of them.

ON THROUGHPUT SPECIFICALLY, since you expected it to match one: it does not, and the reason is structural rather than an oversight. Throughput is the surface half of LOGISTICS.md's Logistic Points, and NO LEDGER SURFACES LP - it appears only on the canvas and the overlay legend. Logistics is the road and Supply is the traffic; the Convoys ledger is the traffic's and already pairs with `supply_routes`. Throughput's twin would be a Logistics ledger, and there isn't one. That is arguably the finding: a lens exists for a quantity no ledger reads.

*Files: `docs/ui/LENSES.md`, `src/ui/nav_pane.cpp`, `docs/ui/ledgers/corporation.md`, `docs/ui/ledgers/balance.md`*

### NR-744 — Three purged AI items still hold dated standing-rule grants and have no owner
*observation · raised 2026-08-31 · from Sprint 26 opening; found while sizing the diplomatic half of the AI thread.*

The 2026-08-23 cull purged BL-450 (rivals score stance), BL-539 (rival lobbying), BL-540 (nation stance gates the player) and BL-334 (Stage C dialogue layer). None was completed - the cull's own note is explicit that nothing in it is open work and nothing in it is done.

THEIR GRANTS DID NOT GO WITH THEM. io-standing-rules.md still carries, dated and argued at length, Ben's 2026-08-22 permission for a rival to act politically against the player's corp (naming BL-539 and BL-540 by id), his same-date grant for a rival to score stance (naming BL-450), and AI_OPPONENT.md § 7 still names BL-334 as the item carrying Stage C.

SO THE STANDING RULES CITE FOUR ITEM IDS THAT NO LONGER EXIST as work anywhere. A reader following those citations lands in the purge archive, whose note correctly says nothing in it is open - which is true of the record and false of the intent.

BL-699 (rival stance scoring) resurrects the first, authored fresh. The other three remain unowned.

This is the same failure shape as NR-70x's FINANCE.md finding: a document describing something in the present tense that no code and no item backs. Here it is subtler, because the rules are not wrong - the permission genuinely stands. It is the ID CITATIONS that dangle.

**Why it matters.** The standing rules are the always-on file every session reads. An id in it that resolves only to a purge archive teaches the reader that its citations are unreliable, which is expensive for a file whose whole value is that it can be trusted without checking.

> **Recommendation:** Two options, Ben's call. (1) Mint owners for lobbying, nation-stance-gating and Stage C now, so the citations resolve - probably the sprint after 26, since all three sit above the margin term. (2) Or edit the standing rules to cite the RULINGS by date rather than by item id, which is what the state-independence rule would prefer anyway - a doc citing a BL- id as the owner of a design is fine, citing one that no longer exists is not.

*Files: `.claude/rules/io-standing-rules.md`, `docs/ai/AI_OPPONENT.md`, `docs/development/backlog.json`*

### NR-746 — Restraint moved out of the agent and into the world - the supersession record, and the one risk it carries
*decision · raised 2026-08-31 · from Ben, 2026-08-31, answering the margin-objective design form.*

Recorded because AI_OPPONENT.md § The goal was rewritten TWICE on 2026-08-31 and the second rewrite supersedes the first. A future reader finding both framings in the git history should be able to tell which one stands and why.

FIRST FRAMING (mine, from Ben's opening steer): the margin is a term in the scorer's objective. The AI plays at full skill but aims at 'stay narrowly ahead' instead of 'maximise'. Argued as honest on the grounds that nothing is taken from the AI - only the target moved.

BEN'S RULING, which supersedes it: "The angle we are taking here is a literal handicap. I prefer to consider it as always a force from within the game system. With 7 corporations, we have plenty of room for alliances to form against leaders, and we haven't built a critical system which is climate. Therefore, I prefer if an agent's decision to slow down can be framed as usually motivated by systems."

HE IS RIGHT AND THE FIRST FRAMING WAS WRONG. An objective term is invisible, uncausable and uncounterable: the player cannot see it, provoke it, benefit from it or fight it. Moving the target rather than lowering the skill is a real distinction, but it is a distinction the player never gets to observe - which makes it a handicap in every way that matters to the person playing. A coalition has a cause and a face.

BOTH MODES KEPT, at Ben's instruction - the systemic route as the foundation, the scorer dial demoted to an opt-in difficulty knob (BL-698, opt-in margin dial), off by default.

THE RISK THE NEW SHAPE CARRIES, and it is the reason this is a `decision` rather than an observation: an emergent brake CANNOT BE GUARANTEED. A margin term hits its number by construction; coalitions might not form, might form against the wrong corp, might form and not bite, or might over-bite and produce a dogpile where the leader collapses rather than being held. There is no dial that fixes that directly - the fix is always to the coalition scoring, which is the honest cost of preferring a system to a handicap. BL-697's (skill harness margin metric) spread band is what will tell us, and it is the reason that item is an instrument rather than a nicety.

SECOND RISK, smaller and worth naming: coalitions brake the leader, and the PLAYER is often the leader. The brake will therefore be aimed at the player a good deal of the time, which is correct and is the design working - but it must be visibly caused, or it will read as exactly the rubber-banding this reframe rejected. Hence the legibility requirement written into BL-699 as a requirement rather than a follow-up.

**Why it matters.** It is the central design decision of the AI thread, it reversed inside one session, and the reversal is the kind that a later reader would otherwise re-litigate from first principles. It also names the one thing the new shape cannot promise, which the old shape could.

> **Recommendation:** No action - this is a record, not a question. Read it with NR-743, which the reframe dissolved. The thing to actually watch for at the sprint retro is the dogpile failure mode: a leader that COLLAPSES rather than being held is the new design failing, and it will look superficially like success in a spread band.

*Files: `docs/ai/AI_OPPONENT.md`, `docs/development/sprints.json`, `src/world/corp_ai.cpp`*

### NR-747 — How much climate load is reversible, and where does the permanent floor sit? (next Era, not urgent)
*question · raised 2026-08-31 · from Ben, 2026-08-31: "Let's design climate docs here... how the hazard affects players." CLIMATE.md § 6 is written with a recommendation and marked as his call.*

Climate degrades under the strain of what corporations extract and process on a body. The question is what happens when the strain stops. It is the one open call in CLIMATE.md that changes the system's FEEL rather than its numbers, and every other open call reads differently depending on it.

REVERSIBLE - degrades under strain, recovers when strain falls. Climate is a live pressure and a standing negotiation between operators. COST: it becomes a thermostat. A cost to manage, never a stake, and it supplies no Era boundary at all - which loses one of the three jobs the system was admitted for.

RATCHETING - degrades only, never recovers. The countdown has teeth and the Era rupture is inevitable. COST: it removes agency. If the ending arrives regardless, the rational player ignores the meter and climate becomes scenery with a number on it. It also makes ERAS.md's own sentence false - see below.

FLOORED (recommended, and written into CLIMATE.md § 3 and § 10.1 as the recommendation) - recovery is real up to a threshold; past it a floor is set that never lifts, and each further breach raises it. The body can recover TO the floor and no further.

WHY FLOORED, and it is not a split-the-difference argument. ERAS.md already commits to this sentence about the Era 0 exit: "The backstory establishes that these powers CAN pull back from the brink; the Era 0 exit is the occasion they do not." Under a ratchet nobody could ever have pulled back, so that sentence is a lie. Under pure reversibility nobody ever needed to, so the rupture is unmotivated. Only a floor makes pulling back genuinely possible, genuinely costly, and genuinely something a field of competing corporations may fail to coordinate on. That is a tragedy of the commons rather than a scripted apocalypse - and it makes the Era boundary a CONSEQUENCE OF PLAY rather than a date on the clock.

IT ALSO DECIDES WHETHER CLIMATE IS A MARKET. If recovery exists, something a corporation BUILDS could accelerate it, and remediation becomes a business - which is CLIMATE.md § 10's third open call and is only live under reversible or floored.

WHAT IT DOES NOT DECIDE: the constraint that climate never vetoes construction or extension is Ben's 2026-08-31 ruling and holds under all three shapes.

UPDATED 2026-08-31 after Ben named the rupture as a NUCLEAR WAR. That does not answer this question, it SHARPENS it - the war supplies a permanent floor by itself, so the live question is now specifically about the Era 0 DRIFT: how much of the pre-war industrial loading is recoverable if the field pulls back. A fully reversible drift means the war is the only thing that ever leaves a mark, and the commons stops being a stake during the era the player actually plays.

RETARGETED 2026-08-31, THIRD TIME, and DEMOTED. Ben placed climate in the NEXT Era: "I am not against climate change being a large problem for Era 2, but our prototype works solely on Era 1 for now." So this question no longer gates anything in the prototype - the war framing that made it urgent is gone, because the war is this Era's catastrophe-to-avoid rather than a scheduled event leaving a climate behind. Keep it open as a design note against CLIMATE.md § 8.2; do not treat it as blocking.

**Why it matters.** It decides whether climate is a pressure, a doom clock, or a commons - and therefore whether the Era 0 boundary is something the world's corporations did or something the calendar did. The build should not be minted before it is answered; building against an unsettled curve would be implementing the wrong feel efficiently.

- Floored - reversible up to a threshold, then a permanent floor that each breach raises. RECOMMENDED, and already written into CLIMATE.md § 6 as such.
- Reversible - a pure pressure. Simplest, and the safest if climate turns out to be annoying rather than interesting; loses the Era boundary job.
- Ratcheting - a pure doom clock. Strongest era arc, weakest agency; also contradicts ERAS.md's existing backstory sentence.

> **Recommendation:** Leave open and unhurried. Recommended shape unchanged - recoverable up to a threshold, then a floor each further breach raises - because a load that fully recovers means nothing a corporation does before the test matters. Answer it when the climate Era is actually designed, not now.

*Files: `docs/CLIMATE.md`, `docs/economy/ERAS.md`, `docs/CONCEPT.md`*

### NR-750 — The Era catastrophe model was promoted from a research doc into ERAS.md, and its owner BL-087 is purged
*observation · raised 2026-08-31 · from Sprint 26 doc correction, 2026-08-31.*

ERAS.md § The point of an Era is NEW and it is a PROMOTION, not an authoring. The Ceiling/Alarm scalars, the test, the seven red-herring kinds, the tell-before-commitment rule and the inverse herring were all drafted 2026-08-05 in docs/research/ERA1_TECH_LANDSCAPE.md on Ben's steer, and have been sitting there since.

THAT DOC IS EXPLICITLY NOT AUTHORITY. CLAUDE.md lists docs/research/*.md as "research scaffolding. Not authority." So the design defining what an Era IS - the thing CONCEPT.md calls the gear shift and the thing every quest tree hangs off - was in a file no session is required to read and no doc is required to agree with. ERAS.md even said so at the site: the old text noted the reframe "lives here until work lands" and that ERAS.md was "deliberately not edited yet".

IT IS NOW IN ERAS.md, which is where a reader looking for the Era model would go.

THE OWNER IS GONE. BL-087 (era tech/quest system) carried this and was PURGED in the 2026-08-23 cull - not completed. So the model now has authority and no work item, and the drafted Era tree (five sectors x three rings, ~45 objects, four keystones each opened by a DEED) plus the `deed` condition primitive (NR-067) and the Alarm scalar (NR-068) are all unowned.

SAME SHAPE AS NR-744, which found three other purged AI items whose dated grants still stand. This is the fourth instance of one pattern: the cull removed work items whose DESIGNS remained load-bearing.

**Why it matters.** Two live consequences. The catastrophe model is now authority, so anything contradicting it is a defect rather than a difference of drafts - which is the point of promoting it. And the tech/quest system that implements it has no owner, so the moment the era work is scheduled there is nothing to schedule.

> **Recommendation:** Mint a fresh owner for the era tech/quest system when the era work is actually next, authored against the current docs per the cull's resurrection rule - not now, since sprint 26 is about watching the AI. Worth doing at the same time as NR-744's three, since all four are the same cull and the same shape.

*Files: `docs/economy/ERAS.md`, `docs/research/ERA1_TECH_LANDSCAPE.md`, `docs/development/backlog.json`*

### NR-751 — The Era renumber leaves the F9 tech-tree viewer labelling its tabs by the old numbers
*observation · raised 2026-08-31 · from The 2026-08-31 Era renumber (NR-749).*

The ladder renumbered 1-based in the docs. The F9 mock tech-tree viewer's TAB LABELS are authored strings in scripts/tech_tree.lua and its viewer, and they still read 'Era -1 Antiquity' / 'Era 0 - Terrestrial' / 'Era 1 - Early Space' / 'Standing lines'.

docs/ai/ACTIONS.md records those labels in three places (the F9 entry, its expected output, and the tab-button press with its enum). ACTIONS.md was DELIBERATELY NOT SWEPT with the rest of the docs, and that was the right call: it is a mirror of what the control actually says, so editing it to match the new numbering would have made the dictionary lie about the UI. A dictionary that describes a press must track the code, not the design.

So the divergence is real, narrow, and currently HONEST - the docs say Era 1 Terrestrial, the viewer says Era 0 Terrestrial, and ACTIONS.md correctly reports the viewer.

CONTEXT THAT LOWERS THE URGENCY: the viewer is a MOCK. ACTIONS.md says so itself - "a design aid with no simulation coupling; nothing can be researched and nothing in the world reads it". No simulation behaviour depends on these strings.

**Why it matters.** It is small now and gets worse the moment anyone authors real tech content against the viewer's numbering, because the authored ids and the doc ladder would then disagree structurally rather than cosmetically.

> **Recommendation:** Fix the labels when the tech/quest system gets a real owner (NR-750), not before - it is a string change that wants to happen in the same pass as the E0-/E1- id question, and doing it alone would just move the divergence from the labels to the ids. Regenerate ACTIONS.md from the code at that point rather than hand-editing it.

*Files: `scripts/tech_tree.lua`, `docs/ai/ACTIONS.json`, `docs/ai/ACTIONS.md`, `docs/economy/ERAS.md`*

### NR-752 — spectator_determinism's byte-identity golden is stale by 16 world-changing commits - re-bless is your call
*question · raised 2026-08-31 · from Sprint 26 wave 1. Found by the slice-A agent, verified independently by the main session.*

`spectator_determinism` fails one row on main, and has been failing it since before this sprint:

  [FAIL] R2 byte-identity: the unspectated hash equals the pre-BL-409 golden
     golden=E350DF2A50BF4BAA  observed=8274DFA6251C116E

VERIFIED INDEPENDENTLY, not taken on the agent's word, and the verification caught something worth knowing on its own. The prebuilt exe under build_gen/ PASSES with E350DF2A50BF4BAA - which is why nobody noticed. It is stale: compiled from an older tree and still runnable. A freshly compiled harness on clean main, pinned MSVC 14.44, produces 8274DFA6251C116E, exactly matching what the agent got from its own build. So the agent's Ninja configuration was NOT the cause and the drift is real.

A STALE PREBUILT HARNESS THAT PASSES IS WORSE THAN ONE THAT FAILS. It reports on a world that no longer exists, in the affirmative. Worth remembering the next time a verify exe is run without being rebuilt first.

THE DRIFT IS EXPECTED AND ATTRIBUTABLE. The golden was last blessed at 1e43d696 (2026-08-26). Since then main landed a run of world-changing commits, several of which move the state hash by construction: the mercenary-contract tear-out, opening capital 0 -> 400, debt interest 2% -> 1.5%, no standing army at spawn, the era-banded household basket, ownership-closure retirement.

THE TWO PROPERTIES THE STANDING RULE NAMES BOTH PASS, and they are the invariant:
  [PASS] R1 unspectated the player corp is NEVER due, on any tick (the prohibition)
  [PASS] R1 admitting the player shifts NO rival's cadence slot
The rule's own words are that a state_hash constant is dated evidence, not the invariant itself.

NEITHER THE AGENT NOR I RE-BLESSED IT. The NR-596 precedent is explicit: a golden re-blessed by whoever happens to trip over it is a golden nobody reviewed. This one wants a deliberate re-bless with dated provenance in the harness's own provenance log, which already records two prior instances of exactly this.

**Why it matters.** It is red now and will stay red through this sprint's every harness run, which trains everyone to read a FAIL as background noise - the precise condition under which a real regression gets waved through.

- Re-bless to 8274DFA6251C116E now, with a dated provenance entry naming the commits that moved it. Clears the noise before the sprint's own changes land.
- Re-bless at the END of sprint 26, once the wave-2 corp_ai changes have moved it again - one bless instead of two.
- Retire the byte-identity row. It asserts world-content stability, which is not what this harness is for; the two properties above are.

> **Recommendation:** Option 2 - re-bless once at the sprint close. This sprint changes corp_ai (BL-700, BL-699), which will move the hash again, so blessing now buys a few days of green and a second bless. Option 3 is worth a thought at the same moment: the row keeps catching world-content drift that this harness never claimed to guard, which is the NR-596 argument one step further.

*Files: `tools/verify/spectator_determinism.cpp`, `.claude/rules/io-standing-rules.md`*

### NR-753 — Worktrees are cut from origin/main, which has diverged from local main - the stale-base trap, structurally
*observation · raised 2026-08-31 · from Sprint 26 wave 1, reported by the slice-A agent and confirmed by the main session.*

Every worktree this session is created at `origin/main` = e60cc726, a merge of PR #57 (sprint-24b-ledgers). Local `main` is 8 commits ahead and e60cc726 is NOT one of its ancestors, so the two have genuinely diverged and a fast-forward is impossible.

CHECKED, AND NOTHING IS AT RISK: e60cc726 is the ONLY commit reachable from origin/main and not from local main, and it is a merge whose second parent (9a5537bd) was local main's tip at session start. Its other parent 442e999b is already an ancestor of local main. So origin's PR merge introduces no content local main lacks - local is simply ahead, and unpushed.

THE OPERATIONAL PROBLEM IS REAL THOUGH. DELIVERY.md's rule is that every agent verifies its own base as its FIRST action and fast-forwards to the working branch tip. Here that instruction cannot be followed literally - the fast-forward is impossible by construction - so an agent that follows the rule exactly either stops, or (as slice A correctly did) branches fresh from local main and reports the discrepancy.

This is the third recorded firing of the stale-base trap (NR-459, NR-480 are the first two), and the first where the cause is a DIVERGED REMOTE rather than a lagging worktree. The existing brief wording assumes the base is behind; it does not cover the base being sideways.

Slice A handled it correctly and without being told how, which is the discipline working.

**Why it matters.** Every agent spawned this sprint hits it. One that resolves it by hard-resetting to origin/main would silently discard eight commits of local work - which is the same class of loss as the 2026-08-23 incident where another session's commit path wiped uncommitted work.

> **Recommendation:** Two things. Short term: push local main so origin and local reconverge, which makes the whole problem disappear for the rest of the sprint - that is Ben's call since git push is in the deny net. Longer term: amend the standing sub-agent brief wording from 'fast-forward to the tip' to 'fast-forward if possible; if the bases have DIVERGED, branch fresh from the named base commit and report' - the instruction currently has no branch for sideways.

*Files: `docs/development/DELIVERY.md`, `.claude/rules/io-standing-rules.md`*

### NR-754 — No doc owns HOW a session-scoped mode is entered, and --spectate just set the precedent
*novel-work · raised 2026-08-31 · from Sprint 26 wave 1, novelty flagged by the slice-A agent.*

AI_OPPONENT.md § 10i owns what spectator mode IS. TIME_CONTROLS.md owns the time panel. STARTUP.md owns the screen state machine. None of them owns launch-flag policy - whether a mode is entered at start or toggled live, and what that choice implies.

The call was made on the argument supplied in the brief: ENTRY-AT-START ONLY, because BL-409's grant is a property of the SESSION (nobody seated), and flipping it mid-run changes who the scorer may legally act on halfway through - a separate argument nobody has made. The agent wrote the reasoning into § 10i so the next session inherits a settled answer rather than re-deciding.

IT IS A PRECEDENT, which is why it is flagged rather than just done. Future session-scoped modes - a replay mode, a headless observer, whatever the MCP seam grows - will each face the same question, and there is now one answer in the tree with no doc claiming ownership of the general rule.

ADJACENT AND WORTH KNOWING: the agent found that `app.cpp:1111` passes `m_ui.spectating || m_warm_starting`, so the warm start ALREADY runs under spectate unconditionally (BL-630's spawn shortlist). --spectate therefore only changes behaviour after seating. That is correct, and it means the flag's blast radius is smaller than it looks.

**Why it matters.** Novelty should be chosen rather than accreted. A launch-flag policy set once inside an AI doc is exactly how a general rule ends up owned by a specific feature.

> **Recommendation:** No action this sprint. If a second session-scoped mode appears, that is the moment to lift the rule out of § 10i into STARTUP.md, which owns how a session begins. Not worth a doc move for one instance.

*Files: `docs/ai/AI_OPPONENT.md`, `docs/ui/STARTUP.md`*

### NR-755 — A fresh worktree cannot run build_app.bat - no configured build/ directory
*observation · raised 2026-08-31 · from Sprint 26 wave 1, reported by the slice-A agent.*

`build_app.bat` hard-fails in a fresh worktree with "no configured build" - `build/` is gitignored, so a new worktree has none and the script does not cold-configure one.

The agent worked around it by cold-configuring with Ninja -j 12 instead of the script's serial nmake, same pinned VS2022 BuildTools toolchain, same Debug config. 420 targets green.

THAT WORKAROUND IS NOT FREE AND IT COST REAL TIME HERE. A different generator can produce different codegen, which is exactly the doubt that made the spectator_determinism result (NR-752) need independent re-verification from the main session. The answer came out the same, but the verification only happened because the toolchain differed and I did not want to trust it.

The sibling problem in build_harness.js was FIXED at source this session (commit b7389013 - the sol2 include path). This one is not fixed: it needs either a cold-configure path in build_app.bat, or a documented worktree bootstrap step in the standing sub-agent brief.

**Why it matters.** Every worktree agent this sprint pays a cold-configure cost and may reach for a different generator to do it - and a differing toolchain undermines exactly the determinism results this sprint's harnesses exist to produce.

> **Recommendation:** Cheapest useful fix: have build_app.bat cold-configure when build/ is absent, using the same generator it always uses. Failing that, add the bootstrap command to the saved agent definitions so every agent uses the SAME toolchain rather than improvising one. Worth doing before wave 2 if it is quick.

*Files: `build_app.bat`, `.claude/agents/ui-dev.md`, `.claude/agents/economy-dev.md`, `.claude/agents/generation-dev.md`*

### NR-756 — campaign_epoch_year was one name over two quantities; the static_assert was retired rather than replaced
*decision taken on your behalf · raised 2026-08-31 · from Sprint 26 wave 1, BL-705. Found by the slice-B agent, verified at the save seam by the main session.*

The brief said "replace the static_assert with whatever keeps the two clocks honest once the value is dynamic". The agent concluded there is NOTHING LEFT TO GUARD, which is a stronger claim than the brief invited, and it is correct.

THE TWO QUANTITIES, which shared a name and a static_assert:
  - `::history_datum_year` (was `::campaign_epoch_year`, planetology.hpp) - the FIXED datum `history_event::years_before_epoch` counts back from. It is SERIALISED: save_game.cpp:127 writes years_before_epoch with w_i64. So it must stay constexpr - make it dynamic and a save reinterprets its own recorded history when loaded into a campaign with a different epoch.
  - `ui::fmt::campaign_epoch_year()` - the year the LIVE campaign opens, published from world_params at the two sites that assign m_active_world_params. Display only; never enters `world`, never reaches the save seam.

Welding them with a static_assert is precisely what pinned the tick calendar at 1960 while every default campaign generated at 0 CE. One name over two quantities is what kept it wrong, and no amount of making 'the value' dynamic would have fixed it while both readings shared the name.

I VERIFIED THE SERIALISATION CLAIM MYSELF at save_game.cpp before accepting a static_assert removal - that is the class of change that is cheap to wave through and expensive to be wrong about.

WHAT WAS GIVEN UP: nothing measurable. format_history_date renders recorded history as an absolute calendar year (history_datum_year - y), correct at any epoch, and its >= 10 kyr branch is the geological before-present convention whose datum is fixed by design. planetology_harness's R14 assertion changed LABEL only ('the campaign epoch is year zero' -> 'the history datum is year zero'), not value.

**Why it matters.** A retired check is a check nobody re-derives later. This entry is the argument for why the guard was wrong rather than merely inconvenient, so a future reader finding an unguarded pair does not reinstate it.

> **Recommendation:** No action. Recorded as the reasoning behind a removal. If anything, the lesson generalises: a static_assert between two values is only meaningful if they are the same QUANTITY, and this one asserted an identity between a display value and a serialisation datum that were never required to agree.

*Files: `src/world/planetology.hpp`, `src/ui/format.hpp`, `src/ui/format.cpp`, `src/core/save_game.cpp`*

### NR-757 — There is no scripted capture of a 1960s world - --epoch deliberately does not reach --verify
*observation · raised 2026-08-31 · from Sprint 26 wave 1, BL-705, flagged by the slice-B agent and confirmed at merge.*

--epoch is parsed BELOW the --verify / --verify-all / --serve dispatch, so those three never see it. That is deliberate and correct as far as it goes: every golden and every pinned band in the verify suite is taken against the default world, and giving --verify a second epoch means a second golden set.

THE CONSEQUENCE IS REAL THOUGH: there is currently NO WAY to take a scripted visual capture of a 1960s world. Every scripts/verify/*.lua check runs at 0 CE.

IT LANDS ON BL-703 (watch session finding), which is this sprint's actual deliverable. That item watches a 1960s world through the live app, and it now has no scripted capture available as a fallback or as evidence - the finding will rest on live observation and on whatever BL-704's trace export produces.

Worth knowing BEFORE that item starts rather than discovering it inside it.

**Why it matters.** The sprint's stated goal is to WATCH the AI play on the 1960s start. Half the project's verification culture - the visual tier - cannot see that world at all, so any regression there is invisible to the automated suite.

- Leave it. The 1960s start is new and unproven; a second golden set for a world still being shaped would be re-blessed constantly.
- Wire --epoch to --verify and bless a SMALL 1960s capture set - just the surfaces BL-703 needs to read (decision feed, time panel, a body canvas), not the whole suite.
- Wire it and take captures WITHOUT blessing them - capture-and-look, no goldens. Gets evidence into the sprint without committing to maintaining a second set.

> **Recommendation:** Option 3 for this sprint. It gets BL-703 real artifacts to point at with no golden-maintenance cost, and it defers the option-2 question until the 1960s world has stopped moving. Option 1 is defensible if BL-703's live watch turns out to be enough on its own.

*Files: `src/main.cpp`, `scripts/verify/`, `docs/development/DEVELOPMENT_PRACTICES.md`*

### NR-758 — AI corps end most runs deeply insolvent - CORRECTED 2026-08-31, the original figures came from a world the game never produces
*question · raised 2026-08-31 · from Sprint 26 wave 1 baseline. Measured by the main session with ai_skill_harness and demand_census, 2026-08-31.*

THE OBSERVATION, from ai_skill_harness on the merged wave-1 tree. Five seeds, thirty econ ticks:

  seed 0: net_worth final=-624100.1   min=-624100.1   solvency_below_zero=29/30  survival=0.71
  seed 1: net_worth final=-997700.4   min=-997700.4   solvency_below_zero=30/30  survival=0.86
  seed 2: net_worth final=-694799.6   min=-694799.6   solvency_below_zero=29/30  survival=0.86
  seed 3: net_worth final=-1234353.5  min=-1234353.5  solvency_below_zero=30/30  survival=0.86
  seed 4: net_worth final=-1090065.2  min=-1090065.2  solvency_below_zero=29/30  survival=1.00

`final == min` on EVERY seed: net worth declines monotonically and never recovers. The rivals are not competing, they are collapsing, and they do it identically on all five seeds. Survival stays high (0.71-1.00) because survival counts buildings still standing, not solvency.

NOT CAUSED BY THIS BATCH, confirmed from both ends: the slice-C agent stashed its own corp_ai change and got the same figures to the digit, and an independent baseline run on the merged tree WITHOUT slice C reproduced the same 21 band failures.

THE CAUSE, measured with demand_census on the INDUSTRIAL band (epoch 1960 - the band this sprint's 1960s start uses):

  iron_ore      produced 42991.6   demand 0.000 across every channel
  petroleum     produced  6169.9   demand 0.000
  copper_ore    produced  2213.5   demand 122.4 (processing inputs only)
  timber        produced  1224.2   demand 0.000

Only THREE resources carry any household demand at all: agricultural_produce (109.6), water (230.7), food_rations (244.5). Everything else is bought by nobody, or bought only as an input to make something else nobody buys.

FIVE OF THE EIGHT DESIGNED DEMAND CHANNELS DO NOT EXIST (MARKETS.md § Demand channels; demand_census R1a names each absent one and its unbuilt owner):
  Infrastructure - ABSENT (BL-643). No material draw for roads/ports/hubs anywhere in src/world.
  State          - ABSENT (BL-644). Nation budget lines spend credits; no goods purchase exists.
  Research       - ABSENT (BL-645). research_institute credits science; nothing draws goods.
  Conflict       - ABSENT (BL-646). Battle resolution consumes no goods.
  Endemic trade  - ABSENT (BL-647). Nothing wants tobacco, spices, coffee or furs.

And of the three that DO exist, Industry - the largest structural sink - ships its rates at ZERO: 'industry: 156 of 317 buildings eligible to draw upkeep; 0 of those carry an authored basket in this band'.

SO THE AI IS NOT PLAYING BADLY. It is playing a game with no demand side. A corp digs 43,000 units of iron ore, pays wages to do it, and there is no buyer at any price. Monotonic bankruptcy is the arithmetically inevitable result, and no scorer improvement can change it.

SPRINT 21 ALREADY KNOWS THIS and says so in its own goal: 'the ancient economy terminates in artisan goods nobody buys and most spawns are structurally unprofitable - not mistuned, UNBOUGHT'. It is PAUSED at wave 0, with the UI batches having taken priority.

=== CORRECTION, 2026-08-31, SAME DAY. THE ORIGINAL FIGURES WERE MEASURED ON A WORLD THE GAME NEVER PRODUCES. ===

Found by the BL-707 slice and verified in the main session: `init_survey_states` is called from `src/core/app.cpp` at campaign start and from `survey_harness.cpp`, and FROM NOWHERE ELSE. `make_hard_coded_world` does not call it. So every harness that builds a world through it - ai_skill_harness included - ran with EVERY BODY `hidden`, home included, and `rank_extraction_sites` returns an empty candidate list on a hidden body.

Re-measured with `init_survey_states(w)` added to ai_skill_harness, five seeds, thirty ticks:

  seed   BEFORE (hidden world)              AFTER (the app's world)
  0      -624100.1  29/30  surv 0.71        -865882.7  29/30  surv 0.86
  1      -997700.4  30/30  surv 0.86        -421331.0  27/30  surv 1.00
  2      -694799.6  29/30  surv 0.86       -1064162.9  27/30  surv 1.00
  3     -1234353.5  30/30  surv 0.86        -153923.5  28/30  surv 1.00
  4     -1090065.2  29/30  surv 1.00        +60873.0  29/30  surv 1.00   <-- SOLVENT

WHAT CHANGES. Seed 4 is now POSITIVE and its min is +5083.1 - net worth never goes below zero and it RECOVERS, so `final == min` is no longer universal. Seed 3 improved eight-fold. Survival is 1.00 on four of five seeds. The world the app produces is materially healthier than the world the benchmark measured.

WHAT SURVIVES, and it is still the sprint's premise. Four of five seeds end deeply negative, every seed is insolvent 27-29 ticks of 30, and the demand_census finding is untouched: iron_ore produced 42991.6 against total demand 0.000 on the industrial band, five of eight channels absent, Industry shipping at zero. Demand is still the problem and no scorer change moves it.

WHAT I GOT WRONG, plainly: I wrote 'every AI corp goes bankrupt on every seed' and built sprint 26's close-out and sprint 27's premise on it. The DIRECTION was right and the MAGNITUDE was overstated - one seed in five is solvent on the real world. The decision to close sprint 26 at wave 1 still holds on the surviving evidence (a coalition brake tuned against a field that is insolvent 28 ticks in 30 would still be tuned against noise), but the sentence 'every corp on every seed' should not be repeated.

The band failure count moved 21 -> 25 with the fix, which is expected: the bands were already stale by 16 world-changing commits (NR-752) and this adds a real behavioural change on top.

**Why it matters.** It decides what the rest of sprint 26 is worth. BL-699's coalitions form against 'whoever leads' - among corps at -624k to -1.2M the leader is the least bankrupt, so the brake has nothing meaningful to bite on and tuning it would be tuning against noise. BL-697's spread band would measure the distance between failures. And BL-703, the sprint's actual deliverable, would watch seven corporations go broke and call it the meta.

- BUILD wave 2's mechanisms, do not tune or bless them. BL-699 lands as scored stance with its determinism and legibility rows green; BL-697 lands the spread metric with its band deliberately unblessed and a note saying why. BL-703 then reports on a bankrupt field, which is itself the most useful finding available.
- PAUSE sprint 26 here and unpause sprint 21 (demand). The instruments now work - spectate, the feed, the standing index, the 1960s start - which was the sprint's stated goal. Resume the AI work against an economy where standing means something.
- Do both in sequence: close sprint 26 at wave 1 as an instruments sprint, take demand next, and re-open the coalition work after.

> **Recommendation:** Unchanged in direction: sprint 27 takes demand. But re-run this measurement after BL-654 lands rather than quoting the pre-correction figures, and treat NR-762 as the reason to distrust any harness-derived economic number taken before today.

*Files: `tools/verify/ai_skill_harness.cpp`, `tools/verify/demand_census.cpp`, `docs/economy/MARKETS.md`, `docs/development/sprints.json`*

### NR-759 — Four build-wiring breaks in one session, none caught by any test, two of them masked by stale binaries that PASS
*observation · raised 2026-08-31 · from Sprint 26 batch delivery, 2026-08-31.*

Four separate pieces of build wiring were broken on main when this batch opened. None was caused by this sprint, none was caught by any test, and the sprint spent a material share of its time on them before reaching its own work.

1. CMAKELISTS - main had not CONFIGURED since cc88997c (eight commits). The mercenary-contract tear-out deleted src/world/contract_template.cpp and tools/verify/contract_dispatch_harness.cpp and left three references. CMake's generate step failed before a single TU compiled. Fixed: ef19dab7.

2. build_harness.js - the ENTIRE verifier-headless tier could not compile. A src/world TU now reaches scripting/lua_state.hpp -> <sol/sol.hpp>, and the script passed only -I src -I tools/verify. Fixed: b7389013.

3. build_app.bat - hard-failed in any fresh worktree (build/ is gitignored and it did not cold-configure). Every worktree agent improvised its own configure, and one reached for a DIFFERENT GENERATOR, which is what forced an independent re-verification of a state_hash result. Fixed: 2eb15030.

4. demand_census - has never linked. Glob-declared against io_world_obj alone while calling lua_state, recipe_registry::load_from_lua and world_gen_config::load_from_lua. Six unresolved externals. Fixed by hand-declaring it beside its three siblings.

TWO OF THE FOUR WERE MASKED BY STALE BINARIES THAT PASS, which is the part worth the entry. build_gen/ holds prebuilt harness exes compiled from older trees. spectator_determinism's prebuilt exe PASSES its byte-identity golden; a fresh build of the same source FAILS it. A stale harness that passes is strictly worse than one that fails - it reports, in the affirmative, on a world that no longer exists.

CMAKELISTS ALREADY KNOWS THIS FAILURE MODE and says so in its own comment about the sibling include-path rot: 'An existing build tree kept passing on stale objects, which is why it survived a release cut.' The same sentence would serve for two of the four above.

THE COMMON SHAPE: the things that CHECK the code are themselves unchecked. Every one of these is a build-wiring fact that only surfaces when someone asks for that exact target from a clean state, and nothing in the project ever does.

**Why it matters.** A release was cut over at least one of these. Two of them make green results untrustworthy rather than merely absent, which is the more expensive failure - a session can read a stale PASS and conclude something false about the world.

- A clean-tree CI target: configure from scratch, build every harness target by name, run them. Catches all four classes and nothing else does.
- Cheaper stopgap: have build_harness.js and the verify skills REFUSE to run a prebuilt exe older than its newest source input, so a stale binary reports staleness instead of passing.
- Leave it and fix instances as they appear, which is what has happened so far.

> **Recommendation:** Option 2 first - it is small, it targets the specific thing that makes these dangerous rather than merely annoying, and it would have caught the spectator_determinism mask immediately. Option 1 is the real answer but is a CI question and wants Ben's call on where it runs. Filing rather than building either, since both are outside this batch's scope.

*Files: `CMakeLists.txt`, `tools/verify/build_harness.js`, `build_app.bat`, `.claude/skills/verifier-headless/SKILL.md`*

### NR-762 — About thirty harnesses build a world the app never produces - nothing owns which app-start passes a harness must replicate
*novel-work · raised 2026-08-31 · from Found by the BL-707 slice agent, 2026-08-31; verified and quantified in the main session.*

`init_survey_states` is called from `src/core/app.cpp:1341` at campaign start, and from `tools/verify/survey_harness.cpp`. NOWHERE ELSE. `make_hard_coded_world` does not call it.

So every harness building a world through `make_hard_coded_world` and not calling it by hand runs with EVERY BODY `hidden` - home included. `rank_extraction_sites` returns an empty candidate list on a hidden body, so no rival can site a mine anywhere, for any resource.

COUNTED: about thirty harnesses. acquisition_viability, ai_skill_harness, build_spree_harness, convoy_cargo_census, corp_terrain_matrix, debt_decomposition, demand_census, determinism_harness, era_world_harness, haulage_measure, material_floor, ownership_class, population_demand_harness, pregame_balance_harness, province_capacity_probe and more.

MEASURED CONSEQUENCE on ai_skill_harness alone (see NR-758): with survey initialised, one of five seeds flips from -1090065 to +60873 and never goes below zero, another improves eight-fold, and survival reaches 1.00 on four of five. The chain_conversion_probe shows iron_ore extraction sites going 30 -> 124 with survey on, against 30 -> 30 without.

THIS IS THE SECOND INSTANCE OF THE CLASS IN THIS ONE FILE. ai_skill_harness.cpp already carries a comment about the first: the default-recipe pass, which 'this harness never ran, so every GENERATED processor in the benchmark carried no_recipe for all 300 ticks - paying maintenance, never producing, and reporting as ordinary idleness.' Same shape, same file, found months apart.

THE NOVELTY, and why it is filed as one: NO DOC OWNS the question 'which app-start passes must a world-building harness replicate'. `make_hard_coded_world` builds a world; `app::start_new_game` then runs a tail of passes on it; and the boundary between them is defined by nothing except which passes somebody remembered. Every harness re-answers it independently, silently, and wrongly by default.

THE DANGEROUS PROPERTY is that the failure is INVISIBLE AND GREEN. A harness measuring the wrong world does not fail - it reports confidently on a world that does not exist, and its goldens then encode that world. This is the same class as the stale-prebuilt-exe problem (NR-759) and has the same cost: a green result nobody can trust.

COUNTED EXACTLY 2026-09-01, replacing the 'about thirty' estimate — and it is worse, with one qualification that makes it more actionable rather than less.

**48 of the 51** harnesses that build a world through `make_hard_coded_world` never call `init_survey_states`. Only three do: `ai_skill_harness`, `survey_harness` and `chain_conversion_probe` (the last two being the ones that had to).

**Of those 48, 22 also run the economy tick**, so the corp AI is live in a world where no body is surveyed and no rival can site a mine anywhere: acquisition_viability, cadence_schedule, convoy_cargo_census, data_creep_harness, debt_decomposition, decision_trace_harness, haulage_measure, history_log_harness, interbody_pull_harness, material_floor, nation_wiring, player_seed_sweep, population_demand_harness, population_mvp, pregame_balance_harness, quarterly_return, spawn_solvency, spectator_determinism, tech_effect_union_harness, tick_profile, tier_margin, whole_firm_buyout. The other 26 do not run the AI, so the pass does not bite them TODAY — which is a reprieve, not a fix, since any of them could grow an economy tick.

That 22 is the list that matters, and it contains most of this project's economic instruments — the haulage baseline, the bankruptcy figure, spawn solvency, the debt decomposition, the tier margins, acquisition viability. `demand_census` was the 23rd until 2026-09-01 (NR-772).

THIRD INSTANCE OF THE CLASS, and the first one that cost a wrong conclusion inside a live sprint: sprint 27 ran demand_census before and after every item as its stated method, and for every item touching what the AI BUILDS the instrument could not see the work. BL-711 came back byte-identical there while the probe showed coal going 0 -> 25.

**Why it matters.** Every economic conclusion this project has drawn from a harness may be measuring a world with no mines. That includes BL-634's acquisition viability, BL-635's spawn diagnosis, the haulage baseline, and the bankruptcy figure this sprint's whole premise rested on - which moved materially when corrected.

- A shared `harness_world()` helper that runs make_hard_coded_world PLUS every app-start tail pass, which every harness uses instead of calling make_hard_coded_world directly. One definition, one place to fix the next time a pass is added.
- Fix ai_skill_harness and demand_census now (the two this sprint depends on) and file the rest as a sweep.
- Audit first: run the ~30 harnesses with and without and report which ones actually move, before changing any.

> **Recommendation:** Option 1 (a shared world-setup helper in harness_params.hpp, the shape `no_prehistory()` already establishes) is still the real answer, and the exact list above makes it schedulable rather than open-ended: 22 harnesses need the pass and will move their numbers, 26 need it eventually and will not move today. Two sessions running have now paid for this gap — and the failure mode is the worst available, since a harness measuring the wrong world reports confidently rather than going red. Recommend taking it as its own item BEFORE the remaining sprint-27 channels, so the channels are measured against instruments that work; the 26 quiet ones can follow in the same item at no extra risk.

*Files: `tools/verify/harness_params.hpp`, `tools/verify/ai_skill_harness.cpp`, `tools/verify/demand_census.cpp`, `src/core/app.cpp`, `src/world/hard_coded_world.cpp`*

### NR-763 — Self-sufficiency is the NORM, not the exception - chain closure saturates and destroys the endowment asymmetry generation produces
*question · raised 2026-08-31 · from BL-706 (chain completeness read), first run, 2026-08-31. Measured by the slice agent, verified independently in the main session.*

The instrument worked on its first run and its first finding contradicts the design assumption it was built to test.

MEASURED - fraction of the band's terminal chains a market can source within reach:

  ANCIENT (14 markets, 15 terminal goods)
    min 0.400  p25 0.867  median 0.933  p75 0.933  max 1.000   sd 0.186
    [0.4,0.5)  2 markets
    [0.8,0.9)  3 markets
    [0.9,1.0]  9 markets     <- and NOTHING between 0.5 and 0.8

  INDUSTRIAL (9 markets, 11 terminal goods)
    min 0.636  median 1.000  max 1.000   sd 0.134
    SEVEN OF NINE markets close EVERY chain in the band.

MARKETS.md property 5 expects self-sufficiency to be "possible, uncommon, and unevenly distributed". It is currently the NORM, most sharply in the industrial band - which is the band the 1960s start uses.

THIS IS THE FLAT FAILURE MODE WE NAMED AS THE ENEMY, and it is now measured rather than feared. GENERATION_STRATEGY.md § Asymmetry is the deliverable: "every region can close its own chains -> no region needs another -> trade has no reason to exist." Nine of fourteen ancient markets and seven of nine industrial ones are effectively interchangeable.

THE MECHANISM IS THE INTERESTING PART, and it is visible in the per-market table. The generator DOES produce real raw asymmetry - market 48704 reaches 7 distinct deposited resources, 48703 reaches 16, 48698 reaches 17. But CHAIN CLOSURE SATURATES:

    market 48704:  7 raws -> closes  6 of 15
    market 48706:  7 raws -> closes  6 of 15
    market 48705: 12 raws -> closes 12 of 15
    market 48699: 13 raws -> closes 14 of 15
    market 48703: 16 raws -> closes 14 of 15
    market 48698: 17 raws -> closes 15 of 15

Twelve raws already closes twelve chains; going from 13 to 17 buys ONE more. So asymmetry in INPUTS does not survive into asymmetry in CAPABILITY - the middle of the distribution collapses upward and the world becomes bimodal: a large interchangeable cluster plus two genuinely poor outliers.

That is a threshold effect, not a tuning miss, and it will not be fixed by varying deposits more.

OPTION 2 IS MEASURED AND DEAD (2026-09-01). The recommended first probe was 'vary `max_logistics_reach` and re-read the spread'. `demand_census` now carries a `--reach` flag for exactly this (a knob, not an edit-and-revert of a shipped constant), and the sweep is conclusive:

    reach   in-reach   ancient SPREAD
    24      73%        min 0.3750  p25 0.8750  median 0.9375  p75 0.9375  max 1.0000
    16      63%        min 0.3750  p25 0.8750  median 0.9375  p75 0.9375  max 1.0000
    12      58%        min 0.3750  p25 0.8750  median 0.9375  p75 0.9375  max 1.0000
     8      52%        min 0.3750  p25 0.8750  median 0.9375  p75 0.9375  max 1.0000
     4      44%        min 0.3750  p25 0.8750  median 0.9375  p75 0.9375  max 1.0000

Industrial is identical at every step too (min 0.6154, median 1.0000). Cutting the budget from 24 to 4 removes 40% of every market's reachable ground — market 48711 goes from 2510 in-reach tiles to 1240, a 51% loss — and NOT ONE MARKET'S COMPLETENESS MOVES, on either band, to four decimal places. The flag is verified to be biting: the R5 header echoes the overridden budget and the in-reach counts collapse as expected.

WHY, and it sharpens the original finding rather than contradicting it: the raws a market needs to close its chains are all in its INNER catchment. The ground a tighter budget takes away is the far ground, and the far ground carries duplicates of resources the market already reaches. So geography does not bite because there is nothing distinctive at the edge to lose.

By the entry's own reasoning that leaves the saturation STRUCTURAL, and option 1 (deeper or narrower chains) as the real answer — or option 4, accepting it and sourcing trade from price and scale rather than capability.

**Why it matters.** Trade is one of the game's two pillars and it needs a reason to exist. If most markets can close most chains locally, the reason is thin - and the 1960s start, which is what the prototype opens on, is the worse of the two bands. It also means BL-706's spread is currently reporting a world that fails the design's own intent, which is exactly what the instrument was built to be able to say.

- DEEPER OR NARROWER CHAINS. If closing a chain needed more distinct inputs, fewer markets would clear the threshold and the middle would spread out. Touches the recipe graph, and it is the change that attacks the saturation directly.
- A TIGHTER REACH BUDGET. `economy.construction.max_logistics_reach = 24.0` is generous - market 48706's catchment is 493 tiles and ALL 493 are in reach. Cutting it makes geography bite and costs nothing but a constant.
- RARER INPUTS. Make some raws genuinely scarce rather than merely varied, so a market missing one is missing it badly. Touches planetology's endowment.
- ACCEPT IT. Self-sufficiency being common is a legitimate world if trade comes from something other than necessity - price, scale, or specialisation rather than capability.

> **Recommendation:** Option 2 is eliminated by measurement — do not spend more on it. Between the two survivors: option 1 (deeper or narrower chains) attacks the mechanism directly and the measurement says why it would work — twelve raws already closes twelve chains, so the threshold is what is flat, not the endowment. Option 4 is a legitimate world and cheaper, but it needs trade to come from somewhere, and sprint 27's whole finding is that there is barely any demand to trade INTO yet. My read: option 1, but AFTER the demand channels land — a deeper recipe graph in an economy with no buyers just makes more things nobody wants. Option 3 stays ruled out for the reason already recorded: the generator already produces a 7-to-17 raw range and closure flattens it.

*Files: `tools/verify/demand_census.cpp`, `scripts/economy.lua`, `docs/generation/GENERATION_STRATEGY.md`, `docs/economy/MARKETS.md`*

### NR-764 — Two definitions baked into the completeness instrument that other work will tune against
*decision taken on your behalf · raised 2026-08-31 · from BL-706, flagged by the slice agent for Ben's eye.*

BL-706 had to define two things no doc owned. Both are reasonable, both were composed from existing rules rather than invented, and both are now baked into an instrument that later work will be tuned against - which is why they are flagged rather than left in a code comment.

1. "WITHIN REACH" FOR A MARKET. Resolved as the composition of two rules that already exist: `market_for_tile` (which already partitions every tile into exactly one market catchment, routing to the nearest centre_tile on multi-market bodies) plus `place_building_allowed`'s own reach clause against the AUTHORED constant `economy.construction.max_logistics_reach = 24.0`. So a tile counts for a market when it clears against that market AND a corporation could legally site a building on it.

   The alternative the agent rejected - "connected to an anchor at any cost" - would have been far too permissive. Grain was market catchment rather than body because every market in the shipped world is on the same body (15124), so body grain yields exactly ONE data point and no spread at all.

2. THE BACKGROUND-INDUSTRIAL BASKET IS EXCLUDED from the terminal set. MARKETS.md property 4 names three terminal sinks and it is not one of them, and the register already labels it a STOPGAP. Counting a world-scale constant basket as a chain endpoint would put identical goods in every market's denominator for a reason that is not a fact about the world.

BOTH ARE CONTAINED. If Ben wants province grain instead of market catchment, or reach measured against a different budget, the change is confined to `measure_completeness` and `tile_in_reach`.

**Why it matters.** NR-763 proposes tuning the reach budget on the strength of this instrument's reading. If the reach definition is wrong, that tuning would be chasing an artifact - so the definition wants an eye on it before it is used as a lever, not after.

> **Recommendation:** Both calls look right to me and I would keep them. The reach one is the load-bearing half: it is composed from the game's own placement rule against an authored constant, so the instrument measures what a corporation could actually DO rather than an abstract connectivity. That is the property that makes NR-763's option 2 a legitimate experiment rather than a circular one.

*Files: `tools/verify/demand_census.cpp`, `docs/economy/LOGISTICS.md`, `docs/economy/MARKETS.md`*

### NR-766 — The world seeder sizes against consumer demand only, never against processing-input demand
*novel-work · raised 2026-08-31 · from Found by the BL-708 slice while making power plants get built at all.*

`body_demand` in `corporation_generation.cpp` counted only household baskets plus the background stopgap. It never counted what PROCESSORS need as inputs.

THE CONSEQUENCE, and it explains a census reading that has been sitting unexplained: **any chain whose raw is not independently demanded by consumers never gets mined.** Coal has a recipe. Coal is produced 0.0. Nothing consumes coal directly, so the seeder never sized for it, so no coal mine was ever placed, so the recipe that wants coal starves - and the census reports it as an orphan resource rather than as a seeding gap.

The slice added `body_upkeep_demand`, re-measured PER ITERATION because placing a firm creates its own draw, selecting on absolute shortfall. It is zero while no upkeep rate is authored, so pre-BL-708 worlds generate byte-identically.

THAT PATCHES THE UPKEEP HALF ONLY. Processing-input demand is still unseen, which is the larger half: it is the difference between a world seeded to feed its own chains and one seeded to feed its households and hope.

NOVELTY: the change is in `corporation_generation.cpp`, which is the GENERATION layer, and it was made by an economy slice whose brief did not name it. It was load-bearing - without it BL-708 cannot work at all - but it deserves generation-dev's eyes rather than standing as an economy agent's judgement call.

**Why it matters.** It is a structural reason the demand census reads the way it does, and it is upstream of everything sprint 27 is doing. Building demand channels into a world that was never seeded to supply them produces exactly the starved chains BL-641 measured.

> **Recommendation:** File the processing-input half as its own item and give it to generation-dev, with the upkeep half that just landed as the worked pattern. It is closely related to NR-765 - both are about a selection step that cannot see a whole category of want - and the two together are probably why the ancient chain does not convert.

*Files: `src/world/corporation_generation.cpp`, `docs/generation/CORPORATION_GENERATION.md`, `docs/generation/GENERATION_STRATEGY.md`*

### NR-767 — Lua-LINKED harnesses are a growing class and the only record of it is a hand-written list
*observation · raised 2026-09-01 · from Found by BL-710 while repairing the harness.*

`tools/verify/save_roundtrip.cpp` includes `harness_params.hpp`, which since NR-686's fix includes `scripting/lua_state.hpp` — so it pulls sol2 and needs the Lua link. `build_harness.js` and `build_harness.bat` fail it with `fatal error C1083: Cannot open include file: 'sol/sol.hpp'`, which reads as a broken harness rather than as the wrong builder. It builds cleanly with `cmd //c toolserifyuild_lua_harness.bat save_roundtrip`.

The documented Lua-linked list (build_lua_harness.bat's own header, and NEXT_SESSION.md) names pregame_balance_harness, condition_set_harness, mercenary_contract_harness, spawn_solvency, demand_census and chain_depth. It does not name save_roundtrip, and the list is a hand-maintained enumeration rather than a derived fact.

THE GENERAL SHAPE: **any harness that includes `harness_params.hpp` is now Lua-linked**, and `harness_params.hpp` is the header that DEVELOPMENT_PRACTICES.md § A harness must build the world the application builds tells every harness to use. So the class is growing by design and the list will keep going stale.

MEASURED WIDER, 2026-09-01. Working BL-712 I reached for eight harnesses and FIVE of them failed `build_harness.bat` on the same `sol/sol.hpp` line, none of them on any list: world_determinism, determinism_harness, recipe_switch_harness, build_spree_harness, decision_trace_harness — plus save_roundtrip and ai_skill_harness. So this is not one stale entry; the documented list names six harnesses and the real class is at least thirteen and growing every time a harness adopts harness_params.hpp, which DEVELOPMENT_PRACTICES.md § A harness must build the world the application builds tells it to do.

**Why it matters.** This is NR-759's pattern one level up: a harness that will not build looks identical to a harness that is broken, and the last session lost time to exactly that ambiguity. The failure mode is silent-absence, not red.

> **Recommendation:** Derive the routing rather than list it: have `build_harness.js` detect a transitive `sol/sol.hpp` include (or simply an include of `harness_params.hpp`) and either delegate to `build_lua_harness.bat` or fail with a one-line message naming it. Cheap, and it retires the hand-maintained list.

*Files: `tools/verify/build_harness.js`, `tools/verify/build_lua_harness.bat`, `tools/verify/harness_params.hpp`, `docs/development/DEVELOPMENT_PRACTICES.md`*

### NR-768 — save_roundtrip verifies a resource_count bump only incidentally — it never names the constant
*observation · raised 2026-09-01 · from Found by BL-710 while confirming v21 and v22 round-trip, which was the item's closing instruction.*

BL-708 (v21, `power`) and BL-709 (v22, `construction_capacity`) are both **resource_count widenings** — 47 -> 48 -> 49 — not new containers. So the repaired harness does verify them: P7's container table and P8's byte-for-byte re-serialise of a fully populated world both run over per-resource arrays at the current length, and both pass at v22.

But it verifies them by accident of coverage, not by assertion. The harness never mentions `resource_count`. There is no row that would go red if a per-resource array were written at the wrong length while the world happened not to exercise the difference, and no row that names the two bumps this item was asked to confirm.

Related but distinct: the literal version-refusal ladder stops at v15 (P19b). v16 through v20 have no row at all; v21 is covered only through P20's deliberately symbolic `world_save_version - 1u`. That gap is by design per P20's own comment and is NOT being reported as a defect — it is noted only because it means the refusal side of these two bumps rests on one symbolic row.

**Why it matters.** Two of the last three save bumps were per-resource widenings, so this is now the format's most common kind of change, and it is the kind the harness asserts least directly. A future append would be 'verified' the same incidental way.

> **Recommendation:** Add one cheap row: assert `resource_count` equals what the populated fixture wrote, and note the current value in the harness output the way `next entity id` is noted. Not done here — BL-710 is scoped as deletions only, and adding an assertion to the save seam mid-repair is the thing that item explicitly warned against.

*Files: `tools/verify/save_roundtrip.cpp`, `src/world/world_save.hpp`*

### NR-769 — BL-712's fix is in, and the scale-blind exclusion has a SECOND seat inside the build score itself
*decision · raised 2026-09-01 · from Measured by BL-712 after implementing the per-category recipe selection the item designed.*

BL-712's fix shape is done: the build candidate keeps a best PER GROUP and hands every group to the scorer. It works, and it is measurable — instrumented on the industrial band, `Power Generation` emitted **168** build candidates and `Construction` **430**, where before the change both emitted **zero**, in any world, at any time.

THEY STILL DO NOT WIN. Peak scores by group over one 80-tick warm start:

    Advanced Fabrication   n=1375   max 1884.3
    Electronics            n=2140   max 1234.1
    Welfare Goods          n=1196   max  411.6
    Metal Foundry          n=1483   max  264.0
    Construction           n= 430   max  105.3
    Food Processing        n=1570   max  102.2
    Power Generation       n= 168   max   31.9
    Refinery               n=1174   max    1.6

Builds are capped per evaluation, so an 18-60x score gap is still an exclusion — just one seat further down. The score is `net^2 / capex`, which is an ABSOLUTE contest, and § Selection must be scale-free's own sentence condemns it in as many words: *a cheap good can never win an absolute contest, however badly the world needs it*.

WHY THIS WAS NOT JUST FIXED. AI_OPPONENT.md § Scoring says the quadratic's margin bias is **retained deliberately** and that replacing it is a re-tune plus a golden reshuffle — BL-417 (build score is quadratic), **Ben's call**. Taking it here would have been deciding an open item on his behalf, silently, inside a different one.

**Why it matters.** BL-712's stated VERIFY criterion — power and construction_capacity produced > 0 from AI builds — is not reached, and this is why. It is also the sprint's own success test in miniature: the channel is now reachable and the corp still will not build it.

- A) Take BL-417 now — replace net^2/capex with an explicit linear capital-efficiency metric. One deliberate golden re-bless wave with dated provenance, which BL-711 is already going to need.
- B) Normalise the build score WITHIN its group before the global sort, so categories are ranked comparably. Truest to the rule as written; the largest behavioural change, and it would let a lone Refinery candidate rank beside Advanced Fabrication's best.
- C) Add a scarcity term mirroring the existing `glut` multiplier — a good the world wants and nobody makes scores UP. Systemic rather than a handicap (the standing preference), but it is new design, not a fix.
- D) Leave it. BL-712 delivered its fix shape; the second seat becomes its own item behind BL-417.

> **Recommendation:** C is the one that reads as an in-world force rather than a term inside the agent, and it composes with A rather than competing — but it is design, so it wants your call before it is built. If the sprint needs movement sooner, D plus a new item is the honest sequencing: BL-712 is not being stretched to cover BL-417.

*Files: `src/world/corp_ai.cpp`, `docs/ai/AI_OPPONENT.md`*

### NR-770 — Construction yards do not survive, and where they do survive they produce nothing
*observation · raised 2026-09-01 · from Measured by BL-712 with chain_conversion_probe, industrial band, before and after the fix.*

`steel -> construction_capacity` reads 2 facilities at spawn and **0** after an 80-tick warm start. That is unchanged by BL-712: with the chase now confined to a facility's own group, a yard can only move to another Construction method, and no other Construction row appears in the probe at all. So the two yards are not being switched — they are being removed.

WHAT BL-712 DID FIX NEARBY, for contrast, so the two are not confused: `silica -> silicon` was 16 -> 2 before and is 16 -> 10 after; total processing facilities after the warm start went 268 -> 282 and distinct recipes in use 14 -> 17. The scale-blind chase was real and it is gone. The yards are a different mechanism.

THE LIKELY CAUSE, unverified and deliberately not chased here: `economy.lua` authors `construction_capacity` upkeep at **0.0 in both bands** (lines 494-499, the BL-641 zero-rate note). Its only demand is the construction draw itself, so a yard's revenue depends entirely on something being built within reach of it. That makes it a persistently loss-making building, which is exactly what the BL-079 reflex tier idles.

THE ANCIENT BAND IS THE SHARPER HALF, found closing BL-709 the same day. There the two `timber+planks -> construction_capacity` yards DO survive the 80-tick warm start — chain_conversion_probe reads 2 at spawn and 2 at the end — and demand_census reads `construction_capacity` PRODUCED = 0.0. Both inputs are on the shelf in quantity (timber 5359.1, planks 635.8). So this is not one failure mode but two: industrial yards are removed, and ancient yards stand idle. A yard that is idled for want of revenue and a yard that is idled for want of a reachable input look the same from outside, and neither has been separated yet.

This is what holds BL-709's R1 open — 'capacity exists, draws its inputs every tick, and grows with the world' is the requirement, and the world does none of the three.

**Why it matters.** BL-709 landed construction as an economy-scaled sector and the economy is removing it. This is upstream of BL-642 (construction draws) and probably the same fact from the other side.

> **Recommendation:** Hand it to BL-642 as the first thing that item measures: it already owns the construction draw, and 'the yard has no revenue' and 'the draw does not reach the market' are likely one finding. Do not fix it inside BL-712 — a yard that the reflex tier is right to idle is not an AI-selection defect. The ancient half is the cheaper diagnosis and should go first: two yards, on a body whose inputs are demonstrably present, producing zero. Whatever gates them is likely the same gate the industrial yards fail on before they are removed.

*Files: `scripts/economy.lua`, `src/world/economy_system.cpp`, `tools/verify/chain_conversion_probe.cpp`*

### NR-771 — ai_skill_harness — sprint 27's own success criterion — is STRUCTURALLY BLIND to a per-category change
*observation · raised 2026-09-01 · from Found by BL-712 running the harness before and after the fix and getting byte-identical numbers.*

Sprint 27's success test is `ai_skill_harness` showing a field that is not monotonically insolvent. Run at HEAD and again with BL-712's fix in, it returns **identical numbers on all five seeds** — same net worths to the decimal, same solvency counts, the same 25 failures. Not similar: identical.

THE HARNESS CANNOT SEE THE CHANGE. `make_registry` (ai_skill_harness.cpp ~145-162) hand-builds **three** recipes — steel, refined_fuel, food_rations — and sets `group` on none of them, so all three fall to the default "General". BL-712 is a per-`recipe::group` change. With exactly one group in the registry, a per-group best IS a global argmax and the cross-group dial guard can never fire. The instrument is inert by construction, and byte-identity is the proof.

THE FIX IS NOT IN DOUBT — it was measured on the REAL Lua registry with chain_conversion_probe, which reads 45 recipes across 13 groups. Industrial band, before -> after: total processing facilities after an 80-tick warm start 268 -> 282, distinct recipes in use 14 -> 17, `silica -> silicon` 16->2 becomes 16->10, and instrumented candidate emission for `Power Generation` and `Construction` went from ZERO to 168 and 430. It is the SUCCESS TEST that is blind, not the work.

This is DEVELOPMENT_PRACTICES.md § A harness must build the world the application builds, at the worst possible altitude: a three-recipe economy standing in for a forty-five-recipe one, in the harness the sprint is steering by.

**Why it matters.** Sprint 27 is steering by this instrument. A change can be right, measured, and land — and the harness will report no movement, which reads as 'the fix did nothing'. Worse in the other direction: a change that genuinely helps the shipped economy still cannot show here, so the sprint could complete every item and its own criterion would not move.

> **Recommendation:** Two options, and they are not exclusive. (a) Give the hand-built registry real `group` values and a few more recipes spanning at least three groups — cheap, keeps the harness Lua-free, and makes it able to see category-shaped work. (b) Move the harness onto the Lua registry the way demand_census and chain_conversion_probe already do, which is the § A harness must build the world the application builds answer and costs it the Lua link (NR-767). Until one of them is done, sprint 27's success criterion should be read as chain_conversion_probe + demand_census, not this.

*Files: `tools/verify/ai_skill_harness.cpp`, `docs/development/DEVELOPMENT_PRACTICES.md`, `docs/development/sprints.json`*

### NR-772 — demand_census surveyed nothing, so the corp AI built ZERO extraction sites in it - fixed, and it moves every reading
*decision · raised 2026-09-01 · from Found by BL-711 when the census came back byte-identical before and after a change the probe showed as enormous.*

`demand_census` never called `init_survey_states`, which `app.cpp` runs at campaign start. So every body - HOME INCLUDED - stayed `hidden`, and `rank_extraction_sites` gates on survey visibility: it returned an empty candidate list on every tick of every run. THE CORP AI BUILT ZERO EXTRACTION SITES for the entire warm start. Not fewer. Zero. Every extraction count this census has ever printed was the seeder's placement, frozen.

HOW IT SURFACED: BL-711 changed the extraction candidate list from a global top-M to a per-resource top-K, which chain_conversion_probe (which DOES survey) measures as coal going from 0 mines in any world to 25. demand_census reported the two builds byte-identical, because in its world neither list is ever consulted.

FIXED, one line, matching ai_skill_harness.cpp's own 2026-08-31 note verbatim in reasoning - same class, same day, same answer, and that in-repo precedent is why this was applied rather than filed. Only THREE of ~125 harnesses call `init_survey_states`: ai_skill_harness, chain_conversion_probe and survey_harness.

IT MOVES EVERY READING, and here is the separation so the two effects are not confused. Ancient band, `produced`, blind census -> surveyed pre-BL-711 -> surveyed with BL-711:

    coal            0.0  ->    0.0  ->  632.7
    clay            0.0  ->    0.0  ->  349.1
    hides           0.0  ->    0.0  ->  353.9
    sand            0.0  ->    0.0  ->   35.8
    ceramics       11.5  ->   24.5  ->   55.3
    leather         4.6  ->    8.0  ->   58.4
    buildings       335  ->    419  ->    440

Surveying gets the AI building AT ALL (335 -> 419 buildings). The four permanently-zero chains are BL-711's alone - they stay at exactly 0.0 in a surveyed world without it. Every census assertion still passes in all three configurations.

**Why it matters.** Sprint 27's stated method is 'run demand_census before and after every item; the deltas are the sprint'. For every item that touches what the AI BUILDS, the instrument could not see the work. This is the second blind instrument this session (NR-771 is ai_skill_harness) and it is NR-762's family - ~30 harnesses skipping the app's world-building tail - landing on the sprint's primary measurement.

> **Recommendation:** Kept. It is DEVELOPMENT_PRACTICES.md § A harness must build the world the application builds, applied to the file that most needs it, with an in-repo precedent from the previous session. Reversing it is deleting one line, and the dated comment says why it is there. What wants YOUR call is broader: NR-762 asks whether the other ~120 harnesses should be swept the same way, and this is now the second expensive instance of that gap in as many sessions.

*Files: `tools/verify/demand_census.cpp`, `tools/verify/ai_skill_harness.cpp`, `docs/development/DEVELOPMENT_PRACTICES.md`*

### NR-773 — BL-642's premise has moved under it, and its second half needs one design call before it can be built
*question · raised 2026-09-01 · from Scoped by the sprint-27 session after BL-711 and NR-772 landed; measured, not assumed.*

BL-642 has two halves. Both need a word from you before building, for different reasons.

HALF (1) - 'make the opening years actually build'. The item says the construction draw NEVER FIRES during the opening years. Measured today, that is no longer the reading. `ERAS.md` settles what the phrase means: the first ~20 years ARE `app::start_new_game`'s 80-tick warm start, not a separate generation pass. And the warm start does now construct. Ancient-band construction-channel demand, blind census -> surveyed (NR-772) -> surveyed with BL-711: **93.25 -> 202.58 -> 210.08**, with timber's construct draw going 54.0 -> 118.3 and clay 0.0 -> 13.3. The AI builds 105 more buildings than the blind census could see.

What REMAINS true of half (1) is narrower and sharper: the SEEDER's ~330 buildings are authored complete (`author_building` writes a default `ticks_remaining` of 0, corporation_generation.cpp) and draw nothing, ever. Fixing that literally would open the campaign with 330 half-built buildings, which is a gameplay change, not a plumbing one - so it wants your reading rather than my guess.

HALF (2) - 'population centres draw construction materials as they grow' - is the half the item itself calls the one that lasts, and it is unambiguous in intent. The hook is clean and already there: economy_system.cpp's BL-616 growth block, where `pcc.population` takes a step and `++pcc.scale` promotes. What is NOT settled is whether that draw GATES growth.

- A) A WANT ONLY. The centre registers demand for its materials, prices them, and grows regardless. Matches every existing injector - none of them gates - and is the safe direction: if you wanted gating, this is a subset rather than a wrong turn.
- B) GATED, like a build. Growth stretches or pauses when the market cannot supply, exactly as run_construction stretches a build. Truest to MARKETS.md property 3 and it makes construction materials genuinely load-bearing - but a centre that stops growing for want of timber is a real gameplay change and it can deadlock a poor region.
- C) A want now, gating later behind its own item, once the materials actually reach the shelf.

> **Recommendation:** C. Half (2) as a WANT, with rates measured against the census rather than guessed, and gating filed separately - because BL-641's cliff is the standing warning here: a brand-new universal draw unmet on tick 1 collapsed operating firms 198/328 -> 33/317, and gating growth on an unmet draw is that same cliff pointed at population instead of firms. For half (1), my reading is that BL-711 and NR-772 have already delivered most of what it was reaching for, and what is left (the seeder's instant buildings) is a separate and much bigger question. Not built either way - the item is left open with this measurement attached.

*Files: `src/world/economy_system.cpp`, `src/world/corporation_generation.cpp`, `scripts/economy.lua`, `docs/economy/PRODUCTION.md`, `docs/economy/MARKETS.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-592 — corp_ai.cpp never prices resource_build_cost when scoring a build candidate — pre-existing, not caused by BL-590
*observation · raised 2026-08-24 · from Measured while authoring BL-590 (per-building materials): the build-candidate loops price only building_economics::build_cost (ex.build_cost / pe.build_cost / mex.build_cost), never resource_build_cost.*

BL-590 gave named buildings materially different resource_build_cost baskets (ancient buildings now cost timber/stone, not steel). The AI scorer's capex estimate never priced that array before this item and still does not after it — a candidate scores on cash alone, so a rival can propose a build whose MATERIALS it cannot actually reach even with sufficient cash. construct_building's own affordability gate refuses it cleanly at apply time (no mutation), so this is a missed-opportunity gap for the scorer, not a correctness bug: the seam already enforces the real cost, same shape as the recipe-switch scorer's own documented gap (NR-242, PRODUCTION.md § Alternate production methods).

**Why it matters.** Was already true before BL-590 (every type shared one steel basket, so the gap was invisible — steel is cheap and plentiful for most rivals). BL-590 makes it visible for the first time: an ancient rival with no timber stockpile could now score a Sawmill it cannot actually place. Not urgent (the gate protects correctness), but worth knowing before BL-589's start-gate audit or BL-594's playthrough, in case a rival's build thrash traces back to this rather than to something either of those items would otherwise suspect.

> **Recommendation:** Leave as documented debt unless BL-589/BL-594 measure it costing something real (excess refused-build churn, a rival stalling on a candidate it can never place). If so, pricing resource_build_cost_for into the scorer's capex estimate is a small, contained addition — the same shape corp_ai.cpp already applies to build_cost.

> **RESOLVED.** FIXED by BL-709 (construction as a rate), verified 2026-09-01. `build_material_cost` is priced into the capex of BOTH build candidates — corp_ai.cpp:951 (extraction) and :1202 (processing) — so a candidate's score and its solvency spend now carry the material basket, not the flat build_cost alone. The recipe travels into the lookup, because since BL-590 the basket is keyed by it: a Sawmill and a Smithy do not cost the same to build. Ben's call as recorded in PRODUCTION.md § Construction as a rate — under a shared contended capacity pool this stopped being a missed opportunity and became a correctness problem.

*Files: `src/world/corp_ai.cpp`, `docs/economy/PRODUCTION.md`*

### NR-743 — The margin is measured against the strongest corporation, uniformly - not against the player specifically
*decision taken on your behalf · raised 2026-08-31 · from Sprint 26 opening. Ben, 2026-08-31: "trying to match the skill of their opponent, and beating them by a slim margin."*

Rewriting AI_OPPONENT.md § The goal needed one thing decided that Ben's sentence leaves genuinely open: a slim margin over WHOM.

TWO READINGS, both faithful to the words.
  (a) UNIFORM - every corp holds the same rule and measures it against whoever currently leads. The player is simply another corporation to it.
  (b) PLAYER-FACING - the margin term reads the player's corp specifically, so the AI tracks the human's performance and stays just above it.

I TOOK (a) AND WROTE IT INTO THE DOC. Three reasons, in order of weight:
  1. (b) is rubber-banding aimed at one seat. § 1 Area 4's own finding is that players tolerate rubber-banding only when it is explicit, and revolt when they detect it hidden. (a) is a rule about the world; (b) is a rule about the human, and reads as one the first time somebody notices it.
  2. (b) has NO SUBJECT UNDER SPECTATE - the mode this very sprint exists to watch through (BL-695, live spectate route). A margin term that degenerates to nothing in spectate cannot be tuned in the mode we intend to tune it in.
  3. (b) fails when the player is NOT the leader. Tracking a struggling human means the AI throttles down to match, which produces a world where nothing is happening rather than a close race.

WHAT (a) COSTS, honestly stated. It does not literally 'match the skill of the opponent' when the opponent is mid-field: the AI holds a narrow margin over the LEADER, and if that leader is another rival, the player can be beaten by considerably more than a slim margin while the objective is technically satisfied. (b) is the truer reading of Ben's sentence taken literally. If the felt experience under (a) turns out to be 'the rivals had a close race and I was nowhere', that is the signal to revisit - and the fix is likely a hybrid (uniform rule, but the margin measured against the field's median rather than its leader) rather than a swing to (b).

CHEAP TO OVERTURN NOW, expensive later. It is one term's definition in BL-698 (scorer margin term), which is not yet built. Once the tunables are calibrated against it, changing the measured quantity invalidates the tuning.

**Why it matters.** It is the definition of the sprint's central quantity, and every tunable in BL-698 (scorer margin term) and every band in BL-697 (skill harness margin metric) is calibrated against whichever reading stands. It is also the difference between a design that survives being noticed and one that does not.

> **Recommendation:** Keep (a) through the sprint and JUDGE IT BY WATCHING, under BL-695's (live spectate route) spectate. The question is not answerable on paper - it is whether a run feels like a close race or like a race you were not in. Revisit at the sprint retro with an actual run to point at.

> **RESOLVED.** DISSOLVED, not answered, by Ben's systemic reframe the same day (see NR-746 and AI_OPPONENT.md § Where restraint comes from).

The entry asked whether the margin is measured against the strongest corporation or against the player specifically. Both readings assumed a term in the scorer aiming at a margin. Ben removed that assumption: no actor aims at the margin at all, so there is no term whose subject needed choosing. The close race is produced by coalitions and climate, and the spread is something the HARNESS measures rather than anything the code targets.

What survives of it: the uniform-vs-player-facing instinct was right for the wrong reason. Coalitions are inherently uniform - they form against whoever leads, and against the player identically when the player leads - so the property the entry argued for is now structural rather than a choice anyone has to keep making.

Ben DID answer the adjacent question directly: standing is a composite of net worth, research and military strength (BL-700, composite standing index).

*Files: `docs/ai/AI_OPPONENT.md`, `src/world/corp_ai.cpp`, `tools/verify/ai_skill_harness.cpp`*

### NR-745 — No authority doc owns a LIVING climate, and it is now load-bearing for the AI design
*novel-work · raised 2026-08-31 · from Ben, 2026-08-31: "we haven't built a critical system which is climate." Filed at the moment the feeling arose, per the novelty rule.*

Climate became load-bearing today. AI_OPPONENT.md § Where restraint comes from names it as one of the two systemic brakes that keep a leader from running away - so the AI design now DEPENDS on a system that does not exist and that no doc owns.

WHAT EXISTS, measured. Climate appears in planetology.cpp, tile_generation.cpp and hard_coded_world.cpp, and is documented in TILES.md, PLANETOLOGY.md, TILE_GENERATION.md and RESOURCES.md - all of it GENERATION-TIME, shaping terrain as tiles are made. economy_system.cpp, corp_ai.cpp and budget_system.cpp do not read the word. Both directions are missing: nothing a corporation does can affect climate, and climate cannot affect a running campaign.

WHY IT IS A NOVELTY FLAG AND NOT JUST A BIG ITEM. PLANETOLOGY.md owns body-level atmosphere, chemistry and biosphere HISTORY - generation's concern. A live campaign force that responds to what corporations do is a different subject, and filing it under a generation doc would put a campaign system in the wrong authority. SYSTEMS.md owns the system map and would need to admit it. This is a new system in the project, and per the standing rule novelty should be CHOSEN rather than accreted.

IT ALSO GROWS SCOPE IN A DIRECTION NOTHING ELSE HAS. A commons that strains under total activity touches production, habitability, terrain and plausibly nations (which would reach the 2026-08-18 nation grant). BL-701 (living climate) is filed design-owed at difficulty 8 with no build plan, deliberately.

I DID NOT PAUSE THE SESSION FOR IT, per the rule's own carve-out, because the sprint does not depend on it: BL-699 (rival coalitions) can produce a close race on its own. Climate is what later makes that race honest when the PLAYER is the one leading.

**Why it matters.** A design doc now cites a system that has no owner, no design and no code. That is the exact shape NR-70x caught in FINANCE.md - a doc describing something in the present tense that nothing backs - and it is worth catching before it is written into more places than one.

- Give climate its own authority doc (docs/economy/CLIMATE.md or docs/generation/CLIMATE.md) and admit it to SYSTEMS.md. Cleanest, and matches how every other system in Io is owned.
- Extend PLANETOLOGY.md with a live-campaign section. Cheaper, but puts a campaign force in a generation doc, which the state-independence rule would read as a category error.
- Design it inside AI_OPPONENT.md as an AI-support mechanism. WRONG, and named here only to reject it - climate would bear on the player identically, so it is a world system that happens to brake the AI, not an AI feature.

> **Recommendation:** Its own doc, admitted to SYSTEMS.md, designed in sprint 26 and built in the sprint after. The design questions BL-701 lists are unanswered and several are yours to call - notably whether it degrades reversibly or ratchets, which decides whether climate is a pressure or a doom clock.

> **RESOLVED.** ANSWERED by Ben the same day - "Let's design climate docs here" - and by BL-701 (climate doc), which took option 1: climate has its own authority doc.

docs/CLIMATE.md is top-level, alongside EVENTS.md and META_LAYER.md, NOT a section inside PLANETOLOGY.md. The reasoning the entry set out held up: PLANETOLOGY.md owns generation-time atmosphere, chemistry and biosphere history, and a live campaign force that responds to what corporations do is a different subject. Admitted to SYSTEMS.md § Environment and to CLAUDE.md's router.

The novelty is therefore CHOSEN rather than accreted, which is what the flag existed to secure. What remains is not novelty but ordinary open design - CLIMATE.md § 11 lists it, and the one call that changes the feel of the system is raised separately as NR-747.

*Files: `docs/generation/PLANETOLOGY.md`, `docs/SYSTEMS.md`, `docs/ai/AI_OPPONENT.md`, `docs/economy/TILES.md`*

### NR-748 — I read "Era 1 is actually nuclear war" as the Era 0 -> Era 1 BOUNDARY, and edited CONCEPT.md on that reading
*decision taken on your behalf · raised 2026-08-31 · from Ben, 2026-08-31: "My mistake with the word choice. Era 1 is actually nuclear war."*

Flagged because the sentence has two readings, because I picked one without asking, and because acting on it edited CONCEPT.md - a doc whose framing is Ben's.

THE TWO READINGS.
  (a) THE BOUNDARY. The Era 0 -> Era 1 rupture is a nuclear war. Era 1 itself remains Early Space and opens on the aftermath.
  (b) THE ERA. Era 1 as a PERIOD is characterised by nuclear war - i.e. the ladder's naming is wrong and Era 1 is not Early Space at all.

I TOOK (a). Reasons: CONCEPT.md § Eras already had the Era 0 exit as "a global-rupture-scale war" that reshapes the world enough for rapid space expansion, so (a) NAMES something already designed rather than overturning it; ERAS.md § What moves an Era already makes the boundary a catastrophic seeded event that shocks markets and destroys infrastructure, which is the shape of a war; and Ben's own sentence was a correction of MY word choice, which suggests a clarification rather than a redesign of the ladder.

WHAT I WROTE ON IT. CONCEPT.md § Eras now says the rupture is nuclear and names climate as what makes the space expansion plausible. ERAS.md § What moves an Era says the same and splits climate into a pre-war drift and a post-war aftermath. CLIMATE.md § 1 and § 3 are built on it throughout.

IF (b) IS WHAT HE MEANT, the correction is real but contained: the era NAMES and the ladder change, and CLIMATE.md's two-regime model survives unaltered, because it is anchored to 'before the war' and 'after the war' rather than to an era number.

**Why it matters.** It is now written into CONCEPT.md, ERAS.md and CLIMATE.md as settled, and CONCEPT.md is the doc that owns what the game is about. A wrong reading propagating from there is expensive to unpick later, and cheap to correct today.

> **Recommendation:** Confirm or correct in one line. If (a) is right, nothing to do. If (b), tell me and I will re-cut the ladder - the climate model itself does not move either way.

> **RESOLVED.** ANSWERED by Ben, 2026-08-31: "The aim of each era is to give a catastrophe for players to avoid."

NEITHER of the two readings the entry offered was right. It asked whether the nuclear war is the BOUNDARY between two eras or a RENAMING of Era 1. It is neither: it is the CATASTROPHE OF ITS ERA, and the player plays to AVOID it. Passing the Era means the war did not happen and the next Era's quest trees open; failing means it did, and the destruction lands on exactly the assets the next Era needed.

So the entry's own framing was the error - it assumed the rupture fires and asked only where to file it. The pre-existing design in docs/research/ERA1_TECH_LANDSCAPE.md (2026-08-05, drafted on Ben's steer) had the answer the whole time: Alarm above Ceiling on the seeded date and it goes hot, below and it is averted a second time. The date decides WHEN THE TEST IS TAKEN, not the outcome.

CORRECTED IN: ERAS.md (§ The point of an Era, new, and § What moves an Era), CONCEPT.md (the exit is a test with a failure branch), CLIMATE.md (rewritten as the next Era's catastrophe), SYSTEMS.md and AI_OPPONENT.md.

WHAT REMAINS OPEN is only the numbering, which is a separate and narrower question - NR-749.

*Files: `docs/CONCEPT.md`, `docs/economy/ERAS.md`, `docs/CLIMATE.md`*

### NR-749 — Three Era numbering schemes are live in the corpus, and they disagree by one
*question · raised 2026-08-31 · from Ben, 2026-08-31: "My mistake with the word choice. Era 1 is actually nuclear war... our prototype works solely on Era 1 for now."*

THE UNCLEAR THING, raised because Ben invited it and because I cannot write the era docs precisely without it. The CONCEPTS are now settled and consistent; only the NUMBERS disagree.

THREE SCHEMES, all currently in the tree.
  (1) ERAS.md / CONCEPT.md: **Era 0 = Terrestrial** (Cold War / Information Age footing), **Era 1 = Early Space**, **Era 2 = Dimensional**. The era you play is Era 0 and its EXIT is the war.
  (2) BL-087 and docs/research/ERA1_TECH_LANDSCAPE.md prose: "**Era 1 failure (WW3)**" - the era you play is Era 1 and its catastrophe is the war. Ben's own 2026-08-05 wording, and the wording he used again today.
  (3) The same research doc's NODE IDS, and the live tech ids in code: **E0-**HEAVY, E0-ELEC, E0-ROCKET; and shipped ancient-arc ids E0-EC-01 (Toolmaker), E1-EC-01 (Bessemer Converter), E0-ML-01. Here E0/E1 are era BANDS on the ancient arc, a third meaning again.

So "Era 1" means the space era in (1), the nuclear era in (2), and an ancient tech band in (3). Note ERAS.md ALREADY warns about exactly this class of collision in its section 'Three things that say era in code, and which one this is' - era_band, condition_subject::era, and the Era of that document. This is a fourth.

WHAT I DID, and it is deliberately number-light: I wrote the corrections using RELATIVE language - 'this Era's catastrophe', 'the next Era's catastrophe', 'the prototype Era' - wherever the number is contested, so nothing needs rewriting whichever way this goes. CLIMATE.md § 8.1 names the open number explicitly.

THE SECOND HALF OF THE QUESTION, and possibly the bigger one: "our prototype works solely on Era 1 for now" sits oddly against ROADMAP.md § The two arcs, which says the LIVE product is the ANCIENT arc at 0 CE with the player as a mercenary company, and that the industrial/space arc (v0.2.0-v1.0.0) is PARKED for DLC. Under scheme (1) the prototype is Era 0 of the ancient arc. Under scheme (2) the prototype is the Cold-War-footing era, which is the SPACE arc's first rung - a parked product. These cannot both be true, and I do not know which one moved.

**Why it matters.** Era numbers appear in ERAS.md, CONCEPT.md, CLIMATE.md, the research draft, the tech ids in code, and now in sprint planning. A silent off-by-one across those is the exact failure ERAS.md already has a section warning about, and it gets more expensive with every doc that cites a number. The arc question is larger still: it decides whether the era work being designed is for the live product or for a parked one.

- RENUMBER the ladder so the era you play is Era 1, its catastrophe the nuclear war, and climate's era is Era 2. Matches Ben's own wording twice, and makes the catastrophe ladder and the era numbers line up.
- KEEP ERAS.md's numbering (played era = Era 0, exit test = the war, Early Space = Era 1) and treat Ben's 'Era 1' as shorthand for the era-1 TRANSITION. Cheapest in edits; leaves the wording collision live.
- DROP the numbers from prose entirely and name eras by their catastrophe or territory - 'the nuclear Era', 'the climate Era'. Sidesteps the collision permanently; larger doc sweep.

> **Recommendation:** Answer the ARC half first - is the era/catastrophe work for the ancient live product or for the parked space arc? The numbering follows from it and is cheap either way. On the numbering itself I would take option 1: aligning the number with the catastrophe is what makes the ladder readable, and Ben has now used that scheme twice unprompted.

> **RESOLVED.** ANSWERED by Ben, 2026-08-31: "We will be working on the 1960s start, so you can resolve that too."

BOTH HALVES SETTLED.

THE ARC: the working start is the 1960s. It is a live branch, not new work - world_params::epoch_year = 1960 selects it, era_band_for_epoch puts the registry on the industrial band, and era_minus_one.cpp skips the antiquity prehistory above 1700. It is however UNREACHABLE without editing a default, which BL-705 (selectable 1960s start) fixes. The 0 CE ancient start stays a supported configuration on the same ladder; ROADMAP.md § The two arcs still owns which is the commercial product and this resolution does not touch that.

THE NUMBERING: option 1 taken. The ladder is renumbered 1-BASED so each Era's number matches its catastrophe - Era 1 Terrestrial (nuclear war), Era 2 Early Space (climate collapse), Era 3 Dimensional, Era 4 Megastructure. This is Ben's own scheme, used three times unprompted.

WHAT MOVED: ERAS.md (the ladder, every heading, the gates), CONCEPT.md, CLIMATE.md, GLOSSARY.md, SYSTEMS.md, PRODUCTION.md, RESOURCES.md, PLANETOLOGY.md, HISTORY.md.

WHAT DELIBERATELY DID NOT MOVE, and this is the important half: the renumber is the DOCUMENT's Era axis only. `era_band` (any/ancient/industrial), `condition_subject::era` (1 for a corp owning a launchpad), and every E0-/E1- technology id are untouched, because ERAS.md § Three things that say era in code already establishes them as separate axes. NOTHING IN src/ MOVED. That section now carries a note that the launchpad proxy reads one Era later than its name suggests.

ONE KNOWN DIVERGENCE CREATED, tracked as NR-751.

*Files: `docs/economy/ERAS.md`, `docs/CONCEPT.md`, `docs/CLIMATE.md`, `docs/development/ROADMAP.md`, `docs/research/ERA1_TECH_LANDSCAPE.md`*

### NR-760 — Should a pool draw also bid on the market? ALREADY SETTLED - I filed this without querying the backlog
*question · raised 2026-08-31 · from Open in scripts/economy.lua since BL-641, escalated to Ben there and never answered. Sprint 26's census made it the first question of sprint 27.*

TWO OF THE THREE LIVE DEMAND CHANNELS CONSUME GOODS WITHOUT PRICING THEM. Building upkeep (run_building_upkeep) and standing-force upkeep (run_unit_upkeep) are POOL draws: they take goods from a corp's stockpile and never reach `market_component::demand`.

THE DEADLOCK THAT CAUSES, in economy.lua's own words: "wanting tools does not RAISE THE PRICE of tools, no rival ever scores building a Toolmaker, and the supply that would meet the draw is never induced."

IT HAS BEEN MEASURED, and the measurement is why BL-641 shipped inert. Turning the upkeep rates on without fixing this:

                          RATES OFF        RATES ON
  buildings                   584             576
  ... complete & operating    227              19
  ... under construction        0              47

A 92% COLLAPSE IN OPERATING FIRMS over an 80-tick warm start. And the note is explicit that halving the rates only delays it, because the cause is structural: nothing in the shipped world produces tools or planks, so every draw goes unmet, the supply factor decays 50/tick to zero inside 20 ticks, output follows, and BL-079's reflex tier idles the now-loss-making firm. The shortfall rule worked exactly as designed - it was fed a good nobody makes.

SO BL-641 LANDED THE SHAPE AND SHIPPED THE RATES AT ZERO, deliberately, and left this in the file: "Whether the Industry channel should also register a market WANT - as run_construction and run_processing both do, and as MARKETS.md § Demand channels arguably requires of a demand channel - is Ben's call, not this file's."

THE PRECEDENT IS IN THIS CODEBASE, one layer up, and it should be read before designing the fix. BL-440 hit the identical shape in the scorer: "a processor must be RUNNING to bid a scarce input's price up, and it cannot run without that input. The deadlock is the defect." Its answer was NOT to route through the price signal - which cannot work - but to take the pull from the RECIPE GRAPH, a static deterministic world fact. That is corp_ai_params::input_demand_pull, and it is self-limiting by construction: the bonus divides by the sites already targeting the resource, so it decays as the shortage is answered.

WHY IT GATES THE SPRINT: every other channel in sprint 27 faces the same choice the moment it is built. If a channel's draw does not price what it wants, it consumes supply without inducing any, and building five more of them produces five more silent sinks and a faster collapse.

**Why it matters.** It is the difference between sprint 27 delivering an economy and delivering five more inert channels. BL-641 is the worked example of getting it wrong: the shape was correct, verified, era-banded, and it had to ship switched off because a channel that consumes without pricing starves the firms it draws from.

- A demand channel MUST register a market want. Pool draws become market demand, so wanting a good raises its price and induces the supply that meets it. Matches what run_construction and run_processing already do, and what MARKETS.md arguably already requires.
- Keep pool draws, and break the deadlock the BL-440 way - take the pull from the RECIPE/UPKEEP GRAPH as a static world fact rather than from the price signal. Precedent exists, is deterministic, and is self-limiting.
- Keep pool draws and accept that upkeep channels never induce supply - they are a tax on holding assets, not a demand channel, and MARKETS.md's eight-channel register should say so.

> **Recommendation:** Option 1, with option 2's self-limiting shape borrowed if the price signal proves too slow. A channel that consumes without pricing is not a demand channel in any sense MARKETS.md means - it is an upkeep tax wearing the word - and the census already lists both pool draws in a separate OFF-REGISTER column for exactly that reason, which is the document telling us the answer. Option 3 is honest but gives up the coupling that makes demand interesting: it is the choice to have sinks rather than buyers.

> **RESOLVED.** WITHDRAWN. The question was already settled and already owned, and I filed it anyway.

BL-654 (a channel must bid) exists, is status `designed`, and carries Ben's ruling from 2026-08-26: "Buy on the market, but at a threshold, buying is not allowed. This goes hand in hand with maximum and minimum prices for goods." ONE RULE FOR EVERY GOODS DRAW - unit upkeep takes the same shape, not a second one. It is written into MARKETS.md § Settled.

THE WORK, per that item: a short pool bids the shortfall into market_component::demand and PAYS for what it gets; above a reservation ceiling it does not bid at all and the existing shortfall rule weakens the building or unit instead. The ceiling is authored in the PRICE BAND family beside floor_mult/ceil_mult - a statement about what a good is worth paying, not about who is buying - and its value is MEASURED against the census and the operating-firm count, never guessed. It is the exact mirror of BL-386's seller-side floor_price: both sides may decline a trade, neither may dictate one.

BL-654 also already anticipated the sharper half of the question I was going to ask: whether a short pool BUYS (spending credits, making upkeep a market participant) or merely SIGNALS (raising price without a transaction). Its own note calls the second "a thumb on the scale [that] should probably be refused on the same grounds the project refuses clamps", and Ben's ruling took the first.

HOW I GOT HERE, recorded because it is a repeatable mistake. I read the open question in scripts/economy.lua's BL-641 comment - which is genuine and correctly says the call is Ben's - and filed it without running backlog_query first. The comment was written BEFORE the ruling and was never updated, so the file still reads as open while the backlog has the answer. Both my own memory and CLAUDE.md say to check the owning store before trusting a filed premise; I checked the code and not the store.

NOTHING IS LOST: the analysis stands and is reproduced in BL-654's own prose. The only cost was a redundant entry and a wrong first line in sprint 27's plan, both corrected.

*Files: `scripts/economy.lua`, `src/world/economy_system.cpp`, `src/world/market_clearing.cpp`, `docs/economy/MARKETS.md`*

### NR-761 — Can a corporation SELL power to another corporation? YES - and internationally
*question · raised 2026-08-31 · from Sprint 27 design session on power, 2026-08-31. Ben answered how power MOVES; this is the half his answer leaves open.*

Ben settled that power is "not a resource" and that it travels on the road network with a flat one-tick latency to anything connected. What that leaves open is whether power is ever SOLD.

THE DESIGN AS WRITTEN SAYS NO: a corp generates for its own connected network, and the market demand power creates is the generator's FUEL purchase, not a trade in power itself. That reading is what makes "not a resource" coherent - no order book entry, no price, no convoy.

IT IS ALSO A REAL CONSTRAINT ON THE PLAYER, which may be exactly right or may be too harsh. A corp with no fuel access and no generation is simply short of power, and cannot buy its way out. That is asymmetry with a cause, which GENERATION_STRATEGY.md § Asymmetry is the deliverable actively wants. But selling power is also an obvious business, and a world where nobody can is a world missing a firm type.

THE SHAPE IF THE ANSWER IS YES, and it is not a market good: power is a standing relationship over a CONNECTION, not a spot trade in a fungible cargo - so the natural home is a CONTRACT (docs/economy/CONTRACTS.md, the BL-350 procurement seam) rather than the order book. A supply agreement between two corps whose networks touch, priced per tick, terminable. That keeps power off the market while making it sellable, and it reuses a seam that exists.

NOT URGENT. BL-708 (power as a grid) is coherent and buildable without it, and the fuel-chain endpoint - which is the whole reason power is being built this sprint - does not depend on the answer either way.

**Why it matters.** It decides whether power is infrastructure a corp builds for itself or a commodity with a market. The first is simpler and creates sharper regional asymmetry; the second adds a firm type and a reason to build generation beyond your own needs. Cheap to answer now, and awkward once generation buildings are tuned against one assumption.

- NO - power is infrastructure you build for yourself. Simplest, sharpest asymmetry, and what the design as written assumes.
- YES, via CONTRACT - a standing supply agreement between connected corps through the existing procurement seam. Keeps power off the order book while making it sellable.
- YES, as a market good - overturns "not a resource" and needs power to acquire a price, a reach model for trading it, and an answer to what a convoy carrying electricity means.

> **Recommendation:** Build BL-708 on option 1 and leave the question open - the item is coherent without it and nothing about the fuel endpoint depends on it. If it turns out that power-poor regions feel stuck rather than challenged when watched, option 2 is the answer and the contract seam already exists to carry it. Option 3 should probably be refused: it reintroduces every question 'not a resource' was chosen to avoid.

> **RESOLVED.** ANSWERED by Ben the same day, and it overturned the design I had just written.

"I believe we need to distribute power across all industry. This means that it has to be a bought good when it is taken as upkeep. Therefore corporations can buy power from each other, and background companies can produce power with a profit." And, immediately after: "Even internationally."

SO IT IS OPTION 3, WHICH I HAD RECOMMENDED REFUSING - and the recommendation was wrong. I argued option 3 "reintroduces every question not-a-resource was chosen to avoid". It does not, because the thing that made power awkward as a good was never the PRICE, it was the CONVOY. Power keeps its grid transmission - roads, flat one-tick latency, connection-gated matching - and simply also has a price. Movement and market are separate questions, and only the movement needed to be special.

WHY BENS VERSION IS BETTER, and it is not close. As private infrastructure, power demand is bounded by each corp own generation - a small closed loop that adds one sink per generator. As a bought good, EVERY BUILDING ON THE MAP IS A BUYER, which is the economy-scaled sink demand_census says is missing. It also makes generation a BUSINESS rather than a cost centre, so background firms build power plants at a profit and the supply is induced by price the way every other goods is. And it removes the harsh consequence of the private reading, where a corp with no fuel access was simply stuck rather than able to buy.

INTERNATIONAL TRADE NEEDS NO NEW MACHINERY. MARKETS.md section Tariffs already resolves a market to a jurisdiction and charges the enacted import duty on any matched trade whose buyer is domiciled outside it. Cross-border power is an ordinary cross-border sale.

AND IT REACHES THE ERA CATASTROPHE. ERAS.md section The point of an Era scores Era 1 on Alarm, and its danger table names cross-border trade as the cheapest Alarm suppressant in the game, with Autarkic Substitution as the herring that cuts those ties. A nation whose lights depend on a neighbours generation has a reason not to escalate; severing that tie is a visible causal step toward the rupture. Power now feeds Trade and Conflict at once, which is the SYSTEMS.md test and very few systems pass it on both sides.

*Files: `docs/economy/PRODUCTION.md`, `docs/economy/CONTRACTS.md`, `docs/economy/LOGISTICS.md`*

### NR-765 — THE SAME DEFECT, THREE TIMES: an argmax on an absolute quantity, in a domain whose values span orders of magnitude
*observation · raised 2026-08-31 · from Sprint 27 waves 1 and 2. BL-707 diagnosed one instance; BL-708 hit a second building on top of it; BL-440 had already fixed a third.*

Three separate symptoms this sprint traced to one shape. Naming it once is worth more than three fixes.

THE SHAPE: a pre-selection step ranks candidates by ONE ABSOLUTE QUANTITY and truncates. Where that quantity's values span orders of magnitude, the truncation is not a ranking - it is a CATEGORY EXCLUSION. Everything below the top band never reaches scoring at all, so the scorer looks correct while never having been offered a real choice.

INSTANCE 1 - BL-440, already fixed, and it named the trap in its own comment. Extraction site selection pre-picked a tile's RICHEST deposit. Its fix comment reads: "Pre-selecting the richest was a TILE-LOCAL heuristic answering a WORLD-level question."

INSTANCE 2 - BL-707, diagnosed this sprint. `rank_extraction_sites` enumerates every deposit per tile (BL-440's fix working) and then truncates to a GLOBAL `top_m_sites = 8` by deposit x affinity x demand_weight. All eight slots are iron_ore, cut-off 7847.5, against clay's best 205.7 and peat's 75.0 - a 60x gap that `input_demand_pull` (bounded ~9x) cannot close. So BL-440 fixed the tile-local version and RE-SET THE IDENTICAL TRAP ONE ALTITUDE UP.

INSTANCE 3 - found by BL-708 while building power. `corp_ai.cpp:1071` picks the max net margin per site: `if (n > best_net ...)`. Power's net is 3.98; price-ceiled electronics is 290. **The corp AI can never build a power plant**, in any world, at any time. The first attempt at BL-708 collapsed the industrial band to 25 of 407 operating firms with power produced 0.0, purely because of this.

WHY IT KEEPS RECURRING, and this is the part worth carrying: each instance is locally reasonable. "Take the richest tile", "take the top 8 sites", "take the best-margin recipe" are all sensible sentences. The defect is invisible until you ask what the DISTRIBUTION of the ranked quantity looks like - and in this economy it is always long-tailed, because deposit magnitudes, margins and prices all span three orders.

A CHEAP GOOD CAN NEVER WIN AN ABSOLUTE-MARGIN CONTEST, however badly the world needs it. Power is the clearest case: it is wanted by every building on the map and it is worth 3.98 a unit.

THE FIXES ARE THE SAME SHAPE TOO: rank scale-free (normalise within resource or within category), or take a per-category top-K instead of a global top-M, so every category is represented in the candidate list and the SCORER decides - which is what the scorer is for.

**Why it matters.** It is the root cause of two of this sprint's three biggest findings, and it will silently break the next channel too. Power only works today because the SEEDER places plants; the AI cannot add one, so power supply never grows with the world - which defeats the economy-scaled sink the channel was built to be.

- One item fixing the shape wherever it appears - per-category top-K in the extraction pre-filter AND scale-free recipe selection. Determinism-affecting and moves every economy golden, so it wants its own sprint slot.
- Fix only the recipe-selection instance now, since it is what makes BL-708's power supply unable to grow, and carry the extraction one as BL-707's handover already proposes.
- Carry both as documented debt with the shape recorded here, and fix when a measurement shows it costing something beyond what is already measured.

> **Recommendation:** Option 2, then 1. The recipe-selection instance is the one that makes a LANDED feature structurally incomplete rather than merely suboptimal - power exists in the world only because generation placed it, and no rival will ever build another. That is worth fixing before the sprint's own construction item lands on the same scorer. The extraction instance is bigger and already has a handover.

> **RESOLVED.** RESOLVED 2026-09-01 — all three instances are fixed and the shape is now a written rule rather than a recurring surprise.

Instance 1, BL-440 (tile-local richest): already fixed before this entry was filed.
Instance 2, BL-711 (global top-M extraction list): `rank_extraction_sites` keeps a per-resource top-K. Coal went from 0 mines in any world ever to 25 on the industrial band; clay, hides and sand went from `produced 0.0` to 349.1 / 353.9 / 35.8 on the ancient band.
Instance 3, BL-712 (max-net-margin recipe): the build candidate keeps a best per `recipe::group`. `Power Generation` and `Construction` went from ZERO build candidates to 168 and 430.

THE ENTRY'S REAL ASK — 'naming it once is worth more than three fixes' — is met: the shape is written into `docs/ai/AI_OPPONENT.md` § Selection must be scale-free, as a rule with its own reasoning, so the next instance is a rule violation rather than a fresh discovery. BL-712 also added a fourth sibling the entry did not know about: the recipe margin-chase, which carried the same argmax AND proposed cross-group switches the seam has refused since 2026-08-16 — that one is written up beside the rule as 'a scorer that proposes what its seam forbids cannot tell a refusal from an absence'.

WHAT DID NOT GET FIXED, and it is filed rather than folded in: NR-769. Both fixes put the excluded categories in front of the scorer and the scorer still refuses them, because `net^2/capex` is itself an absolute contest — the entry's own sentence, 'a cheap good can never win an absolute-margin contest', applies one level below where it was aimed. That is BL-417 and Ben's.

*Files: `src/world/corp_ai.cpp`, `src/world/corp_ai.hpp`, `tools/verify/chain_conversion_probe.cpp`*

