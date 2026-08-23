// cadence_schedule — does every rival get its turn on the schedule the LIVE
// APP actually runs? (BL-568, 2026-08-23.)
//
// THE DEFECT THIS GUARDS. corp_ai's stagger is `tick % cadence_k == index %
// cadence_k` (corp_ai.cpp). It was keyed on `world::current_day_tick`, which the
// app mirrors from the sim loop each frame — and at every quarter boundary that
// value is 90n (sim_loop.hpp: econ_tick_days = 90). 90n mod 4 is only ever 0 or
// 2, so rivals at sorted index 1 or 3 (mod 4) were NEVER due in a played game,
// and during the 80-tick warm start (day tick 0) only index 0 evaluated. Every
// harness, --serve and --verify passed an econ counter 1..N and rotated all four
// slots — so the suite certified a schedule the game never ran, for weeks.
//
// WHY THE EXISTING HARNESSES CANNOT CATCH IT. They ARE the working schedule:
// each sets `current_day_tick = tick` with one tick per econ step, which is
// exactly the rotation that was correct everywhere but the app. This harness
// drives `run_economy_step` on the APP's schedule — day tick 90n, econ tick n —
// and asserts on `economy_report::corps_evaluated`, the record the strategic
// tier now leaves of who it actually looked at.
//
// Headless: no registry needed, because the due-check happens before any
// scoring and the record is written at the gate.

#include "harness_params.hpp"
#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <set>
#include <vector>

namespace
{

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

/// Sorted non-player corp ids — the set corp_ai indexes its cadence over.
std::vector<entity_id> rival_ids(const world& w)
{
    std::vector<entity_id> ids;
    for (const auto& [id, cc] : w.corporations)
        if (!cc.is_player && id != w.player_entity) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

/// Run `ticks` econ steps on the APP's schedule and return every corp the
/// strategic tier evaluated at least once.
std::set<entity_id> evaluated_on_app_schedule(world& w, const recipe_registry& reg, int ticks)
{
    std::set<entity_id> seen;
    for (int n = 0; n < ticks; ++n)
    {
        // This is the pair app::step_economy produces at a quarter boundary:
        // the day tick at 90n (mirrored each frame), the econ counter at n.
        w.current_day_tick  = 90 * n;
        w.current_econ_tick = n;
        const economy_report rep = run_economy_step(w, reg, /*spectating=*/false);
        for (const entity_id c : rep.corps_evaluated) seen.insert(c);
    }
    return seen;
}

} // namespace

int main()
{
    std::printf("cadence_schedule — every rival evaluates on the live app's schedule (BL-568)\n\n");

    world_params params;
    world        w = make_hard_coded_world(no_prehistory(params));
    recipe_registry reg;
    corp_ai_params  p;
    const int k = std::max(1, p.cadence_k);

    const std::vector<entity_id> rivals = rival_ids(w);
    std::printf("  %zu rival corps, cadence_k = %d\n", rivals.size(), k);
    check(rivals.size() >= static_cast<std::size_t>(k),
          "C0 the generated world has at least cadence_k rivals, so every slot is occupied");

    // C1 — the arithmetic the defect hid behind, stated so it cannot be unlearned.
    {
        std::set<int> day_slots;
        for (int n = 0; n < 64; ++n) day_slots.insert((90 * n) % k);
        check(static_cast<int>(day_slots.size()) < k,
              "C1 the DAY tick at quarter boundaries (90n) reaches fewer than cadence_k slots — it is not a cadence key");
        std::set<int> econ_slots;
        for (int n = 0; n < 64; ++n) econ_slots.insert(n % k);
        check(static_cast<int>(econ_slots.size()) == k,
              "C1b the ECON counter reaches every slot");
    }

    // C2 — through the real tick: k steps on the app's schedule evaluate every rival.
    {
        const std::set<entity_id> seen = evaluated_on_app_schedule(w, reg, k);
        std::size_t missing = 0;
        for (const entity_id r : rivals)
            if (!seen.count(r)) ++missing;
        std::printf("  after %d app-schedule ticks: %zu of %zu rivals evaluated, %zu never due\n",
                    k, seen.size(), rivals.size(), missing);
        check(missing == 0,
              "C2 every rival is evaluated at least once within cadence_k ticks on the APP's schedule");
        check(!seen.count(w.player_entity),
              "C2b the player corp is never evaluated on a played schedule");
    }

    // C3 — the record is per tick and exactly the due set: each tick evaluates
    // the rivals at index ≡ tick (mod k), no more, no fewer.
    {
        world w2 = make_hard_coded_world(no_prehistory(params));
        const std::vector<entity_id> r2 = rival_ids(w2);
        bool exact = true;
        for (int n = 0; n < 2 * k && exact; ++n)
        {
            w2.current_day_tick  = 90 * n;
            w2.current_econ_tick = n;
            const economy_report rep = run_economy_step(w2, reg, false);
            std::set<entity_id> due;
            for (const entity_id c : r2)
                if (corp_strategic_eval_due(w2, c, n, p)) due.insert(c);
            std::set<entity_id> got(rep.corps_evaluated.begin(), rep.corps_evaluated.end());
            if (got != due) exact = false;
        }
        check(exact, "C3 corps_evaluated equals corp_strategic_eval_due's set on every tick — the record and the gate agree");
    }

    std::printf("\n%s\n", g_failures == 0 ? "ALL PASS" : "FAILURES");
    return g_failures == 0 ? 0 : 1;
}
