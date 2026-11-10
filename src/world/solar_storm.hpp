#pragma once

#include "entity.hpp"
#include "orbital_node.hpp"
#include "space_assets.hpp"
#include "world.hpp"

#include <cstdint>
#include <vector>

struct station_params;

// ---------------------------------------------------------------------------
// Solar storms (BL-1605) — the environmental family's space event
// ---------------------------------------------------------------------------
// docs/EVENTS.md § Four families (Solar storms). A storm is scheduled from the
// world seed and the econ tick, so it is seeded and replayable. It strikes every
// station and satellite OUTSIDE A PLANET'S SHELTER — at a Lagrange point, or at
// any node of an asteroid — and stops their modules and roles for one quarter.
// A station Observatory sees a storm a quarter ahead, and its owner is told.
//
// DERIVED, NEVER SAVED. Whether a quarter carries a storm is a pure function of
// (world seed, econ tick); whether an asset is struck is a pure function of its
// node and that. Nothing about a storm is stored, so nothing is added to the
// save or to state_hash, and a world with nothing in space is not touched at all.
// The seed is `w.provinces.seed` — the one world seed the world stores (the
// campaign battle stream reads the same one, battle_system.cpp).
//
// THE SCHEDULE. Time is cut into SOLAR CYCLES of `cycle_quarters` quarters
// (EVENTS.md § Rhythm: "one every few years, so a decade has a shape"). A cycle
// carries at most one storm: a draw from the stream folded from (seed, cycle)
// decides whether it carries one (`storm_per_mille`) and in which of its quarters
// it falls. One draw per cycle, so storms never bunch and a decade holds three or
// so. Quarters before tick 1 (the campaign settle's) never carry one.
//
// SHELTER. A planet's or a moon's low and high orbit are sheltered (a moon rides
// inside its planet's shelter). Every Lagrange point is exposed, and so is every
// orbital node of an asteroid. Exposure is a yes or a no: the doc names no
// strength between, and no shielding module exists.
//
// WHAT A STORM DOES, for its one quarter (the readers are the existing ones):
//   * a satellite at an exposed node is DARK: it counts as not active for every
//     role read — coverage, navigation, observation, the activity fog, survey
//     (`solar_storm_satellite_dark`);
//   * a station at an exposed node has every module STOPPED: its budget reports
//     them `stormed`, none working, so research, factories, relays, observatories,
//     docks and depots all stop (`evaluate_station_budget`).
// Nothing is worn and nothing is lost: the next quarter works as before. Upkeep
// is still paid. Astronauts are not touched (the doc names no exposure for them).
//
// HEADER-ONLY for the reads, because world.cpp links alone in several harness
// targets and its activity-fog twin must darken a Comms satellite too. The report
// (`run_solar_storm_report`) is solar_storm.cpp.
// ---------------------------------------------------------------------------

/// The dials. First-cut, in-band (EVENTS.md § Rhythm); not yet a Lua key.
struct solar_storm_params
{
    /// Quarters in one solar cycle; a cycle carries at most one storm. 12 = three years.
    int cycle_quarters = 12;
    /// Chance, in thousandths, that a cycle carries its storm.
    int storm_per_mille = 750;
};

namespace solar_storm_detail {
inline uint64_t mix(uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}
inline constexpr uint64_t k_salt = 0x534F4C4152535432ull; // the storm stream's own salt
} // namespace solar_storm_detail

/// PURE. Does econ tick @p tick carry a storm, for world seed @p seed?
inline bool solar_storm_at(uint32_t seed, int tick, const solar_storm_params& p = {})
{
    if (tick < 1 || p.cycle_quarters < 1 || p.storm_per_mille <= 0)
        return false;
    const int cycle  = (tick - 1) / p.cycle_quarters;
    const int within = (tick - 1) % p.cycle_quarters;
    const uint64_t h = solar_storm_detail::mix(
        solar_storm_detail::mix(static_cast<uint64_t>(seed) ^ solar_storm_detail::k_salt)
        + static_cast<uint64_t>(cycle));
    if (static_cast<int>(h % 1000u) >= p.storm_per_mille)
        return false;
    return static_cast<int>((h >> 20) % static_cast<uint64_t>(p.cycle_quarters)) == within;
}

/// The world's storm at @p tick (its stored seed).
inline bool solar_storm_at(const world& w, int tick, const solar_storm_params& p = {})
{
    return solar_storm_at(w.provinces.seed, tick, p);
}

/// PURE. Is @p n outside a planet's shelter — a Lagrange point, or any orbital
/// node of an asteroid? The surface is never an orbital asset's node.
inline bool solar_storm_node_exposed(const world& w, orbital_node n)
{
    if (!orbital_node_is_orbital(n.kind))
        return false;
    if (orbital_node_is_lagrange(n.kind))
        return true;
    const auto it = w.bodies.find(n.body);
    return it != w.bodies.end() && it->second.type == body_type::asteroid;
}

/// Does a storm strike node @p n at econ tick @p tick? Cheap when no storm.
inline bool solar_storm_strikes(const world& w, orbital_node n, int tick,
                                const solar_storm_params& p = {})
{
    return solar_storm_node_exposed(w, n) && solar_storm_at(w, tick, p);
}

/// Is satellite @p a dark this quarter — an active satellite at a struck node?
/// Every role read treats a dark satellite as not active.
inline bool solar_storm_satellite_dark(const world& w, const space_asset& a, int tick,
                                       const solar_storm_params& p = {})
{
    return a.cls == space_asset_class::satellite && a.state == space_asset_state::active
        && solar_storm_strikes(w, a.node, tick, p);
}

/// One nation's storm quarter — reported, never stored. Ascending nation id.
struct solar_storm_nation_row
{
    entity_id nation          = null_entity;
    int       satellites_dark = 0; ///< Its active satellites struck this quarter.
    int       stations_struck = 0; ///< Its stations (core in place) struck this quarter.
    int       modules_stopped = 0; ///< Modules the storm stopped on them.
    bool      warned          = false; ///< A working Observatory of its sees next quarter's storm.
};

/// What the storm read saw this quarter. Empty (and `storm` false) in a world
/// with nothing in space.
struct solar_storm_report
{
    bool storm      = false; ///< This quarter carries a storm.
    bool storm_next = false; ///< Next quarter carries one (what an Observatory sees).
    std::vector<solar_storm_nation_row> nations; ///< Only nations struck or warned.
};

/// THE READ. Once per econ tick, after the space-effects step: who the storm
/// struck this quarter and who an Observatory warned of the next. Reads only;
/// does nothing in a world with no space assets and no stations.
void run_solar_storm_report(const world& w, const station_params& stp, int econ_tick,
                            solar_storm_report* out, const solar_storm_params& p = {});
