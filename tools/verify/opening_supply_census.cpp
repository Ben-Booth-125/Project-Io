// opening_supply_census.cpp -- BL-1501 (units start supplied; MILITARY.md § Nation garrisons).
// On the app's start world: is a nation's opening army FED, on the campaign's own numbers?
//
// It reads what the resolver itself granted (`lp_tick::supplies`, one row per unit) at the
// handoff and through the first live quarters, and files every unit fed less than whole
// under the cause the row names:
//
//   past   no road its network reaches within the 8-tile draw (paid_tile null, share 0)
//   cover  the tile it pays at is reached, but below full cover: the share is the cover
//   pool   full cover, and the nation's pool ran out before this unit's turn
//
// Placement answers `past` and `cover`; `pool` is the nation's network being too small for
// its army, which no choice of post can mend. The gate (exit code) is the placement half:
//
//   U1  every garrison on its post at the handoff pays at full cover (`unit_tile_fully_fed`)
//   U2  the handoff's `lp_share_permille` is what the first quarter's resolver grants, for
//       every unit the pool could serve: no unit is told it is whole and then starved
//
//   build_gen/verify/opening_supply_census.exe [--seeds a,b,c] [--ticks T] [--list]
//
// Build: bash tools/verify/build_lua_harness.sh opening_supply_census. Run from the repo root.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/battle_system.hpp"
#include "world/campaign_settle.hpp"
#include "world/carried_supplies.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/logistic_points.hpp"
#include "world/nation_generation.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace {

int g_failed = 0;

void check(bool ok, unsigned seed, const char* what)
{
    std::printf("  %s  seed %u: %s\n", ok ? "PASS" : "FAIL", seed, what);
    if (!ok) ++g_failed;
}

struct quarter
{
    int rows = 0, whole = 0, past = 0, cover = 0, pool = 0, zero = 0;
    long long share = 0;
};

/// A row's cause, the same three-way the header names. A request of zero is whole.
quarter read_quarter(const world& w, const lp_tick& lp, bool list, const char* when)
{
    quarter q;
    for (const lp_supply_row& r : lp.supplies)
    {
        if (w.units.count(r.unit) == 0)
            continue; // a wing's refill: not a unit's food
        ++q.rows;
        q.share += r.share_permille;
        q.zero += r.share_permille <= 0 ? 1 : 0;
        if (r.share_permille >= 1000)
        {
            ++q.whole;
            continue;
        }
        const char* cause = "pool";
        if (r.paid_tile == null_entity)
        {
            ++q.past;
            cause = "past";
        }
        else if (r.cover < 1.0f - 1e-5f)
        {
            ++q.cover;
            cause = "cover";
        }
        else
            ++q.pool;
        if (list)
            std::printf("    %s unit %u nation %u share %d: %s (cover %.3f, %d steps, need %.1f, granted %.1f)\n",
                        when, static_cast<unsigned>(r.unit), static_cast<unsigned>(r.nation), r.share_permille,
                        cause, static_cast<double>(r.cover), r.draw_steps, static_cast<double>(r.need),
                        static_cast<double>(r.granted));
    }
    return q;
}

void print_quarter(const char* when, const quarter& q, std::size_t units, int hungry, int starving)
{
    std::printf("  %-8s units %4zu rows %4d: whole %4d | short by: past the draw %3d, cover %3d, pool %3d "
                "| zero share %3d, mean share %4lld | hungry %3d starving %3d\n",
                when, units, q.rows, q.whole, q.past, q.cover, q.pool, q.zero,
                q.rows > 0 ? q.share / q.rows : 0, hungry, starving);
}

void food_of(const world& w, int& hungry, int& starving)
{
    hungry = starving = 0;
    for (const auto& [uid, u] : w.units)
    {
        (void)uid;
        if (u.count <= 0)
            continue;
        hungry   += unit_hungry(u) ? 1 : 0;
        starving += unit_starving(u) ? 1 : 0;
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    int ticks = 8;
    bool list = false;
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
        else if (std::strcmp(argv[i], "--list") == 0)
            list = true;
    }
    if (seeds.empty())
        seeds = {0u};

    lua_state lua;
    for (const std::uint32_t seed : seeds)
    {
        std::printf("=== opening supply census (BL-1501), seed %u, %d live quarters ===\n", seed, ticks);
        world_params p;
        p.seed = seed;
        auto start = std::make_unique<app_start_world>();
        build_app_start_world(lua, p, *start);
        world& w = start->w;

        // Where the rule posted each garrison, to name how far the fed rule moves them.
        std::map<entity_id, entity_id> rule_post;
        for (const world::generated_garrison& g : w.gen_garrisons)
            if (const auto uit = w.units.find(g.unit); uit != w.units.end())
                rule_post[g.unit] = uit->second.position;

        settle_tick_result last;
        for (int step = 0; step < k_app_validation_ticks; ++step)
            last = run_settle_tick(w, start->reg, step, /*day_tick=*/0, /*spectating=*/true);
        clear_research_at_handover(w);

        int hungry = 0, starving = 0;
        food_of(w, hungry, starving);
        print_quarter("settle12", read_quarter(w, last.report.logistic_points, false, "settle12"), w.units.size(),
                      hungry, starving);

        // THE HANDOFF: every garrison on its post stands where it is fed whole.
        std::map<entity_id, int> told;
        {
            unit_draw_memo dm;
            int on_post = 0, undrawable = 0, part_fed = 0, told_whole = 0, told_short = 0;
            std::map<entity_id, const unit_component*> units; // ascending: every unit, raised or posted
            for (const auto& [uid, u] : w.units)
                units[uid] = &u;
            for (const auto& [uid, up] : units)
            {
                const unit_component& u = *up;
                if (u.count <= 0 || w.nations.count(u.owner) == 0)
                    continue;
                told[uid] = u.lp_share_permille;
                told_whole += u.lp_share_permille >= 1000 ? 1 : 0;
                told_short += u.lp_share_permille < 1000 ? 1 : 0;
                if (u.front != null_entity || u.order.dest != null_entity || unit_in_battle(w, uid))
                    continue;
                ++on_post;
                undrawable += unit_tile_drawable(w, dm, u.owner, u.position) ? 0 : 1;
                const bool fed = unit_tile_fully_fed(w, dm, u.owner, u.position);
                part_fed += fed ? 0 : 1;
                if (!fed && list)
                    std::printf("    HANDOFF unit %u nation %u share %d: not at full cover\n",
                                static_cast<unsigned>(uid), static_cast<unsigned>(u.owner),
                                u.lp_share_permille);
            }
            int moved = 0;
            long long moved_steps = 0;
            for (const auto& [uid, from] : rule_post)
            {
                const auto uit = w.units.find(uid);
                if (uit == w.units.end() || uit->second.position == from)
                    continue;
                const tile_component& a = w.tiles.at(from);
                const tile_component& b = w.tiles.at(uit->second.position);
                ++moved;
                moved_steps += std::abs(a.grid_x - b.grid_x) + std::abs(a.grid_y - b.grid_y);
            }
            std::printf("  HANDOFF: %d units on post, %d past the draw, %d not at full cover; "
                        "share as set at the handoff: %d whole, %d short | off their rule post: %d of %zu, "
                        "mean %.1f grid steps\n",
                        on_post, undrawable, part_fed, told_whole, told_short, moved, rule_post.size(),
                        moved > 0 ? static_cast<double>(moved_steps) / moved : 0.0);
            check(part_fed == 0, seed, "U1 every garrison on its post at the handoff pays at full cover");
        }

        // THE LIVE QUARTERS: what the resolver grants, and what it does to the units.
        // Whoever is short after the first is short of a network that CHANGED under it, or
        // has marched off: so a short unit is filed by whether it still stands where the
        // handoff left it, and a nation by the sources on its own ground that stopped running.
        std::map<entity_id, entity_id> handoff_tile;
        for (const auto& [uid, u] : w.units)
            handoff_tile[uid] = u.position;
        std::map<entity_id, int> sources_running_at_handoff;
        const auto sources_running = [&](std::map<entity_id, int>& out) {
            out.clear();
            for (const auto& [bid, b] : w.buildings)
            {
                (void)bid;
                lp_network net;
                if (!lp_source_network(b.type, net))
                    continue;
                const auto own = w.tile_to_nation.find(b.tile);
                if (own != w.tile_to_nation.end() && b.ticks_remaining <= 0 && !b.decommissioned && !b.state_idle)
                    ++out[own->second];
            }
        };
        sources_running(sources_running_at_handoff);
        const std::size_t units_at_handoff = w.units.size();
        int told_whole_then_short = 0;
        for (int k = 1; k <= ticks; ++k)
        {
            const int day = k * 90;
            advance_orbits(w, 90.0);
            advance_surveys(w, 90);
            w.current_day_tick = day;
            const settle_tick_result res =
                run_settle_tick(w, start->reg, k_app_validation_ticks + (k - 1), day, /*spectating=*/false);
            char label[16];
            std::snprintf(label, sizeof(label), "t%d", k);
            const quarter q = read_quarter(w, res.report.logistic_points, list && k <= 2, label);
            food_of(w, hungry, starving);
            print_quarter(label, q, w.units.size(), hungry, starving);
            {
                std::map<entity_id, int> running;
                sources_running(running);
                int stayed = 0, moved = 0, stayed_lost_source = 0;
                for (const lp_supply_row& r : res.report.logistic_points.supplies)
                {
                    const auto uit = w.units.find(r.unit);
                    if (uit == w.units.end() || r.share_permille >= 1000)
                        continue;
                    const auto ht = handoff_tile.find(r.unit);
                    if (ht == handoff_tile.end() || ht->second != uit->second.position)
                    {
                        ++moved;
                        continue;
                    }
                    ++stayed;
                    const auto was = sources_running_at_handoff.find(r.nation);
                    const auto now = running.find(r.nation);
                    if ((was != sources_running_at_handoff.end() ? was->second : 0)
                        > (now != running.end() ? now->second : 0))
                        ++stayed_lost_source;
                }
                std::printf("           short: %3d stayed on their post (%3d of them in a nation running fewer "
                            "sources than at the handoff), %3d moved off it\n",
                            stayed, stayed_lost_source, moved);
            }
            if (k == 1)
                for (const lp_supply_row& r : res.report.logistic_points.supplies)
                {
                    const auto it = told.find(r.unit);
                    if (it != told.end() && it->second >= 1000 && r.share_permille < 1000
                        && r.paid_tile != null_entity && r.cover < 1.0f - 1e-5f)
                        ++told_whole_then_short;
                }
        }
        std::printf("  LIVE: units %zu at the handoff, %zu after %d quarters\n", units_at_handoff, w.units.size(),
                    ticks);
        check(told_whole_then_short == 0, seed,
              "U2 no garrison told it was whole at the handoff is short of cover in the first quarter");
    }
    std::printf("\n%s opening supply census: %d check(s) failed\n", g_failed == 0 ? "PASS" : "FAIL", g_failed);
    return g_failed == 0 ? 0 : 1;
}
