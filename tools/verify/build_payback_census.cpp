// ---------------------------------------------------------------------------
// tools/verify/build_payback_census.cpp - BL-1496 (build payback census).
//
// REPORTS; it does not gate. Two readings on the real path (make_hard_coded_world
// with the wizard's nation targets, then finish_campaign_world):
//
//   1. SITE rows, at the handoff: for each major and each finder row (one per
//      extractable good, one per unlocked processing recipe), the build door's own
//      gates and the door's own profit figure over every tile of the major's ground.
//      Best, median and worst net per quarter against capex, so payback in years and
//      the spread between sites are known. PRODUCTION.md § What construction costs at
//      the opening rests the whole reward for searching on that spread.
//
//      The estimate is one quarter at today's prices. It does not model the price
//      a new building's own output or demand moves, so read paybacks as a ceiling on
//      how good a site looks, not a forecast.
//
//   2. QTR rows, over N play quarters with nobody seated: the treasury's movement
//      beside the quarterly return's flows and the buildings and men it bought.
//      `resid` is the movement between two consecutive return closes less the later
//      return's net: credits that moved outside the return. It reads closes and not
//      `delta`, the end-of-tick treasury's, because a return books last quarter's
//      research and garrison pay, which `delta` holds a quarter earlier.
//
// Usage: build_gen/verify/build_payback_census.exe [--seeds a,b] [--pairs 30:4]
//                                                  [--quarters N] [--no-sites]
// Build: bash tools/verify/build_lua_harness.sh build_payback_census
// Pool:  node tools/verify/build_payback_census_pool.js
// ---------------------------------------------------------------------------

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/campaign_settle.hpp"
#include "world/building_profit.hpp"
#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/economy_system.hpp"
#include "world/finish_campaign_world.hpp"
#include "world/hard_coded_world.hpp"
#include "world/logistic_points.hpp"
#include "world/nation_tech.hpp"
#include "world/orbital_system.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/site_quote.hpp"
#include "world/resource_names.hpp"
#include "world/survey_system.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"
#include "world/works_roster.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
std::vector<int> parse_ints(const char* s)
{
    std::vector<int> out;
    std::string cur;
    for (const char* p = s;; ++p)
    {
        if (*p == ',' || *p == '\0')
        {
            if (!cur.empty()) out.push_back(std::atoi(cur.c_str()));
            cur.clear();
            if (*p == '\0') break;
        }
        else cur.push_back(*p);
    }
    return out;
}

struct option_row
{
    building_type type = building_type::extraction_site;
    resource_type target = resource_type::iron_ore;
    std::uint16_t recipe = no_recipe;
    std::string   name;
    const char*   kind = "extract";
};

std::vector<option_row> finder_rows(const world& w, const recipe_registry& reg, entity_id actor)
{
    std::vector<option_row> out;
    if (reg.building_available(building_type::extraction_site))
        for (const resource_type er : placement_rules::k_extractable)
            out.push_back({building_type::extraction_site, er, no_recipe, resource_names::name_of(er), "extract"});
    if (reg.building_available(building_type::processing_facility))
        for (int ri = 0; ri < reg.recipe_count(building_type::processing_facility); ++ri)
        {
            const recipe& browsed = reg.recipe_at(building_type::processing_facility, ri);
            const std::uint16_t id = reg.recipe_id(browsed.name);
            if (!recipe_unlocked(w, reg, actor, id)) continue;
            out.push_back({building_type::processing_facility, resource_type::iron_ore, id, browsed.name, "process"});
        }
    return out;
}

/// A read-only tick hook: each major's treasury after each lap, so money that moved
/// outside the quarterly return can be placed in the tick.
struct lap_reader
{
    const std::vector<entity_id>* majors = nullptr;
    std::map<entity_id, std::array<double, k_campaign_settle_lap_count>> treasury;
    static void read(const world& w, int lap, void* ctx)
    {
        auto* self = static_cast<lap_reader*>(ctx);
        if (lap < 0 || lap >= k_campaign_settle_lap_count) return;
        for (const entity_id nid : *self->majors)
            if (const auto it = w.nations.find(nid); it != w.nations.end())
                self->treasury[nid][static_cast<std::size_t>(lap)] = it->second.treasury;
    }
};

/// Steel on a major's own markets: the materials lever spends this shelf. Inventory,
/// last clear's supply and demand are summed over the distinct markets on its ground;
/// price is the mean over them.
void steel_row(const world& w, int seed, entity_id nid, const nation_component& n, int q)
{
    std::set<entity_id> mkts;
    for (const entity_id tid : n.tiles)
        if (const entity_id mid = market_for_tile(w, tid); mid != null_entity) mkts.insert(mid);
    const std::size_t g = static_cast<std::size_t>(resource_type::steel);
    double inv = 0.0, sup = 0.0, dem = 0.0, price = 0.0, base = 0.0;
    for (const entity_id mid : mkts)
    {
        const auto m = w.markets.find(mid);
        if (m == w.markets.end()) continue;
        inv += m->second.inventory[g];
        sup += m->second.supply[g];
        dem += m->second.demand[g];
        price += m->second.price[g];
        base += m->second.base_price[g];
    }
    const double k = mkts.empty() ? 1.0 : static_cast<double>(mkts.size());
    std::printf("STEEL seed=%d nation=%llu q=%d markets=%zu inventory=%.1f supply=%.1f demand=%.1f price=%.2f base=%.2f\n",
                seed, static_cast<unsigned long long>(nid), q, mkts.size(), inv, sup, dem, price / k, base / k);
    // Every good the same way, so a materials bill can be read against what the shelves hold.
    for (std::size_t gi = 0; gi < resource_count; ++gi)
    {
        double ginv = 0.0, gsup = 0.0, gdem = 0.0, gprice = 0.0;
        for (const entity_id mid : mkts)
        {
            const auto m = w.markets.find(mid);
            if (m == w.markets.end()) continue;
            ginv += m->second.inventory[gi];
            gsup += m->second.supply[gi];
            gdem += m->second.demand[gi];
            gprice += m->second.price[gi];
        }
        std::printf("GOOD seed=%d nation=%llu q=%d good=%s inventory=%.1f supply=%.1f demand=%.1f price=%.2f\n", seed,
                    static_cast<unsigned long long>(nid), q, resource_names::name_of(static_cast<resource_type>(gi)).c_str(),
                    ginv, gsup, gdem, gprice / k);
    }
}

/// Years for a net per quarter to repay a capex; -1 when it never does.
double payback_years(double capex, double net_per_quarter)
{
    return net_per_quarter > 0.0 ? capex / (net_per_quarter * 4.0) : -1.0;
}

void site_census(world& w, const recipe_registry& reg, int seed, int tt, int mt, entity_id nid, const nation_component& n)
{
    const float reach = reg.construction().max_logistics_reach;
    lp_reach_memo memo;
    std::map<entity_id, float> cover;
    for (const entity_id tid : n.tiles)
    {
        const auto t = w.tiles.find(tid);
        if (t != w.tiles.end())
            cover[tid] = lp_route_to(w, memo, nid, t->second.body, tid).cover;
    }
    for (const option_row& o : finder_rows(w, reg, nid))
    {
        const placement_gate gate = reg.placement_gate_for(o.type, o.recipe);
        const recipe* rc = (o.type == building_type::processing_facility) ? reg.get_recipe(o.recipe) : nullptr;
        struct site { double net; double capex; double quote; int starved; double materials; };
        site_quote_context cx = make_site_quote_context(w, reg, nullptr);
        std::vector<site> sites;
        int uncovered = 0;
        for (const entity_id tid : n.tiles)
        {
            if (rc != nullptr && !recipe_runs_at_tile(w, *rc, tid)) continue;
            if (!placement_rules::can_place_in_world(w, tid, o.type, o.target, reach, nid, gate)) continue;
            const float cv = cover[tid];
            if (!(cv > 0.0f)) ++uncovered;
            const building_profit bp = estimate_prospective_profit(w, reg, tid, o.type, o.target, o.recipe, nullptr, nullptr, cv);
            if (!bp.has_data) continue;
            const site_quote sq = quote_site(w, reg, cx, tid, o.type, o.target, o.recipe, cv);
            const double capex = construction_capex(w, reg, tid, o.type, o.target, o.recipe);
            // The materials' part of the capex at this tile's market, so the bill can be
            // read as credits and materials. The site multiplier scales both alike.
            double mats = 0.0;
            if (const auto mi = w.markets.find(market_for_tile(w, tid)); mi != w.markets.end())
            {
                const auto& row = reg.resource_build_cost_for(o.type, o.target, o.recipe);
                for (std::size_t g = 0; g < resource_count; ++g)
                    if (row[g] > 0.0f)
                        mats += row[g] * (mi->second.price[g] > 0.0f ? mi->second.price[g] : mi->second.base_price[g]);
            }
            const double authored = reg.live_build_credits(o.type) + mats;
            sites.push_back({static_cast<double>(bp.net()), capex, static_cast<double>(sq.profit.net()), sq.starved ? 1 : 0,
                             authored > 0.0 ? mats * capex / authored : 0.0});
        }
        if (sites.empty()) continue;
        std::sort(sites.begin(), sites.end(), [](const site& a, const site& b) { return a.net > b.net; });
        const site& best = sites.front();
        const site& med  = sites[sites.size() / 2];
        const site& worst = sites.back();
        int positive = 0, starved = 0;
        std::vector<double> quotes;
        for (const site& s : sites) { if (s.net > 0.0) ++positive; starved += s.starved; quotes.push_back(s.quote); }
        std::sort(quotes.begin(), quotes.end(), std::greater<double>());
        std::printf("SITE seed=%d pair=%d:%d nation=%llu kind=%s name=%s sites=%zu positive=%d uncovered=%d"
                    " best=%.2f med=%.2f worst=%.2f capex_best=%.1f capex_med=%.1f mats_best=%.1f mats_med=%.1f payback_best=%.2f payback_med=%.2f qbest=%.2f qmed=%.2f starved=%d\n",
                    seed, tt, mt, static_cast<unsigned long long>(nid), o.kind, o.name.c_str(), sites.size(), positive,
                    uncovered, best.net, med.net, worst.net, best.capex, med.capex, best.materials, med.materials,
                    payback_years(best.capex, best.net), payback_years(med.capex, med.net),
                    quotes.front(), quotes[quotes.size() / 2], starved);
    }
}
} // namespace

int main(int argc, char** argv)
{
    std::vector<int> seeds{ 0 };
    int tt = 30, mt = 4, quarters = 80;
    bool sites = true;
    float chest_override = -1.0f; // --chest N: the kit's handoff figure, for a ladder
    std::string kit_override;     // --kit NAME: another configuration of the kit table
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = parse_ints(argv[++i]);
        else if (!std::strcmp(argv[i], "--quarters") && i + 1 < argc) quarters = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--no-sites")) sites = false;
        else if (!std::strcmp(argv[i], "--kit") && i + 1 < argc) kit_override = argv[++i];
        else if (!std::strcmp(argv[i], "--chest") && i + 1 < argc) chest_override = static_cast<float>(std::atof(argv[++i]));
        else if (!std::strcmp(argv[i], "--pairs") && i + 1 < argc)
        {
            const std::vector<int> nums = [&] {
                std::string s = argv[++i];
                std::replace(s.begin(), s.end(), ':', ',');
                return parse_ints(s.c_str());
            }();
            if (nums.size() != 2) { std::fprintf(stderr, "--pairs takes one total:majors pair\n"); return 2; }
            tt = nums[0]; mt = nums[1];
        }
        else
        {
            std::fprintf(stderr, "usage: build_payback_census [--seeds a,b] [--pairs 30:4] [--quarters N] [--no-sites]\n");
            return 2;
        }
    }

    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
    recipe_registry  reg;
    lua.load("scripts/world_gen.lua");
    cfg.load_from_lua(lua);
    if (!kit_override.empty()) cfg.opening_kit.active = kit_override;
    if (chest_override >= 0.0f)
        for (auto& kv : cfg.opening_kit.configs)
            kv.second.handoff_treasury = chest_override;
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    reg.load_from_lua(lua);

    std::printf("build_payback_census - BL-1496: %d:%d over %d play quarters, nobody seated\n", tt, mt, quarters);
    for (const int seed : seeds)
    {
        world_params p = arc_params(world_arc::shipped);
        p.seed                = static_cast<uint32_t>(seed);
        p.nation_total_target = tt;
        p.major_target        = mt;
        generation_report rep{};
        world w = make_hard_coded_world(p, &rep, cfg, nullptr, &works);
        recipe_registry seed_reg = reg;
        (void)finish_campaign_world(w, rep, seed_reg, p, cfg, nullptr);

        entity_id home = null_entity;
        for (const generation_report::body_entry& b : rep.bodies)
            if (b.is_homeworld) { home = b.id; break; }
        std::vector<entity_id> majors;
        for (const auto& [nid, n] : w.nations)
        {
            if (!n.is_major || n.tiles.empty()) continue;
            const auto t = w.tiles.find(n.tiles.front());
            if (t != w.tiles.end() && t->second.body == home) majors.push_back(nid);
        }
        std::sort(majors.begin(), majors.end());

        if (sites)
            for (const entity_id nid : majors)
                site_census(w, seed_reg, seed, tt, mt, nid, w.nations.at(nid));

        for (const entity_id nid : majors)
        {
            steel_row(w, seed, nid, w.nations.at(nid), 0);
            int built = 0, constructing = 0, idled = 0;
            for (const entity_id a : w.nations.at(nid).state_assets)
            {
                const auto b = w.buildings.find(a);
                if (b == w.buildings.end()) continue;
                if (b->second.decommissioned) ++idled;
                else if (b->second.ticks_remaining > 0) ++constructing;
                else ++built;
            }
            std::printf("HANDOFF seed=%d nation=%llu treasury=%.1f built=%d constructing=%d idled=%d refmine=%.1f\n", seed,
                        static_cast<unsigned long long>(nid), static_cast<double>(w.nations.at(nid).treasury), built, constructing, idled,
                        w.nations.at(nid).tiles.empty() ? 0.0
                            : static_cast<double>(construction_capex(w, seed_reg, w.nations.at(nid).tiles.front(), building_type::extraction_site,
                                                                     resource_type::iron_ore, no_recipe)));
        }
        std::map<entity_id, double> last;
        for (const entity_id nid : majors) last[nid] = w.nations.at(nid).treasury;
        constexpr int k_econ_tick_days = 90;
        // The holdout: a building the moment it is ordered is a real answer to "what
        // would a site earn", with its output not yet in any market. Quote it then, read
        // what it earns a year after it stands.
        struct watched { int ordered; std::string name; double old_quote; double new_quote; int starved; double share; int age; double earned; double run; };
        std::map<entity_id, watched> watch;
        for (int k = 1; k <= quarters; ++k)
        {
            const int day = k * k_econ_tick_days;
            advance_orbits(w, static_cast<double>(k_econ_tick_days));
            advance_surveys(w, k_econ_tick_days);
            w.current_day_tick = day;
            lap_reader laps;
            laps.majors = &majors;
            settle_tick_hooks hooks;
            hooks.after_lap = &lap_reader::read;
            hooks.ctx = &laps;
            const std::map<entity_id, double> before = last;
            const settle_tick_result tick = run_settle_tick(w, seed_reg, k_campaign_settle_ticks + (k - 1), day,
                                                            /*spectating=*/true, &hooks);
            // What the report can name of the money that left outside the return.
            std::map<entity_id, double> research_spend, research_share, research_owed, research_rp, budget_paid;
            for (const research_accrual& a : tick.report.research)
            {
                research_spend[a.nation] += a.line_spend;
                research_share[a.nation] += a.line_share;
                research_owed[a.nation] += static_cast<double>(a.owed_milli) / 1000.0;
                research_rp[a.nation] += static_cast<double>(a.rp_milli) / 1000.0;
            }
            for (const nation_budget_result& b : tick.report.national_budget.nations) budget_paid[b.nation] += b.paid;
            for (const entity_id nid : majors)
            {
                double prev = before.at(nid);
                std::printf("LAPS seed=%d nation=%llu q=%d research=%.1f research_share=%.1f owed_rp=%.1f rp=%.1f budget=%.1f", seed,
                            static_cast<unsigned long long>(nid), k, research_spend[nid], research_share[nid], research_owed[nid], research_rp[nid], budget_paid[nid]);
                for (int l = 0; l < k_campaign_settle_lap_count; ++l)
                {
                    const double now = laps.treasury[nid][static_cast<std::size_t>(l)];
                    std::printf(" l%d=%.1f", l, now - prev);
                    prev = now;
                }
                std::printf("\n");
            }
            {
                site_quote_context hcx = make_site_quote_context(w, seed_reg, &tick.report);
                for (const auto& [a, bc] : w.buildings)
                {
                    if (bc.type != building_type::processing_facility && bc.type != building_type::extraction_site) continue;
                    auto wit = watch.find(a);
                    if (bc.ticks_remaining > 0)
                    {
                        if (wit != watch.end()) continue;
                        const recipe* rc = seed_reg.get_recipe(bc.recipe);
                        const building_profit old_q = estimate_prospective_profit(w, seed_reg, bc.tile, bc.type, bc.target_resource, bc.recipe);
                        const site_quote nq = quote_site(w, seed_reg, hcx, bc.tile, bc.type, bc.target_resource, bc.recipe, 1.0f, a);
                        watch[a] = watched{k, rc != nullptr ? rc->name : std::string(resource_names::name_of(bc.target_resource)),
                                           old_q.net(), nq.profit.net(), nq.starved ? 1 : 0, nq.price_share, 0, 0.0, 0.0};
                        continue;
                    }
                    if (wit == watch.end()) continue;
                    watched& h = wit->second;
                    if (h.age >= 8) continue;
                    ++h.age;
                    if (h.age > 4)
                    {
                        h.earned += estimate_building_profit(w, seed_reg, tick.report, a).net();
                        for (const building_report& br : tick.report.buildings)
                            if (br.building == a) { h.run += br.run; break; }
                    }
                    if (h.age == 8)
                        std::printf("HOLDOUT seed=%d q=%d name=%s old=%.2f new=%.2f starved=%d share=%.3f earned=%.2f run=%.2f kind=%d\n",
                                    seed, h.ordered, h.name.c_str(), h.old_quote, h.new_quote, h.starved, h.share, h.earned / 4.0, h.run / 4.0, bc.type == building_type::processing_facility ? 1 : 0);
                }
            }
            // What a standing plant truly earned, beside what the door would quote.
            if (k == 4 || k == 20)
                for (const entity_id nid : majors)
                {
                    const auto nit = w.nations.find(nid);
                    if (nit == w.nations.end()) continue;
                    for (const entity_id a : nit->second.state_assets)
                    {
                        const auto b = w.buildings.find(a);
                        if (b == w.buildings.end() || b->second.ticks_remaining > 0
                            || b->second.type != building_type::processing_facility) continue;
                        const recipe* rc = seed_reg.get_recipe(b->second.recipe);
                        if (rc == nullptr) continue;
                        const building_profit actual = estimate_building_profit(w, seed_reg, tick.report, a);
                        const building_profit quoted = estimate_prospective_profit(
                            w, seed_reg, b->second.tile, b->second.type, b->second.target_resource, b->second.recipe);
                        float run = 0.0f;
                        for (const building_report& br : tick.report.buildings)
                            if (br.building == a) { run = br.run; break; }
                        std::printf("ACTUAL seed=%d nation=%llu q=%d name=%s idled=%d run=%.2f actual=%.2f quoted=%.2f\n",
                                    seed, static_cast<unsigned long long>(nid), k, rc->name.c_str(),
                                    b->second.decommissioned ? 1 : 0, run, actual.net(), quoted.net());
                    }
                }

            if (k == 4 || k == 8 || k == 20 || k == 40 || k == 80)
                for (const entity_id nid : majors)
                    if (const auto it = w.nations.find(nid); it != w.nations.end())
                        steel_row(w, seed, nid, it->second, k);
            std::map<entity_id, std::pair<int, long>> force;
            for (const auto& [uid, u] : w.units)
            {
                auto& f = force[u.owner];
                ++f.first;
                f.second += u.count;
            }
            for (const entity_id nid : majors)
            {
                const auto it = w.nations.find(nid);
                if (it == w.nations.end() || it->second.returns.empty()) continue;
                const nation_component& n = it->second;
                const quarterly_return& r = n.returns.back();
                int built = 0, constructing = 0, idled = 0, stretched = 0, paused = 0;
                for (const entity_id a : n.state_assets)
                {
                    const auto b = w.buildings.find(a);
                    if (b == w.buildings.end()) continue;
                    if (b->second.decommissioned) ++idled;
                    else if (b->second.ticks_remaining > 0)
                    {
                        ++constructing;
                        // The rate this quarter: 0 is paused, under full is stretched.
                        if (b->second.build_rate_permille <= 0) ++paused;
                        else if (b->second.build_rate_permille < 1000) ++stretched;
                    }
                    else ++built;
                }
                const double delta = static_cast<double>(n.treasury) - last[nid];
                const double resid = n.returns.size() >= 2
                    ? static_cast<double>(r.balance) - n.returns[n.returns.size() - 2].balance - r.net
                    : 0.0;
                last[nid] = n.treasury;
                const auto f = force.find(nid);
                std::printf("QTR seed=%d nation=%llu q=%d treasury=%.1f delta=%.1f net=%.1f resid=%.1f income=%.1f"
                            " expenditure=%.1f maintenance=%.1f wages=%.1f upkeep=%.1f taxes=%.1f refunds=%.1f research=%.1f"
                            " built=%d constructing=%d idled=%d stretched=%d paused=%d refmine=%.1f units=%d men=%ld\n",
                            seed, static_cast<unsigned long long>(nid), k, static_cast<double>(n.treasury), delta, r.net,
                            resid, r.income, r.expenditure, r.maintenance, r.wages, r.upkeep, r.taxes, r.refunds, r.research,
                            built, constructing, idled, stretched, paused,
                            n.tiles.empty() ? 0.0
                                            : static_cast<double>(construction_capex(w, seed_reg, n.tiles.front(), building_type::extraction_site,
                                                                                    resource_type::iron_ore, no_recipe)),
                            f == force.end() ? 0 : f->second.first,
                            f == force.end() ? 0L : f->second.second);
            }
        }
        std::fflush(stdout);
    }
    return 0;
}
