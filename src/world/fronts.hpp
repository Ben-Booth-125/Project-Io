#pragma once

#include "components.hpp"
#include "entity.hpp"

#include <cstdint>
#include <map>
#include <vector>

struct world;
class recipe_registry;

// ---------------------------------------------------------------------------
// Fronts (BL-1427; MILITARY.md § Fronts).
//
// A front is one contiguous span of a nation's border with one neighbour. A
// nation's border tiles toward neighbour B are its own tiles with a hex side onto
// B's ground; those tiles, joined wherever two touch, fall into spans, and each
// span is a front. Fronts are DERIVED from owner or occupier (`world::tile_controller`,
// BL-1502; MILITARY.md § Occupation), never stored: the same map always gives the same
// fronts in the same order.
//
// What is stored is what the player (or a rival's scorer) chose: a unit's
// assignment (`unit_component::front`) and each front's order (`world::front_orders`).
// Both name a front by its KEY — normally the lowest tile entity id in its span —
// so a front is addressed by a tile, not by an index that shifts when territory
// moves. `reanchor_fronts` keeps both pointing at the right span after a border
// changes.
//
// KEYS ARE UNIQUE PER NATION. One tile can border two neighbours and so sit in two
// spans, and could then be the lowest tile of both. So keys are claimed smallest
// span first (ties by neighbour, then lowest tile), each front taking the lowest
// tile of its span that no front claimed before it. A front whose every tile is
// already another front's key — a lone corner tile touching two neighbours — is
// keyed instead by the lowest unclaimed tile of its NEIGHBOUR'S ground across it
// (BL-1441), which no span of this nation holds. One case would still leave a span
// unnamed and dropped: two spans toward ONE neighbour, each wholly claimed, whose
// only tile across is the same tile. No border is known to draw it (fronts_harness
// F1g counts any dropped tile); short of it, every span is a front and every border
// tile is in exactly one front per neighbour.
//
// WALK ORDER. A span is ordered end to end: from one end A (the span tile farthest
// from its lowest tile, ties to the lower id) along the shortest path to the far
// end B (farthest from A); a span that closes on itself (it rings an enclave) runs
// A -> B and back round the other side toward A. Tiles off that line (a span two
// tiles thick) sit next to their nearest line tile. All distances are hex steps
// inside the span; every tie goes to the lower tile id.
// ---------------------------------------------------------------------------

struct front
{
    entity_id              nation    = null_entity; ///< The nation the front belongs to.
    entity_id              neighbour = null_entity; ///< The nation across it.
    entity_id              key       = null_entity; ///< The front's name: a tile in `span`, else one across it (above).
    std::vector<entity_id> span;                    ///< Border tiles in walk order, end to end.
};

/// `world::fronts_cache` (world.hpp): every nation's fronts as of the last
/// `reanchor_fronts`. Derived; never saved, never hashed.
struct front_cache
{
    bool          built = false;
    std::uint64_t territory_generation = 0; ///< `world::territory_generation` at derivation.
    std::map<entity_id, std::vector<front>> by_nation; ///< Nations with no front are absent.
};

/// Every front of `nation`, ordered by (neighbour, key). Empty for a nation with
/// no land border. Pure: reads `tile_to_nation` and the body's grid only.
std::vector<front> derive_fronts(const world& w, entity_id nation);

/// Every nation's fronts at once (one pass over the territory map), keyed by
/// nation; each list as `derive_fronts` gives it. Nations with none are absent.
std::map<entity_id, std::vector<front>> derive_all_fronts(const world& w);

/// The front of `nation` whose span contains `tile`, or nullptr. `fronts` is
/// `derive_fronts(w, nation)`'s result; the pointer is into it.
const front* front_at(const std::vector<front>& fronts, entity_id tile);

/// The front a verb's `tile` names: the front KEYED `tile` if there is one, else
/// `front_at`. A key names its own front even where its tile sits in two spans.
const front* front_named(const std::vector<front>& fronts, entity_id tile);

/// The order stored for the front keyed `key`, or `front_order::hold` when none is.
front_order front_order_of(const world& w, entity_id nation, entity_id key);

/// `nation`'s fronts from the cache, re-anchoring first if the territory moved
/// since it was built (so this is never stale). Empty when it has none. The
/// reference is valid until the next call that can re-anchor.
const std::vector<front>& nation_fronts(world& w, entity_id nation);

/// `nation`'s fronts as they stand, WITHOUT re-anchoring anything: the cached list
/// when the cache is current, else a fresh `derive_fronts`. Pure — what a verb's
/// checks read, so a refusal cannot have re-anchored the world on its way out.
std::vector<front> current_fronts(const world& w, entity_id nation);

/// After `tile_to_nation` changes: move every unit assignment and stored order
/// onto the span that now holds its front, and drop those whose front is gone.
/// Deterministic; mutates only `unit_component::front`, `world::front_orders` and
/// the derived `world::fronts_cache`. The rule (MILITARY.md § Fronts):
///   1. a key that still names a front stays;
///   2. else it moves to the front toward the SAME neighbour (read off the cached
///      fronts it was set against) whose span now holds the key tile, else to the
///      nearest front toward that neighbour (fewest hex steps from the key tile to
///      any span tile, ties to the lower key);
///   3. else it is dropped — no front toward that neighbour remains.
/// An order entry also drops when it is not the actor's to keep: a merged front
/// keeps the first entry (in (nation, key) order) that lands on it.
/// Called at the start and end of every economy tick and before `advance_fronts`;
/// cheap when nothing moved (the generation stamp matches).
void reanchor_fronts(world& w);

/// BL-1502 (MILITARY.md § Occupation; Ben, 2026-11-05): a land unit of @p nation has
/// come onto @p tile. At war with the tile's holder, it takes it: it occupies the tile,
/// or, where it is the owner or is not at war with the owner, the tile returns to its
/// owner. Water is not taken (a bridge is a road, and is). Legal ownership never moves.
/// True when the layer changed. @p batch_open starts false for a pass: the first write
/// re-anchors the fronts first, so the old fronts a key remaps against exist on a world
/// fresh from a load as on an unbroken run. A pass with @p batch_open true at its end
/// must call `reanchor_fronts`.
bool occupy_on_entry(world& w, entity_id nation, uint16_t type, entity_id tile, bool& batch_open);

/// BL-1502: every occupied tile whose occupier is no longer at war with its owner returns
/// to that owner, and the fronts re-anchor. `conclude_peace` calls it once the war record
/// has closed; a ceded tile's new owner is its occupier, so it clears by the same test.
void clear_occupation_at_peace(world& w);

/// True iff the front's nation may push across it: it has declared hostility
/// toward the neighbour (`is_hostile(nation, neighbour)`, the battle trigger's
/// directed test). A push order toward a neighbour it is not hostile to is held.
bool front_may_push(const world& w, const front& f);

/// The weakest tile a push across `f` could march onto (Ben, 2026-10-27): of the
/// neighbour's LAND tiles with a hex side onto the span that `f.nation`'s own units
/// do not already stand on, the one whose standing stack (summed `unit_strength` of
/// every other nation's units on it) is lowest — an empty tile lowest of all —
/// ties to the lower tile id. null_entity when none. @p defence, if given, takes
/// that strength. Each pushing unit breaks ties by its own slot (advance_fronts);
/// this is the front-wide reading a rival's scorer weighs.
entity_id front_push_target(const world& w, const front& f, int64_t* defence = nullptr);

/// BL-1540 (rival march supply): the tile `advance_fronts`' push would march each of @p members
/// (ascending id, the front's units) onto: the weakest stack, nearest its slot, ties to the lower
/// id, past what its class may not enter. null_entity for a unit the push would not march (busy,
/// standing on the neighbour's ground with no war, nothing reachable). Read-only but for the
/// march path cache: the same ranking and the same `march_check` the step asks.
std::vector<entity_id> front_push_tiles(world& w, const recipe_registry& reg, const front& f,
                                        const std::vector<entity_id>& members);

/// BL-1540: hex steps between two tiles of one body, the straight read a scorer may take of a
/// march. No path is solved.
int tile_hex_steps(const world& w, entity_id a, entity_id b);

/// BL-1540: how often each path of a rival's march score fired. An opt-in sink for the census:
/// nothing in the sim reads it, and it is never saved or hashed. Per unit where it names a unit,
/// per front evaluation where it names a push.
struct rival_march_tally
{
    long long fed_push = 0;       ///< A front set to push with no unit priced.
    long long prize_push = 0;     ///< A front set to push with a unit priced: the prize was worth it.
    long long refused_supply = 0; ///< 3:2 passes, and the hunger outweighs the ground: holds.
    long long refused_battle = 0; ///< The ground is worth it, and 3:2 fails only on the men the march loses.
    long long fall_back = 0;      ///< A unit marched off priced ground to its own fed ground.
    long long reweigh = 0;        ///< A unit taken off a front with no reason left, its slot priced.
    long long assign_refused = 0; ///< A unit left a front's slot it had no reason to go hungry on.
    long long odds_hungry = 0;    ///< A unit sent to a priced slot on a front at odds.
    long long odds_hungry_no_war = 0; ///< Of them, on a bare hostility: no war between the pair.
    long long liberation = 0;     ///< A priced push target on its own occupied ground, weighed as plain ground.
};
rival_march_tally*& rival_march_tally_sink();

/// BL-1540: the tile `advance_fronts`' hold gives @p unit as the @p i th of @p n on
/// `f`, by its walls but not asking `march_check`: slot i, else the nearest tile to it its class
/// may stand on. null_entity when the span has none. What a scorer reads supply at.
entity_id front_hold_slot(world& w, const recipe_registry& reg, const unit_component& unit, const front& f,
                          std::size_t i, std::size_t n);

/// BL-1441 (front edge cases): may @p unit reach front @p f? The test a front's
/// own hold step would meet for a lone unit: of the span tiles nearest the span's
/// middle (ties to the lower index), the first the unit's class may enter; reachable
/// when the unit already stands on it or `march_check` would accept a march onto
/// it. False for a span with no enterable tile, or one on another landmass. A
/// rival assigns only to fronts this answers true for, so a march the game refuses
/// is not re-issued every tick. Read-only but for the march path cache.
bool front_reachable(world& w, const recipe_registry& reg, entity_id unit, const front& f);

/// The per-tick front step (MILITARY.md § Fronts, "What a front does with its
/// units"): assigned units take slot tiles spaced evenly along their span and march
/// onto them; under push each marches onto the neighbour's weakest adjacent tile.
/// Every move is a legal `march_unit` (or `halt_unit`) applied through
/// `apply_corp_command` as the unit's own nation; a unit in battle or refitting
/// is left alone. @p reg is what `apply_corp_command` takes.
/// BL-1441: every march is asked of `march_check` first and issued only when it would
/// apply; a unit whose slot (or push target, and then slot) is refused stands where it
/// is, its standing order halted, and is asked again next day. Stateless.
struct front_step
{
    int marches_applied = 0; ///< `march_unit` verbs issued and applied this step.
    int marches_refused = 0; ///< `march_unit` verbs issued and refused (0 by construction).
};
front_step advance_fronts(world& w, const recipe_registry& reg);

// ---------------------------------------------------------------------------
// Defend province (BL-1504; MILITARY.md § Defend province, Ben 2026-11-05).
//
// A defending unit reacts only inside its province: it marches to a hostile stack
// there and otherwise returns to its post. It does not pursue out of the province.
// "Hostile" is its own nation's directed hostility, the battle trigger's test, since
// a march onto a stack it is not hostile to is a block and not an attack.
// ---------------------------------------------------------------------------

/// The post a unit ordered to defend @p province would take: the tile it stands on when
/// that is land of the province, else the nearest land tile of it the unit's nation owns
/// that `march_check` accepts (hex steps, ties to the lower id). null_entity when none.
/// Read-only but for the march path cache.
entity_id defend_post_for(world& w, const recipe_registry& reg, entity_id unit, uint32_t province);

/// The daily defending step, beside `advance_fronts`: each defending unit, ascending id,
/// outside a battle and not refitting. The nearest hostile stack in its province (hex
/// steps from the unit, ties to the lower tile id) takes a march; with none a march can
/// reach it goes back to its post, or stands. A province it no longer owns any of ends the
/// order. Every move is a legal `march_unit` or `halt_unit`, asked of `march_check` first.
front_step advance_defenders(world& w, const recipe_registry& reg);

/// May `advance_defenders` have anything to do today? A defending unit off its post, or
/// a hostile stack in a defended province. The day-skipping test (`military_active`), so
/// a garrison at its post in a quiet province keeps no day walking.
bool defenders_awake(const world& w);

/// BL-1504: the start order. Every unit of a major standing in its capital province, with
/// no front and no order, defends that province from where it stands. Called once, at the
/// handoff, after the garrisons are posted. Assigns no front (§ Nation garrisons). Returns
/// the units given the order.
int open_capital_defence(world& w);
