// ---------------------------------------------------------------------------
// war_bodies_harness — BL-1602 (war spans bodies). No SDL/Lua.
// ---------------------------------------------------------------------------
// docs/military/MILITARY.md § Space, "A war spans every body"; § Fronts; § Battles;
// § Occupation; docs/politics/NATIONS.md § Off-world bases.
//
// A hand-built fixture on base_harness's pattern: a star, a home planet (16x8, the
// west half Alpha's, the east half Beta's, each with a centre and a staffed pad),
// and an airless moon with NO provinces. Both nations hold the far-space keys; each
// lands colonists and a kit and FOUNDS A BASE on the moon through the space command
// seam, Beta's beside Alpha's, so the two hold neighbouring ground there. Land units
// on the moon are STAGED (no inter-body march exists: reaching another body is the
// space layer's job). Rows:
//
//   W1  fronts on the moon: each nation has a front toward the other whose span is
//       all moon ground it holds; its home front stays on the home body.
//   W2  a front on another body is refused: a home unit to the moon front, a moon
//       unit to the home front (rejected_invalid, nothing written); a moon unit to
//       the moon front applies.
//   W3  the war: goals on the moon are legal goals (declare and answer apply).
//   W4  a garrison on a base: Beta's unit defends its base province (the verb
//       applies off-world); the attack-goal fallback names Beta's base province on
//       the moon, not its home capital's.
//   W5  muster: a completed military base on base ground answers the hire check as
//       one on home ground does.
//   W6  a battle on the moon by the ordinary rules: Alpha's moon front pushes (and
//       occupies empty ground), then its unit marches onto the base's garrison: the
//       battle stands on a moon tile with the garrison defending,
//       resolves, and the won tile is occupied (and the holder moves when Beta has
//       nobody left in the province).
//   W7  a cession of the moon province moves the ground off-world; Beta keeps its
//       home capital; its base there is no longer its capital on the moon.
//   W8  a ceded HOME capital stays on the home body while a home province remains,
//       even where the only centre left is the moon base.
//   W9  determinism: two identical runs end with equal hash and bytes; a save
//       mid-war round-trips byte-identical.
//
// Build: node tools/verify/build_harness.js war_bodies_harness --run
// ---------------------------------------------------------------------------

#include "world/battle_system.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/early_space_tree_data.hpp"
#include "world/economy_system.hpp" // run_unit_march
#include "world/fronts.hpp"
#include "world/hex_neighbors.hpp"
#include "world/offworld_base.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/sentiment.hpp"
#include "world/space_command.hpp"
#include "world/space_units.hpp"
#include "world/stance.hpp"
#include "world/survey_system.hpp"
#include "world/unit_roster.hpp"
#include "world/war.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
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
    char buf[700];
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

constexpr int GW = 16, GH = 8;

int row_named(const char* name)
{
    const auto& t = unit_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
        if (std::string(t[i].name) == name) return static_cast<int>(i);
    return -1;
}

recipe_registry make_reg()
{
    recipe_registry reg;
    building_economics ex;
    ex.base_rate = 10.0f;
    ex.build_cost = 100.0f;
    ex.build_duration_ticks = 1.0f;
    reg.set_economics(building_type::extraction_site, ex);
    building_economics hub;
    hub.build_cost = 50.0f;
    reg.set_economics(building_type::inland_logistics_hub, hub);
    // fronts_harness's per-call march speeds: one run_unit_march call moves a unit one
    // plains tile (these rows are about where a war is fought, not how fast).
    military_capability_params mp = reg.military();
    const float per_call[unit_class_count] = { 1.0f, 1.5f, 1.0f, 0.5f, 0.0f };
    for (int k = 0; k < unit_class_count; ++k)
        mp.march_days_per_cls[static_cast<std::size_t>(k)] = per_call[k] > 0.0f ? 1.0f / per_call[k] : 0.0f;
    for (int c = 0; c < land_class_count; ++c)
        if (const int row = modern_row_for(static_cast<land_class>(c)); row >= 0)
            mp.march_days_per_land_class[static_cast<std::size_t>(c)] =
                mp.march_days_per_cls[static_cast<std::size_t>(unit_roster_table()[static_cast<std::size_t>(row)].cls)];
    reg.set_military(mp);
    return reg;
}

struct fixture
{
    world     w;
    entity_id star = null_entity, home = null_entity, moon = null_entity;
    entity_id a = null_entity, b = null_entity;
    entity_id home_mkt = null_entity;
    entity_id home_cap[2] = {};
    entity_id moon_tile[GW * GH] = {};
    entity_id passenger[2] = {}, carrier[2] = {};
    entity_id base_tile[2] = {};
    uint32_t  base_prov[2] = {};
};

entity_id add_body(world& w, const char* name, body_type t, entity_id parent, float au, int gw, int gh,
                   atmosphere_class atm)
{
    const entity_id id = w.create_entity();
    body_component bc;
    bc.name = name;
    bc.type = t;
    bc.parent = parent;
    bc.orbital_radius_au = au;
    bc.orbital_angle_rad = 0.0f;
    bc.grid_width = gw;
    bc.grid_height = gh;
    bc.survey = {};
    bc.atmosphere = atm;
    w.bodies.emplace(id, bc);
    return id;
}

entity_id add_tile(world& w, entity_id body, int x, int y, terrain_substrate sub, float hab)
{
    const entity_id id = w.create_entity();
    tile_component tc{};
    tc.body = body;
    tc.grid_x = x;
    tc.grid_y = y;
    tc.substrate = sub;
    tc.landform = terrain_landform::plains;
    tc.habitability = hab;
    tc.hazard_level = 0.0f;
    w.tiles.emplace(id, tc);
    return id;
}

void grant_far_space(world& w, entity_id nation)
{
    nation_tech_state& st = w.nation_tech[nation];
    for (const io::es::effect& e : io::es::effects)
        if (e.kind == io::es::effect_kind::mechanic && e.node >= 0 && e.node < io::es::node_count)
            st.es_held.set(static_cast<std::size_t>(e.node));
}

space_design design_of(const char* bay)
{
    space_design d;
    d.fill(k_no_space_item);
    const auto& cat = space_item_catalogue();
    const std::pair<space_unit_slot, const char*> items[] = {
        {space_unit_slot::drive, "drive.chemical"}, {space_unit_slot::tanks, "tanks.extended"}, {space_unit_slot::bay, bay}};
    for (const auto& [slot, id] : items)
        for (std::size_t i = 0; i < cat.size(); ++i)
            if (std::strcmp(cat[i].id, id) == 0)
                d[static_cast<std::size_t>(slot)] = static_cast<uint8_t>(i);
    return d;
}

entity_id stage_lander(world& w, entity_id owner, const char* bay, orbital_node node, space_unit_state state)
{
    const entity_id id = w.create_entity();
    space_unit u;
    u.id = id;
    u.owner = owner;
    u.hull = space_hull::lander;
    u.design = design_of(bay);
    u.state = state;
    u.node = node;
    u.dest = node;
    u.hull_points = evaluate_space_design(u.hull, u.design).hull_points;
    w.space_units.emplace(id, u);
    return id;
}

space_command_outcome space_cmd(fixture& f, entity_id nation, space_verb v, entity_id subject, int32_t slot = 0)
{
    space_command c;
    c.nation = nation;
    c.verb = static_cast<uint8_t>(v);
    c.subject = subject;
    c.slot = slot;
    return apply_space_command(f.w, make_reg(), c);
}

entity_id mt(const fixture& f, int x, int y) { return f.moon_tile[y * GW + x]; }

/// The hex neighbours of @p t on its body (columns wrap), ascending id.
std::vector<entity_id> neighbours(const fixture& f, entity_id t)
{
    std::vector<entity_id> out;
    const tile_component& tc = f.w.tiles.at(t);
    const body_component& bc = f.w.bodies.at(tc.body);
    for (int s = 0; s < 6; ++s)
    {
        hex_neighbors::coord c = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, s);
        if (c.gy < 0 || c.gy >= bc.grid_height) continue;
        c.gx = (c.gx % bc.grid_width + bc.grid_width) % bc.grid_width;
        for (const auto& [tid, oc] : f.w.tiles)
            if (oc.body == tc.body && oc.grid_x == c.gx && oc.grid_y == c.gy) out.push_back(tid);
    }
    std::sort(out.begin(), out.end());
    return out;
}

entity_id owner_of(const world& w, entity_id t)
{
    const auto it = w.tile_to_nation.find(t);
    return it != w.tile_to_nation.end() ? it->second : null_entity;
}

void build(fixture& f)
{
    world& w = f.w;
    f.star = add_body(w, "Star", body_type::star, null_entity, 0.0f, 0, 0, atmosphere_class::none);
    f.home = add_body(w, "Home", body_type::planet, null_entity, 1.0f, GW, GH, atmosphere_class::moderate);
    f.moon = add_body(w, "Moon", body_type::moon, f.home, 0.0026f, GW, GH, atmosphere_class::none);
    w.star_body = f.star;
    w.home_body = f.home;
    w.bodies.at(f.home).survey.phase = survey_phase::surveyed;
    for (entity_id* n : {&f.a, &f.b})
    {
        *n = w.create_entity();
        nation_component nc;
        nc.name = (n == &f.a) ? "Alpha" : "Beta";
        nc.treasury = 100000.0f;
        w.nations.emplace(*n, nc);
    }
    w.player_entity = f.a;

    // The home planet: the west half Alpha's, the east half Beta's.
    for (int y = 0; y < GH; ++y)
        for (int x = 0; x < GW; ++x)
        {
            const entity_id t = add_tile(w, f.home, x, y, terrain_substrate::sedimentary, 1.0f);
            const entity_id n = x < GW / 2 ? f.a : f.b;
            if (y == 2 && (x == 2 || x == 12))
                f.home_cap[x == 2 ? 0 : 1] = t;
            w.set_tile_nation(t, n);
            w.nations.at(n).tiles.push_back(t);
        }
    f.home_mkt = w.create_entity();
    {
        market_component m{};
        m.body = f.home;
        m.centre_tile = f.home_cap[0];
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            m.base_price[r] = 10.0f;
            m.inventory[r] = 1000.0f;
        }
        m.price = m.base_price;
        w.markets.emplace(f.home_mkt, m);
    }
    for (int i = 0; i < 2; ++i)
    {
        const entity_id n = i == 0 ? f.a : f.b;
        w.nations.at(n).capital_tile = f.home_cap[i];
        const entity_id centre = w.create_entity();
        population_centre_component pc;
        pc.scale = 3;
        pc.population = 50;
        w.population_centres[centre] = pc;
        w.population_centre_tile[centre] = f.home_cap[i];
        const entity_id pad = w.create_entity();
        building_component bc{};
        bc.tile = f.home_cap[i];
        bc.type = building_type::launchpad;
        bc.workforce_assigned = 1.0f;
        bc.ticks_remaining = 0;
        w.buildings.emplace(pad, bc);
        w.nations.at(n).state_assets.push_back(pad);
    }
    build_province_partition(w, 0x1602u);
    seed_province_holders(w);

    for (int y = 0; y < GH; ++y)
        for (int x = 0; x < GW; ++x)
            f.moon_tile[y * GW + x] = add_tile(w, f.moon, x, y, terrain_substrate::regolith, 0.0f);
    w.body_tile_index.clear(); // the partition warmed the raster before the moon had tiles

    for (int i = 0; i < 2; ++i)
    {
        const entity_id n = i == 0 ? f.a : f.b;
        grant_far_space(w, n);
        f.passenger[i] = stage_lander(w, n, "bay.passenger", {f.home, orbital_node_kind::low_orbit}, space_unit_state::idle);
        f.carrier[i] = stage_lander(w, n, "bay.cargo", {f.moon, orbital_node_kind::surface}, space_unit_state::landed);
        space_unit& cu = w.space_units.at(f.carrier[i]);
        cu.cargo = 20.0f;
        space_haul h;
        h.owner = n;
        h.phase = space_haul_phase::held;
        h.cargo_res = resource_type::machinery;
        h.src_market = f.home_mkt;
        h.target = cu.node;
        h.home = cu.node;
        w.space_hauls.emplace(f.carrier[i], h);
    }
}

/// Board 4, land the passenger Lander on the moon, map the moon whole.
void ready(fixture& f, int i)
{
    const entity_id n = i == 0 ? f.a : f.b;
    (void)space_cmd(f, n, space_verb::embark_colonists, f.passenger[i], 4);
    space_unit& u = f.w.space_units.at(f.passenger[i]);
    u.state = space_unit_state::landed;
    u.node = u.dest = {f.moon, orbital_node_kind::surface};
    survey_map_regions(f.w, n, f.moon, 99);
}

/// Both bases founded: Alpha's at (4, 3), Beta's on the lowest-id free moon tile
/// beside Alpha's ground whose province nobody holds. Returns whether both applied.
bool found_both(fixture& f)
{
    ready(f, 0);
    ready(f, 1);
    f.base_tile[0] = mt(f, 4, 3);
    const auto oa = space_cmd(f, f.a, space_verb::found_base, f.base_tile[0]);
    if (oa.result != space_command_result::applied)
        return false;
    f.base_prov[0] = f.w.provinces.province_of(f.base_tile[0]);
    std::vector<entity_id> moon_ids(f.moon_tile, f.moon_tile + GW * GH);
    std::sort(moon_ids.begin(), moon_ids.end());
    for (const entity_id t : moon_ids)
    {
        if (owner_of(f.w, t) != null_entity) continue;
        const uint32_t p = f.w.provinces.province_of(t);
        if (p == 0 || province_holder_for(f.w, p) != null_entity) continue;
        bool beside = false;
        for (const entity_id n : neighbours(f, t))
            beside = beside || owner_of(f.w, n) == f.a;
        if (!beside) continue;
        f.base_tile[1] = t;
        break;
    }
    if (f.base_tile[1] == null_entity)
        return false;
    const auto ob = space_cmd(f, f.b, space_verb::found_base, f.base_tile[1]);
    f.base_prov[1] = f.w.provinces.province_of(f.base_tile[1]);
    return ob.result == space_command_result::applied;
}

entity_id add_unit(fixture& f, entity_id owner_nation, entity_id tile, int count)
{
    const entity_id u = f.w.create_entity();
    unit_component uc{};
    uc.position = tile;
    uc.owner = owner_nation;
    uc.count = count;
    uc.type = static_cast<uint16_t>(row_named("Rifle Infantry"));
    f.w.units[u] = uc;
    return u;
}

corp_command_result cmd(world& w, const recipe_registry& reg, corp_verb v, entity_id actor, entity_id subject,
                        entity_id tile, uint32_t order = 0, uint32_t province = no_province)
{
    corp_command c;
    c.corp = actor;
    c.as_nation = actor;
    c.verb = v;
    c.subject = subject;
    c.tile = tile;
    c.order = order;
    c.province = province;
    return apply_corp_command(w, reg, c);
}

const front* front_toward_on(const std::vector<front>& fs, const world& w, entity_id neighbour, entity_id body)
{
    for (const front& fr : fs)
        if (fr.neighbour == neighbour && !fr.span.empty() && w.tiles.at(fr.span.front()).body == body)
            return &fr;
    return nullptr;
}

bool span_all(const world& w, const front& fr, entity_id body, entity_id holder)
{
    for (const entity_id t : fr.span)
        if (w.tiles.at(t).body != body || w.tile_controller(t) != holder)
            return false;
    return !fr.span.empty();
}

/// The war on the moon, opened: goals on each side's moon province, both rows.
bool open_war(fixture& f)
{
    const sentiment_params sp{};
    const war_result d = declare_war(f.w, sp, f.a, f.b, {f.base_prov[1]});
    const war_result r = answer_war(f.w, f.b, f.a, {f.base_prov[0]});
    return d == war_result::applied && r == war_result::applied;
}

struct battle_run
{
    bool     opened_on_moon = false, garrison_defended = false, concluded = false;
    entity_id fought_tile = null_entity, field = null_entity;
    int      days = 0;
    int      pushed_occupied = 0; ///< Beta's moon tiles Alpha's push occupied in its first days.
};

/// The ordinary day, as the tick orders it: fronts, defenders, march, battles.
/// PUSH FIRST: the moon front pushes for @p push_days (empty ground is the weakest
/// target, so it occupies). Then the unit is unassigned and marched onto the
/// garrison's tile by hand: an attack by the ordinary rules.
battle_run fight(fixture& f, const recipe_registry& reg, entity_id attacker_unit, entity_id garrison,
                 int push_days, int max_days)
{
    battle_run out;
    for (int d = 0; d < max_days; ++d)
    {
        if (d == push_days)
        {
            for (const auto& [t, occ] : f.w.tile_occupier)
                out.pushed_occupied += (occ == f.a && f.w.tiles.at(t).body == f.moon && owner_of(f.w, t) == f.b) ? 1 : 0;
            (void)cmd(f.w, reg, corp_verb::assign_front, f.a, attacker_unit, null_entity);
            (void)cmd(f.w, reg, corp_verb::march_unit, f.a, attacker_unit, f.w.units.at(garrison).position);
        }
        advance_fronts(f.w, reg);
        advance_defenders(f.w, reg);
        (void)run_unit_march(f.w, reg);
        for (const active_battle& b : f.w.battles)
            if (f.w.tiles.at(b.tile).body == f.moon && b.attacker == f.a && b.defender == f.b)
            {
                out.opened_on_moon = true;
                out.fought_tile = b.tile;
                out.garrison_defended = out.garrison_defended
                    || std::find(b.defender_units.begin(), b.defender_units.end(), garrison) != b.defender_units.end();
            }
        const battle_tick bt = run_battles(f.w, reg, d);
        for (const battle_dispatch& dp : bt.dispatches)
            if (dp.tile == out.fought_tile && dp.field_held_by != null_entity)
            {
                out.concluded = true;
                out.field = dp.field_held_by;
            }
        reanchor_fronts(f.w);
        out.days = d + 1;
        if (out.concluded)
        {
            // One more march day for the advance to settle on the won tile.
            (void)run_unit_march(f.w, reg);
            reanchor_fronts(f.w);
            break;
        }
    }
    return out;
}

} // namespace

int main()
{
    std::printf("=== war spans bodies (BL-1602) ===\n");
    const recipe_registry reg = make_reg();

    // -----------------------------------------------------------------------
    std::printf("\nW1 fronts on the moon\n");
    fixture f;
    build(f);
    const bool founded = found_both(f);
    check(founded && f.base_prov[0] != 0 && f.base_prov[1] != 0 && f.base_prov[0] != f.base_prov[1],
          "W1a", fmt("both bases founded on the moon, neighbouring provinces %u (Alpha) and %u (Beta)",
                     f.base_prov[0], f.base_prov[1]));
    if (!founded)
    {
        std::printf("\n=== %d passed, %d failed (fixture failed) ===\n", g_pass, g_fail + 1);
        return 1;
    }
    reanchor_fronts(f.w);
    const std::vector<front> fa = nation_fronts(f.w, f.a);
    const std::vector<front> fb = nation_fronts(f.w, f.b);
    const front* a_moon = front_toward_on(fa, f.w, f.b, f.moon);
    const front* b_moon = front_toward_on(fb, f.w, f.a, f.moon);
    const front* a_home = front_toward_on(fa, f.w, f.b, f.home);
    check(a_moon && b_moon && span_all(f.w, *a_moon, f.moon, f.a) && span_all(f.w, *b_moon, f.moon, f.b),
          "W1b", fmt("each has a front toward the other on the moon, its span all its own moon ground (%zu, %zu tiles)",
                     a_moon ? a_moon->span.size() : 0, b_moon ? b_moon->span.size() : 0));
    check(a_home && span_all(f.w, *a_home, f.home, f.a) && a_home->key != a_moon->key,
          "W1c", "Alpha's home front toward Beta stays on the home body, a front of its own");
    const entity_id a_moon_key = a_moon->key, a_home_key = a_home->key;

    // -----------------------------------------------------------------------
    std::printf("\nW2 a front on another body is refused\n");
    const entity_id a_home_unit = add_unit(f, f.a, f.home_cap[0], 40);
    const entity_id a_moon_unit = add_unit(f, f.a, f.base_tile[0], 200);
    {
        const std::string b0 = to_bytes(f.w);
        const uint64_t h0 = f.w.state_hash(0);
        const auto r1 = cmd(f.w, reg, corp_verb::assign_front, f.a, a_home_unit, a_moon_key);
        const auto r2 = cmd(f.w, reg, corp_verb::assign_front, f.a, a_moon_unit, a_home_key);
        check(r1 == corp_command_result::rejected_invalid && r2 == corp_command_result::rejected_invalid
                  && to_bytes(f.w) == b0 && f.w.state_hash(0) == h0,
              "W2a", "home unit -> moon front, moon unit -> home front: rejected_invalid, nothing written");
        check(cmd(f.w, reg, corp_verb::assign_front, f.a, a_home_unit, a_home_key) == corp_command_result::applied
                  && cmd(f.w, reg, corp_verb::assign_front, f.a, a_moon_unit, a_moon_key) == corp_command_result::applied,
              "W2b", "each unit to the front on its own body applies");
        (void)cmd(f.w, reg, corp_verb::assign_front, f.a, a_home_unit, null_entity); // keep home quiet
    }

    // -----------------------------------------------------------------------
    std::printf("\nW3 the war: goals on the moon\n");
    check(open_war(f) && at_war(f.w, f.a, f.b), "W3a",
          "declare_war naming Beta's moon province, answer_war naming Alpha's: both apply");

    // -----------------------------------------------------------------------
    std::printf("\nW4 a garrison on a base\n");
    const entity_id garrison = add_unit(f, f.b, f.base_tile[1], 30);
    {
        const auto r = cmd(f.w, reg, corp_verb::defend_province, f.b, garrison, null_entity, 0, f.base_prov[1]);
        const entity_id post = f.w.units.at(garrison).defend_post;
        check(r == corp_command_result::applied && f.w.units.at(garrison).defend_province == f.base_prov[1]
                  && post != null_entity && f.w.tiles.at(post).body == f.moon,
              "W4a", "Beta's garrison is ordered to defend its base province: applies, its post on the moon");
        // The garrison then holds its tile (no defend order) so W6's attack finds it
        // where it stands; a defender chasing a stack that steps away is § Defend
        // province's own behaviour, the same on every body.
        (void)cmd(f.w, reg, corp_verb::defend_province, f.b, garrison, null_entity, 0, no_province);
        const uint32_t goal = attack_goal_province(f.w, f.b, f.base_tile[0]);
        const uint32_t home_goal = attack_goal_province(f.w, f.b, f.home_cap[0]);
        check(goal == f.base_prov[1]
                  && home_goal == f.w.provinces.province_of(f.home_cap[1]),
              "W4b", fmt("off Beta's ground on the moon the attack goal is its base province (%u, want %u); "
                         "on the home body its capital's (%u)", goal, f.base_prov[1], home_goal));
    }

    // -----------------------------------------------------------------------
    std::printf("\nW5 muster on base ground\n");
    {
        world probe = f.w;
        const auto stage_base = [&](entity_id tile) {
            const entity_id id = probe.create_entity();
            building_component bc{};
            bc.tile = tile;
            bc.type = building_type::military_base;
            bc.ticks_remaining = 0;
            bc.workforce_assigned = 1.0f;
            probe.buildings.emplace(id, bc);
            probe.nations.at(f.b).state_assets.push_back(id);
        };
        const uint16_t row = static_cast<uint16_t>(row_named("Rifle Infantry"));
        const auto none = hire_unit_check(probe, reg, f.b, f.base_tile[1], row, nullptr);
        stage_base(f.base_tile[1]);
        stage_base(f.home_cap[1]);
        entity_id mb = null_entity, hb = null_entity;
        const auto moon_r = hire_unit_check(probe, reg, f.b, f.base_tile[1], row, &mb);
        const auto home_r = hire_unit_check(probe, reg, f.b, f.home_cap[1], row, &hb);
        check(moon_r == home_r && moon_r != corp_command_result::rejected_placement
                  && none == corp_command_result::rejected_placement,
              "W5a", fmt("the hire check answers the same on base ground (%d) as at home (%d), past the base test; "
                         "with no base there it is refused for placement (%d)",
                         static_cast<int>(moon_r), static_cast<int>(home_r), static_cast<int>(none)));
    }

    // -----------------------------------------------------------------------
    std::printf("\nW6 a battle on the moon\n");
    world before_battle = f.w; // W9's twin starts here
    check(cmd(f.w, reg, corp_verb::set_front_order, f.a, null_entity, a_moon_key, 1) == corp_command_result::applied,
          "W6a", "Alpha sets its moon front to push (hostile: at war)");
    const battle_run br = fight(f, reg, a_moon_unit, garrison, 3, 80);
    check(br.pushed_occupied > 0, "W6p",
          fmt("at war, the moon front's push occupied %d of Beta's moon tiles in 3 days", br.pushed_occupied));
    check(br.opened_on_moon && br.garrison_defended, "W6b",
          fmt("a battle opened on a moon tile (%u), Alpha attacking, Beta's garrison among the defenders", br.fought_tile));
    check(br.concluded && br.field == f.a, "W6c",
          fmt("it resolved by the ordinary rules in %d days; the field held by %s", br.days,
              br.field == f.a ? "Alpha" : br.field == f.b ? "Beta" : "nobody"));
    {
        bool occupied_on_moon = false;
        for (const auto& [t, occ] : f.w.tile_occupier)
            occupied_on_moon = occupied_on_moon || (occ == f.a && f.w.tiles.at(t).body == f.moon && owner_of(f.w, t) == f.b);
        check(occupied_on_moon, "W6d", "Alpha occupies Beta's moon ground it won; ownership unmoved");
        bool b_left = false;
        for (const auto& [uid, u] : f.w.units)
            b_left = b_left || (u.owner == f.b && u.count > 0 && f.w.provinces.province_of(u.position) == f.base_prov[1]);
        const entity_id holder = province_holder_for(f.w, f.base_prov[1]);
        check(b_left ? holder == f.b : holder == f.a, "W6e",
              fmt("the province holder follows the battle rule: Beta %s in the province, holder %s", b_left ? "still" : "no longer",
                  holder == f.a ? "Alpha" : holder == f.b ? "Beta" : "nobody"));
    }

    // -----------------------------------------------------------------------
    std::printf("\nW9 determinism and the save, mid-war on the moon\n");
    {
        fixture g;
        g.w = before_battle;
        g.star = f.star; g.home = f.home; g.moon = f.moon; g.a = f.a; g.b = f.b;
        (void)cmd(g.w, reg, corp_verb::set_front_order, g.a, null_entity, a_moon_key, 1);
        (void)fight(g, reg, a_moon_unit, garrison, 3, 80);
        check(g.w.state_hash(0) == f.w.state_hash(0) && to_bytes(g.w) == to_bytes(f.w), "W9a",
              "a twin run from the same state ends with equal hash and bytes");
        const std::string b = to_bytes(f.w);
        world back;
        check(from_bytes(b, back) && to_bytes(back) == b && back.state_hash(0) == f.w.state_hash(0)
                  && back.tile_occupier == f.w.tile_occupier,
              "W9b", "saved mid-war: byte-identical, equal hash, the occupation by value");
    }

    // -----------------------------------------------------------------------
    std::printf("\nW7 a cession on the moon\n");
    {
        world& w = f.w;
        const entity_id b_cap = w.nations.at(f.b).capital_tile;
        cede_provinces(w, f.b, f.a, {f.base_prov[1]});
        const province* pr = w.provinces.find(f.base_prov[1]);
        bool all_a = pr != nullptr;
        if (pr)
            for (const entity_id t : pr->tiles)
                all_a = all_a && owner_of(w, t) == f.a;
        check(all_a && province_holder_for(w, f.base_prov[1]) == f.a, "W7a",
              "the moon province's ground and holder pass to Alpha");
        reanchor_fronts(w);
        // Beta owns nothing on the moon now; a front it keeps there runs only over
        // ground its units occupy (its garrison's retreat took Alpha's ground at war).
        bool only_occupied = true;
        for (const front& fr : nation_fronts(w, f.b))
            for (const entity_id t : fr.span)
                if (w.tiles.at(t).body == f.moon)
                    only_occupied = only_occupied && owner_of(w, t) != f.b && w.tile_controller(t) == f.b;
        check(w.nations.at(f.b).capital_tile == b_cap && only_occupied
                  && offworld_base_capital(w, f.b, f.moon) == null_entity,
              "W7b", "Beta keeps its home capital; it owns no moon ground (any moon front of its runs over "
                     "ground it occupies) and has no capital on the moon any more");
    }

    // -----------------------------------------------------------------------
    std::printf("\nW8 a ceded home capital stays on the home body\n");
    {
        fixture h;
        build(h);
        if (!found_both(h))
            check(false, "W8", "fixture: bases not founded");
        else
        {
            const uint32_t cap_p = h.w.provinces.province_of(h.home_cap[1]);
            // A second home province for Beta, with no centre on it: this fixture's
            // home world has two provinces, so Alpha's is handed to Beta whole and its
            // centre removed. The only centre Beta then holds off its capital province
            // is the moon base, so without the same-body rule the capital would move
            // to the moon.
            const uint32_t a_cap_p = h.w.provinces.province_of(h.home_cap[0]);
            if (const province* pr = h.w.provinces.find(a_cap_p); pr != nullptr && a_cap_p != cap_p)
            {
                const std::set<entity_id> in(pr->tiles.begin(), pr->tiles.end());
                for (const entity_id t : pr->tiles)
                {
                    h.w.set_tile_nation(t, h.b);
                    h.w.nations.at(h.b).tiles.push_back(t);
                }
                std::vector<entity_id> gone;
                for (const auto& [cid, t] : h.w.population_centre_tile)
                    if (in.count(t)) gone.push_back(cid);
                for (const entity_id cid : gone)
                {
                    h.w.population_centres.erase(cid);
                    h.w.population_centre_tile.erase(cid);
                }
            }
            std::set<uint32_t> b_home;
            for (const entity_id t : h.w.nations.at(h.b).tiles)
                if (h.w.tiles.at(t).body == h.home)
                    if (const uint32_t p = h.w.provinces.province_of(t); p != 0 && p != cap_p)
                        b_home.insert(p);
            const sentiment_params sp{};
            (void)declare_war(h.w, sp, h.a, h.b, {cap_p});
            cede_provinces(h.w, h.b, h.a, {cap_p});
            const entity_id nc = h.w.nations.at(h.b).capital_tile;
            const auto nt = h.w.tiles.find(nc);
            check(!b_home.empty() && nt != h.w.tiles.end() && nt->second.body == h.home && owner_of(h.w, nc) == h.b,
                  "W8a", fmt("Beta's capital moved to its own home ground (%zu home provinces left), not to the moon base",
                             b_home.size()));
        }
    }

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
