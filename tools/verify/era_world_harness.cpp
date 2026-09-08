// ---------------------------------------------------------------------------
// Headless Era -1 world harness (BL-271 first slice; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Generates the canonical world with world_params::epoch_year = 0 — the 0 CE
// antiquity start — and both VERIFIES the stop and PRINTS the world, because
// "you cannot look at a screenshot and see whether relations are interesting"
// (DEVLOG 2026-08-04 handoff: build the instrument before the feature).
//
//   R1  THE STOP HOLDS. Every region exists by year 0 (founded_year <= 0),
//       none is industrialised, and the median industrial year is 0 (nobody).
//   R2  DEMOGRAPHY IS SEEDED. Every region holds population in
//       (0, carrying_capacity]; manpower sits within its ceiling. The 1960
//       world stays unseeded (graduation is the antiquity path's job).
//   R3  MULTIPOLAR BY CONSTRUCTION. More than one nation exists at 0 CE —
//       the precondition for BL-309's two-great-powers seed to mean anything.
//   R4  DETERMINISM. Two epoch-0 generations produce byte-identical region
//       tables (the settlement pass's whole deterministic surface).
//   R5  THE 1960 ARC IS UNTOUCHED. An explicit epoch_year = 1960 world still
//       industrialises, still resolves ruptures (lacunae or checkpoints
//       present), and holds at least as many regions as the 0 CE world.
//
// Exits non-zero on any FAIL. Links the generation TU superset (as
// world_audit / determinism_harness).

#include "world/hard_coded_world.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cinttypes>
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

const generation_report::body_entry* kepler_entry(const generation_report& rep)
{
    for (const auto& be : rep.bodies)
        if (be.is_homeworld) return &be; // identity, not display name (BL-257)
    return nullptr;
}

bool regions_identical(const std::vector<region>& a, const std::vector<region>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const region& x = a[i];
        const region& y = b[i];
        if (x.anchor != y.anchor || x.name != y.name || x.culture != y.culture // BL-826: culture_shares::operator==
            || x.founded_year != y.founded_year || x.industrial_year != y.industrial_year
            || x.population != y.population || x.manpower_stock != y.manpower_stock
            || x.farm_q != y.farm_q || x.ore_q != y.ore_q || x.energy_q != y.energy_q
            || x.port_q != y.port_q || x.nation != y.nation
            // BL-766: the urban record is sim output too, so R4's determinism
            // claim has to cover it or a non-deterministic city map passes.
            || x.centres != y.centres || x.centres_razed != y.centres_razed
            || x.urban_population != y.urban_population)
            return false;
    }
    return true;
}

} // namespace

int main()
{
    // --- The 0 CE world (twice, for R4) ------------------------------------
    world_params antiq{};
    antiq.epoch_year = 0;

    generation_report rep_a{}, rep_b{}, rep_1960{};
    world wa = make_hard_coded_world(antiq, &rep_a);
    world wb = make_hard_coded_world(antiq, &rep_b);

    // R5's control world. NOT `{}` any more: the default epoch became 0 with the
    // ancient refocus (NR-177), so default params now generate the SAME world as
    // `antiq` and every R5 check would fail by construction rather than by
    // regression. The 1960 arc is still reachable — it just has to be asked for.
    world_params modern{};
    modern.epoch_year = 1960;
    world w60 = make_hard_coded_world(modern, &rep_1960);

    const auto* ka = kepler_entry(rep_a);
    const auto* kb = kepler_entry(rep_b);
    const auto* k60 = kepler_entry(rep_1960);
    check(ka && kb && k60, "Kepler entry present in all three reports");
    if (!ka || !kb || !k60) return 1;

    const std::vector<region>& ps = ka->settlement.regions;
    const std::vector<region>& ps60 = k60->settlement.regions;

    // --- R1: the stop holds -------------------------------------------------
    bool all_founded = !ps.empty(), none_industrial = true;
    for (const region& p : ps)
    {
        if (p.founded_year > 0) all_founded = false;
        if (p.industrialised || p.industrial_year != 0) none_industrial = false;
    }
    check(all_founded, "R1 every 0 CE region was founded by year 0");
    check(none_industrial, "R1 no 0 CE region has lit a furnace");
    check(ka->settlement.median_industrial_year == 0, "R1 median industrial year is 0 (nobody)");

    // --- R2: demography seeded ----------------------------------------------
    bool pop_ok = !ps.empty(), mp_ok = true;
    int64_t total_pop = 0, total_mp = 0;
    for (const region& p : ps)
    {
        const int64_t cap = region_carrying_capacity(p.farm_q);
        if (p.population <= 0 || p.population > cap) pop_ok = false;
        if (p.manpower_stock < 0 || p.manpower_stock > manpower_ceiling(p.population)) mp_ok = false;
        total_pop += p.population;
        total_mp += p.manpower_stock;
    }
    check(pop_ok, "R2 every 0 CE region holds population within (0, capacity]");
    check(mp_ok, "R2 manpower sits within its population ceiling");
    bool unseeded_1960 = true;
    for (const region& p : ps60)
        if (p.population != 0) unseeded_1960 = false;
    check(unseeded_1960, "R2 the 1960 world stays demography-unseeded (graduation not its path)");

    // --- R3: multipolar ------------------------------------------------------
    check(wa.nations.size() > 1, "R3 more than one nation at 0 CE");

    // --- R4: determinism -----------------------------------------------------
    check(regions_identical(ps, kb->settlement.regions),
          "R4 two 0 CE generations produce identical region tables");

    // --- R5: the 1960 arc untouched ------------------------------------------
    bool any_industrial_1960 = false;
    for (const region& p : ps60)
        if (p.industrialised) { any_industrial_1960 = true; break; }
    check(any_industrial_1960, "R5 the 1960 world still industrialises");
    check(k60->settlement.lacunae > 0 || !k60->settlement.checkpoints.empty(),
          "R5 the 1960 world still resolves ruptures");
    // R5's region-count comparison is against the SETTLEMENT PASS's 0 CE
    // output, not against `ps`. Since the year-tick sim was wired into
    // generation (2026-08-12) the 0 CE world keeps founding regions for 400
    // years after the settlement pass stops — 1107 of them on a measured run —
    // so `ps` is no longer "what had been settled by year 0" and comparing 1960
    // against it asks the wrong question. The claim worth checking is unchanged:
    // the 1960 arc settles at least as much ground as the ancient stop did.
    world_params antiq_settled = antiq;
    antiq_settled.prehistory_years = 0;
    generation_report rep_settled{};
    make_hard_coded_world(antiq_settled, &rep_settled);
    const auto* k_settled = kepler_entry(rep_settled);
    check(k_settled != nullptr, "R5 the settlement-only 0 CE control generated");
    if (k_settled != nullptr)
        check(ps60.size() >= k_settled->settlement.regions.size(),
              "R5 the 1960 world holds at least as many regions as the 0 CE settlement pass");

    // --- R6: roads and markets FROM the history (BL-768) --------------------
    //
    // THE ONLY PLACE THIS CAN BE ASSERTED. Every road and market census in the
    // project declares `no_prehistory()` — correct for their own subjects, and
    // structurally blind to this one: with no era there are no corridors, so the
    // ancient stamp is a no-op and the market carve's trade term never fires.
    // This harness generates the era-ON world by construction, so the check
    // belongs here rather than beside the passes it measures.
    //
    // Four claims, in the order the phase produces them:
    //   R6a  the history RECORDED where it walked;
    //   R6b  those lines MET somewhere — a network with no junction is a set of
    //        unconnected spokes, and the market term would have nothing to read;
    //   R6c  the ancient roads are ON THE GROUND, not merely recorded;
    //   R6d  a market actually emerges from trade — over a SEED SWEEP, not at one
    //        seed.
    //
    // R6d IS A REACHABILITY CLAIM, and deliberately so: whether a junction
    // happens to hold a centre its nation's own gate would have refused is
    // spatial luck at any one world, exactly as road_generation_harness argues
    // for the Highway tier. A world where trade opened no market is a legitimate
    // outcome (§ Asymmetry is the deliverable, and the five calls' "tune the
    // forces, never the outcome"); a SWEEP where it never happens would mean the
    // term is dead, which is the thing worth failing on.
    {
        std::printf("\n--- R6  roads and markets from the history (BL-768) ---\n");
        std::printf("  seed 0: corridors %" PRId64 "  junctions (degree >= 3) %" PRId64
                    "  markets opened by trade %" PRId64 "\n",
                    rep_a.prehistory_corridors, rep_a.prehistory_junctions,
                    rep_a.markets_from_trade);
        check(rep_a.prehistory_corridors > 0,
              "R6a the era recorded the corridors it supplied and settled along");
        check(rep_a.prehistory_junctions > 0,
              "R6b those corridors MEET somewhere — there are trade junctions to carve on");
        check(rep_a.prehistory_corridors == rep_b.prehistory_corridors
              && rep_a.prehistory_junctions == rep_b.prehistory_junctions
              && rep_a.markets_from_trade == rep_b.markets_from_trade,
              "R6 the record is deterministic across two generations");

        int roaded = 0;
        for (const auto& [tid, tc] : wa.tiles)
            if (tc.body == wa.home_body && tc.road_level > 0 && !is_water(tc.substrate))
                ++roaded;
        std::printf("  roaded land tiles on the 0 CE world: %d\n", roaded);
        check(roaded > 0, "R6c the ancient world opens with roads on the ground");

        int seeds_with_trade_market = 0, trade_markets = 0;
        std::int64_t seed0_trade = rep_a.markets_from_trade;
        for (std::uint32_t s = 1; s < 4; ++s)
        {
            world_params sp{};
            sp.epoch_year = 0;
            sp.seed = s * 0x9E3779B1u;
            generation_report rs{};
            make_hard_coded_world(sp, &rs);
            std::printf("  seed %u: corridors %" PRId64 "  junctions %" PRId64
                        "  markets opened by trade %" PRId64 "\n",
                        s, rs.prehistory_corridors, rs.prehistory_junctions,
                        rs.markets_from_trade);
            if (rs.markets_from_trade > 0) ++seeds_with_trade_market;
            trade_markets += static_cast<int>(rs.markets_from_trade);
        }
        if (seed0_trade > 0) { ++seeds_with_trade_market; trade_markets += static_cast<int>(seed0_trade); }
        std::printf("  markets opened by trade: %d across %d of 4 worlds\n",
                    trade_markets, seeds_with_trade_market);
        check(seeds_with_trade_market > 0,
              "R6d a market emerges where trade concentrated (reachable across the seed sweep)");
    }

    // --- The dossier ---------------------------------------------------------
    std::printf("\n=== KEPLER AT 0 CE ===\n");
    // Signed difference deliberately: the 0 CE world now out-founds the 1960 one
    // (the year-tick sim keeps settling for 400 years), so an unsigned subtraction
    // here wrapped to a nonsense number.
    std::printf("regions: %zu at 0 CE (the 1960 arc reaches %zu — a difference of %" PRId64 ")\n",
                ps.size(), ps60.size(),
                static_cast<int64_t>(ps60.size()) - static_cast<int64_t>(ps.size()));
    std::printf("nations:   %zu\n", wa.nations.size());
    std::printf("people:    %" PRId64 " across all regions; %" PRId64 " recruitable under arms\n",
                total_pop, total_mp);

    int64_t oldest = 0, youngest = -100000;
    for (const region& p : ps)
    {
        oldest = std::min(oldest, p.founded_year);
        youngest = std::max(youngest, p.founded_year);
    }
    std::printf("founding:  %" PRId64 " to %" PRId64 " (negative = years before epoch)\n\n", oldest, youngest);

    std::vector<const region*> by_pop;
    for (const region& p : ps) by_pop.push_back(&p);
    std::sort(by_pop.begin(), by_pop.end(), [](const region* a, const region* b) {
        return a->population != b->population ? a->population > b->population : a->name < b->name;
    });
    std::printf("%-34s %10s %9s %8s %7s\n", "region", "population", "manpower", "founded", "farm_q");
    const std::size_t shown = std::min<std::size_t>(12, by_pop.size());
    for (std::size_t i = 0; i < shown; ++i)
    {
        const region& p = *by_pop[i];
        std::printf("%-34s %10" PRId64 " %9" PRId64 " %8" PRId64 " %7d\n",
                    p.name.c_str(), p.population, p.manpower_stock, p.founded_year, p.farm_q);
    }

    std::printf("\nnations at 0 CE:\n");
    std::vector<std::pair<std::string, std::size_t>> nations;
    for (const auto& [id, n] : wa.nations) nations.push_back({ n.name, n.tiles.size() });
    std::sort(nations.begin(), nations.end(),
              [](const auto& a, const auto& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
    for (const auto& [name, tiles] : nations)
        std::printf("  %-30s %zu tiles\n", name.c_str(), tiles);

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
