// ---------------------------------------------------------------------------
// fronts_harness — BL-1427 (fronts; MILITARY.md § Fronts). No SDL/Lua.
// ---------------------------------------------------------------------------
// Requirement group `fronts`, rows R1-R4 (R5 is the UI lane's).
//
//   F1  DERIVATION on a real generated world: every nation with a land border has
//       fronts; every span tile is the nation's and borders the neighbour; spans
//       are contiguous, disjoint per neighbour, and cover the border; keys are
//       unique per nation and ordered (neighbour, key). Deterministic: twice, per
//       nation, and on a copy, the same answer. Its cost, measured.
//   F2  A RING AND A CUT on a hand-built cylinder: a border that wraps the body is
//       one closed front walked round; cut once it is one chain, cut twice it is
//       two fronts.
//   F3  THE VERBS refuse whole: every refusal leaves the world byte- and
//       hash-identical; the applied forms do what they say.
//   F4  HOLD: assigned units reach slots spaced evenly along the span; a killed
//       unit's gap is refilled; a retreated unit returns; a unit in a battle or
//       refitting is left alone.
//   F5  PUSH marches onto the neighbour's weakest adjacent TILE (empty weakest),
//       nearest the unit's slot, only while hostile; a held tile is an attack
//       (BL-1443, tile contact: re-pointed from the province grain).
//   F6  REANCHOR survives a tile flip, and drops a front that is gone.
//   F7  SAVE: assignments and orders round-trip by value; a v61 stream reads as
//       none; a row naming nothing is refused. (Offsets from the stream's end
//       step over v63's attacking-stack section, BL-1443.)
//   F8  RIVALS (real world): the scorer assigns only its own units to its own
//       fronts, never the player's; deterministic; ticks and a mid-run save/load
//       replay identically.
//
// Run: node tools/verify/build_harness.js fronts_harness --run
// ---------------------------------------------------------------------------

#include "harness_params.hpp"
#include "world/battle_system.hpp"
#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/fronts.hpp"
#include "world/hex_neighbors.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/state_industry.hpp"
#include "world/survey_system.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

int failures = 0, checks = 0;

void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

std::string to_bytes(const world& w)
{
    std::ostringstream o(std::ios::binary);
    write_world_snapshot(w, o);
    return o.str();
}

bool from_bytes(const std::string& b, world& w)
{
    std::istringstream in(b, std::ios::binary);
    return read_world_snapshot(w, in);
}

int row_named(const char* name)
{
    const auto& t = unit_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
        if (std::string(t[i].name) == name) return static_cast<int>(i);
    return -1;
}

recipe_registry make_registry()
{
    recipe_registry reg;
    military_capability_params mp = reg.military();
    // scripts/economy.lua's per-class march points (MILITARY.md § Marching).
    // BL-1445 (move by days): a class is authored as DAYS PER PLAINS TILE and the march
    // steps once a DAY. This fixture keeps the per-call speeds its rows were written
    // against (infantry 1.0 of traversal cost a call, cavalry 1.5, ranged 1.0, siege
    // 0.5), so one run_unit_march call - one day - moves a unit as one call always did:
    // days = 1 / those points, and a modern row by the cls it names. These rows are
    // about contact, battles and fronts, not speed; the authored speeds are measured in
    // move_by_days_harness.
    const float per_call[unit_class_count] = { 1.0f, 1.5f, 1.0f, 0.5f, 0.0f }; // inf, cav, ranged, siege, naval
    for (int k = 0; k < unit_class_count; ++k)
        mp.march_days_per_cls[static_cast<std::size_t>(k)] = per_call[k] > 0.0f ? 1.0f / per_call[k] : 0.0f;
    for (int c = 0; c < land_class_count; ++c)
        if (const int row = modern_row_for(static_cast<land_class>(c)); row >= 0)
            mp.march_days_per_land_class[static_cast<std::size_t>(c)] =
                mp.march_days_per_cls[static_cast<std::size_t>(unit_roster_table()[static_cast<std::size_t>(row)].cls)];
    reg.set_military(mp);
    return reg;
}

// --- an independent neighbour walk (not fronts.cpp's) -----------------------

struct grid_index
{
    std::map<entity_id, std::vector<entity_id>> raster;
    explicit grid_index(const world& w)
    {
        for (const auto& [bid, bc] : w.bodies)
            if (bc.grid_width > 0 && bc.grid_height > 0)
                raster[bid].assign(static_cast<std::size_t>(bc.grid_width * bc.grid_height), null_entity);
        for (const auto& [tid, tc] : w.tiles)
        {
            auto it = raster.find(tc.body);
            if (it == raster.end()) continue;
            const int gw = w.bodies.at(tc.body).grid_width;
            it->second[static_cast<std::size_t>(tc.grid_y * gw + tc.grid_x)] = tid;
        }
    }
    std::vector<entity_id> neighbours(const world& w, entity_id t) const
    {
        std::vector<entity_id> out;
        const tile_component& tc = w.tiles.at(t);
        const body_component& bc = w.bodies.at(tc.body);
        const auto& r = raster.at(tc.body);
        for (int s = 0; s < 6; ++s)
        {
            hex_neighbors::coord c = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, s);
            if (c.gy < 0 || c.gy >= bc.grid_height) continue;
            c.gx = (c.gx % bc.grid_width + bc.grid_width) % bc.grid_width;
            const entity_id n = r[static_cast<std::size_t>(c.gy * bc.grid_width + c.gx)];
            if (n != null_entity) out.push_back(n);
        }
        return out;
    }
};

entity_id owner(const world& w, entity_id t)
{
    const auto it = w.tile_to_nation.find(t);
    return it != w.tile_to_nation.end() ? it->second : null_entity;
}

bool same_fronts(const std::vector<front>& a, const std::vector<front>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].nation != b[i].nation || a[i].neighbour != b[i].neighbour || a[i].key != b[i].key
            || a[i].span != b[i].span)
            return false;
    return true;
}

bool same_all(const std::map<entity_id, std::vector<front>>& a,
              const std::map<entity_id, std::vector<front>>& b)
{
    if (a.size() != b.size()) return false;
    for (const auto& [n, f] : a)
    {
        const auto it = b.find(n);
        if (it == b.end() || !same_fronts(f, it->second)) return false;
    }
    return true;
}

/// Is @p span connected through hex adjacency inside itself?
bool connected(const world& w, const grid_index& g, const std::vector<entity_id>& span)
{
    if (span.empty()) return false;
    const std::set<entity_id> in(span.begin(), span.end());
    std::set<entity_id> seen{ span.front() };
    std::deque<entity_id> q{ span.front() };
    while (!q.empty())
    {
        const entity_id u = q.front(); q.pop_front();
        for (const entity_id v : g.neighbours(w, u))
            if (in.count(v) && seen.insert(v).second) q.push_back(v);
    }
    return seen.size() == in.size();
}

bool adjacent(const world& w, const grid_index& g, entity_id a, entity_id b)
{
    const auto n = g.neighbours(w, a);
    return std::find(n.begin(), n.end(), b) != n.end();
}

// --- the cylinder fixture ----------------------------------------------------
//
// A 12 x 6 body of plains. Nation A holds rows 0-2, nation B rows 3-5; nation C
// starts with nothing. Every A tile is its own province (so a slot is a tile);
// B's ground is four provinces of three columns each. A's border toward B is row
// 2, which wraps the cylinder: one closed front.

constexpr int W = 12, H = 6;

struct fixture
{
    world     w;
    entity_id body = null_entity;
    entity_id A = null_entity, B = null_entity, C = null_entity;
    std::vector<entity_id> cell; // y * W + x
    entity_id at(int x, int y) const { return cell[static_cast<std::size_t>(y * W + x)]; }
    uint32_t  block(int k) const { return at(3 * k, 3); } // B's province k's id
};

void build(fixture& f)
{
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Cylinder"; bc.type = body_type::planet; bc.grid_width = W; bc.grid_height = H;
    w.bodies[f.body] = bc;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body = f.body; tc.grid_x = x; tc.grid_y = y;
            tc.substrate = terrain_substrate::rocky; tc.landform = terrain_landform::plains;
            w.tiles[t] = tc;
            f.cell.push_back(t);
        }
    const auto add_nation = [&](const char* name) {
        const entity_id n = w.create_entity();
        nation_component nc{};
        nc.name = name; nc.treasury = 1.0e6f;
        w.nations[n] = nc;
        return n;
    };
    f.A = add_nation("Aster");
    f.B = add_nation("Brael");
    f.C = add_nation("Corvan");
    w.player_entity = f.A;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            w.set_tile_nation(f.at(x, y), y < 3 ? f.A : f.B);

    // Provinces, ascending id: A's tiles one each, then B's four blocks.
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < W; ++x)
        {
            province p; p.id = f.at(x, y); p.body = f.body; p.tiles = { f.at(x, y) };
            w.provinces.provinces.push_back(p);
        }
    for (int k = 0; k < 4; ++k)
    {
        province p; p.id = f.block(k); p.body = f.body;
        for (int y = 3; y < H; ++y)
            for (int x = 3 * k; x < 3 * k + 3; ++x)
                p.tiles.push_back(f.at(x, y));
        std::sort(p.tiles.begin(), p.tiles.end());
        w.provinces.provinces.push_back(p);
    }
    for (const province& p : w.provinces.provinces)
        for (const entity_id t : p.tiles)
            w.provinces.tile_province[t] = p.id;
}

entity_id add_unit(fixture& f, entity_id owner_nation, entity_id tile, int count = 40)
{
    const entity_id u = f.w.create_entity();
    unit_component uc{};
    uc.position = tile; uc.owner = owner_nation; uc.count = count;
    uc.type = static_cast<uint16_t>(row_named("Rifle Infantry"));
    f.w.units[u] = uc;
    return u;
}

corp_command_result cmd_assign(world& w, const recipe_registry& reg, entity_id actor, entity_id unit,
                               entity_id tile)
{
    corp_command c; c.corp = actor; c.as_nation = actor; c.verb = corp_verb::assign_front;
    c.subject = unit; c.tile = tile;
    return apply_corp_command(w, reg, c);
}

corp_command_result cmd_order(world& w, const recipe_registry& reg, entity_id actor, entity_id tile,
                              uint32_t order)
{
    corp_command c; c.corp = actor; c.as_nation = actor; c.verb = corp_verb::set_front_order;
    c.tile = tile; c.order = order;
    return apply_corp_command(w, reg, c);
}

/// One front step and one march pass, as the tick runs them (no battles here).
void step(world& w, const recipe_registry& reg, int n = 1)
{
    for (int i = 0; i < n; ++i)
    {
        advance_fronts(w, reg);
        (void)run_unit_march(w, reg, false);
        reanchor_fronts(w);
    }
}

const front* front_toward(const std::vector<front>& fs, entity_id neighbour)
{
    for (const front& f : fs)
        if (f.neighbour == neighbour) return &f;
    return nullptr;
}

std::vector<entity_id> expected_slots(const front& f, std::size_t n)
{
    std::vector<entity_id> s;
    for (std::size_t i = 0; i < n; ++i)
        s.push_back(f.span[((2 * i + 1) * f.span.size()) / (2 * n)]);
    return s;
}

double ms_since(std::chrono::steady_clock::time_point t0)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace

int main()
{
    std::printf("=== fronts (BL-1427) ===\n");
    const recipe_registry reg = make_registry();

    // -----------------------------------------------------------------------
    std::printf("\nF2 - a ring and a cut on the cylinder\n");
    {
        fixture f; build(f);
        const grid_index g(f.w);
        const std::vector<front> fa = derive_fronts(f.w, f.A);
        check(fa.size() == 1 && fa[0].neighbour == f.B && fa[0].span.size() == W,
              "F2a A's border wrapping the cylinder is ONE front of 12 tiles toward B");
        bool ring = fa.size() == 1;
        if (ring)
        {
            const auto& s = fa[0].span;
            for (std::size_t i = 0; i < s.size(); ++i)
                ring = ring && adjacent(f.w, g, s[i], s[(i + 1) % s.size()]);
            ring = ring && fa[0].key == f.at(0, 2);
        }
        check(ring, "F2b the closed front is walked round: every step adjacent, last back to first; key the lowest tile");
        const std::vector<front> fb = derive_fronts(f.w, f.B);
        check(fb.size() == 1 && fb[0].neighbour == f.A && fb[0].span.size() == W,
              "F2c and B's side of it is one front of 12 toward A");

        f.w.set_tile_nation(f.at(5, 2), f.C);
        const std::vector<front> f1 = derive_fronts(f.w, f.A);
        const front* tb1 = front_toward(f1, f.B);
        int toward_b = 0;
        for (const front& x : f1) toward_b += x.neighbour == f.B ? 1 : 0;
        bool chain = toward_b == 1 && tb1 != nullptr && tb1->span.size() == W - 1;
        if (chain)
            for (std::size_t i = 0; i + 1 < tb1->span.size(); ++i)
                chain = chain && adjacent(f.w, g, tb1->span[i], tb1->span[i + 1]);
        check(chain, "F2d cut once (one tile to C): one CHAIN of 11 toward B, walked end to end");

        f.w.set_tile_nation(f.at(0, 2), f.C);
        const std::vector<front> f2 = derive_fronts(f.w, f.A);
        std::vector<const front*> tb;
        for (const front& x : f2) if (x.neighbour == f.B) tb.push_back(&x);
        bool two = tb.size() == 2;
        if (two)
        {
            std::set<entity_id> s0(tb[0]->span.begin(), tb[0]->span.end());
            std::set<entity_id> s1(tb[1]->span.begin(), tb[1]->span.end());
            std::set<entity_id> e0, e1;
            for (int x = 1; x <= 4; ++x) e0.insert(f.at(x, 2));
            for (int x = 6; x <= 11; ++x) e1.insert(f.at(x, 2));
            two = s0 == e0 && s1 == e1 && tb[0]->key == f.at(1, 2) && tb[1]->key == f.at(6, 2);
        }
        check(two, "F2e cut twice: TWO fronts toward B (cols 1-4 keyed (1,2), cols 6-11 keyed (6,2)), in key order");
        bool toward_c = false;
        for (const front& x : f2) toward_c = toward_c || x.neighbour == f.C;
        check(toward_c, "F2f and the new neighbour C has fronts of its own (every neighbour has fronts)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nF3 - the verbs: refusals mutate nothing\n");
    {
        fixture f; build(f);
        reanchor_fronts(f.w);
        const entity_id ua = add_unit(f, f.A, f.at(3, 0));
        const entity_id ub = add_unit(f, f.B, f.at(3, 5));
        const entity_id key = f.at(0, 2);
        const auto refused = [&](corp_command_result got, corp_command_result want, const char* what,
                                 const std::string& before, uint64_t hash_before) {
            check(got == want && to_bytes(f.w) == before && f.w.state_hash(9) == hash_before, what);
        };
        std::string b = to_bytes(f.w); uint64_t h = f.w.state_hash(9);
        refused(cmd_assign(f.w, reg, f.A, 0x7FFFFF00u, key), corp_command_result::rejected_invalid,
                "F3a assign: no such unit -> rejected_invalid, world identical", b, h);
        refused(cmd_assign(f.w, reg, f.A, ub, key), corp_command_result::rejected_not_owner,
                "F3b assign: another nation's unit -> rejected_not_owner, world identical", b, h);
        refused(cmd_assign(f.w, reg, f.A, ua, f.at(3, 1)), corp_command_result::rejected_invalid,
                "F3c assign: an interior tile (no front holds it) -> rejected_invalid, identical", b, h);
        refused(cmd_assign(f.w, reg, f.A, ua, f.at(3, 3)), corp_command_result::rejected_invalid,
                "F3d assign: a tile of the neighbour's (B's front, not A's) -> rejected_invalid, identical", b, h);
        refused(cmd_assign(f.w, reg, f.A, ua, null_entity), corp_command_result::rejected_state,
                "F3e unassign an unassigned unit -> rejected_state, identical", b, h);
        refused(cmd_order(f.w, reg, f.A, key, 2), corp_command_result::rejected_invalid,
                "F3f order: order 2 -> rejected_invalid, identical", b, h);
        refused(cmd_order(f.w, reg, f.A, null_entity, 1), corp_command_result::rejected_invalid,
                "F3g order: no tile -> rejected_invalid, identical", b, h);
        refused(cmd_order(f.w, reg, f.A, f.at(3, 1), 0), corp_command_result::rejected_invalid,
                "F3h order: no such front -> rejected_invalid, identical", b, h);
        refused(cmd_order(f.w, reg, f.A, key, 1), corp_command_result::rejected_state,
                "F3i order: push toward a neighbour A is not hostile to -> rejected_state, identical", b, h);
        refused(cmd_order(f.w, reg, f.A, key, 0), corp_command_result::rejected_state,
                "F3j order: hold on a front already holding -> rejected_state, identical", b, h);
        (void)declare_hostile(f.w, f.B, f.A); // B hostile to A is not A hostile to B
        b = to_bytes(f.w); h = f.w.state_hash(9);
        refused(cmd_order(f.w, reg, f.A, key, 1), corp_command_result::rejected_state,
                "F3k order: push when only the NEIGHBOUR declared -> rejected_state (directed), identical", b, h);

        // Applied forms.
        check(cmd_assign(f.w, reg, f.A, ua, f.at(7, 2)) == corp_command_result::applied
                  && f.w.units.at(ua).front == key,
              "F3l assign by any span tile stores the front's KEY");
        b = to_bytes(f.w); h = f.w.state_hash(9);
        refused(cmd_assign(f.w, reg, f.A, ua, key), corp_command_result::rejected_state,
                "F3m assign to the same front again -> rejected_state, identical", b, h);
        (void)declare_hostile(f.w, f.A, f.B);
        check(cmd_order(f.w, reg, f.A, f.at(9, 2), 1) == corp_command_result::applied
                  && f.w.front_orders.size() == 1 && f.w.front_orders[0].key == key
                  && front_order_of(f.w, f.A, key) == front_order::push,
              "F3n push toward a hostile neighbour applies, stored under the key");
        b = to_bytes(f.w); h = f.w.state_hash(9);
        refused(cmd_order(f.w, reg, f.A, key, 1), corp_command_result::rejected_state,
                "F3o push twice -> rejected_state, identical", b, h);
        check(cmd_order(f.w, reg, f.A, key, 0) == corp_command_result::applied && f.w.front_orders.empty(),
              "F3p hold erases the entry (hold is never stored)");
        check(cmd_assign(f.w, reg, f.A, ua, null_entity) == corp_command_result::applied
                  && f.w.units.at(ua).front == null_entity,
              "F3q a null tile unassigns");
    }

    // -----------------------------------------------------------------------
    std::printf("\nF4 - hold: even slots, gaps refilled, the busy left alone\n");
    {
        fixture f; build(f);
        reanchor_fronts(f.w);
        std::vector<entity_id> us;
        for (int x : { 2, 6, 10 })
            us.push_back(add_unit(f, f.A, f.at(x, 0)));
        for (const entity_id u : us)
            (void)cmd_assign(f.w, reg, f.A, u, f.at(0, 2));
        step(f.w, reg, 8);
        const front& fr = nation_fronts(f.w, f.A).front();
        const std::vector<entity_id> s3 = expected_slots(fr, 3);
        bool at3 = true;
        for (std::size_t i = 0; i < 3; ++i)
            at3 = at3 && f.w.units.at(us[i]).position == s3[i] && f.w.units.at(us[i]).order.dest == null_entity;
        check(at3, "F4a three assigned units stand on slots 2, 6 and 10 of the 12-tile walk (evenly spaced)");

        f.w.units.erase(us[1]); // killed
        step(f.w, reg, 8);
        const std::vector<entity_id> s2 = expected_slots(fr, 2);
        check(f.w.units.at(us[0]).position == s2[0] && f.w.units.at(us[2]).position == s2[1],
              "F4b one killed: the two left re-space to slots 3 and 9 (the gap refilled)");

        f.w.units.at(us[0]).position = f.at(0, 0); // retreated, deep inside
        step(f.w, reg, 8);
        check(f.w.units.at(us[0]).position == s2[0], "F4c a retreated unit marches back to its slot");

        // In a battle: left alone.
        f.w.units.at(us[2]).position = f.at(9, 0);
        active_battle ab{};
        ab.attacker = f.A; ab.attacker_units = { us[2] };
        f.w.battles.push_back(ab);
        advance_fronts(f.w, reg);
        check(f.w.units.at(us[2]).order.dest == null_entity, "F4d a unit in a battle is given no order");
        f.w.battles.clear();
        // Refitting: left alone.
        unit_component& r = f.w.units.at(us[2]);
        r.refit_type = static_cast<uint16_t>(row_named("Levy Spear")); r.refit_days = 90;
        advance_fronts(f.w, reg);
        check(f.w.units.at(us[2]).order.dest == null_entity, "F4e a refitting unit is given no order");
        r.refit_type = k_no_refit; r.refit_days = 0;
        advance_fronts(f.w, reg);
        check(f.w.units.at(us[2]).order.dest == s2[1],
              "F4f free again, it is sent to its slot TILE (a legal march_unit; every march goes to a tile)");
        // Unassigned: never moved by a front.
        const entity_id loose = add_unit(f, f.A, f.at(4, 0));
        step(f.w, reg, 3);
        check(f.w.units.at(loose).position == f.at(4, 0) && f.w.units.at(loose).order.dest == null_entity,
              "F4g a unit nobody assigned is never moved");
    }

    // -----------------------------------------------------------------------
    std::printf("\nF5 - push: the weakest adjacent tile, only while hostile\n");
    {
        fixture f; build(f);
        reanchor_fronts(f.w);
        // B holds every tile of row 3 — the tiles with a hex side onto A's span —
        // with 30 men, save (4,3) with 10 and (1,3) with 20: (4,3) is the weakest,
        // and NOT the lowest id, so the pick is the rule's and not a tie-break's.
        for (int x = 0; x < W; ++x)
            (void)add_unit(f, f.B, f.at(x, 3), x == 4 ? 10 : x == 1 ? 20 : 30);
        std::vector<entity_id> us;
        for (int x : { 2, 6, 10 })
            us.push_back(add_unit(f, f.A, f.at(x, 0)));
        for (const entity_id u : us)
            (void)cmd_assign(f.w, reg, f.A, u, f.at(0, 2));
        const front fr = nation_fronts(f.w, f.A).front();
        int64_t want_def = 0;
        for (const auto& [id, u] : f.w.units)
            if (u.owner == f.B && u.position == f.at(4, 3))
                want_def += unit_strength(f.w, u);
        int64_t def = -1;
        check(front_push_target(f.w, fr, &def) == f.at(4, 3) && def == want_def && want_def > 0,
              "F5a the push target is B's weakest adjacent tile (4,3) (10 men), with its defence");
        step(f.w, reg, 2);
        bool held = true;
        for (const entity_id u : us)
            held = held && owner(f.w, f.w.units.at(u).position) == f.A;
        check(held && f.w.battles.empty(), "F5b while it holds, nobody enters B's ground and nothing is attacked");
        (void)declare_hostile(f.w, f.A, f.B);
        check(cmd_order(f.w, reg, f.A, fr.key, 1) == corp_command_result::applied, "F5c push applies once hostile");
        step(f.w, reg, 8);
        // Every row-3 tile is held, so marching onto the weakest is an ATTACK: the
        // march stops short and a battle opens over (4,3), A attacking B.
        bool attacked = false;
        for (const active_battle& b : f.w.battles)
            attacked = attacked || (b.tile == f.at(4, 3) && b.attacker == f.A && b.defender == f.B
                                    && !b.attacking_units.empty());
        bool none_in = true;
        for (const entity_id u : us)
            none_in = none_in && owner(f.w, f.w.units.at(u).position) == f.A;
        check(attacked && none_in,
              "F5d the push ATTACKS the weakest tile: a battle over (4,3), A on B, no A unit stepped onto B's ground");
        f.w.battles.clear(); // the fight is battle_engagement's; release the units
        (void)return_to_neutral(f.w, f.A, f.B);
        step(f.w, reg, 10);
        const std::vector<entity_id> s3 = expected_slots(fr, 3);
        bool back = true;
        for (std::size_t i = 0; i < 3; ++i) back = back && f.w.units.at(us[i]).position == s3[i];
        check(back && front_order_of(f.w, f.A, fr.key) == front_order::push,
              "F5e hostility ended: the stored push holds, and the units walk back to their slots");
    }
    {
        // EMPTY IS WEAKEST, and the tie goes to the tile nearest the unit's slot.
        // One unit: its slot is span[6]. B holds row 3 save two empty tiles — the one
        // straight below the slot, and one five columns on. Both are empty (equal
        // strength), so the nearer wins; the unit marches onto it with no fight and
        // holds what it took.
        fixture f; build(f);
        reanchor_fronts(f.w);
        const entity_id u = add_unit(f, f.A, f.at(5, 0));
        (void)cmd_assign(f.w, reg, f.A, u, f.at(0, 2));
        const front fr = nation_fronts(f.w, f.A).front();
        const entity_id slot = expected_slots(fr, 1)[0];
        const int sx = f.w.tiles.at(slot).grid_x;
        const int far_x = (sx + 5) % W;
        for (int x = 0; x < W; ++x)
            if (x != sx && x != far_x)
                (void)add_unit(f, f.B, f.at(x, 3), 30);
        (void)declare_hostile(f.w, f.A, f.B);
        (void)cmd_order(f.w, reg, f.A, fr.key, 1);
        step(f.w, reg, 10);
        check(f.w.units.at(u).position == f.at(sx, 3) && f.w.battles.empty(),
              "F5f an EMPTY tile is weakest: the unit marched onto the empty tile nearest its slot, with no fight");
        step(f.w, reg, 3);
        check(f.w.units.at(u).position == f.at(sx, 3) && f.w.units.at(u).order.dest == null_entity,
              "F5g ...and holds what it took (no further order while it stands on B's ground)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nF6 - reanchor across a tile flip\n");
    {
        fixture f; build(f);
        (void)declare_hostile(f.w, f.A, f.B);
        reanchor_fronts(f.w);
        const entity_id u = add_unit(f, f.A, f.at(4, 0));
        (void)cmd_assign(f.w, reg, f.A, u, f.at(0, 2));
        (void)cmd_order(f.w, reg, f.A, f.at(0, 2), 1);
        f.w.set_tile_nation(f.at(5, 2), f.C);
        reanchor_fronts(f.w);
        check(f.w.units.at(u).front == f.at(0, 2) && front_order_of(f.w, f.A, f.at(0, 2)) == front_order::push,
              "F6a the ring cut once keeps its key (0,2): assignment and order unmoved");
        f.w.set_tile_nation(f.at(0, 2), f.C);
        reanchor_fronts(f.w);
        check(f.w.units.at(u).front == f.at(1, 2) && f.w.front_orders.size() == 1
                  && f.w.front_orders[0].key == f.at(1, 2),
              "F6b the key tile lost: both move to the NEAREST front toward the same neighbour, keyed (1,2)");
        const std::vector<front>& now = nation_fronts(f.w, f.A);
        check(front_named(now, f.w.units.at(u).front) != nullptr
                  && front_named(now, f.w.units.at(u).front)->neighbour == f.B,
              "F6c the new key names a live front toward B");
        for (int y = 3; y < H; ++y)
            for (int x = 0; x < W; ++x)
                f.w.set_tile_nation(f.at(x, y), f.A);
        reanchor_fronts(f.w);
        check(f.w.units.at(u).front == null_entity && f.w.front_orders.empty(),
              "F6d B gone from the map: no front toward B remains, assignment and order dropped");
    }
    {
        // F6e (cold review, 2026-10-27): keys are claimed across neighbours, so a
        // span changing size can hand a front's key tile to the front toward ANOTHER
        // neighbour. The assignment and the push must stay facing the neighbour
        // they were set against. C takes one B tile under row 2 (a two-tile front
        // toward C, claimed before B's twelve), then nearly all of row 3, so the
        // front toward B shrinks and claims the shared tile first.
        fixture f; build(f);
        f.w.set_tile_nation(f.at(0, 3), f.C);
        (void)declare_hostile(f.w, f.A, f.C);
        reanchor_fronts(f.w);
        const front* fc = front_toward(nation_fronts(f.w, f.A), f.C);
        const entity_id old_key = fc != nullptr ? fc->key : null_entity;
        const entity_id u = add_unit(f, f.A, f.at(4, 0));
        (void)cmd_assign(f.w, reg, f.A, u, old_key);
        (void)cmd_order(f.w, reg, f.A, old_key, 1);
        for (int x = 1; x < W - 1; ++x)
            f.w.set_tile_nation(f.at(x, 3), f.C);
        reanchor_fronts(f.w);
        const std::vector<front>& now = nation_fronts(f.w, f.A);
        const front* fb = front_toward(now, f.B);
        check(old_key != null_entity && fb != nullptr && fb->key == old_key,
              "F6e the scenario holds: the front toward B now owns the old key tile");
        const front* uf = front_named(now, f.w.units.at(u).front);
        check(uf != nullptr && uf->neighbour == f.C,
              "F6e the assignment still faces C, not the front that took its key");
        bool push_on_c = false, push_on_b = false;
        for (const front_order_entry& e : f.w.front_orders)
            if (e.nation == f.A && e.order == front_order::push)
                if (const front* of = front_named(now, e.key))
                    (of->neighbour == f.C ? push_on_c : push_on_b) = true;
        check(push_on_c && !push_on_b, "F6e the push still faces C; no push toward B was made");
    }

    // -----------------------------------------------------------------------
    std::printf("\nF7 - the save\n");
    {
        fixture f; build(f);
        (void)declare_hostile(f.w, f.A, f.B);
        reanchor_fronts(f.w);
        const entity_id u1 = add_unit(f, f.A, f.at(2, 0));
        const entity_id u2 = add_unit(f, f.A, f.at(8, 0));
        (void)add_unit(f, f.A, f.at(5, 0)); // unassigned
        (void)cmd_assign(f.w, reg, f.A, u1, f.at(0, 2));
        (void)cmd_assign(f.w, reg, f.A, u2, f.at(0, 2));
        (void)cmd_order(f.w, reg, f.A, f.at(0, 2), 1);
        const std::string bytes = to_bytes(f.w);
        world back;
        const bool ok = from_bytes(bytes, back);
        check(ok && back.units.at(u1).front == f.at(0, 2) && back.units.at(u2).front == f.at(0, 2)
                  && back.front_orders.size() == 1 && back.front_orders[0].order == front_order::push
                  && back.state_hash(5) == f.w.state_hash(5) && to_bytes(back) == bytes,
              "F7a assignments and the push round-trip by value (hash and bytes equal)");
        check(!back.fronts_cache.built, "F7b the front cache is not in the stream (a load rebuilds it)");

        std::string v61;
        {
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_pre_fronts(f.w, o);
            v61 = o.str();
        }
        uint32_t stamped = 0;
        std::memcpy(&stamped, &v61[4], sizeof stamped);
        world old;
        bool none = from_bytes(v61, old) && stamped == k_world_save_version_pre_fronts && old.front_orders.empty();
        for (const auto& [id, uu] : old.units) none = none && uu.front == null_entity;
        // BL-1434 (v64): today's stream carries the designs section, three empty counts (12 bytes);
        // BL-1445 (v65) ends it with the march-grant section, an empty count (4 bytes).
        check(none && v61.size() == bytes.size() - (4 + 2 * 8 + 4 + 9) - 4 - 12 - 4,
              "F7c a v61 stream (the section's 33 bytes, v63's 4-byte stack count, v64's 12-byte designs section and v65's 4-byte grant count shorter) reads as no fronts");

        // A row naming nothing: the first assignment's key -> a ghost tile.
        std::string bad = bytes;
        const std::size_t tail = 4 + 12 + 4; // v63's attacking-stack section (a zero battle count), v64's empty
                                             // designs section, v65's march-grant section (a zero count)
        const std::size_t key0 = bytes.size() - tail - (4 + 9) - 2 * 8 + 4;
        const entity_id ghost = 0x7FFFFFF0u;
        std::memcpy(&bad[key0], &ghost, sizeof ghost);
        world s1;
        check(!from_bytes(bad, s1) && s1.units.empty(), "F7d an assignment naming no tile is REFUSED, destination untouched");
        // An order naming a nation that is not in the stream.
        std::string bad2 = bytes;
        std::memcpy(&bad2[bytes.size() - tail - 9], &ghost, sizeof ghost);
        world s2;
        check(!from_bytes(bad2, s2) && s2.units.empty(), "F7e an order naming no nation is REFUSED, destination untouched");
        // A stored hold (never written) is refused.
        std::string bad3 = bytes;
        bad3[bytes.size() - tail - 1] = 0;
        world s3;
        check(!from_bytes(bad3, s3) && s3.units.empty(), "F7f a stored HOLD is REFUSED (hold is never stored)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nF1 - derivation on a generated world\n");
    world w = make_hard_coded_world(no_prehistory());
    init_survey_states(w);
    (void)fold_to_ground(w);
    const grid_index g(w);
    {
        // The border, independently: (nation, neighbour) -> its border tiles.
        std::map<std::pair<entity_id, entity_id>, std::set<entity_id>> border;
        std::vector<entity_id> owned;
        for (const auto& [t, n] : w.tile_to_nation) owned.push_back(t);
        std::sort(owned.begin(), owned.end());
        for (const entity_id t : owned)
            for (const entity_id n : g.neighbours(w, t))
            {
                const entity_id a = owner(w, t), b = owner(w, n);
                if (a != null_entity && b != null_entity && a != b) border[{ a, b }].insert(t);
            }

        auto t0 = std::chrono::steady_clock::now();
        const auto all = derive_all_fronts(w);
        const double cold_ms = ms_since(t0);
        std::size_t nf = 0, ntiles = 0;
        for (const auto& [n, fs] : all) { nf += fs.size(); for (const front& f : fs) ntiles += f.span.size(); }
        std::set<entity_id> bordered;
        for (const auto& [k, s] : border) bordered.insert(k.first);
        std::printf("  [info] %zu nations with a land border, %zu with fronts; %zu fronts over %zu span tiles\n",
                    bordered.size(), all.size(), nf, ntiles);
        check(!all.empty() && all.size() == bordered.size(),
              "F1a every nation with a land border has fronts, and only those");

        bool owned_ok = true, borders_ok = true, conn_ok = true, keys_ok = true, order_ok = true;
        std::size_t uncovered = 0, covered = 0, steps = 0, adj_steps = 0;
        for (const auto& [n, fs] : all)
        {
            std::set<entity_id> keys;
            std::map<entity_id, std::set<entity_id>> seen; // neighbour -> span tiles
            for (std::size_t i = 0; i < fs.size(); ++i)
            {
                const front& f = fs[i];
                if (i > 0 && !(fs[i - 1].neighbour < f.neighbour
                               || (fs[i - 1].neighbour == f.neighbour && fs[i - 1].key < f.key)))
                    order_ok = false;
                keys_ok = keys_ok && f.nation == n && keys.insert(f.key).second
                       && std::find(f.span.begin(), f.span.end(), f.key) != f.span.end();
                for (const entity_id t : f.span)
                {
                    owned_ok = owned_ok && owner(w, t) == n;
                    bool touches = false;
                    for (const entity_id x : g.neighbours(w, t)) touches = touches || owner(w, x) == f.neighbour;
                    borders_ok = borders_ok && touches;
                    if (!seen[f.neighbour].insert(t).second) conn_ok = false; // two spans share a tile
                }
                conn_ok = conn_ok && connected(w, g, f.span);
                for (std::size_t k = 0; k + 1 < f.span.size(); ++k)
                {
                    ++steps;
                    adj_steps += adjacent(w, g, f.span[k], f.span[k + 1]) ? 1 : 0;
                }
            }
            for (const auto& [k, s] : border)
                if (k.first == n)
                    for (const entity_id t : s)
                        (seen[k.second].count(t) ? covered : uncovered) += 1;
        }
        check(owned_ok, "F1b every span tile is the nation's own");
        check(borders_ok, "F1c every span tile has a hex side onto the neighbour");
        check(conn_ok, "F1d every span is contiguous, and no two spans toward one neighbour share a tile");
        check(keys_ok, "F1e keys are unique per nation and each lies in its span");
        check(order_ok, "F1f fronts are ordered by (neighbour, key)");
        std::printf("  [info] border tiles covered %zu, uncovered %zu (a lone corner tile whose every tile is "
                    "another front's key)\n", covered, uncovered);
        check(uncovered == 0, "F1g every border tile lies in a front toward each neighbour it touches");
        std::printf("  [info] walk steps between adjacent tiles: %zu / %zu (%.1f%%; a span two tiles thick "
                    "steps sideways)\n", adj_steps, steps, steps ? 100.0 * adj_steps / steps : 100.0);

        const auto again = derive_all_fronts(w);
        bool per = true;
        for (const auto& [n, fs] : all) per = per && same_fronts(fs, derive_fronts(w, n));
        const world copy = w;
        check(same_all(all, again) && per && same_all(all, derive_all_fronts(copy)),
              "F1h deterministic: twice, per nation, and on a copy of the world, the same fronts");

        // Cost.
        t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 5; ++i) (void)derive_all_fronts(w);
        const double warm_ms = ms_since(t0) / 5.0;
        reanchor_fronts(w); // builds the cache
        t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 100; ++i) reanchor_fronts(w);
        const double fresh_us = ms_since(t0) * 10.0;
        std::printf("  [cost] derive_all_fronts: %.1f ms cold, %.1f ms warm raster; a current re-anchor "
                    "(no territory change): %.1f us\n", cold_ms, warm_ms, fresh_us);
    }

    // -----------------------------------------------------------------------
    std::printf("\nF8 - rival nations use fronts\n");
    {
        const entity_id player = player_nation(w);
        std::size_t units_total = 0;
        for (const auto& [id, u] : w.units) units_total += 1;
        std::printf("  [info] %zu units in the world; the player's nation %u\n", units_total,
                    static_cast<unsigned>(player));
        world wa = w, wb = w;
        corp_ai_params p; p.cadence_k = 1;
        {
            economy_report ra, rb;
            run_corp_strategic_step(wa, reg, ra, 1, p);
            run_corp_strategic_step(wb, reg, rb, 1, p);
        }
        std::size_t assigned = 0;
        bool own_only = true, never_player = true, capital_home = true;
        for (const auto& [id, u] : wa.units)
        {
            if (u.front == null_entity) continue;
            ++assigned;
            const std::vector<front> fs = current_fronts(wa, u.owner);
            const front* f = front_named(fs, u.front);
            own_only = own_only && f != nullptr && f->key == u.front && f->nation == u.owner;
            never_player = never_player && u.owner != player;
            const auto nit = wa.nations.find(u.owner);
            if (nit != wa.nations.end() && nit->second.capital_tile != null_entity
                && w.units.count(id) != 0)
                capital_home = capital_home
                    && wa.provinces.province_of(w.units.at(id).position)
                           != wa.provinces.province_of(nit->second.capital_tile);
        }
        for (const front_order_entry& e : wa.front_orders) never_player = never_player && e.nation != player;
        std::printf("  [info] after one evaluation: %zu units assigned, %zu fronts set to push\n",
                    assigned, wa.front_orders.size());
        check(assigned > 0, "F8a rivals assign units to fronts");
        check(own_only, "F8b every assignment is a unit's own nation's front, named by its key");
        check(never_player, "F8c the player's nation is never assigned or ordered (seated session)");
        check(capital_home, "F8d no unit standing in its nation's capital province is assigned");
        check(wa.state_hash(1) == wb.state_hash(1), "F8e the rival scoring is deterministic (twins hash alike)");

        // Ticks: twins agree, and a mid-run save/load replays as the original.
        world wc = wa;
        const auto t0 = std::chrono::steady_clock::now();
        for (int t = 2; t <= 3; ++t)
        {
            wa.current_day_tick = wa.current_econ_tick = t; (void)run_economy_step(wa, reg);
            wc.current_day_tick = wc.current_econ_tick = t; (void)run_economy_step(wc, reg);
        }
        const double tick_ms = ms_since(t0) / 4.0;
        world wl;
        const bool loaded = from_bytes(to_bytes(wa), wl);
        for (int t = 4; t <= 6; ++t)
        {
            wa.current_day_tick = wa.current_econ_tick = t; (void)run_economy_step(wa, reg);
            wl.current_day_tick = wl.current_econ_tick = t; (void)run_economy_step(wl, reg);
            wc.current_day_tick = wc.current_econ_tick = t; (void)run_economy_step(wc, reg);
        }
        check(wa.state_hash(6) == wc.state_hash(6), "F8f five economy ticks: the twins hash alike");
        check(loaded && wa.state_hash(6) == wl.state_hash(6),
              "F8g saved and loaded at tick 3, the world replays ticks 4-6 to the same hash");
        std::size_t moving = 0, still_assigned = 0;
        for (const auto& [id, u] : wa.units)
            if (u.front != null_entity) { ++still_assigned; moving += u.order.dest != null_entity ? 1 : 0; }
        std::printf("  [info] at tick 6: %zu assigned, %zu of them marching; one economy tick %.0f ms\n",
                    still_assigned, moving, tick_ms);
        // The front step's own cost on this world.
        const auto t1 = std::chrono::steady_clock::now();
        for (int i = 0; i < 20; ++i) advance_fronts(wa, reg);
        std::printf("  [cost] advance_fronts on the generated world: %.2f ms a call\n", ms_since(t1) / 20.0);
    }

    std::printf("\nF8 - a rival's hold or push (the cylinder, C seated)\n");
    for (const int b_men : { 10, 200 })
    {
        fixture f; build(f);
        f.w.player_entity = f.C; // A and B are rivals
        (void)declare_hostile(f.w, f.A, f.B);
        // B stands on every tile of row 3 (the tiles with a side onto the span), so
        // the push target is a held tile: (4,3) with b_men, the rest with 200.
        for (int x = 0; x < W; ++x)
            (void)add_unit(f, f.B, f.at(x, 3), x == 4 ? b_men : 200);
        std::vector<entity_id> us;
        for (int x : { 2, 6, 10 })
            us.push_back(add_unit(f, f.A, f.at(x, 0)));
        const entity_id c_unit = add_unit(f, f.C, f.at(0, 0)); // the seat's, on A's ground
        reanchor_fronts(f.w);
        world twin = f.w;
        corp_ai_params p; p.cadence_k = 1;
        economy_report r1, r2;
        run_corp_strategic_step(f.w, reg, r1, 1, p);
        run_corp_strategic_step(twin, reg, r2, 1, p);
        const entity_id key = f.at(0, 2);
        bool all_a = true;
        for (const entity_id u : us) all_a = all_a && f.w.units.at(u).front == key;
        const bool want_push = b_men == 10; // 3 x 40 men against 10: past 3:2; against 200: not
        char what[200];
        std::snprintf(what, sizeof what,
                      "F8%s weakest tile %d men: A assigns its three units and sets %s; the seat's unit untouched; twins agree",
                      want_push ? "h" : "i", b_men, want_push ? "PUSH" : "HOLD");
        check(all_a && front_order_of(f.w, f.A, key) == (want_push ? front_order::push : front_order::hold)
                  && f.w.units.at(c_unit).front == null_entity && f.w.state_hash(2) == twin.state_hash(2),
              what);
    }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    std::printf(failures == 0 ? "ALL PASS (0 failures)\n" : "FAILURES\n");
    return failures == 0 ? 0 : 1;
}
