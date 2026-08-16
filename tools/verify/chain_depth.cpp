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
// WHAT IS ASSERTED HERE (BL-428, then BL-432). The D/G rows cover the METRIC and
// the GATE. The R rows are BL-432's roster invariants, which needed a fuller
// roster to be meaningful. BL-432's third assertion — every building's minimum
// depth reachable — is G3 below, landed with the gate.
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
// THE GATE (BL-428 second half, 2026-08-16). The rows above cover the metric as a
// READOUT. These cover it as a GATE - the half that makes depth the growth track:
//
//   G1  A recipe's required depth is its DEEPEST input's depth, and 0 for an
//       input-free recipe. Derived, so it cannot drift from the graph.
//   G2  Monotonicity: a corp's reached depth is produced-once-ever. Clearing a
//       stockpile, idling, or demolishing never lowers it - the property the
//       gate rests on, since a placement that was legal must not become illegal.
//   G3  No unreachable ancient recipe: every ancient-band recipe's required depth
//       is climbable from a fresh corp by some chain of ancient recipes. An
//       unplaceable building is the roster's orphan (BL-432 assertion 3).
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
//   R2  No dominant production method — but only between recipes a corp can
//       actually choose between. See the axis note at the row itself: disjoint-raw
//       siblings are supply routes, not methods, and comparing them on price is
//       the wrong question. Every sibling pair is bucketed, so none escapes by
//       being unclassifiable.

#include "scripting/lua_state.hpp"
#include "world/components.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"

#include <algorithm>
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

    std::printf("\nG2 - reached depth is produced-once-ever and monotonic\n");
    {
        recipe_registry reg;
        reg.add_recipe(make("mid",  {{RAW_A, 1.0f}},              {{MID, 1.0f}}));
        reg.add_recipe(make("deep", {{MID, 1.0f}, {RAW_B, 1.0f}}, {{DEEP, 1.0f}}));

        corporation_component c;
        check(corp_reached_depth(c, reg) == 0, "a fresh corp has reached depth 0");

        c.produced_ever[static_cast<std::size_t>(MID)] = true;
        const int after_mid = corp_reached_depth(c, reg);
        check(after_mid == 1, "producing a depth-1 good raises reached depth to 1");

        // The monotonicity property, stated as the thing that would break it: the
        // bit is the ONLY input, so no stockpile or building state whose loss
        // could lower the number even exists. Assert the invariant directly.
        c.produced_ever[static_cast<std::size_t>(DEEP)] = true;
        c.produced_ever[static_cast<std::size_t>(MID)]  = false; // as if MID were "forgotten"
        check(corp_reached_depth(c, reg) >= after_mid,
              "losing a shallower good never lowers reached depth below what a deeper one earned");
    }

    std::printf("\nG3 - every ancient recipe is reachable from a fresh corp\n");
    {
        lua_state lua;
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        recipe_registry reg;
        reg.load_from_lua(lua);
        reg.set_era(era_band::ancient);

        // Climb: start at reached 0, admit every ancient recipe the current depth
        // allows, bank what they produce, repeat until nothing new opens. Anything
        // still shut out at the fixed point is unplaceable for the whole campaign.
        corporation_component c;
        for (std::size_t pass = 0; pass < resource_count; ++pass)
        {
            const int reached = corp_reached_depth(c, reg);
            bool      grew    = false;
            for (int i = 0; i < reg.recipe_count(building_type::processing_facility); ++i)
            {
                const recipe&       rc   = reg.recipe_at(building_type::processing_facility, i);
                const std::uint16_t id   = reg.recipe_id(rc.name);
                const int           need = reg.recipe_required_depth(id);
                if (need < 0 || need > reached)
                    continue;
                for (std::size_t r = 0; r < resource_count; ++r)
                    if (rc.outputs[r] > 0.0f && !c.produced_ever[r])
                    {
                        c.produced_ever[r] = true;
                        grew               = true;
                    }
            }
            if (!grew)
                break;
        }

        const int final_reached = corp_reached_depth(c, reg);
        std::vector<std::string> stranded;
        for (int i = 0; i < reg.recipe_count(building_type::processing_facility); ++i)
        {
            const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
            if (rc.era != era_band::ancient)
                continue;
            const int need = reg.recipe_required_depth(reg.recipe_id(rc.name));
            if (need < 0 || need > final_reached)
                stranded.push_back(rc.display_name.empty() ? rc.name : rc.display_name);
        }
        for (const std::string& n : stranded)
            std::printf("      stranded ancient recipe: %s\n", n.c_str());
        std::printf("      ancient ladder climbs to depth %d\n", final_reached);
        check(stranded.empty(),
              "no ancient recipe is permanently unplaceable (the roster's orphan)");
        check(final_reached >= 1,
              "the ancient ladder actually has a rung above raw (anti-vacuity)");
    }

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

    // --- R1: no orphan resources, either direction ----------------------------
    //
    // BL-432 assertion 2. Every resource_type must be OBTAINABLE (produced by a
    // recipe or extractable from a deposit) and must be WANTED (consumed by a
    // recipe, or by a named non-recipe actor from the table below).
    //
    // Resource NAMES are not printed: the only lookup table lives in the UI
    // layer, and BL-414 (resource name triple-desync) owns consolidating it.
    // Adding a fourth hand-rolled copy here to prettify a harness would make
    // that item worse. Ids match components.hpp's enum, same as D6 above.
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

        // The EXPLICIT exemption list BL-432's design asks for — a good that is
        // consumed by a named actor rather than by a recipe. Each entry names
        // the actor, so an orphan cannot hide here as an assumed terminal.
        struct exemption { resource_type res; const char* consumer; };
        static const exemption k_actor_consumed[] = {
            { resource_type::spacecraft_components, "BL-350 procurement contracts (terminal object)" },
            { resource_type::propellant,            "per-convoy dispatch, space mode (BL-308)" },
            { resource_type::clean_water,           "population centres, inject_population_demand" },
            { resource_type::consumer_goods,        "population centres, inject_population_demand" },
            { resource_type::medical_supplies,      "population centres, inject_population_demand" },
            { resource_type::tobacco,               "mercantile demand, endemic good (BL-191)" },
            { resource_type::spices,                "mercantile demand, endemic good (BL-191)" },
            { resource_type::coffee,                "mercantile demand, endemic good (BL-191)" },
            { resource_type::furs,                  "mercantile demand, endemic good (BL-191)" },
            { resource_type::trade_goods_misc,      "mercantile demand, endemic-luxury placeholder" },
        };
        auto exempt_consumer = [&](std::size_t r) -> const char* {
            for (const exemption& e : k_actor_consumed)
                if (static_cast<std::size_t>(e.res) == r)
                    return e.consumer;
            return nullptr;
        };

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

        std::vector<std::size_t> unobtainable, unwanted;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (!produced[r] && !extractable[r])
                unobtainable.push_back(r);
            if (!consumed[r] && exempt_consumer(r) == nullptr)
                unwanted.push_back(r);
        }

        for (std::size_t r : unobtainable)
            std::printf("      UNOBTAINABLE: resource id %zu - no recipe produces it and no deposit yields it\n", r);
        for (std::size_t r : unwanted)
            std::printf("      UNWANTED: resource id %zu - no recipe consumes it and no actor is named for it\n", r);
        std::printf("      %zu resources: %zu unobtainable, %zu unwanted, %zu actor-consumed exemptions\n",
                    resource_count, unobtainable.size(), unwanted.size(),
                    sizeof(k_actor_consumed) / sizeof(k_actor_consumed[0]));

        check(unobtainable.empty(),
              "every resource is obtainable - produced by a recipe or extractable from a deposit");
        check(unwanted.empty(),
              "every resource is wanted - consumed by a recipe, or by a named actor on the exemption list");
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

        // Reference prices: world_gen.lua's base_price for the goods the sibling
        // pairs actually use. A fixed snapshot — this asks about recipe SHAPE, not
        // a moment's market state. Mirrors recipe_switch_harness's table.
        auto reference_price = [](resource_type r) -> float {
            switch (r)
            {
                case resource_type::timber: return 1.5f;
                case resource_type::peat:   return 1.2f;
                case resource_type::clay:   return 1.2f;
                case resource_type::sand:   return 1.0f;
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
                    float cost_a = 0.0f, cost_b = 0.0f;
                    int   depth_a = 0,   depth_b = 0;
                    for (std::size_t r = 0; r < resource_count; ++r)
                    {
                        const resource_type rt = static_cast<resource_type>(r);
                        if (ra.inputs[r] > 0.0f)
                        {
                            cost_a += ra.inputs[r] * reference_price(rt);
                            depth_a = std::max(depth_a, reg.depth_of(rt));
                        }
                        if (rb.inputs[r] > 0.0f)
                        {
                            cost_b += rb.inputs[r] * reference_price(rt);
                            depth_b = std::max(depth_b, reg.depth_of(rt));
                        }
                    }
                    const bool a_dom = cost_a <= cost_b && depth_a <= depth_b &&
                                       (cost_a < cost_b || depth_a < depth_b);
                    const bool b_dom = cost_b <= cost_a && depth_b <= depth_a &&
                                       (cost_b < cost_a || depth_b < depth_a);
                    if (a_dom || b_dom)
                    {
                        ++dominated;
                        std::printf("      DOMINATED METHOD: '%s' (cost %.1f, depth %d) vs '%s' (cost %.1f, depth %d)\n",
                                    ra.name.c_str(), cost_a, depth_a,
                                    rb.name.c_str(), cost_b, depth_b);
                    }
                }
        }

        std::printf("      %d sibling pair%s: %d supply route%s, %d precondition, %d interchangeable method%s\n",
                    routes + exempted + methods, (routes + exempted + methods) == 1 ? "" : "s",
                    routes, routes == 1 ? "" : "s", exempted, methods, methods == 1 ? "" : "s");

        check(routes + exempted + methods > 0,
              "ANTI-VACUITY: the roster actually contains sibling pairs to classify");
        check(dominated == 0,
              "no interchangeable method dominates a sibling on both input cost and chain depth");
    }

    std::printf("\n=== %s (%d failure%s) ===\n",
                failures == 0 ? "ALL PASS" : "FAILURES",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
