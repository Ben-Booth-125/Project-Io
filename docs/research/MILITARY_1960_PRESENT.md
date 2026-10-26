# Military 1960 → present — Research and sprint 58 proposal

> **Proposes:** what real militaries were like from 1960 to today, as mechanism · what comparable
> games got right and wrong · a campaign force model across four theatres (land, air, sea, space)
> · unit classes, stats, equipment and doctrine · fronts · war declaration and peace · how rivals
> use force · cost anchors · what in Ben's 2026-10-26 brief is unrealistic.
> **Not here:** how force works today (military/MILITARY, the authority) · the Era 1 catastrophe
> and Alarm (economy/ERAS) · stance as a quantity (politics/RELATIONS) · the near-space classes
> (economy/SPACE_ASSETS) · the rival scorer's grants (ai/AI_OPPONENT § 11).
> **Confused with:** military/MILITARY.md, generation/MILITARY_HISTORY.md, economy/ERAS.md.

> **Research scaffolding — not authority.** Written for the sprint 58 design session, from Ben's
> brief of 2026-10-26. It settles nothing. What Ben rules is written into `military/MILITARY.md`
> (and its siblings) the moment it is settled; this note then stays as the record of why.

Real history here is a **mechanism reference**. Weapon and doctrine names below are for the
reader; no proper noun transfers into the game (standing rules § Terms & docs).

---

## 1. What changed, 1960 → today

Five shifts carry almost everything. Each one is a mechanism the game can express.

| Span | What changed | Mechanism for the game |
|---|---|---|
| **1960s** | Mass conscript armies; the nuclear triad; the main battle tank absorbs medium and heavy; SAMs end high-altitude bombing; helicopters make infantry airmobile; first recon satellites. | Manpower is cheap and plentiful; nuclear capability is posture, not a battlefield tool; ground-based air defence exists from day one. |
| **1970s–80s** | Guided anti-tank missiles make infantry dangerous to armour (1973); precision bombs; composite armour; anti-ship missiles sink ships at range (1967, 1982); look-down radar, AWACS; stealth. | Hard attack spreads to infantry. Quality starts beating quantity. Detection becomes the naval stat. |
| **1991** | The first space-enabled war: GPS, satellite recon and comms, stealth and precision collapse a large conventional army in weeks. | Satellites are a force multiplier, not a fighter. Information beats mass when one side is blind. |
| **1990s–2000s** | Smaller professional forces; armed drones; networked warfare; counter-insurgency; MRAPs. | Professional vs conscript is a doctrine choice with a cost curve. Drones begin as strike assets. |
| **2010s–today** | Cheap loitering munitions and FPV drones; electronic warfare decides who can fly them; artillery mass returns; missile defence works against short-range threats; sea drones deny water to fleets; industrial output decides attrition wars. | **War is an economy.** Shell and drone production, not unit count, sets the ceiling. Interceptors cost more than what they shoot down. |

Three facts from the current war are worth holding onto, because they make Io's premise true:

- **Consumption is enormous.** One side fired ~44,500 shells a day for a week in August 2024; the
  other ~14,600. Pre-war production estimates were far too small. A military's real limit is its
  munitions industry.
- **Drones are consumables, not aircraft.** Losses ran around 10,000 a month for one side in 2023.
  They behave like ammunition with a guidance package.
- **Volume still matters in a precision age.** Self-propelled guns that fire and move in minutes
  are the new standard.

### Doctrine families — mechanism, not names

A doctrine is a set of trade-offs a nation commits to. Six families cover the span:

| Family | Bets on | Pays with |
|---|---|---|
| **Mass and depth** | Artillery weight, reserves, conscription, echelons | Low unit quality, high casualties, huge munitions draw |
| **Manoeuvre and initiative** | Tempo, armour, junior-leader autonomy | Expensive professional troops; brittle when outnumbered |
| **Air-power first** | Air superiority, then precision strike | Enormous cost per airframe; helpless against dense SAMs |
| **Territorial defence** | Dispersed, cheap, terrain-hugging infantry; bunkers | Cannot attack; trades ground for time |
| **Sea control** | Carriers, escorts, open-ocean reach | Few, enormous, irreplaceable hulls |
| **Sea and area denial** | Missiles, submarines, mines, shore batteries | Cannot project; only keeps others out |

These map cleanly onto an Io tech pair or a doctrine slot (§ 6).

---

## 2. What comparable games teach

| Game | What works | What fails |
|---|---|---|
| **Hearts of Iron IV** | Soft/hard attack against a hardness share — clean and legible. Organisation as a retreat bar, separate from strength. Equipment as produced goods; fronts and battle plans automate a line. | Division designer and battle-plan micro are a skill wall. The AI cannot use fronts well. Piercing vs armour duplicates hardness. Air is an opaque per-region number. |
| **Victoria 3** | Fronts and generals with no unit micro; war as a consequence of the economy. | Too abstract: battles feel random, tech has no visible effect, the player has no lever mid-war. |
| **Unity of Command / Shadow Empire** | Supply is the game: units fight at what reaches them. Readiness and formations matter. | Steep learning curve; dense UI. |
| **Command: Modern Operations** | Real detection, real ranges, real munitions. | A simulator, not a strategy game. The level of detail to avoid. |
| **Supreme Ruler** | Covers 1960 → future at grand scale with unit production. | Unit micro across a whole world buries the strategy. |

**The lesson for Io:** HOI4's combat arithmetic, Victoria 3's hands-off fronts, Shadow Empire's
supply honesty — and visible numbers everywhere, so a tech or a shortage is felt in the next fight.

---

## 3. What Io already has

The brief lands on a working base (`military/MILITARY.md`). What exists:

- A **unit** is a group token on a tile: owner, count, roster type, supply factor, muster base.
- **Battles** open when a hostile nation's units share a province with the target's; fought in
  rounds, three per tick, with a **priced withdrawal**. Resolved by `resolve_campaign_battle`,
  which scores power through the Era −1 `resolve_battle`.
- **Muster** at a military base; equipment **bought off the shelf** on the order book.
- **Upkeep** is credits plus goods (ordnance, rations) — authored at zero, waiting for a baseline.
- **One reach field**: a unit beyond the logistics envelope decays. Marches draw **Logistic Points**.
- **Stance**: `declare_hostile` is directed, with the ambush property. No peace terms, no war goal.
- **Garrisons** per nation, static. A tech branch (`ST-FO`) already names mechanised divisions,
  radar, precision munitions, airmobile forces and a Strategic Deterrent.
- **Era 1's catastrophe is nuclear war**, decided by aggregate **Alarm** vs **Ceiling** on a seeded
  date — deliberately a scalar, not a weapon object (`economy/ERAS.md`).

The brief replaces the combat core and adds three theatres. It keeps the seams: muster, shelf,
upkeep, reach, stance, province battles.

---

## 4. Proposed model — land

### 4.1 Stats

Every land unit carries the brief's stats, with one addition (`defence` and `breakthrough` split):

| Stat | Kind | Meaning |
|---|---|---|
| **HP** | stored, per unit | Men and equipment fit to fight. 0 → the unit is destroyed (or surrenders, encircled). Refilled over time from equipment and manpower. |
| **Readiness** | stored, per unit | Cohesion in the fight — HOI4's organisation. Takes most hits. 0 → the unit **retreats** a tile away from the enemy. Recovers out of contact. |
| **Morale** | proposed: **per army or nation, slow** | Will to fight. Caps maximum readiness and scales recovery. Moved by supply, victories, losses, war support. |
| **Supply** | stored (exists today) | Share of the unit's draw that arrived. Scales attack and recovery. |
| **Soft attack** | per class × equipment | Hits per round against the soft share of a target. |
| **Hard attack** | per class × equipment | Hits per round against the hard share. |
| **Hardness** | per class × equipment, 0..1 | Share of the unit that is armoured. |
| **Defence** | per class | Hits absorbed when defending. |
| **Breakthrough** | per class | Hits absorbed when attacking. |

**The arithmetic** (HOI4's, which is proven and legible):

```
hits_on_target = soft_attack × (1 − target.hardness) + hard_attack × target.hardness
absorbed       = defender ? defence : breakthrough      (plus terrain, bunker, doctrine)
landed         = hits beyond what was absorbed land in full; absorbed hits land at a fraction
split          = ~30% of landed damage to HP, ~70% to readiness
```

The 30/70 split is what makes most fights end in **retreat**, not annihilation — which is true:
units become combat-ineffective long before they are destroyed.

**Recommendation: no piercing vs armour.** HOI4 runs both, and they duplicate one another.
Hardness alone carries "infantry can't hurt tanks" if infantry hard attack is low.

### 4.2 Classes

Brief list, with corrections (§ 10 explains each):

| Class | Hardness | Soft | Hard | Defence | Speed | Notes |
|---|---|---|---|---|---|---|
| **Infantry** | 0.0 | med | low → **med with ATGMs** | high | 1.0 | Cheapest; holds ground; anti-tank missiles raise hard attack by tech. |
| **Motorised infantry** | 0.1 | med | low → med | med | 1.5 | Infantry in trucks. Replaces "trucks" as a class. |
| **Mechanised infantry** | 0.6 | med | med | med | 1.5 | Infantry in IFVs. Already unlocked by `Mechanised Divisions`. |
| **Armour (MBT)** | 0.9 | med | high | low | 1.5 | One main battle tank line, by generation. |
| **Light armour / recon** | 0.5 | high | low | low | 2.0 | Optional; fast, sees far, dies fast. |
| **Artillery** | 0.1 → 0.6 (self-propelled) | very high | low | very low | 1.0 | Fights from the second line; the biggest munitions draw. |
| **Air defence** | 0.2 → 0.5 | — | — | low | 1.0 | **Missing from the brief.** Shoots aircraft and drones over its province. |

Helicopters are best as an **air** class (§ 5), not a land one.

### 4.3 Equipment and production

Units are **class × equipment generation**, not a free designer. A tank unit fielding gen-2 armour
is better than one with gen-1; the class is fixed, the kit is a good.

Equipment is a **good on the shelf**, as muster already buys it:

| Good | Feeds |
|---|---|
| Small arms | infantry, all classes |
| Armoured vehicles (gen n) | mechanised, armour, recon |
| Artillery pieces (gen n) | artillery |
| Ordnance (exists) | every unit's draw; artillery most |
| Air defence systems | air defence |
| Aircraft, helicopters, drones | air (§ 5) |
| Missiles | strikes, air defence interceptors (§ 7) |
| Hulls | ships — built at a shipyard over months, not bought (§ 6) |

**HP refills** by drawing equipment and manpower from the shelf. A unit whose kit cannot be bought
stays wounded — the economy is the war's ceiling, exactly as 2022 showed.

**Manpower is the Trade ↔ Conflict link the brief is missing.** Recommend: soldiers come from the
same `(nation, body)` labour pool buildings staff from (`economy/POPULATION.md`). A mobilised army
is an emptied factory. Conscript vs professional sets the trade-off: conscripts are cheap per head
and drain labour; professionals cost wages and are fewer and better.

### 4.4 Rendering

As briefed: a **rectangle within the tile** — count of units, HP bar, readiness bar. Morale as a
nation-level badge, not a third bar (keeps the counter readable at zoom-out). Class glyph from
`src/ui/icons.*`.

### 4.5 Bunkers

A **fortification building** on a tile: + defence to units on it, and + hardness to that tile's
defenders (concrete makes infantry hard to soft attack). Degraded by artillery and strikes; repaired
over time. Real modern evidence supports it: dense fortified belts stopped a major offensive in 2023.

---

## 5. Proposed model — fronts

A **front** is the line between two hostile nations' held ground. Units **assigned** to a front
spread along its border tiles, refill gaps, and fall back to the next line on retreat.

- **Built from what exists.** Garrison placement already finds "the member tile with the most hex
  sides onto that neighbour's ground". A front is that rule run over every border province.
- **Province battles stay.** The tested province envelope is still where fights happen; a front
  only decides which units stand where.
- **Two orders:** *hold* (default) and *push* (advance into the enemy province with the weakest
  defence-to-attack ratio). Push is the only offensive automation — no battle-plan arrows.
- **Why fronts matter for the AI:** a scorer cannot plan a campaign, and is forbidden to. It *can*
  score "how many units on which front, hold or push". Fronts make a competent rival legal.

**Grant flag.** Fronts move the player's own units automatically. The standing rules allow "save
one opt-out dial" of auto-action on the player's nation. Front automation is a **new widening**,
and it is Ben's to give (`ai/AI_OPPONENT.md` § 11).

---

## 6. Proposed model — air, sea, space

### 6.1 Air — acts on provinces

As briefed: an air wing is **based** at an airbase and **assigned** to provinces in range, with a
mission. It never stands on a tile.

| Class | Missions | Notes |
|---|---|---|
| **Fighter** | Air superiority | Air attack only. |
| **Strike / multirole** | Tactical bombing, superiority (worse) | Ground soft/hard attack. |
| **Strategic bomber** | Strategic bombing | Costly; dies to dense air defence. |
| **Attack helicopter** | Tactical bombing, short range | 1960s+ staple; cheap airmobile. |
| **Strike drone** (medium-altitude) | Tactical bombing, recon | Cheap, no pilot, no morale. |
| **Recon / AWACS** | Spotting | Raises everyone's hit chance in the province. |

**Stats:** HP, air attack, ground soft/hard attack, **air defence evasion** — not hardness (§ 10).
**Air superiority** is a per-province share; it scales the other side's tactical and strategic
missions and its ground units' readiness recovery. **Ground air defence** shoots every mission.

Small tactical drones (FPV) are better as **equipment** on land units — a soft/hard attack boost
that draws a drone good — than as air units. That is how they are actually used.

### 6.2 Sea — coast and ocean tiles

As briefed: patrol, escort, convoy raiding, on **sea zones** (groups of ocean tiles).

| Class | Hits | Hit by | Notes |
|---|---|---|---|
| **Submarine** | surface, convoys | destroyers, ASW aircraft | **Stealthy**: only engaged once detected. |
| **Destroyer / frigate** | surface, submarines, missiles | everything | The escort and the workhorse. |
| **Capital ship** (carrier / cruiser) | surface, shore, projects air | subs, missiles | Carrier = a mobile airbase. |
| **Amphibious / transport** | — | everything | **Missing from the brief.** How land units cross open sea. |

The brief's **one attack value** works if each ship also has a **detection** value and a **target
mask** (which classes it can hit). Recommend that over a second attack stat.

Sea ties to what exists: a raided convoy is SUPPLY's convoy; sea control is what makes overseas
supply flow (`military/MILITARY.md` § Domains and traversal already says overseas supply works).

### 6.3 Space — pre-space military

| Asset | Effect |
|---|---|
| **Recon satellite** | Lifts survey and activity fog over enemy ground; reveals unit stacks. |
| **Early-warning satellite** | Required for missile defence to fire against long-range missiles. |
| **Navigation / comms** | Precision bonus to strikes; better front coordination. Ties to existing Satellite Navigation node. |
| **Anti-satellite weapon** | Destroys a satellite. Raises everyone's Alarm. |

These are SPACE_ASSETS classes with a military yield (BL-1329, satellites and rovers). No orbital
combat between units.

---

## 7. Missiles, missile defence, and the nuclear question

**Missiles are munitions, not units.** A missile has no morale, no readiness, no position. It is a
**good** fired by a launcher (a land unit, a ship, an aircraft, or a silo building) at a target
province or building, consumed on use.

| Missile family | Range | Target | Counter |
|---|---|---|---|
| Loitering munition / one-way drone | short–medium | units, buildings | Air defence, EW |
| Cruise missile | medium–long | buildings, ships | Air defence |
| Ballistic (short / medium) | medium | buildings, airbases | Missile defence |
| Anti-ship | medium | ships | Destroyer point defence |
| **Intercontinental (nuclear)** | world | — | See below |

**Missile defence** is an interceptor stock at an air defence unit or a defence building. It works
well against short-range threats and poorly against long-range ones. **The cost exchange is the
game:** an interceptor costs far more than a cheap drone (§ 9), so defence drains the defender's
economy. That is Trade ↔ Conflict in one mechanic.

> **Ruled 2026-10-26:** C — tactical use permitted, strategic never fired. The same day Ben retired
> the Era catastrophe (`economy/ERAS.md` § The point of an Era), so the collision below no longer
> stands; strategic nuclear weapons now act through Alarm alone.

### The nuclear question — the brief collides with ERAS.md

`economy/ERAS.md` makes **nuclear war Era 1's catastrophe**, fired when aggregate Alarm passes
aggregate Ceiling on a seeded date. It is deliberately **a scalar, not a weapon object**.

A launchable nuclear missile contradicts that. Three readings:

- **A) Arsenal as posture (recommended).** Nuclear missiles are built, stocked and visible. They
  raise rivals' Alarm and deter attack (the existing Strategic Deterrent node). They are **never
  fired by a verb** — using them *is* the catastrophe, and the catastrophe test decides it.
- **B) Usable, and use is catastrophic.** A launch verb exists; firing one triggers the
  catastrophe immediately for everyone. Honest, but one click ends the era.
- **C) Tactical nukes usable, strategic not.** Battlefield weapons as a huge Alarm spike. Real
  doctrine debated this for decades; it is the most complex option.

---

## 8. War declaration, war goals, peace

Today a war is `declare_hostile` and nothing else: no goal, no end, and `province_holder` moves on
a decisive battle while the territorial map does not follow.

**What v0.3.0 needs:**

- **Declaration with a cost.** Declaring raises Alarm and drops sentiment and reputation. Since
  1945, formal declarations are rare; a **justification** (a grudge, a border dispute, an embargo)
  lowers the cost. No justification → full price.
- **War goals.** Named provinces the declarer wants. Holding them is what war score counts.
- **War score and exhaustion.** Score from held goals and battles; exhaustion from losses and
  mobilisation. Exhaustion feeds **unrest** (the Order tree) and **morale**.
- **Peace.** A white peace, or the loser cedes held goal provinces. Ceding is when `tile_to_nation`
  finally follows `province_holder`.

All of this is Alarm-raising, which is the point: every war moves the world toward the era's
catastrophe.

---

## 9. Cost anchors

Order-of-magnitude real costs, 2020s USD, to set **ratios** — not prices. Sources vary widely.

| Item | ~Cost | | Item | ~Cost |
|---|---|---|---|---|
| FPV drone | $0.5k | | Strike drone (medium-altitude) | $5–30M |
| 155 mm shell | $3–8k | | Fighter (current) | $70–100M |
| One-way attack drone | $20–50k | | Strategic bomber | $700M |
| Anti-tank missile | $20–200k | | Short-range interceptor | $50–100k |
| Cruise missile | $2M | | Long-range interceptor | $4M |
| IFV | $3–5M | | Destroyer | $2B |
| Self-propelled gun | $5–10M | | Attack submarine | $3.5–4.5B |
| Main battle tank | $5–15M | | Carrier | $13B |
| Professional soldier, per year | ~$100k | | Recon satellite | $0.1–2B |

**Ratios that matter for balance:** a tank ≈ 100 professional soldier-years; a destroyer ≈ 200
tanks; a carrier ≈ 6 destroyers; an interceptor ≈ 2–200 of what it shoots down. The existing value
anchor (BL-543, value anchor: annual equipment = 2× salary) should be re-measured against these.

---

## 10. What in the brief is unrealistic

1. **Light tanks with high hardness.** Light tanks are lightly armoured — low hardness, high
   speed. Their role was scouting and airborne support.
2. **Medium vs heavy tanks after 1960.** The main battle tank absorbed both by the mid-1960s;
   heavy tanks were retired. One MBT line with generations is the honest model.
3. **Trucks as a combat unit.** Trucks are motorisation and logistics. Io's Logistic Points
   already carry supply; trucks belong as the equipment that makes motorised infantry fast.
4. **Hardness on aircraft.** Every aircraft is soft. What matters is what it can hit and how well
   it evades air defence.
5. **Missiles with health and morale.** Missiles are expendable munitions. Morale is meaningless
   for them; most drones likewise.
6. **Air superiority as plane-vs-plane only.** From 1965 on, ground-based air defence decided most
   air wars. Without it, air dominates everything.
7. **One naval attack, no detection.** A submarine's whole value is not being found. Without
   detection, a submarine is a weak destroyer.
8. **Strategic bombing of production by bombers.** Against a peer with air defence, bombers rarely
   go since the 1970s. Missiles and drones do this job now.
9. **Missile defence as a shield.** It thins short-range attacks well and long-range ones poorly.
   No defence makes a nation safe from intercontinental missiles.
10. **Nuclear missiles as a unit you use.** See § 7. Real nuclear weapons shaped wars by not being
    used.

**Realistic and worth keeping:** retreat at zero readiness; morale plus supply in combat; soft/hard
split; satellites as intelligence; bunkers; convoy raiding; fronts.

---

## 11. More detail, less detail

**Worth more detail** — each feeds Trade or Conflict directly:

- **Manpower from the labour pool** — the war empties the factories.
- **Equipment as goods with generations** — tech shows in the next fight.
- **Ground air defence** — without it air is overpowered.
- **War goals and peace** — without them a war cannot end.
- **Interceptor cost exchange** — defence as an economic drain.
- **Amphibious transport** — otherwise sea power cannot change the map.

**Worth less detail** — each adds a bar or a screen without a decision:

- **Morale per unit** — make it per nation (or per front).
- **A free unit designer** — class × equipment generation is enough.
- **Piercing vs armour** — hardness covers it.
- **Missiles and drones as units** — make them munitions.
- **Naval detection simulation** — one detection number and a target mask.
- **Orbital combat** — satellites are effects, not fighters.

---

## 12. Suggested cut toward v0.3.0

The brief is three to four sprints. One sequencing that keeps each sprint shippable:

| Sprint | Theme | Delivers |
|---|---|---|
| 58 | **Land core** | New stats, soft/hard resolver, HP/readiness/retreat, classes, equipment goods, rendering, bunkers. |
| 59 | **Fronts and war** | Fronts, hold/push, declaration cost, war goals, peace, manpower, AI front scoring. |
| 60 | **Air and sea** | Airbases, wings, missions, superiority, air defence; sea zones, ships, convoy raiding, transport. |
| 61 | **Strategic and balance** | Missiles and interceptors, satellites' military yield, nuclear posture, cost balancing, the done-definition. |

---

## Sources

- [All Combat Stats Explained in Hearts of Iron IV — Games Lantern](https://gameslantern.com/article/all-combat-stats-explained-in-hearts-of-iron-iv)
- [HOI4 hardness discussion — Steam](https://steamcommunity.com/app/394360/discussions/0/144512753470067654)
- [Victoria 3 Dev Diary 64, post-release plans — Paradox](https://www.paradoxinteractive.com/games/victoria-3/news/victoria-3-dev-diary-64-Post-Release-Plans)
- [Victoria 3 post-launch roadmap — PCGamesN](https://www.pcgamesn.com/victoria-3/post-launch-roadmap)
- [Shell consumption, Aug 2024 — Interfax-Ukraine](https://interfax.com.ua/news/general/1008324-amp.html)
- [How Russia's and Ukraine's militaries stack up — Al Jazeera, 2024](https://www.aljazeera.com/news/2024/2/21/analysis-how-russia-ukraines-militaries-stack-up-after-two-years-of-war)
- [Artillery in the Drone Age — AOFS, 2026](https://www.aofs.org/wp-content/uploads/2026/02/260220-Artillery-in-the-Drone-Age_CU.pdf)
- [Artillery logistics lessons — army.ca](https://army.ca/forums/threads/artillery-logistics-lessons.137556/latest)
- [Ukraine lessons: drones and war preparedness — Geopolemic](https://geopolemic.substack.com/p/ukraine-lessons-learned-drones-and)
- Cost figures in § 9 are approximate public figures from general knowledge, not checked
  item by item in this session.
