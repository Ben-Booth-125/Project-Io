// ---------------------------------------------------------------------------
// digitisation_sim_harness — BL-982 (DIGITISATION_READINGS). The thirteen
// readings DIGITISATION.md sec "What the phase is judged on" names, taken at
// the epoch over the curated seed spread, NEVER per world.
//
// WHY THIS EXISTS BEFORE ANY MECHANISM. The phase is designed backwards from
// its consumer, and this file is that consumer made executable. The
// Exploration sprint landed its readings first (exploration_sweep, BL-937) and
// that is what made the frozen-map risk measurable; this is the same move one
// phase later. Its value today is the BASELINE: what the world generation
// hands play already reads on each property, before the 1660 -> 1960 span
// that is supposed to produce it exists.
//
// WHAT "AT 1960" READS (BL-1040; BL-1029 before it). With `--through 1960` the
// harness generates the SHIPPED world (epoch_year 0) with the DIGITISATION SPAN
// switched on (world_params::digitisation_span_enabled): Exploration closes at
// 1660 as shipped, and the span runs as its own call from the 1660
// `exploration_output` to 1960 — 75 rounds, the span's own seed, the Industry
// tree open — before world setup builds the campaign world on its close. Never
// epoch_year 1960, which selects the superseded 1160 -> 1560 -> 1960 two-span
// arc with Exploration (and so this span) off (DIGITISATION.md's opening).
// Every reading is taken off one of three surfaces, and each printed line
// names its surface and its year:
//   * the CAMPAIGN WORLD as the app builds it on that close, up to the applied
//     search winner (readings 1, 3, 6's proxy, and the evidence counts);
//   * the HANDOFF at the close (`era_minus_one_fixture::digitisation_handoff`,
//     the span's own fold at --through); or
//   * the 1660 CONTROL: the Exploration span re-run from the fixture and
//     stopped at 1660, folded with `make_exploration_output` — the handoff the
//     shipped world hands at 1660, and the one the span resumed from. Its
//     battles, flows and living polities must equal the seed library's
//     fingerprint (taken off a default generation): that equality is the proof
//     the world IS the shipped world up to 1660, and a seed that fails it
//     reports no reading as measured.
// `--continued` keeps BL-1029's run instead of the span: Exploration's own call
// continued to --through (world_params::exploration_stop_year), Exploration's
// forces only. Every line labels it a 1200-NETWORK RUN — it prices corridor
// income and land trade on the network it inherited at 1200 until it closes,
// where the span prices on the network that survived 1660 — so it is a
// comparison, never the phase's reading.
// Without --through (1660) there is no span: the close and the control are the
// same world.
//
// THE SPAN-OPEN SURVEY (BL-1051). In span mode a section after the span's own
// table prints, per seed, the forest and fuel survey generation took over every
// region the span opened on (`era_minus_one_fixture::digitisation_open_regions`)
// and the Fuel Doctrine split at the close with each side's mean held forest --
// the evidence that `ground_forest` makes the wooded polity choose Charcoal.
// Evidence, not one of the thirteen readings.
//
// FOUR STATES, NEVER A FIFTH. A reading is MEASURED (its observable exists in
// today's world and is computed off it), PARTIAL (one clause is measured, the
// other prints n/a with its reason), a STRUCTURAL ZERO (the observable exists
// but a setting forbids it on every seed, so the zero is the setting, not a
// finding), or n/a (the mechanism it reads does not exist; the line names the
// missing piece and, where a count proves the absence, prints that count). A
// reading is never approximated into a pass: a PROXY line, where one is
// printed, is labelled as not the reading. A seed whose generation recorded a
// handoff violation (generation_report::handoff_invalid) is excluded from
// every handoff reading, and says so.
//
// THIS HARNESS REPORTS; IT DOES NOT GATE. "A reading is a requirement, not a
// target. A seed that refuses one is a legitimate world; a spread that
// refuses one is a phase that did not do its job" — that is Ben's judgement
// off the printed spread, not a PASS/FAIL line here. Exit status is non-zero
// only when the data layer fails to load or no seed generates at all.
//
// GENERATION PARITY (BL-1007). Mirrors the shipped start in app order:
//   1. scripts/world_gen.lua -> world_gen_config, scripts/works.lua ->
//      works_registry (app::begin_new_game), then make_hard_coded_world at
//      `world_params{}` with only the seed set — epoch 0, the shipped default;
//   2. scripts/recipes.lua + scripts/economy.lua -> recipe_registry, and
//      `set_era(era_band_for_epoch(epoch))` (app::load_economy);
//   3. `apply_shipped_landscape` (harness_params.hpp), which mirrors
//      app::start_new_game_prelude's search and winner.
// NOT MIRRORED, AND SAID ON THE FACE: the winner's 12-tick validation run
// (app::poll_worldgen). No shared harness helper mirrors app::step_economy,
// and copying it here would be the second construction BL-1007 exists to
// stop. Every reading below is structural — firms, installations, headcount,
// seeded treasury, culture shares, flows — and is read off the applied winner
// before any tick; an AI build during those 12 ticks is the one way the
// validation run could move a number here.
//
// THE SEEDS are read from docs/generation/seed_library.json, so the library
// and the harness cannot drift. Run from the repo root.
//
// Usage:  digitisation_sim_harness [--limit N] [--seeds a,b,c] [--through Y [--continued]] [--out path]
//                                  [--fidelity] [--resume-tier]
//   --limit N    take the library's first N seeds (a quick run)
//   --seeds ...  measure these seeds instead of the library's
//   --through Y  BL-1040: run the Digitisation span from the 1660 handoff to Y
//                (default 1660: no span). Y = 1960 is the phase's reading.
//   --continued  BL-1029's run instead of the span: Exploration's own call
//                continued to Y -- a 1200-NETWORK run, labelled so on every line
//   --integral-stride N  BL-1041, span mode: reading 8's head-years integral
//                samples the span's urban heads every N decision rounds (1, the
//                default, is every round). It re-runs the span from the 1660
//                handoff once per sample (a capture at the top of that year),
//                so it costs about (75 / N) / 2 span-lengths per seed; 0 skips
//                it and prints the integral lines as not taken.
//   --out path   BL-1029: also write the per-seed table as JSON
//   --fidelity   BL-1036: instead of the readings, the RESUME-FIDELITY check --
//                the Digitisation span's resume from the 1660
//                exploration_output, gated on its opening, on one neutralised
//                round, and (BL-1040) on the shipped span BEING that resume,
//                with the 1960 divergence reported by source (see `namespace
//                fidelity`). This mode GATES: exit 1 when any gate fails on
//                any seed.
//   --resume-tier BL-1037: at the 1200 and 1660 span boundaries, with
//                `resume_seeds_corridor_tier` off and on, the corridors whose
//                resumed rung the switch changes, any rung reopened below or
//                above its record, and any rung bought twice. GATES the
//                switch-on invariants (see `run_resume_tier`).
// ---------------------------------------------------------------------------

#include "harness_params.hpp" // apply_shipped_landscape, print_shipped_landscape

#include "scripting/lua_state.hpp"
#include "world/components.hpp"
#include "world/era_band.hpp"
#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"
#include "world/works_roster.hpp"

#include <algorithm>
#include <bitset>
#include <chrono> // --fidelity: per-seed wall time, reported only
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace
{

constexpr double k_undef = std::numeric_limits<double>::quiet_NaN();

// ---------------------------------------------------------------------------
// Statistics. Ranks are AVERAGE ranks over a stable index sort, so a tie never
// depends on container order.
// ---------------------------------------------------------------------------

std::vector<double> average_ranks(const std::vector<double>& v)
{
    std::vector<std::size_t> idx(v.size());
    std::iota(idx.begin(), idx.end(), std::size_t{0});
    std::stable_sort(idx.begin(), idx.end(),
                     [&](std::size_t a, std::size_t b) { return v[a] < v[b]; });
    std::vector<double> r(v.size(), 0.0);
    std::size_t i = 0;
    while (i < idx.size())
    {
        std::size_t j = i;
        while (j + 1 < idx.size() && v[idx[j + 1]] == v[idx[i]]) ++j;
        const double avg = (static_cast<double>(i) + static_cast<double>(j)) / 2.0 + 1.0;
        for (std::size_t k = i; k <= j; ++k) r[idx[k]] = avg;
        i = j + 1;
    }
    return r;
}

/// Pearson over two equal-length series; undefined (NaN) under 3 points or a
/// zero variance on either side — a constant series has no rank order to
/// correlate, and printing 0 there would be a reading nobody took.
double pearson(const std::vector<double>& a, const std::vector<double>& b)
{
    if (a.size() != b.size() || a.size() < 3) return k_undef;
    double ma = 0.0, mb = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) { ma += a[i]; mb += b[i]; }
    ma /= static_cast<double>(a.size());
    mb /= static_cast<double>(b.size());
    double sab = 0.0, saa = 0.0, sbb = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        sab += (a[i] - ma) * (b[i] - mb);
        saa += (a[i] - ma) * (a[i] - ma);
        sbb += (b[i] - mb) * (b[i] - mb);
    }
    if (saa <= 0.0 || sbb <= 0.0) return k_undef;
    return sab / std::sqrt(saa * sbb);
}

double spearman(const std::vector<double>& a, const std::vector<double>& b)
{
    return pearson(average_ranks(a), average_ranks(b));
}

/// Gini over non-negative values; undefined when empty or summing to zero.
double gini(std::vector<double> v)
{
    if (v.empty()) return k_undef;
    std::sort(v.begin(), v.end());
    double sum = 0.0, weighted = 0.0;
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        sum += v[i];
        weighted += static_cast<double>(i + 1) * v[i];
    }
    if (sum <= 0.0) return k_undef;
    const double n = static_cast<double>(v.size());
    return (2.0 * weighted) / (n * sum) - (n + 1.0) / n;
}

double median_of(std::vector<double> v)
{
    if (v.empty()) return k_undef;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return (n % 2 == 1) ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

/// One spread line: the defined values' min / p25 / median / p75 / max, and
/// how many worlds the quantity was undefined on. Nearest-rank quantiles.
void print_spread(const char* label, const std::vector<double>& values)
{
    std::vector<double> v;
    std::size_t undef = 0;
    for (double x : values)
    {
        if (std::isnan(x)) ++undef;
        else v.push_back(x);
    }
    if (v.empty())
    {
        std::printf("    %-62s undefined on all %zu worlds\n", label, values.size());
        return;
    }
    std::sort(v.begin(), v.end());
    const auto q = [&](double p) {
        return v[static_cast<std::size_t>(std::floor(p * static_cast<double>(v.size() - 1) + 0.5))];
    };
    std::printf("    %-62s min %8.3f  p25 %8.3f  med %8.3f  p75 %8.3f  max %8.3f",
                label, v.front(), q(0.25), q(0.5), q(0.75), v.back());
    if (undef > 0) std::printf("  (%zu undefined)", undef);
    std::printf("\n");
}

// ---------------------------------------------------------------------------
// ONE HANDOFF'S READINGS (BL-1029). Taken identically off the 1660 control and
// off the handoff at the close.
// ---------------------------------------------------------------------------
struct handoff_reading
{
    // Reading 5, present half: DIGITISATION.md sec 4 — checkered when the
    // second culture's share is at least 95% of the first's.
    int settled_land_regions = 0;
    int checkered_regions    = 0;

    // Reading 7: far trade.
    int     flows            = 0;
    int     flows_far        = 0;
    int     flows_unreadable = 0; ///< a flow whose party or capital is out of range.
    int64_t volume           = 0;
    int64_t volume_far       = 0;

    // Reading 8's evidence: region half (Stage 4) and polity half (the band).
    int industrialised_regions = 0;
    int polities_industrial    = 0; ///< living polities whose materials band crossed industrial.
    int polities_industry_nodes = 0; ///< BL-1040: living polities holding any Industry-tree node.
    int industry_nodes          = 0; ///< ... and the nodes they hold, summed.

    // Reading 9's urban half.
    int64_t urban_heads = 0;
    int64_t heads       = 0;

    // Reading 10.
    int subjects       = 0;
    int alive_polities = 0;
};

handoff_reading read_handoff(const exploration_output& h, int gw)
{
    handoff_reading out;
    const std::vector<region>& R = h.regions;
    const std::vector<polity>& P = h.polities;

    for (const region& rg : R)
    {
        if (rg.industrialised) ++out.industrialised_regions;
        out.heads       += rg.population;
        out.urban_heads += rg.urban_population;
        if (rg.domain != region_domain::land || rg.culture.empty()) continue;
        ++out.settled_land_regions;
        const int w0 = rg.culture.weight_q[0];
        const int w1 = rg.culture.weight_q[1];
        if (rg.culture.id[1] >= 0 && w1 > 0 && 100 * w1 >= 95 * w0)
            ++out.checkered_regions;
    }

    for (const polity& q : P)
    {
        if (!q.alive) continue;
        ++out.alive_polities;
        if (q.overlord >= 0) ++out.subjects;
        if (q.industrial_year != k_never_industrialised) ++out.polities_industrial;
        if (q.industry_mask != 0)
        {
            ++out.polities_industry_nodes;
            out.industry_nodes += static_cast<int>(std::bitset<64>(q.industry_mask).count());
        }
    }

    // Reading 7. A market here is a LIVING polity's capital seat: the flow
    // model sizes a flow at the buyer's capital, so these are the markets any
    // flow could have landed at. A flow is FAR when its buyer's market is
    // strictly farther from the seller's than the seller's nearest other
    // market; a tie is not far.
    std::vector<int> market_regions;
    for (const polity& q : P)
        if (q.alive && q.capital >= 0 && static_cast<std::size_t>(q.capital) < R.size())
            market_regions.push_back(q.capital);
    std::sort(market_regions.begin(), market_regions.end());
    market_regions.erase(std::unique(market_regions.begin(), market_regions.end()), market_regions.end());

    for (const trade_flow& f : h.trade_flows)
    {
        ++out.flows;
        out.volume += f.volume_q;
        if (f.seller >= P.size() || f.buyer >= P.size()) { ++out.flows_unreadable; continue; }
        const int sc = P[f.seller].capital;
        const int bc = P[f.buyer].capital;
        if (sc < 0 || bc < 0 || static_cast<std::size_t>(sc) >= R.size()
         || static_cast<std::size_t>(bc) >= R.size()) { ++out.flows_unreadable; continue; }
        const int d_buyer = region_distance(R[static_cast<std::size_t>(sc)], R[static_cast<std::size_t>(bc)], gw);
        int d_nearest = std::numeric_limits<int>::max();
        for (int m : market_regions)
        {
            if (m == sc) continue;
            d_nearest = std::min(d_nearest, region_distance(R[static_cast<std::size_t>(sc)],
                                                            R[static_cast<std::size_t>(m)], gw));
        }
        if (d_buyer > d_nearest)
        {
            ++out.flows_far;
            out.volume_far += f.volume_q;
        }
    }
    return out;
}

/// The library fingerprint for one seed: the three counters the 1660 control
/// can reproduce exactly (Exploration battles, standing flows, living
/// polities). A minimal scan, like `library_seeds`.
struct library_fingerprint
{
    int64_t expl_battles    = -1;
    int64_t flows           = -1;
    int64_t living_polities = -1;
};

std::map<uint32_t, library_fingerprint> library_fingerprints(const char* path)
{
    std::map<uint32_t, library_fingerprint> out;
    std::ifstream in(path, std::ios::binary);
    if (!in) return out;
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    const auto int_after = [&](const std::string& key, std::size_t from, std::size_t to) -> int64_t {
        const std::size_t at = text.find("\"" + key + "\"", from);
        if (at == std::string::npos || at >= to) return -1;
        std::size_t p = at + key.size() + 2;
        while (p < text.size() && (text[p] == ' ' || text[p] == ':')) ++p;
        return std::strtoll(text.c_str() + p, nullptr, 10);
    };
    std::size_t pos = text.find("\"seeds\"");
    if (pos == std::string::npos) return out;
    const std::string key = "\"seed\":";
    while ((pos = text.find(key, pos)) != std::string::npos)
    {
        const uint32_t seed = static_cast<uint32_t>(std::strtoul(text.c_str() + pos + key.size(), nullptr, 10));
        std::size_t next = text.find(key, pos + key.size());
        if (next == std::string::npos) next = text.size();
        const std::size_t fp = text.find("\"fingerprint\"", pos);
        if (fp != std::string::npos && fp < next)
        {
            library_fingerprint f;
            f.expl_battles    = int_after("expl_battles", fp, next);
            f.flows           = int_after("flows", fp, next);
            f.living_polities = int_after("living_polities", fp, next);
            out[seed] = f;
        }
        pos = next;
    }
    return out;
}

// ---------------------------------------------------------------------------
// ONE SEED'S READING. Everything a reading needs is captured here in the main
// loop, so the report below reads `rows` only and no section regenerates.
// ---------------------------------------------------------------------------
struct seed_row
{
    uint32_t seed = 0;
    bool     era_ran  = false; ///< The Empires round ran (fx.ran).
    bool     expl_ran = false; ///< The Exploration span ran, so the 1660 handoff exists.
    /// The close this run reads exists: in span mode the Digitisation span
    /// ran (fx.digitisation_ran); otherwise the Exploration close exists.
    bool     close_ran = false;

    // --- BL-1040: the Digitisation span itself (span mode only) -------------
    bool    span_ran       = false;
    int64_t span_start     = 0;
    int64_t span_stop      = 0;
    int64_t span_rounds    = 0; ///< decision rounds the span ran (75 at 1660 -> 1960)
    int64_t span_battles   = 0; ///< this span's own counters (a resumed run starts them at 0)
    int64_t span_conquests = 0;
    int64_t span_foundings = 0;
    int64_t span_ms        = 0; ///< the span's run_history_sim call, wall clock (reported only)

    // --- BL-1051: the span-open survey and the Fuel Doctrine (span mode) ----
    // The survey is read off the table the span OPENED on
    // (`era_minus_one_fixture::digitisation_open_regions`); the doctrine and
    // the held forest off the span's own 1960 close. Evidence for the
    // `ground_forest` term, not one of the thirteen readings.
    bool   survey_ran        = false;
    int    survey_regions    = 0;  ///< regions the span opened on
    int    survey_unseen     = 0;  ///< ... of which the survey left at -1 (must be 0)
    int    survey_forest[5]  = {}; ///< survey_forest_q min / p25 / med / p75 / max at the open
    int    survey_forested   = 0;  ///< regions at the open with any forest (> 0)
    int    survey_wooded     = 0;  ///< ... whose forest score is at or above the world's mean (>= 500)
    int    survey_fuel_med   = 0;  ///< survey_fuel_q median at the open (500 = the world's mean region)
    int    survey_fuel_seam  = 0;  ///< regions whose survey_fuel_q clears the 250 seam bar
    int    survey_seam_energy = 0; ///< ... and whose inherited energy_q clears it (the gate's reading before BL-1041's DEFAULT B)
    int    close_regions     = 0;  ///< regions at the close
    int    close_unsurveyed  = 0;  ///< ... founded after the open, so never surveyed (-1)
    int    doctrine[3]       = {}; ///< living polities at the close: coke / charcoal / neither
    /// Mean of each living polity's held-ground forest reading
    /// (`industry_ground_forest_q` over the regions it holds at the close:
    /// its best held forest score since the 2026-09-18 ruling), per doctrine
    /// group; NaN for an empty group.
    double doctrine_forest[3] = { k_undef, k_undef, k_undef };
    int    charcoal_seam_seen  = 0; ///< Charcoal polities that ever passed the seam gate (fuel_seen)
    int    charcoal_seam_close = 0; ///< Charcoal polities holding a seam (the gate's reading >= 250) at the close

    // --- BL-1041: reading 8, industry points (span mode) --------------------
    // Read off the span's 1960 close (`digitisation_handoff.regions`, the
    // located stock) and its own counters (`digitisation_state`). The points
    // have no sink, so the region table must hold exactly what the counters
    // credited (`points_ledger_ok`).
    bool    points_on          = false; ///< the span ran with industry_points_enabled
    bool    points_rejected    = false; ///< the run's constants were out of domain (nothing credited)
    int64_t points_total       = 0;     ///< summed over every region at the close
    int64_t points_scale       = 0;     ///< credited by the scale accrual, all rounds
    int64_t points_treasury    = 0;     ///< credited from capital treasuries, all rounds
    int64_t treasury_debited   = 0;     ///< ... and the treasury units that took out of capitals
    int64_t points_refused     = 0;     ///< credits refused (must be 0)
    bool    points_ledger_ok   = false; ///< points_total == points_scale + points_treasury
    int     points_regions     = 0;     ///< regions holding any points at the close
    int     centre_regions     = 0;     ///< regions with centres at the close
    double  points_region_gini = k_undef; ///< over regions holding points
    double  points_top_region  = k_undef; ///< the largest region's share of the total
    int     points_polities    = 0;     ///< living polities whose held ground holds points
    int     living_close       = 0;     ///< living polities at the close
    double  points_polity_gini = k_undef; ///< over living polities (held-ground sums; 0s included)
    double  points_top_polity  = k_undef; ///< the largest polity's share of the held total
    double  points_unheld      = k_undef; ///< share of the total standing on ground nobody holds
    double  treasury_share     = k_undef; ///< points_treasury / (scale + treasury)
    int     fuel_factor[5]     = {};    ///< fuel factor per mille over centre regions: min/p25/med/p75/max
    int     fuel_at_floor      = 0;     ///< centre regions whose reading is 0 (factor at the floor)
    double  rho_points_urban   = k_undef; ///< Spearman(points, urban heads) over regions with points or centres
    double  rho_intensity_fuel = k_undef; ///< Spearman(points per urban head, fuel factor) over centre regions with points
    double  inherited_share    = k_undef; ///< share of the total on regions the span founded (inherited fuel)
    int     inherited_regions  = 0;     ///< ... how many such regions hold points
    int64_t ns_points          = 0;     ///< the scale accrual's wall clock over the span (reported only)

    // --- BL-1041 fix round: reading 8's honest headcount test ----------------
    // The rho above ranks 300 years of points against urban heads at 1960 alone,
    // so a city that grew, shrank, was sacked or founded mid-span pulls it below
    // 1 even if points were pure headcount x fuel. These rank SCALE points only
    // (treasury points taken out exactly, below) against the span's integrated
    // urban head-years, both taken by re-running the span harness-side.
    //
    //  * SCALE vs TREASURY, per region, exactly: the exchange rate moves no
    //    dynamics (it only multiplies the credit), so one re-run at 1 point per
    //    treasury unit gives P2 = S + T per region beside the shipped close's
    //    P1 = S + 1000 T; T = (P1 - P2) / 999, S = P2 - T.
    //  * HEAD-YEARS: the span re-run to the top of each sampled decision year
    //    (a BL-1036 capture), summing urban heads x the years the sample stands
    //    for over rounds the region HAD CENTRES (the accrual's own condition).
    //    The capture is the year's opening state, one demography year before
    //    the accrual reads it, so this is the integral to within that year.
    bool    rerun_ok           = false; ///< the re-run's close is the shipped close outside the points
    int     rerun_mismatch     = 0;     ///< regions whose owner/heads/treasury differ (must be 0)
    bool    split_ok           = false; ///< every region's P1 - P2 split exactly (divisible, non-negative)
    int     integral_samples   = 0;     ///< capture years summed (0: the integral was not taken)
    int64_t integral_ms        = 0;     ///< wall clock of the re-runs, this seed (reported only)
    int     scale_regions      = 0;     ///< regions with scale points or head-years (the rho's population)
    double  rho_scale_hy       = k_undef; ///< Spearman(scale points, integrated urban head-years)
    double  rho_scale_int_fuel = k_undef; ///< Spearman(scale points / head-years, fuel factor)

    // --- BL-1041 fix round: WHERE the points stand, by the ground's survey --
    // Of every point at the close: original settlement ground (the pre-sim
    // settlement pass's regions and its scheduled foundings, surveyed from
    // tiles then), ground the sim founded BEFORE 1660 (its energy_q inherited,
    // re-surveyed at the span open), and ground the span founded (no survey of
    // its own: the parent's inherited under DEFAULT A).
    double  share_original     = k_undef;
    double  share_resurveyed   = k_undef;
    int     regions_original   = 0;     ///< regions at the close of each class, holding points or not
    int     regions_resurveyed = 0;
    int     regions_span_founded = 0;

    // --- Reading 1: density follows cities (campaign world) -----------------
    int    markets        = 0;
    int    markets_urban  = 0;       ///< markets whose catchment holds any urban heads.
    int    corps_on_body  = 0;       ///< corporations with >= 1 installation on the home body.
    double rho_firms_urban = k_undef;
    double rho_firms_goods = k_undef;

    // --- Reading 3: advanced chains (campaign world) ------------------------
    int64_t installations = 0;
    int64_t advanced      = 0;
    int     nations       = 0;
    int     nations_with_advanced = 0;
    double  advanced_gini          = k_undef;
    double  rho_adv_treasury       = k_undef;
    double  rho_adv_population     = k_undef;
    double  rho_adv_territory      = k_undef;

    // --- Reading 6: PROXY only (campaign world) -----------------------------
    int    nations_with_heads = 0;
    double treasury_per_head_gini       = k_undef;
    double treasury_per_head_max_on_med = k_undef;

    // --- Handoff readings (5, 7, 8, 9, 10), at the 1660 control and at the
    //     close. BL-1029: one struct read by one function, so the two years
    //     cannot be measured two different ways.
    handoff_reading at_1660;
    handoff_reading at_close;
    bool    handoff_ok      = true;  ///< generation recorded no handoff violation.
    std::string handoff_violation;
    int64_t secessions_1660  = 0;    ///< refused-renewal secessions (no war) by 1660.
    int64_t secessions_close = 0;    ///< ... by the close.
    int64_t subjections_formed_1660  = 0;
    int64_t subjections_formed_close = 0;

    // --- The 1660 control's proof: the library fingerprint ------------------
    bool    has_fingerprint = false;
    bool    prefix_matches  = false;
    int64_t control_battles = 0;

    // --- Evidence counts behind the n/a lines -------------------------------
    std::size_t battles_standing   = 0; ///< world::battles after generation.
    std::size_t buy_orders         = 0; ///< world::buy_orders (the preferred_seller carrier).
    std::size_t buy_orders_with_preferred_seller = 0;
    std::size_t trade_routes       = 0; ///< world::trade_routes after generation.
};

bool is_tier3_good(std::size_t r)
{
    // DIGITISATION.md sec 2: "1960's 'computers and parts' are the existing
    // tier-3 goods" — electronics, machinery and alloys, named there. No
    // other good is counted, so the reading cannot be widened by a roster add.
    return r == static_cast<std::size_t>(resource_type::machinery)
        || r == static_cast<std::size_t>(resource_type::alloys)
        || r == static_cast<std::size_t>(resource_type::electronics);
}

bool recipe_makes_tier3(const recipe& rc)
{
    for (std::size_t r = 0; r < resource_count; ++r)
        if (is_tier3_good(r) && rc.outputs[r] > 0.0f) return true;
    return false;
}

/// The library's seeds, in library order. A minimal scan rather than a JSON
/// parser: every `"seed": N` after the `"seeds"` key.
std::vector<uint32_t> library_seeds(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    std::vector<uint32_t> out;
    std::size_t pos = text.find("\"seeds\"");
    if (pos == std::string::npos) return out;
    const std::string key = "\"seed\"";
    while ((pos = text.find(key, pos)) != std::string::npos)
    {
        pos += key.size();
        std::size_t p = pos;
        while (p < text.size() && (text[p] == ' ' || text[p] == ':' || text[p] == '\t')) ++p;
        if (p < text.size() && text[p] >= '0' && text[p] <= '9')
            out.push_back(static_cast<uint32_t>(std::strtoul(text.c_str() + p, nullptr, 10)));
    }
    return out;
}

// ===========================================================================
// BL-1036 -- THE RESUME-FIDELITY CHECK (`--fidelity`)
// ===========================================================================
//
// DIGITISATION.md: "The span is its own call, resumed from
// `exploration_output`, and the resume loses nothing the struct carries."
// This mode proves the sentence rather than trusting it. Per seed, from the
// SHIPPED 1660 handoff (`era_minus_one_fixture::exploration_handoff`, the
// world generated at its defaults and stopped after Exploration):
//
//   C    THE CONTINUED RUN. Exploration's own call, re-run from the fixture's
//        pre-Exploration capture and carried on past 1660 -- to 1664 (its
//        1660 round), with its working state captured at the top of 1660,
//        and to 1960 (the world `--through 1960` measures).
//   R    A RESUME. A second `run_history_sim` call opened on the handoff
//        struct alone: its polities, grudges, contacts, corridor record,
//        dated objects and civilisation/creed tables through the resume
//        pointers; its regions as the settlement; a creed_state rebuilt from
//        its cultures; works and terrain from the fixture; and (BL-1040) THE
//        DIGITISATION SPAN'S OWN PARAMS as generation captured them
//        (`era_minus_one_fixture::digitisation_params`: Exploration's
//        derivation, the span moved to 1660, both anchors left at 1200, the
//        Industry tree open from 1660).
//
// THE CONTINUED RUN CARRIES THE SPAN'S FORCES FROM 1660 (BL-1040). The span
// opens the Industry tree, so C does too, from the same year: before 1660 the
// switch reads nothing (`industry_open_year`), so C up to the top of 1660 is
// Exploration's shipped run bit for bit, and after it C and R play the same
// forces. Without that, the Industry tree would be a fifth divergence source
// the doc does not name.
//
// GATE 1 (BL-1036 R4): the resume's OPENING, captured before its first act,
//   equals the struct field for field -- every region field, every polity
//   field, and every row of the other tables, dated objects included.
// GATE 2 (BL-1036 R5): after ONE round, a resume whose named network sources
//   are neutralised -- the corridor record and live road counts it prices on
//   set to the continued run's own at 1660, and the close's market stamp
//   undone -- has the continued run's 1660 round's treaties, overlord graph
//   and stocks exactly. What is left once the named sources are removed is
//   what the resume LOSES, and it must be nothing.
// GATE 3 (BL-1040 R5): THE SHIPPED SPAN IS THE REAL RESUME. V5 below -- the
//   handoff as it is plus the span-open survey (BL-1051: generation surveys
//   every region's tiles between the struct copy and the call, and captures
//   the table it opened on), the span's own seed and params, run to the span's
//   own stop -- folded with `make_digitisation_output`, equals generation's own
//   1960 close (`era_minus_one_fixture::digitisation_handoff`) field for field
//   and table for table, with the same battle, conquest and founding counts;
//   and the opening table differs from the handoff in the survey's two fields
//   only, with no region left unsurveyed.
//   This is what makes the span "the real consumer of the resume" a checked
//   claim: the resume gates 1 and 2 prove lossless is the resume the shipped
//   world runs, and no settlement or creed state outside the struct reaches it.
// REPORT: the one-round footprint of the real resume, the entry state the
//   handoff differs from the continued run in, and the 1960 divergence
//   against the continued run, attributed by source by adding them back one
//   at a time (V0 all neutralised .. V4 on the span's own seed .. V5 the real
//   resume, with the span-open survey).
//
// NEVER GATED BIT FOR BIT AT 1960 AGAINST C. A lossless resume cannot equal
// the continued run: a span reads its INHERITED corridor record until it
// closes (the continued run prices 1660-1960 on the 1200 record; a resume on
// the 1660 one), and the handoff filters that record over the span's dead.
//
// The own seed is the span's (`era_minus_one_fixture::digitisation_seed`,
// `digitisation_sim_seed`); before BL-1040 a stand-in with the same constant
// stood here, so the "seed" source reads the same run before and after.
namespace fidelity
{

using field_census = std::map<std::string, int>; ///< field -> rows differing

#define FID_CMP(f)     do { if (!(a.f == b.f)) ++out[#f]; } while (0)
#define FID_CMP_ARR(f) do { if (!std::equal(std::begin(a.f), std::end(a.f), std::begin(b.f))) ++out[#f]; } while (0)

/// Every `region` field, one by one. A field added to `region` later is not
/// compared until it is listed here -- the census prints the field count it
/// knows, so a reader can see the list is the struct's.
void region_fields(const region& a, const region& b, field_census& out)
{
    FID_CMP(anchor); FID_CMP(col); FID_CMP(row); FID_CMP(domain); FID_CMP(culture);
    FID_CMP(founding_culture); FID_CMP(creed_conquered); FID_CMP(name); FID_CMP(settle_score_q);
    FID_CMP(farm_q); FID_CMP(ore_q); FID_CMP(energy_q); FID_CMP(port_q); FID_CMP(dominant);
    FID_CMP(founded_year); FID_CMP(industrial_year); FID_CMP(industrialised);
    FID_CMP(industrial_lag_years); FID_CMP(nation); FID_CMP(contest_q); FID_CMP(protection_q);
    FID_CMP(is_seat); FID_CMP(seat_region); FID_CMP(has_market); FID_CMP(material_stock);
    FID_CMP(treasury); FID_CMP_ARR(scarcity_q); FID_CMP_ARR(scarcity_raw_q); FID_CMP(port_stock_q);
    FID_CMP(standing_army); FID_CMP(standing_army_owner); FID_CMP(mix_years); FID_CMP(civilisation);
    FID_CMP(population); FID_CMP(last_demography_year); FID_CMP(manpower_stock); FID_CMP(army_stock);
    FID_CMP(centres); FID_CMP(centres_razed); FID_CMP(urban_population); FID_CMP(network_supply_q);
    FID_CMP(creed_hold); FID_CMP(universal_creed); FID_CMP(creed_residue_culture);
    FID_CMP(creed_hold_years); FID_CMP(works_built); FID_CMP(work_capacity_mod);
    FID_CMP(work_manpower_mod); FID_CMP(work_reach_mod); FID_CMP(work_defence_mod);
    FID_CMP(work_industrial_mod);
    FID_CMP(survey_fuel_q); FID_CMP(survey_forest_q); // BL-1051: the span-open survey
    FID_CMP(industry_points);                         // BL-1041: the located stock
}
constexpr int k_region_fields = 54; // counts the FID_CMP lines above; keep them equal

/// Every `polity` field, one by one (same caveat as `region_fields`).
void polity_fields(const polity& a, const polity& b, field_census& out)
{
    FID_CMP(id); FID_CMP(culture); FID_CMP(capital); FID_CMP(aggression_q); FID_CMP_ARR(capacity);
    FID_CMP_ARR(progress_q); FID_CMP(cohesion_q); FID_CMP(industrial_year); FID_CMP(protection_q);
    FID_CMP(major); FID_CMP(parent); FID_CMP(universal_creed); FID_CMP(creed_adopted_year);
    FID_CMP(alive); FID_CMP(empire_mask); FID_CMP(empire_investing); FID_CMP(empire_progress_q);
    FID_CMP(exploration_mask); FID_CMP(exploration_investing); FID_CMP(exploration_progress_q);
    FID_CMP(overlord); FID_CMP(subject_kind); FID_CMP(navy_stock); FID_CMP(treaties_broken);
    FID_CMP_ARR(tree_mod_q); FID_CMP(tree_keys);
    // BL-1038's Industry triple and its fuel flag (merged beside BL-1036).
    FID_CMP(industry_mask); FID_CMP(industry_investing); FID_CMP(industry_progress_q);
    FID_CMP(industry_fuel_seen);
}
constexpr int k_polity_fields = 30;

#undef FID_CMP
#undef FID_CMP_ARR

template <class T, class Census>
field_census census_of(const std::vector<T>& a, const std::vector<T>& b, Census fields)
{
    field_census out;
    if (a.size() != b.size()) out["(row count)"] = 1;
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) fields(a[i], b[i], out);
    return out;
}

bool grudge_eq(const grudge& a, const grudge& b)
{
    if (a.from != b.from || a.to != b.to || a.score != b.score || a.peak != b.peak
     || a.event_count != b.event_count || a.events_kept != b.events_kept) return false;
    for (int k = 0; k < grudge_events_kept; ++k)
        if (a.events[k].year != b.events[k].year || a.events[k].region != b.events[k].region
         || a.events[k].kind != b.events[k].kind || a.events[k].magnitude != b.events[k].magnitude)
            return false;
    return true;
}
bool contact_eq(const contact& a, const contact& b)
{
    return a.from == b.from && a.to == b.to && a.first.year == b.first.year
        && a.first.region == b.first.region && a.first.kind == b.first.kind;
}
bool corridor_eq(const history_corridor& a, const history_corridor& b)
{
    return a.a == b.a && a.b == b.b && a.uses == b.uses && a.tier == b.tier;
}
bool dated_eq(const dated_object& a, const dated_object& b)
{
    return a.expires_year == b.expires_year && a.kind == b.kind && a.a == b.a && a.b == b.b;
}
bool civ_eq(const civilisation& a, const civilisation& b)
{
    return a.name == b.name && a.members == b.members && a.ethic.zeal == b.ethic.zeal
        && a.ethic.dominion == b.ethic.dominion && a.strain_q == b.strain_q
        && a.formed_year == b.formed_year;
}
bool creed_eq(const universal_creed& a, const universal_creed& b)
{
    return a.name == b.name && a.speech.onsets == b.speech.onsets && a.speech.vowels == b.speech.vowels
        && a.speech.codas == b.speech.codas && a.founded_year == b.founded_year
        && a.origin_polity == b.origin_polity && a.origin_region == b.origin_region;
}

/// "" when the two tables are equal row for row, else what differs first.
template <class T, class Eq>
std::string table_diff(const std::vector<T>& a, const std::vector<T>& b, Eq eq)
{
    if (a.size() != b.size())
        return "rows " + std::to_string(a.size()) + " vs " + std::to_string(b.size());
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!eq(a[i], b[i])) return "row " + std::to_string(i);
    return {};
}

std::string census_text(const field_census& c)
{
    std::string s;
    for (const auto& [field, n] : c)
        s += (s.empty() ? "" : ", ") + field + " x" + std::to_string(n);
    return s.empty() ? "-" : s;
}

std::vector<dated_object> sorted_dated(std::vector<dated_object> v)
{
    std::sort(v.begin(), v.end(), [](const dated_object& x, const dated_object& y) {
        if (x.a != y.a) return x.a < y.a;
        if (x.b != y.b) return x.b < y.b;
        if (x.kind != y.kind) return x.kind < y.kind;
        return x.expires_year < y.expires_year;
    });
    return v;
}

/// Size of the symmetric difference of two dated-object tables, as sets.
int dated_symdiff(const std::vector<dated_object>& a, const std::vector<dated_object>& b)
{
    const auto key = [](const dated_object& o) {
        return std::make_tuple(o.a, o.b, o.kind, o.expires_year);
    };
    std::vector<std::tuple<int32_t, int32_t, int32_t, int64_t>> ka, kb, d;
    for (const dated_object& o : a) ka.push_back(key(o));
    for (const dated_object& o : b) kb.push_back(key(o));
    std::sort(ka.begin(), ka.end());
    std::sort(kb.begin(), kb.end());
    std::set_symmetric_difference(ka.begin(), ka.end(), kb.begin(), kb.end(), std::back_inserter(d));
    return static_cast<int>(d.size());
}

/// Bound (non-aggression) pairs, symmetric difference.
int treaty_pair_symdiff(const std::vector<dated_object>& a, const std::vector<dated_object>& b)
{
    const auto pairs = [](const std::vector<dated_object>& v) {
        std::vector<std::pair<int32_t, int32_t>> p;
        for (const dated_object& o : v)
            if (o.kind == static_cast<int32_t>(treaty_clause::non_aggression)) p.push_back({o.a, o.b});
        std::sort(p.begin(), p.end());
        p.erase(std::unique(p.begin(), p.end()), p.end());
        return p;
    };
    const auto pa = pairs(a), pb = pairs(b);
    std::vector<std::pair<int32_t, int32_t>> d;
    std::set_symmetric_difference(pa.begin(), pa.end(), pb.begin(), pb.end(), std::back_inserter(d));
    return static_cast<int>(d.size());
}

struct run_out
{
    settlement_state  ss;
    history_sim_state hs;
    creed_state       cs; ///< the creeds as the run left them (BL-1040: the fold reads them)
};

history_sim_params span_params(const history_sim_params& ep, int64_t start, int64_t stop)
{
    history_sim_params hp = ep; // Exploration's own params: never the Empires derivation.
    hp.start_year      = start;
    hp.stop_year       = stop;
    hp.tick_bands[0]   = {stop, ep.tick_bands[0].step_years};
    hp.tick_band_count = 1;
    hp.trace_battles   = false;
    return hp;
}

/// C: Exploration's own call from the fixture's pre-Exploration capture, run
/// to @p stop, capturing its state at the top of @p capture. With
/// @p span_forces (BL-1040), the Digitisation span's Industry switch and open
/// year ride along -- inert before the open year, so the run is Exploration's
/// own until then and plays the span's forces after it.
run_out continued(const era_minus_one_fixture& fx, int64_t stop, int64_t capture, bool tier_seed = false,
                  const history_sim_params* span_forces = nullptr)
{
    history_sim_params hp = span_params(fx.exploration_params, fx.exploration_params.start_year, stop);
    hp.resume_polities  = &fx.pre_exploration_polities;
    hp.resume_grudges   = &fx.pre_exploration_grudges;
    hp.resume_contacts  = &fx.pre_exploration_contacts;
    hp.resume_corridors = &fx.pre_exploration_corridors;
    hp.capture_year     = capture;
    hp.resume_seeds_corridor_tier = tier_seed; // BL-1037; off unless --resume-tier asks
    if (span_forces != nullptr)
    {
        hp.industry_tree_enabled = span_forces->industry_tree_enabled;
        hp.industry_open_year    = span_forces->industry_open_year;
        // BL-1041: industry points open with the tree (inert before its open
        // year, so C to the top of 1660 is still Exploration's shipped run),
        // and the two ruled defaults (A, B) ride along -- both inert here, where
        // nothing was surveyed, but the forces are the span's.
        hp.industry_points_enabled              = span_forces->industry_points_enabled;
        hp.industry_survey_inherits_at_founding = span_forces->industry_survey_inherits_at_founding;
        hp.industry_fuel_gate_reads_survey      = span_forces->industry_fuel_gate_reads_survey;
    }
    run_out r;
    r.ss = fx.pre_exploration_settlement;
    r.cs = fx.pre_exploration_creeds;
    r.hs = run_history_sim(r.ss, &r.cs, fx.terrain.view(), fx.gw, fx.gh, hp, fx.exploration_seed,
                           nullptr, fx.works, nullptr);
    return r;
}

/// What a resume opens on. Everything else comes from the handoff struct.
struct resume_spec
{
    const std::vector<region>*           regions = nullptr; ///< the settlement's regions
    const std::vector<history_corridor>* record  = nullptr; ///< `resume_corridors`
    const std::vector<history_corridor>* live    = nullptr; ///< `resume_live_roads` (oracle) or null
    uint32_t                             seed    = 0;
    bool                                 tier_seed = false; ///< BL-1037's `resume_seeds_corridor_tier`
};

/// R: a second call opened on the handoff struct @p H at its own stop year,
/// on @p base's forces -- the Digitisation span's own captured params for
/// `--fidelity` (BL-1040), Exploration's for `--resume-tier` (unchanged).
run_out resume(const era_minus_one_fixture& fx, const exploration_output& H, const resume_spec& s,
               int64_t stop, int64_t capture, const history_sim_params& base)
{
    // Both 1200 anchors stay as Exploration set them (`consolidation_year`,
    // `near_home_cutoff_year`) -- the span's params never move them: only the
    // span moves.
    history_sim_params hp = span_params(base, H.stop_year, stop);
    hp.resume_polities         = &H.polities;
    hp.resume_grudges          = &H.grudges;
    hp.resume_contacts         = &H.contacts;
    hp.resume_corridors        = s.record;
    hp.resume_live_roads       = s.live;
    hp.resume_dated_objects    = &H.dated_objects;
    hp.resume_civilisations    = &H.civilisations;
    hp.resume_universal_creeds = &H.universal_creeds;
    hp.capture_year            = capture;
    hp.resume_seeds_corridor_tier = s.tier_seed;
    run_out r;
    r.ss.regions = *s.regions;
    r.cs.cultures = H.cultures; // "rebuild creed_state from cultures": the sim reads nothing else of it.
    r.hs = run_history_sim(r.ss, &r.cs, fx.terrain.view(), fx.gw, fx.gh, hp, s.seed,
                           nullptr, fx.works, nullptr);
    return r;
}

/// Treaties, the overlord graph and stocks after a round, x against y.
struct round_diff
{
    int          treaty_objects = 0; ///< dated objects, symmetric difference
    int          overlord       = 0; ///< polities whose overlord or subject kind differs
    int          alive          = 0; ///< polities whose liveness differs
    int          polity_rows    = 0; ///< |row count difference|
    int          navy           = 0; ///< polities whose navy_stock differs
    int          region_rows    = 0;
    int          regions_any    = 0; ///< regions with ANY stock or owner field differing
    field_census stock_fields;       ///< which region stock fields differ, and how often

    bool zero() const
    {
        return treaty_objects == 0 && overlord == 0 && alive == 0 && polity_rows == 0
            && navy == 0 && region_rows == 0 && regions_any == 0;
    }
};

round_diff compare_round(const run_out& x, const run_out& y)
{
    round_diff d;
    d.treaty_objects = dated_symdiff(x.hs.dated_objects, y.hs.dated_objects);
    const std::vector<polity>& px = x.hs.polities;
    const std::vector<polity>& py = y.hs.polities;
    d.polity_rows = static_cast<int>(px.size() > py.size() ? px.size() - py.size() : py.size() - px.size());
    for (std::size_t i = 0; i < std::min(px.size(), py.size()); ++i)
    {
        if (px[i].overlord != py[i].overlord || px[i].subject_kind != py[i].subject_kind) ++d.overlord;
        if (px[i].alive != py[i].alive) ++d.alive;
        if (px[i].navy_stock != py[i].navy_stock) ++d.navy;
    }
    const std::vector<region>& rx = x.ss.regions;
    const std::vector<region>& ry = y.ss.regions;
    d.region_rows = static_cast<int>(rx.size() > ry.size() ? rx.size() - ry.size() : ry.size() - rx.size());
    for (std::size_t i = 0; i < std::min(rx.size(), ry.size()); ++i)
    {
        const region& a = rx[i];
        const region& b = ry[i];
        field_census f;
        if (a.treasury != b.treasury)                       ++f["treasury"];
        if (a.material_stock != b.material_stock)           ++f["material_stock"];
        if (a.army_stock != b.army_stock)                   ++f["army_stock"];
        if (a.manpower_stock != b.manpower_stock)           ++f["manpower_stock"];
        if (a.port_stock_q != b.port_stock_q)               ++f["port_stock_q"];
        if (a.standing_army != b.standing_army)             ++f["standing_army"];
        if (a.standing_army_owner != b.standing_army_owner) ++f["standing_army_owner"];
        if (a.population != b.population)                   ++f["population"];
        if (a.nation != b.nation)                           ++f["nation"];
        if (a.industry_points != b.industry_points)         ++f["industry_points"]; // BL-1041
        if (!f.empty()) ++d.regions_any;
        for (const auto& [k, n] : f) d.stock_fields[k] += n;
    }
    return d;
}

std::string round_text(const round_diff& d)
{
    char buf[256];
    std::snprintf(buf, sizeof buf, "treaty objs %d, overlord %d, alive %d, navy %d, regions %d%s",
                  d.treaty_objects, d.overlord, d.alive, d.navy, d.regions_any,
                  (d.polity_rows || d.region_rows) ? " (ROW COUNTS DIFFER)" : "");
    return buf;
}

/// One 1960 state against another's (the continued run's, or the previous
/// variant's for the step-by-step attribution).
struct divergence
{
    int     owner_regions = 0; ///< regions whose owner differs (plus any row-count gap)
    int     living        = 0; ///< living polities in this run
    int     treaty_pairs  = 0; ///< bound pairs, symmetric difference
    int     overlord      = 0;
    int64_t battles       = 0; ///< this run's battles over its own span
    int64_t treasury      = 0; ///< this run's summed region treasury at its close
    int     doctrine[3]   = {}; ///< BL-1051: living polities at its close, coke / charcoal / neither
};

/// BL-1051 — the Fuel Doctrine side a polity holds: 0 Coke Smelting, 1
/// Charcoal Iron, 2 neither. By id: the harness names the pair it reports.
int fuel_doctrine_side(const polity& q)
{
    static const int coke = [] {
        for (int i = 0; i < io::industry_tree::node_count; ++i)
            if (std::strcmp(io::industry_tree::nodes[i].id, "IN-MT-1a") == 0) return i;
        return -1;
    }();
    static const int charcoal = [] {
        for (int i = 0; i < io::industry_tree::node_count; ++i)
            if (std::strcmp(io::industry_tree::nodes[i].id, "IN-MT-1b") == 0) return i;
        return -1;
    }();
    if (coke >= 0 && ((q.industry_mask >> coke) & 1ULL) != 0) return 0;
    if (charcoal >= 0 && ((q.industry_mask >> charcoal) & 1ULL) != 0) return 1;
    return 2;
}

divergence diverge(const run_out& v, const run_out& c)
{
    divergence d;
    for (const polity& q : v.hs.polities)
        if (q.alive) ++d.doctrine[fuel_doctrine_side(q)];
    const std::size_t n = std::min(v.ss.regions.size(), c.ss.regions.size());
    for (std::size_t i = 0; i < n; ++i)
        if (v.ss.regions[i].nation != c.ss.regions[i].nation) ++d.owner_regions;
    d.owner_regions += static_cast<int>(std::max(v.ss.regions.size(), c.ss.regions.size()) - n);
    for (const polity& q : v.hs.polities) if (q.alive) ++d.living;
    d.treaty_pairs = treaty_pair_symdiff(v.hs.dated_objects, c.hs.dated_objects);
    for (std::size_t i = 0; i < std::min(v.hs.polities.size(), c.hs.polities.size()); ++i)
        if (v.hs.polities[i].overlord != c.hs.polities[i].overlord) ++d.overlord;
    d.battles = v.hs.battles;
    for (const region& r : v.ss.regions) d.treasury += r.treasury;
    return d;
}

struct seed_fidelity
{
    uint32_t seed = 0;
    bool     ran  = false;
    std::string skip_reason;

    // Gate 1 -- the opening against the struct.
    field_census open_regions, open_polities;
    std::vector<std::string> open_tables; ///< "<table>: <first difference>" per table that differs
    int  open_living = 0;       ///< living polities in the handoff (each noted founded once)
    int  open_dead   = 0;       ///< polities the handoff carries dead (never noted founded)
    int  open_founded_dead = 0; ///< founded events at the open for a dead polity (must be 0)
    int  expl_ghosts       = 0; ///< polities dead at 1200 that Exploration's own open carries
    int  expl_founded_dead = 0; ///< ... and notes founded (must be 0)
    bool gate1 = false;

    // The entry state the handoff differs from the continued run in.
    field_census entry_regions, entry_polities;
    std::vector<std::string> entry_tables;
    int  record_rows_h = 0, record_rows_c = 0;         ///< corridor record rows, handoff vs continued
    int  live_edges_h  = 0, live_edges_c  = 0;         ///< live road edges at each opening
    int  live_count_diff = 0, live_tier_below = 0, live_tier_above = 0, live_missing = 0;

    // Gate 2 -- one network-neutralised round.
    round_diff neutral_round;
    bool gate2 = false;
    round_diff real_round; ///< the real resume's one round, reported

    // The 1960 divergence, V0..V5 against the continued run, each step
    // against the variant before it, and the continued run's own numbers.
    // BL-1051 added V5, the span-open survey, as its own step.
    static constexpr int k_variants = 6;
    divergence v[k_variants];
    divergence step[k_variants]; ///< step[k] = V_k against V_{k-1} (step[0] unused)
    int        c_living = 0;
    int64_t    c_battles = 0;
    int64_t    c_treasury = 0;
    int        c_doctrine[3] = {}; ///< BL-1051: the continued run's Fuel Doctrine split
    double     seconds = 0.0;

    // Gate 3 (BL-1040) -- the shipped span against V5, the real resume.
    std::vector<std::string> span_params_issues; ///< the captured params are not the span's derivation
    field_census span_regions, span_polities;
    std::vector<std::string> span_tables;        ///< "<table>: <first difference>" per table that differs
    int64_t span_rounds = 0;
    double  span_ms     = 0.0;                   ///< generation's own span call, wall clock
    int     controls_untried = 0;                ///< BL-1053 negative controls with no candidate here
    bool gate3 = false;
};

bool flow_eq(const trade_flow& a, const trade_flow& b)
{
    return a.seller == b.seller && a.buyer == b.buyer && a.good == b.good && a.volume_q == b.volume_q;
}
bool holding_eq(const polity_holdings& a, const polity_holdings& b)
{
    return a.polity == b.polity && a.regions == b.regions;
}

/// Is @p dp the Digitisation span's derivation, as far as the params can say?
/// Exploration's base (the upkeep step and want weight it sets, its band's
/// step), the span moved to open on the handoff's close, both 1200 anchors
/// untouched, the Industry tree open from the span's own open, BL-1037's
/// switch off, and a seed that is neither Exploration's nor a re-use.
std::vector<std::string> span_params_issues(const history_sim_params& dp, const history_sim_params& ep,
                                            const exploration_output& H, const world_params& wp,
                                            uint32_t dseed, uint32_t eseed)
{
    std::vector<std::string> out;
    const auto need = [&](bool ok, const char* what) { if (!ok) out.push_back(what); };
    need(dp.start_year == H.stop_year, "the span does not open on the handoff's close");
    need(dp.stop_year == wp.digitisation_stop_year, "the span does not close on digitisation_stop_year");
    need(dp.tick_band_count == 1 && dp.tick_bands[0].until_year == dp.stop_year
             && dp.tick_bands[0].step_years == ep.tick_bands[0].step_years,
         "the span is not one band at Exploration's step");
    need(dp.consolidation_year == ep.consolidation_year && dp.near_home_cutoff_year == ep.near_home_cutoff_year
             && dp.consolidation_year == wp.empires_stop_year,
         "an anchor moved off 1200");
    need(dp.exploration_upkeep_enabled == ep.exploration_upkeep_enabled && dp.w_want_q == ep.w_want_q,
         "the base is not Exploration's derivation");
    need(dp.industry_tree_enabled && dp.industry_open_year == dp.start_year,
         "the Industry tree is not open from the span's open");
    need(!ep.industry_tree_enabled, "the Industry tree is open in Exploration's own span");
    // BL-1041: the industry-point switch and its two ruled defaults (A, the
    // founding inherits the survey; B, every Industry fuel read takes it) are
    // the span's own forces -- ON here, OFF in Exploration's params.
    need(dp.industry_points_enabled && dp.industry_survey_inherits_at_founding
             && dp.industry_fuel_gate_reads_survey,
         "industry points or a ruled survey default (A, B) is off in the span");
    need(!ep.industry_points_enabled && !ep.industry_survey_inherits_at_founding
             && !ep.industry_fuel_gate_reads_survey,
         "industry points or a ruled survey default (A, B) is on in Exploration's own span");
    need(!dp.resume_seeds_corridor_tier, "BL-1037's switch is on");
    need(dseed == digitisation_sim_seed(wp) && dseed != eseed, "the span's seed is not its own fold");

    // BL-1053: EVERY EMPIRES-ONLY SETTING IS OFF. The two base checks above
    // read two fields; a span derived from the Empires round's params would
    // pass them while running the Empires verb set, and a wrong derivation
    // that is quiet in the first round is invisible to gates 1-3. So every
    // field `era_minus_one_sim_params` moves off the struct default (for this
    // world) must sit AT the default in the span's params -- the settings
    // Exploration's derivation never turns on. The list is the Empires
    // derivation's own override list; the ones it only reads from the world
    // (the turbulence lean) are checked the same way.
    {
        const history_sim_params E = era_minus_one_sim_params(wp);
        const history_sim_params B{};
        int empires_only = 0;
#define EMPIRES_ONLY(f)                                                                     \
        do {                                                                                \
            if (!(E.f == B.f)) {                                                            \
                ++empires_only;                                                             \
                if (!(dp.f == B.f)) out.push_back("an Empires-only setting is on: " #f);    \
            }                                                                               \
        } while (0)
        EMPIRES_ONLY(amphibious_weight_crossing);
        EMPIRES_ONLY(sea_legs_ration_q);
        EMPIRES_ONLY(sea_legs_floor_q);
        EMPIRES_ONLY(sea_legs_port_q);
        EMPIRES_ONLY(trade_income_per_class);
        EMPIRES_ONLY(army_upkeep_per_1000_heads);
        EMPIRES_ONLY(unpaid_army_disband_q);
        EMPIRES_ONLY(road_build_material_cost);
        EMPIRES_ONLY(supply_upgrade_material_cost);
        EMPIRES_ONLY(supply_upgrade_reach_gain_q);
        EMPIRES_ONLY(supply_upgrade_threshold_q);
        EMPIRES_ONLY(secession_supply_floor_q);
        EMPIRES_ONLY(secession_min_regions);
        EMPIRES_ONLY(universal_creed_humbled_cohesion_q);
        EMPIRES_ONLY(universal_creed_min_trade_links);
        EMPIRES_ONLY(universal_creed_network_floor_q);
        EMPIRES_ONLY(universal_creed_hold_years);
        EMPIRES_ONLY(universal_creed_convert_supply_q);
        EMPIRES_ONLY(universal_creed_alien_penalty_q);
        EMPIRES_ONLY(schism_min_regions);
        EMPIRES_ONLY(centre_chain_reach);
        EMPIRES_ONLY(centre_reach_rebate_q);
        EMPIRES_ONLY(centre_reach_rebate_cap_q);
        EMPIRES_ONLY(centre_reach_min_centres);
        EMPIRES_ONLY(w_aggr_q);
        EMPIRES_ONLY(w_fear_q);
        EMPIRES_ONLY(fear_reference);
        EMPIRES_ONLY(turbulence_lean);
#undef EMPIRES_ONLY
        // A list that checks nothing is a stale list, not a pass.
        need(empires_only > 0, "no Empires-only setting found to check (the list above is stale)");
    }
    return out;
}

/// BL-1053: WORLD SETUP READS THE CLOSE IT BUILDS ON. What world setup was
/// handed at each of its four reads of the history -- the grudges sentiment
/// is seeded from, the corridor record roads are stamped from, the record
/// junction markets are counted over, and the polity-indexed treasuries
/// nations are credited -- against @p close, the last span's own fold. The
/// treasuries are summed here from `close.regions` under the flag, by hand,
/// rather than through generation's helper, so the check is not the code it
/// checks. "" per table when equal; the vector holds one "<table>: <first
/// difference>" per table that differs, and is empty when setup read the
/// close table for table.
std::vector<std::string> setup_diff(const era_minus_one_fixture& fx, const exploration_output& close)
{
    std::vector<std::string> out;
    const auto note = [&](const char* table, const std::string& d) {
        if (!d.empty()) out.push_back(std::string(table) + ": " + d);
    };
    note("grudges",             table_diff(fx.setup_grudges, close.grudges, grudge_eq));
    note("stamped roads",       table_diff(fx.setup_corridors, close.surviving_corridors, corridor_eq));
    note("junction corridors",  table_diff(fx.setup_junction_corridors, close.surviving_corridors, corridor_eq));

    std::vector<int64_t> chests;
    for (const region& rg : close.regions)
    {
        if (rg.nation < 0 || rg.treasury <= 0) continue;
        const std::size_t pol = static_cast<std::size_t>(rg.nation);
        if (pol >= chests.size()) chests.resize(pol + 1, 0);
        chests[pol] += rg.treasury;
    }
    note("treasuries", table_diff(fx.setup_polity_treasuries, chests,
                                  [](int64_t a, int64_t b) { return a == b; }));
    return out;
}

int run(const std::vector<uint32_t>& seeds, const world_gen_config& cfg_in, works_registry& works)
{
    // BL-1040: generation runs the Digitisation span and stops right after it
    // -- the 1660 handoff and the span's own 1960 close are all this mode
    // reads, and nothing of world setup is built.
    world_gen_config cfg = cfg_in;
    cfg.stop_after_digitisation = true;

    std::printf("=== digitisation_sim_harness --fidelity (BL-1036, BL-1040) - the Digitisation span, resumed from "
                "exploration_output ===\n");
    std::printf("handoff: the shipped world (world_params defaults) with the Digitisation span on, stopped after it.\n"
                "C = Exploration's own call continued from the fixture, carrying the span's forces from 1660;\n"
                "R = a second call opened on the struct, on the span's own captured params.\n"
                "field lists: region %d fields, polity %d fields, every row of every other resumed table.\n\n",
                k_region_fields, k_polity_fields);
    std::fflush(stdout);

    std::vector<seed_fidelity> rows;
    for (uint32_t seed : seeds)
    {
        const auto t0 = std::chrono::steady_clock::now();
        seed_fidelity row;
        row.seed = seed;
        std::fprintf(stderr, "[fidelity] seed %u generating\n", seed);

        world_params wp{};
        wp.seed = seed;
        wp.digitisation_span_enabled = true; // BL-1040: the span is the resume's real consumer
        generation_report     rep;
        era_minus_one_fixture fx;
        (void)make_hard_coded_world(wp, &rep, cfg, /*progress=*/nullptr, &works, &fx);
        if (!fx.exploration_ran)
        {
            row.skip_reason = "Exploration did not run";
            rows.push_back(row);
            continue;
        }
        if (!fx.digitisation_ran)
        {
            // Exploration ran and the switch was on, so the span MUST have:
            // that is its run predicate. Not a skip -- a failure of R3.
            row.ran = true;
            row.span_params_issues.push_back("Exploration ran with the switch on and the span did not");
            row.open_tables.push_back("the Digitisation span did not run");
            rows.push_back(row);
            continue;
        }
        const exploration_output& H = fx.exploration_handoff;
        const history_sim_params& ep = fx.exploration_params;
        const history_sim_params& dp = fx.digitisation_params; // the span's own, as generation ran it
        if (H.stop_year != 1660 || ep.consolidation_year != ep.start_year
         || ep.near_home_cutoff_year != ep.start_year || !fx.pre_exploration_settlement.pending_foundings.empty())
        {
            row.skip_reason = "the handoff is not the shipped 1660 one, the anchors are not Exploration's "
                              "start year, or a founding is still pending";
            rows.push_back(row);
            continue;
        }
        row.ran = true;
        row.span_params_issues = span_params_issues(dp, ep, H, wp, fx.digitisation_seed, fx.exploration_seed);
        {
            // BL-1053 negative control: the span's own params with ONE Empires
            // setting switched on must be caught, by name.
            history_sim_params wrong = dp;
            wrong.trade_income_per_class = era_minus_one_sim_params(wp).trade_income_per_class;
            const std::vector<std::string> caught =
                span_params_issues(wrong, ep, H, wp, fx.digitisation_seed, fx.exploration_seed);
            bool named = false;
            for (const std::string& t : caught) named = named || t.find("trade_income_per_class") != std::string::npos;
            if (!named)
                row.span_params_issues.push_back("negative control: an Empires setting switched on was not caught");
        }
        row.span_rounds = fx.digitisation_rounds;
        row.span_ms     = static_cast<double>(fx.ms_digitisation);
        const int64_t open = H.stop_year;      // 1660
        const int64_t one  = open + dp.tick_bands[0].step_years; // one round: 1664
        const int64_t stop = dp.stop_year;     // 1960
        const uint32_t own_seed = fx.digitisation_seed; // BL-1040: the span's own fold

        // ---- C: the continued run, carrying the span's forces from 1660 -------
        std::fprintf(stderr, "[fidelity] seed %u continued runs\n", seed);
        const run_out c_one  = continued(fx, one, open, false, &dp);
        const run_out c_1960 = continued(fx, stop, INT64_MIN, false, &dp);
        const history_sim_capture& C = c_one.hs.capture;

        // ---- Gate 1: the real resume's opening, and its one round ------------
        resume_spec real;
        real.regions = &H.regions;
        real.record  = &H.surviving_corridors;
        real.seed    = fx.exploration_seed;
        const run_out r_one = resume(fx, H, real, one, open, dp);
        const history_sim_capture& O = r_one.hs.capture;
        {
            row.open_regions  = census_of(O.regions, H.regions, region_fields);
            row.open_polities = census_of(O.polities, H.polities, polity_fields);
            const auto note = [&](const char* table, const std::string& d) {
                if (!d.empty()) row.open_tables.push_back(std::string(table) + ": " + d);
            };
            note("grudges",          table_diff(O.grudges, H.grudges, grudge_eq));
            note("contacts",         table_diff(O.contacts, H.contacts, contact_eq));
            note("corridor record",  table_diff(O.supply_corridors, H.surviving_corridors, corridor_eq));
            note("dated objects",    table_diff(O.dated_objects, H.dated_objects, dated_eq));
            note("civilisations",    table_diff(O.civilisations, H.civilisations, civ_eq));
            note("universal creeds", table_diff(O.universal_creeds, H.universal_creeds, creed_eq));
            if (!O.trade_flows.empty())
                row.open_tables.push_back("trade flows: " + std::to_string(O.trade_flows.size())
                                          + " rows at the open (round 1 rebuilds them; none may be carried)");
            int owner_mismatch = 0;
            for (std::size_t i = 0; i < O.owner.size() && i < O.regions.size(); ++i)
                if (O.owner[i] != O.regions[i].nation) ++owner_mismatch;
            if (owner_mismatch > 0)
                row.open_tables.push_back("owner map vs region::nation: " + std::to_string(owner_mismatch));

            // The event layer: a resumed open notes the LIVING as founded,
            // each once, and never a polity the prior span already ended.
            int living = 0, founded_living = 0, founded_dead = 0;
            for (const polity& q : H.polities) if (q.alive) ++living;
            for (const lapse_event& e : r_one.hs.events)
            {
                if (e.year != open || e.kind != static_cast<uint8_t>(lapse_event_kind::founded)) continue;
                if (e.polity < H.polities.size() && H.polities[e.polity].alive) ++founded_living;
                else ++founded_dead;
            }
            row.open_living = living;
            row.open_dead   = static_cast<int>(H.polities.size()) - living;
            row.open_founded_dead = founded_dead;

            // The same rule on Exploration's own 1200 resume (the continued
            // run IS that call): the Empires span's dead are carried and no
            // longer noted founded -- the ghosts the shipped replay lost.
            const int64_t expl_open = fx.exploration_params.start_year;
            for (const polity& q : fx.pre_exploration_polities) if (!q.alive) ++row.expl_ghosts;
            for (const lapse_event& e : c_one.hs.events)
            {
                if (e.year != expl_open || e.kind != static_cast<uint8_t>(lapse_event_kind::founded)) continue;
                if (e.polity < fx.pre_exploration_polities.size() && !fx.pre_exploration_polities[e.polity].alive)
                    ++row.expl_founded_dead;
            }
            if (row.expl_founded_dead != 0)
                row.open_tables.push_back("Exploration's 1200 open noted " + std::to_string(row.expl_founded_dead)
                                          + " dead polities founded");
            if (founded_dead != 0 || founded_living != living)
                row.open_tables.push_back("founded events at the open: " + std::to_string(founded_living)
                                          + " living of " + std::to_string(living) + ", "
                                          + std::to_string(founded_dead) + " for polities already dead");
            row.gate1 = O.captured && O.year == open && row.open_regions.empty()
                     && row.open_polities.empty() && row.open_tables.empty();
        }

        // ---- The entry state: the handoff against the continued run at 1660 --
        {
            row.entry_regions  = census_of(H.regions, C.regions, region_fields);
            row.entry_polities = census_of(H.polities, C.polities, polity_fields);
            const auto note = [&](const char* table, const std::string& d) {
                if (!d.empty()) row.entry_tables.push_back(std::string(table) + ": " + d);
            };
            note("grudges",          table_diff(H.grudges, C.grudges, grudge_eq));
            note("contacts",         table_diff(H.contacts, C.contacts, contact_eq));
            std::vector<dated_object> c_dated = C.dated_objects;
            expire_dated_objects(c_dated, open); // what round 1 does before any read
            note("dated objects (as sets, after the 1660 expiry)",
                 dated_symdiff(H.dated_objects, c_dated) == 0 ? std::string{}
                     : std::to_string(dated_symdiff(H.dated_objects, c_dated)) + " objects");
            note("civilisations",    table_diff(H.civilisations, C.civilisations, civ_eq));
            note("universal creeds", table_diff(H.universal_creeds, C.universal_creeds, creed_eq));
            row.record_rows_h = static_cast<int>(H.surviving_corridors.size());
            row.record_rows_c = static_cast<int>(C.supply_corridors.size());
            row.live_edges_h  = static_cast<int>(O.live_roads.size());
            row.live_edges_c  = static_cast<int>(C.live_roads.size());
            std::map<std::pair<int, int>, const history_corridor*> live_h;
            for (const history_corridor& e : O.live_roads) live_h[{e.a, e.b}] = &e;
            for (const history_corridor& e : C.live_roads)
            {
                const auto it = live_h.find({e.a, e.b});
                if (it == live_h.end()) { ++row.live_missing; continue; }
                if (it->second->uses != e.uses) ++row.live_count_diff;
                if (it->second->tier < e.tier) ++row.live_tier_below;
                if (it->second->tier > e.tier) ++row.live_tier_above;
            }
        }

        // ---- Gate 2: one round with the named network sources neutralised ---
        // The corridor record and the live road counts the resume prices on
        // are the continued run's own at 1660, and the 1660 close's market
        // stamp (BL-910: `has_market` is set on every living capital at a
        // span's close, and nowhere else) is undone to the continued run's.
        std::vector<region> neutral_regions = H.regions;
        if (neutral_regions.size() == C.regions.size())
            for (std::size_t i = 0; i < neutral_regions.size(); ++i)
                neutral_regions[i].has_market = C.regions[i].has_market;
        resume_spec neutral;
        neutral.regions = &neutral_regions;
        neutral.record  = &C.supply_corridors;
        neutral.live    = &C.live_roads;
        neutral.seed    = fx.exploration_seed;
        {
            const run_out n_one = resume(fx, H, neutral, one, INT64_MIN, dp);
            row.neutral_round = compare_round(n_one, c_one);
            row.gate2 = row.neutral_round.zero();
            row.real_round = compare_round(r_one, c_one);
        }

        // ---- The 1960 divergence, one source added back at a time ------------
        std::fprintf(stderr, "[fidelity] seed %u 1960 variants\n", seed);
        {
            resume_spec v1 = neutral;            // + the 1660 close's market stamp
            v1.regions = &H.regions;
            resume_spec v2 = v1;                 // + the 1660 corridor record, unfiltered,
            v2.record = &fx.exploration_state.supply_corridors; // live counts seeded from it
            v2.live   = nullptr;
            resume_spec v3 = v2;                 // + the dead-filter: the handoff as it is
            v3.record = &H.surviving_corridors;
            resume_spec v4 = v3;                 // + the span's own seed
            v4.seed = own_seed;
            // BL-1051: + the span-open survey -- the real resume. Generation
            // surveys every region's tiles between the struct copy and the
            // call; a fixture carries no tiles, so V5 opens on the table
            // generation captured (`digitisation_open_regions`), which the
            // gate below holds to "the handoff plus exactly the survey".
            const std::vector<region>& opened = fx.digitisation_open_regions.empty()
                                                    ? H.regions : fx.digitisation_open_regions;
            resume_spec v5 = v4;
            v5.regions = &opened;
            const resume_spec* specs[seed_fidelity::k_variants] = { &neutral, &v1, &v2, &v3, &v4, &v5 };
            run_out prev;
            for (int k = 0; k < seed_fidelity::k_variants; ++k)
            {
                run_out cur = resume(fx, H, *specs[k], stop, INT64_MIN, dp);
                row.v[k] = diverge(cur, c_1960);
                if (k > 0) row.step[k] = diverge(cur, prev);
                prev = std::move(cur);
            }

            // ---- Gate 3 (BL-1040): the shipped span IS V5 ---------------------
            // `prev` is V5: the handoff as it is plus the span-open survey, on
            // the span's own seed and params, to the span's own stop. Folded by
            // the span's own rule, it must be generation's 1960 close exactly.
            const digitisation_output V = make_digitisation_output(prev.ss, prev.hs, &prev.cs);
            const digitisation_output& S = fx.digitisation_handoff;
            const history_sim_state&   T = fx.digitisation_state;
            row.span_regions  = census_of(V.regions, S.regions, region_fields);
            row.span_polities = census_of(V.polities, S.polities, polity_fields);
            const auto note = [&](const char* table, const std::string& d) {
                if (!d.empty()) row.span_tables.push_back(std::string(table) + ": " + d);
            };
            // BL-1051: the table the span opened on is the handoff plus the
            // survey's two fields and nothing else, and the survey saw every
            // region (none left at -1).
            {
                if (fx.digitisation_open_regions.empty())
                    note("span opening", "generation captured no opening table");
                std::vector<region> expect = H.regions;
                int unsurveyed = 0;
                if (expect.size() == opened.size())
                    for (std::size_t i = 0; i < expect.size(); ++i)
                    {
                        expect[i].survey_fuel_q   = opened[i].survey_fuel_q;
                        expect[i].survey_forest_q = opened[i].survey_forest_q;
                        if (opened[i].survey_fuel_q < 0 || opened[i].survey_forest_q < 0) ++unsurveyed;
                    }
                const field_census beyond = census_of(expect, opened, region_fields);
                if (!beyond.empty())
                    note("span opening", "differs from the handoff beyond the survey: " + census_text(beyond));
                if (unsurveyed > 0)
                    note("span opening", std::to_string(unsurveyed) + " regions left unsurveyed");
            }
            note("grudges",           table_diff(V.grudges, S.grudges, grudge_eq));
            note("contacts",          table_diff(V.contacts, S.contacts, contact_eq));
            note("surviving network", table_diff(V.surviving_corridors, S.surviving_corridors, corridor_eq));
            note("dated objects",     table_diff(V.dated_objects, S.dated_objects, dated_eq));
            note("trade flows",       table_diff(V.trade_flows, S.trade_flows, flow_eq));
            note("holdings",          table_diff(V.holdings, S.holdings, holding_eq));
            note("civilisations",     table_diff(V.civilisations, S.civilisations, civ_eq));
            note("universal creeds",  table_diff(V.universal_creeds, S.universal_creeds, creed_eq));
            {
                // The culture table, by the sim's own row comparator: V's copy
                // validated against the shipped close's table as the "live" one.
                creed_state shipped_cultures;
                shipped_cultures.cultures = S.cultures;
                std::string why;
                if (V.culture_count != S.culture_count)
                    note("cultures", "count " + std::to_string(V.culture_count) + " vs " + std::to_string(S.culture_count));
                else if (!exploration_output_valid(V, &why, &shipped_cultures))
                    note("cultures (or V's own validity)", why);
            }
            if (V.start_year != S.start_year || V.stop_year != S.stop_year)
                note("span years", std::to_string(V.start_year) + "-" + std::to_string(V.stop_year) + " vs "
                                   + std::to_string(S.start_year) + "-" + std::to_string(S.stop_year));
            if (prev.hs.battles != T.battles || prev.hs.conquests != T.conquests || prev.hs.foundings != T.foundings
             || prev.hs.subjections_formed != T.subjections_formed || prev.hs.subjections_freed != T.subjections_freed)
                note("counters", "battles " + std::to_string(prev.hs.battles) + " vs " + std::to_string(T.battles)
                                 + ", conquests " + std::to_string(prev.hs.conquests) + " vs " + std::to_string(T.conquests)
                                 + ", foundings " + std::to_string(prev.hs.foundings) + " vs " + std::to_string(T.foundings));
            // And the shipped close passed its own validator, against the
            // value it resumed from and the stop year it was asked to reach
            // (BL-1053), on generation's path.
            {
                std::string why;
                if (!digitisation_output_valid(S, &why, nullptr, &H, wp.digitisation_stop_year))
                    note("digitisation_output_valid", why);
                if (rep.handoff_invalid)
                    note("generation recorded a handoff violation", rep.handoff_violation);
            }
            // BL-1053: THE CONTINUITY RULES HOLD, AND CAN FAIL. Three negative
            // controls, each of which the validator must refuse FOR ITS OWN
            // REASON (the message names the rule): a resumed contact whose
            // first event moved and a stop year one short, on copies of the
            // shipped 1960 close; and a resumed standing object dropped with
            // no break or release, on the ONE-ROUND close (the real resume
            // run to 1664, folded by the span's rule) -- because a treaty's
            // term is 80 years, no object standing at 1660 can still stand at
            // 1960, so the object rule only has material on a close inside
            // one term of the open. That 1664 close must itself pass, against
            // the 1660 value and its own stop year: the positive half. A
            // control with no candidate on this seed is noted, not failed.
            {
                const auto refuses = [&](const digitisation_output& bad, int64_t stop, const char* needle) {
                    std::string why;
                    return !digitisation_output_valid(bad, &why, nullptr, &H, stop)
                        && why.find(needle) != std::string::npos;
                };
                const auto alive_in_s = [&](std::size_t p) { return p < S.polities.size() && S.polities[p].alive; };
                const digitisation_output R1 = make_digitisation_output(r_one.ss, r_one.hs, &r_one.cs);
                {
                    std::string why;
                    if (!digitisation_output_valid(R1, &why, &r_one.cs, &H, one))
                        note("the one-round close (1664) fails its validator", why);
                }

                bool contact_tried = false;
                for (std::size_t i = 0; i < S.contacts.size() && !contact_tried; ++i)
                {
                    const contact& c = S.contacts[i];
                    if (!alive_in_s(c.from) || !alive_in_s(c.to)) continue;
                    bool resumed = false;
                    for (const contact& h : H.contacts) if (h.from == c.from && h.to == c.to) { resumed = true; break; }
                    if (!resumed) continue;
                    contact_tried = true;
                    digitisation_output bad = S;
                    bad.contacts[i].first.year += 1;
                    if (!refuses(bad, wp.digitisation_stop_year, "contact"))
                        note("negative control", "a resumed contact's moved first event was not refused");
                }
                if (!contact_tried) ++row.controls_untried;

                bool object_tried = false;
                for (std::size_t i = 0; i < R1.dated_objects.size() && !object_tried; ++i)
                {
                    const dated_object& d = R1.dated_objects[i];
                    if (d.kind == static_cast<int32_t>(treaty_clause::trade_access)   // flows would fail first
                     || d.kind == static_cast<int32_t>(treaty_clause::tribute)) continue;
                    bool resumed = false;
                    for (const dated_object& h : H.dated_objects)
                        if (h.a == d.a && h.b == d.b && h.kind == d.kind && h.expires_year == d.expires_year)
                        { resumed = true; break; }
                    if (!resumed) continue;
                    const auto broke = [&](int32_t p) {
                        return p >= 0 && static_cast<std::size_t>(p) < R1.polities.size()
                            && static_cast<std::size_t>(p) < H.polities.size()
                            && R1.polities[static_cast<std::size_t>(p)].treaties_broken
                                   > H.polities[static_cast<std::size_t>(p)].treaties_broken;
                    };
                    if (broke(d.a) || broke(d.b)) continue;
                    object_tried = true;
                    digitisation_output bad = R1;
                    bad.dated_objects.erase(bad.dated_objects.begin() + static_cast<std::ptrdiff_t>(i));
                    if (!refuses(bad, one, "dated object"))
                        note("negative control", "a resumed standing object dropped without cause was not refused");
                }
                if (!object_tried) ++row.controls_untried;

                if (!refuses(S, S.stop_year - 1, "stop year"))
                    note("negative control", "a close one year past its asked-for stop year was not refused");
            }
            row.gate3 = row.span_params_issues.empty() && row.span_regions.empty()
                     && row.span_polities.empty() && row.span_tables.empty();
        }
        for (const polity& q : c_1960.hs.polities)
            if (q.alive) { ++row.c_living; ++row.c_doctrine[fuel_doctrine_side(q)]; }
        for (const region& r : c_1960.ss.regions) row.c_treasury += r.treasury;
        row.c_battles = c_1960.hs.battles - fx.exploration_state.battles;

        row.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("seed %u  gate1 %s  gate2 %s  gate3 %s  (%.1f s; the span itself %lld rounds, %.0f ms)\n", seed,
                    row.gate1 ? "PASS" : "FAIL", row.gate2 ? "PASS" : "FAIL", row.gate3 ? "PASS" : "FAIL",
                    row.seconds, static_cast<long long>(row.span_rounds), row.span_ms);
        std::fflush(stdout);
        rows.push_back(row);
    }

    // ---- The report ---------------------------------------------------------
    int ran = 0, g1 = 0, g2 = 0;
    std::printf("\n=== GATE 1 (R4): the resumed opening equals exploration_output, field for field ===\n");
    for (const seed_fidelity& r : rows)
    {
        if (!r.ran) { std::printf("  %5u  SKIPPED: %s\n", r.seed, r.skip_reason.c_str()); continue; }
        ++ran;
        if (r.gate1) ++g1;
        std::printf("  %5u  %s  regions: %s | polities: %s | founded at the open: %d living, %d dead carried "
                    "and not noted (1200: %d dead not noted)", r.seed, r.gate1 ? "PASS" : "FAIL",
                    census_text(r.open_regions).c_str(), census_text(r.open_polities).c_str(),
                    r.open_living, r.open_dead, r.expl_ghosts);
        for (const std::string& t : r.open_tables) std::printf(" | %s", t.c_str());
        std::printf("\n");
    }

    std::printf("\n=== ENTRY: where the handoff differs from the continued run at the top of 1660 (reported) ===\n");
    std::printf("  (record = the corridor rows income and trade price on; live = the road counts reach reads)\n");
    for (const seed_fidelity& r : rows)
    {
        if (!r.ran) continue;
        std::printf("  %5u  regions: %s | polities: %s", r.seed, census_text(r.entry_regions).c_str(),
                    census_text(r.entry_polities).c_str());
        for (const std::string& t : r.entry_tables) std::printf(" | %s", t.c_str());
        std::printf("\n         record rows H %d vs C %d | live edges H %d vs C %d: missing from H %d, count "
                    "differs %d, H tier below C %d, above %d\n",
                    r.record_rows_h, r.record_rows_c, r.live_edges_h, r.live_edges_c, r.live_missing,
                    r.live_count_diff, r.live_tier_below, r.live_tier_above);
    }

    std::printf("\n=== GATE 2 (R5): one round, named network sources neutralised, equals the continued run's 1660 round ===\n");
    std::printf("  (treaties = dated objects; overlord graph = overlord + subject kind; stocks = region treasury,\n"
                "   material, army, manpower, port, standing army, population, owner, and polity navy)\n");
    for (const seed_fidelity& r : rows)
    {
        if (!r.ran) continue;
        if (r.gate2) ++g2;
        std::printf("  %5u  %s  neutralised: %s%s%s\n", r.seed, r.gate2 ? "PASS" : "FAIL",
                    round_text(r.neutral_round).c_str(),
                    r.neutral_round.stock_fields.empty() ? "" : " | fields: ",
                    r.neutral_round.stock_fields.empty() ? "" : census_text(r.neutral_round.stock_fields).c_str());
        std::printf("         the real resume (reported, the named sources' one-round footprint): %s%s%s\n",
                    round_text(r.real_round).c_str(),
                    r.real_round.stock_fields.empty() ? "" : " | fields: ",
                    r.real_round.stock_fields.empty() ? "" : census_text(r.real_round.stock_fields).c_str());
    }

    std::printf("\n=== 1960 DIVERGENCE against the continued run (a 1200-network run carrying the span's forces from\n"
                "    1660), attributed by source (REPORTED) ===\n");
    std::printf("  V0 all named sources neutralised, Exploration's seed | V1 + the 1660 close's market stamp |\n"
                "  V2 + the 1660 corridor record, unfiltered, live counts seeded from it | V3 + the dead-filter\n"
                "  (the handoff as it is) | V4 + the span's own seed | V5 + the span-open survey (BL-1051: the\n"
                "  real resume; C never has it, so V5 against V4 is the survey's own footprint)\n");
    std::printf("  Against C: regions owned differently, living polities, treaty pairs differing, battles 1660-1960,\n"
                "  summed region treasury (x1e3), the Fuel Doctrine coke/charcoal/neither over living polities.\n"
                "  By source: each variant against the one before it (the source's own footprint, 'regions owned\n"
                "  differently / treaty pairs differing').\n");
    for (const seed_fidelity& r : rows)
    {
        if (!r.ran) continue;
        std::printf("  %5u  owner  V0..V5 %4d %4d %4d %4d %4d %4d | living C %3d: %3d %3d %3d %3d %3d %3d | treaty pairs "
                    "%3d %3d %3d %3d %3d %3d\n",
                    r.seed, r.v[0].owner_regions, r.v[1].owner_regions, r.v[2].owner_regions,
                    r.v[3].owner_regions, r.v[4].owner_regions, r.v[5].owner_regions,
                    r.c_living, r.v[0].living, r.v[1].living, r.v[2].living, r.v[3].living, r.v[4].living,
                    r.v[5].living,
                    r.v[0].treaty_pairs, r.v[1].treaty_pairs, r.v[2].treaty_pairs, r.v[3].treaty_pairs,
                    r.v[4].treaty_pairs, r.v[5].treaty_pairs);
        std::printf("         battles C %4lld: %4lld %4lld %4lld %4lld %4lld %4lld | treasury C %lld: %lld %lld %lld %lld %lld %lld\n",
                    static_cast<long long>(r.c_battles), static_cast<long long>(r.v[0].battles),
                    static_cast<long long>(r.v[1].battles), static_cast<long long>(r.v[2].battles),
                    static_cast<long long>(r.v[3].battles), static_cast<long long>(r.v[4].battles),
                    static_cast<long long>(r.v[5].battles),
                    static_cast<long long>(r.c_treasury / 1000), static_cast<long long>(r.v[0].treasury / 1000),
                    static_cast<long long>(r.v[1].treasury / 1000), static_cast<long long>(r.v[2].treasury / 1000),
                    static_cast<long long>(r.v[3].treasury / 1000), static_cast<long long>(r.v[4].treasury / 1000),
                    static_cast<long long>(r.v[5].treasury / 1000));
        std::printf("         Fuel Doctrine coke/charcoal/neither: C %d/%d/%d | V4 %d/%d/%d | V5 %d/%d/%d\n",
                    r.c_doctrine[0], r.c_doctrine[1], r.c_doctrine[2],
                    r.v[4].doctrine[0], r.v[4].doctrine[1], r.v[4].doctrine[2],
                    r.v[5].doctrine[0], r.v[5].doctrine[1], r.v[5].doctrine[2]);
        std::printf("         by source: market stamp %d/%d | 1660 record %d/%d | dead-filter %d/%d | seed %d/%d | "
                    "survey %d/%d | residual %d/%d\n",
                    r.step[1].owner_regions, r.step[1].treaty_pairs, r.step[2].owner_regions, r.step[2].treaty_pairs,
                    r.step[3].owner_regions, r.step[3].treaty_pairs, r.step[4].owner_regions, r.step[4].treaty_pairs,
                    r.step[5].owner_regions, r.step[5].treaty_pairs,
                    r.v[0].owner_regions, r.v[0].treaty_pairs);
    }
    {
        // Attribution: each source's own footprint (the variant against the
        // one before it), spread over the seeds.
        const char* names[seed_fidelity::k_variants] = {
            "residual: V0 against C", "the close's market stamp: V1 against V0",
            "the 1660 corridor record: V2 against V1", "the dead-filter: V3 against V2",
            "the seed: V4 against V3", "the span-open survey: V5 against V4" };
        std::printf("  regions owned differently at 1960, per source:\n");
        for (int k = 0; k < seed_fidelity::k_variants; ++k)
        {
            std::vector<double> inc;
            for (const seed_fidelity& r : rows)
                if (r.ran)
                    inc.push_back(static_cast<double>(k == 0 ? r.v[0].owner_regions : r.step[k].owner_regions));
            print_spread(names[k], inc);
        }
        std::printf("    (a footprint is measured against the variant before it; sources interact, so footprints\n"
                    "     do not sum to V5's distance from C)\n");
    }

    int g3 = 0;
    std::printf("\n=== GATE 3 (BL-1040 R5): the shipped Digitisation span IS the real resume (V5), field for field ===\n");
    std::printf("  (generation's own 1960 close against V5 folded by make_digitisation_output; the span's params\n"
                "   checked against its derivation; the shipped close's own validator re-run against the 1660 value;\n"
                "   BL-1051: the table the span opened on is the handoff plus exactly the survey, every region seen)\n");
    for (const seed_fidelity& r : rows)
    {
        if (!r.ran) continue;
        if (r.gate3) ++g3;
        std::printf("  %5u  %s  rounds %lld | regions: %s | polities: %s", r.seed, r.gate3 ? "PASS" : "FAIL",
                    static_cast<long long>(r.span_rounds), census_text(r.span_regions).c_str(),
                    census_text(r.span_polities).c_str());
        for (const std::string& t : r.span_params_issues) std::printf(" | params: %s", t.c_str());
        for (const std::string& t : r.span_tables) std::printf(" | %s", t.c_str());
        std::printf(" | negative controls run: %d of 3%s\n", 3 - r.controls_untried,
                    r.controls_untried > 0 ? " (the rest had no candidate on this seed)" : "");
    }

    double total = 0.0, worst = 0.0;
    for (const seed_fidelity& r : rows) { total += r.seconds; worst = std::max(worst, r.seconds); }
    std::printf("\nSUMMARY  gate 1 (opening == struct) %d of %d seeds | gate 2 (one neutralised round == continued) "
                "%d of %d seeds | gate 3 (shipped span == the resume) %d of %d seeds | %.1f s total, worst seed "
                "%.1f s\n", g1, ran, g2, ran, g3, ran, total, worst);
    const bool pass = ran > 0 && g1 == ran && g2 == ran && g3 == ran;
    std::printf("%s\n", pass ? "FIDELITY PASS" : "FIDELITY FAIL");
    return pass ? 0 : 1;
}

// ===========================================================================
// BL-1037 -- A RESUMED SPAN REOPENS A BOUGHT RUNG AT ITS TIER (`--resume-tier`)
// ===========================================================================
//
// Per seed, at BOTH span boundaries -- Exploration's own open at 1200 (from
// the Empires record) and a Digitisation resume at 1660 (from the handoff) --
// with `history_sim_params::resume_seeds_corridor_tier` OFF (today) and ON:
//
//   changed      record rows whose resumed rung the switch changes, read off
//                the record: the walks UNDER-read a bought rung ("bought,
//                demoted") or OVER-read a refused walk ("refused, promoted").
//   below/above  rows whose rung at the captured OPENING sits below / above
//                the record's `tier`.
//   re-crossed   a road_promoted event during the span to a rung at or below
//                the one the record says the corridor already stood on -- a
//                rung bought, or walked, twice. "post" counts post roads.
//
// The 1660 ON boundary resumes from a handoff folded off an ON Exploration
// span -- the world the re-bless would ship; OFF resumes from the shipped
// handoff. Both 1660 resumes run to 1960 on Exploration's seed.
//
// GATES the switch-on invariants (exit 1 on any seed): nothing below, nothing
// above, nothing re-crossed, at either boundary. The OFF columns are the
// defect's size today, reported.

struct boundary_tiers
{
    int     rows = 0;             ///< record rows a resume seeds
    int     bought_demoted   = 0; ///< walks under-read the rung (walked tier < tier)
    int     refused_promoted = 0; ///< walks over-read the rung (walked tier > tier)
    int     below = 0, above = 0; ///< opening live rung against the record's
    int     recrossed = 0, recrossed_post = 0;
    int64_t post_roads_built = 0; ///< the span's own post-road purchases
};

int tier_for_uses(const history_sim_params& p, int uses)
{
    if (uses >= p.road_tier3_uses) return 3;
    if (uses >= p.road_tier2_uses) return 2;
    if (uses >= p.road_tier1_uses) return 1;
    return 0;
}

boundary_tiers measure_boundary(const history_sim_params& p, const std::vector<history_corridor>& record,
                                const history_sim_state& span)
{
    boundary_tiers b;
    b.post_roads_built = span.post_roads_built;
    std::map<std::pair<int, int>, int> open_live;
    for (const history_corridor& e : span.capture.live_roads) open_live[{e.a, e.b}] = e.tier;
    std::map<std::pair<int, int>, int> record_tier;
    for (const history_corridor& c : record)
    {
        if (c.a == c.b || c.uses <= 0) continue; // exactly the rows a resume seeds
        ++b.rows;
        const int walked = tier_for_uses(p, c.uses);
        if (walked < c.tier) ++b.bought_demoted;
        if (walked > c.tier) ++b.refused_promoted;
        const auto it = open_live.find({c.a, c.b});
        const int live = it == open_live.end() ? 0 : it->second;
        if (live < c.tier) ++b.below;
        if (live > c.tier) ++b.above;
        record_tier[{c.a, c.b}] = c.tier;
    }
    for (const lapse_event& e : span.events)
    {
        if (e.kind != static_cast<uint8_t>(lapse_event_kind::road_promoted)) continue;
        const int lo = std::min<int>(e.region, e.other), hi = std::max<int>(e.region, e.other);
        const auto it = record_tier.find({lo, hi});
        if (it == record_tier.end()) continue;
        if (static_cast<int>(e.polity) <= it->second)
        {
            ++b.recrossed;
            if (e.polity == 3) ++b.recrossed_post;
        }
    }
    return b;
}

int run_resume_tier(const std::vector<uint32_t>& seeds, const world_gen_config& cfg_in, works_registry& works)
{
    world_gen_config cfg = cfg_in;
    cfg.stop_after_exploration = true;

    std::printf("=== digitisation_sim_harness --resume-tier (BL-1037) - a resumed corridor reopens at its rung ===\n");
    std::printf("switch: history_sim_params::resume_seeds_corridor_tier. 1200 = Exploration's own open (Empires\n"
                "record); 1660 = a resume from the handoff to 1960 (OFF: the shipped handoff; ON: one folded off an\n"
                "ON Exploration span). rows = record rows seeded; changed = bought,demoted / refused,promoted;\n"
                "below/above = opening rung against the record's tier; re-crossed = a rung bought or walked twice\n"
                "(post roads in brackets); post = the span's own post-road purchases.\n\n");
    std::fflush(stdout);

    int ran = 0, failed = 0;
    double total = 0.0, worst = 0.0;
    for (uint32_t seed : seeds)
    {
        const auto t0 = std::chrono::steady_clock::now();
        std::fprintf(stderr, "[resume-tier] seed %u generating\n", seed);
        world_params wp{};
        wp.seed = seed;
        generation_report     rep;
        era_minus_one_fixture fx;
        (void)make_hard_coded_world(wp, &rep, cfg, /*progress=*/nullptr, &works, &fx);
        if (!fx.exploration_ran || fx.exploration_handoff.stop_year != 1660)
        {
            std::printf("  %5u  SKIPPED: no shipped 1660 handoff\n", seed);
            continue;
        }
        ++ran;
        const history_sim_params&  ep = fx.exploration_params;
        const exploration_output&  H  = fx.exploration_handoff;
        const int64_t open_1200 = ep.start_year;
        const int64_t first     = open_1200 + ep.tick_bands[0].step_years;

        // 1200, OFF: the opening from a one-round re-run; the span from
        // generation's own (shipped) run.
        std::fprintf(stderr, "[resume-tier] seed %u 1200 boundary\n", seed);
        history_sim_state off_1200 = continued(fx, first, open_1200, false).hs;
        off_1200.events           = fx.exploration_state.events;
        off_1200.post_roads_built = fx.exploration_state.post_roads_built;
        const boundary_tiers b1200_off = measure_boundary(ep, fx.pre_exploration_corridors, off_1200);

        // 1200, ON: the whole Exploration span with the switch, folded into
        // the handoff an ON world would carry to 1660.
        run_out on_expl = continued(fx, H.stop_year, open_1200, true);
        const boundary_tiers b1200_on = measure_boundary(ep, fx.pre_exploration_corridors, on_expl.hs);
        const exploration_output H_on = make_exploration_output(on_expl.ss, on_expl.hs, &fx.pre_exploration_creeds);

        // 1660: a resume from each handoff to 1960.
        std::fprintf(stderr, "[resume-tier] seed %u 1660 boundary\n", seed);
        resume_spec off_spec;
        off_spec.regions = &H.regions;
        off_spec.record  = &H.surviving_corridors;
        off_spec.seed    = fx.exploration_seed;
        const boundary_tiers b1660_off =
            measure_boundary(ep, H.surviving_corridors, resume(fx, H, off_spec, 1960, H.stop_year, ep).hs);
        resume_spec on_spec;
        on_spec.regions   = &H_on.regions;
        on_spec.record    = &H_on.surviving_corridors;
        on_spec.seed      = fx.exploration_seed;
        on_spec.tier_seed = true;
        const boundary_tiers b1660_on =
            measure_boundary(ep, H_on.surviving_corridors, resume(fx, H_on, on_spec, 1960, H_on.stop_year, ep).hs);

        const auto ok = [](const boundary_tiers& b) { return b.below == 0 && b.above == 0 && b.recrossed == 0; };
        const bool seed_ok = ok(b1200_on) && ok(b1660_on);
        if (!seed_ok) ++failed;

        const auto line = [](const char* span, const boundary_tiers& off, const boundary_tiers& on) {
            std::printf("         %s  rows %5d/%-5d changed %3d,%-3d/%3d,%-3d | OFF below %3d above %3d re-crossed "
                        "%3d (%d) post %3lld | ON below %d above %d re-crossed %d (%d) post %3lld\n",
                        span, off.rows, on.rows, off.bought_demoted, off.refused_promoted, on.bought_demoted,
                        on.refused_promoted, off.below, off.above, off.recrossed, off.recrossed_post,
                        static_cast<long long>(off.post_roads_built), on.below, on.above, on.recrossed,
                        on.recrossed_post, static_cast<long long>(on.post_roads_built));
        };
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        total += secs;
        worst = std::max(worst, secs);
        std::printf("  %5u  %s  (%.1f s)   [rows / changed: OFF record / ON record]\n", seed,
                    seed_ok ? "PASS" : "FAIL", secs);
        line("1200", b1200_off, b1200_on);
        line("1660", b1660_off, b1660_on);
        std::fflush(stdout);
    }
    std::printf("\nSUMMARY  switch-on invariants (nothing below, above or re-crossed at 1200 and 1660) hold on %d of %d "
                "seeds | %.1f s total, worst seed %.1f s\n", ran - failed, ran, total, worst);
    const bool pass = ran > 0 && failed == 0;
    std::printf("%s\n", pass ? "RESUME-TIER PASS" : "RESUME-TIER FAIL");
    return pass ? 0 : 1;
}

} // namespace fidelity

} // namespace

int main(int argc, char** argv)
{
    // --- Arguments ---------------------------------------------------------
    std::vector<uint32_t> seeds;
    bool seeds_from_args = false;
    int  limit = -1;
    int64_t through_year = 1660; // BL-1029
    std::string out_path;        // BL-1029
    bool fidelity_mode = false;  // BL-1036
    bool resume_tier_mode = false; // BL-1037
    bool continued_mode   = false; // BL-1040: --continued, the 1200-network run
    int  integral_stride  = 1;     // BL-1041: --integral-stride, rounds per head-years sample (0 = off)
    for (int a = 1; a < argc; ++a)
    {
        if (std::strcmp(argv[a], "--limit") == 0 && a + 1 < argc)
        {
            limit = std::atoi(argv[++a]);
        }
        else if (std::strcmp(argv[a], "--integral-stride") == 0 && a + 1 < argc)
        {
            integral_stride = std::atoi(argv[++a]);
            if (integral_stride < 0 || integral_stride > 75)
            {
                std::printf("--integral-stride must be 0 (off) to 75 rounds\n");
                return 2;
            }
        }
        else if (std::strcmp(argv[a], "--continued") == 0)
        {
            continued_mode = true;
        }
        else if (std::strcmp(argv[a], "--fidelity") == 0)
        {
            fidelity_mode = true;
        }
        else if (std::strcmp(argv[a], "--resume-tier") == 0)
        {
            resume_tier_mode = true;
        }
        else if (std::strcmp(argv[a], "--through") == 0 && a + 1 < argc)
        {
            through_year = std::atoll(argv[++a]);
            if (through_year < 1660)
            {
                std::printf("--through must be 1660 or later (the control is the 1660 handoff)\n");
                return 2;
            }
        }
        else if (std::strcmp(argv[a], "--out") == 0 && a + 1 < argc)
        {
            out_path = argv[++a];
        }
        else if (std::strcmp(argv[a], "--seeds") == 0 && a + 1 < argc)
        {
            seeds_from_args = true;
            std::stringstream list(argv[++a]);
            std::string item;
            while (std::getline(list, item, ','))
                if (!item.empty()) seeds.push_back(static_cast<uint32_t>(std::strtoul(item.c_str(), nullptr, 10)));
        }
        else
        {
            std::printf("unknown argument '%s'\nusage: digitisation_sim_harness [--limit N] [--seeds a,b,c] "
                        "[--through Y [--continued] [--integral-stride N]] [--out path] [--fidelity] [--resume-tier]\n",
                        argv[a]);
            return 2;
        }
    }
    if (continued_mode && through_year == 1660)
    {
        std::printf("--continued needs --through past 1660 (it continues Exploration's call to that year)\n");
        return 2;
    }
    if (!seeds_from_args)
    {
        seeds = library_seeds("docs/generation/seed_library.json");
        if (seeds.empty())
        {
            std::printf("FATAL  docs/generation/seed_library.json not found or carries no seeds "
                        "(run from the repo root)\n");
            return 1;
        }
    }
    if (limit > 0 && static_cast<std::size_t>(limit) < seeds.size())
        seeds.resize(static_cast<std::size_t>(limit));

    const std::map<uint32_t, library_fingerprint> fingerprints =
        library_fingerprints("docs/generation/seed_library.json");

    // BL-1040: which close this run reads. SPAN: the Digitisation span, its
    // own call from the 1660 handoff (the phase's reading). CONTINUED: BL-1029's
    // run, Exploration's call stretched -- a 1200-network comparison. Neither
    // past 1660: no span, close == control.
    const bool past_1660 = through_year > 1660;
    const bool span_mode = past_1660 && !continued_mode;

    // The readings header belongs to the readings mode; --fidelity and
    // --resume-tier print their own below.
    if (!fidelity_mode && !resume_tier_mode)
    {
        std::printf("=== digitisation_sim_harness (BL-982) - the thirteen Digitisation readings ===\n");
        if (span_mode)
            std::printf("close: %lld CE - the shipped world (epoch_year 0) with the DIGITISATION SPAN on: its own call,\n"
                        "resumed from the 1660 exploration_output, 1660 -> %lld (BL-1040);\n",
                        static_cast<long long>(through_year), static_cast<long long>(through_year));
        else if (continued_mode)
            std::printf("close: %lld CE - A 1200-NETWORK RUN: the shipped world (epoch_year 0) with Exploration's own\n"
                        "call continued to %lld, pricing on the network it inherited at 1200 (BL-1029's run, not the span);\n",
                        static_cast<long long>(through_year), static_cast<long long>(through_year));
        else
            std::printf("close: 1660 CE - the shipped world (epoch_year 0), no span: close and control are one world;\n");
        std::printf("control: the 1660 handoff, re-run from the fixture and held to the seed library fingerprint\n");
    }
    std::printf("seeds (%s, %zu):", seeds_from_args ? "--seeds" : "docs/generation/seed_library.json",
                seeds.size());
    for (uint32_t s : seeds) std::printf(" %u", s);
    std::printf("\n");

    // --- The shipped data layer, in app order ------------------------------
    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
    recipe_registry  reg;
    lua.load("scripts/world_gen.lua");
    cfg.load_from_lua(lua);
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    reg.load_from_lua(lua);
    if (reg.recipe_count() == 0)
    {
        std::printf("FATAL  scripts/recipes.lua loaded no recipes (run from the repo root)\n");
        return 1;
    }
    std::printf("generation inputs: scripts/world_gen.lua + scripts/works.lua (%zu works rows) + "
                "scripts/recipes.lua/economy.lua (%zu recipes) - the shipped configuration (BL-1007)\n",
                works.size(), reg.recipe_count());
    if (cfg.corporation_count != 8)
        std::printf("PARITY WARNING  world_gen.lua sets corporation_count=%d but apply_shipped_landscape "
                    "searches from 8; the landscape measured is not the app's\n", cfg.corporation_count);

    // BL-1036: the resume-fidelity check is its own mode -- it reads the 1660
    // handoff and nothing past it, so it neither builds a campaign world nor
    // takes a reading. --through does not apply to it (it runs its own 1960).
    if (fidelity_mode)
        return fidelity::run(seeds, cfg, works);
    if (resume_tier_mode) // BL-1037, same footing: the handoff and the two boundaries only.
        return fidelity::run_resume_tier(seeds, cfg, works);

    const world_params shipped_descriptor{};
    const era_band band = era_band_for_epoch(shipped_descriptor.epoch_year);
    int band_tier3_recipes = 0;
    {
        reg.set_era(band);
        for (std::size_t id = 0; id < reg.recipe_count(); ++id)
        {
            const recipe* rc = reg.get_recipe(static_cast<uint16_t>(id));
            if (rc != nullptr && recipe_makes_tier3(*rc) && era_permits(band, rc->era))
                ++band_tier3_recipes;
        }
    }
    std::printf("epoch_year %lld (world_params default, the shipped descriptor) -> era band %s\n\n",
                static_cast<long long>(shipped_descriptor.epoch_year),
                band == era_band::ancient ? "ancient" : (band == era_band::industrial ? "industrial" : "any"));
    std::fflush(stdout);

    std::vector<seed_row> rows;
    rows.reserve(seeds.size());
    std::size_t setup_checked = 0, setup_empty = 0; // BL-1053: the setup-diff line's tally

    for (uint32_t seed : seeds)
    {
        world_params wp = shipped_descriptor;
        wp.seed = seed;
        if (span_mode)
        {
            // BL-1040: the span runs from Exploration's shipped 1660 close.
            wp.digitisation_span_enabled = true;
            wp.digitisation_stop_year    = through_year;
        }
        else if (continued_mode)
        {
            wp.exploration_stop_year = through_year; // BL-1029: never epoch_year.
        }

        std::fprintf(stderr, "[digitisation] seed %u generating\n", seed);
        generation_report     rep;
        era_minus_one_fixture fx;
        world w = make_hard_coded_world(wp, &rep, cfg, /*progress=*/nullptr, &works, &fx);

        reg.set_era(era_band_for_epoch(wp.epoch_year)); // app::load_economy
        const shipped_landscape land = apply_shipped_landscape(w, reg, wp.seed);
        std::printf("seed %u ", seed);
        print_shipped_landscape(land);

        // BL-1053 -- THE SETUP-DIFF LINE. World setup's four reads of the
        // history (grudges, stamped roads, junction corridors, treasuries)
        // against the close this world is built on: the span's 1960 fold when
        // it ran, else Exploration's own. Empty is the pass; any table named
        // is setup reading a close the world does not stand on.
        if (fx.exploration_ran)
        {
            const bool on_span = span_mode && fx.digitisation_ran;
            const exploration_output& close = on_span ? fx.digitisation_handoff : fx.exploration_handoff;
            const std::vector<std::string> d = fidelity::setup_diff(fx, close);
            int64_t chest_close = 0, chest_1660 = 0;
            for (const int64_t t : fx.setup_polity_treasuries) chest_close += t;
            for (const region& rg : fx.exploration_handoff.regions) if (rg.nation >= 0 && rg.treasury > 0) chest_1660 += rg.treasury;
            std::printf("seed %u SETUP-DIFF vs the %lld close (%s): %s | read: %zu grudges, %zu corridors, %zu junction "
                        "corridors, %zu polity chests summing %lld (the 1660 close's: %zu grudges, %zu corridors, "
                        "chests %lld)\n",
                        seed, static_cast<long long>(close.stop_year),
                        on_span ? "the Digitisation span's" : span_mode ? "Exploration's -- THE SPAN DID NOT RUN"
                                                                        : "Exploration's",
                        d.empty() ? "EMPTY" : "DIFFERS", fx.setup_grudges.size(), fx.setup_corridors.size(),
                        fx.setup_junction_corridors.size(), fx.setup_polity_treasuries.size(),
                        static_cast<long long>(chest_close), fx.exploration_handoff.grudges.size(),
                        fx.exploration_handoff.surviving_corridors.size(), static_cast<long long>(chest_1660));
            for (const std::string& t : d) std::printf("    setup-diff | %s\n", t.c_str());
            ++setup_checked;
            if (d.empty() && (on_span || !span_mode)) ++setup_empty;
        }
        std::fflush(stdout);

        seed_row row;
        row.seed     = seed;
        row.era_ran  = fx.ran;
        row.expl_ran = fx.exploration_ran;
        row.close_ran = span_mode ? fx.digitisation_ran : fx.exploration_ran;
        if (fx.digitisation_ran)
        {
            row.span_ran       = true;
            row.span_start     = fx.digitisation_params.start_year;
            row.span_stop      = fx.digitisation_params.stop_year;
            row.span_rounds    = fx.digitisation_rounds;
            row.span_battles   = fx.digitisation_state.battles;
            row.span_conquests = fx.digitisation_state.conquests;
            row.span_foundings = fx.digitisation_state.foundings;
            row.span_ms        = fx.ms_digitisation;

            // ---- BL-1051: the span-open survey, and who took which fuel ----
            const std::vector<region>& open_t = fx.digitisation_open_regions;
            row.survey_ran     = !open_t.empty();
            row.survey_regions = static_cast<int>(open_t.size());
            std::vector<int> forest_v, fuel_v;
            for (const region& rg : open_t)
            {
                if (rg.survey_forest_q < 0 || rg.survey_fuel_q < 0) { ++row.survey_unseen; continue; }
                forest_v.push_back(rg.survey_forest_q);
                fuel_v.push_back(rg.survey_fuel_q);
                if (rg.survey_forest_q > 0)   ++row.survey_forested;
                if (rg.survey_forest_q >= 500) ++row.survey_wooded;
                if (rg.survey_fuel_q >= industry_fuel_seam_bar_q) ++row.survey_fuel_seam;
                if (rg.energy_q      >= industry_fuel_seam_bar_q) ++row.survey_seam_energy;
            }
            std::sort(forest_v.begin(), forest_v.end());
            std::sort(fuel_v.begin(), fuel_v.end());
            const auto rank = [](const std::vector<int>& v, int pct) {
                if (v.empty()) return 0;
                return v[static_cast<std::size_t>((pct * (static_cast<int>(v.size()) - 1) + 50) / 100)];
            };
            const int pcts[5] = { 0, 25, 50, 75, 100 };
            for (int k = 0; k < 5; ++k) row.survey_forest[k] = rank(forest_v, pcts[k]);
            row.survey_fuel_med = rank(fuel_v, 50);

            const std::vector<region>& close_t = fx.digitisation_handoff.regions;
            row.close_regions = static_cast<int>(close_t.size());
            for (const region& rg : close_t) if (rg.survey_forest_q < 0) ++row.close_unsurveyed;

            // Held sets at the close, in ascending region order, by `nation`.
            std::map<int, std::vector<int>> held_by;
            for (std::size_t i = 0; i < close_t.size(); ++i)
                if (close_t[i].nation >= 0) held_by[close_t[i].nation].push_back(static_cast<int>(i));
            double forest_sum[3] = {};
            for (const polity& q : fx.digitisation_handoff.polities)
            {
                if (!q.alive) continue;
                const int side = fidelity::fuel_doctrine_side(q);
                ++row.doctrine[side];
                const auto it = held_by.find(q.id);
                const std::vector<int> none;
                const std::vector<int>& held = it != held_by.end() ? it->second : none;
                forest_sum[side] += static_cast<double>(industry_ground_forest_q(close_t, held));
                if (side == 1)
                {
                    if (q.industry_fuel_seen) ++row.charcoal_seam_seen;
                    // BL-1041: the seam as the span's gate read it -- the fuel
                    // reading under DEFAULT B, energy_q with it off.
                    const bool b_on = fx.digitisation_params.industry_fuel_gate_reads_survey;
                    int seam = 0;
                    for (int hi : held)
                    {
                        const region& hr = close_t[static_cast<std::size_t>(hi)];
                        seam = std::max(seam, b_on ? industry_fuel_reading_q(hr) : hr.energy_q);
                    }
                    if (seam >= industry_fuel_seam_bar_q) ++row.charcoal_seam_close;
                }
            }
            for (int s = 0; s < 3; ++s)
                if (row.doctrine[s] > 0) row.doctrine_forest[s] = forest_sum[s] / row.doctrine[s];

            // ---- BL-1041: reading 8, industry points at the close ----------
            const history_sim_params& dpp = fx.digitisation_params;
            const history_sim_state&  dst = fx.digitisation_state;
            row.points_on       = dpp.industry_points_enabled;
            row.points_rejected = dst.industry_points_params_rejected;
            row.points_scale    = dst.industry_points_from_scale;
            row.points_treasury = dst.industry_points_from_treasury;
            row.treasury_debited = dst.treasury_spent_on_industry;
            row.points_refused  = dst.industry_points_refused;
            row.ns_points       = fx.ns_digitisation_industry_points;
            const std::size_t open_n = open_t.size();
            int64_t inherited_pts = 0, unheld_pts = 0;
            std::vector<double> reg_pts, rho_p, rho_u, int_p, int_f;
            std::vector<int> factors;
            std::map<int, int64_t> by_polity;
            for (std::size_t i = 0; i < close_t.size(); ++i)
            {
                const region& rg = close_t[i];
                row.points_total += rg.industry_points;
                if (rg.centres > 0)
                {
                    ++row.centre_regions;
                    const int fr = industry_fuel_reading_q(rg);
                    factors.push_back(industry_points_fuel_factor_q(fr, dpp));
                    if (fr == 0) ++row.fuel_at_floor;
                    if (rg.industry_points > 0 && rg.urban_population > 0)
                    {
                        int_p.push_back(static_cast<double>(rg.industry_points)
                                        / static_cast<double>(rg.urban_population));
                        int_f.push_back(static_cast<double>(factors.back()));
                    }
                }
                if (rg.industry_points > 0 || rg.centres > 0)
                {
                    rho_p.push_back(static_cast<double>(rg.industry_points));
                    rho_u.push_back(static_cast<double>(rg.urban_population));
                }
                if (rg.industry_points <= 0) continue;
                ++row.points_regions;
                reg_pts.push_back(static_cast<double>(rg.industry_points));
                if (i >= open_n) { inherited_pts += rg.industry_points; ++row.inherited_regions; }
                if (rg.nation >= 0) by_polity[rg.nation] += rg.industry_points;
                else unheld_pts += rg.industry_points;
            }
            row.points_ledger_ok = row.points_total == row.points_scale + row.points_treasury;
            if (row.points_total > 0)
            {
                const double tot = static_cast<double>(row.points_total);
                row.points_region_gini = gini(reg_pts);
                row.points_top_region  = *std::max_element(reg_pts.begin(), reg_pts.end()) / tot;
                row.inherited_share    = static_cast<double>(inherited_pts) / tot;
                row.points_unheld      = static_cast<double>(unheld_pts) / tot;
                row.treasury_share     = static_cast<double>(row.points_treasury)
                                       / static_cast<double>(row.points_scale + row.points_treasury);
            }
            // Per LIVING polity, zeros included: a realm with no points is part
            // of the spread, not missing from it.
            std::vector<double> pol_pts;
            int64_t held_total = 0;
            for (const polity& q : fx.digitisation_handoff.polities)
            {
                if (!q.alive) continue;
                ++row.living_close;
                const auto it = by_polity.find(q.id);
                const int64_t v = it != by_polity.end() ? it->second : 0;
                if (v > 0) ++row.points_polities;
                pol_pts.push_back(static_cast<double>(v));
                held_total += v;
            }
            row.points_polity_gini = gini(pol_pts);
            if (held_total > 0)
                row.points_top_polity = *std::max_element(pol_pts.begin(), pol_pts.end())
                                      / static_cast<double>(held_total);
            std::sort(factors.begin(), factors.end());
            for (int k = 0; k < 5; ++k) row.fuel_factor[k] = rank(factors, pcts[k]);
            row.rho_points_urban   = spearman(rho_p, rho_u);
            row.rho_intensity_fuel = spearman(int_p, int_f);

            // ---- BL-1041 fix round: WHERE the points stand, by survey ------
            // ORIGINAL ground is the pre-sim settlement pass's: the regions the
            // Empires sim received (the table's own prefix -- regions are only
            // ever appended) and its scheduled foundings, which the sim moves
            // in whole, survey and all, matched on (col, row, founded_year).
            // Everything else in the table the span opened on is ground the
            // sim founded before 1660 (energy_q inherited, re-surveyed at the
            // open); beyond it is ground the span itself founded.
            {
                const std::vector<region>& pre = fx.settlement.regions;
                std::multiset<std::tuple<int, int, int64_t>> scheduled;
                for (const region& p : fx.settlement.pending_foundings)
                    scheduled.insert(std::make_tuple(p.col, p.row, p.founded_year));
                int64_t orig_pts = 0, resurv_pts = 0;
                for (std::size_t i = 0; i < close_t.size(); ++i)
                {
                    const region& rg = close_t[i];
                    bool original = false;
                    if (i < pre.size())
                        original = true; // the prefix (validated col/row-stable across every span)
                    else if (i < open_n)
                    {
                        const auto it = scheduled.find(std::make_tuple(rg.col, rg.row, rg.founded_year));
                        if (it != scheduled.end()) { original = true; scheduled.erase(it); }
                    }
                    if (i >= open_n)  ++row.regions_span_founded;
                    else if (original) { ++row.regions_original;   orig_pts   += rg.industry_points; }
                    else               { ++row.regions_resurveyed; resurv_pts += rg.industry_points; }
                }
                if (row.points_total > 0)
                {
                    row.share_original   = static_cast<double>(orig_pts)   / static_cast<double>(row.points_total);
                    row.share_resurveyed = static_cast<double>(resurv_pts) / static_cast<double>(row.points_total);
                }
            }

            // ---- BL-1041 fix round: the honest headcount test --------------
            // Two harness-side re-runs of the span on the fixture, each the
            // shipped span by the same construction `--fidelity`'s gate 3 proves
            // (V5: the opened table, the span's own seed and params). The first
            // checks that claim on this seed before anything is read off it.
            const int64_t E = dpp.industry_points_per_treasury_unit;
            if (integral_stride > 0 && row.points_on && !row.points_rejected && !open_t.empty() && E > 1)
            {
                const auto t_int = std::chrono::steady_clock::now();
                const exploration_output& H = fx.exploration_handoff;
                fidelity::resume_spec spec;
                spec.regions = &open_t;
                spec.record  = &H.surviving_corridors;
                spec.live    = nullptr;
                spec.seed    = fx.digitisation_seed;

                // (1) SCALE vs TREASURY: the same span at one point per
                // treasury unit. Nothing but the points may differ.
                history_sim_params d1 = dpp;
                d1.industry_points_per_treasury_unit = 1;
                const fidelity::run_out x = fidelity::resume(fx, H, spec, dpp.stop_year, INT64_MIN, d1);
                const std::vector<region>& R2 = x.ss.regions;
                if (R2.size() == close_t.size())
                    for (std::size_t i = 0; i < R2.size(); ++i)
                    {
                        const region& a = close_t[i];
                        const region& b = R2[i];
                        if (a.nation != b.nation || a.population != b.population
                         || a.urban_population != b.urban_population || a.centres != b.centres
                         || a.treasury != b.treasury || a.army_stock != b.army_stock)
                            ++row.rerun_mismatch;
                    }
                else
                    row.rerun_mismatch = -1;
                row.rerun_ok = row.rerun_mismatch == 0 && x.hs.industry_points_refused == 0
                            && dst.industry_points_refused == 0;

                std::vector<int64_t> S(close_t.size(), 0);
                if (row.rerun_ok)
                {
                    bool ok = true;
                    int64_t sum_s = 0, sum_t = 0;
                    for (std::size_t i = 0; i < close_t.size(); ++i)
                    {
                        const int64_t d = close_t[i].industry_points - R2[i].industry_points; // (E - 1) T
                        if (d < 0 || d % (E - 1) != 0) { ok = false; break; }
                        const int64_t T = d / (E - 1);
                        S[i] = R2[i].industry_points - T;
                        if (S[i] < 0) { ok = false; break; }
                        sum_s += S[i];
                        sum_t += T;
                    }
                    // The split must reproduce the span's own ledger exactly.
                    row.split_ok = ok && sum_s == dst.industry_points_from_scale
                                      && sum_t * E == dst.industry_points_from_treasury;
                }

                // (2) HEAD-YEARS: the span re-run to the top of each sampled
                // decision year. The open is the table the span opened on.
                std::vector<int64_t> hy(close_t.size(), 0);
                const int64_t step    = std::max<int64_t>(dpp.tick_bands[0].step_years, 1);
                const int64_t advance = step * integral_stride;
                bool captured_all = true;
                for (int64_t Y = dpp.start_year; Y < dpp.stop_year && row.split_ok; Y += advance)
                {
                    fidelity::run_out c;
                    const std::vector<region>* R = &open_t;
                    if (Y != dpp.start_year)
                    {
                        c = fidelity::resume(fx, H, spec, Y + 1, Y, dpp);
                        if (!c.hs.capture.captured || c.hs.capture.year != Y) { captured_all = false; break; }
                        R = &c.hs.capture.regions;
                    }
                    const int64_t years = std::min<int64_t>(advance, dpp.stop_year - Y);
                    for (std::size_t i = 0; i < R->size() && i < hy.size(); ++i)
                    {
                        const region& rg = (*R)[i];
                        if (rg.centres > 0 && rg.urban_population > 0) hy[i] += rg.urban_population * years;
                    }
                    ++row.integral_samples;
                }
                if (!captured_all) row.integral_samples = 0;

                if (row.split_ok && row.integral_samples > 0)
                {
                    std::vector<double> s_v, hy_v, int_v, fuel_v;
                    for (std::size_t i = 0; i < close_t.size(); ++i)
                    {
                        if (S[i] <= 0 && hy[i] <= 0) continue;
                        ++row.scale_regions;
                        s_v.push_back(static_cast<double>(S[i]));
                        hy_v.push_back(static_cast<double>(hy[i]));
                        if (S[i] > 0 && hy[i] > 0)
                        {
                            int_v.push_back(static_cast<double>(S[i]) / static_cast<double>(hy[i]));
                            fuel_v.push_back(static_cast<double>(
                                industry_points_fuel_factor_q(industry_fuel_reading_q(close_t[i]), dpp)));
                        }
                    }
                    row.rho_scale_hy       = spearman(s_v, hy_v);
                    row.rho_scale_int_fuel = spearman(int_v, fuel_v);
                }
                row.integral_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::steady_clock::now() - t_int).count();
            }
        }

        const entity_id body = fx.ran ? fx.body : w.home_body;

        // ================= Reading 1: density follows cities ================
        {
            std::vector<entity_id> mids;
            for (const auto& [mid, mc] : w.markets)
                if (mc.body == body) mids.push_back(mid);
            std::sort(mids.begin(), mids.end());
            std::map<entity_id, std::size_t> mindex;
            for (std::size_t i = 0; i < mids.size(); ++i) mindex[mids[i]] = i;
            const std::size_t n = mids.size();

            std::vector<double> urban(n, 0.0);
            std::vector<std::bitset<resource_count>> goods(n);
            std::vector<std::set<entity_id>> firms(n);

            for (const auto& [cid, pcc] : w.population_centres)
            {
                if (pcc.razed) continue;
                const auto tit = w.population_centre_tile.find(cid);
                if (tit == w.population_centre_tile.end()) continue;
                const auto tile = w.tiles.find(tit->second);
                if (tile == w.tiles.end() || tile->second.body != body) continue;
                const auto mi = mindex.find(market_for_tile(w, tit->second));
                if (mi == mindex.end()) continue;
                urban[mi->second] += static_cast<double>(pcc.population);
            }
            for (const auto& [tid, tc] : w.tiles)
            {
                if (tc.body != body) continue;
                bool any = false;
                for (std::size_t r = 0; r < resource_count; ++r)
                    if (tc.resource_deposit[r] > 0.0f) { any = true; break; }
                if (!any) continue;
                const auto mi = mindex.find(market_for_tile(w, tid));
                if (mi == mindex.end()) continue;
                for (std::size_t r = 0; r < resource_count; ++r)
                    if (tc.resource_deposit[r] > 0.0f) goods[mi->second].set(r);
            }
            std::set<entity_id> corps_on_body;
            for (const auto& [corp_id, cc] : w.corporations)
            {
                for (entity_id bid : cc.assets)
                {
                    const auto bit = w.buildings.find(bid);
                    if (bit == w.buildings.end()) continue;
                    const auto tile = w.tiles.find(bit->second.tile);
                    if (tile == w.tiles.end() || tile->second.body != body) continue;
                    corps_on_body.insert(corp_id);
                    const auto mi = mindex.find(market_for_tile(w, bit->second.tile));
                    if (mi != mindex.end()) firms[mi->second].insert(corp_id);
                }
            }

            std::vector<double> firm_count(n), good_count(n);
            for (std::size_t i = 0; i < n; ++i)
            {
                firm_count[i] = static_cast<double>(firms[i].size());
                good_count[i] = static_cast<double>(goods[i].count());
                if (urban[i] > 0.0) ++row.markets_urban;
            }
            row.markets         = static_cast<int>(n);
            row.corps_on_body   = static_cast<int>(corps_on_body.size());
            row.rho_firms_urban = spearman(firm_count, urban);
            row.rho_firms_goods = spearman(firm_count, good_count);
        }

        // ============== Readings 3 and 6: per nation on the body ============
        {
            std::vector<entity_id> nids;
            for (const auto& [nid, nc] : w.nations)
            {
                if (nc.tiles.empty()) continue;
                const auto tile = w.tiles.find(nc.tiles.front());
                if (tile != w.tiles.end() && tile->second.body == body) nids.push_back(nid);
            }
            std::sort(nids.begin(), nids.end());
            std::map<entity_id, std::size_t> nindex;
            for (std::size_t i = 0; i < nids.size(); ++i) nindex[nids[i]] = i;
            const std::size_t n = nids.size();

            std::vector<double> heads(n, 0.0), adv(n, 0.0), treasury(n, 0.0), territory(n, 0.0);
            for (std::size_t i = 0; i < n; ++i)
            {
                const nation_component& nc = w.nations.at(nids[i]);
                treasury[i]  = static_cast<double>(nc.treasury);
                territory[i] = static_cast<double>(nc.tiles.size());
            }
            for (const auto& [cid, pcc] : w.population_centres)
            {
                if (pcc.razed) continue;
                const auto tit = w.population_centre_tile.find(cid);
                if (tit == w.population_centre_tile.end()) continue;
                const auto owner = w.tile_to_nation.find(tit->second);
                if (owner == w.tile_to_nation.end()) continue;
                const auto ni = nindex.find(owner->second);
                if (ni != nindex.end()) heads[ni->second] += static_cast<double>(pcc.population);
            }
            for (const auto& [bid, bc] : w.buildings)
            {
                const auto tile = w.tiles.find(bc.tile);
                if (tile == w.tiles.end() || tile->second.body != body) continue;
                ++row.installations;
                if (bc.recipe == no_recipe) continue;
                const recipe* rc = reg.get_recipe(bc.recipe);
                if (rc == nullptr || !recipe_makes_tier3(*rc)) continue;
                ++row.advanced;
                const auto owner = w.tile_to_nation.find(bc.tile);
                if (owner == w.tile_to_nation.end()) continue;
                const auto ni = nindex.find(owner->second);
                if (ni != nindex.end()) adv[ni->second] += 1.0;
            }

            row.nations = static_cast<int>(n);
            for (std::size_t i = 0; i < n; ++i)
                if (adv[i] > 0.0) ++row.nations_with_advanced;
            if (row.advanced > 0)
            {
                row.advanced_gini      = gini(adv);
                row.rho_adv_treasury   = spearman(adv, treasury);
                row.rho_adv_population = spearman(adv, heads);
                row.rho_adv_territory  = spearman(adv, territory);
            }

            // Reading 6's PROXY: seeded treasury per thousand heads.
            std::vector<double> tph;
            for (std::size_t i = 0; i < n; ++i)
                if (heads[i] > 0.0) tph.push_back(std::max(0.0, treasury[i]) / heads[i]);
            row.nations_with_heads = static_cast<int>(tph.size());
            row.treasury_per_head_gini = gini(tph);
            const double med = median_of(tph);
            if (!tph.empty() && !std::isnan(med) && med > 0.0)
                row.treasury_per_head_max_on_med = *std::max_element(tph.begin(), tph.end()) / med;
        }

        // ====== The handoff at the close, and the 1660 control (BL-1029) =====
        row.handoff_ok        = !rep.handoff_invalid;
        row.handoff_violation = rep.handoff_violation;
        if (fx.exploration_ran)
        {
            if (span_mode)
            {
                // BL-1040: the close is the span's own fold. Its counters count
                // the span alone (a resumed run starts them at zero), so the
                // cumulative figure is Exploration's plus the span's.
                if (fx.digitisation_ran)
                    row.at_close = read_handoff(fx.digitisation_handoff, fx.gw);
                row.secessions_close         = fx.exploration_state.subjections_freed
                                             + fx.digitisation_state.subjections_freed;
                row.subjections_formed_close = fx.exploration_state.subjections_formed
                                             + fx.digitisation_state.subjections_formed;
            }
            else
            {
                row.at_close = read_handoff(fx.exploration_handoff, fx.gw);
                row.secessions_close         = fx.exploration_state.subjections_freed;
                row.subjections_formed_close = fx.exploration_state.subjections_formed;
            }

            // THE 1660 CONTROL: the Exploration span re-run from the fixture
            // and stopped at 1660, built exactly as exploration_sweep's re-runs
            // are (resume pointers, fx.works, the band's step, the seed).
            history_sim_params hp = fx.exploration_params;
            hp.stop_year        = 1660;
            hp.tick_bands[0]    = {1660, fx.exploration_params.tick_bands[0].step_years};
            hp.tick_band_count  = 1;
            hp.trace_battles    = false;
            hp.resume_polities  = &fx.pre_exploration_polities;
            hp.resume_grudges   = &fx.pre_exploration_grudges;
            hp.resume_contacts  = &fx.pre_exploration_contacts;
            hp.resume_corridors = &fx.pre_exploration_corridors;
            settlement_state ss_control = fx.pre_exploration_settlement;
            creed_state      cs_control = fx.pre_exploration_creeds;
            const history_sim_state control = run_history_sim(
                ss_control, &cs_control, fx.terrain.view(), fx.gw, fx.gh, hp,
                fx.exploration_seed, /*year_progress=*/nullptr, fx.works, /*tap=*/nullptr);
            const exploration_output control_handoff = make_exploration_output(ss_control, control, &cs_control);
            row.at_1660                 = read_handoff(control_handoff, fx.gw);
            row.secessions_1660         = control.subjections_freed;
            row.subjections_formed_1660 = control.subjections_formed;
            row.control_battles         = control.battles;

            // Living polities off the sim state, as exploration_sweep counts
            // them for the fingerprint (BL-1027 matched this on all 16 seeds).
            int64_t control_alive = 0;
            for (const polity& q : control.polities) if (q.alive) ++control_alive;
            const auto fp = fingerprints.find(seed);
            row.has_fingerprint = fp != fingerprints.end();
            // Flows against the RAW sim state, as the fingerprint counts them:
            // the fold prunes flows whose clause lapsed or whose party died by
            // the close, so the folded count can sit one or two below it.
            if (row.has_fingerprint)
                row.prefix_matches = fp->second.expl_battles    == control.battles
                                  && fp->second.flows           == static_cast<int64_t>(control.trade_flows.size())
                                  && fp->second.living_polities == control_alive;
        }

        // ================ Evidence behind the n/a lines ======================
        row.battles_standing = w.battles.size();
        row.buy_orders       = w.buy_orders.size();
        for (const buy_order& bo : w.buy_orders)
            if (bo.preferred_seller != null_entity) ++row.buy_orders_with_preferred_seller;
        row.trade_routes = w.trade_routes.size();

        rows.push_back(row);
    }

    // BL-1040: a handoff reading counts a seed only when its close exists (the span
    // ran, in span mode), generation recorded no handoff violation, and the 1660
    // control matched its fingerprint.
    const auto usable = [](const seed_row& r) {
        return r.close_ran && r.handoff_ok && (!r.has_fingerprint || r.prefix_matches);
    };
    std::size_t generated = 0, handoffs = 0;
    std::size_t prefix_checked = 0, prefix_failed = 0, violations = 0;
    for (const seed_row& r : rows)
    {
        if (r.era_ran) ++generated;
        if (usable(r)) ++handoffs;
        if (r.expl_ran && r.has_fingerprint) { ++prefix_checked; if (!r.prefix_matches) ++prefix_failed; }
        if (!r.handoff_ok) ++violations;
    }
    if (rows.empty())
    {
        std::printf("FATAL  no seed generated\n");
        return 1;
    }
    const long long T = static_cast<long long>(through_year);
    const char* close_name = span_mode ? "the Digitisation span's close"
                           : continued_mode ? "a 1200-network run's close" : "no span";

    // ======================= Per-seed table =================================
    std::printf("\n=== per seed (inputs to the spread; not verdicts) - C = the %lld close (%s), K = the 1660 control ===\n",
                T, close_name);
    std::printf("  seed | mkts urb corps  rho(f,urb) rho(f,gds) | inst  adv nat nat+adv | chk K/C | "
                "flows K/C far K/C | subj K/C secess K/C | urban%% K/C | ind.polities K/C | tph gini | prefix\n");
    const auto fmt_rho = [](double x, char* buf, std::size_t len) {
        if (std::isnan(x)) std::snprintf(buf, len, "  undef");
        else std::snprintf(buf, len, "%7.3f", x);
    };
    const auto urban_pct = [](const handoff_reading& h) {
        return h.heads > 0 ? 100.0 * static_cast<double>(h.urban_heads) / static_cast<double>(h.heads) : 0.0;
    };
    for (const seed_row& r : rows)
    {
        char a[16], b[16], c[16];
        fmt_rho(r.rho_firms_urban, a, sizeof a);
        fmt_rho(r.rho_firms_goods, b, sizeof b);
        fmt_rho(r.treasury_per_head_gini, c, sizeof c);
        const char* prefix = !r.expl_ran ? "no handoff"
                           : !r.close_ran ? "no span"
                           : !r.has_fingerprint ? "no fingerprint"
                           : r.prefix_matches ? "matches" : "MISMATCH";
        std::printf("  %4u | %4d %3d %5d    %s    %s | %5lld %4lld %3d %7d | %3d/%-3d | %3d/%-3d %3d/%-3d | "
                    "%2d/%-2d %2lld/%-2lld | %4.1f/%-4.1f | %3d/%-3d | %s | %s%s%s\n",
                    r.seed, r.markets, r.markets_urban, r.corps_on_body, a, b,
                    static_cast<long long>(r.installations), static_cast<long long>(r.advanced),
                    r.nations, r.nations_with_advanced,
                    r.at_1660.checkered_regions, r.at_close.checkered_regions,
                    r.at_1660.flows, r.at_close.flows, r.at_1660.flows_far, r.at_close.flows_far,
                    r.at_1660.subjects, r.at_close.subjects,
                    static_cast<long long>(r.secessions_1660), static_cast<long long>(r.secessions_close),
                    urban_pct(r.at_1660), urban_pct(r.at_close),
                    r.at_1660.polities_industrial, r.at_close.polities_industrial,
                    c, prefix,
                    r.handoff_ok ? "" : "  HANDOFF VIOLATION: ", r.handoff_ok ? "" : r.handoff_violation.c_str());
    }
    std::printf("SETUP-DIFF (BL-1053): world setup read the close its world stands on, table for table (grudges,\n"
                "  stamped roads, junction corridors, treasuries), on %zu of %zu seeds that ran Exploration%s\n",
                setup_empty, setup_checked,
                span_mode ? " -- the close is the Digitisation span's own" : "");

    // ======================= The span itself (BL-1040) ======================
    if (span_mode)
    {
        std::printf("\n=== THE DIGITISATION SPAN, per seed - its own call from the 1660 handoff (BL-1040) ===\n");
        std::printf("  rounds = decision rounds the span ran; battles/conquests/foundings = the span's own counters;\n"
                    "  living and subjects at K (1660) -> C; industry = living polities holding an Industry node at C\n"
                    "  (nodes summed); ms = generation's own span call, wall clock, this build (reported, never asserted)\n");
        std::printf("  seed | ran | span       | rounds | battles conq found | living K->C | subj K->C | industry pol/nodes |"
                    "  span ms\n");
        std::vector<double> ms;
        std::size_t ran_n = 0, rounds_ok = 0;
        for (const seed_row& r : rows)
        {
            if (!r.span_ran)
            {
                std::printf("  %4u |  no | (the span did not run%s)\n", r.seed,
                            r.expl_ran ? " though Exploration did -- R3 FAILS" : ": Exploration did not run");
                continue;
            }
            ++ran_n;
            const int64_t step = 4; // Exploration's band (NR-888); the span's params are checked by --fidelity
            if (r.span_rounds == (r.span_stop - r.span_start + step - 1) / step) ++rounds_ok;
            ms.push_back(static_cast<double>(r.span_ms));
            std::printf("  %4u | yes | %4lld-%4lld |   %4lld | %7lld %4lld %5lld |   %3d->%-3d  |  %3d->%-3d |   %4d/%-5d        | %7lld\n",
                        r.seed, static_cast<long long>(r.span_start), static_cast<long long>(r.span_stop),
                        static_cast<long long>(r.span_rounds), static_cast<long long>(r.span_battles),
                        static_cast<long long>(r.span_conquests), static_cast<long long>(r.span_foundings),
                        r.at_1660.alive_polities, r.at_close.alive_polities, r.at_1660.subjects, r.at_close.subjects,
                        r.at_close.polities_industry_nodes, r.at_close.industry_nodes,
                        static_cast<long long>(r.span_ms));
        }
        std::printf("  the span ran on %zu of %zu seeds; ran its full round count ((stop - start) / 4) on %zu\n",
                    ran_n, rows.size(), rounds_ok);
        if (!ms.empty())
            std::printf("  SPAN COST per seed: median %.0f ms, max %.0f ms, over %zu seeds (this harness build, serial)\n",
                        median_of(ms), *std::max_element(ms.begin(), ms.end()), ms.size());
    }

    // ============ BL-1051: the span-open survey and the Fuel Doctrine ========
    if (span_mode)
    {
        std::printf("\n=== THE SPAN-OPEN SURVEY AND THE FUEL DOCTRINE, per seed (BL-1051; evidence, not a reading) ===\n");
        std::printf("  survey (at the open, every region the span opened on): forest = survey_forest_q, the window's\n"
                    "  land share under forest SCORED against the open's mean share as fuel is (500 = the world's mean\n"
                    "  region; Ben 2026-09-18) -- min/p25/med/p75/max, regions with any forest, regions at or above the\n"
                    "  mean (>= 500); fuel = survey_fuel_q (500 = the world's mean region), its median, and regions\n"
                    "  clearing the 250 seam bar on the survey vs on the inherited energy_q (the gate's reading before\n"
                    "  BL-1041's DEFAULT B; with B on, every Industry fuel read takes the survey).\n"
                    "  close (1960): regions the span founded after its open are unsurveyed (-1, out of every mean);\n"
                    "  the Fuel Doctrine side each living polity holds (IN-MT-1a coke / IN-MT-1b charcoal / neither),\n"
                    "  per side the mean over its polities of ground_forest (each polity's BEST held forest score),\n"
                    "  and Charcoal polities that ever passed the seam gate (fuel_seen) or hold a seam at the close --\n"
                    "  they had Coke open and took Charcoal anyway.\n");
        std::printf("  seed | open (unseen) | forest min/p25/med/p75/max | any / >=500 | fuel med | seams surv/energy_q |"
                    " close (unsurv) | coke/charc/none | held forest coke / charc / none | charc: seam seen / now\n");
        int p_doc[3] = {};
        double p_forest_sum[3] = {};
        int p_forested = 0, p_wooded = 0, p_open = 0, p_unseen = 0, p_close = 0, p_unsurv = 0;
        int p_seen = 0, p_now = 0, seeds_charcoal_wooder = 0, seeds_compared = 0;
        std::vector<double> med_forest;
        for (const seed_row& r : rows)
        {
            if (!r.span_ran) continue;
            const auto f = [](double x, char* buf, std::size_t n) {
                if (std::isnan(x)) std::snprintf(buf, n, "  -");
                else std::snprintf(buf, n, "%3.0f", x);
            };
            char fc[16], fch[16], fn[16];
            f(r.doctrine_forest[0], fc, sizeof fc);
            f(r.doctrine_forest[1], fch, sizeof fch);
            f(r.doctrine_forest[2], fn, sizeof fn);
            std::printf("  %4u | %5d (%3d)   | %3d/%3d/%3d/%3d/%4d       | %4d/%-4d   |   %4d   |   %4d / %-4d       |"
                        " %5d (%4d)   | %3d/%3d/%3d     |   %s / %s / %s                 |   %3d / %-3d\n",
                        r.seed, r.survey_regions, r.survey_unseen,
                        r.survey_forest[0], r.survey_forest[1], r.survey_forest[2], r.survey_forest[3],
                        r.survey_forest[4], r.survey_forested, r.survey_wooded, r.survey_fuel_med,
                        r.survey_fuel_seam, r.survey_seam_energy, r.close_regions, r.close_unsurveyed,
                        r.doctrine[0], r.doctrine[1], r.doctrine[2], fc, fch, fn,
                        r.charcoal_seam_seen, r.charcoal_seam_close);
            for (int s = 0; s < 3; ++s)
            {
                p_doc[s] += r.doctrine[s];
                if (!std::isnan(r.doctrine_forest[s])) p_forest_sum[s] += r.doctrine_forest[s] * r.doctrine[s];
            }
            p_forested += r.survey_forested; p_wooded += r.survey_wooded; p_open += r.survey_regions;
            p_unseen += r.survey_unseen; p_close += r.close_regions; p_unsurv += r.close_unsurveyed;
            p_seen += r.charcoal_seam_seen; p_now += r.charcoal_seam_close;
            med_forest.push_back(static_cast<double>(r.survey_forest[2]));
            if (!std::isnan(r.doctrine_forest[0]) && !std::isnan(r.doctrine_forest[1]))
            {
                ++seeds_compared;
                if (r.doctrine_forest[1] > r.doctrine_forest[0]) ++seeds_charcoal_wooder;
            }
        }
        const auto pooled_mean = [&](int s) { return p_doc[s] > 0 ? p_forest_sum[s] / p_doc[s] : k_undef; };
        std::printf("  POOLED survey: %d regions opened on, %d left unseen; %d with any forest (%.0f%%), %d scoring at or"
                    " above the mean (%.0f%%); at the close %d of %d regions unsurveyed (founded in the span)\n",
                    p_open, p_unseen, p_forested, p_open > 0 ? 100.0 * p_forested / p_open : 0.0, p_wooded,
                    p_open > 0 ? 100.0 * p_wooded / p_open : 0.0, p_unsurv, p_close);
        print_spread("median region forest score at the open, per world", med_forest);
        std::printf("  POOLED Fuel Doctrine at the close: coke %d / charcoal %d / neither %d (living polities).\n"
                    "         BL-1051 cites 404 / 118 / 340 for BL-1038's run, which this harness does not reproduce;\n"
                    "         the like-for-like split WITHOUT the survey is --fidelity's V4 column (V5 is this run).\n",
                    p_doc[0], p_doc[1], p_doc[2]);
        std::printf("  ARE THE CHARCOAL POLITIES THE WOODED ONES? mean ground_forest (best held forest score), pooled\n"
                    "         over polities: coke %.0f, charcoal %.0f, neither %.0f;\n"
                    "         charcoal wooder than coke on %d of %d seeds holding both sides; Charcoal polities that\n"
                    "         ever passed the seam gate %d, holding a seam at the close %d (of %d)\n",
                    pooled_mean(0), pooled_mean(1), pooled_mean(2), seeds_charcoal_wooder, seeds_compared,
                    p_seen, p_now, p_doc[1]);
    }

    // ============ BL-1041: readings 8 and 9, per seed =========================
    if (span_mode)
    {
        std::printf("\n=== READINGS 8 AND 9, per seed - industry points at the close, and the urban share (BL-1041) ===\n");
        std::printf("  points = industry points standing on regions at the close (a located stock, no sink this cut);\n"
                    "  ledger = the region table holds exactly what the span credited; treas = the share paid in from\n"
                    "  capital treasuries; reg/pol = regions / living polities holding points, with a Gini over each\n"
                    "  (polities: every living one, zeros included) and the largest one's share; fuel factor = per mille\n"
                    "  over regions with centres, min/med/max, and how many sit at the floor (reading 0);\n"
                    "  rho(p,urb) = Spearman(points, urban heads) over regions with points or centres -- 1.000 would\n"
                    "  mean points only restate headcount; rho(int,fuel) = Spearman(points per urban head, fuel factor);\n"
                    "  inher = share on ground the span founded (fuel inherited under DEFAULT A); cost = the scale\n"
                    "  accrual's own wall clock over the span. [9] urban share of heads at 1660 -> the close.\n");
        std::printf("  seed |       points ledger treas | reg  gini  top | pol/liv  gini  top | fuel min/med/max flr |"
                    " rho(p,urb) rho(int,fuel) | inher (reg) | cost ms | [9] urban%% K -> C  verdict\n");
        for (const seed_row& r : rows)
        {
            if (!r.span_ran) continue;
            const auto f3 = [](double x, char* b, std::size_t n) {
                if (std::isnan(x)) std::snprintf(b, n, "    -");
                else std::snprintf(b, n, "%5.3f", x);
            };
            char tg[16], rg[16], rt[16], pg[16], pt[16], rpu[16], rif[16], ih[16];
            f3(r.treasury_share, tg, sizeof tg);
            f3(r.points_region_gini, rg, sizeof rg);
            f3(r.points_top_region, rt, sizeof rt);
            f3(r.points_polity_gini, pg, sizeof pg);
            f3(r.points_top_polity, pt, sizeof pt);
            f3(r.rho_points_urban, rpu, sizeof rpu);
            f3(r.rho_intensity_fuel, rif, sizeof rif);
            f3(r.inherited_share, ih, sizeof ih);
            const double u0 = r.at_1660.heads > 0
                ? 100.0 * static_cast<double>(r.at_1660.urban_heads) / static_cast<double>(r.at_1660.heads) : 0.0;
            const double u1 = r.at_close.heads > 0
                ? 100.0 * static_cast<double>(r.at_close.urban_heads) / static_cast<double>(r.at_close.heads) : 0.0;
            const bool u_ok = usable(r) && r.at_1660.heads > 0 && r.at_close.heads > 0;
            // Compared at the precision the fraction itself carries: a rise
            // is a strictly larger share of heads, never a rounded one.
            const bool rose = u_ok && static_cast<double>(r.at_close.urban_heads) * static_cast<double>(r.at_1660.heads)
                                    > static_cast<double>(r.at_1660.urban_heads) * static_cast<double>(r.at_close.heads);
            const char* verdict = !u_ok ? "unusable handoff" : rose ? "rising" : "FAILING (flat or falling)";
            std::printf("  %4u | %12lld %6s %5s | %3d %s %s | %3d/%-3d %s %s |    %4d/%4d/%4d %3d |"
                        "      %s         %s | %s (%3d) | %7.1f | %5.2f -> %5.2f  %s%s%s\n",
                        r.seed, static_cast<long long>(r.points_total),
                        !r.points_on ? "off" : r.points_rejected ? "REJ" : r.points_ledger_ok ? "ok" : "BROKEN",
                        tg, r.points_regions, rg, rt, r.points_polities, r.living_close, pg, pt,
                        r.fuel_factor[0], r.fuel_factor[2], r.fuel_factor[4], r.fuel_at_floor,
                        rpu, rif, ih, r.inherited_regions, static_cast<double>(r.ns_points) / 1.0e6,
                        u0, u1, verdict,
                        r.points_refused > 0 ? "  REFUSED CREDITS: " : "",
                        r.points_refused > 0 ? std::to_string(r.points_refused).c_str() : "");
        }

        // BL-1041 fix round: the honest headcount test, and where the points
        // stand by the ground's survey. rho(p,urb) above ranks 300 years of
        // points against 1960's heads alone; these rank SCALE points only
        // against the span's integrated urban head-years.
        std::printf("\n  THE HONEST HEADCOUNT TEST, and where the points stand (BL-1041 fix round):\n"
                    "  re-run = the span re-run harness-side at 1 point per treasury unit, its close equal to the\n"
                    "  shipped one outside the points (ok) and splitting every region's points exactly into scale\n"
                    "  and treasury (split); samples = decision years re-run to their top for the head-years integral\n"
                    "  (urban heads x the years each sample stands for, over rounds the region had centres);\n"
                    "  rho(S,hy) = Spearman(SCALE points, head-years) over regions with either -- 1.000 would mean\n"
                    "  scale points only restate time-integrated headcount; rho(S/hy,fuel) = Spearman(scale points\n"
                    "  per head-year, fuel factor). Of ALL points at the close: orig = on original settlement ground\n"
                    "  (surveyed from tiles by the settlement pass), resurv = on ground the sim founded before 1660\n"
                    "  (energy_q inherited, re-surveyed at the open), span = on ground the span founded (DEFAULT A).\n");
        std::printf("  seed | re-run split samples | regions  rho(S,hy) rho(S/hy,fuel) | orig (reg)    resurv (reg)  "
                    "span (reg)   | re-runs ms\n");
        for (const seed_row& r : rows)
        {
            if (!r.span_ran) continue;
            const auto f3 = [](double x, char* b, std::size_t n) {
                if (std::isnan(x)) std::snprintf(b, n, "    -");
                else std::snprintf(b, n, "%5.3f", x);
            };
            char a[16], b[16], so[16], sr[16], si[16];
            f3(r.rho_scale_hy, a, sizeof a);
            f3(r.rho_scale_int_fuel, b, sizeof b);
            f3(r.share_original, so, sizeof so);
            f3(r.share_resurveyed, sr, sizeof sr);
            f3(r.inherited_share, si, sizeof si);
            std::printf("  %4u |  %-5s %-5s %4d    | %6d    %s      %s      | %s (%5d) %s (%5d) %s (%4d) | %9lld\n",
                        r.seed,
                        integral_stride == 0 || !r.points_on ? "-" : r.rerun_ok ? "ok" : "DIFF",
                        integral_stride == 0 || !r.points_on ? "-" : r.split_ok ? "ok" : "BAD",
                        r.integral_samples, r.scale_regions, a, b,
                        so, r.regions_original, sr, r.regions_resurveyed, si, r.regions_span_founded,
                        static_cast<long long>(r.integral_ms));
        }
        if (integral_stride == 0)
            std::printf("  (--integral-stride 0: the re-runs were not taken; rho(S,hy) and rho(S/hy,fuel) read '-')\n");
    }

    const auto collect = [&](double (*get)(const seed_row&)) {
        std::vector<double> v;
        for (const seed_row& r : rows) v.push_back(get(r));
        return v;
    };
    // A handoff reading counts a seed only when the handoff exists, generation
    // recorded no violation, and the 1660 control matched its fingerprint.
    const auto handoff_collect = [&](double (*get)(const seed_row&)) {
        std::vector<double> v;
        for (const seed_row& r : rows)
            v.push_back(usable(r) ? get(r) : k_undef);
        return v;
    };
    const std::size_t N = rows.size();

    std::printf("\n=== THE THIRTEEN READINGS - a spread over %zu seeds, never a per-world verdict ===\n", N);
    std::printf("(%zu of %zu worlds ran the Empires round; %zu carry a usable handoff. 1660 control vs the\n"
                " library fingerprint: %zu checked, %zu mismatched. Handoff violations: %zu.)\n",
                generated, N, handoffs, prefix_checked, prefix_failed, violations);
    if (span_mode)
        std::printf("WHAT 'AT %lld' READS: the DIGITISATION SPAN's close. The shipped world (epoch 0) with the span\n"
                    "switched on: Exploration closes at 1660 as shipped, the span runs 1660 -> %lld as its own call\n"
                    "from the 1660 exploration_output (the span's own seed, Exploration's forces plus the Industry\n"
                    "tree), then world setup and the applied landscape search winner build on its close (the 12-tick\n"
                    "validation run is not mirrored). World setup reads the span's close for its grudges, roads,\n"
                    "junction markets and treasuries (BL-1053; the SETUP-DIFF lines above). Each line names its\n"
                    "surface and year.\n\n", T, T);
    else if (continued_mode)
        std::printf("WHAT 'AT %lld' READS: A 1200-NETWORK RUN, NOT THE SPAN. The shipped world (epoch 0) with\n"
                    "Exploration's own call continued to %lld - Exploration's forces only, pricing corridor income and\n"
                    "land trade on the network it inherited at 1200 - plus world setup and the applied landscape\n"
                    "search winner (the 12-tick validation run is not mirrored). A comparison for the span, never the\n"
                    "phase's reading. Each line names its surface and year.\n\n", T, T);
    else
        std::printf("WHAT 'AT 1960' READS HERE: nothing ran past 1660 (--through 1660). The world play opens on\n"
                    "is the 1660 Exploration close plus world setup and the applied landscape search winner (the\n"
                    "12-tick validation run is not mirrored). Run with --through 1960 for the Digitisation span.\n\n");

    // ---- 1 ------------------------------------------------------------------
    {
        std::printf("[ 1] Density follows cities - MEASURED on the campaign world built on the %lld close (firms\n"
                    "     laid by today's landscape search; the city charter budget meant to lay them does not exist,\n"
                    "     so running past 1660 moves this reading's inputs, not its cause)\n", T);
        std::printf("     firm = a corporation with >= 1 installation clearing against the market (market_for_tile);\n"
                    "     urban population = non-razed centres routed there; good count = distinct goods deposited\n"
                    "     on the catchment's tiles. Spearman rho across one world's markets:\n");
        print_spread("firm count vs catchment urban population",
                     collect([](const seed_row& r) { return r.rho_firms_urban; }));
        print_spread("firm count vs catchment good count",
                     collect([](const seed_row& r) { return r.rho_firms_goods; }));
        print_spread("markets on the home body",
                     collect([](const seed_row& r) { return static_cast<double>(r.markets); }));
        std::size_t urban_wins = 0, comparable = 0;
        for (const seed_row& r : rows)
        {
            if (std::isnan(r.rho_firms_urban) || std::isnan(r.rho_firms_goods)) continue;
            ++comparable;
            if (r.rho_firms_urban > r.rho_firms_goods) ++urban_wins;
        }
        std::printf("     worlds where rho(urban) > rho(goods): %zu of %zu comparable (%zu worlds undefined)\n\n",
                    urban_wins, comparable, N - comparable);
    }

    // ---- 2 ------------------------------------------------------------------
    {
        std::size_t orders = 0, preferred = 0;
        for (const seed_row& r : rows) { orders += r.buy_orders; preferred += r.buy_orders_with_preferred_seller; }
        std::printf("[ 2] Cultural stock - n/a: the campaign world carries no per-market preference (household\n"
                    "     demand weights by culture do not cross into play; `culture_preference` exists only at\n"
                    "     culture grain over the four sim good classes, mapped onto no market's goods) and no\n"
                    "     preferred-seller relationship is seeded.\n");
        std::printf("     evidence: buy orders in the generated worlds %zu, naming a preferred seller %zu\n\n",
                    orders, preferred);
    }

    // ---- 3 ------------------------------------------------------------------
    const bool r3_structural_zero = band_tier3_recipes == 0;
    {
        if (r3_structural_zero)
            std::printf("[ 3] Advanced chains - STRUCTURAL ZERO on the campaign world: the shipped descriptor's era\n"
                        "     band (it follows epoch_year, which stays 0 however far the span runs) admits no tier-3\n"
                        "     recipe, so no installation can make machinery, alloys or electronics on any seed. The\n"
                        "     counts below are the band, not a finding; the reading moves only when the epoch flip\n"
                        "     decouples the arc from the recipe band.\n");
        else
            std::printf("[ 3] Advanced chains - MEASURED on the campaign world (installations running a recipe that\n"
                        "     makes machinery, alloys or electronics - DIGITISATION.md sec 2's tier-3 goods; the\n"
                        "     wealth-to-production force that is meant to place them does not exist)\n");
        std::printf("     tier-3 recipes the campaign's era band admits: %d\n", band_tier3_recipes);
        print_spread("installations making a tier-3 good",
                     collect([](const seed_row& r) { return static_cast<double>(r.advanced); }));
        std::size_t realised = 0;
        for (const seed_row& r : rows) if (r.advanced > 0) ++realised;
        std::printf("     realised somewhere: %zu of %zu worlds\n", realised, N);
        print_spread("unevenly: Gini across nations",
                     collect([](const seed_row& r) { return r.advanced_gini; }));
        print_spread("Spearman across nations vs seeded treasury",
                     collect([](const seed_row& r) { return r.rho_adv_treasury; }));
        print_spread("Spearman across nations vs population (size)",
                     collect([](const seed_row& r) { return r.rho_adv_population; }));
        print_spread("Spearman across nations vs territory tiles (size)",
                     collect([](const seed_row& r) { return r.rho_adv_territory; }));
        std::printf("     (Gini and Spearman are undefined on a world where nothing is realised.)\n\n");
    }

    // ---- 4 ------------------------------------------------------------------
    {
        std::size_t battles = 0;
        for (const seed_row& r : rows) battles += r.battles_standing;
        std::printf("[ 4] Live conflict - n/a: no war condition exists to stand at the epoch - no province carries\n"
                    "     a standing war with a patron, and neither left-behind nor decolonised ground is a\n"
                    "     classification anything computes.\n");
        std::printf("     evidence: campaign battles standing in the generated worlds %zu\n\n", battles);
    }

    // ---- 5 ------------------------------------------------------------------
    {
        std::printf("[ 5] Mixed cities - PARTIAL\n");
        std::printf("     present - MEASURED off region culture shares at the 1660 control and the %lld close\n"
                    "     (checkered: second culture >= 95%% of the first, DIGITISATION.md sec 4). While no centre\n"
                    "     carries shares a province reads its region's, so a checkered land region is a checkered\n"
                    "     province set:\n", T);
        print_spread("checkered land regions per world, 1660 control",
                     handoff_collect([](const seed_row& r) { return static_cast<double>(r.at_1660.checkered_regions); }));
        print_spread("checkered land regions per world, at the close",
                     handoff_collect([](const seed_row& r) { return static_cast<double>(r.at_close.checkered_regions); }));
        print_spread("share of settled land regions checkered, at the close",
                     handoff_collect([](const seed_row& r) {
                         return r.at_close.settled_land_regions > 0
                             ? static_cast<double>(r.at_close.checkered_regions) / static_cast<double>(r.at_close.settled_land_regions)
                             : k_undef;
                     }));
        std::size_t present = 0;
        for (const seed_row& r : rows)
            if (usable(r) && r.at_close.checkered_regions > 0)
                ++present;
        std::printf("     present at the close in %zu of %zu worlds with a usable handoff\n", present, handoffs);
        std::printf("     concentrated in large centres - n/a: population centres carry no culture shares (the\n"
                    "     PROPOSED Beat 2 mechanism), so every province of a region reads alike and there is no\n"
                    "     city-grain mix to concentrate.\n\n");
    }

    // ---- 6 ------------------------------------------------------------------
    {
        std::printf("[ 6] Inequality - n/a: no GDP quantity exists (seeded GDP crosses into play and is not\n"
                    "     built), and no output is attributed to a city (installations belong to firms and clear\n"
                    "     against markets, never against a centre).\n");
        std::printf("     PROXY, NOT THE READING - seeded nation treasury per thousand heads (a stock, not GDP):\n");
        print_spread("Gini across nations",
                     collect([](const seed_row& r) { return r.treasury_per_head_gini; }));
        print_spread("max / median across nations",
                     collect([](const seed_row& r) { return r.treasury_per_head_max_on_med; }));
        std::printf("\n");
    }

    // ---- 7 ------------------------------------------------------------------
    {
        std::size_t unreadable = 0, any_far = 0;
        for (const seed_row& r : rows)
        {
            unreadable += static_cast<std::size_t>(r.at_close.flows_unreadable);
            if (usable(r) && r.at_close.flows_far > 0)
                ++any_far;
        }
        std::printf("[ 7] Far trade - MEASURED off the standing trade flows at the 1660 control and the %lld close,\n"
                    "     the only seeded trade relationships generation holds (nothing yet carries them into play as\n"
                    "     preferred sellers). A market is a living polity's capital seat; a flow is far when its buyer\n"
                    "     is strictly farther (region_distance) than the seller's nearest other market:\n", T);
        print_spread("far share of flows by count, 1660 control",
                     handoff_collect([](const seed_row& r) {
                         return r.at_1660.flows > 0 ? static_cast<double>(r.at_1660.flows_far) / static_cast<double>(r.at_1660.flows) : k_undef;
                     }));
        print_spread("far share of flows by count, at the close",
                     handoff_collect([](const seed_row& r) {
                         return r.at_close.flows > 0 ? static_cast<double>(r.at_close.flows_far) / static_cast<double>(r.at_close.flows) : k_undef;
                     }));
        print_spread("far share of flows by volume, at the close",
                     handoff_collect([](const seed_row& r) {
                         return r.at_close.volume > 0 ? static_cast<double>(r.at_close.volume_far) / static_cast<double>(r.at_close.volume) : k_undef;
                     }));
        print_spread("standing flows per world, 1660 control",
                     handoff_collect([](const seed_row& r) { return static_cast<double>(r.at_1660.flows); }));
        print_spread("standing flows per world, at the close",
                     handoff_collect([](const seed_row& r) { return static_cast<double>(r.at_close.flows); }));
        std::printf("     worlds with at least one far flow at the close: %zu of %zu with a usable handoff;\n"
                    "     unreadable flows %zu\n", any_far, handoffs, unreadable);
        std::printf("     NOT THIS READING'S SECOND HALF: haulage_measure's far-trade reading (BL-1006) ticks a year\n"
                    "     of play on epoch_year 1960, which is the superseded two-span arc with Exploration off; it\n"
                    "     moves with the epoch flip, not with --through.\n\n");
    }

    // ---- 8 ------------------------------------------------------------------
    {
        std::size_t lit_1660 = 0, lit_close = 0;
        for (const seed_row& r : rows)
        {
            lit_1660  += static_cast<std::size_t>(r.at_1660.industrialised_regions);
            lit_close += static_cast<std::size_t>(r.at_close.industrialised_regions);
        }
        if (!span_mode)
        {
            std::printf("[ 8] Industrialisation - n/a: industry points exist only inside the Digitisation span\n"
                        "     (BL-1041), and this run %s, so there is no holding to be uneven.\n",
                        continued_mode ? "is a 1200-network run with Exploration's forces only"
                                       : "stops at 1660 (run with --through 1960)");
        }
        else
        {
            // BL-1041 -- the points ARE the reading's observable: a located
            // stock, credited on regions with centres from scale, their own
            // fuel, Industry capacity and treasury paid in. The spread is
            // printed per seed above; this is the spread of those per-seed
            // numbers. Judgement, not a gate.
            std::size_t ran = 0, ledger_ok = 0, refused = 0, rejected = 0;
            for (const seed_row& r : rows)
            {
                if (!r.span_ran || !r.points_on) continue;
                ++ran;
                if (r.points_ledger_ok) ++ledger_ok;
                if (r.points_refused > 0) ++refused;
                if (r.points_rejected) ++rejected;
            }
            std::printf("[ 8] Industrialisation - MEASURED: industry points at the %lld close (BL-1041), the uneven\n"
                        "     holding the reading asks for, over %zu worlds that ran the span with points on\n"
                        "     (region table == credited on %zu; worlds with a refused credit %zu, with rejected\n"
                        "     constants %zu -- each must be 0 / all):\n", T, ran, ledger_ok, refused, rejected);
            const auto span_collect = [&](double (*get)(const seed_row&)) {
                std::vector<double> v;
                for (const seed_row& r : rows) v.push_back(r.span_ran && r.points_on ? get(r) : k_undef);
                return v;
            };
            print_spread("points per world at the close",
                         span_collect([](const seed_row& r) { return static_cast<double>(r.points_total); }));
            print_spread("Gini of points over regions holding any",
                         span_collect([](const seed_row& r) { return r.points_region_gini; }));
            print_spread("largest region's share of the world's points",
                         span_collect([](const seed_row& r) { return r.points_top_region; }));
            print_spread("Gini of points over living polities (zeros in)",
                         span_collect([](const seed_row& r) { return r.points_polity_gini; }));
            print_spread("largest polity's share of held points",
                         span_collect([](const seed_row& r) { return r.points_top_polity; }));
            print_spread("share paid in from capital treasuries",
                         span_collect([](const seed_row& r) { return r.treasury_share; }));
            print_spread("median fuel factor over centre regions (per mille)",
                         span_collect([](const seed_row& r) { return static_cast<double>(r.fuel_factor[2]); }));
            print_spread("fuel factor spread, max - min (per mille)",
                         span_collect([](const seed_row& r) {
                             return static_cast<double>(r.fuel_factor[4] - r.fuel_factor[0]); }));
            print_spread("rho(points, urban heads) - 1.0 would be headcount",
                         span_collect([](const seed_row& r) { return r.rho_points_urban; }));
            print_spread("rho(points per urban head, fuel factor)",
                         span_collect([](const seed_row& r) { return r.rho_intensity_fuel; }));
            // BL-1041 fix round: the lines above rank 300 years of points
            // against 1960's heads; these two are the honest test (scale points
            // only, against integrated head-years -- see the per-seed table).
            print_spread("rho(SCALE points, integrated urban head-years)",
                         span_collect([](const seed_row& r) { return r.rho_scale_hy; }));
            print_spread("rho(scale points per head-year, fuel factor)",
                         span_collect([](const seed_row& r) { return r.rho_scale_int_fuel; }));
            print_spread("share on original settlement ground (surveyed from tiles)",
                         span_collect([](const seed_row& r) { return r.share_original; }));
            print_spread("share on ground the sim founded before 1660 (re-surveyed)",
                         span_collect([](const seed_row& r) { return r.share_resurveyed; }));
            print_spread("share on ground the span founded (inherited fuel)",
                         span_collect([](const seed_row& r) { return r.inherited_share; }));
            print_spread("share on ground nobody holds",
                         span_collect([](const seed_row& r) { return r.points_unheld; }));
            print_spread("the scale accrual's cost over the span (ms)",
                         span_collect([](const seed_row& r) { return static_cast<double>(r.ns_points) / 1.0e6; }));
        }
        std::printf("     region half - STRUCTURAL ZERO: Stage 4's per-region lag is computed only for a settlement\n"
                    "     stop at or after 1700, and the shipped world's settlement stops at epoch 0, so no region can\n"
                    "     light a furnace however far the span runs. Regions industrialised, summed over the spread:\n"
                    "     1660 control %zu, close %zu.\n", lit_1660, lit_close);
        std::printf("     polity half - evidence, not the reading: living polities whose materials band crossed\n"
                    "     industrial (polity::industrial_year set):\n");
        print_spread("industrial polities per world, 1660 control",
                     handoff_collect([](const seed_row& r) { return static_cast<double>(r.at_1660.polities_industrial); }));
        print_spread("industrial polities per world, at the close",
                     handoff_collect([](const seed_row& r) { return static_cast<double>(r.at_close.polities_industrial); }));
        if (span_mode)
        {
            // BL-1040: the span opens the Industry tree (BL-1038), so its
            // uptake is evidence the span played the forces it was given --
            // still not the reading, which reads industry points.
            std::printf("     Industry tree - evidence, not the reading: the span opens it at 1660 to every living\n"
                        "     polity; living polities holding any Industry node at the close, and the nodes held:\n");
            print_spread("polities holding an Industry node, at the close",
                         handoff_collect([](const seed_row& r) { return static_cast<double>(r.at_close.polities_industry_nodes); }));
            print_spread("Industry nodes held, at the close",
                         handoff_collect([](const seed_row& r) { return static_cast<double>(r.at_close.industry_nodes); }));
        }
        std::printf("\n");
    }

    // ---- 9 ------------------------------------------------------------------
    if (past_1660)
    {
        // BL-1041: a world whose urban share did not rise FAILS this reading,
        // and a world with no cross-border stream fails its second clause --
        // printed as failing, never n/a: the reading's observable exists (the
        // share, and a stream count of zero), so a world that does not show
        // it is a refusal measured, not a reading nobody took.
        std::size_t r9_usable = 0, r9_rising = 0;
        std::string r9_failing;
        for (const seed_row& r : rows)
        {
            if (!usable(r) || r.at_1660.heads <= 0 || r.at_close.heads <= 0) continue;
            ++r9_usable;
            const bool up = static_cast<double>(r.at_close.urban_heads) * static_cast<double>(r.at_1660.heads)
                          > static_cast<double>(r.at_1660.urban_heads) * static_cast<double>(r.at_close.heads);
            if (up) ++r9_rising;
            else r9_failing += (r9_failing.empty() ? "" : " ") + std::to_string(r.seed);
        }
        std::printf("[ 9] Migration - FAILING on %zu of %zu worlds (urban share flat or falling%s), and on every world\n"
                    "     for its second clause (no cross-border stream)\n",
                    r9_usable - r9_rising, r9_usable,
                    r9_failing.empty() ? "" : (": seeds " + r9_failing).c_str());
        std::printf("     urban share rising - MEASURED off region urban_population / population at the 1660 control\n"
                    "     and the %lld close:\n", T);
        print_spread("urban share of heads, 1660 control",
                     handoff_collect([](const seed_row& r) {
                         return r.at_1660.heads > 0 ? static_cast<double>(r.at_1660.urban_heads) / static_cast<double>(r.at_1660.heads) : k_undef;
                     }));
        print_spread("urban share of heads, at the close",
                     handoff_collect([](const seed_row& r) {
                         return r.at_close.heads > 0 ? static_cast<double>(r.at_close.urban_heads) / static_cast<double>(r.at_close.heads) : k_undef;
                     }));
        std::size_t rising = 0;
        for (const seed_row& r : rows)
        {
            if (!(usable(r))) continue;
            if (r.at_1660.heads <= 0 || r.at_close.heads <= 0) continue;
            const double a = static_cast<double>(r.at_1660.urban_heads) / static_cast<double>(r.at_1660.heads);
            const double b = static_cast<double>(r.at_close.urban_heads) / static_cast<double>(r.at_close.heads);
            if (b > a) ++rising;
        }
        std::printf("     urban share rose across the span in %zu of %zu worlds with a usable handoff; FAILING on the\n"
                    "     other %zu\n", rising, handoffs, handoffs - std::min(rising, handoffs));
        std::printf("     at least one cross-border stream - FAILING on every world: 0 streams measured, because\n"
                    "     generation runs no cross-border migration stream (the PROPOSED Beat 2 mechanism).\n\n");
    }
    else
    {
        std::printf("[ 9] Migration - n/a: nothing ran past 1660 for an urban share to rise across (run with\n"
                    "     --through 1960), and generation runs no cross-border migration stream.\n\n");
    }

    // ---- 10 -----------------------------------------------------------------
    {
        std::printf("[10] Decolonisation - %s\n", past_1660 ? "MEASURED" : "n/a (the 1660 half only)");
        std::printf("     subjects = living polities with an overlord, at the 1660 control%s:\n",
                    past_1660 ? " and at the close" : "");
        print_spread("subjects at 1660",
                     handoff_collect([](const seed_row& r) { return static_cast<double>(r.at_1660.subjects); }));
        if (past_1660)
        {
            print_spread("subjects at the close",
                         handoff_collect([](const seed_row& r) { return static_cast<double>(r.at_close.subjects); }));
            print_spread("subjections formed after 1660",
                         handoff_collect([](const seed_row& r) {
                             return static_cast<double>(r.subjections_formed_close - r.subjections_formed_1660);
                         }));
            print_spread("subjects lost without a war after 1660 (refused-renewal secessions)",
                         handoff_collect([](const seed_row& r) {
                             return static_cast<double>(r.secessions_close - r.secessions_1660);
                         }));
            std::size_t fewer = 0, none_left = 0, peaceful_loss = 0, held_any = 0;
            for (const seed_row& r : rows)
            {
                if (!(usable(r))) continue;
                if (r.at_1660.subjects == 0) continue;
                ++held_any;
                if (r.at_close.subjects < r.at_1660.subjects) ++fewer;
                if (r.at_close.subjects == 0) ++none_left;
                if (r.secessions_close > r.secessions_1660) ++peaceful_loss;
            }
            std::printf("     of %zu worlds holding a subject at 1660: fewer at the close %zu, none left %zu, at least\n"
                        "     one lost without a war after 1660 %zu\n\n", held_any, fewer, none_left, peaceful_loss);
        }
        else
        {
            std::printf("     (a 1960 half needs --through 1960)\n\n");
        }
    }

    // ---- 11 -----------------------------------------------------------------
    std::printf("[11] World war - n/a: no world-war mechanism exists in any span, so presence cannot be read\n"
                "     (printing 'absent in every world' would be a refusal nobody measured).\n\n");

    // ---- 12 -----------------------------------------------------------------
    std::printf("[12] War dead - n/a: battles destroy armies, never population, in every span that runs\n"
                "     (settlement.hpp `region::army_stock`, Ben 2026-09-08); the war-death mechanisms are\n"
                "     Digitisation's and do not exist.\n\n");

    // ---- 13 -----------------------------------------------------------------
    std::printf("[13] Catastrophe lean - n/a: it splits worlds by world war (reading 11 is n/a) and reads\n"
                "     aggregate Alarm against Ceiling, and the campaign world seeds neither a per-nation Alarm\n"
                "     nor a Ceiling.\n\n");

    // ---- SUMMARY (BL-1029: the states are computed, not hand-typed) ----------
    {
        // BL-1041: 8 is measured wherever the span ran (its points); 9 is
        // measured past 1660 -- both clauses have an observable, the urban
        // share and a stream count of 0 -- and its verdict is FAILING where
        // the reading refuses it, printed at [9] and below, never n/a.
        std::string measured = "1, 7", partial = "5", zero, na = span_mode ? "2, 4, 6, 11, 12, 13"
                                                                      : "2, 4, 6, 8, 11, 12, 13";
        if (r3_structural_zero) zero = "3"; else measured = "1, 3, 7";
        if (span_mode) measured += ", 8";
        if (past_1660) { measured += ", 9, 10"; }
        else           { na += ", 9, 10"; }
        const auto count = [](const std::string& list) {
            return list.empty() ? 0 : 1 + static_cast<int>(std::count(list.begin(), list.end(), ','));
        };
        std::size_t r9_n = 0, r9_up = 0;
        for (const seed_row& r : rows)
        {
            if (!usable(r) || r.at_1660.heads <= 0 || r.at_close.heads <= 0) continue;
            ++r9_n;
            if (static_cast<double>(r.at_close.urban_heads) * static_cast<double>(r.at_1660.heads)
              > static_cast<double>(r.at_1660.urban_heads) * static_cast<double>(r.at_close.heads)) ++r9_up;
        }
        std::printf("SUMMARY at %lld (%s)  measured %d (%s) | partial %d (%s) | structural zero %d (%s) | n/a %d (%s);\n"
                    "         6 prints a labelled proxy; 8 prints its region half as a structural zero and its polity\n"
                    "         half as evidence%s. Prefix: %zu of %zu fingerprinted controls matched.\n",
                    T, close_name, count(measured), measured.c_str(), count(partial), partial.c_str(),
                    count(zero), zero.empty() ? "-" : zero.c_str(), count(na), na.c_str(),
                    span_mode ? ", beside its points" : "",
                    prefix_checked - prefix_failed, prefix_checked);
        if (past_1660)
            std::printf("         9 is FAILING on %zu of %zu worlds (urban share flat or falling) and on every world for\n"
                        "         its stream clause (0 cross-border streams).\n", r9_n - r9_up, r9_n);
    }

    // ---- JSON (BL-1029) -----------------------------------------------------
    if (!out_path.empty())
    {
        FILE* f = std::fopen(out_path.c_str(), "w");
        if (!f)
        {
            std::printf("Could not write %s\n", out_path.c_str());
        }
        else
        {
            const auto put_rho = [&](const char* key, double v, const char* tail) {
                if (std::isnan(v)) std::fprintf(f, "\"%s\": null%s", key, tail);
                else               std::fprintf(f, "\"%s\": %.4f%s", key, v, tail);
            };
            const auto put_handoff = [&](const char* key, const handoff_reading& h, const char* tail) {
                std::fprintf(f, "\"%s\": {\"settled_land_regions\": %d, \"checkered_regions\": %d, \"flows\": %d, "
                                "\"flows_far\": %d, \"flows_unreadable\": %d, \"volume\": %lld, \"volume_far\": %lld, "
                                "\"industrialised_regions\": %d, \"polities_industrial\": %d, \"urban_heads\": %lld, "
                                "\"heads\": %lld, \"subjects\": %d, \"alive_polities\": %d, "
                                "\"polities_industry_nodes\": %d, \"industry_nodes\": %d}%s",
                             key, h.settled_land_regions, h.checkered_regions, h.flows, h.flows_far, h.flows_unreadable,
                             (long long)h.volume, (long long)h.volume_far, h.industrialised_regions,
                             h.polities_industrial, (long long)h.urban_heads, (long long)h.heads, h.subjects,
                             h.alive_polities, h.polities_industry_nodes, h.industry_nodes, tail);
            };
            std::fprintf(f, "{\n \"_note\": \"BL-1029/BL-1040 digitisation_sim_harness per-seed table. mode 'span' (BL-1040): "
                            "close = the shipped world (epoch_year 0) with the Digitisation span run as its own call from the "
                            "1660 exploration_output to through_year; mode 'continued': a 1200-network run, Exploration's call "
                            "continued to through_year (BL-1029), a comparison and not the span; mode 'none': no span, close = "
                            "control. Control = the 1660 handoff re-run from the fixture and held to the seed library "
                            "fingerprint. span_ms is wall clock, reported only. Reported, not gated.\",\n");
            std::fprintf(f, " \"mode\": \"%s\",\n \"through_year\": %lld,\n \"band_tier3_recipes\": %d,\n \"worlds\": [\n",
                         span_mode ? "span" : (continued_mode ? "continued" : "none"), T, band_tier3_recipes);
            for (std::size_t i = 0; i < rows.size(); ++i)
            {
                const seed_row& r = rows[i];
                std::fprintf(f, "  {\"seed\": %u, \"era_ran\": %s, \"expl_ran\": %s, \"handoff_ok\": %s, "
                                "\"has_fingerprint\": %s, \"prefix_matches\": %s, \"control_battles\": %lld,\n",
                             r.seed, r.era_ran ? "true" : "false", r.expl_ran ? "true" : "false",
                             r.handoff_ok ? "true" : "false", r.has_fingerprint ? "true" : "false",
                             r.prefix_matches ? "true" : "false", (long long)r.control_battles);
                std::fprintf(f, "   \"close_ran\": %s, \"span_ran\": %s, \"span_start\": %lld, \"span_stop\": %lld, "
                                "\"span_rounds\": %lld, \"span_battles\": %lld, \"span_conquests\": %lld, "
                                "\"span_foundings\": %lld, \"span_ms\": %lld,\n",
                             r.close_ran ? "true" : "false", r.span_ran ? "true" : "false",
                             (long long)r.span_start, (long long)r.span_stop, (long long)r.span_rounds,
                             (long long)r.span_battles, (long long)r.span_conquests, (long long)r.span_foundings,
                             (long long)r.span_ms);
                // BL-1051: the span-open survey and the Fuel Doctrine.
                std::fprintf(f, "   \"survey_ran\": %s, \"survey_regions\": %d, \"survey_unseen\": %d, "
                                "\"survey_forest\": [%d, %d, %d, %d, %d], \"survey_forested\": %d, "
                                "\"survey_wooded\": %d, \"survey_fuel_med\": %d, \"survey_fuel_seam\": %d, "
                                "\"survey_seam_energy\": %d, \"close_regions\": %d, \"close_unsurveyed\": %d, "
                                "\"doctrine\": [%d, %d, %d], ",
                             r.survey_ran ? "true" : "false", r.survey_regions, r.survey_unseen,
                             r.survey_forest[0], r.survey_forest[1], r.survey_forest[2], r.survey_forest[3],
                             r.survey_forest[4], r.survey_forested, r.survey_wooded, r.survey_fuel_med,
                             r.survey_fuel_seam, r.survey_seam_energy, r.close_regions, r.close_unsurveyed,
                             r.doctrine[0], r.doctrine[1], r.doctrine[2]);
                put_rho("doctrine_forest_coke", r.doctrine_forest[0], ", ");
                put_rho("doctrine_forest_charcoal", r.doctrine_forest[1], ", ");
                put_rho("doctrine_forest_neither", r.doctrine_forest[2], ", ");
                std::fprintf(f, "\"charcoal_seam_seen\": %d, \"charcoal_seam_close\": %d,\n",
                             r.charcoal_seam_seen, r.charcoal_seam_close);
                // BL-1041: reading 8, industry points at the close.
                std::fprintf(f, "   \"points_on\": %s, \"points_rejected\": %s, \"points_total\": %lld, "
                                "\"points_scale\": %lld, \"points_treasury\": %lld, \"treasury_debited\": %lld, "
                                "\"points_refused\": %lld, "
                                "\"points_ledger_ok\": %s, \"points_regions\": %d, \"centre_regions\": %d, "
                                "\"points_polities\": %d, \"living_close\": %d, "
                                "\"fuel_factor\": [%d, %d, %d, %d, %d], \"fuel_at_floor\": %d, "
                                "\"inherited_regions\": %d, \"ns_points\": %lld, ",
                             r.points_on ? "true" : "false", r.points_rejected ? "true" : "false",
                             (long long)r.points_total, (long long)r.points_scale, (long long)r.points_treasury,
                             (long long)r.treasury_debited,
                             (long long)r.points_refused, r.points_ledger_ok ? "true" : "false",
                             r.points_regions, r.centre_regions, r.points_polities, r.living_close,
                             r.fuel_factor[0], r.fuel_factor[1], r.fuel_factor[2], r.fuel_factor[3],
                             r.fuel_factor[4], r.fuel_at_floor, r.inherited_regions, (long long)r.ns_points);
                put_rho("points_region_gini", r.points_region_gini, ", ");
                put_rho("points_top_region", r.points_top_region, ", ");
                put_rho("points_polity_gini", r.points_polity_gini, ", ");
                put_rho("points_top_polity", r.points_top_polity, ", ");
                put_rho("points_unheld", r.points_unheld, ", ");
                put_rho("treasury_share", r.treasury_share, ", ");
                put_rho("rho_points_urban", r.rho_points_urban, ", ");
                put_rho("rho_intensity_fuel", r.rho_intensity_fuel, ", ");
                put_rho("inherited_share", r.inherited_share, ",\n");
                // BL-1041 fix round: the honest headcount test and the survey split.
                std::fprintf(f, "   \"rerun_ok\": %s, \"rerun_mismatch\": %d, \"split_ok\": %s, "
                                "\"integral_samples\": %d, \"integral_ms\": %lld, \"scale_regions\": %d, "
                                "\"regions_original\": %d, \"regions_resurveyed\": %d, \"regions_span_founded\": %d, ",
                             r.rerun_ok ? "true" : "false", r.rerun_mismatch, r.split_ok ? "true" : "false",
                             r.integral_samples, (long long)r.integral_ms, r.scale_regions,
                             r.regions_original, r.regions_resurveyed, r.regions_span_founded);
                put_rho("rho_scale_hy", r.rho_scale_hy, ", ");
                put_rho("rho_scale_int_fuel", r.rho_scale_int_fuel, ", ");
                put_rho("share_original", r.share_original, ", ");
                put_rho("share_resurveyed", r.share_resurveyed, ",\n");
                std::fprintf(f, "   \"markets\": %d, \"markets_urban\": %d, \"corps_on_body\": %d, ",
                             r.markets, r.markets_urban, r.corps_on_body);
                put_rho("rho_firms_urban", r.rho_firms_urban, ", ");
                put_rho("rho_firms_goods", r.rho_firms_goods, ",\n");
                std::fprintf(f, "   \"installations\": %lld, \"advanced\": %lld, \"nations\": %d, "
                                "\"nations_with_advanced\": %d, \"nations_with_heads\": %d, ",
                             (long long)r.installations, (long long)r.advanced, r.nations,
                             r.nations_with_advanced, r.nations_with_heads);
                put_rho("treasury_per_head_gini", r.treasury_per_head_gini, ", ");
                put_rho("treasury_per_head_max_on_med", r.treasury_per_head_max_on_med, ",\n   ");
                put_handoff("at_1660", r.at_1660, ",\n   ");
                put_handoff("at_close", r.at_close, ",\n");
                std::fprintf(f, "   \"secessions_1660\": %lld, \"secessions_close\": %lld, "
                                "\"subjections_formed_1660\": %lld, \"subjections_formed_close\": %lld, "
                                "\"battles_standing\": %zu, \"buy_orders\": %zu, \"buy_orders_with_preferred_seller\": %zu, "
                                "\"trade_routes\": %zu}%s\n",
                             (long long)r.secessions_1660, (long long)r.secessions_close,
                             (long long)r.subjections_formed_1660, (long long)r.subjections_formed_close,
                             r.battles_standing, r.buy_orders, r.buy_orders_with_preferred_seller, r.trade_routes,
                             i + 1 < rows.size() ? "," : "");
            }
            std::fprintf(f, " ]\n}\n");
            std::fclose(f);
            std::printf("\nWrote %s (%zu rows)\n", out_path.c_str(), rows.size());
        }
    }

    std::printf("REPORT ONLY - no reading is asserted; the spread above is for judgement.\n");
    return 0;
}
