# Project Io — Eras

> **Settles:** what an Era is and what separates one rung of the ladder from the next · what
> moves an Era, and what merely gates access within one · what achievement opens each rung · what
> a technology costs off its own stat line · what territory, resources and buildings a rung opens.
> **Not here:** what those resources are or how they are made (RESOURCES, PRODUCTION) · the
> strain and hazard model Era 2's pressure rests on (../CLIMATE.md) · Alarm as a quantity between
> nations (../politics/RELATIONS) · what technology
> unlocks and what pays for it (RESEARCH) · the pre-campaign history
> (../generation/GENERATION_STRATEGY.md).
> **Confused with:** ../CLIMATE.md, RESEARCH.md, RESOURCES.md.

An **Era** is a named phase in the game's industrial arc, defined by the accessible territory, available buildings, and the dominant strategic challenge. Eras are a formal game system: each has a defined entry, a distinct resource profile, and a characteristic question for the player to answer. An Era changes as a **gear shift that changes what the game is about**, each Era necessitating the one before — and each is **opened by an achievement**, never by a failure avoided.

Only Era 1 is designed in any depth; Era 2 has its territory and its pressure named and little
else. Later Eras are stubbed. **The game begins in Era 1** — the ladder is 1-based.

---

## The ladder, and its numbering (Ben, 2026-08-31)

**The game starts in Era 1.** The ladder was renumbered on 2026-08-31 (NR-749) so the 1960s start is
Era 1 rather than Era 0.

| Era | Territory | Opened by | Its defining pressure |
|---|---|---|---|
| **Era 1 — Terrestrial** | Home planet and equivalent bodies | The campaign epoch | **Alarm** — how threatened other nations feel by you |
| **Era 2 — Early Space** | The whole solar system | Reaching space (§ Era 1 → Era 2 gate) | **Climate** — the strain on a body's commons (`docs/CLIMATE.md`) |
| **Era 3 — Dimensional** | Open | Open | Open |
| **Era 4 — Megastructure** | Open | Open | Open |

**This renumber is the DOCUMENT's Era axis only.** It does not touch `era_band`, does not touch
`condition_subject::era`, and does not renumber any `E0-`/`E1-` technology id — see § Three things
that say "era" in code, which already establishes those as separate axes. Nothing in `src/` moves.

### Where the ladder starts

**The working start is the 1960s** (Ben, 2026-08-31). `world_params::epoch_year` defaults to 1960.
The epoch names the calendar only — never a history, and never the band (Ben, 2026-09-24,
superseding the reading under which the epoch picked the band): generation reads no epoch, and the
world it builds is the one the Industrialisation span closes on at 1960 whatever start is asked for
(`../generation/INDUSTRIALISATION.md`).

**Every nation opens on the industrial recipe band (Ben, 2026-10-10, overturning NR-929's band
per nation).** Every nation is playable and every nation starts the campaign at the same tech level
(`RESEARCH.md` § Who holds technology), so no nation is held to an ancient roster: the Launchpad, the
petroleum and propellant chains and the spacecraft chain are open to every nation, gated only
by technology and placement. The band is fixed, not derived from the history, and reads the same on a
new game and a loaded save. **Epoch 0 is retired (Ben,
2026-09-25, NR-920):** the epoch names the calendar only. `docs/development/ROADMAP.md` § The two arcs owns which is the commercial product,
and this document owns only the Era structure, which is the same on both.

**The prehistory is a generator, not a play layer.** Generation runs a pre-epoch history sim that
produces the world the campaign opens on. It runs three spans on one engine, each
on one band at 4 years a tick: Empires, 400 BCE → 1200 CE; Exploration, to 1660; Industrialisation,
to 1960 (`src/world/era_minus_one.cpp` derives each; `world_params::prehistory_years` is the scope
knob that runs them at all). A six-band ladder
(100 → 50 → 20 → 10 → 5 → 1 years) is authored as the `history_sim_params` struct default
(`src/world/history_sim.hpp`) and read by the tile inspector and the harnesses; generation
overrides it on every world. A settle-dominated run is the intended shape, not a defect (NR-205,
ruled 2026-08-12). Authority for the prehistory run is `docs/lore/HISTORY.md`.

**The opening position has no calendar meaning** (BL-369, warm start; BL-978, warm start retired).
The world is generated *at* the epoch and handed to play **already settled** — not generated at an
earlier date and advanced to it. The settling pass is phase 6's **single validation run** of the
winning landscape — `../generation/GENERATION_STRATEGY.md` § Three passes: pass 3 *selects*, and its
one validation tick-run is the settle (BL-978, warm start retired, owns the work; BL-369 owned the
fixed `pre_game_ticks` warm start it replaces). Whatever the pass that settles it
spends in simulated quarters, the clock is **rebased at the handoff**, so play always opens at `epoch_year`. That rebase is the
load-bearing half of this paragraph and it does not depend on which pass does the settling: the
opening position owes the calendar nothing.

**The validation run is twelve quarterly econ ticks** (`k_campaign_settle_ticks`,
`src/world/campaign_settle.hpp` — the one tick and settle the app, the worker and every harness
call), run on the winner
inside the Industrialisation round's worker as generation's last act — the round hands Begin a
world already proved, and a cold Begin with no wizard makes the same call in the same order (Ben,
2026-09-24) — under spectate with nobody seated, with the persona counsel and battle dispatches
suppressed. The treasuries, shelves, filed returns and prices those ticks leave
*are* the opening position; the seat card shows the returns they file. Twelve is **measured,
not round**, and the instrument is `haulage_measure --per-tick` (the only harness that sees
trade): pooled over five seeds, the per-tick convoy dispatch count climbs from zero — tick 1
dispatches nothing, tick 2 about 40 % of the settled rate, tick 4 about 90 % — and then holds at
its 80-tick level (≈125 convoys per tick over the last eight of eighty). Twelve is the first tick
at which **both** the 4-tick and the 8-tick trailing means of that count sit within 5 % of the
80-tick level (98 % and 96 %; at eleven the 8-tick mean is still 94.8 %). A longer run buys
nothing the player can see.

**The seat's viability floor does not read this run** (Ben, 2026-09-16, NR-881; BL-1020, the seat
floor reads the static score). Twelve ticks over a field still ramping from zero file no trading
record a viability verdict could stand on, at any trailing window. The shortlist therefore gates
and ranks on the static landscape score phase 6 computed before the run, and the run's figures —
the closing treasury and the trailing net over `k_spawn_trailing_quarters` = 8 filed returns — reach
the seat card as information only (`../generation/CORPORATION_GENERATION.md` § The spawn
shortlist, and the seat).

One series does not settle in twelve, and it is named rather than hidden: the **intra-body
market-to-market** share of those convoys drifts upward slowly, from ≈80 % of its 80-tick level
at tick 12 to within 5 % only around tick 40, inside ±15 % quarter-to-quarter noise. That is a
slow composition drift in the trade mix, not the cold-start transient the settle exists to
absorb; buying it would cost half the eighty-tick warm start this replaces. If a future reading
needs that share settled at the handoff, the number to move is `k_campaign_settle_ticks` and the
measurement to re-run is the one above.

**What settles it is a search, not a settle.** The third pass of simulated history is phase 6 — a
**directed static search** over candidate corporate landscapes (rosters, placements, road tiers),
scored on chain completeness, the supply-to-demand ratio and the *spread* of both, with one short
validation run on the winner. It is not an undirected pre-game tick loop run long enough for the
dust to settle: a settle asks *what survives whatever generation happened to place*, a search asks
*which placement is worth handing over*, and paying for both would pay twice for the weaker answer.
The property that must hold either way is the one the player meets — they enter a field that has
**already been decided** rather than one about to be. The design is
`docs/generation/GENERATION_STRATEGY.md` § The eight phases and § Three passes of simulated
history; the search itself is BL-770 (Era 0 candidate search).

## Three things that say "era" in code, and which one this is

- **`era_band`** (`src/world/recipe_registry.hpp`) tags each authored building type and recipe
  `any` / `ancient` / `industrial`, and the registry masks the browsable roster on the world's band.
  The campaign's band is always `industrial` (§ Where the ladder starts; Ben, 2026-10-10); the
  `ancient` tag survives for content no campaign nation is offered. It is **not** the Era of this
  document.
- **`condition_subject::era`** (`src/world/condition_set.cpp`) is the predicate subject the laws
  and techs layers evaluate. It measures **1 for a nation that owns a launchpad and 0 otherwise** —
  a per-nation proxy for "has industrialised into space", chosen so that authored conditions keep
  their meaning when the Era measure becomes a lookup (NR-113). **Do not author an `era` condition
  that reads wrongly under it:** `era >= 1` means *owns a launchpad*, so a condition capturing "the
  nation has reached space" is fine and one meaning "the campaign has advanced to Era 1" is not,
  since the proxy is per-nation and the Era is world-wide. `condition_set_harness` C2h/C2l pin it.
- **The Era of this document** asks *how far has the campaign got within its arc*. It was renumbered
  1-based on 2026-08-31 (§ The ladder) and **the two axes above did not move with it** — `era_band`
  is still `any`/`ancient`/`industrial`, and `condition_subject::era` still measures 1 for a nation
  that owns a launchpad. That proxy now reads one Era later than its name suggests (space is Era 2
  in this document), which is exactly why the section below insists the axes never merge. A building can
  be `industrial`-band and still Era-0 (a refinery); nothing is `ancient`-band and Era 1. The three
  axes must not be merged — one word with two meanings in one file is the failure the proxy note
  already guards against.

## The point of an Era — an achievement to reach (Ben, 2026-10-26)

> *"I'm thinking the era catastrophe system should be retired. In real play we want to encourage
> successes, rather than avoiding failure."*

**An Era is opened by what a nation achieves, not by a disaster it avoided.** This retires the
2026-08-31 frame, under which each Era carried one catastrophe and passing an Era meant it had not
happened. There is no seeded test date, no countdown, no Ceiling and no failure branch. The next
Era's territory, buildings and quest trees open for a nation **when that nation meets the Era's
gate** — a keystone quest made of deeds (§ Gating on appropriate use).

**Each nation crosses on its own.** Technology is a nation's (`RESEARCH.md` § Who holds
technology), and the space-access gate is already read per nation (§ Era 1 → Era 2 gate), so the
first nation to reach orbit is the first into Era 2. The race is the success the player chases.

**What survives is the cost a technology does not print on its own line.** The 2026-08-05 draft's
point stands — *"more advanced does not mean better... the player must be skilled at avoiding danger,
in each dimension of play"* — but the danger is no longer a world-ending test. It is how other nations
**treat** you:

1. Some attractive technologies carry a cost that is **not on their own stat line**.
2. That cost accumulates as **Alarm**, which the player can **watch** and **act on**.
3. Alarm's consequence is **deterministic and relational**: rival nations' scorers read it when they
   weigh an embargo, a coalition or a declaration of war. It informs those decisions and never makes
   one (`../politics/RELATIONS.md` § Alarm).

### Alarm — kept, as a quantity between nations

**Alarm is how threatened one nation feels by another**, and it is a relations quantity:
`../politics/RELATIONS.md` owns it. It is moved by the other nation's **visible** capability,
severed trade ties, military posture and domestic instability, and it decays.

**The epoch opens with the Alarm the history left, and a world war leaves more (Ben, 2026-09-15).** A
world that fought a world war late in the pre-game span opens with survivors whose standing force
their neighbours can see, grudges written by the dead and trade flows the war severed — all three
Alarm's inputs (`../generation/INDUSTRIALISATION.md` § A world war leaves the campaign more alarmed).
It opens a tenser world, not a doomed one.

**Ceiling is retired** with the test it was compared against.

### Mistakes compound — the red herrings

The technologies that raise Alarm are the ones that look best. Six kinds of trap, one danger in each
dimension of play:

| Kind | The trap | The tell |
|---|---|---|
| **Escalator** | Genuinely better output; the capability is dual-use | Its own dual-use description; rival alarm ticks on completion |
| **Legibility trap** | The capability is fine — being *seen* to hold it is the cost | A large, visible, single-site facility |
| **Interdependence severer** | Autarky looks like resilience, and cuts the trade ties holding Alarm down | Your own export routes go quiet |
| **Brittle optimisation** | Better numbers, far worse under blockade | The input it optimises is one you import |
| **Contextual dud** | Advanced, and wrong *here* | Its prerequisite substrate is absent on this body |
| **Domestic destabiliser** | Output up, unrest up — and unrest is what makes a nation reach for a foreign enemy | The unrest surface moves the same tick |

The tempo trap — a technology that costs you the clock — retired with the clock.

**Every trap carries a tell, and the tell precedes commitment.** That is the project's tone rule —
legible in hindsight, not locally clever — and it is the whole difference between a skill test and a
gotcha. A player who reads the tell and takes the technology anyway has made a *decision*; a player
who could not have known has been cheated.

**One herring is inverse, deliberately.** If every menacing-looking node were a trap, "menacing"
would become the tell and the lesson would collapse into colour-coding. At least one node must look
aggressive and be **stabilising** — a survivable force is not a panicked one.

### Gating on appropriate use

Capstones are opened by **deeds** — tangible actions taken in game — not by accumulated points. That
is what "gating certain technologies behind appropriate use of prior ones" means concretely: the
next rung is earned by having *used* the last one. The drafted node set and the herring roster live
in `docs/research/ERA1_TECH_LANDSCAPE.md` (research, not authority).

---

## What moves an Era

**An Era moves when a nation meets its gate** (Ben, 2026-10-26). This retires the 2026-07-08 reading
under which Eras were seeded events on the world clock that shocked markets and destroyed
infrastructure. Technology never opens an Era by itself; capstone technologies open **quest
trees**, and a quest tree's keystone is the gate.

**Nuclear war is not an Era's outcome.** A strategic nuclear arsenal is held, seen and deterring,
and raises Alarm; no verb fires it (`../military/MILITARY.md` § Missiles). The backstory may still
hold a near-miss the world pulled back from (`../lore/HISTORY.md` Stage 5) — that is lore about why
the arsenal is feared, not a test ahead of the player.

---

## Era 1 — Terrestrial

**Starts:** the campaign epoch.
**Territory:** home planet and bodies of equivalent class (rocky, habitable, or near-habitable planets).
**Strategic question:** build an industrial base capable of funding and supplying the first space operation.

### Character

The opening economy is heavy industry: iron ore, coal, petroleum, and the products derived from them. Nations trade globally but the solar system is a project, not yet an industry. The player competes for terrestrial resources and market position while working toward the economic threshold required for space access.

The epoch signals the technology level and industrial character, not historical accuracy. The home planet's specific resource profile varies between campaigns (procedurally generated); what matters is that Era 1 resources are bulk, extractable, and refinable with pre-space infrastructure.

### Available resources

All home-body raw materials and their downstream products: iron ore, coal, petroleum, silica, copper ore, rare earth ore, agricultural produce, plus the refined goods and products they feed. Space-sourced resources (iron-nickel ore, platinum group metals, regolith) are inaccessible. Water trades terrestrially from tick 0 — it is extracted from icy ground and is not space-gated (RESOURCES.md § What actually trades).

### Available buildings

Mine, Oil Platform, Farm, Smelter, Refinery, Chemical Plant, Electronics Lab, Fabricator, Food Processor, Port. These are the Era's *roles*; the authored `building_type` roster expresses them as generic extraction and processing buildings (PRODUCTION.md § Extraction buildings).

The Launchpad can be **constructed** during Era 1 (a ground installation; authored cost steel 50 + refined fuel 20 plus 500 credits, `scripts/economy.lua`) but cannot be **operated** until the space-access gate is fully met.

### Era 1 → Era 2 gate — space access

This is the **keystone quest** that unlocks space access (BL-087). All three conditions must be met simultaneously:

1. **Rocketry research purchased.** Rocketry is the only technology required for space access; the full quest tree is BL-087's (era tech/quest system). Whether it stands alone or is the Digitisation tree's Reach quest is open (`../generation/trees/TREES.md` § Open questions).
2. **Launchpad constructed and staffed.** A Launchpad building exists on the home body with `workforce_assigned > 0`.
3. **Propellant on the shelf.** A market on the home body holds at least the minimum propellant quantity required for an initial launch on its shelf — where every launch buys its propellant (`PRODUCTION.md` § Launchpad). This threshold is a Lua balance value. Propellant is the product of the propellant loop (`docs/research/ERA1_TECH_LANDSCAPE.md`), not a roster resource of the ancient arc.

**The gate says what leaving costs.** Reaching orbit requires the lift stack that frightens everyone,
so the space programme is *itself* the largest single contributor to Alarm (§ Alarm). The player
cannot dodge the dangerous technology — the skill is buying the reassurance that lets them hold it
while they race for orbit.

When all three conditions hold, space bodies become accessible to trade, and the Ice Extractor and Assembly Plant become available for construction. The operative half of the gate in the shipment layer is launchpad presence and propellant — a trade between bodies ships iff the trader holds a launchpad on the source body (`corp_has_launchpad_on`, `src/world/supply_system.cpp`; SUPPLY.md § Infrastructure gates) and the source shelf holds the launch's propellant at a price the ceiling admits (`TRADE.md` § A trade).

---

## Era 2 — Early Space

**Starts:** for each nation, when it meets the Era 1 → Era 2 gate (§ The point of an Era).
**Territory:** all bodies in the solar system.
**Strategic question:** establish self-sustaining extraction on at least one off-world body to reduce dependence on the home planet for key inputs.

### Character

Space operations carry a structural cost premium: every convoy leg consumes propellant, and any supply disruption halts production at the destination. The dominant logistical challenge is closing the propellant loop — extracting water off-world and producing propellant in-situ rather than shipping it from home. Corporations that close this loop gain a decisive cost advantage in the outer solar system.

### Newly accessible resources

Water (from icy bodies), iron-nickel ore (from metallic asteroids), platinum group metals (from asteroids), regolith.

### Newly available buildings

Ice Extractor, Surface Extractor, Assembly Plant, Orbital Port.

### Era 2's pressure is climate (Ben, 2026-08-31)

*"I am not against climate change being a large problem for Era 2, but our prototype works solely on
Era 1 for now."* Climate is Era 2's defining pressure, as Alarm is Era 1's — a cost that accumulates
and bites, not a test that ends the Era (§ The point of an Era). It is designed ahead in
`docs/CLIMATE.md` and is deliberately **outside prototype scope**; nothing in the prototype should
be built against it.

### Era 2 → Era 3 gate

Not designed. Placeholder: expansion beyond the home solar system, requiring technologies and resources not yet defined.

What climate suggests about its **shape**, without designing it: if a degrading homeworld is why the
player left, the end of Era 1 is when the homeworld stops being the thing they depend on — which is
the threshold Era 1's own strategic question already names.

---

## Era 3 and beyond

Era 3 likely involves multi-system expansion where the home solar system becomes a single node in a larger trade network. The era system is designed to accommodate this without retroactive changes: each era entry gate is a self-contained condition set, and later eras extend rather than replace the resource and building model.

---

## Era and body accessibility

Bodies are **not** fully visible from day one (BL-067, survey fog). Every body except the home
body starts `survey_phase::hidden`: the player sees its type, orbit, and grid size on the Solar
canvas, but its tile map and deposits are revealed only by a paid survey (`docs/ui/DISCOVERY.md`).
Accessibility (whether a trade can ship there) is a separate axis from visibility; the Era
gate controls the former.
