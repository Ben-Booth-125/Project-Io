// ---------------------------------------------------------------------------
// Air combat (BL-1449; MILITARY.md § Air) and rival air (BL-1451). No SDL.
// ---------------------------------------------------------------------------
// One 80 x 4 body cut into twenty 4-column provinces. Nation A (the seat) holds
// columns 0-39, nation B 40-79. A's airbase stands at column 30, so B's nearest
// province (columns 40-43) lies 10 tiles off and the one at columns 60-63 lies 30.
//
//   A1  HIRE ONLY AT AN AIRBASE: a bare tile, a military base, a rival's airbase and
//       an unfinished airbase are refused (placement); an unknown row and a row the
//       shelves cannot field are refused (invalid); a short treasury is refused
//       (funds); each refusal leaves the world byte-identical. At the airbase the
//       hire applies, pays its credits and goods, and makes a wing in world::wings —
//       never in world::units — mustering for its class's days.
//   A2  ASSIGNMENT: range refusal (placement), hostility refusal for bombing (state),
//       a mission the class does not fly, none-with-provinces, a repeated or unknown
//       province (invalid), another nation's wing (not_owner) — each mutating
//       nothing; an in-range superiority assignment applies; the range query agrees.
//   A3  SUPERIORITY SHARE and its three effects: the share is A's alone (1000/0),
//       then split by power when B flies too; the side that lacks it bombs less, its
//       march slows, and its units are revealed to the holder.
//   A4  AIR DEFENCE shoots every hostile mission over its province; no AD, no loss.
//   A5  TACTICAL PRIORITY: a unit in battle in the province is hit before a lower-id
//       unit standing there out of battle; with no battle, any enemy unit is hit.
//   A6  STRATEGIC: a bomber lowers a hostile building's condition; throughput scales
//       with it; it repairs by drawing goods, and stays damaged without them.
//   A7  ORPHANS: a wing whose airbase is demolished, or shot down to 0 HP, is disbanded.
//   A8  SAVE: the air round-trips byte-identically; a v64 stream reads as no air; a
//       corrupt air section is refused whole.
//   A9  DETERMINISM: twin runs hash and serialise alike; a world with no air hashes
//       as before the air pass ran.
//   A10 DESIGNS: the air catalogue is well-formed; a rival may not design; a locked
//       item is refused; a researched one moves the wing's stats.
//   A11 THE ECONOMY TICK flies the air: run_economy_step's report carries the days.
//   R1-R11 RIVAL AIR (BL-1451): a rival builds, hires and assigns its own air, and
//       each 'not admitted' clause of the AI_OPPONENT.md § 11 grant is a check.
//
// Run: air_combat_harness
// ---------------------------------------------------------------------------

#include "world/air_combat.hpp"
#include "world/battle_system.hpp"
#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/unit_designs.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include "world/placement_rules.hpp"
#include "world/state_industry.hpp"
#include "world/trade.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace {

int failures = 0, checks = 0;

void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

std::string to_bytes(const world& w)
{
    std::ostringstream o(std::ios::binary);
    write_world_snapshot(w, o);
    return o.str();
}

bool from_bytes(const std::string& b, world& w)
{
    std::istringstream in(b, std::ios::binary);
    return read_world_snapshot(w, in);
}

int unit_row(const char* name)
{
    const auto& t = unit_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
        if (std::string(t[i].name) == name) return static_cast<int>(i);
    return -1;
}

uint16_t air_row(air_class c)
{
    const auto& t = air_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
        if (t[i].cls == c) return static_cast<uint16_t>(i);
    return 0xFFFF;
}

constexpr int GW = 80, GH = 4, STRIP = 4;

constexpr resource_type k_goods[] = {
    resource_type::steel, resource_type::coal, resource_type::machinery, resource_type::alloys,
    resource_type::electronics, resource_type::ordnance, resource_type::refined_fuel,
    resource_type::iron_ore, resource_type::petroleum,
};

struct fixture
{
    world     w;
    entity_id body = null_entity;
    entity_id a = null_entity, b = null_entity;
    entity_id a_market = null_entity, b_market = null_entity;
    entity_id airbase = null_entity;     ///< A's, at (30, 1)
    entity_id b_airbase = null_entity;   ///< B's, at (50, 1)
    entity_id mil_base = null_entity;    ///< A's military base, at (28, 1)
    std::vector<entity_id> grid;         ///< row-major tile ids
    std::vector<uint32_t>  strip_id;     ///< province id of each strip
};

entity_id tile(const fixture& f, int c, int r) { return f.grid[static_cast<std::size_t>(r * GW + c)]; }
uint32_t  strip(const fixture& f, int k) { return f.strip_id[static_cast<std::size_t>(k)]; }

entity_id add_building(fixture& f, entity_id owner, int c, int r, building_type t, int ticks = 0)
{
    const entity_id id = f.w.create_entity();
    building_component b{};
    b.tile = tile(f, c, r);
    b.type = t;
    b.ticks_remaining = ticks;
    f.w.buildings[id] = b;
    f.w.nations.at(owner).state_assets.push_back(id);
    return id;
}

entity_id add_market(fixture& f, int c, int r, float shelf)
{
    const entity_id m = f.w.create_entity();
    market_component mc{};
    mc.body = f.body;
    mc.centre_tile = tile(f, c, r);
    for (const resource_type g : k_goods)
    {
        const std::size_t i = static_cast<std::size_t>(g);
        mc.base_price[i] = 10.0f; mc.price[i] = 10.0f; mc.inventory[i] = shelf;
    }
    f.w.markets[m] = mc;
    return m;
}

void build(fixture& f, float shelf = 1000.0f)
{
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Field"; bc.type = body_type::planet; bc.grid_width = GW; bc.grid_height = GH;
    w.bodies[f.body] = bc;
    for (int r = 0; r < GH; ++r)
        for (int c = 0; c < GW; ++c)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body = f.body; tc.grid_x = c; tc.grid_y = r;
            tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass;
            tc.cover_density = 150; tc.landform = terrain_landform::plains; tc.habitability = 0.5f;
            w.tiles[t] = tc;
            f.grid.push_back(t);
        }
    // Twenty vertical strips of four columns: a province each, its id its lowest tile.
    w.provinces.provinces.clear();
    for (int k = 0; k < GW / STRIP; ++k)
    {
        province p;
        p.body = f.body;
        for (int r = 0; r < GH; ++r)
            for (int c = k * STRIP; c < (k + 1) * STRIP; ++c)
                p.tiles.push_back(tile(f, c, r));
        std::sort(p.tiles.begin(), p.tiles.end());
        p.id = p.tiles.front();
        for (const entity_id t : p.tiles)
            w.provinces.tile_province[t] = p.id;
        f.strip_id.push_back(p.id);
        w.provinces.provinces.push_back(std::move(p));
    }

    f.a = w.create_entity();
    nation_component na{}; na.name = "Aerie"; na.treasury = 100000.0f;
    w.nations[f.a] = na;
    f.b = w.create_entity();
    nation_component nb{}; nb.name = "Bastion"; nb.treasury = 100000.0f;
    w.nations[f.b] = nb;
    w.player_entity = f.a;
    for (int r = 0; r < GH; ++r)
        for (int c = 0; c < GW; ++c)
            w.set_tile_nation(tile(f, c, r), c < GW / 2 ? f.a : f.b);
    w.province_holder.assign(w.provinces.provinces.size(), null_entity);
    for (std::size_t k = 0; k < w.province_holder.size(); ++k)
        w.province_holder[k] = (static_cast<int>(k) < GW / STRIP / 2) ? f.a : f.b;

    f.a_market = add_market(f, 30, 1, shelf);
    f.b_market = add_market(f, 50, 1, shelf);
    f.airbase   = add_building(f, f.a, 30, 1, building_type::airbase);
    f.b_airbase = add_building(f, f.b, 50, 1, building_type::airbase);
    f.mil_base  = add_building(f, f.a, 28, 1, building_type::military_base);
}

corp_command_result hire(fixture& f, const recipe_registry& reg, entity_id actor, entity_id t,
                         uint16_t row, entity_id* out = nullptr)
{
    corp_command cmd;
    cmd.tick = 1; cmd.corp = actor; cmd.verb = corp_verb::hire_wing; cmd.tile = t; cmd.unit_type = row;
    return apply_corp_command(f.w, reg, cmd, out);
}

corp_command_result assign(fixture& f, const recipe_registry& reg, entity_id actor, entity_id wing,
                           air_mission m, std::vector<uint32_t> provs)
{
    corp_command cmd;
    cmd.tick = 1; cmd.corp = actor; cmd.verb = corp_verb::assign_air_mission;
    cmd.subject = wing; cmd.order = static_cast<uint32_t>(m);
    for (std::size_t i = 0; i < provs.size() && i < cmd.air_provinces.size(); ++i)
        cmd.air_provinces[i] = provs[i];
    return apply_corp_command(f.w, reg, cmd);
}

/// A ready wing of @p cls for @p owner at @p base, assigned directly (fixture).
entity_id add_wing(fixture& f, entity_id owner, entity_id base, air_class cls, air_mission m,
                   std::vector<uint32_t> provs)
{
    const entity_id id = f.w.create_entity();
    air_wing wg;
    wg.owner = owner; wg.airbase = base; wg.type = air_row(cls);
    wg.mission = m; wg.provinces = std::move(provs);
    std::sort(wg.provinces.begin(), wg.provinces.end());
    f.w.wings.emplace(id, wg);
    return id;
}

entity_id add_unit(fixture& f, entity_id owner, int c, int r, int row, int count = 50)
{
    const entity_id u = f.w.create_entity();
    unit_component uc{};
    uc.position = tile(f, c, r); uc.owner = owner; uc.count = count; uc.establishment = count;
    uc.type = static_cast<uint16_t>(row);
    f.w.units[u] = uc;
    return u;
}

air_day_tick fly(fixture& f, const recipe_registry& reg, int days = 1)
{
    economy_report rep;
    return run_air_missions(f.w, reg, rep, days);
}

template <typename Fn>
void refused(fixture& f, Fn&& fn, corp_command_result want, const std::string& what)
{
    const std::string before = to_bytes(f.w);
    const uint64_t h = f.w.state_hash(1);
    const corp_command_result r = fn();
    check(r == want && to_bytes(f.w) == before && f.w.state_hash(1) == h,
          what + " (got " + std::to_string(static_cast<int>(r)) + ")");
}

float shelf_of(const fixture& f, entity_id m, resource_type r)
{
    return f.w.markets.at(m).inventory[static_cast<std::size_t>(r)];
}

recipe_registry make_registry()
{
    recipe_registry reg;
    // Movement is by the day (BL-1445): the registry's march days default to zero
    // (cannot march), so the fixture authors foot pace for every row it raises.
    military_capability_params mp = reg.military();
    mp.march_days_per_land_class.fill(3.0f);
    mp.march_days_per_cls[static_cast<std::size_t>(unit_class::infantry)] = 3.0f;
    mp.march_days_per_cls[static_cast<std::size_t>(unit_class::cavalry)]  = 3.0f;
    mp.march_days_per_cls[static_cast<std::size_t>(unit_class::ranged)]   = 3.0f;
    mp.march_days_per_cls[static_cast<std::size_t>(unit_class::siege)]    = 3.0f;
    reg.set_military(mp);
    building_economics ex = reg.economics(building_type::extraction_site);
    ex.base_rate = 4.0f;
    reg.set_economics(building_type::extraction_site, ex);
    return reg;
}

} // namespace

int main()
{
    std::printf("=== air combat (BL-1449) and rival air (BL-1451) ===\n");
    const recipe_registry reg = make_registry();
    const int rifle = unit_row("Rifle Infantry");
    const int ad_row = unit_row("Air Defence Battery");
    const uint16_t FTR = air_row(air_class::fighter);
    const uint16_t MR  = air_row(air_class::multirole);
    const uint16_t BMR = air_row(air_class::bomber);
    const uint16_t RCN = air_row(air_class::recon);
    check(rifle >= 0 && ad_row >= 0 && FTR != 0xFFFF && MR != 0xFFFF && BMR != 0xFFFF
              && air_roster_table().size() == 6,
          "A0 the fixture's rows exist; six air rows, one per class");

    // -----------------------------------------------------------------------
    std::printf("\nA1 - hire only at an airbase\n");
    {
        fixture f;
        build(f);
        f.w.nations.at(f.b).treasury = 100000.0f;
        refused(f, [&] { return hire(f, reg, f.a, tile(f, 10, 1), FTR); },
                corp_command_result::rejected_placement, "A1a a bare tile is refused");
        refused(f, [&] { return hire(f, reg, f.a, tile(f, 28, 1), FTR); },
                corp_command_result::rejected_placement, "A1b a military base hosts no wings");
        refused(f, [&] { return hire(f, reg, f.a, tile(f, 50, 1), FTR); },
                corp_command_result::rejected_placement, "A1c a rival's airbase is not a door");
        add_building(f, f.a, 20, 1, building_type::airbase, 2);
        refused(f, [&] { return hire(f, reg, f.a, tile(f, 20, 1), FTR); },
                corp_command_result::rejected_placement, "A1d an unfinished airbase is not a door");
        refused(f, [&] { return hire(f, reg, f.a, tile(f, 30, 1), 99); },
                corp_command_result::rejected_invalid, "A1e an unknown air row is refused");
        {
            fixture g;
            build(g, 0.0f); // empty shelves: no steel, no fuel — the gate shuts
            refused(g, [&] { return hire(g, reg, g.a, tile(g, 30, 1), FTR); },
                    corp_command_result::rejected_invalid, "A1f a row the shelves cannot field is refused");
        }
        f.w.nations.at(f.a).treasury = 10.0f;
        refused(f, [&] { return hire(f, reg, f.a, tile(f, 30, 1), FTR); },
                corp_command_result::rejected_funds, "A1g a short treasury is refused");
        f.w.nations.at(f.a).treasury = 100000.0f;

        const float t0 = f.w.nations.at(f.a).treasury;
        const float al0 = shelf_of(f, f.a_market, resource_type::alloys);
        const std::size_t units0 = f.w.units.size();
        entity_id wid = null_entity;
        const auto r = hire(f, reg, f.a, tile(f, 30, 1), FTR, &wid);
        const air_class_stats& cs = reg.military().air.cls[0];
        check(r == corp_command_result::applied && f.w.wings.count(wid) == 1
                  && f.w.units.size() == units0 && f.w.units.count(wid) == 0,
              "A1h at the airbase the hire applies: a wing in world::wings, none in world::units");
        const air_wing& wg = f.w.wings.at(wid);
        const float paid = t0 - f.w.nations.at(f.a).treasury;
        const float alloys = al0 - shelf_of(f, f.a_market, resource_type::alloys);
        check(wg.airbase == f.airbase && wg.owner == f.a && wg.muster_days == cs.muster_days
                  && paid >= cs.hire_cost - 0.01f && std::fabs(alloys - 4.0f) < 0.01f,
              "A1i it records its airbase, musters for its class's days, pays credits and goods (paid "
                  + std::to_string(paid) + ", alloys " + std::to_string(alloys) + ")");
        // A mustering wing flies nothing; once raised it does.
        corp_command_result ar = assign(f, reg, f.a, wid, air_mission::superiority, {strip(f, 6)});
        const air_day_tick d0 = fly(f, reg);
        f.w.wings.at(wid).muster_days = 0;
        const air_day_tick d1 = fly(f, reg);
        check(ar == corp_command_result::applied && d0.sorties == 0 && d1.sorties == 1,
              "A1j a mustering wing flies nothing; a raised one flies");
    }

    // -----------------------------------------------------------------------
    std::printf("\nA2 - assignment: range, hostility, refusals\n");
    {
        fixture f;
        build(f);
        const entity_id ftr = add_wing(f, f.a, f.airbase, air_class::fighter, air_mission::none, {});
        const entity_id mr  = add_wing(f, f.a, f.airbase, air_class::multirole, air_mission::none, {});
        const entity_id bw  = add_wing(f, f.b, f.b_airbase, air_class::fighter, air_mission::none, {});
        const entity_id bm  = add_wing(f, f.a, f.airbase, air_class::bomber, air_mission::none, {});
        check(air_province_distance(f.w, tile(f, 30, 1), strip(f, 10)) == 10
                  && air_province_distance(f.w, tile(f, 30, 1), strip(f, 15)) == 30,
              "A2a the straight-line distance to a province is to its nearest tile (10 and 30)");
        refused(f, [&] { return assign(f, reg, f.a, ftr, air_mission::superiority, {strip(f, 15)}); },
                corp_command_result::rejected_placement, "A2b a province out of the fighter's range (30 > 15) is refused");
        refused(f, [&] { return assign(f, reg, f.a, bm, air_mission::strategic, {strip(f, 10)}); },
                corp_command_result::rejected_state, "A2c strategic bombing of a nation the owner is not hostile to is refused");
        refused(f, [&] { return assign(f, reg, f.a, bm, air_mission::strategic, {strip(f, 5)}); },
                corp_command_result::rejected_state, "A2d strategic bombing of the owner's own province is refused");
        {
            world keep = f.w;
            const bool own = assign(f, reg, f.a, mr, air_mission::tactical, {strip(f, 5)})
                             == corp_command_result::applied;
            f.w = keep;
            check(own, "A2d2 tactical bombing may fly over the owner's own province (close air support)");
        }
        refused(f, [&] { return assign(f, reg, f.a, ftr, air_mission::tactical, {strip(f, 6)}); },
                corp_command_result::rejected_invalid, "A2e a mission the class does not fly is refused");
        refused(f, [&] { return assign(f, reg, f.a, ftr, air_mission::none, {strip(f, 6)}); },
                corp_command_result::rejected_invalid, "A2f none with provinces is refused");
        refused(f, [&] { return assign(f, reg, f.a, ftr, air_mission::superiority, {}); },
                corp_command_result::rejected_invalid, "A2g a mission with no province is refused");
        refused(f, [&] { return assign(f, reg, f.a, ftr, air_mission::superiority, {strip(f, 6), strip(f, 6)}); },
                corp_command_result::rejected_invalid, "A2h a repeated province is refused");
        refused(f, [&] { return assign(f, reg, f.a, ftr, air_mission::superiority, {12345678u}); },
                corp_command_result::rejected_invalid, "A2i an unknown province is refused");
        refused(f, [&] { return assign(f, reg, f.a, bw, air_mission::superiority, {strip(f, 12)}); },
                corp_command_result::rejected_not_owner, "A2j another nation's wing is refused");
        refused(f, [&] { return assign(f, reg, f.a, ftr, static_cast<air_mission>(9), {strip(f, 6)}); },
                corp_command_result::rejected_invalid, "A2k an unknown mission is refused");
        check(assign(f, reg, f.a, ftr, air_mission::superiority, {strip(f, 10), strip(f, 6)})
                      == corp_command_result::applied
                  && f.w.wings.at(ftr).provinces == std::vector<uint32_t>({strip(f, 6), strip(f, 10)}),
              "A2l an in-range superiority assignment applies, provinces sorted");
        refused(f, [&] { return assign(f, reg, f.a, ftr, air_mission::superiority, {strip(f, 6), strip(f, 10)}); },
                corp_command_result::rejected_state, "A2m the same assignment again changes nothing");
        declare_hostile(f.w, f.a, f.b);
        check(assign(f, reg, f.a, mr, air_mission::tactical, {strip(f, 10)}) == corp_command_result::applied
                  && assign(f, reg, f.a, bm, air_mission::strategic, {strip(f, 10)}) == corp_command_result::applied,
              "A2n once hostile, tactical and strategic bombing over the enemy's province apply");
        const std::vector<uint32_t> in = air_provinces_in_range(f.w, reg, ftr);
        bool agree = !in.empty();
        for (std::size_t k = 0; k < f.strip_id.size(); ++k)
        {
            const bool listed = std::find(in.begin(), in.end(), strip(f, static_cast<int>(k))) != in.end();
            agree = agree && listed == air_province_in_range(f.w, reg, f.w.wings.at(ftr), strip(f, static_cast<int>(k)));
        }
        check(agree, "A2o the range query lists exactly the provinces the verb admits");
    }

    // -----------------------------------------------------------------------
    std::printf("\nA3 - superiority share and its three effects\n");
    {
        fixture f;
        build(f);
        declare_hostile(f.w, f.a, f.b);
        declare_hostile(f.w, f.b, f.a);
        const uint32_t p10 = strip(f, 10);
        add_wing(f, f.a, f.airbase, air_class::fighter, air_mission::superiority, {p10});
        const entity_id bu = add_unit(f, f.b, 42, 1, rifle);
        fly(f, reg);
        check(air_share_permille(f.w, p10, f.a) == 1000 && air_share_permille(f.w, p10, f.b) == 0,
              "A3a A alone flies superiority: A's share 1000, B's 0");
        check(f.w.air_revealed.count({f.a, bu}) == 1 && f.w.air_revealed.count({f.b, bu}) == 0,
              "A3b effect 3: B's unit in the province is revealed to A, the holder");
        check(air_march_cost_permille(f.w, reg, f.b, tile(f, 41, 1)) == 1500
                  && air_march_cost_permille(f.w, reg, f.a, tile(f, 41, 1)) == 1000
                  && air_march_cost_permille(f.w, reg, f.b, tile(f, 46, 1)) == 1000,
              "A3c effect 2: a step into the province costs B 1.5x, A and elsewhere 1x");

        // The march itself: B marches west through strip 11 into strip 10, against a
        // control world with no air. The slowed column gets less far.
        auto march_west = [&](fixture& g) {
            const entity_id u = add_unit(g, g.b, 47, 2, rifle);
            corp_command cmd;
            cmd.tick = 1; cmd.corp = g.b; cmd.verb = corp_verb::march_unit; cmd.subject = u;
            cmd.tile = tile(g, 40, 2);
            const auto r = apply_corp_command(g.w, reg, cmd);
            // Movement is by the day (BL-1445): 18 days of marching, a call a day.
            for (int day = 0; day < 18; ++day)
                run_unit_march(g.w, reg, day);
            return std::make_pair(r, g.w.tiles.at(g.w.units.at(u).position).grid_x);
        };
        fixture ctl;
        build(ctl);
        const auto slowed = march_west(f);
        const auto plain  = march_west(ctl);
        check(slowed.first == corp_command_result::applied && plain.first == corp_command_result::applied
                  && plain.second < slowed.second,
              "A3d the march slows: the column under A's superiority ends at column "
                  + std::to_string(slowed.second) + ", the control at " + std::to_string(plain.second));

        // B flies fighters too: the share splits by power (equal wings, equal share).
        add_wing(f, f.b, f.b_airbase, air_class::fighter, air_mission::superiority, {p10});
        fly(f, reg);
        const int sa = air_share_permille(f.w, p10, f.a), sb = air_share_permille(f.w, p10, f.b);
        check(sa + sb >= 998 && sa + sb <= 1000 && sa > 0 && sb > 0,
              "A3e both fly: the share splits by power (A " + std::to_string(sa) + ", B " + std::to_string(sb) + ")");

        // Effect 1: B's strike wing bombing A's province hits less when A holds the air.
        auto bomb_with = [&](bool a_fighters) {
            fixture g;
            build(g);
            declare_hostile(g.w, g.a, g.b);
            declare_hostile(g.w, g.b, g.a);
            const uint32_t p9 = strip(g, 9);
            if (a_fighters)
                for (int k = 0; k < 3; ++k)
                    add_wing(g, g.a, g.airbase, air_class::fighter, air_mission::superiority, {p9});
            add_wing(g, g.b, g.b_airbase, air_class::multirole, air_mission::tactical, {p9});
            const entity_id target = add_unit(g, g.a, 37, 1, rifle, 50);
            fly(g, reg, 1);
            const auto bit = g.w.air_hit_bank.find(target);
            const int64_t banked = bit == g.w.air_hit_bank.end() ? 0
                : int64_t{bit->second.first} + bit->second.second;
            return banked + int64_t{50 - g.w.units.at(target).count} * 400;
        };
        const int64_t free_hits = bomb_with(false), contested = bomb_with(true);
        check(free_hits > 0 && contested > 0 && contested < free_hits,
              "A3f effect 1: the side lacking superiority bombs less (" + std::to_string(contested)
                  + " against " + std::to_string(free_hits) + " milli-hits a day)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nA4 - air defence shoots every mission\n");
    {
        auto hp_after = [&](bool with_ad, air_mission m) {
            fixture g;
            build(g);
            declare_hostile(g.w, g.a, g.b);
            const uint32_t p10 = strip(g, 10);
            const air_class cls = m == air_mission::recon ? air_class::recon : air_class::fighter;
            const entity_id wid = add_wing(g, g.a, g.airbase, cls, m, {p10});
            if (with_ad)
                add_unit(g, g.b, 42, 2, ad_row);
            const air_day_tick t = fly(g, reg, 10);
            return std::make_pair(g.w.wings.at(wid).hp_ppm, t.ad_shots);
        };
        const auto bare = hp_after(false, air_mission::superiority);
        const auto shot = hp_after(true, air_mission::superiority);
        const auto rshot = hp_after(true, air_mission::recon);
        check(bare.first == k_wing_hp_full && bare.second == 0, "A4a no air defence, no enemy fighters: no loss");
        check(shot.first < k_wing_hp_full && shot.second == 10,
              "A4b air defence shoots the superiority mission each day (HP ppm " + std::to_string(shot.first) + ")");
        check(rshot.first < k_wing_hp_full && rshot.second == 10, "A4c and the recon mission too");
    }

    // -----------------------------------------------------------------------
    std::printf("\nA5 - tactical bombing: units in battle first\n");
    {
        fixture f;
        build(f);
        declare_hostile(f.w, f.a, f.b);
        const uint32_t p10 = strip(f, 10);
        const entity_id idle   = add_unit(f, f.b, 43, 3, rifle); // lower id, out of battle
        const entity_id engaged = add_unit(f, f.b, 41, 1, rifle);
        const entity_id att    = add_unit(f, f.a, 40, 1, rifle);
        const bool opened = open_tile_battle(f.w, 0, tile(f, 41, 1), f.a, f.b, {att});
        add_wing(f, f.a, f.airbase, air_class::multirole, air_mission::tactical, {p10});
        fly(f, reg, 1);
        check(opened && f.w.air_hit_bank.count(engaged) == 1 && f.w.air_hit_bank.count(idle) == 0,
              "A5a the unit in battle is hit before the lower-id unit out of battle");
        check(f.w.air_hit_bank.count(att) == 0, "A5b the wing's own side is never hit");
        f.w.battles.clear();
        f.w.air_hit_bank.clear();
        fly(f, reg, 1);
        check(f.w.air_hit_bank.count(idle) == 1 && f.w.air_hit_bank.count(engaged) == 0,
              "A5c with no battle, the lowest-id enemy unit in the province is hit");
        const int men0 = f.w.units.at(idle).count, rd0 = f.w.units.at(idle).readiness_permille;
        // RE-SCOPED (Ben, 2026-10-29: bombing a unit takes the land round's day scale,
        // 1/8): the same damage over eight times the days, so the row flies 480, not 60.
        fly(f, reg, 480);
        check(f.w.units.at(idle).count < men0 && f.w.units.at(idle).readiness_permille < rd0,
              "A5d bombing at the day scale kills men and spends readiness (" + std::to_string(men0) + " -> "
                  + std::to_string(f.w.units.at(idle).count) + " men)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nA6 - strategic bombing: condition, throughput, repair\n");
    {
        fixture f;
        build(f);
        declare_hostile(f.w, f.a, f.b);
        const uint32_t p10 = strip(f, 10);
        const entity_id mine = add_building(f, f.b, 42, 2, building_type::extraction_site);
        {
            building_component& b = f.w.buildings.at(mine);
            b.target_resource = resource_type::iron_ore;
            b.workforce_assigned = 1.0f;
            f.w.tiles.at(b.tile).resource_deposit[static_cast<std::size_t>(resource_type::iron_ore)] = 500.0f;
        }
        const float full = extraction_nominal(f.w, reg, f.w.buildings.at(mine), 1.0f);
        const entity_id bmr = add_wing(f, f.a, f.airbase, air_class::bomber, air_mission::strategic, {p10});
        const air_day_tick t = fly(f, reg, 30);
        const int cond = f.w.buildings.at(mine).condition_permille;
        check(t.strategic_hits == 30 && cond < 1000 && cond > 0,
              "A6a a bomber lowers the hostile building's condition (1000 -> " + std::to_string(cond) + ")");
        const float now = extraction_nominal(f.w, reg, f.w.buildings.at(mine), 1.0f);
        check(full > 0.0f && std::fabs(now / full - static_cast<float>(cond) / 1000.0f) < 0.002f
                  && std::fabs(building_supply_scalar(f.w.buildings.at(mine)) - cond / 1000.0f) < 1e-4f,
              "A6b throughput scales with condition (" + std::to_string(now) + " of " + std::to_string(full) + ")");
        // Stop bombing: it repairs, drawing steel and machinery off its market's shelf.
        f.w.wings.at(bmr).mission = air_mission::none;
        f.w.wings.at(bmr).provinces.clear();
        const float steel0 = shelf_of(f, f.b_market, resource_type::steel);
        fly(f, reg, 20);
        const int repaired = f.w.buildings.at(mine).condition_permille;
        check(repaired == std::min(1000, cond + 80) && shelf_of(f, f.b_market, resource_type::steel) < steel0,
              "A6c it repairs 4 per-mille a day by drawing goods (" + std::to_string(cond) + " -> "
                  + std::to_string(repaired) + ")");
        for (const resource_type g : k_goods)
            f.w.markets.at(f.b_market).inventory[static_cast<std::size_t>(g)] = 0.0f;
        fly(f, reg, 20);
        check(f.w.buildings.at(mine).condition_permille == repaired,
              "A6d with nothing on the shelf the repair stalls and it stays damaged");
        check(f.w.buildings.at(f.airbase).condition_permille == 1000,
              "A6e the bomber's own side's buildings are untouched");
        {
            recipe_registry treg = reg;
            trade_params tp = treg.trade();
            tp.marketplace_points = 10.0f;
            tp.port_points = 8.0f;
            treg.set_trade(tp);
            building_component mk{};
            mk.type = building_type::planetary_marketplace;
            mk.workforce_assigned = 1.0f; mk.workforce_target = 200;
            const float whole = building_trade_points(treg, mk, 1.0f, true);
            mk.condition_permille = 500;
            const float half = building_trade_points(treg, mk, 1.0f, true);
            building_component port{};
            port.type = building_type::port;
            const float pw = building_trade_points(treg, port, 1.0f, true);
            port.condition_permille = 250;
            const float pq = building_trade_points(treg, port, 1.0f, true);
            check(whole > 0.0f && std::fabs(half - whole * 0.5f) < 1e-4f && std::fabs(pq - pw * 0.25f) < 1e-4f,
                  "A6f a bombed marketplace and port make trade points at their condition");
        }
        {
            fixture g;
            build(g);
            declare_hostile(g.w, g.a, g.b);
            // Strip 12 holds only military installations of B: nothing a bomber bites.
            add_building(g, g.b, 49, 2, building_type::military_base);
            add_wing(g, g.a, g.airbase, air_class::bomber, air_mission::strategic, {strip(g, 12)});
            const air_day_tick t = fly(g, reg, 10);
            bool whole = true;
            for (const auto& kv : g.w.buildings) whole = whole && kv.second.condition_permille == 1000;
            check(t.strategic_hits == 0 && whole && condition_bites(building_type::extraction_site)
                      && !condition_bites(building_type::military_base) && !condition_bites(building_type::airbase),
                  "A6g military bases and airbases are not strategic targets (their output is not condition-scaled)");
        }
    }

    // -----------------------------------------------------------------------
    std::printf("\nA7 - orphans\n");
    {
        fixture f;
        build(f);
        const entity_id w1 = add_wing(f, f.a, f.airbase, air_class::fighter, air_mission::none, {});
        const entity_id w2 = add_wing(f, f.b, f.b_airbase, air_class::fighter, air_mission::none, {});
        economy_report rep;
        run_wing_upkeep(f.w, reg, rep);
        check(f.w.wings.size() == 2, "A7a wings with a standing airbase survive the upkeep pass");
        const bool gone = demolish_state_building(f.w, f.a, f.airbase);
        f.w.wings.at(w2).hp_ppm = 0;
        const wing_upkeep_tick t = run_wing_upkeep(f.w, reg, rep);
        check(gone && t.disbanded == 2 && f.w.wings.count(w1) == 0 && f.w.wings.count(w2) == 0,
              "A7b a wing whose airbase is demolished, and one shot down, are disbanded");
        {
            fixture g;
            build(g);
            const entity_id gw = add_wing(g, g.b, g.b_airbase, air_class::fighter, air_mission::none, {});
            g.w.buildings.at(g.b_airbase).decommissioned = true;
            const float fuel0 = shelf_of(g, g.b_market, resource_type::refined_fuel);
            economy_report r2;
            const wing_upkeep_tick u = run_wing_upkeep(g.w, reg, r2);
            check(u.wings == 1 && g.w.wings.count(gw) == 1
                      && shelf_of(g, g.b_market, resource_type::refined_fuel) == fuel0,
                  "A7c a wing at a decommissioned airbase is grounded: kept, and draws no fuel");
        }
    }

    // -----------------------------------------------------------------------
    std::printf("\nA8 - save\n");
    {
        fixture f;
        build(f);
        declare_hostile(f.w, f.a, f.b);
        declare_hostile(f.w, f.b, f.a);
        const uint32_t p10 = strip(f, 10);
        add_wing(f, f.a, f.airbase, air_class::fighter, air_mission::superiority, {p10});
        add_wing(f, f.a, f.airbase, air_class::multirole, air_mission::tactical, {p10});
        // One province: split over two contested ones a bomber lands 3 per-mille a day,
        // under the 4 a day repair, and the building would never stay damaged.
        add_wing(f, f.a, f.airbase, air_class::bomber, air_mission::strategic, {p10});
        add_wing(f, f.a, f.airbase, air_class::recon, air_mission::recon, {strip(f, 9), p10}); // a two-province record
        add_wing(f, f.b, f.b_airbase, air_class::fighter, air_mission::superiority, {p10});
        add_unit(f, f.b, 42, 1, rifle);
        add_unit(f, f.b, 42, 3, ad_row);
        add_building(f, f.b, 41, 2, building_type::extraction_site);
        set_kit_research_waived_for_harness(true);
        corp_command dc;
        dc.tick = 1; dc.corp = f.a; dc.verb = corp_verb::set_air_design;
        dc.order = static_cast<uint32_t>(air_class::fighter); dc.recipe = 1;
        dc.unit_type = air_kit_item_by_id("ftr.armament.radar_guided");
        const auto dr = apply_corp_command(f.w, reg, dc);
        set_kit_research_waived_for_harness(false);
        fly(f, reg, 5);
        const bool has_all = dr == corp_command_result::applied && !f.w.wings.empty()
            && !f.w.air_superiority.empty() && !f.w.air_revealed.empty() && !f.w.air_hit_bank.empty()
            && !f.w.nations.at(f.a).air_design_kit.empty();
        bool damaged = false;
        for (const auto& kv : f.w.buildings) damaged = damaged || kv.second.condition_permille < 1000;
        const std::string bytes = to_bytes(f.w);
        world back;
        const bool ok = from_bytes(bytes, back);
        check(has_all && damaged && ok && to_bytes(back) == bytes && back.state_hash(3) == f.w.state_hash(3)
                  && back.wings.size() == f.w.wings.size(),
              "A8a wings, superiority, reveals, hit bank, air designs and condition round-trip byte-identically");
        {
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_pre_air(f.w, o);
            world old;
            std::istringstream in(o.str(), std::ios::binary);
            const bool ok64 = read_world_snapshot(old, in);
            bool whole = true;
            for (const auto& kv : old.buildings) whole = whole && kv.second.condition_permille == 1000;
            check(ok64 && old.wings.empty() && old.air_superiority.empty() && whole
                      && old.nations.at(f.a).air_design_kit.empty(),
                  "A8b a v64 stream reads as a world with no air and every building whole");
        }
        {
            // Corrupt one wing's mission byte (the stream is the snapshot with the air
            // section at its end): find a mission value 2 (tactical) followed by n=1.
            fixture g;
            build(g);
            add_wing(g, g.a, g.airbase, air_class::fighter, air_mission::superiority, {strip(g, 6)});
            std::string s = to_bytes(g.w);
            std::ostringstream o64(std::ios::binary);
            write_world_snapshot_pre_air(g.w, o64);
            const std::size_t at = o64.str().size(); // where the air section starts
            // wing count u32, id, owner, airbase (u32 each), u16 row, 4 x i32, 3 x u8 kit, u8 mission
            const std::size_t mission_at = at + 4 + 4 + 4 + 4 + 2 + 16 + 3;
            bool refused_bad = false;
            if (mission_at < s.size())
            {
                s[mission_at] = static_cast<char>(7);
                world bad;
                refused_bad = !from_bytes(s, bad);
            }
            check(refused_bad, "A8c a corrupt mission in the air section refuses the stream whole");
        }
    }

    {
        std::printf("\nA8d - a unit the air names, disbanded, then saved\n");
        fixture f;
        build(f);
        declare_hostile(f.w, f.a, f.b);
        const uint32_t p10 = strip(f, 10);
        add_wing(f, f.a, f.airbase, air_class::fighter, air_mission::superiority, {p10});
        add_wing(f, f.a, f.airbase, air_class::multirole, air_mission::tactical, {p10});
        const entity_id u = add_unit(f, f.b, 42, 1, rifle);
        fly(f, reg, 1);
        const bool named = f.w.air_hit_bank.count(u) == 1 && f.w.air_revealed.count({f.a, u}) == 1;
        corp_command cmd;
        cmd.tick = 1; cmd.corp = f.b; cmd.verb = corp_verb::disband_unit; cmd.subject = u;
        const auto r = apply_corp_command(f.w, reg, cmd);
        const bool forgotten = f.w.air_hit_bank.count(u) == 0 && f.w.air_revealed.count({f.a, u}) == 0;
        world back;
        const std::string bytes = to_bytes(f.w);
        const bool loads = from_bytes(bytes, back) && to_bytes(back) == bytes
                           && back.state_hash(4) == f.w.state_hash(4);
        check(named && r == corp_command_result::applied && forgotten && loads,
              "A8d reveal + bank on a unit, the unit disbanded: the air forgets it, and the save loads byte-identically");
        // The second guard: a unit erased by a path that forgot nothing still saves loadably.
        const entity_id v = add_unit(f, f.b, 42, 2, rifle);
        f.w.air_hit_bank[v] = {5, 5};
        f.w.air_revealed.insert({f.a, v});
        f.w.units.erase(v);
        world back2;
        check(from_bytes(to_bytes(f.w), back2) && back2.air_hit_bank.count(v) == 0,
              "A8e a dangling bank / reveal entry is pruned on write, so the strict reader still loads it");
    }

    // -----------------------------------------------------------------------
    std::printf("\nA9 - determinism\n");
    {
        auto run = [&](fixture& g) {
            build(g);
            declare_hostile(g.w, g.a, g.b);
            declare_hostile(g.w, g.b, g.a);
            const uint32_t p10 = strip(g, 10), p9 = strip(g, 9);
            add_wing(g, g.a, g.airbase, air_class::fighter, air_mission::superiority, {p9, p10});
            add_wing(g, g.a, g.airbase, air_class::multirole, air_mission::tactical, {p10});
            add_wing(g, g.a, g.airbase, air_class::bomber, air_mission::strategic, {p10});
            add_wing(g, g.b, g.b_airbase, air_class::fighter, air_mission::superiority, {p10, p9});
            add_wing(g, g.b, g.b_airbase, air_class::drone, air_mission::tactical, {p9});
            add_wing(g, g.a, g.airbase, air_class::recon, air_mission::recon, {p10});
            add_unit(g, g.b, 42, 1, rifle);
            add_unit(g, g.b, 41, 3, ad_row);
            add_unit(g, g.a, 37, 2, rifle);
            add_unit(g, g.a, 38, 1, ad_row);
            add_building(g, g.b, 41, 2, building_type::extraction_site);
            economy_report rep;
            for (int q = 0; q < 2; ++q)
            {
                run_air_missions(g.w, reg, rep, 90);
                run_wing_upkeep(g.w, reg, rep);
            }
        };
        fixture a, b;
        run(a);
        run(b);
        check(a.w.state_hash(9) == b.w.state_hash(9) && to_bytes(a.w) == to_bytes(b.w),
              "A9a twin runs hash and serialise alike");
        fixture c;
        build(c);
        const uint64_t h = c.w.state_hash(0);
        fly(c, reg, 30);
        economy_report rep;
        run_wing_upkeep(c.w, reg, rep);
        check(c.w.state_hash(0) == h, "A9b a world with no air is untouched by the passes");
    }

    // -----------------------------------------------------------------------
    std::printf("\nA10 - air designs\n");
    {
        check(air_kit_catalogue_first_error() == nullptr && air_kit_catalogue().size() == 54,
              "A10a the air catalogue is well-formed: six classes x three slots, base + two each");
        bool slots_named = std::string(air_kit_slot_name(0)) == "Airframe"
            && std::string(air_kit_slot_name(1)) == "Armament" && std::string(air_kit_slot_name(2)) == "Avionics";
        check(slots_named, "A10b the slots are airframe, armament and avionics");
        fixture f;
        build(f);
        const uint16_t it = air_kit_item_by_id("ftr.armament.radar_guided");
        corp_command dc;
        dc.tick = 1; dc.corp = f.a; dc.verb = corp_verb::set_air_design;
        dc.order = static_cast<uint32_t>(air_class::fighter); dc.recipe = 1; dc.unit_type = it;
        refused(f, [&] { return apply_corp_command(f.w, reg, dc); },
                corp_command_result::rejected_tech_locked, "A10c a locked air item is refused");
        corp_command rc = dc;
        rc.corp = f.b;
        set_kit_research_waived_for_harness(true);
        refused(f, [&] { return apply_corp_command(f.w, reg, rc); },
                corp_command_result::rejected_not_owner, "A10d a rival nation may not design");
        corp_command wrong = dc;
        wrong.recipe = 0; // the radar-guided missile is armament, not an airframe
        refused(f, [&] { return apply_corp_command(f.w, reg, wrong); },
                corp_command_result::rejected_invalid, "A10e an item in the wrong slot is refused");
        const auto r = apply_corp_command(f.w, reg, dc);
        entity_id wid = null_entity;
        const auto hr = hire(f, reg, f.a, tile(f, 30, 1), FTR, &wid);
        set_kit_research_waived_for_harness(false);
        const int base_air = reg.military().air.cls[0].air;
        check(r == corp_command_result::applied && hr == corp_command_result::applied
                  && wing_stats(reg, f.w.wings.at(wid)).air == base_air + 400,
              "A10f a researched item fits, and the next wing hired flies it (+400 air)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nA11 - the economy tick flies the air\n");
    {
        fixture f;
        build(f);
        declare_hostile(f.w, f.a, f.b);
        add_wing(f, f.a, f.airbase, air_class::fighter, air_mission::superiority, {strip(f, 10)});
        add_unit(f, f.b, 42, 2, ad_row);
        const economy_report rep = run_economy_step(f.w, reg);
        check(rep.air.sorties == 90 && rep.air.ad_shots == 90 && rep.wing_upkeep.wings == 1,
              "A11a run_economy_step flies the quarter's 90 days and upkeeps the wing ("
                  + std::to_string(rep.air.sorties) + " sorties)");
    }

    // -----------------------------------------------------------------------
    // R — RIVAL AIR (BL-1451; AI_OPPONENT.md § 11, "Rival nations fly air wings").
    // B is the rival, A the seat, C a third nation holding columns 72-79. Each
    // "not admitted" clause of the grant is a check of its own.
    std::printf("\nR - rival air (BL-1451)\n");
    {
        auto rival_fixture = [&](fixture& g, entity_id& c) {
            build(g);
            // B builds its own airbase in R2: remove the fixture's.
            demolish_state_building(g.w, g.b, g.b_airbase);
            g.b_airbase = null_entity;
            c = g.w.create_entity();
            nation_component nc{}; nc.name = "Corrie"; nc.treasury = 100000.0f;
            g.w.nations[c] = nc;
            for (int r = 0; r < GH; ++r)
                for (int col = 56; col < 64; ++col) // strips 14-15, inside a drone's reach of B's base
                    g.w.set_tile_nation(tile(g, col, r), c);
            g.w.province_holder[14] = c;
            g.w.province_holder[15] = c;
            // A road along row 1, east to west: no building stands where no road reaches.
            for (int col = 0; col < GW - 1; ++col)
                lay_road_edge(g.w, tile(g, col, 1), 0, 1);
            // The nations' own tile lists (generation fills them; the scorer reads them).
            for (int r = 0; r < GH; ++r)
                for (int col = 0; col < GW; ++col)
                {
                    const entity_id t = tile(g, col, r);
                    g.w.nations.at(g.w.tile_to_nation.at(t)).tiles.push_back(t);
                }
            g.w.nations.at(g.b).capital_tile = tile(g, 46, 1);
            g.w.nations.at(g.a).capital_tile = tile(g, 30, 1);
        };

        fixture f;
        entity_id c = null_entity;
        rival_fixture(f, c);
        const uint64_t h_peace = f.w.state_hash(1);
        score_nation_air(f.w, reg, f.b, 1);
        check(f.w.state_hash(1) == h_peace, "R1 at peace a rival builds no airbase and hires nothing");

        declare_hostile(f.w, f.b, f.a);
        const auto pairs0 = f.w.corp_hostile_pairs;
        score_nation_air(f.w, reg, f.b, 2);
        entity_id built = null_entity;
        for (const entity_id bid : f.w.nations.at(f.b).state_assets)
            if (f.w.buildings.count(bid) && f.w.buildings.at(bid).type == building_type::airbase)
                built = bid;
        const bool own_ground = built != null_entity
            && f.w.tile_to_nation.at(f.w.buildings.at(built).tile) == f.b;
        check(own_ground, "R2 at war it builds an airbase, on its own ground, through the build verb");
        if (built != null_entity)
            f.w.buildings.at(built).ticks_remaining = 2; // a default registry builds at once: make it a site
        score_nation_air(f.w, reg, f.b, 3);
        int bases = 0;
        for (const entity_id bid : f.w.nations.at(f.b).state_assets)
            if (f.w.buildings.count(bid) && f.w.buildings.at(bid).type == building_type::airbase)
                ++bases;
        check(bases == 1 && f.w.wings.empty(), "R2b one airbase at a time; no wing while it is still building");

        if (built != null_entity)
        {
            f.w.buildings.at(built).ticks_remaining = 0;
            f.w.buildings.at(built).days_remaining = 0;
        }
        const entity_id c_unit = add_unit(f, c, 58, 2, rifle); // in reach, a nation the rival is NOT hostile to
        add_unit(f, f.a, 38, 1, rifle);       // a target it is
        add_building(f, f.a, 37, 2, building_type::extraction_site);
        for (int k = 0; k < 6; ++k)
            score_nation_air(f.w, reg, f.b, 10 + k);
        // The seat's and the third nation's air, which the rival must never touch.
        const entity_id a_wing = add_wing(f, f.a, f.airbase, air_class::fighter, air_mission::none, {});
        const entity_id c_base = add_building(f, c, 58, 1, building_type::airbase);
        const entity_id c_wing = add_wing(f, c, c_base, air_class::multirole, air_mission::none, {});
        int b_wings = 0;
        bool at_own_base = true, one_province = true, bombs_hostile_only = true;
        for (const auto& [wid, wg] : f.w.wings)
        {
            if (wg.owner != f.b)
                continue;
            ++b_wings;
            at_own_base = at_own_base && wg.airbase == built;
            one_province = one_province && wg.provinces.size() <= 1;
            if (air_mission_bombs(wg.mission))
                for (const uint32_t p : wg.provinces)
                    bombs_hostile_only = bombs_hostile_only && province_holder_for(f.w, p) == f.a;
        }
        check(b_wings == 3 && at_own_base, "R3 it hires wings only at its own airbase, up to its cap of three ("
                  + std::to_string(b_wings) + ")");
        // Raise them, then let it assign.
        for (auto& [wid, wg] : f.w.wings)
            if (wg.owner == f.b) wg.muster_days = 0;
        score_nation_air(f.w, reg, f.b, 20);
        int assigned = 0, bombing = 0;
        one_province = true; bombs_hostile_only = true;
        for (const auto& [wid, wg] : f.w.wings)
        {
            if (wg.owner != f.b)
                continue;
            assigned += wg.mission != air_mission::none ? 1 : 0;
            bombing  += air_mission_bombs(wg.mission) ? 1 : 0;
            one_province = one_province && wg.provinces.size() <= 1;
            if (wg.mission == air_mission::strategic)
                for (const uint32_t p : wg.provinces)
                    bombs_hostile_only = bombs_hostile_only && province_holder_for(f.w, p) == f.a;
        }
        {
            world flown = f.w;
            economy_report rr;
            run_air_missions(flown, reg, rr, 20);
            bombs_hostile_only = bombs_hostile_only && flown.air_hit_bank.count(c_unit) == 0
                                 && flown.units.at(c_unit).count == 50;
        }
        check(assigned >= 1 && bombing >= 1, "R4 it assigns its own raised wings missions (" + std::to_string(assigned)
                  + " of 3, " + std::to_string(bombing) + " bombing)");
        check(bombs_hostile_only,
              "R5 not admitted: bombing a nation it is not already hostile to (strategic only over the enemy's provinces; the third nation's unit untouched)");
        check(f.w.corp_hostile_pairs == pairs0, "R6 not admitted: declaring the hostility a bombing mission needs");
        check(one_province, "R7 not admitted: a campaign planned past the next assignment (one province a wing)");
        check(f.w.wings.at(a_wing).mission == air_mission::none && f.w.wings.at(c_wing).mission == air_mission::none,
              "R8 not admitted: another nation's wing, the seat's included");
        int base_count_a = 0;
        for (const entity_id bid : f.w.nations.at(f.a).state_assets)
            if (f.w.buildings.count(bid) && f.w.buildings.at(bid).type == building_type::airbase) ++base_count_a;
        check(base_count_a == 1, "R9 not admitted: an airbase on another nation's ground (the seat's still one)");

        // The seat's nation, at war and armed, is never acted for.
        declare_hostile(f.w, f.a, f.b);
        f.w.wings.at(a_wing).muster_days = 0;
        const uint64_t h_seat = f.w.state_hash(30);
        score_nation_air(f.w, reg, f.a, 30);
        check(f.w.state_hash(30) == h_seat, "R10 not admitted: the player's nation — its pass changes nothing");

        // Determinism: twin runs alike.
        auto twin = [&](fixture& g) {
            entity_id cc = null_entity;
            rival_fixture(g, cc);
            declare_hostile(g.w, g.b, g.a);
            add_unit(g, g.a, 38, 1, rifle);
            add_building(g, g.a, 37, 2, building_type::extraction_site);
            economy_report rep;
            for (int k = 0; k < 8; ++k)
            {
                score_nation_air(g.w, reg, g.b, k);
                for (auto& kv : g.w.buildings) if (kv.second.type == building_type::airbase) { kv.second.ticks_remaining = 0; kv.second.days_remaining = 0; }
                run_air_missions(g.w, reg, rep, 90);
                run_wing_upkeep(g.w, reg, rep);
            }
        };
        fixture t1, t2;
        twin(t1);
        twin(t2);
        check(t1.w.state_hash(9) == t2.w.state_hash(9) && to_bytes(t1.w) == to_bytes(t2.w) && !t1.w.wings.empty(),
              "R11 twin rival-air runs hash and serialise alike");
    }

    std::printf("\n%d / %d checks passed\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
