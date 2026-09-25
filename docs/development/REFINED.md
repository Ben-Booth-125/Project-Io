# REFINED — active worklist

## Sprint 47 — one history, told through the rounds (opened 2026-09-24)

**Goal.** The wizard reads as one history: the world is built once at the Life gate and moves
forward through every round with nothing recomputed; rerolls are per stage; round 6 absorbs
Begin's work and closes on the map the campaign opens on; a realm keeps its colour, shade and name
from the Empires round to the seat card; each round carries its flair (arrows, hard borders,
fleets, works chartered). The rulings record is `drafts/sprint-47-rulings.md`; the sprint row in
`sprints.json` carries the waves, the risk and the done-when. Items are promoted here as each lane
opens; a lane's tasks are written by the main session before its agent is briefed.

**Lanes, by file (keep them disjoint):**
- **Generation seams** — `hard_coded_world.*`, `era_minus_one.cpp`, `world_gen_config.hpp`,
  `startup_screens.cpp` (the worker/slots only): BL-1083 (one seed per span) then BL-1084 (the
  world built once and moved). Serial; the cursor proof gates every UI lane below.
- **Core / Begin** — `app.cpp`, `app.hpp`, new `world/finish_campaign_world.*`,
  `world/campaign_settle.*`, `harness_params.hpp`: BL-1085 (Begin retired into round six),
  BL-1108 (quit-during-build crash, Light). Then BL-1086 (search inside generation, stretch).
- **World-movers** — `history_sim.*` (the subjection block; the end-of-run block), `era_band.hpp`,
  `law.cpp`: BL-1096 (purchase verb) + BL-1097 (sea legs recorded) in one lane; BL-1101 (band from
  history); BL-1102 (tariff posture derived). Then BL-1098 (lane stamp, stretch).
- **Identity (UI)** — `history_lapse.*`, `presentation.cpp`, `nation_generation.*`,
  `seat_screen.cpp`, `tile_inspector.cpp`: BL-1087 (colour) → BL-1088 (name) → BL-1089 (nation);
  BL-1090 (hard borders), BL-1094 (marks), BL-1106 (names and voice) beside them.
- **Rounds' flair (UI + record kinds)** — `history_lapse.*`, `era_timelapse.hpp`,
  `save_game.cpp`, `generation_charts.cpp`, `settlement.*`: BL-1091 (from Life to people) →
  BL-1092 (routes and splits); BL-1095 (fleets and ties); BL-1099 (works chartered events);
  BL-1100 (rung crossing); BL-1104 (Culture record saved); BL-1107 (ground profile, stretch).

**Gates.** Every merge: Release build, `world_determinism` twice (bit-identical), `save_roundtrip`,
`save_envelope_roundtrip`, the item's harnesses, a cold `code-reviewer` pass, then Ben's live
click for any UI item. World-movers land behind the sprint's ONE re-bless, taken once after the
last of them. Save versions: one envelope bump and one world bump for the whole sprint, each
claimed by the first lane that needs it (`next_save_version.js --kind envelope|world --claim`).

**Owed live clicks carried in:** BL-1068, BL-1072, BL-1073, BL-1076, BL-1080 (round 6, the wait,
Begin adopts, select company, industry heat). BL-1078 closes with BL-1085.

### Wave 0, lane G (generation seams) — BL-1083 (one seed per span). Group `span-seeds-per-round`.

- [ ] T1 `world_params::span_seed[4]` after `era_seed` (hard_coded_world.hpp:40-63), zero by default,
  the fold rule in its comment; the stale `era_seed` comment rewritten. provides: the field.
  consumes: nothing. (R1)
- [ ] T2 each span folds ONLY its own counter, additively with an odd multiplier so zero is neutral:
  `era_minus_one_sim_seed` / `exploration_sim_seed` / `industrialisation_sim_seed`
  (era_minus_one.cpp:315, :382, :446) take span_seed[1..3]; the `run_settlement` seed
  (hard_coded_world.cpp:919) folds span_seed[0]. provides: the folds. consumes: T1. (R1, R2, R3)
- [ ] T3 `same_world_params` compares the four (app.cpp:514-534); the reroll handler bumps
  `span_seed[lapse_index]` instead of `era_seed` (startup_screens.cpp:1647); invalidation unchanged.
  consumes: T1. (R4)
- [ ] T4 the save envelope writes and reads the four under a claimed envelope version
  (`next_save_version.js --kind envelope --claim "BL-1083 span seeds"`; save_game.cpp:88-111);
  `save_roundtrip` and `save_envelope_roundtrip` green. consumes: T1. (R4)
- [ ] T5 the turbulence lean's own dirty path: invalidate Empires onward and relaunch round 4 at
  once, no `m_wiz_dirty` (startup_screens.cpp:1573-1585; app.hpp:287-321). (R5)
- [ ] T6 a headless check that span_seed[2]=1 leaves the migration and Empires records identical and
  changes Exploration, and span_seed[0]=1 changes the migration (a `tools/verify` harness or a
  `--verify` script; name it in the group). (R2, R3)
- [ ] T7 seed library entries and harness fixtures that set `era_seed` still build; STARTUP.md's two
  sections re-read against the built behaviour; Release build; `world_determinism` twice; Ben's
  live click (R6, R7).

### Wave 0, lane C (core / Begin) — BL-1085 (Begin retired into round six). Group `begin-retired-into-round-six`.

- [x] T1 promote the settle tick body from `tools/verify/harness_params.hpp:569-639` into
  `src/world/campaign_settle.{hpp,cpp}` (`run_settle_tick`, `run_settle`); `app::step_economy`'s
  world half and the harness mirror call it; `begin_adopts_check.js` PASS and the 16-seed
  `--digest-check` unchanged before anything else moves. provides: the one settle function. (R1)
  DONE 2026-09-24: `run_settle_tick` / `run_settle` (k_campaign_settle_ticks = 12); step_economy,
  run_app_validation_tick, run_app_live_window all call it; begin_adopts_check 12/12 PASS. The
  `--digest-check` is OWED to the main session (the brief bars player_seed_sweep in a lane).
- [x] T2 `src/world/finish_campaign_world.{hpp,cpp}`: the world-only setup halves, the band (from the
  epoch until BL-1101 lands), `assign_default_recipes`, the stockpile budget, the search, the winner
  apply, the recipe pass, the twelve-tick settle; returns the winner score and the charter report.
  consumes: T1. (R2) DONE 2026-09-24: `finish_campaign_world(w, report, reg, params, cfg, progress)`
  — the report is a sixth argument (seed_genesis_history reads it); `campaign_band_from_epoch` is
  the one band body BL-1101 replaces; `campaign_search_params` is the search's one keying.
- [x] T3 the registry loaded from Lua on the main thread before the round-6 launch (the `m_works`
  pattern) and a copy handed to the worker; `wizard_world_cache` carries the banded copy, the score
  and the report; Begin moves them out. consumes: T2. (R3) DONE 2026-09-24: `load_recipe_registry`
  + `slot->registry = m_registry` at both launches; seat_pick_check 13/13 PASS on 19575BBC3B912558.
- [x] T4 the round-6 lambda calls `finish_campaign_world` after `make_hard_coded_world`; the label
  table widened past 16 with "Searching the landscape" / "Proving the field" and measured weights
  (re-measure the search in Release first and write the number into the item). consumes: T2. (R6)
  DONE 2026-09-24: labels 16/17, `generation_step_slot_count` 18, `weight_after` on the sink;
  `gen_step_costs --finish 0 28` (Release): search 28,801 / 12,623 ms, settle 113,922 / 68,832 ms
  → weights 20,000 / 90,000; the search reports per evaluation on the inner bar. ONE settle tick
  (tick 1) runs 44-62 s on seed 0 — the bar holds still through it (NEEDS_REVIEW, main session).
- [x] T5 Begin: waits on round 6's future (never a second build); adopt path moves everything;
  the cold worker calls the same function; the twelve ticks' presentation half dropped;
  `m_econ_steps = 12`; STARTUP.md § Handoff re-read against it. consumes: T3. (R4, R5)
  DONE 2026-09-24: `m_begin_waits_round6`, `try_adopt_wizard_world`, `launch_cold_build`; the
  balance series is seeded from the seated corp's filed returns in finish_new_game; the validation
  members and the `[validation phases]` print are gone; the doc reads as built.
- [x] T6 `run_serve` adopts; `run_verify` unchanged; `--autostart-windowed` waits on the future;
  `harness_params.hpp` mirrors call the promoted functions; `begin_adopts_check.js`'s log proof
  re-pointed; `seat_pick_check.js` PASS. consumes: T5. (R7) DONE 2026-09-24: `--serve --ticks 1`
  prints state_hash=19575BBC3B912558 == `--autostart`'s; the windowed walk now launches each lapse
  round on arrival (it never had) and logs "round 6 is still building: waiting" → "adopted".
- [ ] T7 Release build; `world_determinism` twice; both check scripts; Ben's live click — Begin after
  the playback opens the seat with no freeze. (R8) BL-1078 closes here. 2026-09-24: Release build
  green; world_determinism 17/17 twice, digests identical; save_roundtrip 63/0;
  save_envelope_roundtrip 34/0; both scripts PASS. OWES Ben's live click (R8).

### Wave 0, lane W1 (world-movers, the subjection block) — BL-1096 (purchase verb) then BL-1097 (sea legs recorded). Groups `purchase-verb`, `sea-legs-recorded`.

- [ ] T1 `purchase_price_q(native_seat, params)` beside `choose_subjection_native`; params
  `subjection_purchase_rate_q` / `subjection_purchase_floor` on `history_sim_params`, validated,
  zero-disabled. (1096 R1, R2)
- [ ] T2 the fork inside the BL-934 block (history_sim.cpp:3817-3918): BUY when the native seat
  has a port window and the price is affordable — debit/credit, `subject_kind = 0`, no grudge,
  shares untouched, `province_bought` noted, counters; else TAKE as today; the header comment at
  history_sim.hpp:2718-2731 rewritten. (1096 R1, R3, R4)
- [ ] T3 `province_bought` on `lapse_event_kind` (append-only) and its ticker caption; the
  envelope bump claimed (`next_save_version.js --kind envelope --claim "BL-1096/1097 kinds"`) or
  shared with BL-1083's claim if it landed first. (1096 R4)
- [ ] T4 exploration_sim_harness: the price-fork case and the control equality; exploration_sweep:
  the bought/taken split; the 16-seed reading written into the item. (1096 R2, R3)
- [ ] T5 `sea_leg{a,b,uses}` on the sim state and the two span outputs (folded, sorted, validated);
  `note_sea_leg` at the three sites; `sea_lane_opened` at `sea_lane_tier1_uses` (default 4).
  (1097 R1, R2)
- [ ] T6 round 5's lane bake and draw, its own water layer; the sweep column; EXPLORATION.md re-read
  against the build. (1097 R3, R4, R5)
- [ ] T7 Release build; `world_determinism` twice; `save_envelope_roundtrip`;
  `industrialisation_sim_harness --fidelity`; cold review; Ben's live click on the lane line.

### Wave 0, lane W2 (world-movers, the band) — BL-1101 (band from history). Group `band-from-history`.

- [ ] T1 derive `world::campaign_band` at the Industrialisation fold (after the validator,
  hard_coded_world.cpp:~1571) from the polities' materials capacity; persist it (world save
  version claimed, `--kind world --claim "BL-1101 campaign_band"`). (R1)
- [ ] T2 `load_economy` and `load_game_from` apply the world's band; retire
  `industrial_band_from_year` / `era_band_for_epoch`; the ten harness call sites read the world's
  band through `harness_params`. Touch app.cpp in the two application sites only (lane C is in
  the same file). (R2)
- [ ] T3 history_sweep column: polities crossed / max materials capacity / derived band per seed
  across the 16 curated seeds; the reading written into the item. (R3)
- [ ] T4 `begin_adopts_check.js` PASS; `--epoch 0` vs `--epoch 1960` opening hashes reported;
  `save_roundtrip`; Release build; `world_determinism` twice; the docs re-read. (R4, R5)

### Wave 0, lane W3 (world-movers, the end-of-run block) — BL-1102 (tariff posture derived). Group `tariff-posture-derived`.

- [ ] T1 the stated formula for `protection_q` from scarcity_q, trade_flows and culture_preference
  at the end-of-run block (history_sim.cpp:~7857), independent of `boundary_year`; the comment and
  INDUSTRIALISATION.md § What crosses into play carry the arithmetic. (R1)
- [ ] T2 `seed_national_tariffs` writes at least one LAW-IMPORT-TARIFF-* law on the verify seed;
  the posture spread across the 16 seeds reported and written into the item. (R2, R3)
- [ ] T3 Release build; `world_determinism` twice; digests recorded for the re-bless. (R4)

### Wave 0, Light — BL-1108 (quit during a build crashes). Folded into lane C (same file).

### Wave 1 (opens when BL-1083 lands), lane I1 (identity) — BL-1087 (colour) → BL-1088 (name) → BL-1089 (nation). Groups `realm-keeps-its-colour`, `realm-keeps-its-name`, `realm-becomes-nation`.

- [ ] T1 (1087) pinned slots on the record and in `assign_polity_colours` (history_lapse.cpp:~640-760):
  pins set at launch from the predecessor's slots and re-pinned at its landing; greedy for the
  rest; the clash and dead-slot rules; a history_sweep column counting pinned clashes on the 16
  seeds, read before the rules are fixed. (1087 R1, R5)
- [ ] T2 (1087) the lineage palette built for every lapse (startup_screens.cpp:~181-224); polity hue
  from the founding family's wedge with a greedy within-wedge offset (presentation.cpp:~416-447
  replaced); the polity's culture from the founded region's plurality; the CVD check recorded.
  (1087 R2, R5)
- [ ] T3 (1087) the culture base under the fill on rounds 4-6 from `culture_changes` plurality; the
  carry fade lands on it; the shade ratchet (civilisation_formed / ROSE) carried by id. (1087 R3, R4)
- [ ] T4 (1088) `polity::name` coined at `founded` via `coin_lexicon` over the founding region's
  speech; a `polity_name` table on the record beside `region_name`, carried by id; board and ticker
  print it. (1088 R1, R2)
- [ ] T5 (1088) the UI-side capital-at-year fold (founded + capital_moved) placing the seat dot and
  the 1200 burst; the resume re-emit → a ticker-silent `inherited` kind (append-only; the envelope
  bump); 'A people settle at X' for an ownerless founding. (1088 R3, R4, R5)
- [ ] T6 (1089) `polity_names` on `nation_params`; Pass 5 follows the fold representative through
  the Pass 2c merge and inherits the realm's name; merge rule A; ownerless ground coined; the
  headless check that every nation with a founding realm carries its name. (1089 R1)
- [ ] T7 (1089) the per-world nation → slot table set at Begin/load from the saved report and read in
  `nation_colour` (hash as fallback): the border band, the seat map, the carve and the Ages view
  agree; the seat card lists absorbed realms and cites the city's region; the nation-count line.
  (1089 R2, R3)
- [ ] T8 Release build; `world_determinism` twice; `save_envelope_roundtrip`; `begin_adopts_check.js`;
  the verify captures named in the groups; cold review; Ben's live click across the 1200/1660 seams
  and into the seat. (all R6/R5)

### Wave 1, lane I2 (frontier and marks) — BL-1090 (hard borders) then BL-1094 (marks). Groups `hard-borders-by-people-share`, `marks-that-earn-their-place`.

- [x] T1 (1090) history_sweep column: the people-share distribution per world on the 16 seeds and the
  count a candidate threshold bolds; the threshold and hysteresis fixed from it and written into the
  item. (1090 R1) DONE 2026-09-25: the BL-1090 block (distribution, five candidates, dips, hysteresis
  margins, and the PINNED pair walked as the map walks it); 16-seed reading in the item's design —
  20%+ bolds nothing anywhere, 15% one realm on one world, 10% on 6 of 16 worlds and none on the
  city-state worlds; deepest single-step dip 39‰. Pinned on 100 / off 60 (history_lapse.hpp).
- [x] T2 (1090) the per-owner hard flag with hysteresis; both frontier passes (history_lapse.cpp:~979-985,
  ~1000-1013) draw 2 px dark + 1 px inner stroke in the realm's colour; the board bolds; the flag
  carries by id into round 5. (1090 R2) DONE 2026-09-25: `lapse_hard_walk` bakes a per-step bitmap
  at finish (seeded from `hard_carry`), both passes read it per edge, the board faux-bolds the seat
  and share (one face only); the carry is set at launch, at landing and on the `--verify` adopted
  path (startup_screens.cpp, beside the slot inheritance). Capture history_lapse_hard.lua: at
  1000 CE on seed 0, Zeihei Meize (10.5%) and Tume Memma (9.0%, held by hysteresis) draw hard and
  bold; Ruke Rate at 9.5% stays plain; round 5 opens at 1200 with three rows bold by the carry alone
  (none above 10% at that step). The pinned pair walked on the six bolding worlds agrees with the
  captures (seed 0: 2 at 1000 CE, 3 at the close).
- [x] T3 (1094) the four marks with their glyphs, the capital-moved slide (on I1's fold — if I1 has
  not landed, slide old → new from the event's `other` region directly), the Post Road pulse; no
  other kind marks; STARTUP.md § Identity across the rounds matches. (1094 R1, R2) DONE 2026-09-25:
  pass 3f in draw_lapse_map (ring / crack / diamond / slide table) and the pulse in 3b; I1's fold was
  not on the base, so the slide's endpoints are the event's `other` → `region` and the crack's parent
  end is `polity_seat` (one lookup, `seat_region_of`, to re-point when the fold lands); the marker
  switch comment restated; the doc's list matches the built set, no edit needed.
- [ ] T4 captures per kind on a library seed; Release build; cold review; Ben's live click. (R3s)
  2026-09-25: Release build green in the lane's worktree; `history_lapse_hard.lua` (round 4 at
  1000 CE and the close, round 5's opening) and `history_lapse_marks.lua` (every kind on seed 0 at
  the event and half a window later, the diamond two windows on, the Post Road on round 6) both
  PASS with the clipping ledger clean; the marks are eyeballed on the crops (ring / crack / diamond /
  pulse; the slide's mid-frame is captured but not identified by eye). OWES the cold review and
  Ben's live click (1090 R3, 1094 R3). Note for the click: at 1920x1080 the map's right 116 px sit
  past the pane's clip (`map_nudge_x`), so a mark there is off-screen on that layout.

### Wave 2 (opens as wave 1's lanes land), lane F1 (Culture) — BL-1091 (from Life to people) → BL-1092 (routes and splits). Groups `from-life-to-people`, `routes-and-splits-on-the-map`.

- [ ] T1 (1091) the chain table: Life = water..legacy, Spend alone in a third group captioned for the
  History ledger (generation_charts.cpp:~30-43); the Legacy panel as the round's closing fold; the
  Drawdown lean row moves from Culture (startup_screens.cpp:~1555-1558) to Life under it. (1091 R1)
- [ ] T2 (1091) `cradle` on the enum (append-only; the envelope bump); cradle names and packages as
  two pure-output records on `settlement_state` (settlement.cpp:~859-875 coins them), resolved
  read-side in `build_migration_timelapse`; the ticker's opening lines. (1091 R2)
- [ ] T3 (1091) the globe held through the Culture wait and cross-faded into the map under a 2400 BCE
  stamp (startup_screens.cpp wait branch ~:1325-1365 and the carry gate ~:473); the body name on
  every lapse header. (1091 R3)
- [ ] T4 (1092) region-grain kin arrows baked from the change list and `culture_split` parents,
  dashed over water (`lapse_corridor_over_water`); parent-then-daughter changes per
  `culture_recultured` entry in the fold; the board census in place of folded-daughter lines; the
  400 BCE playhead clamp and caption. (1092 R1-R4)
- [ ] T5 the verify captures named in the groups; Release build; `world_determinism` twice;
  `save_envelope_roundtrip`; cold review; Ben's live click. (R5s)

### Wave 2, lane F2 (Exploration) — BL-1095 (fleets and ties). Group `fleets-and-ties`.

- [ ] T1 `navy_stock` and the capital's `port_stock_q` sampled onto `polity_sample`; `sea_leg_campaign`
  at the wet launch (history_sim.cpp:~5695-5708; append-only; the envelope bump). (R1)
- [ ] T2 ties and treaty arcs baked from events with endpoints from the capital fold (BL-1088, else the
  seat region); the hull glyph, the harbour mark, the sail crossing, the landing for an over-water
  `seat_captured`; the lane line (BL-1097) kept as its own layer. (R2, R3)
- [ ] T3 captures; Release build; `world_determinism` twice; cold review; Ben's live click. (R4)

### Wave 2, lane F3 (Industrialisation) — BL-1099 (works chartered) with BL-1100 (rung crossing) and BL-1104 (Culture record saved). Groups `works-chartered-events`, `rung-crossed-narrated`, `culture-record-saved`.

- [ ] T1 (1099) `works_chartered` in the span loop after the accrual (history_sim.cpp:~3532-3546
  region), the running price from the world stock so far over the charter divisor, a loop-local
  per-region counter, the per-round cap, `works_event_fraction_q` validated and zero-disabled;
  the harness rows (control equality, bounded/monotone count, events vs firms per seed). (1099 R1, R2)
- [ ] T2 (1099) `founded_year` / `origin_region` on `corporation_component` (world save bump), set in
  `charter_web_from_budget`; the briefing's origin sentence (seat_screen.cpp:~478-512). (1099 R3)
- [ ] T3 (1099) the in-span flash marker and, on BL-1085's charter report, the closing-frame flash of
  the real charters dated by pairing. (1099 R4)
- [ ] T4 (1100) the polity crossing noted at history_sim.cpp:~7029 and narrated; the ember layer marks
  the capital; crossings per seed reported. (1100 R1)
- [ ] T5 (1104) `body_entry::migration_timelapse` on every full run (hard_coded_world.cpp:~1134 vs
  ~1670-1673), serialised in the envelope bump. (1104 R1, R2)
- [ ] T6 harness rows; captures; Release build; `world_determinism` twice; the round trips; cold review;
  Ben's live click. (R5, R2)

### Wave 1, lane I3 (voice) — BL-1106 (names and voice). Group `names-and-voice`.

- [ ] T1 the Culture board 'peoples / Homeland', no battle cells; the civilisation/creed name table on
  the report (record-only, the envelope bump); schism prose naming both parties and the creed;
  'Something happens' gone. (R1)
- [ ] T2 in-world footers (startup_screens.cpp:~1510-1518); ticker priority by kind
  (history_lapse.cpp:~2028-2035); the lagged-slice clamp (startup_screens.cpp:~1151-1153); the
  quiet-age sentence. (R2)
- [ ] T3 captures; Release build; cold review; Ben's live click. (R3)

- [x] T1 join every in-flight worker at the top of `~app` (and on the quit path) with a
  "finishing the build before quitting" line on the wait surface. Verification: start a cold Begin
  build in a Release run and close the window mid-build — exit 0, no exception line; the same during
  a wizard round's wait. DONE 2026-09-24: `app::join_workers` (all six futures) at the end of
  `run()` with the line drawn, and first in `~app`; WM_CLOSE at +25 s of `--autostart-play`:
  cold build — exit 0, "workers joined after 131961 ms"; a wizard round's wait — exit 0,
  "workers joined after 115675 ms"; no exception line either time.

## Drained 2026-09-16

Three finished blocks were cleared at the session close; each one's record lives in the
DEVLOG entry for its session, its requirement group, and the archived backlog rows.

- **Sprint 41 — Exploration trade** (COMPLETE 2026-09-14). Nine items; world shape authorised
  at Alarm 525. Plan and collision map in `cacd27dc`.
- **Post-sprint-40 review block** (opened 2026-09-13). Its last open task was the lapse pace
  control, which landed 2026-09-16 at 30 s / 1 m / 1 m 30 s after Ben watched the first cut.
- **Sprint 42 wave 0** (COMPLETE 2026-09-15). Instruments and gates; re-blessed on NR-875.
- **Sprint 42 wave 1** (COMPLETE 2026-09-16). Sixteen items; re-blessed on NR-877 the same day.
- **Backlog wave A** (CLOSED 2026-09-16). Twelve lanes, sixteen items: twelve delivered, four archived unmerged; re-blessed in 32e04a19. Waves B-D never ran -- Ben stopped at wave A and archived the rest. Record in the DEVLOG entry of the same date.
- **Sprint 44 — the corporate web's plumbing** (CLOSED 2026-09-18). BL-1030..1033 delivered; the retro is archived with the sprint; NR-889 carries the cost readings.

## Sprint 45 — industrialisation makes the web real (opened 2026-09-18)

**CLOSED 2026-09-23.** All seventeen items delivered, one re-bless spent; the retro is in
sprints.json. What the sprint raised and did not answer: NR-915 (the tick tail), BL-1065 (the
CTest tier), BL-1066 (the player cannot build) and the sprint 46 draft DENSITY_FOLLOWS_CITIES.

Three waves. Every world-mover lands behind a switch and turns on together in BL-1044, so the sprint
spends one re-bless. Memory caps concurrency (15.5 GB; a sweep holds ~2.9 GB): lanes run their own
harnesses one at a time and never `player_seed_sweep`; the main session runs the 16-seed
`--digest-check` on the integrated tree after each merge. Requirement groups:
`world-copy-ticks-exact`, `span-boundary-resume`, `resume-road-tier`, `industry-tree-wired`,
`charter-spend-rules`; later waves get theirs when promoted.

**Wave 0 — neutral, parallel worktrees**

- [x] **BL-1034 (world copies diverge)** — DELIVERED 2026-09-18: merged d61b6ae8, cold-reviewed, --digest-check 16/16 on 26c8ec6e. files: src/world/faithful_unordered_map.hpp (new), world.hpp, province.hpp, tools/verify/world_copy_determinism.cpp (new), world_digest.hpp (new).
  provides: a faithful world copy; the reproduction harness. consumes: harness_params.hpp's app-order build and settle (landed).
- [x] **BL-1036 (span boundary resume)** then **BL-1037 (resume road tier)** — DELIVERED 2026-09-18: merged 614a02e5, follow-ups 58193812, --digest-check 16/16. BL-1036 R6 partial (BL-1049). Lane A, generation-dev. files: src/world/history_sim.hpp/.cpp, era_minus_one.cpp, tools/verify/digitisation_sim_harness.cpp.
  provides: resume_dated_objects, consolidation_year, near_home_cutoff_year, resume_seeds_corridor_tier (off), the fidelity mode. consumes: exploration_output (landed).
- [x] **BL-1038 (Industry tree wired)** — DELIVERED 2026-09-18: merged 29b74f69, fix round 191c212b, --digest-check 16/16. Lane B, generation-dev. files: src/world/industry_tree_data.hpp (generated), history_sim.hpp/.cpp, tools/session/tree_lint.js, tools/verify/exploration_sim_harness.cpp. Shares history_sim.* with lane A in disjoint regions; the main session resolves the merge.
  provides: industry_tree_enabled (off), industry_open_year, the Industry mask triple and fold, the urban-mass rate, the fork-reachability lint. consumes: the amended industry_tree.json (this cut).
- [x] **BL-1039 (charter spend rules)** — DELIVERED 2026-09-19: timed rows on a quiet machine (runs.bl1039_sqrt), ceiling 120 (NR-902), --digest-check 16/16 and empty/zero/refused 3/3 on be5b3d72 (chain 11). files: src/world/charter_budget.hpp, corporation_generation.cpp, budget_system.hpp, tools/verify/player_seed_sweep.cpp, harness_params.hpp, charter_cost_sweep.json.
  provides: the sqrt cap rule, density_ceiling reason (filling goods in turn), per-good tallies; the unspent-points capital rule was built and reverted (NR-895). consumes: charter_budget / charter_spend_params (landed, BL-1032).

**Wave 1 — the span and Beat 1, still behind switches**

- [x] **BL-1040 (Digitisation span)** — DELIVERED 2026-09-19: merged 70923856, cold-reviewed, --digest-check 16/16 on 849a358d (chain 8). files: era_minus_one.*, hard_coded_world.*, world_gen_config.hpp, history_sim.hpp, src/ui/startup_screens.cpp, digitisation_sim_harness.cpp, exploration_sweep.cpp.
- [x] **BL-1051 (span open survey)** — DELIVERED 2026-09-19: merged 44545123, cold-reviewed, --digest-check 16/16 on 849a358d (chain 8). Every region surveyed for fuel and forest at the span open; ground_forest and furnace_lit read from it (NR-891, NR-892). files: settlement.hpp/.cpp, hard_coded_world.cpp, history_sim.*, exploration_sim_harness.cpp, digitisation_sim_harness.cpp.
- [x] **BL-1041 (industry points)** — DELIVERED 2026-09-19: merged f8137b8d, fix round 849a358d, --digest-check 16/16 (chain 8), readings 8 and 9 on 849a358d. files: settlement.hpp, history_sim.*, hard_coded_world.cpp, digitisation_sim_harness.cpp.
- [x] **BL-1053 (setup reads span close)** — DELIVERED 2026-09-19: merged 9725ee79, --digest-check 16/16 (chain 8), cold review clean (two low findings in BL-1058). When the span runs, world setup reads the 1960 close (treasuries, grudges, roads, junction markets); plus the BL-1040 review's harness and validator tightening and the loading-bar count. Must land before BL-1043. files: hard_coded_world.cpp, nation_generation.cpp, history_sim.*, app.cpp, startup_screens.cpp, digitisation_sim_harness.cpp.
- [x] **BL-1056 (points size-neutral)** — DELIVERED 2026-09-19: merged acc81398, fix round 2867cdaa, --digest-check 16/16 (chain 9). The resulting Fuel Doctrine split (coke 142 / charcoal 373) is NR-899, Ben's call before BL-1043 measures. files: history_sim.*, settlement.hpp, digitisation_sim_harness.cpp, exploration_sim_harness.cpp.
- [x] **BL-1059 (top-third bar)** — DELIVERED 2026-09-19: three rounds (NR-900, NR-904 in the build), --digest-check 16/16 on be5b3d72 (chain 11). files: history_sim.*, settlement.*, digitisation_sim_harness.cpp, exploration_sim_harness.cpp.
- [x] **BL-1042 (stockpile to budget)** — DELIVERED 2026-09-19: merged c9431b0b, fix round 587b5657 (NR-901), --digest-check 16/16, stockpile accounts close on 0/28/46 (chain 11).
- [x] **BL-1060 (charter spend hardening)** — DELIVERED 2026-09-20: four rounds (NR-902, NR-903, NR-905, NR-906), --digest-check 16/16 and the budget modes 3/3 on 9a39152a (chain 12). Round 3's review caught a refusal that would have switched the charter web off on a data tune.

**Wave 2 — measure, then Ben's calls**

- [ ] **BL-1043 (real-stockpile charter sweep)** — STAGE 2 IN (2026-09-21, serial, runs.real_stockpile_bl1043_stage2): seats median 1/3/4 at d/m 113/163/225 against the anchor's 9; no-specialist worlds at every d/m; 650:4 runs the live tick at x0.91 legacy. R3 and R6 complete; the seat curve read (d/m 162-1300) and every call ruled (NR-910), R5 complete; owed: R7 (the sprint-wide cold review, at BL-1044). STAGE 1 IN (2026-09-21): 16 seeds x 3 firm prices in three parallel shards, folded as runs.real_stockpile_bl1043_a/_b/_c; R1, R2 and R4 complete. It found that no fixed price holds the seat menu (NR-907, ruled: the price is a share of the world's stockpile), so stage 2 sweeps BL-1064's divisor instead. Owed: stage 2, the serial timing pass (R3), the province-cap reading, the remaining calls (R5), R6 and R7.
- [x] **BL-1064 (derived charter price)** — DELIVERED 2026-09-21: main session, two cold-review rounds, --digest-check 16/16 (2620 s), world_determinism digests unchanged, stockpile_budget_check --r8 ALL PASS. Ben 2026-09-21 (NR-907): a charter's price is the world's own stockpile divided by a constant, fixed at build; the specialist keeps its price in firm charters. Built before BL-1043 stage 2, which reads the divisor. Requirement group `derived-charter-price`. Built in the main session (2026-09-21), cold review after. files: stockpile_budget.*, harness_params.hpp, app.cpp, player_seed_sweep.cpp, stockpile_budget_check.cpp, world_determinism.cpp (the fold carries the price; span-on only).
  - [x] T1 derive the price in the builder; the divisor constant; the spend reads it (R1, R2).
  - [x] T2 the mirror and the app pass the derived spend; the harness records the spend it charged (R1).
  - [x] T3 the sweep's --price-divisors axis, the charged price per row, the seat-menu close (R3).
  - [x] T4 stockpile_budget_check's derivation and 5x cases, part 2's price (R4).
  - [x] T5 build, run the checks, world_determinism, digest-check 16/16, cold review (R5, R6).
  - STAGE 2, RE-AIMED (Ben, 2026-09-21, NR-908): the divisor answers live-play cost, the specialist price m answers the seat menu, which turns on d/m alone (the handoff's "150" was d/m, direction inverted). Five rows per seed as --price-pairs: 450:4, 650:4, 900:4 (seat menu at d/m 112/162/225) and 325:2, 1300:8 (density at 162).

**Wave 3 — ship**

- [ ] **BL-1050 (order-dependent reads)** — BUILT and VERIFIED on branch worktree-agent-ab112b1821025f551 (2026-09-20): seven readers ordered, fix round 3c9c5792 after a cold review; --copy-by snapshot 16/16 PASS (4166 s); the 16-seed --digest-check on the branch fails on D_settle and D_seat ONLY, never D_search or D_land, so the re-bless is tick-only (NR-894). R1-R6 complete; R7 (the re-pin) is BL-1044's. MERGED ONLY WITH BL-1044.
- [ ] **BL-1044 (Beat 1 ships)** — waits on BL-1037, BL-1043, BL-1050 and Ben's rulings. The one re-bless, the 1960 readings, the cold review.

  **INTEGRATION PLAN (drafted 2026-09-21 while BL-1043 stage 2 ran; read the item's design text first).**
  Three gates, in order: Ben's calls, then the build and the measurement, then Ben authorises the
  re-bless against the shape. Main session throughout; the machine quiet for Step 2.

  *Gate 0 — RULED 2026-09-21 (NR-908, NR-909, NR-910): m = 2 with the divisor tuned so the median
  library world opens 9 seats (near 580, from the seat curve); the seat spread accepted; the
  no-specialist world falls back to the no-budget world; province cap 2; sqrt base 8; the search-less
  paths spend the budget on the seed candidate. The list below is the call record.*
  - C1 the DIVISOR, against live-play cost (NR-908): stage 2's density pairs 325:2 / 650:4 / 1300:8
    at d/m 162 and their tick ratio against the legacy row.
  - C2 m, the specialist price in firm charters, against the seat menu. EARLY STAGE 2 (6 seeds): at
    d/m 162 the median is ~2.5 seats against a legacy 8, and 225 gives ~3.5 — the anchor's 9 sits
    ABOVE the bracket. Seats do turn on d/m alone (the three d/m-162 pairs agree on 5 of 6 seeds), so
    a supplementary SEAT-CURVE reading is owed before C2: seats at d/m 300 / 450 / 650. Cheap if
    built as a budget-only instrument (centres affording a specialist, from `build_stockpile_budget`
    alone — no search, no settle), else a stage 2b row set (seats are deterministic, so shards may
    run in parallel).
  - C3 THE NO-SPECIALIST WORLD — now real: seed 46 opens no seat at four of five pairs, seed 37 none
    at 450:4. Today the budget path leaves the player NULL (corporation_generation.cpp:3907-3926) and
    spawn_seat returns early (spawn_seat.cpp:118), against world.hpp:300's "exactly one is_player".
  - C4 the per-province cap (PROPOSED held at 2): read `prov` unspent off stage 2.
  - C5 the sqrt base c = 8 (`k_stockpile_per_resource_firm_cap`, still in the PROVISIONAL block).
  - C6 NR-909: the search-less paths on a span world (below).

  *Step 1 — integrate (on main, committed in increments; `git merge-tree` shows the merge CLEAN).*
  BUILT 2026-09-22 (6fddf614 merge, 99b8d1ce, 49a6deb1); each task's verification is in Step 2.
  Requirement group `beat-one-ships` (R1-R11, 2026-09-22). Step 2 is R8, Gate 2 is R9-R11.
  - [x] T1 (R1) merge BL-1050 (3c9c5792). Its eight files are untouched on main since the merge-base
    1dc8332b, so no conflict. It moves the legacy world's D_settle and D_seat only (NR-894).
  - [x] T2 (R2) flip `digitisation_span_enabled` (hard_coded_world.hpp:181) and `resume_seeds_corridor_tier`
    (history_sim.hpp:1962; the struct default, which also covers Exploration's resume) on by default;
    rewrite both "OFF BY DEFAULT" comments and era_minus_one.cpp:470. DONE, plus a
    `world_params::resume_seeds_corridor_tier` (not saved) so the legacy arc stays buildable.
  - [x] T3 (R3) pin C1, C2, C4, C5 in stockpile_budget.hpp and drop PROVISIONAL.
  - [x] T4 (R4) C3's ruling, and the invariant stated where it binds: print it on the seat line
    (app.cpp:970) and count it per seed in player_seed_sweep. DONE: `charter_budget_affords_specialist`
    in the search and the apply, reason `no_specialist`, guard S7; the walk's residual is NR-911.
  - [x] T5 (R5) C6: verify_api.cpp:529, main.cpp:148 and main.cpp:258 call `generate_background_firms`
    directly and only warn when the stockpile is non-empty — once T2 lands, EVERY `--verify`,
    `--serve` and headless world is a span world whose budget they ignore. DONE:
    `spend_stockpile_on_seed_candidate` (stockpile_budget.hpp).
  - [x] T6 (R6) harness re-points: exploration_sim_harness.cpp:812 (R6.6 reads the 1660 grudges; setup reads
    the 1960 close with the span on, BL-1053); haulage_measure run with `--epoch 0` (its default is
    1960, :178); player_seed_sweep's pins gain an ARC field — legacy rows keep BL-1031's D_search
    and D_land and take BL-1050's D_settle/D_seat under NR-894, recorded old -> new; the shipped
    rows are new; `--digest-check` checks the arc it builds, and a legacy mode keeps the span-off
    pins a live check. DONE: `world_arc` in harness_params.hpp, `--arc`; also era_world_harness R1/R7,
    digitisation_sim_harness's 1660/--continued/--fidelity/--resume-tier modes (they assumed the old
    default).
  - [x] T7 (R7) the rulings into DIGITISATION.md § 1 and CORPORATION_GENERATION.md; nothing else in a doc. Both already
    carried them (0b2e42f4); no edit.

  *Step 2 — measure (Release, serial, keep-awake; roughly 5-6 h of machine time).*
  world_determinism old -> new (A/A records); stockpile_budget_check --r8; charter_refusal_probe;
  world_copy_determinism --copy-by snapshot 16/16 (BL-1050's owed re-run, ~70 min);
  player_seed_sweep --digest then --digest-check on the shipped arc, and the legacy arc's check;
  exploration_sweep --out then seed_library.js --check --from (fingerprints may move ONLY through
  BL-1037's tier) then --bless; history_sim_harness (its 2-failure baseline), exploration_sim_harness,
  digitisation_sim_harness; haulage_measure --epoch 0 against the 1055/802 baseline; story_check
  (2 pre-existing US-016 failures); the 1960 readings per library seed (density follows cities:
  rho(firms, urban) against rho(firms, goods), BL-1029's 0.431 vs 0.460; industrialisation);
  setup cost per seed in Release AND the Debug app; the nation treasury credit rate against its two
  stated scales (BL-1053 (1)); the ctest suite — world-building rows may breach the 60 s default
  now that setup takes 21 -> 51 s on seed 28; the visual verify suite if C6 routes verify through
  the budget.

  *Step 2 RAN 2026-09-22 (5 h 20, serial, keep-awake).* Every determinism check held
  (world_determinism A/A twice; world_copy_determinism --copy-by snapshot 16/16; --fidelity and
  --resume-tier 16/16; the legacy arc kept D_search/D_land on 16/16 and moved D_settle/D_seat on
  16/16, NR-894's shape). haulage 2035 dispatches / 1627 market-to-market against the 1055/802
  floor. history_sim (2), story_check (2 US-016) and era_world (R5, BL-1010) sat at their known
  baselines; world_determinism R3.7, era_world R1/R7 and the digitisation modes were stale checks,
  re-pointed. FOUR FINDINGS went to Ben as NR-911 to NR-914.

  *Gate 2 — RULED 2026-09-22 (Ben, two forms). NR-911 the residual is reported and counted; NR-912
  the treasury rate stands and § Pass 7 is restated from the shipped world; NR-914 the divisor is
  re-pinned to the shipped world's nine-seat value, 650 (580 was its tier-off reading); NR-913
  pooling was built and measured (region/market/nation lift rho(firms, urban) 0.139 -> 0.186-0.211,
  the 120 ceiling binds, seats move) and NOT shipped — the web ships unpooled, the done-when is
  restated as taken-and-reported, density becomes the sprint 46 draft DENSITY_FOLLOWS_CITIES, and
  the pool stays an off-by-default switch. THE RE-BLESS is authorised and taken: the shipped arc's
  16 pins at 650:2 (every row one player, none fell back), the legacy rows' D_settle/D_seat moved
  (old -> new in the DEVLOG), the seed library blessed (6 of 16; the tier-off control reproduced all
  16, so the tier is the only mover). TWO COLD-REVIEW ROUNDS: eight findings, all fixed — the
  heaviest, the launch view framed world-gen's player whom the search replaces (latent since
  BL-977, on every world here).*

  *OWED before BL-1050 and BL-1044 close: the 650:2 live-play cost row (the tick against legacy and
  setup cost per seed), the ctest suite, the visual suite, and the requirement rows' results. Then
  sprint 45's retro and the sprint 46 cut.*

## Sprint 46 — the generation reaches the game (opened 2026-09-23)

BL-1066 (the player cannot build) opens it, and Ben's rulings on it (2026-09-23) put BL-1003
(pools per market) first. The cut from `drafts/sprint-46-items.json` follows. Requirement groups:
`pools-per-market`; BL-1066's gets written when BL-1003 has landed and capacity can be measured.

- [ ] **BL-1003 (pools per market)** — economy-dev in a worktree; main session merges, verifies, cold-reviews. Digest mover: reported, not re-pinned.
  - [ ] T1 rename the pool map; key (corp, market) with the body-level fallback and absorb-on-spawn (R1).
  - [ ] T2 production, processing, construction and upkeep draw and deposit at the tile's market; wants, purchases, upkeep_wants key the market (R1, R2).
  - [ ] T3 clearing: auto-surplus and standing orders per (corp, market); retire market_for_corp_on_body from goods flow (R2).
  - [ ] T4 convoys: source and destination market pools; the dispatcher's source walk (R3).
  - [ ] T5 every other reader: a body-aggregate helper or a stated market (R4).
  - [ ] T6 save format, state hash, the new pools_per_market harness (R1-R3, R5).
  - [ ] T7 main session: merge, build, world_determinism, the straddle count, the BL-1066 probe re-read, cold review (R5-R8).
**The sprint's core characteristic (Ben, 2026-09-24): the wizard plays the whole arc.** BL-1068
(round 6 plays the span) is the centre of sprint 46, not one row of the cut: the generation reaching
the game is first something the player WATCHES. Order: BL-1067 (the rename) -> BL-1068 -> Begin
adopts the wizard's world. The economy thread (BL-1066 and the steel collapse) runs alongside.

**The one re-bless** (Ben, 2026-09-24: "Ok"): authorised in principle, taken ONCE after sprint 46's
world-movers have landed (BL-1003, BL-995, BL-1066's slices, BL-1049, the epoch flip). Until then every
digest movement is reported old -> new in its commit and nothing is re-pinned. BL-1082 (the hash folds in
the seat) lands in the same re-bless.

- [ ] **BL-1067 (rename to Industrialisation)** — agent in a worktree; digests must not move.
- [ ] **BL-1068 (round 6 plays the span)** — after BL-1067; a live click-through is its done-when.
- [ ] **BL-995 (trade reaches for price)** — restored 2026-09-23 on Ben's BL-1066 ruling; economy-dev in a worktree; requirement group `trade-reaches-for-price`. Dispatch moves before the clear.
  - [ ] T1 the net-price rule and its quantity, the shortfall scan retired (R2, R3).
  - [ ] T2 the tick order at every replicating site; the margin threshold authored (R1, R4).
  - [ ] T3 directed verb parity; the scorer's home-price valuation (R5, R6).
  - [ ] T4 trade_reaches_for_price harness (R7); main session: determinism, far trade, the BL-1066 probe, cold review (R8-R10).
- [ ] **BL-1066 (the player cannot build)** — after BL-1003: price the site multiplier at placement (gate, Build door preview, rival scorer); construction capacity where the player stands (measure first); the fixture places near home and waits out the tech gate.
