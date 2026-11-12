#pragma once

// ---------------------------------------------------------------------------
// space_fixture.hpp — the launch-gate staging the space harnesses share
// (BL-1581 / BL-1582). Not a harness: a header space_assets_harness.cpp and
// station_harness.cpp include, so both stage a nation that MAY launch the same
// way: every Digitisation node held, a staffed Launchpad on the home body, and a
// home market whose shelf holds the launch goods at a posted price.
// ---------------------------------------------------------------------------

#include "world/components.hpp"
#include "world/digitisation_tree_data.hpp"
#include "world/nation_tech.hpp"
#include "world/space_tech_keys.hpp"
#include "world/world.hpp"

#include <string_view>

namespace space_fixture {

/// The fixture shelf's posted prices (the goods' BL-644 base prices).
inline constexpr float k_components_price = 310.0f;
inline constexpr float k_propellant_price = 64.0f;
inline constexpr float k_shelf_stock      = 100000.0f;

/// Does node @p n carry any near-space key?
inline bool carries_space_key(int n)
{
    for (const std::string_view k : space_keys::near_space)
        if (node_has_mechanic(n, k))
            return true;
    return false;
}

/// Every Digitisation node @p nation may hold at once — every node, less one
/// side of each pick-one pair (the save refuses both held; the side carrying no
/// space key goes) — so Rocketry, every role and module key and the 7x5 upgrade
/// are held. Far-space keys stay closed by construction.
inline void hold_every_node(world& w, entity_id nation)
{
    auto& held = w.nation_tech[nation].held;
    held.set();
    for (int n = 0; n < io::dg::node_count; ++n)
    {
        const int ex = io::dg::nodes[static_cast<std::size_t>(n)].excludes;
        if (ex < 0 || !held.test(static_cast<std::size_t>(n)) || !held.test(static_cast<std::size_t>(ex)))
            continue;
        held.reset(static_cast<std::size_t>(carries_space_key(n) ? ex : n));
    }
}

/// Release every node carrying mechanic @p key from @p nation.
inline void drop_key(world& w, entity_id nation, std::string_view key)
{
    for (int i = 0; i < io::dg::node_count; ++i)
        if (node_has_mechanic(i, key))
            w.nation_tech[nation].held.reset(static_cast<std::size_t>(i));
}

/// A tile on @p body (no grid: the fixture's bodies carry none).
inline entity_id add_tile(world& w, entity_id body)
{
    const entity_id id = w.create_entity();
    tile_component t{};
    t.body = body;
    w.tiles.emplace(id, t);
    return id;
}

/// A completed Launchpad of @p nation on @p tile, staffed by @p workforce.
inline entity_id add_pad(world& w, entity_id nation, entity_id tile, float workforce)
{
    const entity_id bid = w.create_entity();
    building_component b{};
    b.tile = tile;
    b.type = building_type::launchpad;
    b.workforce_assigned = workforce;
    b.ticks_remaining = 0;
    w.buildings.emplace(bid, b);
    w.nations.at(nation).state_assets.push_back(bid);
    return bid;
}

/// A market on @p body whose shelf holds the launch goods.
inline entity_id add_shelf(world& w, entity_id body, float stock = k_shelf_stock)
{
    const entity_id id = w.create_entity();
    market_component m{};
    m.body = body;
    m.centre_tile = null_entity;
    const std::size_t c = static_cast<std::size_t>(resource_type::spacecraft_components);
    const std::size_t p = static_cast<std::size_t>(resource_type::propellant);
    m.base_price[c] = m.price[c] = k_components_price;
    m.base_price[p] = m.price[p] = k_propellant_price;
    m.inventory[c] = stock;
    m.inventory[p] = stock;
    w.markets.emplace(id, m);
    return id;
}

/// The shelf's two launch goods, for "nothing touched" snapshots.
inline float shelf_of(const world& w, entity_id market, resource_type r)
{
    const auto it = w.markets.find(market);
    return it == w.markets.end() ? -1.0f : it->second.inventory[static_cast<std::size_t>(r)];
}

} // namespace space_fixture
