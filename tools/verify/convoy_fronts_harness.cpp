// ---------------------------------------------------------------------------
// convoy_fronts_harness — BL-1505 (SUPPLY.md § A lane does not cross a warring
// front, Ben 2026-11-05). No SDL/Lua.
// ---------------------------------------------------------------------------
// A 16 x 6 cylinder of plains: A owns columns 0-5, C columns 6-9, B columns 10-15.
// Columns wrap, so A and B also touch at the seam (15 next to 0), and a market in A
// and a market in B are joined most cheaply across that seam.
//
//   C1  AT PEACE the lane crosses the seam; the leg prices that lane.
//   C2  AT WAR the party's lane goes round through C: a dearer leg, no hop over the
//       front, and the price is the lane's own hop costs (price and lane are one).
//   C3  BOTH PARTIES are bound; a NEUTRAL's convoy and a market's own leg are not.
//   C4  NO ROUTE: with C's corridor gone to B, the leg is refused and the reason
//       (`front_closes_lane`) is the front's, not a missing road.
//   C5  IN FLIGHT: stranded cargo lands on its source; a detour, a neutral and an
//       arrived convoy are left alone. Quantity conserved.
//   C6  THE FRONT MOVES: occupation moves what is closed; peace reopens the old lane.
//   C7  DETERMINISM: a save and load prices and routes to the same bits.
//
// Run: node tools/verify/build_harness.js convoy_fronts_harness --run
// ---------------------------------------------------------------------------

#include "harness_params.hpp"
#include "world/components.hpp"
#include "world/fronts.hpp"
#include "world/logistics.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/supply_system.hpp"
#include "world/unit_roster.hpp"
#include "world/war.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

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

constexpr int W = 16, H = 6;

struct fixture
{
    world     w;
    entity_id body = null_entity, A = null_entity, B = null_entity, C = null_entity;
    entity_id ma = null_entity, mb = null_entity;
    std::vector<entity_id> cell;
    entity_id at(int x, int y) const { return cell[static_cast<std::size_t>(y * W + x)]; }
};

constexpr std::size_t k_good = static_cast<std::size_t>(resource_type::iron_ore);

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
            w.set_tile_nation(f.at(x, y), x <= 5 ? f.A : x <= 9 ? f.C : f.B);
    const auto market = [&](entity_id centre) {
        const entity_id m = w.create_entity();
        market_component mc{};
        mc.body = f.body;
        mc.centre_tile = centre;
        mc.base_price[k_good] = 2.0f;
        mc.price = mc.base_price;
        mc.inventory[k_good] = 100.0f;
        w.markets[m] = mc;
        return m;
    };
    f.ma = market(f.at(2, 3));
    f.mb = market(f.at(13, 3));
}

void open_war(fixture& f)
{
    war_record rec;
    rec.declarer = f.A; rec.defender = f.B;
    rec.declarer_goals = { 1u };
    f.w.wars[{ f.A, f.B }] = rec;
    (void)declare_hostile(f.w, f.A, f.B);
}

convoy_component convoy_of(const fixture& f, entity_id owner, std::uint32_t id, float progress = 0.5f)
{
    convoy_component c;
    c.id             = id;
    c.source_market  = f.ma;
    c.dest_market    = f.mb;
    c.mode           = convoy_mode::land;
    c.cargo_resource = resource_type::iron_ore;
    c.cargo_qty      = 10.0f;
    c.progress       = progress;
    c.speed          = 0.1f;
    c.corp           = owner;
    c.origin_tile    = f.at(2, 3);
    return c;
}

bool crosses_seam(const std::vector<entity_id>& tiles, const fixture& f)
{
    for (std::size_t i = 1; i < tiles.size(); ++i)
    {
        const int x0 = f.w.tiles.at(tiles[i - 1]).grid_x, x1 = f.w.tiles.at(tiles[i]).grid_x;
        if ((x0 == 15 && x1 == 0) || (x0 == 0 && x1 == 15))
            return true;
    }
    return false;
}

float lane_hops(world& w, const std::vector<entity_id>& tiles)
{
    float sum = 0.0f;
    for (std::size_t i = 1; i < tiles.size(); ++i)
        sum += edge_traversal_cost(w.tiles.at(tiles[i - 1]), w.tiles.at(tiles[i]),
                                   road_side_between(w, tiles[i - 1], tiles[i]));
    return sum;
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
    std::printf("=== convoy fronts (BL-1505) ===\n");
    const recipe_registry reg;
    const float rate = reg.logistics_cost(convoy_mode::land);

    std::printf("\nC1 - at peace\n");
    fixture f; build(f);
    const logistics_nodes nodes = collect_logistics_nodes(f.w);
    const convoy_leg peace = price_trade_leg(f.w, reg, nodes, f.A, f.ma, f.mb, k_good, 1.0f);
    const convoy_route peace_lane = convoy_route_tiles(f.w, convoy_of(f, f.A, 1));
    check(peace.viable && crosses_seam(peace_lane.tiles, f), "C1a the lane crosses the seam, the cheapest way");
    check(lane_party(f.w, f.A) == null_entity, "C1b at peace nobody is a party");

    std::printf("\nC2 - at war the lane goes round\n");
    open_war(f);
    const convoy_leg war = price_trade_leg(f.w, reg, nodes, f.A, f.ma, f.mb, k_good, 1.0f);
    const convoy_route war_lane = convoy_route_tiles(f.w, convoy_of(f, f.A, 2));
    check(war.viable && war.cost > peace.cost, "C2a the leg still runs, and costs more");
    check(!war_lane.tiles.empty() && !lane_crosses_warring_front(f.w, f.A, war_lane.tiles)
              && !crosses_seam(war_lane.tiles, f),
          "C2b the lane crosses no hop of the warring front");
    bool through_c = false;
    for (const entity_id t : war_lane.tiles)
        through_c = through_c || f.w.tile_to_nation.at(t) == f.C;
    check(through_c, "C2c it goes through the neutral's ground");
    check(std::fabs(war.cost - rate * lane_hops(f.w, war_lane.tiles)) < 1e-3f * war.cost,
          "C2d the price is the lane's own hop costs: price and lane are one answer");
    check(war_lane.tiles.front() == f.at(2, 3) && war_lane.tiles.back() == f.at(13, 3),
          "C2e the lane runs source to destination");

    std::printf("\nC3 - who is bound\n");
    const convoy_leg reverse = price_trade_leg(f.w, reg, nodes, f.B, f.mb, f.ma, k_good, 1.0f);
    const convoy_route rev_lane = [&] {
        convoy_component c = convoy_of(f, f.B, 3);
        c.source_market = f.mb; c.dest_market = f.ma; c.origin_tile = f.at(13, 3);
        return convoy_route_tiles(f.w, c);
    }();
    check(reverse.viable && !crosses_seam(rev_lane.tiles, f), "C3a the defender is bound too, unanswered as it is");
    const convoy_leg neutral = price_trade_leg(f.w, reg, nodes, f.C, f.ma, f.mb, k_good, 1.0f);
    check(neutral.viable && neutral.cost == peace.cost && crosses_seam(convoy_route_tiles(f.w, convoy_of(f, f.C, 4)).tiles, f),
          "C3b a neutral's convoy flies the seam at the peace price, to the bit");
    check(price_market_leg(f.w, reg, nodes, f.ma, f.mb, 1.0f).viable
              && price_market_leg(f.w, reg, nodes, f.ma, f.mb, 1.0f).cost == peace.cost,
          "C3c a market's own leg names no party, and no front binds it");
    check(!front_closes_lane(f.w, reg, nodes, f.A, f.ma, f.mb), "C3d a lane that detours is not refused");

    std::printf("\nC5 - in flight (a detour left alone)\n");
    f.w.convoys.clear();
    f.w.convoys.push_back(convoy_of(f, f.A, 10));
    f.w.convoys.push_back(convoy_of(f, f.C, 11));
    advance_convoys(f.w);
    check(f.w.convoys.size() == 2 && !convoy_stranded_by_front(f.w, f.w.convoys[0]),
          "C5a with a detour open, nothing returns: the cargo flies the detour");

    std::printf("\nC4 - no route left\n");
    fixture g; build(g);
    open_war(g);
    for (int y = 0; y < H; ++y)
        for (int x = 6; x <= 9; ++x)
            g.w.set_tile_nation(g.at(x, y), g.B);
    invalidate_logistics_caches(g.w);
    const convoy_leg none = price_trade_leg(g.w, reg, nodes, g.A, g.ma, g.mb, k_good, 1.0f);
    check(!none.viable, "C4a A meets B on both sides now: the leg is refused");
    check(front_closes_lane(g.w, reg, nodes, g.A, g.ma, g.mb), "C4b and the reason is the front");
    check(!front_closes_lane(g.w, reg, nodes, g.C, g.ma, g.mb), "C4c a neutral has no such reason");
    check(convoy_route_tiles(g.w, convoy_of(g, g.A, 20)).tiles.empty(), "C4d the party's convoy has no lane");
    check(price_trade_leg(g.w, reg, nodes, g.C, g.ma, g.mb, k_good, 1.0f).viable, "C4e a neutral still trades across");

    std::printf("\nC5 - in flight (no route left)\n");
    {
        g.w.convoys.clear();
        g.w.convoys.push_back(convoy_of(g, g.A, 30, 0.2f)); // on the near side of the front
        g.w.convoys.push_back(convoy_of(g, g.C, 31, 0.2f));
        convoy_component arrived = convoy_of(g, g.A, 32, 1.0f);
        arrived.arrived = true;
        g.w.convoys.push_back(arrived);
        check(convoy_stranded_by_front(g.w, g.w.convoys[0]) && !convoy_stranded_by_front(g.w, g.w.convoys[1]),
              "C5b the party's cargo is stranded by the front; the neutral's is not");
        advance_convoys(g.w);
        credit_arrived_convoys(g.w, 1);
        bool thirty = false, thirty_one = false;
        for (const convoy_component& c : g.w.convoys)
        {
            thirty     = thirty || c.id == 30;
            thirty_one = thirty_one || c.id == 31;
        }
        check(!thirty && thirty_one, "C5c the stranded convoy is gone; the neutral's flies on");
        const auto landed = g.w.landed_this_tick.find({ g.A, g.ma });
        check(landed != g.w.landed_this_tick.end() && landed->second.quantities[k_good] == 10.0f,
              "C5d its cargo landed whole on its source market");
    }

    std::printf("\nC6 - the front moves\n");
    {
        fixture h; build(h);
        open_war(h);
        const logistics_nodes hn = collect_logistics_nodes(h.w);
        // B takes the whole seam column of A's: the front moves one column into A. A's market
        // is unmoved, and the lane still may not cross it; its detour is the same one.
        const float before = price_trade_leg(h.w, reg, hn, h.A, h.ma, h.mb, k_good, 1.0f).cost;
        // A's corridor east closes when B occupies column 5 (between A and C) in every row.
        bool batch = false;
        const uint16_t rifle = 0;
        std::size_t row = 0;
        for (const auto& r : unit_roster_table())
        {
            if (std::string(r.name) == "Rifle Infantry") break;
            ++row;
        }
        (void)rifle;
        for (int y = 0; y < H; ++y)
            (void)occupy_on_entry(h.w, h.B, static_cast<uint16_t>(row), h.at(5, y), batch);
        reanchor_fronts(h.w);
        check(h.w.tile_controller(h.at(5, 3)) == h.B && h.w.tile_to_nation.at(h.at(5, 3)) == h.A,
              "C6a occupied, the column reads B's front and still A's ground");
        const convoy_leg cut = price_trade_leg(h.w, reg, hn, h.A, h.ma, h.mb, k_good, 1.0f);
        check(!cut.viable, "C6b the occupation moved the front: A's road round through C is closed");
        // The price cache is not stale the other way: peace reopens it, at the peace price.
        h.w.wars.clear();
        clear_occupation_at_peace(h.w);
        const convoy_leg again = price_trade_leg(h.w, reg, hn, h.A, h.ma, h.mb, k_good, 1.0f);
        check(again.viable && again.cost < before && crosses_seam(convoy_route_tiles(h.w, convoy_of(h, h.A, 40)).tiles, h),
              "C6c peace reopens the seam, at the peace price: nothing cached outlives the war");
    }

    std::printf("\nC7 - determinism\n");
    {
        fixture h; build(h);
        h.w.markets.at(h.ma).inventory[k_good] = 100.0f;
        open_war(h);
        h.w.convoys.push_back(convoy_of(h, h.A, 50));
        const logistics_nodes hn = collect_logistics_nodes(h.w);
        const convoy_leg first = price_trade_leg(h.w, reg, hn, h.A, h.ma, h.mb, k_good, 1.0f);
        const std::vector<entity_id> lane = convoy_route_tiles(h.w, h.w.convoys[0]).tiles;
        std::istringstream in(to_bytes(h.w), std::ios::binary);
        world back;
        const bool loaded = read_world_snapshot(back, in);
        const convoy_leg second = loaded ? price_trade_leg(back, reg, hn, h.A, h.ma, h.mb, k_good, 1.0f) : convoy_leg{};
        check(loaded && second.viable && second.cost == first.cost
                  && convoy_route_tiles(back, back.convoys[0]).tiles == lane,
              "C7a a save and load, its caches empty, routes and prices to the same bits");
        world twin = h.w;
        twin.lane_path_cache.clear();
        twin.lane_flood_fields.clear();
        check(price_trade_leg(twin, reg, hn, h.A, h.ma, h.mb, k_good, 1.0f).cost == first.cost,
              "C7b the same on a copy with the lane caches dropped: the cache is a cache");
    }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    std::printf(failures == 0 ? "ALL PASS (0 failures)\n" : "FAILURES\n");
    return failures == 0 ? 0 : 1;
}
