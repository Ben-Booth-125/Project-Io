// ---------------------------------------------------------------------------
// processor_placement_probe — BL-1233 (processors to inputs), diagnosis probe
// ---------------------------------------------------------------------------
// QUESTION. inflow_probe found bodies make 28-63% of the raw silica / copper
// ore / rare earth ore their processors want. BL-1185 made a chain FEASIBLE
// (some producer of every input stands within reach); this asks whether it is
// SIZED: per body and input good, the processors placed and their full-run
// want, against the extraction placed and its REALISTIC output — and which
// placement path (specialist roster, charter-web firm, legacy Pass 6 firm, the
// settle scorer) owns the processors that end up input-starved.
//
// TWO READS per seed, the world built exactly as inflow_probe / market_viability
// build it (build_app_start_world, the 12-tick settle, seat_player_corporation):
//   GEN      the end of generation, before the first settle tick
//   HANDOFF  after the settle and the seat (the settle's last report is the
//            actual cross-check)
//
// A PURE READER. Reach is asked through input_reach (the one definition
// placement and the scorer share); the haul memo warms the logistics caches
// only.
//
// FULL RUN. Every building is read at full labour (workforce_assigned 1.0) and
// its own workforce_target and supply scalar, so want and output are measured
// on one footing; the mean assigned labour on each side is printed beside it.
//   want   processor:  inputs[g] x base_rate x wt x supply
//   NOM    extraction: extraction_nominal x co-extraction share — what
//                      generation's reach sees (no stack rank)
//   REAL   extraction: NOM x stack_output_scalar(rank) x taper (shared reserve)
//          processor:  outputs[g] x base_rate x wt x supply x its own covered
//                      run (iterated 6 rounds down the chain)
//
// ALLOCATION (the model). Each producer market P splits its REAL output of g
// among the consumer markets in reach of it, pro-rata to their want; a consumer
// market's coverage is what it is allotted over its want, capped at 1. A
// processor's run is the min over its non-grid inputs of its market's coverage.
// STARVED = run < t_idle. The handoff read also prints the ACTUAL starved count
// from the settle's last report (has_limiting, not active) beside the model's.
//
// Usage (repo root): build_gen/verify/processor_placement_probe.exe [--seeds a,b] [--bodies]
// Build:  ./tools/verify/build_lua_harness.sh processor_placement_probe
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/input_reach.hpp"
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

using arr = std::array<double, resource_count>;
std::string gname(std::size_t g) { return resource_names::name_of(static_cast<resource_type>(g)); }

enum pclass : int { k_spec = 0, k_seat, k_charter, k_pass6, k_settle, k_unowned, k_ncls };
const char* k_cls[k_ncls] = {"specialist", "seat", "charter-firm", "pass6-firm", "settle-scorer", "unowned"};

constexpr std::size_t k_focus[] = {static_cast<std::size_t>(resource_type::silica),
                                   static_cast<std::size_t>(resource_type::copper_ore),
                                   static_cast<std::size_t>(resource_type::rare_earth_ore)};

bool is_extractable(std::size_t g)
{
    for (const resource_type r : placement_rules::k_extractable)
        if (static_cast<std::size_t>(r) == g) return true;
    return false;
}

struct proc
{
    entity_id bid, market, body, corp;
    int cls;
    const recipe* rc;
    double batches; // base_rate x wt x supply at full labour
    double assigned;
    double run = 1.0;
    std::size_t limit = resource_count;
    bool actual_starved = false, has_row = false;
};

struct good_body
{
    double want = 0, nom = 0, real = 0, alloc = 0;
    int procs = 0, sites = 0;
    std::array<int, k_ncls> procs_cls{};
    std::array<double, k_ncls> want_cls{};
};

/// Pooled over seeds, per (read, class).
struct pool
{
    std::array<int, k_ncls> placed{}, starved{}, zero{}, actual{}, actual_rows{};
    std::array<double, k_ncls> short_units{}; ///< sum of (t_idle - run) x limiting want, starved only
    std::map<std::size_t, std::array<double, 4>> goods; ///< want, nom, real, alloc
    /// starved processors' limiting input, by cause:
    /// 0 none on body, 1 producer on body none in reach, 2 in reach but REAL ~0 (unlaboured/spent),
    /// 3 in reach, NOM covers t_idle but REAL does not (stack/share/taper), 4 contended (REAL in reach >= t_idle x want, allotted less)
    /// 5 intermediate (processed) input, upstream short
    std::array<int, 6> cause{};
    std::vector<double> short_list;
    std::map<std::size_t, std::array<std::array<int, 3>, k_ncls>> focus_cov; ///< focus good -> class -> (<t_idle, partial, full)
};
const char* k_cause[6] = {"no producer on body", "producer on body, none in reach", "in reach, REAL output 0",
                          "in reach: NOM covers t_idle, REAL does not (stack/share/taper)",
                          "contended: REAL in reach covers t_idle alone, allotted less", "processed input, upstream short"};

struct read_ctx
{
    world& w;
    const recipe_registry& reg;
    const std::set<entity_id>* gen_ids; // null at GEN
    const economy_report* rep;          // null at GEN
};

void read_world(read_ctx& c, const char* label, std::uint32_t seed, bool bodies, pool& P)
{
    world& w = c.w;
    const recipe_registry& reg = c.reg;
    const double t_idle = reg.t_idle();
    const double prate = reg.economics(building_type::processing_facility).base_rate;

    std::map<entity_id, entity_id> owner;
    for (const auto& [cid, cc] : w.corporations) for (const entity_id a : cc.assets) owner.emplace(a, cid);
    const auto cls_of = [&](entity_id bid, entity_id corp) {
        if (c.gen_ids && !c.gen_ids->count(bid)) return int(k_settle);
        if (corp == null_entity) return int(k_unowned);
        const corporation_component& cc = w.corporations.at(corp);
        if (cc.is_player) return int(k_seat);
        if (!cc.is_background) return int(k_spec);
        return cc.origin_region >= 0 ? int(k_charter) : int(k_pass6);
    };

    std::vector<entity_id> ids;
    for (const auto& kv : w.buildings) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());

    std::map<entity_id, const building_report*> row;
    if (c.rep) for (const building_report& br : c.rep->buildings) row[br.building] = &br;

    // ---- processors ----
    std::vector<proc> procs;
    for (const entity_id bid : ids)
    {
        const building_component& b = w.buildings.at(bid);
        if (b.type != building_type::processing_facility || b.decommissioned || b.ticks_remaining > 0) continue;
        const recipe* rc = reg.get_recipe(b.recipe);
        if (!rc) continue;
        const entity_id m = market_for_tile(w, b.tile);
        const auto mi = w.markets.find(m);
        if (mi == w.markets.end()) continue;
        proc p;
        p.bid = bid; p.market = m; p.body = mi->second.body;
        const auto oi = owner.find(bid);
        p.corp = oi == owner.end() ? null_entity : oi->second;
        p.cls = cls_of(bid, p.corp);
        p.rc = rc;
        p.batches = prate * std::clamp(b.workforce_target / 100.0, 0.0, 2.0) * building_supply_scalar(b);
        p.assigned = b.workforce_assigned;
        if (const auto ri = row.find(bid); ri != row.end())
        {
            p.has_row = true;
            p.actual_starved = ri->second->has_limiting && !ri->second->active;
        }
        procs.push_back(p);
    }

    // ---- extraction REAL / NOM by (market, good) ----
    std::map<std::pair<entity_id, std::size_t>, double> ext_real, ext_nom;
    std::map<std::pair<entity_id, std::size_t>, int> ext_sites;
    double ext_assigned = 0, ext_sup = 0; int ext_n = 0, ext_stacked = 0;
    // shared-stack taper: combined nominal per (tile, target)
    std::map<std::pair<entity_id, std::size_t>, double> stack_nom;
    for (const entity_id bid : ids)
    {
        const building_component& b = w.buildings.at(bid);
        if (b.type != building_type::extraction_site || b.decommissioned || b.ticks_remaining > 0) continue;
        building_component x = b; x.workforce_assigned = 1.0f;
        const auto oi = owner.find(bid);
        const entity_id corp = oi == owner.end() ? null_entity : oi->second;
        stack_nom[{b.tile, std::size_t(b.target_resource)}] +=
            extraction_nominal(w, reg, x, 1.0f, corp) * placement_rules::stack_output_scalar(placement_rules::stack_rank(w, bid));
    }
    for (const entity_id bid : ids)
    {
        const building_component& b = w.buildings.at(bid);
        if (b.type != building_type::extraction_site || b.decommissioned || b.ticks_remaining > 0) continue;
        const entity_id m = market_for_tile(w, b.tile);
        if (w.markets.find(m) == w.markets.end()) continue;
        const auto oi = owner.find(bid);
        const entity_id corp = oi == owner.end() ? null_entity : oi->second;
        building_component x = b; x.workforce_assigned = 1.0f;
        const double nom = extraction_nominal(w, reg, x, 1.0f, corp);
        const double ss = placement_rules::stack_output_scalar(placement_rules::stack_rank(w, bid));
        ext_assigned += b.workforce_assigned; ++ext_n;
        ext_sup += building_supply_scalar(b); if (ss < 1.0) ++ext_stacked;
        const std::size_t pr = std::size_t(b.target_resource);
        if (placement_rules::is_depositless_site(w, b.tile, b.target_resource))
        {
            ext_nom[{m, pr}] += nom; ext_real[{m, pr}] += nom * ss; ++ext_sites[{m, pr}];
            continue;
        }
        const tile_component& tc = w.tiles.at(b.tile);
        if (!(tc.resource_remaining[pr] > 0.0f)) continue;
        double tot = 0;
        for (const resource_type r : placement_rules::k_extractable) tot += tc.resource_deposit[std::size_t(r)];
        if (!(tot > 0)) continue;
        const double sn = stack_nom[{b.tile, pr}];
        double taper = 1.0;
        if (sn > 0) { taper = std::clamp(double(tc.resource_remaining[pr]) / (deposit_taper_ticks * sn), 0.0, 1.0); if (taper < deposit_min_taper) taper = 0; }
        for (const resource_type r : placement_rules::k_extractable)
        {
            const std::size_t g = std::size_t(r);
            if (!(tc.resource_deposit[g] > 0.0f) || !(tc.resource_remaining[g] > 0.0f)) continue;
            const double share = tc.resource_deposit[g] / tot;
            ext_nom[{m, g}] += nom * share;
            ext_real[{m, g}] += nom * ss * taper * share;
            ++ext_sites[{m, g}];
        }
    }

    // ---- reach, memoised per (P, C, g) ----
    input_reach ir = make_input_reach(w, reg);
    std::map<std::tuple<entity_id, entity_id, std::size_t>, bool> reach_memo;
    const auto reach = [&](entity_id Pm, entity_id Cm, std::size_t g) {
        if (Pm == Cm) return true;
        const auto k = std::make_tuple(Pm, Cm, g);
        auto it = reach_memo.find(k);
        if (it == reach_memo.end()) it = reach_memo.emplace(k, market_within_reach(w, reg, ir, Pm, Cm, g)).first;
        return it->second;
    };
    std::map<entity_id, entity_id> mbody;
    for (const auto& [mid, mc] : w.markets) mbody[mid] = mc.body;

    // consumer want by (C, g)
    std::map<std::pair<entity_id, std::size_t>, double> want;
    for (const proc& p : procs)
        for (std::size_t g = 0; g < resource_count; ++g)
            if (p.rc->inputs[g] > 0.0f && !reg.grid_goods().grid(g)) want[{p.market, g}] += p.rc->inputs[g] * p.batches;

    std::array<std::vector<std::pair<entity_id, double>>, resource_count> want_by_g;
    for (const auto& [k, wv] : want) want_by_g[k.second].push_back({k.first, wv});

    // ---- iterate: processor output scaled by its covered run ----
    std::map<std::pair<entity_id, std::size_t>, double> cover, alloc;
    for (int round = 0; round < 6; ++round)
    {
        std::map<std::pair<entity_id, std::size_t>, double> supply = ext_real;
        for (const proc& p : procs)
        {
            for (std::size_t g = 0; g < resource_count; ++g)
                if (p.rc->outputs[g] > 0.0f) supply[{p.market, g}] += p.rc->outputs[g] * p.batches * p.run;
        }
        alloc.clear();
        // per (P, g): split over reachable consumers on the same body pro-rata to want
        for (const auto& [pk, s] : supply)
        {
            if (!(s > 0)) continue;
            const std::size_t g = pk.second;
            if (reg.grid_goods().grid(g)) continue;
            std::vector<std::pair<entity_id, double>> cs; double wt = 0;
            for (const auto& [C, wv] : want_by_g[g])
            {
                if (!reach(pk.first, C, g)) continue;
                cs.push_back({C, wv}); wt += wv;
            }
            if (!(wt > 0)) continue;
            for (const auto& [C, wv] : cs) alloc[{C, g}] += s * wv / wt;
        }
        cover.clear();
        for (const auto& [k, wv] : want) cover[k] = wv > 0 ? std::min(1.0, alloc[k] / wv) : 1.0;
        for (proc& p : procs)
        {
            p.run = 1.0; p.limit = resource_count;
            for (std::size_t g = 0; g < resource_count; ++g)
            {
                if (!(p.rc->inputs[g] > 0.0f) || reg.grid_goods().grid(g)) continue;
                const double cv = cover[{p.market, g}];
                if (cv < p.run) { p.run = cv; p.limit = g; }
            }
        }
    }

    // ---- tallies ----
    std::map<std::pair<entity_id, std::size_t>, good_body> GB;
    for (const proc& p : procs)
        for (std::size_t g = 0; g < resource_count; ++g)
            if (p.rc->inputs[g] > 0.0f && !reg.grid_goods().grid(g))
            {
                good_body& gb = GB[{p.body, g}];
                const double wv = p.rc->inputs[g] * p.batches;
                gb.want += wv; ++gb.procs; ++gb.procs_cls[p.cls]; gb.want_cls[p.cls] += wv;
            }
    for (const auto& [k, v] : ext_real) { auto& gb = GB[{mbody[k.first], k.second}]; gb.real += v; gb.sites += ext_sites[k]; }
    for (const auto& [k, v] : ext_nom) GB[{mbody[k.first], k.second}].nom += v;
    for (const auto& [k, v] : alloc) GB[{mbody[k.first], k.second}].alloc += std::min(v, want[k]);

    std::array<int, k_ncls> placed{}, starved{}, zero{}, act{}, actrows{};
    std::array<double, k_ncls> shortu{};
    std::array<int, 6> cause{};
    std::vector<double> shorts;
    std::map<int, std::pair<int, int>> by_recipe; // recipe -> (placed, starved)
    double proc_assigned = 0;
    for (const proc& p : procs)
    {
        ++placed[p.cls]; proc_assigned += p.assigned;
        auto& br = by_recipe[int(w.buildings.at(p.bid).recipe)]; ++br.first;
        if (p.has_row) { ++actrows[p.cls]; if (p.actual_starved) ++act[p.cls]; }
        if (!(p.run < t_idle)) continue;
        ++starved[p.cls]; ++br.second;
        if (p.run <= 0.0) ++zero[p.cls];
        const std::size_t g = p.limit;
        const double wv = p.rc->inputs[g] * p.batches;
        const double su = (t_idle - p.run) * wv;
        shortu[p.cls] += su; shorts.push_back(su);
        // cause
        if (!is_extractable(g)) { ++cause[5]; continue; }
        double on_body = 0, nom_r = 0, real_r = 0;
        for (const auto& [k, v] : ext_nom)
        {
            if (k.second != g || mbody[k.first] != p.body) continue;
            on_body += v;
            if (reach(k.first, p.market, g)) { nom_r += v; real_r += ext_real[k]; }
        }
        // producing processors of a raw are rare; ignored here
        if (!(on_body > 0)) ++cause[0];
        else if (!(nom_r > 0)) ++cause[1];
        else if (!(real_r > 0)) ++cause[2];
        else if (real_r >= t_idle * wv) ++cause[4];
        else if (nom_r >= t_idle * wv) ++cause[3];
        else ++cause[4];
    }

    // ---- print ----
    int np = 0, ns = 0, na = 0, nar = 0;
    for (int i = 0; i < k_ncls; ++i) { np += placed[i]; ns += starved[i]; na += act[i]; nar += actrows[i]; }
    std::printf("\n-- seed %u %s: %d processors (mean assigned %.2f), %d extraction sites (mean assigned %.2f); MODEL starved (run < t_idle %.2f) %d",
                seed, label, np, np ? proc_assigned / np : 0.0, ext_n, ext_n ? ext_assigned / ext_n : 0.0, t_idle, ns);
    if (c.rep) std::printf("; ACTUAL starved (settle's last report) %d of %d rows", na, nar);
    std::printf("\n   extraction: %d sites at stack rank > 1; mean supply scalar %.2f", ext_stacked, ext_n ? ext_sup / ext_n : 0.0);
    std::printf("\n   by class (placed / model-starved / zero-supply%s / short units):\n", c.rep ? " / actual-starved" : "");
    for (int i = 0; i < k_ncls; ++i)
    {
        if (!placed[i]) continue;
        std::printf("     %-14s %4d / %4d / %4d", k_cls[i], placed[i], starved[i], zero[i]);
        if (c.rep) std::printf(" / %4d", act[i]);
        std::printf(" / %.2f\n", shortu[i]);
    }
    std::printf("   starved cause (limiting input):");
    for (int i = 0; i < 6; ++i) std::printf("  [%s] %d", k_cause[i], cause[i]);
    std::printf("\n");
    // concentration
    std::sort(shorts.begin(), shorts.end(), std::greater<double>());
    double stot = 0; for (const double s : shorts) stot += s;
    double top = 0; const std::size_t n10 = std::max<std::size_t>(1, shorts.size() / 10);
    for (std::size_t i = 0; i < n10 && i < shorts.size(); ++i) top += shorts[i];
    if (!shorts.empty())
        std::printf("   shortfall concentration: %zu starved, total %.2f u/t; top 10%% (%zu) hold %.0f%%; median %.3f, max %.3f\n",
                    shorts.size(), stot, n10, 100.0 * top / stot, shorts[shorts.size() / 2], shorts[0]);
    // recipes starved most
    std::vector<std::pair<int, int>> rs; for (const auto& [r, v] : by_recipe) if (v.second) rs.push_back({v.second, r});
    std::sort(rs.rbegin(), rs.rend());
    std::printf("   starved by recipe:");
    for (std::size_t i = 0; i < rs.size() && i < 8; ++i)
    {
        const recipe* rc = reg.get_recipe(uint16_t(rs[i].second));
        std::printf("  %s %d/%d", rc ? rc->name.c_str() : "?", rs[i].first, by_recipe[rs[i].second].first);
    }
    std::printf("\n");
    // focus goods per body
    for (const std::size_t g : k_focus)
    {
        good_body S;
        for (const auto& [k, gb] : GB)
        {
            if (k.second != g) continue;
            S.want += gb.want; S.nom += gb.nom; S.real += gb.real; S.alloc += gb.alloc; S.procs += gb.procs; S.sites += gb.sites;
            for (int i = 0; i < k_ncls; ++i) { S.procs_cls[i] += gb.procs_cls[i]; S.want_cls[i] += gb.want_cls[i]; }
        }
        auto& pg = P.goods[g]; pg[0] += S.want; pg[1] += S.nom; pg[2] += S.real; pg[3] += S.alloc;
        // per processor taking g: its market's coverage of g, by class (<t_idle / <1 / full)
        {
            std::array<std::array<int, 3>, k_ncls> h{};
            std::map<std::string, std::array<int, 2>> rec; // recipe -> (procs, cover < t_idle)
            for (const proc& p : procs)
            {
                if (!(p.rc->inputs[g] > 0.0f)) continue;
                const double cv = cover[{p.market, g}];
                const int b = cv < t_idle ? 0 : (cv < 1.0 ? 1 : 2);
                ++h[p.cls][b];
                auto& rr = rec[p.rc->name]; ++rr[0]; if (b == 0) ++rr[1];
                auto& ph = P.focus_cov[g][p.cls]; ++ph[b];
            }
            std::printf("     %s coverage per processor (<t_idle / partial / full):", gname(g).c_str());
            for (int i = 0; i < k_ncls; ++i) if (h[i][0] + h[i][1] + h[i][2]) std::printf("  %s %d/%d/%d", k_cls[i], h[i][0], h[i][1], h[i][2]);
            std::printf("  | recipes:");
            for (const auto& [n, v] : rec) std::printf(" %s %d(%d starved)", n.c_str(), v[0], v[1]);
            std::printf("\n");
        }
        std::printf("   %-15s want %7.2f | NOM %7.2f (%3.0f%%) REAL %7.2f (%3.0f%%) allotted %7.2f (%3.0f%%) | procs %d sites %d | want by class:",
                    gname(g).c_str(), S.want, S.nom, S.want > 0 ? 100 * S.nom / S.want : 0.0, S.real,
                    S.want > 0 ? 100 * S.real / S.want : 0.0, S.alloc, S.want > 0 ? 100 * S.alloc / S.want : 0.0, S.procs, S.sites);
        for (int i = 0; i < k_ncls; ++i) if (S.procs_cls[i]) std::printf(" %s %d/%.2f", k_cls[i], S.procs_cls[i], S.want_cls[i]);
        std::printf("\n");
        if (bodies)
            for (const auto& [k, gb] : GB)
                if (k.second == g && (gb.want > 0 || gb.real > 0))
                    std::printf("       body %5u: want %7.2f NOM %7.2f REAL %7.2f allotted %7.2f procs %d sites %d\n",
                                unsigned(k.first), gb.want, gb.nom, gb.real, gb.alloc, gb.procs, gb.sites);
    }
    for (int i = 0; i < k_ncls; ++i)
    {
        P.placed[i] += placed[i]; P.starved[i] += starved[i]; P.zero[i] += zero[i];
        P.actual[i] += act[i]; P.actual_rows[i] += actrows[i]; P.short_units[i] += shortu[i];
    }
    for (int i = 0; i < 6; ++i) P.cause[i] += cause[i];
    P.short_list.insert(P.short_list.end(), shorts.begin(), shorts.end());
}

void print_pool(const char* label, const pool& P, bool actual)
{
    std::printf("\n== POOLED %s ==\n   class          placed / starved (%%) / zero%s / short u/t\n", label, actual ? " / actual" : "");
    for (int i = 0; i < k_ncls; ++i)
    {
        if (!P.placed[i]) continue;
        std::printf("   %-14s %5d / %5d (%3.0f%%) / %4d", k_cls[i], P.placed[i], P.starved[i], 100.0 * P.starved[i] / P.placed[i], P.zero[i]);
        if (actual) std::printf(" / %4d of %d", P.actual[i], P.actual_rows[i]);
        std::printf(" / %.2f\n", P.short_units[i]);
    }
    std::printf("   cause:"); for (int i = 0; i < 6; ++i) std::printf("  [%s] %d", k_cause[i], P.cause[i]);
    std::vector<double> s = P.short_list; std::sort(s.begin(), s.end(), std::greater<double>());
    double tot = 0; for (const double x : s) tot += x;
    double top = 0; const std::size_t n10 = std::max<std::size_t>(1, s.size() / 10);
    for (std::size_t i = 0; i < n10 && i < s.size(); ++i) top += s[i];
    if (!s.empty()) std::printf("\n   shortfall: %zu starved, %.2f u/t, top 10%% hold %.0f%%, median %.3f max %.3f", s.size(), tot, 100 * top / tot, s[s.size() / 2], s[0]);
    std::printf("\n");
    for (const auto& [g, a] : P.goods)
        std::printf("   %-15s want %8.2f  NOM %8.2f (%3.0f%%)  REAL %8.2f (%3.0f%%)  allotted %8.2f (%3.0f%%)\n", gname(g).c_str(), a[0], a[1],
                    a[0] > 0 ? 100 * a[1] / a[0] : 0.0, a[2], a[0] > 0 ? 100 * a[2] / a[0] : 0.0, a[3], a[0] > 0 ? 100 * a[3] / a[0] : 0.0);
    for (const auto& [g, h] : P.focus_cov)
    {
        std::printf("   %-15s processors' coverage (<t_idle / partial / full):", gname(g).c_str());
        for (int i = 0; i < k_ncls; ++i) if (h[i][0] + h[i][1] + h[i][2]) std::printf("  %s %d/%d/%d", k_cls[i], h[i][0], h[i][1], h[i][2]);
        std::printf("\n");
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds = {0, 43, 10, 28, 38};
    bool bodies = false;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--bodies")) bodies = true;
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
        else { std::fprintf(stderr, "usage: processor_placement_probe [--seeds a,b] [--bodies]\n"); return 2; }
    }
    std::printf("processor_placement_probe - BL-1233: processors placed vs the input supply they can reach\n");
    pool PG, PH;
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
        std::set<entity_id> gen_ids;
        for (const auto& kv : w.buildings) gen_ids.insert(kv.first);
        {
            read_ctx c{w, reg, nullptr, nullptr};
            read_world(c, "GEN (end of generation, before the settle)", seed, bodies, PG);
        }
        economy_report last;
        for (int step = 0; step < k_campaign_settle_ticks; ++step)
        {
            settle_tick_result r = run_settle_tick(w, reg, step, 0, true);
            if (step == k_campaign_settle_ticks - 1) last = std::move(r.report);
        }
        seat_player_corporation(w, seed, start->land.search.winner_score);
        {
            read_ctx c{w, reg, &gen_ids, &last};
            read_world(c, "HANDOFF (after the settle and the seat)", seed, bodies, PH);
        }
        std::fflush(stdout);
    }
    print_pool("GEN", PG, false);
    print_pool("HANDOFF", PH, true);
    return 0;
}
