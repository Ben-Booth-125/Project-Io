// ---------------------------------------------------------------------------
// history_sweep — BL-275. Run the 0 -> 2000 CE sim over a seed spread of
// generated worlds and REPORT THE DISTRIBUTIONS the campaign premise's
// philosophical claims are tuned against.
//
// THIS HARNESS REPORTS; IT DOES NOT GATE. That is the item's own instruction
// and it is deliberate: the first job is to show the raw spread, including the
// ugly cases, before anybody writes an assertion that hides them. Ben, filing
// it: "if a problem becomes very hard to solve, I will be fascinated." So the
// only PASS/FAIL rows below are structural (did every seed run, is the sim
// deterministic) — never "hegemony stays under X%", which is a tuning target
// nobody has chosen yet.
//
// Usage:  history_sweep [seed_count]      (default 16)
// Writes: history_sweep.json beside the working directory, plus the table below.
// ---------------------------------------------------------------------------

#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/sim_terrain_build.hpp"
#include "world/settlement.hpp"
#include "world/works_roster.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
        if (b.is_homeworld) return &b; // identity, not display name (BL-257)
    return r.bodies.empty() ? nullptr : &r.bodies.front();
}

// ---------------------------------------------------------------------------
// The works fixture (BL-321)
// ---------------------------------------------------------------------------
//
// A HAND-BUILT REGISTRY, NOT A SECOND AUTHORING PATH. The real table lives in
// scripts/works.lua and is loaded by works_registry.cpp, the one TU that pulls
// sol2 — and this harness is Lua-free by design, because every `world/*` check
// in the project is. So the fixture below is a small stand-in with the same
// SHAPE as the real table: one ungated reach work, one endowment-gated work per
// axis, one population-gated defence work.
//
// The distinction matters and is worth being explicit about. This is not
// works.lua duplicated — it is deliberately different and deliberately smaller,
// because what these checks assert is that the MECHANISM works (gates fire on
// the ground, effects reach a consumer, two runs agree), never that a
// particular authored magnitude is right. Magnitudes are the sweep's job to
// report and Ben's to tune; a fixture that mirrored works.lua would silently
// become a second copy to keep in step, which is exactly what moving the table
// to Lua was meant to avoid.
works_registry works_fixture()
{
    works_registry reg;

    auto add = [&](const char* name, roster_band band, work_gate g, work_effect e, int weight) {
        work_row r;
        r.name = name; r.band = band; r.gate = g; r.effect = e; r.weight = weight;
        reg.add_row(r);
    };

    // Ungated reach: the row poor ground can still raise. Its existence is the
    // half of the invariant that keeps breadth a decision rather than a ceiling.
    { work_gate g; g.population = 3000; work_effect e; e.reach_mod = 120;
      add("Test Way", roster_band::classical, g, e, 240); }

    { work_gate g; g.farm_q = 300; g.population = 4000; work_effect e;
      e.capacity_mod = 180; e.manpower_mod = 40;
      add("Test Granary", roster_band::classical, g, e, 220); }

    { work_gate g; g.ore_q = 400; g.population = 5000; work_effect e;
      e.manpower_mod = 60; e.industrial_mod = 120;
      add("Test Pits", roster_band::classical, g, e, 170); }

    { work_gate g; g.port_q = 400; g.population = 5000; work_effect e;
      e.reach_mod = 200; e.capacity_mod = 120;
      add("Test Mole", roster_band::classical, g, e, 150); }

    { work_gate g; g.population = 8000; work_effect e;
      e.capacity_mod = 40; e.defence_mod = 250;
      add("Test Wall", roster_band::classical, g, e, 200); }

    // A later band, to check that bands gate and that they are CUMULATIVE.
    { work_gate g; g.ore_q = 350; g.population = 15000; work_effect e;
      e.manpower_mod = 80; e.defence_mod = 480;
      add("Test Fortress", roster_band::medieval, g, e, 210); }

    return reg;
}

/// A minimal one-polity world of `n` provinces in a row, each with the ground
/// the caller asks for. Cheap enough to run several of inside a sweep.
settlement_state strip_world(int n, int farm_q, int ore_q, int port_q, int culture)
{
    settlement_state ss;
    for (int i = 0; i < n; ++i)
    {
        province p;
        p.col = i * 3; p.row = 0; p.anchor = i * 3;
        p.culture = culture; p.founding_culture = culture;
        p.farm_q = farm_q; p.ore_q = ore_q; p.port_q = port_q;
        p.settle_score_q = 900 - i;
        p.name = "Strip" + std::to_string(i);
        ss.provinces.push_back(p);
    }
    return ss;
}

/// Two rival strips laid end to end — the smallest world in which a campaign
/// has anywhere to go. `strip_world` alone is one culture, so it seeds ONE
/// polity and the campaign verb never has a target; anything asserting about
/// supply needs two.
settlement_state rival_strip(int per_side, int farm_q, int ore_q, int port_q)
{
    settlement_state ss = strip_world(per_side, farm_q, ore_q, port_q, 0);
    settlement_state b  = strip_world(per_side, farm_q, ore_q, port_q, 1);
    for (province& p : b.provinces)
    {
        p.col += per_side * 3;      // Continue the row, so the two sides adjoin.
        p.anchor = p.col;
        p.name += "B";
    }
    ss.provinces.insert(ss.provinces.end(), b.provinces.begin(), b.provinces.end());
    return ss;
}

/// One province's works mask, so a check can say WHICH works stand rather than
/// only how many.
uint32_t masks_of(const settlement_state& ss)
{
    uint32_t all = 0;
    for (const province& p : ss.provinces) all |= p.works_built;
    return all;
}

/// One world's row. Every field is a metric BL-275 named at filing.
struct sweep_row
{
    uint32_t seed = 0;

    int provinces_start = 0;
    int provinces_end   = 0;
    int powers_start    = 0;
    int powers_end      = 0;

    /// Largest single polity's share of provinces at the epoch, per-mille.
    int  top_share_q     = 0;
    /// First year any polity crossed the hegemony threshold, or -1 for never.
    int64_t hegemony_year = -1;
    int  peak_share_q     = 0; ///< The highest share reached at any point.
    /// Provinces held by the WEAKEST surviving power at the epoch — how close
    /// the model gets to eliminating anyone (BL-308).
    int  smallest_holding = 0;

    int64_t battles   = 0;
    int64_t conquests = 0;
    int64_t foundings = 0;
    /// Works raised over the run (BL-321), and how many provinces ended the run
    /// with at least one. Reported rather than gated, like every other metric
    /// here — but a column of zeroes would mean the roster never fired at all,
    /// which is the failure this item is most likely to have.
    int64_t works_raised   = 0;
    int     provinces_with_works = 0;

    int64_t peak_population  = 0;
    int64_t peak_year        = 0;
    int64_t epoch_population = 0;

    int lacunae = 0;

    int64_t industrial_first  = 0;
    int64_t industrial_median = 0;
    int64_t industrial_last   = 0;

    int64_t ms = 0;

    /// DOES THE HISTORY ACTUALLY RECUR? The sim's stated premise is that wars
    /// and expansion recur across the whole run rather than firing once. These
    /// two measure it directly: the last year anything changed hands, and the
    /// share of changes that happened in the first tenth of the run.
    int64_t last_change_year  = 0;
    int      early_change_pct = 0;
};

/// A polity holding this share of all provinces counts as a hegemon. 0.5 is
/// the plainest reading of "one power dominates the world" and is a REPORTING
/// threshold only — it gates nothing.
constexpr int hegemony_threshold_q = 500;

/// Distinct owners, the largest owner's share, and the SMALLEST holding, in one
/// materialised slice.
///
/// The smallest holding is the diagnostic for BL-308: "elimination rate 0/12"
/// alone cannot distinguish a model that is one province away from killing
/// somebody from one where the weakest power still holds fifty. Those need
/// completely different fixes, so the sweep reports the distance rather than
/// just the binary.
void slice_shape(const std::vector<uint16_t>& slice, int& powers, int& top_share_q,
                 int* smallest_out = nullptr)
{
    std::vector<uint16_t> ids;
    std::vector<int>      counts;
    int live = 0;
    for (uint16_t o : slice)
    {
        if (o == owner_none) continue;
        ++live;
        auto it = std::find(ids.begin(), ids.end(), o);
        if (it == ids.end()) { ids.push_back(o); counts.push_back(1); }
        else                 { ++counts[static_cast<std::size_t>(it - ids.begin())]; }
    }
    powers = static_cast<int>(ids.size());
    int top = 0;
    for (int c : counts) top = c > top ? c : top;
    top_share_q = live > 0 ? (top * 1000) / live : 0;

    if (smallest_out)
    {
        int small = 0;
        for (int c : counts) if (small == 0 || c < small) small = c;
        *smallest_out = small;
    }
}

/// Median of a copy — the sweep reports medians rather than means because a
/// single runaway world would drag a mean somewhere no world actually is.
int64_t median_of(std::vector<int64_t> v)
{
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

std::string json_escape(const std::string& s)
{
    std::string out;
    for (char c : s) { if (c == '"' || c == '\\') out += '\\'; out += c; }
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    int seed_count = 16;
    if (argc > 1)
    {
        const int n = std::atoi(argv[1]);
        if (n > 0) seed_count = n;
    }

    // Label the span from the defaults rather than restating it, so the banner
    // cannot drift from the run the way "0 -> 1960 CE" did.
    const history_sim_params epoch;
    std::printf("=== history sweep (BL-275) — %d seeds, %lld -> %lld ===\n\n",
                seed_count,
                static_cast<long long>(epoch.start_year),
                static_cast<long long>(epoch.stop_year));

    std::vector<sweep_row> rows;
    rows.reserve(static_cast<std::size_t>(seed_count));

    // BL-321: every sweep run now carries the works roster, so the distributions
    // below describe a world whose polities could BUILD. That is a deliberate
    // change to what this harness measures — the item's whole claim is that
    // works move the frontier stall from a ceiling to a decision, and a sweep
    // run without them could not show whether they did.
    const works_registry works = works_fixture();

    for (int i = 0; i < seed_count; ++i)
    {
        const auto t0 = std::chrono::steady_clock::now();

        world_params wp;
        wp.seed = static_cast<uint32_t>(i);
        generation_report rep;
        const world w = make_hard_coded_world(wp, &rep);

        const generation_report::body_entry* k = kepler_of(rep);
        if (!k) continue;

        settlement_state ss = k->settlement;

        sweep_row row;
        row.seed            = wp.seed;
        row.provinces_start = static_cast<int>(ss.provinces.size());
        row.lacunae         = k->settlement.lacunae;

        // THE DEFAULT EPOCH IS THE ONE THE GAME RUNS. This was pinned to
        // 0 -> 1960, which the 0 CE epoch change superseded: the sim runs
        // 4000 BCE -> 0 CE now, under the stepped decision clock whose bands all
        // sit below year 0. Pinned to 0..1960 the sweep ran 1960 FLAT ticks past
        // the last band and measured neither the span nor the clock the game
        // uses — so no calibration could be read off it. Every window below is
        // derived from `params` for the same reason.
        history_sim_params params;
        const int64_t span = params.stop_year - params.start_year;

        // REAL TERRAIN (BL-316 S1). Kepler's own ground, so mountains cost what
        // mountains cost and terrain_combat is finally live. Grid dims come from
        // the one authority (hard_coded_world.hpp): a wrong-but-same-area pair
        // here (the original 168x90 vs the real 180x84 — both 15120) misaligns
        // every terrain lookup SILENTLY instead of crashing.
        const entity_id kepler_id = k->id; // the report entry names its own entity (BL-257)
        const sim_terrain_arrays terr = build_sim_terrain(w, kepler_id,
                                                          home_grid_width, home_grid_height);

        const history_sim_state sim =
            run_history_sim(ss, nullptr, terr.view(),
                            home_grid_width, home_grid_height, params, wp.seed,
                            nullptr, &works);

        {
            int64_t early = 0;
            for (const owner_change& c : sim.owner_changes)
            {
                if (c.year > row.last_change_year) row.last_change_year = c.year;
                if (c.year <= params.start_year + span / 10) ++early; // First tenth of the run.
            }
            if (!sim.owner_changes.empty())
                row.early_change_pct =
                    static_cast<int>((early * 100) / static_cast<int64_t>(sim.owner_changes.size()));
        }

        row.provinces_end = static_cast<int>(ss.provinces.size());
        row.battles       = sim.battles;
        row.conquests     = sim.conquests;
        row.foundings     = sim.foundings;
        row.peak_population = sim.peak_population;
        row.peak_year       = sim.peak_year;

        row.works_raised = sim.works_raised;
        for (const province& p : ss.provinces)
            if (p.works_built != 0) ++row.provinces_with_works;

        for (const province& p : ss.provinces) row.epoch_population += p.population;

        // Shape at the start and at the epoch, plus the highest concentration
        // reached at any century — a world can form a hegemony and lose it
        // again, and only the peak shows that it happened at all.
        int dummy = 0;
        slice_shape(owner_slice_at(sim, params.start_year), row.powers_start, dummy);
        slice_shape(owner_slice_at(sim, params.stop_year), row.powers_end, row.top_share_q,
                    &row.smallest_holding);

        for (int64_t y = params.start_year; y <= params.stop_year; y += 100)
        {
            int p = 0, share = 0;
            slice_shape(owner_slice_at(sim, y), p, share);
            if (share > row.peak_share_q) row.peak_share_q = share;
            if (row.hegemony_year < 0 && share >= hegemony_threshold_q) row.hegemony_year = y;
        }

        std::vector<int64_t> ind;
        for (const province& p : ss.provinces)
            if (p.industrialised) ind.push_back(p.industrial_year);
        if (!ind.empty())
        {
            std::sort(ind.begin(), ind.end());
            row.industrial_first  = ind.front();
            row.industrial_median = ind[ind.size() / 2];
            row.industrial_last   = ind.back();
        }

        const auto t1 = std::chrono::steady_clock::now();
        row.ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        rows.push_back(row);
    }

    // --- The table ---------------------------------------------------------
    std::printf("seed  prov(0>epoch)  powers(0>epoch)  top%%  hegem  battles  conq  "
                "peak pop (yr)      epoch pop     ms   works(prov)\n");
    std::printf("----  -------------  ---------------  ----  -----  -------  ----  "
                "-----------------  ------------  ----  -----------\n");
    for (const sweep_row& r : rows)
    {
        char heg[16];
        if (r.hegemony_year < 0) std::snprintf(heg, sizeof heg, "  -  ");
        else                     std::snprintf(heg, sizeof heg, "%5lld",
                                               static_cast<long long>(r.hegemony_year));
        std::printf("%4u  %5d > %5d  %6d > %6d  %3d%%  %3d  %s  %7lld  %4lld  %11lld (%4lld)  %12lld  %4lld  %5lld (%4d)\n",
                    r.seed, r.provinces_start, r.provinces_end,
                    r.powers_start, r.powers_end,
                    r.top_share_q / 10, r.smallest_holding, heg,
                    static_cast<long long>(r.battles), static_cast<long long>(r.conquests),
                    static_cast<long long>(r.peak_population), static_cast<long long>(r.peak_year),
                    static_cast<long long>(r.epoch_population),
                    static_cast<long long>(r.ms),
                    static_cast<long long>(r.works_raised), r.provinces_with_works);
    }

    // --- The distributions -------------------------------------------------
    std::printf("\n--- distributions over %d worlds ---\n", static_cast<int>(rows.size()));
    if (!rows.empty())
    {
        std::vector<int64_t> powers, tops, battles, conq, ends;
        int hegemonies = 0, eliminations = 0;
        for (const sweep_row& r : rows)
        {
            powers.push_back(r.powers_end);
            tops.push_back(r.top_share_q);
            battles.push_back(r.battles);
            conq.push_back(r.conquests);
            ends.push_back(r.provinces_end);
            if (r.hegemony_year >= 0) ++hegemonies;
            if (r.powers_end < r.powers_start) ++eliminations;
        }
        auto span = [](std::vector<int64_t> v) {
            std::sort(v.begin(), v.end());
            return std::pair<int64_t, int64_t>{v.front(), v.back()};
        };
        const auto ps = span(powers), ts = span(tops), bs = span(battles);

        std::printf("  powers at epoch      median %lld   range %lld..%lld\n",
                    static_cast<long long>(median_of(powers)),
                    static_cast<long long>(ps.first), static_cast<long long>(ps.second));
        std::printf("  largest share        median %lld%%   range %lld%%..%lld%%\n",
                    static_cast<long long>(median_of(tops) / 10),
                    static_cast<long long>(ts.first / 10), static_cast<long long>(ts.second / 10));
        std::printf("  battles per world    median %lld   range %lld..%lld\n",
                    static_cast<long long>(median_of(battles)),
                    static_cast<long long>(bs.first), static_cast<long long>(bs.second));
        std::printf("  conquests per world  median %lld\n",
                    static_cast<long long>(median_of(conq)));
        std::printf("  provinces at epoch   median %lld\n",
                    static_cast<long long>(median_of(ends)));
        std::vector<int64_t> lasts;
        for (const sweep_row& r : rows) lasts.push_back(r.last_change_year);
        int64_t early_sum = 0;
        for (const sweep_row& r : rows) early_sum += r.early_change_pct;
        std::printf("\n  LAST CHANGE YEAR     median %lld   (does history recur, or stop?)\n",
                    static_cast<long long>(median_of(lasts)));
        std::printf("  CHANGES IN 1st 10%%   %lld%% on average\n",
                    static_cast<long long>(early_sum / static_cast<int64_t>(rows.size())));
        std::printf("\n  HEGEMONY RATE        %d / %d worlds reached %d%% single-power share\n",
                    hegemonies, static_cast<int>(rows.size()), hegemony_threshold_q / 10);
        std::vector<int64_t> smalls;
        for (const sweep_row& r : rows) smalls.push_back(r.smallest_holding);
        const auto ss_ = span(smalls);
        std::printf("  ELIMINATION RATE     %d / %d worlds lost even one power\n",
                    eliminations, static_cast<int>(rows.size()));
        std::printf("  WEAKEST POWER holds  median %lld provinces   range %lld..%lld\n",
                    static_cast<long long>(median_of(smalls)),
                    static_cast<long long>(ss_.first), static_cast<long long>(ss_.second));
        std::printf("\n  (Both rates are REPORTED, not asserted — BL-224's non-hegemony becomes a\n"
                    "   tuning target read off this spread, not a construction guarantee.)\n");
    }

    // --- JSON ---------------------------------------------------------------
    if (FILE* f = std::fopen("history_sweep.json", "w"))
    {
        std::fprintf(f, "{\n \"_note\": \"%s\",\n \"threshold_q\": %d,\n \"worlds\": [\n",
                     json_escape("BL-275 history sweep. Reported, not gated — see the harness "
                                 "header. One row per seed; shares are per-mille.").c_str(),
                     hegemony_threshold_q);
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const sweep_row& r = rows[i];
            std::fprintf(f,
                "  {\"seed\": %u, \"provinces_start\": %d, \"provinces_end\": %d, "
                "\"powers_start\": %d, \"powers_end\": %d, \"top_share_q\": %d, "
                "\"peak_share_q\": %d, \"smallest_holding\": %d, \"hegemony_year\": %lld, \"battles\": %lld, "
                "\"conquests\": %lld, \"foundings\": %lld, \"peak_population\": %lld, "
                "\"peak_year\": %lld, \"epoch_population\": %lld, \"lacunae\": %d, "
                "\"industrial_first\": %lld, \"industrial_median\": %lld, "
                "\"industrial_last\": %lld, \"ms\": %lld}%s\n",
                r.seed, r.provinces_start, r.provinces_end, r.powers_start, r.powers_end,
                r.top_share_q, r.peak_share_q, r.smallest_holding,
                static_cast<long long>(r.hegemony_year),
                static_cast<long long>(r.battles), static_cast<long long>(r.conquests),
                static_cast<long long>(r.foundings), static_cast<long long>(r.peak_population),
                static_cast<long long>(r.peak_year), static_cast<long long>(r.epoch_population),
                r.lacunae, static_cast<long long>(r.industrial_first),
                static_cast<long long>(r.industrial_median),
                static_cast<long long>(r.industrial_last), static_cast<long long>(r.ms),
                (i + 1 < rows.size()) ? "," : "");
        }
        std::fprintf(f, " ]\n}\n");
        std::fclose(f);
        std::printf("\nWrote history_sweep.json (%d rows)\n", static_cast<int>(rows.size()));
    }

    // --- Structural checks only --------------------------------------------
    std::printf("\n");
    check(static_cast<int>(rows.size()) == seed_count,
          "S1   every seed produced a world and completed its run");

    // Determinism across the sweep boundary: re-running one seed reproduces it.
    if (!rows.empty())
    {
        world_params wp; wp.seed = rows.front().seed;
        generation_report rep;
        const world w = make_hard_coded_world(wp, &rep);
        const generation_report::body_entry* k = kepler_of(rep);
        settlement_state ss = k->settlement;
        // Defaults, exactly as the sweep rows above use them — a recheck that
        // ran a different span would not be a recheck.
        history_sim_params params;
        // The re-run must be the SAME run: real terrain, real dims. The original
        // recheck passed an empty sim_terrain_view against real-terrain rows —
        // a guaranteed false FAIL the moment terrain changes any decision.
        const entity_id kepler_id = k->id; // the report entry names its own entity (BL-257)
        const sim_terrain_arrays terr = build_sim_terrain(w, kepler_id,
                                                          home_grid_width, home_grid_height);
        // The re-run must carry the SAME works registry for the same reason it
        // must carry the same terrain: a re-run that differs in an input is not
        // a determinism check, it is a guaranteed false FAIL.
        const history_sim_state again =
            run_history_sim(ss, nullptr, terr.view(),
                            home_grid_width, home_grid_height, params, wp.seed,
                            nullptr, &works);
        check(again.battles == rows.front().battles
           && again.conquests == rows.front().conquests
           && again.works_raised == rows.front().works_raised,
              "S2   re-running a swept seed reproduces its row exactly");
    }

    bool spread = false;
    for (const sweep_row& r : rows)
        if (r.battles != rows.front().battles) { spread = true; break; }
    check(spread || rows.size() <= 1,
          "S3   seeds actually diverge — the sweep measures a spread, not one world N times");

    // --- Works checks (BL-321) ---------------------------------------------
    //
    // These DO gate, unlike the distribution rows above, and the difference is
    // principled: S1-S3 refuse to assert tuning targets nobody has chosen, but
    // "the gate fires on the ground it names" and "the effect reaches a
    // consumer" are not tuning — they are the mechanism either working or not.
    std::printf("\n");
    {
        const sim_terrain_view no_terrain{};

        // W1 — gates read the ground they name. An ore-gated work is offered on
        // ore-rich land and withheld on ore-poor land, at the same population.
        {
            settlement_state rich = strip_world(1, 900, 900, 900, 0);
            settlement_state poor = strip_world(1, 900,   0, 900, 0);
            rich.provinces[0].population = 50000;
            poor.provinces[0].population = 50000;

            const auto ra = works.available(rich.provinces[0], roster_band::classical);
            const auto pa = works.available(poor.provinces[0], roster_band::classical);

            bool rich_has_pits = false, poor_has_pits = false;
            for (const work_row* r : ra) if (r->name == "Test Pits") rich_has_pits = true;
            for (const work_row* r : pa) if (r->name == "Test Pits") poor_has_pits = true;

            check(rich_has_pits && !poor_has_pits,
                  "W1   an ore-gated work is offered on ore-rich ground and withheld on ore-poor");

            // The other half of the gate invariant: poor ground is not shut out
            // entirely. If it were, breadth would be a ceiling again for exactly
            // the polities that most need to buy their way past it.
            bool poor_has_reach = false;
            for (const work_row* r : pa) if (r->effect.reach_mod > 0) poor_has_reach = true;
            check(poor_has_reach,
                  "W1b  ore-poor ground can still raise a reach work — breadth stays a decision");
        }

        // W2 — the population floor gates independently of the endowment.
        {
            settlement_state small = strip_world(1, 900, 900, 900, 0);
            small.provinces[0].population = 1000; // Under every population floor here.
            const auto sa = works.available(small.provinces[0], roster_band::classical);
            check(sa.empty(),
                  "W2   a province under every population floor is offered nothing");
        }

        // W3 — bands gate, and are CUMULATIVE.
        {
            settlement_state p = strip_world(1, 900, 900, 900, 0);
            p.provinces[0].population = 50000;
            const auto classical = works.available(p.provinces[0], roster_band::classical);
            const auto medieval  = works.available(p.provinces[0], roster_band::medieval);

            bool cl_fortress = false, md_fortress = false, md_way = false;
            for (const work_row* r : classical) if (r->name == "Test Fortress") cl_fortress = true;
            for (const work_row* r : medieval)
            {
                if (r->name == "Test Fortress") md_fortress = true;
                if (r->name == "Test Way")      md_way      = true;
            }
            check(!cl_fortress && md_fortress && md_way,
                  "W3   a later-band work is withheld at classical and offered at medieval, "
                  "and the earlier band's rows survive");
        }

        // W4 — the effect ACCUMULATORS are what the consumers read. Asserted on
        // the pure functions, because an end-to-end population comparison would
        // be measuring the scorer's choices as much as the effect itself.
        {
            works_registry r2 = works;
            settlement_state p = strip_world(1, 900, 900, 900, 0);
            p.provinces[0].population = 50000;

            const int gid = r2.id_of("Test Granary");
            const bool applied = gid >= 0 && apply_work_to_province(p.provinces[0], r2, gid);
            const bool refused = gid >= 0 && !apply_work_to_province(p.provinces[0], r2, gid);

            check(applied && refused,
                  "W4   a work applies once and REFUSES to be built twice");
            check(p.provinces[0].work_capacity_mod == 180
               && p.provinces[0].work_manpower_mod == 40,
                  "W4b  the province's accumulators carry the row's effect");

            const work_effect derived = r2.total_effect_mask(p.provinces[0].works_built);
            check(derived.capacity_mod == p.provinces[0].work_capacity_mod
               && derived.manpower_mod == p.provinces[0].work_manpower_mod,
                  "W4c  the accumulators agree with a fresh recompute from the mask");

            check(province_carrying_capacity(900, 180) > province_carrying_capacity(900)
               && manpower_ceiling(100000, 40) > manpower_ceiling(100000),
                  "W4d  the capacity and manpower effects reach their consumers");
        }

        // W5 — the effect is observable IN A RUN, not merely in the arithmetic.
        // Same world, same seed, works on and off: the capacity works must leave
        // the world carrying more people than it otherwise would.
        {
            history_sim_params wp2;
            wp2.start_year = -400;
            wp2.stop_year  = 0;
            // Settle OFF (an unreachable pressure threshold), so the province
            // count is fixed and the comparison isolates the capacity effect.
            // Left on, a works run would found a different number of provinces
            // from the plain one and the population totals would be measuring
            // that instead — a true-for-the-wrong-reason check either way.
            wp2.settle_pressure_q = 1001;

            settlement_state on  = strip_world(6, 800, 600, 500, 0);
            settlement_state off = strip_world(6, 800, 600, 500, 0);

            const history_sim_state a =
                run_history_sim(on, nullptr, no_terrain, 60, 30, wp2, 4242u, nullptr, &works);
            const history_sim_state b =
                run_history_sim(off, nullptr, no_terrain, 60, 30, wp2, 4242u, nullptr, nullptr);

            int64_t pop_on = 0, pop_off = 0;
            for (const province& p : on.provinces)  pop_on  += p.population;
            for (const province& p : off.provinces) pop_off += p.population;

            std::printf("      works on: %lld raised, pop %lld   |   off: %lld raised, pop %lld\n",
                        static_cast<long long>(a.works_raised), static_cast<long long>(pop_on),
                        static_cast<long long>(b.works_raised), static_cast<long long>(pop_off));

            check(a.works_raised > 0, "W5   the roster actually fires in a run");
            check(b.works_raised == 0,
                  "W5b  a null registry disables works entirely — no verb, no effect");
            check(pop_on > pop_off,
                  "W5c  capacity works leave the world carrying more people than without them");
            check(masks_of(on) != 0 && masks_of(off) == 0,
                  "W5d  the works land on provinces, and only when a registry was supplied");
        }

        // W6 — determinism, the binding invariant. Two runs at one seed must
        // produce not merely the same counts but the same works on the same
        // provinces: a roster that chose differently on a replay would make
        // every downstream generation pass irreproducible.
        {
            history_sim_params wp2;
            wp2.start_year = -400;
            wp2.stop_year  = 0;

            settlement_state s1 = strip_world(6, 800, 600, 500, 0);
            settlement_state s2 = strip_world(6, 800, 600, 500, 0);

            const history_sim_state a =
                run_history_sim(s1, nullptr, no_terrain, 60, 30, wp2, 77u, nullptr, &works);
            const history_sim_state b =
                run_history_sim(s2, nullptr, no_terrain, 60, 30, wp2, 77u, nullptr, &works);

            bool identical = a.works_raised == b.works_raised
                          && s1.provinces.size() == s2.provinces.size();
            if (identical)
                for (std::size_t i = 0; i < s1.provinces.size(); ++i)
                    if (s1.provinces[i].works_built     != s2.provinces[i].works_built
                     || s1.provinces[i].work_reach_mod  != s2.provinces[i].work_reach_mod
                     || s1.provinces[i].work_capacity_mod != s2.provinces[i].work_capacity_mod)
                    { identical = false; break; }

            check(identical,
                  "W6   two runs at one seed raise the same works on the same provinces");
        }

        // W7 — the reach effect reaches the supply path. Pre-seed every province
        // with the reach work and the polity's campaigns are supplied better, so
        // the run DIVERGES from the unseeded one.
        //
        // Divergence rather than a signed inequality on stalled_campaigns is the
        // honest claim here: better supply means more campaigns are launched as
        // well as better supplied, so the count can move either way for the right
        // reason. What must be true is that the discount is READ at all — an
        // inert field would leave the two runs byte-identical.
        {
            history_sim_params wp2;
            wp2.start_year = -400;
            wp2.stop_year  = 0;

            // THE COEFFICIENTS ARE AMPLIFIED ON PURPOSE, and the amplification
            // is the reason this check is trustworthy rather than a reason to
            // doubt it. At the authored values the terrain term on a ten-province
            // strip is worth a couple of points of supply out of 1000, so a 12%
            // discount on it truncates to zero and an inert field would pass.
            // Scaling the coefficient scales the thing being discounted, not the
            // discount rule, so what is asserted is still exactly the rule.
            wp2.terrain_reach_cost_q = 800;
            wp2.holdings_burden_q    = 40;
            wp2.free_holdings        = 2; // Bite the burden of breadth at this scale.

            works_registry r2 = works;
            const int way  = r2.id_of("Test Way");
            const int mole = r2.id_of("Test Mole");

            settlement_state seeded = rival_strip(5, 800, 600, 500);
            settlement_state plain  = rival_strip(5, 800, 600, 500);
            for (province& p : seeded.provinces)
            {
                p.population = 60000;
                apply_work_to_province(p, r2, way);
                apply_work_to_province(p, r2, mole);
            }
            for (province& p : plain.provinces) p.population = 60000;

            // Works OFF in both runs, so the only difference is the reach the
            // provinces already carry — otherwise the scorer's own building
            // would confound the comparison.
            const history_sim_state a =
                run_history_sim(seeded, nullptr, no_terrain, 60, 30, wp2, 909u, nullptr, nullptr);
            const history_sim_state b =
                run_history_sim(plain,  nullptr, no_terrain, 60, 30, wp2, 909u, nullptr, nullptr);

            std::printf("      reach seeded: %lld battles / %lld stalled   |   plain: %lld / %lld\n",
                        static_cast<long long>(a.battles), static_cast<long long>(a.stalled_campaigns),
                        static_cast<long long>(b.battles), static_cast<long long>(b.stalled_campaigns));

            check(way >= 0 && mole >= 0, "W7   the fixture carries reach works to seed");
            check(a.battles != b.battles || a.stalled_campaigns != b.stalled_campaigns
               || a.conquests != b.conquests || a.foundings != b.foundings
               || a.owner_changes.size() != b.owner_changes.size(),
                  "W7b  pre-built reach changes the supply path — reach_mod is read, not inert");
        }
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
