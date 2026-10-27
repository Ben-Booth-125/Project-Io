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
// span is a front. Fronts are DERIVED from `world::tile_to_nation`, never stored:
// the same territory map always gives the same fronts in the same order.
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
// already another front's key — a lone corner tile touching two neighbours — has
// no name of its own and is not listed (it is covered by the front that holds its
// tile). Apart from that, every border tile is in exactly one front per neighbour.
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
    entity_id              key       = null_entity; ///< The front's name: a tile in `span` (above).
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

/// True iff the front's nation may push across it: it has declared hostility
/// toward the neighbour (`is_hostile(nation, neighbour)`, the battle trigger's
/// directed test). A push order toward a neighbour it is not hostile to is held.
bool front_may_push(const world& w, const front& f);

/// The neighbour province a push across `f` marches into: of the LAND provinces
/// holding a neighbour tile with a hex side onto the span, the one whose
/// defending strength (summed `unit_strength` of the neighbour's units standing
/// in it) is lowest, ties to the lower province id. 0 when none. @p defence, if
/// given, takes that strength.
uint32_t front_push_target(const world& w, const front& f, int64_t* defence = nullptr);

/// The per-tick front step (MILITARY.md § Fronts, "What a front does with its
/// units"): assigned units take slots spaced evenly along their span and march to
/// them; under push they march into the neighbour's weakest adjacent province.
/// Every move is a legal `march_unit` (or `halt_unit`) applied through
/// `apply_corp_command` as the unit's own nation; a unit in battle or refitting
/// is left alone. @p reg is what `apply_corp_command` takes.
void advance_fronts(world& w, const recipe_registry& reg);
