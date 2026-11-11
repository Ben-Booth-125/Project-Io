#pragma once

// ---------------------------------------------------------------------------
// BL-1431 — missiles, interceptors and tactical nuclear warheads
// (MILITARY.md § Missiles; AI_OPPONENT.md § 11, "Rival nations fire missiles")
// ---------------------------------------------------------------------------
// A missile is a MUNITION, not a unit: assembled from kit goods into a nation's
// MAGAZINE (`world::missile_magazines`) by `stock_missiles`, and consumed when a
// LAUNCHER of that nation fires it at a province or a building (`fire_missile`).
// Launchers are the nation's own land units, ships and wings, by class (the table in
// `missile_launcher_carries`); a silo building is not modelled.
//
// Missile defence is the INTERCEPTOR STOCK at an air defence unit
// (`world::interceptor_stocks`, `stock_interceptors`). A strike on a province draws
// the interceptors of every air defence unit there whose nation is hostile to the
// firer, either way, `interceptors_per_missile` to a missile, ascending unit id. What
// gets through is ARITHMETIC, not a draw: one interceptor lets (1000 - intercept)
// per-mille of a missile through, two let that squared, and a missile no interceptor
// met lands whole. The strike's damage is scaled by the milli-missiles that land.
//
// A TACTICAL NUCLEAR warhead is a missile with an AREA effect and a severe Alarm cost:
// firing one queues `nuclear_used` conduct from every other nation toward the firer
// (the struck nation's counted `nuclear_struck_magnitude` times), whether or not it
// is intercepted. A STRATEGIC weapon is never held and no verb fires one.
//
// DETERMINISM. Ordered maps, ascending ids, integer per-mille arithmetic; no RNG.
// HEADLESS. Pure world layer: no SDL, no Lua.
// ---------------------------------------------------------------------------

#include "corp_command.hpp" // corp_command_result
#include "entity.hpp"
#include "missile_params.hpp"

#include <cstdint>
#include <vector>

struct world;
struct sentiment_params;
class recipe_registry;

/// What a launcher is: one of the nation's land units, ships or wings.
enum class missile_launcher_type : uint8_t { none = 0, land_unit, ship, wing };

/// The launcher type of entity @p launcher (none when it is no unit, ship or wing).
missile_launcher_type missile_launcher_of(const world& w, entity_id launcher);

/// May launcher @p launcher carry kind @p k (MILITARY.md § Missiles, the launcher table)?
/// Artillery: short range and tactical nuclear. Destroyer: short. Capital ship: short and
/// long. Submarine: long and tactical nuclear. Multirole: short. Bomber: long and tactical
/// nuclear. Nothing carries a strategic weapon.
bool missile_launcher_carries(const world& w, entity_id launcher, missile_kind k);

/// The tile a launcher fires from (a unit's or ship's tile, a wing's airbase's), and its
/// reach for kind @p k in tiles (a wing adds its class's range). Null / -1 when it cannot
/// fire now: not raised, sunk, refitting, or a wing whose airbase is gone or not its own.
entity_id missile_launch_tile(const world& w, const recipe_registry& reg, entity_id launcher,
                              missile_kind k, int* reach_out);

/// Does @p nation SEE @p province — the target rule both the player's verb and the rival
/// scorer obey? It does when it holds or owns ground there; when one of its raised land
/// units or ships stands within `sight_tiles` of the province's nearest tile; when one of
/// its flying wings is assigned there; or when its air revealed a hostile unit there, or
/// its ships sighted a ship there.
bool missile_province_sighted(const world& w, const recipe_registry& reg, entity_id nation,
                              uint32_t province);

/// A nation's magazine count of kind @p k (0 for a strategic weapon or an unknown nation).
int32_t missile_stock(const world& w, entity_id nation, missile_kind k);

/// The goods @p count missiles of @p k (or, with @p interceptors, interceptors) take.
std::array<float, resource_count> missile_goods(const recipe_registry& reg, missile_kind k, int count,
                                                bool interceptors = false);

/// The node a tactical nuclear warhead needs held before it may be stocked
/// (Uranium Enrichment, DG-PR-EN-B2).
int missile_nuclear_node();

/// Every check `stock_missiles` makes before money moves, READ-ONLY: @p nation a nation
/// (rejected_invalid); @p kind a stockable kind, never strategic (rejected_invalid);
/// @p count 1..stock_per_verb (rejected_invalid); a tactical warhead needs the nuclear
/// node (rejected_tech_locked); the magazine has room (rejected_state). The goods and
/// price legs are the seam's (corp_command.cpp).
corp_command_result stock_missiles_check(const world& w, const recipe_registry& reg, entity_id nation,
                                         uint32_t kind, uint32_t count);

/// Every check `stock_interceptors` makes before money moves, READ-ONLY: @p unit exists
/// (rejected_invalid) and is @p nation's (rejected_not_owner), an air defence unit with
/// men (rejected_invalid); @p count 1..stock_per_verb (rejected_invalid); room under the
/// cap (rejected_state).
corp_command_result stock_interceptors_check(const world& w, const recipe_registry& reg, entity_id nation,
                                             entity_id unit, uint32_t count);

/// One planned strike: what `fire_missile` would do, computed without mutating.
struct missile_strike_plan
{
    missile_kind kind  = missile_kind::short_range;
    int          salvo = 0;
    entity_id    struck = null_entity; ///< The nation whose ground or building is hit.
    int64_t      landed_milli = 0;     ///< Milli-missiles that get through.
    int          interceptors = 0;     ///< Interceptors drawn.
    std::vector<std::pair<entity_id, int32_t>> interceptor_draws; ///< (unit, count), ascending.
    /// (unit, men lost, readiness per-mille lost), ascending unit id.
    struct unit_hit { entity_id unit; int32_t men; int32_t readiness; };
    std::vector<unit_hit> unit_hits;
    std::vector<std::pair<entity_id, int32_t>> building_hits; ///< (building, condition lost).
    int men = 0;        ///< Summed.
    int condition = 0;  ///< Summed per-mille.
};

/// Every check `fire_missile` makes, READ-ONLY, in its order: the launcher exists
/// (rejected_invalid) and is @p nation's (rejected_not_owner); @p kind is a fireable kind
/// the launcher carries — a strategic weapon is refused here, always (rejected_invalid);
/// @p salvo 1..salvo_max (rejected_invalid); the launcher can fire now (rejected_state);
/// @p province exists (rejected_invalid); a named @p building stands, completed, in it
/// (rejected_invalid); the target belongs to a nation @p nation has declared hostile —
/// the building's owner, or for a province strike its holder or a hostile unit standing
/// there (rejected_state); within reach (rejected_placement); sighted (rejected_state);
/// the magazine holds the salvo (rejected_funds). When it would apply, @p plan_out (if
/// given) receives the strike.
corp_command_result fire_missile_check(const world& w, const recipe_registry& reg, entity_id nation,
                                       entity_id launcher, uint32_t kind, uint32_t salvo,
                                       uint32_t province, entity_id building,
                                       missile_strike_plan* plan_out = nullptr);

/// One target a launcher may strike: a province (its hostile units), or a building in it.
struct missile_target
{
    uint32_t  province = 0;
    entity_id building = null_entity; ///< Null: a province strike.
};

/// Every target @p launcher of @p nation may strike with kind @p k now, the magazine
/// aside: the provinces holding a unit of a nation @p nation has declared hostile, then
/// the hostile buildings whose condition matters, each ascending — kept only where
/// `fire_missile_check` passes every leg but the stock. What the card's target list shows.
std::vector<missile_target> missile_targets(const world& w, const recipe_registry& reg, entity_id nation,
                                            entity_id launcher, missile_kind k);

/// Commit a strike `fire_missile_check` planned for @p nation: the magazine and the
/// interceptor stocks drawn, the damage applied, `force_used` conduct for the men lost,
/// and for a tactical warhead the `nuclear_used` Alarm queued.
void apply_missile_strike(world& w, const recipe_registry& reg, entity_id nation,
                          const missile_strike_plan& plan);

/// The Alarm a tactical strike of @p salvo warheads by @p firer on @p struck raises,
/// summed over every observer, at @p sp's authored weight (before any alarm_toward
/// scale). What the rival scorer prices and the harness compares with a declaration.
float nuclear_alarm_sum(const world& w, const sentiment_params& sp, entity_id firer, entity_id struck,
                        int salvo, float struck_magnitude = default_missile_params().nuclear_struck_magnitude);

/// BL-1431 (AI_OPPONENT.md § 11, "Rival nations fire missiles"): a rival nation's missile
/// pass. Never the player's nation; only while mobilised. At most: one stock_missiles, one
/// stock_interceptors, and one fire_missile from its own launcher at a sighted target of a
/// nation it is already hostile to, chosen by scored value against the missiles' cost (a
/// tactical warhead also charged its Alarm). Every action through `apply_corp_command`.
void score_nation_missiles(world& w, const recipe_registry& reg, entity_id nation, int tick);
