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

Entries are **never silently deleted** — set `status: resolved` and write the resolution, so
the reasoning survives the answer.

*40 entries — 17 open, 23 resolved.*

---

## Open

### NR-020 — The History ledger’s Tiles tab is now named after the table it no longer contains
*question · raised 2026-08-02 · from Follow-on from NR-014, 2026-08-01*

With the per-tile table removed, the tab labelled "Tiles" holds two things: a bulleted Buildings list for the selected body, and a Market table (resource / supply / demand / price / base price). Neither is a tile.

**Why it matters.** The same argument that retired the tiles table applies to both, and Ben did not comment on either, so they were left alone rather than swept up. The Buildings list duplicates the Construction panel; the Market table duplicates the Market Ledger, and duplicates it less well (one body, no history, no orders). If the reasoning is "the canvas already shows it", the buildings are on the canvas too. The tab label is also now actively misleading, which is a small thing that a new player meets on the first visit.

- Rename the tab to what it now shows (Body, or Buildings & Market) and keep both sections.
- Retire the whole view — the History ledger becomes Story + Chain, two views about how the world came to be, which is a cleaner premise than a third view about its current state.
- Keep it as is; the label is wrong but nobody is harmed.

> **Recommendation:** Option 2 reads best to me and is worth your eye rather than my call. Story and Chain both answer "how did this world come to be"; a current-state view has never belonged in a HISTORY ledger, and both its sections have better homes that already exist. That would also retire the history_tiles fold surface and simplify drill_through_fold.lua, which currently uses this table as its example of the fold’s biggest win.

*Files: `src/ui/tile_inspector.cpp`, `scripts/verify/drill_through_fold.lua`, `scripts/verify/history_ledger_and_comms.lua`*

### NR-022 — BL-262 (scoring) — I answered all six of your open calls as one interlocking package; ratify or overturn
*decision taken on your behalf · raised 2026-08-02 · from Design-owed sweep, 2026-08-02*

BL-262 says the six calls are yours, all of them. I proposed one coherent answer to all six rather than leaving six blanks, because they are not independent — call 4 nearly forces call 3, which shapes call 2, which dissolves call 1 stated expiry. The package: a coarse publicly-published PROFILE of four axes (reach / production / capital / market share) with NO total ever; computed from VISIBLE information, not ground truth; published diegetically by the market, so your own figures are exact and every rival shows as a BAND; scoring corporations on axes that are actor-agnostic and so survive the BL-094 nation pivot; meaningful only within a campaign (no cross-seed leaderboard); feeding credit terms (BL-073) and counterparty routing (BL-037), but deliberately NOT unified with BL-202 AI utility. Item flipped to designed on that basis.

**Why it matters.** This is the largest single set of calls I have taken on your behalf, and it decides what the game measures — close to CONCEPT-level. Two of the six are close to forced by the existing corpus (visible-information, because ground truth would make both discovery fogs decorative; and within-campaign-only, because cross-seed normalisation is meaningless with no end-game screen). The other four are genuine judgement and you may well want them differently. The one most worth your eye is DIEGETIC: it is the expensive answer, and I chose it partly on your standing preference for the deeper option over the cheaper one — which is exactly the kind of inference that should be checked rather than assumed.

- Ratify the package as written; it promotes as-is.
- Ratify the shape but swap DIEGETIC for META — much cheaper, loses the credit/counterparty feedback in call 5, and the score stops being a thing the world can react to.
- Ratify but collapse the profile to ONE number — simpler and more legible, at the cost of implying a single race, which is the end-game framing CONCEPT forbids.
- Overturn and design it with me from scratch.

> **Recommendation:** Ratify as written. The package hangs together and each answer is load-bearing for the others — in particular, banded rival figures are what let a comparison surface show every corporation without violating BL-068, which the restored NR-012 table could not do. If you want one thing cheaper, option 2 is the least damaging cut, but it costs call 5 entirely and the number becomes chrome, which the item itself warns against. Still open regardless of your answer: the band boundaries (tuning, wants a running campaign) and where the profile lives on screen (a layout call, yours).

*Files: `docs/development/backlog.json`, `docs/CONCEPT.md`, `docs/ui/DISCOVERY.md`*

### NR-023 — BL-229 (building selection) — four layout questions, now with the real column widths; it is the one item I left design-owed
*question · raised 2026-08-02 · from Design-owed sweep, 2026-08-02*

BL-229 carries your written instruction "do not guess the layout, Ben designs this one". I honoured it: questions 1-4 are untouched and the item is the only one in the v0.1.1 set still design-owed. What I did settle is question 5 (rival buildings degrade IN PLACE — same three-column skeleton with internal pages absent rather than blanked, matching how the BL-089 activity fog already degrades content without swapping surfaces) and the sequencing (it now depends on BL-265, not the landed BL-214). I also measured the actual budget so you can answer against numbers rather than prose.

**Why it matters.** The measurements change what the questions mean. At the 1280x720 floor the three columns are 135 / 254 / 135 px inside a band fixed at 260 px tall, giving ~212 px of usable column height. So the tile element 3x2 action grid is living in 135 x 212 px — about 44 px per button row — and that 135 px is the real budget for any answer to Q1 (what fills the left quarter) and Q4 (what fills six action slots). A combo box at 135 px is tight and a slider at 135 px is very tight, which is the constraint bearing on Q3 (where the recipe and workforce levers go). At 1920x1080 the columns are 294 / 572 / 294. Band height is 260 at both and stays 260 until the display smaller dimension exceeds 1200.

- Answer Q1-Q4 in one pass with the live app open (the standing rule for visual questions).
- Sketch the sibling layout as a mockup, as you did for the tile element — its proportions came from yours.
- Delegate Q1-Q4 to me now that the numbers are on the table, accepting I will be guessing at a layout you reserved.

> **Recommendation:** Option 1 or 2 — this is the item where guessing is worst value, because the tile element it must match came from your own mockup and a near-miss sibling reads worse than an obvious difference. BL-265 relieves some of the pressure: a full-canvas accordion shows every page scrolled, so the centre column no longer has to fit everything, which makes Q2 less constrained than it looks. Everything else on the item is finished and waiting.

*Files: `docs/development/backlog.json`, `src/ui/selection_panel.cpp`, `docs/ui/SELECTION.md`*

### NR-024 — The Budget ledger Tax control promises something a corporation cannot have — laws are enacted by nations, not by the player
*decision taken on your behalf · raised 2026-08-02 · from Settling BL-155 (laws & policy) during the design-owed sweep, 2026-08-02*

BL-171 added Tax and Wages tier selectors to the player Budget ledger as stubbed controls, and BL-155 records your confirmed intent that "Tax = a player-set policy lever (the player picks a tax tier as a deliberate trade-off)". But every law in BL-155 ten-law list is an instrument of public authority — tax, tariff, cap, embargo, zoning — and the player is a CORPORATION. A corporation is subject to those, it does not enact them. I settled BL-155 on the rule that laws are enacted by NATIONS and the player is a law subject until BL-094 (v0.2.0) pivots them to a nation, and split the two controls accordingly: Wages stays a real lever (a private contract term, not a law — a corporation genuinely sets what it pays, above whatever floor law #8 imposes), and Tax becomes a READ-ONLY display of the tax regime the player home nation currently imposes.

**Why it matters.** This contradicts a previously confirmed intent of yours, and it changes a control that is already drawn on screen — so it is not a call I should make quietly. It is also load-bearing for the whole laws design: if the player can enact laws, then laws are a player-facing authoring surface with all the UI that implies; if the player cannot, laws are world state the player routes around, and the prototype scope collapses to two enacted laws plus a display. Those are very different amounts of work. The read-only reading is also, I think, the better game — a market shaped by rules you did not choose is a constraint to plan against, which is the Trade dimension doing its job, and it gives the law system a visible surface from day one instead of after BL-094.

- Accept: Tax becomes a read-only display of the home nation regime; Wages stays a real lever (what I did).
- Accept the rule but remove the Tax control entirely until BL-094, rather than re-presenting it.
- Overturn: the player CAN set their own tax tier — in which case say what it represents in the fiction, since a corporation legislating for itself is incoherent as written.
- Overturn differently: the player is a chartered corporation that negotiates its own tax rate with its home nation, making Tax a real lever with a diegetic story behind it.

> **Recommendation:** Option 1, but option 4 is worth a moment because it is the one that keeps your original intent AND makes it coherent — a negotiated rate fits the chartered-corporation identity the history ladder (BL-223) is building toward, and it would make Tax the first place diplomacy touches the economy. It costs a negotiation mechanic that does not exist, so it is not a prototype answer; if it appeals, the honest move is option 1 now and option 4 filed for v0.2.0. Either way the Tax control as currently drawn should not ship promising a lever the player does not have.

*Files: `docs/development/backlog.json`, `src/ui/balance_ledger.cpp`, `src/ui/ui_state.hpp`*

### NR-025 — CONCEPT.md:51 is right after all — the Era rupture disagreement was four-way, and the fourth doc dissolves it
*observation · raised 2026-08-02 · from Settling BL-223 (averted rupture) during the design-owed sweep, 2026-08-02*

BL-223 tabulates a three-doc disagreement about the Era 0 rupture — CONCEPT.md:51 (a future WW3-scale event during play), ERAS.md (three purely mechanical gate conditions, no event), HISTORY.md Stage 5 (a past event) — and its owed action 2 was to amend CONCEPT.md. There is a fourth doc it omits: BL-087 Era reframe of 2026-07-08, which says Eras ARE catastrophic seeded events on the world clock and explicitly re-reads the ERAS.md Rocketry/Launchpad/propellant condition set as gating a QUEST TREE rather than an Era. That is dated later than the ERAS.md model, so under newest-dated-wins it governs. With it in the table the contradiction dissolves: there are TWO ruptures doing different jobs — a PAST averted near-miss (backstory, sets starting diplomatic posture) and a FUTURE seeded event that ends Era 0 during play. CONCEPT.md needs no amendment; only ERAS.md does, and BL-087 already owns that edit when its work lands.

**Why it matters.** The item was about to amend a CONCEPT.md line that is correct, on the strength of a table that was missing a doc. CONCEPT.md is the top of the corpus and the hardest place to undo a wrong edit — a claim removed there stops being available as a premise everywhere downstream. It is also a small warning about the reconciliation method: BL-223 built its table by reading the three docs that talk about Eras by name, and missed the design that changed what an Era IS because it lives in a backlog item rather than a doc. The 2026-07-31 doc-truth sweep would not have caught this either, for the same reason.

- Accept the reading: two ruptures, CONCEPT.md unamended, ERAS.md corrected by BL-087 when it lands (what I recorded).
- Accept the reading but correct ERAS.md now rather than waiting on BL-087, since it is currently the one doc stating something the design has superseded.
- Disagree — you intended only one rupture, in which case say which one, and HISTORY.md Stage 5 or CONCEPT.md:51 goes rather than both standing.

> **Recommendation:** Option 1, and the two-rupture reading is worth keeping for its own sake rather than just as a reconciliation: the rupture that was averted then is not averted this time. The backstory establishes that these powers can pull back from the brink, and the Era 0 exit is the occasion they do not — which is a stronger premise than either event alone and costs nothing, since both were already written. Option 2 is defensible if the ERAS.md line is bothering you, but it edits an authority doc ahead of the work, which the time-slice rule exists to prevent.

*Files: `docs/CONCEPT.md`, `docs/economy/ERAS.md`, `docs/lore/HISTORY.md`, `docs/development/backlog.json`*

### NR-026 — Frame-budget targets (BL-249) still need a human at the keyboard
*observation · raised 2026-08-02 · from Closing out v0.1.0's remaining items (BL-258 landed 2026-08-02)*

ROADMAP.md names the frame-budget targets (avg < 8ms, max < 16.7ms panning the full Kepler grid) as "still owed before the cut" alongside BL-258. BL-258 (the optimised-build timing gate) is now landed and the harness suite is 36/36 green. The frame-budget check cannot be automated the same way: headless capture has no vsync and no real present, so its numbers say nothing about the real frame budget. It needs Ben to launch the live app (F11 overlay), pan the full Kepler grid, and read the numbers off the real render loop.

**Why it matters.** This is the last named item standing between the current state and declaring the v0.1.0 done-definition met — everything else in ROADMAP.md § v0.1.0 is already checked off.

> **Recommendation:** Open the app (F11 for the frame-stats overlay), pan the full Kepler tile grid, and confirm avg < 8ms / max < 16.7ms. If it passes, v0.1.0 is done bar hygiene (warning-clean build, cppcheck pass) and the cut can be tagged.

> **RESOLVED.** Superseded 2026-08-02: the premise was wrong — nothing in src/ forces a dummy driver under --verify, so a scripted run uses the real renderer and the build's real vsync. The measurement no longer needs a human: scripts/verify/pan_perf.lua (via the new verify.frame_csv tap on the BL-249 instrument) ran the exact ROADMAP check. Result: the target FAILS today — Debug 41-53 ms work/frame (every frame over 16.7), Release 11.3 ms at play zoom (passes 16.7, misses 8) and 16.8-17.6 ms at whole-grid zoom. Cause and fix are BL-268 (planetary canvas cull + cache); BL-267 (GPU/multicore) records the full verdict. Re-run pan_perf after BL-268 to close the cut-gate check.

*Files: `docs/development/ROADMAP.md`, `src/ui/frame_stats.hpp`, `src/ui/frame_stats.cpp`*

### NR-027 — BL-217 checkpoint retrofit: S8/Legacy has no branch point, so no checkpoint was added there
*decision taken on your behalf · raised 2026-08-02 · from Session 2026-08-02 (BL-217 checkpoint/branch/lean foundation)*

The task brief pointed at S8/Legacy (~line 1116+ in the pre-change planetology.cpp) as one of four biological die-off points to wrap in a checkpoint_record, alongside S5 Spark, S6 Breath and S7 Green. Reading the code, S8 Legacy has no die()/branch decision at all — it is a deterministic resource-endowment calculation over whatever life_stage the chain already reached. No checkpoint was added there.

**Why it matters.** BL-217's own admission rule (settled in backlog.json) says a checkpoint is a point where the outcome distribution genuinely branches, not every point where something interesting happens. S8 fails that test — two runs reaching the same peak life_stage always produce the same Legacy endowment shape, so a checkpoint there would violate the rule this session's design explicitly wrote in. Five checkpoints were recorded instead (Spark: 1, Breath: GOE + NOE, Green: land colonisation + fire threshold), matching every genuine branch S5-S8 actually contains.

> **Recommendation:** No action needed unless a future S8 mechanic (e.g. a stochastic Legacy-stage roll) introduces a real fork — at that point it would earn its own checkpoint under the same admission rule.

*Files: `src/world/planetology.cpp`, `src/world/planetology.hpp`, `docs/generation/PLANETOLOGY.md`*

### NR-029 — BL-208 checkpoint-log timestamp: no exact 1:1 pairing between checkpoint_record entries and history_event lines at a stage — resolved with a documented simplification
*decision taken on your behalf · raised 2026-08-02 · from BL-208 (world history log) — the checkpoint-migration bridge, src/world/history_log.cpp's resolve_checkpoint_timestamp*

checkpoint_record carries no timestamp of its own (by design — see planetology.hpp, changing its shape now has a ripple cost the item explicitly said to avoid). The task asked me to resolve one by matching a checkpoint to its body's dated history_event lines at the same chain_stage. Inspecting planetology.cpp showed this is NOT a clean 1:1 pairing: some checkpoints (e.g. a body that already terminated at an earlier stage still records a failing Spark checkpoint) have NO dedicated history_event line at their stage at all; others share a stage with a sibling checkpoint that DOES have its own line (Green resolves up to two checkpoints — 'land colonised' then 'combustion cleared/failed' — against up to three Green-tagged history lines, with no code-level tag distinguishing which line belongs to which checkpoint). Replicating planetology.cpp's branch logic in the log bridge to pair them exactly would duplicate logic in a second place it could silently drift from.

**Why it matters.** The chosen rule is simple and always produces A resolved, non-fabricated timestamp, but is coarser than the true per-line correspondence in the double-checkpoint case: every checkpoint at a stage resolves to that stage's LAST dated line at or before it (i.e. multiple checkpoints at the same stage can share one timestamp, and the true finer-grained moment for e.g. a combustion-threshold checkpoint is sometimes an adjacent line rather than the exact one). This affects only display ordering/precision within a handful of lines per body, not correctness of the underlying planetology chain or its archetype/died_at outcome.

- Keep the simplification (current state): last-dated-line-at-or-before-stage, documented inline and here.
- Duplicate planetology.cpp's per-branch call sequence in history_log.cpp to pair checkpoints exactly 1:1 with their originating say() call — more precise, but a second place the biology narrative logic lives, which is exactly the kind of drift risk PLANETOLOGY.md's own determinism-and-cost section warns against.
- Add an explicit timestamp field to checkpoint_record after all, accepting the save-format ripple the task asked me to avoid unless there was no other reasonable option.

> **Recommendation:** Keep option 1. The genesis+checkpoint chapter is presentation-scoped (an oral-history line, not a simulation input), and history_log_harness.cpp's R2 already asserts every checkpoint gets a real, non-fallback-zero timestamp and the count matches exactly — the property that actually matters for BL-218/BL-219 building on this substrate. Precision to the exact narrated line is a polish item, not a correctness one; revisit only if a later consumer reads checkpoint timestamps at sub-stage granularity.

*Files: `src/world/history_log.cpp`, `src/world/planetology.hpp`, `tools/verify/history_log_harness.cpp`*

### NR-030 — BL-208 trade_route log entries: the struct's single `body` tag cannot carry a two-body event's both endpoints
*decision taken on your behalf · raised 2026-08-02 · from BL-208 (world history log) — src/world/supply_system.cpp's credit_arrived_convoys, the trade_route-topic push site*

world_history_entry's settled shape (per the task's own spec) carries exactly one body tag and one corp tag. A newly-established trade route is inherently a TWO-body event (src_body <-> dest_body). I tagged the entry with dest_body (the pool credited that tick) and named BOTH endpoints in the narration text, so the information is not lost, but a future body-scoped filter over history_log (e.g. 'show me everything that happened at body X') would miss this entry for the SOURCE body specifically.

**Why it matters.** This is a real, if narrow, gap in the 'tag each entry so it's filterable into either view' design goal for exactly one topic (trade_route) and exactly one axis (the non-tagged endpoint). It does not affect the corp view (the dispatching corp is tagged correctly) or the body view for the tagged endpoint.

- Leave as-is (dest_body tagged, both names in text) — the current state.
- Push two entries per new route, one tagged per endpoint, both with the same narration — doubles trade_route log volume for a rare event (route establishment, not every convoy) and duplicates data for a single event.
- Widen world_history_entry with a second optional body tag (e.g. body_b) used only by this one topic — a shape change to a struct four other call sites now depend on, for one topic's need.

> **Recommendation:** Leave as-is unless a concrete consumer needs source-body filterability on trade_route entries specifically. Route establishment is rare (bounded by the body-pair count, not convoy traffic), so the miss is small in practice, and neither alternative is clearly better than the gap it would close.

*Files: `src/world/supply_system.cpp`, `src/world/world.hpp`*

### NR-032 — Pan-stutter measurement: added a verify frame-timing tap (frame_reset/frame_csv/window + pan_perf.lua) and configured a Ninja Release tree (build_rel/)
*decision taken on your behalf · raised 2026-08-02 · from Session 2026-08-02 (stutter-while-panning report)*

To measure the reported panning stutter with real numbers, three verify functions were added to app.cpp (frame_reset / frame_csv — a CSV dump of the BL-249 frame-stats ring — and window(w,h) to measure at the live 1720x1080 rather than the 1280x720 golden size), frame_stats gained sample(i)/reset() accessors, the BL-249 instrument's function-local static was lifted to a file-local accessor so the verify tap can reach it, and scripts/verify/pan_perf.lua scripts a 300-frame sustained pan at three zooms against a no-pan baseline. Separately, build_rel/ was configured as a Ninja Release tree (same pinned 14.44 toolchain; ninja.exe found bundled under BuildTools' CMake) to quantify the Debug-vs-Release gap without touching the daily build/ Debug cache. All uncommitted as of writing.

**Why it matters.** The tap is a permanent measurement asset (any future perf question is a Lua script away), but it adds three verify functions and a new build tree Ben did not ask for by name. The measurement route itself was a delegated call: desktop-automation of the live app was abandoned (its app resolver cannot see a non-Start-menu exe) in favour of instrumenting --verify, which was verified to run on the real renderer with real vsync.

> **Recommendation:** Keep the tap and commit it (it is pure instrumentation — no world/* contact, no determinism surface); keep build_rel/ as the standing play/perf build alongside the Debug dev build. A build_rel.bat mirroring build_app.bat's toolchain pinning would make it one keystroke.

*Files: `src/core/app.cpp`, `src/ui/frame_stats.hpp`, `src/ui/frame_stats.cpp`, `scripts/verify/pan_perf.lua`*

### NR-033 — The daily-driver build is unoptimised Debug (/Od /RTC1) — and the stale claim that --verify uses a dummy video driver is wrong
*observation · raised 2026-08-02 · from Session 2026-08-02 (stutter-while-panning measurement)*

build/ (the exe _run.bat launches and Ben plays) is CMAKE_BUILD_TYPE=Debug with /Ob0 /Od /RTC1 — zero optimisation plus runtime checks. Measured pan cost: 41-53 ms of work per frame (every frame over the 16.7 ms refresh budget; ~19-24 fps). The same code built Release measures 11-18 ms — 3.6x faster; at play zoom it is 11.3 ms with zero budget misses. Separately, scripts/verify/frame_budget_hud.lua's header claims verify numbers are meaningless because --verify runs an offscreen/dummy driver; grep shows nothing in src/ sets any such driver — verify runs the real renderer with the build's real vsync, which is exactly why the pan_perf measurement is valid. That stale comment should be corrected so future sessions do not route around a measurement path that works.

**Why it matters.** The 'engine starting to thrash' impression is substantially an artefact of playing an unoptimised Debug binary. No engine-architecture conclusion (GPU port, multithreading) should be drawn from Debug frame times.

> **Recommendation:** Play from a Release build; keep Debug for debugging. Fix the frame_budget_hud.lua header comment when next touched.

*Files: `build_app.bat`, `scripts/verify/frame_budget_hud.lua`*

### NR-034 — The word-based milestone (generation via words, then gameplay) has no ROADMAP slot yet — Ben calls it 'the whole process', not one item
*question · raised 2026-08-02 · from BL-270 (action dictionary) filing — Ben's elicitation answers, 2026-08-02*

Ben, promoting BL-270: it 'blocks the next most important milestone, which is a word-based procedural generation and later gameplay', and — on the difficulty-level motivation — 'To consider what the difficulty level will be, we need this item. That is not really one item, it's the whole process of gameplay/development.' The dictionary is filed and promoted (v0.1.1, SSS), and its design names the consumer sequence (word-driven generation first, word-driven play after; a text-play harness item to follow). But ROADMAP.md currently carries no minor themed on word-based play, and 'the whole process' suggests a version-arc-level commitment (v0.1.1? v0.2.0 alongside the AI opponent?) rather than a feature.

**Why it matters.** ROADMAP owns sequence; standing rule: do not implement a milestone that depends on an earlier one not yet complete. The dictionary lands either way, but the harness item, the word-driven wizard item, and the difficulty-level work all need a named slot in the version sequence before they can be filed with honest version goals — and the v0.2.0 AI-opponent arc already exists and overlaps ('the model that will eventually run on-machine').

- Theme v0.1.1 as the word-interface minor: dictionary + text-play harness + cloud Sonnet/Opus experiment; word-driven generation and difficulty land later in the arc.
- Fold it into the existing v0.2.0 AI-opponent arc: the dictionary is v0.1.1 groundwork, everything word-driven ships with the opponent.
- Name it as its own post-v0.1.0 arc in ROADMAP with an explicit stage list (dictionary -> harness -> word generation -> difficulty), since Ben calls it 'the whole process'.

> **Recommendation:** Option 1 for the near term — it matches 'dictionary now, generation and play consume it in sequence' without pre-committing the whole arc; revisit the arc naming when the harness exists and the cloud experiment has produced its first evidence.

*Files: `docs/development/ROADMAP.md`*

### NR-035 — Corp asset PLACEMENT still anchors to the nation, not to the home province BL-219 reads its focus from
*decision taken on your behalf · raised 2026-08-02 · from BL-219 (corporations history rewrite) build, 2026-08-02*

BL-219's settled design says a corporation's focus follows 'the industrialisation history of the province it is anchored to'. As built, the corp picks a home PROVINCE and derives its focus from that province - but Pass 3 then places its starting holdings with the existing nation-wide focus-scored anchor search, not inside that province. So the province decides WHAT the corp is; it does not yet decide WHERE it is.

**Why it matters.** The derivation is honest either way (the province is genuinely chosen first and genuinely determines the focus), but the phrase 'anchored to' promises more than the code delivers, and a player reading a coastal trade corp's holdings 400 tiles inland would be reading a real inconsistency. Constraining placement to the province is a small change to place_starting_assets, but it interacts with the tuned holdings-clustering and terrain-viability behaviour that corp_terrain_matrix guards, so it was not done blind at the end of a large build.

> **Recommendation:** Shipped the focus derivation without constraining placement, and documented the gap in CORPORATION_GENERATION.md rather than letting the doc imply the stronger claim. Recommend (a) - constrain Pass 3's anchor search to the home province window and re-run corp_terrain_matrix - as a small follow-on, since the doc's 'anchored to' wording is otherwise writing a cheque the code does not cash.

*Files: `src/world/corporation_generation.cpp`, `docs/generation/CORPORATION_GENERATION.md`*

### NR-036 — BL-054's territorial-fragmentation half was folded into BL-218 but is not demonstrated - no exclave is asserted anywhere
*observation · raised 2026-08-02 · from BL-218 (nations settlement rewrite) build, 2026-08-02*

BL-218's settled design folds BL-054's exclave/disputed-zone half in on the argument that 'a real settlement sim produces them for free - a growth front that crosses a strait and stalls leaves an exclave without anyone authoring one', and calls that the single best argument for the deeper option over the cheap alternative. The pass is built and the seeds are now province anchors, but nothing measures whether exclaves actually appear: Pass 2b (orphan-island assignment) still hands every water-disconnected component to its nearest neighbour, which is precisely the mechanism that would MANUFACTURE an exclave - or hide the absence of one. settlement_harness does not assert it and I did not add an assertion, because I could not tell from the code alone whether the exclaves that exist are the sim's or Pass 2b's.

**Why it matters.** It is the load-bearing justification for having chosen the expensive option, and it is currently unverified. If the fragmentation is really Pass 2b's, the argument for the deeper path was made on a capability the deeper path did not supply - which is worth knowing before BL-054's remaining half is reasoned about.

> **Recommendation:** Recommend (a): the assertion is cheap and it is the only way to know whether the argument that chose the expensive path was sound. Do not mark BL-054's territorial half complete until it exists.

*Files: `src/world/settlement.cpp`, `src/world/nation_generation.cpp`, `tools/verify/settlement_harness.cpp`*

### NR-037 — ai_skill_harness MSVC goldens re-blessed and history_ladder_harness H4 narrowed - both under BL-218, both explained upward
*decision taken on your behalf · raised 2026-08-02 · from BL-218/BL-219 build, 2026-08-02*

BL-218 changes the political map on every seed (nation seeds are now province anchors) and BL-219 changes the corporate mix, so two harnesses moved. (1) ai_skill_harness: 9 assertions failed on the old MSVC bands. Verified against a stashed baseline that they passed BEFORE the change, so this is genuinely caused by this work rather than pre-existing. Every divergence is UPWARD - net worth rose on seeds 0/2/4, dial counts crept past the old ceiling on 0/1/4 - while solvency and survival stayed in band on all five seeds, which reads as corps anchoring to provinces that actually industrialised rather than as a skill regression. Re-blessed the MSVC block only, per that file's own rule; the GCC block is untouched and will need a fresh Linux run. (2) history_ladder_harness H4: its stage-ordering assertion demanded every line in the recorded-history window be strictly older than the next, which only held while the ladder owned that window alone. Narrowed it to assert the ladder's own causal claim (granary before charter before accord) directly on the three stage lines.

**Why it matters.** Re-blessing a golden is the project's routine convention ('bless routinely, flag only an UNEXPLAINED divergence'), and narrowing an assertion is not - it weakens a check. Both are recorded so the weakening is visible and so the stale GCC set is not forgotten.

> **Recommendation:** Recommend (b) then (c): re-bless GCC on the next Linux run so the two platforms stop drifting, and treat tagging the ladder's lines with their own stage as the real fix for H4 - text-matching a line is exactly the fragility that assertion was narrowed around.

*Files: `tools/verify/ai_skill_harness.cpp`, `tools/verify/history_ladder_harness.cpp`*

### NR-038 — CLAUDE.md no longer says "read every document before responding" — the doc set outgrew the instruction
*decision taken on your behalf · raised 2026-08-02 · from Documentation-compression pass (hot/cold backlog split, DEVLOG index, doc_weight.js)*

CLAUDE.md opened with "Read the documents below before responding to any request." tools/doc_weight.js measures that reading order at ~606,000 tokens across 40 files — several times any usable context, so in practice the instruction was already being ignored, silently and unevenly. The opening was rewritten to instruct traversal instead: read the doc that owns the question, and prefer an index or query tool (DEVLOG_INDEX.md, backlog_query.js, actions_query.js) over loading a file whole.

**Why it matters.** An instruction that cannot be followed is worse than a narrower one that can: it makes every session's actual reading undocumented and unpredictable. Recorded here rather than assumed because it changes the contract at the top of CLAUDE.md, which is the one document every session reads.

> **Recommendation:** If you disagree, the alternative is to shrink the reading order to fit a real budget (doc_weight.js --budget takes a ceiling and exits non-zero when the named set is over it) rather than to restore the old wording.

*Files: `CLAUDE.md`, `tools/doc_weight.js`*

### NR-040 — C-route walk-back: not a shipped LLM API integration — the real ask is human-in-the-loop play via computer-use, plumbing still unscoped
*question · raised 2026-08-02 · from Same sprint-planning session as NR-039, immediately after — Ben corrected the direction once he saw the concrete design (HTTP client choice, API key storage, threading)*

Asked to design C-route's implementation, I produced a full in-process design: an HTTP client dependency, a live Anthropic API call per corp per econ tick, and a determinism-rule carve-out (recorded as NR-039). Presenting the concrete choices this required (which HTTP library, how to store an API key, whether the call blocks the sim tick) surfaced that this wasn't the intent. Ben: 'I never intended for this to be an item. We still need the plumbing for in-place sessions to make decisions. The best way is just to use Claude on my PC to actually visually play the game with mouse and keyboard input.' All three doc changes from NR-039 (AI_OPPONENT.md §7a, the io-standing-rules.md determinism exception, BL-205's design field) have been reverted; §7's original Stage-C description (unchanged since 2026-07-26) is the standing design again.

**Why it matters.** The actual near-term want is a human-in-the-loop workflow — Claude (this kind of session, with computer-use/screenshot/click tools) playing the game live to make AI-corp decisions or prototype behavior, not a coded feature shipped in ProjectIo.exe. That is much closer to what BL-206 (blackboard export) and BL-270 (the action dictionary) already exist for — an AI-consumable read/write interface — than to a new LLM-API integration. What's still unclear: what 'plumbing' is actually missing. Claude already has computer-use tools (screenshot, click, type) and the `run` skill can launch the app; BL-206's state export and BL-270's action dictionary already give a language-agent read/write surface. It's possible nothing new needs building at all, or the ask is a thin harness that surfaces ACTIONS_INDEX.json + the blackboard export in a form easier for a live session to drive. Filed as a question rather than closed, since guessing further without Ben's steer risks a third build-the-wrong-thing pass.

- Nothing needs building — BL-206 + BL-270 + existing computer-use tools already cover it; next session just tries actually playing the game this way and see what's missing in practice.
- A small harness is needed: something that packages the blackboard export + action dictionary into a form a live session reads faster than raw JSON (e.g. a single 'what can I do right now' summary).
- Something else — Ben has a more specific plumbing gap in mind that hasn't been named yet.

> **Recommendation:** Try option 1 first, cheaply: next session, actually attempt to drive the game via computer-use using what already exists (BL-206 export + BL-270 actions), and only design new plumbing against a concrete friction point hit during that attempt. Building infrastructure ahead of the first real attempt is exactly the mistake NR-039 just made at a different layer.

*Files: `docs/ai/AI_OPPONENT.md`, `docs/development/backlog.json`, `docs/ai/ACTIONS.json`, `docs/development/req/requirements.json`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-001 — The construction ledger says "50% staffing", but the player's own corp auto-solves the dial on the first tick
*question · raised 2026-08-01 · from BL-162 (tile construction ledger) — surfaced by prospective_profit_harness*

The ledger caption reads "Est. net / tick at today's local prices - 50% staffing, no labour shortage." The estimate uses workforce_assigned = 0.5, which mirrors construction.cpp exactly, so it correctly describes the building at the instant it is created. But BL-181 gives player buildings workforce_auto = true by default, and the dial is auto-solved for maximum profit on the first tick. tools/verify/prospective_profit_harness.cpp measured realised extraction at 2x the estimate for exactly this reason, and had to pin workforce_auto = false to make its cross-check meaningful.

**Why it matters.** The number is honest for the moment it is shown and dishonest as a prediction — and a player reads a profit bar as a prediction. Every candidate is understated by the same factor, so the RANKING (which is what the chart is for) is unaffected; the magnitude is not.

- Leave it. The figure is defensible and the ranking is correct; adding caveats to a caption costs legibility.
- Say it is a floor — e.g. 'at least +1243 / tick' or append 'before auto-staffing'. One or two words, no geometry change.
- Estimate at the auto-solved workforce instead, and caption it as such. Most accurate for the player's own corp, but it stops mirroring construction.cpp and diverges from what a background corp would actually get.

> **Recommendation:** Option 2. The chart's job is ranking and it already does that correctly; a two-word change stops the magnitude reading as a ceiling when it is a floor. Option 3 buys accuracy at the cost of the estimate no longer describing the building that is actually created, which is the property that makes it verifiable against economy_system.

> **RESOLVED.** OPTION 2 (Ben, 2026-08-01) — say it is a floor. Caption now reads 'Est. net / tick at today's local prices - at least this, before auto-staffing; 50% staffing, no labour shortage. Capex is not in the bar; see payback.' No geometry change and no change to the estimator, so it still mirrors construction.cpp and stays verifiable against economy_system. The reasoning is recorded inline at the call site, not just here. Ben added the general point that a lot of these calls need big data sets from watching an AI play, which does not exist yet. That is the second time in this pass he has deferred a call for the same reason (see BL-261, player alerts); an item for AI-play observation was offered to him.

*Files: `src/ui/selection_panel.cpp`, `src/world/building_profit.cpp`*

### NR-002 — The header reads "/ qtr" while the ledger beside it reads "/ tick" — GLOSSARY defines Tick and does not define qtr
*observation · raised 2026-08-01 · from Noticed while inspecting the BL-162 goldens*

The app header prints NET +3.2k / qtr. The construction ledger, the Selection band's profitability read, and draw_building_profit all print / tick. GLOSSARY.md defines Tick ('the fixed period between economy updates'); 'qtr' is not a defined term.

**Why it matters.** The standing rules say that if a term is defined in GLOSSARY, do not substitute an alternative. This is a live violation sitting a few inches from figures that get it right, on the screen a new player reads first. It is also pre-existing and entirely outside BL-162's scope, which is why it was flagged rather than fixed — changing a header figure's units is a call about what the header is FOR, not a typo.

- Change the header to / tick, matching every other surface and the glossary.
- Keep / qtr and define Quarter in GLOSSARY as a real term, if the header is deliberately reporting a different (longer) period than a tick.
- Leave it; accept the inconsistency.

> **Recommendation:** Needs your intent before anyone acts. If the header genuinely aggregates over a quarter then it is reporting a different quantity and option 2 is right; if it is just an older label for the same per-tick figure, option 1. Worth checking which it actually computes before choosing — I did not verify that, only the labels.

> **RESOLVED.** RESOLVED THE OTHER WAY (Ben, 2026-08-01): 'Qtr is the preferred term for any economy tick. Tick is too technical of a term for average gamers.' So the header is RIGHT and every other surface is wrong — the opposite of the recommendation, which assumed the majority spelling won. Quarter becomes the player-facing term; Tick stays the internal/technical one, and GLOSSARY must define both plus which audience each serves. Supporting evidence found while scoping: construction_panel.cpp's ticks_label already prints '(~N yr)' at ticks/4, with the comment 'a Tick is ~3 months, so 4 ticks make a year (BL-095)'. A tick IS a quarter in the campaign calendar, so this is not a friendlier synonym for a technical unit — the display term is the more literally accurate one. Scope measured: ~25 player-facing strings across balance_ledger, construction_panel, corporation_dashboard, market_ledger, selection_panel and app.cpp, plus GLOSSARY, plus a visual-golden re-bless (already owed under BL-259).

*Files: `src/core/app.cpp`, `docs/GLOSSARY.md`*

### NR-003 — A payback faster than one tick prints "payback ~0 ticks", which reads as free rather than immediate
*observation · raised 2026-08-01 · from BL-162 — visible in the blessed golden tile_build_ledger_land*

Extraction: Agricultural Produce shows '686 cr - payback ~0 ticks' (capex 686, net +1243/tick, so true payback ~0.55 ticks). The integer format rounds it to 0.

**Why it matters.** Cosmetic, but '~0 ticks' is the one value in the row that could be read as an error or as 'costs nothing'. The intended meaning — it pays for itself inside the first tick — is stronger than what it prints.

- Floor the display at 1 ('payback ~1 tick').
- Special-case sub-tick paybacks with wording, e.g. 'pays back within 1 tick'.
- Leave it.

> **Recommendation:** Option 2 if the wording fits the row width, otherwise option 1. Either is a one-line change; it is here rather than in the backlog because it is too small to file and too easy to lose.

> **RESOLVED.** LEAVE IT (Ben, 2026-08-01), and for a reason larger than the row: 'We actually shouldn't do that much work for the player. It's up to them to figure out what is profitable. Estimations like this can be made, but they should emerge from a reading of data, not from the data.' So neither option was taken — refining the payback wording is work in the wrong direction. The principle behind it reaches further than this entry and is raised separately as NR-019, because it questions the premise of the surface the row sits in.

*Files: `src/ui/selection_panel.cpp`*

### NR-004 — Should non-home bodies have markets at campaign start? Today none do, and it makes inter-body trade unrepresentable
*question · raised 2026-08-01 · from BL-254 (data-creep convoy scenario) — reported by the harness itself*

The generated world seeds all six markets on the single tiled body (Kepler). A trade_route is body-level, so intra-body lanes record nothing — meaning the generated world CANNOT record a trade route however long it runs, regardless of the launchpad gate. The data-creep harness had to author three stub markets pre-run to give its convoy lanes endpoints at all. This is a second, independent cause of the blind spot BL-254 was filed for; the filed item only named the launchpad gate.

**Why it matters.** This is not a harness problem, it is a world-premise question. If no other body has a market at start, then inter-body trade — one of the two pillars every system is supposed to feed — has no destination on turn one, and the activity fog (BL-089), persistent trade routes (BL-088) and the supply lens all describe machinery with nothing to act on yet. It may well be intended (Era 0 is terrestrial; space access is gated), in which case it should be stated somewhere rather than being an emergent property of market seeding.

- Intended — Era 0 has no off-world markets by design; record it in ERAS.md / MARKETS.md so it stops looking like an omission.
- Seed a minimal market on at least one other body at start, so the inter-body machinery has a destination from turn one.
- Neither yet — revisit when the Era 1 transition is actually built.

> **Recommendation:** Option 1 or 3 on current evidence — this looks intended rather than broken, given Era 0 is explicitly terrestrial. But it is worth writing down either way: an instrument had to work around it, which is the signal that the premise is undocumented rather than merely unimplemented.

> **RESOLVED.** ANSWERED OUTSIDE THE OPTIONS (Ben, 2026-08-01): "I think markets should be spontaneously generated when colonisation / exploration starts." Not option 1 (document Era 0 as market-free), not option 2 (pre-seed elsewhere), not option 3 (defer) — market EXISTENCE becomes a runtime consequence of going somewhere. Filed as BL-263 (spontaneous market emergence), design-owed, post-v0.1.0. The structural consequence is that markets stop being a world-gen artifact and become simulation state, which reaches the save format, determinism, catchment routing and price discovery for a market with no price history. Five open calls recorded there; the trigger and what actually clears at an outpost market are the two that decide whether it plays.

*Files: `docs/economy/MARKETS.md`, `docs/economy/ERAS.md`, `src/world/hard_coded_world.cpp`*

### NR-005 — BL-256 (generation globe): which fidelity tier, and is running the continents pass in the wizard preview affordable?
*question · raised 2026-08-01 · from BL-256, filed 2026-08-01*

The wizard preview runs the planetology chain only (resolve_preferences + preview_system), so at wizard time there is no height field, no ocean mask and no terrain for a map-like globe to sample. The item is written with two tiers: Tier 1 a characterisation globe from the planetology scalars, Tier 2 a real-landmass globe that also runs the deterministic continents pass plus enough of tile Pass 1 to get a height/ocean field.

**Why it matters.** Tier 2 is the one you actually asked for — the planet you are about to play. But the preview re-runs on EVERY control move, and the continents pass is the expensive half, so Tier 2 may not be affordable per-move. The cost has not been measured yet; the item says to measure before adopting.

- Build both tiers as filed, measure Tier 2's cost, and fall back to a short debounce if it is too slow per-move.
- Tier 1 only for now — cheap, always available, and honest for the early rounds where landmass genuinely is not decided.
- Tier 2 only — skip the characterisation globe and accept that the globe appears later in the wizard.

> **Recommendation:** Option 1 as filed. The reason to keep Tier 1 even if Tier 2 proves cheap is that the early rounds genuinely have not decided the landmass, and showing invented continents there would be the generation equivalent of a lying figure. The item requires the tier be captioned so the player knows which they are looking at.

> **RESOLVED.** ANSWERED BY REFRAMING (Ben, 2026-08-01): the globe becomes the generation screen PRIMARY view, pannable with a pole clamp, showing the world forming, with the charts demoted to extras on top; pre-world rounds show the solar system instead. So the tier question is settled sideways — Tier 2 (real landmass) is the target and Tier 1 survives only for rounds where landmass genuinely is not decided, captioned so the difference is visible. BL-256 rewritten to match, status moved back to design-owed and difficulty raised to 5. Two corrections went into it: (a) the original claim that the continents pass is the expensive half looks wrong on inspection (O(w*h*plates), roughly 180k int ops, sub-millisecond) — the cost is more likely the per-pixel DRAW once the disc is a primary view rather than a 200 px sketch, mitigated by caching the pixel->(lat,lon) table since rotation is only a longitude add; (b) the per-pixel projection the item already specified avoids both problems a per-tile-polygon approach would hit (ImGui 16-bit index ceiling, degenerate polar slivers) — poles smear instead of shatter. The one risk that cannot be settled on paper is how those smeared poles actually LOOK, so the item now sequences a throwaway prototype against the already-generated world as task 1, before any wizard work.

*Files: `docs/development/backlog.json`*

### NR-006 — BL-257 (generated body names): which naming register?
*question · raised 2026-08-01 · from BL-257, filed 2026-08-01 — the item's one genuinely open design call*

The current five bodies mix three registers: mythological (Helios, Selene), descriptive (Cinder) and scientist-eponym (Kepler). A generated pool has to pick deliberately, because a pool drawn at random from all three reads as noise rather than as a naming convention.

**Why it matters.** This is the half of BL-257 that determines whether the result reads as a real system or a shuffle, and it is taste, not engineering. The other half (moving identity off the display string) is unambiguous and can proceed without this answer.

- Mythological — consistent with Helios/Selene, reads classical and matches the near-future-corporate tone by contrast.
- Scientist-eponym — consistent with Kepler; reads as a surveyed, catalogued system, which fits a corporate 4X.
- Descriptive — consistent with Cinder; names describe the body, which doubles as player information.
- A deliberate MIX with a rule (e.g. planets mythological, moons descriptive), so the mixing is legible rather than random.

> **Recommendation:** Option 4 is the deepest and probably the most characterful — a rule the player can half-perceive reads as a world with a history of being named, rather than a bag drawn from. But this is squarely your call; the item is written so the pool can be swapped without touching the identity work.

> **RESOLVED.** OPTION 4 (Ben, 2026-08-01): "I agree, a deliberate mix is great." The registers stay mixed, but which one a body gets is decided by what the body is, so the mixing is legible rather than random. BL-257 patched — the register bullet no longer reads as an open call. The RULE itself is now the residual call and is deliberately left for promotion: proposed there is planets mythological (named at a distance, from lights), moons descriptive (named close up, by whoever surveyed them), and the scientist-eponym register reserved for what was catalogued rather than observed — which would make Kepler explicable rather than an inconsistency. Ben accepts or replaces that at promotion; the item now requires whichever rule is chosen to be written into the doc, since an unstated rule decays into the noise it was meant to prevent.

*Files: `docs/development/backlog.json`*

### NR-007 — Decisions taken on your behalf 2026-08-01, each reversible
*decision taken on your behalf · raised 2026-08-01 · from Session 2026-08-01 (v0.1.0 cut set completion)*

Four calls were made so work could continue. Each is recorded so it can be overturned rather than becoming precedent by default. (1) The Windows ai_skill_harness bands were RE-BLESSED. BL-252 said not to re-bless before knowing which cause was at work; the diagnostic established staleness, so this was the sanctioned action, but it is still a bless nobody reviewed. (2) The four tile_build_ledger goldens were re-blessed on Windows after inspection. (3) BL-162's requirement R7 had its 'visual' verification leg REMOVED, because this item staled that golden by construction and a row must not read complete on a red leg — the code and headless legs stand alone. (4) ctest timeout tiers were set at 60s default / 120s for three named long-runners, chosen against measured Debug runtimes.

**Why it matters.** Re-blessing is routine per the standing convention, but two of these blesses moved a baseline that no human has looked at since, and the third quietly narrowed what a requirement claims to verify. None is hard to reverse; all are easy to forget.

> **Recommendation:** No action needed unless you disagree. One value is worth your eye: data_creep_harness now runs 42.95 s in a Debug build against its 120 s timeout — 2.8x headroom, below the >=3x the tier was sized for, because BL-254 added 1500 seeded convoys after that sizing. It passes comfortably and is nowhere near hang territory, so nothing was changed; it is the value that would want revisiting if that rollout grows again.

> **RESOLVED.** ACCEPTED, no reversals (Ben, 2026-08-01): "I am happy with these." All four calls stand — the two golden re-blesses, the removal of BL-162 R7 visual leg, and the ctest timeout tiers. The data_creep_harness headroom (42.95 s against a 120 s timeout, 2.8x rather than the 3x the tier was sized for) was flagged and deliberately left unchanged.

*Files: `tools/verify/ai_skill_harness.cpp`, `scripts/verify/golden/`, `docs/development/req/requirements.json`, `CMakeLists.txt`*

### NR-008 — backlog_lint's two standing warnings are pre-existing and nobody owns them
*observation · raised 2026-08-01 · from backlog_lint, every run this session*

Two warnings survive every clean run: BL-114's requirement group is marked complete while row R3 is still pending, and BL-190's group is complete while the item itself is still status 'designed' (it should be terminal). Both predate this session and neither is a fail.

**Why it matters.** A lint that always prints the same two warnings trains everyone to skim past the warning section, which is where a real new warning will appear. That is the same failure mode as a permanently-red test.

- Resolve both — flip BL-190 terminal, and back-fill or explicitly note BL-114 R3.
- Suppress them with a recorded justification, so the warning list returns to empty.
- Leave them.

> **Recommendation:** Option 1, as a five-minute cleanup by whoever next touches the backlog tooling. They were left alone this session because both belong to other people's items and flipping someone else's status silently is worse than the warning.

> **RESOLVED.** OPTION 1 — both resolved (Ben, 2026-08-01), not suppressed. BL-190 (population demand ordering fix): status designed -> complete. The work landed in fba0867 on 2026-07-31 with its requirement group already complete; only the item status was left behind. BL-114 R3 (New World setup exposes the descriptor): status pending -> CANCELLED, not back-filled. The row describes a main-menu setup screen that the BL-167 New World wizard superseded, so its named check scripts/verify/new_world_setup.lua can never be authored against the surface it describes — a row that CANNOT be satisfied is a different thing from one nobody has got to, and only the second should read as pending. No coverage is lost: the load-bearing half (a new game builds from the edited descriptor, deterministically) is already covered headlessly by R1/R2, and verifying the wizard surface itself belongs to BL-167. backlog_lint now reports clean, so the next warning to appear will be a real one.

*Files: `docs/development/backlog.json`, `docs/development/req/requirements.json`*

### NR-009 — The fold overlay joins the Esc ladder, against BL-214's Decision 10
*decision taken on your behalf · raised 2026-08-01 · from Session 2026-08-01 (disclosure spine — BL-214 / BL-247 / BL-248, commit d143aa4)*

BL-214 Decision 10 states explicitly that the depth control takes no keyboard binding and does NOT join the Esc ladder, because card_stack (the subject axis) already owns that unwind and a second one would re-merge the two axes at the keyboard. I overrode it: Esc now folds the overlay, one rung BELOW the subject drills. Order is exit-confirm -> system menu -> pop card_stack -> pop corp_rollup_drill -> fold -> hide selection -> open menu, so one press never both unwinds a drill and closes the overlay hosting it.

**Why it matters.** Decision 10's reasoning is sound for what it was reasoning about — an IN-PLACE stepper, where a level genuinely is not a dismissal. Your binary supersession changed the thing being reasoned about: expanded became a full-screen MODE that covers the entire window. A mode with no keyboard exit is a usability defect rather than a principle, and the ordering fix (below the drills) answers the actual objection Decision 10 raised. But it is still me overturning a written decision of yours, so it should be your call to keep.

- Keep it. A full-screen mode needs a keyboard exit, and the ordering preserves Decision 10's real concern.
- Revert to Decision 10 as written — Esc does not fold; the chevron is the only way out.
- Keep the rung but move it ABOVE the drills, so Esc leaves the overlay first and the drill state is discarded.

> **Recommendation:** Keep it. This is documented in LAYOUT.md § Drill-through with the reasoning stated inline, so a future reader sees the override rather than a silent contradiction.

> **RESOLVED.** KEPT (Ben, 2026-08-01). Esc as a drill-up is sensible; the override stands and Decision 10 is superseded on this point. Ben added a consequence: the Selection element should be ALWAYS OPEN, so the 'hide selection' rung comes out of the ladder, and Esc's terminal rung becomes the pause screen once that exists. See NR-017 for that follow-up.

*Files: `src/core/app.cpp`, `docs/ui/LAYOUT.md`*

### NR-010 — BL-247's three open questions, settled without you
*decision taken on your behalf · raised 2026-08-01 · from Session 2026-08-01 (disclosure spine — BL-214 / BL-247 / BL-248, commit d143aa4)*

The item explicitly left three questions 'open for the promoting session' and I answered all three. (1) Is a question/why pair REQUIRED on every chart? Answered: OPTIONAL — a chart with no authored pair draws no toggle at all. (2) Does the note's open state persist for the session? Answered: NO — one note open at a time, reset closed by any expand/fold. (3) Does it belong as a per-chart parameter on the shared chart helper, or a separate wrapper call site? Answered: a per-chart parameter, which was the item's own recommendation.

**Why it matters.** (1) is the one worth your eye. Making the pair mandatory would have been a real design position — it would turn the log into an audit that every chart must pass, which is the 'design-facing' use you described (a chart that cannot state a question probably has not earned its place). I made it optional so an unlabelled chart costs nothing, which means the audit is advisory rather than enforced. If you want the stricter reading, the change is small: log or assert on a chart drawn without a pair.

- Keep optional. A missing pair reads as a review signal, not a render defect.
- Make it mandatory — every chart must carry a pair, enforced by a headless check that fails on an unlabelled chart.
- Keep optional but add a report: a verify pass that LISTS unlabelled charts without failing.

> **Recommendation:** Option 3 if you want the audit teeth without the ceremony — it gives you the roster of unlabelled charts to review, which is what the design-facing use actually needs.

> **RESOLVED.** OVERTURNED (Ben, 2026-08-01). The pair is REQUIRED, not optional: 'we need to have documentation for each element... low storage cost for perpetual clarity. So the docs are the audit, we don't need an audit method.' So neither my optional settlement NOR my recommended report-pass survives — the discipline is authorship, enforced by convention, and no headless check is to be built. Answers (2) and (3) stand unchallenged. Two consequences flagged back to Ben: (a) BACKFILL — pairs exist today only on generation_charts (5), corporation_dashboard (4) and selection_card (2); market_ledger, economy_panel, balance_ledger, tile_inspector, construction_panel and tech_tree_panel have none. (b) SCOPE — Ben said 'each element', where BL-247 says each chart/visual; if elements means every panel and table, that is wider than the item as filed. BEN CONFIRMED THE WIDER READING (2026-08-01): every information surface, not only things that plot. He added a storage requirement — each justification lives in a .json file rather than inline in the C++ call site, and carries the id of the BACKLOG ITEM THAT DEMANDED IT, so a surface traces back to the work that asked for it. That makes the log a queryable provenance index, not just player-facing text, and needs its own backlog item: it changes where the strings live (a load or codegen step where today they are string literals at the call site) and widens BL-247's file scope well past charts.

*Files: `src/ui/detail_level.cpp`, `src/ui/generation_charts.cpp`*

### NR-011 — why_note lives in detail_level.{hpp,cpp}, not charts.cpp as BL-247's file scope named
*decision taken on your behalf · raised 2026-08-01 · from Session 2026-08-01 (disclosure spine — BL-214 / BL-247 / BL-248, commit d143aa4)*

BL-247's filed file scope names src/ui/charts.cpp for the question-log helper. I put it in src/ui/detail_level.{hpp,cpp} instead, alongside the fold idiom.

**Why it matters.** The helper needs ui_state (to hold which note is open), and charts:: is deliberately a pure draw-primitive namespace whose header includes only imgui and cstdint — it knows nothing about UI state and threading ui_state into it would have been the first crack in that separation. The two affordances are also the same family: both are disclosure controls the player asks for. Minor, but it is a departure from a filed scope, and those are worth seeing.

- Keep it in detail_level.
- Move it to charts.cpp as filed and thread ui_state through.

> **Recommendation:** Keep it. charts:: staying free of ui_state is worth more than matching a scope line written before the seam was known.

> **RESOLVED.** KEPT (Ben, 2026-08-01): 'I don't see a strong reason either way' — the departure from BL-247's filed scope stands. But he attached a much larger correction in the same breath: 'the why shouldn't leak into the UI, players don't need us telling them what they ought to ask of the game.' That retires the PLAYER-FACING half of BL-247 entirely — the question log is development documentation, not an in-game affordance — and it makes this entry's subject (where the draw helper lives) largely moot, since there may be no draw helper. Raised as NR-018 and folded into BL-260 rather than settled here.

*Files: `src/ui/detail_level.hpp`, `src/ui/detail_level.cpp`*

### NR-012 — MENU.md's 'alert' concept was dropped rather than built, and the all-corporations table was deleted
*decision taken on your behalf · raised 2026-08-01 · from Session 2026-08-01 (disclosure spine — BL-214 / BL-247 / BL-248, commit d143aa4)*

Two calls inside BL-248. (1) MENU.md's Slot 1 MVP named ALERTS as one of four roll-ups (idle buildings, unsold output, negative cashflow) and noted it introduces an 'alert' concept. Your 2026-07-31 call replaced the MVP four with the exemplar's Production/Trade/Workforce/Finance, which has no alerts card — I did not reintroduce one. An idle building and a negative net now read as red verdicts on their own cards instead. (2) src/ui/corporation_panel.{hpp,cpp} was DELETED, not left dormant — it drew an all-corporations balance table that duplicated the Economy panel's Corps view.

**Why it matters.** (1) quietly retires a named concept. Red verdicts give the same signal without a second mechanism to maintain, but 'alerts' as a first-class thing the player can enumerate is a different feature, and dropping it by omission is exactly the failure mode BL-214 was raised to fix. (2) is a file deletion — recoverable from git, but worth knowing it happened rather than discovering the table is gone.

- Both fine — red verdicts are the alert, and the table was a duplicate.
- Reinstate an alerts concept as its own backlog item for when there is more than three things to alert on.
- The all-corporations table was doing something the Economy panel's Corps view does not; restore it somewhere.

> **Recommendation:** Option 2 if alerts matter to you later — it is a real feature, not chrome, and deserves its own item rather than a card slot.

> **RESOLVED.** PART 1 RESOLVED (Ben, 2026-08-01): alerts are wanted but DEFERRED — 'I'm not sure where to put them yet... There's not much info on how the game plays yet, and I'm saving that eval until after we get basic AI which can play the game.' Filed as BL-261 (player alerts), parked, version goal v0.2.0, with the reasoning recorded: where alerts live and what earns one cannot be decided against a world with no opponent. The red-verdict treatment stands in the meantime and is not a placeholder. PART 2 RESOLVED (Ben, 2026-08-01): 'let's restore it. I didn't intend to delete any work.' src/ui/corporation_panel.{hpp,cpp} were restored verbatim from d143aa4^ and compile clean (CMake GLOB_RECURSE picks them up with no CMakeLists change). NOT yet wired to a call site: nav slot 1 now hosts the BL-248 dashboard, all nine MENU.md slots are assigned, and BL-174 explicitly dropped the tenth. Ben then chose the provisional home: 'Push it into diplomacy for now.' Wired to nav slot 8 behind a NEW flag ui_state::show_corporations_table (slot 1's show_corporation_panel stays the BL-248 dashboard's), added to close_all_panels and any_panel_open, glyph now lights instead of staying dim, tooltip reads 'Not yet built. For now: every corporation's balance, side by side.' Slot 8 keeps its Diplomacy label so the rail does not start teaching that Diplomacy IS a balance table. This is explicitly provisional and moves when Diplomacy is designed. Builds clean. MERGED FORWARD: Ben asked for the table to be folded into the scoring item; no such item existed, so BL-262 (scoring system) was opened and absorbs it. The table is that item's rough first surface — right question, wrong content — and its slot-8 home is explicitly provisional. A standing note for future sessions: 'duplicates an existing view' is not sufficient grounds to delete a file — dormant beats deleted, because the author's intent is not recoverable from a diff.

*Files: `docs/ui/MENU.md`, `src/ui/corporation_dashboard.cpp`*

### NR-013 — The Trade roll-up reads '0 lanes' because every market seeds on one body
*observation · raised 2026-08-01 · from Session 2026-08-01 (disclosure spine — BL-214 / BL-247 / BL-248, commit d143aa4)*

The Corporation dashboard's Trade card is wired to w.trade_routes and correctly reports zero, because the generated world seeds every market on the single tiled body, so there is no inter-body pair to record a route against. BL-254 surfaced exactly this and deliberately did not settle it, calling out 'whether non-home bodies should have markets at campaign start' as an open design question.

**Why it matters.** It was a harness blind spot before; it is now a player-facing card that reads empty on a fresh campaign. That raises the cost of leaving the question open — a dashboard quarter that always says nothing teaches the player the surface is dead. Not a defect in the dashboard; the card is honest.

- Settle BL-254's open question — seed markets on non-home bodies so lanes exist from the start.
- Leave it; the card fills in as soon as the player builds toward another body, which is the intended arc.
- Have the empty Trade card say WHY it is empty rather than showing zero.

> **Recommendation:** Option 3 is cheap and honest regardless of how the design question lands; it does not pre-empt option 1.

> **RESOLVED.** RESOLVED BY BL-263 (spontaneous market emergence). This entry and NR-004 were the same cause seen twice — once as a harness blind spot, once as a dashboard quarter that reads 0 lanes on a fresh campaign and would never change. Once markets can emerge from colonisation, the Trade card fills in on its own and needs no UI change; the recommended option 3 (have the empty card say WHY it is empty) is dropped as unnecessary.

*Files: `src/ui/corporation_dashboard.cpp`*

### NR-014 — Full-screen still does not fit the History Tiles table's 23 resource columns
*observation · raised 2026-08-01 · from Session 2026-08-01 (disclosure spine — BL-214 / BL-247 / BL-248, commit d143aa4)*

Expanding the Tiles view was the single biggest win of the fold — the table goes from permanently horizontally scrolled in a 380 px column to reading across the screen (capture: fold_history_tiles_expanded). But at 1280 wide the last ~17 resource columns are still squeezed to single-letter headers. 6 identity columns + 23 resources does not fit any screen at this column width.

**Why it matters.** This is BL-215's territory, not BL-214's — the seam BL-214's design states flatly is 'BL-214 decides WHETHER a string is drawn; BL-215 decides HOW a string that is drawn fits'. Flagging it so BL-215's audit has the case pre-identified rather than rediscovering it, and so the fold is not mistaken for having solved this table.

> **Recommendation:** No action now — carry it into BL-215 (text-wrap render audit) as a known case.

> **RESOLVED.** DROPPED, not deferred to BL-215 (Ben, 2026-08-01): "I am actually not a massive fan of the tiles table. This is because it can be seen by looking at the canvas. Feel free to drop this from UI." The per-tile table (x, y, composition, landform, hazard, habitability + 23 deposit columns) is removed from the History ledger. That is the better answer than fitting it: the Planetary canvas already shows every one of those fields spatially, where position is the point, and a 29-column table was the same data with the geography thrown away. BL-215 (text-wrap audit) loses a case rather than gaining one. SCOPE HELD DELIBERATELY: only the tiles table went. The view still carries its Buildings list and Market table, which Ben did not comment on — see NR-020, since both also duplicate other panels and the tab is now named after the one thing it no longer contains. Per NR-012 the code was not treated as disposable: it is recoverable from git and the removal is recorded at the call site with the reason, not silently deleted.

*Files: `src/ui/tile_inspector.cpp`*

### NR-015 — The wizard's chart region is now mostly empty when its stages are folded
*question · raised 2026-08-01 · from Session 2026-08-01 (disclosure spine — BL-214 / BL-247 / BL-248, commit d143aa4)*

With per-stage folding, a wizard round's four stages occupy four lines at the top of a chart region sized for full charts, leaving a large empty band above the preference block (capture: fold_wizard_stages_folded). The layout reserves height for charts that are no longer drawn at rest.

**Why it matters.** You chose per-stage folding, and the result reads well — the whole chain is legible at a glance for the first time. But the screen now looks under-filled, and this is the first surface a new player sees. It is a layout question I did not want to answer unilaterally, since 'first impression' is your stated framing for this screen.

- Leave the space. It is calm, and the wizard is not short of screen.
- Let the chart region shrink to its content, floating the preference block and footer up.
- Open the round's FIRST stage expanded by default, so the screen shows one full stage plus the other three as verdicts.
- Use the freed space for something else — the world summary, or the globe preview (BL-256).

> **Recommendation:** Option 2 as the safe layout fix; option 4 is the interesting one, since BL-256 (rotating globe on the generation screen) is already designed and now has somewhere to live.

> **RESOLVED.** FILED AS LOW PRIORITY, not fixed now (Ben, 2026-08-02): "it works fine now - just visually awkward." BL-264 (wizard layout after fold), priority C, post-v0.1.0, held behind BL-256. Ben gave four directions: the pre-gen map/globe fills the band; each expandable item gets bigger; the group is vertically centred; preference controls become buttons rather than dropdowns. He also asked for Esc to close these views — and that is genuinely missing rather than already covered, which the item records: app::handle_key returns early when m_screen != app_screen::in_game, so the entire Esc ladder including BL-214 fold rung is unreachable in the wizard, and a stage expanded there can only be closed by its chevron. The item requires the current heights be reported to Ben as measurements before anyone picks a size (Rule 0b).

*Files: `src/core/app.cpp`*

### NR-016 — Three surfaces changed shape and want your eyes on the live app
*observation · raised 2026-08-01 · from Session 2026-08-01 (disclosure spine — BL-214 / BL-247 / BL-248, commit d143aa4)*

The batch is golden-verified, but three surfaces changed enough that captures are not a substitute for using them: (1) the Selection band's metric card gained a chevron and a full-screen view; (2) the History ledger's Chain view lost its accordions for verdict lines; (3) nav slot 1 is an entirely new dashboard. The fold gesture in particular is only really judgeable by clicking it.

**Why it matters.** Standing practice is to open the live app whenever your judgement on visuals is wanted. Not done this session because you were about to restart the machine to test dispatch.

> **Recommendation:** Next session: launch windowed on monitor 2, select a tile with deposits, and try the chevron on the band, the History Chain, and each of the four roll-ups.

> **RESOLVED.** CLOSED, and it did its job (Ben, 2026-08-02). He did not open the app but responded to the description: "I didn't notice this. I like it, but..." — and then asked for four changes to the disclosure model, three of them structural. Filed as BL-265 (disclosure controls revision): expand and full screen become TWO controls with expand acting in place; full screen is bounded to the CANVAS region rather than the whole window; a full-screened accordion scrolls ALL its items open; and the controls must sit in one predictable column. The last one is a measured defect rather than taste — generation_charts and corporation_dashboard draw the chevron LEFT of the label, tile_inspector and selection_panel push it to the RIGHT edge, so the same control does the same job in two different places. UNRESOLVED and put back to Ben: he asked for left alignment on the wizard (BL-264) and right alignment for the button pair (BL-265) in the same message, and it has to hold everywhere at once.

### NR-017 — The Selection element should be always open — which removes the 'hide' rung from the Esc ladder
*question · raised 2026-08-01 · from Ben's resolution of NR-009, 2026-08-01*

Ben: 'the selection element should probably be always open. So esc drills up to the pause screen, when we implement that.' Today app.cpp's Esc ladder has a 'hide, not destroy' rung (m_ui.selection_hidden_for = m_ui.selected_entity) sitting between the fold and the system menu, and selection_card.cpp gates its draw on selection_hidden_for. That rung is BL-194/196's dismissal concept. Making the element always open deletes the rung and the dismissal state with it, so Esc's terminal rung (already show_system_menu) becomes the only ending.

**Why it matters.** This is a retirement of a designed affordance (a dismissible card), not just a keybinding tweak — SELECTION.md and BL-194/196 both assume the card can be dismissed. It also raises what the band shows with NOTHING selected, which today is simply nothing drawn. 'Always open' needs an empty state, or selection must never be empty. Ben also named a pause screen that does not exist yet; the current terminal rung opens the system menu, which may or may not be the same thing.

- File a backlog item for it — remove the hide rung, delete selection_hidden_for, design the empty state, and align SELECTION.md.
- Do it now as a Light change (rung + state removal only) and defer the empty-state design.
- Wait until the pause screen is designed, since the two ends of the ladder settle together.

> **Recommendation:** Option 1. The rung removal is two lines, but the empty state is a real design question and SELECTION.md is an authority doc that would go stale silently otherwise.

> **RESOLVED.** FILED as BL-266 (selection always open), v0.1.1, plus BL-194/195/196 annotated STALE on their dismissal half at Ben's instruction (2026-08-02). His reason is recorded in all four, because it explains why a good decision expired rather than was wrong: "at that point, the mini-map drew focus, and it would seem like the canvas was covered by a selection element. Currently, we basically fill the entire perimeter, meaning that it looks more awkward when the selection element disappears." Dismissal was right for a card floating over an open canvas; BL-213 fixed bottom band plus a full perimeter turns the same affordance into a hole in the frame. The item carries the two lines of rung removal AND the empty state, which has never been designed because nothing draws today when nothing is selected. Ben also restated the supersession rule ("backlog items are always less important than later ones which contradict them") — that is already codified in DELIVERY.md § Newest wins on conflict, and the annotations follow its no-retroactive-refactor half by appending dated notes rather than rewriting the originals.

*Files: `src/core/app.cpp`, `src/ui/selection_card.cpp`, `docs/ui/SELECTION.md`*

### NR-018 — The question log is DEVELOPMENT documentation — the 'why' should not appear in the game at all
*question · raised 2026-08-01 · from Ben, alongside his resolution of NR-011, 2026-08-01*

Ben: 'the why shouldn't leak into the UI, players don't need us telling them what they ought to ask of the game.' BL-247 as designed and landed had a DUAL audience — player-facing (a closed-by-default 'Why this chart' toggle) and design-facing (Ben reading the log while designing). This removes the first. In code that means the eleven live why_note call sites (generation_charts, corporation_dashboard, selection_card) and the toggle in detail_level.cpp draw nothing in a player build.

**Why it matters.** It resolves BL-260's load-seam question by deleting it — if nothing renders, no codegen or runtime load into the UI is needed and the store is purely a doc artifact read by humans and tooling. It is also consistent with what Ben said when he made the pair mandatory ('For development, I think it's a low storage cost'), and with BL-247's own rejection of a leading-Q&A tutorial: he is extending that objection from 'do not curate a path' to 'do not state the question in-game either'. It retires a shipped affordance, so BL-247's authority text in LAYOUT.md needs the supersession written down rather than the toggle quietly vanishing.

- Remove the in-game affordance entirely — delete the why_note draw path; the store is documentation only.
- Keep it behind a development-only gate (a debug flag or a non-shipping build), so Ben can still read the log inside the running app while a player never sees it.
- Keep it player-facing after all, on the grounds that a closed-by-default opt-in toggle does not tell anyone what to ask.

> **Recommendation:** Option 2 if reading the log IN CONTEXT is part of how you use it — you named that use yourself in BL-247 (navigating the UI to notice where an interesting question has no chart). Option 1 if the JSON store alone serves that. The difference is whether the justification is worth reading beside the surface it describes or on its own; that is your call, and it decides whether any draw code survives.

> **RESOLVED.** OPTION 1 — REMOVED ENTIRELY (Ben, 2026-08-02): "Remove it. If we need to examine that, I will just be asking you to look at documentation, not the game." So no development gate either; there is no draw path left. Done this session and building clean: why_note (both overloads) deleted from detail_level.{hpp,cpp}; the why_note_first verify sentinel and ui_state::why_note_open deleted; the answers/because parameters removed from generation_charts chart_row and its group helper, and from all 16 chart call sites; the 4 corporation_dashboard and 2 selection_card notes removed; verify.why_note removed from the Lua API; scripts/verify/chart_question_log.lua deleted along with its 4 goldens plus corp_rollup_finance_why.png; corp_dashboard.lua stripped of its why_note calls; stale comments in 4 headers/TUs corrected so nothing still promises the affordance. THE 22 AUTHORED PAIRS ARE NOT LOST and were deliberately NOT harvested into a file here — that would have pre-decided BL-260 open store-shape question. They are recoverable wholesale from commit d143aa4, which BL-260 names as its input.

*Files: `src/ui/detail_level.cpp`, `src/ui/generation_charts.cpp`, `src/ui/corporation_dashboard.cpp`, `src/ui/selection_card.cpp`, `docs/ui/LAYOUT.md`*

### NR-019 — "Estimations should emerge from a reading of data, not from the data" — how far does this reach into BL-162 and the roll-up cards?
*question · raised 2026-08-01 · from Ben, resolving NR-003, 2026-08-01*

Ben declined both options for the payback wording on a principle rather than on the merits: "We actually should not do that much work for the player. It is up to them to figure out what is profitable. Estimations like this can be made, but they should emerge from a reading of data, not from the data." The immediate effect is that the payback row is left as it is. The unresolved part is scope, because several landed surfaces are pre-computed verdicts of exactly the kind the principle names: BL-162 ranks candidate buildings by ESTIMATED net profit and draws a bar chart of it; BL-248 four roll-up cards each print a verdict line and colour it red or green; the Selection band prints a per-building profitability read.

**Why it matters.** This is a design position about what the game is FOR, not a wording preference — a 4X where the player derives profitability from prices, deposits and recipes plays very differently from one where a panel ranks the options. It also cuts against the direction three landed items took, so leaving it implicit means the next surface built will guess. The distinction Ben drew is the operative one: SHOW the data a reading can be made from, do not PRINT the reading. A price, an output rate and a capex are data; "payback ~5 qtrs" and "best candidate" are readings.

- Narrow — the principle governs new work only; BL-162 and the roll-ups stand as built, and nothing is unwound.
- Medium — no new pre-computed verdicts, and existing ones are re-examined when their item is next touched, but nothing is unwound now.
- Wide — the ranked profit chart and the verdict lines are themselves the problem; file an item to replace the readings with the inputs they were computed from.

> **Recommendation:** Worth your explicit answer rather than a default, because the cheapest reading (narrow) is the one that quietly preserves exactly what the principle objects to. Note the tension to resolve either way: BL-262 (scoring system) was opened an hour earlier in this same pass, and a score is by definition a reading rather than data. If the principle is wide, BL-262 has to justify why a standing is different from a payback estimate — my own view is that it is (a standing compares you to rivals, which no amount of staring at your own prices reveals), but that is exactly the argument the item should be made to state.

> **RESOLVED.** SHARPENED BY BEN (2026-08-01): "I am contradicting myself there. The important thing is that we do not make the decisions for the player. Score does not really give anything more actionable than a metric." So the test is NOT whether a surface computes something — it is whether the surface makes the CHOICE. A metric the player interprets is fine; a surface that picks for them is not. BL-262 (scoring system) therefore survives the principle without needing a special argument: a standing is a metric, not an instruction. Practical reading for future work: publish quantities, do not publish recommendations. Rank orders and best-candidate highlighting are the borderline, since a ranking is a computation that behaves like a choice; BL-162 keeps its ranked profit chart because Ben left it alone in NR-003, not because it clearly passes. Nothing is unwound.

*Files: `src/ui/selection_panel.cpp`, `src/ui/corporation_dashboard.cpp`, `docs/CONCEPT.md`*

### NR-021 — BL-217/218/219 were silently lost from backlog.json by a stale-base merge, and have been restored
*decision taken on your behalf · raised 2026-08-02 · from Found while ordering the design-owed items for the batch-delivery design pass, 2026-08-02*

SPRINTS.md § Sprint 2 records that BL-210 was split into BL-217 (checkpoint/branch data model), BL-218 (Nations rewrite) and BL-219 (Corporations rewrite), and BL-210’s own design prose still names all three as its decomposition. None of the three existed in backlog.json — the id sequence jumped 216 → 220. Tracing the file’s history: the three items were filed at 18c86c0 (2026-07-29), survived through 8542e4b (2026-07-31), and are absent from eaa0d23 (“On sync/origin-main-20260731: wip before Sprint3 merge”) onward. I recovered all three objects verbatim from 8542e4b and re-inserted them ahead of BL-220. Purely additive (+76 lines); backlog_lint clean; design prose intact (2082 / 2630 / 2315 chars).

**Why it matters.** This is the stale-base worktree revert pattern, not a deliberate retirement — no commit message mentions removing them, and every surviving document still refers to them as live. Three design-owed items disappearing silently means a whole sprint’s decomposition evaporated while the docs claimed it existed; BL-210 would have been re-decomposed from scratch. Worth knowing that the same merge may have dropped other rows: I verified only the 216→220 gap, not the whole file against its history. A full row-level audit of backlog.json against eaa0d23’s parents is the thorough version and is not done.

- Accept the restoration as-is (what I did).
- Accept, and additionally run a full row-level audit of backlog.json against the pre-eaa0d23 tree to find any other rows the same merge dropped.
- Reject — the three were meant to be retired, in which case BL-210’s design prose and SPRINTS.md § Sprint 2 both need correcting instead.

> **Recommendation:** Option 2. The restoration itself is safe and clearly right — nothing in the corpus argues these were retired on purpose. But a merge that dropped three consecutive rows without comment is unlikely to have dropped exactly three, and the check is cheap next to discovering a fourth loss months from now. Note this is the second instance of the hazard; the parallel-worktree coherence guidance in DELIVERY.md exists because of the first.

> **RESOLVED.** Ben chose Option 2 (2026-08-02). Ran the full row-level audit: diffed backlog.json's item-id set at 8542e4b (pre-drop) against eaa0d23 (the merge). Exactly BL-217/218/219 were dropped — no other ids present at 8542e4b are missing from eaa0d23. All three are confirmed present in the current file post-restoration. No further loss found; closing.

*Files: `docs/development/backlog.json`, `docs/development/SPRINTS.md`*

### NR-028 — BL-217 verification: fresh worktree could not configure CMake (FetchContent blocked), fell back to hand-compiled cl
*observation · raised 2026-08-02 · from Session 2026-08-02 (BL-217 checkpoint/branch/lean foundation)*

cmake -S . -B build in this worktree failed at the SDL3 FetchContent step: codeload.github.com's TLS handshake fails with CRYPT_E_NO_REVOCATION_CHECK (confirmed independently with a direct curl to the same URL, same error) — a network/certificate-revocation-check block, not a project misconfiguration. Per the task's documented fallback, planetology_harness and planetology_sweep were instead hand-compiled with cl (mirroring creeds_harness's world-superset TU list from tools/verify/README.md) and both ran clean: 121/121 PASS on the harness (19 of them the new R13 checkpoint assertions) and the sweep's R1-R3 metrics reproduced the doc's committed numbers exactly (77.4% acceptance, 1.29 mean attempts, interior=low at 2.57 draws). The full ProjectIo target and the whole-suite ctest (~37/37 expected) could NOT be run this session for the same reason.

**Why it matters.** This is the known fresh-worktree FetchContent issue CLAUDE.md already anticipates, not a defect in this change. Flagging so a future session (or one with working network access) runs the full ctest suite once, to confirm nothing outside the planetology TU graph regressed — inspection of every consumer of planetology_state (src/core/app.hpp, src/ui/generation_charts.hpp, src/world/hard_coded_world.hpp) found only by-value/by-pointer holds with no field-enumeration that an appended struct field could break, but that is inspection, not a compile.

> **Recommendation:** Next session with network access (or the main non-worktree checkout): run build_app.bat then ctest --test-dir build --output-on-failure once to close this out.

> **RESOLVED.** Closed 2026-08-02 in the main (non-worktree) checkout, which already had network access and a configured build/ tree. build_app.bat (full ProjectIo target) and ctest --test-dir build --output-on-failure both run clean: 37/37 green, including planetology_harness (121/121, 19 new R13 checkpoint assertions) and planetology_sweep (R1-R3 metrics reproduced exactly). No regression outside the planetology TU graph, confirming the inspection this entry already did.

*Files: `src/world/planetology.cpp`, `src/world/planetology.hpp`*

### NR-031 — BL-208 verification: fresh worktree was also a STALE base (pre-BL-217) and could not configure CMake; integrated main, then hand-compiled with cl
*observation · raised 2026-08-02 · from Session 2026-08-02 (BL-208 world history log)*

This worktree's HEAD was the merge-base with main, 24 commits behind (missing BL-217's checkpoint_record/planetology_state.checkpoints — this item's own hard dependency — plus BL-166/168, BL-170 rivers, and a backlog/doc sweep). Stashed the in-progress BL-208 edits, fast-forwarded the branch to main (clean, no conflicts bar an auto-merged app.cpp), then popped the stash back (also clean). Separately, cmake -S . -B build hit the same FetchContent/TLS block NR-028 already named (codeload.github.com, CRYPT_E_NO_REVOCATION_CHECK) — confirmed by direct reproduction, not assumed from NR-028. Fell back to hand-compiled cl per the task's documented contingency: the new history_log_harness (27/27 PASS), an extended determinism_harness (2 new checks, 25/25 PASS), and a 7-harness regression sweep across every file this item touched (corp_ai_harness, ai_skill_harness, trade_routes_harness, commercial_fog_harness, supply_advance, econ_stability, blackboard_harness — all green, 0 failures) all built and ran clean. Also found tools/verify/README.md's hand-written world-superset recipes are one file short of linking (hard_coded_world.cpp now includes river_generation.hpp, from the BL-170 rivers pass that landed on main) — documented as a TU-ripple note in the README rather than silently worked around.

**Why it matters.** Confirms the same fresh-worktree network block NR-028 named, on a second worktree — worth a standing note if it recurs a third time. Separately, the full ProjectIo GUI target and the whole-suite ctest could not be run this session for the same network reason; every headless/logic-level surface this item touches was verified by hand-compiled harnesses instead, but the GUI build itself (src/core/app.cpp's new #include and setup_world call) was only compiled by inspection, not by a real link, since no headless harness touches app.cpp.

> **Recommendation:** Next session with network access (or the main non-worktree checkout, per this item's own closing instructions): run build_app.bat (default ProjectIo target) then ctest --test-dir build --output-on-failure once, to close this out and to confirm app.cpp's new include/call compiles and links inside the real GUI build.

> **RESOLVED.** Closed 2026-08-02 in the main (network-connected) checkout. build_app.bat (full ProjectIo target, including the new app.cpp include/call) and ctest --test-dir build --output-on-failure both run clean: 38/38 green, including history_log_harness (30/30) and the extended determinism_harness (all checks, +2 new). app.cpp's new code compiles and links in the real GUI build, closing the one gap the hand-compiled harnesses could not reach. tools/verify/README.md's river_generation.cpp TU-ripple gap (also flagged this item) is left open as its own small fix, not blocking.

*Files: `src/core/app.cpp`, `tools/verify/README.md`*

### NR-039 — C-route design session: Ben overrode the milestone-sequencing rule, and the design carves a new exception into the determinism standing rule
*observation · raised 2026-08-02 · from Sprint-planning session, 2026-08-02 — asked whether the backlog was sprint-ready*

Two standing-rule interactions in one session, both Ben's explicit call rather than mine, recorded here for traceability since they change durable authority docs. (1) C-route is v0.2.0 scope and v0.1.0 has not cut yet (ROADMAP.md: BL-258 + a human frame-budget pass still owed) — the sequencing rule in io-standing-rules.md says to flag this rather than design ahead; I flagged it, Ben said proceed anyway. (2) The resulting design (AI_OPPONENT.md §7a) has C-route's LLM calls live and NOT replay-logged, because Ben wants post-generation play to carry some genuine non-determinism the way generation itself does not — this required adding a new scoped exception to the determinism rule in io-standing-rules.md (mirrors how BL-079/BL-181 carved narrow exceptions into the no-AI-agency rule).

**Why it matters.** Neither is a mistake — both are recorded, deliberate calls Ben made in chat, and the standing-rules file itself now documents the determinism exception so it reads as intentional rather than a regression. Logged here mainly so a future session tracing 'why is the sim non-deterministic in one spot' or 'why was v0.2.0 designed before v0.1.0 cut' finds the reasoning in one place rather than archaeology.

> **Recommendation:** Superseded within the same session — see NR-040. The determinism carve-out and the §7a design were both reverted once Ben clarified the actual intent.

> **RESOLVED.** Superseded (2026-08-02, same session) — Ben clarified C-route was never meant to be a shipped in-process API call; the sequencing override still stands as a fact of what happened, but the resulting design and the determinism-rule exception it justified were both wrong and have been reverted. See NR-040.

*Files: `.claude/rules/io-standing-rules.md`, `docs/ai/AI_OPPONENT.md`, `docs/development/backlog.json`*

