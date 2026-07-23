# Project Io — Corporation Generation

Corporations are the primary actors in the simulation. They extract resources, build
infrastructure, trade goods, and eventually project power off-world. At campaign start,
all corporations — including the player's — are generated procedurally and are present
from turn one.

Corporation system behaviour (trading decisions, expansion logic, diplomatic posture) is
**an open item and deferred from the prototype**. This document covers the generation
strategy and data model.

---

## Design principles

**Corporations are embedded in nations.** Every corporation is legally registered in a
home nation. This determines their starting territory access and initial diplomatic context.
The relationship is legally binding but operationally loose: corporations can operate across
borders, hold foreign assets, and act against their home nation's interests. The legal
binding primarily means the home nation has authority to tax, licence, and — potentially —
seize the corporation's domestic assets.

**Corporations are specialists.** The broad industrial base of the saturated, earth-like home
economy is owned by the Nation AI as background (see `GENERATION_STRATEGY.md` § The economic
premise). A corporation is **not** a full-chain industrialist reproducing the whole economy; it
occupies a **focused slice** of the chain and is differentiated by an interest in expanding to
space. This is why its opening position is a **lean, focus-coherent holding set**, not a broad
spread — the player competes as a specialist, not by out-building an entire economy.

**Industrial focus drives differentiation.** Each corporation has a primary focus along
the resource chain: extraction, refining, or manufactured products. This shapes their
starting assets, their natural trade partners, and their expansion priorities. Focus is not
a hard lock — a corporation can pivot — but it defines their initial position.

**Starting assets reflect tile reality.** A corporation's opening asset position is placed
on tiles within their home nation's territory. The tiles chosen reflect their industrial
focus: an extraction-focused corporation gets a mine or well on a resource-rich tile; a
trade-focused one gets proximity to a port or market. Asset placement uses the same tile
data as the economy.

---

## Generation pipeline

### Pass 1 — Nation assignment

For each generated corporation, a home nation is selected weighted by that nation's
`economic_focus` attribute and population of already-assigned corporations. Nations with
an `extraction` focus attract more extraction-focused corporations; mercantile nations
attract trade-focused ones. A balancing factor prevents any single nation from hosting
all corporations.

Corporation count per campaign is a tunable parameter. The prototype targets **6–10
corporations** on Kepler (including the player's).

### Pass 2 — Industrial focus assignment

Each corporation receives a primary `industrial_focus`:

| Focus | Description | Starting asset type |
|---|---|---|
| `extraction` | Raw material producers | Mine, well, or quarry on a resource-rich tile |
| `processing` | Refiners and converters | Processing building near an extraction cluster |
| `trade` | Logistics and market operators | Warehouse or depot near a high-traffic zone |

Focus is drawn with weighted probability shaped by the home nation's `economic_focus`
and by the current distribution of already-generated corporations, providing diversity
without enforcing a strict quota.

### Pass 3 — Starting asset placement

Each corporation receives a **lean, focus-coherent set of holdings** placed on tiles
within their home nation's territory. Because corporations are **specialists** (above), the
opening set is small and thematically tight — it reads as a foothold in one slice of the chain,
not a broad presence across the nation.

- **Count — lean and specialisation-shaped.** A corporation opens with a **small** holding set
  rather than the earlier flat 3–6 generic spread. The count is **shaped by focus** (a specialist
  occupies the footprint its slice needs). The concrete prototype ranges (inclusive,
  `holdings_range` in `corporation_generation.cpp`) are: **extraction 3–4** (a few deposit tiles),
  **processing 2–3** (processors plus a little feed), **trade 1–2** (about one depot). A nation
  too cramped or deposit-poor to host the drawn count yields fewer — the count is a ceiling the
  placement walks up to, not a guarantee.
- **Cluster to the home nation.** Holdings sit **within the home nation's territory** and pack
  around a single focus-scored **anchor** tile, the remaining slots filling the nearest valid
  unoccupied tiles by grid distance — so a corp's holdings read as one contiguous operation. The
  lean counts ride on top of this retained clustering; with the smaller counts the cluster is
  naturally tighter than the earlier 3–6 spread.
- **Mix follows focus.** The asset mix is shaped by `industrial_focus` — an extraction corp
  places extractors on its richest deposits, a processing corp pairs processors with feed, a
  trade corp a depot. The prototype retains this pattern (applied over the lean counts); the
  patterns themselves are an open item to ground in post-WW2 industrial history (§ Open items).
- **Validity-gated.** Every placement is gated by `placement_rules::can_place` (the shared
  placement seam), so no asset lands on invalid terrain or a zero-deposit tile.

Placement is collision-checked against already-placed assets from other corporations.
No two corporations begin on the same tile.

### Pass 3b — HQ designation & border range (BL-201)

Immediately after placement, each corporation is assigned a **HQ** and an **influence
range** — the data-model foundation of the corporate border (BL-182). The HQ is the
corp's **seat**: the holding nearest its holdings centroid on the home body. The range
is the furthest holding's distance from that HQ plus a **fixed projected reach**, in
**unit-hex distance** (`hex_size = 1`), so a renderer obtains pixels by
`influence_range · hex_size · zoom`. Both are stored on `corporation_component`
(`hq_building`, `influence_range`) and are a pure, deterministic function of the placed
holdings; a corp with no home-body holdings keeps `{null_entity, 0}` (no border). This
is **render-only** — the border drives the Corporation-lens chrome (LENSES.md) but gates
nothing until the deferred BL-182 operate-gate lands. Audited by `world_audit`
(BL-182 R1/R2).

### Pass 4 — Financial profile

Each corporation receives starting capital drawn from a seeded range. A tunable
`wealth_variance` parameter controls spread. Corporations with `processing` or `trade`
focus receive slightly higher starting capital to offset their lack of direct resource
access.

**Pre-game operating history.** Corporations do not open cold. At campaign start the economy
is run forward a fixed number of **pre-game ticks** (currently 12, at app startup after the
economy registry loads) so every corporation enters turn one with a **multi-tick operating
history** — warm stockpile pools and a balance already moved by production, wages, and trade,
rather than the seeded capital alone. The headless `--verify` path stays deterministically
cold (no pre-game ticks) so generation audits remain reproducible.

### Pass 5 — Naming

Each corporation receives a generated name from a corporate naming template bank.
Templates combine structural forms (Holdings, Industries, Extraction Co., etc.) with
seeded identifier strings or geographic references to their home territory.

---

## Player corporation

One generated corporation is flagged as `is_player = true`. In the prototype this is a
fixed assignment — a corporation is selected from the generated set and marked as the
player's. No special generation rules apply: the player starts on the same footing as
any other corporation.

**Corporation selection screen** — the intended flow is that the player is shown an
analytical profile of each generated corporation (territory position, resource access,
industrial focus, starting capital, home nation political context) and can cherry-pick
the final stage of generation: accepting a generated corporation, re-rolling it within
the same nation, or selecting a different nation to generate a corp within. This flow is
**deferred from the prototype**. For now, a corporation is simply marked as the player's
at generation time.

---

## Prototype scope

The prototype does not implement faction behaviour. Corporations are generated, hold
assets, and exist as data — but they take no autonomous economic or strategic actions.
The simulation runs for the player only. This is a deliberate deferral; the generation
data model is designed so that AI behaviour can be added without restructuring it.

---

## Open items

**Corporate borders — HQ range & the tall/wide axis (BL-182).** Corporations should have
**borders too**, symmetric with nations: a corporate sphere projected outward from **HQ
building(s)**, each with an **influence range** (the union of a corp's HQ ranges is its border).
A corp opens with **one HQ** and can **build more as it advances** (tech/law-gated), which creates
the specialisation lever — **tall** (deepen fewer HQs) vs **wide** (spread more HQs). Intended as a
*gameplay* mechanic (range gates/modifies where the corp can operate), not just a visual layer, and
extensible via laws and technology. It **generalises** the player-only home-cluster ring + `hq` star
(BL-085; the seed of this) into a real ranged border for every corporation on the **Corporation**
lens. Deferred with corporation-system behaviour (post-v0.1.0); full design and open questions live
in `docs/development/backlog.json` (BL-182).

> **Data-model foundation landed (BL-201, 2026-07-23).** The persisted HQ + influence
> range (Pass 3b above) and the border render-off-real-data are built; the border now
> reads from generation rather than a render-time recompute. What stays deferred in
> BL-182: the **operate-gate** (border restricts where a corp can build), the **national
> origin gate** (corps start home-nation-confined; international reach for 1–2 great
> powers, tech-gated), **branch offices** on non-home bodies, the **cost-field** range
> shape, and the **tall/wide advancement curve**.

**Building tiers / levels.** A level/tier axis for buildings — **distinct from production
methods (recipes)**. A specialist's footprint may be characterised as much by the *tier* of its
assets as by their count, which bears directly on the lean-holdings shape above. Unsettled;
interacts with `docs/economy/PRODUCTION.md`. Cross-doc item — also in `GENERATION_STRATEGY.md`.

**International relations & corporate origin.** Whether **allied nations share corporations**,
or **prefer generated franchises** across borders, is open — it couples nation diplomacy to the
clustering frame (holdings cluster to the home nation today) and to the Franchise item below.
Cross-doc item — also in `GENERATION_STRATEGY.md`.

**Post-WW2 industrial grounding for the asset mix.** The focus→asset-mix patterns should be
grounded in research on the post-WW2 industries that led to space-related capability, so a
specialist's holdings read as a plausible pathway toward off-world reach. Cross-doc item — also
in `GENERATION_STRATEGY.md`.

**Franchise generation.** A franchising model is a candidate alternative origin for some
corporations: rather than generating all corporations as independent entities, some could
be generated as subsidiaries or licensees of larger parent entities. This would produce
a more structured corporate landscape — a dominant parent and several regional franchise
holders — and would interact naturally with the nation system (a parent corp registering
franchises in foreign nations as market entry). This is noted as an open design direction.

**Nation-seeded privatisation.** Nations may generate new corporations during play by
investing in required or promising industries that the private sector has not filled.
A nation with high `extraction` focus and underserved petroleum deposits might seed a
state-adjacent extraction corporation. This is an open behavioural design item that
interacts with both the nation system and the tax/licence layer.

**Tax as an automated layer.** The working model is that taxation is an automated
background layer: corporations pay a percentage of revenue or profit to their home
nation each Tick, without the player needing to manage it directly. How nations spend
that revenue — whether it feeds back into infrastructure, military, or new corporate
investment — is unresolved. Noted here for design continuity.

**Era-based corporate sovereignty.** There is a working hypothesis that later Eras see
corporations become de-facto powers above states. At some point a sufficiently large
or space-capable corporation may renegotiate or shed its legal relationship with its
home nation entirely, or acquire enough leverage to dictate the relationship's terms.
The exact trigger and mechanics are unscoped; noted as a potential arc for the diplomacy
and policy systems.

**Diplomatic posture.** Each corporation will need a sentiment value toward every other
known faction (both corporations and nations). Generation-time sentiment initialisation
from industrial overlap and home nation relationships is a natural fit. Deferred with
faction behaviour.
