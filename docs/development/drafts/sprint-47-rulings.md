# Sprint 47 rulings (2026-09-24)

> Derived evidence and the session's call record, not authority. Ben answered the sprint 47 design
> form on 2026-09-24 (the survey's twelve questions amended by two reader workflows:
> `sprint-47-scoping-readers.json`, `sprint-47-design-prep.json`). Each ruling below is written into
> its owning authority doc as present-tense design; this file is the record of WHO ruled WHAT and the
> delegated readings taken on Ben's behalf (each of those is also a `NEEDS_REVIEW.json` entry so it
> can be overturned). Line citations are as of v0.1.25 + 1d401e8c.

## Ben's brief, verbatim in substance

- "Pull details from each generation round into the next, without having to recalculate."
  Mid-session: "we should pass the calculations of each round over without having to recalculate
  at each step."
- "Visually see the growth: carry colour and shade from one round to the next."
- Visual flair per round: migration arrows (Culture), harder borders for large empires (Empires),
  visual fleets (Exploration), flashing company creation (Industrialisation). "Suggest more."
- Form notes: "We basically want the Industrialisation round to do all of the work in 'Begin'; we
  should completely obsolete that block of code. When it comes to generation, we don't want to do
  the work twice, so it makes sense to seed actual materials at the Life gate, and pass tiles
  through the whole phase. This can also make cultures more accurate, as often materials shaped
  culture, and even primitive cultures can gain a preference towards certain amenities." And:
  "it should apply some slightly different readings of some of the options I chose above."
- On rerolls: "We should focus on eliminating rerolls, or ensuring that they are only in each
  stage. That way, we don't worry about prior stage results, and we can carry prior worlds
  faithfully. If there is a stronger reason to recalculate, then let me know — if each round isn't
  even accurate, then what's the point in displaying them?"

## A. The seams — the world is built once and moves forward

**R1. What crosses a seam (Ben): the whole half-built world.** Not a struct of the sim close;
the world itself. Reading (delegated, NR): deposits already land at stage 4 (`tile_generation.cpp`
Pass 6, `generate_life_deposits_over` :2284-2298), downstream of S8 Legacy and S9 Spend
(`planetology.cpp:1533-1555` — the drawdown lean multiplies endowments BEFORE tiles), so "seed
materials at the Life gate" is already the pass ORDER; what changes is OWNERSHIP: today every lapse
round's worker rebuilds stages 0-7 from params (`startup_screens.cpp:375-379`;
`hard_coded_world.cpp:550-916`) and rounds 0-1 build a scratch world for the globe and discard it.
The design: `make_hard_coded_world` becomes the composition of resumable stage functions over one
`generation_cursor` (world + report + naming + rw + deposit_scalar + kepler_tiles/pl/hist/creeds/
settlement/np/corridors/grudges/region_polity/polity_treasuries/capital_market_shells + the last
span's resume struct) — `gen_life_gate` (bumps 0-5: Helios, Cinder, Kepler chain, tiles WITH
deposits, rivers), `gen_culture`, `gen_empires`, `gen_exploration`, `gen_industrialisation` + the
tail (bumps 9-12). The composition is byte-identical to the monolith (every harness, fixture,
`gen_step_costs` and the 16 seed-library pins keep one invocation); proven by `world_determinism`
and a cursor-vs-monolith check at EVERY round boundary, not only at Begin. The wizard's Life round
builds the real Life-gate world into slot 0 (the globe rasters pack off it); each lapse round's
worker takes slot i-1 and runs only stage i into slot i; Begin adopts slot 3 exactly as BL-1073
does. The gate world is Helios + Cinder + Kepler; **Selene and Pallas stay in Finishing** (moving
them forward shifts every entity id: a full re-bless for two bodies no round shows — delegated, NR).

**R2. Rerolls are per stage (Ben), and the "stronger reason to recalculate" is answered.** A round-N
reroll re-seeds ONLY span N. This needs **one seed per span**: `span_seed[4]` (migration, Empires,
Exploration, Industrialisation) on `world_params`, each folded additively into its own span's seed
so all-zero reproduces today's digests; `era_seed` kept as the legacy term; `same_world_params`,
the save envelope and the seed library grow (`era_minus_one.cpp:315/:382/:446`;
`hard_coded_world.cpp:919` for the migration, which today has NO seed so a Culture reroll changes
nothing visible while forking every later age). The one honest reason to recalculate: a world
MOVED into round N's worker and mutated is consumed, so a reroll of N needs round N-1's closing
world back. Reading (delegated, NR): **the hybrid** — hold the predecessor's closed world by a
faithful copy at landing (BL-1034 proved a copy ticks as its original; `world_copy_determinism`),
move a copy into the worker, re-copy on reroll; **if the predecessor is not held, replay from the
Life gate with the fixed per-span seeds**, which is bit-identical by determinism and costs in
Release 0.5 s (stages 0-7) + 1.6 s Empires + 1.5 s Exploration (`hard_coded_world.cpp:360-398`).
Nothing displayed is ever inaccurate: with per-span seeds a replay IS the same calculation, so the
forward path never recomputes and the reroll path recomputes only what it must. Measure the copy
cost first; if it exceeds the replay it saves, ship pure replay. Back keeps every record and world
(no recompute). A serialised snapshot is rejected: a save round trip reorders the unordered stores
(`harness_params.hpp:403-412`). The turbulence lean on round 4 routes through its own dirty path
(invalidate Empires onward, relaunch at once) — today `m_wiz_dirty` wipes all four records for a
migration that is provably identical under the lean (`startup_screens.cpp:1575, :1006-1013`).

**R3. Round 6 lands its record at the 1960 close; the tail builds behind the playback (Ben).**
Today the wait is ~55 s of which ~47 s is borders (3.5 s), roads (38 s), the old-road stamp
(5.5 s), companies and finishing — after the span. The tap's final publish
(`history_sim.cpp:7899`, year == stop_year-1) is "record whole"; playback starts there while the
same worker continues the tail into the world cache; landing only swaps the world slot. Captions
name only the step under way. The blank wait ruling of 2026-09-16 stands for the span itself.
Prebuilding round N+1 during round N: NOT taken (Ben chose the plain "yes" option); the 2026-09-09
"arriving on the round IS the instruction to run it" wording stands.

**R4. Round 6 does all of Begin's work; Begin's block is obsoleted (Ben).** Everything Begin does
that mutates the world moves into ONE `world/*` function, `finish_campaign_world(world&,
recipe_registry&, const world_params&, const world_gen_config&, generation_progress*)` = the
world-only halves of setup (genesis history, survey states) + the band (R14) + `assign_default_recipes`
+ the stockpile budget + the landscape search + the winner apply + the recipe pass + the twelve-tick
settle — called by the round-6 worker after `make_hard_coded_world` and by Begin's cold worker (menu →
Begin with no wizard, `--autostart`), same call, same order, so an adopted world and a cold build
still open on one state hash. The settle's tick body is PROMOTED from `tools/verify/harness_params.hpp
:569-639` into `src/world` first (one function for the app, the worker and every harness). The
recipe registry is loaded from Lua on the main thread before the round-6 launch (the `m_works`
pattern) and a COPY handed to the worker, banded there from the world it built; Begin moves that
banded copy into `m_registry`. What stays at Begin: the presentation half of setup (epoch formatter,
chat line, zoom/focus/frame), `m_econ_steps = 12`, the tech tree and persona bench, the balance
series from filed returns, the seat (rank → canvas / `--seat` / draw, through `take_seat`), the
clock rebase. **Begin pressed while round 6's worker runs WAITS on it** (never a second build:
BL-1078's memory-pressure case). The presentation half of the twelve pre-game ticks (agency comms,
history samples, the last report) is DROPPED — counsel and battles were already suppressed and the
comms were stamped day 0 and never seen (delegated, NR). The settle keeps ERAS.md's meaning: twelve
quarterly ticks with no calendar meaning, the clock rebased at Begin; only the wait's caption names
it ("Proving the field"). `run_serve` adopts the searched+settled world; `run_verify` keeps the
unsearched seed-candidate world this sprint (goldens). The loading bar's label table (16/16,
`hard_coded_world.hpp:541`) widens for "Searching the landscape" / "Proving the field" with
measured weights. BL-1078 (Begin blocks the window) is delivered by this. **Folding the search INTO
`make_hard_coded_world` at stage 11** so markets carve on the searched roster and no world-gen
roster is laid then removed (`landscape_search.cpp:296-298`) is a world-mover of its own —
filed as a stretch item in the sprint's re-bless, not in the first wave (delegated, NR).

## B. Identity across rounds 4 → 5 → 6 → Begin

**R5. A realm's name is coined in its founding culture's tongue at founding and carried by id
(Ben).** `polity::name` coined via `coin_lexicon` over the founding region's culture speech at
`founded`; a `polity_name` table on `era_timelapse` beside `region_name`; the board and ticker
print it; a resumed span carries it by id. Nation naming (Pass 5) inherits it verbatim. The seat
dot and the 1200 burst follow a UI-side capital-at-year fold of `founded` + `capital_moved`; the
NAME never follows the capital. The resume's "A realm rises" re-emit (`history_sim.cpp:2753-2757`,
the only place a resumed record states a live realm's capital) becomes a ticker-silent `inherited`
kind (region = q.capital, dated start_year). An ownerless founding reads "A people settle at X".

**R6. At Begin a 1960 nation keeps its realm's name and colour, unchanged (Ben).** The polity fold
(`nation_generation.cpp:1025`, the code's "Pass 2d") is one realm to one nation; where the Pass 2c
size floor absorbs a small realm into a neighbour, **the absorber keeps its name and the card lists
what it absorbed** (merge rule A); ownerless ground keeps a coined name and the card says so. Pass 5
follows the fold representative through the merge (closing the defect where the lowest-index
surviving seed names the realm). Nation COLOUR: a per-world nation → slot table pinned into
`nation_colour` at Begin/load from the saved report, so the national border band (the Country lens is retired — LENSES.md), the seat map and the
Ages view share the wizard's colours (today three mappings: `presentation.cpp:369-396` hash,
the lapse's 20-slot greedy table, `tile_inspector.cpp:529-530`); sequenced AFTER the pinned-slot fix
and after the 20-slot table's colour-blind safety is checked against the 12-slot table's reasoning.

**R7. Colour: BOTH (Ben).** Polity hue drawn from the founding culture family's wedge with a
within-wedge offset chosen greedily (kin realms read as kin; the lineage palette built for every
lapse, not only lapse 0 — `startup_screens.cpp:181-224`), AND a dull lineage-hue culture base
under the polity fill on rounds 4-6 from each region's plurality in `culture_changes`. Pinned
slots replace the dead landing copy (`startup_screens.cpp:488-509`; `lapse_from_report` never
calls `finish_history_lapse`, so `polity_slot` is empty at landing): pins live on the record,
greedy for the rest; a pinned/fresh adjacency clash re-slots the smaller people-share realm at the
round's opening step; a dead realm's slot is retired only for a newcomer seated inside its
last-held ground. Count pinned clashes across the 16 curated seeds first (Rule 0b).

**R8. Shade is a ratchet (Ben):** one darker rung at a named moment — `civilisation_formed` for the
realm, or crossing the ROSE threshold (peak ≥ 2× start and +3 regions, the sweep's own rule,
`history_lapse.hpp:529-531`) — stable except at the moment, so it carries by id. Today the fill is
one fixed `tint_alpha 170` (`history_lapse.cpp:76, 889-891`).

**R9. A hard border is people share above a fixed threshold with hysteresis (Ben).** The threshold
is MEASURED across the 16 curated seeds before it is fixed (a share-distribution column on
`history_sweep`, Rule 0b); the two frontier passes (`history_lapse.cpp:979-985, :1000-1013`) draw
2 px dark plus a 1 px inner stroke in the realm's colour when either side is hard; the board bolds
the same rows; the flag carries into round 5 by id.

**R10. Marks (Ben): four kinds earn a map mark**, each a different glyph — `seat_captured` (a
winner-colour ring at the fallen seat), `broke_away`/`schism` (a crack from parent seat to
successor seat), `civilisation_formed` (a two-tone diamond that stays at the coining region),
`capital_moved` (the seat dot slides old → new). Layer transitions count as marks on a thing, not
pings: the Post Road promotion pulse (chosen), tie tint. `realm_ended` and `creed_preached` do not
mark. The 2026-09-16 no-blanket-ring ruling (today only in `history_lapse.cpp:1332-1348`) is
written into STARTUP.md.

## C. Each round's flair

**R11. Culture: region-grain kin arrows now (Ben); the true hop record filed as its own item.**
Each founding draws a fading line from its people's previous region, dashed where
`lapse_corridor_over_water` says so; recultured regions show the parent hue until the split year
(`settlement_state::culture_recultured`, read by nothing today); folded daughters (~85%, the fold
census on `settlement_state::census`) leave the ticker for a board census; the round-3 playhead is
clamped at 400 BCE with an overrun caption (6 of 60 seeds run past it — presentation clamp, not a
record clamp, which would contradict BL-947). Board: "peoples / Homeland", no battle cells.

**R12. The Life → people bridge (Ben): Legacy as the Life round's last fold, plus a `cradle`
event.** The chain table gives Life = water..legacy (Spend parked in the third group for the
History ledger only; `generation_charts.cpp:30-43`); the globe holds in the pane during the Culture
wait and cross-fades into the map with a 2400 BCE year stamp; the body name sits on every lapse
header; a `cradle` lapse kind per people (region = the cradle seat, dated -2400) with cradle names
and packages retained as two pure-output records on `settlement_state` and resolved read-side. The
**Drawdown lean moves to the Life round under the Legacy fold** (it is a planetology input — S9
Spend → endowments — so it belongs where the world is built); no new late-round axes this sprint.
Inheritance is NOT revived as a round.

**R13. Exploration: fleets that are there (Ben).** Colonial ties (dashed overlord → subject lines
from `subject_bound`/`subject_freed`/`realm_ended`) and treaty arcs (`treaty_formed`/`treaty_broken`)
baked from events; `navy_stock` and the capital's `port_stock_q` as series fields on
`polity_sample` (a hull glyph that scales, a harbour mark that silts); one `sea_leg_campaign`
event at the wet campaign launch (`history_sim.cpp:5695-5708`; region = target, polity = attacker,
other = staging hub) with a sail crossing; over-water seat captures drawn as a landing (chosen
extra). All record-only writes; digests unmoved.

**R14. Bought or taken: build the purchase verb and the sea-lane tier this sprint — a sim beat
with a re-bless (Ben).** The specific ruling wins over the general "no sim beats" (delegated
reading, NR). Purchase: a fork inside the BL-934 subjection block (`history_sim.cpp:3817-3918`) —
when the native seat is coastal (port_q > 0) and the arriver's seat treasury covers the price, BUY:
price = max(floor, native seat treasury × rate), debited buyer seat → credited native seat, no
ground-taken grudge, culture shares untouched, `subject_kind` = the VERB TAKEN (no longer derived
from port_q), a `province_bought` event; else TAKE as today. A trade province stays a LABEL on the
bound native polity this sprint; the minted foothold region EXPLORATION.md:279-283 describes is a
continuity-pass item. Constants measured on the 16 seeds before they are set (bought/taken split
not degenerate on ≥ 4 seeds; treasury spread reported). The fuller `sim_verb::purchase` contest is
filed, not built. Sea lanes: a `sea_leg{a,b,uses}` table on the sim state and the span outputs,
noted at three sites — the wet campaign launch, the purchase crossing, and once per decision round
per standing overlord link ("the standing traffic between a metropole and what it holds") — with a
`sea_lane_opened` event when a leg crosses `sea_lane_tier1_uses` (default 4, mirroring
`road_tier1_uses`) so round 5 bakes a persistent lane line distinct from a tie. The STAMP —
`stamp_sea_lanes` beside `stamp_history_roads`, a water-only walker, a new `tile_component::
lane_level` (world save bump), one multiplier line in `tile_traversal_cost`, folded into the deep
digest — is its own item (the water walker is new code). Switching `sea_legs_ration_q` on for the
later spans (today 0, so wet campaigns starve and the port bonus is dead code in the spans
EXPLORATION.md owns) is raised to Ben as its own call (NR), not folded in.

**R15. Industrialisation: company creation as mid-span `works_chartered` events at a fixed
fraction of a price (Ben).** Reading (delegated, NR): shape C. The event is RECORD-ONLY in the
span — emitted after the year's accrual when a region's `industry_points` crosses the next multiple
of f × the RUNNING price (world stock so far / 650, self-consistent with the close's price; a fixed
constant was rejected by NR-907; the 1960 price cannot be applied retroactively), capped per region
per round, gated exactly as `furnace_lit`; f measured on the harness (works events vs 1960 firm
count per seed) before it is pinned, starting at 1. Points are NOT debited (a Works sink is Beat
1's, a real force with its own re-bless). The event carries (region, polity, focus, year) so that
once Begin's walk runs inside round 6 (R4) the CLOSE flashes the REAL charters
(`charter_spend_report.charters`, richest centre first, at each anchor tile), dated by pairing a
region's k-th real firm with its k-th in-span crossing. `corporation_component` gains
`founded_year` and `origin_region` (world save bump) so the seat briefing's origin sentence reads
the firm, not the report. The world-gen roster at the round-6 close is never flashed (the player
never meets it). Heat blooms: not chosen.

**R16. The furnace: narrate the polity crossing the sim already computes (Ben).** Region furnaces
never light on a generated world (the antiquity branch, `settlement.cpp:1230-1240`); the polity's
crossing into the industrial materials band (`history_sim.cpp:7025-7030`, `industrial_year`) is
noted as a record-only event (reuse `furnace_lit` with region = capital, or a `rung_crossed`
kind) and narrated; on a seed where none crosses the layer is honestly empty.

**R17. The recipe band comes from the history's industry state, not the epoch (Ben).** Reading
(delegated, NR): per WORLD — `industrial` iff any living polity's materials capacity reaches the
industrial rung at the 1960 fold (`roster_band_for_capacity`, the derivation `industrial_year`
already uses), persisted as `world::campaign_band` (world save bump, the `qualification` pattern)
and applied on both `load_economy` and `load_game_from` (closing the hole where a loaded save never
sets its band); `industrial_band_from_year` / `era_band_for_epoch` retired; the ten harness call
sites read the world's band. Per-nation grain comes through the already-ruled CAMPAIGN_TECH_STATE
route (Industry mask → per-corporation `earned_techs`), not a second band system. Measure
`polities_crossed` across the 16 seeds BEFORE fixing the threshold — a seed may derive `ancient`
and lose its industrial roster; never reshape the sim to force `industrial`. What `--epoch 0`
means afterwards (calendar only, or plus a `--band` override for the ancient sandbox) is Ben's
(NR).

**R18. Tariffs: the Industrialisation span derives each nation's tariff posture (Ben).**
`protection_q` derived at the span's end-of-run block from scarcity_q, trade_flows and
culture_preference (today written only when `boundary_year != INT64_MIN`, which the span never
sets, so `seed_national_tariffs` skips every nation); the broadcast then feeds
`derive_national_protection` unchanged. The convoy-arrival duty that makes a posture bite in play
(MARKETS.md § Tariffs, ruled 2026-09-15) is filed as its own economy item, not this sprint.

**R19. Sprint-46 carried calls (Ben):** the Logistic Point cap on market exports — measure more
first (BL-1071 carries the note); a market export moves no money, the haul is the margin given up
— CONFIRMED; the epoch picks the band — NO, see R17.

## D. Closes, history in play, the first frame, scope

**R20. No per-round close (Ben).** The arc readout stays the only summary; no `contact_made` /
`market_stood` kinds; the survey's item 9 is dropped.

**R21. History in play: save the Culture record now, the Ages view later (Ben).**
`body_entry::migration_timelapse` written on every full run from the same
`build_migration_timelapse` fold, in the ONE envelope bump this sprint takes (with the per-span
seeds and the new event kinds); the four-span Ages view is filed for later.

**R22. The seat briefing gains one origin sentence (Ben):** "Chartered from <city>'s industry, in
<region>, under <nation>, the realm of <X> since <year>" — read from the firm's `founded_year` /
`origin_region` and the record's `owner_slice_at`. No opening card, no header change, no city
renaming.

**R23. Scope (Ben): presentation, record-only kinds and fields, and the per-span seeds; no sim
beats; no people** — with the EXCEPTIONS Ben chose by name on the same form: the purchase verb and
sea-lane tier (R14), the band from history (R17), the tariff posture (R18), the mid-span works
events (R15, record-only), and the Begin relocation (R4). Reading (delegated, NR): a specific
ruling wins over the general one. **No named individual appears in any round** — no ruler, founder
or chartering magnate — because no role gates anything there (PEOPLE.md's rule); written into
STARTUP.md as the rule-out.

**R24. Materials shape culture (Ben's note; the profile's composition is a delegated reading, NR-926).** A per-culture GROUND PROFILE coined at the cradle
beside the package (a `survey_endowment`-shaped sum over the cradle window's deposits plus an
amenity class of cover), stored on `culture` beside `origin_farm_class`, inherited by daughters
through `derive_daughter_culture`; record-only this sprint, no consumer (the natural later reader
is `derive_culture_preference`). Filed; built if the cursor lands early (delegated, NR).

**R25. The exit crash (found this session).** Quitting while a cold Begin build runs crashes with
an access violation: `~app` never joins `m_worldgen_future` and the progress/works objects the
worker reads are destroyed before the future blocks (`app.cpp:252`; `app.hpp:613/:615/:891`).
Generation has no cancel flag. Filed as a defect: join the in-flight workers at the top of the
destructor and say so on screen.

## E. Doc-truth fixes that land with the first item touching each doc

STARTUP.md :72/:75/:95-97 round count and numbering ("three rounds", "§ Rounds 4, 5 and 6");
:122-124 drawdown "editable nowhere"; :183-186 "research speed" (and GENERATION_STRATEGY.md:343);
:214-235 rail/migration/furnace described as happening; :251-256 the reroll claim; :307-308 causal
downward only; :336-343 the continuity claim; :382-404 "ONE world at most"; the missing pings
ruling. PLANETOLOGY.md:895-901 Inheritance listed as a round. CIVILISATION.md:50-51 "coast to
1560". COLONISATION.md:421 package on the founding set. NATION_GENERATION.md:276-278 the
absorbed-seed claim; no fold section. EXPLORATION.md:151-157 throughput exemplar; :383-395 the
port bonus in spans where sea legs are off. INDUSTRIALISATION.md:64-74 epoch/band; :792 campaign
tech per nation; :246-249 (STARTUP) firms at the close. ERAS.md:40-46, :69-70, :110-114.
PRODUCTION.md:887, :1060-1063. GENERATION_STRATEGY.md:33-36, :67-70, :249, :884-889.
LOGISTICS.md:282-283 lane decay deferred to a retired doc. AI_OPPONENT.md:1641 owner citation.
Stale code comments: `startup_screens.cpp:488-500`, `world_gen_config.hpp:95-101`,
`hard_coded_world.hpp:44-63, :410-412`, `history_lapse.hpp:15/:38-45/:61-66/:286-287`,
`app.hpp:280-283, :643-650, :711-727`, `history_sim.cpp:936-939/:961`, `history_sim.hpp:2683-2687,
:2718-2731`, `components.hpp:1741`, `hard_coded_world.hpp:730`, `era_roster.cpp:3-4`, the duplicated
"Pass 2c" labels in `nation_generation.cpp`, `charter_budget.hpp:185-188`.

## F. The close form (Ben, 2026-09-25)

Taken after the one re-bless (`2f877e2c`). Each ruling is written into its owning doc; the NR entry
carries the resolution.

- **Sprint shape.** BL-1084 (the world built once and moved) and wave 3 (BL-1086, BL-1098, BL-1107)
  carry to sprint 48; NR-936 (Next does not wait) rides with BL-1084. The done-when drops the
  seam clause.
- **NR-941 → A.** The origin sentence collapses when the holder is the nation's own realm.
- **NR-939 → B.** "Since" is tenure: walk back through the earlier records by polity id.
- **NR-940 → A.** The works-chartered dawn cluster is accepted.
- **NR-937 → B.** Hard borders at 10% on / 6% off, inter-realm edges only; a coast is not a frontier.
- **NR-942 → A.** A kin arrow dashes on two interior water tiles.
- **NR-931 → C.** The Culture round carries no Reroll.
- **NR-935 → A.** The tariff bands stay 300 / 500 / 700.
- **NR-934 → A.** The rung stays; per-nation grain through BL-1109.
- **NR-921 → B.** Sea legs run in the later spans, as BL-1115 with its own re-bless.
- **NR-920 → C.** Epoch 0 is retired (BL-1114).
- **NR-932 → C.** A settle progress tap now; tick 1 instrumented as BL-1117.
- **NR-943 → B.** Conditional range checks in both validators (BL-1116, Light).

## G. The delegated-decision form (Ben, 2026-09-25)

- **NR-919: the blanket "no sim beats" is overruled.** Sim beats are in scope, one re-bless each.
- **Confirmed as taken:** NR-918 (copy + replay), NR-923 (Selene and Pallas in Finishing), NR-924
  (the order), NR-927 (settle presentation dropped), NR-928 (stamp through the existing seam),
  NR-930 (the purchase fork), NR-933 (2000 per mille), NR-938 (pinned-vs-pinned). NR-922 accepted.
- **Overturned into sim beats:** NR-925 → real in-span chartering (BL-1122); NR-926 → the ground
  profile gets its reader (BL-1107 widened); NR-929 → the band is per nation, the registry
  actor-aware (BL-1123; ERAS.md owns the rule).
- **The lane reads as a wide soft sea-blue band** under the colonial tie (built; BL-1097 R3 owes a look).
