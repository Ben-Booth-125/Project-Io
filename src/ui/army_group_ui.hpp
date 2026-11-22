#pragma once

// BL-1737 (army groups; docs/ui/SELECTION.md § The army group element,
// docs/ui/ledgers/military.md § Groups). The UI's shared reads and the one selection
// door onto an army group. The world record and its verb are world/army_groups.hpp.

#include "world/entity.hpp"

#include <string>
#include <vector>

struct world;
struct ui_state;

namespace ui {

/// One army group as a ledger row or a pin reads it.
struct army_group_stats
{
    std::string name;            ///< "1st Army".
    int         units = 0;       ///< Standing units.
    long long   men = 0, establishment = 0;
    int         tiles = 0;       ///< Distinct tiles its standing units occupy.
    std::string front;           ///< The front's label when every unit holds that one; else "".
};

army_group_stats army_group_stats_of(const world& w, entity_id group);

/// Makes @p group the band's subject: `selected_entity` its lowest-id unit,
/// `selected_units` its units, and the other channels cleared. No-op for a group with
/// no standing unit.
void select_army_group(const world& w, ui_state& ui, entity_id group);

/// Re-derives the shown army group from the world, once a frame before the band draws:
/// clears it when the group ended or the selection moved off it, and otherwise keeps
/// `selected_units` its current members. Returns the group's standing members (empty
/// when no army group is shown).
std::vector<entity_id> sync_army_group_selection(const world& w, ui_state& ui);

} // namespace ui
