// opening_supply_census.cpp -- BL-1501 (units start supplied; MILITARY.md § Nation garrisons).
// On the app's start world, settled and SEATED as the app does it, then stepped as the app
// steps it (harness_params.hpp, `run_app_live_window`'s tick state): is a nation's opening
// army FED, on the campaign's own numbers?
//
// It reads what the resolver itself grants (`lp_tick::supplies`, one row per unit), at the
// handoff and through the first live quarters, and files every unit fed less than whole
// under the cause the row names:
//
//   past   no road its network reaches within the 8-tile draw (paid_tile null, share 0)
//   cover  the tile it pays at is reached, but below full cover: the share is the cover
//   pool   full cover, and the nation's pool ran out before this unit's turn
//
// Placement answers `past` and `cover`; `pool` is the nation's network being too small for
// its army, which no choice of post can mend, so it is reported and never gated by U1.
// The gate (exit code):
//
//   U1  Scope: the garrisons generation recorded (`gen_garrisons`), still on post at the
//       handoff; a unit on a front, marching or in a battle is the front's. Every other
//       unit is counted beside them and asserted nothing.
//       a. The repost's predicate (`unit_tile_fully_fed`) agrees with the resolver's row.
//       b. No such garrison is granted less than whole by cover or the draw while its own
//          nation holds a tile, not walled for its class and with no other nation's units
//          on it, that would feed it whole. Short with no such tile is the design's
//          permitted part-fed post (MILITARY.md), and is counted as that.
//   U2  a. The share the handoff leaves (`lp_share_permille`, what the first quarter eats)
//          is the resolver's grant on the handoff's world, for every nation's unit.
//       b. No unit that stayed where the handoff left it is told more than the first live
//          quarter grants it: told whole then past the draw, told whole then pool short,
//          told 400 then granted 100. A unit that marched off that tile is left out, and
//          counted: the share it was told was for ground it left. With no live quarter to
//          read (`--ticks 0`, or no row at all) U2b fails rather than passing on nothing.
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
#include "world/placement_rules.hpp"
#include "world/spawn_seat.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
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
const char* cause_of(const lp_supply_row& r)
{
    if (r.paid_tile == null_entity)
        return "past";
    return r.cover < 1.0f - 1e-5f ? "cover" : "pool";
}

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
        const char* cause = cause_of(r);
        q.past  += cause[1] == 'a' ? 1 : 0;
        q.cover += cause[0] == 'c' ? 1 : 0;
        q.pool  += cause[0] == 'p' && cause[1] == 'o' ? 1 : 0;
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

/// The unit's row in @p lp, or null: it asked for nothing, and is whole.
const lp_supply_row* row_of(const lp_tick& lp, entity_id unit)
{
    for (const lp_supply_row& r : lp.supplies)
        if (r.unit == unit)
            return &r;
    return nullptr;
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
        const recipe_registry& reg = start->reg;

        // Where the rule posted each garrison, to name how far the fed rule moves them.
        std::map<entity_id, entity_id> rule_post;
        for (const world::generated_garrison& g : w.gen_garrisons)
            if (const auto uit = w.units.find(g.unit); uit != w.units.end())
                rule_post[g.unit] = uit->second.position;

        settle_tick_result last;
        for (int step = 0; step < k_app_validation_ticks; ++step)
            last = run_settle_tick(w, reg, step, /*day_tick=*/0, /*spectating=*/true);
        clear_research_at_handover(w);
        // The app seats after the run closes (app.cpp), before the first live tick.
        seat_player_corporation(w, seed, start->land.search.winner_score);

        int hungry = 0, starving = 0;
        food_of(w, hungry, starving);
        print_quarter("settle12", read_quarter(w, last.report.logistic_points, false, "settle12"), w.units.size(),
                      hungry, starving);

        // THE HANDOFF, read off the resolver itself: the pure read of the claims the
        // first live tick is about to make.
        const lp_tick handoff = resolve_logistic_points(w, collect_supply_claims(w, reg));
        print_quarter("handoff", read_quarter(w, handoff, list, "handoff"), w.units.size(), hungry, starving);

        std::map<entity_id, const unit_component*> units; // ascending: every nation's unit
        for (const auto& [uid, u] : w.units)
            if (u.count > 0 && w.nations.count(u.owner) != 0)
                units[uid] = &u;

        std::map<entity_id, std::set<entity_id>> nations_on; // tile -> nations whose units stand there
        for (const auto& [uid, up] : units)
        {
            (void)uid;
            nations_on[up->position].insert(up->owner);
        }

        // U1, garrisons on post. U2a, every unit.
        std::map<entity_id, int> told;
        std::map<entity_id, entity_id> handoff_tile;
        {
            unit_draw_memo dm;
            int on_post = 0, left = 0, g_whole = 0, g_pool = 0, permitted = 0, defect = 0, disagree = 0;
            int others = 0, others_short = 0, mistold = 0, told_short = 0;
            for (const auto& [uid, up] : units)
            {
                const unit_component& u = *up;
                told[uid]         = u.lp_share_permille;
                handoff_tile[uid] = u.position;
                told_short += u.lp_share_permille < 1000 ? 1 : 0;
                const lp_supply_row* r = row_of(handoff, uid);
                const int grant = r != nullptr ? r->share_permille : 1000;
                if (u.lp_share_permille != grant)
                {
                    ++mistold;
                    if (list)
                        std::printf("    U2a unit %u nation %u: told %d, the resolver grants %d\n",
                                    static_cast<unsigned>(uid), static_cast<unsigned>(u.owner),
                                    u.lp_share_permille, grant);
                }
                if (rule_post.count(uid) == 0)
                {
                    ++others;
                    others_short += grant < 1000 ? 1 : 0;
                    continue;
                }
                if (u.front != null_entity || u.order.dest != null_entity || unit_in_battle(w, uid))
                {
                    ++left;
                    continue;
                }
                ++on_post;
                const bool row_fed = r == nullptr
                    || (r->paid_tile != null_entity && r->cover >= 1.0f - 1e-5f);
                // The predicate reads the tile; a claim of zero has no row but its tile still reads.
                if (r != nullptr && unit_tile_fully_fed(w, dm, u.owner, u.position) != row_fed)
                {
                    ++disagree;
                    if (list)
                        std::printf("    U1a unit %u nation %u: the predicate and the row disagree (share %d, %s)\n",
                                    static_cast<unsigned>(uid), static_cast<unsigned>(u.owner), grant,
                                    cause_of(*r));
                }
                if (grant >= 1000)
                {
                    ++g_whole;
                    continue;
                }
                if (row_fed)
                {
                    ++g_pool; // the network is too small; no post mends it
                    continue;
                }
                // Short by placement: permitted only where nothing of its nation's feeds it whole.
                const std::function<bool(const tile_component&)> walled = class_wall(reg.military(), u.type);
                bool open_ground = false;
                const nation_component& nc = w.nations.at(u.owner);
                for (const entity_id t : nc.tiles)
                {
                    const auto tit = w.tiles.find(t);
                    if (tit == w.tiles.end() || placement_rules::is_water_tile(tit->second.substrate))
                        continue;
                    const auto own = w.tile_to_nation.find(t);
                    if (own == w.tile_to_nation.end() || own->second != u.owner)
                        continue;
                    if (walled && walled(tit->second))
                        continue;
                    const auto on = nations_on.find(t);
                    bool foreign = false;
                    if (on != nations_on.end())
                        for (const entity_id who : on->second)
                            foreign = foreign || who != u.owner;
                    if (!foreign && unit_tile_fully_fed(w, dm, u.owner, t))
                    {
                        open_ground = true;
                        break;
                    }
                }
                ++(open_ground ? defect : permitted);
                if (open_ground && list)
                    std::printf("    U1b unit %u nation %u share %d: %s, on ground that could feed it whole\n",
                                static_cast<unsigned>(uid), static_cast<unsigned>(u.owner), grant, cause_of(*r));
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
            std::printf("  HANDOFF garrisons: %d on post (%d on a front, marching or fighting, left out): whole %d, "
                        "pool short %d, permitted part fed %d, DEFECT %d; predicate vs row disagree %d | other "
                        "units %d, %d short (asserted nothing) | told short %d, mistold %d | off their rule "
                        "post: %d of %zu, mean %.1f grid steps\n",
                        on_post, left, g_whole, g_pool, permitted, defect, disagree, others, others_short,
                        told_short, mistold, moved, rule_post.size(),
                        moved > 0 ? static_cast<double>(moved_steps) / moved : 0.0);
            check(on_post > 0 && disagree == 0, seed, "U1a the repost's predicate is the resolver's row");
            check(on_post > 0 && defect == 0, seed,
                  "U1b no garrison on post is short by placement where its nation could feed it whole");
            check(!told.empty() && mistold == 0, seed, "U2a the handoff's share is the resolver's grant");
        }

        // THE LIVE QUARTERS: what the resolver grants, and what it does to the units.
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
                if (own != w.tile_to_nation.end() && lp_source_running(b))
                    ++out[own->second];
            }
        };
        sources_running(sources_running_at_handoff);
        const std::size_t units_at_handoff = w.units.size();
        int first_rows = 0, stayed_read = 0, marched = 0, undertold = 0;
        int whole_then_past = 0, whole_then_cover = 0, whole_then_pool = 0, part_then_less = 0;
        for (int k = 1; k <= ticks; ++k)
        {
            const int day = k * 90;
            advance_orbits(w, 90.0);
            advance_surveys(w, 90);
            w.current_day_tick = day;
            const settle_tick_result res =
                run_settle_tick(w, reg, k_app_validation_ticks + (k - 1), day, /*spectating=*/false);
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
                    const auto uit = w.units.find(r.unit);
                    if (it == told.end() || uit == w.units.end())
                        continue;
                    ++first_rows;
                    if (handoff_tile.at(r.unit) != uit->second.position)
                    {
                        ++marched;
                        continue;
                    }
                    ++stayed_read;
                    if (it->second < r.share_permille)
                        ++undertold;
                    if (it->second <= r.share_permille)
                        continue;
                    const char* cause = cause_of(r);
                    if (it->second < 1000)
                        ++part_then_less;
                    else if (cause[1] == 'a')
                        ++whole_then_past;
                    else if (cause[0] == 'c')
                        ++whole_then_cover;
                    else
                        ++whole_then_pool;
                    if (list)
                        std::printf("    U2b unit %u nation %u: told %d, granted %d (%s)\n",
                                    static_cast<unsigned>(r.unit), static_cast<unsigned>(r.nation), it->second,
                                    r.share_permille, cause);
                }
        }
        std::printf("  FIRST QUARTER: %d rows, %d stayed and read, %d marched off (left out) | told whole then past "
                    "%d, then cover %d, then pool %d; told part then less %d; told less than granted %d\n",
                    first_rows, stayed_read, marched, whole_then_past, whole_then_cover, whole_then_pool,
                    part_then_less, undertold);
        std::printf("  LIVE: units %zu at the handoff, %zu after %d quarters\n", units_at_handoff, w.units.size(),
                    ticks);
        check(stayed_read > 0 && whole_then_past + whole_then_cover + whole_then_pool + part_then_less == 0, seed,
              "U2b no unit that stayed is told more than the first live quarter grants it");
    }
    std::printf("\n%s opening supply census: %d check(s) failed\n", g_failed == 0 ? "PASS" : "FAIL", g_failed);
    return g_failed == 0 ? 0 : 1;
}
