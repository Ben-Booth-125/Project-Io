// ---------------------------------------------------------------------------
// water_pair_probe — BL-1203 (surplus water reaches the markets that are dry)
// ---------------------------------------------------------------------------
// WHY THIS EXISTS. The sprint 49 k re-sweep (drafts/sprint-49-k-resweep.md)
// found thousands of water units parked on surplus shelves while ~17 dry
// consuming markets per seed pay ~7x base, and named two brakes from a pair
// breakdown over EVERY destination: no route, and no room. This probe asks the
// narrower question BL-1203 needs answered: for each (surplus market, dry
// market) WATER pair, which rule of the market-export dispatcher
// (export_market_shelves, supply_system.cpp) refuses it — and, per dry market,
// how close the best surplus source comes to sending.
//
// THE WORLD. The seated game, exactly as market_viability steps it: the app's
// start world, the 12-tick settle (spectating, day 0), the seat, then play ticks
// through `run_settle_tick`. Read after the run_economy_step lap (production,
// construction and dispatch have run; the clear has not) on the probe ticks:
// what is STILL on a surplus shelf after this tick's dispatch, and why it did
// not go to each dry market.
//
// CLASSES (first rule failed, in the dispatcher's own order):
//   body     the dry market is on another body (a market has no space lane)
//   gate     price_d <= price_src x (1 + dispatch_margin)
//   noroute  price_market_export_leg is not viable: split into no path at all,
//            fewer than 2 active ports on the body, no port the source reaches
//            overland, no port that reaches the destination overland, or both
//            ends reach a port but no sea leg joins them
//   costly   routed (land or sea leg), but price_d - haul - price_src does not
//            clear the margin
//   noroom   dispatch_absorbable at the landed cost, less pending and the
//            destination's shelf, is <= 0
//   room     the rule would send (held back by the one-destination-per-pass
//            rule or the passive-LP cap, or sent elsewhere this tick)
// Per dry market, its BEST class over every surplus source (room > noroom >
// costly > gate > noroute > body) — "is this market land-locked from every
// surplus" is the share whose best class is noroute or body.
//
// ROOM COUNTERFACTUALS (reported, never applied). For every dry market whose
// best source is routed and priced (costly / noroom / room), at the cheapest
// landed cost L among those sources:
//   room_now  dispatch_absorbable(L) - pending - shelf (what the rule sees)
//   room_A    the same derivation with the households' bid RE-READ AT L — the
//             elastic factor clamp((base/L)^e) in place of the one at the
//             posted price — i.e. the want a hauler landing at L would meet
//   room_B    room_A plus the water want of the market's processors that the
//             fair-price ceiling suppressed (BL-1172: a draw over
//             reservation_mult x base does not bid), counted only when
//             L <= reservation_mult x base so the ceiling would admit them.
//             The processors' need is base_rate x effective workforce (this
//             tick's report row) x workforce target x supply scalar x the
//             recipe's water input, less their own pools: an ESTIMATE.
//
// A PURE READER. The hook receives a const world; the router's path caches are
// warmed through a const_cast (a cache fill computes the answer the next call
// would compute; it never changes one). The wall clock is not read.
//
// Usage (repo root):
//   build_gen/verify/water_pair_probe.exe [--seeds 0,43,10] [--ticks 100]
//        [--probe 30,50,75,100] [--sea X] [--handling X] [--good water]
//   --sea / --handling override the registry AFTER the world is built (the
//   play only — generation placed its firms at the shipped rates): a preview,
//   not the shipped world.
// Build: bash tools/verify/build_lua_harness.sh water_pair_probe
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/spawn_seat.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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

enum cls : int { c_body = 0, c_noroute, c_gate, c_costly, c_noroom, c_room, c_count };
const char* k_cls[c_count] = {"body", "noroute", "gate", "costly", "noroom", "room"};
// best-of order: higher is closer to sending
const int k_rank[c_count] = {0, 1, 2, 3, 4, 5};

struct tally
{
    long probes = 0;
    long surplus_mkts = 0, dry_mkts = 0, consuming = 0;
    double surplus_units = 0.0;
    long pairs[c_count] = {};
    long noroute_nopath = 0, noroute_ports_lt2 = 0, noroute_no_port_src = 0,
         noroute_no_port_dst = 0, noroute_no_sea = 0;
    long costly_land = 0, costly_sea = 0, room_land = 0, room_sea = 0, noroom_land = 0,
         noroom_sea = 0;
    long noroom_absorb0 = 0, noroom_pending = 0;
    double costly_haul_sum = 0.0, costly_gap_sum = 0.0;
    long best[c_count] = {};
    // room counterfactuals over dry markets whose best class is costly/noroom/room
    long cf_n = 0, cf_admit = 0;
    double room_now = 0.0, room_a = 0.0, room_b = 0.0, latent = 0.0, hh_bid = 0.0,
           hh_bid_at_l = 0.0, landed_over_base = 0.0, demand = 0.0;
    // sea vs land where both exist, over surplus->dry same-body pairs that route
    long both_modes = 0, sea_cheaper = 0;
};

struct probe_ctx
{
    const recipe_registry* reg = nullptr;
    std::size_t G = 0;
    int lap = -1;
    bool armed = false;
    tally* t = nullptr;
    // dry markets with a routed best source, for the post-tick latent want
    struct cf_row
    {
        entity_id d;
        float landed;
        float room_a_no_latent; // room_A
        float ratio2;           // (base/L)^2
        bool admit;
    };
    std::vector<cf_row> cf;
};

float household_bid_at(const recipe_registry& reg, const market_component& dm, std::size_t G,
                       float L)
{
    const population_demand_params& pd = reg.population_demand();
    const float base  = dm.base_price[G];
    const float price = dm.price[G] > 0.0f ? dm.price[G] : base;
    const float e_now = std::clamp(std::pow(base / price, pd.demand_elasticity), pd.elasticity_min,
                                   pd.elasticity_max);
    const float e_l = std::clamp(std::pow(base / L, pd.demand_elasticity), pd.elasticity_min,
                                 pd.elasticity_max);
    return e_now > 0.0f ? dm.household_bid[G] * (e_l / e_now) : 0.0f;
}

void probe_hook(const world& cw, int lap, void* vctx)
{
    auto* ctx = static_cast<probe_ctx*>(vctx);
    if (!ctx->armed || lap != ctx->lap)
        return;
    world& w = const_cast<world&>(cw); // path caches only (see header)
    const recipe_registry& reg = *ctx->reg;
    const std::size_t G = ctx->G;
    tally& t = *ctx->t;
    ++t.probes;

    const float margin = reg.dispatch_margin();
    const float ceil   = reg.price_band().ceil_mult;
    const float res    = reg.price_band().reservation_mult;
    const logistics_nodes nodes = collect_logistics_nodes(w);
    std::vector<entity_id> corp_ids, mids;
    for (const auto& kv : w.corporations) corp_ids.push_back(kv.first);
    for (const auto& kv : w.markets) mids.push_back(kv.first);
    std::sort(corp_ids.begin(), corp_ids.end());
    std::sort(mids.begin(), mids.end());
    reservation_memo memo;

    std::vector<entity_id> sur, dry;
    for (const entity_id m : mids)
    {
        const market_component& mc = w.markets.at(m);
        if (!(mc.base_price[G] > 0.0f)) continue;
        if (mc.household_bid[G] > 0.0f)
        {
            ++t.consuming;
            if (mc.inventory[G] < 1.0f) dry.push_back(m);
        }
        const float s = market_shelf_surplus(w, m, G);
        if (s >= 1.0f && mc.centre_tile != null_entity)
        {
            sur.push_back(m);
            t.surplus_units += s;
        }
    }
    t.surplus_mkts += static_cast<long>(sur.size());
    t.dry_mkts += static_cast<long>(dry.size());

    for (const entity_id d : dry)
    {
        const market_component& dm = w.markets.at(d);
        const float base = dm.base_price[G];
        const float p_d  = dispatch_market_price(dm, G);
        int best = -1;
        float best_l = 0.0f;
        for (const entity_id s : sur)
        {
            if (s == d) continue;
            const market_component& sm = w.markets.at(s);
            const float p_s = dispatch_market_price(sm, G);
            int c = -1;
            convoy_mode mode = convoy_mode::land;
            float landed = 0.0f;
            if (sm.body != dm.body) c = c_body;
            else if (!(p_d > p_s + margin * p_s)) c = c_gate;
            else
            {
                const convoy_leg leg = price_market_export_leg(w, reg, nodes, s, d, 1.0f);
                // sea vs land comparison where both exist (diagnostic)
                if (!leg.viable)
                {
                    c = c_noroute;
                    const entity_id o = sm.centre_tile, dc = dm.centre_tile;
                    const logistics_path& p = intra_body_path(w, sm.body, o, dc);
                    const std::vector<entity_id> ports = body_active_port_tiles(w, sm.body);
                    if (!p.reachable && ports.size() < 2) ++t.noroute_nopath;
                    else if (ports.size() < 2) ++t.noroute_ports_lt2;
                    else
                    {
                        bool from_s = false, to_d = false;
                        for (const entity_id pt : ports)
                        {
                            if (!from_s && intra_body_leg_path(w, sm.body, o, pt, leg_domain::land).reachable) from_s = true;
                            if (!to_d && intra_body_leg_path(w, sm.body, pt, dc, leg_domain::land).reachable) to_d = true;
                        }
                        if (!from_s) ++t.noroute_no_port_src;
                        else if (!to_d) ++t.noroute_no_port_dst;
                        else ++t.noroute_no_sea;
                    }
                }
                else
                {
                    mode = leg.mode;
                    const float haul = leg.cost;
                    landed = p_s + haul;
                    if (!(p_d - haul - p_s > margin * p_s))
                    {
                        c = c_costly;
                        (mode == convoy_mode::sea ? t.costly_sea : t.costly_land)++;
                        t.costly_haul_sum += haul;
                        t.costly_gap_sum += p_d - p_s;
                    }
                    else
                    {
                        const float absorb = dispatch_absorbable(w, reg, d, G, landed);
                        const float pend = dispatch_pending(w, reg, d, G, corp_ids, memo)
                                         + std::max(0.0f, dm.inventory[G]);
                        if (!(absorb - pend > 0.0f))
                        {
                            c = c_noroom;
                            (mode == convoy_mode::sea ? t.noroom_sea : t.noroom_land)++;
                            (absorb > 0.0f ? t.noroom_pending : t.noroom_absorb0)++;
                        }
                        else
                        {
                            c = c_room;
                            (mode == convoy_mode::sea ? t.room_sea : t.room_land)++;
                        }
                    }
                }
            }
            ++t.pairs[c];
            if (c >= c_costly && (best < c_costly || landed < best_l)) best_l = landed;
            if (best < 0 || k_rank[c] > k_rank[best]) best = c;
        }
        if (best < 0) continue; // no surplus market at all this tick
        ++t.best[best];
        if (best >= c_costly)
        {
            // room counterfactuals at the cheapest routed landed cost
            const float L = best_l;
            const float S = pricing_supply(dm, G, reg.price_band().shelf_supply_ticks);
            const float pend = dispatch_pending(w, reg, d, G, corp_ids, memo)
                             + std::max(0.0f, dm.inventory[G]);
            const float now = std::max(0.0f, dispatch_absorbable(w, reg, d, G, L) - pend);
            const float bid_l = household_bid_at(reg, dm, G, L);
            const float other = std::max(0.0f, dm.demand[G] - dm.household_bid[G]);
            const float r2 = (L > 0.0f) ? (base / L) * (base / L) : 0.0f;
            float a = 0.0f;
            if (L < ceil * base)
                a = (S <= 0.0f) ? (bid_l + other) : (bid_l + other) * r2 - S;
            a = std::max(0.0f, a - pend);
            ++t.cf_n;
            t.room_now += now;
            t.room_a += a;
            t.hh_bid += dm.household_bid[G];
            t.hh_bid_at_l += bid_l;
            t.demand += dm.demand[G];
            t.landed_over_base += L / base;
            const bool admit = L <= res * base;
            if (admit) ++t.cf_admit;
            ctx->cf.push_back({d, L, a, S <= 0.0f ? 1.0f : r2, admit});
        }
    }
}

/// After the tick: the suppressed processor want per counterfactual dry market.
void add_latent(const world& w, const recipe_registry& reg, const economy_report& rep,
                std::size_t G, probe_ctx& ctx)
{
    std::map<entity_id, float> eff;
    for (const building_report& br : rep.buildings) eff[br.building] = br.effective_workforce;
    const float base_rate = reg.economics(building_type::processing_facility).base_rate;
    std::map<entity_id, float> latent;
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::processing_facility || b.decommissioned || b.ticks_remaining > 0)
            continue;
        const recipe* rc = reg.get_recipe(b.recipe);
        if (rc == nullptr || !(rc->inputs[G] > 0.0f)) continue;
        const auto e = eff.find(bid);
        if (e == eff.end()) continue;
        const entity_id m = market_for_tile(w, b.tile);
        const float wt = std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);
        const float need = rc->inputs[G] * base_rate * e->second * wt * building_supply_scalar(b);
        entity_id owner = null_entity;
        for (const auto& [cid, cc] : w.corporations)
            if (std::find(cc.assets.begin(), cc.assets.end(), bid) != cc.assets.end()) { owner = cid; break; }
        const stockpile_component* p = owner != null_entity ? w.find_pool(owner, m) : nullptr;
        latent[m] += std::max(0.0f, need - (p ? p->quantities[G] : 0.0f));
    }
    for (const auto& row : ctx.cf)
    {
        const auto it = latent.find(row.d);
        const float lw = it == latent.end() ? 0.0f : it->second;
        ctx.t->latent += lw;
        ctx.t->room_b += row.room_a_no_latent + (row.admit ? lw * row.ratio2 : 0.0f);
    }
    ctx.cf.clear();
}

void print(const char* who, const tally& t)
{
    const double n = t.probes > 0 ? static_cast<double>(t.probes) : 1.0;
    long pairs = 0;
    for (long v : t.pairs) pairs += v;
    std::printf("\n== %s: %ld probe ticks; per probe: consuming %.1f, surplus mkts %.1f (%.0f u), dry mkts %.1f; pairs %.1f\n",
                who, t.probes, t.consuming / n, t.surplus_mkts / n, t.surplus_units / n, t.dry_mkts / n, pairs / n);
    std::printf("   pairs by first refusal (per probe):");
    for (int c = 0; c < c_count; ++c) std::printf("  %s %.1f", k_cls[c], t.pairs[c] / n);
    std::printf("\n   noroute: no path & <2 ports %.1f | <2 ports %.1f | no port reached overland from src %.1f | none reaching dst %.1f | no sea leg %.1f\n",
                t.noroute_nopath / n, t.noroute_ports_lt2 / n, t.noroute_no_port_src / n,
                t.noroute_no_port_dst / n, t.noroute_no_sea / n);
    std::printf("   costly: land %.1f sea %.1f (mean haul/u %.2f vs mean gap p_d-p_s %.2f) | noroom: land %.1f sea %.1f (absorbable 0 %.1f, <= pending %.1f) | room: land %.1f sea %.1f\n",
                t.costly_land / n, t.costly_sea / n,
                (t.costly_land + t.costly_sea) ? t.costly_haul_sum / (t.costly_land + t.costly_sea) : 0.0,
                (t.costly_land + t.costly_sea) ? t.costly_gap_sum / (t.costly_land + t.costly_sea) : 0.0,
                t.noroom_land / n, t.noroom_sea / n, t.noroom_absorb0 / n, t.noroom_pending / n,
                t.room_land / n, t.room_sea / n);
    std::printf("   dry markets by BEST class (per probe):");
    for (int c = 0; c < c_count; ++c) std::printf("  %s %.1f", k_cls[c], t.best[c] / n);
    const long dry = [&] { long s = 0; for (long v : t.best) s += v; return s; }();
    std::printf("\n   land-locked from every surplus (best = body/noroute): %.0f%% of dry markets\n",
                dry ? 100.0 * (t.best[c_body] + t.best[c_noroute]) / dry : 0.0);
    const double m = t.cf_n ? static_cast<double>(t.cf_n) : 1.0;
    std::printf("   room counterfactuals over %ld routed dry markets (mean L/base %.2f; L <= reservation in %ld): mean demand %.2f, hh bid %.2f -> at L %.2f, latent processor want %.2f; ROOM now %.2f | A (bid re-read at L) %.2f | B (A + suppressed processor want) %.2f units per market\n",
                t.cf_n, t.landed_over_base / m, t.cf_admit, t.demand / m, t.hh_bid / m,
                t.hh_bid_at_l / m, t.latent / m, t.room_now / m, t.room_a / m, t.room_b / m);
}

std::vector<int> parse_ints(const std::string& s)
{
    std::vector<int> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ','))
        if (!tok.empty()) out.push_back(std::atoi(tok.c_str()));
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<int> seeds{0, 43, 10};
    int ticks = 100;
    std::vector<int> probes{30, 50, 75, 100};
    float sea = -1.0f, handling = -1.0f;
    std::string good = "water";
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        const bool more = i + 1 < argc;
        if (a == "--seeds" && more) seeds = parse_ints(argv[++i]);
        else if (a == "--ticks" && more) ticks = std::atoi(argv[++i]);
        else if (a == "--probe" && more) probes = parse_ints(argv[++i]);
        else if (a == "--sea" && more) sea = static_cast<float>(std::atof(argv[++i]));
        else if (a == "--handling" && more) handling = static_cast<float>(std::atof(argv[++i]));
        else if (a == "--good" && more) good = argv[++i];
        else { std::fprintf(stderr, "unknown arg %s\n", a.c_str()); return 2; }
    }
    bool ok = false;
    const std::size_t G = static_cast<std::size_t>(resource_names::resource_from_name(good, ok));
    if (!ok) { std::fprintf(stderr, "unknown good %s\n", good.c_str()); return 2; }
    std::set<int> probe_set(probes.begin(), probes.end());

    tally pooled;
    int failures = 0;
    for (const int seed : seeds)
    {
        lua_state lua;
        world_params p;
        p.seed = static_cast<std::uint32_t>(seed);
        auto start = std::make_unique<app_start_world>();
        try { build_app_start_world(lua, p, *start); }
        catch (const std::exception& e) { std::printf("seed %d: build threw %s\n", seed, e.what()); ++failures; continue; }
        world& w = start->w;
        if (sea >= 0.0f) start->reg.set_logistics_cost(convoy_mode::sea, sea);
        if (handling >= 0.0f) start->reg.set_port_handling(handling);
        const recipe_registry& reg = start->reg;
        for (int step = 0; step < k_campaign_settle_ticks; ++step)
            run_settle_tick(w, reg, step, 0, true);
        seat_player_corporation(w, static_cast<std::uint32_t>(seed), start->land.search.winner_score);

        tally t;
        probe_ctx ctx;
        ctx.reg = &reg;
        ctx.G = G;
        ctx.t = &t;
        for (int i = 0; i < k_campaign_settle_lap_count; ++i)
            if (std::strcmp(k_campaign_settle_lap_names[i], "run_economy_step") == 0) ctx.lap = i;
        settle_tick_hooks hooks;
        hooks.after_lap = probe_hook;
        hooks.ctx = &ctx;
        constexpr int k_econ_tick_days = 90;
        for (int k = 1; k <= ticks; ++k)
        {
            const int day = k * k_econ_tick_days;
            advance_orbits(w, static_cast<double>(k_econ_tick_days));
            advance_surveys(w, k_econ_tick_days);
            w.current_day_tick = day;
            ctx.armed = probe_set.count(k) != 0;
            settle_tick_result res = run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), day,
                                                     false, &hooks);
            if (ctx.armed) add_latent(w, reg, res.report, G, ctx);
        }
        char who[64];
        std::snprintf(who, sizeof who, "seed %d (%s)", seed, good.c_str());
        print(who, t);
        // pool
        pooled.probes += t.probes; pooled.surplus_mkts += t.surplus_mkts; pooled.dry_mkts += t.dry_mkts;
        pooled.consuming += t.consuming; pooled.surplus_units += t.surplus_units;
        for (int c = 0; c < c_count; ++c) { pooled.pairs[c] += t.pairs[c]; pooled.best[c] += t.best[c]; }
        pooled.noroute_nopath += t.noroute_nopath; pooled.noroute_ports_lt2 += t.noroute_ports_lt2;
        pooled.noroute_no_port_src += t.noroute_no_port_src; pooled.noroute_no_port_dst += t.noroute_no_port_dst;
        pooled.noroute_no_sea += t.noroute_no_sea;
        pooled.costly_land += t.costly_land; pooled.costly_sea += t.costly_sea;
        pooled.room_land += t.room_land; pooled.room_sea += t.room_sea;
        pooled.noroom_land += t.noroom_land; pooled.noroom_sea += t.noroom_sea;
        pooled.noroom_absorb0 += t.noroom_absorb0; pooled.noroom_pending += t.noroom_pending;
        pooled.costly_haul_sum += t.costly_haul_sum; pooled.costly_gap_sum += t.costly_gap_sum;
        pooled.cf_n += t.cf_n; pooled.cf_admit += t.cf_admit; pooled.room_now += t.room_now;
        pooled.room_a += t.room_a; pooled.room_b += t.room_b; pooled.latent += t.latent;
        pooled.hh_bid += t.hh_bid; pooled.hh_bid_at_l += t.hh_bid_at_l;
        pooled.landed_over_base += t.landed_over_base; pooled.demand += t.demand;
        std::fflush(stdout);
    }
    print("POOLED", pooled);
    std::printf("\n=== water_pair_probe: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
