#pragma once

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// era_timelapse — the Era -1 ownership replay, as a standalone record
// ---------------------------------------------------------------------------
//
// WHY THIS HEADER EXISTS, and it is a dependency argument rather than a design
// one. The time-lapse is produced by `run_history_sim` (history_sim.hpp) and
// consumed by the History ledger's Ages view through `generation_report`
// (hard_coded_world.hpp). Those two headers must not meet: hard_coded_world.hpp
// says so in its own words, refusing to include history_sim.hpp because "this
// header's several hundred includers should [not] have to pay for" it.
//
// So the shared vocabulary lives here instead, in a header with NO dependencies
// beyond <cstdint> and <vector>. `owner_change` was moved out of history_sim.hpp
// rather than copied — a duplicated wire type is exactly the drift BL-462 was
// written about.
//
// WHAT CHANGED WITH IT (NR-733, Ben's ruling 2026-08-30). The Ages view used to
// RE-RUN the era every time it was opened, which made it a seventh caller of an
// invocation `era_minus_one.hpp` exists to keep singular, and it diverged from
// generation on all six of BL-462's axes. Three of those were closable at the
// call site; the other three were not, because the report carries the settlement
// AFTER the sim mutated it. Replaying a recorded timeline closes all six at once
// by deleting the second caller: generation records what happened, and the view
// shows that, so there is no second invocation left to drift.

/// Reserved for a region leaving all ownership. NOTHING EMITS IT: once settled
/// or conquered a region always has an owner, and no path resets one to unowned.
/// It is kept because `owner_slice_at` needs a value for regions that do not
/// exist yet in an early year, and because depopulation-to-abandonment is a
/// plausible later mechanic — a reader should not infer from the sentinel that
/// abandonment exists (BL-312).
inline constexpr uint16_t owner_none = 0xFFFFu;

/// One ownership change: region `region` came under polity `owner` in year
/// `year`.
///
/// THIS IS THE WHOLE TIME-LAPSE FORMAT. A change LIST rather than a per-year
/// grid, because ownership is overwhelmingly static — most years, on most
/// regions, nothing happens, and a dense grid pays for all of it.
struct owner_change
{
    int32_t  year   = 0;
    uint16_t region = 0;
    uint16_t owner  = 0;
};

/// The recorded Era -1 ownership history of one body: everything the Ages view
/// needs to replay it, and nothing else.
///
/// EMPTY IS MEANINGFUL and is the common case. Generation runs the era for at
/// most ONE body (the cradle), and only when `era_minus_one_enabled` — so every
/// other body carries an empty record, which the view reads as "never settled"
/// rather than as missing data.
struct era_timelapse
{
    std::vector<owner_change> changes;   ///< Ascending by year; the replay substrate.
    int32_t region_stride = 0;           ///< Final region count — the slice width.
    int32_t start_year    = 0;           ///< First simulated year (negative = BCE).
    int32_t years         = 0;           ///< Years simulated, so last = start + years.

    bool empty() const { return changes.empty(); }
};

/// Materialise the ownership map as it stood in `year`.
///
/// Folds every change up to and including `year`; `changes` is in year order, so
/// the walk stops at the first one past it. A region that has not appeared yet
/// reads `owner_none`.
std::vector<uint16_t> owner_slice_at(const era_timelapse& t, int64_t year);
