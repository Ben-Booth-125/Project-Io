// ---------------------------------------------------------------------------
// Headless missile harness (BL-1431 missiles and satellites; no SDL / Lua / ImGui)
// docs/military/MILITARY.md § Missiles; AI_OPPONENT.md § 11, "Rival nations fire missiles".
// ---------------------------------------------------------------------------
// The fixture is a 60 x 10 plains cylinder cut into strips four columns wide, one
// province each. A holds columns 0-29, B columns 30-59, C (the third nation, the
// player in the rival rows) holds nothing. Each of A and B has a market with arms
// goods on its shelves and a factory beside it. Every refusal is checked by a
// WHOLE-WORLD compare (the save bytes and `state_hash`), never by the return code alone.
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/missiles.hpp"
#include "world/nation_tech.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/sentiment.hpp"
#include "world/sentiment_emitters.hpp"
#include "world/stance.hpp"
#include "world/unit_designs.hpp"
#include "world/unit_roster.hpp"
#include "world/war.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <tuple>
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

struct snap { std::string bytes; uint64_t hash = 0; };
snap take(const world& w) { return snap{ to_bytes(w), w.state_hash(0) }; }
bool same(const snap& a, const world& w) { return a.bytes == to_bytes(w) && a.hash == w.state_hash(0); }

constexpr int GW = 60, GH = 10, STRIP = 4;

/// scripts/economy.lua's rows for the factors these checks compare (the harness's own
/// copy, so a row reads the arithmetic; the Lua file is the calibration).
sentiment_params lua_params()
{
    sentiment_params p;
    auto set = [&p](sentiment_factor_kind k, float a, float t, float al) {
        p.factors[static_cast<std::size_t>(k)] = sentiment_factor{a, t, al};
    };
    set(sentiment_factor_kind::hostility_declared, -5.0f, -10.0f, 5.0f);
    set(sentiment_factor_kind::war_declared, 0.0f, -4.0f, 3.0f);
    set(sentiment_factor_kind::nuclear_used, 0.0f, -15.0f, 30.0f);
    set(sentiment_factor_kind::force_used, 0.0f, -0.004f, 0.002f);
    return p;
}

int first_row_of(land_class lc)
{
    const auto& t = unit_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        unit_component u{};
        u.type = static_cast<uint16_t>(i);
        if (unit_land_class(u) == lc)
            return static_cast<int>(i);
    }
    return -1;
}

struct fixture
{
    world     w;
    entity_id body = null_entity;
    entity_id a = null_entity, b = null_entity, c = null_entity;
    entity_id a_factory = null_entity, b_factory = null_entity;
    std::vector<entity_id> grid;
    std::vector<uint32_t>  strip_id;
    entity_id at(int x, int y) const { return grid[static_cast<std::size_t>(y * GW + x)]; }
    uint32_t  strip(int k) const { return strip_id[static_cast<std::size_t>(k)]; }
};

entity_id add_building(fixture& f, entity_id owner, int x, int y, building_type t)
{
    const entity_id id = f.w.create_entity();
    building_component b{};
    b.tile = f.at(x, y);
    b.type = t;
    b.ticks_remaining = 0;
    f.w.buildings[id] = b;
    f.w.nations.at(owner).state_assets.push_back(id);
    return id;
}

void add_market(fixture& f, int x, int y, float shelf)
{
    const entity_id m = f.w.create_entity();
    market_component mc{};
    mc.body = f.body;
    mc.centre_tile = f.at(x, y);
    for (const resource_type g : {resource_type::guided_weapons, resource_type::ordnance, resource_type::electronics,
                                  resource_type::refined_fuel, resource_type::machinery, resource_type::alloys,
                                  resource_type::steel})
    {
        const std::size_t i = static_cast<std::size_t>(g);
        mc.base_price[i] = 10.0f; mc.price[i] = 10.0f; mc.inventory[i] = shelf;
    }
    f.w.markets[m] = mc;
}

entity_id add_unit(fixture& f, entity_id owner, int x, int y, land_class lc, int men = 600)
{
    const int row = first_row_of(lc);
    const entity_id id = f.w.create_entity();
    unit_component u{};
    u.owner = owner; u.position = f.at(x, y); u.count = men; u.establishment = men;
    u.type = static_cast<uint16_t>(std::max(0, row));
    f.w.units[id] = u;
    return id;
}

void build(fixture& f, float shelf = 5000.0f)
{
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Field"; bc.type = body_type::planet; bc.grid_width = GW; bc.grid_height = GH;
    w.bodies[f.body] = bc;
    for (int y = 0; y < GH; ++y)
        for (int x = 0; x < GW; ++x)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body = f.body; tc.grid_x = x; tc.grid_y = y;
            tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass;
            tc.cover_density = 150; tc.landform = terrain_landform::plains; tc.habitability = 0.5f;
            w.tiles[t] = tc;
            f.grid.push_back(t);
        }
    w.provinces.provinces.clear();
    for (int k = 0; k < GW / STRIP; ++k)
    {
        province p;
        p.body = f.body;
        for (int y = 0; y < GH; ++y)
            for (int x = k * STRIP; x < (k + 1) * STRIP; ++x)
                p.tiles.push_back(f.at(x, y));
        std::sort(p.tiles.begin(), p.tiles.end());
        p.id = p.tiles.front();
        for (const entity_id t : p.tiles)
            w.provinces.tile_province[t] = p.id;
        f.strip_id.push_back(p.id);
        w.provinces.provinces.push_back(std::move(p));
    }
    const auto nation = [&](const char* name) {
        const entity_id n = w.create_entity();
        nation_component nc{};
        nc.name = name; nc.treasury = 1.0e6f;
        w.nations[n] = nc;
        return n;
    };
    f.a = nation("Aerie"); f.b = nation("Bastion"); f.c = nation("Cinder");
    for (int y = 0; y < GH; ++y)
        for (int x = 0; x < GW; ++x)
        {
            const entity_id n = x < 30 ? f.a : f.b;
            w.set_tile_nation(f.at(x, y), n);
            w.nations.at(n).tiles.push_back(f.at(x, y));
        }
    for (const entity_id n : {f.a, f.b})
        std::sort(w.nations.at(n).tiles.begin(), w.nations.at(n).tiles.end());
    seed_province_holders(w);
    w.player_entity = f.a;
    add_market(f, 5, 5, shelf);
    add_market(f, 54, 5, shelf);
    f.a_factory = add_building(f, f.a, 6, 5, building_type::processing_facility);
    f.b_factory = add_building(f, f.b, 53, 5, building_type::processing_facility);
}

corp_command stock_cmd(entity_id n, missile_kind k, int count)
{
    corp_command c;
    c.tick = 1; c.corp = n; c.verb = corp_verb::stock_missiles;
    c.order = static_cast<uint32_t>(k); c.unit_type = static_cast<uint16_t>(count);
    return c;
}

corp_command intercept_cmd(entity_id n, entity_id unit, int count)
{
    corp_command c;
    c.tick = 1; c.corp = n; c.verb = corp_verb::stock_interceptors; c.subject = unit;
    c.unit_type = static_cast<uint16_t>(count);
    return c;
}

corp_command fire_cmd(entity_id n, entity_id launcher, uint32_t kind, int salvo, uint32_t province,
                      entity_id building = null_entity)
{
    corp_command c;
    c.tick = 1; c.corp = n; c.verb = corp_verb::fire_missile; c.subject = launcher;
    c.order = kind; c.unit_type = static_cast<uint16_t>(salvo); c.province = province; c.counterparty = building;
    return c;
}

corp_command_result run(fixture& f, const recipe_registry& reg, const corp_command& c)
{
    return apply_corp_command(f.w, reg, c);
}

float shelf_of(const world& w, resource_type r)
{
    float s = 0.0f;
    for (const auto& [mid, m] : w.markets)
        s += m.inventory[static_cast<std::size_t>(r)];
    return s;
}

template <typename F>
void refused(fixture& f, F press, corp_command_result want, const std::string& what)
{
    const snap before = take(f.w);
    const corp_command_result r = press();
    check(r == want && same(before, f.w), what);
}

void hold_node(world& w, entity_id n, int node)
{
    w.nation_tech[n].held.set(static_cast<std::size_t>(node));
}

/// A's artillery at column 29 (strip 7, within sight of strip 8), B's infantry and air defence at column 33 (strip 8),
/// A hostile toward B, A's magazine stocked.
struct scene
{
    entity_id art = null_entity, b_inf = null_entity, b_ad = null_entity;
};

scene stage(fixture& f, const recipe_registry& reg)
{
    scene s;
    s.art   = add_unit(f, f.a, 29, 5, land_class::artillery);
    s.b_inf = add_unit(f, f.b, 33, 5, land_class::infantry);
    s.b_ad  = add_unit(f, f.b, 33, 6, land_class::air_defence);
    f.w.corp_hostile_pairs.insert({f.a, f.b});
    (void)reg;
    return s;
}

} // namespace

static_assert(static_cast<int>(missile_kind::strategic_nuclear) == 3 && missile_fireable_count == 3,
              "a strategic weapon is a name past the magazine's three columns");
static_assert(std::tuple_size<decltype(world{}.missile_magazines)::mapped_type>::value == 3,
              "the magazine holds no strategic column");

int main()
{
    std::printf("=== missiles (BL-1431) ===\n");
    recipe_registry reg;
    reg.set_sentiment(lua_params());
    const missile_params& P = reg.military().missile;

    check(first_row_of(land_class::artillery) >= 0 && first_row_of(land_class::air_defence) >= 0
              && first_row_of(land_class::infantry) >= 0,
          "setup: the roster has artillery, air defence and infantry rows");

    // =====================================================================
    std::printf("\nM1 - stock_missiles buys the goods into the magazine; refusals mutate nothing\n");
    {
        fixture f; build(f);
        const float gw0 = shelf_of(f.w, resource_type::guided_weapons);
        check(run(f, reg, stock_cmd(f.a, missile_kind::short_range, 3)) == corp_command_result::applied
                  && missile_stock(f.w, f.a, missile_kind::short_range) == 3,
              "M1a three short-range missiles stocked");
        check(shelf_of(f.w, resource_type::guided_weapons) < gw0 - 2.5f,
              "M1b their guided weapons left the shelves (1 a missile)");
        refused(f, [&] { return run(f, reg, stock_cmd(f.a, missile_kind::strategic_nuclear, 1)); },
                corp_command_result::rejected_invalid, "M1c a strategic weapon is never stocked; nothing moves");
        refused(f, [&] { return run(f, reg, stock_cmd(f.a, missile_kind::tactical_nuclear, 1)); },
                corp_command_result::rejected_tech_locked,
                "M1d a tactical warhead needs Uranium Enrichment held; nothing moves");
        refused(f, [&] { return run(f, reg, stock_cmd(f.a, missile_kind::short_range, 0)); },
                corp_command_result::rejected_invalid, "M1e a count of 0 is refused; nothing moves");
        refused(f, [&] { return run(f, reg, stock_cmd(f.a, missile_kind::short_range, P.stock_per_verb + 1)); },
                corp_command_result::rejected_invalid, "M1f a count past the verb's bound is refused");
        hold_node(f.w, f.a, missile_nuclear_node());
        check(run(f, reg, stock_cmd(f.a, missile_kind::tactical_nuclear, 1)) == corp_command_result::applied,
              "M1g with the node held, a tactical warhead is stocked");
        fixture g; build(g, 0.0f);
        refused(g, [&] { return run(g, reg, stock_cmd(g.a, missile_kind::short_range, 1)); },
                corp_command_result::rejected_funds, "M1h empty shelves refuse the buy; nothing moves");
    }

    // =====================================================================
    std::printf("\nM2 - firing consumes a missile and lands on a hostile unit\n");
    {
        fixture f; build(f);
        const scene s = stage(f, reg);
        run(f, reg, stock_cmd(f.a, missile_kind::short_range, 4));
        const int men0 = f.w.units.at(s.b_inf).count + f.w.units.at(s.b_ad).count;
        const corp_command_result r = run(f, reg, fire_cmd(f.a, s.art, 0, 1, f.strip(8)));
        const int men1 = f.w.units.at(s.b_inf).count + f.w.units.at(s.b_ad).count;
        check(r == corp_command_result::applied && missile_stock(f.w, f.a, missile_kind::short_range) == 3,
              "M2a the strike applies and the magazine falls 4 -> 3");
        check(men1 < men0, "M2b B's men in the province fall (" + std::to_string(men0) + " -> "
                               + std::to_string(men1) + ")");
        // A building strike lowers condition.
        const entity_id fac = add_building(f, f.b, 34, 4, building_type::processing_facility);
        const corp_command_result rb = run(f, reg, fire_cmd(f.a, s.art, 0, 1, f.strip(8), fac));
        const int32_t cond = f.w.buildings.at(fac).condition_permille;
        check(rb == corp_command_result::applied && cond == 850,
              "M2c a strike on a hostile factory lowers its condition 1000 -> " + std::to_string(cond)
                  + " (150 a short-range missile landed)");
    }

    // =====================================================================
    std::printf("\nM3 - out of range, not hostile, unsighted, short stock, a strategic kind: refused, nothing moves\n");
    {
        fixture f; build(f);
        const scene s = stage(f, reg);
        run(f, reg, stock_cmd(f.a, missile_kind::short_range, 2));
        // Out of range: B's strip 14 (columns 56-59) is 29+ tiles from column 27; short reaches 12.
        add_unit(f, f.b, 57, 5, land_class::infantry);
        const entity_id far_scout = add_unit(f, f.a, 56, 2, land_class::infantry, 10); // sight there
        (void)far_scout;
        refused(f, [&] { return run(f, reg, fire_cmd(f.a, s.art, 0, 1, f.strip(14))); },
                corp_command_result::rejected_placement, "M3a a province past the reach is refused");
        // Not hostile: B never declared on A, so B firing at A is refused.
        const entity_id b_art = add_unit(f, f.b, 32, 5, land_class::artillery);
        run(f, reg, stock_cmd(f.b, missile_kind::short_range, 2));
        refused(f, [&] { return run(f, reg, fire_cmd(f.b, b_art, 0, 1, f.strip(6))); },
                corp_command_result::rejected_state, "M3b a target of a nation the firer has not declared hostile is refused");
        // Unsighted: B's strip 11 (columns 44-47), in long reach of nothing A sees.
        add_unit(f, f.b, 45, 5, land_class::infantry);
        f.w.missile_magazines[f.a][1] = 2;
        // The artillery cannot carry long range; the reach of short (12) from col 29 reaches col 41,
        // so strip 10 (40-43) is in reach and unsighted.
        add_unit(f, f.b, 41, 5, land_class::infantry);
        refused(f, [&] { return run(f, reg, fire_cmd(f.a, s.art, 0, 1, f.strip(10))); },
                corp_command_result::rejected_state, "M3c an unsighted province in reach is refused");
        refused(f, [&] { return run(f, reg, fire_cmd(f.a, s.art, 0, 3, f.strip(8))); },
                corp_command_result::rejected_funds, "M3d a salvo past the magazine is refused");
        refused(f, [&] { return run(f, reg, fire_cmd(f.a, s.art, 3, 1, f.strip(8))); },
                corp_command_result::rejected_invalid, "M3e a strategic weapon is refused");
        refused(f, [&] { return run(f, reg, fire_cmd(f.a, s.art, 1, 1, f.strip(8))); },
                corp_command_result::rejected_invalid, "M3f artillery does not carry long range");
        refused(f, [&] { return run(f, reg, fire_cmd(f.a, s.b_inf, 0, 1, f.strip(8))); },
                corp_command_result::rejected_not_owner, "M3g another nation's launcher is refused");
        refused(f, [&] { return run(f, reg, fire_cmd(f.a, s.art, 0, 0, f.strip(8))); },
                corp_command_result::rejected_invalid, "M3h a salvo of 0 is refused");
        refused(f, [&] { return run(f, reg, fire_cmd(f.a, s.art, 0, 1, no_province)); },
                corp_command_result::rejected_invalid, "M3i no province is refused");
    }

    // =====================================================================
    std::printf("\nM4 - interception thins short-range far more than long-range\n");
    {
        int64_t landed[2] = {0, 0};
        int used[2] = {0, 0};
        for (int ki = 0; ki < 2; ++ki)
        {
            fixture f; build(f);
            const scene s = stage(f, reg);
            const entity_id cap = f.w.create_entity(); // a capital ship carries both kinds
            sea_ship sh;
            sh.owner = f.a; sh.tile = f.at(26, 3); sh.type = 2; // ship_roster_table: 2 capital
            f.w.ships.emplace(cap, sh);
            check(run(f, reg, intercept_cmd(f.b, s.b_ad, 8)) == corp_command_result::applied,
                  "M4  B stocks 8 interceptors at its air defence unit");
            f.w.missile_magazines[f.a][static_cast<std::size_t>(ki)] = 4;
            missile_strike_plan plan;
            const corp_command_result r =
                fire_missile_check(f.w, reg, f.a, cap, static_cast<uint32_t>(ki), 4, f.strip(8), null_entity, &plan);
            check(r == corp_command_result::applied, std::string("M4  a capital ship may fire ") + (ki ? "long" : "short"));
            landed[ki] = plan.landed_milli;
            used[ki] = plan.interceptors;
            check(run(f, reg, fire_cmd(f.a, cap, static_cast<uint32_t>(ki), 4, f.strip(8))) == corp_command_result::applied
                      && f.w.interceptor_stocks.count(s.b_ad) == 0,
                  "M4  the salvo spends all eight interceptors (two a missile)");
        }
        std::printf("      landed of 4000 milli-missiles: short %lld, long %lld (interceptors %d, %d)\n",
                    static_cast<long long>(landed[0]), static_cast<long long>(landed[1]), used[0], used[1]);
        check(used[0] == 8 && used[1] == 8 && landed[0] * 4 < landed[1],
              "M4a against the same eight interceptors, under a quarter as much short range lands as long range");
        check(landed[0] < 1000 && landed[1] > 2000,
              "M4b short range is mostly stopped (<1 of 4 lands); long range mostly gets through (>2 of 4)");
        // No interceptors: everything lands.
        fixture f; build(f);
        const scene s = stage(f, reg);
        f.w.missile_magazines[f.a][0] = 2;
        missile_strike_plan plan;
        fire_missile_check(f.w, reg, f.a, s.art, 0, 2, f.strip(8), null_entity, &plan);
        check(plan.landed_milli == 2000 && plan.interceptors == 0, "M4c with no interceptor every missile lands");
        // Interceptors cost more than the missile they stop.
        const float m_cost = kit_goods_price_bound(f.w, f.a, missile_goods(reg, missile_kind::short_range, 1));
        const float i_cost = kit_goods_price_bound(f.w, f.a, missile_goods(reg, missile_kind::short_range, 1, true));
        check(i_cost > m_cost, "M4d one interceptor costs more than one short-range missile at the same prices");
    }

    // =====================================================================
    std::printf("\nM5 - a tactical warhead raises Alarm above a declaration of war\n");
    {
        fixture f; build(f);
        const scene s = stage(f, reg);
        hold_node(f.w, f.a, missile_nuclear_node());
        f.w.missile_magazines[f.a][2] = 1;
        const war_price dp = declaration_price(f.w, reg.sentiment(), f.a, f.b, { f.strip(8) });
        float decl_max = 0.0f;
        for (const war_price_row& r : dp.rows) decl_max = std::max(decl_max, r.alarm);
        const float nuke_sum = nuclear_alarm_sum(f.w, reg.sentiment(), f.a, f.b, 1, P.nuclear_struck_magnitude);
        check(dp.valid && nuke_sum > dp.sum_alarm,
              "M5a one warhead's Alarm summed over observers (" + std::to_string(nuke_sum)
                  + ") exceeds the declaration's (" + std::to_string(dp.sum_alarm) + ")");
        // The worst any single declaration row can reach: war_declared x (1 + 0.5 x 7 goals) x border 2.
        const float worst_row = 3.0f * (1.0f + k_war_per_extra_goal * 7.0f) * k_war_border_multiplier;
        check(30.0f > worst_row && 30.0f > decl_max,
              "M5b one warhead per observer (+30) exceeds the worst declaration row (" + std::to_string(worst_row) + ")");
        const int men0 = f.w.units.at(s.b_inf).count;
        check(run(f, reg, fire_cmd(f.a, s.art, 2, 1, f.strip(8))) == corp_command_result::applied,
              "M5c the warhead is fired");
        fold_pending_sentiment(f.w, reg);
        const float alarm_b = sentiment_toward(f.w.sentiment, f.b, f.a).alarm;
        const float alarm_c = sentiment_toward(f.w.sentiment, f.c, f.a).alarm;
        std::printf("      Alarm toward A: struck B %.2f, bystander C %.2f; B's infantry %d -> %d\n",
                    alarm_b, alarm_c, men0, f.w.units.at(s.b_inf).count);
        check(alarm_c > worst_row && alarm_b > alarm_c,
              "M5d after the fold a bystander's Alarm toward the firer passes the worst declaration row, the struck nation's more");
        check(f.w.units.at(s.b_inf).count < men0 / 2,
              "M5e the area strike kills more than half of every hostile unit in the province");
    }

    // =====================================================================
    std::printf("\nM6 - no verb fires a strategic weapon\n");
    {
        fixture f; build(f);
        const scene s = stage(f, reg);
        f.w.missile_magazines[f.a] = {8, 8, 8};
        hold_node(f.w, f.a, missile_nuclear_node());
        bool none = true;
        for (uint32_t kind = 3; kind < 256; ++kind)
        {
            const snap before = take(f.w);
            none = none && run(f, reg, fire_cmd(f.a, s.art, kind, 1, f.strip(8))) == corp_command_result::rejected_invalid
                   && same(before, f.w);
            none = none && run(f, reg, stock_cmd(f.a, static_cast<missile_kind>(kind & 0xFF), 1))
                               == corp_command_result::rejected_invalid
                   && same(before, f.w);
        }
        check(none, "M6a fire_missile and stock_missiles refuse every kind from 3 (strategic) up, mutating nothing");
        bool verb_scan = true;
        for (uint8_t v = 0; v < corp_verb_count; ++v)
        {
            corp_command c = fire_cmd(f.a, s.art, 3, 1, f.strip(8));
            c.verb = static_cast<corp_verb>(v);
            world copy = f.w;
            (void)apply_corp_command(copy, reg, c);
            int32_t total = 0;
            for (const auto& [n, m] : copy.missile_magazines)
                total += m[0] + m[1] + m[2];
            // No verb named with the strategic kind fires anything (the magazine is untouched).
            verb_scan = verb_scan && total == 24;
        }
        check(verb_scan, "M6b no verb in the dictionary, carrying kind 3, draws a missile");
    }

    // =====================================================================
    std::printf("\nM7 - a rival fires only at a sighted target of a nation it is hostile to; never for the player\n");
    {
        // B is the rival here; C is the seat.
        const auto rival_scene = [&](fixture& f, bool hostile, entity_id* b_art, entity_id* a_inf, entity_id* c_inf) {
            build(f);
            f.w.player_entity = f.c;
            *b_art = add_unit(f, f.b, 32, 5, land_class::artillery);
            *a_inf = add_unit(f, f.a, 28, 5, land_class::infantry);
            *c_inf = add_unit(f, f.c, 34, 2, land_class::infantry); // a neutral's unit beside it
            f.w.missile_magazines[f.b] = {4, 0, 0};
            if (hostile)
                f.w.corp_hostile_pairs.insert({f.b, f.a});
            else
                f.w.corp_hostile_pairs.insert({f.c, f.a}); // mobilised elsewhere, not toward A by B
        };
        {
            fixture f; entity_id b_art, a_inf, c_inf;
            rival_scene(f, false, &b_art, &a_inf, &c_inf);
            f.w.corp_hostile_pairs.insert({f.b, f.c}); // B hostile to C only... C's unit is on B's ground
            const int a0 = f.w.units.at(a_inf).count;
            score_nation_missiles(f.w, reg, f.b, 20);
            check(f.w.units.at(a_inf).count == a0,
                  "M7a B, hostile only to C, never strikes A's units");
        }
        {
            fixture f; entity_id b_art, a_inf, c_inf;
            rival_scene(f, true, &b_art, &a_inf, &c_inf);
            const int a0 = f.w.units.at(a_inf).count, c0 = f.w.units.at(c_inf).count;
            score_nation_missiles(f.w, reg, f.b, 20);
            check(f.w.units.at(a_inf).count < a0 && f.w.units.at(c_inf).count == c0
                      && missile_stock(f.w, f.b, missile_kind::short_range) < 4,
                  "M7b B, hostile to A, fires its own missiles at A's sighted unit and at nothing of C's");
        }
        {
            fixture f; entity_id b_art, a_inf, c_inf;
            rival_scene(f, true, &b_art, &a_inf, &c_inf);
            f.w.player_entity = f.b; // now B is the seat
            const snap before = take(f.w);
            score_nation_missiles(f.w, reg, f.b, 20);
            check(same(before, f.w), "M7c the scorer never acts for the player's nation");
        }
        {
            fixture f; entity_id b_art, a_inf, c_inf;
            rival_scene(f, true, &b_art, &a_inf, &c_inf);
            // Move A's unit out of everything B can see: deep in A's west, beyond sight.
            f.w.units.at(a_inf).position = f.at(25, 5); // strip 6, 7 tiles from B's launcher: sighted? no units of B within 4
            f.w.units.at(b_art).position = f.at(36, 5);
            const int a0 = f.w.units.at(a_inf).count;
            score_nation_missiles(f.w, reg, f.b, 20);
            check(f.w.units.at(a_inf).count == a0, "M7d an unsighted target is never struck by a rival");
        }
        {
            fixture f; entity_id b_art, a_inf, c_inf;
            rival_scene(f, true, &b_art, &a_inf, &c_inf);
            hold_node(f.w, f.b, missile_nuclear_node());
            f.w.missile_magazines[f.b] = {0, 0, 1};
            // Nothing on the shelves, so it cannot stock a cheaper missile to fire instead.
            for (auto& [mid, m] : f.w.markets)
                m.inventory.fill(0.0f);
            const int a0 = f.w.units.at(a_inf).count;
            score_nation_missiles(f.w, reg, f.b, 20);
            check(f.w.units.at(a_inf).count == a0 && missile_stock(f.w, f.b, missile_kind::tactical_nuclear) == 1,
                  "M7e against one 600-man unit the warhead's Alarm price outweighs the strike: the rival holds it");
            // A large stack: the same warhead is now worth its price.
            for (int i = 0; i < 12; ++i)
                add_unit(f, f.a, 28 + (i % 2), 1 + (i % 8), land_class::infantry, 1500);
            score_nation_missiles(f.w, reg, f.b, 21);
            check(missile_stock(f.w, f.b, missile_kind::tactical_nuclear) == 0,
                  "M7f against a stack of 18,600 men in the province the rival fires the warhead");
        }
    }

    // =====================================================================
    std::printf("\nM8 - determinism: the same scene twice is byte-identical\n");
    {
        std::string out[2];
        uint64_t h[2] = {0, 0};
        for (int i = 0; i < 2; ++i)
        {
            fixture f; build(f);
            const scene s = stage(f, reg);
            hold_node(f.w, f.a, missile_nuclear_node());
            run(f, reg, stock_cmd(f.a, missile_kind::short_range, 5));
            run(f, reg, stock_cmd(f.a, missile_kind::tactical_nuclear, 1));
            run(f, reg, intercept_cmd(f.b, s.b_ad, 3));
            run(f, reg, fire_cmd(f.a, s.art, 0, 2, f.strip(8)));
            run(f, reg, fire_cmd(f.a, s.art, 2, 1, f.strip(8)));
            fold_pending_sentiment(f.w, reg);
            out[i] = to_bytes(f.w);
            h[i] = f.w.state_hash(0);
        }
        check(out[0] == out[1] && h[0] == h[1], "M8a two runs end byte-identical, state hash equal");
    }

    // =====================================================================
    std::printf("\nM9 - the v89 missile section round-trips; v88 reads as nothing stocked; bad streams refused\n");
    {
        fixture f; build(f);
        const scene s = stage(f, reg);
        f.w.missile_magazines[f.a] = {5, 2, 1};
        f.w.interceptor_stocks[s.b_ad] = 7;
        const std::string bytes = to_bytes(f.w);
        world loaded;
        check(from_bytes(bytes, loaded) && loaded.missile_magazines == f.w.missile_magazines
                  && loaded.interceptor_stocks == f.w.interceptor_stocks && to_bytes(loaded) == bytes
                  && loaded.state_hash(0) == f.w.state_hash(0),
              "M9a the magazine and the interceptors survive save and load, bytes and hash equal");
        std::ostringstream o(std::ios::binary);
        write_world_snapshot_pre_missiles(f.w, o);
        world old;
        check(o.str().size() + world_save_missile_section_bytes(f.w) == bytes.size()
                  && from_bytes(o.str(), old) && old.missile_magazines.empty() && old.interceptor_stocks.empty(),
              "M9b a v88 stream is the v89 stream less the section, and reads with nothing stocked");
        // An interceptor stock on a non-air-defence unit is refused whole. The writer filters
        // one, so poke the bytes: the good stream with the last record's unit id swapped.
        std::string poked = bytes;
        const std::size_t at = poked.size() - 8; // the last interceptor record: id, count
        const entity_id inf = s.b_inf;
        std::memcpy(&poked[at], &inf, 4);
        world refused_w;
        check(!from_bytes(poked, refused_w), "M9c an interceptor stock on an infantry unit is refused");
        std::string over = bytes;
        const int32_t too_many = k_interceptor_cap + 1;
        std::memcpy(&over[over.size() - 4], &too_many, 4);
        world refused_o;
        check(!from_bytes(over, refused_o), "M9d an interceptor count past the cap is refused");
        // A magazine of all zeros is refused (the writer never writes one).
        std::string zero = bytes;
        // The magazine record sits before the interceptor count (4) and its one record (8).
        const std::size_t mag_at = zero.size() - 8 - 4 - 12; // A's three counts
        const int32_t z = 0;
        for (int i = 0; i < 3; ++i)
            std::memcpy(&zero[mag_at + 4 * static_cast<std::size_t>(i)], &z, 4);
        world refused_z;
        check(!from_bytes(zero, refused_z), "M9e an all-zero magazine is refused");
    }

    std::printf("\nRESULT: %d failures / %d checks\n", failures, checks);
    return failures == 0 ? 0 : 1;
}
