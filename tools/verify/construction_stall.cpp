// ---------------------------------------------------------------------------
// construction_stall — BL-1183 (ceiling stalls construction), the diagnosis
// ---------------------------------------------------------------------------
// WHY a building under construction makes no progress, per site, on the
// SHIPPED start. The world is market_viability's exactly: build_app_start_world,
// the 12-tick settle (spectating), the seat, then play ticks as
// run_app_live_window steps them.
//
// THE READ. A hook after lap 0 ("convoys") snapshots what run_construction
// (economy_system.cpp, the first thing run_economy_step does) is about to read:
// each site's per-tick need row, and each needed material's shelf and posted
// price. The site's verdict for that tick is then the construction pass's own
// rule restated:
//   nomkt      no market for the site's tile
//   ceiling    a material's posted price > reservation_mult x base
//              (shelf_admits false) — split by whether the shelf HELD the
//              tick's need (stocked) or not (empty)
//   short      admitted, but the shelf holds < need / max_stretch (pause)
//   ok         every material clears the pause line (capacity stretches,
//              never pauses — BL-709)
// A site's first blocking material (lowest resource index) names the cause;
// "ceiling" outranks "short" (a ceiling block is the BL-1172 mechanism).
//
// PROGRESS. (ticks_remaining, construction_progress) after every tick. A site
// is STALLED at a reading when it has not moved for >= 10 ticks.
//
// OVER THE RUN: sites that ever stalled 10+ ticks, how many moved again, how
// many completed; new sites started during play, and how many of those by an
// owner already holding a stalled site; what stalled sites cost their owners
// (compute_building_opex at contention 1, hab 1 — an approximation of what
// apply_budget charges an under-construction building, which it does not skip).
//
// A PURE READER. Usage (repo root):
//   build_gen/verify/construction_stall.exe [--seeds a,b] [--ticks N]
// Build: bash tools/verify/build_lua_harness.sh construction_stall
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/budget_system.hpp"
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

constexpr int k_stall_ticks = 10;

const char* type_name(building_type t)
{
    switch (t)
    {
        case building_type::extraction_site:      return "extract";
        case building_type::processing_facility:  return "process";
        case building_type::port:                 return "port";
        case building_type::launchpad:            return "launchpad";
        case building_type::inland_logistics_hub: return "hub";
        case building_type::military_base:        return "mil_base";
        case building_type::research_institute:   return "research";
        case building_type::schooling:            return "school";
        case building_type::university:           return "university";
        default:                                  return "none";
    }
}

enum verdict { v_nomkt = 0, v_ceil_stocked, v_ceil_empty, v_short, v_contended, v_ok, v_count };
const char* k_verdict[v_count] = {"nomkt", "ceil+stocked", "ceil+empty", "short", "contended", "ok"};

struct site_verdict
{
    verdict v = v_ok;
    int     material = -1;   ///< the blocking material
    float   price_over_base = 0.0f;
    float   shelf = 0.0f, need = 0.0f;
    int     elsewhere = 0;          ///< other markets on the body stocking 10x the need
    float   elsewhere_best = 0.0f;
    entity_id market = null_entity;
};

struct site_rec
{
    float last_left   = 0.0f;   ///< ticks_remaining - progress
    int   last_move   = 0;      ///< play tick (settle ticks negative) it last moved
    int   born        = 0;
    bool  ever_stalled = false;
    bool  moved_after_stall = false;
    bool  completed   = false;
    bool  stalled_now = false;
    entity_id owner   = null_entity;
};

struct ctx_t
{
    const recipe_registry* reg = nullptr;
    std::map<entity_id, site_verdict> verdicts; ///< this tick's, from the lap-0 snapshot
};

std::map<entity_id, entity_id> owners(const world& w)
{
    std::map<entity_id, entity_id> o;
    for (const auto& [cid, cc] : w.corporations)
        for (const entity_id a : cc.assets) o[a] = cid;
    return o;
}

void after_lap(const world& w, int lap, void* vctx)
{
    if (lap != 0) return; // after "convoys": what run_construction reads
    auto* c = static_cast<ctx_t*>(vctx);
    const recipe_registry& reg = *c->reg;
    c->verdicts.clear();
    const float max_stretch = reg.construction().max_stretch;
    const float pause_below = max_stretch > 1.0f ? 1.0f / max_stretch : 0.0f;
    const float res_mult    = reg.price_band().reservation_mult;
    const std::size_t cap = static_cast<std::size_t>(resource_type::construction_capacity);
    std::vector<entity_id> ids;
    for (const auto& [bid, b] : w.buildings) if (b.ticks_remaining > 0) ids.push_back(bid);
    std::sort(ids.begin(), ids.end());
    // Each market's shelf as run_construction will see it, DRAWN DOWN site by
    // site in ascending id order exactly as the pass draws (contention).
    std::map<entity_id, std::array<float, resource_count>> shelf;
    for (const auto& [mid, m] : w.markets) shelf[mid] = m.inventory;
    for (const entity_id bid : ids)
    {
        const building_component& b = w.buildings.at(bid);
        const float duration = reg.economics(b.type).build_duration_ticks;
        site_verdict sv;
        if (duration <= 0.0f) { c->verdicts[bid] = sv; continue; }
        const auto& row = reg.resource_build_cost_for(b.type, b.target_resource, b.recipe);
        const entity_id mid = market_for_tile(w, b.tile);
        if (mid == null_entity) { sv.v = v_nomkt; c->verdicts[bid] = sv; continue; }
        const market_component& m = w.markets.at(mid);
        auto& inv_row = shelf[mid];
        site_verdict first_short; bool have_short = false;
        float rate = 1.0f;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (r == cap) continue;
            const float need = row[r] / duration;
            if (need <= 0.0f) continue;
            const float inv  = std::max(0.0f, inv_row[r]);
            const float inv0 = std::max(0.0f, m.inventory[r]);
            const bool admitted = shelf_admits(m, r, res_mult, true);
            const float pob = m.base_price[r] > 0.0f ? posted_price(m, r) / m.base_price[r] : 0.0f;
            if (!admitted)
            {
                if (sv.v == v_ok)
                {
                    sv.v = inv0 >= need ? v_ceil_stocked : v_ceil_empty;
                    sv.material = static_cast<int>(r); sv.price_over_base = pob; sv.shelf = inv0; sv.need = need;
                }
                rate = 0.0f;
                continue;
            }
            rate = std::min(rate, inv / need);
            if (inv / need < pause_below && !have_short)
            {
                // short because OTHER sites drew it first, or never there?
                first_short.v = inv0 / need >= pause_below ? v_contended : v_short;
                first_short.material = static_cast<int>(r);
                first_short.price_over_base = pob; first_short.shelf = inv0; first_short.need = need;
                have_short = true;
            }
        }
        if (sv.v == v_ok && have_short) sv = first_short;
        if (rate < pause_below) rate = 0.0f;
        if (rate > 0.0f)
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                if (r == cap) continue;
                const float need = row[r] / duration;
                if (need <= 0.0f) continue;
                inv_row[r] -= std::min(need * rate, std::max(0.0f, inv_row[r]));
            }
        // Where else on the body is the blocking material stocked?
        if (sv.material >= 0)
        {
            const entity_id body = m.body;
            for (const auto& [om, omc] : w.markets)
                if (om != mid && omc.body == body && omc.inventory[sv.material] >= 10.0f * sv.need)
                { ++sv.elsewhere; sv.elsewhere_best = std::max(sv.elsewhere_best, omc.inventory[sv.material]); }
        }
        sv.market = mid;
        c->verdicts[bid] = sv;
    }
}

struct reading
{
    int uc = 0, stalled = 0, seat_uc = 0, seat_stalled = 0;
    int stalled_born[3] = {}; ///< stalled sites by birth: generation, settle, play
    std::map<std::string, std::pair<int, int>> by_type;            ///< type -> (uc, stalled)
    int verdict_all[v_count] = {}, verdict_stalled[v_count] = {};
    std::map<std::pair<int, int>, std::array<double, 5>> by_mat;    ///< (verdict, mat) -> n, sum p/b, sum shelf, n with elsewhere, sum best
    std::map<std::pair<int, int>, std::set<entity_id>> mat_mkts;
    double stalled_opex = 0.0;
};

void take_reading(const world& w, const recipe_registry& reg, const ctx_t& c,
                  const std::map<entity_id, site_rec>& sites, entity_id seat, int now, reading& rd)
{
    for (const auto& [bid, s] : sites)
    {
        const auto bi = w.buildings.find(bid);
        if (bi == w.buildings.end() || bi->second.ticks_remaining <= 0) continue;
        const building_component& b = bi->second;
        const bool st = now - s.last_move >= k_stall_ticks;
        ++rd.uc; if (st) ++rd.stalled;
        if (s.owner == seat) { ++rd.seat_uc; if (st) ++rd.seat_stalled; }
        if (st) ++rd.stalled_born[s.born <= -k_campaign_settle_ticks ? 0 : s.born <= 0 ? 1 : 2];
        auto& bt = rd.by_type[type_name(b.type)]; ++bt.first; if (st) ++bt.second;
        const auto vi = c.verdicts.find(bid);
        const site_verdict sv = vi == c.verdicts.end() ? site_verdict{} : vi->second;
        ++rd.verdict_all[sv.v];
        if (st)
        {
            ++rd.verdict_stalled[sv.v];
            auto& bm = rd.by_mat[{sv.v, sv.material}];
            bm[0] += 1; bm[1] += sv.price_over_base; bm[2] += sv.shelf;
            if (sv.elsewhere > 0) { bm[3] += 1; bm[4] += sv.elsewhere_best; }
            rd.mat_mkts[{sv.v, sv.material}].insert(sv.market);
            const building_opex o = compute_building_opex(b, reg.economics(b.type), 1.0f, 1.0f,
                                                          reg.idle_maintenance_floor());
            rd.stalled_opex += o.maintenance + o.wages;
        }
    }
}

void print_reading(const char* label, const reading& rd)
{
    std::printf("  %-8s under construction %5d  stalled(%d+) %5d (born: gen %d, settle %d, play %d)  | seat %d/%d\n", label, rd.uc,
                k_stall_ticks, rd.stalled, rd.stalled_born[0], rd.stalled_born[1], rd.stalled_born[2], rd.seat_uc, rd.seat_stalled);
    std::printf("           by type (uc/stalled):");
    for (const auto& [t, p] : rd.by_type) std::printf(" %s %d/%d", t.c_str(), p.first, p.second);
    std::printf("\n           verdict all:");
    for (int v = 0; v < v_count; ++v) std::printf(" %s %d", k_verdict[v], rd.verdict_all[v]);
    std::printf("\n           verdict stalled:");
    for (int v = 0; v < v_count; ++v) std::printf(" %s %d", k_verdict[v], rd.verdict_stalled[v]);
    std::printf("  | stalled sites' opex/tick ~%.0f\n", rd.stalled_opex);
    for (const auto& [k, a] : rd.by_mat)
    {
        if (k.second < 0) continue;
        std::printf("             %-12s %-14s sites %4.0f in %3zu mkts  mean p/base %.2f  mean shelf %.1f"
                    "  | another mkt on the body stocks 10x need: %3.0f sites (mean best %.0f u)\n",
                    k_verdict[k.first], resource_names::name_of(static_cast<resource_type>(k.second)).c_str(),
                    a[0], rd.mat_mkts.at(k).size(), a[1] / a[0], a[2] / a[0], a[3], a[3] > 0 ? a[4] / a[3] : 0.0);
    }
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

void run_seed(std::uint32_t seed, int ticks)
{
    lua_state lua;
    world_params p;
    p.seed = seed;
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, p, *start);
    world& w = start->w;
    const recipe_registry& reg = start->reg;
    std::printf("=== seed %u ===\n", seed);

    ctx_t c; c.reg = &reg;
    settle_tick_hooks hooks; hooks.after_lap = after_lap; hooks.ctx = &c;
    std::map<entity_id, site_rec> sites;
    int settle_started = 0, play_started = 0, play_started_by_staller = 0;
    std::map<std::string, int> play_started_type;

    auto track = [&](int now, bool play) {
        const auto own = owners(w);
        std::set<entity_id> stalling_owners;
        for (auto& [bid, s] : sites)
            if (!s.completed && w.buildings.count(bid) && now - s.last_move >= k_stall_ticks)
                stalling_owners.insert(s.owner);
        std::vector<entity_id> ids;
        for (const auto& [bid, b] : w.buildings) ids.push_back(bid);
        std::sort(ids.begin(), ids.end());
        for (const entity_id bid : ids)
        {
            const building_component& b = w.buildings.at(bid);
            auto it = sites.find(bid);
            if (it == sites.end())
            {
                if (b.ticks_remaining <= 0) continue;
                site_rec s; s.born = now; s.last_move = now;
                s.last_left = b.ticks_remaining - b.construction_progress;
                const auto oi = own.find(bid); s.owner = oi == own.end() ? null_entity : oi->second;
                sites[bid] = s;
                if (play)
                {
                    ++play_started; ++play_started_type[type_name(b.type)];
                    if (stalling_owners.count(s.owner)) ++play_started_by_staller;
                }
                else ++settle_started;
                continue;
            }
            site_rec& s = it->second;
            if (s.completed) continue;
            const float left = b.ticks_remaining > 0 ? b.ticks_remaining - b.construction_progress : 0.0f;
            if (left != s.last_left)
            {
                if (now - s.last_move >= k_stall_ticks) s.moved_after_stall = true;
                s.last_move = now; s.last_left = left;
            }
            else if (now - s.last_move >= k_stall_ticks) s.ever_stalled = true;
            if (b.ticks_remaining <= 0) s.completed = true;
        }
    };

    // Sites standing when the world is handed to the settle.
    track(-k_campaign_settle_ticks, false);
    for (int step = 0; step < k_campaign_settle_ticks; ++step)
    {
        run_settle_tick(w, reg, step, 0, true, &hooks);
        track(step - k_campaign_settle_ticks + 1, false);
    }
    const spawn_seat_result seat = seat_player_corporation(w, seed, start->land.search.winner_score);
    // ownership can change at the seat — refresh
    { const auto own = owners(w); for (auto& [bid, s] : sites) { const auto oi = own.find(bid); if (oi != own.end()) s.owner = oi->second; } }

    const std::set<int> readings = {1, 50, 200, 400};
    std::map<int, reading> rds;
    constexpr int k_econ_tick_days = 90;
    double cum_stalled_opex = 0.0;
    for (int k = 1; k <= ticks; ++k)
    {
        const int day = k * k_econ_tick_days;
        advance_orbits(w, static_cast<double>(k_econ_tick_days));
        advance_surveys(w, k_econ_tick_days);
        w.current_day_tick = day;
        // a reading at tick k is the state run_construction reads on tick k
        // (verdicts from the lap-0 hook) against stall ages as of tick k-1's end
        settle_tick_result res = run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), day, false, &hooks);
        (void)res;
        if (readings.count(k))
        {
            // stall age as of the end of tick k-1, sites as of before tick k ran:
            // approximate with the current building set (sites completed this
            // tick drop out; they were not stalled).
            take_reading(w, reg, c, sites, seat.seated, k - 1, rds[k]);
        }
        track(k, true);
        for (const auto& [bid, s] : sites)
            if (!s.completed && w.buildings.count(bid) && k - s.last_move >= k_stall_ticks)
            {
                const building_component& b = w.buildings.at(bid);
                const building_opex o = compute_building_opex(b, reg.economics(b.type), 1.0f, 1.0f,
                                                              reg.idle_maintenance_floor());
                cum_stalled_opex += o.maintenance + o.wages;
            }
    }
    for (const auto& [k, rd] : rds)
    {
        char lab[16]; std::snprintf(lab, sizeof lab, k == 1 ? "handoff" : "t%d", k);
        print_reading(lab, rd);
    }
    int ever = 0, moved = 0, done = 0, done_after = 0, gone = 0;
    for (const auto& [bid, s] : sites)
    {
        if (!s.ever_stalled) continue;
        ++ever;
        if (s.moved_after_stall) ++moved;
        if (s.completed) ++done;
        if (!w.buildings.count(bid)) ++gone;
    }
    for (const auto& [bid, s] : sites) if (s.completed) ++done_after;
    std::printf("  run: sites seen %zu (settle-start %d, play-start %d, of which by an owner holding a stalled site %d)\n",
                sites.size(), settle_started, play_started, play_started_by_staller);
    std::printf("       play starts by type:");
    for (const auto& [t, n] : play_started_type) std::printf(" %s %d", t.c_str(), n);
    std::printf("\n       ever stalled %d+: %d  moved again %d  completed %d  demolished/gone %d | all completed %d\n",
                k_stall_ticks, ever, moved, done, gone, done_after);
    std::printf("       cumulative opex of stalled sites (approx) %.0f\n", cum_stalled_opex);
    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv)
{
    int ticks = 400;
    std::vector<std::uint32_t> seeds = {0, 43, 10};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::max(1, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = parse_seed_list(argv[++i]);
    }
    for (const std::uint32_t s : seeds) run_seed(s, ticks);
    return 0;
}
