# Project Io — Eras

> **Status: designed, NOT implemented (recorded 2026-07-31).** No era state exists in code — no
> enum, no gate check, no transition. The tech side is data-only: `scripts/tech_tree.lua` states
> in its own header that nothing in the simulation reads it (the F9 viewer displays it, BL-087).
> What actually gates space access today is **launchpad presence alone**: an inter-body convoy
> dispatches iff the corp holds a launchpad on the source body (`corp_has_launchpad_on`,
> `src/world/supply_system.cpp`). No research, no propellant reserve, no staffing check.
> The design below stands as the target; the inline markers flag the terms with no code backing.

An **Era** is a named phase in the game's industrial arc, defined by the accessible territory, available buildings, and the dominant strategic challenge. Eras are a formal game system: each has a defined entry gate, a distinct resource profile, and a characteristic question for the player to answer. The game begins in Era 0; transitions are triggered by meeting explicit conditions, not by an automatic timer.

Only Era 0 and Era 1 are designed for the prototype. Later eras are stubbed.

---

## Era 0 — Terrestrial

**Starts:** game start (campaign epoch: 1 January 1960).  
**Territory:** home planet and bodies of equivalent class (rocky, habitable, or near-habitable planets).  
**Strategic question:** build an industrial base capable of funding and supplying the first space operation.

> **The warm start has no calendar meaning (BL-369, 2026-08-11).** `app::start_new_game` runs
> `pre_game_ticks` (80, ~20 years) before play begins, then rebases the clock — so play always
> opens at epoch 1960 regardless of how many warm-start ticks ran. This is a settling pass that
> produces a plausible opening position, not a simulated 1940→1960 history; the world is
> generated *at* 1960 state and ticked forward to reach a steady economy, not generated at an
> earlier date and advanced. Revisit only if the opening year becomes player-visible or
> configurable (see `docs/ui/STARTUP.md`'s New World wizard).

### Character

The opening economy is heavy industry: iron ore, coal, petroleum, and the products derived from them. Corporations operate globally but the solar system is a project, not yet an industry. The player competes for terrestrial resources and market position while working toward the economic threshold required for space access.

The start date signals the technology level and industrial character, not historical accuracy. The home planet's specific resource profile varies between campaigns (procedurally generated); what matters is that Era 0 resources are bulk, extractable, and refinable with pre-space infrastructure.

### Available resources

All Earth-sourced raw materials and their downstream products: iron ore, coal, petroleum, silica, copper ore, rare earth ore, agricultural produce, plus the refined goods and products they feed. Space-sourced resources (iron-nickel ore, platinum group metals, regolith) are inaccessible. *(Water is not on that inaccessible list in practice — it trades terrestrially from tick 0; RESOURCES.md § What actually trades.)*

### Available buildings

Mine, Oil Platform, Farm, Smelter, Refinery, Chemical Plant, Electronics Lab, Fabricator, Food Processor, Port.

*(None of these named types is a `building_type` value — the prototype ships generic extraction/processing buildings; PRODUCTION.md § Extraction buildings.)*

The Launchpad can be **constructed** during Era 0 (it is a ground installation requiring steel and machinery) but cannot be **operated** until the Era 0→1 gate is fully met. *(Machinery has no `resource_type` value; the launchpad's real authored cost is steel 50 + refined fuel 20 plus 500 credits, `scripts/economy.lua`. And in code a built launchpad operates immediately — the operate-gate is unimplemented, per the banner.)*

### Era 0 → Era 1 gate

> **Superseded as an *Era* gate (2026-08-03, Ben — NR-025).** BL-087's Era reframe of 2026-07-08
> holds that Eras **are catastrophic seeded events on the world clock**, and it explicitly
> re-reads the three-condition set below as gating a **quest tree**, not an Era. This section is
> therefore correct as a *condition set* and wrong about what it gates: meeting these three
> conditions unlocks space access, but the Era 0 → Era 1 transition proper is the rupture on the
> world clock. Corrected here rather than waiting on BL-087 to land, because this was the one
> doc still asserting something the design had already superseded — the rename to **Filter**
> (v0.3.0) is where the vocabulary catches up.
>
> The wider reconciliation, for the record: the Era 0 rupture disagreement was **four-way**, not
> three-way, and the two ruptures are **distinct** — HISTORY.md Stage 5's averted rupture is in
> the backstory, CONCEPT.md:51's is ahead of the player. CONCEPT.md stands unamended. The
> reading is deliberate rather than a reconciliation of convenience: the backstory establishes
> that these powers *can* pull back from the brink, and the Era 0 exit is the occasion they do
> not.

All three conditions must be met simultaneously:

1. **Rocketry research purchased.** Rocketry is the only technology required for the Era 0→1 transition. In the prototype it is a single standalone unlock with no prerequisite. The full tech tree is a post-prototype feature.
2. **Launchpad constructed and staffed.** A Launchpad building exists on the home body with `workforce_assigned > 0`.
3. **Propellant reserve met.** The home body's stockpile contains at least the minimum propellant quantity required for an initial launch. This threshold is a Lua balance value. *(Propellant has no `resource_type` value; no such threshold exists in the Lua constants.)*

When all three conditions hold, Era 1 begins: space bodies become accessible to convoy dispatch, and the Ice Extractor and Assembly Plant become available for construction. *(In code, condition 2's presence half is the whole gate — see the banner.)*

---

## Era 1 — Early Space

**Starts:** after the Era 0→1 gate is met.  
**Territory:** all bodies in the solar system.  
**Strategic question:** establish self-sustaining extraction on at least one off-world body to reduce dependence on the home planet for key inputs.

### Character

Space operations carry a structural cost premium: every convoy leg consumes propellant, and any supply disruption halts production at the destination. The dominant logistical challenge is closing the propellant loop — extracting water off-world and producing propellant in-situ rather than shipping it from Earth. Corporations that close this loop gain a decisive cost advantage in the outer solar system.

### Newly accessible resources

Water (from icy bodies), iron-nickel ore (from metallic asteroids), platinum group metals (from asteroids), regolith.

### Newly available buildings

Ice Extractor, Surface Extractor, Assembly Plant, Orbital Port.

*(None is a `building_type` value — all four are design targets.)*

### Era 1 → Era 2 gate

Not designed for the prototype. Placeholder: expansion beyond the home solar system, requiring technologies and resources not yet defined.

---

## Era 2 and beyond

Deferred. Era 2 likely involves multi-system expansion where the home solar system becomes a single node in a larger trade network. The era system is designed to accommodate this without retroactive changes: each era entry gate is a self-contained condition set, and later eras extend rather than replace the resource and building model.

---

## Era and body accessibility

Bodies are **not** fully visible from day one (corrected 2026-07-31 — BL-067, survey fog,
superseded the original claim here). Every body except the home body starts
`survey_phase::hidden`: the player sees its type, orbit, and grid size on the Solar canvas, but
its tile map and deposits are revealed only by a paid survey (`docs/ui/DISCOVERY.md`). The
underlying point stands with that amendment: accessibility (whether a convoy can be dispatched)
is a separate axis from visibility, and the designed era gate would control the former.
