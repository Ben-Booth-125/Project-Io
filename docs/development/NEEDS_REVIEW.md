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

*6 entries — 5 open, 1 resolved.*

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

