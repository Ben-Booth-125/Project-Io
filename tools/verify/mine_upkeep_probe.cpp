// ---------------------------------------------------------------------------
// mine_upkeep_probe — BL-1228 (mine upkeep supply), diagnosis probe
// ---------------------------------------------------------------------------
// QUESTION. The extractors of silica / copper ore / rare earth ore run at a
// mean supply scalar ~0.72 (inflow_probe round 2). Which upkeep goods are
// short, where would the draw have found them, and why did it not?
//
// THE WORLD. Seated exactly as market_viability / inflow_probe seat it:
// build_app_start_world, the 12-tick settle, seat_player_corporation, then
// PLAY ticks through run_settle_tick.
//
// THE REPLICA (how a pure reader sees the draw). run_building_upkeep is the
// LAST phase of run_economy_step and dispatch follows it inside the same lap,
// so no after-lap state shows the shelves and pools the draw saw. So at lap 0
// (after convoys) the probe COPIES the world, runs run_economy_step on the copy
// under a registry copy whose building-upkeep goods are zeroed (the pass is
// then inert: it is last, and a zero basket draws nothing and moves no factor),
// and on that pre-upkeep copy re-walks run_building_upkeep's draw itself —
// every completed building in ascending id, the grid strip, pool first, then
// the tile market's shelf under shelf_admits. The copy is the probe's; the
// simulated world is never written. CROSS-CHECK: the replica's unmet flag per
// focus extractor is compared with the REAL world's supply-factor step this
// tick (down, or held at the floor = unmet; up, or held at 1000 = met), and the
// agreement is printed — the replica is trusted only as far as that holds.
//
// CAUSE of a focus extractor's short upkeep good g at its market d, read at
// the moment of its own draw in the replica:
//   ceiling      d's shelf held g but posted above base x reservation_mult
//   nomarket     the building's body has no market (body-level pool only)
//   shelf:<cls>  d's shelf was empty (or drained by earlier draws) and another
//                market on the body has a shelf surplus of g (dispatcher rule);
//                <cls> = export_refusal's BEST class over those sources
//                (body/noroute/gate/costly/noroom/room) — the shelf exporter's
//                verdict for hauling g to d
//   grid:short   power (BL-1230): the building's grid held some, but less than
//                every open bid on it, so each got its pro-rata share
//   grid:none    power (BL-1230): no shelf on the building's grid held any
//                (a grid with no generation, or none listed last tick)
//   (BL-1230: power is drawn as run_building_upkeep draws it — wired by the
//   PROVINCE, bid on the tile's market, filled from every shelf on the grid
//   pro rata after every building has bid. Other grid goods keep the tile-
//   reach wire and the local shelf.)
//   pool:<cls>   no shelf surplus elsewhere, but a pool on the body (key != d)
//                holds g; <cls> = the owner corp's dispatcher rule for its own
//                pools (reserved/gate/noroute/costly/room), or 'othercorp' when
//                only other corps' pools hold it
//   nospare      g is made on the body (a live producer) but nothing is held
//                anywhere on it at this instant
//   notmade      no live producer of g on the body and nothing held
// A 'producer' is a live extraction site targeting g or a live processor whose
// recipe outputs g.
//
// SCALAR LOSS. The step is binary on 'any good short' (decay 50, recovery 100,
// floor 500), so loss is attributed per building to its unmet ticks: each
// unmet tick is split equally over the goods short that tick, by cause, and a
// building's mean (1 - scalar) over the run is divided pro rata over its own
// unmet-tick shares.
//
// Usage (repo root): build_gen/verify/mine_upkeep_probe.exe [--seeds a,b] [--ticks N]
// Build:  ./tools/verify/build_lua_harness.sh mine_upkeep_probe
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "export_refusal.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp"
#include "world/resource_names.hpp"
#include "world/recipe_registry.hpp"
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
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr int k_sel_ticks[] = {10, 25, 50, 100};
constexpr std::size_t k_focus[] = {static_cast<std::size_t>(resource_type::silica),
                                   static_cast<std::size_t>(resource_type::copper_ore),
                                   static_cast<std::size_t>(resource_type::rare_earth_ore)};
constexpr int k_nfocus = 3;

using arr = std::array<float, resource_count>;
std::string gname(std::size_t g) { return resource_names::name_of(static_cast<resource_type>(g)); }

// ---- causes -----------------------------------------------------------------
enum cause : int
{
    k_ceiling = 0, k_nomarket,
    k_shelf_body, k_shelf_noroute, k_shelf_gate, k_shelf_costly, k_shelf_noroom, k_shelf_room,
    k_grid, k_grid_drained,
    k_pool_reserved, k_pool_gate, k_pool_noroute, k_pool_costly, k_pool_room, k_pool_other,
    k_nospare, k_notmade, k_ncause
};
const char* k_cause[k_ncause] = {
    "ceiling", "nomarket",
    "shelf:body", "shelf:noroute", "shelf:gate", "shelf:costly", "shelf:noroom", "shelf:room",
    "grid:none (its grid held none)", "grid:short (shared pro rata)",
    "pool:reserved", "pool:gate", "pool:noroute", "pool:costly", "pool:room", "pool:othercorp",
    "nospare(made, none held)", "notmade(on body)"};
// the brief's three families
int family(int c)
{
    if (c == k_ceiling) return 0;                       // ceiling
    if (c == k_nospare || c == k_notmade || c == k_nomarket || c == k_grid) return 2; // not available on the body / grid
    if (c == k_grid_drained) return 3;                  // d's own shelf too small
    return 1;                                           // made/held elsewhere, refused
}
const char* k_family[4] = {"ceiling (shelf held it, priced over)", "held elsewhere on body, not moved (by class)",
                           "not on the body (none made spare / not made / no market) or, power, none on its grid",
                           "power: its grid held some, short, shared pro rata"};

/// A building is the probe's subject if it is a live extraction site on a tile
/// carrying a focus deposit (co-extraction included, inflow_probe's set).
int focus_of(const world& w, const building_component& b)
{
    if (b.type != building_type::extraction_site) return -1;
    const auto ti = w.tiles.find(b.tile);
    if (ti == w.tiles.end()) return -1;
    if (placement_rules::is_depositless_site(w, b.tile, b.target_resource)) return -1;
    // the site's own target first, then any co-extracted focus deposit
    for (int fi = 0; fi < k_nfocus; ++fi)
        if (static_cast<std::size_t>(b.target_resource) == k_focus[fi]) return fi;
    for (int fi = 0; fi < k_nfocus; ++fi)
        if (ti->second.resource_deposit[k_focus[fi]] > 0.0f) return fi;
    return -1;
}

// ---- tallies ----------------------------------------------------------------
struct good_t
{
    double need = 0, pool = 0, shelf = 0, shortu = 0; ///< units over every focus draw
    long draws = 0, short_draws = 0;
    std::array<double, k_ncause> short_units{};
    std::array<double, k_ncause> unmet_ticks{};      ///< unmet building-ticks, split over short goods
    std::array<double, k_ncause> loss{};              ///< scalar-loss attribution
    double ceil_ratio = 0; long ceil_n = 0;           ///< posted / ceiling where 'ceiling'
};

struct bld_hist
{
    int fi = -1;
    long ticks = 0, unmet = 0;
    double loss = 0;                                  ///< sum of (1 - scalar)
    std::map<std::pair<std::size_t, int>, double> share; ///< (good, cause) -> unmet-tick share
};

struct seed_out
{
    std::array<good_t, resource_count> g{};
    std::map<entity_id, bld_hist> hist;
    long agree = 0, disagree = 0;
    // walk: per tick, over live focus extractors
    std::vector<double> mean_s, frac_unmet, frac_floor, frac_full;
    std::vector<long> n_live;
};

// ---- the per-tick context --------------------------------------------------
struct probe
{
    int lap_conv = -1;
    std::unique_ptr<world> copy; ///< the world at lap 0
    std::map<entity_id, int> before; ///< real factor at lap 0
};

void after_lap(const world& cw, int lap, void* ctx)
{
    auto* p = static_cast<probe*>(ctx);
    if (lap != p->lap_conv) return;
    p->copy = std::make_unique<world>(cw);
    p->before.clear();
    for (const auto& [bid, b] : cw.buildings) p->before[bid] = b.supply_factor_permille;
}

/// Producers of g on body (live extraction sites targeting g, live processors outputting g).
bool made_on_body(const world& w, const recipe_registry& reg, entity_id body, std::size_t g)
{
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.decommissioned || b.ticks_remaining > 0) continue;
        const auto ti = w.tiles.find(b.tile);
        if (ti == w.tiles.end() || ti->second.body != body) continue;
        if (b.type == building_type::extraction_site && static_cast<std::size_t>(b.target_resource) == g) return true;
        if (b.type == building_type::processing_facility)
            if (const recipe* rc = reg.get_recipe(b.recipe); rc && rc->outputs[g] > 0.0f) return true;
    }
    return false;
}

struct short_rec
{
    entity_id bid; int fi; std::size_t g; float need, pool, shelf, shortu; int c; float ratio;
};

/// What a grid-cleared bid still lacks: exactly 0 when the clear covered it.
float covered_left(float open, float filled) { return filled >= open ? 0.0f : open - filled; }

/// Replay run_building_upkeep's draw on the pre-upkeep copy. Returns, per focus
/// building, (replica unmet, short goods). Mutates only the copy.
void replay(world& W, const recipe_registry& reg, std::map<entity_id, bool>& unmet_out,
            std::vector<short_rec>& shorts, std::map<entity_id, std::vector<std::pair<std::size_t, float>>>& basket_of)
{
    const building_upkeep_params& up = reg.building_upkeep();
    const era_band band = reg.era();
    const bool grid_rules = reg.grid_goods().any();
    const float res_mult = reg.price_band().reservation_mult;
    const float margin = reg.dispatch_margin();
    std::array<arr, building_type_count> basket{};
    std::array<bool, building_type_count> any{};
    for (std::size_t t = 0; t < building_type_count; ++t)
    {
        basket[t] = building_upkeep_goods(up, static_cast<building_type>(t), band);
        for (std::size_t r = 0; r < resource_count; ++r) if (basket[t][r] > 0.0f) { any[t] = true; break; }
    }
    std::map<entity_id, entity_id> owner_of;
    for (const auto& [corp, cc] : W.corporations)
        for (const entity_id bid : cc.assets)
            if (W.buildings.count(bid)) owner_of[bid] = corp;

    export_refusal::context x(W, reg);
    std::map<std::pair<entity_id, std::size_t>, export_refusal::good_markets> scans; // (body, g) lazily
    std::map<entity_id, arr> inv_start; // every shelf at the start of the upkeep pass
    for (const auto& [mid0, mc0] : W.markets) inv_start.emplace(mid0, mc0.inventory);
    const logistics_nodes nodes = collect_logistics_nodes(W);
    std::vector<entity_id> corp_ids;
    for (const auto& kv : W.corporations) corp_ids.push_back(kv.first);
    std::sort(corp_ids.begin(), corp_ids.end());
    reservation_memo memo;

    // BL-1230: open power bids awaiting the grid clear (every building, focus or not).
    struct pending { entity_id bid; int fi; std::size_t r; float need, pool, open; entity_id corp, mid; std::uint32_t grid; };
    std::vector<pending> pend;

    for (const auto& [bid, corp] : owner_of)
    {
        const building_component& b = W.buildings.at(bid);
        if (b.ticks_remaining > 0 || b.decommissioned) continue;
        const std::size_t ti = static_cast<std::size_t>(b.type);
        if (ti >= building_type_count || !any[ti]) continue;
        const auto tit = W.tiles.find(b.tile);
        if (tit == W.tiles.end()) continue;
        const entity_id body = tit->second.body;
        if (grid_rules) body_reach_field(W, body);
        arr need = basket[ti];
        bool any_need = true, connected = true;
        std::uint32_t grid_id = 0;
        if (grid_rules)
        {
            // BL-1230: power is wired by the province; other grid goods by the tile.
            grid_id = tile_power_grid(W, b.tile);
            const float rc = tile_reach_cost(W, b.tile);
            connected = (rc >= 0.0f) && std::isfinite(rc);
            bool stripped = false;
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                if (!reg.grid_goods().grid(r) || !(need[r] > 0.0f)) continue;
                const bool wired = grid_good_crosses_markets(r) ? grid_id != 0 : connected;
                if (!wired) { need[r] = 0.0f; stripped = true; }
            }
            if (stripped)
            {
                any_need = false;
                for (std::size_t r = 0; r < resource_count; ++r) if (need[r] > 0.0f) any_need = true;
            }
        }
        const int fi = focus_of(W, b);
        if (!any_need) { if (fi >= 0) unmet_out[bid] = false; continue; }

        const entity_id mid = market_for_tile(W, b.tile);
        market_component* m = (mid != null_entity) ? &W.markets.at(mid) : nullptr;
        const entity_id pool_key = (mid != null_entity) ? mid : body;
        stockpile_component& pool = W.pool_at(corp, pool_key);
        bool unmet = false;
        if (fi >= 0)
        {
            auto& bv = basket_of[bid];
            bv.clear();
            for (std::size_t r = 0; r < resource_count; ++r) if (need[r] > 0.0f) bv.push_back({r, need[r]});
        }
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float required = need[r];
            if (!(required > 0.0f)) continue;
            const float have = std::max(0.0f, pool.quantities[r]);
            const float take = std::min(required, have);
            pool.quantities[r] = have - take;
            float shortfall = required - take;
            float drawn = 0.0f;
            const float inv_before = m ? std::max(0.0f, m->inventory[r]) : 0.0f;
            const bool admits = m && shelf_admits(*m, r, res_mult, false);
            const bool via_grid = grid_rules && grid_id != 0 && grid_good_crosses_markets(r);
            if (via_grid && shortfall > 0.0f && admits)
            {
                // BL-1230: bid here, filled by the grid clear below.
                pend.push_back({bid, fi, r, required, take, shortfall, corp, mid, grid_id});
                continue;
            }
            if (shortfall > 0.0f && admits)
            {
                drawn = std::min(shortfall, inv_before);
                m->inventory[r] -= drawn;
                shortfall -= drawn;
            }
            if (shortfall > 0.0f) unmet = true;
            if (fi < 0) continue;
            short_rec s{bid, fi, r, required, take, drawn, std::max(0.0f, shortfall), -1, 0.0f};
            if (shortfall > 0.0f)
            {
                // ---- the cause --------------------------------------------
                int c;
                if (m == nullptr) c = k_nomarket;
                else if (!admits && (inv_before > 0.0f || via_grid))
                {
                    c = k_ceiling;
                    const float ceil = m->base_price[r] * res_mult;
                    s.ratio = ceil > 0.0f ? posted_price(*m, r) / ceil : 0.0f;
                }
                else
                {
                    c = -1;
                    const bool grid = reg.grid_goods().grid(r);
                    // other shelves / pools on the body holding g
                    bool other_shelf = false, owner_pool = false, other_pool = false;
                    for (const entity_id om : x.mids)
                    {
                        if (om == mid) continue;
                        const market_component& omc = W.markets.at(om);
                        if (omc.body == body && omc.inventory[r] > 0.0f) other_shelf = true;
                    }
                    for (const auto& [pk, st] : W.corp_market_pools)
                    {
                        if (pk.second == pool_key || !(st.quantities[r] > 0.0f)) continue;
                        if (pool_key_body(W, pk.second) != body) continue;
                        (pk.first == corp ? owner_pool : other_pool) = true;
                    }
                    if (grid)
                    {
                        // a grid good the ruling does not name (capacity):
                        // the tile-local draw, as before BL-1230
                        if (inv_start.at(mid)[r] > 0.0f) c = k_grid_drained;
                        else if (other_shelf || owner_pool || other_pool) c = k_grid;
                    }
                    else
                    {
                        auto si = scans.find({body, r});
                        if (si == scans.end())
                        {
                            export_refusal::good_markets gm = export_refusal::scan_good(x, r, export_refusal::surplus_rule::dispatcher);
                            export_refusal::good_markets on_body;
                            for (const entity_id sm : gm.sur) if (W.markets.at(sm).body == body) on_body.sur.push_back(sm);
                            si = scans.emplace(std::make_pair(body, r), on_body).first;
                        }
                        const export_refusal::best_result br = export_refusal::classify_destination(x, mid, r, si->second.sur);
                        if (br.best >= 0) c = k_shelf_body + br.best;
                        else if (owner_pool)
                        {
                            // the owner's own dispatcher rule, best over its pools on the body
                            int best = -1;
                            const float price_d = dispatch_market_price(*m, r);
                            for (const auto& [pk, st] : W.corp_market_pools)
                            {
                                if (pk.first != corp || pk.second == pool_key || !(st.quantities[r] > 0.0f)) continue;
                                if (pool_key_body(W, pk.second) != body) continue;
                                auto rit = memo.find(pk);
                                if (rit == memo.end()) rit = memo.emplace(pk, processor_reservation(W, reg, corp, pk.second)).first;
                                const float surplus = st.quantities[r] - rit->second[r] - dispatch_arrived(W, corp, pk.second, r);
                                int k;
                                if (!(surplus > 0.0f)) k = 0;
                                else
                                {
                                    const float ps = dispatch_home_price(W, pk.second, r);
                                    if (!(price_d > ps + margin * ps)) k = 1;
                                    else
                                    {
                                        const convoy_leg leg = price_convoy_leg(W, reg, nodes, corp, pk.second, mid, r, 1.0f,
                                                                                reg.logistics_cost(convoy_mode::space));
                                        if (!leg.viable) k = 2;
                                        else if (!(price_d - leg.cost - ps > margin * ps)) k = 3;
                                        else k = 4;
                                    }
                                }
                                // rank: room > costly > noroute > gate > reserved (closer to sending)
                                static const int rank[5] = {0, 2, 1, 3, 4};
                                if (best < 0 || rank[k] > rank[best]) best = k;
                            }
                            c = k_pool_reserved + best;
                        }
                        else if (other_pool || other_shelf) c = k_pool_other;
                    }
                    if (c < 0) c = made_on_body(W, reg, body, r) ? k_nospare : k_notmade;
                }
                s.c = c;
                shorts.push_back(s);
            }
            else
                shorts.push_back(s);
        }
        if (fi >= 0) unmet_out[bid] = unmet;
        (void)connected;
    }

    // ---- BL-1230: the grid clear, exactly as run_building_upkeep does it ----
    if (!pend.empty())
    {
        std::map<std::uint32_t, std::vector<entity_id>> shelves_on;
        {
            std::vector<entity_id> mids;
            for (const auto& kv : W.markets) mids.push_back(kv.first);
            std::sort(mids.begin(), mids.end());
            for (const entity_id m0 : mids)
            {
                const entity_id centre = W.markets.at(m0).centre_tile;
                if (centre == null_entity) continue;
                const std::uint32_t g = tile_power_grid(W, centre);
                if (g != 0) shelves_on[g].push_back(m0);
            }
        }
        std::map<std::uint32_t, std::vector<std::size_t>> waiting_on;
        for (std::size_t i = 0; i < pend.size(); ++i) waiting_on[pend[i].grid].push_back(i);
        std::vector<float> filled(pend.size(), 0.0f);
        std::vector<char> grid_held(pend.size(), 0);
        for (const auto& [g, idxs] : waiting_on)
        {
            const auto sit = shelves_on.find(g);
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                float asked = 0.0f;
                for (const std::size_t i : idxs) if (pend[i].r == r) asked += pend[i].open;
                if (!(asked > 0.0f) || sit == shelves_on.end()) continue;
                float held = 0.0f;
                for (const entity_id m0 : sit->second) held += std::max(0.0f, W.markets.at(m0).inventory[r]);
                if (!(held > 0.0f)) continue;
                const bool covered = held >= asked;
                const float fill = covered ? 1.0f : held / asked;
                const float take = covered ? asked / held : 1.0f;
                for (const entity_id m0 : sit->second)
                {
                    float& inv = W.markets.at(m0).inventory[r];
                    if (!(inv > 0.0f)) continue;
                    inv = covered ? std::max(0.0f, inv - inv * take) : 0.0f;
                }
                for (const std::size_t i : idxs)
                {
                    if (pend[i].r != r) continue;
                    grid_held[i] = 1;
                    filled[i] = covered ? pend[i].open : pend[i].open * fill;
                }
            }
        }
        for (std::size_t i = 0; i < pend.size(); ++i)
        {
            const pending& q = pend[i];
            const float left = covered_left(q.open, filled[i]);
            if (q.fi < 0) continue;
            if (left > 0.0f) unmet_out[q.bid] = true;
            short_rec s{q.bid, q.fi, q.r, q.need, q.pool, filled[i], left, -1, 0.0f};
            if (left > 0.0f) s.c = grid_held[i] ? k_grid_drained : k_grid;
            shorts.push_back(s);
        }
    }
}

void run_seed(std::uint32_t seed, int ticks, seed_out& S)
{
    lua_state lua;
    world_params wp;
    wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    try { build_app_start_world(lua, wp, *start); }
    catch (const std::exception& e) { std::printf("seed %u: build threw %s\n", seed, e.what()); return; }
    world& w = start->w;
    const recipe_registry& reg = start->reg;
    const building_upkeep_params& up = reg.building_upkeep();
    auto reg0 = std::make_unique<recipe_registry>(reg);
    {
        building_upkeep_params z = up;
        z.goods = {};
        reg0->set_building_upkeep(z);
    }
    std::printf("\n=== seed %u (era band %d; upkeep decay %d recovery %d floor %d; reservation_mult %.2f) ===\n", seed,
                static_cast<int>(reg.era()), up.supply_decay_permille, up.supply_recovery_permille,
                up.supply_floor_permille, reg.price_band().reservation_mult);
    {
        const arr bk = building_upkeep_goods(up, building_type::extraction_site, reg.era());
        std::printf("  extraction_site basket:");
        for (std::size_t r = 0; r < resource_count; ++r) if (bk[r] > 0.0f) std::printf(" %s %.2f%s", gname(r).c_str(), bk[r], reg.grid_goods().grid(r) ? "(grid)" : "");
        std::printf("\n");
    }

    for (int step = 0; step < k_campaign_settle_ticks; ++step) run_settle_tick(w, reg, step, 0, true);
    seat_player_corporation(w, seed, start->land.search.winner_score);

    probe p;
    for (int i = 0; i < k_campaign_settle_lap_count; ++i)
        if (!std::strcmp(k_campaign_settle_lap_names[i], "convoys")) p.lap_conv = i;

    std::array<good_t, resource_count> seed_g{};
    constexpr int k_days = 90;
    for (int k = 1; k <= ticks; ++k)
    {
        advance_orbits(w, static_cast<double>(k_days));
        advance_surveys(w, k_days);
        w.current_day_tick = k * k_days;
        settle_tick_hooks hooks;
        hooks.after_lap = after_lap;
        hooks.ctx = &p;
        (void)run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), k * k_days, false, &hooks);

        // ---- the replica: pre-upkeep state on the copy --------------------
        world& W = *p.copy;
        (void)run_economy_step(W, *reg0, false, nullptr);
        std::map<entity_id, bool> rep_unmet;
        std::vector<short_rec> shorts;
        std::map<entity_id, std::vector<std::pair<std::size_t, float>>> baskets;
        replay(W, reg, rep_unmet, shorts, baskets);

        // ---- the real step, cross-check, walk -----------------------------
        double ssum = 0; long n = 0, nu = 0, nfl = 0, nfu = 0;
        std::map<entity_id, std::vector<std::pair<std::size_t, int>>> short_goods;
        for (const short_rec& s : shorts)
        {
            good_t& G = S.g[s.g];
            G.need += s.need; G.pool += s.pool; G.shelf += s.shelf; G.shortu += s.shortu; ++G.draws;
            if (s.c >= 0)
            {
                ++G.short_draws;
                G.short_units[s.c] += s.shortu;
                if (s.c == k_ceiling) { G.ceil_ratio += s.ratio; ++G.ceil_n; }
                short_goods[s.bid].push_back({s.g, s.c});
            }
        }
        for (const auto& [bid, b] : w.buildings)
        {
            if (b.decommissioned || b.ticks_remaining > 0) continue;
            const int fi = focus_of(w, b);
            if (fi < 0) continue;
            const auto bi = p.before.find(bid);
            if (bi == p.before.end()) continue; // built this tick
            const int before = bi->second, after = b.supply_factor_permille;
            const bool real_unmet = after < before || (after == before && before <= up.supply_floor_permille && before < 1000);
            const auto ri = rep_unmet.find(bid);
            const bool rep = ri != rep_unmet.end() && ri->second;
            if (ri != rep_unmet.end()) { if (rep == real_unmet) ++S.agree; else ++S.disagree; }
            const float sc = building_supply_scalar(b);
            ssum += sc; ++n;
            if (real_unmet) ++nu;
            if (after <= up.supply_floor_permille) ++nfl;
            if (after >= 1000) ++nfu;
            bld_hist& H = S.hist[bid];
            H.fi = fi; ++H.ticks; H.loss += 1.0 - sc;
            if (rep)
            {
                ++H.unmet;
                const auto& sg = short_goods[bid];
                for (const auto& gc : sg) H.share[gc] += 1.0 / double(sg.size());
                for (const auto& gc : sg) S.g[gc.first].unmet_ticks[gc.second] += 1.0 / double(sg.size());
            }
        }
        S.mean_s.push_back(n ? ssum / n : 0); S.frac_unmet.push_back(n ? double(nu) / n : 0);
        S.frac_floor.push_back(n ? double(nfl) / n : 0); S.frac_full.push_back(n ? double(nfu) / n : 0);
        S.n_live.push_back(n);

        const bool sel = std::find(std::begin(k_sel_ticks), std::end(k_sel_ticks), k) != std::end(k_sel_ticks);
        if (sel)
        {
            std::printf("  t%-3d live focus extractors %ld: mean scalar %.3f, unmet this tick %.0f%%, at floor %.0f%%, at 1000 %.0f%%\n",
                        k, n, n ? ssum / n : 0.0, n ? 100.0 * nu / n : 0.0, n ? 100.0 * nfl / n : 0.0, n ? 100.0 * nfu / n : 0.0);
            // per good this tick
            std::map<std::size_t, std::array<double, 4>> pg; // need, pool, shelf, short
            std::map<std::size_t, std::array<double, k_ncause>> pc;
            for (const short_rec& s : shorts)
            {
                auto& a = pg[s.g]; a[0] += s.need; a[1] += s.pool; a[2] += s.shelf; a[3] += s.shortu;
                if (s.c >= 0) pc[s.g][s.c] += 1;
            }
            for (const auto& [g, a] : pg)
            {
                std::printf("      %-10s need %6.2f  pool %6.2f  shelf %6.2f  short %6.2f (%.0f%%) | short draws by cause:", gname(g).c_str(),
                            a[0], a[1], a[2], a[3], a[0] > 0 ? 100.0 * a[3] / a[0] : 0.0);
                for (int c = 0; c < k_ncause; ++c) if (pc[g][c] > 0) std::printf(" %s %.0f", k_cause[c], pc[g][c]);
                std::printf("\n");
            }
            // two samples
            int printed = 0;
            for (const auto& [bid, sg] : short_goods)
            {
                if (printed >= 2) break;
                ++printed;
                const building_component& b = w.buildings.at(bid);
                std::printf("      e.g. site %u (%s) scalar %.2f short:", unsigned(bid), gname(k_focus[S.hist[bid].fi]).c_str(), building_supply_scalar(b));
                for (const auto& gc : sg) std::printf(" %s[%s]", gname(gc.first).c_str(), k_cause[gc.second]);
                std::printf("\n");
            }
        }
        std::fflush(stdout);
    }
    // ---- seed summary -----------------------------------------------------
    std::printf("  replica vs real unmet flag: agree %ld, disagree %ld\n", S.agree, S.disagree);
    std::printf("  walk (every 10 ticks): ");
    for (std::size_t i = 9; i < S.mean_s.size(); i += 10) std::printf(" t%zu %.3f/%.0f%%u", i + 1, S.mean_s[i], 100.0 * S.frac_unmet[i]);
    std::printf("\n");
}

void print_goods(const std::array<good_t, resource_count>& G, const std::map<std::pair<std::size_t, int>, double>& loss_by,
                 double loss_tot)
{
    for (std::size_t g = 0; g < resource_count; ++g)
    {
        const good_t& t = G[g];
        if (t.draws == 0) continue;
        double ut = 0; for (double v : t.unmet_ticks) ut += v;
        std::printf("  %-10s draws %ld (short %ld = %.0f%%): need %.1f = pool %.0f%% + shelf %.0f%% + SHORT %.0f%% | unmet building-ticks %.1f\n",
                    gname(g).c_str(), t.draws, t.short_draws, t.draws ? 100.0 * t.short_draws / t.draws : 0.0, t.need,
                    t.need > 0 ? 100.0 * t.pool / t.need : 0.0, t.need > 0 ? 100.0 * t.shelf / t.need : 0.0,
                    t.need > 0 ? 100.0 * t.shortu / t.need : 0.0, ut);
        for (int c = 0; c < k_ncause; ++c)
        {
            if (!(t.short_units[c] > 0)) continue;
            const auto li = loss_by.find({g, c});
            const double l = li == loss_by.end() ? 0.0 : li->second;
            std::printf("      %-26s short units %8.2f (%4.0f%%)  unmet-ticks %7.1f  scalar loss %5.1f%% of total%s\n", k_cause[c],
                        t.short_units[c], t.shortu > 0 ? 100.0 * t.short_units[c] / t.shortu : 0.0, t.unmet_ticks[c],
                        loss_tot > 0 ? 100.0 * l / loss_tot : 0.0,
                        c == k_ceiling && t.ceil_n ? (" (mean posted/ceiling " + std::to_string(t.ceil_ratio / t.ceil_n).substr(0, 5) + ")").c_str() : "");
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    int ticks = 100;
    std::vector<std::uint32_t> seeds = {0, 43, 10, 28, 38};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::max(10, std::atoi(argv[++i]));
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
        else { std::fprintf(stderr, "usage: mine_upkeep_probe [--seeds a,b] [--ticks N]\n"); return 2; }
    }
    std::printf("mine_upkeep_probe - BL-1228: which upkeep goods hold silica / copper_ore / rare_earth_ore extractors below 1.0, and why\n");

    std::array<good_t, resource_count> all{};
    std::map<std::pair<std::size_t, int>, double> loss_by;
    double loss_tot = 0; long agree = 0, dis = 0;
    std::array<double, k_nfocus> fsum{}; std::array<long, k_nfocus> fn{};
    // unmet-fraction histogram over building histories: 0, (0,1/3), [1/3,2/3), [2/3,1), 1
    std::array<long, 5> hist{}; std::array<double, 5> hist_loss{};
    std::vector<double> walk_s, walk_u, walk_fl, walk_fu; std::vector<long> walk_n;
    for (const std::uint32_t s : seeds)
    {
        seed_out S;
        run_seed(s, ticks, S);
        agree += S.agree; dis += S.disagree;
        for (std::size_t g = 0; g < resource_count; ++g)
        {
            good_t& a = all[g]; const good_t& b = S.g[g];
            a.need += b.need; a.pool += b.pool; a.shelf += b.shelf; a.shortu += b.shortu;
            a.draws += b.draws; a.short_draws += b.short_draws; a.ceil_ratio += b.ceil_ratio; a.ceil_n += b.ceil_n;
            for (int c = 0; c < k_ncause; ++c) { a.short_units[c] += b.short_units[c]; a.unmet_ticks[c] += b.unmet_ticks[c]; }
        }
        for (const auto& [bid, H] : S.hist)
        {
            loss_tot += H.loss;
            fsum[H.fi] += H.loss; fn[H.fi] += H.ticks;
            double tot = 0; for (const auto& kv : H.share) tot += kv.second;
            for (const auto& kv : H.share) loss_by[kv.first] += tot > 0 ? H.loss * kv.second / tot : 0.0;
            if (tot <= 0 && H.loss > 0) loss_by[{resource_count, -1}] += H.loss; // loss with no unmet tick (carried in)
            const double f = H.ticks ? double(H.unmet) / H.ticks : 0.0;
            const int bin = f <= 0.0 ? 0 : f < 1.0 / 3 ? 1 : f < 2.0 / 3 ? 2 : f < 1.0 ? 3 : 4;
            ++hist[bin]; hist_loss[bin] += H.loss;
        }
        if (walk_s.size() < S.mean_s.size()) { walk_s.resize(S.mean_s.size()); walk_u.resize(S.mean_s.size()); walk_fl.resize(S.mean_s.size()); walk_fu.resize(S.mean_s.size()); walk_n.resize(S.mean_s.size()); }
        for (std::size_t i = 0; i < S.mean_s.size(); ++i)
        {
            walk_s[i] += S.mean_s[i] * S.n_live[i]; walk_u[i] += S.frac_unmet[i] * S.n_live[i];
            walk_fl[i] += S.frac_floor[i] * S.n_live[i]; walk_fu[i] += S.frac_full[i] * S.n_live[i]; walk_n[i] += S.n_live[i];
        }
    }

    std::printf("\n================ POOLED over %zu seeds, %d ticks ================\n", seeds.size(), ticks);
    std::printf("  replica cross-check (unmet flag vs the real factor step): agree %ld, disagree %ld (%.2f%%)\n", agree, dis,
                agree + dis ? 100.0 * dis / (agree + dis) : 0.0);
    for (int fi = 0; fi < k_nfocus; ++fi)
        std::printf("  %-15s mean scalar over building-ticks %.3f (%ld building-ticks)\n", gname(k_focus[fi]).c_str(),
                    fn[fi] ? 1.0 - fsum[fi] / fn[fi] : 0.0, fn[fi]);
    long nb = 0; for (long v : hist) nb += v;
    std::printf("  buildings by unmet-tick fraction (and their share of the scalar loss):\n");
    const char* hn[5] = {"never unmet", "(0,1/3)", "[1/3,2/3)", "[2/3,1)", "always unmet"};
    for (int i = 0; i < 5; ++i)
        std::printf("    %-13s %5ld (%4.0f%%)  loss share %4.0f%%\n", hn[i], hist[i], nb ? 100.0 * hist[i] / nb : 0.0,
                    loss_tot > 0 ? 100.0 * hist_loss[i] / loss_tot : 0.0);
    std::printf("\n  BY UPKEEP GOOD AND CAUSE (focus extractors' own draws):\n");
    print_goods(all, loss_by, loss_tot);
    {
        const auto it = loss_by.find({resource_count, -1});
        if (it != loss_by.end())
            std::printf("  scalar loss with no unmet tick in the run (carried in from the settle): %.1f%%\n", loss_tot > 0 ? 100.0 * it->second / loss_tot : 0.0);
    }
    std::printf("\n  BY FAMILY (scalar loss):\n");
    std::array<double, 4> fam{};
    for (const auto& [k, v] : loss_by) if (k.second >= 0) fam[family(k.second)] += v;
    for (int i = 0; i < 4; ++i) std::printf("    %-58s %5.1f%%\n", k_family[i], loss_tot > 0 ? 100.0 * fam[i] / loss_tot : 0.0);
    std::printf("  BY GOOD (scalar loss):");
    std::map<std::size_t, double> bygood;
    for (const auto& [k, v] : loss_by) if (k.second >= 0) bygood[k.first] += v;
    for (const auto& [g, v] : bygood) std::printf("  %s %.1f%%", gname(g).c_str(), loss_tot > 0 ? 100.0 * v / loss_tot : 0.0);
    std::printf("\n\n  THE WALK (pooled, per tick: mean scalar / unmet this tick / at floor / at 1000):\n");
    for (std::size_t i = 0; i < walk_s.size(); ++i)
        if (i < 5 || (i + 1) % 5 == 0)
        {
            const double n = walk_n[i] ? double(walk_n[i]) : 1.0;
            std::printf("    t%-3zu n %4ld  scalar %.3f  unmet %3.0f%%  floor %3.0f%%  full %3.0f%%\n", i + 1, walk_n[i],
                        walk_s[i] / n, 100.0 * walk_u[i] / n, 100.0 * walk_fl[i] / n, 100.0 * walk_fu[i] / n);
        }
    return 0;
}
