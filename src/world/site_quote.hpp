#pragma once

// ---------------------------------------------------------------------------
// site_quote - BL-1536 (quote obtainable). What the Build door and the site finder
// tell the player a building would earn on a tile.
// ---------------------------------------------------------------------------
// `estimate_prospective_profit` alone prices inputs at the posted price with a full
// run, and output at the posted price however much it would add. On a scarce good
// that price is high exactly because nobody is making it, so the figure was a mirage
// (build_payback_census, 2026-11-05: alloys quoted 4,669, earned 615; electronics
// quoted 2,332, earned -6).
//
// This asks the two questions the player would ask:
//   * can the plant get its inputs here, at what cost, and without its own bid taking
//     the price over what a plant pays (`input_obtainable`, the reservation), and
//   * what does the output sell for once this plant adds to the market's supply
//     (`price_target`, the law clearing itself aims at).
//
// Read-only: it writes no world state and the scorer does not call it, so no digest
// moves. `world&` is non-const only because `input_reach`'s memoised haul asks for it.
// ---------------------------------------------------------------------------

#include "building_profit.hpp"
#include "corp_ai.hpp"
#include "input_reach.hpp"

#include <cstdint>

class recipe_registry;
struct economy_report;

/// Built once per evaluation: the producer index and the plants under construction.
struct site_quote_context
{
    input_reach        reach;
    pending_output_map pending;
};
/// @p report is the last quarter's: a producer that made nothing supplies nothing.
site_quote_context make_site_quote_context(world& w, const recipe_registry& reg, const economy_report* report);

struct site_quote
{
    building_profit profit;                 ///< The figure to show. Upkeep alone when starved.
    bool            starved = false;        ///< An input is not obtainable, so it would not run.
    resource_type   short_of = resource_type::iron_ore; ///< The first input it cannot get, when starved.
    float           price_share = 1.0f;     ///< The output's forecast price over its posted price.
};

/// The quote for a hypothetical building of @p type on @p tile, at the tile's
/// Logistic Point cover @p lp_cover. @p self names a building already ordered there, for
/// a harness asking what its site was worth: its own draw and output are taken out.
site_quote quote_site(world& w, const recipe_registry& reg, site_quote_context& cx,
                      entity_id tile, building_type type, resource_type target,
                      std::uint16_t recipe_id, float lp_cover, entity_id self = null_entity);
