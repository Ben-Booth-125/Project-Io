// ---------------------------------------------------------------------------
// space_assets_harness — BL-1327 (space asset model), the headless proof
// ---------------------------------------------------------------------------
// docs/economy/SPACE_ASSETS.md § Era 1 near space (Ben, 2026-10-15). A hand-
// built fixture: a star, a home planet at 1.0 AU, its moon, a far planet at
// 1.5 AU, and two nations on the home planet. Rows:
//
//   A1  LAUNCH: a legal launch debits the treasury exactly the quote, allocates
//       one id, stores the asset in transit with arrival = launch + travel.
//   A2  REACH: further is costlier and slower (home orbit < moon orbit <
//       moon surface; far orbit costs the per-AU term and its extra quarters).
//   A3  ARRIVAL: the step leaves an asset in transit before its arrival tick and
//       makes it active on it; no upkeep is drawn in transit. The tick contract:
//       the launch tick is w.current_econ_tick, and travel 0 vs travel 1 differ by
//       exactly one quarter; the step reports the quarter's launches and cost.
//   A4  UPKEEP: an active asset pays its upkeep whole each quarter.
//   A5  DECAY: an unpaid quarter makes it decaying; a paid one restores it;
//       quarters_to_loss consecutive unpaid quarters lose it; a lost asset never
//       steps or pays again.
//   A6  REJECTION MUTATES NOTHING: unaffordable, illegal pairs, the home surface,
//       the star, an unknown nation/body, an out-of-range class — each refused
//       with state_hash, treasury, the store and the allocator cursor unchanged.
//   A7  SAVE: a store holding decaying, lost and in-transit assets round-trips with an equal state_hash
//       and re-serialises byte for byte; a v50 stream (v51 less its section)
//       reads with no assets, and a SET share rule in a v50 stream survives; a
//       tampered owner, an illegal slot, the star, a state/unpaid mismatch, an
//       unpaid count past the ceiling or an id another entity owns each refuse
//       the stream whole.
//   A8  WIRING: run_nation_step steps the store (an asset arrives through it) and
//       files the launch and upkeep debit on economy_report::space_assets; an
//       empty store hashes exactly as a world that never had one.
//   A9  DETERMINISM: two independently built fixtures driven identically end
//       with equal state_hash.
//
// Build: node tools/verify/build_harness.js space_assets_harness
// Run:   build_gen/verify/space_assets_harness.exe
// ---------------------------------------------------------------------------

#include "world/economy_system.hpp"
#include "world/nation_step.hpp"
#include "world/recipe_registry.hpp"
#include "world/space_assets.hpp"
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
    entity_id a = null_entity, b = null_entity; // two nations
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

void build(fixture& f)
{
    world& w = f.w;
    f.star = add_body(w, "Star", body_type::star, null_entity, 0.0f);
    f.home = add_body(w, "Home", body_type::planet, null_entity, 1.0f);
    f.moon = add_body(w, "Moon", body_type::moon, f.home, 0.0026f);
    f.far  = add_body(w, "Far", body_type::planet, null_entity, 1.5f);
    w.star_body = f.star;
    w.home_body = f.home;
    for (entity_id* n : {&f.a, &f.b})
    {
        *n = w.create_entity();
        nation_component nc;
        nc.name = (n == &f.a) ? "Alpha" : "Beta";
        nc.treasury = 0.0f;
        w.nations.emplace(*n, nc);
    }
    w.nations.at(f.a).treasury = 200000.0f;
    w.nations.at(f.b).treasury = 5000.0f;
}

struct snap
{
    std::uint64_t hash;
    std::size_t   assets;
    std::uint32_t cursor;
    float         ta, tb;
};

snap take(const fixture& f)
{
    return {f.w.state_hash(0), f.w.space_assets.size(), f.w.next_entity_id(),
            f.w.nations.at(f.a).treasury, f.w.nations.at(f.b).treasury};
}

bool same(const snap& x, const snap& y)
{
    return x.hash == y.hash && x.assets == y.assets && x.cursor == y.cursor && x.ta == y.ta
        && x.tb == y.tb;
}

const char* state_name(space_asset_state s)
{
    switch (s)
    {
        case space_asset_state::in_transit: return "in_transit";
        case space_asset_state::active:     return "active";
        case space_asset_state::decaying:   return "decaying";
        case space_asset_state::lost:       return "lost";
    }
    return "?";
}

} // namespace

int main()
{
    const space_asset_params p{};
    std::printf("space_assets_harness — BL-1327\n");
    std::printf("calibration: launch sat %.0f rover %.0f crewed %.0f station %.0f; upkeep %.0f/%.0f/%.0f/%.0f;\n"
                "             reach %.0f/%.0f/%.0f/%.0f/%.0f (+%.0f/AU far); travel q %d/%d/%d/%d/%d (+%.1f/AU far); loss at %d unpaid\n",
                p.launch_cost[0], p.launch_cost[1], p.launch_cost[2], p.launch_cost[3], p.upkeep[0],
                p.upkeep[1], p.upkeep[2], p.upkeep[3], p.reach_cost[0], p.reach_cost[1],
                p.reach_cost[2], p.reach_cost[3], p.reach_cost[4], p.far_cost_per_au,
                p.travel_quarters[0], p.travel_quarters[1], p.travel_quarters[2],
                p.travel_quarters[3], p.travel_quarters[4], p.far_quarters_per_au,
                p.quarters_to_loss);

    fixture f;
    build(f);
    world& w = f.w;

    // ---- A2 reach ladder (quotes are pure) ---------------------------------
    std::printf("--- A2 reach ---\n");
    {
        const space_launch_quote ho = quote_space_launch(w, p, f.a, space_asset_class::satellite, {f.home, space_slot::orbit});
        const space_launch_quote mo = quote_space_launch(w, p, f.a, space_asset_class::satellite, {f.moon, space_slot::orbit});
        const space_launch_quote ms = quote_space_launch(w, p, f.a, space_asset_class::rover, {f.moon, space_slot::surface});
        const space_launch_quote fo = quote_space_launch(w, p, f.a, space_asset_class::satellite, {f.far, space_slot::orbit});
        const space_launch_quote fs = quote_space_launch(w, p, f.a, space_asset_class::rover, {f.far, space_slot::surface});
        std::printf("    home orbit sat %.0f/%dq  moon orbit sat %.0f/%dq  moon surface rover %.0f/%dq  far orbit sat %.0f/%dq  far surface rover %.0f/%dq\n",
                    ho.cost, ho.travel_quarters, mo.cost, mo.travel_quarters, ms.cost,
                    ms.travel_quarters, fo.cost, fo.travel_quarters, fs.cost, fs.travel_quarters);
        check(ho.reach == space_reach::home_orbit && mo.reach == space_reach::moon_orbit
                  && ms.reach == space_reach::moon_surface && fo.reach == space_reach::far_orbit
                  && fs.reach == space_reach::far_surface,
              "A2", "each destination resolves to its reach tier");
        check(ho.cost < mo.cost && ho.travel_quarters <= mo.travel_quarters && mo.cost < fo.cost
                  && mo.travel_quarters < fo.travel_quarters,
              "A2", "for one class, further is costlier and no faster (home < moon < far)");
        const float far_expect = p.launch_cost[0] + p.reach_cost[3] + p.far_cost_per_au * 0.5f;
        check(fo.cost == far_expect && fo.travel_quarters == p.travel_quarters[3] + 0,
              "A2", fmt("far orbit at 0.5 AU = launch + reach + 0.5 x per-AU (%.0f), extra quarters floor(0.5) = 0", far_expect));
        // A further far body: 3.0 AU away from home.
        const entity_id outer = add_body(w, "Outer", body_type::planet, null_entity, 4.0f);
        const space_launch_quote oo = quote_space_launch(w, p, f.a, space_asset_class::satellite, {outer, space_slot::orbit});
        check(oo.cost > fo.cost && oo.travel_quarters == p.travel_quarters[3] + 3,
              "A2", fmt("a body 3 AU out costs more (%.0f) and takes 3 more quarters (%d)", oo.cost, oo.travel_quarters));
    }

    // ---- A6 rejection mutates nothing --------------------------------------
    std::printf("--- A6 rejection ---\n");
    {
        struct reject_case { const char* what; entity_id nation; space_asset_class cls; space_destination d; space_launch_result want; };
        const reject_case cases[] = {
            {"unaffordable (Beta, 5000, crewed to the moon)", f.b, space_asset_class::crewed_mission, {f.moon, space_slot::surface}, space_launch_result::rejected_insufficient_treasury},
            {"satellite to a surface", f.a, space_asset_class::satellite, {f.moon, space_slot::surface}, space_launch_result::rejected_illegal_destination},
            {"rover to an orbit", f.a, space_asset_class::rover, {f.moon, space_slot::orbit}, space_launch_result::rejected_illegal_destination},
            {"crewed mission to home orbit", f.a, space_asset_class::crewed_mission, {f.home, space_slot::orbit}, space_launch_result::rejected_illegal_destination},
            {"station module to a far orbit", f.a, space_asset_class::station_module, {f.far, space_slot::orbit}, space_launch_result::rejected_illegal_destination},
            {"anything to the home surface", f.a, space_asset_class::rover, {f.home, space_slot::surface}, space_launch_result::rejected_illegal_destination},
            {"anything to the star", f.a, space_asset_class::satellite, {f.star, space_slot::orbit}, space_launch_result::rejected_illegal_destination},
            {"an unknown nation", 999999u, space_asset_class::satellite, {f.home, space_slot::orbit}, space_launch_result::rejected_unknown_nation},
            {"a body as nation", f.home, space_asset_class::satellite, {f.home, space_slot::orbit}, space_launch_result::rejected_unknown_nation},
            {"an unknown body", f.a, space_asset_class::satellite, {999999u, space_slot::orbit}, space_launch_result::rejected_unknown_body},
            {"an out-of-range class", f.a, static_cast<space_asset_class>(7), {f.home, space_slot::orbit}, space_launch_result::rejected_invalid},
            {"an out-of-range slot", f.a, space_asset_class::satellite, {f.home, static_cast<space_slot>(9)}, space_launch_result::rejected_invalid},
        };
        for (const reject_case& c : cases)
        {
            const snap before = take(f);
            w.current_econ_tick = 3;
            const space_launch_quote q = launch_space_asset(w, p, c.nation, c.cls, c.d);
            const snap after = take(f);
            check(q.result == c.want && q.asset == null_entity && same(before, after), "A6",
                  fmt("%s: refused (code %d), hash/treasuries/store/cursor unchanged", c.what, static_cast<int>(q.result)));
        }
        // The boundary: a treasury exactly equal to the cost is affordable; one
        // credit short is not.
        const float keep = w.nations.at(f.b).treasury;
        const space_launch_quote q0 = quote_space_launch(w, p, f.a, space_asset_class::satellite, {f.home, space_slot::orbit});
        w.nations.at(f.b).treasury = q0.cost;
        const space_launch_quote qe = quote_space_launch(w, p, f.b, space_asset_class::satellite, {f.home, space_slot::orbit});
        w.nations.at(f.b).treasury = q0.cost - 1.0f;
        const space_launch_quote qs = quote_space_launch(w, p, f.b, space_asset_class::satellite, {f.home, space_slot::orbit});
        w.nations.at(f.b).treasury = keep;
        check(qe.result == space_launch_result::launched
                  && qs.result == space_launch_result::rejected_insufficient_treasury,
              "A6", fmt("treasury == cost (%.0f) is affordable; one credit short is refused", q0.cost));
    }

    // ---- A1 launch ---------------------------------------------------------
    std::printf("--- A1 launch ---\n");
    entity_id sat = null_entity, rov = null_entity;
    {
        const int t0 = 10;
        const float before = w.nations.at(f.a).treasury;
        const std::uint32_t cursor = w.next_entity_id();
        const space_launch_quote qq = quote_space_launch(w, p, f.a, space_asset_class::satellite, {f.moon, space_slot::orbit});
        w.current_econ_tick = t0; // the tick about to run: the launch tick
        const space_launch_quote q = launch_space_asset(w, p, f.a, space_asset_class::satellite, {f.moon, space_slot::orbit});
        sat = q.asset;
        const auto it = w.space_assets.find(sat);
        check(q.result == space_launch_result::launched && it != w.space_assets.end()
                  && w.nations.at(f.a).treasury == before - qq.cost && sat == cursor
                  && w.next_entity_id() == cursor + 1,
              "A1", fmt("a satellite to moon orbit launches: treasury %.0f -> %.0f (cost %.0f), id %u from the allocator",
                        before, w.nations.at(f.a).treasury, qq.cost, static_cast<unsigned>(sat)));
        if (it != w.space_assets.end())
        {
            const space_asset& a = it->second;
            check(a.owner == f.a && a.cls == space_asset_class::satellite && a.dest.body == f.moon
                      && a.dest.slot == space_slot::orbit && a.state == space_asset_state::in_transit
                      && a.launch_tick == t0 && a.arrival_tick == t0 + qq.travel_quarters
                      && a.upkeep == p.upkeep[0] && a.unpaid_quarters == 0,
                  "A1", fmt("record: owner, class, destination, in_transit, launch %d, arrival %d, upkeep %.0f",
                            a.launch_tick, a.arrival_tick, a.upkeep));
        }
        const space_launch_quote r = launch_space_asset(w, p, f.a, space_asset_class::rover, {f.far, space_slot::surface});
        rov = r.asset;
        check(r.result == space_launch_result::launched && w.space_assets.at(rov).arrival_tick == t0 + r.travel_quarters
                  && r.travel_quarters == p.travel_quarters[4],
              "A1", fmt("a rover to the far surface arrives after %d quarters", r.travel_quarters));
    }

    // ---- A3/A4 arrival and upkeep ------------------------------------------
    std::printf("--- A3/A4 arrival and upkeep ---\n");
    {
        const float t_before = w.nations.at(f.a).treasury;
        space_asset_step_report r;
        run_space_asset_step(w, p, 10, &r); // launch quarter: the sat (1q) is still travelling
        check(w.space_assets.at(sat).state == space_asset_state::in_transit && r.arrived == 0
                  && w.nations.at(f.a).treasury == t_before && r.upkeep_paid == 0.0f,
              "A3", "before its arrival tick the asset is in transit and draws no upkeep");
        run_space_asset_step(w, p, 11, &r);
        check(w.space_assets.at(sat).state == space_asset_state::active && r.arrived == 1
                  && w.space_assets.at(rov).state == space_asset_state::in_transit,
              "A3", "on its arrival tick it becomes active; the rover (3q) is still travelling");
        check(w.nations.at(f.a).treasury == t_before - p.upkeep[0] && r.paid == 1,
              "A4", fmt("the arrived satellite pays its quarter whole: %.0f", p.upkeep[0]));
        const float t1 = w.nations.at(f.a).treasury;
        run_space_asset_step(w, p, 12, &r);
        run_space_asset_step(w, p, 13, &r); // rover arrives
        check(w.space_assets.at(rov).state == space_asset_state::active
                  && w.nations.at(f.a).treasury == t1 - 2 * p.upkeep[0] - p.upkeep[1],
              "A4", fmt("two more quarters: sat x2 + the arrived rover x1 = %.0f debited", 2 * p.upkeep[0] + p.upkeep[1]));
    }

    // ---- A3b the tick contract ---------------------------------------------
    std::printf("--- A3b tick contract ---\n");
    {
        fixture k;
        build(k);
        k.w.current_econ_tick = 5;
        const space_launch_quote q0 = launch_space_asset(k.w, p, k.a, space_asset_class::satellite, {k.home, space_slot::orbit});
        const space_launch_quote q1 = launch_space_asset(k.w, p, k.a, space_asset_class::satellite, {k.moon, space_slot::orbit});
        const space_asset& a0 = k.w.space_assets.at(q0.asset);
        const space_asset& a1 = k.w.space_assets.at(q1.asset);
        check(a0.launch_tick == 5 && a1.launch_tick == 5 && q0.travel_quarters == 0 && q1.travel_quarters == 1
                  && a0.launch_cost == q0.cost && a1.launch_cost == q1.cost,
              "A3b", "the launch tick is w.current_econ_tick; the record carries the quoted cost");
        space_asset_step_report r5, r6;
        run_space_asset_step(k.w, p, 5, &r5);
        const bool at5 = a0.state == space_asset_state::active && a1.state == space_asset_state::in_transit;
        run_space_asset_step(k.w, p, 6, &r6);
        const bool at6 = a1.state == space_asset_state::active;
        check(at5 && at6, "A3b", "travel 0 is active in the launch quarter's step; travel 1 exactly one step later");
        check(r5.launched == 2 && r5.launch_spent == q0.cost + q1.cost && r5.upkeep_paid == p.upkeep[0]
                  && r6.launched == 0 && r6.launch_spent == 0.0f && r6.upkeep_paid == 2 * p.upkeep[0],
              "A3b", fmt("the step reports the quarter's launches (2, %.0f) then none; upkeep %.0f then %.0f",
                         q0.cost + q1.cost, p.upkeep[0], 2 * p.upkeep[0]));
    }

    // ---- A5 decay ----------------------------------------------------------
    std::printf("--- A5 decay ---\n");
    {
        w.nations.at(f.a).treasury = 0.0f;
        space_asset_step_report r;
        run_space_asset_step(w, p, 14, &r);
        const space_asset& s = w.space_assets.at(sat);
        check(s.state == space_asset_state::decaying && s.unpaid_quarters == 1 && r.unpaid == 2
                  && w.nations.at(f.a).treasury == 0.0f,
              "A5", "an unpaid quarter: decaying, 1 unpaid, nothing debited (treasury stays 0, never negative)");
        // A partial treasury pays the sat (lower id) and not the rover: whole or nothing, ascending id.
        w.nations.at(f.a).treasury = p.upkeep[0] + 1.0f;
        run_space_asset_step(w, p, 15, &r);
        check(w.space_assets.at(sat).state == space_asset_state::active
                  && w.space_assets.at(sat).unpaid_quarters == 0
                  && w.space_assets.at(rov).state == space_asset_state::decaying
                  && w.space_assets.at(rov).unpaid_quarters == 2
                  && w.nations.at(f.a).treasury == 1.0f,
              "A5", "a paid quarter restores the satellite; the rover (higher id) finds 1 credit and stays decaying");
        w.nations.at(f.a).treasury = 0.0f;
        int q = 16;
        while (w.space_assets.at(rov).state != space_asset_state::lost && q < 40)
            run_space_asset_step(w, p, q++, &r);
        check(w.space_assets.at(rov).state == space_asset_state::lost
                  && w.space_assets.at(rov).unpaid_quarters == p.quarters_to_loss,
              "A5", fmt("the rover is lost at %d consecutive unpaid quarters", p.quarters_to_loss));
        const int sat_unpaid = w.space_assets.at(sat).unpaid_quarters;
        check(w.space_assets.at(sat).state == space_asset_state::decaying && sat_unpaid == 2,
              "A5", fmt("the satellite, one paid quarter later, is decaying at %d unpaid", sat_unpaid));
        w.nations.at(f.a).treasury = 100000.0f;
        run_space_asset_step(w, p, q++, &r);
        check(w.space_assets.at(rov).state == space_asset_state::lost
                  && w.space_assets.at(rov).unpaid_quarters == p.quarters_to_loss
                  && w.nations.at(f.a).treasury == 100000.0f - p.upkeep[0],
              "A5", "a lost asset never pays or recovers; only the satellite drew upkeep");
    }

    // ---- A7 save -----------------------------------------------------------
    std::printf("--- A7 save ---\n");
    {
        // Add one of each remaining state: an in-transit station module.
        w.current_econ_tick = 50;
        const space_launch_quote st = launch_space_asset(w, p, f.a, space_asset_class::station_module, {f.home, space_slot::orbit});
        check(st.result == space_launch_result::launched, "A7", "fixture: a station module in transit");
        // states present: active sat, lost rover, in-transit station; decaying via a fourth
        w.nations.at(f.a).treasury = 500000.0f;
        w.current_econ_tick = 20;
        const space_launch_quote cm = launch_space_asset(w, p, f.a, space_asset_class::crewed_mission, {f.moon, space_slot::surface});
        check(cm.result == space_launch_result::launched, "A7", "fixture: a crewed mission to the moon surface");
        if (cm.result == space_launch_result::launched)
        {
            // Drive it to decaying the honest way: one step with an empty treasury.
            // (The station, arriving at 50, is untouched at tick 21.)
            const float keep = w.nations.at(f.a).treasury;
            w.nations.at(f.a).treasury = 0.0f;
            run_space_asset_step(w, p, 21);
            w.nations.at(f.a).treasury = keep;
        }
        const std::string b = to_bytes(w);
        world r;
        const bool ok = from_bytes(b, r);
        bool eq = ok && r.space_assets.size() == w.space_assets.size();
        if (eq)
            for (const auto& [id, a] : w.space_assets)
            {
                const space_asset& x = r.space_assets.at(id);
                eq = eq && x.id == a.id && x.owner == a.owner && x.cls == a.cls && x.dest.body == a.dest.body
                     && x.dest.slot == a.dest.slot && x.state == a.state && x.launch_tick == a.launch_tick
                     && x.arrival_tick == a.arrival_tick && x.upkeep == a.upkeep
                     && x.unpaid_quarters == a.unpaid_quarters && x.launch_cost == a.launch_cost;
            }
        std::printf("    %zu assets:", w.space_assets.size());
        for (const auto& [id, a] : w.space_assets)
            std::printf(" %u=%s", static_cast<unsigned>(id), state_name(a.state));
        std::printf("; stream %zu bytes at world_save_version %u\n", b.size(), static_cast<unsigned>(world_save_version));
        check(eq && r.state_hash(0) == w.state_hash(0), "A7",
              "v51 round-trips every asset field by field, with an equal state_hash");
        check(ok && to_bytes(r) == b, "A7", "re-serialising the loaded world reproduces the stream byte for byte");
        check(ok && r.next_entity_id() == w.next_entity_id(), "A7", "the allocator cursor round-trips (no id re-issued)");

        // v50: the stream less the space section (count + n x 35-byte records), stamped 50.
        const std::size_t rec = 4 + 4 + 1 + 4 + 1 + 1 + 4 + 4 + 4 + 4 + 4;
        // BL-1335 (v52): the tax section follows the space section (count + 9
        // bytes a nation whose tier or Discontent moved), so both come off; the
        // record offsets below still count from the space section's start.
        // BL-1337 (v54): and the exact-net section after the tax section.
        // BL-1407 (v56): no corporation store, so a v56 stream is no longer a
        // v50 one with sections appended. The v50 prefix is cut from a v55 stream
        // (the verification writer, empty corporation store): the space section,
        // the v52 tax section (with its empty corporation half), the v54 net
        // section and the v55 majors. The tamper rows below read the v56 stream,
        // whose tax section carries the nation rows only, followed by the majors
        // and the v56 nation books.
        const auto v55_of = [](const world& x) {
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_legacy(x, {}, o, k_world_save_version_pre_books);
            return o.str();
        };
        const auto tail55_of = [&](const world& x) {
            return 4 + x.space_assets.size() * rec + world_save_tax_section_bytes(x)
                 + world_save_net_section_bytes(x) + world_save_major_section_bytes(x);
        };
        const std::size_t tail = 4 + w.space_assets.size() * rec + (world_save_tax_section_bytes(w) - 4)
                               + world_save_major_section_bytes(w) + world_save_books_section_bytes(w)
                               + 12  // BL-1434 (v64): the designs section, three empty counts
                               + 24  // BL-1449 (v65): the air section, six empty counts
                               + world_save_battle_carry_section_bytes(w); // BL-1453 (v67)
        const std::string b55 = v55_of(w);
        std::string v50 = b55.substr(0, b55.size() - tail55_of(w));
        const std::uint32_t pre = k_world_save_version_pre_space;
        std::memcpy(&v50[4], &pre, sizeof pre);
        world old;
        const bool ok50 = from_bytes(v50, old);
        check(ok50 && old.space_assets.empty() && old.nations.size() == w.nations.size(),
              "A7", "a v50 stream (the v51 prefix) reads, as a world with no space assets");

        // Tampered: the first record's owner -> an id no nation holds.
        std::string bad = b;
        const entity_id junk = 0x7FFFFFF0u;
        std::memcpy(&bad[b.size() - tail + 4 + 4], &junk, sizeof junk);
        world victim;
        victim.current_econ_tick = 77;
        check(!from_bytes(bad, victim) && victim.current_econ_tick == 77 && victim.nations.empty(),
              "A7", "an owner naming no nation refuses the stream whole, destination untouched");
        // Tampered: a state byte out of range.
        std::string bad2 = b;
        bad2[b.size() - tail + 4 + 4 + 4 + 1 + 4 + 1] = static_cast<char>(9);
        world victim2;
        check(!from_bytes(bad2, victim2) && victim2.nations.empty(), "A7", "an out-of-range state byte refuses the stream whole");
        // Record field offsets from the first record's start: id 0, owner 4,
        // class 8, body 9, slot 13, state 14, launch 15, arrival 19, upkeep 23,
        // unpaid 27, launch cost 31. The first record is the satellite (decaying, 1 unpaid).
        const std::size_t r0 = b.size() - tail + 4;
        auto refused = [&](std::size_t off, const void* v, std::size_t n) {
            std::string t = b;
            std::memcpy(&t[r0 + off], v, n);
            world vv;
            return !from_bytes(t, vv) && vv.nations.empty();
        };
        const space_asset& first = w.space_assets.begin()->second;
        check(first.cls == space_asset_class::satellite && first.state == space_asset_state::decaying
                  && first.unpaid_quarters == 1,
              "A7", "fixture: the first record is a decaying satellite at 1 unpaid");
        const std::uint8_t surface = 1, active = 1, lost = 3, transit = 0;
        const std::int32_t over = k_space_quarters_to_loss_ceiling + 1, zero = 0;
        check(refused(13, &surface, 1), "A7", "a satellite stored on a SURFACE slot refuses the stream whole");
        check(refused(9, &f.star, sizeof f.star), "A7", "an asset stored at the STAR refuses the stream whole");
        check(refused(14, &active, 1), "A7", "state active with 1 unpaid quarter refuses the stream whole");
        check(refused(14, &transit, 1), "A7", "state in_transit with 1 unpaid quarter refuses the stream whole");
        check(refused(27, &zero, 4), "A7", "state decaying with 0 unpaid quarters refuses the stream whole");
        {
            std::string t = b;
            std::memcpy(&t[r0 + 14], &lost, 1);
            std::memcpy(&t[r0 + 27], &over, 4);
            world vv;
            check(!from_bytes(t, vv), "A7", fmt("state lost with %d unpaid (past the ceiling %d) refuses the stream whole",
                                                over, k_space_quarters_to_loss_ceiling));
        }
        {
            // An id another entity kind owns: a corporation keyed by the first
            // asset's id. The control — the same corporation under a fresh id — loads.
            world okc = w;
            const entity_id fresh = okc.create_entity();
            okc.nations.emplace(fresh, nation_component{});
            world back_ok;
            const bool control = from_bytes(to_bytes(okc), back_ok);
            world clash = w;
            clash.nations.emplace(w.space_assets.begin()->first, nation_component{});
            world back_clash;
            const bool clash_refused = !from_bytes(to_bytes(clash), back_clash);
            check(control && clash_refused, "A7",
                  "an asset id a nation also owns refuses the stream whole (control: a fresh-id nation loads)");
        }
        {
            // A SET share rule in a v50 stream survives: the share section is read
            // for v50 as for v51.
            world ws = w;
            ws.nations.at(f.a).lp_shares.set   = true;
            ws.nations.at(f.a).lp_shares.share = {0.2f, 0.5f, 0.3f};
            const std::string bs = v55_of(ws); // BL-1407: the v55 stream, as above
            std::string s50 = bs.substr(0, bs.size() - tail55_of(ws));
            std::memcpy(&s50[4], &pre, sizeof pre);
            world back;
            const bool ok_s = from_bytes(s50, back);
            check(ok_s && back.nations.at(f.a).lp_shares.set
                      && back.nations.at(f.a).lp_shares.share == ws.nations.at(f.a).lp_shares.share
                      && back.space_assets.empty(),
                  "A7", "a v50 stream carrying a SET share rule reads with the rule intact (and no assets)");
        }
        // A version past the current one is refused.
        std::string next = b;
        const std::uint32_t nv = world_save_version + 1;
        std::memcpy(&next[4], &nv, sizeof nv);
        world victim3;
        check(!from_bytes(next, victim3), "A7", "a stream stamped world_save_version + 1 is refused");
    }

    // ---- A8 wiring ---------------------------------------------------------
    std::printf("--- A8 wiring through run_nation_step ---\n");
    {
        fixture g;
        build(g);
        const std::uint64_t empty_hash = g.w.state_hash(0);
        recipe_registry reg;
        economy_report rep;
        g.w.current_econ_tick = 1;
        run_nation_step(g.w, reg, rep, 1);
        check(g.w.space_assets.empty(), "A8", "an empty store stays empty through the nation step");
        {
            // The fold is inert when empty: a world whose store held an asset and
            // was emptied again hashes exactly as it did before (no allocation, so
            // nothing else moved).
            fixture h;
            build(h);
            const std::uint64_t before = h.w.state_hash(0);
            check(before == empty_hash, "A8", "two fresh fixtures hash equal");
            space_asset probe;
            probe.id = 999;
            probe.owner = h.a;
            probe.dest = {h.home, space_slot::orbit};
            h.w.space_assets.emplace(999, probe);
            const std::uint64_t with = h.w.state_hash(0);
            h.w.space_assets.clear();
            check(with != empty_hash && h.w.state_hash(0) == empty_hash, "A8",
                  "an emptied store hashes as one that never held an asset (the fold is inert when empty)");
        }
        g.w.current_econ_tick = 2; // the tick about to run
        const space_launch_quote q = launch_space_asset(g.w, reg.space_assets(), g.a, space_asset_class::satellite, {g.home, space_slot::orbit});
        const float t = g.w.nations.at(g.a).treasury;
        economy_report rep2;
        run_nation_step(g.w, reg, rep2, 2);
        const space_asset& a = g.w.space_assets.at(q.asset);
        check(q.result == space_launch_result::launched && a.state == space_asset_state::active
                  && g.w.nations.at(g.a).treasury == t - reg.space_assets().upkeep[0],
              "A8", fmt("a home-orbit satellite (0 quarters) arrives and pays %.0f through run_nation_step",
                        reg.space_assets().upkeep[0]));
        const space_asset_step_report& sr = rep2.space_assets;
        check(sr.launched == 1 && sr.launch_spent == q.cost && sr.arrived == 1 && sr.paid == 1
                  && sr.upkeep_paid == reg.space_assets().upkeep[0],
              "A8", fmt("economy_report::space_assets files the quarter: 1 launch at %.0f, 1 arrival, %.0f upkeep",
                        q.cost, reg.space_assets().upkeep[0]));
        // The fold sees the store: flipping one asset's state moves the hash.
        const std::uint64_t h0 = g.w.state_hash(0);
        g.w.space_assets.at(q.asset).state = space_asset_state::decaying;
        const std::uint64_t h1 = g.w.state_hash(0);
        g.w.space_assets.at(q.asset).state = space_asset_state::active;
        check(h0 != h1 && g.w.state_hash(0) == h0, "A8", "state_hash folds the store (a state flip moves it, the flip back restores it)");
    }

    // ---- A9 determinism ----------------------------------------------------
    std::printf("--- A9 determinism ---\n");
    {
        auto drive = [&](fixture& x) {
            x.w.current_econ_tick = 0;
            launch_space_asset(x.w, p, x.a, space_asset_class::satellite, {x.moon, space_slot::orbit});
            launch_space_asset(x.w, p, x.a, space_asset_class::rover, {x.far, space_slot::surface});
            x.w.current_econ_tick = 1;
            launch_space_asset(x.w, p, x.b, space_asset_class::satellite, {x.home, space_slot::orbit});
            for (int t = 0; t < 30; ++t)
            {
                if (t == 12) x.w.nations.at(x.a).treasury = 0.0f;
                run_space_asset_step(x.w, p, t);
            }
            return x.w.state_hash(0);
        };
        fixture x, y;
        build(x);
        build(y);
        const std::uint64_t hx = drive(x), hy = drive(y);
        check(hx == hy && to_bytes(x.w) == to_bytes(y.w), "A9",
              fmt("two fixtures driven identically: equal state_hash %016llx and equal streams",
                  static_cast<unsigned long long>(hx)));
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
