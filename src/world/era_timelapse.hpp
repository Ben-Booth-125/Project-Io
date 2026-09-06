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

// ---------------------------------------------------------------------------
// The ancient road record (BL-768)
// ---------------------------------------------------------------------------
//
// THE SECOND ERA -1 RECORD THAT CROSSES INTO GENERATION, and it sits here for
// exactly the dependency argument the top of this file makes about the first:
// it is produced by `run_history_sim` (history_sim.hpp) and consumed by
// `stamp_history_roads` (road_generation.hpp), and those two headers must not
// meet either — the road pass has no business carrying the sim's polity /
// combat / creed vocabulary, and a duplicated wire type is the drift BL-462 was
// written about. So this header is now the Era -1 records that cross into
// generation, plural, rather than the ownership replay alone.
//
// WHY A RECORD AT ALL, RATHER THAN ROADS LAID IN THE SIM. Three structural
// reasons, none of them incidental (GENERATION_STRATEGY.md § The eight phases):
// the sim has no write channel to the world — no `world&`, deliberately; its
// pathfinder is region-to-region over the neighbour graph and returns a COST,
// never a tile list; and `generate_roads`' node source does not exist until
// after the sim has run. There is also an idiom mismatch — the sim's decision
// path is integer fixed-point with no floats in it, while the modern road pass's
// tier gate and redundancy rationing are float. So the sim RECORDS the corridors
// it moved along and a pass immediately after it STAMPS them. What is recorded
// is a fact about the history — an army was supplied along this line, a founding
// party walked it — never a road the sim pretended to build.

/// ONE REGION-TO-REGION CORRIDOR THE HISTORY ACTUALLY USED, and how often.
///
/// PURE OBSERVATION, in the sense `battle_trace` is: nothing in the sim reads a
/// corridor back, no field feeds a decision or a draw, and a run with the record
/// suppressed would be byte-identical in every other output. Unlike the battle
/// traces it is NOT gated on `trace_battles`, because generation — not a
/// harness — is its consumer.
///
/// `a < b` always, and the vector is sorted ascending by (a, b): the stamping
/// pass walks it in that order, so which corridor wins an overlapping tile
/// cannot depend on the order the events happened to be appended in.
struct history_corridor
{
    uint16_t a    = 0; ///< Lower region index.
    uint16_t b    = 0; ///< Higher region index.
    int32_t  uses = 0; ///< Times a polity moved supply or settlers along it.
};

/// Where one region stood, and what its own history invested in MOVING things.
///
/// The flattened form of the two `region` fields `stamp_history_roads` needs,
/// passed as an array rather than as a `settlement_state` for the same reason
/// `sim_terrain_view` is flattened: the road pass has no use for the settlement
/// vocabulary, and a harness can build a synthetic corridor case out of two
/// plain structs instead of out of a whole settled world.
struct history_road_node
{
    int32_t col       = 0;
    int32_t row       = 0;
    /// `region::work_reach_mod` — the accumulated per-mille reach effect of the
    /// works this region raised. Every reach-bearing row of the Era -1 works
    /// roster (Way Station, Span Bridge, Cut Canal and their siblings) adds to
    /// it and nothing else does, so a non-zero value means "this ground built
    /// something for the road" without the road pass naming a single work row.
    int32_t reach_mod = 0;
};
