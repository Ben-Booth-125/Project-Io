// naval_reading.cpp - BL-1526 (sea trade reading) and BL-1535 (naval war reading). A MEASUREMENT
// on the shipped start (the app's world, its 12-tick settle, the seat, then live quarters as
// the app steps them), per seed. It REPORTS; the readings are for Ben, not gates.
//
//   TRADE (BL-1526, MILITARY.md § Sea): every convoy that lands is valued at its cargo times
//   its destination market's price the tick it was last seen in flight. Per seed and per
//   major: the share of landed value that crossed a sea leg (a loading port, `port_a`), the
//   same for what the nation's markets IMPORTED and EXPORTED, and how many ports carried it.
//
//   WAR (BL-1535): fleets (ships alive at the handoff and the end, rival hires), wars, ship
//   ticks on raid and on blockade, raids by outcome and the value they took, ports closed
//   (the most at once), units landed from transports and their carried food at the landing
//   and at the end, the crew wage bill against the treasury of nations with ships, and the
//   price at a blockaded market against the mean of the same good elsewhere.
//
//   INVARIANT (asserted): the run measured a non-empty world (landed value > 0 on some seed).
//
//   build_gen/verify/naval_reading.exe [--seeds a,b,c] [--ticks T]
//
// Build: bash tools/verify/build_lua_harness.sh naval_reading. Run from the repo root.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/campaign_settle.hpp"
#include "world/economy_system.hpp" // corp_is_mobilised, economy_report::interceptions
#include "world/embargo.hpp"        // embargo_market_nation
#include "world/nation_step.hpp"    // nation_crew_wage_bill
#include "world/orbital_system.hpp"
#include "world/sea_combat.hpp"
#include "world/spawn_seat.hpp"
#include "world/survey_system.hpp"
#include "world/war.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

const std::vector<std::uint32_t> k_library = { 46, 28, 11, 31, 40, 12, 37, 13, 41, 43, 32, 10, 25, 38, 9, 0 };

struct flight
{
    entity_id     src = null_entity, dst = null_entity, port_a = null_entity, port_b = null_entity;
    resource_type res = resource_type::iron_ore;
    float         qty = 0.0f, value = 0.0f;
};

struct nation_trade
{
    double imp = 0.0, imp_sea = 0.0, exp = 0.0, exp_sea = 0.0;
};

int g_fail = 0;
double g_landed_all = 0.0;

int nations_at_war(const world& w)
{
    std::set<entity_id> s;
    for (const auto& [a, b] : w.corp_hostile_pairs) { s.insert(a); s.insert(b); }
    return static_cast<int>(s.size());
}

void run_seed(std::uint32_t seed, int last_tick)
{
    lua_state lua;
    world_params wp;
    wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, wp, *start);
    world& w = start->w;
    const recipe_registry& reg = start->reg;

    for (int step = 0; step < k_campaign_settle_ticks; ++step)
        (void)run_settle_tick(w, reg, step, /*day_tick=*/0, /*spectating=*/true);
    clear_research_at_handover(w);
    seat_player_corporation(w, seed, start->land.search.winner_score);
    const entity_id player = player_nation(w);

    const auto price_at = [&](entity_id m, resource_type r) {
        const auto it = w.markets.find(m);
        return it == w.markets.end() ? 0.0f : it->second.price[static_cast<std::size_t>(r)];
    };
    std::map<uint32_t, flight> in_flight;
    const auto snapshot = [&]() {
        std::map<uint32_t, flight> now;
        for (const convoy_component& c : w.convoys)
        {
            flight f;
            f.src = c.source_market; f.dst = c.dest_market; f.port_a = c.port_a; f.port_b = c.port_b;
            f.res = c.cargo_resource; f.qty = c.cargo_qty;
            f.value = c.cargo_qty * price_at(c.dest_market, c.cargo_resource);
            now[c.id] = f;
        }
        return now;
    };
    in_flight = snapshot();

    std::set<entity_id> known_ships;
    int ships_start = 0;
    for (const auto& [sid, s] : w.ships) { known_ships.insert(sid); ++ships_start; }

    double landed = 0.0, landed_sea = 0.0;
    long long convoys_landed = 0, convoys_sea = 0;
    std::map<entity_id, nation_trade> per;
    std::set<entity_id> ports_used;
    int hired = 0, wars_max = 0, closed_max = 0;
    long long raid_ticks = 0, blockade_ticks = 0;
    int raids_sunk = 0, raids_prize = 0;
    double raid_value = 0.0;
    std::map<entity_id, int32_t> landed_food; ///< unit -> food at the landing
    std::map<entity_id, entity_id> aboard_prev;  ///< unit -> ship, last tick
    double blockade_ratio_sum = 0.0;
    long long blockade_ratio_n = 0;
    // Before/during: each market's prices, one row a tick, so a closure reads against the
    // same market's price the tick before it closed, netted of the other markets' drift.
    using price_row = std::map<entity_id, std::vector<float>>;
    std::vector<price_row> history;
    std::map<entity_id, std::size_t> closed_since; ///< market -> history index before it closed
    double own_move_sum = 0.0, ctrl_move_sum = 0.0;
    long long move_n = 0;

    constexpr int k_days = 90;
    for (int step = k_campaign_settle_ticks, k = 1; step <= last_tick; ++step, ++k)
    {
        advance_orbits(w, static_cast<double>(k_days));
        advance_surveys(w, k_days);
        w.current_day_tick = k * k_days;
        const settle_tick_result res = run_settle_tick(w, reg, step, k * k_days, /*spectating=*/false);

        std::set<uint32_t> cut;
        for (const interception_record& x : res.report.interceptions)
        {
            cut.insert(x.convoy_id);
            const auto f = in_flight.find(x.convoy_id);
            const double v = f != in_flight.end() ? f->second.value : 0.0;
            if (x.outcome == interception_outcome::raid_sunk)  { ++raids_sunk;  raid_value += v; }
            if (x.outcome == interception_outcome::raid_prize) { ++raids_prize; raid_value += v; }
        }
        std::map<uint32_t, flight> now = snapshot();
        for (const auto& [id, f] : in_flight)
        {
            if (now.count(id) != 0 || cut.count(id) != 0)
                continue;
            const bool sea = f.port_a != null_entity;
            landed += f.value; ++convoys_landed;
            if (sea)
            {
                landed_sea += f.value; ++convoys_sea;
                ports_used.insert(f.port_a);
                if (f.port_b != null_entity) ports_used.insert(f.port_b);
            }
            const entity_id to = embargo_market_nation(w, f.dst), from = embargo_market_nation(w, f.src);
            if (to != from)
            {
                if (to != null_entity)   { per[to].imp += f.value;   if (sea) per[to].imp_sea += f.value; }
                if (from != null_entity) { per[from].exp += f.value; if (sea) per[from].exp_sea += f.value; }
            }
        }
        in_flight = std::move(now);

        wars_max = std::max(wars_max, nations_at_war(w));
        closed_max = std::max(closed_max, static_cast<int>(w.closed_ports.size()));
        std::map<entity_id, entity_id> aboard_now;
        for (const auto& [sid, s] : w.ships)
        {
            if (known_ships.insert(sid).second && s.owner != player)
                ++hired;
            if (s.mission == sea_mission::raid) ++raid_ticks;
            if (s.mission == sea_mission::blockade) ++blockade_ticks;
            for (const auto& [uid, u] : s.aboard) { (void)u; aboard_now[uid] = sid; }
        }
        for (const auto& [uid, sid] : aboard_prev)
            if (aboard_now.count(uid) == 0)
                if (const auto u = w.units.find(uid); u != w.units.end() && landed_food.count(uid) == 0)
                    landed_food[uid] = u->second.food_permille;
        aboard_prev = std::move(aboard_now);

        // Before/during: the closed market's own move against every other market's move.
        {
            price_row row;
            for (const auto& [mid, mc] : w.markets)
                row[mid] = std::vector<float>(std::begin(mc.price), std::end(mc.price));
            std::set<entity_id> closed_now;
            for (const entity_id port_tile : w.closed_ports)
                if (const entity_id m = market_for_tile(w, port_tile); w.markets.count(m) != 0)
                    closed_now.insert(m);
            for (auto it = closed_since.begin(); it != closed_since.end();)
                it = closed_now.count(it->first) ? std::next(it) : closed_since.erase(it);
            for (const entity_id m : closed_now)
                if (closed_since.count(m) == 0 && !history.empty())
                    closed_since[m] = history.size() - 1;
            for (const auto& [m, at] : closed_since)
            {
                const price_row& base = history[at];
                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    const float b0 = base.count(m) ? base.at(m)[r] : 0.0f, b1 = row[m][r];
                    if (!(b0 > 0.0f) || !(b1 > 0.0f)) continue;
                    double cs = 0.0; int cn = 0;
                    for (const auto& [om, prices] : row)
                        if (om != m && base.count(om) && base.at(om)[r] > 0.0f && prices[r] > 0.0f)
                        { cs += prices[r] / base.at(om)[r]; ++cn; }
                    if (cn == 0) continue;
                    own_move_sum += b1 / b0; ctrl_move_sum += cs / cn; ++move_n;
                }
            }
            history.push_back(std::move(row));
        }

        // A closed port's market: its price against the same good's mean elsewhere.
        // world::closed_ports holds the port's TILE (sea_combat.cpp refresh_closed_ports).
        for (const entity_id port_tile : w.closed_ports)
        {
            const entity_id m = market_for_tile(w, port_tile);
            const auto mit = w.markets.find(m);
            if (mit == w.markets.end()) continue;
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                double sum = 0.0; int n = 0;
                for (const auto& [om, mc] : w.markets)
                    if (om != m && mc.price[r] > 0.0f) { sum += mc.price[r]; ++n; }
                if (n == 0 || !(mit->second.price[r] > 0.0f)) continue;
                blockade_ratio_sum += mit->second.price[r] / (sum / n);
                ++blockade_ratio_n;
            }
        }
    }

    g_landed_all += landed;
    int ships_end = 0;
    std::set<entity_id> fleet_owners;
    for (const auto& [sid, s] : w.ships) { ++ships_end; fleet_owners.insert(s.owner); }
    double crew_bill = 0.0, fleet_treasury = 0.0;
    for (const entity_id n : fleet_owners)
        if (const auto nit = w.nations.find(n); nit != w.nations.end())
        {
            crew_bill += nation_crew_wage_bill(w, reg, n);
            fleet_treasury += nit->second.treasury;
        }
    int food_end_n = 0; long long food_land = 0, food_end = 0, share_end = 0;
    for (const auto& [uid, f] : landed_food)
        if (const auto u = w.units.find(uid); u != w.units.end())
        { ++food_end_n; food_land += f; food_end += u->second.food_permille; share_end += u->second.lp_share_permille; }

    std::printf("seed %u trade: landed %lld convoys %.0f value, sea %lld convoys %.0f value (%.1f%%), ports %zu\n",
                seed, convoys_landed, landed, convoys_sea, landed_sea, landed > 0 ? 100.0 * landed_sea / landed : 0.0,
                ports_used.size());
    for (const auto& [n, t] : per)
    {
        const auto nit = w.nations.find(n);
        if (nit == w.nations.end() || !nit->second.is_major) continue;
        std::printf("seed %u major n%llu%s: imports %.0f sea %.1f%%, exports %.0f sea %.1f%%\n", seed,
                    static_cast<unsigned long long>(n), n == player ? " (player)" : "",
                    t.imp, t.imp > 0 ? 100.0 * t.imp_sea / t.imp : 0.0,
                    t.exp, t.exp > 0 ? 100.0 * t.exp_sea / t.exp : 0.0);
    }
    std::printf("seed %u war: at war max %d nations; ships %d -> %d, rival hires %d; raid ship-ticks %lld, "
                "blockade ship-ticks %lld; raids sunk %d prize %d value %.0f (%.2f%% of sea value); "
                "ports closed max %d; blockaded price / elsewhere %.3f over %lld readings; "
                "blockaded price / its own before closing %.3f, other markets over the same span %.3f, over %lld readings\n",
                seed, wars_max, ships_start, ships_end, hired, raid_ticks, blockade_ticks, raids_sunk, raids_prize,
                raid_value, landed_sea > 0 ? 100.0 * raid_value / landed_sea : 0.0, closed_max,
                blockade_ratio_n > 0 ? blockade_ratio_sum / blockade_ratio_n : 0.0, blockade_ratio_n,
                move_n > 0 ? own_move_sum / move_n : 0.0, move_n > 0 ? ctrl_move_sum / move_n : 0.0, move_n);
    std::printf("seed %u landings: %zu unit(s) landed, %d standing at the end, food %.0f -> %.0f per-mille, LP share at the end %.0f; "
                "crew wage bill %.1f a tick against treasury %.0f across %zu fleet nation(s)\n",
                seed, landed_food.size(), food_end_n, food_end_n ? double(food_land) / food_end_n : 0.0,
                food_end_n ? double(food_end) / food_end_n : 0.0,
                food_end_n ? double(share_end) / food_end_n : 0.0, crew_bill, fleet_treasury, fleet_owners.size());
    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds = k_library;
    int ticks = 40;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            seeds.clear();
            std::string s = argv[++i];
            for (std::size_t p = 0; p <= s.size();)
            {
                const std::size_t q = s.find(',', p);
                seeds.push_back(static_cast<std::uint32_t>(std::strtoul(s.substr(p, q - p).c_str(), nullptr, 10)));
                if (q == std::string::npos) break;
                p = q + 1;
            }
        }
        else if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc)
            ticks = std::atoi(argv[++i]);
    }
    for (const std::uint32_t s : seeds)
        run_seed(s, ticks);
    if (!(g_landed_all > 0.0)) { std::printf("[FAIL] the run landed no cargo\n"); ++g_fail; }
    std::printf("%s (%d failure(s))\n", g_fail ? "FAIL" : "PASS", g_fail);
    return g_fail ? 1 : 0;
}
