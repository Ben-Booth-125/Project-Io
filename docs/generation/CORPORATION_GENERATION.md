# Project Io — Corporation Generation

> **⟳ Pending rework (2026-06-15) — transient.** Reconciled with code landed in the >C Brief
> pass: **Pass 3** rewritten to record the clustered, focus-shaped 3–6-holding placement
> (anchor + nearest-tile fill, `placement_rules`-gated) that replaced the single vague
> placement; **Pass 4** gained the pre-game operating-history note. **This describes the
> current code, but the holdings *shape* was flagged wrong on review (2026-06-15) and is to be
> revised** — see TODO § Environment → Corporation generation [B4] and § Documentation [S1].
> Keep this note until the revision lands.

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

Each corporation receives a **clustered, focus-shaped set of holdings** placed on tiles
within their home nation's territory:

- **Count.** A corporation opens with **3–6 buildings** (`k_min_holdings` / `k_max_holdings`),
  not a single asset.
- **Anchor, then cluster.** Placement chooses a **focus-weighted anchor tile** first, then
  fills the remaining slots from the home nation's tiles **nearest that anchor** (squared grid
  distance, tile-id tie-break), so a corporation's holdings form a **geographic cluster**
  rather than scattering across the nation.
- **Mix follows focus.** The asset mix is shaped by `industrial_focus` (`focus_asset_pattern`)
  — an extraction corp clusters extractors on its richest deposits, a processing corp pairs
  processors with feed, etc.
- **Validity-gated.** Every placement is gated by `placement_rules::can_place` (the shared
  placement seam), so no asset lands on invalid terrain or a zero-deposit tile. The
  `world_audit` harness confirms **0 invalid placements** across the generated set.

Placement is collision-checked against already-placed assets from other corporations.
No two corporations begin on the same tile.

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
