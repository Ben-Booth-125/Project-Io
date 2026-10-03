// ---------------------------------------------------------------------------
// firm_attrition_trace — BL-1164 (background firm attrition), the diagnosis
// instrument
// ---------------------------------------------------------------------------
// On the shipped start (`build_app_start_world`), run the settle and the play
// ticks one at a time, and attribute every corporation that leaves the field
// and every building that leaves the map:
//
//   corp gone, in report.firm_exits      -> INSOLVENT  (BL-743 wind-up)
//   corp gone, not in firm_exits         -> ACQUIRED   (buy_corporation)
//   building gone, owner wound up        -> wound-up
//   building gone, owner acquired        -> (transferred, not lost)
//   building gone, owner still standing  -> demolished by its owner
//
// Per corp it keeps the lifetime sum of the seven money-loop flows (read from
// the quarterly return apply_budget just filed), its balance trajectory, the
// tick it first went negative and first crossed the exit floor, its holdings'
// goods, its buildings' distance (grid cells) to the centre of the market each
// clears at, and whether that market is one of the body's big ones (top 5 by
// priced demand at play start). Read-only on the world; prints only.
//
// Usage (repo root): firm_attrition_trace [--seeds a,b] [--ticks N] [--every K]
//                                         [--corps]   (one line per exited corp)
// Lua harness: bash tools/verify/build_lua_harness.sh firm_attrition_trace
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct corp_track
{
    bool  background = false, player = false;
    int   focus = 0;
    int   born = 0, died = -1;
    int   cause = 0; // 0 alive, 1 insolvent, 2 acquired
    float bal0 = 0, bal_min = 1e30f, bal_last = 0;
    int   neg_at = -1, floor_at = -1;
    int   holdings0 = -1, holdings_max = 0;
    double f[7] = {0, 0, 0, 0, 0, 0, 0}; // inc exp maint wage int levy upkeep
    int   n = 0;
    double dist = 0; int dn = 0; int big = 0, bign = 0;
    std::map<int, int> goods; // primary good -> building count
    int   active_rows = 0, idle_rows = 0;
    double offloop = 0;                   // balance moved outside the money loop (build/hire/buy/contract)
    float  bal_prev = 0; bool has_prev = false;
    std::map<int, double> sold, bought;   // resource -> credits, from the exchange ring
    std::map<int, double> sold_q, bought_q;
    std::map<entity_id, int> mkt;         // market -> building-rows
    int    unit_ticks = 0, builds = 0, builds_neg = 0; // builds started; ...while balance < 0
    std::set<entity_id> assets_prev;
};

const char* k_flow[7] = {"income", "expend", "maint", "wages", "interest", "levies", "upkeep"};

int good_of(const recipe_registry& reg, const building_report& b)
{
    if (b.type == building_type::extraction_site) return static_cast<int>(b.target_resource);
    if (const recipe* r = reg.get_recipe(b.recipe)) return static_cast<int>(primary_output_resource(*r));
    return -1;
}

std::string gname(int g)
{
    return g < 0 ? std::string("?") : resource_names::name_of(static_cast<resource_type>(g));
}

void summarise(const char* label, const std::vector<const corp_track*>& v, int ticks_ref)
{
    if (v.empty()) { std::printf("  %-22s n=0\n", label); return; }
    double f[7] = {0}, life = 0, h0 = 0, bal0 = 0, dist = 0, big = 0, bg = 0;
    int dn = 0, bn = 0;
    for (const corp_track* c : v)
    {
        const int n = std::max(1, c->n);
        for (int i = 0; i < 7; ++i) f[i] += c->f[i] / n;
        life += c->n; h0 += std::max(0, c->holdings0); bal0 += c->bal0; bg += c->background;
        if (c->dn) { dist += c->dist / c->dn; ++dn; }
        if (c->bign) { big += static_cast<double>(c->big) / c->bign; ++bn; }
    }
    const double k = static_cast<double>(v.size());
    std::printf("  %-22s n=%3zu bg %3.0f%% | life %5.1f t | holdings0 %4.1f | bal0 %7.0f | per tick:",
                label, v.size(), 100 * bg / k, life / k, h0 / k, bal0 / k);
    for (int i = 0; i < 7; ++i) std::printf(" %s %.1f", k_flow[i], f[i] / k);
    std::printf(" | dist %.1f | in-big-mkt %.0f%%\n", dn ? dist / dn : 0.0, bn ? 100 * big / bn : 0.0);
    (void)ticks_ref;
}

} // namespace

int main(int argc, char** argv)
{
    int ticks = 450, every = 25;
    bool per_corp = false;
    std::vector<uint32_t> seeds = {0};
    std::set<int> at_ticks; // --at a,b,c: extra rows at these (1-based, settle-inclusive) ticks
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--every") && i + 1 < argc) every = std::max(1, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--corps")) per_corp = true;
        else if (!std::strcmp(argv[i], "--at") && i + 1 < argc)
        {
            std::stringstream ss(argv[++i]);
            std::string t;
            while (std::getline(ss, t, ',')) if (!t.empty()) at_ticks.insert(std::atoi(t.c_str()));
        }
        else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc)
        {
            seeds.clear();
            std::stringstream ss(argv[++i]);
            std::string t;
            while (std::getline(ss, t, ',')) if (!t.empty()) seeds.push_back(std::strtoul(t.c_str(), nullptr, 10));
        }
    }
    std::printf("firm_attrition_trace — BL-1164\n");
    for (const uint32_t seed : seeds)
    {
        lua_state lua;
        world_params p;
        p.seed = seed;
        auto start = std::make_unique<app_start_world>();
        build_app_start_world(lua, p, *start);
        world& w = start->w;
        const recipe_registry& reg = start->reg;
        std::printf("seed %u  floor %.0f x %d quarters  corps %zu buildings %zu\n", seed,
                    reg.firm_exit().balance_floor, reg.firm_exit().consecutive_quarters,
                    w.corporations.size(), w.buildings.size());

        std::map<entity_id, corp_track> track;
        std::map<entity_id, entity_id> bowner; // building -> owner last tick
        std::set<entity_id> big_markets;
        auto snapshot_owners = [&]() {
            bowner.clear();
            std::vector<entity_id> ids;
            for (const auto& kv : w.corporations) ids.push_back(kv.first);
            std::sort(ids.begin(), ids.end());
            for (const entity_id c : ids)
                for (const entity_id b : w.corporations.at(c).assets) bowner[b] = c;
        };
        for (const auto& [cid, cc] : w.corporations)
        {
            corp_track& t = track[cid];
            t.background = cc.is_background; t.player = cc.is_player || cid == w.player_entity;
            t.focus = static_cast<int>(cc.focus); t.bal0 = cc.balance; t.born = 0;
        }
        snapshot_owners();

        std::printf("  (off-loop = balance moved outside apply_budget per tick: build presses, hires, buyouts, contracts; negative = spend)\n");
        std::printf("  tick | corps exit-ins exit-acq | bldg live built demol wound idle-flag | corps<0 <floor | field bal | sum flows/tick: inc exp maint wage int lev upk\n");
        int c_ins = 0, c_acq = 0, b_built = 0, b_demol = 0, b_wound = 0;
        std::size_t ex_total = w.exchanges.total; int ex_overflow = 0; double win_off = 0;
        std::map<int, double> win_sold, win_bought;
        double win_f[7] = {0};
        int win_dispatch = 0, tot_dispatch = 0;
        std::map<int, int> exit_hist_ins, exit_hist_acq;
        const int total = k_campaign_settle_ticks + ticks;
        for (int step = 0; step < total; ++step)
        {
            const bool settle = step < k_campaign_settle_ticks;
            const std::set<entity_id> before_b = [&] { std::set<entity_id> s; for (const auto& kv : w.buildings) s.insert(kv.first); return s; }();
            const settle_tick_result res = run_settle_tick(w, reg, step, settle ? 0 : step, settle);

            if (step == k_campaign_settle_ticks)
            {
                // Rank markets by priced demand, per body; top 5 per body are "big".
                std::map<entity_id, std::vector<std::pair<double, entity_id>>> per_body;
                for (const auto& [mid, mc] : w.markets)
                {
                    double v = 0;
                    for (std::size_t r = 0; r < resource_count; ++r) v += mc.demand[r] * mc.price[r];
                    per_body[mc.body].push_back({-v, mid});
                }
                for (auto& [body, vec] : per_body)
                {
                    std::sort(vec.begin(), vec.end());
                    for (std::size_t i = 0; i < vec.size() && i < 5; ++i) big_markets.insert(vec[i].second);
                    std::printf("  body %llu: %zu markets\n", static_cast<unsigned long long>(body), vec.size());
                }
            }

            {
                const std::size_t fresh = std::min<std::size_t>(w.exchanges.total - ex_total, w.exchanges.size());
                if (w.exchanges.total - ex_total > w.exchanges.size()) ++ex_overflow;
                for (std::size_t i = w.exchanges.size() - fresh; i < w.exchanges.size(); ++i)
                {
                    const exchange_record& e = w.exchanges.oldest_first(i);
                    const double v = static_cast<double>(e.quantity) * e.unit_price;
                    const int r = static_cast<int>(e.resource);
                    if (auto it = track.find(e.seller); it != track.end()) { it->second.sold[r] += v; it->second.sold_q[r] += e.quantity; }
                    if (auto it = track.find(e.buyer); it != track.end()) { it->second.bought[r] += v; it->second.bought_q[r] += e.quantity; }
                    if (e.seller != null_entity) win_sold[r] += v;
                    if (e.buyer != null_entity) win_bought[r] += v;
                }
                ex_total = w.exchanges.total;
            }
            std::set<entity_id> insolvent;
            for (const firm_exit_record& r : res.report.firm_exits) insolvent.insert(r.corp);

            // Corps: update survivors, classify the departed.
            int ins = 0, acq = 0;
            for (auto& [cid, t] : track)
            {
                if (t.died >= 0) continue;
                const auto it = w.corporations.find(cid);
                if (it == w.corporations.end())
                {
                    t.died = step; t.cause = insolvent.count(cid) ? 1 : 2;
                    (t.cause == 1 ? ins : acq)++;
                    (t.cause == 1 ? exit_hist_ins : exit_hist_acq)[step / 50 * 50]++;
                    continue;
                }
                const corporation_component& cc = it->second;
                if (!cc.returns.empty())
                {
                    const quarterly_return& q = cc.returns.back();
                    const float fl[7] = {q.income, q.expenditure, q.maintenance, q.wages, q.interest, q.levies, q.upkeep};
                    for (int i = 0; i < 7; ++i) { t.f[i] += fl[i]; win_f[i] += fl[i]; }
                    ++t.n;
                    if (t.has_prev) { t.offloop += (cc.balance - t.bal_prev) - q.net; win_off += (cc.balance - t.bal_prev) - q.net; }
                    else            { t.offloop += (cc.balance - t.bal0) - q.net; win_off += (cc.balance - t.bal0) - q.net; }
                    t.bal_prev = cc.balance; t.has_prev = true;
                }
                t.bal_last = cc.balance;
                t.bal_min = std::min(t.bal_min, cc.balance);
                if (t.neg_at < 0 && cc.balance < 0) t.neg_at = step;
                if (t.floor_at < 0 && cc.balance < reg.firm_exit().balance_floor) t.floor_at = step;
                const int h = static_cast<int>(cc.assets.size());
                for (const entity_id a : cc.assets)
                    if (!t.assets_prev.count(a) && step > 0) { ++t.builds; if (t.bal_prev < 0) ++t.builds_neg; }
                t.assets_prev = std::set<entity_id>(cc.assets.begin(), cc.assets.end());
                if (step == k_campaign_settle_ticks) t.holdings0 = h;
                t.holdings_max = std::max(t.holdings_max, h);
            }
            // Corps appearing (none expected) are tracked from now.
            for (const auto& [cid, cc] : w.corporations)
                if (!track.count(cid)) { corp_track& t = track[cid]; t.born = step; t.bal0 = cc.balance; t.background = cc.is_background; }
            c_ins += ins; c_acq += acq;
            win_dispatch += static_cast<int>(res.report.battle_dispatches.size());
            tot_dispatch += static_cast<int>(res.report.battle_dispatches.size());
            for (const auto& [uid, u] : w.units) { (void)uid; if (auto it = track.find(u.owner); it != track.end()) ++it->second.unit_ticks; }

            // Buildings: gone this tick, attributed by last tick's owner.
            for (const entity_id b : before_b)
            {
                if (w.buildings.count(b)) continue;
                const auto o = bowner.find(b);
                const entity_id own = o == bowner.end() ? null_entity : o->second;
                if (own != null_entity && insolvent.count(own)) ++b_wound;
                else if (own != null_entity && !w.corporations.count(own)) ++b_wound; // acquirer demolished? count as lost with owner
                else ++b_demol;
            }
            for (const auto& kv : w.buildings) if (!before_b.count(kv.first)) ++b_built;

            // Per-building facts for the owners' profiles (goods, distance, big market, activity).
            if (!settle)
            {
                for (const building_report& br : res.report.buildings)
                {
                    auto ti = track.find(br.corp);
                    if (ti == track.end()) continue;
                    corp_track& t = ti->second;
                    if (br.active) ++t.active_rows; else ++t.idle_rows;
                    if (step == k_campaign_settle_ticks || t.goods.empty()) t.goods[good_of(reg, br)]++;
                    const auto bi = w.buildings.find(br.building);
                    if (bi == w.buildings.end()) continue;
                    const entity_id mk = market_for_tile(w, bi->second.tile);
                    const auto mi = w.markets.find(mk);
                    const auto tt = w.tiles.find(bi->second.tile);
                    if (mi == w.markets.end() || tt == w.tiles.end()) continue;
                    const auto ct = w.tiles.find(mi->second.centre_tile);
                    if (ct != w.tiles.end())
                    {
                        const double dx = tt->second.grid_x - ct->second.grid_x, dy = tt->second.grid_y - ct->second.grid_y;
                        t.dist += std::sqrt(dx * dx + dy * dy); ++t.dn;
                    }
                    t.big += big_markets.count(mk) ? 1 : 0; ++t.bign;
                    t.mkt[mk]++;
                }
            }
            snapshot_owners();

            if ((step + 1) % every == 0 || step == k_campaign_settle_ticks - 1 || at_ticks.count(step + 1))
            {
                int live = 0, idlef = 0;
                for (const auto& kv : w.buildings)
                {
                    if (kv.second.decommissioned) ++idlef; else if (kv.second.ticks_remaining <= 0) ++live;
                }
                int neg = 0, flo = 0; double fb = 0;
                for (const auto& kv : w.corporations)
                {
                    fb += kv.second.balance;
                    if (kv.second.balance < 0) ++neg;
                    if (kv.second.balance < reg.firm_exit().balance_floor) ++flo;
                }
                const int span = (step == k_campaign_settle_ticks - 1) ? k_campaign_settle_ticks : every;
                std::printf("  %4d%s | %3zu %4d %4d | %4d %4d %4d %4d %4d | %3d %3d | %9.0f |",
                            step + 1, settle ? "s" : " ", w.corporations.size(), c_ins, c_acq,
                            live, b_built, b_demol, b_wound, idlef, neg, flo, fb);
                for (int i = 0; i < 7; ++i) { std::printf(" %.0f", win_f[i] / span); win_f[i] = 0; }
                std::printf(" off-loop %.0f", win_off / span); win_off = 0;
                {
                    // Battles in play and the standing force's supply (BL-1172 lane).
                    int cu = 0; long long sup = 0;
                    for (const auto& [uid, u] : w.units)
                    { (void)uid; if (w.corporations.count(u.owner)) { ++cu; sup += u.supply_factor_permille; } }
                    std::printf(" | battles live %zu dispatched %d (cum %d) | corp units %d supply %.0f",
                                w.battles.size(), win_dispatch, tot_dispatch, cu, cu ? static_cast<double>(sup) / cu : 0.0);
                    win_dispatch = 0;
                }
                auto top = [&](const char* l, std::map<int, double>& m) {
                    std::vector<std::pair<double, int>> v; for (auto& [r, x] : m) v.push_back({-x, r}); std::sort(v.begin(), v.end());
                    std::printf(" | %s", l); for (std::size_t i = 0; i < v.size() && i < 4; ++i) std::printf(" %s %.0f", gname(v[i].second).c_str(), -v[i].first / span);
                    m.clear(); };
                top("sold", win_sold); top("bought", win_bought);
                std::printf("\n");
                c_ins = c_acq = b_built = b_demol = b_wound = 0;
            }
        }

        // Summary.
        std::vector<const corp_track*> ins_v, acq_v, alive_v, alive_bg, alive_named;
        std::map<int, int> g_ins, g_alive, g_acq;
        for (const auto& [cid, t] : track)
        {
            if (t.cause == 1) { ins_v.push_back(&t); for (auto& [g, n] : t.goods) g_ins[g] += n; }
            else if (t.cause == 2) { acq_v.push_back(&t); for (auto& [g, n] : t.goods) g_acq[g] += n; }
            else { alive_v.push_back(&t); (t.background ? alive_bg : alive_named).push_back(&t); for (auto& [g, n] : t.goods) g_alive[g] += n; }
        }
        std::printf(" summary seed %u\n", seed);
        summarise("insolvent", ins_v, ticks);
        summarise("acquired", acq_v, ticks);
        summarise("alive", alive_v, ticks);
        summarise("alive background", alive_bg, ticks);
        summarise("alive non-background", alive_named, ticks);
        auto hist = [](const char* l, const std::map<int, int>& h) {
            std::printf("  %s by tick:", l); for (auto& [k, n] : h) std::printf(" %d:%d", k, n); std::printf("\n"); };
        hist("insolvent exits", exit_hist_ins);
        hist("acquired exits", exit_hist_acq);
        auto goods = [](const char* l, const std::map<int, int>& g) {
            std::vector<std::pair<int, int>> v; for (auto& [k, n] : g) v.push_back({-n, k});
            std::sort(v.begin(), v.end());
            std::printf("  %s goods:", l);
            for (std::size_t i = 0; i < v.size() && i < 10; ++i) std::printf(" %s %d", gname(v[i].second).c_str(), -v[i].first);
            std::printf("\n"); };
        goods("insolvent", g_ins); goods("acquired", g_acq); goods("alive", g_alive);
        // Time from first negative to exit, and to the floor.
        double neg2exit = 0, flo2exit = 0, bornneg = 0; int nn = 0, nf = 0;
        for (const corp_track* c : ins_v)
        {
            if (c->neg_at >= 0) { neg2exit += c->died - c->neg_at; ++nn; if (c->neg_at < k_campaign_settle_ticks) ++bornneg; }
            if (c->floor_at >= 0) { flo2exit += c->died - c->floor_at; ++nf; }
        }
        std::printf("  insolvent: first-negative -> exit %.1f t (negative during settle: %.0f of %zu); floor -> exit %.1f t\n",
                    nn ? neg2exit / nn : 0.0, bornneg, ins_v.size(), nf ? flo2exit / nf : 0.0);
        std::printf("  exchange ring overflow ticks: %d\n", ex_overflow);
        auto trade = [&](const char* l, const std::vector<const corp_track*>& v) {
            std::map<int, double> so, bo, sq, bq; double off = 0, life = 0;
            for (const corp_track* c : v) { for (auto& [r, x] : c->sold) so[r] += x; for (auto& [r, x] : c->bought) bo[r] += x;
                for (auto& [r, x] : c->sold_q) sq[r] += x; for (auto& [r, x] : c->bought_q) bq[r] += x; off += c->offloop; life += c->n; }
            double ut = 0, bl = 0, bn = 0, mil = 0;
            for (const corp_track* c : v) { ut += c->unit_ticks; bl += c->builds; bn += c->builds_neg;
                for (int r : {static_cast<int>(resource_type::ordnance), static_cast<int>(resource_type::food_rations)}) { auto it = c->bought.find(r); if (it != c->bought.end()) mil += it->second; } }
            std::printf("  %s: units/corp %.2f | builds/corp %.1f (%.1f while balance<0) | ordnance+rations bought per corp-tick %.1f\n", l, life ? ut / life : 0.0, bl / v.size(), bn / v.size(), life ? mil / life : 0.0);
            std::printf("  %s: off-loop per corp-tick %.1f | sold (cr/corp-tick @unit price):", l, life ? off / life : 0.0);
            std::vector<std::pair<double, int>> vs; for (auto& [r, x] : so) vs.push_back({-x, r}); std::sort(vs.begin(), vs.end());
            for (std::size_t i = 0; i < vs.size() && i < 7; ++i) std::printf(" %s %.1f@%.2f", gname(vs[i].second).c_str(), -vs[i].first / life, sq[vs[i].second] > 0 ? -vs[i].first / sq[vs[i].second] : 0.0);
            std::printf("\n      bought:");
            std::vector<std::pair<double, int>> vb; for (auto& [r, x] : bo) vb.push_back({-x, r}); std::sort(vb.begin(), vb.end());
            for (std::size_t i = 0; i < vb.size() && i < 7; ++i) std::printf(" %s %.1f@%.2f", gname(vb[i].second).c_str(), -vb[i].first / life, bq[vb[i].second] > 0 ? -vb[i].first / bq[vb[i].second] : 0.0);
            std::printf("\n"); };
        trade("insolvent", ins_v); trade("alive", alive_v);
        {
            // Per market: corps whose dominant market it is, by fate, and its size rank.
            std::map<entity_id, std::array<int, 3>> by_m;
            for (const auto& [cid, t] : track)
            {
                if (t.mkt.empty()) continue;
                entity_id best = null_entity; int bn = -1;
                for (auto& [m, n] : t.mkt) if (n > bn) { bn = n; best = m; }
                by_m[best][t.cause == 1 ? 1 : (t.cause == 2 ? 2 : 0)]++;
            }
            std::printf("  markets (dominant market of each corp): id big alive/insolvent:");
            for (auto& [m, a] : by_m) std::printf(" %llu%s %d/%d", static_cast<unsigned long long>(m), big_markets.count(m) ? "*" : "", a[0], a[1]);
            std::printf("\n");
        }
        if (per_corp)
        {
            for (const auto& [cid, t] : track)
            {
                const int n = std::max(1, t.n);
                std::printf("  corp %llu %s%s cause %d died %d bal0 %.0f min %.0f last %.0f neg@%d floor@%d h0 %d hmax %d act/idle %d/%d dist %.1f big %.0f%% |",
                            static_cast<unsigned long long>(cid), t.player ? "P" : "", t.background ? "bg" : "rv", t.cause, t.died,
                            t.bal0, t.bal_min, t.bal_last, t.neg_at, t.floor_at, t.holdings0, t.holdings_max, t.active_rows, t.idle_rows,
                            t.dn ? t.dist / t.dn : 0.0, t.bign ? 100.0 * t.big / t.bign : 0.0);
                for (int i = 0; i < 7; ++i) std::printf(" %.1f", t.f[i] / n);
                std::printf(" off %.1f |", t.offloop / n);
                for (auto& [g, k] : t.goods) std::printf(" %s:%d", gname(g).c_str(), k);
                std::printf(" | units %.2f |", static_cast<double>(t.unit_ticks) / n);
                auto top3 = [&](const char* l, const std::map<int, double>& m) {
                    std::vector<std::pair<double, int>> v; for (auto& [r, x] : m) v.push_back({-x, r}); std::sort(v.begin(), v.end());
                    std::printf(" %s", l); for (std::size_t i = 0; i < v.size() && i < 3; ++i) std::printf(" %s=%.0f", gname(v[i].second).c_str(), -v[i].first / n); };
                top3("sold", t.sold); top3("bought", t.bought);
                std::printf("\n");
            }
        }
        std::fflush(stdout);
    }
    return 0;
}
