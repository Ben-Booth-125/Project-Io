// ---------------------------------------------------------------------------
// boom_probe — BL-1227 (idle mines), review round 2 diagnosis
// ---------------------------------------------------------------------------
// QUESTION. With the stack-aware site ranking, play placed ~2,949 new
// extraction sites over 5 seeds (fibre 1,764, hides 473). Why do fibre and
// hides look endlessly profitable to the scorer?
//
// THE WORLD. Seated as idle_mines_probe / market_viability seat it: the
// 12-tick settle, seat_player_corporation, then PLAY ticks as the app steps
// them. A PURE READER: nothing it does writes the world.
//
// PER NEW SITE (any watched good, placed in play t >= 1), read at the lap
// before the scorer ran (lap 0, convoys — the economy step, where the corp AI
// runs, is lap 1):
//   * the price the scorer assumed (local_price: cleared price, else base),
//     the base, the market's PUBLIC demand and supply, its shelf;
//   * the scorer's own estimate re-derived (rate, rank, revenue, net) and the
//     glut forecast's ratio (supply + added x horizon) / demand, or
//     "no demand" when demand is 0 (the forecast then returns 1.0);
//   * siblings: other new sites of the same good in the same market this tick;
//   * who buys the good in that market: running processors whose recipe
//     consumes it, and the household bid.
// Then over the site's FIRST 20 OPERATING TICKS (after construction): its
// realised revenue, upkeep and wages (estimate_building_profit, the reflex's
// own figure), the price after each clear, the shelf, and whether it idled.
//
// Usage: build_gen/verify/boom_probe.exe [--seeds a,b] [--ticks N]
// Build:  ./tools/verify/build_lua_harness.sh boom_probe
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/building_profit.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/corp_ai.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/orbital_system.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/spawn_seat.hpp"
#include "world/survey_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr int k_op_window = 20;

std::string gname(std::size_t g) { return resource_names::name_of(static_cast<resource_type>(g)); }

struct mkt_snap
{
    float price = 0, base = 0, demand = 0, supply = 0, inv = 0, hh_bid = 0;
};

struct tracked
{
    entity_id bid = null_entity, tile = null_entity, market = null_entity;
    std::size_t r = 0;
    int placed = 0, rank = 1, siblings = 0, consumers = 0;
    mkt_snap at;
    float est_rev = 0, est_net = 0, glut_ratio = -1; // -1: no demand -> no forecast
    int op_ticks = 0;
    bool idled = false;
    double rev = 0, maint = 0, wages = 0, out = 0;
    float price_first = 0, price_last = 0, inv_first = 0, inv_last = 0, dem_last = 0;
};

struct good_tally
{
    long n = 0, no_demand = 0, base_price = 0, with_consumer = 0;
    double p_ratio = 0, glut = 0; long glut_n = 0;
    double est_rev = 0, est_net = 0, siblings = 0, demand = 0, supply = 0, hh = 0;
    long matured = 0, idled = 0;
    double rev = 0, maint = 0, wages = 0, out = 0;
    double pf = 0, pl = 0, invf = 0, invl = 0, deml = 0;
};

/// BL-1227 review round 2: the scorer's extraction trace for the diagnosed goods.
struct trace_tally
{
    std::array<long, static_cast<std::size_t>(extraction_trace_outcome::count)> out{};
    long veto_rows = 0;            ///< veto_listed + veto_dead rows
    long veto_consumer_any = 0;    ///< ... with a processor consuming the good in that market (any state)
    long veto_consumer_running = 0;///< ... with one running (not idled, not under construction)
    long veto_consumer_starved = 0;///< ... with one standing idled (decommissioned)
    long veto_consumer_uc = 0;     ///< ... with one under construction
    long veto_body_bid = 0;        ///< ... and another market on the body bids for the good
    long veto_no_buyer = 0;        ///< ... no consumer processor in the market at all
    long emitted_built = 0;        ///< emitted rows whose (tile, good) got a new site that tick
};
const char* k_tout[] = {"placement", "net<=0", "materials", "veto: listed, no bid", "veto: dead market",
                        "veto: ratio", "emitted"};

struct ctx
{
    std::vector<extraction_trace_row> trace;
    std::map<std::size_t, trace_tally> tt;
    std::set<std::size_t> diag;
    const recipe_registry* reg = nullptr;
    std::set<std::size_t> watch;
    std::map<entity_id, mkt_snap> pre[resource_count]; // lap-0 snapshot per market, watched goods only
    std::set<entity_id> seen;
    std::set<entity_id> seen_proc;                ///< BL-1227 r4: processors seen
    std::map<std::string, long>* proc_new = nullptr; ///< new processors in play by recipe
    std::vector<tracked> sites;
    int tick = 0;
};

mkt_snap snap_of(const market_component& m, std::size_t r)
{
    mkt_snap s;
    s.price = m.price[r] > 0.0f ? m.price[r] : m.base_price[r];
    s.base = m.base_price[r]; s.demand = m.demand[r]; s.supply = m.supply[r];
    s.inv = m.inventory[r]; s.hh_bid = m.household_bid[r];
    return s;
}

void after_lap(const world& w, int lap, void* vp)
{
    ctx& c = *static_cast<ctx*>(vp);
    if (lap == 0)
    {
        for (const std::size_t r : c.watch)
        {
            c.pre[r].clear();
            for (const auto& [mid, m] : w.markets) c.pre[r][mid] = snap_of(m, r);
        }
        return;
    }
    if (lap != 1) return;
    // BL-1227 r4: new processing facilities in play, by recipe
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::processing_facility) continue;
        if (!c.seen_proc.insert(bid).second) continue;
        if (c.tick < 1 || c.proc_new == nullptr) continue;
        const recipe* rc = c.reg->get_recipe(b.recipe);
        ++(*c.proc_new)[rc ? rc->name : std::string("?")];
    }
    // new watched sites this tick
    std::vector<entity_id> fresh;
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::extraction_site) continue;
        if (!c.seen.insert(bid).second) continue;
        if (c.tick < 1) continue;
        if (!c.watch.count(static_cast<std::size_t>(b.target_resource))) continue;
        fresh.push_back(bid);
    }
    // ---- BL-1227 round 2: classify this tick's traced extraction candidates
    if (c.tick >= 1 && !c.trace.empty())
    {
        std::set<std::pair<entity_id, std::size_t>> built;
        for (const entity_id bid : fresh)
            built.insert({w.buildings.at(bid).tile, static_cast<std::size_t>(w.buildings.at(bid).target_resource)});
        // consumers per (market, good) by state
        std::map<std::pair<entity_id, std::size_t>, std::array<int, 3>> cons; // running, idled, uc
        for (const auto& [bid, b] : w.buildings)
        {
            if (b.type != building_type::processing_facility) continue;
            const recipe* rc = c.reg->get_recipe(b.recipe);
            if (!rc) continue;
            for (const std::size_t r : c.diag)
                if (rc->inputs[r] > 0.0f)
                {
                    auto& a = cons[{market_for_tile(w, b.tile), r}];
                    if (b.ticks_remaining > 0) ++a[2]; else if (b.decommissioned) ++a[1]; else ++a[0];
                }
        }
        for (const extraction_trace_row& row : c.trace)
        {
            const std::size_t r = static_cast<std::size_t>(row.target);
            if (!c.diag.count(r)) continue;
            trace_tally& t = c.tt[r];
            ++t.out[static_cast<std::size_t>(row.outcome)];
            if (row.outcome == extraction_trace_outcome::emitted && built.count({row.tile, r})) ++t.emitted_built;
            if (row.outcome == extraction_trace_outcome::veto_listed || row.outcome == extraction_trace_outcome::veto_dead)
            {
                ++t.veto_rows;
                const entity_id mid = market_for_tile(w, row.tile);
                const auto ci = cons.find({mid, r});
                const std::array<int, 3> a = ci != cons.end() ? ci->second : std::array<int, 3>{0, 0, 0};
                if (a[0] + a[1] + a[2] > 0) ++t.veto_consumer_any; else ++t.veto_no_buyer;
                if (a[0] > 0) ++t.veto_consumer_running;
                if (a[1] > 0) ++t.veto_consumer_starved;
                if (a[2] > 0) ++t.veto_consumer_uc;
                const auto mit = w.markets.find(mid);
                if (mit != w.markets.end())
                    for (const auto& [om, m2] : w.markets)
                        if (om != mid && m2.body == mit->second.body && m2.demand[r] > 0.0f) { ++t.veto_body_bid; break; }
            }
        }
    }
    c.trace.clear();
    if (fresh.empty()) return;
    // consumers per (market, good): non-idle processors whose recipe takes it
    std::map<std::pair<entity_id, std::size_t>, int> consumers;
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::processing_facility || b.decommissioned || b.ticks_remaining > 0) continue;
        const recipe* rc = c.reg->get_recipe(b.recipe);
        if (!rc) continue;
        for (const std::size_t r : c.watch)
            if (rc->inputs[r] > 0.0f) ++consumers[{market_for_tile(w, b.tile), r}];
    }
    std::map<std::pair<entity_id, std::size_t>, int> sib;
    std::vector<tracked> batch;
    const building_economics& ex = c.reg->economics(building_type::extraction_site);
    for (const entity_id bid : fresh)
    {
        const building_component& b = w.buildings.at(bid);
        tracked t;
        t.bid = bid; t.tile = b.tile; t.r = static_cast<std::size_t>(b.target_resource);
        t.market = market_for_tile(w, b.tile);
        t.placed = c.tick;
        t.rank = placement_rules::stack_rank(w, bid);
        if (const auto it = c.pre[t.r].find(t.market); it != c.pre[t.r].end()) t.at = it->second;
        const tile_component& tc = w.tiles.at(b.tile);
        const float rich = placement_rules::is_depositless_site(w, b.tile, b.target_resource)
            ? placement_rules::depositless_rate_scalar(b.target_resource)
            : richness_rate_scalar(ex, tc.resource_deposit[t.r]);
        const float rate = ex.base_rate * rich * 0.5f * (1.0f - tc.hazard_level)
                         * placement_rules::stack_output_scalar(t.rank);
        t.est_rev = rate * t.at.price;
        t.est_net = t.est_rev - ex.maintenance - ex.base_wage * 0.5f;
        if (t.at.demand > 0.0f)
        {
            const float horizon = ex.build_duration_ticks + 1.0f;
            t.glut_ratio = (t.at.supply + rate * horizon) / t.at.demand;
        }
        const auto ci = consumers.find({t.market, t.r});
        t.consumers = ci != consumers.end() ? ci->second : 0;
        ++sib[{t.market, t.r}];
        batch.push_back(t);
    }
    for (tracked& t : batch) { t.siblings = sib[{t.market, t.r}] - 1; c.sites.push_back(t); }
}

void after_tick(const world& w, const recipe_registry& reg, const settle_tick_result& res, ctx& c)
{
    for (tracked& t : c.sites)
    {
        if (t.op_ticks >= k_op_window) continue;
        const auto bi = w.buildings.find(t.bid);
        if (bi == w.buildings.end()) { t.op_ticks = k_op_window; t.idled = true; continue; }
        const building_component& b = bi->second;
        if (b.ticks_remaining > 0) continue;
        if (b.decommissioned) t.idled = true;
        const building_profit bp = estimate_building_profit(w, reg, res.report, t.bid);
        if (bp.has_data) { t.rev += bp.revenue; t.maint += bp.maintenance; t.wages += bp.wages; }
        if (const auto rit = res.report.building_row.find(t.bid);
            rit != res.report.building_row.end() && rit->second < res.report.buildings.size())
            t.out += res.report.buildings[rit->second].output_quantity;
        const auto mit = w.markets.find(t.market);
        if (mit != w.markets.end())
        {
            const mkt_snap s = snap_of(mit->second, t.r);
            if (t.op_ticks == 0) { t.price_first = s.price / std::max(1e-6f, s.base); t.inv_first = s.inv; }
            t.price_last = s.price / std::max(1e-6f, s.base); t.inv_last = s.inv; t.dem_last = s.demand;
        }
        ++t.op_ticks;
    }
}

void fold(const std::vector<tracked>& v, std::map<std::size_t, good_tally>& g)
{
    for (const tracked& t : v)
    {
        good_tally& a = g[t.r];
        ++a.n;
        if (t.at.demand <= 0.0f) ++a.no_demand;
        if (t.at.base > 0 && t.at.price == t.at.base) ++a.base_price;
        if (t.consumers > 0) ++a.with_consumer;
        a.p_ratio += t.at.base > 0 ? t.at.price / t.at.base : 0;
        if (t.glut_ratio >= 0) { a.glut += t.glut_ratio; ++a.glut_n; }
        a.est_rev += t.est_rev; a.est_net += t.est_net; a.siblings += t.siblings;
        a.demand += t.at.demand; a.supply += t.at.supply; a.hh += t.at.hh_bid;
        if (t.op_ticks >= k_op_window)
        {
            ++a.matured; if (t.idled) ++a.idled;
            a.rev += t.rev / k_op_window; a.maint += t.maint / k_op_window; a.wages += t.wages / k_op_window;
            a.out += t.out / k_op_window;
            a.pf += t.price_first; a.pl += t.price_last; a.invf += t.inv_first; a.invl += t.inv_last; a.deml += t.dem_last;
        }
    }
}

void print(const std::map<std::size_t, good_tally>& g)
{
    for (const auto& [r, a] : g)
    {
        if (!a.n) continue;
        const double n = double(a.n), m = a.matured ? double(a.matured) : 1.0;
        std::printf("  %-22s new %5ld | at placement: demand 0 %5.1f%%  price==base %5.1f%%  p/base %.2f  "
                    "demand %.1f supply %.1f hh_bid %.1f  consumer in mkt %5.1f%%  same-tick siblings %.2f\n",
                    gname(r).c_str(), a.n, 100.0 * a.no_demand / n, 100.0 * a.base_price / n, a.p_ratio / n,
                    a.demand / n, a.supply / n, a.hh / n, 100.0 * a.with_consumer / n, a.siblings / n);
        std::printf("  %-22s       | estimate: rev %.2f net %.2f  glut ratio %.2f (over %ld with demand)\n",
                    "", a.est_rev / n, a.est_net / n, a.glut_n ? a.glut / a.glut_n : 0.0, a.glut_n);
        std::printf("  %-22s       | first %d op ticks (%ld matured): out %.2f rev %.2f maint %.2f wages %.2f "
                    "net %.2f | p/base op1 %.2f op%d %.2f | shelf op1 %.0f op%d %.0f | demand op%d %.1f | idled %5.1f%%\n",
                    "", k_op_window, a.matured, a.out / m, a.rev / m, a.maint / m, a.wages / m,
                    (a.rev - a.maint - a.wages) / m, a.pf / m, k_op_window, a.pl / m, a.invf / m, k_op_window,
                    a.invl / m, k_op_window, a.deml / m, 100.0 * a.idled / m);
    }
}

} // namespace

int main(int argc, char** argv)
{
    int ticks = 400;
    std::vector<std::uint32_t> seeds = {0, 43, 10, 28, 38};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::max(1, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc)
        {
            seeds.clear();
            std::string s = argv[++i];
            std::size_t a = 0;
            while (a <= s.size())
            {
                const std::size_t b = s.find(',', a);
                const std::string tok = s.substr(a, b == std::string::npos ? std::string::npos : b - a);
                if (!tok.empty()) seeds.push_back(static_cast<std::uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
                if (b == std::string::npos) break;
                a = b + 1;
            }
        }
        else { std::fprintf(stderr, "usage: boom_probe [--seeds a,b] [--ticks N]\n"); return 2; }
    }
    std::map<std::size_t, good_tally> pooled;
    long lifts_all = 0, lifts_built_all = 0; // BL-1227 chain start, play only
    std::map<std::size_t, trace_tally> tt_all;
    std::map<std::string, long> proc_new_all;
    for (const std::uint32_t seed : seeds)
    {
        lua_state lua;
        world_params wp;
        wp.seed = seed;
        auto start = std::make_unique<app_start_world>();
        try { build_app_start_world(lua, wp, *start); }
        catch (const std::exception& e) { std::printf("seed %u: build threw %s\n", seed, e.what()); continue; }
        world& w = start->w;
        const recipe_registry& reg = start->reg;
        ctx c;
        c.reg = &reg;
        for (const resource_type rt : placement_rules::k_extractable) c.watch.insert(static_cast<std::size_t>(rt));
        for (const auto& [bid, b] : w.buildings) { c.seen.insert(bid); c.seen_proc.insert(bid); }
        c.proc_new = &proc_new_all;
        c.diag = {static_cast<std::size_t>(resource_type::iron_ore),
                  static_cast<std::size_t>(resource_type::petroleum),
                  static_cast<std::size_t>(resource_type::coal),
                  static_cast<std::size_t>(resource_type::copper_ore)};
        corp_extraction_trace_sink() = &c.trace;
        settle_tick_hooks hooks;
        hooks.after_lap = after_lap;
        hooks.ctx = &c;
        for (int step = 0; step < k_campaign_settle_ticks; ++step)
        {
            c.tick = step - k_campaign_settle_ticks;
            run_settle_tick(w, reg, step, 0, true, &hooks);
        }
        seat_player_corporation(w, seed, start->land.search.winner_score);
        constexpr int k_days = 90;
        long seed_lifts = 0, seed_built = 0;
        for (int k = 1; k <= ticks; ++k)
        {
            advance_orbits(w, static_cast<double>(k_days));
            advance_surveys(w, k_days);
            w.current_day_tick = k * k_days;
            c.tick = k;
            const settle_tick_result res =
                run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), k * k_days, false, &hooks);
            after_tick(w, reg, res, c);
            // BL-1227, the chain start: lifted vetoes this tick, and those whose
            // mine (same corp, tile, good) appeared this tick.
            for (const auto& l : res.report.chain_start_lifts)
            {
                ++lifts_all; ++seed_lifts;
                bool built = false;
                for (const tracked& t : c.sites)
                    if (t.placed == k && t.tile == l.tile && t.r == static_cast<std::size_t>(l.target))
                        if (const auto ci = w.corporations.find(l.corp); ci != w.corporations.end()
                            && std::find(ci->second.assets.begin(), ci->second.assets.end(), t.bid)
                               != ci->second.assets.end())
                            built = true;
                if (built) { ++lifts_built_all; ++seed_built; }
            }
        }
        corp_extraction_trace_sink() = nullptr;
        for (const auto& [r, t] : c.tt)
        {
            trace_tally& a = tt_all[r];
            for (std::size_t o = 0; o < t.out.size(); ++o) a.out[o] += t.out[o];
            a.veto_rows += t.veto_rows; a.veto_consumer_any += t.veto_consumer_any;
            a.veto_consumer_running += t.veto_consumer_running; a.veto_consumer_starved += t.veto_consumer_starved;
            a.veto_consumer_uc += t.veto_consumer_uc; a.veto_body_bid += t.veto_body_bid;
            a.veto_no_buyer += t.veto_no_buyer; a.emitted_built += t.emitted_built;
        }
        std::map<std::size_t, good_tally> g;
        fold(c.sites, g);
        std::printf("\nseed %u chain-start lifts in play: %ld, of them built that tick: %ld\n", seed, seed_lifts, seed_built);
        std::printf("\n=== seed %u: %zu new sites in play ===\n", seed, c.sites.size());
        print(g);
        fold(c.sites, pooled);
        std::fflush(stdout);
    }
    std::printf("\n================ POOLED ================\n");
    print(pooled);
    std::printf("\n  NEW processing facilities in play (all seeds), by recipe:");
    for (const auto& [name, n] : proc_new_all) std::printf(" %s %ld", name.c_str(), n);
    std::printf("\n");
    std::printf("\n================ SCORER EXTRACTION TRACE (play, candidate-evaluations) ================\n");
    for (const auto& [r, t] : tt_all)
    {
        std::printf("  %-14s", gname(r).c_str());
        for (std::size_t o = 0; o < t.out.size(); ++o) std::printf(" | %s %ld", k_tout[o], t.out[o]);
        std::printf("\n  %-14s   emitted and built that tick %ld | zero-bid vetoes %ld: no consumer processor in the market %ld, "
                    "a consumer there %ld (running %ld, idled %ld, under construction %ld); another market on the body bids %ld\n",
                    "", t.emitted_built, t.veto_rows, t.veto_no_buyer, t.veto_consumer_any, t.veto_consumer_running,
                    t.veto_consumer_starved, t.veto_consumer_uc, t.veto_body_bid);
    }
    std::printf("\n  chain-start lifts in play (all seeds): %ld; mines built from them that tick: %ld\n",
                lifts_all, lifts_built_all);
    return 0;
}
