// embargo_harness.cpp — BL-1418: an embargo bars a nation's goods from entering
// your markets (RELATIONS.md § 3 Embargo).
//
// Rows:
//   R1  THE PATH WORKS without an embargo: a manual trade from a market on a third
//       nation's ground to one on A's ships and lands B's cargo.
//   R2  ENACTMENT through the seam (`set_embargo`, order 1): the pair (A, B) stands
//       and embargo_imposed is queued B -> A; after the fold B's row toward A has
//       Access -, Trust -, Alarm untouched, and A forms nothing.
//   R3  THE BORDER REFUSES cargo in flight: B's convoy arriving at A's market does
//       not land; it is destroyed (gone from the convoys, from every shelf and from
//       the landings), and B is paid nothing.
//   R4  DISPATCH REFUSES: the standing trade ships nothing while the embargo holds
//       (the source shelf is untouched), and the trade itself still stands.
//   R5  A NEW manual trade into A's market answers rejected_embargo, mutating nothing.
//   R6  ONE-WAY: A's own cargo is not refused anywhere, and B's cargo is not refused
//       in a market on another nation's ground.
//   R7  LIFTING restores it: order 0 repeals, queues no conduct, and the standing
//       trade ships and lands again.
//   R8  THE SEAM IS AN UNTRUSTED BOUNDARY: an out-of-range order, a self-target, a
//       ghost id, a non-nation counterparty, a null counterparty, a non-nation actor,
//       a foreign as_nation, enacting twice and repealing nothing are each refused
//       whole, the world fingerprint byte-identical.
//   R9  SAVE (v70): the embargoes round-trip byte for byte; a v69 stream (today's
//       less the embargo section) loads with none; a pair naming a ghost nation, a
//       self pair, or a pair out of order is refused whole, the destination untouched.
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/embargo.hpp"
#include "world/logistic_points.hpp"
#include "world/logistics.hpp"
#include "world/recipe_registry.hpp"
#include "world/sentiment.hpp"
#include "world/sentiment_emitters.hpp"
#include "world/supply_system.hpp"
#include "world/trade.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include "lp_fixture.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

constexpr std::size_t r_iron = static_cast<std::size_t>(resource_type::iron_ore);
constexpr float k_cap = 25.0f;

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

/// Every field a set_embargo or set_trade command could touch, sorted walks only.
std::string fingerprint(const world& w)
{
    std::ostringstream o;
    o.precision(9);
    std::vector<entity_id> ids;
    for (const auto& kv : w.nations) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    for (const entity_id id : ids)
        o << "N" << id << ':' << w.nations.at(id).treasury << ':' << w.nations.at(id).trade_reserve << ';';
    for (const auto& [a, b] : w.nation_embargoes)
        o << "E" << a << '>' << b << ';';
    for (const sentiment_event& e : w.sentiment_pending)
        o << "P" << e.observer << '>' << e.subject << ':' << static_cast<int>(e.kind) << ':' << e.magnitude << ';';
    for (const auto& [k, v] : w.sentiment.pairs)
        o << "S" << k.first << '>' << k.second << ':' << v.access << '/' << v.trust << '/' << v.alarm << ';';
    for (const standing_trade& t : w.trades)
        o << "X" << t.id << ':' << t.owner << ':' << t.from_market << '>' << t.to_market << ':' << t.points << ';';
    for (const auto& [key, sp] : w.landed_this_tick)
    {
        o << "L" << key.first << '/' << key.second << ':';
        for (const float q : sp.quantities) o << q << ',';
    }
    std::vector<entity_id> mids;
    for (const auto& kv : w.markets) mids.push_back(kv.first);
    std::sort(mids.begin(), mids.end());
    for (const entity_id id : mids)
        o << "M" << id << ':' << w.markets.at(id).inventory[r_iron] << ';';
    o << "C" << w.next_convoy_id << ':' << w.convoys.size() << ';';
    return o.str();
}

struct scenario
{
    world     w;
    entity_id body   = null_entity;
    entity_id trader = null_entity; ///< B: the nation whose cargo is barred.
    entity_id author = null_entity; ///< A: the nation whose market is on its ground.
    entity_id third  = null_entity; ///< the lp_fixture nation holding the source ground.
    entity_id src    = null_entity;
    entity_id dst    = null_entity;
};

entity_id tile_at(world& w, entity_id body, int c, int r)
{
    const int gw = w.bodies.at(body).grid_width;
    return body_tile_grid(w, body)[static_cast<std::size_t>(r) * static_cast<std::size_t>(gw)
                                   + static_cast<std::size_t>(c)];
}

/// convoy_command.cpp's fixture: one 32x4 plains body, a source market at (0,0) and a
/// destination at (0,3); B holds a staffed Planetary Marketplace in the source's
/// catchment. The destination's centre is A's ground; every other tile goes to the
/// lp_fixture nation, which makes the source's Logistic Points.
scenario make_scenario()
{
    scenario s;
    s.body = s.w.create_entity();
    body_component bc{};
    bc.name = "Anvil"; bc.type = body_type::planet; bc.orbital_radius_au = 1.0f;
    bc.grid_width = 32; bc.grid_height = 4;
    s.w.bodies[s.body] = bc;
    for (int r = 0; r < bc.grid_height; ++r)
        for (int c = 0; c < bc.grid_width; ++c)
        {
            const entity_id t = s.w.create_entity();
            tile_component tc{};
            tc.body = s.body; tc.grid_x = c; tc.grid_y = r;
            tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass; tc.cover_density = 150;
            tc.landform = terrain_landform::plains;
            s.w.tiles[t] = tc;
        }

    s.trader = s.w.create_entity();
    nation_component tn{};
    tn.name = "Shipper";
    tn.treasury = 1000.0f;
    tn.trade_reserve = 1.0f; // the point is held for manual trades (no auto in this fixture)
    const entity_id mp = s.w.create_entity();
    building_component b{};
    b.tile = tile_at(s.w, s.body, 20, 0);
    b.type = building_type::planetary_marketplace;
    b.workforce_assigned = 1.0f;
    s.w.buildings[mp] = b;
    tn.state_assets.push_back(mp);
    s.w.nations[s.trader] = tn;
    s.w.player_entity = s.trader;

    s.author = s.w.create_entity();
    nation_component an{};
    an.name = "Author";
    an.treasury = 500.0f;
    s.w.nations[s.author] = an;

    s.w.population_centre_tile[s.w.create_entity()] = tile_at(s.w, s.body, 1, 0);

    s.src = s.w.create_entity();
    market_component sm{};
    sm.body = s.body;
    sm.centre_tile = tile_at(s.w, s.body, 0, 0);
    sm.base_price[r_iron] = 5.0f;
    sm.price = sm.base_price;
    sm.inventory[r_iron] = 200.0f;
    s.w.markets[s.src] = sm;

    s.dst = s.w.create_entity();
    market_component dm{};
    dm.body = s.body;
    dm.centre_tile = tile_at(s.w, s.body, 0, 3);
    dm.base_price[r_iron] = 5.0f;
    dm.price = dm.base_price;
    s.w.markets[s.dst] = dm;

    s.w.tile_to_nation[dm.centre_tile] = s.author; // the destination is A's ground
    // The hub stands ON the source centre: a road source reaches along the road
    // network, and this bare grid has none, so a hub a tile away covers nothing.
    s.third = lp_fixture::stage(s.w, s.body, {tile_at(s.w, s.body, 0, 0)}, 10);
    return s;
}

corp_command trade_cmd(const scenario& s)
{
    corp_command cmd;
    cmd.corp = s.trader;
    cmd.verb = corp_verb::set_trade;
    cmd.subject = s.src;
    cmd.counterparty = s.dst;
    cmd.target = resource_type::iron_ore;
    cmd.quantity = 1.0f;
    return cmd;
}

corp_command embargo_cmd(entity_id author, entity_id target, uint32_t order)
{
    corp_command cmd;
    cmd.corp = author;
    cmd.verb = corp_verb::set_embargo;
    cmd.counterparty = target;
    cmd.order = order;
    return cmd;
}

trade_tick trade_pass(scenario& s, const recipe_registry& reg)
{
    economy_report rep;
    return run_trades(s.w, reg, rep);
}

float iron_everywhere(const scenario& s)
{
    float total = s.w.markets.at(s.src).inventory[r_iron] + s.w.markets.at(s.dst).inventory[r_iron];
    for (const auto& [key, sp] : s.w.landed_this_tick) total += sp.quantities[r_iron];
    for (const convoy_component& c : s.w.convoys)
        if (c.cargo_resource == resource_type::iron_ore) total += c.cargo_qty;
    return total;
}

void check_rejected(scenario& s, const recipe_registry& reg, const corp_command& cmd,
                    corp_command_result want, const char* what)
{
    const std::string before = fingerprint(s.w);
    const corp_command_result got = apply_corp_command(s.w, reg, cmd);
    if (got != want)
        std::printf("       (expected result %d, got %d)\n", static_cast<int>(want), static_cast<int>(got));
    check(got == want && fingerprint(s.w) == before, what);
}

} // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("=== embargo_harness (BL-1418: an embargo bars a nation's goods) ===\n");

    recipe_registry reg;
    {
        trade_params tp;
        tp.capacity[r_iron] = k_cap;
        tp.marketplace_points = 1.0f;
        reg.set_trade(tp);
        sentiment_params sp; // RELATIONS.md § Who emits: embargo_imposed is Access -, Trust -
        sp.factors[static_cast<std::size_t>(sentiment_factor_kind::embargo_imposed)] =
            sentiment_factor{ -4.0f, -2.0f, 0.0f };
        reg.set_sentiment(sp);
    }

    scenario s = make_scenario();
    const entity_id A = s.author, B = s.trader;

    // --- R1: the path works ------------------------------------------------
    std::printf("\n-- R1 the path works without an embargo --\n");
    check(apply_corp_command(s.w, reg, trade_cmd(s)) == corp_command_result::applied && s.w.trades.size() == 1,
          "R1a B's manual trade into a market on A's ground applies");
    {
        // The source anchor must be covered, or every shipment is refused for Logistic
        // Points before the embargo is ever read (the fixture's own precondition).
        world probe = s.w;
        const lp_tick lt = resolve_logistic_points(probe);
        const auto ai = lt.trade_anchors.find(s.src);
        check(ai != lt.trade_anchors.end() && ai->second.cover >= 1.0f,
              "R1 fixture: the source market's anchor is fully covered by Logistic Points");
    }
    {
        const trade_tick t1 = trade_pass(s, reg);
        std::printf("       (points made %.3f, spent %.3f, refused for LP %d)\n",
                    t1.points_made, t1.points_spent, static_cast<int>(t1.refused_no_lp));
        check(t1.manual_shipments == 1 && s.w.convoys.size() == 1,
              "R1b the trade pass ships B's convoy toward A's market");
    }
    if (s.w.convoys.empty())
    {
        std::printf("FAIL  fixture: no convoy to carry the rest\n");
        return 1;
    }
    {
        // A copy lands it, to prove the border admits B's cargo with no embargo.
        world probe = s.w;
        probe.convoys.front().arrived = true;
        const float q = probe.convoys.front().cargo_qty;
        credit_arrived_convoys(probe, 1);
        check(q > 0.0f && probe.landed(B, s.dst, r_iron) == q && probe.convoys.empty(),
              "R1c with no embargo B's arriving cargo lands on A's market whole");
    }

    // --- R2: enactment -----------------------------------------------------
    std::printf("\n-- R2 enactment through the seam --\n");
    check(apply_corp_command(s.w, reg, embargo_cmd(A, B, 1)) == corp_command_result::applied
              && embargoes(s.w, A, B) && !embargoes(s.w, B, A) && s.w.nation_embargoes.size() == 1,
          "R2a set_embargo order 1 applies: (A, B) stands, and only that direction");
    check(s.w.sentiment_pending.size() == 1 && s.w.sentiment_pending[0].observer == B
              && s.w.sentiment_pending[0].subject == A
              && s.w.sentiment_pending[0].kind == sentiment_factor_kind::embargo_imposed,
          "R2b enactment queues embargo_imposed from the embargoed toward the author");
    fold_pending_sentiment(s.w, reg);
    {
        const sentiment_value ba = sentiment_toward(s.w.sentiment, B, A);
        check(ba.access < 0.0f && ba.trust < 0.0f && ba.alarm == 0.0f
                  && ba == sentiment_value{ -4.0f, -2.0f, 0.0f } && s.w.sentiment.pairs.count({ A, B }) == 0,
              "R2c after the fold B's row toward A reads Access -4, Trust -2, Alarm 0; A forms nothing");
    }

    // --- R3: the border refuses cargo in flight -----------------------------
    std::printf("\n-- R3 the border refuses cargo already in flight --\n");
    {
        const float q = s.w.convoys.front().cargo_qty;
        const float before = iron_everywhere(s);
        const float tb = s.w.nations.at(B).treasury;
        const float ta = s.w.nations.at(A).treasury;
        const std::size_t routes = s.w.trade_routes.size();
        s.w.convoys.front().arrived = true;
        credit_arrived_convoys(s.w, 2);
        check(s.w.landed(B, s.dst, r_iron) == 0.0f && s.w.landed_this_tick.empty(),
              "R3a B's cargo arriving at A's market does not land");
        check(s.w.convoys.empty() && std::fabs((before - iron_everywhere(s)) - q) < 1e-4f,
              "R3b the refused cargo is destroyed: the convoy is gone and the iron leaves the world");
        check(s.w.nations.at(B).treasury == tb && s.w.nations.at(A).treasury == ta
                  && s.w.trade_routes.size() == routes && s.w.sentiment_trade_landed.empty(),
              "R3c nobody is paid, no duty moves, no route or trade value is recorded");
    }

    // --- R4: dispatch refuses ----------------------------------------------
    std::printf("\n-- R4 dispatch refuses while the embargo holds --\n");
    {
        const float shelf = s.w.markets.at(s.src).inventory[r_iron];
        const float tb = s.w.nations.at(B).treasury;
        const trade_tick t = trade_pass(s, reg);
        check(t.manual_shipments == 0 && t.auto_shipments == 0 && s.w.convoys.empty()
                  && s.w.markets.at(s.src).inventory[r_iron] == shelf && s.w.nations.at(B).treasury == tb,
              "R4a the standing trade ships nothing: no convoy, the source shelf and B's treasury untouched");
        check(s.w.trades.size() == 1, "R4b the standing trade itself is kept");
    }

    // --- R5: a new trade is refused ----------------------------------------
    std::printf("\n-- R5 a new manual trade into A's market --\n");
    check_rejected(s, reg, trade_cmd(s), corp_command_result::rejected_embargo,
                   "R5 set_trade into a market whose nation embargoes the actor is rejected_embargo, mutating nothing");

    // --- R6: one-way -------------------------------------------------------
    std::printf("\n-- R6 one-way --\n");
    check(!embargo_refuses(s.w, A, s.dst) && !embargo_refuses(s.w, A, s.src),
          "R6a the author's own cargo is refused nowhere");
    check(embargo_refuses(s.w, B, s.dst) && !embargo_refuses(s.w, B, s.src),
          "R6b B's cargo is refused on A's ground only, not on a third nation's");

    // --- R7: lifting restores ----------------------------------------------
    std::printf("\n-- R7 lifting the embargo restores the trade --\n");
    check(apply_corp_command(s.w, reg, embargo_cmd(A, B, 0)) == corp_command_result::applied
              && s.w.nation_embargoes.empty() && s.w.sentiment_pending.empty(),
          "R7a set_embargo order 0 repeals: the pair is gone and no conduct is queued");
    check(trade_pass(s, reg).manual_shipments == 1 && s.w.convoys.size() == 1,
          "R7b the standing trade ships again");
    {
        s.w.convoys.front().arrived = true;
        const float q = s.w.convoys.front().cargo_qty;
        credit_arrived_convoys(s.w, 3);
        check(q > 0.0f && s.w.landed(B, s.dst, r_iron) == q, "R7c ...and B's cargo lands on A's market");
    }
    s.w.landed_this_tick.clear();

    // --- R8: the seam ------------------------------------------------------
    std::printf("\n-- R8 the seam refuses malformed commands whole --\n");
    {
        check_rejected(s, reg, embargo_cmd(A, B, 2), corp_command_result::rejected_invalid,
                       "R8a order 2 is rejected_invalid, mutating nothing");
        check_rejected(s, reg, embargo_cmd(A, B, 0xFFFFFFFFu), corp_command_result::rejected_invalid,
                       "R8b order UINT32_MAX is rejected_invalid, mutating nothing");
        check_rejected(s, reg, embargo_cmd(A, A, 1), corp_command_result::rejected_invalid,
                       "R8c a nation embargoing itself is rejected_invalid, mutating nothing");
        check_rejected(s, reg, embargo_cmd(A, 0x7FFFFFF0u, 1), corp_command_result::rejected_invalid,
                       "R8d a ghost counterparty id is rejected_invalid, mutating nothing");
        check_rejected(s, reg, embargo_cmd(A, s.dst, 1), corp_command_result::rejected_invalid,
                       "R8e a counterparty that is not a nation (a market) is rejected_invalid, mutating nothing");
        check_rejected(s, reg, embargo_cmd(A, null_entity, 1), corp_command_result::rejected_invalid,
                       "R8f a null counterparty is rejected_invalid, mutating nothing");
        check_rejected(s, reg, embargo_cmd(s.src, B, 1), corp_command_result::rejected_no_corp,
                       "R8g an actor that is not a nation is rejected_no_corp, mutating nothing");
        corp_command foreign = embargo_cmd(A, B, 1);
        foreign.as_nation = B;
        check_rejected(s, reg, foreign, corp_command_result::rejected_not_owner,
                       "R8h acting as another nation is rejected_not_owner, mutating nothing");
        check_rejected(s, reg, embargo_cmd(A, B, 0), corp_command_result::rejected_state,
                       "R8i repealing an embargo not in force is rejected_state, mutating nothing");
        check(apply_corp_command(s.w, reg, embargo_cmd(A, B, 1)) == corp_command_result::applied,
              "R8j (re-enacted for the next row)");
        s.w.sentiment_pending.clear();
        check_rejected(s, reg, embargo_cmd(A, B, 1), corp_command_result::rejected_state,
                       "R8k enacting an embargo already in force is rejected_state, mutating nothing");
        check(set_embargo(s.w, A, B, true) == embargo_result::rejected_state
                  && set_embargo(s.w, A, A, true) == embargo_result::rejected_invalid,
              "R8l set_embargo itself answers the same refusals");
    }

    // --- R9: save ----------------------------------------------------------
    std::printf("\n-- R9 save (v70) --\n");
    {
        s.w.nation_embargoes.insert({ B, A });       // both directions now
        s.w.nation_embargoes.insert({ s.third, A }); // and a third pair
        const std::string b = to_bytes(s.w);
        world back;
        const bool ok = from_bytes(b, back);
        check(ok && back.nation_embargoes == s.w.nation_embargoes && to_bytes(back) == b,
              "R9a three embargoes round-trip with their directions, byte for byte");
        uint32_t ver = 0;
        std::memcpy(&ver, b.data() + 4, 4);
        check(ver == world_save_version && ver == k_world_save_version_embargo, "R9b the stream is v70");

        std::ostringstream o69(std::ios::binary);
        write_world_snapshot_pre_embargo(s.w, o69);
        const std::string b69 = o69.str();
        world old;
        check(from_bytes(b69, old) && old.nation_embargoes.empty()
                  && b69.size() == b.size() - world_save_embargo_section_bytes(s.w)
                  && world_save_embargo_section_bytes(s.w) == 4 + 8 * 3,
              "R9c a v69 stream (today's less the 28-byte embargo section) loads with no embargo");

        // The section is last: count, then (author, embargoed) pairs ascending.
        const std::size_t sec = b.size() - world_save_embargo_section_bytes(s.w);
        const auto refused = [&](std::string bad, const char* what) {
            world untouched;
            check(!from_bytes(bad, untouched) && untouched.nations.empty() && untouched.nation_embargoes.empty(), what);
        };
        {
            std::string bad = b;
            const entity_id ghost = 0x7FFFFFF0u;
            std::memcpy(&bad[sec + 4], &ghost, sizeof ghost);
            refused(bad, "R9d an embargo naming a ghost nation is refused whole, the destination untouched");
        }
        {
            std::string bad = b;
            std::memcpy(&bad[sec + 8], &bad[sec + 4], 4); // first pair: (x, x)
            refused(bad, "R9e a nation embargoing itself in the stream is refused whole");
        }
        {
            std::string bad = b;
            std::string first = bad.substr(sec + 4, 8), second = bad.substr(sec + 12, 8);
            bad.replace(sec + 4, 8, second);
            bad.replace(sec + 12, 8, first);
            refused(bad, "R9f pairs out of ascending order are refused whole");
        }
        {
            std::string bad = b;
            const uint32_t huge = 0x00FFFFFFu;
            std::memcpy(&bad[sec], &huge, 4);
            refused(bad, "R9g a count past nations x nations is refused whole");
        }
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
