// campaign_lapse — BL-723. The spectated-campaign measurement instrument.
//
// Runs one spectated campaign (nobody seated, every corp scorer-driven — the
// BL-409/BL-630 reading) under one parameter set, and writes a per-tick CSV
// time series to build_gen/verify/lapse/<tag>/. This is the instrument every
// sweep in BL-724..BL-729 runs on; nothing here asserts an economy magnitude —
// the CSVs are the product, and the only assertions are the T0 validity rows
// (--t0), which prove the instrument itself can be believed:
//
//   T0a  A/A — two same-parameter runs emit BYTE-IDENTICAL CSV content.
//   T0b  differential — a knob that must move the world (population
//        demand_scale x4) moves the measured demand; an instrument no knob can
//        move is not measuring.
//   T0c  zero-observation FAILS — a run that logged no corps, no markets, or
//        zero valued production is a broken instrument, never a quiet pass
//        (the BL-714 discipline: a check proves it LOOKED).
//   T0d  wall-clock budget — a stall is one red row, not a hung suite.
//
// WHY EXTERNAL LOGGING EXISTS AT ALL: quarterly returns retain 40 quarters and
// market_plot_history 64 samples — a campaign-length trend cannot be read from
// in-world stores (BL-723's design).
//
// THE WORLD IS THE SHIPPED ONE (acquisition_viability's R0 lesson, verbatim):
// gen config PARSED and passed, priced-resource count printed in the manifest;
// spawn order: make_hard_coded_world -> assign_default_recipes ->
// generate_background_firms -> assign_default_recipes; the tick is
// app::step_economy's order. Spectating stays TRUE for warm start AND the
// measured window — the field is the subject; nobody is seated.
//
// PARAMETER OVERRIDES take the demand_census --reach pattern (NR-763): applied
// AFTER load_from_lua so the override is the last word, echoed in the manifest
// so a probe run can never be mistaken for a shipped one. The authored Lua is
// never touched.
//
// PROXY BRACKETS (NR-774), carried in the manifest because every report quotes
// them: the corp AI acts once per corp per cadence_k ticks (a human acts every
// tick — pessimistic), and under spectate every corp can claim nation budget
// lines (a played player-corp never is a claimant — optimistic).
//
// Usage:
//   campaign_lapse [--seed N] [--ticks N] [--warm N] [--fast] [--epoch Y]
//                  [--tag NAME] [--pop-scale F] [--bg-scale F] [--t0]
//   --fast       zero the pre-epoch prehistory (NOT the shipped spawn; iteration
//                only — the manifest says so).
//   --pop-scale  multiply population_demand.demand_scale (override, echoed).
//   --bg-scale   multiply background_demand.demand_scale (override, echoed).
//   --t0         run the validity battery instead of a campaign; small ticks.
//
// Outputs (build_gen/verify/lapse/<tag>/):
//   manifest.txt     every parameter, override, and bracket, stated
//   corps_index.csv  corp,name,focus,home_nation,is_background
//   corps.csv        tick,corp,balance,income,expenditure,maintenance,wages,
//                    interest,levies,upkeep,subsidies,net,holdings,
//                    footprint_tiles,market_share
//   markets.csv      tick,market,body,resource,price,base_price,supply,demand,
//                    shortfall            (priced resources only)
//   world.csv        tick,valued_production,exchange_revenue,convoys,
//                    buildings_active,buildings_idle,corps,corps_in_debt,
//                    hostile_pairs,friend_pairs,sell_orders,treasury_sum,
//                    state_purchase_qty
//
// Exits 0 on success (campaign mode: CSVs written; --t0: all rows PASS).

#include "scripting/lua_state.hpp"

#include "harness_params.hpp"
#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/market_clearing.hpp"
#include "world/nation_step.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/standing.hpp"
#include "world/supply_system.hpp"
#include "world/tech_gate.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* row, const char* what)
{
    std::printf("  [%s] %-3s %s\n", ok ? "PASS" : "FAIL", row, what);
    if (!ok)
        ++g_failures;
}

// ---------------------------------------------------------------------------
// Parameters — one struct, echoed whole in the manifest
// ---------------------------------------------------------------------------

struct lapse_params
{
    uint32_t    seed        = 0;
    int         warm_ticks  = 80;    ///< app::pre_game_ticks.
    int         ticks       = 120;   ///< measured window (30 years of quarters).
    bool        prehistory  = true;  ///< the shipped spawn; --fast zeroes it.
    int         epoch_year  = 0;     ///< 0 = leave world_params' own default.
    float       pop_scale   = 1.0f;  ///< population demand_scale multiplier.
    float       bg_scale    = 1.0f;  ///< background demand_scale multiplier.
    abundance_level abundance = abundance_level::standard; ///< BL-724's scarcity axis.
    std::string tag         = "run";
    double      budget_s    = 900.0; ///< T0d wall-clock ceiling for one rollout.
};

/// One economy tick in app::step_economy's order — acquisition_viability's own
/// loop, spectating always true here.
std::unordered_map<entity_id, corp_cash_flow>
tick(world& w, const recipe_registry& reg, int t, economy_report& rep_out)
{
    w.current_econ_tick = t;
    w.current_day_tick  = t;
    lp_pool_map lp;
    dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                     reg.logistics_cost(convoy_mode::space), &lp);
    advance_convoys(w);
    economy_report rep = run_economy_step(w, reg, /*spectating=*/true, &lp);
    auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention, &rep.budgets, &rep.buildings,
                 &rep.building_labour);
    run_nation_step(w, reg, rep, t);
    advance_tech_gates(w);
    credit_arrived_convoys(w, t);
    // BL-743: the insolvency wind-up, last — app::step_economy's own order.
    run_firm_exits(w, reg.firm_exit(), &rep.firm_exits);
    rep_out = std::move(rep);
    return flows;
}

std::vector<entity_id> sorted_corp_ids(const world& w)
{
    std::vector<entity_id> ids;
    ids.reserve(w.corporations.size());
    for (const auto& kv : w.corporations)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<entity_id> sorted_market_ids(const world& w)
{
    std::vector<entity_id> ids;
    ids.reserve(w.markets.size());
    for (const auto& kv : w.markets)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

// ---------------------------------------------------------------------------
// Metric derivations — every walk sorted, every sum accumulated in doubles
// ---------------------------------------------------------------------------

/// Valued production this tick: Σ over building rows of output × the resolved
/// price at the building's own catchment market — NR-774's GDP definition.
/// Extraction values its target resource; a processor's summed output is
/// decomposed through its recipe's own output vector (scale = reported total /
/// authored total), so a two-output recipe is valued per good, never at one
/// good's price. Unpriced goods (base_price 0 at that market) value at the
/// resolved price on the row, which is 0 for a good no market prices — an
/// unpriced good contributes nothing, exactly as it earns nothing.
double valued_production(const world& w, const recipe_registry& reg,
                         const economy_report& rep)
{
    double total = 0.0;
    for (const building_report& br : rep.buildings)   // deterministic pass order
    {
        if (!(br.output_quantity > 0.0f))
            continue;
        const auto bit = w.buildings.find(br.building);
        if (bit == w.buildings.end())
            continue;
        const entity_id mid = market_for_tile(w, bit->second.tile);
        if (mid == null_entity)
            continue;   // marketless body: produced but priced nowhere
        const market_component& mc = w.markets.at(mid);

        if (br.type == building_type::extraction_site)
        {
            total += static_cast<double>(br.output_quantity)
                   * static_cast<double>(mc.price[static_cast<std::size_t>(br.target_resource)]);
            continue;
        }
        const recipe* rc = reg.get_recipe(br.recipe);
        if (rc == nullptr)
            continue;
        double authored_sum = 0.0;
        for (std::size_t r = 0; r < resource_count; ++r)
            authored_sum += static_cast<double>(rc->outputs[r]);
        if (!(authored_sum > 0.0))
            continue;
        const double scale = static_cast<double>(br.output_quantity) / authored_sum;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (rc->outputs[r] > 0.0f)
                total += static_cast<double>(rc->outputs[r]) * scale
                       * static_cast<double>(mc.price[r]);
    }
    return total;
}

/// Revenue of every exchange the ring holds for tick @p t (the ring is
/// chronological; a campaign tick's rows are contiguous but we filter by field,
/// not position, so a wrapped ring reads the same).
double exchange_revenue_at(const world& w, int t)
{
    double total = 0.0;
    for (std::size_t i = 0; i < w.exchanges.size(); ++i)
    {
        const exchange_record& e = w.exchanges.oldest_first(i);
        if (e.tick == t)
            total += static_cast<double>(e.quantity) * static_cast<double>(e.unit_price);
    }
    return total;
}

int distinct_footprint_tiles(const world& w, const corporation_component& cc)
{
    std::set<entity_id> tiles;
    for (const entity_id bid : cc.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit != w.buildings.end())
            tiles.insert(bit->second.tile);
    }
    return static_cast<int>(tiles.size());
}

// ---------------------------------------------------------------------------
// The rollout — one campaign, three CSVs accumulated as strings
// ---------------------------------------------------------------------------

struct rollout_result
{
    std::string corps_index;
    std::string corps_csv;
    std::string markets_csv;
    std::string world_csv;
    int         priced        = 0;      ///< gen-config priced-resource count
    double      final_valued  = 0.0;    ///< last tick's valued production
    double      final_demand  = 0.0;    ///< last tick's summed market demand
    int         corp_rows     = 0;
    int         market_rows   = 0;
    double      elapsed_s     = 0.0;
};

void appendf(std::string& s, const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    s += buf;
}

rollout_result run_rollout(const lapse_params& lp, const recipe_registry& reg,
                           const world_gen_config& gen_cfg)
{
    const auto t0 = std::chrono::steady_clock::now();
    rollout_result out;

    world_params p;
    p.seed      = lp.seed;
    p.abundance = lp.abundance;
    if (lp.epoch_year != 0)
        p.epoch_year = lp.epoch_year;
    if (!lp.prehistory)
        p = no_prehistory(p);

    for (std::size_t i = 0; i < resource_count; ++i)
        if (gen_cfg.kepler_base_price[i] > 0.0f)
            ++out.priced;

    // The shipped spawn's own order (acquisition_viability's header).
    world w = make_hard_coded_world(p, nullptr, gen_cfg);
    assign_default_recipes(w, reg);
    generate_background_firms(w, reg, lp.seed ^ 0x8A21F00Du);
    assign_default_recipes(w, reg);

    // The corp index, once — ids and identities never change mid-campaign.
    out.corps_index = "corp,name,focus,home_nation,is_background\n";
    for (const entity_id id : sorted_corp_ids(w))
    {
        const corporation_component& cc = w.corporations.at(id);
        const char* focus =
            cc.focus == industrial_focus::extraction ? "extraction" :
            cc.focus == industrial_focus::processing ? "processing" : "trade";
        appendf(out.corps_index, "%llu,%s,%s,%llu,%d\n",
                static_cast<unsigned long long>(id), cc.name.c_str(), focus,
                static_cast<unsigned long long>(cc.home_nation),
                cc.is_background ? 1 : 0);
    }

    out.corps_csv = "tick,corp,balance,income,expenditure,maintenance,wages,interest,"
                    "levies,upkeep,subsidies,net,holdings,footprint_tiles,market_share\n";
    out.markets_csv = "tick,market,body,resource,price,base_price,supply,demand,shortfall\n";
    out.world_csv = "tick,valued_production,exchange_revenue,convoys,buildings_active,"
                    "buildings_idle,corps,corps_in_debt,hostile_pairs,friend_pairs,"
                    "sell_orders,treasury_sum,state_purchase_qty,firm_exits\n";

    const int total_ticks = lp.warm_ticks + lp.ticks;
    for (int t = 1; t <= total_ticks; ++t)
    {
        economy_report rep;
        auto flows = tick(w, reg, t, rep);
        if (t <= lp.warm_ticks)
            continue;   // the warm start settles; the measured window logs

        // --- per-corp rows (rep.budgets is a std::map — already sorted) -----
        const std::vector<corp_standing> standings = compute_corp_standings(w, flows);
        std::unordered_map<entity_id, float> share;
        for (const corp_standing& s : standings)
            share[s.corp] = s.market_share;

        for (const entity_id id : sorted_corp_ids(w))
        {
            const corporation_component& cc = w.corporations.at(id);
            corp_budget b{};
            const auto bit = rep.budgets.find(id);
            if (bit != rep.budgets.end())
                b = bit->second;
            const auto sit = share.find(id);
            appendf(out.corps_csv,
                    "%d,%llu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%d,%.6f\n",
                    t, static_cast<unsigned long long>(id),
                    static_cast<double>(cc.balance),
                    static_cast<double>(b.income), static_cast<double>(b.expenditure),
                    static_cast<double>(b.maintenance), static_cast<double>(b.wages),
                    static_cast<double>(b.interest), static_cast<double>(b.levies),
                    static_cast<double>(b.upkeep), static_cast<double>(b.subsidies),
                    static_cast<double>(b.net()),
                    static_cast<int>(cc.assets.size()),
                    distinct_footprint_tiles(w, cc),
                    sit == share.end() ? 0.0 : static_cast<double>(sit->second));
            ++out.corp_rows;
        }

        // --- per-market rows, priced resources only -------------------------
        double demand_sum = 0.0;
        for (const entity_id mid : sorted_market_ids(w))
        {
            const market_component& mc = w.markets.at(mid);
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                if (!(mc.base_price[r] > 0.0f))
                    continue;
                const double shortfall =
                    std::max(0.0, static_cast<double>(mc.demand[r]) -
                                  static_cast<double>(mc.supply[r]));
                demand_sum += static_cast<double>(mc.demand[r]);
                appendf(out.markets_csv, "%d,%llu,%llu,%s,%.4f,%.4f,%.4f,%.4f,%.4f\n",
                        t, static_cast<unsigned long long>(mid),
                        static_cast<unsigned long long>(mc.body),
                        resource_names::name_of(static_cast<resource_type>(r)).c_str(),
                        static_cast<double>(mc.price[r]),
                        static_cast<double>(mc.base_price[r]),
                        static_cast<double>(mc.supply[r]),
                        static_cast<double>(mc.demand[r]), shortfall);
                ++out.market_rows;
            }
        }

        // --- the world row ---------------------------------------------------
        int active = 0, idle = 0;
        for (const building_report& br : rep.buildings)
        {
            if (br.active) ++active;
            if (br.idle)   ++idle;
        }
        int in_debt = 0;
        for (const entity_id id : sorted_corp_ids(w))
            if (w.corporations.at(id).balance < 0.0f)
                ++in_debt;
        double treasury_sum = 0.0;
        {
            std::vector<entity_id> nids;
            for (const auto& kv : w.nations)
                nids.push_back(kv.first);
            std::sort(nids.begin(), nids.end());
            for (const entity_id n : nids)
                treasury_sum += static_cast<double>(w.nations.at(n).treasury);
        }
        double state_qty = 0.0;
        for (const space_purchase& sp : rep.space_purchases)
            if (sp.completed)
                state_qty += static_cast<double>(sp.quantity);

        const double valued = valued_production(w, reg, rep);
        appendf(out.world_csv,
                "%d,%.4f,%.4f,%d,%d,%d,%d,%d,%d,%d,%d,%.4f,%.4f,%d\n",
                t, valued, exchange_revenue_at(w, t),
                static_cast<int>(w.convoys.size()), active, idle,
                static_cast<int>(w.corporations.size()), in_debt,
                static_cast<int>(w.corp_hostile_pairs.size()),
                static_cast<int>(w.corp_friend_pairs.size()),
                static_cast<int>(w.sell_orders.size()), treasury_sum, state_qty,
                static_cast<int>(rep.firm_exits.size()));

        if (t == total_ticks)
        {
            out.final_valued = valued;
            out.final_demand = demand_sum;
        }
    }

    out.elapsed_s = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t0).count();
    return out;
}

// ---------------------------------------------------------------------------
// The data layer — loaded once, overrides applied AFTER (the --reach pattern)
// ---------------------------------------------------------------------------

struct data_layer
{
    recipe_registry  reg;
    world_gen_config gen_cfg{};
};

bool load_data(data_layer& d, const lapse_params& lp)
{
    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");
    d.reg.load_from_lua(lua);
    d.gen_cfg.load_from_lua(lua);

    world_params probe;
    const int epoch = lp.epoch_year != 0 ? lp.epoch_year : probe.epoch_year;
    d.reg.set_era(era_band_for_epoch(epoch));

    // Overrides: last word, echoed in the manifest, authored Lua untouched.
    if (lp.pop_scale != 1.0f)
    {
        population_demand_params p = d.reg.population_demand();
        p.demand_scale *= lp.pop_scale;
        d.reg.set_population_demand(p);
    }
    if (lp.bg_scale != 1.0f)
    {
        background_demand_params b = d.reg.background_demand();
        b.demand_scale *= lp.bg_scale;
        d.reg.set_background_demand(b);
    }

    if (d.reg.economics(building_type::extraction_site).maintenance <= 0.0f)
    {
        std::printf("FATAL: scripts/economy.lua did not load.\n");
        return false;
    }
    return true;
}

void write_file(const std::filesystem::path& p, const std::string& content)
{
    std::FILE* f = std::fopen(p.string().c_str(), "wb");
    if (f == nullptr)
    {
        std::printf("FATAL: cannot write %s\n", p.string().c_str());
        std::exit(2);
        return;
    }
    std::fwrite(content.data(), 1, content.size(), f);
    std::fclose(f);
}

} // namespace

int main(int argc, char** argv)
{
    lapse_params lp;
    bool t0_mode = false;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
            lp.seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        else if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc)
            lp.ticks = std::max(1, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--warm") == 0 && i + 1 < argc)
            lp.warm_ticks = std::max(0, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--epoch") == 0 && i + 1 < argc)
            lp.epoch_year = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--fast") == 0)
            lp.prehistory = false;
        else if (std::strcmp(argv[i], "--pop-scale") == 0 && i + 1 < argc)
            lp.pop_scale = static_cast<float>(std::atof(argv[++i]));
        else if (std::strcmp(argv[i], "--bg-scale") == 0 && i + 1 < argc)
            lp.bg_scale = static_cast<float>(std::atof(argv[++i]));
        else if (std::strcmp(argv[i], "--abundance") == 0 && i + 1 < argc)
        {
            ++i;
            if (std::strcmp(argv[i], "sparse") == 0)        lp.abundance = abundance_level::sparse;
            else if (std::strcmp(argv[i], "lean") == 0)     lp.abundance = abundance_level::lean;
            else if (std::strcmp(argv[i], "standard") == 0) lp.abundance = abundance_level::standard;
            else { std::printf("usage: --abundance sparse|lean|standard\n"); return 2; }
        }
        else if (std::strcmp(argv[i], "--tag") == 0 && i + 1 < argc)
            lp.tag = argv[++i];
        else if (std::strcmp(argv[i], "--t0") == 0)
            t0_mode = true;
    }

    std::printf("campaign_lapse — BL-723, the spectated-campaign instrument\n");

    if (t0_mode)
    {
        // ===================================================================
        // T0 — the validity battery. Small, fast, and about the INSTRUMENT.
        // ===================================================================
        lapse_params small = lp;
        small.prehistory = false;   // T0 is about the instrument, not the spawn
        small.warm_ticks = 6;
        small.ticks      = 8;

        data_layer d;
        if (!load_data(d, small))
            return 2;

        const auto t_start = std::chrono::steady_clock::now();

        std::printf("\n=== T0  VALIDITY ===\n");
        rollout_result a = run_rollout(small, d.reg, d.gen_cfg);
        rollout_result b = run_rollout(small, d.reg, d.gen_cfg);
        check(a.corps_csv == b.corps_csv && a.markets_csv == b.markets_csv
                  && a.world_csv == b.world_csv && a.corps_index == b.corps_index,
              "T0a", "A/A: two same-parameter rollouts emit byte-identical CSVs");

        lapse_params moved = small;
        moved.pop_scale = 4.0f;
        data_layer d2;
        if (!load_data(d2, moved))
            return 2;
        rollout_result c = run_rollout(moved, d2.reg, d2.gen_cfg);
        check(c.final_demand > a.final_demand * 1.05,
              "T0b", "differential: population demand_scale x4 moves measured demand "
                     "(an instrument no knob can move is not measuring)");

        check(a.corp_rows > 0 && a.market_rows > 0,
              "T0c", "zero-observation fails: corp and market rows were actually logged");
        check(a.final_valued > 0.0,
              "T0c", "zero-observation fails: valued production is non-zero at the "
                     "final tick");
        check(a.priced > 10,
              "T0c", "the gen config was PARSED (priced resources exceed the C++ "
                     "fallback's ten)");

        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_start).count();
        check(elapsed < lp.budget_s,
              "T0d", "wall-clock budget: the battery finished inside its ceiling");
        std::printf("  battery wall clock: %.1f s (ceiling %.0f s)\n", elapsed, lp.budget_s);

        std::printf("\n%s — %d failure(s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
        return g_failures == 0 ? 0 : 1;
    }

    // =======================================================================
    // Campaign mode — one rollout, CSVs to disk, manifest first
    // =======================================================================
    data_layer d;
    if (!load_data(d, lp))
        return 2;

    const std::filesystem::path dir =
        std::filesystem::path("build_gen") / "verify" / "lapse" / lp.tag;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    rollout_result r = run_rollout(lp, d.reg, d.gen_cfg);

    std::string manifest;
    appendf(manifest, "campaign_lapse manifest — BL-723\n");
    appendf(manifest, "seed=%u\nwarm_ticks=%d\nmeasured_ticks=%d\nprehistory=%s\n",
            lp.seed, lp.warm_ticks, lp.ticks,
            lp.prehistory ? "ON (the shipped spawn)" : "OFF (--fast, NOT the spawn)");
    appendf(manifest, "epoch_year=%s\n",
            lp.epoch_year != 0 ? std::to_string(lp.epoch_year).c_str()
                               : "(world_params default)");
    appendf(manifest, "abundance=%s\n",
            lp.abundance == abundance_level::sparse ? "sparse"
            : lp.abundance == abundance_level::lean ? "lean" : "standard");
    appendf(manifest, "override pop demand_scale x%.3f%s\n",
            static_cast<double>(lp.pop_scale), lp.pop_scale == 1.0f ? " (none)" : "");
    appendf(manifest, "override bg demand_scale x%.3f%s\n",
            static_cast<double>(lp.bg_scale), lp.bg_scale == 1.0f ? " (none)" : "");
    appendf(manifest, "priced_resources=%d of %d (C++ fallback carries 10)\n",
            r.priced, static_cast<int>(resource_count));
    appendf(manifest, "spectating=TRUE throughout (nobody seated; the field is the "
                      "subject)\n");
    appendf(manifest, "proxy brackets (NR-774): cadence pessimism (one strategic batch "
                      "per corp per cadence_k ticks); subsidy optimism (spectated corps "
                      "can claim nation budget lines)\n");
    appendf(manifest, "elapsed_s=%.1f\ncorp_rows=%d\nmarket_rows=%d\n",
            r.elapsed_s, r.corp_rows, r.market_rows);
    write_file(dir / "manifest.txt", manifest);
    write_file(dir / "corps_index.csv", r.corps_index);
    write_file(dir / "corps.csv", r.corps_csv);
    write_file(dir / "markets.csv", r.markets_csv);
    write_file(dir / "world.csv", r.world_csv);

    // The zero-observation guard holds in campaign mode too — a silent empty
    // run must not look like a delivered one.
    check(r.corp_rows > 0 && r.market_rows > 0 && r.final_valued > 0.0,
          "C1", "the campaign logged corps, markets, and non-zero valued production");
    check(r.elapsed_s < lp.budget_s,
          "C2", "the rollout finished inside its wall-clock ceiling");

    std::printf("  wrote %s (%d corp rows, %d market rows, %.1f s)\n",
                dir.string().c_str(), r.corp_rows, r.market_rows, r.elapsed_s);
    std::printf("\n%s — %d failure(s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
