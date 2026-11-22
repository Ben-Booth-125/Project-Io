// BL-1737 (army groups). See army_group_ui.hpp.

#include "army_group_ui.hpp"

#include "front_view.hpp"
#include "ui_state.hpp"
#include "world/army_groups.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <set>

namespace ui {

army_group_stats army_group_stats_of(const world& w, entity_id group)
{
    army_group_stats s;
    s.name = army_group_name(w, group);
    const std::vector<entity_id> members = army_group_members(w, group);
    s.units = static_cast<int>(members.size());
    std::set<entity_id> tiles;
    entity_id front = null_entity, owner = null_entity;
    bool one_front = !members.empty();
    for (const entity_id uid : members)
    {
        const unit_component& u = w.units.at(uid);
        s.men           += u.count;
        s.establishment += unit_establishment(u);
        tiles.insert(u.position);
        owner = u.owner;
        if (uid == members.front())
            front = u.front;
        else if (u.front != front)
            one_front = false;
    }
    s.tiles = static_cast<int>(tiles.size());
    if (one_front && front != null_entity)
        if (const front_view* fv = front_by_key(w, owner, front))
            s.front = fv->label;
    return s;
}

void select_army_group(const world& w, ui_state& ui, entity_id group)
{
    const std::vector<entity_id> members = army_group_members(w, group);
    if (members.empty())
        return;
    ui.clear_battle_selection();
    ui.clear_front_selection();
    ui.clear_lens_region_selection();
    ui.selected_convoy       = 0;
    ui.selected_province     = 0u;
    ui.selection_cycle_tile  = null_entity;
    ui.selected_entity       = members.front();
    ui.province_sync_entity  = members.front(); // a selection made here, not a change to reconcile
    ui.selection_army_group  = group;
    ui.selection_unit_group  = members.size() > 1;
    if (members.size() > 1)
        ui.selected_units = members;
    else
        ui.selected_units.clear();
}

std::vector<entity_id> sync_army_group_selection(const world& w, ui_state& ui)
{
    if (ui.selection_army_group == null_entity)
        return {};
    const std::vector<entity_id> members = army_group_members(w, ui.selection_army_group);
    // The group ended, or the selection moved to something not of it (a reconcile in the
    // canvas clears the channel too; this covers every other surface).
    if (members.empty()
        || std::find(members.begin(), members.end(), ui.selected_entity) == members.end())
    {
        if (!members.empty() && ui.selected_entity == null_entity)
        {
            select_army_group(w, ui, ui.selection_army_group); // a fallen head: the next one leads
            return members;
        }
        ui.selection_army_group = null_entity;
        return {};
    }
    // A row press opens one unit's card (selection_unit_group false): keep the card, but
    // stand down the group so the band draws that unit.
    if (members.size() > 1 && !ui.selection_unit_group)
    {
        ui.selection_army_group = null_entity;
        return {};
    }
    ui.selection_unit_group = members.size() > 1;
    if (members.size() > 1)
        ui.selected_units = members;
    else
        ui.selected_units.clear();
    return members;
}

} // namespace ui
