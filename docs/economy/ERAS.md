# Project Io — Eras

An **Era** is a named phase in the game's industrial arc, defined by the accessible territory, available buildings, and the dominant strategic challenge. Eras are a formal game system: each has a defined entry gate, a distinct resource profile, and a characteristic question for the player to answer. The game begins in Era 0; transitions are triggered by meeting explicit conditions, not by an automatic timer.

Only Era 0 and Era 1 are designed for the prototype. Later eras are stubbed.

---

## Era 0 — Terrestrial

**Starts:** game start (campaign epoch: 1 January 1960).  
**Territory:** home planet and bodies of equivalent class (rocky, habitable, or near-habitable planets).  
**Strategic question:** build an industrial base capable of funding and supplying the first space operation.

### Character

The opening economy is heavy industry: iron ore, coal, petroleum, and the products derived from them. Corporations operate globally but the solar system is a project, not yet an industry. The player competes for terrestrial resources and market position while working toward the economic threshold required for space access.

The start date signals the technology level and industrial character, not historical accuracy. The home planet's specific resource profile varies between campaigns (procedurally generated); what matters is that Era 0 resources are bulk, extractable, and refinable with pre-space infrastructure.

### Available resources

All Earth-sourced raw materials and their downstream products: iron ore, coal, petroleum, silica, copper ore, rare earth ore, agricultural produce, plus the refined goods and products they feed. Space-sourced resources (water, iron-nickel ore, platinum group metals, regolith) are inaccessible.

### Available buildings

Mine, Oil Platform, Farm, Smelter, Refinery, Chemical Plant, Electronics Lab, Fabricator, Food Processor, Port.

The Launchpad can be **constructed** during Era 0 (it is a ground installation requiring steel and machinery) but cannot be **operated** until the Era 0→1 gate is fully met.

### Era 0 → Era 1 gate

All three conditions must be met simultaneously:

1. **Rocketry research purchased.** Rocketry is the only technology required for the Era 0→1 transition. In the prototype it is a single standalone unlock with no prerequisite. The full tech tree is a post-prototype feature.
2. **Launchpad constructed and staffed.** A Launchpad building exists on the home body with `workforce_assigned > 0`.
3. **Propellant reserve met.** The home body's stockpile contains at least the minimum propellant quantity required for an initial launch. This threshold is a Lua balance value.

When all three conditions hold, Era 1 begins: space bodies become accessible to convoy dispatch, and the Ice Extractor and Assembly Plant become available for construction.

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

Ice Extractor, Assembly Plant, Orbital Port.

### Era 1 → Era 2 gate

Not designed for the prototype. Placeholder: expansion beyond the home solar system, requiring technologies and resources not yet defined.

---

## Era 2 and beyond

Deferred. Era 2 likely involves multi-system expansion where the home solar system becomes a single node in a larger trade network. The era system is designed to accommodate this without retroactive changes: each era entry gate is a self-contained condition set, and later eras extend rather than replace the resource and building model.

---

## Era and body accessibility

Space bodies are visible on the Solar canvas from day one — the player can see Cinder, the asteroid belt, and Selene from the opening screen. Accessibility (whether a convoy can be dispatched there) is what the era gate controls, not visibility. This keeps the solar system legible as a strategic map throughout Era 0 even when its bodies are out of reach.
