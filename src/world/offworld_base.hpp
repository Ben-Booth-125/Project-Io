#pragma once

#include "components.hpp" // resource_type
#include "entity.hpp"

#include <cstddef>
#include <cstdint>

struct world;
class recipe_registry;

// ---------------------------------------------------------------------------
// Off-world bases (BL-1601) — docs/politics/NATIONS.md § Off-world bases;
// docs/military/SPACE_FLEETS.md (the Lander); docs/ui/DISCOVERY.md § Mapping,
// then prospecting; docs/generation/PROVINCES.md; docs/economy/POPULATION.md.
// ---------------------------------------------------------------------------
// A BASE IS A CAPITAL ON ANOTHER BODY. A nation founds one by landing COLONISTS
// and a BASE KIT on a mapped tile of another body's surface.
//
// COLONISTS. Population drawn from one of the nation's home population centres
// and carried in a Lander's passenger berths (`embark_colonists`). A colonist is
// counted in the unit population is counted in — a thousand heads — so one berth
// carries one thousand (STAND-IN: the berth's head count is not authored by any
// doc; see `offworld_base_params::min_colonists`). They board a nation's Lander
// standing at its home body (idle in its low orbit, or landed on its surface)
// where the nation holds a staffed Launchpad, out of the most populous of its
// centres there. Held per unit in `world::space_colonists`; a lost unit loses
// them (pruned at the quarter and never written by the save).
//
// THE BASE KIT. A held cargo of `kit_res` aboard the nation's landed Landers at
// the target body's surface (space_convoy.hpp's hand-loaded haul), at least
// `kit_qty` in all. A passenger bay holds no cargo, so a founding reads every
// Lander of the nation landed there: one may carry the colonists, another the kit.
//
// FOUNDING (`found_offworld_base`) — refused WHOLE, nothing written, unless:
// the nation holds `base_founding` (space_tech_keys.hpp); the tile is LAND on a
// gridded body other than the star and the nation's home body; the nation has
// MAPPED it (`survey_tile_mapped`); nobody owns it, no centre or building stands
// on it, and no nation holds its province; and the nation's landed Landers there
// carry at least `min_colonists` colonists and `kit_qty` of the kit. Then:
//   1. A body with no provinces is PARTITIONED by the generation rule for a body
//      with no centres (`partition_body_if_unpartitioned`, province.hpp). Every
//      gridded body of a generated world was partitioned at generation, so on a
//      shipped world this finds provinces already drawn and does nothing.
//   2. The base CLAIMS ITS PROVINCE: every unowned tile of it becomes the nation's
//      (`world::set_tile_nation`, the nation's tile list and deposit profile as
//      `cede_provinces` moves them), and the nation holds the province.
//   3. A POPULATION CENTRE is founded on the tile with every colonist landed there
//      (scale 1, the tile's habitability), named after its body.
//   4. The body's MARKET: `maybe_spawn_market` founds one centred on the tile when
//      the body has none (the same off-world emergence a completed building
//      triggers); a body that has one already routes the base to it.
//   5. The colonists and the kit are spent (lowest unit id first); the Landers
//      stay landed, empty. A `world::offworld_bases` record keyed by the centre.
// From then on nothing about the base is special: the nation builds on its own
// ground there, its market clears, its logistics route — the ordinary systems.
//
// LIFE SUPPORT (`run_offworld_base_quarter`, the nation step, once a quarter).
// Each base, ascending centre id, needs `water_per_k` water and `food_per_k`
// food rations per thousand people, scaled by the habitability its tile falls
// short of 1 — a body that gives full habitability needs nothing. Bought whole
// off the base's own market (the shelf its tile routes to) at the posted price
// under the fair-price ceiling, paid from its owner's treasury. A base that
// cannot buy the whole lot SHRINKS by `shrink_share` of its people (at least one
// thousand) and counts an unfed quarter; at none it is razed (the razed
// settlement tier: no labour, no demand). A FED base reads habitability 1 where
// the economy aggregates its body's habitability (the labour cap, workforce
// efficiency and the growth gate, economy_system.cpp): life support is what makes
// up the shortfall, which is what it is for. An unfed one reads its tile's. The
// province a fed base stands in reads its ground at full habitability for its
// building ceiling too (province.cpp `measure_province_sustain`), else an airless
// base province sustains the one-building floor.
// OXYGEN has no good of its own: it is folded into water (electrolysis, as the
// propellant chain folds it), so the lot is water and food rations (STAND-IN).
//
// DETERMINISM: std::map stores walked ascending; integer head counts; the lot
// is float arithmetic over static inputs. No RNG, no clock.
//
// SAVED: world::offworld_bases and world::space_colonists — the BL-1601 trailing
// section (world_save.hpp). Folded into state_hash when either is non-empty.
// ---------------------------------------------------------------------------

/// One base: the centre it is keyed by names its tile, the tile its body and its
/// owner. SERIALISED (BL-1601).
struct offworld_base
{
    int32_t founded_tick   = 0; ///< The econ tick it was founded at.
    int32_t unfed_quarters = 0; ///< Consecutive econ ticks (months since BL-1642) its life support went unbought.
    /// BL-1642: the shrink CARRIED between unfed months, in milli-heads, [0, 1000). Each
    /// unfed month adds its share of the people (at least a third of a head, the quarter
    /// tick's one-head floor spread over its three months); a whole head leaves when the
    /// carry reaches 1000. Feeding does not clear it, so no pattern of fed and unfed
    /// months dodges the calendar rate. SERIALISED from world v95.
    int32_t loss_carry_milli = 0;
};

/// BL-1642: the carry's bound (exclusive), and the least a month adds to it.
inline constexpr int32_t k_offworld_loss_carry_max   = 1000;
inline constexpr int32_t k_offworld_loss_floor_milli = 334; // a third of a head: 3 months >= 1 head

/// The dials. First cut, in-band; not a Lua key yet.
struct offworld_base_params
{
    int32_t       min_colonists = 2;                      ///< Thousands of heads a founding needs.
    resource_type kit_res       = resource_type::machinery; ///< The base kit's good.
    float         kit_qty       = 20.0f;                  ///< Of it, aboard the landed Landers.
    float         water_per_k   = 1.0f / 3.0f;            ///< Water per thousand a tick (a month; a quarter 1.0, BL-1642) at habitability 0.
    float         food_per_k    = 1.0f / 3.0f;            ///< Food rations per thousand a tick (a month; a quarter 1.0) at habitability 0.
    float         shrink_share  = 0.0914397f;             ///< Of its people a base loses an unfed tick: 1 - 0.75^(1/3), the quarter's 0.25 a month (BL-1642).
    float         reservation_mult = 0.0f;                ///< The fair-price ceiling (0 = buy what the shelf holds).
};

enum class offworld_base_result : uint8_t
{
    ok = 0,
    rejected_invalid,        ///< Unknown tile, the star, no grid, water, or the home body.
    rejected_unknown_nation,
    rejected_tech_locked,    ///< `base_founding` is not held.
    rejected_unmapped,       ///< The nation has not mapped the tile.
    rejected_claimed,        ///< The tile or its province is held, or a centre or building stands on it.
    rejected_no_colonists,   ///< Fewer than `min_colonists` aboard the nation's Landers landed there.
    rejected_no_kit,         ///< Less than `kit_qty` of the kit aboard them.
    // embark_colonists:
    rejected_unknown_unit,
    rejected_not_owner,
    rejected_not_lander,     ///< Not a Lander, or no passenger berths.
    rejected_wrong_node,     ///< Not idle in its home body's low orbit or landed on it.
    rejected_no_pad,         ///< No staffed Launchpad of the nation on its home body.
    rejected_no_berths,      ///< The count does not fit the free berths.
    rejected_no_people,      ///< No centre of the nation there could spare them.
};

/// What a founding did.
struct offworld_base_op
{
    offworld_base_result result = offworld_base_result::rejected_invalid;
    entity_id centre      = null_entity;
    entity_id market      = null_entity; ///< The market the base routes to.
    bool      new_market  = false;       ///< The founding spawned it.
    bool      partitioned = false;       ///< The body had no provinces and was partitioned.
    uint32_t  province    = 0;           ///< The province claimed.
    int32_t   colonists   = 0;           ///< Heads (thousands) landed.
    int32_t   tiles_claimed = 0;
    bool      road_laid   = false;       ///< A Track was laid on the base tile.
};

/// The colonists aboard @p unit (0 when none).
int32_t space_unit_colonists(const world& w, entity_id unit);

/// THE EMBARK VERB (see the header). Refused whole. @p count thousands of heads.
offworld_base_result embark_colonists(world& w, entity_id nation, entity_id unit, int32_t count);

/// THE FOUNDING VERB (see the header). Refused whole, nothing written.
offworld_base_op found_offworld_base(world& w, const recipe_registry& reg, const offworld_base_params& p,
                                     entity_id nation, entity_id tile);

/// The base's owner (its tile's nation), body and tile; null for an unknown centre.
entity_id offworld_base_tile(const world& w, entity_id centre);

/// Is @p centre a base whose life support was bought last quarter (it has people
/// and no unfed quarter)? A base just founded reads fed until its first quarter.
bool offworld_base_fed(const world& w, entity_id centre);

/// The nation's CAPITAL ON @p body when its own capital stands elsewhere: the tile
/// of its base there (the lowest base centre id whose tile the nation holds), else
/// null. NATIONS.md § Off-world bases: "the base is ... the nation's capital on that
/// body". Read where "the capital" anchors a body's logistics: the Logistic Point
/// chain's anchor (logistic_points.cpp `chain_anchor`) and the capital's free source
/// (state_industry.cpp `capital_free_source`). No base, null.
entity_id offworld_base_capital(const world& w, entity_id nation, entity_id body);

/// The habitability the economy reads for @p centre: a fed base's life support makes
/// up what its body falls short of, so it reads 1; every other centre its own @p own.
/// The two body aggregates (the labour cap and the growth/efficiency mean,
/// economy_system.cpp) read it. No base, no change.
float offworld_base_habitability(const world& w, entity_id centre, float own);

/// What one quarter did — reported, never stored.
struct offworld_base_quarter_report
{
    int   fed = 0, unfed = 0, razed = 0;
    int   heads_lost = 0;   ///< Thousands.
    float water_bought = 0.0f, food_bought = 0.0f, spent = 0.0f;
};

/// THE QUARTER (see the header). Empty stores do nothing at all.
void run_offworld_base_quarter(world& w, const offworld_base_params& p,
                               offworld_base_quarter_report* out = nullptr);

/// Ceilings the save reader holds the stream to.
inline constexpr std::size_t k_offworld_bases_max    = 1u << 16;
inline constexpr int32_t     k_offworld_unfed_max    = 1 << 20;
inline constexpr int32_t     k_space_colonists_max   = 1 << 16;
