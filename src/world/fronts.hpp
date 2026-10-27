#pragma once

#include "components.hpp"
#include "entity.hpp"

#include <cstdint>
#include <vector>

struct world;

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
// Both name a front by its KEY — the lowest tile entity id in its span — so a
// front is addressed by a tile, not by an index that shifts when territory moves.
// `reanchor_fronts` keeps both pointing at the right span after a border changes.
// ---------------------------------------------------------------------------

struct front
{
    entity_id              nation    = null_entity; ///< The nation the front belongs to.
    entity_id              neighbour = null_entity; ///< The nation across it.
    entity_id              key       = null_entity; ///< The lowest tile id in `span`: the front's name.
    std::vector<entity_id> span;                    ///< Border tiles in walk order, end to end.
};

/// Every front of `nation`, ordered by (neighbour, key). Empty for a nation with
/// no land border. Pure: reads `tile_to_nation` and the body's grid only.
std::vector<front> derive_fronts(const world& w, entity_id nation);

/// The front of `nation` whose span contains `tile`, or nullptr. `fronts` is
/// `derive_fronts(w, nation)`'s result; the pointer is into it.
const front* front_at(const std::vector<front>& fronts, entity_id tile);

/// The order stored for the front keyed `key`, or `front_order::hold` when none is.
front_order front_order_of(const world& w, entity_id nation, entity_id key);

/// After `tile_to_nation` changes: move every unit assignment and stored order
/// onto the span that now holds its front, and drop those whose front is gone.
/// Deterministic; mutates only `unit_component::front` and `world::front_orders`.
void reanchor_fronts(world& w);

/// The per-tick front step (MILITARY.md § Fronts, "What a front does with its
/// units"): assigned units take slots spaced evenly along their span and march to
/// them; under push they march into the neighbour's weakest adjacent province.
/// Every move is a legal `march_unit`; a unit in battle is left alone.
void advance_fronts(world& w);
