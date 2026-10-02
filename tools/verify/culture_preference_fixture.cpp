// ---------------------------------------------------------------------------
// culture_preference_fixture — BL-1107 review fixes. The cultural preference's
// reading of the ground profile (EXPLORATION.md sec A good acquires a cultural
// preference; COLONISATION.md sec The ground profile), asserted on synthetic
// tables small enough that every expected weight is written out by hand, and
// the window amenity reader (TILES.md sec Amenity tiles) on a synthetic grid.
//
//   P0  no cradle held a good -> every cradle sits at the mean -> no lack term
//   P1  a good the culture holds NOW is never preferred, whatever the profile
//   P2  the 1000 cap applies AFTER both terms are summed
//   P3  a culture that is the plurality of no region gets no profile term
//   P4  the profile mean is taken over CRADLE cultures only
//   P5  read_window_amenity: the 120 floor, and a tie goes to the lower class
//
// Pure: no world is generated, so it runs in well under a second and can be
// used to prove each row fails when the line it guards is broken.
//
// Build: node tools/verify/build_harness.js culture_preference_fixture
// Run:   build_gen/verify/culture_preference_fixture.exe
// ---------------------------------------------------------------------------

#include "world/colonisation.hpp"
#include "world/history_sim.hpp"
#include "world/settlement.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace
{

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

/// A coined cradle culture with the given four profile classes, `open` amenity.
culture cradle(int farm, int ore, int energy, int water)
{
    culture c;
    c.profile.farm = farm; c.profile.ore = ore; c.profile.energy = energy; c.profile.water = water;
    c.profile.amenity = static_cast<int8_t>(amenity_class::open);
    c.profile.amenity_share = 0;
    return c;
}

region on(int nation, int plurality, region_class dominant)
{
    region r;
    r.nation = nation;
    r.dominant = dominant;
    r.culture.id[0] = static_cast<int16_t>(plurality);
    if (plurality >= 0) { r.culture.weight_q[0] = 1000; r.culture.other_q = 0; }
    return r;
}

std::vector<polity> living(int n)
{
    std::vector<polity> ps(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) { ps[i].id = i; ps[i].alive = true; ps[i].capital = i; }
    return ps;
}

void sort_contacts(std::vector<contact>& cs)
{
    std::sort(cs.begin(), cs.end(), [](const contact& a, const contact& b) {
        return a.from != b.from ? a.from < b.from : a.to < b.to;
    });
}

/// The weight culture @p c carries on @p g, or -1 for no entry.
int weight(const std::vector<culture_good_preference>& t, int c, region_class g)
{
    for (const culture_good_preference& p : t)
        if (p.culture == c && p.good == g) return p.weight_q;
    return -1;
}

} // namespace

int main()
{
    std::printf("=== culture_preference_fixture (BL-1107) ===\n\n");

    // P0 -- three cradles, none of which held any energy. Each stands on its
    // own subsistence region (dominant none) under its own polity; no contacts,
    // so the exposure term is 0 throughout and only the profile can speak.
    {
        std::vector<culture> cs = { cradle(100, 50, 0, 10), cradle(300, 150, 0, 30), cradle(200, 100, 0, 20) };
        std::vector<region> rs = { on(0, 0, region_class::none), on(1, 1, region_class::none),
                                   on(2, 2, region_class::none) };
        const auto t = derive_culture_preference(rs, {}, living(3), 3, &cs);
        bool any_energy = false;
        for (const auto& p : t) if (p.good == region_class::energy) any_energy = true;
        // Culture 0 is below the mean on farm, ore and water, so it still
        // carries a lack there -- the case is not vacuous.
        check(!any_energy && weight(t, 0, region_class::farm) > 0,
              "P0  cradles that ALL lack energy give no energy preference from the lack term "
              "(no cradle held it, so each sits at the mean)");
    }

    // P1 -- culture 0's cradle held no ore at all (full lack) but culture 0
    // stands today on ore ground; culture 1 is the ore-rich cradle the mean
    // needs. Ore must not be preferred by culture 0.
    {
        std::vector<culture> cs = { cradle(500, 0, 500, 500), cradle(500, 1000, 500, 500) };
        std::vector<region> rs = { on(0, 0, region_class::ore), on(1, 1, region_class::farm) };
        const auto t = derive_culture_preference(rs, {}, living(2), 2, &cs);
        check(weight(t, 0, region_class::ore) == -1,
              "P1  a good the culture holds NOW is never preferred, whatever its cradle lacked");
    }

    // P2 -- culture 0 has met four polities that hold energy (exposure 4 ->
    // 1000 on its own) and its cradle held no energy (lack 250). The sum is
    // 1250; the stored weight must be the cap.
    {
        std::vector<culture> cs = { cradle(500, 500, 0, 500), cradle(500, 500, 1000, 500) };
        std::vector<region> rs = { on(0, 0, region_class::none) };
        for (int i = 1; i <= 4; ++i) rs.push_back(on(i, 1, region_class::energy));
        std::vector<contact> ct;
        for (int i = 1; i <= 4; ++i) { contact c; c.from = 0; c.to = static_cast<uint16_t>(i); ct.push_back(c); }
        sort_contacts(ct);
        const auto t = derive_culture_preference(rs, ct, living(5), 2, &cs);
        check(weight(t, 0, region_class::energy) == 1000,
              "P2  the 1000 cap applies after BOTH terms (exposure 1000 + lack 250 stores 1000)");
    }

    // P3 -- culture 2 carries a full ore lack in its profile but is the
    // plurality of NO region (it holds only a minority share of region 0).
    {
        std::vector<culture> cs = { cradle(500, 1000, 500, 500), cradle(500, 1000, 500, 500),
                                    cradle(500, 0, 500, 500) };
        std::vector<region> rs = { on(0, 0, region_class::none), on(1, 1, region_class::none) };
        rs[0].culture.id[1] = 2; rs[0].culture.weight_q[0] = 700; rs[0].culture.weight_q[1] = 300;
        const auto t = derive_culture_preference(rs, {}, living(2), 3, &cs);
        bool any_c2 = false;
        for (const auto& p : t) if (p.culture == 2) any_c2 = true;
        check(!any_c2, "P3  a culture that is the plurality of no region gets no profile term");
    }

    // P4 -- cradles A (ore 0), B (ore 1000), C (ore 400), plus eight daughters
    // of B carrying B's profile whole. Over the CRADLES the mean is 1400 / 3:
    // C scores 400*500*3/1400 = 428 and lacks (500-428)*250/500 = 36. Over
    // every culture it would be 9400 / 11: C scores 234 and lacks 133.
    {
        std::vector<culture> cs = { cradle(500, 0, 500, 500), cradle(500, 1000, 500, 500),
                                    cradle(500, 400, 500, 500) };
        for (int d = 0; d < 8; ++d)
        {
            culture k = cs[1];
            k.parent = 1; k.coined_from = 1;
            cs.push_back(k);
        }
        std::vector<region> rs = { on(0, 0, region_class::none), on(1, 1, region_class::none),
                                   on(2, 2, region_class::none) };
        const auto t = derive_culture_preference(rs, {}, living(3), static_cast<int>(cs.size()), &cs);
        check(weight(t, 2, region_class::ore) == 36,
              "P4  the profile mean is over the CRADLES only (C's ore lack is 36, not 133)");
    }

    // P5 -- a 7x7 grid of dry soil under grass, read as one radius-3 window
    // centred on (3,3): 49 land cells, none on a shore, so everything is
    // `open` until cover is written in.
    {
        constexpr int gw = 7, gh = 7;
        const auto fresh = [&](std::vector<terrain_substrate>& s, std::vector<terrain_cover>& c,
                               std::vector<terrain_landform>& l) {
            s.assign(gw * gh, terrain_substrate::sedimentary);
            c.assign(gw * gh, terrain_cover::grass);
            l.assign(gw * gh, terrain_landform::plains);
        };
        std::vector<terrain_substrate> s; std::vector<terrain_cover> c; std::vector<terrain_landform> l;

        // 6 forest cells = 122 per mille: at the floor's right side.
        fresh(s, c, l);
        for (int i = 0; i < 6; ++i) c[static_cast<std::size_t>(i)] = terrain_cover::forest;
        const amenity_reading six = read_window_amenity(s, c, l, gw, gh, 3, 3, 3);
        // 5 forest cells = 102 per mille: under the floor.
        fresh(s, c, l);
        for (int i = 0; i < 5; ++i) c[static_cast<std::size_t>(i)] = terrain_cover::forest;
        const amenity_reading five = read_window_amenity(s, c, l, gw, gh, 3, 3, 3);
        check(six.cls == amenity_class::forest && six.share == 122
              && five.cls == amenity_class::open && five.share == 0,
              "P5a the 120 floor: 6/49 forest reads forest (122), 5/49 reads open");

        // 6 forest and 6 marsh-in-valley: a tie, which goes to the LOWER class.
        fresh(s, c, l);
        for (int i = 0; i < 6; ++i) c[static_cast<std::size_t>(i)] = terrain_cover::forest;
        for (int i = 10; i < 16; ++i)
        {
            c[static_cast<std::size_t>(i)] = terrain_cover::marsh;
            l[static_cast<std::size_t>(i)] = terrain_landform::valley;
        }
        const amenity_reading tie = read_window_amenity(s, c, l, gw, gh, 3, 3, 3);
        check(tie.cls == amenity_class::forest,
              "P5b a tie between two amenity classes goes to the lower one (forest over valley marsh)");
    }

    std::printf("\n=== culture_preference_fixture: %d failure(s) ===\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
