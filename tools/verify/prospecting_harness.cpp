// ---------------------------------------------------------------------------
// prospecting_harness — BL-1600 (mapping then prospecting)
// ---------------------------------------------------------------------------
// docs/ui/DISCOVERY.md § Mapping, then prospecting; docs/economy/SPACE_ASSETS.md
// § Satellites, § Space stations (Observatory). A hand-built fixture on the
// space_effects_harness pattern: a star, a home planet, its moon (a 32x16 grid,
// eight survey regions in a 4x2 raster, hidden), a far planet, and two nations;
// Alpha is the player. Assets and units are STAGED by writing them directly.
// Rows:
//
//   M1  a covered Survey satellite maps 4 regions a quarter, for its owner only,
//       in raster order; mapped whole in two quarters; nothing more after.
//   M2  another nation's satellite never writes the player's per-body view.
//   M3  the player's view mirrors its mapping: scanning, regions_done = mapped,
//       no probe schedule (advance_surveys leaves it alone over 400 days).
//   P1  prospecting is refused before mapping: a landed Lander on an unmapped
//       moon prospects nothing and is counted refused; no record is made.
//   P2  a landed Lander prospects one region a quarter, nearest the landing
//       (region 0) first, only inside the mapped prefix.
//   P3  deposits are revealed only to the prospecting nation: Beta, which mapped
//       the moon too, has no tile prospected.
//   P4  an active rover prospects as a Lander does; an in-transit one does not.
//   P5  every region mapped and prospected: the body is surveyed for its nation,
//       and the player's view reads surveyed.
//   P6  the home world is mapped and prospected for every nation; the star never
//       takes a record.
//   D1  two fixtures driven identically end with equal hash and equal stream.
//   S1  save round-trip: equal hash, byte-identical, store equal; the section
//       sums the tail; the v84 prefix of a knowledge-free world reads.
//   S2  tampered section reads are refused whole: a prospected byte above the
//       mapped prefix, a byte of 2, a mapped count of 0, a wrong region total,
//       the home world as a body, keys out of order, an unknown nation.
//
// Build: node tools/verify/build_harness.js prospecting_harness --run
// ---------------------------------------------------------------------------

#include "world/space_assets.hpp"
#include "world/space_effects.hpp"
#include "world/space_units.hpp"
#include "world/stations.hpp"
#include "world/survey_system.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

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

bool from_bytes(const std::string& b, world& w)
{
    std::istringstream is(b, std::ios::binary);
    return read_world_snapshot(w, is);
}

struct fixture
{
    world     w;
    entity_id star = null_entity, home = null_entity, moon = null_entity, far = null_entity;
    entity_id a = null_entity, b = null_entity;
};

entity_id add_body(world& w, const char* name, body_type t, entity_id parent, float au, int gw, int gh)
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
    w.bodies.emplace(id, bc);
    return id;
}

void build(fixture& f)
{
    world& w = f.w;
    f.star = add_body(w, "Star", body_type::star, null_entity, 0.0f, 0, 0);
    f.home = add_body(w, "Home", body_type::planet, null_entity, 1.0f, 16, 8);
    f.moon = add_body(w, "Moon", body_type::moon, f.home, 0.0026f, 32, 16);
    f.far  = add_body(w, "Far", body_type::planet, null_entity, 2.0f, 16, 16);
    w.star_body = f.star;
    w.home_body = f.home;
    w.bodies.at(f.home).survey.phase = survey_phase::surveyed;
    for (entity_id* n : {&f.a, &f.b})
    {
        *n = w.create_entity();
        nation_component nc;
        nc.name = (n == &f.a) ? "Alpha" : "Beta";
        nc.treasury = 10000.0f;
        w.nations.emplace(*n, nc);
    }
    w.player_entity = f.a;
}

entity_id stage_sat(world& w, entity_id owner, orbital_node node, space_satellite_role role)
{
    const entity_id id = w.create_entity();
    space_asset a;
    a.id = id;
    a.owner = owner;
    a.cls = space_asset_class::satellite;
    a.dest = {node.body, space_slot::orbit};
    a.state = space_asset_state::active;
    a.node = node;
    a.role = role;
    w.space_assets.emplace(id, a);
    return id;
}

entity_id stage_rover(world& w, entity_id owner, entity_id body, space_asset_state state)
{
    const entity_id id = w.create_entity();
    space_asset a;
    a.id = id;
    a.owner = owner;
    a.cls = space_asset_class::rover;
    a.dest = {body, space_slot::surface};
    a.state = state;
    a.node = {body, orbital_node_kind::surface};
    w.space_assets.emplace(id, a);
    return id;
}

space_design lander_design()
{
    space_design d;
    d.fill(k_no_space_item);
    const auto& cat = space_item_catalogue();
    const std::pair<space_unit_slot, const char*> items[] = {
        {space_unit_slot::drive, "drive.chemical"},
        {space_unit_slot::tanks, "tanks.extended"},
        {space_unit_slot::bay, "bay.passenger"}};
    for (const auto& [slot, id] : items)
        for (std::size_t i = 0; i < cat.size(); ++i)
            if (std::strcmp(cat[i].id, id) == 0)
                d[static_cast<std::size_t>(slot)] = static_cast<uint8_t>(i);
    return d;
}

entity_id stage_lander(world& w, entity_id owner, entity_id body)
{
    const entity_id id = w.create_entity();
    space_unit u;
    u.id = id;
    u.owner = owner;
    u.hull = space_hull::lander;
    u.design = lander_design();
    u.state = space_unit_state::landed;
    u.node = {body, orbital_node_kind::surface};
    u.dest = u.node;
    const space_design_stats st = evaluate_space_design(u.hull, u.design);
    u.propellant = 0.0f;
    u.hull_points = st.hull_points;
    w.space_units.emplace(id, u);
    return id;
}

/// One QUARTER of the space-effects pass: BL-1642 made the tick a month, and the
/// per-quarter region counts are spread over its three months (quarter_count_this_tick),
/// so quarter @p q runs ticks 3q, 3q+1, 3q+2 and the rows read the quarter's sums.
space_effects_report step(world& w, int q)
{
    space_effects_report sum;
    for (int m = 0; m < k_econ_ticks_per_quarter; ++m)
    {
        const int tick = q * k_econ_ticks_per_quarter + m;
        w.current_econ_tick = tick;
        space_effects_report r;
        run_space_effects(w, space_effects_params{}, station_params{}, 1.5f, tick, &r);
        sum.survey_regions      += r.survey_regions;
        sum.observatory_regions += r.observatory_regions;
        sum.prospected_regions  += r.prospected_regions;
        sum.prospect_unmapped   += r.prospect_unmapped;
        sum.stations_fed        += r.stations_fed;
        sum.stations_starved    += r.stations_starved;
    }
    return sum;
}

int prospected_count(const world& w, entity_id n, entity_id body)
{
    const body_survey_knowledge* k = survey_knowledge_of(w, n, body);
    int c = 0;
    if (k)
        for (const uint8_t b : k->prospected)
            c += b;
    return c;
}

/// Alpha maps the moon with a covered Survey satellite (a Comms satellite gives
/// the coverage); Beta maps it too; Alpha lands a Lander. Six quarters.
void programme(fixture& f)
{
    stage_sat(f.w, f.a, {f.moon, orbital_node_kind::low_orbit}, space_satellite_role::survey);
    stage_sat(f.w, f.a, {f.moon, orbital_node_kind::high_orbit}, space_satellite_role::comms);
    stage_sat(f.w, f.b, {f.moon, orbital_node_kind::low_orbit}, space_satellite_role::survey);
    stage_sat(f.w, f.b, {f.moon, orbital_node_kind::high_orbit}, space_satellite_role::comms);
    stage_lander(f.w, f.a, f.moon);
    for (int t = 1; t <= 6; ++t)
        step(f.w, t);
}

void rows_mapping()
{
    std::printf("\nM mapping from orbit\n");
    fixture f; build(f);
    stage_sat(f.w, f.a, {f.moon, orbital_node_kind::low_orbit}, space_satellite_role::survey);
    stage_sat(f.w, f.a, {f.moon, orbital_node_kind::high_orbit}, space_satellite_role::comms);
    const auto r1 = step(f.w, 1);
    const body_survey_knowledge* k = survey_knowledge_of(f.w, f.a, f.moon);
    const bool raster = survey_tile_mapped(f.w, f.a, f.moon, 0, 0) && survey_tile_mapped(f.w, f.a, f.moon, 31, 7)
                     && !survey_tile_mapped(f.w, f.a, f.moon, 0, 8);
    check(r1.survey_regions == 4 && k && k->regions_total == 8 && k->regions_mapped == 4 && raster
              && survey_knowledge_of(f.w, f.b, f.moon) == nullptr, "M1",
          fmt("one quarter: %d of %d regions mapped for Alpha, the top region row; Beta knows nothing",
              k ? k->regions_mapped : -1, k ? k->regions_total : -1));
    step(f.w, 2);
    const auto r3 = step(f.w, 3);
    check(k->regions_mapped == 8 && r3.survey_regions == 0 && survey_tile_mapped(f.w, f.a, f.moon, 31, 15)
              && !survey_tile_prospected(f.w, f.a, f.moon, 0, 0), "M1b",
          "two quarters map it whole; a third maps nothing; no deposit revealed by mapping");

    const survey_state& s = f.w.bodies.at(f.moon).survey;
    check(s.phase == survey_phase::scanning && s.regions_done == 8 && s.ticks_remaining == k_survey_ticks_orbit_mapped,
          "M3", fmt("the player's view mirrors it: scanning, %d of %d, no probe schedule", s.regions_done, s.regions_total));
    advance_surveys(f.w, 400);
    check(f.w.bodies.at(f.moon).survey.phase == survey_phase::scanning
              && f.w.bodies.at(f.moon).survey.regions_done == 8, "M3b",
          "advance_surveys leaves an orbit-mapped body alone over 400 days");

    fixture g; build(g);
    stage_sat(g.w, g.b, {g.moon, orbital_node_kind::low_orbit}, space_satellite_role::survey);
    stage_sat(g.w, g.b, {g.moon, orbital_node_kind::high_orbit}, space_satellite_role::comms);
    step(g.w, 1);
    check(survey_knowledge_of(g.w, g.b, g.moon) != nullptr
              && g.w.bodies.at(g.moon).survey.phase == survey_phase::hidden, "M2",
          "Beta's mapping is Beta's: the player's view of the moon stays hidden");
}

void rows_prospecting()
{
    std::printf("\nP prospecting on the ground\n");
    {
        fixture f; build(f);
        stage_lander(f.w, f.a, f.moon);
        const auto r = step(f.w, 1);
        check(r.prospected_regions == 0 && r.prospect_unmapped == 1 && survey_knowledge_of(f.w, f.a, f.moon) == nullptr
                  && survey_prospect_region(f.w, f.a, f.moon) == survey_prospect_result::not_mapped, "P1",
              "a landed Lander on an unmapped moon: refused, nothing recorded");
    }
    {
        fixture f; build(f);
        stage_lander(f.w, f.a, f.moon);
        survey_map_regions(f.w, f.a, f.moon, 4); // the top row of regions, 0..3
        survey_map_regions(f.w, f.b, f.moon, 8); // Beta has mapped it all
        int order[5] = {-1, -1, -1, -1, -1};
        int r = -1;
        for (int i = 0; i < 4; ++i)
        {
            const survey_prospect_result pr = survey_prospect_region(f.w, f.a, f.moon, &r);
            order[i] = pr == survey_prospect_result::prospected ? r : -9;
        }
        const survey_prospect_result fifth = survey_prospect_region(f.w, f.a, f.moon, &r);
        check(order[0] == 0 && order[1] == 1 && order[2] == 2 && order[3] == 3
                  && fifth == survey_prospect_result::not_mapped, "P2",
              fmt("nearest the landing first: %d %d %d %d; the fifth refused (nothing mapped left)",
                  order[0], order[1], order[2], order[3]));

        survey_map_regions(f.w, f.a, f.moon, 4);
        survey_prospect_region(f.w, f.a, f.moon, &r);
        // Regions 4..7 are the second row: 4 is (0,1), distance 1, nearest.
        check(r == 4, "P2b", fmt("after more mapping, the next is region %d (0,1): squared distance 1", r));

        const auto q = step(f.w, 1);
        check(q.prospected_regions == 1 && prospected_count(f.w, f.a, f.moon) == 6, "P2c",
              fmt("the quarterly step: a landed Lander prospects one region (%d prospected)",
                  prospected_count(f.w, f.a, f.moon)));

        check(survey_tile_prospected(f.w, f.a, f.moon, 0, 0) && !survey_tile_prospected(f.w, f.b, f.moon, 0, 0)
                  && prospected_count(f.w, f.b, f.moon) == 0 && survey_tile_mapped(f.w, f.b, f.moon, 0, 0), "P3",
              "deposits revealed to Alpha only; Beta mapped the ground but sees no deposit");
    }
    {
        fixture f; build(f);
        stage_rover(f.w, f.b, f.moon, space_asset_state::in_transit);
        survey_map_regions(f.w, f.b, f.moon, 8);
        auto r = step(f.w, 1);
        const int before = prospected_count(f.w, f.b, f.moon);
        stage_rover(f.w, f.b, f.moon, space_asset_state::active);
        r = step(f.w, 2);
        check(before == 0 && r.prospected_regions == 1 && prospected_count(f.w, f.b, f.moon) == 1, "P4",
              "an in-transit rover prospects nothing; an active one prospects a region");
    }
    {
        fixture f; build(f);
        programme(f);
        for (int t = 7; t <= 12; ++t)
            step(f.w, t);
        check(survey_body_surveyed_for(f.w, f.a, f.moon) && !survey_body_surveyed_for(f.w, f.b, f.moon)
                  && f.w.bodies.at(f.moon).survey.phase == survey_phase::surveyed, "P5",
              "every region mapped and prospected: surveyed for Alpha (and the player's view), not Beta");
        const auto r = step(f.w, 13);
        check(r.prospected_regions == 0 && r.prospect_unmapped == 0, "P5b", "a surveyed body prospects nothing more");
    }
    {
        fixture f; build(f);
        check(survey_tile_mapped(f.w, f.b, f.home, 3, 3) && survey_tile_prospected(f.w, f.b, f.home, 3, 3)
                  && survey_body_surveyed_for(f.w, f.a, f.home), "P6", "the home world: surveyed for every nation");
        check(survey_map_regions(f.w, f.a, f.star, 4) == 0 && survey_map_regions(f.w, f.a, f.home, 4) == 0
                  && f.w.survey_knowledge.empty(), "P6b", "neither the star nor the home world takes a record");
    }
}

void rows_determinism_and_save()
{
    std::printf("\nD determinism / S save\n");
    fixture x; build(x); programme(x);
    fixture y; build(y); programme(y);
    check(x.w.state_hash(0) == y.w.state_hash(0) && to_bytes(x.w) == to_bytes(y.w)
              && !x.w.survey_knowledge.empty(), "D1",
          fmt("two identical programmes: equal hash %016llx and stream",
              static_cast<unsigned long long>(x.w.state_hash(0))));
    {
        fixture e; build(e);
        const std::uint64_t h0 = e.w.state_hash(0);
        survey_map_regions(e.w, e.a, e.moon, 1);
        check(e.w.state_hash(0) != h0, "D2", "the store folds into state_hash once non-empty");
    }

    const std::string b = to_bytes(x.w);
    world w2;
    const bool ok = from_bytes(b, w2);
    bool same = ok && w2.survey_knowledge.size() == x.w.survey_knowledge.size();
    if (same)
        for (const auto& [key, k] : x.w.survey_knowledge)
        {
            const auto it = w2.survey_knowledge.find(key);
            same = same && it != w2.survey_knowledge.end() && it->second.regions_total == k.regions_total
                && it->second.regions_mapped == k.regions_mapped && it->second.prospected == k.prospected;
        }
    check(same && w2.state_hash(0) == x.w.state_hash(0) && to_bytes(w2) == b, "S1",
          fmt("round-trip: %zu records; equal hash; byte-identical", x.w.survey_knowledge.size()));

    const std::size_t sec = world_save_prospecting_section_bytes(x.w);
    std::ostringstream o84(std::ios::binary);
    write_world_snapshot_pre_prospecting(x.w, o84);
    check(o84.str().size() + world_save_space_line_gap_bytes(x.w) /* the space merge: main's naval..missile sections */ + sec + (world_save_space_convoy_section_bytes(x.w) + (world_save_debris_section_bytes(x.w) + (world_save_space_bombard_section_bytes(x.w) + (world_save_offworld_base_section_bytes(x.w) + (world_save_space_firsts_section_bytes(x.w) + world_save_transit_origin_section_bytes(x.w) + world_save_launch_boost_section_bytes(x.w)))))) == b.size() && sec == 4 + 2 * (16 + 8), "S1b",
          fmt("the section is the tail: v84 prefix %zu + %zu == %zu", o84.str().size(), sec, b.size()));
    {
        fixture e; build(e);
        std::ostringstream oe(std::ios::binary);
        write_world_snapshot_pre_prospecting(e.w, oe);
        world we;
        check(from_bytes(oe.str(), we) && we.survey_knowledge.empty() && we.state_hash(0) == e.w.state_hash(0)
                  && world_save_prospecting_section_bytes(e.w) == 4, "S1c",
              "a knowledge-free world's v84 prefix reads, with nothing mapped");
    }

    // Tampers. The section: u32 count, then per record (nation, body, total,
    // mapped, total bytes). Records are (Alpha, moon), (Beta, moon).
    const std::size_t s0  = b.size() - sec - (world_save_space_convoy_section_bytes(x.w) + (world_save_debris_section_bytes(x.w) + (world_save_space_bombard_section_bytes(x.w) + (world_save_offworld_base_section_bytes(x.w) + (world_save_space_firsts_section_bytes(x.w) + world_save_transit_origin_section_bytes(x.w) + world_save_launch_boost_section_bytes(x.w)))))); // v87 follows
    const std::size_t r0  = s0 + 4;           // Alpha's record
    const std::size_t r1  = r0 + 16 + 8;      // Beta's record
    const auto refused = [&](std::string bad) {
        world wb;
        wb.player_entity = 12345; // a refused read must not touch the target
        return !from_bytes(bad, wb) && wb.player_entity == 12345;
    };
    const auto put_u32 = [](std::string& s, std::size_t at, uint32_t v) { std::memcpy(&s[at], &v, 4); };
    {
        // Beta mapped all 8 by quarter 2, so shrink Beta's mapped to 2 while its
        // prospected bytes are all 0 - legal - then set a byte above it.
        std::string bad = b;
        put_u32(bad, r1 + 12, 2);
        bad[r1 + 16 + 5] = 1;
        check(refused(bad), "S2a", "a prospected byte above the mapped prefix: refused");
    }
    {
        std::string bad = b;
        bad[r0 + 16 + 0] = 2;
        check(refused(bad), "S2b", "a prospected byte of 2: refused");
    }
    {
        std::string bad = b;
        put_u32(bad, r1 + 12, 0);
        check(refused(bad), "S2c", "a mapped count of 0: refused");
    }
    {
        std::string bad = b;
        put_u32(bad, r0 + 8, 7);
        check(refused(bad), "S2d", "a region total unlike the body's: refused");
    }
    {
        std::string bad = b;
        put_u32(bad, r0 + 4, x.home);
        check(refused(bad), "S2e", "the home world as a record's body: refused");
    }
    {
        std::string bad = b;
        put_u32(bad, r0, x.b); // (Beta, moon) twice: keys not strictly ascending
        check(refused(bad), "S2f", "keys out of order: refused");
    }
    {
        std::string bad = b;
        put_u32(bad, r0, 99999);
        check(refused(bad), "S2g", "an unknown nation: refused");
    }
    {
        std::string bad = b;
        put_u32(bad, s0, 3);
        check(refused(bad), "S2h", "a count past the stream's end: refused");
    }
}

} // namespace

int main()
{
    std::printf("prospecting_harness — BL-1600 (mapping then prospecting)\n");
    rows_mapping();
    rows_prospecting();
    rows_determinism_and_save();
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
