// Headless harness: construction is measured in DAYS (Ben, 2026-10-24;
// docs/economy/PRODUCTION.md § Construction is measured in days).
//
//   R1  a site's completion day = its first tick + its duration, under full supply
//   R2  a mid-quarter completion's first-quarter output is the prorated share
//   R3  a paused quarter adds no days
//   R4  the headless catch-up (quarters only) and a day-by-day run complete on the
//       same day, and the two worlds hash identically tick for tick
//   R5  the last quarter draws only the days it needs
//   R6  credits up front, materials in arrears (Ben, 2026-10-25, ruling NR-1073): the
//       whole flat cost at placement, one quarter of materials per tick at most, no
//       tick paying credits, and a quarters-only world matching day-by-day
//   R7  a shortfall is carried, drawn first, and claimed ahead of the quarter ahead
//   R8  no over-promise: two sites placed on one shelf in one tick, and two at a tick
//   R9  a site cancelled before completion refunds its credits in full, not materials
//   R10 A DRAW IS A PURCHASE (Ben, 2026-10-26: costs depend on market prices): the
//       materials a tick draws are bought at the shelf's posted price, billed at the
//       clear with a purchase row and recorded on the site, and are the market's demand
//   R11 ONE SITE PER TILE (Ben, 2026-10-26): the door refuses a second site on a tile
//       with one under construction; and the quote is the flat credit the site pays
//       plus its materials at the market's price
//
// Hand-builds a minimal world (no Lua / SDL / ImGui), the construction_gate_harness
// shape: one nation, one market, one extraction site.

#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp" // clear_markets: R10 bills the tick
#include "world/placement_rules.hpp" // k_richness_per_site: R11
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;
void check(bool ok, const char* label, double got = 0.0, double want = 0.0)
{
    std::printf("  [%s] %s  (got %.4f want %.4f)\n", ok ? "PASS" : "FAIL", label, got, want);
    if (!ok) ++g_failures;
}
std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

constexpr int Q = k_construction_quarter_days;

struct scene { world w; entity_id site = null_entity; entity_id market = null_entity; entity_id nation = null_entity; };

// One nation on one tile with an iron deposit, a market there, and an extraction site
// under construction that needs @p days of work (steel 18 over 2 authored quarters).
scene make_scene(float steel_on_shelf, int days)
{
    scene s;
    world& w = s.w;
    const entity_id body = w.create_entity();
    {
        body_component bc{};
        bc.name = "Anvil"; bc.type = body_type::planet; bc.orbital_radius_au = 1.0f;
        bc.grid_width = 4; bc.grid_height = 2; // a real raster: the point routes walk it
        w.bodies[body] = bc;
    }
    entity_id tile = null_entity;
    for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 4; ++c)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body = body; tc.grid_x = c; tc.grid_y = r;
            tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass; tc.cover_density = 150;
            tc.landform = terrain_landform::plains;
            if (c == 0 && r == 0)
            {
                tc.resource_deposit[ri(resource_type::iron_ore)] = 2.0f;
                tc.resource_remaining[ri(resource_type::iron_ore)] = 1.0e6f; // full rate, no taper
                tile = t;
            }
            w.tiles[t] = tc;
        }
    s.market = w.create_entity();
    {
        market_component mc;
        mc.body = body; mc.centre_tile = tile;
        mc.base_price[ri(resource_type::steel)]    = 8.0f;
        mc.base_price[ri(resource_type::iron_ore)] = 2.5f;
        mc.price = mc.base_price;
        mc.supply[ri(resource_type::steel)]    = steel_on_shelf;
        mc.inventory[ri(resource_type::steel)] = steel_on_shelf;
        w.markets[s.market] = mc;
    }
    s.site = w.create_entity();
    {
        building_component b{};
        b.tile = tile; b.type = building_type::extraction_site;
        b.target_resource = resource_type::iron_ore;
        b.workforce_assigned = 1.0f; b.workforce_target = 100;
        b.ai_cooldown = 1000; // the scorer's anti-thrash hold: the dial is not the subject
        b.days_remaining  = days;
        b.ticks_remaining = (days + Q - 1) / Q;
        w.buildings[s.site] = b;
    }
    s.nation = w.create_entity();
    {
        nation_component nc{};
        nc.name = "Builder"; nc.treasury = 100000.0f;
        nc.state_assets.push_back(s.site);
        for (const auto& [tid, tc] : w.tiles) { (void)tc; nc.tiles.push_back(tid); }
        std::sort(nc.tiles.begin(), nc.tiles.end());
        for (const entity_id tid : nc.tiles) w.tile_to_nation[tid] = s.nation;
        w.nations[s.nation] = nc;
        // Points are not the subject: two running road hubs on the site's tile (lp_fixture's
        // shape) keep the nation's pool well clear of the site's need.
        for (int k = 0; k < 2; ++k)
        {
            const entity_id hub = w.create_entity();
            building_component hb{};
            hb.type = building_type::inland_logistics_hub; hb.tile = tile; hb.ticks_remaining = 0;
            w.buildings[hub] = hb;
            w.nations.at(s.nation).state_assets.push_back(hub);
        }
    }
    return s;
}

recipe_registry make_reg()
{
    recipe_registry reg;
    building_economics ex;
    ex.base_rate = 10.0f; ex.maintenance = 0.0f; ex.base_wage = 0.0f;
    ex.build_cost = 180.0f;
    ex.build_duration_ticks = 2.0f; // 180 days
    ex.resource_build_cost[ri(resource_type::steel)] = 18.0f; // 9 a full quarter
    reg.set_economics(building_type::extraction_site, ex);
    construction_params cp; cp.max_stretch = 10.0f; reg.set_construction(cp);
    return reg;
}

float output_of(const economy_report& rep, entity_id b)
{
    for (const building_report& r : rep.buildings)
        if (r.building == b)
            return r.output_quantity;
    return 0.0f;
}

/// Step one quarter. @p daily: advance the day loop 1..90 first, as the app does;
/// otherwise the economy step's own catch-up advances the quarter. Returns (daily
/// only) the absolute day the site completed on, -1 if not this quarter, counting
/// @p quarter quarters already elapsed since the first clearing.
int step_quarter(scene& s, const recipe_registry& reg, bool daily, int quarter, economy_report* out = nullptr)
{
    int done_day = -1;
    const bool open = s.w.buildings.at(s.site).ticks_remaining > 0;
    if (daily)
        for (int d = 1; d <= Q; ++d)
        {
            advance_construction_to(s.w, reg, d);
            if (open && done_day < 0 && s.w.buildings.at(s.site).ticks_remaining == 0)
                done_day = quarter * Q + d;
        }
    economy_report rep = run_economy_step(s.w, reg);
    if (out) *out = rep;
    return done_day;
}

} // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const recipe_registry reg = make_reg();
    std::printf("Construction is measured in days (Ben, 2026-10-24)\n");

    // R1 + R2: a 150-day site under full supply, day by day. Its first clearing is
    // quarter 0's step; it then advances 90 days in quarter 1 and completes on day 60
    // of quarter 2 — absolute day 150 after the first clearing.
    float prorated = 0.0f;
    int   share_pm = -1;
    {
        scene s = make_scene(/*steel=*/1000.0f, /*days=*/150);
        run_economy_step(s.w, reg); // the first clearing: rate set, nothing advanced yet
        check(s.w.buildings.at(s.site).days_remaining == 150,
              "R1 a clearing sets the quarter's rate and advances no day itself",
              s.w.buildings.at(s.site).days_remaining, 150);
        check(s.w.buildings.at(s.site).build_rate_permille == 1000,
              "R1 full supply sets the full rate", s.w.buildings.at(s.site).build_rate_permille, 1000);
        int done = -1;
        for (int q = 0; q < 4 && done < 0; ++q)
            done = step_quarter(s, reg, /*daily=*/true, q);
        check(done == 150, "R1 completion day = first clearing + 150 days", done, 150);
        // The step that closed the completion quarter set the share: 30 of 90 days.
        share_pm = s.w.buildings.at(s.site).output_share_permille;
        check(share_pm == (Q - 60) * 1000 / Q, "R2 the share is the 30 of 90 days it stood complete",
              share_pm, (Q - 60) * 1000 / Q);
    }
    // R2: that step's OUTPUT against a control world whose site stood complete all quarter.
    {
        scene s = make_scene(1000.0f, 150);
        run_economy_step(s.w, reg);
        economy_report rep;
        step_quarter(s, reg, true, 0);
        step_quarter(s, reg, true, 1, &rep); // the clearing that closes the completion quarter
        prorated = output_of(rep, s.site);

        scene c = make_scene(1000.0f, 0);
        c.w.buildings.at(c.site).ticks_remaining = 0;
        const economy_report crep = run_economy_step(c.w, reg);
        const float whole = output_of(crep, c.site);
        std::printf("     whole-quarter output %.4f, completion-quarter output %.4f\n",
                    static_cast<double>(whole), static_cast<double>(prorated));
        check(whole > 0.0f, "R2 control: a whole-quarter site produces", whole, 1.0);
        check(prorated > 0.0f && prorated < whole, "R2 the completion quarter produces, and less than whole",
              prorated, whole);
        const float want = whole * static_cast<float>(share_pm) / 1000.0f;
        check(std::fabs(prorated - want) < 1e-3f * std::max(1.0f, whole),
              "R2 the completion quarter's output is the prorated share of a whole quarter's",
              prorated, want);
        economy_report rep2;
        step_quarter(s, reg, true, 2, &rep2);
        check(std::fabs(output_of(rep2, s.site) - whole) < 1e-3f * std::max(1.0f, whole),
              "R2 from then on it runs whole quarters", output_of(rep2, s.site), whole);
    }

    // R3: a paused quarter (no steel on the shelf) adds no days.
    {
        scene s = make_scene(/*steel=*/0.0f, 150);
        run_economy_step(s.w, reg);
        step_quarter(s, reg, true, 0);
        check(s.w.buildings.at(s.site).days_remaining == 150 && s.w.buildings.at(s.site).build_rate_permille == 0,
              "R3 a paused quarter adds no days", s.w.buildings.at(s.site).days_remaining, 150);
    }
    // R3b: a stretched quarter (half the steel) adds half its days.
    {
        scene s = make_scene(/*steel=*/4.5f, 150);
        run_economy_step(s.w, reg);
        step_quarter(s, reg, true, 0);
        check(s.w.buildings.at(s.site).days_remaining == 150 - 45,
              "R3 a half-supplied quarter adds 45 days", s.w.buildings.at(s.site).days_remaining, 105);
    }

    // R4: headless (quarters only, the economy step's catch-up) against day by day.
    {
        scene a = make_scene(1000.0f, 150);
        scene b = make_scene(1000.0f, 150);
        run_economy_step(a.w, reg);
        run_economy_step(b.w, reg);
        bool same = true;
        int  done_q_a = -1, done_q_b = -1, done_b = -1, share_a = -1, share_b = -1;
        for (int q = 0; q < 5; ++q)
        {
            const bool open_a = a.w.buildings.at(a.site).ticks_remaining > 0;
            step_quarter(a, reg, /*daily=*/false, q);
            if (open_a && a.w.buildings.at(a.site).ticks_remaining == 0)
            { done_q_a = q; share_a = a.w.buildings.at(a.site).output_share_permille; }
            const int d = step_quarter(b, reg, /*daily=*/true, q);
            if (d >= 0) { done_b = d; done_q_b = q; share_b = b.w.buildings.at(b.site).output_share_permille; }
            same = same && a.w.state_hash(q) == b.w.state_hash(q);
        }
        // Same quarter and the same share of it stood complete = the same day.
        check(done_b == 150 && done_q_a == done_q_b && share_a == share_b && share_a >= 0,
              "R4 headless catch-up completes on the day-by-day day (same quarter, same share)",
              share_a, share_b);
        check(same, "R4 the two worlds hash identically after every quarter", same ? 1 : 0, 1);
    }

    // R6 (Ben, 2026-10-25, ruling NR-1073): CREDITS UP FRONT, MATERIALS IN ARREARS.
    // A 180-day site placed on day 10 of a quarter pays its WHOLE flat build cost at
    // placement (180 = build_cost x 180 / 180), builds from that day, and completes on
    // day 190. Each tick draws the materials for the days built since the last one —
    // 80 days (8 steel), then 90 (9), then 10 (1): never more than a quarter, 18 in
    // all — and pays no credits. A quarters-only world placing on the same day
    // completes it on the same day and hashes identically.
    {
        scene a = make_scene(1000.0f, 180); // day by day
        scene h = make_scene(1000.0f, 180); // headless: quarters only
        const float t0 = a.w.nations.at(a.nation).treasury;
        for (scene* sc : {&a, &h})
        {
            building_component& b = sc->w.buildings.at(sc->site);
            b.build_rate_permille = 0; b.build_paid_days = 0;
            advance_construction_to(sc->w, reg, 10);                                  // today is day 10
            begin_construction_on_placement(sc->w, reg, sc->nation, b, /*founding=*/false); // placed today
        }
        const building_component& pb = a.w.buildings.at(a.site);
        check(pb.build_rate_permille == 1000 && pb.build_paid_days == 80 && pb.build_arrears_milli == 0,
              "R6 a placed site builds at once, for the 80 days left in its quarter, owing nothing yet",
              pb.build_paid_days, 80);
        const float credit = t0 - a.w.nations.at(a.nation).treasury;
        check(std::fabs(credit - 180.0f) < 1e-3f && std::fabs(pb.build_credit_paid - 180.0f) < 1e-3f
                  && std::fabs(pb.construction_paid - 180.0f) < 1e-3f,
              "R6 placement pays the WHOLE flat build cost from the treasury, once", credit, 180.0);
        int done = -1;
        float steel = 0.0f, tick_credits = 0.0f;
        float per_tick[4] = {0, 0, 0, 0};
        bool same = true;
        for (int q = 0; q < 4; ++q)
        {
            const bool open = a.w.buildings.at(a.site).ticks_remaining > 0;
            for (int d = (q == 0 ? 11 : 1); d <= Q; ++d)
            {
                advance_construction_to(a.w, reg, d);
                if (open && done < 0 && a.w.buildings.at(a.site).ticks_remaining == 0)
                    done = q * Q + d;
            }
            const economy_report rep = run_economy_step(a.w, reg);
            if (const auto it = rep.purchases.find({a.nation, a.market}); it != rep.purchases.end())
                per_tick[q] = it->second[ri(resource_type::steel)];
            steel += per_tick[q];
            if (const auto lit = a.w.state_ledger.find(a.nation); lit != a.w.state_ledger.end())
                tick_credits += lit->second.construction;
            run_economy_step(h.w, reg);
            same = same && a.w.state_hash(q) == h.w.state_hash(q);
        }
        std::printf("     steel drawn per tick: %.3f %.3f %.3f %.3f\n", per_tick[0], per_tick[1], per_tick[2], per_tick[3]);
        check(done == 190, "R6 placed on day 10, a 180-day site completes on day 190 (placement + duration)",
              done, 190);
        check(std::fabs(per_tick[0] - 8.0f) < 1e-3f && std::fabs(per_tick[1] - 9.0f) < 1e-3f
                  && std::fabs(per_tick[2] - 1.0f) < 1e-3f && per_tick[3] == 0.0f,
              "R6 each tick draws the days built since the last, in arrears (8, 9, 1 steel)", per_tick[0], 8.0);
        bool one_quarter = true;
        for (float x : per_tick) one_quarter = one_quarter && x <= 9.0f + 1e-3f;
        check(one_quarter && std::fabs(steel - 18.0f) < 1e-3f,
              "R6 no tick draws more than one quarter of materials; 18 steel in all", steel, 18.0);
        check(tick_credits == 0.0f, "R6 no economy tick pays construction credits", tick_credits, 0.0);
        check(same && h.w.buildings.at(h.site).ticks_remaining == 0
                  && h.w.buildings.at(h.site).output_share_permille == a.w.buildings.at(a.site).output_share_permille,
              "R6 a quarters-only world completes it on the same day and hashes identically", same ? 1 : 0, 1);
    }

    // R7: A SHORTFALL IS CARRIED AND DRAWN FIRST. Placed on day 0 at full rate, the
    // site builds 90 days (9 steel owed); the shelf then falls to 3. The tick draws 3,
    // and 6 stay owed — claimed ahead of the quarter ahead, so it pauses (the shelf
    // left is short of its own debt). Restocked to 10, the next tick draws the 6 owed
    // FIRST and sets the quarter ahead from the 4 left: rate 4/9.
    {
        scene s = make_scene(1000.0f, 180);
        building_component& b = s.w.buildings.at(s.site);
        b.build_rate_permille = 0; b.build_paid_days = 0;
        begin_construction_on_placement(s.w, reg, s.nation, b, false);
        const std::size_t st = ri(resource_type::steel);
        s.w.markets.at(s.market).inventory[st] = 3.0f;
        economy_report r1 = run_economy_step(s.w, reg);
        const float d1 = r1.purchases[{s.nation, s.market}][st];
        const building_component& b1 = s.w.buildings.at(s.site);
        check(std::fabs(d1 - 3.0f) < 1e-3f && b1.build_arrears_milli == 60000 && b1.build_rate_permille == 0,
              "R7 a short shelf: 3 of 9 drawn, 60 days' materials (6 steel) stay owed, the quarter ahead pauses",
              b1.build_arrears_milli, 60000);
        s.w.markets.at(s.market).inventory[st] = 10.0f;
        economy_report r2 = run_economy_step(s.w, reg);
        const float d2 = r2.purchases[{s.nation, s.market}][st];
        const building_component& b2 = s.w.buildings.at(s.site);
        check(std::fabs(d2 - 6.0f) < 1e-3f && b2.build_arrears_milli == 0,
              "R7 the next tick draws the 6 owed first", d2, 6.0);
        check(b2.build_rate_permille == 444,
              "R7 ...and sets the quarter ahead from what is left after it (4 of 9 = 444 per mille)",
              b2.build_rate_permille, 444);
    }

    // R8: NO OVER-PROMISE. Two sites placed in one tick on a shelf of 13.5 steel (a
    // quarter of one is 9): the first claims 9, the second reads what is left (4.5) and
    // builds at half rate — the claims never exceed the shelf, and the tick draws
    // exactly what was claimed, with nothing short. At a tick, two sites on a short
    // shelf share it pro-rata (BL-1209), again claiming no more than it holds.
    {
        scene s = make_scene(13.5f, 180);
        const entity_id site2 = s.w.create_entity();
        {
            building_component b2 = s.w.buildings.at(s.site);
            s.w.buildings[site2] = b2;
            s.w.nations.at(s.nation).state_assets.push_back(site2);
        }
        for (const entity_id id : {s.site, site2})
        {
            building_component& b = s.w.buildings.at(id);
            b.build_rate_permille = 0; b.build_paid_days = 0;
            begin_construction_on_placement(s.w, reg, s.nation, b, false);
        }
        const std::size_t st = ri(resource_type::steel);
        const int r1 = s.w.buildings.at(s.site).build_rate_permille;
        const int r2 = s.w.buildings.at(site2).build_rate_permille;
        const float claimed = construction_claims_on(s.w, reg, s.market)[st];
        check(r1 == 1000 && r2 == 500, "R8 placement: the second site reads what the first left (1000, 500)", r2, 500);
        check(claimed <= 13.5f + 1e-3f, "R8 a quarter's claims on one shelf never exceed it", claimed, 13.5);
        const economy_report rep = run_economy_step(s.w, reg);
        const auto pit = rep.purchases.find({s.nation, s.market});
        const float drawn = pit != rep.purchases.end() ? pit->second[st] : 0.0f;
        check(std::fabs(drawn - 13.5f) < 1e-3f && s.w.buildings.at(s.site).build_arrears_milli == 0
                  && s.w.buildings.at(site2).build_arrears_milli == 0,
              "R8 the tick draws what was claimed, and nothing is short", drawn, 13.5);
        // The shelf is now empty; restock to 13.5 and let the NEXT tick set both rates.
        s.w.markets.at(s.market).inventory[st] = 13.5f + 0.0f;
        // (no days are built in between: a quarters-only step closes the quarter first)
        const economy_report rep2 = run_economy_step(s.w, reg);
        (void)rep2;
        const int t1 = s.w.buildings.at(s.site).build_rate_permille;
        const int t2 = s.w.buildings.at(site2).build_rate_permille;
        const float claimed2 = construction_claims_on(s.w, reg, s.market)[st];
        const float left2    = s.w.markets.at(s.market).inventory[st];
        std::printf("     tick rates %d %d, claimed %.3f against a shelf of %.3f\n", t1, t2, claimed2, left2);
        check(claimed2 <= left2 + 1e-3f && t1 > 0 && t2 > 0,
              "R8 at a tick, two sites share a short shelf and claim no more than it holds", claimed2, left2);
    }

    // R9 (Ben, 2026-10-25): A SITE CANCELLED BEFORE IT COMPLETES REFUNDS ITS CREDITS IN
    // FULL to the treasury (booked as a refund); the materials it drew are not
    // returned, and what it still owed is cleared, not drawn. A standing building
    // refunds nothing.
    {
        scene s = make_scene(1000.0f, 180);
        building_component& b = s.w.buildings.at(s.site);
        b.build_rate_permille = 0; b.build_paid_days = 0;
        begin_construction_on_placement(s.w, reg, s.nation, b, false);
        run_economy_step(s.w, reg); // 90 days built: 9 steel drawn and billed
        const float paid = s.w.buildings.at(s.site).construction_paid;
        const float before = s.w.nations.at(s.nation).treasury;
        const float inv_before = s.w.markets.at(s.market).inventory[ri(resource_type::steel)];
        const bool gone = demolish_building(s.w, s.nation, s.site);
        const float back = s.w.nations.at(s.nation).treasury - before;
        check(gone && std::fabs(back - 180.0f) < 1e-3f
                  && std::fabs(s.w.nations.at(s.nation).refund_unbooked - 180.0f) < 1e-3f,
              "R9 a cancelled site refunds the whole 180 credits paid at placement, booked as a refund", back, 180.0);
        check(paid > 180.0f + 1e-3f && s.w.markets.at(s.market).inventory[ri(resource_type::steel)] == inv_before,
              "R9 the materials it drew are not returned (paid more than its credits; the shelf unchanged)", paid, 180.0);

        scene c = make_scene(1000.0f, 0);
        c.w.buildings.at(c.site).ticks_remaining = 0;
        const float cb = c.w.nations.at(c.nation).treasury;
        demolish_building(c.w, c.nation, c.site);
        check(c.w.nations.at(c.nation).treasury == cb, "R9 a standing building refunds nothing",
              c.w.nations.at(c.nation).treasury - cb, 0.0);
    }

    // R5: the last quarter draws only the days it needs. 150 days at full supply:
    // a full quarter (9 steel), then 60 days (6 steel) — 15 = 18 x 150 / 180.
    {
        scene s = make_scene(1000.0f, 150);
        float steel = 0.0f;
        for (int q = 0; q < 3; ++q)
        {
            const economy_report rep = run_economy_step(s.w, reg);
            if (const auto it = rep.purchases.find({s.nation, s.market}); it != rep.purchases.end())
                steel += it->second[ri(resource_type::steel)];
        }
        check(std::fabs(steel - 15.0f) < 1e-3f, "R5 the whole build draws 18 x 150 / 180 steel", steel, 15.0);
        // And construction_capex prices the same days.
        const entity_id tile = s.w.buildings.at(s.site).tile;
        check(construction_build_days(s.w, reg, tile, building_type::extraction_site, resource_type::iron_ore) >= 1,
              "R5 a durationed type takes at least one day", 1, 1);
    }

    // R10 (Ben, 2026-10-26: "it's better with costs depending on market prices"). A
    // DRAW IS A PURCHASE. A site placed on day 0 builds 90 days; the next tick draws
    // their 9 steel off the shelf AS A PURCHASE: the register holds them, the clear
    // bills the nation the steel at the shelf's posted price and writes the purchase
    // row at it, the site's record adds the same credits, and the 9 steel are the
    // market's demand, so the price stands above a control world's with no site.
    {
        scene s = make_scene(1000.0f, 180);
        building_component& b = s.w.buildings.at(s.site);
        b.build_rate_permille = 0; b.build_paid_days = 0;
        begin_construction_on_placement(s.w, reg, s.nation, b, false);
        const std::size_t st = ri(resource_type::steel);
        const float paid0  = s.w.buildings.at(s.site).construction_paid;
        const float shelf0 = s.w.markets.at(s.market).inventory[st];
        economy_report rep = run_economy_step(s.w, reg);
        const float drawn = shelf0 - s.w.markets.at(s.market).inventory[st];
        const auto pit = rep.purchases.find({s.nation, s.market});
        const float bought = pit != rep.purchases.end() ? pit->second[st] : 0.0f;
        const float px = posted_price(s.w.markets.at(s.market), st);
        const float recorded = s.w.buildings.at(s.site).construction_paid - paid0;
        const auto flows = clear_markets(s.w, reg, rep);
        const auto fit = flows.find(s.nation);
        const float spent = fit != flows.end() ? fit->second.expenditure : 0.0f;
        bool purchase_row = false;
        for (const exchange_record& e : s.w.exchanges.entries)
            if (e.side == 1 && e.resource == resource_type::steel
                && std::fabs(e.quantity - 9.0f) < 1e-3f && e.unit_price == px)
                purchase_row = true;
        std::printf("     drew %.4f, bought %.4f at posted %.4f; billed %.4f, recorded %.4f\n",
                    drawn, bought, px, spent, recorded);
        check(std::fabs(drawn - 9.0f) < 1e-3f && bought == drawn,
              "R10 the tick drew the 90 days' 9 steel off the shelf, every unit a purchase", bought, 9.0);
        check(px > 0.0f && spent == bought * px,
              "R10 the clear bills the draw at the shelf's posted price, exactly", spent, bought * px);
        check(purchase_row, "R10 ...and writes the purchase row at that price", purchase_row ? 1 : 0, 1);
        check(std::fabs(recorded - spent) < 1e-3f,
              "R10 the site's record adds what the draw was billed", recorded, spent);
        const float demand = s.w.markets.at(s.market).demand[st];
        check(demand >= 9.0f - 1e-3f, "R10 the draw is the market's demand (the clearing sees it)", demand, 9.0);

        scene c = make_scene(1000.0f, 0); // control: the same world with the site standing
        c.w.buildings.at(c.site).ticks_remaining = 0;
        economy_report crep = run_economy_step(c.w, reg);
        clear_markets(c.w, reg, crep);
        const float p_site = s.w.markets.at(s.market).price[st];
        const float p_ctrl = c.w.markets.at(c.market).price[st];
        std::printf("     steel demand %.3f (control %.3f); price %.4f (control %.4f)\n", demand,
                    c.w.markets.at(c.market).demand[st], p_site, p_ctrl);
        check(c.w.markets.at(c.market).demand[st] == 0.0f && p_site > p_ctrl,
              "R10 ...and the price answers it: above the control world's, which has no site", p_site, p_ctrl);
    }

    // R11 (Ben, 2026-10-26): ONE SITE PER TILE, through the build door. "To slow the pace
    // of the game down, we need to add a limit so that tiles can only construct one
    // building at a time." A roaded tile of the nation's own ground holds a site under
    // construction: a second building of ANY type is refused site_busy and nothing
    // moves. Once the site completes the tile takes its next site; placement takes the
    // flat credit, and the door's quote is that credit plus the materials at the
    // market's price (costs depend on market prices, Ben, 2026-10-26).
    {
        scene s = make_scene(1000.0f, 180);
        const entity_id tile = s.w.buildings.at(s.site).tile;
        s.w.tiles.at(tile).road_edges = road_edges_on_every_side(1); // a building needs a road
        // A deposit rich enough for a stack of sites, so the stack cap is not the refusal.
        s.w.tiles.at(tile).resource_deposit[ri(resource_type::iron_ore)] = 4.0f * placement_rules::k_richness_per_site;
        const float t0 = s.w.nations.at(s.nation).treasury;
        const std::size_t n0 = s.w.buildings.size();
        entity_id built = null_entity;
        const construction_result r_same = construct_nation(s.w, reg, s.nation, tile,
            building_type::extraction_site, resource_type::iron_ore, no_recipe, built);
        const construction_result r_other = construct_nation(s.w, reg, s.nation, tile,
            building_type::processing_facility, resource_type::iron_ore, no_recipe, built);
        check(r_same == construction_result::site_busy && r_other == construction_result::site_busy,
              "R11 a second site on a tile with one under construction is refused site_busy, whatever its type",
              static_cast<double>(r_other), static_cast<double>(construction_result::site_busy));
        check(built == null_entity && s.w.buildings.size() == n0 && s.w.nations.at(s.nation).treasury == t0,
              "R11 the refusal mutates nothing", s.w.nations.at(s.nation).treasury, t0);

        building_component& b = s.w.buildings.at(s.site);
        b.ticks_remaining = 0; b.days_remaining = 0; // the site completes
        const float quote = construction_capex(s.w, reg, tile, building_type::extraction_site,
                                               resource_type::iron_ore);
        const float t1 = s.w.nations.at(s.nation).treasury;
        const construction_result r_next = construct_nation(s.w, reg, s.nation, tile,
            building_type::extraction_site, resource_type::iron_ore, no_recipe, built);
        const float paid = t1 - s.w.nations.at(s.nation).treasury;
        std::printf("     quoted %.4f, paid at placement %.4f\n", quote, paid);
        check(r_next == construction_result::placed && built != null_entity,
              "R11 once the site completes, the tile takes its next site", static_cast<double>(r_next), 0.0);
        if (built != null_entity)
        {
            const building_component& nb = s.w.buildings.at(built);
            const float days = static_cast<float>(nb.days_remaining);
            const float flat = 180.0f * days / 180.0f;
            const float mats = 18.0f * days / 180.0f * 8.0f; // steel at the market's price, 8
            check(paid == nb.build_credit_paid && paid == construction_credit_cost(reg, nb)
                      && std::fabs(paid - flat) < 1e-3f,
                  "R11 placement takes the flat credit, build_cost x the site's days / 180", paid, flat);
            check(std::fabs(quote - (flat + mats)) < 1e-3f && quote > paid,
                  "R11 the quote is that credit plus the materials at the market's price", quote, flat + mats);
        }
        entity_id third = null_entity;
        check(construct_nation(s.w, reg, s.nation, tile, building_type::extraction_site,
                               resource_type::iron_ore, no_recipe, third) == construction_result::site_busy,
              "R11 ...and that new site blocks the tile in turn", 0.0, 0.0);
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
