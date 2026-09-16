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
// WHAT "AT 1960" READS TODAY. There is no Digitisation span. The world the
// campaign opens on is the Exploration close (1660 CE) plus world setup —
// nations, centres, markets — plus the landscape the search applied. So every
// reading below is taken off one of two surfaces, and each printed line says
// which:
//   * the CAMPAIGN WORLD as the app builds it, up to the applied search
//     winner (readings 1, 3, 6's proxy, and the evidence counts); or
//   * the Exploration HANDOFF (`era_minus_one_fixture::exploration_handoff`),
//     the explicit 1660 value the next phase is meant to consume (readings
//     5, 7, 10's 1660 half, and 8's evidence).
//
// THREE STATES, NEVER A FOURTH. A reading is MEASURED (its observable exists
// in today's world and is computed off it), PARTIAL (one clause is measured,
// the other prints n/a with its reason), or n/a (the mechanism it reads does
// not exist; the line names the missing piece and, where a count proves the
// absence, prints that count). A reading is never approximated into a pass: a
// PROXY line, where one is printed, is labelled as not the reading.
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
// Usage:  digitisation_sim_harness [--limit N] [--seeds a,b,c]
//   --limit N    take the library's first N seeds (a quick run)
//   --seeds ...  measure these seeds instead of the library's
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
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
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
// ONE SEED'S READING. Everything a reading needs is captured here in the main
// loop, so the report below reads `rows` only and no section regenerates.
// ---------------------------------------------------------------------------
struct seed_row
{
    uint32_t seed = 0;
    bool     era_ran  = false; ///< The Empires round ran (fx.ran).
    bool     expl_ran = false; ///< The Exploration span ran, so the 1660 handoff exists.

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

    // --- Reading 5: mixed cities, present half (1660 handoff) ---------------
    int settled_land_regions = 0;
    int checkered_regions    = 0;

    // --- Reading 6: PROXY only (campaign world) -----------------------------
    int    nations_with_heads = 0;
    double treasury_per_head_gini       = k_undef;
    double treasury_per_head_max_on_med = k_undef;

    // --- Reading 7: far trade (1660 handoff) --------------------------------
    int     flows       = 0;
    int     flows_far   = 0;
    int     flows_unreadable = 0; ///< a flow whose party or capital is out of range.
    int64_t volume      = 0;
    int64_t volume_far  = 0;

    // --- Reading 10: the 1660 half (1660 handoff) ---------------------------
    int subjects_1660 = 0;

    // --- Evidence counts behind the n/a lines -------------------------------
    int         industrialised_regions_1660 = 0;
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

} // namespace

int main(int argc, char** argv)
{
    // --- Arguments ---------------------------------------------------------
    std::vector<uint32_t> seeds;
    bool seeds_from_args = false;
    int  limit = -1;
    for (int a = 1; a < argc; ++a)
    {
        if (std::strcmp(argv[a], "--limit") == 0 && a + 1 < argc)
        {
            limit = std::atoi(argv[++a]);
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
            std::printf("unknown argument '%s'\nusage: digitisation_sim_harness [--limit N] [--seeds a,b,c]\n", argv[a]);
            return 2;
        }
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

    std::printf("=== digitisation_sim_harness (BL-982) - the thirteen Digitisation readings ===\n");
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

    for (uint32_t seed : seeds)
    {
        world_params wp = shipped_descriptor;
        wp.seed = seed;

        std::fprintf(stderr, "[digitisation] seed %u generating\n", seed);
        generation_report     rep;
        era_minus_one_fixture fx;
        world w = make_hard_coded_world(wp, &rep, cfg, /*progress=*/nullptr, &works, &fx);

        reg.set_era(era_band_for_epoch(wp.epoch_year)); // app::load_economy
        const shipped_landscape land = apply_shipped_landscape(w, reg, wp.seed);
        std::printf("seed %u ", seed);
        print_shipped_landscape(land);
        std::fflush(stdout);

        seed_row row;
        row.seed     = seed;
        row.era_ran  = fx.ran;
        row.expl_ran = fx.exploration_ran;

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

        // ================ The 1660 handoff: readings 5, 7, 8, 10 ============
        if (fx.exploration_ran)
        {
            const exploration_output& h = fx.exploration_handoff;
            const std::vector<region>& R = h.regions;
            const std::vector<polity>& P = h.polities;

            // Reading 5, present half: DIGITISATION.md sec 4 — checkered when
            // the second culture's share is at least 95% of the first's.
            for (const region& rg : R)
            {
                if (rg.industrialised) ++row.industrialised_regions_1660;
                if (rg.domain != region_domain::land || rg.culture.empty()) continue;
                ++row.settled_land_regions;
                const int w0 = rg.culture.weight_q[0];
                const int w1 = rg.culture.weight_q[1];
                if (rg.culture.id[1] >= 0 && w1 > 0 && 100 * w1 >= 95 * w0)
                    ++row.checkered_regions;
            }

            // Reading 10's 1660 half.
            for (const polity& q : P)
                if (q.alive && q.overlord >= 0) ++row.subjects_1660;

            // Reading 7. A market here is a LIVING polity's capital seat: the
            // flow model sizes a flow at the buyer's capital, so these are the
            // markets any flow could have landed at. A flow is FAR when its
            // buyer's market is strictly farther from the seller's than the
            // seller's nearest other market; a tie is not far.
            std::vector<int> market_regions;
            for (const polity& q : P)
                if (q.alive && q.capital >= 0 && static_cast<std::size_t>(q.capital) < R.size())
                    market_regions.push_back(q.capital);
            std::sort(market_regions.begin(), market_regions.end());
            market_regions.erase(std::unique(market_regions.begin(), market_regions.end()),
                                 market_regions.end());

            for (const trade_flow& f : h.trade_flows)
            {
                ++row.flows;
                row.volume += f.volume_q;
                if (f.seller >= P.size() || f.buyer >= P.size()) { ++row.flows_unreadable; continue; }
                const int sc = P[f.seller].capital;
                const int bc = P[f.buyer].capital;
                if (sc < 0 || bc < 0 || static_cast<std::size_t>(sc) >= R.size()
                 || static_cast<std::size_t>(bc) >= R.size()) { ++row.flows_unreadable; continue; }
                const int d_buyer = region_distance(R[static_cast<std::size_t>(sc)],
                                                    R[static_cast<std::size_t>(bc)], fx.gw);
                int d_nearest = std::numeric_limits<int>::max();
                for (int m : market_regions)
                {
                    if (m == sc) continue;
                    d_nearest = std::min(d_nearest, region_distance(R[static_cast<std::size_t>(sc)],
                                                                    R[static_cast<std::size_t>(m)], fx.gw));
                }
                if (d_buyer > d_nearest)
                {
                    ++row.flows_far;
                    row.volume_far += f.volume_q;
                }
            }
        }

        // ================ Evidence behind the n/a lines ======================
        row.battles_standing = w.battles.size();
        row.buy_orders       = w.buy_orders.size();
        for (const buy_order& bo : w.buy_orders)
            if (bo.preferred_seller != null_entity) ++row.buy_orders_with_preferred_seller;
        row.trade_routes = w.trade_routes.size();

        rows.push_back(row);
    }

    std::size_t generated = 0, handoffs = 0;
    for (const seed_row& r : rows)
    {
        if (r.era_ran) ++generated;
        if (r.expl_ran) ++handoffs;
    }
    if (rows.empty())
    {
        std::printf("FATAL  no seed generated\n");
        return 1;
    }

    // ======================= Per-seed table =================================
    std::printf("\n=== per seed (inputs to the spread; not verdicts) ===\n");
    std::printf("  seed | mkts urb corps  rho(f,urb) rho(f,gds) | inst  adv nat nat+adv | chk/settled | "
                "flows far  vol  volfar | subj1660 | tph gini\n");
    const auto fmt_rho = [](double x, char* buf, std::size_t len) {
        if (std::isnan(x)) std::snprintf(buf, len, "  undef");
        else std::snprintf(buf, len, "%7.3f", x);
    };
    for (const seed_row& r : rows)
    {
        char a[16], b[16], c[16];
        fmt_rho(r.rho_firms_urban, a, sizeof a);
        fmt_rho(r.rho_firms_goods, b, sizeof b);
        fmt_rho(r.treasury_per_head_gini, c, sizeof c);
        std::printf("  %4u | %4d %3d %5d    %s    %s | %5lld %4lld %3d %7d | %4d/%-6d | %5d %3d %5lld %6lld | %8d | %s%s\n",
                    r.seed, r.markets, r.markets_urban, r.corps_on_body, a, b,
                    static_cast<long long>(r.installations), static_cast<long long>(r.advanced),
                    r.nations, r.nations_with_advanced,
                    r.checkered_regions, r.settled_land_regions,
                    r.flows, r.flows_far, static_cast<long long>(r.volume),
                    static_cast<long long>(r.volume_far), r.subjects_1660, c,
                    r.expl_ran ? "" : "  (no Exploration handoff)");
    }

    const auto collect = [&](double (*get)(const seed_row&)) {
        std::vector<double> v;
        for (const seed_row& r : rows) v.push_back(get(r));
        return v;
    };
    const auto handoff_collect = [&](double (*get)(const seed_row&)) {
        std::vector<double> v;
        for (const seed_row& r : rows) v.push_back(r.expl_ran ? get(r) : k_undef);
        return v;
    };
    const std::size_t N = rows.size();

    std::printf("\n=== THE THIRTEEN READINGS - a spread over %zu seeds, never a per-world verdict ===\n", N);
    std::printf("(%zu of %zu worlds ran the Empires round; %zu carry the 1660 Exploration handoff.)\n",
                generated, N, handoffs);
    std::printf("WHAT 'AT 1960' READS TODAY: the Digitisation span (1660 -> 1960) does not exist. The world\n"
                "play opens on is the 1660 Exploration close plus world setup and the applied landscape\n"
                "search winner (the 12-tick validation run is not mirrored). Each line names its surface.\n\n");

    // ---- 1 ------------------------------------------------------------------
    {
        std::printf("[ 1] Density follows cities - MEASURED on the campaign world (firms laid by today's\n"
                    "     landscape search; the city charter budget that is meant to lay them does not exist)\n");
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
    {
        std::printf("[ 3] Advanced chains - MEASURED on the campaign world (installations running a recipe that\n"
                    "     makes machinery, alloys or electronics - DIGITISATION.md sec 2's tier-3 goods; the\n"
                    "     wealth-to-production force that is meant to place them does not exist)\n");
        std::printf("     tier-3 recipes the campaign's era band admits: %d\n", band_tier3_recipes);
        if (band_tier3_recipes == 0)
            std::printf("     STRUCTURAL ZERO: the shipped descriptor's era band admits no tier-3 recipe, so no\n"
                        "     installation can make one on any seed - the count below is not a finding about the\n"
                        "     spread, it is the band.\n");
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
        std::printf("     present - MEASURED off the 1660 handoff's region culture shares (checkered: second\n"
                    "     culture >= 95%% of the first, DIGITISATION.md sec 4). While no centre carries shares a\n"
                    "     province reads its region's, so a checkered land region is a checkered province set:\n");
        print_spread("checkered land regions per world",
                     handoff_collect([](const seed_row& r) { return static_cast<double>(r.checkered_regions); }));
        print_spread("share of settled land regions checkered",
                     handoff_collect([](const seed_row& r) {
                         return r.settled_land_regions > 0
                             ? static_cast<double>(r.checkered_regions) / static_cast<double>(r.settled_land_regions)
                             : k_undef;
                     }));
        std::size_t present = 0;
        for (const seed_row& r : rows) if (r.expl_ran && r.checkered_regions > 0) ++present;
        std::printf("     present in %zu of %zu worlds with a handoff\n", present, handoffs);
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
            unreadable += static_cast<std::size_t>(r.flows_unreadable);
            if (r.expl_ran && r.flows_far > 0) ++any_far;
        }
        std::printf("[ 7] Far trade - MEASURED off the 1660 handoff's standing trade flows, the only seeded trade\n"
                    "     relationships generation holds (nothing yet carries them into play as preferred\n"
                    "     sellers). A market is a living polity's capital seat; a flow is far when its buyer is\n"
                    "     strictly farther (region_distance) than the seller's nearest other market:\n");
        print_spread("far share of flows, by count",
                     handoff_collect([](const seed_row& r) {
                         return r.flows > 0 ? static_cast<double>(r.flows_far) / static_cast<double>(r.flows) : k_undef;
                     }));
        print_spread("far share of flows, by volume",
                     handoff_collect([](const seed_row& r) {
                         return r.volume > 0 ? static_cast<double>(r.volume_far) / static_cast<double>(r.volume) : k_undef;
                     }));
        print_spread("standing flows per world",
                     handoff_collect([](const seed_row& r) { return static_cast<double>(r.flows); }));
        std::printf("     worlds with at least one far flow: %zu of %zu with a handoff; unreadable flows %zu\n\n",
                    any_far, handoffs, unreadable);
    }

    // ---- 8 ------------------------------------------------------------------
    {
        std::size_t lit = 0;
        for (const seed_row& r : rows) lit += static_cast<std::size_t>(r.industrialised_regions_1660);
        std::printf("[ 8] Industrialisation - n/a: industry points do not exist (Beat 1: cities make industry\n"
                    "     points), so there is no holding to be uneven and no fuel gate to show in it.\n");
        std::printf("     evidence: regions industrialised at the 1660 handoff, summed over the spread %zu\n\n", lit);
    }

    // ---- 9 ------------------------------------------------------------------
    std::printf("[ 9] Migration - n/a: there is no 1660 -> 1960 span for an urban share to rise across, and\n"
                "     generation runs no cross-border migration stream.\n\n");

    // ---- 10 -----------------------------------------------------------------
    {
        std::printf("[10] Decolonisation - n/a: it compares subjects at 1960 against 1660, and no 1960 polity map\n"
                    "     exists; losing a subject without a war is likewise unrecorded. The 1660 half, MEASURED\n"
                    "     off the handoff (living polities with an overlord):\n");
        print_spread("subjects at 1660",
                     handoff_collect([](const seed_row& r) { return static_cast<double>(r.subjects_1660); }));
        std::size_t with = 0;
        for (const seed_row& r : rows) if (r.expl_ran && r.subjects_1660 > 0) ++with;
        std::printf("     worlds holding at least one subject at 1660: %zu of %zu (a world with none cannot\n"
                    "     decolonise)\n\n", with, handoffs);
    }

    // ---- 11 -----------------------------------------------------------------
    std::printf("[11] World war - n/a: no world-war mechanism exists in any span, so presence cannot be read\n"
                "     (printing 'absent in every world' would be a refusal nobody measured).\n\n");

    // ---- 12 -----------------------------------------------------------------
    std::printf("[12] War dead - n/a: there are no 1660 -> 1960 wars, and in the spans that do exist battles\n"
                "     destroy armies, never population (settlement.hpp `region::army_stock`, Ben 2026-09-08).\n\n");

    // ---- 13 -----------------------------------------------------------------
    std::printf("[13] Catastrophe lean - n/a: it splits worlds by world war (reading 11 is n/a) and reads\n"
                "     aggregate Alarm against Ceiling, and the campaign world seeds neither a per-nation Alarm\n"
                "     nor a Ceiling.\n\n");

    std::printf("SUMMARY  measured 3 (1, 3, 7) | partial 1 (5) | n/a 9 (2, 4, 6, 8, 9, 10, 11, 12, 13);\n"
                "         6 prints a labelled proxy and 10 its 1660 half beside the n/a.\n");
    std::printf("REPORT ONLY - no reading is asserted; the spread above is for judgement.\n");
    return 0;
}
