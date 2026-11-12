#include "selection.hpp"

namespace ui {

selection_kind selection_kind_of(const world& w, entity_id e)
{
    if (e == null_entity)
        return selection_kind::none;

    // Probe in the same order as focus_on_entity (view_nav.cpp): tiles,
    // buildings, units, markets, bodies. Ids are unique across the stores.
    if (w.tiles.count(e))        return selection_kind::tile;
    if (w.buildings.count(e))    return selection_kind::building;
    if (w.units.count(e))        return selection_kind::unit;
    if (w.markets.count(e))      return selection_kind::market;
    if (w.bodies.count(e))       return selection_kind::body;
    if (w.nations.count(e))      return selection_kind::nation;
    if (w.wings.count(e))        return selection_kind::wing; // BL-1450
    if (w.ships.count(e))        return selection_kind::ship; // BL-1534
    // BL-1590: a station (its id is its core's) or a satellite.
    if (w.stations.count(e))     return selection_kind::space_asset;
    if (const auto ait = w.space_assets.find(e);
        ait != w.space_assets.end() && ait->second.cls == space_asset_class::satellite)
        return selection_kind::space_asset;
    if (w.space_units.count(e))  return selection_kind::space_unit; // BL-1609
    // BL-1407: no corporation exists to select (`selection_kind::corporation`
    // stays in the enum, never returned).

    return selection_kind::none;
}

const char* selection_kind_name(selection_kind k)
{
    switch (k)
    {
        case selection_kind::none:     return "Nothing";
        case selection_kind::body:     return "Body";
        case selection_kind::tile:     return "Tile";
        case selection_kind::building: return "Building";
        case selection_kind::market:   return "Market";
        case selection_kind::unit:     return "Unit";
        case selection_kind::nation:      return "Nation";
        case selection_kind::corporation: return "Corporation";
        case selection_kind::battle:      return "Battle";
        case selection_kind::contract:    return "Contract";
        case selection_kind::wing:        return "Wing";
        case selection_kind::ship:        return "Ship";
        case selection_kind::space_asset: return "Space asset";
        case selection_kind::space_unit:  return "Space unit";
    }
    return "?";
}

} // namespace ui
