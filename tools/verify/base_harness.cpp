// ---------------------------------------------------------------------------
// base_harness — BL-1601 (off-world bases)
// ---------------------------------------------------------------------------
// docs/politics/NATIONS.md § Off-world bases; docs/generation/PROVINCES.md;
// docs/economy/POPULATION.md; docs/ui/DISCOVERY.md § Mapping, then prospecting.
// A hand-built fixture on the prospecting_harness / space_units_harness pattern:
// a star, a home planet (16x8, all Alpha's, a centre, a market and a staffed
// Launchpad, partitioned as generation partitions it), its airless moon (16x8,
// habitability 0, NO provinces: its tiles are made after the partition), and two
// nations; Alpha holds the far-space keys, Beta none. Units are STAGED by writing
// them directly; the colonists board and the base is founded through the space
// command seam (src/world/space_command.hpp). Rows:
//
//   E1  embarking is refused off the home body, without a pad, over the berths, by
//       another nation and on a cargo Lander; each refusal writes nothing.
//   E2  embarking draws the heads off the home centre and puts them aboard.
//   F1  founding is refused: without the key, on unmapped ground, without
//       colonists, without the kit, on water, on the home body, on a tile another
//       nation holds; each refusal writes nothing (hash and stream unchanged).
//   F2  a founding: a population centre with every colonist on the tile, the tile
//       and its whole province the nation's (province holder too), the moon
//       partitioned on this first landing, a market founded there, a Track laid,
//       the colonists and the kit spent.
//   F3  a second founding in the held province is refused (claimed).
//   B1  a building is placed on base ground by the nation's ordinary build door,
//       and refused on moon ground outside the base's province.
//   L1  life support: a stocked shelf feeds the base — water and rations leave
//       the shelf, the treasury pays — and the base reads fed.
//   L2  starvation: an empty shelf shrinks the base a quarter at a time until it
//       is razed; a razed base buys nothing more.
//   L3  colonists aboard a lost unit are pruned at the quarter.
//   D1  two fixtures driven identically end with equal hash and equal stream.
//   S1  save round-trip: equal hash, byte-identical, stores equal; the section is
//       the tail; the v87 prefix of a base-free world reads.
//   S2  tampered section reads are refused whole.
//   G1  no base and no colonist: the stores stay empty and a world without them
//       hashes as before (the empty-store guard).
//
// Build: node tools/verify/build_harness.js base_harness --run
// ---------------------------------------------------------------------------

#include "world/construction.hpp"
#include "world/economy_system.hpp"
#include "world/early_space_tree_data.hpp"
#include "world/market_clearing.hpp"
#include "world/offworld_base.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/space_command.hpp"
#include "world/space_units.hpp"
#include "world/state_industry.hpp"
#include "world/survey_system.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
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

constexpr std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

constexpr int GW = 16, GH = 8;

struct fixture
{
    world     w;
    entity_id star = null_entity, home = null_entity, moon = null_entity;
    entity_id a = null_entity, b = null_entity;
    entity_id home_centre = null_entity, home_mkt = null_entity;
    entity_id moon_tile[GW * GH] = {};
    entity_id passenger = null_entity, carrier = null_entity;
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

recipe_registry make_reg()
{
    recipe_registry reg;
    building_economics ex;
    ex.base_rate = 10.0f;
    ex.build_cost = 100.0f;
    ex.build_duration_ticks = 1.0f;
    reg.set_economics(building_type::extraction_site, ex);
    building_economics hub;
    hub.build_cost = 50.0f; // instant: the subject is what it supplies, not its build
    reg.set_economics(building_type::inland_logistics_hub, hub);
    return reg;
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

    // The home planet: all Alpha's, one centre, one market, a staffed pad.
    entity_id home_tile = null_entity;
    for (int y = 0; y < GH; ++y)
        for (int x = 0; x < GW; ++x)
        {
            const entity_id t = add_tile(w, f.home, x, y, terrain_substrate::sedimentary, 1.0f);
            if (x == 2 && y == 2)
                home_tile = t;
            w.set_tile_nation(t, f.a);
            w.nations.at(f.a).tiles.push_back(t);
        }
    w.nations.at(f.a).capital_tile = home_tile;
    f.home_centre = w.create_entity();
    population_centre_component pc;
    pc.scale = 3;
    pc.population = 50;
    w.population_centres[f.home_centre] = pc;
    w.population_centre_tile[f.home_centre] = home_tile;
    f.home_mkt = w.create_entity();
    {
        market_component m{};
        m.body = f.home;
        m.centre_tile = home_tile;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            m.base_price[r] = 10.0f;
            m.inventory[r] = 1000.0f;
        }
        m.price = m.base_price;
        w.markets.emplace(f.home_mkt, m);
    }
    {
        const entity_id pad = w.create_entity();
        building_component bc{};
        bc.tile = home_tile;
        bc.type = building_type::launchpad;
        bc.workforce_assigned = 1.0f;
        bc.ticks_remaining = 0;
        w.buildings.emplace(pad, bc);
        w.nations.at(f.a).state_assets.push_back(pad);
    }
    // The partition as generation draws it — BEFORE the moon has tiles, so the
    // moon is a body with no provinces.
    build_province_partition(w, 0x1601u);
    seed_province_holders(w);

    // The moon: regolith, habitability 0; the last column is water (an ice sea
    // stand-in) for the water refusal; one iron deposit at (5, 3).
    for (int y = 0; y < GH; ++y)
        for (int x = 0; x < GW; ++x)
        {
            const entity_id t = add_tile(w, f.moon, x, y,
                                         x == GW - 1 ? terrain_substrate::ocean : terrain_substrate::regolith, 0.0f);
            f.moon_tile[y * GW + x] = t;
        }
    {
        tile_component& tc = w.tiles.at(f.moon_tile[3 * GW + 5]);
        tc.resource_deposit[ri(resource_type::iron_ore)] = 2.0f;
        tc.resource_remaining[ri(resource_type::iron_ore)] = 1.0e6f;
    }
    // The partition warmed every body's raster index while the moon had no tiles; the
    // index is a cache of the tiles, so a fixture adding tiles afterwards drops it.
    w.body_tile_index.clear();
    grant_far_space(w, f.a);
    w.nation_tech[f.b];

    f.passenger = stage_lander(w, f.a, "bay.passenger", {f.home, orbital_node_kind::low_orbit}, space_unit_state::idle);
    f.carrier = stage_lander(w, f.a, "bay.cargo", {f.moon, orbital_node_kind::surface}, space_unit_state::landed);
    space_unit& cu = w.space_units.at(f.carrier);
    cu.cargo = 20.0f;
    space_haul h;
    h.owner = f.a;
    h.phase = space_haul_phase::held;
    h.cargo_res = resource_type::machinery;
    h.src_market = f.home_mkt;
    h.target = cu.node;
    h.home = cu.node;
    w.space_hauls.emplace(f.carrier, h);
}

entity_id mt(const fixture& f, int x, int y) { return f.moon_tile[y * GW + x]; }

space_command_outcome embark(fixture& f, entity_id nation, entity_id unit, int32_t n)
{
    space_command c;
    c.nation = nation;
    c.verb = static_cast<uint8_t>(space_verb::embark_colonists);
    c.subject = unit;
    c.slot = n;
    return apply_space_command(f.w, make_reg(), c);
}

space_command_outcome found(fixture& f, entity_id nation, entity_id tile)
{
    space_command c;
    c.nation = nation;
    c.verb = static_cast<uint8_t>(space_verb::found_base);
    c.subject = tile;
    return apply_space_command(f.w, make_reg(), c);
}

/// Board 4, land the Lander on the moon, map the moon whole.
void ready(fixture& f)
{
    embark(f, f.a, f.passenger, 4);
    space_unit& u = f.w.space_units.at(f.passenger);
    u.state = space_unit_state::landed;
    u.node = u.dest = {f.moon, orbital_node_kind::surface};
    survey_map_regions(f.w, f.a, f.moon, 99);
}

/// The full programme: ready, found at (4, 3), stock the shelf, two quarters.
void programme(fixture& f)
{
    ready(f);
    found(f, f.a, mt(f, 4, 3));
    const entity_id m = market_for_tile(f.w, mt(f, 4, 3));
    f.w.markets.at(m).inventory[ri(resource_type::water)] = 10.0f;
    f.w.markets.at(m).inventory[ri(resource_type::food_rations)] = 10.0f;
    f.w.markets.at(m).base_price[ri(resource_type::water)] = 3.0f;
    f.w.markets.at(m).base_price[ri(resource_type::food_rations)] = 5.0f;
    for (int q = 1; q <= 3; ++q)
    {
        f.w.current_econ_tick = q;
        run_offworld_base_quarter(f.w, offworld_base_params{});
    }
}

bool unchanged(const fixture& f, const std::string& bytes, std::uint64_t h)
{
    return to_bytes(f.w) == bytes && f.w.state_hash(0) == h;
}

void rows_embark()
{
    std::printf("\nE embarking colonists\n");
    fixture f; build(f);
    const std::string b0 = to_bytes(f.w);
    const std::uint64_t h0 = f.w.state_hash(0);
    bool all = true;
    std::string why;
    const auto expect = [&](bool r, const char* what) { all = all && r; if (!r) { why += " "; why += what; } };
    expect(embark(f, f.a, f.carrier, 2).base == offworld_base_result::rejected_not_lander, "cargo-lander");
    expect(embark(f, f.b, f.passenger, 2).result == space_command_result::rejected_not_owner, "owner");
    expect(embark(f, f.a, f.passenger, 7).base == offworld_base_result::rejected_no_berths, "berths");
    expect(embark(f, f.a, f.passenger, 0).result == space_command_result::rejected_invalid, "zero");
    expect(embark(f, f.a, f.passenger, 60).base == offworld_base_result::rejected_no_berths, "sixty");
    {
        space_unit& u = f.w.space_units.at(f.passenger);
        u.node = u.dest = {f.moon, orbital_node_kind::low_orbit};
        expect(embark(f, f.a, f.passenger, 2).base == offworld_base_result::rejected_wrong_node, "off-home");
        u.node = u.dest = {f.home, orbital_node_kind::low_orbit};
    }
    {
        building_component& pad = f.w.buildings.at(f.w.nations.at(f.a).state_assets.front());
        pad.workforce_assigned = 0.0f;
        expect(embark(f, f.a, f.passenger, 2).base == offworld_base_result::rejected_no_pad, "pad");
        pad.workforce_assigned = 1.0f;
    }
    check(all && unchanged(f, b0, h0), "E1",
          "refused: cargo Lander, another nation, over the berths, zero, off the home body, no pad; nothing written"
              + (why.empty() ? std::string() : " FAILED:" + why));

    const auto o = embark(f, f.a, f.passenger, 4);
    const auto o2 = embark(f, f.a, f.passenger, 2);
    const auto o3 = embark(f, f.a, f.passenger, 1);
    check(o.result == space_command_result::applied && o2.result == space_command_result::applied
              && o3.base == offworld_base_result::rejected_no_berths
              && space_unit_colonists(f.w, f.passenger) == 6
              && f.w.population_centres.at(f.home_centre).population == 44,
          "E2", fmt("4 then 2 board (6 berths full, a 7th refused); the home centre 50 -> %d; aboard %d",
                    f.w.population_centres.at(f.home_centre).population, space_unit_colonists(f.w, f.passenger)));
}

void rows_found()
{
    std::printf("\nF founding\n");
    {
        fixture f; build(f);
        bool all = true;
        std::string why;
        const auto expect = [&](bool r, const char* what) { all = all && r; if (!r) { why += " "; why += what; } };
        // No colonists yet; the left half of the moon mapped (region 0: columns 0..7).
        survey_map_regions(f.w, f.a, f.moon, 1);
        std::string b0 = to_bytes(f.w);
        std::uint64_t h0 = f.w.state_hash(0);
        expect(found(f, f.a, mt(f, 4, 3)).base == offworld_base_result::rejected_no_colonists, "colonists");
        expect(unchanged(f, b0, h0), "colonists-wrote");
        ready(f);
        f.w.survey_knowledge.erase({f.a, f.moon});
        survey_map_regions(f.w, f.a, f.moon, 1);
        b0 = to_bytes(f.w);
        h0 = f.w.state_hash(0);
        expect(found(f, f.b, mt(f, 4, 3)).base == offworld_base_result::rejected_tech_locked, "key");
        expect(found(f, f.a, mt(f, 12, 3)).base == offworld_base_result::rejected_unmapped, "unmapped");
        expect(unchanged(f, b0, h0), "key/unmapped-wrote");
        survey_map_regions(f.w, f.a, f.moon, 99);
        b0 = to_bytes(f.w);
        h0 = f.w.state_hash(0);
        expect(found(f, f.a, mt(f, GW - 1, 3)).base == offworld_base_result::rejected_invalid, "water");
        expect(found(f, f.a, f.w.nations.at(f.a).capital_tile).base == offworld_base_result::rejected_invalid, "home");
        expect(found(f, f.a, 0x00FFFFFFu).result == space_command_result::rejected_invalid, "ghost-tile");
        {
            f.w.space_units.at(f.carrier).cargo = 19.0f;
            expect(found(f, f.a, mt(f, 4, 3)).base == offworld_base_result::rejected_no_kit, "kit");
            f.w.space_units.at(f.carrier).cargo = 20.0f;
        }
        {
            f.w.set_tile_nation(mt(f, 2, 2), f.b);
            const std::string bb = to_bytes(f.w);
            const std::uint64_t hb = f.w.state_hash(0);
            expect(found(f, f.a, mt(f, 2, 2)).base == offworld_base_result::rejected_claimed, "held");
            expect(unchanged(f, bb, hb), "held-wrote");
            f.w.tile_to_nation.erase(mt(f, 2, 2));
            ++f.w.catchment_generation;
        }
        expect(unchanged(f, b0, h0), "wrote");
        check(all, "F1", "refused: no colonists, no key, unmapped, water, the home body, an unknown tile, short kit, "
                         "another nation's tile" + (why.empty() ? std::string() : " FAILED:" + why));
        check(f.w.offworld_bases.empty() && f.w.provinces.province_of(mt(f, 4, 3)) == 0 && space_unit_colonists(f.w, f.passenger) == 4,
              "F1b", "every refusal wrote nothing: no base, the moon still unpartitioned, the colonists aboard");
    }

    fixture f; build(f);
    ready(f);
    const std::size_t provinces_before = f.w.provinces.provinces.size();
    const std::size_t markets_before = f.w.markets.size();
    const entity_id home_holder = province_holder_for(f.w, f.w.provinces.province_of(f.w.nations.at(f.a).capital_tile));
    const entity_id tile = mt(f, 4, 3);
    const auto o = found(f, f.a, tile);
    const entity_id centre = o.base_centre;
    const uint32_t pid = f.w.provinces.province_of(tile);
    const province* pr = f.w.provinces.find(pid);
    int owned = 0;
    if (pr)
        for (const entity_id t : pr->tiles)
            owned += f.w.tile_controller(t) == f.a ? 1 : 0;
    const entity_id m = market_for_tile(f.w, tile);
    const bool market_ok = m != null_entity && f.w.markets.at(m).body == f.moon && f.w.markets.at(m).centre_tile == tile;
    const auto nm = f.w.population_centre_name.find(centre);
    check(o.result == space_command_result::applied && centre != null_entity
              && f.w.population_centres.at(centre).population == 4 && f.w.population_centre_tile.at(centre) == tile
              && f.w.offworld_bases.count(centre) == 1 && nm != f.w.population_centre_name.end() && nm->second == "Moon Base",
          "F2a", fmt("founded: centre %u with 4 (thousand) on the tile, named '%s'", centre,
                     nm != f.w.population_centre_name.end() ? nm->second.c_str() : "?"));
    check(pr != nullptr && pr->body == f.moon && f.w.provinces.provinces.size() > provinces_before
              && province_holder_for(f.w, pid) == f.a && owned == static_cast<int>(pr->tiles.size()) && owned > 1
              && province_holder_for(f.w, f.w.provinces.province_of(f.w.nations.at(f.a).capital_tile)) == home_holder,
          "F2b", fmt("the moon partitioned on first landing (%zu -> %zu provinces); province %u (%zu tiles) all Alpha's, "
                     "held by Alpha; the home world's holders unmoved",
                     provinces_before, f.w.provinces.provinces.size(), pid, pr ? pr->tiles.size() : 0));
    {
        bool ascending = true;
        for (std::size_t i = 1; i < f.w.provinces.provinces.size(); ++i)
            ascending = ascending && f.w.provinces.provinces[i - 1].id < f.w.provinces.provinces[i].id;
        bool all_moon = true;
        for (int y = 0; y < GH; ++y)
            for (int x = 0; x < GW; ++x)
                all_moon = all_moon && f.w.provinces.province_of(mt(f, x, y)) != 0;
        check(ascending && all_moon && f.w.province_holder.size() == f.w.provinces.provinces.size(), "F2c",
              "the splice keeps ascending ids, every moon tile has a province, the holders stay aligned");
    }
    check(market_ok && f.w.markets.size() == markets_before + 1 && tile_has_road(f.w.tiles.at(tile)), "F2d",
          fmt("a market founded on the base tile (the body had none: %d, %zu -> %zu); a Track laid on it (edges %u)", market_ok ? 1 : 0, markets_before, f.w.markets.size(), static_cast<unsigned>(f.w.tiles.at(tile).road_edges)));
    check(space_unit_colonists(f.w, f.passenger) == 0 && f.w.space_colonists.empty()
              && f.w.space_units.at(f.carrier).cargo == 0.0f && f.w.space_hauls.count(f.carrier) == 0
              && f.w.space_units.count(f.passenger) == 1,
          "F2e", "the colonists and the kit spent; the Landers stay, empty");
    check(found(f, f.a, mt(f, 5, 3)).base == offworld_base_result::rejected_claimed
              || f.w.provinces.province_of(mt(f, 5, 3)) != pid,
          "F3", "a second founding in the held province is refused (claimed)");

    // B1: the ordinary build door on base ground.
    std::printf("\nB building on base ground\n");
    const recipe_registry reg = make_reg();
    entity_id built = null_entity;
    const entity_id ore = mt(f, 5, 3);
    const bool ore_in = f.w.tile_controller(ore) == f.a;
    // The founding's Track runs east to the ore tile (every neighbour costs the same,
    // so the lower side wins): the road a building needs is already there.
    const bool ore_road = tile_has_road(f.w.tiles.at(ore));
    const construction_result r = ore_in ? construct_nation(f.w, reg, f.a, ore, building_type::extraction_site,
                                                            resource_type::iron_ore, no_recipe, built)
                                         : construction_result::invalid_tile;
    check(ore_in && r == construction_result::placed && built != null_entity && f.w.buildings.at(built).tile == ore,
          "B1", fmt("an iron mine placed on base ground by construct_nation (road on it %d, build %d)",
                    ore_road ? 1 : 0, static_cast<int>(r)));
    entity_id outside = null_entity;
    for (int y = 0; y < GH && outside == null_entity; ++y)
        for (int x = 0; x < GW - 1 && outside == null_entity; ++x)
            if (f.w.tile_controller(mt(f, x, y)) == null_entity)
                outside = mt(f, x, y);
    entity_id built2 = null_entity;
    const construction_result r2 = construct_nation(f.w, reg, f.a, outside, building_type::extraction_site,
                                                    resource_type::iron_ore, no_recipe, built2);
    check(outside != null_entity && r2 != construction_result::placed, "B1b",
          fmt("refused on moon ground outside the base's province (%d)", static_cast<int>(r2)));
}

void rows_life_support()
{
    std::printf("\nL life support\n");
    fixture f; build(f);
    ready(f);
    const entity_id tile = mt(f, 4, 3);
    const auto o = found(f, f.a, tile);
    const entity_id centre = o.base_centre;
    const entity_id m = market_for_tile(f.w, tile);
    market_component& mk = f.w.markets.at(m);
    mk.inventory[ri(resource_type::water)] = 10.0f;
    mk.inventory[ri(resource_type::food_rations)] = 10.0f;
    mk.base_price[ri(resource_type::water)] = 3.0f;
    mk.base_price[ri(resource_type::food_rations)] = 5.0f;
    mk.price[ri(resource_type::water)] = 3.0f;
    mk.price[ri(resource_type::food_rations)] = 5.0f;
    const float t0 = f.w.nations.at(f.a).treasury;
    offworld_base_quarter_report r;
    run_offworld_base_quarter(f.w, offworld_base_params{}, &r);
    const float paid = t0 - f.w.nations.at(f.a).treasury;
    check(r.fed == 1 && r.unfed == 0 && mk.inventory[ri(resource_type::water)] == 6.0f
              && mk.inventory[ri(resource_type::food_rations)] == 6.0f && paid > 0.0f
              && f.w.population_centres.at(centre).population == 4 && offworld_base_fed(f.w, centre),
          "L1", fmt("fed: 4 water and 4 rations off the base's shelf (4 thousand at habitability 0), %.1f paid; reads fed", paid));

    mk.inventory[ri(resource_type::water)] = 0.5f; // short: the whole lot or nothing
    int pops[5] = {0, 0, 0, 0, 0};
    bool razed = false;
    for (int q = 0; q < 5; ++q)
    {
        run_offworld_base_quarter(f.w, offworld_base_params{}, &r);
        pops[q] = f.w.population_centres.at(centre).population;
        if (q == 3)
            razed = f.w.population_centres.at(centre).razed;
    }
    check(pops[0] == 3 && pops[1] == 2 && pops[2] == 1 && pops[3] == 0 && razed && !offworld_base_fed(f.w, centre)
              && f.w.offworld_bases.at(centre).unfed_quarters == 4 && mk.inventory[ri(resource_type::water)] == 0.5f
              && mk.inventory[ri(resource_type::food_rations)] == 6.0f,
          "L2", fmt("starving: %d %d %d %d — razed at none; nothing bought from a short shelf; a razed base buys nothing (%d)",
                    pops[0], pops[1], pops[2], pops[3], pops[4]));

    fixture g; build(g);
    embark(g, g.a, g.passenger, 3);
    g.w.space_units.erase(g.passenger);
    const std::string before = to_bytes(g.w);
    const std::uint64_t hb = g.w.state_hash(0);
    run_offworld_base_quarter(g.w, offworld_base_params{});
    check(g.w.space_colonists.empty() && to_bytes(g.w) == before && g.w.state_hash(0) == hb, "L3",
          "colonists aboard a lost unit are pruned at the quarter; they were never in the stream or the hash");
}

void rows_determinism_and_save()
{
    std::printf("\nD determinism / S save\n");
    fixture x; build(x); programme(x);
    fixture y; build(y); programme(y);
    check(!x.w.offworld_bases.empty() && x.w.state_hash(0) == y.w.state_hash(0) && to_bytes(x.w) == to_bytes(y.w),
          "D1", fmt("two identical programmes: equal hash %016llx and stream",
                    static_cast<unsigned long long>(x.w.state_hash(0))));

    // A colonist aboard too, so both halves of the section carry a record.
    stage_lander(x.w, x.a, "bay.passenger", {x.home, orbital_node_kind::low_orbit}, space_unit_state::idle);
    const entity_id second = x.w.space_units.rbegin()->first;
    embark(x, x.a, second, 3);
    const std::string b = to_bytes(x.w);
    world w2;
    const bool ok = from_bytes(b, w2);
    bool same = ok && w2.offworld_bases.size() == x.w.offworld_bases.size() && w2.space_colonists == x.w.space_colonists;
    if (same)
        for (const auto& [cid, rec] : x.w.offworld_bases)
            same = same && w2.offworld_bases.count(cid) == 1 && w2.offworld_bases.at(cid).founded_tick == rec.founded_tick
                && w2.offworld_bases.at(cid).unfed_quarters == rec.unfed_quarters;
    check(same && w2.state_hash(0) == x.w.state_hash(0) && to_bytes(w2) == b
              && w2.provinces.provinces.size() == x.w.provinces.provinces.size(),
          "S1", fmt("round-trip: %zu base, %zu colonist entry; equal hash; byte-identical; the spliced partition reloads",
                    x.w.offworld_bases.size(), x.w.space_colonists.size()));

    const std::size_t sec = world_save_offworld_base_section_bytes(x.w);
    std::ostringstream o87(std::ios::binary);
    write_world_snapshot_pre_offworld_bases(x.w, o87);
    check(o87.str().size() + sec + (world_save_space_firsts_section_bytes(x.w) + world_save_transit_origin_section_bytes(x.w)) == b.size() && sec == 8 + 12 + 8, "S1b", // BL-1606 (v91) follows
          fmt("the section is the tail: v87 prefix %zu + %zu == %zu", o87.str().size(), sec, b.size()));
    {
        fixture e; build(e);
        std::ostringstream oe(std::ios::binary);
        write_world_snapshot_pre_offworld_bases(e.w, oe);
        world we;
        check(from_bytes(oe.str(), we) && we.offworld_bases.empty() && we.space_colonists.empty()
                  && we.state_hash(0) == e.w.state_hash(0) && world_save_offworld_base_section_bytes(e.w) == 8,
              "S1c", "a base-free world's v87 prefix reads, with no base and nobody aboard");
    }

    // Tampers. Section: u32 n, (centre, i32 founded, i32 unfed), u32 n, (unit, i32 count).
    const std::size_t s0 = b.size() - sec - (world_save_space_firsts_section_bytes(x.w) + world_save_transit_origin_section_bytes(x.w)); // BL-1606 (v91) follows
    const std::size_t base0 = s0 + 4;
    const std::size_t col0 = s0 + 4 + 12 + 4;
    const auto refused = [&](std::string bad) {
        world wb;
        wb.player_entity = 12345;
        return !from_bytes(bad, wb) && wb.player_entity == 12345;
    };
    const auto put_u32 = [](std::string& s, std::size_t at, uint32_t v) { std::memcpy(&s[at], &v, 4); };
    bool all = true;
    std::string why;
    const auto expect = [&](bool r, const char* what) { all = all && r; if (!r) { why += " "; why += what; } };
    { std::string bad = b; put_u32(bad, base0, 0x00FFFFFFu); expect(refused(bad), "ghost-centre"); }
    { std::string bad = b; put_u32(bad, base0, x.home_centre); expect(refused(bad), "home-centre"); }
    { std::string bad = b; put_u32(bad, base0 + 4, static_cast<uint32_t>(-1)); expect(refused(bad), "founded<0"); }
    { std::string bad = b; put_u32(bad, base0 + 8, static_cast<uint32_t>(-1)); expect(refused(bad), "unfed<0"); }
    { std::string bad = b; put_u32(bad, col0, 0x00FFFFFFu); expect(refused(bad), "ghost-unit"); }
    { std::string bad = b; put_u32(bad, col0, x.carrier); expect(refused(bad), "cargo-bay-unit"); }
    { std::string bad = b; put_u32(bad, col0 + 4, 7); expect(refused(bad), "over-berths"); }
    { std::string bad = b; put_u32(bad, col0 + 4, 0); expect(refused(bad), "zero"); }
    { std::string bad = b; put_u32(bad, s0, 5); expect(refused(bad), "count"); }
    expect(refused(b.substr(0, b.size() - 1)), "truncated");
    check(all, "S2", "tampered sections refused whole (ghost centre, a home-world centre, negative founded/unfed, ghost "
                     "unit, a cargo Lander, over the berths, zero aboard, a bad count, truncation)"
                         + (why.empty() ? std::string() : " FAILED:" + why));
}

/// R1: the ordinary systems run on the base's body. A logistics hub on the base tile
/// (the nation's capital there: its source runs free and anchors the chain), then a
/// mine on base ground; four economy steps: the mine completes, staffs off the base's
/// people, draws the hub's points and extracts ore that clears at the moon market.
/// R1b: the same mine with no hub is dark (no Logistic Points), as at home.
void rows_economy()
{
    std::printf("\nR the economy step on base ground\n");
    const recipe_registry reg = make_reg();
    float out_with = 0.0f, out_without = 0.0f;
    bool  free_src = false, anchored = false, done = false;
    int   ceiling = -1;
    entity_id m = null_entity, ore_market = null_entity;
    for (const bool with_hub : {true, false})
    {
        fixture f; build(f);
        ready(f);
        const entity_id tile = mt(f, 4, 3);
        found(f, f.a, tile);
        entity_id hub = null_entity, mine = null_entity;
        construction_result rh = construction_result::placed;
        if (with_hub)
            rh = construct_nation(f.w, reg, f.a, tile, building_type::inland_logistics_hub, resource_type::iron_ore,
                                  no_recipe, hub);
        const construction_result rm = construct_nation(f.w, reg, f.a, mt(f, 5, 3), building_type::extraction_site,
                                                        resource_type::iron_ore, no_recipe, mine);
        if (rh != construction_result::placed || rm != construction_result::placed)
        {
            std::printf("    build refused: hub %d mine %d\n", static_cast<int>(rh), static_cast<int>(rm));
            continue;
        }
        float output = 0.0f;
        for (int q = 0; q < 4; ++q)
        {
            f.w.current_econ_tick = q + 1;
            const economy_report rep = run_economy_step(f.w, reg);
            for (const building_report& br : rep.buildings)
                if (br.building == mine)
                    output += br.output_quantity;
        }
        if (with_hub)
        {
            out_with = output;
            free_src = capital_free_source(f.w, hub);
            anchored = offworld_base_capital(f.w, f.a, f.moon) == tile;
            done = f.w.buildings.at(mine).ticks_remaining == 0;
            ceiling = province_building_ceiling(f.w, f.w.provinces.province_of(tile));
            m = market_for_tile(f.w, tile);
            ore_market = market_for_tile(f.w, mt(f, 5, 3));
        }
        else
            out_without = output;
    }
    check(done && out_with > 0.0f && free_src && anchored && m != null_entity && ore_market == m && ceiling > 2, "R1",
          fmt("a hub on the base (its free source: %d; the chain's anchor: %d), a mine beside it: completed and "
              "extracted %.2f ore over four steps, routed to the moon market; the fed base's province sustains %d",
              free_src ? 1 : 0, anchored ? 1 : 0, out_with, ceiling));
    check(out_without == 0.0f, "R1b",
          fmt("with no hub the same mine extracts %.2f: a base makes no points of its own, as no city does", out_without));
}

void rows_guard()
{
    std::printf("\nG the empty-store guard\n");
    fixture f; build(f);
    const std::uint64_t h0 = f.w.state_hash(0);
    const std::string b0 = to_bytes(f.w);
    run_offworld_base_quarter(f.w, offworld_base_params{});
    check(f.w.offworld_bases.empty() && f.w.space_colonists.empty() && f.w.state_hash(0) == h0 && to_bytes(f.w) == b0,
          "G1", "no base and nobody aboard: the quarter does nothing at all");
}

} // namespace

int main()
{
    std::printf("base_harness — BL-1601 (off-world bases)\n");
    rows_embark();
    rows_found();
    rows_life_support();
    rows_determinism_and_save();
    rows_guard();
    rows_economy();
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
