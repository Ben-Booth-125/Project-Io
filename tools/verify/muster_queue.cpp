// ---------------------------------------------------------------------------
// BL-1435 — the muster queue. No SDL/Lua.
// ---------------------------------------------------------------------------
// MILITARY.md § The muster interface, "Raising takes time" (Ben, 2026-10-27): a
// hire is paid and QUEUED at its base, and becomes a unit when its muster
// completes.
//
//   Q1  A HIRE QUEUES, IT DOES NOT RAISE. hire_unit applies, charges, and leaves one
//       muster_order at the base with the row's muster days — and no unit.
//   Q2  A MUSTER COMPLETES INTO THE UNIT A HIRE USED TO RAISE: on the base's tile,
//       the hiring nation's, 50 men, establishment 50, its muster_base recorded.
//   Q3  LENGTH BY CLASS: a 180-day muster takes two ticks, not one.
//   Q4  A BASE GONE OR DECOMMISSIONED DROPS ITS ORDER, with no unit.
//   Q5  A REFUSED HIRE QUEUES NOTHING and charges nothing.
//   Q6  DETERMINISM: twin worlds hash alike through hires and ticks; an empty queue
//       hashes as a world that never hired.
//   Q7  THE QUEUE SURVIVES A SAVE, by value, and loads into an identical hash.
//
// Run: muster_queue
// ---------------------------------------------------------------------------

#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <cstdio>
#include <sstream>
#include <string>

namespace {

int failures = 0, checks = 0;

void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

struct fixture
{
    world     w;
    entity_id nation = null_entity;
    entity_id tile   = null_entity;
    entity_id base   = null_entity;
};

/// One body, one land tile, one nation with a completed military base on it.
void build(fixture& f, float treasury = 100000.0f)
{
    world& w = f.w;
    const entity_id body = w.create_entity();
    body_component bc{};
    bc.name = "Home"; bc.type = body_type::planet; bc.grid_width = 2; bc.grid_height = 2;
    w.bodies[body] = bc;
    f.tile = w.create_entity();
    tile_component tc{};
    tc.body = body; tc.substrate = terrain_substrate::rocky; tc.landform = terrain_landform::plains;
    w.tiles[f.tile] = tc;

    f.nation = w.create_entity();
    nation_component nc{};
    nc.name = "Hosts"; nc.treasury = treasury;
    f.base = w.create_entity();
    building_component b{};
    b.tile = f.tile; b.type = building_type::military_base; b.ticks_remaining = 0;
    w.buildings[f.base] = b;
    nc.state_assets.push_back(f.base);
    w.nations[f.nation] = nc;
}

corp_command_result hire(fixture& f, const recipe_registry& reg, uint16_t row)
{
    corp_command cmd;
    cmd.tick = 1; cmd.corp = f.nation; cmd.verb = corp_verb::hire_unit;
    cmd.tile = f.tile; cmd.unit_type = row;
    return apply_corp_command(f.w, reg, cmd);
}

std::string to_bytes(const world& w)
{
    std::ostringstream o(std::ios::binary);
    write_world_snapshot(w, o);
    return o.str();
}

} // namespace

int main()
{
    std::printf("=== muster queue (BL-1435) ===\n");
    const recipe_registry reg{};
    constexpr uint16_t levy = 0; // ungated, every band: infantry, 90 days

    std::printf("\nQ1/Q2 — a hire queues, a tick raises it\n");
    {
        fixture f;
        build(f);
        const float before = f.w.nations.at(f.nation).treasury;
        const auto r = hire(f, reg, levy);
        check(r == corp_command_result::applied, "Q1a the hire applies");
        check(f.w.units.empty() && f.w.musters.size() == 1, "Q1b it QUEUES: one muster order and no unit");
        check(f.w.nations.at(f.nation).treasury < before, "Q1c and it is paid now");
        if (!f.w.musters.empty())
        {
            const muster_order& m = f.w.musters.front();
            check(m.base == f.base && m.owner == f.nation && m.unit_type == levy
                      && m.days_total == muster_days_for(levy) && m.days_remaining == m.days_total
                      && m.days_total == 90,
                  "Q1d the order names its base, owner and row, with the row's 90 muster days");
        }
        advance_musters(f.w);
        check(f.w.musters.empty() && f.w.units.size() == 1, "Q2a one tick later the muster has become a unit");
        if (f.w.units.size() == 1)
        {
            const unit_component& u = f.w.units.begin()->second;
            check(u.position == f.tile && u.owner == f.nation && u.type == levy && u.count == hire_batch_manpower
                      && u.establishment == hire_batch_manpower && u.muster_base == f.base
                      && u.readiness_permille == 1000 && u.morale_permille == 1000,
                  "Q2b on the base's tile, the nation's, 50 men raised to 50, its muster base recorded");
        }
    }

    std::printf("\nQ3 — length by class\n");
    {
        int tracked = -1;
        const auto& table = unit_roster_table();
        for (std::size_t i = 0; i < table.size(); ++i)
            if (land_class_for(table[i]) == land_class::armour) { tracked = static_cast<int>(i); break; }
        check(tracked >= 0 && muster_days_for(static_cast<uint16_t>(tracked)) == 180,
              "Q3a an armour row musters 180 days");
        fixture f;
        build(f);
        muster_order m;
        m.id = f.w.next_muster_id++; m.base = f.base; m.owner = f.nation;
        m.unit_type = static_cast<uint16_t>(tracked < 0 ? 0 : tracked);
        m.days_total = m.days_remaining = 180;
        f.w.musters.push_back(m);
        advance_musters(f.w);
        check(f.w.units.empty() && f.w.musters.size() == 1 && f.w.musters.front().days_remaining == 90,
              "Q3b after one tick it is half mustered, no unit yet");
        advance_musters(f.w);
        check(f.w.musters.empty() && f.w.units.size() == 1, "Q3c after two it is a unit");
    }

    std::printf("\nQ4 — a base gone drops its order\n");
    {
        fixture f;
        build(f);
        hire(f, reg, levy);
        f.w.buildings.at(f.base).decommissioned = true;
        advance_musters(f.w);
        check(f.w.musters.empty() && f.w.units.empty(), "Q4a a decommissioned base's order is dropped, no unit");
        fixture g;
        build(g);
        hire(g, reg, levy);
        g.w.buildings.erase(g.base);
        advance_musters(g.w);
        check(g.w.musters.empty() && g.w.units.empty(), "Q4b a demolished base's order is dropped, no unit");
    }

    std::printf("\nQ5 — a refused hire queues nothing\n");
    {
        fixture f;
        build(f, /*treasury=*/0.0f);
        const auto r = hire(f, reg, levy);
        check(r == corp_command_result::rejected_funds && f.w.musters.empty() && f.w.units.empty()
                  && f.w.next_muster_id == 1,
              "Q5a an unaffordable hire is refused and queues nothing, not even an id");
        fixture g;
        build(g);
        g.w.buildings.at(g.base).ticks_remaining = 2; // a base still building
        const auto r2 = hire(g, reg, levy);
        check(r2 == corp_command_result::rejected_placement && g.w.musters.empty(),
              "Q5b a hire at an unfinished base is refused and queues nothing");
    }

    std::printf("\nQ6 — determinism\n");
    {
        fixture a, b, c;
        build(a); build(b); build(c);
        const uint64_t never = c.w.state_hash(0);
        for (fixture* f : { &a, &b }) { hire(*f, reg, levy); hire(*f, reg, levy); }
        check(a.w.state_hash(1) == b.w.state_hash(1), "Q6a twin worlds with the same hires hash alike");
        check(a.w.state_hash(0) != never, "Q6b a queued hire moves the hash");
        advance_musters(a.w); advance_musters(b.w);
        check(a.w.state_hash(2) == b.w.state_hash(2), "Q6c and alike after the musters complete");
        fixture d;
        build(d);
        check(d.w.state_hash(0) == never, "Q6d an empty queue hashes as a world that never hired");
    }

    std::printf("\nQ7 — the queue survives a save\n");
    {
        fixture f;
        build(f);
        hire(f, reg, levy);
        hire(f, reg, levy);
        f.w.musters.back().days_remaining = 45;
        const std::string bytes = to_bytes(f.w);
        world back;
        std::istringstream in(bytes, std::ios::binary);
        const bool ok = read_world_snapshot(back, in);
        check(ok && back.musters.size() == 2 && back.next_muster_id == f.w.next_muster_id
                  && back.musters.back().days_remaining == 45 && back.musters.front().base == f.base
                  && back.state_hash(4) == f.w.state_hash(4) && to_bytes(back) == bytes,
              "Q7a the queue and its next id round-trip by value (hash and bytes equal)");
    }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    std::printf("%s\n", failures == 0 ? "ALL PASS (0 failures)" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
