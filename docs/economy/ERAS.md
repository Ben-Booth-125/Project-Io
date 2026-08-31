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

**What the countdown counts is climate** (`docs/CLIMATE.md`). The boundary stays what this section
describes — a seeded date, a visible countdown, a rupture that shocks markets and destroys
infrastructure — and climate supplies its **cause**: a shared per-body commons degrading under the
strain of what every corporation extracts and processes, until competition over it breaks. This is
what makes the sentence above true rather than asserted. Pulling back has to be genuinely possible
for failing to pull back to mean anything, which is why the shape of the climate curve
(CLIMATE.md § 6) is the load-bearing question under this boundary.

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

**The gate says what leaving costs, and nothing about why anyone would.** Climate is the motive
(`docs/CLIMATE.md` § 9): a homeworld getting worse under its own industry turns off-world extraction
from an ambition into an exit. The three conditions above are unchanged by it — climate moves the
reason to pass the gate, never the gate.

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

### Climate in Era 1

Climate state is held per body (`docs/CLIMATE.md` § 5), so spreading production across bodies is
also **spreading strain** — Era 1's existing strategic question acquires a second reason to answer
it. The homeworld's accumulated damage is a fact the player carries rather than escapes.

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
