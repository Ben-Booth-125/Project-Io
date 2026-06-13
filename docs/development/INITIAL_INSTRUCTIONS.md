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

### Layer 3 — Extraction and production
An extraction building reads a tile's resource deposit and adds to a stockpile each simulation step. A processing building consumes one resource type and outputs another. Workforce allocation as a scalar modifier on output rate. First observable output: stockpile numbers changing over time.

### Layer 4 — Market and price resolution
Each market holds supply and demand quantities per good. At each economy tick, price resolves from the supply/demand ratio modulated by global rarity. Extraction output feeds supply into the local market. First closed loop: tiles produce → extraction harvests → market price responds.

### Layer 5 — Supply routing
A supply convoy entity with source body, destination body, cargo, and fractional progress. Progress increments each simulation step; completion evaluates at the economy tick boundary. Delivered cargo adjusts the destination market's supply. Logistical cost is distance-based and reduces profit margin. First spatial dimension: price can now diverge between bodies.

### Layer 6 — Budget
Revenue equals goods sold multiplied by price minus logistical cost. Outgoings equal construction and maintenance as flat per-tick costs. A running balance that can go negative. Budget should visibly reflect pressure from competing demands.

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