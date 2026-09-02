// material_floor — is the 30% material-maintenance floor a spawn-viability trap?
//
// THE QUESTION, and it is a MEASUREMENT, not a fix. `compute_building_opex`
// (budget_system.cpp, BL-049) charges `e.maintenance * 0.3f` UNCONDITIONALLY —
// when the building is decommissioned, and when its `workforce_target` is 0. Two
// other systems, each correct on its own, can drive a building into exactly that
// state and leave it there:
//
//   * BL-181's workforce auto-solver dials an unprofitable player building to a
//     target of 0, because 0 maximises THAT building's own profit.
//   * BL-079's reflex tier decommissions a rival building that has run at a loss
//     for 8 consecutive ticks.
//
// The hypothesis under test (Ben, 2026-08-27) is that the three compose into a
// trap: a building that produces nothing, pays 30% of its maintenance constant
// forever, and has no exit but a manual demolition that refunds nothing
// (`construction.hpp`). BL-635's diagnosis attributed 60.2% of the seated corp's
// residual operating deficit to "maintenance on buildings that cannot produce",
// which is the same phenomenon seen from the money side.
//
// THIS HARNESS IS NOT ALLOWED TO CONFIRM THAT BY CONSTRUCTION. It measures four
// separable things and reports each with units, so the hypothesis can fail:
//
//   R1  THE CENSUS. Per seed, how many of a corp's buildings end the warm start
//       at workforce_target 0 or decommissioned, and how many of THOSE produced
//       nothing across a long trailing run. A building can be at zero target and
//       still have produced last quarter; a building can be idle for one tick
//       without being dead. The two are counted separately.
//
//   R2  THE PRICE OF THE FLOOR. What the dead set costs per quarter, as an
//       absolute (cr/qtr), as a share of the corp's filed maintenance, and as a
//       share of its filed operating gap. The reconstruction is checked against
//       the LIVE budget: maintenance depends on nothing but the building (not on
//       contention, not on habitability — only wages do), so summing
//       compute_building_opex over `assets` must reproduce the filed maintenance
//       to float epsilon. If it does not, the instrument is wrong and every
//       number below it is void.
//
//   R3  RECOVERY. Is the auto-solver's zero a stable fixed point? The code says
//       it need not be — pass 3 of run_economy_step re-solves EVERY tick from
//       live market state and never reads a sticky flag, so a price move re-dials
//       it up. That is a claim about the code; whether prices move is a claim
//       about the world. So the sweep runs a further `--extra` ticks past the warm
//       start and counts how many dead buildings ever produce again, alongside
//       whether their target resource's clearing price ever leaves the band floor.
//
//   R4  THE FIELD. The same census over the background corporations, because a
//       field paying dead maintenance is a field that cannot act.
//
// ASSERTED vs REPORTED. Magnitudes are REPORTED — a diagnosis cannot be a
// pass/fail, and a bar drawn today is a bar tuned against a story. What IS
// asserted are properties that must hold whatever the constants are:
//   A1  a building at workforce_target 0 pays EXACTLY the same maintenance as the
//       same building decommissioned, and both equal maintenance x 0.3.
//   A2  the reconstruction reconciles with the live filed return (non-vacuity).
//   A3  the census is non-vacuous — the sweep found buildings, and filed returns.
//
// It loads the REAL scripts (recipes/economy/world_gen) through a live Lua state
// for spawn_solvency's reason: this asserts magnitudes, and a restated registry
// would answer a question about a world that does not ship. Build it with
//     cmd //c tools\verify\build_lua_harness.bat material_floor
//
// ---------------------------------------------------------------------------
// WHAT IT MEASURED, 2026-08-27, 8 seeds from 0, prehistory ON, 80 warm + 32
// recovery ticks. Kept here as constants-in-prose rather than in a doc, because
// the numbers ARE the deliverable and prose elsewhere goes stale.
// ---------------------------------------------------------------------------
//
// THE HYPOTHESIS IS CONTRADICTED FOR THE SEATED CORPORATION AND ONLY PARTLY
// SUPPORTED FOR THE FIELD.
//
//   seated: 4.6 holdings, 0.2 floored, 0.1 DEAD per seed. The dead floor costs
//           0.38 cr/qtr = 1.0% of the corp's filed maintenance (38.69 cr/qtr).
//           The corp's operating position is +17.38 cr/qtr, so there is no gap
//           for it to be a share of. Seven of eight seeds carry no dead building
//           at all; the single instance (seed 1) pays 3.00 cr/qtr, 8.6% of that
//           one seed's gap. A floor that costs one percent of maintenance is not
//           a viability factor.
//
//   field:  24.2% of 2,751 rival holdings end the warm start dead, and 58.3% of
//           rivals carry at least one. The dead floor is 2.86 cr/qtr per rival =
//           10.6% of filed maintenance and 35.7% of a -8.00 cr/qtr operating gap.
//           Real, and the largest single named share of that gap — but a third of
//           it, not the whole.
//
//   exit:   0 of 666 dead buildings were demolished, on any seed. That is not a
//           tuning observation: corp_ai.cpp never ENUMERATES a demolish candidate
//           (grep it — the verb appears only in the label switch and the event
//           mapping), so no rival can take the exit, and demolish_building
//           refunds nothing and erases the stockpile, so the player's exit costs
//           more than the floor it escapes.
//
//   the zero is NOT sticky: 339 of 666 were re-dialled above workforce_target 0
//   and 295 resumed from decommission within 32 ticks. Pass 3 of
//   run_economy_step re-solves every tick from live market state and reads no
//   sticky flag, so the fixed point is the economy's, never the code's.
//
// THE INSTRUMENT GAP THIS FOUND, and it is the larger result. Before the fix
// below, the same sweep read: seated 1.5 dead per seed, operating -19.13 cr/qtr,
// field 57.0% dead, dead floor 38.6% of the field gap — numbers that DO support
// the hypothesis. The difference is one argument. `make_hard_coded_world`'s
// `gen_cfg` parameter defaults to a C++ fallback `world_gen_config{}` carrying
// base prices for TEN resources; scripts/world_gen.lua authors 42 of 47. Every
// market is seeded from that table, so a harness that omits the argument
// measures a world where stone, timber, clay, fibre, planks, tools and most of
// the ancient roster are UNPRICED — no market quotes them, no site working them
// can sell a unit at any workforce level, and the solver's zero is forced by an
// absent price rather than a low one. 10 of the 12 seated dead buildings in that
// reading were extraction sites on an unpriced resource; with the table parsed,
// zero are. `--default-cfg` reproduces the old reading for comparison.
//
// spawn_solvency.cpp HAS THE SAME OMISSION and its BL-635 baselines were taken
// with it (it loads world_gen.lua into the Lua state — its own 2026-08-26 fix —
// but never parses a world_gen_config from it, which is the half that matters).
// Not corrected here: those are committed baselines and moving them is not this
// harness's call. Flagged for Ben. Both harnesses also omit the works_registry
// that app.cpp passes to generation (BL-321), a second, unquantified divergence
// from the shipped spawn.
//
// Usage:  material_floor [seed_count] [--seed0 N] [--fast] [--extra N]
//                        [--default-cfg]
//   --fast        prehistory_years = 0. NOT the shipped spawn — iteration only.
//   --extra N     ticks past the warm start for the recovery probe (default 80).
//                 Keep it under 40 (k_quarterly_return_retention) or the
//                 warm-start return has rolled out of the buffer and the A2
//                 reconciliation is skipped rather than run against the wrong
//                 quarter.
//   --default-cfg generate with the C++ fallback config instead of the parsed
//                 one — reproduces the pre-fix reading described above.
//
// Exits 0 on PASS, non-zero on any failed assertion.

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
#include "world/supply_system.hpp"
#include "world/tech_gate.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
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

constexpr int k_warm_ticks = 80;   ///< app::pre_game_ticks.
constexpr int k_window     = 8;    ///< Trailing quarters averaged (FINANCE.md's figure).
constexpr int k_dead_run   = 20;   ///< Trailing ticks of zero output that read as "dead".

/// BL-573: empty is correct — nothing in this sweep opens a mercenary contract.

/// Only these two building types produce, so only these two can be said to have
/// "produced nothing". A port, an inland hub or a research institute NEVER emits
/// a building_report (economy_system.cpp § L3 switch falls through on them), so a
/// census that reads "no report" as "idle" marks every one of them dead. It is a
/// pure measurement artifact and it inflates the answer toward the hypothesis,
/// which is exactly the direction a harness must not be wrong in.
bool is_producing_type(building_type t)
{
    return t == building_type::extraction_site ||
           t == building_type::processing_facility;
}

const char* type_name(building_type t)
{
    switch (t)
    {
    case building_type::extraction_site:      return "extraction";
    case building_type::processing_facility:  return "processing";
    case building_type::port:                 return "port";
    case building_type::inland_logistics_hub: return "inland_hub";
    case building_type::research_institute:   return "research_inst";
    case building_type::schooling:            return "schooling";
    case building_type::university:           return "university";
    default:                                  return "other";
    }
}

/// Everything tracked about one building across the run.
struct btrack
{
    entity_id     id     = null_entity;
    entity_id     corp   = null_entity;
    building_type type   = building_type::none;
    resource_type target = resource_type::iron_ore;
    uint16_t      recipe = no_recipe;

    // State at the end of the warm start.
    int  wt_at_warm       = 0;
    bool decom_at_warm    = false;
    bool building_at_warm = false;   ///< ticks_remaining > 0 (still under construction)
    float maint_const     = 0.0f;    ///< reg.economics(type).maintenance
    float floor_at_warm   = 0.0f;    ///< compute_building_opex(...).maintenance at warm close

    // Production over the warm start.
    double out_warm          = 0.0;  ///< total output units over the whole warm start
    double out_trailing      = 0.0;  ///< output over the last k_dead_run warm ticks
    int    producing_ticks   = 0;    ///< warm ticks with output > 0
    int    last_producing    = -1;   ///< last warm tick index with output > 0
    int    longest_dry_run   = 0;    ///< longest run of consecutive zero-output warm ticks

    // The recovery probe (past the warm start).
    double out_extra         = 0.0;
    int    producing_extra   = 0;
    int    wt_at_end         = 0;
    bool   decom_at_end      = false;
    bool   wt_rose_after     = false;  ///< target went above 0 at any post-warm tick
    bool   resumed_after     = false;  ///< decommission flag cleared at any post-warm tick
    bool   gone              = false;  ///< demolished during the extra run

    // Market context for a dead extraction site. base_price and price are kept
    // APART rather than as one ratio: a ratio of 0 conflates "the good clears at
    // the band floor" with "the good has no authored base price at all", and
    // those are different defects wanting different fixes.
    float  base_price_warm   = -1.0f;  ///< -1 = not an extraction site / no market
    float  price_warm        = -1.0f;
    float  price_ratio_min   =  2.0f;
    float  price_ratio_max   =  0.0f;
    bool   ever_priced       = false;  ///< price > 0 at any warm tick
    float  deposit           = 0.0f;
    float  remaining         = 0.0f;

    /// Produced nothing across the trailing run, whatever it is paying. Restricted
    /// to the two types that CAN produce — see is_producing_type.
    bool dry_at_warm() const
    { return !building_at_warm && is_producing_type(type) && out_trailing <= 0.0; }

    bool dead_at_warm() const
    {
        // Dead = pays the floor and has produced nothing for a long trailing run.
        // Buildings under construction are excluded: they pay the floor too, but
        // they are pre-operational, not abandoned, and conflating the two would
        // inflate the answer in the hypothesis's favour.
        return dry_at_warm() && floored_at_warm();
    }
    /// THE DISCRIMINATOR. Produced nothing, and is NOT floored — so it is paying
    /// FULL maintenance, scaled by a workforce target nobody dialled down. This
    /// bucket is the one the material floor does not explain, and separating it
    /// is the whole point: a fix aimed at the floor does nothing for it.
    bool dry_paying_full() const
    { return dry_at_warm() && !floored_at_warm(); }

    /// Pays the floor for want of labour or a decommission, whatever it produced.
    bool floored_at_warm() const
    {
        return !building_at_warm && (decom_at_warm || wt_at_warm == 0);
    }
};

/// The seven filed flows, averaged per quarter.
struct flow_row
{
    double income = 0, expenditure = 0, maintenance = 0, wages = 0;
    double interest = 0, levies = 0, upkeep = 0, net = 0;
    double balance = 0;
    int    quarters = 0;

    void add(const quarterly_return& q)
    {
        income += q.income; expenditure += q.expenditure; maintenance += q.maintenance;
        wages += q.wages;   interest += q.interest;       levies += q.levies;
        upkeep += q.upkeep; net += q.net;
        ++quarters;
    }
    void mean()
    {
        if (quarters <= 0) return;
        const double n = quarters;
        income /= n; expenditure /= n; maintenance /= n; wages /= n;
        interest /= n; levies /= n; upkeep /= n; net /= n;
    }
    /// income - expenditure - maintenance - wages - levies - upkeep. Interest is
    /// excluded for spawn_solvency's reason: it is charged only on an already
    /// negative balance, so it is a consequence of the deficit, never a cause.
    double operating() const
    { return income - expenditure - maintenance - wages - levies - upkeep; }
};

flow_row trailing(const corporation_component& cc, int window)
{
    flow_row r;
    const std::size_t n = cc.returns.size();
    const std::size_t first = (n > static_cast<std::size_t>(window))
                            ? n - static_cast<std::size_t>(window) : 0;
    for (std::size_t i = first; i < n; ++i)
        r.add(cc.returns[i]);
    r.mean();
    r.balance = cc.balance;
    return r;
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

/// One economy tick in app::step_economy's order, production sink live so the
/// BL-343 levy path runs. Returns the report so the caller can read output.
economy_report tick(world& w, const recipe_registry& reg, int t)
{
    w.current_econ_tick = t;
    w.current_day_tick  = t;
    lp_pool_map lp;
    dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                     reg.logistics_cost(convoy_mode::space), &lp);
    advance_convoys(w);
    economy_report rep = run_economy_step(w, reg, /*spectating=*/false, &lp);
    auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention, &rep.budgets, &rep.buildings,
                 &rep.building_labour);
    run_nation_step(w, reg, rep, t);
    advance_tech_gates(w);
    credit_arrived_convoys(w, t);
    return rep;
}

/// The clearing price and the base price of one resource at the building's own
/// catchment market. Both, separately: base_price 0 means the good is UNPRICED —
/// the market never quotes it — which is a different failure from a good that is
/// priced and clearing at the band floor, and only one of them is about the floor.
struct market_price { float base = -1.0f; float price = -1.0f; };

market_price price_at(const world& w, entity_id tile, resource_type r)
{
    const entity_id mid = market_for_tile(w, tile);
    if (mid == null_entity)
        return {};
    const auto it = w.markets.find(mid);
    if (it == w.markets.end())
        return {};
    const std::size_t ri = static_cast<std::size_t>(r);
    return {it->second.base_price[ri], it->second.price[ri]};
}

/// One corp's whole-holding cost picture, reconstructed from the building state.
struct corp_census
{
    entity_id corp = null_entity;
    bool  seated = false;
    int   holdings = 0;
    int   under_construction = 0;
    int   floored = 0;          ///< at wt 0 or decommissioned
    int   dead = 0;             ///< floored AND no output for k_dead_run ticks
    int   dead_decom = 0;       ///< of the dead, how many are decommissioned
    int   dead_zero_wt = 0;     ///< of the dead, how many are at wt 0 but operating
    int   dry_full = 0;         ///< produced nothing but pays FULL maintenance
    double maint_dry_full = 0.0;///< cr/qtr paid by that bucket
    int   dead_unpriced = 0;    ///< dead extraction sites whose target has base_price 0
    double floor_dead = 0.0;    ///< cr/qtr paid by the dead set
    double floor_all = 0.0;     ///< cr/qtr paid by every floored building
    double maint_recon = 0.0;   ///< reconstructed total maintenance, cr/qtr
    double maint_filed = 0.0;   ///< filed maintenance, last warm quarter, cr/qtr
    flow_row flows;             ///< trailing-window flows
    int   recovered = 0;        ///< dead buildings that produced again in the extra run
    int   demolished = 0;       ///< dead buildings gone by the end of the extra run
    int   resumed = 0;          ///< dead buildings whose decommission flag was cleared
    int   redialled = 0;        ///< dead buildings whose workforce target rose above 0
};

struct seed_result
{
    uint32_t seed = 0;
    corp_census seated;
    std::vector<corp_census> rivals;
    std::vector<btrack> seated_buildings;
    std::vector<btrack> dead_sample;   ///< dead rows across the whole field, for the detail table
    bool filed_ok = true;
    bool recon_ok = true;
    double worst_recon_err = 0.0;
};

seed_result run_seed(uint32_t seed, const recipe_registry& reg,
                     const world_gen_config& gen_cfg, bool prehistory, int extra)
{
    seed_result out;
    out.seed = seed;

    world_params p;
    p.seed = seed;
    if (!prehistory)
        p = no_prehistory(p);

    // app::setup_world's order (app.cpp § start_new_game_prelude).
    //
    // THE gen_cfg ARGUMENT IS LOAD-BEARING AND IT IS EASY TO OMIT. Its default is
    // a C++ fallback `world_gen_config{}` carrying base prices for TEN resources.
    // scripts/world_gen.lua authors ~45. Every market in the world is seeded from
    // `gen_cfg.kepler_base_price`, so a harness that omits this measures a world
    // in which stone, timber, clay, fibre, planks, tools and most of the roster
    // are UNPRICED — not cheap, unpriced, so no market ever quotes them and no
    // site working them can sell a unit at any workforce level. app.cpp:490 calls
    // load_from_lua; a harness must too. Loading world_gen.lua into the Lua state
    // is NOT sufficient — the table has to be parsed into this object and passed.
    world w = make_hard_coded_world(p, nullptr, gen_cfg);
    assign_default_recipes(w, reg);
    generate_background_firms(w, reg, seed ^ 0x8A21F00Du);
    assign_default_recipes(w, reg);

    std::map<entity_id, btrack> track;   // ordered: the walk below must be stable
    std::map<entity_id, int> dry_run;    // running consecutive-zero-output counter

    // ---- the warm start -------------------------------------------------
    for (int t = 0; t < k_warm_ticks; ++t)
    {
        const economy_report rep = tick(w, reg, t);

        // Output this tick, by building. rep.buildings is a vector in a fixed order.
        std::map<entity_id, double> produced;
        for (const building_report& br : rep.buildings)
            produced[br.building] += br.output_quantity;

        for (const entity_id corp : sorted_corp_ids(w))
        {
            const corporation_component& cc = w.corporations.at(corp);
            for (const entity_id bid : cc.assets)     // a vector — fixed order
            {
                const auto bit = w.buildings.find(bid);
                if (bit == w.buildings.end())
                    continue;
                const building_component& b = bit->second;
                btrack& bt = track[bid];
                bt.id = bid; bt.corp = corp; bt.type = b.type;
                bt.target = b.target_resource; bt.recipe = b.recipe;

                const auto pit = produced.find(bid);
                const double o = (pit != produced.end()) ? pit->second : 0.0;
                bt.out_warm += o;
                if (t >= k_warm_ticks - k_dead_run)
                    bt.out_trailing += o;
                if (o > 0.0)
                {
                    ++bt.producing_ticks;
                    bt.last_producing = t;
                    dry_run[bid] = 0;
                }
                else
                {
                    const int r = ++dry_run[bid];
                    bt.longest_dry_run = std::max(bt.longest_dry_run, r);
                }
                if (b.type == building_type::extraction_site)
                {
                    const market_price mp = price_at(w, b.tile, b.target_resource);
                    if (mp.base > 0.0f)
                    {
                        const float pr = mp.price / mp.base;
                        bt.price_ratio_min = std::min(bt.price_ratio_min, pr);
                        bt.price_ratio_max = std::max(bt.price_ratio_max, pr);
                    }
                    bt.ever_priced = bt.ever_priced || (mp.price > 0.0f);
                }
            }
        }
    }

    // ---- state at the close of the warm start ---------------------------
    for (const entity_id corp : sorted_corp_ids(w))
    {
        const corporation_component& cc = w.corporations.at(corp);
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            const building_component& b = bit->second;
            btrack& bt = track[bid];
            bt.wt_at_warm       = b.workforce_target;
            bt.decom_at_warm    = b.decommissioned;
            bt.building_at_warm = b.ticks_remaining > 0;
            const building_economics& e = reg.economics(b.type);
            bt.maint_const   = e.maintenance;
            // Maintenance is a pure function of the building: contention and
            // habitability move only WAGES. 1.0f/1.0f is therefore exact, not an
            // approximation, and A2 below proves it against the filed return.
            bt.floor_at_warm = compute_building_opex(b, e, 1.0f, 1.0f,
                                                     reg.idle_maintenance_floor()).maintenance;
            if (b.type == building_type::extraction_site)
            {
                const market_price mp = price_at(w, b.tile, b.target_resource);
                bt.base_price_warm = mp.base;
                bt.price_warm      = mp.price;
                if (const auto tit = w.tiles.find(b.tile); tit != w.tiles.end())
                {
                    const std::size_t ri = static_cast<std::size_t>(b.target_resource);
                    bt.deposit   = tit->second.resource_deposit[ri];
                    bt.remaining = tit->second.resource_remaining[ri];
                }
            }
        }
    }

    // ---- the recovery probe ---------------------------------------------
    std::set<entity_id> dead_ids;
    for (const auto& kv : track)
        if (kv.second.dead_at_warm())
            dead_ids.insert(kv.first);

    for (int t = k_warm_ticks; t < k_warm_ticks + extra; ++t)
    {
        const economy_report rep = tick(w, reg, t);
        std::map<entity_id, double> produced;
        for (const building_report& br : rep.buildings)
            produced[br.building] += br.output_quantity;
        for (const entity_id bid : dead_ids)
        {
            btrack& bt = track[bid];
            const auto pit = produced.find(bid);
            if (pit != produced.end() && pit->second > 0.0)
            {
                bt.out_extra += pit->second;
                ++bt.producing_extra;
            }
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            if (bit->second.workforce_target > 0)
                bt.wt_rose_after = true;
            // A resume is only meaningful for a building that WAS decommissioned;
            // for one dialled to zero the flag was never set and this is vacuous.
            if (bt.decom_at_warm && !bit->second.decommissioned)
                bt.resumed_after = true;
        }
    }
    for (const entity_id bid : dead_ids)
    {
        btrack& bt = track[bid];
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
        {
            bt.gone = true;
            continue;
        }
        bt.wt_at_end    = bit->second.workforce_target;
        bt.decom_at_end = bit->second.decommissioned;
    }

    // ---- the census -----------------------------------------------------
    for (const entity_id corp : sorted_corp_ids(w))
    {
        const corporation_component& cc = w.corporations.at(corp);
        corp_census c;
        c.corp   = corp;
        c.seated = (corp == w.player_entity);
        for (const entity_id bid : cc.assets)
        {
            const auto tit = track.find(bid);
            if (tit == track.end())
                continue;   // built after the warm start closed — not this census's subject
            const btrack& bt = tit->second;
            ++c.holdings;
            c.maint_recon += bt.floor_at_warm;
            if (bt.building_at_warm) ++c.under_construction;
            if (bt.floored_at_warm())
            {
                ++c.floored;
                c.floor_all += bt.floor_at_warm;
            }
            if (bt.dry_paying_full())
            {
                ++c.dry_full;
                c.maint_dry_full += bt.floor_at_warm;  // its FULL maintenance, not a floor
            }
            if (bt.dead_at_warm())
            {
                ++c.dead;
                c.floor_dead += bt.floor_at_warm;
                if (bt.decom_at_warm) ++c.dead_decom; else ++c.dead_zero_wt;
                if (bt.type == building_type::extraction_site && bt.base_price_warm <= 0.0f)
                    ++c.dead_unpriced;
                if (bt.producing_extra > 0) ++c.recovered;
                if (bt.gone) ++c.demolished;
                if (bt.resumed_after) ++c.resumed;
                if (bt.wt_rose_after) ++c.redialled;
                if (out.dead_sample.size() < 24)
                    out.dead_sample.push_back(bt);
            }
        }
        // The reconstruction guard. The filed return this compares against is the
        // one filed at the LAST WARM TICK — cc.returns has grown by `extra` since,
        // so index back to it rather than reading the back(). It is also a rolling
        // buffer (k_quarterly_return_retention), so the reconciliation is skipped
        // when the warm-start quarter has already rolled out of it.
        const std::size_t n = cc.returns.size();
        if (n == 0)
        {
            out.filed_ok = false;
        }
        else if (n > static_cast<std::size_t>(extra))
        {
            const std::size_t idx = n - static_cast<std::size_t>(extra) - 1;
            c.maint_filed = cc.returns[idx].maintenance;
            const double scale = std::max(1.0, std::fabs(c.maint_filed));
            const double err = std::fabs(c.maint_recon - c.maint_filed) / scale;
            out.worst_recon_err = std::max(out.worst_recon_err, err);
            if (err > 2e-3)
                out.recon_ok = false;
        }
        else
        {
            // The warm-start return has rolled out of the retained window; report
            // the reconstruction alone rather than reconciling against the wrong
            // quarter. (Reduce --extra below the retention to restore the check.)
            c.maint_filed = c.maint_recon;
        }
        c.flows = trailing(cc, k_window);
        if (c.seated)
        {
            out.seated = c;
            for (const entity_id bid : cc.assets)
                if (const auto tit = track.find(bid); tit != track.end())
                    out.seated_buildings.push_back(tit->second);
        }
        else
        {
            out.rivals.push_back(c);
        }
    }
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    int      seed_count = 8;
    uint32_t seed0      = 0;
    bool     prehistory = true;
    int      extra      = 80;
    bool     lua_cfg    = true;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--fast") == 0)
            prehistory = false;
        else if (std::strcmp(argv[i], "--default-cfg") == 0)
            lua_cfg = false;   // reproduce the 10-price fallback world — see below
        else if (std::strcmp(argv[i], "--seed0") == 0 && i + 1 < argc)
            seed0 = static_cast<uint32_t>(std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--extra") == 0 && i + 1 < argc)
            extra = std::max(0, std::atoi(argv[++i]));
        else if (argv[i][0] != '-')
            seed_count = std::max(1, std::atoi(argv[i]));
    }

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");   // kepler_market.base_price — see spawn_solvency
    recipe_registry reg;
    reg.load_from_lua(lua);

    // THE GENERATION CONFIG, parsed — not merely loaded into the Lua state. See
    // the note in run_seed: omitting this silently measures a world where most of
    // the resource roster has no market price at all.
    world_gen_config gen_cfg;
    if (lua_cfg)
        gen_cfg.load_from_lua(lua);

    world_params probe;
    reg.set_era(era_band_for_epoch(probe.epoch_year));

    {
        int priced = 0;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (gen_cfg.kepler_base_price[r] > 0.0f)
                ++priced;
        std::printf("  world_gen_config: %s — %d of %d resources carry a base price\n",
                    lua_cfg ? "PARSED from scripts/world_gen.lua"
                            : "DEFAULT C++ fallback (--default-cfg)",
                    priced, static_cast<int>(resource_count));
    }

    if (reg.economics(building_type::extraction_site).maintenance <= 0.0f)
    {
        std::printf("FATAL: the registry authored no extraction maintenance — "
                    "scripts/economy.lua did not load.\n");
        return 2;
    }

    std::printf("material_floor — is the 30%% material-maintenance floor a "
                "spawn-viability trap?\n");
    std::printf("  %d seeds from %u, %d warm ticks + %d recovery ticks, "
                "dead = %d trailing ticks of zero output, prehistory %s\n",
                seed_count, seed0, k_warm_ticks, extra, k_dead_run,
                prehistory ? "ON (the shipped spawn)" : "OFF (--fast, NOT the spawn)");

    // ---------------------------------------------------------------------
    // A1 — the arithmetic property, proved on the registry before any world.
    // ---------------------------------------------------------------------
    std::printf("\n=== A1  THE FLOOR IS THE SAME WHETHER IDLE OR DECOMMISSIONED ===\n");
    {
        bool all_equal = true, all_thirty = true;
        const building_type types[] = {
            building_type::extraction_site, building_type::processing_facility,
            building_type::port, building_type::inland_logistics_hub };
        for (building_type t : types)
        {
            const building_economics& e = reg.economics(t);
            building_component zero;  zero.type = t;  zero.workforce_target = 0;
            building_component decom; decom.type = t; decom.workforce_target = 100;
            decom.decommissioned = true;
            building_component full;  full.type = t;  full.workforce_target = 100;
            const float m_zero  = compute_building_opex(zero,  e, 1.0f, 1.0f,
                                                        reg.idle_maintenance_floor()).maintenance;
            const float m_decom = compute_building_opex(decom, e, 1.0f, 1.0f,
                                                        reg.idle_maintenance_floor()).maintenance;
            const float m_full  = compute_building_opex(full,  e, 1.0f, 1.0f,
                                                        reg.idle_maintenance_floor()).maintenance;
            std::printf("  %-14s maintenance const %6.2f | wt=0 %6.2f  decom %6.2f  "
                        "wt=100 %6.2f   floor share of full %5.1f%%\n",
                        type_name(t), static_cast<double>(e.maintenance),
                        static_cast<double>(m_zero), static_cast<double>(m_decom),
                        static_cast<double>(m_full),
                        m_full > 0.0f ? 100.0 * m_zero / m_full : 0.0);
            all_equal  = all_equal && (m_zero == m_decom);
            all_thirty = all_thirty &&
                (std::fabs(m_zero - e.maintenance * 0.3f) <= 1e-5f * std::max(1.0f, e.maintenance));
        }
        check(all_equal, "A1", "a building at workforce_target 0 pays EXACTLY the "
                               "maintenance it would pay decommissioned");
        check(all_thirty, "A1", "and that figure is maintenance x 0.30 (BL-049's floor)");
    }

    std::vector<seed_result> rows;
    rows.reserve(static_cast<std::size_t>(seed_count));
    for (int i = 0; i < seed_count; ++i)
    {
        rows.push_back(run_seed(seed0 + static_cast<uint32_t>(i), reg, gen_cfg,
                                prehistory, extra));
        std::printf("  ... seed %u done\n", rows.back().seed);
        std::fflush(stdout);
    }

    // ---------------------------------------------------------------------
    // R1/R2 — the seated corporation.
    // ---------------------------------------------------------------------
    std::printf("\n=== R1/R2  THE SEATED CORPORATION at the close of the warm start ===\n");
    std::printf("  seed | hold  bld floor dead (dec/wt0) dryFULL | floor_dead  "
                "filed_maint  %%maint | income  operating   %%gap | balance\n");
    double t_hold = 0, t_floor = 0, t_dead = 0, t_floor_dead = 0, t_maint = 0;
    double t_oper = 0, t_income = 0, t_recovered = 0, t_demolished = 0, t_bld = 0;
    double t_dry_full = 0, t_maint_dry_full = 0;
    for (const seed_result& r : rows)
    {
        const corp_census& c = r.seated;
        const double gap = c.flows.operating();
        std::printf("  %4u | %4d %4d %5d %4d (%3d/%3d) %7d | %9.2f %11.2f %6.1f%% | "
                    "%6.2f %9.2f %6.1f%% | %10.1f\n",
                    r.seed, c.holdings, c.under_construction, c.floored, c.dead,
                    c.dead_decom, c.dead_zero_wt, c.dry_full, c.floor_dead, c.maint_filed,
                    c.maint_filed > 0 ? 100.0 * c.floor_dead / c.maint_filed : 0.0,
                    c.flows.income,
                    gap, gap < 0 ? 100.0 * c.floor_dead / -gap : 0.0, c.flows.balance);
        t_hold += c.holdings; t_floor += c.floored; t_dead += c.dead;
        t_bld  += c.under_construction;
        t_floor_dead += c.floor_dead; t_maint += c.maint_filed;
        t_oper += gap; t_income += c.flows.income;
        t_recovered += c.recovered; t_demolished += c.demolished;
        t_dry_full += c.dry_full; t_maint_dry_full += c.maint_dry_full;
    }
    const double n = static_cast<double>(rows.size());
    std::printf("  ---- mean over %d seeds ----\n", static_cast<int>(n));
    std::printf("  holdings %.1f, of which %.1f under construction, %.1f floored "
                "(wt0 or decom), %.1f DEAD (no output for %d ticks)\n",
                t_hold / n, t_bld / n, t_floor / n, t_dead / n, k_dead_run);
    std::printf("  dead-building material floor: %.2f cr/qtr = %.1f%% of filed "
                "maintenance (%.2f cr/qtr)\n",
                t_floor_dead / n, t_maint > 0 ? 100.0 * t_floor_dead / t_maint : 0.0,
                t_maint / n);
    std::printf("  THE DISCRIMINATOR — buildings that produced nothing but pay FULL "
                "maintenance (not the floor): %.1f, costing %.2f cr/qtr\n",
                t_dry_full / n, t_maint_dry_full / n);
    std::printf("  so of all non-producing maintenance, %.1f%% is the floor and "
                "%.1f%% is full-rate maintenance the floor does not explain\n",
                (t_floor_dead + t_maint_dry_full) > 0
                    ? 100.0 * t_floor_dead / (t_floor_dead + t_maint_dry_full) : 0.0,
                (t_floor_dead + t_maint_dry_full) > 0
                    ? 100.0 * t_maint_dry_full / (t_floor_dead + t_maint_dry_full) : 0.0);
    std::printf("  operating position %.2f cr/qtr (income %.2f cr/qtr); the dead floor "
                "is %.1f%% of the gap\n",
                t_oper / n, t_income / n,
                t_oper < 0 ? 100.0 * t_floor_dead / -t_oper : 0.0);
    std::printf("  counterfactual, FIRST ORDER ONLY: waiving the dead floor moves the "
                "operating position %.2f -> %.2f cr/qtr\n",
                t_oper / n, (t_oper + t_floor_dead) / n);
    std::printf("    (first order because it holds behaviour fixed — it does not model "
                "the solver re-dialling, nor the interest a smaller deficit avoids)\n");

    // ---------------------------------------------------------------------
    // R3 — recovery.
    // ---------------------------------------------------------------------
    std::printf("\n=== R3  DO DEAD BUILDINGS EVER RECOVER?  (%d ticks past the warm "
                "start) ===\n", extra);
    std::printf("  seated: %.1f dead per seed, of which %.1f produced again and %.1f "
                "were demolished\n",
                t_dead / n, t_recovered / n, t_demolished / n);
    int f_dead = 0, f_rec = 0, f_dem = 0, f_hold = 0, f_floored = 0;
    int f_res = 0, f_redial = 0, f_dry_full = 0, f_unpriced = 0;
    int s_res = 0, s_redial = 0, s_unpriced = 0;
    double f_floor_dead = 0, f_maint = 0, f_oper = 0, f_maint_dry_full = 0;
    int rivals_total = 0, rivals_with_dead = 0;
    for (const seed_result& r : rows)
    {
        s_res += r.seated.resumed; s_redial += r.seated.redialled;
        s_unpriced += r.seated.dead_unpriced;
        for (const corp_census& c : r.rivals)
        {
            ++rivals_total;
            if (c.dead > 0) ++rivals_with_dead;
            f_dead += c.dead; f_rec += c.recovered; f_dem += c.demolished;
            f_res += c.resumed; f_redial += c.redialled;
            f_dry_full += c.dry_full; f_maint_dry_full += c.maint_dry_full;
            f_unpriced += c.dead_unpriced;
            f_hold += c.holdings; f_floored += c.floored;
            f_floor_dead += c.floor_dead; f_maint += c.maint_filed;
            f_oper += c.flows.operating();
        }
    }
    std::printf("  seated: %.1f re-dialled above wt 0, %.1f resumed from decommission "
                "per seed\n", s_redial / n, s_res / n);
    std::printf("  field:  %d dead across %d rival corps, of which %d produced again, "
                "%d were re-dialled, %d resumed, %d demolished\n",
                f_dead, rivals_total, f_rec, f_redial, f_res, f_dem);

    std::printf("\n  A SAMPLE OF DEAD BUILDINGS (first seed, up to 24 rows). base/price "
                "are the target resource's at the site's own market; -1 = not an\n"
                "  extraction site. base 0 means the good is UNPRICED, which is not the "
                "same defect as a good clearing at the band floor.\n");
    std::printf("  type          res/rec  wt  dec  maint  floor | out(warm) dryrun "
                "lastprod | base  price ratio[min..max] priced | deposit    left | "
                "after: wt out gone\n");
    if (!rows.empty())
        for (const btrack& b : rows.front().dead_sample)
        {
            char what[24];
            if (b.type == building_type::extraction_site)
                std::snprintf(what, sizeof what, "res #%d", static_cast<int>(b.target));
            else if (b.type == building_type::processing_facility)
                std::snprintf(what, sizeof what, "rec %u", static_cast<unsigned>(b.recipe));
            else
                std::snprintf(what, sizeof what, "-");
            const bool is_ext = (b.type == building_type::extraction_site);
            std::printf("  %-13s %-8s %3d %4d %6.2f %6.2f | %8.1f %6d %8d | "
                        "%5.2f %6.2f [%.2f..%.2f] %6s | %7.1f %9.1f | %3d %5.1f %s\n",
                        type_name(b.type), what, b.wt_at_warm, b.decom_at_warm ? 1 : 0,
                        static_cast<double>(b.maint_const),
                        static_cast<double>(b.floor_at_warm),
                        b.out_warm, b.longest_dry_run, b.last_producing,
                        static_cast<double>(b.base_price_warm),
                        static_cast<double>(b.price_warm),
                        b.price_ratio_max > 0.0f ? static_cast<double>(b.price_ratio_min) : 0.0,
                        static_cast<double>(b.price_ratio_max),
                        is_ext ? (b.ever_priced ? "yes" : "NEVER") : "-",
                        static_cast<double>(b.deposit), static_cast<double>(b.remaining),
                        b.wt_at_end, b.out_extra, b.gone ? "GONE" : "");
        }

    // ---------------------------------------------------------------------
    // R4 — the field.
    // ---------------------------------------------------------------------
    std::printf("\n=== R4  THE BACKGROUND FIELD ===\n");
    std::printf("  %d rival corps over %d seeds, %d holdings tracked\n",
                rivals_total, static_cast<int>(n), f_hold);
    std::printf("  floored (wt0 or decom): %d (%.1f%% of holdings);  DEAD: %d (%.1f%%)\n",
                f_floored, f_hold > 0 ? 100.0 * f_floored / f_hold : 0.0,
                f_dead, f_hold > 0 ? 100.0 * f_dead / f_hold : 0.0);
    std::printf("  %d of %d rival corps carry at least one dead building (%.1f%%)\n",
                rivals_with_dead, rivals_total,
                rivals_total > 0 ? 100.0 * rivals_with_dead / rivals_total : 0.0);
    std::printf("  dead floor across the field: %.2f cr/qtr total, %.2f cr/qtr per rival "
                "= %.1f%% of filed maintenance\n",
                f_floor_dead, rivals_total > 0 ? f_floor_dead / rivals_total : 0.0,
                f_maint > 0 ? 100.0 * f_floor_dead / f_maint : 0.0);
    std::printf("  field operating position: %.2f cr/qtr per rival; the dead floor is "
                "%.1f%% of that gap\n",
                rivals_total > 0 ? f_oper / rivals_total : 0.0,
                f_oper < 0 ? 100.0 * f_floor_dead / -f_oper : 0.0);
    std::printf("  the discriminator, field-wide: %d holdings produced nothing but pay "
                "FULL maintenance, costing %.2f cr/qtr\n", f_dry_full, f_maint_dry_full);
    std::printf("  of all non-producing maintenance in the field, %.1f%% is the floor "
                "and %.1f%% is full-rate\n",
                (f_floor_dead + f_maint_dry_full) > 0
                    ? 100.0 * f_floor_dead / (f_floor_dead + f_maint_dry_full) : 0.0,
                (f_floor_dead + f_maint_dry_full) > 0
                    ? 100.0 * f_maint_dry_full / (f_floor_dead + f_maint_dry_full) : 0.0);
    std::printf("  dead EXTRACTION sites whose target resource is UNPRICED "
                "(base_price 0, so no market ever quotes it): field %d, seated %d\n",
                f_unpriced, s_unpriced);

    // ---------------------------------------------------------------------
    // Non-vacuity and reconciliation.
    // ---------------------------------------------------------------------
    std::printf("\n=== A2/A3  THE INSTRUMENT ===\n");
    bool filed_ok = true, recon_ok = true;
    double worst = 0.0;
    for (const seed_result& r : rows)
    {
        filed_ok = filed_ok && r.filed_ok;
        recon_ok = recon_ok && r.recon_ok;
        worst = std::max(worst, r.worst_recon_err);
    }
    std::printf("  worst maintenance reconstruction error across every corp, every "
                "seed: %.3g (relative)\n", worst);
    check(filed_ok, "A2", "every corporation filed a quarterly return");
    check(recon_ok, "A2", "summing compute_building_opex over `assets` reproduces the "
                          "filed maintenance (the reconstruction is the live figure)");
    check(t_hold > 0 && f_hold > 0, "A3",
          "the census is non-vacuous — buildings were found on both sides");

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES ABOVE",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
