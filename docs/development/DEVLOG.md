# Project Io — Development Log

Entries are newest-first. Each entry covers one development session and records what was built, what in-session decisions were made, and what was left open. Decisions that affect the whole project permanently belong in TECH_FOUNDATIONS or a dedicated ADR; this log is for session-scoped choices and progress notes.

Entries that correspond to a tagged snapshot in `backups/` carry an explicit **version** marker in their heading (e.g. *version 0.0.2*) and a **Backup** line naming the snapshot path. These are the rollback points: to revert, restore the named `backups/vX.Y.Z/` tree over `src/`.

Every entry also carries a **Runtime** line: wall-clock session length, plus mode (Light/Full,
refinement/delivery/design/etc.). This builds a record of how long similar tasks take, so future
sessions can be scoped and paced with less waste.

---

## 2026-09-24 (evening) — Sprint 47 cut: one history, told through the rounds

**Runtime:** ~6 h so far (the design pass, the cut, wave 0 launched; the lanes run on). Design → Full
(the cut); Fable with ultracode — four reader workflows (7 + 5 + 10 + 4 agents) and one lane
workflow (5 + 5).

### What landed
- **The design form.** The survey's twelve questions amended by two reader workflows (seven
  scoping readers over the seams, colour carry, per-round visual data and naming; five design-prep
  readers over the world-once, Begin-retired, mid-span-chartering, band-from-history and
  purchase-verb rulings). Twenty-seven groups; Ben answered every one. His picks went deeper than
  the recommendations on six: the whole world moves (not the sim close), hard borders by a share
  threshold with hysteresis, the purchase verb and sea-lane tier now, the band from the history,
  mid-span works events, the search inside the round's wait. No per-round close, no prebuild.
- **The rulings record** `drafts/sprint-47-rulings.md` (R1-R25) and the rulings written into
  twenty-seven docs by ten writers in place, then a four-lens cold review (50 findings, ~30 distinct)
  and a five-fixer pass. `df44d903` (the cut), `c4eb810c` (wave-0 filing).
- **Filed** BL-1083..BL-1109 (23 in the sprint, 4 for later), NR-918..NR-930 (delegated readings, two
  open calls: NR-920 `--epoch 0`, NR-921 sea legs in the later spans), sprint 47 opened with four
  waves, six requirement groups, REFINED.md lanes G / C / W1-W3.
- **Wave 0 launched** as one workflow, five worktree lanes each followed by a cold code review:
  BL-1083 (one seed per span) → BL-1084 next; BL-1085 (Begin retired into round six) + BL-1108;
  BL-1096 → BL-1097 (purchase verb, sea legs); BL-1101 (band from history); BL-1102 (tariff posture).
  Merge, Release build, `world_determinism` ×2, the round trips and the live click stay in the main
  session.

### What the measurement said
- The recalculation Ben's brief targets is small: 0.5 s (stages 0-7), 2.1 s (+Empires), 3.6 s
  (+Exploration) per seam in Release; of round 6's ~55 s wait, ~47 s is the tail after the 1960 close
  (roads 38 s alone). So the seam item is continuity, and the wait item is landing the record at the
  close — which is what Ben chose.
- Deposits already land at stage 4, downstream of S8/S9: "materials at the Life gate" is a change of
  ownership (the world is built once and moved), not of pass order.
- **Quitting during a build crashes on exit** (0xC0000005, both the cold Begin build and a wizard
  round): `~app` never joins the workers and their progress/works objects die first. BL-1108.

### The overnight merges (2026-09-25, 00:00–03:30)
- **Wave 0 all merged**: G `7bf2bac0` + fix round `8889487a` (stale-run relaunch), C `2c5c2746`
  (Begin retired into round six; BL-1108 join on quit), W2 `f954a0fc` (the band's one body is
  `campaign_band_from_world`; a fold-less fixture bands industrial), W1 `747a5bf4` (the sea-leg
  producer line and the Empires-span gate added on merge; the two envelope claims reconciled to
  ONE v22), W3 `cc5e51d1`. Gates after each: determinism twice bit-identical, both round trips,
  Begin-adopts, seat-pick; on the full tree the exploration harness fails exactly its R3b pin
  (+400 tribute) and industrialisation fidelity is 16/16 with the producer wired.
- **Wave 1**: I2 (hard borders, marks) `27154072` + fix round `70b2759e` (rest seat off the record,
  civilisation diamonds carried); I3 (names and voice) `4a8848a7` + fix round `84baaa65`. I1
  (colour, name, nation) and wave 2's F3 (works chartered, rung, Culture record) were cut off by
  the session limit mid-build and RESUMED on their own worktrees.
- **The re-bless reading**: `player_seed_sweep --digest-check` 0/16 on the full wave-0 tree — every
  pin moved (laws on all sixteen seeds); world_determinism's seed B reads 4F7BBD76AEEF3431 on main
  now. The re-bless is taken once, after I1 merges. A second digest check runs on the G+C-only tree
  to settle whether the settle promotion alone moved anything (BL-1085 R1).
- Filed NR-937 (the hard-border pin and the heavy coast), NR-938 (the clash rule is
  pinned-vs-pinned), BL-1110 and BL-1111 (two defects lane I1 met in passing).
- **I1 merged** `66bdbc0f` after a branch-side fix round (main merged in first, so the identity
  files' collisions were resolved on the branch) and four low notes taken on main `f81d0d05`.
- **Attribution by row comparison, not by pins.** The sweep's 16 pins were stale since sprint 46,
  so every tree read 0/16 against them and the FAIL said nothing. Running the digest check on the
  pre-sprint tree and on the G+C-only tree gave identical rows on all 16 seeds: the seeds and the
  settle promotion moved nothing (BL-1085 R1 closed). Against the full wave-0 tree the laws move
  every seed from the landing digest on and the purchases move five seeds from the search digest.
- **F3 merged** `845c3fde` + its fix round on main `b596747c` (05:30-06:20). The second cold review
  found only evidence faults -- rows pinning pre-I1 digests, R3/R4 quoting pre-fix text -- and one
  low: a report row whose corp is gone now takes the epoch year. Determinism three times on the
  merged tree: seedA/on 6DBC0094F0B6B0EF, seedB/on 95EEAD1204FD31AC, seedA/off 4834366D19271E5F,
  main's own post-I1 digits, so F3 moves nothing; every other gate green; the works close flashes
  exactly 40 marks for 40 charters. **NR-941**: the origin sentence doubles its name now that
  nations carry their realm's coined name ('under Guashe These, the realm of Guashe These since
  1660 CE') -- two lanes composing, each right alone. The I1 identity checks on main all green and
  the 16-seed sweep re-read into BL-1087 R5 (75 / 5 pinned clashes, was 101 / 18 over the 1960
  raster). Twenty-two stale worktrees pruned; header_graph is comparable again (329 dangling).

### Open
- Ben's five live clicks (BL-1068, 1072, 1073, 1076, 1080) — he opened the Release build and walked into
  round 4 but did not file the answers on the form — plus the sprint-47 clicks the merged lanes
  owe (BL-1083 R5/R6, BL-1085 R8, BL-1090 R3, BL-1094 R3, BL-1097 R3, BL-1106 R3).
- F1 (BL-1091/1092) and F2 (BL-1095) committed on their branches, their cold review running;
  then the re-bless, BL-1084 (the cursor) and the stretch items.
- NR-918..NR-941 for Ben (NR-941 before the live click on the seat briefing).

---

## 2026-09-22 — Beat 1 ships: the span and the charter budget on by default, one re-bless

**Runtime:** ~11 h (08:30 to ~19:30, the measurement unattended through the day). Delivery — Full
(BL-1044, main session throughout; two cold-review rounds), with two Gate 2 design forms (NR-911 to
NR-914, then NR-913's pooling call).

### What landed
- **BL-1050 (order-independent reads)** merged (6fddf614), as planned, only with BL-1044.
- **BL-1044 (Beat 1 ships).** The Digitisation span and BL-1037's corridor tier run on by default;
  a `world_params` tier switch keeps the pre-flip world buildable as the LEGACY arc. The pins: a
  specialist two firm charters, the divisor **650** (NR-914 — the nine-seat rule read on the shipped
  world; 580 was its tier-off reading), province cap 2, sqrt base 8. The no-specialist world falls
  back to the no-budget world before anything is chartered (NR-910), with its own unspent reason;
  the one-player invariant is printed on the seat line and counted per seed (guard S7); the residual
  is reported, not patched (NR-911). `--verify`, `--serve` and the headless run spend a non-empty
  budget on the seed candidate (NR-909). `player_seed_sweep --arc shipped|legacy`.
- **The re-bless** (authorised at Gate 2): the shipped arc's 16 pins taken at 650:2; the legacy
  rows' D_settle/D_seat moved (below); the seed library blessed — 6 of 16 moved, and
  `exploration_sweep --arc legacy` reproduced all 16 old fingerprints, so the tier is the only mover.

### What the measurement said
Every determinism check held (world_determinism A/A, world_copy_determinism --copy-by snapshot
16/16, --fidelity 16/16, --resume-tier 16/16). Haulage 2035 dispatches / 1627 market to market.
Four findings went to Ben: the seat menu at 580 came out 6.5, not 9, because the tier moved every
stockpile (NR-914, re-pinned 650); the median nation opens on 0.41 credits (NR-912, the rate kept,
§ Pass 7 restated); the no-player residual (NR-911, reported); and **density does not follow
cities** (NR-913): a firm costs 1/650 of the whole stock, most of 5,000-15,000 centres hold less,
~85% of every stock goes unspent, and rho(firms, urban) 0.139 sits below rho(firms, goods) 0.167.
Pooling the stranded remainder was built and measured (region 0.186 / 0.195, market 0.211 / 0.204,
nation 0.156 / 0.213) — the 120 ceiling binds and seats move — so Ben shipped it unpooled and made
density its own sprint 46 design item; the pool stays an off-by-default switch.

### The cold review
Six findings in round 1, all fixed: a fell-back cost row failed its rule check; exploration R3b took
the tier from the struct default; the search-less paths were silent on a rejected budget; pooling
could overflow int32; **the launch view framed world-gen's player, whom the search replaces**
(latent since BL-977, on every world under BL-1044 — now `app::frame_launch_view`, re-run after the
seat); two stale comments. Round 2 held all six and caught that a re-frame must reset first.

### The legacy arc's re-pinned digests (NR-894: BL-1050 moves the ticks only)

| seed | D_settle old → new | D_seat old → new |
|---|---|---|
| 46 | `05B4865F46884E7C` → `01B28F9955D0EC29` | `496E75B156DC9208` → `51AF24CE2939B4D0` |
| 28 | `265C48A23E313B1A` → `4D338B0202264D5C` | `AA35460CE5894594` → `2C463C3DD4724685` |
| 11 | `2E8907B0BBE768E7` → `4C97C1C842D23C3B` | `82A858E16FE9CA69` → `66427046BACBC06C` |
| 31 | `4AEB84A2E62A4536` → `2D4BD68AE2B9E70B` | `B58F31B1D2761E4D` → `15694B8A93C3CD06` |
| 40 | `4987C80D094C8DAE` → `F336C089F6EEBFEB` | `2B509E9C965DD8BF` → `D7275BB13B29AB2D` |
| 12 | `FC9F8D4246024A2F` → `111F81D5AFEEA576` | `81E9BB11AB34278B` → `602E807556398990` |
| 37 | `F175EAB9B7F2BF41` → `928B123846974CAA` | `94B3B7C1A92369E5` → `CEA516E07DFF3356` |
| 13 | `72797C2C57E94EB1` → `BC9A8253728E77CC` | `0D38309D62D10FE3` → `13D563007848185C` |
| 41 | `C0EDD8B3B38193C6` → `9D7DBA35957885A1` | `9CB3AC1F2EC21D7F` → `2676C5DC78EEAF10` |
| 43 | `557E96CF9F2BA372` → `E4D38BA322A56963` | `03D4B5D542CB228B` → `0D441274814EE0CD` |
| 32 | `AE3347D83E077849` → `953F9A92426452FF` | `FD384DA6A173808F` → `4FC2A8BD099DD3AB` |
| 10 | `8EA4043497A22AB7` → `2F69415005705C94` | `59D340FE15B12642` → `50DF22739DFC7D73` |
| 25 | `FEFD82C8BCDD4D22` → `22C20B2D79B86944` | `2F145BF320CFB58B` → `A2046D98550FBDAC` |
| 38 | `73239E8FE1A24AA3` → `87EE17644608DB65` | `90D2AC55FC74A8D3` → `9FD043C8BB29D902` |
| 9 | `23DBD6FA7E7D5955` → `9D8F22AD82E026E7` | `A2B82933E77D219A` → `FBDBDC152D43D4AE` |
| 0 | `A392EFF987F374E2` → `AF3BDD5524ED9FF3` | `8BBEAB8453901456` → `08F13299905A405A` |

D_search and D_land hold their 2026-09-17 values on every row.

---

## 2026-09-21 — The charter price becomes the world's own, and the seat menu is read off the budget

**Runtime:** ~10 h (14:00 to ~00:00, stage 2 unattended through the evening). Delivery — Full
(BL-1064 built in the main session, two cold-review rounds) and Design (two elicitation forms:
NR-908, then NR-909 and NR-910).

### What landed
- **BL-1064 (derived charter price)**, b89d0c2b. A firm charter costs the world's whole stockpile
  over a divisor, fixed once at build, never below a point; a bad divisor or an int32-breaking price
  rejects the budget, and an empty budget prices nothing. player_seed_sweep's stockpile mode gained
  `--price-divisors` and `--price-pairs D:M`, reads each row's charged price back from the shipped
  path, and closes with the seat menu. Span off, nothing moved: --digest-check 16/16, the four
  world_determinism digests unchanged.
- **BL-1043 stage 2**, e28fc758: 16 seeds x five price pairs, serial with keep-awake, 6.4 h.
- **The seat curve**, ab655703: `stockpile_budget_check --seat-curve` counts the centres affording a
  specialist straight off the budget — equal to the charted seats on 74 of 80 stage 2 rows, at ~80 s
  a seed instead of ~25 minutes.

### Reading the docs changed the plan
Ben asked for the surrounding generation docs to be read before the sweep was re-aimed. They already
gave the firm price to live-play cost and the specialist price to the seat menu — which BL-1064 as
filed had collapsed into one knob. The handoff's "divisor near 150" turned out to be the divisor over
the specialist's charters, with the seat direction inverted. Ben ruled the split (NR-908), and stage
2 was re-cut into pairs that hold the seat ratio while the divisor moves.

### What the measurements said
Seats turn on the ratio alone — the three pairs at d/m 162 opened the same seats while their firm
counts ran 4 to 120 — and the divisor alone sets the tick: x0.43 / x0.91 / x1.70 the legacy world at
325:2 / 650:4 / 1300:8. But the anchor's 9 seats sit near d/m 290, above the whole stage 2 bracket,
and whole charters are too coarse to land it (three open ~4, two ~13.5). Nor does the derived price
make the menu one size: a world with many near-equal cities crosses the price together (seed 32 goes
from 4 to 98 seats between d/m 225 and 325), so the library runs 2 to 73 seats at the pinned 580:2. What the
derived price does remove is the empty world above d/m ~325. Ben ruled NR-910: two charters, the
divisor tuned to the median (580: a median of 9.5 and no world falling back), the spread accepted, the no-specialist world falling back
to the no-budget world, province cap 2, sqrt base 8. NR-909: the search-less starts spend the budget
on the seed candidate.

### Also
BL-1044's integration plan is in REFINED.md (BL-1050 merges clean). The keep-awake that failed on
shell quoting is now `tools/session/keepawake.ps1`.

## 2026-09-20 — Sprint 45's last builds: the charter spend hardened, a load that replays, and a refusal that would have gone quiet

**Runtime:** ~17 h (07:50 to 01:00, the sweep unattended at the end). Delivery — Full (three lanes with four fix rounds, four cold
reviews, chain 12 plus three long verification runs) with four design calls on elicitation forms.

### What landed
- **BL-1060 (charter spend hardening)** took four rounds and ships the ceiling at 120 (NR-902), the
  turn skipping a good it cannot place (NR-903), and each good keeping an even share of a binding
  ceiling as a RESERVATION (NR-905, and NR-906 after the cap reading held room back). Chain 12 on
  `9a39152a`: digest check 16/16, the budget modes 3/3, every body landing 120 firms with all ten
  goods of the turn holding some.
- **BL-1050 (order-independent reads)** is built and verified on its branch: seven readers (not the
  five the review named — the lane found a sixth, its reviewer a seventh) now walk sorted ids, and a
  saved-and-loaded world settles byte-identical to its original on **16 of 16 seeds**, where seed 28
  used to diverge at tick 0. The 16-seed pin check fails on the two TICK digests only, never on
  generation, so BL-1044's re-bless stays the shape NR-894 authorised.
- **BL-1043 (real-stockpile charter sweep)**: the harness half merged and stage 1 started — 16 seeds
  x three firm prices, in two parallel shards after Ben asked to halve the wait.

### The review that paid for itself
Round 3 of BL-1060 was rescued from a lane that stalled mid-edit, and its cold review found the new
world refusal reading the UNCAPPED yard want instead of the places actually reserved. Dormant on
today's data — and at the 0.30 seed rate `economy.lua` itself records as measured, it refuses every
world, whereupon the search silently falls back to the legacy no-budget world. A data tune would have
switched the charter web off with one printed line. Round 4 fixed it and the probe now fails on all
four assertions if the old reading comes back.

### What the sweep is already saying
Legacy seat anchors run 6 to 17 specialists across the library; the budget at the provisional firm
price opens 24, 33 and 182 on the first three seeds. So the provisional price is far too cheap, and
20k brackets the anchor on two of three. Separately, 128 of 132 extraction buildings extract a good
other than the one they were chartered for — so "every good holds firms", the property NR-903 and
NR-905 protect, is bookkeeping rather than ground truth. Both go to Ben with the sweep's figures.

### Also
The phase rename Ben raised (Digitisation names computing, not 1660-1960) is filed into the sprint 46
drafts with the name left to him. Three continuity items came out of the reviews: BL-1061, BL-1062,
BL-1063. Two self-inflicted costs are recorded as hazards: chain 8 was re-run for ~90 minutes because
a memory saying it had finished was not read at session start, and a chain-12 digest check 10 hours in
was killed as a supposed orphan.

### Stage 1 came in overnight, and moved a ruling
Three parallel shards (Ben freed memory for a third) finished 16 seeds by 00:40 — seeds 9 and 0 ran
in two shards at once and agree to the digit, a free cross-process determinism check. The reading
overturned the premise the seat-menu anchor rested on: the library's stockpiles run 24.7M to 133.6M
points and a fixed price's seats track them almost proportionally, while the no-budget roster does
not track them at all (seed 31 holds 95.3M and offers 6 legacy seats; seed 9 holds 27.5M and offers
16). At 40000 a firm, the poorest world opened 1 seat and the richest 91 against an anchor median of
9 — no single number sits inside both tails. **Ben ruled (NR-907) that a charter's price is a SHARE
of the world's own stockpile**, fixed once at build, so the menu is the same size everywhere and the
specialist keeps its price in firm charters. **BL-1064** builds it; stage 2 then sweeps the divisor
rather than a fixed band.

### Left open
BL-1064 (the derived price), then BL-1043 stage 2 over the divisor, its serial timing pass and its
remaining calls, then BL-1044 — the one re-bless, with BL-1050 merging in the same integration. See
`NEXT_SESSION.md`.

## 2026-09-19 — Sprint 45 wave 1 lands: seven items delivered, the Fuel Doctrine and the charter spend ruled nine times

**Runtime:** ~19 h wall clock (02:20 to 21:15, long unattended chains). Delivery — Full (five
worktree lanes with seven fix rounds, nine cold reviews or checks, four main-session chains and
the timed rows) with nine design calls on elicitation forms.

### Picked up from the sleep
BL-1041's fix round had committed in its worktree before the PC slept; its source change was comments
only, so it merged first and chain 8 verified it with everything else. **Chain 8 on `849a358d`**
(digest check 16/16, fidelity 16/16, seed library unmoved, digests unchanged) closed **BL-1040 (the
Digitisation span)** and **BL-1051 (span-open survey)**. A cold review nobody had run closed **BL-1053
(setup reads span close)**: no 1660 read left on the span path; two checks that cannot fail went to
BL-1058 for the continuity pass. The `--through 1960` readings closed **BL-1041 (industry points)**.

### The Fuel Doctrine, re-ruled twice
- **NR-896 and NR-897 (option A both).** The forest and fuel pulls read the SHARE of held regions over
  the world mean, not the best one (a best-of-held maximum grows with the realm); treasury points
  spread over a polity's centres by urban scale, not onto the capital. **BL-1056 (points
  size-neutral)** built it. Its cold review found the rounding of the spread broke the instrument
  behind BL-1041's headcount test, silently, and a fallback that still grew with size; the fix round
  added a report-only treasury tally and one region set for both pulls. Chain 9 on `2867cdaa`: top
  region's share of points 0.280 -> 0.039, digest check 16/16.
- **NR-899 (option C).** The share against the mean pushed the split further toward Charcoal
  (coke 142 / charcoal 373), because coal is concentrated and forest is not. Ben ruled each pull
  counts regions in its own resource's top third, a bar fixed at the span open. **BL-1059
  (top-third bar)** is building.

- **NR-900 and NR-904.** BL-1059's first build ranked only regions carrying the resource; its cold
  review showed that brings back a milder rarity tilt and leaves a cliff where scores pile at the
  survey's cap. Ben ruled every region, on the unclamped share, and a tie at the cut clearing whole.
  Chain 11 on `be5b3d72`: each bar clears a third of the world on every seed; coke 239 / charcoal
  277 / neither 340. **BL-1059 delivered.**

### The charter spend
- **BL-1039's 21 timed rows** ran on a quiet machine once League closed (7126 s). Ceiling 160 never
  bound — each good's own cap filled first — and cost a 12-31% dearer tick at 4x: Ben ruled **120**
  (NR-902). With chain 11's empty/zero/refused checks, **BL-1039 delivered.**
- **BL-1042 (stockpile to budget)** built the split of a region's points over its campaign centres.
  Its review found 44% of seed 0's stock parked on townless ground; Ben ruled a polity with no town
  converts nothing, and razed ground loses its points (NR-901).
- **BL-1060 (charter spend hardening)** carries the review findings and three rulings: the turn
  skips a good it cannot place (NR-903), and when the ceiling binds each good keeps an even share
  (NR-905) — a reservation, not a cap (NR-906, confirmed by Ben after the cap reading held room back).

### Also
Sprint 46's 19 item drafts were written from a fresh three-lane code read
(`docs/development/drafts/sprint-46-items.json`); the read found BL-1047 misses a seventh arc
selector (a 1960 epoch switches Exploration, and so the span, off). `stockpile_budget_check` joined
the verifier-headless skill with Ben's permission. BL-1057, 1058 filed for the continuity pass.

### Left open
BL-1060 round 3 (mid-edit in its lane at the sleep), chain 12 on the final main (closes BL-1060 and
BL-1042), then BL-1043, BL-1050 and BL-1044 (the re-bless), then the sprint 46 cut. See
`NEXT_SESSION.md`.

---

## 2026-09-18 — Sprint 45 cut and mostly built: the span, Beat 1's points and the charter rules, behind switches; sprint 46 shaped

**Runtime:** ~10.5 h. Design (four elicitation forms, two read workflows) and Delivery — Full (eight
worktree lanes, every one cold-reviewed, five with a fix round; nine main-session verification chains).

### The cut
A five-lane engine read with an adversarial check per lane mapped the seams before any item was filed.
Ben ruled the sprint on a form: the span runs whatever the epoch and ships on at epoch 0; Beat 1
stockpiles, with treasury paid in as a consequence rather than a verb; every living polity enters the
Industry tree at 1660 and its root is ungated; the per-resource cap scales by a square root under a
density ceiling. Every world-mover landed behind a switch, so sprint 45 spends one re-bless (BL-1044).

### What was built, all neutral with switches off
- **BL-1034 (world copies diverge):** MSVC's `unordered_map` copy reverses every multi-key bucket, and
  `body_mean_habitability` sums floats in that order. `faithful_unordered_map` makes copies iterate in
  their source's order. The same review found **a saved-and-loaded world still diverges** — BL-1050
  (order-independent reads) now rides sprint 45's re-bless (NR-894).
- **BL-1036/1037:** a lossless span resume (dated objects, civilisations, creeds; anchors explicit at
  1200) proven by a fidelity check, and a road-tier switch.
- **BL-1038, BL-1051:** the Industry tree wired, with a span-open fuel and forest survey.
- **BL-1040:** the Digitisation span as its own call from `exploration_output`.
- **BL-1053:** world setup reads the 1960 close when the span runs.
- **BL-1041:** located industry points from scale, surveyed fuel, Industry capacity and treasury.
- **BL-1039:** the charter spend rules (round 2 after Ben reversed the unspent-points capital the same
  day it was ruled: it opened 10 of 11 specialists with nothing).
- The wizard's "Loading the X round" wait gained a progress bar; Ben watched it live.

### What the reviews caught
Cold reviews found something real in every lane: BL-1036's index reuse at 1200 (BL-1049), BL-1040's
setup still reading 1660 records (BL-1053), BL-1039's capital and breadth problems, `furnace_lit`
constant wherever it is read (twice), BL-1041's biased headcount reading. No lane's figures were quoted
to Ben until a main-session chain reproduced them; every one did.

### Sprint 46 shaped
Ben asked for a sprint that brings the work into the wizard and hands it cleanly to the game. A
three-lane read found the play list almost untouched (11 of 15 rows with no item), every nation reading
"Isolationist" on screen, tariffs enacted as zero, and the game on the ancient roster at 0 CE. Ben ruled
twelve calls (NR-898): the flip lands in sprint 46 after the superseded arc is retired, round 6 plays
the span, Begin adopts the wizard's world, all eight offered play-list rows carry, and a
select-corporation screen closes the sprint.

### Left open
Chain 8's 16-seed digest check (paused by the PC's sleep), BL-1041's fix round, BL-1039's timed rows
(2026-09-19), NR-896 and NR-897, then BL-1042, 1043, 1050 and 1044, then the sprint 46 cut. See
`NEXT_SESSION.md`.

---

## 2026-09-17 — Digitisation picked up: the 1960 baseline measured, then the corporate web's plumbing

**Runtime:** ~11 h. Design (cutting the plan, four elicitation forms), Delivery — Full for sprint 43
(one lane) and sprint 44 (three worktree lanes, each cold-reviewed, two with a fix round).

### The plan was re-cut before anything was built
The handoff said Digitisation was designed and its build plan archived as "highly unstructured". Seven
read-only lanes and an adversarial cross-check mapped the engine first, and the cross-check earned its
keep: three lanes said running 1660 → 1960 needed a third resumed sim call, and it needs one parameter
(`world_params::exploration_stop_year`) with no source change. The handoff also expected the run to
un-zero *advanced chains*; the recipe band follows `epoch_year`, so it cannot. **`epoch_year = 1960`
selects the superseded 1160 → 1560 → 1960 two-span arc with Exploration off** — never a baseline.

Ben ruled the plan on a form: sprint 43 filed in full, 44–45 as goal rows; the seed library re-blessed
with replacements; the charter budget's only source is Beat 1's stockpile, no stand-in; **the budget
charters the whole web, specialists included**; both inherited weaknesses measured in 43 and ruled at
its gate.

### Sprint 43 — the 1960 baseline (four items, one day, no shipped world moved)
- **BL-1026 (seed library re-read).** `exploration_sweep` gained `--seeds`/`--out`; the library reads
  its own table. Every one of the 16 seeds had moved under wave A. Nine rationales were reworded; six
  went to Ben, who replaced four (4 → 9, 6 → 40, 19 → 38, 17 → 28) and kept 12 and 13. Seed 19 was not
  on his original list: it was neither rich nor inward any more.
- **BL-1027 (span cost to 1960).** `--through` and `--cost`. The run stopped at 1660 reproduces every
  library fingerprint, so the continued world *is* the shipped world. **1660 → 1960 costs a median
  1.7–1.9 s a seed in Release — less than the 460 years before it.** Reach is 2–17% of sim time, not
  the dominant term.
- **BL-1028 (weakness counters).** Trace-only counters ported from two unmerged commits (never their
  re-scaled capability reference), per half, in a worktree lane with a cold review. Digests identical.
  **The alarm is a seal, not a deterrent:** ~89% of near-home reads at the ceiling, 99%+ of near-home
  campaigns treaty-blocked, pooled displacement 3.02 → 1.74. **Polities meet and never bind:** 1,250
  first contacts by 1660, 0 of 492 new pairs holding non-aggression, 43 new contacts in the 300 years
  after.
- **BL-1029 (readings at 1960).** `--through` plus a 1660 control held to the library fingerprint
  (16/16). **On Exploration's forces the continued world coasts:** urban share 14.7% → 14.6%, subjects
  median 3 → 3 with none lost on 14 of 14 worlds, flows 51 → 52, checkered regions 1 → 0; only
  industrial polities climb, 12 → 21. Advanced chains stays a structural zero and now prints as one.
  A first run's two prefix mismatches were the check comparing a folded flow count against a raw one.

`EXPLORATION.md`'s contact paragraph rested on an unmerged snapshot artefact ("the seeds that met one
polity"); it now states main's measurement. At the gate (NR-888) Ben inherited both weaknesses into
Digitisation's beats — Beat 3 carries its own displacement force, this phase binds far pairs itself —
on Exploration's 4-year band.

### Sprint 44 — the corporate web's plumbing (three of four items)
Ben ruled the charter calls: **one specialist per centre that can afford one, richest first**; today's
starting capital kept; the no-specialist world deferred to sprint 45; Pass 1's nation balancing dropped
on the budget path.

- **BL-1030 (parity).** `player_seed_sweep` was measuring an off-product world: no `world_gen.lua`, no
  works registry, no `set_era`, ticks numbered from 1, the convoy credit given the wrong tick, no firm
  exits. Two helpers in `harness_params.hpp` now mirror `app.cpp` line by line. Baseline move: 109 of
  170 specialists shortlisted (was 81 of 158), a different seat on every seed, negative trailing net
  93.8% → 74.3% — which re-reads NR-886 item 3's figure.
- **BL-1031 (the world pin).** Nothing pinned world bytes across a commit: `world_determinism` never
  calls the search, and `state_hash` cannot see a building's tile or assets. Four FNV digests per seed
  (search, after the landscape, after the validation run, after the seat), pinned for all 16 seeds on
  the pre-seam tree. Tamper tests: a changed search seed fails all four; a validation run one tick short
  fails only the last two.
- **BL-1032 (the charter budget seam).** Off by default, and pinned: **16/16 seeds hold the pins with no
  budget and again with an empty one**, an all-zero and a refused budget too, with the refusal flags
  asserted. `app.cpp` takes a comment and nothing else. Two cold reviews and a fix round: a refused
  budget now mutates nothing (it used to wipe the roster), each centre spends in turn, a charter cannot
  anchor in a far region, and holding spill is measured.
- **BL-1033 (the cost instrument).** Eleven rows a seed — 1x/2x/4x crossed with the per-resource cap
  kept and lifted, a specialist-price ladder, and a build-only row that forces the province cap to bind.
  The sweep itself runs on a quiet machine after this entry.

### What the seam found, and what it costs
**A charter budget cannot raise a body's firm count above Pass 6's breadth cap.** On seed 0 at 4x, 219
of 348 points found no good to serve; lifting the cap spends 244 of 348. That is two of Ben's rulings
meeting, filed as **NR-889** — he chose to measure both before ruling. The dense rows are the cost
risk: 443 s against 198 s for 1x, nearly all of it economy ticks.

**A copied world does not tick byte for byte as its original** (seed 28: D_settle 18F78EB9B2B20F29
against the pinned 265C48A23E313B1A). Latent today — the search copies but never ticks a copy — filed
as **BL-1034** with **NR-890**, scheduling Ben's call.

### Left open
NR-886 (four of seven calls), NR-889 (the density cap), NR-890 (scheduling BL-1034).

**Closed the next morning (2026-09-18).** The cost sweep ran 33 rows over seeds 0/28/46 (the PC slept
22:07 -> 09:03 mid-run; the one straddled row was re-run). Keeping the cap freezes firms at 81 a body
and leaves 208-228 of 348 points unspent at 4x; lifting it reaches the 200-per-body guard at 4-6x the
legacy live tick; clustering alone costs up to 6x on seed 28. The readings and a recommendation (keep
the cap for now) are on NR-889. BL-1033 and sprint 44 closed.

**The queue ruled the same morning.** NR-889: the per-resource cap **scales with charter capital** on a
budget world (the deeper option over keeping it). NR-890: BL-1034 first in sprint 45. NR-886: coastal
cheapness and the trade re-base accepted as built; seat solvency moved to sprint 45's capital call;
the three red harness rows filed as BL-1035; the small-grudge fade documented in CIVILISATION.md; the
culture fold accepted. The review queue is empty.

---

## 2026-09-16 (late) — The queue emptied, the backlog cut, wave A built, and the rest archived

**Runtime:** Design (the review queue), Corpus (the backlog cut), then Delivery — Full as one batch of
twelve worktree lanes, stopped by Ben at the end of wave A.

### The review queue: fifteen calls in one form
Ben ruled every open entry (NR-807..884) in one elicitation form. The ones that moved docs: the
**industrial arc is the live product** (CONCEPT.md, ROADMAP.md) while the player identity was left to
its own ruling and raised as **NR-885** (the corpus says mercenary company, the seat work says
corporation); **COLONIAL_ERA.md folded and retired** to docs/research (claim verbs and sea lanes to
EXPLORATION.md, the search seed to DIGITISATION.md); **poverty is the brake** (NR-878); **a seeded
grudge fades by design** (NR-829), which exposed that RELATIONS.md credited campaign sentiment with
a job the generation-span carry does. The NR-877 re-bless was authorised and pinned.

### The backlog cut
Eighteen rows closed on a second form (four already delivered by later work, checked in code; BL-880's
seat-canvas ruling written into STARTUP.md, which still said "no corp-selection stage"). Then wave A
opened over the 16 items with no open prerequisite and no wall-clock measurement.

### Wave A: what the lanes found
| item | outcome |
|---|---|
| BL-1006 far-trade reading | 0.0 units sold at a destination on every seed: per-body pools send every same-body haul home |
| BL-1009 digest sees the money | 8/8 new fields proven visible; the old digest missed 7 |
| BL-982 Digitisation readings | 3 measured, 1 partial, 9 n/a; cities beat goods for firm density in only 6/16 worlds |
| BL-1023 Forest branch | tree 29 -> 36 nodes; arid and stone stated as hostile country |
| BL-1016 creed classifier | the raw compare picked a consolidator in 0/872 polities; by rank both kinds appear |
| BL-1017 culture fold | tree -85.5%, and nothing moved: one culture re-parented in 16 seeds |
| BL-1010 era_world reds | two stale checks; one real: no furnace lights in any 1960 world |
| BL-1022 coastal cost | coastal ground is ~15% cheaper to take, via garrison size |
| BL-1008 six harnesses | re-read on the 12-tick settle; two rows honestly red |
| BL-1020 seat floor | old floor empty on 7/16 curated seeds, new floor on 0; live click not run |
| BL-842 small grudges | min decrement above the floor; cold review found it correct |
| BL-1021 trade re-base | trade 0.19% -> 3.86% of production; R2 half-holds |
| BL-996 demand ladder | ARCHIVED: cold review found six issues; fix round stopped |
| BL-841 assimilation | REVERTED: the slot claim evicts large peoples on a one-unit gain |
| BL-1018/1019 alarm, first crossing | ARCHIVED: an unsaturated alarm collapses displacement (2.73 -> 0.50); finding kept in EXPLORATION.md |

**Cold review earned its place again.** Two of the three world-movers it read were sent back on
substance (BL-996, BL-841) after their lanes reported green. The digest also could not see BL-996's
economy change at all (the search winner moved on 3 of 5 seeds with every digest identical).

### The re-bless (32e04a19)
Causes: BL-1009 (coverage), BL-842 and BL-1021 (world). Over 16 seeds the Empires round fights less
(battles 158,215 -> 145,345) and the Exploration phase's shape holds (pooled displacement 2.73 ->
2.67, displaced seeds 12 -> 12). The R3b pin fixture nearly stops fighting (322 -> 30) from BL-1021
alone; it is the w_want_q = 0 variant and not the phase's reading.

### Stopped
Ben: *"This work is highly unstructured, so just leave it at wave A and we can reinvent anything
important later. (So archive)."* The 24 items of waves B-D were archived unbuilt; each design stands in
its authority doc. The calls left on merged work are **NR-886**.

**Found and not filed** (reinvent if wanted): raw leans still steer the Exploration tree's node choice
(the one-scale defect BL-1016 fixed in the sweep); sea legs sit at 580-820 on every polity, so the
landlocked consolidator never appears; material_floor's reconciliation row passes on nothing at its
default; spawn_solvency ticks with the player seated and skips the search; no non-SDL constant carries
the 12-tick settle; population centres are generated exactly on their rung, so one loss demotes them;
the Digitisation readings generate at epoch 0; the wizard's lineage palette (BL-1017) was never viewed
live. Harness hazard: lanes shared one scratchpad and one lane overwrote another's build script.

## 2026-09-16 (after the sprint) — Measurement parity, sixteen saved worlds, and an evening at the live app

**Runtime:** Delivery — Light, then a long live session with Ben watching the wizard. Everything
here was found by looking at the thing running.

### The sweeps were measuring a world nobody plays (BL-1007)

Two gaps, not one. `history_sweep` and `exploration_sweep` generated with `world_gen_config`'s
struct defaults where the app loads `scripts/world_gen.lua`, AND passed a null works registry,
which makes `build_work` a dead branch — no polity in a swept world had ever raised a work. Both
now load the shipped data layer the way `app::begin_new_game` and `ensure_works_loaded` do, and
print the configuration on their face.

| reading (16 seeds) | struct defaults | parity |
|---|---|---|
| seed 0 Empire battles | 6,253 | 11,824 |
| median battles / conquests | 6,762 / 5,843 | 9,932 / 7,913 |
| pooled displacement | 1.28 | 2.73 |
| silent seeds | 2 | 0 |
| Exploration battles per century | 28.7 | 69.3 |

**Sprints 40 and 41 tuned deterrence against a crippled world.** On the real data layer the phase
displaces conflict about twice as strongly as the reading Ben authorised. Both artefacts are
regenerated; the live app and the sweep now agree seed for seed (the wizard's own round 4 readout
says 11,824 battles on seed 0, which is what the sweep prints).

### Sixteen worlds saved for Digitisation

A seed IS the save — generation is a pure function of the descriptor — so what was missing was not
a snapshot format but the REASON a world is worth opening. `docs/generation/seed_library.json`
holds sixteen, chosen off a 48-seed parity sweep, each with the readings that made it interesting
and a five-counter fingerprint; `tools/session/seed_library.js` queries it (`--for`, `--seed`,
`--check`, `--bless`). They span what the next phase forms companies on: a median chest of 27.1M
(seed 46) against 245 (12), 112 polities (11) against fourteen (17), eight colonial subjects
(13, 41) against none (37, 4), thirty post roads with little trade (32) against no roads and
plenty (10).

### The live session, and what watching found

**BL-1000 closed on a real click** — menu to round 4 by presses, on a rolled seed. Worth recording:
three access requests were denied because the Start-menu entry for ProjectIo resolves to
`.claude/worktrees/elated-mclean-7dd61c/build/ProjectIo.exe`, a build from 14 September in a
worktree git no longer lists. It launched twice instead of the real build and briefly looked like a
regression in the board. Granting the running process by basename reached the right one.

Then five things Ben saw and asked for, in the order he saw them:

1. **The white event pings** fired several a year and drew the eye off the borders. Every one of
   fifteen region-carrying kinds drew the same ring, so a realm dying and a trade route opening
   were one mark. Removed entirely (`BL-1011`); the record, the ticker, the arc readout and the
   corridor overlays are untouched.
2. **The Exploration round's wait** read "Running the ancient era — year 397 of 460" while the
   ancient era is 1,600 years long: the stage label was last set at stage 8 and never moved. A
   fourteenth label is published before the Exploration pass; it now reads 613 of 1600 (`BL-1011`).
3. **Roads wrapping the whole view** (`BL-1012`). The world is a cylinder and both corridor bakes
   stored raw anchor columns, so a ten-column hop over the seam was drawn as a 250-column line —
   and the same columns fed the over-water sample, the bridge test and the exemplar's midpoint.
   Fixed at the bake; the draw strokes each corridor twice, a world width apart, inside the map's
   clip rect.
4. **A pace control** (`BL-948`), filed 2026-09-13 as 45/90/180. Ben chose 90/180/270 on the form,
   watched it and called it "much slower than I imagined": the rungs are 30 s / 1 m / 1 m 30 s,
   a minute by default. It sets wall clock for the whole span, so a longer span moves faster.
5. **Continuity between rounds** (`BL-1013`, then `BL-1014`). A round now opens on the ground the
   round before it left, cross-fading over the opening tenth of its own span, and it generalises:
   Empires carries the migration, Exploration carries Empires, Digitisation joins when it exists.

### Three fixes the continuity work needed, each found by watching

- The first cut gated the carry on the predecessor having polity SAMPLE steps. The migration record
  has none — its board shows dashes in People for that very reason — so the one hand-over the
  feature existed for was the one that could never fire.
- The second painted the carried frame only under UNCLAIMED ground. Right for Culture into Empires
  (400 BCE is nearly all unorganised); invisible for Empires into Exploration, where every realm
  already holds its land at 1200 CE. It now goes under the whole map.
- Ben: *"it looks like it actually carried over from culture."* It had not. `polity_slot` is a
  greedy graph colouring over each record's OWN adjacency, so the same realm got a different colour
  in each round and the boundary read as a different world. A shared polity id now inherits its slot.

### The reversal worth naming (BL-1014)

Ben: *"there is a clear phase where the simulation is done rapidly, and this sort of breaks the
narrative flow of the time-lapse... separate each part with an otherwise completely blank Loading X
Round."* That and the missing fade were ONE defect: a round that opens chasing the sim's frontier is
past its own cross-fade before anyone sees it. The wait is now one centred line and nothing else,
and the lapse plays from its first year. This reverses BL-914's "the wait is the round" for the pass
rounds, and `STARTUP.md` records it in his words.

### Also answered, in passing

Does the Exploration round really continue the Empires world, or start fresh? **It continues, by
determinism, but recomputes**: each wizard round calls the generator from scratch with the same seed
and stops at its own span, so round 5 replays the migration and the whole Empires era first. Proved
twice — a run that stops after Empires and one that continues into Exploration produce identical
Empires spans on all sixteen seeds, and live, round 5 scrubbed to 1200 CE shows round 4's realms in
the same order. The cost (four full generations by the time Begin is pressed) is `NR-811`, still open.

---

---

## 2026-09-16 — Sprint 42 wave 1: sixteen items, five that moved the world, and the measurements that outran them

**Runtime:** Full / Batch Delivery, continuing the same session. Sixteen worktree agents, one per item,
merged in dependency order on `sprint-42-wave-1`; every branch base-checked and every agent's claim
independently re-run in the main session before its merge was trusted. Two agents were stopped
mid-flight by a usage limit and were finished here from their own commits and snapshotted diffs.

**What landed.** The physical stages: the tile pipeline splits at the Body/Life boundary so the Life
half re-runs over a cached record (the 120-seed census re-runs in 13% of the time), planetology keeps
a per-epoch thermal series the fossil pass samples, and Pass 3 bands from the plate-carried position
so the present is the frame's epoch 0 rather than a lookup proved equal to it. The history stages:
rivers take the same corridor discount as the coast, fleets and standing armies pay a per-head bill
from the treasury, every node of the two wired trees now does something through one generic apply,
and every seat a polity holds folds its stores into the capital at 1200. The seam: nations open with
the money their history banked, the landscape search sees road tiers and rosters, and the warm start
is retired in favour of the search's own 12-tick validation run. Plus the readings — the held-seed
census, the ephemeral-culture count — and two doc items, one of which retired `COLLAPSE.md`.

**Five causes moved the world, each measured alone**, and `NR-877` asks Ben to authorise them as one
act: the thermal series (fossil magnitudes about a percent), the river discount (the migration reaches
inland 4,000-11,000 tiles earlier per world), the upkeep bill, the tree effects, and the nation
treasuries. Digests `457483363D79D700 / 728607C66CE6A4BE / F9BF05466A631FF9 / 851FE345B2E37618`
become `983298AE413B0A8E / F2A66ACE583F1784 / 5346EB2A9C4E1144 / 82EE79155BA2F47D`. The shape, over
16 seeds: the Empires span ends with more and smaller realms (powers 49 -> 55, largest share by people
110 -> 100 per mille) and slightly more fighting; the Exploration round is half as violent
(55.9 -> 28.7 battles per century) with displacement's median up (1.35 -> 1.70) and its pooled ratio
down (1.68 -> 1.28).

**Three measurements outran their items, and they are the session's real output.**

1. **Nothing in the scorer reads the purse** (`NR-878`). With the saturation caps gone the median
   polity ends the span with a treasury of 211 against 1.26M before, and 14% of polity-rounds cannot
   pay their army bill. At a tenth of the rate the median still collapses, so it is structural: the
   caps were hiding a 600x treasury spread.
2. **Four of the six held seeds have no frontier at all** (`NR-880`). They meet one polity across a
   landmass in 460 years, because the first crossing almost never happens. And alarm saturates at
   1000 on essentially every near pair, so the deterrence weight tuned in sprint 41 is acting as a
   flat constant.
3. **The sweeps and the app generate different worlds** (`BL-1007`). `history_sweep` builds with
   `world_gen_config`'s struct defaults while the app loads the Lua config: seed 0 fights 6,479
   battles in the sweep and 9,928 in the game. Every figure this sprint argued from is internally
   consistent and describes a world the player never gets.

**Gates on the integrated tree:** `world_determinism`, `pass_one_handoff`, `colonisation_harness`,
`deposit_origin`, `planetology_harness`, `continent_drift`, `tile_height_retention`,
`survey_endowment_harness` (34/0), `earthlike_tile_census` (120 seeds, 0 fail), `tree_lint` (four
stores, both headers fresh), `landscape_score`/`search`, `haulage_measure` (2035 / 1627 against the
1055/802 baseline), `demand_census` — all green. `history_sim_harness` sits at its 2-failure baseline.
Two reds stand deliberately: the exploration R3b pin, which is what NR-877 re-blesses, and
`era_world_harness`'s R1/R2/R5, which fail at the wave base too and are now filed (`BL-1010`).

**One integration break worth recording.** The Body/Life split and the plate-carried frame landed in
the same function from two worktrees; the merge was clean and the build was not, because Pass 3 now
needed a continents pointer the Body half's new signature did not carry. Caught by the gate run, not
by the merge.

**Owed:** BL-1000's live click (access was denied when requested, so the visual row stays open),
`BL-1007` measurement parity, `BL-1008` six harnesses still simulating an 80-tick warm start,
`BL-1009` the digest's blind spots — region treasury moved by 80% with every digest identical.

---

---

## 2026-09-15 — Sprint 42 wave 0: instruments before mechanisms, and the harness that paid for itself the same day

**Runtime:** Full / Batch Delivery, the same session as the audit below. Nine worktree agents,
one per item, briefed to fast-forward first and block on their own runs; the main session merged
each branch on `sprint-42-wave-0` after `agent_base_check` (9/9 PASS) and re-ran every harness the
agent cited before trusting the merge. A cold `code-reviewer` pass over the integrated diff found
eleven items; four were fixed here, two corrected the bookkeeping, the rest confirmed.

| Item | Landed | Verified on the merged tree |
|---|---|---|
| BL-981 (colonisation D1/D3 regression) | the schism verb demoted every seat in a residue block; seats now walk out as seats, block hinterland keeps its seat or re-points at the new one, parent ground whose seat left re-points at the parent capital | `colonisation_harness` 0 failures (2484/206, 2773/168, 1720/116) |
| BL-980 (corpus reconcile) | 8 contradictions corrected, 4 calls filed (NR-868..871), the 0.90 ratio deleted from code | `header_graph --strict` 81 → 81 dangling in clean worktrees |
| BL-974 (tree roots and header lint) | two stores reoriented (7 and 14 leaves had derived as root), one-root rule, header-matches-store check | `tree_lint all` OK, both headers byte-identical |
| BL-962 + BL-964 | endowment spread asserted (floor 40% after review); drift digest `481BDCA5300AF14F` and a cost ceiling | `planetology_sweep` OK, `continent_drift` ALL PASS |
| BL-966 (terrain economic gate) | census bands asserted; `survey_endowment_harness` (34 rows); `survey_endowment` exposed from the anonymous namespace | both green, census 120 seeds in 29 s |
| BL-970 (shares by population) | both columns on the face and in the JSON; scoreboard on population, region check kept beside it after review | `history_sweep 16` regenerated on the merged tree |
| BL-971 (displacement weighted) | pooled and volume-weighted beside the median; silent floor 20; `exploration_sweep.json` checked in | reproduced byte-identical pre-BL-981, regenerated after |
| BL-979 (instruments run the winner) | `apply_shipped_landscape` in `harness_params.hpp`; ten call sites; `fraction_in_band` | haulage on the winner **2183 / 1614** (seed candidate 1839 / 1411; baseline 1055 / 802) |
| BL-969 (validators in production) | both handoff validators on the shipped path; culture table crosses and is checked against `creed_state`; `save_game_version` 14 → 15 | `world_determinism` ALL PASS with R3.6/R4.4; `pass_one_handoff` ALL PASS; `save_envelope_roundtrip` PASS via the CMake tree |

**The one world-mover, attributed.** BL-981 alone moves seedA/on `49FB45407FF7C3C0` → `457483363D79D700`
(the reviewer's control build at the merge before it reproduces main's digests exactly); seedB/on,
seedA/off and the 1960 two-span are unchanged. `exploration_sim_harness` R3b (the exact-counter pin)
is red by the same cause: battles 306 → 308, conquests 304 → 306, foundings 522 → 472, owner changes
2174 → 2196. Not re-pinned. **NR-875 asks Ben to authorise the wave re-bless** against the shape:
seed 2 holds 116 seats where the razing world held 154, because breakaway realms keep theirs.
Exploration sweep on the integrated tree: pooled displacement 1.58 → 1.68, median 1.33 → 1.35, held
6 → 6, silent 2 → 1, Exploration battle rate 43.5 → 55.9/century. History sweep on the integrated tree: all 16 rows moved; medians top share 85 → 89‰ by regions and 101 → 110‰ by population, powers at 1200 CE 52 → 49, rose-and-fell 74 → 67 by regions and 22 → 22 by population, battle and conquest medians unchanged; ALL PASS.

**Findings that outrank the items.** NR-872: six of the ten varying endowments move together with
metallicity, so 55.5% of accepted homeworlds are poor in nothing. NR-876: by population the largest
polities are 2–5 points *more* concentrated than region count showed, while rose-and-fell counts
drop three times (74 → 22 per world) — the audit's "growth inflates the arc" premise was wrong on
concentration and right on shape. NR-874 records the design call taken in BL-981 (a schism moves
seats, it does not raze them).

**Review fixes made here.** `ownership_class` ran the search on an empty registry (now the stated
seed candidate); the population scoreboard row could pass vacuously with no playback record (both
columns now asserted, unrecorded fails); the planetology floor had 0.5 pp of headroom (40 now);
ERAS.md, MARKETS.md and GENERATION_STRATEGY.md had been reworded to build state by BL-980 and are
back to design state. All nine BL-979 instruments build on the merged tree; `ownership_class` ALL PASS on the stated seed candidate; `demand_census` PASS with pooled fraction-in-band 211 of 2335 (0.0904) industrial, identical to the agent's pre-merge figure — which, with haulage reading 2183/1614 both with and without the 0.90 ratio, also answers the reviewer's first finding: the deletion did not move the winner.

**Authorised the same day (Ben, 2026-09-15, on the form).** NR-875: the wave re-bless. `exploration_sim_harness` R3b re-pinned to 308 / 306 / 472 / 2196 (ALL PASS); **authorised digest baseline:** seedA/on `457483363D79D700`, seedB/on `728607C66CE6A4BE`, seedA/off `F9BF05466A631FF9`, 1960 `851FE345B2E37618`. The other eight calls: a schism moves seats (NR-874, into CREEDS.md); measure a cause per held seed before any constant moves (NR-873 → BL-999); accept the arc's shape and rank the wizard board by population (NR-876 → BL-1000, CIVILISATION.md); metal endowments share the metallicity axis by design (NR-872, into PLANETOLOGY.md); every seat folds into the capital at 1200 (NR-871 → BL-998, world-moving); the default epoch moves when Digitisation lands (NR-869, into DIGITISATION.md); COLLAPSE.md folds and retires (NR-868 → BL-1001); the breadcrumb is dropped (NR-870 → BL-1002). All nine entries archived. Sprints 38, 39 and 41 closed and archived on Ben's word; sprint 42 is the only hot sprint.

**What the builder bought.** Every agent's first harness build was 22 s cold and each rebuild
seconds; the regression BL-981 fixed had shipped through two sprints because the harness that
catches it cost 12 minutes on top of a clean compile nobody paid.

**Owed.** Six of BL-979's instruments were built but not run for their headline movement
(campaign_lapse, acquisition_viability, material_floor, chain_conversion_probe, convoy_cargo_census,
player_seed_sweep). `history_sim_harness` stays at its 2-failure baseline (R3a2/R3a3). Haulage seed
4 scores composite 0, so the search cannot move there — BL-977's to look at.

---

---

## 2026-09-15 — The generation audit: every stage delivers its struct and the next reads two fields of it; BL-960 lands, sprint 42 opens

**Runtime:** Design → Light delivery → sprint open. Ben asked, before Digitisation: do the existing
generation stages fill their promises, what tooling would make change cheaper, and will the chain end
in a working market — with two improvements per step.

**The audit.** Five parallel readers, one per stage group (physical stages; Culture and Empires;
Exploration and the trees; the political map to the market; the tooling loop), each checking the
authority doc's promises against the code, the harnesses and the sprint record. The verdict, stage
by stage: each stage delivers its own struct and the harnesses prove that well; the promises break
at the **handoffs**. The Exploration span writes treasuries, scarcity signals, trade flows and
preference into `exploration_output`, and the campaign economy consumes grudges and roads and drops
the rest into a test fixture; `nation_component::treasury` is 0.0 at generation. The shipped default
is `epoch_year = 0` with a history ending in 1660, so the industrial span, tariffs and ruptures never
run while GENERATION_STRATEGY.md says the campaign is the 1960 arc. No stage measures its economic
output until the end: deposit spread is printed and never asserted, chain completeness did not move
when 40% of reach was cut, the landscape search sees one live axis of three, and the warm start the
docs call retired still runs 80 ticks. The Empires arc is now present in the spread (31/32 seeds
eliminate polities and fall from a peak, 0/32 hegemony) but its share readings divide by a region
count that grows 2–6× inside the run. Exploration's displacement median is carried by one constant
pair, with two frozen seeds and zero-neighbour seeds dropped. Node effects in both wired trees reach
the sim nowhere; three nodes are hand-wired by index or string. Twelve doc-versus-code
contradictions were listed (peat rows, the plate serialisation claim, five stale contract rows,
COLLAPSE.md unbuilt, the "three terms" header, the warm-start retirement, the 0.90 ratio, the epoch
and pass map, the ledger breadcrumb, NEXT_SESSION, the README, the 1200 consolidation).

**Ben's call: A then B, then build.** File the improvements, build the shared harness library, then
return to the filed items in development mode — "sharpen each prior step before the instrumental
one (digitisation), where the generation becomes the world players see."

**Filed:** BL-960..BL-980 (two per stage, the corpus pass, the harness library). **Built:** BL-960 —
`build_harness.js` compiles the 62-TU world set once per configuration and links every harness
against the cached objects through a response file. Cold 22 s + 6 s link; unchanged rebuild 5 s;
one world TU touched 12 s. Before: 3.3 GB of the same objects 59 times over and a clean build per
edit. Objects are linked directly, not archived, so every world TU still reaches the link.

**The first harness through the new path found a regression.** `colonisation_harness` D1/D3 is red
on seed 2 (one dangling seat pointer), the exact shape closed at sprint 39; sprints 40 and 41 never
ran this harness. Reproduced byte for byte through the previous builder before attributing it to the
code. Filed as BL-981, first in sprint 42.

**Sprint 42 opened** — "generation sharpened before Digitisation": twenty-one items in four waves,
instruments before mechanisms, every world-moving wave re-blessed once. The perf CSVs and
`history_sweep.json` remain modified in the tree from an earlier session; untouched here (the sweep
JSON is stale against the harness schema, which BL-970 regenerates).

---

---

## 2026-09-14 — Sprint 41 (Exploration trade): goods flow, spend is chosen, displacement clears 1.0 — gated on authorisation

**Runtime:** long session, Design → Review → Full / Batch Delivery. Opened as a design question
("what levers ensure global trade, and what does Digitisation expect?"), became a 16-seed review of
the Exploration round, then a nine-item batch across four waves.

**The review that opened it.** `exploration_sweep` over 16 seeds showed the round busy but not
trading: the trade-access clause, the foreign scarcity reader and cultural preference had no caller
in the sim; ports, navies and armies were bought whenever affordable; nothing crossed the handoff,
and world setup read 1200 CE grudges and corridors onto a 1660 map. Displacement median 0.08.

**Design, settled on a form (Ben, 2026-09-14)** and written into `EXPLORATION.md` before any code:
trade is a want met by throughput, opened ONLY by the trade-access clause; trade value counts toward
a binding (the only thing that lets a distant pair bind); flow income replaces the flat market
income; a met want relieves the signal; an eleventh reading (Trade); spend is a scored allocation
inside upkeep (Ben chose this over new verbs). Three further calls taken on his behalf: NR-863.

**The bisect (NR-862).** BL-950's 0.88 starting point was a 3-seed median resting on one seed; the
wave-4 tree read 0.33 over 16 seeds, and the schism verb (BL-944, `3d58001d`) cut it to 0.08 by
changing the Empires world Exploration opens on. No reading went red because the sweep reports.

| Wave | Items | Landed | 16-seed displacement |
|---|---|---|---|
| 1 | BL-952, BL-951, BL-956, BL-953, BL-954 | sweep builds once; strength by treasury; `exploration_output` handoff + 1660 sentiment/roads; outward wants; trade flows | 0.08 → 0.10 |
| 2 | BL-955 | scored spend; paid standing armies persist (the muster had disbanded them every year — a sprint-40 defect) | 0.10 → 0.17 |
| 2b | BL-958 | sweep stops at the Exploration close: 16 seeds ~25 min → 99 s (Ben: build it before tuning) | — |
| 3 | BL-949, BL-950 | road rung recoverable (`history_corridor::tier`, resumed spans seed live uses) and bought (30M); deterrence Alarm 575 / far penalty 700 | 0.17 → 0.06 → **1.34** |

**Every wave was independently rebuilt and reverified here, and every wave had a cold review that
found real defects:** one want imported once per seller (shared); a holding sold to every buyer
(shared); treaty value double-counting the shared want (marginal); flows outliving their clause; a
squared proportional-loss formula; the campaign scorer pricing a levy the muster no longer raises;
an army cap reading only the capital. A `generation-dev` agent labelled two check blocks both "R6"
and one report named the wrong branch — both caught by checking, not trusting.

**Final state (16 seeds, `41e2f5c7`):** displacement 1.34 (1.34 also without the quiet seeds);
battle rate 44.8/century (Empires 404.9); 710 flows, 7.1% crossing landmasses, top-decile pairs
73%, every flow on a clause; post roads in 12/16 seeds; navies 454 → 189; consolidators in the top 3
still 0/16. **Cost:** seeds 0, 3, 9, 14 fight under 5 battles/century (lowest before: 11.5).

**Digests (not re-blessed):** seedA/on 584D731FC7E5DDD0 → CC34FD59D4E79580, seedB/on
1BFB3594DB6A319A → 728607C66CE6A4BE; seedA/off F9BF05466A631FF9 and 1960 851FE345B2E37618
unchanged. Harnesses: `exploration_sim_harness` 43 → 109, all pass; `history_sim_harness` at its
2-failure baseline (R3a2/R3a3); `save_roundtrip` OK. `story_check`'s 55 failures are pre-existing
dead traces to long-archived ids.

**Status:** 8 of 9 items complete and archived. **BL-950 (displacement clears the bar) is gated on
NR-867** — Ben authorises the world shape at Alarm 575, at the gentler 525, or not yet.

**Design-direction Q&A for Ben** (all in the queue, none blocking the merged code):
- NR-867 — authorise the world shape; four near-frozen seeds.
- NR-865 — wants plus trade did not point conflict outward (0.08 → 0.10); weaken the doc claim?
- NR-866 — spend saturation is a scorer brake; replace with a per-head upkeep bill?
- NR-864 — creed leans are on incommensurable scales; reading 3's 0/16 cannot yet indict the creeds.
- NR-863 — flow income at both ends; shared holding; holding as a share not a quantity.
- NR-862 — accept the schism verb's cut to displacement as a real force?
- NR-861 — treaties almost never break (5 in 6140).

**Filed for later:** BL-957 (world digest sees sentiment and roads), BL-959 (Empires corridor
upgrade records its uses).

**Authorised the same day (Ben, NR-867 and NR-865).** Alarm weight 575 -> **525**: displacement median
1.33, battle rate 43.5/century, quiet seeds 4 -> 2 (9 and 14). **Authorised digest baseline:** seedA/on
`49FB45407FF7C3C0`, seedB/on `728607C66CE6A4BE`, seedA/off `F9BF05466A631FF9`, 1960 `851FE345B2E37618`.
`EXPLORATION.md` § A want points a campaign outward became § A want ranks a campaign: wants rank,
deterrence, treaties and ports displace. BL-950 closed; sprint 41 complete. The release play build
(`build_rel`) was rebuilt at 525 for Ben to watch the round live before the remaining five calls.

---

---

## 2026-09-12 — Sprint 40 (Exploration) closes in full: six waves, fifteen items, one new phase

**Runtime:** long session, Full / Batch Delivery. Opened on a stale `NEXT_SESSION.md` (still
describing sprint 39, already fully closed) and a design branch (`claude/exploration-era-design-
e0f6d7`) sitting unmerged with sprint 40's full decomposition already on it.

**The merge.** The design branch predated main's sprint-39 close-out by one commit, conflicting
only in `backlog.json`; resolved as main's dropped-BL-928 state plus the branch's sixteen new
items. A parallel independent merge of the same conflict (by Ben, in the design branch's own
worktree) landed on GitHub moments later with the identical resolution — confirmed by diff, not
assumed.

**The six waves**, each built by a `generation-dev`/`ui-dev` sub-agent in its own worktree, then
independently rebuilt and reverified in this session (full app build, `exploration_sim_harness`,
`exploration_sweep`, `world_determinism`, `history_sim_harness`) before merging — no wave was taken
on a sub-agent's self-report alone:

| Wave | Items | What landed |
|---|---|---|
| 1 | BL-930, BL-931 | Exploration tree wired; the 1200→1660 span runs on the shared engine, opt-in (`exploration_sim_enabled`, default false) |
| 1.5 | BL-937 | The ten readings instrumented; 1-2 measured for real, 3-10 honestly scaffolded |
| 2 | BL-932, BL-939, BL-940 | Treasury, scarcity signal, corridor throughput — BL-930's stubbed scorer terms go live |
| 3 | BL-933, BL-934, BL-935 | Treaties with a term, colonies as subjects, ports/navies/armies with decay |
| 4 | BL-941, BL-942, BL-936 | Deterrence, creed-weighted strategy, cultural preference |
| 5 | BL-938, BL-943 | Industry tree ring-1 migration (not a trim — ring 4 survives whole); lapse-map fleet/caravan exemplars |
| 6 | BL-944 | The schism verb; the reassertion floor raised per Ben's call |

**The displacement reading's arc is the sprint's clearest signal.** BL-937's ratio moved
0.16 (wave 1.5, no mechanism) → 0.01 (wave 3, treaties suppressed both sides evenly, briefly
*worse*) → 0.88 (wave 4, deterrence finally split near-home cost from far-away cost) — exactly the
shape a phase-wide claim should take across a sprint that builds its cause incrementally: invisible,
then briefly wrong, then genuinely better once the real mechanism exists. Still not over the
harness's own bar (median > 1) — real progress, not a closed reading (NR-855).

**Fourteen NR entries filed** (NR-846 through NR-859) as things arose, not batched at the close:
a build-order bug in the handover's own wave table; an opt-in-default flag; a road-ladder rung that
never fired in any sweep across the whole sprint (NR-849); a treasury income-cadence reading;
displacement's non-movement then partial recovery (NR-851, NR-855); the creed axes not yet
separating consolidator/expansionist strategies on a small sweep (NR-856); a trade-province proxy
(NR-854); a one-commit-for-three-items deviation, twice (NR-853, and wave 4's equivalent); a
novel-work flag — the Exploration span's own events don't reach the lapse-map surface yet (NR-857);
a live-click-denied-so-verified-by-capture resolution (NR-858); and the Empire-span digest movement
BL-944's floor-raise caused, the first sprint-40 change not gated behind `exploration_sim_enabled`
(NR-859).

**One correction worth naming plainly.** BL-944's own backlog text claimed reassertion fired ZERO
times in 16 worlds. Re-measured before touching anything: it was already a thin but non-zero 90
fires/16 worlds, because sprint 39's unrelated changes had moved it. The floor was raised anyway
per Ben's explicit instruction, and the result (426 fires, 14/14 creed-worlds) was the right outcome
regardless of the stale diagnosis — recorded so nobody re-diagnoses the same "zero" claim from the
old text.

**Session close.** All fifteen designed items built, verified, archived. `BL-945` (depletion
retrofit) stays parked for Digitisation. Sprint 40 closed to
`archive/sprints-2026-Q3.json`. Backlog is empty but for the one parked row.

---

---

## 2026-09-11 — Sprint 39's session concludes: the full-tree reading, BL-928 dropped, levers accepted for sprint 40

**Runtime:** closing entry for the same session as the two below. Full / Batch Delivery close-out,
final step.

### The full-tree 16-seed reading, on top of the wave-1 reading already on file

The two entries below each carry a reading taken partway through integration (wave 1 alone, then
the wave-2/3 close-out that didn't re-sweep). This is the honest one: all fifteen sprint-39 items
together, 16 seeds, `--epoch 0`, against the pre-sprint main baseline (`6995b41e`):

| metric | pre-sprint-39 baseline | wave 1 alone | full tree (all 15 items) |
|---|---|---|---|
| powers at epoch | 42 | 42 | 59 |
| largest share | 11% | 21% | 8% |
| battles/world | 3114 | 689 | 6694 |
| conquests/world | 1812 | 311 | 5569 |
| secessions/world | 2 | 2 | 164 |
| regions ever conquered | 54 | — | 621 (of ~1340) |
| taken once & kept | 66% | 66% | 24% |
| taken 3+ times | 31% | 25% | 67% |
| worlds showing rise-and-fall | 15/16 | 7/16 | 16/16 |

Two things worth naming plainly, since they pull in opposite directions. Rise-and-fall is now
**universal** — every one of 16 worlds shows at least one empire forming and later falling, up
from 7/16 with wave 1 alone and 15/16 on the pre-sprint baseline: the "drama" the sprint set out
to deliver is unambiguously present. But TAKEN 3+ TIMES more than doubled to 67%, which
`CIVILISATION.md`'s own diagnostic reads as the BAD reading ("the same ground trading hands,
which logs conquests without ever moving the political map") rather than accumulation — worse
than the 31% baseline BL-924 was built specifically to bring down. Secessions went from a
baseline of 2/world to 164/world, roughly a fifth of the map. No single item's own isolated sweep
predicted this; it is very likely genuine compounding across BL-920 (many more, smaller starting
polities), BL-921 (bigger realms field disproportionately bigger armies, so a win snowballs
further before the next one), and BL-923 (anything overextended fragments hard) — three
mechanisms that each read as reasonable alone and as considerably more violent stacked.

**Ben's call, 2026-09-11: accepted as-is.** The levers for sprint 40 ("the new world," the
exploration/colonisation phase) are declared in place on this reading — no dial was touched, and
none is proposed here. The churn number is on record for whoever next reads this sweep, not
silently resolved.

### BL-928 dropped

`BL-928` (a river and a mountain defend, in the resolver and the scorer) was deferred at filing
on exactly this precondition — "before this we must ensure we can see strong growth" — which the
table above settles. Ben's call was not to take it up now regardless: cancelled, backlog now
empty, folded into "a future pass on history alongside tech" rather than filed as its own item.

### Session close

Backlog is empty. Sprint 39 is closed at fifteen items. No sprint 40 decomposition happened this
session — its own record (`sprints.json` id 40, "the new world") is still open and empty by
design, waiting on someone to read this table and start filing.

---

---

## 2026-09-11 — Sprint 39 closes in full: waves 2 and 3 land, eight items, one cross-item bug caught and fixed

**Runtime:** long session (continuation of the same session that closed wave 1 below), Full /
Batch Delivery, seven parallel worktree agents across two waves plus main-session merge/build/
verify/live-click. **Items:** BL-914 BL-917 BL-920 BL-921 BL-924 BL-925 BL-929 BL-923. Sprint 39
("the drama of the time-lapse") is now fully closed — fifteen items across three waves.

### Wave 2, six independent items dispatched in parallel

BL-914 (the tap mechanism: rounds render live instead of only after the pass finishes, Restart
retired for Pause+scrubber), BL-917 (promoted roads and river bridges drawn on the Empires map,
pure render, zero sim impact), BL-920 (the Empires opening reframed onto culture ground: city
states seed above a 300,000-head population threshold, a new ORGANISE verb grows them onto
reachable unorganised ground), BL-921 (a campaign's stack pools every held region's garrison
weighted by capital-read `network_supply_q`, so a 30-region polity now fields ~2.9x a 3-region
polity's stack at the same frontier — the structural fix for "no compounding"), BL-924 (region
value reads what stands on it, not just endowment, so a seat is worth attacking), BL-929
(a polity can spend its capital stockpile to deliberately upgrade a supply site). All six merged
clean or with small, expected conflicts (two new `sim_verb` enum cases, two new dispatch `case`
blocks) — every merge built and re-verified individually before the next.

### The one real bug this wave: BL-924 broke the shared scoring currency

BL-924's first cut folded its "what stands here" term directly into `region_value_q` — the
function `history_sim.cpp` documents explicitly as "THE COMMON CURRENCY... every verb scores in
ONE unit... 0-1000." Summing endowment and the new built/seat terms let it run to 3000, silently
rescaling the currency every OTHER verb (Settle, Consolidate, `build_work`'s reach gain, a
polity's own holdings-value sum) also reads. Cost `history_sim_harness` six checks — B318c,
BL384a, R5, B384c, BL837b1, M2b — nearly all "Campaign never fires" symptoms of a threshold read
against an inflated scale. Caught only because the wave-1 close-out had already established a
clean 2-failure baseline to compare against; the merging agent's own brief hadn't asked it to run
`history_sim_harness`, only `history_sweep` and `world_determinism` — an omission in this
session's own briefing, not the agent's error, now worth remembering for the next batch. Root-
caused by bisection (disabling the seat premium, then the built term, then scaling it) down to
the fact that Campaign's own target valuation never went through `region_value_q` in the first
place — it reads `w_farm`/`w_ore`/`w_port` against the target directly. Fixed by reverting
`region_value_q` to its original pure-endowment form and adding the prize term
(`campaign_prize_q`, a new `w_prize` weight) only at Campaign's own scoring site. Back to the two
pre-existing tracked failures (R3a2/R3a3) after the fix; the churn reading BL-924 was built to
move (seats' share of conquest, TAKEN 3+ TIMES) is unchanged in shape.

### Wave 3: BL-923, and a second, narrower cross-item interaction

BL-923 replaces BL-896's contiguous-block secession with city states, one per cut-off seat
(Ben's ruling on NR-837, reversing NR-826 call 2; call 3 stands). Building it exposed a second,
much narrower defect: BL-920's new "unorganised ground points at its geometrically-nearest seat"
pointer interacts with BL-896's old secession code, which demotes a seat's `is_seat` flag with no
knowledge of that new pointer type — `colonisation_harness`'s D1/D3 check (BL-866, "every
hinterland region shares its seat's nation") caught it as one dangling pointer on one of three
seeds. Fixed in two steps: first, a genuine, narrower regression from BL-920 itself (unorganised
ground correctly has NO nation to match, so the check's nation-mismatch test needed to exempt it
— this alone took `colonisation_harness` from a larger mismatch count to 1 remaining dangling
case); second, BL-923's own rewrite of the secession block closed the dangling case as a natural
consequence of replacing the code that caused it. `colonisation_harness`: 0 failures once both
landed. 16-seed sweep: breakdowns median 119/world (was 2), pieces overwhelmingly city-state
sized, and — the qualitative point BL-922 exists to make possible — most breakdowns now occur on
ground the reach model calls connected, not only graph-disconnected islands.

### Live-clicked the fully integrated tree

Opened a fresh build in the wizard and played rounds 3 and 4 to completion (1200 CE). Confirmed
live, not just in harness output: the map draws and animates from the moment each round arrives
(BL-914) with no Restart button; kin cultures cluster by hue (BL-919, wave 1); the ticker
narrated a broke-away line (BL-923) and two cross-border trade openings (BL-925) by name and
year; the board carries Land/Rgn/Pop/Mt; the map shows terrain relief, rivers, and the promoted
road network (BL-915/917, several sessions' worth of rendering work all present at once).

### Bookkeeping

All eight items' `req/requirements.json` rows flipped complete (BL-929's render row marked
`partial` — the mechanism and its event/ticker feedback are live, but a bespoke waystation glyph
and a road-stroke distinct from BL-917's own were cut for scope, left as a follow-on) and
archived; all eight backlog rows flipped complete with resolution prose and evicted; `REFINED.md`
emptied — sprint 39 carries no more active work. `backlog_lint.js` 0 fail(s).

### What's next

Sprint 39 is closed. The 16-seed reading recorded in the wave-1 entry below — largest share
roughly doubled, rise-and-fall shape now appearing in fewer worlds rather than more — was taken
before waves 2/3 landed; a fresh 16-seed sweep on the fully-integrated tree (all fifteen items
together) would be the honest next reading before any tuning conversation, per
`GENERATION_STRATEGY.md`'s "measured, not argued" rule. No sprint 40 planning happened this
session.

---

---

## 2026-09-11 — Sprint 39 wave 1 closes out: seven items verified, re-blessed, and a 16-seed reading for Ben

**Runtime:** long session, Full / Batch Delivery close-out (no new build this session — wave 1's
seven items, BL-926 BL-927 BL-918 BL-919 BL-915 BL-916 BL-922, were already merged to main by the
prior session; this one runs the close-out checklist `NEXT_SESSION.md` left owed). **Items:** BL-926
BL-927 BL-918 BL-919 BL-915 BL-916 BL-922.

### Close-out checklist, run in full

`build_app.bat` BUILD_OK. Re-built and ran every harness the handoff named: `world_determinism`
(ALL PASS, digests moved as expected on BL-918/BL-922 — recorded below), `save_roundtrip` and
`save_envelope_roundtrip` (both green, save format 13), `history_sim_harness` (down to the two
pre-existing, tracked failures R3a2/R3a3 — both about far-objective under-supply, which is exactly
the mechanism BL-922 changed, so their persistence is expected, not a regression), and
`colonisation_harness` (0 failures). A `verifier-review` static pass and a separate cold
`code-reviewer` adversarial pass (author ≠ reviewer, correctness-focused: overflow, sentinel bugs,
serialisation symmetry) both came back clean — no blocking findings, two non-blocking hygiene
suggestions noted for later. Live-clicked the wizard's rounds 3 and 4 in a fresh `build/ProjectIo.exe`
(this worktree's own Debug build, not the stale `build_rel` the handoff named — equivalent evidence):
confirmed rivers, terrain relief, seat markers and frontier lines on both maps, kin cultures sharing
hue families on round 3, and the event ticker plus Population/Might board columns on round 4,
animating live from 44 BCE to the 1200 CE epoch.

**One authorised `world_determinism` re-bless for the whole wave**, per the handoff's instruction
(never per item): seedA/on `DA6F5E1E50BC9A68`, seedB/on `A78B4BB975568091`, seedA/off
`F9BF05466A631FF9`, 1960/two-span `1785663397FA92CD` — all reproduce bit-identical across two runs;
ALL PASS, 0 failures. (The wave's earlier BL-922-only re-bless recorded in `NEXT_SESSION.md`,
seedA/on `5A641C67838E8B54` etc., is superseded — BL-918 moved the digest again on top of it.)

### The 16-seed reading Ben asked to see before any dial moves

Per `GENERATION_STRATEGY.md` § The asymmetry is political, this is reported, not acted on.
`history_sweep 16 --epoch 0` on the wave-1 tree, against the pre-wave main baseline
(`6995b41e`: powers 42, largest share 11%, battles 3114, conquests 1812, secessions 2, reach
refusals 0, rose-and-fell 2/world in 15/16):

| metric | baseline (main) | wave 1 (16 seeds) |
|---|---|---|
| powers at epoch | median 42 | median 42, range 20–72 |
| largest single share | median 11% | median 21%, range 12–32% |
| battles per world | 3114 | median 689, range 57–2023 |
| conquests per world | 1812 | median 311 |
| taken 3+ times | 31% | 25% |
| secessions per world | 2 | median 2 |
| REFUSED reach gate | 0 | median 10,471 of ~207,935 contacts examined |
| worlds showing rise-and-fall | 15/16 | **7/16** |
| rose-and-fell, median | 2/world | **0/world** |

The two-seed integrated reading the prior session took (largest share 30%, battles 844, reach
refusals 28,921 combined) sits at the aggressive end of a much wider 16-seed spread, not the
centre of it — battles alone range 57 to 2023 across seeds, a 35× spread. The headline tension:
**largest share roughly doubled while the rise-and-fall shape Ben asked sprint 39 to deliver
("drama") appears in fewer worlds, not more** (15/16 → 7/16). `terrain_reach_cost_q` (currently
4000; 3000 refuses far fewer contacts per the BL-922 agent's own measurement) is the one number
`NEXT_SESSION.md` names as the candidate to shift, but no dial was touched this session — the
finding is filed for Ben's read, not resolved.

### Novelty and decisions taken, filed

- **NR-840 (novel-work):** `culture_kinship_years` tested `year < 0` for "undated," but every
  culture in the migration is coined BCE, so the function returned −1 for every pair on every
  world since BL-870 — the kin-opposition discount has never fired in a real run. Fixed in the
  BL-918 commit (sentinel is now `INT64_MIN`) because the split census could not proceed without
  it; recorded separately because BL-918 didn't ask for it.
- **NR-841 (decision taken):** the wizard's Culture/Empires preview surface never ran the river
  pass; BL-915 needed it to, so `generate_home_surface_preview` now runs `generate_rivers` with the
  campaign's own seed formula. A generation-layer change made inside a UI item's scope.
- **NR-842 (decision taken):** round 3 under `--verify` used to silently replay the Empires sim's
  polities instead of the migration; BL-919 needed the real migration record for the lineage tree,
  so the adopt path now calls `build_migration_timelapse` for round 3 specifically.
- **NR-831 resolved:** BL-927 moved the `w_aggr_q` lean outside the season loop, confirming the
  compounding this entry flagged was real and unintended; battle/conquest medians unchanged by
  this item alone.

### Bookkeeping

All seven items' `req/requirements.json` rows flipped complete with result metrics and archived to
`archive/requirements-2026-Q3.json`; all seven backlog rows flipped complete with resolution prose
and evicted to `archive/backlog-design-2026-Q3.json` via `archive_landed.js` (verified byte-exact
round trip); `REFINED.md`'s wave-1 task blocks are stale now that the rows are archived (left in
place — wave 2/3 blocks below them are still live); `backlog_lint.js` 0 fail(s). A `mirror_check.js`
run separately found `docs/ai/ACTIONS.md`/`ACTIONS_INDEX.json` stale against a change from an
earlier, unrelated commit — reverted out of this batch and spawned as its own follow-up rather than
bundled here.

### What's next

Wave 2 (BL-914, BL-917, BL-920, BL-921, BL-924, BL-925, BL-929) and wave 3 (BL-923) are briefed in
`NEXT_SESSION.md` (now stale on the close-out section, live on the wave-2/3 briefs) and
`REFINED.md`. The 16-seed table above is Ben's to read before any of `terrain_reach_cost_q`,
the reach-gate constants, or the aggression/fear lean magnitudes move again.

---

---

## 2026-09-11 — Sprint 38 closes for real: the whole closure contract wired, twelve items, and the phase pronounced too lively

**Runtime:** very long session (survived one crash and a full resume), Full / batch delivery,
heavy parallel sub-agent fan-out. **Items:** BL-891 BL-901 BL-902 BL-903 BL-904 BL-905 BL-906
BL-907 BL-908 BL-909 BL-910 BL-911 BL-912.

### What landed

**BL-906 first, alone, because everything else depended on it.** Pass 1's stop year decoupled
from the epoch (`world_params::empires_stop_year`, default 1200) — the Empires phase now runs its
full 400 BCE → 1200 CE, not the 400-year slice it had been silently running. Every sprint-38 figure
re-read at full span: hegemony 0/16 unchanged, largest share barely moved (median 11% vs 12%),
wall clock roughly quadrupled as warned.

**Then the closure contract, in two waves of parallel agents.** `CIVILISATION.md`'s seven readings
— explorer set, strength spread, contact, directed wants, markets, inherited network, grudges —
are now ALL live in `history_sweep`'s report, each measured over the 16-seed spread with no target
numbers set. `BL-908` (contact, the grudge table's shape reused for a different cause) and `BL-910`
(capitals/markets, a pure read of existing seat state, no new placement pass) landed wave one, in
parallel with `BL-901` (a culture that crossed water now carries that fact into sea legs), `BL-902`
(harness fixture repointed at real generation instead of a strip that stopped producing war),
`BL-904`/`BL-891` (the wizard's footer was never actually unreachable — a scripted-walk limitation
plus a genuine smaller layout undercount), and `BL-905` (below). Wave two closed the loop: `BL-909`
(directed wants, gated on contact, no price) and `BL-912` (the empire tree wired into the sim —
real per-polity state, a deterministic scorer, a fork that closes for real, the rim milestone
crossing the handoff) both required wave-one's output and both landed clean. `BL-907`'s scoreboard
was built partway through and two of its seven readings were re-wired from stale MISSING
placeholders as their sources landed on top of it in the same wave — the same bug class as a stale
UI string caught live in the wizard the same session (see below).

**BL-912 found a real bug in itself.** The empire tree's root node was permanently unlockable
under the first cut's symmetric neighbour-mask construction — every child that named the root as a
prerequisite fed a "needs a neighbour held" requirement back onto the root itself. Caught by the
sweep showing 0 nodes bought at any span before the fix, 8–51 after.

**Twelve of thirteen items shipped; the thirteenth taught the most.** `BL-903` (a communication
rung, gating a polity's ability to act on ground it cannot hear from) was built exactly to its
settled spec and measured `REFUSED comms gate median 0` on every seed — not a tuning miss but a
structural one: Campaign's own candidate-generation loop already only ever enumerates directly-held
border ground, so the gate's first condition is true by construction for every candidate it ever
sees. Reverted rather than shipped as a no-op (`NR-833`), and retired outright once Ben read the
finding — the candidate-generation widening that would make it real is a bigger, separate item.

### The finding that outranks the rest

`BL-905`, independently converging with `BL-903`'s dead end: the reach gate — `CIVILISATION.md`'s
named PRIMARY LEVER — refuses campaigns via a **step function, not a gradient**. Probed the
`sustainable_campaign_floor_q` directly: 250 refuses 0, 700 refuses 0.1%, 999 refuses 94% and
collapses battles 815→34/world. No value threads the needle, because `campaign_supply` prices from
the staging hub rather than the capital, so an ordinary neighbour-adjacent march's distance term
barely decays the currency. `CIVILISATION.md` was corrected to name what's actually filtering
campaigns today (score threshold and verb competition, not reach) rather than have a constant
re-guessed against it. This is a real design decision, not yet taken.

### Ben's verdict, live

Ben reviewed the phase running in `build_rel` and called it: **"really lively, and perhaps too much
so… I think this would work as a precursor to the next phase of generation, but I want to tighten
some levers."** Sprint 39 was renamed from its previous placeholder ("the new world," moved intact
to sprint 40) to **"tighten the levers"** — the tuning pass that follows sprint 38's now-fully-wired
mechanism, starting from `BL-905`'s reach-gate finding and `NR-833`.

### Investigated and closed: NR-834

`BL-909`'s build hit an intermittent, unreproducible-by-inspection crash in `history_sweep`, fixed
empirically by switching two lookups to bounds-checked `.at()` without the actual mechanism ever
being pinned. Static read: every index BL-909 touches is either loop-bound-guaranteed or explicitly
range-checked before use, so the `.at()` calls cannot actually throw as written — itself evidence
against a real logic bug. Reverted to plain `operator[]` and ran a single ISOLATED 32-seed sweep
(no concurrent harness) — ALL PASS, no crash. Most likely cause: this session's sandbox spawning
duplicate/concurrent harness processes against shared output state, independently reported by the
`BL-907` agent the same session — not a real out-of-bounds read. `.at()` restored anyway as cheap,
reasonable defensive bounds-checking, and `NR-834` closed.

### What went wrong, worth remembering

**A worktree agent's stale base cost real integration risk.** `BL-912` (the empire tree, difficulty
6) branched from a commit 9 behind main despite being explicitly briefed to fetch and fast-forward
first — it built the whole feature against a base with none of `BL-908`/`910`/`911`'s new fields.
Auto-merged clean by luck (the additions landed in different regions of the same files), but this
is the `BL-480` shape and it will not always be luck. The main session's independent re-verification
after every merge — never trusting an agent's self-reported PASS — is what caught it in time to
matter here rather than after.

**The session itself crashed mid-batch**, losing three just-launched agents (`BL-907`, `BL-909`,
`BL-912`) before they had made a single commit. Checked their worktrees for partial work first
(none — clean bases, zero commits) before relaunching fresh rather than trying to resume an unknown
crashed state.

**Two stale-copy bugs, both caught only by looking, not by a green harness.** A wizard disclaimer
insisting the Empires round was "not yet the full 1,600 years" kept asserting that after `BL-906`
made it true — caught live-clicking the wizard, not by any script. `BL-907`'s scoreboard carried
two hardcoded MISSING readings that went quietly false the moment their sibling items landed in the
same wave — caught by rereading the report after the merge rather than trusting the build-time
snapshot. Both are the same lesson `io-same-day-ruling-orphans-siblings` already names: a fact
asserted at one moment does not update itself when the ground under it moves.

### Where the backlog stands

Zero open items. `docs/development/sprints.json` now carries sprint 38 (open — the phase's
mechanism is complete but Ben has not called it closed), sprint 39 (open, "tighten the levers,"
empty — the tuning items are not yet decomposed), and sprint 40 (open, "the new world," unchanged).

---

---

## 2026-09-11 — Sprint 38 closes the phase: seven items, five agents, and a lever that refuses nothing

**Runtime:** long session, Full / batch delivery. **Items:** BL-823 BL-838 BL-839 BL-887 BL-893
BL-895 BL-896 BL-897 BL-898 BL-899.

### What landed

**The economy of war closed (`BL-895`).** Materials had no sink but campaigns — hundreds against
hundreds of millions produced — so the trade income shipped the session before could not gate
anything however large it was. Two sinks now compete for a realm's stock: a standing army eats
every year, with the unpaid share of a garrison walking home; and a corridor promotion costs the
acting seat, a refused one held one short rather than discarded. The magnitude was chosen against a
**stated target** rather than by feel — at 200 per 1,000 heads the sinks claimed 63% of production
and refused 324 road builds a world, which is poverty governing the network; at 20 they claim 8%
with 43 refusals.

**Collapse became mechanical (`BL-896`).** Ground whose reach from its own seat falls under a floor
**secedes** as a contiguous block rather than falling to a neighbour — a successor with real ground,
carrying its new seat's own culture and its parent's ladders. Median 2 secessions and 14 regions a
world, arc intact.

**The two creeds (`BL-897`, `BL-899`).** Ben settled eight design calls in one form. A universalising
creed is coined new and belongs to nobody, arises from humiliation *and* density together, and
splits the institution from the people so the two can disagree — 2 of 16 worlds, which is the spread
the item demanded. Sea legs are a reduced ration scaled by how seafaring a people is, earned from
facts already recorded; 405 crossings now land fed where none did, which finally moved `BL-893` from
"changes no outcome" to a real one.

**Grudges became consequential without becoming an agent term (`BL-898`).** The item's original
design said a grudge must change who a polity campaigns against — precisely what `BL-827` ruled out.
The corrected route seeds nation-to-nation sentiment at the handoff instead, and `history_sim.cpp`
is **unchanged by the diff**, which is how the claim was verified rather than asserted.

**And the dangerous lever, built in the admissible form (`BL-838`).** A realm is attacked for what
it did to peoples like the decider's own, never for its size. It needed a dated widening of the
AI-behaviour prohibition, raised rather than assumed — the rule it moves is written at the field
itself. The scope is asserted on a built ledger where **the peaceful polity is the larger one**;
clearing the ledger with nothing else changed drops both to zero, which a rank term could not do.

### The finding that outranks all of it

Three agents, working different items with no contact, independently hit the same line:

> `REFUSED reach gate   median 0`

`BL-823`'s own resolution promoted reach-gating from one lever among six to **the** primary lever,
and `CIVILISATION.md` is built on that claim. It refuses nothing. That explains two null results at
once: `BL-887`'s centre-chain relay measured no movement because cheaper reach can only unlock
ground a price was keeping shut — tripling the rebate reproduced the figures *exactly*, which is
what turned it from a tuning question into a finding — and `BL-839`'s turbulence lean pulls three
forces of which two are inert. `BL-905` carries it, with the explicit instruction not to raise a
floor until the cause is measured.

### What this session got right, and what it got wrong

Right: **every null result was reported as one.** Reassertion fired zero times in `BL-897` and was
not tuned into firing; no launched crossing starves in `BL-899` and the floor turned out redundant
with the port gate; calm and turbulent worlds are indistinguishable in `BL-839`. Each is recorded
with its cause rather than smoothed.

Wrong, and worth remembering: the saved implementer agent definitions carried **prose where a tool
list belongs** (`tools: "All tools except Agent"`), so every spawn came up with `Agent` as its only
tool. That is the "subagents were unusable" note from the previous handoff — a config fault, not a
brief problem, and it cost two wasted spawns before it was diagnosed. Fixed, but definitions are
cached at session start, so the five slices ran as `general-purpose` instead.

Also wrong, and mine: a merge resolution in the sweep report left one brace too many. The compiler
caught it immediately and it never went anywhere, but it was the integrating session's error rather
than any agent's.

### Left open

`BL-891` is the only item blocked on something a session cannot do alone: the arc readout renders
and reads well on a headless capture, but the scripted walk cannot reach round 4, and the verify API
has **no scroll verb for the wizard column**, so no script can test the path a human would take.
`BL-904` owns that, including the discovery that the verify window reports 1720×1080 while captures
come out 1920×1080 — if those are different spaces, every wizard coordinate ever read off a capture
was read in the wrong one.

### How the session actually ended, which matters more than what it built

Ben closed it by naming the thing none of the ten items addressed: *"we are still yet to work on the
output data, and what our age of exploration expects to find after the age of empires."*

That reframes every null result above. `BL-887` moved nothing, `BL-839`'s forces are inert,
`BL-838` didn't shift the figure it was written against, and `BL-905` found the primary lever
refusing zero campaigns — four shrugs in one session. The common cause is not any of the
mechanisms: it is that **nothing downstream asks pass 1 for anything specific**, so no mechanism can
be judged, and a lever that stopped firing was noticed by nobody because no consumer would have
missed it.

The next block is therefore a design session on the **pass 1 → pass 2 contract**, not more
mechanism. Items stay open deliberately; the handoff says do not open by tuning.

One good report from the build: the round-4 time-lapse — `BL-817`'s record, `BL-830`'s scoreboard
and `BL-891`'s arc readout together — **looks great**. That is the surface this whole phase is read
through, and it is working.

---

---

## 2026-09-10 (sprint 37 closes) — The seven owed items, and a delegation bug caught mid-flight

**Mode:** Full, batch delivery, closing a reopened sprint.

Delivered sprint 37's full owed list: BL-849 (province partition takes settled cells as a hard
input, via a settlement lock mirroring the existing nation lock), BL-852 (fragmentation now
derives from culture-contact interpenetration, retiring the kinship-blind tribal marches), BL-853
(the sim's shared terrain view carries rivers — turned out narrower than filed, since production
colonisation already had them via BL-857), BL-854 (B384a retired per Ben's ruling — capability
carries to the history round, not asserted in the migration round), BL-855 (region adjacency
degree-capped at 10, bounding the graph BL-844's heap fix could not reach), BL-859 (cancelled,
redirected to BL-888: narrowed the polar/subpolar latitude bands, shrinking ice-cap extent ~36%),
BL-861 (measured rather than fixed directly — resolved as a side effect of the other four,
confirmed via `history_sweep` across 16 seeds; later found superseded by sprint 38's own
independent resolution of the same pathology, reached first with Ben's ruling — NR-824).

**A genuine tooling failure cost real time, caught and reported rather than worked around.** The
`generation-dev` sub-agent, given the Agent tool via a blanket "All tools" grant, recursively
spawned further sub-agents instead of implementing directly — eight runaway agents made zero
commits before this was caught and killed via `TaskStop`. A same-session self-correction agent,
launched without worktree isolation by mistake, then discarded a set of legitimate uncommitted
edits from the *shared* worktree along with the duplicate's — caught immediately via the
on-disk-change warning and redone before anything was lost. Filed a background investigation task;
Ben (in a separate session) fixed the root cause mid-session (`Agent` removed from
`generation-dev`/`economy-dev`/`ui-dev`'s toolset, commit `eb3c1186`). A second wave of agents,
post-fix, still hit assorted glitches (one losing all tool access mid-task, one resuming into a
tools-less spawn) but did real, mergeable work before stalling — every stall was recovered by
treating the stalled agent's worktree as a diff to inspect and finish directly (build it, run its
own stated verification, commit) rather than trying to "resume" it (no such tool exists in this
environment; a second Agent call is always a fresh spawn in a fresh worktree).

**Honesty over a clean number, twice.** BL-855's own measured result did not cleanly hit its
stated "flat ms/rebuild" criterion at the larger region counts this run reached — reported as
such, with NR-825 filed rather than the number quietly rounded up to a pass. BL-888's
boreal-share-of-unfarmed-land number moved more modestly than its raw tile-incidence number, for a
real and stated reason (freed ground reclassifies into other marginal cold-adjacent classes, not
farmland) — also reported plainly.

**Merging surfaced concurrent sprint-38 work.** Root `main` had moved 2 commits ahead (a backlog
purge/consolidation, plus sprint 38's own independent resolution of the no-conquest pathology and
BL-868) while this session worked in its worktree. Merged cleanly with real conflict resolution in
`backlog.json`/`sprints.json`/`NEEDS_REVIEW.json` (one NR-id collision, renumbered), rather than
force-pushing over either side. `build_rel` rebuilt and confirmed working after the merge.

**Runtime:** several hours, spanning a sub-agent-heavy delivery pass, a debugging detour into the
delegation bug, and a merge/build/verify pass. Items: BL-849, BL-852, BL-853, BL-854, BL-855,
BL-859 (cancelled), BL-861 (cancelled), BL-888 (new). NR-825 filed and open.

---

---

## 2026-09-10 (sprint 38) — City states become empires, eight of nine

**Mode:** Full, batch delivery.
**Runtime:** one extended session, largely parallel sub-agents in separate worktrees; the main
session merged, built, verified, and reconciled backlog state at the end.

### What started it

Ben: mint backlog items for the Empires round out of the CIVILISATION.md design (closed
2026-09-09), mirroring how the colonisation wizard stage presents, and batch-deliver the whole
chain — "if there are problems that come up, or if you have any questions, just ask."

### What landed

Eight of sprint 38's nine items, all verified on main: BL-873 (culture coining year), BL-871
(empire span split, 400 BCE→1200 CE, so Culture and Empires stop running the same simulation),
BL-866 (sparse settlements — seats and hinterland pointers, conquest carries a whole hinterland
at once), BL-837 (ancient roads — a heavily-walked edge gets cheaper, campaigns beyond
sustainable reach are denied outright), BL-867 (materials spent on action — labour splits
subsistence/industry/muster, a captured seat carries its material stock rather than resetting),
BL-870 (culture relations — opposition PERMITS conquest rather than forbidding it, corrected
from an initially backwards formula), BL-869 (civilisations from mixing — named records with an
ethic, gated by an opposition bar so estranged cultures never coin one), and BL-872 (centres
from supply/governance — a population centre grows only where the road network can still feed
and rule it; cut-off ground freezes rather than razes).

Several caught real defects before shipping: an integer-truncation bug that zeroed campaign
material cost under 250 raised heads; two directional inversions relative to CIVILISATION.md's
stated design (opposition's discount ran backwards; a test fixture skipped `draw_region_urban`
and compared against the wrong baseline); a test seed that happened to land on a peaceful world.

### What didn't

BL-868 (creeds raise armies — a culture's `aggression_q` should lean the Campaign score) failed
verification seven times running. The wiring has looked correct since the first attempt; every
failure has been in the TEST — vacuous metrics, calibration outside default ranges, a genuine
confound (a symmetric arms race between two aggressive cultures can end a war EARLY, logging
FEWER campaign events for the more warlike world, not more), and finally, once that confound was
fixed, a fixture that no longer fights at all under BL-837/BL-872's reach gate and supply floor —
both landed after BL-868 was first designed. Left open; see NEXT_SESSION.md.

### What was found late

BL-867's backlog record was still unmarked days after its code landed on main (`b25db602`) — the
delivery commit happened, the bookkeeping commit didn't. Caught and fixed during this session's
close-out; a reminder that the last step of a batch delivery is as easy to drop as any other.

### What is open

BL-868 (above). BL-887 (reach-as-centre-chains) was filed and deliberately deferred — Ben's own
call: too few small polities survive Round 4, and a "new world" should remain for a future
exploration/colonisation phase, but the fix (chains of population centres, Logistic Points)
waits on tech progression being wired into generation first. BL-861 and BL-823 both need a
re-scoping pass before implementation; their prose predates this sprint's mechanics changes.

---

---

## 2026-09-10 (sprint 40 opens) — Three trees, one grammar

**Mode:** Design, out of order — sprint 40 opened ahead of 38 and 39 on Ben's call.
**Runtime:** one session; two sub-agents authoring disjoint doc+JSON pairs in place, the main
session authoring the grammar, the lint, the Empire tree and its scorer.

### What started it

Ben's sprint-40 brief: design specific technology trees for Empire, Colonisation and Industry,
using the placeholder trees for inspiration, and consider how generation produces research points
and what motivates a polity to pursue a branch. The first assessment found three tech objects and
only one running — the sim held seven capacity bands and drew a picture of a tree beside them —
and that the three trees Ben named are the three simulated spans, each with a different way of
acquiring a node.

### What was settled

Ben's calls: three trees, not one web; the sim reads nodes, not bands; each tree its own doc and
its own JSON; nodes are minor, major and milestone, with milestones unlocking the next tree; a
central spire branching outward. Then the five grammar proposals, agreed as put: the sizes with a
64-node cap (one 64-bit mask per tree, the `works_built` precedent); forks as majors with
`excludes`; minors diffuse free by contact, majors by their class, milestones never; a milestone
needs two branches at its own ring; home `docs/generation/trees/`.

### What was written

`TREES.md` (the grammar, the five lintable adjacency rules, the scorer shape, the JSON schema,
where research comes from before a university exists), then three docs and three stores:
Colonisation 29 nodes carried by time on ground, Empire 52 nodes with `pursued_when` on every
node, Industry 62 nodes with four forks. `tools/session/tree_lint.js` enforces every rule and
cross-checks doc and store both ways; all three pass. The Empire scorer is specified per node —
the term that makes it the top pick and the state of the world in which that happens — with
thirteen terms, each a reading of state the sim already carries.

Two grammar additions taken on Ben's behalf while authoring and recorded to be overturned:
`requires_fork` (either side) and `requires_any` (any N of a set), because a carried tree cannot
demand two named branches of a people coined on one ground (NR-820). The old ladder is kept as a
calibration reference under a superseded banner rather than deleted (NR-821).

### What is open

Twelve farm classes in the classifier against four Colonisation branches (NR-819). Every
magnitude in the three stores is a placeholder; BL-886 (tree sizing sweep) prices them. How a JSON
store reaches a Lua-free sim is BL-881's first decision. Six items minted, BL-881..BL-886.

---

---

## 2026-09-09 (sprint 39 design) — The colonial round sets the demand, and the corporations come after

**Mode:** Design, while sprint 38 runs. No code.
**Runtime:** one session; one background agent measuring generation cost at `/O2`.

### The question

Whether to approach markets directly from where the empires left them, or to run a colonial
precursor first — and whether either can be generated fast enough that the wait does not bore.

### What was settled

Both, sequenced: **pass 2 thin, then pass 3.** The colonial era (1560 → 1960, on the polity engine)
claims ground across water by **purchase** or **conquest**, records **who discovered which luxury
good**, derives **how wealthy each nation is**, and leaves **sea lanes** on the map. Then the static
search selects a corporate landscape over that world, seeded from the strength of the trade
network. Two wizard rounds — 5 the colonial era as a **still**, 6 the corporations — because one
round doing both "complicates the story for the user". Authority: `docs/generation/COLONIAL_ERA.md`
(new); items BL-874..BL-880; sprint 39 proposed.

### What moved the design

**The tie's consumer was dormant.** `MARKETS.md` § Where the order book lives: no press and no
`corp_verb` submits a buy order, so the preferred-seller routing the design had seeded ties into
runs for nobody. The tie became a **sea lane** — a stamped discount on sea-leg traversal cost, the
water analogue of the ancient roads — read by convoys, reach and placement alike because traversal
cost is one weight function. Wider than a market preference, and intended.

**The endemic channel was the consumer the demand output needed.** `inject_endemic_demand` already
injects a wealth-scaled, character-flavoured want; the colonial era moves its weights, and "set
demands for goods" is a mechanism rather than a noun.

**The seat is a choice again.** Ben reversed the 2026-08-26 draw: Begin is to open a corporation
selection canvas. Noted as BL-880 (corporation selection canvas), priority B, not sprint 39's — it
needs the rounds to hand a world forward (NR-811), which is the larger half of it.

### The numbers, and a correction

Pass 1 is **not** low seconds any more. `history_span_cost` seed 0 at `/O2`: 400 years 0.9 s at
1,742 regions; 2,000 years 34 s; 4,000 years **66 s** at 3,734 regions, reach 34–55%. So the
3,600-year pass sits near a minute and NR-809 (region count vs adjacency) is the live cause. Pass 2
is 400 years at the end-of-span count — expected in the low seconds, **measured on landing** because
sea legs widen the neighbour graph. The wait the player feels is the unwatched bar: `world_determinism`
puts post-era at 10–25 s, the warm start is 72 s and is retired by the search, and Begin re-pays
round 4's build.

### Housekeeping

`archive_landed.js` has no `--help`; it ran and evicted BL-871 and BL-873 (both complete) to the
cold file — their correct home, kept. BL-833 (tariff posture from history) cancelled as a duplicate
of the delivered BL-750. Two calls taken on Ben's behalf: NR-819 (a purchase keeps culture shares),
NR-820 (the canvas offers the shortlist).

---

## 2026-09-09 (sprint 37) — The world stops opening already full

**Mode:** Batch delivery in three waves, on a design pass that had closed the same morning.
**Runtime:** one long session; 3 sub-agents in worktrees; all lanes merged, built and verified in the main session.

### What started it

`docs/generation/COLONISATION.md` had landed that morning and the build objective was Ben's: a
wizard page carrying the 2D grid-map, evidence it produces interesting cultures, and regions
becoming provinces without aiming for a small set.

The sprint's real subject turned out to be narrower and better: **the world used to open already
full.** `run_settlement` placed and dated every region before the sim's first tick, so a time-lapse
of the ancient era could only ever show borders moving. There was no origin to watch. Everything
below follows from fixing that and then looking at what the fix revealed.

### What landed

**The migration happens on camera.** Settlement now hands the sim a *founding schedule* and the
year loop founds each region as its year arrives. Measured at the 4,000-year span: 10 ownership
changes at the opening frame, 523 during, across 319 distinct years. The world opens as ten cradles
and fills.

**Culture is earned rather than assigned.** `run_settlement` takes each founding's date *and*
culture from a single multi-source flood over the tile raster. C11 is the acceptance test on real
worlds: 23–33% of regions carry a culture a Voronoi over the map's own observed origins would not
have given them. The RNG draw in the founding date is gone — an arrival year is a consequence of
the ground, not a roll on top of one.

**The coast became the road**, rivers cheaper still, and streams cross straits by a *crude* bounded
hop of at most three water tiles. The bound is the specification: it crosses a strait and never an
ocean, which is what keeps "people got everywhere" from becoming "people sailed".

**Migration coins its own peoples**, derived from their parents — the tongue drifts rather than
re-rolling, so daughters read as kin. They divide on **country** (settling a farm class they were
not coined on) and on **size**. Distinct peoples holding ground: **12 → 38 / 56 / 33**.

**The wizard walks five named rounds** — System, Life, Culture, Empires, Industrialisation — each
pass round starting its own pass on arrival, with no Run button: arriving *is* the instruction.

### The corrections that mattered more than the features

**A ruling was overturned the same day it was made.** COLONISATION.md settled the span's end as a
*stated* year and explicitly recorded the derived alternative as considered-and-rejected. A fixed
span then left seed 0's last 1,300 years measurably static, and the terminating condition became
derived. The replacement turned out to need no safety stop at all: "wait until nothing more is
going to happen" is answered by a flood that finishes, so it is one field read off a walk that
terminates, not the mechanism the item budgeted for.

**Round 4 was split in two after watching it.** Fused, it showed conquest with the migration
already finished off-screen; once migration moved inside it, it showed migration with no conquest
at all. Two subjects, two rounds.

**Half a continent of "unsettled" land was settled.** Ben's screenshot showed grey masses that
never took a colour. The cause was arithmetic: 604 foundings against 523 ownership changes — 81
regions founded and owned by nobody, because polities are seeded once from the cradle cultures and
a region carrying a coined culture matched none. Seeding a polity on demand fixed it, and brought
conquest with it (~472 ownership transfers where the span had zero).

### The failure mode of the sprint, in six costumes

Every one of these looked like coverage and was not:

1. A synthetic route case whose two cradles were **equidistant**, so its proximity assertion was a tie.
2. A cradle-outcome classifier that asked *how much ground* and so measured **crowding**, reporting 317 encircled / 0 sterile — a number a sweep would have tuned the barrier costs against.
3. `stop_after_ancient_era` shipping an **empty map for 4,000 years** while its header reported 611 foundings, because `--verify` adopts the harness's own world and never takes that branch.
4. A harness **segfault reported as exit 0**, because it was piped through `grep` and a pipeline returns the *filter's* status.
5. A `sizeof(app)` `static_assert` whose bar was **~6× too loose** while the process died at startup with `0xC00000FD` and no output.
6. An out-of-bounds write that **both** harness builds passed — `/O2` because it is silent there, `--debug` because that flag drops optimisation but not MSVC's `_ITERATOR_DEBUG_LEVEL`. Only the app's real Debug build caught it.

The standing lesson, adopted mid-session: **run verification to a file and read the exit code**, and
never let a pipeline's status stand in for a program's.

### What actually found the defects

Driving the built app. The empty map, the fused round and the grey continents were all found by
opening the game and looking, and every one of them sat behind green harnesses. The live-click rule
earned its place three times in one day.

### Two diagnoses I got wrong

**BL-862** said coined cultures never win a region anchor. They win 81 of them — the measurement
behind the claim was taken at the 400-year span, where the founding schedule is *empty*, so it never
tested the case that mattered.

**The Begin crash** I reasoned was pre-existing (three worlds in Debug, likely `bad_alloc`) and gave
better-than-even odds. One bisect refuted it flatly. The cause was my own change breaking an
invariant a *comment* was holding — *"sized to the polity table, which the sim never grows"* — true
when written, false the moment a polity could be born mid-run. The comment is rewritten to what is
now true rather than deleted.

### Left open

- **The roads pass crashes the full generation pipeline** (Ben, known) — the autostart never reaches in-game.
- The scoreboard normalises **Land%** over *claimed* land, understating how empty the world is.
- The **drawdown lean** is editable nowhere until the Industrialisation round is built.
- **~95% of coined cultures never hold ground** — bounded and harmless, but most are ephemeral.
- Regions are **~3× where the day started** (609 → 1,906), which lands on the watched wait and on Begin; NR-809 measured reach as roughly quadratic in region count, and BL-844 is already spent, so the cap is the **adjacency model** (BL-855).

---

---

## 2026-09-08/09 (sprint 35) — Generation gets two more rounds, and the measurements refute nearly everything

**Mode:** Design → Batch delivery in waves → paused mid-sprint on Ben's call → review barrier and close.
**Runtime:** one long session; 5 sub-agents in worktrees; 3 lanes merged, built and verified in the main session.

### What started it

Ben: *"everything up to the planetology looks great"* — and nothing after it is visible at all.
The wizard is the one generation surface that works, and it stops at phase 1. The two passes a
player would most want to have watched — the ancient history and the industrial economy — happen
behind a loading bar. Sprint 35, opened that morning on the startup budget, was repurposed.

### What was settled

The wizard grows to **five rounds**. Round 4 is the **history**: a 2D map replaces the globe and
runs a time-lapse of 4000 years to 1200 CE, with an ordered **top-sixteen scoreboard** on the left.
Round 5 is the **economy pass, 1560 → 1960**, its own page with its own run and reroll. `Begin`
moves to the last round; the 4000 years can be rerolled, which is what forces the wait to be
genuinely affordable rather than merely tolerable. Ben: *a watched wait needs no budget* — which
dropped the startup-budget chain the sprint had opened on.

That fixed the calendar: 4000 years to 1200 CE, a deliberate **coast** to 1560, then 1560 → 1960,
epoch 1960. An industrial-band campaign, where the 0 CE default gave an ancient one.

### Three measurements, and every one of them said no

This is the session's actual content. Wave 0 existed to stop the sprint being planned on a guess,
and it did exactly that — three times over.

**4000 years is not free.** I had read the sim and concluded the span was probably already
affordable: it works on the region graph, the O(N²) neighbour build sits outside the year loop, and
the stepped decision clock already amortises decisions. Wrong. Per-year cost at 4000 years is
**6–9× its cost at 400**. Reach is **66–86%** of the run, and `rebuild_reach` caches into a *single
shared slot* — so twelve polities evict each other every round, 12,000 rebuilds over 1,000 rounds.
Its own comment claims the cache survives until a capital moves. With more than one polity it does
not survive one iteration (BL-834).

**Most of the war is not war.** An R5 regression after the culture-shares merge was first diagnosed
as a fragile fixture, and then by me as a scoring reorder. Both wrong. All 258 battles of the
seed-0 fixture are **the same region** — zero population, zero defenders — taken and retaken for
four thousand years. `battles == conquests == 258`, exactly 1:1, which the shipped harness had been
printing unread all along. Culture shares were exonerated by direct experiment: the pre-change
equality test gives identical counts (BL-835).

**And the batch was about to close green on five real defects.** The step 4a barrier — one cold
review across the whole integrated set — returned five confirmed findings, including an
out-of-bounds write in the `--autostart-windowed` driver, which indexed a `uint32_t[3]` with a
round counter the wizard had just widened to 0..4. The Reroll button was gated when the wizard
grew; the driver was missed, and it is the path nobody eyeballs. Fixed here; BL-840–843 filed for
the rest, two of which mean headline behaviours of the grudge and share systems do not actually
occur in the sim.

### What landed

Wizard rounds 4 and 5 with labelled placeholders, verified by a live click-through on the release
build (BL-816, BL-824). The span-cost harness and its profiling counters (BL-825). Culture shares,
the grudge ledger and the `pass_one_output` handoff (BL-826, BL-827, BL-828 — the last half done by
the author's own admission; its consumers are not rewired).

### What Ben settled when development paused

**Population is civilian, armies are distinct, and stage 4 does not simulate total warfare.** That
is the root fix for the dead region: war stops *producing* empty ground, so the pathology has no
cause rather than a block. It also gives culture shares their subject back, since conquest now
transfers people. The scorer gains two questions — *can I keep it* (logistics and ancient roads) and
*will others attack me for fear of being next*, which is what finally makes the balancing-coalition
lever admissible: fear reads a **behaviour**, not a rank. And **turbulence is round 4's lean** —
roll for a world with fewer or more countries, tuning forces and never clamping a count.

### Left open

Six review-queue entries, the sharpest being NR-807: `CONCEPT.md` still names the ancient arc as
the live product. I fixed the generation-side citation and deliberately did not touch that one —
it names who the player is, which is a product call rather than a reconciliation.

Sprint 35 is **paused, not closed**, with `NEXT_SESSION.md` as the handoff.

---

---

## 2026-09-07 (sprint 33 opens) — The corpus stops charging every session, and two tools are found lying

**Mode:** Design (one question, two calls) → Corpus/Batch delivery in one wave → close.
**Runtime:** one session; 7 sub-agents in worktrees, no compile — not one line of `src/` changed.

### What started it

Ben asked whether the query tools reading *both* the hot backlog and its archive defeats the
archive's purpose, and whether reading the relevant docs was creating context creep.

Half of that answer was easy and stayed easy. The archive exists to keep `backlog.json` meaning
exactly one thing — open work — so the hot file never needs disambiguating. The union exists
because `--touches` has to see closed items or it cannot answer "is this built?". Those two do not
fight: the archive keeps the *file's meaning* clean, the union keeps the *question* answerable.

The other half was the real cost, and it was somewhere else entirely: ~650K tokens of authority
docs with no summary layer, so a nearly-right traversal opens a 40K doc to discover it wanted the
sibling; `--full` prose resolving out of 1.9MB of archived designs; and `CLAUDE.md` plus the
standing rules loading whole for a one-line doc tweak. Five items, filed as **sprint 33**; the
market-viability sprint that held that number moved to **34** unchanged.

### What was built

**BL-787 (doc summary headers)** — every authority doc named in `CLAUDE.md` § 3 now opens with a
header block. Ben chose the **index of questions** form over a précis and over questions-plus-stance:
it lists the questions a doc settles and never the answers, so it survives a design change that
alters one. 71 docs, five parallel slices. `ACTIONS.md` is regenerated whole, so its header lives in
`render_actions.js`'s preamble instead.

**BL-789 (standing rules split)** — `io-standing-rules.md` went **230 lines to 133**. The
AI-behaviour grant register — BL-079 through the rival-network grant, 110 lines of dated precedent —
moved verbatim into `AI_OPPONENT.md` § 11, diffed line-for-line with all 20 ids accounted for. The
*gate* stayed, and was sharpened rather than summarised: a new widening is raised, never assumed.
That was the item's whole risk — moving the history must not make the next widening cheaper to take.

**BL-788** gave `--grep`/`--touches` a one-line-per-item default (33,072 bytes → 2,294 on one sweep,
`--full` byte-identical). **BL-790** added `doc_owner.js`, which answers "which doc owns this file"
from what work actually cited. **BL-791** put the fan-out-as-compression paragraph in `DELIVERY.md`.

### What it found — the part worth keeping

**The union does not union.** `archive_store.js` globs `backlog-design-*.json` only. It sees **138
items; the archive directory holds 762.** The 624 it misses — 420 of them `complete`, 590 carrying
the `files[]` array `--touches` reads — sit in `backlog-complete-*`, `backlog-cancelled-*` and
`backlog-purged-*`, which predate `archive_landed.js` and use an `items` array instead of a `records`
object. On `MARKETS.md` alone, 35 invisible items. **BL-792**, priority A.

**And `--grep` throws away what it does find.** It matches across the union, then drops terminal
items in the default view: `--grep market` prints "nothing matched" while `--grep market --all`
returns 11 landed items. `DELIVERY.md` makes `--grep` the first step before authoring an item
precisely to catch duplicate work, and it is blind to exactly that case. **BL-793**, priority A.

Both fail the same way — a confident, silent NO — and both were surfaced by delivering something
else. The session's opening answer needed its correction: the split is sound, the union is the right
design, and the union as implemented has been answering wrong.

**Six doc-truth defects, from writing one-line boundaries.** `PRODUCTION` and `POPULATION` each name
the *other* as the workforce authority and both restate the derivation. `MARKETS` holds 70 lines of
procurement; `CONTRACTS`, the doc named for it, is 113 lines total. `TILE_GENERATION` holds province
rules the new headers now point away from — orphaned authority, which is worse than duplicated.
`HISTORY` holds an ancient-naval rule `MILITARY_HISTORY` owns. Four UI subjects have no single owner,
including `LAYOUT`'s 170-line drill-through system. **BL-794, BL-795, BL-796.** Fixed in place:
`NATION_GENERATION` § Pass 7 carried "(RULED, not yet built)" in a heading — a state claim in an
authority doc, forbidden outright.

None of these are visible while each doc is read alone. Stating a boundary in one line is what
exposes that it was never stated.

### Method note

BL-791 demonstrated by the sprint that filed it: five slices read the entire ~650K corpus and the
main session paid for none of it. Every report came back as boundaries and findings, not excerpts.

### Verification

No compile — no `src/` change in the sprint. `backlog_lint`: **0 fails** throughout (warnings
pre-existing). `next_id.js`: BL-797. All five agent branches merged in the main session, one conflict
in `DELIVERY.md` resolved by keeping both bullets. `render_sprints`, `render_actions` and
`devlog_index` re-run. `archive_landed` evicted the five closed items plus four already-terminal rows
that had been sitting in the hot file, all verified to rebuild byte-exact; the hot file holds 31 open
items.

### Block 2 — the batch, and what four failed reviews bought

**Mode:** two Workflow runs — 20 agents, then 10. No compile: the only `src/` edits in the whole
batch are comment pointers repointed at moved sections.

Five lanes, each built from a settled instruction. **Four failed cold review**, and none of the four
on style:

- **tools** traded the false negative for a false positive. Widening the union admitted 204 cold
  rows whose `status` field *lies* — the 2026-08 sweeps froze each row at its pre-purge status, so a
  culled item still reads `designed`. `--grep market --open` returned 18 rows with **zero** on the
  hot worklist. It also shipped a DELIVERY.md bullet and a `--help` line promising a behaviour its
  own code did not have. The rule that fixed it: **a cold row's state comes from the file it is
  archived in**, normalised once at the union so no caller can be fooled.
- **generation** deleted the *accurate* half of a doubled claim. PROVINCES was left asserting the
  one-domain invariant is "structural rather than checked" while
  `province_partition_harness.cpp:203` checks it as P2b. One false statement where there had been
  two, one of them true — the specific failure mode of consolidating a boundary.
- **ui** claimed to have grepped for dangling citations and had not (TOOLTIP still cited a moved
  CANVASES section), and asserted an answer in SELECTION.md on a question the same agent had told
  the judge was Ben's.
- **proposes** switched PEOPLE.md and EVENTS.md to `Proposes:` on a half-read sentence. Both carry
  `## Settled — Ben's rulings, 2026-08-22`, and the disclaimer they were switched on is qualified:
  *"except where § Settled records one."* **That half-read was mine** — it is the premise I gave Ben
  when I asked the question. The variant itself stands, on nine genuine research and exploration
  docs.

The **second** review earned its keep too: it caught a fabricated `(Ben, 2026-09-07)` attribution on
the cold-row rule. That rule was mine, off the review's own measurement. In this repo a dated Ben tag
is load-bearing provenance, and it would have hardened an agent's design call into a settled human
ruling.

**The design panel changed an answer.** Two independent proposals per boundary — one arguing from
CLAUDE.md's router, one licensed to say the router is wrong — split workforce differently from the
way the backlog item suggested. A builder reading the item alone would have built the wrong split
confidently.

### What the completeness critic proved about scope

The three boundary lanes were scoped off defects a header sweep *happened* to notice. The critic
built the `Confused with:` graph properly — **72 docs, 59 mutual pairs, 89 one-way edges** — and
found three overlaps no lane would have reached: LOGISTICS and SUPPLY both holding the travel-time
model under the same ruling date (**BL-797**); the navigation model and per-rung lens table each
asserted **three** times (**BL-798**); and CREEDS/NATION_GENERATION, which survived every sweep
because it is a *one-way* edge (**BL-799**).

BL-798 is the lesson worth keeping. BL-796 deleted copy two of the lens table on entirely correct
grounds and left copies three and four, which were outside its write set. **A correct rule applied
to a partial scope leaves the corpus more inconsistent than it found it.** That is what **BL-801**
(the header graph checker) exists to prevent, and it is why the tool comes before the next boundary
sweep rather than after it.

### Verification, block 2

Union **138 → 757** items. `--grep market` **0 → 101** matches; `--touches MARKETS.md` **1 → 29**.
Non-terminal over the union is 36, and that set *is* the hot set. `next_id` BL-806, never lower.
`backlog_lint` 0 fails throughout. `doc_owner`, `backlog_view` and `status.ps1` all unchanged or
better. 72 docs carry a header; `docs/ui/DRILL_THROUGH.md` exists with its CLAUDE.md § 3 row. All
five fix branches merged with no conflicts.

### Block 3 — the checker, and the sweep it scoped

**Mode:** three Workflow runs — 3 agents, then 3, then 15. Still no application compile: the only
`src/` edits are comment pointers and one harness correction.

**BL-801 (header graph checker) came first, deliberately.** Three boundary lanes in block 2 had been
scoped off defects a header sweep *happened* to notice, and sweeping again by hand would repeat that.
`tools/session/header_graph.js` now checks four things: dangling citations (fails the run), the
header graph (prints), router coverage both directions, and state-independence (fails).

**It failed both its cold reviews on the first cut, and the graph half was badly wrong.**
`demarkup()` stripped underscores along with markdown emphasis, so `NATION_GENERATION.md` resolved
to nothing and **no doc with an underscore in its filename could ever be an edge target** — about a
third of the corpus. The consequence is the one that matters: **the defect it was built to catch
(BL-799, a one-way edge) was absent from its output entirely.** It measured 195 edges against a true
264. The resolver half carried a *false pass* — a one-token anchor certifying a citation to a
heading that does not exist — plus two citation shapes it never swept at all.

After repair, an independent parser agrees **edge for edge**: 264 edges, 83 mutual pairs, 98 one-way,
72 headers, set difference zero both ways. The dangling class went 2.7% false-positive → **0%**,
audited at 80 of 80 rows. It ships with 27 self-test assertions, each pinned by a mutation test.

First measurement of the corpus: **1119 citations — 729 OK, 310 prefix, 83 dangling.** The dangling
cluster by *rename*, not by file, and one cluster was ours: five references named
`io-standing-rules § the player-corp exception`, which BL-789 had moved that morning without
sweeping. Filed as **BL-807**.

### The sweep — and the item that repeated itself

Five lanes; three passed, two failed.

**BL-798 failed in exactly the way it was filed to fix.** It consolidated copies two and three of
the pan/zoom claim and left a verbatim **fourth** at `MINIMAP.md:301`, outside its write set — and
its completeness evidence was *false* when the reviewer re-ran it. Widening the write set and
requiring the search be pasted in full, empty results included, is what fixed it. `"primary slot"`
now appears once in the corpus.

**BL-799 failed twice, identically**, and was finished in the main session by reading the code site
by site. Both agents wrote the tidy universal rule; the truth has an exception the corpus had
already recorded elsewhere — `make_corp_name` pairs a tongue-inheriting identifier with one of
twelve **English structural type words**. When two independent attempts fail the same way, the
brief is wrong, not the agent.

**BL-804 corrected the harness’s MEASUREMENT, and left it red.** P9c asks about seed strength, and
`build_province_partition` skips `province_anchor` centres when gathering seeds — those are founded
*after* the partition ships, so their size owes nothing to their scale, and the row was pooling 1,011
of them. On the corrected measurement **P9c still fails** (s1: 458 @ 7.51 · s4: 1 @ 3.00), and so does
A1. Rebuilt and rerun in the main session to confirm: **42 PASS, 2 FAIL, exit 1.** Neither was
weakened to pass, and the surviving question is sharper than the one BL-804 answered — a monotone
claim over four buckets decided by a single scale-4 sample may not be answerable as written. **BL-809**.

### The defect the docs were hiding

**BL-808**, priority A. `history_sim.cpp:1564` names a sim-founded region `src.name + " Reach"` — an
English literal in a name that ships, against the standing rule that every generated name is
sci-fi/fantasy.

**The project fixed this exact defect once already.** `settlement.cpp:272` carries the post-mortem in
its own words — *"'MelethWorirUlael Reach' put two naming systems side by side in one string, which
reads as a bug rather than a style"* — and BL-348 coined the quarter word from the tongue. The second
pass was never swept. It surfaced because a doc claim was too broad: **the tidy rule was wrong
because the code was wrong.**

### The pre-push audit, and what it caught

Three cold auditors over the finished state — corpus coherence, doc-versus-code truth, and whether
the RECORD is honest — then an adjudicator that verified each blocker itself before accepting it.
Two blockers were raised; one survived.

**NR-794 was minted twice, and the second mint was mine.** A prior session used it for a
naval-composition ruling and cited it at three source sites (`unit_roster.hpp:283`,
`unit_roster.cpp:211`, `history_sim.cpp:201`) — and **never filed it**. So nothing in the review
store could see the id was taken. This session minted NR-794 for the border-band question, and for
a few hours three code comments resolved to a ruling about lens chrome: a visible gap converted
into a confident wrong answer, which is the exact failure class these 61 commits spent the day
removing. Renumbered to **NR-797**, with the collision recorded on that entry. **BL-811** widens
`next_id.js`, which guards BL ids and nothing else — and the guard must scan the *tree*, not the
store, because this id was cited in code and never filed.

**The second blocker did not survive, and the adjudication is worth keeping.** Six docs derive
km-per-tile from a 312-column grid the code retired at BL-424 (`home_grid_width = 261`), so the
constant is ~20% wrong. Real — but the session did not cause it: BL-797 deleted the duplicate in
SUPPLY, taking the corpus from seven false copies to six, and four of the five survivors were never
touched. Filed as **BL-810**, with the aggravating detail that the re-authored line now cites
`body_km_per_tile` beside the wrong number, so a false constant reads as code-verified.

The auditors also confirmed the load-bearing claim by the right method: today's checker run against
an extracted base tree, diffing the *misses by citing site* rather than by count. **4 added, 4
removed — zero citations broken by this session**, and three of the four additions are BL-807's own
prose quoting the broken forms it exists to fix.

### Verification, block 3

Dangling **83 → 81** across the whole sweep: five lanes moved prose between docs and created no net
dangling citation. `header_graph --self-test` 27/27. `backlog_lint` 0 fails throughout. `next_id`
monotonic. Nine items closed and evicted, all verified to rebuild byte-exact; the hot file holds 28.
`docs/development/design/` deleted after three independent checks, `--doc GLOBAL_STYLE_SHEET` now
resolving where two files had shared the basename.

### Open for Ben

- **BL-792 and BL-793 landed in block 2**, so a bare `--touches` or `--grep` negative is evidence
  again. It was not, for the whole life of this session before that point — worth knowing when
  reading anything filed earlier today.
- `--sprint` does not exist as a flag; unknown flags are ignored silently and the tool returns
  everything. Minor next to the two above, and not chased.
- `PEOPLE.md` and `EVENTS.md` are proposal-stage but their headers say **Settles:** like every other
  doc. A `Proposes:` variant would be more honest; I did not invent one without your say.
- `research/ERA1_TECH_LANDSCAPE.md` and `TECH_EFFECTS.md` route readers to `economy/RESEARCH.md`,
  which is a stub. Correct routing, empty destination.

---

---

## 2026-09-06 (sprint 32b closes) — The world changes, and the instruments learn to see it

**Mode:** Design (one elicitation form, eight calls) → Full batch delivery in three waves → two cold
reviews → live check → close.
**Runtime:** one long session; ~40 harness builds, three play builds, 10 sub-agents in worktrees,
two cold adversarial reviews.

Sprint 32b closed with **eleven items delivered** across three waves; **32c opened** for the
remaining 28. The distinction from 32a is the whole point: 32a delivered instruments and moved no
world on purpose; 32b moved every world four separate times and kept the causes attributable.

### The design pass that set the scope

Ben answered a market-work elicitation form settling **eight open calls**. Five became authority-doc
text rather than commit messages: *no price, no draw* in PRODUCTION.md; phase 6's three-term
objective in GENERATION_STRATEGY.md; pass 3 rewritten to SELECT a landscape rather than settle one;
protection DERIVED at handoff in NATIONS.md; `trade_goods_misc` joining the endemic basket with its
asymmetry cost stated in MARKETS.md. BL-751 was cancelled superseded with its parts named on BL-770
and BL-772, so nothing went with it.

Later, ruling on a paleo measurement, Ben added the steer that outlives the item: **richness is
absorbed by the per-province infrastructure score, never clamped at generation** (PROVINCES.md
§ Richness is absorbed here). Ground is a fact about the world; what a corporation can *do* with it
is gated behind roads it has to build.

### The result that decides the most

**BL-770 slice 1 returned a negative result, and it was the right question to ask.** Five candidates
whose fixtures differed by 20 corporations and 41 buildings scored *identically to the last digit* —
every term read tiles, markets and population, and `market_saturation.cpp` contains neither
"corporation" nor "building". Slice 2 added the roster-aware term (actual against potential
completeness) and the same five candidates now spread **3.2e-01**. The search itself is still
unbuilt, which is why BL-772 stayed blocked rather than shipping an unsettled opening position.

### Four things that went wrong, kept because they will recur

1. **Two agents bumped `save_game_version` 4 → 5 independently**, each correct alone. Merged, the
   stream carried both and was neither one's v5 — two layouts under one version number, each
   readable only by the build that wrote it. Resolved to 6; the wave ended at 8. Thereafter: exactly
   one item per wave may bump it.
2. **Three save-format changes landed unasserted**, because no worktree agent can build
   `save_envelope_roundtrip` (it links imgui). Each time the integrating session wrote the assertion
   and ran the differential. That is the builder gap's standing cost, now recorded in the harness.
3. **The cold review caught a regression shipped as an improvement.** BL-767's Invest ground-pull
   made industrialisation *worse* — 8/16 against 14/16 at the value that measures best — and the
   item's own diagnosis ("the ceiling is in the selection rule, not the price") was falsified by
   measurement. Underneath it was a real bug: a capped domain kept its pull, but the investment it
   won did nothing.
4. **Three builder fixes, and my first two verifications were run where the bug could not appear.**
   Deps-cache resolution, `JSON.stringify` quoting, MSYS export ordering — each "fixed" and each
   still broken for worktree agents until tested *from* a worktree.

### Measurement over argument

The pattern that worked, repeatedly. BL-783 tested its causal claim as a controlled **experiment** —
sack 59 regions in a copy and re-materialise: sacked lose 73.3% of their cities, controls 2.4% —
rather than resting on the one razed region the seed happened to produce. BL-765 asserted "the
fossils read the past" by generating the same body twice with the drift record withheld. BL-768
measured both its constants off a bimodal distribution instead of choosing round numbers. BL-750
traced its flat tariff table to an upstream furnace count rather than tuning the banding.

And the peat re-examination **overturned** a change: RESOURCES.md authors peat as a pair on scrub,
BL-765 had narrowed it to marsh on a reading of prose, and 58% of the world's peat went with it.
Restored, and measured back to 7930 against a 7792 baseline.

### The live check, run at the close rather than owed forward

BL-754's budget line **verified on screen**. BL-768's roads render, but an ancient road is
indistinguishable by eye from a national one, so the headless differential is the stronger evidence.
**Coastal ownership could not be checked at all** (NR-791): the hover card reports terrain, clicking
water selects nothing, no lens colours by owner. That is a hole in BL-780's own done-when.

Two measurements fell out of it: `build/` is Debug, so its 84.75 s generation must not be quoted
against BL-761's Release figure; and the Debug warm start is **667,428 ms**, of which
`run_economy_step` is 534,719 — BL-761's finding quantified, naming the same culprits.

### And a bug the harnesses could not see

Ben, looking at the live world: **one nation carries a complete road lattice** while its neighbours
are sparse. The aggregate (+320 roaded tiles) was right and the per-nation distribution was wrong,
and nothing reports per-nation road density. I had seen the same density, doubted it, checked a
different region and moved on — an aggregate cannot see a distribution. **BL-784.**

---

---

## 2026-09-06 (sprint 32a closes) — The arc runs, and four instruments could not see

**Mode:** Design → Full (batch, then hand-built slices) → three design rulings → close.
**Runtime:** one long session across several days; ~25 harness builds, one play build, 12 sub-agent
launches of which **zero** succeeded.

### What closed, and why here

Sprint 32 opened with 8 items on "three passes of simulated history". Ben's eight-phase reorder
added 12, the water-domain ruling added 5, and findings added the rest — **34 items, 5 delivered**.
Closed as **32a** at a natural boundary rather than pushed on: everything delivered is one coherent
thing (*the arc runs, and the instruments that measure it are honest*), and everything remaining
moves the generated world, which wants its own before/after. 32b carries the other 29.

### Delivered

**BL-747 (two-span prehistory).** The Era −1 sim now runs at any epoch — the gate had refused
anything above 1700, so the arc we build the product on had ancient borders and no simulated history
behind them. Two spans on **one** invocation, because `era_minus_one.hpp` exists precisely to stop
that call drifting across callers. `boundary_year` (INT64_MIN) and `span1_band_ceiling`
(`industrial`) are inert at their defaults, which is what makes the 0 CE world byte-identical rather
than hoped-identical. A 1960 world runs 1160 → 1560 → 1960.

**BL-757 (the sweep measures generation's own era).** `history_sweep` printed "−4000 → 0" because it
built default params; generation runs −400 → 0. So the harness that `HISTORY.md` and `COLLAPSE.md`
both name as the place every Era −1 magnitude is argued was describing a run no world is built from.
It now runs from `era_minus_one_fixture`, closing all six divergence axes, asserted per seed by a
**self-checking** row: the report already carries generation's own counts, so a pinned 270/207/833
would have rotted the first time the world legitimately changed.

**BL-763 (continent time axis).** `drift_col`/`drift_row` were documented "per-epoch" with no epoch
defined anywhere — the vector existed and nothing integrated it. Now 5 My over 20 epochs, with any
past configuration derived rather than stored (`continent_state` is on the save envelope seam;
twenty rasters per body would be ~2.5 MB for data that is a pure function of five floats per plate).

**BL-771 (tick length).** Closed on its **audit**, not its code. The audit found the tick length was
the wrong dial, Ben ruled the quarterly tick, and the parameter became unnecessary — so R1, R2 and
R4 were **cancelled rather than deferred**, because a deferred row invites someone to build it later
for a reason that has gone away.

**BL-775 (saturation measure promoted).** Ben's phase 6 question turned out to be already answered
by two computations the project verifies against daily — they were inside harness anonymous
namespaces where `src/` could not link them. Now in `src/world/market_saturation.{hpp,cpp}`.

### Three rulings, each of which changed the work rather than confirming it

**The quarterly tick** (NR-786). It moved phase 6's *span*, not just BL-771: at 90-day ticks, 100
ticks is 25 years rather than the 400 Ben's point 6 named.

**The static saturation search.** Ben restated phase 6 narrowly — *"not a 100% accurate series of
trades… nor what makes the most profit per tile"* — and that is not a simulation question at all.
Saturation is a **static property** of a candidate roster against a fixed world. It dissolved the
span question entirely: there is no span, because there is no clock.

**The water domains.** Ben: *"Give coastal to owners, and sea provinces are unowned."* Better than
what was filed, and the codebase was most of the way there — `province_kind` already partitions the
three domains exclusively, ownership already derives from tiles, and the naval class already exists
as three authored rows worth zero. It halved BL-756's destructiveness and dissolved BL-749.

### The finding that matters most: four instruments that could not see their subject

1. **`history_sweep` swept the struct default** rather than generation's run (BL-757).
2. **`era_world_harness` was never run** while `world_determinism` was green — so a real regression
   shipped inside a wave I had called verified. Found by the cold review, confirmed by hand: 12 pass,
   1 fail, and only the 1960 clause moved (BL-758, left **red** for Ben).
3. **The app never prints the generation budget** the item added — the line is inside an
   `if (fixture)` branch and the app passes none, so every figure quoted is a harness figure.
4. **`history_sweep.json` diffs on a wall-clock field**, so genuine drift hides in timing churn —
   and the "it is stale" claim I had repeated turned out to be wrong on inspection.

This is the BL-714 pattern recurring, and it recurs because **a green check is not evidence that it
looked**.

### Measure-first paid for itself twice, and both times the number inverted the item

`sim_water_census`, three seeds of generation's own era:

- **1105 of 3819 regions sit on water** — 29% — with 613 on open ocean. BL-756 had assumed the count
  might be zero and its guard free; it would have deleted a third of every world.
- **43% of adjacency edges cross sea.** BL-755 had assumed sea reach was absent. It is unpriced, not
  absent, and every tuning constant in `history_sim_params` was fitted with it happening.

Both items would have been built wrong from their own filed premise. Ben's water ruling then made
the ~492 coastal regions *legitimate* and left only the 613 on open ocean to fix.

### Two process lessons

**A requirement written after the code describes the code, not the intent.** BL-775 said "promote
both"; one half was promoted, the group was written describing that half, and lint, requirements and
harnesses all went green on a half-delivered item. Caught only by re-reading the item.

**Sub-agent capacity was unavailable for the entire session** — 12 launches, every one a 529 with
zero tool calls. Every slice was hand-built, and the batch's **cross-slice review barrier never
ran**. Recorded as a debt, not dropped. BL-774 compounds it: worktree agents cannot build the
harness class that checks the byte-identity invariant.

### Owed

The review barrier; `continent_drift`, `sim_water_census` and the promoted measure are **ad hoc**
until Ben names them in the verifier skill; BL-762's deposit split stays blocked on BL-765 because
removing biological deposits before the Life phase exists would delete `agricultural_produce` and
take the food chain with it.


---

---

## 2026-09-03 (sprint 32 opens, wave 1) — The second span is free, and four instruments were pointing the wrong way

**Mode:** Design → Full (merge repair, then one delivery wave). **Runtime:** one session; one
7-agent subsystem map, one implementer in a worktree, one cold review; ~6 harness builds.

### The design

Ben's brief: gamify generation — duplicate the 400-year timelapse so a second pass produces
post-Enlightenment industry, colonisation by major powers, and market conditions at game start.
Settled as **three passes on two engines** and written into `GENERATION_STRATEGY.md` § Three
passes of simulated history, `HISTORY.md` § The epoch and the run, `ERAS.md`, and
`CORPORATION_GENERATION.md` § Pass 6. Two polity spans on the existing sim, then the warm start
promoted to an economic settle. Market differentiation comes from three in-world forces with
visible causes — tariffs from industrialisation timing, distance on the real network, colonial
ties as preferred sellers — and a seed-sweep scoreboard reads the SPREAD, never a per-world value.

### The merge, which needed three repairs

The branch was cut before 2026-09-02 and collided on all three shared numbering spaces. Main had
minted **BL-746** (upkeep starvation cliff) while this branch filed BL-746 (two-span prehistory);
**sprint 31 closed** mid-session and sprint 33 opened; and the sprint number itself moved twice —
renumbered to 34 by reading `next_up`, then corrected back to **32** by Ben, who reads 32 as a gap
to fill rather than a number to skip. `next_up` is corrected so the next session does not re-derive
34. Resolution took main's stores whole and re-applied this branch's additions on top, so nothing
of the other session's was displaced. Memory `io-backlog-id-collision-on-stale-branch` records the
tell: `next_id.js` reported its own scan INCOMPLETE and was believed anyway.

### Built — BL-747 (two-span prehistory) and BL-754 (generation budget)

**One invocation, not two.** `era_minus_one.hpp` exists because this exact call has drifted across
callers on six axes with a seventh going uncounted, so a second `run_history_sim` call was ruled
out at design time. The spans live inside the single existing call: `history_sim_params` gains
`boundary_year` (default `INT64_MIN`) and `span1_band_ceiling` (default `industrial`), and
`sim_band_ceiling(params, y)` is one derivation read at BOTH roster sites — the works table off
materials capacity, the unit table through a new `build_stack` ceiling argument. The gate drops its
epoch clause and asks `prehistory_years > 0` alone; the epoch now decides only whether there is a
second span.

**Both new fields are inert at their defaults**, which is what makes the ancient arc byte-identical
rather than hoped-identical: no year is before `INT64_MIN`, and a clamp to `industrial` is the
identity. Adding a candidate to the scorer would have moved the argmax even where it never won; an
inert clamp cannot.

**Save format:** `world_params` is serialised, so `industrial_years` is a mid-record insertion and
`save_game_version` goes 3 → 4. **Existing `.iosave` files are rejected** — that file has no
upgrade path by design, refusal being its whole compatibility story.

### Verified — in the main session, not on the agent's report

`world_determinism` ALL PASS. The three 0 CE digests are unmoved — `039EE9880739CDF6`,
`B0EBBA249B3DDABB`, `DE55600457797638` — with the era report still years=400 battles=270
conquests=207 foundings=833 on seed A. The 1960 arc runs **1160 → 1560 → 1960**, two tick bands,
span-1 ceiling medieval, years=800 battles=1323 conquests=1128 foundings=506, digest
`DB86651B9A596F7B` identical across two builds. Three new reported rows (R4.1–R4.3) assert only
that both spans ran, that the pass did something, and that it is deterministic — no magnitude pinned.

**A defect found while reviewing the merge, fixed in the main session:**
`era_minus_one_has_industrial_span` tested only the epoch, so `industrial_years = 0` on a 1960 arc
would have put the boundary AT the epoch and capped the whole run at medieval — the exact opposite
of what that field's own doc-comment promises. It now tests `industrial_years > 0` too.

### The answer to Ben's question, which was the point of the wave

*"I am interested to see if this can be done cheaply."* **Yes, and the second span is free.**

| run | total | pre-settle | settlement | era-1 | post-era |
|---|---|---|---|---|---|
| 0 CE, 400 y | 8200 ms | 443 | 4 | **323** | 7428 |
| 0 CE, no era | 6661 ms | 455 | 3 | 0 | 6202 |
| 1960, two-span 800 y | 7996 ms | 465 | 2 | **197** | 7331 |

800 simulated years at 1960 cost **less** than 400 at 0 CE, and the whole 1960 world builds faster
than the ancient one. Cost tracks the region table the sim grows, not the years it walks — at 1960
the settlement pass has already founded most regions, so the sim founds 506 where the ancient arc
founds 833. `COLLAPSE.md` § The 4000-year problem already said cost tracks the province table; this
is that sentence measured.

**And the era pass costs four times its own wall clock.** Era on versus off at 0 CE is a ~1.5 s
difference of which only 323 ms is the sim; the rest is every pass after it working over the 833
regions the era founded. A budget taken off the era's own timer is wrong by 4×, and the
affordability rungs in `COLLAPSE.md` are aimed at the 323 ms rather than the 1.2 s. **No rung is
re-filed**, per BL-754's own rule: re-file the one a measurement points at, when it does.

### Four instruments that were pointing the wrong way

The wave's most valuable output was not the feature. Scoping the sea-leg item against the code
inverted its premise, and measuring the baseline caught two more:

- **BL-755** — region adjacency is Chebyshev radius 9 and **water-blind**, so short overseas
  campaigns are already legal and FREE, and every tuning constant in `history_sim_params` was
  measured with that happening. Sea reach is unpriced, not absent.
- **BL-756** — the Settle verb applies **no terrain test**, so a region can be founded on ocean,
  where `terrain_combat` returns 0 defence. Silently undefendable, and nothing reports it. Also:
  `region::port_q` counts lakes and is inherited-and-decayed rather than re-surveyed, so it is
  wetness, not sea access — any harbour gate keyed on it gates on the wrong thing.
- **BL-757** — `history_sweep` prints "16 seeds, -4000 -> 0" because it constructs bare default
  params instead of deriving through `era_minus_one_sim_params`. Generation runs -400 -> 0 on one
  band. So the harness both `HISTORY.md` and `COLLAPSE.md` name as the place every Era -1 magnitude
  is argued describes a run no world is built from — BL-462's defect, unclosed, in the one harness
  whose whole subject is the era. Its banner now says which params it runs, and `--epoch` derives
  through the real helper.
- **Zero works raised across all 16 seeds**, reproduced in all three modes. The cause is the
  **argmax, not the gates**: gates are met comfortably, but `work_score_q` lands in the low tens
  while `build_work` is scored LAST against a `best_score` already set by campaign and settle. So
  the roster is not inert, but **BL-748's band unlock has nothing to unlock** until `build_work` can
  win a round. Read from code, not yet measured.
- A fifth, same class: every harness calls generation with `works = nullptr` while the app passes
  the real registry, so **the shipped game scores five verbs and every harness scores four**.

`history_conquest_gap` R3 fails on all 8 seeds and **was already failing** — confirmed by building
the harness from unmodified base sources in a scratch tree, which emits the identical table. The pin
is stale, not broken by this change; `history_sweep.json` is stale the same way. Neither was
re-blessed, because re-blessing inside this commit would bury the signal.

### Left open, deliberately

BL-749 (sea legs) is held out of wave 1: its premise is inverted by BL-755/BL-756 and five design
calls on it are open (NR-785). BL-751 (economic settle) is gated on sprint 33's growth half — sprint
31 made the field solvent, but valued production still falls, and a settle over a shrinking field
culls toward a smaller economy rather than a steady one. **NR-783** asks whether the span boundary
should be derived from the first furnace rather than authored at epoch − 400; **NR-784** records the
call taken to keep the ancient arc's band ladder uncapped, and asks whether it should be capped at
all — a 400 BCE polity can currently reach the gunpowder band and nobody has measured whether it does.

---

---

## 2026-09-02 (sprint 31 closes, sprint 33 opens) — A field that can pay, and the one that must grow

**Mode:** the whole arc in one session — Design → Full → measure → rule → fix → measure — closed on
Ben's call. **Runtime:** one long session; ~10 full builds, ~40 harness builds, ~20 lapse runs, one
doc-sweep agent, no worktree agents.

### What the session did, in order

1. Merged sprint 29's ground bake to main and pushed.
2. Ben's brief for the market: *every recipe, at base price, makes a greater profit than marginal
   cost*. Built the instrument first (`recipe_margin`), and its first table was the finding: 41 of
   44 priced recipes failed.
3. Three rulings by form (reading A, processing rate 16, extraction fixed costs cut) → the retune,
   the consequences absorbed (unit wages ×2.776, three harnesses re-aimed, two CMake targets that
   never linked, an era-band header split), the docs re-priced.
4. Four more rulings (one table with the ancient chain at depth one; the constants and the anchor
   route rule ratified; two design-red harness rows re-expressed).
5. Ben: *track balance for every company*. The lapse gained phase-attributed balance deltas and a
   debt table; two differential runs named the drains — construction materials at the ceiling,
   and the tick-20 supply-factor cliff on an unmet power draw.
6. NR-782 (a)+(b) approved: the supply floor and the no-wire rule. The cliff is gone.

### The session's number

Standard industrial lapse, 80 warm / 120 measured, session-start tables against the engine now:

| quarter 81 → 200 | baseline s0 | baseline s1 | now s0 | now s1 |
|---|---|---|---|---|
| operating-positive corps at end | 4 of 37 | 0 of 44 | 46 of 61 | 31 of 55 |
| corps in debt at end | 7 of 37 | 12 of 44 | 6 of 61 | 4 of 55 |
| median operating net / qtr at end | −3.0 | −3.0 | +16.9 | +1.7 |
| median balance at end | 1,022 | 908 | 4,424 | 3,797 |
| active buildings at end | 9 | 4 | 140 | 85 |
| valued production 81 → 200 | 1,091 → 181 | 1,245 → 685 | 20,761 → 4,364 | 5,775 → 3,531 |
| interest share of net loss at end | 84% | 82% | 5% | 70% |

**Success on the solvency half; not yet on growth.** Valued production still falls across the
thirty years, from a level ten to twenty times the old one; the field runs at a mean supply factor
of ~0.57 for want of power; 42 of the 57 remaining debt entries are processors converting at a loss.

### Closed, filed, archived

BL-744 (recipe margin anchor) closed with BL-740; sprint 31 closed with its retro; **sprint 33**
opened on the growth half — BL-746 stage 2 (generation bootstrap) first, then BL-745, BL-738,
BL-726, BL-725 — with a done-when of production holding or growing over the run, a majority
operating-positive, mean supply factor above 0.8, interest under a quarter of net loss. NR-775..782
ruled and archived; the review queue is empty. Requirements groups archived. `NEXT_SESSION.md`
is superseded by sprint 33's plan.

---

---

## 2026-09-02 (BL-744 stage 2) — The tables clear the anchor, and the field still falls

**Mode:** Full. **Runtime:** the same session as the opening, after Ben's three rulings via
the form (reading A, processing base_rate 16, extraction fixed costs cut); ~6 full builds,
~25 harness builds, 5 lapse runs.

### Built

**The retune.** Both producer base_rates doubled (40:16, the 2.5:1 ratio economy.lua's own note
demands); extraction maintenance 5 → 2, wage 8 → 4; processing maintenance 10 → 2 (NR-779);
25 goods re-priced off their cheapest in-band route at k = 1 with alternates at k = 0 plus the
floor half (NR-780); **base prices era-banded** — `base_price_ancient` in world_gen.lua, a
`base_price_for_epoch` accessor, markets seeded from the band's table (NR-778); five recipes
re-costed; regolith 0.6 → 1.0. `recipe_margin` ALL PASS in both bands and registered with
ctest (script-rooted). BL-740 closed with it.

**Consequences absorbed.** Ordnance 43 → 140.8 broke Ben's military value anchor (19/19 rows
out of band), so unit wages and hire costs moved ×2.776 (NR-781's sibling, recorded on the
table); `value_anchor` R6 re-aimed from "one common markup" to the anchor rule; `upkeep_harness`
U1 reads the authored prices instead of a mirror; `spawn_solvency` and `upkeep_harness` CMake
targets gained the config TU they always needed; the era-band enum moved to `era_band.hpp`
after the first cut put the accessor in a Lua-linked TU and every Lua-free harness failed to
link. `haulage_measure`: the ceiling bound relaxed 14.07 → 8.84 (cheapest good now 1.0), so
MARKETS.md's derivation clears again; RESOURCES.md and PRODUCTION.md price tables rewritten.

### Verified

Release-path battery green: value_anchor, era_roster, price_band, throughput_field_census,
building_upkeep, unit_march, condition_set, world_determinism, spectator_determinism,
upkeep_harness, demand_census, haulage_measure. Deliberately red: chain_depth's named-list
guard (unchanged). Red for design reasons, left standing for Ben (NR-781): tier_margin R2
(mining 28.2 vs refining 24.9 per building-tick) and spawn_solvency R3 (seated 3.1× the field
per holding). `ctest -j6` is unusable here — 60 s Debug timeouts on every world-building
harness are contention, not verdicts — and `nmake all` stops at battle_engagement_harness,
which has not compiled since the mercenary teardown (BL-731 gains the note).

### Addendum, same session — the review queue ruled, and the ancient chain gets shorter

Ben ruled the four open entries via the form. **NR-778 overturned:** one price table, and the
ancient chain to steel shortened to depth one rather than banded prices. Applied as
`steel_bloomery` (Bloomery Furnace: ore + timber → steel, appended so ids hold), the Smithy
re-costed as the deeper alternate (0.4 blooms + 0.2 charcoal), the banded-price plumbing removed
from config, seeding and world_gen.lua, and the single table re-derived at the larger of the two
bands' needs — steel 16.1, machinery 61.0, alloys 85.0, consumer goods 61.0, ordnance 155.8,
spacecraft components 310.0, construction capacity 6.6, ladder ~124×. `recipe_margin` ALL PASS
in both bands against one table. **NR-779 and NR-780 ratified.** **NR-781:** `tier_margin` R2
now compares net per unit of output between the tiers (the per-tick figures stay printed);
`spawn_solvency` R3 holds the seated corp to the best rival's income per holding rather than
3× the field mean. The stale 1.433× markup derivations in world_gen.lua's comments were swept.

### Addendum, same session — every balance tracked, and the collapse has a name

Ben: *"run a harness which tracks balance for every company"*. `campaign_lapse` now attributes
every corp's balance delta per tick to the tick's phases — convoy legs, the agency batch
(build, hire, buyout: capital), the seven budget flows, the nation step, convoy arrivals, exits —
by snapshotting balances between phases (exact by construction, residual 0.000000, row C3), and
logs each corp's produced value, active/idle/limited/unstaffed/exhausted/building/mothballed
counts, labour and mean supply factor. `debt.csv` carries one row per debt entry with the
trailing-4-tick flows and the dominant drain.

**Industrial band, unwarmed, seeds 0/1: 129 of 129 debt entries dominated by expenditure**, and
expenditure > income in every trailing window. Two differentials say what is in it:
- Pool bids off (`reservation_mult` 0): no change. **Not the drain.**
- Construction materials free: the first wave nearly vanishes (debtors at tick 20: 26 → 4,
  median entry tick 22 → 34). **A processor costs 25 steel at 8–10× base while the scorer builds
  through the boom.** BL-745 re-scoped to this, priority B.
- **The cliff.** Between tick 20 and 21 active buildings fall 219 → 40 in one tick; unstaffed 0,
  exhausted 0, labour unchanged. The mean supply factor decays from 1.0 to 0 and lands at tick 20:
  `supply_decay_permille` 50 is 5%/tick, so a building whose power/timber/stone draw is never met
  is dark exactly 20 ticks in — and power is a grid good most tiles cannot receive. **Industrial
  goods-upkeep draws zeroed: no cliff, 207 active at tick 60, income ahead of inputs from tick
  25, debtors 19 → 5.** The ancient band has no such draw and recovers on its own. BL-746 (upkeep
  starvation cliff) filed at priority A; NR-782 asks which rule — a floor on the factor, no draw
  where no wire reaches, or a generation bootstrap.

### Addendum, same session — the lights go dim, not out

Ben approved NR-782 (a) and (b). **The floor:** `supply_floor_permille = 500` in
`economy.building_upkeep`, parsed and range-checked like its siblings; the decay stops there and a
stranded factor is lifted to it. **No wire, no draw:** an unreached building's grid goods are struck
from its basket before the shared draw, so it neither draws power it cannot receive nor weakens for
want of it, while timber and stone still bind. `building_upkeep` R8/R9 pin both with differentials
(floor 500 → 500 over 41 ticks, floor 250 → 250, lift from 120; unreached + on-grid stays 1000,
off-grid decays to 750, timber alone to 950). PRODUCTION.md § A shortfall scales output carries the
rule. **Re-measured:** no building reaches factor 0 at any tick; active buildings 224 → 221 over
ticks 20–60 on seed 0 (was 219 → 2), 178 → 137 on seed 1; income at tick 60 13.4k / 6.8k (was
132 / 2.9k); debtors 11 of 86 / 6 of 73 (was 22 of 66 / 13 of 58). The ancient band is byte-identical.
What remains: the field's mean supply factor sits at ~0.57 — most buildings run at the floor
because power still does not arrive (BL-746 stage 2, the generation bootstrap, NR-782 (c) held) —
and 42 of the 57 remaining debt entries are loss-converting processors, which is BL-745's ground.

### The finding — the anchor is necessary and was never sufficient

`campaign_lapse --epoch 1960`, seeds 0 and 1: valued production ~0 at every measured tick,
buildings active 1 of 22 reporting. The unwarmed 40-tick trace says why: 237 → 263 active
buildings and 13–23k/tick through tick 12, then expenditure at 1.5–2.5× income from tick 4
(inputs bought at the band's ceiling — processed goods ceiled in every market), interest to
1k/tick, exits from tick 20, survivors idled by their own agency from tick 25. Maintenance and
wages are now small; the drain is purchases. The ancient band takes the same wave and
**recovers** (66 active, +1.9k/tick, one debtor at tick 40). The anchor is evaluated at base;
the sim runs at the ceiling. **BL-745 (processor input bid cap)** carries the next lever: a
processor's input bid capped by its recipe's live output value, the M1 identity at the tick.

---

---

## 2026-09-02 (sprint 31 opens) — Every recipe priced at base, and most of them lose

**Mode:** Design → Light build (one harness, one data table) → sprint bookkeeping. **Runtime:**
one session; sprint 29's branch merged and pushed first (the ground bake + sprint 30 wave 1).

Ben's brief: a robust market where every player makes a steady profit, and *"the simplest way to
do this is to ensure that all recipes (at base price) make a greater profit than marginal
costs"* — the obvious part the demand work walked past. Agreed, with two qualifications now in
`PRODUCTION.md` § The recipe margin anchor: base is the middle of a band, so the rule has a second
half at the price floor (BL-740's form, whole roster); and the anchor is necessary, not sufficient
— the 2026-09-01 ledger's structural −755/qtr and the year-30 debt spiral are separate levers.

### Built

**BL-744 stage 1 — `tools/verify/recipe_margin`.** Loads the three authored tables through a live
Lua state (NEEDS_LUA), prices every processing recipe and every `k_extractable` target in both
bands, asserts M1 (margin ≥ k × marginal cost at base) and M2 (fixed cost covered at the floor at
typical staffing), R0 non-vacuity, R5 unpriced inputs, R6 differential red-proof. Knobs in
`economy.recipe_margin_anchor` (k = 1.0, Ben's sentence verbatim; W = 0.5, generation's seeded
staffing). Not `add_test`'ed until the tables clear it. CMake target declared.

### The finding — the opening table of sprint 31

| band | priced recipes | M1 red | any positive margin | clear 2× | M2 red | extraction M1 / M2 red (of 18) |
|---|---|---|---|---|---|---|
| ancient | 19 | 18 | 9 | 0 | 18 | 1 / 16 |
| industrial | 25 (+2 propellant, exempt) | 23 | 16 | 0 | 23 | 1 / 16 |

The shape: the mid-chain is authored at zero or negative value-add **before wages** — steel 8 from
7.0 of inputs, refined_copper 7.5 from 6.0, silicon 5 from 4, food_rations 6 from 6, clean_water 3
from 3, consumer_goods 12 from 14, steel_from_blooms 8 from 22 — and the wage per batch
(12 / 8 = 1.50) eats what is left. Only glass (ancient), medical_supplies and
spacecraft_components_heavy (industrial) clear k = 1. Extraction clears M1 everywhere but regolith
and fails M2 on 16 of 18: at the floor a mine at W = 0.5 earns 2.5 × price per tick against 9 of
wages + maintenance. Steel's authored margin was already recorded as positive in `tier_margin` R7's
comment (7.0 in, 8.0 out) — true before wages, false after.

### Filed

BL-744 (recipe margin anchor) — the retune is stage 2; BL-740 (maintenance floor anchor) folded
into the same instrument and re-sprinted. NR-775 (the two delegated constants), NR-776 (retune
direction: rates and costs first, input quantities, prices last — Ben's call). Requirements group
`recipe-margin-anchor` R1 complete, R2–R3 pending.

### Sprints

On Ben's instruction every prior sprint is archived: 21 and 26 (were "complete", now `closed`),
25 and 28 (`superseded`, never opened), 27, 29 and 30 (`closed` with retros). **Sprint 31 opened** —
long-term market viability, every recipe pays at base — with the done-when on `recipe_margin`
R1–R4 green in both bands and `campaign_lapse` on the industrial band operating-positive.

---

---

## 2026-09-02 (sprint 30 opens) — The ground gets its edges back, and the land tilts

**Mode:** Full, batch (BL-736 + BL-737), committed as one intertwined change on Ben's
call — he paused the pre-commit review fleet and took the improvement as-is, noting *"I
expect we will want a different approach later on"* (the 3D milestone stays the likely
destination; everything here — brushes, stamps, the tilt seam — carries into it).

**BL-736 (ground sharpness pass).** Diagnosis first: texel:pixel is ~0.85–1.0 at every
rung, so "still a general blur" was edge content, not resolution — the new ground HUD
line (tier, texel/px, chunks; the texel renderer's polygon count) makes that measurable.
The bake became an apron orchestrator (A=6, post passes can't seam at chunk edges):
cover-boundary + shoreline INK (1 px, follows the warped organic edge for free),
UNSHARP mask at ≥ 40 px/r (tag-gated, kernel-reach checked so the survey mask stays
byte-exact), grade haze halves / contrast rises with resolution. The v7 forest preview
reads like a hand-inked map.

**BL-737 (stepped tilt).** The reference mock's feature, taken: rungs 3/4 view the land
at 22.5°/45°, axonometric, tilt a pure function of zoom, plain canvas only. The camera
is ONE vertex-range squash about the canvas centre plus one inverse on the cursor —
every existing hit test unchanged; the tilted rungs bake OBLIQUE tiers (tier keyed
ppr+tilt): height displaces rows into real hill silhouettes (double-gather resolve,
masked re-resolve locks so peaks truncate at the survey mask), and trees STAND — trunk,
upright canopy pre-stretched 1/cos(tilt), shadow left on the ground plane.
ground_bake_check grew to 17 checks (P9: oblique determinism + wrap); live on the
Release build: click/hover/step all correct at the tilted rungs.

**Caveat recorded:** the 21-agent review fleet over this diff was stopped before its
verdicts on Ben's instruction — this batch, unlike waves 1–2, shipped without the
adversarial pass. The next session should treat the tilt/apron seams as unreviewed.

---

---

## 2026-09-01 (BL-735, wave 2) — The ground sharpens, steps, and stops stalling

**Mode:** Full. **Runtime:** same evening as the BL-732 delivery; Ben judged the first bake
live and ruled: not smooth enough, too blurred (approximate C-F's 3D read with STEPPED zoom —
2.5D), borders way too strong (muted palette, 1-tile glow).

### Built

**Stepped zoom + tiers.** Planetary zoom is a fixed ×2 ladder (`planetary_zoom_stepped`,
kMinZoom × 2^k, five rungs — kMinZoom×16 ≈ kMaxZoom, so the ladder spans the old continuous
range exactly); wheel and `=`/`-` step it, upper rungs stay continuous, verify `set_zoom`
stays free-form so scripted framings hold. Each rung pairs a bake tier {far 6, 12, 24, 48,
96 px/r} — never magnified more than ~7%, minification capped at 2:1. ACTIONS zoom entries
updated.

**Resolution-adaptive bake.** The bake's softness was canonical-scale (1.4 tiles ≈ 70 px of
blur at the close tiers): past the 24 px tier the interpolation weight EXPONENT steepens
(coverage radius can't drop below one tile; crispness comes from the falloff), detail/noise
amplitudes lift, a fine grain octave and a warm/cool colour mottle join. Diagnosed via a
temporary tier/chunk print — the tier pipeline was correct; the mist was bake content.

**Threaded bake.** All baking moved to a worker thread against immutable source snapshots
(generation-guarded, self-contained jobs, LRU-capped tiers, far-hash once per source
generation); the render thread hashes, enqueues, uploads, publishes. `--verify` stays fully
synchronous. Until the far page lands after a body switch, the vector fallback carries the
frame — a visual pop traded for zero render-thread stalls.

**Muted borders (the BL-734 partial).** Band collapses to the single frontier ring at 0.35
alpha; wash AND stroke draw the nation colour pulled 0.55 toward its own luma (the corridor
hover label keeps full identity). PLANETARY.md's falloff table rewritten with the ruling.

**Release staged for play.** The roughness Ben felt was partly the Debug build staged at the
granted path — the Release play build now sits there instead.

### Checks

ground_bake_check 10/10 unchanged; ground_bake.lua regrown to per-rung captures (every tier,
the 12 px rung included after review) with `frames(2)` settling the request latency. A
21-agent adversarial review over the integrated diff (4 dimensions → per-finding refuters)
confirmed 14 findings, 3 refuted; the critical family — a generation bump or failed texture
allocation orphaning an in-flight bake with its `queued` flag stuck, bricking chunks or
darkening the far page until a body switch — was fixed by clearing pending flags
unconditionally in upload, re-arming the far hash on an orphaned drop, gating source refresh
on an in-flight counter, flooring eviction at 2× the wanted set, raising tier headroom to
1.2× (0.93 sent every rung one tier high), and accumulating fractional wheel deltas to whole
notches. Doc fleet-catches fixed the ~7% magnification overclaim (fit-derived hex sizes; the
top rung's large-window bound is now stated), ACTIONS' stale expected_output, R4's
unprovable promise, and BL-734's stale remaining-work line.

---

---

## 2026-09-01 (BL-732 delivery) — The ground bakes, and it looks like a planet

**Mode:** Full. **Runtime:** one session; one cold-configure + full Debug build in the fresh
worktree, ~8 incremental builds, ~4 verify runs, one live computer-use pass. **Requirements:**
`ground-bake-renderer` R1–R4 all complete.

The procedural-first half of BL-732 (ground bake renderer): the Planetary canvas ground now
draws from CPU-baked painterly chunk textures on the plain canvas, with the classic vector
path surviving under every lens and wherever a chunk is not yet baked.

### Built

`src/ui/terrain_palette.{hpp,cpp}` (pure palette extracted from hex_render, byte-identical
delegation, compile-time layout guard); `src/ui/ground_bake.{hpp,cpp}` (pure bake: class-
separated tile interpolation, two-octave **domain warp** for organic coastlines, fractal-
detail **hillshade** low-octave-gradient, period-snapped noise lattices, separable near-future
grade); `src/core/ground_layer.{hpp,cpp}` (SDL chunk cache: synchronous far page, budgeted
512 px chunks against the canvas's ground_request, content-hash invalidation); canvas
integration (chunk quads under everything, per-tile fill/texture skip via `on_bake`, washes
re-expressed as translucent overlays, no-grid rule live); `tools/verify/ground_bake_check`
(10/10 + six param-variant preview PNGs per run); `scripts/verify/ground_bake.lua` (7
captures incl. a bare pair via the new verify-only `set_border_band` toggle).

### Look iterations (the useful failures)

Round 1: blend radius 1.55 → mush. Round 2: radius 1.15 → hex mosaic (per-tile jitter is the
mosaic dial). Round 3: fractal gradient at full frequency → speckle (gradient must read the
LOW octave only, half-cell central differences). Round 4: single-octave warp translates hex
corners without breaking them; big warp swirls texture → two-octave warp for the boundary,
detail/grain sampled UNWARPED. End state genuinely reads in the C-F family.

### The BL-734 evidence this produced

Over muted graded ground the analytic chrome inverts its old contrast relationship: the
national border band (hex-scalloped, full-strength, on every coastline) is now the loudest
mark on the map, and fog steps read hex-crisp over organic ground. Captured in the
wide/play vs bare pairs — the layer-contract ruling now has its exhibits.

### Notes

Debug-build chunk bake can jank the first seconds after a body switch (CPU bake, 2
chunks/frame budget; far page covers meanwhile) — a future threaded bake if it matters.
The computer-use grant resolved to a pruned worktree's exe path (the standing trap); fixed
by staging this build at the granted path — the Start-menu ProjectIo shortcut now runs
this build. story_check's 55 fails and the ACTIONS/NEEDS_REVIEW mirror staleness predate
this session and were left untouched.

---

---

## 2026-09-01 (sprint 29 opens) — The ground gets a mechanism

**Mode:** Design (research + authority docs, no code). **Runtime:** one short session, parallel
worktree while sprint 28 runs.

The brief: can the canvas move to much more detailed rendering — a tile renderer, the planet
canvas view? Research first, authority docs after, questions to Ben as they arose.

### Researched

`docs/research/CANVAS_RENDERING.md`: the option ladder (richer vector / textured hex atlas /
baked terrain chunks / SDL3 GPU pipeline), the finding that tiles already carry everything a
detailed renderer eats (continuous `height` from BL-517, river edges with flow, graded cover),
and that the current 2D backend supports textures, render targets and textured meshes unused.
The advisor's pick was baked chunks; Ben ruled otherwise, which is what the ladder was for.

### Ruled (Ben, the design form)

Authored **hex-tile atlas** on the current backend; scope the **Planetary tile grid** only;
**authored raster assets** from reference images (coming later); **ambient animation** in scope
(flipbook); layer contract **held for the reference images**.

### Landed

`docs/ui/RENDERING.md` (new authority: mechanism, manifest + vector fallback-by-coverage,
variant hash, transition fringes, verify-pinned flipbook clock, far page at the 7 px pivot,
asset policy); TECH_FOUNDATIONS amended (the "not a prototype concern" sentence overturned,
decision-log row added); PLANETARY.md pointer; CLAUDE.md router row. Items: BL-732 (hex tile
atlas renderer, buildable against a placeholder atlas), BL-733 (tile art asset pipeline) and
BL-734 (ground/chrome layer contract) — the latter two in review.json, blocked on the reference
images. Sprint 29 opened in sprints.json.

### Round 2 — the reference images arrive (same day)

Ben pointed at `Ui-Development:docs/ui/design/renders/map/` — a whole style workstream
(owner: Joe, `GLOBAL_STYLE_SHEET.md`) with two judged rounds. The images falsified the
morning's atlas ruling: the direction is grid-free continuous terrain, oblique camera,
installations as real geometry. Second form ruled: **C-F ratified** (painterly relief +
near-future grade); mechanism **switched to baked chunks** (hillshade + authored biome
brushes); **no on-ground grid** (selection/hover hex only, amber); **installations as
rendered geometry, glyphs retire from the canvas**; camera **staged** — 2D bake now,
oblique end-state (2.5D vs 3D, trade-offs in the research note § The end-state choice)
as a future milestone; design docs **merged to main** and registered in the router.

RENDERING.md rewritten to the baked-chunk mechanism; TECH_FOUNDATIONS re-amended;
style sheet gains the Planetary-map sub-track with the ratification; BL-732 →
GROUND_BAKE_RENDERER, BL-733 → BIOME_BRUSH_ART_PIPELINE (unblocked), BL-734 re-scoped
to the surviving channels (both review.json blocks resolved).

### Open

it4 supporting frames and glyph round 5 remain with Joe; the 2.5D-vs-3D end-state call
is Ben's, parked until the staged bake is real.

---

---

## 2026-09-01 (success-lever session) — The two buyers come back, and the sweeps get their design

**Mode:** Design → Full (two worktree agents) → integration. **Runtime:** one session; one full
build; ~10 harness builds. Sub-agents: two economy-dev builders + two cold reviewers.

The brief: design tests that inform each tuning lever for player success (target: 0 research
points, a functioning economy, influence around spawn, measured as GDP and net-income trend), with
a working spectator mode and time-lapses. A 12-agent corpus sweep produced the lever inventory;
Ben then ordered the two missing buyers built first.

### Built

**BL-644 (state channel).** The tenth budget line `space_programme`: whole-or-nothing lumps of
spacecraft_components + propellant, priced at the supplier's market (base_price fallback),
conservation-exact, consumed on settlement. Save v22→23. Cold review found no correctness defect;
integration added the missing coverage — R7k pricing twin, R7l rogue-claim claw-back (the claim
vector is wire-reachable), and R8, an end-to-end `run_nation_step` row red-proved by severing the
wiring. `nation_budget_harness` 49→62/62.

**BL-647 (endemic luxury).** `inject_endemic_demand`: wealth-scaled (treasury + positive domiciled
balances), nation-flavoured by a pure seeded hash (a real FNV diffusion defect caught by the
harness's own asymmetry row, fixed with fmix64), the four luxuries into `k_extractable`. 29/29,
every row mutation-proved. Census after: luxuries carry demand and price 3.4–4.0× base in band;
rivals extract and sell into the pull end to end.

**Integration.** demand_census gains the two channels honestly (END market bid; `st` paid pool
draw on its own line), endemic + state/pl columns; chain_depth gains both injectors — the
names-no-pass list shrinks 8→3, all owned; `read_resource_map` rejects NaN/negative weights.

### Filed

BL-723 (campaign lapse instrument) + BL-724…729 (the sweep battery: spawn distribution, price
levers, debt dynamics, scarcity geography, demand composition, interactions), BL-730
(trade_goods_misc buyer), BL-731 (nation_scorer_harness rot). NR-774 carries the four
definitional calls (GDP := valued production; influence := market share + footprint + routes;
band := industrial 1960; proxy := corp AI with stated brackets) for ratification.

### Found, not fixed

The ceiling derivation has drifted: `haulage_measure` now demands ceil > 14.07 at the binding
case against the authored 10.0 (p90 haul 5.65 vs the 1.67 the 10.0 was derived from). Trade is
healthy (1,481 dispatches vs the 1055 baseline) — but the worst-tail pair is unservable at any
scarcity. Sweep 2 (BL-725) owns re-deriving it. chain_depth's named-list guard stays
deliberately red on tools / rigging / trade_goods_misc.

### Open calls

NR-774 (the four definitions); the player-corp supplier exclusion under spectate (BL-644's
delegated call — the BL-409 no-subject reading says it should lift); lump sizes 25/50 first-cut.

### Addendum, same session — the operating-loss block: two land, one waits, one teaches

Ben ruled on the six proposed operating-loss methods: infrastructure channel **in full**;
building upkeep **yes** ("Building should have upkeep too"); extraction throttle-on-floored-price
**REJECTED** — *"we should see players penalised for overinvesting in mining, and we should see
the market decide what is profitable. Eventually a deposit runs out, and so it is up to a player
to pace their consumption"* — a dated narrowing worth remembering: no price-triggered supply
throttle, ever; the market's verdict and deposit depletion are the pacing mechanisms. Idle-floor
trim and the maintenance anchor agreed; firm exit pending his read of the re-explanation.

**Landed:** BL-643 (network upkeep, the BL-644 template, 72/72 red-proved) and BL-739 (idle floor
to data, 0.30 → 0.15, all seven call sites). **Found:** the state channels' wallet is missing —
one nation in 43 holds any treasury (BL-741), and state purchases read pools auto-surplus already
swept (BL-742). **Taught:** BL-738's stage-1 repair rates ran a full campaign cell and made the
field WORSE (extraction operating net −6.75 → −31.29; cost side without the income side), so they
were withdrawn with their measurement written at the table and BL-738 now requires BL-741/742.
The clean positive: stone off the floor, 0.42× → 1.38× base — the price signal works; the money
loop behind it is what is missing. BL-740 (maintenance anchor) waits for its measured table —
BL-739 just moved its baseline.

### Addendum, same session — NR-774 ruled, and the demand curves split the bands

Ben ruled the four calls via the form (GDP = valued production; influence = share + footprint +
routes; **band = industrial 1960 only**; proxy = corp AI, brackets stated) and held the
sprint-31 spawn-intelligence idea. The full demand curves then ran on both bands (2 seeds per
cell, pop ×0.5 → ×4):

- **Ancient**: a real, noisy climb — −6.9/qtr at ×0.5 to **+2.4/qtr at ×2**, with an optimum
  near ×2 (×2.5 falls back to −6.5: past the optimum, extra demand hits price ceilings and
  raises everyone's input costs). Two-seed cells are noise-prone (×1.25 is an outlier).
- **Industrial (the ruled band): demand scaling does NOT fix it.** Mean operating net wobbles
  −7.3 → −4.5 with no monotone trend, op-positive corps 0–2 of ~37 at every level, and valued
  production collapses (×0.13–0.29) as exits cull nearly half the field. The industrial band's
  binding constraint is structural — the ceiled mid-chain (machinery/electronics), the missing
  endpoint channels, and exit thresholds tuned on ancient numbers — not the demand level.

The programme consequence: for the ruled band, the next lever is mid-chain supply and endpoint
structure (and a band-sensitive exit threshold), with the demand knob shelved until those land.

### Addendum, same session — firm exit lands, and the first sweeps speak

**BL-743 built and verified** (7/7, conservation and exemption rows red-proved): insolvency
finally has a consequence, the CANCEL/DROP half shared with the buyout so the two ends of a
corporation cannot drift, the player exempt absolutely.

**The first sweep rounds ran** — 36 cells plus 3 industrial spot cells — and one instrument
lesson came with them: `world_params::epoch_year` defaults to **0**, so every lapse run to this
point measured the **ancient band**, not NR-774's declared 1960 start; the manifest now names its
band. The findings, band-labelled:

- **Demand composition dominates** (sweep 5): pop demand_scale ×2 flips the ancient field to
  **mean operating PROFIT (+2.36/qtr)** — the first operating-positive field ever measured.
  The bg knob is inert in-band (the stopgap is industrial-only), and on the industrial spot
  cells bg×2 makes things *worse* (more demand for ceiled mid-chain goods).
- **Abundance barely differentiates** (sweep 1): sparse/lean/standard within ±1.2/qtr of each
  other — in a demand-bound economy, more deposits do not help.
- **Exits work**: end-of-run debtor counts fall ~57 → ~20; the industrial band's ~45% cull rate
  says the thresholds are band-sensitive — an exit-threshold sweep axis is owed.
- **The field culls but does not regrow** (valued production ×0.67–0.73): the growth half of
  "profitable and growing" now hinges on the AI's build tempo and the demand level.

### Addendum, same session — the money loop closes a turn, and the diagnosis lands

Ben asked why the after-fixes lapse still slides into debt: taxes or wages, or the market? The
field ledger exonerated both suspects — levies are 0.07% of drains, wages 13% — and named the
real pair: **maintenance is 80% of every credit leaving the field**, and the market mints only
1,485/qtr of external income against 2,241/qtr of drains: a structural −755/qtr the field must
lose collectively, whoever wins individually.

**Built on his ruling (firm exit yes; BL-741/742 first):** BL-741 — every nation levies its own
jurisdiction (one seeded author had left 42 treasuries empty); BL-742 — state purchases fall back
to market inventory when pools are swept (the measured industrial case), each channel keeping its
own shape, self-capped at the line share, settling as the documented unbacked-market debit.
BL-738's repair rates re-landed as stage 2. BL-743 (firm exit) filed fresh — the old BL-657/658
rows no longer exist in any store.

**The three-cell table** (baseline / after-fixes / round-two): extraction operating net −6.75 /
−31.29 / **−20.83**; subsidies 0.0 / 0.7 / **61.1/qtr**; treasuries 167 / 99 / **8,280**. The
circulation works — and exposed the next sink: **nations hoard ~68/qtr** because most budget
lines still have no consumer. That calibration belongs to the sweeps (BL-728/BL-729), the
spend-side breadth, and BL-743's exits — not to another ad-hoc rate change.

### Addendum, same session — BL-723 lands and the first film ships

The campaign-lapse instrument built, T0-verified (every validity row mutation-proved red), and
run: seed 0, shipped spawn, 120 measured quarters in 23 s. **The first baseline is the finding.**
Valued production grows only ×1.05 over 30 years; corps in debt climb 24 → 57 of 89; convoy
traffic thins 85 → 28; at year 30 interest is ~96% of the field's summed net loss — the debt
spiral, not operations, is the drag. Every top climber is a processing corp; extraction and trade
corps bleed. No runaway leader (top share 6.6%). The 33-frame time-lapse (one Corporation-lens
frame per game-year, whole-continent framing) delivered as a GIF. Sweep priorities this reorders:
debt dynamics (BL-726) and the insolvency-exit design look more load-bearing than prices; the
extraction-vs-processing margin gap wants sweep 2's eye on the levy and the band.

---

---

## 2026-09-01 (later) — The review queue drained, and the quadratic finally went

**Mode:** Design → Full → Corpus. **Sprint 27: still open.**
**Runtime:** continuation of the same session. Two integrating builds.

Ben answered five open calls and then made a sixth, larger one: *"Really we are relying on the fact
there is a review queue too much. Use judgment on things that seem obvious in hindsight, and close
all the remaining items - then archive."*

### The five rulings

**BL-417 taken (NR-769, option A).** The build score is `net / capex` — return on capital per tick —
at all three curve sites, including the road, which is priced *under* the build curve by
construction and would have outscored the site it exists to reach by two orders had it stayed
quadratic. What settled it was that two independent fixes hit the same wall one level down: BL-712
put Power Generation and Construction in front of the scorer for the first time and the scorer
refused them anyway; BL-711 gave the same shape from the other side with peat. `AI_OPPONENT.md`'s
own rule condemns an absolute contest in as many words, and `net²/capex` was one — the rule applied
to everything except the score.

Measured before reshaping: 15,549 candidates, quadratic median 13.54 / max 1884, linear median
0.106 / max 2.81. That puts builds in the same band as survey and hire, which is an incomparability
the quadratic was masking rather than resolving. **No fudge scale was added** to restore the old
ordering — that would be taking the change and cancelling it.

**`spectator_determinism`'s byte-identity row retired (NR-752).** It asserted world-content
stability, which is not what the harness is for; it had been re-blessed ten times and not one of
those moves was a spectator-mode fact. The provenance log is kept as the *argument* for the
retirement. The harness is ALL PASS for the first time in weeks with both real invariants intact.

**BL-713 scheduled after sprint 27** (NR-762), **BL-642's centre growth gated** (NR-773, Ben taking
the deeper option over the recommendation), and **NR-763 answered** — deeper chains, after the
demand channels.

### The probe that came back dead

NR-763's recommended first move was to vary `max_logistics_reach` and re-read the completeness
spread. `demand_census` gained a `--reach` flag for it, and the sweep 24 → 4 removes **40% of every
market's reachable ground** — market 48711 loses 51% of its in-reach tiles — and **not one market's
completeness moves**, on either band, to four decimal places. The raws a market needs are all in its
inner catchment; the ground a tighter budget removes carries duplicates. Geography cannot bite
because there is nothing distinctive at the edge to lose. That leaves the saturation structural.

### The drain

**117 open → 0.** The finding that matters is *why* there were 117: most entries named **work**, not
a judgement. The queue had become a shadow backlog, and a backlog nobody reads is not a record.

Ten items now carry the real content — **BL-713** (harnesses build the app's world; 48 of 51 skip
the app-start tail, 22 of those run the AI in an unsurveyed world), **BL-714** (instruments that
cannot see their subject), **BL-715** (the save seam past the world), **BL-716** (the tech tree is
inert and its gate ids are wrong), **BL-717** (designed but silent), **BL-718** (the name column is
one character — a class, not three instances), **BL-719** (shell defects), **BL-720** (the seeder
cannot see processing demand), **BL-721** (paid for outcomes has no mechanism), **BL-722** (the
live-click debt). Every one of the 125 entries is archived with a resolution saying where it went.

`CLAUDE.md` Rule 0c gained the discipline that keeps it drained: a call only Ben can make goes in
the queue; work goes in the backlog; a fact worth remembering goes in the comment next to the code.

### One red, fixed rather than papered over

`decision_trace_harness` T2's anti-vacuity guard went red under the linear score: the calmer scorer
stopped overflowing the 256-entry ring (269 → 207 pushed). The guard was right and the tick count
was the problem — 269 against 256 is five percent of headroom, and the decision rate is a property
of the **scorer**, not of that file. Re-measured (2400/207, 3600/267, 4800/327, 6000/387), re-sized
to 6000 for 51% headroom, with a `--ticks` flag so the next scorer change re-measures instead of
guessing.

---

## 2026-09-01 — Sprint 27 block 2: two scale-blind selections, and two instruments that were lying

**Mode:** Full (Batch Delivery). **Sprint 27: still open — the channels remain.**
**Runtime:** one session, main-session only, no worktree agents. Two integrating builds.

Five commits. Four backlog items and the sprint's own measuring instrument.

**BL-710 — save_roundtrip compiles again.** It had not since `cc88997c`, and two save-version bumps
landed inside that window (v21 power, v22 construction), both appending to the format, neither
verified by the harness whose whole job is that. Six regions removed by deletion, not repair. The v6
and v7 refusal rows stay: what they assert is the version contract, which the tear-out did not
touch. Green at v22, 63 PASS / 0 FAIL, both dead bumps carried.

**BL-712 — the scorer chooses between categories.** Two argmax loops, not the one the item named.
The build candidate took a site's highest-margin recipe, so power at net 3.98 could never out-rank
price-ceiled electronics at net 290 and no rival ever built a plant. Now a best per `recipe::group`.
The recipe margin-chase carried the same defect plus two of its own: it used the browse index as an
absolute recipe id, and it proposed cross-group switches **that the seam has refused since
2026-08-16** — Ben's own BL-434 retraction. One proposal per building per evaluation, so the refused
proposal starved the legal within-group switch. That is the sharper finding, and it is now a doc
section: *a scorer that proposes what its seam forbids cannot tell a refusal from an absence.*

**BL-709 verified, and deliberately left open.** Three of four requirement rows land. R1 does not:
on the ancient band `construction_capacity` is produced **0.0** over 80 warm ticks with two yards
standing and both inputs on the shelf; on the industrial band both yards are gone by tick 80. The
linter's false-open warning said *a partial slice can land under an item that is legitimately still
open*, and that turned out to be the right reading rather than a formality.

**BL-711 — every resource reaches the scorer.** `rank_extraction_sites` kept a global top-8 over
deposit × affinity × demand; deposit magnitudes span three orders, so all eight rows were iron_ore
and clay, peat, sand, hides and fibre were never *candidates*, anywhere, in any world. Now a
per-resource top-K. **Coal goes from zero mines in any world ever to 25** — the chain NR-766 named,
feeding the industrial band's largest recipe. This is BL-440's own trap one altitude up, which is
why the rule now lives in `AI_OPPONENT.md` rather than in that item's comment.

### The two instruments

Both were found by accident, and both had been reporting confidently.

`demand_census` never called `init_survey_states`. Every body stayed hidden,
`rank_extraction_sites` gates on survey visibility, and **the corp AI built zero extraction sites in
every census ever run.** It surfaced because BL-711 came back byte-identical there while the probe
showed coal going 0 → 25. One line, matching `ai_skill_harness`'s own note from the previous
session — same class, same day, same answer. It moves every reading the file produces, so the three
runs are recorded side by side to keep the instrument effect and the item effect separable:
coal 0.0 → 0.0 → 632.7, clay 0.0 → 0.0 → 349.1, hides 0.0 → 0.0 → 353.9, buildings 335 → 419 → 440.

`ai_skill_harness` — **the sprint's stated success criterion** — is structurally blind to a
per-category change: three recipes in its hand-built registry, `group` set on none, so all three
fall to the default. BL-712 returned byte-identical numbers before and after, and that identity is
the proof rather than a null result (NR-771).

### Decisions taken, and what is Ben's

Taken: the census survey fix (NR-772 — an in-repo precedent from the previous session, reversible in
one line); confining the recipe chase to its own group (aligning the scorer with a ruling Ben had
already made at the seam, not a new rule).

His: **NR-769 is the one that matters.** BL-712's fix works — `Power Generation` and `Construction`
went from zero build candidates to 168 and 430 — and they still never win, because `net²/capex` is
itself an absolute contest. § Selection must be scale-free condemns that curve in as many words;
§ Scoring says its retention is BL-417, his call. BL-711 left the same fingerprint independently:
peat reaches the scorer, both slots placeable, still no site. Also NR-770 (the yards), NR-771/772/762
(the blind instruments), NR-773 (BL-642's fork), NR-767/768.

Nothing was re-blessed. `spectator_determinism` R2 moved further from its already-150-commit-stale
golden and is reported with both hashes; `ai_skill_harness` stays at 25 known-red band failures.
That harness moved and **not for the better on aggregate** — three seeds better, two worse — which
is the sprint's own thesis as a number: rivals now reach the whole resource field, and reaching more
of a world with almost no demand means building more into debt. The channels are the answer to that,
and they are what is left.
