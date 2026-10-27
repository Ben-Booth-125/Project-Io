# Military kit — proposal

> **Proposes:** the kit slots each land class has · the kit that can fill each slot, from 1960 to
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

Air defence's kit is mostly for the air theatre (BL-1429), which does not exist yet; its numbers
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
