// ---------------------------------------------------------------------------
// ceiling_trace — BL-1217 (inputs reach processors), wave 1 diagnosis probe
// ---------------------------------------------------------------------------
// QUESTION. market_viability's logistics row (BL-1223) reads 51% of
// input-starved processors beside a STOCKED shelf of their scarcest input that
// is priced OVER the fair-price ceiling (reservation_mult x base). BL-1209's
// rule (the shelf's share of supply is min(inventory, k x (demand + the want
// the ceiling silenced))) was meant to let such a price fall. Why does it not?
//
// THE WORLD. Seated exactly as market_viability seats it: build_app_start_world,
// the 12-tick settle (run_settle_tick, econ steps 0..11, day 0, spectating),
// seat_player_corporation, then PLAY ticks as run_app_live_window steps them.
//
// A PURE READER. Two const after-lap reads per play tick and the tick's report:
//   lap 0 (convoys)          : the PRE-STEP shelf and the posted price (the
//                              price every draw this tick checks the ceiling at).
//   lap 1 (run_economy_step) : the shelf as clear_markets' reference-price pass
//                              reads it (nothing between the start of
//                              clear_markets and the reference prices moves
//                              inventory - market_clearing.cpp ~1620).
//   lap 2 (clear_markets)    : the tick's demand, listings, hauler_want and the
//                              price the clear left posted.
// The probe RECOMPUTES the price law (pricing_supply + price_target + the 0.5
// EMA) from those reads and compares with the posted result; a mismatch means
// the clear took its VWAP branch (a matched book trade) instead.
//
// SELECTION. At play ticks 10/25/50: every processor whose report row is
// starved (idle, or run < 1) with limiting input g, at a market whose PRE-STEP
// shelf of g is >= 1 u and whose posted price of g is over the ceiling. Each
// distinct (market, good) is followed from 3 ticks before to 20 after.
//
// Usage (repo root): build_gen/verify/ceiling_trace.exe [--seeds a,b] [--ticks N] [--traces N]
// Build:  ./tools/verify/build_lua_harness.sh ceiling_trace
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/resource_names.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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

constexpr int k_sel_ticks[] = {10, 25, 50};
constexpr int k_before = 3, k_after = 20;

/// One (market, good) at one play tick.
struct rec
{
    float base = 0, prior = 0, inv_pre = 0, inv_price = 0;
    float demand = 0, listed = 0, hw = 0, posted = 0;
    float admitted_want = 0; ///< report.wants at this market (processor/construction/upkeep draws under the ceiling)
    float fill = 0;          ///< report.purchases at this market (what draws took off the shelf)
    float buy_orders = 0;    ///< standing buy-order qty on the body
    int   sell_orders = 0;   ///< standing sell orders on the body
};

using tick_map = std::map<std::pair<entity_id, std::size_t>, rec>;

struct probe
{
    int lap_conv = -1, lap_econ = -1, lap_clear = -1;
    tick_map* cur = nullptr;
};

void after_lap(const world& w, int lap, void* ctx)
{
    auto* p = static_cast<probe*>(ctx);
    tick_map& t = *p->cur;
    for (const auto& [mid, mc] : w.markets)
        for (std::size_t g = 0; g < resource_count; ++g)
        {
            if (!(mc.base_price[g] > 0.0f)) continue;
            rec& r = t[std::make_pair(mid, g)];
            if (lap == p->lap_conv)
            {
                r.base = mc.base_price[g];
                r.prior = posted_price(mc, g);
                r.inv_pre = mc.inventory[g];
            }
            else if (lap == p->lap_econ)
                r.inv_price = mc.inventory[g];
            else if (lap == p->lap_clear)
            {
                r.demand = mc.demand[g];
                r.listed = mc.supply[g];
                r.hw = mc.hauler_want[g];
                r.posted = mc.price[g];
            }
        }
}

struct law
{
    float share = 0, supply = 0, target = 0, ref = 0;
    bool  inv_bound = false; ///< the shelf share is the whole inventory (not k x wants)
    bool  vwap = false;      ///< the posted price is not the reference price
    int   branch = 0;        ///< 0 ratio, 1 no-supply cap, 2 no signal
};

law apply_law(const rec& r, const price_band_params& pb)
{
    law L;
    const float k = pb.shelf_supply_ticks;
    const float shelf = std::max(0.0f, r.inv_price);
    const float wants = std::max(0.0f, r.demand) + std::max(0.0f, r.hw);
    const float sells = k * wants;
    L.share = k > 0.0f ? std::min(shelf, sells) : 0.0f;
    L.inv_bound = k > 0.0f && shelf <= sells;
    L.supply = std::max(0.0f, r.listed) + L.share;
    L.branch = (L.supply <= 0.0f && r.demand <= 0.0f) ? 2 : (L.supply <= 0.0f ? 1 : 0);
    L.target = price_target(r.base, L.supply, r.demand, pb.floor_mult, pb.ceil_mult);
    L.ref = std::clamp(r.prior + k_price_smoothing * (L.target - r.prior), r.base * pb.floor_mult,
                       r.base * pb.ceil_mult);
    L.vwap = std::fabs(L.ref - r.posted) > 1e-3f * r.base;
    return L;
}

/// Why the price the clear left stands over the ceiling.
enum reason { rs_vwap, rs_nosupply, rs_demand_over_shelf, rs_smoothing, rs_count };
const char* k_reason[rs_count] = {"vwap(book trade)", "no supply (cap)", "D>=4S (shelf-bound)",
                                  "EMA descent (target<ceil)"};

reason why_over(const rec& r, const law& L, float res_mult)
{
    if (L.vwap) return rs_vwap;
    if (L.branch == 1) return rs_nosupply;
    if (L.target > res_mult * r.base) return rs_demand_over_shelf;
    return rs_smoothing;
}

std::string gname(std::size_t g) { return resource_names::name_of(static_cast<resource_type>(g)); }

struct pool_tally
{
    int pairs = 0, fell = 0, never = 0, drew_after_fall = 0, respiked = 0;
    int ticks_to_fall_sum = 0;
    std::array<long, rs_count> over_ticks{};
    long over_total = 0, over_inv_bound = 0, over_hw_pos = 0;
    double d_admit = 0, d_orders = 0, d_inject = 0, d_total = 0, shelf_sum = 0, hw_sum = 0;
    std::map<std::size_t, int> goods;
    // per-pair class over the window: PHANTOM = injected demand (population,
    // background, endemic, interbody - bids that never draw) outweighs the
    // admitted draw want; PULSE = the market's own draws dominate.
    int   cls_pairs[2] = {0, 0}, cls_fell[2] = {0, 0}, cls_over_ticks[2] = {0, 0}, cls_ticks[2] = {0, 0};
    std::map<std::size_t, int> cls_goods[2];
    // draw ticks in the window (admitted want > 0): want vs what the shelf gave
    long draw_ticks = 0, draw_ticks_emptied = 0;
    double draw_want = 0, draw_fill = 0;
    // over-ceiling ticks split: the tick's own draws emptied the shelf (admitted > 0)
    // vs silenced (admitted == 0)
    long over_drawn = 0, over_silenced = 0;
};

void run_seed(std::uint32_t seed, int ticks, int max_traces, pool_tally& T)
{
    lua_state lua;
    world_params wp;
    wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    try { build_app_start_world(lua, wp, *start); }
    catch (const std::exception& e) { std::printf("seed %u: build threw %s\n", seed, e.what()); return; }
    world& w = start->w;
    const recipe_registry& reg = start->reg;
    const price_band_params& pb = reg.price_band();
    const float res_mult = pb.reservation_mult;
    std::printf("\n=== seed %u  k=%.2f reservation %.1fx cap %.1fx floor %.2fx ===\n", seed,
                pb.shelf_supply_ticks, res_mult, pb.ceil_mult, pb.floor_mult);

    for (int step = 0; step < k_campaign_settle_ticks; ++step)
        run_settle_tick(w, reg, step, 0, true);
    seat_player_corporation(w, seed, start->land.search.winner_score);

    probe p;
    for (int i = 0; i < k_campaign_settle_lap_count; ++i)
    {
        if (!std::strcmp(k_campaign_settle_lap_names[i], "convoys")) p.lap_conv = i;
        if (!std::strcmp(k_campaign_settle_lap_names[i], "run_economy_step")) p.lap_econ = i;
        if (!std::strcmp(k_campaign_settle_lap_names[i], "clear_markets")) p.lap_clear = i;
    }
    std::vector<tick_map> hist(static_cast<std::size_t>(ticks) + 1);
    std::map<int, std::set<std::pair<entity_id, std::size_t>>> selected;
    std::map<std::pair<entity_id, std::size_t>, int> sel_procs; // count of starved processors

    constexpr int k_days = 90;
    for (int k = 1; k <= ticks; ++k)
    {
        advance_orbits(w, static_cast<double>(k_days));
        advance_surveys(w, k_days);
        w.current_day_tick = k * k_days;
        p.cur = &hist[k];
        settle_tick_hooks hooks;
        hooks.after_lap = after_lap;
        hooks.ctx = &p;
        settle_tick_result res = run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), k * k_days, false, &hooks);
        tick_map& t = hist[k];
        for (const auto& [key, a] : res.report.wants)
            for (std::size_t g = 0; g < resource_count; ++g)
                if (a[g] > 0.0f) { auto it = t.find({key.second, g}); if (it != t.end()) it->second.admitted_want += a[g]; }
        for (const auto& [key, a] : res.report.purchases)
            for (std::size_t g = 0; g < resource_count; ++g)
                if (a[g] > 0.0f) { auto it = t.find({key.second, g}); if (it != t.end()) it->second.fill += a[g]; }
        // BL-1265: the order book retired — the buy/sell order columns read 0.

        if (std::find(std::begin(k_sel_ticks), std::end(k_sel_ticks), k) == std::end(k_sel_ticks)) continue;
        for (const building_report& br : res.report.buildings)
        {
            if (br.type != building_type::processing_facility || !br.has_limiting) continue;
            if (!(br.idle || br.run < 1.0f)) continue;
            const auto bi = w.buildings.find(br.building);
            if (bi == w.buildings.end() || bi->second.decommissioned || bi->second.ticks_remaining > 0) continue;
            const entity_id m = market_for_tile(w, bi->second.tile);
            const std::size_t g = static_cast<std::size_t>(br.limiting_input);
            const auto it = t.find({m, g});
            if (it == t.end()) continue;
            const rec& r = it->second;
            if (r.inv_pre >= 1.0f && r.prior > res_mult * r.base)
            {
                selected[k].insert({m, g});
                ++sel_procs[{m, g}];
            }
        }
    }

    int traces = 0;
    for (const auto& [k, pairs] : selected)
    {
        std::printf("  tick %d: %zu stocked-over-ceiling (market, good) pairs under starved processors\n", k, pairs.size());
        for (const auto& key : pairs)
        {
            ++T.pairs;
            ++T.goods[key.second];
            const int lo = std::max(1, k - k_before), hi = std::min(ticks, k + k_after);
            const bool print = traces < max_traces;
            if (print)
            {
                ++traces;
                std::printf("   TRACE seed %u mkt %u %s (sel tick %d, %d starved procs)\n", seed,
                            static_cast<unsigned>(key.first), gname(key.second).c_str(), k, sel_procs[key]);
                std::printf("    tk  prior/b post/b  inv_pre inv_px   listed  demand(adm/ord/inj)        hw   share  supply  target/b ref/b  branch fill\n");
            }
            int fell_at = -1;
            bool drew = false, respike = false;
            double w_inj = 0, w_adm = 0;
            int w_over = 0, w_n = 0;
            for (int t = lo; t <= hi; ++t)
            {
                const auto it = hist[t].find(key);
                if (it == hist[t].end()) continue;
                const rec& r = it->second;
                const law L = apply_law(r, pb);
                const bool over = r.posted > res_mult * r.base;
                if (t >= k)
                {
                    ++w_n;
                    w_inj += std::max(0.0f, r.demand - r.admitted_want - r.buy_orders);
                    w_adm += r.admitted_want + r.hw; // the market's own processors, heard or silenced
                    if (over) ++w_over;
                    if (r.admitted_want > 0.0f)
                    {
                        ++T.draw_ticks; T.draw_want += r.admitted_want; T.draw_fill += r.fill;
                        if (r.inv_price < 1.0f) ++T.draw_ticks_emptied;
                    }
                    if (over) { if (r.admitted_want > 0.0f) ++T.over_drawn; else ++T.over_silenced; }
                    if (over)
                    {
                        const reason why = why_over(r, L, res_mult);
                        ++T.over_ticks[why];
                        ++T.over_total;
                        if (L.inv_bound) ++T.over_inv_bound;
                        if (r.hw > 0.0f) ++T.over_hw_pos;
                        const float inj = std::max(0.0f, r.demand - r.admitted_want - r.buy_orders);
                        T.d_admit += r.admitted_want; T.d_orders += r.buy_orders; T.d_inject += inj;
                        T.d_total += r.demand; T.shelf_sum += r.inv_price; T.hw_sum += r.hw;
                        if (fell_at >= 0) respike = true;
                    }
                    else if (fell_at < 0) fell_at = t;
                    if (fell_at >= 0 && t > fell_at && r.fill > 0.0f) drew = true;
                }
                if (print)
                {
                    const float inj = std::max(0.0f, r.demand - r.admitted_want - r.buy_orders);
                    const char* br = L.vwap ? "VWAP" : (L.branch == 1 ? "cap" : (L.branch == 2 ? "nosig" : (L.inv_bound ? "ratio/inv" : "ratio/kW")));
                    std::printf("    %3d %6.2f %6.2f %8.1f %6.1f %8.1f %7.1f(%5.1f/%5.1f/%6.1f) %7.1f %7.1f %7.1f %7.2f %6.2f %-9s %5.1f%s\n",
                                t, r.prior / r.base, r.posted / r.base, r.inv_pre, r.inv_price, r.listed,
                                r.demand, r.admitted_want, r.buy_orders, inj, r.hw, L.share, L.supply,
                                L.target / r.base, L.ref / r.base, br, r.fill, t == k ? "  <- sel" : "");
                }
            }
            const int c = w_inj > w_adm ? 1 : 0;
            ++T.cls_pairs[c]; T.cls_over_ticks[c] += w_over; T.cls_ticks[c] += w_n; ++T.cls_goods[c][key.second];
            if (fell_at >= 0) ++T.cls_fell[c];
            if (fell_at >= 0) { ++T.fell; T.ticks_to_fall_sum += fell_at - k; if (drew) ++T.drew_after_fall; if (respike) ++T.respiked; }
            else ++T.never;
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    int ticks = 60, traces = 4;
    std::vector<std::uint32_t> seeds = {0, 43, 10};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::max(51, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--traces") && i + 1 < argc) traces = std::atoi(argv[++i]);
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
        else { std::fprintf(stderr, "usage: ceiling_trace [--seeds a,b] [--ticks N] [--traces N]\n"); return 2; }
    }
    std::printf("ceiling_trace - BL-1217 wave 1: why a stocked industrial shelf stays over the ceiling\n");
    pool_tally T;
    for (const std::uint32_t s : seeds) { run_seed(s, ticks, traces, T); std::fflush(stdout); }

    std::printf("\nPOOLED over %zu seeds: %d (market, good) pairs selected at ticks 10/25/50\n", seeds.size(), T.pairs);
    std::printf("  fell under the ceiling within %d ticks: %d (mean %.1f ticks; %d drew after; %d spiked back over)\n",
                k_after, T.fell, T.fell ? double(T.ticks_to_fall_sum) / T.fell : 0.0, T.drew_after_fall, T.respiked);
    std::printf("  never fell in the window: %d\n", T.never);
    const double n = T.over_total ? double(T.over_total) : 1.0;
    std::printf("  over-ceiling pair-ticks %ld, by reason:", T.over_total);
    for (int i = 0; i < rs_count; ++i) std::printf("  %s %ld (%.0f%%)", k_reason[i], T.over_ticks[i], 100.0 * T.over_ticks[i] / n);
    std::printf("\n  of those: share inventory-bound %.0f%%, hauler_want > 0 %.0f%%\n", 100.0 * T.over_inv_bound / n, 100.0 * T.over_hw_pos / n);
    std::printf("  mean per over-tick: demand %.1f = admitted %.1f + buy orders %.1f + injected %.1f; shelf at pricing %.1f; hauler_want %.1f\n",
                T.d_total / n, T.d_admit / n, T.d_orders / n, T.d_inject / n, T.shelf_sum / n, T.hw_sum / n);
    std::printf("  over-ceiling ticks: own draws registered that tick (shelf drawn) %ld, silenced (no admitted draw) %ld\n",
                T.over_drawn, T.over_silenced);
    std::printf("  draw ticks %ld: admitted want %.1f vs filled %.1f per tick (%.0f%% met); shelf left < 1 u at pricing %.0f%%\n",
                T.draw_ticks, T.draw_ticks ? T.draw_want / T.draw_ticks : 0.0, T.draw_ticks ? T.draw_fill / T.draw_ticks : 0.0,
                T.draw_want > 0 ? 100.0 * T.draw_fill / T.draw_want : 0.0,
                T.draw_ticks ? 100.0 * T.draw_ticks_emptied / T.draw_ticks : 0.0);
    const char* cn[2] = {"PULSE (own processors' want dominates)", "PHANTOM (non-drawing injected demand dominates)"};
    for (int c = 0; c < 2; ++c)
    {
        std::printf("  %s: %d pairs, %d fell under within %d, over the ceiling %.0f%% of window ticks; goods:", cn[c],
                    T.cls_pairs[c], T.cls_fell[c], k_after, T.cls_ticks[c] ? 100.0 * T.cls_over_ticks[c] / T.cls_ticks[c] : 0.0);
        for (const auto& [g, n] : T.cls_goods[c]) std::printf(" %s %d", gname(g).c_str(), n);
        std::printf("\n");
    }
    std::printf("  goods:");
    for (const auto& [g, c] : T.goods) std::printf(" %s %d", gname(g).c_str(), c);
    std::printf("\n");
    return 0;
}
