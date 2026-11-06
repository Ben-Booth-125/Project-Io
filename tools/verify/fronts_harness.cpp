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
#include "world/campaign_settle.hpp" // BL-1452: k_campaign_settle_ticks
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
#include "world/war.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
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
        (void)run_unit_march(w, reg);
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
    // F9 (BL-1441, front edge cases). (1) A one-tile salient T of A's touching both
    // B and C, and nothing else of A's touching either: T is the whole span toward B
    // AND the whole span toward C, so only one of the two can be keyed by T. The
    // other must still be a front ("Every neighbour has fronts"). (2) A rival never
    // assigns a unit to a front it cannot reach.
    std::printf("\nF9 - edge cases: a span never unnamed; a rival assigns only reachable fronts\n");
    {
        fixture f; build(f);
        world& w = f.w;
        const entity_id T = f.at(5, 2);
        for (int x = 0; x < W; ++x)
            if (x != 5)
            {
                w.tiles.at(f.at(x, 2)).substrate = terrain_substrate::ocean;
                w.set_tile_nation(f.at(x, 2), null_entity);
            }
        // T's row-3 neighbours: the first goes to C, the rest stay B's.
        const grid_index g9(w);
        std::vector<entity_id> below;
        for (const entity_id n : g9.neighbours(w, T))
            if (w.tiles.at(n).grid_y == 3) below.push_back(n);
        std::sort(below.begin(), below.end());
        const entity_id c_tile = below.front();
        w.set_tile_nation(c_tile, f.C);
        const std::vector<front> fs = derive_fronts(w, f.A);
        const front* fb = front_toward(fs, f.B);
        const front* fc = front_toward(fs, f.C);
        const bool scenario = fb != nullptr && fc != nullptr && fb->span == std::vector<entity_id>{ T }
                           && fc->span == std::vector<entity_id>{ T };
        std::printf("  [info] A has %zu fronts; toward B %s, toward C %s\n", fs.size(),
                    fb ? "present" : "MISSING", fc ? "present" : "MISSING");
        check(fs.size() == 2 && scenario,
              "F9a the salient T is the whole span toward B AND toward C: both are fronts");
        const bool keys_ok = scenario && fb->key != fc->key
                          && (fb->key == T || fc->key == T)
                          && (fb->key == T ? owner(w, fc->key) == f.C : owner(w, fb->key) == f.B);
        check(keys_ok, "F9b one is keyed by T, the other by a tile of its neighbour's ground across T");
        if (scenario)
        {
            const front* across = fb->key == T ? fc : fb;
            const entity_id u = add_unit(f, f.A, f.at(5, 0));
            check(cmd_assign(w, reg, f.A, u, across->key) == corp_command_result::applied
                      && w.units.at(u).front == across->key,
                  "F9c assign_front by the across key reaches that front, and stores its key");
            reanchor_fronts(w);
            step(w, reg, 6);
            check(w.units.at(u).front == across->key && w.units.at(u).position == T,
                  "F9d the unit holds the span's one tile, T; the assignment survives re-anchoring");
        }
    }
    {
        fixture f; build(f);
        world& w = f.w;
        w.player_entity = f.C; // A is a rival
        // A second body: one tile, A's, holding one A unit. No march leaves a body.
        const entity_id isle = w.create_entity();
        body_component bc{};
        bc.name = "Isle"; bc.type = body_type::planet; bc.grid_width = 1; bc.grid_height = 1;
        w.bodies[isle] = bc;
        const entity_id it = w.create_entity();
        tile_component tc{};
        tc.body = isle; tc.grid_x = 0; tc.grid_y = 0;
        tc.substrate = terrain_substrate::rocky; tc.landform = terrain_landform::plains;
        w.tiles[it] = tc;
        w.set_tile_nation(it, f.A);
        const entity_id stranded = add_unit(f, f.A, it);
        const entity_id home     = add_unit(f, f.A, f.at(5, 0));
        reanchor_fronts(w);
        check(!front_reachable(w, reg, stranded, nation_fronts(w, f.A).front())
                  && front_reachable(w, reg, home, nation_fronts(w, f.A).front()),
              "F9e front_reachable: the unit on the other body reaches no front; the one at home does");
        world twin = w;
        corp_ai_params p; p.cadence_k = 1;
        economy_report r1, r2;
        run_corp_strategic_step(w, reg, r1, k_campaign_settle_ticks, p);
        run_corp_strategic_step(twin, reg, r2, k_campaign_settle_ticks, p);
        check(w.units.at(stranded).front == null_entity && w.units.at(home).front != null_entity
                  && w.state_hash(2) == twin.state_hash(2),
              "F9f the rival assigns its home unit, never the stranded one (no refused march to re-issue); twins agree");
    }

    // F9g-F9j (BL-1441 fix round): a front NEVER ISSUES A MARCH THE GAME REFUSES, for
    // any unit (the player's too), at any slot - not just a lone unit's. Each case is a
    // two-unit front whose SECOND slot is refused; unit 2 carries a standing order
    // first, so "stands where it is" is observable: fixed, the order is halted and no
    // march_unit is refused; unfixed, a refused march every day while the old order
    // walks on.
    const auto two_unit_case = [&](fixture& f, const char* label, int u2x, int u2dest_x, const std::function<void(fixture&, entity_id)>& block) {
        world& w = f.w;
        const entity_id u1 = add_unit(f, f.A, f.at(2, 0));
        const entity_id u2 = add_unit(f, f.A, f.at(u2x, 0));
        reanchor_fronts(w);
        const front* fr = front_toward(nation_fronts(w, f.A), f.B);
        if (fr == nullptr) { check(false, std::string(label) + ": the front exists"); return; }
        const front fcopy = *fr;
        const std::vector<entity_id> slots = expected_slots(fcopy, 2);
        block(f, slots[1]);
        // Unit 2's standing order, to a tile in row 0 (never the slot, never blocked).
        corp_command go; go.corp = f.A; go.as_nation = f.A; go.verb = corp_verb::march_unit;
        go.subject = u2; go.tile = f.at(u2dest_x, 0);
        const bool ordered = apply_corp_command(w, reg, go) == corp_command_result::applied;
        const corp_command_result probe =
            march_check(w, f.A, u2, slots[1], null_entity, nullptr, &reg.military());
        (void)cmd_assign(w, reg, f.A, u1, fcopy.key);
        (void)cmd_assign(w, reg, f.A, u2, fcopy.key);
        int refused = 0, applied = 0;
        for (int d = 0; d < 4; ++d)
        {
            const front_step fs = advance_fronts(w, reg);
            refused += fs.marches_refused;
            applied += fs.marches_applied;
            (void)run_unit_march(w, reg);
            reanchor_fronts(w);
        }
        const tile_component& st = w.tiles.at(slots[1]);
        std::printf("  [info] %s: slot 2 at (%d,%d); march_check onto it %d; unit 2 ordered %d; 4 days: %d marches "
                    "applied, %d refused; unit 2 order %s\n", label, st.grid_x, st.grid_y, static_cast<int>(probe),
                    ordered ? 1 : 0, applied, refused,
                    w.units.at(u2).order.dest == null_entity ? "halted" : "standing");
        check(ordered && probe == corp_command_result::rejected_invalid,
              std::string(label) + ": the scenario holds - unit 2's slot is refused (rejected_invalid)");
        check(refused == 0 && w.units.at(u2).order.dest == null_entity,
              std::string(label) + ": no refused march is issued; unit 2 stands where it is, its order halted");
        check(w.units.at(u1).order.dest == slots[0] || w.units.at(u1).position == slots[0],
              std::string(label) + ": unit 1 still takes its own (reachable) slot");
    };
    {
        // SAME BODY, ANOTHER LANDMASS (BL-1466; MILITARY.md § Domains and traversal).
        // Columns 0 and 6 become ocean from pole to pole, splitting the cylinder into two
        // landmasses. A land unit west of column 6 is asked to march to (9,2), east of it.
        // A land class crosses coastal water only where owned and never open ocean, and
        // the march's PATH is solved around every tile the class may not stand on - not
        // only its destination - so the march is refused, with no path. (Before the fix
        // `march_check` accepted it: an 8-tile path with one water tile.) A march on the
        // unit's own landmass still applies, its path dry.
        fixture f; build(f);
        for (int y = 0; y < H; ++y)
            for (const int x : { 0, 6 })
                f.w.tiles.at(f.at(x, y)).substrate = terrain_substrate::ocean;
        const entity_id u = add_unit(f, f.A, f.at(4, 0));
        std::vector<entity_id> path;
        const corp_command_result r =
            march_check(f.w, f.A, u, f.at(9, 2), null_entity, &path, &reg.military());
        std::size_t wet = 0;
        for (const entity_id t : path) wet += is_water(f.w.tiles.at(t).substrate) ? 1 : 0;
        std::printf("  [info] F9g other landmass: march (4,0) -> (9,2) across the ocean column: "
                    "march_check %d, path %zu tiles, %zu of them water\n", static_cast<int>(r), path.size(), wet);
        check(r == corp_command_result::rejected_invalid && path.empty(),
              "F9g a land unit's march across an open-ocean column is refused (rejected_invalid), no path");
        std::vector<entity_id> dry;
        const corp_command_result r2 =
            march_check(f.w, f.A, u, f.at(2, 2), null_entity, &dry, &reg.military());
        std::size_t dry_wet = 0;
        for (const entity_id t : dry) dry_wet += is_water(f.w.tiles.at(t).substrate) ? 1 : 0;
        check(r2 == corp_command_result::applied && dry.size() >= 2 && dry_wet == 0,
              "F9g a march on the unit's own landmass still applies, its path crossing no water");
    }
    {
        // F9k-F9n (BL-1466 fix round; MILITARY.md § Domains and traversal, PROVINCES.md §
        // Who owns water): WATER IS OWNED PER TILE. Column 0 is ocean pole to pole (no way
        // round the cylinder) and column 6 is a strait: rows 0-2 the test's water, rows 3-5
        // ocean. A land unit at (4,1) marches to (9,1); the strait is the only crossing.
        // The province holders are seeded as `seed_province_holders` seeds them - land
        // provinces held, WATER PROVINCES NULL - so a province-holder reading of ownership
        // (the bug) reads every shore unowned, and F9k/F9n fail under it.
        enum class strait { own_coast, foreign_coast, own_ocean, foreign_bridge };
        const auto strait_case = [&](strait kind, std::size_t& wet_out) {
            fixture f; build(f);
            for (int y = 0; y < H; ++y)
                f.w.tiles.at(f.at(0, y)).substrate = terrain_substrate::ocean;
            for (int y = 3; y < H; ++y)
                f.w.tiles.at(f.at(6, y)).substrate = terrain_substrate::ocean;
            for (int y = 0; y < 3; ++y)
            {
                tile_component& t = f.w.tiles.at(f.at(6, y));
                t.substrate = (kind == strait::own_ocean) ? terrain_substrate::ocean : terrain_substrate::coast;
                if (kind == strait::foreign_coast || kind == strait::foreign_bridge)
                    f.w.set_tile_nation(f.at(6, y), f.C);
                if (kind == strait::foreign_bridge)
                    t.road_edges = road_edges_on_every_side(1);
            }
            f.w.province_holder.assign(f.w.provinces.provinces.size(), null_entity);
            for (std::size_t i = 0; i < f.w.provinces.provinces.size(); ++i)
            {
                const province& p = f.w.provinces.provinces[i];
                const tile_component& t0 = f.w.tiles.at(p.tiles.front());
                if (is_water(t0.substrate))
                {
                    // A water province holds nobody (seed_province_holders). F9l instead
                    // gives C's strait a holder that DISAGREES with its tiles (A), so a
                    // province reading would let A wade C's water: only the per-tile
                    // owner refuses it.
                    if (kind == strait::foreign_coast)
                        f.w.province_holder[i] = f.A;
                    continue;
                }
                const auto tn = f.w.tile_to_nation.find(p.tiles.front());
                if (tn != f.w.tile_to_nation.end())
                    f.w.province_holder[i] = tn->second;
            }
            const entity_id u = add_unit(f, f.A, f.at(4, 1));
            std::vector<entity_id> path;
            const corp_command_result r =
                march_check(f.w, f.A, u, f.at(9, 1), null_entity, &path, &reg.military());
            wet_out = 0;
            for (const entity_id t : path) wet_out += is_water(f.w.tiles.at(t).substrate) ? 1 : 0;
            return r;
        };
        std::size_t wet = 0;
        const corp_command_result own = strait_case(strait::own_coast, wet);
        std::printf("  [info] F9k own strait: march_check %d, %zu water tile(s) on the path\n", static_cast<int>(own), wet);
        check(own == corp_command_result::applied && wet > 0,
              "F9k a land unit's march across its OWN nation's coastal strait is accepted, the path crossing it");
        const corp_command_result foreign = strait_case(strait::foreign_coast, wet);
        check(foreign == corp_command_result::rejected_invalid,
              "F9l a march across ANOTHER nation's coastal water is refused (rejected_invalid)");
        const corp_command_result ocean = strait_case(strait::own_ocean, wet);
        check(ocean == corp_command_result::rejected_invalid,
              "F9m a march across open ocean is refused, whoever's tiles they are");
        const corp_command_result bridge = strait_case(strait::foreign_bridge, wet);
        std::printf("  [info] F9n bridged foreign strait: march_check %d, %zu water tile(s) on the path\n", static_cast<int>(bridge), wet);
        check(bridge == corp_command_result::applied && wet > 0,
              "F9n a roaded crossing (a bridge is a road) over another nation's coastal water is crossed as land");
    }
    {
        // SAME LANDMASS, A RINGED SLOT: every neighbour of slot 2 holds a stack of C's
        // (not hostile to A), and a march path never crosses a foreign-held tile.
        fixture f; build(f);
        two_unit_case(f, "F9h ringed slot", 8, 9, [&](fixture& ff, entity_id slot) {
            const grid_index gi(ff.w);
            for (const entity_id n : gi.neighbours(ff.w, slot))
                (void)add_unit(ff, ff.C, n);
        });
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
        // BL-1434 (v64): today's stream ends with the designs section, three empty counts (12 bytes).
        // BL-1449 (v65): and then the air section, six empty counts (24 bytes).
        // BL-1452 (v66): and then the carried-supplies section, one empty count (4 bytes).
        // BL-1453 (v67): and then the battle-carry section.
        check(none && v61.size() == bytes.size() - world_save_kit_goods_bytes(f.w) /* BL-1422 */ - (4 + 2 * 8 + 4 + 9) - 4 - 12 - 24 - 4
                                     - world_save_battle_carry_section_bytes(f.w)
                                     - world_save_alarm_section_bytes(f.w) // BL-1432 (v68)
                                     - world_save_war_section_bytes(f.w) // BL-1459 (v69)
                                     - world_save_embargo_section_bytes(f.w) - world_save_gen_capital_section_bytes(f.w) // BL-1418 (v70)
                                     - world_save_recruits_section_bytes(f.w) - world_save_sea_recruits_section_bytes(f.w) - world_save_refusals_section_bytes(f.w) /* BL-1480 (v75) */ - world_save_contributors_section_bytes(f.w) /* BL-1470 (v77) */ - world_save_occupation_section_bytes(f.w) /* BL-1502 (v78) */ - world_save_defend_section_bytes(f.w) /* BL-1504 (v79) */ - world_save_research_books_section_bytes(f.w) /* BL-1539 (v80), last */,
              "F7c a v61 stream (the section's 33 bytes, v63's 4-byte stack count and v64's 12-byte designs section shorter) reads as no fronts");

        // A row naming nothing: the first assignment's key -> a ghost tile.
        std::string bad = bytes;
        const std::size_t tail = 4 + 12 + 24 + 4 // v63's attacking-stack section (a zero battle count), v64's empty designs section, v65's empty air section, v66's empty supplies section
                               + world_save_battle_carry_section_bytes(f.w) // BL-1453: v67's battle carry
                               + world_save_alarm_section_bytes(f.w)       // BL-1432: v68's Alarm, last
                               + world_save_war_section_bytes(f.w)          // BL-1459: v69's wars
                               + world_save_embargo_section_bytes(f.w) + world_save_gen_capital_section_bytes(f.w)     // BL-1418: v70's embargoes
                               + world_save_recruits_section_bytes(f.w) + world_save_sea_recruits_section_bytes(f.w) + world_save_refusals_section_bytes(f.w) /* BL-1480 (v75) */ + world_save_contributors_section_bytes(f.w) /* BL-1470 (v77) */ + world_save_occupation_section_bytes(f.w) /* BL-1502 (v78) */ + world_save_defend_section_bytes(f.w) /* BL-1504 (v79) */ + world_save_research_books_section_bytes(f.w) /* BL-1539 (v80), last */;
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
                // BL-1441: a key lies in its span, or (a span every tile of which an
                // earlier span claimed) on the neighbour's ground with a side onto it.
                bool key_across = false;
                if (owner(w, f.key) == f.neighbour)
                    for (const entity_id t : f.span) key_across = key_across || adjacent(w, g, t, f.key);
                keys_ok = keys_ok && f.nation == n && keys.insert(f.key).second
                       && (std::find(f.span.begin(), f.span.end(), f.key) != f.span.end() || key_across);
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
        check(keys_ok, "F1e keys are unique per nation and each lies in its span (or across it, BL-1441)");
        check(order_ok, "F1f fronts are ordered by (neighbour, key)");
        std::printf("  [info] border tiles covered %zu, uncovered %zu (a span named across the border still covers its tiles; "
                    "a gap here is a span with no key)\n", covered, uncovered);
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
            run_corp_strategic_step(wa, reg, ra, k_campaign_settle_ticks, p); // a live tick: the settle scores no fronts (BL-1452)
            run_corp_strategic_step(wb, reg, rb, k_campaign_settle_ticks, p); // a live tick: the settle scores no fronts (BL-1452)
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
        {
            // BL-1452 (Ben, 2026-10-29): no unit is assigned to a front by default - a
            // settle tick (econ 0..11, under spectate) scores no fronts at all.
            world ws = w;
            corp_ai_params sp = p; sp.spectating = true;
            economy_report rs;
            run_corp_strategic_step(ws, reg, rs, 1, sp);
            std::size_t settle_assigned = 0;
            for (const auto& [id, u] : ws.units) settle_assigned += u.front != null_entity ? 1 : 0;
            check(settle_assigned == 0 && ws.front_orders.empty(),
                  "F8h a settle tick, spectating, assigns no unit to a front and sets no front order");
        }

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
        run_corp_strategic_step(f.w, reg, r1, k_campaign_settle_ticks, p); // a live tick: the settle scores no fronts (BL-1452)
        run_corp_strategic_step(twin, reg, r2, k_campaign_settle_ticks, p); // a live tick: the settle scores no fronts (BL-1452)
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

    // -----------------------------------------------------------------------
    // F10 - BL-1503 (MILITARY.md § Fronts, Slots): a land unit on hold never takes a
    // water slot, bridges excepted, and an all-water span leaves it where it stands.
    // The owned-coast marching rule still stands: a march by hand onto own shore applies.
    std::printf("\nF10 - hold never walks into water\n");
    {
        // Row 2 is A's border with B. Make it A's OWN shore water, save column 7 (land).
        // Every slot but that one is owned coast, which domain_wall lets a land class on.
        fixture f; build(f);
        for (int x = 0; x < W; ++x)
            if (x != 7)
                f.w.tiles.at(f.at(x, 2)).substrate = terrain_substrate::coast;
        reanchor_fronts(f.w);
        const entity_id u = add_unit(f, f.A, f.at(7, 0));
        (void)cmd_assign(f.w, reg, f.A, u, f.at(0, 2));
        step(f.w, reg, 12);
        check(f.w.units.at(u).position == f.at(7, 2),
              "F10a one land tile on a shore span: the unit takes it, not the water slot in the middle");
        const entity_id v = add_unit(f, f.A, f.at(3, 0));
        corp_command m; m.corp = f.A; m.as_nation = f.A; m.verb = corp_verb::march_unit;
        m.subject = v; m.tile = f.at(3, 2);
        check(apply_corp_command(f.w, reg, m) == corp_command_result::applied,
              "F10b the owned-coast ruling stands: a march by hand onto own shore still applies");
    }
    {
        // ALL WATER: no slot at all. The unit stands where it is, and is not reachable.
        fixture f; build(f);
        for (int x = 0; x < W; ++x)
            f.w.tiles.at(f.at(x, 2)).substrate = terrain_substrate::coast;
        reanchor_fronts(f.w);
        const entity_id u = add_unit(f, f.A, f.at(4, 0));
        const front fr = nation_fronts(f.w, f.A).front();
        check(!front_reachable(f.w, reg, u, fr), "F10c an all-water span is no front a land unit can reach");
        (void)cmd_assign(f.w, reg, f.A, u, f.at(0, 2));
        step(f.w, reg, 12);
        check(f.w.units.at(u).position == f.at(4, 0) && f.w.units.at(u).order.dest == null_entity,
              "F10d assigned anyway, it stays where it stands, with no standing order");
    }
    {
        // A BRIDGE IS A ROAD: every shore tile roaded, the slot in the middle is taken.
        fixture f; build(f);
        for (int x = 0; x < W; ++x)
        {
            tile_component& t = f.w.tiles.at(f.at(x, 2));
            t.substrate = terrain_substrate::coast;
            t.road_edges = road_edges_on_every_side(1);
        }
        reanchor_fronts(f.w);
        const entity_id u = add_unit(f, f.A, f.at(4, 0));
        (void)cmd_assign(f.w, reg, f.A, u, f.at(0, 2));
        const front fr = nation_fronts(f.w, f.A).front();
        const entity_id slot = expected_slots(fr, 1)[0];
        step(f.w, reg, 16);
        check(f.w.units.at(u).position == slot, "F10e a bridged water slot is taken: a bridge is a road");
    }

    // -----------------------------------------------------------------------
    // F11 - BL-1502 (MILITARY.md § Occupation; Ben, 2026-11-05). At war, marching past a
    // front occupies ground and moves the front. Ownership moves only at a ceding peace.
    std::printf("\nF11 - occupation moves the front\n");
    const auto open_war = [](fixture& f) { // the record and both hostility rows
        war_record rec; rec.declarer = f.A; rec.defender = f.B; rec.declarer_goals = { f.block(1) };
        f.w.wars[{ f.A, f.B }] = rec;
        (void)declare_hostile(f.w, f.A, f.B);
        (void)declare_hostile(f.w, f.B, f.A);
    };
    {
        fixture f; build(f);
        reanchor_fronts(f.w);
        const entity_id u = add_unit(f, f.A, f.at(5, 0));
        (void)cmd_assign(f.w, reg, f.A, u, f.at(0, 2));
        const entity_id legal = owner(f.w, f.at(6, 3));
        // Bare hostility is the old rule: nothing is occupied (the ruling says "at war").
        (void)declare_hostile(f.w, f.A, f.B);
        (void)cmd_order(f.w, reg, f.A, nation_fronts(f.w, f.A).front().key, 1);
        step(f.w, reg, 10);
        check(f.w.tile_occupier.empty() && owner(f.w, f.w.units.at(u).position) == f.B,
              "F11a a bare hostility takes no ground: the unit stands on B's tile, nothing occupied");
        // At war the push occupies and goes on, past the first tile.
        f.w.wars[{ f.A, f.B }] = [&] { war_record r; r.declarer = f.A; r.defender = f.B; r.declarer_goals = { f.block(1) }; return r; }();
        // The real sequence: the unit is ALREADY on B's ground when the war opens. No hand
        // on its position: the next day's push must take it on from where it stands.
        const entity_id stood_on = f.w.units.at(u).position;
        (void)declare_hostile(f.w, f.B, f.A);
        step(f.w, reg, 30);
        check(!f.w.tile_occupier.empty(), "F11b at war, the push occupies the tiles it enters");
        check(owner(f.w, f.w.units.at(u).position) == f.B && f.w.tile_controller(f.w.units.at(u).position) == f.A,
              "F11c tile_to_nation does not move: the unit stands on B's tile, occupied by A");
        check(owner(f.w, f.at(6, 3)) == legal, "F11d legal ownership of the ground is untouched");
        (void)stood_on;
        // The push kept going: it reached past B's first row.
        const int row = f.w.tiles.at(f.w.units.at(u).position).grid_y;
        check(row >= 4, "F11e the push went on past the first tile (the front moved with it)");
        // The front now runs through occupied ground, and B's front faces it.
        bool span_on_occupied = false;
        for (const front& fr : nation_fronts(f.w, f.A))
            for (const entity_id t : fr.span)
                span_on_occupied = span_on_occupied || f.w.tile_occupied(t);
        check(span_on_occupied, "F11f A's front runs along occupied ground (owner or occupier)");
        bool all_span_b = true;
        for (const front& fr : nation_fronts(f.w, f.B))
            for (const entity_id t : fr.span)
                all_span_b = all_span_b && f.w.tile_controller(t) == f.B;
        check(all_span_b, "F11g no front of B's holds a tile A occupies");
        check(f.w.units.at(u).front != null_entity, "F11h the unit's assignment followed the moving front");
        // Saved mid-occupation: by value, hash and bytes.
        const std::string b = to_bytes(f.w);
        world back;
        check(from_bytes(b, back) && back.tile_occupier == f.w.tile_occupier && to_bytes(back) == b
                  && back.state_hash(7) == f.w.state_hash(7),
              "F11i the layer round-trips by value, bytes and hash");
        world drop = f.w;
        drop.tile_occupier.clear();
        check(drop.state_hash(7) != f.w.state_hash(7), "F11j state_hash reads the layer");
        // Ghost rows are refused whole.
        {
            const std::size_t at = b.size() - world_save_research_books_section_bytes(f.w) - world_save_defend_section_bytes(f.w) - world_save_occupation_section_bytes(f.w) + 4;
            std::string ghost = b;
            const entity_id nobody = 0x7FFFFFF0u;
            std::memcpy(&ghost[at], &nobody, sizeof nobody);
            world s1; check(!from_bytes(ghost, s1) && s1.units.empty(), "F11k a row naming no tile is REFUSED");
            std::string ghost2 = b;
            std::memcpy(&ghost2[at + 4], &nobody, sizeof nobody);
            world s2; check(!from_bytes(ghost2, s2) && s2.units.empty(), "F11l a row naming no occupier is REFUSED");
            std::string own = b;
            const entity_id bn = f.B;
            std::memcpy(&own[at + 4], &bn, sizeof bn);
            world s3; check(!from_bytes(own, s3) && s3.units.empty(), "F11m a row occupying its own owner's tile is REFUSED");
        }
        {
            // A real v77 world has no layer, so no key stands on occupied ground.
            world clean = f.w;
            clean.tile_occupier.clear();
            clean.front_orders.clear();
            for (auto& [id, cu] : clean.units) cu.front = null_entity;
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_pre_occupation(clean, o);
            const std::string v77 = o.str();
            world old;
            check(from_bytes(v77, old) && old.tile_occupier.empty()
                      && v77.size() + world_save_occupation_section_bytes(clean) + world_save_defend_section_bytes(clean) + world_save_research_books_section_bytes(clean) == to_bytes(clean).size(),
                  "F11n a v77 stream reads as nothing occupied; v78 is v77 plus the section");
        }
        // A world saved on the day a tile fell replays as the unbroken one does.
        world replay;
        (void)from_bytes(b, replay);
        fixture twin_holder;
        twin_holder.w = f.w;
        step(replay, reg, 6);
        step(twin_holder.w, reg, 6);
        check(replay.state_hash(9) == twin_holder.w.state_hash(9),
              "F11o saved and loaded, six more days replay to the same hash as the unbroken run");
        // Peace clears it, and the ground goes home.
        clear_occupation_at_peace(f.w); // still at war: nothing clears
        const bool kept = !f.w.tile_occupier.empty();
        f.w.wars.clear();
        clear_occupation_at_peace(f.w);
        check(kept && f.w.tile_occupier.empty(), "F11p occupation stands while the war does, and clears at peace");
        bool home = true;
        for (const front& fr : nation_fronts(f.w, f.A))
            for (const entity_id t : fr.span)
                home = home && owner(f.w, t) == f.A;
        check(home, "F11q at peace A's fronts are back on A's own ground");
    }
    {
        // The advance occupies, the owner's entry liberates, and water is not taken.
        fixture f; build(f);
        open_war(f);
        reanchor_fronts(f.w);
        bool batch = false;
        const uint16_t rifle = static_cast<uint16_t>(row_named("Rifle Infantry"));
        const entity_id t = f.at(4, 3);
        check(occupy_on_entry(f.w, f.A, rifle, t, batch) && f.w.tile_controller(t) == f.A,
              "F11r entry occupies an empty enemy tile");
        check(!occupy_on_entry(f.w, f.A, rifle, t, batch), "F11s a second entry changes nothing");
        check(occupy_on_entry(f.w, f.B, rifle, t, batch) && !f.w.tile_occupied(t) && f.w.tile_controller(t) == f.B,
              "F11t the owner marching back on liberates it");
        f.w.tiles.at(f.at(5, 3)).substrate = terrain_substrate::coast;
        check(!occupy_on_entry(f.w, f.A, rifle, f.at(5, 3), batch), "F11u water is not ground");
        f.w.tiles.at(f.at(5, 3)).road_edges = road_edges_on_every_side(1);
        check(occupy_on_entry(f.w, f.A, rifle, f.at(5, 3), batch), "F11v a bridge is a road, and is taken");
        check(!occupy_on_entry(f.w, f.C, rifle, f.at(6, 3), batch), "F11w a third nation at war with nobody takes nothing");
        // A third party at war with the occupier only hands the tile home.
        war_record ca; ca.declarer = f.C; ca.defender = f.A; ca.declarer_goals = { f.block(1) };
        f.w.wars[{ f.C, f.A }] = ca;
        check(occupy_on_entry(f.w, f.A, rifle, f.at(7, 3), batch) && occupy_on_entry(f.w, f.C, rifle, f.at(7, 3), batch)
                  && f.w.tile_controller(f.at(7, 3)) == f.B,
              "F11x C drives A off B's tile, with no war on B: the tile goes home to B");
        reanchor_fronts(f.w);
        bool front_b = false;
        for (const front& fr : nation_fronts(f.w, f.A))
            front_b = front_b || std::find(fr.span.begin(), fr.span.end(), f.at(5, 3)) != fr.span.end();
        check(front_b, "F11y the occupied tile is in A's fronts");
    }

    // -----------------------------------------------------------------------
    // F12 - BL-1504 (MILITARY.md § Defend province; AI_OPPONENT.md § 11). A defending
    // unit reacts only inside its province, and goes home. No pursuit, no planner.
    std::printf("\nF12 - defend province\n");
    {
        // B's blocks are three columns by three rows. A unit of B's on block(1) (cols 3-5).
        fixture f; build(f);
        reanchor_fronts(f.w);
        const entity_id d = add_unit(f, f.B, f.at(4, 5));
        const uint32_t pv = f.block(1);
        const auto order = [&](world& w, entity_id actor, entity_id unit, uint32_t province) {
            corp_command c; c.corp = actor; c.as_nation = actor; c.verb = corp_verb::defend_province;
            c.subject = unit; c.province = province;
            return apply_corp_command(w, reg, c);
        };
        const auto refused = [&](corp_command_result want, entity_id actor, entity_id unit, uint32_t province, const char* what) {
            const std::string before = to_bytes(f.w);
            const uint64_t h = f.w.state_hash(3);
            check(order(f.w, actor, unit, province) == want && to_bytes(f.w) == before && f.w.state_hash(3) == h, what);
        };
        refused(corp_command_result::rejected_not_owner, f.A, d, pv, "F12a another nation's unit -> rejected_not_owner, identical");
        refused(corp_command_result::rejected_invalid, f.B, 0x7FFFFFF0u, pv, "F12b no such unit -> rejected_invalid, identical");
        refused(corp_command_result::rejected_invalid, f.B, d, 0x12345u, "F12c no such province -> rejected_invalid, identical");
        refused(corp_command_result::rejected_invalid, f.B, d, f.at(0, 0), "F12d a province with none of its ground -> rejected_invalid, identical");
        refused(corp_command_result::rejected_state, f.B, d, no_province, "F12e clearing no order -> rejected_state, identical");
        check(order(f.w, f.B, d, pv) == corp_command_result::applied && f.w.units.at(d).defend_province == pv
                  && f.w.units.at(d).defend_post == f.at(4, 5),
              "F12f the order stores the province, and the post it stands on");
        refused(corp_command_result::rejected_state, f.B, d, pv, "F12g the same order again -> rejected_state, identical");

        const auto day = [&](world& w, int n) {
            for (int i = 0; i < n; ++i)
            {
                advance_fronts(w, reg);
                advance_defenders(w, reg);
                (void)run_unit_march(w, reg);
                reanchor_fronts(w);
            }
        };
        check(!defenders_awake(f.w), "F12h at its post in a quiet province it keeps no day walking");
        day(f.w, 3);
        check(f.w.units.at(d).position == f.at(4, 5) && f.w.units.at(d).order.dest == null_entity,
              "F12i with nothing hostile it stands at its post");

        // A's stack lands in the province. Not hostile yet: nothing.
        const entity_id e = add_unit(f, f.A, f.at(3, 3), 10);
        day(f.w, 2);
        check(f.w.units.at(d).position == f.at(4, 5) && f.w.units.at(d).order.dest == null_entity,
              "F12j a stack it is not hostile to draws nothing (the trigger's own test)");
        (void)declare_hostile(f.w, f.B, f.A);
        check(defenders_awake(f.w), "F12k a hostile stack in the province wakes the day");
        advance_defenders(f.w, reg);
        check(f.w.units.at(d).order.dest == f.at(3, 3), "F12l it marches on the stack, a legal march");
        day(f.w, 6);
        bool fighting = false;
        for (const active_battle& b : f.w.battles)
            fighting = fighting || (b.tile == f.at(3, 3) && b.attacker == f.B);
        check(fighting, "F12m the march is an attack: a battle over the stack's tile");
        f.w.battles.clear(); // the fight is battle_engagement's; release the units

        // THE STACK MOVES OUT: no pursuit.
        f.w.units.at(e).position = f.at(1, 3); // block(0): next door
        day(f.w, 10);
        check(f.w.units.at(d).position == f.at(4, 5) && f.w.units.at(d).order.dest == null_entity,
              "F12n the stack gone next door: it does not pursue, and is back at its post");
        check(!defenders_awake(f.w), "F12o and the day sleeps again");

        // Save, by value and refused whole.
        const std::string b = to_bytes(f.w);
        world back;
        check(from_bytes(b, back) && back.units.at(d).defend_province == pv && back.units.at(d).defend_post == f.at(4, 5)
                  && to_bytes(back) == b && back.state_hash(4) == f.w.state_hash(4),
              "F12p the order round-trips by value, bytes and hash");
        world cleared = f.w;
        cleared.units.at(d).defend_province = no_province;
        cleared.units.at(d).defend_post = null_entity;
        check(cleared.state_hash(4) != f.w.state_hash(4), "F12q state_hash reads the order");
        {
            const std::size_t at = b.size() - world_save_research_books_section_bytes(f.w) - world_save_defend_section_bytes(f.w) + 4;
            std::string bad = b;
            const entity_id nobody = 0x7FFFFFF0u;
            std::memcpy(&bad[at], &nobody, sizeof nobody);
            world s1; check(!from_bytes(bad, s1) && s1.units.empty(), "F12r a row naming no unit is REFUSED");
            std::string bad2 = b;
            std::memcpy(&bad2[at + 4], &nobody, sizeof nobody);
            world s2; check(!from_bytes(bad2, s2) && s2.units.empty(), "F12s a row naming no province is REFUSED");
            std::string bad3 = b;
            const entity_id off = f.at(0, 0);
            std::memcpy(&bad3[at + 8], &off, sizeof off);
            world s3; check(!from_bytes(bad3, s3) && s3.units.empty(), "F12t a post outside the province is REFUSED");
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_pre_defend(f.w, o);
            world old;
            check(from_bytes(o.str(), old) && old.units.at(d).defend_province == no_province
                      && o.str().size() + world_save_defend_section_bytes(f.w) + world_save_research_books_section_bytes(f.w) == b.size(),
                  "F12u a v78 stream reads as nobody defending; v79 is v78 plus the section");
        }

        // A front and a defend order exclude each other, and a hand clear stands.
        const entity_id g = add_unit(f, f.B, f.at(4, 4));
        (void)order(f.w, f.B, g, pv);
        reanchor_fronts(f.w);
        const entity_id bkey = nation_fronts(f.w, f.B).front().key;
        check(cmd_assign(f.w, reg, f.B, g, bkey) == corp_command_result::applied
                  && f.w.units.at(g).defend_province == no_province,
              "F12v assigning it to a front clears the order");
        check(order(f.w, f.B, g, pv) == corp_command_result::applied && f.w.units.at(g).front == null_entity,
              "F12w ordering it to defend leaves the front");
        check(order(f.w, f.B, g, no_province) == corp_command_result::applied && f.w.units.at(g).defend_province == no_province,
              "F12x a clear applies");
    }
    {
        // F12 through the game's own door: a whole run_military_day, not its parts.
        fixture f; build(f);
        reanchor_fronts(f.w);
        const entity_id d = add_unit(f, f.B, f.at(3, 5));
        corp_command c; c.corp = f.B; c.as_nation = f.B; c.verb = corp_verb::defend_province;
        c.subject = d; c.province = f.block(1);
        (void)apply_corp_command(f.w, reg, c);
        (void)add_unit(f, f.A, f.at(5, 3), 10);
        (void)declare_hostile(f.w, f.B, f.A);
        const entity_id post = f.w.units.at(d).defend_post;
        (void)run_military_day(f.w, reg, 1);
        check(f.w.units.at(d).position != post && f.w.units.at(d).defend_province == f.block(1),
              "F12aa run_military_day wakes the defender and walks it at the stack, on its order");
    }
    {
        // THE START ORDER: a major's units in its capital province, and nobody else's.
        fixture f; build(f);
        f.w.nations.at(f.B).is_major = true;
        f.w.nations.at(f.B).capital_tile = f.at(4, 5);
        f.w.nations.at(f.A).capital_tile = f.at(4, 0); // not a major
        const entity_id cap_unit = add_unit(f, f.B, f.at(5, 4));
        const entity_id away     = add_unit(f, f.B, f.at(10, 4));
        const entity_id minor    = add_unit(f, f.A, f.at(4, 0));
        check(open_capital_defence(f.w) == 1 && f.w.units.at(cap_unit).defend_province == f.block(1)
                  && f.w.units.at(cap_unit).defend_post == f.at(5, 4) && f.w.units.at(cap_unit).front == null_entity
                  && f.w.units.at(away).defend_province == no_province && f.w.units.at(minor).defend_province == no_province,
              "F12y a major's unit in its capital province starts on it, assigning no front; others do not");
    }
    {
        // RIVALS: the scorer gives its capital's idle units the order, never the seat's.
        fixture f; build(f);
        f.w.nations.at(f.A).capital_tile = f.at(4, 1);
        f.w.nations.at(f.B).capital_tile = f.at(4, 5);
        f.w.player_entity = f.B;
        const entity_id ru = add_unit(f, f.A, f.at(4, 1));
        const entity_id pu = add_unit(f, f.B, f.at(4, 5));
        reanchor_fronts(f.w);
        world twin = f.w;
        corp_ai_params p; p.cadence_k = 1;
        economy_report r1, r2;
        run_corp_strategic_step(f.w, reg, r1, k_campaign_settle_ticks, p);
        run_corp_strategic_step(twin, reg, r2, k_campaign_settle_ticks, p);
        check(f.w.units.at(ru).defend_province == f.w.provinces.province_of(f.at(4, 1))
                  && f.w.units.at(pu).defend_province == no_province && f.w.units.at(ru).front == null_entity
                  && f.w.state_hash(2) == twin.state_hash(2),
              "F12z the rival's capital unit defends; the player's is untouched; twins agree");
    }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    std::printf(failures == 0 ? "ALL PASS (0 failures)\n" : "FAILURES\n");
    return failures == 0 ? 0 : 1;
}
