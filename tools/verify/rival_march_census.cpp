// rival_march_census.cpp -- BL-1540, rival march supply (AI_OPPONENT.md § 11, "A rival's march may weigh
// supply", Ben 2026-11-07). A MEASUREMENT: on the app's start world, settled and seated as the app
// does it, then stepped as the app steps it, what becomes of the rivals' OPENING armies, and did
// the wars survive the scorer weighing supply?
//
// Per seed it prints one greppable ROW line at quarter 8 and one at the last quarter (tick=):
//
//   opening        every rival unit standing at the handoff, and their men
//   starved_t8     of them, those at zero food at the last quarter's end, or dead having been
//   ever_starved   of them, those at zero food at ANY quarter's end
//   hungry_t8      of them, those below full food at the last quarter's end
//   men_lost       men the ever-starved units lost, whatever took them (their share is the men's
//                  face of the same hunger; a man lost in battle in a starving unit counts too)
//   why_*          each ever-starved unit, filed once, as it stood the first quarter it read zero:
//                  on a front holding (why_hold_odds: toward a nation at odds with it, hostile
//                  either way) / on a front pushing / on no front, and by the resolver's row:
//                  past the draw, part cover, pool short
//   declared, closed, cessions      the wars
//   battles        distinct (tile, attacker, defender) that fought in the run
//   men_battle     men lost in those battles
//   occupied_new   tile-occupier pairs seen across quarter ends; occupied_max the most at once
//   foreign_max    the most rival units standing on another nation's ground at a quarter's end
//   push_max       the most PUSH orders rivals stood under at a quarter's end
//   assigned_t8    opening rival units on a front at the last quarter's end
//   player_fronts  the player's own units on a front: nothing here presses one, so must be 0
//   fronts_moved   (nation, quarter) pairs whose fronts differ from the quarter before
//   t_*            `rival_march_tally`: how often each path of the score fired. fed_push,
//                  prize_push, refused_supply, refused_battle (per front evaluation); fall_back,
//                  reweigh, assign_refused, odds_hungry, odds_hungry_no_war, liberation (per unit)
//
// The exit code gates only that last: the player's nation is never acted on.
//
//   build_gen/verify/rival_march_census.exe [--seeds a,b,c] [--ticks T]   (default T 8)
//
// Build: bash tools/verify/build_lua_harness.sh rival_march_census. Run from the repo root.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/campaign_settle.hpp"
#include "world/carried_supplies.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/fronts.hpp"
#include "world/logistic_points.hpp"
#include "world/orbital_system.hpp"
#include "world/spawn_seat.hpp"
#include "world/stance.hpp"
#include "world/survey_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    int ticks = 8;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            for (const char* p = argv[++i]; *p != '\0';)
            {
                seeds.push_back(static_cast<std::uint32_t>(std::atoi(p)));
                while (*p != '\0' && *p != ',') ++p;
                if (*p == ',') ++p;
            }
        }
        else if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc)
            ticks = std::atoi(argv[++i]);
    }
    if (seeds.empty())
        seeds = { 0u, 28u, 46u, 12u };

    int failed = 0;
    lua_state lua;
    for (const std::uint32_t seed : seeds)
    {
        world_params p;
        p.seed = seed;
        auto start = std::make_unique<app_start_world>();
        build_app_start_world(lua, p, *start);
        world& w = start->w;
        const recipe_registry& reg = start->reg;

        for (int step = 0; step < k_campaign_settle_ticks; ++step)
            (void)run_settle_tick(w, reg, step, /*day_tick=*/0, /*spectating=*/true);
        clear_research_at_handover(w);
        seat_player_corporation(w, seed, start->land.search.winner_score);
        const entity_id player = player_nation(w);

        std::map<entity_id, int> opening; // unit -> men at the handoff, ascending
        long long opening_men = 0;
        for (const auto& [uid, u] : w.units)
            if (u.count > 0 && u.owner != player && w.nations.count(u.owner) != 0)
            {
                opening[uid] = u.count;
                opening_men += u.count;
            }

        std::set<entity_id> ever;
        int why_hold = 0, why_hold_odds = 0, why_push = 0, why_none = 0, why_past = 0, why_cover = 0, why_pool = 0;
        std::set<std::tuple<entity_id, entity_id, entity_id>> battles;
        std::set<std::pair<entity_id, entity_id>> occupations;
        long long men_battle = 0;
        std::size_t occupied_max = 0;
        int foreign_max = 0, push_max = 0, player_fronts = 0;
        long long fronts_moved = 0;
        std::map<entity_id, std::vector<std::pair<entity_id, std::vector<entity_id>>>> last_fronts;
        const auto front_shape = [](const std::map<entity_id, std::vector<front>>& all) {
            std::map<entity_id, std::vector<std::pair<entity_id, std::vector<entity_id>>>> out;
            for (const auto& [n, fs] : all)
                for (const front& f : fs)
                    out[n].push_back({ f.neighbour, f.span });
            return out;
        };
        last_fronts = front_shape(derive_all_fronts(w));
        rival_march_tally tally;
        rival_march_tally_sink() = &tally;

        const auto print_row = [&](int k) {
            int starved_now = 0, hungry_now = 0, dead = 0, assigned = 0;
            long long men_lost = 0;
            for (const auto& [uid, men] : opening)
            {
                const auto uit = w.units.find(uid);
                const bool alive = uit != w.units.end() && uit->second.count > 0;
                dead += alive ? 0 : 1;
                if (ever.count(uid) != 0)
                    men_lost += men - (alive ? uit->second.count : 0);
                if (alive)
                {
                    hungry_now += unit_hungry(uit->second) ? 1 : 0;
                    assigned += uit->second.front != null_entity ? 1 : 0;
                }
                starved_now += ((alive && unit_starving(uit->second)) || (!alive && ever.count(uid) != 0)) ? 1 : 0;
            }
            std::printf("ROW seed=%u tick=%d opening=%zu opening_men=%lld starved_t8=%d ever_starved=%zu hungry_t8=%d dead=%d "
                        "men_lost=%lld why_hold=%d why_hold_odds=%d why_push=%d why_none=%d why_past=%d why_cover=%d why_pool=%d "
                        "declared=%zu closed=%zu cessions=%zu battles=%zu men_battle=%lld occupied_new=%zu "
                        "occupied_max=%zu foreign_max=%d push_max=%d assigned_t8=%d player_fronts=%d fronts_moved=%lld "
                        "t_fed_push=%lld t_prize_push=%lld t_refused_supply=%lld t_refused_battle=%lld t_fall_back=%lld "
                        "t_reweigh=%lld t_assign_refused=%lld t_odds_hungry=%lld t_odds_hungry_no_war=%lld t_liberation=%lld\n",
                        seed, k, opening.size(), opening_men, starved_now, ever.size(), hungry_now, dead, men_lost, why_hold,
                        why_hold_odds, why_push, why_none, why_past, why_cover, why_pool, w.wars.size() + w.war_history.size(),
                        w.war_history.size(), w.province_cessions.size(), battles.size(), men_battle, occupations.size(),
                        occupied_max, foreign_max, push_max, assigned, player_fronts, fronts_moved, tally.fed_push,
                        tally.prize_push, tally.refused_supply, tally.refused_battle, tally.fall_back, tally.reweigh,
                        tally.assign_refused, tally.odds_hungry, tally.odds_hungry_no_war, tally.liberation);
            std::fflush(stdout);
        };

        for (int k = 1; k <= ticks; ++k)
        {
            const int day = k * 90;
            advance_orbits(w, 90.0);
            advance_surveys(w, 90);
            w.current_day_tick = day;
            const settle_tick_result res =
                run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), day, /*spectating=*/false);

            for (const battle_dispatch& d : res.report.battle_dispatches)
            {
                battles.insert({ d.tile, d.attacker, d.defender });
                men_battle += d.attacker_men_lost + d.defender_men_lost;
            }
            for (const auto& [tile, occ] : w.tile_occupier)
                occupations.insert({ tile, occ });
            occupied_max = std::max(occupied_max, w.tile_occupier.size());

            std::map<entity_id, const lp_supply_row*> rows;
            for (const lp_supply_row& r : res.report.logistic_points.supplies)
                rows[r.unit] = &r;
            for (const auto& [uid, men] : opening)
            {
                (void)men;
                const auto uit = w.units.find(uid);
                if (uit == w.units.end() || uit->second.count <= 0 || !unit_starving(uit->second)
                    || !ever.insert(uid).second)
                    continue;
                const unit_component& u = uit->second;
                if (u.front == null_entity)
                    ++why_none;
                else if (front_order_of(w, u.owner, u.front) == front_order::push)
                    ++why_push;
                else
                {
                    ++why_hold;
                    const std::vector<front>& fs = nation_fronts(w, u.owner);
                    if (const front* f = front_named(fs, u.front); f != nullptr
                        && (is_hostile(w, u.owner, f->neighbour) || is_hostile(w, f->neighbour, u.owner)))
                        ++why_hold_odds;
                }
                const auto row = rows.find(uid);
                if (row != rows.end())
                {
                    if (row->second->paid_tile == null_entity)
                        ++why_past;
                    else if (row->second->cover < 1.0f - 1e-5f)
                        ++why_cover;
                    else
                        ++why_pool;
                }
            }

            int foreign = 0;
            player_fronts = 0;
            for (const auto& [uid, u] : w.units)
            {
                (void)uid;
                if (u.count <= 0)
                    continue;
                if (u.owner == player)
                {
                    player_fronts += u.front != null_entity ? 1 : 0;
                    continue;
                }
                const auto own = w.tile_to_nation.find(u.position);
                foreign += own != w.tile_to_nation.end() && own->second != null_entity && own->second != u.owner;
            }
            foreign_max = std::max(foreign_max, foreign);
            int pushes = 0;
            for (const auto& e : w.front_orders)
                pushes += e.nation != player && e.order == front_order::push ? 1 : 0;
            push_max = std::max(push_max, pushes);
            {
                auto now = front_shape(derive_all_fronts(w));
                for (const auto& [n, shape] : now)
                {
                    const auto was = last_fronts.find(n);
                    fronts_moved += (was == last_fronts.end() || was->second != shape) ? 1 : 0;
                }
                last_fronts = std::move(now);
            }
            if (k == 8 || k == ticks)
                print_row(k);
        }
        rival_march_tally_sink() = nullptr;
        if (player_fronts != 0)
        {
            std::printf("  FAIL  seed %u: %d of the player's units on a front nothing pressed\n", seed, player_fronts);
            ++failed;
        }
    }
    std::printf("%s rival march census: %d check(s) failed\n", failed == 0 ? "PASS" : "FAIL", failed);
    return failed == 0 ? 0 : 1;
}
