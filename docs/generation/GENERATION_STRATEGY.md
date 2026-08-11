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
real background corporations** (BL-365, 2026-08-11), not a nation actor. It is not the player's
playing field and is not surfaced as manageable detail on any per-firm basis; it is the saturated
background the contest happens *on top of*.

**Real firms, calibrated count, not an injected substrate (BL-365, superseding BL-050/BL-078,
2026-08-11).** Earlier designs modelled this background as an abstract *substrate*: a per-capita
demand basket plus a nation-level abstract supply capacity, injected straight into market arrays
each tick by `inject_substrate_demand`, with no building behind either side. That mechanism is
gone. Background industry is now **generated corporations** — `corporation_component.is_background
= true` — with real buildings that actually produce and consume through the ordinary economy tick.
Generation is **calibrated, not authored**: firms are placed until real production reaches ~90% of
real demand for the tradeable resource set (`docs/economy/MARKETS.md` § Background corporations),
the same figure BL-078's `clearing_fraction` used, now **emergent** rather than **injected**. This
is a **corporation**, not a nation, actor deliberately — `.claude/rules/io-standing-rules.md`
sanctions a scored-utility strategy layer for background *corporations* but forbids nation-level
strategic behaviour, so filling the world with nation-owned industry would need the one actor class
the standing rules forbid. Background firms run the **full corp_ai scored-utility layer**, identical
to today's ~8 rival corps, not a cheaper reduced model (Ben, 2026-08-11). The markets they feed stay
**resource-carved (BL-096)**: a nation's territory fractures into more markets where its
tradeable-resource concentration is high and folds into a neighbour where it is barren, with
nations still the carving actor even though they no longer own the industry. Together these keep
the premise (a saturated base the player competes *on top of*) while making it a legible, fillable
opportunity surface rather than an inert price floor.

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
| `body_count` (`int`) | **Reserved — phased to a follow-on.** The body set is still hand-authored prototype *profiles* (hot inner planet / homeworld / moon / metallic asteroid — their **names** are generated per seed, BL-257); a true count knob needs the generator to synthesise variable body profiles, which is out of BL-114's budget. The field exists so the descriptor is forward-shaped. | heaviest (deferred) |
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
- **Nations** draw territory over the tiles. A nation's `economic_focus` biases which
  corporations register there; carving resource-carved markets (BL-096) stays a nation-level
  act even though the broad industrial base itself is now firm-owned (below).
- **Corporations** are placed within their home nation's territory as **specialists**: a lean,
  focus-coherent set of holdings clustered in the nation (see `CORPORATION_GENERATION.md`). A
  later, separate generation pass places **background** corporations that carry the broad
  industrial base the premise above describes (BL-365, `CORPORATION_GENERATION.md` § Pass 6) —
  the specialist premise for the player and named rivals is unchanged; only who fills in the
  rest of the economy changed, from an abstract substrate to real background firms.

---

## Real history in, invented names out (Ben, 2026-08-03)

A standing constraint on every generation pass, stated here because it cuts across all of them.

**The design leans on real history deliberately, and should keep doing so.** The institutional
ladder (`docs/lore/HISTORY.md`) derives fragmentation, nation count and industrialisation timing
from a real causal sequence; the Era −1 sim (BL-271) was filed off "use Rome as a sandbox"; the
mil-sim (BL-272) takes real constants. That is the cheapest source of mechanisms that are known
to work, and abandoning it would mean inventing social physics from nothing.

**What transfers is the mechanism. What never transfers is a proper noun.**

| Transfers | Does not |
|---|---|
| How a charter makes a promise enforceable | "The Hanseatic League" |
| How a growth front stalls at a strait and leaves an exclave | "Sicily" |
| How an inland sea concentrates littoral power | "The Mediterranean" |
| Plausible hegemony-formation speed, campaign-season length, supply radii | "Rome" |

Every generated proper name — nation, province, population centre, corporation, body, person —
is **sci-fi / fantasy**, produced by the seeded template banks and phoneme tables described in
the per-subject naming passes (`NATION_GENERATION.md` § Pass 5, `CORPORATION_GENERATION.md`
§ Pass 5, `generate_city_name`). Two consequences worth stating because they are easy to get
wrong:

1. **"Culture-flavoured" must not mean "Earth-culture-flavoured."** The template banks are
   flavoured by *generated* cultural character — the phoneme tables should not read as
   recognisably Latin, Han, Norse or anything else an Earth reader can place. A name that makes
   a player think "that's the Roman one" has failed, however good the mechanism underneath it.
2. **Analogy language in docs is for the reader, not the generator.** When a design doc says
   "its VOC moment" or "Rome as a sandbox", it is orienting a human. Nothing downstream should
   read those words as content, and no name pool should be seeded from them.

The one exception lives outside Io: **Project-Rival** plays an actual RTS with actual
civilisations, because rehearsing the method needed an arena that exists today. It returns
numbers and doctrine to Io — never names.

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
- **Generating the saturated background — closed 2026-07-04, mechanism superseded 2026-08-11.**
  The economic premise (above) says the broad background industry is generated, not authored; this
  was originally **[B4]** (BL-050, shipped 2026-06-17 — a per-tile industry/productivity field
  aggregated into per-body markets) and rendered as the Industry map-lens (BL-084, shipped
  2026-07-04; see `docs/ui/LENSES.md`). **BL-050's and BL-078's substrate mechanism is itself
  superseded by BL-365** (2026-08-11): the background is now real generated corporations, not a
  per-tile field or an injected nation capacity — see § The economic premise above and
  `docs/economy/MARKETS.md` § Background corporations. The Industry lens's rendering job is
  unaffected; what changed is what it renders the aggregate of.
