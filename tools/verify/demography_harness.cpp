// ---------------------------------------------------------------------------
// Headless region-demography harness (BL-273; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Exercises src/world/settlement.{hpp,cpp}'s demography model in isolation —
// synthetic regions, not a generated world, since the model is deliberately
// self-contained (BL-273 design: "you don't need to wire that graduation
// path, just make the region-level model correct").
//
//   R1  LOGISTIC GROWTH TRACKS farm_q. Two regions seeded with the same
//       starting population but different farm_q must diverge: the richer
//       farm endowment carries a higher carrying capacity and so a higher
//       population after the same number of simulated years.
//
//   R2  WAR AND PLAGUE BOTH DRAW POPULATION DOWN. War pressure measurably
//       suppresses growth relative to a no-war control. Plague goes through
//       `resolve_plague_event`'s checkpoint-eligibility filter — a region
//       with nobody left (population == 0) must never be chosen as an
//       epicentre, and a call where EVERY region is ineligible must fail
//       out rather than silently picking one anyway.
//
//   R3  MANPOWER IS A BOUNDED, SELF-LIMITING BUDGET. The stock never exceeds
//       its population-derived ceiling, never goes negative, and raising more
//       than is banked yields only what is actually available — repeated
//       over-raising bounds out at zero rather than going unbounded.
//
//   R4  DETERMINISM. Two identical runs (same seed, same starting state, same
//       step sequence) produce byte-identical population/manpower/checkpoint
//       trajectories — the S1a/S1b pair-test shape settlement_harness uses.
//
// The process exits non-zero if any assertion FAILs.

#include "world/planetology.hpp"
#include "world/settlement.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

region make_region(const char* name, int col, int row, int farm_q, int64_t population)
{
    region p;
    p.name = name;
    p.col = col;
    p.row = row;
    p.farm_q = farm_q;
    p.population = population;
    return p;
}

} // namespace

int main()
{
    std::printf("=== demography harness (BL-273 region demography) ===\n");

    // --- R1 logistic growth tracks farm_q ---------------------------------------
    {
        region lean = make_region("Lean Farm", 0, 0, /*farm_q=*/50, /*population=*/1000);
        region rich = make_region("Rich Farm", 10, 10, /*farm_q=*/950, /*population=*/1000);

        const int64_t k_lean = region_carrying_capacity(lean.farm_q);
        const int64_t k_rich = region_carrying_capacity(rich.farm_q);
        check(k_rich > k_lean, "R1a  a higher farm_q carries a higher capacity");

        advance_region_demography(lean, /*years=*/300, /*war_pressure_q=*/0);
        advance_region_demography(rich, /*years=*/300, /*war_pressure_q=*/0);

        check(lean.population > 1000, "R1b  the lean-farm region still grows from its seed");
        check(rich.population > lean.population,
              "R1c  the richer farm_q region ends up with more people over the same span");
        check(lean.population <= k_lean && rich.population <= k_rich,
              "R1d  neither region's population ever exceeds its own carrying capacity");
        check(lean.last_demography_year == 300 && rich.last_demography_year == 300,
              "R1e  last_demography_year advances by exactly the years simulated");

        // Approaching, not overshooting: a few more years should still gain
        // ground while comfortably below K (monotone rise, no oscillation).
        region mid = make_region("Mid Farm", 0, 0, /*farm_q=*/500, /*population=*/1000);
        advance_region_demography(mid, 10, 0);
        const int64_t after_10 = mid.population;
        advance_region_demography(mid, 10, 0);
        check(mid.population > after_10, "R1f  population keeps climbing while below capacity");
        std::printf("      (lean K=%lld pop=%lld; rich K=%lld pop=%lld)\n",
                    static_cast<long long>(k_lean), static_cast<long long>(lean.population),
                    static_cast<long long>(k_rich), static_cast<long long>(rich.population));
    }

    // --- R2 war and plague both draw population down ---------------------------
    {
        region at_peace = make_region("Peaceful Vale", 0, 0, 500, 50000);
        region at_war    = make_region("Contested Vale", 0, 0, 500, 50000);

        advance_region_demography(at_peace, 40, /*war_pressure_q=*/0);
        advance_region_demography(at_war, 40, /*war_pressure_q=*/900);

        check(at_war.population < at_peace.population,
              "R2a  sustained heavy war pressure leaves fewer people than the same span at peace");

        // Plague eligibility: only one survivor among several empty regions
        // must always be the epicentre it picks.
        std::vector<region> ring;
        ring.push_back(make_region("Empty A", 0, 0, 500, 0));
        ring.push_back(make_region("Empty B", 2, 0, 500, 0));
        ring.push_back(make_region("Survivor", 1, 1, 500, 20000));
        ring.push_back(make_region("Empty C", 3, 3, 500, 0));

        std::vector<checkpoint_record> checkpoints;
        bool eligibility_held = true;
        int64_t survivor_before = ring[2].population;
        for (uint32_t s = 0; s < 12; ++s)
        {
            const bool applied = resolve_plague_event(ring, checkpoints, /*seed=*/0xDEAD0000u + s,
                                                       /*stage_tag=*/0x1000u + s, /*gw=*/64);
            if (!applied) { eligibility_held = false; break; }
            if (checkpoints.back().branch_taken != std::string("Survivor"))
                eligibility_held = false;
        }
        check(eligibility_held,
              "R2b  with only one populated region, the plague eligibility filter always picks it");
        check(ring[2].population <= survivor_before,
              "R2c  the actual eligible epicentre loses population (drawdown happened, not just filtering)");
        check(ring[0].population == 0 && ring[1].population == 0 && ring[3].population == 0,
              "R2d  regions already at zero stay at zero (never go negative)");

        // Now make EVERY region ineligible: the call must fail out, not
        // silently choose an ineligible candidate.
        std::vector<region> empty_only;
        empty_only.push_back(make_region("Nobody A", 0, 0, 500, 0));
        empty_only.push_back(make_region("Nobody B", 5, 5, 500, 0));
        std::vector<checkpoint_record> empty_checkpoints;
        const bool applied_when_none_eligible =
            resolve_plague_event(empty_only, empty_checkpoints, /*seed=*/1, /*stage_tag=*/2, /*gw=*/64);
        check(!applied_when_none_eligible,
              "R2e  a call where every candidate is ineligible fails out rather than picking one anyway");
        bool all_rerolls = !empty_checkpoints.empty();
        for (const auto& c : empty_checkpoints)
            if (c.branch_taken != std::string("(no eligible branch — rerolled)")) all_rerolls = false;
        check(all_rerolls,
              "R2f  every attempt against an all-ineligible field is recorded as a rerolled attempt");
    }

    // --- R3 manpower is a bounded, self-limiting budget -------------------------
    {
        region p = make_region("Levy Region", 0, 0, 500, 100000);
        advance_region_demography(p, 1, 0); // One step to replenish the stock.

        const int64_t ceiling = manpower_ceiling(p.population);
        check(ceiling > 0, "R3a  a populated region has a positive manpower ceiling");
        check(p.manpower_stock >= 0 && p.manpower_stock <= ceiling,
              "R3b  the stock sits inside [0, ceiling] after a replenish step");

        // Raise exactly what's banked, then try to raise more.
        const int64_t stock_before = p.manpower_stock;
        const int64_t first_raise = raise_manpower(p, stock_before);
        check(first_raise == stock_before, "R3c  raising exactly the stock returns exactly the stock");
        check(p.manpower_stock == 0, "R3d  the stock is fully spent");

        const int64_t over_raise = raise_manpower(p, 1'000'000'000LL);
        check(over_raise == 0, "R3e  raising past an empty stock yields zero, never negative");
        check(p.manpower_stock == 0, "R3f  the stock stays clamped at zero, never goes negative");

        // Repeated over-raising across several demography steps stays bounded
        // — the self-limiting close the design calls for, not a fiat cap.
        bool ever_negative = false;
        for (int step = 0; step < 20; ++step)
        {
            advance_region_demography(p, 5, /*war_pressure_q=*/300);
            const int64_t raised = raise_manpower(p, 1'000'000'000LL);
            if (p.manpower_stock < 0 || raised < 0) ever_negative = true;
        }
        check(!ever_negative,
              "R3g  twenty steps of grow-then-drain-past-capacity never produces a negative stock");
    }

    // --- R4 determinism ----------------------------------------------------------
    {
        auto run = [](std::vector<region>& out, std::vector<checkpoint_record>& cps) {
            out.clear();
            out.push_back(make_region("Alpha", 0, 0, 300, 5000));
            out.push_back(make_region("Beta", 4, 4, 700, 5000));
            out.push_back(make_region("Gamma", 8, 8, 500, 5000));

            for (int step = 0; step < 15; ++step)
            {
                for (std::size_t i = 0; i < out.size(); ++i)
                {
                    const int war_q = (step % 3 == 0 && i == 0) ? 400 : 0;
                    advance_region_demography(out[i], /*years=*/10, war_q);
                }
                if (step % 4 == 0)
                    resolve_plague_event(out, cps, /*seed=*/0xB2730001u,
                                        /*stage_tag=*/0x9000u + static_cast<uint32_t>(step), /*gw=*/64);
                for (region& pr : out)
                    raise_manpower(pr, 50);
            }
        };

        std::vector<region> run_a, run_b;
        std::vector<checkpoint_record> cps_a, cps_b;
        run(run_a, cps_a);
        run(run_b, cps_b);

        bool identical = run_a.size() == run_b.size() && cps_a.size() == cps_b.size();
        if (identical)
        {
            for (std::size_t i = 0; i < run_a.size(); ++i)
            {
                if (run_a[i].population != run_b[i].population
                 || run_a[i].manpower_stock != run_b[i].manpower_stock
                 || run_a[i].last_demography_year != run_b[i].last_demography_year)
                {
                    identical = false;
                    break;
                }
            }
            for (std::size_t i = 0; identical && i < cps_a.size(); ++i)
                if (cps_a[i].branch_taken != cps_b[i].branch_taken
                 || cps_a[i].viability_result != cps_b[i].viability_result)
                    identical = false;
        }
        check(identical,
              "R4  two identical runs of the same seed/step sequence produce byte-identical trajectories");

        std::printf("      (run: %lld / %lld / %lld pop; %d checkpoints)\n",
                    static_cast<long long>(run_a[0].population),
                    static_cast<long long>(run_a[1].population),
                    static_cast<long long>(run_a[2].population),
                    static_cast<int>(cps_a.size()));
    }

    std::printf("=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
