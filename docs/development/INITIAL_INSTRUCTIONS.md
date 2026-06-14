# Project Io — Initial Development Instructions

You are assisting with the development of Project Io. The project's concept, systems design, glossary, and technical decisions are defined in the accompanying documents. Read those first. This document covers the prototype build sequence and the constraints that govern your assistance.

---

## Prototype scope

The prototype validates the **economy loop only**. Refer to TECH_FOUNDATIONS for the full list of exclusions. When a question or request touches something outside the economy loop, say so and decline to design or implement it rather than finding a way to include it.

---

## Build sequence

Development follows these layers in order. Do not design or implement anything that depends on a layer not yet complete. If asked to work on a later layer before an earlier one is solid, flag it.

### Layer 0 — Engine scaffolding
SDL3 window, input, and render loop. Fixed-timestep simulation loop with accumulator. Economy tick firing at a configurable rate on top of the simulation loop. sol2 embedded and loading a Lua file successfully. No gameplay yet — only the two loops ticking correctly and Lua alive.

### Layer 1 — Data model
Define core structs: Body, Tile, Resource, Building stub, Market, Unit stub. Hard-code one or two bodies with a small number of tile types. No logic yet — only correctly shaped data that will not need to be retrofitted when later layers are added.

### Layer 2 — Primary canvases

Build the Solar System Canvas and Body Surface Canvas. One canvas occupies the primary viewport; the other renders as an inset minimap. Clicking the minimap swaps which is primary.

See **`docs/ui/CANVASES.md`** for the full visual design, interaction spec, colour palette, coordinate mapping, and implementation approach. That document is the authoritative reference for this layer.

Key data model change in this layer: add `orbital_angle_rad` (float) to `body_component`. Hard-coded authored values for the prototype bodies; not procedurally generated.

ImGui draw lists are the only rendering mechanism introduced. No third-party canvas or mapping library.

### Layer 3 — Production economy
Layer 3 implements the full single-body economic loop: extraction, processing, the corporate stockpile, market supply/demand aggregation, and a basic money loop. *(Re-scoped 2026-06-14 — see DEVLOG; market resolution was collapsed forward from the original Layer 4. Layer 3 operates only the authored starting assets — player construction is Layer 4.)*

- **Extraction.** An extraction building reads its tile's deposit for an authored target resource and credits a fractional quantity to its corporation's stockpile pool **at each economy tick**. Output is linear in deposit richness, workforce, and `(1 − hazard)`.
- **Processing.** A processing building runs a Lua-authored recipe, consuming inputs from the shared per-`(corporation, body)` pool and adding outputs to it. Insufficient inputs scale the building down or idle it (a two-threshold partial run), rather than always halting.
- **Workforce.** An authored `0–1` scalar modifies output rate at both stages (the corporation-wide labour pool is deferred to the population layer).
- **Stockpile.** One pool per `(corporation, body)`, held in a world-level map; extraction and processing outputs accrue there.
- **Market.** At each economy tick, each body's market aggregates **supply** (goods corporations list for sale) and **demand** (processor input shortfalls); transactions clear at **base price**. Price resolution from supply/demand is deferred to a discrete open Brief (TODO.md § Trade).
- **Budget.** A running per-corporation balance: income from sales, outgoings from input purchases, per-building maintenance, and wages. The balance may go negative.

First observable output: stockpile quantities, market figures, and the corporate balance changing each economy tick, surfaced in the Layer 3 economy panel.

The full resource list, building types, and recipes are specified in **`docs/economy/RESOURCES.md`** and **`docs/economy/PRODUCTION.md`**. Implement the **seven-resource prototype subset** described there: four raw materials (iron ore, petroleum, water, agricultural produce), three refined goods (steel, refined fuel, food rations), and four extraction buildings (Mine, Oil Platform, Farm, Ice Extractor) plus three processing buildings (Smelter, Refinery, Food Processor). Define all 23 resource type enum values and all building type enum values from the start so no data-model retrofitting is required when the remainder are authored.

### Layer 4 — Production UI and management
*(Re-scoped 2026-06-14 from "Market and price resolution".)* A production-focused UI pass over the Layer 3 economy: building **construction** (placement, build-cost spend, terrain/deposit validation), building **management** (workforce, recipe selection, sell orders), and **market/economy ledgers**. Layer 3 operates only the authored starting assets; Layer 4 is where the player builds and manages.

Price resolution from local supply/demand — the original Layer 4 content — is now a discrete open Brief (TODO.md § Trade), scheduled independently of this UI work.

### Layer 5 — Supply routing
A supply convoy entity with source body, destination body, cargo, and fractional progress. Progress increments each simulation step; completion evaluates at the economy tick boundary. Delivered cargo adjusts the destination market's supply. Logistical cost is distance-based and reduces profit margin. First spatial dimension: price can now diverge between bodies.

### Layer 6 — Budget
*(Basic balance and income — sales revenue, input purchases, maintenance, wages — now land in Layer 3. Layer 6 extends that to the full budget once logistics and price resolution exist.)* Revenue equals goods sold multiplied by price minus logistical cost. Outgoings equal construction and maintenance as flat per-tick costs. A running balance that can go negative. Budget should visibly reflect pressure from competing demands.

---

## ImGui panels

Wire an ImGui panel alongside each layer as it is built, not at the end. Each panel needs only to make that layer's state observable — a tile inspector, a market price readout, a convoy list, a budget line. These panels are debugging tools and the functional specification for the production UI. Write ImGui code clearly, not cleverly. It is reference material as much as working code.

---

## What to avoid

- Do not suggest or implement anything outside the prototype scope defined in TECH_FOUNDATIONS.
- Do not expose individual tile data to Lua.
- Do not use unprotected sol2 calls where errors can occur.
- Do not add SQLite — flat binary serialisation is correct for now.
- Do not build AI faction behaviour beyond the data model minimum stub.
- Do not introduce retained-mode UI frameworks in place of ImGui for the prototype.

---

## Tone and approach

Every system should justify its existence by feeding into Trade or Conflict. Favour solutions that are legible and composable over solutions that are locally clever but opaque.

When the right approach is uncertain, state the uncertainty and present options with tradeoffs rather than picking one silently. Stay the advisor — the developer makes the calls.