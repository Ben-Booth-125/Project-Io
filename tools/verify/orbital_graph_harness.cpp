// ---------------------------------------------------------------------------
// orbital_graph_harness — BL-1580 (orbital graph).
//
// docs/economy/SPACE_ASSETS.md § The orbital graph and § Orbital slots. The
// graph is a pure function of the bodies; this checks its shape, positions,
// costs and slots on a hand-built Sol-analogue (with the planetology chain's
// real masses), then node counts and determinism on a generated world.
//
//   O1  node counts: seven per non-star body, ascending, none on the star.
//   O2  L4/L5 sit 60° from the moon about its planet, at the moon's radius;
//       L1/L2 lie on the planet→moon line, inside and beyond.
//   O3  path cost is symmetric (both directions, every pair of a sample).
//   O4  a small airless body (moon, asteroid) climbs cheaper than home.
//   O5  home surface → moon low orbit is cheaper than → a far planet's low.
//   O6  repeated calls are bit-identical (links, paths, positions).
//   O7  slot capacities match the table.
//   O8  invalid ends fail; from == to is a zero-cost trip.
//   G*  the generated world: counts, reachability, determinism; and it PRINTS
//       each body's climb so the masses generation actually carries are read.
//
// Headless: world/* logic only, no SDL and no Lua.
// ---------------------------------------------------------------------------

#include "world/orbital_graph.hpp"
#include "world/hard_coded_world.hpp"
#include "world/world_gen_config.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

constexpr double k_pi = 3.14159265358979323846;

struct test_world
{
    world     w;
    entity_id star = null_entity, cinder = null_entity, kepler = null_entity,
              selene = null_entity, pallas = null_entity;
};

entity_id add_body(world& w, body_type t, entity_id parent, float r, float a, float vel,
                   float mass, atmosphere_class atm)
{
    const entity_id id = w.create_entity();
    body_component b{};
    b.name = "b";
    b.type = t;
    b.parent = parent;
    b.orbital_radius_au = r;
    b.orbital_angle_rad = a;
    b.orbital_angular_velocity_rad_per_day = vel;
    b.grid_width = 10;
    b.grid_height = 5;
    b.mass_earths = mass;
    b.atmosphere = atm;
    w.bodies[id] = b;
    return id;
}

test_world make_test_world()
{
    test_world t;
    t.star   = add_body(t.w, body_type::star, null_entity, 0.0f, 0.0f, 0.0f, 1.0f, atmosphere_class::none);
    t.w.star_body = t.star;
    // Masses from planetology.cpp's prototype set; atmospheres as generation reads them.
    t.cinder = add_body(t.w, body_type::planet, null_entity, 0.39f, 0.40f, 0.07f, 0.055f, atmosphere_class::none);
    t.kepler = add_body(t.w, body_type::planet, null_entity, 1.00f, 1.05f, 0.017f, 1.0f, atmosphere_class::moderate);
    t.selene = add_body(t.w, body_type::moon, t.kepler, 0.30f, 0.7f, 0.30f, 0.0123f, atmosphere_class::none);
    t.pallas = add_body(t.w, body_type::asteroid, null_entity, 3.05f, 4.6f, 0.003f, 0.000034f, atmosphere_class::none);
    t.w.home_body = t.kepler;
    return t;
}

double angle_between(orbital_position c, orbital_position a, orbital_position b)
{
    const double ax = a.x_au - c.x_au, ay = a.y_au - c.y_au;
    const double bx = b.x_au - c.x_au, by = b.y_au - c.y_au;
    const double d = std::atan2(ax * by - ay * bx, ax * bx + ay * by);
    return std::fabs(d);
}

double dist(orbital_position a, orbital_position b)
{
    return std::hypot(a.x_au - b.x_au, a.y_au - b.y_au);
}

bool same_links(const std::vector<orbital_link>& x, const std::vector<orbital_link>& y)
{
    if (x.size() != y.size()) return false;
    for (std::size_t i = 0; i < x.size(); ++i)
        if (x[i].a != y[i].a || x[i].b != y[i].b || x[i].kind != y[i].kind ||
            std::memcmp(&x[i].dv, &y[i].dv, sizeof(double)) != 0 ||
            std::memcmp(&x[i].days, &y[i].days, sizeof(double)) != 0)
            return false;
    return true;
}

bool same_path(const orbital_path_result& x, const orbital_path_result& y)
{
    return x.ok == y.ok && x.nodes == y.nodes &&
           std::memcmp(&x.dv, &y.dv, sizeof(double)) == 0 &&
           std::memcmp(&x.days, &y.days, sizeof(double)) == 0;
}

const char* kind_name(orbital_node_kind k)
{
    switch (k)
    {
    case orbital_node_kind::surface:    return "surface";
    case orbital_node_kind::low_orbit:  return "low";
    case orbital_node_kind::high_orbit: return "high";
    case orbital_node_kind::l1:         return "L1";
    case orbital_node_kind::l2:         return "L2";
    case orbital_node_kind::l4:         return "L4";
    case orbital_node_kind::l5:         return "L5";
    }
    return "?";
}

void print_path(const char* label, const orbital_path_result& r)
{
    std::printf("      %s: ok=%d dv=%.4f days=%.3f  [", label, r.ok ? 1 : 0, r.dv, r.days);
    for (std::size_t i = 0; i < r.nodes.size(); ++i)
        std::printf("%s%u.%s", i ? " > " : "", static_cast<unsigned>(r.nodes[i].body), kind_name(r.nodes[i].kind));
    std::printf("]\n");
}

void run_test_world()
{
    using K = orbital_node_kind;
    const orbital_graph_params p{};
    const test_world t = make_test_world();
    const world& w = t.w;

    // O1
    const std::vector<orbital_node> nodes = orbital_nodes(w);
    check(nodes.size() == 4u * orbital_node_kind_count, "O1a  4 non-star bodies carry 7 nodes each (28)");
    bool ascending = true, no_star = true;
    for (std::size_t i = 1; i < nodes.size(); ++i) ascending = ascending && (nodes[i - 1] < nodes[i]);
    for (const orbital_node& n : nodes) no_star = no_star && n.body != t.star;
    check(ascending, "O1b  nodes are strictly ascending (body, kind)");
    check(no_star && !orbital_node_valid(w, { t.star, K::low_orbit }), "O1c  the star carries no node");

    const std::vector<orbital_link> links = orbital_links(w, p);
    // Per body: climb, low-high, 2 own high-L, 6 L-L = 10; Selene adds 4 parent high-L;
    // 3 star-orbiting bodies give 3 pairs × 4 interplanetary links.
    check(links.size() == 4u * 10u + 4u + 12u, "O1d  link count = 56 (10 per body + 4 parent high-L + 12 interplanetary)");
    std::printf("      nodes=%zu links=%zu\n", nodes.size(), links.size());

    // O2
    const orbital_position planet = orbital_node_position(w, { t.kepler, K::high_orbit });
    const orbital_position moon   = orbital_node_position(w, { t.selene, K::low_orbit });
    const orbital_position l4     = orbital_node_position(w, { t.selene, K::l4 });
    const orbital_position l5     = orbital_node_position(w, { t.selene, K::l5 });
    const orbital_position l1     = orbital_node_position(w, { t.selene, K::l1 });
    const orbital_position l2     = orbital_node_position(w, { t.selene, K::l2 });
    const double a4 = angle_between(planet, moon, l4), a5 = angle_between(planet, moon, l5);
    std::printf("      L4 angle=%.9f deg, L5 angle=%.9f deg\n", a4 * 180.0 / k_pi, a5 * 180.0 / k_pi);
    check(std::fabs(a4 - k_pi / 3.0) < 1e-6 && std::fabs(a5 - k_pi / 3.0) < 1e-6,
          "O2a  L4 and L5 are 60 deg from the moon about its planet");
    check(std::fabs(angle_between(planet, l4, l5) - 2.0 * k_pi / 3.0) < 1e-6,
          "O2b  L4 and L5 are on opposite sides (120 deg apart)");
    check(std::fabs(dist(planet, l4) - 0.30) < 1e-6 && std::fabs(dist(planet, l5) - 0.30) < 1e-6,
          "O2c  L4/L5 sit on the moon's orbit radius");
    // Moon at planet + 0.3 AU: the composition rule.
    check(std::fabs(dist(planet, moon) - 0.30) < 1e-6, "O2d  moon composes as planet position + own orbit");
    // L4 leads: Selene moves CCW, so L4 is at +60 (cross product positive).
    const double cross4 = (moon.x_au - planet.x_au) * (l4.y_au - planet.y_au) - (moon.y_au - planet.y_au) * (l4.x_au - planet.x_au);
    check(cross4 > 0.0, "O2e  L4 leads in the direction of motion (CCW moon)");
    check(dist(planet, l1) < dist(planet, moon) && dist(planet, l2) > dist(planet, moon) &&
          angle_between(planet, moon, l1) < 1e-6 && angle_between(planet, moon, l2) < 1e-6,
          "O2f  L1 inside and L2 beyond the moon on the planet->moon line");

    // O3: symmetry over a sample of nodes from every body.
    const orbital_node sample[] = {
        { t.kepler, K::surface }, { t.kepler, K::high_orbit }, { t.selene, K::surface },
        { t.selene, K::l4 }, { t.cinder, K::low_orbit }, { t.pallas, K::surface }, { t.kepler, K::l2 },
    };
    bool symmetric = true, all_ok = true;
    double worst = 0.0;
    for (const orbital_node& a : sample)
        for (const orbital_node& b : sample)
        {
            const orbital_path_result ab = orbital_path(w, p, a, b), ba = orbital_path(w, p, b, a);
            all_ok = all_ok && ab.ok && ba.ok;
            const double d = std::fabs(ab.dv - ba.dv);
            worst = d > worst ? d : worst;
            symmetric = symmetric && d <= 1e-12 * (1.0 + ab.dv) && std::fabs(ab.days - ba.days) <= 1e-9 * (1.0 + ab.days);
        }
    std::printf("      worst |dv(a,b) - dv(b,a)| = %.3g\n", worst);
    check(all_ok, "O3a  every sampled pair is reachable both ways");
    check(symmetric, "O3b  path cost and days are symmetric (within 1e-12 relative)");

    // O4
    const double c_home = orbital_climb_dv(w, p, t.kepler);
    const double c_moon = orbital_climb_dv(w, p, t.selene);
    const double c_ast  = orbital_climb_dv(w, p, t.pallas);
    const double c_cin  = orbital_climb_dv(w, p, t.cinder);
    std::printf("      climb dv: Kepler=%.4f Selene=%.4f Pallas=%.4f Cinder=%.4f\n", c_home, c_moon, c_ast, c_cin);
    const orbital_path_result moon_up = orbital_path(w, p, { t.selene, K::surface }, { t.selene, K::low_orbit });
    const orbital_path_result ast_up  = orbital_path(w, p, { t.pallas, K::surface }, { t.pallas, K::low_orbit });
    const orbital_path_result home_up = orbital_path(w, p, { t.kepler, K::surface }, { t.kepler, K::low_orbit });
    check(moon_up.ok && home_up.ok && moon_up.dv < home_up.dv, "O4a  moon surface->low is cheaper than home surface->low");
    check(ast_up.ok && ast_up.dv < home_up.dv, "O4b  asteroid surface->low is cheaper than home surface->low");
    check(ast_up.dv < moon_up.dv, "O4c  the tinier asteroid climbs cheaper than the moon");

    // O5
    const orbital_path_result to_moon = orbital_path(w, p, { t.kepler, K::surface }, { t.selene, K::low_orbit });
    const orbital_path_result to_far  = orbital_path(w, p, { t.kepler, K::surface }, { t.pallas, K::low_orbit });
    const orbital_path_result to_cin  = orbital_path(w, p, { t.kepler, K::surface }, { t.cinder, K::low_orbit });
    print_path("home->moon low  ", to_moon);
    print_path("home->Pallas low", to_far);
    print_path("home->Cinder low", to_cin);
    check(to_moon.ok && to_far.ok && to_moon.dv < to_far.dv, "O5a  home surface->moon low is cheaper than ->far body low (Pallas)");
    check(to_cin.ok && to_moon.dv < to_cin.dv, "O5b  home surface->moon low is cheaper than ->Cinder low");
    check(to_moon.days < to_far.days, "O5c  and quicker in days");
    check(to_far.dv > to_cin.dv, "O5d  the interplanetary leg grows with orbital-radius difference");

    // O6
    const test_world t2 = make_test_world();
    check(same_links(links, orbital_links(w, p)) && same_links(links, orbital_links(t2.w, p)),
          "O6a  links bit-identical across calls and across a rebuilt world");
    check(same_path(to_far, orbital_path(w, p, { t.kepler, K::surface }, { t.pallas, K::low_orbit })) &&
          same_path(to_far, orbital_path(t2.w, p, { t2.kepler, K::surface }, { t2.pallas, K::low_orbit })),
          "O6b  paths bit-identical across calls and across a rebuilt world");
    const orbital_position l4b = orbital_node_position(w, { t.selene, K::l4 });
    check(std::memcmp(&l4b, &l4, sizeof l4) == 0, "O6c  positions bit-identical across calls");

    // O7
    check(orbital_slot_capacity(w, { t.kepler, K::surface }) == 0, "O7a  surface holds 0");
    check(orbital_slot_capacity(w, { t.kepler, K::low_orbit }) == 12, "O7b  home low orbit = 8 + 4x1.0 = 12");
    check(orbital_slot_capacity(w, { t.selene, K::low_orbit }) == 8 && orbital_slot_capacity(w, { t.pallas, K::low_orbit }) == 8,
          "O7c  moon / asteroid low orbit = 8 (4 x mass rounds down to 0)");
    check(orbital_slot_capacity(w, { t.kepler, K::high_orbit }) == 4 && orbital_slot_capacity(w, { t.cinder, K::high_orbit }) == 4,
          "O7d  planet high orbit = 4");
    check(orbital_slot_capacity(w, { t.selene, K::high_orbit }) == 2 && orbital_slot_capacity(w, { t.pallas, K::high_orbit }) == 2,
          "O7e  moon / asteroid high orbit = 2");
    check(orbital_slot_capacity(w, { t.selene, K::l4 }) == 3 && orbital_slot_capacity(w, { t.kepler, K::l5 }) == 3,
          "O7f  L4 / L5 = 3");
    check(orbital_slot_capacity(w, { t.selene, K::l1 }) == 2 && orbital_slot_capacity(w, { t.kepler, K::l2 }) == 2,
          "O7g  L1 / L2 = 2");
    check(orbital_slot_capacity(w, { t.star, K::low_orbit }) == 0, "O7h  the star (no node) holds 0");

    // O8
    check(!orbital_path(w, p, { t.star, K::surface }, { t.kepler, K::surface }).ok, "O8a  a star end fails");
    check(!orbital_path(w, p, { 99999u, K::surface }, { t.kepler, K::surface }).ok, "O8b  a missing body fails");
    const orbital_path_result self = orbital_path(w, p, { t.kepler, K::l4 }, { t.kepler, K::l4 });
    check(self.ok && self.dv == 0.0 && self.days == 0.0 && self.nodes.size() == 1, "O8c  from == to is a zero-cost trip");
}

void run_generated_world()
{
    using K = orbital_node_kind;
    const orbital_graph_params p{};
    world_params wp;
    wp.seed = 20260911u;
    // The FULL generation: a stopped one (stop_after_ancient_era) returns a
    // reduced body set with no moon or asteroid, so it would test nothing.
    const world g  = make_hard_coded_world(wp);
    const world g2 = make_hard_coded_world(wp);

    std::size_t non_star = 0;
    for (const auto& [id, b] : g.bodies) if (b.type != body_type::star) ++non_star;
    const std::vector<orbital_node> nodes = orbital_nodes(g);
    std::printf("      generated: bodies=%zu non-star=%zu nodes=%zu links=%zu\n",
                g.bodies.size(), non_star, nodes.size(), orbital_links(g, p).size());
    check(non_star > 0 && nodes.size() == non_star * orbital_node_kind_count, "G1  generated world: 7 nodes per non-star body");

    bool all_reach = true;
    for (const orbital_node& n : nodes)
        all_reach = all_reach && orbital_path(g, p, { g.home_body, K::surface }, n).ok;
    check(all_reach, "G2  every node is reachable from the home surface");

    check(same_links(orbital_links(g, p), orbital_links(g2, p)), "G3  links bit-identical across two generations of one seed");

    // Small airless bodies climb cheaper than home on the generated world too.
    // NOTE: generation leaves mass_earths at its 1.0 default on every body but
    // the homeworld (hard_coded_world.cpp sets only the homeworld's), so this
    // row holds on the ATMOSPHERE term alone; the printout below shows masses.
    bool small_cheaper = true;
    int  defaulted     = 0;
    const double home_climb = orbital_climb_dv(g, p, g.home_body);
    for (const auto& [id, b] : g.bodies)
    {
        if (b.type == body_type::moon || b.type == body_type::asteroid)
            small_cheaper = small_cheaper && orbital_climb_dv(g, p, id) < home_climb;
        if (b.type != body_type::star && id != g.home_body && b.mass_earths == 1.0f) ++defaulted;
    }
    check(small_cheaper, "G4  generated moons/asteroids climb cheaper than the homeworld");
    if (defaulted > 0)
        std::printf("WARN  %d non-home bodies carry the default mass_earths 1.0 "
                    "(generation does not copy planetology's mass)\n", defaulted);

    for (const orbital_node& n : nodes)
        if (n.kind == K::surface)
        {
            const body_component& b = g.bodies.at(n.body);
            const orbital_path_result r = orbital_path(g, p, { g.home_body, K::surface }, { n.body, K::low_orbit });
            std::printf("      body %u %-10s type=%d mass=%.6f atm=%d climb=%.4f  home->low dv=%.4f days=%.2f\n",
                        static_cast<unsigned>(n.body), b.name.c_str(), static_cast<int>(b.type), b.mass_earths,
                        static_cast<int>(b.atmosphere), orbital_climb_dv(g, p, n.body), r.dv, r.days);
        }
}

} // namespace

int main()
{
    std::printf("== orbital_graph_harness (BL-1580) ==\n");
    run_test_world();
    run_generated_world();
    std::printf("== %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
