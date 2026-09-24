# Sprint 47 narrative survey (2026-09-24)

> Derived evidence, not authority. Produced by the `sprint47-narrative-survey` workflow (91 agents: one reader per wizard round, an adversarial refuter per claimed break, a synthesis and a completeness critic) on main at v0.1.25. 80 breaks confirmed, 3 refuted; the confirmed list is `sprint-47-breaks.json`. Line citations are as of v0.1.25 — re-read before trusting. Section A is the synthesis; section B is the critic's corrections, which OVERRIDE A where they conflict.

## A. Synthesis


## 1. The arc as it plays today

**Rounds 1–2, Planetology (The System, Life).**
- Round 1 is a flat orrery. The star is coloured by mass, bodies carry archetype tags, and the homeworld is ringed and named (`generation_preview.cpp:121-192`).
- Round 2 turns it into a spinning globe built from the real tile raster. The coined name sits under the globe (`:424-425`) and the stage folds run Water, Spark, Breath, Green.
- The story stops at "land was colonised and can burn". There is no year, the star's coined name is never drawn (`body_names.hpp:54`), and there is no biography.
- The Legacy stage is never walked (`generation_charts.cpp:35-38`). That stage holds the civilisation gate and the endemics that later seed cultures.
- Next blanks the screen to "Loading the Culture round" (`startup_screens.cpp:1727`).

**Round 3, Culture (2400 → ~400 BCE).**
- A dull, unnamed flat map fades in with no cross-fade from the globe; the carry is gated `i > 0` (`startup_screens.cpp:473`).
- Lineage-tinted patches fill the land from seat dots. Cradles and daughters look identical except for hue lightness (`history_lapse.cpp:240-334, 1243-1261`).
- No route, coastal path or overseas hop is drawn (`history_lapse.cpp:559-570`).
- The board calls cultures "powers" with a "Seat", and People/Pop/Mt stay "-" all round. The counter reads "0 battles, 0 conquests".
- The only ticker line is "A new people parts from its kin at X". About 85% of those daughters are folded away and never appear on the map (`hard_coded_world.cpp:156-182`).
- No "What happened here" appears (`history_lapse.cpp:1874`). The Drawdown lean sits on this screen and has no visible effect here (`startup_screens.cpp:1556-1559`).

**Round 4, Empires (400 BCE → 1200 CE).**
- The screen goes blank again, then plays.
- Round 3's last frame cross-fades out over ~160 years (`history_lapse.cpp:787-800`). After that, culture is gone from the map.
- Polity colours come from adjacency-only greedy slots with no hue family (`history_lapse.cpp:656`). A blue-family people can found an orange city state.
- Borders, seats, roads, bridges and trade links grow. The ticker speaks in region names.
- No people, creed or civilisation is ever named on screen. `civilisation_formed` and `creed_preached` discard `e.other` (`history_lapse.cpp:1954-1959`), and coined civilisation names go only to the history log (`history_sim.cpp:4160-4166`).
- The close reports destroyed polities and peak share. It never mentions contacts, wants or the markets on capitals that round 5 needs (CIVILISATION.md:823-993).

**Round 5, Exploration (1200 → 1660).**
- The screen goes blank while the worker re-runs migration and Empires from scratch (`startup_screens.cpp:348-351, 375-378`).
- The round opens with no roads (`history_lapse.cpp:524-577`), and many realms jump colour because the palette carry is dead code (`startup_screens.cpp:500-509`).
- Some realms are renamed, because the name comes from the lowest-index region held at 1200 (`history_lapse.cpp:407-416`).
- Realms that are centuries old announce "A realm rises at X" (`history_sim.cpp:2753-2757`). Every realm gets a "*" newcomer mark for 38 years.
- Gold bursts mark the treasury consolidation, but at arbitrary regions, not capitals. Trade links re-open in a burst.
- There are no treasuries, ports, navies, sea lanes or overlord links, and no bought-or-taken distinction. The close still says "largest empire… rose and fell". The thesis appears only in a footer that cites a doc path (`startup_screens.cpp:1513-1515`).

**Round 6, Industrialisation (1660 → 1960).**
- The wait's captions replay the whole build, "Forming the system" through "Running the exploration age" (`startup_screens.cpp:1364`; `hard_coded_world.hpp:521-536`).
- The same re-founding, renaming and recolouring happen again. Yellow heat stipple and an Ind column rise.
- Furnaces never light on a generated world (`settlement.cpp:1240`; `history_sim.cpp:7040`). There is no rail, migration stream or decolonisation visual.
- The round ends on a 1960 polity fill: no firms, markets or prices, though STARTUP.md:246-249 promises "the map they will play on".

**Begin and the seat canvas.**
- The map vanishes. The UI can freeze for about 20 s while the landscape search runs on the main thread (`app.cpp:1310-1333`).
- "BUILDING THE WORLD" then shows over an already-built world, with no map.
- The canvas "WHICH CORPORATION ARE YOU?" lists firms nobody has seen, over a home map.
- Nation names are re-coined (`nation_generation.cpp:1196-1226`), colours are re-hashed at a 35% mix (`seat_screen.cpp:344-346`), and city names are re-coined (`city_names.cpp:43-84`).
- In play, only the Empires span can be replayed (`tile_inspector.cpp:401-440`). The Culture record is not in the adopted world at all (`hard_coded_world.cpp:1134`).

---

## 2. The threads

| Thread | Where it exists | Where it breaks | What carrying it needs |
|---|---|---|---|
| **The world itself** (body, star, clock) | The body is named in rounds 1–2, and the same coastline raster runs through rounds 3–6 (`startup_screens.cpp:1071-1081`). | The name is gone from round 3 on, and the star is never named. Deep time jumps to 2400 BCE with no bridge. On 6 of 60 seeds, round 3 ends after 400 BCE and the clock steps back (`hard_coded_world.cpp:1079-1082`). | The body name in each lapse header or map stamp, a dated bridge line at the civilisation gate, and a clamp so round 3 ends at or before round 4's opening year. |
| **Peoples and cultures** | Round 3's lineage palette (hue family per cradle); `q.culture` on every polity (`history_sim.cpp:1600-1653`); nation speech in the plurality tongue (`hard_coded_world.cpp:1770-1784`). | The palette is built only for lapse 0 (`startup_screens.cpp:164, 181-222`). The fade dies at ~240 BCE. Cradle names never cross the report (`creeds.hpp`). Recultured splits are painted from founding, not from the split year (`settlement.cpp:1146-1160`). | A persistent culture base under the polity fill in rounds 4–6 (promised at STARTUP.md:166-172 and CIVILISATION.md:338-341), polity hue family taken from its culture, culture names carried in the report, and a second owner-change at `culture_recultured.year`. |
| **Named polities and lineage** | Polity ids are shared across resumed spans (`history_sim.cpp:1542-1566`). Greedy colour slots exist. | The palette carry copies into an empty vector and is then cleared (`startup_screens.cpp:500-509`; `history_lapse.cpp:656, 661`). The name comes from the lowest-index region at each span's opening (`history_lapse.cpp:407-416`). Resumed spans re-found every live realm (`history_sim.cpp:2753-2757`). At Begin, nations are re-named and re-coloured. | Seed `assign_polity_colours` with the predecessor slots. Give each realm a name fixed at founding and carried by id. Put the seat dot at `q.capital`. Emit no `founded` event on resume. The nation inherits the realm's name and colour, or cites it. |
| **Creeds and civilisations** | Coined in the sim (`history_sim.cpp:4160-4166`); `creed_preached`, `civilisation_formed` and `schism` events. | The ticker drops `e.other`. `schism` has no prose and prints "Something happens at X" (`history_lapse.cpp:1922-2003`). | Names in ticker prose, which needs a name table in the lapse record. A civilisation label on the board is optional. |
| **Places and capitals** | Region names are stable across rounds 3–6 (settlement seed `params.seed^0x5E77ED`, `hard_coded_world.cpp:917-919`). | Seat dots and the 1200 burst sit at `polity_seat`, not the capital. The ticker names `q.capital` while the board names `polity_seat`. At Begin, cities get fresh names unrelated to regions (`city_names.cpp:18-41`). | Seat dot and burst at the capital. The city takes its region's name, or the card shows "<city>, in <region>". |
| **Roads and routes** | Round 3 has none. Round 4 draws promoted corridors and trade links. Corridors carry into the sim on resume (`history_sim.cpp:1920-1949`). | Round 3 draws no migration routes. Round 5 and round 6 start with no roads, and trade links re-open in a burst. Round 6 has no rail. | Migration hop events in round 3's record. Inherited corridors and open links baked into a resumed record as its starting state, per EXPLORATION.md § goods move as throughput ("reopens at the rung it was recorded at"). |
| **Industry that ends the arc** | Round 6's heat stipple and Ind column (industry heat work). Industry points become the charter budget (`app.cpp:1321-1328`). | The capital-to-industry conversion is never shown. There are no furnace events and no goods layer. The Ind column's tie to the firms is never stated. | A round 6 close that names the chartering. Firms drawn at 1960 on the round 6 map. |
| **The corporation the player becomes** | Seat card: HQ city, home market, nation with stance (`seat_screen.cpp:480-512`). | Firms have no past. `corporation_component` has no founding year or origin, and no lapse event kind covers a firm. The landscape search replaces round 6's roster after Begin (`app.cpp:1283-1295`). | Run the search inside round 6's build so firms draw in place at 1960. Add an origin line on the card: chartered from <city>'s industry, under the realm formerly called X. |

---

## 3. The breaks, ranked by damage to "one history"

1. **A reroll forks the history silently.**
   - A round 5 reroll bumps the shared `era_seed`. Round 5, round 6 and Begin then descend from a different Empires history than round 4 still shows (`startup_screens.cpp:1631-1649`; `app.hpp:287-322`; `era_minus_one.cpp:315, 382, 446`).
   - The carry is built from the stale record. What the player watched is not the history they play.
2. **Realms lose their identity at every seam.**
   - The colour carry is dead code (`startup_screens.cpp:500-509`; `history_lapse.cpp:656, 661`).
   - Names reset to the lowest-index region (`history_lapse.cpp:407-416`; `history_sim.cpp:2705-2712`).
   - Every survivor is announced as "A realm rises" (`history_sim.cpp:2753-2757`).
3. **Begin re-mints the world.**
   - Nation names (`nation_generation.cpp:1196-1226`), colours (`seat_screen.cpp:344-346`) and city names (`city_names.cpp`) are all new. The player cannot tell which realm became their nation.
   - The borders do carry (Pass 2c fold, `hard_coded_world.cpp:1745`).
4. **The culture thread dies about 160 years into Empires.** The persistent culture base under the polity fill promised at STARTUP.md:166-172 does not exist, and polity hues ignore culture (`history_lapse.cpp:656, 787-800, 950-975`).
5. **Round 6 does not close on the map you play.**
   - Firms, markets and prices are made after Begin (`app.cpp:781, 1330-1333`), and nothing draws them (`history_lapse.cpp:802-1349`).
   - STARTUP.md:246-249 is not met.
6. **Every seam is a blank cut.**
   - The pane is empty during each wait (`startup_screens.cpp:1727`). Round 6's captions narrate a full rebuild (`:1364`).
   - Begin shows "BUILDING THE WORLD" with no map, after a ~20 s freeze (`app.cpp:562-572, 807`).
7. **No bridge from life to people.**
   - The globe cuts to an unnamed map at 2400 BCE. The Legacy stage, the civilisation gate and the endemics are unwalked (`generation_charts.cpp:35-38`; PLANETOLOGY.md:322-409).
   - Cradles and domestication packages are unannounced. No cradle event exists (`hard_coded_world.cpp:164-169`).
8. **Roads vanish at the 1200 and 1660 seams, and trade links re-open in a burst** (`history_lapse.cpp:524-628`; `history_sim.cpp:1920-1949`).
9. **Nothing is ever named but ground.**
   - No people, creed or civilisation appears in any ticker line (`history_lapse.cpp:1954-1959`).
   - `schism` prints "Something happens" (`:2002`). An ownerless founding is called a realm rising (`history_sim.cpp:3144-3146`).
10. **Each age's close speaks Empires' words.**
    - `draw_lapse_arc` is identical for rounds 4–6 and absent for round 3 (`history_lapse.cpp:1871-1911`).
    - No close sets up the next age: Empires names no contacts or wants, Exploration shows no displacement reading, Industrialisation says nothing about industry.
11. **Round 3 shows no routes, and its splits are misdated.** COLONISATION.md:122-159 wants routes drawn and splits visible when they happen (`settlement.cpp:1146-1160`; `colonisation.hpp:738-744`).
12. **The history cannot be revisited after Begin.** Only Empires replays. The Culture record is dropped (`hard_coded_world.cpp:1134`), and Exploration and Industrialisation are saved but unread (`tile_inspector.cpp:431`).
13. **Voice and wording.**
    - Culture uses "powers/Seat" and "0 battles, 0 conquests". Footers cite repository paths (`startup_screens.cpp:1510-1518`).
    - The quiet-age sentence calls polities "peoples" (`history_lapse.cpp:1893`).
14. **The clock and leans.**
    - Round 3 can end after 400 BCE, so the clock steps back.
    - Drawdown sits on Culture, which reads nothing from it, and is stated "editable nowhere" at STARTUP.md:122-124.
    - The turbulence lean needlessly wipes Culture's record (`startup_screens.cpp:1006-1013, 1572-1585`).
    - Exploration and Industrialisation have no lean (`:825-830`).
15. **The firms have no past.** Nothing links the Ind column to the charter budget (`app.cpp:1321-1328`; `seat_screen.cpp:480-512`).

Minor, and fixable alongside the above:
- Five-round `*` newcomer marks at an opening, from the unclamped lagged slice (`startup_screens.cpp:1151-1153`).
- Ticker crowding by trade and treaty churn (`history_lapse.cpp:2028-2035`).
- Begin during round 6's build starts a second, competing cold build (`app.cpp:553-637`).
- Stale comments and `wizard.round4.*` fit_text ids (`history_lapse.cpp:1645, 2055`; `.hpp:12-16, 38-66`).

---

## 4. Candidate sprint 47 items

Suggested order: **identity first** (1–4), then **the culture thread** (5–7), then **the closes** (8–9), then **the hand-over into play** (10–12). Every later item assumes a realm keeps its id, name and colour across a seam.

**1. `one-history-reroll`: a reroll never forks what was watched.**
- **Does:** gives each span its own seed (Empires, Exploration, Industrialisation), so a round N reroll re-seeds only span N. Invalidation and carry stay consistent with this.
- **Owner:** STARTUP.md § New World wizard (leans and reroll).
- **Answers:** break 1.
- **Done when:** reroll round 5, go Back to round 4, then Next. Round 4's 1200 frame is unchanged and round 5 opens on it. After Begin, the Country lens borders match round 6's last frame.
- **Design call:** see Q3.

**2. `realm-keeps-its-colour`: the palette carry actually carries.**
- **Does:** passes the predecessor record's `polity_slot` into `assign_polity_colours` as a seed; new realms take free slots. This replaces the dead copy at `startup_screens.cpp:500-509`.
- **Owner:** STARTUP.md § A round opens on the ground the round before it left (:336-343).
- **Answers:** break 2 (colour).
- **Done when:** on seed 650, a realm alive at 1200 has the same swatch on the round 4 board at 1200 and the round 5 board at 1200. The same holds at 1660.
- **Design call:** none.

**3. `realm-keeps-its-name`: one name per realm, one seat, no re-founding.**
- **Does:**
  - The realm name is fixed at first founding and carried by polity id through the resume.
  - The seat dot and the 1200 burst draw at `q.capital`; `capital_moved` moves the dot.
  - A resumed span emits no `founded` event for inherited realms (`history_sim.cpp:2753-2757`).
  - An ownerless founding reads "A people settle at X".
- **Owner:** CIVILISATION.md (naming and § How a fall is told); STARTUP.md § Round 4 (board).
- **Answers:** break 2 (name), break 9 (partly).
- **Done when:** round 5's ticker at 1200 has no "A realm rises" line for a realm on round 4's final board. A realm's board name is identical on round 4's last frame and round 5's first. After "X re-seats itself at Y", the dot is at Y.
- **Design call:** Q1 (what the name is).

**4. `roads-survive-the-seam`: resumed spans open on the inherited network.**
- **Does:** writes inherited corridors (at their rungs) and open trade links into a resumed record as its starting layer. The first-year re-open burst is suppressed in the ticker.
- **Owner:** EXPLORATION.md § Goods move as throughput.
- **Answers:** break 8.
- **Done when:** scrub round 5 to 1200. Round 4's 1200 roads and green links are drawn, and the ticker shows no link openings dated 1200.
- **Design call:** none.

**5. `culture-under-the-realms`: the peoples stay visible to 1960.**
- **Does:**
  - A dull lineage-hue culture base under the polity fill in rounds 4–6, per STARTUP.md:166-172 and CIVILISATION.md:338-341.
  - Polity slots are drawn from their culture's hue family, so kin realms read as kin.
  - Needs the lineage palette built for every lapse, not only lapse 0 (`startup_screens.cpp:164`).
- **Owner:** STARTUP.md § Round 4; CIVILISATION.md § The arc is watched.
- **Answers:** break 4.
- **Done when:** at 800 CE, unorganised peopled ground shows its culture tint. Two city states from one cradle share a hue family. The base is still present in round 6.
- **Design call:** Q4.

**6. `from-life-to-people`: the Life round hands a named world and its cradles to Culture.**
- **Does:**
  - Walks the Legacy stage (the civilisation gate and endemics) as the Life round's last fold.
  - The globe dissolves into the Culture map (a cross-fade, with a map year stamp that starts at 2400 BCE).
  - Body name on every lapse header.
  - A `cradle` lapse event naming each cradle culture and its domestication package, with cradle culture names carried across the report (`creeds.hpp`).
- **Owner:** PLANETOLOGY.md § S8 Legacy and § Body naming; COLONISATION.md § The domestication package; STARTUP.md § Round 3.
- **Answers:** break 7, and part of break 6.
- **Done when:** pressing Next on Life shows no blank pane. The Culture ticker's first lines name each cradle and its package, and the header carries the world's name.
- **Design call:** Q6.

**7. `routes-and-splits-on-the-map`: the migration shows its roads.**
- **Does:**
  - Migration hops (coastal and overseas) become lapse events and are drawn as fading route lines.
  - Recultured regions show the parent colour first, then change at the split year.
  - Folded daughters are dropped from the ticker.
- **Owner:** COLONISATION.md:122-159, :387-416.
- **Answers:** break 11, plus the ticker noise noted in round 3.
- **Done when:** on a seed with an overseas hop, the line is visible at its year, and a range visibly changes hue at the split year.
- **Design call:** none.

**8. `names-and-voice`: the ticker and board speak the history's words.**
- **Does:**
  - Culture: "peoples/Homeland" in place of "powers/Seat", and a migration counter.
  - Creed, civilisation and schism names in prose.
  - In-world footers in place of doc paths, and the quiet-age wording fixed.
  - Priority by kind, so rare arc events beat trade churn.
  - The lagged-slice clamp.
- **Owner:** STARTUP.md § Rounds (board and ticker); CIVILISATION.md § How a fall is told.
- **Answers:** breaks 9 and 13, and the minor items.
- **Done when:** the Culture board header reads "peoples". A schism line names both parties and the creed. No footer contains "docs/". At 1200, no inherited realm shows a `*`.
- **Design call:** none (Light-sized).

**9. `each-age-closes-its-chapter`: a per-round "What happened here" that tees up the next age.**
- **Does:**
  - Culture: cradles, peoples, splits, overseas hops.
  - Empires: the arc, plus contacts and wants at 1200.
  - Exploration: displacement, treaties, colonies bought or taken.
  - Industrialisation: industry and the charter budget.
  - Each close ends with one line naming what the next age inherits.
- **Owner:** STARTUP.md § Rounds; each generation doc's "judged on" or closure section.
- **Answers:** break 10.
- **Done when:** each of the four rounds, played to its end, shows a close in its own terms, ending with a line that names the next round's subject.
- **Design call:** Q7.

**10. `no-blank-seams`: the map never goes empty between rounds.**
- **Does:**
  - Holds the previous round's final frame (or the globe) in the map pane during a wait.
  - Captions name only the span being lived (`startup_screens.cpp:1364`).
  - Begin on the adopted path shows the 1960 map, not an empty "BUILDING THE WORLD".
  - Optionally, prebuilds round N+1 while round N plays.
- **Owner:** STARTUP.md § A round opens on the ground (:336-343) and § The world cache.
- **Answers:** break 6.
- **Done when:** press Next on each of rounds 2–5, and on Begin. At no frame is the map pane empty, and round 6's wait never reads "Forming the system".
- **Design call:** Q5.

**11. `realm-becomes-nation`: Begin keeps the names and colours the player learned.**
- **Does:**
  - Each 1960 nation takes its founding realm's name and colour, or cites it ("formerly X").
  - Cities are named for their region, or the card cites the region.
  - The seat map cross-fades from round 6's last frame.
  - The nation-count line explains merges.
- **Owner:** NATION_GENERATION.md (naming, the polity fold); STARTUP.md § The seat.
- **Answers:** break 3.
- **Done when:** pick the realm ranked #1 on round 6's final board. On the seat map, its territory carries the same colour and the card's nation line carries its name or cites it.
- **Design call:** Q2.

**12. `the-map-you-will-play`: firms at 1960, with a past.**
- **Does:**
  - Runs the landscape search inside round 6's build, off the main thread, so the chosen firms, markets and price field draw on round 6's closing frame.
  - The seat card gains an origin line tying the firm to its city's industry.
- **Owner:** STARTUP.md:246-249 and § The seat; INDUSTRIALISATION.md § What crosses into play.
- **Answers:** breaks 5 and 15, and the ~20 s freeze.
- **Done when:** round 6's final frame shows HQ glyphs and market icons. The seat list is exactly those firms, and Begin shows no freeze.
- **Design call:** Q8 and Q9.

**Out of scope unless Ben pulls them in:**
- **`history-in-play`:** all four spans in the Ages view, and the Culture record kept in the adopted world (break 12).
- **Sim-side beats:** Industrialisation Beat 2 migration and Beat 3 decolonisation (both cancelled unbuilt), and rail. Neither is presentation work; see Q12.

**Doc-truth fixes to land with the first item that touches each doc:**
- STARTUP.md:72, :75 and :95-97: round count and numbering.
- STARTUP.md:122-124: Drawdown is "editable nowhere", but it is on the Culture round.
- STARTUP.md:183-186 and GENERATION_STRATEGY.md:343: "research speed" on the board.
- STARTUP.md:214-235: rail, migration and the furnace ticker, described as if they happen.
- PLANETOLOGY.md:895-901: Inheritance still listed as a round.
- CIVILISATION.md:50-51: "coast to 1560, pass 2".

---

## 5. Open design questions for Ben (form-ready)

1. **What is a realm's name across rounds 4–6?**
   - (a) Its founding seat region's name, frozen.
   - (b) A name coined in its culture's tongue at founding.
   - (c) Seat region until a civilisation forms, then the civilisation's name.
2. **At Begin, what does a 1960 nation call itself?**
   - (a) Its round-6 realm's name and colour, unchanged.
   - (b) A new name, but "formerly X" on the card and in tooltips.
   - (c) Keep today's re-mint.
3. **What does a reroll re-roll?**
   - (a) Only this round's span; earlier rounds stay fixed.
   - (b) This round and everything after it, with earlier rounds visibly re-run.
   - Also: should a Culture reroll change the migration itself? Today it does not.
4. **After round 3, how are the peoples shown?**
   - (a) A persistent dull culture base under the polity fill.
   - (b) Polity hues from their culture's family only.
   - (c) Both.
5. **How should the wait between rounds read?**
   - (a) Hold the last frame, with a span-only caption.
   - (b) Prebuild the next round in the background while this one plays, costing CPU during playback.
   - (c) One continuous scrubber across 2400 BCE–1960 with chapter stops, replacing separate rounds.
6. **How does the Life round hand over to people?**
   - (a) Legacy returns as the Life round's last fold.
   - (b) An opening card on Culture names the cradles and their packages.
   - (c) Both.
7. **Should each round's close name what the next age inherits?** Yes or no. If yes: one line, or a short list of the readings?
8. **Should the landscape search run inside round 6's build?** Yes means a longer round-6 wait and firms drawn at 1960 with no Begin freeze. No means keep it after Begin and cross-fade into the seat.
9. **Can the seat card break its four-line cap for an origin line?** For example: "Chartered from <city>'s industry, under the realm formerly <X>."
10. **Where does the Drawdown lean live, and do Exploration and Industrialisation get leans?**
    - (a) Drawdown moves to Industrialisation, and the two late rounds get new axes.
    - (b) Drawdown is removed from the wizard.
    - (c) Leave it on Culture and fix the doc.
11. **Should the whole four-span history stay replayable in play (the Ages view)?** Yes, this sprint / yes, later / no.
12. **Is sprint 47 presentation-only?** That means showing the history that already exists. Or does it also take on sim work: Industrialisation migration streams, decolonisation, rail, and a war event kind?

## B. Completeness critic — corrections and gaps

- **The palette carry is dead code (confirmed, with the mechanism).** `lapse_from_report` never calls `finish_history_lapse`, so a landed record's `polity_slot` is still empty at `startup_screens.cpp:505-508`. `n = min(0, …)` is 0, so nothing is copied. Later, `finish_history_lapse` → `assign_polity_colours` clears and recolours (`history_lapse.cpp:656, 661, 750`).
  - **Correction to item 2:** the fix cannot seed at landing. `poll_wizard_history_tap` clears `tile_region` every ~0.2 s while live (`startup_screens.cpp:593`), and that re-derives the palette.
  - The fix must also go inside `assign_polity_colours` as pinned slots, with greedy colouring for the rest. A plain copy would let a carried slot clash with a neighbour's fresh greedy slot. Item 2's "Done when" should add "no two adjacent realms share a swatch".

- **The reroll fork is confirmed, and a Culture reroll does nothing visible.** One `era_seed` feeds the Empires, Exploration and Industrialisation seeds (`era_minus_one.cpp:315, 382, 446`). The reroll bumps it (`startup_screens.cpp:1646`) and invalidates only later rounds (`:1633`).
  - **Gap:** the migration's seed is `params.seed ^ 0x5E77ED` with no `era_seed` (`hard_coded_world.cpp:917-919`). So a Culture reroll replays the same migration but forks every age after it.
  - The synthesis puts this in Q3 only as an aside. It is a live defect: a Reroll button with no visible effect that silently changes the downstream history. Item 1 should own it.

- **Losing the watched history on Back or a planetology change is missing.** Any planetology change sets `m_wiz_dirty`, which calls `invalidate_wizard_rounds_below(1)` (`startup_screens.cpp:1006-1013`). That throws away all four lapse records without warning.
  - Break 14 says the turbulence lean "needlessly wipes Culture". That is true, but not through the explicit call at `:1584`, which only reaches round 4 onward. It happens through `m_wiz_dirty = true` at `:1576`, which also recomputes the planetology preview.
  - The migration it forces to re-run is the same one, because its seed does not depend on the lean. So it is a wasted wait and a blank pane.
  - No item covers what the player loses or whether they are told. Item 1 or item 10 should.

- **The "A realm rises" re-founding is confirmed.** `history_sim.cpp:2753-2757` skips only dead polities on resume. Every live inherited realm gets a `founded` event at `q.capital`, which confirms item 3's premise.

- **The furnace claim is misframed.** The event kind and its prose exist: `furnace_lit` is emitted at `history_sim.cpp:2746` and `:7047` (BL-1080) and has a ticker case in `history_lapse.cpp`.
  - Nothing lights because of the sim. Generation always takes the `antiquity` branch (`settlement.cpp:1240`, `stop_year < 1700`), so Stage 4's lag is never dated. `industrial_lag_years < 0` then skips every region (`history_sim.cpp:7040`).
  - The threads table says "no furnace events". The fix is sim work, so it belongs with Q12's sim-side beats, not with presentation. No item mentions it.

- **"Walk the Legacy stage" means bringing back a retired round.** `generation_charts.cpp:30-38` still defines a third chain round, "Inheritance" (legacy→spend), which the two-round wizard never reaches.
  - Item 6 and Q6a are really "revive part of Inheritance". That conflicts with PLANETOLOGY.md:895-901, which the synthesis files only as a doc-truth fix. This needs to be a design call, not a fold tweak.

- **The Begin cache has a second cold path the synthesis misses.** Begin adopts the round-6 world only if `same_world_params(...)` holds and `ready` is set (`app.cpp:553-557`). Otherwise it drops the cache and runs a full cold build.
  - Item 10's "Begin shows the 1960 map" must cover this path too, not just the adopted one.
  - Note: a round-5 reroll stays consistent here, because round 6 re-runs under the new `era_seed`.

- **The roster swap is confirmed in the code's own words.** `app.cpp:1290-1294`: "What the loading screen's ledger listed during generation was the world-gen roster; the one the player meets is the winner's."
  - **The freeze figure is stale.** The ~20 s comes from a 2026-09-07 measurement of "placement + tier only" (`app.cpp:1319-1322`). The roster axis now regenerates specialists for every candidate, so the real freeze may be longer. Re-measure before item 12 is sized.

- **The nation re-mint is confirmed, with one detail that matters for Q1.** Nation names are coined in the tongue of the lowest-index surviving seed (`nation_generation.cpp:1196-1226`, `make_nation_name`). So nations already speak their culture's tongue, while realms on the board use region names.
  - Q1(b), "coined in the culture's tongue", would make realm and nation naming the same thing. Q2 should say so.
  - The nation count is a fold of realms, so "one realm, one nation" does not hold. Item 11 needs a rule for merged realms, beyond a line of explanation.

- **The campaign's own opening screen is missing.** STARTUP.md owns everything "between launch and the first frame of play" (`STARTUP.md:3, 11`). The synthesis stops at the seat card.
  - It does not ask what the first in-game frame shows: the body name, 1960, a line looking back at the history, or a way into the Ages view. It also does not ask whether the header carries the world's name.
  - Add it as a thread, or fold it into item 11 or 12.

- **Creeds and cultures in the campaign are unread.** The synthesis traces names that persist for nations, cities and firms. It never checks whether a creed, cradle culture or civilisation reaches campaign data (nation, population centre, tooltips).
  - Only `nation_speech` is shown to carry over. Someone needs to read this before Q1 and Q2 go to Ben.

- **People are a missing thread.** PEOPLE.md owns named individuals. No ruler, founder or chartering magnate appears in any round. That may be right, since a person exists only where a role gates something, but the synthesis should rule it out explicitly.

- **Some measurements are cited with no command behind them.**
  - "6 of 60 seeds end after 400 BCE" and "about 85% of daughters are folded away" name no tool. Give the command or drop the numbers.
  - The seed-650 test in item 2 depends on a seed library entry. Check with `seed_library.js --seed 650`.

- **Duplicate check and process.** There is no evidence the candidates were checked against open work (`backlog_query.js --grep lapse|wizard|round`). The Ages view and firm-origin work may already be filed.
  - The Exploration and Industrialisation leans (Q10), and a new `cradle` event kind, grow the scope. Per the standing rules, each needs a `novel-work` entry in NEEDS_REVIEW.json.

**What I verified by reading the code:** the palette carry, reroll/era_seed, re-founding on resume, furnace_lit, the invalidation paths, the Begin cache path, the nation naming, the roster swap and the Inheritance chain round. Every other file:line citation in the synthesis is unchecked.