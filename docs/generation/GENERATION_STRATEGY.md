# Project Io — Generation Strategy

This document is the **map of the generation layer** — the strategy that ties the
per-subject generation docs together — and the home of the **economic premise** the whole
campaign setup derives from. Each subject below has its own authoritative doc; this one
summarises how they relate and records the cross-doc decisions that no single one owns.

The subject docs:

- **`PLANETOLOGY.md`** — **implemented (first cut, BL-167 complete)** — the body-level history
  pass: generated atmosphere/chemistry and a simulated abiogenesis/evolution history, ahead of
  tile generation, in the spirit of Shadow Empire's Planetology phase. It now *derives* each
  `body_profile` that used to be hand-authored.
- **`CONTINENTS.md`** (new 2026-07-31) — the plate-drift pass (BL-210 first slice, landed
  2026-07-28): plates derived from Planetology's Engine output, feeding a height bias into the
  tile pipeline's Pass 1.
- **`TILE_GENERATION.md`** — the procedural tile pipeline (terrain, ocean, deposits) per body.
- **`NATION_GENERATION.md`** — Voronoi territory placement and nation profiles over the tile map,
  now driven by the pre-national history ladder.
- **`../lore/HISTORY.md`** — the institutional history ladder: *why* the 1960 campaign world is
  market-based and non-hegemonic. Stages 0–2 are built (BL-221); Stages 5–6 as written are
  superseded pending BL-223 (averted rupture).
- **`CORPORATION_GENERATION.md`** — corporation placement, focus, holdings, and finance.
- **`GENERATION_LEDGER.md`** — the tuning surface that explains *why* a tile generated as it did
  (Chain half built and player-facing; breadcrumb/lens half still design).

Generation runs (verified against `make_hard_coded_world`, 2026-07-31):

```
planetology → continents → tiles          (per body)
  → population centres → history ladder → nations
  → institutional history → roads → corporations   (Kepler only)
```

A body's atmosphere/history precedes its plates; plates precede its terrain; deposits exist
before territory is drawn over them. On Kepler, population centres are placed **before** nations
(so the substrate-density pass can read them), the history ladder runs **before**
`generate_nations` because it *drives* the seed budget, and Stages 1–2 of the institutional
history are recorded **after** — they name and count nations that did not exist a moment
earlier. Roads are stamped once nations + centres exist; corporations are placed last. All
passes are **deterministic** from the campaign seed. Within the tile-generation layer, the
six-pass core stays fixed and every extension lands as a **sibling pass** reading the shared
`generation_record` rather than growing the core pipeline (settled 2026-07-21, BL-051).

---

## The economic premise

The campaign opens on a **saturated, earth-like economy**. The home world's broad industrial
base — the bulk of ordinary extraction, processing, and manufacture — is **owned and run by
the Nation AI as background**. It is not the player's playing field and is not surfaced as
manageable detail; it is the saturated substrate the contest happens *on top of*.

**Refined into two faces (BL-078, 2026-07-07).** The substrate is *redefined, not removed*.
Its **demand face** is population — a price-elastic per-capita basket (so every product has a
real referent and price *discovers* rather than clamping) with minimal met-supply-keyed growth.
Its **supply face** is the nation's abstract production capacity, which tracks that demand and
clears it only *partially*, leaving a **live margin** — the saturation cushion where the nation
has the capacity, and a wide **opportunity gap** where it lacks the deposit (price pegs high;
the gap the player fills). The markets those faces feed are **resource-carved (BL-096)**: a
nation's territory fractures into more markets where its tradeable-resource concentration is
high and folds into a neighbour where it is barren, with nations as the carving actor. Together
these keep the premise (a saturated base the player competes *on top of*) while making it a
legible, fillable opportunity surface rather than an inert price floor.

**Corporations are specialists, not full-chain industrialists.** The player and the major AI
rivals each occupy a **focused slice** of the resource chain and are differentiated by a single
shared trait: an **interest in expanding to space**. The strategic contest is therefore between
a small number of competing, space-interested specialists — not a field of generic firms
reproducing the whole economy.

This premise is what **simplifies the gameplay loop**: the player does not have to own or
balance an entire economy, only to compete as a specialist and convert that position into
off-world reach. It is the reason corporation generation produces **lean, focus-coherent
holdings** rather than a broad spread (see `CORPORATION_GENERATION.md` § Pass 3), and the
reason nation generation carries the broad industrial base implicitly rather than the player
managing it.

---

## The world descriptor — seed + generation parameters (BL-114)

Generation is driven by a small **world descriptor** — a master **seed** plus a `world_params`
struct — chosen on the main-menu **New World** setup and threaded through
`make_hard_coded_world(world_params)`. Same descriptor → identical world on a given binary; this
is the reproducible key the setup screen surfaces (with a dice-randomise and a copyable readout).
The descriptor lives in the app, **not** the `world` struct, so it stays off the serialisation seam.

**`world_params` fields and how each maps onto the generators:**

| Field | Maps to | Cost |
|---|---|---|
| `seed` (`uint32_t`) | XOR-folded into each **existing per-body seed literal** (`params.seed ^ 0xC1D0001u`, …). Seed `0` yields the original literals, so the **default descriptor reproduces the legacy world bit-for-bit**. | cheap |
| `abundance` (`sparse`/`lean`/`standard`) | A **deposit-density scalar** applied as a pure post-multiply in `generate_body_tiles` Pass 6 (`0.40` / `0.65` / `1.00`). Consumes no RNG, so `standard` (1.0) is bit-identical to the unscaled surface. | cheap, isolated |
| `body_count` (`int`) | **Reserved — phased to a follow-on.** The body set is still hand-authored prototype profiles (Cinder/Kepler/Selene/Pallas); a true count knob needs the generator to synthesise variable body profiles, which is out of BL-114's budget. The field exists so the descriptor is forward-shaped. | heaviest (deferred) |
| `preferences` (`world_preferences`) | The New World wizard's input (BL-167): eight **leans** (`any`/`low`/`mid`/`high`), resolved against the seed by `resolve_preferences` with reject-and-reroll until the homeworld clears the strict Earth-like floor. Preferences, not parameters — see `PLANETOLOGY.md` § Preferences, not parameters. | cheap |

**There is no nation-count field** — still true. The number of nations on the home body is a
*consequence* of generation, not a descriptor input: seeds scale with habitable land area,
every nation below a minimum viable territory is absorbed (`NATION_GENERATION.md` § Pass 1 /
Pass 2c), and since BL-221 (pre-national ladder, landed 2026-07-30) the seed budget itself is
**driven by the history ladder's `fragmentation_q`** — a broken, many-cradled world seeds more
densely and keeps smaller survivors (`nation_params_from_ladder`, `history_ladder.cpp`). The
New World setup screen therefore has no nations slider — a world's political granularity is
something the player discovers, not something they dial in.

**Abundance honours the resource ceiling (above).** `standard` **is** the earth-like ceiling
(1.0×); the other tiers step *down* (`lean` 0.65, `sparse` 0.40) — there is no tier above Earth.
Determinism is verified headlessly by `tools/verify/world_determinism.cpp` (same seed → identical
world; different seed → different; `sparse < lean < standard`). The descriptor is also the natural
input to the staged-generation Tile Ledger ([[BL-100]]) as a tuning surface.

---

## How the layers compose

- **Tiles** establish the physical reality: terrain, hazard, habitability, and resource
  deposits. Nothing downstream contradicts tile data — nations and corporations are *placed
  onto* it.
- **Nations** draw territory over the tiles and carry the broad industrial base (the premise
  above). A nation's `economic_focus` biases which corporations register there.
- **Corporations** are placed within their home nation's territory as **specialists**: a lean,
  focus-coherent set of holdings clustered in the nation (see `CORPORATION_GENERATION.md`).

---

## The oral-history pivot (BL-210, design-owed — first slices landed)

**Settled direction, 2026-07-28 (Ben):** generation is being reframed from four separate
mechanisms into **one continuous simulated history**, extending BL-167's proven S0–S9 chain
forward and backward:

```
S0 System → S1–S4 Continents (simulated plate drift/collision/rift, replaces the
  mechanical 6-pass heightmap/noise pipeline) → S5–S8 Biosphere (existing BL-167 chain,
  unchanged) → Settlement → Industrialisation → 1900s (replaces Voronoi nations +
  authored corporation focus tables)
```

Where that stands as of 2026-07-31:

| Slice | Status |
|---|---|
| Continents/Drift — plates from Engine output, height bias into tile Pass 1 | **Landed** 2026-07-28 (`CONTINENTS.md`); Continent lens built 2026-07-30 (BL-226) |
| Settlement Stages 0–2 — cradles, charter, border accord; ladder drives the nation seed budget | **Landed** 2026-07-30 (BL-221 pre-national ladder; `../lore/HISTORY.md` § Implementation) |
| Full S1–S4 continents simulation (replacing the remaining noise machinery) | Owed — **BL-210 (oral-history pivot, design-owed)** |
| Industrialisation / later ladder stages | Owed — **BL-222 (industrial ladder, designed)**, **BL-223 (averted rupture, design-owed)** |
| Branch checkpoints (historical-extinction analogues) + lean × branch sweep | Owed — stays with BL-210 |

The gap `PLANETOLOGY.md` § Known weaknesses named as the largest missing connection — *"the
pass hands nothing to nation or corporation generation"* — is now **half-closed**:
`run_history_ladder` consumes the planetology state and `nation_params_from_ladder` feeds its
fragmentation into nation seeding. The corporation half is still open. **Full architecture,
rationale, and the per-doc open questions live in BL-210** (`backlog.json`).

---

## Open cross-doc items

These are design questions that span more than one generation doc; each is also noted in the
owning doc's § Open items.

- **Building tiers / levels.** A level/tier axis for buildings — distinct from **production
  methods (recipes)**. A specialist corporation's footprint may be characterised as much by the
  *tier* of its assets as by their number. Unsettled; interacts with `PRODUCTION.md`.
- **International relations & corporate origin.** Whether **allied nations share corporations**,
  or **prefer generated franchises** across borders, is open — it couples nation diplomacy to
  corporation generation (the Franchise open item in `CORPORATION_GENERATION.md`).
- **Post-WW2 industrial grounding.** The focus→asset-mix patterns should be grounded in research
  on the post-WW2 industries that led to space-related capability, so a specialist's holdings
  read as a plausible pathway toward off-world reach rather than an arbitrary mix.
- **Generating the saturated substrate — closed 2026-07-04.** The economic premise (above) says
  the Nation AI owns the broad background industry; this is no longer just described, it is
  **generated** (BL-050, shipped 2026-06-17 — a per-tile industry/productivity field that consumes
  shared tile building-slots + resources, aggregated into the per-body markets on both supply and
  demand, player-displaceable) and now **rendered** as the Industry map-lens (BL-084, shipped
  2026-07-04; see `docs/ui/LENSES.md`). **[B4]** is closed; residual sub-design (if any surfaces)
  moves to a fresh item rather than reopening this one.
