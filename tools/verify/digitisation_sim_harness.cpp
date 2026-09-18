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
// GATE 3 (BL-1040 R5): THE SHIPPED SPAN IS THE REAL RESUME. V4 below -- the
//   handoff as it is, the span's own seed and params, run to the span's own
//   stop -- folded with `make_digitisation_output`, equals generation's own
//   1960 close (`era_minus_one_fixture::digitisation_handoff`) field for field
//   and table for table, with the same battle, conquest and founding counts.
//   This is what makes the span "the real consumer of the resume" a checked
//   claim: the resume gates 1 and 2 prove lossless is the resume the shipped
//   world runs, and no settlement or creed state outside the struct reaches it.
// REPORT: the one-round footprint of the real resume, the entry state the
//   handoff differs from the continued run in, and the 1960 divergence
//   against the continued run, attributed by source by adding them back one
//   at a time (V0 all neutralised .. V4 the real resume on its own seed).
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
}
constexpr int k_region_fields = 51; // counts the FID_CMP lines above; keep them equal

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
};

divergence diverge(const run_out& v, const run_out& c)
{
    divergence d;
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

    // The 1960 divergence, V0..V4 against the continued run, each step
    // against the variant before it, and the continued run's own numbers.
    divergence v[5];
    divergence step[5]; ///< step[k] = V_k against V_{k-1} (step[0] unused)
    int        c_living = 0;
    int64_t    c_battles = 0;
    int64_t    c_treasury = 0;
    double     seconds = 0.0;

    // Gate 3 (BL-1040) -- the shipped span against V4, the real resume.
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
            resume_spec v4 = v3;                 // + the span's own seed: the real resume
            v4.seed = own_seed;
            const resume_spec* specs[5] = { &neutral, &v1, &v2, &v3, &v4 };
            run_out prev;
            for (int k = 0; k < 5; ++k)
            {
                run_out cur = resume(fx, H, *specs[k], stop, INT64_MIN, dp);
                row.v[k] = diverge(cur, c_1960);
                if (k > 0) row.step[k] = diverge(cur, prev);
                prev = std::move(cur);
            }

            // ---- Gate 3 (BL-1040): the shipped span IS V4 ---------------------
            // `prev` is V4: the handoff as it is, on the span's own seed and
            // params, to the span's own stop. Folded by the span's own rule,
            // it must be generation's 1960 close exactly.
            const digitisation_output V = make_digitisation_output(prev.ss, prev.hs, &prev.cs);
            const digitisation_output& S = fx.digitisation_handoff;
            const history_sim_state&   T = fx.digitisation_state;
            row.span_regions  = census_of(V.regions, S.regions, region_fields);
            row.span_polities = census_of(V.polities, S.polities, polity_fields);
            const auto note = [&](const char* table, const std::string& d) {
                if (!d.empty()) row.span_tables.push_back(std::string(table) + ": " + d);
            };
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
        for (const polity& q : c_1960.hs.polities) if (q.alive) ++row.c_living;
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
                "  (the handoff as it is) | V4 + the span's own seed (the real resume)\n");
    std::printf("  Against C: regions owned differently, living polities, treaty pairs differing, battles 1660-1960,\n"
                "  summed region treasury (x1e3). By source: each variant against the one before it (the source's\n"
                "  own footprint, 'regions owned differently / treaty pairs differing').\n");
    for (const seed_fidelity& r : rows)
    {
        if (!r.ran) continue;
        std::printf("  %5u  owner  V0..V4 %4d %4d %4d %4d %4d | living C %3d: %3d %3d %3d %3d %3d | treaty pairs "
                    "%3d %3d %3d %3d %3d\n",
                    r.seed, r.v[0].owner_regions, r.v[1].owner_regions, r.v[2].owner_regions,
                    r.v[3].owner_regions, r.v[4].owner_regions,
                    r.c_living, r.v[0].living, r.v[1].living, r.v[2].living, r.v[3].living, r.v[4].living,
                    r.v[0].treaty_pairs, r.v[1].treaty_pairs, r.v[2].treaty_pairs, r.v[3].treaty_pairs,
                    r.v[4].treaty_pairs);
        std::printf("         battles C %4lld: %4lld %4lld %4lld %4lld %4lld | treasury C %lld: %lld %lld %lld %lld %lld\n",
                    static_cast<long long>(r.c_battles), static_cast<long long>(r.v[0].battles),
                    static_cast<long long>(r.v[1].battles), static_cast<long long>(r.v[2].battles),
                    static_cast<long long>(r.v[3].battles), static_cast<long long>(r.v[4].battles),
                    static_cast<long long>(r.c_treasury / 1000), static_cast<long long>(r.v[0].treasury / 1000),
                    static_cast<long long>(r.v[1].treasury / 1000), static_cast<long long>(r.v[2].treasury / 1000),
                    static_cast<long long>(r.v[3].treasury / 1000), static_cast<long long>(r.v[4].treasury / 1000));
        std::printf("         by source: market stamp %d/%d | 1660 record %d/%d | dead-filter %d/%d | seed %d/%d | "
                    "residual %d/%d\n",
                    r.step[1].owner_regions, r.step[1].treaty_pairs, r.step[2].owner_regions, r.step[2].treaty_pairs,
                    r.step[3].owner_regions, r.step[3].treaty_pairs, r.step[4].owner_regions, r.step[4].treaty_pairs,
                    r.v[0].owner_regions, r.v[0].treaty_pairs);
    }
    {
        // Attribution: each source's own footprint (the variant against the
        // one before it), spread over the seeds.
        const char* names[5] = { "residual: V0 against C", "the close's market stamp: V1 against V0",
                                 "the 1660 corridor record: V2 against V1", "the dead-filter: V3 against V2",
                                 "the seed: V4 against V3" };
        std::printf("  regions owned differently at 1960, per source:\n");
        for (int k = 0; k < 5; ++k)
        {
            std::vector<double> inc;
            for (const seed_fidelity& r : rows)
                if (r.ran)
                    inc.push_back(static_cast<double>(k == 0 ? r.v[0].owner_regions : r.step[k].owner_regions));
            print_spread(names[k], inc);
        }
        std::printf("    (a footprint is measured against the variant before it; sources interact, so footprints\n"
                    "     do not sum to V4's distance from C)\n");
    }

    int g3 = 0;
    std::printf("\n=== GATE 3 (BL-1040 R5): the shipped Digitisation span IS the real resume (V4), field for field ===\n");
    std::printf("  (generation's own 1960 close against V4 folded by make_digitisation_output; the span's params\n"
                "   checked against its derivation; the shipped close's own validator re-run against the 1660 value)\n");
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
    for (int a = 1; a < argc; ++a)
    {
        if (std::strcmp(argv[a], "--limit") == 0 && a + 1 < argc)
        {
            limit = std::atoi(argv[++a]);
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
                        "[--through Y [--continued]] [--out path] [--fidelity] [--resume-tier]\n", argv[a]);
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
        std::printf("[ 8] Industrialisation - n/a: industry points do not exist (Beat 1: cities make industry\n"
                    "     points), so there is no holding to be uneven and no fuel gate to show in it.\n");
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
        std::printf("[ 9] Migration - PARTIAL\n");
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
        std::printf("     urban share rose across the span in %zu of %zu worlds with a usable handoff\n", rising, handoffs);
        std::printf("     at least one cross-border stream - n/a: generation runs no cross-border migration stream\n"
                    "     (the PROPOSED Beat 2 mechanism).\n\n");
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
        std::string measured = "1, 7", partial = "5", zero, na = "2, 4, 6, 8, 11, 12, 13";
        if (r3_structural_zero) zero = "3"; else measured = "1, 3, 7";
        if (past_1660) { measured += ", 10"; partial += ", 9"; }
        else           { na += ", 9, 10"; }
        const auto count = [](const std::string& list) {
            return list.empty() ? 0 : 1 + static_cast<int>(std::count(list.begin(), list.end(), ','));
        };
        std::printf("SUMMARY at %lld (%s)  measured %d (%s) | partial %d (%s) | structural zero %d (%s) | n/a %d (%s);\n"
                    "         6 prints a labelled proxy; 8 prints its region half as a structural zero and its polity\n"
                    "         half as evidence. Prefix: %zu of %zu fingerprinted controls matched.\n",
                    T, close_name, count(measured), measured.c_str(), count(partial), partial.c_str(),
                    count(zero), zero.empty() ? "-" : zero.c_str(), count(na), na.c_str(),
                    prefix_checked - prefix_failed, prefix_checked);
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
