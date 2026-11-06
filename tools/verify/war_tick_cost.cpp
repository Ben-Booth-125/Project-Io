// ---------------------------------------------------------------------------
// war_tick_cost - what a live, daily-pushing war costs a tick (cold review finding 13,
// BL-1502 / BL-1505). A measurement, not a pass/fail row. Wall-clock is read here and
// nowhere near the simulation's own state.
// ---------------------------------------------------------------------------
// One seed, the campaign's opening world, copied into two arms that run the same ticks:
//   quiet  the rivals as they are;
//   war    the two neighbouring rivals with the most units are put at war through the
//          verbs, both answered, and every unit of both assigned to the shared front and
//          set to push, so the occupation layer, the front re-derive and the party lanes
//          all run every day.
//
// Run: bash tools/verify/build_lua_harness.sh war_tick_cost, then
//      build_gen/verify/war_tick_cost.exe [--seed N] [--ticks N]
// ---------------------------------------------------------------------------

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/campaign_settle.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/fronts.hpp"
#include "world/orbital_system.hpp"
#include "world/spawn_seat.hpp"
#include "world/stance.hpp"
#include "world/survey_system.hpp"
#include "world/war.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace {

struct arm_result
{
    std::vector<double> ms;
    std::size_t occupied_max = 0;
    int war_ticks = 0;
    std::size_t convoys_max = 0;
};

arm_result run_arm(world& w, const recipe_registry& reg, int first_step, int ticks, entity_id a, entity_id b)
{
    arm_result out;
    constexpr int k_days = 90;
    for (int i = 0; i < ticks; ++i)
    {
        const int step = first_step + i;
        const int k = step - k_campaign_settle_ticks + 1;
        advance_orbits(w, static_cast<double>(k_days));
        advance_surveys(w, k_days);
        w.current_day_tick = k * k_days;
        const auto t0 = std::chrono::steady_clock::now();
        (void)run_settle_tick(w, reg, step, k * k_days, /*spectating=*/false);
        const auto t1 = std::chrono::steady_clock::now();
        out.ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        out.occupied_max = std::max(out.occupied_max, w.tile_occupier.size());
        out.convoys_max  = std::max(out.convoys_max, w.convoys.size());
        if (a != null_entity && at_war(w, a, b))
            ++out.war_ticks;
    }
    return out;
}

double mean(std::vector<double> v)
{
    double s = 0.0;
    for (const double d : v) s += d;
    return v.empty() ? 0.0 : s / static_cast<double>(v.size());
}

double median(std::vector<double> v)
{
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

} // namespace

int main(int argc, char** argv)
{
    std::uint32_t seed = 0;
    int ticks = 12;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::atoi(argv[++i]);
    }
    lua_state lua;
    world_params wp;
    wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, wp, *start);
    world& base = start->w;
    const recipe_registry& reg = start->reg;
    for (int step = 0; step < k_campaign_settle_ticks; ++step)
        (void)run_settle_tick(base, reg, step, /*day_tick=*/0, /*spectating=*/true);
    clear_research_at_handover(base);
    seat_player_corporation(base, seed, start->land.search.winner_score);
    reanchor_fronts(base);

    // The pair: two rival neighbours, with the most units between them.
    const auto units_of = [&](entity_id n) {
        int c = 0;
        for (const auto& [uid, u] : base.units)
            c += u.owner == n && u.count > 0 ? 1 : 0;
        return c;
    };
    entity_id a = null_entity, b = null_entity;
    int best = -1;
    for (const auto& [n, nc] : base.nations)
    {
        if (n == base.player_entity)
            continue;
        for (const front& f : nation_fronts(base, n))
        {
            if (f.neighbour == base.player_entity || f.neighbour == null_entity || f.neighbour == n)
                continue;
            const int c = units_of(n) + units_of(f.neighbour);
            if (c > best) { best = c; a = n; b = f.neighbour; }
        }
    }
    if (a == null_entity)
    {
        std::printf("war_tick_cost: no rival pair shares a front\n");
        return 1;
    }

    auto quiet_arm = std::make_unique<world>(base);
    auto war_arm   = std::make_unique<world>(base);
    world& wa = *war_arm;
    const sentiment_params sp{};
    const auto province_of_nation = [&](entity_id n) {
        const auto& tiles = wa.nations.at(n).tiles;
        return tiles.empty() ? no_province : wa.provinces.province_of(tiles.front());
    };
    const war_result d = declare_war(wa, sp, a, b, { province_of_nation(b) });
    const war_result r = answer_war(wa, b, a, { province_of_nation(a) });
    reanchor_fronts(wa);
    int assigned = 0, pushing = 0;
    for (const auto& [side, other] : { std::make_pair(a, b), std::make_pair(b, a) })
    {
        entity_id key = null_entity;
        for (const front& f : nation_fronts(wa, side))
            if (f.neighbour == other) { key = f.key; break; }
        if (key == null_entity)
            continue;
        std::vector<entity_id> ids;
        for (const auto& [uid, u] : wa.units)
            if (u.owner == side && u.count > 0)
                ids.push_back(uid);
        std::sort(ids.begin(), ids.end());
        for (const entity_id uid : ids)
        {
            corp_command c; c.corp = side; c.as_nation = side; c.verb = corp_verb::assign_front;
            c.subject = uid; c.tile = key;
            assigned += apply_corp_command(wa, reg, c) == corp_command_result::applied ? 1 : 0;
        }
        corp_command o; o.corp = side; o.as_nation = side; o.verb = corp_verb::set_front_order;
        o.tile = key; o.order = 1;
        pushing += apply_corp_command(wa, reg, o) == corp_command_result::applied ? 1 : 0;
    }
    std::printf("pair %u v %u: declared %d answered %d; %d unit(s) assigned, %d side(s) pushing, %zu nation(s), %zu tile(s)\n",
                a, b, static_cast<int>(d), static_cast<int>(r), assigned, pushing, base.nations.size(), base.tiles.size());

    const arm_result q = run_arm(*quiet_arm, reg, k_campaign_settle_ticks, ticks, null_entity, null_entity);
    const arm_result m = run_arm(wa, reg, k_campaign_settle_ticks, ticks, a, b);
    std::printf("quiet: mean %.1f ms/tick, median %.1f; occupied at most %zu, convoys at most %zu\n",
                mean(q.ms), median(q.ms), q.occupied_max, q.convoys_max);
    std::printf("war:   mean %.1f ms/tick, median %.1f; occupied at most %zu, convoys at most %zu, the war stood %d of %d tick(s)\n",
                mean(m.ms), median(m.ms), m.occupied_max, m.convoys_max, m.war_ticks, ticks);
    std::printf("per tick (ms), quiet / war:");
    for (std::size_t i = 0; i < q.ms.size() && i < m.ms.size(); ++i)
        std::printf(" %.0f/%.0f", q.ms[i], m.ms[i]);
    std::printf("\n");
    return 0;
}
