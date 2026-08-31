// chain_depth — the BL-428 growth spine: how far down the production graph a
// corp can reach, computed off the recipe graph.
//
// WHY THIS METRIC AT ALL. Ben's ruling, 2026-08-15, choosing chain depth over
// building tiers, the ancient tech ladder, and settlement scale: every
// alternative is a SECOND system that has to be kept in agreement with the
// economy, and the project has already paid that reconciliation cost once
// (BL-155 and BL-156 both settled on the same condition_set and neither built
// it). Depth is read off the graph that has to exist anyway.
//
// TWO COMPOSITION RULES, and they differ on purpose:
//   * MAX within a recipe  — you cannot run it until its deepest input exists.
//   * MIN across recipes   — two routes to a good means you have reached it as
//                            soon as the EASIER one is open.
// The asymmetry only starts to bite when BL-430 adds alternate methods, which is
// exactly why it is pinned by a test now rather than discovered then.
//
// WHAT IS ASSERTED HERE (BL-428, then BL-432). The D/G rows cover the METRIC —
// they covered the GATE too until BL-692 retired it. The R rows are BL-432's
// roster invariants, which needed a fuller roster to be meaningful. BL-432's
// third assertion — every building reachable — was G3, and passed to G5 when
// BL-692 retired G3 as vacuous.
//
//   D1  Raws are depth 0, and a good is deeper than every one of its inputs.
//   D2  MAX-within-a-recipe: a two-input recipe takes the deeper input's depth.
//   D3  MIN-across-recipes: a second, shallower route lowers a good's depth.
//   D4  Cycles and input orphans are UNREACHABLE (-1), not an infinite loop and
//       not a fabricated number. This is the row that would catch the metric
//       silently succeeding on a graph it cannot actually evaluate.
//   D5  Determinism: the depth vector is identical across two loads, and does
//       not depend on the order recipes are added.
//   D6  Against the REAL authored economy: depth is well-defined for every good
//       the shipped recipes produce, and the ancient band is not deeper than the
//       industrial one (BL-433 masks routes out; masking must never ADD depth).
//
// THE GATE (BL-428 second half, 2026-08-16) — RETIRED BY BL-692 (2026-08-29).
// Chain depth no longer gates a build or a retool anywhere; tech is the only lock
// on a method. What survives here is the METRIC, which is still live code:
//
//   G1  A recipe's required depth is its DEEPEST input's depth, and 0 for an
//       input-free recipe. Derived, so it cannot drift from the graph.
//   G2  RETIRED as vacuous by BL-692 — asserted monotonicity of reached depth,
//       whose stated purpose was "a placement that was legal must not become
//       illegal". No placement is depth-legal any more. Retired, not weakened:
//       it was passing, and would have gone on passing over a dead property.
//   G3  RETIRED as vacuous by BL-692 — asserted every ancient recipe is
//       reachable by simulating the climb the gate forced. With no gate every
//       ancient recipe is placeable at tick 0 by construction, so the row could
//       not fail for any authoring mistake. G5 now owns "nothing is stranded",
//       and owns it more strictly (an exact opening, not a reachability floor).
//   G4  Determinism: the required-depth vector is byte-identical across two
//       loads, and independent of insertion order (the BL-406 lesson).
//
// THE ROSTER INVARIANTS (BL-432, 2026-08-16):
//
//   R1  No orphan resources, EITHER direction: every resource_type is obtainable
//       (a recipe produces it or a deposit yields it) and wanted (a recipe
//       consumes it, or a named actor does, via an explicit exemption table).
//       BL-286 is the precedent this exists for — eleven values added with
//       "behaviour unfiled", and no diff review ever caught the ones with no
//       consumer.
//   R1b No orphan-in-a-BAND resources (BL-460, 2026-08-19): R1 only checks a
//       consumer EXISTS somewhere in the roster, not that producer and
//       consumer are reachable under the SAME concrete campaign band —
//       `ordnance` passed R1 (BL-454's upkeep is its named consumer) while
//       being genuinely unproducible at the shipped ancient-band default. This
//       row generalises the check per band; known, tracked, out-of-scope gaps
//       of the same shape are named rather than silently passed (NR-355).
//   R2  No dominant production method — but only between recipes a corp can
//       actually choose between. See the axis note at the row itself: disjoint-raw
//       siblings are supply routes, not methods, and comparing them on price is
//       the wrong question. Every sibling pair is bucketed, so none escapes by
//       being unclassifiable.

#include "scripting/lua_state.hpp"
#include "world/components.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp" // BL-648: NAME the good in a failure, never an id
#include "world/supply_system.hpp"  // BL-648: the launch draw declares what it burns
#include "world/tech_gate.hpp" // BL-589: the start-gate audit reads recipe_unlocked/advance_tech_gates
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok)
        ++failures;
}

/// Build a recipe by resource ids, so the fixtures below read as graphs rather
/// than as tables of floats.
recipe make(const std::string& name,
            const std::vector<std::pair<resource_type, float>>& in,
            const std::vector<std::pair<resource_type, float>>& out)
{
    recipe r;
    r.name = name;
    for (const auto& [res, q] : in)
        r.inputs[static_cast<std::size_t>(res)] = q;
    for (const auto& [res, q] : out)
        r.outputs[static_cast<std::size_t>(res)] = q;
    return r;
}

// Stand-in goods for the synthetic graphs. Which real resources these are does
// not matter — the fixtures test the metric, not the economy.
constexpr resource_type RAW_A = resource_type::iron_ore;
constexpr resource_type RAW_B = resource_type::coal;
constexpr resource_type MID   = resource_type::steel;
constexpr resource_type DEEP  = resource_type::machinery;
constexpr resource_type FAR   = resource_type::alloys;

// ---------------------------------------------------------------------------
// THE INJECTOR REGISTRY (BL-648 — an exemption must name a pass that injects)
// ---------------------------------------------------------------------------
//
// WHAT WENT WRONG. R1's exemption table let a resource pass the orphan check by
// NAMING a consumer in prose — "mercantile demand, terminal artisan good". A
// name is not a pass. Ten goods rested on a *mercantile demand* that has never
// existed: market_clearing.cpp has exactly three demand injections and none of
// them is it. The row was green throughout, which is how the ancient roster came
// to terminate in artisan goods nobody buys. MARKETS.md § Demand channels states
// the rule this registry enforces: **a consumer is a MECHANISM, not a noun** —
// a good is wanted when some pass adds to a market's `demand` for it, or draws
// it from a pool.
//
// WHERE THE REGISTRY IS DERIVED FROM, and why it cannot drift (R4). Every entry
// resolves through the SAME object the running pass multiplies by. Not one line
// below contains a hand-written resource name, which is the whole point — a
// second hand-maintained list beside the first is the loophole rebuilt:
//
//   population_demand           reg.population_demand_basket()
//                               (economy.population_demand in Lua - the SHARED
//                               `demand_basket` tranche plus every `baskets` row
//                               the registry's band admits, i.e. the exact vector
//                               inject_population_demand scales by centre scale
//                               and elasticity. BL-640: banded, so this probe's
//                               answer depends on the band the registry carries)
//   background_demand           reg.background_demand_basket()
//                               (economy.background_demand; likewise for
//                               inject_background_demand. All six of its goods
//                               are banded industrial, so it moves NOTHING in an
//                               ancient registry - which is the point)
//   unit_upkeep_draw            reg.military().upkeep.goods_per_head
//                               (economy.military.unit_upkeep; the per-head,
//                               per-tick vector run_unit_upkeep draws from the
//                               (owner, body) pool)
//   construction_material_draw  reg.economics(t).resource_build_cost, every
//                               per-building override reg.resource_build_cost_for
//                               resolves, and reg.road_econ(tier).resource_build_cost
//                               (the vectors run_economy_step's build pass and
//                               road placement actually consume from market
//                               inventory)
//   launch_draw                 launch_draw_per_convoy() — supply_system's OWN
//                               exported draw vector, the one price_convoy_leg
//                               gates on and commit_convoy debits
//
// THE ONE HONEST COMPROMISE, stated rather than hidden. Four of the five are
// Lua-authored data and are therefore derived in the strongest sense: change the
// number and the registry changes with it, with no C++ edit anywhere. The fifth
// is not — a launch's burn is a C++ constant, and no amount of reading Lua will
// find it. Rather than re-type it here (a second copy, i.e. the defect) or drop
// propellant into the red list (a false alarm against a draw that demonstrably
// runs), BL-648 made the pass EXPORT what it draws: supply_system.hpp's
// `launch_draw_per_convoy()` is now the single definition that both the gate and
// the debit read. The residual risk is narrow and worth naming — deleting the
// draw while leaving the vector standing would leave this registry asserting a
// pass that no longer fires. That is a far smaller surface than a prose string,
// and R4 below pins the property that matters: the probes read live state.
//
// TWO PASSES ARE DELIBERATELY ABSENT, and neither absence is an oversight:
//   * inject_interbody_demand ORIGINATES nothing. It pulls a fraction of a home
//     market's *already-injected* unmet demand onto outposts, so its answer for
//     a good nothing else wants is identically zero. A redistributor cannot be
//     the reason a good is alive.
//   * procurement (BL-350 request_quote/accept_quote) is a resource-AGNOSTIC
//     transfer between two corps' pools, elected per command. Admitting it would
//     substantiate every resource in the roster at once — the exemption table's
//     original sin in a new costume — and nothing consumes what it delivers.
enum class injector
{
    none = 0,                   ///< The claim names no pass this registry knows.
    population_demand,
    background_demand,
    unit_upkeep_draw,
    building_upkeep_draw,       ///< BL-708: run_building_upkeep's per-type, era-banded goods draw.
    construction_material_draw,
    launch_draw,
};

const char* injector_label(injector i)
{
    switch (i)
    {
        case injector::none:                       return "(names no pass)";
        case injector::population_demand:          return "inject_population_demand";
        case injector::background_demand:          return "inject_background_demand";
        case injector::unit_upkeep_draw:           return "run_unit_upkeep goods draw";
        case injector::building_upkeep_draw:       return "run_building_upkeep goods draw";
        case injector::construction_material_draw: return "construction material draw";
        case injector::launch_draw:                return "commit_convoy launch draw";
    }
    return "(unknown injector)";
}

/// Every injector but `none`, in declaration order — the deterministic walk the
/// census and R4 both use. Listing the ENUM is not a second list of resources:
/// what each entry moves is still computed, never authored.
constexpr injector k_injectors[] = {
    injector::population_demand, injector::background_demand,
    injector::unit_upkeep_draw,  injector::construction_material_draw,
    injector::launch_draw,
};

/// Does the pass named by @p i really move resource @p r? Every branch reads the
/// live vector the pass itself reads — see the header comment above.
bool injector_moves(const recipe_registry& reg, injector i, std::size_t r)
{
    switch (i)
    {
        case injector::none:
            return false;
        // BL-640: the ERA-RESOLVED folds, not `.demand_basket` on the params
        // (which is now the shared `any` tranche alone). These are the exact
        // vectors inject_population_demand / inject_background_demand multiply
        // by, so the probe still reads what the pass reads - and it now answers
        // PER BAND, which is what makes R1b's derivation honest.
        case injector::population_demand:
            return reg.population_demand_basket()[r] > 0.0f;
        case injector::background_demand:
            return reg.background_demand_basket()[r] > 0.0f;
        case injector::unit_upkeep_draw:
            return reg.military().upkeep.goods_per_head[r] > 0.0f;
        // BL-708. Resolved through `building_upkeep_goods` — the SAME free
        // function the live pass and the census both compose the bands with —
        // so the probe reads what the pass reads.
        //
        // ACROSS EVERY BAND AND EVERY BUILDING TYPE, deliberately, because that
        // is the question R1 asks: "does a real pass draw this good ANYWHERE",
        // not "does it draw it in the band this probe registry happens to
        // carry". The two basket cases above read the same way — `ceramics` is
        // authored only in the ancient household tranche and substantiates here
        // regardless of the probe's band — so scanning one band would make this
        // row stricter than its neighbours for no reason. Whether producer and
        // consumer meet IN THE SAME band is R1b's question, and R1b asks it
        // separately with a banded registry.
        //
        // The contract this keeps: zero the Lua rate and the row goes red by
        // name. Narrow the rate to a band and it stays green here, and R1b is
        // what would speak if that band could not also make the good.
        case injector::building_upkeep_draw:
        {
            for (std::size_t b = 0; b < era_band_count; ++b)
                for (int t = 0; t < static_cast<int>(building_type_count); ++t)
                {
                    const auto basket = building_upkeep_goods(
                        reg.building_upkeep(), static_cast<building_type>(t),
                        static_cast<era_band>(b));
                    if (basket[r] > 0.0f)
                        return true;
                }
            return false;
        }
        case injector::launch_draw:
            return launch_draw_per_convoy()[r] > 0.0f;
        case injector::construction_material_draw:
        {
            // The base cost of every building type, then every per-building
            // override (BL-590) `resource_build_cost_for` resolves, then the
            // road ladder. Generic over the roster, so a future override is
            // covered without an edit here.
            // BL-640: gated on building_available, so a probe against a registry
            // carrying a BAND sees only that band's buildings. Under the default
            // `any` band every type is available, so R1 and R4's own probes are
            // unchanged; R1b, which sets a band, gets the truth instead.
            // BL-709: the construction SECTOR's own per-project draw. It is a
            // real part of this pass — `run_construction` adds
            // `capacity_per_build_tick` to the same per-tick need row it builds
            // from the material baskets below — but it is not expressible AS a
            // basket row, because it applies to every building under
            // construction whatever its type or recipe. Read from the dial the
            // pass itself reads, so zeroing the dial turns the claim red by name
            // rather than leaving this branch asserting a draw that stopped.
            if (r == static_cast<std::size_t>(resource_type::construction_capacity)
                && reg.construction().capacity_per_build_tick > 0.0f)
                return true;
            for (int t = 1; t <= 9; ++t) // building_type 1..9; `none` (0) has no economics
            {
                const building_type bt = static_cast<building_type>(t);
                if (!reg.building_available(bt))
                    continue;
                if (reg.economics(bt).resource_build_cost[r] > 0.0f)
                    return true;
            }
            for (const resource_type target : placement_rules::k_extractable)
                if (reg.resource_build_cost_for(building_type::extraction_site,
                                                target, no_recipe)[r] > 0.0f)
                    return true;
            const int n = reg.recipe_count(building_type::processing_facility);
            for (int k = 0; k < n; ++k)
            {
                const recipe& rc = reg.recipe_at(building_type::processing_facility, k);
                if (reg.resource_build_cost_for(building_type::processing_facility,
                                                resource_type::iron_ore,
                                                reg.recipe_id(rc.name))[r] > 0.0f)
                    return true;
            }
            for (std::uint8_t tier = 1; tier <= 3; ++tier)
                if (reg.road_econ(tier).resource_build_cost[r] > 0.0f)
                    return true;
            return false;
        }
    }
    return false;
}

/// One row of R1's exemption table: the good, the pass its author CLAIMS wants
/// it, and the prose reason. The claim is authored — an author asserting
/// something is fine; what BL-648 changed is that the assertion is now CHECKED
/// against the registry instead of taken on trust.
struct exemption
{
    resource_type res;
    injector      via;
    const char*   claim;
};

/// THE table. `injector::none` is not a placeholder to be tidied away later — it
/// is the honest record of a claim with no pass behind it, and the row it makes
/// fail is this item succeeding.
const exemption k_actor_consumed[] = {
    // --- Claims that name a real pass ------------------------------------
    { resource_type::propellant, injector::launch_draw,
      "per-convoy dispatch, space mode (BL-308)" },
    { resource_type::clean_water, injector::population_demand,
      "population centres, the BL-368 habitability tranche" },
    { resource_type::consumer_goods, injector::population_demand,
      "population centres, the BL-368 habitability tranche" },
    { resource_type::medical_supplies, injector::population_demand,
      "population centres, the BL-368 habitability tranche" },
    // BL-457/BL-454. Named rather than assumed, per this table's own rule:
    // ordnance is drawn per-tick, per unit, by the upkeep pass. If BL-454 is
    // ever reverted this row does not become a lie in silence — the rate it
    // resolves through goes to zero and R1 says so by name. R4 pins exactly
    // that behaviour rather than trusting this comment.
    { resource_type::ordnance, injector::unit_upkeep_draw,
      "BL-454 unit upkeep draw (per-tick, per unit)" },

    // BL-708. Power is produced by the two generation recipes and consumed by
    // NO recipe, so it reads as terminal — but it is not an orphan, and this row
    // says which pass buys it rather than leaving that to a comment. The claim
    // resolves through `building_upkeep_goods`, so zeroing the Lua rate turns
    // this row red by name rather than letting it quietly become a lie — the
    // same contract ordnance's row above carries. The rate is authored on the
    // industrial band alone (there is no ancient power analogue by design);
    // R1b is the check that asks whether producer and consumer meet in one
    // band, and it does so with a banded registry rather than this one.
    { resource_type::power, injector::building_upkeep_draw,
      "BL-708 building upkeep draw (per-tick, per building; industrial band only)" },

    // BL-709. Construction capacity is produced by the five era-banded
    // construction methods and consumed by NO recipe, so it reads terminal here
    // exactly as power does — and, exactly as power does, it names the pass that
    // buys it rather than leaving that to a comment. TWO passes buy it, and the
    // pass named is the one that actually fires: `run_construction`'s per-project
    // draw. A per-building MAINTENANCE draw was authored first and measured — it
    // collapsed the ancient band's operating firms 198 of 328 -> 33 of 317 on a
    // cold start no rate could soften — so `economy.building_upkeep.goods` ships
    // that rate at zero and would leave this row a lie. Resolving through
    // `economy.construction.capacity_per_build_tick` means zeroing THAT dial
    // turns this row red BY NAME, the same contract ordnance's and power's rows
    // carry.
    { resource_type::construction_capacity, injector::construction_material_draw,
      "BL-709 run_construction's per-project draw (economy.construction."
      "capacity_per_build_tick) — every site under construction, both bands" },

    // BL-640, and the three rows this item exists to move. They sat in the
    // no-pass half below claiming a "mercantile demand" that never existed; the
    // era-banded household basket is a real weight in economy.population_demand's
    // `ancient` tranche, which is what `injector_moves` resolves through. No
    // harness edit substantiates them - deleting the Lua row un-substantiates
    // them again, exactly as R4 pins for clean_water and ordnance.
    { resource_type::ceramics, injector::population_demand,
      "the ancient household basket (BL-640) - terminal artisan good (BL-586)" },
    { resource_type::leather, injector::population_demand,
      "the ancient household basket (BL-640) - terminal artisan good (BL-586 slice 2)" },
    { resource_type::dressed_stone, injector::population_demand,
      "the ancient household basket (BL-640) - the household's share of building, "
      "distinct from BL-642's construction draw, which owns the other share" },

    // --- Claims with no pass behind them ---------------------------------
    // Everything below names a want that was never built. Each stays here,
    // with its claim intact, because a named list is what makes the failure
    // actionable — moving a good onto a fake recipe consumer to quiet the row
    // would destroy the only record of what is owed. MARKETS.md § Demand
    // channels carries the owning item for each channel.
    { resource_type::spacecraft_components, injector::none,
      "BL-350 procurement contracts (terminal object) - but procurement is a "
      "resource-agnostic transfer between two corps' pools, and nothing consumes "
      "what it delivers; the Space-programme budget line (BL-644) owns the first real buyer" },
    { resource_type::tobacco, injector::none,
      "mercantile demand, endemic good (BL-191) - endemic luxury demand (BL-647) owns the buyer" },
    { resource_type::spices, injector::none,
      "mercantile demand, endemic good (BL-191) - endemic luxury demand (BL-647) owns the buyer" },
    { resource_type::coffee, injector::none,
      "mercantile demand, endemic good (BL-191) - endemic luxury demand (BL-647) owns the buyer" },
    { resource_type::furs, injector::none,
      "mercantile demand, endemic good (BL-191) - endemic luxury demand (BL-647) owns the buyer" },
    { resource_type::trade_goods_misc, injector::none,
      "mercantile demand, endemic-luxury placeholder - endemic luxury demand (BL-647) owns the buyer" },
    { resource_type::tools, injector::none,
      "mercantile demand for now; a construction-material draw (BL-590) when it lands" },
    { resource_type::rigging, injector::none,
      "mercantile demand, terminal trade good (BL-586 slice 2, the Shipwright's output). "
      "ITS CLAIM NAMED BL-640 AND BL-640 DID NOT BUY IT: POPULATION.md's ancient household "
      "is ceramics, cloth, leather and dressed stone - rigging is ship's tackle, not a "
      "household good. Re-pointed at no pass rather than at a guess, and left red" },
};

const exemption* exemption_for(std::size_t r)
{
    for (const exemption& e : k_actor_consumed)
        if (static_cast<std::size_t>(e.res) == r)
            return &e;
    return nullptr;
}

/// The whole admission test for a non-recipe consumer, in one place so R1 and
/// R4 cannot answer it differently.
bool exemption_substantiated(const recipe_registry& reg, std::size_t r)
{
    const exemption* e = exemption_for(r);
    return e != nullptr && injector_moves(reg, e->via, r);
}

/// Name the good. `resource_names::name_of` reverses the ONE canonical
/// name<->enum table (BL-414), so this is not the fourth hand-rolled copy the
/// old comment here rightly refused to add.
std::string res_name(std::size_t r)
{
    return resource_names::name_of(static_cast<resource_type>(r));
}

} // namespace

int main()
{
    std::printf("=== chain depth — the growth spine (BL-428) ===\n\n");

    // --- D1/D2: a linear chain, and max-within-a-recipe -----------------------
    std::printf("D1/D2 — depth over a hand-built chain\n");
    {
        recipe_registry reg;
        // RAW_A + RAW_B -> MID   (both raws, so depth 1)
        // MID  + RAW_A -> DEEP   (max(1, 0) + 1 = 2, NOT 1 + 0 + 1)
        reg.add_recipe(make("mid",  {{RAW_A, 1.0f}, {RAW_B, 1.0f}}, {{MID, 1.0f}}));
        reg.add_recipe(make("deep", {{MID, 1.0f}, {RAW_A, 1.0f}},   {{DEEP, 1.0f}}));

        check(reg.depth_of(RAW_A) == 0 && reg.depth_of(RAW_B) == 0,
              "a good no recipe produces is a raw at depth 0");
        check(reg.is_raw(RAW_A) && !reg.is_raw(MID),
              "is_raw agrees with depth 0");
        check(reg.depth_of(MID) == 1, "a good made from raws sits at depth 1");
        check(reg.depth_of(DEEP) == 2,
              "MAX-within-a-recipe: mixing a depth-1 input with a raw gives depth 2, not 3");
        check(reg.max_depth() == 2, "max_depth reports the graph's ceiling");
    }

    // --- D3: min-across-recipes ----------------------------------------------
    std::printf("\nD3 — a shallower alternate route lowers the depth\n");
    {
        recipe_registry reg;
        reg.add_recipe(make("mid",       {{RAW_A, 1.0f}},              {{MID, 1.0f}}));   // depth 1
        reg.add_recipe(make("far_long",  {{MID, 1.0f}},                {{FAR, 1.0f}}));   // via MID: 2
        check(reg.depth_of(FAR) == 2, "the only route puts FAR at depth 2");

        // A direct route from a raw. BL-430's alternates are exactly this shape.
        reg.add_recipe(make("far_short", {{RAW_B, 1.0f}},              {{FAR, 1.0f}}));
        check(reg.depth_of(FAR) == 1,
              "MIN-across-recipes: adding a direct-from-raw route drops FAR to depth 1");
        check(reg.depth_of(MID) == 1, "...and does not disturb the other goods");
    }

    // --- D4: cycles and orphans are unreachable, not hangs --------------------
    std::printf("\nD4 — cycles and input orphans are unreachable\n");
    {
        recipe_registry reg;
        // A pure cycle: MID needs DEEP, DEEP needs MID. Neither bottoms out.
        reg.add_recipe(make("mid_from_deep", {{DEEP, 1.0f}}, {{MID, 1.0f}}));
        reg.add_recipe(make("deep_from_mid", {{MID, 1.0f}},  {{DEEP, 1.0f}}));
        check(reg.depth_of(MID) == -1 && reg.depth_of(DEEP) == -1,
              "a 2-cycle leaves both goods unreachable rather than looping or fabricating a depth");

        // The cycle must not poison the rest of the graph.
        reg.add_recipe(make("far", {{RAW_A, 1.0f}}, {{FAR, 1.0f}}));
        check(reg.depth_of(FAR) == 1, "an unrelated good is unaffected by the cycle");
        check(reg.depth_of(MID) == -1, "...and the cycle is still unreachable");

        // An escape hatch INTO the cycle resolves it — the honest outcome, since
        // the good really is obtainable once a route bottoms out.
        reg.add_recipe(make("mid_from_raw", {{RAW_B, 1.0f}}, {{MID, 1.0f}}));
        check(reg.depth_of(MID) == 1 && reg.depth_of(DEEP) == 2,
              "adding a raw route into the cycle makes both goods reachable");
    }
    {
        recipe_registry reg;
        // Input orphan: FAR needs DEEP, and nothing produces DEEP.
        reg.add_recipe(make("far_from_deep", {{DEEP, 1.0f}}, {{FAR, 1.0f}}));
        // DEEP is produced by nothing, so it reads as a RAW (depth 0) — which is
        // the truth about the RECIPE graph. Whether it can actually be extracted
        // is a deposit question and belongs to BL-432's roster audit, not here.
        check(reg.depth_of(DEEP) == 0,
              "a good nothing produces reads as a raw — the recipe graph's honest answer");
        check(reg.depth_of(FAR) == 1, "...so what depends on it is depth 1");
    }

    // --- D5: determinism -------------------------------------------------------
    std::printf("\nD5 — the depth vector is order-independent and reproducible\n");
    {
        recipe_registry a, b;
        a.add_recipe(make("mid",  {{RAW_A, 1.0f}},            {{MID, 1.0f}}));
        a.add_recipe(make("deep", {{MID, 1.0f}, {RAW_B, 1.0f}}, {{DEEP, 1.0f}}));
        // Same graph, recipes added in the opposite order.
        b.add_recipe(make("deep", {{MID, 1.0f}, {RAW_B, 1.0f}}, {{DEEP, 1.0f}}));
        b.add_recipe(make("mid",  {{RAW_A, 1.0f}},            {{MID, 1.0f}}));

        bool same = true;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (a.depth_of(static_cast<resource_type>(r)) != b.depth_of(static_cast<resource_type>(r)))
                same = false;
        check(same, "insertion order does not change any good's depth");
        check(a.depth_of(DEEP) == 2 && b.depth_of(DEEP) == 2,
              "...and the shared answer is the correct one");
    }

    // --- D6: the real authored economy ----------------------------------------
    std::printf("\nD6 — against the shipped recipes\n");
    {
        lua_state lua;
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        recipe_registry reg;
        reg.load_from_lua(lua);

        // Every good some allowed recipe PRODUCES must have a depth. An
        // unreachable one would mean the shipped graph has a cycle or an orphaned
        // input — worth failing the gate over.
        std::vector<std::string> unreachable;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            bool produced = false;
            for (int i = 0; i < reg.recipe_count(building_type::processing_facility); ++i)
                if (reg.recipe_at(building_type::processing_facility, i)
                        .outputs[r] > 0.0f)
                    produced = true;
            if (produced && reg.depth_of(static_cast<resource_type>(r)) < 0)
                unreachable.push_back(std::to_string(r));
        }
        for (const std::string& r : unreachable)
            std::printf("      unreachable produced good: resource id %s\n", r.c_str());
        check(unreachable.empty(),
              "every good the shipped recipes produce has a well-defined depth");

        const int industrial_max = (reg.set_era(era_band::industrial), reg.max_depth());
        const int ancient_max    = (reg.set_era(era_band::ancient),    reg.max_depth());
        std::printf("      max depth: industrial %d, ancient %d\n", industrial_max, ancient_max);
        check(industrial_max >= 2,
              "the industrial graph is genuinely layered (anti-vacuity: not all depth 1)");
        check(ancient_max <= industrial_max,
              "masking routes out (BL-433) never ADDS depth");
    }

    // --- G1-G4: the gate ------------------------------------------------------
    std::printf("\nG1 - required depth is the deepest input's depth\n");
    {
        recipe_registry reg;
        reg.add_recipe(make("mid",  {{RAW_A, 1.0f}},              {{MID, 1.0f}}));
        reg.add_recipe(make("deep", {{MID, 1.0f}, {RAW_B, 1.0f}}, {{DEEP, 1.0f}}));

        const std::uint16_t mid  = reg.recipe_id("mid");
        const std::uint16_t deep = reg.recipe_id("deep");
        check(reg.recipe_required_depth(mid) == 0,
              "a recipe drawing only on raws requires depth 0 (a fresh corp can run it)");
        // MAX within the recipe: RAW_B sits at 0, MID at 1, so the pair costs 1.
        check(reg.recipe_required_depth(deep) == 1,
              "a recipe mixing a raw and a depth-1 good requires 1, not 0 and not 2");
        check(reg.recipe_required_depth(no_recipe) == -1,
              "no_recipe requires -1 rather than accidentally reading as depth 0");
    }

    // --- G2 and G3: RETIRED as VACUOUS by BL-692 (2026-08-29) ----------------
    //
    // RETIRED, NOT WEAKENED, and the distinction is the whole point. Neither row
    // was failing and neither was relaxed to make it pass: the property each one
    // asserted stopped existing when the gate did.
    //
    //   G2 asserted MONOTONICITY of `corp_reached_depth` — that losing a shallower
    //      good never lowers the reached number. Its own comment named the reason
    //      that mattered: "the property the gate rests on, since a placement that
    //      was legal must not become illegal". With no gate, no placement is
    //      legal-or-illegal by depth, so monotonicity guards nothing. The row would
    //      still have gone green, which is exactly why leaving it would have been
    //      worse than deleting it — a green row over a dead property is a claim
    //      that something is protected when nothing is.
    //
    //   G3 asserted that every ancient recipe is REACHABLE from a fresh corp, by
    //      simulating the climb the gate forced. It was a pure gate assertion: with
    //      the gate gone every ancient recipe is placeable at tick 0 by
    //      construction, so the row could not fail for any authoring mistake. It
    //      became a tautology, not a test.
    //
    // What the roster still needs from those two — "no ancient recipe is stranded"
    // — is now G5's job and G5 alone, which asserts the tick-0 opening EXACTLY
    // rather than as a reachability floor. The metric G2 exercised
    // (`corp_reached_depth`) is still live code and still covered: G1 and G4 pin
    // required-depth, D1-D6 pin `depth_of`, and R2 uses depth as a dominance axis.

    std::printf("\nG4 - required depth is deterministic\n");
    {
        lua_state lua;
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        recipe_registry a, b;
        a.load_from_lua(lua);
        b.load_from_lua(lua);

        bool same = true;
        for (int i = 0; i < a.recipe_count(building_type::processing_facility); ++i)
        {
            const std::uint16_t id = a.recipe_id(
                a.recipe_at(building_type::processing_facility, i).name);
            if (a.recipe_required_depth(id) != b.recipe_required_depth(id))
                same = false;
        }
        check(same, "two loads of the shipped recipes agree on every required depth");

        recipe_registry x, y;
        x.add_recipe(make("mid",  {{RAW_A, 1.0f}},              {{MID, 1.0f}}));
        x.add_recipe(make("deep", {{MID, 1.0f}, {RAW_B, 1.0f}}, {{DEEP, 1.0f}}));
        y.add_recipe(make("deep", {{MID, 1.0f}, {RAW_B, 1.0f}}, {{DEEP, 1.0f}}));
        y.add_recipe(make("mid",  {{RAW_A, 1.0f}},              {{MID, 1.0f}}));
        check(x.recipe_required_depth(x.recipe_id("deep"))
                  == y.recipe_required_depth(y.recipe_id("deep")),
              "insertion order does not change a recipe's required depth");
    }

    // --- G5: the start gate — a STATED opening, ruled by Ben (BL-589) ---------
    //
    // Measured before this row existed: five processing groups were open to a
    // fresh ancient corp (Metal Foundry, Fuel Production, Food Processing,
    // Artisan Goods, Construction Materials), one of them — Metal Foundry —
    // open ONLY through `refined_copper`, an `any`-band recipe with no ancient
    // identity at all (required depth 0, so the ladder never touched it).
    //
    // Ben's ruling (2026-08-24, the start-gate elicitation form): gate
    // refined_copper by tech specifically (E0-EC-03); leave every other open
    // recipe as-is — Food Processing keeps BOTH Food Rations and Miller, Fuel
    // Production keeps BOTH Charcoal Burner and Peat Kiln (a genuine supply-
    // route pair, R2's own classification), Artisan Goods and Construction
    // Materials stay fully open, and the any-band depth exemption itself is
    // NOT narrowed. So this row asserts the RULED opening exactly — not a
    // narrower one this session might have preferred — and that the one newly
    // locked recipe is not a permanent orphan (its gate actually resolves).
    std::printf("\nG5 - the start gate: the ruled opening, exactly, and nothing stranded\n");
    {
        lua_state lua;
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        recipe_registry reg;
        reg.load_from_lua(lua);
        reg.set_era(era_band::ancient);

        world w;
        const entity_id corp = w.create_entity();
        w.corporations[corp] = corporation_component{}; // fresh: depth 0, no tech, no balance

        // The ruled opening, by recipe NAME (not display_name — the Smithy's two
        // recipes share a display_name and must not collide here).
        // RE-BLESSED 2026-08-29 (BL-692): 11 -> 16. Retiring the chain-depth gate
        // opened every ancient recipe that was shut on depth alone. The five that
        // moved, all previously depth>0:
        //     iron_blooms          (Bloomery)      — the Metal Foundry entry rung
        //     steel_from_blooms    (Smithy)        — downstream of blooms
        //     ordnance_from_blooms (Smithy)        — downstream of blooms
        //     charcoal_from_kiln   (Coking Kiln)   — the deeper fuel route
        //     shipwright           (Advanced Fab.) — inputs planks + cloth
        //
        // SIXTEEN, NOT SEVENTEEN, and the missing one is the interesting row.
        // `toolmaker` is ancient-band and was shut by depth, so a count taken off
        // required-depth alone predicts it opening here. It does not: E0-EC-01
        // "Tool-and-Die Practice" gates it on tech, and this fixture's corp is
        // fresh — no buildings, no balance — so that predicate fails. It was
        // closed by BOTH locks and only one was removed. That is the design
        // working, not a shortfall: tech is now the only lock, and it still holds.
        static const char* const k_open[] = {
            "charcoal", "peat_charcoal", "charcoal_from_kiln",       // Fuel Production
            "food_rations", "food_rations_milled",                    // Food Processing
            "trade_goods", "glass", "tannery", "weaver",               // Artisan Goods
            "ceramics_kiln", "stonemason", "sawmill",                  // Construction Materials
            "iron_blooms", "steel_from_blooms", "ordnance_from_blooms",// Metal Foundry
            "shipwright",                                              // Advanced Fabrication
            // BL-709 (2026-08-31) — the two ANCIENT construction methods. Both
            // open at tick 0 and must: capacity is not cargo, so a region with
            // no yard cannot import its way to one, and a start that cannot
            // build a yard cannot build. Neither carries a tech lock and both
            // draw goods the ancient band already makes, so nothing else could
            // close them. 16 -> 18.
            "timber_frame_construction", "stone_and_brick_construction",
            // "toolmaker" is DELIBERATELY NOT here — see the note above: it is
            // the one ancient recipe still closed at tick 0, and by TECH now.
        };
        auto expected_open = [&](const std::string& name) {
            for (const char* n : k_open)
                if (name == n)
                    return true;
            return false;
        };

        std::vector<std::string> mismatches;
        bool saw_refined_copper_locked = false;
        const int n = reg.recipe_count(building_type::processing_facility);
        for (int i = 0; i < n; ++i)
        {
            const recipe&  rc  = reg.recipe_at(building_type::processing_facility, i);
            const uint16_t rid = reg.recipe_id(rc.name);
            // BL-692: the `depth_ok` term that stood beside this one is gone.
            // Tech is the only thing that can close a recipe at tick 0 now, so
            // `placeable` asks exactly what construct_building asks.
            const bool     placeable = recipe_unlocked(w, reg, corp, rid);

            if (rc.name == "refined_copper")
            {
                saw_refined_copper_locked = !placeable;
                continue; // asserted separately below — it is the one deliberate closure
            }
            const bool should_be_open = expected_open(rc.name);
            if (placeable != should_be_open)
                mismatches.push_back(rc.name + (placeable ? " is open, ruled locked"
                                                          : " is locked, ruled open"));
        }
        for (const std::string& m : mismatches)
            std::printf("      MISMATCH: %s\n", m.c_str());
        check(mismatches.empty(),
              "every recipe outside the one ruled lock matches the ruled opening exactly");
        // RE-SPECIFIED 2026-08-26 (NR-675). The property this protects is "the one
        // deliberate closure HOLDS at tick 0" — it was never "the closure is spelled
        // tech_lock". Ben ruled refined_copper's missing era tag an oversight and
        // tagged it `industrial`, so the ancient band no longer LISTS the recipe at
        // all and the loop above cannot see it to find it locked. Same closure,
        // earlier mechanism. Asserting either one alone would now be asserting the
        // spelling rather than the property, so this takes both and REPORTS which it
        // observed — equal strength, and it survives the closure being re-spelled
        // again. If refined_copper ever becomes openable at tick 0 by any route, this
        // row fails, which is what it is for.
        const bool copper_in_band =
            [&]
        {
            const int m = reg.recipe_count(building_type::processing_facility);
            for (int i = 0; i < m; ++i)
                if (reg.recipe_at(building_type::processing_facility, i).name == "refined_copper")
                    return true;
            return false;
        }();
        std::printf("      refined_copper closure at tick 0: %s\n",
                    !copper_in_band ? "ABSENT from the ancient band (era tag, NR-675)"
                                    : (saw_refined_copper_locked ? "present but TECH-LOCKED (E0-EC-03)"
                                                                 : "OPEN — the closure has been lost"));
        check(!copper_in_band || saw_refined_copper_locked,
              "refined_copper is closed at tick 0 — by era band or by tech lock, the one deliberate closure");

        // Not a permanent orphan: build a corp whose state satisfies E0-EC-03's
        // own authored predicate (one processing facility, Cr 400+ surplus) and
        // confirm advance_tech_gates actually earns it and recipe_unlocked
        // flips — proving the closure is a gate, not a silent dead end.
        const entity_id tile = w.create_entity();
        const entity_id bld  = w.create_entity();
        building_component bc{};
        bc.tile = tile;
        bc.type = building_type::processing_facility;
        w.buildings[bld] = bc;
        w.corporations[corp].assets.push_back(bld);
        w.corporations[corp].balance = 500.0f;
        const int earned = advance_tech_gates(w);
        check(earned >= 1 && w.has_tech(corp, "E0-EC-03"),
              "E0-EC-03 resolves once its own authored predicate is met");
        check(recipe_unlocked(w, reg, corp, reg.recipe_id("refined_copper")),
              "...and refined_copper is placeable immediately after");
    }

    // --- R1: no orphan resources, either direction ----------------------------
    //
    // BL-432 assertion 2. Every resource_type must be OBTAINABLE (produced by a
    // recipe or extractable from a deposit) and must be WANTED (consumed by a
    // recipe, or by a named non-recipe actor from the table below).
    //
    // BL-648 SHARPENED THE SECOND HALF. "Wanted" no longer means "an exemption
    // row exists" — the row's named consumer must resolve to a pass in the
    // injector registry above that really moves the good. See that registry's
    // comment for what went wrong and where each probe reads from.
    //
    // Resource names ARE printed now, which is also BL-648's doing: a list of
    // enum indices is not an actionable failure. `resource_names::name_of`
    // reverses BL-414's single canonical table rather than adding a copy of it.
    std::printf("\nR1 - no orphan resources, either direction\n");
    {
        lua_state lua;
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        recipe_registry reg;
        reg.load_from_lua(lua);
        // `any` is the UNMASKED roster — both arcs at once. `industrial` is a
        // band like any other and hides every ancient recipe, which would report
        // charcoal, iron_blooms, timber, clay, peat and the rest as orphans when
        // they are only masked. The roster invariant is about the whole authored
        // file, not about one campaign's view of it.
        reg.set_era(era_band::any);

        // The exemption table itself now lives at file scope (`k_actor_consumed`,
        // beside the registry), because R4 has to interrogate the SAME table this
        // row admits on. `cloth` is deliberately absent from it: the Weaver's
        // output feeds the Shipwright, so it is expected to show
        // `consumed[r] == true` on its own merits.

        std::vector<bool> produced(resource_count, false);
        std::vector<bool> consumed(resource_count, false);
        for (int i = 0; i < reg.recipe_count(building_type::processing_facility); ++i)
        {
            const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                if (rc.outputs[r] > 0.0f) produced[r] = true;
                if (rc.inputs[r]  > 0.0f) consumed[r] = true;
            }
        }
        std::vector<bool> extractable(resource_count, false);
        for (const resource_type e : placement_rules::k_extractable)
            extractable[static_cast<std::size_t>(e)] = true;

        // The SECOND obtainability route, and it is not k_extractable. Endemic
        // goods (BL-191) are deposited by tile_generation.cpp's C->D pass off a
        // body's `planetology::endemics`, which ADDS a deposit rather than
        // scaling one — so they never appear in the extractable table. The
        // eligible set is the terrain switch at tile_generation.cpp:1401.
        // Without this, all four read as orphans and the row cries wolf.
        for (const resource_type e : { resource_type::tobacco, resource_type::spices,
                                       resource_type::coffee,  resource_type::furs })
            extractable[static_cast<std::size_t>(e)] = true;

        // THE CENSUS the registry makes possible, printed before the verdict so a
        // green row is never silent about what is holding it up. Ascending
        // resource index inside each injector; injectors in declaration order.
        std::printf("      injector registry - what each pass really moves:\n");
        for (const injector inj : k_injectors)
        {
            std::string moved;
            int         n_moved = 0;
            for (std::size_t r = 0; r < resource_count; ++r)
                if (injector_moves(reg, inj, r))
                {
                    moved += (n_moved++ ? ", " : "") + res_name(r);
                }
            std::printf("        %-28s %2d good%s%s%s\n", injector_label(inj), n_moved,
                        n_moved == 1 ? "" : "s", n_moved ? ": " : "", moved.c_str());
        }

        std::vector<std::size_t> unobtainable, unwanted, unsubstantiated;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (!produced[r] && !extractable[r])
                unobtainable.push_back(r);
            if (consumed[r])
                continue; // a recipe wants it; the exemption machinery is not involved
            if (exemption_for(r) == nullptr)
                unwanted.push_back(r);
            else if (!exemption_substantiated(reg, r))
                unsubstantiated.push_back(r);
        }

        for (std::size_t r : unobtainable)
            std::printf("      UNOBTAINABLE: %s (id %zu) - no recipe produces it and no deposit yields it\n",
                        res_name(r).c_str(), r);
        for (std::size_t r : unwanted)
            std::printf("      UNWANTED: %s (id %zu) - no recipe consumes it and no actor is named for it\n",
                        res_name(r).c_str(), r);
        // The actionable half (BL-648). Each line is one good and the exact claim
        // that could not be substantiated — the list Sprint 21's demand-channel
        // items exist to shorten, one good at a time.
        for (std::size_t r : unsubstantiated)
        {
            const exemption* e = exemption_for(r);
            std::printf("      UNSUBSTANTIATED: %s (id %zu)\n"
                        "          claims: \"%s\"\n"
                        "          resolves to: %s - which injects nothing for this good\n",
                        res_name(r).c_str(), r, e->claim, injector_label(e->via));
        }
        std::printf("      %zu resources: %zu unobtainable, %zu unwanted, %zu unsubstantiated, "
                    "%zu actor-consumed exemptions\n",
                    resource_count, unobtainable.size(), unwanted.size(), unsubstantiated.size(),
                    sizeof(k_actor_consumed) / sizeof(k_actor_consumed[0]));

        check(unobtainable.empty(),
              "every resource is obtainable - produced by a recipe or extractable from a deposit");
        check(unwanted.empty(),
              "every resource is wanted - consumed by a recipe, or by a named actor on the exemption list");
        // BL-648. EXPECTED RED, and expected to STAY red until the demand
        // channels MARKETS.md registers are built: a known-red guard carrying a
        // named list is worth more than a green one that means nothing. It must
        // never be quieted by weakening this assertion, nor by moving a good onto
        // a recipe consumer authored only to absorb it.
        check(unsubstantiated.empty(),
              "every actor-consumed exemption names a pass in the injector registry "
              "that really adds demand for the good or draws it from a pool");
    }

    // --- R1b: producer and consumer reachable in the SAME era band ------------
    //
    // BL-460. R1 above only checks that a consumer EXISTS somewhere in the
    // roster; `era = "..."` tags gate PLACEMENT, not existence, so a resource
    // can pass R1 (some recipe or named actor consumes it, some recipe
    // produces it) and still be UNPRODUCIBLE at the one concrete campaign band
    // that actually wants it. This is exactly what happened to `ordnance`:
    // BL-457 gave it an industrial-only Fabricator recipe while BL-454's unit
    // upkeep draws it every tick in EITHER band (units exist in both arcs), so
    // the shipped default campaign (epoch_year = 0, era_band::ancient) consumed
    // a good it could never make. R1's own exemption table is the thing that
    // hid it: a consumer NAMED is not the same as a consumer REACHABLE.
    //
    // The check, per concrete band B in {ancient, industrial}: is the resource
    // WANTED in B (some recipe consumes it under era_permits(B, recipe.era), or
    // a named actor-consumed exemption applies) but NOT REACHABLE in B (no
    // recipe produces it under era_permits(B, recipe.era), and no deposit
    // yields it)? Deposits are band-independent — world generation does not
    // gate extraction by era — so only the RECIPE side of reachability can
    // strand a good.
    //
    // BL-640 REPLACED THIS ROW'S NOTION OF "WANTED". It used to treat every
    // actor-consumed good as wanted in EVERY band, from a hand-narrowed list,
    // on the faithful-at-the-time reading that no consumer was era-gated. Two
    // of them are now: the population and background baskets carry `era` rows
    // and are masked by era_permits exactly as recipes are. So the want is
    // DERIVED per band from BL-648's injector registry instead - see the block
    // below, which also records what that derivation exposed.
    std::printf("\nR1b - producer and consumer reachable in the SAME era band\n");
    {
        lua_state lua;
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        recipe_registry reg;
        reg.load_from_lua(lua);
        reg.set_era(era_band::any); // opening state only; the band loop sets its own

        std::vector<bool> extractable(resource_count, false);
        for (const resource_type e : placement_rules::k_extractable)
            extractable[static_cast<std::size_t>(e)] = true;
        for (const resource_type e : { resource_type::tobacco, resource_type::spices,
                                       resource_type::coffee,  resource_type::furs })
            extractable[static_cast<std::size_t>(e)] = true;

        // BL-640 R4: THE WANT IS DERIVED FROM THE INJECTOR REGISTRY, PER BAND.
        //
        // WHAT THIS REPLACED, and why it could not stay. Until now this row read
        // a HAND-NARROWED list of "band-independent actors" - six resources
        // typed out here, justified in a comment. BL-648 flagged the obvious
        // move (derive it from the registry) and explicitly deferred it, because
        // deriving it against UNBANDED baskets would have added the whole
        // population and background baskets to "wanted in every band" and
        // reported an ancient campaign as genuinely wanting silicon, machinery,
        // alloys and electronics. That report would have been CORRECT - it was
        // the defect, not a false alarm - but it belonged to the item that bands
        // the baskets. This is that item, so the derivation lands here.
        //
        // THE DERIVATION. A resource is actor-wanted in band B if any pass in
        // the injector registry moves it with the registry SET TO B. Nothing is
        // typed out: `injector_moves` resolves every branch through the live
        // vector the pass itself reads, and the two basket branches now resolve
        // through the era-masked fold (recipe_registry::population_demand_basket).
        // So the answer changes when scripts/economy.lua changes, in the right
        // direction, with no edit here.
        //
        // WHAT THE DERIVATION EXPOSES that the hand list hid, stated rather than
        // absorbed:
        //   * The three habitability goods (clean_water, consumer_goods,
        //     medical_supplies) LEAVE the ancient red list, because the basket
        //     that wanted them is now banded industrial. Their k_known_gaps rows
        //     went with them - see below.
        //   * spacecraft_components leaves it too, for a DIFFERENT and less
        //     comfortable reason: no pass in the registry moves it in either
        //     band. Its R1 row stays red and says so; this row simply stops
        //     double-counting a want that does not exist.
        //   * Every pass that is NOT era-gated still reports band-independently,
        //     which is the honest reading of the current code: the unit-upkeep
        //     draw and the launch draw fire in both arcs, so propellant is still
        //     wanted at 0 CE by a dispatch no ancient producer can supply.
        //   * Construction and the two baskets DO vary by band now, so a
        //     material or a household good the band cannot make is a finding
        //     here rather than a silence.
        auto actor_consumed = [](const recipe_registry& r, std::size_t res) {
            for (const injector i : k_injectors)
                if (injector_moves(r, i, res))
                    return true;
            return false;
        };

        // Known, tracked gaps of this SAME defect class, found by this row but
        // out of BL-460's scope (a single-good, difficulty-2 fix) to also
        // repair. Each is a genuine finding — none of the named actors above
        // are era-gated, so an ancient campaign really can starve these goods
        // exactly as it starved ordnance — filed rather than silently fixed or
        // silently ignored (io-standing-rules "named rather than assumed").
        // Remove an entry the moment its own item lands an ancient (or
        // industrial) route.
        struct known_gap { resource_type res; era_band band; const char* tracking; };
        //
        // BL-640 CLOSED FOUR OF THE FIVE, and the reasons differ - recorded
        // rather than quietly deleted, because "the row went green" is only
        // useful with the reason attached:
        //   clean_water / consumer_goods / medical_supplies - REPAIRED. Their
        //     rationale was literally "population demand is not era-gated". It
        //     is now, so the ancient band no longer wants them, and the ancient
        //     band gained ceramics / cloth / leather / dressed_stone instead -
        //     each of which HAS an ancient producer, which is why no new row
        //     replaces these three.
        //   spacecraft_components - NOT repaired, RE-CLASSIFIED. Procurement is
        //     still not era-gated, but procurement is not in the injector
        //     registry (deliberately - see the registry header), so the derived
        //     want no longer names it. It stays red in R1 with an
        //     injector::none claim, which is the row that should carry it.
        // Remove an entry the moment its own item lands an ancient (or
        // industrial) route.
        static const known_gap k_known_gaps[] = {
            { resource_type::propellant, era_band::ancient, "NR-355: the per-convoy launch draw is not era-gated; no ancient producer exists" },
        };
        auto known_gap_tracked = [&](std::size_t r, era_band band) {
            for (const known_gap& g : k_known_gaps)
                if (static_cast<std::size_t>(g.res) == r && g.band == band)
                    return true;
            return false;
        };

        std::vector<std::string> stranded;
        int gap_count = 0;
        for (const era_band band : { era_band::ancient, era_band::industrial })
        {
            const char* band_name = (band == era_band::ancient) ? "ancient" : "industrial";
            // BL-640: the registry CARRIES the band while it is probed, so the
            // injector registry's basket branches answer for this band and not
            // for the union of both. The explicit era_permits filter below is
            // now redundant (recipe_at walks the band's own mask) and is kept as
            // the statement of intent it always was.
            reg.set_era(band);
            const int n = reg.recipe_count(building_type::processing_facility);
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                bool wanted    = actor_consumed(reg, r);
                bool reachable = extractable[r];
                for (int i = 0; i < n && (!wanted || !reachable); ++i)
                {
                    const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
                    if (!era_permits(band, rc.era))
                        continue;
                    if (rc.inputs[r]  > 0.0f) wanted    = true;
                    if (rc.outputs[r] > 0.0f) reachable = true;
                }
                if (wanted && !reachable)
                {
                    if (known_gap_tracked(r, band))
                    {
                        ++gap_count;
                        std::printf("      known gap (tracked): %s (id %zu, %s)\n",
                                    res_name(r).c_str(), r, band_name);
                        continue;
                    }
                    stranded.push_back(res_name(r) + " (id " + std::to_string(r) + ", " + band_name + ")");
                }
            }
        }
        for (const std::string& s : stranded)
            std::printf("      STRANDED: %s - wanted in this band, no producer reaches it there\n", s.c_str());
        std::printf("      %d stranded, %d known-and-tracked gap%s (see k_known_gaps)\n",
                    static_cast<int>(stranded.size()), gap_count, gap_count == 1 ? "" : "s");
        check(stranded.empty(),
              "every resource wanted in a band has a producer reachable in that same band, "
              "or an authored, tracked exemption");
    }

    // --- R2: no dominant production method ------------------------------------
    //
    // BL-432 assertion 4, and NR-243's answer (Ben's call, 2026-08-16: settle the
    // tier-vs-alternate axis FIRST, then retune only what is genuinely dominated).
    //
    // THE AXIS, and why the old grouping was wrong. recipe_switch_harness's R1
    // groups by (primary output, era) and finds four "dominated" pairs. But
    // recipes.lua already states the distinction twice in its own comments (ids 22
    // and 23): distinct raws feeding a shared good is "an ordinary multi-producer
    // economy fact, NOT BL-430's alternate-METHOD feature (one building offering
    // interchangeable recipes for the same output)". Cost dominance is only
    // meaningful between recipes a corp can actually CHOOSE BETWEEN. If their raws
    // are disjoint, which one you run is decided by deposit access, not by price.
    //
    // So every same-output sibling pair must fall into exactly one bucket:
    //   (a) DISJOINT inputs      -> a supply route, not a method. Exempt, counted.
    //   (b) explicitly exempted  -> differs by a placement precondition the recipe
    //                               data does not carry. Named, with a reason.
    //   (c) everything else      -> a genuine interchangeable method. Must not
    //                               dominate on both cost and depth.
    // Bucketing every pair is what keeps this non-vacuous: a pair cannot escape by
    // being unclassifiable, and the counts are printed so a green row is never silent.
    std::printf("\nR2 - no production method dominates an interchangeable sibling\n");
    {
        lua_state lua;
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        recipe_registry reg;
        reg.load_from_lua(lua);
        reg.set_era(era_band::any); // the whole authored roster — see R1's note

        // TWO reference-price snapshots (BL-592), not one. A single fixed vector
        // cannot see a method that is only better "depending on which market it
        // builds to" (Ben, 2026-08-23) — it reads as dominated under the one
        // price regime tested, when a corp sitting on a different deposit mix
        // would genuinely prefer it. `fuel_cheap` and `fuel_dear` bracket the
        // axis BL-587's own methods trade on (timber/peat/coal locally abundant
        // vs scarce); a genuine interchangeable method must win — not be
        // dominated — under AT LEAST ONE of the two. Depth does not vary by
        // price, so only cost is computed twice.
        auto reference_price_cheap = [](resource_type r) -> float {
            switch (r)
            {
                case resource_type::timber: return 0.8f;
                case resource_type::peat:   return 0.6f;
                case resource_type::coal:   return 0.5f;
                case resource_type::clay:   return 1.2f;
                case resource_type::sand:   return 1.0f;
                // BL-709: steel and stone are TABLED because this comparison now
                // has to judge a pair that trades an EXPENSIVE input against
                // CHEAP BULK — the steel frame against reinforced concrete. Under
                // the neutral 1.0 default a unit of steel and a unit of stone
                // cost the same, so the bulk-heavy basket always reads dearer and
                // the check fires on a difference that does not exist: at the
                // roster's own prices (world_gen.lua) the two baskets cost 8.00
                // each, deliberately and to the digit. Flat across both regimes,
                // because the cheap/dear axis of this table is FUEL and neither
                // of these is one.
                case resource_type::steel:  return 8.0f;
                case resource_type::stone:  return 1.0f;
                default:                    return 1.0f; // untabled input: neutral
            }
        };
        auto reference_price_dear = [](resource_type r) -> float {
            switch (r)
            {
                case resource_type::timber: return 3.0f;
                case resource_type::peat:   return 2.5f;
                case resource_type::coal:   return 2.5f;
                case resource_type::clay:   return 1.2f;
                case resource_type::sand:   return 1.0f;
                case resource_type::steel:  return 8.0f; // BL-709, see the cheap table
                case resource_type::stone:  return 1.0f; // BL-709, see the cheap table
                default:                    return 1.0f; // untabled input: neutral
            }
        };

        // Pairs that differ by a PLACEMENT precondition rather than by cost. The
        // recipe carries no atmosphere field, so the distinction cannot be derived
        // and is declared here with its reason.
        auto precondition_exempt = [](const std::string& a, const std::string& b) -> const char* {
            const bool prop = (a == "propellant_atmospheric" && b == "propellant_electrolysis") ||
                              (b == "propellant_atmospheric" && a == "propellant_electrolysis");
            return prop ? "atmosphere vs airless body (BL-308): the airless route is run because the "
                          "cheap one is unavailable, not because it is preferred"
                        : nullptr;
        };

        const int n = reg.recipe_count(building_type::processing_facility);
        std::unordered_map<std::string, std::vector<int>> siblings;
        for (int i = 0; i < n; ++i)
        {
            const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
            const std::string key =
                std::to_string(static_cast<int>(primary_output_resource(rc))) + "/" +
                std::to_string(static_cast<int>(rc.era));
            siblings[key].push_back(i);
        }

        // Deterministic walk: sort the group keys, and each group's members are
        // already in authored order. The BL-406 lesson — never let container
        // iteration order decide an assertion.
        std::vector<std::string> keys;
        keys.reserve(siblings.size());
        for (const auto& [k, v] : siblings)
            if (v.size() >= 2)
                keys.push_back(k);
        std::sort(keys.begin(), keys.end());

        int routes = 0, exempted = 0, methods = 0, dominated = 0;
        for (const std::string& k : keys)
        {
            const std::vector<int>& ids = siblings[k];
            for (std::size_t a = 0; a < ids.size(); ++a)
                for (std::size_t b = a + 1; b < ids.size(); ++b)
                {
                    const recipe& ra = reg.recipe_at(building_type::processing_facility, ids[a]);
                    const recipe& rb = reg.recipe_at(building_type::processing_facility, ids[b]);

                    bool shares_input = false;
                    for (std::size_t r = 0; r < resource_count; ++r)
                        if (ra.inputs[r] > 0.0f && rb.inputs[r] > 0.0f)
                            shares_input = true;

                    if (!shares_input)
                    {
                        ++routes;
                        std::printf("      supply route (disjoint raws): '%s' vs '%s'\n",
                                    ra.name.c_str(), rb.name.c_str());
                        continue;
                    }
                    if (const char* why = precondition_exempt(ra.name, rb.name))
                    {
                        ++exempted;
                        std::printf("      precondition pair: '%s' vs '%s' - %s\n",
                                    ra.name.c_str(), rb.name.c_str(), why);
                        continue;
                    }

                    ++methods;
                    float cost_a_cheap = 0.0f, cost_b_cheap = 0.0f;
                    float cost_a_dear  = 0.0f, cost_b_dear  = 0.0f;
                    int   depth_a = 0,   depth_b = 0;
                    for (std::size_t r = 0; r < resource_count; ++r)
                    {
                        const resource_type rt = static_cast<resource_type>(r);
                        if (ra.inputs[r] > 0.0f)
                        {
                            cost_a_cheap += ra.inputs[r] * reference_price_cheap(rt);
                            cost_a_dear  += ra.inputs[r] * reference_price_dear(rt);
                            depth_a = std::max(depth_a, reg.depth_of(rt));
                        }
                        if (rb.inputs[r] > 0.0f)
                        {
                            cost_b_cheap += rb.inputs[r] * reference_price_cheap(rt);
                            cost_b_dear  += rb.inputs[r] * reference_price_dear(rt);
                            depth_b = std::max(depth_b, reg.depth_of(rt));
                        }
                    }
                    auto dominated_under = [&](float cost_a, float cost_b) {
                        const bool a_dom = cost_a <= cost_b && depth_a <= depth_b &&
                                           (cost_a < cost_b || depth_a < depth_b);
                        const bool b_dom = cost_b <= cost_a && depth_b <= depth_a &&
                                           (cost_b < cost_a || depth_b < depth_a);
                        return a_dom || b_dom;
                    };
                    // Dominated only if BOTH price regimes agree — "must win
                    // under at least one" restated as its negation.
                    if (dominated_under(cost_a_cheap, cost_b_cheap) &&
                        dominated_under(cost_a_dear, cost_b_dear))
                    {
                        ++dominated;
                        std::printf("      DOMINATED METHOD under BOTH price regimes: '%s' "
                                    "(cheap %.1f, dear %.1f, depth %d) vs '%s' (cheap %.1f, dear %.1f, depth %d)\n",
                                    ra.name.c_str(), cost_a_cheap, cost_a_dear, depth_a,
                                    rb.name.c_str(), cost_b_cheap, cost_b_dear, depth_b);
                    }
                }
        }

        std::printf("      %d sibling pair%s: %d supply route%s, %d precondition, %d interchangeable method%s\n",
                    routes + exempted + methods, (routes + exempted + methods) == 1 ? "" : "s",
                    routes, routes == 1 ? "" : "s", exempted, methods, methods == 1 ? "" : "s");

        check(routes + exempted + methods > 0,
              "ANTI-VACUITY: the roster actually contains sibling pairs to classify");
        check(dominated == 0,
              "no interchangeable method dominates a sibling under BOTH price regimes "
              "(fuel-cheap and fuel-dear) and chain depth");
    }

    // --- R3: every named building's material basket is obtainable in its own
    // band (BL-590/BL-592) --------------------------------------------------
    //
    // A per-building override (BL-590) could name a good only the industrial
    // arc makes for an ancient building — unbuildable at 0 CE, and nothing
    // else in this suite would catch it: R1/R1b ask whether a good is
    // obtainable SOMEWHERE, not whether it is obtainable in the SAME band as
    // the specific building that costs it. Generic over the whole roster
    // (every processing recipe, every extraction target `resource_build_cost_for`
    // is ever called on), not a hand-picked list, so a future override earns
    // this check for free rather than needing to be added to it.
    std::printf("\nR3 - every named building's material basket is obtainable in its own band\n");
    {
        lua_state lua;
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        recipe_registry reg;
        reg.load_from_lua(lua);
        reg.set_era(era_band::any);

        std::vector<bool> extractable(resource_count, false);
        for (const resource_type e : placement_rules::k_extractable)
            extractable[static_cast<std::size_t>(e)] = true;
        for (const resource_type e : { resource_type::tobacco, resource_type::spices,
                                       resource_type::coffee,  resource_type::furs })
            extractable[static_cast<std::size_t>(e)] = true;

        // Obtainable in @p band: extractable (deposits are band-independent —
        // world generation does not gate extraction by era, R1b's own note),
        // OR produced by a recipe era_permits(band, ...) allows.
        auto obtainable_in_band = [&](std::size_t r, era_band band) {
            if (extractable[r])
                return true;
            const int n = reg.recipe_count(building_type::processing_facility);
            for (int i = 0; i < n; ++i)
            {
                const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
                if (era_permits(band, rc.era) && rc.outputs[r] > 0.0f)
                    return true;
            }
            return false;
        };

        std::vector<std::string> offenders;
        int checked = 0;

        // Extraction overrides: every extractable target, checked against the
        // ancient band (the band this item's own overrides target).
        for (const resource_type target : placement_rules::k_extractable)
        {
            const auto& def = reg.economics(building_type::extraction_site).resource_build_cost;
            const auto& cost = reg.resource_build_cost_for(building_type::extraction_site,
                                                            target, no_recipe);
            if (cost == def)
                continue; // no override authored for this target
            ++checked;
            for (std::size_t r = 0; r < resource_count; ++r)
                if (cost[r] > 0.0f && !obtainable_in_band(r, era_band::ancient))
                    offenders.push_back("extraction target '" +
                                        std::to_string(static_cast<int>(target)) +
                                        "' costs resource " + std::to_string(r) +
                                        ", unobtainable in the ancient band");
        }

        // Processing overrides: every ancient recipe, checked against ITS OWN
        // band — the band a per-building override is authored for.
        const int n = reg.recipe_count(building_type::processing_facility);
        for (int i = 0; i < n; ++i)
        {
            const recipe&  rc  = reg.recipe_at(building_type::processing_facility, i);
            const uint16_t rid = reg.recipe_id(rc.name);
            const auto& def  = reg.economics(building_type::processing_facility).resource_build_cost;
            const auto& cost = reg.resource_build_cost_for(building_type::processing_facility,
                                                            resource_type::iron_ore, rid);
            if (cost == def)
                continue; // no override authored for this recipe
            ++checked;
            for (std::size_t r = 0; r < resource_count; ++r)
                if (cost[r] > 0.0f && !obtainable_in_band(r, rc.era))
                    offenders.push_back("recipe '" + rc.name + "' costs resource " +
                                        std::to_string(r) + ", unobtainable in its own band");
        }

        for (const std::string& o : offenders)
            std::printf("      OFFENDER: %s\n", o.c_str());
        std::printf("      %d named building%s carry a material override; %zu offender%s\n",
                    checked, checked == 1 ? "" : "s", offenders.size(), offenders.size() == 1 ? "" : "s");

        check(checked > 0, "ANTI-VACUITY: the roster actually authors per-building overrides");
        check(offenders.empty(),
              "every named building's material basket is obtainable in its own band");
    }

    // --- R4: the injector registry is DERIVED, not a second authored list -----
    //
    // BL-648's own failure mode, tested rather than promised. R1 is only worth
    // more than the prose table it replaced if `injector_moves` genuinely reads
    // the vector the running pass reads; a probe that returned a baked-in answer
    // would look identical from the outside and would be the same loophole with
    // more ceremony. So this row MOVES THE DATA and requires the verdict to move
    // with it — the one property a hand-maintained copy cannot fake.
    //
    // No Lua here on purpose: a default-constructed registry has empty baskets,
    // so what the probes report can only have come from what is set below.
    std::printf("\nR4 - the injector registry is derived from live data, not a second list\n");
    {
        const std::size_t i_clean_water = static_cast<std::size_t>(resource_type::clean_water);
        const std::size_t i_ordnance    = static_cast<std::size_t>(resource_type::ordnance);
        // BL-640 moved ceramics onto injector::population_demand (it now has a
        // real basket weight), so the "a claim naming no pass cannot be
        // laundered" probe below needs a subject that still names none. `rigging`
        // is that subject, and the property under test is unchanged.
        const std::size_t i_rigging     = static_cast<std::size_t>(resource_type::rigging);

        // 1. A registry that was never loaded substantiates NOTHING. If the
        //    probes carried a baked-in list, these would still read as wanted.
        recipe_registry blank;
        check(!exemption_substantiated(blank, i_clean_water)
                  && !exemption_substantiated(blank, i_ordnance),
              "an empty registry substantiates no exemption - the probes read data, not a literal");

        // 2. Author the want, and the SAME exemption row goes green. This is the
        //    population basket's own field, so a Lua edit alone moves the verdict.
        population_demand_params pd{};
        pd.demand_basket[i_clean_water] = 0.35f;
        blank.set_population_demand(pd);
        check(exemption_substantiated(blank, i_clean_water),
              "authoring a population-basket weight substantiates clean_water's exemption with no harness edit");
        check(!exemption_substantiated(blank, i_ordnance),
              "...and substantiates ONLY the good authored, not the whole exemption table");

        // 3. The regression the old table asked for in a comment and could not
        //    enforce: revert BL-454's draw and ordnance's exemption must become
        //    a lie the row reports, not a lie the row keeps green.
        military_capability_params mil{};
        mil.upkeep.goods_per_head[i_ordnance] = 0.0035f;
        blank.set_military(mil);
        check(exemption_substantiated(blank, i_ordnance),
              "a live unit-upkeep rate substantiates ordnance's exemption");
        mil.upkeep.goods_per_head[i_ordnance] = 0.0f; // as if BL-454 were reverted
        blank.set_military(mil);
        check(!exemption_substantiated(blank, i_ordnance),
              "zeroing the upkeep rate UNSUBSTANTIATES it - a reverted draw cannot leave a green row behind");

        // 4. No amount of authoring in one channel launders a claim that names
        //    no pass at all. `rigging` claims a mercantile demand; giving the
        //    population basket a weight for it must not help, because its row
        //    does not name that pass.
        population_demand_params pd2 = blank.population_demand();
        pd2.demand_basket[i_rigging] = 1.0f;
        blank.set_population_demand(pd2);
        check(!exemption_substantiated(blank, i_rigging),
              "an injector::none claim stays unsubstantiated however the data moves - "
              "the row must be repointed at the pass that lands, not merely surrounded by one");

        // 5. ANTI-VACUITY against the SHIPPED registry: a registered pass that
        //    moves nothing is the same defect one level up - a named injector
        //    that does not inject. Every one of the five must carry real goods.
        lua_state lua;
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        recipe_registry reg;
        reg.load_from_lua(lua);
        reg.set_era(era_band::any);

        std::vector<std::string> empty_injectors;
        for (const injector inj : k_injectors)
        {
            int n_moved = 0;
            for (std::size_t r = 0; r < resource_count; ++r)
                if (injector_moves(reg, inj, r))
                    ++n_moved;
            if (n_moved == 0)
                empty_injectors.push_back(injector_label(inj));
        }
        for (const std::string& e : empty_injectors)
            std::printf("      EMPTY INJECTOR: %s moves nothing in the shipped registry\n", e.c_str());
        check(empty_injectors.empty(),
              "ANTI-VACUITY: every pass in the injector registry really moves at least one good");
    }

    std::printf("\n=== %s (%d failure%s) ===\n",
                failures == 0 ? "ALL PASS" : "FAILURES",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
