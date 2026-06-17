# Project Io — Multiplayer Principles

> **Status: forward-looking, non-binding.** Multiplayer is **not** in the prototype scope
> (`docs/tech/TECH_FOUNDATIONS.md` § Direction) and is **not** on the backlog. Nothing here is a
> commitment to build, and no item should be promoted on the strength of this doc. Its purpose is
> narrow: record *which already-settled decisions keep multiplayer cheap later*, and *which
> properties must not be casually traded away in the meantime*. Read it as a set of preservation
> constraints, not a plan.

> **For an agent reading this later:** the one actionable takeaway is in § Preserve now — those are
> properties to protect in day-to-day prototype work. Everything else is context for *why*. If a
> change would violate a "preserve now" property for a local convenience, flag it (it is a standing
> determinism concern), do not silently take it.

---

## The headline

The stack does not preclude multiplayer — it is unusually well-shaped for it, and for the same
reason the single-player prototype is shaped the way it is: a **deterministic, fixed-timestep
simulation isolated from real time**. That property is the foundation the cheapest viable
multiplayer model is built on. Most prototypes have to be partly torn apart to retrofit network
play; this one would not, *provided the determinism is not eroded first*.

This is an argument for vigilance, not for building anything now.

---

## The target model — deterministic lockstep

When multiplayer is eventually built, the model of choice is **deterministic lockstep** (also
called synchronised simulation): every client runs the *identical* simulation and exchanges only
**player commands** (small, discrete intents) tagged with the tick they apply on, rather than
replicating full world state across the wire every tick.

This is the standard model for 4X / RTS-shaped games because the state is large (many tiles, many
buildings, many convoys) while the *input* is tiny. Replicating state every tick scales with the
world; replicating commands scales with the players. For Project Io's data sizes, lockstep is the
natural fit.

Three of the engine's settled decisions are exactly the primitives lockstep needs:

- **Fixed simulation tick, independent of frame rate** (`TECH_FOUNDATIONS.md` § Simulation tick
  architecture). Lockstep advances all clients in agreed tick steps; a real-time-coupled sim cannot
  do this. Already true.
- **Inputs batched as discrete actions applied at the step boundary.** The render loop accumulates
  input and applies it in batch at the start of each simulation step. Networked play replaces "local
  input" with "the set of all players' commands for tick N", applied at the same boundary — the same
  seam, a different source.
- **Full state snapshot at the economy-tick boundary** (the save model). That snapshot is already a
  deterministic, reproducible state image. In a networked context it doubles as the **resync /
  late-join / desync-recovery** payload, and as the thing you checksum to *detect* desync.

In other words: the save system, the tick model, and the input model are each, independently,
load-bearing pieces of a lockstep netcode that does not exist yet.

---

## Preserve now

These are the properties to protect during ordinary prototype work. They cost nothing extra to
maintain today and are expensive to recover once lost. They are framed as constraints, not tasks.

1. **Determinism in `world/*` is the whole game.** Lockstep means every client re-derives identical
   state from identical inputs; any divergence desyncs. This is already a standing rule
   (`.claude/rules/io-standing-rules.md` § Determinism). Multiplayer raises its stakes from "saves
   are reproducible" to "clients stay in sync" — so treat any new non-determinism in the simulation
   as a correctness bug, not a cosmetic one.

2. **Keep player actions expressed as discrete, serialisable commands.** The canvas command
   vocabulary (`canvas_command`, shared between verify scripts and keyboard bindings) is already
   heading this way. The closer every meaningful player action is to "a small struct that can be
   recorded, replayed, and sent", the closer the engine is to lockstep-ready. Favour routing actions
   through that command layer over ad-hoc mutation of simulation state.

3. **Keep the C++/Lua boundary narrow, hot-path simulation in C++.** Cross-machine determinism of
   scripted logic is fragile; data-definition Lua is fine. This is already the settled boundary
   (`TECH_FOUNDATIONS.md` § Language and scripting) — multiplayer is another reason not to let
   simulation logic drift into Lua.

4. **Keep the tick-boundary snapshot reproducible and tick-tagged.** Anything that makes the
   snapshot depend on wall-clock time, frame timing, or render state weakens both saves and the
   future resync path.

---

## The one sleeper risk — floating-point determinism

The non-obvious hazard. Same-binary play (two copies of the identical build) is usually fine.
**Cross-platform or cross-compiler** lockstep can diverge on floating-point results — x87 vs SSE
code paths, fast-math reordering, and platform-specific transcendental functions (`sin`, `exp`,
`pow`) can each produce a last-bit difference, and lockstep amplifies a last-bit difference into a
full desync over time.

Project Io's price resolution, extraction yields, and supply calculations are float-based, so this
is a real future consideration. **No action is warranted now** — solving it prematurely (fixed-point
arithmetic, strict-FP flags) would be scope creep against a feature that does not exist. The only
present-day discipline is *awareness*:

- Do not enable fast-math (`/fp:fast`, `-ffast-math`) for the simulation translation units.
- Do not scatter transcendental calls through hot simulation paths gratuitously.

Both are good hygiene regardless of multiplayer; they simply also keep the option open.

---

## Later work — additive, not a rewrite

For completeness, the pieces that *would* need real work when multiplayer is built. The point of
listing them is to confirm they sit *beside* the simulation rather than requiring it to be
restructured:

- **A netcode layer** — transport, lockstep tick scheduling, input-delay buffering, and reconnection.
  Entirely absent today; entirely additive. It wraps the existing sim loop, feeding it the merged
  command set per tick.
- **Save-schema versioning.** The prototype's flat-binary, unversioned serialisation is correct for
  single-player (`TECH_FOUNDATIONS.md` § Tile and body data model). Networked clients must agree on
  the exact byte layout, so a protocol/schema version handshake becomes necessary — but this is
  already a deliberately deferred decision, not a reversal of one.
- **Multiple simultaneous actors.** Multiplayer means several corporations acting at once. The data
  model is already required not to preclude multi-faction play (`TECH_FOUNDATIONS.md` § Factions),
  and units/factions exist as data-model stubs, so this extends the existing model rather than
  replacing it.

None of these is load-bearing on a decision being made now. They are named here only so a future
reader can see the whole shape.

---

## One idea this surfaces (parked)

A **deterministic state hash at the economy-tick boundary** — a checksum over the tick snapshot — is
worth noting because it has value in *both* worlds. Today it is a save/regression debugging aid (two
runs of the same seed should hash identically). In a future lockstep context it is exactly the
**desync-detection** primitive: clients exchange and compare the hash each tick, and a mismatch
localises the divergence immediately.

It is **not** on the backlog and is **not** proposed here as work — recorded only so the connection
is not rediscovered from scratch later. If it is ever wanted, it is a small, self-contained,
single-player-justified addition.

---

## Cross-references

- `docs/tech/TECH_FOUNDATIONS.md` — the settled engine decisions this doc leans on (tick
  architecture, save model, scripting boundary, serialisation, factions).
- `.claude/rules/io-standing-rules.md` § Determinism & data model — the always-on determinism
  invariants that this doc reframes as multiplayer-load-bearing.
