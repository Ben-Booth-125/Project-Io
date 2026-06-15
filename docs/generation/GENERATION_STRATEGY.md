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
- **Generating the saturated substrate.** The economic premise (above) says the Nation AI owns the
  broad background industry, but that substrate is currently *described, not generated* — so the
  world does not yet read as saturated. How it is represented and generated (productivity field /
  background buildings / economic aggregate) is **design-owed**; see OPENS § Environment
  → § Cross-cutting **[B4 ~] Generate the saturated nation-owned background substrate**.

> ⟳ Added 2026-06-15 (B4 Q&A): forward pointer to the new substrate-generation Brief. Pending user review.
