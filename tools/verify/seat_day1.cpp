// ---------------------------------------------------------------------------
// seat_day1 — BL-1206 (seat day-1 profit), the diagnosis probe
// ---------------------------------------------------------------------------
// Does the seated corporation operate at a profit from day 1, and where it does
// not, WHY? A pure reader over the shipped start, stepped exactly as
// market_viability steps it (that file's header owns the tick-state argument):
// `build_app_start_world`, the 12-tick settle in spectate, the seat
// (`seat_player_corporation`, the headless draw — the path with no player to
// ask), then PLAY ticks with the seat held by the player (not auto-acted on).
//
// PER SEED it prints:
//   * the seat: provisional (generation) pick, drawn seat, its rank among the
//     ranked specialists, shortlist size, floor_unmet; whether the drawn seat is
//     the first-ranked (what a player taking the top card would hold).
//   * opening cash and stock: balance and summed pool units at generation and at
//     the handoff, the pool's largest goods at the handoff.
//   * the seat's filed flows, one row per quarter: the last 8 settle quarters
//     and the first N play quarters (default 8): income, expenditure (inputs
//     bought), maintenance, wages, levies, upkeep, OPERATING net (interest
//     excluded, spawn_solvency's definition), interest, balance.
//   * per holding: type, target / recipe, market, a per-tick state string over
//     the settle window and the play window, output and its value at the posted
//     price, and for every input of a processor: its reach tier from the
//     processor's market at the handoff (own / market / reach / NONE — Pass 3's
//     tiers, `market_within_reach`), and, on input-starved play ticks, the
//     input's posted price against the fair-price ceiling (reservation_mult x
//     base), the corp's pool at the market and the shelf.
//   State letters: R ran, i idle input-starved, n no labour, u unsupplied (zero
//   workforce target or supply scalar), d decommissioned, b under construction,
//   x extraction exhausted, o other idle, . no report row.
//
// A PURE READER: the reach read warms the logistics caches, so it is taken at
// the handoff and the caches are invalidated before the first play tick (the
// audit_chain_feasibility discipline).
//
// Usage (repo root): build_gen/verify/seat_day1.exe [--seeds a,b] [--play N] [--drop-builds]
//   --drop-builds  COUNTERFACTUAL: remove the seat's under-construction projects at
//                  the handoff, to attribute the inherited construction backlog.
// Default seeds: docs/generation/seed_library.json, in library order.
// Build: bash tools/verify/build_lua_harness.sh seat_day1
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/input_reach.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/resource_names.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int k_settle_window = 8;
bool g_drop_builds = false; ///< --drop-builds: the counterfactual, see run_seed

std::vector<std::uint32_t> library_seeds(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    std::vector<std::uint32_t> out;
    std::size_t pos = text.find("\"seeds\"");
    if (pos == std::string::npos) return out;
    const std::string key = "\"seed\"";
    while ((pos = text.find(key, pos)) != std::string::npos)
    {
        pos += key.size();
        std::size_t p = pos;
        while (p < text.size() && (text[p] == ' ' || text[p] == ':' || text[p] == '\t')) ++p;
        if (p < text.size() && text[p] >= '0' && text[p] <= '9')
            out.push_back(static_cast<std::uint32_t>(std::strtoul(text.c_str() + p, nullptr, 10)));
    }
    return out;
}

std::vector<std::uint32_t> parse_seed_list(const std::string& s)
{
    std::vector<std::uint32_t> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ','))
        if (!tok.empty()) out.push_back(static_cast<std::uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
    return out;
}

std::string rname(std::size_t r) { return resource_names::name_of(static_cast<resource_type>(r)); }

double opnet(const quarterly_return& q)
{
    return static_cast<double>(q.income) - q.expenditure - q.maintenance - q.wages - q.levies - q.upkeep;
}

const building_report* row_of(const economy_report& rep, entity_id bid)
{
    const auto it = rep.building_row.find(bid);
    return it == rep.building_row.end() ? nullptr : &rep.buildings[it->second];
}

char state_of(const recipe_registry& reg, const building_component& b, const building_report* br)
{
    if (b.ticks_remaining > 0) return 'b';
    if (b.decommissioned)      return 'd';
    if (br == nullptr)         return '.';
    if (br->active)            return 'R';
    if (br->exhausted)         return 'x';
    if (br->effective_workforce <= 0.0f) return 'n';
    if (building_supply_scalar(b) <= 0.0f || b.workforce_target <= 0.0f) return 'u';
    if (b.type == building_type::processing_facility && br->has_limiting) return 'i';
    (void)reg;
    return 'o';
}

struct starve_note { int n = 0; std::size_t input = 0; float price = 0, base = 0, ceil = 0, pool = 0, shelf = 0, need = 0; };

/// The shelf and the seat's pools as the economy step found them (after the
/// convoy lap, before production draws) — a READ hook.
struct pre_draw_snap
{
    entity_id corp = null_entity;
    std::map<entity_id, std::array<float, resource_count>> shelf, price, pool;
};
void snap_pre_draw(const world& w, int lap, void* ctx)
{
    if (lap != 0) return;
    auto* s = static_cast<pre_draw_snap*>(ctx);
    s->shelf.clear(); s->price.clear(); s->pool.clear();
    for (const auto& [mid, mc] : w.markets)
    {
        s->shelf[mid] = mc.inventory;
        std::array<float, resource_count> pr{};
        for (std::size_t r = 0; r < resource_count; ++r) pr[r] = posted_price(mc, r);
        s->price[mid] = pr;
    }
    // BL-1265: corporations hold no pools; `pool` stays empty (reads 0).
}

struct holding
{
    entity_id id = null_entity;
    building_type type = building_type::none;
    std::string what;
    entity_id market = null_entity;
    std::string settle_states, play_states;
    double play_out = 0, play_value = 0;
    std::map<std::size_t, starve_note> starve; // by limiting input
    std::vector<std::pair<std::size_t, int>> input_tier; // input -> tier at handoff
    float wf_target = 0;
};

const char* tier_name(int t)
{
    switch (t) { case 0: return "own"; case 1: return "market"; case 2: return "reach"; default: return "NONE"; }
}

/// Pass 3's tier of input r at a consumer in market `mkt`, over the buildings
/// standing now (corporation_generation.cpp chain_input_tier, restated read-only).
int input_tier(world& w, const recipe_registry& reg, input_reach& ir, entity_id self, entity_id mkt,
               std::size_t r, entity_id corp)
{
    if (mkt == null_entity) return 3;
    int best = 3;
    std::vector<entity_id> far;
    const corporation_component* cc = nullptr;
    if (auto it = w.corporations.find(corp); it != w.corporations.end()) cc = &it->second;
    for (const auto& [bid, b] : w.buildings)
    {
        if (bid == self || !building_produces(w, reg, b, r)) continue;
        const entity_id mp = market_for_tile(w, b.tile);
        if (mp == null_entity) continue;
        if (mp == mkt)
        {
            const bool mine = cc && std::find(cc->assets.begin(), cc->assets.end(), bid) != cc->assets.end();
            best = std::min(best, mine ? 0 : 1);
        }
        else far.push_back(mp);
    }
    if (best != 3) return best;
    std::sort(far.begin(), far.end());
    far.erase(std::unique(far.begin(), far.end()), far.end());
    for (const entity_id mp : far)
        if (market_within_reach(w, reg, ir, mp, mkt, r)) return 2;
    return 3;
}

/// BL-1265: corporations hold no pools — what the corp "holds" is read as the
/// stock on the shelves of the markets it sits in (`corp_shelf_stock`).
double pool_units(const world& w, entity_id corp)
{
    double s = 0;
    for (std::size_t r = 0; r < resource_count; ++r)
        s += corp_shelf_stock(w, corp, r);
    return s;
}

void run_seed(std::uint32_t seed, int play)
{
    lua_state lua;
    world_params p;
    p.seed = seed;
    auto start = std::make_unique<app_start_world>();
    try { build_app_start_world(lua, p, *start); }
    catch (const std::exception& e) { std::printf("seed %u: build threw: %s\n", seed, e.what()); return; }
    world& w = start->w;
    const recipe_registry& reg = start->reg;
    const entity_id provisional = w.player_entity;

    // Every specialist processor's recipe tiers AS BUILT (before the settle):
    // what the chain-feasible choice could see, re-asked now.
    std::map<entity_id, std::string> gen_tiers;
    {
        input_reach ir = make_input_reach(w, reg);
        std::vector<entity_id> cids;
        for (const auto& [cid, cc] : w.corporations) if (!cc.is_background) cids.push_back(cid);
        std::sort(cids.begin(), cids.end());
        for (const entity_id cid : cids)
            for (const entity_id a : w.corporations.at(cid).assets)
            {
                const auto bi = w.buildings.find(a);
                if (bi == w.buildings.end() || bi->second.type != building_type::processing_facility) continue;
                const entity_id mkt = market_for_tile(w, bi->second.tile);
                const recipe* cur = reg.get_recipe(bi->second.recipe);
                std::string line = std::string(" (recipe as built: ") + (cur ? cur->name : "none") + ")";
                const int n = reg.recipe_count(building_type::processing_facility);
                for (int i = 0; i < n; ++i)
                {
                    const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
                    int worst = 0;
                    for (std::size_t r = 0; r < resource_count; ++r)
                        if (rc.inputs[r] > 0.0f) worst = std::max(worst, input_tier(w, reg, ir, a, mkt, r, cid));
                    line += " " + rc.name + "[" + tier_name(worst) + "]";
                }
                gen_tiers[a] = line;
            }
        invalidate_logistics_caches(w);
    }
    std::map<entity_id, float> bal_gen;
    std::map<entity_id, double> stock_gen;
    for (const auto& [cid, cc] : w.corporations) { bal_gen[cid] = cc.balance; stock_gen[cid] = pool_units(w, cid); }

    // --- the settle, every corp's per-building rows kept for the window ---
    std::vector<economy_report> settle_reps;
    std::vector<std::pair<int, agency_event>> settle_events;
    for (int step = 0; step < k_campaign_settle_ticks; ++step)
    {
        settle_tick_result res = run_settle_tick(w, reg, step, 0, true);
        for (const agency_event& e : res.report.agency_events) settle_events.push_back({step, e});
        if (step >= k_campaign_settle_ticks - k_settle_window) settle_reps.push_back(std::move(res.report));
    }
    // --- the seat ---
    const spawn_seat_result seat = seat_player_corporation(w, seed, start->land.search.winner_score);
    const entity_id s = seat.seated;
    int rank = -1;
    for (std::size_t i = 0; i < seat.candidates.size(); ++i)
        if (seat.candidates[i].corp == s) rank = static_cast<int>(i);
    std::printf("\n=== seed %u ===\n", seed);
    std::printf(" seat %llu (provisional generation pick %llu%s); drawn rank %d of %d specialists, shortlist %d%s; "
                "first-ranked %llu%s\n",
                (unsigned long long)s, (unsigned long long)provisional, provisional == s ? ", SAME" : "",
                rank + 1, seat.specialist_count, seat.shortlist_size, seat.floor_unmet ? " FLOOR UNMET" : "",
                seat.candidates.empty() ? 0ULL : (unsigned long long)seat.candidates.front().corp,
                (!seat.candidates.empty() && seat.candidates.front().corp == s) ? " (= seat)" : "");
    const auto cit = w.corporations.find(s);
    if (cit == w.corporations.end()) { std::printf(" NO SEAT\n"); return; }
    {
        const corporation_component& cc = cit->second;
        std::printf(" \"%s\" focus %d; cash gen %.0f -> handoff %.0f (of which clean-slate refund %.1f); "
                    "pool units gen %.0f -> handoff %.0f\n",
                    cc.name.c_str(), static_cast<int>(cc.focus), bal_gen[s], cc.balance,
                    static_cast<double>(cc.refund_unbooked), stock_gen[s], pool_units(w, s));
        std::array<double, resource_count> by{};
        for (std::size_t r = 0; r < resource_count; ++r) by[r] = corp_shelf_stock(w, s, r); // BL-1265
        std::vector<std::pair<double, std::size_t>> top;
        for (std::size_t r = 0; r < resource_count; ++r) if (by[r] >= 0.5) top.emplace_back(by[r], r);
        std::sort(top.rbegin(), top.rend());
        std::printf("   handoff stock:");
        for (std::size_t i = 0; i < top.size() && i < 8; ++i) std::printf(" %s %.0f", rname(top[i].second).c_str(), top[i].first);
        std::printf("\n");
    }

    {
        static const char* kn[] = {"recipe_switch", "idled", "built", "demolished", "workforce_set", "resumed",
                                   "road_placed", "survey_dispatched", "hired", "order_placed", "order_removed"};
        std::printf("   seat agency in the settle:");
        for (const auto& [step, e] : settle_events)
        {
            if (e.corp != s || e.what == agency_event::kind::workforce_set) continue;
            const recipe* rc = e.what == agency_event::kind::recipe_switch ? reg.get_recipe(e.new_recipe) : nullptr;
            std::printf(" [t%d %s %llu%s%s]", step, kn[static_cast<int>(e.what)], (unsigned long long)e.building,
                        rc ? " -> " : "", rc ? rc->name.c_str() : "");
        }
        std::printf("\n");
    }
    // --- holdings, and the reach of each processor input at the handoff ---
    std::vector<holding> hs;
    {
        std::vector<entity_id> assets(cit->second.assets.begin(), cit->second.assets.end());
        std::sort(assets.begin(), assets.end());
        input_reach ir = make_input_reach(w, reg);
        for (const entity_id a : assets)
        {
            const auto bi = w.buildings.find(a);
            if (bi == w.buildings.end()) continue;
            const building_component& b = bi->second;
            holding h;
            h.id = a; h.type = b.type; h.market = market_for_tile(w, b.tile);
            char buf[96];
            if (b.type == building_type::extraction_site)
                std::snprintf(buf, sizeof buf, "extract %s", rname(static_cast<std::size_t>(b.target_resource)).c_str());
            else if (b.type == building_type::processing_facility)
            {
                const recipe* rc = reg.get_recipe(b.recipe);
                std::snprintf(buf, sizeof buf, "process %s", rc ? rc->name.c_str() : "(no recipe)");
                if (rc)
                    for (std::size_t r = 0; r < resource_count; ++r)
                        if (rc->inputs[r] > 0.0f) h.input_tier.emplace_back(r, input_tier(w, reg, ir, a, h.market, r, s));
            }
            else std::snprintf(buf, sizeof buf, "type %d", static_cast<int>(b.type));
            h.what = buf;
            for (const economy_report& rep : settle_reps) h.settle_states += state_of(reg, b, row_of(rep, a));
            hs.push_back(std::move(h));
        }
        invalidate_logistics_caches(w);
    }
    // NOTE: settle_states above read the building's CURRENT flags against old rows;
    // a building decommissioned at the end reads 'd' throughout. The row letters
    // (R/i/n/u/x/o) are faithful.

    // --- COUNTERFACTUAL (--drop-builds): the seat's settle-started projects
    // removed at the handoff, so the play rows show the seat WITHOUT the
    // construction backlog its spectate-era scorer left it. NOT the shipped
    // world — an attribution device only.
    if (g_drop_builds)
    {
        corporation_component& sc = w.corporations.at(s);
        std::vector<entity_id> keep;
        int dropped = 0;
        for (const entity_id a : sc.assets)
        {
            const auto bi = w.buildings.find(a);
            if (bi != w.buildings.end() && bi->second.ticks_remaining > 0)
            {
                w.buildings.erase(bi);
                w.stockpiles.erase(a);
                ++dropped;
                continue;
            }
            keep.push_back(a);
        }
        sc.assets = keep;
        invalidate_logistics_caches(w);
        std::printf("   COUNTERFACTUAL: dropped %d settle-started projects at the handoff\n", dropped);
    }
    // --- play ---
    constexpr int k_days = 90;
    const float res_mult = reg.price_band().reservation_mult;
    for (int k = 1; k <= play; ++k)
    {
        advance_orbits(w, static_cast<double>(k_days));
        advance_surveys(w, k_days);
        w.current_day_tick = k * k_days;
        pre_draw_snap snap;
        snap.corp = s;
        settle_tick_hooks hooks;
        hooks.after_lap = snap_pre_draw;
        hooks.ctx = &snap;
        settle_tick_result res = run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), k * k_days, false, &hooks);
        for (holding& h : hs)
        {
            const auto bi = w.buildings.find(h.id);
            if (bi == w.buildings.end()) { h.play_states += '-'; continue; }
            const building_component& b = bi->second;
            h.wf_target = static_cast<float>(b.workforce_target);
            const building_report* br = row_of(res.report, h.id);
            const char c = state_of(reg, b, br);
            h.play_states += c;
            if (br)
            {
                h.play_out += br->output_quantity;
                const auto mi = w.markets.find(h.market);
                if (mi != w.markets.end())
                {
                    if (b.type == building_type::extraction_site)
                        h.play_value += br->output_quantity * posted_price(mi->second, static_cast<std::size_t>(b.target_resource));
                    else if (const recipe* rc = reg.get_recipe(b.recipe))
                        for (std::size_t r = 0; r < resource_count; ++r)
                            if (rc->outputs[r] > 0.0f && br->active)
                                h.play_value += rc->outputs[r] / std::max(1e-6f, [&] { float t = 0; for (float o : rc->outputs) t += o; return t; }())
                                              * br->output_quantity * posted_price(mi->second, r);
                    if (c == 'i')
                    {
                        const std::size_t r = static_cast<std::size_t>(br->limiting_input);
                        starve_note& sn = h.starve[r];
                        ++sn.n; sn.input = r;
                        sn.price = snap.price.count(h.market) ? snap.price[h.market][r] : 0.0f;
                        sn.base = mi->second.base_price[r];
                        sn.ceil = res_mult * sn.base;
                        sn.pool = snap.pool.count(h.market) ? snap.pool[h.market][r] : 0.0f;
                        sn.shelf = snap.shelf.count(h.market) ? snap.shelf[h.market][r] : 0.0f;
                        const recipe* rc = reg.get_recipe(b.recipe);
                        sn.need = rc ? rc->inputs[r] * reg.economics(building_type::processing_facility).base_rate
                                         * br->effective_workforce * std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f)
                                         * building_supply_scalar(b) : 0.0f;
                    }
                }
            }
        }
    }

    // --- flows ---
    const corporation_component& cc = w.corporations.at(s);
    const std::size_t n = cc.returns.size();
    const std::size_t total = static_cast<std::size_t>(k_settle_window + play);
    const std::size_t from = n > total ? n - total : 0;
    std::printf("   quarter |   income  expend   maint   wages  levies  upkeep |  op.net | interest   balance\n");
    double settle_sum = 0, play_sum = 0;
    int ns = 0, np = 0;
    std::array<double, 6> play_flow{};
    for (std::size_t i = from; i < n; ++i)
    {
        const quarterly_return& q = cc.returns[i];
        const bool is_play = (n - i) <= static_cast<std::size_t>(play);
        const int label = is_play ? static_cast<int>(play - (n - i) + 1) : -static_cast<int>(n - i - play);
        std::printf("   %s%3d | %8.1f %7.1f %7.1f %7.1f %7.1f %7.1f | %7.1f | %8.1f %9.0f\n", is_play ? "play " : "settle", label,
                    q.income, q.expenditure, q.maintenance, q.wages, q.levies, q.upkeep, opnet(q), q.interest, q.balance);
        if (is_play)
        {
            play_sum += opnet(q); ++np;
            play_flow[0] += q.income; play_flow[1] += q.expenditure; play_flow[2] += q.maintenance;
            play_flow[3] += q.wages; play_flow[4] += q.levies; play_flow[5] += q.upkeep;
        }
        else { settle_sum += opnet(q); ++ns; }
    }
    for (double& x : play_flow) x /= std::max(1, np);
    std::printf("   holdings (settle last %d | play %d):\n", k_settle_window, play);
    for (const holding& h : hs)
    {
        std::printf("    %6llu %-40s mkt %-6llu %s | %s  wf_tgt %.0f  play out %.1f value %.1f\n",
                    (unsigned long long)h.id, h.what.c_str(), (unsigned long long)h.market,
                    h.settle_states.c_str(), h.play_states.c_str(), h.wf_target, h.play_out, h.play_value);
        if (gen_tiers.count(h.id))
            std::printf("           recipe tiers as built:%s\n", gen_tiers.at(h.id).c_str());
        if (!h.input_tier.empty())
        {
            std::printf("           inputs:");
            for (const auto& [r, t] : h.input_tier) std::printf(" %s[%s]", rname(r).c_str(), tier_name(t));
            std::printf("\n");
        }
        for (const auto& [r, sn] : h.starve)
            std::printf("           starved on %s %d play ticks: posted %.2f vs ceiling %.2f (base %.2f)%s, pool %.1f, shelf %.1f\n",
                        rname(r).c_str(), sn.n, sn.price, sn.ceil, sn.base, sn.price > sn.ceil ? " OVER CEILING" : "",
                        sn.pool, sn.shelf);
    }
    std::printf(" SUMMARY seed %u seat %llu rank %d: settle op.net mean %.1f | play op.net mean %.1f "
                "(inc %.1f exp %.1f maint %.1f wage %.1f levy %.1f upk %.1f) | cash %.0f -> %.0f\n",
                seed, (unsigned long long)s, rank + 1, ns ? settle_sum / ns : 0.0, np ? play_sum / np : 0.0,
                play_flow[0], play_flow[1], play_flow[2], play_flow[3], play_flow[4], play_flow[5],
                bal_gen[s], cc.balance);
    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv)
{
    int play = 8;
    std::vector<std::uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = parse_seed_list(argv[++i]);
        else if (!std::strcmp(argv[i], "--play") && i + 1 < argc) play = std::max(1, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--drop-builds")) g_drop_builds = true;
        else { std::fprintf(stderr, "usage: seat_day1 [--seeds a,b] [--play N] [--drop-builds]\n"); return 2; }
    }
    if (seeds.empty()) seeds = library_seeds("docs/generation/seed_library.json");
    if (seeds.empty()) { std::printf("FATAL no seeds\n"); return 1; }
    std::printf("seat_day1 — BL-1206 (seat day-1 profit): %zu seeds, settle window %d, play %d\n",
                seeds.size(), k_settle_window, play);
    for (const std::uint32_t seed : seeds) run_seed(seed, play);
    return 0;
}
