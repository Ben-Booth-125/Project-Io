// ---------------------------------------------------------------------------
// deposit_origin — BL-762. Deposits split by ORIGIN.
//
// Ben's generation reorder (point 1) puts metals with the body and the
// biosphere's residue with Life. Part 1 of the item landed the classification —
// `resource_origin_of`, a switch with no default over every resource, guaranteed
// complete by a static_assert. It had ZERO consumers: nothing in the generator
// asked a resource where it came from, so the split existed as a type and not as
// a behaviour.
//
// This harness checks the half that closes that. Pass 6 now dispatches every
// deposit write on the origin table, so the BODY phase's output and the LIFE
// phase's output are separate destinations, and `generation_record` reports what
// each phase placed. The claims below are the ones that were previously only
// assertable in a comment:
//
//   D1  the Body phase places NO biological deposit — not one, on any tile.
//   D2  the Life phase places NO geological deposit — the split cuts both ways.
//   D3  nothing reaches a tile that no phase placed (the halves are a partition,
//       not a filter with a leak).
//   D4  a manufactured good is placed by neither — it has no origin in the
//       ground, and asking for one is a category error rather than a zero.
//
// WHAT THIS DOES NOT CLAIM, and the distinction matters. The life half is still
// drawn from the PRESENT cover — a coal seam appears where the ground is barren
// today, not where a swamp stood in that tile's own past. Deriving it from the
// paleo record is BL-765's act; BL-764 is what makes the past askable. This item
// builds the seam, and the world is byte-identical because it does not yet write
// through it.
//
// Lua-free: it runs the shipped planetology -> continents -> tile pipeline and
// reads the generation record, without a world save or a Lua state.
// ---------------------------------------------------------------------------

#include "world/continents.hpp"
#include "world/planetology.hpp"
#include "world/resource_names.hpp"
#include "world/tile_generation.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr int gw = 180, gh = 84;

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

const char* origin_name(resource_origin o)
{
    switch (o)
    {
        case resource_origin::geological:   return "geological";
        case resource_origin::biological:   return "biological";
        case resource_origin::manufactured: return "manufactured";
        case resource_origin::unset:        break;
    }
    return "UNSET";
}

/// One body generated end to end, with the record kept.
struct run
{
    planetology_state st;
    generation_record rec;
    /// Summed `resource_deposit` over the finished tiles — what actually landed,
    /// after the three post-multiplies the phase totals deliberately precede.
    std::array<double, resource_count> on_tiles{};
};

run generate(uint32_t campaign_seed)
{
    run out;
    const resolved_world rw = resolve_preferences(world_preferences{}, campaign_seed);
    body_inputs home = prototype_body(1);
    home.orbit_au = rw.home_orbit_au;

    const uint32_t body_seed = campaign_seed ^ prototype_body_seed(1);
    out.st = run_planetology(home, rw.params, body_seed);
    const continent_state cs = run_continents(out.st, gw, gh, body_seed ^ 0xC0117E57u);

    world w;
    const entity_id body = w.create_entity();
    const std::vector<entity_id> ids =
        generate_body_tiles(w, body, gw, gh, out.st.profile, campaign_seed ^ 0xE471001u,
                            1.0f, &out.st, &out.rec, &cs.height_bias, &cs.convergent);

    for (entity_id id : ids)
    {
        if (id == null_entity) continue;
        const tile_component& t = w.tiles.at(id);
        for (std::size_t r = 0; r < resource_count; ++r)
            out.on_tiles[r] += static_cast<double>(t.resource_deposit[r]);
    }
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    const int seeds = (argc > 1) ? std::atoi(argv[1]) : 4;

    std::printf("=== deposit_origin (BL-762) — the Body phase places no biological deposit ===\n\n");

    // --- D0: the classification is TOTAL ------------------------------------
    // A runtime mirror of the static_assert in components.hpp. Cheap, and it
    // states in the harness output the guarantee the build already enforces, so
    // a reader of this log does not have to take it on trust.
    bool total = true;
    for (std::size_t i = 0; i < resource_count; ++i)
        if (resource_origin_of(static_cast<resource_type>(i)) == resource_origin::unset) total = false;
    check(total, "D0   every resource carries an origin (the compile-time guarantee, restated)");

    bool d1 = true, d2 = true, d3 = true, d4 = true;
    bool life_placed_something = false, body_placed_something = false;
    resource_type d1_witness = resource_type::count;
    resource_type d2_witness = resource_type::count;
    resource_type d3_witness = resource_type::count;

    for (int s = 0; s < seeds; ++s)
    {
        const uint32_t seed = 0xABCDEF01u ^ (static_cast<uint32_t>(s) * 0x9E3779B9u);
        const run r = generate(seed);

        for (std::size_t i = 0; i < resource_count; ++i)
        {
            const resource_type res = static_cast<resource_type>(i);
            const double body = r.rec.body_phase_placed[i];
            const double life = r.rec.life_phase_placed[i];

            switch (resource_origin_of(static_cast<resource_type>(i)))
            {
                case resource_origin::biological:
                    if (body != 0.0) { d1 = false; d1_witness = res; }
                    if (life > 0.0)  life_placed_something = true;
                    break;
                case resource_origin::geological:
                    if (life != 0.0) { d2 = false; d2_witness = res; }
                    if (body > 0.0)  body_placed_something = true;
                    break;
                case resource_origin::manufactured:
                    if (body != 0.0 || life != 0.0) { d4 = false; }
                    break;
                case resource_origin::unset:
                    break;
            }

            // D3: nothing on a tile that no phase placed. The converse does NOT
            // hold and must not be asserted — planetology's endowment can zero a
            // resource the life phase legitimately placed, which is exactly how
            // "no life, no coal" works.
            if (r.on_tiles[i] > 0.0 && body == 0.0 && life == 0.0)
            {
                d3 = false;
                d3_witness = res;
            }
        }

        if (s == 0)
        {
            std::printf("     seed %08X: life stage %d, peak %d\n",
                        seed, static_cast<int>(r.st.stage), static_cast<int>(r.st.peak));
            std::printf("     %-24s %-13s %14s %14s %14s\n",
                        "resource", "origin", "body placed", "life placed", "on tiles");
            for (std::size_t i = 0; i < resource_count; ++i)
            {
                if (r.rec.body_phase_placed[i] == 0.0 && r.rec.life_phase_placed[i] == 0.0
                    && r.on_tiles[i] == 0.0)
                    continue;
                std::printf("     %-24s %-13s %14.0f %14.0f %14.0f\n",
                            resource_names::name_of(static_cast<resource_type>(i)).c_str(),
                            origin_name(resource_origin_of(static_cast<resource_type>(i))),
                            r.rec.body_phase_placed[i], r.rec.life_phase_placed[i],
                            r.on_tiles[i]);
            }
            std::printf("\n");
        }
    }

    if (!d1) std::printf("     witness: %s reached the body phase\n",
                         resource_names::name_of(d1_witness).c_str());
    if (!d2) std::printf("     witness: %s reached the life phase\n",
                         resource_names::name_of(d2_witness).c_str());
    if (!d3) std::printf("     witness: %s landed on a tile no phase placed\n",
                         resource_names::name_of(d3_witness).c_str());

    check(d1, "D1   the Body phase places NO biological deposit, on any tile of any seed");
    check(d2, "D2   the Life phase places NO geological deposit — the split cuts both ways");
    check(d3, "D3   nothing reaches a tile that neither phase placed");
    check(d4, "D4   a manufactured good is placed by neither phase — it has no ground origin");

    // Both halves must be NON-EMPTY or the two rows above are vacuous: an
    // all-zero record satisfies D1 and D2 perfectly and proves nothing.
    check(body_placed_something && life_placed_something,
          "D5   both phases actually placed something — D1/D2 are not vacuous");

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
