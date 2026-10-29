# Military — design Q&A

> **Working design doc** for the ledger-mockup pass. Strawman answers — Ben revises.
> Menu slot: **rail slot 7** (Ben, 2026-10-27) · Source: none yet (hire lives on the military base's
> tile card, `selection_panel.cpp` § Hire) · Mock table(s): none yet.
> Host: shell fold-out column, ~380px @1720 (derived — `shell_column_width(disp.x)`, 380–460 by resolution).

Asked for by Ben (2026-10-27): *"We can also design the military ledger on the side menu, this will
need to have two tabs, one for recruiting and another for design."* The force model it serves is
`../../military/MILITARY.md` § The campaign force model.

## 1. Top question — the one thing this answers at first glance

**"What force can I raise, where, at what cost — and what is it raised with?"**

It earns a rail slot under `../MENU.md` § Menus are broad ledgers because both halves are broad:
every base the nation holds, and every unit type it may field. MENU.md used to say Conflict had
no broad ledger — true while hiring was one press on one base's tile card, and not once force is
a system with types, costs and kit.

**What it is NOT.** It does not answer "where are my units and how are they doing" — the canvas
counters and the unit and stack cards answer that, and a list of every unit would be a second
surface for the same question. Fronts (BL-1427) will want their own read when they exist; whether
that is a third tab here or the canvas is open question 4.

## 2. Sub-levels — views & default

Two views behind a `nav_button` strip, inside `foldout_begin("Military")`.

| View | Answers (one question) | Content |
|---|---|---|
| **Recruit** *(default)* | "What can I raise, where, and what will it cost?" | A **base picker** — the nation's completed military bases, each with its province and the units standing there. Below it, the **roster**: one row per unit type the nation may field (`available_rows`), each carrying its land class, its numbers (soft, hard, hardness, defence), its hire price in credits and the kit it buys off the shelf, and a **Raise** press that hires at the chosen base. A type the nation cannot field yet is listed dimmed with **why** (the missing good, or the technology it waits on), so the roster shows the ladder, not only the rungs in reach. |
| **Design** | "What is each kind of unit raised with?" | **Designs, one per land class to start** (Ben, 2026-10-27): each class's **kit slots** with the researched choice in each — infantry's rifles or anti-tank launchers, artillery's guns with drones or anti-armour alongside — what the kit costs per unit, and what the design gives (its soft, hard and hardness). A slot offers only researched kit; a class with nothing researched for a slot fields its base kit. Saving a design makes it the one Recruit raises; units in the field refit to it as kit arrives (BL-1423). Kit opens by military research. |

**Why Recruit is the default.** It is the act the ledger exists for; Design is set once and
revisited when a technology lands.

### Grouped by theatre (Ben, 2026-10-28)

**Recruit and Design group their rows by theatre** — Land, then Air (and Sea and Space as they
arrive) — each under a **collapsible header** carrying the theatre's name and its count. A header
is a toggle: pressing it folds or unfolds its rows. On Recruit the base picker lists military bases
under Land and airbases under Air, and a row raises at a base of its own theatre only. Design lists
the air classes beside the land ones; each opens the same full-canvas designer with its own slots
(airframe, armament, avionics).

**A wing is ordered from the canvas.** With a wing selected, right-clicking a province assigns the
wing there on its current mission; a province out of range is refused with its distance and the
wing's range. The wing card on the Selection band sets the mission and lists the provinces assigned.

### The empty states

- **No military base** → Recruit says **"No military base"** and offers the Construction ledger's
  site finder pre-set to the military base, because the base is the door to every hire
  (`../../military/MILITARY.md` § The muster interface). Nothing on the roster is pressable.
- **A base, nothing affordable** → the roster still lists every type; each Raise is disabled with its
  reason (`rejected_funds`, a missing good), the same idiom as the tile card's disabled build rows.

### The designer (Ben, 2026-10-28)

Clicking a **unit type** — a row of the Design tab, or the type name on a unit card — opens the
**designer on the full canvas** (`../DRILL_THROUGH.md`, the full-canvas rung). It answers "what is
this kind of unit carrying, and what can it carry instead?"

- **The unit, centred.** An authored image of the class fills the middle of the canvas, scaled to
  it with its aspect kept: one PNG per land class, `assets/units/<class>.png`, a placeholder until
  the art lands (`tools/art/make_unit_placeholders.py` draws the placeholders).
- **One box per changeable piece of equipment**, placed around the image, each joined by a leader
  line to the point on the unit that piece sits. **The boxes are the class's own slots (Ben,
  2026-10-28)**: a tank shows armament, armour and tracks; foot infantry shows weapon and armour
  (`../../military/MILITARY.md` § The kit has the table). Each class's image carries one anchor
  point per slot, in `assets/units/<class>.anchors` beside the image (one line per slot, image-
  relative), so new art moves its anchors without a code change. A box takes the side of the
  image its anchor is nearer, and hovering it lights its anchor. A box shows the slot, the kit
  fitted and its stat change.
- **Opened, a box lists the slot's kit**, base first: each item with its stat change against the
  fitted item and its goods per unit. **Locked kit is listed, not hidden**: it carries a LOCKED
  badge and names the research node that opens it, and its numbers stay readable, because what
  research would buy is half of what the screen is for. A pick the seam refuses is disabled with
  its reason. Pressing the box again closes its list.
- **The summary** under the image is the design's soft attack, hard attack, defence, breakthrough
  and hardness and its kit goods per unit; hovering an item previews them with it fitted. There is
  no speed: speed is the class's alone (Ben, 2026-10-28).
- **A pick drafts; Save commits (Ben, 2026-10-28, NR-1096).** A pick changes the open design's
  **draft** only — the boxes and summary show the draft, and a slot whose draft differs from the
  live design is marked changed. **Save** sends the draft as the class's **one live design**
  (`set_unit_design`, each changed slot); **Revert** drops the draft. Closing with an unsaved draft
  asks to save or discard. The fitted item stays disabled in its list ("Fitted"), since the slot is
  a one-of-n selector. Once saved, new hires are raised with it, and units already in the
  field take it by refit (§ Design above; `../../military/MILITARY.md` § The muster interface,
  "Refit to a newer type"). The summary counts the units in the field still on an older design;
  their unit card offers the refit.
- Closing the designer (the toggle rule — the press that opened it — its return control, or
  Escape) returns to the ledger or card it was opened from, which the takeover never covered.

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
| Design rows | the player nation's live design per class — `design_of` / `design_stats`, the kit catalogue and its lock (`src/world/unit_designs.hpp`); a pick's refusal is `unit_design_check` |

## 5. Toggle / close semantics

Standard: the rail icon toggles the ledger; re-clicking the active tab closes it; switching tabs is
a view change. The base picker is a target selector (exempt from the toggle rule, like the body
and market combos). **Raise is a press, not a toggle** — it has no active state.

## Settled (Ben, 2026-10-27)

- **Slot 7**, where the reserved Corp. Strategy stood; Corp. Strategy retires.
- **Design is kit per class, made from researched kit.** *"Design will be more useful as military
  tech is researched, i.e. better weapons, munitions and armour divisions need to have their kit
  researched and units designed before deployment. We can also swap things like rifles / RPGs, and
  artillery units can field drones / anti-armour alongside artillery."* Reading A, extended with
  swappable slots — not templates (B) and not a free designer (C).
- **Raising takes time**: a muster queue at the base, shown on Recruit like Construction's queue.
- **Fronts live on the canvas**, not in this ledger.
- **Manpower shows on Recruit**: the labour pool and the conscript or professional choice.

## The questions as they were put

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
