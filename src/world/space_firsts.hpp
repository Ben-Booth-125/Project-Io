#pragma once

#include "components.hpp" // entity_id, null_entity

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>

struct world;
struct research_params;

// ---------------------------------------------------------------------------
// Space firsts (BL-1606; ERAS.md § Firsts)
// ---------------------------------------------------------------------------
// The first nation to do each space deed earns a one-time research grant and a
// public history-log entry; a first is never taken twice.
//
// A DEED IS READ FROM STATE (ERAS.md: "a deed read from state the game already
// carries"). `run_space_firsts` reads the space stores and, for every first not
// yet taken, finds the nations that meet it NOW. It runs at the end of each space
// day and after the quarter's space steps (nation_step.cpp step 7g), so a deed
// lasting a day is seen the day it happens:
//
//   satellite     a satellite asset in place (active or decaying)
//   crew_in_orbit a crewed asset in place at an orbital node (a crewed mission
//                 to an orbit, a station core or module with crew aboard), or a
//                 space unit with crew aboard at an orbital node, not in transit
//   station_core  a station core asset in place
//   crew_on_moon  a crewed mission in place on a moon's surface, or a space unit
//                 with crew aboard landed on a moon
//   offworld_base an off-world base whose tile the nation holds
//   space_convoy  a Hauler that has delivered a trade's cargo to a market on
//                 another body (its haul RETURNING from a destination market)
//
// TIES: two nations meeting one first in the same pass (the same day) — the
// lowest nation id takes it (ERAS.md § Firsts). A deed met later is not a first.
//
// THE GRANT goes into the nation's research pool (`world::nation_tech`), in
// milli-RP — the path the quarter's accrual and a station's Research module take.
// THE LOG writes one `history_topic::space_first` entry, `corp` = the nation:
// the world history log is the public record every nation reads.
//
// DETERMINISM. Every store read is either an ordered map or reduced to its
// minimum nation id, so the unordered nation store's order never matters. No RNG,
// no clock.
//
// SAVED: world::space_firsts — the BL-1606 trailing section (world_save.hpp).
// Folded into state_hash when non-empty.
// ---------------------------------------------------------------------------

/// The deeds. Append-only: the value is written to the save.
enum class space_first_kind : uint8_t
{
    satellite     = 0,
    crew_in_orbit = 1,
    station_core  = 2,
    crew_on_moon  = 3,
    offworld_base = 4,
    space_convoy  = 5,
};
inline constexpr std::size_t space_first_kind_count = 6;

/// Who took a first, and when. SERIALISED (BL-1606).
struct space_first_record
{
    entity_id nation   = null_entity; ///< The nation that took it.
    int32_t   day_tick = 0;           ///< `world::current_day_tick` when it was taken.

    bool operator==(const space_first_record&) const = default;
};

/// What one pass took (reported, not stored).
struct space_firsts_report
{
    int          taken       = 0;  ///< Firsts taken this pass.
    std::int64_t grant_milli = 0;  ///< Milli-RP granted in all.
    std::array<entity_id, space_first_kind_count> by{}; ///< The taker per kind this pass; null if none.
};

/// The deed's short name for the log ("the first satellite in orbit").
const char* space_first_label(space_first_kind k);

/// The lowest nation id meeting deed @p k in @p w right now, or null.
entity_id space_first_candidate(const world& w, space_first_kind k);

/// The pass (see the header). Each first not yet taken whose deed some nation
/// meets is taken by the lowest such nation id: recorded, granted
/// `p.space_first_grant_rp`, logged. Taken firsts are never re-read.
void run_space_firsts(world& w, const research_params& p, space_firsts_report* out = nullptr);
