#pragma once
// BL-1370 (a road must name which neighbour it crosses to) — the ARMED half of
// the road ladder, shared by the Construction view's road rows and the Planetary
// canvas that highlights their targets.
//
// A road is a CROSSING between two tiles, never a property of one
// (`docs/economy/LOGISTICS.md` § 4), so laying one names a tile AND a side. Until
// this item no UI ever supplied a side: the press passed `k_road_side_auto` and
// the rules then guessed, which is why a tile that already carried a road answered
// *"this tile already has an equal or better road"* — the question it was asked was
// about the tile. The two presses
// (`docs/ui/ledgers/construction.md` § "A road is built from a road to a tile, in
// two presses") are: the row ARMS, then a press on a highlighted neighbour lays
// that one crossing.
//
// THE RULE IS THE AUTHORITY, not this file. `road_crossing_targets` walks the six
// sides and asks `placement_rules::can_place_road(w, tile, side, tier)` — the same
// gate `place_road` runs — so the highlight and the refusal cannot disagree, and
// the legality of a crossing is stated in exactly one place
// (`src/world/placement_rules.cpp`). Nothing here invents a condition of its own.

#include "world/placement_rules.hpp" // BL-1385: the refusal the press explains
#include "world/world.hpp"

#include <cstdint>
#include <vector>

namespace ui
{

/// One legal crossing out of an armed tile: the hex side of the armed tile, and
/// the neighbour that side crosses to.
struct road_target
{
    int       side = -1;           ///< 0=E, 1=NE, 2=NW, 3=W, 4=SW, 5=SE (odd-r offset).
    entity_id tile = null_entity;  ///< The neighbour across @ref side.
};

/// Every crossing `placement_rules::can_place_road` admits out of @p tile at
/// @p tier, in side order (so the walk is deterministic and world-order
/// independent). Empty for a tile with nowhere left to go — and an EMPTY SET MUTES
/// THE PRESS (BL-1379, construction.md § "A tile with nowhere to go mutes the
/// press"): the row disables its Build and its explainer says why, because a press
/// that arms and highlights nothing teaches nothing about the refusal.
/// @p actor is the CORPORATION the highlight is drawn FOR -- the player's. BL-1357 reads
/// it: a tile another nation holds is one a road may cross INTO and not build OUT of, so
/// the set it admits depends on who is asking. Passing `null_entity` asks the
/// nation-blind form, which is the generation/affordance read and not a player surface.
std::vector<road_target> road_crossing_targets(const world& w, entity_id tile,
                                               std::uint8_t tier,
                                               entity_id actor = null_entity);

/// WHY an empty set is empty (BL-1385; Ben, in play 2026-10-18: "every tile with a road
/// I try to press has the build button greyed out"). The muted press carries the whole
/// of the reason, and until this it carried ONE reason for every case - "every side is
/// either already roaded to this standard or one no crossing may take" - which is a
/// sentence about the ground that was never checked against the ground. A crossing
/// refused because the tile belongs to ANOTHER NATION (BL-1357) was told it had nowhere
/// left to go, which is a different thing the player would act on differently.
///
/// Returns the refusal that best explains the set: `foreign_ground` where any side was
/// refused for it (the one the player cannot fix on this tile), then
/// `no_road_to_extend`, then `already_road`, then `ocean` for a tile whose every side is
/// uncrossable or has no neighbour. `ok` when the set is NOT empty.
placement_rules::placement_reason road_crossing_refusal(const world& w, entity_id tile,
                                                        std::uint8_t tier,
                                                        entity_id actor = null_entity);

} // namespace ui
