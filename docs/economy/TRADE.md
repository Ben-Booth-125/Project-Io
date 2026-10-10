# Project Io — Trade

> **Settles:** how goods move from one market to another · the Planetary Marketplace and the
> trade points it makes · trade capacity per resource · what a trade is, who sets it, and what it
> earns · auto and reserved trade.
> **Not here:** the shelf a trade buys from and sells onto, and its prices (MARKETS) · the
> network a shipment travels — traversal cost, travel time, Logistic Points, interdiction
> (LOGISTICS) · the shipment in transit — the convoy, its lane and its arrival (SUPPLY).
> **Confused with:** SUPPLY.md, LOGISTICS.md, MARKETS.md.

**Trade is the only way goods move between markets (Ben, 2026-10-10).** A market's shelf holds
what is made, bought and sold there (`MARKETS.md` § The shelf economy); nothing crosses to
another market except by a trade. The shape is borrowed from a reference grand strategy game's
trade capacity — a building makes points, each resource moves so many units per point — and
moved onto Io's own network: every shipment is priced, timed and capped by `LOGISTICS.md`, and
travels as a convoy (`SUPPLY.md`).

---

## The Planetary Marketplace

A **Planetary Marketplace** is a building a corporation owns. It makes **trade points** for its
owner each tick, at its staffed rate, as a processor makes goods. Trade points are a **rate**,
not a stock: made and spent within the tick, never banked — the Logistic Points rule
(`LOGISTICS.md` § Logistic Points), applied to an owner's capacity.

**Ports make trade points too (Ben, 2026-10-10).** A Port, besides its sea-lane role, makes trade
points for its owner each tick, as a Marketplace does at a lower rate the data sets. A Marketplace
makes its points at its staffed rate; a Port, which staffs at zero, makes its flat rate.

**Points belong to the market the building stands in (Ben, 2026-10-10).** Each trade building
makes its points for the **market whose catchment holds it**, and those points move goods
*leaving that market*. An owner trades from the markets where it holds a trade building, and from
nowhere else.

**Trade buildings have upkeep (Ben, 2026-10-10).** A Planetary Marketplace and a Port each draw an
upkeep of **fuel and building materials** every tick, bought from their own market's shelf like
any other upkeep (`PRODUCTION.md`); the goods and amounts are authored in data. A trade building
whose upkeep goes unmet makes no trade points that tick.

**Trade points are separate from Logistic Points (Ben, 2026-10-10).** Trade points are how much
an *owner* can move; Logistic Points are how much can move *through a place*. A shipment spends
its owner's trade points and still passes the network's Logistic Point cap at the nodes it
crosses. Neither replaces the other.

## Trade capacity

Each resource has a **trade capacity**: the units of it one trade point moves per tick, authored
per resource in data (Ben, 2026-10-10). A point moves many units of a bulk raw and few of a
dense product, as the author sets it; there is no formula behind the number.

## A trade

A **trade** is a standing route set by an owner: *move resource R from market A to market B,
with P trade points*. Each tick it ships up to `P × capacity(R)` units:

1. **It buys** at A's shelf, at A's posted price, as any buyer does — the fair-price ceiling
   reads it like any other bid (Ben, 2026-10-10; `MARKETS.md`).
2. **It pays the haul**, priced by the network from A to B (`LOGISTICS.md` § 1), and the goods
   travel as a convoy for the leg's travel time (`SUPPLY.md`), seen and interdicted along their
   lane.
3. **It sells** on landing at B, onto B's shelf, at B's clearing price — landing is selling, as
   it is for production (`MARKETS.md` § The shelf economy).

**The trader keeps the margin and pays the haul (Ben, 2026-10-10)**: B's price on landing, less
A's price, less the haul. A trade that loses money still runs until its owner changes it; auto
trade (below) does not choose one.

**Between bodies** a trade rides the space lane: the trader must hold a Launchpad on the source
body, and each launch burns propellant the trader buys from that body's shelf
(`PRODUCTION.md` § Launchpad). The network prices and times the lane as it does any leg.

## Auto and reserved trade

Each owner chooses **one reserve** — how much of its trade points to **reserve for manual
trades** (Ben, 2026-10-10); the rest is **auto**.

**Reach runs from market centre to market centre (Ben, 2026-10-10).** A trade leaves a market
where its owner holds a trade building and may go to any market a leg reaches from that market's
centre: on the same body, overland or through Ports; to another body, by the owner's Launchpad
and propellant (`LOGISTICS.md`, `SUPPLY.md`). The same reach binds manual and auto alike.

- **Manual trades** are the routes the owner sets, each with its points, spent out of the points
  of the market it leaves, in the order they were set, up to the reserve.
- **Auto** assigns the unreserved points each tick to the routes with the best margin per point
  — destination price less source price less haul, per unit, times the resource's capacity —
  each market's points on routes leaving it, best first, until the points run out or no route
  earns more than the network's dispatch margin of its source price. Reserved points a manual
  trade did not spend stay reserved: they are held back from every market in proportion to what
  it has left. Deterministic: candidates in a fixed order, ties broken by market and resource id.

**Who may trade (Ben, 2026-10-10).** The player and every AI corporation. An AI corporation may
set **manual** trades as well as run auto, and may **build a Planetary Marketplace** — two grants
in `../ai/AI_OPPONENT.md` § 11 — under the grant constraints every AI decision is bound by.

## Trade in generation

**Trade runs through every round of the history, and its record places the Marketplaces the
campaign opens with (Ben, 2026-10-10).**

- **The Exploration age (1200–1660) and the Industrialisation span (1660–1960):** a nation earns
  **trade points** where it trades, or where its relations with another are good. Those points
  move goods between the markets of the nations that hold them, by the same capacity and the same
  network, and they replace the Exploration age's trade flows (`../generation/EXPLORATION.md`).
  The history's points are a nation's, not a corporation's: no corporation exists before the
  charter walk.
- **After Industrialisation, the record is spent.** The trade points a history accumulated are
  consumed to **retrofit Planetary Marketplaces** where trade most likely took place: on the
  markets whose trade the history carried, owned by the corporations chartered there. A route
  the history never traded gets no Marketplace for its sake.
- **How the record is read (approved by Ben, 2026-10-10).** Each round, a nation's trade flows
  credit their volume to both ends, each at its own capital, and each standing mutual treaty
  credits its years to both sides' capitals (tribute does not count). At the end of generation
  the record converts to points by authored rates (`world_gen.trade_retrofit` in data) and each
  market whose history traded buys up to a capped count of Marketplaces at an authored price in
  points. **Owner:** the corporation, other than the seat, with the most buildings in that
  market's catchment (ties to the lower id); a market where no corporation holds ground gets
  none. **Site:** the free tile of the catchment nearest the market's centre that placement
  allows. A retrofitted Marketplace is built and staffed when the campaign opens.
- **The 1960 settle** runs trade as play does, on the corporations' Marketplaces.
- **The seat holds a Marketplace only if its corporation has one by the end of generation**
  (Ben, 2026-10-10); the player builds the first otherwise.

## The capacity table

Each resource's trade capacity is authored in data. The first table is proposed by measurement:
values set, read on the market-viability gate across the curated seeds, and put to Ben for
approval (Ben, 2026-10-10). No value is final until he has approved it.

## What trade replaces

The corporation convoy (a pool hauling its own surplus), the market export (a market shipping
its shelf to a richer one), and sell orders all retire with corporation pools
(`MARKETS.md` § The shelf economy). Convoys remain as the **shipment** of a trade: the cargo,
lane, interdiction and arrival rules in `SUPPLY.md` hold for a trade's goods in transit.
