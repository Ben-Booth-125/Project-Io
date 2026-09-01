// spawn_solvency — BL-635. Is the corporation the player is handed solvent at
// the gate, and if not, WHICH FLOW is taking the money? Requirement group
// `spawn-solvency`, rows R1-R4.
//
// THE MEASUREMENT THIS EXISTS TO MAKE (R1, and it is a deliverable in its own
// right). A wire test on 2026-08-25 found the default spawn opening at Cr -1
// with a net of -627/qtr, and dispatch_convoy refusing a two-unit haul for want
// of funds. That is a symptom with six possible causes — wages, maintenance,
// interest, the generation-seeded extraction levy, thin market income, or
// standing-force upkeep — and a constant nudged before the cause is named is a
// guess. BL-626 landed the instrument the morning this was written: every
// corporation now FILES a quarterly return carrying all seven money-loop flows.
// This harness reads those filed returns and prints the per-flow attribution,
// per seed, for the seated corp and for the background field beside it.
//
// WHY IT LOADS LUA RATHER THAN MIRRORING IT. Every other economy harness
// restates scripts/economy.lua as C++ constants, and for their questions that is
// right — they assert a RELATION (a retain property, a conservation, an
// ordering) that holds whatever the constants are. This one asserts a
// MAGNITUDE. A restated registry would answer a question about a world that does
// not ship: the campaign opens at 0 CE, so the era band masks the industrial
// recipe roster down to the ancient one, and which recipes exist decides whether
// a generated processor earns or idles. So it takes the real scripts/recipes.lua,
// scripts/economy.lua and scripts/world_gen.lua through a live Lua state, exactly
// as app::load_economy does. That puts it in NEEDS_LUA alongside
// pregame_balance_harness: build it through CMake, or with the manual cl/g++ line
// in tools/verify/README.md. `build_harness.js` refuses it by name.
//
// THE TICK IS app::step_economy's, IN ITS ORDER. Convoys, economy step, market
// clearing, apply_budget WITH the production sink (so the levy path is live — a
// harness that omits it charges nothing and would exonerate the levy by
// construction), the nation step, tech gates, convoy credit. Anything less would
// measure a money loop the game does not run.
//
//   R1  Per-flow attribution of the opening position across a seed sweep, read
//       from the filed returns. REPORTED, not asserted: the row passes when the
//       instrument produced a non-vacuous reading (every corp filed, and the
//       flows sum to the filed net), because a diagnosis cannot be a pass/fail.
//
//   R2  THE GATE. The seated corporation's net at the end of the warm start is
//       non-negative on a MAJORITY of swept seeds, and its balance is above zero.
//       CURRENTLY RED at 1/8 — deliberately left red rather than relaxed; the
//       residual it is failing on is named in the block above.
//
//   R3  No clamp. The balance is never floored, and no subsidy is handed to the
//       seated corp: asserted here as the two properties a clamp would break —
//       a corp CAN still end the warm start underwater on some seed (an honest
//       constraint still bites), and the seated corp does not out-earn the field
//       per holding.
//
//   R4  Rivals survive it. The requirement's own words are "solvent enough to
//       KEEP ACTING, so the seed's data is not poisoned by dead corps", so the
//       assertions are that the field's holding count grew, that it still fields
//       a standing force, and that rival solvency is at or above its pre-fix
//       baseline. The absolute percentage is REPORTED beside that baseline
//       rather than asserted against a bar: it was 26.9% BEFORE this item
//       touched anything, so a bar there would be testing the sprint's viability
//       question, not this fix.
//
// WHAT THE DIAGNOSIS FOUND, and what was done about it (2026-08-26). Recorded
// here because the numbers are the deliverable and prose elsewhere goes stale.
//
//   THE CAUSE WAS UPKEEP, by an order of magnitude over anything else. The
//   seated corporation paid 667.5 cr/qtr of operating outgoings against 21.2
//   cr/qtr of gross income, and 600.0 of the 667.5 — 89.9% — was standing-force
//   upkeep. Ranking the flows by SIZE names interest first (2,160.7 cr/qtr), and
//   that is the wrong answer: interest is charged only on an already-negative
//   balance, so it is the symptom of the deficit, never its cause. See
//   flow_row::operating().
//
//   Three things were changed, each at the layer the measurement pointed at:
//
//   1. scripts/economy.lua — the whole unit_upkeep vector x 0.025. Its
//      derivation compared a PER-HEAD annual wage against a PER-UNIT hire price,
//      and a unit is 50 heads, so the shipped rate said a Levy Spear cost 40
//      credits to raise and 1,200 a year to keep. Both halves of the vector are
//      scaled by one factor, so value_anchor's equipment:wage band is preserved
//      exactly rather than weakened (re-verified: 19/19 roster rows in band).
//
//   2. src/world/hard_coded_world.cpp — the player unit stub deleted. It handed
//      w.player_entity a 50-head unit that corporation generation ALSO gives
//      every corp, so the seated corp fielded 2 units / 100 heads on 8/8 seeds
//      while a rival fielded 1 / 50. Deleting a duplicate, not a subsidy.
//
//   3. src/world/law.hpp — the seeded extraction levy 1.0 -> 0.1 cr per unit.
//      Measured at 1.14 cr of levy per 1.27 cr realised on a unit of raw output:
//      a flat charge that had drifted past the ancient tier's own prices (stone
//      is authored at 1.00).
//
//   MEASURED AFTER, same 8 seeds: operating net -646.4 -> -27.9 cr/qtr, closing
//   balance -120,334.6 -> -412.3 cr, rival solvency 26.9% -> 28.4% and the field
//   still acting (holdings +35.9%, rivals fielding 214 units against ~135).
//
//   R2 IS NOT MET, AND THE RESIDUAL IS NOT A FINANCE FLOW. After the fix the
//   seated corp still runs at -27.9 cr/qtr, and 60.2% of what remains is
//   MAINTENANCE on buildings that do not produce. The per-building rows below
//   name it exactly: an extraction site targeting the ancient ambient tier
//   (stone #11, timber #12, fibre #43) is idle 80/80 ticks on every seed, with a
//   real unexhausted deposit under it, while iron_ore #0 and agricultural_
//   produce #6 sites run 80/80. The arithmetic behind that is not a bug in any
//   one file: an ambient raw with no downstream demand clears at the price band
//   floor (0.25 x base, so stone at 0.25 cr/unit), a full-staffed site on a
//   median deposit grosses ~3.3 cr/qtr there, and its avoidable operating cost
//   is ~7.5 cr/qtr at base_wage 8.0 — so the BL-181 workforce auto-solver
//   correctly dials it to zero, and the site then pays the 30% material
//   maintenance floor forever, producing nothing. Whether the seated corp clears
//   zero is therefore decided by whether the generator handed it a resource with
//   a live consumer, which is a production/pricing viability question and
//   BL-630's floor to define, not a money-loop constant to nudge. Nothing here
//   was clamped to close that gap.
//
// Usage:  spawn_solvency [seed_count] [--fast] [--seed0 N]
//   --fast   zero the pre-epoch year-tick sim (prehistory_years = 0). ~23 s a
//            world cheaper, and NOT the shipped spawn — for iteration only. The
//            default runs the real one.
//
// Exits 0 on PASS, non-zero on any failure.

#include "scripting/lua_state.hpp"

#include "world/world_gen_config.hpp"
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

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
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

/// True on the shipped spawn, false under --fast.
///
/// Every baseline constant below was taken on the SHIPPED spawn. --fast zeroes
/// the pre-epoch year-tick sim, and the world it leaves has no settlement
/// history, so its buildings run near-unstaffed and every magnitude in it is a
/// different quantity wearing the same name. Rows that compare against a
/// baseline are therefore REPORTED under --fast and asserted only on the real
/// spawn — the alternative is a red row that means nothing, which teaches a
/// reader to ignore red rows.
bool g_real_spawn = true;

void check_on_real_spawn(bool ok, const char* row, const char* what)
{
    if (!g_real_spawn)
    {
        std::printf("  [....] %-3s %s  (not asserted under --fast — "
                    "the baseline is the shipped spawn's)\n", row, what);
        return;
    }
    check(ok, row, what);
}

constexpr int k_warm_ticks = 80;  ///< app::pre_game_ticks.
constexpr int k_window     = 8;   ///< Trailing quarters averaged (FINANCE.md's own figure).

// ---------------------------------------------------------------------------
// THE PRE-FIX BASELINE — measured, dated, and kept
// ---------------------------------------------------------------------------
// Taken on this harness at commit 8548cea3 (the wave-2 promotion commit), 8
// seeds from 0, prehistory ON, 80 warm ticks, trailing 8-quarter window. They
// are the numbers BL-635's diagnosis was made on, and they are constants here
// rather than prose so a later regression is measured against the same rows
// this fix was measured against.
//
//   seated operating net    -646.4 cr/qtr   (income 21.2, outgoings 667.5)
//     of which upkeep        600.0 cr/qtr   = 89.9% of operating outgoings
//     maintenance             32.3           levies 21.7   expenditure 11.1
//     wages                    2.5
//   seated closing balance  -120,334.6 cr   on 8/8 seeds, every one underwater
//   seated standing force    2 units / 100 heads on 8/8 seeds
//   rival standing force    15-20 units / 750-1000 heads per seed
//   rivals solvent at close  26.9%
//
// The wire test this item was opened on (2026-08-25) reported "Cr -1, net
// -627/qtr". -627 is the OPERATING net, and this sweep reproduces it: the
// per-seed range was -613.9 to -702.9, mean -646.4. The balance figure did not
// reproduce (-1 against -120,335) and is not explained here; a live session
// reads its balance at a different moment in the tick than a warm start's last
// filed return, and the item's diagnosis never rested on it.
constexpr double k_baseline_seated_operating_net   = -646.4;
constexpr double k_baseline_seated_upkeep          =  600.0;
constexpr double k_baseline_rival_solvent_pct      =   26.9;
constexpr double k_baseline_rival_units_per_seed   =   16.9; // mean of 15..20

/// BL-573: run_nation_step's template registry. Empty is correct here — nothing
/// in this sweep opens a mercenary contract, so the walk is vacuous.

/// The producing side of the seated corp, accumulated over the trailing window.
/// The cost side alone cannot say WHY income is thin — a corp can be poor because
/// its costs are high or because its buildings barely run, and those want
/// different fixes. This is the denominator.
struct output_probe
{
    entity_id corp = null_entity;
    int   ticks     = 0;
    int   building_ticks = 0;
    int   active    = 0;
    int   idle      = 0;
    double workforce = 0.0;   ///< summed effective_workforce over building-ticks
    double output    = 0.0;   ///< summed output_quantity over building-ticks
    double extraction_output = 0.0;
    int   exhausted = 0;      ///< extraction building-ticks reporting a spent reserve

    /// Per-building: active, idle, exhausted, and what the building IS. Named so
    /// "idle" can be told apart from "idle because the tile has nothing".
    struct per_building
    {
        entity_id     id   = null_entity;
        building_type type = building_type::none;
        resource_type target = resource_type::iron_ore;
        uint16_t      recipe = no_recipe;
        int active = 0, idle = 0, exhausted = 0;
        double output = 0.0;
        float  deposit = 0.0f, remaining = 0.0f;
    };
    std::map<entity_id, per_building> buildings;
};

output_probe* g_probe = nullptr;

/// One economy tick, in app::step_economy's order. The production sink is passed
/// so the BL-343 levy path is LIVE; see the header note.
void tick(world& w, const recipe_registry& reg, int t)
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

    if (g_probe != nullptr && g_probe->corp != null_entity)
    {
        ++g_probe->ticks;
        for (const building_report& br : rep.buildings) // a vector, fixed order
        {
            if (br.corp != g_probe->corp)
                continue;
            ++g_probe->building_ticks;
            g_probe->active    += br.active ? 1 : 0;
            g_probe->idle      += br.idle ? 1 : 0;
            g_probe->workforce += br.effective_workforce;
            g_probe->output    += br.output_quantity;
            g_probe->exhausted += br.exhausted ? 1 : 0;
            if (br.type == building_type::extraction_site)
                g_probe->extraction_output += br.output_quantity;

            auto& pb = g_probe->buildings[br.building];
            pb.id      = br.building;
            pb.type    = br.type;
            pb.target  = br.target_resource;
            pb.recipe  = br.recipe;
            pb.active  += br.active ? 1 : 0;
            pb.idle    += br.idle ? 1 : 0;
            pb.exhausted += br.exhausted ? 1 : 0;
            pb.output  += br.output_quantity;
            if (const auto bit = w.buildings.find(br.building); bit != w.buildings.end())
                if (const auto tit = w.tiles.find(bit->second.tile); tit != w.tiles.end())
                {
                    const std::size_t ri = static_cast<std::size_t>(br.target_resource);
                    pb.deposit   = tit->second.resource_deposit[ri];
                    pb.remaining = tit->second.resource_remaining[ri];
                }
        }
    }
}

/// The seven flows plus the derived net, averaged per quarter.
struct flow_row
{
    double income = 0, expenditure = 0, maintenance = 0, wages = 0;
    double interest = 0, levies = 0, upkeep = 0, net = 0;
    double balance = 0;
    double holdings = 0;
    int    quarters = 0;

    void add(const quarterly_return& q)
    {
        income      += q.income;
        expenditure += q.expenditure;
        maintenance += q.maintenance;
        wages       += q.wages;
        interest    += q.interest;
        levies      += q.levies;
        upkeep      += q.upkeep;
        net         += q.net;
        holdings    += q.holdings;
        ++quarters;
    }
    void mean()
    {
        if (quarters <= 0)
            return;
        const double n = quarters;
        income /= n; expenditure /= n; maintenance /= n; wages /= n;
        interest /= n; levies /= n; upkeep /= n; net /= n; holdings /= n;
    }
    /// income - expenditure - maintenance - wages - interest - levies - upkeep.
    double reconstructed() const
    {
        return income - expenditure - maintenance - wages - interest - levies - upkeep;
    }

    /// The net WITHOUT interest — the operating position.
    ///
    /// This is the number the diagnosis has to be made on, and the distinction
    /// is not cosmetic. Interest is charged only on an already-negative balance
    /// (budget_system.cpp, BL-073), so it is a CONSEQUENCE of an operating
    /// deficit, never a cause of one. A corp that is 120k underwater pays ~2.4k
    /// a quarter in interest and any attribution that ranks flows by size will
    /// name interest first and diagnose the symptom. Rank on this instead.
    double operating() const
    {
        return income - expenditure - maintenance - wages - levies - upkeep;
    }
};

/// Mean of a corp's LAST `window` filed returns.
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

struct seed_result
{
    uint32_t seed = 0;
    // The seated (player) corporation.
    flow_row seated;
    std::string seated_name;
    int  seated_extraction = 0, seated_processing = 0, seated_other = 0;
    int  seated_units = 0, seated_heads = 0;
    bool seated_dipped = false;   ///< balance went below zero at any filed quarter
    std::vector<float> seated_balance_trace; ///< closing balance, quarter by quarter
    // The background field.
    int    rivals = 0;
    int    rivals_solvent = 0;
    double rival_net_mean = 0.0;
    double rival_balance_median = 0.0;
    flow_row rival_mean;          ///< per-corp mean of the trailing flows
    int    field_holdings_open = 0;
    int    field_holdings_close = 0;
    int    rival_units = 0, rival_heads = 0;
    output_probe probe;
    bool   filed_ok = true;       ///< every corp filed, and flows reconstruct the net
};

double median_of(std::vector<double> v)
{
    if (v.empty())
        return 0.0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

seed_result run_seed(uint32_t seed, const recipe_registry& reg, bool prehistory,
                    const world_gen_config& gen_cfg)
{
    seed_result out;
    out.seed = seed;

    world_params p;
    p.seed = seed;
    if (!prehistory)
        p = no_prehistory(p);

    // app::setup_world -> load_economy -> generate_background_firms ->
    // assign_default_recipes, in that order (app.cpp § start_new_game_prelude).
    //
    // THE GENERATION CONFIG MUST BE PARSED AND PASSED (fixed 2026-08-26, found by
    // material_floor.cpp). Omitting it does not fall back to something close: the
    // default `world_gen_config{}` carries base prices for TEN of forty-seven
    // resources, where scripts/world_gen.lua authors FORTY-TWO. Markets are seeded
    // from that table, so a harness without it measures a world in which stone,
    // timber, clay, fibre, planks and tools are UNPRICED — not cheap, unquoted —
    // and every extraction site working them is unsellable at any workforce.
    //
    // Loading world_gen.lua into the Lua state is NOT sufficient and this file
    // made exactly that mistake: the 2026-08-26 loader fix added the load and not
    // the parse, so the numbers it produced afterwards were still of the wrong
    // world. app.cpp:488-490 loads AND parses AND passes; a harness must do all
    // three or it is not measuring the shipped spawn.
    world w = make_hard_coded_world(p, nullptr, gen_cfg);
    assign_default_recipes(w, reg);
    generate_background_firms(w, reg, seed ^ 0x8A21F00Du);
    assign_default_recipes(w, reg);

    for (const auto& kv : w.corporations)
        out.field_holdings_open += static_cast<int>(kv.second.assets.size());

    output_probe probe;
    probe.corp = w.player_entity;
    g_probe = &probe;
    for (int t = 0; t < k_warm_ticks; ++t)
        tick(w, reg, t);
    g_probe = nullptr;
    out.probe = probe;

    const std::vector<entity_id> ids = sorted_corp_ids(w);
    std::vector<double> rival_balances;
    double rival_net_sum = 0.0;
    flow_row rival_acc;

    // The standing force, bucketed by owner. Ascending unit id so the walk over
    // the unordered `w.units` cannot inherit its layout.
    std::vector<entity_id> unit_ids;
    unit_ids.reserve(w.units.size());
    for (const auto& kv : w.units)
        unit_ids.push_back(kv.first);
    std::sort(unit_ids.begin(), unit_ids.end());
    std::map<entity_id, std::pair<int, int>> force; // owner -> (units, heads)
    for (const entity_id uid : unit_ids)
    {
        const unit_component& u = w.units.at(uid);
        auto& f = force[u.owner];
        f.first  += 1;
        f.second += u.count;
    }

    for (const entity_id id : ids)
    {
        const corporation_component& cc = w.corporations.at(id);
        out.field_holdings_close += static_cast<int>(cc.assets.size());
        if (cc.returns.empty())
        {
            out.filed_ok = false;
            continue;
        }
        // The flows must reconstruct the filed net to within a float epsilon of
        // the magnitudes involved — the retain property quarterly_return already
        // proves exactly; here it is only a guard that the reading is real.
        const quarterly_return& q = cc.returns.back();
        const double rebuilt = static_cast<double>(q.income) - q.expenditure - q.maintenance
                             - q.wages - q.interest - q.levies - q.upkeep;
        const double scale = std::max(1.0, std::fabs(static_cast<double>(q.net)));
        if (std::fabs(rebuilt - static_cast<double>(q.net)) > 1e-3 * scale)
            out.filed_ok = false;

        const flow_row row = trailing(cc, k_window);
        if (id == w.player_entity)
        {
            out.seated      = row;
            out.seated_name = cc.name;
            for (const entity_id bid : cc.assets)
            {
                const auto bit = w.buildings.find(bid);
                if (bit == w.buildings.end())
                    continue;
                switch (bit->second.type)
                {
                case building_type::extraction_site:      ++out.seated_extraction; break;
                case building_type::processing_facility:  ++out.seated_processing; break;
                default:                                  ++out.seated_other;      break;
                }
            }
            for (const quarterly_return& qq : cc.returns)
            {
                out.seated_dipped = out.seated_dipped || qq.balance < 0.0f;
                out.seated_balance_trace.push_back(qq.balance);
            }
            if (const auto f = force.find(id); f != force.end())
            {
                out.seated_units = f->second.first;
                out.seated_heads = f->second.second;
            }
        }
        else
        {
            ++out.rivals;
            if (const auto f = force.find(id); f != force.end())
            {
                out.rival_units += f->second.first;
                out.rival_heads += f->second.second;
            }
            if (cc.balance > 0.0f)
                ++out.rivals_solvent;
            rival_balances.push_back(cc.balance);
            rival_net_sum += row.net;
            rival_acc.income      += row.income;
            rival_acc.expenditure += row.expenditure;
            rival_acc.maintenance += row.maintenance;
            rival_acc.wages       += row.wages;
            rival_acc.interest    += row.interest;
            rival_acc.levies      += row.levies;
            rival_acc.upkeep      += row.upkeep;
            rival_acc.net         += row.net;
            rival_acc.holdings    += row.holdings;
            ++rival_acc.quarters;
        }
    }

    rival_acc.mean();
    out.rival_mean           = rival_acc;
    out.rival_net_mean       = (out.rivals > 0) ? rival_net_sum / out.rivals : 0.0;
    out.rival_balance_median = median_of(rival_balances);
    return out;
}

void print_header()
{
    std::printf("\n seed  E/P/O  units/heads |    income   expend    maint    wages "
                "interest   levies   upkeep |    op.net       net       balance\n");
    std::printf("---------------------------+-------------------------------------"
                "---------------------------+--------------------------------\n");
}

void print_row(const char* label, const flow_row& f)
{
    std::printf("%-26s | %9.1f %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f | %9.1f %9.1f %11.1f\n",
                label, f.income, f.expenditure, f.maintenance, f.wages,
                f.interest, f.levies, f.upkeep, f.operating(), f.net, f.balance);
}

} // namespace

int main(int argc, char** argv)
{
    int      seed_count = 12;
    uint32_t seed0      = 0;
    bool     prehistory = true;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--fast") == 0)
            prehistory = false;
        else if (std::strcmp(argv[i], "--seed0") == 0 && i + 1 < argc)
            seed0 = static_cast<uint32_t>(std::atoi(argv[++i]));
        else if (argv[i][0] != '-')
            seed_count = std::max(1, std::atoi(argv[i]));
    }

    // The real data layer, loaded as app::load_economy loads it. A restated
    // registry would answer a question about a world that does not ship.
    g_real_spawn = prehistory;

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    // FIXED 2026-08-26 (found by BL-649's census, which hit the same bug on its own
    // first run). This file's own header says it loads world_gen.lua and it did not.
    // The consequence is NOT a missing table -- it is silent: world_gen.lua carries
    // `kepler_market.base_price`, and BOTH basket injectors SKIP an unpriced resource
    // without saying so, so background demand simply did not exist in the measured
    // world and the diagnosis was taken against less demand than the game has.
    lua.load("scripts/world_gen.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);

    // Parsed, not merely loaded — see the note in run_seed. This one line is the
    // difference between measuring the shipped spawn and measuring a world where
    // most of the ancient roster cannot be sold at all.
    world_gen_config gen_cfg{};
    gen_cfg.load_from_lua(lua);

    world_params probe;              // for the epoch year alone
    reg.set_era(era_band_for_epoch(probe.epoch_year));

    // Vacuity guard (the standing lesson from interbody_pull_harness): an empty
    // registry would print every corp as equally poor and diagnose nothing.
    if (reg.economics(building_type::extraction_site).maintenance <= 0.0f)
    {
        std::printf("FATAL: the registry authored no extraction maintenance — "
                    "scripts/economy.lua did not load.\n");
        return 2;
    }

    std::printf("spawn_solvency — BL-635, requirement group `spawn-solvency` R1-R4\n");
    std::printf("  %d seeds from %u, %d warm ticks, trailing window %d quarters, "
                "prehistory %s\n",
                seed_count, seed0, k_warm_ticks, k_window,
                prehistory ? "ON (the shipped spawn)" : "OFF (--fast, NOT the spawn)");

    std::vector<seed_result> rows;
    rows.reserve(static_cast<std::size_t>(seed_count));
    for (int i = 0; i < seed_count; ++i)
    {
        rows.push_back(run_seed(seed0 + static_cast<uint32_t>(i), reg, prehistory, gen_cfg));
        std::printf("  ... seed %u done\n", rows.back().seed);
        std::fflush(stdout);
    }

    // ---------------------------------------------------------------------
    // R1 — the attribution. Reported, not asserted.
    // ---------------------------------------------------------------------
    std::printf("\n=== R1  THE SEATED CORPORATION — trailing %d-quarter mean, "
                "credits per quarter ===\n", k_window);
    print_header();
    for (const seed_result& r : rows)
    {
        char label[80];
        std::snprintf(label, sizeof label, "%5u %2d/%2d/%2d  %2d/%4d %s", r.seed,
                      r.seated_extraction, r.seated_processing, r.seated_other,
                      r.seated_units, r.seated_heads,
                      r.seated_dipped ? "(dip)" : "     ");
        print_row(label, r.seated);
    }

    flow_row seated_mean;
    for (const seed_result& r : rows)
    {
        seated_mean.income      += r.seated.income;
        seated_mean.expenditure += r.seated.expenditure;
        seated_mean.maintenance += r.seated.maintenance;
        seated_mean.wages       += r.seated.wages;
        seated_mean.interest    += r.seated.interest;
        seated_mean.levies      += r.seated.levies;
        seated_mean.upkeep      += r.seated.upkeep;
        seated_mean.net         += r.seated.net;
        seated_mean.balance     += r.seated.balance;
        seated_mean.holdings    += r.seated.holdings;
        ++seated_mean.quarters;
    }
    const int n_rows = seated_mean.quarters;
    seated_mean.mean();
    if (n_rows > 0)
        seated_mean.balance /= n_rows;
    std::printf("------------------------+-------------------------------------"
                "---------------------------+------------------------\n");
    print_row("  MEAN across seeds", seated_mean);

    std::printf("\n=== R1  THE BACKGROUND FIELD — same window, per-corp mean ===\n");
    print_header();
    for (const seed_result& r : rows)
    {
        char label[80];
        std::snprintf(label, sizeof label, "%5u %3d rivals %3d/%5d", r.seed, r.rivals,
                      r.rival_units, r.rival_heads);
        flow_row f = r.rival_mean;
        f.balance = r.rival_balance_median;
        print_row(label, f);
    }

    // The cost side ranked, which is the sentence the diagnosis has to produce.
    // Ranked on the OPERATING outgoings — interest excluded, and its exclusion
    // stated rather than quiet. See flow_row::operating().
    std::printf("\n=== R1  COST ATTRIBUTION — the seated corp, share of OPERATING outgoings ===\n");
    {
        const double out_total = seated_mean.expenditure + seated_mean.maintenance
                               + seated_mean.wages + seated_mean.levies
                               + seated_mean.upkeep;
        const struct { const char* n; double v; } terms[] = {
            {"upkeep (standing force)",     seated_mean.upkeep},
            {"expenditure (inputs bought)", seated_mean.expenditure},
            {"maintenance",                 seated_mean.maintenance},
            {"wages",                       seated_mean.wages},
            {"levies",                      seated_mean.levies},
        };
        std::vector<std::pair<double, const char*>> ranked;
        for (const auto& t : terms)
            ranked.emplace_back(t.v, t.n);
        std::sort(ranked.begin(), ranked.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });
        std::printf("  gross income             %10.1f cr/qtr\n", seated_mean.income);
        std::printf("  operating outgoings      %10.1f cr/qtr\n", out_total);
        for (const auto& [v, n] : ranked)
            std::printf("    %-30s %9.1f cr/qtr  %5.1f%% of operating outgoings\n", n, v,
                        out_total > 0.0 ? 100.0 * v / out_total : 0.0);
        std::printf("  OPERATING NET            %10.1f cr/qtr   (income - the five above)\n",
                    seated_mean.income - out_total);
        std::printf("  interest (a CONSEQUENCE) %10.1f cr/qtr   charged only on a "
                    "balance already below zero\n", seated_mean.interest);
        std::printf("  filed net                %10.1f cr/qtr\n", seated_mean.net);
    }

    // The DENOMINATOR. Costs alone cannot say whether the corp is expensive or
    // simply idle, and those want different fixes.
    std::printf("\n=== R1  THE PRODUCING SIDE — the seated corp over the whole warm start ===\n");
    std::printf("  seed | bldg-ticks  active   idle | mean eff.workforce | output/tick "
                "| extraction out/tick\n");
    for (const seed_result& r : rows)
    {
        const output_probe& p = r.probe;
        const double bt = p.building_ticks > 0 ? static_cast<double>(p.building_ticks) : 1.0;
        std::printf("  %4u |    %6d  %6d %6d |            %7.3f |     %7.2f |          %7.2f\n",
                    r.seed, p.building_ticks, p.active, p.idle, p.workforce / bt,
                    p.ticks > 0 ? p.output / p.ticks : 0.0,
                    p.ticks > 0 ? p.extraction_output / p.ticks : 0.0);
    }
    // Per building, on every seed. "Idle" is a single flag over four different
    // causes (no workforce / no deposit / below t_idle / misconfigured), and the
    // fix differs by cause, so the deposit the building actually stands on is
    // printed beside its idle count rather than inferred.
    std::printf("\n=== R1  THE SEATED CORP'S BUILDINGS, one line each ===\n");
    std::printf("  seed  type                  target/recipe   active   idle  exhaust "
                "|  deposit  remaining |  output\n");
    for (const seed_result& r : rows)
    {
        for (const auto& [bid, pb] : r.probe.buildings)
        {
            const char* tn = "?";
            switch (pb.type)
            {
            case building_type::extraction_site:     tn = "extraction_site";     break;
            case building_type::processing_facility: tn = "processing_facility"; break;
            case building_type::port:                tn = "port";                break;
            case building_type::military_base:       tn = "military_base";       break;
            case building_type::launchpad:           tn = "launchpad";           break;
            case building_type::inland_logistics_hub:tn = "inland_hub";          break;
            case building_type::research_institute:  tn = "research_institute";  break;
            case building_type::schooling:           tn = "schooling";           break;
            case building_type::university:          tn = "university";          break;
            default: break;
            }
            char what[32];
            // The resource is printed by enum index: world/ carries no
            // resource_type -> string table (resource_names goes the other way),
            // and the index is enough to identify the row against RESOURCES.md.
            if (pb.type == building_type::extraction_site)
                std::snprintf(what, sizeof what, "res #%d",
                              static_cast<int>(pb.target));
            else if (pb.type == building_type::processing_facility)
                std::snprintf(what, sizeof what, "recipe %u", static_cast<unsigned>(pb.recipe));
            else
                std::snprintf(what, sizeof what, "-");
            std::printf("  %4u  %-21s %-14s %6d %6d %8d | %8.2f %10.1f | %7.1f\n",
                        r.seed, tn, what, pb.active, pb.idle, pb.exhausted,
                        static_cast<double>(pb.deposit),
                        static_cast<double>(pb.remaining), pb.output);
        }
    }

    {
        double units = 0.0, levy = 0.0;
        for (const seed_result& r : rows)
        {
            units += (r.probe.ticks > 0)
                   ? r.probe.extraction_output / r.probe.ticks : 0.0;
            levy  += r.seated.levies;
        }
        std::printf("  mean raw output %.2f units/qtr against %.1f cr/qtr of levy "
                    "and %.1f cr/qtr of income\n",
                    units / std::max<std::size_t>(1, rows.size()),
                    levy / std::max<std::size_t>(1, rows.size()), seated_mean.income);
        const double u = units / std::max<std::size_t>(1, rows.size());
        if (u > 0.0)
            std::printf("  => realised %.2f cr per raw unit sold; the levy charges "
                        "a FLAT %.2f cr per raw unit\n",
                        seated_mean.income / u, levy / std::max<std::size_t>(1, rows.size()) / u);
    }

    // The trajectory, so a reader can see whether the corp ever earned.
    std::printf("\n=== R1  THE SEATED BALANCE, quarter by quarter (last %d filed) ===\n",
                static_cast<int>(rows.empty() ? 0 : rows.front().seated_balance_trace.size()));
    for (const seed_result& r : rows)
    {
        std::printf("  seed %5u:", r.seed);
        const std::size_t n = r.seated_balance_trace.size();
        for (std::size_t i = 0; i < n; i += 5)
            std::printf(" %.0f", static_cast<double>(r.seated_balance_trace[i]));
        std::printf("\n");
    }

    bool all_filed = true;
    for (const seed_result& r : rows)
        all_filed = all_filed && r.filed_ok;
    std::printf("\n");
    check(all_filed && n_rows > 0, "R1",
          "every corporation filed, and the seven flows reconstruct the filed net");
    check(std::fabs(seated_mean.income) + std::fabs(seated_mean.wages) > 0.0, "R1",
          "the reading is non-vacuous (the seated corp moved money)");

    // ---------------------------------------------------------------------
    // R2 — the gate.
    // ---------------------------------------------------------------------
    int solvent = 0, positive_net = 0, both = 0;
    for (const seed_result& r : rows)
    {
        const bool bal = r.seated.balance > 0.0;
        const bool net = r.seated.net >= 0.0;
        solvent      += bal ? 1 : 0;
        positive_net += net ? 1 : 0;
        both         += (bal && net) ? 1 : 0;
    }
    std::printf("\n=== R2  THE GATE ===\n");
    std::printf("  balance > 0   on %d/%d seeds\n", solvent, n_rows);
    std::printf("  net    >= 0   on %d/%d seeds\n", positive_net, n_rows);
    std::printf("  both          on %d/%d seeds\n", both, n_rows);
    std::printf("  MOVEMENT against the pre-BL-635 baseline:\n");
    std::printf("    operating net   %10.1f -> %10.1f cr/qtr\n",
                k_baseline_seated_operating_net, seated_mean.operating());
    std::printf("    upkeep          %10.1f -> %10.1f cr/qtr\n",
                k_baseline_seated_upkeep, seated_mean.upkeep);
    std::printf("    closing balance %10.1f -> %10.1f cr\n",
                -120334.6, seated_mean.balance);
    check_on_real_spawn(both * 2 > n_rows, "R2",
          "a MAJORITY of swept seeds end the warm start with balance > 0 and net >= 0");

    // ---------------------------------------------------------------------
    // R3 — no clamp, no subsidy. Asserted as the two properties a clamp breaks.
    // ---------------------------------------------------------------------
    std::printf("\n=== R3  NO CLAMP, NO SUBSIDY ===\n");
    {
        // A floor on the balance would make an underwater quarter impossible
        // ANYWHERE in the world, seated or not. Interest is only ever charged on
        // a negative balance, so a non-zero interest term anywhere is direct
        // evidence that a balance was allowed below zero.
        bool any_underwater = false;
        for (const seed_result& r : rows)
            any_underwater = any_underwater || r.seated_dipped
                          || r.rival_mean.interest > 0.0
                          || r.rivals_solvent < r.rivals;
        check(any_underwater, "R3",
              "the constraint still bites — a balance is still allowed below zero");

        // A subsidy would show as the seated corp out-earning the field per
        // holding on income it did not produce. The honest form is the seated
        // corp's income per holding sitting within the field's own range.
        const double seated_per_holding =
            seated_mean.holdings > 0.0 ? seated_mean.income / seated_mean.holdings : 0.0;
        double field_income = 0.0, field_holdings = 0.0;
        for (const seed_result& r : rows)
        {
            field_income   += r.rival_mean.income;
            field_holdings += r.rival_mean.holdings;
        }
        const double field_per_holding =
            field_holdings > 0.0 ? field_income / field_holdings : 0.0;
        std::printf("  income per holding: seated %.1f cr/qtr, field %.1f cr/qtr\n",
                    seated_per_holding, field_per_holding);
        check_on_real_spawn(
              field_per_holding <= 0.0 || seated_per_holding <= 3.0 * field_per_holding,
              "R3", "the seated corp is not out-earning the field per holding "
                    "(no handed subsidy)");
    }

    // ---------------------------------------------------------------------
    // R4 — rivals survive it.
    // ---------------------------------------------------------------------
    std::printf("\n=== R4  THE BACKGROUND FIELD SURVIVES ===\n");
    int total_rivals = 0, total_solvent = 0, holdings_open = 0, holdings_close = 0;
    int total_rival_units = 0, total_rival_heads = 0;
    for (const seed_result& r : rows)
    {
        total_rivals      += r.rivals;
        total_solvent     += r.rivals_solvent;
        holdings_open     += r.field_holdings_open;
        holdings_close    += r.field_holdings_close;
        total_rival_units += r.rival_units;
        total_rival_heads += r.rival_heads;
    }
    const double solvent_pct = total_rivals > 0
                             ? 100.0 * total_solvent / total_rivals : 0.0;
    std::printf("  rivals solvent at close: %d/%d (%.1f%%)   "
                "[pre-BL-635 baseline: %.1f%%]\n",
                total_solvent, total_rivals, solvent_pct, k_baseline_rival_solvent_pct);
    std::printf("  field holdings: %d at generation -> %d after the warm start (%+.1f%%)\n",
                holdings_open, holdings_close,
                holdings_open > 0
                    ? 100.0 * (holdings_close - holdings_open) / holdings_open : 0.0);
    std::printf("  rival standing force: %d units / %d heads   "
                "[pre-BL-635 baseline: %.1f units per seed]\n",
                total_rival_units, total_rival_heads, k_baseline_rival_units_per_seed);

    // R4's requirement reads "solvent enough to KEEP ACTING, so the seed's data
    // is not poisoned by dead corps" — it is a test that the fix did not starve
    // the field, not a bar on the field's absolute wealth. Both halves are
    // asserted; the absolute percentage is REPORTED beside its pre-fix baseline
    // so a regression cannot hide inside a relative test.
    check(holdings_close > holdings_open, "R4",
          "the field is still ACTING — its holding count grew over the warm start");
    check_on_real_spawn(
          total_rivals > 0 && solvent_pct >= k_baseline_rival_solvent_pct - 1.0, "R4",
          "the fix did not starve the field — rival solvency is at or above its "
          "pre-BL-635 baseline");
    check(total_rival_units > 0, "R4",
          "the field still fields a standing force (rivals can still afford to hire)");

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES ABOVE",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
