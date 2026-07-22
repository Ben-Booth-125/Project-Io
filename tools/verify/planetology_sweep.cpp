// ---------------------------------------------------------------------------
// Planetology preference sweep (BL-167 calibration; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// A MEASUREMENT tool over resolve_preferences(), not a pass/fail check of the
// chain itself.
//
// Ben's calls (2026-07-22): the homeworld floor stays STRICT — every homeworld
// should be recognisably Earth — and a miss is handled by REJECTING AND
// REROLLING rather than clamping, so no value is ever silently overridden. The
// wizard expresses PREFERENCES, not settings, so the player narrows a sampling
// range and the seed picks within it.
//
// That design only works if three numbers hold up, and this harness measures all
// three:
//
//   R1  ACCEPTANCE. Reroll is only free if the viable region is a decent
//       fraction of the space. Reported as attempts-to-first-hit.
//
//   R2  NO DEAD LEAN. A preference the player can express that almost never
//       yields a viable world is a lie in the UI — it reads as a choice but
//       behaves as a slow reroll. Every lean on every axis is measured.
//
//   R3  VARIETY SURVIVES THE FLOOR. A strict floor is only acceptable if it
//       still produces visibly different Earths. Reported as the p05->p95 spread
//       of each endowment across accepted worlds.
//
// Run: planetology_sweep [draws]     (default 20000)

#include "world/components.hpp"
#include "world/planetology.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

float dep(const planetology_state& s, resource_type r)
{
    return s.endowment[static_cast<std::size_t>(r)];
}

/// Resolve one campaign and return the homeworld it produced.
planetology_state roll_home(const world_preferences& pref, uint32_t seed, resolved_world& rw_out)
{
    rw_out = resolve_preferences(pref, seed);
    body_inputs home = prototype_body(1);
    home.orbit_au = rw_out.home_orbit_au;
    return run_planetology(home, rw_out.params, seed ^ prototype_body_seed(1));
}

struct stats
{
    std::vector<float> v;
    void add(float x) { v.push_back(x); }
    float pct(float q)
    {
        if (v.empty()) return 0.0f;
        std::vector<float> c = v;
        const std::size_t k = static_cast<std::size_t>(q * static_cast<float>(c.size() - 1));
        std::nth_element(c.begin(), c.begin() + static_cast<std::ptrdiff_t>(k), c.end());
        return c[k];
    }
};

void report_spread(const char* name, stats& s)
{
    if (s.v.empty()) { std::printf("    %-20s (none)\n", name); return; }
    const float lo = s.pct(0.05f), md = s.pct(0.50f), hi = s.pct(0.95f);
    std::printf("    %-20s p05 %7.2f   median %7.2f   p95 %7.2f   spread x%.2f\n",
                name, static_cast<double>(lo), static_cast<double>(md),
                static_cast<double>(hi), static_cast<double>(lo > 0.001f ? hi / lo : 0.0f));
}

const char* lean_name(lean l)
{
    switch (l) { case lean::any: return "any"; case lean::low: return "low";
                 case lean::mid: return "mid"; case lean::high: return "high"; }
    return "?";
}

// Every axis the wizard exposes, so R2 can walk them generically.
struct axis { const char* name; lean world_preferences::*field; };
const axis k_axes[] = {
    { "star",         &world_preferences::star         },
    { "world_size",   &world_preferences::world_size   },
    { "interior",     &world_preferences::interior     },
    { "metal",        &world_preferences::metal        },
    { "ocean",        &world_preferences::ocean        },
    { "oxygen_story", &world_preferences::oxygen_story },
    { "coal_basins",  &world_preferences::coal_basins  },
    { "drawdown",     &world_preferences::drawdown     },
};

} // namespace

int main(int argc, char** argv)
{
    const int draws = (argc > 1) ? std::atoi(argv[1]) : 20000;

    std::printf("=== Planetology preference sweep: %d campaigns ===\n\n", draws);

    // --- R1: acceptance with no preferences expressed --------------------------
    {
        long long total_attempts = 0;
        int gave_up = 0, worst = 0;
        stats iron, coal, oil, copper, o2, arable, ocean, temp, grav;

        for (int i = 0; i < draws; ++i)
        {
            world_preferences pref;              // every axis `any`
            resolved_world rw;
            const planetology_state s = roll_home(pref, 0xBEEF0000u + static_cast<uint32_t>(i), rw);

            total_attempts += rw.attempts;
            worst = std::max(worst, static_cast<int>(rw.attempts));
            if (rw.gave_up) ++gave_up;

            iron.add(dep(s, resource_type::iron_ore));
            coal.add(dep(s, resource_type::coal));
            oil.add(dep(s, resource_type::petroleum));
            copper.add(dep(s, resource_type::copper_ore));
            o2.add(s.o2_fraction * 100.0f);
            arable.add(s.arable_share * 100.0f);
            ocean.add(s.profile.water_fraction * 100.0f);
            temp.add(s.surface_temp_k);
            grav.add(s.v_esc_kms);
        }

        const double mean_att = static_cast<double>(total_attempts) / draws;
        std::printf("R1 ACCEPTANCE (no preferences expressed)\n");
        std::printf("    mean attempts to a viable homeworld : %.2f  (=> %.1f%% acceptance)\n",
                    mean_att, 100.0 / mean_att);
        std::printf("    worst case                          : %d attempts\n", worst);
        std::printf("    gave up (hit the cap)               : %d\n\n", gave_up);

        std::printf("R3 VARIETY across accepted homeworlds\n");
        report_spread("iron ore",   iron);
        report_spread("coal",       coal);
        report_spread("petroleum",  oil);
        report_spread("copper ore", copper);
        report_spread("oxygen %",   o2);
        report_spread("arable %",   arable);
        report_spread("ocean %",    ocean);
        report_spread("surface K",  temp);
        report_spread("escape km/s", grav);
        std::printf("\n");

        check(gave_up == 0, "R1 no campaign exhausted the reroll cap");
        check(mean_att < 4.0, "R1 a viable homeworld is found in under 4 draws on average");
        check(coal.pct(0.95f) / std::max(coal.pct(0.05f), 0.01f) > 2.5f,
              "R3 coal still varies several-fold under the strict floor");
        check(iron.pct(0.95f) / std::max(iron.pct(0.05f), 0.01f) > 1.4f,
              "R3 iron still varies under the strict floor");
    }

    // --- R2: no lean may be a dead end ----------------------------------------
    // A preference that reads as a choice but almost never yields a viable world
    // is worse than no preference at all: the player sets it and the generator
    // quietly ignores them.
    std::printf("R2 PER-LEAN COST (mean attempts; a dead lean shows up as a large number)\n");
    const int per = std::max(200, draws / 40);
    float worst_mean = 0.0f;
    const char* worst_axis = "";
    const char* worst_lean = "";

    for (const axis& a : k_axes)
    {
        std::printf("    %-14s", a.name);
        for (lean l : { lean::low, lean::mid, lean::high })
        {
            long long att = 0;
            int gave_up = 0;
            for (int i = 0; i < per; ++i)
            {
                world_preferences pref;
                pref.*(a.field) = l;
                resolved_world rw;
                roll_home(pref, 0xC0FFEE00u + static_cast<uint32_t>(i), rw);
                att += rw.attempts;
                if (rw.gave_up) ++gave_up;
            }
            const float mean = static_cast<float>(att) / per;
            if (mean > worst_mean) { worst_mean = mean; worst_axis = a.name; worst_lean = lean_name(l); }
            std::printf("  %s %5.2f%s", lean_name(l), static_cast<double>(mean),
                        gave_up ? "!" : " ");
        }
        std::printf("\n");
    }
    std::printf("\n    worst lean: %s=%s at %.2f attempts\n\n",
                worst_axis, worst_lean, static_cast<double>(worst_mean));

    check(worst_mean < 12.0f, "R2 no expressible preference costs more than 12 draws on average");

    std::printf("\n=== %s ===\n", g_fail == 0 ? "SWEEP OK" : "SWEEP PROBLEM");
    return g_fail == 0 ? 0 : 1;
}
