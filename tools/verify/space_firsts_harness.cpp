// ---------------------------------------------------------------------------
// space_firsts_harness — BL-1606 (space firsts)
// ---------------------------------------------------------------------------
// docs/economy/ERAS.md § Firsts. A hand-built fixture: a star, a home planet, its
// moon, a market on each, and three nations (ids ascending A < B < C). Deeds are
// STAGED by writing the space stores directly — a first is a deed read from state
// (`run_space_firsts`), so the staging is the deed. Rows:
//
//   K1  each of the six deeds, met by one nation, is taken by it: the record, the
//       grant in its research pool, one `space_first` history entry tagged with it.
//   K2  a second nation meeting a taken first later gets nothing: no grant, no
//       entry, the record unchanged; a repeat pass by the taker gets nothing more.
//   K3  near misses take nothing: a satellite in transit, crew in transit, a
//       crewless lander on the moon, crew landed on a planet, a HELD haul, a lost
//       station core.
//   T1  a tie (two nations meeting one first in the same pass) goes to the lowest
//       nation id, whatever the store order.
//   G1  the grant is the Lua dial: a zero grant records and logs, adds nothing.
//   H1  the hook: a crewed unit idle in orbit is taken on the space day
//       (`advance_quarter_to`'s day path), not only by a direct call.
//   S1  save round-trip: equal hash, byte-identical, the records equal; the section
//       is the tail; the v90 prefix reads as no first taken.
//   S2  tampered sections are refused whole, the target world untouched.
//   D1  two fixtures driven identically end with equal hash and equal stream; a
//       world with no first taken hashes as one where the pass never ran.
//
// Build: node tools/verify/build_harness.js space_firsts_harness --run
// ---------------------------------------------------------------------------

#include "world/economy_system.hpp"
#include "world/nation_tech.hpp"
#include "world/recipe_registry.hpp"
#include "world/space_firsts.hpp"
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

struct fixture
{
    world     w;
    entity_id star = null_entity, home = null_entity, moon = null_entity;
    entity_id a = null_entity, b = null_entity, c = null_entity;
    entity_id home_mkt = null_entity, moon_mkt = null_entity;
    entity_id moon_tile = null_entity, moon_tile2 = null_entity, moon_tile3 = null_entity;
};

entity_id add_body(world& w, const char* name, body_type t, entity_id parent, float au)
{
    const entity_id id = w.create_entity();
    body_component bc;
    bc.name = name;
    bc.type = t;
    bc.parent = parent;
    bc.orbital_radius_au = au;
    bc.grid_width = 4;
    bc.grid_height = 2;
    w.bodies.emplace(id, bc);
    return id;
}

entity_id add_tile(world& w, entity_id body, int x, int y)
{
    const entity_id id = w.create_entity();
    tile_component tc{};
    tc.body = body;
    tc.grid_x = x;
    tc.grid_y = y;
    tc.substrate = terrain_substrate::sedimentary;
    tc.landform = terrain_landform::plains;
    tc.habitability = 1.0f;
    w.tiles.emplace(id, tc);
    return id;
}

entity_id add_market(world& w, entity_id body, entity_id tile)
{
    const entity_id id = w.create_entity();
    market_component m{};
    m.body = body;
    m.centre_tile = tile;
    for (std::size_t r = 0; r < resource_count; ++r)
        m.base_price[r] = 10.0f;
    m.price = m.base_price;
    w.markets.emplace(id, m);
    return id;
}

void build(fixture& f)
{
    world& w = f.w;
    f.star = add_body(w, "Star", body_type::star, null_entity, 0.0f);
    f.home = add_body(w, "Home", body_type::planet, null_entity, 1.0f);
    f.moon = add_body(w, "Moon", body_type::moon, f.home, 0.0026f);
    w.star_body = f.star;
    w.home_body = f.home;
    const char* names[] = {"Alpha", "Beta", "Gamma"};
    entity_id* ids[] = {&f.a, &f.b, &f.c};
    for (int i = 0; i < 3; ++i)
    {
        *ids[i] = w.create_entity();
        nation_component nc;
        nc.name = names[i];
        nc.treasury = 1000.0f;
        w.nations.emplace(*ids[i], nc);
    }
    w.player_entity = f.a;
    const entity_id ht = add_tile(w, f.home, 0, 0);
    f.moon_tile  = add_tile(w, f.moon, 0, 0);
    f.moon_tile2 = add_tile(w, f.moon, 1, 0);
    f.moon_tile3 = add_tile(w, f.moon, 2, 0);
    f.home_mkt = add_market(w, f.home, ht);
    f.moon_mkt = add_market(w, f.moon, f.moon_tile);
    w.current_day_tick = 100;
}

orbital_node node(entity_id body, orbital_node_kind k) { return orbital_node{body, k}; }

entity_id stage_asset(world& w, entity_id owner, space_asset_class cls, orbital_node n, space_asset_state st,
                      int crew = 0)
{
    const entity_id id = w.create_entity();
    space_asset a;
    a.id = id;
    a.owner = owner;
    a.cls = cls;
    a.node = n;
    a.dest.body = n.body;
    a.dest.slot = n.kind == orbital_node_kind::surface ? space_slot::surface : space_slot::orbit;
    a.state = st;
    a.crew = crew;
    if (cls == space_asset_class::satellite)
        a.role = space_satellite_role::comms;
    w.space_assets.emplace(id, a);
    return id;
}

entity_id stage_unit(world& w, entity_id owner, space_hull h, orbital_node n, space_unit_state st, int crew)
{
    const entity_id id = w.create_entity();
    space_unit u;
    u.id = id;
    u.owner = owner;
    u.hull = h;
    u.design.fill(k_no_space_item);
    u.state = st;
    u.node = n;
    u.dest = n;
    u.days_remaining = st == space_unit_state::in_transit ? 3 : 0;
    u.hull_points = 10.0f;
    u.crew = crew;
    w.space_units.emplace(id, u);
    return id;
}

void stage_haul(world& w, entity_id owner, entity_id unit, space_haul_phase ph, entity_id src, entity_id dest,
                orbital_node home)
{
    space_haul h;
    h.owner = owner;
    h.phase = ph;
    h.src_market = src;
    h.dest_market = ph == space_haul_phase::held ? null_entity : dest;
    h.home = home;
    h.target = home;
    w.space_hauls.emplace(unit, h);
}

void stage_base(world& w, entity_id owner, entity_id tile)
{
    const entity_id cid = w.create_entity();
    population_centre_component pc;
    pc.scale = 1;
    pc.population = 2;
    w.population_centres[cid] = pc;
    w.population_centre_tile[cid] = tile;
    w.set_tile_nation(tile, owner);
    offworld_base b;
    w.offworld_bases.emplace(cid, b);
}

/// Stage deed @p k for nation @p n.
void stage_deed(fixture& f, space_first_kind k, entity_id n)
{
    world& w = f.w;
    switch (k)
    {
    case space_first_kind::satellite:
        stage_asset(w, n, space_asset_class::satellite, node(f.home, orbital_node_kind::low_orbit),
                    space_asset_state::active);
        break;
    case space_first_kind::crew_in_orbit:
        stage_unit(w, n, space_hull::lander, node(f.home, orbital_node_kind::low_orbit), space_unit_state::idle, 2);
        break;
    case space_first_kind::station_core:
        stage_asset(w, n, space_asset_class::station_core, node(f.home, orbital_node_kind::high_orbit),
                    space_asset_state::active, 0);
        break;
    case space_first_kind::crew_on_moon:
        stage_unit(w, n, space_hull::lander, node(f.moon, orbital_node_kind::surface), space_unit_state::landed, 2);
        break;
    case space_first_kind::offworld_base:
        stage_base(w, n, n == f.a ? f.moon_tile : n == f.b ? f.moon_tile2 : f.moon_tile3);
        break;
    case space_first_kind::space_convoy:
    {
        const entity_id u = stage_unit(w, n, space_hull::hauler, node(f.moon, orbital_node_kind::low_orbit),
                                       space_unit_state::idle, 0);
        stage_haul(w, n, u, space_haul_phase::returning, f.home_mkt, f.moon_mkt,
                   node(f.home, orbital_node_kind::low_orbit));
        break;
    }
    }
}

int count_entries(const world& w, entity_id n)
{
    int c = 0;
    for (const world_history_entry& e : w.history_log)
        if (e.topic == history_topic::space_first && (n == null_entity || e.corp == n))
            ++c;
    return c;
}

std::int64_t pool(const world& w, entity_id n)
{
    const auto it = w.nation_tech.find(n);
    return it != w.nation_tech.end() ? it->second.pool_milli : 0;
}

const char* kind_name(std::size_t k)
{
    static const char* n[] = {"satellite", "crew_in_orbit", "station_core", "crew_on_moon", "offworld_base",
                              "space_convoy"};
    return k < space_first_kind_count ? n[k] : "?";
}

void rows_take()
{
    research_params p;
    const std::int64_t grant = static_cast<std::int64_t>(p.space_first_grant_rp) * k_milli_per_rp;
    bool k1 = true, k2 = true;
    std::string why;
    for (std::size_t k = 0; k < space_first_kind_count; ++k)
    {
        const auto kind = static_cast<space_first_kind>(k);
        fixture f;
        build(f);
        stage_deed(f, kind, f.b);
        space_firsts_report r;
        run_space_firsts(f.w, p, &r);
        const auto it = f.w.space_firsts.find(static_cast<uint8_t>(k));
        const bool ok = r.taken == 1 && f.w.space_firsts.size() == 1 && it != f.w.space_firsts.end()
                     && it->second.nation == f.b && it->second.day_tick == 100 && pool(f.w, f.b) == grant
                     && count_entries(f.w, f.b) == 1 && count_entries(f.w, null_entity) == 1
                     && f.w.history_log.back().timestamp == 100
                     && f.w.history_log.back().event.find("Beta") != std::string::npos;
        if (!ok) { k1 = false; why += " "; why += kind_name(k); }

        // K2: Gamma does the same deed later; Beta repeats it. Nothing more.
        f.w.current_day_tick = 140;
        stage_deed(f, kind, f.c);
        space_firsts_report r2;
        run_space_firsts(f.w, p, &r2);
        run_space_firsts(f.w, p, &r2);
        const bool ok2 = r2.taken == 0 && pool(f.w, f.c) == 0 && pool(f.w, f.b) == grant
                      && count_entries(f.w, null_entity) == 1 && f.w.space_firsts.at(static_cast<uint8_t>(k)).nation == f.b
                      && f.w.space_firsts.at(static_cast<uint8_t>(k)).day_tick == 100;
        if (!ok2) { k2 = false; why += " later:"; why += kind_name(k); }
    }
    check(k1, "K1", fmt("each of the six deeds taken once by its doer: record, %lld milli-RP, one log entry",
                        static_cast<long long>(grant)) + (k1 ? "" : " FAILED:" + why));
    check(k2, "K2", "a second nation meeting a taken first later, and a repeat pass, get nothing"
                        + std::string(k2 ? "" : " FAILED:" + why));

    // K3: near misses.
    {
        fixture f;
        build(f);
        world& w = f.w;
        stage_asset(w, f.a, space_asset_class::satellite, node(f.home, orbital_node_kind::low_orbit),
                    space_asset_state::in_transit);
        stage_unit(w, f.a, space_hull::lander, node(f.home, orbital_node_kind::low_orbit),
                   space_unit_state::in_transit, 2);
        stage_unit(w, f.a, space_hull::lander, node(f.moon, orbital_node_kind::surface), space_unit_state::landed, 0);
        stage_unit(w, f.a, space_hull::lander, node(f.home, orbital_node_kind::surface), space_unit_state::landed, 3);
        const entity_id hu = stage_unit(w, f.a, space_hull::hauler, node(f.home, orbital_node_kind::low_orbit),
                                        space_unit_state::idle, 0);
        stage_haul(w, f.a, hu, space_haul_phase::held, f.home_mkt, null_entity,
                   node(f.home, orbital_node_kind::low_orbit));
        stage_asset(w, f.a, space_asset_class::station_core, node(f.home, orbital_node_kind::high_orbit),
                    space_asset_state::lost, 1);
        space_firsts_report r;
        run_space_firsts(w, research_params{}, &r);
        check(r.taken == 0 && w.space_firsts.empty() && pool(w, f.a) == 0 && count_entries(w, null_entity) == 0,
              "K3", fmt("near misses take nothing (taken %d): satellite in transit, crew in transit, crewless lander "
                        "on the moon, crew landed on a planet, a held haul, a lost core", r.taken));
    }
}

void rows_tie()
{
    // Gamma's deeds staged FIRST (lower asset/unit ids), Beta's after: the lowest
    // NATION id takes every first, whatever the store order.
    fixture f;
    build(f);
    for (std::size_t k = 0; k < space_first_kind_count; ++k)
        stage_deed(f, static_cast<space_first_kind>(k), f.c);
    for (std::size_t k = 0; k < space_first_kind_count; ++k)
        stage_deed(f, static_cast<space_first_kind>(k), f.b);
    space_firsts_report r;
    run_space_firsts(f.w, research_params{}, &r);
    bool all = r.taken == 6;
    for (const auto& [k, rec] : f.w.space_firsts)
        all = all && rec.nation == f.b;
    const std::int64_t grant = static_cast<std::int64_t>(research_params{}.space_first_grant_rp) * k_milli_per_rp;
    check(all && f.w.space_firsts.size() == 6 && pool(f.w, f.c) == 0 && pool(f.w, f.b) == 6 * grant
              && count_entries(f.w, f.b) == 6 && count_entries(f.w, f.c) == 0,
          "T1", fmt("a same-pass tie goes to the lowest nation id: Beta takes all %d, Gamma none", r.taken));
}

void rows_grant_dial()
{
    fixture f;
    build(f);
    stage_deed(f, space_first_kind::satellite, f.a);
    research_params p;
    p.space_first_grant_rp = 0.0f;
    run_space_firsts(f.w, p);
    check(f.w.space_firsts.size() == 1 && pool(f.w, f.a) == 0 && count_entries(f.w, f.a) == 1,
          "G1", "a zero grant dial records and logs the first and adds no RP");
}

void rows_hook()
{
    fixture f;
    build(f);
    stage_deed(f, space_first_kind::crew_in_orbit, f.c);
    recipe_registry reg;
    f.w.construction_quarter_day = 0;
    (void)advance_quarter_to(f.w, reg, 1, 0);
    const auto it = f.w.space_firsts.find(static_cast<uint8_t>(space_first_kind::crew_in_orbit));
    check(it != f.w.space_firsts.end() && it->second.nation == f.c && count_entries(f.w, f.c) == 1,
          "H1", fmt("the space day takes the first (firsts %zu)", f.w.space_firsts.size()));
}

/// A fixture with three firsts taken, the staged deeds then cleared (the section
/// stands alone; the deeds' own stores are other lanes' sections).
void taken_fixture(fixture& f)
{
    build(f);
    stage_deed(f, space_first_kind::satellite, f.a);
    stage_deed(f, space_first_kind::station_core, f.c);
    stage_deed(f, space_first_kind::offworld_base, f.b);
    run_space_firsts(f.w, research_params{});
    f.w.space_assets.clear();
    f.w.offworld_bases.clear();
    f.w.population_centres.clear();
    f.w.population_centre_tile.clear();
}

void rows_save()
{
    fixture x;
    taken_fixture(x);
    const std::string b = to_bytes(x.w);
    world w2;
    const bool ok = from_bytes(b, w2);
    check(ok && x.w.space_firsts.size() == 3 && w2.space_firsts == x.w.space_firsts
              && w2.state_hash(0) == x.w.state_hash(0) && to_bytes(w2) == b && count_entries(w2, null_entity) == 3,
          "S1", fmt("round-trip: %zu firsts, equal hash, byte-identical, log entries kept (read %d)",
                    x.w.space_firsts.size(), ok ? 1 : 0));

    const std::size_t sec = world_save_space_firsts_section_bytes(x.w);
    const std::size_t after = world_save_space_tail_bytes_after(k_world_save_version_space_firsts, x.w); // BL-1632: BL-1614 (v92) and every later section follow
    std::ostringstream o90(std::ios::binary);
    write_world_snapshot_pre_space_firsts(x.w, o90);
    check(o90.str().size() + world_save_space_line_gap_bytes(x.w) /* the space merge: main's naval..missile sections */ + sec + after == b.size() && sec == 4 + 9 * 3, "S1b",
          fmt("the section is the tail: v90 prefix %zu + %zu == %zu", o90.str().size(), sec, b.size()));
    {
        world wp;
        fixture e;
        build(e);
        std::ostringstream oe(std::ios::binary);
        write_world_snapshot_pre_space_firsts(e.w, oe);
        // The space merge: v90 is a space-line stream - 55 wide, without main's naval..missile
        // sections - so it no longer hashes as today's world; it must still read, firsts empty.
        check(from_bytes(oe.str(), wp) && wp.space_firsts.empty()
                  && world_save_space_firsts_section_bytes(e.w) == 4,
              "S1c", "the v90 prefix reads as a world where no first is taken");
        // v90 bytes of the taken world read too, as no first taken.
        world wq;
        check(from_bytes(o90.str(), wq) && wq.space_firsts.empty(), "S1d",
              "the v90 prefix of a world with firsts reads with none taken");
    }

    // Tampers. Section: u32 n, then (u8 kind, u32 nation, i32 day_tick) per record.
    const std::size_t s0 = b.size() - sec - after;
    const std::size_t r0 = s0 + 4;
    const auto refused = [&](std::string bad) {
        world wb;
        wb.player_entity = 12345;
        return !from_bytes(bad, wb) && wb.player_entity == 12345 && wb.space_firsts.empty();
    };
    const auto put_u32 = [](std::string& s, std::size_t at, uint32_t v) { std::memcpy(&s[at], &v, 4); };
    bool all = true;
    std::string why;
    const auto expect = [&](bool r, const char* what) { all = all && r; if (!r) { why += " "; why += what; } };
    { std::string bad = b; bad[r0] = 6; expect(refused(bad), "kind-out-of-range"); }
    { std::string bad = b; bad[r0 + 9] = bad[r0]; expect(refused(bad), "duplicate-kind"); }
    { std::string bad = b; std::swap(bad[r0], bad[r0 + 9]); expect(refused(bad), "descending-kind"); }
    { std::string bad = b; put_u32(bad, r0 + 1, 0x00FFFFFFu); expect(refused(bad), "ghost-nation"); }
    { std::string bad = b; put_u32(bad, r0 + 1, 0); expect(refused(bad), "null-nation"); }
    { std::string bad = b; put_u32(bad, r0 + 5, static_cast<uint32_t>(-1)); expect(refused(bad), "tick<0"); }
    { std::string bad = b; put_u32(bad, s0, 7); expect(refused(bad), "count>6"); }
    { std::string bad = b; put_u32(bad, s0, 4); expect(refused(bad), "count-overrun"); }
    expect(refused(b.substr(0, b.size() - 1)), "truncated");
    check(all, "S2", "tampered sections refused whole, the target untouched (kind out of range, a duplicate kind, "
                     "descending kinds, a ghost or null nation, a negative tick, a bad count, truncation)"
                         + (why.empty() ? std::string() : " FAILED:" + why));
}

void rows_determinism()
{
    fixture p, q;
    taken_fixture(p);
    taken_fixture(q);
    check(p.w.state_hash(0) == q.w.state_hash(0) && to_bytes(p.w) == to_bytes(q.w), "D1",
          "two fixtures driven identically: equal hash, equal stream");
    // The same world before and after a pass that finds no deed.
    fixture e;
    build(e);
    const std::string before = to_bytes(e.w);
    const uint64_t    h0     = e.w.state_hash(0);
    run_space_firsts(e.w, research_params{});
    check(e.w.space_firsts.empty() && e.w.state_hash(0) == h0 && to_bytes(e.w) == before && e.w.history_log.empty(),
          "D1b", "no deed met: the pass writes nothing, and the world hashes and streams as before");
}

} // namespace

int main()
{
    std::printf("space_firsts_harness - BL-1606 (ERAS.md § Firsts)\n");
    rows_take();
    rows_tie();
    rows_grant_dial();
    rows_hook();
    rows_save();
    rows_determinism();
    std::printf("\n%d passed, %d failed\n%s\n", g_pass, g_fail, g_fail == 0 ? "ALL PASS" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}
