# Project Io — Generation Strategy

This document is the **map of the generation layer** — the strategy that ties the
per-subject generation docs together — and the home of the **economic premise** the whole
campaign setup derives from. Each subject below has its own authoritative doc; this one
summarises how they relate and records the cross-doc decisions that no single one owns.

The four subject docs:

- **`TILE_GENERATION.md`** — the procedural tile pipeline (terrain, ocean, deposits) per body.
- **`NATION_GENERATION.md`** — Voronoi territory placement and nation profiles over the tile map.
- **`CORPORATION_GENERATION.md`** — corporation placement, focus, holdings, and finance.
- **`GENERATION_LEDGER.md`** — the design-only tuning surface that explains *why* a tile generated as it did.

Generation runs **tiles → nations → corporations** (deposits exist before territory is
drawn over them; territory exists before corporations are placed within it). All passes are
**deterministic** from the campaign seed.

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

**There is no nation-count field.** The number of nations on the home body is a *consequence* of
generation, not a descriptor input: seeds scale with habitable land area and every nation below a
minimum viable territory is absorbed (`NATION_GENERATION.md` § Pass 1 / Pass 2c). The New World
setup screen therefore has no nations slider — a world's political granularity is something the
player discovers, not something they dial in.

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
