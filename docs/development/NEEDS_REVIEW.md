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

*18 entries — 18 open, 0 resolved.*

---

## Open

### NR-164 — Re-stress generation and time-lapse feel when playable
*question · raised 2026-08-11 · from 2026-08-11 notes session with Ben.*

Ben wants to re-stress-test world generation and the staged-generation time-lapse once the game is playable, to judge whether it feels alive rather than just correct. RULING 2026-08-13 (Ben, elicitation form): keep open until playable - not scheduled, not folded into another item.

*Files: `docs/ui/STARTUP.md`*

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

### NR-256 — --autostart-play still terminates unattended, cause not established
*observation · raised 2026-08-16 · from Three unattended launches while adding the flag (commit e4a087a).*

--autostart-play removes --autostart-windowed's 120-frame cap so the window stays open for a human to look at. It works interactively â€” Ben used it and reported on what he saw. But launched unattended from a background shell it has terminated on its own three times, at roughly 20s, 34s and ~60s: exit code 0, no crash log, no exception, output simply ending after the warm-start timings. The frame cap is definitely not the cause (it is gated on autostart_mode::smoke and the variable timing rules it out anyway). run()'s loop has only three exits: SDL_EVENT_QUIT, SDL_EVENT_WINDOW_CLOSE_REQUESTED, and m_quit_requested, so something is delivering a close.

**Why it matters.** Two candidate causes and I could not separate them from this session: (a) a genuine stray quit/window-close event in the autostart path, which would be a real defect, or (b) the background-shell environment reaping a GUI process whose launching shell is not interactive, which would make the flag fine in real use and only untestable the way I was testing it. The interactive evidence points at (b), but 'it worked when a human was watching' is not a diagnosis. Worth settling before anyone relies on this flag for an unattended soak or a long-running capture.

> **Recommendation:** Instrument the three exit paths with a one-line printf naming which one fired, then launch once from a real terminal and once from a background shell. That distinguishes (a) from (b) in a single run each. Cheap, and it is the same two-line diagnostic NR-238 records as the thing that settled a similar this-looks-like-a-regression question.

*Files: `src/core/app.cpp`, `src/main.cpp`*

### NR-259 — player_seed_sweep had never once passed under ctest â€” a 60s timeout on a 69s tool, silent because a Timeout looks like nothing
*observation · raised 2026-08-16 · from The full 78-test ctest run done to verify NR-257's resource_type removal.*

player_seed_sweep was added 2026-08-15/16 and registered in CMakeLists.txt's IO_TEST_SCRIPT_ROOTED_HARNESSES (so it gets the repo root as its working directory) but NOT in IO_TEST_LONG_HARNESSES, so it inherited the 60 s default timeout. It generates one full world per seed, 24 by default, and takes ~69 s on this box. It therefore timed out on every ctest run from the day it was added, while passing perfectly standalone - which is how it was used, and why nobody noticed. Fixed by adding it to IO_TEST_LONG_HARNESSES (240 s, ~3.5x headroom); it now passes at 71.8 s.

**Why it matters.** The failure mode is the point, and it is the mirror of the one Sprint 18's retro named. That retro found a check running GREEN while pointed at a deleted tab - coverage that was not coverage. This is the same defect from the other side: a check that had never run at all, reporting as a Timeout, which reads as infrastructure noise rather than as 'this harness has never verified anything'. Worth a standing habit: when a harness is added to the gate, run the GATE once, not just the exe. Two of the three ways a check can be worthless - green-but-blind, and never-executed - are both invisible from the harness's own output.

> **Recommendation:** No further action on this instance; it is fixed and verified. Worth considering whether a new harness's first ctest run should be part of the checklist in the verifier-headless skill, since 'passes standalone' is what both the author and the skill's own Procedure section naturally check.

*Files: `CMakeLists.txt`, `.claude/skills/verifier-headless/SKILL.md`*

### NR-345 — The unit card is unreachable to the verify harness - battle-visual work needs a unit-selection driver first
*observation · raised 2026-08-18 · from Battle-visual design browse (2026-08-18 session): toured canvases and military surfaces via --verify captures.*

verify_api.cpp has select_tile (selects the TILE entity directly) and select_building, but no binding selects a UNIT - the click-cycle (unit -> building -> tile) exists only in live mouse handling. So the Selection unit card (Strength / Roster pages) cannot be captured headless, and any battle-visual requirement over units cannot be verified until a driver hook (e.g. verify.select_unit or a cycling select) is added. Pairs with the known no-unit-marker gap in MILITARY.md section What is absent.

*Files: `src/core/verify_api.cpp`, `docs/military/MILITARY.md`*

### NR-392 — novel-work: a worktree agent had no sanctioned way to build a harness, so it authored build_gen_harness.bat
*novel-work · raised 2026-08-20 · from Lane B1 (Sprint B2, road cuts), agent report + main-session diff review, merged at 530eb87.*

Nothing in the reading list owns the question how does a worktree agent build a harness. A fresh worktree has no configured build tree and a CMake configure pulls SDL + Lua over FetchContent, which is not worth paying for one harness. The agent wrote build_gen_harness.bat at the repo root: it globs srcworld*.cpp minus the four sol2 TUs exactly as io_world_obj does, so unlike the README hand-written cl recipes it cannot drift stale - which is the standing complaint against those recipes. Merged with the lane. Two things owed and both need your permission: folding this into the verifier-headless skill, and naming road_reach_census there so it becomes a permanent asset rather than a loose tool.

### NR-400 — The D4 import tariff has NO authoring path - nothing in src/ can enact one
*observation · raised 2026-08-20 · from Pre-compile static review of the integrated four-lane batch, 2026-08-20; blocker fixed at bd238d5.*

Outside law.{hpp,cpp} and market_clearing.cpp there is not one reference to law_effect_kind::import_tariff anywhere in src/. No corp_verb, no UI control, no generation seeding, no agent-seam command creates a tariff law; seed_prototype_laws seeds only the extraction levy. So in a real campaign any_import_tariff_enacted is permanently false and the entire tariff pass is unreachable. The mechanism is built, proved and conserved - in a harness fixture. If D4 acceptance is a cross-border sale pays a duty into the market nation treasury, that is NOT met in the shipped binary. Deliberately not fixed here: seeding a tariff at generation changes every generated world and is a design call, and the granted nation-grain scorer enacting one mid-campaign is the seam the work was shaped for. Your call which of the two it should be.

### NR-402 — Six new harnesses joined the routine ctest gate at the 60s default, two of them census-class
*observation · raised 2026-08-20 · from Pre-compile static review of the integrated four-lane batch, 2026-08-20; blocker fixed at bd238d5.*

The CMake GLOB registers tools/verify/*.cpp automatically, so this batch six new harnesses entered the routine gate silently. road_reach_census (3x make_hard_coded_world) and rival_military_seeding_harness (4x) are the exposure. substrate_census own CMake note records ~62s per world in a Debug tree and parked itself in IO_TEST_SWEEP_HARNESSES for exactly this reason; road_reach_census is by name the same category and is not in that list. Expect a Debug-tree Timeout, which NR-259 calls a silent failure. Also: rival_military_seeding_harness has never been named in tools/verify/README.md - it arrived undocumented in an earlier commit, not in this batch.

### NR-480 — Concurrent agents get worktrees at the SESSION base again — B2 was five commits behind
*observation · raised 2026-08-21 · from Sprint B2 (Lane B), which caught it because the brief told it to check.*

The B2 worktree was created at 4a504d7, FIVE commits behind the branch tip, and the agent fast-forwarded itself before reading any code. This is NR-459 recurring. It mattered concretely: at the stale base the three road cuts looked unlanded and BL-516's water kinds were absent, so the agent would have rebuilt work that already existed against an enum that had changed.

**Why it matters.** NR-459 recorded this once and the mechanism has not changed. What DID work is the mitigation: briefing every agent to verify its base and fast-forward before reading anything, as its first action. That is now worth making standing practice rather than a per-brief habit — three agents launched today, and the one that was told to check, caught it.

> **Recommendation:** Add the base-check to DELIVERY.md's sub-agent section so it is not a thing each brief has to remember.

*Files: `docs/development/DELIVERY.md`*

### NR-513 — novel-work: the phantom register is a new standing artifact, and it has no lifecycle
*novel-work · raised 2026-08-22 · from Phantom-feature scan, 2026-08-22.*

docs/development/PHANTOMS.md is a new document class - a scan output that points at design sessions. Nothing in the corpus owns 'a list of things with no owner', and it overlaps three existing stores: NEEDS_REVIEW.json (open calls), backlog.json (work), and MILITARY.md § What is absent (a per-doc holes list, which is the pattern this file generalises). It is written as transient - stale by design once the sessions run - but nothing says who prunes it or when.

**Why it matters.** An unowned list of unowned things is how a fourth store accretes. The alternative shape, and probably the better one, is no central file at all: every doc that owns a partially-built system carries its own 'What is absent' section, MILITARY.md's model, and the scan becomes a periodic check rather than a document.

- Keep PHANTOMS.md as a transient scan output, pruned as sessions land.
- Dissolve it into per-doc 'What is absent' sections and re-run the scan periodically instead.
- Keep it and give it a query tool if it grows.

> **Recommendation:** Option 2 as the destination, option 1 to get there - the file is useful right now because ten sessions are not yet run. Once each subject has an owner, its rows move into that owner's absent-list and the file goes away.

*Files: `docs/development/PHANTOMS.md`*

### NR-532 — The design register is live — 41 open calls across ten sections, as a form
*observation · raised 2026-08-22 · from Ben, 2026-08-22: "now please revisit each one and open forms for answering the open questions."*

Published at https://claude.ai/code/artifact/debe7b8f-7315-429a-a805-0e295e9405bc. Every open question across the eight new authority docs plus SYSTEMS.md § The progression chain plus four cross-cutting calls, gathered into one form: 41 questions in 10 sections. Each carries its evidence, 3-5 options with one marked as suggested, and a free-text field that overrides the options. Progress is tracked per section; "Copy all answers" puts everything on the clipboard in one block. It is a LIVE DOC - radios and contenteditable fields are captured as edits, so answers reach a watching session directly; deliberately no <textarea> and no <select>, neither of which is captured. Answers also persist to localStorage as a per-viewer draft.

The generator is committed rather than being a one-off (CLAUDE.md § Tool creation is skill creation): tools/session/register/questions.js is the canonical question set and build.js emits the HTML. Verified to regenerate byte-identically. Republish to the same URL to keep answers in place.

**Why it matters.** The open questions were the point of writing the docs as capture rather than design, and they were spread across ten files. A form is the difference between 41 questions that get answered and 41 that get skimmed. It also means the answers arrive in one structured block that can be propagated in a single pass, the way the six nations rulings were.

> **Recommendation:** Answer in any order. The four that change the most downstream: LOGISTICS Q1 (what generates LP), EVENTS Q4 (drive the collapse metagame or express it - it decides the system's size), PEOPLE Q2 (one bias or several - cheapest to overturn now), and NATIONS Q1 (whether the grant reaches a rival, which blocks the player-facing halves of two items).

*Files: `tools/session/register/questions.js`, `tools/session/register/build.js`*

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

### NR-576 — Review queue purged 240 -> 17 with the backlog: 206 retired, 16 settled by reading, and the residue is the form
*decision taken on your behalf · raised 2026-08-23 · from Review-queue sweep, 2026-08-23. Ben: "if a referred item is in the backlog purge list, then delete it... otherwise work through these, grouping similar items into the same form".*

240 open entries went in; 17 came out. RETIRED (206): every entry whose only backlog references are ids <= BL-568, i.e. items snapshotted into archive/backlog-purged-2026-08-23.json. SETTLED BY READING (16), each carrying its answer: NR-512 (CONCEPT.md now opens on the mercenary company and calls the governing-body framing superseded); NR-525..529 (the five doc reviews, superseded by the design register, NR-532); NR-562 (answered in code by BL-569, which folds the holder vector into state_hash and the save); NR-424 (BL-521's click injection names it as the thing it closes); NR-319/452/484/487 (folded into NR-548); NR-459 (into NR-480); NR-488/425 (into NR-547); the duplicate-id NR-513 observation (fixed with BL-536). Everything removed is FROZEN verbatim in archive/needs-review-purged-2026-08-23.json with a resolution saying why. The 17 survivors are the entries whose subject is a file that still exists and whose ask is not tied to retired work; five were re-verified against the tree this date and carry a verified_2026_08_23 field. The 17 survivors are asked as 14 questions in four sections - Harness truth, What the harness cannot reach, Live gaps no Sprint 16 item owns, Stores and method - through tools/session/register/review_queue.js, a second question set for the register generator. build.js now takes the set as an argument; the design register rebuilds byte-identical, which is the proof the refactor changed nothing.

**Why it matters.** Two calls were taken rather than asked. FIRST, the survivor test. Read literally, 'refers to a purged item' would have deleted entries that merely CITE a retired item as provenance while asking about a live doc, a live harness or a live process file. The test used instead is whether the ENTRY'S SUBJECT survives the purge - so NR-400 (the tariff has no authoring path) stays although the item that surfaced it is gone, and NR-269 (re-tune the AI cost side) goes although the defect was real, because the work it belongs to is retired. SECOND, archive rather than delete. The instruction said delete; the entries were moved to a frozen cold store instead, mirroring exactly what the backlog purge did an hour earlier. Nothing is lost and the hot file is the queue again.

- Accept both calls - the survivor test and the archive.
- Tighter: retire some of the 17 survivors too (name which).
- Looser: some retired entries should have survived (name which, and they come back out of the archive verbatim).

> **Recommendation:** Accept. The 17 are answered through the review-queue form rather than one at a time; three of them (NR-548, NR-400, NR-402) are small enough to be backlog items the moment you say so.

*Files: `docs/development/NEEDS_REVIEW.json`, `docs/development/archive/needs-review-purged-2026-08-23.json`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

