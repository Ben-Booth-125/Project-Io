// Headless harness (BL-1267): a rival sets its own trades — the scorer's trade
// candidate family under the AI_OPPONENT.md § 11 grant (Ben, 2026-10-10).
//
//   T1 PIN      a rival with a Planetary Marketplace and a profitable route pins
//               the route the shared ranking (`rank_trade_routes`, the one auto
//               trade spends by) names best, with points capped at
//               `trade_pin_share` of the points it makes; one pin per evaluation.
//   T2 RESERVE  in the same evaluation its reserve becomes the sum of its manual
//               trades' points, and stays so.
//   T3 UNPIN    when that route's margin per unit turns negative, it clears the
//               trade, and the reserve follows to what remains.
//   T4 PLAYER   the player's corp — given a Marketplace, points and a losing
//               manual trade of its own — is never acted on: its trades and
//               reserve are untouched and no decision names it, played OR
//               spectated (the trade family excludes it even under spectate).
//   T5 DETERMINISM  the whole sequence on a faithful copy of the world
//               (world_copy_determinism) yields the identical command stream
//               and trade book.
//
// The world is the app's campaign start for one seed (`build_app_start_world`),
// seated as the app seats it; the route is made profitable by setting one good's
// prices on two markets of the home body. Build:
//   bash tools/verify/build_lua_harness.sh ai_trade_harness
// Run: ./build_gen/verify/ai_trade_harness.exe [--seed N]   (default 43)

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/supply_system.hpp"
#include "world/trade.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
void check(bool ok, const char* label)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

std::uint32_t fbits(float f)
{
    std::uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

/// The decisions pushed since the ring's lifetime count was @p total_before.
std::vector<corp_decision> decisions_since(const corp_decision_ring& ring, std::size_t total_before)
{
    std::vector<corp_decision> out;
    const std::size_t n = ring.total - total_before;
    const std::size_t cap = corp_decision_ring::capacity;
    if (ring.entries.size() < cap)
    {
        for (std::size_t i = ring.entries.size() - n; i < ring.entries.size(); ++i)
            out.push_back(ring.entries[i]);
        return out;
    }
    for (std::size_t k = 0; k < n; ++k)
        out.push_back(ring.entries[(ring.next + cap - n + k) % cap]);
    return out;
}

/// A line per command — what the determinism check compares.
std::string command_line(const corp_decision& d)
{
    char buf[256];
    std::snprintf(buf, sizeof buf, "t%d c%llu v%d s%llu cp%llu r%d q%08X o%u sc%08X ru%08X re%d\n",
                  d.tick, static_cast<unsigned long long>(d.corp), static_cast<int>(d.command.verb),
                  static_cast<unsigned long long>(d.command.subject),
                  static_cast<unsigned long long>(d.command.counterparty),
                  static_cast<int>(d.command.target), fbits(d.command.quantity), d.command.order,
                  fbits(d.winning_score), fbits(d.runner_up), static_cast<int>(d.reason));
    return buf;
}

std::string trade_book(const world& w)
{
    std::string s;
    char buf[160];
    for (const standing_trade& t : w.trades)
    {
        std::snprintf(buf, sizeof buf, "id%u o%llu r%d %llu->%llu p%08X\n", t.id,
                      static_cast<unsigned long long>(t.owner), static_cast<int>(t.resource),
                      static_cast<unsigned long long>(t.from_market),
                      static_cast<unsigned long long>(t.to_market), fbits(t.points));
        s += buf;
    }
    std::vector<entity_id> ids;
    for (const auto& [cid, cc] : w.corporations) { (void)cc; ids.push_back(cid); }
    std::sort(ids.begin(), ids.end());
    for (const entity_id cid : ids)
    {
        std::snprintf(buf, sizeof buf, "c%llu res%08X\n", static_cast<unsigned long long>(cid),
                      fbits(w.corporations.at(cid).trade_reserve));
        s += buf;
    }
    return s;
}

float manual_points(const world& w, entity_id corp)
{
    float s = 0.0f;
    for (const standing_trade& t : w.trades)
        if (t.owner == corp)
            s += t.points;
    return s;
}

entity_id give_marketplace(world& w, entity_id corp, entity_id tile)
{
    const entity_id id = w.create_entity();
    building_component b{};
    b.tile               = tile;
    b.type               = building_type::planetary_marketplace;
    b.workforce_assigned = 0.5f;
    b.workforce_target   = 100;
    b.ticks_remaining    = 0;
    w.buildings[id] = b;
    w.corporations.at(corp).assets.push_back(id);
    return id;
}

/// The fixture's chosen route: good r from market a to market b on the home body.
struct rigged_route
{
    entity_id   a = null_entity;
    entity_id   b = null_entity;
    std::size_t r = 0;
};

/// Make good r cheap and stocked at a, dear and wanted at b.
void rig_profitable(world& w, const rigged_route& rr)
{
    market_component& ma = w.markets.at(rr.a);
    market_component& mb = w.markets.at(rr.b);
    ma.inventory[rr.r] = 5000.0f;
    ma.price[rr.r]     = ma.base_price[rr.r] * 0.5f;
    mb.price[rr.r]     = mb.base_price[rr.r] * 1.5f;
    mb.demand[rr.r]    = 5000.0f;
    mb.supply[rr.r]    = 0.0f;
}

struct run_record
{
    std::string stream;  ///< every command, in order
    std::string book;    ///< the trade book after each step
};

/// The scripted sequence. @p verbose prints the checks (the first run only).
run_record run_sequence(world& w, const recipe_registry& reg, bool verbose)
{
    run_record rec;
    corp_ai_params p;
    p.cadence_k = 1; // every corp evaluates every step

    // --- the actors -------------------------------------------------------
    const entity_id player = w.player_entity;
    std::vector<entity_id> ids;
    for (const auto& [cid, cc] : w.corporations) { (void)cc; ids.push_back(cid); }
    std::sort(ids.begin(), ids.end());
    entity_id rival = null_entity;
    for (const entity_id c : ids)
        if (c != player && !w.corporations.at(c).is_player) { rival = c; break; }

    const entity_id body = w.home_body;
    entity_id tile = null_entity;
    for (const auto& [tid, tc] : w.tiles)
        if (tc.body == body && (tile == null_entity || tid < tile))
            tile = tid;
    std::vector<entity_id> mkts;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body) mkts.push_back(mid);
    std::sort(mkts.begin(), mkts.end());

    if (rival == null_entity || player == null_entity || tile == null_entity || mkts.size() < 2)
    {
        check(false, "fixture: a rival, a seated player, a home tile and two home markets");
        return rec;
    }
    give_marketplace(w, rival, tile);
    give_marketplace(w, player, tile);
    w.corporations.at(rival).balance += 1.0e7f; // the solvency gate is not under test here
    w.corporations.at(rival).trade_points = 20.0f;   // as made on the last trade pass
    w.corporations.at(player).trade_points = 20.0f;

    // --- the route: a viable home-body pair and the good with the best margin
    const trade_params& tp = reg.trade();
    const logistics_nodes nodes = collect_logistics_nodes(w);
    trade_haul_memo memo;
    rigged_route rr;
    float best = 0.0f;
    for (const entity_id a : mkts)
        for (const entity_id b : mkts)
        {
            if (a == b) continue;
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                if (!(tp.capacity[r] > 0.0f) || reg.grid_goods().grid(r)) continue;
                const float ba = w.markets.at(a).base_price[r], bb = w.markets.at(b).base_price[r];
                if (!(ba > 0.0f) || !(bb > 0.0f)) continue;
                const float haul = trade_haul_per_unit(w, reg, nodes, memo, rival, a, b, r);
                if (!std::isfinite(haul)) continue;
                const float m = (1.5f * bb - 0.5f * ba - haul) * tp.capacity[r];
                if (m > best) { best = m; rr = {a, b, r}; }
            }
        }
    if (rr.a == null_entity)
    {
        check(false, "fixture: a viable profitable home-body route");
        return rec;
    }
    rig_profitable(w, rr);
    if (verbose)
        std::printf("  fixture: rival %llu, player %llu, route good %zu  %llu -> %llu\n",
                    static_cast<unsigned long long>(rival), static_cast<unsigned long long>(player),
                    rr.r, static_cast<unsigned long long>(rr.a), static_cast<unsigned long long>(rr.b));

    // The player holds a LOSING manual trade of its own and a stray reserve: a
    // scorer that touched the player's corp would clear the one or move the other.
    {
        corp_command pc;
        pc.corp = player; pc.verb = corp_verb::set_trade;
        pc.subject = rr.b; pc.counterparty = rr.a; // backwards: dear -> cheap
        pc.target = static_cast<resource_type>(rr.r); pc.quantity = 3.0f;
        const bool ok = apply_corp_command(w, reg, pc, nullptr) == corp_command_result::applied;
        if (verbose) check(ok, "fixture: the player's losing trade is placed through the seam");
        w.corporations.at(player).trade_reserve = 7.0f;
    }
    const std::string player_book_before = [&] {
        std::string s;
        for (const standing_trade& t : w.trades) if (t.owner == player) s += std::to_string(t.id) + ",";
        return s;
    }();

    // The oracle: the shared ranking, read before the scorer runs.
    std::vector<trade_route_offer> ranked;
    {
        trade_haul_memo m2;
        rank_trade_routes(w, reg, nodes, m2, rival, corp_trade_markets(w, rival), ranked);
    }

    auto step = [&](int tick, bool spectating) -> std::vector<corp_decision> {
        corp_ai_params q = p;
        q.spectating = spectating;
        economy_report report;
        const std::size_t before = w.ai_decisions.total;
        run_corp_strategic_step(w, reg, report, tick, q);
        std::vector<corp_decision> ds = decisions_since(w.ai_decisions, before);
        for (const corp_decision& d : ds) rec.stream += command_line(d);
        rec.book += trade_book(w);
        return ds;
    };
    auto count_reason = [&](const std::vector<corp_decision>& ds, entity_id corp,
                            corp_decision_reason why) {
        int n = 0;
        for (const corp_decision& d : ds) if (d.corp == corp && d.reason == why) ++n;
        return n;
    };
    auto names_player = [&](const std::vector<corp_decision>& ds) {
        for (const corp_decision& d : ds) if (d.corp == player) return true;
        return false;
    };

    // --- T1 / T2: pin and reserve -------------------------------------------
    const std::vector<corp_decision> d1 = step(100, false);
    std::vector<standing_trade> mine;
    for (const standing_trade& t : w.trades) if (t.owner == rival) mine.push_back(t);
    if (verbose)
    {
        std::printf("T1 pin\n");
        check(!ranked.empty(), "the shared ranking names at least one route for the rival");
        check(mine.size() == 1, "the rival pinned exactly one manual trade (one per evaluation)");
        check(count_reason(d1, rival, corp_decision_reason::trade_pin) == 1,
              "the decision log carries one trade_pin for the rival");
        if (!mine.empty() && !ranked.empty())
        {
            const standing_trade& t = mine.front();
            check(t.from_market == ranked.front().a && t.to_market == ranked.front().b
                      && static_cast<std::size_t>(t.resource) == ranked.front().r,
                  "the pinned route is the shared ranking's best");
            check(t.points > 0.0f && t.points <= p.trade_pin_share * 20.0f + 1e-4f,
                  "its points are within trade_pin_share of the points made");
            std::printf("    pinned %.3f points of good %d, %llu -> %llu (margin/unit %.3f)\n",
                        t.points, static_cast<int>(t.resource),
                        static_cast<unsigned long long>(t.from_market),
                        static_cast<unsigned long long>(t.to_market), ranked.front().margin_per_unit);
        }
        std::printf("T2 reserve\n");
        check(std::fabs(w.corporations.at(rival).trade_reserve - manual_points(w, rival)) <= 1e-4f,
              "the rival's reserve equals its manual trades' points, same evaluation");
        check(count_reason(d1, rival, corp_decision_reason::trade_reserve) == 1,
              "the decision log carries the reserve correction");
    }

    // A second evaluation at unchanged prices: nothing to unpin, the held route
    // is not pinned twice, and the reserve still tracks.
    const std::vector<corp_decision> d2 = step(101, false);
    if (verbose)
    {
        int dup = 0;
        for (const standing_trade& a : w.trades)
            for (const standing_trade& b : w.trades)
                if (a.id < b.id && a.owner == rival && b.owner == rival && a.resource == b.resource
                    && a.from_market == b.from_market && a.to_market == b.to_market)
                    ++dup;
        check(dup == 0, "a held route is never pinned twice");
        check(count_reason(d2, rival, corp_decision_reason::trade_unpin) == 0,
              "a still-earning trade is not unpinned");
        check(std::fabs(w.corporations.at(rival).trade_reserve - manual_points(w, rival)) <= 1e-4f,
              "the reserve still equals the manual points after a second evaluation");
    }

    // --- T3: unpin -------------------------------------------------------------
    std::vector<uint32_t> losing_ids;
    for (const standing_trade& t : w.trades)
        if (t.owner == rival)
        {
            // Turn every one of the rival's routes into a loser.
            market_component& mb = w.markets.at(t.to_market);
            const std::size_t r = static_cast<std::size_t>(t.resource);
            mb.price[r] = std::max(1e-3f, w.markets.at(t.from_market).price[r] * 0.25f);
            losing_ids.push_back(t.id);
        }
    const std::size_t held_before = losing_ids.size();
    const std::vector<corp_decision> d3 = step(102, false);
    if (verbose)
    {
        std::printf("T3 unpin\n");
        std::size_t still = 0;
        for (const standing_trade& t : w.trades)
            if (std::find(losing_ids.begin(), losing_ids.end(), t.id) != losing_ids.end()) ++still;
        check(held_before >= 1 && still == held_before - 1,
              "one losing trade is cleared this evaluation (one unpin per evaluation)");
        check(count_reason(d3, rival, corp_decision_reason::trade_unpin) == 1,
              "the decision log carries one trade_unpin for the rival");
        check(std::fabs(w.corporations.at(rival).trade_reserve - manual_points(w, rival)) <= 1e-4f,
              "the reserve follows the manual points down");
    }

    // --- T4: the player's corp, played and spectated -------------------------
    const std::vector<corp_decision> d4 = step(103, true);
    if (verbose)
    {
        std::printf("T4 player\n");
        const std::string player_book_after = [&] {
            std::string s;
            for (const standing_trade& t : w.trades) if (t.owner == player) s += std::to_string(t.id) + ",";
            return s;
        }();
        check(player_book_after == player_book_before && !player_book_before.empty(),
              "the player's losing manual trade still stands (never unpinned for it)");
        check(w.corporations.at(player).trade_reserve == 7.0f,
              "the player's reserve is untouched");
        check(!names_player(d1) && !names_player(d2) && !names_player(d3),
              "no decision names the player's corp in a played session");
        bool trade_for_player = false;
        for (const corp_decision& d : d4)
            if (d.corp == player
                && (d.command.verb == corp_verb::set_trade || d.command.verb == corp_verb::clear_trade
                    || d.command.verb == corp_verb::set_trade_reserve))
                trade_for_player = true;
        check(!trade_for_player, "no trade command for the player's corp even under spectate");
    }
    return rec;
}

} // namespace

int main(int argc, char** argv)
{
    std::uint32_t seed = 43;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
            seed = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 10));

    std::printf("ai_trade_harness (BL-1267): seed %u\n", seed);
    lua_state lua;
    world_params wp;
    wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, wp, *start);
    const spawn_seat_result seat =
        seat_player_corporation(start->w, seed, start->land.search.winner_score);
    if (seat.seated == null_entity)
    {
        std::printf("FAIL: no corporation seated\n");
        return 1;
    }
    world copy = start->w; // faithful copy (BL-1034) for the determinism run

    const run_record a = run_sequence(start->w, start->reg, /*verbose=*/true);
    const run_record b = run_sequence(copy, start->reg, /*verbose=*/false);
    std::printf("T5 determinism\n");
    check(!a.stream.empty() && a.stream == b.stream,
          "two runs from the same start issue the identical command stream");
    check(a.book == b.book, "and leave the identical trade book and reserves after every step");

    std::printf("ai_trade_harness: %s (%d failure%s)\n", g_failures == 0 ? "PASS" : "FAIL",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
