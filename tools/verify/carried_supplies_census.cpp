// carried_supplies_census.cpp — BL-1452 (carried supplies). A MEASUREMENT, not a gate:
// on a generated campaign world (the app's start, its 12-tick settle, then live quarters
// as market_viability steps them), what the units' supply factors and carried stocks do.
//
// Per Ben's ruling (2026-10-29: a garrison past its roads is unfed even at home), it
// counts HOME GARRISONS — units standing on their own nation's ground — that reach zero
// food at a tick end, and how many of those then STARVE TO DEATH: gone from the world
// after being seen at zero food and not in a battle at that tick end (a unit that was
// starving and then died in a battle it entered later is still counted — an upper
// bound). The settle and the 50-tick window (market_viability's) are counted apart.
//
//   build_gen/verify/carried_supplies_census.exe [--seed N] [--ticks T]
//
// Build: bash tools/verify/build_lua_harness.sh carried_supplies_census. Run from the
// repo root.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/battle_system.hpp"
#include "world/campaign_settle.hpp"
#include "world/carried_supplies.hpp"
#include "world/logistic_points.hpp"
#include "world/nation_generation.hpp"
#include "world/state_industry.hpp"
#include "world/unit_roster.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/nation_budget.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <array>
#include <set>
#include <string>
#include <vector>
#include <algorithm>

namespace {

bool at_home(const world& w, const unit_component& u)
{
    const auto it = w.tile_to_nation.find(u.position);
    return it != w.tile_to_nation.end() && it->second == u.owner;
}

void census(const world& w, const char* when)
{
    int units = 0, men = 0, est = 0, full_cover = 0, part_cover = 0, below_even = 0, no_cover = 0;
    int hungry = 0, starving = 0, home = 0, road_full = 0, road_below = 0, road_zero = 0;
    long long road = 0;
    long long food = 0, ammo = 0, equip = 0;
    for (const auto& [uid, u] : w.units)
    {
        if (u.count <= 0) continue;
        ++units;
        home += at_home(w, u) ? 1 : 0;
        men += u.count;
        est += unit_establishment(u);
        if (u.supply_factor_permille >= 1000) ++full_cover;
        else if (u.supply_factor_permille <= 0) ++no_cover;
        else ++part_cover;
        below_even += u.supply_factor_permille < 667 ? 1 : 0;
        road += u.lp_share_permille;
        road_full  += u.lp_share_permille >= 1000 ? 1 : 0;
        road_below += u.lp_share_permille < 667 ? 1 : 0;
        road_zero  += u.lp_share_permille <= 0 ? 1 : 0;
        hungry   += unit_hungry(u) ? 1 : 0;
        starving += unit_starving(u) ? 1 : 0;
        food += u.food_permille; ammo += u.ammo_permille; equip += u.equipment_permille;
    }
    const int n = units > 0 ? units : 1;
    std::printf("  %-8s units %4d (home %4d) men %7d / est %7d  cover full %4d part %4d (<667 %4d) none %4d  "
                "hungry %4d starving %4d  mean food %4lld ammo %4lld equip %4lld\n",
                when, units, home, men, est, full_cover, part_cover, below_even, no_cover, hungry,
                starving, food / n, ammo / n, equip / n);
    std::printf("  %-8s road share: full %4d  <667 %4d  zero %4d  mean %4lld\n", when, road_full,
                road_below, road_zero, road / n);
}

/// Which trigger holds the units under cover: their tile's Logistic Point share, or a
/// garrison bill the nation's military line does not pay (no weight on it, or no money).
void triggers(const world& w, const lp_tick& lp, int tick)
{
    int lp_known = 0, lp_short = 0, lp_below = 0, unpaid_line = 0, under = 0;
    for (const auto& [uid, u] : w.units)
    {
        if (u.count <= 0) continue;
        under += u.supply_factor_permille < 1000 ? 1 : 0;
        if (const auto it = lp.supply_share.find(uid); lp.resolved && it != lp.supply_share.end())
        {
            ++lp_known;
            lp_short += it->second < 1000 ? 1 : 0;
            lp_below += it->second < 667 ? 1 : 0;
        }
        const auto nit = w.nations.find(u.owner);
        const auto bit = w.nation_budgets.find(u.owner);
        const std::size_t mi = static_cast<std::size_t>(budget_priority::military_research);
        const bool line = nit != w.nations.end() && nit->second.treasury > 0.0f
                       && bit != w.nation_budgets.end() && bit->second.weights[mi] > 0.0f;
        unpaid_line += line ? 0 : 1;
    }
    std::printf("  triggers settle t%-2d: under full cover %3d | LP share known %3d, <1000 %3d, <667 %3d | "
                "owner with no paid military line %3d\n", tick, under, lp_known, lp_short, lp_below,
                unpaid_line);
}

/// One nation's Logistic Point sources at a moment: on its ground (any owner), how many
/// run, and how many are under construction, decommissioned or state-idle; plus the
/// capital tile's own source state ("none" when no source stands there).
struct nation_sources
{
    int total = 0, running = 0, building = 0, decommissioned = 0, idle = 0;
    const char* capital = "none";
    float treasury = 0.0f;
};

std::map<entity_id, nation_sources> sources_by_nation(const world& w)
{
    std::map<entity_id, nation_sources> out;
    for (const auto& [nid, nc] : w.nations) out[nid].treasury = nc.treasury;
    std::vector<entity_id> bids;
    for (const auto& [bid, b] : w.buildings) bids.push_back(bid);
    std::sort(bids.begin(), bids.end());
    for (const entity_id bid : bids)
    {
        const building_component& b = w.buildings.at(bid);
        lp_network net;
        if (!lp_source_network(b.type, net))
            continue;
        const auto own = w.tile_to_nation.find(b.tile);
        if (own == w.tile_to_nation.end())
            continue;
        nation_sources& ns = out[own->second];
        ++ns.total;
        const char* st = "running";
        if (b.ticks_remaining > 0)  { ++ns.building; st = "building"; }
        else if (b.decommissioned)  { ++ns.decommissioned; st = "decommissioned"; }
        else if (b.state_idle)      { ++ns.idle; st = "state_idle"; }
        else                         ++ns.running;
        const auto nit = w.nations.find(own->second);
        if (nit != w.nations.end() && nit->second.capital_tile == b.tile)
            ns.capital = st;
    }
    return out;
}

/// Every nation's treasury after each settle lap: lap_treasury[tick][lap][nation].
struct lap_tap
{
    std::vector<std::array<std::map<entity_id, float>, k_campaign_settle_lap_count + 1>> ticks;
    /// The national budget pass's per-line spend, summed over the settle, per nation.
    std::map<entity_id, std::array<float, priority_count>> budget_lines;
};

void tap_after_lap(const world& w, int lap, void* ctx)
{
    auto* t = static_cast<lap_tap*>(ctx);
    if (t->ticks.empty()) return;
    auto& row = t->ticks.back()[static_cast<std::size_t>(lap) + 1];
    for (const auto& [nid, nc] : w.nations) row[nid] = nc.treasury;
}

/// Zero-food and starved-to-death tracking over one phase.
struct tracker
{
    std::set<entity_id> home_zero, any_zero, starved_home, starved_any, watch, watch_home;
    std::set<entity_id> zero_drawable; ///< seen at zero food on a tile the 8-tile draw reaches

    void observe(world& w)
    {
        unit_draw_memo dm;
        for (auto it = watch.begin(); it != watch.end();)
        {
            if (w.units.count(*it) == 0)
            {
                starved_any.insert(*it);
                if (watch_home.count(*it)) starved_home.insert(*it);
                it = watch.erase(it);
            }
            else
                ++it;
        }
        const std::set<entity_id> fighting = units_in_live_battles(w);
        for (const auto& [uid, u] : w.units)
        {
            if (u.count <= 0 || !unit_starving(u)) continue;
            if (any_zero.insert(uid).second && unit_tile_drawable(w, dm, u.owner, u.position))
                zero_drawable.insert(uid);
            const bool home = at_home(w, u);
            if (home) home_zero.insert(uid);
            if (fighting.count(uid) == 0)
            {
                watch.insert(uid);
                if (home) watch_home.insert(uid);
            }
        }
    }

    void print(std::uint32_t seed, const char* phase) const
    {
        std::size_t starved_drawable = 0;
        for (const entity_id u : starved_any) starved_drawable += zero_drawable.count(u);
        std::printf("  STARVATION seed %u %-7s: home garrisons at zero food %3zu (any unit %3zu); "
                    "starved to death: home %3zu (any %3zu) | 8-TILE READING: of those at zero food "
                    "%3zu stood within the draw, of the starved %3zu -> starved past the draw %3zu\n",
                    seed, phase, home_zero.size(), any_zero.size(), starved_home.size(),
                    starved_any.size(), zero_drawable.size(), starved_drawable,
                    starved_any.size() - starved_drawable);
    }
};

} // namespace

int main(int argc, char** argv)
{
    std::uint32_t seed = 0;
    int ticks = 50;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) seed = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) ticks = std::atoi(argv[++i]);
    }
    std::printf("=== carried supplies census (BL-1452), seed %u, %d live ticks ===\n", seed, ticks);

    lua_state lua;
    world_params p;
    p.seed = seed;
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, p, *start);
    world& w = start->w;
    census(w, "start");
    std::map<entity_id, entity_id> posted; // unit -> the tile it opened on
    for (const auto& [uid, u] : w.units) posted[uid] = u.position;
    const auto moved_report = [&](const char* when) {
        // Of the units at a zero road share: how many stand off the tile they opened on
        // (a front or a march took them there), how many on a front, and how many still
        // on their opening tile (the network stopped covering it).
        int zero = 0, off_post = 0, on_front = 0, on_post = 0, post_now_covered = 0, drawable = 0;
        lp_reach_memo memo;
        unit_draw_memo dm;
        for (const auto& [uid, u] : w.units)
        {
            if (u.count <= 0 || u.lp_share_permille > 0) continue;
            ++zero;
            const auto it = posted.find(uid);
            const bool moved = it == posted.end() || it->second != u.position;
            off_post += moved ? 1 : 0;
            on_post  += moved ? 0 : 1;
            on_front += u.front != null_entity ? 1 : 0;
            if (!moved && lp_coverage_at(w, memo, u.position) >= 1.0f) ++post_now_covered;
            if (unit_tile_drawable(w, dm, u.owner, u.position)) ++drawable;
        }
        std::printf("  %-8s zero road share %3d: off their opening tile %3d (on a front %3d), on it %3d "
                    "(of which its tile now reads covered %3d); within the 8-tile draw %3d\n",
                    when, zero, off_post, on_front, on_post, post_now_covered, drawable);
    };

    // The settle, a tick at a time, as run_app_validation_settle steps it.
    tracker settle;
    // Each nation's two standing bills as generation hands it over: the garrison's wage
    // (the credit half of every unit's upkeep, resolve_unit_upkeep) and the state's first
    // tick (state_first_tick_bill: its buildings, nodes among them).
    std::map<entity_id, std::pair<float, float>> bills;
    std::map<entity_id, int> heads;
    for (const auto& [nid, nc] : w.nations)
        bills[nid].second = state_first_tick_bill(w, start->reg, nid);
    for (const auto& [uid, u] : w.units)
        if (w.nations.count(u.owner) != 0)
        {
            bills[u.owner].first += resolve_unit_upkeep(u, start->reg.military().upkeep).credits;
            heads[u.owner] += u.count;
        }
    std::vector<std::map<entity_id, nation_sources>> src_ticks;
    src_ticks.push_back(sources_by_nation(w)); // as generation handed it to the settle
    {
        int n = 0, with_cap = 0, sources = 0;
        for (const auto& [nid, ns] : src_ticks[0])
        {
            ++n; sources += ns.total;
            with_cap += std::string(ns.capital) != "none" ? 1 : 0;
        }
        std::printf("  SOURCES at start: %d nations, %d sources (%.1f a nation), %d capitals holding one\n",
                    n, sources, n > 0 ? static_cast<double>(sources) / n : 0.0, with_cap);
        std::printf("  SOURCES per nation:");
        for (const auto& [nid, ns] : src_ticks[0]) std::printf(" %u:%d", static_cast<unsigned>(nid), ns.total);
        std::printf("\n");
    }
    lap_tap tap;
    for (int step = 0; step < k_app_validation_ticks; ++step)
    {
        tap.ticks.emplace_back();
        for (const auto& [nid, nc] : w.nations) tap.ticks.back()[0][nid] = nc.treasury;
        settle_tick_hooks hooks;
        hooks.after_lap = tap_after_lap;
        hooks.ctx       = &tap;
        const settle_tick_result res = run_settle_tick(w, start->reg, step, /*day_tick=*/0,
                                                       /*spectating=*/true, &hooks);
        settle.observe(w);
        src_ticks.push_back(sources_by_nation(w));
        for (const nation_budget_result& nr : res.report.national_budget.nations)
            for (std::size_t i = 0; i < priority_count; ++i)
                tap.budget_lines[nr.nation][i] += nr.lines[i].paid;
        if (step == 3) census(w, "settle4");
        if (step == 3 || step == k_app_validation_ticks - 1)
            triggers(w, res.report.logistic_points, step + 1);
    }
    clear_research_at_handover(w);
    census(w, "handoff");
    moved_report("handoff");
    {
        // THE DARK NATIONS: a nation with garrisons none of whose ground draws at the
        // handoff. Its sources' states per settle tick (t0 = as generation left them).
        unit_draw_memo dm;
        std::set<entity_id> with_units;
        for (const auto& [uid, u] : w.units)
            if (u.count > 0 && w.nations.count(u.owner) != 0) with_units.insert(u.owner);
        for (const entity_id nid : with_units)
        {
            const nation_component& nc = w.nations.at(nid);
            bool any = false;
            for (const entity_id t : nc.tiles)
                if (unit_tile_drawable(w, dm, nid, t)) { any = true; break; }
            if (any) continue;
            int units = 0;
            for (const auto& [uid, u] : w.units) units += (u.owner == nid && u.count > 0) ? 1 : 0;
            std::printf("  DARK seed %u nation %u (%zu tiles, %d units, capital tile %u):", seed,
                        static_cast<unsigned>(nid), nc.tiles.size(), units,
                        static_cast<unsigned>(nc.capital_tile));
            for (std::size_t k = 0; k < src_ticks.size(); ++k)
            {
                const auto it = src_ticks[k].find(nid);
                const nation_sources ns = it != src_ticks[k].end() ? it->second : nation_sources{};
                std::printf(" t%zu[src %d run %d bld %d dec %d idle %d cap %s tr %.0f]", k, ns.total,
                            ns.running, ns.building, ns.decommissioned, ns.idle, ns.capital,
                            static_cast<double>(ns.treasury));
            }
            std::printf("\n");
            // Where the chest went: each settle tick's treasury change by lap (0 convoys,
            // 1 economy step + trade, 2 market clearing, 3 budget + nation step, 4 -, 5
            // standings + the handoff's posts).
            // What the nation's state still holds at the handoff, and what each costs a tick
            // (the funding gate's bound).
            std::printf("  DARK seed %u nation %u state holds:", seed, static_cast<unsigned>(nid));
            for (const entity_id bid : nc.state_assets)
                if (const auto bit = w.buildings.find(bid); bit != w.buildings.end())
                    std::printf(" type%d%s=%.1f", static_cast<int>(bit->second.type),
                                bit->second.tile == nc.capital_tile ? "@capital" : "",
                                static_cast<double>(state_building_cost_bound(w, start->reg, bit->second)));
            std::printf("\n");
            std::printf("  DARK seed %u nation %u treasury by lap:", seed, static_cast<unsigned>(nid));
            float by_lap[k_campaign_settle_lap_count] = {};
            for (std::size_t k = 0; k < tap.ticks.size(); ++k)
            {
                std::printf(" t%zu[", k + 1);
                for (int lap = 0; lap < k_campaign_settle_lap_count; ++lap)
                {
                    const auto& a = tap.ticks[k][static_cast<std::size_t>(lap)];
                    const auto& b = tap.ticks[k][static_cast<std::size_t>(lap) + 1];
                    const float d = (b.count(nid) ? b.at(nid) : 0.0f) - (a.count(nid) ? a.at(nid) : 0.0f);
                    by_lap[lap] += d;
                    std::printf("%s%.0f", lap ? " " : "", static_cast<double>(d));
                }
                std::printf("]");
            }
            std::printf(" | settle total by lap:");
            for (int lap = 0; lap < k_campaign_settle_lap_count; ++lap)
                std::printf(" L%d %.0f", lap, static_cast<double>(by_lap[lap]));
            std::printf(" | national budget paid by line (the settle):");
            float budget_total = 0.0f;
            if (const auto bl = tap.budget_lines.find(nid); bl != tap.budget_lines.end())
                for (std::size_t i = 0; i < priority_count; ++i)
                {
                    budget_total += bl->second[i];
                    if (bl->second[i] != 0.0f)
                        std::printf(" line%zu %.0f", i, static_cast<double>(bl->second[i]));
                }
            std::printf(" (total %.0f; the rest of L3 is apply_budget and the garrison bill: %.0f)"
                        " | at the opening: garrison wage %.1f a tick (%d men), state bill %.1f a tick,"
                        " treasury %.0f = %.1f ticks of both\n",
                        static_cast<double>(budget_total),
                        static_cast<double>(by_lap[3] + budget_total),
                        static_cast<double>(bills[nid].first), heads[nid],
                        static_cast<double>(bills[nid].second),
                        static_cast<double>(tap.ticks.empty() ? 0.0f : tap.ticks[0][0][nid]),
                        (bills[nid].first + bills[nid].second) > 0.0f
                            ? static_cast<double>((tap.ticks.empty() ? 0.0f : tap.ticks[0][0][nid])
                                                  / (bills[nid].first + bills[nid].second))
                            : 0.0);
        }
    }

    // The live window, a tick at a time, exactly as run_app_live_window steps it. The
    // watch carries over: a unit starving at the handoff that dies in the window
    // counts in the window.
    tracker live;
    live.watch      = settle.watch;
    live.watch_home = settle.watch_home;
    for (int k = 1; k <= ticks; ++k)
    {
        const int day = k * 90;
        advance_orbits(w, 90.0);
        advance_surveys(w, 90);
        w.current_day_tick = day;
        (void)run_settle_tick(w, start->reg, k_app_validation_ticks + (k - 1), day, /*spectating=*/false);
        live.observe(w);
        if (k == 10 || k == ticks)
        {
            char label[16];
            std::snprintf(label, sizeof(label), "t%d", k);
            census(w, label);
        }
    }
    settle.print(seed, "settle");
    live.print(seed, "t1-t50");
    return 0;
}
