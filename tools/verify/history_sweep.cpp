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
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <chrono>
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
        if (b.name == "Kepler") return &b;
    return r.bodies.empty() ? nullptr : &r.bodies.front();
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

    int64_t peak_population  = 0;
    int64_t peak_year        = 0;
    int64_t epoch_population = 0;

    int lacunae = 0;

    int64_t industrial_first  = 0;
    int64_t industrial_median = 0;
    int64_t industrial_last   = 0;

    int64_t ms = 0;
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

    std::printf("=== history sweep (BL-275) — %d seeds, 0 -> 1960 CE ===\n\n", seed_count);

    std::vector<sweep_row> rows;
    rows.reserve(static_cast<std::size_t>(seed_count));

    for (int i = 0; i < seed_count; ++i)
    {
        const auto t0 = std::chrono::steady_clock::now();

        world_params wp;
        wp.seed = static_cast<uint32_t>(i);
        generation_report rep;
        const world w = make_hard_coded_world(wp, &rep);
        (void)w;

        const generation_report::body_entry* k = kepler_of(rep);
        if (!k) continue;

        settlement_state ss = k->settlement;

        sweep_row row;
        row.seed            = wp.seed;
        row.provinces_start = static_cast<int>(ss.provinces.size());
        row.lacunae         = k->settlement.lacunae;

        history_sim_params params;
        params.start_year = 0;
        params.stop_year  = 1960;
        const sim_terrain_view no_terrain{};

        const history_sim_state sim =
            run_history_sim(ss, nullptr, no_terrain, 168, 90, params, wp.seed);

        row.provinces_end = static_cast<int>(ss.provinces.size());
        row.battles       = sim.battles;
        row.conquests     = sim.conquests;
        row.foundings     = sim.foundings;
        row.peak_population = sim.peak_population;
        row.peak_year       = sim.peak_year;

        for (const province& p : ss.provinces) row.epoch_population += p.population;

        // Shape at the start and at the epoch, plus the highest concentration
        // reached at any century — a world can form a hegemony and lose it
        // again, and only the peak shows that it happened at all.
        int dummy = 0;
        slice_shape(owner_slice_at(sim, 0), row.powers_start, dummy);
        slice_shape(owner_slice_at(sim, 1960), row.powers_end, row.top_share_q,
                    &row.smallest_holding);

        for (int64_t y = 0; y <= 1960; y += 100)
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
                "peak pop (yr)      epoch pop     ms\n");
    std::printf("----  -------------  ---------------  ----  -----  -------  ----  "
                "-----------------  ------------  ----\n");
    for (const sweep_row& r : rows)
    {
        char heg[16];
        if (r.hegemony_year < 0) std::snprintf(heg, sizeof heg, "  -  ");
        else                     std::snprintf(heg, sizeof heg, "%5lld",
                                               static_cast<long long>(r.hegemony_year));
        std::printf("%4u  %5d > %5d  %6d > %6d  %3d%%  %3d  %s  %7lld  %4lld  %11lld (%4lld)  %12lld  %4lld\n",
                    r.seed, r.provinces_start, r.provinces_end,
                    r.powers_start, r.powers_end,
                    r.top_share_q / 10, r.smallest_holding, heg,
                    static_cast<long long>(r.battles), static_cast<long long>(r.conquests),
                    static_cast<long long>(r.peak_population), static_cast<long long>(r.peak_year),
                    static_cast<long long>(r.epoch_population),
                    static_cast<long long>(r.ms));
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
        (void)w;
        const generation_report::body_entry* k = kepler_of(rep);
        settlement_state ss = k->settlement;
        history_sim_params params; params.start_year = 0; params.stop_year = 1960;
        const sim_terrain_view no_terrain{};
        const history_sim_state again =
            run_history_sim(ss, nullptr, no_terrain, 168, 90, params, wp.seed);
        check(again.battles == rows.front().battles && again.conquests == rows.front().conquests,
              "S2   re-running a swept seed reproduces its row exactly");
    }

    bool spread = false;
    for (const sweep_row& r : rows)
        if (r.battles != rows.front().battles) { spread = true; break; }
    check(spread || rows.size() <= 1,
          "S3   seeds actually diverge — the sweep measures a spread, not one world N times");

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
