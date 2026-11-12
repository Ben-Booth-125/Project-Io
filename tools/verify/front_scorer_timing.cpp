// ---------------------------------------------------------------------------
// front_scorer_timing — BL-1643 (missile silos): what one rival scorer quarter costs on a
// large front with batteries, and the state it leaves (a hash, so two builds of the scorer
// can be compared for the same behaviour as well as the time).
// ---------------------------------------------------------------------------
// A 200 x 12 plains cylinder. Rival A holds rows 0-5 (one province), B rows 6-11 (blocks
// of ten columns); the seat holds nothing. A is hostile to B, so its front has a reason.
// A has 60 infantry and 40 artillery (half with a long barrel) assigned to its front, and
// 30 infantry and 10 artillery idle at home - so the scorer prices both the assigned lists
// and every idle unit's join. Three hubs feed A's ground.
//
// Prints the median of five timed quarters (each on a fresh copy of the same world) and the
// state hash after one. No PASS/FAIL beyond the run being deterministic across the five.
//
// Run: node tools/verify/build_harness.js front_scorer_timing --run
// ---------------------------------------------------------------------------

#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/fronts.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/unit_designs.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

namespace {

constexpr int WW = 200, HH = 12;

struct fixture
{
    world w;
    entity_id body = null_entity, A = null_entity, B = null_entity, seat = null_entity;
    std::vector<entity_id> cell;
    entity_id at(int x, int y) const { return cell[static_cast<std::size_t>(y * WW + x)]; }
};

entity_id add_unit(fixture& f, entity_id owner, int x, int y, land_class lc)
{
    const entity_id id = f.w.create_entity();
    unit_component u{};
    u.owner = owner; u.position = f.at(x, y); u.count = 40;
    u.type = static_cast<uint16_t>(std::max(0, modern_row_for(lc)));
    f.w.units[id] = u;
    return id;
}

void build(fixture& f, const recipe_registry& reg)
{
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Long"; bc.type = body_type::planet; bc.grid_width = WW; bc.grid_height = HH;
    w.bodies[f.body] = bc;
    for (int y = 0; y < HH; ++y)
        for (int x = 0; x < WW; ++x)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body = f.body; tc.grid_x = x; tc.grid_y = y;
            tc.substrate = terrain_substrate::rocky; tc.landform = terrain_landform::plains;
            w.tiles[t] = tc;
            f.cell.push_back(t);
        }
    const auto nation = [&](const char* name) {
        const entity_id n = w.create_entity();
        nation_component nc{};
        nc.name = name; nc.treasury = 1.0e6f;
        w.nations[n] = nc;
        return n;
    };
    f.A = nation("Aster"); f.B = nation("Brael"); f.seat = nation("Seat");
    w.player_entity = f.seat;
    for (int y = 0; y < HH; ++y)
        for (int x = 0; x < WW; ++x)
        {
            const entity_id holder = y < 6 ? f.A : f.B;
            w.set_tile_nation(f.at(x, y), holder);
            w.nations.at(holder).tiles.push_back(f.at(x, y));
        }
    province pa; pa.id = f.at(0, 0); pa.body = f.body;
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < WW; ++x)
            pa.tiles.push_back(f.at(x, y));
    std::sort(pa.tiles.begin(), pa.tiles.end());
    w.provinces.provinces.push_back(pa);
    for (int k = 0; k < WW / 10; ++k)
    {
        province p; p.id = f.at(10 * k, 6); p.body = f.body;
        for (int y = 6; y < HH; ++y)
            for (int x = 10 * k; x < 10 * k + 10; ++x)
                p.tiles.push_back(f.at(x, y));
        std::sort(p.tiles.begin(), p.tiles.end());
        w.provinces.provinces.push_back(p);
    }
    for (const province& p : w.provinces.provinces)
        for (const entity_id t : p.tiles)
            w.provinces.tile_province[t] = p.id;
    for (const int x : { 20, 100, 170 })
    {
        const entity_id b = w.create_entity();
        building_component hub{};
        hub.type = building_type::inland_logistics_hub;
        hub.tile = f.at(x, 1);
        w.buildings[b] = hub;
    }
    w.corp_hostile_pairs.insert({ f.A, f.B });
    reanchor_fronts(w);

    std::vector<entity_id> assigned;
    for (int i = 0; i < 60; ++i)
        assigned.push_back(add_unit(f, f.A, (i * 3) % WW, 5, land_class::infantry));
    for (int i = 0; i < 40; ++i)
    {
        const entity_id u = add_unit(f, f.A, (i * 5 + 1) % WW, 3, land_class::artillery);
        if (i % 2 == 0)
        {
            const uint16_t item = kit_item_by_id("art.gun.long_barrel");
            w.units.at(u).kit.fit[kit_catalogue().at(item).slot] = kit_variant_of(item);
        }
        assigned.push_back(u);
    }
    for (int i = 0; i < 30; ++i)
        add_unit(f, f.A, (i * 7) % WW, 1, land_class::infantry);
    for (int i = 0; i < 10; ++i)
        add_unit(f, f.A, (i * 19 + 3) % WW, 0, land_class::artillery);
    const std::vector<front> fs = current_fronts(w, f.A);
    for (const entity_id u : assigned)
    {
        corp_command c; c.corp = f.A; c.as_nation = f.A; c.verb = corp_verb::assign_front;
        c.subject = u; c.tile = fs.empty() ? null_entity : fs.front().key;
        (void)apply_corp_command(w, reg, c);
    }
}

recipe_registry make_registry()
{
    recipe_registry reg;
    military_capability_params mp = reg.military();
    for (int k = 0; k < unit_class_count; ++k)
        mp.march_days_per_cls[static_cast<std::size_t>(k)] = 1.0f;
    for (int c = 0; c < land_class_count; ++c)
        mp.march_days_per_land_class[static_cast<std::size_t>(c)] = 1.0f;
    reg.set_military(mp);
    return reg;
}

} // namespace

int main()
{
    std::printf("=== front scorer timing (BL-1643) ===\n");
    const recipe_registry reg = make_registry();
    fixture base;
    build(base, reg);
    std::printf("  fronts of A: %zu; units %zu\n", current_fronts(base.w, base.A).size(), base.w.units.size());

    std::vector<double> ms;
    uint64_t hash = 0;
    bool same = true;
    for (int rep = 0; rep < 5; ++rep)
    {
        world w = base.w;
        corp_ai_params p; p.cadence_k = 1;
        economy_report r;
        const auto t0 = std::chrono::steady_clock::now();
        run_corp_strategic_step(w, reg, r, k_campaign_settle_ticks, p);
        const auto t1 = std::chrono::steady_clock::now();
        ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        const uint64_t h = w.state_hash(0);
        same = same && (rep == 0 || h == hash);
        hash = h;
    }
    std::sort(ms.begin(), ms.end());
    std::printf("  one scorer quarter: median %.1f ms (min %.1f, max %.1f) over 5\n", ms[2], ms[0], ms[4]);
    std::printf("  state hash after: %016llX\n", static_cast<unsigned long long>(hash));
    std::printf("  %s  the five quarters leave the same state\n", same ? "PASS" : "FAIL");
    return same ? 0 : 1;
}
