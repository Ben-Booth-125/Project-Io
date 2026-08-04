// ---------------------------------------------------------------------------
// history_sim_harness — BL-277 (Era -1 campaign scorer) + BL-271 first slice.
//
// Binds the requirement group "era-minus-1-history-sim" (R1..R8). Runs the
// year-tick sim over the generated Kepler settlement state and over a small
// synthetic two-polity world where the supply-decay stall can be isolated.
//
// Headless: world/* logic only, no SDL and no Lua.
// ---------------------------------------------------------------------------

#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

const generation_report::body_entry* kepler_of(const generation_report& r)
{
    for (const generation_report::body_entry& b : r.bodies)
        if (b.name == "Kepler") return &b;
    return r.bodies.empty() ? nullptr : &r.bodies.front();
}

/// Compare two runs for the byte-level equality R1 asserts.
bool same_run(const history_sim_state& a, const history_sim_state& b,
              const settlement_state& sa, const settlement_state& sb)
{
    if (a.owner_changes.size() != b.owner_changes.size()) return false;
    for (std::size_t i = 0; i < a.owner_changes.size(); ++i)
        if (a.owner_changes[i].year != b.owner_changes[i].year
         || a.owner_changes[i].province != b.owner_changes[i].province
         || a.owner_changes[i].owner != b.owner_changes[i].owner)
            return false;
    if (a.province_stride != b.province_stride) return false;
    if (a.battles != b.battles || a.conquests != b.conquests
     || a.foundings != b.foundings || a.winter_campaigns != b.winter_campaigns)
        return false;
    if (sa.provinces.size() != sb.provinces.size()) return false;
    for (std::size_t i = 0; i < sa.provinces.size(); ++i)
    {
        const province& p = sa.provinces[i];
        const province& q = sb.provinces[i];
        if (p.nation != q.nation || p.population != q.population
         || p.manpower_stock != q.manpower_stock || p.culture != q.culture
         || p.contest_q != q.contest_q)
            return false;
    }
    return true;
}

/// A minimal two-province world: one rich target, one owner, at a chosen
/// distance. Used to isolate the supply-decay stall from everything else.
settlement_state two_polity_world(int separation)
{
    settlement_state ss;

    province a;
    a.col = 0; a.row = 0; a.anchor = 0;
    a.culture = 0; a.founding_culture = 0;
    a.farm_q = 900; a.ore_q = 500; a.port_q = 100;
    a.settle_score_q = 900; a.name = "Home";
    ss.provinces.push_back(a);

    province b;
    b.col = separation; b.row = 0; b.anchor = separation;
    b.culture = 1; b.founding_culture = 1;
    b.farm_q = 950; b.ore_q = 900; b.port_q = 100;
    b.settle_score_q = 880; b.name = "Prize";
    ss.provinces.push_back(b);

    return ss;
}

} // namespace

int main()
{
    std::printf("=== history sim harness (BL-277 scorer + BL-271 year tick) ===\n");

    world_params wp;
    generation_report r1;
    const world w1 = make_hard_coded_world(wp, &r1);
    (void)w1;

    const generation_report::body_entry* k1 = kepler_of(r1);
    if (!k1)
    {
        std::printf("FAIL  no body in the generation report\n");
        return 1;
    }

    const sim_terrain_view no_terrain{}; // Both members null: neutral terrain, legal by design.
    history_sim_params params;
    params.start_year = 0;
    params.stop_year  = 1960;

    // --- R1  determinism ---------------------------------------------------
    {
        settlement_state s1 = k1->settlement;
        settlement_state s2 = k1->settlement;
        const history_sim_state a = run_history_sim(s1, nullptr, no_terrain, 168, 90, params, 12345u);
        const history_sim_state b = run_history_sim(s2, nullptr, no_terrain, 168, 90, params, 12345u);
        check(same_run(a, b, s1, s2),
              "R1   two runs of the same seed produce an identical history and ownership ring");
        check(!a.polities.empty(), "R1b  the sim seeds at least one polity from the cultures");
    }

    // --- R6/R7/R8  ring size, runtime, and founding ------------------------
    {
        settlement_state s = k1->settlement;
        const std::size_t before = s.provinces.size();

        const auto t0 = std::chrono::steady_clock::now();
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, 168, 90, params, 7u);
        const auto t1 = std::chrono::steady_clock::now();
        const int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        const int64_t bytes = owner_ring_bytes(a);
        std::printf("      run: %lld provinces -> %lld, %lld battles, %lld conquests, "
                    "%lld foundings, %lld winter, ring %lld bytes, %lld ms\n",
                    static_cast<long long>(before),
                    static_cast<long long>(s.provinces.size()),
                    static_cast<long long>(a.battles),
                    static_cast<long long>(a.conquests),
                    static_cast<long long>(a.foundings),
                    static_cast<long long>(a.winter_campaigns),
                    static_cast<long long>(bytes),
                    static_cast<long long>(ms));

        check(bytes > 0 && bytes < 1024 * 1024,
              "R6   the ownership time-lapse substrate stays under 1 MB");
        check(ms < 1000,
              "R7   a full 0->1960 run completes in under a second");
        check(s.provinces.size() > before,
              "R8   the Settle verb founds provinces during the run");
        check(a.years == params.stop_year - params.start_year,
              "R6b  the run covers one tick per simulated year");

        // The change list must actually replay: the final slice has to agree
        // with the provinces' own nation field, or the time-lapse would drift
        // from the world it claims to depict.
        const std::vector<uint16_t> last = owner_slice_at(a, params.stop_year);
        bool replay_agrees = (last.size() == s.provinces.size());
        if (replay_agrees)
            for (std::size_t i = 0; i < s.provinces.size(); ++i)
                if (s.provinces[i].nation >= 0
                 && last[i] != static_cast<uint16_t>(s.provinces[i].nation))
                    { replay_agrees = false; break; }
        check(replay_agrees,
              "R6c  replaying the change list reproduces the final political map");
    }

    // --- R3  the supply-decay stall ----------------------------------------
    //
    // Same two polities, same prize, different distance. Near, the campaign is
    // supplied and the province can change hands; far, supply decays to
    // nothing and the frontier stalls on arithmetic alone.
    {
        history_sim_params p2 = params;
        p2.stop_year = 400;

        settlement_state near_w = two_polity_world(3);
        settlement_state far_w  = two_polity_world(34);

        const history_sim_state a = run_history_sim(near_w, nullptr, no_terrain, 168, 90, p2, 99u);
        const history_sim_state b = run_history_sim(far_w,  nullptr, no_terrain, 168, 90, p2, 99u);

        std::printf("      near: %lld battles / %lld conquests | far: %lld battles / %lld conquests\n",
                    static_cast<long long>(a.battles), static_cast<long long>(a.conquests),
                    static_cast<long long>(b.battles), static_cast<long long>(b.conquests));

        check(a.battles > 0, "R3a  an adjacent objective is actually campaigned for");
        check(b.conquests == 0,
              "R3b  a distant objective is never taken — supply decay stalls the frontier");
        check(a.conquests >= b.conquests,
              "R3c  proximity never scores worse than distance for territorial gain");
    }

    // --- R5  season is an action axis, not a clock -------------------------
    {
        settlement_state s = k1->settlement;
        history_sim_params p3 = params;
        p3.stop_year = 600;
        p3.winter_score_premium_q = 0; // Make winter freely competitive.
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, 168, 90, p3, 4242u);
        check(a.winter_campaigns > 0,
              "R5   winter campaigns are chosen as candidates, not scheduled by a clock");
        check(a.battles >= a.winter_campaigns,
              "R5b  winter campaigns are a subset of all campaigns");
    }

    // --- R4  province granularity ------------------------------------------
    //
    // Structural, not observational: run_history_sim takes no mutable tile
    // access at all — its terrain view is two const pointers — so it CANNOT
    // write a tile. The assertion below records that transfer moves the
    // province's own owner field and nothing wider.
    {
        settlement_state s = two_polity_world(3);
        history_sim_params p4 = params;
        p4.stop_year = 400;
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, 168, 90, p4, 5u);
        bool anchors_intact = (s.provinces[0].anchor == 0 && s.provinces[1].anchor == 3);
        check(anchors_intact,
              "R4   a run never rewrites a province's anchor tile — transfer is province-granular");
        check(a.province_stride == static_cast<int>(s.provinces.size()),
              "R4b  the ring's stride matches the final province count");
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
