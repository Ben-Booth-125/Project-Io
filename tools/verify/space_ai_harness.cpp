// ---------------------------------------------------------------------------
// space_ai_harness — BL-1589 (rival space AI), the headless proof
// ---------------------------------------------------------------------------
// docs/ai/AI_OPPONENT.md § 11, "Rival nations go to space as the player does"
// (Ben, 2026-11-10); docs/economy/SPACE_ASSETS.md § Launching, § Satellites,
// § Space stations. A hand-built fixture: a star and a home planet at 1.0 AU,
// three nations on it — two rivals and the player's — each staged to launch
// (space_fixture.hpp: every Digitisation node held, a staffed Launchpad, a
// stocked home shelf). Rows:
//
//   P1  LAUNCH: a keyed, funded rival's first pick is a comms satellite to its
//       home low orbit, applied through the seam: one asset, role comms, the
//       treasury debited exactly the pick's quoted cost.
//   P2  ORDER: then a station core, then modules in the target layout's order.
//   P3  ONE A QUARTER: over twenty quarters no rival gains more than one asset
//       (satellite, core or module) in any quarter, and every acting pick
//       names exactly one asset gained.
//   P4  NEVER THE PLAYER: the player's nation, keyed, funded and padded, owns
//       nothing in space, its treasury and pool untouched, and no pick row
//       names it.
//   P5  NO KEYS: a rival holding no node takes no action (no_rocketry), and the
//       world hashes exactly as before the pass.
//   P6  NO PAD / RESERVE / CREW: each wall is reported and touches nothing.
//   P7  SETTLE: before the research age opens the pass does nothing.
//   P8  DETERMINISM: two fixtures driven identically end with equal state_hash
//       and byte-identical snapshots.
//   P9  THROUGH THE STEP: run_nation_step files the picks on economy_report.
//
// Build: node tools/verify/build_harness.js space_ai_harness --run
// ---------------------------------------------------------------------------

#include "world/campaign_settle.hpp"
#include "world/economy_system.hpp"
#include "world/nation_ai.hpp"
#include "world/nation_step.hpp"
#include "world/recipe_registry.hpp"
#include "world/space_assets.hpp"
#include "world/space_command.hpp"
#include "world/stations.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include "space_fixture.hpp"

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
int g_pass = 0;

void check(bool ok, const char* row, const std::string& what)
{
    std::printf("  [%s] %s %s\n", ok ? "PASS" : "FAIL", row, what.c_str());
    if (ok) ++g_pass; else ++g_fail;
}

std::string fmt(const char* f, ...)
{
    char buf[600];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

std::string to_bytes(const world& w)
{
    std::ostringstream os(std::ios::binary);
    write_world_snapshot(w, os);
    return os.str();
}

struct fixture
{
    world     w;
    entity_id star = null_entity, home = null_entity;
    entity_id a = null_entity, b = null_entity, player = null_entity;
    entity_id market = null_entity;
};

entity_id add_body(world& w, const char* name, body_type t, entity_id parent, float au)
{
    const entity_id id = w.create_entity();
    body_component bc;
    bc.name = name;
    bc.type = t;
    bc.parent = parent;
    bc.orbital_radius_au = au;
    bc.orbital_angle_rad = 0.0f;
    bc.grid_width = 0;
    bc.grid_height = 0;
    bc.survey = {};
    w.bodies.emplace(id, bc);
    return id;
}

void build(fixture& f, bool keys = true)
{
    world& w = f.w;
    f.star = add_body(w, "Star", body_type::star, null_entity, 0.0f);
    f.home = add_body(w, "Home", body_type::planet, null_entity, 1.0f);
    w.star_body = f.star;
    w.home_body = f.home;
    const char* names[3] = {"Alpha", "Beta", "Seat"};
    entity_id* ids[3] = {&f.a, &f.b, &f.player};
    const entity_id tile = space_fixture::add_tile(w, f.home);
    for (int i = 0; i < 3; ++i)
    {
        *ids[i] = w.create_entity();
        nation_component nc;
        nc.name = names[i];
        nc.treasury = 2000000.0f;
        nc.astronauts = 20;
        w.nations.emplace(*ids[i], nc);
        if (keys)
            space_fixture::hold_every_node(w, *ids[i]);
        space_fixture::add_pad(w, *ids[i], tile, 10.0f);
    }
    w.player_entity = f.player;
    f.market = space_fixture::add_shelf(w, f.home);
    w.current_econ_tick = k_campaign_settle_ticks; // the research age is open
}

/// Assets (satellites, cores, modules) a nation owns, lost or not.
int assets_of_nation(const world& w, entity_id n)
{
    int c = 0;
    for (const auto& [id, a] : w.space_assets)
        if (a.owner == n)
            ++c;
    return c;
}

/// One quarter, in the nation step's order: the pick, the queue, the assets, the stations.
std::vector<space_ai_pick> quarter(world& w, const recipe_registry& reg, int tick)
{
    w.current_econ_tick = tick;
    std::vector<space_ai_pick> picks;
    run_nation_space_ai(w, reg, &picks);
    run_space_orders(w, reg, tick);
    run_space_asset_step(w, reg.space_assets(), tick);
    run_station_step(w, reg.stations(), tick);
    return picks;
}

const space_ai_pick* pick_for(const std::vector<space_ai_pick>& v, entity_id n)
{
    for (const space_ai_pick& p : v)
        if (p.nation == n)
            return &p;
    return nullptr;
}

/// The scripted programme (P8 runs it twice): twenty quarters.
void programme(fixture& f, const recipe_registry& reg)
{
    for (int q = 0; q < 20; ++q)
        quarter(f.w, reg, k_campaign_settle_ticks + q);
}

} // namespace

int main()
{
    const recipe_registry reg;
    const nation_ai_params& p = reg.nation_ai();
    std::printf("space_ai_harness — BL-1589 (rival space AI)\n");
    std::printf("dials: reserve %.2f of treasury + %d quarters of space upkeep; key relief %.2f\n",
                p.space_reserve_share, p.space_reserve_upkeep_quarters, p.research_space_key_relief);

    // ---- P1 / P2 / P3 / P4 ----------------------------------------------------
    {
        fixture f;
        build(f);
        world& w = f.w;
        const float ta0 = w.nations.at(f.a).treasury;
        const float tp0 = w.nations.at(f.player).treasury;
        const int32_t pp0 = w.nations.at(f.player).astronauts;

        const space_ai_pick pre = choose_space_action(w, reg, f.a, p);
        std::vector<space_ai_pick> q0 = quarter(w, reg, k_campaign_settle_ticks);
        const space_ai_pick* a0 = pick_for(q0, f.a);
        bool comms = false;
        for (const auto& [id, a] : w.space_assets)
            if (a.owner == f.a && a.cls == space_asset_class::satellite && a.role == space_satellite_role::comms
                && a.node.body == f.home && a.node.kind == orbital_node_kind::low_orbit)
                comms = true;
        const float sat_upkeep = reg.space_assets().upkeep[0];
        check(a0 != nullptr && a0->action == space_ai_action::comms_satellite
                  && a0->result == space_command_result::applied && comms
                  && assets_of_nation(w, f.a) == 1,
              "P1", "a keyed, funded rival's first pick is a comms satellite to home low orbit, applied");
        // The quarter's step paid the satellite's first upkeep (travel 0: active now).
        check(a0 != nullptr && pre.cost > 0.0f && pre.cost == a0->cost
                  && std::fabs(w.nations.at(f.a).treasury - (ta0 - a0->cost - sat_upkeep)) < 1.0f,
              "P1", fmt("treasury debited the quote %.0f (+ upkeep %.0f): %.0f -> %.0f",
                        a0 ? a0->cost : 0.0f, sat_upkeep, ta0, w.nations.at(f.a).treasury));

        std::vector<space_ai_pick> q1 = quarter(w, reg, k_campaign_settle_ticks + 1);
        const space_ai_pick* a1 = pick_for(q1, f.a);
        check(a1 != nullptr && a1->action == space_ai_action::station_core
                  && a1->result == space_command_result::applied && w.stations.size() >= 1,
              "P2", "the second quarter founds a station core");

        std::vector<uint8_t> fitted;
        bool one_a_quarter = true, acted_means_one = true;
        int prev_a = assets_of_nation(w, f.a), prev_b = assets_of_nation(w, f.b);
        bool player_row = pick_for(q0, f.player) || pick_for(q1, f.player);
        for (int q = 2; q < 20; ++q)
        {
            std::vector<space_ai_pick> qq = quarter(w, reg, k_campaign_settle_ticks + q);
            player_row |= pick_for(qq, f.player) != nullptr;
            const int na = assets_of_nation(w, f.a), nb = assets_of_nation(w, f.b);
            const space_ai_pick* pa = pick_for(qq, f.a);
            const space_ai_pick* pb = pick_for(qq, f.b);
            one_a_quarter &= (na - prev_a) <= 1 && (nb - prev_b) <= 1;
            if (pa && pa->action != space_ai_action::none)
                acted_means_one &= (na - prev_a) == 1;
            if (pb && pb->action != space_ai_action::none)
                acted_means_one &= (nb - prev_b) == 1;
            if (pa && pa->action == space_ai_action::station_module)
                fitted.push_back(pa->module);
            prev_a = na;
            prev_b = nb;
        }
        // The first four fits follow the target's head: power, research, habitation, life support.
        const bool order_ok = fitted.size() >= 4
            && fitted[0] == static_cast<uint8_t>(station_module_kind::power)
            && fitted[1] == static_cast<uint8_t>(station_module_kind::research)
            && fitted[2] == static_cast<uint8_t>(station_module_kind::habitation)
            && fitted[3] == static_cast<uint8_t>(station_module_kind::life_support);
        check(order_ok, "P2", fmt("modules follow the target layout (%zu fitted; first %d %d %d %d)",
                                   fitted.size(), fitted.size() > 0 ? fitted[0] : -1,
                                   fitted.size() > 1 ? fitted[1] : -1, fitted.size() > 2 ? fitted[2] : -1,
                                   fitted.size() > 3 ? fitted[3] : -1));
        check(one_a_quarter && acted_means_one, "P3",
              "no rival gains more than one asset in a quarter; an acting pick gains exactly one");
        // Station budget: every fitted module works (the cover check held).
        int stopped = 0, working = 0;
        for (const auto& [sid, st] : w.stations)
        {
            const station_budget bud = evaluate_station_budget(w, reg.stations(), sid, w.current_econ_tick);
            stopped += static_cast<int>(bud.stopped.size());
            working += static_cast<int>(bud.working.size());
        }
        check(stopped == 0 && working > 0, "P2",
              fmt("the cover check holds: %d modules working, %d stopped by a shortfall", working, stopped));
        check(assets_of_nation(w, f.player) == 0 && w.nations.at(f.player).treasury == tp0
                  && w.nations.at(f.player).astronauts == pp0 && !player_row,
              "P4", "the player's nation owns nothing in space, its treasury and pool untouched, no pick row");
        std::printf("    rival A ended with %d assets, treasury %.0f, pool %d\n", assets_of_nation(w, f.a),
                    w.nations.at(f.a).treasury, w.nations.at(f.a).astronauts);
    }

    // ---- P5 no keys -------------------------------------------------------------
    {
        fixture f;
        build(f, /*keys=*/false);
        const std::uint64_t h0 = f.w.state_hash(0);
        std::vector<space_ai_pick> picks;
        const int issued = run_nation_space_ai(f.w, reg, &picks);
        bool all_blocked = !picks.empty();
        for (const space_ai_pick& k : picks)
            all_blocked &= k.action == space_ai_action::none && k.block == space_ai_block::no_rocketry;
        check(issued == 0 && all_blocked && f.w.space_assets.empty() && f.w.state_hash(0) == h0,
              "P5", "with no keys nothing happens: no_rocketry, no asset, hash unchanged");
    }

    // ---- P6 the walls -------------------------------------------------------------
    {
        // No pad: the rival's pad goes unstaffed.
        fixture f;
        build(f);
        for (auto& [bid, b] : f.w.buildings)
            b.workforce_assigned = 0.0f;
        const std::uint64_t h0 = f.w.state_hash(0);
        const space_ai_pick k = choose_space_action(f.w, reg, f.a, p);
        run_nation_space_ai(f.w, reg);
        check(k.block == space_ai_block::no_launchpad && f.w.space_assets.empty() && f.w.state_hash(0) == h0,
              "P6", "no staffed pad: no_launchpad, nothing touched");
    }
    {
        // Reserve: a treasury that covers the quote but not the quote plus its share.
        fixture f;
        build(f);
        const space_ai_pick full = choose_space_action(f.w, reg, f.a, p);
        f.w.nations.at(f.a).treasury = full.cost * 1.5f;
        const std::uint64_t h0 = f.w.state_hash(0);
        const float t0 = f.w.nations.at(f.a).treasury;
        std::vector<space_ai_pick> picks;
        run_nation_space_ai(f.w, reg, &picks);
        const space_ai_pick* k = pick_for(picks, f.a);
        check(k && k->action == space_ai_action::none && k->block == space_ai_block::reserve
                  && f.w.nations.at(f.a).treasury == t0 && assets_of_nation(f.w, f.a) == 0,
              "P6", fmt("inside the reserve: reserve, nothing spent (treasury %.0f, quote %.0f)", t0, full.cost));
        (void)h0;
    }
    {
        // Crew: the comms satellite goes, then an empty pool founds nothing.
        fixture f;
        build(f);
        f.w.nations.at(f.a).astronauts = 0;
        quarter(f.w, reg, k_campaign_settle_ticks);
        const std::uint64_t h0 = f.w.state_hash(0);
        const std::size_t n0 = f.w.space_assets.size();
        const space_ai_pick k = choose_space_action(f.w, reg, f.a, p);
        check(k.action == space_ai_action::none && k.block == space_ai_block::crew_short
                  && f.w.state_hash(0) == h0 && f.w.space_assets.size() == n0,
              "P6", "an empty astronaut pool founds no station: crew_short, nothing touched");
    }
    {
        // A pending order waits: a fit queued outside coverage blocks the next pick.
        fixture f;
        build(f);
        space_order o;
        o.id = f.w.next_space_order_id++;
        o.due_tick = f.w.current_econ_tick + 1;
        o.cmd.nation = f.a;
        o.cmd.verb = static_cast<uint8_t>(space_verb::delete_template);
        o.cmd.name = "x";
        f.w.space_orders.push_back(o);
        const space_ai_pick k = choose_space_action(f.w, reg, f.a, p);
        check(k.action == space_ai_action::none && k.block == space_ai_block::pending_order,
              "P6", "a pending near-space order of its own: pending_order, no new pick");
    }

    // ---- P7 settle ------------------------------------------------------------------
    {
        fixture f;
        build(f);
        f.w.current_econ_tick = k_campaign_settle_ticks - 1;
        const std::uint64_t h0 = f.w.state_hash(0);
        std::vector<space_ai_pick> picks;
        const int issued = run_nation_space_ai(f.w, reg, &picks);
        check(issued == 0 && picks.empty() && f.w.state_hash(0) == h0, "P7",
              "in the settle the pass does nothing at all");
    }

    // ---- P8 determinism -----------------------------------------------------------------
    {
        fixture f1, f2;
        build(f1);
        build(f2);
        programme(f1, reg);
        programme(f2, reg);
        const std::uint64_t h1 = f1.w.state_hash(0), h2 = f2.w.state_hash(0);
        check(h1 == h2 && to_bytes(f1.w) == to_bytes(f2.w) && !f1.w.space_assets.empty(), "P8",
              fmt("two runs end equal: hash %016llX, %zu assets, snapshots byte-identical",
                  static_cast<unsigned long long>(h1), f1.w.space_assets.size()));
    }

    // ---- P9 through the step -----------------------------------------------------------
    {
        fixture f;
        build(f);
        economy_report rep;
        run_nation_step(f.w, reg, rep, k_campaign_settle_ticks);
        const space_ai_pick* k = pick_for(rep.space_picks, f.a);
        check(k != nullptr && k->action == space_ai_action::comms_satellite && pick_for(rep.space_picks, f.player) == nullptr
                  && rep.space_assets.launched >= 1,
              "P9", fmt("run_nation_step files the picks (%zu rows) and the launch reaches the asset step (%d launched)",
                        rep.space_picks.size(), rep.space_assets.launched));
    }

    std::printf("\nspace_ai_harness: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
