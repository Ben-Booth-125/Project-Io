// Headless harness: BL-1187 (build only what runs) — the OBTAINABLE input test
// (src/world/input_reach.hpp) and the scorer decisions it gates (corp_ai.cpp).
// No SDL / Lua / ImGui.
//
// Fixture: ONE plains body 64x4; three markets on row 0 — A at column 0 (the
// consumer's market), N at column 6 (near), F at column 40 (far). Coal is priced
// so the dispatcher's export gate (price_A - haul > (1 + dispatch_margin) x
// price_src) passes from N and fails from F. reservation_mult 2.0.
//
//   R1 CROSS-MARKET: a coal mine in N admits coal at A; the same mine in F does
//      not; a mine in A itself admits; a mine in N whose coal lands over A's
//      fair-price ceiling does not; a lane the OLD haul bound admitted but the
//      dispatcher's gate refuses is refused; with the ceiling OFF a mine in N is
//      still in reach.
//   R2 SPARE CAPACITY: one mine, two consumers. With no other consumer the
//      candidate is admitted; with a standing consumer drawing the mine's whole
//      output it is refused; that consumer asking for itself is admitted (its
//      own draw is not its competitor).
//   R3 PRODUCED THIS TICK: with a report carrying rows, a mine whose row is idle
//      supplies nothing; the same mine with an active row does; without rows
//      (generation) the nominal rate counts. An exhausted primary, an
//      unstaffed site and one under construction never count.
//   R4 BUILD (scorer, 12 evals): with no reachable coal the AI never builds the
//      coal processor; with a coal mine in N it does.
//   R5 RESUME (scorer, 12 evals): an idled coal plant stays idled while no coal
//      can reach it; once a mine in N stands, it resumes.
//
//   R6 OWN OUTPUT IS NOT OWN STOCK COVER (BL-1206 cold review): a steel plant
//      whose pool holds the steel it made may not count it toward a steel-eating
//      recipe; a third party's spare may. Shared test, the reflex rescue (one
//      run_economy_step) and the scorer's within-group switch (12 evals); and an
//      unstaffed plant is judged at a non-zero need (judged_batches).
//   R7 THE GLUT GATE INSIDE THE ARGMAX (BL-1227): a group's net winner whose
//      output market is dead yields to its runner-up, not to nothing.
//   R8 THE CHAIN START (BL-1227, AI_OPPONENT.md § 11): a corp's own processor
//      refused only for an unobtainable input lifts the dead-market veto on
//      its OWN mine for that input, in that evaluation only; another corp's
//      mine stays vetoed; the next evaluation without the refusal is vetoed.
//   R4-R8 run IN PLAY (current_econ_tick > 0, a market that has cleared).
//
// Exits non-zero on any FAIL.

#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/economy_system.hpp"
#include "world/input_reach.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <tuple>

namespace {

int g_fail = 0;
void check(bool ok, const char* what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

constexpr std::size_t r_coal  = static_cast<std::size_t>(resource_type::coal);
constexpr std::size_t r_steel = static_cast<std::size_t>(resource_type::steel);
constexpr std::size_t r_mach  = static_cast<std::size_t>(resource_type::machinery);

struct scene
{
    world     w;
    entity_id body = null_entity;
    entity_id a = null_entity, n = null_entity, f = null_entity; ///< markets
    entity_id ai = null_entity, pl = null_entity;                 ///< corps
    entity_id ai_tile = null_entity;                              ///< the AI's own ground, in A
};

entity_id tile_at(world& w, entity_id body, int c, int r)
{
    const int gw = w.bodies.at(body).grid_width;
    return body_tile_grid(w, body)[static_cast<std::size_t>(r) * static_cast<std::size_t>(gw)
                                   + static_cast<std::size_t>(c)];
}

recipe_registry make_registry()
{
    recipe_registry reg;
    price_band_params pb = reg.price_band();
    pb.reservation_mult = 2.0f;
    reg.set_price_band(pb);

    building_economics ex;
    ex.base_rate = 10.0f; ex.maintenance = 0.0f; ex.base_wage = 0.0f;
    ex.build_cost = 100.0f; ex.build_duration_ticks = 2.0f;
    reg.set_economics(building_type::extraction_site, ex);
    building_economics pe;
    pe.base_rate = 1.0f; pe.maintenance = 0.0f; pe.base_wage = 0.0f;
    pe.build_cost = 100.0f; pe.build_duration_ticks = 2.0f;
    reg.set_economics(building_type::processing_facility, pe);

    recipe coal_steel;
    coal_steel.name  = "coal_steel";
    coal_steel.group = "Foundry";
    coal_steel.inputs [r_coal]  = 1.0f;
    coal_steel.outputs[r_steel] = 3.0f;
    reg.add_recipe(coal_steel);

    // BL-1206 (R6): two recipes that EAT steel — one in another group (only the
    // reflex rescue, which may cross groups, can reach it) and one in coal_steel's
    // own group (the scorer's within-group switch).
    recipe forge;
    forge.name  = "steel_forge";
    forge.group = "Forge";
    forge.inputs [r_steel] = 1.0f;
    forge.outputs[r_mach]  = 1.0f;
    reg.add_recipe(forge);
    recipe temper;
    temper.name  = "steel_temper";
    temper.group = "Foundry";
    temper.inputs [r_steel] = 1.0f;
    temper.outputs[r_mach]  = 1.0f;
    reg.add_recipe(temper);
    return reg;
}

entity_id add_market(scene& s, int col)
{
    const entity_id id = s.w.create_entity();
    market_component m{};
    m.body        = s.body;
    m.centre_tile = tile_at(s.w, s.body, col, 0);
    m.base_price[r_coal]  = 2.0f; // reset from the hauls below
    m.base_price[r_steel] = 20.0f;
    m.base_price[r_mach]  = 400.0f; // BL-1206 R6: the steel-eaters' output, dear
    m.price = m.base_price;
    s.w.markets[id] = m;
    return id;
}

scene make_scene()
{
    scene s;
    s.body = s.w.create_entity();
    body_component bc{};
    bc.name = "Anvil"; bc.type = body_type::planet; bc.orbital_radius_au = 1.0f;
    bc.grid_width = 64; bc.grid_height = 4;
    bc.survey.phase = survey_phase::surveyed;
    s.w.bodies[s.body] = bc;
    s.w.home_body = s.body;
    for (int r = 0; r < bc.grid_height; ++r)
        for (int c = 0; c < bc.grid_width; ++c)
        {
            const entity_id t = s.w.create_entity();
            tile_component tc{};
            tc.body = s.body; tc.grid_x = c; tc.grid_y = r;
            tc.substrate = terrain_substrate::sedimentary;
            tc.cover = terrain_cover::grass; tc.cover_density = 150;
            tc.landform = terrain_landform::plains;
            s.w.tiles[t] = tc;
        }
    s.a = add_market(s, 0);
    s.n = add_market(s, 6);
    s.f = add_market(s, 40);

    // The player corp holds every producer (the scorer never acts on it).
    s.pl = s.w.create_entity();
    corporation_component pc;
    pc.name = "Player"; pc.is_player = true; pc.balance = 1000.0f;
    s.w.corporations[s.pl] = pc;
    s.w.player_entity = s.pl;

    // The AI corp: cash, and one plain anchor building on its own ground in A.
    s.ai = s.w.create_entity();
    corporation_component ac;
    ac.name = "Rival"; ac.balance = 1.0e6f; ac.starting_capital = 1.0e6f;
    ac.focus = industrial_focus::processing;
    s.ai_tile = tile_at(s.w, s.body, 1, 1);
    const entity_id anchor = s.w.create_entity();
    building_component b{};
    b.tile = s.ai_tile; b.type = building_type::extraction_site;
    b.target_resource = resource_type::iron_ore; b.workforce_assigned = 0.5f;
    s.w.buildings[anchor] = b;
    ac.assets.push_back(anchor);
    s.w.corporations[s.ai] = ac;
    return s;
}

/// Price coal so the export gate passes from N and fails from F in BOTH forms:
/// N and F price coal at 1 (base and posted). A's base is set for the
/// generation form (2 x base_A - haul > (1 + margin) x 1, the gate the scorer
/// rows below read: their reports are empty), and A's posted price for the play
/// form (price_A - haul > (1 + margin) x 1). Returns false if the fixture
/// cannot separate the two lanes.
bool place_bound(scene& s, const recipe_registry& reg)
{
    input_reach ir = make_input_reach(s.w, reg);
    const float hn = input_reach_haul(s.w, reg, ir, s.n, s.a);
    const float hf = input_reach_haul(s.w, reg, ir, s.f, s.a);
    std::printf("  hauls per unit: N->A %.4f  F->A %.4f  (dispatch margin %.2f)\n", hn, hf,
                ir.dispatch_margin);
    if (!(hn > 0.0f) || !(hf > hn))
        return false;
    const float mid_haul = 0.5f * (hn + hf);
    const float pa_play  = (1.0f + ir.dispatch_margin) + mid_haul;
    const float ba_gen   = pa_play / ir.gen_price_mult;
    for (auto& [mid, m] : s.w.markets)
    {
        m.base_price[r_coal] = (mid == s.a) ? ba_gen : 1.0f;
        m.price[r_coal]      = (mid == s.a) ? pa_play : 1.0f;
    }
    return true;
}

/// A staffed coal mine (player-owned) on a fresh coal tile at (col, row).
entity_id add_mine(scene& s, int col, int row)
{
    const entity_id t = tile_at(s.w, s.body, col, row);
    s.w.tiles.at(t).resource_deposit[r_coal]   = 1.0f;
    s.w.tiles.at(t).resource_remaining[r_coal] = 1.0e6f;
    const entity_id m = s.w.create_entity();
    building_component b{};
    b.tile = t; b.type = building_type::extraction_site;
    b.target_resource = resource_type::coal; b.workforce_assigned = 0.5f;
    s.w.buildings[m] = b;
    s.w.corporations.at(s.pl).assets.push_back(m);
    return m;
}

/// A staffed coal-steel plant owned by @p corp at (col, row).
entity_id add_plant(scene& s, const recipe_registry& reg, entity_id corp, int col, int row,
                    float workforce, bool idled)
{
    const entity_id p = s.w.create_entity();
    building_component b{};
    b.tile = tile_at(s.w, s.body, col, row);
    b.type = building_type::processing_facility;
    b.recipe = reg.recipe_id("coal_steel");
    b.target_resource = resource_type::steel;
    b.workforce_assigned = workforce;
    b.workforce_auto = false;
    b.decommissioned = idled;
    s.w.buildings[p] = b;
    s.w.corporations.at(corp).assets.push_back(p);
    return p;
}

bool coal_ok(scene& s, const recipe_registry& reg, float need, entity_id self = null_entity,
             const economy_report* rep = nullptr)
{
    input_reach ir = make_input_reach(s.w, reg);
    ir.report = rep;
    return input_obtainable(s.w, reg, ir, s.a, nullptr, r_coal, need, self).obtainable;
}

int processors_of(const scene& s, entity_id corp)
{
    int n = 0;
    for (const entity_id b : s.w.corporations.at(corp).assets)
        if (const auto it = s.w.buildings.find(b);
            it != s.w.buildings.end() && it->second.type == building_type::processing_facility)
            ++n;
    return n;
}

} // namespace

int main()
{
    std::printf("build_only_what_runs — BL-1187\n");
    const recipe_registry reg = make_registry();
    // One run's need: 1 coal a batch at the plant's staffing (1.0 x base_rate 1).
    const float need = 1.0f;

    std::printf("R1 cross-market reach\n");
    {
        scene s = make_scene();
        const bool fixture = place_bound(s, reg);
        check(fixture, "R1 fixture: the N->A haul is under the F->A haul (the gate placed between)");
        check(!coal_ok(s, reg, need), "R1 no producer anywhere: coal refused at A");
        {
            scene t = make_scene(); place_bound(t, reg);
            add_mine(t, 6, 2);
            check(coal_ok(t, reg, need), "R1 a mine in N (the dispatcher gate passes): coal admitted at A");
        }
        {
            scene t = make_scene(); place_bound(t, reg);
            add_mine(t, 40, 2);
            check(!coal_ok(t, reg, need), "R1 the same mine in F (its haul eats the price gap): coal refused at A");
        }
        {
            scene t = make_scene(); place_bound(t, reg);
            add_mine(t, 0, 2);
            check(coal_ok(t, reg, need), "R1 a mine in A itself: coal admitted");
        }
        {
            scene t = make_scene(); place_bound(t, reg);
            add_mine(t, 6, 2);
            market_component& mn = t.w.markets.at(t.n);
            mn.price[r_coal] = 2.0f * t.w.markets.at(t.a).base_price[r_coal]; // + haul lands over A's ceiling
            check(!coal_ok(t, reg, need),
                  "R1 a mine in N whose market posts coal so dear it lands over A's ceiling: refused");
        }
        {
            // PLAY: the OLD reach bound (haul <= (reservation_mult - 1 - margin)
            // x base at A) admits this lane; the dispatcher's literal gate does
            // not, because N posts coal at A's price and a haul can never beat
            // that by the margin. (The mine has a producing row this tick.)
            scene t = make_scene(); place_bound(t, reg);
            const entity_id mine = add_mine(t, 6, 2);
            const float pa = t.w.markets.at(t.a).price[r_coal];
            for (auto& [mid, m] : t.w.markets) { (void)mid; m.base_price[r_coal] = pa; m.price[r_coal] = pa; }
            input_reach ir = make_input_reach(t.w, reg);
            const float hn = input_reach_haul(t.w, reg, ir, t.n, t.a);
            const bool old_bound = hn <= (2.0f - 1.0f - ir.dispatch_margin) * pa;
            economy_report rep;
            building_report br;
            br.building = mine; br.type = building_type::extraction_site;
            br.target_resource = resource_type::coal; br.active = true; br.output_quantity = 5.0f;
            rep.buildings.push_back(br);
            rep.building_row[mine] = 0;
            check(old_bound && !coal_ok(t, reg, need, null_entity, &rep),
                  "R1 play: a lane the old haul bound admitted but the dispatcher's gate refuses is REFUSED");
        }
        {
            // GENERATION-SIDE reach (no report): equal bases everywhere and the
            // short N->A haul. The play-side test on base prices would refuse it
            // (a haul can never beat an equal price); generation takes A at
            // reservation_mult x base, so the lane is in reach. F's long haul is
            // priced out even so if it eats more than that headroom.
            scene t = make_scene();
            for (auto& [mid, m] : t.w.markets) { (void)mid; m.base_price[r_coal] = 1.0f; m.price[r_coal] = 1.0f; }
            add_mine(t, 6, 2);
            input_reach ir = make_input_reach(t.w, reg);
            const float hn = input_reach_haul(t.w, reg, ir, t.n, t.a);
            const bool cheap = 2.0f * 1.0f - hn > (1.0f + ir.dispatch_margin) * 1.0f;
            check(cheap && coal_ok(t, reg, need),
                  "R1 generation: equal bases and a cheap haul (N->A) are IN reach");
            economy_report rep;
            building_report br; br.building = null_entity; rep.buildings.push_back(br);
            check(!coal_ok(t, reg, need, null_entity, &rep),
                  "R1 play: the same equal-priced lane fails the dispatcher's literal test");
        }
        {
            // The ceiling OFF must not collapse reach to the same market.
            recipe_registry off = make_registry();
            price_band_params pb = off.price_band();
            pb.reservation_mult = 0.0f;
            off.set_price_band(pb);
            scene t = make_scene(); place_bound(t, off);
            add_mine(t, 6, 2);
            check(coal_ok(t, off, need), "R1 ceiling OFF (reservation_mult 0): a mine in N is still in reach");
        }
    }

    std::printf("R2 spare capacity\n");
    {
        scene s = make_scene(); place_bound(s, reg);
        const entity_id mine = add_mine(s, 0, 2);
        input_reach probe = make_input_reach(s.w, reg);
        const float out = building_output(s.w, reg, mine, s.w.buildings.at(mine), r_coal, nullptr);
        std::printf("  mine nominal coal %.3f a tick; t_idle %.2f\n", out, reg.t_idle());
        // A candidate whose idle-threshold share fits the mine's output.
        const float cand_need = 0.9f * out / reg.t_idle();
        check(coal_ok(s, reg, cand_need), "R2 one mine, no other consumer: the candidate is admitted");
        // A standing consumer drawing HALF the mine's output: the mine still has
        // spare, but less than the candidate's idle-threshold share.
        const entity_id first = add_plant(s, reg, s.pl, 0, 3, 0.5f * out, /*idled=*/false);
        check(!coal_ok(s, reg, cand_need),
              "R2 a standing consumer draws half the mine: the spare left is under the SECOND's need -> refused");
        check(coal_ok(s, reg, cand_need, first),
              "R2 that consumer asking for itself is admitted (its own draw is not its competitor)");
    }

    std::printf("R3 produced this tick\n");
    {
        scene s = make_scene(); place_bound(s, reg);
        const entity_id mine = add_mine(s, 0, 2);
        economy_report rep;
        building_report br;
        br.building = mine; br.type = building_type::extraction_site;
        br.target_resource = resource_type::coal; br.active = false; br.output_quantity = 0.0f;
        rep.buildings.push_back(br);
        rep.building_row[mine] = 0;
        // A second, unrelated row so the report is not empty either way.
        check(!coal_ok(s, reg, need, null_entity, &rep),
              "R3 play: the mine's row produced nothing this tick -> it supplies nothing");
        rep.buildings[0].active = true; rep.buildings[0].output_quantity = 5.0f;
        check(coal_ok(s, reg, need, null_entity, &rep),
              "R3 play: the same mine's row produced this tick -> admitted");
        check(coal_ok(s, reg, need), "R3 generation (no report): the nominal rate counts");

        building_component& m = s.w.buildings.at(mine);
        m.ticks_remaining = 2;
        check(!coal_ok(s, reg, need), "R3 a mine under construction never counts");
        m.ticks_remaining = 0; m.workforce_assigned = 0.0f;
        check(!coal_ok(s, reg, need), "R3 an unstaffed mine never counts");
        m.workforce_assigned = 0.5f;
        s.w.tiles.at(m.tile).resource_remaining[r_coal] = 0.0f;
        check(!coal_ok(s, reg, need), "R3 a mine whose primary is spent yields nothing");
    }

    std::printf("R4 build (scorer, 12 evaluations)\n");
    {
        auto walk = [&](bool with_mine) {
            scene s = make_scene(); place_bound(s, reg);
            if (with_mine) add_mine(s, 6, 2);
            // BL-1227: IN PLAY - a cleared market A that bids for the steel the
            // plant would make (econ tick > 0; at 0 the glut forecast reads a
            // never-cleared market and proves nothing about play).
            s.w.markets.at(s.a).demand[r_steel] = 50.0f;
            int first = -1;
            for (int t = 1; t <= 12; ++t)
            {
                s.w.current_econ_tick = t;
                economy_report rep;
                run_corp_strategic_step(s.w, reg, rep, t);
                if (first < 0 && processors_of(s, s.ai) > 0) first = t;
            }
            return first;
        };
        const int none = walk(false);
        const int near = walk(true);
        std::printf("  first processor built: no coal -> %d, mine in N -> %d (-1 = never)\n", none, near);
        check(none < 0, "R4 no reachable coal: the coal processor is never built over 12 evaluations");
        check(near > 0, "R4 a coal mine in N: the coal processor is built");
    }

    std::printf("R5 resume (scorer, 12 evaluations)\n");
    {
        auto walk = [&](int mine_at_tick, int mine_col) {
            scene s = make_scene(); place_bound(s, reg);
            const entity_id plant = add_plant(s, reg, s.ai, 1, 1, 1.0f, /*idled=*/true);
            int resumed = -1;
            for (int t = 1; t <= 12; ++t)
            {
                if (t == mine_at_tick) add_mine(s, mine_col, 2);
                s.w.current_econ_tick = t; // BL-1227: in play
                economy_report rep;
                run_corp_strategic_step(s.w, reg, rep, t);
                if (resumed < 0 && !s.w.buildings.at(plant).decommissioned) resumed = t;
            }
            return resumed;
        };
        const int never = walk(-1, 6);
        const int far   = walk(4, 40);
        const int later = walk(6, 6);
        std::printf("  resumed at: no coal -> %d, mine in F from tick 4 -> %d, mine in N from tick 6 -> %d\n",
                    never, far, later);
        check(never < 0, "R5 no coal can reach it: the idled plant stays idled for 12 evaluations");
        check(far < 0, "R5 a mine beyond reach (F) does not bring it back");
        check(later >= 6, "R5 once a mine in N stands, the idled plant resumes (and not before)");
    }

    // R6 (BL-1206 cold review): a steel plant asking whether it could run a
    // recipe that EATS steel. Its pool holds the steel it made; that is not
    // cover, because it stops arriving the tick the plant switches. Only a
    // third party's spare admits the switch — in the shared test, in the
    // reflex rescue, and in the scorer's switch.
    std::printf("R6 own output is not own stock cover (shared test, reflex, scorer)\n");
    {
        const uint16_t coal_steel = reg.recipe_id("coal_steel");
        const recipe*  forge      = reg.get_recipe(reg.recipe_id("steel_forge"));
        // AI steel plant P at A (col 1, row 2), its own steel leftovers in its pool.
        auto make = [&](bool third_party, float price_steel_mult) {
            scene s = make_scene(); place_bound(s, reg);
            const entity_id p = add_plant(s, reg, s.ai, 1, 2, 1.0f, /*idled=*/false);
            s.w.pool_at(s.ai, s.a).quantities[r_steel] = 100.0f;
            entity_id third = null_entity;
            if (third_party)
            {
                third = add_plant(s, reg, s.pl, 0, 3, 1.0f, /*idled=*/false);
                s.w.pool_at(s.pl, s.a).quantities[r_coal] = 1000.0f;
            }
            for (auto& [mid, m] : s.w.markets)
            {
                (void)mid;
                m.price[r_steel] = price_steel_mult * m.base_price[r_steel];
                m.price[r_mach]  = m.base_price[r_mach];
            }
            return std::make_tuple(std::move(s), p, third);
        };
        {
            auto [s, p, third] = make(false, 1.0f);
            (void)third;
            input_reach ir = make_input_reach(s.w, reg);
            const stockpile_component* pool = s.w.find_pool(s.ai, s.a);
            const float need = judged_batches(reg, s.w.buildings.at(p)) * forge->inputs[r_steel];
            check(!input_obtainable(s.w, reg, ir, s.a, pool, r_steel, need, p).obtainable,
                  "R6 shared: the steel plant's own leftover steel does NOT admit a steel-eating run");
            input_reach ir2 = make_input_reach(s.w, reg);
            check(input_obtainable(s.w, reg, ir2, s.a, pool, r_steel, need, null_entity).obtainable,
                  "R6 shared: the same stock DOES cover a candidate that is not its maker (a build)");
        }
        {
            auto [s, p, third] = make(true, 1.0f);
            (void)third;
            input_reach ir = make_input_reach(s.w, reg);
            const stockpile_component* pool = s.w.find_pool(s.ai, s.a);
            const float need = judged_batches(reg, s.w.buildings.at(p)) * forge->inputs[r_steel];
            check(input_obtainable(s.w, reg, ir, s.a, pool, r_steel, need, p).obtainable,
                  "R6 shared: a third party's steel plant in A (spare) admits it");
        }
        {
            // judged_batches: an unstaffed plant is judged at its authored
            // staffing, never at zero need.
            building_component b{};
            b.type = building_type::processing_facility;
            b.workforce_assigned = 0.0f;
            check(judged_batches(reg, b) > 0.0f,
                  "R6 an unstaffed plant is judged at a non-zero need (labour basis, not zero)");
        }
        // The REFLEX rescue: P's steel floored (0.1 x base), machinery dear. One
        // economy step; the scorer is held off P (ai_cooldown) so any switch is
        // the reflex's.
        auto reflex = [&](bool third_party) {
            auto [s, p, third] = make(third_party, 0.1f);
            (void)third;
            s.w.buildings.at(p).ai_cooldown = 1000;
            s.w.current_econ_tick = 1;
            const economy_report rep = run_economy_step(s.w, reg);
            bool third_ran = false;
            for (const building_report& br : rep.buildings)
                if (br.building == third && br.active) third_ran = true;
            return std::make_pair(s.w.buildings.at(p).recipe, third_ran);
        };
        {
            const auto [rc, ran] = reflex(false);
            (void)ran;
            check(rc == coal_steel,
                  "R6 reflex: a floored steel plant with only its OWN steel stays on coal_steel");
        }
        {
            const auto [rc, ran] = reflex(true);
            std::printf("  reflex with a third-party steel plant: it ran this tick %s; recipe now %s\n",
                        ran ? "yes" : "NO", reg.get_recipe(rc) ? reg.get_recipe(rc)->name.c_str() : "?");
            check(ran && rc != coal_steel,
                  "R6 reflex: with a third party's steel produced this tick, it may switch to a steel-eater");
        }
        // The SCORER's within-group switch (steel_temper), 12 evaluations.
        auto scorer = [&](bool third_party) {
            auto [s, p, third] = make(third_party, 1.0f);
            (void)third;
            const uint16_t temper = reg.recipe_id("steel_temper");
            int at = -1;
            for (int t = 1; t <= 12; ++t)
            {
                s.w.current_econ_tick = t; // BL-1227: in play
                economy_report rep;
                run_corp_strategic_step(s.w, reg, rep, t);
                if (at < 0 && s.w.buildings.at(p).recipe == temper) at = t;
            }
            return at;
        };
        const int own_only = scorer(false);
        const int with_3p  = scorer(true);
        std::printf("  scorer switched to steel_temper at: own stock only -> %d, third party -> %d (-1 never)\n",
                    own_only, with_3p);
        check(own_only < 0, "R6 scorer: never switches onto its own leftover steel");
        check(with_3p > 0, "R6 scorer: switches when a third party's spare steel covers it");
    }

    // R7 (BL-1227 review round 1): THE GLUT GATE INSIDE THE ARGMAX, through
    // the scorer, in play. A second coal recipe joins coal_steel's group
    // (Foundry): coal -> machinery, whose net dwarfs steel's (machinery base
    // 400), so it WINS the group's net argmax. When A bids for machinery it is
    // built; when A has cleared but nobody bids for or lists machinery (a dead
    // market), the veto must fall through to the runner-up — coal_steel, which
    // A does bid for — rather than leave the group empty.
    std::printf("R7 the dead-market veto inside the processor argmax (scorer, in play)\n");
    {
        recipe_registry reg7 = make_registry();
        recipe coal_mach;
        coal_mach.name  = "coal_mach";
        coal_mach.group = "Foundry";
        coal_mach.inputs [r_coal] = 1.0f;
        coal_mach.outputs[r_mach] = 1.0f;
        reg7.add_recipe(coal_mach);
        auto built = [&](bool mach_bid) {
            scene s = make_scene(); place_bound(s, reg7);
            add_mine(s, 6, 2);
            s.w.markets.at(s.a).demand[r_steel] = 50.0f;
            if (mach_bid)
                s.w.markets.at(s.a).demand[r_mach] = 50.0f;
            for (int t = 1; t <= 12; ++t)
            {
                s.w.current_econ_tick = t;
                economy_report rep;
                run_corp_strategic_step(s.w, reg7, rep, t);
                for (const entity_id b : s.w.corporations.at(s.ai).assets)
                    if (const auto it = s.w.buildings.find(b);
                        it != s.w.buildings.end() && it->second.type == building_type::processing_facility)
                        return it->second.recipe;
            }
            return no_recipe;
        };
        const uint16_t with_bid = built(true);
        const uint16_t dead     = built(false);
        const auto nm = [&](uint16_t id) {
            const recipe* r = reg7.get_recipe(id);
            return r ? r->name.c_str() : "(none)";
        };
        std::printf("  first processor: machinery bid -> %s, machinery dead -> %s\n", nm(with_bid), nm(dead));
        check(with_bid == reg7.recipe_id("coal_mach"),
              "R7 not vacuous: with a machinery bid the fatter coal_mach wins the group and is built");
        check(dead == reg7.recipe_id("coal_steel"),
              "R7 machinery dead in a cleared market: the vetoed winner yields to coal_steel, not to nothing");
    }

    // R8 (BL-1227, the chain start; AI_OPPONENT.md § 11, Ben 2026-10-07/08). A
    // coal deposit stands in A, A has cleared (it bids for steel) but nobody
    // bids for or lists coal there — a dead market for coal, so a coal mine is
    // vetoed. The AI corp's own coal_steel candidate on its ground in A is
    // refused ONLY because coal is unobtainable (no producer anywhere). That
    // refused draw is the corp's private bid on coal in A, for its own mine
    // candidates, in that one evaluation.
    std::printf("R8 the chain start: a corp's own refused processor bids for its own mine\n");
    {
        auto staged = [&]() {
            scene s = make_scene(); place_bound(s, reg);
            s.w.markets.at(s.a).demand[r_steel] = 50.0f;
            const entity_id ct = tile_at(s.w, s.body, 2, 2);
            s.w.tiles.at(ct).resource_deposit[r_coal]   = 1.0f;
            s.w.tiles.at(ct).resource_remaining[r_coal] = 1.0e6f;
            return s;
        };
        auto coal_mines_of = [&](const scene& s, entity_id corp) {
            int n = 0;
            for (const entity_id b : s.w.corporations.at(corp).assets)
                if (const auto it = s.w.buildings.find(b);
                    it != s.w.buildings.end() && it->second.type == building_type::extraction_site
                    && it->second.target_resource == resource_type::coal)
                    ++n;
            return n;
        };
        auto lifts_for = [&](const economy_report& rep, entity_id corp) {
            int n = 0;
            for (const auto& l : rep.chain_start_lifts)
                if (l.corp == corp && l.target == resource_type::coal) ++n;
            return n;
        };
        // The AI corp evaluates at tick % 4 == its index % 4; walk 4 ticks so
        // it is due exactly once.
        auto one_eval = [&](scene& s, int& lifts) {
            lifts = 0;
            for (int t = 1; t <= 4; ++t)
            {
                s.w.current_econ_tick = t;
                economy_report rep;
                run_corp_strategic_step(s.w, reg, rep, t);
                lifts += lifts_for(rep, s.ai);
            }
        };

        // (a) the refusing corp: the veto is lifted and the mine is built on
        // its ordinary score.
        {
            scene s = staged();
            int lifts = 0;
            one_eval(s, lifts);
            std::printf("  (a) lifts %d, AI coal mines %d\n", lifts, coal_mines_of(s, s.ai));
            check(lifts > 0, "R8 (a) a corp's own processor refused only for coal lifts its own coal-mine veto");
            check(coal_mines_of(s, s.ai) > 0, "R8 (a) ... and the lifted mine, scored on the ordinary estimate, is built");
        }
        // (b) a DIFFERENT corp, with no processor candidate of its own, beside
        // the refusing one: its mine in the same market stays vetoed.
        {
            scene s = staged();
            // The other corp holds ground in A (an anchor) whose tile already
            // carries a processor, so it offers no processor candidate of its
            // own (one processor per tile) — and so refuses nothing.
            auto add_other = [&](scene& sc) {
                const entity_id id = sc.w.create_entity();
                corporation_component c;
                c.name = "Other"; c.balance = 1.0e6f; c.starting_capital = 1.0e6f;
                c.focus = industrial_focus::extraction;
                const entity_id t = tile_at(sc.w, sc.body, 3, 1);
                const entity_id anchor = sc.w.create_entity();
                building_component a{};
                a.tile = t; a.type = building_type::extraction_site;
                a.target_resource = resource_type::iron_ore; a.workforce_assigned = 0.5f;
                sc.w.buildings[anchor] = a;
                const entity_id plant = sc.w.create_entity();
                building_component pb{};
                pb.tile = t; pb.type = building_type::processing_facility;
                pb.recipe = reg.recipe_id("steel_forge"); pb.target_resource = resource_type::machinery;
                pb.workforce_assigned = 0.5f; pb.workforce_auto = false;
                sc.w.buildings[plant] = pb;
                c.assets = {anchor, plant};
                sc.w.corporations[id] = c;
                return id;
            };
            const entity_id other = add_other(s);
            // The refusing AI corp still refuses (and would lift for itself) but
            // cannot afford a mine, so the deposit's one slot stays open to Other.
            s.w.corporations.at(s.ai).balance = 1.0f;
            int other_lifts = 0;
            for (int t = 1; t <= 4; ++t)
            {
                s.w.current_econ_tick = t;
                economy_report rep;
                run_corp_strategic_step(s.w, reg, rep, t);
                other_lifts += lifts_for(rep, other);
            }
            std::printf("  (b) other corp: lifts %d, coal mines %d\n", other_lifts, coal_mines_of(s, other));
            check(other_lifts == 0 && coal_mines_of(s, other) == 0,
                  "R8 (b) another corp's refused want is not a bid: its mine in the same dead market stays vetoed");
            // Not vacuous: the same corp DOES build that mine once A bids for coal.
            scene s2 = staged();
            const entity_id other2 = add_other(s2);
            s2.w.corporations.at(s2.ai).balance = 1.0f;
            s2.w.markets.at(s2.a).demand[r_coal] = 50.0f;
            for (int t = 1; t <= 4; ++t)
            {
                s2.w.current_econ_tick = t;
                economy_report rep;
                run_corp_strategic_step(s2.w, reg, rep, t);
            }
            check(coal_mines_of(s2, other2) > 0,
                  "R8 (b) not vacuous: with a public coal bid in A the other corp builds that mine");
        }
        // (c) no memory: an evaluation that lifts but cannot afford the mine,
        // then the next one with the processor candidate gone (the corp's
        // ground sold) and the cash back — vetoed again.
        {
            scene s = staged();
            corporation_component& ac = s.w.corporations.at(s.ai);
            ac.balance = 1.0f;
            int lifts1 = 0;
            one_eval(s, lifts1);
            const int built1 = coal_mines_of(s, s.ai);
            ac.balance = 1.0e6f;
            ac.assets.clear();
            int lifts2 = 0;
            for (int t = 5; t <= 8; ++t)
            {
                s.w.current_econ_tick = t;
                economy_report rep;
                run_corp_strategic_step(s.w, reg, rep, t);
                lifts2 += lifts_for(rep, s.ai);
            }
            std::printf("  (c) eval 1 (poor): lifts %d built %d | eval 2 (no refused processor): lifts %d built %d\n",
                        lifts1, built1, lifts2, coal_mines_of(s, s.ai));
            check(lifts1 > 0 && built1 == 0, "R8 (c) not vacuous: the first evaluation lifts, but cannot afford the mine");
            check(lifts2 == 0 && coal_mines_of(s, s.ai) == 0,
                  "R8 (c) the next evaluation, without the refused candidate, is vetoed again (no memory)");
        }
        // (d) the PLAYER's corp is never a chain start, even when a spectated
        // session evaluates it like a rival: the same staging as (a), with the
        // AI corp made the seat and the scorer told it is spectating.
        {
            scene s = staged();
            s.w.corporations.at(s.ai).is_player = true;
            s.w.corporations.at(s.pl).is_player = false;
            s.w.player_entity = s.ai;
            corp_ai_params sp;
            sp.spectating = true;
            int lifts = 0;
            bool evaluated = false;
            for (int t = 1; t <= 4; ++t)
            {
                s.w.current_econ_tick = t;
                economy_report rep;
                run_corp_strategic_step(s.w, reg, rep, t, sp);
                lifts += lifts_for(rep, s.ai);
                for (const entity_id c : rep.corps_evaluated)
                    if (c == s.ai) evaluated = true;
            }
            std::printf("  (d) seat under spectate: evaluated %s, lifts %d, coal mines %d\n",
                        evaluated ? "yes" : "no", lifts, coal_mines_of(s, s.ai));
            check(evaluated, "R8 (d) not vacuous: under spectate the seat IS evaluated like a rival");
            check(lifts == 0 && coal_mines_of(s, s.ai) == 0,
                  "R8 (d) the player's corp is excluded from the chain start: no lift, no mine");
        }
    }

    // R9 (BL-1227 review round 3; Ben 2026-10-07/08: "a buyer that takes goods
    // without posting a bid is still a buyer"). Market A has cleared (it bids
    // for steel) and nobody bids for or lists coal there. Its only coal
    // consumer is the player's coal_steel plant, fed from the player's own
    // pool — it posts no bid. Through run_economy_step (production, then the
    // scorer), in play: while that plant RUNS, its draw is a bid and a third
    // corp builds the coal mine; with the plant IDLED (decommissioned), it
    // draws nothing, is no buyer, and the mine stays vetoed.
    std::printf("R9 a running consumer fed from its own pool is a bid (run_economy_step, in play)\n");
    {
        auto mines_for = [&](bool plant_idled) {
            scene s = make_scene(); place_bound(s, reg);
            s.w.markets.at(s.a).demand[r_steel] = 50.0f;
            s.w.corporations.at(s.ai).balance = 1.0f; // the processing-focus AI stays out (no chain start)
            const entity_id ct = tile_at(s.w, s.body, 2, 2);
            s.w.tiles.at(ct).resource_deposit[r_coal]   = 1.0f;
            s.w.tiles.at(ct).resource_remaining[r_coal] = 1.0e6f;
            add_plant(s, reg, s.pl, 0, 3, 1.0f, plant_idled);
            // The mine-builder: holds ground in A whose tile already carries a
            // processor, so it offers no processor candidate (and refuses none).
            const entity_id other = s.w.create_entity();
            corporation_component oc;
            oc.name = "Other"; oc.balance = 1.0e6f; oc.starting_capital = 1.0e6f;
            oc.focus = industrial_focus::extraction;
            const entity_id t = tile_at(s.w, s.body, 3, 1);
            const entity_id anchor = s.w.create_entity();
            building_component a{};
            a.tile = t; a.type = building_type::extraction_site;
            a.target_resource = resource_type::iron_ore; a.workforce_assigned = 0.5f;
            s.w.buildings[anchor] = a;
            const entity_id plant = s.w.create_entity();
            building_component pb{};
            pb.tile = t; pb.type = building_type::processing_facility;
            pb.recipe = reg.recipe_id("steel_forge"); pb.target_resource = resource_type::machinery;
            pb.workforce_assigned = 0.5f; pb.workforce_auto = false;
            s.w.buildings[plant] = pb;
            oc.assets = {anchor, plant};
            s.w.corporations[other] = oc;
            bool ran = false;
            for (int t2 = 1; t2 <= 4; ++t2)
            {
                s.w.pool_at(s.pl, s.a).quantities[r_coal] = 1000.0f;
                s.w.current_econ_tick = t2;
                const economy_report rep = run_economy_step(s.w, reg);
                for (const building_report& br : rep.buildings)
                    if (br.corp == s.pl && br.type == building_type::processing_facility && br.active) ran = true;
            }
            int mines = 0;
            for (const entity_id b : s.w.corporations.at(other).assets)
                if (const auto it = s.w.buildings.find(b);
                    it != s.w.buildings.end() && it->second.type == building_type::extraction_site
                    && it->second.target_resource == resource_type::coal)
                    ++mines;
            return std::make_pair(ran, mines);
        };
        const auto [ran_on, mines_on]   = mines_for(false);
        const auto [ran_off, mines_off] = mines_for(true);
        std::printf("  plant running: ran %s, coal mines %d | plant idled: ran %s, coal mines %d\n",
                    ran_on ? "yes" : "no", mines_on, ran_off ? "yes" : "no", mines_off);
        check(ran_on, "R9 not vacuous: the player's plant runs on its own pool's coal");
        check(mines_on > 0, "R9 a market whose only consumer is a running processor fed from its own pool does not veto a mine");
        check(!ran_off && mines_off == 0, "R9 the same market with the processor idled: no buyer, the mine is vetoed");
    }

    std::printf("\n%s (%d failure%s)\n", g_fail == 0 ? "ALL PASS" : "FAILURES", g_fail,
                g_fail == 1 ? "" : "s");
    return g_fail == 0 ? 0 : 1;
}
