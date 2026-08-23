// review_queue.js — the answer form for what SURVIVED the 2026-08-23 review-queue
// purge. Companion set to questions.js (the design register); rendered by build.js:
//
//   node tools/session/register/build.js review_queue.html review_queue.js
//
// The register asks what the DOCUMENTS are short of. This asks what the CODE, the
// harnesses and the method files are short of — the residue of 240 open entries once
// every one referring to a purged backlog item (ids <= BL-568) was retired.
//
// Each question names the NR entries it settles. Answering it resolves them.
module.exports = {
meta:{
  title:'Io Review Queue',
  eyebrow:'Project Io &middot; 14 open calls &middot; 23 August 2026',
  headline:'What the queue still holds after the purge',
  lead:'240 open entries went into the sweep and 17 came out. 206 were retired with the backlog they referred to; 16 were answered by reading the docs and the tree. These are the ones that are genuinely yours.',
  store:'io-review-queue-v1',
  stamp:'Generated from the surviving NEEDS_REVIEW entries, 23 August 2026.',
},
sections:[

{doc:'HARNESS', file:'tools/verify/ &middot; .claude/skills/verifier-headless/', title:'Harness truth', blurb:'Four calls about whether a green harness means anything. Every one was raised by a check that lied — or by a red row that is not a regression.', qs:[

 {n:1, ids:['NR-548'], q:'What happens to <code>spectator_determinism</code>&rsquo;s golden, which is MSVC-derived and reds under g++?',
  ctx:'Verified against the tree this date. <code>spectator_determinism.cpp:119</code> says in its own comment that the golden is <b>toolchain-specific</b> — &ldquo;float clearing arithmetic differs under g++&rdquo; — and R2 at line 582 asserts it <b>unconditionally</b>. So every cloud or Linux session opens on one red row that is not a regression, and the honest reading of a red row is the whole value of the harness. The standing-rules half of this is already closed: <code>io-standing-rules.md</code> names <code>3CBAD1D44EE71EDE</code> as the value <i>at the time</i> and sends the reader to the harness provenance log.',
  blocks:'Nothing. But it is the first red every remote session sees, and it teaches new sessions to discount reds.',
  opts:['Guard R2 at compile time — assert the golden only on the toolchain it was blessed from, and print SKIPPED elsewhere with the reason','Carry one golden per toolchain, both blessed and both asserted','Replace R2&rsquo;s byte-identity with a same-run A/B: spectated vs unspectated in one process, which needs no golden at all','Leave it red and rely on the provenance log'], rec:2,
  freeform:'If you want the A/B, say whether the pinned golden also stays as a second row.'},

 {n:2, ids:['NR-558'], q:'How does a headless harness measure the configuration the game actually ships?',
  ctx:'<code>make_hard_coded_world</code> reads two Lua-authored inputs a headless harness cannot load — <code>scripts/works.lua</code> (the Era&nbsp;&minus;1 works table) and <code>scripts/world_gen.lua</code> — because the four translation units excluded from a headless build are exactly the Lua ones. The consequence is concrete: <code>history_sim.cpp</code> gates <code>build_work</code> on a non-empty works registry, so <b>the harness scores a verb set the game does not have</b>. This is the same failure mode BL-462 named — a check that looks like it measures the product and does not.',
  blocks:'Any honest measurement of the Era &minus;1 sim, which is where the war-rate question lives.',
  opts:['Extend the live-Lua pattern (tier_margin, player_seed_sweep already do it) to the harnesses that need it','Re-author the two files as C++ defaults so a headless build has them','Accept the divergence and make every affected harness print, in its banner, which configuration it measured'], rec:0},

 {n:3, ids:['NR-547'], q:'Is mutation-proving a stated step for a new harness row?',
  ctx:'Sprint N1 ran three implementing agents, each writing the code <b>and</b> its harness. All three harnesses passed — 17/17, 39/39, 25/25 — and <b>two of the three subjects were unsound</b>. Not one defect was found by reading; every one was found by mutating content or turning on a sanitizer. Two further instances folded in here: a mutation probe that passed because a fallback masked it (NR-488), and an 81/81 verification that was hollow because the harness segfaulted mid-run and two cases never executed (NR-425).',
  opts:['Required on every new harness row','Required on rule and arithmetic rows only — the cheap regression rows are already fine','Left to judgement, with the three instances written into DEVELOPMENT_PRACTICES.md as the argument'], rec:1},

 {n:4, ids:['NR-402','NR-259'], q:'Should a harness be able to join the ctest gate without anyone choosing its timeout?',
  ctx:'The CMake glob registers <code>tools/verify/*.cpp</code> automatically, so a new harness enters the routine gate silently at <code>IO_TEST_TIMEOUT_DEFAULT</code> (60&nbsp;s). Verified today: <code>road_reach_census</code> and <code>rival_military_seeding_harness</code> are in <b>neither</b> <code>IO_TEST_LONG_HARNESSES</code> nor <code>IO_TEST_SWEEP_HARNESSES</code>, and both still exist. This is exactly how <code>player_seed_sweep</code> spent its whole life red — a 60&nbsp;s timeout on a 69&nbsp;s tool, invisible because a ctest Timeout looks like nothing.',
  opts:['Move the two into IO_TEST_LONG_HARNESSES now, and add &ldquo;run it once under ctest&rdquo; to the verifier-headless checklist','Just move the two — the checklist is ceremony','Make the glob refuse an unlisted harness, so every new one names its tier deliberately'], rec:2},
]},

{doc:'REACH', file:'src/core/verify_api.cpp &middot; docs/development/DELIVERY.md', title:'What the harness cannot reach', blurb:'Three gaps between what a check can drive and what a requirement asks for. One of them gates a Sprint 16 item.', qs:[

 {n:5, ids:['NR-345'], q:'How does a script select a <b>unit</b>?',
  ctx:'The press half of this is <b>closed</b> — BL-521 landed <code>verify.click</code> / <code>click_tile</code> / <code>hover</code>, and <code>click_injection.lua</code> is the standing proof (that is what settled NR-424). The selection half is not. Verified today, the bindings are <code>select_tile</code>, <code>select_province</code>, <code>select_battle</code>, <code>select_building</code>, <code>select_corp</code>, <code>select_body</code> — <b>there is no <code>select_unit</code></b>, and the unit &rarr; building &rarr; tile click cycle exists only in live mouse handling. So the Selection unit card cannot be reached headless.',
  blocks:'BL-575 (unit marker and march UI), whose design ends on &ldquo;open the app, click a unit, march it, watch it move&rdquo;.',
  opts:['Add verify.select_unit as part of BL-575 — it is one binding and the item needs it','Add a generic verify.select(kind, ...) and retire the six narrow bindings over time','Leave it: the unit press stays a human live check, as the standing rule requires anyway'], rec:0},

 {n:6, ids:['NR-392'], q:'How does a worktree agent build a harness?',
  ctx:'Raised as <b>novel-work</b>, and nothing in the reading list owns the question. A fresh worktree has no configured build tree, and a CMake configure pulls SDL and Lua over FetchContent — not worth paying for one harness. The agent wrote <code>build_gen_harness.bat</code> at the repo root, globbing <code>src/world/*.cpp</code> minus the four sol2 TUs exactly as <code>io_world_obj</code> does. It works, and it is an unowned file authored by a sub-agent. NR-264 established the same recipe for Linux.',
  opts:['Adopt it: one committed cross-platform script covering both the .bat and the NR-264 Linux recipe, named in the verifier-headless skill','Keep it ad hoc — an agent that needs a harness build writes one','Solve it upstream instead: a CMake preset that skips FetchContent for world-only targets'], rec:0},

 {n:7, ids:['NR-480'], q:'Where does the worktree-base check live?',
  ctx:'Recorded three times in three weeks. A fanned-out agent gets its worktree at the commit the <b>session</b> started from, not at the branch head — one agent was five commits behind, and an earlier one built an entire item against an enum that had already been deleted. Every instance was caught by an agent noticing on its own, which is not a mechanism.',
  opts:['Add it to DELIVERY.md &sect; Sub-agents, so it is not a thing each brief has to remember','A line in the brief template only','Fix it at the tool level if the worktree base can be chosen, and drop the instruction'], rec:0},
]},

{doc:'CODE', file:'src/world/ &middot; src/ui/ &middot; src/main.cpp', title:'Live gaps no Sprint 16 item owns', blurb:'Three things that are in the tree, are reachable by a player or a session, and belong to nothing on the board.', qs:[

 {n:8, ids:['NR-400'], q:'The import tariff has no authoring path — does it get one, or go?',
  ctx:'Re-confirmed by grep this date. <code>law_effect_kind::import_tariff</code> appears only in <code>law.{hpp,cpp}</code>, in <code>market_clearing.cpp</code> (the consumer) and in <code>world_save.cpp</code> (the enum bound). <code>hard_coded_world.cpp:1217</code> seeds prototype laws and seeds <b>only the extraction levy</b>. No corp verb, no UI control, no generation path enacts a tariff — so <code>any_import_tariff_enacted</code> is permanently false in a played campaign and the whole tariff pass in market clearing is <b>unreachable code</b>.',
  opts:['File it: a nation enacts a tariff, which is the 2026-08-18 nation grant doing its first piece of work','Delete the tariff pass until something can enact one — unreachable code that looks implemented is worse than absent','Leave it, and say so in NATIONS.md so the next reader is not misled'], rec:0},

 {n:9, ids:['NR-256'], q:'<code>--autostart-play</code> terminates unattended and nobody has established why.',
  ctx:'It works interactively — you have used it and reported on what you saw. Launched unattended from a background shell it has ended on its own three times, at roughly 20&nbsp;s, 34&nbsp;s and ~60&nbsp;s: <b>exit code 0, no crash log, no exception</b>, output simply stopping after the warm-start. The proposed diagnosis is one printf on each of the three exit paths, then one launch from a real terminal and one from a background shell.',
  opts:['Instrument the three exit paths — it is a few lines and it distinguishes the two hypotheses in one run each','Leave it: the flag is for a human at the app, and a human at the app sees it work','Drop the flag and drive the app through --verify scripts instead'], rec:0},

 {n:10, ids:['NR-248','NR-249'], q:'The Profitability page draws a placeholder series. Does the player get told?',
  ctx:'Two entries, one subject. <code>building_profit</code> tracks four numbers and there is <b>no per-building profit history anywhere in the simulation</b>, so the page&rsquo;s &ldquo;Net, 6&nbsp;mo.&rdquo; chart is a smooth deterministic series anchored to the live estimate — the same honest-placeholder idiom the Workforce trend graph already used. The Revenue/Expenses split is as fine as the data gets: one pooled revenue figure the pooled market resists attributing per building.',
  opts:['Record per-building profit history — then both graphs become real, and it is one item covering both','Label it on screen as an estimate until history exists','Remove the chart until there is something to chart','Leave it — a plausible-looking line the player reads as history is a small lie, and small lies are the point of this queue'], rec:1},
]},

{doc:'METHOD', file:'docs/development/', title:'Stores and method', blurb:'Four calls about the way the project works on itself, including the sweep that produced this form.', qs:[

 {n:11, ids:['NR-513'], q:'What is <code>PHANTOMS.md</code>&rsquo;s lifecycle?',
  ctx:'Raised as <b>novel-work</b>. It is a new document class — a scan output that points at design sessions — and nothing in the corpus owns &ldquo;a list of things with no owner&rdquo;. It overlaps three existing stores: NEEDS_REVIEW (open calls), backlog.json (work), and MILITARY.md &sect; What is absent, which is the per-doc pattern this file generalises. It is written as transient and <b>nothing says who prunes it or when</b>. Still 322 lines.',
  opts:['Keep it as a transient scan output, pruned as the sessions it points at land','Dissolve it into per-doc &ldquo;What is absent&rdquo; sections and make the scan a periodic check rather than a document','Keep it and give it a query tool if it grows'], rec:1,
  freeform:'If option 2, say whether the file goes now or once each subject has an owner.'},

 {n:12, ids:['NR-556'], q:'Should authoring a backlog item require checking for one that already owns the subject?',
  ctx:'A duplicate item was written for a defect a priority-A item already owned, naming the same divergence at the same line numbers. The existing rule covers <b>id</b> allocation (<code>next_id.js</code>) and not <b>subject</b> collision. Cheap either way: <code>backlog_query.js --grep</code> already answers it in a second.',
  opts:['A line in DELIVERY.md before authoring: grep the backlog for the subject first','A lint check — backlog_lint flags near-duplicate titles or overlapping file scopes','Leave it: the cost of the occasional duplicate is lower than the ceremony'], rec:0},

 {n:13, ids:['NR-557'], q:'Does &ldquo;generation hands a harness its own arguments back&rdquo; get named as a pattern?',
  ctx:'To let a harness reproduce the Era&nbsp;&minus;1 exactly, <code>make_hard_coded_world</code> gained an optional out-parameter capturing the arguments the sim was <i>actually</i> called with. Nothing in TILE_GENERATION.md, GENERATION_LEDGER.md or DEVELOPMENT_PRACTICES.md owns this shape, and the two nearby precedents are different things — <code>generation_report</code> is presentational, the ledger is a why-surface. The argument for naming it is BL-462&rsquo;s: <b>two constructions of one call will drift</b>, and that applies to every generation pass.',
  opts:['Name it in DEVELOPMENT_PRACTICES.md — it costs a paragraph and it generalises','Leave it as a one-off on this call','Generalise it now: every generation entry point gains the same out-parameter'], rec:0},

 {n:14, ids:['NR-576'], q:'The sweep itself: two calls were taken on your behalf.',
  ctx:'<b>The survivor test.</b> Read literally, &ldquo;refers to a purged item&rdquo; would have deleted entries that merely <i>cite</i> a retired item as provenance while asking about a live doc, harness or method file. The test used instead is whether the <b>entry&rsquo;s subject</b> survives — so NR-400 stays although the item that surfaced it is gone, and the AI cost-side calibration entries go although the defect was real, because the work they belong to is retired. <b>Archive rather than delete.</b> The instruction said delete; the 222 removed entries were frozen verbatim into <code>archive/needs-review-purged-2026-08-23.json</code>, each with a resolution saying why, mirroring what the backlog purge did an hour earlier.',
  opts:['Accept both','Tighter — retire some of these 14 as well','Looser — some retired entries should have survived, and they come back out of the archive verbatim'], rec:0,
  freeform:'If tighter or looser, name the entries.'},
]},

]};
