// Corp standing profile (BL-262 first slice) — a coarse, visibility-honest read of how each
// corporation compares. The player's own figures are exact; every other corporation is shown
// only as a band, never a number (docs/ui/DISCOVERY.md's competitor-visibility rule, BL-068).
// Deliberately NOT unified with corp_ai's utility scorer (src/world/corp_ai.hpp) — see BL-262
// Call 5: the AI optimises an internal ground-truth quantity, this is a public coarse read;
// unifying them would make every AI directly optimise the published number (a Goodhart trap).
//
// Production (physical output) is intentionally NOT one of the axes here, though BL-262's full
// design calls for four (reach/production/capital/market-share). A rival's true output needs
// their assigned recipe and workforce dial, both private under BL-068 — there is no honest
// visible-information source for it today. Left for a follow-on once one exists.

#pragma once

#include "components.hpp"
#include "market_clearing.hpp"
#include "world.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

enum class standing_band : uint8_t
{
    negligible = 0,
    minor      = 1,
    notable    = 2,
    major      = 3,
    dominant   = 4,
};

/// "Negligible"/"Minor"/"Notable"/"Major"/"Dominant".
const char* standing_band_label(standing_band b);

struct corp_standing
{
    entity_id corp      = null_entity;
    bool      is_player = false;

    // Exact figures — always computed; the UI decides whether to show them (only for is_player).
    int   reach_bodies    = 0;    ///< Count of distinct bodies with >=1 owned building.
    float capital_balance = 0.0f; ///< corporation_component.balance.
    float market_share    = 0.0f; ///< This corp's clearing income / total clearing income this tick, in [0,1]; 0 if total is 0.

    // Bands — always computed from the exact figures above via fixed, documented boundaries.
    standing_band reach_band   = standing_band::negligible;
    standing_band capital_band = standing_band::negligible;
    standing_band share_band   = standing_band::negligible;
};

/// Boundary rule (all three banding functions): a value exactly ON a boundary lands in the
/// HIGHER band (>=, not >). Deterministic — no floating-point ambiguity.

/// 0->negligible, 1->minor, 2->notable, 3->major, 4+->dominant.
standing_band band_reach(int bodies);

/// <0 negligible; [0,5000) minor; [5000,20000) notable; [20000,50000) major; >=50000 dominant.
standing_band band_capital(float balance);

/// <0.05 negligible; [0.05,0.15) minor; [0.15,0.30) notable; [0.30,0.50) major; >=0.50 dominant.
standing_band band_market_share(float share01);

/// Computes the standing profile for every corporation in `w`, in a SORTED walk by entity_id
/// (never iterate w.corporations directly — it's an unordered_map; sort the id list first, same
/// pattern src/ui/corporation_panel.cpp already uses). `cash_flow` is this tick's per-corp
/// result from clear_markets (src/world/market_clearing.hpp's corp_cash_flow) — pass an empty
/// map if none is available yet (market_share resolves to 0 for everyone in that case, not a
/// crash).
std::vector<corp_standing> compute_corp_standings(
    const world& w,
    const std::unordered_map<entity_id, corp_cash_flow>& cash_flow);
