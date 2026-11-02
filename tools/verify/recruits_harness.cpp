// recruits_harness.cpp — BL-1463 (recruits; MILITARY.md § Recruits, Ben 2026-10-30).
//
// Bases make recruits from their province's people; each nation holds one land and one
// air pool, capped at fifteen quarters of output; a hire spends its men or its crew and
// is refused before money moves when the pool cannot fill it; a land unit below its
// establishment refills from its pool at a Logistic Point cost, paid at its road within
// 8 tiles; bases carry no upkeep. Measured against the AUTHORED numbers
// (scripts/economy.lua § economy.military.recruits):
//
//   R1  output per base: a military base makes 50 land recruits a quarter and an airbase
//       20 air; a base under construction, idled or state-idled makes none;
//   R2  the province-centre draw: the centre with the most heads first, ties to the
//       lowest id, only the base's own province; a province that cannot give makes
//       fewer; a thousand heads takes one off the centre's population;
//   R3  the cap: 15 quarters of output; a full pool takes no more and its bases take no
//       heads; a pool above its cap is never cut;
//   R4  the hire: a unit spends 50 land recruits and a wing its crew; a hire the pool
//       cannot fill is refused rejected_no_recruits, by the seam and its check, and
//       mutates nothing; the shortfall's words;
//   R5  the refill: a unit below its establishment draws what it lacks while its pool
//       lasts, ascending unit id; its recruits x 1 LP join its supply request; on the
//       road and exactly 8 tiles off it refills whole, 9 off it refills nothing;
//   R6  bases carry no upkeep: zero maintenance and wage, so zero opex;
//   R7  the save (v72): pools and carries round-trip; v71 and v67 streams read with empty pools and
//       are today's less exactly the later sections; a corrupt row is refused whole;
//   R8  determinism: the same quarters twice are byte-identical, and the real economy
//       step makes the quarter's recruits;
//   R9  the loader rejects a bad recruits value, and a rate past its 1000 ceiling;
//   R10 wings refill from the air pool (Ben, 2026-10-31): HP maps to crew, whole
//       recruits, conservation, the 8-tile draw, ascending wing id, determinism;
//   R11 a dropped muster refunds its men, uncapped; credits are not refunded.
//
// Build: bash tools/verify/build_lua_harness.sh recruits_harness. Run from the repo root.

#include "world/air_combat.hpp"
#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/logistic_points.hpp"
#include "world/logistics.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/recruits.hpp"
#include "world/river_generation.hpp" // hex_side_for_offset
#include "world/unit_roster.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include "scripting/lua_state.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    ok ? ++g_pass : ++g_fail;
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

constexpr int GW = 40;

/// One row of GW plains tiles, every one the nation's; two provinces, columns 0..19 (A)
/// and 20..39 (B); a market at column 1 with every good shelved, so every land and air
/// row is fieldable.
struct fixture
{
    world     w;
    entity_id body   = null_entity;
    entity_id nation = null_entity;
    entity_id market = null_entity;
    std::vector<entity_id> grid;
    uint32_t  prov_a = 0, prov_b = 0;
};

entity_id tile(const fixture& f, int c) { return f.grid[static_cast<std::size_t>(c)]; }

void build(fixture& f)
{
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Field"; bc.type = body_type::planet; bc.grid_width = GW; bc.grid_height = 1;
    w.bodies[f.body] = bc;
    for (int c = 0; c < GW; ++c)
    {
        const entity_id t = w.create_entity();
        tile_component tc{};
        tc.body = f.body; tc.grid_x = c; tc.grid_y = 0;
        tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass;
        tc.cover_density = 150; tc.landform = terrain_landform::plains; tc.habitability = 0.5f;
        w.tiles[t] = tc;
        f.grid.push_back(t);
    }
    w.provinces.provinces.clear();
    for (int k = 0; k < 2; ++k)
    {
        province p;
        p.body = f.body;
        for (int c = k * 20; c < (k + 1) * 20; ++c)
            p.tiles.push_back(tile(f, c));
        std::sort(p.tiles.begin(), p.tiles.end());
        p.id = p.tiles.front();
        for (const entity_id t : p.tiles)
            w.provinces.tile_province[t] = p.id;
        (k == 0 ? f.prov_a : f.prov_b) = p.id;
        w.provinces.provinces.push_back(std::move(p));
    }
    f.nation = w.create_entity();
    nation_component nc{};
    nc.name = "Hosts"; nc.treasury = 100000.0f;
    w.nations[f.nation] = nc;
    w.player_entity = f.nation;
    for (int c = 0; c < GW; ++c)
        w.set_tile_nation(tile(f, c), f.nation);
    w.province_holder.assign(w.provinces.provinces.size(), f.nation);

    f.market = w.create_entity();
    market_component mc{};
    mc.body = f.body;
    mc.centre_tile = tile(f, 1);
    for (std::size_t i = 0; i < resource_count; ++i)
    {
        mc.base_price[i] = 10.0f; mc.price[i] = 10.0f; mc.inventory[i] = 1000.0f;
    }
    w.markets[f.market] = mc;
}

entity_id add_building(fixture& f, int c, building_type t, int ticks = 0)
{
    const entity_id id = f.w.create_entity();
    building_component b{};
    b.tile = tile(f, c);
    b.type = t;
    b.ticks_remaining = ticks;
    f.w.buildings[id] = b;
    f.w.nations.at(f.nation).state_assets.push_back(id);
    return id;
}

entity_id add_centre(fixture& f, int c, int population, int carry = 0)
{
    const entity_id id = f.w.create_entity();
    population_centre_component p{};
    p.scale = 1;
    p.population = population;
    p.recruit_heads_taken = carry;
    f.w.population_centres[id] = p;
    f.w.population_centre_tile[id] = tile(f, c);
    return id;
}

entity_id add_unit(fixture& f, int c, int count, int establishment)
{
    const entity_id u = f.w.create_entity();
    unit_component uc{};
    uc.owner = f.nation;
    uc.position = tile(f, c);
    uc.count = count;
    uc.establishment = establishment;
    f.w.units[u] = uc;
    return u;
}

void lay_row_road(fixture& f, int from, int to)
{
    const int east = hex_side_for_offset(1, 0, /*odd_row=*/false);
    for (int c = from; c < to; ++c)
        lay_road_edge(f.w, tile(f, c), east, 1);
}

nation_component& nat(fixture& f) { return f.w.nations.at(f.nation); }
const population_centre_component& centre(fixture& f, entity_id c) { return f.w.population_centres.at(c); }

corp_command_result hire_unit(fixture& f, const recipe_registry& reg, int c, uint16_t row)
{
    corp_command cmd;
    cmd.tick = 1; cmd.corp = f.nation; cmd.verb = corp_verb::hire_unit;
    cmd.tile = tile(f, c); cmd.unit_type = row;
    return apply_corp_command(f.w, reg, cmd);
}

corp_command_result hire_wing(fixture& f, const recipe_registry& reg, int c, uint16_t row)
{
    corp_command cmd;
    cmd.tick = 1; cmd.corp = f.nation; cmd.verb = corp_verb::hire_wing;
    cmd.tile = tile(f, c); cmd.unit_type = row;
    return apply_corp_command(f.w, reg, cmd);
}

const lp_supply_row* supply_row(const lp_tick& t, entity_id unit)
{
    for (const lp_supply_row& r : t.supplies)
        if (r.unit == unit)
            return &r;
    return nullptr;
}

const recruit_refill* plan_row(const std::vector<recruit_refill>& p, entity_id unit)
{
    for (const recruit_refill& r : p)
        if (r.unit == unit)
            return &r;
    return nullptr;
}

/// The quarter's refill as the economy step runs it.
struct refill_result
{
    std::vector<recruit_refill> plan;
    lp_tick lp;
    int delivered = 0;
};
refill_result refill_quarter(fixture& f, const recipe_registry& reg)
{
    refill_result r;
    r.plan = plan_recruit_refills(f.w);
    r.lp = resolve_logistic_points(f.w, collect_supply_claims(f.w, reg, r.plan));
    r.delivered = apply_recruit_refills(f.w, r.plan, r.lp);
    return r;
}

} // namespace

int main()
{
    std::printf("=== recruits (BL-1463) ===\n");
    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    const recruit_params& rp = reg.military().recruits;
    std::printf("  authored: land_per_base %d, air_per_base %d, lp_per_recruit %.2f\n",
                rp.land_per_base, rp.air_per_base, rp.lp_per_recruit);
    {
        const recruit_params def{};
        check(rp.land_per_base == def.land_per_base && rp.air_per_base == def.air_per_base
                  && rp.lp_per_recruit == def.lp_per_recruit,
              "R0 the authored recruits equal the C++ defaults (a harness registry recruits what the game does)");
        const air_params defa = default_air_params();
        bool crew_eq = true;
        for (std::size_t c = 0; c < air_class_count; ++c)
            crew_eq = crew_eq && reg.military().air.cls[c].crew == defa.cls[c].crew;
        check(crew_eq, "R0 every air class's authored crew equals its C++ default");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR1 - output per base\n");
    {
        fixture f; build(f);
        const entity_id big   = add_centre(f, 4, 5);  // 5000 heads
        const entity_id small = add_centre(f, 3, 3);  // 3000 heads
        add_building(f, 5, building_type::military_base);
        add_building(f, 6, building_type::airbase);
        check(recruit_quarter_output(f.w, reg, f.nation, recruit_theatre::land) == 50
                  && recruit_quarter_output(f.w, reg, f.nation, recruit_theatre::air) == 20,
              "R1a one military base makes 50 land recruits a quarter, one airbase 20 air");
        check(recruit_cap(f.w, reg, f.nation, recruit_theatre::land) == 750
                  && recruit_cap(f.w, reg, f.nation, recruit_theatre::air) == 300,
              "R1b the caps are 15 quarters: 750 land, 300 air");
        const recruitment_tick t = run_recruitment(f.w, reg);
        std::printf("      pools land %d air %d; heads taken %lld; big carry %d, small carry %d\n",
                    nat(f).land_recruits, nat(f).air_recruits, static_cast<long long>(t.heads_taken),
                    centre(f, big).recruit_heads_taken, centre(f, small).recruit_heads_taken);
        check(nat(f).land_recruits == 50 && nat(f).air_recruits == 20 && t.heads_taken == 70,
              "R1c a quarter fills the land pool by 50 and the air pool by 20, 70 heads taken");
        check(t.nations.size() == 1 && t.nations[0].land_made == 50 && t.nations[0].air_made == 20
                  && t.nations[0].land_short == 0 && t.nations[0].air_short == 0,
              "R1d the tick's row reports what each theatre made, none short");
        add_building(f, 7, building_type::military_base);
        check(recruit_quarter_output(f.w, reg, f.nation, recruit_theatre::land) == 100,
              "R1e two bases make 100: the nation's ONE land pool takes both");
        (void)run_recruitment(f.w, reg);
        check(nat(f).land_recruits == 150 && nat(f).air_recruits == 40,
              "R1f the next quarter: land 50 + 100, air 20 + 20");

        fixture g; build(g);
        add_centre(g, 4, 5);
        add_building(g, 5, building_type::military_base, /*ticks=*/3);   // under construction
        const entity_id idle = add_building(g, 6, building_type::military_base);
        g.w.buildings.at(idle).decommissioned = true;
        const entity_id sidle = add_building(g, 7, building_type::airbase);
        g.w.buildings.at(sidle).state_idle = true;
        const recruitment_tick tg = run_recruitment(g.w, reg);
        check(nat(g).land_recruits == 0 && nat(g).air_recruits == 0 && tg.heads_taken == 0
                  && recruit_quarter_output(g.w, reg, g.nation, recruit_theatre::land) == 0,
              "R1g a base under construction, idled, or idled by the state's funding makes none");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR2 - the province-centre draw\n");
    {
        fixture f; build(f);
        const entity_id small = add_centre(f, 3, 3);
        const entity_id big   = add_centre(f, 4, 5);
        const entity_id other = add_centre(f, 25, 100); // province B
        add_building(f, 5, building_type::military_base);
        (void)run_recruitment(f.w, reg);
        check(centre(f, big).recruit_heads_taken == 50 && centre(f, small).recruit_heads_taken == 0
                  && centre(f, other).recruit_heads_taken == 0,
              "R2a the base takes from the centre with the most heads, in its own province only");

        fixture t; build(t);
        const entity_id lo = add_centre(t, 3, 4);
        const entity_id hi = add_centre(t, 4, 4);
        add_building(t, 5, building_type::military_base);
        (void)run_recruitment(t.w, reg);
        check(lo < hi && centre(t, lo).recruit_heads_taken == 50 && centre(t, hi).recruit_heads_taken == 0,
              "R2b a tie goes to the lower centre id");
        (void)run_recruitment(t.w, reg);
        check(centre(t, lo).recruit_heads_taken == 50 && centre(t, hi).recruit_heads_taken == 50,
              "R2c ...and next quarter the other now has the most heads, so it gives");

        fixture s; build(s);
        const entity_id last = add_centre(s, 3, 1, /*carry=*/970); // 30 heads left
        add_building(s, 5, building_type::military_base);
        const recruitment_tick ts = run_recruitment(s.w, reg);
        std::printf("      30 heads: pool %d, short %d, population %d carry %d\n", nat(s).land_recruits,
                    ts.nations.empty() ? -1 : ts.nations[0].land_short, centre(s, last).population,
                    centre(s, last).recruit_heads_taken);
        check(nat(s).land_recruits == 30 && !ts.nations.empty() && ts.nations[0].land_short == 20
                  && centre(s, last).population == 0 && centre(s, last).recruit_heads_taken == 0,
              "R2d a province with 30 heads left makes 30, 20 short, and is emptied");
        (void)run_recruitment(s.w, reg);
        check(nat(s).land_recruits == 30, "R2e an emptied province makes none");

        fixture e; build(e);
        add_building(e, 5, building_type::military_base);
        add_centre(e, 25, 50); // only in B
        (void)run_recruitment(e.w, reg);
        check(nat(e).land_recruits == 0, "R2f a base in a province with no centre makes none (B's people stay B's)");

        fixture k; build(k);
        const entity_id kc = add_centre(k, 3, 5, /*carry=*/990);
        add_building(k, 5, building_type::military_base);
        const recruitment_tick tk = run_recruitment(k.w, reg);
        check(centre(k, kc).population == 4 && centre(k, kc).recruit_heads_taken == 40
                  && tk.centres_lost_thousand == 1,
              "R2g a thousand heads takes one off the centre's population (5 at 990 + 50 -> 4 at 40)");

        fixture r; build(r);
        const entity_id rz = add_centre(r, 3, 9);
        r.w.population_centres.at(rz).razed = true;
        add_building(r, 5, building_type::military_base);
        (void)run_recruitment(r.w, reg);
        check(nat(r).land_recruits == 0, "R2h a razed centre gives no heads");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR3 - the cap\n");
    {
        fixture f; build(f);
        const entity_id c = add_centre(f, 3, 50);
        add_building(f, 5, building_type::military_base);
        nat(f).land_recruits = 740;
        (void)run_recruitment(f.w, reg);
        check(nat(f).land_recruits == 750 && centre(f, c).recruit_heads_taken == 10,
              "R3a 10 below the cap: the base makes 10 and takes 10 heads");
        const recruitment_tick t = run_recruitment(f.w, reg);
        check(nat(f).land_recruits == 750 && centre(f, c).recruit_heads_taken == 10 && t.heads_taken == 0,
              "R3b a full pool takes no more: no recruit, no head");
        nat(f).land_recruits = 900;
        (void)run_recruitment(f.w, reg);
        check(nat(f).land_recruits == 900, "R3c a pool above its cap (its bases gone) is never cut");
        int quarters = 0;
        fixture z; build(z);
        add_centre(z, 3, 50);
        add_building(z, 5, building_type::military_base);
        while (nat(z).land_recruits < recruit_cap(z.w, reg, z.nation, recruit_theatre::land) && quarters < 100)
        {
            (void)run_recruitment(z.w, reg);
            ++quarters;
        }
        check(quarters == 15, "R3d an empty pool fills in exactly 15 quarters (" + std::to_string(quarters) + ")");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR4 - the hire spends and refuses\n");
    {
        fixture f; build(f);
        add_building(f, 5, building_type::military_base);
        add_building(f, 6, building_type::airbase);
        const auto avail = available_rows(f.w, f.nation, campaign_roster_band_for(reg.era()));
        check(!avail.empty(), "R4a the fixture fields a land row");
        const uint16_t row = avail.empty() ? 0
            : static_cast<uint16_t>(avail.front() - unit_roster_table().data());
        check(hire_unit_recruits(row) == 50, "R4b a land hire's men are 50 (hire_batch_manpower)");

        nat(f).land_recruits = 49;
        const std::string before = to_bytes(f.w);
        const float treasury = nat(f).treasury;
        check(hire_unit_check(f.w, reg, f.nation, tile(f, 5), row) == corp_command_result::rejected_no_recruits,
              "R4c hire_unit_check refuses a hire the pool cannot fill: rejected_no_recruits");
        const corp_command_result r = hire_unit(f, reg, 5, row);
        check(r == corp_command_result::rejected_no_recruits && to_bytes(f.w) == before
                  && nat(f).treasury == treasury && f.w.musters.empty(),
              "R4d the seam refuses it before money moves, and mutates nothing");
        check(recruit_shortfall_text(50, 49) == "Needs 50 recruits; the pool holds 49",
              "R4e the shortfall in the shelf's words: \"" + recruit_shortfall_text(50, 49) + "\"");
        nat(f).land_recruits = 50;
        check(hire_unit_check(f.w, reg, f.nation, tile(f, 5), row) == corp_command_result::applied,
              "R4f with 50 the check passes");
        const corp_command_result r2 = hire_unit(f, reg, 5, row);
        check(r2 == corp_command_result::applied && nat(f).land_recruits == 0 && f.w.musters.size() == 1
                  && nat(f).treasury < treasury,
              "R4g the hire applies, pays, and spends its 50 recruits");
        check(hire_unit_check(f.w, reg, f.nation, tile(f, 7), row) == corp_command_result::rejected_placement,
              "R4h the pool is checked after the base: no base is still rejected_placement");

        // Wings.
        const std::vector<uint16_t> air = available_air_rows(f.w, f.nation);
        check(!air.empty(), "R4i the fixture fields an air row");
        static const int k_crew[air_class_count] = {20, 24, 40, 12, 6, 30};
        bool crew_ok = air_roster_table().size() == air_class_count;
        for (std::size_t i = 0; i < air_roster_table().size(); ++i)
            crew_ok = crew_ok && wing_crew(reg, static_cast<uint16_t>(i))
                                     == k_crew[static_cast<std::size_t>(air_roster_table()[i].cls)];
        check(crew_ok, "R4j crews: fighter 20, multirole 24, bomber 40, helicopter 12, drone 6, recon 30");
        const uint16_t fighter = 0;
        nat(f).air_recruits = 19;
        const std::string wb = to_bytes(f.w);
        check(hire_wing_check(f.w, reg, f.nation, tile(f, 6), fighter) == corp_command_result::rejected_no_recruits,
              "R4k hire_wing_check refuses a fighter wing (crew 20) on 19 air recruits");
        check(hire_wing(f, reg, 6, fighter) == corp_command_result::rejected_no_recruits
                  && to_bytes(f.w) == wb && f.w.wings.empty(),
              "R4l the seam refuses it before money moves, and mutates nothing");
        nat(f).air_recruits = 20;
        check(hire_wing(f, reg, 6, fighter) == corp_command_result::applied && nat(f).air_recruits == 0
                  && f.w.wings.size() == 1,
              "R4m with 20 the wing is raised and spends its crew");
        check(nat(f).land_recruits == 0, "R4n a wing draws on the air pool only");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR5 - the refill, its LP cost and the 8-tile draw\n");
    {
        // A hub at column 0 and a road 0..2. u_on stands on the road, u_eight 8 tiles past
        // its end, u_nine 9 past.
        fixture f; build(f);
        add_building(f, 0, building_type::inland_logistics_hub);
        lay_row_road(f, 0, 2);
        const entity_id u_on    = add_unit(f, 2, 30, 50);
        const entity_id u_eight = add_unit(f, 10, 40, 50);
        const entity_id u_nine  = add_unit(f, 11, 10, 50);
        const entity_id u_full  = add_unit(f, 1, 50, 50);
        nat(f).land_recruits = 1000;

        const std::vector<recruit_refill> plan = plan_recruit_refills(f.w);
        const recruit_refill* p_on = plan_row(plan, u_on);
        const recruit_refill* p_eight = plan_row(plan, u_eight);
        const recruit_refill* p_nine = plan_row(plan, u_nine);
        check(p_on && p_on->planned == 20 && p_eight && p_eight->planned == 10 && p_nine && p_nine->planned == 40
                  && plan_row(plan, u_full) == nullptr,
              "R5a the plan: each unit what it lacks (20, 10, 40); a full unit plans nothing");

        // The LP cost: each planned recruit adds lp_per_recruit to the supply request.
        const std::vector<lp_supply_claim> with = collect_supply_claims(f.w, reg, plan);
        const std::vector<lp_supply_claim> without = collect_supply_claims(f.w, reg, {});
        float d_on = -1.0f, d_eight = -1.0f;
        for (std::size_t i = 0; i < with.size() && i < without.size(); ++i)
        {
            if (with[i].unit == u_on) d_on = with[i].need - without[i].need;
            if (with[i].unit == u_eight) d_eight = with[i].need - without[i].need;
        }
        std::printf("      LP added to the request: on the road %.3f, 8 off %.3f\n", d_on, d_eight);
        check(d_on > 19.999f && d_on < 20.001f && d_eight > 9.999f && d_eight < 10.001f,
              "R5b each recruit adds 1 LP to its unit's supply request (20 and 10)");
        check(collect_supply_claims(f.w, reg).size() == with.size(),
              "R5c the plan-less form computes the same plan itself");

        world no_refill = f.w;
        no_refill.nations.at(f.nation).land_recruits = 0;
        const lp_tick base_lp = resolve_logistic_points(no_refill, collect_supply_claims(no_refill, reg));
        const refill_result r = refill_quarter(f, reg);
        const lp_supply_row* s_on = supply_row(r.lp, u_on);
        const lp_supply_row* s_eight = supply_row(r.lp, u_eight);
        const lp_supply_row* s_nine = supply_row(r.lp, u_nine);
        float base_granted = 0.0f, granted = 0.0f;
        for (const lp_nation_row& n : base_lp.nations) if (n.nation == f.nation) base_granted = n.supply_granted;
        for (const lp_nation_row& n : r.lp.nations) if (n.nation == f.nation) granted = n.supply_granted;
        std::printf("      shares on %d / 8 off %d / 9 off %d; granted %.3f vs %.3f with no refill; delivered %d\n",
                    s_on ? s_on->share_permille : -1, s_eight ? s_eight->share_permille : -1,
                    s_nine ? s_nine->share_permille : -1, granted, base_granted, r.delivered);
        check(s_on && s_on->share_permille == 1000 && f.w.units.at(u_on).count == 50,
              "R5d on the road: share 1000, refilled whole (30 -> 50)");
        check(s_eight && s_eight->draw_steps == 8 && s_eight->share_permille == 1000
                  && f.w.units.at(u_eight).count == 50,
              "R5e exactly 8 tiles off: paid at the road's end, refilled whole (40 -> 50)");
        check(s_nine && s_nine->paid_tile == null_entity && s_nine->share_permille == 0
                  && f.w.units.at(u_nine).count == 10,
              "R5f 9 tiles off: past its roads, nothing refills (10 stays 10)");
        check(r.delivered == 30 && nat(f).land_recruits == 970,
              "R5g 30 recruits delivered, 30 taken from the pool (1000 -> 970); the unfilled 40 stay in it");
        check(granted > base_granted + 29.99f && granted < base_granted + 30.01f,
              "R5h the nation's pool granted exactly 30 more points for the 30 recruits");

        // A short pool: ascending unit id while it lasts.
        fixture g; build(g);
        add_building(g, 0, building_type::inland_logistics_hub);
        lay_row_road(g, 0, 2);
        const entity_id a = add_unit(g, 1, 30, 50);
        const entity_id b = add_unit(g, 2, 30, 50);
        nat(g).land_recruits = 25;
        const refill_result rg = refill_quarter(g, reg);
        check(a < b && g.w.units.at(a).count == 50 && g.w.units.at(b).count == 35 && nat(g).land_recruits == 0
                  && rg.delivered == 25,
              "R5i a short pool (25) refills in ascending unit id: 20 to the first, 5 to the second");
        fixture h; build(h);
        add_building(h, 0, building_type::inland_logistics_hub);
        const entity_id hu = add_unit(h, 0, 30, 50);
        const refill_result rh = refill_quarter(h, reg);
        check(rh.plan.empty() && h.w.units.at(hu).count == 30, "R5j an empty pool refills nothing");

        // WHOLE RECRUITS (cold review): on a fading road a unit's share is a fraction, and
        // the pool pays only for the recruits it delivers. A hub at 0 and a road 0..7: a
        // unit at column 7 is 7 road steps out, reach 1 - 1/6, share 833.
        const auto fade_case = [&](int missing, int pool, int& share, int& delivered, float& extra) {
            fixture q; build(q);
            add_building(q, 0, building_type::inland_logistics_hub);
            lay_row_road(q, 0, 7);
            const entity_id u = add_unit(q, 7, 50 - missing, 50);
            world dry = q.w;
            dry.nations.at(q.nation).land_recruits = 0;
            const lp_tick base = resolve_logistic_points(dry, collect_supply_claims(dry, reg));
            nat(q).land_recruits = pool;
            const refill_result rq = refill_quarter(q, reg);
            const lp_supply_row* s = supply_row(rq.lp, u);
            const lp_supply_row* b = supply_row(base, u);
            share     = s ? s->share_permille : -1;
            delivered = q.w.units.at(u).count - (50 - missing);
            extra     = (s && b) ? s->granted - b->granted : -1.0f;
        };
        int sh1 = 0, d1 = 0, sh6 = 0, d6 = 0;
        float x1 = 0.0f, x6 = 0.0f;
        fade_case(1, 1000, sh1, d1, x1);
        fade_case(6, 1000, sh6, d6, x6);
        std::printf("      share %d: 1 missing -> %d delivered, %.4f extra points; 6 missing -> %d delivered, %.4f extra\n",
                    sh1, d1, x1, d6, x6);
        check(sh1 > 0 && sh1 < 1000 && d1 == 0 && x1 > -1e-4f && x1 < 1e-4f,
              "R5k a fractional share on ONE missing man delivers none and draws no point for it");
        check(sh6 == sh1 && d6 == 6 * sh6 / 1000 && x6 > static_cast<float>(d6) - 1e-3f
                  && x6 < static_cast<float>(d6) + 1e-3f,
              "R5l on six missing it delivers floor(6 x share) whole recruits and draws exactly 1 point each");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR10 - wings refill from the air pool (Ben, 2026-10-31)\n");
    {
        // A hub at 0 and a road 0..7. Airbases at column 2 (on the road, full cover), 7
        // (7 road steps out: share 833) and 11 within a row whose road ends at 2 is set
        // up separately for the 9-off case.
        const auto make_wing = [&](fixture& f, entity_id base, int hp) {
            const entity_id id = f.w.create_entity();
            air_wing wg;
            wg.owner = f.nation; wg.airbase = base; wg.type = 0; // fighter, crew 20
            wg.hp_ppm = hp; wg.muster_days = 0;
            f.w.wings.emplace(id, wg);
            return id;
        };
        const auto wing_quarter = [&](fixture& f) {
            refill_result r;
            const std::vector<wing_refill> wp = plan_wing_refills(f.w, reg);
            r.plan = plan_recruit_refills(f.w);
            r.lp = resolve_logistic_points(f.w, collect_supply_claims(f.w, reg, r.plan, wp));
            r.delivered = apply_wing_refills(f.w, reg, wp, r.lp);
            return std::make_pair(wp, r);
        };
        constexpr int full = k_wing_hp_full;

        fixture f; build(f);
        add_building(f, 0, building_type::inland_logistics_hub);
        lay_row_road(f, 0, 7);
        const entity_id on   = add_building(f, 2, building_type::airbase);
        const entity_id fade = add_building(f, 7, building_type::airbase);
        const entity_id w_half  = make_wing(f, on, full / 2);
        const entity_id w_hair  = make_wing(f, on, full - 1);
        const entity_id w_fhalf = make_wing(f, fade, full / 2);
        const entity_id w_fhair = make_wing(f, fade, full - 1);
        const entity_id w_full  = make_wing(f, on, full);
        nat(f).air_recruits = 1000;
        const int pool0 = nat(f).air_recruits;
        const std::vector<wing_refill> plan = plan_wing_refills(f.w, reg);
        const auto planned_of = [&](entity_id wid) {
            for (const wing_refill& r : plan) if (r.wing == wid) return r.planned;
            return -1;
        };
        check(planned_of(w_half) == 10 && planned_of(w_hair) == 1 && planned_of(w_full) == -1,
              "R10a the plan maps HP to crew: half of a fighter's 20 is 10; a wing one ppm short plans 1 "
              "(ceil, never 0 below full); a full wing plans nothing");
        world dry = f.w;
        dry.nations.at(f.nation).air_recruits = 0;
        const lp_tick base_lp = resolve_logistic_points(dry, collect_supply_claims(dry, reg));
        float base_granted = 0.0f;
        for (const lp_nation_row& n : base_lp.nations) if (n.nation == f.nation) base_granted = n.supply_granted;
        auto [wp, r] = wing_quarter(f);
        float granted = 0.0f;
        for (const lp_nation_row& n : r.lp.nations) if (n.nation == f.nation) granted = n.supply_granted;
        const int hp_half  = f.w.wings.at(w_half).hp_ppm,  hp_hair  = f.w.wings.at(w_hair).hp_ppm;
        const int hp_fhalf = f.w.wings.at(w_fhalf).hp_ppm, hp_fhair = f.w.wings.at(w_fhair).hp_ppm;
        std::printf("      on road: half -> %d, hair -> %d; fading (833): half -> %d, hair -> %d; "
                    "delivered %d, pool %d -> %d, points %.4f vs %.4f\n",
                    hp_half, hp_hair, hp_fhalf, hp_fhair, r.delivered, pool0, nat(f).air_recruits,
                    granted, base_granted);
        check(hp_half == full && hp_hair == full,
              "R10b on the road both refill to exactly full (10 recruits, and 1)");
        check(hp_fhalf == full / 2 + 8 * (full / 20) && hp_fhair == full - 1,
              "R10c at share 833 the half wing gets 8 whole recruits (+8/20 HP); the hair-short wing "
              "gets none and stays put");
        check(r.delivered == 19 && nat(f).air_recruits == pool0 - 19,
              "R10d conservation: 19 recruits delivered, exactly 19 taken from the air pool");
        check(granted > base_granted + 19.0f - 1e-3f && granted < base_granted + 19.0f + 1e-3f,
              "R10e and exactly 19 points drawn for them: none for the fraction nobody received");
        check(nat(f).land_recruits == 0, "R10f a wing draws on the air pool only");

        fixture g; build(g);
        add_building(g, 0, building_type::inland_logistics_hub);
        lay_row_road(g, 0, 2);
        const entity_id far  = add_building(g, 11, building_type::airbase); // 9 tiles past the road
        const entity_id near = add_building(g, 10, building_type::airbase); // 8 tiles past
        const entity_id w_far  = make_wing(g, far, full / 2);
        const entity_id w_near = make_wing(g, near, full / 2);
        nat(g).air_recruits = 1000;
        (void)wing_quarter(g);
        check(g.w.wings.at(w_near).hp_ppm == full && g.w.wings.at(w_far).hp_ppm == full / 2
                  && nat(g).air_recruits == 990,
              "R10g an airbase 8 tiles off its road refills whole; 9 off, nothing");

        fixture s; build(s);
        add_building(s, 0, building_type::inland_logistics_hub);
        const entity_id sb = add_building(s, 0, building_type::airbase);
        const entity_id a = make_wing(s, sb, full / 2);
        const entity_id b = make_wing(s, sb, full / 2);
        nat(s).air_recruits = 14;
        (void)wing_quarter(s);
        check(a < b && s.w.wings.at(a).hp_ppm == full && s.w.wings.at(b).hp_ppm == full / 2 + 4 * (full / 20)
                  && nat(s).air_recruits == 0,
              "R10h a short air pool (14) refills in ascending wing id: 10 to the first, 4 to the second");

        const auto twice = [&]() {
            fixture q; build(q);
            add_building(q, 0, building_type::inland_logistics_hub);
            lay_row_road(q, 0, 7);
            const entity_id b1 = add_building(q, 3, building_type::airbase);
            const entity_id b2 = add_building(q, 7, building_type::airbase);
            make_wing(q, b1, 300000); make_wing(q, b2, 610000); make_wing(q, b1, 999000);
            add_unit(q, 1, 20, 50);
            nat(q).air_recruits = 25; nat(q).land_recruits = 100;
            for (int k = 0; k < 4; ++k)
            {
                const std::vector<wing_refill> p = plan_wing_refills(q.w, reg);
                const std::vector<recruit_refill> lpn = plan_recruit_refills(q.w);
                const lp_tick t = resolve_logistic_points(q.w, collect_supply_claims(q.w, reg, lpn, p));
                (void)apply_recruit_refills(q.w, lpn, t);
                (void)apply_wing_refills(q.w, reg, p, t);
            }
            return to_bytes(q.w);
        };
        check(twice() == twice(), "R10i determinism: four quarters of land and wing refills, twice, byte-identical");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR11 - a dropped muster refunds its men (Ben, 2026-10-31)\n");
    {
        fixture f; build(f);
        const entity_id base = add_building(f, 5, building_type::military_base);
        const auto avail = available_rows(f.w, f.nation, campaign_roster_band_for(reg.era()));
        const uint16_t row = avail.empty() ? 0
            : static_cast<uint16_t>(avail.front() - unit_roster_table().data());
        nat(f).land_recruits = 800; // above the 750 cap one base gives
        check(hire_unit(f, reg, 5, row) == corp_command_result::applied && nat(f).land_recruits == 750
                  && f.w.musters.size() == 1,
              "R11a the hire spends its 50 (800 -> 750)");
        f.w.buildings.erase(base); // the base is lost mid-muster
        const float treasury = nat(f).treasury;
        advance_musters(f.w);
        check(f.w.musters.empty() && nat(f).land_recruits == 800 && f.w.units.empty(),
              "R11b the dropped muster returns its 50 recruits, uncapped by the pool's cap (750 -> 800)");
        check(nat(f).treasury == treasury, "R11c credits stay as they are: no refund of money");

        fixture d; build(d);
        const entity_id db = add_building(d, 5, building_type::military_base);
        nat(d).land_recruits = 50;
        (void)hire_unit(d, reg, 5, row);
        d.w.buildings.at(db).decommissioned = true;
        advance_musters(d.w);
        check(d.w.musters.empty() && nat(d).land_recruits == 50,
              "R11d a muster at a decommissioned base is dropped and refunded too");

        muster_order m;
        m.owner = f.nation; m.unit_type = row;
        nat(f).land_recruits = k_recruit_pool_max - 10;
        check(refund_muster_recruits(f.w, m) == 10 && nat(f).land_recruits == k_recruit_pool_max,
              "R11e a refund stops only at the save's pool ceiling (2^30)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR6 - bases carry no upkeep\n");
    {
        const building_economics& mb = reg.economics(building_type::military_base);
        const building_economics& ab = reg.economics(building_type::airbase);
        std::printf("      military base maintenance %.2f wage %.2f; airbase %.2f / %.2f\n",
                    mb.maintenance, mb.base_wage, ab.maintenance, ab.base_wage);
        check(mb.maintenance == 0.0f && mb.base_wage == 0.0f, "R6a a military base: no maintenance, no wage");
        check(ab.maintenance == 0.0f && ab.base_wage == 0.0f, "R6b an airbase: no maintenance, no wage");
        building_component b{};
        b.type = building_type::military_base;
        const building_opex o = compute_building_opex(b, mb, 1.0f, 1.0f, reg.idle_maintenance_floor());
        b.type = building_type::airbase;
        const building_opex o2 = compute_building_opex(b, ab, 1.0f, 1.0f, reg.idle_maintenance_floor());
        check(o.maintenance == 0.0f && o.wages == 0.0f && o2.maintenance == 0.0f && o2.wages == 0.0f,
              "R6c so a completed base's opex is zero");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR7 - the save\n");
    {
        fixture f; build(f);
        const entity_id c = add_centre(f, 3, 5, /*carry=*/0);
        add_building(f, 5, building_type::military_base);
        add_building(f, 6, building_type::airbase);
        (void)run_recruitment(f.w, reg);
        const std::string bytes = to_bytes(f.w);
        world back;
        const bool ok = from_bytes(bytes, back);
        check(ok && back.nations.at(f.nation).land_recruits == 50 && back.nations.at(f.nation).air_recruits == 20
                  && back.population_centres.at(c).recruit_heads_taken == 70,
              "R7a pools (50, 20) and the centre's carry (70) round-trip");
        check(ok && to_bytes(back) == bytes && back.state_hash(1) == f.w.state_hash(1),
              "R7b the loaded world re-writes byte-identical and hashes the same");
        check(f.w.state_hash(1) != [&] { world z = f.w; z.nations.at(f.nation).land_recruits = 0; return z.state_hash(1); }(),
              "R7c the pool is in the state hash");

        std::ostringstream o67(std::ios::binary);
        write_world_snapshot_pre_recruits(f.w, o67);
        const std::string v67 = o67.str();
        world old;
        const bool old_ok = from_bytes(v67, old);
        check(old_ok && old.nations.at(f.nation).land_recruits == 0 && old.nations.at(f.nation).air_recruits == 0
                  && old.population_centres.at(c).recruit_heads_taken == 0,
              "R7d a v71 stream reads, its pools empty and its carries 0");
        check(v67.size() + world_save_recruits_section_bytes(f.w) + world_save_kit_goods_bytes(f.w) /* BL-1422 */ == bytes.size()
                  && world_save_recruits_section_bytes(f.w) == 4 + 12 + 4 + 8,
              "R7e a v71 stream is today's less exactly the recruits section (28 bytes here)");
        {
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_pre_alarm(f.w, o);
            const std::string v67s = o.str();
            world w67;
            const bool ok67 = from_bytes(v67s, w67);
            check(ok67 && w67.nations.at(f.nation).land_recruits == 0
                      && v67s.size() + world_save_alarm_section_bytes(f.w) + world_save_war_section_bytes(f.w)
                             + world_save_embargo_section_bytes(f.w) + world_save_gen_capital_section_bytes(f.w)
                             + world_save_recruits_section_bytes(f.w) + world_save_kit_goods_bytes(f.w) /* BL-1422 (v73) */
                             == bytes.size(),
                  "R7e2 a v67 stream reads with empty pools and is today's less the Alarm (v68), war (v69), embargo (v70), generation-capital (v71) AND recruits (v72) sections");
        }

        // Corrupt rows. The section is the tail: [n=1][id][land][air][m=1][cid][carry].
        const std::size_t land_at = bytes.size() - 8 - 4 - 8; // land is 8 bytes into the nation row
        {
            std::string bad = bytes;
            const int32_t neg = -1;
            std::memcpy(&bad[land_at], &neg, sizeof neg);
            world z;
            check(!from_bytes(bad, z) && z.nations.empty(), "R7f a negative pool is refused whole");
        }
        {
            std::string bad = bytes;
            const int32_t zero = 0;
            std::memcpy(&bad[land_at], &zero, sizeof zero);
            std::memcpy(&bad[land_at + 4], &zero, sizeof zero);
            world z;
            check(!from_bytes(bad, z) && z.nations.empty(), "R7g a nation row with both pools 0 is refused whole");
        }
        {
            std::string bad = bytes;
            const int32_t big = 1000;
            std::memcpy(&bad[bytes.size() - 4], &big, sizeof big);
            world z;
            check(!from_bytes(bad, z) && z.nations.empty(), "R7h a centre carry of 1000 is refused whole");
        }
        {
            std::string bad = bytes;
            const entity_id ghost = 0x7FFFFFF0u;
            std::memcpy(&bad[bytes.size() - 8], &ghost, sizeof ghost);
            world z;
            check(!from_bytes(bad, z) && z.nations.empty(), "R7i a carry naming no centre is refused whole");
        }
    }

    // -----------------------------------------------------------------------
    std::printf("\nR8 - determinism and the real step\n");
    {
        const auto run = [&]() {
            fixture f; build(f);
            add_centre(f, 3, 3); add_centre(f, 4, 3); add_centre(f, 25, 7);
            add_building(f, 5, building_type::military_base);
            add_building(f, 22, building_type::military_base);
            add_building(f, 6, building_type::airbase);
            add_building(f, 0, building_type::inland_logistics_hub);
            lay_row_road(f, 0, 2);
            add_unit(f, 1, 10, 50); add_unit(f, 2, 20, 50);
            for (int q = 0; q < 6; ++q)
            {
                (void)run_recruitment(f.w, reg);
                (void)refill_quarter(f, reg);
            }
            return to_bytes(f.w);
        };
        check(run() == run(), "R8a six quarters of recruiting and refilling, twice: byte-identical");

        fixture f; build(f);
        add_centre(f, 3, 5);
        add_building(f, 5, building_type::military_base);
        add_building(f, 6, building_type::airbase);
        const economy_report rep = run_economy_step(f.w, reg, /*spectating=*/false);
        std::printf("      after one economy step: land %d air %d; report heads %lld\n",
                    nat(f).land_recruits, nat(f).air_recruits,
                    static_cast<long long>(rep.recruitment.heads_taken));
        check(nat(f).land_recruits == 50 && nat(f).air_recruits == 20 && rep.recruitment.heads_taken == 70,
              "R8b the economy step makes the quarter's recruits");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR9 - the loader rejects a bad recruits value\n");
    {
        const auto rejects = [](const char* patch) {
            lua_state l;
            l.load("scripts/recipes.lua");
            l.load("scripts/economy.lua");
            l.state().safe_script(patch);
            recipe_registry r;
            try { r.load_from_lua(l); }
            catch (const std::exception& e) { std::printf("      rejected: %s\n", e.what()); return true; }
            return false;
        };
        check(rejects("economy.military.recruits.land_per_base = -1"), "R9a a negative rate");
        check(rejects("economy.military.recruits.air_per_base = 2.5"), "R9b a fractional rate");
        check(rejects("economy.military.recruits.lp_per_recruit = 1/0"), "R9c an infinite LP cost");
        check(rejects("economy.military.recruits.sea_per_base = 3"), "R9d an unknown key");
        check(rejects("economy.military.air.classes.fighter.crew = -4"), "R9e a negative crew");
        check(rejects("economy.military.recruits.land_per_base = 1001"),
              "R9f a rate past the ceiling (1000 a base a quarter) is refused");
        {
            lua_state l;
            l.load("scripts/recipes.lua");
            l.load("scripts/economy.lua");
            l.state().safe_script("economy.military.recruits.land_per_base = 1000");
            recipe_registry r;
            bool ok = true;
            try { r.load_from_lua(l); } catch (const std::exception&) { ok = false; }
            check(ok && r.military().recruits.land_per_base == k_recruit_rate_max,
                  "R9g the ceiling itself (1000) loads");
        }
        check(static_cast<int64_t>(k_recruit_cap_quarters) * k_recruit_rate_max * 71583 > k_recruit_pool_max
                  && static_cast<int64_t>(k_recruit_cap_quarters) * k_recruit_rate_max * 71582 <= k_recruit_pool_max,
              "R9h at the ceiling rate a theatre's cap reaches the 2^30 pool ceiling only past 71,582 bases");
        {
            // And past it the cap is clamped, never wrapped: 72,000 running bases of one nation.
            fixture f; build(f);
            nation_component& n = nat(f);
            for (int i = 0; i < 72000; ++i)
            {
                const entity_id id = f.w.create_entity();
                building_component b{};
                b.tile = tile(f, 5); b.type = building_type::military_base;
                f.w.buildings[id] = b;
                n.state_assets.push_back(id);
            }
            recipe_registry big = reg;
            military_capability_params mp = big.military();
            mp.recruits.land_per_base = k_recruit_rate_max;
            big.set_military(mp);
            const int cap = recruit_cap(f.w, big, f.nation, recruit_theatre::land);
            check(cap == k_recruit_pool_max, "R9i 72,000 bases at the ceiling rate: the cap is clamped to 2^30 ("
                                                 + std::to_string(cap) + ")");
        }
    }

    std::printf("\nrecruits_harness: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
