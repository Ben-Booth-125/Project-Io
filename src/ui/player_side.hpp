#pragma once

#include "world/world.hpp"

namespace ui {

/// BL-1406 (Ben, 2026-10-23: the player IS its nation): true when @p id names the
/// player as an OWNER — of a unit, a trade, a convoy, a battle side — which is its
/// nation once the world is folded (`player_actor`), or the seat corporation on an
/// unfolded world (a fixture) and on any record written before the fold. The one
/// test every "is this mine?" read in the UI uses, so the two ids cannot drift.
///
/// Not for a per-tile loop: `player_actor` walks the corporations. Hoist
/// `player_actor(w)` out of the loop and compare against it there instead.
inline bool is_player_side(const world& w, entity_id id)
{
    return id != null_entity && (id == player_actor(w) || id == w.player_entity);
}

/// BL-1406: the display name of an OWNER — a nation (after the fold every
/// building, unit and trade is one's) or a corporation; null when it is neither.
inline const char* owner_display_name(const world& w, entity_id id)
{
    if (const auto nit = w.nations.find(id); nit != w.nations.end())
        return nit->second.name.c_str();
    if (const auto cit = w.corporations.find(id); cit != w.corporations.end())
        return cit->second.name.c_str();
    return nullptr;
}

} // namespace ui
