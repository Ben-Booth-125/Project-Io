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
   reads it like any other bid (`MARKETS.md`).
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

Each owner chooses, per Marketplace or for all of them, how much of its trade points to
**reserve for manual trades**; the rest is **auto**.

- **Manual trades** are the routes the owner sets, each with its points.
- **Auto** assigns the unreserved points each tick to the routes with the best margin per point
  — destination price less source price less haul, per unit, times the resource's capacity —
  among the markets the owner's Marketplaces reach, best first, until the points run out or no
  route earns. Deterministic: candidates in a fixed order, ties broken by market and resource id.

**Who may trade (Ben, 2026-10-10).** The player and every AI corporation. An AI corporation may
set **manual** trades as well as run auto — a grant in `../ai/AI_OPPONENT.md` § 11 — under the
grant constraints every AI decision is bound by.

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
