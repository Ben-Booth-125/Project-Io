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

namespace {

int g_fail = 0;
void check(bool ok, const char* what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

constexpr std::size_t r_coal  = static_cast<std::size_t>(resource_type::coal);
constexpr std::size_t r_steel = static_cast<std::size_t>(resource_type::steel);

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
            int first = -1;
            for (int t = 1; t <= 12; ++t)
            {
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

    std::printf("\n%s (%d failure%s)\n", g_fail == 0 ? "ALL PASS" : "FAILURES", g_fail,
                g_fail == 1 ? "" : "s");
    return g_fail == 0 ? 0 : 1;
}
