// ---------------------------------------------------------------------------
// handoff_starvation — BL-1207 (handoff starvation), a DIAGNOSIS instrument
// ---------------------------------------------------------------------------
// WHY THIS EXISTS. market_viability's G1 (processors running at the handoff)
// reads ~49% against a 70% target. G1 says HOW MANY are not running; this
// says WHY, per processor, before a lever is chosen: for every processor that
// is input-starved at the handoff, or decommissioned after its last reported
// row was input-starved, its recipe, its limiting input, and that input's
// status on the body.
//
// THE WORLD. market_viability's, exactly: `build_app_start_world`, the
// 12-tick settle through `run_settle_tick` (econ steps 0..11, day 0,
// spectating), then `seat_player_corporation`. The state classification is
// market_viability's `classify` term for term, so the totals here ARE its G1
// handoff row. No play ticks are run.
//
// TWO READING POINTS on the settle's LAST tick:
//   PRE-DRAW  after lap 0 ("convoys"), before run_economy_step: every market's
//             shelf and posted price, and every (corp, market) pool, as the
//             production pass will first see them (prices are the previous
//             clear's). The stock tests below read this snapshot.
//   HANDOFF   after the tick and the seat: the report rows (limiting input),
//             the buildings, and input_reach's two forms.
//
// THE LIMITING INPUT'S STATUS. For a starved processor at market C on body B
// whose limiting input is r, with `need` = its full-run per-tick need of r and
// `floor` = t_idle x need (the production tick's idle threshold):
//   over_ceiling  PRE-DRAW pool + shelf >= floor, pool alone < floor, and the
//                 shelf's posted price > reservation_mult x base (shelf_admits
//                 refuses it): stock was there, priced over the ceiling.
//   drawn_away    PRE-DRAW pool + admitted shelf >= floor, yet the row starved:
//                 earlier draws inside the tick (another processor's, upkeep,
//                 construction) took it first.
//   never_chart   no building on B in ANY state makes r (a recipe with r as an
//                 output; an extraction site targeting r, or with an
//                 extractable r deposit on its tile): never chartered on B.
//                 `elsewhere` counts whether another body has one.
//   not_standing  makers of r exist on B but none is standing (all
//                 decommissioned, under construction or unlaboured).
//   makers_idle   standing makers on B, none produced r this tick (the chain
//                 one rung up is itself starved / idle).
//   own_mkt       a maker in C produced r this tick, but it did not reach this
//                 processor's coverage (it sits in the maker's pool, or was
//                 drawn by the maker's own corp / other draws first).
//                 `others_draw` beside it: other standing consumers' nominal
//                 draw of r in C against C's actual output.
//   reach_play    a maker producing r this tick stands in a market P != C that
//                 is WITHIN REACH of C in the PLAY form (input_reach with the
//                 handoff report: the dispatcher's own gate on resolved prices),
//                 and none of it landed.
//   reach_gen     within reach only in the GENERATION form (input_reach without
//                 a report: reservation_mult x base_C - haul > (1+margin) x
//                 base_P) — the lane placement assumed, which the live prices
//                 do not open.
//   no_reach      makers of r produced it this tick on B, but no producing
//                 market is within reach of C in either form.
// The PRIMARY bucket is the first that holds, in that order. Every flag is
// also counted independently (they overlap), and `obtainable` is answered in
// both forms through input_obtainable (the scorer's own question).
// `sole short` = the limiting input is the ONLY input under the floor at
// PRE-DRAW (pool + admitted shelf) — the processors a fix to r alone frees.
//
// HISTORY (the economy-rows-must-be-multi-tick rule): the PRE-DRAW posted price
// of every market is kept for each of the 12 settle ticks, and every processor's
// count of settle ticks it produced output. Per analysed processor the probe
// prints `ran_settle` (ticks of 12 it ran) and `over_ticks` (ticks of 12 its
// limiting input's pre-draw price at C sat over reservation_mult x base), so a
// one-tick snapshot is told from a chronic state.
//
// THE TABLES. T1 state by provenance (gen / scorer); T2 the buckets, with the
// settle-history line under each; T3 the limiting inputs; T4 the recipes; T5
// the (seed, market, input) holes with the market's pre-draw shelf, this tick's
// output and standing draw of r, and its post-clear demand / supply / household
// bid; then the holes pooled per leading bucket.
//
// PROVENANCE. A processor present when the world is handed to the settle was
// placed by generation; one that appears during the settle was built by the
// scorer.
//
// A PURE READER. Nothing written is read by the simulation; input_reach warms
// the logistics path caches only, after the handoff, and no tick follows.
//
// Usage (repo root):
//   build_gen/verify/handoff_starvation.exe [--seeds a,b] [--top N] [--list] [--k X]
//   --list   one line per starved / idled-starving processor
// Default seeds: docs/generation/seed_library.json, in library order.
// Build:  bash tools/verify/build_lua_harness.sh handoff_starvation
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/input_reach.hpp"
#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/spawn_seat.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace {

float g_res_mult = 0.0f;
std::string rn(std::size_t r) { return resource_names::name_of(static_cast<resource_type>(r)); }

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

// --- market_viability's classification, term for term ------------------------

enum proc_state { ps_run = 0, ps_input, ps_nolab, ps_unsup, ps_decom, ps_build, ps_other, ps_count };
const char* k_state_name[ps_count] = {"run", "input", "nolab", "unsupplied", "decom", "build", "other"};

proc_state classify_row(const building_component& b, const building_report* rep, const recipe_registry& reg)
{
    if (reg.get_recipe(b.recipe) == nullptr)  return ps_other;
    if (rep == nullptr)                       return ps_other;
    if (rep->active)                          return ps_run;
    if (rep->effective_workforce <= 0.0f)     return ps_nolab;
    if (building_supply_scalar(b) <= 0.0f || b.workforce_target <= 0.0f) return ps_unsup;
    if (rep->has_limiting)                    return ps_input;
    return ps_other;
}

proc_state classify(const building_component& b, const building_report* rep, const recipe_registry& reg)
{
    if (b.ticks_remaining > 0) return ps_build;
    if (b.decommissioned)      return ps_decom;
    return classify_row(b, rep, reg);
}

const building_report* row_of(const economy_report& rep, entity_id bid)
{
    const auto it = rep.building_row.find(bid);
    return it == rep.building_row.end() ? nullptr : &rep.buildings[it->second];
}

/// Last reported row per processor: its state, limiting input and workforce.
struct last_row
{
    proc_state    state = ps_other;
    bool          has_limiting = false;
    std::size_t   limiting = 0;
    float         eff = 0.0f;
    std::uint16_t recipe = no_recipe;
};

// --- the PRE-DRAW snapshot (after lap 0 of the settle's last tick) -------------

struct predraw
{
    int  lap  = -1;
    bool armed = false;
    bool taken = false;
    std::map<entity_id, std::array<float, resource_count>> inv, price;
    std::map<std::pair<entity_id, entity_id>, std::array<float, resource_count>> pool;
};

void snap_after_lap(const world& w, int lap, void* ctx)
{
    auto* s = static_cast<predraw*>(ctx);
    if (!s->armed || lap != s->lap) return;
    s->inv.clear(); s->price.clear(); s->pool.clear();
    for (const auto& [mid, mc] : w.markets)
    {
        s->inv[mid] = mc.inventory;
        std::array<float, resource_count> p{};
        for (std::size_t r = 0; r < resource_count; ++r) p[r] = posted_price(mc, r);
        s->price[mid] = p;
    }
    // BL-1265: corporations hold no pools; `pool` stays empty (every pool read is 0).
    s->taken = true;
}

// --- buckets ----------------------------------------------------------------------

enum bucket { b_ceiling = 0, b_drawn, b_never, b_notstand, b_makers_idle, b_own_mkt,
              b_reach_play, b_reach_gen, b_no_reach, b_count };
const char* k_bucket_name[b_count] = {"over_ceiling", "drawn_away", "never_chart", "not_standing",
                                      "makers_idle", "own_mkt", "reach_play", "reach_gen", "no_reach"};

struct starved_rec
{
    std::uint32_t seed = 0;
    entity_id     bid = null_entity, market = null_entity, body = null_entity, corp = null_entity;
    bool          gen = false, decom = false, home = false, seat = false;
    std::string   recipe;
    std::size_t   r = 0;
    float         need = 0.0f, floor = 0.0f;
    float         pre_pool = 0.0f, pre_shelf = 0.0f, price_over_base = 0.0f;
    bool          flag[b_count] = {};
    bool          elsewhere = false;      ///< another body has a maker of r (any state)
    bool          obt_gen = false, obt_play = false;
    bool          sole = false;           ///< the only input under the floor at PRE-DRAW
    int           ran_settle = 0;         ///< settle ticks (of 12) the processor produced output
    int           over_ticks = 0;         ///< settle ticks r's PRE-DRAW posted price at C sat over the ceiling
    int           short_inputs = 0;
    float         own_out = 0.0f, others_draw = 0.0f; ///< C's actual output of r; other consumers' nominal draw in C
    float         mkt_draw = 0.0f;        ///< every standing consumer's nominal draw of r in C
    float         h_demand = 0.0f, h_supply = 0.0f, h_hh_bid = 0.0f; ///< C's demand / supply / household bid of r, post-clear
    bucket        primary = b_count;
};

struct seed_out
{
    std::uint32_t seed = 0;
    std::string   fail;
    int n[2][ps_count] = {};          ///< [gen=0 / scorer=1][state]
    int decom_after_input[2] = {};    ///< decom whose last row was input-starved
    std::vector<starved_rec> recs;
};

float labour_of(const building_component& b, float eff)
{
    return eff * std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f) * building_supply_scalar(b);
}

/// Does building @p b make resource @p r in ANY state (chartered, standing or not)?
bool makes_any_state(const world& w, const recipe_registry& reg, const building_component& b, std::size_t r)
{
    if (b.type == building_type::processing_facility)
    {
        const recipe* rc = reg.get_recipe(b.recipe);
        return rc != nullptr && rc->outputs[r] > 0.0f;
    }
    if (b.type == building_type::extraction_site)
    {
        if (static_cast<std::size_t>(b.target_resource) == r) return true;
        const auto tit = w.tiles.find(b.tile);
        if (tit == w.tiles.end()) return false;
        if (placement_rules::is_well_site(w, b.tile, b.target_resource)) return false;
        bool extractable = false;
        for (const resource_type x : placement_rules::k_extractable)
            if (static_cast<std::size_t>(x) == r) extractable = true;
        return extractable && tit->second.resource_deposit[r] > 0.0f;
    }
    return false;
}

bool standing_b(const building_component& b)
{
    return !b.decommissioned && b.ticks_remaining <= 0
        && b.workforce_assigned * std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f) > 0.0f;
}

entity_id body_of(const world& w, const building_component& b)
{
    const auto it = w.tiles.find(b.tile);
    return it == w.tiles.end() ? null_entity : it->second.body;
}

/// BL-1209: price_band.shelf_supply_ticks override (market_viability's --k); < 0 = shipped.
float g_k_override = -1.0f;

void run_seed(std::uint32_t seed, seed_out& out)
{
    out.seed = seed;
    lua_state lua;
    world_params p;
    p.seed = seed;
    auto start = std::make_unique<app_start_world>();
    try { build_app_start_world(lua, p, *start); }
    catch (const std::exception& e) { out.fail = std::string("world build threw: ") + e.what(); return; }
    world& w = start->w;
    if (g_k_override >= 0.0f)
    {
        // BL-1209: market_viability's --k, applied the same way (before the settle).
        price_band_params pb = start->reg.price_band();
        pb.shelf_supply_ticks = g_k_override;
        start->reg.set_price_band(pb);
    }
    const recipe_registry& reg = start->reg;
    if (w.corporations.empty()) { out.fail = "no corporations"; return; }

    std::set<entity_id> gen_ids;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::processing_facility) gen_ids.insert(bid);

    predraw snap;
    for (int i = 0; i < k_campaign_settle_lap_count; ++i)
        if (std::strcmp(k_campaign_settle_lap_names[i], "convoys") == 0) snap.lap = i;
    settle_tick_hooks hooks;
    hooks.after_lap = snap_after_lap;
    hooks.ctx = &snap;

    std::map<entity_id, last_row> last;
    std::map<entity_id, int> ran_ticks;   ///< settle ticks each processor produced output
    std::vector<std::map<entity_id, std::array<float, resource_count>>> price_hist; ///< PRE-DRAW posted price per settle tick
    economy_report rep;
    for (int step = 0; step < k_campaign_settle_ticks; ++step)
    {
        snap.armed = true;
        settle_tick_result res = run_settle_tick(w, reg, step, 0, true, &hooks);
        price_hist.push_back(snap.price);
        for (const building_report& br : res.report.buildings)
        {
            if (br.type != building_type::processing_facility) continue;
            const auto bi = w.buildings.find(br.building);
            if (bi == w.buildings.end()) continue;
            if (br.active) ++ran_ticks[br.building];
            last_row& lr = last[br.building];
            lr.state = classify_row(bi->second, &br, reg);
            lr.has_limiting = br.has_limiting;
            lr.limiting = static_cast<std::size_t>(br.limiting_input);
            lr.eff = br.effective_workforce;
            lr.recipe = br.recipe;
        }
        if (step == k_campaign_settle_ticks - 1) rep = std::move(res.report);
    }
    if (!snap.taken) { out.fail = "pre-draw snapshot not taken"; return; }

    const spawn_seat_result seat = seat_player_corporation(w, seed, start->land.search.winner_score);

    // Owner per building (sorted corp walk).
    std::map<entity_id, entity_id> owner;
    {
        std::vector<entity_id> cids;
        for (const auto& kv : w.corporations) cids.push_back(kv.first);
        std::sort(cids.begin(), cids.end());
        for (const entity_id c : cids)
            for (const entity_id a : w.corporations.at(c).assets) owner[a] = c;
    }

    // Sorted building ids, the per-building market, and the makers index.
    std::vector<entity_id> ids;
    for (const auto& kv : w.buildings) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    std::map<entity_id, entity_id> mkt_of;
    for (const entity_id bid : ids) mkt_of[bid] = market_for_tile(w, w.buildings.at(bid).tile);

    input_reach ir_gen  = make_input_reach(w, reg);
    input_reach ir_play = make_input_reach(w, reg);
    ir_play.report = &rep;
    const float t_idle = reg.t_idle();
    const float res_mult = reg.price_band().reservation_mult;
    g_res_mult = res_mult;
    const float base_rate = reg.economics(building_type::processing_facility).base_rate;

    // Per (body, r): makers any state / standing / producing; per (market, r): actual output; draws.
    struct body_r { int any = 0, standing = 0, producing = 0; };
    std::map<std::pair<entity_id, std::size_t>, body_r> body_makers;
    std::map<std::pair<entity_id, std::size_t>, float> mkt_out, mkt_draw;
    std::map<std::size_t, std::set<entity_id>> bodies_with_maker;
    // producing markets per (body, r), for reach
    std::map<std::pair<entity_id, std::size_t>, std::set<entity_id>> producing_mkts, standing_mkts;
    for (const entity_id bid : ids)
    {
        const building_component& b = w.buildings.at(bid);
        if (b.type != building_type::processing_facility && b.type != building_type::extraction_site) continue;
        const entity_id B = body_of(w, b);
        const entity_id M = mkt_of[bid];
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (makes_any_state(w, reg, b, r))
            {
                body_r& br = body_makers[{B, r}];
                ++br.any;
                bodies_with_maker[r].insert(B);
                if (standing_b(b) && building_produces(w, reg, b, r))
                {
                    ++br.standing;
                    standing_mkts[{B, r}].insert(M);
                }
                const float o = building_output(w, reg, bid, b, r, &rep);
                if (o > 0.0f)
                {
                    ++br.producing;
                    mkt_out[{M, r}] += o;
                    producing_mkts[{B, r}].insert(M);
                }
            }
            const float d = building_draw(reg, b, r);
            if (d > 0.0f) mkt_draw[{M, r}] += d;
        }
    }

    for (const entity_id bid : ids)
    {
        const building_component& b = w.buildings.at(bid);
        if (b.type != building_type::processing_facility) continue;
        const int prov = gen_ids.count(bid) ? 0 : 1;
        const building_report* row = row_of(rep, bid);
        const proc_state s = classify(b, row, reg);
        ++out.n[prov][s];

        // Which processors are analysed: input-starved now, or decommissioned with
        // an input-starved last row.
        bool analyse = false;
        std::size_t r = 0;
        float eff = 0.0f;
        if (s == ps_input && row != nullptr)
        {
            analyse = true; r = static_cast<std::size_t>(row->limiting_input); eff = row->effective_workforce;
        }
        else if (s == ps_decom)
        {
            const auto it = last.find(bid);
            if (it != last.end() && it->second.state == ps_input && it->second.has_limiting)
            {
                ++out.decom_after_input[prov];
                analyse = true; r = it->second.limiting; eff = it->second.eff > 0.0f ? it->second.eff : b.workforce_assigned;
            }
        }
        if (!analyse) continue;

        const recipe* rc = reg.get_recipe(b.recipe);
        if (rc == nullptr) continue;
        starved_rec sr;
        sr.seed = seed; sr.bid = bid; sr.gen = prov == 0; sr.decom = (s == ps_decom);
        sr.market = mkt_of[bid]; sr.body = body_of(w, b);
        sr.home = sr.body == w.home_body;
        sr.corp = owner.count(bid) ? owner[bid] : null_entity;
        sr.seat = sr.corp != null_entity && sr.corp == seat.seated;
        sr.recipe = rc->name;
        sr.r = r;
        const float batches = base_rate * labour_of(b, eff);
        sr.need  = rc->inputs[r] * batches;
        sr.floor = t_idle * sr.need;
        const entity_id C = sr.market;
        const auto mit = w.markets.find(C);

        // PRE-DRAW stock tests, every input (sole-short), and the limiting one's flags.
        const auto pit = snap.pool.find({sr.corp, C});
        const auto iit = snap.inv.find(C);
        const auto prt = snap.price.find(C);
        auto pre_cov = [&](std::size_t x, bool& admitted, float& pool_q, float& shelf_q, float& pr_base) {
            pool_q  = (pit != snap.pool.end()) ? std::max(0.0f, pit->second[x]) : 0.0f;
            shelf_q = (iit != snap.inv.end()) ? std::max(0.0f, iit->second[x]) : 0.0f;
            const float base = mit != w.markets.end() ? mit->second.base_price[x] : 0.0f;
            const float price = (prt != snap.price.end()) ? prt->second[x] : 0.0f;
            pr_base = base > 0.0f ? price / base : 0.0f;
            admitted = res_mult <= 0.0f ? true : (base > 0.0f && price <= base * res_mult);
            return pool_q + (admitted ? shelf_q : 0.0f);
        };
        for (std::size_t x = 0; x < resource_count; ++x)
        {
            if (!(rc->inputs[x] > 0.0f)) continue;
            bool adm; float pq, sq, pb;
            const float avail = pre_cov(x, adm, pq, sq, pb);
            if (avail < t_idle * rc->inputs[x] * batches) ++sr.short_inputs;
        }
        bool adm; float pq, sq, pb;
        const float avail = pre_cov(r, adm, pq, sq, pb);
        sr.pre_pool = pq; sr.pre_shelf = sq; sr.price_over_base = pb;
        sr.sole = sr.short_inputs <= 1 && avail < sr.floor;
        sr.flag[b_ceiling] = !adm && pq < sr.floor && pq + sq >= sr.floor;
        if (const auto rt = ran_ticks.find(bid); rt != ran_ticks.end()) sr.ran_settle = rt->second;
        if (mit != w.markets.end() && res_mult > 0.0f)
            for (const auto& ph : price_hist)
                if (const auto pm = ph.find(C); pm != ph.end()
                    && pm->second[r] > res_mult * mit->second.base_price[r])
                    ++sr.over_ticks;
        sr.flag[b_drawn]   = avail >= sr.floor;

        const auto bm = body_makers.find({sr.body, r});
        const body_r brs = bm == body_makers.end() ? body_r{} : bm->second;
        sr.flag[b_never]       = brs.any == 0;
        sr.flag[b_notstand]    = brs.any > 0 && brs.standing == 0;
        sr.flag[b_makers_idle] = brs.standing > 0 && brs.producing == 0;
        for (const entity_id ob : bodies_with_maker[r]) if (ob != sr.body) sr.elsewhere = true;

        const auto mo = mkt_out.find({C, r});
        sr.own_out = mo == mkt_out.end() ? 0.0f : mo->second;
        const auto md = mkt_draw.find({C, r});
        sr.mkt_draw = md == mkt_draw.end() ? 0.0f : md->second;
        if (mit != w.markets.end())
        {
            sr.h_demand = mit->second.demand[r];
            sr.h_supply = mit->second.supply[r];
            sr.h_hh_bid = mit->second.household_bid[r];
        }
        sr.others_draw = sr.mkt_draw - building_draw(reg, b, r);
        sr.flag[b_own_mkt] = sr.own_out > 0.0f;

        bool rp = false, rg = false, any_other = false;
        if (const auto pm = producing_mkts.find({sr.body, r}); pm != producing_mkts.end())
            for (const entity_id P : pm->second)
            {
                if (P == C || P == null_entity) continue;
                any_other = true;
                if (!rp && market_within_reach(w, reg, ir_play, P, C, r)) rp = true;
                if (!rg && market_within_reach(w, reg, ir_gen, P, C, r)) rg = true;
            }
        // The generation form also reaches over STANDING makers (nominal), producing or not.
        if (!rg)
            if (const auto sm = standing_mkts.find({sr.body, r}); sm != standing_mkts.end())
                for (const entity_id P : sm->second)
                    if (P != C && P != null_entity && market_within_reach(w, reg, ir_gen, P, C, r)) { rg = true; break; }
        sr.flag[b_reach_play] = rp;
        sr.flag[b_reach_gen]  = rg && !rp;
        sr.flag[b_no_reach]   = any_other && !rp && !rg && !sr.flag[b_own_mkt];

        const stockpile_component* pool = nullptr; // BL-1265: no pools
        sr.obt_gen  = input_obtainable(w, reg, ir_gen, C, pool, r, sr.need, bid).obtainable;
        sr.obt_play = input_obtainable(w, reg, ir_play, C, pool, r, sr.need, bid).obtainable;

        for (int k = 0; k < b_count; ++k)
            if (sr.flag[k]) { sr.primary = static_cast<bucket>(k); break; }
        if (sr.primary == b_count) sr.primary = b_no_reach; // makers produce on B, none in C, none reachable
        out.recs.push_back(std::move(sr));
    }
}

double pct(double a, double b) { return b > 0 ? 100.0 * a / b : 0.0; }

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    bool from_args = false, list = false;
    int top = 15;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) { seeds = parse_seed_list(argv[++i]); from_args = true; }
        else if (!std::strcmp(argv[i], "--top") && i + 1 < argc) top = std::max(1, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--list")) list = true;
        else if (!std::strcmp(argv[i], "--k") && i + 1 < argc) g_k_override = static_cast<float>(std::atof(argv[++i]));
        else { std::fprintf(stderr, "usage: handoff_starvation [--seeds a,b] [--top N] [--list] [--k X]\n"); return 2; }
    }
    if (!from_args)
    {
        seeds = library_seeds("docs/generation/seed_library.json");
        if (seeds.empty()) { std::printf("FATAL  seed library not found (run from the repo root)\n"); return 1; }
    }
    std::printf("handoff_starvation — BL-1207 (handoff starvation)\nseeds (%zu):", seeds.size());
    for (const std::uint32_t s : seeds) std::printf(" %u", s);
    std::printf("\n");
    std::fflush(stdout);

    std::vector<seed_out> outs;
    int fails = 0;
    for (const std::uint32_t seed : seeds)
    {
        outs.emplace_back();
        seed_out& o = outs.back();
        run_seed(seed, o);
        if (!o.fail.empty()) { ++fails; std::printf("seed %u FAILED: %s\n", seed, o.fail.c_str()); std::fflush(stdout); continue; }
        int built = 0, run = 0;
        for (int g = 0; g < 2; ++g)
            for (int s = 0; s < ps_count; ++s) { if (s != ps_build) built += o.n[g][s]; if (s == ps_run) run += o.n[g][s]; }
        std::printf("seed %5u  built %4d run %4d (%5.1f%%) | gen:", seed, built, run, pct(run, built));
        for (int s = 0; s < ps_count; ++s) std::printf(" %s %d", k_state_name[s], o.n[0][s]);
        std::printf(" | scorer:");
        for (int s = 0; s < ps_count; ++s) std::printf(" %s %d", k_state_name[s], o.n[1][s]);
        std::printf(" | decom-after-input gen %d scorer %d | analysed %zu\n",
                    o.decom_after_input[0], o.decom_after_input[1], o.recs.size());
        std::fflush(stdout);
    }

    // --- T1 states by provenance ---
    int n[2][ps_count] = {}, dai[2] = {};
    for (const seed_out& o : outs)
        for (int g = 0; g < 2; ++g) { for (int s = 0; s < ps_count; ++s) n[g][s] += o.n[g][s]; dai[g] += o.decom_after_input[g]; }
    std::printf("\nT1 STATE AT HANDOFF, pooled (G1 denominator = all but build)\n");
    std::printf("  %-8s %6s %6s %6s %6s %6s %6s %6s %6s | %6s %7s %s\n", "prov", "run", "input", "nolab", "unsup",
                "decom", "other", "build", "total", "built", "run%", "decom-after-input");
    for (int g = 0; g <= 2; ++g)
    {
        int row[ps_count] = {}; int d = 0;
        for (int s = 0; s < ps_count; ++s) row[s] = g < 2 ? n[g][s] : n[0][s] + n[1][s];
        d = g < 2 ? dai[g] : dai[0] + dai[1];
        int tot = 0; for (int s = 0; s < ps_count; ++s) tot += row[s];
        const int built = tot - row[ps_build];
        std::printf("  %-8s %6d %6d %6d %6d %6d %6d %6d %6d | %6d %6.1f%% %d\n", g == 0 ? "gen" : g == 1 ? "scorer" : "all",
                    row[ps_run], row[ps_input], row[ps_nolab], row[ps_unsup], row[ps_decom], row[ps_other],
                    row[ps_build], tot, built, pct(row[ps_run], built), d);
    }

    // --- T2 buckets ---
    std::vector<const starved_rec*> all;
    for (const seed_out& o : outs) for (const starved_rec& r : o.recs) all.push_back(&r);
    std::printf("\nT2 LIMITING-INPUT STATUS, %zu analysed (input-starved + decom-after-input-starved)\n", all.size());
    std::printf("  %-13s %8s %6s %6s %6s %6s | %8s | %6s %6s\n", "bucket", "primary", "gen", "scorer", "starv", "decom",
                "flag(any)", "sole", "home");
    int prim_tot[b_count] = {};
    for (int k = 0; k < b_count; ++k)
    {
        int pr = 0, gg = 0, sc = 0, st = 0, de = 0, fl = 0, so = 0, ho = 0;
        for (const starved_rec* s : all)
        {
            if (s->flag[k]) ++fl;
            if (s->primary != k) continue;
            ++pr; (s->gen ? gg : sc)++; (s->decom ? de : st)++; if (s->sole) ++so; if (s->home) ++ho;
        }
        prim_tot[k] = pr;
        std::printf("  %-13s %8d %6d %6d %6d %6d | %8d | %6d %6d\n", k_bucket_name[k], pr, gg, sc, st, de, fl, so, ho);
        if (pr)
        {
            int ran = 0, ran0 = 0; int oh[4] = {}; // over-ceiling settle ticks: 0, 1-3, 4-8, 9-12
            for (const starved_rec* s : all)
            {
                if (s->primary != k) continue;
                ran += s->ran_settle; if (s->ran_settle == 0) ++ran0;
                ++oh[s->over_ticks == 0 ? 0 : s->over_ticks <= 3 ? 1 : s->over_ticks <= 8 ? 2 : 3];
            }
            std::printf("  %-13s   settle ticks run: mean %.1f of %d, never %d | r over the ceiling at pre-draw, settle ticks: 0 %d, 1-3 %d, 4-8 %d, 9-12 %d\n",
                        "", static_cast<double>(ran) / pr, k_campaign_settle_ticks, ran0, oh[0], oh[1], oh[2], oh[3]);
        }
    }
    {
        int og = 0, op = 0, sole = 0, never_else = 0, never = 0;
        double pb = 0; int npb = 0;
        for (const starved_rec* s : all)
        {
            if (s->obt_gen) ++og; if (s->obt_play) ++op; if (s->sole) ++sole;
            if (s->primary == b_never) { ++never; if (s->elsewhere) ++never_else; }
            if (s->primary == b_ceiling) { pb += s->price_over_base; ++npb; }
        }
        int none_short = 0, none_short_st = 0;
        for (const starved_rec* s : all) if (s->short_inputs == 0) { ++none_short; if (!s->decom) ++none_short_st; }
        std::printf("  obtainable (input_obtainable) gen form %d, play form %d | sole short input %d of %zu"
                    " | NO input short at pre-draw %d (%d starved now, %d decom)\n",
                    og, op, sole, all.size(), none_short, none_short_st, none_short - none_short_st);
        std::printf("  never_chart with a maker on another body: %d of %d | over_ceiling mean price/base %.2f (reservation %.2f)\n",
                    never_else, never, npb ? pb / npb : 0.0, static_cast<double>(g_res_mult));
    }

    // --- T3 top limiting inputs ---
    std::map<std::size_t, std::array<int, b_count + 3>> by_r; // [buckets..., total, sole, scorer]
    for (const starved_rec* s : all)
    {
        auto& a = by_r[s->r];
        ++a[s->primary]; ++a[b_count]; if (s->sole) ++a[b_count + 1]; if (!s->gen) ++a[b_count + 2];
    }
    std::vector<std::pair<int, std::size_t>> rk;
    for (const auto& [r, a] : by_r) rk.push_back({-a[b_count], r});
    std::sort(rk.begin(), rk.end());
    std::printf("\nT3 TOP LIMITING INPUTS (count of analysed processors)\n  %-16s %5s %5s %6s |", "input", "n", "sole", "scorer");
    for (int k = 0; k < b_count; ++k) std::printf(" %12s", k_bucket_name[k]);
    std::printf("\n");
    for (std::size_t i = 0; i < rk.size() && static_cast<int>(i) < top; ++i)
    {
        const auto& a = by_r[rk[i].second];
        std::printf("  %-16s %5d %5d %6d |", rn(rk[i].second).c_str(), a[b_count], a[b_count + 1], a[b_count + 2]);
        for (int k = 0; k < b_count; ++k) std::printf(" %12d", a[k]);
        std::printf("\n");
    }

    // --- T4 top recipes ---
    std::map<std::string, std::pair<int, std::map<std::size_t, int>>> by_rc;
    for (const starved_rec* s : all) { auto& e = by_rc[s->recipe]; ++e.first; ++e.second[s->r]; }
    std::vector<std::pair<int, std::string>> rck;
    for (const auto& [name, e] : by_rc) rck.push_back({-e.first, name});
    std::sort(rck.begin(), rck.end());
    std::printf("\nT4 TOP RECIPES among the analysed (limiting inputs)\n");
    for (std::size_t i = 0; i < rck.size() && static_cast<int>(i) < top; ++i)
    {
        const auto& e = by_rc[rck[i].second];
        std::printf("  %-26s %4d :", rck[i].second.c_str(), e.first);
        for (const auto& [r, c] : e.second) std::printf(" %s %d", rn(r).c_str(), c);
        std::printf("\n");
    }

    // --- T5 top (seed, market, input) holes ---
    std::map<std::tuple<std::uint32_t, entity_id, std::size_t>, std::pair<int, std::array<int, b_count>>> holes;
    std::map<std::tuple<std::uint32_t, entity_id, std::size_t>, bool> hole_home;
    std::map<std::tuple<std::uint32_t, entity_id, std::size_t>, const starved_rec*> hole_rep;
    for (const starved_rec* s : all)
    {
        auto& h = holes[{s->seed, s->market, s->r}];
        ++h.first; ++h.second[s->primary];
        hole_home[{s->seed, s->market, s->r}] = s->home;
        hole_rep[{s->seed, s->market, s->r}] = s;
    }
    std::vector<std::pair<int, std::tuple<std::uint32_t, entity_id, std::size_t>>> hk;
    for (const auto& [k, h] : holes) hk.push_back({-h.first, k});
    std::sort(hk.begin(), hk.end());
    std::printf("\nT5 TOP (seed, market, input) HOLES (%zu distinct)\n", holes.size());
    for (std::size_t i = 0; i < hk.size() && static_cast<int>(i) < top; ++i)
    {
        const auto& [seed, m, r] = hk[i].second;
        const auto& h = holes[hk[i].second];
        std::printf("  seed %5u market %6llu%s %-16s %3d :", seed, static_cast<unsigned long long>(m),
                    hole_home[hk[i].second] ? " (home)" : "       ", rn(r).c_str(), h.first);
        for (int k = 0; k < b_count; ++k) if (h.second[k]) std::printf(" %s %d", k_bucket_name[k], h.second[k]);
        const starved_rec* x = hole_rep[hk[i].second];
        std::printf("  | pre-draw shelf %.1f p/base %.2f, C output %.1f, C standing draw %.1f;"
                    " handoff demand %.1f supply %.1f household bid %.1f\n",
                    x->pre_shelf, x->price_over_base, x->own_out, x->mkt_draw, x->h_demand, x->h_supply, x->h_hh_bid);
    }
    {
        int one = 0, two_plus = 0, in_multi = 0;
        for (const auto& [k, h] : holes) { if (h.first == 1) ++one; else { ++two_plus; in_multi += h.first; } }
        std::printf("  holes holding 1 processor: %d; holding 2+: %d (covering %d processors)\n", one, two_plus, in_multi);
        // Per primary bucket, over the holes it dominates: the market's pre-draw
        // shelf and this tick's output of r against the standing draw of r there.
        for (int k = 0; k < b_count; ++k)
        {
            int nh = 0, np = 0; double sh = 0, ou = 0, dr = 0;
            for (const auto& [key, h] : holes)
            {
                int best = 0; for (int j = 1; j < b_count; ++j) if (h.second[j] > h.second[best]) best = j;
                if (best != k) continue;
                const starved_rec* x = hole_rep.at(key);
                ++nh; np += h.first; sh += x->pre_shelf; ou += x->own_out; dr += x->mkt_draw;
            }
            if (nh)
                std::printf("  holes led by %-13s %4d (%4d procs): pre-draw shelf %8.1f + C output %8.1f vs C standing draw %8.1f (%.0f%%)\n",
                            k_bucket_name[k], nh, np, sh, ou, dr, dr > 0 ? 100.0 * (sh + ou) / dr : 0.0);
        }
    }

    if (list)
    {
        std::printf("\nLIST seed bid prov state market home seat recipe input need floor pre_pool pre_shelf p/base short sole own_out others_draw mkt_draw obt_gen obt_play primary ran_settle over_ticks\n");
        for (const starved_rec* s : all)
            std::printf("  %u %llu %s %s %llu %d %d %s %s %.2f %.2f %.2f %.2f %.2f %d %d %.2f %.2f %.2f %d %d %s %d %d\n",
                        s->seed, static_cast<unsigned long long>(s->bid), s->gen ? "gen" : "scorer",
                        s->decom ? "decom" : "input", static_cast<unsigned long long>(s->market), s->home, s->seat,
                        s->recipe.c_str(), rn(s->r).c_str(), s->need, s->floor, s->pre_pool, s->pre_shelf,
                        s->price_over_base, s->short_inputs, s->sole, s->own_out, s->others_draw, s->mkt_draw, s->obt_gen,
                        s->obt_play, k_bucket_name[s->primary], s->ran_settle, s->over_ticks);
    }
    std::printf("\nhandoff_starvation: analysed %zu, seeds %zu, failed %d\n", all.size(), outs.size(), fails);
    return fails ? 1 : 0;
}
