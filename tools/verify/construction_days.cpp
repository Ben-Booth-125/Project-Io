// Headless harness: construction is measured in DAYS (Ben, 2026-10-24;
// docs/economy/PRODUCTION.md § Construction is measured in days).
//
//   R1  a site's completion day = its first clearing + its duration, under full supply
//   R2  a mid-quarter completion's first-quarter output is the prorated share
//   R3  a paused quarter adds no days
//   R4  the headless catch-up (quarters only) and a day-by-day run complete on the
//       same day, and the two worlds hash identically tick for tick
//   R5  the last quarter draws only the days it needs
//
// Hand-builds a minimal world (no Lua / SDL / ImGui), the construction_gate_harness
// shape: one nation, one market, one extraction site.

#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/economy_system.hpp"
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

    // R6 (Ben, 2026-10-25): construction STARTS ON THE PLACEMENT DAY. A 180-day site
    // placed on day 10 of a quarter builds from that day on credit and completes on
    // day 190 (day 10 of the quarter after next); the first clearing pays the 80 days
    // built since placement plus the quarter ahead (17 steel), the next the last 10
    // days (1 steel) — the whole 18. A quarters-only world placing on the same day
    // completes it on the same day and hashes identically.
    {
        scene a = make_scene(1000.0f, 180); // day by day
        scene h = make_scene(1000.0f, 180); // headless: quarters only
        for (scene* sc : {&a, &h})
        {
            building_component& b = sc->w.buildings.at(sc->site);
            b.build_rate_permille = 0; b.build_paid_days = 0;
            advance_construction_to(sc->w, reg, 10);           // today is day 10
            begin_construction_on_placement(sc->w, reg, b);    // placed today
        }
        check(a.w.buildings.at(a.site).build_on_credit && a.w.buildings.at(a.site).build_rate_permille == 1000
                  && a.w.buildings.at(a.site).build_paid_days == 80,
              "R6 a placed site builds on credit at once, for the 80 days left in its quarter",
              a.w.buildings.at(a.site).build_paid_days, 80);
        int done = -1;
        float steel = 0.0f;
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
                steel += it->second[ri(resource_type::steel)];
            run_economy_step(h.w, reg);
            same = same && a.w.state_hash(q) == h.w.state_hash(q);
        }
        check(done == 190, "R6 placed on day 10, a 180-day site completes on day 190 (placement + duration)",
              done, 190);
        check(std::fabs(steel - 18.0f) < 1e-3f,
              "R6 the clearings pay every day built, arrears included (18 steel in all)", steel, 18.0);
        check(same && h.w.buildings.at(h.site).ticks_remaining == 0
                  && h.w.buildings.at(h.site).output_share_permille == a.w.buildings.at(a.site).output_share_permille,
              "R6 a quarters-only world completes it on the same day and hashes identically", same ? 1 : 0, 1);
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

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
