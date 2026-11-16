#pragma once

#include "components.hpp" // resource_type, resource_count
#include "entity.hpp"
#include "orbital_node.hpp"
#include "space_units.hpp"

#include <array>
#include <cstdint>

struct world;

// ---------------------------------------------------------------------------
// Space convoys (BL-1596) — docs/military/SPACE_FLEETS.md § Space convoys,
// § Raising and launching; docs/economy/SUPPLY.md § A shipment; TRADE.md § A
// trade, "Between bodies".
// ---------------------------------------------------------------------------
// A SPACE CONVOY IS A HAULER. Cargo aboard a space unit is a `space_haul` record
// (keyed by the unit's id): the good, the market it was bought at and, for a
// trade, the convoy riding aboard, the market it unloads to and the node it
// returns to. A unit with cargo > 0 has a record; a record outlives its cargo
// only while a trade's hauler flies home.
//
// THE HAND VERBS. `load_space_cargo` lifts goods off a body's shelf into a carrier
// (Hauler or Lander) idle in that body's low orbit, through its owner's staffed
// Launchpad there, buying the goods and the lift's propellant
// (climb Δv x `launch_propellant_per_dv` x the cargo's mass) off the pad tile's
// market. `unload_space_cargo` sets a carrier's whole cargo down from a low orbit
// through any staffed Launchpad on that body, buying the descent's propellant the
// same way off that pad's market, and the cargo LANDS there: it becomes an arrived
// convoy that `credit_arrived_convoys` lands and sells at that tick's clear, as any
// convoy's cargo is. Both are refused WHOLE — nothing written — on any shortfall,
// and while an armed unit of a nation at war with the owner stands at the node.
//
// THE TRADE. A trade between markets on different bodies, by a NATION holding a
// live Hauler design (`space_haul_eligible`), ships as a Hauler: `quote_space_haul`
// prices it (pure), `launch_space_haul` takes the space side (an idle, empty Hauler
// of the nation in the source's low orbit refuelled over its pad, else one raised
// at that pad, then the cargo's lift propellant), and the supply seam buys the
// cargo and makes the convoy, which rides aboard at speed 0 (it never advances by
// itself) until the Hauler unloads it. The cargo is capped so the laden outbound
// AND the empty return both fit one tank: how far a Hauler can carry is a number.
//
// THE DAY (`run_space_convoy_day`, inside `run_space_unit_day` after arrivals):
// each record, ascending unit id, whose Hauler is idle at a node —
//   1. INTERDICTION: an armed unit of a nation at war with the owner at the node
//      stops it. With an armed unit of its own there (an escort) it OPENS the battle
//      with each such nation that is hostile to it (`open_space_battle`, BL-1599) and
//      waits there until it is decided; with none it is LOST with its cargo: the unit, the record and
//      the riding convoy are erased (battles are SPACE_FLEETS.md § Battles', and an
//      unarmed, unescorted hull has nothing to fight with).
//   2. At the destination's low orbit, outbound: it unloads — the descent's
//      propellant off the destination shelf (waits a day when that cannot be paid,
//      or while the destination is blockaded - BL-1599, space_bombard.hpp)
//      and the convoy is released ARRIVED — then tops up where it may and turns
//      home. At home, returning: the record ends.
//   3. Otherwise it orders the next hop of the cheapest path to where it is going
//      (`order_space_unit`, a legal verb), so every node of the path is a node it
//      stands at. A hop the budget cannot pay ends the trip where it stands.
//
// DETERMINISM: std::map stores walked ascending; nation sets read through the
// ascending unit store. No RNG, no clock.
//
// SAVED: world::space_hauls (world_save.hpp's BL-1596 block). Hashed when non-empty.
// ---------------------------------------------------------------------------

enum class space_haul_phase : uint8_t
{
    held      = 0, ///< Hand-loaded cargo; moves only by order.
    outbound  = 1, ///< A trade's Hauler carrying its convoy to the destination.
    returning = 2, ///< A trade's Hauler, unloaded, flying home empty.
};

/// Cargo aboard one space unit. SERIALISED (BL-1596).
struct space_haul
{
    entity_id        owner       = null_entity;
    space_haul_phase phase       = space_haul_phase::held;
    resource_type    cargo_res   = resource_type::iron_ore;
    entity_id        src_market  = null_entity; ///< Where the cargo was bought.
    uint32_t         convoy      = 0;           ///< Outbound: the riding convoy's id. Otherwise 0.
    entity_id        dest_market = null_entity; ///< Outbound / returning: the trade's destination. Held: null.
    orbital_node     target{};                  ///< Outbound: the destination's low orbit; returning: home's.
    orbital_node     home{};                    ///< Outbound / returning: the source's low orbit.
};

/// The whole-lot space side of one trade's Hauler, priced. Pure.
struct space_haul_quote
{
    bool      viable = false;
    entity_id unit   = null_entity;  ///< An idle Hauler to reuse; null = raise one at the pad.
    entity_id pad    = null_entity;  ///< The nation's staffed pad on the source body.
    entity_id src_market = null_entity;
    entity_id dest_market = null_entity;
    orbital_node from{}, to{};       ///< The two low orbits.
    float     cap = 0.0f;            ///< The most cargo the trip can carry.
    std::array<float, resource_count> fixed{}; ///< The raise lot (kit and tank) or the refuel, off the pad's shelf.
    float     fixed_credits  = 0.0f;
    /// BL-1637 (SPACE_ASSETS.md § Boost): every climb through the pad spends its boost.
    float     fixed_boost    = 0.0f; ///< A raised Hauler's own climb (0 for a reused one).
    float     lift_per_unit  = 0.0f; ///< Boost per unit of cargo lifted (source).
    float     boost_held     = 0.0f; ///< The pad's boost now.
    float     boost_price    = 0.0f; ///< A boost's replacement value on the pad's shelf (the decision's price only).
    float     drop_per_unit  = 0.0f; ///< Propellant per unit of cargo set down - burned from the tank (BL-1637).
    float     days = 0.0f;           ///< Outbound days, hop by hop, plus the day it waits to leave.
    /// What a send of @p q units costs in all: the fixed credits, plus the boost it
    /// spends at its replacement value (the goods the pad bought for it; no credits
    /// change hands at the launch for them). The descent burns the tank, which the
    /// fixed lot filled.
    float cost_for(float q) const
    {
        return fixed_credits + boost_price * (fixed_boost + q * lift_per_unit);
    }
    /// The credits the launch itself debits (the raise or refuel's goods).
    float credits_now() const { return fixed_credits; }
};

/// Does @p nation ship its inter-body trade by Hauler? A nation holding a live
/// Hauler design. The trader must be the nation itself.
bool space_haul_eligible(const world& w, entity_id trader, entity_id nation);

/// Price a Hauler trip from @p src_market's body to @p dest_market's (different
/// bodies) for @p nation. Not viable: no design or key, no staffed pad of the
/// nation on the source body, no staffed pad on the destination body, no path,
/// no crew to raise with, a cap at nothing, or prices that are not finite.
space_haul_quote quote_space_haul(const world& w, const space_unit_params& p, entity_id nation,
                                  entity_id src_market, entity_id dest_market);

/// The space side of a quoted trip, for a cargo of @p send of good @p ri: the
/// Hauler readied (reused and refuelled, or raised: its goods off the pad's shelf and
/// the treasury) and the climbs - the raise's and the cargo's lift - spent off the
/// pad's boost (BL-1637). Refused WHOLE (null, nothing written) when the shelf cannot
/// fill the lot, the cargo with it, the pad the boost, or the treasury the credits;
/// @p out_credits is what it spent. The cargo itself is the
/// caller's to buy; `attach_space_haul` then puts it aboard.
entity_id launch_space_haul(world& w, const space_unit_params& p, const space_haul_quote& q,
                            entity_id nation, std::size_t ri, float send, float* out_credits);

/// Put @p send of good @p ri aboard @p unit as trade convoy @p convoy_id: the
/// record, outbound. Called once `launch_space_haul` and the cargo purchase are done.
void attach_space_haul(world& w, const space_haul_quote& q, entity_id unit, entity_id nation,
                       std::size_t ri, float send, uint32_t convoy_id);

/// THE HAND VERBS (see the header).
space_unit_op load_space_cargo(world& w, const space_unit_params& p, entity_id nation, entity_id unit,
                               std::size_t ri, float qty);
space_unit_op unload_space_cargo(world& w, const space_unit_params& p, entity_id nation, entity_id unit);

/// Is an armed unit of a nation at war with @p nation at @p node (not in transit)?
bool space_node_hostile(const world& w, entity_id nation, orbital_node node);

/// The convoy day (`space_convoy_day_report` is space_units.hpp's, carried on the
/// unit day's report).
void run_space_convoy_day(world& w, const space_unit_params& p, const station_params& stp,
                          space_convoy_day_report* out = nullptr);

/// Drop the records of units no longer in the store, and their riding convoys.
void prune_space_hauls(world& w);

/// Ceilings the save reader holds the stream to.
inline constexpr std::size_t k_space_hauls_max = 1u << 20;
