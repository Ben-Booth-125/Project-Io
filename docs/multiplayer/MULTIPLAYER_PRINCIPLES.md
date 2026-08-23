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
  do this.
- **Inputs batched as discrete actions applied at the step boundary.** The render loop accumulates
  input and applies it in batch at the start of each simulation step. Networked play replaces "local
  input" with "the set of all players' commands for tick N", applied at the same boundary — the same
  seam, a different source.
- **Full state snapshot at the economy-tick boundary** (the save model). The world half is
  `write_world_snapshot` / `read_world_snapshot` (`src/world/world_save.{hpp,cpp}`, magic `"IOSV"`
  plus a version word, mismatch rejected); the app-layer envelope lives in
  `src/core/save_game.{hpp,cpp}` (TECH_FOUNDATIONS.md § Save model). The snapshot doubles as the
  **resync / late-join / desync-recovery** payload, and the thing to checksum against it is the
  state hash — see § The deterministic state hash below.

In other words: the tick model, the input model and the snapshot are load-bearing pieces of a
lockstep netcode that does not exist; only the netcode itself is missing.

---

## Preserve now

These are the properties to protect during ordinary prototype work. They cost nothing extra to
maintain today and are expensive to recover once lost. They are framed as constraints, not tasks.

1. **Determinism in `world/*` is the whole game.** Lockstep means every client re-derives identical
   state from identical inputs; any divergence desyncs. This is already a standing rule
   (`.claude/rules/io-standing-rules.md` § Determinism). Multiplayer raises its stakes from "saves
   are reproducible" to "clients stay in sync" — so treat any new non-determinism in the simulation
   as a correctness bug, not a cosmetic one.

2. **Keep player actions expressed as discrete, serialisable commands.**
   **`corp_command`** (`src/world/corp_command.hpp`) is exactly the lockstep primitive —
   `{tick, corp, verb, args}` over an append-only `corp_verb` enum, with rejections returned as
   data (`corp_command_result`) rather than thrown. Its sibling `canvas_command` is
   **navigation-only**; `corp_command` is the sim-mutating one (AI_OPPONENT.md § 6). Route any
   new player action through that seam rather than mutating simulation state ad hoc.

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
  Entirely additive: it wraps the existing sim loop, feeding it the merged command set per tick.
- **Save-schema versioning.** The prototype's flat-binary serialisation is correct for
  single-player (`TECH_FOUNDATIONS.md` § Tile and body data model). Every stream carries a magic
  plus a `uint32_t` version and rejects a mismatch rather than reinterpreting it (`"IOHL"`,
  `"IOOB"`, `"IOPC"`, `"IOSV"`). Networked clients must agree on the exact byte layout, so a
  protocol/schema version handshake becomes necessary — but migration and forward compatibility
  are a deliberately deferred decision, not a reversal of one.
- **Multiple simultaneous actors.** Multiplayer means several corporations acting at once. The data
  model is already required not to preclude multi-faction play (`TECH_FOUNDATIONS.md` § Factions),
  and every corporation is an equal `corporation_component` acting through the same seam, so this
  extends the existing model rather than replacing it.

None of these is load-bearing on a decision being made now. They are named here only so a future
reader can see the whole shape.

---

## The deterministic state hash

**`world::state_hash(int tick)`** (`src/world/world.{hpp,cpp}`) folds the fields a tick may mutate
into one `uint64_t`; it is sampled by `tools/verify/ai_skill_harness.cpp`, `determinism_harness.cpp`
and `save_roundtrip.cpp` among others. It has value in *both* worlds, which is why it is worth
recording here. As a single-player tool it is a save/regression aid: two runs of the same seed hash
identically. In a lockstep context it is exactly the **desync-detection** primitive — clients
exchange and compare the hash each tick, and a mismatch localises the divergence immediately.

Note what it hashes: live world state at a tick, not the serialised snapshot. The snapshot
serialises the authoritative containers and rebuilds derived caches on load, so the two agree on
what is authoritative; any field added to one should be weighed for the other.

---

## Cross-references

- `docs/tech/TECH_FOUNDATIONS.md` — the settled engine decisions this doc leans on (tick
  architecture, save model, scripting boundary, serialisation, factions).
- `.claude/rules/io-standing-rules.md` § Determinism & data model — the always-on determinism
  invariants that this doc reframes as multiplayer-load-bearing.
