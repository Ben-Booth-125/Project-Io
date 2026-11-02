#pragma once

// ---------------------------------------------------------------------------
// BL-1463 — recruits (MILITARY.md § Recruits, Ben 2026-10-30)
// ---------------------------------------------------------------------------
// SOLDIERS ARE RECRUITS, AND BASES MAKE THEM. Once a quarter every completed, running
// `military_base` makes `land_per_base` LAND recruits and every `airbase`
// `air_per_base` AIR recruits (economy.military.recruits, recruit_params.hpp). There is
// no other source.
//
//   * FROM THE PROVINCE'S PEOPLE. Each recruit is one head taken from the population
//     centres of the base's province, the centre with the most heads first, ties to the
//     lowest id. A province whose centres cannot give makes fewer. A head comes off the
//     centre's thousands through its carry (`population_centre_component::
//     recruit_heads_taken`), so the count is exact.
//   * ONE POOL PER THEATRE PER NATION, the base's OWNER's (`owner_nation_of`), wherever
//     the base stands (`nation_component::land_recruits` / `air_recruits`).
//   * THE CAP IS FIFTEEN QUARTERS of the theatre's output: 15 x (its running bases x the
//     rate) - the NOMINAL output, so the cap does not move with what the provinces gave.
//     A full pool takes no more; its bases make none that quarter. The cap stops intake
//     only: a pool above it (its bases gone) is never cut.
//   * A HIRE SPENDS ITS MEN (`hire_unit_recruits`, a land unit's `hire_batch_manpower`)
//     or ITS CREW (`wing_crew`); the seam refuses a hire the pool cannot fill before any
//     money moves (`rejected_no_recruits`).
//   * LOSSES REFILL FROM THE POOL, PAID IN LOGISTIC POINTS. A land unit below its
//     establishment plans to draw what it lacks from its nation's land pool (ascending
//     unit id, while the pool lasts: `plan_recruit_refills`). The plan's recruits x
//     `lp_per_recruit` join the unit's supply request (`collect_supply_claims`), paid at
//     its paying road within 8 tiles exactly as its supply is; the unit receives its
//     planned recruits x the share it was granted (`apply_recruit_refills`). Past the
//     8-tile draw the share is 0 and nothing refills.
//   * PORTS MAKE SEA RECRUITS (Ben, 2026-11-02). Every completed, running `port` makes
//     `sea_per_port` SEA recruits a quarter, from its province's centres exactly as a
//     base does, into the nation's one sea pool (`nation_component::sea_recruits`),
//     capped at 15 quarters of its ports' output. Nothing spends the sea pool until ships
//     are raised: no hire, no refill and no refund touches it, so it fills and stops.
//
// DETERMINISM. Nations, bases, centres and units are walked in ascending id; all of it is
// integer arithmetic save the LP need, which is the resolver's own float path. No RNG.
// HEADLESS. Pure world layer: no SDL, no Lua.
// ---------------------------------------------------------------------------

#include "entity.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct world;
struct lp_tick;
struct population_centre_component;
struct muster_order;
class recipe_registry;

/// The three theatres a pool belongs to.
enum class recruit_theatre : uint8_t
{
    land = 0, ///< military_base -> land_recruits
    air  = 1, ///< airbase       -> air_recruits
    sea  = 2, ///< port          -> sea_recruits (Ben, 2026-11-02); nothing spends it yet
};

/// The land recruits a hire of land roster row @p row spends: its men at muster.
int hire_unit_recruits(uint16_t row);

/// The air recruits a hire of air roster row @p row spends: its class's crew. 0 for an
/// unknown row (the seam refuses it on its own check first).
int wing_crew(const recipe_registry& reg, uint16_t row);

/// @p nation's pool in @p t (0 for no such nation).
int recruit_pool(const world& w, entity_id nation, recruit_theatre t);

/// The recruits @p nation's running bases of theatre @p t make a quarter at the authored
/// rate - the nominal output the cap is measured on.
int recruit_quarter_output(const world& w, const recipe_registry& reg, entity_id nation,
                           recruit_theatre t);

/// 15 x `recruit_quarter_output`: the most the pool takes in.
int recruit_cap(const world& w, const recipe_registry& reg, entity_id nation, recruit_theatre t);

/// The heads @p p can still give: its population in heads less its carry; 0 once razed.
int64_t centre_heads_available(const population_centre_component& p);

/// The words a refused hire shows: "Needs 400 recruits; the pool holds 120".
std::string recruit_shortfall_text(int need, int held);

/// One nation's quarter, as `run_recruitment` made it.
struct recruitment_row
{
    entity_id nation     = null_entity;
    int       land_made  = 0; ///< land recruits into the pool this quarter
    int       air_made   = 0; ///< air recruits into the pool this quarter
    int       land_short = 0; ///< land recruits the bases' rate asked that the provinces could not give
    int       air_short  = 0;
    int       sea_made   = 0; ///< sea recruits into the pool this quarter (ports)
    int       sea_short  = 0;
};

struct recruitment_tick
{
    std::vector<recruitment_row> nations; ///< ascending nation id, every nation with a running base or port
    int64_t heads_taken = 0;              ///< heads taken from population centres, world-wide
    int     centres_lost_thousand = 0;    ///< times a centre's population fell by one (thousand)
};

/// The quarter's recruitment (see the file comment). Run once per economy tick.
recruitment_tick run_recruitment(world& w, const recipe_registry& reg);

/// One land unit's refill this quarter: what it lacks, as far as its pool reaches.
struct recruit_refill
{
    entity_id unit    = null_entity;
    entity_id nation  = null_entity;
    int       planned = 0; ///< recruits it would draw at a whole share
};

/// The quarter's refill plan: every living land unit of a nation below its establishment,
/// ascending unit id, each planned min(what it lacks, what its nation's land pool has
/// left after the units before it). Units planning nothing are left out. Pure.
std::vector<recruit_refill> plan_recruit_refills(const world& w);

/// The Logistic Points @p r's plan asks: planned x `lp_per_recruit`.
float recruit_refill_lp_need(const recipe_registry& reg, const recruit_refill& r);

/// Deliver @p plan at the shares @p lp granted: each unit receives the WHOLE recruits the
/// resolver granted it (`lp_supply_row::recruits_delivered`, floor(planned x share /
/// 1000)), taken from its pool. Returns the recruits delivered world-wide.
int apply_recruit_refills(world& w, const std::vector<recruit_refill>& plan, const lp_tick& lp);

// ---- Wings refill too (Ben, 2026-10-31) ---------------------------------------
// A wing below full HP draws AIR recruits from its nation's air pool, paid in Logistic
// Points at its paying road (its airbase's tile, the closest road within 8 tiles) as a
// land refill is, none past the 8-tile draw. HP maps to crew by the class's crew table:
// each recruit restores k_wing_hp_full / crew of HP, so a wing missing a share s of its
// HP plans ceil(s x crew) recruits - never 0 while it is below full, so a wing can always
// come back whole - and the last of them tops it up exactly to full.

/// One wing's refill this quarter.
struct wing_refill
{
    entity_id wing    = null_entity;
    entity_id nation  = null_entity;
    entity_id tile    = null_entity; ///< its airbase's tile, where the request is routed from
    int       planned = 0;           ///< air recruits it would draw at a whole share
    int       lacks   = 0;           ///< ceil(missing HP share x crew): the recruits that make it whole
};

/// The quarter's wing refill plan: every flying (mustered), living wing of a nation below
/// full HP whose airbase stands, ascending wing id, each planned min(ceil(missing HP share
/// x crew), what its nation's air pool has left after the wings before it). Pure.
std::vector<wing_refill> plan_wing_refills(const world& w, const recipe_registry& reg);

/// Deliver @p plan at what @p lp granted, in whole recruits: a wing receiving d of its
/// planned p regains d x full / crew HP, or exactly to full when d makes it whole; its air pool
/// gives d. Returns the air recruits delivered world-wide.
int apply_wing_refills(world& w, const recipe_registry& reg, const std::vector<wing_refill>& plan,
                       const lp_tick& lp);

// ---- A dropped muster refunds its men (Ben, 2026-10-31) -------------------------

/// A muster order dropped unraised (its base lost, demolished or decommissioned) returns
/// the recruits its hire spent to its nation's land pool. Uncapped by the pool's cap - a
/// refund never discards - and bounded only by the save's pool ceiling. Credits are not
/// refunded. Returns the recruits returned.
int refund_muster_recruits(world& w, const muster_order& m);
