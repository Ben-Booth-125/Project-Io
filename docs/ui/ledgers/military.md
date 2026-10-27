# Military — design Q&A

> **Working design doc** for the ledger-mockup pass. Strawman answers — Ben revises.
> Menu slot: **proposed** (open question 1) · Source: none yet (hire lives on the military base's
> tile card, `selection_panel.cpp` § Hire) · Mock table(s): none yet.
> Host: shell fold-out column, ~380px @1720 (derived — `shell_column_width(disp.x)`, 380–460 by resolution).

Asked for by Ben (2026-10-27): *"We can also design the military ledger on the side menu, this will
need to have two tabs, one for recruiting and another for design."* The force model it serves is
`../../military/MILITARY.md` § The campaign force model.

## 1. Top question — the one thing this answers at first glance

**"What force can I raise, where, at what cost — and what is it raised with?"**

It earns a rail slot under `../MENU.md` § Menus are broad ledgers because both halves are broad:
every base the nation holds, and every unit type it may field. That reverses MENU.md's standing
line that *"Conflict has no broad ledger"* — the line was true while hiring was one press on one
base's tile card, and stops being true once force is a system with types, costs and kit.

**What it is NOT.** It does not answer "where are my units and how are they doing" — the canvas
counters and the unit and stack cards answer that, and a list of every unit would be a second
surface for the same question. Fronts (BL-1427) will want their own read when they exist; whether
that is a third tab here or the canvas is open question 4.

## 2. Sub-levels — views & default

Two views behind a `nav_button` strip, inside `foldout_begin("Military")`.

| View | Answers (one question) | Content |
|---|---|---|
| **Recruit** *(default)* | "What can I raise, where, and what will it cost?" | A **base picker** — the nation's completed military bases, each with its province and the units standing there. Below it, the **roster**: one row per unit type the nation may field (`available_rows`), each carrying its land class, its numbers (soft, hard, hardness, defence), its hire price in credits and the kit it buys off the shelf, and a **Raise** press that hires at the chosen base. A type the nation cannot field yet is listed dimmed with **why** (the missing good, or the technology it waits on), so the roster shows the ladder, not only the rungs in reach. |
| **Design** | "What is each kind of unit raised with?" | **Strawman — open question 2.** One row per land class: which **equipment generation** that class musters with, what that kit costs per unit, and what it gives (the class's numbers at that generation). Choosing a generation is a standing order for every unit raised after it; units in the field refit to it as kit arrives (BL-1423). Generations open by technology (`DG-ST-FO`, the Force branch). |

**Why Recruit is the default.** It is the act the ledger exists for; Design is set once and
revisited when a technology lands.

### The empty states

- **No military base** → Recruit says **"No military base"** and offers the Construction ledger's
  site finder pre-set to the military base, because the base is the door to every hire
  (`../../military/MILITARY.md` § The muster interface). Nothing on the roster is pressable.
- **A base, nothing affordable** → the roster still lists every type; each Raise is disabled with its
  reason (`rejected_funds`, a missing good), the same idiom as the tile card's disabled build rows.

## 3. Lens on open

**None by default.** A strawman worth weighing: with Recruit open and a base chosen, the canvas
highlights **that base's tile** and the reach field it can supply (`../../economy/LOGISTICS.md`
§ The reach field) — where a unit raised here can stand and stay supplied. It is a mode of the
recruit flow, as the placement lens is of the build flow, not an `overlay_mode`.

## 4. Data sources

| Read | Source |
|---|---|
| Bases | the nation's `military_base` buildings, completed (`ticks_remaining <= 0`), not decommissioned |
| Roster rows | `available_rows(world, nation, campaign_roster_band_for(era))`, plus the full table for dimmed rows |
| A row's numbers | `unit_land_stats` / `land_class_stats` (`src/world/unit_roster.hpp`) |
| Hire price | `hire_base_cost + hire_cost_per_power x power_mod` (`scripts/economy.lua` § military), plus the shelf draw per gated axis |
| Raise | the existing `hire_unit` verb — the ledger is a door onto it, never a second hire path |
| Design rows | **not backed yet**: equipment generations are goods that do not exist (BL-1422, land classes and kit) |

## 5. Toggle / close semantics

Standard: the rail icon toggles the ledger; re-clicking the active tab closes it; switching tabs is
a view change. The base picker is a target selector (exempt from the toggle rule, like the body
and market combos). **Raise is a press, not a toggle** — it has no active state.

## Open questions for Ben

1. **Which rail slot?** Slots 7 (Corp. Strategy) and 8 (Diplomacy) are reserved and disabled.
   Strawman: **Military takes slot 7**, after Convoys — force follows supply in the curated order —
   and Corp. Strategy, whose subject retired with the corporations, leaves the rail.
2. **What does "design" mean?** Three readings:
   - **A — kit per class (strawman).** Each land class chooses its equipment generation; the class
     fixes the role. Small, legible, and what MILITARY.md § Land — the classes currently says
     ("a class × equipment generation, never a free design") — but that line was my recommendation
     (research note § 11), not your ruling.
   - **B — templates.** A unit is a named mix of classes (say, two infantry, one armour, one
     artillery) that is raised and fights as one counter. Closer to a division designer; it needs the
     resolver to fight a mixed unit, which it already can (a counter is a group of combatants).
   - **C — a free designer.** Choose chassis, weapon and armour, and the stats follow. The deepest,
     and the one the research note flagged as a skill wall in comparable games.
3. **Is hiring instant?** Today a hire places a unit at once. A muster time (the Force branch's
   *Helicopter Lift* and *Proven Readiness* already promise "muster time -10% / -20%") would give
   Recruit a queue like Construction's.
4. **Where do fronts live** once BL-1427 exists — a third tab here, or the canvas only?
5. **Manpower.** Once soldiers come from the labour pool (BL-1424), Recruit should show the pool and
   the conscript/professional choice. Here, or on the Nation overview?
