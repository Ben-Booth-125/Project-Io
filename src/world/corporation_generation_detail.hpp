#pragma once

// ---------------------------------------------------------------------------
// corporation_generation_detail.hpp — the Pass 3 helpers industry generation
// shares with the major opening kit (BL-1473, opening_kit.cpp).
// ---------------------------------------------------------------------------
// CORPORATION_GENERATION.md § A major's opening kit: "Every kit placement
// passes § Pass 3's rules, unchanged." So the kit reads the SAME occupancy, the
// same cover at placement, the same dig ladder, the same chain-feasible and
// sized-to-spare tests and the same want test the charter walk reads — these,
// defined in corporation_generation.cpp, not a second copy. Generation-internal:
// nothing outside src/world/ includes this.

#include "components.hpp"
#include "placement_rules.hpp"
#include "recipe_registry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

struct world;
struct input_reach;

namespace corpgen
{

/// GENERATION'S OCCUPANCY, BY STACK (Ben, 2026-10-26; CORPORATION_GENERATION.md
/// § Pass 3, "Buildings stack on every tile"): a charter may place buildings on
/// any tile up to that tile's stack cap. Each tile carries two counts, read the
/// way placement_rules reads a stack: extraction sites per target, against the
/// deposit's richness-bound `stack_capacity`, and every other building together,
/// against `non_extraction_stack_cap`. `count` keeps the old reading (does the
/// tile hold any building at all) for the passes that are not a charter's — the
/// muster base and the Marketplace retrofit keep one building per empty tile.
class gen_occupancy
{
public:
    void reserve(std::size_t n) { m_tiles.reserve(n); }

    void add(const building_component& b)
    {
        tile_stack& s = m_tiles[b.tile];
        ++s.total;
        if (b.type == building_type::extraction_site)
            ++s.extraction[static_cast<std::size_t>(b.target_resource)];
        else
            ++s.non_extraction;
    }

    void remove(const building_component& b)
    {
        const auto it = m_tiles.find(b.tile);
        if (it == m_tiles.end())
            return;
        tile_stack& s = it->second;
        if (b.type == building_type::extraction_site)
        {
            auto& e = s.extraction[static_cast<std::size_t>(b.target_resource)];
            if (e > 0) --e;
        }
        else if (s.non_extraction > 0)
            --s.non_extraction;
        if (--s.total <= 0)
            m_tiles.erase(it);
    }

    /// 1 when the tile holds any building, else 0 (the pre-stacking reading).
    std::size_t count(entity_id tile) const { return m_tiles.count(tile); }

    /// Extraction sites of @p target standing on @p tile.
    int extraction_on(entity_id tile, resource_type target) const
    {
        const auto it = m_tiles.find(tile);
        return it == m_tiles.end() ? 0 : it->second.extraction[static_cast<std::size_t>(target)];
    }

    /// May one more building of @p type (extracting @p target) stand on @p tile?
    /// Validity (terrain, deposit) is `placement_rules::can_place`'s; this is the
    /// stack's room alone.
    bool has_room(const tile_component& tc, entity_id tile, building_type type,
                  resource_type target) const
    {
        const auto it = m_tiles.find(tile);
        if (it == m_tiles.end())
            return true;
        const tile_stack& s = it->second;
        if (type == building_type::extraction_site)
            return s.extraction[static_cast<std::size_t>(target)]
                 < placement_rules::stack_capacity(tc, type, target);
        return s.non_extraction < placement_rules::non_extraction_stack_cap(tc.substrate, tc.cover);
    }

private:
    struct tile_stack
    {
        int total = 0;
        int non_extraction = 0;
        std::array<int, resource_count> extraction{};
    };
    std::unordered_map<entity_id, tile_stack> m_tiles;
};

/// Four-cardinal network walk over @p body from @p seeds, as the Logistic Point
/// road field walks (see the definition's header block). With @p track_nation
/// set, a step across an unroaded edge between two of that nation's dry tiles
/// costs a Track's. Bounded at @p bound (<= 0: unbounded).
std::unordered_map<entity_id, float> gen_road_walk(world& w, entity_id body,
                                                   std::vector<entity_id> seeds,
                                                   entity_id track_nation, float bound);

/// Drop @p bid from whichever nation lists it in `state_assets`.
void release_from_owner(world& w, entity_id bid);

/// Author a building of @p btype onto @p tid (staffed as generation staffs it),
/// with @p target as its extraction target (null: the tile's richest). Adds it
/// to @p occupied_tiles. The caller has already passed the placement rules.
entity_id author_building(world& w, entity_id tid, building_type btype,
                          gen_occupancy& occupied_tiles, const resource_type* target = nullptr);

/// How a tile can host an extraction site digging a named good (BL-1197).
enum class dig_site { none, well, wharf, deposit };
dig_site dig_site_of(const world& w, entity_id tid, const tile_component& tc, resource_type dig);
/// The dig ladder's tiers for @p dig, in the order they are tried.
std::array<dig_site, 2> dig_ladder(resource_type dig);

/// THE COVER AT PLACEMENT, memoised per (nation, body) for one pass.
class gen_lp_cover
{
public:
    float cover(world& w, entity_id tile);

private:
    std::map<std::pair<entity_id, entity_id>, std::unordered_map<entity_id, float>> m_fields;
};

/// `body_id`'s final household + background demand, generation's reading.
std::array<float, resource_count> body_demand(const world& w, const recipe_registry& reg,
                                              entity_id body_id);

/// How near an input's nearest producer stands, best first (BL-1185).
enum chain_tier : int
{
    chain_tier_own    = 0, ///< one of the consumer corporation's own holdings, same market
    chain_tier_market = 1, ///< any producer in the consumer's market
    chain_tier_reach  = 2, ///< a producer in another market within reach
    chain_tier_none   = 3, ///< no producer within reach: infeasible
};

/// Bring the reach context's producer/draw index up to the buildings standing now.
void chain_begin_decision(const world& w, const recipe_registry& reg, input_reach& cr);

/// A recipe's chain tier at processor @p self (its worst input's).
int chain_recipe_tier(world& w, const recipe_registry& reg, input_reach& cr, entity_id self,
                      entity_id consumer_market, const recipe& rc, const std::vector<entity_id>* own);

/// A recipe's PLACEMENT tier: its chain tier, or `none` where an input's spare
/// reachable supply does not cover the plant's draw (BL-1233).
int chain_recipe_placeable(world& w, const recipe_registry& reg, input_reach& cr, entity_id self,
                           entity_id consumer_market, const recipe& rc,
                           const std::vector<entity_id>* own);

/// Undo one authored building: the building, its stockpile, its occupancy and
/// its owner's list entry.
void chain_unplace(world& w, entity_id bid, gen_occupancy& occupied);

/// THE ONE WANT TEST (BL-1217 D6): is a processor's output still short?
struct output_want
{
    entity_id                         body = null_entity;
    std::array<float, resource_count> want{};
    std::array<float, resource_count> production{};
    bool                              power_by_grid = false;
    std::set<std::uint32_t>           short_grids; ///< power: the grids still short

    /// Is @p rc's primary output still short for a processor on @p tile?
    bool short_for(world& w, const recipe& rc, entity_id tile) const;
    /// Book a processor running @p rc (@p sign -1 takes it out).
    void book(world& w, const recipe_registry& reg, const recipe& rc, float sign = 1.0f);
};

/// The want test's arrays measured off the world for @p body, with the caller's
/// final demand.
output_want measure_output_want(world& w, const recipe_registry& reg, entity_id body,
                                const std::array<float, resource_count>& final_demand);

/// Pass 4b's stock for one kit category: the focus weights at the base
/// magnitude, unjittered (BL-1473).
std::array<float, resource_count> kit_opening_stock(industrial_focus focus);

} // namespace corpgen
