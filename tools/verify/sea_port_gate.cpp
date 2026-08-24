// Headless sea-port-gate harness (BL-602; no SDL / Lua / ImGui).
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

/// `src_port` / `dst_port`: whether an active Port sits on the source anchor
/// / destination centre tile respectively. `stock` iron ore in the corp's
/// on-body pool.
scenario make_scenario(bool src_port, bool dst_port, float stock = 100.0f,
                       float balance = 1000.0f)
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
            const bool water = (c == 1 || c == 2);
            tc.substrate     = water ? terrain_substrate::ocean : terrain_substrate::sedimentary;
            tc.cover         = terrain_cover::grass;
            tc.cover_density = water ? 0 : 150;
            s.w.tiles[t] = tc;
        }

    s.src_tile = tile_at(s.w, s.body, 0, 0);
    s.dst_tile = tile_at(s.w, s.body, 3, 0);

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

    if (src_port)
    {
        const entity_id p = s.w.create_entity();
        building_component pb{};
        pb.tile = s.src_tile;
        pb.type = building_type::port;
        s.w.buildings[p] = pb; // built + active: ticks_remaining=0, !decommissioned (defaults)
    }
    if (dst_port)
    {
        const entity_id p = s.w.create_entity();
        building_component pb{};
        pb.tile = s.dst_tile;
        pb.type = building_type::port;
        s.w.buildings[p] = pb;
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

    s.w.pool_for(s.corp, s.body).quantities[r_iron] = stock;
    return s;
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
    const auto it = s.w.corp_body_pools.find({s.corp, s.body});
    return it != s.w.corp_body_pools.end() ? it->second.quantities[r_iron] : 0.0f;
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
    for (const auto& [key, sc] : w.corp_body_pools)
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
            // endpoints (logistics.cpp): 1.75 + 2.5 + 1.75 = 6.0.
            check(std::fabs(c.cost_paid - reg.logistics_cost(convoy_mode::sea) * 6.0f * 25.0f) < 1e-2f,
                  "R0.5 cost = sea_unit_cost x A*(water-weighted) x qty");
            trace << "R0:" << static_cast<int>(c.mode) << ':' << c.cost_paid << ';';
        }
        check(std::fabs(pool_iron(s) - 75.0f) < 1e-4f, "R0.6 source pool debited by exactly 25");
    }

    // R1 — missing a Port at either endpoint refuses, mutating nothing.
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

    return trace.str();
}

} // namespace

int main()
{
    std::printf("=== sea_port_gate (BL-602: the Port building gates the sea leg) ===\n");

    const recipe_registry reg;

    const std::string trace1 = run_sequence(reg);
    const std::string trace2 = run_sequence(reg);
    check(trace1 == trace2, "R3 determinism: two runs of the same sequence agree byte-for-byte");

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
