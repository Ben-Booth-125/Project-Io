// ---------------------------------------------------------------------------
// idle_mines_probe — BL-1227 (idle mines), diagnosis probe
// ---------------------------------------------------------------------------
// QUESTION. inflow_probe (BL-1217 round 2) counted, per read, silica 393 sites
// under construction and 77 decommissioned against 630 live; copper ore 280 /
// 320 against 536; rare earth 302 / 255 against 315. Why does each sit idle?
//
// THE WORLD. Seated exactly as market_viability / inflow_probe seat it:
// build_app_start_world, the 12-tick settle (run_settle_tick, econ steps
// 0..11, day 0, spectating), seat_player_corporation, then PLAY ticks as
// run_app_live_window steps them. The settle is observed too (ticks -12..-1),
// so a site idled before the handoff has a history.
//
// THE SITES. Every extraction site whose tile's deposit includes silica,
// copper_ore or rare_earth_ore (co-extraction: a site works the whole tile).
//
// A PURE READER. Three after-lap reads per tick, nothing written:
//   lap 0 (convoys)  the state run_construction sees (it is the economy step's
//                    first act; only sentiment decay precedes it). Each site
//                    under construction is classified by run_construction's own
//                    rule re-applied: per-tick need row, the fair-price ceiling
//                    (shelf_admits), the shelf, the pause threshold.
//   lap 1 (economy+dispatch) progress made (did it move?), decommission and
//                    resume transitions (the loss reflex leaves loss_streak at
//                    >= 8; the idle verb zeroes it), new sites, and the prices
//                    the reflex valued output at (the clear has not run yet).
//   after the tick   the per-site economics row (the reflex's own estimate:
//                    output x the PRIMARY's price - maintenance - wages; plus the
//                    basket at each good's own price, the prorated levy and the
//                    supply scalar), and sites gone (firm exit / demolish verb).
//
// STALL CLASSES (a site under construction, at lap 0):
//   run          rate >= 1 (full)
//   stretched    0 < rate < 1 (binding material or capacity named)
//   ceil_stock   PAUSED: binding material's posted price is over the ceiling
//                (reservation_mult x base) while the shelf holds enough for a
//                stretched tick — on the shelf, refused by the ceiling
//   ceil_empty   PAUSED: over the ceiling AND the shelf is short
//   shelf_body   PAUSED: shelf short (admitted or not counted); another market
//                on the body holds at least one tick's need
//   pools_body   PAUSED: no shelf on the body holds it; corporation pools on
//                the body do (held / reserved, never listed)
//   none_body    PAUSED: nothing on the body holds it
//   contended    the solo rate was > 0 but the site made no progress (the
//                BL-1209 pro-rata ration gave its share to another site)
//   no_market    the tile resolves no market
// "Owner cannot pay" is recorded as a flag (balance < 0), not a class:
// run_construction does not gate a draw on the owner's balance.
//
// Usage: build_gen/verify/idle_mines_probe.exe [--seeds a,b] [--ticks N] [--examples N] [--dump]
// Build:  ./tools/verify/build_lua_harness.sh idle_mines_probe
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/budget_system.hpp"
#include "world/building_profit.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/spawn_seat.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr int k_read_ticks[] = {0, 10, 25, 50, 100};
constexpr int k_nreads = 5;

constexpr std::size_t k_focus[] = {static_cast<std::size_t>(resource_type::silica),
                                   static_cast<std::size_t>(resource_type::copper_ore),
                                   static_cast<std::size_t>(resource_type::rare_earth_ore)};
constexpr int k_nfocus = 3;

std::string gname(std::size_t g) { return resource_names::name_of(static_cast<resource_type>(g)); }

// --- under-construction classes ----------------------------------------------
enum ucls : int { u_run = 0, u_stretch, u_ceil_stock, u_ceil_empty, u_shelf_body, u_pools_body, u_none_body,
                  u_contended, u_nomkt, u_count };
const char* k_ucls[u_count] = {"run", "stretched", "ceil_stock", "ceil_empty", "shelf_body", "pools_body",
                               "none_body", "contended", "no_market"};
bool paused_cls(int c) { return c >= u_ceil_stock; }

// --- decommission origin -----------------------------------------------------
enum dby : int { d_pregen = 0, d_reflex, d_verb, d_count };
const char* k_dby[d_count] = {"before the settle (generation)", "loss reflex", "idle verb (scorer)"};

// --- what sank it (reflex view, over the streak window) ----------------------
enum sink : int { s_noout_unstaffed = 0, s_noout_exhausted, s_noout_supply, s_noout_other, s_maint, s_wages,
                  s_unknown, s_count };
const char* k_sink[s_count] = {"no output: unstaffed", "no output: exhausted", "no output: supply scalar 0",
                               "no output: other", "revenue < maintenance", "wages tip it (rev >= maint)",
                               "no history"};
enum pcls : int { p_ok = 0, p_glut, p_far, p_zero, p_count };
const char* k_pcls[p_count] = {"price >= 0.8 base", "low, glut (no better market on body)",
                               "low, far (a body market pays >= 1.5x home)", "no price (no market)"};

// --- gone ----------------------------------------------------------------------
enum gby : int { g_exit = 0, g_demolish, g_seat, g_count };
const char* k_gby[g_count] = {"firm exit", "demolish verb", "seat clean slate"};

// --- owner classes -----------------------------------------------------------
enum ocls : int { o_bg = 0, o_rival, o_seat, o_none, o_count };
const char* k_ocls[o_count] = {"background", "rival", "seat", "none"};

struct econ_row
{
    int tick = 0;
    float output = 0, price_ratio = 0, best_ratio = 0, revenue = 0, basket = 0, maint = 0, wages = 0, levy = 0;
    float supply = 1, eff = 0, assigned = 0;
    int wt = 100, streak = 0;
    bool active = false, exhausted = false, has = false;
    float net() const { return revenue - maint - wages; }
};

struct site
{
    entity_id bid = null_entity, tile = null_entity, body = null_entity;
    resource_type target = resource_type::iron_ore;
    unsigned gmask = 0;
    int origin_tick = INT_MIN;  ///< INT_MIN = present at build
    bool uc_at_first = false;
    int completed = INT_MIN;    ///< tick it completed (observed)
    int last_progress = INT_MIN;
    int uc_since = INT_MIN;     ///< tick the current stall began
    int decom_tick = INT_MIN;   ///< latest decommission
    int decom_by = -1;
    int sink_cls = -1, price_cls = -1;
    bool basket_profitable = false; ///< basket at own prices covered maint+wages on the streak window
    int resumed = 0, decoms = 0;
    int gone_tick = INT_MIN, gone_by = -1;
    std::deque<econ_row> hist; ///< last 10 rows
    std::vector<std::string> log;
    // lap-0 classification this tick
    int cls = -1; float solo_rate = 0; std::size_t bind = 0; float bind_ratio = 0;
    int tr0 = 0; float pr0 = 0; bool dec0 = false;
    bool neg_balance = false;
    bool stalled = false;
    int wn = 0; double w_out = 0, w_rev = 0, w_mt = 0, w_wg = 0, w_lv = 0, w_sup = 0, w_eff = 0, w_wt = 0, w_pr = 0, w_best = 0, w_bk = 0;
};

struct read_tally
{
    // [owner][state] state: 0 live, 1 uc, 2 decom
    std::array<std::array<long, 3>, o_count> by_owner{};
    std::array<std::array<long, 3>, k_nfocus> by_good{};
    std::array<long, u_count> uc_cls{};
    std::array<std::array<long, u_count>, k_nfocus> uc_cls_g{};
    std::map<std::string, long> uc_bind; ///< paused binding material name
    std::array<long, d_count> dec_by{};
    std::array<long, s_count> dec_sink{};
    std::array<long, p_count> dec_price{};
    long dec_basket_ok = 0;
    long uc_neg = 0;
    long dec_resume_pos = 0, dec_seat = 0;
    double uc_stalled_ticks = 0; long uc_stalled_n = 0;
    /// stack rank on (tile, primary) as run_extraction ranks it (ascending id):
    /// buckets 1 | 2-3 | 4-8 | 9-20 | 21+, by state (0 live, 1 uc, 2 decom)
    std::array<std::array<long, 5>, 3> rank{};
    long tall_tiles = 0; ///< (tile, primary) stacks of 9+ sites
    long tall_owners = 0; ///< distinct owners summed over those stacks
};
int rank_bucket(int r) { return r <= 1 ? 0 : r <= 3 ? 1 : r <= 8 ? 2 : r <= 20 ? 3 : 4; }
const char* k_rank[5] = {"rank 1", "2-3", "4-8", "9-20", "21+"};

struct seed_out
{
    std::array<read_tally, k_nreads> reads{};
    long uc_handoff = 0, uc_handoff_done = 0, uc_started_live = 0, uc_started_live_done = 0;
    long built_live = 0, built_on_idle_tile = 0;
    std::array<long, g_count> gone{};
    std::array<long, d_count> decom_events{};
    std::array<long, s_count> decom_sink_events{};
    std::array<long, p_count> decom_price_events{};
    long decom_basket_ok_events = 0, resumes = 0;
    std::array<long, u_count> uc_tick_cls{}; ///< every UC site-tick in live play
    std::array<std::array<double, 11>, s_count> ev{}; ///< per sink class: n, out, rev, maint, wages, levy, supply, px, best, basket, target
    std::array<long, o_count> decom_owner{};
    double stretch_rate = 0; long stretch_n = 0;
    std::map<std::string, long> pause_good;
};

struct probe
{
    int lap_conv = -1, lap_econ = -1;
    int tick = 0;
    const recipe_registry* reg = nullptr;
    std::map<entity_id, site>* sites = nullptr;
    std::map<entity_id, std::array<float, resource_count>> price1, base1; ///< lap-1 prices
    std::map<entity_id, std::set<entity_id>>* tile_history = nullptr; ///< tile -> sites ever seen idle/gone
    seed_out* out = nullptr;
};

unsigned focus_mask(const world& w, const building_component& b)
{
    if (b.type != building_type::extraction_site) return 0;
    const auto ti = w.tiles.find(b.tile);
    if (ti == w.tiles.end()) return 0;
    if (placement_rules::is_depositless_site(w, b.tile, b.target_resource)) return 0;
    unsigned m = 0;
    for (int i = 0; i < k_nfocus; ++i)
        if (ti->second.resource_deposit[k_focus[i]] > 0.0f) m |= 1u << i;
    return m;
}

std::map<entity_id, entity_id> owner_map(const world& w)
{
    std::map<entity_id, entity_id> o;
    for (const auto& [cid, cc] : w.corporations) for (const entity_id a : cc.assets) o.emplace(a, cid);
    return o;
}

int owner_class(const world& w, entity_id corp)
{
    const auto it = w.corporations.find(corp);
    if (it == w.corporations.end()) return o_none;
    if (it->second.is_player) return o_seat;
    if (it->second.is_background) return o_bg;
    return o_rival;
}

void logf(site& s, const char* fmt, ...)
{
    char buf[320];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (s.log.size() < 40) s.log.push_back(buf);
}

/// run_construction's rule, re-applied at the state it sees.
void classify_uc(const world& w, const recipe_registry& reg, const building_component& b, site& s,
                 const std::map<entity_id, entity_id>& owners)
{
    const float max_stretch = reg.construction().max_stretch;
    const float pause_below = max_stretch > 1.0f ? 1.0f / max_stretch : 0.0f;
    const float cap_rate = reg.construction().capacity_per_build_tick;
    const std::size_t ci = static_cast<std::size_t>(resource_type::construction_capacity);
    const float res_mult = reg.price_band().reservation_mult;
    const float duration = reg.economics(b.type).build_duration_ticks;
    s.cls = u_run; s.solo_rate = 1.0f; s.bind = 0; s.bind_ratio = 0;
    if (duration <= 0.0f) return;
    const auto oi = owners.find(s.bid);
    s.neg_balance = oi != owners.end() && w.corporations.at(oi->second).balance < 0.0f;
    const entity_id mid = market_for_tile(w, b.tile);
    if (mid == null_entity || !w.markets.count(mid)) { s.cls = u_nomkt; s.solo_rate = 0; return; }
    const market_component& m = w.markets.at(mid);
    std::array<float, resource_count> need{};
    const auto& row = reg.resource_build_cost_for(b.type, b.target_resource, b.recipe);
    for (std::size_t r = 0; r < resource_count; ++r) need[r] = row[r] / duration;
    if (cap_rate > 0.0f) need[ci] += cap_rate;
    float rate = 1.0f; std::size_t bind = resource_count;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (r == ci || !(need[r] > 0.0f)) continue;
        const float avail = shelf_admits(m, r, res_mult, true) ? std::max(0.0f, m.inventory[r]) : 0.0f;
        const float c = avail / need[r];
        if (c < rate) { rate = c; bind = r; }
    }
    if (cap_rate > 0.0f)
    {
        const float avail = shelf_admits(m, ci, res_mult, true) ? std::max(0.0f, m.inventory[ci]) : 0.0f;
        const float cov = need[ci] > 0.0f ? avail / need[ci] : 1.0f;
        const float c = std::max(cov, pause_below);
        if (c < rate) { rate = c; bind = ci; }
    }
    rate = std::clamp(rate, 0.0f, 1.0f);
    if (rate < pause_below) rate = 0.0f;
    s.solo_rate = rate;
    s.bind = bind < resource_count ? bind : 0;
    if (rate >= 1.0f) { s.cls = u_run; return; }
    if (rate > 0.0f) { s.cls = u_stretch; return; }
    // paused: the binding material
    const std::size_t r = bind;
    const float base = m.base_price[r];
    s.bind_ratio = base > 0.0f ? posted_price(m, r) / base : 0.0f;
    const bool adm = shelf_admits(m, r, res_mult, true);
    const float thr = need[r] * pause_below;
    if (!adm)
    {
        s.cls = std::max(0.0f, m.inventory[r]) >= thr ? u_ceil_stock : u_ceil_empty;
        return;
    }
    const entity_id body = m.body;
    for (const auto& [xid, xm] : w.markets)
        if (xid != mid && xm.body == body && xm.inventory[r] >= need[r]) { s.cls = u_shelf_body; return; }
    // BL-1265: corporations hold no pools, so `u_pools_body` can no longer
    // occur; a site no shelf on the body covers is `u_none_body`.
    s.cls = u_none_body;
}

/// Discover / refresh the focus sites present now.
void scan_sites(const world& w, probe& p, int tick, bool at_build)
{
    for (const auto& [bid, b] : w.buildings)
    {
        const unsigned gm = focus_mask(w, b);
        if (!gm) continue;
        auto it = p.sites->find(bid);
        if (it != p.sites->end()) continue;
        site s;
        s.bid = bid; s.tile = b.tile; s.target = b.target_resource; s.gmask = gm;
        s.body = w.tiles.at(b.tile).body;
        s.origin_tick = at_build ? INT_MIN : tick;
        s.uc_at_first = b.ticks_remaining > 0;
        s.dec0 = b.decommissioned; s.tr0 = b.ticks_remaining; s.pr0 = b.construction_progress;
        if (at_build && b.decommissioned) { s.decom_tick = INT_MIN; s.decom_by = d_pregen; ++s.decoms; }
        if (!at_build)
        {
            if (p.tile_history->count(b.tile)) ++p.out->built_on_idle_tile;
            if (tick >= 1) ++p.out->built_live;
            if (tick >= 1 && b.ticks_remaining > 0) ++p.out->uc_started_live;
        }
        logf(s, "t%d %s (%s, primary %s, %d build ticks)", tick, at_build ? "present at build" : "placed",
             b.ticks_remaining > 0 ? "under construction" : (b.decommissioned ? "decommissioned" : "live"),
             gname(static_cast<std::size_t>(b.target_resource)).c_str(), b.ticks_remaining);
        p.sites->emplace(bid, std::move(s));
    }
}

void after_lap(const world& w, int lap, void* ctx)
{
    auto* p = static_cast<probe*>(ctx);
    if (lap == p->lap_conv)
    {
        const auto owners = owner_map(w);
        for (auto& [bid, s] : *p->sites)
        {
            if (s.gone_tick != INT_MIN) continue;
            const auto bi = w.buildings.find(bid);
            if (bi == w.buildings.end()) continue;
            const building_component& b = bi->second;
            s.tr0 = b.ticks_remaining; s.pr0 = b.construction_progress; s.dec0 = b.decommissioned;
            s.cls = -1;
            if (b.ticks_remaining > 0) classify_uc(w, *p->reg, b, s, owners);
        }
        return;
    }
    if (lap != p->lap_econ) return;
    // prices the reflex valued at
    p->price1.clear(); p->base1.clear();
    for (const auto& [mid, mc] : w.markets) { p->price1[mid] = mc.price; p->base1[mid] = mc.base_price; }
    const int t = p->tick;
    for (auto& [bid, s] : *p->sites)
    {
        if (s.gone_tick != INT_MIN) continue;
        const auto bi = w.buildings.find(bid);
        if (bi == w.buildings.end()) continue; // demolished this lap; caught after the tick
        const building_component& b = bi->second;
        if (s.cls >= 0) // was under construction at lap 0
        {
            const bool progressed = b.ticks_remaining < s.tr0 || b.construction_progress > s.pr0 + 1e-6f;
            if (!progressed && s.solo_rate > 0.0f) s.cls = u_contended;
            if (progressed)
            {
                if (s.stalled) logf(s, "t%d moves again after %d stalled ticks", t, t - s.uc_since);
                s.stalled = false;
                s.last_progress = t;
            }
            else if (!s.stalled)
            {
                s.stalled = true;
                s.uc_since = t;
                logf(s, "t%d stalls: %s on %s (posted/base %.2f)%s", t, k_ucls[s.cls], gname(s.bind).c_str(),
                     s.bind_ratio, s.neg_balance ? " [owner balance < 0]" : "");
            }
            if (t >= 1) ++p->out->uc_tick_cls[s.cls];
            if (t >= 1 && s.cls == u_stretch) { p->out->stretch_rate += s.solo_rate; ++p->out->stretch_n; }
            if (t >= 1 && paused_cls(s.cls) && s.cls != u_contended && s.cls != u_nomkt) ++p->out->pause_good[gname(s.bind)];
            if (b.ticks_remaining <= 0)
            {
                s.completed = t;
                logf(s, "t%d COMPLETED", t);
            }
        }
        if (!s.dec0 && b.decommissioned)
        {
            s.decom_tick = t; ++s.decoms;
            s.decom_by = b.loss_streak >= 8 ? d_reflex : d_verb;
            p->tile_history->operator[](s.tile).insert(bid);
        }
        else if (s.dec0 && !b.decommissioned)
        {
            ++s.resumed; ++p->out->resumes;
            logf(s, "t%d resumed", t);
        }
    }
    scan_sites(w, *p, t, false);
}

econ_row make_row(const world& w, const recipe_registry& reg, const economy_report& rep, const probe& p,
                  const building_component& b, entity_id bid, entity_id corp, int tick,
                  const std::map<entity_id, float>& corp_ext)
{
    econ_row r;
    r.tick = tick;
    r.streak = b.loss_streak; r.supply = building_supply_scalar(b); r.assigned = b.workforce_assigned;
    r.wt = b.workforce_target;
    const auto ri = rep.building_row.find(bid);
    if (ri == rep.building_row.end() || ri->second >= rep.buildings.size()) return r;
    const building_report& br = rep.buildings[ri->second];
    r.has = true;
    r.output = br.output_quantity; r.active = br.active; r.exhausted = br.exhausted; r.eff = br.effective_workforce;
    float scalar = 1.0f;
    if (auto c = rep.workforce_contention.find({br.corp, br.body}); c != rep.workforce_contention.end()) scalar = c->second;
    if (auto bl = rep.building_labour.find(bid); bl != rep.building_labour.end()) scalar = bl->second;
    const building_opex ox = compute_building_opex(b, reg.economics(b.type), scalar, body_mean_habitability(w, br.body),
                                                   reg.idle_maintenance_floor());
    r.maint = ox.maintenance; r.wages = ox.wages;
    const entity_id mid = market_for_tile(w, b.tile);
    const auto pi = p.price1.find(mid);
    const auto bi = p.base1.find(mid);
    const std::size_t tg = static_cast<std::size_t>(b.target_resource);
    if (pi != p.price1.end())
    {
        const auto pr = [&](std::size_t g) { return pi->second[g] > 0.0f ? pi->second[g] : bi->second[g]; };
        r.revenue = br.output_quantity * pr(tg);
        r.price_ratio = bi->second[tg] > 0.0f ? pr(tg) / bi->second[tg] : 0.0f;
        // the basket at each good's own price (shares by richness, as run_extraction)
        const tile_component& tc = w.tiles.at(b.tile);
        float rt = 0.0f;
        for (const resource_type q : placement_rules::k_extractable) rt += tc.resource_deposit[static_cast<std::size_t>(q)];
        if (rt > 0.0f)
            for (const resource_type q : placement_rules::k_extractable)
            {
                const std::size_t g = static_cast<std::size_t>(q);
                if (tc.resource_deposit[g] > 0.0f) r.basket += br.output_quantity * tc.resource_deposit[g] / rt * pr(g);
            }
        // best price for the primary on the body
        const entity_id body = w.markets.at(mid).body;
        float best = 0.0f;
        for (const auto& [xid, xm] : w.markets)
        {
            if (xm.body != body) continue;
            const auto xp = p.price1.find(xid);
            const auto xb = p.base1.find(xid);
            if (xp == p.price1.end() || !(xb->second[tg] > 0.0f)) continue;
            const float v = xp->second[tg] > 0.0f ? xp->second[tg] : xb->second[tg];
            best = std::max(best, v / xb->second[tg]);
        }
        r.best_ratio = best;
    }
    if (auto bu = rep.budgets.find(corp); bu != rep.budgets.end() && bu->second.levies != 0.0f)
        if (auto ce = corp_ext.find(corp); ce != corp_ext.end() && ce->second > 0.0f)
            r.levy = bu->second.levies * br.output_quantity / ce->second;
    return r;
}

/// The sink class over the rows of the streak before a decommission.
void judge_sink(site& s)
{
    int n = 0; double out = 0, rev = 0, mt = 0, wg = 0, bk = 0, eff = 0, sup = 0, pr = 0, best = 0;
    int exh = 0;
    for (const econ_row& r : s.hist)
    {
        if (!r.has) continue;
        ++n; out += r.output; rev += r.revenue; mt += r.maint; wg += r.wages; bk += r.basket; eff += r.eff;
        sup += r.supply; pr += r.price_ratio; best += r.best_ratio; exh += r.exhausted ? 1 : 0;
    }
    double lv = 0, wt = 0;
    for (const econ_row& r : s.hist) if (r.has) { lv += r.levy; wt += r.wt; }
    s.wn = n;
    if (n) { s.w_out = out / n; s.w_rev = rev / n; s.w_mt = mt / n; s.w_wg = wg / n; s.w_lv = lv / n; s.w_sup = sup / n;
             s.w_eff = eff / n; s.w_wt = wt / n; s.w_pr = pr / n; s.w_best = best / n; s.w_bk = bk / n; }
    if (!n) { s.sink_cls = s_unknown; s.price_cls = -1; return; }
    if (!(out > 1e-6))
    {
        if (exh * 2 >= n) s.sink_cls = s_noout_exhausted;
        else if (!(eff > 1e-6)) s.sink_cls = s_noout_unstaffed;
        else if (!(sup > 1e-6)) s.sink_cls = s_noout_supply;
        else s.sink_cls = s_noout_other;
    }
    else s.sink_cls = rev < mt ? s_maint : s_wages;
    pr /= n; best /= n;
    if (!(pr > 0.0)) s.price_cls = p_zero;
    else if (pr >= 0.8) s.price_cls = p_ok;
    else s.price_cls = best >= 1.5 * pr ? p_far : p_glut;
    s.basket_profitable = out > 1e-6 && bk >= mt + wg;
}

void after_tick(world& w, const recipe_registry& reg, const settle_tick_result& res, probe& p, int tick)
{
    const auto owners = owner_map(w);
    std::set<entity_id> exited;
    for (const firm_exit_record& f : res.report.firm_exits) exited.insert(f.corp);
    std::map<entity_id, float> corp_ext;
    for (const building_report& br : res.report.buildings)
        if (br.type == building_type::extraction_site) corp_ext[br.corp] += br.output_quantity;
    std::map<entity_id, entity_id> last_owner;
    for (auto& [bid, s] : *p.sites)
    {
        if (s.gone_tick != INT_MIN) continue;
        const auto bi = w.buildings.find(bid);
        if (bi == w.buildings.end())
        {
            s.gone_tick = tick;
            // owner before the tick: the last row's corp is not stored; use the exit list membership
            bool ex = false;
            for (const firm_exit_record& f : res.report.firm_exits) (void)f;
            for (const agency_event& e : res.report.agency_events)
                if (e.building == bid && e.what == agency_event::kind::demolished) { s.gone_by = g_demolish; }
            if (s.gone_by < 0) s.gone_by = g_exit; // demolished outside the scorer: the wind-up
            (void)ex;
            ++p.out->gone[s.gone_by];
            (*p.tile_history)[s.tile].insert(bid);
            logf(s, "t%d GONE (%s)", tick, k_gby[s.gone_by]);
            continue;
        }
        const building_component& b = bi->second;
        const auto oi = owners.find(bid);
        const entity_id corp = oi == owners.end() ? null_entity : oi->second;
        if (b.ticks_remaining > 0) continue;
        if (b.decommissioned && s.decom_tick == tick)
        {
            judge_sink(s);
            logf(s, "t%d DECOMMISSIONED by %s (owner %s): %s, price %s | mean of %d rows: out %.2f rev %.2f basket %.2f maint %.2f wages %.2f levy %.2f supply %.2f eff %.2f target %.0f px/base %.2f best-on-body %.2f",
                 tick, k_dby[s.decom_by], k_ocls[owner_class(w, corp)], k_sink[s.sink_cls], s.price_cls >= 0 ? k_pcls[s.price_cls] : "-",
                 s.wn, s.w_out, s.w_rev, s.w_bk, s.w_mt, s.w_wg, s.w_lv, s.w_sup, s.w_eff, s.w_wt, s.w_pr, s.w_best);
            if (tick >= 1 && s.sink_cls >= 0)
            {
                auto& e = p.out->ev[s.sink_cls];
                e[0] += 1; e[1] += s.w_out; e[2] += s.w_rev; e[3] += s.w_mt; e[4] += s.w_wg; e[5] += s.w_lv;
                e[6] += s.w_sup; e[7] += s.w_pr; e[8] += s.w_best; e[9] += s.w_bk; e[10] += s.w_wt;
                ++p.out->decom_owner[owner_class(w, corp)];
            }
            if (tick >= 1)
            {
                ++p.out->decom_events[s.decom_by];
                ++p.out->decom_sink_events[s.sink_cls];
                if (s.price_cls >= 0) ++p.out->decom_price_events[s.price_cls];
                if (s.basket_profitable) ++p.out->decom_basket_ok_events;
            }
            continue;
        }
        if (b.decommissioned) continue;
        s.hist.push_back(make_row(w, reg, res.report, p, b, bid, corp, tick, corp_ext));
        if (s.hist.size() > 8) s.hist.pop_front();
    }
}

void take_read(world& w, const recipe_registry& reg, probe& p, int ri, int tick)
{
    read_tally& R = p.out->reads[ri];
    const auto owners = owner_map(w);
    // stacks as run_extraction ranks them: (tile, primary) -> ids ascending
    std::map<std::pair<entity_id, int>, std::vector<entity_id>> stacks;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::extraction_site) stacks[{b.tile, int(b.target_resource)}].push_back(bid);
    for (auto& [k, v] : stacks) std::sort(v.begin(), v.end());
    std::set<std::pair<entity_id, int>> tall_seen;
    for (auto& [bid, s] : *p.sites)
    {
        if (s.gone_tick != INT_MIN) continue;
        const auto bi = w.buildings.find(bid);
        if (bi == w.buildings.end()) continue;
        const building_component& b = bi->second;
        const auto oi = owners.find(bid);
        const int oc = owner_class(w, oi == owners.end() ? null_entity : oi->second);
        const int st = b.ticks_remaining > 0 ? 1 : (b.decommissioned ? 2 : 0);
        {
            const auto sk = std::make_pair(b.tile, int(b.target_resource));
            const auto& v = stacks[sk];
            const int rank = int(std::find(v.begin(), v.end(), bid) - v.begin()) + 1;
            ++R.rank[st][rank_bucket(rank)];
            if (v.size() >= 9 && tall_seen.insert(sk).second)
            {
                ++R.tall_tiles;
                std::set<entity_id> os;
                for (const entity_id x : v) if (auto o = owners.find(x); o != owners.end()) os.insert(o->second);
                R.tall_owners += long(os.size());
            }
        }
        ++R.by_owner[oc][st];
        for (int i = 0; i < k_nfocus; ++i) if (s.gmask & (1u << i)) ++R.by_good[i][st];
        if (st == 1)
        {
            if (tick == 0) classify_uc(w, reg, b, s, owners); // the handoff state
            const int c = s.cls >= 0 ? s.cls : u_run;
            ++R.uc_cls[c];
            for (int i = 0; i < k_nfocus; ++i) if (s.gmask & (1u << i)) ++R.uc_cls_g[i][c];
            if (paused_cls(c) && c != u_contended && c != u_nomkt) ++R.uc_bind[gname(s.bind)];
            if (c == u_stretch) ++R.uc_bind["(stretched by) " + gname(s.bind)];
            if (s.neg_balance) ++R.uc_neg;
            if (s.stalled && paused_cls(c)) { R.uc_stalled_ticks += tick - s.uc_since; ++R.uc_stalled_n; }
        }
        else if (st == 2)
        {
            const int by = s.decom_by >= 0 ? s.decom_by : d_pregen;
            ++R.dec_by[by];
            if (by != d_pregen)
            {
                if (s.sink_cls < 0) judge_sink(s);
                ++R.dec_sink[s.sink_cls];
                if (s.price_cls >= 0) ++R.dec_price[s.price_cls];
                if (s.basket_profitable) ++R.dec_basket_ok;
            }
            if (oc == o_seat) ++R.dec_seat;
            // would the resume candidate fire? (prospective net + the idle floor saved)
            const building_profit pp = estimate_prospective_profit(w, reg, b.tile, b.type, b.target_resource,
                                                                   b.recipe, &b, nullptr);
            const float idle_m = reg.economics(b.type).maintenance * reg.idle_maintenance_floor();
            if (pp.has_data && pp.net() + idle_m > 0.0f) ++R.dec_resume_pos;
        }
    }
}

void print_read(const char* label, const read_tally& R)
{
    std::printf("  %s\n", label);
    for (int i = 0; i < k_nfocus; ++i)
        std::printf("    %-15s live %5ld  under construction %5ld  decommissioned %5ld\n", gname(k_focus[i]).c_str(),
                    R.by_good[i][0], R.by_good[i][1], R.by_good[i][2]);
    std::printf("    owners (live/uc/decom):");
    for (int o = 0; o < o_count; ++o)
        std::printf("  %s %ld/%ld/%ld", k_ocls[o], R.by_owner[o][0], R.by_owner[o][1], R.by_owner[o][2]);
    long uc = 0; for (long v : R.uc_cls) uc += v;
    std::printf("\n    UNDER CONSTRUCTION %ld (sites):", uc);
    for (int c = 0; c < u_count; ++c) if (R.uc_cls[c]) std::printf("  %s %ld", k_ucls[c], R.uc_cls[c]);
    std::printf("\n      paused on:");
    for (const auto& [g, n] : R.uc_bind) std::printf("  %s %ld", g.c_str(), n);
    std::printf("\n      owner balance < 0: %ld; mean ticks stalled (paused): %.1f over %ld\n", R.uc_neg,
                R.uc_stalled_n ? R.uc_stalled_ticks / R.uc_stalled_n : 0.0, R.uc_stalled_n);
    long dc = 0; for (long v : R.dec_by) dc += v;
    std::printf("    DECOMMISSIONED %ld:", dc);
    for (int d = 0; d < d_count; ++d) if (R.dec_by[d]) std::printf("  %s %ld", k_dby[d], R.dec_by[d]);
    std::printf("\n      sank by:");
    for (int k = 0; k < s_count; ++k) if (R.dec_sink[k]) std::printf("  %s %ld", k_sink[k], R.dec_sink[k]);
    std::printf("\n      primary price:");
    for (int k = 0; k < p_count; ++k) if (R.dec_price[k]) std::printf("  %s %ld", k_pcls[k], R.dec_price[k]);
    std::printf("\n      basket at own prices covered maint+wages: %ld; owned by the seat: %ld; resume estimate > 0 now: %ld\n",
                R.dec_basket_ok, R.dec_seat, R.dec_resume_pos);
    std::printf("      stack rank (live / uc / decom):");
    for (int k = 0; k < 5; ++k) std::printf("  %s %ld/%ld/%ld", k_rank[k], R.rank[0][k], R.rank[1][k], R.rank[2][k]);
    std::printf("\n      stacks of 9+ sites on one (tile, primary): %ld, distinct owners on them %ld\n", R.tall_tiles, R.tall_owners);
}

void add(read_tally& a, const read_tally& b)
{
    for (int o = 0; o < o_count; ++o) for (int s = 0; s < 3; ++s) a.by_owner[o][s] += b.by_owner[o][s];
    for (int i = 0; i < k_nfocus; ++i) for (int s = 0; s < 3; ++s) a.by_good[i][s] += b.by_good[i][s];
    for (int c = 0; c < u_count; ++c) { a.uc_cls[c] += b.uc_cls[c]; for (int i = 0; i < k_nfocus; ++i) a.uc_cls_g[i][c] += b.uc_cls_g[i][c]; }
    for (const auto& [g, n] : b.uc_bind) a.uc_bind[g] += n;
    for (int d = 0; d < d_count; ++d) a.dec_by[d] += b.dec_by[d];
    for (int k = 0; k < s_count; ++k) a.dec_sink[k] += b.dec_sink[k];
    for (int k = 0; k < p_count; ++k) a.dec_price[k] += b.dec_price[k];
    a.dec_basket_ok += b.dec_basket_ok; a.uc_neg += b.uc_neg; a.dec_resume_pos += b.dec_resume_pos; a.dec_seat += b.dec_seat;
    a.uc_stalled_ticks += b.uc_stalled_ticks; a.uc_stalled_n += b.uc_stalled_n;
    for (int x = 0; x < 3; ++x) for (int y = 0; y < 5; ++y) a.rank[x][y] += b.rank[x][y];
    a.tall_tiles += b.tall_tiles; a.tall_owners += b.tall_owners;
}

void print_events(const seed_out& o)
{
    std::printf("  stretched UC site-ticks: mean rate %.3f over %ld (1/rate = the build-time multiple)\n",
                o.stretch_n ? o.stretch_rate / o.stretch_n : 0.0, o.stretch_n);
    std::printf("  paused UC site-ticks by binding material:");
    for (const auto& [g, n] : o.pause_good) std::printf("  %s %ld", g.c_str(), n);
    std::printf("\n  decommission events in live play by owner:");
    for (int k = 0; k < o_count; ++k) std::printf("  %s %ld", k_ocls[k], o.decom_owner[k]);
    std::printf("\n  decommission events, window means (8 rows before) by sink class:\n");
    for (int k = 0; k < s_count; ++k)
    {
        const auto& e = o.ev[k];
        if (!(e[0] > 0)) continue;
        const double n = e[0];
        std::printf("    %-30s n %4.0f | out %.2f rev %.2f basket %.2f maint %.2f wages %.2f levy %.2f | supply %.2f target %.0f | px/base %.2f best-on-body %.2f\n",
                    k_sink[k], n, e[1] / n, e[2] / n, e[9] / n, e[3] / n, e[4] / n, e[5] / n, e[6] / n, e[10] / n, e[7] / n, e[8] / n);
    }
}

void add_out(seed_out& a, const seed_out& b)
{
    a.uc_handoff += b.uc_handoff; a.uc_handoff_done += b.uc_handoff_done;
    a.uc_started_live += b.uc_started_live; a.uc_started_live_done += b.uc_started_live_done;
    a.built_live += b.built_live; a.built_on_idle_tile += b.built_on_idle_tile; a.resumes += b.resumes;
    a.decom_basket_ok_events += b.decom_basket_ok_events;
    for (int g = 0; g < g_count; ++g) a.gone[g] += b.gone[g];
    for (int d = 0; d < d_count; ++d) a.decom_events[d] += b.decom_events[d];
    for (int k = 0; k < s_count; ++k) { a.decom_sink_events[k] += b.decom_sink_events[k]; for (int j = 0; j < 11; ++j) a.ev[k][j] += b.ev[k][j]; }
    for (int k = 0; k < p_count; ++k) a.decom_price_events[k] += b.decom_price_events[k];
    for (int c = 0; c < u_count; ++c) a.uc_tick_cls[c] += b.uc_tick_cls[c];
    for (int k = 0; k < o_count; ++k) a.decom_owner[k] += b.decom_owner[k];
    a.stretch_rate += b.stretch_rate; a.stretch_n += b.stretch_n;
    for (const auto& [g, n] : b.pause_good) a.pause_good[g] += n;
}

void run_seed(std::uint32_t seed, int ticks, int examples, bool dump, seed_out& out)
{
    lua_state lua;
    world_params wp;
    wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    try { build_app_start_world(lua, wp, *start); }
    catch (const std::exception& e) { std::printf("seed %u: build threw %s\n", seed, e.what()); return; }
    world& w = start->w;
    const recipe_registry& reg = start->reg;
    std::printf("\n=== seed %u ===\n", seed);

    std::map<entity_id, site> sites;
    std::map<entity_id, std::set<entity_id>> tile_hist;
    probe p;
    p.reg = &reg; p.sites = &sites; p.tile_history = &tile_hist; p.out = &out;
    for (int i = 0; i < k_campaign_settle_lap_count; ++i)
    {
        if (!std::strcmp(k_campaign_settle_lap_names[i], "convoys")) p.lap_conv = i;
        if (!std::strcmp(k_campaign_settle_lap_names[i], "run_economy_step")) p.lap_econ = i;
    }
    p.tick = -k_campaign_settle_ticks;
    scan_sites(w, p, p.tick, true);
    settle_tick_hooks hooks;
    hooks.after_lap = after_lap;
    hooks.ctx = &p;
    for (int step = 0; step < k_campaign_settle_ticks; ++step)
    {
        p.tick = step - k_campaign_settle_ticks;     // -12 .. -1
        settle_tick_result res = run_settle_tick(w, reg, step, 0, true, &hooks);
        after_tick(w, reg, res, p, p.tick);
    }
    // seat: its sites under construction are demolished (clean slate)
    seat_player_corporation(w, seed, start->land.search.winner_score);
    for (auto& [bid, s] : sites)
        if (s.gone_tick == INT_MIN && !w.buildings.count(bid))
        {
            s.gone_tick = 0; s.gone_by = g_seat; ++out.gone[g_seat]; tile_hist[s.tile].insert(bid);
            logf(s, "t0 GONE (seat clean slate)");
        }
    // handoff read
    for (const auto& [bid, s] : sites)
        if (s.gone_tick == INT_MIN && w.buildings.count(bid) && w.buildings.at(bid).ticks_remaining > 0) ++out.uc_handoff;
    std::set<entity_id> uc_at_handoff;
    for (const auto& [bid, s] : sites)
        if (s.gone_tick == INT_MIN && w.buildings.count(bid) && w.buildings.at(bid).ticks_remaining > 0) uc_at_handoff.insert(bid);
    take_read(w, reg, p, 0, 0);

    constexpr int k_days = 90;
    for (int k = 1; k <= ticks; ++k)
    {
        advance_orbits(w, static_cast<double>(k_days));
        advance_surveys(w, k_days);
        w.current_day_tick = k * k_days;
        p.tick = k;
        settle_tick_result res = run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), k * k_days, false, &hooks);
        after_tick(w, reg, res, p, k);
        for (int ri = 1; ri < k_nreads; ++ri) if (k_read_ticks[ri] == k) take_read(w, reg, p, ri, k);
        std::fflush(stdout);
    }
    for (const entity_id bid : uc_at_handoff) if (sites.at(bid).completed != INT_MIN) ++out.uc_handoff_done;
    for (const auto& [bid, s] : sites)
        if (s.origin_tick != INT_MIN && s.origin_tick >= 1 && s.uc_at_first && s.completed != INT_MIN) ++out.uc_started_live_done;

    for (int ri = 0; ri < k_nreads; ++ri)
    {
        char lbl[64];
        std::snprintf(lbl, sizeof lbl, "seed %u read t%d", seed, k_read_ticks[ri]);
        print_read(lbl, out.reads[ri]);
    }
    std::printf("  under construction at handoff %ld, completed by t%d: %ld | started in live play %ld, completed %ld\n",
                out.uc_handoff, ticks, out.uc_handoff_done, out.uc_started_live, out.uc_started_live_done);
    std::printf("  sites placed in live play %ld; placed on a tile with an earlier idled/gone focus site (all phases) %ld\n",
                out.built_live, out.built_on_idle_tile);
    std::printf("  gone:"); for (int g = 0; g < g_count; ++g) std::printf("  %s %ld", k_gby[g], out.gone[g]);
    std::printf("\n  decommission events in live play:"); for (int d = 0; d < d_count; ++d) std::printf("  %s %ld", k_dby[d], out.decom_events[d]);
    std::printf("\n    sank by:"); for (int k = 0; k < s_count; ++k) if (out.decom_sink_events[k]) std::printf("  %s %ld", k_sink[k], out.decom_sink_events[k]);
    std::printf("\n    price:"); for (int k = 0; k < p_count; ++k) if (out.decom_price_events[k]) std::printf("  %s %ld", k_pcls[k], out.decom_price_events[k]);
    std::printf("\n    basket at own prices covered costs: %ld; resumes %ld\n", out.decom_basket_ok_events, out.resumes);
    std::printf("  UC site-ticks in live play:"); for (int c = 0; c < u_count; ++c) if (out.uc_tick_cls[c]) std::printf("  %s %ld", k_ucls[c], out.uc_tick_cls[c]);
    std::printf("\n");
    print_events(out);

    // examples: the first few UC-at-handoff sites, decommissioned-in-play sites, gone sites
    int ex_uc = 0, ex_dc = 0, ex_pre = 0;
    for (const auto& [bid, s] : sites)
    {
        const bool uc = uc_at_handoff.count(bid) != 0;
        const bool dc = s.decom_tick != INT_MIN && s.decom_tick >= -12;
        const bool pre = s.decom_by == d_pregen;
        int* ctr = uc ? &ex_uc : dc ? &ex_dc : pre ? &ex_pre : nullptr;
        if (!dump || !(uc || dc || pre))
        {
            if (!ctr || *ctr >= examples) continue;
            ++*ctr;
        }
        std::printf("  EXAMPLE site %u tile %u (%s; focus mask %u)%s:\n", unsigned(bid), unsigned(s.tile),
                    uc ? "UC at handoff" : dc ? "decommissioned in settle/play" : "decommissioned at build", s.gmask, "");
        for (const std::string& l : s.log) std::printf("     %s\n", l.c_str());
    }
}

} // namespace

int main(int argc, char** argv)
{
    int ticks = 100, examples = 2;
    bool dump = false;
    std::vector<std::uint32_t> seeds = {0, 43, 10, 28, 38};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::max(1, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--examples") && i + 1 < argc) examples = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--dump")) dump = true;
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
        else { std::fprintf(stderr, "usage: idle_mines_probe [--seeds a,b] [--ticks N] [--examples N] [--dump]\n"); return 2; }
    }
    std::printf("idle_mines_probe - BL-1227: why silica / copper ore / rare earth sites sit under construction or decommissioned\n");
    std::array<read_tally, k_nreads> pooled{};
    seed_out tot;
    for (const std::uint32_t s : seeds)
    {
        seed_out o;
        run_seed(s, ticks, examples, dump, o);
        for (int r = 0; r < k_nreads; ++r) add(pooled[r], o.reads[r]);
        add_out(tot, o);
    }
    std::printf("\n================ POOLED over %zu seeds ================\n", seeds.size());
    read_tally all{};
    for (int r = 0; r < k_nreads; ++r)
    {
        char lbl[64];
        std::snprintf(lbl, sizeof lbl, "read t%d", k_read_ticks[r]);
        print_read(lbl, pooled[r]);
        add(all, pooled[r]);
    }
    print_read("ALL READS (t0+t10+t25+t50+t100)", all);
    // shares of idle site-reads
    long uc = 0, dc = 0;
    for (long v : all.uc_cls) uc += v;
    for (long v : all.dec_by) dc += v;
    const double I = double(uc + dc) > 0 ? double(uc + dc) : 1.0;
    std::printf("\n  IDLE SITE-READS %ld (uc %ld, decom %ld) - share by cause:\n", uc + dc, uc, dc);
    for (int c = 0; c < u_count; ++c) if (all.uc_cls[c]) std::printf("    uc %-12s %5.1f%%\n", k_ucls[c], 100.0 * all.uc_cls[c] / I);
    for (int d = 0; d < d_count; ++d) if (all.dec_by[d]) std::printf("    decom %-30s %5.1f%%\n", k_dby[d], 100.0 * all.dec_by[d] / I);
    std::printf("  under construction at handoff %ld, completed by t%d: %ld | started in live play %ld, completed %ld\n",
                tot.uc_handoff, ticks, tot.uc_handoff_done, tot.uc_started_live, tot.uc_started_live_done);
    std::printf("  placed in live play %ld; on a tile with an earlier idled/gone focus site %ld; resumes %ld\n",
                tot.built_live, tot.built_on_idle_tile, tot.resumes);
    std::printf("  gone:"); for (int g = 0; g < g_count; ++g) std::printf("  %s %ld", k_gby[g], tot.gone[g]);
    std::printf("\n  decommission events in live play:"); for (int d = 0; d < d_count; ++d) std::printf("  %s %ld", k_dby[d], tot.decom_events[d]);
    std::printf("\n    sank by:"); for (int k = 0; k < s_count; ++k) if (tot.decom_sink_events[k]) std::printf("  %s %ld", k_sink[k], tot.decom_sink_events[k]);
    std::printf("\n    price:"); for (int k = 0; k < p_count; ++k) if (tot.decom_price_events[k]) std::printf("  %s %ld", k_pcls[k], tot.decom_price_events[k]);
    std::printf("\n    basket at own prices covered costs: %ld\n", tot.decom_basket_ok_events);
    std::printf("  UC site-ticks in live play:"); for (int c = 0; c < u_count; ++c) if (tot.uc_tick_cls[c]) std::printf("  %s %ld", k_ucls[c], tot.uc_tick_cls[c]);
    std::printf("\n");
    print_events(tot);
    return 0;
}
