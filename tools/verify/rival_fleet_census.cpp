// rival_fleet_census.cpp - BL-1533 (rival fleets). A MEASUREMENT with three invariants: on a
// generated campaign world (the app's start, its settle, the seat, then live quarters as the
// app steps them), what the rivals' sea pass does by econ tick T.
//
// Per seed it prints: nations, wars declared, rival nations holding ships at the start and the
// end, ships rivals hired, rival designs set, rival ships ever given a mission (by mission),
// transports that boarded units, landings ordered, and how many ship kit items the rivals
// with NO standing port hold beyond base kit (AI_OPPONENT.md § 11, read 2026-11-08: ship kit
// is worth nothing to them). The bound (BL-1533's "measure whether rivals actually land") is
// reported, never asserted: the next-verb rule may legitimately leave a world with none.
//
//   INVARIANTS (asserted):
//     I1  no ship of the player's nation changes mission, zone, landing or aboard by any
//         hand but the player's (this harness presses nothing for it);
//     I2  no rival raid or blockade is ASSIGNED (its mission or zone changed this tick)
//         while the rival is not mobilised. A mission set in a war can outlast its peace
//         until the rival's next staggered evaluation recalls it; the sea day works a raid
//         or blockade only against a nation still hostile (is_hostile at the roll and at the
//         closed-port pass), so those lingering ship-ticks take nothing. They are reported;
//     I3  (--replay) two runs of a seed match: the world's state hash and the tables.
//
//   build_gen/verify/rival_fleet_census.exe [--seeds a,b,c] [--ticks T] [--replay]
//
// Build: bash tools/verify/build_lua_harness.sh rival_fleet_census. Run from the repo root.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/campaign_settle.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp" // no_province
#include "world/economy_system.hpp" // corp_is_mobilised
#include "world/nation_ai.hpp" // nation_land_borders
#include "world/orbital_system.hpp"
#include "world/sea_combat.hpp"
#include "world/spawn_seat.hpp"
#include "world/state_industry.hpp" // state_uncommitted_treasury
#include "world/survey_system.hpp"
#include "world/unit_designs.hpp"
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

struct seed_result
{
    std::uint32_t seed = 0;
    int nations = 0, wars = 0;
    int rivals_with_ships_start = 0, rivals_with_ships_end = 0;
    int hired = 0, designs = 0;
    std::map<int, int> assigned; ///< mission -> rival ships ever on it
    int boarded = 0, landings = 0;
    int landlocked_rivals = 0, landlocked_ship_kit = 0;
    int i1_breaks = 0, i2_breaks = 0, i2_linger = 0;
    // Where the landing chain stops, in rival-transport-ticks: lying ready at home while
    // mobilised; with a held-water hostile coast in reach; with an idle unit of its own too.
    int ready = 0, targeted = 0, with_idle = 0;
    int minor_ports = 0, minor_pool = 0; ///< minors holding a standing port; of those, any sea crew
    // Where a minor's hire stops, in minor-ticks: holding a port while mobilised; with a row it
    // fields; with twice the cheapest fielded row's price uncommitted.
    int m_mob = 0, m_rows = 0, m_cash = 0;
    std::map<int, int> m_door, m_press; ///< hire_ship_check / the full press, by corp_command_result
    std::uint64_t hash = 0;
};

struct player_ship_state
{
    sea_mission mission;
    std::vector<uint32_t> provinces;
    entity_id landing;
    std::size_t aboard;
    bool operator==(const player_ship_state& o) const
    {
        return mission == o.mission && provinces == o.provinces && landing == o.landing && aboard == o.aboard;
    }
};

seed_result run_seed(std::uint32_t seed, int last_tick)
{
    seed_result r;
    r.seed = seed;
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
    r.nations = static_cast<int>(w.nations.size());

    std::set<entity_id> known_ships, ever_boarded, ever_landing;
    std::map<entity_id, std::set<int>> ever_mission;
    std::map<entity_id, std::pair<int, std::vector<uint32_t>>> last_order; ///< ship -> mission, zone
    std::map<entity_id, player_ship_state> mine;
    {
        std::set<entity_id> owners;
        for (const auto& [sid, s] : w.ships)
        {
            known_ships.insert(sid);
            if (s.owner != player)
                owners.insert(s.owner);
            else
                mine[sid] = {s.mission, s.provinces, s.landing_tile, s.aboard.size()};
        }
        r.rivals_with_ships_start = static_cast<int>(owners.size());
    }

    constexpr int k_days = 90;
    for (int step = k_campaign_settle_ticks, k = 1; step <= last_tick; ++step, ++k)
    {
        advance_orbits(w, static_cast<double>(k_days));
        advance_surveys(w, k_days);
        w.current_day_tick = k * k_days;
        (void)run_settle_tick(w, reg, step, k * k_days, /*spectating=*/false);
        for (const auto& [sid, s] : w.ships)
        {
            if (s.owner == player)
            {
                const player_ship_state now{s.mission, s.provinces, s.landing_tile, s.aboard.size()};
                const auto it = mine.find(sid);
                if (it == mine.end())
                    continue; // the player hires nothing here; a new player ship would be a hire for it
                if (!(it->second == now) && s.landing_tile == null_entity && s.aboard.empty()
                    && s.mission == it->second.mission && s.provinces == it->second.provinces)
                    it->second = now; // a landing that came ashore clears itself: the sea day's, not a hand
                else if (!(it->second == now))
                {
                    ++r.i1_breaks;
                    it->second = now;
                }
                continue;
            }
            if (known_ships.insert(sid).second)
                ++r.hired;
            if (s.mission != sea_mission::none)
            {
                ever_mission[sid].insert(static_cast<int>(s.mission));
            }
            {
                const std::pair<int, std::vector<uint32_t>> order{static_cast<int>(s.mission), s.provinces};
                const auto lit = last_order.find(sid);
                const bool changed = lit == last_order.end() || !(lit->second == order);
                if ((s.mission == sea_mission::raid || s.mission == sea_mission::blockade)
                    && !corp_is_mobilised(w, s.owner))
                    ++(changed ? r.i2_breaks : r.i2_linger);
                last_order[sid] = order;
            }
            if (!s.aboard.empty())
                ever_boarded.insert(sid);
            if (ship_class_of(s) == ship_class::transport && corp_is_mobilised(w, s.owner) && s.muster_days <= 0
                && s.route.empty() && s.landing_tile == null_entity && s.tile == port_mouth_tile(w, s.home_port))
            {
                ++r.ready;
                const int range = reg.military().sea.cls[static_cast<std::size_t>(ship_class::transport)].range;
                if (!held_water_hostile_coast(w, reg, s.owner, range).empty())
                {
                    ++r.targeted;
                    for (const auto& [uid, u] : w.units)
                        if (u.owner == s.owner && u.count > 0 && u.front == null_entity && u.defend_province == no_province)
                        {
                            ++r.with_idle;
                            break;
                        }
                }
            }
            if (s.landing_tile != null_entity)
                ever_landing.insert(sid);
        }
        for (const auto& [nid, nc] : w.nations)
        {
            if (nc.is_major || nid == player || !corp_is_mobilised(w, nid) || own_standing_ports(w, nid).empty())
                continue;
            ++r.m_mob;
            const std::vector<uint16_t> rows = available_ship_rows(w, nid);
            if (rows.empty())
                continue;
            ++r.m_rows;
            float cheapest = 1.0e30f;
            for (const uint16_t row : rows)
                cheapest = std::min(cheapest, reg.military().sea.cls[static_cast<std::size_t>(ship_roster_table()[row].cls)].hire_cost);
            if (state_uncommitted_treasury(w, nid) < 2.0f * cheapest)
                continue;
            ++r.m_cash;
            // The door's own answer at its lowest port, for the cheapest fielded row; and the
            // full press on a copy, to see the payment legs' answer.
            corp_command_result best = corp_command_result::rejected_placement;
            for (const entity_id port : own_standing_ports(w, nid))
                for (const uint16_t row : rows)
                {
                    const corp_command_result c = hire_ship_check(w, reg, nid, w.buildings.at(port).tile, row);
                    if (c == corp_command_result::applied && best != corp_command_result::applied)
                    {
                        best = c;
                        world copy = w;
                        corp_command cmd;
                        cmd.tick = step; cmd.corp = nid; cmd.as_nation = nid;
                        cmd.verb = corp_verb::hire_ship; cmd.tile = w.buildings.at(port).tile; cmd.unit_type = row;
                        ++r.m_press[static_cast<int>(apply_corp_command(copy, reg, cmd))];
                    }
                    else if (best != corp_command_result::applied)
                        best = c;
                }
            ++r.m_door[static_cast<int>(best)];
        }
        for (const auto& [sid, s] : w.ships)
            if (s.owner == player && mine.find(sid) == mine.end())
            {
                ++r.i1_breaks; // a ship appeared for the player
                mine[sid] = {s.mission, s.provinces, s.landing_tile, s.aboard.size()};
            }
    }
    r.wars = static_cast<int>(w.wars.size() + w.war_history.size());
    for (const auto& [sid, ms] : ever_mission)
        for (const int m : ms)
            ++r.assigned[m];
    r.boarded  = static_cast<int>(ever_boarded.size());
    r.landings = static_cast<int>(ever_landing.size());
    std::set<entity_id> owners_end;
    for (const auto& [sid, s] : w.ships)
        if (s.owner != player)
            owners_end.insert(s.owner);
    r.rivals_with_ships_end = static_cast<int>(owners_end.size());
    const std::vector<ship_kit_item>& cat = ship_kit_catalogue();
    for (const auto& [nid, nc] : w.nations)
    {
        if (nid == player)
            continue;
        r.designs += nc.ship_design_kit.empty() ? 0 : 1;
        if (!own_standing_ports(w, nid).empty())
            continue;
        ++r.landlocked_rivals;
        for (uint16_t id = 0; id < cat.size(); ++id)
            if (cat[id].research_node != nullptr && ship_kit_item_researched(w, nid, id))
                ++r.landlocked_ship_kit;
    }
    for (const auto& [nid, nc] : w.nations)
        if (!nc.is_major && nid != player && !own_standing_ports(w, nid).empty())
        {
            ++r.minor_ports;
            r.minor_pool += nc.sea_recruits > 0 ? 1 : 0;
        }
    {
        // BL-1552 (force sizing): armies against income, for the read that settles the numbers.
        const auto borders = nation_land_borders(w);
        int rivals = 0, earning = 0, at_target = 0, threatened = 0, broke = 0, land = 0, air = 0, sea = 0;
        float income = 0.0f, running = 0.0f, target = 0.0f, share = 0.0f, treasury = 0.0f;
        for (const auto& [nid, nc] : w.nations)
        {
            if (nid == player)
                continue;
            ++rivals;
            const force_reading f = read_nation_force(w, reg, nid, borders);
            income += f.income; running += f.running; target += f.target; share += f.share;
            treasury += nc.treasury;
            earning += f.income > 0.0f ? 1 : 0;
            at_target += f.target > 0.0f && f.running >= f.target ? 1 : 0;
            threatened += f.threat > 1.0f ? 1 : 0;
            broke += nc.treasury <= 0.0f ? 1 : 0;
        }
        for (const auto& [uid, u] : w.units) land += u.owner != player && u.count > 0 ? 1 : 0;
        for (const muster_order& m : w.musters) land += m.owner != player ? 1 : 0;
        for (const auto& [wid, wg] : w.wings) air += wg.owner != player ? 1 : 0;
        for (const auto& [sid, s] : w.ships) sea += s.owner != player ? 1 : 0;
        int works = 0;
        const uint16_t sa = reg.recipe_id("small_arms"), av = reg.recipe_id("armoured_vehicles"), ap = reg.recipe_id("artillery_pieces");
        for (const auto& [bid, b] : w.buildings)
            works += b.recipe == sa || b.recipe == av || b.recipe == ap ? 1 : 0;
        std::printf("    seed %u forces: rivals %d, earning %d, at target %d, threat>1 %d, broke %d; sum income %.0f running %.0f target %.0f, mean share %.2f, treasury %.0f; land %d air %d sea %d; arms works %d\n",
                    seed, rivals, earning, at_target, threatened, broke, income, running, target,
                    rivals > 0 ? share / static_cast<float>(rivals) : 0.0f, treasury, land, air, sea, works);
    }
    r.hash = w.state_hash(static_cast<uint64_t>(last_tick));
    return r;
}

const char* mission_name(int m)
{
    switch (static_cast<sea_mission>(m))
    {
        case sea_mission::patrol:   return "patrol";
        case sea_mission::escort:   return "escort";
        case sea_mission::raid:     return "raid";
        case sea_mission::blockade: return "blockade";
        default:                    return "?";
    }
}

void print_row(const seed_result& r)
{
    std::printf("  seed %u: nations %d, wars %d; rivals with ships %d -> %d; hired %d, designs %d; missions",
                r.seed, r.nations, r.wars, r.rivals_with_ships_start, r.rivals_with_ships_end, r.hired, r.designs);
    if (r.assigned.empty())
        std::printf(" none");
    for (const auto& [m, n] : r.assigned)
        std::printf(" %s %d", mission_name(m), n);
    std::printf("; boarded %d, landings %d; landlocked rivals %d holding %d ship kit item(s)\n",
                r.boarded, r.landings, r.landlocked_rivals, r.landlocked_ship_kit);
    std::printf("    seed %u chain: transport-ticks ready %d, with a held-water target %d, with an idle unit %d; "
                "minors with a port %d, of them with sea crew %d\n",
                r.seed, r.ready, r.targeted, r.with_idle, r.minor_ports, r.minor_pool);
    std::printf("    seed %u minor hire: minor-ticks mobilised with a port %d, fielding a sea row %d, with 2x the cheapest price %d\n",
                r.seed, r.m_mob, r.m_rows, r.m_cash);
    for (const auto& [c, n] : r.m_door) std::printf("      door result %d: %d\n", c, n);
    for (const auto& [c, n] : r.m_press) std::printf("      press result %d: %d\n", c, n);
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    int last_tick = 40;
    bool replay = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            std::string s = argv[++i];
            std::size_t pos = 0;
            while (pos <= s.size())
            {
                const std::size_t c = s.find(',', pos);
                const std::string tok = s.substr(pos, c == std::string::npos ? std::string::npos : c - pos);
                if (!tok.empty()) seeds.push_back(static_cast<std::uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
                if (c == std::string::npos) break;
                pos = c + 1;
            }
        }
        else if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) last_tick = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--replay") == 0) replay = true;
    }
    if (seeds.empty()) seeds = k_library;

    std::printf("=== rival fleet census (BL-1533): %zu seed(s), econ ticks 0..%d ===\n", seeds.size(), last_tick);
    int failures = 0;
    int total_assigned = 0, total_landings = 0, total_hired = 0;
    for (const std::uint32_t seed : seeds)
    {
        const seed_result r = run_seed(seed, last_tick);
        print_row(r);
        for (const auto& [m, n] : r.assigned) total_assigned += n;
        total_landings += r.landings;
        total_hired += r.hired;
        const bool i1 = r.i1_breaks == 0, i2 = r.i2_breaks == 0;
        std::printf("  %s  seed %u: I1 no hand but the player's moves a player ship (%d)\n", i1 ? "PASS" : "FAIL", seed, r.i1_breaks);
        std::printf("  %s  seed %u: I2 no rival raid/blockade is assigned while not mobilised (%d; %d ship-ticks lingering to the next evaluation)\n",
                    i2 ? "PASS" : "FAIL", seed, r.i2_breaks, r.i2_linger);
        failures += (i1 ? 0 : 1) + (i2 ? 0 : 1);
        if (replay)
        {
            const seed_result again = run_seed(seed, last_tick);
            const bool same = again.hash == r.hash && again.hired == r.hired && again.assigned == r.assigned
                           && again.boarded == r.boarded && again.landings == r.landings;
            std::printf("  %s  seed %u: I3 a second run replays it (hash %016llx)\n", same ? "PASS" : "FAIL", seed,
                        static_cast<unsigned long long>(r.hash));
            failures += same ? 0 : 1;
        }
    }
    std::printf("\nRIVAL FLEET CENSUS (econ tick %d): %d hired, %d rival ship-missions, %d landing(s) ordered\n",
                last_tick, total_hired, total_assigned, total_landings);
    std::printf("%s (%d failure(s))\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
