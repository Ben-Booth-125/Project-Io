// space_launch_census.cpp - BL-1637 (launch boost). A MEASUREMENT, not a gate: on a generated
// campaign world (the app's start, its settle, the seat, then live months as the app steps
// them), when do the RIVALS launch, and what do their pads hold?
//
// Per seed it prints, every five years and at the end: staffed Launchpads (rival / player),
// pads that have banked a home-orbit satellite's boost (75), the mean boost a staffed pad
// holds, pads at their cap, and the rival launches so far (assets a rival nation owns, by
// class). Then the year of the first rival launch and the launches by 2000 and by the last
// year. BL-1630 / BL-1637's done-line ("a spectated run shows rival launches before 2000,
// without pooling markets") is READ off this, never asserted: the bound depends on trade,
// and a world may legitimately launch late.
//
// The one invariant asserted: --replay runs a seed twice and the state hashes match.
//
// --stage-space: a STAGED reading, not the shipped game. At the handover every rival nation
// is handed each Digitisation node that carries a near-space key (Rocketry, the satellite
// roles, astronaut training, the modules) with its requirement chain, one side of each
// pick-one pair. It answers what boost does once rivals reach space, where the shipped
// research pace may not reach Rocketry inside the run at all.
//
// The calendar: tick 0 is January of the epoch year (1960), a tick is a month (BL-1642).
//
//   build_gen/verify/space_launch_census.exe [--seeds a,b,c] [--year Y] [--replay] [--stage-space]
//
// Build: cmd //c tools\verify\build_lua_harness.bat space_launch_census. Run from the repo root.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/campaign_settle.hpp"
#include "world/corp_ai.hpp"
#include "world/resource_names.hpp"
#include "world/digitisation_tree_data.hpp"
#include "world/market_clearing.hpp"
#include "world/nation_tech.hpp"
#include "world/space_tech_keys.hpp"
#include "world/orbital_system.hpp"
#include "world/space_assets.hpp"
#include "world/spawn_seat.hpp"
#include "world/survey_system.hpp"
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

constexpr int k_epoch_year = 1960;

int year_of(int tick) { return k_epoch_year + tick / k_econ_ticks_per_year; }

struct pad_read
{
    int staffed_rival = 0, staffed_player = 0, banked_one = 0, full = 0;
    int rocketry = 0, building = 0; ///< nations holding Rocketry; pads still under construction
    int most_nodes = 0;             ///< Digitisation nodes the leading nation holds
    int no_market = 0;              ///< staffed pads whose tile resolves to no market
    int sc_stocked = 0, pr_stocked = 0;   ///< staffed pads whose market holds the good
    int sc_admitted = 0, pr_admitted = 0; ///< ... at a price under the fair-price ceiling
    double sc_units = 0.0, pr_units = 0.0; ///< summed over those pads' markets (a shared market counts each time)
    double boost_sum = 0.0;
};

pad_read read_pads(const world& w, const space_asset_params& sp, float res_mult, entity_id player)
{
    pad_read r;
    for (const auto& [nid, n] : w.nations)
    {
        if (nation_has_mechanic(w, nid, space_keys::dispatch))
            ++r.rocketry;
        const auto tit = w.nation_tech.find(nid);
        if (tit != w.nation_tech.end())
            r.most_nodes = std::max(r.most_nodes, static_cast<int>(tit->second.held.count()));
        for (const entity_id bid : n.state_assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            const building_component& b = bit->second;
            if (b.type == building_type::launchpad && b.ticks_remaining > 0 && !b.decommissioned)
                ++r.building;
            if (b.type != building_type::launchpad || b.ticks_remaining > 0 || b.decommissioned
                || !(b.workforce_assigned > 0.0f))
                continue;
            (nid == player ? r.staffed_player : r.staffed_rival) += 1;
            {
                constexpr std::size_t SC = static_cast<std::size_t>(resource_type::spacecraft_components);
                constexpr std::size_t PR = static_cast<std::size_t>(resource_type::propellant);
                const auto mit = w.markets.find(market_for_tile(w, b.tile));
                if (mit == w.markets.end())
                    ++r.no_market;
                else
                {
                    const market_component& m = mit->second;
                    r.sc_units += m.inventory[SC];
                    r.pr_units += m.inventory[PR];
                    r.sc_stocked += m.inventory[SC] > 0.0f ? 1 : 0;
                    r.pr_stocked += m.inventory[PR] > 0.0f ? 1 : 0;
                    r.sc_admitted += shelf_admits(m, SC, res_mult, true) ? 1 : 0;
                    r.pr_admitted += shelf_admits(m, PR, res_mult, true) ? 1 : 0;
                }
            }
            r.boost_sum += b.boost;
            if (b.boost >= 75.0f)
                ++r.banked_one;
            if (b.boost >= sp.boost_cap)
                ++r.full;
        }
    }
    return r;
}

struct seed_result
{
    std::uint32_t seed = 0;
    int first_rival_launch_year = 0;
    int by_2000 = 0, by_end = 0;
    std::uint64_t hash = 0;
};

/// Hold @p n and, for each requirement group, its first member not excluded by a held
/// sibling - recursively.
void hold_with_chain(nation_tech_state& t, int n)
{
    if (n < 0 || t.held.test(static_cast<std::size_t>(n)))
        return;
    const io::dg::node& nd = io::dg::nodes[static_cast<std::size_t>(n)];
    if (nd.excludes >= 0 && t.held.test(static_cast<std::size_t>(nd.excludes)))
        return; // the pair's other side is held
    t.held.set(static_cast<std::size_t>(n));
    for (int g = 0; g < nd.group_count; ++g)
    {
        bool met = false;
        for (const std::int16_t m : nd.groups[static_cast<std::size_t>(g)])
            met = met || (m >= 0 && t.held.test(static_cast<std::size_t>(m)));
        for (const std::int16_t m : nd.groups[static_cast<std::size_t>(g)])
            if (!met && m >= 0)
            {
                hold_with_chain(t, m);
                met = t.held.test(static_cast<std::size_t>(m));
            }
    }
}

seed_result run_seed(std::uint32_t seed, int last_year, bool quiet, bool stage_space)
{
    seed_result res;
    res.seed = seed;
    lua_state lua;
    world_params wp;
    wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, wp, *start);
    world& w = start->w;
    const recipe_registry& reg = start->reg;
    const space_asset_params& sp = reg.space_assets();

    for (int step = 0; step < k_campaign_settle_ticks; ++step)
        (void)run_settle_tick(w, reg, step, /*day_tick=*/0, /*spectating=*/true);
    clear_research_at_handover(w);
    nation_build_tally_log().clear(); // NR-1229: count the play window only
    seat_player_corporation(w, seed, start->land.search.winner_score);
    const entity_id player = player_nation(w);
    if (stage_space)
        for (const auto& [nid, n] : w.nations)
        {
            if (nid == player)
                continue;
            nation_tech_state& t = w.nation_tech[nid];
            for (int i = 0; i < io::dg::node_count; ++i)
                for (const std::string_view k : space_keys::near_space)
                    if (node_has_mechanic(i, k))
                        hold_with_chain(t, i);
        }

    std::set<entity_id> seen;
    for (const auto& [aid, a] : w.space_assets)
        seen.insert(aid);
    std::map<int, int> by_class;
    int rival_launches = 0;
    const int last_tick = (last_year - k_epoch_year + 1) * k_econ_ticks_per_year - 1;
    for (int step = k_campaign_settle_ticks, k = 1; step <= last_tick; ++step, ++k)
    {
        advance_orbits(w, static_cast<double>(k_econ_tick_days));
        advance_surveys(w, k_econ_tick_days);
        w.current_day_tick = k * k_econ_tick_days;
        (void)run_settle_tick(w, reg, step, k * k_econ_tick_days, /*spectating=*/false);
        for (const auto& [aid, a] : w.space_assets)
        {
            if (!seen.insert(aid).second || a.owner == player)
                continue;
            ++rival_launches;
            ++by_class[static_cast<int>(a.cls)];
            if (res.first_rival_launch_year == 0)
                res.first_rival_launch_year = year_of(step);
            if (year_of(step) < 2000)
                ++res.by_2000;
        }
        const bool year_end = (step + 1) % k_econ_ticks_per_year == 0;
        if (!quiet && year_end && (year_of(step) % 5 == 0 || step == last_tick))
        {
            const pad_read pr = read_pads(w, sp, reg.price_band().reservation_mult, player);
            const int staffed = pr.staffed_rival + pr.staffed_player;
            std::printf("  seed %u %d: most nodes %d; Rocketry %d nations; pads building %d; staffed pads rival %d player %d; "
                        "banked >= 75: %d; at cap: %d; mean boost %.0f; rival launches %d\n",
                        seed, year_of(step), pr.most_nodes, pr.rocketry, pr.building, pr.staffed_rival, pr.staffed_player,
                        pr.banked_one, pr.full,
                        staffed > 0 ? pr.boost_sum / staffed : 0.0, rival_launches);
            std::printf("      pad markets: none %d; components stocked %d (%.0f units) admitted %d; "
                        "propellant stocked %d (%.0f units) admitted %d\n",
                        pr.no_market, pr.sc_stocked, pr.sc_units, pr.sc_admitted,
                        pr.pr_stocked, pr.pr_units, pr.pr_admitted);
            // Where components come from at all (NR-1229): the plants whose recipe makes
            // them - built, standing, staffed and not idled - and the world's shelves.
            {
                constexpr std::size_t SC = static_cast<std::size_t>(resource_type::spacecraft_components);
                int plants = 0, standing = 0, staffed_p = 0, markets_holding = 0;
                double units = 0.0;
                for (const auto& [bid, b] : w.buildings)
                {
                    if (b.type != building_type::processing_facility || b.decommissioned)
                        continue;
                    const recipe* rc = reg.get_recipe(b.recipe);
                    if (rc == nullptr || !(rc->outputs[SC] > 0.0f))
                        continue;
                    ++plants;
                    if (b.ticks_remaining > 0)
                        continue;
                    ++standing;
                    if (b.workforce_assigned > 0.0f && !b.state_idle)
                        ++staffed_p;
                }
                for (const auto& [mid, m] : w.markets)
                    if (m.inventory[SC] > 0.0f)
                    {
                        ++markets_holding;
                        units += m.inventory[SC];
                    }
                std::printf("      component plants: %d (%d standing, %d staffed); world shelves %.0f units on %d market(s)\n",
                            plants, standing, staffed_p, units, markets_holding);
                // The scorer's walk over component methods since the handover, summed over
                // every nation (corp_ai.cpp's NR-1229 trace; cumulative).
                nation_build_tally t{};
                std::map<int, int> shorts;
                for (const auto& [nid, bt] : nation_build_tally_log())
                {
                    t.sc_visit += bt.sc_visit; t.sc_locked += bt.sc_locked; t.sc_air += bt.sc_air;
                    t.sc_stacked += bt.sc_stacked; t.sc_supply += bt.sc_supply; t.sc_ceiling += bt.sc_ceiling;
                    t.sc_arrive += bt.sc_arrive; t.sc_nodata += bt.sc_nodata; t.sc_veto += bt.sc_veto;
                    t.sc_best += bt.sc_best; t.sc_placement += bt.sc_placement; t.sc_net += bt.sc_net;
                    t.sc_materials += bt.sc_materials; t.sc_cand += bt.sc_cand;
                    for (const auto& [r, n] : bt.sc_short)
                        shorts[r] += n;
                }
                std::printf("      component walk: visits %d locked %d air %d stacked %d | supply %d ceiling %d arrive %d"
                            " | nodata %d veto %d best %d | placement %d net %d materials %d cand %d | short:",
                            t.sc_visit, t.sc_locked, t.sc_air, t.sc_stacked, t.sc_supply, t.sc_ceiling, t.sc_arrive,
                            t.sc_nodata, t.sc_veto, t.sc_best, t.sc_placement, t.sc_net, t.sc_materials, t.sc_cand);
                for (const auto& [r, n] : shorts)
                    std::printf(" %s %d", resource_names::name_of(static_cast<resource_type>(r)).c_str(), n);
                std::printf("\n");
            }
        }
    }
    res.by_end = rival_launches;
    res.hash = w.state_hash(0);
    if (!quiet)
    {
        std::printf("  seed %u: first rival launch %d; by 2000 %d; by %d %d; by class",
                    seed, res.first_rival_launch_year, res.by_2000, last_year, res.by_end);
        for (const auto& [c, n] : by_class)
            std::printf(" [%d]=%d", c, n);
        std::printf("\n");
    }
    return res;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds{0};
    int last_year = 2015;
    bool replay = false, stage_space = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            seeds.clear();
            std::string s = argv[++i];
            std::size_t p = 0;
            while (p <= s.size())
            {
                const std::size_t q = s.find(',', p);
                seeds.push_back(static_cast<std::uint32_t>(std::strtoul(s.substr(p, q - p).c_str(), nullptr, 10)));
                if (q == std::string::npos)
                    break;
                p = q + 1;
            }
        }
        else if (std::strcmp(argv[i], "--year") == 0 && i + 1 < argc)
            last_year = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--replay") == 0)
            replay = true;
        else if (std::strcmp(argv[i], "--stage-space") == 0)
            stage_space = true;
    }
    int fails = 0;
    for (const std::uint32_t seed : seeds)
    {
        const seed_result a = run_seed(seed, last_year, false, stage_space);
        if (replay)
        {
            const seed_result b = run_seed(seed, last_year, true, stage_space);
            const bool same = a.hash == b.hash && a.by_end == b.by_end;
            std::printf("  %s  seed %u: replay matches (hash %016llx)\n", same ? "PASS" : "FAIL", seed,
                        static_cast<unsigned long long>(a.hash));
            if (!same)
                ++fails;
        }
    }
    std::printf("space_launch_census: %s\n", fails == 0 ? "done" : "REPLAY FAILED");
    return fails == 0 ? 0 : 1;
}
