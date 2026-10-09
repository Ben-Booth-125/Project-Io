// Headless sea-port-gate harness (BL-608, BL-1186; no SDL / Lua / ImGui).
//
// BL-1186 (goods cross markets) REWROTE THE RULE THIS FILE PINS. SUPPLY.md §
// Logistical cost: a route that crosses water is land -> port -> sea -> port ->
// land, each leg at its own mode, the ports chosen to minimise the whole route
// with a HANDLING fee at each; the Port gates the sea LEG, not the market
// centre; and a pair whose cheapest path crosses water but which has an overland
// road is routed overland rather than refused. So:
//   * R0 still dispatches by sea, and now pays two handling fees.
//   * R1/R2 still refuse — on an ISLAND fixture (a second water band closes the
//     overland detour round the cylinder), because no port PAIR exists.
//   * R4 (new): with the detour open and no port pair, the haul goes overland.
//   * R5 (new): both markets INLAND, ports on the coast between them — the route
//     is three legs priced at their own modes plus two fees (the B1 case the old
//     endpoint gate refused).
//   * R6 (new): of two far-shore ports, the one minimising the WHOLE route wins.
//   * R7 (new): a port off the water is no sea end; legs never change domain.
//
// SUPPLY.md § Infrastructure gates says sea mode requires "Port building at
// both endpoints", but the mode-selection logic (`path.crosses_ocean` picking
// sea vs land in `price_convoy_leg`, src/world/supply_system.cpp) was UNGATED
// — sea pricing/speed applied whenever the cheapest A* path crossed water,
// with no check that either endpoint held a Port. This harness asserts the
// gate `tile_has_active_port` now enforces.
//
//   R0  BOTH-PORTS: a sea leg with an active Port at both the source anchor
//       and the destination market's centre tile dispatches normally, in sea
//       mode, at the sea unit cost.
//
//   R1  MISSING-PORT REFUSES, MUTATES NOTHING. A sea leg missing a Port at
//       EITHER endpoint (dest only, source only, neither) is refused through
//       the existing `!leg.viable -> rejected_placement` path (same as "no
//       launchpad", "no reachable route") rather than silently falling back
//       to land pricing over a path that physically crosses open water. The
//       corp's balance and cargo pool are asserted unchanged via a full
//       world fingerprint, matching convoy_command.cpp's R1 discipline.
//
//   R2  A DECOMMISSIONED OR UNBUILT PORT DOES NOT COUNT. Same built+active
//       test `is_supply_anchor` / `collect_logistics_nodes` already use for
//       port/hub anchors (ticks_remaining <= 0 && !decommissioned).
//
//   R3  DETERMINISM. Two runs of the same scripted sequence (both-ports
//       dispatch, then a missing-port rejection) produce byte-identical
//       results.
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/logistics.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

constexpr std::size_t r_iron = static_cast<std::size_t>(resource_type::iron_ore);

// ---------------------------------------------------------------------------
// Fixture: one body, a single row of tiles — land at column 0 (the corp's
// anchor / source market) and column 3 (the destination market's centre),
// OCEAN at columns 1-2 in between, so the only path between them crosses
// water and `crosses_ocean` is forced true (mirrors convoy_command.cpp's
// hand-built-grid idiom).
// ---------------------------------------------------------------------------
struct scenario
{
    world     w;
    entity_id body       = null_entity;
    entity_id corp       = null_entity;
    entity_id src_market = null_entity;
    entity_id dst_market = null_entity;
    entity_id src_tile   = null_entity;
    entity_id dst_tile   = null_entity;
};

entity_id tile_at(world& w, entity_id body, int c, int r)
{
    const int gw = w.bodies.at(body).grid_width;
    return body_tile_grid(w, body)[static_cast<std::size_t>(r) * static_cast<std::size_t>(gw)
                                   + static_cast<std::size_t>(c)];
}

/// The general fixture (BL-1186): water at every row of `water_cols`, the source
/// market + the corp's extraction site at column `src_col`, the destination
/// market at (`dst_col`, `dst_row`), an active Port on each (column, row) of
/// `port_cells`.
scenario make_world(const std::vector<int>& water_cols, int src_col, int dst_col,
                    const std::vector<std::pair<int, int>>& port_cells, float stock = 100.0f,
                    float balance = 1000.0f, int dst_row = 0,
                    const std::vector<std::pair<int, int>>& land_cells = {})
{
    scenario s;

    s.body = s.w.create_entity();
    body_component bc{};
    bc.name              = "Isleward";
    bc.type              = body_type::planet;
    bc.orbital_radius_au = 1.0f;
    bc.grid_width        = 32; // physical-scale sanity, as convoy_command.cpp
    bc.grid_height       = 4;
    s.w.bodies[s.body] = bc;
    for (int r = 0; r < bc.grid_height; ++r)
        for (int c = 0; c < bc.grid_width; ++c)
        {
            const entity_id t = s.w.create_entity();
            tile_component tc{};
            tc.body        = s.body;
            tc.grid_x      = c;
            tc.grid_y      = r;
            tc.landform    = terrain_landform::plains;
            // Columns 1-2 are open ocean for EVERY row, not just row 0 — a
            // vertical band the A* search cannot detour around by dropping to
            // another row (it CAN wrap the east-west cylinder instead, but
            // that costs ~30 land edges versus 3 short water ones, so it never
            // wins). A single water row left every other row as a free detour
            // and A* correctly preferred the all-land route around it, so
            // `crosses_ocean` never fired — this fixture makes the crossing
            // genuinely unavoidable.
            const bool water =
                std::find(water_cols.begin(), water_cols.end(), c) != water_cols.end()
                && std::find(land_cells.begin(), land_cells.end(), std::make_pair(c, r))
                       == land_cells.end();
            tc.substrate     = water ? terrain_substrate::ocean : terrain_substrate::sedimentary;
            tc.cover         = terrain_cover::grass;
            tc.cover_density = water ? 0 : 150;
            s.w.tiles[t] = tc;
        }

    s.src_tile = tile_at(s.w, s.body, src_col, 0);
    s.dst_tile = tile_at(s.w, s.body, dst_col, dst_row);

    s.corp = s.w.create_entity();
    corporation_component cc;
    cc.balance   = balance;
    cc.is_player = true; // keep the BL-202 strategic tier out of this harness

    const entity_id bld = s.w.create_entity();
    building_component b{};
    b.tile = s.src_tile;
    b.type = building_type::extraction_site;
    s.w.buildings[bld] = b;
    cc.assets.push_back(bld);

    for (const auto& [pc, pr] : port_cells)
    {
        const entity_id p = s.w.create_entity();
        building_component pb{};
        pb.tile = tile_at(s.w, s.body, pc, pr);
        pb.type = building_type::port;
        s.w.buildings[p] = pb; // built + active: ticks_remaining=0, !decommissioned (defaults)
    }

    s.w.corporations[s.corp] = cc;
    s.w.player_entity = s.corp;

    s.src_market = s.w.create_entity();
    market_component sm{};
    sm.body               = s.body;
    sm.centre_tile        = s.src_tile;
    sm.base_price[r_iron] = 5.0f;
    sm.price              = sm.base_price;
    s.w.markets[s.src_market] = sm;

    s.dst_market = s.w.create_entity();
    market_component dm{};
    dm.body               = s.body;
    dm.centre_tile        = s.dst_tile;
    dm.base_price[r_iron] = 5.0f;
    dm.price              = dm.base_price;
    s.w.markets[s.dst_market] = dm;

    s.w.pool_at(s.corp, pool_key_for_body(s.w, s.body)).quantities[r_iron] = stock;
    return s;
}

/// The original BL-608 fixture: water at columns 1-2, source at 0, destination at
/// 3, a Port on either endpoint as asked. `island` (BL-1186) adds a second water
/// band at columns 16-17, closing the overland detour round the cylinder, so a
/// pair with no port PAIR has no route at all.
scenario make_scenario(bool src_port, bool dst_port, float stock = 100.0f,
                       float balance = 1000.0f, bool island = true)
{
    std::vector<int> water = {1, 2};
    if (island)
    {
        water.push_back(16);
        water.push_back(17);
    }
    std::vector<std::pair<int, int>> ports;
    if (src_port)
        ports.push_back({0, 0});
    if (dst_port)
        ports.push_back({3, 0});
    return make_world(water, 0, 3, ports, stock, balance);
}

corp_command dispatch_cmd(const scenario& s, float qty)
{
    corp_command cmd;
    cmd.corp         = s.corp;
    cmd.verb         = corp_verb::dispatch_convoy;
    cmd.subject      = s.src_market;
    cmd.counterparty = s.dst_market;
    cmd.target       = resource_type::iron_ore;
    cmd.quantity     = qty;
    return cmd;
}

float pool_iron(const scenario& s)
{
    const auto it = s.w.corp_market_pools.find({s.corp, pool_key_for_body(s.w, s.body)});
    return it != s.w.corp_market_pools.end() ? it->second.quantities[r_iron] : 0.0f;
}

std::string fingerprint(const world& w)
{
    std::ostringstream o;
    o.precision(9);
    std::vector<entity_id> corp_ids;
    for (const auto& kv : w.corporations) corp_ids.push_back(kv.first);
    std::sort(corp_ids.begin(), corp_ids.end());
    for (const entity_id id : corp_ids)
        o << "C" << id << ':' << w.corporations.at(id).balance << ';';
    for (const auto& [key, sc] : w.corp_market_pools)
    {
        o << "P" << key.first << '/' << key.second << ':';
        for (const float q : sc.quantities) o << q << ',';
        o << ';';
    }
    o << "N" << w.next_convoy_id << ';';
    for (const convoy_component& c : w.convoys)
        o << "V" << c.id << ':' << c.source_market << '>' << c.dest_market << ':'
          << static_cast<int>(c.mode) << ':' << c.cargo_qty << ':' << c.cost_paid << ';';
    return o.str();
}

/// BL-1195: a hostile raider corporation with one unit standing on `tile`.
entity_id add_raider(scenario& s, entity_id tile)
{
    const entity_id raider = s.w.create_entity();
    corporation_component rc;
    rc.balance = 100.0f;
    s.w.corporations[raider] = rc;
    declare_hostile(s.w, raider, s.corp);
    const entity_id u = s.w.create_entity();
    unit_component uc{};
    uc.position = tile;
    uc.owner    = raider;
    uc.count    = 3;
    s.w.units[u] = uc;
    return raider;
}

bool tile_is_water(const world& w, entity_id t)
{
    const auto it = w.tiles.find(t);
    return it != w.tiles.end() && is_water(it->second.substrate);
}

bool lane_has(const std::vector<entity_id>& lane, entity_id t)
{
    return std::find(lane.begin(), lane.end(), t) != lane.end();
}

/// Walk the one convoy's head across the whole lane in fine steps; true when any
/// step is intercepted. The lane is re-read each step from the convoy.
bool cut_anywhere(scenario& s)
{
    for (int k = 0; k <= 400 && !s.w.convoys.empty(); ++k)
    {
        s.w.convoys.front().progress = static_cast<float>(k) / 400.0f;
        if (!intercept_convoys(s.w, k).empty())
            return true;
    }
    return false;
}

/// Runs the full R0/R1/R2 scripted sequence once, returning a fingerprint of
/// everything it touched (for R3's cross-run determinism check).
std::string run_sequence(const recipe_registry& reg)
{
    std::ostringstream trace;

    // R0 — both endpoints have an active Port: sea mode, normal dispatch.
    {
        scenario s = make_scenario(/*src_port=*/true, /*dst_port=*/true);
        const float bal_before = s.w.corporations.at(s.corp).balance;
        const corp_command_result r = apply_corp_command(s.w, reg, dispatch_cmd(s, 25.0f));
        check(r == corp_command_result::applied,
              "R0.1 both endpoints Port-equipped: dispatch_convoy applies");
        check(s.w.convoys.size() == 1, "R0.2 exactly one convoy is created");
        if (s.w.convoys.size() == 1)
        {
            const convoy_component& c = s.w.convoys.front();
            check(c.mode == convoy_mode::sea,
                  "R0.3 a water-crossing path with both Ports gated is SEA mode");
            check(c.cost_paid > 0.0f &&
                  std::fabs((bal_before - s.w.corporations.at(s.corp).balance) - c.cost_paid) < 1e-4f,
                  "R0.4 the sea-rate haul cost is recorded and debited");
            // sea unit cost .05 (recipe_registry default) x A* cost. Path crosses
            // 3 edges: land(1.0)-ocean(2.5) = 1.75, ocean-ocean = 2.5,
            // ocean(2.5)-land(1.0) = 1.75, edge cost = average of the two
            // endpoints (logistics.cpp): 1.75 + 2.5 + 1.75 = 6.0. BL-1186: plus
            // HANDLING at both ports (registry default 0.10 per unit per port);
            // both land legs are zero-length (each market sits on its port).
            check(std::fabs(c.cost_paid - (reg.logistics_cost(convoy_mode::sea) * 6.0f
                                           + 2.0f * reg.port_handling()) * 25.0f) < 1e-2f,
                  "R0.5 cost = (sea_unit_cost x A*(water-weighted) + 2 x handling) x qty");
            trace << "R0:" << static_cast<int>(c.mode) << ':' << c.cost_paid << ';';
        }
        check(std::fabs(pool_iron(s) - 75.0f) < 1e-4f, "R0.6 source pool debited by exactly 25");
    }

    // R1 — missing a Port at either endpoint, on the island: no port PAIR and no
    // overland road, so the haul is refused, mutating nothing.
    for (const auto& [src_port, dst_port, label] :
         { std::tuple{true, false, "dest missing"}, std::tuple{false, true, "source missing"},
           std::tuple{false, false, "neither present"} })
    {
        scenario s = make_scenario(src_port, dst_port);
        const std::string before = fingerprint(s.w);
        const corp_command_result r = apply_corp_command(s.w, reg, dispatch_cmd(s, 25.0f));
        const std::string after = fingerprint(s.w);
        const bool ok = r == corp_command_result::rejected_placement && before == after;
        std::string what = std::string("R1 ungated sea leg (") + label
                          + ") is rejected_placement and mutates nothing";
        check(ok, what.c_str());
        trace << "R1[" << label << "]:" << static_cast<int>(r) << ';';
    }

    // R2 — a decommissioned Port does not count as active.
    {
        scenario s = make_scenario(/*src_port=*/true, /*dst_port=*/true);
        // Decommission the destination's Port.
        for (auto& [bid, bc] : s.w.buildings)
            if (bc.tile == s.dst_tile && bc.type == building_type::port)
                bc.decommissioned = true;
        const std::string before = fingerprint(s.w);
        const corp_command_result r = apply_corp_command(s.w, reg, dispatch_cmd(s, 25.0f));
        const std::string after = fingerprint(s.w);
        check(r == corp_command_result::rejected_placement && before == after,
              "R2.1 a decommissioned destination Port does not gate sea mode open");
        trace << "R2:" << static_cast<int>(r) << ';';
    }
    {
        scenario s = make_scenario(/*src_port=*/true, /*dst_port=*/true);
        // Destination Port still under construction.
        for (auto& [bid, bc] : s.w.buildings)
            if (bc.tile == s.dst_tile && bc.type == building_type::port)
                bc.ticks_remaining = 5;
        const std::string before = fingerprint(s.w);
        const corp_command_result r = apply_corp_command(s.w, reg, dispatch_cmd(s, 25.0f));
        const std::string after = fingerprint(s.w);
        check(r == corp_command_result::rejected_placement && before == after,
              "R2.2 an unbuilt (under-construction) destination Port does not gate sea mode open");
        trace << "R2b:" << static_cast<int>(r) << ';';
    }

    // R4 — BL-1186 B2: no port pair, but the overland detour round the cylinder
    // is open (no second band). The cheapest path still crosses water; the haul
    // is routed overland instead of refused. The source Port is there only as a
    // passive-LP anchor (no anchor, no passive LP); one port is no crossing.
    {
        scenario s = make_scenario(/*src_port=*/true, /*dst_port=*/false, 100.0f, 1000.0f,
                                   /*island=*/false);
        const float bal_before = s.w.corporations.at(s.corp).balance;
        const corp_command_result r = apply_corp_command(s.w, reg, dispatch_cmd(s, 25.0f));
        check(r == corp_command_result::applied,
              "R4.1 no port pair but an overland road: dispatch_convoy applies (B2)");
        if (s.w.convoys.size() == 1)
        {
            const convoy_component& c = s.w.convoys.front();
            check(c.mode == convoy_mode::land, "R4.2 the fallback route is LAND mode");
            // Westward round the 32-column cylinder: columns 0 -> 31 -> ... -> 4 -> 3,
            // 29 plains edges of 1.0, at the land rate, no handling.
            check(std::fabs(c.cost_paid - reg.logistics_cost(convoy_mode::land) * 29.0f * 25.0f) < 1e-2f,
                  "R4.3 cost = land_unit_cost x land-only path (29) x qty");
            check(std::fabs((bal_before - s.w.corporations.at(s.corp).balance) - c.cost_paid) < 1e-4f,
                  "R4.4 the land haul cost is recorded and debited");
            trace << "R4:" << static_cast<int>(c.mode) << ':' << c.cost_paid << ';';
        }
        else
            check(false, "R4.2 exactly one convoy is created");
    }

    // R5 — BL-1186 B1: both markets INLAND, Ports on the coast between them. Water
    // at columns 3-4 (and 16-17, closing the detour); source at 0, Port at 2,
    // Port at 5, destination at 8. The old gate demanded a Port on each market
    // centre and refused this; the route is now three legs.
    {
        scenario s = make_world({3, 4, 16, 17}, 0, 8, {{2, 0}, {5, 0}});
        const corp_command_result r = apply_corp_command(s.w, reg, dispatch_cmd(s, 25.0f));
        check(r == corp_command_result::applied,
              "R5.1 inland markets, coastal Ports: dispatch_convoy applies (B1)");
        if (s.w.convoys.size() == 1)
        {
            const convoy_component& c = s.w.convoys.front();
            check(c.mode == convoy_mode::sea, "R5.2 a route with a sea leg is SEA mode");
            // land 0 -> 2: 2 plains edges = 2.0 at the land rate;
            // sea 2 -> 5: 1.75 + 2.5 + 1.75 = 6.0 at the sea rate;
            // land 5 -> 8: 3 plains edges = 3.0 at the land rate;
            // plus handling at both ports.
            const float want = (reg.logistics_cost(convoy_mode::land) * (2.0f + 3.0f)
                                + reg.logistics_cost(convoy_mode::sea) * 6.0f
                                + 2.0f * reg.port_handling()) * 25.0f;
            check(std::fabs(c.cost_paid - want) < 1e-2f,
                  "R5.3 cost = land legs at the land rate + sea leg at the sea rate + 2 x handling");
            trace << "R5:" << static_cast<int>(c.mode) << ':' << c.cost_paid << ';';
        }
        else
            check(false, "R5.2 exactly one convoy is created");
    }

    // R6 — the ports are CHOSEN to minimise the whole route. Water 3-4 (and 16-17);
    // source (0,0), destination (8,3); Ports at (2,0) on the near shore and at (5,0)
    // and (5,3) on the far one. Via (5,3): sea 2 -> (5,3) runs down the channel,
    // 1.75 + 4 x 2.5 + 1.75 = 13.5, then land 3.0. Via (5,0): sea 6.0, then land
    // (5,0) -> (8,3) = 6.0. At land 0.02 / sea 0.05 the second is cheaper
    // (0.16 + 0.30 vs 0.10 + 0.675 per unit before fees), and it is taken.
    // BL-1194: the SHIPPED sea rate (0.002, below a highway tile) makes the long
    // channel the cheaper whole route here, and so the port nearest the destination
    // — which would no longer tell "whole route" from "nearest". The fixture tests
    // the router's selection, not the rate, so it pins its own: sea 0.05.
    {
        recipe_registry reg6 = reg;
        reg6.set_logistics_cost(convoy_mode::sea, 0.05f);
        const recipe_registry& reg = reg6; // R6's rates, shadowing the shipped ones
        scenario s = make_world({3, 4, 16, 17}, 0, 8, {{2, 0}, {5, 0}, {5, 3}}, 100.0f,
                                1000.0f, /*dst_row=*/3);
        const corp_command_result r = apply_corp_command(s.w, reg, dispatch_cmd(s, 25.0f));
        check(r == corp_command_result::applied, "R6.1 two far-shore Ports: dispatch applies");
        if (s.w.convoys.size() == 1)
        {
            const convoy_component& c = s.w.convoys.front();
            const float want = (reg.logistics_cost(convoy_mode::land) * (2.0f + 6.0f)
                                + reg.logistics_cost(convoy_mode::sea) * 6.0f
                                + 2.0f * reg.port_handling()) * 25.0f;
            check(c.mode == convoy_mode::sea && std::fabs(c.cost_paid - want) < 1e-2f,
                  "R6.2 the port pair minimising the WHOLE route is taken, not the nearest "
                  "to the destination");
            trace << "R6:" << c.cost_paid << ';';
        }
        else
            check(false, "R6.2 exactly one convoy is created");
    }

    // R7 — a sea leg never runs overland, and a land leg never swims. Water 3-4
    // (and 16-17); source 0, destination 9; Ports at 2 and 7 — 7 is inland (columns
    // 5-6 are land), so no sea leg reaches it. Refused, nothing mutated.
    {
        scenario s = make_world({3, 4, 16, 17}, 0, 9, {{2, 0}, {7, 0}});
        const std::string before = fingerprint(s.w);
        const corp_command_result r = apply_corp_command(s.w, reg, dispatch_cmd(s, 25.0f));
        const std::string after = fingerprint(s.w);
        check(r == corp_command_result::rejected_placement && before == after,
              "R7 a Port off the water is no sea end: refused, nothing mutated");
        trace << "R7:" << static_cast<int>(r) << ';';
    }

    // R8 — a Port coastal only across a hex DIAGONAL is a sea end. Water in column
    // 4 and in column 3 except (3,1), which stays land (and 16-17). The Port at
    // (2,1) — an odd row — touches water only at its NE (3,0) and SE (3,2) hex
    // sides, which the four-way flood never steps; placement's six-side
    // `is_coastal` accepts it. Route: land (0,0) -> (2,1) 3.0; sea (2,1) -> (3,0)
    // -> (4,0) -> (5,0) = 1.75 + 2.5 + 1.75 = 6.0; land (5,0) -> (8,0) 3.0.
    {
        scenario s = make_world({3, 4, 16, 17}, 0, 8, {{2, 1}, {5, 0}}, 100.0f, 1000.0f,
                                /*dst_row=*/0, /*land_cells=*/{{3, 1}});
        const corp_command_result r = apply_corp_command(s.w, reg, dispatch_cmd(s, 25.0f));
        check(r == corp_command_result::applied,
              "R8.1 a Port whose sea lies only across a hex diagonal is a usable sea end");
        if (s.w.convoys.size() == 1)
        {
            const convoy_component& c = s.w.convoys.front();
            const float want = (reg.logistics_cost(convoy_mode::land) * (3.0f + 3.0f)
                                + reg.logistics_cost(convoy_mode::sea) * 6.0f
                                + 2.0f * reg.port_handling()) * 25.0f;
            check(c.mode == convoy_mode::sea && std::fabs(c.cost_paid - want) < 1e-2f,
                  "R8.2 the diagonal port hop is priced as an ordinary land<->water edge");
            trace << "R8:" << c.cost_paid << ';';
        }
        else
            check(false, "R8.2 exactly one convoy is created");
    }

    // R9 — a LAKE is no sea lane. R5's fixture with the crossing (columns 3-4)
    // turned to lake: ports gate on the SEA (`is_coastal` reads is_sea), so a sea
    // leg may not sail a lake, a land leg may not swim it, and the ocean bands at
    // 16-17 close the detour. Refused, nothing mutated.
    {
        scenario s = make_world({3, 4, 16, 17}, 0, 8, {{2, 0}, {5, 0}});
        for (auto& [tid, tc] : s.w.tiles)
            if (tc.body == s.body && (tc.grid_x == 3 || tc.grid_x == 4))
                tc.substrate = terrain_substrate::lake;
        const std::string before = fingerprint(s.w);
        const corp_command_result r = apply_corp_command(s.w, reg, dispatch_cmd(s, 25.0f));
        const std::string after = fingerprint(s.w);
        check(r == corp_command_result::rejected_placement && before == after,
              "R9 a crossing by LAKE is no sea leg: refused, nothing mutated");
        trace << "R9:" << static_cast<int>(r) << ';';
    }

    // R10-R13 — BL-1195: THE LANE IS THE LEGS (SUPPLY.md § Logistical cost). R5's
    // crossing, but the Ports sit on row 3 while both markets sit on row 0, so the
    // routed legs (land (0,0) -> port (2,3), sea along row 3, land (5,3) -> (8,0))
    // and the direct centre-to-centre path (straight along row 0, through the water
    // at (3,0)/(4,0)) are different ground. Every reader of a convoy's position
    // must walk the legs.
    {
        const auto sea_fixture = [] {
            return make_world({3, 4, 16, 17}, 0, 8, {{2, 3}, {5, 3}});
        };
        scenario s = sea_fixture();
        const corp_command_result r = apply_corp_command(s.w, reg, dispatch_cmd(s, 25.0f));
        check(r == corp_command_result::applied && s.w.convoys.size() == 1
                  && s.w.convoys.front().mode == convoy_mode::sea,
              "R10.0 setup: the row-3 port pair carries the haul by sea");
        if (s.w.convoys.size() == 1)
        {
            const entity_id pa = tile_at(s.w, s.body, 2, 3);
            const entity_id pb = tile_at(s.w, s.body, 5, 3);
            const convoy_component& c = s.w.convoys.front();
            check(c.origin_tile == s.src_tile && c.port_a == pa && c.port_b == pb,
                  "R10.1 dispatch records the route's origin and its two Ports on the convoy");
            const convoy_route lane = convoy_route_tiles(s.w, c);
            const std::vector<entity_id> direct =
                intra_body_path(s.w, s.body, s.src_tile, s.dst_tile).tiles;
            entity_id direct_water = null_entity;
            for (const entity_id t : direct)
                if (tile_is_water(s.w, t) && !lane_has(lane.tiles, t))
                {
                    direct_water = t;
                    break;
                }
            check(direct_water != null_entity,
                  "R10.2 precondition: the direct path crosses water the legs never enter");
            bool water_on_row3 = true;
            for (const entity_id t : lane.tiles)
                if (tile_is_water(s.w, t) && s.w.tiles.at(t).grid_y != 3)
                    water_on_row3 = false;
            check(!lane.tiles.empty() && lane.tiles.front() == s.src_tile
                      && lane.tiles.back() == s.dst_tile && lane_has(lane.tiles, pa)
                      && lane_has(lane.tiles, pb)
                      && lane_has(lane.tiles, tile_at(s.w, s.body, 3, 3))
                      && lane_has(lane.tiles, tile_at(s.w, s.body, 4, 3)) && water_on_row3,
                  "R10.3 the lane runs origin -> port A -> the sea leg's water -> port B -> "
                  "destination, not the straight line");
            bool joined = true; // consecutive tiles are distinct: a port is written once
            for (std::size_t i = 1; i < lane.tiles.size(); ++i)
                if (lane.tiles[i] == lane.tiles[i - 1])
                    joined = false;
            check(joined, "R10.4 the legs join at each Port without repeating it");

            // R11 + R13: a hostile unit on the INLAND leg past the far port, on
            // ground the direct path never crosses, intercepts; the capture lands
            // on land and credits that tile's pool.
            std::size_t ib = 0;
            for (std::size_t i = 0; i < lane.tiles.size(); ++i)
                if (lane.tiles[i] == pb)
                    ib = i;
            entity_id inland = null_entity;
            std::size_t ii = 0;
            for (std::size_t i = ib + 1; i + 1 < lane.tiles.size(); ++i)
                if (!lane_has(direct, lane.tiles[i]))
                {
                    inland = lane.tiles[i];
                    ii = i;
                    break;
                }
            check(inland != null_entity && !tile_is_water(s.w, inland),
                  "R11.0 precondition: the far land leg has a tile off the direct path");
            if (inland != null_entity)
            {
                const entity_id raider = add_raider(s, inland);
                s.w.convoys.front().progress =
                    static_cast<float>(ii) / static_cast<float>(lane.tiles.size() - 1);
                const std::vector<interception_record> cuts = intercept_convoys(s.w, 1);
                check(cuts.size() == 1 && cuts[0].tile == inland && s.w.convoys.empty(),
                      "R11 a hostile unit on the sea route's inland leg intercepts it");
                if (cuts.size() == 1)
                {
                    const auto pit =
                        s.w.corp_market_pools.find({raider, pool_key_for_tile(s.w, inland)});
                    const float got = pit != s.w.corp_market_pools.end()
                                          ? pit->second.quantities[r_iron] : 0.0f;
                    check(cuts[0].outcome == interception_outcome::captured
                              && !tile_is_water(s.w, cuts[0].tile) && std::fabs(got - 25.0f) < 1e-4f,
                          "R13 the capture credits the raider's pool at a LAND tile, whole");
                    trace << "R11:" << cuts[0].tile << ':' << got << ';';
                }
            }

            // R12: one placed on the straight-line water the convoy never enters
            // does not intercept, wherever the head is.
            if (direct_water != null_entity)
            {
                scenario s2 = sea_fixture();
                apply_corp_command(s2.w, reg, dispatch_cmd(s2, 25.0f));
                add_raider(s2, direct_water);
                const bool cut = cut_anywhere(s2);
                check(!s2.w.convoys.empty() && !cut,
                      "R12 a hostile unit on the direct path's water never intercepts a convoy "
                      "whose legs go round it");
                trace << "R12:" << cut << ';';
            }
        }
    }

    // R14 — BL-1195: a LAND-FALLBACK route (R4: no port pair, the overland road
    // round the cylinder) is laid on that road. Its lane holds no water; a hostile
    // unit on the water the direct path would cross never intercepts it, and one
    // on the road it does take does.
    {
        const auto land_fixture = [] {
            return make_scenario(/*src_port=*/true, /*dst_port=*/false, 100.0f, 1000.0f,
                                 /*island=*/false);
        };
        scenario s = land_fixture();
        apply_corp_command(s.w, reg, dispatch_cmd(s, 25.0f));
        check(s.w.convoys.size() == 1 && s.w.convoys.front().mode == convoy_mode::land
                  && s.w.convoys.front().port_a == null_entity,
              "R14.0 setup: the land fallback is dispatched, with no Ports recorded");
        if (s.w.convoys.size() == 1)
        {
            const convoy_route lane = convoy_route_tiles(s.w, s.w.convoys.front());
            bool dry = !lane.tiles.empty();
            for (const entity_id t : lane.tiles)
                if (tile_is_water(s.w, t))
                    dry = false;
            check(dry && lane.tiles.front() == s.src_tile && lane.tiles.back() == s.dst_tile
                      && lane_has(lane.tiles, tile_at(s.w, s.body, 31, 0)),
                  "R14.1 the land fallback's lane is the overland road round the cylinder");

            scenario s2 = land_fixture();
            apply_corp_command(s2.w, reg, dispatch_cmd(s2, 25.0f));
            add_raider(s2, tile_at(s2.w, s2.body, 1, 0));
            check(!cut_anywhere(s2),
                  "R14.2 a hostile unit on the water the direct path crosses never intercepts it");

            scenario s3 = land_fixture();
            apply_corp_command(s3.w, reg, dispatch_cmd(s3, 25.0f));
            add_raider(s3, tile_at(s3.w, s3.body, 20, 0));
            check(cut_anywhere(s3),
                  "R14.3 a hostile unit on the overland road the convoy takes intercepts it");
            trace << "R14:" << lane.tiles.size() << ';';
        }
    }

    return trace.str();
}

} // namespace

int main()
{
    std::printf("=== sea_port_gate (BL-608 + BL-1186: the Port gates the sea LEG; routes are "
                "legs) ===\n");

    recipe_registry reg;
    {
        // BL-597: a deliberately generous per-anchor LP rate, same reasoning as
        // convoy_command.cpp and unit_march_harness.cpp — this file's subject is
        // BL-608's PORT gate, not BL-597's passive-LP admissibility gate, so the
        // latter is set up never to bind here. No anchor needs planting: R0's
        // Port on `src_tile` IS a supply anchor (LOGISTICS.md § 3, "a city, or a
        // built and active port or inland logistics hub"), so the only thing the
        // default registry lacks is a non-zero rate for it to generate against.
        // R1's port-less cases are refused by `price_convoy_leg` before the LP
        // gate is ever reached, so their `rejected_placement` reason is unchanged.
        military_capability_params mp = reg.military();
        mp.active_lp_per_anchor_tick = 1.0e6f;
        reg.set_military(mp);
    }

    const std::string trace1 = run_sequence(reg);
    const std::string trace2 = run_sequence(reg);
    check(trace1 == trace2, "R3 determinism: two runs of the same sequence agree byte-for-byte");

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
