// ---------------------------------------------------------------------------
// war_fix_harness - the cold review's findings on occupation, defend province and
// convoy lanes (BL-1502, BL-1504, BL-1505). No SDL/Lua.
// ---------------------------------------------------------------------------
// Each row goes through the path the game runs unless it says otherwise: a peace through
// offer_peace + accept_peace (conclude_peace), a withdrawal through request_withdraw +
// run_battles, a step-off through resolve_shared_tiles, a day through run_military_day,
// a trade through run_trades, a convoy through advance_convoys + credit_arrived_convoys.
//
//   W3   occupied ground: universities, institutes, musters, extraction, processors.
//   W4   every battle-system move occupies, and an owner re-entering frees its ground.
//   W5   peace walks a stack left deep in returned ground, whichever province it is in.
//   W7   a convoy in flight when its lane moves: where it stands is kept.
//   W8   a march or halt on a defender clears its order rather than being reversed.
//   W10  returned cargo: the embargo's refusal and record; the owner is paid.
//   W11  the auto path reaches the "front closed" reason.
//   W12  a line whose nearest egress crosses a front tries the next station.
//   W14  the save reader refuses what the verb refuses.
//
// Run: node tools/verify/build_harness.js war_fix_harness --run
// ---------------------------------------------------------------------------

#include "harness_params.hpp"
#include "world/battle_system.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/embargo.hpp"
#include "world/fronts.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/nation_tech.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/supply_system.hpp"
#include "world/trade.hpp"
#include "world/unit_roster.hpp"
#include "world/war.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
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

int row_named(const char* name)
{
    const auto& t = unit_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
        if (std::string(t[i].name) == name) return static_cast<int>(i);
    return -1;
}

int naval_row()
{
    const auto& t = unit_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
        if ((t[i].domains & domain_bit(traversal_domain::land)) == 0) return static_cast<int>(i);
    return -1;
}

recipe_registry make_registry()
{
    recipe_registry reg;
    military_capability_params mp = reg.military();
    const float per_call[unit_class_count] = { 1.0f, 1.5f, 1.0f, 0.5f, 0.0f };
    for (int k = 0; k < unit_class_count; ++k)
        mp.march_days_per_cls[static_cast<std::size_t>(k)] = per_call[k] > 0.0f ? 1.0f / per_call[k] : 0.0f;
    for (int c = 0; c < land_class_count; ++c)
        if (const int row = modern_row_for(static_cast<land_class>(c)); row >= 0)
            mp.march_days_per_land_class[static_cast<std::size_t>(c)] =
                mp.march_days_per_cls[static_cast<std::size_t>(unit_roster_table()[static_cast<std::size_t>(row)].cls)];
    reg.set_military(mp);
    return reg;
}

// --- the war fixture: A owns rows 0-2 tile by tile; B rows 3-5 in 3 x 3 blocks ------

constexpr int W = 12, H = 6;

struct fixture
{
    world     w;
    entity_id body = null_entity, A = null_entity, B = null_entity, C = null_entity;
    std::vector<entity_id> cell;
    entity_id at(int x, int y) const { return cell[static_cast<std::size_t>(y * W + x)]; }
    uint32_t  block(int k) const { return at(3 * k, 3); }
};

void build(fixture& f)
{
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Cylinder"; bc.type = body_type::planet; bc.grid_width = W; bc.grid_height = H;
    w.bodies[f.body] = bc;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
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
    f.A = nation("Aster");
    f.B = nation("Brael");
    f.C = nation("Corvan");
    w.player_entity = f.A;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            w.set_tile_nation(f.at(x, y), y < 3 ? f.A : f.B);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < W; ++x)
        {
            province p; p.id = f.at(x, y); p.body = f.body; p.tiles = { f.at(x, y) };
            w.provinces.provinces.push_back(p);
        }
    for (int k = 0; k < 4; ++k)
    {
        province p; p.id = f.block(k); p.body = f.body;
        for (int y = 3; y < H; ++y)
            for (int x = 3 * k; x < 3 * k + 3; ++x)
                p.tiles.push_back(f.at(x, y));
        std::sort(p.tiles.begin(), p.tiles.end());
        w.provinces.provinces.push_back(p);
    }
    for (const province& p : w.provinces.provinces)
        for (const entity_id t : p.tiles)
            w.provinces.tile_province[t] = p.id;
    w.province_holder.assign(w.provinces.provinces.size(), null_entity);
    for (std::size_t i = 0; i < w.provinces.provinces.size(); ++i)
    {
        const auto own = w.tile_to_nation.find(w.provinces.provinces[i].tiles.front());
        if (own != w.tile_to_nation.end())
            w.province_holder[i] = own->second;
    }
}

entity_id add_unit(fixture& f, entity_id owner, entity_id tile, int count = 40)
{
    const entity_id u = f.w.create_entity();
    unit_component uc{};
    uc.position = tile; uc.owner = owner; uc.count = count;
    uc.type = static_cast<uint16_t>(row_named("Rifle Infantry"));
    f.w.units[u] = uc;
    return u;
}

void open_war(fixture& f, entity_id a, entity_id b, uint32_t goal)
{
    war_record rec; rec.declarer = a; rec.defender = b; rec.declarer_goals = { goal };
    f.w.wars[{ a, b }] = rec;
    (void)declare_hostile(f.w, a, b);
    (void)declare_hostile(f.w, b, a);
}

entity_id add_building(fixture& f, entity_id owner, entity_id tile, building_type type)
{
    const entity_id b = f.w.create_entity();
    building_component bc{};
    bc.tile = tile; bc.type = type;
    bc.workforce_assigned = 0.5f;
    bc.workforce_target = 100.0f;
    f.w.buildings[b] = bc;
    f.w.nations.at(owner).state_assets.push_back(b);
    return b;
}

corp_command_result order_of(world& w, const recipe_registry& reg, entity_id actor, corp_verb verb,
                             entity_id unit, entity_id tile, uint32_t province = no_province)
{
    corp_command c; c.corp = actor; c.as_nation = actor; c.verb = verb;
    c.subject = unit; c.tile = tile; c.province = province;
    return apply_corp_command(w, reg, c);
}

/// A white peace through the verbs' own door: offered, then accepted (conclude_peace).
bool white_peace(world& w, entity_id a, entity_id b)
{
    const sentiment_params sp{};
    peace_terms t;
    t.kind = peace_kind::white;
    return offer_peace(w, a, b, t) == war_result::applied
        && accept_peace(w, sp, b, a) == war_result::applied;
}

// --- the convoy fixture: A columns 0-5, C 6-9, B 10-15, markets at (2,3) and (13,3) ---

constexpr int CW = 16, CH = 6;
constexpr std::size_t k_good = static_cast<std::size_t>(resource_type::iron_ore);

struct lanes
{
    world     w;
    entity_id body = null_entity, A = null_entity, B = null_entity, C = null_entity;
    entity_id ma = null_entity, mb = null_entity;
    std::vector<entity_id> cell;
    entity_id at(int x, int y) const { return cell[static_cast<std::size_t>(y * CW + x)]; }
};

entity_id add_market(lanes& f, entity_id centre)
{
    const entity_id m = f.w.create_entity();
    market_component mc{};
    mc.body = f.body;
    mc.centre_tile = centre;
    mc.base_price[k_good] = 2.0f;
    mc.price = mc.base_price;
    mc.inventory[k_good] = 100.0f;
    f.w.markets[m] = mc;
    return m;
}

void build(lanes& f)
{
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Cylinder"; bc.type = body_type::planet; bc.grid_width = CW; bc.grid_height = CH;
    w.bodies[f.body] = bc;
    for (int y = 0; y < CH; ++y)
        for (int x = 0; x < CW; ++x)
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
    f.A = nation("Aster");
    f.B = nation("Brael");
    f.C = nation("Corvan");
    w.player_entity = f.A;
    for (int y = 0; y < CH; ++y)
        for (int x = 0; x < CW; ++x)
            w.set_tile_nation(f.at(x, y), x <= 5 ? f.A : x <= 9 ? f.C : f.B);
    f.ma = add_market(f, f.at(2, 3));
    f.mb = add_market(f, f.at(13, 3));
}

void open_war(lanes& f)
{
    war_record rec;
    rec.declarer = f.A; rec.defender = f.B;
    rec.declarer_goals = { 1u };
    f.w.wars[{ f.A, f.B }] = rec;
    (void)declare_hostile(f.w, f.A, f.B);
}

/// A convoy as the dispatch makes one: its speed is 1 / the priced leg's ticks.
convoy_component shipped(const lanes& f, const convoy_leg& leg, entity_id owner, std::uint32_t id, float progress)
{
    convoy_component c;
    c.id             = id;
    c.source_market  = f.ma;
    c.dest_market    = f.mb;
    c.mode           = convoy_mode::land;
    c.cargo_resource = resource_type::iron_ore;
    c.cargo_qty      = 10.0f;
    c.progress       = progress;
    c.speed          = 1.0f / static_cast<float>(leg.travel_ticks);
    c.corp           = owner;
    c.origin_tile    = f.at(2, 3);
    return c;
}

entity_id head_of(world& w, const convoy_component& c)
{
    const convoy_route r = convoy_route_tiles(w, c);
    const int i = convoy_lane_index(r.at, c.progress);
    return i < 0 ? null_entity : r.tiles[static_cast<std::size_t>(i)];
}

int hops_between(const world& w, entity_id a, entity_id b)
{
    const tile_component& ta = w.tiles.at(a);
    const tile_component& tb = w.tiles.at(b);
    const int gw = w.bodies.at(ta.body).grid_width;
    int dx = std::abs(ta.grid_x - tb.grid_x);
    dx = std::min(dx, gw - dx);
    return dx + std::abs(ta.grid_y - tb.grid_y);
}

float landed_at(const world& w, entity_id owner, entity_id market)
{
    const auto it = w.landed_this_tick.find({ owner, market });
    return it == w.landed_this_tick.end() ? 0.0f : it->second.quantities[k_good];
}

} // namespace

int main()
{
    const recipe_registry reg = make_registry();
    std::printf("=== war fixes ===\n");

    // -----------------------------------------------------------------------
    std::printf("\nW3 - occupied ground makes nothing, raises nothing\n");
    {
        fixture f; build(f);
        open_war(f, f.B, f.A, f.at(1, 1));
        const entity_id held = f.at(1, 1), free_tile = f.at(4, 1);
        add_building(f, f.A, held, building_type::university);
        add_building(f, f.A, free_tile, building_type::university);
        add_building(f, f.A, held, building_type::research_institute);
        add_building(f, f.A, free_tile, building_type::research_institute);
        f.w.tile_occupier[held] = f.B;
        std::vector<research_accrual> rows;
        advance_research(f.w, research_params{}, &rows);
        int unis = -1, insts = -1;
        for (const research_accrual& r : rows)
            if (r.nation == f.A) { unis = r.universities; insts = r.institutes; }
        check(unis == 1, "W3a a university on occupied ground counts for no research points");
        check(insts == 1, "W3b nor does an institute");

        // Through advance_musters, the economy step's own call.
        const entity_id base = add_building(f, f.A, held, building_type::military_base);
        muster_order m;
        m.id = 1; m.base = base; m.owner = f.A; m.days_remaining = 1;
        m.unit_type = static_cast<uint16_t>(row_named("Rifle Infantry"));
        f.w.musters.push_back(m);
        const std::size_t units_before = f.w.units.size();
        advance_musters(f.w);
        check(f.w.units.size() == units_before && f.w.musters.size() == 1,
              "W3c a finished muster on occupied ground raises nobody, and waits");
        f.w.tile_occupier.erase(held);
        advance_musters(f.w);
        check(f.w.units.size() == units_before + 1 && f.w.musters.empty(),
              "W3d freed, it raises on the next tick");
    }
    {
        // The economy step's production pass, whole: a site on occupied ground is not run.
        fixture f; build(f);
        open_war(f, f.B, f.A, f.at(1, 1));
        const entity_id occ_site  = add_building(f, f.A, f.at(1, 1), building_type::extraction_site);
        const entity_id free_site = add_building(f, f.A, f.at(4, 1), building_type::extraction_site);
        const entity_id occ_proc  = add_building(f, f.A, f.at(2, 1), building_type::processing_facility);
        const entity_id free_proc = add_building(f, f.A, f.at(5, 1), building_type::processing_facility);
        f.w.tile_occupier[f.at(1, 1)] = f.B;
        f.w.tile_occupier[f.at(2, 1)] = f.B;
        const economy_report rep = run_economy_step(f.w, reg);
        bool os = false, fs = false, op = false, fp = false;
        for (const auto& row : rep.buildings)
        {
            os = os || row.building == occ_site;
            fs = fs || row.building == free_site;
            op = op || row.building == occ_proc;
            fp = fp || row.building == free_proc;
        }
        check(fs && !os, "W3e an extraction site on occupied ground is not run; its free twin is");
        check(fp && !op, "W3f a processor likewise, its shelf claim with it");
    }

    // -----------------------------------------------------------------------
    std::printf("\nW4 - every battle-system move reads the occupation layer\n");
    {
        // Step-off, through resolve_shared_tiles: B's unit shares a tile with A's lower-id
        // stack and steps onto its own ground, which A holds.
        fixture f; build(f);
        open_war(f, f.A, f.B, f.block(1));
        const entity_id here = f.at(5, 4);
        (void)add_unit(f, f.A, here);
        const entity_id ub = add_unit(f, f.B, here);
        for (int y = 3; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (f.at(x, y) != here)
                    f.w.tile_occupier[f.at(x, y)] = f.A;
        (void)resolve_shared_tiles(f.w, &reg.military());
        const entity_id to = f.w.units.at(ub).position;
        check(to != here && f.w.tile_to_nation.at(to) == f.B && f.w.tile_controller(to) == f.B,
              "W4a a step-off onto its own occupied ground frees it");
    }
    {
        // Withdrawal, through request_withdraw and the day's battles (retreat_side).
        fixture f; build(f);
        open_war(f, f.A, f.B, f.block(1));
        const entity_id here = f.at(5, 4);
        const entity_id ua = add_unit(f, f.A, f.at(5, 2));
        const entity_id ub = add_unit(f, f.B, here);
        for (int y = 3; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (f.at(x, y) != here)
                    f.w.tile_occupier[f.at(x, y)] = f.A;
        active_battle b;
        b.tile = here; b.attacker = f.A; b.defender = f.B;
        b.attacker_units = { ua };
        b.defender_units = { ub };
        b.attacking_units = { ua };
        f.w.battles.push_back(b);
        const bool asked = request_withdraw(f.w, f.B, here, f.A);
        (void)run_battles(f.w, reg, 1);
        const entity_id to = f.w.units.at(ub).position;
        check(asked && to != here && f.w.tile_to_nation.at(to) == f.B && f.w.tile_controller(to) == f.B,
              "W4b a withdrawal onto its own occupied ground frees it");
    }
    {
        // The peace walk, through conclude_peace: A's stack on B's goal walks home onto
        // A's ground that C holds, in a war that goes on.
        fixture f; build(f);
        open_war(f, f.A, f.B, f.block(1));
        open_war(f, f.C, f.A, f.at(0, 0));
        const entity_id ua = add_unit(f, f.A, f.at(4, 3));
        f.w.tile_occupier[f.at(4, 3)] = f.A;
        for (int x = 0; x < W; ++x)
            f.w.tile_occupier[f.at(x, 2)] = f.C;
        const bool done = white_peace(f.w, f.A, f.B);
        const entity_id to = f.w.units.at(ua).position;
        check(done && f.w.tile_to_nation.at(to) == f.A && f.w.tile_controller(to) == f.A,
              "W4c the peace walk onto its own ground frees it from a war that goes on");
    }

    // -----------------------------------------------------------------------
    std::printf("\nW5 - a peace walks a stack off any ground it returned\n");
    {
        // Occupation reaches past the goal: A's stack stands in block 3, the goal is block 1.
        fixture f; build(f);
        open_war(f, f.A, f.B, f.block(1));
        const entity_id deep = add_unit(f, f.A, f.at(10, 5));
        f.w.tile_occupier[f.at(10, 5)] = f.A;
        const entity_id ground = add_unit(f, f.B, f.at(7, 1)); // B's on A's ground, too
        f.w.tile_occupier[f.at(7, 1)] = f.B;
        const bool done = white_peace(f.w, f.A, f.B);
        check(done && f.w.tile_occupier.empty(), "W5a the peace cleared the layer (through conclude_peace)");
        check(f.w.tile_to_nation.at(f.w.units.at(deep).position) == f.A,
              "W5b the declarer's stack deep in a non-goal province walked home");
        check(f.w.tile_to_nation.at(f.w.units.at(ground).position) == f.B,
              "W5c the defender's stack on the declarer's ground did too");
    }

    // -----------------------------------------------------------------------
    std::printf("\nW7 - a convoy in flight when its lane moves\n");
    {
        lanes f; build(f);
        const logistics_nodes nodes = collect_logistics_nodes(f.w);
        const convoy_leg peace = price_trade_leg(f.w, reg, nodes, f.A, f.ma, f.mb, k_good, 1.0f);
        const int peace_ticks = peace.travel_ticks;
        std::printf("  INFO peace leg: %d tick(s)\n", peace_ticks);
        check(peace.viable && peace_ticks >= 3, "W7a the fixture's leg takes several ticks");
        // On the near side of the seam, a little way along.
        f.w.convoys.push_back(shipped(f, peace, f.A, 1, 0.2f));
        const entity_id before = head_of(f.w, f.w.convoys[0]);
        open_war(f);
        advance_convoys(f.w);
        const entity_id after = head_of(f.w, f.w.convoys[0]);
        std::printf("  INFO head moved %d hop(s) on the first tick of the war\n", hops_between(f.w, before, after));
        check(hops_between(f.w, before, after) <= 2 + hops_between(f.w, f.at(2, 3), f.at(1, 3)) * 8 / peace_ticks,
              "W7b it does not teleport: the head stays near where it stood");
        int ticks = 1;
        while (!f.w.convoys.empty() && !f.w.convoys[0].arrived && ticks < 200)
        {
            advance_convoys(f.w);
            ++ticks;
        }
        const convoy_leg round = price_trade_leg(f.w, reg, nodes, f.A, f.ma, f.mb, k_good, 1.0f);
        std::printf("  INFO detour leg %d tick(s); arrived after %d more\n", round.travel_ticks, ticks);
        const float gone = 0.2f * static_cast<float>(peace_ticks);
        check(static_cast<float>(ticks) + gone > static_cast<float>(peace_ticks) + 1.0f,
              "W7c the detour costs time: it arrives later than the peace lane would");
    }
    {
        // Past the front when the front closes the whole lane: it flies on, to its destination.
        lanes f; build(f);
        const logistics_nodes nodes = collect_logistics_nodes(f.w);
        const convoy_leg peace = price_trade_leg(f.w, reg, nodes, f.A, f.ma, f.mb, k_good, 1.0f);
        for (int y = 0; y < CH; ++y)
            for (int x = 6; x <= 9; ++x)
                f.w.set_tile_nation(f.at(x, y), f.B);
        invalidate_logistics_caches(f.w);
        f.w.convoys.push_back(shipped(f, peace, f.A, 2, 0.8f));
        const entity_id before = head_of(f.w, f.w.convoys[0]);
        check(f.w.tile_to_nation.at(before) == f.B, "W7d the convoy stands past the seam");
        open_war(f);
        advance_convoys(f.w);
        credit_arrived_convoys(f.w, 1);
        check(landed_at(f.w, f.A, f.ma) == 0.0f, "W7e a convoy past the front does not land at its source");
        int ticks = 0;
        while (!f.w.convoys.empty() && ticks < 50)
        {
            advance_convoys(f.w);
            credit_arrived_convoys(f.w, 2 + ticks);
            ++ticks;
        }
        check(landed_at(f.w, f.A, f.mb) == 10.0f, "W7f it lands at its destination, whole");
    }

    // -----------------------------------------------------------------------
    std::printf("\nW8 - a hand order on a defender is obeyed\n");
    {
        fixture f; build(f);
        const entity_id d = add_unit(f, f.B, f.at(4, 5));
        reanchor_fronts(f.w);
        check(order_of(f.w, reg, f.B, corp_verb::defend_province, d, null_entity, f.block(1))
                  == corp_command_result::applied, "W8a the order is taken");
        check(order_of(f.w, reg, f.B, corp_verb::march_unit, d, f.at(10, 5)) == corp_command_result::applied,
              "W8b a march on it answers applied");
        check(f.w.units.at(d).defend_province == no_province, "W8c and clears the defend order, in the verb");
        (void)run_military_day(f.w, reg, 1);
        check(f.w.units.at(d).order.dest == f.at(10, 5), "W8d the next military day does not reverse the march");

        const entity_id g = add_unit(f, f.B, f.at(3, 3));
        (void)order_of(f.w, reg, f.B, corp_verb::defend_province, g, null_entity, f.block(1));
        (void)declare_hostile(f.w, f.B, f.A);
        const entity_id e = add_unit(f, f.A, f.at(5, 3));
        (void)run_military_day(f.w, reg, 2);
        bool fighting = false;
        for (const active_battle& bt : f.w.battles)
            fighting = fighting || (bt.tile == f.at(5, 3) && bt.attacker == f.B);
        check(f.w.units.at(g).defend_province == f.block(1) && f.w.units.at(g).position == f.at(4, 3) && fighting,
              "W8e the reaction keeps the order: run_military_day walks it onto the stack, a battle");
        (void)e;
        // Back to a walk home: the fight over and the unit far from its post. The position is
        // set by hand; the order that follows is the day's own.
        f.w.battles.clear();
        f.w.units.erase(e);
        f.w.units.at(g).position = f.at(5, 5);
        (void)run_military_day(f.w, reg, 3);
        check(f.w.units.at(g).defend_province == f.block(1) && f.w.units.at(g).order.dest == f.at(3, 3),
              "W8f it walks home on the order, still defending");
        check(order_of(f.w, reg, f.B, corp_verb::halt_unit, g, null_entity) == corp_command_result::applied
                  && f.w.units.at(g).defend_province == no_province,
              "W8g a halt on it clears the order too");
    }

    // -----------------------------------------------------------------------
    std::printf("\nW10 - cargo returned by a front\n");
    {
        // Water at column 15 cuts the seam, D holds columns 0-2 and A 3-5, so a lane from
        // D's market to B runs through A: a war closes it, and the cargo returns to D's.
        lanes f; build(f);
        const entity_id D = f.w.create_entity();
        { nation_component nc{}; nc.name = "Dorin"; nc.treasury = 1.0e6f; f.w.nations[D] = nc; }
        for (int y = 0; y < CH; ++y)
        {
            for (int x = 0; x <= 2; ++x) f.w.set_tile_nation(f.at(x, y), D);
            for (int x = 3; x <= 5; ++x) f.w.set_tile_nation(f.at(x, y), f.A);
            for (int x = 6; x <= 14; ++x) f.w.set_tile_nation(f.at(x, y), f.B);
            f.w.tiles.at(f.at(15, y)).substrate = terrain_substrate::ocean;
        }
        f.w.markets.at(f.mb).centre_tile = f.at(10, 3);
        invalidate_logistics_caches(f.w);
        const logistics_nodes nodes = collect_logistics_nodes(f.w);
        const convoy_leg peace = price_trade_leg(f.w, reg, nodes, f.A, f.ma, f.mb, k_good, 1.0f);
        check(peace.viable, "W10a at peace D's market ships to B's through A's ground");
        open_war(f);
        check(!price_trade_leg(f.w, reg, nodes, f.A, f.ma, f.mb, k_good, 1.0f).viable, "W10b at war it has no route");

        world refused = f.w;
        check(set_embargo(refused, D, f.A, true) == embargo_result::applied, "W10c D embargoes A");
        refused.convoys.push_back(shipped(f, peace, f.A, 3, 0.1f));
        std::vector<interception_record> cuts;
        advance_convoys(refused);
        credit_arrived_convoys(refused, 1, &cuts);
        bool recorded = false;
        for (const interception_record& r : cuts)
            recorded = recorded || (r.convoy_id == 3 && r.outcome == interception_outcome::refused_embargo);
        check(landed_at(refused, f.A, f.ma) == 0.0f, "W10d the source's embargo refuses the returning cargo");
        check(recorded && refused.convoys.empty(), "W10e and the loss is recorded as an embargo's, not silent");

        world home = f.w;
        home.convoys.push_back(shipped(f, peace, f.A, 4, 0.1f));
        advance_convoys(home);
        credit_arrived_convoys(home, 1);
        check(landed_at(home, f.A, f.ma) == 10.0f && home.convoys.empty(), "W10f with no embargo it lands whole on its source");
        const float treasury = home.nations.at(f.A).treasury;
        const auto flows = clear_markets(home, reg, economy_report{});
        const auto flow = flows.find(f.A);
        check(flow != flows.end() && flow->second.income > 0.0f,
              "W10g and its owner is paid for it at the clear, as a landing is");
        (void)treasury;
    }

    // -----------------------------------------------------------------------
    std::printf("\nW11 - the auto path reaches front closed\n");
    {
        recipe_registry treg = make_registry();
        trade_params tp = treg.trade();
        tp.capacity[k_good] = 10.0f;
        tp.marketplace_points = 10.0f;
        tp.port_points = 10.0f;
        treg.set_trade(tp);
        lanes f; build(f);
        for (int y = 0; y < CH; ++y)
            for (int x = 6; x <= 9; ++x)
                f.w.set_tile_nation(f.at(x, y), f.B);
        // A's marketplace at the source market; a price gap the trader would ship on.
        {
            const entity_id b = f.w.create_entity();
            building_component bc{};
            bc.tile = f.at(2, 3); bc.type = building_type::planetary_marketplace;
            bc.workforce_assigned = 1.0f; bc.workforce_target = 100.0f;
            f.w.buildings[b] = bc;
            f.w.nations.at(f.A).state_assets.push_back(b);
        }
        f.w.markets.at(f.mb).base_price[k_good] = 500.0f;
        f.w.markets.at(f.mb).price[k_good] = 500.0f;
        invalidate_logistics_caches(f.w);
        world peaceful = f.w;
        economy_report rep0;
        const trade_tick t0 = run_trades(peaceful, treg, rep0);
        check(t0.auto_shipments > 0, "W11a at peace the auto trade ships across the seam");

        open_war(f);
        economy_report rep;
        const trade_tick t1 = run_trades(f.w, treg, rep);
        check(t1.auto_shipments == 0, "W11b at war it cannot");
        check(t1.refused_front > 0, "W11c run_trades counts the auto route the front closed");
        bool lens = false;
        for (const trade_flow_pass& pass : f.w.player_trade_flow)
            for (const auto& [key, why] : pass.best)
                lens = lens || (key.first == f.mb && why == trade_refusal::front_closed);
        check(lens, "W11d and the Trade-flow lens reads front closed for it");

        // The reserved path, which reaches the door on its own.
        lanes g; build(g);
        for (int y = 0; y < CH; ++y)
            for (int x = 6; x <= 9; ++x)
                g.w.set_tile_nation(g.at(x, y), g.B);
        {
            const entity_id b = g.w.create_entity();
            building_component bc{};
            bc.tile = g.at(2, 3); bc.type = building_type::planetary_marketplace;
            bc.workforce_assigned = 1.0f; bc.workforce_target = 100.0f;
            g.w.buildings[b] = bc;
            g.w.nations.at(g.A).state_assets.push_back(b);
        }
        standing_trade st;
        st.id = 1; st.owner = g.A; st.from_market = g.ma; st.to_market = g.mb;
        st.resource = resource_type::iron_ore; st.points = 1.0f;
        g.w.trades.push_back(st);
        invalidate_logistics_caches(g.w);
        open_war(g);
        economy_report rep2;
        const trade_tick t2 = run_trades(g.w, treg, rep2);
        check(t2.refused_front > 0, "W11e a reserved trade the front closes is refused the same way");
    }

    // -----------------------------------------------------------------------
    std::printf("\nW12 - the next egress\n");
    {
        // A line runs both ways from A's station at (2,3): west over the seam to a station
        // in B at (14,3), nearest the destination; east to a station in neutral C at (8,3).
        lanes f; build(f);
        const auto station = [&](entity_id tile, entity_id owner) {
            const entity_id b = f.w.create_entity();
            building_component bc{};
            bc.tile = tile; bc.type = building_type::railway_station;
            f.w.buildings[b] = bc;
            f.w.nations.at(owner).state_assets.push_back(b);
        };
        station(f.at(2, 3), f.A);
        station(f.at(14, 3), f.B);
        station(f.at(8, 3), f.C);
        for (const int x : { 2, 1, 0, 15, 14, 3, 4, 5, 6, 7, 8 })
            f.w.rail_track.emplace(f.at(x, 3), f.A);
        recipe_registry rreg = make_registry();
        auto rp = rreg.rail();
        rp.cost_per_step = 0.001f;
        rp.station_handling = 0.0f;
        rreg.set_rail(rp);
        invalidate_logistics_caches(f.w);
        const logistics_nodes nodes = collect_logistics_nodes(f.w);
        const convoy_leg peace = price_trade_leg(f.w, rreg, nodes, f.A, f.ma, f.mb, k_good, 1.0f);
        check(peace.viable && peace.station_b == f.at(14, 3), "W12a at peace the train takes the nearest egress");
        open_war(f);
        const convoy_leg war = price_trade_leg(f.w, rreg, nodes, f.A, f.ma, f.mb, k_good, 1.0f);
        check(war.viable && war.station_b == f.at(8, 3),
              "W12b at war it tries the next one, on the neutral's ground, rather than giving up the line");
    }

    // -----------------------------------------------------------------------
    std::printf("\nW14 - the reader refuses what the verb refuses\n");
    {
        fixture f; build(f);
        reanchor_fronts(f.w);
        const entity_id d = add_unit(f, f.B, f.at(4, 5));
        check(order_of(f.w, reg, f.B, corp_verb::defend_province, d, null_entity, f.block(1))
                  == corp_command_result::applied, "W14a the fixture's order");
        { world ok; check(from_bytes(to_bytes(f.w), ok), "W14b the honest stream loads"); }
        const auto refused = [&](auto&& corrupt, const char* what) {
            world bad = f.w;
            corrupt(bad);
            world out;
            check(!from_bytes(to_bytes(bad), out), what);
        };
        refused([&](world& w) { w.units.at(d).type = static_cast<uint16_t>(naval_row()); },
                "W14c a naval row with a defend order is refused");
        refused([&](world& w) { w.units.at(d).front = nation_fronts(w, f.B).front().key; },
                "W14d a unit with both a front and a defend order is refused");
        refused([&](world& w) { w.set_tile_nation(f.at(4, 5), f.A); },
                "W14e a post the unit's nation does not own is refused");
        refused([&](world& w) { w.tiles.at(f.at(4, 5)).substrate = terrain_substrate::ocean; },
                "W14f a water post is refused");
        {
            fixture g; build(g);
            open_war(g, g.A, g.B, g.block(1));
            g.w.tile_occupier[g.at(4, 4)] = g.A;
            { world ok; check(from_bytes(to_bytes(g.w), ok), "W14g an honest occupation loads"); }
            g.w.tiles.at(g.at(4, 4)).substrate = terrain_substrate::ocean;
            world out;
            check(!from_bytes(to_bytes(g.w), out), "W14h an occupied water tile with no road is refused");
        }
        // A cession between two days leaves no stream the reader would refuse.
        {
            fixture g; build(g);
            open_war(g, g.A, g.B, g.block(1));
            const entity_id post = add_unit(g, g.B, g.at(4, 4));
            reanchor_fronts(g.w);
            (void)order_of(g.w, reg, g.B, corp_verb::defend_province, post, null_entity, g.block(1));
            // A holds the goal; B accepts its cession: through conclude_peace.
            for (std::size_t i = 0; i < g.w.provinces.provinces.size(); ++i)
                if (g.w.provinces.provinces[i].id == g.block(1))
                    g.w.province_holder[i] = g.A;
            const sentiment_params sp{};
            peace_terms t;
            t.kind = peace_kind::cession; t.gainer = g.A; t.provinces = { g.block(1) };
            const war_result off = offer_peace(g.w, g.A, g.B, t);
            const war_result acc = accept_peace(g.w, sp, g.B, g.A);
            check(off == war_result::applied && acc == war_result::applied, "W14i the province is ceded through the peace");
            check(g.w.units.at(post).defend_province == no_province, "W14k its defender's order ended with the ground");
            world out;
            check(from_bytes(to_bytes(g.w), out), "W14j a defended province ceded still loads");
        }
    }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    std::printf(failures == 0 ? "ALL PASS (0 failures)\n" : "FAILURES\n");
    return failures == 0 ? 0 : 1;
}
