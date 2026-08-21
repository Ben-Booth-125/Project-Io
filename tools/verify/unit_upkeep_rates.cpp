// ---------------------------------------------------------------------------
// Sprint C3's rider — what turning the upkeep rates off zero would actually cost
// ---------------------------------------------------------------------------
// THIS IS A REPORT, NOT A GATE, and the distinction is the whole point.
//
// BL-454 landed every `unit_upkeep_params` rate at ZERO, and `scripts/economy.lua`
// says why in as many words: "the pricing can be tuned by playtest against a
// MEASURED BASELINE rather than guessed here." Sprint C3's rider is "turn the
// upkeep rates off zero and measure the movement" — so the deliverable is the
// measurement, and picking the numbers stays Ben's.
//
// So this harness asserts almost nothing. It sweeps candidate rate sets against a
// standing force and prints what each one does to the three things that decide
// whether a rate is playable:
//
//   1. THE WAGE BILL, as a share of what the corp actually earns. A rate that
//      eats a corp's whole income is not a cost, it is a countdown.
//   2. THE GOODS DRAW, against what the economy can actually make. Ordnance is
//      the good (BL-457 added it for this consumer), and if the draw outruns
//      production the army starves however rich its owner is.
//   3. THE SUPPLY CURVE — how many ticks a unit survives out of supply, which is
//      what BL-458 (supply lines cannot be cut) is really waiting on. A decay
//      rate that takes 200 ticks to bite makes cutting a lane a gesture; one that
//      takes 3 makes every road a tripwire.
//
// The four assertions it DOES make are invariants, not tuning: shipped rates are
// still zero, a zero rate still costs nothing, the draw is linear in heads, and
// decay is bounded.
//
// Run: unit_upkeep_rates
// ---------------------------------------------------------------------------

#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;
int checks   = 0;

void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

constexpr std::size_t ORD       = static_cast<std::size_t>(resource_type::ordnance);
constexpr uint16_t    ROW_LEVY  = 0;
constexpr uint16_t    ROW_RIFLE = 15;

struct fixture
{
    world     w;
    entity_id body = null_entity, tile = null_entity, corp = null_entity, base = null_entity;
};

fixture make_fixture()
{
    fixture f;
    f.body = f.w.create_entity();
    body_component bc{};
    bc.grid_width = 1; bc.grid_height = 1;
    f.w.bodies[f.body] = bc;

    f.tile = f.w.create_entity();
    tile_component tc{};
    tc.body = f.body; tc.grid_x = 0; tc.grid_y = 0;
    tc.substrate = terrain_substrate::sedimentary;
    tc.cover = terrain_cover::grass; tc.cover_density = 150;
    tc.landform = terrain_landform::plains;
    f.w.tiles[f.tile] = tc;

    f.corp = f.w.create_entity();
    corporation_component cc{};
    cc.name = "Measured Corp"; cc.balance = 100000.0f;
    f.w.corporations[f.corp] = cc;
    f.w.player_entity = f.corp;

    f.base = f.w.create_entity();
    building_component b{};
    b.type = building_type::military_base; b.tile = f.tile; b.ticks_remaining = 0;
    f.w.buildings[f.base] = b;
    f.w.corporations[f.corp].assets.push_back(f.base);
    return f;
}

entity_id add_unit(fixture& f, uint16_t type, int count, int supply = 1000)
{
    const entity_id u = f.w.create_entity();
    unit_component uc{};
    uc.owner = f.corp; uc.position = f.tile; uc.count = count; uc.type = type;
    uc.supply_factor_permille = supply; uc.muster_base = f.base;
    f.w.units[u] = uc;
    return u;
}

recipe_registry registry_with(float credits_per_head, float credits_per_power,
                              float ordnance_per_head, int decay, int recovery)
{
    recipe_registry reg;
    military_capability_params mp = reg.military();
    mp.upkeep.credits_per_head           = credits_per_head;
    mp.upkeep.credits_per_head_per_power = credits_per_power;
    mp.upkeep.goods_per_head[ORD]        = ordnance_per_head;
    mp.upkeep.supply_decay_permille      = decay;
    mp.upkeep.supply_recovery_permille   = recovery;
    reg.set_military(mp);
    return reg;
}

/// One candidate rate set, and a plain-language reason it is on the sweep.
struct candidate
{
    const char* name;
    float credits_per_head;
    float credits_per_power;
    float ordnance_per_head;
    int   decay;
    int   recovery;
    const char* rationale;
};

/// Resolve one tick's draw for a force, without running the world — the cost
/// arithmetic alone, which is what the wage-bill column needs.
unit_upkeep_draw draw_for(const unit_component& u, const recipe_registry& reg)
{
    return resolve_unit_upkeep(u, reg.military().upkeep);
}

} // namespace

int main()
{
    std::printf("=== unit upkeep rates: what turning them on would cost (Sprint C3 rider) ===\n");

    // -----------------------------------------------------------------------
    std::printf("\nI — invariants (these ARE asserted)\n");
    {
        const recipe_registry bare;
        check(bare.military().upkeep.credits_per_head == 0.0f
                  && bare.military().upkeep.goods_per_head[ORD] == 0.0f
                  && bare.military().upkeep.supply_decay_permille == 0,
              "I1 the SHIPPED rates are still zero — this harness measures, it does not change them");

        unit_component u{}; u.type = ROW_LEVY; u.count = 100;
        const unit_upkeep_draw z = draw_for(u, bare);
        check(z.credits == 0.0f && !z.any_goods,
              "I2 at zero rates a standing force still costs exactly nothing");

        const recipe_registry on = registry_with(0.10f, 0.0f, 0.02f, 0, 0);
        unit_component a{}; a.type = ROW_LEVY; a.count = 100;
        unit_component b{}; b.type = ROW_LEVY; b.count = 200;
        const unit_upkeep_draw da = draw_for(a, on);
        const unit_upkeep_draw db = draw_for(b, on);
        check(std::fabs(db.credits - 2.0f * da.credits) < 1e-3f,
              "I3 the draw is LINEAR in heads — twice the men, twice the bill");
        check(std::fabs(db.goods[ORD] - 2.0f * da.goods[ORD]) < 1e-4f,
              "I4 ...and linear in the goods half too");
    }

    // -----------------------------------------------------------------------
    // THE SWEEP. Reported, never asserted: nobody has chosen these numbers, and
    // asserting a band around a number nobody chose would be inventing the rule
    // this harness exists to inform.
    // -----------------------------------------------------------------------
    static const candidate k_candidates[] = {
        { "shipped (all zero)", 0.00f, 0.000f, 0.000f,   0,  0,
          "The baseline every other row is read against." },
        { "token",              0.02f, 0.00002f, 0.002f,  5, 10,
          "Upkeep is a rounding error; standing armies are effectively free." },
        { "light",              0.10f, 0.0001f,  0.010f, 20, 25,
          "Noticeable but not a constraint — a corp funds a force from spare income." },
        { "real",               0.40f, 0.0004f,  0.040f, 50, 40,
          "A standing army is a budget line you feel; cutting a lane bites in ~20 ticks." },
        { "heavy",              1.00f, 0.0010f,  0.100f,120, 60,
          "Force projection is the dominant cost; an unsupplied army collapses in under 10 ticks." },
    };

    // A representative force: a cheap line and an expensive one, so the
    // power-scaled term is visible rather than folded into the flat one.
    struct force { const char* label; uint16_t type; int count; };
    static const force k_force[] = {
        { "500 Levy Spear (power_mod 0)",     ROW_LEVY,  500 },
        { "200 Rifle Regiment (power_mod 380)", ROW_RIFLE, 200 },
    };

    std::printf("\nS — the sweep (REPORTED, not asserted)\n");
    std::printf("  %-20s | %10s %10s | %9s | %6s | %s\n",
                "rate set", "credits/t", "ordnance/t", "ticks to", "recov", "rationale");
    std::printf("  %-20s | %10s %10s | %9s | %6s |\n",
                "", "(the force)", "(the force)", "collapse", "ticks");
    std::printf("  ---------------------+-----------------------+-----------+--------+\n");

    for (const candidate& c : k_candidates)
    {
        const recipe_registry reg =
            registry_with(c.credits_per_head, c.credits_per_power, c.ordnance_per_head,
                          c.decay, c.recovery);

        float credits = 0.0f, ordnance = 0.0f;
        for (const force& fo : k_force)
        {
            unit_component u{}; u.type = fo.type; u.count = fo.count;
            const unit_upkeep_draw d = draw_for(u, reg);
            credits  += d.credits;
            ordnance += d.goods[ORD];
        }

        // Ticks from fully supplied to zero at this decay rate — the number that
        // decides whether cutting a supply line is a threat or a gesture. This is
        // what BL-458 has been waiting on.
        const int to_collapse = c.decay > 0 ? (1000 + c.decay - 1) / c.decay : 0;
        // Ticks to recover from zero once the lane is restored.
        const int to_recover  = c.recovery > 0 ? (1000 + c.recovery - 1) / c.recovery : 0;

        char collapse[16];
        if (to_collapse > 0) std::snprintf(collapse, sizeof collapse, "%d", to_collapse);
        else                 std::snprintf(collapse, sizeof collapse, "never");
        char recov[16];
        if (to_recover > 0) std::snprintf(recov, sizeof recov, "%d", to_recover);
        else                std::snprintf(recov, sizeof recov, "never");

        std::printf("  %-20s | %10.2f %10.3f | %9s | %6s | %s\n",
                    c.name, static_cast<double>(credits), static_cast<double>(ordnance),
                    collapse, recov, c.rationale);
    }

    // -----------------------------------------------------------------------
    std::printf("\nS2 — the wage bill against what a corp actually earns\n");
    {
        // The bill is only meaningful as a SHARE. Quoted against a range of corp
        // incomes rather than one, because the shipped spread is wide and a rate
        // that is trivial for a large corp can be fatal for a small one — which
        // is exactly the asymmetry a flat per-head rate creates.
        static const float k_incomes[] = { 50.0f, 200.0f, 1000.0f };
        std::printf("  %-20s |", "rate set");
        for (const float inc : k_incomes) std::printf(" %8.0f/t", static_cast<double>(inc));
        std::printf("   <- corp income per tick\n");
        std::printf("  ---------------------+---------------------------------\n");
        for (const candidate& c : k_candidates)
        {
            const recipe_registry reg =
                registry_with(c.credits_per_head, c.credits_per_power, c.ordnance_per_head,
                              c.decay, c.recovery);
            float credits = 0.0f;
            for (const force& fo : k_force)
            {
                unit_component u{}; u.type = fo.type; u.count = fo.count;
                credits += draw_for(u, reg).credits;
            }
            std::printf("  %-20s |", c.name);
            for (const float inc : k_incomes)
                std::printf(" %8.1f%%", static_cast<double>(100.0f * credits / inc));
            std::printf("\n");
        }
        std::printf("  (the force is 500 Levy + 200 Rifle — 700 men, a mid-sized standing army)\n");
    }

    // -----------------------------------------------------------------------
    std::printf("\nS3 — the decay curve, run for real through the tick\n");
    {
        // Not arithmetic this time: the units are put in a world and the economy
        // stepped, so the number includes whatever the pass actually does rather
        // than what the formula says it should.
        for (const candidate& c : k_candidates)
        {
            if (c.decay == 0) continue;
            const recipe_registry reg =
                registry_with(c.credits_per_head, c.credits_per_power, c.ordnance_per_head,
                              c.decay, c.recovery);
            fixture f = make_fixture();
            const entity_id u = add_unit(f, ROW_LEVY, 500);
            // No ordnance in the pool, so the goods draw goes unmet every tick
            // and trigger (b) fires — which is the cut-supply-line case.
            int ticks = 0;
            while (ticks < 500 && f.w.units.count(u)
                   && f.w.units.at(u).supply_factor_permille > 0)
            {
                run_economy_step(f.w, reg);
                ++ticks;
            }
            const int left = f.w.units.count(u) ? f.w.units.at(u).supply_factor_permille : -1;
            std::printf("  %-20s unsupplied: %3d ticks to supply %d%s\n",
                        c.name, ticks, left,
                        left <= 0 ? " (fully unsupplied)" : " (capped at 500 ticks)");
        }
        std::printf("  A unit at supply 0 is not dead — it is WEAK: supply_factor_permille feeds\n"
                    "  unit_to_stack_entry, so an unsupplied army is measurably worse in the\n"
                    "  resolver. That is the mechanism BL-458 would cut a lane to exploit.\n");
    }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    std::printf("%s\n", failures == 0 ? "ALL PASS (0 failures)" : "FAILURES PRESENT");
    std::printf("\nNOTHING HERE CHANGES A SHIPPED RATE. Picking one is Ben's — see NEEDS_REVIEW.\n");
    return failures == 0 ? 0 : 1;
}
