// Headless harness for BUILDING STACKS (BL-193, rule settled 2026-07-26; no SDL /
// Lua / ImGui). Several extraction sites may work one tile's deposit, and the
// settled rule has two halves that have to hold together:
//
//   * diminishing per-site output — the k-th site of a stack yields 0.8^(k-1) of a
//     lone site, so two sites make 1.8x and five make 3.36x, never 5x;
//   * honest shared depletion — every site draws REAL reserve for what it takes,
//     and the whole stack tapers against the COMBINED nominal draw, so it drains
//     the deposit faster than its output multiple and exhausts together.
//
// The pair is the point: stacking buys throughput now at the cost of the tile's
// life, so stack-vs-spread is a real trade-off rather than a dominant strategy. A
// check on either half alone would pass while the trade-off was broken, so this
// harness asserts the curve against `placement_rules` AND the realised output and
// exhaustion against `run_economy_step`'s own arithmetic.
//
// Also pins the two things the rule leans on: the richness-derived ceiling
// (max(1, richness/50), a legibility bound now that the curve carries the balance)
// and the determinism of the stored-order walk that decides who is rank 1.
//
// Kept outside src/ so the CMake glob does not pull it into the real build; the
// CTest block in CMakeLists.txt picks it up automatically and links io_world_obj.
//
// Build: cmake --build build_linux --target stack_capacity_harness
// Run:   ./build_linux/stack_capacity_harness

#include "world/building_profit.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_failures = 0;

static void check(bool cond, const char* what, float got = 0.0f, float want = 0.0f)
{
    if (cond) std::printf("  PASS  %s\n", what);
    else { std::printf("  FAIL  %s   (got %.4f, want %.4f)\n", what, got, want); ++g_failures; }
}

static bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }
static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

namespace {

/// The fixture: one body, one iron tile, `sites` extraction sites stacked on it,
/// all owned by a single corp. Deliberately plain — every factor except the stack
/// rank is held at 1 so a realised output IS the stack scalar times a round number.
struct fixture
{
    world                  w;
    entity_id              body   = null_entity;
    entity_id              tile   = null_entity;
    entity_id              corp   = null_entity;
    entity_id              market = null_entity; ///< Only when the fixture asked for one.
    std::vector<entity_id> sites;
};

/// Lone-site nominal for the fixture below: base_rate 20 x richness 2.0 x
/// workforce 0.5 x target 100 % x (1 - hazard 0) = 20.0 per tick.
constexpr float k_lone_nominal = 20.0f;

recipe_registry make_registry()
{
    recipe_registry reg;
    reg.set_thresholds(/*t_full=*/1.0f, /*t_idle=*/0.2f);
    building_economics ex;
    ex.base_rate = 20.0f; ex.maintenance = 5.0f; ex.base_wage = 8.0f; ex.build_cost = 100.0f;
    reg.set_economics(building_type::extraction_site, ex);
    building_economics pr;
    pr.base_rate = 8.0f; pr.maintenance = 10.0f; pr.base_wage = 12.0f; pr.build_cost = 150.0f;
    reg.set_economics(building_type::processing_facility, pr);
    return reg;
}

/// Iron price in the optional market below. The estimator values output at it, and so
/// does every estimate-vs-actual comparison here, so it never has to be a round number
/// for the arithmetic to be legible — it just has to be the SAME number on both sides.
constexpr float k_iron_price = 2.5f;

fixture make_fixture(int sites, float reserve, bool with_market = false)
{
    fixture f;
    f.body = f.w.create_entity();
    f.w.bodies[f.body] = body_component{};
    f.w.bodies[f.body].name = "TestWorld";

    f.tile = f.w.create_entity();
    {
        tile_component tc{};
        tc.body        = f.body;
        tc.composition = terrain_composition::rocky;
        tc.resource_deposit[ri(resource_type::iron_ore)]   = 2.0f;
        tc.resource_remaining[ri(resource_type::iron_ore)] = reserve;
        tc.hazard_level = 0.0f;
        f.w.tiles[f.tile] = tc;
    }

    // Sites are created in order, so their ids ascend: creation order IS stored
    // order is the assumption the rank rule rests on, and the fixture exercises it.
    for (int i = 0; i < sites; ++i)
    {
        const entity_id bid = f.w.create_entity();
        building_component b{};
        b.tile               = f.tile;
        b.type               = building_type::extraction_site;
        b.workforce_assigned = 0.5f;
        b.target_resource    = resource_type::iron_ore;
        // Pin the target: the BL-181 auto-solver would otherwise reprice it against
        // a market this fixture deliberately does not have, and the whole point here
        // is that the only thing moving output is the stack rank.
        b.workforce_auto     = false;
        f.w.buildings[bid]   = b;
        f.sites.push_back(bid);
    }

    f.corp = f.w.create_entity();
    {
        corporation_component cc;
        cc.name             = "Stacker Co";
        cc.starting_capital = 100000.0f;
        cc.balance          = 100000.0f;
        cc.assets           = f.sites;
        // Not AI-driven: BL-202's strategic tier would command this corp at the end
        // of the tick and move the very dials being asserted (see econ_harness).
        cc.is_player        = true;
        f.w.corporations[f.corp] = cc;
    }
    f.w.player_entity = f.corp;

    // Created LAST on purpose: every id above keeps the value it had before the market
    // was an option, so S.R4-S.R6's stored-order assertions are untouched by it.
    // estimate_prospective_profit refuses to answer without a market (a chart of zeros
    // is a lie), so the BL-346 section is the only one that asks for it.
    if (with_market)
    {
        f.market = f.w.create_entity();
        market_component mc;
        mc.body = f.body;
        mc.base_price[ri(resource_type::iron_ore)] = k_iron_price;
        mc.price = mc.base_price;
        f.w.markets[f.market] = mc;
    }
    return f;
}

/// Add one more extraction site to the fixture's tile, owned by the same corp. Created
/// after every existing site, so its entity id is the highest and its rank is last —
/// which is exactly what building one more site on the tile does.
entity_id add_site(fixture& f)
{
    const entity_id bid = f.w.create_entity();
    building_component b{};
    b.tile               = f.tile;
    b.type               = building_type::extraction_site;
    b.workforce_assigned = 0.5f;
    b.target_resource    = resource_type::iron_ore;
    b.workforce_auto     = false;
    f.w.buildings[bid]   = b;
    f.sites.push_back(bid);
    f.w.corporations[f.corp].assets.push_back(bid);
    return bid;
}

/// What `estimate_prospective_profit` says the NEXT extraction site on the fixture's
/// tile would earn per tick.
building_profit estimate_next(const fixture& f, const recipe_registry& reg)
{
    return estimate_prospective_profit(f.w, reg, f.tile, building_type::extraction_site,
                                       resource_type::iron_ore);
}

/// This tick's output for `building`, 0 if it produced nothing.
float output_of(const economy_report& rep, entity_id building)
{
    for (const building_report& r : rep.buildings)
        if (r.building == building)
            return r.output_quantity;
    return 0.0f;
}

bool exhausted_at(const economy_report& rep, entity_id building)
{
    for (const building_report& r : rep.buildings)
        if (r.building == building)
            return r.exhausted;
    return false;
}

} // namespace

int main()
{
    std::printf("BL-193 building-stack harness (diminishing returns, shared depletion)\n");

    const recipe_registry reg = make_registry();

    // ---------------------------------------------------------------------
    // S.R1 — the curve. d = 0.8 per step down the stack, and the cumulative
    // multiples are the numbers the design names: 2 sites = 1.8x, 5 = 3.36x.
    // ---------------------------------------------------------------------
    {
        using placement_rules::stack_output_scalar;
        check(near(stack_output_scalar(1), 1.0f),    "S.R1 site 1 undiminished",       stack_output_scalar(1), 1.0f);
        check(near(stack_output_scalar(2), 0.8f),    "S.R1 site 2 yields 0.8",         stack_output_scalar(2), 0.8f);
        check(near(stack_output_scalar(3), 0.64f),   "S.R1 site 3 yields 0.64",        stack_output_scalar(3), 0.64f);
        check(near(stack_output_scalar(4), 0.512f),  "S.R1 site 4 yields 0.512",       stack_output_scalar(4), 0.512f);
        check(near(stack_output_scalar(5), 0.4096f), "S.R1 site 5 yields 0.4096",      stack_output_scalar(5), 0.4096f);

        float total = 0.0f;
        for (int k = 1; k <= 5; ++k)
        {
            total += stack_output_scalar(k);
            if (k == 2) check(near(total, 1.8f),    "S.R1 2 sites = 1.8x a lone site",  total, 1.8f);
            if (k == 3) check(near(total, 2.44f),   "S.R1 3 sites = 2.44x",             total, 2.44f);
            if (k == 5) check(near(total, 3.3616f), "S.R1 5 sites = 3.36x, never 5x",   total, 3.3616f);
        }
        // A rank the caller could not supply must not silently zero a site's output.
        check(near(stack_output_scalar(0), 1.0f), "S.R1 rank 0 reads as a lone site",
              stack_output_scalar(0), 1.0f);
    }

    // ---------------------------------------------------------------------
    // S.R2 — the ceiling stays richness/50, minimum 1. Deposits generate in the
    // 0-250 band, so this is the 1-5 site range the markers have to stay legible
    // across; it is a legibility bound, not the balance lever.
    // ---------------------------------------------------------------------
    {
        const float bands[] = { 0.0f, 1.0f, 49.9f, 50.0f, 99.0f, 100.0f, 175.0f, 250.0f };
        const int   want[]  = { 1,    1,    1,     1,     1,     2,      3,      5     };
        for (std::size_t i = 0; i < sizeof(want) / sizeof(want[0]); ++i)
        {
            tile_component tc{};
            tc.composition = terrain_composition::rocky;
            tc.resource_deposit[ri(resource_type::iron_ore)] = bands[i];
            const int cap = placement_rules::stack_capacity(
                tc, building_type::extraction_site, resource_type::iron_ore);
            check(cap == want[i], "S.R2 capacity = max(1, richness/50)",
                  static_cast<float>(cap), static_cast<float>(want[i]));
        }
    }

    // ---------------------------------------------------------------------
    // S.R3 — non-extraction types stay capacity 1. Whether processors stack on
    // land / workforce / road tier is a separate question BL-193 defers rather
    // than answers, and richness must not leak into that answer.
    // ---------------------------------------------------------------------
    {
        tile_component tc{};
        tc.composition = terrain_composition::rocky;
        tc.resource_deposit[ri(resource_type::iron_ore)] = 250.0f; // richest band
        const building_type others[] = { building_type::processing_facility,
                                         building_type::port,
                                         building_type::launchpad,
                                         building_type::inland_logistics_hub,
                                         building_type::none };
        for (const building_type t : others)
        {
            const int cap = placement_rules::stack_capacity(tc, t, resource_type::iron_ore);
            check(cap == 1, "S.R3 non-extraction type stays capacity 1 on a rich tile",
                  static_cast<float>(cap), 1.0f);
        }
    }

    // ---------------------------------------------------------------------
    // S.R4 — realised output through run_economy_step, not a restatement of the
    // formula. Rank comes from stored order, so the sites must come out 20 /
    // 16 / 12.8 in creation order, summing to 2.44x the lone-site control.
    // ---------------------------------------------------------------------
    {
        fixture solo = make_fixture(/*sites=*/1, /*reserve=*/1.0e6f);
        const economy_report rsolo = run_economy_step(solo.w, reg);
        const float lone = output_of(rsolo, solo.sites[0]);
        check(near(lone, k_lone_nominal), "S.R4 lone site yields its full nominal",
              lone, k_lone_nominal);

        fixture f = make_fixture(/*sites=*/3, /*reserve=*/1.0e6f);
        for (std::size_t i = 0; i < f.sites.size(); ++i)
            check(placement_rules::stack_rank(f.w, f.sites[i]) == static_cast<int>(i) + 1,
                  "S.R4 rank follows stored order (oldest site first)",
                  static_cast<float>(placement_rules::stack_rank(f.w, f.sites[i])),
                  static_cast<float>(i + 1));

        const economy_report rep = run_economy_step(f.w, reg);
        const float o1 = output_of(rep, f.sites[0]);
        const float o2 = output_of(rep, f.sites[1]);
        const float o3 = output_of(rep, f.sites[2]);
        check(near(o1, lone),          "S.R4 site 1 of a stack is undiminished", o1, lone);
        check(near(o2, lone * 0.8f),   "S.R4 site 2 yields 0.8 of site 1",       o2, lone * 0.8f);
        check(near(o3, lone * 0.64f),  "S.R4 site 3 yields 0.64 of site 1",      o3, lone * 0.64f);
        check(near(o1 + o2 + o3, lone * 2.44f, 1e-2f),
              "S.R4 3-site stack totals 2.44x a lone site", o1 + o2 + o3, lone * 2.44f);

        // Honest depletion: the reserve falls by exactly what was produced. No
        // clamp, no fake cap, no output conjured that the deposit did not pay for.
        const float drawn = 1.0e6f - f.w.tiles[f.tile].resource_remaining[ri(resource_type::iron_ore)];
        // Tolerance is one float ulp at 1e6 (0.0625) — the reserve is a float, not
        // the assertion being loose.
        check(near(drawn, o1 + o2 + o3, 0.1f),
              "S.R4 reserve falls by exactly the stack's output", drawn, o1 + o2 + o3);
    }

    // ---------------------------------------------------------------------
    // S.R5 — shared depletion. The taper is sized against the stack's COMBINED
    // nominal, so every member rides the same curve, the stack drains faster than
    // its output multiple, and the members exhaust on the same tick.
    // ---------------------------------------------------------------------
    {
        // Combined nominal of a 3-site stack = 20 * 2.44 = 48.8; the taper band is
        // 8x that = 390.4. Half a band of reserve puts every member at taper 0.5.
        fixture f = make_fixture(/*sites=*/3, /*reserve=*/195.2f);
        const economy_report rep = run_economy_step(f.w, reg);
        check(near(output_of(rep, f.sites[0]), k_lone_nominal * 1.0f   * 0.5f, 1e-2f),
              "S.R5 site 1 tapered by the STACK's band", output_of(rep, f.sites[0]), 10.0f);
        check(near(output_of(rep, f.sites[1]), k_lone_nominal * 0.8f   * 0.5f, 1e-2f),
              "S.R5 site 2 sees the same taper", output_of(rep, f.sites[1]), 8.0f);
        check(near(output_of(rep, f.sites[2]), k_lone_nominal * 0.64f  * 0.5f, 1e-2f),
              "S.R5 site 3 sees the same taper", output_of(rep, f.sites[2]), 6.4f);

        // Same reserve, one site: its own band is 8 x 20 = 160, so 195.2 is ample
        // and it runs at full rate. The stack is already half-tapered on the reserve
        // the lone site has not started to feel — that IS the trade-off.
        fixture solo = make_fixture(/*sites=*/1, /*reserve=*/195.2f);
        const economy_report rsolo = run_economy_step(solo.w, reg);
        check(near(output_of(rsolo, solo.sites[0]), k_lone_nominal, 1e-2f),
              "S.R5 a lone site on the same reserve is not tapered at all",
              output_of(rsolo, solo.sites[0]), k_lone_nominal);

        // Exhaustion is simultaneous. Run to the end of the deposit and record the
        // first tick each member reports exhausted; they must all be the same tick.
        fixture g = make_fixture(/*sites=*/3, /*reserve=*/2000.0f);
        std::vector<int> first_exhausted(g.sites.size(), -1);
        int active_ticks = 0;
        for (int tick = 0; tick < 200; ++tick)
        {
            const economy_report r = run_economy_step(g.w, reg);
            bool any_active = false;
            for (std::size_t i = 0; i < g.sites.size(); ++i)
            {
                if (exhausted_at(r, g.sites[i]) && first_exhausted[i] < 0)
                    first_exhausted[i] = tick;
                if (output_of(r, g.sites[i]) > 0.0f)
                    any_active = true;
            }
            if (any_active) ++active_ticks;
        }
        check(first_exhausted[0] >= 0, "S.R5 the stack does exhaust within 200 ticks",
              static_cast<float>(first_exhausted[0]), 0.0f);
        check(first_exhausted[0] == first_exhausted[1] &&
              first_exhausted[1] == first_exhausted[2],
              "S.R5 every member of the stack exhausts on the SAME tick",
              static_cast<float>(first_exhausted[2]), static_cast<float>(first_exhausted[0]));

        // And the stack burns the deposit down faster than its output multiple: a
        // lone site on an identical reserve is still working long after.
        fixture h = make_fixture(/*sites=*/1, /*reserve=*/2000.0f);
        int solo_active = 0;
        for (int tick = 0; tick < 200; ++tick)
            if (output_of(run_economy_step(h.w, reg), h.sites[0]) > 0.0f)
                ++solo_active;
        check(active_ticks < solo_active,
              "S.R5 a full stack drains the deposit sooner than a lone site",
              static_cast<float>(active_ticks), static_cast<float>(solo_active));
    }

    // ---------------------------------------------------------------------
    // S.R6 — determinism. Rank is read off an ordered walk, not world::buildings'
    // own (unordered) iteration, so the same inputs must give bit-identical
    // outputs. Two fixtures built independently from the same parameters (not one
    // copied), so world construction is in the comparison too, ticked in lockstep.
    // ---------------------------------------------------------------------
    {
        fixture a = make_fixture(/*sites=*/4, /*reserve=*/5000.0f);
        fixture b = make_fixture(/*sites=*/4, /*reserve=*/5000.0f);
        check(a.sites == b.sites, "S.R6 identical inputs allocate identical entity ids");

        bool identical = true;
        for (int tick = 0; tick < 25 && identical; ++tick)
        {
            const economy_report ra = run_economy_step(a.w, reg);
            const economy_report rb = run_economy_step(b.w, reg);
            for (std::size_t i = 0; i < a.sites.size(); ++i)
                if (output_of(ra, a.sites[i]) != output_of(rb, b.sites[i]))
                    identical = false;
        }
        check(identical, "S.R6 the same state produces bit-identical outputs twice");
        const float res_a = a.w.tiles[a.tile].resource_remaining[ri(resource_type::iron_ore)];
        const float res_b = b.w.tiles[b.tile].resource_remaining[ri(resource_type::iron_ore)];
        check(res_a == res_b,
              "S.R6 the reserve draws down identically over 25 ticks", res_a, res_b);

        // The ordered walk itself: repeated calls cannot reshuffle a stack.
        const std::vector<entity_id> first = placement_rules::stack_members(
            a.w, a.tile, building_type::extraction_site, resource_type::iron_ore);
        bool stable = (first == a.sites);
        for (int i = 0; i < 8; ++i)
            stable = stable && (placement_rules::stack_members(
                a.w, a.tile, building_type::extraction_site,
                resource_type::iron_ore) == first);
        check(stable, "S.R6 stack_members is stored order, stable across calls");
    }

    // ---------------------------------------------------------------------
    // S.R7 — the ESTIMATE and the TICK agree, member by member (BL-346).
    //
    // estimate_prospective_profit is what the construction ledger ranks the player's
    // build options by, so an estimate that ignores BL-193 does not merely misprint a
    // number — it recommends the build. BL-193 shipped without updating it: every
    // candidate was priced at the lone-site rate and tapered against its own nominal,
    // so a fourth site on a half-worked deposit read like a first site on a fresh one.
    //
    // The assertions below are deliberately estimate-against-ACTUAL, never
    // estimate-against-a-constant: each takes the estimate for the next site, builds
    // that site, ticks, and compares against what run_economy_step credited it. Pinned
    // to a constant, the pair could drift together and stay green.
    // ---------------------------------------------------------------------
    {
        // --- the decay half: rank 1, 2, 3 on an ample reserve (taper is 1 throughout,
        //     so what is left in the comparison is the stack curve alone).
        fixture f = make_fixture(/*sites=*/0, /*reserve=*/1.0e6f, /*with_market=*/true);
        float   est_rank1 = 0.0f, est_rank3 = 0.0f;
        for (int rank = 1; rank <= 3; ++rank)
        {
            const building_profit est = estimate_next(f, reg);
            check(est.has_data, "S.R7 the estimator answers for a stacked tile");

            const entity_id        site = add_site(f);
            const economy_report   rep  = run_economy_step(f.w, reg);
            const float            actual = output_of(rep, site) * k_iron_price;
            check(near(est.revenue, actual, 1e-2f),
                  "S.R7 estimated revenue == what the tick credits that very site",
                  est.revenue, actual);

            if (rank == 1) est_rank1 = est.revenue;
            if (rank == 3) est_rank3 = est.revenue;
        }
        // And the curve is genuinely in there: the third site is priced at 0.64 of the
        // first. Before BL-346 both estimates were the lone-site figure.
        check(near(est_rank3, est_rank1 * 0.64f, 1e-2f),
              "S.R7 the rank-3 estimate is 0.64x the rank-1 estimate (decay is priced)",
              est_rank3, est_rank1 * 0.64f);
        check(est_rank3 < est_rank1,
              "S.R7 stacking is never estimated as free", est_rank3, est_rank1);

        // --- the shared-depletion half: sweep the reserve across a 3-stack's taper
        //     band, including below the exhaustion floor. At each level the estimate
        //     for the third site must equal what that site is actually credited, and
        //     must read zero on exactly the reserves where the tick reports exhausted.
        //     That is the remaining-life claim, checked pointwise rather than asserted.
        //
        //     A 3-stack's combined nominal is 20 x 2.44 = 48.8, so its taper band is
        //     8 x 48.8 = 390.4. The levels below straddle it: ample, mid-band, deep,
        //     and under run_extraction's floor.
        const float reserves[] = { 1.0e6f, 390.0f, 195.2f, 40.0f, 4.0f };
        for (const float reserve : reserves)
        {
            fixture g = make_fixture(/*sites=*/2, reserve, /*with_market=*/true);

            const building_profit est  = estimate_next(g, reg);
            const entity_id       site = add_site(g);
            const economy_report  rep  = run_economy_step(g.w, reg);

            const float actual = output_of(rep, site) * k_iron_price;
            check(near(est.revenue, actual, 1e-2f),
                  "S.R7 estimate matches the credited revenue across the shared taper band",
                  est.revenue, actual);

            // The estimator's zero and the tick's `exhausted` are the same event: both
            // are "the shared taper fell under run_extraction's floor".
            const bool est_zero  = (est.revenue <= 0.0f);
            const bool tick_done = exhausted_at(rep, site);
            check(est_zero == tick_done,
                  "S.R7 the estimate reads zero on exactly the reserves the tick exhausts",
                  est_zero ? 1.0f : 0.0f, tick_done ? 1.0f : 0.0f);
        }

        // --- and the regression the fix closes: the pre-BL-346 model priced the
        //     candidate at the lone-site rate tapered against its own nominal. On a
        //     mid-band reserve that is a strict over-statement, so the corrected
        //     estimate has to come in under it.
        {
            fixture g = make_fixture(/*sites=*/2, /*reserve=*/195.2f, /*with_market=*/true);
            const building_profit est = estimate_next(g, reg);

            // The old model, restated here rather than imported: lone nominal 20, its
            // own band 8 x 20 = 160, reserve 195.2 -> taper clamps to 1.
            const float old_model = k_lone_nominal * 1.0f * k_iron_price;
            check(est.revenue < old_model,
                  "S.R7 the stack-aware estimate is BELOW the lone-site model it replaced",
                  est.revenue, old_model);

            // A lone site on an identical reserve still estimates the old figure — the
            // fix is a correction for stacked sites, not a haircut on every estimate.
            fixture solo = make_fixture(/*sites=*/0, /*reserve=*/195.2f, /*with_market=*/true);
            const building_profit est_solo = estimate_next(solo, reg);
            check(near(est_solo.revenue, old_model, 1e-2f),
                  "S.R7 an unstacked tile estimates exactly what it always did",
                  est_solo.revenue, old_model);
        }
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
