# src/world — scoped instructions

Loaded automatically when working in this directory. The simulation and generation core:
SDL-free, Lua-free, deterministic. `CLAUDE.md` at the repo root holds the full doc map;
this file holds only what every `world/*` session needs.

## Invariants (absolute in this directory)

- **Deterministic, always.** Seeded generation, fixed Tick model, byte-identical replay.
  No wall-clock, no unseeded randomness, no pointer/hash-layout-dependent iteration order.
- **The serialisation seam travels with the change.** A new persistent field lands with its
  flat-binary save/load path in the same commit, or is flagged loudly as owed. No SQLite.
- **AI agency is exception-scoped.** Corp/nation behaviour exists only as the dated,
  deterministic scored-utility grants listed in `.claude/rules/io-standing-rules.md`
  (§ Determinism & data model). Anything beyond those grants — any planner, any ambient
  action on a human-owned corp — is prohibited; do not widen a grant without a dated ruling.
- **AI-facing seams are untrusted input boundaries.** Wire input (`--serve`, MCP) is
  validated as the value that lands in the destination; reject whole commands, mutate
  nothing on rejection. `apply_corp_command` stays permissive only for in-process callers.
- No individual tile data exposed to Lua; no unprotected sol2 calls.

## Doc ownership (read the one your change touches)

| Subject | Authority |
|---|---|
| Markets, prices, orders | `docs/economy/MARKETS.md` |
| Buildings, recipes, workforce | `docs/economy/PRODUCTION.md` |
| Money flows, wages, interest | `docs/economy/FINANCE.md` |
| Resources roster | `docs/economy/RESOURCES.md` |
| Terrain axes, deposits | `docs/economy/TILES.md` |
| Tile pipeline, body profiles | `docs/generation/TILE_GENERATION.md` |
| Plates / continents | `docs/generation/CONTINENTS.md` |
| Planetology, nations, corps (gen) | `docs/generation/*.md` (per subject) |
| Era −1 history sim | `docs/lore/HISTORY.md`, `docs/lore/COLLAPSE.md` |
| Units, combat, muster | `docs/military/MILITARY.md` |
| Corp AI, MCP, agent seams | `docs/ai/AI_OPPONENT.md` |
| The command dictionary | `docs/ai/ACTIONS.json` — a changed `corp_command.hpp` verb
  changes its dictionary entry in the same commit (`node tools/session/render_actions.js`) |

## Verification

Logic here is verified by **headless harnesses** (`tools/verify/*.cpp`, run via the
`verifier-headless` skill) — no unit-test framework, by decision. Goldens and pinned bands
(state hashes, generation sweeps) are contracts: report movement, never re-bless without
authorisation.
