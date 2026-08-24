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

*18 entries — 16 open, 2 resolved.*

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

### NR-616 — ai_skill_harness's five golden bands are already red on main -- unrelated to BL-602 (Port gates the sea leg)
*observation · raised 2026-08-24 · from BL-602 (SEA_PORT_GATE) golden-churn verification pass.*

Building BL-602 (the Port-building sea-mode gate in price_convoy_leg, src/world/supply_system.cpp), I ran ai_skill_harness as a golden-churn check. All five seeds fail net-worth-final, net-worth-min and solvency (net worth reads roughly -3.1M to -4.9M against golden bands blessed 2026-08-16), plus several dial/build thrash-ceiling rows. I re-ran the identical harness against the UNMODIFIED main tree (git stash of my one changed file) and got byte-for-byte the SAME 20 failures -- so this is pre-existing golden drift, not something BL-602 caused. Flagging rather than silently re-blessing, per the golden-churn discipline; I made no attempt to fix or re-bless it, since it is out of BL-602's scope.

**Why it matters.** The 2026-08-16 bands (BL-436) are stale against whatever landed since -- possibly BL-573/BL-574 (mercenary contracts), the N-series nation grants, or BL-586's roster widening, any of which could move rival net worth. Whoever owns ai_skill_harness next should re-derive the bands from a fresh bless rather than assume BL-602 (or any other recent item) broke them.

*Files: `tools/verify/ai_skill_harness.cpp`*

### NR-617 — build_harness.js's LUA_TUS exclusion list is stale -- contract_template.cpp needs sol2/lua54 but is not excluded
*observation · raised 2026-08-24 · from BL-602 (SEA_PORT_GATE) build pass -- the headless one-line builder failed on the world-superset TU set.*

tools/verify/build_harness.js globs every src/world/*.cpp minus LUA_TUS = {recipe_registry, works_registry, tech_tree, world_gen_config} and documents that set as 'the four sol2/Lua TUs io_world_obj excludes'. contract_template.cpp #includes lua_state.hpp (which pulls sol/sol.hpp) and is NOT in that set, so the one-line builder fails to compile (no sol2 on INCLUDE) and, once sol2's include dir is added by hand, fails to LINK (no lua54.lib named at all -- the script's cl invocation carries no /link section). I routed around it for BL-602's sea_port_gate.cpp by hand-adding sol2_src/include + lua_src to INCLUDE and appending build\lua54.lib on the link line outside the tool. Did not touch build_harness.js itself -- out of BL-602's scope and I did not want to guess at the intended fix (exclude contract_template.cpp like the others, since nothing here needs to actually LOAD a script; or thread a real lua54.lib link path through for every TU that only compiles against the headers).

**Why it matters.** Every session that reaches for this builder against a harness pulling in contract_template.cpp (any world-superset harness, which is most of them) hits the same wall and has to rediscover the same workaround. It is the documented 'one-line builder' (NR-392) silently failing on a large slice of its own advertised surface.

*Files: `tools/verify/build_harness.js`*

### NR-618 — merge/17b-onto-main holds a complete, unmerged Sprint 17b (UI sweep) — a concurrent session finished it tonight and it is not on main
*observation · raised 2026-08-24 · from Discovered indirectly: four Sprint 18 worktree agents (BL-599/600/601/602/603) all forked from a base other than main tip, whose history includes bd3f3431 ("Merge Sprint 17b (the shell stops fighting the map) into main"), reachable only via the branch merge/17b-onto-main — not from main. Investigated rather than assumed: this is a real, deliberate, well-documented merge by another concurrent session, not stale/lost data.*

merge/17b-onto-main (tip bd3f3431, 2026-08-24 22:41:48) merges a complete UI sweep — icons, lenses, the Selection band accordion, a shell-pass verification script, ~72 files — cleanly resolving nine NR-id collisions against this session's own concurrent NR-589..597 filings by renumbering the 17b side to NR-607..615. It sits sandwiched between two of THIS session's own commits (93b38dc5 at 21:32 and c50a9a7c at 23:17) and was never fast-forwarded or merged into main itself. Every Sprint 18 wave-1 worktree agent (four separate Agent tool calls, each with isolation:"worktree") independently forked from this branch's tip rather than main's — meaning worktree isolation picked up whatever HEAD the shared checkout carried at creation time, not necessarily main. No work was lost or corrupted: each agent's OWN commit was cherry-picked onto main individually rather than trusting a wholesale merge, specifically because this contamination was caught.

**Why it matters.** Two things. (1) A full session's worth of real UI work (Sprint 17b) is sitting ready and unintegrated — whoever owns it may still want to merge it, or may be waiting on review. (2) The worktree isolation mechanism forking from a non-main HEAD is a process risk worth knowing about for future sessions: it is exactly the shared-checkout hazard CLAUDE.md already warns about ("this checkout is shared with other sessions"), now observed concretely rather than theoretically.

- Merge merge/17b-onto-main into main directly (if that session is done and it is ready)
- Leave it for that session/Ben to merge deliberately
- Investigate why worktree isolation forked from it rather than main tip, as a tooling fix

> **Recommendation:** Leave the branch alone — it is not this session's work to merge unreviewed. Worth a deliberate merge decision the next time a session is free to review Sprint 17b's content on its own terms, not folded into Sprint 18's wave-1 integration.

### NR-619 — BL-596 active LP: nearest-anchor rule and refusal surfacing are reasoned interpretations, not lookups
*decision taken on your behalf · raised 2026-08-25 · from BL-596 (LP_ACTIVE_MARCH) landing — LOGISTICS.md does not name the mid-route anchor-attachment rule or a narration pathway for a refused march.*

Two calls made so the item could land: (1) a marching unit draws from the anchor with the LOWEST intra_body_path cost from its CURRENT position, recomputed fresh each tick (never cached per-unit) — reusing the exact anchor enumeration body_reach_field seeds its Dijkstra from. (2) a refusal is surfaced only via unit_march_tick::refused_no_lp, a counter, matching the marching/arrived/recomputed style — this codebase has no existing per-unit-movement narration pathway (no agency_event/world_history_entry equivalent fires from run_unit_march today), so no comms/UI surface was invented for this first cut.

**Why it matters.** LOGISTICS.md (Refusal, surfacing and determinism) requires refusal to be non-optional and legible to the player — a counter on a return struct nobody currently reads UI-side is real but not yet PLAYER-visible. BL-598 (the Throughput lens) is the named surface; until it lands, a refused march is legible only to a harness, not to Ben in play.

- Leave refused_no_lp as the sole signal until BL-598 lands the lens
- Add a lightweight comms-log line now (piggybacking the pattern BL-458 interdiction uses) ahead of BL-598

> **Recommendation:** Leave it — BL-598 is already scoped to be the throughput surface, and a second ad-hoc narration path now would likely be thrown away when it lands.

*Files: `src/world/economy_system.cpp`, `src/world/economy_system.hpp`, `src/world/logistics.hpp`, `src/world/logistics.cpp`, `docs/economy/LOGISTICS.md`*

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

### NR-598 — sprints.json's status_table half-contradicts SPRINTS.md, and four surviving entries appear in neither table
*observation · raised 2026-08-24 · from Rendering the Sprint Map artifact against the canonical sprints array surfaced the drift. Two mechanical fixes applied in the same pass: SPRINTS.md's row 17 and Next-up paragraph (said 16+17 active; both closed 2026-08-24) and status_table's row 16 (said open) — both now match the canonical array and the v0.1.15/v0.1.17 tags.*

Three things remain that want a call rather than a fix. (1) status_table still carries 34 rows including the sprints deleted outright in the 2026-08-24 purge (20, the 26–33 arc, etc.) — the purge removed their `sprints` entries but not their status rows. (2) Sprint 27 (the run is retained, Lane A, opened 2026-08-18, closed 2026-08-20) is a real executed sprint in the canonical array, yet '27' sits in SPRINTS.md's deleted-numbers list and the sprint appears in NEITHER status table — it looks like the Fall-arc proposed 27 was deleted by number while the executed 27 survived, and the tables never picked it up. Its retro fields are also empty strings ('' for landed/slipped/feedback), as are P1's, N1's and N2's. (3) Sprint 25b (gated) survived the purge despite never opening, and its settled gate sequence (25a → 21 → 23 → 25b, Ben 2026-08-17 on NR-312) now points at numbers 21 and 23 that the purge deleted.

**Why it matters.** The status tables are the read-first surface for 'where do sprints stand'; two sessions today each fixed one half and left the other half stale in the opposite direction. And 25b cannot open as sequenced — its prerequisites no longer exist as sprints.

> **Recommendation:** Rule on three small things: (a) purge the deleted sprints' rows out of status_table (mechanical once blessed — or bless a tiny regeneration script so the two tables stop drifting independently, per 'save the tool'); (b) say whether executed-but-unlisted entries (27, 18, 18 retro, 25b, 26) should appear in the tables — the Sprint Map artifact now shows all 30 either way; (c) either re-sequence 25b against the current board or park it explicitly in backlog.json like BL-518/BL-514.

> **RESOLVED.** Ben, 2026-08-24: "Let's move sprints into JSON. We also need to archive them once they're complete." Done same session: status_table retired in favour of a per-entry status_line (one entry, one line, nothing to drift); SPRINTS.md is now a generated mirror (tools/session/render_sprints.js) reading sprints.json plus archive/sprints-*.json, so every executed sprint — 27 included — is indexed; 29 completed sprints moved cold via tools/session/archive_sprints.js (hot file 128.7 KB → 12.6 KB). The empty retro fields on 27/P1/N1/N2 are frozen in the archive as found. The one remaining fork — gated 25b's sequence pointing at purged numbers 21/23 — is carried into the Sprint 18 logistics design, in flight this session.

*Files: `docs/development/sprints.json`, `docs/development/SPRINTS.md`*

