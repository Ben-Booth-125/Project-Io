# Handoff — the shelf economy and trade (sprint 50, extended)

Written 2026-10-10 by the sprint 50 session for the session that builds this. Read this, then the
four design docs below, then `docs/development/NEXT_SESSION.md` for the rest of sprint 50's state.
Mode: **Delivery — Full** (`docs/development/DELIVERY.md`): it touches the economy, the save format
and the AI seam.

## What Ben asked for (2026-10-10)

> "Remove corporation / company stockpiles, and push the initial goods to markets instead. It
> should simplify the model, and help to resolve the problem with construction at game start."
>
> "For trade, let's add a trade building, give each resource a trade capacity per unit of trade
> points produced, and allow players/AI to make trades — where it is optional to just allow trade
> to auto determine the best routes, or reserve an amount of points for manual trade. This is the
> system used in EU5, and we will tweak it to use our logistics system, especially between
> planets."
>
> "We can generate trade points for nations which have trade (or good relations), and then consume
> these after industrialisation to retrofit where trade was likely to take place."

He ruled, in forms the same day: **all** corporation pools go (production lands on the shelf);
shelf goods are **market-owned** (the maker is paid on landing at the clearing price; everyone buys
from the shelf); the opening stock is **the same total, redistributed** to the markets the
corporations sit in; **trade is the only way goods move between markets**; the **trader** keeps the
margin and pays the haul; capacity is **authored per resource in data**; AI corporations may set
**manual** trades (a new grant); trade points are **separate** from Logistic Points; the building is
the **Planetary Marketplace**; trade runs in **all three generation rounds** (Exploration,
Industrialisation, the 1960 settle) and **replaces the Exploration age's trade flows**; the seat
holds a Marketplace **only if its corporation has one by the end of generation**; the capacity table
is **proposed by measurement and approved by Ben**; and **sprint 50 stays open — one re-bless after
this lands**.

And, in the last forms: the **market is the counterparty** on landing, at the clearing price, bid or
no bid; a trade's purchase **obeys the fair-price ceiling** like any bid; **the order book retires** —
no standing buy or sell orders, everyone buys at the posted price (procurement contracts stay);
**Ports make trade points too**; trade buildings carry an **upkeep of fuel and building materials**
(unmet upkeep, no points); the history's trade-point rates and the retrofit rule are **proposed by
measurement and approved by Ben**.

## The design — read these, they are the authority

| Doc | What it owns here |
|---|---|
| `docs/economy/TRADE.md` | The whole trade system: Marketplace, trade points, capacity, a trade, auto and reserved, trade in generation, the capacity table, what trade replaces. **New doc.** |
| `docs/economy/MARKETS.md` § The shelf economy | Market-owned shelves; landing is selling; everyone buys from the shelf; opening stock on the shelves; **what retires with the pool**. |
| `docs/generation/CORPORATION_GENERATION.md` § Pass 4b | The opening stock goes on the shelves (supersedes "held until a bid"). |
| `docs/ai/AI_OPPONENT.md` § 11, last entry | The grant: a rival may set its own trades. Read the whole register first. |
| `docs/GLOSSARY.md` | Shelf, Planetary Marketplace, trade points, trade capacity, trade; Stockpile retired. |

Sibling docs that still describe pools, auto-surplus, sell orders, the corporation convoy or the
market export — **rewrite each to the new model as its code changes** (the doc rule: a ruling lands
in its owning doc; grep the OLD wording across docs/). Known: MARKETS.md steps 4–5 and 9 (auto-surplus,
sell orders), § Real market inventory; PRODUCTION.md § Output and the shelf, § Launchpad (the
pad's pool reserve); SUPPLY.md (dispatch trigger, the convoy as a pool haul; keep the convoy as the
trade's shipment); FINANCE.md (where a sale's money comes from); CONTRACTS.md (procurement delivery
from a pool); LOGISTICS.md § Logistic Points (passive LP "serves automatic trading" — now serves
trades' shipments); EXPLORATION.md (the trade flows trade replaces); `docs/ai/ACTIONS.json`
(`place_sell_order` and the convoy verbs retire; trade verbs arrive). Run the `ruling-check` skill
at the end.

## What retires, and what each sprint 50 rule becomes

Sprint 50 built several rules on pools. With no pools:

| Sprint 50 rule | Becomes |
|---|---|
| Opening stock held until a bid (D5, `world::opening_stock_held`, save v39) | Gone — stock is on the shelf. Remove the field. |
| The dial reads posted demand + stock-fed running draws (D3, `dial_pool_draw`, v40) | Posted demand alone — every want is posted. Remove the draw register; § 11's narrowed grant collapses to "posted demand". |
| A pad's pool keeps its propellant (`auto_surplus_reservation`) | Gone — a launch buys its propellant from the source shelf. BL-1249 (pad fuel drained) dissolves. |
| An order is a floor, not a hold (BL-1229 steel stays home) | The whole order book retires — buy and sell orders; everyone buys at the posted price. Procurement contracts stay. |
| Auto-surplus, processor reservation, unposted bids from pool draws | Gone. Off-book want (space programme, upkeep, procurement) becomes on-book: they buy from the shelf. |
| Spare supply net of household and background draws (G1b R2, `background_fill`, v43) | Stays — the shelf is still drawn by households and the background. |
| The background pull leaves one tick of processor want (G1b R3) | Stays. |
| Every recipe switch judged on supply (G1b R1) | Stays. |
| The corporation convoy, the market export (BL-1195's lane is the trade shipment's lane) | Retire as verbs; the convoy stays as a trade's shipment, with BL-1195's legs, sweep and leg-time head. |

**The save format moves.** Pools, held stock, the dial draw register, sell orders go; Marketplaces,
trade points state (if any persists — points are a rate), trades and the capacity table arrive. One
bump, sequenced after the current v43 (`tools/session/next_save_version.js --kind world --claim`).

## Suggested lanes

Collision map is a splitting heuristic, not a gate. Lanes in worktrees; the integrating session
merges, builds and gates. Every lane gets a cold `code-reviewer` round — budget a fix round; most
sprint 50 lanes needed two or three.

1. **Shelf economy core** (economy-dev) — BL-1265 (shelf economy). Production lands on the shelf, paid at the
   clearing price; every buyer buys from the shelf; retire pools, auto-surplus, the processor
   reservation, sell orders, the pad reserve, the opening-stock hold, the dial draw register; the
   opening stock to the shelves; save bump. The biggest lane and the base for the rest.
2. **Trade core** (economy-dev) — BL-1266 (trade core). The Planetary Marketplace (data + building type),
   trade points per tick, the capacity table (data), the trade record, auto allocation, manual
   trades; a trade buys, hauls as a convoy under the LP cap, lands and sells; inter-body via a
   Launchpad and shelf propellant; retire the corporation convoy and market export. After lane 1.
3. **AI trades** (economy-dev) — BL-1267 (AI trades). The § 11 grant: the scorer sets, changes and clears
   manual trades and its reserve. After lane 2. Read the register.
4. **Trade in generation** (generation-dev) — BL-1268 (trade in generation). Nations earn trade points in Exploration and
   Industrialisation where they trade or have good relations, replacing the Exploration flows;
   after Industrialisation the record retrofits Marketplaces where trade took place; the settle runs
   trade; the seat holds one only if its corporation has one. Can start beside lane 2 on the
   history side; the retrofit needs lane 2's building.
5. **Trade UI** (ui-dev) — BL-1269 (trade UI). Manual trades and the reserve on a ledger; the Marketplace in the
   Build door; sell-order and pool/stockpile surfaces retire or become shelf readings; ACTIONS.json;
   question_log entries. **Coordinate with sprint 51** (below) — the trade-flow lens (BL-1222) reads
   convoy records.
6. **The capacity table** — BL-1270 (trade capacity table). Propose values, read the market-viability gate on 16 seeds,
   put the table to Ben. Not final until he approves.

Then: the 16-seed market-viability gate (targets in `.claude/skills/market-viability/SKILL.md`:
G1 >= 85% at handoff, G1b <= 5%, G2, G3; G1 at tick 50 reported, deferred to BL-1248), the scripted
visual checks (`scripts/verify/air_gate.lua`, `sea_lane.lua` — sea_lane will need its convoy made by
a trade), `ruling-check`, then **the one re-bless with Ben's go**, the retro, the version cut, and
integration onto main.

## Hazards learned this sprint

- **Two sessions, one branch, wipes work.** Work on your own branch cut from `worktree-sprint-50`
  and merge into it; check `git status` before committing; never `git checkout --`.
- **A sprint 51 session runs in parallel** on `claude/parallel-sprints-collision-check-*`
  (visibility pass BL-1239..1244, lenses, ground rendering) and mints backlog ids at the same time:
  after filing, re-check that branch's backlog for your ids and renumber yours on a hit.
- **Agent worktree isolation sometimes refuses** ("git identity could not be verified"). Retry, or
  hand-make a worktree with `git worktree add` from the sprint worktree. An agent in a hand-made
  worktree cannot commit; copy its files in.
- **Computer use:** grant the running exe by FULL PATH; "ProjectIo" resolves to stale worktrees and
  masks the window.
- **Cold review catches what self-report cannot.** Brief reviewers static-only, not to run harnesses.
- **Measure on the current tip.** A lane's before/after on a stale base misled twice this sprint
  (BL-1252 income; D5/D6 denominator). Read absolute counts (running/built, income), not ratios.
- Plain shell in this repo's sessions: no `$(...)`, no loops, no `bash x.sh`; JSON stores are
  1-space with CRLF — edit with a node script.

## Readings still owed to Ben

None of the design is open. Three tables are proposed by measurement and need his approval before
they are final: the trade capacity per resource, the Marketplace and Port point rates with the
trade-building upkeep (BL-1270, trade capacity table), and the history's trade-point rates with the
retrofit rule (BL-1268, trade in generation). Anything new that no doc answers: file a NEEDS_REVIEW
entry when it arises and keep going (Rule 0c), or a `novel-work` entry if it grows scope.
