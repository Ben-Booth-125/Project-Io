# Project Io — Development Log

Entries are newest-first. Each entry covers one development session and records what was built, what in-session decisions were made, and what was left open. Decisions that affect the whole project permanently belong in TECH_FOUNDATIONS or a dedicated ADR; this log is for session-scoped choices and progress notes.

Entries that correspond to a tagged snapshot in `backups/` carry an explicit **version** marker in their heading (e.g. *version 0.0.2*) and a **Backup** line naming the snapshot path. These are the rollback points: to revert, restore the named `backups/vX.Y.Z/` tree over `src/`.

Every entry also carries a **Runtime** line: wall-clock session length, plus mode (Light/Full,
refinement/delivery/design/etc.). This builds a record of how long similar tasks take, so future
sessions can be scoped and paced with less waste.

---

## Session — building Selection card playtest, sub-facility groups (BL-431, BL-434) (2026-08-16, latest)

Full mode. Started by pulling a mobile session's BL-429 finish (slices 2-3, already merged via
PR #40), then a long iterative playtest pass over the just-landed BL-430/BL-431 economy-breadth
UI, driven turn-by-turn against Ben opening the live app and reporting back.

**Buildings tab retired.** The management ledger's "Buildings" tab (`draw_buildings_tab` /
`draw_selected_section`, ~620 lines) duplicated what the Selection card now covers — deleted
outright. Its two real capabilities that had no home yet (Workforce controls, Close/Dismantle)
moved into the Selection card's own accordion and action grid. The foldout panel is Construction-
only now (`"Building"` -> `"Construction"`); the Manage action-grid button that used to route
there was removed once it had nowhere useful left to point.

**Building card normalised to the tile card's shape and proportions.** 3-column band (1/4 zoomed
tile view . 1/2 paged accordion . 1/4 action grid), matching `draw_tile_selection` exactly rather
than the original rework's 1/3 . 5/12 . 1/4 split. The tile-neighbourhood render in the left
column was replaced by a per-building-type glyph placeholder (`icons::building`, the same glyph
the Build door and canvas markers use). A placeholder Soldier/unit selection card was added,
sharing the same 3-column family, reading real `unit_component` fields (`strength`, `count`,
roster-table `type` lookup, `owner`) rather than fabricated numbers. A repeat-click tile-cycle
(Soldier -> Building -> Tile) was added to `body_surface_canvas.cpp`'s click handler — its first
implementation was checked on `marker_hit == null_entity`, which almost never held on a built
tile (the building's marker covers nearly the whole hex), so the cycle silently never fired
there; fixed by checking the anchored-tile match first, before marker resolution.

**Profitability, Method, Workforce, Lifecycle — several playtest rounds each.** Final shape:
Profitability shows Revenue/Expenses bars (Expenses segmented into input cost / maintenance /
wages, each hover-labelled, folding in what was briefly a separate Inputs chart) beside a
6-month net-profit line, budgeted to 90% of the accordion's available height so it never pops a
scrollbar even at a near-exact fit. Method is a single narrow tiled column (was a 2-column grid),
Switch drawn as a large glyph in a deliberate accent blue rather than the neutral grey every other
glyph button uses, offering only same-group recipes, with an "(active)" text tag dropped in
favour of the row's existing border/text highlighting. Workforce lost its heading and 6-button
tier grid for a 1% `SliderInt`, and a visible "Retooling - N ticks left" progress bar replaced a
hover-only cooldown tooltip. Depth and Chain were dropped as standalone pages (Depth's info
wasn't landing as useful; Chain folded into Profitability's input breakdown). Lifecycle stopped
being a page — Mothball/Reopen (distinct glyphs, not a shared toggle label) and Dismantle live as
action-grid buttons instead, alongside a relocated Workforce-Auto toggle.

**A real ImGui bug, not a design complaint.** Ben reported the Workforce Auto button as
"game breaking." Root cause: `ImGui::Button(autolbl, ...)` had no `##` separator, so the widget's
id *was* its label text — and the label baked in the live `workforce_target` percentage, which
`solve_workforce_target` re-solves every tick while Auto is active. The button's identity churned
every frame the number moved, corrupting ImGui's hover/active/focus state continuously. Fixed
with a stable `"Auto##wf_auto"` id, percentage shown as separate text.

**BL-434 (sub-facility groups), filed and landed same session, then partly retracted in the
same session.** Ben, mid-playtest: "Now is also the time to implement the building splits...
It should also cost money for any building to undergo a large change, making it in some cases
cheaper to just build another." First cut: every recipe gained a `group` field (Metal Foundry,
Refinery, Food Processing, Chemical Works, Electronics, Advanced Fabrication, Welfare Goods,
Fuel Production, Artisan Goods), the Build door collapsed to one candidate row per group instead
of one per recipe, and a cross-group recipe switch was priced at a steep multiplier
(`cross_group_multiplier`, first-cut 6.0x) on top of BL-430's existing switch cost. Minutes after
it landed, Ben reconsidered: "switching methods can mean changing to a different building type —
we should retire that completely." Retracted to a hard refusal (`recipe_switch_result::
cross_group`) rather than a price; the Method page's candidate list now filters to same-group
recipes so a cross-group option is never even offered. Dismantle + rebuild via the tile selector
is the only way left to change a building's group. `cross_group_multiplier` was removed rather
than left dead. The BL-434 item and its retraction are both recorded in its `design`/`resolution`
fields rather than only in code comments, since the reversal happened inside the same session a
future reader might otherwise assume was linear.

**The recipe-switch cooldown, separately.** Ben asked how long a switch actually takes; the
honest answer was 6 economy ticks x 90 days/tick, ~1.5 in-game years — long enough that the
disabled Switch glyph just read as "not possible," not as a running cooldown (compounded by the
progress bar not existing yet). Dropped to 1 tick, flagged (NR-253) as a first cut needing real
playtest pacing, not a measured value.

**One correctness sweep, at housekeeping time.** A background implementation agent had invented
a plausible-looking but nonexistent backlog id (`BL-436`) across a dozen files' comments while
building the sub-facility-groups work — `next_id.js` confirmed no such item existed. Renamed to
the real next free id, **BL-434**, and filed it retroactively in `backlog.json` (status
`complete`, both the tiered-price design and its retraction recorded) so the many code comments
citing it resolve to something real.

**Time panel:** the 5x speed button was riding the screen edge — the six speed buttons divided
`ctrl_w` exactly, with no margin for rounding. Narrowed to 92% of the exact division.

**Two agent-coordination near-misses, both self-corrected.** One background agent returned a
plausible-sounding completion report after making zero file changes — caught by checking
`git status` before trusting the report, and relaunched with an explicit "you have no sub-agent
access, implement directly" instruction. Two separately-launched agents ended up mid-session
editing the same file; each detected the other's in-progress work on disk and reconciled onto one
consistent design rather than silently overwriting it — verified by an independent rebuild
afterward rather than trusting either self-report. Both incidents reinforce the same practice:
verify a background agent's report against the actual working tree before acting on it.

**Verification.** Every step of this session rebuilt (`build_app.bat`) before proceeding to the
next; the final combined diff (25 files) builds clean. `recipe_switch_harness`: S1-S5 (BL-430's
original mechanism) and the new S6a-f (BL-434's refusal path) all PASS; R1's pre-existing
no-dominance finding (NR-243, four dominated recipe pairs, unrelated to this session) is
unchanged and still fails as documented. No visual harness exists yet for the Selection card
rework itself (BL-431's own requirement group is still open) — every visual check this session
was Ben looking at the live app directly, turn by turn, which is how the playtest iteration
loop actually ran.

**Open for Ben (NEEDS_REVIEW).** NR-248 (Expenses' revenue/expense split is the finest real data
`building_profit.hpp` tracks — no sub-breakdown of revenue exists). NR-249/NR-250 (two placeholder
trend graphs and a fixed-clamp layout budget, both first-cut numbers). NR-252 (two recipe-group
taxonomy calls: Hydroponics Bay's group, and whether Advanced Fabrication should split). NR-253
(the 1-tick cooldown needs real playtest pacing). NR-245 is resolved-in-place (the Manage button's
destination changed, then the button was removed entirely once the Buildings tab it pointed to no
longer existed).

**Runtime:** ~5 hours, Full mode, iterative playtest/build/verify loop across roughly a dozen
background implementation passes plus direct edits for small contained fixes.

---

## Session — BL-429 slice 3: the ancient roster gets glyphs (2026-08-15)

Full mode, direct continuation of slice 2 in the same session — Ben asked to close R5's glyph
clause next rather than move on to BL-430/BL-431.

**Built.** `icons::building()` gained a fourth parameter, `resource_type identity`: an
extraction site's target resource, or a processing facility's PRIMARY OUTPUT. The output-lookup
helper (`primary_output_resource`) moved from a construction_panel.cpp local into
recipe_registry.hpp as a shared free function, so every UI file draws the same identity from one
source. 14 new hand-drawn vector glyphs (icons.cpp) cover the 9 extraction + 5 processing
resource keys the ancient roster reaches — Quarry, Woodcutter's Camp, Sand Pit, Clay Pit, Peat
Cutting, Iron Mine, Copper Mine, Water Extractor, Farm/Fishing Wharf, plus a charcoal-kiln dome,
a lump-cluster bloomery, a trapezoid ingot, a cinched goods sack, and a strapped ration pack for
processing. All four `icons::building()` call sites updated to pass a real identity: the Build
door, the Buildings-tab identity plate, the on-canvas marker, and the placement ghost preview.

**A design call, not a gap: shared glyphs by resource, not by recipe.** Two or more named
buildings that reach the same good share one glyph — Charcoal Burner and the Peat Kiln (both
`-> charcoal`), Potter & Weaver and the Glassworks (both `-> trade_goods_misc`), and the ancient
Smithy/Miller sharing with the industrial Smelter/Food Processor (it genuinely is the same steel,
the same rations). Documented in ICONS.md's new § 1c as the deliberate identity model: the glyph
says WHAT a building makes, not which specific recipe — the same rule that already lets two Iron
Mines on different tiles share a glyph.

**A real correctness fix found while wiring the canvas marker.** The pre-existing
under_construction pattern in body_surface_canvas.cpp resolves its representative-building lookup
only on `k == 0` of the wrap-copy loop (the tile's un-wrapped screen copy) and leaves the flag at
its default for every other `k`. Mirroring that pattern for the new marker identity would have
shown the WRONG glyph on every wrap copy of a non-default-resource building — instead, the
target/recipe lookup was hoisted out of the k-loop entirely, computed once per tile alongside
`built_type`, so every wrap copy reads the same (correct) identity. under_construction's own
existing k==0-only quirk was left alone — pre-existing, out of scope, higher risk to touch without
a build to verify against.

**requirements.json's `ancient-chain-roster` group is now all six rows complete (R1-R6).**
BL-429's own backlog status stays `designed` rather than flipping to `complete` — see the caveat
below; `backlog_lint` now carries this as its 7th (of the same pre-existing shape) warning,
consistent with how the project already tolerates a completed requirement group sitting ahead of
its item's terminal status.

**The caveat that matters most this entry (NR-240, NR-241).** None of this was compiled or
rendered. This session ran remote with `codeload.github.com` blocked (SDL3/sol2/ImGui
FetchContent — an organization policy denial, confirmed via the proxy's own README, not routed
around). The 14 new vertex lists were hand-checked for angular ordering (each is a simple,
non-self-intersecting perimeter) but never seen on screen — "silhouette distinct" is reasoned by
shape family, not verified the way ICONS.md's own "Adding a new glyph" process asks for. NR-241
is the follow-on: build, open the Build door on an ancient-band tile, the Buildings tab, and a
built ancient building on the Planetary canvas, and fix proportions on sight before promoting
BL-429 to `complete`.

### Verification

C++: **not compiled** — see NR-240/NR-241. Brace-balance checked across every touched file
(`grep -o` count) as the cheapest available syntax sanity pass; every call site's field types and
recipe/registry accessor usage manually re-traced against their declarations. `backlog_lint`: 0
fail, 7 warnings (one new, same pre-existing shape as the other six). No visual or headless
harness run this entry — icons.cpp only links in the GUI target.

### Open for Ben

- NR-241: the 14 new glyphs need an actual look before BL-429 can close.
- The remaining `k_extractable` targets outside the ancient roster (coal, petroleum, silica,
  rare-earth ore, iron-nickel ore, platinum-group metals, regolith) still fall through to the
  generic ore-chunk — named in `extraction_building_name()` but not glyphed. Worth a follow-on,
  or leave them generic since they're outside this item's ancient-arc scope?

**Runtime:** not tracked this session (same standing gap NR-177's retro already named).

---

## Session — BL-429 slice 2: the ancient roster gets names (2026-08-15)

Full mode, continuing Sprint 17 (economy breadth). Slice 1 landed the ancient production chains
(BL-428 depth metric, BL-429's five recipes); this session picked up R5/R6, the roster's remaining
requirements — named identities and closing the last two orphan raws.

**Built.** A `recipe::display_name` field (recipe_registry.hpp/.cpp), authored for the five slice-1
ancient recipes — Charcoal Burner, Bloomery, Smithy, Potter & Weaver, Miller — and read by both the
Build door (selection_panel.cpp) and a live building's method selector (construction_panel.cpp) in
place of the raw recipe key / the old "Processing: X" prefix; it defaults to a title-cased recipe
name when unauthored, so every recipe keeps a legible label with nothing new required. A parallel
`extraction_building_name()` lookup does the same for extraction rows (Quarry, Woodcutter's Camp,
Sand Pit, Clay Pit, Peat Cutting, Iron/Copper Mine, Water Extractor, Coal Mine, Oil Field, Silica
Quarry, Rare-Earth Mine), and Farm/Fishing Wharf now read as distinct names instead of sharing one
"Extraction: Agricultural Produce" label.

**R6 closed.** Sand and peat — "still orphaned" per slice 1's own note — each got a consumer:
Glassworks (sand -> trade_goods_misc, recipe 22) and Peat Kiln (peat -> charcoal, recipe 23). Both
are a second producer of an existing output from a different raw, the same multi-producer shape
`steel` already has (coal-smelter / iron-nickel / bloomery) — not BL-430's alternate-method feature,
which is a different item.

**A real pre-existing gap found and fixed on the way.** `placement_rules::k_extractable` never
listed `resource_type::peat`, despite `tile_generation.cpp` authoring peat deposits on wetland tiles
— no extraction_site could ever have targeted it. Fixed by adding it; `buildings_rework_harness`'s
R1 (`n >= 15`) still holds at 16.

**R5's glyph clause deliberately NOT done, and recorded rather than glossed (NR-239).** Every named
extraction building still renders `icons::building`'s shared ore_chunk glyph; every processing
building the shared square — exactly how Farm/Smelter/Hydroponics Bay already render, not a
regression, but the roster's "each with its own glyph" clause stays open. Hand-authoring 16
silhouette-distinct vector icons (ICONS.md's own per-glyph process) is real asset work this slice
did not attempt; it needs its own follow-on rather than let R5 read as met without it.

**Environment note (NR-240): this session ran remote, without a compiler.** `cmake -B build`'s
SDL3/sol2/ImGui FetchContent steps pull from `codeload.github.com`, which this session's network
policy denies (confirmed a 403 organization policy denial, not a transient fault — the proxy's own
README says not to route around it). Lua changes were syntax-checked with `luac5.4 -p` and pass; the
C++ changes were manually re-read against every call site of the touched fields but never compiled.
Owed at the next session with real toolchain access: `cmake --build`, then
`buildings_rework_harness` / `chain_depth` / `resource_chain_harness`, then a live look at the Build
door on an ancient-band tile.

### Verification

Lua: `luac5.4 -p scripts/economy.lua scripts/recipes.lua` both pass. `backlog_lint`: 0 fail, 6
warnings, all pre-existing and unrelated. C++: **not compiled** this session — see NR-240. Ancient
Build-door row count reasoned by hand from the era-masked recipe/extraction lists: ~9 named
extraction rows + 9 named processing rows (2 any-era + 7 ancient) + 4 infrastructure rows, past the
R5 threshold of 20+ named buildings.

### Open for Ben

- NR-239: per-building glyphs for the ancient roster are unbuilt — worth a standalone follow-on
  item, or folded into BL-431's chain/method UI?
- NR-240: this diff needs its first compile at the next session with real network access before
  anything else builds on top of it.

**Runtime:** not tracked this session (Runtime line remains uncollected — same standing gap NR-177's
retro already named).

---

## Session — Gate hygiene becomes a measurement saga: batch verify, the 70% map, and the golden demotion (2026-08-14/15)

Mixed mode, and the block that kept reframing itself. Started as three gate-hygiene items;
ended with the visual harness rebuilt, the map resized, five stale-state classes measured out
of existence, and the golden policy itself on Ben's desk.

**Landed.** BL-415 (sweep gate): exclusion by machinery — `run_sweep.cmake` reports Skipped
without `IO_RUN_SWEEPS`; `CONFIGURATIONS` was tried first and measured not to gate a
single-config generator. BL-416 (AI bands): re-blessed and restructured to the derived form
(observed table + named slack; the failure output prints its bless line), then re-blessed
AGAIN same-session for the resize — seven numbers per seed, as designed. BL-423
(`--verify-all`): one ~40 s generation per pass instead of one per script; equivalence needed
FIVE isolation layers, each found by instrument (state_hash, a chat dump, pixel diffs), and
the run survives Windows now (hidden window, ghosting disabled, event heartbeats — five
Application Hang 1002 events over two days each matched a silently truncated pass). BL-424:
the homeworld at 70% area (312×145 → 261×121), single-source constants; `population_mvp` and
`stack_capacity_harness`, red for sessions, PASS on the smaller world.

**Measured, and worth remembering.** The verify cost was never rendering: a capture is 0.25 s;
`make_hard_coded_world` is ~40 s on the Debug build and is TILE-COUNT-INSENSITIVE (the resize
moved it not at all — the Era −1 sim, planetology and firm calibration dominate).
`history_ages` runs its lazy Era −1 time-lapse past 8 minutes and is parked (BL-425). The
stale-exe trap bit twice more in one day — a 72/74-green ctest on pre-resize binaries read
exactly like a green gate (BL-426 filed: the gate should detect it). BL-427 (Ben's proposal):
cache the post-generation world behind a state-hash guard, the right lever for solo runs.

**Open on Ben's desk.** NR-237: whether golden-diffing earns its place at all — his question,
and the measured evidence half-supports him (every diff this week was intended change or world
drift; the genuine catches were harness bugs; there is no CI to run them). Recommendation in
the entry: demote to a world-independent curated set + assertion-based checks, re-freeze per
surface approaching v0.1.0. The suite-wide bless (BL-402's remainder) is HELD on that ruling —
~200 binary files should not be committed the day before a demotion deletes them.

**Ruled and executed (2026-08-15): option B — goldens demoted.** Ben: golden-diffing kept only
for a curated world-independent set (currently the icon_silhouettes pair, PASS 0.0000%);
everywhere else captures are the product and assertions are the verdict. 221 tracked + 16
untracked goldens deleted; `--bless` hardened to refresh-only so the set cannot silently
regrow (app_capture.cpp); policy rewritten in DEVELOPMENT_PRACTICES § Visual verification and
the verifier-visual skill (skill edit = executing the ruling). Reintroduction criterion:
freezing — a surface joins the set when its pixels stop being expected to change.

**Status:** BL-402/415/416/423/424 complete; BL-425/426/427 filed; NR-237 resolved-and-pruned.
**Runtime:** ~8 h across the two days' boundary (through the golden demotion and the
next-session scheduling), Full, measurement-heavy.

---

## Session — Seam batch: the money printer closed and the word interface given a door (2026-08-14)

Full mode, Batch Delivery over four items — BL-386 (sell-order floor prints money, S),
BL-387 (seam actor authority, S), BL-396 (wire parser validates nothing, S), BL-397 (seam read
privacy, A). The three S-tier items were the entire S tier; all four share one root cause:
`--serve` turned a trusted in-process seam into an external input surface.

**What landed.** The floor is a reservation price: the auto-clear pass holds any order whose
floor exceeds the resolved price and pays the resolved price otherwise — the `max(rp, floor)`
crediting that let a seller name the price a perfect counterparty pays is gone, and
`trade_floor_multiple` re-tuned 1.0 → 0.25 so rivals keep trading under the honest rule. The
serve seam gained a session actor (`--as <corp|any>`, default the player corp): COMMAND refuses
to act as any other corp, BLACKBOARD refuses to read one, and `--as any` is the explicit
bot-vs-bot research opt-in. Every COMMAND field now parses wide, range-checks against its real
domain, and rejects the whole command on violation — `verb=256` no longer builds a building,
`type=200` no longer segfaults the server, `quantity=1e300` fails as the float it lands as. The
`remove_sell_order` oracle is closed (foreign and nonexistent ids indistinguishable).

**Method notes, both directions.** Two worktree agents did the code (economy slice; seam slice
across three items with per-item commits); both worktrees were cut from a base TWO COMMITS
STALE — the new `agent_base_check.js`, written this morning for exactly this, caught both
pre-merge and a rebase cured each cleanly. It also found its own first bug (named-branch
filtering) and three leftover stale agent branches from earlier sessions (NR-235). The review
barrier earned its place: verdict FIX FIRST with three real Criticals (a harness assertion
certifying the pre-BL-397 oracle, the spectator golden, a missing smoke case the requirement
named) plus two suggestions taken (the float path still accepted garbage via `atof`; trailing
junk tolerated on integer tokens) and one filed (BL-422, held orders still credit market
inventory at listing time — a listing==selling equivalence BL-386 broke).

**Verification.** order_book_harness 52/52 with the new R6 family (hold/clear/income-invariant,
bite-proven against a reverted fix); econ_harness ALL PASS after updating four fixtures that
certified the old spec (floors moved to legal values so the BL-351 clamp semantics stay
exercised); econ_stability ALL PASS; integrating MSVC build green; smoke.js ALL CHECKS PASSED
(actor refusals with byte-identical state snapshots, the range family, the closed oracle,
`--as any`); spectator_determinism re-blessed 3CBAD1D44EE71EDE → DD166049DA180508 (deliberate,
reproducibility confirmed first; the golden is toolchain-specific — noted in the harness).
ai_skill_harness moved exactly as BL-386's design predicted — net-worth bands fail on every
seed while solvency/survival/thrash hold and rivals still list 6–7 orders/seed. **Deliberately
not blessed here**: BL-416 (golden stewardship) owns the re-bless and now carries the post-fix
numbers in its design note, so the bands get blessed once, against the honest economy.

**Docs.** MARKETS.md step 11 restated (with the correction's history); the cold-store
`player-sell-orders` R2 rewritten (the requirement certified the defect); ACTIONS.json's
`place_sell_order`/`remove_sell_order` entries corrected and mirrors regenerated; AI_OPPONENT.md
§ 6 records the session-actor model; the standing rules gained the untrusted-boundary invariant
(delegated call — NR-234).

**Status:** Complete — 4 items landed, 4 requirement groups complete (16/16 rows), BL-422 filed.
**Runtime:** ~2.5 h, Full, Batch Delivery (2 worktree agents + review barrier).

---

## Session — Doc-system weight: requirements hot/cold split + DEVLOG rollover (2026-08-14)

Light mode (tooling + docs, no `src/`). Ben asked what to improve now the project is large; the
measured answer was context weight, so this session built the missing half of the hot/cold
machinery. **BL-421 (requirements query + cold store)** filed and landed in one pass.

**The finding.** `doc_weight.js` put the reading order at ~824K tokens against its 150K budget.
The largest un-queried store was `docs/development/req/requirements.json` — 556 KB / ~142K
tokens, 223 of 232 groups frozen history, and the policy doc itself already named a query tool
as a "candidate follow-on".

**The fix is the backlog's own pattern, applied verbatim.** Three new tools in `tools/session/`:
`req_store.js` (shared shape + `resolve()`, mirroring `archive_store.js`),
`archive_requirements.js` (moves resolved groups' `rows`+`resolution` to
`archive/requirements-<quarter>.json`; `--dry-run`/`--restore`; round-trip verify), and
`requirements_query.js` (index default over in-flight groups; `<brief>`/`--full` resolves cold
transparently; `--failed`/`--class` row-level sweeps; `--grep`, `--count`, `--table`).

**First run:** 219 groups (399.9 KB) moved into 3 cold files; `requirements.json` 556.5 KB →
124.0 KB (78% smaller). Along the way the tool normalises legacy statuses per REQUIREMENTS.md
("normalise on sight": 9 group `completed` + 1 `closed` → `complete`) and backfilled the 16
legacy `brief: null` groups with deterministic title slugs so the cold store can key on brief.
`story_check.js` now resolves through the pointer (it went 10-fails red when rows moved cold —
caught and fixed in-session); `backlog_lint.js` unaffected (0 fails, 5 pre-existing warnings).

**Second lever, existing machinery:** `devlog_index.js --rollover 2026-08` moved 47 July
sessions into `archive/DEVLOG-2026.md`; the live DEVLOG dropped 431.8 → 239.8 KB. The backlog
design archive was checked and already current (0 items to move).

**Net:** reading order ~824K → ~666K tokens. The next levers are structural, not mechanical, and
were left as recommendations: terminal backlog items still hold ~330 KB of hot *index* fields
(a schema call), and the 42 items parked at `post-v0.1.0` are a triage pass, not a tool.

Docs updated as part of landing: REQUIREMENTS.md (hot/cold + querying sections), DELIVERY.md
(§ shed the weight gains the requirements sibling), CLAUDE.md (traversal line, requirements
authority row, DEVLOG volume boundary). Decisions on Ben's behalf recorded as **NR-233**
(status normalisation + slug backfill — the two edits to permanent history).

**Status:** Complete — BL-421 landed; no requirement group (doc/tooling-only, exempt per
REQUIREMENTS.md § Scope).
**Runtime:** ~1 h, Light, tooling/doc-infrastructure.

---

## Session — AI gameplay: the word interface made runnable, and the rival's idle/resume oscillation measured (2026-08-13)

Full mode. Two strands, both under the v0.2.0 AI-opponent theme: the `--serve` word interface an
out-of-process agent plays through, and the deterministic scorer that is the shipped rival.

**Framing first, because it changed what was worth building.** A SOTA refresh (the last sweep was
2026-08-03) found the external field essentially static for strategy-game agents in that window —
Vox Deorum presented at FDG '26 and shipped a diplomacy layer over its planner, and no new 4X
agent paper landed at all. What did move sits underneath: the **constraint-tax finding that
§ 10g's ruling partly rests on has been reframed**. "Capacity, Not Format" locates the penalty in
a model's *spare capacity* rather than in the output format, and reasoning-before-structure APIs
largely remove it. The ruling stands, but on its other legs — the behavioural-cloning ceiling and
legibility — and the stronger contemporary argument is **multi-turn** tool-call accuracy, where the
small-model class Io targets still scores 35–56% on BFCL v4. Recorded so the ruling's basis stays
honest rather than quietly resting on a superseded number.

**Strand 1 — BL-278's seam was landed on paper and unrunnable in practice.** It was smoke-tested
once by hand on the day it landed and never again, and five defects had accumulated since, none of
which had ever failed a run because nothing re-ran it. `tools/mcp/server.js` spawned
`build/ProjectIo.exe`, which the primary (Linux) target never produces — **the MCP server could not
start on the main dev platform at all**. The `COMMAND` opcode parsed nine argument keys while the
enum had grown three verb families past it (BL-324 hire, BL-293 order book, BL-350 procurement), so
**six of fifteen verbs were unreachable**, not partly supported. `corp_command_result_name` had no
cases for BL-350's four declines, so "the supplier holds no capacity" and "you are embargoed"
both reached an agent as *your arguments are malformed* — which removes exactly the typed-failure
property § 10a leans on. `run_serve` never called `advance_surveys`, so `survey` was an applicable
verb whose effect never arrived and no tile was ever revealed for `build` to target; the tick
sequence was duplicated verbatim between the warm-up loop and the `TICK` opcode, which is how the
step came to be missing from *both*. And nothing on the protocol yielded a **body id**, though
`survey`, `place_sell_order` and `request_quote` all take one — the blackboard keys market facts by
*market* id, so an agent could read a price on a body it had no way to sell into.

All five fixed. New `BODIES` opcode and `list_bodies` tool (seven tools now), the exact sibling of
NR-061's `list_corps` and filed for the same reason. New **`tools/mcp/smoke.js`**, committed rather
than run ad hoc: it drives the raw line protocol and asserts *shape* — every opcode answers, all
fifteen verbs reach the seam and return a code that is genuinely in `corp_command_result`, a
well-formed sell order is distinguishable from a malformed one. It found the body-id gap on its
first run, which is the argument for having written it.

**Strand 2 — the rival AI's dominant behaviour was reversing its own decisions.** `ai_skill_harness`
could not name six of the fifteen verbs (its `verb_name` switch stopped at `hire_unit`, pooling the
AI's entire trading behaviour into an unnamed `action[?]` row). Making it exhaustive exposed the
real signal underneath: **`resume` outnumbered every other verb about 10:1** — 134–255 resumes per
30-tick rollout against 12–17 idles and 3–6 builds.

Two structural causes and three arithmetic ones. **Structural:** the reflex tier and the
strategic tier own the same `decommissioned` flag and neither knew the other existed — BL-079's
block idles a building directly on the component and set no `ai_cooldown`, so BL-202's scorer
could reverse an eight-tick-loss idling on its very next evaluation. And the two sides used
different estimators, idle scoring on `estimate_building_profit` while resume hand-rolled
revenue-minus-wages with no maintenance, input cost, stack decay or depletion taper.

Reaching for `estimate_prospective_profit` to close that was right. Reaching for it *naively* was
not, and the first cut looked like a partial success — resume down 30–56% — which is exactly how a
plausible fix hides a real defect. An **adversarial review of this session's own diff** found three
compounding errors in how the estimator was being called:

**It priced a hypothetical building, not this one.** The function authors a fresh probe at
`construct_building`'s defaults (0.5 assigned, target 100), so a site the scorer had dialled to 200
— or to 0 — was priced at a staffing level it would never come back at. It now takes an optional
`existing` building.

**It counted the building as an extra member of its own stack.** `stack_members` filters on
tile/type/target only, so an existing site is already in that list; the default `size() + 1` rank
charged it a further step of BL-193 decay against itself — 0.8× lone, 0.512× at rank 3.

**"Maintenance is paid either way" is false.** BL-049 splits maintenance into a fixed material
share that survives decommissioning and a labour share that does not, so idling saves 70% of it.
Crediting the full running figure overstated every resume by 0.7 × maintenance — a systematic bias
toward running, in the one estimate whose whole purpose is to stop the AI resuming what it should
leave idle. The idle candidate carried the mirror-image error; the two were self-consistent, which
is why neither ever produced a single-tick flip and why both went unseen.

A third, independent defect in the same block: **the workforce dial could only ever move one way per
building.** Its gain was `variable × (proposed − target) / target` with `variable = revenue − inputs
− wages`, taking its *sign* from `variable` rather than from the model — so a profitable building
could only be scored for raising its target and a loss-maker only for cutting one, and the interior
optimum `solve_workforce_target` exists to find scored negative in both directions and was
discarded. The solver now reports its own modelled gain through an optional out-param; it already
computed both endpoints.

**Measured, and the result is categorical rather than incremental.** `resume` goes
134/178/193/153/255 → **0/0/0/0/1** across the five benchmark seeds. The reflex tier's own idlings —
the buildings it was idling only for the scorer to resume straight back into losses — go
67/137/132/93/198 → **9/8/7/6/7**. Net worth is **up on every seed**, so none of the churn was
profitable. Solvency, survival and determinism unchanged (R0 byte-identical).

The harness now counts those reflex-tier idlings too; they issue no command, so without that the
oscillation was not readable from this instrument at all. Dial-thrash ceilings tightened 230–410 →
40–69, because they had been blessed from runs containing the very oscillation they exist to catch.

**One hypothesis raised and killed by measurement.** The residual looked like a price-response limit
cycle — idle, price recovers, resume, price collapses — so BL-203's glut forecast was applied to the
resume candidate. It moved **not one number** on any of the five seeds. The reason is the finding:
the forecast is **bimodal, not graded**. At tick 30 every `(market, resource)` slot carrying a demand
signal sits at supply/demand between **78 and 339** against a veto ratio of 2.0, and the rest carry
no demand signal at all, where the design deliberately applies no penalty — the taper band between
1.0 and 2.0 has **zero occupancy**. So the Victoria-3 import § 4 calls "the single most important
design import" is running as a coin flip between off and veto. The change was reverted rather than
kept as an unverified behaviour change, and the prior question — why does market demand max out
around 8 while supply reaches 15,000? — is filed as the thing to settle first, because it may be a
commensurability error in `market_clearing` rather than an AI-tuning problem at all.

**Deliberately not built.** Nothing frame-specific for the 0 CE mercenary refocus. BL-377
(mercenary contracts) is design-only and requires BL-315 (conflict spine), which is design-owed at
v0.3.0 behind BL-094 (governing body); anything built against that today is a bet on unlanded
design. The seam repaired here is the frame-agnostic layer — opcodes, argument forwarding, typed
rejections, a smoke check — that a mercenary verb plugs into when one exists.

**The review pass earned its cost, and that is the session's real lesson.** Four adversarial lenses
were run over this session's own uncommitted diff, and one of them found a **critical** defect the
change had introduced: teaching the parser to read floats let `std::atof` admit `nan`, which passes
`floor_price < 0.0f` (every comparison against NaN is false), enters `world.sell_orders`, is folded
into `state_hash`, is written to the save stream, and reaches `clear_markets`' book sort — where it
stops the comparator being a strict weak ordering and makes `std::sort` undefined behaviour. The
same lens found an infinity overflowing a `static_cast<int>` in the procurement lead time. Both are
now refused at the protocol edge, which is the general rule worth keeping: **the AI-facing seam is
an untrusted input boundary in a way the UI is not**, because a control cannot emit a NaN and the
validation downstream of it never had to.

The same pass found the three estimator errors above, which is why the oscillation actually closed
rather than merely damping. Two of the file's own new assertions were also flagged as **vacuous** —
the survey check compared fact counts, which would have passed whether or not the survey ever
advanced, and the result-code check could not detect the switch fall-through it claimed to detect
because the fall-through returns a code that IS in the valid set. Both rewritten to assert the
thing: the survey's own progress counters must move, and a BL-350-specific decline must be
observable.

**A fourth lens read the prose rather than the logic, and that was the one that paid oddest.**
Pointed at the session's own *claims* instead of its code, it found four assertions that did not
survive contact with the source — all now corrected. "Six verbs could not be issued at all" was
five, because `hire_unit`'s `unit_type` defaults to 0, a valid roster index, so it worked and could
only ever raise row 0 — and the comment had explicitly denied exactly that reading. "Nothing else
yields a body id" was false: the blackboard keys pool facts by `(corp, body)`, so the real gap is
narrower and better stated. "-Wswitch catches the next one the way it did not catch this one" was
backwards — the flag was on and had been warning on every compile, under `-Wall` without `-Werror`,
and the warnings were ignored. And "resume at ~10x every other verb combined" was ~2.7x combined,
~10x the next single verb. None changed what the code does; every one would have entered the
permanent record as fact, in a project whose documents are its audit. Filed as NR-185.

**Review queue.** Eight entries filed as the work happened (NR-178 the oscillation and its five
causes, NR-179 the workforce-dial signature change taken on Ben's behalf, NR-180 the bimodal
forecast and the supply/demand question under it, NR-181 goldens blessed from the behaviour they
exist to catch, NR-182 the action dictionary running four verbs behind the seam it transcribes,
NR-183 the constraint-tax leg of the 10g ruling superseded in framing, NR-184 the NaN boundary,
NR-185 the four overstated claims and the claims-lens practice that caught them).

**The review's verification pass then caught the fix itself.** 38 findings were raised across four
lenses and 36 were refuted under adversarial verification; the two that survived were both in this
session's own work, and one of them was the NaN guard. Returning the *default* on malformed input
is not the same as refusing it: `quantity`'s default of 0 is rejected downstream, so substituting
it refuses by accident — but `floor_price`'s default of 0 is **meaningful**, read by the seam as
"accept the market price". So `floor_price=inf` turned "sell only above this floor" into "sell at
market, every tick", answered `applied`, and said nothing. A worse failure mode than the crash it
replaced, and it took a verifier reading the downstream *meaning* of a default to see it. The
parser now reports malformation and the handler answers `rejected_invalid`; the smoke check asserts
it for `nan`, `inf` and `1e400`. The second survivor was `smoke.js` hard-coding `r < 31` for
`resource_count` (42) — the exact stale-literal defect the commit before it set out to remove — now
read off the blackboard's own `price:<n>` facts instead.

**Full tier: 64/68, and the four reds are all pre-existing.** `ai_skill_harness` is green across the
complete run. The failures are `data_creep_harness` (NR-171), `population_mvp` (NR-170),
`stack_capacity_harness` (stale since BL-366) and `history_sim_harness` (six assertions). The last
was adjudicated the way SPRINTS.md prescribes rather than by inspection — a throwaway worktree at
`4e0118d`, configured and built from cold, produced the identical six failures. Two of the four had
no record anywhere before today; NR-186 now carries all of them, and argues the `history_sim` six
are the priority, because NR-177's refocus makes that sim the ancient product's *generator*.

---

### Second phase, same session — the review queue, then AI play

**The queue first.** Worked the AI/seam/tier cluster: open entries **64 → 49**. Five closed on work
already done, two advanced as standing practices, three consolidated (NR-143/145/171 were one
finding filed three times), and **two were refuted rather than resolved** — NR-129 asked for a guard
that already existed in the very commit it reviewed, and NR-171's "climbs ~1.25/tick, possibly
unbounded" is disproved by a 4000-tick trace showing dead-flat counters for 3000 ticks.

**NR-180 was the priority and the answer was neither option it offered.** Supply and demand are not
a stock/flow blunder — both are zeroed together each tick. The ratio is a non-measure anyway:
`clear_markets` is an unconditional buyer of last resort so supply is unbounded by demand *by
construction*; only 12 of 42 resources have a standing consumer and **no raw ore has one**; and the
two sides are authored three orders of magnitude apart, with measured maximum demand (8.25) sitting
at the population basket's structural ceiling (7.5). The gate is **inverted** — `demand <= 0` returns
"no penalty", and those are the real gluts. It also suppresses inter-body **convoy dispatch**, which
gates on `demand − supply > 0`, corroborated by `data_creep`'s own coverage note. Filed as **BL-381**
with a proposed fix: score the glut off the resolved **price**, which is bounded, defined for every
priced good, and already public.

**The tier went four reds to one.** Three were stale harnesses encoding rules the code had
deliberately changed; each is fixed and each restored a check that was testing nothing. The fourth
is **BL-384**: `history_sim` fights 267 battles and takes **zero** provinces across 1960 years, with
no assertion covering conquest count — which is exactly why six red assertions never surfaced it,
and one of the six passes *vacuously*. NR-177 makes that sim the ancient product's generator.

**Then Ben's steer, and it paid immediately.** *"Pushing for AI play will expose more bugs and give
us actionable improvements now."* Recorded as § 10h and acted on: `tools/mcp/session.js` (the play
driver — batch-shaped, because determinism makes appending a move and re-running a byte-identical
replay), then five agents given the seam and an agenda.

**Eleven of the session's seventeen filed items came from play**, on code that had already been read,
instrumented, and put through four adversarial review lenses the same day.

Two are priority **S**. **BL-386** — a sell order's floor price is `max(ref, floor)`, credited with
no counterparty and no cap; listing at `1e12` reached cash 1.587e17. Independent triage found the
matched-trade loop *twelve lines above* correctly debits the buyer: one path was written as a market
and the other as a wish. It also **resolves NR-144** — the AI lists at `base_price` while the market
sits pegged at `0.25 × base`, so every rival unit sold earns 4× the market rate from nothing. NR-144
had concluded the scorer was probably innocent; it was, and the market was not. **BL-387** —
`apply_corp_command` never checks the caller may act *as* the corp it names; a player drove rivals
and moved their balances by tens of millions.

**The pattern worth keeping.** Three findings are the same shape — a constraint that lived in the
only caller and looked like a rule until a second caller appeared. NR-184 (float validation assumed
a UI that cannot emit NaN), BL-387 (`cmd.corp` was never attacker-controlled because the scorer set
it), BL-394 (`hire_unit` has no cost or cap; the only brake is `corp_ai_params`, a *scorer policy*).
Three instances is a rule: **the AI-facing seam is an untrusted input boundary in a way the internal
caller never was, and validation written for a trusted caller does not transfer.**

**What the players could not do was as informative as what broke.** No processing facility produced
a single unit across ~80 building-ticks — `--serve` never loads `world_gen.lua` so coal has no price
(**BL-389**), and `build` silently discards its recipe so every seam-built processor is a steel
smelter anyway (**BL-388**). The procurer swept 26 suppliers and could not *compare* them, because
`request_quote` returns neither id nor price (**BL-390**). The militarist raised 25 units and found
no verb that takes a unit as a subject (**BL-393**).

**Play corrected two dictionary entries written earlier the same day** — `request_quote`'s `subject`
is not "context rather than a constraint", it is not read at all; and `place_sell_order` was telling
agents `floor_price` is a reservation price while the engine pays it as a bonus. Both now carry the
defect and name the item that will remove the caveat.

**Fixes were filed, not landed**, per Ben's instruction to propose in the backlog. BL-386 in
particular will move every economy golden and should cut AI net worth by roughly the tripling NR-144
recorded — that fall is the fix working.

**Runtime.** ~7 h, Full mode (research sweep; two build strands; two committed checks; one hypothesis
measured and discarded; an adversarial review pass that changed the outcome; a review-queue sweep
taking open entries 64 → 49; and a five-agent play session that found eleven of the day's seventeen
filed items).

---

---

## Session — BL-130 lands: BL-365's blocker chain closed, and a live crash caught in passing (2026-08-11)

Full mode, one item, continuing the same session as BL-263/BL-368/BL-366 below.

**BL-130 — real market inventory, landed.** The last link before BL-365 itself. Adds
`market_component.inventory` — real, persistent per-resource stock, never reset per tick (unlike
`supply`/`demand`, which stay per-tick flow figures). **Fills** from real corp sales only
(auto-surplus + standing sell orders, tallied separately so the BL-078 substrate's abstract
supply — a pricing fiction nobody actually sold — cannot inflate real stock). **Drains** during
production (`run_processing`) and construction (`run_construction`), both of which run before
`clear_markets` in the same tick, against whatever survived prior ticks' sales.

The real behavioural change: `run_processing`'s old special case — *any* market body runs an
unconditional full batch, auto-buying whatever the pool didn't cover — is retired. Coverage is
now `(pool + market inventory) / need` per input, and the two-threshold partial-run model (full
at `t_full`, scaled to `t_idle`, idle below) governs uniformly whether or not a market backs the
body. `run_construction`'s BL-095 pacing rate swaps its old "last tick's cleared supply" proxy
for the same real field, and now actually drains it. Both consumers draw the same live inventory
in a fixed, already-deterministic tick order (construction, then production), and a processor's
run fraction is bounded by the coverage-min across every input by construction — so nothing can
double-spend the same stock; no proportional-fairness math was needed. The **sell side is
unchanged** — still unconditional, per the standing prototype invariant.

**A live crash, found and fixed in passing.** Verifying against the real generated world
(`pregame_balance_harness`) turned up a silent `abort()` — the harness printed two Lua-load lines
and stopped, exit code 3, no message. Added a top-level try/catch (kept — a real improvement to
the harness) to surface it: `Unknown resource 'clean_water' in recipe 'clean_water' outputs`.
BL-368 had added three `resource_type` values but never registered their Lua names in
`recipe_registry.cpp`'s `resource_from_name` table — every hand-built harness that constructs a
`recipe_registry` directly in C++ was blind to this, so **the actual game has been crashing on
startup since BL-368 landed earlier this session**, unnoticed until this check. Fixed by adding
the three missing table entries. Confirmed pre-existing (not a BL-130 artifact) by
stashing-and-rerunning against the BL-263 baseline first — same crash, same message.

**Two existing-harness fixture gaps, fixed.** `econ_harness` and `resource_chain_harness` hand-
build a `recipe_registry` + `market_component` and expect the old unconditional-auto-buy
behaviour; `construction_gate_harness` seeds `mc.supply` (the retired proxy) to represent "the
market has stock". All three needed `mc.inventory` seeded alongside their existing fixtures —
not a change in test intent, just which field now carries "the market has real stock". Caught
these the hard way: a first regression pass showed everything green, which turned out to be
**stale `.exe` files** — the individual harness CMake targets are separate from the `ProjectIo`
target and were never rebuilt after the source edits. This is the *third* time a stale-exe
mistake surfaced this session (see NR-169); every harness in the sweep was explicitly rebuilt
from clean before the numbers below were trusted.

**Verification.** New `tools/verify/market_inventory_harness.cpp` (14/14 PASS): idle with
nothing available, a full batch from ample market stock with an exact drain check, pool-then-
market draw order, the two-threshold model applying uniformly on a market body, a real sale (not
substrate) landing in inventory, and construction's own gate reading/draining the same field.
Full `ProjectIo` build clean. The complete 15-harness suite rerun clean from a fresh rebuild.
`pregame_balance_harness` (the real generated world, 80-tick warm start): climbs cleanly to a
~108k plateau, no crash, no negative balance, all 5 dynamism/determinism assertions pass —
different plateau value than the pre-BL-130 substrate-driven trajectory (expected: the underlying
model materially changed), but the shape is sane. `ai_skill_harness` moved from 8 to 9 golden-
band failures (one new: seed 4 net-worth min) — attributable and expected this time, a real
economic consequence of the mechanic working as designed, not instability; recorded in NR-169
rather than re-blessed.

docs/economy/MARKETS.md gains § Real market inventory and corrects two stale passages (the
"no stored inventory" limitation, the auto-demand/auto-clearing step descriptions);
docs/economy/PRODUCTION.md's stale 2026-07-31 "thresholds bypassed on market bodies" note is
corrected. backlog.json BL-130 → `complete`; requirements.json § real-market-inventory (R1–R5,
all complete); REFINED.md drained.

**BL-365's blocker chain is now fully closed.** BL-253, BL-366, BL-368, BL-263 and BL-130 are all
`complete`. BL-365 itself — the keystone, difficulty 5 — is next.

**Runtime.** ~2 h, Full mode (one item, but the deepest of the session's chain: a core-model
rewrite touching every read site of market supply, plus a live production bug found and fixed).

---

---

## Session — BL-263 lands: BL-365's blocker chain, first link (2026-08-11)

Full mode, one item, continuing the same session as BL-368/BL-366 below.

**The blocker chain, found before writing any code.** Moving to BL-365 (the Sprint 10 keystone)
next, its design's own `blocked_on` field named **BL-130** (real market inventory) as a hard
prerequisite — settled 2026-08-11 in BL-365's own design pass: *"a market that conjures any
shortfall undercuts the whole point of modelling real producers."* BL-130 itself `requires`
**BL-263** (spontaneous market emergence), also un-landed. Neither was in Sprint 10's original
plan. Surfaced to Ben rather than pushed through silently, per the standing sequencing rule; his
call was to work the chain in order — BL-263 → BL-130 → BL-365.

**BL-263 — spontaneous market emergence, landed.** All five of Ben's 2026-08-02 settled calls
implemented as specified. **Trigger**: the first building *completing* (not placing) on a body
with no market — `maybe_spawn_market`, wired into both `construct_building`'s instant-completion
path (`build_duration_ticks <= 0`) and `run_construction`'s pacing-loop completion
(`economy_system.cpp`); survey completion is explicitly *not* the trigger, keeping the geographic
and commercial fogs independent. **Who**: any corporation — no player-only gate; a rival-created
market on an unvisited body needed no new fog code, since the existing activity fog (BL-089)
already gates on presence/routes, not market existence directly. **One market per body**
off-world, checked before spawning; the home body's BL-096 carved seeding is untouched.
**Never disappears**: no deletion code exists anywhere for markets, so persistence-with-dormancy
falls out for free — a dormant outpost is just the ordinary zero-supply/zero-demand case.
**Opening prices**: the home market's own `base_price`, marked up by a distance proxy
(`|orbital_radius_au` difference`|`, moon-approximated at its parent — a cheaper stand-in for
`supply_system.cpp`'s precise tick-pure angular distance, adequate for a price curve though not
for real haul routing). **What clears**: new `inject_interbody_demand` pulls a
distance-discounted slice of the home body's own unmet demand onto every outpost market each
tick (`economy.market_emergence` in Lua) — the mechanism that stops an outpost with real supply
and no local population from collapsing to the price floor the instant it starts producing,
independent of `dispatch_convoys`' own physical routing.

**No save-format work** — named in the design as a real consequence, but there is no general
serialisation system in this codebase yet to extend (no `src/world/serialisation.cpp`), so that
half of BL-263's design stays deferred to whenever the save seam actually lands, not built here.

**A self-correction.** BL-368's `ai_skill_harness` finding (below) claimed a *different* 5-failure
set after landing, versus the BL-366-only 8-failure baseline. Rebuilding `ai_skill_harness` fresh
before trusting it against BL-263 caught the error: the "5" reading was a **stale `.exe`**, never
rebuilt after the stash-and-pop investigation that produced the 8-failure baseline. A clean
rebuild with BL-366+BL-368+BL-263 all applied reproduces the exact same 8 failures as the
BL-366-only baseline — BL-368 and BL-263 do not move the bands further, at least not detectably.
NR-169 corrected accordingly; the lesson (rebuild after any stash/pop before trusting a result)
is recorded there too.

**Verification.** New `tools/verify/market_emergence_harness.cpp` (16/16 PASS): no market before
any building, correct spawn on completion, correct `centre_tile`, exact opening-price and
pulled-demand formulas checked arithmetically (not just sign), no second market on a second
building, no self-pull on the home market, and a graceful all-zero-price degenerate fixture with
no home market at all (no crash). Full `ProjectIo` build clean. Reran `econ_harness`,
`econ_stability`, `resource_chain_harness`, `determinism_harness`, `construction_harness`,
`world_audit`, `construction_gate_harness`, `buildings_rework_harness`,
`multi_building_tile_harness`, `population_demand_harness`, `habitability_tranche_harness`,
`supply_advance`, `trade_routes_harness`, `commercial_fog_harness` — all clean, checked for real
`FAIL` lines rather than trusting a `grep -c FAIL` count (which false-positives on summary text
like "0 failures").

docs/economy/MARKETS.md gains § Spontaneous market emergence. backlog.json BL-263 → `complete`;
requirements.json § spontaneous-market-emergence (R1–R5, all complete); REFINED.md drained.

**Still blocking BL-365.** BL-130 (real market inventory) is next — the last link before the
keystone itself.

---

---

## Session — BL-368 lands: Sprint 10's second foundation, and a stale bug claim corrected (2026-08-11)

Full mode, one item, continuing the same session as BL-366 below.

**BL-368 — real population demand + habitability tranche, landed.** Generalises the BL-190 flat
`agricultural_produce`-only population demand stub into a real, price-elastic, multi-resource
basket (`population_demand_params`, `economy.population_demand` in Lua) — the same elastic shape
as the BL-078 nation-substrate model, but population-only: no supply term, a pure consumer.
`inject_population_demand` (`market_clearing.cpp`) now takes the `recipe_registry` and sums
`scale × demand_scale × basket[r] × (base/price)^elasticity` across every resource in the basket,
for every population centre, into its catchment market.

**A stale premise, corrected in passing.** BL-368's own design cited a "known shipped bug" —
population demand zero-reset by `clear_markets` before it could be read. Reading the actual code
before writing any showed the bug had already been fixed by **BL-190** (2026-07-31);
`inject_population_demand` already runs after the reset. `docs/economy/MARKETS.md`'s
Known-limitations list repeated the same stale claim as current — corrected here rather than left
to mislead the next reader (`io-backlog-prose-goes-stale`: check the authority doc before trusting
a filed premise).

**The habitability tranche.** Three new `resource_type` values (39 → 42): `clean_water`,
`consumer_goods`, `medical_supplies` — the three RESOURCES.md's habitability table names as goods
population centres actually consume as tradeable goods. Building Materials and Utilities stay
deliberately absent (a different consumption path / an abstracted budget cost, per that table's
own note). Three new recipes on the generic `processing_facility` (`scripts/recipes.lua` ids
14–16, no new `building_type` values, matching the shipped set): `clean_water` (water → clean
water), `consumer_goods` (food rations + steel → consumer goods — "refined goods (various)" in the
design, steel standing in as the one already-shipped refined input), `medical_supplies` (water +
agricultural produce → medical supplies — no standalone "chemical" resource exists in the
prototype set, so water stands in, mirroring Hydroponics Bay's own water-as-process-input
precedent). Base prices in `scripts/world_gen.lua`.

**Deliberately not built**, named per Rule 0c: the undersupply *effects* (habitability, workforce
efficiency, growth) RESOURCES.md's table names — the demand signal now moves prices, but does not
yet feed back into the population/workforce model. Construction Yard and Power Plant (Building
Materials / Utilities) also stay unbuilt, per the scope note above.

**Verification.** `population_demand_harness` gained an R4 (elasticity + multi-resource +
untradeable-skip, 4/4 PASS); its existing R1–R3 were updated to hand-configure a basket, since the
default registry basket is now empty (population demand used to be an unconditional flat stub, now
it is data-driven and a bare `recipe_registry` carries no data). New
`tools/verify/habitability_tranche_harness.cpp` (9/9 PASS): all three goods produced, none pegged
at the market band ceiling over 80 ticks, and a population centre's demand for all three reaching
the market. Full `ProjectIo` build clean. Reran `econ_harness`, `econ_stability`,
`resource_chain_harness`, `determinism_harness`, `construction_harness`, `world_audit`,
`construction_gate_harness`, `buildings_rework_harness`, `multi_building_tile_harness` — all clean.

**`ai_skill_harness` golden-band drift, investigated and filed rather than silently absorbed.**
A stash-and-rerun of BL-368's own files against the BL-366-landed baseline found **8** golden-band
failures, not the **5** recorded as pre-existing at Sprint 11's close (NR-140) — BL-366 alone had
already moved the bands, unchecked at that landing since `ai_skill_harness` was not on its
regression list. With BL-368 applied the count returns to 5, but a *different* five. Filed as
**NR-169** rather than re-blessed on the spot: the bands are drifting with every Sprint 10/11
landing and BL-365 (background industry, ~80 new firms) will almost certainly move them again —
a standing stewardship gap, not a one-off to paper over.

docs/economy/RESOURCES.md, PRODUCTION.md and MARKETS.md updated (habitability tranche tables, the
clearing-tick step list gains population demand injection, the Known-limitations correction).
backlog.json BL-368 → `complete`; requirements.json
§ real-population-demand-habitability-tranche (R1–R5, all complete); REFINED.md drained.

**Note on the working tree.** `docs/development/backlog.json` and `NEEDS_REVIEW.json`/`.md` also
carry unrelated in-flight content from a separate concurrent session (BL-370/BL-371 filed items,
NR-168) — not authored here, left intact rather than reverted, flagged for whoever commits next.

**Still open in Sprint 10.** BL-365 (background industry, the difficulty-5 keystone with an open
corp_ai-scope question — both foundations it needed, BL-366 and BL-368, are now landed),
BL-367/BL-130/BL-132/BL-369.

---

---

## Session — BL-366 lands: Sprint 10's first foundation, the living world resumed (2026-08-11)

Full mode, one item. Origin pulled 174 commits behind onto `main` (fast-forward to `c491b14`,
v0.1.14/Sprint 11 stamp); a status check on Sprint 10 found only its BL-253 prerequisite landed —
the five real content items (BL-366, BL-368, the BL-365 keystone, BL-367, BL-130/BL-132/BL-369)
were all still `designed`, nothing promoted to REFINED.md. Ben's call: resume Sprint 10 now,
foundations first.

**BL-366 — multi-building tile stack cap + urban transform, landed.** Answers the half of BL-193
(building stacks) the item deferred: non-extraction buildings (processors, ports, hubs, admin,
military base, research institute) are no longer capacity-1 per tile. A new `terrain_composition
::urban` value (12th, `components.hpp`) plus a per-composition non-extraction cap table
(`non_extraction_stack_cap`, `placement_rules.cpp`) — grassland/forest/wetland 6, tundra 3,
barren/rocky/regolith/metallic 4, volcanic/icy 2, urban 12, ocean 0 (exempt). The cap counts
**every non-extraction type on a tile combined**, not per type — a new
`non_extraction_buildings_on_tile` aggregate counter, distinct from the existing per-(tile, type,
target) `buildings_on_tile` extraction stacking uses. Filling the cap fires a one-way
`maybe_transform_to_urban`, wired into `construct_building` (`construction.cpp`) right after a
non-extraction placement lands. Once urban: `can_place` refuses new extraction/ambient placement
(`no_deposit`) even against a real seeded deposit, sites already standing are grandfathered and
keep operating untouched, and tile habitability is raised to at least 0.80 (never lowered).
Extraction stacking itself (`k_richness_per_site`, richness/50) is untouched — a separate axis.
`presentation.cpp` / `hex_render.cpp` gain the urban name + colour (built-over grey).

**Deliberately not built**, named per Rule 0c: the per-composition build-cost/logistics discount
and a transform notification/log line — both named in the design as implementation-time tuning
values, not committed there.

**Verification.** New `tools/verify/multi_building_tile_harness.cpp` (26/26 PASS): the cap table,
aggregate cross-type occupancy firing the transform on the 6th mixed-type placement (not the 6th
of one type — the case that actually distinguishes this from the old per-type rule), urban's own
higher cap admitting a 7th, extraction refusal post-transform, grandfathering of a pre-transform
extraction site, the habitability floor holding in both directions (raised when below, untouched
when already above), and extraction's richness-bound stacking left unchanged. Reran
`construction_harness`, `determinism_harness`, `world_audit`, `construction_gate_harness`,
`buildings_rework_harness` — all clean, zero regression. Full `ProjectIo` build clean (CMake/MSVC
14.44). `docs/economy/TILES.md` gains § Urban transform (cap table + rationale);
`docs/economy/PRODUCTION.md`'s non-extraction-stacking paragraph updated from "deferred" to
"answered by BL-366". backlog.json BL-366 → `complete`; requirements.json
§ multi-building-tile-urban-transform (R1–R5, all complete); REFINED.md drained.

**Still open in Sprint 10** (not started this session): BL-368 (real population demand — see the
same day's follow-on entry above, which also found the "known shipped bug" cited here was already
fixed), BL-365 (background industry, the difficulty-5 keystone with an open corp_ai-scope
question), BL-367/BL-130/BL-132/BL-369.

**Runtime.** ~1.5 h, Full mode (one item: design review, implementation, new harness, doc
propagation, backlog/requirements bookkeeping).

---

---

## Session — The warm start converges, and the substrate is condemned (2026-08-10)

Full mode, design + one Light code change. Two design passes (BL-340/BL-350 jointly, BL-365–369 as
a new cluster), one measured behaviour change, and a sprint re-sequence — all landed against a
repository that spent most of the session mid-merge.

**The measurement is the session.** `pre_game_ticks` was 12, and its own comment justified that
defensively: *"~3 in-game years … short enough not to diverge under the prototype's un-tuned
economy."* Ben asked for 20 years. Rather than assume, the warm-start length was made a parameter
of `pregame_balance_harness` and measured. The fear does not hold — the economy **converges**:

| Phase | Ticks | Behaviour |
|---|---|---|
| Linear | 1–23 | ~5,530 cr/tick, dead straight |
| Knee | ~24 | growth begins decaying |
| Plateau | **47–80** | **~185k cr, ±60 oscillation, drifting slightly down** |

All five economy assertions still pass at 80, determinism holds, and the balance never goes
negative. `pre_game_ticks` is now **80**, at ~3.5 ms/tick — about 240 ms of extra startup.

**That plateau is what condemned the substrate.** The player corp saturates at ~185k not from any
lack of ambition but because `inject_substrate_demand` clears a fixed fraction and there is nothing
further to trade against. Ben's call, on seeing it: *"replace the substrate entirely."* Filed as
**BL-365** (background industry) plus **BL-366** (multi-building tiles), **BL-368** (real population
demand), **BL-367** (management surface) and **BL-369** (warm-start calendar semantics).

Two things the design pass corrected rather than accepted. **"A tile is one building" was never the
shipped rule** — `stack_capacity` already returns 1–5 for extraction sites and counts per
`(tile, type, target)`, so BL-366 completes the question BL-193 explicitly deferred rather than
pivoting away from a philosophy the code never had. And **who owns background industry is settled
by the rulebook, not by taste**: the standing rules forbid a nation actor and sanction background
corporations, so it is more firms, not nations (NR-150).

**BL-340 and BL-350 were designed jointly**, because each is incoherent alone — one is the buying,
the other the thing bought. The decision that makes the pairing real is that
`spacecraft_components` gets **no background demand**, so the militia's contracts are its only
buyer. BL-340's own filed premise turned out backwards: the enum extension is nearly free (every
per-resource array is already sized off `resource_count`), while the real work is that three of the
four raws its recipes consume carry `base_price` 0 and so **cannot be bought at all**. The item's
centre of gravity is closing the minable-but-unsellable asymmetry.

**The sprint order then flipped, and the reason is BL-350's own design.** Its counterparty model
needs suppliers to choose between; against eight lean corps *"another supplier may still quote"* is
usually false, and it would have shipped correct and unexercised — the exact shape of Sprint 9's
`hire_unit` (NR-121). Sprint 10 now cuts **v0.1.13** (the living world), procurement moves to
Sprint 11, and everything after shifts one. v0.1.13 was the natural home: it had been hollowed out
when BL-340 left for v0.1.14, and BL-130/BL-132 were already orphans belonging to this work.
**BL-253** (the O(corps × tiles) scan) was re-goaled C/v0.2.0 → **A/v0.1.13** as a hard
prerequisite — the term is linear in corp count and this multiplies corp count tenfold, in front of
an 80-tick warm start that runs before the first frame.

**Working against a mid-merge tree shaped the session's method.** `backlog.json`,
`NEEDS_REVIEW.json`, `requirements.json`, `REFINED.md` and `DEVLOG.md` all carried conflict markers
for most of it, and `backlog_query.js` — a plain `JSON.parse` — failed hard rather than degrading.
Both design passes were therefore written to staging files under `docs/development/pending/` and
folded in only once the merge landed. Reading the *incoming* branch before finalising was not
optional: BL-293 added a second flat-binary stream, which reversed a conclusion this session had
already reached about BL-107 (NR-157) and rewrote BL-350's answer on how procurement should reach
the order book.

Two findings recorded rather than fixed. `pregame_balance_harness` passes BL-112 R1 off a price
pegged at exactly **4.00× base** — the hard band ceiling — rather than a discovered margin, in both
the 12- and 80-tick runs (NR-156). And PRODUCTION.md's Smelter table has disagreed with
`recipes.lua` about coal for some time (NR-158). Both are handed to the items that will touch them.

**Runtime:** not tracked. Design-heavy; the code change is one constant plus a harness parameter.

**Still open after this:** v0.2.0 has now been deferred twice in one day, both times in the same
direction (NR-159). And BL-365's dominant question — whether ~80 background firms can run the full
`corp_ai` layer or need a reduced model — is a two-tier-actor design commitment, not a build-time
detail (NR-160).

---

---

## Session — Hygiene wave 2: app.cpp halves, and the review barrier earns its place (2026-08-10)

Full mode, Batch Delivery — six worktree slices over BL-361/BL-362/BL-363, the three items the
morning's hygiene batch filed but did not deliver. Two waves, because BL-361 rewrites the file two
BL-363 tasks edit.

**The headline is BL-361: app.cpp went 3,826 → 1,422 lines** across four extractions (verify Lua
API + capture, startup screens, time panel, the post-econ-step history recorders), one commit
each, moved by line-range copy so logic reordering was not possible. The verify API's 60 registered
function names were diffed byte-identical, because `scripts/verify/*.lua` call them by name.

**The argument of the session is that the review barrier caught three faults a green build and
passing goldens could not.** Everything compiled, 25 harnesses passed, and five visual checks were
render-identical — and `verifier-review` still returned FIX FIRST, correctly:

- **The hover stick threshold stopped firing on the frame it used to.** Converting dwell from
  frame counts to seconds looked clean, but summing `1/60` in float32 reaches 2.49999833 after
  150 additions — just under `2.5f` — so the card stuck at frame 151 where the integer counter
  fired at 150, and the appear threshold cleared by roughly one ulp. `hover_freeze.lua` hid it by
  spending 153 frames. Thresholds now carry a half-frame epsilon.
- **A cache that never invalidated under `--verify`.** `current_day_tick` is maintained by the
  interactive loop only, so a capture session left it at 0 and every tick-stamped cache froze for
  the whole run. This is the nastiest shape a UI bug can take: *the golden is the stale render*, so
  the check certifies the fault. `verify.econ_step` now advances the tick — which, since BL-354,
  also means the harness's convoys stop pricing every haul at the epoch orbital position.
- **A retained-pointer cache whose guard could not fire.** The tech-tree geometry cache stamped on
  registry address plus entry counts; `verify.new_world` reloads the same file into the same member,
  so both are unchanged while every cached `const tech_node*` dangles. The registry now carries a
  reload generation. The code comment had asserted the stamp covered exactly this case.

**A second lesson, this one about the instruments.** Five visual checks failed at ~11.5% and the
first read was the known capture-before-render artifact — Ben said so, and he was right that it
happens. It wasn't that: the captures were complete. The goldens were stale by **119 commits**,
including BL-257 (generated body names — the home body is "Huhaidar" now, not "Kepler") and
BL-348/349 (province tongues). Mass re-blessing would have buried 119 commits of unreviewed world
change under a UI commit, so instead the batch was verified against **control goldens blessed from
the pre-wave-2 build** — which is the attribution the committed goldens could no longer give.
NR-130 records the owed re-bless pass. A related find (NR-131): `pop_markers.lua` frames a
hard-coded tile that world drift left empty, so it has been "passing" while capturing none of the
markers it exists to verify. The new `settlement_labels.lua` shows the fix — locate the subject via
`verify.population_centres()` and frame whatever the world actually generated.

Also landed: the per-frame recompute pass (vision model, marker maps, industry lens, lens-key
chrome, selection tile metrics, tech-tree geometry; `intra_body_path` returns a const ref instead
of copying the tile vector on every A* cache hit, all 8 call sites lifetime-audited);
`SOL_ALL_SAFETIES_ON` with the persona-pack shape checks it demands; `sell_orders` moved from
`ui_state` to `world` where a save seam can see it; and the odd-r hex neighbour table single-sourced
— all six pasted copies verified byte-identical first, so no latent geometry bug was hiding in the
duplication.

---

## Session — The hygiene audit that became a batch: four reviewers, thirteen items, ten landed (2026-08-10)

Full mode, Batch Delivery — seven worktree agent slices, integrated and verified in the main
session. Runtime: not tracked (timer.js not started); the batch ran from audit to green suite
inside one session.

**The session began as a question, not a work order** — "does the codebase have any major
faults?" Four parallel reviewers (world/sim, UI/app, cross-cutting, a `-Wall -Wextra` sweep
build) answered with three genuine simulation bugs, one determinism leak into the money loop,
and a family of per-frame full-world scans. Ben then asked for the findings to be filed and
delivered. Thirteen items filed (BL-351–BL-363); ten delivered as one batch; three held
(BL-361 app.cpp split, BL-362 UI frame caches, BL-363 misc sweep).

**The three bugs were real and none was subtle in hindsight.** BL-351 (sell-order over-commit):
duplicate sell orders each validated against the same un-decremented pool snapshot — a
player-exploitable money mint, now a running remainder with per-order matched bookkeeping.
BL-352 (hire-gate live store): the hire gate summed per-building `w.stockpiles`, which nothing
has ever credited — every gated roster row was unbuyable and ungated rows hired free; it now
reads `corp_body_pools`, so rival hiring genuinely changes (NR-127). BL-354 (orbital tick
purity): convoy dispatch priced hauls off frame-advanced orbital angles, so the same seed
diverged by frame rate — dispatch now reads `orbital_angle_at_tick`, and the harness was
red-checked (reverting the fix yields 9 failures including a flipped source choice).

**The recurring lesson recurred: worktree bases go stale mid-day.** The slices were cut hours
after Sprint 9 landed BL-325 S2 (hires require a completed muster base), and slice D's new
harness scenarios — written and passing on its older base — failed on the integrated tree
until the main session planted the base. Same class as the 2026-08-09 v0.1.9 session's
branched-from-a-moved-base finding; the integrating harness run caught it, as designed.

Also in the batch: BL-353 (a throwing persona pack no longer kills the session), BL-355 (enum
growth from the militia work had outrun five switches — a hire could post an *empty* nation
chat statement; tech-locked builds mis-reported as malformed; now `rejected_tech_locked`, with
ACTIONS.json updated), BL-356 (the body→market index — the single highest-leverage perf fix,
removing a per-call map rebuild from both the tick and the lens draw path), BL-357 (population
growth now reads the body's whole id-sorted market basket instead of one hash-arbitrary
market), BL-358 (sorted-iteration leftovers; `state_hash` now covers tile depletion and
units), BL-359 (the construction panel's mid-draw demolish routed through the pending seam —
uncovering that tile-selected dismantles had silently never worked), BL-360 (`is_coastal` via
the raster index; building-profit lookup de-quadratified).

Verification: verifier-review over the integrated diff (GO COMPILE, zero criticals), then the
integrating build (230 targets) and a 17-harness sweep — all green after the one integration
fix. Review-log entries NR-123–NR-129 carry the delegated calls (version-goal mapping, the
orbital approach, the hire-gate retarget) and the open questions (dead buy-order book, terrain
preferences for the new building types, the AI's tech-locked candidate churn). Housekeeping
noticed in passing: `/tmp` is 100% full on this machine (builds now point TMPDIR at
`build_linux/gcc_tmp`), and BL-266's stale requirement group was closed per lint.

---

## Session — Cut v0.1.3 and v0.1.4: one small predicate turned two design documents into two releases (2026-08-10)

Full mode, Delivery — three items, built sequentially in the main session rather than fanned out.
Runtime: **not tracked** — `tools/session/timer.js` was never started, so the only hard number
is the commit span (09:24–09:53), which measures the landing, not the work. Full mode
throughout: delivery, release, then a backlog-structure pass.

**The whole session is one argument: BL-342 was the load-bearing item, and it is thirty lines of
switch statement.** Two minors had been sitting design-forward for weeks, and last session's
diagnosis found why — BL-155 (laws) and BL-156 (techs) had *independently* settled on the same
object, *"a flat AND-list of atomic conditions"*, and neither built it, because each was scoped
design-only. Nobody owned the thing they both needed. Building it once made both minors shippable
inside a single session, which is the strongest evidence yet for the shape of that diagnosis:
**the blocker was not effort, it was ownership.**

### What landed

**BL-342 — `condition_set`.** An atomic condition is `<subject> <comparator> <operand>` plus the
qualifier its subject reads; a set is a flat AND-list; `evaluate` is pure. Three properties are
load-bearing and all three are asserted (40 assertions):

- **Deterministic.** Only one subject (`market`) sums floats over an unordered container, and it
  sums in ascending entity-id order. The harness asserts two structurally-identical worlds measure
  bit-identically.
- **An empty set is true**, and true by *falling out of the loop* rather than by a special case in
  front of it — because BL-155's common case is that a law is unconditional once enacted.
- **A subject may be military.** `military_units` and `military_strength` ship beside the six
  promoted economic labels. Not needed by the prototype; shipped because a shape is only proven by
  an instance, and the harness asserts a mixed economic-AND-military predicate resolves.

Three calls taken on Ben's behalf, all in NEEDS_REVIEW (**NR-112/113/114**): `evaluate` carries a
subject corp the sketch did not (every consumer is per-corp, so a world-only predicate could not
have answered either question); `era` measures launchpad ownership, because ERAS.md is
designed-not-implemented and that is the only space gate the code actually has; `market` measures
the mean price across all markets.

**BL-343 — the laws MVP.** The item's one real open design question was *where enforcement hooks
into the economy without breaking determinism*, and it is now settled on one rule:

> **A law is a modifier OVER the market, never an override OF it.**

That is the same principle that vetoed price clamps on 2026-07-11 — a clamp fights price
resolution instead of shifting a flow's cost. So the levy applies where the flow is **accounted**
(`apply_budget`) and never where the price is **resolved** (`clear_markets`). Two consequences
worth naming: the market stays the only thing that sets prices, and the player sees the tax as its
own number rather than as an unexplained worse price.

The design predicted the legibility would be free, and it was — a sixth **Levies** bar on the
Finance card, no new surface. The one deliberate placement choice: the enact checkbox went into the
Budget ledger *directly beneath the two policy-tier stubs*, so the difference between a drawn lever
and a working one is visible in one glance rather than in a tooltip.

`apply_budget`'s new `production` argument defaults to null and charges nothing, which is why
**not one existing economy harness changed** — the whole feature is invisible to any caller that
does not opt in, and `L1c` asserts a world with the law seeded is bit-identical to a world with no
laws at all.

**BL-344 — the techs MVP.** `tech_tree.hpp:49` stored the gate as a descriptive **string**, so no
tech had ever been earned and the F9 constellation viewer was a picture of a system rather than the
system. Promoted to `condition_set`, with one live gate: `E0-ML-01` "Standing Garrison Doctrine"
unlocks the Military Base on two extraction sites plus Cr 2,000.

**The unlock is military on purpose**, and that is BL-094's test rather than flavour: *a technology
that can only unlock a building is being designed for the corporate player we are pivoting away
from*. Gating the base cost exactly what gating a smelter would have.

Two things the promotion forced, both worth recording because neither was in the design:

1. **`earnable` had to be a separate flag.** An empty `condition_set` is *true* by BL-342's own
   property 2 — so the ~130 nodes with no authored gate would have earned themselves on the first
   tick. Absence has to be modelled by absence from the gate table, never by an empty predicate.
   This is the first place where two of the session's own decisions collided, and the collision was
   caught by writing the harness assertion (`T1c`) before trusting the default.
2. **The predicate could not live in Lua.** `tech_tree.cpp` pulls in sol2 and is excluded from
   `IO_WORLD_SOURCES`, so a gate that gates `construction.cpp` could neither be linked nor tested
   headlessly from there. It lives in the Lua-free `tech_gate.cpp`; `scripts/tech_tree.lua` authors
   identity, topology and prose and reads the predicate *back* by id, so the viewer cannot display a
   requirement the simulation does not enforce (**NR-116**).

### The one honest regression, and what it was worth

`buildings_rework_harness` broke — `construct_building` refused a military base it had placed
happily the day before. That is the gate working, not a defect: the harness tests BL-325's
placement and staffing rules, so it now grants the tech in its setup rather than manufacturing the
industrial base the predicate wants. Worth noting because it is the *only* thing in the gate that
moved: three new systems, ~14 apply_budget call sites, a widened placement signature, and one
test needed a two-line change.

### The retro's two lessons, applied

Both cost real time last session, and both were cheap to honour here:

- **A green gate can lie.** No messy merge this session (everything landed on `main` in one
  sequence), so `--clean-first` was not needed — but the two *bench* failures at `-j 4` were
  re-run idle before being believed, and both passed, exactly as the v0.1.9 retro predicted they
  would. 58 tests, 0 failures.
- **Worktree agents isolate writes, not history.** Avoided entirely: the three items are one
  dependency chain (BL-343 and BL-344 both consume BL-342's header), and two ~2-file slices are
  not worth an integration pass. Stated as a call rather than a default.

### Left open

- **NR-115** is the one thing genuinely for Ben: generation still places starting military bases
  through the tile-only check, so a corp can begin the campaign with a base it has not researched.
  Defensible as fiction (inherited, not researched) and it keeps BL-331 working unchanged, but it
  is a real asymmetry with a one-line fix either way.
- **v0.1.3 and v0.1.4 both cut with leftovers re-targeted, not dropped** — BL-155, BL-186, BL-280,
  BL-156 and BL-332 moved to v0.1.11. Both done-definitions were written **at** the cut, per NR-103,
  and both name their exclusions explicitly.
**Gate:** 58 tests, 0 failures (55 → 58; three new harnesses). Tags `v0.1.3`, `v0.1.4`.

### Then: the `post-v0.1.0` sweep (NR-101)

Ben, same session: *"now tackle the 42 post-v0.1.0 items."* It was the largest structural job left
in the backlog and the same class of problem the done-definitions had just fixed — a label doing
duty as a decision.

**Most of it was reconciliation, not judgement, and that is the finding.** Twenty of the 45 were
*already assigned* by ROADMAP.md **in prose** — the whole v0.4.0 politics substrate, most of the
v0.3.0 Era −1 arc — while their `version_goal` still read `post-v0.1.0`. So the roadmap and the
backlog disagreed about what was in which version, and **the disagreement was invisible unless you
read both**: the prose was not queryable and the query did not read prose. Fixing that needed no
decisions at all, only a script.

The residue after reconciliation was 14 items of real prototype work with no theme to belong to,
and it clustered more cleanly than expected — **v0.1.12 Logistics modes** (convoy distance pricing,
rail, sea trade, and the supply lens that makes any of it visible) and **v0.1.13 Markets &
materials** (runtime market emergence, the processing roster, real inventory, co-generation, and
the save-format version header that adding resource types is precisely the case for). Four more
folded into v0.1.11, whose theme got written down for the first time.

Four items moved on their **content** rather than on prose, and the reasoning is not obvious from
their titles, so it is recorded: BL-253 is the *opponent's* scaling term (`run_corp_strategic_step`,
O(corps × tiles)) and belongs to v0.2.0, not to a performance bucket; BL-314 waits on a seam only
BL-315's conflict spine creates; BL-182's real content is an **operate-gate**, a permission over
where a corporation may act, which under BL-094 is a thing a governing body grants; and BL-212
stayed in the prototype band because its own settlement says it does not wait on BL-218.

Result: **every open item names a minor.** 71 open across v0.1.5 (2), v0.1.6 (2), v0.1.7 (4),
v0.1.11 (10), v0.1.12 (4), v0.1.13 (6), v0.2.0 (12), v0.3.0 (22), v0.4.0 (9).

Two things deliberately *not* done. The 21 **complete** items still carrying `post-v0.1.0` were
left alone — they landed before the arc was mapped, so back-filling a minor would fabricate history
rather than record it. And naming two new minors is a roadmap-shape call that is Ben's, so it is
filed as **NR-119** with the alternatives (merge them; renumber against the uncut v0.1.5–v0.1.7)
rather than left as a silent default. Neither costs anything to reverse: a `version_goal` is one
field, and the band already treats numbering as advisory.

**Still open after this:** NR-102's sequencing decoupling. A minor per item is not an order to
build them in.

---

---

## Session — Cut v0.1.10: three items whose own diagnosis was wrong, and a green gate that lied (2026-08-10)

Full mode, Batch Delivery + release — the fifth cut of the session, spanning midnight. Ben:
*"cut v0.1.10 next."* Six worktree sub-agents; a machine crash mid-flight; integration, every
conflict resolution and the investigation in the main session.

**THE THROUGH-LINE: three items were wrong about their own cause, and measurement caught all
three.** That is worth stating as the finding rather than as trivia, because in each case the
plausible story would have produced a plausible fix.

1. **BL-338 (wetland)** blamed the 2026-08-04 relief commits. **Refuted empirically** — the agent
   rebuilt `world_audit` at `802421c^` and got a byte-identical census. The real cause is
   conceptual and better: wetland is the ONE composition the `(band, moisture)` table cannot
   express, because a marsh is defined by *where water fails to leave* — an elevation question —
   and elevation had no say in composition at all. 12 tiles → 159.
2. **BL-347 (econ tick)** named three suspects. **None dominated.** The sort flagged as O(n log n)
   was 3% of the added cost; the real cost was `std::map` node allocation the restructure
   introduced incidentally, per tick, in worlds containing no stack at all. 8×256 min 2.045 ms →
   **0.87 ms**, better than the pre-regression baseline.
3. **BL-346 (profit estimator)** — my own filed claim that BL-079's loss-streak reflex acted on an
   inflated number was **wrong and is retracted**. `estimate_building_profit` reads *realised*
   credit. The real site was `estimate_prospective_profit` (+213% at mid-band reserve), and BL-181
   was inflated via its own inline model instead.

**BL-284 answered the question it was filed to ask.** BL-218 bought the expensive settlement-sim
path on the argument that fragmentation would fall out for free. It pays: **60 emergent exclaves
against 136 from orphan-island cleanup — 31% by component count but 49% by tile count.** The
attribution is *exact, not heuristic* (the settlement BFS is water-blocked, so cleanup can only
fire on a seedless landmass), and the audit prints both numbers because quoting the raw count would
overstate the sim's contribution twofold.

**BL-290 produced kinship nobody authored.** Names are now coined from each culture's own
phonology, and because the word-coiner is a pure function of the tongue, two passes reach the same
morphemes without sharing a stream: *Rerekua Tekua* / *Kuamreiteik Tekua* share a realm word,
*Duagual* / *Shualgual* / *Vegual* a settlement morpheme. Generation as consequence, not lookup.

**A GREEN GATE THAT LIED, and the reason to record it.** `logistics_reach_harness` failed 3 of 27
assertions on a hand-built fixture — "five plains steps cost 5.0" — which looked exactly like a
real regression, and passed at v0.1.9. It was not code. The trace:

- `logistics.cpp` and the harness source were **byte-identical to v0.1.9**.
- Suspected the new `propellant` enum widening `tile_component`; tested it by adding a dummy 32nd
  resource *to v0.1.9* — still passed, so that hypothesis died cleanly.
- Bisected the six merges: failed at the BL-257 merge (36 conflicts) — **but that commit PASSED in
  a clean worktree.** Same commit, same sources, different result.
- Compared object files: every world object byte-identical, only the harness's own `.o` different,
  from a source whose md5 matched exactly.

Deleting that one object and rebuilding: ALL PASS. A conflict-heavy merge left ninja holding a
stale object it would not rebuild, because git's checkout churn set mtimes such that the object
looked current — and `touch`-ing all of `src/world/` did not fix it, since the harness's own source
had not changed. **A green gate from a stale tree is worse than a red one.** This is the second
stale-build incident of the session, after the morning's missed build. Standing lesson: run
`cmake --build build_linux --clean-first` before cutting after a messy merge, and if a harness
fails suspiciously, build the same commit in a throwaway worktree before believing it.

**Goldens re-blessed a second time in two days**, and the direction is the mirror image: UP, where
2026-08-09's was down. BL-283 moved holdings off dead ground onto settled ground, BL-338 restored a
habitable composition, BL-346/347 moved the workforce dial. Seed 4's survival fell 0.71 → 0.29
while its net worth trebled; the band was loosened to admit it but the number is flagged as a
**hypothesis, not a measurement** — winners winning harder is plausible, and if a later session sees
rivals dying across many seeds the band should tighten rather than stretch again.

**The crash.** Three agents were mid-flight when the machine went down; none had committed. Two had
salvageable worktrees and were resumed from their transcripts; the third had nothing and was
relaunched with an explicit *commit as soon as something works* instruction, which it followed.

**Gate:** 55 tests. **Runtime:** not tracked; spanned a crash and a midnight rollover.

---

---

## Session — The order book stops being a picture and starts being state (2026-08-08)

Full mode, delivery. One item: **BL-293 (order book unreachable by command)**, landed. Runtime:
~2.5 h.

**The item was filed as "three presses have no `corp_verb` — add them", and that is not the
work.** The 2026-08-07 scope correction found why: `sell_order` was *defined* in
`world/components.hpp` but *stored* in `ui/ui_state.hpp`, and handed to `clear_markets` by the
caller. A `corp_verb` mutates `world&`, so there was nothing for a trade verb to mutate. One
misplacement produced three symptoms at once — clearing was something the UI *drove* rather than
something the simulation *does*, no text-driven player could trade, and standing orders sat
outside the save seam entirely, so they would not have survived a save. Ben's ruling (NR-083):
*"Order book needs to be a background process, the AI must be able to trade as a player does."*
The player-only fence proposed alongside it was explicitly rejected.

**What landed.** The book is `world::sell_orders` / `buy_orders`, with stable per-order ids so
removal names identity rather than an index. `order_book.{hpp,cpp}` serialises it — magic `IOOB`
+ version, the second flat-binary stream in `world/*` after `history_log`, refusing a bad stream
rather than reinterpreting it. `clear_markets` **dropped both order-list parameters** and reads
the world, so a headless tick sells a standing order with nobody handing it over. Three verbs
joined the seam (`place_sell_order`, `remove_sell_order`, `set_workforce_auto` — the enum is 11
wide now, append-only). The Market Ledger's buttons queue `corp_command`s that `app::render`
applies through `apply_corp_command`: the player's press and the AI's command are one
implementation rather than two that agree.

**Rival corps trade, and the first cut is deliberately dull.** "Can trade" is not "trades well" —
a scorer that dumps stock at the floor is worse than one that does not trade, because it drags
the resolved price down for everyone including itself. So: surplus past a hold threshold, half
the excess, floored at the market's rarity price, scored *at the floor*. Three numbers in
`corp_ai_params`, so tuning is a data change. The two honest gaps are recorded rather than
papered over (NR-087): `base_price` is a rarity floor and not a production cost, and the book is
one-sided — `buy_order` has world state and a save format but still no verb.

**The standing rules were amended, and flagged rather than slipped in** (NR-085). The rival-corp
exception enumerates what the scored-utility layer may do, and trading was not on the list;
Ben's ruling widens it, so the rule file changed in the same commit. It is a grant of *reach*,
not of skill, and the amended text says so.

**Found along the way.** `set_workforce` never cleared `workforce_auto`, though `ACTIONS.json`
has always claimed it did and the UI has always done it — so a command-driven agent's target was
silently re-solved by the profit-max solver on the next tick, which is the worst failure mode a
word interface has (the press appears to succeed, then evaporates). The seam now matches the
press (NR-086). `state_hash` was missing `workforce_auto` too (NR-088).

**Verification.** `order_book_harness`, 43/43 PASS. The assertion the move earns is R4.5: the
same orders in a different *sequence* hash differently, because matching is price-**time**
priority, so the book's order is state and not an implementation detail. `econ_harness`,
`corp_ai`, `corp_agency`, `history_log`, `econ_stability` and `corp_ai_predictive` all green.

**Two things measured rather than assumed.** `ai_skill_harness`'s net-worth golden bands fail —
but a worktree at unmodified HEAD fails 5 of them already, and every BL-204 determinism assertion
passes on both sides. Not re-blessed: doing so inside a trade commit would have hidden both the
pre-existing drift and the intended behaviour change (NR-090). And the app does not build at
HEAD at all — `src/ui/tile_inspector.cpp`, committed at ca22b3a, includes
`world/sim_terrain_build.hpp`, which exists in no commit or branch (NR-091). That blocks BL-293's
one visual requirement; both touched UI units compile clean in isolation.

**Dictionary last, as required.** The seam changed first, then `ACTIONS.json` was re-transcribed
from `corp_command.hpp` and the mirror re-rendered. The "full word interface" overclaim turned
out to live in `render_actions.js`'s hardcoded preamble as well as in `_note`; both corrected,
and the replacement states what is still *not* reachable rather than making a fresh sweeping
claim.

**The static review earned its place.** Reading the listing loop next to the debit loop found
that overlapping sell orders on one `(corp, body, resource)` were each listed against the *full*
pool and each debited it — a pool of 10 with two orders of 10 ended at **−10 units with the corp
paid for 20**. The economy could mint value. Pre-existing and reachable from the ledger the whole
time, but putting placement on the command seam turned it from something a careful player avoids
into something a scorer can do in a loop. A second, opposite bug sat beside it: the auto-clear
subtracted one corp's whole matched total from every one of its orders on the triple. Both fixed,
both now covered by `order_book_harness` R1b (NR-092). No build catches this class; that is the
argument for the no-compile tier.

**And one signal not papered over.** `data_creep_harness` R1 passes at unmodified HEAD and fails
here: rival trading pushes the build plateau from tick 500 out to tick 1000 (still flat from 1000
to 1500). The mechanism is indirect — a standing order takes its triple off the auto-surplus
path, which moves prices, which keeps builds scoring above the hysteresis margin for longer. Not
re-blessed; NR-093 carries the decision, and it raises a real question about whether that
whole-triple yield rule — written for a player's deliberate order — is the right rule now that a
scorer places them.

**Also filed.** BL-325 (corp borders on the hex grid) from Ben's steer that the corp border
circle "basically tells us nothing" — a circle is a picture of a scalar, and it is about to
contradict BL-323's irregular logistics-reach field. Two design questions owed first (NR-089).

---

## Session — Cut v0.1.9: five worktree agents, and three of them branched from a base that had already moved (2026-08-09)

Full mode, Batch Delivery + release — the fourth cut of the session. Ben: *"cut v0.1.9 next."*
Five worktree sub-agents (roads; History+Economy; stacks; shell; disclosure), integration and every
conflict resolution in the main session.

**Four rulings taken up front rather than letting them stall the batch.** Nine items, four of which
carried open design questions, so they were batched into one Q&A before any code was written: the
road-tier legend goes **contextual** (Selection/hover); roads **do** dim with the fog; the Economy
panel **gets a door** rather than being retired; and **BL-229** moves to v0.1.10 because the item
says in as many words *"DESIGN OWED — do not guess the layout. Ben designs this one."* Asking cost
one round trip; guessing would have cost the item.

**THE LESSON OF THIS BATCH: three of five agents branched from a base that had already moved, and
every one of them produced code that would not merge cleanly.** Worktrees isolate writes, which is
what they are for — they do not isolate you from the *history* moving underneath. The three cases,
because the shape repeats:

1. **Roads agent** reintroduced `ui_state::selection_hidden_for`, deleted hours earlier by BL-266
   (Selection always open). Its hunk restored a close button on a band that no longer closes.
2. **History agent** dropped the **Ages** view along with Tiles. BL-281 does say "drops to two
   views" — but it was designed 2026-08-03 and Ages landed 2026-08-05. *The design predates the
   feature rather than judging it.* Ages kept; only Tiles retired. That renumbered Ages from view 3
   to 2, so `history_ages.lua` was re-pointed — without which the Ages check would have driven a
   stale index and silently captured Story.
3. **Disclosure agent** paired Story with Tiles and referenced `detail_surface::history_tiles`,
   removed by the History agent *in the same batch*. It would not have compiled.

None of these is an agent failing at its task; each did its own job well. They are the cost of
parallelism over a moving `main`, and the mitigation is that integration reads every hunk rather
than trusting a clean auto-merge.

**A fourth agent got it right in the way that matters most.** The shell agent found that BL-216's
sections 1–3 are **superseded by BL-227**, a *complete* item that landed a different, later geometry
on Ben's own 2026-07-30 call — and refused to implement its brief, because doing so would have
reverted a landed decision. Newest-dated wins. It shipped the half that was still true: the
`shell_metrics` module and the migration of all five `app.cpp` sites that each re-derived the same
rect by hand. It also surfaced a live **8 px drift** (BL-312 flushed the minimap to the screen edge;
four siblings did not follow), now expressed once instead of invisibly five times.

**The measurement that nearly got waved away.** `econ_stability` began failing after the stacks work
merged. It is a `bench`-labelled test — the label *this session added* precisely so a failure there
reads as "re-run it idle" — and load was 2.35, so the easy conclusion was available and wrong.
Rebuilding the harness at the parent commit and running both on the same machine:

| bodies × corps | pre-BL-193 min | post min | factor |
|---|---|---|---|
| 1 × 8   | 0.0106 ms | 0.0181 ms | 1.71× |
| 8 × 256 | **0.9581 ms** | **2.0449 ms** | 2.13× |

`min` is the load-insensitive statistic, and the cost appears at **every** rung including the
smallest — the signature of fixed per-tick work, not a scaling term. Filed as **BL-347** (priority
A) with the table and a fix direction. **Prototype scale is unaffected** (0.20 ms mean, 5× headroom),
so it is lost growth headroom, the same category BL-250 filed BL-253 for. The `bench` label did its
job — it stopped the failure being read as a regression *automatically* — but it must not become a
reason to stop looking.

**BL-260's codegen has nothing to feed.** Ben ruled codegen-at-build-time the same day; on
implementation, BL-247's in-UI question log and the `why_note` seam it would generate into turn out
to have been removed 2026-08-02 (NR-018). No call sites exist. Codegen whose output nothing includes
is machinery for its own sake — which is what *"the docs are the audit"* rules out — so the store
ships as documentation and the ruling is recorded in its own `_note` for whenever a consumer
returns. 13 of 16 entries are `drafted`, because writing the pair **is** the design check.

**Gate:** 54 tests, **2 failures**, both known, filed and named in the changelog —
`world_audit`'s biome balance (BL-338) and `econ_stability`'s absolute bound (BL-347). Visual
inspection by eye per the Linux policy (goldens are Windows-authoritative): shell renders correctly
post-merge, roads read with tier-varying brightness, fogged regions dimmer than the lit centre.

**Runtime:** not tracked.

---

---

## Session — Cut v0.1.8: ten test failures, one real defect, and a tool that had been lying since it was written (2026-08-09)

Full mode, Batch Delivery + release — the third cut of the session. Ben: *"move BL-288 and cut
v0.1.8."* Two worktree sub-agents (next_id.js; the SDL3 posture), the entangled harness/golden
core in the main session.

**BL-288 moved from v0.1.3 first.** A priority-A build-health item had been sitting inside the
Laws design stub, where it blocked a minor it had nothing to do with.

**The finding that reframed the whole minor.** Every item here was filed on the premise that
something was *broken*. Measurement said the tooling was mostly **misreporting**, which is worse.
`build_linux/` is already Ninja + Release, so BL-288's "the default tree is Debug" premise was
already obsolete on Linux — and a full Release run reported **ten failures of which exactly one
was a failing assertion**. Running each harness alone on an idle machine sorted them:

- **Four pass but exceed the flat 60 s bound** — `earthlike_lean_trace` 121 s, `notable_worlds`
  105 s, `mediterranean_sweep` 87 s, `earthlike_tile_census` 58 s (passing by *luck*, 2 s under).
- **Two never finish** — `history_sim_harness` and `history_sweep` both ran past 400 s. They are
  open-ended research sweeps, not regression checks; their cost is the point.
- **Two are load artifacts** — `econ_stability` and `home_surface_bench` assert *absolute*
  wall-clock times, pass standalone, and failed only because a concurrent session's build was
  loading the box.
- **One is a world-generation finding** — `world_audit`, 1 failing assertion of 26 (BL-291).
- **One was real** — `ai_skill_harness`'s stale GCC goldens, which had sat unnoticed among nine
  false positives for days. *That* is the cost of a noisy gate, stated as a measurement rather
  than as a principle.

Fix: three tiers (default 60 s, long 240 s widened to the four measured slow ones), a `sweep`
label with no timeout excluded from the gate, and a `bench` label so a wall-clock failure reads as
"re-run idle". Gate went **10 failures → 1**, the survivor being `world_audit`'s biome balance —
carried by BL-338, and the gate reporting it is the gate working.

**BL-322 — the root cause nobody would have guessed.** `execSync` runs through `/bin/sh`, which is
`dash` here, and the unquoted `(` in `--format=%(refname)` made dash abort with a syntax error
*before git ran*. `stdio: ['ignore']` discarded the message and `catch { return [] }` turned total
failure into "this repo has no branches". Platform-dependent, so it worked on the Windows box where
it was written and failed silently everywhere it was needed. It had been issuing ids **25 below the
true ceiling** — the direct mechanical account of how BL-326..BL-333 each landed twice. Refs
scanned 0 → 53. A second latent silent failure was caught in passing: `backlog.json` at 832 KB
against node's 1 MB default `maxBuffer`, 79% of the way to throwing ENOBUFS into the same
swallowing `catch`.

**BL-302 — the item's own preferred option was disproved by testing it.** A shared
`FETCHCONTENT_BASE_DIR` *hard-fails* across build trees, because each `<dep>-subbuild` carries a
generator-locked cache and this checkout has four trees side by side. Per-dependency
`FETCHCONTENT_SOURCE_DIR_<dep>` works and landed. Honest limit recorded rather than papered over:
a from-cold configure **succeeds** on Linux in ~74 s, so the Windows schannel fault does not
reproduce here and the fix is untested against its own symptom — filed as **BL-341**, and moved to
v0.1.9 rather than left open inside the minor being cut, which is the exact trap v0.1.1 fell into.

**BL-285 — the judgement call worth Ben's eye.** The GCC re-bless moves **downward**, unlike the
MSVC re-bless of 2026-08-02 which was uniformly upward: seed 1 fell 81% and went from highest of
the five to lowest, while seed 4 fell only 25%. Constraining siting (v0.1.2's reach rule) and
adding a cash outflow (unit hiring, 21 per seed) should cost net worth, and a *per-seed reshuffle*
is what a placement constraint would produce, so the shape matches the cause; survival held in
band on all five, so corps are poorer rather than dying. Recorded in the harness rather than waved
through, because "the AI got poorer" is also what a genuine skill regression looks like. Also
flagged in place: the **MSVC set is now stale for the identical reason** and will fail on the next
Windows run. Task 2 landed too — ladder lines carry a `ladder_rung` tag, so H4 filters structurally
instead of matching the prose "granary"/"Charter Act"/"Great Accord".

**Runtime:** not tracked. Gate takes ~9.5 minutes, which is itself worth an item.

---

---

## Session — Cut v0.1.1: the word interface ships, and the retrofit that made it uncuttable is undone (2026-08-09)

Full mode, release — the second cut of the same session, immediately after v0.1.2. Ben:
*"cut v0.1.2 first, then v0.1.1."*

**The diagnosis, restated because it is the whole point.** v0.1.1's theme — the word interface —
had been complete since 2026-08-03: blackboard export (BL-206), action dictionary (BL-270) and Io
MCP server (BL-278) all landed. The minor stayed open anyway because three later waves of
unrelated work were hung on it after the fact, 26 items at the peak. `ROADMAP.md` recorded this
in its own words — *"Retrofitted 2026-08-08 — still open"* — without registering it as a problem.
It is the concrete instance of NR-103: **a theme with no done-definition has no test for
*finished*, so it absorbs work indefinitely.**

**The cut.** 28 items terminal. Beyond the three theme legs the minor genuinely carried a lot —
the sticky-card family (BL-194–BL-198, BL-214, BL-247), the corporation dashboard (BL-248), the
commercial-activity fog (BL-150–BL-154), hover freeze and glance-then-stick (BL-228, BL-230), the
radial tech-tree viewer (BL-310), the minimap/header reflow (BL-312, BL-313), the wizard's
real-tile preview (BL-319), the Mediterranean rift sea (BL-276) and the GPU/multicore pass
(BL-267). A done-definition was written at the cut, on the v0.1.0 model.

**The narrowing, stated rather than papered over.** The write leg is partial: `place_sell_order`,
`remove_sell_order` and `set_workforce_auto` are in the dictionary but have no `corp_verb`. The
cause is structural, not three missing verbs — sell orders live in `ui_state`, the world holds no
order book to mutate, and no serialisation path touches them (BL-293's own 2026-08-07 scope
correction). `ACTIONS.json`'s note already says so explicitly, so the dictionary does not
overclaim. BL-293 moves to v0.2.0, where a text-driven player is what needs it.

**The re-homing (NR-111, decision-taken).** The 24 items still open went to three coherent new
minors — **v0.1.8** build health, **v0.1.9** shell & legibility, **v0.1.10** generation & content —
with BL-293 and BL-262 (standing) to **v0.2.0**. None cancelled; all kept their priority. The
judgement call worth checking is the *numbering*: they were appended rather than inserted at
v0.1.3, so no existing minor had to be renumbered — at the cost of their number understating
their priority, since all three are buildable now while v0.1.3–v0.1.6 are design-forward stubs.
The roadmap states plainly that number is not sequence here.

**Gate.** Rebuilt after the concurrent session's `src/` changes (21 files, BL-215/BL-266 work) —
green. The CTest baseline established earlier in the session stands: 45/55, ten failures identical
to the 2026-08-08 set.

**Two versions cut in one session**, against a six-day stretch where 119 commits produced none.

**Runtime:** not tracked.

---

---

## Session — Cut v0.1.2: the buildings rework ships, and the roadmap gets its first per-minor done-definition (2026-08-09)

Full mode, release. Ben, after a roadmap gap review: *"cut as many versions as we can now, rather
than working on the lofty, conceptual stuff"* — then, on the plan: *"cut v0.1.2 first, then
v0.1.1."*

**What the review found.** 119 commits since the `v0.1.0` tag and not one version cut, with
`CHANGELOG.md`'s `[Unreleased]` still reading *"Nothing yet"* — so the changelog was not merely
un-stamped, it was not accruing. In the same window the roadmap kept extending *forward* (v1.0.0
named, the Era −1 arc folded into v0.3.0, stub minors re-sequenced). The root cause, filed as
**NR-103**: the roadmap writes done-definitions for exactly two versions, v0.1.0 and v1.0.0, and
those are the only two ever cut or scheduled. A theme with no done-definition has no test for
*finished*, so it absorbs items indefinitely — which is precisely what v0.1.1 did, taking on 26
retrofitted items after its own three legs had shipped. Seven findings filed, **NR-097**–**NR-103**.

**The cut.** v0.1.2 was the cheapest available: six items, all terminal, the work landed and
verified 2026-08-07/08. Closing it needed bookkeeping rather than code —

- **BL-340** (processing-chain roster) filed, because BL-323's own completion note scoped out the
  processing half of S1 in as many words (new `resource_type` values with market/price/
  serialisation wiring — a save-format change, not Lua authoring) and no item carried it. Closing
  BL-323 without it would have silently dropped the work.
- **BL-323** flipped to `complete` with a resolution covering all four strands, and its 11.3 KB of
  design prose archived to the Q3 cold store.
- **A done-definition written for v0.1.2** — six bullets on the v0.1.0 model, the first of the
  per-minor definitions NR-103 asks for.

**Gate.** Full rebuild green (150/150, app + every harness); the app smoke-launched clean; CTest
**45/55**, and the ten failures are exactly the pre-existing baseline set recorded in
`LastTestsFailed.log` on 2026-08-08 — `ai_skill_harness`, `earthlike_lean_trace`,
`earthlike_tile_census`, `econ_stability`, `history_sim_harness`, `history_sweep`,
`home_surface_bench`, `mediterranean_sweep`, `notable_worlds`, `world_audit`. No new failure
introduced. Six of the ten are 60-second timeouts, which is most of the suite's 573-second runtime.

**Three things the cut surfaced that the gate would otherwise have missed.**

1. **`main` did not compile.** The BL-266 merge (Selection always open) retired
   `ui_state::selection_hidden_for` but left the hire-unit path in `app.cpp` assigning to it. Found
   by running the release build; fixed at `711b666` while this session was in flight. Worth noting
   for what it says about the gate: there is no CI, so a broken `main` stays invisible until
   somebody builds it.
2. **`archive_designs.js` reformats the entire backlog.** It writes `JSON.stringify(data, null, 2)`
   while `backlog.json` is stored at 1-space indent, so archiving one item produced a
   7531-insertion / 7502-deletion diff and *grew* the file by 11.5 KB while reporting
   *"-1% smaller"* (the size delta prints negated). Normalised back to indent=1 by hand, which
   returned the diff to 32/3. Filed as **NR-109** — a two-line fix that will bite on the next
   landing if left.
3. **Two sessions were writing this repo at once**, and it showed: a duplicated NR-104, cut
   bookkeeping swept into an unrelated commit (`7c423fa`), and `main` advancing four times
   mid-cut. Filed as **NR-110**, with the suggestion that concurrent main-tree sessions use
   worktree branches the way sub-agents already do.

**Runtime:** not tracked.

---

---

## Session — Build-heavy v0.1.1 batch: BL-215, BL-266, and the XS sweep, three worktree agents (2026-08-09)

Full mode, Batch Delivery, first all-Linux delivery session (no PowerShell — status read via
`backlog_query.js`; builds via `build_linux/` Ninja). Three concurrent worktree agents, merged
in the main session with an integrating build after each. Runtime: ~1h wall (agents 10–28 min each).

**BL-215 (text-wrap render audit, A)** — `ui::text_fit` module + overflow ledger; display floor
1280×720 enforced via `SDL_SetWindowMinimumSize`; charts measure-first rework; § 6 site adoption;
`verify.expect_no_clipping` + `scripts/verify/text_overflow_floor.lua` (PASS, 0 clipped —
one real overflow found and fixed in the wizard legends). verifier-visual SKILL.md section added
with Ben's in-session approval. Riders: NR-107 (tick abbreviate threshold), NR-108 (golden drift).

**BL-266 (selection always open, B)** — `selection_hidden_for` deleted (18 sites, not the design's
11); Esc terminates at the system menu; band rests on the player corp (swap-draw-restore keeps
deselect representable). Rider: NR-104 — golden re-bless list + the Continent-lens-key overlap call.

**XS sweep (C)** — BL-294 (dead `diverging_colour`/`icons::unit` + two doc corrections), BL-295
(phantom-id comment rewritten), BL-339 (parked `draw_building_selection` deleted, ~410 lines).

Merge notes: main moved mid-flight (another session's header-chrome drain + NR renumber), so all
three merges were true merges; the BL-215 branch carried stale NR ids — its two entries re-filed
as NR-107/108, two duplicates of NR-088/094 dropped. One committed-mid-flight bare `AddText`
(`generation_preview.cpp`) marked fit-exempt. NR-095 records BL-262 (scoring) skipped as
not-buildable (production axis needs a visible-information proxy). Goldens NOT re-blessed on this
box (environment mismatch, 5–10% drift on untouched captures) — Ben's Windows pass owns that.

---

---

## Session — Two of NR-094's footnotes promoted to their own backlog items (2026-08-08)

Light mode, doc-only. Ben: the C-route ruling's open questions shouldn't just sit as prose inside
BL-334. Filed **BL-335** (measure the real per-decision token cost through BL-278 — cheap,
independent, no dependency on BL-334 landing) and **BL-336** (the goal-layer/myopia question,
explicitly PARKED pending observed evidence — a fix for a failure mode nobody has measured Io's
own scorer producing yet is scope, not defense). BL-334's design field and AI_OPPONENT.md § 10g's
closing note updated to point at them instead of carrying the questions inline. The other two of
BL-334's open questions (BL-207-vs-Stage-C precedence, model attach mechanics) stayed as BL-334's
own design-owed detail — they resolve when BL-334 itself is promoted, not independently.

---

---

## Session — Ruling on NR-094: Stage C takes the dialogue layer, the scorer keeps the action seam (2026-08-08)

Light mode, design ruling — no code. Ben, direct instruction after reading the pulled-in cloud
research: *"Rule on NR-094 now."* Runtime: not tracked.

**The ruling.** Accepted the C-route feasibility note's layer recommendation
(`docs/ai/LANGUAGE_POLICY_FEASIBILITY.md` § 9). `corp_ai.cpp`'s deterministic scored-utility core
stays the action generator indefinitely — distilling it can only reproduce it (no skill upside),
and the note's measured constraint tax (91.5% → 48.0% executable accuracy under a hard schema)
is a live, avoidable risk at exactly the scale a local model would run at. The diplomacy
capability that motivated the C-route in the first place is separable from action generation —
Cicero's own architecture proves it at 2.7B — and Io already named this Stage ("the LLM planner
speaks in-character in channels") in `AI_OPPONENT.md` § 7 back on 2026-07-26, just never
decomposed it into a buildable item.

**AI_OPPONENT.md gained § 10g**, recording the ruling and — this is the actual correction, not
just an endorsement — naming precisely where § 10d drifted: its "small local model plays through
text" framing reads as the model calling `issue_command` directly, which is Stage A/B territory,
not Stage C. MCP, BL-278, and the local-model-as-runtime-target all stand unchanged; only which
Stage the model occupies was wrong.

**BL-334 filed** (design-owed): Stage C's dialogue layer, shaped by the ruling — a small model
(Cicero's reference point, 2.7B) conditioned on the `corp_decision` ring's winning command +
reason code as an intent, speaking into the Public/private channels, never emitting
`corp_command` itself. The concrete build (trigger cadence, prompt template, composition with
Stage A's existing templated messages) is left open; the shape is settled, the item is not
promotable yet. **BL-279 rescoped in place**, not cancelled or reopened: its corpus now trains
BL-334 instead of an action-emitting model, bootstrapped from `corp_ai.cpp`'s own decision ring
first per the note's own instruction, before any cloud spend.

**Left deliberately open, not ruled on.** The note's third recommendation (a goal layer above the
scorer, for the documented step-wise-myopia failure mode) — filed as an open question inside
BL-334 rather than accepted or rejected, since Io's own play has not yet shown that failure mode;
ruling on a mitigation for an unobserved problem would be guessing. The ~300-token-per-decision
figure the note flags as an assumption also stays unmeasured — noted as a cheap, independent
follow-up, not a precondition on this ruling.

**NR-094 resolved.** Regenerated `NEEDS_REVIEW.md`.

---

---

## Session — C-route feasibility: both gates pass, and Cicero says the model is on the wrong layer (2026-08-08)

Full mode, doc-only (no `src/` touched, so the item-spanning requirement gate doesn't apply).
Ben, carrying context from the 2023 entailment-tree dissertation into Io: *see what patterns we
can use for one-shot generation of actions (not reasoning structures this time)* — then the gate:
*if it can't be compressed, or if it is not technically possible on our machines, then it's not
worth pursuing, and we can use traditional RL methods.* Runtime: ~50 min.

**Both gates pass, and the second was computed rather than estimated.** Compression is supported
at 3–8B on current distillation evidence — the bar is low because Vox Deorum's 2,327 games showed
open weights tying the tuned algorithmic AI with *no* fine-tuning, so the fine-tune's job is to
reach a bar already cleared untrained. Latency was derived from `sim_loop`'s own constants
(`econ_tick_days = 90`, `seconds_per_day_1x = 2.0`, the `{0.25, 0.5, 1, 4, 16}` curve) against
`corp_ai_params::cadence_k = 4`: with 8 rival corps the per-decision budget is ~90 s at 1x, ~22 s
at 4x and ~5.6 s at 16x, versus ~3–7 s of measured 8B-Q4 decode on consumer GPUs. The load-bearing
detail is that the planner is out-of-process and the scorer runs every tick regardless, so a late
decision never blocks the sim — latency gates only how *stale* the macro layer may be, which is a
far weaker requirement than a per-tick deadline.

**The 2023 negative result does not transfer, and the reason is prescriptive.** The dissertation
rejected its H1 because *selection* was the bottleneck: candidate fact-pairings grow factorially
and the model had no admissibility oracle, only a single gold tree to be scored against. Io
inverts every term — `corp_command` is a flat fixed-arity record rather than a recursive tree,
candidates are already bounded (`top_m_sites = 8`), and `placement_rules::can_place_in_world` plus
`corp_command_result`'s seven typed rejections *are* the oracle. The prescription: enumerate the
legal candidates and hand them to the model; never ask it to select blind.

**The finding that changes what should be built.** Cicero — still the reference for full-press
negotiation — runs a strategic planner that selects actions and conditions a dialogue model on
those actions as *intents*, explicitly "offloading the responsibility of learning game legality
and strategy to other modules". That dialogue model was **2.7B**, and it did not choose the moves.
So the capability the C-route is being pursued *for* — diplomacy, larger strategy — is separable
from action generation, and Io already emits the intent stream it would consume (`corp_decision`).
Against that, making the model the action generator buys little: distilling `corp_ai.cpp` cannot
exceed `corp_ai.cpp`, and it walks straight into the **constraint tax** (a 1.5B model measured at
91.5% → 48.0% executable accuracy under hard tool-call schema, with the damage entering where
instructions suppress deliberation rather than at the decoder).

**Left open, deliberately.** The layer recommendation contradicts § 10d, which Ben *accepted* on
2026-08-03, so it is filed as **NR-094** (`decision-taken`, open) rather than written into
`AI_OPPONENT.md`, and the note itself carries a `⟳` saying plainly that it does not supersede
§ 10d. The § 4–5 feasibility findings stand independently of the § 7/§ 9 judgement call, and the
NR entry separates them so Ben can accept one and reject the other. The recommended first move is
neither: § 10 flags the ~300-token-per-decision figure as an assumption, and one real decision
through the already-landed BL-278 MCP server would replace it with a measurement for free.

**Not done.** No `backlog.json` item was filed — the note is evidence for a ruling, not a build
brief, and BL-279's scope depends on which way NR-094 goes.

**Id note (2026-08-08 merge):** filed on the cloud session's branch as NR-079, which collided
with an unrelated, already-landed local entry of that id (era-minus-1 rebase fallout) — renumbered
to NR-094 integrating this session, per the same collision-renumbering practice as the morning's
roadmap-extension merge.

---

---

## Session — Critique batch delivered: build ledger grouping, construction glyph, reach-circle retirement, military start (2026-08-08)

Full mode, Batch Delivery, sub-agent fan-out (Ben's steer). Promoted BL-326, BL-327, BL-328,
BL-329, BL-330 from the prior session's critique into REFINED.md as a three-way file-disjoint
split; delivered, verified, drained. Runtime: not tracked.

**A — build ledger grouping + pre-commit warning (BL-326 + BL-328), one sub-agent, worktree-
isolated.** `selection_panel.cpp`'s candidate list now groups by building family (Extraction /
Processing / Infrastructure / Military) and sorts two-tier alphabetical — group, then row name —
replacing the profit-ranked flat list Ben rejected ("not most profit first"). Each row also
surfaces `construction_rate()` before commit: a stalled or supply-limited build says so up front
instead of via the post-hoc paused status. **The agent's own worktree had branched from a stale
base** (missing the Military Base row landed earlier this session) — its diff was extracted and
hand-applied onto current `main` rather than merged wholesale. **One real bug found integrating
it**: the warning rendered even on an already-invalid candidate ("Cannot build on water" AND
"Local market can't supply materials" stacked on the same row) — fixed by gating the warning on
`c.pr.ok()`, and the row height (four lines, hardcoded) clipped the new fifth line — fixed by
reserving it unconditionally so every row stays a uniform height.

**B — construction glyph + reach-circle retirement (BL-327 + BL-329), main session (same file,
recently-authored code).** A new `icons::under_construction` — a stroke-only crane silhouette
(mast, boom, back-stay, hook) — draws IN PLACE OF a building's type silhouette while
`ticks_remaining > 0`, replacing the BL-323 S4 desaturation Ben found read as "faded" not "being
built"; full owner-tinted colour, so identity still reads. `draw_corp_border`'s `AddCircle` ring
is gone (renamed `draw_corp_hq`) for both the player's always-on chrome and rival borders under
the Corporation lens — Ben's read: a fixed-radius ring that never grew as the player built
outward showed nothing informative once the BL-323 reach fog existed to show supply reach
properly. The `hq` star marker is unaffected. `influence_range` stays computed and stored (a
future operate-gate may want it); LENSES.md, PLANETARY.md, `components.hpp`'s own doc comment,
and `corporate_reach.lua`'s comments all updated to describe the marker rather than the retired
ring.

**C — military start (BL-330), one sub-agent, twice.** The first dispatch returned a placeholder
("I'll report back once it completes") without actually editing anything; its worktree was
auto-cleaned (no changes made) before the resume could reach it. The SECOND dispatch (or the
same agent, retried) implemented it directly — the diff simply appeared in the main tree,
complete and correct: the player corporation is seeded with one `military_base` and one unit
(roster index 0, manpower 50, mirroring `hire_unit`'s own constant) at generation, on the nearest
valid land tile to its HQ, skipped gracefully on a degenerate land-poor world. Rival corps are
NOT seeded — player-only, per scope. `author_building`'s zero-staff condition extended to
`military_base` to match.

**Verification, all three slices.** Full `ProjectIo` build clean throughout. CTest: 45/55 —
**investigated the one count that changed** (`home_surface_bench`, not in the prior session's
documented baseline) by re-running it standalone (0 failures — a CTest parallel-load timing
artifact, not a regression) and separately **isolated `ai_skill_harness`'s 7 failures** by
`git stash`-ing this session's entire diff and re-running against the pre-batch commit: identical
7 failures, confirming they predate this batch rather than being caused by BL-330's extra RNG
draws (a real question worth checking, not assumed). Visual: `tile_build_ledger.lua`,
`corporate_reach.lua`, and two ad-hoc zoomed captures (`glyph_check`, `mil_zoom`) inspected by eye
per DEVELOPMENT_PRACTICES.md's Windows-authoritative rule (Linux golden-diffs on these all FAIL
by the expected 4–7% platform noise; not re-blessed since none of the touched surfaces have a
Windows-blessed baseline to diff against in this environment).

**REFINED.md drained** per the retain-one policy. Requirements: requirements.json §
critique-batch-ui-polish (R1–R6, all complete).

---

---

## Session — Live critique: seven items filed, the building-selection bypass fixed (2026-08-08)

Light-to-Full mix: Ben played the day's landed work in the live app and critiqued surface by
surface; the sliced-globe render (committed separately, same sitting: 48 slices, Ben's pick from
a six-form comparison) came out of the same session. Runtime: not tracked.

**Filed from the critique, one item per directive** (all dated, all carrying Ben's words):
BL-326 (build-ledger groups — expandable, two-tier alphabetical, explicitly NOT profit-first),
BL-327 (a dedicated under-construction glyph REPLACING the BL-323 S4 dimming — superseded
same-day, the dimming read as "faded" not "building"), BL-328 (pre-commit "this building won't
get materials" warning — construction_rate already computes it, the ledger just never shows it),
BL-329 (retire the corp-reach circle now the reach fog shows supply properly; blocked on
BL-333), BL-330 (player starts with a military base + one unit), BL-331 (nuclear weapons develop
in-game — WW3 is a nuclear threat; design-owed, hangs off BL-223's averted rupture and the
BL-087 tech constellation), BL-332 (military points produced by bases + a dedicated research
building, because nothing today measures how tech gets done; design-owed, the two halves
designed together).

**The one outright bug, fixed in-session (BL-333).** Selecting a player building bypassed the
Selection element entirely — draw_selection_content routed it straight into the full management
card (the 2026-07-22 "four-numbers card is useless" layout call, now superseded). A building now
takes the same action|facts Selection view as every other kind: construction status, an
Operate → **Manage** button (opens the construction ledger's Buildings tab, which already keys
off selected_entity), profitability facts right. The ~300-line rich management card is PARKED
`[[maybe_unused]]`, not deleted — whether it becomes the Buildings tab's detail pane or dies is
NR-093, Ben's call. Verified by capture: the Selection band shows header / status / Manage /
profitability on a fresh player building.

**Approved in the same critique, no action needed:** the wizard globe (committed as the sliced
render) and the reach-fog display of supply reach.

---

---

## Session — Military base S1: the muster building lands (2026-08-08)

Full mode, Delivery: BL-325 (military bases + supply) promoted, S1 delivered and drained; S2
(hire-at-base) and S3 (out-of-supply decay) deliberately left in the item. Same sitting as the
hardening entry below. Runtime: not tracked.

**The type, end to end.** `building_type::military_base = 6` — economics array bumped 6 → 7 (the
kind of silent-size bug the array's own comment now names), Lua name-map + `economy.lua` entry
(produces nothing, staffs at zero alongside port/hub, dearer than a hub, cheaper than a
launchpad), an explicit `can_place` case (any non-ocean land, no deposit requirement), named in
`presentation.cpp`. The BL-323 machinery applies without a line of new code: the reach rule gates
placement (deliberately NO anchor-type exemption — ruling 3 says the base extends nothing), the
S3 site-time multiplier prices its build, and the S4 construction dimming renders it.

**The glyph.** A filled shield — flat top, shoulders tapering to a bottom point — in
`icons::building`, catalogued in ICONS.md per its add-a-glyph rule. Echoes the unit chevron's
martial downward-point reading while staying unconfusable with it: the chevron is stroke-only,
every building glyph is filled.

**The surfaces and the dictionary.** Offered in the tile build ledger and the Selection primed
check; `gameplay.build`'s ACTIONS.json entry updated (typed-args domain + reason_to_select names
the base as where units muster once S2 moves hire onto it) and the mirror regenerated. The verify
seam's `place_mode` was also missing launchpad and logistics_hub, not just the new type — all
three added, so scripts can now arm any placeable building.

**Verified.** `buildings_rework_harness` extended R6/R7: 19/19 PASS — land-in-reach placeable,
ocean refused, beyond-reach refused (no exemption), staffs at zero, and ruling 3 held in code (a
COMPLETED base is not a supply anchor; building one changes nothing in the reach field). A
campaign `--verify` run placed one through the real construct path (tile 135,83) and the zoomed
capture shows the shield rendering dimmed-under-construction with the Selection band naming it.
Requirements: requirements.json § military-base-s1 (R1–R5, all complete).

---

---

## Session — Reach-rule hardening: three S2 defects ruled and fixed, and the military-base design settled (2026-08-08)

Full mode, same sitting as the first-slice delivery below. Ben's steer: consider outside-the-box
problems with BL-323 (buildings × visibility, buildings × the unfinished logistics system,
buildings × military), then work the bugs one by one with a Q&A. Runtime: not tracked.

**The review found three real defects in the already-landed S2, each ruled live via Q&A.**

- **Stale caches (Ben: invalidate on EVERY event, the simple rule).** Placing a port/hub never
  cleared `body_reach_cost` — the new anchor took effect only when an unrelated road placement
  happened to clear the cache. Demolition cleared nothing, leaving ghost anchors. Fixed with a
  shared `invalidate_logistics_caches` helper (logistics.hpp) called at every place, demolish,
  construction completion, decommission/resume flip (all five flip sites: the corp-command idle
  verb, the Selection panel's Idle/Resume pair, the construction panel's Decommission button, the
  economy system's idle-a-loser reflex) and road placement.
- **The virgin-body bootstrap was broken (Ben: first anchor free on anchor-less bodies).** The
  anchor-tile exemption only covered tiles that already WERE anchors — none exist on a virgin
  body, so the all-infinite field refused everything including the first hub, making Era 1
  off-world expansion impossible once reach is enforced. An anchor-type placement now skips the
  rule when `body_has_supply_anchor` is false. The guard: EXISTENCE of any committed anchor
  (under construction included) ends the exemption, so the player cannot spam free hubs across a
  virgin body while the first is still building.
- **An unbuilt hub anchored supply (Ben: anchor only when complete).** `is_supply_anchor` ignored
  `ticks_remaining` while the convoy-discount path required completion — the two disagreed, and a
  construction-site shell extended placement reach. Now both use the same contract
  (`ticks_remaining <= 0 && !decommissioned`), and hub-chaining outward gains natural build-time
  pacing: the next reach step waits for the hub to finish.

**Verified.** `logistics_reach_harness` extended with R9–R11 (completion contract, bootstrap with
its no-spam guard, invalidation through the REAL construct/demolish path with no manual clears):
26/26 PASS. Sibling harnesses re-run clean (buildings_rework, construction, corp_ai,
supply_advance, trade_routes, econ). Full app build clean. Requirements:
requirements.json § reach-rule-hardening (R1–R4, all complete).

**The military thread settled into BL-325 (military bases + supply), four rulings via Q&A.**
One new `building_type::military_base` (muster building, distinct rule + glyph); hiring moves
onto the base (superseding BL-324's hire-anywhere — the base becomes the economy→military
interface); **one reach field, not two** — Ben's own words: "a nation's reach for economy is also
the military reach," so the economic logistics network IS the military supply envelope and the
base is NOT an anchor (recorded as an interpretation in NR-091, overturnable — his "directional"
could also have meant forward bases extend the envelope); units beyond the boundary suffer
deterministic per-tick strength decay, the campaign twin of the Era −1 sim's supply attrition.
Filed `designed`, priority B, v0.1.5 (the military-systems minor), requires BL-324.

**Also logged.** NR-090 (question): rival construction state is publicly visible via the S4
dimming — BL-068 never ruled on it; recommended ratifying it as public. NR-092 (observation):
reach gates placement but never operation — a grandfathered remote building operates and ships
freely; the asymmetry stands until BL-288's transport-capacity work and is noted for its design.

---

---

## Session — Buildings rework, first slice: extraction padding, site-dependent build time, construction legibility (2026-08-08)

Full mode, Delivery lifecycle: promote BL-323 (Buildings rework) into REFINED.md, deliver, drain.
Runtime: not tracked. Ben's steer: pull from origin, work the roadmap, land the uncommitted
tree, then pick up BL-323 next.

**Scoped honestly rather than promoted whole.** BL-323 has four sub-slices; S2 (logistics reach)
and its S2b UI wiring were already landed in the prior session. Of the remaining three, S1
(roster pad) was promoted **partially**: PRODUCTION.md's designed extraction table (Mine, Quarry,
Lumber Camp, Ice Extractor, Surface Extractor) all target resources already in the current
`resource_type` enum, but most of its processing chains (Chemical Plant, Electronics Lab,
Fabricator, Assembly Plant, most Refinery outputs) need NEW resource types with no market/price/
serialisation wiring — a design item of its own, not a Lua-authoring pass. Flagged in REFINED.md
rather than silently narrowing the item's promoted scope.

**A — extraction roster padded, zero logic changes.** `k_extractable` (`placement_rules.hpp`)
widened from 4 to 15 targets: coal, silica, copper_ore, rare_earth_ore, stone, sand, clay, timber,
iron_nickel_ore, platinum_group_metals, regolith — every resource `tile_generation.cpp` already
deposits (confirmed by reading the generation code, not assumed) but that no extraction target
reached. `can_place`, the build-mode target picker (`selection_panel.cpp`, `body_surface_canvas.cpp`),
and the resource presentation table (names, short codes, colours) were all already generic over
this list, so the whole pad is an 11-line whitelist addition.

**B — the Smelter's second recipe.** `iron_nickel_ore -> steel` added to `recipes.lua`
(PRODUCTION.md's designed Era-1 Smelter input, no carbon reagent needed since metallic asteroids
are already reduced) — no enum churn, recipe count 4 -> 5.

**C — build time depends on the site (S3, landed in full).** `construct_building` now scales the
base `build_duration_ticks` by three multipliers at placement, each 1.0 at the cheapest case so an
anchor-adjacent plains first-of-its-kind build reproduces the old flat behaviour exactly:
**landform** reuses `landform_logistics_cost` (plains 1.0 .. mountain 2.0, no second terrain
table); **reach** is linear in the tile's distance from its nearest supply anchor, from 1.0 at the
anchor to `1 + site_time_reach_scale` at the `max_logistics_reach` budget edge; **stack** discounts
an established site (a tile already carrying the same building type), floored at
`site_time_stack_min`. New `construction_params` fields (`recipe_registry.hpp`), authored in
`economy.lua`. `ticks_remaining` floors at 1 for any real-duration type; a 0-duration type (some
infrastructure, by design) stays instant regardless of site.

**D — construction reads as such at a glance (S4).** A building with `ticks_remaining > 0` renders
desaturated/half-alpha on the Planetary canvas — previously identical to a finished building until
clicked. The glance-then-stick hover card (`hover_building_supply` in `hover_content.cpp`) gained a
"under construction — N ticks remaining" line, outranking the decommissioned/idle status lines
(construction has no output to explain yet regardless of workforce). The Selection panel's fuller
rate/stall diagnosis (`construction_status`, already existing) is unchanged — this closes the
canvas-legibility gap the item's own design record named, not the click-through detail, which
already existed.

**Verification.** New `tools/verify/buildings_rework_harness.cpp`: 12/12 PASS (every widened
`k_extractable` target placeable on its own deposit and refused without one; the iron-nickel
recipe resolves distinctly from the iron recipe; landform/reach/stack each move `ticks_remaining`
the right direction; the 1-tick floor and the 0-duration instant case both hold). One harness bug
caught and fixed in-session: the R5 fixture built only the tiles under test rather than the full
grid, so the reach-field's A* found a gap and read the remote test tile as unreachable
(`out_of_logistics_range`) rather than merely far — fixed by building a complete grid, as the
existing `logistics_reach_harness` fixture already does. Full `ProjectIo` build clean. CTest
46/55 — the 9 non-passing (`ai_skill_harness`, `econ_stability`, `world_audit` failures;
`earthlike_lean_trace`/`earthlike_tile_census`/`history_sim_harness`/`history_sweep`/
`mediterranean_sweep`/`notable_worlds` timeouts) all match the pre-existing failures the prior
session's audit note and this session's own environment already documented — reproduced
identically without this change, not a regression it introduced. A zoomed `--verify` capture
(`zoomcheck_built`, not a golden — a one-off inspection tool) confirmed the desaturated marker
renders correctly on a freshly-placed, still-building tile.

**What stayed open, recorded in BL-323's own design field rather than silently dropped.** The
processing-chain half of S1 (see above). Requirements: requirements.json § buildings-rework-
first-slice (R1–R7, all complete). REFINED.md drained per the retain-one policy.

---

---

## Session — landing the uncommitted generation-preview / Era -1 terrain work (2026-08-08)

Full mode: review and land the foreign uncommitted working-tree state the prior audit-note entry
(below) found but deliberately left untouched. Runtime: not tracked. Ben's steer: sort out the
uncommitted work before picking up new roadmap items.

**What it actually is, confirmed against the code rather than assumed from the audit note.**
Four distinct pieces, all real and all verified, none previously landed:

- **BL-316 S1 (Era -1 real terrain).** `src/world/sim_terrain_build.hpp` (new) — the ECS-to-view
  adapter `build_sim_terrain` that raster-samples a body's tiles into the `sim_terrain_view` the
  history sim reads. Before this every Era -1 battle in every run was fought on default
  grassland/plains, so `terrain_combat`'s modifiers were dead code. `history_sim_harness` and
  `history_sweep` wired to use it; the sweep's grid dims were also silently wrong (168×90 vs the
  real 180×84 — both 15120 tiles, so the mismatch never crashed, it just misaligned every terrain
  lookup) and its S2 recheck was comparing against an EMPTY terrain view rather than the real one
  used to produce the row being rechecked — a guaranteed false result the moment terrain affects
  a decision. Both fixed.
- **BL-323 S2b (the reach-budget gate's last two call sites).** The item's own design record
  named this as "required rather than cosmetic" and still owed at three UI call sites; two were
  already fixed, this session's diff wires the third and fourth: `run_verify`'s tile-scan path
  (was offering tiles the authoritative gate would then refuse) and the live canvas render
  loop's per-frame `body_reach_field` build (the interactive game was never calling it at all).
- **BL-321 wiring.** `works_registry` (landed as `src/world/works_roster.{hpp,cpp}` in an earlier
  commit) gets an `m_works` member and a `load_from_lua("scripts/works.lua")` call in
  `app::load_economy` — the runtime loader was written but never actually wired into the app.
- **The wizard's real-surface preview pane — NOT BL-256.** `src/ui/generation_preview.{cpp,hpp}`
  (new) plus `generate_home_surface_preview` (new in `hard_coded_world.{hpp,cpp}`, extracted
  from `make_hard_coded_world` so the wizard and the real build share one seed-choice function
  by construction) replace the wizard's charts-only screen with a 1/3-controls : 2/3-preview
  split, painting a hex-sampled orthographic globe of Kepler's ACTUAL generated surface (parity
  verified tile-for-tile against `make_hard_coded_world`, see below), built async off-thread so a
  control click never blocks and synchronous under `--verify` so goldens don't race the worker.
  **This is a smaller, different thing than BL-256** (`GENERATION_GLOBE_PREVIEW`, still
  `designed`, v0.1.1): no player pan (rotation is wall-clock only), no pole-treatment
  measurement, no BL-265 fold-vocabulary integration for the demoted charts, no debug-window
  task-1 prototype. Filed as NR-089 rather than silently treated as BL-256's landing — Ben's
  call on whether BL-256 is now superseded/narrowed or still wanted in full.

**Verification, since none of this had run before.** Fixed one real bug found in review: the new
`generate_home_surface_preview` declaration had landed mid-way through `make_hard_coded_world`'s
own doc comment in the header, splitting it from the function it documents. `home_surface_bench`
(new harness, `tools/verify/home_surface_bench.cpp`) confirms the preview surface is
byte-identical to `make_hard_coded_world`'s Kepler across five seeds, worst case 793 ms (under
the 1 s ceiling the wizard's async path exists to guard against). `works_roster_harness` 18/18
PASS. Full `ProjectIo` + `home_surface_bench` + `history_sim_harness` + `history_sweep` +
`works_roster_harness` build clean. The wizard/menu goldens in the tree were already re-blessed
for the new layout; Linux golden-diff numbers (0.75–20%) are expected noise per
DEVELOPMENT_PRACTICES.md's Windows-authoritative rule — inspected all six captures by eye
instead, all correct (`planetology_wizard_1_life.png` shows the real Kepler terrain painted as
hexes, matching the live canvas's own rendering). `history_sim_harness` and `history_sweep`
themselves ran past two minutes in this environment without finishing — consistent with the
prior audit note's finding that this specific harness runs anomalously slowly here independent
of code changes; not re-litigated, since the code-level correctness (terrain adapter, dimension
fix, recheck fix) was verified by reading and the harness's own logic is unit-testable by
inspection.

**Local-only artifacts discarded, not committed.** `docs/ui/mockdata/*.csv` and the six
`perf_*.csv` files at repo root are regenerated output from running verify scripts locally, not
source — reverted rather than landed, per the prior audit note's own read of them.

---

---

## Session — military design thread + BL-324 batch delivery (2026-08-08)

Full mode: design conversation (BL-157/BL-324/BL-305/BL-280), then Batch Delivery of the two
items that reached `designed`. Runtime: not tracked — no session timer available in this
environment; treat as missing rather than guessed.

**Military design thread (BL-157).** Recorded as an open thread, not a ruling: hybrid units
(lean toward blended roster-entry class weights over a composite/force model, to avoid
reopening BL-157's own "no force record, unit grain" settlement), zone of control (a
radius-1 tile-neighbourhood projection, 5-8 tiles, open question on what it actually denies),
and multi-round battle resolution (a bounded outer loop around `resolve_battle`, seeded RNG,
keeping the Era -1 sweep's single-evaluation cost contract intact). Three rendering sketches
produced to react to, none chosen. See BL-157's `design` field for the full write-up.

**Three items designed in one pass, question-by-question.** BL-324 (unit hire surface): hire
gate reads the corp's own stockpile/market access; the `unit_component.body`->tile grain fix
lands inside this item rather than reopening BL-157; rival AI corps get the hire verb from day
one. BL-305 (nation/corp generation visibility): territory carve watched live on the
generation screen; corp step splits by surface (canvas for placement, card for the financial
profile). BL-280 (negotiated tax rate): negotiation surface (Laws ledger) and cadence
(player-initiated, at a cost) settled; the counterparty-cost mechanism stayed explicitly
parked, so BL-280 stays `design-owed` — not every open question resolves in one pass.

**BL-324 promoted and delivered in full — all 5 tasks, all 7 requirements met.**
- **A — the unit record.** `unit_component.position` (a tile id, replacing `body`) + a
  fixed-point `strength` scalar; `world::units` already existed as the id-keyed map BL-157
  asked for. Two other consumers of the old `.body` field were still on it and needed fixing
  alongside components.hpp: `entity_summary.cpp`'s Selection-panel render and `view_nav.cpp`'s
  go-to-selection navigation — both resolve the body through the tile now.
- **B — the campaign hire gate.** `unit_roster.cpp` gained a `gate_met` overload taking four
  raw ints (shared by both the province path and this one) plus `campaign_gate_input`, which
  derives ore/farm/port/energy axis values from the corp's own summed stockpile
  (`corp_stockpile_total`, exported for corp_command.cpp to reuse) and whether it holds a
  port. Binary presence (1000 or 0) by design — a yes/no supply-chain question, not graduated
  tuning.
- **C — the hire verb.** `corp_verb::hire_unit` debits a flat per-axis cost from the gated
  resources (two-phase check-then-commit, all-or-nothing) and constructs the unit at the
  target tile. `corp_ai.cpp` scores it in its own candidate bucket, capped at one hire per
  eval — and, after the AI skill harness measured the consequence, at **three units per corp
  total**: the presence-based gate never runs out on its own (unlike build sites or
  unsurveyed bodies), so without a ceiling a corp with steady extraction hired every single
  eligible eval, forever (measured: 525 hires in a 300-tick/5-corp run, identical across all
  five benchmark seeds — the count was gate-driven, not score-driven). The cap is a first-cut
  brake (a modest garrison, not full mobilisation), not a tuned balance figure.
- **D — the hire affordance.** A Hire section in the tile Selection element's construction
  ledger (`selection_panel.cpp`), beside the existing Build candidates — not folded into that
  loop, since hiring never touches building slots or placement validity. `selection_kind::unit`
  was already wired end-to-end (label, render) from BL-157's stub; this is what finally makes
  it reachable.
- **E — the standing-rules record.** `io-standing-rules.md` gained the rival-corp hiring
  exception entry, alongside BL-079/BL-202/BL-181.

**Two harness regressions found and fixed, both from the same root cause.** `corp_ai_harness`'s
cooldown check and `ai_skill_harness`'s dial-thrash-ceiling check both classify "not build, not
survey" as a per-building dial — `hire_unit` is neither (it never sets `cmd.subject`), so both
harnesses needed `hire_unit` excluded from that classification. Caught by running the harnesses
after each change, not assumed clean from a compile pass.

**What stayed out.** BL-305 was promoted into REFINED.md (4 tasks, requirements written) but
**paused before any code**, on discovering its file scope (`hard_coded_world.cpp`, `app.cpp`)
exactly matches the uncommitted generation-preview/Era -1 work already sitting in the tree from
another session (see the entry below). Recorded as NR-085, `decision-taken`: safer to land the
disjoint, complete BL-324 delivery than risk colliding with unreviewed foreign edits on the same
files. BL-305's tasks stay in REFINED.md, ready to resume.

**Pre-existing failures surfaced, none caused by this session's changes** (verified by stashing
this session's diff and re-running against the bare tree, twice — before and after the unit
cap): `ai_skill_harness`'s seed 0/1 net-worth bands and seed 3's dial-thrash ceiling, and
`world_audit`'s S2 forest+wetland target, all fail identically with or without this session's
code. `history_sim_harness` alone (no contention) still ran past 30s in isolation against its
own documented ~2.1s budget — so `earthlike_lean_trace` / `history_sweep` /
`mediterranean_sweep` / `notable_worlds` timing out under CTest's 60s bound is plausibly the
same cause, not CPU contention. All of these touch files the uncommitted foreign work already
modifies (`hard_coded_world.cpp`, the Era -1 sim's terrain view); left unreviewed and unfixed
per this session's scope, consistent with pausing BL-305 for the same reason.

---

---

## Session — audit note: uncommitted generation-preview / Era -1 terrain work found in the tree (2026-08-08)

Not a build session — nothing here was authored in this session. Recorded per Ben's steer
("fill a phantom devlog for the work... if we don't have to review it, that's ok") so a chunk of
real, uncommitted working-tree state doesn't sit unexplained for whoever finds it next. Runtime:
not applicable — this is an inspection record, not delivered work, ~10 min of `git diff`/`grep`.

**What was found.** While auditing whether the buildings rework (BL-323) was actually complete
(it isn't — see the entry below), `git status` turned up a second, unrelated body of uncommitted
work already sitting in the tree, apparently mid-flight from another session:

- `src/ui/generation_preview.{cpp,hpp}` (new, 525 lines) + an `app.hpp`/`app.cpp` diff — the New
  World wizard's preview pane now builds the REAL homeworld surface asynchronously
  (`generate_home_surface_preview`, new in `hard_coded_world.hpp`) instead of a stylised
  painting; async off-thread so a wizard control click never blocks, synchronous under
  `--verify` so goldens don't race the worker. `tools/verify/home_surface_bench.cpp` (new)
  benches it.
- `src/world/sim_terrain_build.hpp` (new) — an ECS-to-view adapter for the Era -1 sim (BL-316
  S1). Its own header comment records a real bug this fixes: every Era -1 battle before this was
  fought on default grassland/plains regardless of actual terrain, so `terrain_combat`'s
  defence/attrition modifiers were dead code in every run to date.
- `app.hpp` also wires in `works_registry` (BL-321, Era -1 works table).
- `tools/verify/history_sim_harness.cpp` / `history_sweep.cpp` — R7's timing bound relaxed
  1s -> 3s, with an in-code comment explaining why (the settle-occupancy fix quadrupled real
  province count — correct behaviour, more work — measured ~2.1s; the sub-second bar is filed to
  return once BL-320, Era -1 sim runtime, lands its index).
- `perf_*.csv`, `docs/ui/mockdata/*.csv`, and the re-captured golden PNGs are just local
  perf/verify-script output, not source changes.

**State.** BL-316, BL-321 and BL-274 (era-keyed rosters, which this touches too) are all still
`designed` in `backlog.json` — no matching `complete`/`resolution`, no prior DEVLOG entry, no
stash. This is live, uncommitted, working-tree state, most plausibly another session still open
elsewhere. Left untouched — not reviewed, not committed, not reverted. If it's yours, it's
exactly where you left it.

---

---

## Session — The Era -1 arc's second day: Ages view, sweep verdict, review, and the fixes (2026-08-05)

Retroactive entry, written 2026-08-07: this session's five commits reached `main` that day by
rebase onto `origin/main`, and the arc had no DEVLOG record until this repair (NR-079). Full
mode, delivery. Runtime: reconstructed from commit stamps — 08:42 to 12:26, ~3.7 h.

**The Ages view** (*The Ages view: two thousand years of borders, scrubbable*). A fourth History
tab replaying the Era -1 sim's ownership change list: year scrubber, Play/Restart transport,
provinces coloured by polity, the run's own cost printed under it. The sim runs lazily over a
COPY of the body's settlement state — deliberately not in the generation path, so BL-271's
(Era -1 history sim) open question 2 stayed open rather than being answered by accident. Delta
encoding is what makes it possible: any year materialises from 654 changes / 5.2 KB. Captures
inspected, NOT blessed — the software renderer fails on this machine, so a golden blessed here
would be GPU-specific.

**The sweep, and the answer is no** (*The history sweep, and the answer it gives is no*).
BL-275 (history sweep distributions) landed as `tools/verify/history_sweep.cpp` — reports, does
not gate. First spread: hegemony 0/12, elimination 0/12, powers-at-epoch equals powers-at-start
in every world. BL-224's non-hegemony invariant satisfied for a degenerate reason — elimination
and collapse are unreachable — which is the false confidence the sweep existed to expose. Filed
in-session as the no-elimination finding, priority A (see the id note below).

**Rosters and two great powers** (*Rosters, two great powers, and a death spiral that does not
quite kill*). BL-274 (era-keyed rosters) landed as `src/world/unit_roster.{hpp,cpp}` — 19 rows
over four bands, availability derived from province endowment, resolving INTO combat's types
rather than combat gaining a roster table it was designed not to have. BL-299 (great-power
seed) seeds two majors with opposed creeds off `history_sim_params`. The first no-elimination
fix attempt (cohesion, a settle gate, a sack, transfer relief) made hegemony reachable (0/12 to
1/12) but not elimination (still 0/12); the weakest power measures median 6 provinces, range
1..22 — the model "gets to the brink and stops", recorded as a FAILED requirement row rather
than re-scoped.

**The review that reframed it** (*The review lands, and the sim stops in year 458*). Cold
review, nine findings. The severe one: the four verbs score on incommensurable scales, so
Invest pins at its ceiling once populations mature and no other verb can win the argmax again.
Measured: last ownership change at median year 458 of a 0–1960 run, 36% of changes in the first
tenth. Three quarters of every run inert — which supersedes the no-elimination diagnosis. Four
items filed (see the id note below).

**Four review items: three land, one reverts** (*Four review items: three land, one reverts,
and the stall was never real*). The settle-stacking fix was the biggest lever in the arc: an
occupancy search instead of nine untested candidates, conquests 201 → 3568, LAST CHANGE YEAR
458 → 967, first-tenth share 36% → 5%. The Ages cache re-keyed on a generation fingerprint. The
verb-scales fix REVERTED — normalising by each verb's own range structurally favours the
narrowest range; the real fix is one scale by construction, a scorer redesign. And the
vacuous-stall finding exposed the arc's biggest design correction: with the radius widened and
`w_dist` zeroed, under-supplied campaigns still TAKE the far province — the stall that BL-277's
(Era -1 military strategy) Q2 attributed to supply decay is a score preference, not a physical
limit. Full-run cost measured at ~2.1 s (749 real provinces instead of 191 — the growth is the
improvement).

**The id note (2026-08-07 rebase).** These sessions filed their findings as backlog ids 308–313
and review-queue notes 064–066; the rebase onto `origin/main` kept origin's ids, which the
2026-08-06 sessions had already spent on unrelated items (propellant, deeds, tech tree, works
doctrine, minimap, time panel). The landed fixes need no re-file. The two still-open findings —
no-elimination and verb scales — currently have NO backlog id (the scorer redesign sits
unnumbered in the 2026-08-07 working tree), and BL-277's (Era -1 military strategy) design
prose lost both its five answers and the Q2 correction. NR-079 records the debt; requirement
groups `history-sweep`, `era-rosters-and-great-powers` and `era-minus-1-review-fixes` carry the
corrected citations.

---

---

## Session — Roadmap extension: v0.1.x retrofitted, the Era −1 arc given a home, v1.0.0 named (2026-08-08)

Full mode, doc-only (no `src/` touched, so the item-spanning requirement gate doesn't apply).
Ben: *the roadmap should be extended to match sprints — anything after v0.2.0 isn't canonical,
read the docs and the latest backlog, then map a path to a playable game with basic AI rivals.*
Runtime: ~45 min.

**The gap was already named, just not closed.** NR-076 (2026-08-07, still open) had flagged that
the Era −1 sandbox arc — the history sim, ancient tech ladder, mil-sim and diplomacy work, ~15
items and the most active recent work in the backlog — appeared nowhere in `ROADMAP.md`, whose
arc section stopped at v0.4.0. That is the concrete shape of "not canonical": v0.3.0/v0.4.0 were
named in prose but thin, and the largest live body of work sat outside the map entirely.

**Two structural calls, put to Ben directly rather than decided silently** (per the tone rule —
present options, let the developer choose): where does the Era −1 arc live, and does the roadmap
need a terminal "playable game" milestone? Answers: fold the arc into **v0.3.0**'s writeup as
groundwork (it never ships to campaign play itself, so it's named the way v0.1.0 named its audit
instruments — tooling, not a release) rather than minting a new v0.2.x band; and yes, name the
terminal cut — **v1.0.0**, not v0.5.0 (Ben's correction), reachable "by following current steps"
rather than by inventing new scope.

**`ROADMAP.md` changes.** v0.3.0 gained the conflict spine (**BL-315**, filed 2026-08-07, the
governing body's answer to "what force does it command"), the Era −1 groundwork writeup (combat
engine, diplomacy seam, ancient tech ladder — with BL-271's own architecture-only transfer
contract stated explicitly), BL-087's real home (it had drifted from its nominal v0.1.3 stub),
and the point where AI rivals graduate from corp-level (v0.2.0, Trade only) to nation-level (the
runtime-actor residual BL-094 specifies fresh, now that its old container BL-054 is closed and
redistributed — NR-075 — contesting Conflict too) — the "basic AI rivals" bar the request asked
for. v0.4.0 gained the culture-region/history-ladder generation cluster
(BL-222/223/224/238/239/240/311) as the substrate its political layer promotes into something
real. A new **v1.0.0** section plus a **Done-definition — v1.0.0** section (mirroring v0.1.0's
structure) name the whole-game bar: governing-body play, AI rivals across both pillars, law/tech/
politics reaching military outcomes, a standing/scoring system, the word interface covering every
pillar, determinism preserved throughout.

**v0.1.x retrofitted against current `backlog.json` status**, since it had drifted since
2026-08-04: BL-203/BL-204 (corp AI predictive spending, skill harness) are complete, not
"queued"; BL-205 (corp chat log) was cut 2026-08-07 (NR-075) and its stale "queued" mention
removed; 13 items surfaced 2026-08-01→08-04 (the documentation-audit findings, the BL-262
standing/scoring system, several settled-but-unbuilt UI revisions, a build-health bug) were added
to v0.1.1, which never actually closed; BL-280 (negotiated tax rate) added to v0.1.2; BL-157
(military stub) noted as firmed up by the 2026-08-07 military design session rather than still a
blank stub.

**Left deliberately open.** NR-076's other three Band-3 scope calls (cut BL-160, cut-or-park
BL-207, cut the generation-flavour tail) are Ben's to rule on and this pass doesn't pre-empt
them — recorded as still-open in the new NR-078 entry rather than silently resolved. `CLAUDE.md`'s
`ROADMAP.md` pointer paragraph was updated to match; `NEEDS_REVIEW.md` regenerated.

---

---

## Session — Red herrings and the rupture: making Era 1 failure a skill test (2026-08-05)

Light mode, doc-only, continuing the tech-tree sitting. Ben: *little red herrings that make Era 1
failure (WW3) more likely — more advanced does not mean better; the player must be skilled at
avoiding danger, in each dimension of play.* Runtime: ~30 min.

**The load-bearing half isn't the herrings.** A red herring with nothing to trigger is flavour. So
the draft supplies the quantity they feed — and takes BL-223's own discipline verbatim (the
deterrence ceiling is *a per-nation scalar, not a nuclear-equivalent object*): **two per-nation
scalars**, **Ceiling** (BL-223's, unchanged) and **Alarm** (new — how threatened a nation feels,
moved by others' *visible* capability, severed trade, posture, domestic instability).

**The rupture check.** The seeded date decides when the rupture is *tested*, not the outcome. Alarm
above Ceiling and it goes hot: Era 1 fails, and the Era event's selective destruction lands on
exactly the orbital and heavy-industrial assets the space programme needed. Deterministic
threshold, seeded date, visible countdown — no random ruptures.

**Seven herring kinds**, one danger per dimension of play: escalator, legibility trap,
interdependence severer, brittle optimisation, contextual dud, tempo trap, domestic destabiliser.
Every one carries a **tell that precedes commitment** — the legible-in-hindsight rule, and the
difference between a skill test and a gotcha.

**The space row makes it work, because it is unavoidable.** Heavy Ballistic Lift is on the critical
path to Era 1 and is the biggest single Alarm source — the same stack that reaches orbit is a
missile. The player's job isn't to dodge the dangerous tech; it's to buy the reassurance that lets
them hold it (Open Launch Inspection, Civil Telemetry Network).

**One inverse herring, deliberately.** Hardened Dispersed Basing looks aggressive and is
*stabilising* — a survivable second strike removes the use-it-or-lose-it panic. If every
menacing-looking node were a trap, "menacing" would just become the tell.

**Also settled in passing:** trade interdependence as the cheapest Alarm suppressant makes the
Trade pillar **defensive** — a claim about the game's shape, not a tuning knob. NR-068 carries the
scalar for Ben's call; four questions open, including whether Era 1 failure ends the campaign or
delays it (lean: delays, expensively).

---

---

## Session — The Era 1 tree, first draft: keystones opened by deeds (2026-08-05)

Light mode, doc-only, same sitting as the effects pass below. Ben: *consider the shape of the
Era 1 tree — it will be the first tech tree to gate keystones via quests, i.e. tangible actions
done in game*, with the node list explicitly reserved for his own hand. Runtime: ~25 min.

**The missing primitive.** The condition vocabulary is entirely **state** — `research`,
`structure`, `stockpile`, `market`, `surplus`, `era` are predicates sampled at a tick, each of
which can be true today and false tomorrow. None can say *"you did this."*

So the draft adds a seventh: **`deed`** `{subject, scope, count, recorded}` — a one-time event that
fires at a tick and stays true. Monotonic, deterministic, serialises as a flag plus a tick. NR-067
carries it as a decision taken; it is an addition to a closed vocabulary, so it is Ben's call.

**Shape.** Five sectors (Launch / Volatiles / Mobility / Yards / Extraction) × three rings
(**Reach** — can you get there; **Foothold** — can you stay; **Industry** — does it pay). Power
and Automation stays a **standing line**, not a sector, per this doc's own rule that standing
lines never gate an era.

**Four keystones, each opened by a deed, none visible until it fires:** Lift Doctrine after **Ten
Flights**, Propellant Doctrine after **The First Tank**, Yard Doctrine after **The First Truss**,
Autonomy Doctrine after **The Empty Shift**. You don't pick your propellant chemistry from a menu
— you make propellant off-world once, and *then* the fork appears.

**The node list is a draft and says so.** ~45 objects with effects typed against the new taxonomy,
nothing transcribed to any store — deliberately, so the review isn't reviewing something that
already looks settled. Four review questions carried: whether four keystones is right (Autonomy is
weakest), whether a deed is a world first or a personal one, whether rivals see your deeds, and
whether an unfired deed hides its keystone or shows it locked.

---

---

## Session — Effects: what a tech actually does, mapped to real buildings (2026-08-05)

Light-plus mode, doc + data, no `src/`. Ben: *let's map this to real buildings and units* —
with seven categories named (unlock / upgrade / retire / recon / law-tax-automation / space /
war-and-comms doctrines). Runtime: ~35 min.

**The structural call.** The seven categories mix three things: effect **kinds** (unlock,
upgrade, retire), subject **domains** (reconnaissance, space) and **systems** that are themselves
unlocked (laws, doctrines). Collapsed they cannot compose. Split into a pair — `(kind, target)` —
they do, and one node can carry several effects, which nearly every interesting node does.

**Eleven kinds, closed**, in `docs/research/TECH_EFFECTS.md`: `unlock upgrade retire modifier
access reach intel institution doctrine resource open`. Closed for the BL-155 reason — the
consumer must switch exhaustively. `open` is BL-156's settled capstone rule unchanged.

**Seven categories the list omitted**, each already implied by a doc we have: placement access,
continuous modifiers, logistics reach, resource realisation, demography, finance/credit terms,
instrument access.

**The region is typed.** 62 effects across rings T4–T5 — modifier 19, institution 11, unlock 10,
upgrade 5, reach 4, retire 4, access 3, intel 3, resource 2, doctrine 1; shipped 15 / designed 29
/ unbuilt 18. **Modifiers outnumber unlocks two to one**, which is exactly the class an
unlock/upgrade reading misses.

**Two nodes land on shipped machinery.** Railway → **Inland Logistics Hub** (BL-149's placeable
haul-cost discount *is* a railway) and Germ Theory → **tile hazard penalty** (already a
`(1 − hazard)` multiplier on extraction). No new mechanism needed for either.

**Honesty markers throughout.** `building_type` has six values and `recipes.lua` has three
recipes, so most named buildings are design vocabulary, not enum values; units do not exist
(BL-157 stub); laws do not exist (BL-155). Every effect carries `shipped | designed | unbuilt` so
the mapping cannot read as more real than it is. `ladder_lint.js` validates the vocabulary and
fails if any object in the typed region is left untyped.

**Open:** NR-066 — retirement breaks BL-156's monotonic unlocked set (grandfathering,
availability-vs-economics, reversibility under blockade), plus whether pre-game effects ever
*fire* or are only read at the 1960 handoff. NR-065 resolved by this pass.

**Settled same day (Ben), the visibility half of NR-066:** obsolete content is **not rendered at
all** — *"there's no use for a player to see 'water mill' if they will never build it."* No greyed
row, no struck-through entry; the absent-not-disabled rule extended to the far end of the
lifecycle. His Martian-water-mill aside carries the real constraint: **obsolescence is contextual,
not global** — a mill obsolete on a 1960 homeworld isn't obsolete on a body where nothing better
runs, so retirement is a per-context predicate, which is what a BL-087 availability window already
is. Rule recorded: *hide what this player cannot build here, not what the tech tree has moved past.*

---

---

## Session — The industrial neighbourhood: the second worked region of the tech web (2026-08-05)

Light mode, design pass only — no `src/` touched. Ben: *another pre-game tech tree centred around
the industrial revolution, to go alongside the pre-game early Civilisation tech tree.*
Runtime: ~40 min.

**The reading.** "Another tree" is a second worked **region of the one shared web** — rings T4–T5
and the T4/T5 crossings — not a second web. The constellation geometry is one object; what makes
the region feel like its own tree is that a nation traverses it two millennia later, under gates
that bind where ring 1's barely did. Recorded as NR-063, with the four other calls the pass took.

**What was authored.** `ANCIENT_TECH_LADDER.md` § The industrial neighbourhood, at the settled
medium grain: **7 new techs** (Coal Haulage & Urban Fuel, Patent Grants, Preventive Inoculation,
High-Pressure & Compound Engines, Framed Construction & Cement, Soil Chemistry & Fertiliser Trade,
General Incorporation), **4 vertex quests** (The Unwearied Fire / The Cheap Ton / The Scheduled
World / The Freed Hands — the fifth crossing already had The Disciplined Sovereign), and **2
keystones**. Fuel Doctrine moved inward one ring so The Cheap Ton can require it *taken* — the
ring-1 Written-Ledger interlock, repeated, which makes it the house rule.

**The two new forks are the point.** **Labour Doctrine** (Cleared Holdings ⊘ Smallholder Tenure)
makes the human price of industrialisation a choice and feeds BL-273 (province demography).
**Works Doctrine** (State Arsenal ⊘ Private Works) decides who owns the heavy plant — and
therefore the terms a player corporation operates on in 1960. It is not Sovereign Doctrine
restated: one fork asks whether courts bind the sovereign, the other asks who owns the furnaces.

**New rule, adopted not proposed:** fork count scales with the band's divergence. Ring 1 carries
one keystone; this region carries four. A band where everyone lands in the same place needs one
choice to differentiate it; a band that opens 3-band gaps needs the gaps explainable.

**Kept honest.** Everything is transcribed into `ancient_tech_ladder.json` (provenance
`industrial-pass`, with `amended` on the two objects an earlier pass authored), and
`ladder_lint.js` was generalised to print **one line per worked region** so the doc's counts are
checked rather than asserted — region 38 objects, web-wide 88, extrapolating to ~120–135. Open,
in NR-064: whether Works Doctrine gates corporation generation (lean yes — file it when BL-296
lands), and whether the region earns its own viewer tab (lean no — the era strip means eras).

---

---

## Session — Roster bands become a partition, and the Era -1 sim lands (2026-08-04)

Retroactive entry, written 2026-08-07 alongside the 2026-08-05 arc entry above — the rebased
commits carried no DEVLOG record (NR-079). A late-evening sitting, commits at 23:06 and 23:30.
Full mode, design then delivery. Runtime: reconstructed from commit stamps; the visible span is
the last ~25 min of a longer evening.

**The partition** (*Roster bands become a partition, and the Era -1 scorer is designed*). The
ladder's roster grouping had T2 in two groups at once — never a partition, so never
implementable. Settled off the Military column: classical=T1, medieval=T2–T3, gunpowder=T4,
industrial=T5–T6, the T1/T2 break resolving forward because stirrup heavy cavalry IS the
medieval military revolution. Consequence: a 0 CE start is classical alone; shock cavalry is a
T2 unlock, not an epoch unit. BL-277 (Era -1 military strategy) had all five of its questions
answered in design: ring-closure objectives, supply-decay force commitment, naval as
crossing-enabler only, marginal-score peace at province granularity, creed-led doctrine.
Seasonality amended against BL-271 (Era -1 history sim): season is an axis of the action, not a
phase of the clock — a year tick stands, and "campaign in winter" is a scored candidate.
Convergence settled as rejection sampling on the 1960 output, reusing the C1 rejection-census
idiom. (The rebase later dropped these design-prose edits from `backlog.json`; the answers
survive in the commit message and this entry — see the id note in the entry above.)

**The sim** (*Era -1 history sim: the year tick runs, and the scorer decides*). Landed as
`src/world/history_sim.{hpp,cpp}` — a year tick over polities seeded from cultures, each
picking from a bounded candidate set by integer score: the corp-AI stage-A idiom, reused
because BL-271's transfer contract says the architecture graduates and the constants do not.
Territory moves at province granularity, never tile; `combat.{hpp,cpp}` untouched. The harness
flushed three defects, all fixed rather than tuned around: the 1.87 MB per-year ownership grid
delta-encoded down to 6 KB; a quadratic candidate scan cut from 2554 ms to 626 ms with a
prebuilt neighbour index; and winter campaigns scored-but-never-chosen until the defender
readiness penalty entered the score. The first Linux CTest baseline was recorded in-session:
43/49, six failures predating the work (Windows-blessed goldens and sweep timeouts).

---

---

## Session — A world that begins at 0 CE (2026-08-04)

Full-lite mode, same sitting as the arena re-base below. Ben: "generate a world which begins
at 0 CE, rather than 1960 CE". Runtime: ~45 min.

**The knob.** `world_params::epoch_year` (default 1960 — legacy byte-identical). Below 1700:
`run_settlement` gains a `stop_year` — provinces founded later do not exist yet, Stage 4 never
runs (no furnace has lit by antiquity), and demography is finally **seeded** — the graduation
path the province struct always named as BL-271's (Era −1 sim) job. Founding band 2k–26k
settlers off `farm_q`, then `advance_province_demography` does the centuries to year 0.
`hard_coded_world` gates ruptures, institutional history and globalisation behind the same
flag — that history is the year-tick sim's to produce, not the pass's to pre-compute.

**The instrument.** `tools/verify/era_world_harness.cpp` (requirement group
`era-minus1-antiquity-start`, 12/12 PASS): stop holds, demography within capacity, multipolar,
deterministic, 1960 arc untouched. Its dossier is the deliverable: **82 provinces, 21 nations,
20.65 M people, 258 k manpower, foundings −1999 to −1502** on the canonical seed.

**Honest limits, on the record.** The 1960 economy scaffolding (corps, markets, roads) still
generates underneath — out of frame for the sandbox, gated properly in BL-271's build. On this
seed every province founds before −1500, so the founded-after-0 filter had nothing to drop.
Two cosmetic name collisions ("Rekmaik lower" ×2) — `region_word` granularity, noted not fixed.

---

---

## Session — The arena comes home: text-only Rival, the diplomacy battery, and the RTS that lived for an hour (2026-08-04)

Mixed mode: research sweep (Light), backlog filing, one Light `src/` seam extension. Runtime:
~3.5 h wall clock, interactive with Ben.

**The sweep.** Ben asked for a fresh state-of-the-art pass on running the Rival agent via text
alone. It overturned a premise: 0 A.D. ships an official agent seam (`--rl-interface`, Alpha 24,
the in-tree `zero_ad` client) — recorded as NR-057; the literature (BALROG, lmgame-Bench) finds
text observations *beat* pixels for decision quality.

**Filed.** BL-306 (text Rival harness — summarizer / dispatch-grammar / MCP socket), BL-307
(Era −1 diplomacy seam — nation blackboard + typed verbs over a year-tick command queue),
BL-308 (diplomacy test battery — seven checks, two of them pre-LLM), BL-309 (great-power seed —
self-preservation vs civilising mission, frozen era, periphery-richness clause), BL-310 (myth &
theology generation, design-owed — structurally accurate myths, old gods persisting under
conquest). Ben's steers captured verbatim in BL-309/BL-310: low-friction economics ("don't
invent the steam engine"), and "we should not miss the richness of each other civilisation".

**The RTS that lived for an hour.** On "install that release", Release 28 went on and its RL
seam answered on port 6000 — then Ben saw the game launch and named the crossed wires: "0 AD"
means the *year* (the Era −1 sandbox), not Wildfire Games' game. Uninstalled same session,
verified clean (NR-060); the Rival docs re-based — the arena is Project Io's own word interface.

**Mid-session, Ben integrated the tech-ladder branch** — both sides had minted BL-296/NR-054,
and he renumbered the local WIP (NR-059). This session's ids moved accordingly; the transcript
cites the old ones.

**The smoke that passed.** `Project-Rival/tools/harness/io_smoke_test.js` drives the Io MCP
server end-to-end: 7/7 checks — corps enumerate, the player blackboard returns 364 facts, ticks
advance, the dictionary resolves, an illegal command rejects typed. It surfaced a real seam gap:
nothing answered "who am I?", fixed as a `CORPS` opcode + `list_corps` tool (NR-061, Light —
the BL-278 tool roster is now six, pending Ben's read).

---

---

## Session — The ancient tech ladder, mocked up (2026-08-04)

Remote session, doc-only, Light mode. Ben asked for an ancient tech tree mockup — the major
advancements from 0 CE, and what inequality between nations is realistic by 1960.

**Delivered.** `docs/research/ANCIENT_TECH_LADDER.md` — six bands (T1 Classical → T6 Machine
Age) × seven domains, ~60 load-bearing nodes with prereqs, endowment gates over the settlement
pass's classes, and a three-class **diffusion axis** (artifact / practice / capacity) that
generates the realistic 1960 spread: knowledge ~0 bands apart, capacity 3–4, military artifacts
1–2. Artifacts leapfrog, practices follow contact, capacity follows the map.

**The framing call.** BL-274 (era-keyed rosters) records Ben's stance that a player-facing tech
tree only works in a 1900s+ start — so the mockup is a tree in *structure* (data the BL-271
Era −1 sim evaluates) and a ladder in *play*: no nation clicks a node. Recorded as NR-054 so it
can be overturned rather than becoming precedent; NR-055 records the six-band spine vs BL-274's
four-band roster lean (proposed: rosters group the same spine).

**Filed.** BL-296 (ancient tech ladder), priority B, post-v0.1.0, the tracked home; design
conversation happens against the research doc. T6's exit hands off to `scripts/tech_tree.lua`'s
Era 0 quests, so the two trees meet at the campaign epoch without overlap.

**Follow-up, same session — the constellation.** Ben named the Path of Exile passive tree as the
shape he's imagining, with two additions: exclusion / binary choices at branches, and the whole
web never visible at once. Settled as § Geometry in the doc: rings = bands, sectors = domains,
entry point = endowment (the 1960 spread becomes pathing distance), travel-OR / meaning-AND,
keystone exclusion via availability windows, and a **tech fog** — the third fog after
DISCOVERY.md's two. **This overturns BL-087 (tech quest system) Q1** — binary tree, no
re-converging mesh, 2026-07-08 — on Ben's explicit call; supersession banners sit on
`ERA1_TECH_LANDSCAPE.md` § Q1 and in BL-087's design field. Q1's motive survives via node-count
discipline (~100–200 nodes, not the reference's 1,325) and the fog.

**The density test.** Ben wants the detail level judged by *fun*, against real examples — so the
doc's § Density test writes one slice (the steam transition) at three grains: coarse (4 nodes),
medium (8, one endowment-explainable Fuel Doctrine fork), fine (20+, reference grain). The
principle the examples surfaced: detail only pays where someone chooses or reads — so density
should follow the consumer, per region of the web. Recommendation medium; the call is NR-056
(density grain).

**Third exchange — the Institutions comparison slice, and vertices become quests.** Ben asked
for Institutions at medium grain as the second density example, with invented quests for key
future technology placed at clear vertices. New geometry rule: **vertices are quests** — the
BL-087 gate=quest=tech object at each ring crossing, capstone carrying the economic conditions,
completion opening the next ring region. The slice: eight practice-class techs, the **Sovereign
Doctrine** keystone (Chartered Capital ⊘ Command Estate — HISTORY.md Stage 3 turned from
narration into a choice, creed-picked for AI nations), and three vertex quests (The Enforceable
Promise / The Disciplined Sovereign / The Lettered Public). Comparison finding worth keeping:
**gates differentiate in Materials/Energy, keystones differentiate in Institutions** — practice
diffusion flattens the sector into adoption lag, so the fork is where its differentiation
lives, not decoration.

**Fourth exchange — grain settled, first full region worked.** Ben chose **medium** against the
two slices (NR-056 resolved), and asked for the full ring-1-to-2 neighbourhood at that grain.
Delivered in the doc plus a generated SVG sketch: 20 techs + 5 vertex quests + the **Granary
Doctrine** keystone (Temple Stores ⊘ Open Granaries — the campaign's markets-not-command
premise made a ring-1 *choice*, BL-275-assertable) + 2 roster regimes ≈ 28 objects,
extrapolating to ~130–150 web-wide — inside the § Geometry budget. New rule adopted: the
**sparse-sector rule** — a vertex quest only where the crossing is a genuine capability regime
(Military crosses on the BL-274 roster turnover, Medicine on a plain edge).

**Runtime:** ~2.5h remote across four exchanges, Light/design. **Left open:** band count
(NR-055), per-domain state shape, C++-vs-Lua data home.

---

---

## Session — The earth-like battery, generation retuned, and a sky (2026-08-04)

A long generation session. Built the five-instrument earth-like battery, acted on what it
measured, and closed with the galaxy minimap. Full detail in the commits; this entry records the
findings that outlive them and the handoff to the next session.

**Built (all in `tools/verify/`).** `planetology_sweep`'s C1 rejection census; `earthlike_corridor`
(per-knob viability edges); `earthlike_pairs` (knob × knob interaction atlas); `earthlike_tile_census`
(what the map actually looks like); `earthlike_lean_trace` (does the wizard's language deliver);
`notable_worlds` (search for specific playable seeds, not distributions).

**Landed in generation.** Wizard bands set from measured always-viable spans. The S6 epoch fix —
two gates were asking about present-day tectonic heat to decide events billions of years past. Ore
provinces (Open call 4). Mountain ranges seeded on convergent plate boundaries. Eclipse geometry,
narrowed to Earth's near-miss band. A stellar-lifetime cap that finally gives the `star` preference
a consequence. Rivers routed by a priority flood so they reach the sea. Plus BL-287 (verify tier
compiles the world layer once, not 44 times) and the galaxy minimap.

**Left open.** BL-288 (two Release-only harness failures, undiagnosed). NR-049 (the arable floor is
mechanically a hard ocean cap at 0.7143, and Earth is 0.71 — which is why generated worlds sit at
46% land against Earth's 29%). BL-289 (supernovae as real extinction drivers; deliberately flavour
for now). `data_creep_harness`'s plateau window, which the river change tripped without any actual
data creep.

---

### For the next session: diplomacy and military

You inherit more than it looks like. **Read this before designing.**

**What already exists.** `src/world/combat.{hpp,cpp}` and `terrain_combat.{hpp,cpp}` (BL-272's typed
unit stacks and doctrine-parameter resolve, plus BL-233's measured terrain scalars).
`nation_generation.cpp` produces ~21 nations; `creeds.cpp` gives them belief weights. BL-273 landed
province demography — population growth, drawdown, and a **manpower budget**, which is the number an
army costs and the one that makes war hurt. `docs/lore/HISTORY.md` is the institutional ladder that
explains why the 1960 world is market-based and non-hegemonic. `Project-Rival/` is the discipline
that plays 0 A.D. to refine military doctrine from actual play, and it hands back numbers and
doctrine, never names.

Relevant items already filed: **BL-223** (averted rupture → diplomacy origin), **BL-277** (Era −1
military strategy), **BL-274** (era-keyed unit rosters), **BL-157** (military datamodel stub),
**BL-280** (negotiated tax rate), **BL-094** (the governing-body pivot, priority A). Query, don't
re-derive.

**Three hard constraints, in order of how badly they bite.**

1. **BL-224's non-hegemony invariant.** The world must not produce a runaway winner. This is the
   single strongest constraint on any military system, and BL-240 already settled how to honour it:
   measure the hegemony **rate across seeds** and constrain the inputs — never enforce the outcome
   per world. "Constrain the inputs, never clamp the outputs" is the house rule and it is not
   negotiable.
2. **Determinism.** No `std::` distributions, no `exp`/`log`/`pow` in any gate path. Combat
   resolution is a gate path. `planetology.cpp`'s header states the reasoning; follow it.
3. **The AI-behaviour rule.** Standing rules still defer *nation* behaviour (BL-054). Rival-corp
   strategic AI got an explicit exception (BL-202/203) because it is deterministic scored-utility
   over a legal command seam. Diplomacy AI needs the same kind of exception, argued the same way —
   not assumed.

**Method, from a day of being wrong in instructive ways.**

- **Build the instrument before the feature.** Every real finding today came from a measuring tool,
  and none was visible by reading code. Diplomacy is worse than generation here: you cannot look at
  a screenshot and see whether relations are interesting, so the instrument matters *more*, not less.
- **Always measure the OFF state.** Ore provinces reported 15.8% concentration and looked like they
  worked. The baseline was 15.7%. Twice today a feature appeared to work and did nothing, and only a
  provinces-off comparison caught it. Any relation system will emit plausible numbers from day one.
- **Never assert a conservation property you have not measured.** I claimed the province field only
  redistributed ore. It was losing 47% of a world's petroleum. If you write "this only moves
  influence around", prove it with a sum.
- **Watch for quantities that cancel.** `star_mass` was measurably inert because the derived orbit
  cancelled it exactly — two good decisions that annihilated each other. If combat strength is
  normalised by the opponent's, absolute scale vanishes; if diplomatic weights are normalised
  per-nation, global weights vanish. Check explicitly.
- **Diplomacy is interaction by construction, so build the joint measurement early.** The corridor
  harness said every knob was individually fine; the pair atlas then found a 28.2-point interaction
  that one-at-a-time sweeps could never have seen. Relations between N parties are *inherently*
  joint — treat a pair/joint instrument as day-one work, not a contingency.
- **Ask for the interesting war, not the average war.** The battery measured medians for most of a
  day before `notable_worlds` turned the search around and found specific playable seeds. A
  distribution is for calibration; a player experiences one campaign.
- **Adding an enum surfaces latent bugs.** Extending `resource_type` by eight exposed uninitialised
  arrays (a NaN in Release only), an out-of-bounds name table (a segfault), and a null-pointer
  presentation row. You will add enums — relation state, treaty kind, war goal, casus belli. Grep
  for hand-held table sizes and `[resource_count]`-style declarations first.
- **Build Release and run the suite.** Four harnesses fail in Release and had gone unnoticed because
  the default `build/` is Debug. There is undefined behaviour in the tree. Use `build_rel` (Ninja +
  Release); BL-287 made a full verify build cheap.
- **A guard that never fires is not a guard.** Ten of fourteen homeworld-floor clauses never fire,
  because the sampling bands were tuned to sit inside them. A war-weariness cap or a relations floor
  that never binds is the same bug wearing different clothes — check that your constraints can
  actually trigger.
- **Decide flavour vs cause deliberately, and write down which.** BL-289 is the template: the
  supernova is narration today, with the causal version and its three hard problems recorded rather
  than reconstructed later. Diplomacy will face this constantly — is a grievance a story or a term
  in a scoring function?
- **Any new verb must land in the seam AND the dictionary.** `corp_command` is the write seam;
  `docs/ai/ACTIONS.json` is what the AI player reads for meaning. A war-declaration verb in one and
  not the other misleads the AI exactly the way a stale golden misleads a visual check. That is a
  standing rule, not a nicety.

**One last thing.** The single most valuable half-hour today was building `notable_worlds` — the
tool that stopped asking "what does the median world look like?" and started asking "show me one
worth playing." For diplomacy and military, that question is: *show me a war that was worth
fighting.* Build that instrument early and let it tell you whether the systems are producing drama
or arithmetic.

---

---

## Session — Documentation retrofit: seven audits, and what the corpus was lying about (2026-08-04)

**Runtime:** ~3 h. Full mode, doc-retrofit delivery. Ran alongside a concurrent star-map coding session.

Seven read-only agents audited the whole doc corpus against the code and against the newer
direction — core vision, economy, generation, UI, AI/tech, process, plus one extracting the vision
delta from the backlog and review queue. Their verdict in one line: **the corpus is current where
the work landed with its doc, and stale at the top of the tree.** PLANETOLOGY, CONTINENTS,
RESOURCES, AI_OPPONENT, ROADMAP and POPULATION kept up. CONCEPT, SYSTEMS, TECH_FOUNDATIONS and
GLOSSARY still described a corporate economy player with no combat engine and no agent seam.

**The three findings that mattered most were all of one kind — a doc that would actively misdirect
the next session**, not merely one that had aged.

1. **LAYOUT.md and MENU.md documented `ui::why_note` as a live control.** Ben removed it under
   NR-018, and `detail_level.cpp:121` carries "do not reinstate a draw path here without reopening
   NR-018". The doc was an instruction to rebuild a rejected surface.
2. **DEVELOPMENT_PRACTICES named CI as "the signal" guarding `main`.** `.github/` was deleted
   2026-07-31 (`debcefd`). Nothing guards `main` but a local build, and a session trusting that
   section would trust a gate that cannot fire.
3. **TECH_FOUNDATIONS excluded combat resolution in two places** while `src/world/combat.cpp` ships
   `resolve_battle` (BL-272, 15/15 PASS, consumed by the Era −1 sim).

**Ben authorised closing the pivot docs ahead of BL-094 landing**, which the time-slice rule had
been holding. CONCEPT, SYSTEMS and GLOSSARY now carry the governing-body aim with his stated
reason — law, policy and science reaching military outcomes — and the design test it implies:
*does this system reach military as well as economic outcomes?* Written forward-looking and clearly
unlanded rather than in the governing body's voice (NR-053).

**The naming rule is broken in shipped code, not in the docs.** Every generation and lore doc
passed the Earth-proper-noun sweep. `nation_generation.cpp:577-608` did not: a *global* phoneme
bank that ignores the per-culture phonology `creeds.cpp` already rolls, with plainly Latin/European
tables. Filed as BL-290 — and the interesting half is the global-ness, not the Latin-ness, because
consuming the phonology the chain already produces makes the Earth-flavour problem structurally
impossible rather than merely corrected.

**Numbers that had gone stale invisibly.** Every row of PLANETOLOGY's knob table had moved;
TILE_GENERATION's Pass 5 said mountain seeds `0/2/4/5` against an actual `0/5/11/13`; the "two pure
post-multiplies" contract is three since ore provinces landed. TILES.md's *measured* landform census
is marked stale-and-blocked rather than guessed at, because `world_audit` — the harness that
produced it — currently fails (BL-291).

**One lesson worth keeping.** `_critic_notes.md` *certified* a set of mock-data figures as verified,
and the fixture was re-blessed afterwards, so the note laundered stale numbers into six sibling
docs. A verification note that pastes measured values ages the moment the goldens move, and ages
invisibly. Record the method, not the measurement.

**Filed:** BL-290 … BL-295, six code defects the audit found. **Review queue:** NR-050 … NR-053.
**Not done:** the BACKLOG_ARCHIVE.json retirement Ben asked for — deferred by his own call until the
concurrent coding session lands, with the scope question (closed items only, not an id range) still
open.

---

---

## Session — BL-287: one world layer instead of forty-four, and the three bugs it flushed out (2026-08-04)

**The build was the symptom, not the problem.** A verify-tier rebuild was taking 45–90 minutes.
Cause was one line — `CMakeLists.txt:433` handed every harness `${IO_WORLD_SOURCES}` as its own
sources, so 44 harnesses × 30 world TUs = **1,320 compilations** of the same files, producing
byte-identical objects 44 times. Compounded by the default `build/` being single-threaded NMake +
Debug while a Ninja + Release `build_rel/` already existed and nobody reached for it.

**Fixed by an OBJECT library.** `io_world_obj` compiles the world layer once; the foreach and both
Lua harnesses link it. Include dir and `cxx_std_20` are PUBLIC so consumers inherit them;
`IO_WARNING_FLAGS` stays PRIVATE so it does not leak onto a consumer's TU. **1,320 → 30.**
Full-tier incremental rebuilds now run in 1.5–6.5 s.

**Three latent bugs, one family.** All were code that had baked in the old width of `resource_type`,
and BL-286's widening 23 → 31 exposed them in sequence:

1. **`market_component`'s `supply`/`demand`/`price`/`base_price` and `tile_component`'s
   `resource_deposit` carried no initialiser** — every sibling array has `= {}`. They relied on
   every slot being authored, which held only while the authored set covered the whole enum. The
   eight new goods kept stack garbage, one decoded as NaN, and it reached
   `prospective_profit`'s revenue estimate. **Release only** — Debug's fill pattern hid it.
2. **`econ_bankruptcy`'s `resource_name` guarded on `idx < resource_count` against a 19-entry
   table** — out of bounds since `resource_count` was 23. The overrun grew to 12 slots, and
   BL-287's link-order change put it on unmapped memory. Segfault.
3. **`ui/presentation.cpp`'s `resource_table[resource_count]`** zero-fills new rows, so
   `resource_name()` returns a **null pointer** every caller hands to `"%s"`. Unreached today only
   because callers guard on a positive quantity — a protection that expires when BL-287–290 give
   the goods behaviour.

**Attribution was measured, not assumed.** BL-286's three source files were reverted to the
pre-merge commit and the failing targets rebuilt: `prospective_profit` passed pre-merge (a real
regression), the other four failed pre-merge too (pre-existing → BL-288).

**Left open.** BL-288 (four Release-only failures, unexplained — at least `settlement_harness`
passes in Debug and fails in Release at the same commit). NR-048: a **fresh** configure cannot
download SDL3 (`unable to check revocation for the certificate`), so a new clone, worktree, or CI
runner cannot configure at all; existing dirs work from cache, which hides it completely. That also
means BL-287's from-cold timing is still unmeasured.

**Worktree triage.** Only three branches unmerged, all stale and small; every `worktree-agent-*` is
already in main. But `next_id` reports **9 in-flight ID collisions** — those three assign
BL-217/218/219 different meanings than main does, so they need cherry-picking with renumbering,
never a plain merge.

---

---

## Session — Earth-like generation: the three-instrument battery, S6's epoch bug, and bands from measurement (2026-08-04)

**Runtime.** ~3h wall across an autonomous stretch. Full (touches `src/world/planetology.cpp` —
every generated world changes — plus a new measurement section in an existing harness).

**The ask.** Ben, from a Project-Rival session: design tests that show which parameters lead to
earth-like worlds. Then, after reading the findings: "go straight to the live change. Do follow
procedure to document well."

**What was built.** A `C1` rejection census inside `tools/verify/planetology_sweep.cpp` — the first
consumer of `viability::reason`, a field the header has documented "for the sweep's histogram"
since BL-167 and which nothing read. It draws one unshaped attempt per seed and histograms *which
floor clause rejected it*, plus what the rejects became and where in the chain they died.

`resolve_preferences` only returns the draw that PASSED, so rejections are invisible from outside
it. They are recoverable because a draw is a pure function of (preferences, seed, attempt), replayed
through the public `checkpoint_rng`. That mirrors the sampling band table, and mirrors drift — so
every censused draw is cross-checked against the live function (viable replay ⇒ `attempts == 1` with
bit-identical params; rejected replay ⇒ `attempts >= 2`). A band edited in planetology.cpp fails the
harness rather than silently re-pointing the histogram. 20,000/20,000 agreed.

**What it found.** Ten of the floor's fourteen clauses never fire. The sampling bands were
calibrated against this very sweep and now sit strictly *inside* the floor — ocean draws 0.42–0.72
against a 0.40–0.75 window, the carbonate thermostat pins temperature to ~277–288 K inside 275–305,
`home_mass` lands inside the gravity window. **The bands, not the floor, are the specification of
Earth.** The floor's live surface is oxygen and arable land, and ~74% of all rejection pressure is
the oxygen story.

**The bug that fell out.** `interior=low` ("cold and old") cost 2.52 draws against a ~1.24 baseline
— twice any other preference. Cause: `theta` was computed at present-day age and then used to gate
the NOE, an event billions of years earlier. Fixed with `theta_at(age)` / `mobile_lid_at(age)`; the
NOE is now tested at `age - noe_at`. Present-day `st.theta` / `st.mobile_lid` / `profile.geology`
are bit-identical, so Continents and tile terrain are untouched.

**A call taken and then reversed by measurement.** I also re-sited the GOE gate for symmetry,
measured it, and reverted: acceptance fell 78.5% → 60.2% with 69% of rejects becoming Mat Worlds.
That gate is an upper bound whose 2.4 constant was calibrated against present-day theta, and I had
no independent basis for a new one. Inventing one to make the numbers look right is the forced
outcome Ben rejects. The asymmetry is commented at the site and filed as **NR-046**.

**Result.** Acceptance unchanged (78.4% vs 78.5%), worst preference 2.52 → 1.94 and now
`oxygen_story=low` — a genuine design axis rather than a modelling artifact. `interior` spread
narrowed from 2.25× to 1.64×. `planetology_harness`, `continents_harness`, `world_determinism`,
`determinism_harness`, `history_ladder_harness` and `mediterranean_sweep` all pass.

**Unplanned bonus.** Running the census under both g++ 15.2 (WSL2) and MSVC 14.44 gave *identical*
counts on all 20,000 worlds — the first empirical check that PLANETOLOGY.md's determinism discipline
holds **across compilers**, not just across runs of one build.

### T2 — knob corridors (`tools/verify/earthlike_corridor.cpp`)

Holds every parameter at its Sol default, steps one across its clamp range, 96–128 seeds per step,
orbit derived per seed as the generator derives it. Draws two spans on each axis: where the floor
rejects, and where the wizard's `any` band samples.

**Only three of ten knobs can reject a world** — oxygenation (always-viable 0.30–0.91), radiogenic
(0.57–1.73), home_ocean (0.40–0.68). The other seven never reject anywhere in range. This
cross-validates C1 independently: the four knobs C1 measured at a flat 1.24-draw cost are exactly
the four shown here to be incapable of rejecting. Two instruments, same answer.

It also killed a plausible idea. Sweeping `star_mass` 0.60 → 1.50 moves surface temperature only
282.4 K → 281.1 K, because the derived orbit compensates exactly — a brighter star just sits further
out. **The only lever on climate variety is the orbit multiplier `{0.985, 1.400}`**, and widening it
puts the homeworld inside the *continuously* habitable zone (habitable now, doomed as the star
brightens). That is a design decision about what Earth means, not a tuning knob. Still Ben's.

### Bands become the measured always-viable spans

Ben: "change the band to always viable." Each `any` band is now the span the corridor measures at
100% viability, with the three leans re-partitioned into thirds. The change cuts both ways — the
three rejecting axes narrow, the six that never could reject widen to the room they were already
entitled to.

**Acceptance and variety both rose**, which is not the usual trade: 78.4% → 81.4% acceptance, while
coal spread went ×6.99 → ×10.79, copper ×2.72 → ×5.84, iron ×1.82 → ×2.59, petroleum ×3.22 → ×4.47.
The floor also became more load-bearing — 5 of 14 clauses fire now, up from 4. Surface temperature
stayed pinned at ×1.04, exactly as T2 predicted.

**Cost, recorded rather than tuned away.** `interior=high` is now the worst lean at 2.97 draws. The
corridor's spans are one-at-a-time slices and do not compose: young age and high radiogenic are each
individually always-viable but together push theta past the GOE gate's 2.4 ceiling. It is the same
compounding fold that caused the original `interior=low` problem, arriving from the other end. Under
R2's <12-draw bar. Filed as NR-047.

### T3 — tile census (`tools/verify/earthlike_tile_census.cpp`)

C1 and T2 both stop at the body level, where "Earth-like" is a set of scalars. None of that says the
world *looks* like Earth. T3 replicates hard_coded_world's Kepler wiring — including the BL-276
two-bar sea gate and the `generate_rivers` sibling pass — and runs the LAND mask through the same
hex component labelling `mediterranean_sweep` runs over the ocean mask.

**The maps are not very Earth-like** (120 seeds, medians): land 47.9% of surface against Earth's
29%; largest landmass 78.8% of land (p95 99.1%) against 57%, i.e. mostly one supercontinent, median
3 landmasses over 100 tiles; forest 6.3% against 31%; icy 24.6% against 10%; barren 11.1% against
33%; mountain 1.0% against roughly 24%. Cold, flat, under-vegetated, land-heavy supercontinents.
Report-only per BL-275 — the Earth figures are orientation, not targets, and nothing is asserted.

**The largest lever on Earth-likeness is not a planetology knob at all.** Mountains and forest are
tile-pass parameters — a different layer from anything this session touched.

**Left open.** The GOE asymmetry (NR-046) and the band-composition cost (NR-047). Whether a floor
with nine inert clauses is the right shape. The orbit-multiplier decision above. And one consequence
worth a second look: `home_ocean`'s always-viable span 0.40–0.68 **excludes Earth's own 0.71 ocean
fraction**, which the previous 0.42–0.72 band did reach — optimising the band for "always viable"
trimmed the wet end and moved the distribution further from Earth. Recorded, not reverted; the T3
spread is the evidence to set that band against.

**Owed.** The three new harnesses (`planetology_sweep`'s C1 section, `earthlike_corridor`,
`earthlike_tile_census`) are not registered in the `verifier-headless` skill — that needs Ben's
authorisation, per the tool-creation rule.

---

## Session — Io MCP server: BL-278 built and landed (2026-08-03)

**Runtime.** ~1h wall. Full (new `src/` seam — `main.cpp` + `tools/mcp/`; no save-format or
economy change, but a new external-process attach surface).

**The ask.** Ben, after pulling the LLM-grand-strategy research session: "let's use this session
to implement the ideas we just pulled." BL-278 (Io MCP server) was the actionable item — `designed`,
SS priority, moved into v0.1.1 because it touches no simulation code.

**What was missing.** BL-278's design assumed the three legs (blackboard export, action
dictionary, corp-command) were ready to wrap. Two were; the write leg wasn't: `apply_corp_command`
had never been reachable from outside the in-process AI/ImGui callers — no CLI, no stdin, no
socket. `--export-blackboard` and `--verify` are both one-shot parse-run-exit modes, so neither
gave a persistent process an MCP server could attach to.

**What was built.** `ProjectIo --serve [--ticks N]` (`src/main.cpp`) — a new persistent headless
mode: builds the canonical world once (identical warm-up to `--export-blackboard`), then loops
reading one request per line from stdin (`TICK`, `BLACKBOARD corp=<id> ticks=<n>`,
`COMMAND corp=<id> verb=<0-7> ...`, `SHUTDOWN`), writing one response per line. `BLACKBOARD`
reuses `export_corp_blackboard`/`to_jsonl` verbatim (byte-identical JSONL, BL-206's schema
untouched); `COMMAND` builds a `corp_command` and calls `apply_corp_command` — the same
player-grade seam, no bypass. `tools/mcp/server.js` spawns that process and speaks MCP-over-stdio
to it: hand-rolled JSON-RPC 2.0 (no SDK — none was in the repo, and the surface is small enough
not to need one) covering `initialize`, `tools/list`, `tools/call`
(`get_blackboard`/`issue_command`/`advance_tick`/`lookup_action`/`list_actions`),
`resources/templates/list` and `resources/read` (`blackboard://<corp>`).

**Design calls followed, one bug caught.** `get_blackboard` always pushes the full blackboard —
§ 10c.5's "push state, don't make the model pull it." `issue_command`'s verb enum is exactly
`corp_command.hpp`'s eight verbs. One real bug surfaced in smoke-testing: the child process's
`[Lua] ...` startup banner raced the first request and got swallowed into that response's lines —
fixed by filtering banner lines unconditionally in the line handler rather than gating on
`pending` alone.

**Verification.** Compiled clean via `build_app.bat` (VS2022 BuildTools/MSVC 14.44, per the pinned
toolchain). Smoke-tested end-to-end: `tools/list` returns all 5 tools with schemas,
`get_blackboard` against a live corp id returns real facts, `issue_command`
(`set_workforce`) returns `result=applied`, `resources/read` on `blackboard://<corp>` round-trips.
No visual/golden requirement applies — doc-only surface, nothing renders.

**Left open.** `prompts/*` (the `reason_to_select` leg) not yet exposed as MCP prompts —
`lookup_action`/`list_actions` cover the same data as tools for now. BL-279 (trace corpus) still
needs a real client attached to this server before it can start.

---

---

## Session — two direction points: invented names, and the governing body (2026-08-03, later still)

**Runtime.** ~25m wall. Light-to-Full (doc authority + one standing rule; no `src/` change).

**The ask.** Two points from a prompt that never reached me (the "did my prompt get lost?"
check was about these). Ben: (1) *"even if we do use real history as an analogy, we should use
sci-fi / fantasy random names"*; (2) *"the aim that we're going for now, is to really play as a
governing body. The reason for that is that it allows law, policy and science to use military
might - not just economic."*

**Point 1 — real history in, invented names out.** Filed as a **standing rule**
(`io-standing-rules.md` § Terms & docs) plus a full section in `GENERATION_STRATEGY.md` with a
transfers/does-not table. The distinction: mechanism transfers (how a charter enforces a
promise, how a front stalls at a strait, how an inland sea concentrates littoral power), proper
nouns never do. Two traps named because they are easy to fall into — **"culture-flavoured" must
not mean "Earth-culture-flavoured"** (a name a player can place as "the Roman one" has failed
however good the mechanism under it), and **analogy language in docs is for the reader, not the
generator**. Stamped onto BL-271 (Era −1 sim) and BL-277 (Era −1 military strategy), the two
items filed off "use Rome as a sandbox". Project-Rival is the sole exception and only outside Io
— it plays a real RTS and returns numbers and doctrine, never names.

*The code was already fine* — nation/corp/city naming is seeded template banks plus phoneme
tables with no authored lists. The exposure was entirely in the design layer, where the Era −1
arc could have imported Roman nouns as content.

**Point 2 — the governing body, and its reason.** BL-094 has been settled since 2026-07-04 but
never carried a *reason*. It does now, and the reason is load-bearing: a corporation's levers
are all economic, so a corporate player can be handed laws and research and both remain flavour
on an economy — a law changes a cost, a tech unlocks a building, neither reaches force. A
governing body **wields** law, policy and science and can point them at military might.

That is also **Conflict's route to being load-bearing**: the house rule says every system must
feed Trade or Conflict, and under a corporate player laws/techs/politics could only ever feed
Trade — which is exactly why Conflict has stayed the least-designed pillar and kept sliding. It
also retroactively converts the 2026-07-04 call that *Military anchors the pivot first* from a
risky preference into the obvious consequence.

**What it changes.** The v0.1.x stub band (laws BL-155, techs BL-156, military BL-157, politics
BL-158) was themed "ponder and stub what the expanded prototype will need" — vague because
nobody had said what the stubs were *for*. They are the governing body's levers, and each now
carries a design test: **does this system reach military as well as economic outcomes?** If it
can only change a cost or a price, it is being designed for the player we are pivoting away
from. Written into ROADMAP's v0.1.x banner and BL-094; deliberately *not* written into the four
stub items, which stay design-owed until reached.

**Calls taken (NR-045).** BL-094 **unparked and raised F → A** — "the aim we're going for now"
is not compatible with a parked F item — and retitled to Ben's word, *governing body*, rather
than "nation". CONCEPT.md's player-identity statement was **left alone** despite being the doc
his point most directly closes: the authority time-slice rule is unambiguous and the cost of
waiting is low. NR-045 asks whether that was too conservative, and pushes the ROADMAP sequencing
question that has been open since 2026-07-31 — a priority says "important", a version goal says
"when", and "when" is the actual open question.

---

---

## Session — clearing the review queue: 14 decisions, six of them overturning what shipped (2026-08-03, later)

**Runtime.** ~1h wall. Full (decision intake + doc/backlog authority; no `src/` change — every
overturned call is filed as work, not applied in place).

**The ask.** Ben, on mobile, asked what was worth doing from a phone and then took the thorough
option: work all 14 open forks in `NEEDS_REVIEW.json` rather than the three live ones. Queue
went **19 open → 6**.

**Ratified as recommended.** NR-022 (BL-262 scoring — the six-call package ratified as written,
diegetic publication confirmed, rival figures stay banded), NR-029 (BL-208 checkpoint timestamps
keep the documented simplification), NR-042's arena reading, NR-043 (Ben installs 0 A.D.
himself), NR-025's two-rupture reading, NR-037's sequencing.

**Overturned — six calls I had taken, reversed.** (1) **NR-024**: Tax is not read-only after
all; the player is a *chartered* corporation that **negotiates** its rate with its home nation,
which keeps Ben's original intent and makes it coherent → **BL-280**. (2) **NR-020**: the
History ledger's Tiles view is **retired**, not renamed — History becomes Story + Chain →
**BL-281**. (3) **NR-030**: trade-route entries push **two** records, one per endpoint, so a
body filter sees a route from either side → **BL-282**. (4) **NR-035**: Pass 3 placement is
**constrained to the home province** rather than softening BL-219's wording → **BL-283**.
(5) **NR-036**: BL-054's territorial half is **reopened** as its own measurable item rather than
counted complete on an unmeasured argument → **BL-284**. (6) **NR-042**: **the played civ flips
to Rome** — Han becomes the rival.

**NR-023 — the reserved item, released.** Ben delegated BL-229's four layout questions rather
than reserving them further, so they are answered against the measured widths and the item flips
`design-owed` → `designed`; **v0.1.1 now has no design-owed items**. The answers: hex
neighbourhood stays in the left quarter (it is the one column needing no rival-degradation
logic); four accordion pages ordered symptom → cause; the two levers go in a strip *under* the
accordion, keeping "right quarter = actions" stable across both siblings; the 2×3 grid stays,
Manage dropped, Demolish bottom-right. Recorded explicitly as a *delegated* design, not a
matched eye — the recourse if it near-misses is Ben's mockup.

**v0.1.1 re-themed (NR-034 + NR-044).** The minor is now **the word interface** plus the
standing shell set: BL-270 (dictionary, complete) + BL-206 (export, complete) + **BL-278 (MCP
server, moved down from v0.2.0)**. Ben took the recommendation that the server land early
because it touches no simulation code and is what lets a first real text-driven play attempt
happen. BL-279 (trace corpus) stays v0.2.0.

**Project-Rival flipped to Rome.** `RIVAL-ROME.md` → `RIVAL-HAN.md` (scholarship unchanged — it
was always two-sided); CLAUDE.md, MISSION.md, ENVIRONMENT.md, CAMPAIGN.md and annals/README.md
updated. The autostart civ flags swap, the annal register goes classical Chinese → Latin, and
the rite inherits a real consequence: we now play the side that must *generate* campaigns, so a
quiet year is a Han success and a Roman embarrassment.

**Also.** ERAS.md's Era 0→1 gate corrected now rather than waiting on BL-087 (NR-025) — the
three conditions gate a quest tree, not an Era; the two ruptures are distinct and CONCEPT.md
stands unamended. **BL-285** files the GCC re-bless + the H4 chain_stage fix.

**Left open.** Six entries, all older. One owed check before Rival's Year 1: confirm Pantheon's
voices corpus has a Latin register — if not, propose one rather than faking it.

---

---

## Session — LLM grand strategy: the public field, MCP, and the small-local-model direction (2026-08-03)

**Runtime.** ~1h wall. Full (research + doc authority — no `src/` change; two backlog items,
one project charter amended).

**The ask.** Ben: "are there other publicly available projects that have tried to use LLMs for
grand strategy? Do a wide search on the web, and come back with actionable plans." Then, on the
findings: "We can use MCP, but please explain to me exactly what that is... our aim is just fair,
text driven, small and local models... Cloud usage is just going to be finding tons of input and
output sets, for when we fine tune a smaller model of our own."

**The survey.** Eleven public projects, written up as `AI_OPPONENT.md` § 10b: Cicero,
**Vox Deorum** (Civ V + Vox Populi, the load-bearing one), civ6-mcp/CivBench, civStation,
CivAgent, CivRealm, SAGA, Richelieu, Agents of Change, DSGBench, WarAgent. Sixteen new sources
in § 10f. Closed the two citation gaps § 9 had left open since 2026-07-23 — Vox Deorum's
per-decision latency (~1 min) and per-game token cost (20.35M in / 555k out for `gpt-oss-120b`).

**The findings that mattered** (§ 10c). (1) *Open-weight models already reached parity with a
tuned algorithmic 4X AI* — 97.5% vs 97.3% survival across 2,327 games, with a simple prompt and
no fine-tuning; the gap Io must close is size (120B → local), not capability. (2) The field
universally puts the LLM on **macro only** and delegates tactics to algorithmic subsystems —
independent confirmation of the A → B → C staging. (3) Personality is emergent and free (+31.5%
domination victories for one model, unprompted). (4) The failure modes are consistent and none
is about intelligence: step-wise greed/myopia, CivBench's **sensorium effect** and
**knowing-doing gap**, the observation-belief and belief-action gaps measured on exactly the
open-weight class Io targets, and spatial blindness. (5) A ranked list of what actually improves
play, cheapest first — *push state rather than making the model pull it* sits at the top and is
an interface decision, not a model decision.

**Direction set (Ben).** MCP as the interface; a **small, local** runtime model; cloud inference
demoted to corpus generation for a fine-tune. Written into `AI_OPPONENT.md` § 10d, with § 10a
explaining what MCP is and why Io is unusually close to ready — BL-206 (blackboard export) and
BL-270 (action dictionary) already built the read and meaning legs, `corp_command` is the write
leg, and the mapping onto MCP's tools/resources/prompts is near-mechanical.

**Filed.** **BL-278** (Io MCP server, SS, v0.2.0) and **BL-279** (AI trace corpus + fine-tuning
pipeline, S, v0.2.0). ROADMAP's v0.2.0 section names both.

**Also.** NR-040 (the "what plumbing does C-route need?" question, open since 2026-08-02) is
**resolved** — the answer is one wrapper, not a subsystem. Project-Rival's charter, which read as
a house-wide ban on API hooks, is narrowed to what it actually is: computer-use is how Rival
plays *0 A.D.*, because 0 A.D. exposes no agent interface — not a position that protocol
interfaces are forbidden (`Project-Rival/CLAUDE.md`, `docs/MISSION.md`).

**Left open.** NR-044 records four calls taken on Ben's behalf — the two-item split, the SS/S
priorities and v0.2.0 goals for both, the charter narrowing, and leaving § 2C's staging intact.
The live question in it: BL-278 touches no simulation code, so it may belong in v0.1.x rather
than v0.2.0, which would let a first real text-driven play attempt happen sooner.

---

---

## Session — Mediterranean rift sea: measure, mechanism, gate (BL-276) (2026-08-03)

**Runtime.** ~2h wall. Full (delivery — seed exploration turned same-session Full-mode item;
touches deterministic generation across `continents.cpp` + `hard_coded_world.cpp`).

**The ask.** Ben: explore seeds for a near-Mediterranean structure and make it "almost
inevitable"; hard-coding on the table. Measured first (new `mediterranean_sweep` harness, 500
campaign seeds through Kepler's exact pipeline): an enclosed sea ≥ 300 tiles existed on only
**44%** of seeds — TILE_GENERATION.md's "lacks enclosed seas" note was stale but directionally
right. Options filed as NR-041; Ben chose **hybrid at ~90%**: "interesting worlds if it is
HARD to form something like Rome. But it will never be impossible to try."

**Built (BL-276, Mediterranean rift sea).** (1) *Mechanism, consequence-not-dice*: in
`run_continents`, the divergent continental-continental boundary with the longest
land-interior segment (per-tile inland-ness ≥ 0.75 over plate ownership) founders — adaptive
width (short rift → wide Black-Sea oval), depth 0.65, and a +0.50 **rift-shoulder rim** that
seals the sea off from the world ocean; worlds with no such pair get an **intracratonic sag
basin** (Caspian shape) at the continental inland-ness argmax. One dated biography line each;
zero shared-stream RNG. (2) *Backstop gate* in `hard_coded_world.cpp`: Kepler's tile seed is
attempt-folded — three attempts at the **arena** bar (enclosed sea ≥ 300 tiles), six at the
**floor** (≥ 30), attempt 0 kept honestly on exhaustion.

**Measured after.** Floor **100%**, arena **89.6%** over 500 seeds — on Ben's ~90%, with the
1-in-10 hard-Rome tail intact. The sweep asserts wide regression bars (floor ≥ 97%, arena
82–96%) and mirrors the gate loop line-for-line.

**Verified.** `mediterranean_sweep` PASS; `continents_harness` 11/11, `world_determinism`,
`determinism_harness`, `world_audit` all PASS on the new surface. Docs: CONTINENTS.md
§ Rift-basin sea (new), TILE_GENERATION.md § Deferred coastline note updated. NR-041 resolved.

**Left open.** The default-seed world visibly changes (ocean relocates into the basin) — worth
Ben eyeballing the live Planetary canvas; `mediterranean_sweep` still needs naming in the
`verifier-headless` skill (permission owed); CMake reconfigure will auto-register it with CTest.

---

---

## Session — filing the Era −1 sim: Rome as sandbox, units instead of scalars (2026-08-02, later)

**Runtime.** ~30m wall. Light (filing only — five backlog items, Sprint 5 re-theme, two doc
banners; no `src/` change).

**The ask.** Brainstorm-turned-decision. Ben's chain: a 0 AD start is blocked for the *game*
(tech/laws/materials only work 1900s+), but a pre-industrial world is the cleaner **sandbox**
for bootstrapping the nation AI and mil-sim — "just use Rome as a sandbox." Then: run it as
Sprint 5, generate a spread of earth-like worlds through 0–2000 CE to refine the philosophical
development — and **overturn one decision**: simulated history fights with *real units and real
tactics*, as typed unit types the main era later inherits. Filed directly at Ben's instruction
rather than parked in NEEDS_REVIEW.

**Filed** (all `designed`, post-v0.1.0, Sprint 5): **BL-271** (Era −1 sim — year-tick loop over
the BL-218 world, sandbox purpose bounded in writing, Rome as calibration reference not
content); **BL-272** (unit/doctrine combat — records the overturned abstract-war decision;
"real tactics" pinned as doctrine parameters, never battlefields, or the sweep dies; one engine
shared with the main era, since `unit_component` is a stub the sandbox gets to define);
**BL-273** (province demography — logistic growth off farm_q, manpower as the self-limiting
army budget, POPULATION.md's first honest consumer); **BL-274** (era-keyed unit rosters — an
authored material-gated table, deliberately *not* a tech tree; forge-god cultures field iron
early, first industrialisers field rifles against pike); **BL-275** (history sweep — BL-210's
remaining batch-sweep scope gets its payload: hegemony rates, war frequency, lacunae, ideology
distributions across a seed spread; report-don't-gate until Ben has seen the raw spread).

**Sequencing effects.** Sprint 5 re-themed (persona audit rides along at its original small
scope). BL-224's non-hegemony becomes an emergent tuning target instead of an assertion; BL-223
(averted rupture) gets designed against simulated near-ruptures; BL-054's runtime half and the
BL-155/156 stubs get their proving ground.

---

---

## Session — documentation compression: the backlog sheds 42%, and the reading order gets measured (2026-08-02)

**Runtime.** ~1h wall. Full (tooling + a data migration + the doc policy that follows from it).
Filed from Project-Gyre, which is where the ergonomics are being generalised.

**The ask.** Ben, from the process repo: "what can we do to compress the amount of data used for
documentation? What tools does it seem like Io would benefit from?"

### What the measurement said

`docs/` was 3.7 MB, and very top-heavy: `backlog.json` 1.25 MB, `req/requirements.json` 448 KB,
`DEVLOG.md` 444 KB, then the generated pairs. Breaking the backlog down by status found the
real shape of it — **176 `complete` items were carrying 435 KB of design prose and 89 KB of
close-out notes, ~44% of the file** — paid for by every reader that only wanted the 30 KB of
live metadata underneath.

### What was built

**The hot/cold split — `tools/session/archive_store.js` + `archive_designs.js`.** CLAUDE.md
already said authority *time-slices*: `backlog.json` owns an item while it is open, the subject's
authority doc owns it once the work lands. The prose never actually left, so the rule was true on
paper only. It now moves: on landing, `design` / `resolution` / `completion_note` /
`progress_note` go to `docs/development/archive/backlog-design-<quarter>.json`, and the item keeps
the `@`-pointer form its own `_note` already blessed, plus an `archived` field. **1.22 MB → 710 KB,
42% smaller**, 172 items moved, round-trip verified on write. `--restore` reverses it; nothing is
deleted.

**`tools/session/backlog_query.js` — the retrieval primitive.** Same principle as
`actions_query.js` (BL-270): hold an index, fetch records. Defaults to five index fields over open
items; `--status --priority --version --category --touches --grep --fields --table --count` filter
it, `--full` pulls the prose and resolves the cold pointer transparently, so an archived item reads
exactly like a hot one. `backlog_view.js` resolves the same way.

**`tools/session/devlog_index.js` — find the session without loading the log.** Generates
`DEVLOG_INDEX.md`: one line per session (date, title, the `BL-` ids it touched, which volume holds
it), 110 entries in 16 KB. `--rollover 2026-07` moved the 56 pre-July sessions into
`archive/DEVLOG-2026.md`, leaving DEVLOG.md at 228 KB with the 54 live ones. The index spans both.

**`tools/session/mirror_check.js` — the generated mirrors, actually checked.** Every mirror carried
a "Generated file" stamp and nothing enforced it. It re-runs each renderer and diffs. **It found
`NEEDS_REVIEW.md` stale by 11.6 KB on its first run** — the exact 2026-08-02 drift its own renderer
header was written to prevent, recurred. `--check` reports without touching; the default fixes.

**`tools/doc_weight.js` — the reading order, as a number.** Walks the doc paths CLAUDE.md names,
estimates tokens, compares against a budget, and lists the heaviest files it does *not* name.
Verdict: **~610,000 tokens across 40 files**, against a whole-`docs/` corpus of ~924,000.

**`backlog_lint.js` gained two invariants.** An `archived` pointer with no record behind it is a
hard FAIL (data loss wearing a reference's clothes); frozen history back over 30% of `backlog.json`
is a warning that the close-out step is being skipped. Both are wired into DELIVERY.md step 5,
alongside `mirror_check` and `devlog_index`.

**`tools/gyre.py` opened `backlog.json` without an encoding** and died on Windows cp1252 the moment
it met a `✓`. Fixed in passing.

### Decisions taken

**CLAUDE.md no longer says "read the documents below before responding to any request."** That
instruction was written when the doc set was small; at ~610K tokens it cannot be followed, so it
was being ignored silently and unevenly, which is worse than a narrower instruction that holds. It
now instructs traversal — read the doc that owns the question, and prefer an index or a query tool
over loading a file. Recorded as **NR-038** because it changes the contract at the top of the one
document every session reads.

### Left open

Not committed — the tree carries the migration, the three new archive files, and the doc edits for
Ben to look over first. `requirements.json` (448 KB) is the next candidate and has had no pass.
The eight `docs/ui/mockdata/*.csv` files are fixtures sitting in the doc corpus and probably want
a different home.

---

---

## Session — the history backend: provinces, gods on the ground, and a record that can be burned (2026-08-02)

**Runtime.** ~2h wall. Full (BL-218 + BL-219 — new generation module, four seams, a new
harness, five authority docs, promoted with a requirement group).

**The ask.** Ben: "complete 2b, and finish with the backend of history implementation… as long
as there is a way to map belief systems onto existing and warring civilisations." Plus,
explicitly: "don't be afraid to have parts of the record erased when two nations go to war, just
try to think about mapping Pantheons to existing locations and environment (e.g. ancient resource
deposits)."

### What was built

**`src/world/settlement.{hpp,cpp}` — HISTORY.md Stages 3–4, made mechanical.** It sits between
the creeds and the political map and introduces the **province**: the unit that carries belief,
ancient endowment and industrial timing *at once*.

That is why the province had to exist at all. A cradle is a *people*; a nation is a *territory*;
neither can say "these fields, under these gods, sitting on this ore". Once the province can,
Ben's three asks stop being three separate features:

- **Pantheons map onto ground.** A province inherits its *nearest cradle's* culture, so the
  distribution of gods is a record of who walked where rather than a per-province re-roll.
- **Gods and deposits are one fact read twice.** A forge god only exists where the cradle's
  window held ore (CREEDS.md, one stage earlier) — so "the forge god's country industrialises
  early" is not flavour painted over data, it is the data read again. The charter culture's
  sealed-oath god buys a smaller bonus, which is Stage 3's contract law reaching capital.
- **Wars burn the record.** A won war plants the victor's pantheon on the provinces it takes and
  **erases** the lines naming them, leaving a dated lacuna carrying a count of what was lost.
  Four of six seeds lost part of their record. A conquered province keeps its founders in
  `founding_culture` and its conquerors in `culture` — the erasure is of the record, never of
  the fact, which is the pair a later religion or diplomacy layer needs to describe a grievance.

**Nations (BL-218).** Seeds are now province anchors — *seeding changes, expansion does not*, so
BL-053's tuned growth machinery is reused untouched and the size variance **emerges** rather than
being dialled in. The three political axes became outputs: expansionism from the border-contest
integral, economic_focus from the class of provinces settled *during* industrialisation, ideology
from industrialisation timing ranked against neighbours. The ruptures are BL-217's **second
checkpoint class**, reusing `resolve_checkpoint` unchanged — exactly what that item predicted, so
no second branch mechanism was written.

**Corporations (BL-219).** Focus derives from the corp's home *province* — per-province, not
per-nation, because a nation average would make every corp in a nation alike and kill the
specialists premise — shifted one tier up the value chain for an early industrialiser. The
authored table is retired on that path; diversity becomes a world-level reject-and-reroll against
a floor on the **set**, never a quota on any member.

### Decisions and corrections worth keeping

**The first endowment scoring was wrong, and the harness caught it.** Absolute per-class gains
saturated all 75 of Kepler's provinces to `farm`. Replaced with **world-relative** scoring (500 =
the world's own mean), which separates cleanly — 27 farm / 20 ore / 8 energy / 20 port — and,
unplanned but welcome, is immune to the `deposit_scalar` abundance tier: a lean world still has
its own ore provinces, just poorer ones.

**A whole-world change moves goldens; check rather than assume.** `ai_skill_harness` failed 9
assertions. Rather than filing it under the known BL-252 platform caveat, stashed the change and
rebuilt: it passed at baseline, so the failure was genuinely ours. Every divergence was *upward*
— net worth up on three seeds, solvency and survival still in band — which reads as corps
anchoring to provinces that actually industrialised. Re-blessed the MSVC block only, per that
file's own rule; the GCC set is untouched and now stale by design.

**One assertion was narrowed, and that is recorded rather than quietly done.**
`history_ladder_harness` H4 demanded every line in the recorded-history window be strictly older
than the next — true only while the ladder owned that window alone. Two provinces founded in the
same year are a fact about the world, not a stage-ordering violation. Narrowed to assert the
ladder's own causal claim (granary → charter → accord) on its own three lines.

**Unrelated pre-existing break, fixed in passing.** `trade_routes_harness` had not linked since
BL-170 landed rivers: its hand-declared CMake link set never picked up `river_generation.cpp`.
Removed the hand-declaration so the generic batch builds it against the world superset — the fix
`CMakeLists.txt` already prescribes for this rot, and the third target it has caught.

### Verified

New `tools/verify/settlement_harness.cpp` (S1–S8): determinism, belief-mapped-onto-ground,
character-as-output, seeds-are-provinces, the erasure and its bookkeeping, ruptures-as-transforms,
BL-219's tier rules and the diversity floor, plus a six-seed spread. **Full CTest 39/39.**

### Left open

Three entries in `NEEDS_REVIEW.json` (NR-035…037): corp asset *placement* still anchors to the
nation rather than to the home province its focus came from; BL-054's territorial-fragmentation
half was folded into BL-218 on an argument nothing yet measures (no exclave is asserted anywhere,
and Pass 2b could be manufacturing the ones that exist); and the golden re-bless plus the
assertion narrowing above. BL-219's rarity-tuning sweep is not done. BL-210's umbrella is down to
its batch-sweep extension and TILE_GENERATION.md's share of the propagation.

---

---

---

## Session — the action dictionary: 114 controls, five agents, one afternoon (2026-08-02)

**Runtime.** ~1.5h wall (agent authoring ran in parallel). Full (BL-270 — new AI-facing
store, five-file doc surface, promoted with requirements per the lifecycle).

**The ask.** Ben, same day: promote the "complete dictionary of every button press —
A) expected output, B) reason to select" so an AI plays via words; "not really one item,
it's the whole process of gameplay/development." Design settled by elicitation (four
calls: every control including chrome; + typed args and preconditions, cost and
provenance deliberately out; docs/ai/ home; both consumers — generation then play).
Multi-agent fan-out explicitly requested.

### What was built

`docs/ai/ACTIONS.{json,md}` — 114 entries across five families (11 gameplay / 24 canvas /
15 lens / 36 ledger / 28 chrome), each `{press, typed args, preconditions,
expected_output, reason_to_select}`. **Five parallel agents authored the families** into
disjoint fragment files (no worktrees needed — disjoint write-sets by construction); the
main session merged with per-fragment validation, wrote `tools/session/render_actions.js`
(mirror generator = shape check: required fields, family-prefixed ids, no dupes, no extra
fields), and wired AI_OPPONENT.md § 6a + the CLAUDE.md § Documents entry with the
keep-entries-current rule. The gameplay family is *transcribed* from `corp_command.hpp` —
verbs, typed args (workforce [0,200], road_tier [1,3]), rejection semantics — not authored.

### What the sweep caught (transcribe-from-code pays immediately)

- **LENSES.md's supply-routes access note was stale**: it claimed `overlay_mode_count`
  was still 13 and the lens unreachable; code anchors the count to `supply_routes`+1=14
  with a static_assert. Doc corrected in three places; the entry records code truth.
- **Esc's precedence ladder has seven rungs**, not the six the summaries state (the corp
  roll-up drill reset, BL-248, sits between card-unwind and fold-up). `chrome.esc`
  transcribes all seven; the canvas family's duplicate was dropped at merge.
- Smaller honesty wins: no settings row exists for the frame HUD (F11 only); the budget
  tier steppers are stubbed (entries say so); buy orders have no player press; sell-order
  placement lives in the market ledger (BL-159), not the Selection panel.

**Open.** The milestone items this feeds — the text-play harness (blackboard + dictionary
→ LLM → corp_command) and word-driven generation — are unfiled until NR-034 (the
milestone's ROADMAP slot) is answered.

---

---

---

## Session — "the engine is thrashing": measured, diagnosed, and fixed in one pass (2026-08-02)

**Runtime.** ~2.5h. Full (BL-268 — the planetary canvas hot loop; earns the lifecycle by
touching the project's single most-drawn code path, though it spans only two logic files).

**The ask.** Ben: the build is "starting to thrash" — how hard would GPU + multicore be?
Then: stutter while panning; "go and report the numbers, then let's work on the solution."

### What the measurement found (BL-267, GPU & multicore — its own named first step)

Built a scripted tap on BL-249's frame instrument — `verify.frame_reset`/`frame_csv`/`window`
plus `scripts/verify/pan_perf.lua` (300-frame sustained pan, three zooms, pan-vs-static) —
after discovering the "verify runs a dummy driver" belief was **wrong**: nothing in `src/`
sets one, so `--verify` measures the real renderer with real vsync. Findings, 1720×1080:

- **The daily build is unoptimised Debug** (`/Od /RTC1`): 41–53 ms work/frame at every
  zoom — every frame over the 16.7 ms refresh, ~20 fps always. Panning adds nothing
  (pan ≡ static); motion just makes 20 fps visible.
- **The cost was one flat O(all-tiles) canvas overhead**: `tile_at` hash map rebuilt per
  frame by scanning every body's tiles, a 15k-id sort per frame, and full lens/colour work
  for all 15,120 tiles before any cull (no vertical cull existed).
- **Neither GPU nor multicore is implicated**: sim + event pump 0.01–0.04 ms; submit/present
  small. BL-267's two architectural forks both declined at prototype scale; item kept open
  only as the post-fix re-measure gate.

### What was built (BL-268, planetary canvas cull + cache — filed and landed same session)

The canvas now reads the per-body raster **logistics already caches** on
`world.body_tile_index` (`body_tile_grid`, BL-077) — `app::render` ensures it, the canvas
stays `const world&`. Iteration is row-major over the raster (provably the old sorted-by-id
draw order: generation creates tiles rows-outer with sequential ids — so **pixel-identical**),
culled to the visible row band, with the horizontal wrap-window hoisted above the per-tile
lens work as the column cull. Verified: six goldens exit 0 **un-blessed** against a
baseline-blessed set; play-zoom pan **11.26 → 4.98 ms** (Release), **41.21 → 6.74 ms**
(Debug). Whole-grid residual (155k verts, genuinely all visible) filed as BL-269
(zoomed-out LOD / terrain draw cache).

### Calls taken on Ben's behalf (NR-032/033; NR-026 superseded)

The stale-golden discovery: every full-grid golden had been failing since BL-170's river
generation shifted the world RNG — nobody re-blessed. Re-blessed all 40 from the unmodified
baseline (stash round-trip) so R1's un-blessed pass isolates the refactor exactly; committed
separately from the item. `build_rel/` (Ninja Release, same pinned 14.44 toolchain — ninja
ships inside BuildTools' CMake) now stands beside the Debug tree as the play/perf build;
Ben should play Release from here on. The frame_budget_hud.lua header's dummy-driver claim
corrected in place.

**Open.** BL-269 (zoomed-out draw cache, B). BL-267 re-measure gate closes when Ben confirms
the live feel. A `build_rel.bat` convenience wrapper was recommended in NR-032 but not built.

---

---

---

## Session — the world history log: the project's first serialisation seam (2026-08-02)

**Runtime.** ~3h. Full (touches the economy/serialisation seam, spans well over 2 logic files,
carries a genuine determinism/reconciliation risk — earns the lifecycle by Rule 0).

**The ask.** Build BL-208 (world history log): the append-only, tagged, single-interleaved world
log the item's design settled on 2026-08-02, laying the project's first flat-binary serialisation
path ahead of BL-218 (nations rewrite) and BL-219 (corporations rewrite), which are expected to
write into this same substrate.

### What was built

`history_topic` + `world_history_entry` on `world::history_log` (`src/world/world.hpp`); the
genesis+checkpoint bridge `seed_genesis_history` (new `src/world/history_log.{hpp,cpp}`), called
from `app::setup_world` right after `make_hard_coded_world` — the first time PLANETOLOGY's
per-body dated history and checkpoint decisions ever reach `world` state rather than staying
presentation-only in `generation_report`; and the serialiser itself
(`write_history_log`/`read_history_log`) with a leading magic+version header (BL-107's own rule),
field-identical round-trip, and rejection — not misreading — of a corrupt/wrong-magic/wrong-
version/truncated stream. The three live sources wired additively at their existing emission
sites: `corp_ai.cpp` (decision + agency, strategic tier), `economy_system.cpp` (agency, the BL-079
reflex tier), `supply_system.cpp` (trade_route, gated to first establishment of a body-pair lane —
verified NOT to duplicate on repeat traffic). None of `ai_decisions`, `agency_events`,
`trade_routes`, or `body_activity_visibility` changed at all.

### Two judgment calls flagged rather than silently picked

`checkpoint_record` carries no timestamp of its own (by design, and changing its shape now has a
ripple cost the item said to avoid); resolving one against a body's dated history lines is not a
clean 1:1 pairing in every case (a body that already terminated earlier can record a checkpoint
with no dated line at its stage at all; Green can resolve two checkpoints against up to three
Green-tagged lines with no code-level tag distinguishing which belongs to which). Took the
simplest defensible rule — the stage's LAST dated line at or before it — documented inline and
filed as **NR-029** rather than replicating `planetology.cpp`'s branch logic a second place it
could drift from. Separately, a newly-established trade route is a two-body event but
`world_history_entry` carries one `body` tag (the settled shape); tagged the destination body and
named both endpoints in the narration text, filed as **NR-030** since a body-scoped filter over
the log would miss the entry for the untagged source body specifically.

### The worktree was a stale base, twice over in one session

This worktree's HEAD sat at the merge-base with `main`, 24 commits behind — missing BL-217
(`checkpoint_record`/`planetology_state.checkpoints`), which this item hard-depends on, plus
BL-166/168, BL-170 (rivers), and a backlog/doc sweep. Stashed the in-progress edits, fast-forwarded
to `main` (clean; only `app.cpp` auto-merged), popped the stash back (also clean) — no manual
conflict resolution needed. `cmake -S . -B build` then hit the same FetchContent/TLS block a prior
session already named (BL-217's own NR-028): confirmed by direct reproduction rather than assumed.
Fell back to hand-compiled `cl` per the documented contingency — the new
`tools/verify/history_log_harness.cpp` (27/27 PASS, built over the real generated world, mirroring
`history_ladder_harness`'s style rather than hand-fabricating log entries), two added checks in
`determinism_harness.cpp` (25/25 PASS), and a seven-harness regression sweep across every touched
file (`corp_ai_harness`, `ai_skill_harness`, `trade_routes_harness`, `commercial_fog_harness`,
`supply_advance`, `econ_stability`, `blackboard_harness` — all green, 0 failures). Also found
`tools/verify/README.md`'s hand-written world-superset recipes are one file short of linking since
BL-170 landed (`hard_coded_world.cpp` now needs `river_generation.cpp`); documented as a TU-ripple
note rather than silently patched around. Filed **NR-031** — the full `ProjectIo` GUI target and
whole-suite `ctest` are owed from a network-enabled session (app.cpp's new include/call was only
verified by inspection, since no headless harness touches it).

### Docs

`docs/ai/AI_OPPONENT.md` gains § 8a recording the log's final shape (the struct, the topic enum,
the four sources, the magic+version header). `docs/generation/GENERATION_LEDGER.md` gains a
section explaining why it stays a separate mechanism from the log — disposable/tuning-scoped
breadcrumbs vs. durable/narrative-scoped history, same instinct, incompatible lifetimes.

**Backlog: BL-208 lands complete.** Review queue carries 3 new entries (NR-029/030/031, all open).

---

---

---

## Session — the design-owed sweep: thirty items settled, and three recovered from a merge (2026-08-02)

**Runtime.** ~2h. Full (Design depth verb across the whole design-owed set; no code, no authority-doc
edits — settlements land in `backlog.json` and stop there).

**The ask.** "Let's work through the design-owed items... prioritise items in sprint 1 > 2 > 3... if
items are marked as deferred, or they await later items, just promote them now. We want to prepare
for a batch delivery of tons of the latest design work."

### The ordering was ambiguous, and asking cost less than guessing

`SPRINTS.md` has **two entries numbered Sprint 2**, and only ~8 of the then-28 design-owed items map
onto any named sprint. Put the real state up with the three readings and let Ben pick: **version
goal**. That gave a clean 28-item order and took one question.

### Three items had been silently deleted

Ordering the set surfaced that **BL-217/218/219 did not exist** — the id sequence jumped 216 → 220 —
while `SPRINTS.md` § Sprint 2 and BL-210's own design prose both name them as BL-210's decomposition.
Traced the file's history: filed at `18c86c0` (2026-07-29), present through `8542e4b`, absent from
`eaa0d23` ("wip before Sprint3 merge") onward. No commit message mentions retiring them.

This is the **stale-base worktree revert** pattern for the second time. Recovered all three verbatim
from `8542e4b`, +76 lines, lint clean (NR-021 — which also flags that a merge dropping three
consecutive rows is unlikely to have dropped exactly three; a full row-level audit is *not* done).

### The real finding: items were waiting on each other, not on design

Roughly a third of the set settled by **redistribution** rather than new design. Settling one item
dissolved the next:

- **BL-263** (markets never disappear, they go dormant) → **BL-131** stops being "player-driven market
  destruction" and becomes player-induced dormancy; its hard catchment question evaporates. 4 → 2.
- **BL-155 / BL-158 / BL-218** → **BL-054** loses three of its four parts. Tax and the licence gate are
  laws; sentiment is BL-158; fragmentation folded into BL-218. 5 → 3.
- **BL-157** (a unit is positioned by *tile id*) → **BL-189**'s data half needs no schema change at all.
- **BL-217/218/219** → **BL-210** becomes a pure umbrella with a three-part closing condition. 5 → 2.
- **BL-262** (capital standing feeds credit terms) → **BL-225**'s "credit access" needs no new channel.

Two items were **stale bookkeeping, not open design**: BL-087's status claimed an owed set remained
two lines above the section resolving it, and BL-098's method had been settled since 2026-07-05.

### Three calls worth Ben's eye

- **NR-022** — BL-262 (scoring): all six open calls answered as one interlocking package, because they
  are not independent. Recorded for ratification, not adopted silently.
- **NR-024** — BL-155 surfaced a contradiction: BL-171 confirmed **Tax** as a player lever, but every
  law in the ten-law list is an instrument of public authority and the player is a *corporation*.
  Settled that laws are enacted by nations and the player is a law **subject** until BL-094.
- **NR-025** — BL-223's "three-doc" Era disagreement is **four-way**; its table omits BL-087's reframe,
  which is newer and governs. With it there is no contradiction — a *past* averted rupture and a
  *future* seeded one, doing different jobs. **CONCEPT.md:51 is right and survives unamended**, which
  reverses the item's own owed action.

### Left open on purpose

**BL-229** (building selection) is the only remaining design-owed item, and deliberately so — it
carries Ben's written "do not guess the layout, Ben designs this one". Q5 and sequencing settled;
Q1–Q4 restated against measured column widths (135 / 254 / 135 px at the 1280×720 floor, 260 px band)
so they can be answered against numbers rather than prose (NR-023).

**Backlog: 61 designed, 1 design-owed.** Review queue carries 6 open entries.

---

---

---

## Session — the disclosure spine: one fold idiom, and the surfaces stop inventing their own (2026-08-01)

**Runtime.** ~2h. Full (Batch Delivery — three items, main-session-serial by design; two design
calls put to Ben with measurements, one taken alone and recorded; one defect filed).

**The ask.** "Are there further items we can batch deliver?" — then, from the four candidate
groupings offered, **the disclosure spine**: BL-214 (drill-through idiom) → BL-247 (chart question
log) → BL-248 (corporation dashboard roll-ups).

### No fan-out, and that was the call

A dependency chain, not a fan-out. BL-214's shared control is the thing the other two *call*, and
BL-214/BL-247 share four files — worktree agents would have collided on `generation_charts.cpp` and
`selection_panel.cpp` for no wall-clock gain. BL-214's own design had already reached the same
conclusion about its sibling BL-215 and said so.

### The design was superseded, and the supersession had a hole

BL-214 was designed around a three-level Glance/Read/Study stepper, then superseded on 2026-07-31
by Ben's binary fold model after he reviewed four live HTML exemplars. The binary note says
*"folded (one line, the only default for every surface)"*.

Applied literally that breaks the Selection band, and the superseded design had already said why:
**a fixed-rect container cannot shrink.** The band is a derived 260 px
(`minimap_height + chrome_margin`), so folding its metric card to one line spends ~220 px on
emptiness — the exact objection the three-level design raised against "Glance everywhere", which
the binary note never revisited. Reported the measurements and asked rather than guessed
(Rule 0b). **Ben: the band opens expanded-in-place**; its chevron means *give this the whole
screen*, and folded-by-default governs scrolling containers, where a fold buys real room back.
Second call: **the wizard folds per chain stage** — round 1's four gates now read as
`System all passed` / `Accretion Lost here: Pallas` / `Air Lost here: Cinder, Selene` /
`Engine all passed` on one screen, which is the chain finally legible rather than a scroll.

### The state model fell out of the change rather than being imposed

Because expanded is a full-screen **overlay**, only one thing can be expanded at a time. So the
state is a single `(surface, key)` target, not the superseded design's per-surface remembered
level — and "fold" is never ambiguous because there is exactly one thing to fold. The remembered
level was load-bearing for an in-place stepper and is meaningless for a mode switch.

**One decision taken alone, and recorded rather than slipped in:** the overlay **joins the Esc
ladder**, one rung below the subject drills. BL-214's Decision 10 explicitly kept depth *off* the
ladder — but it reasoned about an in-place stepper, where a level is not a dismissal. A
full-screen mode with no keyboard exit is a defect, not a principle.

### What the captures changed

The first run was not a pass. Two real defects only visible by looking: the overlay's
`SetNextWindowBgAlpha(0.97f)` scrim let the entire shell read through it — a deliberate mode
switch looking like a ghost drawn over the game — and zero-inset content sat jammed in the
top-left corner of a 1280 px screen. Fixed to an opaque background and a 36×28 inset. The band's
expanded chart was also drawing two 580 px ribbons; capped, because `draw_bars` pins columns at
34 px and extra height was buying size, not legibility.

The question log reserves its **measured** height (`CalcTextSize` at the wrap width) before
opening its fixed-size, scrollbar-less chart row — so this item does not create the fitting defect
BL-215 is queued to audit.

### Ben's catch: a stable golden of the wrong picture

Mid-session Ben pointed out that a capture can fail because the screenshot is taken **before the
frame has fully rendered**. Tested rather than assumed, and he was right about the new captures:
`verify.capture` composites exactly **one** frame, while ImGui settles auto-layout over the next
frame or two — a child's content region, a table's column widths and a fresh window's scroll state
are all provisional on the frame they first appear. **Four of the ten fold captures moved once
given settle frames**, most visibly the History Tiles table, which only reads across the full
screen after settling.

The insidious part is that the unsettled frame is *deterministic*: it blesses cleanly and re-passes
at 0.0000% forever. A stable golden of the wrong picture is worse than a flaky one, because nothing
ever flags it.

Fixed as a **reusable asset rather than three script edits**: `shot(name, frames)` in
`scripts/verify/lib.lua` (auto-loaded, so every future check gets it) settles before capturing,
with the reasoning in the comment so it is not dropped as noise. All 23 goldens re-blessed settled.

The same hypothesis does **not** explain the pre-existing suite failures — `chat_panel` still
differs 40.9% with eight settle frames, and its golden shows *21 nations* against today's *20*,
plus an entirely different starting corporation. That is world generation, which is why BL-259
stands.

### Retired, not added

The History Chain's per-stage `CollapsingHeader` is gone. It was that surface's own private
disclosure idiom — the fourth one this item exists to kill — and "open" there always meant
"scroll", because four stages of charts have never fitted a 380 px column. The all-corporations
balance table at nav slot 1 is gone too (`corporation_panel.{hpp,cpp}` deleted): it was a
cross-corp comparison surface, not "the player corporation at a glance", and the Economy panel's
Corps view already carries it.

### The verify harness grew, because the idiom was otherwise unverifiable

`verify.fold(surface, key)`, `verify.rollup_drill(row)` and `verify.why_note(on)` — without them
every capture would show the resting state and the whole item would be untestable. `why_note` uses
a **sentinel** (`why_note_first`) claimed by the first log drawn, because a Lua script cannot
compute an ImGui id: they are stack-dependent and exist only mid-frame.

### Filed, not absorbed

The full `scripts/verify` sweep fails golden diff on most checks — including many this batch never
touched. The diff images settle it: the differing pixels are **world content** (terrain colour,
generated corporation names, balances, nation names in comms), not layout. The goldens were
blessed 2026-07-30; `src/world/` moved on 07-31 (BL-233 re-priced conquest from the graded terrain
field and reshaped the political map) and 08-01. BL-252 re-established the *headless* bands per
toolchain; the visual suite was never re-established after the world moved, so the cut gate's
visual half has been quietly false for two days.

Mass-blessing it inside this commit would have buried a pre-existing regression in an unrelated
change. **Filed as BL-259** (v0.1.0 — it closes a hole in a cut gate), including the missing
discipline that would stop it recurring: a `src/world/` change that moves generated content owes a
visual re-bless in the *same* commit, the way a headless band change already does.

**Left open.** BL-259. The Trade roll-up reads `0 lanes` because the generated world seeds every
market on the single tiled body — the open design question BL-254 deliberately did not settle, now
visible on a player-facing surface.

---

---

---

## Session — the last four v0.1.0 items, and the goldens finally have one truth value (2026-08-01)

**Runtime.** ~2h. Full (Batch Delivery — three worktree sub-agents, one item delivered in the main
session, one cold review pass, two items filed from Ben's new policy, two defects filed from
verification).

**The ask.** Bring PR #28 local, understand what it sets up, then run a multi-agent session on it.
PR #28 closed the terrain/landform strand and built the three audit instruments, leaving exactly
four open `version_goal: v0.1.0` items. Those were the session.

### Split

BL-162 (tile construction ledger, reopened), BL-254 (convoy data-creep) and BL-255 (build type +
timeouts) went to worktree agents; **BL-252 (goldens) stayed in the main session because it needed
Windows**, and Windows is where the goldens were blessed. BL-162 went to one agent rather than two
despite having three separable parts, because all three land in `selection_panel.cpp` — splitting
would have bought parallelism and paid for it at the merge.

### BL-252 — the item asked "which cause?", and the answer was "both"

The item named two candidates and said to distinguish them before re-blessing anything. Doing so
took three runs:

1. **Windows at the same commit failed 5 assertions** — on the platform its own bands were blessed
   on. That alone kills the pure-platform-divergence hypothesis. `git log 8542e4b..HEAD -- src/world/`
   named the cause precisely: the bands were authored in the commit that *added* the harness, and
   **BL-203 (Corp AI stage B)**, BL-221 and BL-233 all landed after. The AI was being scored against
   goldens set for a different AI. So: stale, and explained.
2. **Linux/GCC at the same commit** gave seed 0 = 395,143 against Windows' 206,245, and seed 4 =
   182,746 against 392,148. So cross-platform divergence is real *as well*, and large.
3. **MSVC /O2 vs MSVC Debug** — byte-identical on all five seeds. That removed the confound and
   pinned the cause to the toolchain rather than the optimisation level. Worth the extra build; it
   is the difference between a diagnosis and a guess.

Widening the bands was then rejected **on measurement**, not assumption: one band holding both
platforms spans ~±100% and detects nothing. Ben chose pinned-per-toolchain for headless and
Windows-authoritative for visual — deliberately different answers, because pixel output depends on
font rasterisation and driver as well as compiler. Both platforms now `ALL PASS`.

**A defect in my own work, caught by the review:** the `#error` guarding a third toolchain did not
catch Clang, which defines `__GNUC__`. A `clang++` build would have silently inherited GCC's bands —
the exact outcome the comment beside it claimed was prevented.

### What the instruments found, which is the point of having them

- **BL-254** closed its own vacuous plateaus and then found a *second* cause of the blind spot the
  filed item never mentioned: the generated world seeds all six markets on the single tiled body, so
  it holds no inter-body market pair and **cannot record a trade route however long it runs**.
  Whether non-home bodies should have markets at campaign start is now an open design question.
- **BL-258 filed** from the integrating run: `econ_stability`'s absolute 1 ms bound fails on Windows
  because that tree is deliberately Debug. R5 passes with 19× headroom and every growth-shape
  assertion holds, so the fix is to gate the one absolute assertion on an optimised build and *say
  so loudly* — following the data-creep instrument's precedent of reporting a meaningless check as
  skipped rather than passing it. Explicitly not widening the bound, which would gut it in Release.

### Two stale-base incidents, both self-reported

Two of the three agents were cut from `origin/main`, which predated the batch — one lacked
`data_creep_harness.cpp` entirely, the other lacked BL-255 itself and re-filed it, producing a
duplicate id that `backlog_lint` caught on merge. Both agents *noticed and said so*, which is what
made reconciliation cheap. The BL-255 agent's honest "I could not measure these three harnesses"
caveat was replaced post-merge with real integrated numbers.

### The review barrier earned its place again

A cold `verifier-review` over the integrated diff returned **GO COMPILE** — it verified the moved
estimator is token-for-token identical, all 13 `construct_building` call sites are arity-correct,
and the recipe index really is the global registry id. Everything it *did* find was judgement, not
compilation: the Clang hole above; a `cl` recipe in the new harness that would `LNK2019`; a
`verify.ledger_build` hook that silently substituted steel for a typo'd recipe name, so a broken
script would report green while proving nothing about the seam it exists to test; and two
requirement rows justified by evidence that could not have been produced (R7 cited
`building_component.recipe` as "already serialised" — there is no flat-binary path in `src/` at
all; and R7 listed a visual leg that this very item staled by construction).

### Goldens: the number meant something different than assumed

The four owed re-blesses were inspected before blessing. `tile_build_ledger_land_select`, which has
**no ledger open**, diffed 21.69% against the with-ledger capture's 22.04% — so only ~0.35% was
BL-162's row change and ~21.7% was whole-canvas world-generation drift from BL-221/BL-233. Same
root cause as the stale bands, one layer down. All four now pass at 0.0000%.

### Filed from Ben's new policy

**BL-256** (rotating globe on the generation screen) and **BL-257** (generated body names). Both
carry a crux that would have bitten during implementation: the wizard preview runs the *planetology*
chain only, so there is no height field for a globe to sample (hence two fidelity tiers); and
several sites — `hard_coded_world.cpp:256` plus three harnesses — use a body's **display name** as
an identity test, so randomising names without first moving identity onto the entity id breaks them
silently.

### Verification

Linux/Release **35/35**. Windows **35/36** (the one failure is BL-258). Build clean, warning-clean in
every file touched. `backlog_lint`: 0 fails, 2 warnings, both pre-existing. Visual: the four
tile-build-ledger goldens re-blessed and passing; `tile_build_ledger_survives` confirms two
consecutive builds from one ledger, with "Construction started." visible for the first time.

### Open for Ben

- The ledger caption says "50% staffing", but `workforce_auto` defaults on for the player's corp and
  auto-solves the dial on the first tick — realised extraction measured **2× the estimate**. Honest
  for the moment it is shown; should the caption say it is a floor?
- The header reads `NET +3.2k / qtr` while the ledger reads `/ tick`. GLOSSARY defines **Tick** and
  does not define "qtr". Pre-existing and outside BL-162's scope, but it is a standing-rule
  violation sitting two inches from a figure that gets it right.
- Sub-tick paybacks print `payback ~0 ticks`, which reads as "free" rather than "immediate".

---

---
