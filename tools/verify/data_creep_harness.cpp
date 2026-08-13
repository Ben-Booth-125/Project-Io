// Headless harness (BL-251): the DATA-CREEP instrument — the third of the three
// v0.1.0 quality-audit instruments (docs/development/ROADMAP.md § v0.1.0).
//
//   "Entity/pool/market/convoy counters + RSS memory readout run long
//    (100 -> 1000+ ticks): counts and memory must plateau, not climb. A steady
//    climb on an idle run names the unbounded structure."
//
// It builds the REAL generated world (make_hard_coded_world — the honest subject,
// not a hand-built scene), runs the real per-tick sequence that main.cpp /
// app::step_economy run (advance orbits -> dispatch convoys -> advance convoys ->
// run_economy_step -> clear_markets -> apply_budget -> credit convoys -> advance
// surveys), and samples EVERY persistent world container plus process RSS at a
// fixed set of tick marks.
//
// The instrument's whole point is the NAMING: when a counter climbs it prints
// which store grew, by how much, at what per-tick rate, and what that extrapolates
// to. A FAIL here is a finding about the simulation, not a broken harness — do not
// weaken the tolerance to make it green.
//
// BL-254: the rollout also drives CONVOY TRAFFIC. The generated world places no
// launchpad, so `dispatch_convoys`' inter-body lane is gated shut and its intra-body
// lane never fires either — over 1500 ticks the original run dispatched nothing, and
// `world.convoys` / `world.trade_routes` / `world.body_last_glimpse_tick` all sat at
// zero, making their three plateau assertions VACUOUS. Those are the three structures
// with the most reason to grow (`trade_route` is documented as never erased). So the
// harness now seeds one convoy per tick onto a rotating, fully deterministic schedule
// of (corp, body-pair, resource) and lets the REAL supply API carry it — the same
// manufacture-a-convoy pattern app.cpp's `verify.seed_convoy` and
// tools/verify/trade_routes_harness.cpp already use. No src/world change; no RNG.
// See § CONVOY TRAFFIC below.
//
// Hand-builds a recipe_registry (mirrors scripts/economy.lua + scripts/recipes.lua)
// so the harness stays Lua-free, per verifier-headless convention; links the
// SDL/Lua-free world superset (every src/world/*.cpp except recipe_registry.cpp,
// tech_tree.cpp and world_gen_config.cpp).
//
// Build (Linux, from repo root):
//   SRCS=$(ls src/world/*.cpp | grep -v -E 'recipe_registry|tech_tree|world_gen_config')
//   g++ -std=c++20 -O2 -I src tools/verify/data_creep_harness.cpp $SRCS
//       -o build_gen/verify/data_creep_harness && ./build_gen/verify/data_creep_harness
//   (one shell line — continuation elided so the comment stays -Wcomment-clean)
// On Windows via CMake: the tools/verify/*.cpp glob at the foot of CMakeLists.txt
// picks it up — `cmake --build build --target data_creep_harness`.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/market_clearing.hpp"
#include "world/orbital_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/survey_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#if defined(__linux__)
#  include <cstdio>
#  include <unistd.h>
#endif

namespace {

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

// ---------------------------------------------------------------------------
// Resident set size
// ---------------------------------------------------------------------------
// Linux only: /proc/self/statm field 2 is the resident page count. Everything
// else compiles and simply reports RSS as unavailable (-1), and the RSS
// assertion is skipped rather than silently passing.

constexpr long long k_rss_unavailable = -1;

long long rss_kib()
{
#if defined(__linux__)
    std::FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f) return k_rss_unavailable;
    long long total_pages = 0, resident_pages = 0;
    const int n = std::fscanf(f, "%lld %lld", &total_pages, &resident_pages);
    std::fclose(f);
    if (n != 2) return k_rss_unavailable;
    const long long page_kib = static_cast<long long>(sysconf(_SC_PAGESIZE)) / 1024;
    return resident_pages * page_kib;
#else
    return k_rss_unavailable;
#endif
}

// ---------------------------------------------------------------------------
// Registry — mirrors scripts/economy.lua + scripts/recipes.lua for every
// building_type the generator places, so the rollout has real economics.
// (Same hand-build as ai_skill_harness.cpp; kept local so each harness is
// independently readable.)
// ---------------------------------------------------------------------------

std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

recipe_registry make_registry()
{
    recipe_registry reg;

    building_economics extraction;
    extraction.base_rate            = 20.0f;
    extraction.maintenance          = 5.0f;
    extraction.base_wage            = 8.0f;
    extraction.build_cost           = 100.0f;
    extraction.build_duration_ticks = 2.0f;
    extraction.resource_build_cost[ri(resource_type::steel)] = 20.0f;
    reg.set_economics(building_type::extraction_site, extraction);

    building_economics processing;
    processing.base_rate            = 8.0f;
    processing.maintenance          = 10.0f;
    processing.base_wage            = 12.0f;
    processing.build_cost           = 200.0f;
    processing.build_duration_ticks = 3.0f;
    processing.resource_build_cost[ri(resource_type::steel)] = 25.0f;
    reg.set_economics(building_type::processing_facility, processing);

    building_economics port;
    port.maintenance          = 8.0f;
    port.base_wage            = 6.0f;
    port.build_cost           = 150.0f;
    port.build_duration_ticks = 2.0f;
    port.resource_build_cost[ri(resource_type::steel)] = 20.0f;
    reg.set_economics(building_type::port, port);

    building_economics launchpad;
    launchpad.maintenance          = 20.0f;
    launchpad.base_wage            = 15.0f;
    launchpad.build_cost           = 500.0f;
    launchpad.build_duration_ticks = 6.0f;
    launchpad.resource_build_cost[ri(resource_type::steel)]        = 50.0f;
    launchpad.resource_build_cost[ri(resource_type::refined_fuel)] = 20.0f;
    reg.set_economics(building_type::launchpad, launchpad);

    building_economics hub;
    hub.maintenance          = 12.0f;
    hub.base_wage            = 8.0f;
    hub.build_cost           = 250.0f;
    hub.build_duration_ticks = 3.0f;
    hub.resource_build_cost[ri(resource_type::steel)] = 30.0f;
    reg.set_economics(building_type::inland_logistics_hub, hub);

    recipe steel;
    steel.name = "steel";
    steel.inputs[ri(resource_type::iron_ore)] = 2.0f;
    steel.outputs[ri(resource_type::steel)]   = 1.0f;
    reg.add_recipe(steel);

    recipe fuel;
    fuel.name = "refined_fuel";
    fuel.inputs[ri(resource_type::petroleum)]     = 2.0f;
    fuel.outputs[ri(resource_type::refined_fuel)] = 1.0f;
    reg.add_recipe(fuel);

    recipe food;
    food.name = "food_rations";
    food.inputs[ri(resource_type::agricultural_produce)] = 2.0f;
    food.outputs[ri(resource_type::food_rations)]        = 1.0f;
    reg.add_recipe(food);

    return reg;
}

// ---------------------------------------------------------------------------
// CONVOY TRAFFIC (BL-254)
// ---------------------------------------------------------------------------
// Why seeded rather than auto-dispatched: `dispatch_convoys` only creates an
// inter-body convoy for a corp that owns a launchpad on the source body, and the
// generated world places none (app.cpp says as much at `verify.seed_convoy`:
// "without depending on auto-dispatch (which requires launchpads not present in the
// generated world)"). Manufacturing the convoy and handing it to the real
// advance/credit path is the sanctioned pattern for exactly this reason — and it is
// the credit path, not the dispatch path, that owns all three of the structures
// under test (route upsert, glimpse stamp, convoy retirement).
//
// The schedule is a pure function of the tick:
//
//   pair_i = t % pairs.size()          rotating over every ordered body-pair
//   corp_i = (t / pairs.size()) % corps.size()
//   res_i  = t % k_cargo_count
//
// so the run cycles the WHOLE (corp × body-pair) key space of `trade_routes` many
// times over. That is what makes the plateau bind: the route store must saturate at
// its structural bound (distinct corp × unordered-pair keys) early and then stay
// flat while traffic keeps arriving. If the upsert ever failed to dedupe, the store
// would climb linearly for the rest of the run and the instrument would name it.
//
// Speed is fixed at 1/5 so each convoy spends five ticks in flight: `world.convoys`
// must hold a steady ~5 and drain, not accumulate. Cargo is 1 unit — enough to
// exercise the pool credit + market supply injection without drowning the economy
// the other ~30 counters are measuring.

constexpr float k_convoy_speed  = 0.2f;   // 5 ticks in flight
constexpr float k_convoy_cargo  = 1.0f;
constexpr int   k_max_traffic_corps = 3;  // player + the two lowest-id others

const resource_type k_cargo[] = {
    resource_type::iron_ore,
    resource_type::petroleum,
    resource_type::agricultural_produce,
};
constexpr int k_cargo_count = static_cast<int>(sizeof(k_cargo) / sizeof(k_cargo[0]));

struct traffic_plan
{
    std::vector<entity_id> corps;                                  ///< Dispatching corps.
    std::vector<std::pair<entity_id, entity_id>> market_pairs;     ///< Ordered market pairs.
    long long seeded = 0;
    long long stub_markets = 0;   ///< Markets the plan had to author (see below).
    /// Structural bound on trade_routes this schedule can ever produce:
    /// distinct corps x distinct unordered body-pairs.
    long long route_key_bound = 0;
};

/// Build the schedule from the generated world. Deterministic: every collection is
/// gathered in ascending entity-id order (w.markets / w.corporations / w.bodies are
/// ordered maps).
///
/// Takes a mutable world because it may have to AUTHOR markets. Measured 2026-08-01:
/// the generated world's six markets all sit on the single tiled body (Kepler), so the
/// world contains **no inter-body market pair at all** — a second reason, independent
/// of the launchpad gate, that the original rollout could not produce a single trade
/// route. A route is body-level and intra-body lanes record nothing, so the plan seeds
/// a minimal stub market on each remaining non-star body, exactly as app.cpp's
/// `verify.seed_convoy` does ("If the destination body has no market a minimal one is
/// created"). The stubs are authored ONCE, before tick 1 — a fixed additive offset on
/// world.markets / entity count, not a per-tick allocation, so they cannot manufacture
/// a plateau. Their count is reported.
traffic_plan plan_traffic(world& w)
{
    traffic_plan plan;

    // Player corp first (its convoys are the ones that stamp glimpses — see
    // credit_arrived_convoys, which gates record_proximity_glimpses on the player),
    // then the lowest-id others, so the schedule exercises both branches.
    if (w.player_entity != null_entity && w.corporations.count(w.player_entity) != 0)
        plan.corps.push_back(w.player_entity);
    for (const auto& [cid, corp] : w.corporations)
    {
        if (static_cast<int>(plan.corps.size()) >= k_max_traffic_corps) break;
        if (cid != w.player_entity) plan.corps.push_back(cid);
    }

    // One market per body (the lowest-id one), so a pair is always an inter-body lane
    // — an intra-body lane records no route and would not exercise the store.
    std::vector<std::pair<entity_id, entity_id>> body_market; // (body, market)
    for (const auto& [mid, mc] : w.markets)
    {
        if (mc.body == null_entity) continue;
        const bool seen = std::any_of(body_market.begin(), body_market.end(),
                                      [&](const auto& bm) { return bm.first == mc.body; });
        if (!seen) body_market.emplace_back(mc.body, mid);
    }

    // Author the missing endpoints (see the note on this function).
    std::vector<entity_id> unmarketed;
    for (const auto& [bid, bc] : w.bodies)
    {
        if (bc.type == body_type::star) continue;
        const bool has = std::any_of(body_market.begin(), body_market.end(),
                                     [&](const auto& bm) { return bm.first == bid; });
        if (!has) unmarketed.push_back(bid);
    }
    for (const entity_id bid : unmarketed)
    {
        const entity_id mid = w.create_entity();
        market_component mc{};
        mc.body = bid;
        w.markets[mid] = mc;
        body_market.emplace_back(bid, mid);
        ++plan.stub_markets;
    }

    std::sort(body_market.begin(), body_market.end());

    for (std::size_t i = 0; i < body_market.size(); ++i)
        for (std::size_t j = 0; j < body_market.size(); ++j)
            if (i != j)
                plan.market_pairs.emplace_back(body_market[i].second, body_market[j].second);

    const long long n = static_cast<long long>(body_market.size());
    plan.route_key_bound = static_cast<long long>(plan.corps.size()) * (n * (n - 1) / 2);
    return plan;
}

/// Seed the tick's scheduled convoy. Mirrors app.cpp's `verify.seed_convoy` — build a
/// convoy_component and push it; the real advance_convoys / credit_arrived_convoys
/// pair does everything else (route upsert, glimpse stamp, pool credit, retirement).
void seed_scheduled_convoy(world& w, traffic_plan& plan, int t)
{
    if (plan.corps.empty() || plan.market_pairs.empty()) return;

    const std::size_t np = plan.market_pairs.size();
    const std::size_t pi = static_cast<std::size_t>(t) % np;
    const std::size_t ci = (static_cast<std::size_t>(t) / np) % plan.corps.size();

    convoy_component cv{};
    cv.source_market  = plan.market_pairs[pi].first;
    cv.dest_market    = plan.market_pairs[pi].second;
    cv.mode           = convoy_mode::space;
    cv.cargo_resource = k_cargo[t % k_cargo_count];
    cv.cargo_qty      = k_convoy_cargo;
    cv.progress       = 0.0f;
    cv.speed          = k_convoy_speed;
    cv.corp           = plan.corps[ci];
    cv.arrived        = false;
    w.convoys.push_back(cv);
    ++plan.seeded;
}

// ---------------------------------------------------------------------------
// The counter set
// ---------------------------------------------------------------------------
// Every PERSISTENT world container, plus the per-tick transient report sizes
// (a transient whose size climbs is itself a symptom: the world state it is
// derived from is growing). Order is fixed; `k_counter_names` indexes it.

enum counter : int
{
    c_entities = 0,        ///< Distinct entity ids appearing in any store.
    c_bodies,
    c_tiles,
    c_buildings,
    c_stockpiles,
    c_markets,
    c_units,
    c_population_centres,
    c_population_centre_tile,
    c_population_centre_name,
    c_nations,
    c_tile_to_nation,
    c_corporations,
    c_corp_assets_total,   ///< Sum over corporations of assets.size().
    c_nation_tiles_total,  ///< Sum over nations of tiles.size().
    c_corp_body_pools,
    c_convoys,
    c_trade_routes,
    c_glimpse_stamps,
    c_body_tile_index,     ///< Bodies with a built raster index.
    c_body_tile_cells,     ///< Total cells across those indices.
    c_astar_cache,
    c_ai_decisions,        ///< Ring occupancy (capped at 256 by construction).
    c_ai_decisions_total,  ///< Lifetime pushes — EXPECTED to climb; reported, not asserted.
    c_workforce_overrides,
    c_rep_buildings,       ///< Transient: economy_report.buildings.
    c_rep_agency_events,   ///< Transient: economy_report.agency_events.
    c_rep_purchases,       ///< Transient: economy_report.purchases.
    c_rep_contention,      ///< Transient: economy_report.workforce_contention.
    c_rep_budgets,         ///< Transient: economy_report.budgets.
    c_flows,               ///< Transient: clear_markets' per-corp cash-flow map.
    c_count
};

const char* const k_counter_names[c_count] = {
    "entities (distinct ids)",
    "world.bodies",
    "world.tiles",
    "world.buildings",
    "world.stockpiles",
    "world.markets",
    "world.units",
    "world.population_centres",
    "world.population_centre_tile",
    "world.population_centre_name",
    "world.nations",
    "world.tile_to_nation",
    "world.corporations",
    "corporation.assets (sum)",
    "nation.tiles (sum)",
    "world.corp_body_pools",
    "world.convoys",
    "world.trade_routes",
    "world.body_last_glimpse_tick",
    "world.body_tile_index (bodies)",
    "world.body_tile_index (cells)",
    "world.astar_cost_cache",
    "world.ai_decisions.entries",
    "world.ai_decisions.total [informational]",
    "world.workforce_supply_overrides",
    "economy_report.buildings [transient]",
    "economy_report.agency_events [transient]",
    "economy_report.purchases [transient]",
    "economy_report.workforce_contention [transient]",
    "economy_report.budgets [transient]",
    "clear_markets flows [transient]",
};

/// `ai_decisions.total` is a lifetime counter by design (BL-202 telemetry), so it
/// climbs on purpose. It is sampled and printed but excluded from the assertion.
bool informational(int c) { return c == c_ai_decisions_total; }

struct sample
{
    int       tick = 0;
    long long v[c_count]{};
    long long rss = k_rss_unavailable;
};

long long distinct_entities(const world& w)
{
    std::vector<entity_id> ids;
    ids.reserve(w.tiles.size() + w.buildings.size() + w.bodies.size() + 64);
    for (const auto& kv : w.bodies)             ids.push_back(kv.first);
    for (const auto& kv : w.tiles)              ids.push_back(kv.first);
    for (const auto& kv : w.buildings)          ids.push_back(kv.first);
    for (const auto& kv : w.stockpiles)         ids.push_back(kv.first);
    for (const auto& kv : w.markets)            ids.push_back(kv.first);
    for (const auto& kv : w.units)              ids.push_back(kv.first);
    for (const auto& kv : w.population_centres) ids.push_back(kv.first);
    for (const auto& kv : w.nations)            ids.push_back(kv.first);
    for (const auto& kv : w.corporations)       ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return static_cast<long long>(ids.size());
}

sample take_sample(int tick, const world& w, const economy_report& rep,
                   const std::unordered_map<entity_id, corp_cash_flow>& flows)
{
    sample s;
    s.tick = tick;

    s.v[c_entities]                 = distinct_entities(w);
    s.v[c_bodies]                   = static_cast<long long>(w.bodies.size());
    s.v[c_tiles]                    = static_cast<long long>(w.tiles.size());
    s.v[c_buildings]                = static_cast<long long>(w.buildings.size());
    s.v[c_stockpiles]               = static_cast<long long>(w.stockpiles.size());
    s.v[c_markets]                  = static_cast<long long>(w.markets.size());
    s.v[c_units]                    = static_cast<long long>(w.units.size());
    s.v[c_population_centres]       = static_cast<long long>(w.population_centres.size());
    s.v[c_population_centre_tile]   = static_cast<long long>(w.population_centre_tile.size());
    s.v[c_population_centre_name]   = static_cast<long long>(w.population_centre_name.size());
    s.v[c_nations]                  = static_cast<long long>(w.nations.size());
    s.v[c_tile_to_nation]           = static_cast<long long>(w.tile_to_nation.size());
    s.v[c_corporations]             = static_cast<long long>(w.corporations.size());

    long long assets = 0;
    for (const auto& [cid, corp] : w.corporations)
        assets += static_cast<long long>(corp.assets.size());
    s.v[c_corp_assets_total] = assets;

    long long nation_tiles = 0;
    for (const auto& [nid, nat] : w.nations)
        nation_tiles += static_cast<long long>(nat.tiles.size());
    s.v[c_nation_tiles_total] = nation_tiles;

    s.v[c_corp_body_pools]  = static_cast<long long>(w.corp_body_pools.size());
    s.v[c_convoys]          = static_cast<long long>(w.convoys.size());
    s.v[c_trade_routes]     = static_cast<long long>(w.trade_routes.size());
    s.v[c_glimpse_stamps]   = static_cast<long long>(w.body_last_glimpse_tick.size());
    s.v[c_body_tile_index]  = static_cast<long long>(w.body_tile_index.size());

    long long cells = 0;
    for (const auto& [bid, grid] : w.body_tile_index)
        cells += static_cast<long long>(grid.size());
    s.v[c_body_tile_cells] = cells;

    s.v[c_astar_cache]          = static_cast<long long>(w.astar_cost_cache.size());
    s.v[c_ai_decisions]         = static_cast<long long>(w.ai_decisions.entries.size());
    s.v[c_ai_decisions_total]   = static_cast<long long>(w.ai_decisions.total);
    s.v[c_workforce_overrides]  = static_cast<long long>(w.workforce_supply_overrides.size());

    s.v[c_rep_buildings]      = static_cast<long long>(rep.buildings.size());
    s.v[c_rep_agency_events]  = static_cast<long long>(rep.agency_events.size());
    s.v[c_rep_purchases]      = static_cast<long long>(rep.purchases.size());
    s.v[c_rep_contention]     = static_cast<long long>(rep.workforce_contention.size());
    s.v[c_rep_budgets]        = static_cast<long long>(rep.budgets.size());
    s.v[c_flows]              = static_cast<long long>(flows.size());

    s.rss = rss_kib();
    return s;
}

// ---------------------------------------------------------------------------
// The plateau definition (stated in the output too — it is part of the finding)
// ---------------------------------------------------------------------------
// A counter PLATEAUS when its value at the final sample is within tolerance of
// its value at the mid sample:
//
//     |v(T_final) - v(T_mid)|  <=  max(k_abs_tol, k_rel_tol * v(T_mid))
//
// with k_abs_tol = 8 counts (absorbs small integer jitter in tiny stores — a
// convoy or two in flight at the sampling instant) and k_rel_tol = 2%.
// The window is deliberately the LATE half of the run: a structure that fills
// during warm-up and then holds is fine; one still climbing after 500 ticks is
// not. RSS uses its own tolerance — max(4 MiB, 5%) — because the allocator
// returns pages to the OS lazily and page-cache noise is not a leak.

constexpr long long k_abs_tol     = 8;
constexpr double    k_rel_tol     = 0.02;
constexpr long long k_rss_abs_tol = 4 * 1024; // KiB
constexpr double    k_rss_rel_tol = 0.05;

long long tolerance_for(long long mid)
{
    const long long rel = static_cast<long long>(k_rel_tol * static_cast<double>(mid < 0 ? 0 : mid));
    return std::max(k_abs_tol, rel);
}

} // namespace

int main(int argc, char* argv[])
{
    // Tick ceiling is overridable so the instrument can be run harder by hand;
    // the default is the ROADMAP's "1000+".
    // RE-CALIBRATED 2026-08-12 for the 3x map (180x84 -> 312x145, 15,120 ->
    // 45,240 tiles). The old schedule topped out at 1500 ticks, and on the
    // larger world the build-out has NOT saturated by then: entities, buildings,
    // stockpiles and corp assets were all still climbing at the final sample.
    //
    // That was not a leak. Those four counters are exactly the ones with an
    // obvious driver — corps are still finding legal sites, because three times
    // the tiles takes proportionally longer to fill. A plateau instrument has to
    // reach saturation before "still climbing" means anything, so the schedule
    // scales with the world rather than the world outgrowing the schedule.
    std::vector<int> marks = { 300, 750, 1500, 3000, 4500 };
    if (argc > 1)
    {
        const int ceiling = std::max(100, std::atoi(argv[1]));
        marks.clear();
        for (const int m : { 300, 750, 1500, 3000, 4500, 6000, 12000 })
            if (m <= ceiling) marks.push_back(m);
        if (marks.empty() || marks.back() != ceiling) marks.push_back(ceiling);
    }
    const int total_ticks = marks.back();

    // T_mid is the last mark at or below a THIRD of the run — the plateau
    // window's left edge, so the assertion covers the late two thirds.
    //
    // Derived rather than hardcoded (it was the literal 500, which silently
    // stopped being a third of the run the moment the schedule changed). Now an
    // overridden ceiling moves the window with it instead of leaving the
    // instrument asserting over whatever fraction the old constant happens to
    // name.
    int mid_tick = marks.front();
    for (const int m : marks)
        if (m <= total_ticks / 3) mid_tick = m;

    std::printf("BL-251 data-creep instrument (+BL-254 convoy traffic) — real generated world,"
                " %d ticks\n", total_ticks);
    std::printf("Sequence per tick: [seed convoy] -> advance_orbits -> dispatch_convoys ->\n"
                "                   advance_convoys -> run_economy_step -> clear_markets ->\n"
                "                   apply_budget -> credit_arrived_convoys -> advance_surveys\n"
                "                   (mirrors main.cpp; the seed is the harness's own, BL-254)\n\n");

    std::printf("PLATEAU DEFINITION\n");
    std::printf("  A counter plateaus iff  |v(T_final) - v(T_mid)| <= max(%lld, %.0f%% of v(T_mid))\n",
                k_abs_tol, k_rel_tol * 100.0);
    std::printf("  with T_mid = %d ticks and T_final = %d ticks (the late half of the run).\n",
                mid_tick, total_ticks);
    std::printf("  RSS uses max(%lld KiB, %.0f%%) — allocators return pages lazily.\n",
                k_rss_abs_tol, k_rss_rel_tol * 100.0);
    std::printf("  `ai_decisions.total` is a lifetime telemetry counter by design and is\n"
                "  reported but NOT asserted.\n\n");

    const auto t_build0 = std::chrono::steady_clock::now();
    world w = make_hard_coded_world(no_prehistory());
    const recipe_registry reg = make_registry();

    // Author default recipes onto generated processors (mirrors app::load_economy
    // / main.cpp's blackboard export) so processing facilities actually run.
    const uint16_t default_recipe = reg.recipe_id("steel");
    for (auto& [id, b] : w.buildings)
        if (b.type == building_type::processing_facility && b.recipe == no_recipe)
            b.recipe = default_recipe;

    const auto t_build1 = std::chrono::steady_clock::now();
    const double build_s =
        std::chrono::duration<double>(t_build1 - t_build0).count();

    std::printf("World generated in %.2f s: %zu bodies, %zu tiles, %zu buildings, %zu markets,\n"
                "  %zu corporations, %zu nations, %zu population centres.\n\n",
                build_s, w.bodies.size(), w.tiles.size(), w.buildings.size(), w.markets.size(),
                w.corporations.size(), w.nations.size(), w.population_centres.size());

    std::vector<sample> samples;
    samples.reserve(marks.size());
    std::size_t next_mark = 0;

    // Run-wide maxima for the SHORT-LIVED structures. A convoy lives a handful of
    // ticks, so a sample-point reading of 0 does not prove none was ever dispatched
    // — and if none ever was, the convoy/route/glimpse counters were never
    // exercised and their plateau is vacuous. Tracked every tick so the coverage
    // note below can say which it is.
    long long max_convoys = 0, max_routes = 0, max_glimpses = 0, max_astar = 0;
    long long max_flows = 0, max_purchases = 0;

    // BL-254: the convoy schedule. Built once from the generated world, then replayed
    // deterministically. `routes_saturated_at` records the first tick after which
    // trade_routes stopped growing — the honest measure of "it saturated, then held".
    traffic_plan plan = plan_traffic(w);
    long long routes_saturated_at = 0;
    long long completed_convoys   = 0;

    std::printf("CONVOY TRAFFIC (BL-254) — deterministic seeded schedule\n");
    std::printf("  %zu corps x %zu ordered market pairs; one convoy seeded per tick,\n"
                "  speed %.2f (%d ticks in flight), cargo %.0f unit rotating over %d resources.\n",
                plan.corps.size(), plan.market_pairs.size(), k_convoy_speed,
                static_cast<int>(1.0f / k_convoy_speed), k_convoy_cargo, k_cargo_count);
    std::printf("  Structural bound on world.trade_routes for this schedule: %lld keys\n"
                "  (corps x distinct unordered body-pairs). Full schedule cycle: %zu ticks.\n",
                plan.route_key_bound, plan.market_pairs.size() * plan.corps.size());
    if (plan.stub_markets > 0)
        std::printf("  FINDING: the generated world had markets on only ONE body, so it holds no\n"
                    "  inter-body market pair and CANNOT record a trade route however long it runs.\n"
                    "  The plan authored %lld stub market(s) once, pre-run, to give the lanes\n"
                    "  endpoints (the verify.seed_convoy pattern). world.markets is offset by that\n"
                    "  constant throughout; it is not a per-tick allocation.\n",
                    plan.stub_markets);
    std::printf("\n");

    const auto t_run0 = std::chrono::steady_clock::now();
    for (int t = 1; t <= total_ticks; ++t)
    {
        w.current_day_tick = t;
        const long long routes_before  = static_cast<long long>(w.trade_routes.size());
        const long long seeded_before  = plan.seeded;
        const long long convoys_before = static_cast<long long>(w.convoys.size());
        seed_scheduled_convoy(w, plan, t);
        advance_orbits(w, 1.0);
        dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                         reg.logistics_cost(convoy_mode::space));
        advance_convoys(w);
        economy_report rep = run_economy_step(w, reg);
        auto flows = clear_markets(w, reg, rep);
        apply_budget(w, reg, flows, rep.workforce_contention, &rep.budgets);
        credit_arrived_convoys(w, t);
        advance_surveys(w, 1);

        max_convoys  = std::max(max_convoys,  static_cast<long long>(w.convoys.size()));
        max_routes   = std::max(max_routes,   static_cast<long long>(w.trade_routes.size()));
        max_glimpses = std::max(max_glimpses, static_cast<long long>(w.body_last_glimpse_tick.size()));
        max_astar    = std::max(max_astar,    static_cast<long long>(w.astar_cost_cache.size()));
        max_flows     = std::max(max_flows,     static_cast<long long>(flows.size()));
        max_purchases = std::max(max_purchases, static_cast<long long>(rep.purchases.size()));

        // A convoy that left the store this tick was credited + retired. Exact, and it
        // does NOT assume a seed happened: (held + seeded) - held_now. A LOWER bound if
        // dispatch_convoys ever fires (its additions would mask a retirement); it does
        // not fire in this world — see the STILL UNEXERCISED note.
        completed_convoys += convoys_before + (plan.seeded - seeded_before)
                             - static_cast<long long>(w.convoys.size());
        if (static_cast<long long>(w.trade_routes.size()) != routes_before)
            routes_saturated_at = t;

        if (next_mark < marks.size() && t == marks[next_mark])
        {
            samples.push_back(take_sample(t, w, rep, flows));
            ++next_mark;
        }
    }
    const auto t_run1 = std::chrono::steady_clock::now();
    const double run_s = std::chrono::duration<double>(t_run1 - t_run0).count();

    // --- the table ---------------------------------------------------------
    std::printf("COUNTERS AT EACH SAMPLE\n");
    std::printf("  %-48s", "counter");
    for (const auto& s : samples) std::printf("%10d", s.tick);
    std::printf("%14s\n", "delta(mid->end)");

    // A plateau is a comparison between two DISTINCT samples. With a single sample
    // (e.g. `data_creep_harness 100`, which collapses the mark list to one entry)
    // mid_i == end_i, every delta is trivially 0, and R1 would report a green
    // plateau having compared a sample against itself. Refuse instead: a vacuous
    // pass from an instrument built to catch silent growth is the worst outcome
    // available to it.
    if (samples.size() < 2)
    {
        std::printf("\nFAILURES (1)\n  [FAIL] R0 need at least two samples to test a plateau"
                    " (got %zu; raise the tick ceiling)\n", samples.size());
        return 1;
    }

    // Locate T_mid: the last sample at or below a THIRD of the run, matching the
    // rule the header printed above.
    //
    // THIS WAS A SECOND, INDEPENDENT HARDCODED 500 (fixed 2026-08-12). The
    // printed T_mid and the T_mid the assertion actually used were derived in
    // two different places from the same literal, so they agreed only by
    // coincidence. Rescaling the mark schedule for the 3x map desynced them
    // instantly: the header announced "T_mid = 1500" while the delta was still
    // measured from tick 300, which reported world.buildings as CLIMBING by 270
    // when the true delta over the announced window was ZERO — the counter had
    // saturated at 731 by tick 750 and never moved again.
    //
    // A plateau instrument that measures a different window than the one it
    // names is worse than no instrument, so the rule now lives in one place.
    const int mid_cutoff = samples.back().tick / 3;
    std::size_t mid_i = 0;
    for (std::size_t i = 0; i + 1 < samples.size(); ++i)
        if (samples[i].tick <= mid_cutoff) mid_i = i;
    const std::size_t end_i = samples.size() - 1;

    struct climber { int c; long long mid, end; };
    std::vector<climber> climbers;

    for (int c = 0; c < c_count; ++c)
    {
        std::printf("  %-48s", k_counter_names[c]);
        for (const auto& s : samples) std::printf("%10lld", s.v[c]);
        const long long d = samples[end_i].v[c] - samples[mid_i].v[c];
        std::printf("%14lld\n", d);

        if (informational(c)) continue;
        if (std::llabs(d) > tolerance_for(samples[mid_i].v[c]))
            climbers.push_back({ c, samples[mid_i].v[c], samples[end_i].v[c] });
    }

    // --- RSS ---------------------------------------------------------------
    std::printf("\n  %-48s", "RSS (KiB)");
    const bool rss_ok = samples[end_i].rss != k_rss_unavailable;
    for (const auto& s : samples)
    {
        if (s.rss == k_rss_unavailable) std::printf("%10s", "n/a");
        else                            std::printf("%10lld", s.rss);
    }
    if (rss_ok) std::printf("%14lld\n", samples[end_i].rss - samples[mid_i].rss);
    else        std::printf("%14s\n", "n/a");
    if (!rss_ok)
        std::printf("  (RSS readout unavailable on this platform — /proc/self/statm is Linux-only.)\n");

    // --- the naming --------------------------------------------------------
    std::printf("\nGROWTH FINDINGS\n");
    if (climbers.empty())
    {
        std::printf("  None. Every asserted counter is inside its plateau tolerance over\n"
                    "  ticks %d -> %d.\n", samples[mid_i].tick, samples[end_i].tick);
    }
    else
    {
        const int span = samples[end_i].tick - samples[mid_i].tick;
        for (const climber& cl : climbers)
        {
            const long long d    = cl.end - cl.mid;
            const double    rate = span > 0 ? static_cast<double>(d) / span : 0.0;
            std::printf("  CLIMBING: %s\n", k_counter_names[cl.c]);
            std::printf("            %lld -> %lld over ticks %d..%d  (delta %+lld, %.4f/tick)\n",
                        cl.mid, cl.end, samples[mid_i].tick, samples[end_i].tick, d, rate);
            std::printf("            tolerance was %lld; full trace:",
                        tolerance_for(cl.mid));
            for (const auto& s : samples) std::printf("  t%d=%lld", s.tick, s.v[cl.c]);
            std::printf("\n");
            std::printf("            extrapolated to 10000 ticks: ~%lld\n",
                        cl.end + static_cast<long long>(rate * (10000 - samples[end_i].tick)));
        }
    }

    // --- coverage honesty --------------------------------------------------
    // A counter that stayed at zero for the whole run was not proven bounded —
    // it was never exercised. Say so rather than banking a vacuous plateau.
    std::printf("\nCOVERAGE (run-wide maxima, sampled every tick — not just at the marks)\n");
    std::printf("  world.convoys                max %lld\n", max_convoys);
    std::printf("  world.trade_routes           max %lld\n", max_routes);
    std::printf("  world.body_last_glimpse_tick max %lld\n", max_glimpses);
    std::printf("  world.astar_cost_cache       max %lld\n", max_astar);
    std::printf("  clear_markets flows          max %lld\n", max_flows);
    std::printf("  economy_report.purchases     max %lld\n", max_purchases);
    if (max_flows == 0)
        std::printf("  NOTE: clear_markets returned no cash flows on ANY tick, so the\n"
                    "        market/order-book side of this rollout is idle — its plateau\n"
                    "        is likewise vacuous. Not a creep finding; a coverage gap.\n");
    if (max_convoys == 0)
        std::printf("  NOTE: no convoy existed on ANY tick, so the convoy / trade-route /\n"
                    "        glimpse-stamp plateaus above are VACUOUS — those structures were\n"
                    "        never exercised. `world.trade_routes` in particular is documented\n"
                    "        as never-erased (components.hpp § trade_route), i.e. accumulating\n"
                    "        by construction; such a rollout does not bound it. The BL-254\n"
                    "        seed schedule is supposed to prevent this — if you see this note,\n"
                    "        the schedule itself is broken (see R4).\n");
    else
    {
        std::printf("\n  Convoy cycling (BL-254): %lld seeded, %lld credited + retired,"
                    " %lld still in flight at the end.\n",
                    plan.seeded, completed_convoys, static_cast<long long>(w.convoys.size()));
        std::printf("  world.trade_routes reached %lld of a structural bound of %lld keys;"
                    " last grew at tick %lld\n"
                    "  (i.e. flat for the final %d ticks while traffic kept arriving).\n",
                    static_cast<long long>(w.trade_routes.size()), plan.route_key_bound,
                    routes_saturated_at, total_ticks - static_cast<int>(routes_saturated_at));
        std::printf("  world.body_last_glimpse_tick reached %lld (bounded by bodies: %zu);"
                    " stamps are\n  overwritten in place by the player's lanes, not appended.\n",
                    max_glimpses, w.bodies.size());
    }

    // What this rollout STILL does not exercise — named rather than left implied.
    std::printf("\n  STILL UNEXERCISED by this rollout:\n");
    std::printf("    - `dispatch_convoys` auto-dispatch. Inter-body lanes are launchpad-gated\n"
                "      and the generated world places no launchpad; the intra-body branch also\n"
                "      never fired here. The BL-254 traffic is SEEDED (the trade_routes_harness\n"
                "      / verify.seed_convoy pattern), so what is under test is the credit +\n"
                "      upsert + retire path, not the dispatch decision. A launchpad-bearing\n"
                "      scenario would close that.\n");
    if (max_flows == 0)
        std::printf("    - the market order-book / cash-flow path (max flows = 0, above).\n");

    // --- assertions --------------------------------------------------------
    std::printf("\nASSERTIONS\n");
    check(climbers.empty(),
          "R1 every persistent world counter plateaus between the mid and final sample");

    if (rss_ok)
    {
        const long long rss_delta = samples[end_i].rss - samples[mid_i].rss;
        const long long rss_tol   = std::max(k_rss_abs_tol,
            static_cast<long long>(k_rss_rel_tol * static_cast<double>(samples[mid_i].rss)));
        if (rss_delta > rss_tol)
            std::printf("  CLIMBING: process RSS  %lld -> %lld KiB over ticks %d..%d "
                        "(delta %+lld KiB, tolerance %lld KiB)\n",
                        samples[mid_i].rss, samples[end_i].rss,
                        samples[mid_i].tick, samples[end_i].tick, rss_delta, rss_tol);
        check(rss_delta <= rss_tol,
              "R2 process RSS plateaus between the mid and final sample");
    }
    else
    {
        std::printf("  [SKIP] R2 process RSS plateau — RSS readout unavailable on this platform\n");
    }

    check(samples.size() == marks.size(),
          "R3 every requested sample mark was reached (the run completed)");

    // BL-254 coverage assertion. R1's convoy / trade-route / glimpse plateaus are only
    // worth anything if those structures were actually driven; without this, a future
    // change that silently stops the schedule would restore the vacuous green pass the
    // instrument was built to refuse. Asserted, not merely reported.
    const bool exercised = max_convoys > 0 && max_routes > 0 && max_glimpses > 0
                           && completed_convoys > 0;
    if (!exercised)
        std::printf("  convoys max=%lld routes max=%lld glimpses max=%lld completed=%lld\n",
                    max_convoys, max_routes, max_glimpses, completed_convoys);
    check(exercised,
          "R4 convoys / trade routes / glimpse stamps were non-zero and repeatedly cycled "
          "(R1's plateau for them is not vacuous)");

    std::printf("\nRuntime: world gen %.2f s + %d ticks in %.2f s (%.1f ms/tick).\n",
                build_s, total_ticks, run_s, 1000.0 * run_s / total_ticks);
    std::printf("%s  (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
