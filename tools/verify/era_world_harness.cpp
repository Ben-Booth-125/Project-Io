// ---------------------------------------------------------------------------
// Headless Era -1 world harness (BL-271 first slice; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Generates the canonical world with world_params::epoch_year = 0 — the 0 CE
// antiquity start — and both VERIFIES the stop and PRINTS the world, because
// "you cannot look at a screenshot and see whether relations are interesting"
// (DEVLOG 2026-08-04 handoff: build the instrument before the feature).
//
//   R1  THE STOP HOLDS. Every province exists by year 0 (founded_year <= 0),
//       none is industrialised, and the median industrial year is 0 (nobody).
//   R2  DEMOGRAPHY IS SEEDED. Every province holds population in
//       (0, carrying_capacity]; manpower sits within its ceiling. The 1960
//       world stays unseeded (graduation is the antiquity path's job).
//   R3  MULTIPOLAR BY CONSTRUCTION. More than one nation exists at 0 CE —
//       the precondition for BL-309's two-great-powers seed to mean anything.
//   R4  DETERMINISM. Two epoch-0 generations produce byte-identical province
//       tables (the settlement pass's whole deterministic surface).
//   R5  THE 1960 ARC IS UNTOUCHED. A default-params world still
//       industrialises, still resolves ruptures (lacunae or checkpoints
//       present), and holds at least as many provinces as the 0 CE world.
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
        if (be.name == "Kepler") return &be;
    return nullptr;
}

bool provinces_identical(const std::vector<province>& a, const std::vector<province>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const province& x = a[i];
        const province& y = b[i];
        if (x.anchor != y.anchor || x.name != y.name || x.culture != y.culture
            || x.founded_year != y.founded_year || x.industrial_year != y.industrial_year
            || x.population != y.population || x.manpower_stock != y.manpower_stock
            || x.farm_q != y.farm_q || x.ore_q != y.ore_q || x.energy_q != y.energy_q
            || x.port_q != y.port_q || x.nation != y.nation)
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
    world w60 = make_hard_coded_world({}, &rep_1960);

    const auto* ka = kepler_entry(rep_a);
    const auto* kb = kepler_entry(rep_b);
    const auto* k60 = kepler_entry(rep_1960);
    check(ka && kb && k60, "Kepler entry present in all three reports");
    if (!ka || !kb || !k60) return 1;

    const std::vector<province>& ps = ka->settlement.provinces;
    const std::vector<province>& ps60 = k60->settlement.provinces;

    // --- R1: the stop holds -------------------------------------------------
    bool all_founded = !ps.empty(), none_industrial = true;
    for (const province& p : ps)
    {
        if (p.founded_year > 0) all_founded = false;
        if (p.industrialised || p.industrial_year != 0) none_industrial = false;
    }
    check(all_founded, "R1 every 0 CE province was founded by year 0");
    check(none_industrial, "R1 no 0 CE province has lit a furnace");
    check(ka->settlement.median_industrial_year == 0, "R1 median industrial year is 0 (nobody)");

    // --- R2: demography seeded ----------------------------------------------
    bool pop_ok = !ps.empty(), mp_ok = true;
    int64_t total_pop = 0, total_mp = 0;
    for (const province& p : ps)
    {
        const int64_t cap = province_carrying_capacity(p.farm_q);
        if (p.population <= 0 || p.population > cap) pop_ok = false;
        if (p.manpower_stock < 0 || p.manpower_stock > manpower_ceiling(p.population)) mp_ok = false;
        total_pop += p.population;
        total_mp += p.manpower_stock;
    }
    check(pop_ok, "R2 every 0 CE province holds population within (0, capacity]");
    check(mp_ok, "R2 manpower sits within its population ceiling");
    bool unseeded_1960 = true;
    for (const province& p : ps60)
        if (p.population != 0) unseeded_1960 = false;
    check(unseeded_1960, "R2 the 1960 world stays demography-unseeded (graduation not its path)");

    // --- R3: multipolar ------------------------------------------------------
    check(wa.nations.size() > 1, "R3 more than one nation at 0 CE");

    // --- R4: determinism -----------------------------------------------------
    check(provinces_identical(ps, kb->settlement.provinces),
          "R4 two 0 CE generations produce identical province tables");

    // --- R5: the 1960 arc untouched ------------------------------------------
    bool any_industrial_1960 = false;
    for (const province& p : ps60)
        if (p.industrialised) { any_industrial_1960 = true; break; }
    check(any_industrial_1960, "R5 the default 1960 world still industrialises");
    check(k60->settlement.lacunae > 0 || !k60->settlement.checkpoints.empty(),
          "R5 the default 1960 world still resolves ruptures");
    check(ps60.size() >= ps.size(), "R5 the 1960 world holds at least as many provinces");

    // --- The dossier ---------------------------------------------------------
    std::printf("\n=== KEPLER AT 0 CE ===\n");
    std::printf("provinces: %zu settled by year 0 (the 1960 arc reaches %zu — %zu founded later)\n",
                ps.size(), ps60.size(), ps60.size() - ps.size());
    std::printf("nations:   %zu\n", wa.nations.size());
    std::printf("people:    %" PRId64 " across all provinces; %" PRId64 " recruitable under arms\n",
                total_pop, total_mp);

    int64_t oldest = 0, youngest = -100000;
    for (const province& p : ps)
    {
        oldest = std::min(oldest, p.founded_year);
        youngest = std::max(youngest, p.founded_year);
    }
    std::printf("founding:  %" PRId64 " to %" PRId64 " (negative = years before epoch)\n\n", oldest, youngest);

    std::vector<const province*> by_pop;
    for (const province& p : ps) by_pop.push_back(&p);
    std::sort(by_pop.begin(), by_pop.end(), [](const province* a, const province* b) {
        return a->population != b->population ? a->population > b->population : a->name < b->name;
    });
    std::printf("%-34s %10s %9s %8s %7s\n", "province", "population", "manpower", "founded", "farm_q");
    const std::size_t shown = std::min<std::size_t>(12, by_pop.size());
    for (std::size_t i = 0; i < shown; ++i)
    {
        const province& p = *by_pop[i];
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
