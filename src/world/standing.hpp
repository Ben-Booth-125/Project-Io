// Corp standing profile (BL-262, re-specified by BL-633) — an exact, disclosure-honest read of
// how each corporation compares. Three axes: reach (distinct bodies with a building), capital
// (balance), market share (this corp's clearing income over the total).
//
// THE FIVE BANDS ARE RETIRED (Ben, 2026-08-26: "We don't need company information to be
// invisible"). Every axis is an exact figure; what varies between corporations is not how coarse
// the number is but WHETHER THERE IS ONE AT ALL:
//   - Reach and market share are PUBLIC for everyone. Both derive from facts already observable —
//     buildings are visible on-canvas, market supply/demand aggregates are the deliberate public
//     signal (docs/ui/DISCOVERY.md § Competitor visibility). Nothing was ever hidden about them
//     except by the banding, so nothing is lost by printing them.
//   - Capital is a FILED figure and follows the ownership class. Exact for a `public` corporation,
//     absent for a `private` or `closed` one — the same binary disclosure the quarterly return
//     takes (docs/economy/FINANCE.md § Disclosure). A dash means *this firm does not file*, never
//     *you have not earned this*.
//
// THE OPERATIONAL FOG IS UNCHANGED (Ben, 2026-08-26). This was a ruling about published FINANCIAL
// information only. Production rates, stockpile quantities, assigned recipes and workforce dials
// all stay private, and the rival hover card still shows type and owner only. The line, in one
// sentence: **an open book tells you what a firm earned, never how it operates.** No field may be
// added to `corp_standing` that an accounting statement would not carry.
//
// Deliberately NOT unified with corp_ai's utility scorer (src/world/corp_ai.hpp) — see BL-262
// Call 5: the AI optimises an internal ground-truth quantity, this is a published read; unifying
// them would make every AI directly optimise the published number (a Goodhart trap). Retiring the
// bands does not touch that property.
//
// Production (physical output) is intentionally NOT one of the axes. A rival's true output needs
// their assigned recipe and workforce dial, and neither is a filed figure — there is no honest
// source for it, so the axis is left out rather than faked.

#pragma once

#include "components.hpp"
#include "market_clearing.hpp"
#include "world.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

/// The disclosure gate, in one predicate: a corporation files a readable return iff its ownership
/// class is `publicly_held`. Binary — there is no graded middle (FINANCE.md § Disclosure).
bool corp_files_return(ownership_class oc);

struct corp_standing
{
    entity_id corp      = null_entity;
    bool      is_player = false;

    // Exact figures — always computed, always printable for their axis.
    int   reach_bodies    = 0;    ///< Count of distinct bodies with >=1 owned building. PUBLIC for every corp.
    float capital_balance = 0.0f; ///< corporation_component.balance. Readable only where `capital_disclosed`.
    float market_share    = 0.0f; ///< This corp's clearing income / total clearing income this tick, in [0,1]; 0 if total is 0. PUBLIC for every corp.

    /// Whether `capital_balance` may be shown. `corp_files_return(ownership_class)` — the firm's
    /// own filing status, not a fact about the reader. False => the UI prints a dash.
    bool capital_disclosed = false;
};

/// Computes the standing profile for every corporation in `w`, in a SORTED walk by entity_id
/// (never iterate w.corporations directly — it's an unordered_map; sort the id list first, same
/// pattern src/ui/corporation_panel.cpp already uses). `cash_flow` is this tick's per-corp
/// result from clear_markets (src/world/market_clearing.hpp's corp_cash_flow) — pass an empty
/// map if none is available yet (market_share resolves to 0 for everyone in that case, not a
/// crash).
std::vector<corp_standing> compute_corp_standings(
    const world& w,
    const std::unordered_map<entity_id, corp_cash_flow>& cash_flow);
