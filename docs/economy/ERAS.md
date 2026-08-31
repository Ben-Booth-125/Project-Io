# Project Io — Eras

An **Era** is a named phase in the game's industrial arc, defined by the accessible territory, available buildings, and the dominant strategic challenge. Eras are a formal game system: each has a defined entry, a distinct resource profile, and a characteristic question for the player to answer. The game begins in Era 0; an Era changes as a **gear shift that changes what the game is about**, each Era necessitating the one before.

Only Era 0 and Era 1 are designed. Later eras are stubbed.

---

## The two arcs, and where the ladder starts

**The campaign epoch is 0 CE** (Ben, 2026-08-12; NR-177). The player is a mercenary company; the
**ancient arc** is the live product, and the **industrial / space arc** — Era 0's terrestrial
industry and the Era 1 space transition below — is DLC scope, parked on `era/space`
(`docs/development/ROADMAP.md` § The two arcs). The Era *structure* is the same on both arcs;
what differs is where the ladder starts.

**The prehistory is a generator, not a play layer.** Generation runs a pre-epoch history sim that
produces the 0 CE world the campaign opens on: **400 years in one band, at 4 years a tick — 100
decision rounds** (`world_params::prehistory_years = 400`, `src/world/hard_coded_world.hpp`; the
single band is `hp.tick_bands[0] = {epoch_year, 4}` in `hard_coded_world.cpp`). A six-band ladder
(100 → 50 → 20 → 10 → 5 → 1 years) is authored as the `history_sim_params` struct default
(`src/world/history_sim.hpp`) and read by the tile inspector and the harnesses; generation
overrides it on every world. A settle-dominated run is the intended shape, not a defect (NR-205,
ruled 2026-08-12). Authority for the prehistory run is `docs/lore/HISTORY.md`.

**The warm start has no calendar meaning** (BL-369, warm start). `app::start_new_game` runs
`pre_game_ticks` (80, ~20 years) before play begins, then rebases the clock — so play always opens
at the epoch regardless of how many warm-start ticks ran. It is a settling pass that produces a
plausible opening position: the world is generated *at* the epoch and ticked forward to reach a
steady economy, not generated at an earlier date and advanced.

## Three things that say "era" in code, and which one this is

- **`era_band`** (`src/world/recipe_registry.hpp`) tags each authored building type and recipe
  `any` / `ancient` / `industrial`, and the registry masks its browsable roster on the band the
  campaign's `epoch_year` derives (below 1700 → ancient). It answers *which product is this* — a
  world-wide fact fixed at generation (BL-433, era band). It is **not** the Era of this document.
- **`condition_subject::era`** (`src/world/condition_set.cpp`) is the predicate subject the laws
  and techs layers evaluate. It measures **1 for a corp that owns a launchpad and 0 otherwise** —
  a per-corp proxy for "has industrialised into space", chosen so that authored conditions keep
  their meaning when the Era measure becomes a lookup (NR-113). **Do not author an `era` condition
  that reads wrongly under it:** `era >= 1` means *owns a launchpad*, so a condition capturing "the
  corp has reached space" is fine and one meaning "the campaign has advanced to Era 1" is not,
  since the proxy is per-corp and the Era is world-wide. `condition_set_harness` C2h/C2l pin it.
- **The Era of this document** asks *how far has the campaign got within its arc*. A building can
  be `industrial`-band and still Era-0 (a refinery); nothing is `ancient`-band and Era 1. The three
  axes must not be merged — one word with two meanings in one file is the failure the proxy note
  already guards against.

## The point of an Era — a catastrophe to avoid (Ben, 2026-08-31)

> *"The aim of each era is to give a catastrophe for players to avoid. Gating certain technologies
> behind appropriate use of prior ones, and allowing mistakes to compound."*

**Each Era carries one catastrophe, and the Era is about not causing it.** The catastrophe is a
**failure state**, not a scheduled transition: passing the Era means the catastrophe did *not*
happen, and the next Era's quest trees open. This is the frame every other statement in this
document sits inside, and it is what makes an Era a test of skill rather than a chapter break.

Three things must hold for that to be a skill rather than a mood (design drafted 2026-08-05, Ben:
*"more advanced does not mean better... the player must be skilled at avoiding danger, in each
dimension of play"*):

1. Some attractive technologies carry a cost that is **not on their own stat line**.
2. That cost accumulates into something the player can **watch** and **act on**.
3. The catastrophe resolves **deterministically** from it. The standing rule forbids random
   ruptures, and a dice-roll catastrophe would be unlearnable anyway.

### The two scalars

No new world object — two per-nation scalars with named consumers, the discipline BL-223 set for the
deterrence ceiling (*"a per-nation scalar, NOT a nuclear-equivalent object"*):

| Scalar | Meaning | Moved by |
|---|---|---|
| **Ceiling** | How much restraint this nation carries — the memory of the rupture that *was* averted | The history-ladder outcome; decays slowly as the memory ages |
| **Alarm** | How threatened this nation feels *by others* | Others' **visible** capability, severed trade ties, posture, domestic instability |

### The test

The Era event arrives on its **seeded date** with a visible countdown. **The date decides when the
test is taken, not its outcome.**

- **Aggregate Alarm above aggregate Ceiling** → the catastrophe fires. The Era's selective
  destruction lands on exactly the assets the next Era needed, so failing is not a reset — it is
  entering the next Era crippled, which is `docs/CONCEPT.md`'s "you enter the next one
  underprepared, and it is *hard*".
- **Alarm below Ceiling** → averted, and the next Era's quest trees open.

Two properties keep this a skill test rather than a timer. **Alarm is relievable** — it decays,
named nodes cut it, and trade interdependence suppresses it continuously. And **the player is not
its only source**: rival corporations and nations raise it too, so a careful player can still lose a
world someone else wrecked, and managing *others'* alarm is a legitimate goal.

### Mistakes compound — the red herrings

The technologies that raise Alarm are the ones that look best. Seven kinds of trap, one danger in
each dimension of play:

| Kind | The trap | The tell |
|---|---|---|
| **Escalator** | Genuinely better output; the capability is dual-use | Its own dual-use description; rival alarm ticks on completion |
| **Legibility trap** | The capability is fine — being *seen* to hold it is the cost | A large, visible, single-site facility |
| **Interdependence severer** | Autarky looks like resilience, and cuts the trade ties holding Alarm down | Your own export routes go quiet |
| **Brittle optimisation** | Better numbers, catastrophically worse under blockade | The input it optimises is one you import |
| **Contextual dud** | Advanced, and wrong *here* | Its prerequisite substrate is absent on this body |
| **Tempo trap** | A fine technology that costs you the clock | The countdown is visible and does not move |
| **Domestic destabiliser** | Output up, unrest up — and unrest is what makes a nation reach for a foreign enemy | The unrest surface moves the same tick |

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
next rung is earned by having *used* the last one, and used it in a way that did not raise Alarm past
what the player can carry.

**The per-Era catastrophe ladder** is what the Era structure is for. Only the prototype's Era is
designed; the drafted node set and the full herring roster live in
`docs/research/ERA1_TECH_LANDSCAPE.md` (research, not authority).

---

## What moves an Era

**Eras are catastrophic seeded events on the world clock** (BL-087, era tech/quest system — Ben,
2026-07-08). The Era boundary is an external clock: a **seeded date with a visible countdown**,
and the boundary event **shocks markets, destroys some infrastructure, and unlocks the new Era's
quest trees**, all three together. Tech can move faster than the clock but never opens an Era;
capstone techs open **quest trees**. The condition set in § Era 0 → Era 1 gate therefore gates
**space access** — a keystone quest — not the Era transition proper, which is the rupture.

Two ruptures exist and they are **distinct**: HISTORY.md Stage 5's averted rupture is in the
backstory, CONCEPT.md's is ahead of the player. The backstory establishes that these powers *can*
pull back from the brink; the Era 0 exit is the occasion they do not.

**This Era's catastrophe is a NUCLEAR WAR** (Ben, 2026-08-31) — which is what `docs/CONCEPT.md`'s
"global-rupture-scale war" names, and what the player is playing to **avoid**. It is the failure
branch of § The test above, not a scheduled event: Alarm above Ceiling on the seeded date and the
rupture goes hot; below it, and it is averted a second time.

The two ruptures stay distinct and the pair is the point — the backstory is a near-miss that *was*
averted, and the campaign is the second time the question is asked. The player's job is to be the
reason the answer is the same.

---

## Era 0 — Terrestrial

**Starts:** the campaign epoch.
**Territory:** home planet and bodies of equivalent class (rocky, habitable, or near-habitable planets).
**Strategic question:** build an industrial base capable of funding and supplying the first space operation.

### Character

The opening economy is heavy industry: iron ore, coal, petroleum, and the products derived from them. Corporations operate globally but the solar system is a project, not yet an industry. The player competes for terrestrial resources and market position while working toward the economic threshold required for space access.

The epoch signals the technology level and industrial character, not historical accuracy. The home planet's specific resource profile varies between campaigns (procedurally generated); what matters is that Era 0 resources are bulk, extractable, and refinable with pre-space infrastructure.

### Available resources

All home-body raw materials and their downstream products: iron ore, coal, petroleum, silica, copper ore, rare earth ore, agricultural produce, plus the refined goods and products they feed. Space-sourced resources (iron-nickel ore, platinum group metals, regolith) are inaccessible. Water trades terrestrially from tick 0 — it is extracted from icy ground and is not space-gated (RESOURCES.md § What actually trades).

### Available buildings

Mine, Oil Platform, Farm, Smelter, Refinery, Chemical Plant, Electronics Lab, Fabricator, Food Processor, Port. These are the Era's *roles*; the authored `building_type` roster expresses them as generic extraction and processing buildings (PRODUCTION.md § Extraction buildings).

The Launchpad can be **constructed** during Era 0 (a ground installation; authored cost steel 50 + refined fuel 20 plus 500 credits, `scripts/economy.lua`) but cannot be **operated** until the space-access gate is fully met.

### Era 0 → Era 1 gate — space access

This is the **keystone quest** that unlocks space access (BL-087). All three conditions must be met simultaneously:

1. **Rocketry research purchased.** Rocketry is the only technology required for space access. It is a single standalone unlock with no prerequisite; the full quest tree is BL-087's.
2. **Launchpad constructed and staffed.** A Launchpad building exists on the home body with `workforce_assigned > 0`.
3. **Propellant reserve met.** The home body's stockpile contains at least the minimum propellant quantity required for an initial launch. This threshold is a Lua balance value. Propellant is the product of the propellant loop (`docs/research/ERA1_TECH_LANDSCAPE.md`), not a roster resource of the ancient arc.

**The gate says what leaving costs.** What supplies the urgency is the Era's own catastrophe:
reaching orbit requires the lift stack that frightens everyone, so the space programme is *itself*
the largest single contributor to Alarm (§ The point of an Era). The player cannot dodge the
dangerous technology — the skill is buying the reassurance that lets them hold it.

When all three conditions hold, space bodies become accessible to convoy dispatch, and the Ice Extractor and Assembly Plant become available for construction. The operative half of the gate in the convoy layer is launchpad presence — an inter-body convoy dispatches iff the corp holds a launchpad on the source body (`corp_has_launchpad_on`, `src/world/supply_system.cpp`; SUPPLY.md § Infrastructure gates).

---

## Era 1 — Early Space

**Starts:** after the Era 0→1 rupture.
**Territory:** all bodies in the solar system.
**Strategic question:** establish self-sustaining extraction on at least one off-world body to reduce dependence on the home planet for key inputs.

### Character

Space operations carry a structural cost premium: every convoy leg consumes propellant, and any supply disruption halts production at the destination. The dominant logistical challenge is closing the propellant loop — extracting water off-world and producing propellant in-situ rather than shipping it from home. Corporations that close this loop gain a decisive cost advantage in the outer solar system.

### Newly accessible resources

Water (from icy bodies), iron-nickel ore (from metallic asteroids), platinum group metals (from asteroids), regolith.

### Newly available buildings

Ice Extractor, Surface Extractor, Assembly Plant, Orbital Port.

### The next Era's catastrophe is climate (Ben, 2026-08-31)

*"I am not against climate change being a large problem for Era 2, but our prototype works solely on
Era 1 for now."* Each Era carries its own catastrophe (§ The point of an Era), and climate is the
**next** one's, not this one's. It is designed ahead in `docs/CLIMATE.md` and is deliberately
**outside prototype scope**; nothing in the prototype should be built against it.

### Era 1 → Era 2 gate

Not designed. Placeholder: expansion beyond the home solar system, requiring technologies and resources not yet defined.

What climate suggests about its **shape**, without designing it: if a degrading homeworld is why the
player left, the end of Era 1 is when the homeworld stops being the thing they depend on — which is
the threshold Era 1's own strategic question already names.

---

## Era 2 and beyond

Era 2 likely involves multi-system expansion where the home solar system becomes a single node in a larger trade network. The era system is designed to accommodate this without retroactive changes: each era entry gate is a self-contained condition set, and later eras extend rather than replace the resource and building model.

---

## Era and body accessibility

Bodies are **not** fully visible from day one (BL-067, survey fog). Every body except the home
body starts `survey_phase::hidden`: the player sees its type, orbit, and grid size on the Solar
canvas, but its tile map and deposits are revealed only by a paid survey (`docs/ui/DISCOVERY.md`).
Accessibility (whether a convoy can be dispatched) is a separate axis from visibility; the Era
gate controls the former.
