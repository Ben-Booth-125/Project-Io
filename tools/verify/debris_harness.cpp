// ---------------------------------------------------------------------------
// debris_harness — BL-1598 (debris), the proof
// ---------------------------------------------------------------------------
// docs/economy/SPACE_ASSETS.md § Debris; docs/military/SPACE_FLEETS.md § Battles
// (the stall). The space_battle_harness fixture: a star, a home planet, its moon, a
// far planet; one market each on home and moon; nations A and B holding every key,
// each with a staffed home Launchpad. Units are RAISED and satellites LAUNCHED by the
// verbs; debris levels are staged directly where a row needs a known level. Rows:
//
//   D1  BATTLE LOSS: two A frigates destroy B's frigate at home low orbit; the debris
//       day deposits exactly the battle day's mass destroyed at that node, and only there.
//   D2  WEAR SCALES: a satellite at a level L and one at 2L gain exactly wear_per_debris
//       x L and x 2L in a quarter; a satellite at a clean node gains none.
//   D3  UNIT DAMAGE SCALES: an idle frigate at L loses unit_damage_per_debris x L a day,
//       at 2L twice that; a unit in transit takes nothing that day.
//   D4  PASSING THROUGH: a frigate flown home low -> moon low through a fouled node
//       takes one day's damage there on arrival (and none for the clean ends).
//   D5  CASCADE: four satellites at one high orbit (level 20); with one already near worn out, its
//       loss adds satellite_mass to the level and the next quarter's wear on the others
//       is larger; they are all lost a quarter sooner than without it. A satellite lost
//       to upkeep (the watch) leaves satellite_mass.
//   D6  DRAG: one quarter takes low_orbit_drag x the level off low orbit; high orbit
//       and an L-point keep theirs; a level drawn under the floor is erased.
//   D7  TUG: an idle Tug at a fouled low orbit removes tug_clear_per_quarter before drag;
//       a level smaller than that is cleared whole and erased; a hauler clears nothing.
//   D8  THE STALL: two composite-armoured frigates (fire 10 < armour 15) fight
//       stall_days days landing nothing and the battle closes a STANDOFF, both holding;
//       it does not reopen while neither can land; a second A frigate (pool 20) re-opens
//       it; stall_days 0 keeps a quiet battle open.
//   D9  SAVE: debris at two nodes, a worn satellite and a quiet open battle round-trip
//       byte-identical with equal hash; the v87 prefix + the section == the stream; a
//       debris-free world's v87 prefix reads clean. A standoff record round-trips.
//   D10 TAMPERED: NaN / floor / over-max level, a surface node, nodes out of order, wear
//       >= 1 or on a ghost, quiet days on a ghost battle or zero, truncation — each refused
//       whole, the target untouched.
//   D11 DETERMINISM: two fixtures driven identically end with equal hash and stream.
//
// Build: node tools/verify/build_harness.js debris_harness --run
// ---------------------------------------------------------------------------

#include "world/debris.hpp"
#include "world/digitisation_tree_data.hpp"
#include "world/early_space_tree_data.hpp"
#include "world/nation_tech.hpp"
#include "world/orbital_graph.hpp"
#include "world/space_assets.hpp"
#include "world/space_battle.hpp"
#include "world/space_tech_keys.hpp"
#include "world/space_units.hpp"
#include "world/stance.hpp"
#include "world/stations.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

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
    char buf[900];
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

bool from_bytes(const std::string& b, world& w)
{
    std::istringstream is(b, std::ios::binary);
    return read_world_snapshot(w, is);
}

bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps * std::max(1.0f, std::fabs(b)); }

constexpr std::size_t PR = static_cast<std::size_t>(resource_type::propellant);
constexpr std::size_t SC = static_cast<std::size_t>(resource_type::spacecraft_components);

struct fixture
{
    world     w;
    entity_id star = null_entity, home = null_entity, moon = null_entity, far = null_entity;
    entity_id a = null_entity, b = null_entity;
    entity_id home_tile = null_entity, moon_tile = null_entity;
    entity_id home_mkt = null_entity, moon_mkt = null_entity;
    orbital_node low{}, high{}, moon_low{}, l4{};
};

entity_id add_body(world& w, const char* name, body_type t, entity_id parent, float au, float mass,
                   atmosphere_class atm)
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
    bc.mass_earths = mass;
    bc.atmosphere = atm;
    w.bodies.emplace(id, bc);
    return id;
}

entity_id add_tile(world& w, entity_id body)
{
    const entity_id id = w.create_entity();
    tile_component tc{};
    tc.body = body;
    tc.grid_x = 0;
    tc.grid_y = 0;
    w.tiles.emplace(id, tc);
    return id;
}

entity_id add_market(world& w, entity_id body, entity_id centre)
{
    const entity_id id = w.create_entity();
    market_component m{};
    m.body = body;
    m.centre_tile = centre;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        m.base_price[r] = 50.0f;
        m.price[r] = 0.0f; // posted = base
        m.inventory[r] = 20000.0f;
    }
    m.base_price[PR] = 64.0f;
    m.base_price[SC] = 310.0f;
    w.markets.emplace(id, m);
    return id;
}

entity_id add_pad(world& w, entity_id nation, entity_id tile)
{
    const entity_id bid = w.create_entity();
    building_component b{};
    b.tile = tile;
    b.type = building_type::launchpad;
    b.workforce_assigned = 1.0f;
    b.ticks_remaining = 0;
    w.buildings.emplace(bid, b);
    w.nations.at(nation).state_assets.push_back(bid);
    return bid;
}

void grant_keys(world& w, entity_id nation)
{
    nation_tech_state& st = w.nation_tech[nation];
    for (const io::es::effect& e : io::es::effects)
        if (e.kind == io::es::effect_kind::mechanic && e.node >= 0 && e.node < io::es::node_count)
            st.es_held.set(static_cast<std::size_t>(e.node));
    auto& held = st.held;
    held.set();
    for (int i = 0; i < io::dg::node_count; ++i)
    {
        const int ex = io::dg::nodes[static_cast<std::size_t>(i)].excludes;
        if (ex < 0 || !held.test(static_cast<std::size_t>(i)) || !held.test(static_cast<std::size_t>(ex)))
            continue;
        bool keyed = false;
        for (const std::string_view k : space_keys::near_space)
            keyed = keyed || node_has_mechanic(i, k);
        held.reset(static_cast<std::size_t>(keyed ? ex : i));
    }
}

using sk = space_unit_slot;

space_design design_of(std::initializer_list<std::pair<sk, const char*>> items)
{
    space_design d;
    d.fill(k_no_space_item);
    const auto& cat = space_item_catalogue();
    for (const auto& [slot, id] : items)
        for (std::size_t i = 0; i < cat.size(); ++i)
            if (std::strcmp(cat[i].id, id) == 0)
                d[static_cast<std::size_t>(slot)] = static_cast<uint8_t>(i);
    return d;
}

const space_design& hauler_d()
{
    static const space_design d = design_of({{sk::drive, "drive.chemical"}, {sk::tanks, "tanks.extended"}, {sk::bay, "bay.cargo"}});
    return d;
}
const space_design& tug_d()
{
    static const space_design d = design_of({{sk::drive, "drive.chemical"}, {sk::tanks, "tanks.standard"}, {sk::debris_arm, "arm.capture"}});
    return d;
}
const space_design& frigate_d()
{
    static const space_design d = design_of({{sk::drive, "drive.nuclear_thermal"}, {sk::tanks, "tanks.extended"},
                                             {sk::armament, "armament.kinetic"}, {sk::armour, "armour.whipple"}});
    return d;
}
const space_design& frigate_heavy_d()
{
    static const space_design d = design_of({{sk::drive, "drive.nuclear_thermal"}, {sk::tanks, "tanks.extended"},
                                             {sk::armament, "armament.kinetic"}, {sk::armour, "armour.composite"}});
    return d;
}

void build(fixture& f, bool heavy = false)
{
    world& w = f.w;
    f.star = add_body(w, "Star", body_type::star, null_entity, 0.0f, 1.0f, atmosphere_class::none);
    f.home = add_body(w, "Home", body_type::planet, null_entity, 1.0f, 1.0f, atmosphere_class::moderate);
    f.moon = add_body(w, "Moon", body_type::moon, f.home, 0.0026f, 0.012f, atmosphere_class::none);
    f.far  = add_body(w, "Far", body_type::planet, null_entity, 2.0f, 0.8f, atmosphere_class::thin);
    w.star_body = f.star;
    w.home_body = f.home;
    for (entity_id* n : {&f.a, &f.b})
    {
        *n = w.create_entity();
        nation_component nc;
        nc.name = (n == &f.a) ? "Alpha" : "Beta";
        w.nations.emplace(*n, nc);
    }
    f.home_tile = add_tile(w, f.home);
    f.moon_tile = add_tile(w, f.moon);
    f.home_mkt = add_market(w, f.home, f.home_tile);
    f.moon_mkt = add_market(w, f.moon, f.moon_tile);
    for (const entity_id n : {f.a, f.b})
    {
        nation_component& nc = w.nations.at(n);
        nc.capital_tile = f.home_tile;
        nc.treasury = 50000000.0f;
        nc.astronauts = 80;
        grant_keys(w, n);
        add_pad(w, n, f.home_tile);
        set_space_unit_design(w, n, space_hull::hauler, hauler_d());
        set_space_unit_design(w, n, space_hull::tug, tug_d());
        set_space_unit_design(w, n, space_hull::frigate, heavy ? frigate_heavy_d() : frigate_d());
    }
    f.low      = {f.home, orbital_node_kind::low_orbit};
    f.high     = {f.home, orbital_node_kind::high_orbit};
    f.moon_low = {f.moon, orbital_node_kind::low_orbit};
    f.l4       = {f.moon, orbital_node_kind::l4};
    w.current_econ_tick = 1;
}

entity_id raise(fixture& f, entity_id n, space_hull h)
{
    return raise_space_unit_at_pad(f.w, space_unit_params{}, n, h, f.home).unit;
}

/// A satellite of @p n at @p node, arrived (home orbits arrive the launch quarter).
entity_id satellite(fixture& f, entity_id n, orbital_node node)
{
    const space_launch_quote q = launch_space_asset(f.w, space_asset_params{}, n, space_asset_class::satellite, node,
                                                    space_satellite_role::comms);
    return q.result == space_launch_result::launched ? q.asset : null_entity;
}

void arrive_assets(fixture& f)
{
    run_space_asset_step(f.w, space_asset_params{}, f.w.current_econ_tick);
}

/// One space day as the economy runs it: units, battles, debris.
space_debris_day_report sday(fixture& f, space_battle_day_report* bout = nullptr,
                             const space_battle_params& bp = space_battle_params{})
{
    const space_unit_params p{};
    const station_params stp{};
    space_unit_day_report   ud;
    space_battle_day_report bd;
    space_debris_day_report dd;
    run_space_unit_day(f.w, p, stp, &ud);
    run_space_battle_day(f.w, p, stp, bp, &bd);
    run_space_debris_day(f.w, space_debris_params{}, ud, bd, &dd);
    if (bout != nullptr)
        *bout = bd;
    return dd;
}

space_debris_quarter_report quarter(fixture& f)
{
    space_debris_quarter_report r;
    run_space_debris_quarter(f.w, space_debris_params{}, watch_space_losses(f.w), &r);
    return r;
}

float hp(const fixture& f, entity_id u)
{
    const auto it = f.w.space_units.find(u);
    return it == f.w.space_units.end() ? -1.0f : it->second.hull_points;
}

float wear(const fixture& f, entity_id a)
{
    const auto it = f.w.space_assets.find(a);
    return it == f.w.space_assets.end() ? -1.0f : it->second.wear;
}

bool lost(const fixture& f, entity_id a)
{
    const auto it = f.w.space_assets.find(a);
    return it != f.w.space_assets.end() && it->second.state == space_asset_state::lost;
}

/// The D11 script.
void programme(fixture& f)
{
    raise(f, f.a, space_hull::frigate);
    raise(f, f.a, space_hull::frigate);
    raise(f, f.b, space_hull::frigate);
    raise(f, f.b, space_hull::hauler);
    raise(f, f.b, space_hull::tug);
    satellite(f, f.a, f.high);
    satellite(f, f.b, f.high);
    arrive_assets(f);
    f.w.space_debris[f.high] = 40.0f;
    declare_hostile(f.w, f.a, f.b);
    for (int q = 0; q < 3; ++q)
    {
        for (int d = 0; d < 30; ++d)
            sday(f);
        quarter(f);
        ++f.w.current_econ_tick;
    }
}

} // namespace

int main()
{
    const space_debris_params dp{};
    const space_unit_params p{};
    std::printf("debris_harness — BL-1598\n");

    // ---- D1 battle loss ------------------------------------------------------
    std::printf("--- D1 battle loss ---\n");
    {
        fixture f;
        build(f);
        raise(f, f.a, space_hull::frigate);
        raise(f, f.a, space_hull::frigate);
        const entity_id bf = raise(f, f.b, space_hull::frigate);
        declare_hostile(f.w, f.a, f.b);
        float mass = 0.0f;
        int days = 0;
        float level_before_loss = -1.0f;
        while (f.w.space_units.count(bf) != 0 && days < 30)
        {
            level_before_loss = space_debris_at(f.w, f.low);
            space_battle_day_report bd;
            sday(f, &bd);
            for (const auto& [n, m] : bd.mass_destroyed)
                mass += m;
            ++days;
        }
        const float lv = space_debris_at(f.w, f.low);
        const space_design_stats st = evaluate_space_design(space_hull::frigate, frigate_d());
        check(f.w.space_units.count(bf) == 0 && level_before_loss == 0.0f && near(mass, st.dry_mass)
                  && near(lv, mass * dp.debris_per_mass) && f.w.space_debris.size() == 1,
              "D1", fmt("B's frigate destroyed on day %d: mass %.2f (dry %.2f) -> level %.2f at home low orbit, "
                        "the only fouled node", days, mass, st.dry_mass, lv));
    }

    // ---- D2 wear scales ------------------------------------------------------
    std::printf("--- D2 wear scales ---\n");
    {
        fixture f;
        build(f);
        const entity_id s1 = satellite(f, f.a, f.low);
        const entity_id s2 = satellite(f, f.a, f.high);
        const entity_id s3 = satellite(f, f.b, f.high);
        arrive_assets(f);
        const float L = 25.0f;
        f.w.space_debris[f.low] = L;
        // s3 sits at high orbit too; the clean control is a satellite at the moon.
        f.w.space_debris[f.high] = 2.0f * L;
        const space_debris_quarter_report r = quarter(f);
        check(s1 != null_entity && s2 != null_entity && near(wear(f, s1), dp.wear_per_debris * L)
                  && near(wear(f, s2), dp.wear_per_debris * 2.0f * L) && near(wear(f, s2), 2.0f * wear(f, s1))
                  && wear(f, s3) == wear(f, s2) && r.worn_out == 0,
              "D2a", fmt("one quarter: wear %.4f at level %.0f, %.4f at %.0f (exactly twice)", wear(f, s1), L,
                         wear(f, s2), 2.0f * L));
        fixture g;
        build(g);
        const entity_id c = satellite(g, g.a, g.high);
        arrive_assets(g);
        g.w.space_debris[g.low] = L;
        quarter(g);
        check(c != null_entity && wear(g, c) == 0.0f, "D2b", "a satellite at a clean node gains no wear");
    }

    // ---- D3 unit damage scales ----------------------------------------------
    std::printf("--- D3 unit damage scales ---\n");
    {
        fixture f;
        build(f);
        const entity_id u1 = raise(f, f.a, space_hull::frigate);
        const entity_id u2 = raise(f, f.a, space_hull::frigate);
        const float h0 = hp(f, u1);
        const float L = 30.0f;
        f.w.space_debris[f.low] = L;
        const space_debris_day_report r1 = sday(f);
        const float d1 = h0 - hp(f, u1);
        f.w.space_debris[f.low] = 2.0f * L;
        sday(f);
        const float d2 = (h0 - d1) - hp(f, u1);
        // u2 departs for the moon: in transit, it takes nothing at the node it left.
        const space_unit_op mv = order_space_unit(f.w, p, f.a, u2, f.moon_low);
        const float h2 = hp(f, u2);
        const bool transit = f.w.space_units.at(u2).state == space_unit_state::in_transit
                          && f.w.space_units.at(u2).days_remaining > 1;
        sday(f);
        check(near(d1, dp.unit_damage_per_debris * L) && near(d2, 2.0f * d1) && near(r1.damage, 2.0f * d1)
                  && mv.result == space_unit_result::ok && transit && hp(f, u2) == h2,
              "D3", fmt("a day at level %.0f takes %.3f hp, at %.0f %.3f (twice); both idle units charged "
                        "(%.3f); a unit in transit takes nothing", L, d1, 2.0f * L, d2, r1.damage));
    }

    // ---- D4 passing through --------------------------------------------------
    std::printf("--- D4 passing through ---\n");
    {
        fixture f;
        build(f);
        const entity_id u = raise(f, f.a, space_hull::frigate);
        const orbital_path_result path = orbital_path(f.w, orbital_graph_params{}, f.low, f.moon_low);
        const bool via = path.ok && path.nodes.size() > 2;
        const orbital_node mid = via ? path.nodes[1] : f.high;
        const float L = 50.0f;
        f.w.space_debris[mid] = L;
        const float h0 = hp(f, u);
        const space_unit_op mv = order_space_unit(f.w, p, f.a, u, f.moon_low);
        int passed = 0, days = 0;
        while (f.w.space_units.at(u).state == space_unit_state::in_transit && days < 400)
        {
            passed += sday(f).passed;
            ++days;
        }
        check(via && mv.result == space_unit_result::ok && f.w.space_units.at(u).node == f.moon_low && passed == 1
                  && near(h0 - hp(f, u), dp.unit_damage_per_debris * L),
              "D4", fmt("home low -> moon low through %zu nodes, %d days: one pass-through charge at the fouled "
                        "node, %.3f hp (= %.3f x %.0f)", path.nodes.size(), days, h0 - hp(f, u),
                        dp.unit_damage_per_debris, L));
    }

    // ---- D5 cascade ----------------------------------------------------------
    std::printf("--- D5 cascade ---\n");
    {
        // Four satellites at home high orbit, level 20: 0.08 wear a quarter. With the
        // first already at 0.95, it is lost in quarter 1 and its mass raises the level.
        const auto run = [&](bool primed, int* quarters, float* after_q1, float* wear_q2) {
            fixture f;
            build(f);
            entity_id s[4];
            for (int i = 0; i < 4; ++i)
                s[i] = satellite(f, i % 2 == 0 ? f.a : f.b, f.high);
            arrive_assets(f);
            if (primed)
                f.w.space_assets.at(s[0]).wear = 0.95f;
            f.w.space_debris[f.high] = 20.0f;
            int q = 0;
            bool all = false;
            while (!all && q < 40)
            {
                const float w1 = wear(f, s[1]);
                quarter(f);
                ++q;
                if (q == 1)
                    *after_q1 = space_debris_at(f.w, f.high);
                if (q == 2)
                    *wear_q2 = wear(f, s[1]) - w1;
                all = true;
                for (const entity_id e : s)
                    all = all && lost(f, e);
            }
            *quarters = q;
        };
        int q_primed = 0, q_plain = 0;
        float lv1_primed = 0, lv1_plain = 0, w2_primed = 0, w2_plain = 0;
        run(true, &q_primed, &lv1_primed, &w2_primed);
        run(false, &q_plain, &lv1_plain, &w2_plain);
        check(near(lv1_primed, 20.0f + dp.satellite_mass * dp.debris_per_mass) && lv1_plain == 20.0f
                  && w2_primed > w2_plain && near(w2_primed, dp.wear_per_debris * lv1_primed) && q_primed < q_plain,
              "D5a", fmt("the near-worn satellite is lost in quarter 1: level 20 -> %.1f (+ its mass); quarter 2's "
                         "wear %.3f vs %.3f unprimed; all four lost in %d quarters vs %d",
                         lv1_primed, w2_primed, w2_plain, q_primed, q_plain));

        // Upkeep: a satellite unpaid to loss leaves its mass through the watch.
        fixture g;
        build(g);
        const entity_id s = satellite(g, g.a, g.high);
        arrive_assets(g);
        g.w.nations.at(g.a).treasury = 0.0f;
        float deposited = 0.0f;
        for (int q = 0; q < 8 && !lost(g, s); ++q)
        {
            ++g.w.current_econ_tick;
            const space_loss_watch watch = watch_space_losses(g.w);
            run_space_asset_step(g.w, space_asset_params{}, g.w.current_econ_tick);
            space_debris_quarter_report r;
            run_space_debris_quarter(g.w, dp, watch, &r);
            deposited += r.deposited;
        }
        check(lost(g, s) && near(deposited, dp.satellite_mass) && near(space_debris_at(g.w, g.high), dp.satellite_mass),
              "D5b", fmt("a satellite lost to upkeep leaves %.1f debris at its node", deposited));
    }

    // ---- D6 drag -------------------------------------------------------------
    std::printf("--- D6 drag ---\n");
    {
        fixture f;
        build(f);
        f.w.space_debris[f.low] = 100.0f;
        f.w.space_debris[f.high] = 100.0f;
        f.w.space_debris[f.l4] = 100.0f;
        f.w.space_debris[f.moon_low] = k_space_debris_floor * 1.05f;
        const space_debris_quarter_report r = quarter(f);
        check(near(space_debris_at(f.w, f.low), 100.0f * (1.0f - dp.low_orbit_drag))
                  && space_debris_at(f.w, f.high) == 100.0f && space_debris_at(f.w, f.l4) == 100.0f
                  && f.w.space_debris.count(f.moon_low) == 0 && near(r.dragged, 100.0f * dp.low_orbit_drag, 1e-3f),
              "D6", fmt("low orbit 100 -> %.2f; high orbit and L4 keep 100; a moon low orbit drawn under the floor "
                        "is erased", space_debris_at(f.w, f.low)));
    }

    // ---- D7 tug --------------------------------------------------------------
    std::printf("--- D7 tug ---\n");
    {
        fixture f;
        build(f);
        const entity_id t = raise(f, f.a, space_hull::tug);
        raise(f, f.b, space_hull::hauler);
        f.w.space_debris[f.low] = 20.0f;
        const space_debris_quarter_report r = quarter(f);
        const float want = (20.0f - dp.tug_clear_per_quarter) * (1.0f - dp.low_orbit_drag);
        const bool idle = t != null_entity && f.w.space_units.at(t).state == space_unit_state::idle;
        f.w.space_debris[f.low] = 4.0f;
        quarter(f);
        check(idle && near(r.cleared, dp.tug_clear_per_quarter) && near(space_debris_at(f.w, f.low), 0.0f)
                  && f.w.space_debris.empty() && near(want, (20.0f - 6.0f) * 0.92f),
              "D7a", fmt("one idle Tug: 20 -> cleared %.1f, then drag -> expected %.2f; a level of 4 is cleared "
                         "whole and erased", r.cleared, want));
        fixture g;
        build(g);
        raise(g, g.a, space_hull::tug);
        g.w.space_debris[g.low] = 20.0f;
        quarter(g);
        check(near(space_debris_at(g.w, g.low), want), "D7b",
              fmt("the level after the tug and drag is %.3f (want %.3f)", space_debris_at(g.w, g.low), want));
    }

    // ---- D8 the stall --------------------------------------------------------
    std::printf("--- D8 the stall ---\n");
    {
        const space_battle_params bp{};
        fixture f;
        build(f, true);
        const entity_id a1 = raise(f, f.a, space_hull::frigate);
        const entity_id b1 = raise(f, f.b, space_hull::frigate);
        declare_hostile(f.w, f.a, f.b);
        const float ha = hp(f, a1), hb = hp(f, b1);
        int standoffs = 0, day_closed = 0;
        for (int d = 1; d <= bp.stall_days + 5; ++d)
        {
            space_battle_day_report bd;
            sday(f, &bd);
            standoffs += bd.standoffs;
            if (bd.standoffs > 0)
                day_closed = d;
        }
        const space_battle* b = f.w.space_battles.empty() ? nullptr : &f.w.space_battles.begin()->second;
        check(b != nullptr && f.w.space_battles.size() == 1 && b->state == space_battle_state::standoff
                  && b->days == bp.stall_days && day_closed == bp.stall_days && standoffs == 1
                  && b->sides[0].end == space_battle_end::holding && b->sides[1].end == space_battle_end::holding
                  && hp(f, a1) == ha && hp(f, b1) == hb && b->quiet_days == 0,
              "D8a", fmt("fire 10 against armour 15: %d quiet days, then a standoff on day %d, both holding; "
                         "five more days open nothing", b ? b->days : -1, day_closed));
        raise(f, f.a, space_hull::frigate); // A's pool is now 20 > 15
        space_battle_day_report bd;
        sday(f, &bd);
        check(f.w.space_battles.size() == 2 && bd.opened == 1 && bd.landed > 0.0f, "D8b",
              fmt("a second A frigate (pool 20) re-engages: a new battle, %.0f landed", bd.landed));

        fixture g;
        build(g, true);
        raise(g, g.a, space_hull::frigate);
        raise(g, g.b, space_hull::frigate);
        declare_hostile(g.w, g.a, g.b);
        space_battle_params off{};
        off.stall_days = 0;
        for (int d = 0; d < 40; ++d)
            sday(g, nullptr, off);
        const space_battle* gb = g.w.space_battles.empty() ? nullptr : &g.w.space_battles.begin()->second;
        check(gb != nullptr && gb->state == space_battle_state::open && gb->days == 40 && gb->quiet_days == 40,
              "D8c", "stall_days 0: a quiet battle stays open (40 days, 40 quiet)");
    }

    // ---- D9 save -------------------------------------------------------------
    std::printf("--- D9 save ---\n");
    std::string bytes;
    fixture saved;
    {
        fixture& f = saved;
        build(f, true);
        const entity_id s = satellite(f, f.a, f.high);
        arrive_assets(f);
        f.w.space_debris[f.low] = 30.0f;
        f.w.space_debris[f.high] = 40.0f;
        quarter(f); // the satellite wears; low orbit drags
        raise(f, f.a, space_hull::frigate);
        raise(f, f.b, space_hull::frigate);
        declare_hostile(f.w, f.a, f.b);
        sday(f);
        sday(f);
        sday(f);
        const space_battle* b = f.w.space_battles.empty() ? nullptr : &f.w.space_battles.begin()->second;
        bytes = to_bytes(f.w);
        world w2;
        const bool ok = from_bytes(bytes, w2);
        const std::string again = ok ? to_bytes(w2) : std::string();
        const bool same = ok && w2.space_debris == f.w.space_debris && w2.space_battles == f.w.space_battles
                       && w2.space_assets.at(s).wear == f.w.space_assets.at(s).wear;
        check(b != nullptr && b->quiet_days == 3 && wear(f, s) > 0.0f && f.w.space_debris.size() == 2 && same
                  && again == bytes && w2.state_hash(0) == f.w.state_hash(0),
              "D9a", fmt("round-trip: %zu levels, wear %.3f, a battle 3 days quiet: equal hash, byte-identical",
                         f.w.space_debris.size(), wear(f, s)));
        std::ostringstream o87(std::ios::binary);
        write_world_snapshot_pre_debris(f.w, o87);
        const std::size_t sec = world_save_debris_section_bytes(f.w);
        check(o87.str().size() + sec + (world_save_space_bombard_section_bytes(f.w) + (world_save_offworld_base_section_bytes(f.w) + world_save_space_firsts_section_bytes(f.w))) == bytes.size() && sec == 12 + 2 * 9 + 8 + 8, "D9b",
              fmt("the section is %zu bytes (12 + 18 + 8 + 8); v87 prefix %zu + %zu == %zu", sec, o87.str().size(),
                  sec, bytes.size()));
        // The v87 prefix of the same world reads with the debris dropped.
        world w87;
        check(from_bytes(o87.str(), w87) && w87.space_debris.empty() && w87.space_assets.at(s).wear == 0.0f
                  && w87.space_battles.begin()->second.quiet_days == 0,
              "D9c", "the v87 prefix reads as a world with no debris, no wear, no quiet days");
        fixture e;
        build(e);
        std::ostringstream oe(std::ios::binary);
        write_world_snapshot_pre_debris(e.w, oe);
        world we;
        check(from_bytes(oe.str(), we) && we.state_hash(0) == e.w.state_hash(0) && world_save_debris_section_bytes(e.w) == 12
                  && to_bytes(we) == to_bytes(e.w),
              "D9d", "a debris-free world: its v87 prefix reads with an equal hash; its section is 12 bytes");
        // A standoff record round-trips (the state the v88 battle section admits).
        fixture so;
        build(so, true);
        raise(so, so.a, space_hull::frigate);
        raise(so, so.b, space_hull::frigate);
        declare_hostile(so.w, so.a, so.b);
        for (int d = 0; d < space_battle_params{}.stall_days; ++d)
            sday(so);
        const std::string sb = to_bytes(so.w);
        world ws;
        check(!so.w.space_battles.empty() && so.w.space_battles.begin()->second.state == space_battle_state::standoff
                  && from_bytes(sb, ws) && to_bytes(ws) == sb,
              "D9e", "a standoff report round-trips byte-identical");
    }

    // ---- D10 tampered --------------------------------------------------------
    std::printf("--- D10 tampered ---\n");
    {
        const std::string& b = bytes;
        const std::size_t s0 = b.size() - (12 + 2 * 9 + 8 + 8) - 4 - 8 - 4; // the v89 bombard (BL-1599), v90 base (BL-1601) and v91 firsts (BL-1606) sections follow, empty: 4 + 8 + 4 bytes
        const std::size_t lv0 = s0 + 4;           // first level: body, kind, f32
        const std::size_t lv1 = lv0 + 9;
        const std::size_t w0  = lv1 + 9 + 4;      // first wear: id, f32
        const std::size_t q0  = w0 + 8 + 4;       // first quiet: id, i32
        const auto refused = [&](std::string bad) {
            world t;
            t.nations.emplace(999, nation_component{});
            return !from_bytes(bad, t) && t.nations.size() == 1 && t.nations.count(999) == 1;
        };
        const auto poke = [&](std::size_t at, const void* v, std::size_t n) {
            std::string bad = b;
            std::memcpy(&bad[at], v, n);
            return bad;
        };
        const float nan = std::nanf(""), floor_v = k_space_debris_floor, huge = k_space_debris_max * 2.0f;
        const float one = 1.0f, neg = -0.5f;
        const uint8_t surface = static_cast<uint8_t>(orbital_node_kind::surface), bad_kind = 9;
        const uint32_t ghost = 0x00FFFFFFu, big = 0x7FFFFFFFu;
        const int32_t zero = 0;
        bool all = true;
        std::string why;
        const auto expect = [&](bool r, const char* what) { all = all && r; if (!r) { why += what; why += " "; } };
        expect(!refused(b), "clean-refused");
        expect(refused(poke(lv0 + 5, &nan, 4)), "level-nan");
        expect(refused(poke(lv0 + 5, &floor_v, 4)), "level-at-floor");
        expect(refused(poke(lv0 + 5, &huge, 4)), "level-over-max");
        expect(refused(poke(lv0 + 4, &surface, 1)), "surface-node");
        expect(refused(poke(lv0 + 4, &bad_kind, 1)), "kind-range");
        expect(refused(poke(lv0, &ghost, 4)), "ghost-body");
        {
            // Swap the two levels: out of order.
            std::string bad = b;
            std::memcpy(&bad[lv0], &b[lv1], 9);
            std::memcpy(&bad[lv1], &b[lv0], 9);
            expect(refused(bad), "nodes-out-of-order");
        }
        expect(refused(poke(w0 + 4, &one, 4)), "wear-1");
        expect(refused(poke(w0 + 4, &neg, 4)), "wear-negative");
        expect(refused(poke(w0, &ghost, 4)), "wear-ghost");
        expect(refused(poke(q0, &ghost, 4)), "quiet-ghost-battle");
        expect(refused(poke(q0 + 4, &zero, 4)), "quiet-zero");
        expect(refused(poke(s0, &big, 4)), "count-huge");
        expect(refused(b.substr(0, b.size() - 1)), "truncated");
        check(all, "D10", "tampered debris sections refused whole (NaN / floor / over-max level, surface node, kind "
                          "range, ghost body, node order, wear 1 / negative / ghost, quiet ghost / zero, a huge count, "
                          "truncation); the clean stream reads"
                          + (why.empty() ? std::string() : " — FAILED: " + why));
    }

    // ---- D11 determinism -----------------------------------------------------
    std::printf("--- D11 determinism ---\n");
    {
        fixture x, y;
        build(x);
        build(y);
        programme(x);
        programme(y);
        check(x.w.state_hash(0) == y.w.state_hash(0) && to_bytes(x.w) == to_bytes(y.w) && !x.w.space_debris.empty()
                  && x.w.space_debris == y.w.space_debris,
              "D11", fmt("two fixtures driven identically (a battle, wear, a tug, drag, 3 quarters): equal hash and "
                         "stream; %zu fouled nodes, low orbit %.2f, high orbit %.2f", x.w.space_debris.size(),
                         space_debris_at(x.w, x.low), space_debris_at(x.w, x.high)));
    }

    std::printf("debris_harness: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
