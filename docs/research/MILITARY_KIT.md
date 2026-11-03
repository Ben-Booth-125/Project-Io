# Military kit — proposal

> **Proposes:** where each kit item sits in the Digitisation tree, and the grammar calls that raises (§ 8) · the kit slots each land class has · the kit that can fill each slot, from 1960 to
> the near future, and what each item does to the class's numbers · the goods kit is made of · how
> kit is researched, and with which points · how a design is made, kept and retired · marking a unit
> type obsolete.
> **Not here:** the force model and the classes (military/MILITARY, the authority) · the Military
> ledger's layout (ui/ledgers/military) · the age tree's grammar (economy/RESEARCH) · resource rows
> (economy/RESOURCES).
> **Confused with:** military/MILITARY.md, economy/RESEARCH.md, research/MILITARY_1960_PRESENT.md.

> **Research scaffolding — not authority.** Written for Ben's ruling (2026-10-27: *"Design the
> military research kit: which weapons, munitions and armour each class's slots can take, and the
> research nodes that open them"*). What he rules moves into `military/MILITARY.md` § Land — the
> classes, the age tree's store, and `economy/RESOURCES.md`. Names are mechanism names only, never an
> Earth proper noun.

The rule this serves (MILITARY.md, Ben 2026-10-27): **a unit is raised to a design** — a land class
with researched kit in each slot. Kit is researched before it enters a design; a unit is designed
before it is raised.

---

## 1. Slots — two per class

Two slots keeps a design legible at a glance: **the main kit** (what the class is) and **the
support kit** (what it carries alongside). A third slot would double the design space for little
decision.

| Class | Main slot | Support slot |
|---|---|---|
| Infantry | Small arms | Section weapon |
| Motorised infantry | Small arms | Transport |
| Mechanised infantry | Fighting vehicle | Section weapon |
| Armour | Main gun | Protection |
| Light armour | Scout vehicle | Sensors |
| Artillery | Guns | Alongside |
| Air defence | Launcher | Fire control |

**The swaps Ben named live in the support slot.** Infantry's *section weapon* takes an anti-tank
launcher, a mortar or a man-portable air-defence launcher — not more than one. Artillery's
*alongside* slot takes counter-battery radar, anti-armour submunitions or loitering drones.

## 2. The kit

Every class starts on its **base kit** (gen 0) with no research — the 1960 standard. Each later item
is researched. Effects are per man, added to the class's numbers (`land_combat_stats`), before the
row's quality scale. Numbers are a first cut, to be tuned against `campaign_battle_harness`.

### Infantry — small arms (main)
| Gen | Item | Effect |
|---|---|---|
| 0 | Battle rifle | — |
| 1 | Assault rifle | soft +10 |
| 2 | Optics and night sights | soft +10, defence +10 |
| 3 | Networked soldier kit | soft +10, breakthrough +10 |

### Infantry / mechanised — section weapon (support, choose one)
| Gen | Item | Effect |
|---|---|---|
| 0 | Recoilless rifle | hard +5 |
| 1 | Wire-guided anti-tank missile | hard +25 |
| 2 | Fire-and-forget anti-tank missile | hard +40 |
| 3 | Top-attack anti-tank missile | hard +55 |
| 1 | Light mortar *(alternative)* | soft +20 |
| 2 | Man-portable air defence *(alternative)* | — on the ground; engages aircraft over the province (Air, later) |
| 3 | Small attack drones *(alternative)* | soft +15, hard +20; draws drones each tick it fights |

### Motorised — transport (support)
| Gen | Item | Effect |
|---|---|---|
| 0 | Trucks | march 1.5 |
| 1 | Protected trucks | hardness +15%, defence +10 |
| 2 | Wheeled carriers | hardness +25%, breakthrough +15 |

### Mechanised — fighting vehicle (main)
| Gen | Item | Effect |
|---|---|---|
| 0 | Armoured personnel carrier | — |
| 1 | Infantry fighting vehicle (autocannon) | soft +20, hard +15 |
| 2 | Fighting vehicle with missiles | hard +30 |
| 3 | Heavy fighting vehicle, active protection | hardness +15%, defence +20 |

### Armour — main gun (main) and protection (support)
| Gen | Main gun | Effect | Protection | Effect |
|---|---|---|---|---|
| 0 | Rifled gun | — | Rolled steel | — |
| 1 | Smoothbore gun | hard +30 | Composite armour | defence +15, breakthrough +15 |
| 2 | Improved penetrators, thermal sights | hard +25, soft +15 | Reactive armour | defence +15 |
| 3 | Gun-launched missiles, networked fire control | hard +25, soft +15 | Active protection | defence +20, breakthrough +20 |

### Light armour — scout vehicle (main) and sensors (support)
| Gen | Scout vehicle | Effect | Sensors | Effect |
|---|---|---|---|---|
| 0 | Scout car | — | Optics | — |
| 1 | Tracked scout | breakthrough +15 | Thermal imaging | soft +15 |
| 2 | Scout with missiles | hard +30 | Ground radar | the province's enemy hardness is read exactly (no effect until fog) |
| 3 | — | — | Reconnaissance drones | soft +10, and its side's artillery +10% soft |

### Artillery — guns (main) and alongside (support, choose one)
| Gen | Guns | Effect | Alongside | Effect |
|---|---|---|---|---|
| 0 | Towed howitzer | — | — | — |
| 1 | Self-propelled howitzer | hardness +30%, breakthrough +15 | Counter-battery radar | enemy artillery −20% soft |
| 2 | Multiple rocket launcher | soft +60 | Anti-armour submunitions | hard +40 |
| 3 | Guided long-range rockets | soft +40, hard +20 | Loitering drones | hard +40, soft +20; draws drones |

### Air defence — launcher (main) and fire control (support)
| Gen | Launcher | Effect | Fire control | Effect |
|---|---|---|---|---|
| 0 | Radar-directed guns | — | Optical | — |
| 1 | Short-range missiles | (Air) | Search radar | (Air) |
| 2 | Medium-range missiles | (Air) | Integrated network | (Air) |
| 3 | Layered defence with counter-drone | (Air) | Electronic warfare | enemy drone kit −50% in its province |

Air defence's kit is mostly for the air theatre (BL-1449 (air combat core)); its numbers
there are that item's work. Its ground effects stay small.

**Munitions are kit, not a separate ladder.** Guided shells, top-attack missiles and submunitions
are items above, because a precision round changes what a unit can do, and the slot is where that
shows. The **ordnance** good stays the per-tick draw every unit makes.

## 3. Goods — six, by kind not by generation

A kit item names the goods a unit buys when it is raised or refits (MILITARY.md § Land — equipment,
refit and manpower). One good per KIND, not per generation: later items need **more** of a good and
add **electronics**, so the economy feels the generations without a good for every rung.

| Good | Recipe (proposed) | Kit that draws it |
|---|---|---|
| Small arms | steel + machinery | rifles, section weapons |
| Armoured vehicles | steel + machinery + alloys | fighting vehicles, tanks, scouts, protected trucks |
| Artillery pieces | steel + machinery | guns, launchers |
| Guided weapons | electronics + ordnance | anti-tank and air-defence missiles, guided rockets, submunitions |
| Drones | electronics + alloys | every drone item; drawn per tick while fighting |
| Electronics *(existing)* | — | sights, radar, fire control, networked kit — gen 2 and up |

Six goods means a `resource_count` change and a save-format move (`RESOURCES.md`); the proposal
keeps it to six for that reason.

## 4. Research — a kit ladder beside the age tree

**The age tree cannot hold the ladder.** Its grammar is fixed — three trees, three branches, about
fifteen nodes a branch (`economy/RESEARCH.md`) — and the kit above is **about forty items**. Folding
them in would turn the Force branch into the whole tree.

**Proposed: a kit ladder, researched with MILITARY research points.** The national budget already
has a `military_research` line (`politics/NATIONS.md` § A budget) that today pays only garrison
upkeep. Its spend becomes **military research points**, and the Design tab (or a third pane of it)
holds the ladder: per slot, gen 1 → 2 → 3, each rung a cost in military points. Civil research (the
age tree) and military research then compete for the same budget, which is the guns-or-butter
choice the game is about.

**The age tree still gates the big leaps**, through the Force branch nodes it already has:

| Age-tree node | Opens on the kit ladder |
|---|---|
| Mechanised Divisions | Mechanised and armour gen 1 (and the mechanised class itself) |
| Early Warning Radar | Search radar, counter-battery radar |
| Precision Munitions | Every gen 2 guided item: fire-and-forget missiles, guided rockets, submunitions |
| Airmobile Forces | Attack helicopters (the air theatre, later) |
| Combined Arms (mastery) | Networked soldier kit, networked fire control |
| a civil node, Packet Networks | Integrated air defence network |
| a civil node, Communication Satellites | Reconnaissance drones (the satellite layer, later) |

So a nation that never walks the Force branch can still buy rifles and better trucks, but not
precision weapons — and a nation that does still has to pay military points to field them.

**Cost.** Gen 1 a rung costs about one age-tree major; gen 2 twice that; gen 3 four times. Kit that
needs an age-tree node is unavailable, not merely dear, until the node is held.

## 5. Designs — one per class to start, named, retirable

- **One live design per class**, editable: choose an item per slot from what is researched. Saving
  it makes it what Recruit raises for that class. A **named design** list (keep the old one when you
  make a new one) is a later step, not needed to start.
- **Units refit** to their class's live design as its goods arrive, a share per tick (BL-1423).
  Until refitted they fight on their old kit.
- **A design's price** is its class's hire price plus the goods of its kit, shown on Recruit.

## 6. Obsolete unit types (Ben, 2026-10-27)

*"We should be able to mark unit types as obsolete. The recruit options give plenty of completely
outdated unit types."*

- **A nation marks a roster row obsolete** on the Design tab (one toggle per row). An obsolete row
  is hidden from Recruit; a "show obsolete" switch brings it back. Units already raised are untouched.
- **Default: every row below the modern band starts obsolete** for every nation, so Recruit opens
  on the seven modern types. The ancient-to-industrial rows exist for the history sim and for a
  nation whose ground cannot field better; a nation that wants them un-marks them.
- **Rival nations' scorers skip obsolete rows** as the player's Recruit does, so the AI stops raising
  Levy Spear in 1960.
- Once designs exist, **a design replaces its class's row on Recruit**, and marking obsolete moves to
  designs: an old design can be kept for the record and retired.

## Ruled (Ben, 2026-10-27)

- **Kit is folded into the age tree** — not the kit ladder of § 4, which this ruling sets aside. How
  the forty items fit the tree's grammar is the next pass (BL-1436, kit in the age tree).
- **Two slots a class** (§ 1), **six goods by kind** (§ 3), **one live design per class** (§ 5).
- **Obsolete**: every row below the modern band starts obsolete (§ 6); built now.

## 7. The calls as they were put

1. **Where kit is researched**: a kit ladder on military research points (proposed), or folded into
   the age tree?
2. **Slots**: two per class (proposed), or three (for example armour gains a *fire control* slot)?
3. **Goods**: six goods by kind (proposed), or one good per generation?
4. **Obsolete default**: everything below the modern band starts obsolete (proposed), or everything
   starts live and the player marks?
5. **Designs**: one live design per class to start (proposed), or named designs from the start?

## 8. Placement in the Digitisation tree — proposal (design pass, 2026-11-03)

Written for the ruling that kit is folded into the age tree (above). **Ruled (Ben, 2026-11-03):**
A on Q1, Q2, Q4, Q5, Q6 and Q7; **C on Q3** (no kit spur: kit clusters lengthen the existing arms,
so § 8.1's spurs and § 8.2's "Requires" column are superseded by the layout in
`../economy/tech/DIGITISATION_TREE.md` § Kit); **B on Q8** (the age runs slower). The node set and
the items each opens are as § 8.2 placed them.

**The list placed is the authored catalogue, not § 2.** § 2's two-slot table was overturned
(Ben, 2026-10-28: each class has its own slots, `military/MILITARY.md` § Land — the classes). The
catalogue in `src/world/unit_designs.cpp` holds **53 land and 36 air later items**, each naming a
`DG-KIT-*` node the tree does not hold. Many are one technology fitted to several classes: the
assault rifle is three items, a jet airframe four. So the unit placed is a **kit node**: one
technology, opening every catalogue item it names, in every class it fits. **39 kit nodes** cover
all 89 items: 33 new and 6 existing nodes that gain a kit unlock.

### 8.1 Shape: kit spurs

A **kit spur** is a cluster (two minors feeding a major) hung off a node of its branch, outside
the two-arm fan. Its minors open gen-1/2 kit; its major opens the surge item. A spur never feeds
the mastery and carries no quest, so the branch's spine is untouched. Its minors sit on its
parent's ring or later, and its major one ring past them (ring 4 at most).

Gen and ring line up roughly: gen 1 on rings 1–2, gen 2 on rings 2–3, gen 3 on ring 4.

### 8.2 Placement

Kit good letters: **SA** small arms, **AV** armoured vehicles, **AP** artillery pieces, **GW**
guided weapons, **DR** drones, **E** electronics. **AF** marks an airframe, which has no kit good
(§ 8.5, Q6). *Reused* rows are existing nodes that keep their effect and gain the unlock.

#### Force (State) — small arms, section weapons, guns, protection, precision munitions

| Node (id) | Kind, ring | Requires | Opens (class.slot item) | Gen | Good |
|---|---|---|---|---|---|
| Armoured Carriers (`DG-ST-FO-A1`) *reused* | minor r1 | Travel II | mot.vehicle protected trucks · ad.vehicle truck-mounted · lt.armour spaced armour | 1 | AV |
| Early Warning Radar (`DG-ST-FO-A2`) *reused* | minor r1 | Travel II | ad.fire_control search radar | 1 | E |
| Mechanised Divisions (`DG-ST-FO-A`) *reused* | major r2 | A1 + A2 | mech.vehicle IFV (autocannon) · lt.armament autocannon | 1 | AV, AP |
| Precision Munitions (`DG-ST-FO-B2`) *reused* | minor r3 | Travel IV | art.munitions guided shells · mr.armament laser-guided bombs · bmr.armament guided bombs · drn.armament guided missiles | 2 | GW |
| Airmobile Forces (`DG-ST-FO-B`) *reused* | major r4 | B1 + B2 | hel.airframe attack helicopter | 1 | AF |
| Assault Rifles (`DG-ST-FO-D1`) | minor r1 | Travel II | inf/mot/mech.weapon assault rifle | 1 | SA |
| Body Armour (`DG-ST-FO-D2`) | minor r1 | Travel II | inf/mot/mech.armour flak vest | 1 | SA |
| **Anti-Tank Missiles** (`DG-ST-FO-D`) | major r2 | D1 + D2 | inf/mot.weapon wire-guided ATGM · hel.armament wire-guided ATGM | 1 | GW |
| Light Mortars (`DG-ST-FO-E1`) | minor r2 | D | inf.weapon light mortar | 1 | SA, AP |
| Short-Range Missiles (`DG-ST-FO-E2`) | minor r2 | D | ad.launcher short-range · ftr.armament radar-guided · rcn.armament self-defence | 1 | GW |
| **Fire-and-Forget Missiles** (`DG-ST-FO-E`) | major r3 | E1 + E2 | inf/mech.weapon F&F ATGM · mech.vehicle IFV with missiles · lt.armament ATGM mount · hel.armament F&F ATGM | 2 | GW, E |
| Medium-Range Missiles (`DG-ST-FO-F1`) | minor r3 | E | ad.launcher medium-range · ftr.armament beyond-visual-range | 2 | GW, E |
| Submunitions (`DG-ST-FO-F2`) | minor r3 | E | art.munitions anti-armour submunitions | 2 | GW, E |
| **Long-Range Precision Strike** (`DG-ST-FO-F`) | major r4 | F1 + F2 | art.gun guided rockets · mr.armament stand-off · bmr.armament cruise missiles | 3 | AP, GW, E |
| Modern Gun Barrels (`DG-ST-FO-G1`) | minor r2 | Mechanised Divisions | arm.armament smoothbore · art.gun long-barrel howitzer | 1 | AP |
| Tracked Chassis (`DG-ST-FO-G2`) | minor r2 | Mechanised Divisions | art.tow self-propelled · ad.vehicle tracked carrier · lt.wheels tracked scout · arm.tracks hydropneumatic | 1 | AV |
| **Rocket Artillery** (`DG-ST-FO-G`) | major r3 | G1 + G2 | art.gun multiple rocket launcher | 2 | AP, E |
| Wheeled Carriers (`DG-ST-FO-H1`) | minor r3 | G | mot.vehicle wheeled carriers · lt.wheels run-flat six-wheeler | 2 | AV, E |
| Armoured Self-Propelled Guns (`DG-ST-FO-H2`) | minor r3 | G | art.tow armoured SP chassis | 2 | AV, E |
| **Active Protection** (`DG-ST-FO-H`) | major r4 | H1 + H2 | arm.armour active protection · mech.vehicle heavy IFV, active protection | 3 | AV, E |

#### Processing (Production) — armour materials and airframes

| Node (id) | Kind, ring | Requires | Opens | Gen | Good |
|---|---|---|---|---|---|
| Numerical Control (`DG-PR-PC-C1`) *reused* | minor r3 | Travel VI | arm.tracks lightweight composite tracks | 2 | AV, E |
| Composite Armour (`DG-PR-PC-D1`) | minor r1 | Travel II | arm.armour composite · lt.armour composite applique | 1 | AV |
| Ceramic Plates (`DG-PR-PC-D2`) | minor r1 | Travel II | inf/mot/mech.armour ceramic plate carrier | 2 | SA, E |
| **Reactive Armour** (`DG-PR-PC-D`) | major r2 | D1 + D2 | arm.armour reactive | 2 | AV, E |
| Jet Airframes (`DG-PR-PC-E1`) | minor r2 | Bulk Petrochemicals | ftr twin-engine · mr multirole · bmr supersonic · rcn high-altitude | 1 | AF |
| Armoured Helicopters (`DG-PR-PC-E2`) | minor r2 | Bulk Petrochemicals | hel.airframe armoured tandem-seat | 2 | AF |
| **Low-Observable Airframes** (`DG-PR-PC-E`) | major r3 | E1 + E2 | ftr low-observable · mr low-observable strike · bmr low-observable | 2 | AF, E |

#### Signal & Orbit (Network) — sensors, fire control, drones, networked kit

| Node (id) | Kind, ring | Requires | Opens | Gen | Good |
|---|---|---|---|---|---|
| Airborne Radar (`DG-NE-SO-D1`) | minor r1 | Travel II | ftr pulse-doppler · bmr terrain-following · rcn side-looking radar | 1 | E |
| Long-Endurance Drones (`DG-NE-SO-D2`) | minor r1 | Travel II | drn.airframe medium-altitude long-endurance · drn.avionics satellite datalink | 1 | DR, E |
| **Thermal Imaging** (`DG-NE-SO-D`) | major r2 | D1 + D2 | arm.armament penetrators, thermal sights · lt.armament thermal autocannon · hel.avionics night vision · mr.avionics targeting pod | 2 | AP, E |
| Scanned-Array Radar (`DG-NE-SO-E1`) | minor r2 | Mainframe Computing | ftr scanned-array · mr sensor fusion · hel mast-mounted radar | 2 | E |
| Jet Combat Drones (`DG-NE-SO-E2`) | minor r2 | Mainframe Computing | drn.airframe jet-powered combat drone | 2 | DR, E |
| **Integrated Air Defence** (`DG-NE-SO-E`) | major r3 | E1 + E2 | ad.fire_control integrated network · rcn.airframe early warning | 2 | E |
| FPV Drones (`DG-NE-SO-F1`) | minor r3 | Thermal Imaging | mot/mech.weapon FPV attack drones | 3 | SA, DR, E |
| Counter-Drone Defence (`DG-NE-SO-F2`) | minor r3 | Thermal Imaging | ad.launcher layered defence with counter-drone | 3 | GW, DR, E |
| **Loitering Munitions** (`DG-NE-SO-F`) | major r4 | F1 + F2 | art.munitions loitering drones · drn.armament loitering munitions | 3 | DR, E |
| Autonomous Targeting (`DG-NE-SO-G1`) | minor r3 | Integrated Air Defence | drn.avionics autonomous targeting | 3 | E |
| Electronic Warfare (`DG-NE-SO-G2`) | minor r3 | Integrated Air Defence | ad.fire_control EW · bmr countermeasures suite · rcn towed decoys · rcn signals-intelligence suite | 3 | E |
| **Networked Warfare** (`DG-NE-SO-G`) | major r4 | G1 + G2 | inf.armour networked soldier kit · arm.armament gun-launched missiles, networked fire control | 3 | SA, GW, E |

FPV drones draw the **drones** good (Ben, 2026-11-03), not a good of their own.

**An item may open before the earlier rung of its slot.** For example, mast radar (helicopter
avionics 2) is on a ring-2 minor, while night vision (avionics 1) is on a ring-2 major. A slot
offers whatever is researched (`military/MILITARY.md` § The lock rule), so no rule breaks. The
slot's order is just not a research order.

### 8.3 Counts

| Branch | Nodes before → after | Majors before → after | New spurs | Reused nodes |
|---|---|---|---|---|
| Force | 17 → **32** | 3 → 8 | 5 (D–H) | 5 |
| Processing | 17 → **23** | 3 → 5 | 2 (D–E) | 1 |
| Signal & Orbit | 17 → **29** | 3 → 7 | 4 (D–G) | 0 |
| Whole tree | 160 → **193** | 30 → 41 | 11 | 6 |

**No major loses its two-minors rule.** Every new major has two minors, and the reused majors
keep theirs. No pair is proposed, the masteries' requirements do not change, and the quests are
untouched. The whole-age cost rises from 1087 to about **1383 cost units**, roughly 27% more.

### 8.4 What the store edit then needs

- An effect kind, `unlock_kit`, listing catalogue item ids, so each node shows its kit (§ 8.5, Q7).
- The catalogue's `research_node` values re-pointed from `DG-KIT-*` to the ids above. Its validator
  must also allow two items to share a node (§ 8.5, Q1).
- The rendered tables, regenerated with `tech_tree_store.js` and `gen_tech_tree_table.js`.

### 8.5 Where the grammar widens — the calls

**Q1. What one node opens.**
- A) One node per technology, opening its items in every class (**proposed**: 39 nodes). The
  catalogue's "two items share a research node" check must be relaxed.
- B) One node per catalogue item: 89 nodes, with no code change. That is triple the branches' size.
- C) One node per subject and generation: about 20 nodes. Each major opens a whole rung, so the
  tree loses kit choice.

*Recommend A.* It matches "about forty" and keeps an item's meaning the same across classes.

**Q2. Branch size.** The kit branches reach 32, 29 and 23 nodes against the grammar's seventeen.
- A) Accept wider kit branches. The spine stays seventeen and the spurs add to it (**proposed**).
- B) Spread kit to more branches to keep each nearer 23. Vehicles and trucks could go to Freight,
  and body armour to Order.
- C) Defer gen 3 to the next age's tree. That cuts 7 nodes, but the 1960 tree would then reach
  only near-2000 kit.
- D) Merge further, for example missiles into one ladder. That cuts about 6 nodes but blurs the
  slot choices.

*Recommend A.* Ben placed kit by subject, and B splits a subject across branches for a count.

**Q3. The kit spur — a new shape.** The grammar's branch is a travel chain, a cluster, two arms,
two clusters and a mastery.
- A) Name the **kit spur** in `RESEARCH.md`. It is a cluster hung off one node of its branch,
  outside the fan, feeding no mastery and no quest (**proposed**).
- B) Give kit a third arm with its own travel nodes. That adds about 4 nodes a branch, and the
  mastery then needs three majors.
- C) Lengthen the existing arms, with kit clusters between an arm's cluster and the rim. The
  rim majors then sit later.

*Recommend A.* It is the smallest grammar change, and it leaves the spine's pacing as it is.

**Q4. Kit on minors.** The grammar says a minor is "a narrower buff or a small utility". It is a
major that unlocks a unit.
- A) Widen the minor: it may open kit items, and the surge item sits on the major (**proposed**).
- B) Put all kit on majors, with buff minors around them. That makes about 3× the nodes.

*Recommend A.* A gen-1 rifle is a narrow, small thing.

**Q5. Cross-branch requirements.** Should gen-2/3 electronic kit also need, say, Semiconductor
Foundry?
- A) No. Each spur hangs off its own branch only, and the tie is economic: later kit draws
  electronics off the shelf (**proposed**).
- B) Yes. Gen-3 majors take a second requirement group from another branch. This is a new shape,
  since today only joints and masteries span two branches.

*Recommend A.* The goods already carry that dependency legibly.

**Q6. Air kit's goods (Ben, 2026-11-03: air kit moves onto the kit goods).** Armament fits
guided weapons, drone airframes and loitering munitions fit drones, and avionics fit electronics.
Airframes fit no kind.
- A) Add a sixth kit good, **airframes**, made from alloys, machinery and electronics. This is a
  `resource_count` and save-format change.
- B) Airframes draw armoured vehicles. Nothing new is needed, but a fighter then reads as an
  armoured vehicle.
- C) Airframes keep their alloys and machinery, and only armament and avionics move to kit goods.

*Recommend A.* It is legible, and the five kit goods just set the pattern.

**Q7. Where the kit link lives.**
- A) In both places: the store carries `unlock_kit` for display, the catalogue's `research_node`
  stays the read path, and `--check` cross-checks them (**proposed**).
- B) Only in the catalogue, so the tree view cannot show a node's kit.
- C) Only in the store, so the catalogue looks its nodes up.

*Recommend A.*

**Q8. Price.** The age's cost rises about 27%. Pacing is the tuned property, and the node count is
never changed to meet it (`economy/RESEARCH.md` § Cost).
- A) Re-tune the RP unit so the richest nation still finishes inside the age (**proposed**).
- B) Leave the unit as it is, so the age reads slower.

*Recommend A*, measured in the sweep.
